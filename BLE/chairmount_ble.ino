#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>

#define DEVICE_NAME         "ChairMount"
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-1234567890ab"

// Packet format received from the wearable node.
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

class BLECentralReceiver;

// Client callbacks handle connect and disconnect events.
class CentralClientCallbacks : public BLEClientCallbacks {
private:
  BLECentralReceiver* owner;

public:
  CentralClientCallbacks(BLECentralReceiver* ownerPtr) : owner(ownerPtr) {}
  void onConnect(BLEClient* pclient) override;
  void onDisconnect(BLEClient* pclient) override;
};

// Scan callbacks check whether a discovered device is the one we want.
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
private:
  BLECentralReceiver* owner;

public:
  AdvertisedDeviceCallbacks(BLECentralReceiver* ownerPtr) : owner(ownerPtr) {}
  void onResult(BLEAdvertisedDevice advertisedDevice) override;
};

// This class manages scanning, connecting, and receiving BLE notifications.
class BLECentralReceiver {
private:
  BLEAdvertisedDevice* targetDevice;
  BLEClient* client;
  BLERemoteCharacteristic* remoteCharacteristic;
  bool doConnect;
  bool connected;
  bool doScan;

public:
  BLECentralReceiver()
    : targetDevice(nullptr),
      client(nullptr),
      remoteCharacteristic(nullptr),
      doConnect(false),
      connected(false),
      doScan(false) {}

  void begin() {
    Serial.println("Starting chairmount BLE client...");
    BLEDevice::init(DEVICE_NAME);
    startScan();
  }

  void startScan() {
    Serial.println("Scanning...");
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(this));
    scan->setInterval(1349);
    scan->setWindow(449);
    scan->setActiveScan(true);
    scan->start(5, false);
  }

  void handleFoundDevice(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("Found device: ");
    Serial.println(advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      Serial.println("Target device with matching service UUID found");
      BLEDevice::getScan()->stop();

      if (targetDevice != nullptr) {
        delete targetDevice;
      }

      targetDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = false;
    }
  }

  void handleDisconnected() {
    connected = false;
    doScan = true;
    Serial.println("Disconnected from server");
  }

  void setConnected(bool state) {
    connected = state;
  }

  bool isConnected() const {
    return connected;
  }

  // This callback parses incoming motion data and prints it to Serial.
  static void notifyCallback(
    BLERemoteCharacteristic* remoteChar,
    uint8_t* data,
    size_t length,
    bool isNotify
  ) {
    if (length != sizeof(MotionPacket)) {
      Serial.print("Unexpected packet size: ");
      Serial.println(length);
      return;
    }

    MotionPacket pkt;
    memcpy(&pkt, data, sizeof(MotionPacket));

    Serial.print("Received packet -> ");
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

  bool connectToServer() {
    if (targetDevice == nullptr) {
      Serial.println("No target device");
      return false;
    }

    Serial.println("Connecting...");

    client = BLEDevice::createClient();
    client->setClientCallbacks(new CentralClientCallbacks(this));

    if (!client->connect(targetDevice)) {
      Serial.println("Failed to connect");
      return false;
    }

    BLERemoteService* remoteService = client->getService(SERVICE_UUID);
    if (remoteService == nullptr) {
      Serial.println("Service not found");
      client->disconnect();
      return false;
    }

    remoteCharacteristic = remoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (remoteCharacteristic == nullptr) {
      Serial.println("Characteristic not found");
      client->disconnect();
      return false;
    }

    if (remoteCharacteristic->canNotify()) {
      remoteCharacteristic->registerForNotify(notifyCallback);
      Serial.println("Notify registered");
    } else {
      Serial.println("Characteristic cannot notify");
    }

    connected = true;
    return true;
  }

  // update() runs the client state flow for connect and rescan.
  void update() {
    if (doConnect) {
      if (connectToServer()) {
        Serial.println("Connected and ready");
      } else {
        Serial.println("Connection failed, rescanning...");
        doScan = true;
      }
      doConnect = false;
    }

    if (doScan && !connected) {
      startScan();
      doScan = false;
    }
  }
};

// These callback definitions forward BLE events back to the receiver object.
void CentralClientCallbacks::onConnect(BLEClient* pclient) {
  Serial.println("Connected to server");
}

void CentralClientCallbacks::onDisconnect(BLEClient* pclient) {
  owner->handleDisconnected();
}

void AdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
  owner->handleFoundDevice(advertisedDevice);
}

// This is the main BLE receiver instance for the chair side.
BLECentralReceiver chairReceiver;

// setup() starts Serial and initializes the BLE client logic.
void setup() {
  Serial.begin(115200);
  delay(1000);
  chairReceiver.begin();
}

// loop() keeps the BLE state machine running.
void loop() {
  chairReceiver.update();
  delay(100);
}
