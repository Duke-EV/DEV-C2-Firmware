



#include <Arduino.h>

// #define HWSERIAL Serial1  // serial port for debugging, bluetooth 

#define HALL_1_PIN 27       // hall A
#define HALL_2_PIN 26       // hall B
#define HALL_3_PIN 25       // hall C

// Pins from the Teensy to the gate drivers. AH = A high, etc
#define AH_PIN 37           
#define AL_PIN 36
#define BH_PIN 19
#define BL_PIN 18
#define CH_PIN 23            
#define CL_PIN 22

#define LED_PIN 13            // The teensy has a built-in LED on pin 13

#define HALL_OVERSAMPLE 4     // Hall oversampling count. More on this in the getHalls() function

//uint8_t hallToMotor[8] = {255, 0, 2, 1, 4, 5, 3, 255}; // PROBLEM: how to know phase order? 

uint8_t hallToMotor[8] = {255, 4, 0, 5, 2, 3, 1, 255}; // old motor state order from 24V motor 



// Forward declarations
void identifyHalls();
void writePWM(uint8_t motorState, uint8_t dutyCycle);
void writePhases(uint8_t ah, uint8_t bh, uint8_t ch, uint8_t al, uint8_t bl, uint8_t cl);
uint8_t getHalls();

void setup() {                // The setup function is called ONCE on boot-up
  Serial.begin(115200);


  analogReadResolution(10);  
  
  

  pinMode(LED_PIN, OUTPUT);
  digitalWriteFast(LED_PIN, HIGH);

  pinMode(AH_PIN, OUTPUT);    // Set all PWM pins as output
  pinMode(AL_PIN, OUTPUT);
  pinMode(BH_PIN, OUTPUT);
  pinMode(BL_PIN, OUTPUT);
  pinMode(CH_PIN, OUTPUT);
  pinMode(CL_PIN, OUTPUT);

  analogWriteFrequency(AH_PIN, 8000); // Set the PWM frequency. Since all pins are on the same timer, this sets PWM freq for all
  analogWriteFrequency(BH_PIN, 8000); // Set the PWM frequency. Since all pins are on the same timer, this sets PWM freq for all
  analogWriteFrequency(CH_PIN, 8000); // Set the PWM frequency. Since all pins are on the same timer, this sets PWM freq for all


  pinMode(HALL_1_PIN, INPUT);         // Set the hall pins as input
  pinMode(HALL_2_PIN, INPUT);
  pinMode(HALL_3_PIN, INPUT);
  
  //identifyHalls();                  // Uncomment this if you want the controller to auto-identify the hall states at startup!
}

void loop() {                         // The loop function is called repeatedly, once setup() is done
  
  uint8_t throttle = 100;
  for(uint8_t i = 0; i < 200; i++)
  {  
    uint8_t hall = getHalls();              // Read from the hall sensors
    Serial.println((int) hall);
    
    uint8_t motorState = hallToMotor[hall]; // Convert from hall values (from 1 to 6) to motor state values (from 0 to 5) in the correct order. This line is magic

    writePWM(motorState, throttle);
  }

}

void identifyHalls()
{
  for(uint8_t i = 0; i < 6; i++)
  {
    uint8_t nextState = (i + 1) % 6;        // Calculate what the next state should be. This is for switching into half-states

    for(uint16_t j = 0; j < 200; j++)       // For a while, repeatedly switch between states
    { 
      delay(1);
      writePWM(i, 20);
      delay(1);
      writePWM(nextState, 20);
    }
  
    hallToMotor[getHalls()] = (i + 2) % 6;  // Store the hall state - motor state correlation. Notice that +2 indicates 90 degrees ahead, as we're at half states
  }
  
  writePWM(0, 0);                           // Turn phases off
  
}

/* This function takes a motorState (from 0 to 5) as an input, and decides which transistors to turn on
 * dutyCycle is from 0-255, and sets the PWM value.
 * 
 * Note if dutyCycle is zero, or if there's an invalid motorState, then it turns all transistors off
 */

void writePWM(uint8_t motorState, uint8_t dutyCycle)
{
  if(dutyCycle == 0)                          // If zero throttle, turn all off
    motorState = 255;

  if(motorState == 0)                         // LOW A, HIGH B
      writePhases(0, dutyCycle, 0, 1, 0, 0);
  else if(motorState == 1)                    // LOW A, HIGH C
      writePhases(0, 0, dutyCycle, 1, 0, 0);
  else if(motorState == 2)                    // LOW B, HIGH C
      writePhases(0, 0, dutyCycle, 0, 1, 0);
  else if(motorState == 3)                    // LOW B, HIGH A
      writePhases(dutyCycle, 0, 0, 0, 1, 0);
  else if(motorState == 4)                    // LOW C, HIGH A
      writePhases(dutyCycle, 0, 0, 0, 0, 1);
  else if(motorState == 5)                    // LOW C, HIGH B
      writePhases(0, dutyCycle, 0, 0, 0, 1);
  else                                        // All off
      writePhases(0, 0, 0, 0, 0, 0);
}


void writePhases(uint8_t ah, uint8_t bh, uint8_t ch, uint8_t al, uint8_t bl, uint8_t cl) //THIS IS THE PROBLEM
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
  for(uint8_t i = 0; i < HALL_OVERSAMPLE; i++) // Read all the hall pins repeatedly, tally results 
  {
    hallCounts[0] += digitalReadFast(HALL_1_PIN);
    hallCounts[1] += digitalReadFast(HALL_2_PIN);
    hallCounts[2] += digitalReadFast(HALL_3_PIN);
  }

  uint8_t hall = 0;
  
  if (hallCounts[0] >= HALL_OVERSAMPLE / 2)     // If votes >= threshold, call that a 1
    hall |= (1<<0);                             // Store a 1 in the 0th bit
  if (hallCounts[1] >= HALL_OVERSAMPLE / 2)
    hall |= (1<<1);                             // Store a 1 in the 1st bit
  if (hallCounts[2] >= HALL_OVERSAMPLE / 2)
    hall |= (1<<2);                             // Store a 1 in the 2nd bit


  return hall & 0x7;                            // Just to make sure we didn't do anything stupid, set the maximum output value to 7
}
