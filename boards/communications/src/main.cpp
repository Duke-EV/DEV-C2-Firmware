#include <Arduino.h>
#include <vehicle.hpp>
#include <SD.h>
#include <Ticker.h>

//////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

// you can enable debug logging to Serial at 115200
//#define REMOTEXY__DEBUGLOG    

// RemoteXY select connection mode and include library 
#define REMOTEXY_MODE__ESP32CORE_BLE

#include <BLEDevice.h>

// RemoteXY connection settings 
#define REMOTEXY_BLUETOOTH_NAME "DEV_Vehicle"
#define REMOTEXY_ACCESS_PASSWORD "DEV4Life"


#include <RemoteXY.h>

// RemoteXY GUI configuration  
#pragma pack(push, 1)  
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   // 318 bytes V19 
  { 255,0,0,24,0,55,1,19,0,0,0,68,69,86,95,68,105,97,103,0,
  16,1,106,200,1,1,25,0,129,2,12,56,11,64,6,83,112,101,101,100,
  40,107,112,104,41,0,67,60,12,45,11,77,31,6,3,129,2,25,24,11,
  64,6,82,80,77,0,67,60,25,45,11,85,31,6,129,2,38,53,11,64,
  6,86,111,108,116,97,103,101,40,86,41,0,67,60,38,45,11,77,31,6,
  3,129,2,51,51,11,64,6,67,117,114,114,101,110,116,40,65,41,0,67,
  60,51,45,11,77,31,6,3,129,2,64,38,11,64,6,84,104,114,82,97,
  119,0,67,60,64,45,11,85,31,6,129,2,77,35,11,64,6,84,104,114,
  65,118,103,0,67,60,77,45,11,85,31,6,70,40,124,10,10,16,27,6,
  0,129,2,126,35,8,64,6,77,95,69,110,97,98,108,101,0,70,91,124,
  10,10,16,27,6,0,129,53,126,30,8,64,6,72,97,122,97,114,100,115,
  0,129,2,90,3,11,64,6,0,70,40,135,10,10,16,27,6,0,129,2,
  137,14,8,64,6,76,101,102,116,0,70,91,135,10,10,16,27,6,0,129,
  53,137,19,8,64,6,82,105,103,104,116,0,70,40,147,10,10,16,27,6,
  0,129,2,149,30,8,64,6,82,117,110,110,105,110,103,0,70,91,147,10,
  10,16,27,6,0,129,53,149,21,8,64,6,66,114,97,107,101,0 };
  
// this structure defines all the variables and events of your control interface 
struct {

    // output variables
  float app_kph;
  int16_t add_rpm; // -32768 .. +32767
  float app_voltage;
  float app_current;
  int16_t app_throttleR; // -32768 .. +32767
  int16_t app_throttleA; // -32768 .. +32767
  uint8_t app_motorE; // from 0 to 1
  uint8_t app_hazards; // from 0 to 1
  uint8_t app_left; // from 0 to 1
  uint8_t app_right; // from 0 to 1
  uint8_t app_running; // from 0 to 1
  uint8_t app_brake; // from 0 to 1

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0

} RemoteXY;   
#pragma pack(pop)
 
/////////////////////////////////////////////
//           END RemoteXY include          //
/////////////////////////////////////////////


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

File dataFile;
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
  File countFile = SD.open("/count.txt", FILE_READ);
  if (countFile) {
    count = countFile.parseInt();
    countFile.close();
  }

  // Increment and save it back
  count++;
  SD.remove("/count.txt");
  countFile = SD.open("/count.txt", FILE_WRITE);
  countFile.println(count);
  countFile.close();

  char filename[16];
  snprintf(filename, 16, "/log%d.csv", count);

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

    RemoteXY.app_kph        = (float)spd;
    RemoteXY.app_voltage      = (float)(g_vehicle.m_pdb_voltage/1000.0);
    RemoteXY.app_current      = (float)(g_vehicle.m_pdb_current/1000.0);
    RemoteXY.app_throttleR = (int16_t)g_vehicle.m_throttle_raw;
    RemoteXY.app_throttleA = (int16_t)g_vehicle.m_throttle_average;
    RemoteXY.app_motorE   = (uint8_t)g_vehicle.m_pdb_motor_enabled;
    RemoteXY.app_hazards   = (uint8_t)hazard;
    RemoteXY.app_left      = (uint8_t)leftTurn;
    RemoteXY.app_right     = (uint8_t)rightTurn;
    RemoteXY.app_running   = (uint8_t)(g_vehicle.m_peripherals_backrunninglights);
    RemoteXY.app_brake     = (uint8_t)brakeLights;
  }
}