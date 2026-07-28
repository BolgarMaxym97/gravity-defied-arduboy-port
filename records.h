#pragma once
#include <stdint.h>
#include <EEPROM.h>

// ---------------------------------------------------------------------------
// Рекорди часу проходження в EEPROM.
//
// Винесено зі скетчу окремо, щоб десктопний uicheck міг ганяти це без
// Arduboy: логіка «чиста EEPROM -> ініціалізувати» найлегше ламається саме
// мовчки, і на залізі це виглядає як рекорд 0.3 c, який ніхто не поб'є.
//
// EEPROM ATmega32u4 — 1 КБ, перші 16 байт зарезервовані під системні дані
// Arduboy (EEPROM_STORAGE_SPACE_START). 0 означає «рекорду ще немає».
// ---------------------------------------------------------------------------

template <uint8_t N>
struct Records {
  static constexpr uint16_t MAGIC = 0x4744;   // 'GD'

  uint16_t best[N];
  int      addr;

  int slot(uint8_t i) const { return addr + 2 + i * 2; }

  void load(int baseAddr) {
    addr = baseAddr;
    uint16_t magic = 0;
    EEPROM.get(addr, magic);
    if (magic != MAGIC) {
      for (uint8_t i = 0; i < N; i++) best[i] = 0;
      uint16_t m = MAGIC;
      EEPROM.put(addr, m);
      for (uint8_t i = 0; i < N; i++) EEPROM.put(slot(i), best[i]);
      return;
    }
    for (uint8_t i = 0; i < N; i++) EEPROM.get(slot(i), best[i]);
  }

  // true, якщо це новий рекорд. EEPROM.put усередині робить update (пише
  // байт лише якщо він змінився), тож ресурс комірок не витрачається дарма.
  bool submit(uint8_t i, uint16_t frames) {
    if (best[i] && frames >= best[i]) return false;
    best[i] = frames;
    EEPROM.put(slot(i), frames);
    return true;
  }

  uint16_t operator[](uint8_t i) const { return best[i]; }
};
