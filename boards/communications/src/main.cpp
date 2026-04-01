#include <Arduino.h>
#include <vehicle.hpp>
#include <SD.h>
#include <Ticker.h>

#define RL 22 //red light
#define HL 16 //hazard left
#define HR 17 //hazard right 

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

void send_all(void *args) {
  while(true) {
    g_vehicle.send_all();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

bool hazard = false;
bool leftTurn = false;
bool rightTurn = false;
bool backrunningLights = false;
bool brakeLights = false;

unsigned long lastLogTime = 0;
bool sdReady = false;


void setup() {
  g_vehicle.init_network(DevBoard::COMMUNICATIONS);
  xTaskCreate(send_all, "Sending", 4096, nullptr, 5, nullptr);

  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(HR, OUTPUT);
  pinMode(HL, OUTPUT);
  pinMode(RL, OUTPUT);

}

void readStates() {
  if (g_vehicle.m_peripherals_backrunninglights == 1) {
    backrunningLights = true;
  } else {
    backrunningLights = false;
  }

  if(g_vehicle.m_peripherals_brakelights == 1) {
    brakeLights = true;
  } else {
    brakeLights = false;
  }

  if(g_vehicle.m_peripherals_hazard == 1) {
    hazard = true;
  } else {
    hazard = false;
  }

  if(g_vehicle.m_peripherals_turn == 2) {
    rightTurn = true;
  } else {
    rightTurn = false;
  }

  if(g_vehicle.m_peripherals_turn == 1) {
    leftTurn = true;
  } else {
    leftTurn = false;
  }

  
}


void printState() {
  //delay(1000);
  Serial.print("Voltage: ");
  Serial.println(g_vehicle.m_pdb_voltage);
  Serial.print("Current: ");
  Serial.println(g_vehicle.m_pdb_current);
  Serial.print("RPM: ");
  Serial.println(g_vehicle.m_motor_rpm);
  Serial.print("Throttle Avg: ");
  Serial.println(g_vehicle.m_throttle_average);
  Serial.print("Throttle Raw: ");
  Serial.println(g_vehicle.m_throttle_raw);
  
}

void loop() {
  // TODO: rest of device
  readStates();

  if(hazard || leftTurn){
    digitalWrite(HL, HIGH);
  }

  if(hazard || rightTurn){
    digitalWrite(HR, HIGH);
  }

  if(backrunningLights) {
    analogWrite(RL,50);
  } 

  if(brakeLights) {
    analogWrite(RL, 255);
  } 

  printState();

}
