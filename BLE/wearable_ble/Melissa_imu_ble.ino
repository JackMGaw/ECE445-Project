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
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-1234567890ab"
#define NODE_ID 1
#define UPDATE_INTERVAL_MS 100
#define I2C_SDA 16
#define I2C_SCL 17
#define ACC_THRESHOLD_MS2 11.0f

#define ACCEL_4G_MG_PER_LSB 0.122f
#define G_TO_MS2 9.80665f

struct __attribute__((packed)) MotionPacket {
  uint8_t nodeId;
  uint32_t packetId;
  uint32_t timestampMs;
  bool accelAboveThreshold;
};

Adafruit_LSM6DSOX imu;
BLECharacteristic* pCharacteristic = nullptr;
bool clientConnected = false;
uint32_t packetId = 0;
unsigned long lastUpdate = 0;

float sumAx = 0.0f;
float sumAy = 0.0f;
float sumAz = 0.0f;
float sumAccMag = 0.0f;
uint8_t sampleCount = 0;

float rawAccelToMS2(int16_t raw) {
  return raw * ACCEL_4G_MG_PER_LSB * 0.001f * G_TO_MS2;
}

float magnitude3(float x, float y, float z) {
  return sqrtf(x * x + y * y + z * z);
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
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(SERVICE_UUID);
  pAdv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial0.printf("[BLE] Advertising as '%s' (node %d)\n", DEVICE_NAME, NODE_ID);
}

void loop() {
  unsigned long now = millis();
  if (now - lastUpdate < UPDATE_INTERVAL_MS) return;
  lastUpdate = now;

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
    bool accelAboveThreshold = avgAccMag > ACC_THRESHOLD_MS2;

    MotionPacket pkt;
    pkt.nodeId = NODE_ID;
    pkt.packetId = packetId++;
    pkt.timestampMs = now;
    pkt.accelAboveThreshold = accelAboveThreshold;

    if (clientConnected) {
      pCharacteristic->setValue((uint8_t*)&pkt, sizeof(pkt));
      pCharacteristic->notify();
    }

    Serial0.printf("AVG 1s | ACC_MS2=(%.3f, %.3f, %.3f, %.3f) | above50=%s | sent=%s\n",
      avgAx, avgAy, avgAz, avgAccMag,
      accelAboveThreshold ? "TRUE" : "FALSE",
      clientConnected ? "YES" : "NO"
    );

    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    sumAx = 0.0f;
    sumAy = 0.0f;
    sumAz = 0.0f;
    sumAccMag = 0.0f;
    sampleCount = 0;
  }
}
