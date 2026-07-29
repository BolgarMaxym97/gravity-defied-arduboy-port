#!/usr/bin/env bash
# Десктопна перевірка ПЕРЕД заливкою на залізо.
# Запускати з папки sim/:  ./check.sh
#
# Ця папка називається sim/ навмисно: Arduino-збірка компілює корінь скетчу
# і рекурсивно лише src/, а решту підпапок ігнорує. Тому десктопні файли сюди
# не потраплять у AVR-збірку, і мок Arduboy2.h не перекриє справжню бібліотеку.
set -e

CXX="${CXX:-g++}"
FLAGS="-O2 -std=c++17 -Wall -Wextra"
BEAM="${BEAM:-2500}"

echo "[1/7] Збірка"
$CXX $FLAGS -I.. -I. headless.cpp -o headless
$CXX $FLAGS -I.. -I. solver.cpp   -o solver
$CXX $FLAGS -I.. -I. preview.cpp  -o preview
$CXX $FLAGS -I.. -I. camcheck.cpp -o camcheck
$CXX $FLAGS -I.. -I. uicheck.cpp   -o uicheck
# -I. першим, щоб підхопився мок Arduboy2.h із struct Point.
$CXX -c $FLAGS -I. -I.. -x c++ ../TrialsFX.ino -o /tmp/trialsfx_adapter.o

echo "[2/7] Збірка як в arduino-cli (зі згенерованими прототипами)"
python3 inocheck.py ../TrialsFX.ino

echo "[3/7] Стабільність фізики (евристичний бот, рівні 1 і 10)"
./headless 1  4000 | tail -6
./headless 10 4000 | tail -6

echo
echo "[4/7] Геометрія: кадрування камери і торці траси"
./camcheck

echo
echo "[5/7] Прохідність усіх рівнів (перебір, BEAM=$BEAM)"
fail=0
for i in $(seq 1 10); do
  line=$(./solver "$i" "$BEAM" | sed -n '2p')
  # Ненайдене рішення частіше означає замалий промінь, ніж непрохідний
  # рівень. Спершу подвоюємо промінь і тільки потім вважаємо це провалом.
  case "$line" in
    *"НЕ ЗНАЙДЕНО"*)
      line=$(./solver "$i" "$((BEAM * 3))" | sed -n '2p')
      case "$line" in
        *"НЕ ЗНАЙДЕНО"*) fail=1 ;;
        *) line="$line  (потрібен промінь x3)" ;;
      esac
      ;;
  esac
  printf "  рівень %2d: %s\n" "$i" "$line"
done
[ "$fail" = 0 ] || { echo "Є непрохідні рівні — на залізо не заливати."; exit 1; }

echo
echo "[6/7] Шрифт і рекорди"
./uicheck

echo
echo "[7/7] Спрайт"
./preview | sed -n "/рівно/,/^$/p"

echo
echo "OK — можна компілювати під AVR."
