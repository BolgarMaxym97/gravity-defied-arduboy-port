// Мок Arduboy2 для перевірки компіляції адаптера на десктопі.
// Головне тут — struct Point, який і викликав колізію: він відтворює
// реальний конфлікт, тому регресія буде зловлена на десктопі, а не на AVR.
#pragma once
#include <stdint.h>
#include <stdio.h>

#define WHITE 1
#define BLACK 0
#define A_BUTTON     0x01
#define B_BUTTON     0x02
#define UP_BUTTON    0x04
#define DOWN_BUTTON  0x08
#define LEFT_BUTTON  0x10
#define RIGHT_BUTTON 0x20
#define F(s) (s)
#define RED_LED   0
#define GREEN_LED 1
#define BLUE_LED  2
#define RGB_ON    1
#define RGB_OFF   0
#define EEPROM_STORAGE_SPACE_START 16

// Саме цей тип є в справжньому Arduboy2.h.
struct Point {
  int16_t x;
  int16_t y;
};

struct Rect {
  int16_t x, y;
  uint8_t width, height;
};

class Arduboy2 {
public:
  void begin() {}
  void setFrameRate(uint8_t) {}
  void initRandomSeed() {}
  bool nextFrame() { return true; }
  void pollButtons() {}
  bool pressed(uint8_t) { return false; }
  bool justPressed(uint8_t) { return false; }
  void clear() {}
  void display() {}
  void drawLine(int16_t, int16_t, int16_t, int16_t, uint8_t) {}
  void drawCircle(int16_t, int16_t, uint8_t, uint8_t) {}
  void drawRect(int16_t, int16_t, uint8_t, uint8_t, uint8_t) {}
  void fillRect(int16_t, int16_t, uint8_t, uint8_t, uint8_t) {}
  void drawPixel(int16_t, int16_t, uint8_t) {}
  void setCursor(int16_t, int16_t) {}
  void setTextSize(uint8_t) {}
  void exitToBootloader() {}

  // Мок аудіо-підсистеми Arduboy2.
  // enabled() у справжньому Arduboy2Audio статична — саме тому її можна
  // передати в конструктор ArduboyTones як вказівник на функцію. Мок мусить
  // повторювати це, інакше десктопна збірка розходиться з залізом.
  struct Audio {
    static bool on_;
    static bool enabled() { return on_; }
    void on()     { on_ = true; }
    void off()    { on_ = false; }
    void toggle() { on_ = !on_; }
    void saveOnOff() {}
    void begin()  {}
  } audio;
  void drawBitmap(int16_t, int16_t, const uint8_t *, uint8_t, uint8_t, uint8_t) {}
  void digitalWriteRGB(uint8_t, uint8_t) {}
  template <typename T> void print(T) {}
};

inline bool Arduboy2::Audio::on_ = true;
