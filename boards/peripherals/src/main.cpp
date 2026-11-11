#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <ASM330LHHSensor.h>
#include <SPI.h>
#include <vehicle.hpp>

#define Windshield 33 //TODO: find new pin
#define HL 34 //TODO: find new pin  
#define FLH 14 //TODO: find new pin
#define FRH 41 //TODO: find new pin

#define sTurnLeft 3
#define sTurnRight 4
#define sRunning 5
#define sBrakes 6
#define sHorn 7
#define sHazard 8     //TODO: find new pin

#define LATCH_PIN 33
#define CLOCK_PIN 13
#define DATA_PIN 11

const unsigned int MAX_DIST = 400;

const uint8_t MCP_ADDR = 0x20;

IntervalTimer timer;
void send_all_wrapper() {
  g_vehicle.send_all();
}

bool hazard = false;
bool leftTurn = false;
bool rightTurn = false;
bool runningLights = false;

uint16_t outputs = 0; //16 is farthest down daisy chain, 0 is closest to Teensy

Adafruit_MCP23X17 mcp;

ASM330LHHSensor imu(&Wire);

const float G = 9.80665f;
const float ALPHA = 0.98f; //filter weight for gyo <-> accelerometer

float gyroBiasY = 0;             // dps, measured at startup while the car is still
float pitchOffset = 0;           // rad, mounting angle (calibrated on level ground)
float pitch = 0;                 // rad, filtered nose-up angle
uint32_t lastUs = 0;
uint32_t lastPrintMs = 0;

// Forward acceleration of the car in m/s^2. Leave at 0 until wheel speed is available.
// Braking then looks a little like a slope, because this board cannot subtract it yet.
float vehicleAccel = 0;

// Read one accel sample (m/s^2) and the pitch gyro (degrees per second).
// a[] from the library is milli-g. g[] is milli-degrees per second.
void readIMU(float &ax, float &ay, float &az, float &gy) {
  int32_t a[3], g[3];
  imu.Get_X_Axes(a);
  imu.Get_G_Axes(g);
  // Axis order matches the chip. Swap these if nose-up comes out with the wrong sign.
  ax = a[0] * 0.001f * G;
  ay = a[1] * 0.001f * G;
  az = a[2] * 0.001f * G;
  gy = g[1] * 0.001f;
}

// Nose-up pitch from the accelerometer only, in radians.
// atan2(forward, vertical) is 0 when the car is level.
float accelPitch(float ax, float ay, float az) {
  float axc = ax - vehicleAccel;
  return atan2f(axc, sqrtf(ay * ay + az * az));
}

// Call with the car stationary. Averages gyro bias.
// When carIsOnLevelGround is true, the average accel pitch becomes the zero point.
void calibrate(bool carIsOnLevelGround) {
  const int N = 400;
  float sumGy = 0, sumP = 0;
  for (int i = 0; i < N; i++) {
    float ax, ay, az, gy;
    readIMU(ax, ay, az, gy);
    sumGy += gy;
    sumP += atan2f(ax, sqrtf(ay * ay + az * az));
    delay(5);
  }
  gyroBiasY = sumGy / N;
  float p = sumP / N;
  if (carIsOnLevelGround) {
    pitchOffset = p;
  }
  pitch = p - pitchOffset;
}

// index 0-15 selects a bit in outputs. bit is the value written there: 0 or 1.
// Bit 0 is Q0 on chip 1. Bit 8 is Q0 on chip 2.
void bitSetData(uint8_t index, uint8_t bit) {
  if (index > 15 || bit > 1) {
    return;
  }

  if (bit == 1) {
    bitSet(outputs, index);
  } else {
    bitClear(outputs, index);
  }
}

void write_to_shift_register(uint16_t value) {
  // SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  // SPI.transfer16(value);          // high byte -> chip 2, low byte -> chip 1
  // SPI.endTransaction();
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, value >> 8);    // chip 2
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, value & 0xFF);  // chip 1
  digitalWrite(LATCH_PIN, HIGH);
}

