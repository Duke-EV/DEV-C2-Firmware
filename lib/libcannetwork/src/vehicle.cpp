#include "vehicle.hpp"
#include <stdint.h>

#if defined(ARDUINO_ARCH_ESP32)
#include "esp_twai.h"
#include "esp_twai_onchip.h"
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
static FlexCAN_T4FD<CAN3, RX_SIZE_256, TX_SIZE_16> s_teensy_can;
#endif#endif

#if defined(ARDUINO_ARCH_ESP32)
void Vehicle::init_network(DevBoard board) {
  m_board = board;
  twai_onchip_node_config_t node_config = {};
  node_config.io_cfg.tx = (gpio_num_t)VEHICLE_TWAI_TX_PIN;
  node_config.io_cfg.rx = (gpio_num_t)VEHICLE_TWAI_RX_PIN;
  node_config.bit_timing.bitrate = 500000;
  node_config.data_timing.bitrate = 2000000; 
  node_config.tx_queue_depth = 10;
  node_config.rx_queue_depth = 10;

  twai_event_callbacks_t user_cbs = {};
  user_cbs.on_rx_done = Vehicle::twai_rx_cb;

  m_last_error = esp_twai_new_onchip_node(&node_config, &m_twai_handle);
  if (m_last_error != ESP_OK) {
    return; 
  }

  m_last_error = esp_twai_node_register_event_callbacks(m_twai_handle, &user_cbs, this);
  if (m_last_error != ESP_OK) {
    return; 
  }

  m_last_error = esp_twai_node_enable(m_twai_handle);
  if (m_last_error != ESP_OK) {
    return; 
  }
}

void Vehicle::send_message(uint32_t id, uint8_t len, const uint8_t *data) {
  twai_frame_t tx_msg = {};

  tx_msg.header.fdf = 1;      // Enable CAN FD (up to 64 bytes)
  tx_msg.header.brs = 1;      // Enable High-Speed bit rate switch
  tx_msg.header.id = id;      // Set the CAN ID
  tx_msg.header.ide = 0;      // 0 for 11-bit Standard ID
  tx_msg.buffer_len = len;    // Set the payload length
  tx_msg.buffer = (uint8_t *)data;

  esp_err_t err = twai_node_transmit(m_twai_handle, &tx_msg, pdMS_TO_TICKS(10));  
  if (err != ESP_OK) {
    m_last_error = err;
  }
}

bool Vehicle::twai_rx_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx) {
  Vehicle *self = static_cast<Vehicle*>(user_ctx);
  uint8_t recv_buff[64];

  twai_frame_t rx_frame = {};
  rx_frame.buffer = recv_buff;
  rx_frame.buffer_len = sizeof(recv_buff);

  if (twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK) {
    self->on_receive(rx_frame.header.id, rx_frame.buffer_len, rx_frame.buffer);
  }
  return false;
}
#endif // defined(ARDUINO_ARCH_ESP32)

#if defined(CORE_TEENSY)
void Vehicle::forward_flexcan(const CANFD_message_t &msg) {
  g_vehicle.on_receive(msg.id, msg.len, msg.buf);
}

void Vehicle::init_network(DevBoard board) {
  m_board = board;
  s_teensy_can.begin();
  s_teensy_can.setRegions(64);
  s_teensy_can.setBaudRateData(2000000);
  s_teensy_can.setBaudRate(500000);
  s_teensy_can.enableFIFO(true);
  s_teensy_can.enableFIFOInterrupt();
  s_teensy_can.onReceive(forward_flexcan);
}

void Vehicle::send_message(uint32_t id, uint8_t len, const uint8_t *data) {
  // TODO: align with ESP32 implementation once message layout exists
  CANFD_message_t msg = {};
  msg.id = id;
  msg.len = len;
  msg.flags.extended = 0;  // Use standard 11-bit IDs (set to 1 for 29-bit extended)
  msg.flags.remote = 0;    // Not a remote frame
  msg.brs = 1;
  msg.edl = 1;
  msg.seq = 0;             // Not using sequential mode

  memcpy(msg.buf, data, len);

  s_teensy_can.write(msg);
}
#endif // defined(CORE_TEENSY)

