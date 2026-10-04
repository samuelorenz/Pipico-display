"""Genera src/skynet/logo.h (maschera alpha a 8 bit) dal logo Skynet.

Uso:
    python tools/make_skynet_logo.py <logo_skynet.png>

Il logo (rosso/bianco su nero) viene reso in alpha con il canale piu' luminoso di ogni pixel
e ridimensionato a MAX_H pixel di altezza. L'immagine sorgente NON e' inclusa nel repository.
Richiede: pip install pillow numpy
"""
import sys
import numpy as np
from PIL import Image

MAX_H = 232   # altezza massima sul display 240x240
MAX_W = 236


def smoothstep(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    im = Image.open(sys.argv[1]).convert("RGB")
    a = np.asarray(im, dtype=np.float32).max(axis=2)
    alpha = Image.fromarray((smoothstep(a, 25, 255) * 255).astype(np.uint8))
    box = alpha.point(lambda v: 255 if v > 20 else 0).getbbox()  # toglie i margini neri
    alpha = alpha.crop((max(box[0] - 4, 0), max(box[1] - 4, 0), min(box[2] + 4, alpha.width), min(box[3] + 4, alpha.height)))
    s = min(MAX_H / alpha.height, MAX_W / alpha.width)
    alpha = alpha.resize((int(alpha.width * s), int(alpha.height * s)), Image.LANCZOS)
    w, h = alpha.size
    data = np.asarray(alpha, dtype=np.uint8).flatten()
    with open("src/skynet/logo.h", "w") as f:
        f.write("// Generato da tools/make_skynet_logo.py - non modificare a mano\n#pragma once\n#include <stdint.h>\n\n")
        f.write(f"const int SKYNET_LOGO_W = {w};\nconst int SKYNET_LOGO_H = {h};\n")
        f.write(f"const uint8_t SKYNET_LOGO[{w * h}] = {{\n")
        for i in range(0, len(data), 24):
            f.write("  " + ",".join(str(v) for v in data[i:i + 24]) + ",\n")
        f.write("};\n")
    alpha.save("tools/preview_skynet.png")
    print("logo", w, "x", h)
