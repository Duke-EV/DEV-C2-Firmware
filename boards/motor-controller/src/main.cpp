#include <Arduino.h>
#include <vehicle.hpp>

void setup() {
  g_vehicle.init_network(DevBoard::MOTOR_CONTROLLER);
  // TODO: rest of device setup
}

void loop() {}
