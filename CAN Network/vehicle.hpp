// vehicle.hpp
// Single-header "library" for CAN messaging across multiple MCUs.
// - Put this file in a shared repo or submodule and include in all board projects.
// - Compile each board with ONE of: -DMC_BOARD, -DPERI_BOARD, -DTHR_BOARD, -DBEM_BOARD, -DCOMMS_BOARD, -DPD_BOARD, ...
// - Provide (or use provided) platform adapter that implements vehicle::ICAN.
// - Adapter must call vehicle::isrEnqueue() when receiving from ISR/callback.
// - Call vehicle::processRxQueue() periodically from main loop/task to run decoding and handlers.
//
// Usage summary:
//   vehicle::init(adapterPtr);             // adapter implements ICAN
//   vehicle::registerRxHandler(handler);   // optional: receive ALL frames
//   vehicle::sendHeartbeat(vehicle::ID_MY_HEARTBEAT); // use pre-defined IDs
//   // owners only (via compile flag):
//   vehicle::setThrottleRaw(123);
//   vehicle::sendThrottle();                // sends the pre-packed message
//   // readers:
//   auto t = vehicle::getThrottleRaw();
//

#pragma once //avoids initialization errors, only includes file onece
#include <cstdint>
#include <cstring>
#include <functional> //allows functions definition; callback
#include <atomic> //allows "global" variables that can be read/written from multiple mcus

namespace vehicle { //wraps code so that methods don't collide (instead of class bc only wnat one vehicle state and don't want to pass obj everywhere)

//vehicle ids
using CanId = uint32_t;

enum : CanId {
  ID_PERI            = 0x100, //peripherals heartbeat
  ID_PERI_TO_BEM     = 0x110,
  ID_PERI_TO_COMMS   = 0x120,
  ID_PERI_TO_MC      = 0x130,

  ID_PDB             = 0x200, //PDB heartbeat

  ID_MOTOR           = 0x300, //motor controller heartbeat

  ID_THR             = 0x400, //throttle heartbeat
  ID_THR_TO_MOTOR    = 0x410,

  ID_BEM             = 0x500, //battery entry module heartbeat

