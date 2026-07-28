// Десктопний прогін фізики. Компілюється тим самим кодом, що й на AVR.
// Мета: зловити нестабільність (розліт координат, провалювання крізь землю,
// вибух зв'язку) до того, як воно потрапить на пристрій.
//
//   g++ -O2 -std=c++17 -I.. -I. headless.cpp -o headless
//   ./headless [level 1|2] [frames]

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "gd_core.h"
#include "level1.h"
#include "level2.h"

using namespace gd;

struct LevelDesc {
  const char *name;
  const int16_t *pts;
  uint16_t count;
  int16_t finishX, startY;
};

static const LevelDesc LEVELS[] = {
  { "1 (easy)", level1_pts, LEVEL1_COUNT, LEVEL1_FINISH_X, LEVEL1_START_Y },
  { "2 (hard)", level2_pts, LEVEL2_COUNT, LEVEL2_FINISH_X, LEVEL2_START_Y },
};

// Тест-водій. Не «ідеальний гравець» — його задача лише прогнати фізику
// крізь усі секції рівня. Політика: газ на землі, у повітрі вирівнювати
// раму під нахил ландшафту, що попереду.
static Input drive(const Bike &b, const Terrain &ter) {
  Input in{};
  // Газ лише коли є зчеплення заднього колеса: у повітрі він нічого не дає,
  // а на переднє колесо тяги немає взагалі.
  bool grounded = b.rearGrounded;
  in.gas = grounded;

  fp ux, uy;
  if (!bikeAxes(b, ux, uy)) return in;

  if (grounded) {
    // На крутому підйомі трохи задирати ніс, на спуску — притискати.
    if (uy < -60) in.leanLeft = true;
    else if (uy > 60) in.leanRight = true;
    return in;
  }

  // У повітрі: цілимось рамою в нахил ділянки перед носом.
  int16_t ahead = fpToInt(b.front.x) + 24;
  uint16_t s = ter.segAt(ahead, b.segHintF);
  int16_t dx = ter.x(s + 1) - ter.x(s);
  int16_t dy = ter.y(s + 1) - ter.y(s);
  fp target = dx ? fpDiv(fpFromInt(dy), fpFromInt(dx)) : 0;
  fp current = ux ? fpDiv(uy, ux) : 0;

  if (current > target + 16) in.leanLeft = true;    // ніс нижче цілі -> задерти
  else if (current < target - 16) in.leanRight = true;
  return in;
}

int main(int argc, char **argv) {
  int li = argc > 1 ? atoi(argv[1]) - 1 : 0;
  if (li < 0 || li > 1) li = 0;
  const LevelDesc &L = LEVELS[li];

  Terrain ter{ L.pts, L.count };
  BikeTuning tun = DEFAULT_TUNING;

  Bike b;
  bikeInit(b, tun, fpFromInt(20), fpFromInt(L.startY));

  const int FRAMES = argc > 2 ? atoi(argv[2]) : 5000;

  printf("=== рівень %s: %u точок, фініш %d px ===\n", L.name, L.count, L.finishX);

  int crashFrame = -1, finishFrame = -1;
  fp maxAbs = 0;
  int airFrames = 0, f = 0;

  for (; f < FRAMES; f++) {
    bikeStep(b, ter, tun, drive(b, ter));

    if (!b.rearGrounded) airFrames++;

    fp a = b.rear.x > 0 ? b.rear.x : -b.rear.x;
    if (a > maxAbs) maxAbs = a;
    fp ay = b.rear.y > 0 ? b.rear.y : -b.rear.y;
    if (ay > maxAbs) maxAbs = ay;

    if (b.crashed) { crashFrame = f; break; }
    if (fpToInt(b.rear.x) >= L.finishX) { finishFrame = f; break; }

    if (f % 300 == 0) {
      printf("f=%4d  x=%5d  y=%4d  ground=%d\n",
             f, fpToInt(b.rear.x), fpToInt(b.rear.y), b.rearGrounded);
    }

    // Детектор вибуху: координати вилетіли далеко за межі світу.
    if (fpToInt(b.rear.y) < -2000 || fpToInt(b.rear.y) > 4000 ||
        fpToInt(b.rear.x) < -2000 || fpToInt(b.rear.x) > 20000) {
      printf("!!! ФІЗИКА РОЗЛЕТІЛАСЬ на кадрі %d: x=%d y=%d\n",
             f, fpToInt(b.rear.x), fpToInt(b.rear.y));
      return 1;
    }

    // Провалювання крізь землю: колесо глибоко під поверхнею.
    uint16_t s = ter.segAt(fpToInt(b.rear.x), b.segHintR);
    int16_t sx0 = ter.x(s), sx1 = ter.x(s + 1);
    int16_t sy0 = ter.y(s), sy1 = ter.y(s + 1);
    int16_t surf = sx1 != sx0
        ? sy0 + (int16_t)((int32_t)(sy1 - sy0) * (fpToInt(b.rear.x) - sx0) / (sx1 - sx0))
        : sy0;
    if (fpToInt(b.rear.y) > surf + 12) {
      printf("!!! КОЛЕСО ПІД ЗЕМЛЕЮ на кадрі %d: y=%d, поверхня=%d\n",
             f, fpToInt(b.rear.y), surf);
      return 1;
    }
  }

  printf("\n--- підсумок ---\n");
  printf("дистанція:        %d / %d px (%d%%)\n",
         b.distance, L.finishX, b.distance * 100 / L.finishX);
  printf("кадрів у повітрі: %d / %d\n", airFrames, f ? f : 1);
  printf("краш:             %s", crashFrame >= 0 ? "так" : "ні");
  if (crashFrame >= 0) printf(" (кадр %d, %.1f c, x=%d)", crashFrame,
                              crashFrame / 60.0, fpToInt(b.rear.x));
  printf("\n");
  if (finishFrame >= 0) printf("ФІНІШ на кадрі %d (%.1f c)\n", finishFrame, finishFrame / 60.0);
  printf("макс |коорд|:     %d (Q8) — має лишатись у розумних межах\n", maxAbs);

  printf("\nsizeof(Bike)    = %zu байт\n", sizeof(Bike));
  printf("sizeof(Terrain) = %zu байт\n", sizeof(Terrain));
  printf("sizeof(Tuning)  = %zu байт (можна винести в PROGMEM)\n", sizeof(BikeTuning));
  return 0;
}
