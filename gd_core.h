#pragma once
#include "fp.h"

// ---------------------------------------------------------------------------
// Ядро фізики. Жодних залежностей від Arduino — компілюється і на десктопі.
// Модель: Verlet-інтеграція двох точкових мас (заднє й переднє колесо),
// між ними жорсткий зв'язок (projection constraint). Verlet у цілих числах
// набагато стабільніший за явний Ейлер: швидкість не зберігається окремо,
// тому вона не «розганяється» від накопичення похибки.
// ---------------------------------------------------------------------------

#if defined(ARDUINO) || defined(__AVR__)
  #include <avr/pgmspace.h>
  #define TERRAIN_RD(p) ((int16_t)pgm_read_word(p))
#else
  #ifndef PROGMEM
    #define PROGMEM
  #endif
  #ifndef pgm_read_byte
    #define pgm_read_byte(p) (*(const uint8_t *)(p))
  #endif
  #define TERRAIN_RD(p) (*(p))
#endif

namespace gd {

// --------------------------- Ландшафт --------------------------------------
// Полілінія: пари (x, y) у пікселях, x строго зростає.
// Лежить у PROGMEM (на FX — узагалі на зовнішньому флеші), не в RAM.
struct Terrain {
  const int16_t *pts;
  uint16_t count;

  int16_t x(uint16_t i) const { return TERRAIN_RD(&pts[i * 2]); }
  int16_t y(uint16_t i) const { return TERRAIN_RD(&pts[i * 2 + 1]); }

