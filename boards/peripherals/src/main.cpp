#include <Arduino.h>
#include <vehicle.hpp>
#include <algorithm> // Required for sorting
#include <vector>

#define Windshield 33 
#define HL 34         
#define FLH 14         
#define FRH 41         

#define sTurnLeft 3   
#define sTurnRight 4  
#define sRunning 5    
#define sBrakes 6     
#define sHorn 7       
#define sHazard 8     

const int TRIG_PIN = 17;
const int ECHO_PIN = 19;
const unsigned int MAX_DIST = 400;

float median;
float brake_dist = 4.3;

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

  digitalWrite(Windshield, HIGH); 

  g_vehicle.init_network(DevBoard::PERIPHERALS);
  timer.begin(send_all_wrapper, 100000);

  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);

  Serial.begin(9600);
  digitalWrite(LED_BUILTIN, HIGH);
}

float measure_distance() {
  // Use pulseIn for safety - it won't freeze your code if the sensor fails
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // pulseIn returns the duration in microseconds
  // 30000 is the timeout (30ms)
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  if (duration == 0) return 999.0; // Return out of range if no echo
  return duration / 58.0;
}

void readStates() {
  leftTurn = digitalRead(sTurnLeft) == HIGH;
  rightTurn = digitalRead(sTurnRight) == HIGH;
  hazard = digitalRead(sHazard) == LOW; 

  if(digitalRead(sRunning) == LOW){ 
    runningLights = true;
    g_vehicle.m_peripherals_backrunninglights = 1;
    g_vehicle.m_peripherals_headlights = 1;
  }
  else {
    runningLights = false;
    g_vehicle.m_peripherals_backrunninglights = 0;
    g_vehicle.m_peripherals_headlights = 0;
  }

  // Calculate Median
  std::vector<float> brake_readings;
  for(int i = 0; i < 9; i++){
    float cm = measure_distance();
    if(cm > 1.0 && cm < 5.0){ // Only consider valid readings
      brake_readings.push_back(cm);
    }
    delay(50); // Give the sensor a moment between pings
  }
  
  std::sort(brake_readings.begin(), brake_readings.end());
  median = brake_readings[brake_readings.size()/2]; // The middle value is the median

  Serial.println(median);
  g_vehicle.m_peripherals_brakelights = (median < brake_dist);
}

void loop() {
  readStates();  

  // Turn signal/Hazard flashing logic (1Hz)
  if(millis() % 1000 >= 500) { //turn on for 1 second
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
  else { //turn off for 1 second
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