#include "vehicle.hpp"
#include <stdint.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <ESP32-TWAI-CAN.hpp>
#ifndef VEHICLE_TWAI_TX_PIN
#define VEHICLE_TWAI_TX_PIN 25
#endif
#ifndef VEHICLE_TWAI_RX_PIN
#define VEHICLE_TWAI_RX_PIN 35
#elif defined(CORE_TEENSY)
#include <FlexCAN_T4.h>
#else
#error "CAN network library note written for chosen architecture"
#endif // defined

Vehicle g_vehicle;
volatile uint8_t g_can_queue_head = 0;
volatile uint8_t g_can_queue_tail = 0;
CANMessage       g_can_queue[CAN_QUEUE_SIZE];

#if defined(CORE_TEENSY)
static FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> s_teensy_can;
#endif

#if defined(ARDUINO_ARCH_ESP32)
static twai_node_handle_t node_hdl;
#endif

#if defined(ARDUINO_ARCH_ESP32)
void Vehicle::init_network(DevBoard board) {
  twai_onchip_node_config_t node_config = {
    .io_cfg.tx = VEHICLE_TWAI_TX_PIN,
    .io_cfg.rx = VEHICLE_TWAI_RX_PIN,
    .bit_timing.bitrate = 500000,
    .tx_queue_depth = 5,
  };

  ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));
  ESP_ERROR_CHECK(twai_node_enable(node_hdl));

  twai_event_callbacks_t callback = {
    .on_rx_done = Vehicle::twai_receive_task,
  };
  ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &callback, NULL));
}

void Vehicle::send_message(uint32_t id, uint8_t len, const uint8_t *data) {
  twai_frame_t tx_msg = {
    .header.id = 0x1,           // Message ID
    .header.ide = true,         // Use 29-bit extended ID format
    .buffer = send_buff,        // Pointer to data to transmit
    .buffer_len = sizeof(send_buff),  // Length of data to transmit
  };

  ESP_ERROR_CHECK(twai_node_transmit(node_hdl, &tx_msg, 0));
  ESP_ERROR_CHECK(twai_node_transmit_wait_all_done(node_hdl, -1));
}

bool Vehicle::twai_receive_task(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx) {
  uint8_t recv_buff[8];
  twai_frame_t rx_frame = {
    .buffer = recv_buff,
    .buffer_len = sizeof(recv_buff),
  };
  if (ESP_OK == twai_node_receive_from_isr(handle, &rx_frame)) {
    uint32_t id = rx_frame.id;
    uint8_t dlc = rx_frame.dlc;
    uint8_t data[8];

    for(int i = 0; i < dlc; i++) {
      data[i] = recv_buff[i];
    }

    g_vehicle.on_receive(id, dlc, data);
  }
  return false;
}
#endif // defined(ARDUINO_ARCH_ESP32)

#if defined(CORE_TEENSY)
void Vehicle::forward_flexcan(const CAN_message_t &msg) {
  uint8_t next = (g_can_queue_tail + 1) % CAN_QUEUE_SIZE;
  if (next != g_can_queue_head) {  // drop if full
    g_can_queue[g_can_queue_tail].id  = msg.id;
    g_can_queue[g_can_queue_tail].len = msg.len;
    memcpy(g_can_queue[g_can_queue_tail].buf, msg.buf, msg.len);
    g_can_queue_tail = next;
  }
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
    g_vehicle.send_message(0x700, 8, heartbeat)
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