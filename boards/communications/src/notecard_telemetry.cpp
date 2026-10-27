#include "notecard_telemetry.hpp"

#include <Arduino.h>
#include <Notecard.h>
#include <Wire.h>
#include <vehicle.hpp>

static const char *TELEMETRY_FILE = "telemetry_v1.qo"; // defines name of sent file thru notecard
static const unsigned long SAMPLE_INTERVAL_MS = 10000; // sample every 10 sec
static const unsigned long DEBOUNCE_MS = 50;           // button must stay stable for 50ms before works

static Notecard s_notecard;
static bool s_ready = false;       // s_ready tracks if notecard is set up right
static bool s_recording = false;   // is it recording data or not rn
static bool s_button_state = HIGH; // high mean unpressed button
static bool s_last_button_reading = HIGH;
static unsigned long s_button_changed_ms = 0; // how long ago button changed state
static unsigned long s_last_sample_ms = 0;    // timestamp of most recent sample

static bool configure_template();
static bool queue_vehicle_note();
static void request_sync();

// MOST IMPORTANT FUNCTION
bool init_notecard_telemetry()
{
  //
  pinMode(TELEMETRY_BUTTON_PIN, INPUT_PULLUP);
  Wire.begin(NOTECARD_SDA_PIN, NOTECARD_SCL_PIN);                      // initializes I2C thru SDA/SCL
  s_notecard.begin(NOTE_I2C_ADDR_DEFAULT, NOTE_I2C_MAX_DEFAULT, Wire); // connects notecard library to Wire
  s_notecard.setDebugOutputStream(Serial);                             // Sends errors to monitor

  J *request = s_notecard.newRequest("hub.set"); // allocates json request
                                                 // if no request, print below
  if (request == nullptr)
  {
    Serial.println("Notecard: could not create hub.set request");
    return false;
  }

  // adds to json file all the details
  JAddStringToObject(request, "product", NOTECARD_PRODUCT_UID);
  JAddStringToObject(request, "mode", "minimum"); // minimum disables automatic notecard synchronization???
  // checks if request send suceeded
  if (!s_notecard.sendRequest(request))
  {
    Serial.println("Notecard: hub.set failed");
    return false;
  }

  // checks if notecard set up right
  if (!configure_template())
  {
    return false;
  }

  // when notecard ready print this stuff and ready to true, returns true
  Serial.println("Notecard telemetry ready; recording is OFF");
  s_ready = true;
  return true;
}

//
static bool configure_template()
{
  // templates tell the notecard the expected names and binary representation which reduces transmitted data size
  // creates json request for template
  J *request = s_notecard.newRequest("note.template");
  if (request == nullptr)
  {
    Serial.println("Notecard: could not create template request");
    return false;
  }

  // add details to json
  JAddStringToObject(request, "file", TELEMETRY_FILE); // associates template with the telemetry file
  J *body = JAddObjectToObject(request, "body");       // creates nested template body
  // if body is empty, delete request and return false
  if (body == nullptr)
  {
    JDelete(request);
    return false;
  }

  // SETS SENT INFO TYPES
  // 12 means 16 bit
  // 14 means 32 bit
  JAddBoolToObject(body, "peripherals_windshield", true);
  JAddBoolToObject(body, "peripherals_back_running_lights", true);
  JAddNumberToObject(body, "peripherals_turn", 12);
  JAddBoolToObject(body, "peripherals_headlights", true);
  JAddBoolToObject(body, "peripherals_brake_lights", true);
  JAddBoolToObject(body, "peripherals_hazard", true);
  JAddNumberToObject(body, "pdb_current_ma", 12);
  JAddNumberToObject(body, "pdb_voltage_mv", 12);
  JAddBoolToObject(body, "pdb_motor_enabled", true);
  JAddNumberToObject(body, "motor_rpm", 14);
  JAddNumberToObject(body, "throttle_raw", 12);
  JAddNumberToObject(body, "throttle_average", 12);
  JAddNumberToObject(body, "joulemeter_current", 12);
  JAddNumberToObject(body, "joulemeter_voltage", 12);
  JAddNumberToObject(body, "joulemeter_energy", 14);
  JAddNumberToObject(body, "uptime_ms", 14);

  // if request was not transmitted sucessfully, return false
  if (!s_notecard.sendRequest(request))
  {
    Serial.println("Notecard: note.template failed");
    return false;
  }
  return true;
}

