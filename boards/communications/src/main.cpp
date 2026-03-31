#include <Arduino.h>
#include <vehicle.hpp>
#include <SD.h>
#include <Ticker.h>

#define BL 21 //brake light
#define RL 22 //running light
#define HL 16 //hazard left
#define HR 17 //hazard right 

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

Ticker timer;
void send_all_wrapper() {
  g_vehicle.send_all();
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
  // use Ticker on ESP32: 100000us == 100ms -> attach_ms(100,...)
  timer.attach_ms(100, send_all_wrapper);
  // Minimal SD init: create log file header if possible
  Serial.begin(115200);

  File f = SD.open("/can_log.csv", FILE_APPEND);
  if (f) {
    f.println("timestamp_ms,id,len,b0,b1,b2,b3,b4,b5,b6,b7");
    f.close();
  }
  // Start SD card
  if (SD.begin()) {
    sdReady = true;
    Serial.println("SD card ready");

    // Only create header if file does not already exist
    if (!SD.exists("/can_state.csv")) {
      File f = SD.open("/can_state.csv", FILE_WRITE);
      if (f) {
        f.println("timestamp_ms,windshield,backrunning,turn,headlights,brakelights,hazard,pdb_current,pdb_voltage,motor_rpm,throttle_raw,throttle_avg,jm_current,jm_voltage,jm_energy");
        f.close();
      }
    }
  } else {
    Serial.println("SD card failed");
  }

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(HR, OUTPUT);
  pinMode(HL, OUTPUT);
  pinMode(BL, OUTPUT);
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
  delay(1000);
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

void logVehicleState() {
  if (!sdReady) return;

  unsigned long now = millis();
  if (now - lastLogTime < 5000) return;   // log once per 5 seconds
  lastLogTime = now;

  File f = SD.open("/can_state.csv", FILE_APPEND);
  if (!f) {
    Serial.println("Could not open file");
    return;
  }

  f.print(now);
  f.print(",");
  f.print(g_vehicle.m_peripherals_windshield);
  f.print(",");
  f.print(g_vehicle.m_peripherals_backrunninglights);
  f.print(",");
  f.print(g_vehicle.m_peripherals_turn);
  f.print(",");
  f.print(g_vehicle.m_peripherals_headlights);
  f.print(",");
  f.print(g_vehicle.m_peripherals_brakelights);
  f.print(",");
  f.print(g_vehicle.m_peripherals_hazard);
  f.print(",");
  f.print(g_vehicle.m_pdb_current);
  f.print(",");
  f.print(g_vehicle.m_pdb_voltage);
  f.print(",");
  f.print(g_vehicle.m_motor_rpm);
  f.print(",");
  f.print(g_vehicle.m_throttle_raw);
  f.print(",");
  f.print(g_vehicle.m_throttle_average);
  f.print(",");
  f.print(g_vehicle.m_joulemeter_current);
  f.print(",");
  f.print(g_vehicle.m_joulemeter_voltage);
  f.print(",");
  f.println(g_vehicle.m_joulemeter_energy);

  f.close();
  Serial.println("Logged to SD");
}


void loop() {
  // TODO: rest of device
  readStates();
  logVehicleState();

  if(millis() % 2000 >= 1000) {
    if(hazard){
      digitalWrite(HL, HIGH);
      digitalWrite(HR, HIGH);
    }
    else if(leftTurn){
      digitalWrite(HL, HIGH);
    }
    else if(rightTurn) {
      digitalWrite(HR, HIGH);
    }
  }
  else {
    digitalWrite(HL, LOW);
    digitalWrite(HR, LOW);
  }

  if(backrunningLights) {
    digitalWrite(RL, HIGH);
  } else {
    digitalWrite(RL, LOW);
  }

  if(brakeLights) {
    digitalWrite(BL, HIGH);
  } else {
    digitalWrite(BL, LOW);
  }

  printState();


}
