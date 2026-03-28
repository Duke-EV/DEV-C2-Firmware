#include <Arduino.h>
#include <vehicle.hpp>

void send_all(void *args) {
  while(true) {
    g_vehicle.send_all();
    vTaskDelay(pdMS_TO_TICKS(100));
  } 
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("Starting");
  
  g_vehicle.init_network(DevBoard::JOULEMETER);
  g_vehicle.m_joulemeter_voltage = 100;
  xTaskCreate(send_all, "Sending", 4096, nullptr, 5, nullptr);
}

unsigned long last_time = 0;
void loop() {
  unsigned long now =millis();
  if (now - last_time >= 1000) {
    last_time = millis();
    g_vehicle.m_joulemeter_current += 1;
  }

  Serial.println(g_vehicle.m_motor_rpm);
}