  ID_EMERGENCY       = 0x001, //emergency stop
};

static constexpr uint32_t HEARTBEAT_HZ = 10;
static constexpr uint32_t HEARTBEAT_PERIOD_MS = (1000u / HEARTBEAT_HZ);
static constexpr uint32_t HEARTBEAT_TIMEOUT_MS = HEARTBEAT_PERIOD_MS * 3;

#pragma pack(push,1)
struct ThrottleMsg {
  uint16_t throttle_raw; // 0..1000
  uint8_t  status;       // bitflags
  uint8_t  seq;         //sequence number; incremented to debug old/new data
};
struct MotorTelemetry {
  uint16_t rpm;
  uint16_t current_mA;
  int8_t   tempC;
  uint8_t  status;
};
struct PeriState {
  uint8_t lights_mask; // bitfields
  uint8_t brakelight;
  uint8_t reserved;
  uint8_t seq;
};
struct HeartbeatMsg {
  uint8_t node_type;
  uint8_t fw_major;
  uint8_t fw_minor;
  uint8_t status;
};
#pragma pack(pop)

static_assert(sizeof(ThrottleMsg) <= 8, "Throttle must fit CAN 8 bytes");
static_assert(sizeof(MotorTelemetry) <= 8, "Motor telemetry must fit CAN 8 bytes");
static_assert(sizeof(PeriState) <= 8, "Peri state must fit CAN 8 bytes");



// Abstract CAN adapter interface
// Implement one per-platform and pass pointer to vehicle::init(...)
struct ICAN {
  virtual ~ICAN() = default;
  virtual bool begin(uint32_t baud) = 0;
  // Blocking send: returns true on success
  virtual bool send(CanId id, const uint8_t* data, uint8_t len, uint32_t timeout_ms = 0) = 0;
  // Optional: non-blocking polling receive for adapter implementations that don't have ISR callback
  virtual bool receive(CanId &id_out, uint8_t* buf, uint8_t &len_out, uint32_t timeout_ms = 0) = 0;
};


// Public API: init / raw send / handler reg
extern ICAN* g_can; // set by init

inline void init(ICAN* adapter) {
  g_can = adapter;
  if(g_can) g_can->begin(500000);
}
using RxHandler = std::function<void(CanId id, const uint8_t* data, uint8_t len)>;
// Called by user to register an optional app-wide handler.
// Vehicle library calls this after decoding each frame (in processRxQueue)
void registerRxHandler(RxHandler h);

// Raw send helper (all boards)
inline bool sendRaw(CanId id, const uint8_t* data, uint8_t len) {
  if (!g_can) return false;
  return g_can->send(id, data, len);
}

// Heartbeat helper (all boards may call)
inline void sendHeartbeat(CanId hb_id) {
  HeartbeatMsg hb{};
#if defined(MC_BOARD)
  hb.node_type = 1;
#elif defined(PERI_BOARD)
  hb.node_type = 2;
#elif defined(THR_BOARD)
  hb.node_type = 3;
#elif defined(BEM_BOARD)
  hb.node_type = 4;
#elif defined(COMMS_BOARD)
  hb.node_type = 5;
#else
  hb.node_type = 0xFF;
#endif
  hb.fw_major = 1;
  hb.fw_minor = 0;
  hb.status = 0;
  sendRaw(hb_id, reinterpret_cast<const uint8_t*>(&hb), sizeof(hb));
}



// Owned variables (single owner only) and getters/setters
// Implementation notes:
//  - Use std::atomic for single-word values to ensure safe concurrent access between ISR/main.
//  - For multi-field structs, we provide a versioned update pattern (seq/version) to avoid torn reads.
extern std::atomic<uint16_t> s_motor_rpm;      // owner: MC_BOARD
extern std::atomic<uint16_t> s_throttle_raw;   // owner: THR_BOARD
extern std::atomic<uint8_t>  s_back_light;     // owner: PERI_BOARD (bitmask)
extern std::atomic<uint16_t> s_current_mA;     // owner: MC_BOARD
extern std::atomic<uint8_t> s_back_right_blinker; // owner: PERI_BOARD (bitmask)
extern std::atomic<uint8_t> s_back_left_blinker;  // owner: PERI_BOARD (bitmask)

// getters (available to all)
inline uint16_t getMotorRpm()    { return s_motor_rpm.load(); }
inline uint16_t getThrottleRaw() { return s_throttle_raw.load(); }
inline uint8_t  getBackLight()   { return s_back_light.load(); }
inline uint16_t getCurrent_mA()  { return s_current_mA.load(); }

// setters only compiled for owners
#ifdef MC_BOARD
inline void setMotorRpm(uint16_t v)   { s_motor_rpm.store(v); }
inline void setCurrent_mA(uint16_t v) { s_current_mA.store(v); }
#endif

#ifdef THR_BOARD
inline void setThrottleRaw(uint16_t v) { s_throttle_raw.store(v); }
#endif

#ifdef PERI_BOARD
inline void setBackLight(uint8_t v) { s_back_light.store(v); }
#endif

// Convenience senders — owners only: these pack the current owned values into messages and transmit.
// Owners should call the appropriate sendXXX() at their desired telemetry/command rate.

#ifdef MC_BOARD
inline bool sendMotorTelemetry() {
  MotorTelemetry mt{};
  mt.rpm = static_cast<uint16_t>(s_motor_rpm.load());
  mt.current_mA = static_cast<uint16_t>(s_current_mA.load());
  mt.tempC = 0; // fill if you have temp sensor
  mt.status = 0;
  return sendRaw(ID_MOTOR_TELEMETRY, reinterpret_cast<const uint8_t*>(&mt), sizeof(mt));
}
#endif

#ifdef THR_BOARD
// throttle owner keeps an internal seq for message freshness
inline bool sendThrottle(uint8_t seq) {
  ThrottleMsg t{};
  t.throttle_raw = static_cast<uint16_t>(s_throttle_raw.load());
  t.status = 0;
  t.seq = seq;
  return sendRaw(ID_THR_TO_MC, reinterpret_cast<const uint8_t*>(&t), sizeof(t));
}
#endif

#ifdef PERI_BOARD
inline bool sendPeriState(uint8_t seq) {
  PeriState p{};
  p.lights_mask = static_cast<uint8_t>(s_back_light.load());
  p.brakelight = 0;
  p.reserved = 0;
  p.seq = seq;
  return sendRaw(ID_PERI_TO_COMMS, reinterpret_cast<const uint8_t*>(&p), sizeof(p));
}
#endif


// ISR-safe receive queueing and processing
// - Adapters (ISR/callback) should call vehicle::isrEnqueue() with a CanFrame
// - Main loop should call vehicle::processRxQueue() frequently (e.g. every 10-20 ms)
struct CanFrame {
  CanId id;
  uint8_t len;
  uint8_t data[8];
};

// These functions are implemented below in the header (single-file library style)
void isrEnqueue(const CanFrame& f); // called by adapter ISR (must be very fast)
void processRxQueue();              // called by main loop; decodes canonical IDs and calls registered handler


// Default decoding callback: updates s_* atomics and lastSeen times
// Also provides a simple heartbeat last-seen map (small fixed-size)
static constexpr int MAX_NODE_HEARTS = 12;
struct HeartEntry { CanId id; uint32_t last_seen_ms; uint8_t node_type; };
extern HeartEntry g_heartbeats[MAX_NODE_HEARTS];

// Utility: user must provide millis() function for platform or we fallback to a simple stub.
// For Teensy (Arduino-style) provide extern "C" unsigned long millis(); For ESP32, you usually have millis() too.
// If you don't, the adapter file should provide a vehicle_millis() function.
unsigned long vehicle_millis(); // must be provided by user or adapter; default weak fallback below if not defined.

} // namespace vehicle



