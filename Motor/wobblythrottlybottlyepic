/**
 * @author benjamin wang
 * written to send signal from throttle board to the actual motor to control the motor.
 */

#include <Arduino.h>

const int ENABLE = 40; // enable pin
const int SIGNALIN = 38; // signal in (from 0 to ~4.8*(3/5)=2.88v)
/**
 * the 4.8 volts was measured with the multimeter
 * the 3/5 is from the voltage divider of (33k/(33k+22k))
 */
const int A8TX = 22; // tx com bus pin
const int A9RX = 23; // rx com bus pin

// forward declarations


void setup() {
  pinMode(ENABLE, INPUT);
  pinMode(SIGNALIN, INPUT);
  pinMode(A9RX, INPUT);
  pinMode(A8TX, OUTPUT);
}

void loop() {
  int signal = analogRead(SIGNALIN);
  uint8_t out = signal >> 2;
  int enable = analogRead(ENABLE); // todo::: check whether enable sends a digital or analog signal
  
  if (enable > 20) {
    /**
     * so the goal for this function is to send through the thing some UART protocol signal of numbers from 0 to 255 so that the motor knows how much it should go
     * 
     * !! need to figure out UART 
     */
  }

}

// functions
