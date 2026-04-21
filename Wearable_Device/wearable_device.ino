#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LSM6DSOX.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <math.h>

#define DEVICE_NAME "WearableNode"
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define DATA_CHAR_UUID "abcd1234-5678-1234-5678-1234567890ab"
#define CONTROL_CHAR_UUID "dcba4321-8765-4321-8765-ba0987654321"

#define NODE_ID 1
#define UPDATE_INTERVAL_MS 100
#define I2C_SDA 16
#define I2C_SCL 17
#define ACC_THRESHOLD_MS2 11.0f

#define ACCEL_4G_MG_PER_LSB 0.122f
#define G_TO_MS2 9.80665f

enum WearableState {
  START_STATE,
  RECORDING_STATE,
  END_STATE
};

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

Adafruit_LSM6DSOX imu;
BLECharacteristic* dataCharacteristic = nullptr;

bool clientConnected = false;
volatile uint8_t pendingCommand = CMD_NONE;

WearableState currentState = START_STATE;

uint32_t packetId = 0;
unsigned long lastUpdate = 0;

float sumAx = 0.0f;
float sumAy = 0.0f;
float sumAz = 0.0f;
float sumAccMag = 0.0f;
uint8_t sampleCount = 0;

uint8_t truePacketCount = 0;
bool endPacketSent = false;

float rawAccelToMS2(int16_t raw) {
  return raw * ACCEL_4G_MG_PER_LSB * 0.001f * G_TO_MS2;
}

float magnitude3(float x, float y, float z) {
  return sqrtf(x * x + y * y + z * z);
}

void resetRecordingData() {
  sumAx = 0.0f;
  sumAy = 0.0f;
  sumAz = 0.0f;
  sumAccMag = 0.0f;
  sampleCount = 0;
  truePacketCount = 0;
  endPacketSent = false;
  lastUpdate = 0;
}

void sendPacket(uint8_t type, bool value, unsigned long now) {
  MotionPacket pkt;
  pkt.type = type;
  pkt.nodeId = NODE_ID;
  pkt.packetId = packetId++;
  pkt.timestampMs = now;
  pkt.value = value;

  if (clientConnected) {
    dataCharacteristic->setValue((uint8_t*)&pkt, sizeof(pkt));
    dataCharacteristic->notify();
  }

  Serial0.printf("[TX] type=%u pkt=%lu t=%lu value=%s connected=%s\n",
    pkt.type,
    pkt.packetId,
    pkt.timestampMs,
    pkt.value ? "TRUE" : "FALSE",
    clientConnected ? "YES" : "NO"
  );
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    clientConnected = true;
    Serial0.println("[BLE] ChairMount connected");
  }

  void onDisconnect(BLEServer* pServer) override {
    clientConnected = false;
    Serial0.println("[BLE] ChairMount disconnected");
    BLEDevice::startAdvertising();
  }
};

class ControlCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (value.length() < 1) return;

    pendingCommand = (uint8_t)value[0];

    if (pendingCommand == CMD_START) {
      Serial0.println("[CTRL] CMD_START");
    } else if (pendingCommand == CMD_RESET) {
      Serial0.println("[CTRL] CMD_RESET");
    } else {
      Serial0.printf("[CTRL] Unknown cmd=%u\n", pendingCommand);
    }
  }
};

void setup() {
  Serial0.begin(115200);
  delay(1000);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial0.println();
  Serial0.println("Wearable node starting...");

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!imu.begin_I2C()) {
    Serial0.println("[ERROR] LSM6DSOX not found");
    while (true) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(200);
    }
  }

  Serial0.println("[IMU] LSM6DSOX initialized");

  imu.setAccelRange(LSM6DS_ACCEL_RANGE_4_G);
  imu.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
  imu.setAccelDataRate(LSM6DS_RATE_104_HZ);
  imu.setGyroDataRate(LSM6DS_RATE_104_HZ);

  BLEDevice::init(DEVICE_NAME);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* service = server->createService(SERVICE_UUID);

  dataCharacteristic = service->createCharacteristic(
    DATA_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  dataCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic* controlCharacteristic = service->createCharacteristic(
    CONTROL_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  controlCharacteristic->setCallbacks(new ControlCallbacks());

  service->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial0.printf("[BLE] Advertising as '%s' (node %d)\n", DEVICE_NAME, NODE_ID);
}

void loop() {
  unsigned long now = millis();

  if (pendingCommand == CMD_RESET) {
    pendingCommand = CMD_NONE;
    currentState = START_STATE;
    resetRecordingData();
    Serial0.println("[FSM] -> START");
  }

  switch (currentState) {
    case START_STATE:
      digitalWrite(LED_BUILTIN, LOW);

      if (pendingCommand == CMD_START) {
        pendingCommand = CMD_NONE;
        resetRecordingData();
        currentState = RECORDING_STATE;
        Serial0.println("[FSM] START -> RECORDING");
      }
      break;

    case RECORDING_STATE:
      if (now - lastUpdate < UPDATE_INTERVAL_MS) break;
      lastUpdate = now;

      {
        sensors_event_t accelEvt, gyroEvt, tempEvt;
        imu.getEvent(&accelEvt, &gyroEvt, &tempEvt);

        float ax_ms2 = rawAccelToMS2(imu.rawAccX);
        float ay_ms2 = rawAccelToMS2(imu.rawAccY);
        float az_ms2 = rawAccelToMS2(imu.rawAccZ);
        float acc_mag_ms2 = magnitude3(ax_ms2, ay_ms2, az_ms2);

        sumAx += ax_ms2;
        sumAy += ay_ms2;
        sumAz += az_ms2;
        sumAccMag += acc_mag_ms2;
        sampleCount++;

        if (sampleCount >= 10) {
          float avgAx = sumAx / sampleCount;
          float avgAy = sumAy / sampleCount;
          float avgAz = sumAz / sampleCount;
          float avgAccMag = sumAccMag / sampleCount;

          bool aboveThreshold = avgAccMag > ACC_THRESHOLD_MS2;

          sendPacket(PACKET_BOOL, aboveThreshold, now);

          if (aboveThreshold) {
            truePacketCount++;
          }

          Serial0.printf(
            "[FSM] RECORDING AVG | ACC_MS2=(%.3f, %.3f, %.3f, %.3f) | above=%s | trueCount=%u\n",
            avgAx, avgAy, avgAz, avgAccMag,
            aboveThreshold ? "TRUE" : "FALSE",
            truePacketCount
          );

          digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

          sumAx = 0.0f;
          sumAy = 0.0f;
          sumAz = 0.0f;
          sumAccMag = 0.0f;
          sampleCount = 0;

          if (truePacketCount >= 10) {
            currentState = END_STATE;
            Serial0.println("[FSM] RECORDING -> END");
          }
        }
      }
      break;

    case END_STATE:
      digitalWrite(LED_BUILTIN, HIGH);

      if (!endPacketSent) {
        sendPacket(PACKET_END, true, now);
        endPacketSent = true;
      }
      break;
  }
}
