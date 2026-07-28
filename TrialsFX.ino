#include <Arduboy2.h>
#include <EEPROM.h>
#include "gd_core.h"
#include "bike_sprite.h"
#include "levels.h"
#include "tinyfont.h"
#include "records.h"

// Ядро фізики живе в namespace gd. Це не косметика: Arduboy2.h визначає
// власний struct Point, і без ізоляції відбувається колізія імен, при якій
// компілятор мовчки бере чужий тип.
using gd::Terrain;
using gd::BikeTuning;
using gd::Bike;
using gd::Input;
using gd::DEFAULT_TUNING;
using gd::bikeInit;
using gd::bikeStep;
using gd::bikeAxes;

// Адаптер під залізо. Уся фізика — в gd_core.h і про Arduboy нічого не знає.
// Завдяки цьому та сама логіка ганяється в десктопному симуляторі.

Arduboy2 arduboy;

BikeTuning tun = DEFAULT_TUNING;
Bike       bike;

// --------------------------- Рівні -----------------------------------------
// levelData() — у levels.h, згенерованому tools/genlevel.py. Там switch,
// а не таблиця вказівників: таблиця в PROGMEM вимагала б pgm_read_word,
// який на десктопі обрізав би 64-бітний вказівник, і симулятор би брехав.

LevelDesc lvl;
Terrain   ter;
uint8_t   lvlIndex;

void loadLevel(uint8_t i) {
  lvl = levelData(i);
  ter.pts = lvl.pts;
  ter.count = lvl.count;
}

enum class St : uint8_t { Title, Play, Crash, Win };
St st = St::Title;

uint16_t frames;      // час проходження, скидається на старті рівня
uint16_t tick;        // тікає завжди — для миготіння тексту й світлодіода
uint8_t  exitHold;
int16_t  camX, camY;
uint16_t speedShown;  // згладжена швидкість для HUD
bool     newRecord;   // щойно побитий рекорд рівня

constexpr int16_t START_X      = 20;   // де стартує заднє колесо
constexpr int16_t CAM_OFFSET_X = 40;   // мотоцикл на 1/3 екрана
constexpr int16_t HUD_H        = 16;   // смуга під шкалу, час і швидкість
// Центр ВИДИМОЇ смуги (HUD_H..63), а не всього екрана — інакше мотоцикл
// їде під самим HUD.
constexpr int16_t CAM_OFFSET_Y = (HUD_H + 63) / 2;

void updateCamera(bool snap) {
  int16_t tx = fpToInt(bike.rear.x) - CAM_OFFSET_X;
  if (tx < 0) tx = 0;
  camX = tx;

  // Вертикальна камера.
  //   lo — найвища точка рівня лягає одразу під HUD
  //   hi — найнижча лягає на нижній край екрана
  int16_t lo = lvl.minY - HUD_H;
  int16_t hi = lvl.maxY - 63;

  int16_t ty;
  if (hi <= lo) {
    // Рівень цілком влазить у видиму смугу — центруємо його і не скролимо.
    // Раніше тут було hi = lo, і плаский рівень притискало до верху екрана:
    // траса опинялась під самим HUD, а під нею пів екрана штриховки.
    ty = ((lvl.minY + lvl.maxY) >> 1) - CAM_OFFSET_Y;
  } else {
    ty = fpToInt(bike.rear.y) - CAM_OFFSET_Y;
    if (ty < lo) ty = lo;
    if (ty > hi) ty = hi;
  }

  // Згладжування: різкий скрол по Y на 64-піксельному екрані нудить.
  camY = snap ? ty : camY + ((ty - camY) >> 2);
}

void startLevel() {
  loadLevel(lvlIndex);
  bikeInit(bike, tun, fpFromInt(START_X), fpFromInt(lvl.startY));
  frames = 0;
  speedShown = 0;
  newRecord = false;
  updateCamera(true);
  st = St::Play;
}

void toTitle() {
  st = St::Title;
  arduboy.digitalWriteRGB(RED_LED, RGB_OFF);
}

// --------------------------- Дрібний шрифт ---------------------------------
// Вбудований шрифт Arduboy2 — 5x7, а setTextSize() лише множить його на ціле
// число. Менше за 1 штатно не буває, тому підписи малюємо власним 3x5
// (tinyfont.h, згенерований tools/genfont.py). Заголовок лишається штатним
// шрифтом — на цьому контрасті й тримається ієрархія екрана.

uint8_t tinyWidth(const char *s) {
  uint8_t n = 0;
  while (*s++) n++;
  return n ? n * TINY_ADV - 1 : 0;
}