void setup() {
  // pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);

  if (!mcp.begin_I2C(MCP_ADDR)) {          // A0-A2 all tied to GND
    Serial.println("No MCP23017 at 0x20 — check A0-A2 and RESET");
    while (1);
  }

  mcp.pinMode(0, INPUT_PULLUP);
  mcp.pinMode(8, INPUT);

  mcp.pinMode(sTurnLeft, INPUT);
  mcp.pinMode(sTurnRight, INPUT);
  mcp.pinMode(sHazard, INPUT);
  mcp.pinMode(sRunning, INPUT);
  mcp.pinMode(sHorn, INPUT);

  mcp.pinMode(TRIG_PIN, OUTPUT);
  mcp.digitalWrite(TRIG_PIN, LOW);
  mcp.pinMode(ECHO_PIN, INPUT);

  pinMode(LATCH_PIN, OUTPUT);
  digitalWrite(LATCH_PIN, LOW);
  pinMode(CLOCK_PIN, OUTPUT);
  digitalWrite(CLOCK_PIN, LOW);
  pinMode(DATA_PIN, OUTPUT);
  digitalWrite(DATA_PIN, LOW);

  // SPI.begin(10000000, MSBFIRST, SPI_MODE0);

  bitSetData(Windshield, 1);
  
  g_vehicle.init_network(DevBoard::PERIPHERALS);
  timer.begin(send_all_wrapper, 100000);
  
  Wire.begin();
  Wire.setClock(400000);     // 400 kHz; 100000 is the safe default

  imu.begin();
  imu.Enable_X();
  imu.Enable_G();
  delay(100);

  // About 2 seconds. The car must sit still on level ground so bias and zero are valid.
  Serial.println("Keep the car still on LEVEL ground...");
  calibrate(true);
  lastUs = micros();

  // digitalWrite(LED_BUILTIN, HIGH);
}

// One filter step. Gyro tracks quick pitch changes. Accelerometer slowly corrects drift.
// CAN slope uses the old scale: 180 means level. 185 means 5 degrees nose-up.
void updateSlope() {
  float ax, ay, az, gy;
  readIMU(ax, ay, az, gy);

  uint32_t now = micros();
  float dt = (now - lastUs) * 1e-6f;
  lastUs = now;

  // Gyro +y (z up) tips the nose down, so nose-up rate is the opposite sign.
  float pitchRate = -(gy - gyroBiasY) * DEG_TO_RAD;

  float pAcc = accelPitch(ax, ay, az) - pitchOffset;
  pitch = ALPHA * (pitch + pitchRate * dt) + (1.0f - ALPHA) * pAcc;

  float slopeDeg = pitch * RAD_TO_DEG;
  float gradePct = tanf(pitch) * 100.0f;

  int slopeEncoded = (int)roundf(slopeDeg + 180.0f);
  if (slopeEncoded < 0) {
    slopeEncoded = 0;
  }
  if (slopeEncoded > 65535) {
    slopeEncoded = 65535; 
  }
  g_vehicle.m_peripherals_slope = (uint16_t)slopeEncoded;

  if (millis() - lastPrintMs >= 100) {
    lastPrintMs = millis();
    Serial.printf("pitch %.2f deg | grade %.1f %%\n", slopeDeg, gradePct);
  }
}

void readStates() {
  uint16_t pins = mcp.readGPIOAB();

  leftTurn = (pins & (1u << sTurnLeft)) != 0;
  rightTurn = (pins & (1u << sTurnRight)) != 0;
  hazard = (pins & (1u << sHazard)) == 0;

  if ((pins & (1u << sRunning)) == 0) {
    runningLights = true;
    g_vehicle.m_peripherals_backrunninglights = 1;
    g_vehicle.m_peripherals_headlights = 1;
  } 
  else {
    runningLights = false;
    g_vehicle.m_peripherals_backrunninglights = 0;
    g_vehicle.m_peripherals_headlights = 0;
  }
}

void loop() {
  updateSlope();
  readStates();

  outputs = 0;

  if(millis() % 1000 >= 500) { //turn on for 1 second
    if(hazard){
      bitSetData(FLH, 1);
      bitSetData(FRH, 1);
      g_vehicle.m_peripherals_hazard = 1;
      g_vehicle.m_peripherals_turn = 0;
    }
    else if(leftTurn){
      bitSetData(FLH, 1);
      g_vehicle.m_peripherals_hazard = 0;
      g_vehicle.m_peripherals_turn = 1;
    }
    else if(rightTurn) {
      bitSetData(FRH, 1);
      g_vehicle.m_peripherals_hazard = 0;
      g_vehicle.m_peripherals_turn = 2;
    }
  }
  else { //turn off for 1 second
    bitSetData(FLH, 0);
    bitSetData(FRH, 0);
    g_vehicle.m_peripherals_hazard = 0;
    g_vehicle.m_peripherals_turn = 0;
  }

  if(runningLights){
    bitSetData(HL, 1);
  }
  else {
    bitSetData(HL, 0);
  }

  write_to_shift_register(outputs);
}