// Implementation section (still in header for convenience)
#ifndef VEHICLE_HEADER_IMPL_GUARD
#define VEHICLE_HEADER_IMPL_GUARD

#include <array>

namespace vehicle {

// statics
ICAN* g_can = nullptr;
static RxHandler g_handler = nullptr;

// atomics (storage)
std::atomic<uint16_t> s_motor_rpm{0};
std::atomic<uint16_t> s_throttle_raw{0};
std::atomic<uint8_t>  s_back_light{0};
std::atomic<uint16_t> s_current_mA{0};

// heartbeat table
HeartEntry g_heartbeats[MAX_NODE_HEARTS] = {};

// simple ring queue (power-of-two size for easy wrap)
static constexpr int RX_Q_SZ = 64;
static CanFrame rx_q[RX_Q_SZ];
static volatile uint32_t rx_q_head = 0;
static volatile uint32_t rx_q_tail = 0;

void registerRxHandler(RxHandler h) { g_handler = h; }

// Default stub for millis() if platform doesn't provide - adapters should supply one otherwise
#ifndef VEHICLE_HAS_MILLIS
inline unsigned long vehicle_millis() {
  // warning: if not replaced, this returns zero on platforms without millis()
  return 0u;
}
#endif

inline void isrEnqueue(const CanFrame& f) {
  // Very fast: just write into queue if space. No locks, a single producer (ISR) is assumed.
  uint32_t head = (rx_q_head + 1) & (RX_Q_SZ - 1);
  if (head == rx_q_tail) {
    // queue full: drop frame. Could set an overflow flag here.
    return;
  }
  rx_q[rx_q_head] = f; // POD copy
  // Ensure write ordering: critical on some archs; volatile writes are used for head/tail
  rx_q_head = head;
}

inline void processRxQueue() {
  // Drain queue (consumer context = main loop)
  while (rx_q_tail != rx_q_head) {
    CanFrame f = rx_q[rx_q_tail];
    rx_q_tail = (rx_q_tail + 1) & (RX_Q_SZ - 1);

    // Default decode - update owned variables
    switch (f.id) {
      case ID_THR_TO_MOTOR:
        if (f.len >= sizeof(ThrottleMsg)) {
          ThrottleMsg t;
          memcpy(&t, f.data, sizeof(ThrottleMsg));
          // minimal sanity: clamp throttle
          if (t.throttle_raw > 2000) t.throttle_raw = 2000;
          s_throttle_raw.store(t.throttle_raw);
        }
        break;
      case ID_MOTOR:
        if (f.len >= sizeof(MotorTelemetry)) {
          MotorTelemetry m;
          memcpy(&m, f.data, sizeof(MotorTelemetry));
          s_motor_rpm.store(m.rpm);
          s_current_mA.store(m.current_mA);
        }
        break;
      case ID_PERI_TO_COMMS:
        if (f.len >= sizeof(PeriState)) {
          PeriState p;
          memcpy(&p, f.data, sizeof(PeriState));
          s_back_light.store(p.lights_mask & 0xFF);
        }
        break;
      default:
        break;
    }

    // Heartbeat detection: simple heuristic: if incoming ID is heartbeat kind, record it
    constexpr CanId HEARTBEAT_LOWBITS_MASK = 0xFF;
    //use lower byte mask
    if ((f.id & HEARTBEAT_LOWBITS_MASK) == 0x00) {
      // if you prefer distinct hb IDs, adapt here
      unsigned long now = vehicle_millis();
      // find or insert
      int slot = -1;
      for (int i = 0; i < MAX_NODE_HEARTS; ++i) {
        if (g_heartbeats[i].id == f.id) { slot = i; break; }
        if (g_heartbeats[i].id == 0 && slot == -1) slot = i;
      }
      if (slot >= 0) {
        g_heartbeats[slot].id = f.id;
        g_heartbeats[slot].last_seen_ms = (uint32_t)now;
        if (f.len >= 1) g_heartbeats[slot].node_type = f.data[0];
      }
    }

    // Call user handler
    if (g_handler) 
      g_handler(f.id, f.data, f.len);


  }
}

} // namespace vehicle

#endif // impl guard
