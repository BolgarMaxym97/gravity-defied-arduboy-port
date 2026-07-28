#!/usr/bin/env python3
"""Генератор 10 рівнів для Gravity Defied (Arduboy).

Рівень — полілінія (x, y), x строго зростає. Правила нижче перевіряються
жорстко, бо їх порушення ламає рушій, а не просто псує геймплей:

  * x строго зростає            — segAt() шукає сегмент лінійно по x;
  * dx >= MIN_DX                — resolveWheel() дивиться лише на сусідні
  * |dy/dx| <= MAX_SLOPE          сегменти; майже вертикальна стінка випадає
                                  з вікна пошуку і колесо провалюється;
  * довжина сегмента > 2*R      — інакше колесо торкається трьох сегментів
                                  одночасно і виштовхування б'ється саме з собою.

Окремо — high_center(): попередження про місця, де ландшафт піднімається над
хордою між колесами. Там мотоцикл сідає на гребінь і при повному газі повзе
0.07 px/кадр. Це правило про ПАРУ точок на відстані бази, тому жодна з
поточкових перевірок його не бачить.

  python3 tools/genlevel.py > Levels/levels.h
"""
import math
import sys

MAX_SLOPE = 1.9          # ~62°, безпечно для вікна пошуку hint±2
MIN_DX = 4
WHEEL_R = 4
WHEELBASE = 15
MAX_HIGH_CENTER = 3.0

LEVELS = 10


# --------------------------------------------------------------------------
# DSL побудови траси
# --------------------------------------------------------------------------
class Track:
    def __init__(self, x=0, y=120):
        self.pts = [(float(x), float(y))]

    @property
    def y(self):
        return self.pts[-1][1]

    def _add(self, x, y):
        self.pts.append((x, y))

    def flat(self, length, step=12):
        x0, y0 = self.pts[-1]
        n = max(1, round(length / step))
        for i in range(1, n + 1):
            self._add(x0 + length * i / n, y0)

    def ramp(self, dx, dy, step=11, ease=False):
        """Схил. ease=True — плавний вхід/вихід (S-крива)."""
        x0, y0 = self.pts[-1]
        n = max(1, round(dx / step))
        for i in range(1, n + 1):
            t = i / n
            k = (1 - math.cos(math.pi * t)) / 2 if ease else t
            self._add(x0 + dx * t, y0 + dy * k)

    def bumps(self, count, width, height, step=11):
        """Серія горбів. width — період ОДНОГО горба; має бути помітно
        більшим за базу, інакше мотоцикл сідає на гребінь між колесами."""
        x0, y0 = self.pts[-1]
        total = count * width
        n = max(1, round(total / step))
        for i in range(1, n + 1):
            t = i / n
            self._add(x0 + total * t,
                      y0 - height * (1 - math.cos(2 * math.pi * count * t)) / 2)

    def gap(self, down, width_down, floor, up, width_up):
        """Яр: крутий спуск, дно, крутий підйом."""
        self.ramp(width_down, down, step=9)
        self.flat(floor, step=max(10, floor))
        self.ramp(width_up, -up, step=9)

    def stairs(self, count, rise, run, tread):
        """Сходинки. tread мусить бути ширшим за базу, інакше мотоцикл
        зависає на ребрі сходинки обома колесами по різні боки."""
        for _ in range(count):
            self.ramp(run, -rise, step=run)
            self.flat(tread, step=max(12, tread / 2))

    def jump(self, run_up, rise, lip, drop, landing):
        """Трамплін: розгін угору, губа, зрив, посадкова полиця."""
        self.ramp(run_up, -rise, ease=True)
        self.ramp(lip, -lip * 0.4)
        self.ramp(drop, rise * 0.9 + drop * 0.3)
        self.ramp(landing, landing * 0.25, ease=True)


