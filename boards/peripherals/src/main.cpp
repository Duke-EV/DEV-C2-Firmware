#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <ASM330LHHSensor.h>
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

#define LATCH_PIN 33
#define CLOCK_PIN 13
#define DATA_PIN 11

const unsigned int MAX_DIST = 400;

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

// One byte per shift register. Each byte holds outputs 0 through 7.
uint8_t data[2] = {0, 0};

Adafruit_MCP23X17 mcp;

ASM330LHHSensor imu(&Wire);

// Gravity in m/s^2. Used to turn the chip's milli-g readings into real acceleration.
const float G = 9.80665f;
// Gyro share of the slope filter. 0.98 trusts the gyro for fast changes.
// The other 0.02 slowly pulls the estimate back to the accelerometer.
const float ALPHA = 0.98f;

float gyroBiasY = 0;             // dps, measured at startup while the car is still
float pitchOffset = 0;           // rad, mounting angle (calibrated on level ground)
float pitch = 0;                 // rad, filtered nose-up angle
uint32_t lastUs = 0;

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

// index 0-7 selects a bit in data[0]. index 8-15 selects a bit in data[1].
// bit is the value written there: 0 or 1.
// Inside a byte, bit 0 is worth 1 and bit 7 is worth 128.
void bitSetData(uint8_t index, uint8_t bit) {
  if (index > 15 || bit > 1) {
    return;
  }

  uint8_t byteIndex = index / 8;
  uint8_t bitIndex = index % 8;

  if (bit == 1) {
    bitSet(data[byteIndex], bitIndex);
  } else {
    bitClear(data[byteIndex], bitIndex);
  }
}

void write_to_shift_register() {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, data[0]);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, data[1]);
  digitalWrite(LATCH_PIN, HIGH);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

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
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);

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

  digitalWrite(LED_BUILTIN, HIGH);
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

  int slopeEncoded = (int)(slopeDeg + 180.0f);
  if (slopeEncoded < 0) {
    slopeEncoded = 0;
  }
  if (slopeEncoded > 65535) {
    slopeEncoded = 65535;
  }
  g_vehicle.m_peripherals_slope = (uint16_t)slopeEncoded;

  Serial.printf("pitch %.2f deg | grade %.1f %%\n", slopeDeg, gradePct);
}

float measure_distance() {
  // Use pulseIn for safety - it won't freeze your code if the sensor fails
  mcp.digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  mcp.digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  mcp.digitalWrite(TRIG_PIN, LOW);

  // pulseIn returns the duration in microseconds
  // 30000 is the timeout (30ms)
  // unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  unsigned long duration = 0; //no more ultrasonic sensor

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
}

void loop() {
  updateSlope();
  readStates();

  data[0] = 0;
  data[1] = 0;

  // Turn signal/Hazard flashing logic (1Hz)
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

  write_to_shift_register();
}