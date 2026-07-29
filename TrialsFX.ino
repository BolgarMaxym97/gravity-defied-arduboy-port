#include <Arduboy2.h>
#include <ArduboyTones.h>
#include <EEPROM.h>
#include "gd_core.h"
#include "bike_sprite.h"
#include "levels.h"
#include "tinyfont.h"
#include "records.h"
#include "music.h"
#include "game_types.h"

// Адаптер під залізо. Уся фізика — в gd_core.h і про Arduboy нічого не знає.
// Завдяки цьому та сама логіка ганяється в десктопному симуляторі.

Arduboy2 arduboy;
ArduboyTones sound(arduboy.audio.enabled);

BikeTuning tun = DEFAULT_TUNING;
Bike       bike;

// --------------------------- Швидкість байка -------------------------------
// ЄДИНА ручка швидкості: максимальна швидкість на рівному при повному газі,
// у пікселях за кадр (60 кадрів/с). 6 -> ~5.7 px/кадр -> ~345 px/с.
// Прискорення й тяга рахуються від неї, тому міняти треба лише це число.
//
// Розумний діапазон 4..9. Нижче їзда стає в'язкою. Вище — два наслідки:
// на схилах крутіше 33° швидкість усе одно впирається в зчеплення, а не
// в двигун, і байк починає перестрибувати короткі сегменти ландшафту
// між кадрами (сегменти в рівнях від 8 px завдовжки).
constexpr uint8_t BIKE_TOP_SPEED = 6;

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

// Типи стану — в game_types.h: збирач Arduino вставляє згенеровані прототипи
// перед кодом скетчу, тож усе, що з'являється в сигнатурах, мусить бути
// оголошене в заголовку.
St st = St::Splash;

uint8_t menuSel;
uint8_t listSel;    // у списку рівнів: 0..LEVEL_COUNT-1 рівні, LEVEL_COUNT = BACK
uint8_t listTop;    // перший видимий рядок списку
uint8_t optSel;     // 0 = SOUND, 1 = BACK

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
  sound.noTone();          // музика меню й двигун — один голос, див. music.h
  loadLevel(lvlIndex);
  bikeInit(bike, tun, fpFromInt(START_X), fpFromInt(lvl.startY));
  frames = 0;
  speedShown = 0;
  newRecord = false;
  updateCamera(true);
  st = St::Play;
}

void toMenu() {
  st = St::Menu;
  arduboy.digitalWriteRGB(RED_LED, RGB_OFF);
}

// --------------------------- Дрібний шрифт ---------------------------------
// Вбудований шрифт Arduboy2 — 5x7, а setTextSize() лише множить його на ціле
// число. Менше за 1 штатно не буває, тому підписи малюємо власним 3x5
// (tinyfont.h, згенерований tools/genfont.py). Заголовок лишається штатним
// шрифтом — на цьому контрасті й тримається ієрархія екрана.

// Перевантаження, а не аргументи за замовчуванням: збирач Arduino генерує
// прототипи для функцій скетчу, і значення за замовчуванням опинилися б
// одночасно в прототипі й у визначенні — це помилка компіляції.
uint8_t tinyWidth(const char *s, uint8_t scale) {
  uint8_t n = 0;
  while (*s++) n++;
  return n ? n * TINY_ADV * scale - scale : 0;
}

uint8_t tinyWidth(const char *s) { return tinyWidth(s, 1); }

void tinyChar(int16_t x, int16_t y, char c, uint8_t scale) {
  if (c >= 'a' && c <= 'z') c -= 32;               // шрифт тільки великий
  if (c < TINY_FIRST || c > TINY_LAST) c = ' ';
  const uint8_t *g = &TINY_FONT[(uint8_t)(c - TINY_FIRST) * TINY_W];
  for (uint8_t cx = 0; cx < TINY_W; cx++) {
    uint8_t col = pgm_read_byte(&g[cx]);
    for (uint8_t ry = 0; ry < TINY_H; ry++) {
      if (!(col & (1 << ry))) continue;
      if (scale == 1) {
        arduboy.drawPixel(x + cx, y + ry, WHITE);
      } else {
        arduboy.fillRect(x + cx * scale, y + ry * scale, scale, scale, WHITE);
      }
    }
  }
}

void tinyPrint(int16_t x, int16_t y, const char *s, uint8_t scale) {
  while (*s) { tinyChar(x, y, *s++, scale); x += TINY_ADV * scale; }
}

void tinyPrint(int16_t x, int16_t y, const char *s) { tinyPrint(x, y, s, 1); }

