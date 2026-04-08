#include <Arduino.h>
#include <vehicle.hpp>
#include <queue>
#include <vector>

#define Windshield 33 //D33: windshield wipers
#define HL 34         //D34: headlights
#define FLH 14        //A0:  front left hazard
#define FRH 41        //A17: front right hazard
//#define Horn NULL   //powered by BEM

#define sTurnLeft 3   //D3: left turn switch
#define sTurnRight 4  //D4: right turn switch
#define sRunning 5    //D5: running light switch
#define sBrakes 6     //D6: brake light switch
#define sHorn 7       //D7: horn switch
#define sHazard 8     //D8: hazard light switch
//#define sWindshield NULL //built in

const int TRIG_PIN = 17;
const int ECHO_PIN = 19;

// Anything over 400 cm (23200 us pulse) is "out of range"
const unsigned int MAX_DIST = 400;

std::deque<float> distances;
float median;
float brake_dist = 8;

IntervalTimer timer;
void send_all_wrapper() {
  g_vehicle.send_all();
}

bool hazard = false;
bool leftTurn = false;
bool rightTurn = false;
bool runningLights = false;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(Windshield, OUTPUT);
  pinMode(HL, OUTPUT);
  pinMode(FLH, OUTPUT);
  pinMode(FRH, OUTPUT);

  pinMode(sTurnLeft, INPUT);
  pinMode(sTurnRight, INPUT);
  pinMode(sHazard, INPUT);
  pinMode(sRunning, INPUT);
  pinMode(sHorn, INPUT);

  digitalWrite(Windshield, HIGH); //windshield always has power.

  g_vehicle.init_network(DevBoard::PERIPHERALS);
  timer.begin(send_all_wrapper, 100000);

  // The Trigger pin will tell the sensor to range find
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);

  //Set Echo pin as input to measure the duration of 
  //pulses coming back from the distance sensor
  pinMode(ECHO_PIN, INPUT);

  // We'll use the serial monitor to view the sensor output
  Serial.begin(9600);

  digitalWrite(LED_BUILTIN, HIGH);
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
  delay(10);
}

void readStates() {
  leftTurn = digitalRead(sTurnLeft) == HIGH;
  rightTurn = digitalRead(sTurnRight) == HIGH;
  hazard = digitalRead(sHazard) == LOW; //switch inverted

  if(digitalRead(sRunning) == LOW){ //switch inverted
    runningLights = true;
    g_vehicle.m_peripherals_backrunninglights = 1;
    g_vehicle.m_peripherals_headlights = 1;
  }
  else {
    runningLights = false;
    g_vehicle.m_peripherals_backrunninglights = 0;
    g_vehicle.m_peripherals_headlights = 0;
  }
  float brake_readings[5];
  for(int i = 0; i < 5; i++){
    brake_readings[i] = measure_distance();
  }
  std::deque<float> sorted_distances(brake_readings, brake_readings + 5);
  std::sort(sorted_distances.begin(), sorted_distances.end());
  median = sorted_distances[sorted_distances.size() / 2];

  Serial.println(median);
  g_vehicle.m_peripherals_brakelights = median < brake_dist;
}

void loop() {
  readStates();  

  if(millis() % 2000 >= 1000) {
    if(hazard){
      digitalWrite(FLH, HIGH);
      digitalWrite(FRH, HIGH);
      g_vehicle.m_peripherals_hazard = 1;
      g_vehicle.m_peripherals_turn = 0;
    }
    else if(leftTurn){
      digitalWrite(FLH, HIGH);
      g_vehicle.m_peripherals_hazard = 0;
      g_vehicle.m_peripherals_turn = 1;
    }
    else if(rightTurn) {
      digitalWrite(FRH, HIGH);
      g_vehicle.m_peripherals_hazard = 0;
      g_vehicle.m_peripherals_turn = 2;
    }
  }
  else {
    digitalWrite(FLH, LOW);
    digitalWrite(FRH, LOW);
    g_vehicle.m_peripherals_hazard = 0;
     g_vehicle.m_peripherals_turn = 0;
  }

  if(runningLights){
    digitalWrite(HL, HIGH);
  }
  else {
    digitalWrite(HL, LOW);
  }
}