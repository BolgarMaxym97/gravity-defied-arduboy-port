// Перевірка UI-частин, які не має сенсу ганяти на залізі методом «залив і
// подивився»: дрібний шрифт і зберігання рекордів.
//
//   g++ -O2 -std=c++17 -I.. -I. uicheck.cpp -o uicheck && ./uicheck

#include <cstdio>
#include <cstring>
#include <Arduboy2.h>   // мок: звідси EEPROM_STORAGE_SPACE_START
#include "levels.h"
#include "tinyfont.h"
#include "records.h"

// --------------------------- Шрифт -----------------------------------------

static uint8_t tinyWidth(const char *s) {
  uint8_t n = 0;
  while (*s++) n++;
  return n ? n * TINY_ADV - 1 : 0;
}

// Рендер рядка в ASCII. Сенс: гліфи описані ASCII-артом у genfont.py, і тут
// видно, чи вони пережили дорогу до C-таблиці.
static void tinyRender(const char *s) {
  char rows[TINY_H][256];
  memset(rows, ' ', sizeof(rows));
  int x = 0;
  for (const char *p = s; *p; p++) {
    char c = *p;
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < TINY_FIRST || c > TINY_LAST) c = ' ';
    const uint8_t *g = &TINY_FONT[(uint8_t)(c - TINY_FIRST) * TINY_W];
    for (uint8_t cx = 0; cx < TINY_W; cx++) {
      for (uint8_t ry = 0; ry < TINY_H; ry++) {
        rows[ry][x + cx] = (g[cx] & (1 << ry)) ? '#' : '.';
      }
    }
    x += TINY_ADV;
  }
  for (uint8_t r = 0; r < TINY_H; r++) printf("    %.*s\n", x, rows[r]);
}

static int checkFont() {
  const char *lines[] = {
    "LEVEL 10  BRUTAL",
    "BEST 12.3",
    "A=GAS  B=BRAKE/REVERSE",
    "<> = LEAN    v = MENU",
    "TIME 12.3   BEST 11.8",
  };
  int bad = 0;
  for (const char *s : lines) {
    uint8_t w = tinyWidth(s);
    bool fits = w <= 128;
    if (!fits) bad++;
    printf("  %-24s ширина %3d px, центр x=%3d  %s\n",
           s, w, (128 - w) / 2, fits ? "OK" : "<-- ШИРШЕ ЕКРАНА");
  }
  printf("\n");
  tinyRender("BEST 12.3");
  return bad;
}

// --------------------------- Рекорди ---------------------------------------

static int checkRecords() {
  int bad = 0;
  Records<LEVEL_COUNT> r;

  // 1. Чиста EEPROM: усе має стати нулями, а не сміттям.
  memset(EEPROM.data, 0xAB, sizeof(EEPROM.data));
  r.load(EEPROM_STORAGE_SPACE_START);
  for (uint8_t i = 0; i < LEVEL_COUNT; i++) {
    if (r[i] != 0) { printf("  чиста EEPROM: рівень %d = %u, а не 0\n", i + 1, r[i]); bad++; }
  }
  printf("  чиста EEPROM -> усі рекорди порожні: %s\n", bad ? "ПОМИЛКА" : "OK");

  // 2. Запис і покращення.
  bool first  = r.submit(3, 500);
  bool worse  = r.submit(3, 600);
  bool better = r.submit(3, 450);
  printf("  перший результат=%d, гірший=%d, кращий=%d  %s\n",
         first, worse, better,
         (first && !worse && better) ? "OK" : "<-- ПОМИЛКА");
  if (!(first && !worse && better)) bad++;
  if (r[3] != 450) { printf("  збережено %u замість 450\n", r[3]); bad++; }

  // 3. Перезавантаження: значення мають пережити «вимкнення».
  Records<LEVEL_COUNT> r2;
  r2.load(EEPROM_STORAGE_SPACE_START);
  if (r2[3] != 450) { printf("  після перезавантаження %u замість 450\n", r2[3]); bad++; }
  printf("  переживає перезавантаження: %s\n", r2[3] == 450 ? "OK" : "ПОМИЛКА");

  // 4. Не залазимо в системну зону Arduboy.
  int last = EEPROM_STORAGE_SPACE_START + 2 + LEVEL_COUNT * 2 - 1;
  printf("  зона збереження %d..%d (системна 0..%d): %s\n",
         EEPROM_STORAGE_SPACE_START, last, EEPROM_STORAGE_SPACE_START - 1,
         last < 1024 ? "OK" : "ВИХІД ЗА EEPROM");
  if (last >= 1024) bad++;

  return bad;
}

int main() {
  printf("--- дрібний шрифт 3x5 ---\n");
  int bad = checkFont();
  printf("\n--- рекорди в EEPROM ---\n");
  bad += checkRecords();
  if (bad) {
    printf("\nUI-перевірка провалена (%d).\n", bad);
    return 1;
  }
  printf("\nUI OK.\n");
  return 0;
}
