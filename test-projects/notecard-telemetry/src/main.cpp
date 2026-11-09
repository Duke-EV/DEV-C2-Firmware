#include <iostream>
#include <string>
#include "Arduino.h"
#include "Notecard.h"
#include "vehicle.hpp"
#define NOTECARD_PRODUCT_UID "test-product"
#include "../../../boards/communications/src/notecard_telemetry.cpp"

static int failures = 0;

static void expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    failures++;
  }
}

int main() {
  g_vehicle.m_peripherals_windshield = 1;
  g_vehicle.m_peripherals_backrunninglights = 0;
  g_vehicle.m_peripherals_turn = 2;
  g_vehicle.m_peripherals_headlights = 1;
  g_vehicle.m_peripherals_brakelights = 0;
  g_vehicle.m_peripherals_hazard = 1;
  g_vehicle.m_pdb_current = 40000;
  g_vehicle.m_pdb_voltage = 50000;
  g_vehicle.m_pdb_motor_enabled = 1;
  g_vehicle.m_motor_rpm = 123456;
  g_vehicle.m_throttle_raw = 60000;
  g_vehicle.m_throttle_average = 30000;
  g_vehicle.m_joulemeter_current = 45000;
  g_vehicle.m_joulemeter_voltage = 55000;
  g_vehicle.m_joulemeter_energy = 3000000000UL;

  expect(init_notecard_telemetry(), "initialization succeeds");
  expect(fake_retry_seconds == 5, "hub.set retries for five seconds");
  expect(fake_requests.size() == 2, "initialization sends two requests");
  expect(fake_requests[0].request == "hub.set", "hub.set is first");
  expect(fake_requests[0].strings["product"] == "test-product", "ProductUID is sent");
  expect(fake_requests[0].strings["mode"] == "minimum", "minimum mode is set");
  expect(fake_requests[1].request == "note.template", "template is second");
  expect(fake_requests[1].strings["file"] == "telemetry_v1.qo", "template file matches");
  expect(fake_requests[1].body->numbers["peripherals_turn"] == TUINT8, "turn uses uint8");
  expect(fake_requests[1].body->numbers["pdb_current_ma"] == TUINT16, "current uses uint16");
  expect(fake_requests[1].body->numbers["motor_rpm"] == TUINT32, "RPM uses uint32");

  fake_button_reading = LOW;
  fake_millis = 100;
  update_notecard_telemetry();
  fake_millis = 149;
  update_notecard_telemetry();
  expect(fake_requests.size() == 2, "button bounce does not start a Note");
  fake_millis = 150;
  update_notecard_telemetry();

  fake_button_reading = HIGH;
  fake_millis = 200;
  update_notecard_telemetry();
  fake_millis = 250;
  update_notecard_telemetry();
  fake_millis = 10149;
  update_notecard_telemetry();
  expect(fake_requests.size() == 2, "no Note before ten seconds");
  fake_millis = 10150;
  update_notecard_telemetry();

  expect(fake_requests.size() == 3, "one Note is queued at ten seconds");
  J &note = fake_requests[2];
  expect(note.request == "note.add", "third request is note.add");
  expect(note.strings["file"] == "telemetry_v1.qo", "Note file matches");
  expect(note.booleans["full"], "full snapshots are requested");
  expect(note.body->booleans["peripherals_windshield"], "windshield maps to true");
  expect(!note.body->booleans["peripherals_back_running_lights"], "running lights map to false");
  expect(note.body->numbers["peripherals_turn"] == 2, "turn value is preserved");
  expect(note.body->booleans["peripherals_headlights"], "headlights map to true");
  expect(!note.body->booleans["peripherals_brake_lights"], "brake lights map to false");
  expect(note.body->booleans["peripherals_hazard"], "hazard maps to true");
  expect(note.body->numbers["pdb_current_ma"] == 40000, "PDB current is preserved");
  expect(note.body->numbers["pdb_voltage_mv"] == 50000, "PDB voltage is preserved");
  expect(note.body->booleans["pdb_motor_enabled"], "motor enabled maps to true");
  expect(note.body->numbers["motor_rpm"] == 123456, "motor RPM is preserved");
  expect(note.body->numbers["throttle_raw"] == 60000, "raw throttle is preserved");
  expect(note.body->numbers["throttle_average"] == 30000, "average throttle is preserved");
  expect(note.body->numbers["joulemeter_current"] == 45000, "joulemeter current is preserved");
  expect(note.body->numbers["joulemeter_voltage"] == 55000, "joulemeter voltage is preserved");
  expect(note.body->numbers["joulemeter_energy"] == 3000000000UL, "energy is preserved");
  expect(note.body->numbers["uptime_ms"] == 10150, "uptime is recorded");

  fake_failed_request = "note.add";
  fake_millis = 20150;
  update_notecard_telemetry();
  expect(fake_requests.size() == 4, "failed note.add is attempted");
  fake_failed_request.clear();

  fake_button_reading = LOW;
  fake_millis = 20200;
  update_notecard_telemetry();
  fake_millis = 20250;
  update_notecard_telemetry();
  expect(fake_requests.size() == 5, "stopping sends one sync request");
  expect(fake_requests[4].request == "hub.sync", "last request is hub.sync");

  fake_millis = 40250;
  update_notecard_telemetry();
  expect(fake_requests.size() == 5, "recording stays off after sync");

  if (failures == 0) {
    std::cout << "Notecard telemetry host test passed" << std::endl;
  }
  return failures == 0 ? 0 : 1;
}
