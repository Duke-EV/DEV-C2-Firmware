#include <Arduino.h>
#include <vehicle.hpp>
#include <Ticker.h>

#define REMOTEXY_MODE__ESP32CORE_BLE
#include <BLEDevice.h>

#define REMOTEXY_BLUETOOTH_NAME "DEV"

#include <RemoteXY.h>

#pragma pack(push, 1)  
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   // 85 bytes V19 
  { 255,0,0,12,0,78,0,19,0,0,0,0,31,1,106,200,1,1,5,0,
  71,18,6,71,71,56,16,2,24,135,0,0,0,0,0,0,200,66,0,0,
  160,65,0,0,32,65,0,0,0,64,24,0,67,33,78,40,10,86,93,201,
  67,34,107,40,10,86,2,33,67,34,133,40,10,86,2,145,67,34,160,40,
  10,78,2,229,2 };

struct {
  int16_t Speed;
  int16_t Voltage;
  int16_t Current;
  int16_t Throttle_Raw;
  float Throttle_Avg;
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

    RemoteXY.Speed        = (int16_t)spd;
    RemoteXY.Voltage      = g_vehicle.m_pdb_voltage;
    RemoteXY.Current      = g_vehicle.m_pdb_current;
    RemoteXY.Throttle_Raw = g_vehicle.m_throttle_raw;
    RemoteXY.Throttle_Avg = (float)g_vehicle.m_throttle_average;
  }
}