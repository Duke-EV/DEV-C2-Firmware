#include <Arduino.h>
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN3> Can3;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  delay(200);

  Can3.begin();
  Can3.setBaudRate(500000);

  Serial.println("CAN RECEIVER");
}

void loop() {
  Can3.events();

  CAN_message_t msg;
  if (Can3.read(msg)) {

    //LED blinks
    digitalWrite(LED_BUILTIN, HIGH);
    delay(20);
    digitalWrite(LED_BUILTIN, LOW);

    //ID + LEN + DATA
    Serial.print("[RX] ID=0x");
    Serial.print(msg.id, HEX);
    Serial.print(" LEN=");
    Serial.print(msg.len);
    Serial.print(" DATA=");

    for (int i = 0; i < msg.len; i++) {
      if (msg.buf[i] < 16) Serial.print("0");
      Serial.print(msg.buf[i], HEX);
      Serial.print(" ");
    }

    Serial.println();
  }
}