  // Індекс сегмента, що містить worldX. Пошук стартує з підказки —
  // мотоцикл рухається неперервно, тому це майже завжди 0-1 крок.
  uint16_t segAt(int16_t worldX, uint16_t hint) const {
    if (hint >= count - 1) hint = count - 2;
    while (hint > 0 && worldX < x(hint)) hint--;
    while (hint < count - 2 && worldX >= x(hint + 1)) hint++;
    return hint;
  }
};

// --------------------------- Налаштування ----------------------------------
// Саме ці числа доведеться крутити сотні разів. Тому вони тут, а не
// розмазані по коду.
struct BikeTuning {
  fp gravity;     // прискорення за кадр, Q8
  fp damping;     // 256 = без втрат
  fp wheelR;      // радіус колеса
  fp wheelBase;   // відстань між осями
  fp rollDrag;    // опір котіння на накаті (0..256, 256 = миттєвий стоп)
  fp drive;       // номінальна сила газу (визначає максимальну швидкість)
  fp torqueBoost; // у скільки разів більше моменту на нульовій швидкості, Q8
  fp torqueKnee;  // швидкість, вище якої момент уже номінальний
  fp grip;        // коефіцієнт зчеплення: стеля тяги = grip * притискна сила
  fp brake;       // гасіння дотичної швидкості при натиснутому B
  fp reverseGate; // нижче цієї швидкості B перемикається на задній хід
  fp reverse;     // тяга заднього ходу
  fp reverseTop;  // стеля швидкості заднього ходу
  // Нахил зроблено як регулятор кутової швидкості, а не як «постійний
  // імпульс, поки кнопка натиснута». Деталі — в applyLean().
  fp leanRamp;    // за скільки Q8/кадр команда набирає повну силу (0..256)
  fp leanSpin;    // цільова кутова швидкість при повністю натиснутій кнопці
  fp leanGain;    // P-коефіцієнт: наскільки сильно тягнемо до цілі
  fp leanIdle;    // той самий коефіцієнт, коли кнопку відпущено (гасіння)
  fp leanTorque;  // стеля моменту за кадр — щоб не було ривка на старті
  fp leanGrip;    // множник моменту, коли заднє колесо на землі (Q8)
  fp riderHeight; // висота «голови» над рамою — точка, за якою рахується краш
  uint8_t constraintIters;
};

constexpr BikeTuning DEFAULT_TUNING = {
  /*gravity*/          24,
  /*damping*/          254,
  /*wheelR*/           fpFromInt(4),
  /*wheelBase*/        fpFromInt(15),
  /*rollDrag*/         8,          // ~3% за кадр; було 76 (30%) — це був ручник
  /*drive*/            22,
  /*torqueBoost*/      560,        // ~2.2x на низах
  /*torqueKnee*/       fpFromInt(3),
  /*grip*/             470,        // ~1.8; менше — буксує на схилах
  /*brake*/            120,
  /*reverseGate*/      64,         // 0.25 px/кадр — практично стоїмо
  /*reverse*/          14,         // слабша за газ: задом не їздять швидко
  /*reverseTop*/       fpFromInt(2),
  /*leanRamp*/         48,         // повна сила за ~5 кадрів (0.09 c)
  /*leanSpin*/         700,        // масштабується з базою: 512 * 15/11
  /*leanGain*/         20,
  /*leanIdle*/         5,
  /*leanTorque*/       49,        // теж на базу: 36 * 15/11
  /*leanGrip*/         110,
  /*riderHeight*/      fpFromInt(12),
  /*constraintIters*/  3,
};

// --------------------------- Стан ------------------------------------------
struct MassPoint {
  fp x, y, px, py;   // поточна й попередня позиція (Verlet)
};

// Один оберт колеса = 2*pi*R ~= 25.1 px. Ділимо на 16 фаз для спиць.
constexpr int16_t SPIN_PHASES    = 16;
constexpr int16_t SPIN_PER_PHASE = 402;   // Q8: 1.57 px пройденого шляху
constexpr int16_t SPIN_WRAP      = SPIN_PHASES * SPIN_PER_PHASE;

struct Bike {
  MassPoint rear, front;
  uint16_t segHintR, segHintF;
  bool crashed;
  bool rearGrounded;
  bool frontGrounded;
  uint16_t distance;   // ПОТОЧНА позиція по X, не максимум: заднім ходом
                       // прогрес має відкочуватись назад
  int16_t  spin;       // фаза обертання колеса, [0, SPIN_WRAP)
  int16_t  leanCmd;    // згладжена команда нахилу, [-256, 256]
};

struct Input {
  bool gas, brake, leanLeft, leanRight;
};

// Швидкість задається одним зрозумілим числом замість підбору drive навмання.
//
// Множник 2 не косметичний: газ прикладається лише до заднього колеса, а
// зв'язок рами тут же ділить імпульс між двома масами. Тому без нього
// реальна швидкість виходила рівно вдвічі меншою за заявлену в константі.
inline fp driveForTopSpeed(uint8_t pxPerFrame, fp damping) {
  return (fp)pxPerFrame * (FP_ONE - damping) * 2;
}

// --------------------------- Внутрішні хелпери -----------------------------

// Найближча точка на відрізку AB до C. Все в Q8.
inline void closestOnSeg(fp ax, fp ay, fp bx, fp by, fp cx, fp cy, fp &qx, fp &qy) {
  fp ex = bx - ax, ey = by - ay;
  fp wx = cx - ax, wy = cy - ay;
  fp lenSq = (ex * ex + ey * ey) >> FP_SHIFT;
  if (lenSq <= 0) { qx = ax; qy = ay; return; }
  fp dot = (ex * wx + ey * wy) >> FP_SHIFT;
  fp t = fpClamp((dot << FP_SHIFT) / lenSq, 0, FP_ONE);
  qx = ax + ((ex * t) >> FP_SHIFT);
  qy = ay + ((ey * t) >> FP_SHIFT);
}

// Крива моменту. На малих обертах реальний двигун дає більше моменту, ніж
// на максимальних — саме цього бракувало, щоб заїжджати в гору. Вище
// torqueKnee сила стала, тому максимальна швидкість лишається під контролем
// drive і не роз'їжджається разом із тяговитістю.
inline fp engineForce(const BikeTuning &tun, fp vt) {
  if (vt >= tun.torqueKnee) return tun.drive;
  if (vt < 0) vt = 0;
  fp extra = (tun.drive * (tun.torqueBoost - FP_ONE)) >> FP_SHIFT;
  return tun.drive + extra - (extra * vt) / tun.torqueKnee;
}

// Зчеплення. Тягу обмежує не двигун, а притискна сила: на крутому схилі
// нормальна складова ваги менша, тож і тяги менше. Без цієї межі буст
// моменту дозволив би їхати по вертикальній стіні.
inline fp gripLimit(const BikeTuning &tun, fp ny) {
  fp load = (-ny * tun.gravity) >> FP_SHIFT;   // ny < 0, коли поверхня знизу
  if (load <= 0) return 0;
  return (load * tun.grip) >> FP_SHIFT;
}

// Колізія одного колеса з ландшафтом. Повертає true при контакті
// і віддає одиничну нормаль.
inline bool wheelTouch(MassPoint &p, const Terrain &ter, const BikeTuning &tun,
                       uint16_t &hint, fp &nx, fp &ny) {
  int16_t wx = fpToInt(p.x);
  hint = ter.segAt(wx, hint);

  bool hit = false;

  // Вікно +-2 сегменти. +-1 вистачало на пологому рівні 1, але на крутих
  // схилах кілька коротких сегментів лягають майже на однакові x, і колесо
  // провалюється між ними.
  uint16_t from = hint > 1 ? hint - 2 : 0;
  uint16_t to   = hint + 2 < ter.count - 1 ? hint + 2 : ter.count - 2;

  for (uint16_t i = from; i <= to; i++) {
    fp ax = fpFromInt(ter.x(i)),     ay = fpFromInt(ter.y(i));
    fp bx = fpFromInt(ter.x(i + 1)), by = fpFromInt(ter.y(i + 1));

    // Дешевий відсів по X до будь-яких ділень (вони на AVR найдорожчі).
    if (p.x + tun.wheelR < ax || p.x - tun.wheelR > bx) continue;

    fp qx, qy;
    closestOnSeg(ax, ay, bx, by, p.x, p.y, qx, qy);

    fp dx = p.x - qx, dy = p.y - qy;
    fp d = fpLen(dx, dy);

    if (d < tun.wheelR) {
      if (d < 4) { dx = 0; dy = -FP_ONE; d = FP_ONE; }   // виродження
      fp ux = (dx << FP_SHIFT) / d;
      fp uy = (dy << FP_SHIFT) / d;
      fp push = tun.wheelR - d;

      p.x += (ux * push) >> FP_SHIFT;
      p.y += (uy * push) >> FP_SHIFT;

      nx = ux; ny = uy;
      hit = true;
    }
  }
  return hit;
}

// Колізія колеса з ландшафтом уздовж усього шляху за кадр, а не лише в
// кінцевій точці.
//
// Це не перестраховка. З обриву в 98 px колесо набирає 4.3 px/кадр —
// більше за свій радіус — і за один крок опиняється по інший бік землі,
// де жоден сегмент його вже не бачить. Далі воно летить під ландшафтом до
// самого фінішу. Перебірник знаходив цю дірку і «проходив» рівень за
// 4 секунди; гравець провалився б так само, тільки випадково.
inline bool resolveWheel(MassPoint &p, const Terrain &ter, const BikeTuning &tun,
                         uint16_t &hint, fp &nx, fp &ny) {
  nx = 0; ny = -FP_ONE;

  const fp tgtX = p.x, tgtY = p.y;
  const fp dx = p.x - p.px, dy = p.y - p.py;
  const fp dist = fpLen(dx, dy);

  uint8_t steps = 1;
  if (dist > tun.wheelR) {
    steps = (uint8_t)(dist / tun.wheelR) + 1;
    if (steps > 8) steps = 8;          // стеля: далі це вже не фізика, а збій
  }

  if (steps == 1) return wheelTouch(p, ter, tun, hint, nx, ny);

  for (uint8_t s = 1; s <= steps; s++) {
    p.x = p.px + (dx * s) / steps;
    p.y = p.py + (dy * s) / steps;
    if (wheelTouch(p, ter, tun, hint, nx, ny)) return true;   // перший контакт
  }

  p.x = tgtX;
  p.y = tgtY;
  return false;
}

// Невидимі стіни на торцях траси. Без них заднім ходом можна виїхати за
// x(0): segAt() затискає індекс на нульовому сегменті, closestOnSeg() чіпляє
// його крайню точку, і колесо просто зісковзує з кута в порожнечу.
// Гасимо лише горизонтальну складову швидкості (px = x), вертикальну
// лишаємо, інакше мотоцикл зависав би в повітрі при ударі об стіну.
inline void clampToWorld(MassPoint &p, const Terrain &ter, const BikeTuning &tun) {
  fp r = tun.wheelR;
  fp lo = fpFromInt(ter.x(0)) + r;
  fp hi = fpFromInt(ter.x(ter.count - 1)) - r;

  if (p.x < lo) { p.x = lo; p.px = lo; }
  if (p.x > hi) { p.x = hi; p.px = hi; }
}

// --------------------------- Публічний API ---------------------------------

inline void bikeInit(Bike &b, const BikeTuning &tun, fp startX, fp startY) {
  b.rear  = { startX, startY, startX, startY };
  b.front = { startX + tun.wheelBase, startY, startX + tun.wheelBase, startY };
  b.segHintR = b.segHintF = 0;
  b.crashed = false;
  b.rearGrounded = false;
  b.frontGrounded = false;
  b.distance = 0;
  b.spin = 0;
  b.leanCmd = 0;
}

inline void integrate(MassPoint &p, const BikeTuning &tun) {
  fp vx = ((p.x - p.px) * tun.damping) >> FP_SHIFT;
  fp vy = ((p.y - p.py) * tun.damping) >> FP_SHIFT;
  p.px = p.x;
  p.py = p.y;
  p.x += vx;
  p.y += vy + tun.gravity;
}

// Жорсткий зв'язок між осями.
inline void solveConstraint(Bike &b, const BikeTuning &tun) {
  fp dx = b.front.x - b.rear.x;
  fp dy = b.front.y - b.rear.y;
  fp d = fpLen(dx, dy);
  if (d < 16) return;

  fp diff = ((d - tun.wheelBase) << FP_SHIFT) / d;
  fp cx = (dx * diff) >> (FP_SHIFT + 1);
  fp cy = (dy * diff) >> (FP_SHIFT + 1);

  b.rear.x  += cx;  b.rear.y  += cy;
  b.front.x -= cx;  b.front.y -= cy;
}

// Одиничний вектор рами (заднє -> переднє). «Вгору» = (uy, -ux).
inline bool bikeAxes(const Bike &b, fp &ux, fp &uy) {
  fp dx = b.front.x - b.rear.x;
  fp dy = b.front.y - b.rear.y;
  fp d = fpLen(dx, dy);
  if (d < 16) return false;
  ux = (dx << FP_SHIFT) / d;
  uy = (dy << FP_SHIFT) / d;
  return true;
}

// Кутова швидкість як дотична складова відносної швидкості коліс.
// > 0 означає, що ніс іде вгору.
inline fp bikeSpinRate(const Bike &b, fp ux, fp uy) {
  fp vx = (b.front.x - b.front.px) - (b.rear.x - b.rear.px);
  fp vy = (b.front.y - b.front.py) - (b.rear.y - b.rear.py);
  return ((uy * vx) - (ux * vy)) >> FP_SHIFT;   // проєкція на «вгору»
}

// Нахил: пара протилежних імпульсів перпендикулярно до рами = чистий момент,
// без зсуву центру мас. LEFT задирає ніс (вілі), RIGHT опускає —
// як в оригінальному Gravity Defied.
//
// Плавність тут дають три речі, і кожна лікує свій тип ривка:
//
//  1. leanCmd — команда наростає й спадає за кілька кадрів, а не стрибає
//     0 -> 256 на фронті кнопки. Без цього старт і відпускання клацають.
//  2. Регулятор замість постійного імпульсу: момент пропорційний РІЗНИЦІ
//     між поточною і цільовою кутовою швидкістю. Біля цілі він сам згасає,
//     тому немає удару в стелю, як було з жорстким відсіканням по maxSpin.
//  3. leanIdle — той самий регулятор із малим коефіцієнтом працює й тоді,
//     коли кнопку відпущено, і повільно гасить залишкове обертання. Без
//     нього Verlet зберігає набрану кутову швидкість назавжди, і мотоцикл
//     дрейфує носом уже після того, як гравець відпустив кнопку.
inline void applyLean(Bike &b, const BikeTuning &tun, const Input &in) {
  // 1. Згладжування команди.
  int16_t target = 0;
  if (in.leanLeft != in.leanRight) target = in.leanLeft ? 256 : -256;

  if (b.leanCmd < target) {
    b.leanCmd += (int16_t)tun.leanRamp;
    if (b.leanCmd > target) b.leanCmd = target;
  } else if (b.leanCmd > target) {
    b.leanCmd -= (int16_t)tun.leanRamp;
    if (b.leanCmd < target) b.leanCmd = target;
  }

  fp ux, uy;
  if (!bikeAxes(b, ux, uy)) return;

  // 2. P-регулятор кутової швидкості.
  fp desired = (tun.leanSpin * b.leanCmd) >> FP_SHIFT;
  fp err     = desired - bikeSpinRate(b, ux, uy);
  fp gain    = b.leanCmd ? tun.leanGain : tun.leanIdle;
  fp s       = (err * gain) >> FP_SHIFT;

  s = fpClamp(s, -tun.leanTorque, tun.leanTorque);

  // На зчепленні заднього колеса важіль слабший: там реально працює грунт,
  // а не перенос ваги. Коли ззаду порожньо (початок ендо) — повна сила.
  if (b.rearGrounded) s = (s * tun.leanGrip) >> FP_SHIFT;
  if (s == 0) return;

  const fp upx = uy, upy = -ux;
  fp ax = (upx * s) >> FP_SHIFT;
  fp ay = (upy * s) >> FP_SHIFT;

  b.front.x += ax;  b.front.y += ay;
  b.rear.x  -= ax;  b.rear.y  -= ay;
}

inline void bikeStep(Bike &b, const Terrain &ter, const BikeTuning &tun, const Input &in) {
  if (b.crashed) return;

  integrate(b.rear, tun);
  integrate(b.front, tun);

  // Нахил ДО розв'язання зв'язку: перпендикулярний імпульс усе одно трохи
  // міняє довжину рами, і constraint мусить прибрати це в тому ж кадрі.
  applyLean(b, tun, in);
  for (uint8_t i = 0; i < tun.constraintIters; i++) solveConstraint(b, tun);

  fp nxR, nyR, nxF, nyF;
  bool gR = resolveWheel(b.rear,  ter, tun, b.segHintR, nxR, nyR);
  bool gF = resolveWheel(b.front, ter, tun, b.segHintF, nxF, nyF);
  b.rearGrounded  = gR;
  b.frontGrounded = gF;
  (void)nxF; (void)nyF;

  clampToWorld(b.rear,  ter, tun);
  clampToWorld(b.front, ter, tun);

  // Тяга й тертя — тільки на задньому колесі, по дотичній до поверхні.
  if (gR) {
    fp tx = -nyR, ty = nxR;              // дотична = нормаль, повернута на 90°
    fp vx = b.rear.x - b.rear.px;
    fp vy = b.rear.y - b.rear.py;
    fp vt = ((tx * vx + ty * vy) >> FP_SHIFT);

    if (in.gas) {
      fp f = engineForce(tun, vt);
      fp g = gripLimit(tun, nyR);
      if (f > g) f = g;                        // буксування
      b.rear.px -= (tx * f) >> FP_SHIFT;
      b.rear.py -= (ty * f) >> FP_SHIFT;
    } else if (in.brake) {
      // B працює у три стадії: гальмо -> задній хід -> стеля заднього ходу.
      // Одночасно гальмувати й тягнути назад не можна: гасіння в гальмі
      // пропорційне швидкості, тож рівновага настала б на ~0.25 px/кадр
      // і задній хід був би непомітним.
      if (vt > tun.reverseGate) {
        fp k = (vt * tun.brake) >> FP_SHIFT;          // ще котимось уперед
        b.rear.px += (tx * k) >> FP_SHIFT;
        b.rear.py += (ty * k) >> FP_SHIFT;
      } else if (vt > -tun.reverseTop) {
        b.rear.px += (tx * tun.reverse) >> FP_SHIFT;  // здаємо назад
        b.rear.py += (ty * tun.reverse) >> FP_SHIFT;
      } else {
        fp k = (vt * tun.rollDrag) >> FP_SHIFT;       // впертись у стелю
        b.rear.px += (tx * k) >> FP_SHIFT;
        b.rear.py += (ty * k) >> FP_SHIFT;
      }
    } else {
      fp k = (vt * tun.rollDrag) >> FP_SHIFT;
      b.rear.px += (tx * k) >> FP_SHIFT;
      b.rear.py += (ty * k) >> FP_SHIFT;
    }
  }

  fp ux, uy;
  bool haveAxes = bikeAxes(b, ux, uy);

  // Фаза обертання колеса — потрібна рендеру для спиць. Крутиться від
  // поздовжньої швидкості; у повітрі колесо крутиться далі за інерцією.
  if (haveAxes) {
    fp vx = b.rear.x - b.rear.px;
    fp vy = b.rear.y - b.rear.py;
    fp fwd = ((ux * vx) + (uy * vy)) >> FP_SHIFT;
    int16_t s = (int16_t)(b.spin + fwd);
    while (s >= SPIN_WRAP) s -= SPIN_WRAP;
    while (s < 0)          s += SPIN_WRAP;
    b.spin = s;
  }

  // Краш: «голова» райдера над серединою рами торкнулась землі.
  if (haveAxes) {
    fp mx = (b.rear.x + b.front.x) >> 1;
    fp my = (b.rear.y + b.front.y) >> 1;
    fp hx = mx + ((uy * tun.riderHeight) >> FP_SHIFT);
    fp hy = my - ((ux * tun.riderHeight) >> FP_SHIFT);

    uint16_t h = ter.segAt(fpToInt(hx), b.segHintR);
    fp ax = fpFromInt(ter.x(h)),     ay = fpFromInt(ter.y(h));
    fp bx = fpFromInt(ter.x(h + 1)), by = fpFromInt(ter.y(h + 1));
    fp qx, qy;
    closestOnSeg(ax, ay, bx, by, hx, hy, qx, qy);
    if (fpLen(hx - qx, hy - qy) < fpFromInt(2)) b.crashed = true;
  }

  // Прогрес — поточна позиція, а не рекорд. Інакше відкат назад нічого не
  // змінював би на шкалі, і гравець бачив би заповнення, якого вже немає.
  int16_t px = fpToInt(b.rear.x);
  b.distance = px < 0 ? 0 : (uint16_t)px;
}

} // namespace gd