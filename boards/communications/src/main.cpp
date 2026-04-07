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
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   // 358 bytes V19 
  { 255,0,0,26,0,95,1,19,0,0,0,68,69,86,95,68,105,97,103,0,
  16,1,200,84,1,1,24,0,129,144,63,24,5,64,6,86,111,108,116,97,
  103,101,40,86,41,0,67,170,62,28,7,77,31,6,3,129,142,74,24,5,
  64,6,67,117,114,114,101,110,116,40,65,41,0,67,170,73,28,7,77,31,
  6,3,129,88,63,17,5,64,6,84,104,114,82,97,119,0,67,107,62,31,
  7,85,31,6,129,89,74,16,5,64,6,84,104,114,65,118,103,0,67,107,
  73,31,7,85,31,6,70,33,58,7,7,16,24,6,0,129,13,59,18,4,
  64,6,77,95,69,110,97,98,108,101,0,70,76,76,7,7,16,24,6,0,
  129,58,77,15,4,64,6,72,97,122,97,114,100,115,0,129,7,38,0,5,
  64,6,0,70,33,67,7,7,16,24,6,0,129,23,68,7,4,64,6,76,
  101,102,116,0,70,76,58,7,7,16,24,6,0,129,62,59,10,4,64,6,
  82,105,103,104,116,0,70,33,76,7,7,16,24,6,0,129,17,77,15,4,
  64,6,82,117,110,110,105,110,103,0,70,76,67,7,7,16,24,6,0,129,
  62,68,11,4,64,6,66,114,97,107,101,0,71,11,0,70,70,56,0,187,
  8,135,0,0,0,0,0,0,32,66,0,0,32,65,0,0,32,65,0,0,
  0,64,24,107,112,104,0,71,129,1,68,68,56,0,187,24,135,0,0,0,
  0,0,0,200,67,0,0,200,66,0,0,160,65,0,0,160,65,24,114,112,
  109,0,129,79,28,51,10,192,202,68,69,86,66,79,65,82,68,0 };
  
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