"""PiPico Display Config: programma per configurare il display (come i configuratori delle tastiere).

Le impostazioni si scelgono qui e vengono salvate NEL DISPOSITIVO (flash del Pico): una volta
impostate il display si avvia sempre cosi', anche senza questo programma e senza il PC.

- Anteprima dal vivo di quello che sta mostrando il display (comando SNAP)
- Scelta del tema, della schermata, degli elementi e degli effetti, in tempo reale
- Grafici, allarmi, spegnimento dello schermo, velocita' SPI
- Profili: esporta / importa tutta la configurazione, preset pronti
- Invio delle statistiche del PC (in alternativa a pc_stats.py)

Uso:
    python tools/display_gui.py                  oppure il collegamento creato da tools/install_app.ps1
    python tools/display_gui.py --shots cartella   salva uno screenshot di ogni scheda ed esce (collaudo)

Richiede: pip install psutil pyserial   (tkinter e' incluso in Python per Windows; pillow e' facoltativo)
"""
import datetime
import json
import os
import queue
import subprocess
import sys
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

import psutil
import serial

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from gui_defs import BUILDS, GRAPHS, METRICS, THEME_INFO  # noqa: E402
from pc_stats import Collector, find_port  # noqa: E402
from snap import rgb565_to_ppm  # noqa: E402

APP_NAME = "PiPico Display Config"
SETTINGS = os.path.join(HERE, "display_config.json")  # solo le preferenze di questa finestra
STARTUP_LNK = os.path.join(os.environ.get("APPDATA", ""), "Microsoft", "Windows", "Start Menu", "Programs",
                           "Startup", "PiPico Display Stats.lnk")
MIN_FW = (1, 3, 0)  # versione minima del firmware con tutti i comandi usati qui
NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)
GREEN, ORANGE, RED, GRAY = "#1a8f3a", "#c77700", "#c0392b", "#777777"


def stop_pc_stats():
    """Ferma pc_stats.py se sta girando (tiene occupata la seriale). Restituisce True se l'ha fermato."""
    stopped = False
    for p in psutil.process_iter(["pid", "cmdline"]):
        try:
            if "pc_stats.py" in " ".join(p.info["cmdline"] or []) and p.info["pid"] != os.getpid():
                p.kill()
                stopped = True
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass
    return stopped


def start_pc_stats():
    flags = NO_WINDOW | getattr(subprocess, "DETACHED_PROCESS", 0)
    subprocess.Popen([sys.executable, os.path.join(HERE, "pc_stats.py")], creationflags=flags, close_fds=True)


