#!/usr/bin/env python3
"""Генератор дрібного шрифту 3x5 для Gravity Defied.

Вбудований шрифт Arduboy2 — 5x7, а setTextSize() тільки множить його на ціле.
Менше за розмір 1 штатно не буває, тому свій шрифт — єдиний спосіб зробити
підписи дрібнішими за заголовок.

Гліфи описані ASCII-артом, а не hex-константами: константи неможливо
перевірити оком, а тут одруківка видно одразу.

  python3 tools/genfont.py > TrialsFX/tinyfont.h
"""
import sys

W, H = 3, 5

GLYPHS = {
    ' ': "...|...|...|...|...",
    '0': ".#.|#.#|#.#|#.#|.#.",
    '1': ".#.|##.|.#.|.#.|###",
    '2': "##.|..#|.#.|#..|###",
    '3': "##.|..#|.#.|..#|##.",
    '4': "#.#|#.#|###|..#|..#",
    '5': "###|#..|##.|..#|##.",
    '6': ".##|#..|##.|#.#|.#.",
    '7': "###|..#|.#.|.#.|.#.",
    '8': ".#.|#.#|.#.|#.#|.#.",
    '9': ".#.|#.#|.##|..#|##.",
    'A': ".#.|#.#|###|#.#|#.#",
    'B': "##.|#.#|##.|#.#|##.",
    'C': ".##|#..|#..|#..|.##",
    'D': "##.|#.#|#.#|#.#|##.",
    'E': "###|#..|##.|#..|###",
    'F': "###|#..|##.|#..|#..",
    'G': ".##|#..|#.#|#.#|.##",
    'H': "#.#|#.#|###|#.#|#.#",
    'I': "###|.#.|.#.|.#.|###",
    'J': "..#|..#|..#|#.#|.#.",
    'K': "#.#|#.#|##.|#.#|#.#",
    'L': "#..|#..|#..|#..|###",
    'M': "#.#|###|###|#.#|#.#",
    'N': "#.#|##.|###|.##|#.#",
    'O': ".#.|#.#|#.#|#.#|.#.",
    'P': "##.|#.#|##.|#..|#..",
    'Q': ".#.|#.#|#.#|##.|.##",
    'R': "##.|#.#|##.|#.#|#.#",
    'S': ".##|#..|.#.|..#|##.",
    'T': "###|.#.|.#.|.#.|.#.",
    'U': "#.#|#.#|#.#|#.#|###",
    'V': "#.#|#.#|#.#|.#.|.#.",
    'W': "#.#|#.#|###|###|#.#",
    'X': "#.#|#.#|.#.|#.#|#.#",
    'Y': "#.#|#.#|.#.|.#.|.#.",
    'Z': "###|..#|.#.|#..|###",
    '.': "...|...|...|...|.#.",
    ',': "...|...|...|.#.|#..",
    ':': "...|.#.|...|.#.|...",
    '-': "...|...|###|...|...",
    '+': "...|.#.|###|.#.|...",
    '=': "...|###|...|###|...",
    '/': "..#|..#|.#.|#..|#..",
    '<': "..#|.#.|#..|.#.|..#",
    '>': "#..|.#.|..#|.#.|#..",
    '(': "..#|.#.|.#.|.#.|..#",
    ')': "#..|.#.|.#.|.#.|#..",
    '!': ".#.|.#.|.#.|...|.#.",
    '?': "##.|..#|.#.|...|.#.",
    '*': "#.#|.#.|#.#|...|...",
    '%': "#.#|..#|.#.|#..|#.#",
}

# Порядок у таблиці — суцільний діапазон ASCII, щоб індекс рахувався
# відніманням, без пошуку.
FIRST, LAST = ord(' '), ord('_')


def columns(art):
    rows = art.split("|")
    assert len(rows) == H and all(len(r) == W for r in rows), art
    out = []
    for c in range(W):
        b = 0
        for r in range(H):
            if rows[r][c] == '#':
                b |= 1 << r
        out.append(b)
    return out


def main():
    blank = GLYPHS[' ']
    table = []
    for code in range(FIRST, LAST + 1):
        ch = chr(code)
        table.append((ch, columns(GLYPHS.get(ch, blank))))

    missing = sorted(ch for ch in GLYPHS if not (FIRST <= ord(ch) <= LAST))
    if missing:
        print("гліфи поза діапазоном:", missing, file=sys.stderr)

    print("#pragma once")
    print("#include <stdint.h>")
    print()
    print("// Згенеровано tools/genfont.py — НЕ редагувати руками.")
    print("// Шрифт 3x5. Байт = колонка, біт 0 — верхній піксель.")
    print()
    print("#if defined(ARDUINO) || defined(__AVR__)")
    print("  #include <avr/pgmspace.h>")
    print("#else")
    print("  #ifndef PROGMEM")
    print("    #define PROGMEM")
    print("  #endif")
    print("  #ifndef pgm_read_byte")
    print("    #define pgm_read_byte(p) (*(const uint8_t *)(p))")
    print("  #endif")
    print("#endif")
    print()
    print(f"constexpr uint8_t TINY_W     = {W};")
    print(f"constexpr uint8_t TINY_H     = {H};")
    print(f"constexpr uint8_t TINY_ADV   = {W + 1};   // крок між символами")
    print(f"constexpr char    TINY_FIRST = '{chr(FIRST)}';")
    print(f"constexpr char    TINY_LAST  = '{chr(LAST)}';")
    print()
    print(f"// {len(table)} гліфів x {W} байт = {len(table) * W} байт PROGMEM")
    print(f"const uint8_t PROGMEM TINY_FONT[{len(table) * W}] = {{")
    for ch, cols in table:
        label = ch if ch not in "\\'" else "\\" + ch
        print("  " + " ".join(f"0x{b:02X}," for b in cols) + f"   // '{label}'")
    print("};")


if __name__ == "__main__":
    main()
