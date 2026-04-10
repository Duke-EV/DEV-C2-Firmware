/**
 * this board sends:
 * raw signal (from 0 to something at some value less than 1024, approx at 1024*3/3.3)
 * scaled moving average in unit_8 range, can modify window size in WINDOW_SIZE
 */

#include <Arduino.h>
#include <bitset>
#include <vehicle.hpp>
#include <algorithm>
#include <deque>


const int SIGNALIN = 24; // signal in (from 0 to ~4.8*(3/5)=2.88v)
/**
 * the 4.8 volts was measured with the multimeter
 * the 3/5 is from the voltage divider of (33k/(33k+22k))
 */


const int BAUD = 9600;

int CURRENT_CAP = 15000; // mV
int CURRENT_HARD_CAP = 18000; // if current is above this value, do a hard reset by rapidly deacceleration the motor and resetting the moving average. This is to prevent damage to the hardware in case of a fault. The value was chosen based on testing and is above the normal operating current of the system, but below the level that caused damage during testing.

const int ENABLE = 6;
int WINDOW_SIZE = 500; // number of windows to do the moving average by. 
// time to reach a pressed value is (WINDOW_SIZE / * 10) milliseconds 

volatile bool disable_throttle = false; 


uint8_t last_throttle_output = 0;
int curr_throttle_value = 0;

int signal = 0;
int scaled_throttle_output = 0;
//int smoothed_output = 0;
void slowDeacceleration();

int THROTTLE_CAP = 255;

int flag = 0; // so that it doesn't keep updating the throttle cap when the current is above 17000

// throttle limiting
#define POS_STEP_SIZE 0.1
#define NEG_STEP_SIZE 0.5
#define SIGNAL_ARRAY_LENGTH 3
//#define THROTTLE_CAP 230
int signal_array[SIGNAL_ARRAY_LENGTH];
float esc_throttle_input = 0;
float delta_throttle = 0;
int max_signal_idx;
int min_signal_idx;
int max_signal;
int min_signal;
// last throttle value - in g vehicle
// if new < old, set trhottle to new
// if recorded step size > STEP_SIZE, then set the step size to the macro

//void disableThrottleISR();

IntervalTimer timer;
void send_all_wrapper() {
  g_vehicle.send_all();
}

void setup() {
  g_vehicle.init_network(DevBoard::THROTTLE);
  timer.begin(send_all_wrapper, 10000); //100Hz

  //pinMode(ENABLE, INPUT);
  pinMode(SIGNALIN, INPUT);
  //attachInterrupt(digitalPinToInterrupt(6), disableThrottleISR, RISING);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(BAUD);

  // initialize signal array to 0
  for (int i = 0; i < SIGNAL_ARRAY_LENGTH; i++) {
    signal_array[i] = 0;
  }
}

void loop() {
    if(g_vehicle.m_pdb_current >= CURRENT_HARD_CAP) { // if current is this high something is wrong so do a hard reset.
      slowDeacceleration();  // blocking, lowers throttle to 0
    }

    if ((g_vehicle.m_pdb_current >=  CURRENT_CAP)) { // if current is above this value or is projected to be above this value next iteration, cap the throttle to prevent overshoot. This is to prevent damage to the hardware in case of a fault. The value was chosen based on testing and is above the normal operating current of the system, but below the level that caused damage during testing.
      // lower throttle by 1
      esc_throttle_input -= NEG_STEP_SIZE;
      digitalWrite(LED_BUILTIN, LOW);
      goto update_esc_input;
    }

    for(int i = 0; i < SIGNAL_ARRAY_LENGTH; i++) {
      signal = analogRead(SIGNALIN);
      int temp = std::min({(int)(signal * 3.1 / 2.8) >> 2, 255}); //divide to go from 1024 to 256
      signal_array[i] = temp;

      delayMicroseconds(100);
    }

    // find median value - **TODO: rework so works beyond 3 inputs 
    max_signal_idx = 0;
    min_signal_idx = 0;
    max_signal = 0;
    min_signal = 500;

    for (int i = 0; i < SIGNAL_ARRAY_LENGTH; i++) {
      if (signal_array[i] > max_signal) {
        max_signal = signal_array[i];
        max_signal_idx = i;
      }
      if (signal_array[i] < min_signal) {
        min_signal = signal_array[i];
        min_signal_idx = i;
      }
    }
    for (int i = 0; i < SIGNAL_ARRAY_LENGTH; i++) {
      if (i != max_signal_idx && i != min_signal_idx) {
        scaled_throttle_output = signal_array[i];
        g_vehicle.m_throttle_raw = signal_array[i];
      }
    }

    if(scaled_throttle_output < 5) {
      esc_throttle_input = 0;
      goto update_esc_input;
    }
      

    delta_throttle = scaled_throttle_output - esc_throttle_input; // m_throttle_average goes to esc

    if (delta_throttle > POS_STEP_SIZE) {
      esc_throttle_input += POS_STEP_SIZE;
    } else { // if condition above not met, then scaled_throttle_output is either negative or less than step cap
      esc_throttle_input = scaled_throttle_output;
    }
    
    update_esc_input:

    if (esc_throttle_input <= 0) {
      esc_throttle_input = 0;
    }
    if (esc_throttle_input >= THROTTLE_CAP) {
      esc_throttle_input = THROTTLE_CAP;
    }

    g_vehicle.m_throttle_average = (int) esc_throttle_input;

    Serial.print("Current: ");
    Serial.println(g_vehicle.m_pdb_current/1000.0);

    //Serial.println(g_vehicle.m_throttle_average);
    delay(10);
}

// THROTTLE DISABLE LOGIC
/*
void disableThrottleISR() {
  disable_throttle = true;
}
*/
void slowDeacceleration() {
  // every 100 ms drop the duty cycle by 50 till it hits 0 
  // should be a BLOCKING function
  digitalWrite(LED_BUILTIN, LOW);
  while(g_vehicle.m_throttle_average > 0) {
    g_vehicle.m_throttle_average = ((g_vehicle.m_throttle_average - 1) > 0) ? (g_vehicle.m_throttle_average-1) : 0;
    delay(2);
  }
  digitalWrite(LED_BUILTIN, HIGH);

  esc_throttle_input = 0;

}