void tinyChar(int16_t x, int16_t y, char c) {
  if (c >= 'a' && c <= 'z') c -= 32;               // шрифт тільки великий
  if (c < TINY_FIRST || c > TINY_LAST) c = ' ';
  const uint8_t *g = &TINY_FONT[(uint8_t)(c - TINY_FIRST) * TINY_W];
  for (uint8_t cx = 0; cx < TINY_W; cx++) {
    uint8_t col = pgm_read_byte(&g[cx]);
    for (uint8_t ry = 0; ry < TINY_H; ry++) {
      if (col & (1 << ry)) arduboy.drawPixel(x + cx, y + ry, WHITE);
    }
  }
}

void tinyPrint(int16_t x, int16_t y, const char *s) {
  while (*s) { tinyChar(x, y, *s++); x += TINY_ADV; }
}

void tinyCenter(int16_t y, const char *s) {
  tinyPrint((128 - tinyWidth(s)) / 2, y, s);
}

// Своє форматування замість snprintf: той тягне ~1.5 КБ флешу заради
// двох чисел.
char *fmtNum(char *p, uint16_t v) {
  char t[6];
  uint8_t n = 0;
  do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
  while (n) *p++ = t[--n];
  return p;
}

char *fmtTime(char *p, uint16_t f) {
  p = fmtNum(p, f / 60);
  *p++ = '.';
  *p++ = (char)('0' + (f % 60) / 6);
  return p;
}

// --------------------------- Рекорди ---------------------------------------
// Логіка — в records.h, щоб десктопний uicheck міг її перевірити.

Records<LEVEL_COUNT> records;

// --------------------------- Спрайт мотоцикла ------------------------------
// Геометрія каркаса — у bike_sprite.h, спільна з десктопним preview.

void drawWheel(int16_t cx, int16_t cy, uint8_t phase) {
  arduboy.drawCircle(cx, cy, fpToInt(tun.wheelR), WHITE);
  // Одна спиця на половину радіуса + маточина. Повний діаметр (дві спиці)
  // читався як суцільна риска через колесо і перетягував на себе увагу.
  int8_t sx = (int8_t)pgm_read_byte(&gd::SPOKE[phase * 2]);
  int8_t sy = (int8_t)pgm_read_byte(&gd::SPOKE[phase * 2 + 1]);
  arduboy.drawLine(cx, cy, cx + sx / 2, cy + sy / 2, WHITE);
  arduboy.drawPixel(cx, cy, WHITE);
}

void drawBike() {
  fp ux32, uy32;
  if (!bikeAxes(bike, ux32, uy32)) return;

  int16_t px[gd::P_COUNT], py[gd::P_COUNT];
  gd::bikeProject(fpRound(bike.rear.x) - camX,
                  fpRound(bike.rear.y) - camY,
                  (int16_t)ux32, (int16_t)uy32, px, py);

  for (uint8_t i = 0; i < gd::BIKE_LINE_COUNT; i++) {
    uint8_t a = pgm_read_byte(&gd::BIKE_LINES[i * 2]);
    uint8_t b = pgm_read_byte(&gd::BIKE_LINES[i * 2 + 1]);
    arduboy.drawLine(px[a], py[a], px[b], py[b], WHITE);
  }

  uint8_t phase = gd::bikeSpokePhase(bike.spin);
  drawWheel(px[gd::P_RAXLE], py[gd::P_RAXLE], phase);
  drawWheel(px[gd::P_FAXLE], py[gd::P_FAXLE], phase);

  // Голова — кружечок r=1, щоб райдер читався як людина, а не як паличка.
  arduboy.drawCircle(px[gd::P_HEAD], py[gd::P_HEAD], 1, WHITE);
}

// --------------------------- Рендер ландшафту ------------------------------

void drawTerrain() {
  uint16_t i = ter.segAt(camX, 0);
  if (i > 0) i--;

  for (; i + 1 < ter.count; i++) {
    int16_t x0 = ter.x(i)     - camX;
    int16_t x1 = ter.x(i + 1) - camX;
    if (x1 < 0) continue;
    if (x0 > 128) break;

    int16_t y0 = ter.y(i)     - camY;
    int16_t y1 = ter.y(i + 1) - camY;
    if (y0 > 63 && y1 > 63) continue;          // цілком нижче екрана

    arduboy.drawLine(x0, y0, x1, y1, WHITE);

    // Штриховка під поверхнею — дешевий спосіб показати «землю».
    if ((i & 1) == 0 && y0 < 63) {
      arduboy.drawLine(x0, y0 < -1 ? -1 : y0, x0, 63, WHITE);
    }
  }
}

