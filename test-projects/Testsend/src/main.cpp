#include <Arduino.h>
#include <FlexCAN_T4.h>

// CAN3 on Teensy 4.1 (TX=22, RX=23)
FlexCAN_T4<CAN3> Can3;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  delay(200);

  Can3.begin();
  Can3.setBaudRate(500000);

  Serial.println("CAN sender running");
}

void loop() {
  Can3.events();

  CAN_message_t msg;
  msg.id  = 0x100;
  msg.len = 5;                 // length of "HELLO"

  msg.buf[0] = 'H';
  msg.buf[1] = 'E';
  msg.buf[2] = 'L';
  msg.buf[3] = 'L';
  msg.buf[4] = 'O';

  Can3.write(msg);

  // Blink on send
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println("[TX] HELLO");
  bool ok = Can3.write(msg);
  Serial.println(ok ? "OK" : "FAIL");

  delay(1000);
}
