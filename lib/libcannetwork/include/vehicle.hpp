#pragma once
#include <stdint.h>
#if defined(ARDUINO_ARCH_ESP32)
// Moving to CANFD documenation is here https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/peripherals/twai.html
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#elif defined(CORE_TEENSY)
#include <FlexCAN_T4.h>
#else
#error "CAN network library not written for chosen architecture"
#endif // defined

#pragma pack(push, 1) 
struct MsgPeripherals {
  uint8_t heartbeat;
  uint8_t windshield;
  uint8_t backrunninglights;
  uint8_t turn;
  uint8_t headlights;
  uint8_t brakelights;
  uint8_t hazard;
};

struct MsgPDB {
  uint8_t heartbeat;
  uint16_t current;
  uint16_t voltage;
  uint8_t motor_enabled;
};

struct MsgMotor {
  uint8_t heartbeat;
  uint32_t rpm;
};

struct MsgThrottle {
  uint8_t heartbeat;
  uint16_t raw;
  uint16_t average;
};

struct MsgJoulemeter {
  uint8_t heartbeat;
  uint16_t current;
  uint16_t voltage;
  uint32_t energy;
};

#pragma pack(pop)

enum DevBoard {
  COMMUNICATIONS,
  PERIPHERALS,
  MOTOR_CONTROLLER,
  POWER_DISTRIBUTION,
  THROTTLE,
  JOULEMETER,
  TEST_BOARD,
  // Number of designs this library is using
  BOARD_COUNT,
};

class Vehicle {
private:
  DevBoard m_board;

#if defined(ARDUINO_ARCH_ESP32)
  
#elif defined(CORE_TEENSY)
  // Teensy uses the shared static instance in the source file.
#else
#error "CAN network library not written for chosen architecture"
#endif

public:
  uint8_t m_peripherals_windshield;
  uint8_t m_peripherals_backrunninglights;
  uint8_t m_peripherals_turn;
  uint8_t m_peripherals_headlights;
  uint8_t m_peripherals_brakelights;
  uint8_t m_peripherals_hazard;

  uint16_t m_pdb_current;
  uint16_t m_pdb_voltage;
  uint8_t  m_pdb_motor_enabled;
  
  uint32_t m_motor_rpm;
  
  uint16_t m_throttle_raw;
  uint16_t m_throttle_average;
  
  uint16_t m_joulemeter_current;
  uint16_t m_joulemeter_voltage;
  uint32_t m_joulemeter_energy;
  // Private methods unique to architecture for sending/receiving can messages
private:
#if defined(ARDUINO_ARCH_ESP32)
  static void twai_receive_task(void *args);
#elif defined(CORE_TEENSY)
  static void forward_flexcan(const CANFD_message_t &msg);
#else
#error "CAN network library not written for chosen architecture"
#endif

  // Private methods common between architectures
private:
  void send_message(uint32_t id, uint8_t len, const uint8_t *data);
  #if defined(ARDUINO_ARCH_ESP32)
    static bool twai_rx_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx);
  #endif
  void on_receive(uint32_t id, uint8_t len, const uint8_t *data);

  // Public entry point
public:
  void init_network(DevBoard board);
  void send_all();
  #if defined(ARDUINO_ARCH_ESP32)
  twai_node_handle_t m_twai_handle; //
  esp_err_t m_last_error;
  #endif
};

extern Vehicle g_vehicle;
