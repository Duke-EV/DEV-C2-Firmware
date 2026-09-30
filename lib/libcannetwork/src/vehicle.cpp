#include "vehicle.hpp"
#include <stdint.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <ESP32-TWAI-CAN.hpp>
#ifndef VEHICLE_TWAI_TX_PIN
#define VEHICLE_TWAI_TX_PIN 33
#endif
#ifndef VEHICLE_TWAI_RX_PIN
#define VEHICLE_TWAI_RX_PIN 34
#endif
#elif defined(CORE_TEENSY)
#include <FlexCAN_T4.h>
#else
#error "CAN network library note written for chosen architecture"
#endif // defined

Vehicle g_vehicle;

#if defined(CORE_TEENSY)
static FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> s_teensy_can;
#endif

#if defined(ARDUINO_ARCH_ESP32)
void Vehicle::init_network(DevBoard board) {
  m_board = board;
  ESP32Can.setPins(VEHICLE_TWAI_TX_PIN, VEHICLE_TWAI_RX_PIN);
  ESP32Can.setTxQueueSize(10);
  m_last_error = ESP32Can.begin(ESP32Can.convertSpeed(500)) ? ESP_OK : ESP_FAIL;
  if (m_last_error != ESP_OK) {
    return;
  }

  xTaskCreate(Vehicle::twai_receive_task, "twai_rx", 4096, this, 5, nullptr);
}

void Vehicle::send_message(uint32_t id, uint8_t len, const uint8_t *data) {
  CanFrame frame = {0};
  frame.identifier       = id;
  frame.extd             = 0;
  frame.data_length_code = len;

  memcpy(frame.data, data, len);

  ESP32Can.writeFrame(frame);
}

void Vehicle::twai_receive_task(void *args) {
  Vehicle *self = static_cast<Vehicle*>(args);
  CanFrame frame;
  while (true) {
      if (ESP32Can.readFrame(frame, 100)) {
          uint8_t data[8];
          memcpy(data, frame.data, 8);
          self->on_receive(frame.identifier, frame.data_length_code, data);
      }
  }
}
#endif // defined(ARDUINO_ARCH_ESP32)

#if defined(CORE_TEENSY)
void Vehicle::forward_flexcan(const CAN_message_t &msg) {
  g_vehicle.on_receive(msg.id, msg.len, msg.buf);
}

void Vehicle::init_network(DevBoard board) {
  m_board = board;
  s_teensy_can.begin();
  s_teensy_can.setBaudRate(500000);
  s_teensy_can.enableFIFO(true);
  s_teensy_can.enableFIFOInterrupt();
  s_teensy_can.onReceive(forward_flexcan);
}

void Vehicle::send_message(uint32_t id, uint8_t len, const uint8_t *data) {
  // TODO: align with ESP32 implementation once message layout exists
  CAN_message_t msg;
  msg.id = id;
  msg.len = len;
  msg.flags.extended = 0;  // Use standard 11-bit IDs (set to 1 for 29-bit extended)
  msg.flags.remote = 0;    // Not a remote frame
  msg.seq = 0;             // Not using sequential mode

  memcpy(msg.buf, data, len);

  s_teensy_can.write(msg);
}
#endif // defined(CORE_TEENSY)

