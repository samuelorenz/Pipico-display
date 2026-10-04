"""Converte un logo in una maschera alpha a 8 bit per il display (file .h).

Uso:
    python tools/make_logo.py <immagine> <file.h> <NOME> <maxL> <maxA> [--crop x0,y0,x1,y1] [--append]

- La trasparenza viene dal canale alpha dell'immagine se c'e', altrimenti dalla luminosita'.
- I margini vuoti vengono tolti; poi l'immagine viene ridotta per stare in maxL x maxA pixel.
- --crop ritaglia prima una parte dell'immagine (coordinate in pixel dell'originale).
- --append aggiunge la maschera a un file .h esistente invece di riscriverlo.
- --dim-gray scurisce i grigi (poco saturi e non bianchi), per far risaltare il resto del logo.

Nel .h vengono scritti <NOME>_W, <NOME>_H e l'array <NOME>[]. Le immagini sorgente NON
sono incluse nel repository. Richiede: pip install pillow numpy

Esempi usati in questo progetto:
    python tools/make_logo.py robco.png src/fallout/logo.h ROBCO_LOGO 220 110
    python tools/make_logo.py arasaka.png src/cyberpunk/logo.h ARASAKA_LOGO 232 40
    python tools/make_logo.py arasaka.png src/cyberpunk/logo.h ARASAKA_EMBLEM 96 96 --crop 0,425,230,655 --append
    python tools/make_logo.py weyland.png src/alien/logo.h WY_LOGO 224 110 --dim-gray
"""
import argparse

import numpy as np
from PIL import Image


def smoothstep(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("out")
    ap.add_argument("name")
    ap.add_argument("max_w", type=int)
    ap.add_argument("max_h", type=int)
    ap.add_argument("--crop")
    ap.add_argument("--append", action="store_true")
    ap.add_argument("--dim-gray", action="store_true")
    a = ap.parse_args()

    im = Image.open(a.image).convert("RGBA")
    if a.crop:
        im = im.crop(tuple(int(v) for v in a.crop.split(",")))
    px = np.asarray(im, dtype=np.float32)
    alpha = px[:, :, 3]
    if alpha.min() > 250:  # nessuna trasparenza: uso la luminosita'
        alpha = smoothstep(px[:, :, :3].max(axis=2), 25, 255) * 255
    if a.dim_gray:
        rgb = px[:, :, :3]
        mx, mn = rgb.max(axis=2), rgb.min(axis=2)
        sat = (mx - mn) / np.maximum(mx, 1)
        gray = (sat < 0.15) & (mx < 225)
        alpha = np.where(gray, alpha * 0.5, alpha)
    mask = Image.fromarray(alpha.astype(np.uint8))
    box = mask.point(lambda v: 255 if v > 20 else 0).getbbox()  # toglie i margini vuoti
    mask = mask.crop(box)
    s = min(a.max_w / mask.width, a.max_h / mask.height)
    mask = mask.resize((max(1, int(mask.width * s)), max(1, int(mask.height * s))), Image.LANCZOS)

    w, h = mask.size
    data = np.asarray(mask, dtype=np.uint8).flatten()
    with open(a.out, "a" if a.append else "w") as f:
        if not a.append:
            f.write("// Generato da tools/make_logo.py - non modificare a mano\n#pragma once\n#include <stdint.h>\n\n")
        f.write(f"const int {a.name}_W = {w};\nconst int {a.name}_H = {h};\n")
        f.write(f"const uint8_t {a.name}[{w * h}] = {{\n")
        for i in range(0, len(data), 24):
            f.write("  " + ",".join(str(v) for v in data[i:i + 24]) + ",\n")
        f.write("};\n\n")
    mask.save(a.out.rsplit(".", 1)[0] + "_" + a.name.lower() + "_preview.png")
    print(a.name, w, "x", h)


if __name__ == "__main__":
    main()
