#ifndef CHAIRMOUNT_BLE_H
#define CHAIRMOUNT_BLE_H

#include <Arduino.h>
#include <stdint.h>

namespace ChairMountBLE {

constexpr const char* SERVICE_UUID      = "12345678-1234-1234-1234-1234567890ab";
constexpr const char* DATA_CHAR_UUID    = "abcd1234-5678-1234-5678-1234567890ab";
constexpr const char* CONTROL_CHAR_UUID = "dcba4321-8765-4321-8765-ba0987654321";

enum PacketType : uint8_t {
  PACKET_BOOL = 1,
  PACKET_END  = 2
};

enum ControlCommand : uint8_t {
  CMD_NONE  = 0,
  CMD_START = 1,
  CMD_RESET = 2
};

struct __attribute__((packed)) MotionPacket {
  uint8_t  type;
  uint8_t  nodeId;
  uint32_t packetId;
  uint32_t timestampMs;
  bool     value;
};

void begin(const char* deviceName = "ChairMountReceiver");
void update();

bool isConnected();
bool hasExerciseCompleted();
bool getLatestMotionFlag();
void clearExerciseCompletedFlag();

bool startExerciseSession();
bool resetWearable();

}  // namespace ChairMountBLE

#endif
