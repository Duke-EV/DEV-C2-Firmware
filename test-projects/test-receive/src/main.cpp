#include <Arduino.h>
#include <vehicle.hpp>

IntervalTimer timer;

void send_vehicle_states() {
  Serial.print(g_vehicle.m_peripherals_windshield);        Serial.print(",");
  Serial.print(g_vehicle.m_peripherals_backrunninglights); Serial.print(",");
  Serial.print(g_vehicle.m_peripherals_turn);              Serial.print(",");
  Serial.print(g_vehicle.m_peripherals_headlights);        Serial.print(",");
  Serial.print(g_vehicle.m_peripherals_brakelights);       Serial.print(",");
  Serial.print(g_vehicle.m_peripherals_hazard);            Serial.print(",");
  Serial.print(g_vehicle.m_pdb_current);                   Serial.print(",");
  Serial.print(g_vehicle.m_pdb_voltage);                   Serial.print(",");
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
  
  g_vehicle.init_network(DevBoard::JOULEMETER);
  timer.begin(send_vehicle_states, 10000);
}

void loop() {
  while (g_can_queue_head != g_can_queue_tail) {
    CANMessage &msg = g_can_queue[g_can_queue_head];
    Serial.print("ID: 0x");
    Serial.print(msg.id, HEX);
    Serial.print("  Data: ");
    for (int i = 0; i < msg.len; i++) {
        if (msg.buf[i] < 0x10) Serial.print("0");
        Serial.print(msg.buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    g_can_queue_head = (g_can_queue_head + 1) % CAN_QUEUE_SIZE;
  }
}
