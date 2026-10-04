"""Genera src/pipboy/sprites.h (maschere alpha a 8 bit) da due immagini di riferimento.

Uso:
    python tools/make_sprites.py <screenshot_pipboy.png> <logo_vaulttec.png>

- screenshot_pipboy.png: schermata STAT del Pip-Boy (il Vault Boy verde viene ritagliato)
- logo_vaulttec.png:     logo Vault-Tec (giallo su blu, viene ritagliato e binarizzato)

Le immagini sorgente NON sono incluse nel repository. Richiede: pip install pillow numpy
"""
import sys
import numpy as np
from PIL import Image, ImageFilter

# Ritagli nelle immagini di riferimento (1920x1080 e 1600x900), da regolare se cambi sorgente
VB_BOX = (718, 290, 868, 576)     # Vault Boy
VB_SCALE = 0.52                   # fattore di scala -> circa 78x149 px
LOGO_BOX = (300, 60, 1290, 780)   # testo + logo
LOGO_W = 240                      # larghezza finale (display 240 px)


def smoothstep(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def vault_boy(path):
    im = Image.open(path).convert("RGB").crop(VB_BOX)
    g = np.asarray(im, dtype=np.float32)[:, :, 1]
    # le scanline orizzontali del CRT vengono attenuate con un leggero blur verticale
    g = np.asarray(Image.fromarray(g.astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2)), dtype=np.float32)
    alpha = (smoothstep(g, 85, 195) * 255).astype(np.uint8)
    w, h = int(im.width * VB_SCALE), int(im.height * VB_SCALE)
    return Image.fromarray(alpha).resize((w, h), Image.LANCZOS)


def logo(path):
    im = Image.open(path).convert("RGB").crop(LOGO_BOX)
    r = np.asarray(im, dtype=np.float32)[:, :, 0]      # giallo = R alto, blu = R basso
    alpha = (smoothstep(r, 70, 170) * 255).astype(np.uint8)
    h = int(im.height * LOGO_W / im.width)
    return Image.fromarray(alpha).resize((LOGO_W, h), Image.LANCZOS)


def emit(f, name, img):
    w, h = img.size
    data = np.asarray(img, dtype=np.uint8).flatten()
    f.write(f"const int {name}_W = {w};\nconst int {name}_H = {h};\n")
    f.write(f"const uint8_t {name}[{w * h}] = {{\n")
    for i in range(0, len(data), 24):
        f.write("  " + ",".join(str(v) for v in data[i:i + 24]) + ",\n")
    f.write("};\n\n")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    vb, lg = vault_boy(sys.argv[1]), logo(sys.argv[2])
    vb.save("tools/preview_vb.png")
    lg.save("tools/preview_logo.png")
    with open("src/pipboy/sprites.h", "w") as f:
        f.write("// Generato da tools/make_sprites.py - non modificare a mano\n#pragma once\n#include <stdint.h>\n\n")
        emit(f, "VB_SPRITE", vb)
        emit(f, "VAULTTEC_LOGO", lg)
    print("Vault Boy", vb.size, "- logo", lg.size)
