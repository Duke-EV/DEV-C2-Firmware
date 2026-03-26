#include <Arduino.h>
#include <vehicle.hpp>

IntervalTimer timer;
unsigned long last_time = 0;
int state = 0;

void send_all_wrapper() {
  g_vehicle.send_all();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  
  g_vehicle.init_network(DevBoard::JOULEMETER);

  timer.begin(send_all_wrapper, 100000);
}

void loop() {
  unsigned long now = millis();
  if(now - last_time > 1000) {
    last_time = now;
    if(state == 0) {
      state = 1;
      g_vehicle.m_joulemeter_current = 0;
      g_vehicle.m_joulemeter_energy = 0;
      g_vehicle.m_joulemeter_voltage = 0;
    }
    else {
      state = 0;
      g_vehicle.m_joulemeter_current = 255;
      g_vehicle.m_joulemeter_energy = 255;
      g_vehicle.m_joulemeter_voltage = 255;
    }
    //Serial.println("Updating Data!!");
  }

  Serial.println("g_vehicle states");
  Serial.println(g_vehicle.m_motor_rpm);
}