# --------------------------------------------------------------------------
# Прогресія складності
# --------------------------------------------------------------------------
def build(level):
    """level: 1..10. Секції вмикаються поступово, а вже ввімкнені ростуть."""
    t = (level - 1) / (LEVELS - 1)          # 0.0 .. 1.0
    k = Track()

    def lerp(a, b):
        return a + (b - a) * t

    k.flat(90)                                        # розгін є завжди

    # 1. Трясучка. Період тримаємо >= 40, інакше сідання на гребінь.
    k.bumps(2 + level // 3, lerp(52, 42), lerp(3, 6))
    k.flat(lerp(50, 30))

    # 1b. Пологі пагорби — є на всіх рівнях, щоб навіть 1-й не був голим.
    k.ramp(64, -lerp(10, 18), ease=True)
    k.ramp(64, lerp(10, 18), ease=True)

    # 2. Трамплін — з 2-го рівня.
    if level >= 2:
        k.jump(run_up=lerp(70, 50), rise=lerp(18, 40),
               lip=12, drop=lerp(22, 40), landing=lerp(60, 40))
        k.flat(lerp(46, 26))

    # 3. Яр — з 3-го.
    if level >= 3:
        k.gap(down=lerp(24, 44), width_down=lerp(34, 28), floor=lerp(24, 12),
              up=lerp(26, 46), width_up=lerp(36, 30))
        k.flat(lerp(40, 24))

    # 4. Сходинки — з 4-го.
    if level >= 4:
        k.stairs(count=1 + level // 3, rise=lerp(8, 13), run=11, tread=lerp(34, 26))
        k.flat(lerp(36, 22))

    # 5. Довгий підйом — з 5-го.
    if level >= 5:
        k.ramp(lerp(120, 90), -lerp(34, 62), ease=True)
        k.bumps(2, 44, lerp(3, 6))

    # 6. Зрив у прірву — з 6-го.
    if level >= 6:
        k.ramp(24, -lerp(8, 16))
        k.ramp(lerp(52, 46), lerp(40, 72))
        k.ramp(lerp(48, 36), lerp(8, 14), ease=True)

    # 7. Ритм-секція — з 7-го.
    if level >= 7:
        k.bumps(3, lerp(48, 42), lerp(5, 8))
        k.flat(26)

    # 8. Фінальна стінка — з 8-го.
    if level >= 8:
        k.ramp(lerp(30, 22), -lerp(18, 28))

    # Вирівнювання: за багато спусків трек «тоне», повертаємо у робочу смугу.
    if k.y > 150:
        k.ramp(70, -(k.y - 130), ease=True)
    elif k.y < 60:
        k.ramp(70, 100 - k.y, ease=True)

    k.flat(70)
    return quantize(k.pts)


# --------------------------------------------------------------------------
# Квантування й перевірки
# --------------------------------------------------------------------------
def quantize(pts):
    out = []
    for x, y in pts:
        xi, yi = int(round(x)), int(round(y))
        if out and xi <= out[-1][0]:
            xi = out[-1][0] + MIN_DX
        out.append((xi, yi))
    return out


def high_center(pts):
    """Місця, де мотоцикл сідає на гребінь між колесами."""
    bad = []
    for i in range(len(pts)):
        x0, y0 = pts[i]
        j = i + 1
        while j < len(pts) and pts[j][0] - x0 < WHEELBASE:
            j += 1
        if j >= len(pts):
            break
        x1, y1 = pts[j]
        worst = 0.0
        for m in range(i + 1, j):
            xm, ym = pts[m]
            chord = y0 + (y1 - y0) * (xm - x0) / (x1 - x0)
            worst = max(worst, chord - ym)          # y вниз: менший y = вище
        if worst > MAX_HIGH_CENTER:
            bad.append((x0, round(worst, 1)))
    return bad


def validate(pts, label):
    errs = []
    for i in range(len(pts) - 1):
        (x0, y0), (x1, y1) = pts[i], pts[i + 1]
        dx, dy = x1 - x0, y1 - y0
        if dx < MIN_DX:
            errs.append(f"{label} сегмент {i}: dx={dx} < {MIN_DX}")
        slope = abs(dy) / dx if dx else 99
        if slope > MAX_SLOPE:
            errs.append(f"{label} сегмент {i}: нахил {slope:.2f} > {MAX_SLOPE}")
        if math.hypot(dx, dy) <= 2 * WHEEL_R:
            errs.append(f"{label} сегмент {i}: довжина {math.hypot(dx, dy):.1f} <= 2*R")
    return errs


# --------------------------------------------------------------------------
# Вивід
# --------------------------------------------------------------------------
def emit(levels):
    out = []
    w = out.append
    w("#pragma once")
    w("#include <stdint.h>")
    w("")
    w("// Згенеровано tools/genlevel.py — НЕ редагувати руками.")
    w("")
    w("#if defined(ARDUINO) || defined(__AVR__)")
    w("  #include <avr/pgmspace.h>")
    w("#else")
    w("  #ifndef PROGMEM")
    w("    #define PROGMEM")
    w("  #endif")
    w("#endif")
    w("")
    w(f"constexpr uint8_t LEVEL_COUNT = {len(levels)};")
    w("")

    total = 0
    for n, pts in enumerate(levels):
        total += len(pts) * 4
        w(f"// рівень {n + 1}: {len(pts)} точок, {len(pts) * 4} байт")
        w(f"const int16_t PROGMEM lvl{n}_pts[] = {{")
        for i in range(0, len(pts), 6):
            w("  " + " ".join(f"{x},{y}," for x, y in pts[i:i + 6]))
        w("};")
    w("")
    w(f"// Разом даних рівнів: {total} байт PROGMEM.")
    w("")

    # switch, а не таблиця вказівників: таблиця вказівників у PROGMEM вимагає
    # pgm_read_word, який на десктопі повертає обрізаний 64-бітний вказівник.
    w("struct LevelDesc {")
    w("  const int16_t *pts;")
    w("  uint16_t count;")
    w("  int16_t  finishX, startY, minY, maxY;")
    w("};")
    w("")
    w("inline LevelDesc levelData(uint8_t i) {")
    w("  switch (i) {")
    for n, pts in enumerate(levels):
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        w(f"    case {n}: return {{ lvl{n}_pts, {len(pts)}, "
          f"{xs[-1] - 20}, {ys[0] - 14}, {min(ys)}, {max(ys)} }};")
    w("  }")
    w("  return { lvl0_pts, %d, %d, %d, %d, %d };"
      % (len(levels[0]), levels[0][-1][0] - 20, levels[0][0][1] - 14,
         min(p[1] for p in levels[0]), max(p[1] for p in levels[0])))
    w("}")
    return "\n".join(out)


def main():
    levels, errs = [], []
    for n in range(1, LEVELS + 1):
        pts = build(n)
        errs += validate(pts, f"[рівень {n}]")
        for x, h in high_center(pts):
            print(f"УВАГА [рівень {n}] x={x}: гребінь між колесами +{h} px",
                  file=sys.stderr)
        levels.append(pts)
        print(f"[рівень {n}] {len(pts):3} точок, довжина {pts[-1][0]:4} px, "
              f"y {min(p[1] for p in pts)}..{max(p[1] for p in pts)}", file=sys.stderr)

    if errs:
        for e in errs[:25]:
            print("ПОМИЛКА:", e, file=sys.stderr)
        sys.exit(1)

    print(emit(levels))


if __name__ == "__main__":
    main()
