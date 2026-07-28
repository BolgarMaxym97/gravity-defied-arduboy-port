// Перевірка вертикальної камери.
//
// Тут була помилка, яку не ловив жоден інший тест: на рівнях, що цілком
// влазять у екран, кламп вироджувався (hi < lo) і камера притискала трасу
// до верхнього краю — мотоцикл їхав під самим HUD. Це геометрія, а не
// фізика, тому перевіряється окремо.
//
//   g++ -O2 -std=c++17 -I.. -I. camcheck.cpp -o camcheck && ./camcheck

#include <cstdio>
#include "gd_core.h"
#include "levels.h"

using namespace gd;

constexpr int16_t HUD_H = 16;
constexpr int16_t CAM_OFFSET_Y = (HUD_H + 63) / 2;

// Копія updateCamera() зі скетчу (без згладжування).
static int16_t camTarget(const LevelDesc &L, int16_t bikeY) {
  int16_t lo = L.minY - HUD_H;
  int16_t hi = L.maxY - 63;
  if (hi <= lo) return ((L.minY + L.maxY) >> 1) - CAM_OFFSET_Y;
  int16_t ty = bikeY - CAM_OFFSET_Y;
  if (ty < lo) ty = lo;
  if (ty > hi) ty = hi;
  return ty;
}

// Торці траси: заднім ходом можна було виїхати за x(0) у порожнечу.
static int checkWalls() {
  int bad = 0;
  printf("\nрів  траса x        мотоцикл x      прогрес\n");
  for (uint8_t i = 0; i < LEVEL_COUNT; i++) {
    LevelDesc L = levelData(i);
    Terrain ter{ L.pts, L.count };
    BikeTuning tun = DEFAULT_TUNING;
    Bike b;
    bikeInit(b, tun, fpFromInt(20), fpFromInt(L.startY));

    int16_t lo = 32767, hi = -32768;
    for (int f = 0; f < 600; f++) {            // суцільний задній хід
      Input in{}; in.brake = true;
      bikeStep(b, ter, tun, in);
      int16_t x = fpToInt(b.rear.x);
      if (x < lo) lo = x;
      if (x > hi) hi = x;
    }
    for (int f = 0; f < 3000; f++) {           // потім повний газ
      Input in{}; in.gas = true;
      bikeStep(b, ter, tun, in);
      int16_t x = fpToInt(b.front.x);
      if (x > hi) hi = x;
    }

    bool ok = lo >= ter.x(0) && hi <= ter.x(ter.count - 1);
    if (!ok) bad++;
    printf("%3d  %4d..%-6d  %4d..%-6d  %5u  %s\n", i + 1,
           ter.x(0), ter.x(ter.count - 1), lo, hi, b.distance,
           ok ? "OK" : "<-- ВИЇХАВ ЗА КРАЙ");
  }
  return bad;
}

int main() {
  int bad = 0;
  printf("рів  діапазон y   мот.на старті  мот.мін  мот.макс  скрол\n");

  for (uint8_t i = 0; i < LEVEL_COUNT; i++) {
    LevelDesc L = levelData(i);
    Terrain ter{ L.pts, L.count };

    int16_t startScreen = L.startY - camTarget(L, L.startY);

    // Проходимо всі точки траси: мотоцикл їде приблизно по поверхні,
    // тож екранна координата поверхні = межа того, що побачить гравець.
    int16_t lo = 999, hi = -999;
    for (uint16_t k = 0; k < ter.count; k++) {
      int16_t wy = ter.y(k);
      int16_t sy = wy - camTarget(L, wy);
      if (sy < lo) lo = sy;
      if (sy > hi) hi = sy;
    }

    bool scrolls = (L.maxY - 63) > (L.minY - HUD_H);
    bool ok = lo >= HUD_H && hi <= 63;
    if (!ok) bad++;

    printf("%3d  %4d..%-4d   %6d        %6d   %6d   %-3s  %s\n",
           i + 1, L.minY, L.maxY, startScreen, lo, hi,
           scrolls ? "так" : "ні", ok ? "OK" : "<-- ПОЗА СМУГОЮ");
  }

  if (bad) {
    printf("\n%d рівнів виходять за смугу %d..63\n", bad, HUD_H);
    return 1;
  }
  printf("\nУсі рівні тримаються у смузі %d..63 (під HUD не заїжджають).\n", HUD_H);

  if (checkWalls()) {
    printf("\nМотоцикл виїжджає за торець траси.\n");
    return 1;
  }
  printf("\nТорці тримають: за межі траси не виїхати.\n");
  return 0;
}
