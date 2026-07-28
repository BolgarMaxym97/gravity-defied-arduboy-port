#!/usr/bin/env python3
"""Рендерить спрайт мотоцикла в PNG під різними кутами.

Таблиці парсяться прямо з TrialsFX/bike_sprite.h, а не дублюються тут —
інакше превʼю з часом розійшлося б із тим, що реально малює прошивка.

  python3 tools/sprite_preview.py [out.png]
"""
import math
import re
import sys
from PIL import Image, ImageDraw

HEADER = "TrialsFX/bike_sprite.h"
SCALE = 10
R_WHEEL = 4


def parse_arrays(src):
    def grab(name):
        m = re.search(name + r"\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
        body = re.sub(r"//[^\n]*", "", m.group(1))
        return [t.strip() for t in body.split(",") if t.strip()]

    names = re.search(r"enum : uint8_t \{(.*?)\};", src, re.S).group(1)
    names = [n.strip() for n in re.sub(r"//[^\n]*", "", names).split(",") if n.strip()]
    idx = {n: i for i, n in enumerate(names)}

    pts = [int(v) for v in grab("BIKE_PTS")]
    pts = list(zip(pts[0::2], pts[1::2]))
    lines = [idx[n] for n in grab("BIKE_LINES")]
    lines = list(zip(lines[0::2], lines[1::2]))
    spoke = [int(v) for v in grab("SPOKE")]
    spoke = list(zip(spoke[0::2], spoke[1::2]))
    return idx, pts, lines, spoke


def project(pts, ux, uy, rx, ry):
    out = []
    for a, h in pts:
        out.append((rx + ((a * ux + h * uy + 128) >> 8),
                    ry + ((a * uy - h * ux + 128) >> 8)))
    return out


def bresenham(x0, y0, x1, y1):
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        yield x0, y0
        if x0 == x1 and y0 == y1:
            return
        e2 = 2 * err
        if e2 >= dy:
            err += dy; x0 += sx
        if e2 <= dx:
            err += dx; y0 += sy


def circle_pts(cx, cy, r):
    x, y, e = r, 0, 1 - r
    while x >= y:
        for px, py in ((x, y), (y, x), (-y, x), (-x, y),
                       (-x, -y), (-y, -x), (y, -x), (x, -y)):
            yield cx + px, cy + py
        y += 1
        if e < 0:
            e += 2 * y + 1
        else:
            x -= 1; e += 2 * (y - x) + 1


def render(idx, pts, lines, spoke, deg, phase, cell=(46, 34)):
    W, H = cell
    px = set()
    a = math.radians(deg)
    ux = round(math.cos(a) * 256)
    uy = round(math.sin(a) * 256)
    P = project(pts, ux, uy, 14, 22)

    for i, j in lines:
        px.update(bresenham(*P[i], *P[j]))
    for w in (idx["P_RAXLE"], idx["P_FAXLE"]):
        px.update(circle_pts(*P[w], R_WHEEL))
        sx, sy = spoke[phase]
        px.update(bresenham(P[w][0], P[w][1], P[w][0] + sx // 2, P[w][1] + sy // 2))
        px.add(P[w])
    px.update(circle_pts(*P[idx["P_HEAD"]], 1))
    return W, H, px


def main():
    src = open(HEADER, encoding="utf-8").read()
    idx, pts, lines, spoke = parse_arrays(src)

    poses = [(0, "рівно"), (-25, "підйом"), (-65, "вілі"),
             (25, "спуск"), (70, "ендо")]

    W, H = 46, 34
    img = Image.new("RGB", (len(poses) * W * SCALE, H * SCALE), (16, 20, 16))
    d = ImageDraw.Draw(img)

    for n, (deg, _) in enumerate(poses):
        _, _, px = render(idx, pts, lines, spoke, deg, 2)
        ox = n * W * SCALE
        for x, y in px:
            if 0 <= x < W and 0 <= y < H:
                d.rectangle([ox + x * SCALE, y * SCALE,
                             ox + (x + 1) * SCALE - 1, (y + 1) * SCALE - 1],
                            fill=(200, 240, 200))
        if n:
            d.line([ox, 0, ox, H * SCALE], fill=(60, 70, 60))

    out = sys.argv[1] if len(sys.argv) > 1 else "sprite.png"
    img.save(out)
    print("записано", out, img.size)


if __name__ == "__main__":
    main()
