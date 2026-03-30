#include <Arduino.h>
#include <vehicle.hpp>

#define Windshield 33 //D33: windshield wipers
#define HL 34         //D34: headlights
#define FLH 14        //A0:  front left hazard
#define FRH 41        //A17: front right hazard
//#define Horn NULL     //powered by BEM

#define sTurnLeft 3   //D3: left turn switch
#define sTurnRight 4  //D4: right turn switch
#define sRunning 5    //D5: running light switch
#define sBrakes 6     //D6: brake light switch
#define sHorn 7       //D7: horn switch
#define sHazard 8     //D8: hazard light switch
//#define sWindshield NULL //built in

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
}

void readStates() {
  if(digitalRead(sTurnLeft) == HIGH){                             
    leftTurn = true;
    g_vehicle.m_peripherals_turn = 1;
  }
  else {
    leftTurn = false;
  }

  if(digitalRead(sTurnRight) == HIGH){    
    rightTurn = true;
    g_vehicle.m_peripherals_turn = 2;
  }
  else {
    rightTurn = false;
  }

  if(!leftTurn && !rightTurn) g_vehicle.m_peripherals_turn = 0;

  if(digitalRead(sHazard) == HIGH){
    hazard = true;
    g_vehicle.m_peripherals_hazard == 1;
  }
  else {
    hazard = false;
    g_vehicle.m_peripherals_hazard == 0;
  }

  if(digitalRead(sRunning) == HIGH){
    runningLights = true;
    g_vehicle.m_peripherals_backrunninglights = 1;
    g_vehicle.m_peripherals_headlights = 1;
  }
  else {
    runningLights = false;
    g_vehicle.m_peripherals_backrunninglights = 0;
    g_vehicle.m_peripherals_headlights = 0;
  }

  g_vehicle.m_peripherals_brakelights = digitalRead(sBrakes == HIGH);
}

void loop() {
  readStates();  

  if(millis() % 2000 >= 1000) {
    if(hazard){
      digitalWrite(FLH, HIGH);
      digitalWrite(FRH, HIGH);
    }
    else if(leftTurn){
      digitalWrite(FLH, HIGH);
    }
    else if(rightTurn) {
      digitalWrite(FRH, HIGH);
    }
  }
  else {
    digitalWrite(FLH, LOW);
    digitalWrite(FRH, LOW);
  }

  if(runningLights){
    digitalWrite(HL, HIGH);
  }
  else {
    digitalWrite(HL, LOW);
  }
}
