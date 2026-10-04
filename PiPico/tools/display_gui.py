"""GUI di debug per il display (temi fallout, skynet, cyberpunk e alien).

Permette di accendere/spegnere ogni elemento ed effetto del display in tempo reale,
scegliere la schermata, inviare le statistiche del PC e salvare/copiare la configurazione
scelta (pulsante "Copia per Claude"). Quando si collega chiede al Pico quale build sta
girando (comando "?") e mostra le caselle di quella build.

Uso:
    python tools/display_gui.py

Il Pico salva nella flash tema, elementi, schermata, nome e grafico: quando la GUI si collega li
legge (comando D) e imposta le caselle di conseguenza. Il menu Build cambia tema sul Pico (T,<tema>).
Mentre e' aperta ferma pc_stats.py (la porta seriale la puo' avere un solo programma) e lo
rilancia quando viene chiusa.

Richiede: pip install psutil pyserial  (tkinter e' incluso in Python per Windows)
Sostituisce pc_stats.py mentre e' aperta: la porta seriale puo' averla un solo programma.
"""
import json
import os
import queue
import subprocess
import sys
import threading
import time
import tkinter as tk
from tkinter import ttk

import psutil
import serial

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pc_stats import Collector, find_port  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
CONFIG_JSON = os.path.join(HERE, "display_config.json")
CONFIG_TXT = os.path.join(HERE, "display_config.txt")

