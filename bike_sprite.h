#pragma once
#include "gd_core.h"

// ---------------------------------------------------------------------------
// Геометрія спрайта мотоцикла.
//
// Растровий спрайт тут не працює: рама вільно обертається на 360°, тож
// довелося б тримати 16+ повернутих бітмапів (сотні байт флешу) і все одно
// мати рвані кути на проміжних. Замість цього — векторний каркас у локальних
// координатах рами, який щокадру повертається двома множеннями на точку.
//
//   along  — вздовж рами: 0 = задня вісь, 15 = передня
//   height — вгору від лінії осей
//
// Дані тут, а не в .ino, щоб десктопний preview малював рівно те саме,
// що й залізо, а не свою копію, яка з часом розійдеться.
// ---------------------------------------------------------------------------

namespace gd {

enum : uint8_t {
  P_RAXLE, P_FAXLE, P_PIVOT, P_SEATR, P_SEATF, P_TANK, P_STEER, P_BAR,
  P_HIP, P_SHLD, P_HEAD, P_KNEE, P_COUNT
};

// Треліс, а не «мотоцикл із кузовом». На базі 15 px будь-яка друга лінія,
// що йде паралельно за 2 px (контур двигуна, обтічник), зливається з рамою
// в суцільну пляму. Тому лишились тільки ті лінії, що несуть силует —
// і це заразом те, як реально виглядає триальний мот: гола рама.
const int8_t PROGMEM BIKE_PTS[P_COUNT * 2] = {
   0,  0,   // RAXLE  задня вісь
  15,  0,   // FAXLE  передня вісь
   5,  2,   // PIVOT  вісь маятника, вона ж підніжка
   1,  7,   // SEATR  хвіст сідла
   6,  8,   // SEATF  перед сідла
  10,  7,   // TANK   бак
  12,  6,   // STEER  рульова колонка
  14,  8,   // BAR    кермо
   3,  9,   // HIP    таз райдера
   7, 13,   // SHLD   плече
   8, 15,   // HEAD   голова
   6,  6,   // KNEE   коліно
};

const uint8_t PROGMEM BIKE_LINES[] = {
  // ходова
  P_RAXLE, P_PIVOT,   // маятник
  P_RAXLE, P_SEATR,   // задній амортизатор
  P_STEER, P_FAXLE,   // передня вилка з нахилом
  P_STEER, P_BAR,     // кермо
  // рама
  P_PIVOT, P_TANK,    // нижня труба через двигун
  P_PIVOT, P_SEATF,   // підрамник
  P_SEATR, P_SEATF,   // сідло
  P_SEATF, P_TANK,    // бак
  P_TANK,  P_STEER,
  // райдер
  P_HIP,   P_SHLD,    P_SHLD, P_BAR,    P_SHLD, P_HEAD,
  P_HIP,   P_KNEE,    P_KNEE, P_PIVOT,
};
constexpr uint8_t BIKE_LINE_COUNT = sizeof(BIKE_LINES) / 2;

// 16 напрямків радіусом 4 px — спиця, щоб було видно обертання колеса.
const int8_t PROGMEM SPOKE[32] = {
   4, 0,   4, 2,   3, 3,   2, 4,   0, 4,  -2, 4,  -3, 3,  -4, 2,
  -4, 0,  -4,-2,  -3,-3,  -2,-4,   0,-4,   2,-4,   3,-3,   4,-2,
};

// Проєкція каркаса в екранні координати.
// ux, uy — одиничний вектор рами в Q8 (з bikeAxes). «Вгору» = (uy, -ux).
// Усе в int16: |along|,|height| <= 16, |u| <= 256 -> добуток <= 4096.
inline void bikeProject(int16_t rx, int16_t ry, int16_t ux, int16_t uy,
                        int16_t *outX, int16_t *outY) {
  for (uint8_t i = 0; i < P_COUNT; i++) {
    int8_t a = (int8_t)pgm_read_byte(&BIKE_PTS[i * 2]);
    int8_t h = (int8_t)pgm_read_byte(&BIKE_PTS[i * 2 + 1]);
    outX[i] = rx + (int16_t)((a * ux + h * uy + 128) >> 8);
    outY[i] = ry + (int16_t)((a * uy - h * ux + 128) >> 8);
  }
}

inline uint8_t bikeSpokePhase(int16_t spin) {
  return (uint8_t)(spin / SPIN_PER_PHASE) & (SPIN_PHASES - 1);
}

} // namespace gd
