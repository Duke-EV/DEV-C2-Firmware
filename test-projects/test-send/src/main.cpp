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
  
  g_vehicle.init_network(DevBoard::MOTOR_CONTROLLER);

  timer.begin(send_all_wrapper, 100000);
}

void loop() {
  //g_vehicle.loop();
  if(millis() - last_time > 1000) {
    last_time = millis();
    if(state == 0) {
      state = 1;
      g_vehicle.m_motor_rpm = 0;
    }
    else {
      state = 0;
      g_vehicle.m_motor_rpm = 255;
    } 
  }

  Serial.println("g_vehicle states");
  Serial.println(g_vehicle.m_joulemeter_current);
  Serial.println(g_vehicle.m_joulemeter_energy);
  Serial.println(g_vehicle.m_joulemeter_voltage);
}
