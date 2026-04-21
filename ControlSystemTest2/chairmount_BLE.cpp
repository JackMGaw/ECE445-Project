#include "chairmount_BLE.h"

#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLEScan.h>
#include <BLEUtils.h>
#include <cstring>

namespace ChairMountBLE {
namespace {

BLEAdvertisedDevice*      targetDevice      = nullptr;
BLEClient*                bleClient         = nullptr;
BLERemoteCharacteristic*  dataRemoteChar    = nullptr;
BLERemoteCharacteristic*  controlRemoteChar = nullptr;
BLEScan*                  bleScan           = nullptr;

bool bleConnected         = false;
bool bleShouldConnect     = false;
bool exerciseCompleted    = false;
bool latestMotionFlag     = false;

void startBleScan();
bool connectToWearable();
void sendWearableCommand(uint8_t cmd);

class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      Serial.println("[BLE] Wearable found");

      if (targetDevice != nullptr) {
        delete targetDevice;
        targetDevice = nullptr;
      }

      targetDevice = new BLEAdvertisedDevice(advertisedDevice);
      bleScan->stop();
      bleShouldConnect = true;
    }
  }
};

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify
) {
  (void)pBLERemoteCharacteristic;
  (void)isNotify;

  if (length != sizeof(MotionPacket)) {
    Serial.printf("[BLE] Bad packet length=%u expected=%u\n",
                  (unsigned)length,
                  (unsigned)sizeof(MotionPacket));
    return;
  }

  MotionPacket pkt;
  memcpy(&pkt, pData, sizeof(pkt));

  if (pkt.type == PACKET_BOOL) {
    latestMotionFlag = pkt.value;
    Serial.printf("[BLE RX] BOOL node=%u pkt=%lu value=%s\n",
                  pkt.nodeId,
                  (unsigned long)pkt.packetId,
                  pkt.value ? "TRUE" : "FALSE");
  } else if (pkt.type == PACKET_END) {
    exerciseCompleted = true;
    Serial.printf("[BLE RX] END node=%u pkt=%lu\n",
                  pkt.nodeId,
                  (unsigned long)pkt.packetId);
  } else {
    Serial.printf("[BLE RX] Unknown packet type=%u\n", pkt.type);
  }
}

void startBleScan() {
  if (bleConnected || bleScan == nullptr) return;

  Serial.println("[BLE] Scanning...");
  bleScan->start(5, false);
}

bool connectToWearable() {
  if (targetDevice == nullptr) return false;

  bleClient = BLEDevice::createClient();
  if (bleClient == nullptr) {
    Serial.println("[BLE] Failed to create client");
    return false;
  }

  if (!bleClient->connect(targetDevice)) {
    Serial.println("[BLE] Connect failed");
    delete bleClient;
    bleClient = nullptr;
    return false;
  }

  BLERemoteService* remoteService = bleClient->getService(BLEUUID(SERVICE_UUID));
  if (remoteService == nullptr) {
    Serial.println("[BLE] Service not found");
    bleClient->disconnect();
    delete bleClient;
    bleClient = nullptr;
    return false;
  }

  dataRemoteChar = remoteService->getCharacteristic(BLEUUID(DATA_CHAR_UUID));
  controlRemoteChar = remoteService->getCharacteristic(BLEUUID(CONTROL_CHAR_UUID));

  if (dataRemoteChar == nullptr || controlRemoteChar == nullptr) {
    Serial.println("[BLE] Missing characteristic");
    bleClient->disconnect();
    delete bleClient;
    bleClient = nullptr;
    dataRemoteChar = nullptr;
    controlRemoteChar = nullptr;
    return false;
  }

  if (!dataRemoteChar->canNotify()) {
    Serial.println("[BLE] Notify not supported");
    bleClient->disconnect();
    delete bleClient;
    bleClient = nullptr;
    dataRemoteChar = nullptr;
    controlRemoteChar = nullptr;
    return false;
  }

  dataRemoteChar->registerForNotify(notifyCallback);
  bleConnected = true;
  Serial.println("[BLE] Connected to wearable");
  return true;
}

void sendWearableCommand(uint8_t cmd) {
  if (!bleConnected || controlRemoteChar == nullptr) {
    Serial.println("[BLE] Cannot send cmd, not connected");
    return;
  }

  uint8_t data[1] = {cmd};
  controlRemoteChar->writeValue(data, 1, false);

  if (cmd == CMD_START) {
    Serial.println("[BLE TX] CMD_START");
  } else if (cmd == CMD_RESET) {
    Serial.println("[BLE TX] CMD_RESET");
  } else {
    Serial.printf("[BLE TX] CMD_%u\n", cmd);
  }
}

}  // namespace

void begin(const char* deviceName) {
  BLEDevice::init(deviceName);

  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  bleScan->setActiveScan(true);
  bleScan->setInterval(100);
  bleScan->setWindow(80);

  exerciseCompleted = false;
  latestMotionFlag = false;
  bleConnected = false;
  bleShouldConnect = false;

  startBleScan();
}

void update() {
  if (!bleConnected && bleShouldConnect) {
    bleShouldConnect = false;
    if (!connectToWearable()) {
      delay(500);
      startBleScan();
    }
  }

  if (bleConnected && bleClient != nullptr && !bleClient->isConnected()) {
    bleConnected = false;
    dataRemoteChar = nullptr;
    controlRemoteChar = nullptr;
    exerciseCompleted = false;
    latestMotionFlag = false;

    if (targetDevice != nullptr) {
      delete targetDevice;
      targetDevice = nullptr;
    }

    if (bleClient != nullptr) {
      delete bleClient;
      bleClient = nullptr;
    }

    Serial.println("[BLE] Disconnected");
    startBleScan();
  }
}

bool isConnected() {
  return bleConnected;
}

bool hasExerciseCompleted() {
  return exerciseCompleted;
}

bool getLatestMotionFlag() {
  return latestMotionFlag;
}

void clearExerciseCompletedFlag() {
  exerciseCompleted = false;
}

bool startExerciseSession() {
  if (!bleConnected) return false;
  exerciseCompleted = false;
  latestMotionFlag = false;
  sendWearableCommand(CMD_START);
  return true;
}

bool resetWearable() {
  if (!bleConnected) {
    exerciseCompleted = false;
    latestMotionFlag = false;
    return false;
  }

  sendWearableCommand(CMD_RESET);
  exerciseCompleted = false;
  latestMotionFlag = false;
  return true;
}

}  // namespace ChairMountBLE
