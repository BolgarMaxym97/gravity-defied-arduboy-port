// Мок EEPROM для десктопної збірки. Тримає той самий 1 КБ, що й ATmega32u4,
// щоб перевірка адрес мала сенс.
#pragma once
#include <stdint.h>
#include <string.h>

class EEPROMClass {
public:
  uint8_t data[1024] = {0};

  template <typename T> T &get(int idx, T &t) {
    memcpy(&t, data + idx, sizeof(T));
    return t;
  }
  template <typename T> const T &put(int idx, const T &t) {
    memcpy(data + idx, &t, sizeof(T));
    return t;
  }
};

static EEPROMClass EEPROM;