// puts the send file on the queue to be sent
static bool queue_vehicle_note()
{
  J *request = s_notecard.newRequest("note.add");
  if (request == nullptr)
  {
    return false;
  }

  JAddStringToObject(request, "file", TELEMETRY_FILE);
  JAddBoolToObject(request, "full", true);
  J *body = JAddObjectToObject(request, "body");
  if (body == nullptr)
  {
    JDelete(request);
    return false;
  }

  // != 0 converts any nonzero 8 bit value into json true/false
  // this allows the values stored to change as the parameters change
  JAddBoolToObject(body, "peripherals_windshield", g_vehicle.m_peripherals_windshield != 0);
  JAddBoolToObject(body, "peripherals_back_running_lights", g_vehicle.m_peripherals_backrunninglights != 0);
  JAddNumberToObject(body, "peripherals_turn", g_vehicle.m_peripherals_turn);
  JAddBoolToObject(body, "peripherals_headlights", g_vehicle.m_peripherals_headlights != 0);
  JAddBoolToObject(body, "peripherals_brake_lights", g_vehicle.m_peripherals_brakelights != 0);
  JAddBoolToObject(body, "peripherals_hazard", g_vehicle.m_peripherals_hazard != 0);
  JAddNumberToObject(body, "pdb_current_ma", g_vehicle.m_pdb_current);
  JAddNumberToObject(body, "pdb_voltage_mv", g_vehicle.m_pdb_voltage);
  JAddBoolToObject(body, "pdb_motor_enabled", g_vehicle.m_pdb_motor_enabled != 0);
  JAddNumberToObject(body, "motor_rpm", g_vehicle.m_motor_rpm);
  JAddNumberToObject(body, "throttle_raw", g_vehicle.m_throttle_raw);
  JAddNumberToObject(body, "throttle_average", g_vehicle.m_throttle_average);
  JAddNumberToObject(body, "joulemeter_current", g_vehicle.m_joulemeter_current);
  JAddNumberToObject(body, "joulemeter_voltage", g_vehicle.m_joulemeter_voltage);
  JAddNumberToObject(body, "joulemeter_energy", g_vehicle.m_joulemeter_energy);
  JAddNumberToObject(body, "uptime_ms", millis());

  if (!s_notecard.sendRequest(request))
  {
    Serial.println("Notecard: note.add failed");
    return false;
  }
  return true;
}

// tries to sync with cloud
// needed cause minimum mode from earlier disables auto sync
static void request_sync()
{
  J *request = s_notecard.newRequest("hub.sync");
  if (request == nullptr || !s_notecard.sendRequest(request))
  {
    Serial.println("Notecard: hub.sync failed; Notes remain queued");
    return;
  }
  Serial.println("Notecard: queued Notes are syncing");
}

// reads data
void update_notecard_telemetry()
{
  // if initialize fails dont do anything
  if (!s_ready)
  {
    return;
  }

  // returns the number of milliseconds that have elapsed since the current program started running
  unsigned long now = millis();
  bool button_reading = digitalRead(TELEMETRY_BUTTON_PIN); // reads if button is on
  // toggle button
  if (button_reading != s_last_button_reading)
  {
    s_last_button_reading = button_reading;
    s_button_changed_ms = now;
  }

  // if button pressed for more than DEBOUNCE_MS (50ms)
  if ((now - s_button_changed_ms) >= DEBOUNCE_MS && button_reading != s_button_state)
  {
    s_button_state = button_reading;
    // if button is pressed, do this stuff
    if (s_button_state == LOW)
    {
      // change s_recording to opposite state
      s_recording = !s_recording;
      // if it is recording, record
      if (s_recording)
      {
        s_last_sample_ms = now;
        Serial.println("Notecard telemetry recording ON");
      }
      else
      {
        Serial.println("Notecard telemetry recording OFF");
        request_sync();
        // when turn recording off, sync with cloud
      }
    }
  }

  // if its recording and been more than 10 seconds then sample again
  if (s_recording && (now - s_last_sample_ms) >= SAMPLE_INTERVAL_MS)
  {
    s_last_sample_ms = now;
    if (queue_vehicle_note())
    {
      Serial.println("Notecard: telemetry Note queued");
    }
  }
}
