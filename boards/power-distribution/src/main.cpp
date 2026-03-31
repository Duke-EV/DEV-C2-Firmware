// pdb_main.cpp
// Main program for Power Distribution Board (PDB)
// Sends: Heartbeat (power system monitoring in future)
// Receives: Emergency stop commands

#define PDB_BOARD  // Power Distribution Board

#include <Arduino.h>

#include <vehicle.hpp>

IntervalTimer timer;
void send_all_wrapper() {
  g_vehicle.send_all();
}

// PDB state (placeholder for future power monitoring)
bool powerSystemOk = true;

// LED setup 
const int LED = 13;
bool ledState = false;

// relay pins
const int relay1 = 29;
const int relay2 = 28;

elapsedMillis timer_ms;
elapsedMillis precharge_timer_ms;

// variables
uint16_t current_read_mV;
uint16_t current_read_mA;
uint16_t vout_read_mV;
uint16_t vout_scaled_mV;


void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("========================================");
  Serial.println("  Power Distribution Board Starting");
  Serial.println("========================================");
  Serial.println();

  // init CAN stuff
  g_vehicle.init_network(DevBoard::POWER_DISTRIBUTION);
  timer.begin(send_all_wrapper, 100000); // Send on a 100 ms timer. Update if needed.

  // init analog IO
  analogReadResolution(12);
  analogReadAveraging(16);

  // LED 
  pinMode(LED, OUTPUT);

  // relays pins
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  digitalWrite(relay1, LOW);
  digitalWrite(relay2, HIGH);
  
  Serial.println("CAN bus initialized at 500 kbps");
  Serial.println("PDB ready - Power system OK");
  Serial.println();
}

void loop() {
  if(timer_ms > 500) { // Toggle LED every 500 ms
    ledState = !ledState;
    digitalWrite(LED, ledState);
    timer_ms = 0; // reset timer
  }

  if(timer_ms > 100) {
    
    // current sensor read
    int current_read_raw = analogRead(A9);
    current_read_mV = (current_read_raw * 3300) / 4095; // Convert to mA, with 3.3V reference and 12-bit ADC
    // current sensor sensitivity is 40 mV/A = 0.04 mV/mA, 2.5V is 0A, as current draw increases, voltage decreases
    current_read_mA = (1000 * (2500 - current_read_mV)) / 40; // Convert to mA, with 2.5V offset and 40 mV/A sensitivity
    g_vehicle.m_pdb_current = current_read_mA;

    // vout read
    int vout_read_raw = analogRead(A13);
    vout_read_mV = (vout_read_raw * 3300) / 4095; // Convert to mV, with 3.3V reference and 12-bit ADC
    vout_scaled_mV = (vout_read_mV/10) * 156; // Convert to mV, with reverse voltage divider (divider ratio is 15.666, but since no floats, divide vout_read_mV by 10 and multiply by 156 to get better result)
    g_vehicle.m_pdb_voltage = vout_scaled_mV;
  }
  if(precharge_timer_ms > 2000 && vout_scaled_mV > 45000) {
    digitalWrite(relay1, HIGH);
  }
}
