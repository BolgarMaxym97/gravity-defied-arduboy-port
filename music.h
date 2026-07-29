#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// Музика й ефекти у форматі ArduboyTones: пари (частота Гц, тривалість мс),
// 0 — пауза, TONES_END завершує, TONES_REPEAT зациклює.
//
// Ноти визначені тут, а не взяті з ArduboyTonesPitches.h, щоб десктопна
// збірка не тягла всю бібліотеку заради півтора десятка констант.
//
// Важливе обмеження заліза: ArduboyTones грає ОДИН голос. Тому музика і звук
// двигуна не можуть звучати одночасно — музика лише в меню, двигун лише
// в грі. Спроба грати обидва дала б переривчасту кашу.
// ---------------------------------------------------------------------------

#if defined(ARDUINO) || defined(__AVR__)
  #include <avr/pgmspace.h>
#else
  #ifndef PROGMEM
    #define PROGMEM
  #endif
#endif

#ifndef TONES_END
  #define TONES_END    0x8000
#endif
#ifndef TONES_REPEAT
  #define TONES_REPEAT 0x8001
#endif

constexpr uint16_t N_REST = 0;
constexpr uint16_t N_A4   = 440,  N_C5 = 523,  N_D5 = 587,  N_E5 = 659;
constexpr uint16_t N_F5   = 698,  N_G5 = 784,  N_A5 = 880,  N_B5 = 988;
constexpr uint16_t N_C6   = 1047, N_E4 = 330,  N_G4 = 392,  N_C4 = 262;

// Проста мелодія для меню. Навмисно коротка й негучна: вона крутиться
// в циклі, доки гравець обирає рівень.
const uint16_t PROGMEM MENU_MUSIC[] = {
  N_E5, 150,  N_G5, 150,  N_A5, 300,
  N_G5, 150,  N_E5, 150,  N_D5, 300,
  N_C5, 150,  N_D5, 150,  N_E5, 300,
  N_D5, 150,  N_C5, 150,  N_A4, 300,

  N_E5, 150,  N_G5, 150,  N_A5, 300,
  N_C6, 300,  N_B5, 150,  N_A5, 150,
  N_G5, 300,  N_E5, 300,  N_REST, 300,
  TONES_REPEAT
};

const uint16_t PROGMEM SFX_CRASH[] = {
  N_G4, 80,  N_E4, 80,  N_C4, 220,
  TONES_END
};

const uint16_t PROGMEM SFX_FINISH[] = {
  N_C5, 90,  N_E5, 90,  N_G5, 90,  N_C6, 260,
  TONES_END
};

const uint16_t PROGMEM SFX_RECORD[] = {
  N_C5, 70,  N_E5, 70,  N_G5, 70,  N_C6, 70,  N_E5, 70,  N_G5, 260,
  TONES_END
};

const uint16_t PROGMEM SFX_START[] = {
  N_C5, 60,  N_G5, 90,
  TONES_END
};
