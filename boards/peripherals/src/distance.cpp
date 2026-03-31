#include <Arduino.h>

const int TRIG_PIN = 7;
const int ECHO_PIN = 8;

// Anything over 400 cm (23200 us pulse) is "out of range"
const unsigned int MAX_DIST = 400;

void setup() {

  // The Trigger pin will tell the sensor to range find
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);

  //Set Echo pin as input to measure the duration of 
  //pulses coming back from the distance sensor
  pinMode(ECHO_PIN, INPUT);

  // We'll use the serial monitor to view the sensor output
  Serial.begin(9600);
}

void loop() {
    int cm = measure_distance();
    // Print out results

    if (cm > MAX_DIST){
        Serial.println("Out of range");
    }
    else {
        Serial.println(cm + " cm");
    }

    // Wait at least 60ms before next measurement
    delay(60);
}

float measure_distance() {
    unsigned long t1;
    unsigned long t2;
    unsigned long pulse_width;
    float cm;

    // Hold the trigger pin high for at least 10 us
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Wait for pulse on echo pin
    while (digitalRead(ECHO_PIN) == 0);

    // Measure how long the echo pin was held high (pulse width)
    // Note: the micros() counter will overflow after ~70 min
    t1 = micros();
    while (digitalRead(ECHO_PIN) == 1);
    t2 = micros();
    pulse_width = t2 - t1;

    // Calculate distance in centimeters. Calculated from the
    // assumed speed of sound in air at sea level (~340 m/s).
    cm = pulse_width / 58.0;

    return cm;
}


//4ft of wire