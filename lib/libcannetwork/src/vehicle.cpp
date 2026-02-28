#include "vehicle.hpp"
#include <stdint.h>

#if defined(ARDUINO_ARCH_ESP32)
#include "driver/twai.h"
#include "esp_err.h"
#elif defined(CORE_TEENSY)
#include <FlexCAN_T4.h>
#else
#error "CAN network library note written for chosen architecture"
#endif // defined

Vehicle g_vehicle;

#if defined(CORE_TEENSY)
static FlexCAN_T4<CAN3> s_teensy_can;
#endif

#if defined(ARDUINO_ARCH_ESP32)
void Vehicle::init_network(DevBoard board) {
  m_board = board;
  // TODO: TWAI driver
}

void Vehicle::send_message(uint32_t id, uint8_t len, const uint8_t *data) {
  // TODO: implement TWAI transmit once message definitions are known
}

void Vehicle::twai_receive_task(void *arg) {
  Vehicle *self = static_cast<Vehicle *>(arg);
  twai_message_t message;
  while (true) {
    if (twai_receive(&message, portMAX_DELAY) == ESP_OK) {
      self->on_receive(message.identifier, message.data_length_code,
                       message.data);
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
  s_teensy_can.onReceive(forward_flexcan);
}

void Vehicle::send_message(uint32_t id, uint8_t len, const uint8_t *data) {
  // TODO: align with ESP32 implementation once message layout exists
}
#endif // defined(CORE_TEENSY)

void Vehicle::send_all() {
  // TODO:
  // construct a stack array of bytes for each message being sent
  // send_message() for the ID and data necessary
  // repeat for all messages defined for this board
  switch (m_board) {
  case COMMUNICATIONS:
    // TODO:
    // construct a stack array of bytes for the message being sent
    // send_message() for the ID and data necessary
    // repeat for all messages defined for this board
    break;
    
  case PERIPHERALS:
    uint8_t* data = new uint8_t[8];
    data[0] = g_vehicle.m_peripherals_windshield;
    data[1] = g_vehicle.m_peripherals_backrunninglights;
    data[2] = g_vehicle.m_peripherals_turn;
    data[3] = g_vehicle.m_peripherals_headlights;
    data[4] = g_vehicle.m_peripherals_brakelights;
    data[5] = g_vehicle.m_peripherals_hazard;

    g_vehicle.send_message(256, 8, data);
    break;
    
  case MOTOR_CONTROLLER:
    uint8_t rpm_high_byte = (g_vehicle.m_motor_rpm >> 24) & 0xFF;
    uint8_t rpm_second_byte = (g_vehicle.m_motor_rpm >> 16) & 0xFF;
    uint8_t rpm_third_byte = (g_vehicle.m_motor_rpm >> 8) & 0xFF;
    uint8_t rpm_low_byte = g_vehicle.m_motor_rpm & 0xFF;

    uint8_t* data = new uint8_t[8];

    data[0] = rpm_high_byte;
    data[1] = rpm_second_byte;
    data[2] = rpm_third_byte;
    data[3] = rpm_low_byte;

    g_vehicle.send_message(768, 8, data);
    break;
  case POWER_DISTRIBUTION:
    uint8_t current_high_byte = (g_vehicle.m_pdb_current >> 8) & 0xFF;
    uint8_t current_low_byte = g_vehicle.m_pdb_current & 0xFF;
    uint8_t voltage_high_byte = (g_vehicle.m_pdb_voltage >> 8) & 0xFF;
    uint8_t voltage_low_byte = g_vehicle.m_pdb_voltage & 0xFF;
    
    uint8_t* data = new uint8_t[8];
    data[0] = current_high_byte;
    data[1] = current_low_byte;
    data[2] = voltage_high_byte;
    data[3] = voltage_low_byte;
    
    g_vehicle.send_message(1280, 8, data);
    break;
    
  case THROTTLE:
    uint8_t high_byte = (g_vehicle.m_throttle_percentage >> 8) & 0xFF;
    uint8_t low_byte = g_vehicle.m_throttle_percentage & 0xFF;

    uint8_t* data = new uint8_t[8];
    data[0] = high_byte;
    data[1] = low_byte;

    g_vehicle.send_message(1024, 8, data);
    break;
    
  case JOULEMETER:
    uint8_t current_high_byte = (g_vehicle.m_joulemeter_current >> 8) & 0xFF;
    uint8_t current_low_byte = g_vehicle.m_joulemeter_current & 0xFF;
    
    uint8_t voltage_high_byte = (g_vehicle.m_joulemeter_voltage >> 8) & 0xFF;
    uint8_t voltage_low_byte = g_vehicle.m_joulemeter_voltage & 0xFF;

    uint8_t energy_high_byte = (g_vehicle.m_joulemeter_energy >> 24) & 0xFF;
    uint8_t energy_second_byte = (g_vehicle.m_joulemeter_energy >> 16) & 0xFF;
    uint8_t energy_third_byte = (g_vehicle.m_joulemeter_energy >> 8) & 0xFF;
    uint8_t energy_low_byte = g_vehicle.m_joulemeter_energy & 0xFF;
    
    uint8_t* data = new uint8_t[8];
    data[0] = current_high_byte;
    data[1] = current_low_byte;
    data[2] = voltage_high_byte;
    data[3] = voltage_low_byte;
    data[4] = energy_high_byte;
    data[5] = energy_second_byte;
    data[6] = energy_third_byte;
    data[7] = energy_low_byte;
    
    g_vehicle.send_message(1280, 8, data);
    break;
    
  default:
    // should never reach here
    break;
  }
}

void Vehicle::on_receive(uint32_t id, uint8_t len, const uint8_t *data) {
  switch (id) {
  case 0x100:
    // TODO: parse the data to the member variables
    // ex:
    // m_motor_rpm = data[0] << 8 | data[1];
    // NOTE: This may be more complicated than you'd expect depending on
    // how the data was packed into the message. Balance out the efficiency
    // of packing many signals into a message versus the simple inperpretation
    // of the bytes themselves. This will be extra important for packing of 1
    // bit signals like switches or unpacking multibyte signals like 12 bit
    // temperature sensors. ENDIANNESS MATTERS! DO NOT ASSUME BYTES ARE PACKED
    // AS EXPECTED BETWEEN ARCHITECUTURES BAD EXAMPLE:
    // // sending side
    // uint32_t my_motor_rpm = 2048
    // send(0x100, 8, (char*)&my_motor_rpm); //EVIL
    // //receiving side
    // my_motor_rpm = *data; //UNKNOWN ENDIANNESS
    break;
  case 0x101:
    switch (data[0]) {
    case 0:
      break;
    case 1:
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}
