"""Invia le statistiche del PC al Pico via seriale USB (una volta al secondo).

Righe inviate:
    S,cpu%,ram%,disco%,gpu%,tempGpu,netKB/s,uptimeOre      es. S,34,62,48,21,55,120,5
    X,tempCpu,batteria%,inCarica                           es. X,62,85,1   (0 / -1 = non disponibile)
    P,processo,cpu%                                        es. P,CHROME,23 (processo che usa piu' CPU)

Uso:
    python tools/pc_stats.py                 # trova da solo la porta del Pico
    python tools/pc_stats.py --port COM7     # porta indicata a mano
    python tools/pc_stats.py --test          # stampa i dati senza inviarli

Richiede: pip install psutil pyserial
- GPU e temperatura GPU: nvidia-smi (schede NVIDIA); senza, restano a 0.
- Temperatura CPU: serve LibreHardwareMonitor (o OpenHardwareMonitor) aperto con "Remote Web Server"
  attivo (Options > Remote Web Server > Run, porta 8085). Senza, resta a 0.
Se il Pico viene scollegato o ricaricato, lo script si riconnette da solo.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.request

import psutil
import serial
from serial.tools import list_ports

PICO_VID = 0x2E8A  # Raspberry Pi
NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)
LHM_URL = "http://127.0.0.1:8085/data.json"
IGNORED_PROCS = {"system idle process", "idle", "system", "registry", "memory compression"}


def find_port():
    for p in list_ports.comports():
        if p.vid == PICO_VID:
            return p.device
    return None


def gpu_stats():
    """(utilizzo %, temperatura C) dalla prima GPU NVIDIA, oppure (0, 0)."""
    try:
        out = subprocess.run(
            ["nvidia-smi", "--query-gpu=utilization.gpu,temperature.gpu", "--format=csv,noheader,nounits"],
            capture_output=True, text=True, timeout=3, creationflags=NO_WINDOW,
        ).stdout.strip().splitlines()[0]
        util, temp = (int(float(x)) for x in out.split(","))
        return util, temp
    except Exception:
        return 0, 0


def _walk(node, found):
    text = node.get("Text", "")
    val = node.get("Value", "")
    if isinstance(val, str) and val.endswith("C") and re.search(r"[0-9]", val):
        found.append((text, float(re.search(r"-?[0-9]+(?:[.,][0-9]+)?", val).group().replace(",", "."))))
    for ch in node.get("Children", []):
        _walk(ch, found)


class Collector:
    """Raccoglie le statistiche e le trasforma nelle righe S / X / P."""

    def __init__(self):
        self.disk = os.path.splitdrive(sys.executable)[0] + "\\" if os.name == "nt" else "/"
        self.last_net = psutil.net_io_counters()
        self.last_t = time.time()
        self.cores = psutil.cpu_count() or 1
        self.lhm_retry = 0.0
        self.lhm_temp = 0
        self.lhm_at = 0.0
        self.summary = "-"
        psutil.cpu_percent(None)  # la prima chiamata restituisce 0: la scarto
        for p in psutil.process_iter(["cpu_percent"]):
            pass

    def cpu_temp(self):
        """Temperatura CPU da LibreHardwareMonitor, se il suo web server e' attivo."""
        now = time.time()
        if now < self.lhm_retry or now - self.lhm_at < 2:
            return self.lhm_temp
        self.lhm_at = now
        try:
            with urllib.request.urlopen(LHM_URL, timeout=0.6) as r:
                found = []
                _walk(json.load(r), found)
            cpu = [v for name, v in found if re.search(r"CPU Package|Core \(Tctl|Core \(Tdie|CPU \(Tctl|Core Average|^Core #", name)]
            self.lhm_temp = int(max(cpu)) if cpu else 0
        except Exception:
            self.lhm_temp = 0
            self.lhm_retry = now + 30  # non c'e': riprova fra 30 secondi
        return self.lhm_temp

    def top_process(self):
        best = ("", 0.0)
        for p in psutil.process_iter(["name", "cpu_percent"]):
            try:
                name = (p.info["name"] or "").strip()
                use = (p.info["cpu_percent"] or 0.0) / self.cores
                if name.lower() in IGNORED_PROCS or name.lower().replace(".exe", "") in IGNORED_PROCS:
                    continue
                if use > best[1]:
                    best = (name, use)
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        name = re.sub(r"\.exe$", "", best[0], flags=re.I).upper().replace(",", " ")
        name = "".join(ch for ch in name if 32 <= ord(ch) < 127)[:15]
        return name, int(best[1])

    def lines(self):
        now = time.time()
        net = psutil.net_io_counters()
        kbs = int((net.bytes_recv + net.bytes_sent - self.last_net.bytes_recv - self.last_net.bytes_sent)
                  / 1024 / max(now - self.last_t, 0.1))
        self.last_net, self.last_t = net, now
        gpu, gtemp = gpu_stats()
        cpu = int(psutil.cpu_percent(None))
        ram = int(psutil.virtual_memory().percent)
        dsk = int(psutil.disk_usage(self.disk).percent)
        up = int((now - psutil.boot_time()) / 3600)
        ctemp = self.cpu_temp()
        bat = psutil.sensors_battery()
        batt, plug = (int(bat.percent), 1 if bat.power_plugged else 0) if bat else (-1, 0)
        proc, ppct = self.top_process()

        self.summary = "CPU {}%  RAM {}%  DSK {}%  GPU {}%  {}C  NET {}K/s  UP {}h".format(cpu, ram, dsk, gpu, gtemp, kbs, up)
        self.summary += "\nCPU temp {}  batteria {}  top: {} {}%".format(
            f"{ctemp}C" if ctemp else "n.d.", f"{batt}%" if batt >= 0 else "n.d.", proc or "-", ppct)
        out = [f"S,{cpu},{ram},{dsk},{gpu},{gtemp},{kbs},{up}", f"X,{ctemp},{batt},{plug}"]
        if proc:
            out.append(f"P,{proc},{ppct}")
        return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="porta seriale (default: cerca il Pico)")
    ap.add_argument("--test", action="store_true", help="stampa soltanto, senza inviare")
    args = ap.parse_args()

    col = Collector()
    ser = None
    while True:
        time.sleep(1)
        lines = col.lines()
        print(" | ".join(lines), flush=True)
        if args.test:
            continue

        try:
            if ser is None:
                port = args.port or find_port()
                if not port:
                    print("Pico non trovato, riprovo...", flush=True)
                    continue
                ser = serial.Serial(port, 115200, timeout=0.2, write_timeout=1)
                print("Collegato a", port, flush=True)
            ser.write(("\n".join(lines) + "\n").encode())
        except (serial.SerialException, OSError) as e:
            print("Seriale persa (", e, "), mi riconnetto...", flush=True)
            try:
                ser and ser.close()
            except Exception:
                pass
            ser = None


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
