#!/usr/bin/env python3
"""Генератор екрана завантаження (title.png), 128x64, 1 біт.

Це НЕ те саме, що cover.png. Обкладинка стоїть у списку картриджа поруч із
десятками інших і мусить читатись як мініатюра. Сплеш показується на весь
екран одну-дві секунди, тож логотип тут крупніший, а деталей менше.

Малюється з тих самих даних, що й гра: мотоцикл із bike_sprite.h, літери з
tinyfont.h. Рука не бере участі, тому асет не розійдеться з грою.

  python3 tools/gentitle.py title.png [preview.png]
"""
import math
import sys

from PIL import Image

sys.path.insert(0, "tools")
from gencover import (Canvas, W, H, SPRITE_HEADER, load_font, draw_text,  # noqa: E402
                      text_width)
from sprite_preview import parse_arrays, project, circle_pts  # noqa: E402


def build():
    cv = Canvas()
    font = load_font()
    idx, pts, lines, spoke = parse_arrays(open(SPRITE_HEADER, encoding="utf-8").read())

    # --- логотип у два рядки, масштаб 3 ---
    # Одним рядком "GRAVITY DEFIED" на масштабі 3 було б 165 px — не влазить.
    for text, y in (("GRAVITY", 4), ("DEFIED", 21)):
        draw_text(cv, font, (W - text_width(text, 3)) // 2, y, text, scale=3)

    # --- земля ---
    ground = 60
    cv.line(0, ground, W - 1, ground)
    for x in range(0, W, 4):
        cv.vline(x, ground + 1, H - 2)

    # --- мотоцикл у вілі ---
    # Кут крутий: на сплеші силует має читатись як «мотоцикл дибки»,
    # а не як бічна проєкція, яку й так видно в грі.
    deg = -45
    a = math.radians(deg)
    ux = round(math.cos(a) * 256)
    uy = round(math.sin(a) * 256)
    rx, ry = 100, ground - 4

    P = project(pts, ux, uy, rx, ry)
    for i, j in lines:
        cv.line(*P[i], *P[j])
    for w in (idx["P_RAXLE"], idx["P_FAXLE"]):
        for p in circle_pts(*P[w], 4):
            cv.set(*p)
        sx, sy = spoke[2]
        cv.line(P[w][0], P[w][1], P[w][0] + sx // 2, P[w][1] + sy // 2)
        cv.set(*P[w])
    for p in circle_pts(*P[idx["P_HEAD"]], 1):
        cv.set(*p)

    # --- підпис ---
    # Один короткий рядок ліворуч, у порожнечі під логотипом: більше сюди
    # не влазить, а сплеш і не мусить нічого пояснювати.
    draw_text(cv, font, 8, 50, "10 LEVELS", scale=1)
    return cv


def main():
    cv = build()
    img = Image.new("1", (W, H), 0)
    img.putdata([cv.px[y][x] for y in range(H) for x in range(W)])
    out = sys.argv[1] if len(sys.argv) > 1 else "title.png"
    img.save(out)
    print(f"записано {out} {img.size} {img.mode}")

    if len(sys.argv) > 2:
        img.convert("L").resize((W * 6, H * 6), Image.NEAREST).save(sys.argv[2])
        print("прев'ю:", sys.argv[2])


if __name__ == "__main__":
    main()