void Vehicle::send_all() {
  uint8_t data[8];
  uint8_t heartbeat[8];
  heartbeat[0] = 1;
  switch (m_board) {
  case PERIPHERALS:
    data[0] = g_vehicle.m_peripherals_windshield;
    data[1] = g_vehicle.m_peripherals_backrunninglights;
    data[2] = g_vehicle.m_peripherals_turn;
    data[3] = g_vehicle.m_peripherals_headlights;
    data[4] = g_vehicle.m_peripherals_brakelights;
    data[5] = g_vehicle.m_peripherals_hazard;

    g_vehicle.send_message(0x100, 8, data);
    g_vehicle.send_message(0x101, 8, heartbeat);
    break;
  
  case POWER_DISTRIBUTION:
    data[0] = (g_vehicle.m_pdb_current >> 8) & 0xFF;
    data[1] = g_vehicle.m_pdb_current & 0xFF;
    data[2] = (g_vehicle.m_pdb_voltage >> 8) & 0xFF;
    data[3] = g_vehicle.m_pdb_voltage & 0xFF;
    data[4] = g_vehicle.m_pdb_motor_enabled;
    
    g_vehicle.send_message(0x200, 8, data);
    g_vehicle.send_message(0x201, 8, heartbeat);
    break;

  case MOTOR_CONTROLLER:
    data[0] = (g_vehicle.m_motor_rpm >> 24) & 0xFF;
    data[1] = (g_vehicle.m_motor_rpm >> 16) & 0xFF;
    data[2] = (g_vehicle.m_motor_rpm >> 8) & 0xFF;
    data[3] = g_vehicle.m_motor_rpm & 0xFF;

    g_vehicle.send_message(0x300, 8, data);
    g_vehicle.send_message(0x301, 8, heartbeat);
    break;
    
  case THROTTLE:
    data[0] = (g_vehicle.m_throttle_raw >> 8) & 0xFF;
    data[1] = g_vehicle.m_throttle_raw & 0xFF;
    data[2] = (g_vehicle.m_throttle_average >> 8) & 0xFF;
    data[3] = g_vehicle.m_throttle_average & 0xFF;

    g_vehicle.send_message(0x400, 8, data);
    g_vehicle.send_message(0x401, 8, heartbeat);
    break;
    
  case JOULEMETER:
    data[0] = (g_vehicle.m_joulemeter_current >> 8) & 0xFF;
    data[1] = g_vehicle.m_joulemeter_current & 0xFF;
    data[2] = (g_vehicle.m_joulemeter_voltage >> 8) & 0xFF;
    data[3] = g_vehicle.m_joulemeter_voltage & 0xFF;
    data[4] = (g_vehicle.m_joulemeter_energy >> 24) & 0xFF;
    data[5] = (g_vehicle.m_joulemeter_energy >> 16) & 0xFF;
    data[6] = (g_vehicle.m_joulemeter_energy >> 8) & 0xFF;
    data[7] = g_vehicle.m_joulemeter_energy & 0xFF;
    
    g_vehicle.send_message(0x500, 8, data);
    g_vehicle.send_message(0x501, 8, heartbeat);
    break;

  case COMMUNICATIONS:
    g_vehicle.send_message(0x600, 8, heartbeat);
    break;

  case TEST_BOARD:
    g_vehicle.send_message(0x700, 8, heartbeat);
    break;

  default:
    // should never reach here
    break;
  }
}

void Vehicle::on_receive(uint32_t id, uint8_t len, const uint8_t *data) {
  switch (id) {
  case 0x100:
    g_vehicle.m_peripherals_windshield = data[0];
    g_vehicle.m_peripherals_backrunninglights = data[1];
    g_vehicle.m_peripherals_turn = data[2];
    g_vehicle.m_peripherals_headlights = data[3];
    g_vehicle.m_peripherals_brakelights = data[4];
    g_vehicle.m_peripherals_hazard = data[5];
    break;
  case 0x101:
    // Contains software versions, not useful
    break;
  case 0x200:
    g_vehicle.m_pdb_current = (data[0] << 8) | data[1];
    g_vehicle.m_pdb_voltage = (data[2] << 8) | data[3];
    g_vehicle.m_pdb_motor_enabled = data[4];
    break;
  case 0x201:
    // Contains software versions, not useful
    break;
  case 0x300:
    g_vehicle.m_motor_rpm = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
    break;
  case 0x301:
    // Contains software versions, not useful
    break;
  case 0x400:
    g_vehicle.m_throttle_raw = (data[0] << 8) | data[1];
    g_vehicle.m_throttle_average = (data[2] << 8) | data[3];
    break;
  case 0x401:
    // Contains software versions, not useful
    break;
  case 0x500:
    g_vehicle.m_joulemeter_current = (data[0] << 8) | data[1];
    g_vehicle.m_joulemeter_voltage = (data[2] << 8) | data[3];
    g_vehicle.m_joulemeter_energy = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | (data[7]);
    break;
  case 0x501:
    // Contains software versions, not useful
    break;
  case 0x600:
    // Contains software versions, not useful
    break;
  default:
    break;
  }
}
