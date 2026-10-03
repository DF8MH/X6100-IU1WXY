#!/usr/bin/env python3
"""Field Ops icons: rootfs/usr/share/x6100/field/<name>_<size>.png, drawn 4x and scaled down."""
import os
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(__file__), "..", "rootfs", "usr", "share", "x6100", "field")
SS = 4


def canvas(size):
    im = Image.new("RGBA", (size * SS, size * SS), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im), size * SS


def save(im, name, size):
    im.resize((size, size), Image.LANCZOS).save(os.path.join(OUT, f"{name}_{size}.png"), optimize=True)


def mountain(size):
    im, d, s = canvas(size)
    u = s / 100
    # back peak, front peak, snow caps
    d.polygon([(48 * u, 92 * u), (72 * u, 30 * u), (98 * u, 92 * u)], fill=(150, 160, 175, 255))
    d.polygon([(2 * u, 92 * u), (38 * u, 12 * u), (76 * u, 92 * u)], fill=(225, 230, 238, 255))
    d.polygon([(38 * u, 12 * u), (27 * u, 36 * u), (34 * u, 32 * u), (39 * u, 40 * u), (45 * u, 31 * u),
               (50 * u, 38 * u)], fill=(255, 255, 255, 255))
    d.polygon([(72 * u, 30 * u), (64 * u, 50 * u), (70 * u, 46 * u), (75 * u, 52 * u), (80 * u, 46 * u)],
              fill=(235, 240, 245, 255))
    d.line([(38 * u, 12 * u), (76 * u, 92 * u)], fill=(170, 178, 190, 255), width=int(2 * u))
    return im


def trees(size):
    im, d, s = canvas(size)
    u = s / 100

    def tree(cx, top, w, h, col):
        d.rectangle([cx - 3 * u, top + h, cx + 3 * u, top + h + 9 * u], fill=(120, 80, 45, 255))
        for k in range(3):
            y0 = top + k * h / 3.2
            hw = w * (0.45 + k * 0.28)
            d.polygon([(cx, y0), (cx - hw, y0 + h / 2.1), (cx + hw, y0 + h / 2.1)], fill=col)

    tree(24 * u, 26 * u, 26 * u, 52 * u, (46, 160, 70, 255))
    tree(76 * u, 26 * u, 26 * u, 52 * u, (46, 160, 70, 255))
    tree(50 * u, 6 * u, 32 * u, 66 * u, (70, 200, 95, 255))
    return im


def bands(size):
    im, d, s = canvas(size)
    u = s / 100
    for k, h in enumerate((30, 48, 66, 86)):
        x = 10 * u + k * 22 * u
        d.rounded_rectangle([x, (94 - h) * u, x + 16 * u, 94 * u], radius=3 * u, fill=(70, 170, 255, 255))
    return im


def logbook(size):
    im, d, s = canvas(size)
    u = s / 100
    d.rounded_rectangle([16 * u, 6 * u, 86 * u, 94 * u], radius=6 * u, fill=(225, 230, 238, 255))
    d.rectangle([16 * u, 6 * u, 28 * u, 94 * u], fill=(150, 160, 175, 255))
    for k in range(5):
        y = (24 + k * 14) * u
        d.rounded_rectangle([36 * u, y, 78 * u, y + 5 * u], radius=2 * u, fill=(110, 120, 135, 255))
    return im


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, fn in (("sota", mountain), ("pota", trees), ("bands", bands), ("log", logbook)):
        for size in (96, 64):
            save(fn(size), name, size)
    print(sorted(os.listdir(OUT)))
