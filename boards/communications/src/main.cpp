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
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   // 206 bytes V19 
  { 255,0,0,16,0,199,0,19,0,0,0,68,69,86,95,68,105,97,103,0,
  16,2,106,200,200,84,1,1,9,0,129,2,38,53,11,111,45,43,9,64,
  188,86,111,108,116,97,103,101,40,86,41,0,67,60,38,45,11,161,42,31,
  13,77,31,6,3,129,2,51,51,11,111,65,42,9,64,188,67,117,114,114,
  101,110,116,40,65,41,0,67,60,51,45,11,161,64,31,13,77,31,6,3,
  129,2,90,3,11,7,38,0,5,64,6,0,71,231,254,71,71,1,2,95,
  95,56,0,187,8,135,0,0,0,0,0,0,32,66,0,0,32,65,0,0,
  32,65,0,0,0,64,24,107,112,104,0,129,19,50,71,29,110,3,77,15,
  192,202,68,69,86,66,79,65,82,68,0,129,48,105,71,29,109,25,45,9,
  64,188,87,97,116,116,97,103,101,40,74,41,0,67,71,83,21,24,161,22,
  31,13,77,16,203,3 };
  
// this structure defines all the variables and events of your control interface 
struct {

    // output variables
  float app_voltage;
  float app_current;
  float app_kph; // from 0 to 40
  float app_wattage;

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
    analogWrite(RL, 20);
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
    RemoteXY.app_wattage = (float)(g_vehicle.m_pdb_voltage/1000.0) * (float)(g_vehicle.m_pdb_current/1000.0);
  }
}