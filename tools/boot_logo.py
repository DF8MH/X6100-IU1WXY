#!/usr/bin/env python3
"""The IU1WXY boot logo (boot/logo.png, 800x480) drawn at any size, from the SVG it was made from
(docs/mockups/boot_logo.html, commit 0a944a18): antenna and waves, the call, the Italian flag line,
XIEGU X6100. A wider canvas widens only the background.

    python3 tools/boot_logo.py out.png 3840 2160     # 4K
"""
import math
import sys

from PIL import Image, ImageDraw, ImageFont

BG, WHITE, AMBER, TEXT, SUB = (6, 16, 28), (223, 231, 239), (255, 176, 32), (238, 243, 248), (143, 163, 184)
FLAG = ((0, 146, 70), (241, 242, 241), (206, 43, 55))
BOLD = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
REG = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'


def draw(w, h):
    ss = 2                                   # supersampling, then down for smooth edges
    s = h / 480 * ss                         # the 800x480 design, scaled to the height
    W, H = w * ss, h * ss
    ox = W / 2 - 400 * s                     # the design centred, wider canvases add background
    P = lambda x, y: (ox + x * s, y * s)     # design coordinates to pixels

    img = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(img)
    lw = round(5 * s)

    def line(x1, y1, x2, y2, color):
        d.line([P(x1, y1), P(x2, y2)], fill=color, width=lw)
        for x, y in ((x1, y1), (x2, y2)):    # round caps
            cx, cy = P(x, y)
            d.ellipse([cx - lw / 2, cy - lw / 2, cx + lw / 2, cy + lw / 2], fill=color)

    # waves: SVG arcs through (+-x, -14 +- dy) with radius r, opacity 1 / .7 / .4 (drawn as a blend)
    ax, ay = 400, 150
    for x, top, bottom, r, opa in ((30, 44, 16, 42, 1.0), (56, 70, 42, 78, 0.7), (82, 96, 68, 114, 0.4)):
        half = (top + bottom) / 2                                            # half the chord, centred on y -14
        cx = math.sqrt(r * r - half * half) - x                              # centre offset from the axis
        ang = math.degrees(math.asin(half / r))
        color = tuple(round(BG[i] + (AMBER[i] - BG[i]) * opa) for i in range(3))
        for side in (-1, 1):
            c = P(ax + side * -cx, ay - 14)
            R = r * s + lw / 2                                               # PIL draws the width inside the box
            box = [c[0] - R, c[1] - R, c[0] + R, c[1] + R]
            a0, a1 = (180 - ang, 180 + ang) if side < 0 else (-ang, ang)
            d.arc(box, a0, a1, fill=color, width=lw)
            for a in (a0, a1):                                               # round caps
                ex, ey = c[0] + r * s * math.cos(math.radians(a)), c[1] + r * s * math.sin(math.radians(a))
                d.ellipse([ex - lw / 2 + 1, ey - lw / 2 + 1, ex + lw / 2 - 1, ey + lw / 2 - 1], fill=color)

    # antenna: mast and legs, the amber top
    line(ax, ay - 10, ax, ay + 60, WHITE)
    line(ax, ay + 60, ax - 26, ay + 92, WHITE)
    line(ax, ay + 60, ax + 26, ay + 92, WHITE)
    cx, cy = P(ax, ay - 14)
    d.ellipse([cx - 8 * s, cy - 8 * s, cx + 8 * s, cy + 8 * s], fill=AMBER)

    def spaced(text, font_path, size, spacing, y, color):
        f = ImageFont.truetype(font_path, round(size * s))
        adv = [f.getlength(ch) + spacing * s for ch in text]
        x = W / 2 - sum(adv) / 2                                             # text-anchor middle, as SVG
        for ch, a in zip(text, adv):
            d.text((x, y * s), ch, font=f, fill=color, anchor='ls')
            x += a

    spaced('IU1WXY', BOLD, 96, 10, 340, TEXT)
    for i, col in enumerate(FLAG):
        x0, y0 = P(290 + 74 * i, 362)
        d.rectangle([x0, y0, x0 + 74 * s, y0 + 6 * s], fill=col)
    spaced('XIEGU X6100', REG, 22, 8, 410, SUB)
    return img.resize((w, h), Image.LANCZOS)


if __name__ == '__main__':
    out, w, h = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    draw(w, h).save(out)
    print(out, w, h)
