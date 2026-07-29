#!/usr/bin/env python3
"""Відтворює крок збирача Arduino, якого немає в десктопній збірці.

arduino-cli не компілює .ino напряму: він сканує файл, генерує прототипи всіх
функцій верхнього рівня і вставляє їх ОДРАЗУ ПІСЛЯ останнього #include —
тобто перед будь-яким кодом самого скетчу. Наслідки:

  * тип, оголошений у .ino (enum, struct, using), не видно прототипу, який
    його використовує -> "'St' was not declared in this scope";
  * аргумент за замовчуванням опиняється і в прототипі, і у визначенні ->
    "default argument given for parameter".

g++ компілює .ino як звичайний C++ і жодного з цих випадків не бачить, тому
перевірка адаптера давала зелене світло на код, який не збирається під AVR.

  python3 inocheck.py ../TrialsFX.ino
"""
import re
import subprocess
import sys
import tempfile
import os

# Визначення функції верхнього рівня: тип і ім'я з першої колонки.
FUNC = re.compile(
    r"^([A-Za-z_][\w:<>*&\s]*?[\w>*&])\s+([A-Za-z_]\w*)\s*\(([^;{]*)\)\s*\{",
    re.M)

KEYWORDS = {"if", "for", "while", "switch", "do", "else", "return", "catch"}


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def find_functions(src):
    out = []
    for m in FUNC.finditer(src):
        ret, name, args = m.group(1).strip(), m.group(2), m.group(3)
        if name in KEYWORDS or ret.split()[-1] in KEYWORDS:
            continue
        out.append((ret, name, " ".join(args.split())))
    return out


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "../TrialsFX.ino"
    raw = open(path, encoding="utf-8").read()
    clean = strip_comments(raw)

    funcs = find_functions(clean)
    if not funcs:
        print("не знайдено жодної функції — перевірка не має сенсу", file=sys.stderr)
        return 1

    # 1. Аргументи за замовчуванням у .ino — окрема пастка.
    bad = [f"{n}({a})" for _, n, a in funcs if "=" in a]
    if bad:
        print("ПОМИЛКА: аргументи за замовчуванням у .ino "
              "(конфліктують зі згенерованим прототипом):")
        for b in bad:
            print("   ", b)
        return 1

    # 2. Складаємо файл так, як це робить збирач.
    lines = raw.split("\n")
    last_include = max(i for i, l in enumerate(lines) if l.startswith("#include"))
    protos = [f"{r} {n}({a});" for r, n, a in funcs]

    patched = (lines[:last_include + 1]
               + ["", "// --- прототипи, згенеровані як це робить arduino-cli ---"]
               + protos + [""]
               + lines[last_include + 1:])

    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "patched.cpp")
        with open(out, "w", encoding="utf-8") as f:
            f.write("\n".join(patched))

        cmd = ["g++", "-c", "-O1", "-std=c++17", "-Wall",
               "-I.", "-I..", out, "-o", os.path.join(tmp, "a.o")]
        r = subprocess.run(cmd, capture_output=True, text=True)

    print(f"функцій верхнього рівня: {len(funcs)}, прототипів вставлено після "
          f"рядка {last_include + 1}")
    if r.returncode:
        print("ЗБІРКА З ПРОТОТИПАМИ ПРОВАЛЕНА:\n")
        print(r.stderr[:4000])
        return 1
    print("OK — скетч збереться під arduino-cli.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
