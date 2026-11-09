#pragma once

class TwoWire {
public:
  void begin(int sda_pin, int scl_pin);
};

extern TwoWire Wire;
