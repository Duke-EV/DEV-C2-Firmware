/**
 * benjamin wang
 * written to send signal from throttle board to the actual motor to control the motor.
 */

#include <Arduino.h>
#include <bitset>

const int SIGNALIN = 24; // signal in (from 0 to ~4.8*(3/5)=2.88v)
/**
 * the 4.8 volts was measured with the multimeter
 * the 3/5 is from the voltage divider of (33k/(33k+22k))
 */
const int TX = 31; // tx com bus pin
const int RX = 30; // rx com bus pin

const int DELAY_MS = 67;

const int BAUD = 9600;

volatile bool disable_throttle = false; 
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 50; // ms
uint8_t throttle_output = 0;
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

void setup() {
  pinMode(ENABLE, INPUT);
  pinMode(SIGNALIN, INPUT);
  pinMode(RX, INPUT);
  pinMode(TX, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(6), disableThrottleISR, RISING);

  Serial.begin(BAUD);
}

void loop() {
  if(disable_throttle) {
    //disable_throttle = false; --> WHEN TO RE-ENABLE THE THROTTLE SIGNAL?
    throttle_output = 0;
  } else {
    int signal = analogRead(SIGNALIN);
    throttle_output = (int)(signal * 3.1 / 2.8) >> 2; //divide to go from 1024 to 256
    
    if ((throttle_output > 1.1 * last_throttle_output) || (throttle_output < 0.9 * last_throttle_output)) {
      curr_throttle_value = throttle_output;
    }
    // PID Controller
    desired_rpm = (curr_throttle_value/255.0)*MAX_RPM;
    int error = desired_rpm - curr_rpm;
    integral+=error*Ts;
    duty_cycle = k_p*error + k_i*integral;
    output_duty_cycle = (duty_cycle/MAX_RPM)*255.0;
    if(output_duty_cycle>255){
      output_duty_cycle = 255;
    }
    if(output_duty_cycle<0) {
      output_duty_cycle = 0;
    }

    Serial.print("Signal: ");
    Serial.println(signal);
    Serial.print("Scaled throttle: ");
    Serial.println(throttle_output);
    Serial.print("duty cycle output: ");
    Serial.println(output_duty_cycle);
  }

  last_throttle_output =  curr_throttle_value;

  delay(Ts*1000);
}

void disableThrottleISR() {
  unsigned long now = millis();
  if (now - lastInterruptTime > debounceDelay) {
    disable_throttle = true;
    lastInterruptTime = now;
  }
}