#!/usr/bin/env python3
"""The IU1WXY boot logo drawn at any size, from the SVG it was made from (docs/mockups/boot_logo.html, commit
0a944a18): antenna and waves, the call, the Italian flag line, XIEGU X6100, CUSTOM FIRMWARE BY IU1WXY, as the
kernel shows it (AetherX6100Buildroot board/X6100/linux/logo.png, 800x480).

Landscape: the 800x480 design centred, a wider or taller canvas adds background. Upright (a phone): the
antenna big in the upper part, the call in the middle, the footer at the bottom.

    python3 tools/boot_logo.py out.png 800 480       # the radio's
    python3 tools/boot_logo.py out.png 3840 2160     # 4K
    python3 tools/boot_logo.py out.png 1440 3200     # a phone, upright
"""
import math
import sys

from PIL import Image, ImageDraw, ImageFont

BG, WHITE, AMBER, TEXT, SUB = (6, 16, 28), (223, 231, 239), (255, 176, 32), (238, 243, 248), (143, 163, 184)
FOOT = (90, 114, 144)
FLAG = ((0, 146, 70), (241, 242, 241), (206, 43, 55))
BOLD = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
REG = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'


def draw(w, h):
    ss = 2                                   # supersampling, then down for smooth edges
    W, H = w * ss, h * ss
    img = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(img)

    def antenna(cx0, cy0, s):
        """The antenna and waves of the design (centred at 400,150), its point (0,0) at cx0,cy0."""
        lw = round(5 * s)
        P = lambda x, y: (cx0 + x * s, cy0 + y * s)

        def line(x1, y1, x2, y2):
            d.line([P(x1, y1), P(x2, y2)], fill=WHITE, width=lw)
            for x, y in ((x1, y1), (x2, y2)):                                # round caps
                cx, cy = P(x, y)
                d.ellipse([cx - lw / 2, cy - lw / 2, cx + lw / 2, cy + lw / 2], fill=WHITE)

        # SVG arcs through (+-x, -top) and (+-x, bottom), radius r, opacity 1 / .7 / .4 (a blend)
        for x, top, bottom, r, opa in ((30, 44, 16, 42, 1.0), (56, 70, 42, 78, 0.7), (82, 96, 68, 114, 0.4)):
            half = (top + bottom) / 2                                        # half the chord, centred on y -14
            off = math.sqrt(r * r - half * half) - x                         # centre offset from the axis
            ang = math.degrees(math.asin(half / r))
            color = tuple(round(BG[i] + (AMBER[i] - BG[i]) * opa) for i in range(3))
            for side in (-1, 1):
                c = P(side * -off, -14)
                R = r * s + lw / 2                                           # PIL draws the width inside the box
                a0, a1 = (180 - ang, 180 + ang) if side < 0 else (-ang, ang)
                d.arc([c[0] - R, c[1] - R, c[0] + R, c[1] + R], a0, a1, fill=color, width=lw)
                for a in (a0, a1):                                           # round caps
                    ex = c[0] + r * s * math.cos(math.radians(a))
                    ey = c[1] + r * s * math.sin(math.radians(a))
                    d.ellipse([ex - lw / 2 + 1, ey - lw / 2 + 1, ex + lw / 2 - 1, ey + lw / 2 - 1], fill=color)
        line(0, -10, 0, 60)
        line(0, 60, -26, 92)
        line(0, 60, 26, 92)
        cx, cy = P(0, -14)
        d.ellipse([cx - 8 * s, cy - 8 * s, cx + 8 * s, cy + 8 * s], fill=AMBER)

    def spaced(text, font_path, size, spacing, y, color, s):
        """SVG text-anchor middle with letter-spacing, baseline at y (pixels)"""
        f = ImageFont.truetype(font_path, round(size * s))
        adv = [f.getlength(ch) + spacing * s for ch in text]
        x = W / 2 - sum(adv) / 2
        for ch, a in zip(text, adv):
            d.text((x, y), ch, font=f, fill=color, anchor='ls')
            x += a

    def flag(y, s):
        for i, col in enumerate(FLAG):
            x0 = W / 2 + (-111 + 74 * i) * s
            d.rectangle([x0, y, x0 + 74 * s, y + 6 * s], fill=col)

    if H <= W * 1.2:   # landscape: the 800x480 design, centred
        s = min(W / 800, H / 480)
        ox, oy = W / 2 - 400 * s, H / 2 - 240 * s
        antenna(ox + 400 * s, oy + 150 * s, s)
        spaced('IU1WXY', BOLD, 96, 10, oy + 340 * s, TEXT, s)
        flag(oy + 362 * s, s)
        spaced('XIEGU X6100', REG, 22, 8, oy + 410 * s, SUB, s)
        spaced('CUSTOM FIRMWARE BY IU1WXY', REG, 12, 3.5, oy + 453 * s, FOOT, s)
    else:              # upright: the call as wide as the landscape design allows, the antenna twice its size
        s, sa = W / 800 * 0.98, W / 800 * 0.98 * 2.2
        # the block from the outer waves (-110) to XIEGU X6100 centred a little over the middle
        cy = H * 0.45 - (-110 * sa + (92 + 14) * sa + 190 * s) / 2
        antenna(W / 2, cy, sa)
        y = cy + (92 + 14) * sa + 120 * s                                  # under the antenna's legs
        spaced('IU1WXY', BOLD, 96, 10, y, TEXT, s)
        flag(y + 22 * s, s)
        spaced('XIEGU X6100', REG, 22, 8, y + 70 * s, SUB, s)
        spaced('CUSTOM FIRMWARE BY IU1WXY', REG, 12, 3.5, H * 0.95, FOOT, s)
    return img.resize((w, h), Image.LANCZOS)


if __name__ == '__main__':
    out, w, h = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    draw(w, h).save(out)
    print(out, w, h)