// --------------------------- HUD -------------------------------------------

// Картатий прапор 8x8. Формат Arduboy2: байт = колонка, біт 0 — верхній
// піксель. Колонка 0 — древко.
const uint8_t PROGMEM FLAG[8] = {
  0xFF, 0x33, 0x33, 0x0C, 0x0C, 0x33, 0x33, 0x00
};

constexpr uint8_t BAR_W = 116;   // шкала; далі 119..126 — прапор

void drawHud() {
  // Чорна смуга під HUD. Штриховка землі — суцільні білі колонки, і без
  // цього білий текст поверх неї просто зникає.
  arduboy.fillRect(0, 0, 128, HUD_H, BLACK);

  // Шкала прогресу зверху. Відсоток числом читався гірше: гравець дивиться
  // на трасу, а не читає цифри.
  arduboy.drawRect(0, 0, BAR_W, 5, WHITE);
  // Рахуємо від старту, а не від нуля: інакше шкала на старті вже частково
  // заповнена. Ділення int32 — bike.distance uint16, віднімання зі START_X
  // під нулем дало б переповнення.
  int32_t done = (int32_t)bike.distance - START_X;
  int32_t span = (int32_t)lvl.finishX - START_X;
  int32_t fill = span > 0 ? (BAR_W - 2) * done / span : 0;
  if (fill > BAR_W - 2) fill = BAR_W - 2;
  if (fill > 0) arduboy.fillRect(1, 1, (uint8_t)fill, 3, WHITE);
  arduboy.drawBitmap(119, 0, FLAG, 8, 8, WHITE);

  // Поточний час — штатним шрифтом, рекорд у дужках — дрібним, щоб не
  // конкурував з ним за увагу.
  arduboy.setCursor(0, 8);
  arduboy.print(frames / 60);
  arduboy.print('.');
  arduboy.print((frames % 60) / 6);

  if (records[lvlIndex]) {
    char buf[10];
    char *p = buf;
    *p++ = '(';
    p = fmtTime(p, records[lvlIndex]);
    *p++ = ')';
    *p = 0;
    tinyPrint(32, 9, buf);
  }

  // Швидкість. Модуль швидкості заднього колеса, px/кадр -> умовні одиниці.
  // Згладжуємо: сире значення стрибає щокадру на контактах з ландшафтом.
  fp vx = bike.rear.x - bike.rear.px;
  fp vy = bike.rear.y - bike.rear.py;
  uint16_t raw = (uint16_t)((fpLen(vx, vy) * 60) >> 11);
  speedShown += ((int16_t)raw - (int16_t)speedShown) >> 2;

  arduboy.setCursor(speedShown >= 100 ? 104 : 110, 8);
  arduboy.print(speedShown);
}

// Плашка під текст: поверх ландшафту білі літери на білих лініях не читаються.
void drawPanel(int16_t x, int16_t y, uint8_t w, uint8_t h) {
  arduboy.fillRect(x, y, w, h, BLACK);
  arduboy.drawRect(x, y, w, h, WHITE);
}

// --------------------------- Титулка ---------------------------------------
// Титулка — САМОСТІЙНИЙ екран, а не напис поверх сцени. Інакше під ним
// малюються ландшафт і мотоцикл, і спрайт наїжджає на рядок з рівнем.

const char *difficultyName(uint8_t i) {
  if (i < 2) return "EASY";
  if (i < 5) return "MEDIUM";
  if (i < 8) return "HARD";
  return "BRUTAL";
}

void drawTitle() {
  // Заголовок — штатний 5x7. Усе інше дрібним 3x5: різниця в розмірі і
  // створює ієрархію, якої не було, коли все йшло одним кеглем.
  arduboy.setTextSize(1);
  arduboy.setCursor(25, 3);
  arduboy.print(F("GRAVITY DEFIED"));
  arduboy.setCursor(25, 10);
  arduboy.print(F("______________"));

  char buf[24];
  char *p = buf;
  for (const char *q = "LEVEL "; *q; q++) *p++ = *q;
  p = fmtNum(p, lvlIndex + 1);
  *p++ = ' ';
  *p++ = ' ';
  for (const char *q = difficultyName(lvlIndex); *q; q++) *p++ = *q;
  *p = 0;
  tinyCenter(22, buf);

  p = buf;
  for (const char *q = "BEST "; *q; q++) *p++ = *q;
  if (records[lvlIndex]) p = fmtTime(p, records[lvlIndex]);
  else { *p++ = '-'; *p++ = '-'; *p++ = '.'; *p++ = '-'; }
  *p = 0;
  tinyCenter(30, buf);

  // Стрілки-підказки лише там, де є куди йти.
  if (lvlIndex > 0)               tinyPrint(6, 22, "<");
  if (lvlIndex + 1 < LEVEL_COUNT) tinyPrint(119, 22, ">");

  tinyCenter(42, "A=GAS  B=BRAKE/REVERSE");
  tinyCenter(49, "<> = LEAN    v = MENU");

  if ((tick >> 5) & 1) tinyCenter(58, "A = START");
}

