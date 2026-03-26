#include <Arduino.h>
#include <vehicle.hpp>

String incoming_string;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  
  g_vehicle.init_network(DevBoard::MOTOR_CONTROLLER);
}

void loop() {
  while(Serial.available()) {
    char c = Serial.read();
    if(c == '\n') break;
    incoming_string += c;
  }
}
