"""Anteprima del display: chiede al Pico l'ultimo fotogramma (comando SNAP) e lo salva come PNG.

Uso:
    python tools/snap.py schermata.png                # un'istantanea
    python tools/snap.py out.png --port COM7          # porta indicata a mano
    python tools/snap.py out.png --cmd "T,alien" --cmd "M,tracker" --wait 2

--cmd  invia un comando prima dell'istantanea (si puo' ripetere); --wait  secondi di attesa dopo i comandi.
La porta seriale puo' essere usata da un solo programma: chiudi la GUI e pc_stats.py prima.
Richiede: pip install pyserial pillow
"""
import argparse
import struct
import sys
import time

import serial

try:
    from pc_stats import find_port
except ImportError:  # eseguito da un'altra cartella
    sys.path.insert(0, __file__.rsplit("\\", 1)[0].rsplit("/", 1)[0])
    from pc_stats import find_port

_R5 = [(i << 3) | (i >> 2) for i in range(32)]
_G6 = [(i << 2) | (i >> 4) for i in range(64)]


def rgb565_to_rgb(data, w, h):
    """Byte RGB565 little endian -> byte RGB888 (w*h*3)."""
    vals = struct.unpack("<%dH" % (w * h), data)
    out = bytearray(len(vals) * 3)
    out[0::3] = bytes(_R5[(v >> 11) & 31] for v in vals)
    out[1::3] = bytes(_G6[(v >> 5) & 63] for v in vals)
    out[2::3] = bytes(_R5[v & 31] for v in vals)
    return bytes(out)


def rgb565_to_ppm(data, w, h):
    return b"P6\n%d %d\n255\n" % (w, h) + rgb565_to_rgb(data, w, h)


def read_exact(ser, n, timeout):
    end = time.time() + timeout
    buf = bytearray()
    while len(buf) < n and time.time() < end:
        buf += ser.read(n - len(buf))
    return bytes(buf)


def snapshot(ser, timeout=4.0):
    """Chiede SNAP su una porta gia' aperta; restituisce (w, h, byte RGB565) oppure None."""
    ser.reset_input_buffer()
    ser.write(b"SNAP\n")
    end = time.time() + timeout
    while time.time() < end:
        line = ser.readline().decode(errors="replace").strip()
        if line.startswith("SNAP,"):
            _, w, h, bpp = line.split(",")
            n = int(w) * int(h) * int(bpp)
            data = read_exact(ser, n, timeout)
            return (int(w), int(h), data) if len(data) == n else None
    return None


def main():
    from PIL import Image

    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--port")
    ap.add_argument("--cmd", action="append", default=[])
    ap.add_argument("--wait", type=float, default=1.0)
    ap.add_argument("--scale", type=int, default=2, help="ingrandimento dell'immagine salvata")
    a = ap.parse_args()

    port = a.port or find_port()
    if not port:
        sys.exit("Pico non trovato")
    ser = serial.Serial(port, 115200, timeout=0.3)
    time.sleep(0.4)
    for c in a.cmd:
        ser.write((c + "\n").encode())
        time.sleep(0.4)
    time.sleep(a.wait)
    snap = snapshot(ser)
    ser.close()
    if not snap:
        sys.exit("Nessuna risposta a SNAP: il firmware e' aggiornato?")
    w, h, data = snap
    im = Image.frombytes("RGB", (w, h), rgb565_to_rgb(data, w, h))
    if a.scale > 1:
        im = im.resize((w * a.scale, h * a.scale), Image.NEAREST)
    im.save(a.out)
    print("salvata", a.out)


if __name__ == "__main__":
    main()
