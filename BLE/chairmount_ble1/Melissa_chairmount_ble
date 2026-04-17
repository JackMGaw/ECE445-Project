#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLEClient.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-1234567890ab"

struct __attribute__((packed)) MotionPacket {
  uint8_t nodeId;
  uint32_t packetId;
  uint32_t timestampMs;
  bool accelAboveThreshold;
};

BLEAdvertisedDevice* targetDevice = nullptr;
BLERemoteCharacteristic* remoteCharacteristic = nullptr;
BLEClient* bleClient = nullptr;
BLEScan* bleScan = nullptr;

bool connected = false;
bool shouldConnect = false;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    Serial0.print("[BLE] Saw device: ");
    Serial0.println(advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      Serial0.println("[BLE] Matching service found");
      targetDevice = new BLEAdvertisedDevice(advertisedDevice);
      bleScan->stop();
      shouldConnect = true;
    }
  }
};

void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify
) {
  if (length != sizeof(MotionPacket)) {
    Serial0.printf("[BLE] Bad packet length: %u, expected %u\n",
      (unsigned)length, (unsigned)sizeof(MotionPacket));
    return;
  }

  MotionPacket pkt;
  memcpy(&pkt, pData, sizeof(MotionPacket));

  Serial0.printf("[RX] node=%u pkt=%lu t=%lu above50=%s\n",
    pkt.nodeId,
    pkt.packetId,
    pkt.timestampMs,
    pkt.accelAboveThreshold ? "TRUE" : "FALSE"
  );

  digitalWrite(LED_BUILTIN, pkt.accelAboveThreshold ? HIGH : LOW);
}

bool connectToServer() {
  if (targetDevice == nullptr) return false;

  Serial0.println("[BLE] Connecting to wearable...");
  bleClient = BLEDevice::createClient();

  if (!bleClient->connect(targetDevice)) {
    Serial0.println("[BLE] Connect failed");
    return false;
  }

  Serial0.println("[BLE] Connected");

  BLERemoteService* remoteService = bleClient->getService(BLEUUID(SERVICE_UUID));
  if (remoteService == nullptr) {
    Serial0.println("[BLE] Service not found on server");
    bleClient->disconnect();
    return false;
  }

  remoteCharacteristic = remoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
  if (remoteCharacteristic == nullptr) {
    Serial0.println("[BLE] Characteristic not found on server");
    bleClient->disconnect();
    return false;
  }

  if (!remoteCharacteristic->canNotify()) {
    Serial0.println("[BLE] Characteristic cannot notify");
    bleClient->disconnect();
    return false;
  }

  remoteCharacteristic->registerForNotify(notifyCallback);
  Serial0.println("[BLE] Notifications registered");
  return true;
}

void startScan() {
  Serial0.println("[BLE] Scanning for wearable...");
  bleScan->start(5, false);
  Serial0.println("[BLE] Scan finished");
}

void setup() {
  Serial0.begin(115200);
  delay(1000);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial0.println();
  Serial0.println("[BLE] ChairMount receiver starting");

  BLEDevice::init("ChairMountReceiver");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  bleScan->setActiveScan(true);
  bleScan->setInterval(100);
  bleScan->setWindow(80);
}

void loop() {
  if (!connected && !shouldConnect) {
    startScan();
    delay(500);
  }

  if (shouldConnect) {
    shouldConnect = false;
    connected = connectToServer();

    if (!connected) {
      Serial0.println("[BLE] Connection/setup failed, retrying scan");
      delay(1000);
    }
  }

  if (connected && bleClient != nullptr && !bleClient->isConnected()) {
    Serial0.println("[BLE] Disconnected");
    connected = false;
    targetDevice = nullptr;
    remoteCharacteristic = nullptr;
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
  }

  delay(100);
}
