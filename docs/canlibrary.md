# Unified CAN Library

## Installation
In `./boards/YOUR_BOARD_HERE`, put ../../lib/libcannetwork in the library dependencies. Then, the library is installed. If the library is updated, you will need to update it in your individual project too.

## Usage

The CanLibrary contains a `g_vehicle` variable, which contains the state of the vehicle which constantly updates as information comes in from the CAN bus. The intention of this library is that the necessity to implement in each board is minimal. When the CAN Network is initialized, you can define which board you are from the `DevBoard` enum, which contains:

```cpp
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
```

You can access `g_vehicle` states directly using its public fields. No initialization of `g_vehicle` is required, it is included and exposed when you include the library. The following variables exist in the `g_vehicle` state:

```cpp
uint8_t m_peripherals_windshield; // 0 OR 1: off or on
uint8_t m_peripherals_backrunninglights; // 0 OR 1: off or on
uint8_t m_peripherals_turn; // 0, 1, OR 2 (off, left turn, right turn)
uint8_t m_peripherals_headlights; // 0 OR 1: off or on
uint8_t m_peripherals_brakelights; // 0 OR 1: off or on
uint8_t m_peripherals_hazard; // 0 OR 1: off or on

uint16_t m_pdb_current; // Current in milliamps. Please note the variable type
uint16_t m_pdb_voltage; // Voltage in millivolts. Please note the variable type

uint32_t m_motor_rpm; //RPM (Rotations per minute) of the motor, scaled as m_motor_rpm = 1000 * true_rpm

uint16_t m_throttle_percentage; // Percentage of throttle, scaled as m_throttle_percentage = 10 * true_percentage

uint16_t m_joulemeter_current; // Current in milliamps. Please note the variable type
uint16_t m_joulemeter_voltage; // Voltage in millivolts. Please note the variable type
uint32_t m_joulemeter_energy; // Accumulated energy in millijoules. Please note the variable type
```

**IMPORTANT**: You should update variables related to your board constantly. For example, if the throttle board updates its percentage, it should update the percentage variable. You should not modify variables in other boards, but you should access them for logic. Please adhere to the data types in the CAN library. Further implementation details are available [here](https://docs.google.com/spreadsheets/d/10I3f32omGpMJWg03m_2nzKF5BjdreAMC2nzWPjXydf4/edit?gid=1869009184#gid=1869009184).

### ESP32
This is not yet tested for ESP32 boards.

### Teensy 4.1
Once the library is written first include the vehicle header:

```cpp
#include <vehicle.hpp>
```

Next, add a wrapper to create a timer:

```cpp
IntervalTimer timer;
void send_all_wrapper() {
  g_vehicle.send_all();
}
```
Then in `setup()`, include the following lines:

```cpp
g_vehicle.init_network(DevBoard::**YOUR_BOARD_HERE**);
timer.begin(send_all_wrapper, 100000); // Send on a 100 ms timer. Update if needed.
```