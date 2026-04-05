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

int CURRENT_CAP = 17000;
int CURRENT_HARD_CAP = 19800; // if current is above this value, do a hard reset by rapidly deacceleration the motor and resetting the moving average. This is to prevent damage to the hardware in case of a fault. The value was chosen based on testing and is above the normal operating current of the system, but below the level that caused damage during testing.

const int ENABLE = 6;
int WINDOW_SIZE = 500; // number of windows to do the moving average by. 
// time to reach a pressed value is (WINDOW_SIZE / * 10) milliseconds 

volatile bool disable_throttle = false; 


uint8_t last_throttle_output = 0;
int curr_throttle_value = 0;

int signal = 0;
int scaled_throttle_output = 0;
int smoothed_output = 0;
void slowDeacceleration();

int throttle_cap = 255;

int flag = 0; // so that it doesn't keep updating the throttle cap when the current is above 17000

//void disableThrottleISR();

struct MovingAverage {
  std::deque<int> samples;
  unsigned int window;
  int sum = 0;

  MovingAverage(int w) {
    window = w;
  }

  int next (int signal) {
    if (samples.size() == window) {
      sum -= samples.front();
      samples.pop_front();
    }

    sum+=signal;
    samples.push_back(signal);
    return sum / samples.size();
  }
};

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
}

MovingAverage ma{WINDOW_SIZE};

void loop() {
    /*
    if(disable_throttle == true) {
      signal = 0;
      scaled_throttle_output = 0;
      g_vehicle.m_throttle_average = 0;
      g_vehicle.m_throttle_raw = 0;
    } 
    */

    if(g_vehicle.m_pdb_current >= CURRENT_HARD_CAP) { // if current is this high something is wrong so do a hard reset.
      slowDeacceleration();

      for (int i = 0; i < WINDOW_SIZE; i++){
        ma.next(0);
      }
    }

    int nstgbtpo = 7; // num_samples_to_go_back_to_prevent_overshoot, time is value * 10 in ms
    if (g_vehicle.m_pdb_current >=  CURRENT_CAP && flag == 0) {
      throttle_cap = ma.samples.size() > nstgbtpo ? ma.samples[ma.samples.size() - nstgbtpo] : 0; // get the throttle value 
      flag = 1;
      digitalWrite(LED_BUILTIN, LOW);
    }

    if (g_vehicle.m_pdb_current <  CURRENT_CAP && flag == 1) {
      flag = 0; // the cap should only be reset on instants where you just exceeded the current cap
      digitalWrite(LED_BUILTIN, HIGH);
    }

    signal = analogRead(SIGNALIN);
    scaled_throttle_output = std::min({(int)(signal * 3.1 / 2.8) >> 2, 255}); //divide to go from 1024 to 256

    if (scaled_throttle_output > throttle_cap) {
      scaled_throttle_output = throttle_cap;
      Serial.println("throttle capped at: " + String(throttle_cap));
      Serial.println("current: " + String(g_vehicle.m_pdb_current));
    }

    if(throttle_cap==0) {
      throttle_cap = 255; // if the throttle cap is 0, it means that we haven't had enough samples to fill the moving average window, so we should just set the cap to 255 to prevent blocking the throttle. This is a temporary solution and can be improved by having a more robust way of handling the initial state of the system.
    }

    smoothed_output = ma.next(scaled_throttle_output);

    delay(4);

    g_vehicle.m_throttle_raw = (uint16_t) signal; // raw throttle value



    if (scaled_throttle_output < smoothed_output) {
      smoothed_output = scaled_throttle_output;
    }


    // gradually step up the throttle value until we reach the target, but check for disable throttle 

    g_vehicle.m_throttle_average = (uint16_t) smoothed_output; // moving average, scaled to unit_8 range
       
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
    g_vehicle.m_throttle_average = ((g_vehicle.m_throttle_average - 5) > 0) ? (g_vehicle.m_throttle_average-5) : 0;
    delay(10);
  }
  digitalWrite(LED_BUILTIN, HIGH);

}
