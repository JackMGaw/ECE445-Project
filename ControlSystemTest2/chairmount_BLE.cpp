#include "chairmount_BLE.h"

#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLEScan.h>
#include <BLEUtils.h>

namespace ChairMountBLE {

static const char* SERVICE_UUID = "12345678-1234-1234-1234-1234567890ab";
static const char* DATA_CHAR_UUID = "abcd1234-5678-1234-5678-1234567890ab";
static const char* CONTROL_CHAR_UUID = "dcba4321-8765-4321-8765-ba0987654321";

static BLEAdvertisedDevice* targetDevice = nullptr;
static BLEClient* bleClient = nullptr;
static BLERemoteCharacteristic* dataRemoteChar = nullptr;
static BLERemoteCharacteristic* controlRemoteChar = nullptr;
static BLEScan* bleScan = nullptr;

static bool bleConnected = false;
static bool bleShouldConnect = false;
static volatile bool wearableEndReceived = false;
static unsigned long lastStatusPrint = 0;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      Serial.println("[BLE] Wearable found");
      if (targetDevice != nullptr) delete targetDevice;
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
  if (length != sizeof(MotionPacket)) {
    Serial.printf("[BLE] Bad packet length=%u expected=%u\n", (unsigned)length, (unsigned)sizeof(MotionPacket));
    return;
  }

  MotionPacket pkt;
  memcpy(&pkt, pData, sizeof(pkt));

  if (pkt.type == PACKET_BOOL) {
    Serial.printf("[BLE RX] BOOL node=%u pkt=%lu value=%s\n",
      pkt.nodeId,
      pkt.packetId,
      pkt.value ? "TRUE" : "FALSE");
  } else if (pkt.type == PACKET_END) {
    wearableEndReceived = true;
    Serial.printf("[BLE RX] END node=%u pkt=%lu\n", pkt.nodeId, pkt.packetId);
  } else {
    Serial.printf("[BLE RX] Unknown type=%u\n", pkt.type);
  }
}

static void startScan() {
  if (bleConnected || bleScan == nullptr) return;
  Serial.println("[BLE] Scanning...");
  bleScan->start(5, false);
}

static bool connectToWearable() {
  if (targetDevice == nullptr) {
    Serial.println("[BLE] No target device yet");
    return false;
  }

  Serial.println("[BLE] Attempting connection...");
  bleClient = BLEDevice::createClient();

  if (!bleClient->connect(targetDevice)) {
    Serial.println("[BLE] Connect failed");
    return false;
  }

  BLERemoteService* remoteService = bleClient->getService(BLEUUID(SERVICE_UUID));
  if (remoteService == nullptr) {
    Serial.println("[BLE] Service not found");
    bleClient->disconnect();
    return false;
  }

  dataRemoteChar = remoteService->getCharacteristic(BLEUUID(DATA_CHAR_UUID));
  controlRemoteChar = remoteService->getCharacteristic(BLEUUID(CONTROL_CHAR_UUID));

  if (dataRemoteChar == nullptr || controlRemoteChar == nullptr) {
    Serial.println("[BLE] Missing characteristic");
    bleClient->disconnect();
    return false;
  }

  if (!dataRemoteChar->canNotify()) {
    Serial.println("[BLE] Notify not supported");
    bleClient->disconnect();
    return false;
  }

  dataRemoteChar->registerForNotify(notifyCallback);
  bleConnected = true;
  wearableEndReceived = false;
  Serial.println("[BLE] Connected to wearable");
  return true;
}

void begin() {
  Serial.println("[BLE] Init chair client");
  BLEDevice::init("ChairMountReceiver");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  bleScan->setActiveScan(true);
  bleScan->setInterval(100);
  bleScan->setWindow(80);
  startScan();
}

void update() {
  if (!bleConnected && bleShouldConnect) {
    bleShouldConnect = false;
    if (!connectToWearable()) {
      delay(300);
      startScan();
    }
  }

  if (bleConnected && bleClient != nullptr && !bleClient->isConnected()) {
    bleConnected = false;
    dataRemoteChar = nullptr;
    controlRemoteChar = nullptr;
    wearableEndReceived = false;
    Serial.println("[BLE] Disconnected");
    startScan();
  }

  if (millis() - lastStatusPrint >= 2000) {
    lastStatusPrint = millis();
    Serial.printf("[BLE] status connected=%s end=%s\n",
      bleConnected ? "YES" : "NO",
      wearableEndReceived ? "YES" : "NO");
  }
}

bool isConnected() {
  return bleConnected;
}

static void sendCommand(uint8_t cmd) {
  if (!bleConnected || controlRemoteChar == nullptr) {
    Serial.println("[BLE] Cannot send cmd, not connected");
    return;
  }

  uint8_t data[1];
  data[0] = cmd;
  controlRemoteChar->writeValue(data, 1, false);

  if (cmd == CMD_START) {
    Serial.println("[BLE TX] CMD_START");
  } else if (cmd == CMD_RESET) {
    Serial.println("[BLE TX] CMD_RESET");
  } else {
    Serial.printf("[BLE TX] cmd=%u\n", cmd);
  }
}

void startExerciseSession() {
  wearableEndReceived = false;
  sendCommand(CMD_START);
}

void resetWearable() {
  wearableEndReceived = false;
  sendCommand(CMD_RESET);
}

bool hasExerciseCompleted() {
  return wearableEndReceived;
}

void clearExerciseCompleted() {
  wearableEndReceived = false;
}

}
