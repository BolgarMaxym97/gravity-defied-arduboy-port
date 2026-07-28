#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// Fixed-point Q8.8: 256 == 1.0
// Float на ATmega32u4 софтверний (~100+ тактів на множення). При 60 FPS
// і бюджеті ~266000 тактів на кадр це неприйнятно. Все на int32.
// ---------------------------------------------------------------------------

using fp = int32_t;

constexpr int FP_SHIFT = 8;
constexpr fp  FP_ONE   = 1 << FP_SHIFT;

constexpr fp fpFromInt(int16_t v) { return (fp)v << FP_SHIFT; }
constexpr int16_t fpToInt(fp v)   { return (int16_t)(v >> FP_SHIFT); }

// Округлення до найближчого замість відкидання. Для фізики різниці немає,
// а от намальована рама при повільному обертанні перестає смикатись на
// пів пікселя туди-сюди.
constexpr int16_t fpRound(fp v)   { return (int16_t)((v + (FP_ONE / 2)) >> FP_SHIFT); }

// Множення Q8*Q8 -> Q8. Аргументи мусять бути «малими» (різниці координат,
// швидкості), інакше проміжок переповнить int32.
inline fp fpMul(fp a, fp b) { return (a * b) >> FP_SHIFT; }

inline fp fpDiv(fp a, fp b) { return b ? ((a << FP_SHIFT) / b) : 0; }

// Цілочисельний sqrt (метод Ньютона на бітах). Sqrt від Q16 дає Q8.
inline uint32_t isqrt32(uint32_t n) {
  uint32_t res = 0;
  uint32_t bit = 1UL << 30;
  while (bit > n) bit >>= 2;
  while (bit) {
    if (n >= res + bit) { n -= res + bit; res = (res >> 1) + bit; }
    else                { res >>= 1; }
    bit >>= 2;
  }
  return res;
}

// Довжина вектора в Q8. dx, dy — Q8; dx*dx дає Q16.
inline fp fpLen(fp dx, fp dy) {
  return (fp)isqrt32((uint32_t)(dx * dx + dy * dy));
}

inline fp fpClamp(fp v, fp lo, fp hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
