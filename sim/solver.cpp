// Доказ прохідності рівня перебором.
//
// Ручний тест-бот на евристиках нічого не доводить: якщо він падає, незрозуміло,
// рівень непрохідний чи бот дурний. Фізика детермінована, а стан мотоцикла —
// 44 байти, тож можна просто шукати послідовність натискань променевим пошуком.
// Якщо рішення існує — воно знайдеться, і ми отримаємо еталонний час.
//
//   g++ -O2 -std=c++17 -I.. -I. solver.cpp -o solver && ./solver 2

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <unordered_set>
#include <vector>
#include "gd_core.h"
#include "levels.h"

using namespace gd;

// Промінь і крок — параметри командного рядка: якщо рішення не знайшлось,
// перше, що треба спробувати, це ширший промінь, а не правити рівень.
static int HOLD  = 6;         // кадрів на одне рішення
static int BEAM  = 1200;      // ширина променя
static int STEPS = 700;       // максимум рішень

// Комбінації, які має сенс перебирати. Газ+гальмо одночасно безглузді.
struct Act { bool gas, brake, l, r; };
static const Act ACTS[] = {
  { true,  false, false, false },
  { true,  false, true,  false },
  { true,  false, false, true  },
  { false, false, false, false },
  { false, false, true,  false },
  { false, false, false, true  },
  { false, true,  false, false },
  { false, true,  true,  false },
};
constexpr int NACT = sizeof(ACTS) / sizeof(ACTS[0]);

struct Node {
  Bike b;
  int32_t score;
  int parent;
  uint8_t act;
};

int main(int argc, char **argv) {
  int li = argc > 1 ? atoi(argv[1]) : 1;
  if (li < 1 || li > LEVEL_COUNT) li = 1;
  if (argc > 2) BEAM = atoi(argv[2]);
  if (argc > 3) HOLD = atoi(argv[3]);

  LevelDesc L = levelData((uint8_t)(li - 1));
  Terrain ter{ L.pts, L.count };
  int16_t finishX = L.finishX;
  int16_t startY  = L.startY;

  BikeTuning tun = DEFAULT_TUNING;

  std::vector<std::vector<Node>> gens;
  gens.reserve(STEPS + 1);
  gens.push_back({});
  {
    Node n{};
    bikeInit(n.b, tun, fpFromInt(20), fpFromInt(startY));
    n.score = 0; n.parent = -1; n.act = 0;
    gens[0].push_back(n);
  }

  int bestGen = -1, bestIdx = -1;
  int16_t reached = 0;

  for (int g = 0; g < STEPS && bestGen < 0; g++) {
    std::vector<Node> next;
    next.reserve(gens[g].size() * NACT);

    for (size_t i = 0; i < gens[g].size(); i++) {
      for (int a = 0; a < NACT; a++) {
        Node n;
        n.b = gens[g][i].b;
        n.parent = (int)i;
        n.act = (uint8_t)a;

        Input in{ ACTS[a].gas, ACTS[a].brake, ACTS[a].l, ACTS[a].r };
        for (int f = 0; f < HOLD && !n.b.crashed; f++) bikeStep(n.b, ter, tun, in);
        if (n.b.crashed) continue;

        int16_t x = fpToInt(n.b.rear.x);
        if (x > reached) reached = x;
        // Пріоритет — просування по X; швидкість як слабкий тайбрейк,
        // інакше пошук любить «повзти» і не розганяється.
        n.score = (int32_t)x * 64 + (n.b.rear.x - n.b.rear.px);
        next.push_back(n);

        if (x >= finishX) { bestGen = g + 1; bestIdx = (int)next.size() - 1; break; }
      }
      if (bestGen >= 0) break;
    }

    if (next.empty()) {
      printf("глухий кут на кроці %d (кадр %d)\n", g, g * HOLD);
      break;
    }

    // Дедуплікація: стани, що відрізняються на пів пікселя, — це той самий стан,
    // і без цього промінь забивається клонами однієї траєкторії.
    std::sort(next.begin(), next.end(),
              [](const Node &a, const Node &b) { return a.score > b.score; });

    std::vector<Node> pruned;
    std::unordered_set<uint64_t> seen;
    for (const Node &n : next) {
      uint64_t key = ((uint64_t)(uint16_t)(fpToInt(n.b.rear.x) / 2) << 32) |
                     ((uint64_t)(uint16_t)(fpToInt(n.b.rear.y) / 2) << 16) |
                     (uint16_t)((n.b.front.y - n.b.rear.y) >> 6);
      if (!seen.insert(key).second) continue;
      pruned.push_back(n);
      if ((int)pruned.size() >= BEAM) break;
    }

    gens.push_back(std::move(pruned));
    if (bestGen >= 0) { gens.back() = { next[bestIdx] }; bestIdx = 0; }
  }

  printf("=== рівень %d, фініш %d px ===\n", li, finishX);
  if (bestGen < 0) {
    printf("РІШЕННЯ НЕ ЗНАЙДЕНО. Найдальша точка: %d px (%d%%)\n",
           reached, reached * 100 / finishX);
    printf("Це або непрохідна секція, або замалий промінь (BEAM=%d, HOLD=%d).\n",
           BEAM, HOLD);
    return 1;
  }

  printf("ПРОХОДИМИЙ. Кадрів: %d (%.1f c)\n", bestGen * HOLD, bestGen * HOLD / 60.0);

  // Відновлюємо послідовність кнопок — її можна повторити руками в Ardens.
  std::vector<uint8_t> path;
  int idx = bestIdx;
  for (int g = bestGen; g > 0; g--) {
    path.push_back(gens[g][idx].act);
    idx = gens[g][idx].parent;
  }
  std::reverse(path.begin(), path.end());

  printf("Лінія (по %d кадрів на символ): ", HOLD);
  const char *sym = "G<>.LRB{";
  for (uint8_t a : path) putchar(sym[a]);
  printf("\n  G=газ  <=газ+назад  >=газ+вперед  .=накат  L/R=нахил  B=гальмо\n");
  return 0;
}
