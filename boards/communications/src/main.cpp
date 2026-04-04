#include <Arduino.h>
#include <vehicle.hpp>
#include <Ticker.h>

#define REMOTEXY_MODE__ESP32CORE_BLE
#include <BLEDevice.h>

#define REMOTEXY_BLUETOOTH_NAME "DEV-ESP32"

#include <RemoteXY.h>

#pragma pack(push, 1)  
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   // 343 bytes V19 
  { 255,0,0,25,0,80,1,19,0,0,0,0,31,1,106,200,1,1,24,0,
  71,252,5,55,55,56,0,2,24,174,0,0,0,0,0,0,112,66,0,0,
  160,65,0,0,32,65,0,0,0,64,24,0,71,46,7,59,59,56,0,2,
  24,135,0,0,0,0,0,0,200,66,0,0,160,65,0,0,32,65,0,0,
  0,64,24,0,70,21,131,10,10,16,26,37,0,129,4,134,13,5,64,17,
  66,114,97,107,101,0,129,4,147,11,5,64,17,66,97,99,107,0,129,4,
  159,16,5,64,17,72,97,122,97,114,100,0,70,22,144,10,10,16,26,37,
  0,70,22,157,10,10,16,26,37,0,70,22,171,10,10,16,26,37,0,70,
  22,186,10,10,16,26,37,0,129,5,173,11,6,64,17,76,101,102,116,0,
  129,4,188,14,6,64,17,82,105,103,104,116,0,67,68,132,32,12,86,2,
  26,129,45,134,20,7,64,17,83,112,101,101,100,0,67,68,148,32,10,86,
  2,26,67,68,163,32,10,86,2,26,67,68,177,33,10,86,2,26,129,39,
  150,27,5,64,17,116,104,114,111,116,116,108,101,95,97,118,103,0,129,34,
  164,32,6,64,17,116,104,114,111,116,116,108,101,95,114,97,119,0,129,52,
  179,13,6,64,17,82,80,77,0,67,68,62,32,6,86,2,26,67,69,72,
  31,6,86,2,26,129,25,61,38,7,64,17,80,68,66,32,118,111,108,116,
  97,103,101,0,129,25,72,34,6,64,17,80,68,66,95,99,117,114,114,101,
  110,116,0 };

struct {
  float speed_display;
  float rpm_display;
  uint8_t brake;
  uint8_t led_01;
  uint8_t led_02;
  uint8_t led_03;
  uint8_t led_04;
  int16_t speed;
  int16_t throttle_avg;
  int16_t throttle_raw;
  int16_t value_01;
  int16_t pdb_voltage;
  int16_t pdb_current;
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

void printState() {
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

    RemoteXY.speed_display = spd;
    RemoteXY.rpm_display   = g_vehicle.m_motor_rpm;

    RemoteXY.brake  = brakeLights ? 1 : 0;
    RemoteXY.led_01 = backrunningLights ? 1 : 0;
    RemoteXY.led_02 = hazard ? 1 : 0;
    RemoteXY.led_03 = leftTurn ? 1 : 0;
    RemoteXY.led_04 = rightTurn ? 1 : 0;

    RemoteXY.speed        = (int16_t)spd;
    RemoteXY.throttle_avg = g_vehicle.m_throttle_average;
    RemoteXY.throttle_raw = g_vehicle.m_throttle_raw;
    RemoteXY.value_01     = g_vehicle.m_motor_rpm;
    RemoteXY.pdb_voltage  = g_vehicle.m_pdb_voltage;
    RemoteXY.pdb_current  = g_vehicle.m_pdb_current;
  }
}