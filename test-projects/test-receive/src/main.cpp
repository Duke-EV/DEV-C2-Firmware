#include <Arduino.h>
#include <vehicle.hpp>

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  
  g_vehicle.init_network(DevBoard::JOULEMETER);
}

void loop() {
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

  if (g_new_message) {
    g_new_message = false;

    Serial.print("ID: 0x");
    Serial.print(g_recent_id, HEX);
    Serial.print("  Data: ");
    for (int i = 0; i < 8; i++) {
      if (g_recent_data[i] < 0x10) Serial.print("0");
      Serial.print(g_recent_data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}
