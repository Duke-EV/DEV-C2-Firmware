/*
This is a modified vehicle.hpp which exposes more variables and
information for the test board.
*/

#pragma once
#include <stdint.h>

#if defined(ARDUINO_ARCH_ESP32)
#include "driver/twai.h"
#ifndef VEHICLE_TWAI_TX_PIN
#define VEHICLE_TWAI_TX_PIN GPIO_NUM_5
#endif
#ifndef VEHICLE_TWAI_RX_PIN
#define VEHICLE_TWAI_RX_PIN GPIO_NUM_4
#endif
#elif defined(CORE_TEENSY)
#include <FlexCAN_T4.h>
#else
#error "CAN network library not written for chosen architecture"
#endif // defined

enum DevBoard {
  COMMUNICATIONS,
  PERIPHERALS,
  MOTOR_CONTROLLER,
  POWER_DISTRIBUTION,
  THROTTLE,
  JOULEMETER,
  // Number of designs this library is using
  BOARD_COUNT
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
  
  uint32_t m_motor_rpm;
  
  uint16_t m_throttle_percentage;
  
  uint16_t m_joulemeter_current;
  uint16_t m_joulemeter_voltage;
  uint32_t m_joulemeter_energy;
  // Private methods unique to architecture for sending/receiving can messages
private:
#if defined(ARDUINO_ARCH_ESP32)
  static bool twai_receive_task(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx);
#elif defined(CORE_TEENSY)
  static void forward_flexcan(const CAN_message_t &msg);
#else
#error "CAN network library not written for chosen architecture"
#endif

  // Private methods common between architectures
private:
  void send_message(uint32_t id, uint8_t len, const uint8_t *data);
  void on_receive(uint32_t id, uint8_t len, const uint8_t *data);

  // Public entry point
public:
  void init_network(DevBoard board);
  void send_all();
};

extern Vehicle g_vehicle;
extern uint32_t g_recent_id;
extern uint8_t g_recent_data[8];
extern bool g_new_message;