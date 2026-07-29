#pragma once
#include <stdint.h>
#include "gd_core.h"

// ---------------------------------------------------------------------------
// Типи ігрового стану.
//
// Лежать у заголовку, а НЕ в .ino, і це не стиль. Збирач Arduino сам генерує
// прототипи всіх функцій скетчу і вставляє їх одразу після останнього
// #include — тобто перед будь-яким кодом, написаним у самому .ino. Функція
// на кшталт isMenuState(St) отримує прототип раніше, ніж компілятор побачить
// enum class St, і збірка падає з "'St' was not declared in this scope".
//
// Десктопна збірка цього не ловить: g++ компілює .ino як звичайний C++ і
// прототипів не додає. Тому перевірка sim/inocheck.py відтворює цей крок.
// ---------------------------------------------------------------------------

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

enum class St : uint8_t { Splash, Menu, Levels, Options, Play, Crash, Win };

// Пункти головного меню.
enum : uint8_t { MI_PLAY, MI_LEVELS, MI_OPTIONS, MI_COUNT };

constexpr uint8_t LIST_ROWS = 5;    // видимих рядків у списку рівнів

inline bool isMenuState(St s) {
  return s == St::Menu || s == St::Levels || s == St::Options;
}