void Vehicle::send_all() {
  switch (m_board) {
  case PERIPHERALS: {
    MsgPeripherals payload = {};
    payload.heartbeat = 1;
    payload.windshield = g_vehicle.m_peripherals_windshield;
    payload.backrunninglights = g_vehicle.m_peripherals_backrunninglights;
    payload.turn = g_vehicle.m_peripherals_turn;
    payload.headlights = g_vehicle.m_peripherals_headlights;
    payload.brakelights = g_vehicle.m_peripherals_brakelights;
    payload.hazard = g_vehicle.m_peripherals_hazard;

    g_vehicle.send_message(0x100, sizeof(payload), (const uint8_t*)&payload);
    break;
  }
  
  case POWER_DISTRIBUTION: {
    MsgPDB payload = {};
    payload.heartbeat = 1;
    payload.current = g_vehicle.m_pdb_current;
    payload.voltage = g_vehicle.m_pdb_voltage;
    payload.motor_enabled = g_vehicle.m_pdb_motor_enabled;
    
    g_vehicle.send_message(0x200, sizeof(payload), (const uint8_t*)&payload);
    break;
  }

  case MOTOR_CONTROLLER: {
    MsgMotor payload = {};
    payload.heartbeat = 1;
    payload.rpm = g_vehicle.m_motor_rpm;

    g_vehicle.send_message(0x300, sizeof(payload), (const uint8_t*)&payload);
    break;
  }
    
  case THROTTLE: {
    MsgThrottle payload = {};
    payload.heartbeat = 1;
    payload.raw = g_vehicle.m_throttle_raw;
    payload.average = g_vehicle.m_throttle_average;

    g_vehicle.send_message(0x400, sizeof(payload), (const uint8_t*)&payload);
    break;
  }
    
  case JOULEMETER: {
    MsgJoulemeter payload = {};
    payload.heartbeat = 1;
    payload.current = g_vehicle.m_joulemeter_current;
    payload.voltage = g_vehicle.m_joulemeter_voltage;
    payload.energy = g_vehicle.m_joulemeter_energy;
    
    g_vehicle.send_message(0x500, sizeof(payload), (const uint8_t*)&payload);
    break;
  }

  case COMMUNICATIONS:
  case TEST_BOARD: {
    uint8_t heartbeat = 1;
    uint32_t id = (m_board == COMMUNICATIONS) ? 0x600 : 0x700;
    g_vehicle.send_message(id, 1, &heartbeat);
    break;
  }
  
  default:
    break;
  }
}

void Vehicle::on_receive(uint32_t id, uint8_t len, const uint8_t *data) {
  switch (id) {
  case 0x100: {
    if (len >= sizeof(MsgPeripherals)) {
      const MsgPeripherals* payload = (const MsgPeripherals*)data;
      g_vehicle.m_peripherals_windshield = payload->windshield;
      g_vehicle.m_peripherals_backrunninglights = payload->backrunninglights;
      g_vehicle.m_peripherals_turn = payload->turn;
      g_vehicle.m_peripherals_headlights = payload->headlights;
      g_vehicle.m_peripherals_brakelights = payload->brakelights;
      g_vehicle.m_peripherals_hazard = payload->hazard;
    }
    break;
  }
  
  case 0x200: {
    if (len >= sizeof(MsgPDB)) {
      const MsgPDB* payload = (const MsgPDB*)data;
      g_vehicle.m_pdb_current = payload->current;
      g_vehicle.m_pdb_voltage = payload->voltage;
      g_vehicle.m_pdb_motor_enabled = payload->motor_enabled;
    }
    break;
  }
  
  case 0x300: {
    if (len >= sizeof(MsgMotor)) {
      const MsgMotor* payload = (const MsgMotor*)data;
      g_vehicle.m_motor_rpm = payload->rpm;
    }
    break;
  }
  
  case 0x400: {
    if (len >= sizeof(MsgThrottle)) {
      const MsgThrottle* payload = (const MsgThrottle*)data;
      g_vehicle.m_throttle_raw = payload->raw;
      g_vehicle.m_throttle_average = payload->average;
    }
    break;
  }
  
  case 0x500: {
    if (len >= sizeof(MsgJoulemeter)) {
      const MsgJoulemeter* payload = (const MsgJoulemeter*)data;
      g_vehicle.m_joulemeter_current = payload->current;
      g_vehicle.m_joulemeter_voltage = payload->voltage;
      g_vehicle.m_joulemeter_energy = payload->energy;
    }
    break;
  }

  default:
    break;
  }
}
