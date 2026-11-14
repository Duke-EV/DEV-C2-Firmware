#include <Arduino.h>
#include <vehicle.hpp>

unsigned long last_time = 0;

void send_vehicle_states() {
  Serial.print(g_vehicle.m_peripherals_windshield);        Serial.print(",");
  Serial.print(g_vehicle.m_peripherals_backrunninglights); Serial.print(",");
  Serial.print(g_vehicle.m_peripherals_turn);              Serial.print(",");
  Serial.print(g_vehicle.m_peripherals_headlights);        Serial.print(",");
  Serial.print(g_vehicle.m_peripherals_brakelights);       Serial.print(",");
  Serial.print(g_vehicle.m_peripherals_hazard);            Serial.print(",");
  Serial.print(g_vehicle.m_pdb_current);                   Serial.print(",");
  Serial.print(g_vehicle.m_pdb_voltage);                   Serial.print(",");
  Serial.print(g_vehicle.m_pdb_motor_enabled);             Serial.print(",");
  Serial.print(g_vehicle.m_motor_rpm);                     Serial.print(",");
  Serial.print(g_vehicle.m_throttle_raw);                  Serial.print(",");
  Serial.print(g_vehicle.m_throttle_average);              Serial.print(",");
  Serial.print(g_vehicle.m_joulemeter_current);            Serial.print(",");
  Serial.print(g_vehicle.m_joulemeter_voltage);            Serial.print(",");
  Serial.print(g_vehicle.m_joulemeter_energy);
  Serial.println();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  
  g_vehicle.init_network(DevBoard::TEST_BOARD);
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
  CANMessage msg;
  uint8_t frames_processed = 0;
  while (frames_processed < 4 && g_vehicle.read_message(msg)) {
    frames_processed++;
    Serial.print("ID: 0x");
    Serial.print(msg.id, HEX);
    Serial.print("  Data: ");
    for (int i = 0; i < msg.len; i++) {
        if (msg.buf[i] < 0x10) Serial.print("0");
        Serial.print(msg.buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
  }
  unsigned long now = millis();
  if (now - last_time >= 10) {
    last_time = now;
    send_vehicle_states();
  }
}
