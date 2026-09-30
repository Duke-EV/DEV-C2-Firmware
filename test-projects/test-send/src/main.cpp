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
  //FORMAT: 0x___,abcdefgh
  while(Serial.available()) {
    char c = Serial.read();
    if(c == '\n') break;
    incoming_string += c;
  }

  String idstr = incoming_string.substring(0, 5);
  uint32_t id = (uint32_t) strtoul(idstr.c_str(), nullptr, 0);

  uint8_t buf[8];
  memcpy(buf, incoming_string.substring(6, 14).c_str(), 8);
  
  g_vehicle.send_message(id, 8, &buf);
  incoming_string = "";
}
