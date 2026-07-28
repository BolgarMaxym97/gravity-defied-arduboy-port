// Малює спрайт мотоцикла в ASCII під різними кутами нахилу рами.
// Сенс: не заливати на залізо арт, якого жодного разу не бачив. Використовує
// ті самі таблиці з bike_sprite.h, що й прошивка, тож розходження неможливе.
//
//   g++ -O2 -std=c++17 -I.. -I. preview.cpp -o preview && ./preview

#include <cstdio>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include "bike_sprite.h"

using namespace gd;

constexpr int W = 34, H = 22;
static char buf[H][W];

static void plot(int x, int y) {
  if (x >= 0 && x < W && y >= 0 && y < H) buf[y][x] = '#';
}

// Той самий Bresenham, що й в Arduboy2 — форма ліній збігається.
static void line(int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    plot(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

static void circle(int cx, int cy, int r) {
  int x = r, y = 0, e = 1 - r;
  while (x >= y) {
    plot(cx + x, cy + y); plot(cx + y, cy + x);
    plot(cx - y, cy + x); plot(cx - x, cy + y);
    plot(cx - x, cy - y); plot(cx - y, cy - x);
    plot(cx + y, cy - x); plot(cx + x, cy - y);
    y++;
    if (e < 0) e += 2 * y + 1;
    else { x--; e += 2 * (y - x) + 1; }
  }
}

static void render(double deg, int phase) {
  memset(buf, ' ', sizeof(buf));
  double a = deg * M_PI / 180.0;
  int16_t ux = (int16_t)lround(cos(a) * 256);
  int16_t uy = (int16_t)lround(sin(a) * 256);

  int16_t px[P_COUNT], py[P_COUNT];
  bikeProject(9, 15, ux, uy, px, py);

  for (uint8_t i = 0; i < BIKE_LINE_COUNT; i++) {
    uint8_t p = BIKE_LINES[i * 2], q = BIKE_LINES[i * 2 + 1];
    line(px[p], py[p], px[q], py[q]);
  }
  for (uint8_t w : { P_RAXLE, P_FAXLE }) {
    circle(px[w], py[w], 3);
    int8_t sx = SPOKE[phase * 2], sy = SPOKE[phase * 2 + 1];
    line(px[w], py[w], px[w] + sx, py[w] + sy);
    line(px[w], py[w], px[w] - sx, py[w] - sy);
  }
  circle(px[P_HEAD], py[P_HEAD], 1);
}

int main() {
  const struct { double deg; const char *name; } POSES[] = {
    {   0, "рівно" },
    { -30, "підйом 30" },
    { -70, "вілі 70" },
    {  25, "спуск 25" },
    {  75, "ендо 75" },
  };

  for (auto &p : POSES) {
    render(p.deg, 2);
    printf("--- %s (%.0f°) ---\n", p.name, p.deg);
    for (int y = 0; y < H; y++) {
      int last = W - 1;
      while (last > 0 && buf[y][last] == ' ') last--;
      printf("%.*s\n", last + 1, buf[y]);
    }
    printf("\n");
  }
  return 0;
}