// --------------------------- Life cycle ------------------------------------

void setup() {
  arduboy.begin();
  arduboy.setFrameRate(60);
  records.load(EEPROM_STORAGE_SPACE_START);
  lvlIndex = 0;
  startLevel();
  toTitle();
}

void loop() {
  if (!arduboy.nextFrame()) return;
  arduboy.pollButtons();
  tick++;

  // Вихід у FX-меню: UP+DOWN 3 секунди, і тільки з титулки — у грі DOWN
  // зайнятий поверненням у меню.
  if (st == St::Title && arduboy.pressed(UP_BUTTON) && arduboy.pressed(DOWN_BUTTON)) {
    if (++exitHold > 180) arduboy.exitToBootloader();
  } else {
    exitHold = 0;
  }

  if (st == St::Play) {
    Input in;
    in.gas       = arduboy.pressed(A_BUTTON);
    in.brake     = arduboy.pressed(B_BUTTON);
    in.leanLeft  = arduboy.pressed(LEFT_BUTTON);    // задирає ніс
    in.leanRight = arduboy.pressed(RIGHT_BUTTON);   // опускає ніс

    bikeStep(bike, ter, tun, in);
    frames++;

    if (bike.crashed) {
      st = St::Crash;
    } else if (fpToInt(bike.rear.x) >= lvl.finishX) {
      st = St::Win;
      newRecord = records.submit(lvlIndex, frames);
    }

    if (arduboy.justPressed(DOWN_BUTTON)) toTitle();

  } else if (st == St::Title) {
    // Перемикання рівня перезапускає й мотоцикл — інакше він лишався б
    // у координатах попереднього рівня, десь у порожнечі.
    bool changed = false;
    if (arduboy.justPressed(RIGHT_BUTTON) && lvlIndex + 1 < LEVEL_COUNT) {
      lvlIndex++; changed = true;
    }
    if (arduboy.justPressed(LEFT_BUTTON) && lvlIndex > 0) {
      lvlIndex--; changed = true;
    }
    if (changed) {
      loadLevel(lvlIndex);
      bikeInit(bike, tun, fpFromInt(START_X), fpFromInt(lvl.startY));
      updateCamera(true);
    }
    if (arduboy.justPressed(A_BUTTON)) startLevel();

  } else {
    // Crash / Win
    if (arduboy.justPressed(DOWN_BUTTON)) toTitle();
    else if (arduboy.justPressed(A_BUTTON)) startLevel();
    else if (st == St::Win && arduboy.justPressed(RIGHT_BUTTON) &&
             lvlIndex + 1 < LEVEL_COUNT) {
      lvlIndex++;
      startLevel();
    }
  }

  // Червоний світлодіод миготить, поки лежимо.
  arduboy.digitalWriteRGB(RED_LED,
                          (st == St::Crash && ((tick >> 3) & 1)) ? RGB_ON : RGB_OFF);

  updateCamera(false);
  arduboy.clear();

  if (st == St::Title) {
    drawTitle();
    arduboy.display();
    return;
  }

  drawTerrain();
  drawBike();
  drawHud();

  if (st == St::Crash || st == St::Win) {
    drawPanel(18, 18, 92, 34);
    char buf[26];
    char *p;

    if (st == St::Crash) {
      arduboy.setCursor(37, 23); arduboy.print(F("CRASHED"));
      tinyCenter(38, "A = RETRY    v = MENU");
    } else {
      arduboy.setCursor(40, 21); arduboy.print(F("FINISH!"));

      p = buf;
      for (const char *q = "TIME "; *q; q++) *p++ = *q;
      p = fmtTime(p, frames);
      for (const char *q = "   BEST "; *q; q++) *p++ = *q;
      p = fmtTime(p, records[lvlIndex]);
      *p = 0;
      tinyCenter(31, buf);

      if (newRecord) tinyCenter(38, "NEW RECORD!");

      if (lvlIndex + 1 < LEVEL_COUNT) tinyCenter(45, "> = NEXT    v = MENU");
      else                            tinyCenter(45, "A = AGAIN   v = MENU");
    }
  }

  arduboy.display();
}
