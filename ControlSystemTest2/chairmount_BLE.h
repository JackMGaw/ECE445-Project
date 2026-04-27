#ifndef CHAIRMOUNT_BLE_H
#define CHAIRMOUNT_BLE_H

#include <Arduino.h>

namespace ChairMountBLE {

enum PacketType : uint8_t {
  PACKET_BOOL = 1,
  PACKET_END = 2
};

enum ControlCommand : uint8_t {
  CMD_NONE = 0,
  CMD_START = 1,
  CMD_RESET = 2
};

struct __attribute__((packed)) MotionPacket {
  uint8_t type;
  uint8_t nodeId;
  uint32_t packetId;
  uint32_t timestampMs;
  bool value;
};

void begin();
void update();
bool isConnected();
void startExerciseSession();
void resetWearable();
bool hasExerciseCompleted();
void clearExerciseCompleted();
uint32_t getPackCount();
void resetPackCount();
}

#endif
