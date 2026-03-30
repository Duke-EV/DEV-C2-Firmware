#include <Arduino.h>
#include <vehicle.hpp>
#include <SD.h>

#define BL 21 //brake light
#define RL 22 //running light
#define HL 16 //hazard left
#define HR 17 //hazard right 

IntervalTimer timer;
void send_all_wrapper() {
  g_vehicle.send_all();
}

bool hazard = false;
bool leftTurn = false;
bool rightTurn = false;
bool backrunningLights = false;
bool brakeLights = false;


void setup() {
  g_vehicle.init_network(DevBoard::COMMUNICATIONS);
  timer.begin(send_all_wrapper, 100000);
  // Minimal SD init: create log file header if possible
  SD.begin();
  File f = SD.open("/can_log.csv", FILE_APPEND);
  if (f) {
    f.println("timestamp_ms,id,len,b0,b1,b2,b3,b4,b5,b6,b7");
    f.close();
  }
  // Create a simple state CSV for higher-level variables
  File s = SD.open("/can_state.csv", FILE_APPEND);
  if (s) {
    s.println("timestamp_ms,windshield,backrunning,turn,headlights,brakelights,hazard,pdb_current,pdb_voltage,motor_rpm,throttle_raw,throttle_avg,jm_current,jm_voltage,jm_energy");
    s.close();
  }
  // TODO: rest of device setup
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(HR, OUTPUT);
  pinMode(HL, OUTPUT);
  pinMode(BL, OUTPUT);
  pinMode(HR, OUTPUT);

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

  // have to ask about turn bc currently no differentiation between left and right

}

// periodic logger for  vehicle state (runs in loop)
static unsigned long s_last_log = 0;
void log_vehicle_state_if_due() {
  unsigned long now = millis();
  if (now - s_last_log < 1000) return; // log ~1s
  s_last_log = now;

  File f = SD.open("/can_state.csv", FILE_APPEND);
  if (!f) return;
  // timestamp and all relevant members from vehicle.hpp
  // order matches header written in setup()
  char buf[200];
  int n = snprintf(buf, sizeof(buf), "%lu,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%u,%u,%u,%u,%lu\n",
                   now,
                   (unsigned)g_vehicle.m_peripherals_windshield,
                   (unsigned)g_vehicle.m_peripherals_backrunninglights,
                   (unsigned)g_vehicle.m_peripherals_turn,
                   (unsigned)g_vehicle.m_peripherals_headlights,
                   (unsigned)g_vehicle.m_peripherals_brakelights,
                   (unsigned)g_vehicle.m_peripherals_hazard,
                   (unsigned)g_vehicle.m_pdb_current,
                   (unsigned)g_vehicle.m_pdb_voltage,
                   (unsigned long)g_vehicle.m_motor_rpm,
                   (unsigned)g_vehicle.m_throttle_raw,
                   (unsigned)g_vehicle.m_throttle_average,
                   (unsigned)g_vehicle.m_joulemeter_current,
                   (unsigned)g_vehicle.m_joulemeter_voltage,
                   (unsigned long)g_vehicle.m_joulemeter_energy);
  if (n > 0) f.write((const uint8_t*)buf, n);
  f.close();
}

void loop() {
  // TODO: rest of device
  readStates();

  if(millis() % 2000 >= 1000) {
    if(hazard){
      digitalWrite(HL, HIGH);
      digitalWrite(HR, HIGH);
    }
    else if(leftTurn){
      digitalWrite(FLH, HIGH);
    }
    else if(rightTurn) {
      digitalWrite(FRH, HIGH);
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


}
