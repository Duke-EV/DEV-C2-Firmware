#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <SPI.h>
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
#define sHazard 8     //TODO: find new pin

#define TRIG_PIN 14
#define ECHO_PIN 15

#define LATCH_PIN 16
#define CLOCK_PIN 17
#define DATA_PIN 18 

const unsigned int MAX_DIST = 400;

const uint8_t SLOPE_SENSOR_ADDR = 0x6A;
const uint8_t MCP_ADDR = 0x20;

float median;
float brake_dist = 4.0; 

IntervalTimer timer;
void send_all_wrapper() {
  g_vehicle.send_all();
}

bool hazard = false;
bool leftTurn = false;
bool rightTurn = false;
bool runningLights = false;

Adafruit_MCP23X17 mcp;

void write_to_shift_register(uint8_t data) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, data);
  digitalWrite(LATCH_PIN, HIGH);
}

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  if (!mcp.begin_I2C(MCP_ADDR)) {          // A0-A2 all tied to GND
    Serial.println("No MCP23017 at 0x20 — check A0-A2 and RESET");
    while (1);
  }

  mcp.pinMode(0, INPUT_PULLUP);
  mcp.pinMode(8, INPUT);

  // mcp.pinMode(LED_BUILTIN, OUTPUT);
  // mcp.pinMode(Windshield, OUTPUT);
  // mcp.pinMode(HL, OUTPUT);
  // mcp.pinMode(FLH, OUTPUT);
  // mcp.pinMode(FRH, OUTPUT);

  mcp.pinMode(sTurnLeft, INPUT);
  mcp.pinMode(sTurnRight, INPUT);
  mcp.pinMode(sHazard, INPUT);
  mcp.pinMode(sRunning, INPUT);
  mcp.pinMode(sHorn, INPUT);

  // mcp.digitalWrite(Windshield, HIGH);

  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);

  g_vehicle.init_network(DevBoard::PERIPHERALS);
  timer.begin(send_all_wrapper, 100000);

  mcp.pinMode(TRIG_PIN, OUTPUT);
  mcp.digitalWrite(TRIG_PIN, LOW);
  mcp.pinMode(ECHO_PIN, INPUT);

  Serial.begin(9600);
  digitalWrite(LED_BUILTIN, HIGH);

  Wire.begin();
  Wire.setClock(400000);     // 400 kHz; 100000 is the safe default

  Wire.beginTransmission(SLOPE_SENSOR_ADDR);
  Wire.write(0x10);
  Wire.write(0x40);
  Wire.endTransmission();


}

uint16_t measure_slope() {
  Wire.beginTransmission(SLOPE_SENSOR_ADDR);
  Wire.write(0x28);
  if (Wire.endTransmission(false) != 0) {
    return 0;
  }

  if (Wire.requestFrom(SLOPE_SENSOR_ADDR, 6) < 6) {
    return 0;
  }

  int16_t x = Wire.read() | (Wire.read() << 8);
  Wire.read();
  Wire.read(); // skip Y
  int16_t z = Wire.read() | (Wire.read() << 8);

  int angle = atan2(x, z) * 180.0 / PI;
  return angle + 180; // 180 is level; fits in uint16_t
}

//TODO: fix input.
float measure_distance() {
  // Use pulseIn for safety - it won't freeze your code if the sensor fails
  mcp.digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  mcp.digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  mcp.digitalWrite(TRIG_PIN, LOW);

  // pulseIn returns the duration in microseconds
  // 30000 is the timeout (30ms)
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) return 999.0; // Return out of range if no echo
  return duration / 58.0;
}

//TODO: fix input
void readStates() {
  leftTurn = mcp.digitalRead(sTurnLeft) == HIGH;
  rightTurn = mcp.digitalRead(sTurnRight) == HIGH;
  hazard = mcp.digitalRead(sHazard) == LOW;

  if(mcp.digitalRead(sRunning) == LOW){
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
  for(int i = 0; i < 3; i++){
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

  g_vehicle.m_peripherals_slope = measure_slope();
}

void loop() {
  readStates();

  uint8_t data = 0;

  // Turn signal/Hazard flashing logic (1Hz)
  if(millis() % 1000 >= 500) { //turn on for 1 second
    if(hazard){
      // digitalWrite(FLH, HIGH);
      // digitalWrite(FRH, HIGH);
      bitSet(data, FLH);
      bitSet(data, FRH);
      g_vehicle.m_peripherals_hazard = 1;
      g_vehicle.m_peripherals_turn = 0;
    }
    else if(leftTurn){
      // digitalWrite(FLH, HIGH);
      bitSet(data, FLH);
      g_vehicle.m_peripherals_hazard = 0;
      g_vehicle.m_peripherals_turn = 1;
    }
    else if(rightTurn) {
      // digitalWrite(FRH, HIGH);
      bitSet(data, FRH);
      g_vehicle.m_peripherals_hazard = 0;
      g_vehicle.m_peripherals_turn = 2;
    }
  }
  else { //turn off for 1 second
    // digitalWrite(FLH, LOW);
    bitClear(data, FLH);
    // digitalWrite(FRH, LOW);
    bitClear(data, FRH);
    g_vehicle.m_peripherals_hazard = 0;
    g_vehicle.m_peripherals_turn = 0;

  }

  if(runningLights){
    // digitalWrite(HL, HIGH);
    bitSet(data, HL);
  }
  else {
    // digitalWrite(HL, LOW);
    bitClear(data, HL);
  }

  write_to_shift_register(data);
}