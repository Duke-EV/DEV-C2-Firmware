



#include <Arduino.h>
#include <vehicle.hpp>


#define HALL_1_PIN 27       // hall A
#define HALL_2_PIN 26       // hall B
#define HALL_3_PIN 25       // hall C


#define AH_PIN 37            // on board, pin 16, need to switch bc of pwm error
#define AL_PIN 36
#define BH_PIN 19
#define BL_PIN 18
#define CH_PIN 23            // on board, pin 20, need to switch bc of pwm error
#define CL_PIN 22

#define GEAR_RATIO 6     
#define RPM_WINDOW_MS 100  

#define LED_PIN 13           

#define HALL_OVERSAMPLE 4     



uint8_t hallToMotor[8] = {255, 4, 0, 5, 2, 3, 1, 255}; 

volatile unsigned long hallStateChangeCount = 0;
unsigned long rpmWindowStart = 0;
uint8_t lastHallForRPM = 0xFF;

// Forward declarations
void identifyHalls();
void writePWM(uint8_t motorState, uint8_t dutyCycle);
void writePhases(uint8_t ah, uint8_t bh, uint8_t ch, uint8_t al, uint8_t bl, uint8_t cl);
uint8_t getHalls();
void initRPMCounter();
void updateRPMCounter(uint8_t hall);
float revolutions = 0;
float rpm = 0;

IntervalTimer timer;
void send_all_wrapper() {
  g_vehicle.send_all();
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(10);  

  pinMode(LED_PIN, OUTPUT);

  pinMode(AH_PIN, OUTPUT);
  pinMode(AL_PIN, OUTPUT);
  pinMode(BH_PIN, OUTPUT);
  pinMode(BL_PIN, OUTPUT);
  pinMode(CH_PIN, OUTPUT);
  pinMode(CL_PIN, OUTPUT);

  analogWriteFrequency(AH_PIN, 8000);
  analogWriteFrequency(BH_PIN, 8000);
  analogWriteFrequency(CH_PIN, 8000);

  pinMode(HALL_1_PIN, INPUT);
  pinMode(HALL_2_PIN, INPUT);
  pinMode(HALL_3_PIN, INPUT);

  g_vehicle.init_network(DevBoard::MOTOR_CONTROLLER);
  timer.begin(send_all_wrapper, 100000);

  digitalWriteFast(LED_PIN, HIGH);
  delay(1000);
  digitalWriteFast(LED_PIN, LOW);
  initRPMCounter();
}

void loop() {
  uint8_t throttle = g_vehicle.m_throttle_average;
  if(!g_vehicle.m_pdb_motor_enabled){
    throttle = 0;
    digitalWriteFast(LED_PIN, LOW);
  } else{
    digitalWriteFast(LED_PIN, HIGH);
  }
  for(uint8_t i = 0; i < 200; i++)
  {  
    uint8_t hall = getHalls();
 
    
    uint8_t motorState = hallToMotor[hall];
    writePWM(motorState, throttle);
    updateRPMCounter(hall);
  }
  
}

void identifyHalls()
{
  for(uint8_t i = 0; i < 6; i++)
  {
    uint8_t nextState = (i + 1) % 6;

    for(uint16_t j = 0; j < 200; j++)
    { 
      delay(1);
      writePWM(i, 20);
      delay(1);
      writePWM(nextState, 20);
    }
  
    hallToMotor[getHalls()] = (i + 2) % 6;
  }
  writePWM(0, 0);
}

void writePWM(uint8_t motorState, uint8_t dutyCycle)
{
  if(dutyCycle == 0)
    motorState = 255;

  if(motorState == 0)
      writePhases(0, dutyCycle, 0, 1, 0, 0);
  else if(motorState == 1)
      writePhases(0, 0, dutyCycle, 1, 0, 0);
  else if(motorState == 2)
      writePhases(0, 0, dutyCycle, 0, 1, 0);
  else if(motorState == 3)
      writePhases(dutyCycle, 0, 0, 0, 1, 0);
  else if(motorState == 4)
      writePhases(dutyCycle, 0, 0, 0, 0, 1);
  else if(motorState == 5)
      writePhases(0, dutyCycle, 0, 0, 0, 1);
  else
      writePhases(0, 0, 0, 0, 0, 0);
}

void writePhases(uint8_t ah, uint8_t bh, uint8_t ch, uint8_t al, uint8_t bl, uint8_t cl)
{
  analogWrite(AH_PIN, ah);
  analogWrite(BH_PIN, bh);
  analogWrite(CH_PIN, ch);
  digitalWriteFast(AL_PIN, al);
  digitalWriteFast(BL_PIN, bl);
  digitalWriteFast(CL_PIN, cl);
}

uint8_t getHalls()
{
  uint8_t hallCounts[] = {0, 0, 0};
  for(uint8_t i = 0; i < HALL_OVERSAMPLE; i++)
  {
    hallCounts[0] += digitalReadFast(HALL_1_PIN);
    hallCounts[1] += digitalReadFast(HALL_2_PIN);
    hallCounts[2] += digitalReadFast(HALL_3_PIN);
  }

  uint8_t hall = 0;
  
  if (hallCounts[0] >= HALL_OVERSAMPLE / 2)
    hall |= (1<<0);
  if (hallCounts[1] >= HALL_OVERSAMPLE / 2)
    hall |= (1<<1);
  if (hallCounts[2] >= HALL_OVERSAMPLE / 2)
    hall |= (1<<2);

  return hall & 0x7;
}

void initRPMCounter() {
  lastHallForRPM = getHalls();
  rpmWindowStart = millis();
}

void updateRPMCounter(uint8_t hall) {
  uint8_t currentHall = hall;

  // only count valid hall states
  if (currentHall >= 1 && currentHall <= 6) {
    if (currentHall != lastHallForRPM) {
      hallStateChangeCount++;
      lastHallForRPM = currentHall;
    }
  }


  unsigned long now = millis();
  // counting rpm over a 100ms period
  if (now - rpmWindowStart >= RPM_WINDOW_MS) {
    revolutions = hallStateChangeCount/(30.0*GEAR_RATIO);
    // convert RPM window to minutes 
    rpm = revolutions/(RPM_WINDOW_MS/60000.0f);
    g_vehicle.m_motor_rpm = (uint16_t) rpm;
    hallStateChangeCount = 0;
    rpmWindowStart = now;
  }
}