void tinyCenter(int16_t y, const char *s, uint8_t scale) {
  tinyPrint((128 - tinyWidth(s, scale)) / 2, y, s, scale);
}

void tinyCenter(int16_t y, const char *s) { tinyCenter(y, s, 1); }

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

// Малює мотоцикл у довільній позі. Винесено з drawBike(), бо тим самим
// кодом малюється сплеш — інакше заставка жила б власним життям і розійшлася
// б із грою після першої ж правки рами.
void drawBikeAt(int16_t rx, int16_t ry, int16_t ux, int16_t uy, uint8_t phase) {
  int16_t px[gd::P_COUNT], py[gd::P_COUNT];
  gd::bikeProject(rx, ry, ux, uy, px, py);

  for (uint8_t i = 0; i < gd::BIKE_LINE_COUNT; i++) {
    uint8_t a = pgm_read_byte(&gd::BIKE_LINES[i * 2]);
    uint8_t b = pgm_read_byte(&gd::BIKE_LINES[i * 2 + 1]);
    arduboy.drawLine(px[a], py[a], px[b], py[b], WHITE);
  }

  drawWheel(px[gd::P_RAXLE], py[gd::P_RAXLE], phase);
  drawWheel(px[gd::P_FAXLE], py[gd::P_FAXLE], phase);

  // Голова — кружечок r=1, щоб райдер читався як людина, а не як паличка.
  arduboy.drawCircle(px[gd::P_HEAD], py[gd::P_HEAD], 1, WHITE);
}

