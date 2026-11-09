#pragma once

#include <string>

#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2

extern unsigned long fake_millis;
extern int fake_button_reading;

class FakeSerial {
public:
  void println(const char *message);
};

extern FakeSerial Serial;

inline void pinMode(int pin, int mode) {
  (void)pin;
  (void)mode;
}

inline int digitalRead(int pin) {
  (void)pin;
  return fake_button_reading;
}

inline unsigned long millis() {
  return fake_millis;
}

