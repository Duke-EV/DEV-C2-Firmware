#pragma once

#include <stdint.h>

class Vehicle {
public:
  uint8_t m_peripherals_windshield;
  uint8_t m_peripherals_backrunninglights;
  uint8_t m_peripherals_turn;
  uint8_t m_peripherals_headlights;
  uint8_t m_peripherals_brakelights;
  uint8_t m_peripherals_hazard;
  uint16_t m_pdb_current;
  uint16_t m_pdb_voltage;
  uint8_t m_pdb_motor_enabled;
  uint32_t m_motor_rpm;
  uint16_t m_throttle_raw;
  uint16_t m_throttle_average;
  uint16_t m_joulemeter_current;
  uint16_t m_joulemeter_voltage;
  uint32_t m_joulemeter_energy;
};

extern Vehicle g_vehicle;

