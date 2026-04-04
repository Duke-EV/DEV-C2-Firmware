#include <Arduino.h>
#include <vehicle.hpp>
#include <SD.h>
#include <Ticker.h>

#define REMOTEXY_MODE__ESP32CORE_BLE
#include <BLEDevice.h>

#define REMOTEXY_BLUETOOTH_NAME "DEV-Raw"
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
#define SD_CARD_SLOT 5

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

void send_all(void *args) {
  while (true) {
    g_vehicle.send_all();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void write_to_sd(void *args) {
  while(true) {
    dataFile.print(millis());
    dataFile.print(",");
    dataFile.print(g_vehicle.m_motor_rpm);
    dataFile.print(",");
    dataFile.print(g_vehicle.m_pdb_current);
    dataFile.print(",");
    dataFile.print(g_vehicle.m_throttle_average);
    dataFile.print(",");
    dataFile.println(g_vehicle.m_throttle_raw);  // println only on the last one

    dataFile.flush();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

File dataFile;
bool hazard = false;
bool leftTurn = false;
bool rightTurn = false;
bool backrunningLights = false;
bool brakeLights = false;

void setup() {
  RemoteXY_Init();
  SD.begin(SD_CARD_SLOT);

  g_vehicle.init_network(DevBoard::COMMUNICATIONS);
  xTaskCreate(send_all, "Sending", 4096, nullptr, 5, nullptr);

  Serial.begin(115200);

  int count = 0;
  File countFile = SD.open("count.txt", FILE_READ);
  if (countFile) {
    count = countFile.parseInt();
    countFile.close();
  }

  // Increment and save it back
  count++;
  SD.remove("count.txt");
  countFile = SD.open("count.txt", FILE_WRITE);
  countFile.println(count);
  countFile.close();

  char filename[16];
  snprintf(filename, 16, "log%d.csv", count);

  dataFile = SD.open(filename, FILE_WRITE);
  Serial.print("Logging to: ");
  Serial.println(filename);

  dataFile.println("timestamp_ms,rpm,current,throttle_avg,throttle_raw");

  xTaskCreate(write_to_sd, "Writing to SD", 4096, nullptr, 5, nullptr);

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