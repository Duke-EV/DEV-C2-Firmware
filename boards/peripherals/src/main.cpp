#include <Arduino.h>

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

bool hazard = false;
bool leftTurn = false;
bool rightTurn = false;

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
}

void loop() {
  if(digitalRead(sTurnLeft) == HIGH){                             
    leftTurn = true;
  }
  else {
    leftTurn = false;
  }

  if(digitalRead(sTurnRight) == HIGH){    
    rightTurn = true;
  }
  else {
    rightTurn = false;
  }

  if(digitalRead(sHazard) == HIGH){
    hazard = true;
  }
  else {
    hazard = false;
  }

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

  if(digitalRead(sRunning) == HIGH){
    digitalWrite(HL, HIGH);
  }
  else {
    digitalWrite(HL, LOW);
  }
}