void drawBike() {
  fp ux, uy;
  if (!bikeAxes(bike, ux, uy)) return;
  drawBikeAt(fpRound(bike.rear.x) - camX, fpRound(bike.rear.y) - camY,
             (int16_t)ux, (int16_t)uy, gd::bikeSpokePhase(bike.spin));
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

// Заставка. Композиція один-в-один з tools/gentitle.py (title.png):
// логотип у два рядки масштабом 3, мотоцикл у вілі праворуч, лінія землі.
//
// Малюємо кодом, а не бітмапом: PNG 128x64 коштував би 1024 байти флешу,
// а тут усе вже є — шрифт, спрайт, лінії. Виходить близько 200 байт коду,
// і заставка не може розійтися з грою, бо це буквально той самий спрайт.
constexpr int16_t SPLASH_GROUND = 60;

void drawSplash() {
  tinyCenter(4,  "GRAVITY", 3);
  tinyCenter(21, "DEFIED",  3);

  arduboy.drawLine(0, SPLASH_GROUND, 127, SPLASH_GROUND, WHITE);
  for (int16_t x = 0; x < 128; x += 4) {
    arduboy.drawLine(x, SPLASH_GROUND + 1, x, 62, WHITE);
  }

  // -45°: cos/sin = 0.707 -> 181 у Q8.
  drawBikeAt(100, SPLASH_GROUND - 4, 181, -181, 2);

  tinyPrint(8, 50, "10 LEVELS");
}

// --------------------------- Екрани меню -----------------------------------

// Шапка спільна для всіх меню: штатний 5x7 плюс підкреслення. Пункти нижче
// йдуть дрібним шрифтом — на цьому контрасті й тримається ієрархія.
void drawHeader() {
  arduboy.setTextSize(1);
  arduboy.setCursor(25, 3);
  arduboy.print(F("GRAVITY DEFIED"));
  arduboy.setCursor(25, 10);
  arduboy.print(F("______________"));
}

// Курсор ліворуч від пункту. Миготить, щоб було видно навіть на статичному
// екрані, який пункт активний.
void drawCursor(int16_t x, int16_t y, uint8_t scale) {
  if ((tick >> 4) & 1) tinyPrint(x, y, ">", scale);
}

void drawMenu() {
  drawHeader();

  static const char *const ITEMS[MI_COUNT] = { "PLAY", "LEVELS", "OPTIONS" };
  for (uint8_t i = 0; i < MI_COUNT; i++) {
    int16_t y = 24 + i * 12;
    uint8_t w = tinyWidth(ITEMS[i], 2);
    int16_t x = (128 - w) / 2;
    tinyPrint(x, y, ITEMS[i], 2);
    if (i == menuSel) drawCursor(x - 14, y, 2);
  }
}

// Рядок списку рівнів: "10  BRUTAL  12.3" або "--.-", якщо рекорду немає.
void levelRow(char *p, uint8_t i) {
  if (i + 1 < 10) *p++ = ' ';
  p = fmtNum(p, i + 1);
  *p++ = ' ';
  *p++ = ' ';
  const char *d = difficultyName(i);
  uint8_t n = 0;
  for (; *d; d++, n++) *p++ = *d;
  while (n++ < 6) *p++ = ' ';          // вирівнювання колонки з часом
  *p++ = ' ';
  if (records[i]) p = fmtTime(p, records[i]);
  else { *p++ = '-'; *p++ = '-'; *p++ = '.'; *p++ = '-'; }
  *p = 0;
}

void drawLevels() {
  tinyCenter(3, "SELECT LEVEL", 1);
  arduboy.drawLine(20, 9, 107, 9, WHITE);

  char buf[24];
  for (uint8_t r = 0; r < LIST_ROWS; r++) {
    uint8_t i = listTop + r;
    if (i > LEVEL_COUNT) break;
    int16_t y = 14 + r * 8;

    if (i == LEVEL_COUNT) tinyPrint(20, y, "BACK");
    else { levelRow(buf, i); tinyPrint(20, y, buf); }

    if (i == listSel) drawCursor(12, y, 1);
  }

  // Стрілки прокрутки: без них незрозуміло, що список довший за екран.
  if (listTop > 0)                          tinyPrint(112, 14, "<");
  if (listTop + LIST_ROWS <= LEVEL_COUNT)   tinyPrint(112, 46, ">");

  tinyCenter(56, "A = SELECT   B = BACK");
}

void drawOptions() {
  uint8_t w = tinyWidth("OPTIONS", 2);
  tinyPrint((128 - w) / 2, 2, "OPTIONS", 2);

  tinyPrint(24, 16, "SOUND");
  tinyPrint(76, 16, arduboy.audio.enabled() ? "ON" : "OFF");
  if (optSel == 0) drawCursor(14, 16, 1);

  // Опис керування — те саме, що раніше тіснилося на титулці.
  tinyPrint(14, 27, "A       GAS");
  tinyPrint(14, 34, "B       BRAKE / REVERSE");
  tinyPrint(14, 41, "< >     LEAN");
  tinyPrint(14, 48, "v       BACK TO MENU");

  tinyPrint(24, 57, "BACK");
  if (optSel == 1) drawCursor(14, 57, 1);
}

// --------------------------- Life cycle ------------------------------------

void setup() {
  arduboy.begin();                 // сам піднімає audio і читає його стан з EEPROM
  arduboy.setFrameRate(60);
  records.load(EEPROM_STORAGE_SPACE_START);

  // Швидкість задається одним числом, решта рахується від неї.
  tun.drive = gd::driveForTopSpeed(BIKE_TOP_SPEED, tun.damping);

  lvlIndex = 0;
  menuSel = MI_PLAY;
  listSel = listTop = 0;
  optSel = 0;
  startLevel();
  st = St::Splash;
}

// Звук двигуна. Один голос, тому це працює лише в грі: у меню той самий
// канал зайнятий музикою.
void engineSound(const Input &in) {
  if (!arduboy.audio.enabled()) return;
  if ((tick & 3) != 0) return;                 // ~15 разів на секунду

  bool rolling = in.gas || in.brake;
  if (!rolling || bike.crashed) { sound.noTone(); return; }

  // Тон іде за швидкістю. Тривалість трохи довша за інтервал переграшу —
  // інакше між викликами чути провали і звук стає деренчанням.
  uint16_t f = 70 + speedShown * 3;
  if (f > 520) f = 520;
  if (!in.gas) f = f / 2 + 40;                 // задній хід нижчий і тихіший
  sound.tone(f, 90);
}

void menuMusic() {
  // TONES_REPEAT зациклює мелодію всередині бібліотеки, тож достатньо
  // запустити її один раз — перевірка playing() і робить це один раз.
  if (arduboy.audio.enabled() && !sound.playing()) sound.tones(MENU_MUSIC);
}

// --------------------------- Введення --------------------------------------

void updateMenu() {
  if (arduboy.justPressed(DOWN_BUTTON) && menuSel + 1 < MI_COUNT) menuSel++;
  if (arduboy.justPressed(UP_BUTTON) && menuSel > 0) menuSel--;
  if (!arduboy.justPressed(A_BUTTON)) return;

  switch (menuSel) {
    case MI_PLAY:
      lvlIndex = 0;
      sound.tones(SFX_START);
      startLevel();
      break;
    case MI_LEVELS:
      listSel = lvlIndex;
      listTop = 0;
      st = St::Levels;
      break;
    default:
      optSel = 0;
      st = St::Options;
      break;
  }
}

void updateLevels() {
  if (arduboy.justPressed(DOWN_BUTTON) && listSel < LEVEL_COUNT) listSel++;
  if (arduboy.justPressed(UP_BUTTON) && listSel > 0) listSel--;

  // Прокрутка вікна за курсором.
  if (listSel < listTop) listTop = listSel;
  if (listSel >= listTop + LIST_ROWS) listTop = listSel - LIST_ROWS + 1;

  if (arduboy.justPressed(B_BUTTON)) { toMenu(); return; }
  if (!arduboy.justPressed(A_BUTTON)) return;

  if (listSel >= LEVEL_COUNT) { toMenu(); return; }
  lvlIndex = listSel;
  sound.tones(SFX_START);
  startLevel();
}

void updateOptions() {
  if (arduboy.justPressed(DOWN_BUTTON) && optSel < 1) optSel++;
  if (arduboy.justPressed(UP_BUTTON) && optSel > 0) optSel--;

  bool toggle = arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(RIGHT_BUTTON) ||
                (optSel == 0 && arduboy.justPressed(A_BUTTON));
  if (toggle) {
    if (arduboy.audio.enabled()) { arduboy.audio.off(); sound.noTone(); }
    else                          arduboy.audio.on();
    arduboy.audio.saveOnOff();     // стан переживає вимкнення консолі
  }

  if (arduboy.justPressed(B_BUTTON) ||
      (optSel == 1 && arduboy.justPressed(A_BUTTON))) toMenu();
}

void updatePlay() {
  Input in;
  in.gas       = arduboy.pressed(A_BUTTON);
  in.brake     = arduboy.pressed(B_BUTTON);
  in.leanLeft  = arduboy.pressed(LEFT_BUTTON);    // задирає ніс
  in.leanRight = arduboy.pressed(RIGHT_BUTTON);   // опускає ніс

  bikeStep(bike, ter, tun, in);
  frames++;

  if (bike.crashed) {
    st = St::Crash;
    sound.tones(SFX_CRASH);
  } else if (fpToInt(bike.rear.x) >= lvl.finishX) {
    st = St::Win;
    newRecord = records.submit(lvlIndex, frames);
    sound.tones(newRecord ? SFX_RECORD : SFX_FINISH);
  } else {
    engineSound(in);
  }

  if (arduboy.justPressed(DOWN_BUTTON)) { sound.noTone(); toMenu(); }
}

void updateEnd() {
  if (arduboy.justPressed(DOWN_BUTTON)) { toMenu(); return; }
  if (arduboy.justPressed(A_BUTTON)) { startLevel(); return; }
  if (st == St::Win && arduboy.justPressed(RIGHT_BUTTON) &&
      lvlIndex + 1 < LEVEL_COUNT) {
    lvlIndex++;
    startLevel();
  }
}

void loop() {
  if (!arduboy.nextFrame()) return;
  arduboy.pollButtons();
  tick++;

  // Вихід у FX-меню: UP+DOWN 3 секунди, і тільки з головного меню — скрізь
  // інде DOWN зайнятий навігацією.
  if (st == St::Menu && arduboy.pressed(UP_BUTTON) && arduboy.pressed(DOWN_BUTTON)) {
    if (++exitHold > 180) arduboy.exitToBootloader();
  } else {
    exitHold = 0;
  }

  if (st == St::Splash) {
    if (tick > 150 || arduboy.justPressed(A_BUTTON) ||
        arduboy.justPressed(B_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
      toMenu();
    }
    arduboy.clear();
    drawSplash();
    arduboy.display();
    return;
  }

  switch (st) {
    case St::Menu:    updateMenu();    break;
    case St::Levels:  updateLevels();  break;
    case St::Options: updateOptions(); break;
    case St::Play:    updatePlay();    break;
    default:          updateEnd();     break;
  }

  if (isMenuState(st)) menuMusic();

  // Червоний світлодіод миготить, поки лежимо.
  arduboy.digitalWriteRGB(RED_LED,
                          (st == St::Crash && ((tick >> 3) & 1)) ? RGB_ON : RGB_OFF);

  arduboy.clear();

  if (isMenuState(st)) {
    if (st == St::Menu)        drawMenu();
    else if (st == St::Levels) drawLevels();
    else                       drawOptions();
    arduboy.display();
    return;
  }

  updateCamera(false);
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
