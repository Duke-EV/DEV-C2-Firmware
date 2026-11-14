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
#include <IntervalTimer.h>
#include <util/atomic.h>
#else
#error "CAN network library note written for chosen architecture"
#endif // defined

Vehicle g_vehicle;

#if defined(CORE_TEENSY)
static FlexCAN_T4FD<CAN3, RX_SIZE_256, TX_SIZE_16> s_teensy_can;
static IntervalTimer s_can_timer;
static const uint8_t CAN_QUEUE_SIZE = 32;
static CANMessage s_can_queue[CAN_QUEUE_SIZE];
static volatile uint8_t s_can_queue_head = 0;
static volatile uint8_t s_can_queue_tail = 0;
#endif

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
  if (msg.len > sizeof(msg.buf)) return;
  uint8_t next = (s_can_queue_tail + 1) % CAN_QUEUE_SIZE;
  if (next != s_can_queue_head) {  // Drop the raw frame if the queue is full.
    s_can_queue[s_can_queue_tail].id = msg.id;
    s_can_queue[s_can_queue_tail].len = msg.len;
    memcpy(s_can_queue[s_can_queue_tail].buf, msg.buf, msg.len);
    s_can_queue_tail = next;
  }
  g_vehicle.on_receive(msg.id, msg.len, msg.buf);
}

bool Vehicle::read_message(CANMessage &msg) {
  // Restore the previous interrupt state, even if the caller disabled interrupts.
  uint32_t interrupt_state = __get_primask();
  __disable_irq();
  if (s_can_queue_head == s_can_queue_tail) {
    __set_primask(interrupt_state);
    return false;
  }
  msg = s_can_queue[s_can_queue_head];
  s_can_queue_head = (s_can_queue_head + 1) % CAN_QUEUE_SIZE;
  __set_primask(interrupt_state);
  return true;
}

void Vehicle::service_network() {
  s_teensy_can.events();
}

void Vehicle::init_network(DevBoard board) {
  m_board = board;
  m_network_ready = false;
  s_can_timer.end();
  s_teensy_can.begin();
  CANFD_timings_t config;
  config.baudrate = 500000;
  config.baudrateFD = 2000000;
  m_network_error = CAN_NETWORK_TIMING_ERROR;
  if (!s_teensy_can.setBaudRate(config)) return;
  s_teensy_can.setRegions(64);
  s_teensy_can.onReceive(forward_flexcan);
  // Queue received frames for processing by the library's timer.
  s_teensy_can.events();
  m_network_error = CAN_NETWORK_TIMER_ERROR;
  if (!s_can_timer.begin(service_network, 1000)) return;
  m_network_ready = true;
  m_network_error = CAN_NETWORK_OK;
  s_teensy_can.enableMBInterrupts();
}

void Vehicle::send_message(uint32_t id, uint8_t len, const uint8_t *data) {
  CANFD_message_t msg = {};
  if (!m_network_ready) {
    return;  // Preserve the initialization error for the application to report.
  }
  if (id > 0x7FF || len > sizeof(msg.buf) || (len > 0 && data == nullptr)) {
    m_network_error = CAN_NETWORK_INVALID_MESSAGE;
    return;
  }

  msg.id = id;
  msg.len = len;
  // CAN FD uses fixed payload sizes above eight bytes. The buffer starts at zero.
  if (len > 48) msg.len = 64;
  else if (len > 32) msg.len = 48;
  else if (len > 24) msg.len = 32;
  else if (len > 20) msg.len = 24;
  else if (len > 16) msg.len = 20;
  else if (len > 12) msg.len = 16;
  else if (len > 8) msg.len = 12;

  msg.flags.extended = 0;  // Use standard 11-bit IDs (set to 1 for 29-bit extended)
  msg.brs = 1;
  msg.edl = 1;
  msg.seq = 0;             // Not using sequential mode

  if (len > 0) {
    memcpy(msg.buf, data, len);
  }

  // Acceptance by the driver does not confirm delivery on the physical bus.
  if (!s_teensy_can.write(msg)) {
    m_network_error = CAN_NETWORK_TX_REJECTED;
    return;
  }
  m_network_error = CAN_NETWORK_OK;
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
