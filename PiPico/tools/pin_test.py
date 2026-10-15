"""Trova su quale pin del Pico e' collegato ogni pulsante.

Uso:
    python tools/pin_test.py

Premi un pulsante alla volta: il programma scrive il GPIO (e il pin fisico) di quello premuto, sia
se il pulsante e' collegato verso GND (come previsto dal firmware) sia se e' collegato verso 3V3.
Premi Ctrl+C per uscire. Chiudi prima la GUI (la porta seriale la puo' usare un solo programma).
Richiede il firmware 1.3.0 o successivo (comando PINS) e: pip install pyserial
"""
import sys
import time

import serial

try:
    from pc_stats import find_port
except ImportError:
    from os.path import dirname
    sys.path.insert(0, dirname(__file__))
    from pc_stats import find_port

# GPIO -> pin fisico del Pico
PHYSICAL = {0: 1, 1: 2, 2: 4, 3: 5, 4: 6, 5: 7, 6: 9, 7: 10, 8: 11, 9: 12, 10: 14, 11: 15, 12: 16,
            14: 19, 15: 20, 20: 26, 26: 31, 27: 32, 28: 34}
EXPECTED = {14: "pulsante 1 (tema successivo)", 15: "pulsante 2 (schermata successiva)",
            12: "pulsante 3 (grafico successivo)", 11: "pulsante 4 (schermo acceso/spento)"}


def scan(ser, mode):
    ser.reset_input_buffer()
    ser.write(f"PINS,{mode}\n".encode())
    line = ser.readline().decode(errors="replace").strip()
    if not line.startswith("PINS,"):
        return None
    return {int(p[2:]) for p in line.split(",")[2:] if p.startswith("GP")}


def main():
    port = find_port()
    if not port:
        sys.exit("Pico non trovato")
    ser = serial.Serial(port, 115200, timeout=0.3)
    time.sleep(0.5)
    base = {"L": scan(ser, "L"), "H": scan(ser, "H")}
    time.sleep(0.2)
    base = {"L": scan(ser, "L") or set(), "H": scan(ser, "H") or set()}
    if base["L"] is None:
        sys.exit("Il Pico non risponde a PINS: serve il firmware 1.3.0 o successivo")
    ignored = base["L"] | base["H"]
    if ignored:
        print("Ignoro i pin che sono gia' a livello fisso:", ", ".join(f"GP{g}" for g in sorted(ignored)))
    print(f"Collegato a {port}. Premi i pulsanti uno alla volta (Ctrl+C per uscire)...\n")
    last = set()
    try:
        while True:
            now = set()
            for mode in ("L", "H"):
                found = scan(ser, mode) or set()
                for g in found - base[mode]:
                    now.add((g, "verso GND" if mode == "L" else "verso 3V3"))
            for g, how in sorted(now - last):
                phys = PHYSICAL.get(g, "?")
                what = EXPECTED.get(g, "non previsto dal firmware")
                note = "" if how == "verso GND" else "   <-- attenzione: il firmware si aspetta i pulsanti verso GND"
                print(f"GP{g} (pin fisico {phys}) premuto, {how}  ->  {what}{note}")
            last = now
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()


if __name__ == "__main__":
    main()