# Per ogni build: elementi (bit, chiave, etichetta, gruppo) - i bit corrispondono a E_* nel
# main.cpp della build - e schermate (comando M,<chiave>)
BUILDS = {
    "fallout": {
        "title": "RobCo Industries (fallout)",
        "name_label": "Nome amministratore:",
        "screens": [
            ("auto", "Automatica (monitor se arrivano dati, altrimenti hacking)"),
            ("home", "Monitor di sistema"),
            ("hack", "Minigioco hacking password"),
            ("term", "Terminale seriale"),
            ("graph", "Grafico statistiche"),
        ],
        "elements": [
            (16, "logo",      "Logo RobCo Industries nel boot",                  "Contenuti"),
            (0,  "header",    "Intestazione RobCo",                              "Contenuti"),
            (1,  "title",     "Titolo sezione e linea ====",                     "Contenuti"),
            (7,  "menu",      "Menu con voce evidenziata",                       "Contenuti"),
            (8,  "log",       "Righe di LOG / stato",                            "Contenuti"),
            (9,  "cursor",    "Prompt con cursore lampeggiante",                 "Contenuti"),
            (2,  "cpu",       "Riga CPU",                                        "Statistiche"),
            (3,  "ram",       "Riga RAM",                                        "Statistiche"),
            (4,  "dsk",       "Riga disco",                                      "Statistiche"),
            (5,  "gpu",       "Riga GPU",                                        "Statistiche"),
            (6,  "info",      "Info: temp GPU, rete, uptime (offline: User Log)", "Statistiche"),
            (10, "bigstats",  "Statistiche a caratteri grandi (no = barre ASCII)", "Statistiche"),
            (11, "scanlines", "Scanline (righe scure alternate)",                "Effetti CRT"),
            (12, "scanbar",   "Barra di scansione che scende",                   "Effetti CRT"),
            (13, "glow",      "Bagliore centrale / bordi scuri",                 "Effetti CRT"),
            (14, "flicker",   "Sfarfallio del fosforo",                          "Effetti CRT"),
            (15, "typeon",    "Scrittura progressiva del testo",                 "Effetti CRT"),
        ],
    },
    "skynet": {
        "title": "Skynet / T-800 (skynet)",
        "name_label": "Nome bersaglio:",
        "screens": [
            ("hud", "HUD (visione del T-800)"),
            ("term", "Terminale seriale"),
            ("graph", "Grafico statistiche"),
        ],
        "elements": [
            (0,  "header",    "Intestazione CYBERDYNE / MODEL 101",              "Contenuti"),
            (1,  "datacol",   "Colonna sinistra (codice 6502 / statistiche)",    "Contenuti"),
            (7,  "reticle",   "Mirino",                                          "Contenuti"),
            (9,  "analysis",  "Testo di analisi in alto a destra",               "Contenuti"),
            (10, "responses", "Riquadro POSSIBLE RESPONSES",                     "Contenuti"),
            (8,  "status",    "Pannello di stato in basso",                      "Contenuti"),
            (2,  "cpu",       "Riga CPU",                                        "Statistiche"),
            (3,  "ram",       "Riga RAM",                                        "Statistiche"),
            (4,  "dsk",       "Riga disco",                                      "Statistiche"),
            (5,  "gpu",       "Riga GPU",                                        "Statistiche"),
            (6,  "info",      "Info: TMP / NET / UP",                            "Statistiche"),
            (17, "bigstats",  "Statistiche a caratteri grandi",                  "Statistiche"),
            (11, "grid",      "Griglia a punti",                                 "Effetti"),
            (12, "scanbar",   "Barra di scansione",                              "Effetti"),
            (13, "noise",     "Disturbo e righe di glitch",                      "Effetti"),
            (14, "vignette",  "Visione rossa: centro chiaro, bordi scuri",       "Effetti"),
            (15, "scanlines", "Scanline",                                        "Effetti"),
            (16, "typeon",    "Scrittura progressiva del testo",                 "Effetti"),
        ],
    },
    "cyberpunk": {
        "title": "Cyberpunk 2077 (cyberpunk)",
        "name_label": "Nome netrunner:",
        "screens": [
            ("auto", "Automatica (monitor se arrivano dati, altrimenti Breach Protocol)"),
            ("hud", "Monitor di sistema (netrunner)"),
            ("breach", "Minigioco Breach Protocol"),
            ("term", "Terminale seriale"),
            ("graph", "Grafico statistiche"),
        ],
        "elements": [
            (15, "logo",       "Logo Arasaka nel boot",                          "Contenuti"),
            (0,  "header",     "Barra gialla in alto",                           "Contenuti"),
            (6,  "quickhacks", "Elenco QUICKHACKS",                              "Contenuti"),
            (7,  "scanner",    "Riquadro di scansione",                          "Contenuti"),
            (8,  "ticker",     "Notiziario N54 e riga di stato",                 "Contenuti"),
            (9,  "frames",     "Cornici ad angolo tagliato",                     "Contenuti"),
            (1,  "cpu",        "Barra CPU",                                      "Statistiche"),
            (2,  "ram",        "RAM a celle",                                    "Statistiche"),
            (3,  "gpu",        "Barra GPU",                                      "Statistiche"),
            (4,  "dsk",        "Barra disco",                                    "Statistiche"),
            (5,  "info",       "Info: temperatura, rete, uptime",                "Statistiche"),
            (10, "glitch",     "Glitch (fasce spostate)",                        "Effetti"),
            (11, "chroma",     "Separazione RGB sui titoli",                     "Effetti"),
            (12, "scanlines",  "Scanline",                                       "Effetti"),
            (13, "noise",      "Pixel di disturbo",                              "Effetti"),
            (14, "typeon",     "Scrittura progressiva del testo",                "Effetti"),
        ],
    },
    "alien": {
        "title": "Alien MU-TH-UR 6000 (alien)",
        "name_label": "Nome ufficiale:",
        "screens": [
            ("auto", "Automatica (nave se arrivano dati, altrimenti tracker)"),
            ("system", "Stato della nave"),
            ("tracker", "Motion tracker"),
            ("term", "Terminale seriale"),
            ("graph", "Grafico statistiche"),
        ],
        "elements": [
            (14, "logo",      "Logo Weyland-Yutani nel boot",                    "Contenuti"),
            (0,  "header",    "Intestazione Weyland-Yutani",                     "Contenuti"),
            (1,  "title",     "Titolo sezione e linea ====",                     "Contenuti"),
            (7,  "log",       "Conversazione / righe di stato",                  "Contenuti"),
            (8,  "cursor",    "Prompt con cursore lampeggiante",                 "Contenuti"),
            (15, "blips",     "Contatti sul motion tracker",                     "Contenuti"),
            (2,  "cpu",       "REACTOR (CPU)",                                   "Statistiche"),
            (3,  "ram",       "LIFE SUPPORT (RAM)",                              "Statistiche"),
            (4,  "dsk",       "CARGO HOLD (disco)",                              "Statistiche"),
            (5,  "gpu",       "MAIN DRIVE (GPU)",                                "Statistiche"),
            (6,  "info",      "Temperature, comunicazioni, ibernazione",         "Statistiche"),
            (9,  "scanlines", "Scanline (righe scure alternate)",                "Effetti CRT"),
            (10, "scanbar",   "Barra di scansione che scende",                   "Effetti CRT"),
            (11, "glow",      "Bagliore centrale / bordi scuri",                 "Effetti CRT"),
            (12, "flicker",   "Sfarfallio del fosforo",                          "Effetti CRT"),
            (13, "typeon",    "Scrittura progressiva del testo",                 "Effetti CRT"),
        ],
    },
}

