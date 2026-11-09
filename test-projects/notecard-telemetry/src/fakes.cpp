#include "Arduino.h"
#include "Notecard.h"
#include "Wire.h"
#include "vehicle.hpp"

unsigned long fake_millis = 0;
int fake_button_reading = HIGH;
FakeSerial Serial;
TwoWire Wire;
Vehicle g_vehicle = {};
std::vector<J> fake_requests;
std::string fake_failed_request;
unsigned fake_retry_seconds = 0;

void FakeSerial::println(const char *message) {
  (void)message;
}

void TwoWire::begin(int sda_pin, int scl_pin) {
  (void)sda_pin;
  (void)scl_pin;
}

void JAddStringToObject(J *object, const char *key, const char *value) {
  object->strings[key] = value;
}

void JAddNumberToObject(J *object, const char *key, double value) {
  object->numbers[key] = value;
}

void JAddBoolToObject(J *object, const char *key, bool value) {
  object->booleans[key] = value;
}

J *JAddObjectToObject(J *object, const char *key) {
  (void)key;
  object->body = std::shared_ptr<J>(new J());
  return object->body.get();
}

void JDelete(J *object) {
  delete object;
}

void Notecard::begin(unsigned address, unsigned maximum, TwoWire &wire) {
  (void)address;
  (void)maximum;
  (void)wire;
}

void Notecard::setDebugOutputStream(FakeSerial &serial) {
  (void)serial;
}

J *Notecard::newRequest(const char *request) {
  J *result = new J();
  result->request = request;
  return result;
}

bool Notecard::sendRequest(J *request) {
  bool success = request != nullptr && request->request != fake_failed_request;
  if (request != nullptr) {
    fake_requests.push_back(*request);
    delete request;
  }
  return success;
}

bool Notecard::sendRequestWithRetry(J *request, unsigned seconds) {
  fake_retry_seconds = seconds;
  return sendRequest(request);
}
