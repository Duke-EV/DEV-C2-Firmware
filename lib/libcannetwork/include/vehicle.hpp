#pragma once
#include <stdint.h>

#if defined(ARDUINO_ARCH_ESP32)
#include "driver/twai.h"
#include "driver/gpio.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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
  // Number of designs this library is using
  BOARD_COUNT
};

class Vehicle {
private:
  DevBoard m_board;

#if defined(ARDUINO_ARCH_ESP32)
  twai_general_config_t m_twai_general;
  twai_timing_config_t m_twai_timing;
  twai_filter_config_t m_twai_filter;
  TaskHandle_t m_twai_receive_task;
#elif defined(CORE_TEENSY)
  // Teensy uses the shared static instance in the source file.
#else
#error "CAN network library not written for chosen architecture"
#endif

public:
  uint32_t m_motor_rpm;
  float m_ground_speed_kph;
  // NOTE: use Raw values if the number on the network might
  // not be representative of a value with a proper unit (this
  // is something you can decide and design) Example would be
  // an ADC value versus a temperature
  uint16_t m_battery_soc_raw;
  float m_battery_soc;
  // TODO: list all known values to be shared

  // Private methods unique to architecture for sending/receiving can messages
private:
#if defined(ARDUINO_ARCH_ESP32)
  static void twai_receive_task(void *arg);
#elif defined(CORE_TEENSY)
  static void forward_flexcan(const CAN_message_t &msg);
#else
#error "CAN network library not written for chosen architecture"
#endif

  // Private methods common between architectures
private:
  void send_message();
  void send_all();
  void on_receive(uint32_t id, uint8_t len, const uint8_t *data);

  // Public entry point
public:
  void init_network(DevBoard board);
};

extern Vehicle g_vehicle;
