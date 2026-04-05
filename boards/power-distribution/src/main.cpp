// pdb_main.cpp
// Main program for Power Distribution Board (PDB)
// Sends: Heartbeat (power system monitoring in future)
// Receives: Emergency stop commands

#define PDB_BOARD // Power Distribution Board

#include <Arduino.h>

#include <vehicle.hpp>

IntervalTimer timer;
void send_all_wrapper()
{
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

// Define filter size (e.g., average of last 10 readings)
#define FILTER_SIZE 10

// Global or static arrays to store history
int current_history[FILTER_SIZE] = {0};
int vout_history[FILTER_SIZE] = {0};
int filter_idx = 0;

void setup()
{
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
  digitalWrite(LED, LOW);

  // relays pins
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  digitalWrite(relay1, LOW);
  digitalWrite(relay2, HIGH);

  // motor enable signal off
  g_vehicle.m_pdb_motor_enabled = 0;

  Serial.println("CAN bus initialized at 500 kbps");
  Serial.println("PDB ready - Power system OK");
  Serial.println();
}

void loop()
{
  if (timer_ms > 100)
  {
    // 1. Read Raw Values
    int raw_current = analogRead(A9);
    int raw_vout = analogRead(A13);

    // 2. Update Moving Average Buffers
    current_history[filter_idx] = raw_current;
    vout_history[filter_idx] = raw_vout;
    filter_idx = (filter_idx + 1) % FILTER_SIZE;

    // 3. Calculate Averages
    long current_sum = 0;
    long vout_sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++)
    {
      current_sum += current_history[i];
      vout_sum += vout_history[i];
    }
    int avg_current_raw = current_sum / FILTER_SIZE;
    int avg_vout_raw = vout_sum / FILTER_SIZE;

    // 4. Current Calculations (using averaged raw value)
    current_read_mV = (avg_current_raw * 3300) / 4095;
    current_read_mA = (1000 * (2500 - current_read_mV)) / 40;
    g_vehicle.m_pdb_current = current_read_mA;

    // 5. Voltage Calculations (using averaged raw value)
    vout_read_mV = (avg_vout_raw * 3300) / 4095;
    // Optimization: (156/10) is 15.6. To keep precision without floats:
    vout_scaled_mV = (vout_read_mV * 156) / 10;
    g_vehicle.m_pdb_voltage = vout_scaled_mV;

    timer_ms = 0;
  }
  if (precharge_timer_ms > 2000 && vout_scaled_mV > 45000)
  {
    digitalWrite(LED, HIGH);
    digitalWrite(relay1, HIGH);

    if (!g_vehicle.m_pdb_motor_enabled)
    {
      g_vehicle.m_pdb_motor_enabled = 1;
    }
  }
}

// m_motor_enable