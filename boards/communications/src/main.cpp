#include <Arduino.h>
#include <vehicle.hpp>
#include <Ticker.h>

#define REMOTEXY_MODE__ESP32CORE_BLE
#include <BLEDevice.h>

#define REMOTEXY_BLUETOOTH_NAME "DEV"

#include <RemoteXY.h>

#pragma pack(push, 1)  
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   // 154 bytes V19 
  { 255,0,0,10,0,147,0,19,0,0,0,0,31,1,106,200,1,1,9,0,
  71,15,4,80,80,56,16,2,24,135,0,0,0,0,0,0,200,66,0,0,
  160,65,0,0,32,65,0,0,0,64,24,0,67,58,82,40,10,86,2,26,
  67,58,102,40,10,86,2,26,67,58,123,40,10,86,2,26,67,58,144,40,
  10,86,2,26,129,8,82,41,12,64,17,86,111,108,116,97,103,101,0,129,
  8,101,40,12,64,17,67,117,114,114,101,110,116,0,129,2,124,53,9,64,
  17,84,104,114,111,116,116,108,101,95,82,97,119,0,129,4,144,51,9,64,
  17,84,104,114,111,116,116,108,101,95,65,118,103,0 };

struct {
  int16_t speed;          // gauge, 0 to 100
  int16_t Voltage_PDB;
  int16_t Current_PDB;
  int16_t Throttle_raw;
  int16_t Throttle_Avg;
  uint8_t connect_flag;
} RemoteXY;   
#pragma pack(pop)

unsigned long previousMillis = 0;
const long interval = 500;

#define RL 22
#define HL 16
#define HR 17

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

void send_all(void *args) {
  while (true) {
    g_vehicle.send_all();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

bool hazard = false;
bool leftTurn = false;
bool rightTurn = false;
bool backrunningLights = false;
bool brakeLights = false;

void setup() {
  RemoteXY_Init();

  g_vehicle.init_network(DevBoard::COMMUNICATIONS);
  xTaskCreate(send_all, "Sending", 4096, nullptr, 5, nullptr);

  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(HR, OUTPUT);
  pinMode(HL, OUTPUT);
  pinMode(RL, OUTPUT);
}

void readStates() {
  backrunningLights = (g_vehicle.m_peripherals_backrunninglights == 1);
  brakeLights       = (g_vehicle.m_peripherals_brakelights == 1);
  hazard            = (g_vehicle.m_peripherals_hazard == 1);
  rightTurn         = (g_vehicle.m_peripherals_turn == 2);
  leftTurn          = (g_vehicle.m_peripherals_turn == 1);
}

void loop() {
  RemoteXY_Handler();
  readStates();

  if (hazard || leftTurn) {
    digitalWrite(HL, HIGH);
  } else {
    digitalWrite(HL, LOW);
  }

  if (hazard || rightTurn) {
    digitalWrite(HR, HIGH);
  } else {
    digitalWrite(HR, LOW);
  }

  if (brakeLights) {
    analogWrite(RL, 255);
  } else if (backrunningLights) {
    analogWrite(RL, 50);
  } else {
    analogWrite(RL, 0);
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    float spd = (3.14 * g_vehicle.m_motor_rpm * 0.58 * 60) / 1000.0;

    RemoteXY.speed        = (int16_t)spd;
    RemoteXY.Voltage_PDB  = g_vehicle.m_pdb_voltage;
    RemoteXY.Current_PDB  = g_vehicle.m_pdb_current;
    RemoteXY.Throttle_raw = g_vehicle.m_throttle_raw;
    RemoteXY.Throttle_Avg = g_vehicle.m_throttle_average;
  }
}