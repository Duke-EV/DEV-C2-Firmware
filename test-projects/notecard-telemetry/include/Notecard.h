#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Arduino.h"
#include "Wire.h"

#define NOTE_I2C_ADDR_DEFAULT 0x17
#define NOTE_I2C_MAX_DEFAULT 255
#define TUINT8 21
#define TUINT16 22
#define TUINT32 24

struct J {
  std::string request;
  std::map<std::string, std::string> strings;
  std::map<std::string, double> numbers;
  std::map<std::string, bool> booleans;
  std::shared_ptr<J> body;
};

extern std::vector<J> fake_requests;
extern std::string fake_failed_request;
extern unsigned fake_retry_seconds;

void JAddStringToObject(J *object, const char *key, const char *value);
void JAddNumberToObject(J *object, const char *key, double value);
void JAddBoolToObject(J *object, const char *key, bool value);
J *JAddObjectToObject(J *object, const char *key);
void JDelete(J *object);

class Notecard {
public:
  void begin(unsigned address, unsigned maximum, TwoWire &wire);
  void setDebugOutputStream(FakeSerial &serial);
  J *newRequest(const char *request);
  bool sendRequest(J *request);
  bool sendRequestWithRetry(J *request, unsigned seconds);
};
