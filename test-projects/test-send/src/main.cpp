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
  static bool reported_init_error = false;
  if (!g_vehicle.m_network_ready) {
    if (Serial && !reported_init_error) {
      if (g_vehicle.m_network_error == CAN_NETWORK_TIMING_ERROR)
        Serial.println("CAN initialization failed: bit timing configuration");
      else if (g_vehicle.m_network_error == CAN_NETWORK_TIMER_ERROR)
        Serial.println("CAN initialization failed: receive timer unavailable");
      else Serial.println("CAN network is not ready");
      reported_init_error = true;
    }
    return;
  }
  reported_init_error = false;
  //FORMAT: 0x___,payload (up to 64 literal bytes, followed by a newline)
  static bool discard_input = false;
  while(Serial.available()) {
    char c = Serial.read();
    if(c != '\n') {
      if(discard_input) continue;
      // Allow six header characters, 64 payload bytes, and a trailing CR.
      if(incoming_string.length() >= 71) {
        discard_input = true;
        incoming_string = "";
      } else {
        incoming_string += c;
      }
      continue;
    }

    if(incoming_string.endsWith("\r")) {
      incoming_string.remove(incoming_string.length() - 1);
    }
    bool valid = !discard_input && incoming_string.length() >= 6 &&
                 incoming_string.length() <= 70 && incoming_string[5] == ',' &&
                 (incoming_string.startsWith("0x") || incoming_string.startsWith("0X"));
    if(valid) {
      String idstr = incoming_string.substring(0, 5);
      char *end;
      uint32_t id = (uint32_t) strtoul(idstr.c_str(), &end, 16);
      valid = end == idstr.c_str() + 5 && id <= 0x7FF;
      if(valid) {
        uint8_t buf[64];
        uint8_t len = incoming_string.length() - 6;
        memcpy(buf, incoming_string.c_str() + 6, len);
        g_vehicle.send_message(id, len, buf);
        if (g_vehicle.m_network_error != CAN_NETWORK_OK)
          Serial.println("CAN send failed: frame was not queued");
      }
    }
    if(!valid) Serial.println("Invalid CAN command");
    incoming_string = "";
    discard_input = false;
  }
}
