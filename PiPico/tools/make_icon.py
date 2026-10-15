"""Genera tools/app.ico: l'icona di PiPico Display Config (un piccolo monitor CRT con linee di testo).

Uso:  python tools/make_icon.py        Richiede: pip install pillow
"""
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))


def draw(size):
    s = size / 256
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((8 * s, 8 * s, 248 * s, 248 * s), radius=44 * s, fill=(28, 30, 36, 255))          # corpo
    d.rounded_rectangle((34 * s, 40 * s, 222 * s, 196 * s), radius=22 * s, fill=(6, 18, 10, 255),
                        outline=(120, 255, 160, 255), width=max(2, int(6 * s)))                          # schermo
    for i, w in enumerate((130, 100, 150, 70)):                                                          # righe di testo
        y = 66 + i * 30
        d.rounded_rectangle((56 * s, y * s, (56 + w) * s, (y + 12) * s), radius=5 * s, fill=(120, 255, 160, 255))
    d.rectangle((120 * s, 196 * s, 136 * s, 214 * s), fill=(90, 94, 104, 255))                          # collo
    d.rounded_rectangle((84 * s, 212 * s, 172 * s, 228 * s), radius=7 * s, fill=(90, 94, 104, 255))     # base
    return im


if __name__ == "__main__":
    sizes = [16, 24, 32, 48, 64, 128, 256]
    draw(256).save(os.path.join(HERE, "app.ico"), sizes=[(s, s) for s in sizes])
    print("creata", os.path.join(HERE, "app.ico"))