class App:
    def __init__(self, root, shots_dir=None):
        self.root = root
        self.shots_dir = shots_dir
        self.running = True
        self.ser = None
        self.lock = threading.Lock()
        self.rx = queue.Queue()
        self.want_conn = True

        # stato del dispositivo
        self.connected = False
        self.port_name = ""
        self.theme = None
        self.themes = list(BUILDS)
        self.fw = None
        self.fw_date = ""
        self.fps_info = ""
        self.states = {}          # tema -> {"flags", "screen", "name"} (da DALL)
        self.collecting = False
        self.snap_busy = False
        self.snap_t = 0.0
        self.dirty = False
        self.photo = None
        self.state_seen = False

        self.prefs = self.load_prefs()
        self.stopped_stats = stop_pc_stats()

        # variabili dell'interfaccia
        self.status = tk.StringVar(value="Nessun dispositivo")
        self.saved = tk.StringVar(value="")
        self.fwinfo = tk.StringVar(value="")
        self.theme_var = tk.StringVar()
        self.screen = tk.StringVar()
        self.name = tk.StringVar(value="SAMU")
        self.graph = tk.StringVar(value="cpu")
        self.el_vars = {}
        self.off_var = tk.IntVar(value=300)
        self.al_cpu, self.al_gpu, self.al_temp = tk.IntVar(value=90), tk.IntVar(value=90), tk.IntVar(value=85)
        self.al_beep = tk.BooleanVar(value=True)
        self.spi_var = tk.StringVar(value="40")
        self.send_stats = tk.BooleanVar(value=True)
        self.stats_on = True
        self.preview_on = tk.BooleanVar(value=self.prefs.get("preview", True))
        self.scale = tk.IntVar(value=self.prefs.get("scale", 2))
        self.autostart = tk.BooleanVar(value=os.path.exists(STARTUP_LNK))
        self.raw = tk.StringVar()
        self.fw_env = tk.StringVar(value="all")

        self.build_ui()
        self.rebuild_theme_dependent()
        root.protocol("WM_DELETE_WINDOW", self.close)
        threading.Thread(target=self.conn_loop, daemon=True).start()
        threading.Thread(target=self.reader_loop, daemon=True).start()
        threading.Thread(target=self.stats_loop, daemon=True).start()
        root.after(50, self.poll_rx)
        root.after(500, self.preview_loop)
        if shots_dir:
            root.after(9000, self.take_shots)

    # ================= interfaccia =================
    def build_ui(self):
        root = self.root
        root.title(APP_NAME)
        root.geometry(self.prefs.get("geometry", "1060x700"))
        root.minsize(900, 620)
        icon = os.path.join(HERE, "app.ico")
        if os.path.exists(icon):
            try:
                root.iconbitmap(icon)
            except tk.TclError:
                pass

        top = ttk.Frame(root)
        top.pack(fill="x", padx=8, pady=(8, 0))
        self.dot = tk.Label(top, text="●", fg=GRAY, font=("Segoe UI", 14))
        self.dot.pack(side="left")
        ttk.Label(top, textvariable=self.status, font=("Segoe UI", 10, "bold")).pack(side="left", padx=(2, 12))
        self.saved_lbl = tk.Label(top, textvariable=self.saved, fg=GRAY, font=("Segoe UI", 10, "bold"))
        self.saved_lbl.pack(side="right")
        ttk.Button(top, text="Ricollega", command=self.reconnect).pack(side="right", padx=8)
        tk.Label(root, anchor="w", fg=GRAY, font=("Segoe UI", 9),
                 text="Le impostazioni vengono salvate nel dispositivo: dopo averle scelte puoi chiudere questo programma, "
                      "il display si avvia sempre cosi'.").pack(fill="x", padx=10)

        body = ttk.Frame(root)
        body.pack(fill="both", expand=True, padx=8, pady=8)

        # ---- anteprima ----
        left = ttk.LabelFrame(body, text="Anteprima del display")
        left.pack(side="left", fill="y")
        self.preview_lbl = tk.Label(left, bg="black", width=30, height=15, text="in attesa del dispositivo...", fg="#888")
        self.preview_lbl.pack(padx=6, pady=6)
        pc = ttk.Frame(left)
        pc.pack(fill="x", padx=6)
        ttk.Checkbutton(pc, text="Aggiorna dal vivo", variable=self.preview_on).pack(anchor="w")
        sc = ttk.Frame(pc)
        sc.pack(anchor="w")
        ttk.Label(sc, text="Ingrandimento:").pack(side="left")
        for v in (1, 2):
            ttk.Radiobutton(sc, text=f"{v}x", value=v, variable=self.scale).pack(side="left", padx=2)
        ttk.Button(pc, text="Salva immagine...", command=self.save_snapshot).pack(anchor="w", pady=4)
        tk.Label(left, textvariable=self.fwinfo, fg=GRAY, justify="left", font=("Segoe UI", 8)).pack(anchor="w", padx=8, pady=(0, 6))

        # ---- schede ----
        self.nb = ttk.Notebook(body)
        self.nb.pack(side="left", fill="both", expand=True, padx=(10, 0))
        self.tab_theme = ttk.Frame(self.nb)
        self.tab_el = ttk.Frame(self.nb)
        self.tab_data = ttk.Frame(self.nb)
        self.tab_alarm = ttk.Frame(self.nb)
        self.tab_prof = ttk.Frame(self.nb)
        self.tab_adv = ttk.Frame(self.nb)
        for tab, title in ((self.tab_theme, "Tema"), (self.tab_el, "Elementi"), (self.tab_data, "Dati e grafici"),
                           (self.tab_alarm, "Allarmi e schermo"), (self.tab_prof, "Profili"), (self.tab_adv, "Avanzate")):
            self.nb.add(tab, text=title)
        self.build_theme_tab()
        self.build_data_tab()
        self.build_alarm_tab()
        self.build_profiles_tab()
        self.build_advanced_tab()

    def build_theme_tab(self):
        f = self.tab_theme
        self.theme_box = ttk.LabelFrame(f, text="Tema (cambia subito sul display)")
        self.theme_box.pack(fill="x", padx=8, pady=8)
        self.screen_box = ttk.LabelFrame(f, text="Schermata")
        self.screen_box.pack(fill="x", padx=8, pady=4)
        nf = ttk.LabelFrame(f, text="Nome")
        nf.pack(fill="x", padx=8, pady=4)
        r = ttk.Frame(nf)
        r.pack(fill="x", padx=6, pady=6)
        self.name_label = ttk.Label(r, text="Nome:")
        self.name_label.pack(side="left")
        e = ttk.Entry(r, textvariable=self.name, width=18)
        e.pack(side="left", padx=6)
        e.bind("<Return>", lambda _: self.send_name())
        ttk.Button(r, text="Imposta", command=self.send_name).pack(side="left")
        b = ttk.Frame(f)
        b.pack(fill="x", padx=8, pady=8)
        ttk.Button(b, text="Rivedi il boot", command=lambda: self.send("M,boot")).pack(side="left")
        ttk.Button(b, text="Pulisci terminale", command=lambda: self.send("clear")).pack(side="left", padx=6)

    def build_data_tab(self):
        f = self.tab_data
        sb = ttk.LabelFrame(f, text="Statistiche del PC")
        sb.pack(fill="x", padx=8, pady=8)
        ttk.Checkbutton(sb, text="Invia le statistiche al display mentre questo programma e' aperto", variable=self.send_stats,
                        command=lambda: setattr(self, "stats_on", self.send_stats.get())).pack(anchor="w", padx=6, pady=(6, 0))
        self.stats_txt = tk.StringVar(value="-")
        tk.Label(sb, textvariable=self.stats_txt, justify="left", font=("Consolas", 9), anchor="w").pack(fill="x", padx=6, pady=4)
        ttk.Checkbutton(sb, text="Avvia l'invio delle statistiche con Windows (anche con questo programma chiuso)",
                        variable=self.autostart, command=self.toggle_autostart).pack(anchor="w", padx=6, pady=(0, 6))
        gb = ttk.LabelFrame(f, text="Grafico")
        gb.pack(fill="x", padx=8, pady=4)
        r = ttk.Frame(gb)
        r.pack(fill="x", padx=6, pady=6)
        ttk.Label(r, text="Statistica:").pack(side="left")
        self.graph_combo = ttk.Combobox(r, values=[label for _, label in GRAPHS], width=34, state="readonly")
        self.graph_combo.pack(side="left", padx=6)
        self.graph_combo.current(1)
        self.graph_combo.bind("<<ComboboxSelected>>", lambda _: self.pick_graph(GRAPHS[self.graph_combo.current()][0]))
        ttk.Label(gb, text="Scegliendola il display passa alla schermata Grafico.").pack(anchor="w", padx=6, pady=(0, 6))

    def build_alarm_tab(self):
        f = self.tab_alarm
        ab = ttk.LabelFrame(f, text="Allarme (0 = soglia disattivata)")
        ab.pack(fill="x", padx=8, pady=8)
        r = ttk.Frame(ab)
        r.pack(fill="x", padx=6, pady=6)
        for label, var, hi in (("CPU %", self.al_cpu, 100), ("GPU %", self.al_gpu, 100), ("Temp. GPU C", self.al_temp, 150)):
            ttk.Label(r, text=label).pack(side="left", padx=(0, 2))
            ttk.Spinbox(r, from_=0, to=hi, textvariable=var, width=5).pack(side="left", padx=(0, 10))
        r2 = ttk.Frame(ab)
        r2.pack(fill="x", padx=6, pady=(0, 6))
        ttk.Checkbutton(r2, text="Cicalino (GP13)", variable=self.al_beep).pack(side="left")
        ttk.Button(r2, text="Imposta", command=self.send_alarm).pack(side="left", padx=10)
        ttk.Button(r2, text="Prova l'allarme (2,5 s)", command=self.test_alarm).pack(side="left")
        sb = ttk.LabelFrame(f, text="Schermo")
        sb.pack(fill="x", padx=8, pady=4)
        r3 = ttk.Frame(sb)
        r3.pack(fill="x", padx=6, pady=6)
        ttk.Label(r3, text="Spegni lo schermo dopo (secondi, 0 = mai):").pack(side="left")
        ttk.Spinbox(r3, from_=0, to=3600, increment=30, textvariable=self.off_var, width=6).pack(side="left", padx=6)
        ttk.Button(r3, text="Imposta", command=self.send_off).pack(side="left")
        ttk.Label(sb, text="Lo schermo si spegne quando non arriva nulla (per esempio con il PC spento) e riparte al primo dato.").pack(
            anchor="w", padx=6, pady=(0, 6))

    def build_profiles_tab(self):
        f = self.tab_prof
        pb = ttk.LabelFrame(f, text="Profilo = tutta la configurazione (tutti i temi, allarmi, schermo)")
        pb.pack(fill="x", padx=8, pady=8)
        r = ttk.Frame(pb)
        r.pack(fill="x", padx=6, pady=6)
        ttk.Button(r, text="Esporta profilo...", command=self.export_profile).pack(side="left")
        ttk.Button(r, text="Importa e applica...", command=self.import_profile).pack(side="left", padx=8)
        prb = ttk.LabelFrame(f, text="Preset per il tema attivo (modificano gli elementi visibili)")
        prb.pack(fill="x", padx=8, pady=4)
        r = ttk.Frame(prb)
        r.pack(fill="x", padx=6, pady=6)
        for text, mode in (("Tutto acceso", "all"), ("Senza effetti CRT", "noeffects"), ("Solo statistiche", "stats"), ("Tutto spento", "none")):
            ttk.Button(r, text=text, command=lambda m=mode: self.preset(m)).pack(side="left", padx=(0, 6))
        nb = ttk.LabelFrame(f, text="Note (per te o per Claude)")
        nb.pack(fill="both", expand=True, padx=8, pady=4)
        self.notes = tk.Text(nb, height=6, wrap="word")
        self.notes.pack(fill="both", expand=True, padx=6, pady=6)
        self.notes.insert("1.0", self.prefs.get("notes", ""))
        ttk.Button(f, text="Copia il riepilogo negli appunti", command=self.copy_summary).pack(anchor="w", padx=8, pady=(0, 8))

    def build_advanced_tab(self):
        f = self.tab_adv
        ib = ttk.LabelFrame(f, text="Dispositivo")
        ib.pack(fill="x", padx=8, pady=8)
        self.advinfo = tk.StringVar(value="-")
        tk.Label(ib, textvariable=self.advinfo, justify="left", anchor="w", font=("Consolas", 9)).pack(fill="x", padx=6, pady=6)
        r = ttk.Frame(ib)
        r.pack(fill="x", padx=6, pady=(0, 6))
        ttk.Label(r, text="Velocita' SPI (MHz):").pack(side="left")
        ttk.Combobox(r, textvariable=self.spi_var, values=("20", "25", "33", "40", "50", "62"), width=5, state="readonly").pack(side="left", padx=6)
        ttk.Button(r, text="Imposta", command=self.send_spi).pack(side="left")
        ttk.Button(r, text="Misura i fotogrammi/s", command=lambda: self.send("FPS")).pack(side="left", padx=10)
        ttk.Label(ib, text="40 e' il valore sicuro (33 MHz reali). 50 e' piu' fluido ma richiede fili corti: se il display mostra pixel sporchi, torna a 40.").pack(
            anchor="w", padx=6, pady=(0, 6))
        fb = ttk.LabelFrame(f, text="Firmware")
        fb.pack(fill="x", padx=8, pady=4)
        r = ttk.Frame(fb)
        r.pack(fill="x", padx=6, pady=6)
        ttk.Label(r, text="Ambiente:").pack(side="left")
        ttk.Combobox(r, textvariable=self.fw_env, values=["all"] + list(BUILDS), width=10, state="readonly").pack(side="left", padx=6)
        ttk.Button(r, text="Compila e carica sul Pico", command=self.update_firmware).pack(side="left")
        ttk.Button(r, text="Valori di fabbrica...", command=self.factory_reset).pack(side="right")
        cb = ttk.LabelFrame(f, text="Console")
        cb.pack(fill="both", expand=True, padx=8, pady=4)
        self.log = tk.Text(cb, height=10, state="disabled", font=("Consolas", 9))
        self.log.pack(fill="both", expand=True, padx=6, pady=(6, 0))
        r = ttk.Frame(cb)
        r.pack(fill="x", padx=6, pady=6)
        e = ttk.Entry(r, textvariable=self.raw)
        e.pack(side="left", fill="x", expand=True)
        e.bind("<Return>", lambda _: self.send_raw())
        ttk.Button(r, text="Invia", command=self.send_raw).pack(side="left", padx=6)

    def rebuild_theme_dependent(self):
        """Riempie le parti che dipendono dal tema attivo: elenco temi, schermate, elementi."""
        for box in (self.theme_box, self.screen_box):
            for w in box.winfo_children():
                w.destroy()
        for tid in self.themes:
            title, desc = THEME_INFO.get(tid, (tid, ""))
            row = ttk.Frame(self.theme_box)
            row.pack(fill="x", padx=6, pady=2)
            ttk.Radiobutton(row, text=title, value=tid, variable=self.theme_var, command=lambda t=tid: self.request_theme(t)).pack(anchor="w")
            tk.Label(row, text=desc, fg=GRAY, anchor="w", justify="left", wraplength=470, font=("Segoe UI", 8)).pack(anchor="w", padx=22)
        b = BUILDS.get(self.theme)
        self.name_label.configure(text=b["name_label"] if b else "Nome:")
        if b:
            for key, label in b["screens"]:
                ttk.Radiobutton(self.screen_box, text=label, value=key, variable=self.screen, command=self.send_screen).pack(anchor="w", padx=6, pady=1)
        # elementi
        for w in self.tab_el.winfo_children():
            w.destroy()
        if not b:
            ttk.Label(self.tab_el, text="Collega il dispositivo per vedere gli elementi del tema.").pack(padx=10, pady=10)
            return
        old = {k: v.get() for k, v in self.el_vars.items()}
        self.el_vars = {k: tk.BooleanVar(value=old.get(k, True)) for _, k, _, _ in b["elements"]}
        groups = {}
        cols = ttk.Frame(self.tab_el)
        cols.pack(fill="both", expand=True, padx=6, pady=6)
        colframes = [ttk.Frame(cols), ttk.Frame(cols)]
        colframes[0].pack(side="left", fill="both", expand=True, anchor="n")
        colframes[1].pack(side="left", fill="both", expand=True, anchor="n")
        for bit, key, label, group in b["elements"]:
            if group not in groups:
                groups[group] = ttk.LabelFrame(colframes[len(groups) % 2], text=group)
                groups[group].pack(fill="x", padx=4, pady=4)
            ttk.Checkbutton(groups[group], text=label, variable=self.el_vars[key], command=self.send_flags).pack(anchor="w", padx=4, pady=1)

    # ================= salvataggio / stato =================
    def mark_dirty(self):
        self.dirty = True
        self.saved.set("Salvataggio nel dispositivo...")
        self.saved_lbl.configure(fg=ORANGE)

    def mark_saved(self):
        if self.dirty or not self.saved.get():
            self.dirty = False
            self.saved.set("✓ Salvato nel dispositivo")
            self.saved_lbl.configure(fg=GREEN)

    def set_status(self, text, color):
        self.status.set(text)
        self.dot.configure(fg=color)

    # ================= seriale =================
    def conn_loop(self):
        last = None
        while self.running:
            if self.want_conn and self.ser is None:
                port = find_port()
                if port:
                    try:
                        s = serial.Serial(port, 115200, timeout=0.2, write_timeout=1)
                        with self.lock:
                            self.ser = s
                        self.rx.put(("connected", port))
                        last = None
                    except serial.SerialException as e:
                        msg = f"Impossibile aprire {port}: {e}. Un altro programma usa la porta (pc_stats.py, Serial Monitor)?"
                        if msg != last:
                            self.rx.put(("conn_error", msg))
                            last = msg
                elif last != "none":
                    self.rx.put(("nodevice", ""))
                    last = "none"
            time.sleep(1.5)

    def drop(self):
        with self.lock:
            s, self.ser = self.ser, None
        if s:
            try:
                s.close()
            except Exception:
                pass
        self.rx.put(("disconnected", ""))

    def reconnect(self):
        self.want_conn = True
        self.drop()

    def send(self, line, cfg=False):
        with self.lock:
            s = self.ser
        if not s:
            return False
        try:
            s.write((line + "\n").encode())
        except (serial.SerialException, OSError):
            self.drop()
            return False
        if cfg:
            self.root.after(0, self.mark_dirty)
        return True

    def read_exact(self, s, n, timeout):
        end = time.time() + timeout
        buf = bytearray()
        while len(buf) < n and time.time() < end:
            buf += s.read(n - len(buf))
        return bytes(buf)

    def reader_loop(self):
        while self.running:
            with self.lock:
                s = self.ser
            if not s:
                time.sleep(0.2)
                continue
            try:
                raw = s.readline()
                if not raw:
                    continue
                text = raw.decode(errors="replace").strip()
                if text.startswith("SNAP,"):
                    _, w, h, bpp = text.split(",")
                    n = int(w) * int(h) * int(bpp)
                    data = self.read_exact(s, n, 3.0)
                    if len(data) == n:
                        self.rx.put(("snap", (int(w), int(h), data)))
                    else:
                        self.rx.put(("snap_fail", ""))
                elif text:
                    self.rx.put(("line", text))
            except (serial.SerialException, OSError, TypeError, ValueError):
                self.drop()
                time.sleep(0.3)

    # ================= messaggi dal Pico =================
    def poll_rx(self):
        try:
            for _ in range(60):
                kind, val = self.rx.get_nowait()
                self.handle(kind, val)
        except queue.Empty:
            pass
        if self.running:
            self.root.after(40, self.poll_rx)

    def handle(self, kind, val):
        if kind == "connected":
            self.connected = True
            self.port_name = val
            self.state_seen = False
            self.set_status(f"Collegato a {val}", GREEN)
            self.logmsg(f"Collegato a {val}")
            for c in ("?", "V", "L", "D", "FPS"):
                self.send(c)
            self.root.after(4000, self.check_state_seen)
        elif kind == "disconnected":
            self.connected = False
            self.snap_busy = False
            self.set_status("Dispositivo scollegato: ricollegalo, mi ricollego da solo", RED)
            self.saved.set("")
        elif kind == "nodevice":
            self.set_status("Nessun dispositivo: collega il Pico", GRAY)
        elif kind == "conn_error":
            self.set_status("Porta occupata", RED)
            self.logmsg(val)
        elif kind == "snap":
            self.show_snapshot(*val)
        elif kind == "snap_fail":
            self.snap_busy = False
        elif kind == "line":
            self.handle_line(val)

    def handle_line(self, msg):
        if not msg.startswith("STATE,"):
            self.logmsg("< " + msg)
        if msg.startswith("ID,"):
            self.set_theme(msg[3:].strip())
            self.send("D")
        elif msg.startswith("VER,"):
            p = msg.split(",", 2)
            self.fw, self.fw_date = p[1], (p[2] if len(p) > 2 else "")
            self.update_info()
            try:
                if tuple(int(x) for x in self.fw.split(".")) < MIN_FW:
                    messagebox.showwarning(APP_NAME, f"Il firmware sul Pico e' la versione {self.fw}: serve almeno {'.'.join(map(str, MIN_FW))}.\n"
                                           "Aggiornalo dalla scheda Avanzate.")
            except ValueError:
                pass
        elif msg.startswith("LIST,"):
            self.themes = [t for t in msg.split(",")[1:] if t in BUILDS] or list(BUILDS)
            self.rebuild_theme_dependent()
        elif msg.startswith("STATE,"):
            self.apply_state(msg)
        elif msg.startswith("SAVED"):
            self.mark_saved()
        elif msg.startswith("FPS,"):
            self.fps_info = msg
            self.update_info()
        elif msg.startswith("ERR"):
            self.saved.set(msg)
            self.saved_lbl.configure(fg=RED)

    def check_state_seen(self):
        if self.connected and not self.state_seen:
            self.logmsg("Il Pico non ha risposto a D: firmware vecchio. Aggiornalo dalla scheda Avanzate.")
            self.set_status(f"Collegato a {self.port_name} (firmware vecchio)", ORANGE)

    def set_theme(self, tid):
        if tid not in BUILDS:
            return
        changed = tid != self.theme
        self.theme = tid
        self.theme_var.set(tid)
        if changed:
            self.rebuild_theme_dependent()
        self.update_info()

    def apply_state(self, msg):
        """STATE,tema,elementi,schermata,spegnimento,metrica,auto,cpu,gpu,temp,cicalino,nome"""
        p = msg.split(",", 11)
        if len(p) < 12:
            return
        _, tid, flags, screen, off, gm, ga, ac, ag, at, ab, name = p
        if tid not in BUILDS:
            return
        flags, scr = int(flags), int(screen)
        self.states[tid] = {"flags": flags, "screen": scr, "name": name}
        if self.collecting:
            return
        self.off_var.set(int(off))
        metric = METRICS[min(int(gm), len(METRICS) - 1)]
        self.graph.set("auto" if int(ga) else metric)
        keys = [k for k, _ in GRAPHS]
        self.graph_combo.current(keys.index(self.graph.get()))
        self.al_cpu.set(int(ac))
        self.al_gpu.set(int(ag))
        self.al_temp.set(int(at))
        self.al_beep.set(bool(int(ab)))
        if tid == self.theme or self.theme is None:
            self.set_theme(tid)
            b = BUILDS[tid]
            for bit, key, _, _ in b["elements"]:
                self.el_vars[key].set(bool((flags >> bit) & 1) if flags else True)
            screens = [k for k, _ in b["screens"]]
            if scr < len(screens):
                self.screen.set(screens[scr])
            if name:
                self.name.set(name)
        self.state_seen = True
        if not self.dirty:
            self.saved.set("✓ Salvato nel dispositivo")
            self.saved_lbl.configure(fg=GREEN)

    def update_info(self):
        t = THEME_INFO.get(self.theme, (self.theme or "-", ""))[0]
        self.fwinfo.set(f"Tema: {t}\nFirmware: {self.fw or '-'}")
        self.advinfo.set(f"Porta: {self.port_name or '-'}\nFirmware: {self.fw or '-'}  ({self.fw_date})\n"
                         f"Tema attivo: {self.theme or '-'}   Temi nel firmware: {', '.join(self.themes)}\n{self.fps_info}")
        if "spi=" in self.fps_info:
            try:
                self.spi_var.set(self.fps_info.split("spi=")[1].split(",")[0])
            except IndexError:
                pass

    def logmsg(self, msg):
        self.log.configure(state="normal")
        self.log.insert("end", datetime.datetime.now().strftime("%H:%M:%S ") + msg + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    # ================= comandi =================
    def mask(self):
        b = BUILDS[self.theme]
        return sum(1 << bit for bit, key, _, _ in b["elements"] if self.el_vars[key].get())

    def send_flags(self):
        if self.theme:
            self.send(f"C,{self.mask()}", cfg=True)

    def send_screen(self):
        self.send(f"M,{self.screen.get()}", cfg=True)

    def send_name(self):
        self.send(f"N,{self.name.get().strip()[:16]}", cfg=True)

    def request_theme(self, tid):
        if not self.send(f"T,{tid}", cfg=True):
            self.theme_var.set(self.theme or "")

    def pick_graph(self, key):
        self.graph.set(key)
        self.send(f"G,{key}", cfg=True)
        if self.theme:
            screens = [k for k, _ in BUILDS[self.theme]["screens"]]
            if "graph" in screens:
                self.screen.set("graph")
                self.send("M,graph", cfg=True)

    def send_alarm(self):
        try:
            self.send(f"A,{int(self.al_cpu.get())},{int(self.al_gpu.get())},{int(self.al_temp.get())},{1 if self.al_beep.get() else 0}", cfg=True)
        except tk.TclError:
            pass

    def test_alarm(self):
        self.send(f"A,1,1,1,{1 if self.al_beep.get() else 0}")
        self.root.after(2500, self.send_alarm)  # ripristina le soglie prima del salvataggio nella flash (3 s)

    def send_off(self):
        try:
            self.send(f"B,{int(self.off_var.get())}", cfg=True)
        except tk.TclError:
            pass

    def send_spi(self):
        self.send(f"SPI,{self.spi_var.get()}", cfg=True)
        self.root.after(1500, lambda: self.send("FPS"))

    def send_raw(self):
        if self.raw.get().strip():
            self.send(self.raw.get().strip())
            self.logmsg("> " + self.raw.get().strip())
            self.raw.set("")

    def preset(self, mode):
        if not self.theme:
            return
        b = BUILDS[self.theme]
        for bit, key, label, group in b["elements"]:
            if mode == "all":
                v = True
            elif mode == "none":
                v = False
            elif mode == "noeffects":
                v = not group.startswith("Effetti")
            else:  # solo statistiche
                v = group == "Statistiche" or key in ("header", "title")
            self.el_vars[key].set(v)
        self.send_flags()

    def toggle_autostart(self):
        script = os.path.join(HERE, "autostart.ps1")
        args = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script]
        if not self.autostart.get():
            args.append("-Remove")
        try:
            subprocess.run(args, capture_output=True, timeout=30, creationflags=NO_WINDOW)
        except Exception as e:
            messagebox.showerror(APP_NAME, str(e))
        self.autostart.set(os.path.exists(STARTUP_LNK))

    # ================= anteprima =================
    def preview_loop(self):
        if self.running:
            if self.preview_on.get() and self.connected and not self.snap_busy:
                self.snap_busy = True
                self.snap_t = time.time()
                if not self.send("SNAP"):
                    self.snap_busy = False
            elif self.snap_busy and time.time() - self.snap_t > 4:
                self.snap_busy = False
            self.root.after(350, self.preview_loop)

    def show_snapshot(self, w, h, data):
        self.snap_busy = False
        self.last_snap = (w, h, data)
        try:
            photo = tk.PhotoImage(data=rgb565_to_ppm(data, w, h))
            if self.scale.get() > 1:
                photo = photo.zoom(self.scale.get())
            self.photo = photo
            self.preview_lbl.configure(image=photo, width=photo.width(), height=photo.height(), text="")
        except tk.TclError as e:
            self.logmsg(f"Anteprima non disegnabile: {e}")

    def save_snapshot(self):
        snap = getattr(self, "last_snap", None)
        if not snap:
            messagebox.showinfo(APP_NAME, "Nessuna anteprima disponibile.")
            return
        w, h, data = snap
        path = filedialog.asksaveasfilename(defaultextension=".png", filetypes=[("PNG", "*.png"), ("PPM", "*.ppm")],
                                            initialfile="display.png")
        if not path:
            return
        try:
            from PIL import Image
            from snap import rgb565_to_rgb
            Image.frombytes("RGB", (w, h), rgb565_to_rgb(data, w, h)).resize((w * 2, h * 2), Image.NEAREST).save(path)
        except ImportError:
            with open(os.path.splitext(path)[0] + ".ppm", "wb") as f:
                f.write(rgb565_to_ppm(data, w, h))

    # ================= statistiche =================
    def stats_loop(self):
        col = Collector()
        while self.running:
            time.sleep(1)
            lines = col.lines()
            self.root.after(0, lambda s=col.summary: self.stats_txt.set(s))
            if self.stats_on and self.connected:
                for line in lines:
                    self.send(line)

    # ================= profili =================
    def export_profile(self):
        if not self.connected:
            messagebox.showinfo(APP_NAME, "Collega il dispositivo.")
            return
        self.collecting = True
        self.states = {}
        self.send("DALL")
        self.root.after(1500, self._finish_export)

    def _finish_export(self):
        self.collecting = False
        prof = self.current_profile()
        path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("Profilo PiPico", "*.json")], initialfile="profilo.json")
        if path:
            with open(path, "w", encoding="utf-8") as f:
                json.dump(prof, f, indent=2, ensure_ascii=False)
            self.logmsg(f"Profilo salvato in {path}")
        self.send("D")

    def current_profile(self):
        return {
            "format": "pipico-profile", "version": 1, "theme": self.theme,
            "off": int(self.off_var.get()),
            "alarm": {"cpu": int(self.al_cpu.get()), "gpu": int(self.al_gpu.get()), "temp": int(self.al_temp.get()), "beep": bool(self.al_beep.get())},
            "graph": self.graph.get(),
            "themes": self.states,
            "notes": self.notes.get("1.0", "end").strip(),
        }

    def import_profile(self):
        if not self.connected:
            messagebox.showinfo(APP_NAME, "Collega il dispositivo.")
            return
        path = filedialog.askopenfilename(filetypes=[("Profilo PiPico", "*.json")])
        if not path:
            return
        try:
            with open(path, encoding="utf-8") as f:
                p = json.load(f)
            assert p.get("format") == "pipico-profile"
        except Exception:
            messagebox.showerror(APP_NAME, "Il file non e' un profilo PiPico valido.")
            return
        for tid, st in p.get("themes", {}).items():
            if tid in self.themes:
                self.send(f"W,{tid},{st['flags']},{st['screen']},{st.get('name', '')}", cfg=True)
        a = p.get("alarm", {})
        self.send(f"A,{a.get('cpu', 90)},{a.get('gpu', 90)},{a.get('temp', 85)},{1 if a.get('beep', True) else 0}", cfg=True)
        self.send(f"B,{p.get('off', 300)}", cfg=True)
        g = p.get("graph", "cpu")
        if g in [k for k, _ in GRAPHS]:
            self.send(f"G,{g}", cfg=True)
        if p.get("theme") in self.themes and p["theme"] != self.theme:
            self.send(f"T,{p['theme']}", cfg=True)
        self.send("D")
        self.logmsg(f"Profilo applicato: {path}")

    def summary(self):
        if not self.theme:
            return "Nessun dispositivo collegato."
        b = BUILDS[self.theme]
        keep = [label for _, key, label, _ in b["elements"] if self.el_vars[key].get()]
        drop = [label for _, key, label, _ in b["elements"] if not self.el_vars[key].get()]
        lines = [f"Configurazione display - tema {self.theme} ({THEME_INFO.get(self.theme, ('',))[0]})",
                 f"Schermata: {dict(b['screens']).get(self.screen.get(), self.screen.get())}",
                 f"{b['name_label']} {self.name.get()}",
                 f"Grafico: {dict(GRAPHS).get(self.graph.get(), self.graph.get())}",
                 f"Allarme: CPU {self.al_cpu.get()}%, GPU {self.al_gpu.get()}%, temperatura {self.al_temp.get()} C, cicalino {'si' if self.al_beep.get() else 'no'}",
                 f"Spegnimento schermo: {self.off_var.get()} s",
                 f"Maschera: C,{self.mask()}",
                 "TENERE: " + (", ".join(keep) or "-"),
                 "TOGLIERE: " + (", ".join(drop) or "-")]
        n = self.notes.get("1.0", "end").strip()
        if n:
            lines += ["Note:", n]
        return "\n".join(lines)

    def copy_summary(self):
        self.root.clipboard_clear()
        self.root.clipboard_append(self.summary())
        self.logmsg("Riepilogo copiato negli appunti.")

    # ================= avanzate =================
    def factory_reset(self):
        if messagebox.askyesno(APP_NAME, "Cancellare tutte le impostazioni salvate nel dispositivo e riportarlo ai valori di fabbrica?\n"
                                         "Il Pico si riavvia."):
            self.send("K,reset")

    def update_firmware(self):
        pio = os.path.expanduser(r"~\.platformio\penv\Scripts\pio.exe")
        if not os.path.exists(pio):
            pio = "pio"
        if not messagebox.askyesno(APP_NAME, "Compilare e caricare il firmware sul Pico?\n"
                                             "Questa finestra si chiude durante il caricamento e si riapre da sola."):
            return
        env = self.fw_env.get()
        self.logmsg(f"Avvio: pio run -e {env} -t upload (puo' richiedere un minuto)")

        def run():
            try:
                p = subprocess.Popen([pio, "run", "-e", env, "-t", "upload"], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     text=True, creationflags=NO_WINDOW)
                for line in p.stdout:
                    self.root.after(0, lambda l=line.rstrip(): self.logmsg(l))
            except Exception as e:
                self.root.after(0, lambda: self.logmsg(f"Errore: {e}"))

        threading.Thread(target=run, daemon=True).start()

    # ================= preferenze / chiusura / collaudo =================
    def load_prefs(self):
        try:
            with open(SETTINGS, encoding="utf-8") as f:
                p = json.load(f)
            return p if isinstance(p, dict) else {}
        except Exception:
            return {}

    def save_prefs(self):
        p = {"geometry": self.root.geometry(), "preview": self.preview_on.get(), "scale": self.scale.get(),
             "notes": self.notes.get("1.0", "end").strip()}
        try:
            with open(SETTINGS, "w", encoding="utf-8") as f:
                json.dump(p, f, indent=2, ensure_ascii=False)
        except OSError:
            pass

    def close(self):
        self.running = False
        self.save_prefs()
        self.drop()
        if self.stopped_stats:
            start_pc_stats()  # ripristina lo script che avevo fermato
        self.root.destroy()

    def take_shots(self):
        """Collaudo: salva uno screenshot di ogni scheda e chiude."""
        from PIL import ImageGrab
        os.makedirs(self.shots_dir, exist_ok=True)
        for i, (tab, name) in enumerate(((self.tab_theme, "tema"), (self.tab_el, "elementi"), (self.tab_data, "dati"),
                                         (self.tab_alarm, "allarmi"), (self.tab_prof, "profili"), (self.tab_adv, "avanzate"))):
            self.nb.select(tab)
            self.root.update()
            time.sleep(1.0)
            self.root.update()
            x, y, w, h = self.root.winfo_rootx(), self.root.winfo_rooty(), self.root.winfo_width(), self.root.winfo_height()
            ImageGrab.grab(bbox=(x, y, x + w, y + h)).save(os.path.join(self.shots_dir, f"gui_{i}_{name}.png"))
        self.close()


def main():
    shots = None
    if "--shots" in sys.argv:
        shots = sys.argv[sys.argv.index("--shots") + 1]
    root = tk.Tk()
    App(root, shots)
    root.mainloop()


if __name__ == "__main__":
    main()
