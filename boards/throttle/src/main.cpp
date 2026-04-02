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

const int DELAY_MS = 67;

const int BAUD = 9600;

const int ENABLE = 6;
const unsigned int WINDOW_SIZE = 100; // number of windows to do the moving average by

volatile bool disable_throttle = false; 
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 50; // ms
// uint8_t throttle_output = 0;
int long_throttle_output = 0;
uint8_t last_throttle_output = 0;
int curr_throttle_value = 0; // variable that checks if original throttle value changed by plus or minus 10 percent 
int desired_rpm = 0; 
int curr_rpm = 0; 
int MAX_RPM = 2000; 
float integral = 0.0;
int duty_cycle = 0; 
float k_p = 0.2; // current error 
float k_i = 0.4; // long-term error
int output_duty_cycle = 0; 
float Ts = 0.01;
int signal = 0;
int scaled_throttle_output = 0;
void disableThrottleISR();
int smoothed_output = 0;

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
  timer.begin(send_all_wrapper, 100000);

  pinMode(ENABLE, INPUT);
  pinMode(SIGNALIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(6), disableThrottleISR, RISING);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(BAUD);
}

MovingAverage ma{WINDOW_SIZE};

void loop() {

  if(disable_throttle) {
    //disable_throttle = false; --> WHEN TO RE-ENABLE THE THROTTLE SIGNAL?
    signal = 0;
  } else {
    signal = analogRead(SIGNALIN);
    scaled_throttle_output = std::min({(int)(signal * 3.1 / 2.8) >> 2, 255}); //divide to go from 1024 to 256
    // long_throttle_output = (int)(signal * 3.1 / 2.8) >> 2; // dont send a uint_8 bc overflow sucks.

    Serial.print("Signal: ");
    Serial.println(signal);
    Serial.print("Scaled throttle: ");
    Serial.println(scaled_throttle_output);
    // Serial.print("duty cycle output: ");
    // Serial.println(output_duty_cycle);
  }


  if(disable_throttle) {
    scaled_throttle_output = 0;
  }
  
  smoothed_output = ma.next(scaled_throttle_output);

  last_throttle_output =  curr_throttle_value;

  delay(10);

  // put whatever we send into final_number.
  // int final_number = long_throttle_output;
  g_vehicle.m_throttle_raw = (uint16_t) signal; // raw throttle value
  if (scaled_throttle_output < smoothed_output) {
    smoothed_output = scaled_throttle_output;
  }

  g_vehicle.m_throttle_average = (uint16_t) smoothed_output; // moving average, scaled to unit_8 range
  Serial.print("Averaged Throttle with brake correction: ");
  Serial.println(smoothed_output);
}

void disableThrottleISR() {
  unsigned long now = millis();
  if (now - lastInterruptTime > debounceDelay) {
    disable_throttle = true;
    lastInterruptTime = now;
  }
}