# Grafico della schermata "graph" (comando G,<chiave>)
GRAPHS = [
    ("auto", "Automatico (a rotazione ogni 8 s)"),
    ("cpu", "CPU"),
    ("ram", "RAM"),
    ("dsk", "Disco"),
    ("gpu", "GPU"),
    ("tmp", "Temperatura GPU"),
    ("net", "Rete"),
    ("png", "Latenza (ping)"),
]


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
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0) | getattr(subprocess, "DETACHED_PROCESS", 0)
    subprocess.Popen([sys.executable, os.path.join(HERE, "pc_stats.py")], creationflags=flags, close_fds=True)


class App:
    def __init__(self, root):
        self.root = root
        root.title("Pico Display - debug")
        self.ser = None
        self.lock = threading.Lock()
        self.rx = queue.Queue()
        self.running = True

        self.cfg = self.load_config()
        self.build = tk.StringVar(value=self.cfg.get("build") if self.cfg.get("build") in BUILDS else "fallout")
        self.port = tk.StringVar(value=self.cfg.get("port", ""))
        self.status = tk.StringVar(value="Non collegato")
        self.stats_txt = tk.StringVar(value="-")
        self.raw = tk.StringVar()
        self.send_stats = tk.BooleanVar(value=True)
        self.stats_on = True  # copia letta dal thread delle statistiche (le variabili Tk solo dal thread principale)
        self.el_vars, self.screen, self.name = {}, tk.StringVar(), tk.StringVar()
        self.graph = tk.StringVar()
        self.off_var = tk.IntVar(value=300)
        self.al_cpu, self.al_gpu, self.al_temp = tk.IntVar(value=90), tk.IntVar(value=90), tk.IntVar(value=85)
        self.al_beep = tk.BooleanVar(value=True)
        self.state_seen = False
        self.stopped_stats = stop_pc_stats()

        self.build_static_ui()
        self.load_build_vars()
        self.build_dynamic_ui()
        threading.Thread(target=self.stats_loop, daemon=True).start()
        threading.Thread(target=self.reader_loop, daemon=True).start()
        root.after(100, self.poll_rx)
        root.after(300, self.connect)
        root.protocol("WM_DELETE_WINDOW", self.close)

    # ---------- interfaccia ----------
    def build_static_ui(self):
        pad = {"padx": 6, "pady": 3}
        top = ttk.Frame(self.root)
        top.pack(fill="x", **pad)
        ttk.Label(top, text="Porta:").pack(side="left")
        ttk.Entry(top, textvariable=self.port, width=8).pack(side="left", padx=4)
        ttk.Label(top, text="(vuoto = automatica)").pack(side="left")
        ttk.Button(top, text="Collega", command=self.connect).pack(side="left", padx=4)
        ttk.Button(top, text="Scollega", command=self.disconnect).pack(side="left")
        ttk.Label(top, text="   Build:").pack(side="left")
        cb = ttk.Combobox(top, textvariable=self.build, values=list(BUILDS), width=8, state="readonly")
        cb.pack(side="left", padx=4)
        cb.bind("<<ComboboxSelected>>", lambda _: self.request_build(self.build.get()))
        ttk.Label(top, textvariable=self.status).pack(side="left", padx=8)

        body = ttk.Frame(self.root)
        body.pack(fill="both", expand=True, **pad)
        self.left = ttk.Frame(body)
        self.left.pack(side="left", fill="y")
        right = ttk.Frame(body)
        right.pack(side="left", fill="both", expand=True, padx=(10, 0))

        nf = ttk.LabelFrame(right, text="Dati")
        nf.pack(fill="x", pady=3)
        r1 = ttk.Frame(nf)
        r1.pack(fill="x")
        self.name_label = ttk.Label(r1, text="Nome:")
        self.name_label.pack(side="left")
        ttk.Entry(r1, textvariable=self.name, width=16).pack(side="left", padx=4)
        ttk.Button(r1, text="Invia", command=self.send_name).pack(side="left")
        ro = ttk.Frame(nf)
        ro.pack(fill="x", pady=2)
        ttk.Label(ro, text="Spegni lo schermo dopo (s, 0 = mai):").pack(side="left")
        ttk.Spinbox(ro, from_=0, to=3600, increment=30, textvariable=self.off_var, width=6).pack(side="left", padx=4)
        ttk.Button(ro, text="Imposta", command=self.send_off).pack(side="left")
        ra = ttk.LabelFrame(nf, text="Allarme (0 = soglia disattivata)")
        ra.pack(fill="x", pady=2)
        for label, var, hi in (("CPU %", self.al_cpu, 100), ("GPU %", self.al_gpu, 100), ("Temp. GPU C", self.al_temp, 150)):
            ttk.Label(ra, text=label).pack(side="left", padx=(4, 0))
            ttk.Spinbox(ra, from_=0, to=hi, textvariable=var, width=4).pack(side="left", padx=(2, 4))
        ttk.Checkbutton(ra, text="Cicalino (GP13)", variable=self.al_beep).pack(side="left", padx=4)
        ttk.Button(ra, text="Imposta", command=self.send_alarm).pack(side="left", padx=4)
        rg = ttk.Frame(nf)
        rg.pack(fill="x", pady=2)
        ttk.Label(rg, text="Grafico:").pack(side="left")
        gc = ttk.Combobox(rg, values=[label for _, label in GRAPHS], width=32, state="readonly")
        gc.pack(side="left", padx=4)
        gc.bind("<<ComboboxSelected>>", lambda _: self.pick_graph(GRAPHS[gc.current()][0]))
        self.graph_combo = gc
        ttk.Label(rg, text="(passa alla schermata Grafico)").pack(side="left")
        ttk.Checkbutton(nf, text="Invia le statistiche del PC (ogni secondo)", variable=self.send_stats,
                        command=lambda: setattr(self, "stats_on", self.send_stats.get())).pack(anchor="w")
        ttk.Label(nf, textvariable=self.stats_txt, font=("Consolas", 9)).pack(anchor="w")
        r2 = ttk.Frame(nf)
        r2.pack(fill="x", pady=2)
        ttk.Label(r2, text="Riga da inviare:").pack(side="left")
        e = ttk.Entry(r2, textvariable=self.raw, width=26)
        e.pack(side="left", padx=4)
        e.bind("<Return>", lambda _: self.send_raw())
        ttk.Button(r2, text="Invia", command=self.send_raw).pack(side="left")

        notef = ttk.LabelFrame(right, text="Note per Claude (cosa ti piace, cosa cambiare)")
        notef.pack(fill="both", expand=True, pady=3)
        self.notes = tk.Text(notef, height=6, width=50, wrap="word")
        self.notes.pack(fill="both", expand=True)
        cf = ttk.Frame(right)
        cf.pack(fill="x", pady=3)
        ttk.Button(cf, text="Salva configurazione", command=self.save_config).pack(side="left")
        ttk.Button(cf, text="Copia per Claude", command=self.copy_for_claude).pack(side="left", padx=4)

        lf = ttk.LabelFrame(right, text="Risposte del Pico")
        lf.pack(fill="both", expand=True, pady=3)
        self.log = tk.Text(lf, height=8, width=50, state="disabled", font=("Consolas", 9))
        self.log.pack(fill="both", expand=True)

    def build_dynamic_ui(self):
        """Caselle e schermate della build corrente (ricostruite quando cambia la build)."""
        for w in self.left.winfo_children():
            w.destroy()
        b = BUILDS[self.build.get()]
        self.name_label.configure(text=b["name_label"])

        sf = ttk.LabelFrame(self.left, text=f"Schermata - {b['title']}")
        sf.pack(fill="x", pady=3)
        for key, label in b["screens"]:
            ttk.Radiobutton(sf, text=label, value=key, variable=self.screen, command=self.send_screen).pack(anchor="w")
        bf = ttk.Frame(sf)
        bf.pack(anchor="w", pady=2)
        ttk.Button(bf, text="Rivedi il boot", command=lambda: self.send("M,boot")).pack(side="left")
        ttk.Button(bf, text="Pulisci terminale", command=lambda: self.send("clear")).pack(side="left", padx=4)

        groups = {}
        for bit, key, label, group in b["elements"]:
            if group not in groups:
                groups[group] = ttk.LabelFrame(self.left, text=group)
                groups[group].pack(fill="x", pady=3)
            ttk.Checkbutton(groups[group], text=label, variable=self.el_vars[key], command=self.send_flags).pack(anchor="w")
        af = ttk.Frame(self.left)
        af.pack(fill="x", pady=2)
        ttk.Button(af, text="Tutto acceso", command=lambda: self.set_all(True)).pack(side="left")
        ttk.Button(af, text="Tutto spento", command=lambda: self.set_all(False)).pack(side="left", padx=4)

    def load_build_vars(self):
        """Carica nelle variabili Tk la configurazione salvata della build corrente."""
        name = self.build.get()
        b, c = BUILDS[name], self.cfg.get(name, {})
        self.el_vars = {k: tk.BooleanVar(value=c.get("elements", {}).get(k, True)) for _, k, _, _ in b["elements"]}
        screens = [k for k, _ in b["screens"]]
        self.screen.set(c.get("screen") if c.get("screen") in screens else screens[0])
        self.name.set(c.get("name", "SAMU"))
        keys = [k for k, _ in GRAPHS]
        self.graph.set(c.get("graph") if c.get("graph") in keys else "cpu")
        if hasattr(self, "graph_combo"):
            self.graph_combo.current(keys.index(self.graph.get()))
        self.notes.delete("1.0", "end")
        self.notes.insert("1.0", c.get("notes", ""))

    def switch_build(self, name, send):
        if name not in BUILDS:
            return
        self.cfg[self.build.get()] = self.build_config()  # conserva le scelte della build lasciata
        self.build.set(name)
        self.load_build_vars()
        self.build_dynamic_ui()
        if send:
            self.send_all()

    def logmsg(self, msg):
        self.log.configure(state="normal")
        self.log.insert("end", msg + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    # ---------- seriale ----------
    def connect(self):
        self.disconnect()
        port = self.port.get().strip() or find_port()
        if not port:
            self.status.set("Pico non trovato")
            return
        try:
            s = serial.Serial(port, 115200, timeout=0.2, write_timeout=1)
        except serial.SerialException as e:
            self.status.set(f"Errore su {port}")
            self.logmsg(f"Impossibile aprire {port}: {e}\n(pc_stats.py o il Serial Monitor sono aperti? Chiudili.)")
            return
        with self.lock:
            self.ser = s
        self.status.set(f"Collegato a {port}")
        self.logmsg(f"Collegato a {port}, leggo lo stato dal Pico...")
        self.state_seen = False
        self.send("?")
        self.send("D")
        self.root.after(3500, self.fallback_push)

    def disconnect(self):
        with self.lock:
            if self.ser:
                try:
                    self.ser.close()
                except Exception:
                    pass
            self.ser = None
        self.status.set("Non collegato")

    def send(self, line):
        with self.lock:
            s = self.ser
            if not s:
                return False
            try:
                s.write((line + "\n").encode())
                return True
            except (serial.SerialException, OSError):
                self.ser = None
        self.root.after(0, lambda: self.status.set("Collegamento perso"))
        return False

    def reader_loop(self):
        while self.running:
            with self.lock:
                s = self.ser
            if not s:
                time.sleep(0.3)
                continue
            try:
                data = s.readline()
            except (serial.SerialException, OSError, TypeError):
                time.sleep(0.3)
                continue
            if data:
                self.rx.put(data.decode(errors="replace").strip())

    def poll_rx(self):
        while not self.rx.empty():
            msg = self.rx.get()
            self.logmsg("< " + msg)
            if msg.startswith("ID,"):
                name = msg[3:].strip()
                self.logmsg(f"Sul Pico c'e' il tema '{name}'")
                if name != self.build.get():
                    self.switch_build(name, send=False)
                    self.send("D")  # legge lo stato salvato di questo tema
            elif msg.startswith("STATE,"):
                self.apply_state(msg)
        if self.running:
            self.root.after(100, self.poll_rx)

    def request_build(self, name):
        """Chiede al Pico di cambiare tema; la vista cambia quando arriva ID,<tema>."""
        if not self.send("T," + name):
            self.switch_build(name, send=False)  # non collegato: cambia solo la vista

    def fallback_push(self):
        if not self.state_seen and self.ser:
            self.logmsg("Il Pico non ha risposto a D (firmware vecchio?): invio la configurazione della GUI")
            self.send_all()

    def apply_state(self, msg):
        """STATE,tema,elementi,schermata,spegnimento,metrica,auto,cpu,gpu,temp,cicalino,nome: imposta la GUI come il Pico."""
        p = msg.split(",", 11)
        if len(p) < 12:
            return
        _, tid, flags, screen, off, gm, ga, ac, ag, at, ab, name = p
        if tid not in BUILDS:
            return
        if tid != self.build.get():
            self.switch_build(tid, send=False)
        b = BUILDS[tid]
        flags, scr = int(flags), int(screen)
        if b.get("flags", True) and flags:  # 0 = non ancora impostato sul Pico
            for bit, key, _, _ in b["elements"]:
                self.el_vars[key].set(bool((flags >> bit) & 1))
        screens = [k for k, _ in b["screens"]]
        if scr < len(screens):
            self.screen.set(screens[scr])
        self.off_var.set(int(off))
        keys = [k for k, _ in GRAPHS]
        metric = ["cpu", "ram", "dsk", "gpu", "tmp", "net", "png"][min(int(gm), 6)]
        self.al_cpu.set(int(ac)); self.al_gpu.set(int(ag)); self.al_temp.set(int(at)); self.al_beep.set(bool(int(ab)))
        self.graph.set("auto" if int(ga) else metric)
        self.graph_combo.current(keys.index(self.graph.get()))
        if name:
            self.name.set(name)
        self.state_seen = True
        self.logmsg("Stato letto dal Pico: caselle aggiornate")

    def send_alarm(self):
        try:
            self.send(f"A,{int(self.al_cpu.get())},{int(self.al_gpu.get())},{int(self.al_temp.get())},{1 if self.al_beep.get() else 0}")
        except tk.TclError:
            pass

    def send_off(self):
        try:
            self.send(f"B,{int(self.off_var.get())}")
        except tk.TclError:
            pass

    # ---------- comandi ----------
    def mask(self):
        return sum(1 << bit for bit, key, _, _ in BUILDS[self.build.get()]["elements"] if self.el_vars[key].get())

    def has_flags(self):
        return BUILDS[self.build.get()].get("flags", True)

    def send_flags(self):
        if self.has_flags():
            self.send(f"C,{self.mask()}")

    def send_screen(self):
        self.send(f"M,{self.screen.get()}")

    def send_name(self):
        if self.has_flags():
            self.send(f"N,{self.name.get().strip()[:16]}")

    def send_graph(self):
        self.send(f"G,{self.graph.get()}")

    def pick_graph(self, key):
        """Sceglie il grafico e passa alla schermata Grafico."""
        self.graph.set(key)
        self.send_graph()
        self.screen.set("graph")
        self.send_screen()

    def send_all(self):
        self.send_flags()
        self.send_graph()
        self.send_screen()
        self.send_name()

    def send_raw(self):
        if self.raw.get().strip():
            self.send(self.raw.get().strip())
            self.raw.set("")

    def set_all(self, value):
        for v in self.el_vars.values():
            v.set(value)
        self.send_flags()

    def stats_loop(self):
        col = Collector()
        while self.running:
            time.sleep(1)
            lines = col.lines()
            self.root.after(0, lambda s=col.summary: self.stats_txt.set(s))
            if self.stats_on:
                for line in lines:
                    self.send(line)

    # ---------- configurazione ----------
    def load_config(self):
        try:
            with open(CONFIG_JSON, encoding="utf-8") as f:
                cfg = json.load(f)
        except Exception:
            return {}
        if "elements" in cfg and "fallout" not in cfg:  # vecchio formato (solo fallout)
            cfg = {"build": "fallout", "port": cfg.get("port", ""), "fallout": cfg}
        return cfg

    def build_config(self):
        return {
            "screen": self.screen.get(),
            "name": self.name.get().strip(),
            "mask": self.mask(),
            "graph": self.graph.get(),
            "elements": {k: v.get() for k, v in self.el_vars.items()},
            "notes": self.notes.get("1.0", "end").strip(),
        }

    def summary(self):
        b = BUILDS[self.build.get()]
        c = self.build_config()
        keep = [label for _, key, label, _ in b["elements"] if c["elements"][key]]
        drop = [label for _, key, label, _ in b["elements"] if not c["elements"][key]]
        lines = [
            f"Configurazione display - build {self.build.get()} ({b['title']})",
            f"Schermata: {dict(b['screens'])[c['screen']]}",
            f"{b['name_label']} {c['name']}",
            f"Grafico: {dict(GRAPHS)[c['graph']]}",
            f"Maschera: C,{c['mask']}",
            "TENERE: " + (", ".join(keep) or "-"),
            "TOGLIERE: " + (", ".join(drop) or "-"),
        ]
        if c["notes"]:
            lines += ["Note:", c["notes"]]
        return "\n".join(lines)

    def save_config(self):
        self.cfg[self.build.get()] = self.build_config()
        self.cfg["build"] = self.build.get()
        self.cfg["port"] = self.port.get().strip()
        with open(CONFIG_JSON, "w", encoding="utf-8") as f:
            json.dump(self.cfg, f, indent=2, ensure_ascii=False)
        with open(CONFIG_TXT, "w", encoding="utf-8") as f:
            f.write(self.summary() + "\n")
        self.logmsg(f"Salvata in {CONFIG_JSON} e {CONFIG_TXT}")

    def copy_for_claude(self):
        self.save_config()
        self.root.clipboard_clear()
        self.root.clipboard_append(self.summary())
        self.logmsg("Riepilogo copiato negli appunti: incollalo nella chat con Claude.")

    def close(self):
        self.running = False
        self.save_config()
        self.disconnect()
        if self.stopped_stats:  # ripristina lo script che avevo fermato
            start_pc_stats()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    App(root)
    root.mainloop()
