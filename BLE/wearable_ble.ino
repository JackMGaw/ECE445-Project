#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define DEVICE_NAME         "Wearable1"
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-1234567890ab"

// Packet format sent to the central device.
struct __attribute__((packed)) MotionPacket {
  uint8_t nodeId;
  uint32_t packetId;
  uint32_t timestampMs;
  float ax;
  float ay;
  float az;
  float gx;
  float gy;
  float gz;
};

class BLEPeripheralNode;

class PeripheralServerCallbacks : public BLEServerCallbacks {
private:
  BLEPeripheralNode* owner;

public:
  PeripheralServerCallbacks(BLEPeripheralNode* ownerPtr) : owner(ownerPtr) {}
  void onConnect(BLEServer* pServer) override;
  void onDisconnect(BLEServer* pServer) override;
};

// This class manages BLE advertising and packet sending.
class BLEPeripheralNode {
private:
  BLEServer* server;
  BLECharacteristic* characteristic;
  bool deviceConnected;
  bool oldDeviceConnected;
  unsigned long lastSendMs;
  uint32_t packetCounter;
  uint8_t nodeId;

public:
  BLEPeripheralNode(uint8_t node)
    : server(nullptr),
      characteristic(nullptr),
      deviceConnected(false),
      oldDeviceConnected(false),
      lastSendMs(0),
      packetCounter(0),
      nodeId(node) {}

  void begin() {
    Serial.println("Starting wearable BLE server...");

    BLEDevice::init(DEVICE_NAME);

    server = BLEDevice::createServer();
    server->setCallbacks(new PeripheralServerCallbacks(this));

    BLEService* service = server->createService(SERVICE_UUID);

    characteristic = service->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );

    characteristic->addDescriptor(new BLE2902());
    service->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->start();

    Serial.println("Advertising started");
  }

  void setConnected(bool state) {
    deviceConnected = state;
    if (state) {
      Serial.println("Central connected");
    } else {
      Serial.println("Central disconnected");
    }
  }

  // buildPacket() creates one motion packet before sending.
  MotionPacket buildPacket() {
    MotionPacket pkt;
    pkt.nodeId = nodeId;
    pkt.packetId = packetCounter++;
    pkt.timestampMs = millis();

    // Temporary dummy data, replace later with real LSM6DSOTR readings
    pkt.ax = 0.10f + 0.01f * (pkt.packetId % 10);
    pkt.ay = 0.20f + 0.02f * (pkt.packetId % 10);
    pkt.az = 9.80f + 0.01f * (pkt.packetId % 5);

    pkt.gx = 1.00f + 0.10f * (pkt.packetId % 10);
    pkt.gy = 2.00f + 0.10f * (pkt.packetId % 10);
    pkt.gz = 3.00f + 0.10f * (pkt.packetId % 10);

    return pkt;
  }

  // sendPacket() pushes the packet out as a BLE notification.
  void sendPacket(const MotionPacket& pkt) {
    characteristic->setValue((uint8_t*)&pkt, sizeof(MotionPacket));
    characteristic->notify();

    Serial.print("Sent packet -> ");
    Serial.print("nodeId=");
    Serial.print(pkt.nodeId);
    Serial.print(", packetId=");
    Serial.print(pkt.packetId);
    Serial.print(", t=");
    Serial.print(pkt.timestampMs);
    Serial.print(", ax=");
    Serial.print(pkt.ax, 3);
    Serial.print(", ay=");
    Serial.print(pkt.ay, 3);
    Serial.print(", az=");
    Serial.print(pkt.az, 3);
    Serial.print(", gx=");
    Serial.print(pkt.gx, 3);
    Serial.print(", gy=");
    Serial.print(pkt.gy, 3);
    Serial.print(", gz=");
    Serial.println(pkt.gz, 3);
  }

  // update() handles periodic sending and reconnect behavior.
  void update() {
    if (deviceConnected && millis() - lastSendMs >= 500) {
      lastSendMs = millis();
      MotionPacket pkt = buildPacket();
      sendPacket(pkt);
    }

    if (!deviceConnected && oldDeviceConnected) {
      delay(200);
      BLEDevice::startAdvertising();
      Serial.println("Restart advertising");
      oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
    }
  }
};

// These callbacks forward connection events to the main node object.
void PeripheralServerCallbacks::onConnect(BLEServer* pServer) {
  owner->setConnected(true);
}

void PeripheralServerCallbacks::onDisconnect(BLEServer* pServer) {
  owner->setConnected(false);
}

// Main wearable BLE node instance.
BLEPeripheralNode wearableNode(1);

// setup() starts Serial and BLE advertising.
void setup() {
  Serial.begin(115200);
  delay(1000);
  wearableNode.begin();
}

// loop() keeps the BLE node running.
void loop() {
  wearableNode.update();
  delay(10);
}
