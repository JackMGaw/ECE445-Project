#include <Wire.h>
#include <Adafruit_LiquidCrystal.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLEClient.h>

#define BUTTON1 19
#define BUTTON2 20
#define LED 1

#define SDA_PIN 4
#define SCL_PIN 5

#define HX711_DOUT 18
#define HX711_SCK  8

#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define DATA_CHAR_UUID "abcd1234-5678-1234-5678-1234567890ab"
#define CONTROL_CHAR_UUID "dcba4321-8765-4321-8765-ba0987654321"

#define printf0(format, args...) do {char buf[256]; sprintf(buf, format, args); Serial0.print(buf);} while (0)

typedef unsigned int uint;
typedef unsigned long ulong;

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

Adafruit_LiquidCrystal lcd(0);
char buffer[17];

const ulong debounceDelay = 50;

int data;
int value;

struct Calibration {
  int calLow;
  int calHigh;
  int calLowThresh;
  int calHighThresh;
} cal;

enum State {
  EMPTY,
  SITTING,
  ALARM,
  CHECK_ACTIVITY,
  CAN_SIT_AGAIN
};

State currentState = EMPTY;

ulong sitStartTime = 0;
ulong activityStartTime = 0;
ulong lastDisplayTime = 0;

ulong alertTime = 5000;
ulong needActivityTime = 10000;

BLEAdvertisedDevice* targetDevice = nullptr;
BLEClient* bleClient = nullptr;
BLERemoteCharacteristic* dataRemoteChar = nullptr;
BLERemoteCharacteristic* controlRemoteChar = nullptr;
BLEScan* bleScan = nullptr;

bool bleConnected = false;
bool bleShouldConnect = false;
volatile bool wearableEndReceived = false;

int isButton1Pressed();
int isButton2Pressed();

void setupLCD(int sda, int scl);
int displayLCD(int col, int row, char string[]);

int readHX711();
int readAverage(int samplePeriod);
Calibration Calibrate();

bool readSeatOccupied();
bool detectRealActivity();

void updateState();
void updateLCD();
const char* stateName(State s);

void startBleScan();
bool connectToWearable();
void bleLoop();
void sendWearableCommand(uint8_t cmd);

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      Serial0.println("[BLE] Wearable found");
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
    Serial0.printf("[BLE] Bad packet length=%u expected=%u\n", (unsigned)length, (unsigned)sizeof(MotionPacket));
    return;
  }

  MotionPacket pkt;
  memcpy(&pkt, pData, sizeof(pkt));

  if (pkt.type == PACKET_BOOL) {
    Serial0.printf("[BLE RX] BOOL node=%u pkt=%lu value=%s\n",
      pkt.nodeId,
      pkt.packetId,
      pkt.value ? "TRUE" : "FALSE"
    );
  } else if (pkt.type == PACKET_END) {
    wearableEndReceived = true;
    Serial0.printf("[BLE RX] END node=%u pkt=%lu\n", pkt.nodeId, pkt.packetId);
  }
}

void setup() {
  Serial0.begin(115200);

  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(LED, OUTPUT);

  pinMode(HX711_SCK, OUTPUT);
  pinMode(HX711_DOUT, INPUT);
  digitalWrite(HX711_SCK, LOW);

  setupLCD(SDA_PIN, SCL_PIN);

  lcd.setCursor(0, 0);
  lcd.print("Starting...");
  Serial0.println("System starting...");

  delay(1000);

  Calibrate();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  Serial0.println("System ready.");
  delay(1000);

  BLEDevice::init("ChairMountReceiver");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  bleScan->setActiveScan(true);
  bleScan->setInterval(100);
  bleScan->setWindow(80);

  startBleScan();
}

void loop() {
  value = readAverage(20);

  bleLoop();

  if (isButton1Pressed()) {
    alertTime += 10000;
    Serial0.print("Alert time = ");
    Serial0.println(alertTime);
  }

  if (isButton2Pressed()) {
    if (alertTime > 10000) {
      alertTime -= 10000;
    }
    Serial0.print("Alert time = ");
    Serial0.println(alertTime);
  }

  updateState();
  updateLCD();

  delay(20);
}

void startBleScan() {
  if (bleConnected) return;
  Serial0.println("[BLE] Scanning...");
  bleScan->start(5, false);
}

bool connectToWearable() {
  if (targetDevice == nullptr) return false;

  bleClient = BLEDevice::createClient();

  if (!bleClient->connect(targetDevice)) {
    Serial0.println("[BLE] Connect failed");
    return false;
  }

  BLERemoteService* remoteService = bleClient->getService(BLEUUID(SERVICE_UUID));
  if (remoteService == nullptr) {
    Serial0.println("[BLE] Service not found");
    bleClient->disconnect();
    return false;
  }

  dataRemoteChar = remoteService->getCharacteristic(BLEUUID(DATA_CHAR_UUID));
  controlRemoteChar = remoteService->getCharacteristic(BLEUUID(CONTROL_CHAR_UUID));

  if (dataRemoteChar == nullptr || controlRemoteChar == nullptr) {
    Serial0.println("[BLE] Missing characteristic");
    bleClient->disconnect();
    return false;
  }

  if (!dataRemoteChar->canNotify()) {
    Serial0.println("[BLE] Notify not supported");
    bleClient->disconnect();
    return false;
  }

  dataRemoteChar->registerForNotify(notifyCallback);

  bleConnected = true;
  Serial0.println("[BLE] Connected to wearable");
  return true;
}

void bleLoop() {
  if (!bleConnected && bleShouldConnect) {
    bleShouldConnect = false;
    if (!connectToWearable()) {
      delay(500);
      startBleScan();
    }
  }

  if (bleConnected && bleClient != nullptr && !bleClient->isConnected()) {
    bleConnected = false;
    targetDevice = nullptr;
    dataRemoteChar = nullptr;
    controlRemoteChar = nullptr;
    Serial0.println("[BLE] Disconnected");
    startBleScan();
  }
}

void sendWearableCommand(uint8_t cmd) {
  if (!bleConnected || controlRemoteChar == nullptr) {
    Serial0.println("[BLE] Cannot send cmd, not connected");
    return;
  }

  uint8_t data[1];
  data[0] = cmd;
  controlRemoteChar->writeValue(data, 1, false);

  if (cmd == CMD_START) {
    Serial0.println("[BLE TX] CMD_START");
  } else if (cmd == CMD_RESET) {
    Serial0.println("[BLE TX] CMD_RESET");
  }
}

void updateState() {
  bool isSitting = readSeatOccupied();

  switch (currentState) {
    case EMPTY:
      digitalWrite(LED, LOW);

      if (isSitting) {
        currentState = SITTING;
        sitStartTime = millis();
        Serial0.println("State -> SITTING");
      }
      break;

    case SITTING:
      digitalWrite(LED, HIGH);

      if (!isSitting) {
        currentState = EMPTY;
        Serial0.println("State -> EMPTY");
      }
      else if (millis() - sitStartTime >= alertTime) {
        currentState = ALARM;
        Serial0.println("State -> ALARM");
      }
      break;

    case ALARM:
      digitalWrite(LED, (millis() / 300) % 2);

      if (!isSitting) {
        wearableEndReceived = false;
        sendWearableCommand(CMD_START);
        activityStartTime = millis();
        currentState = CHECK_ACTIVITY;
        Serial0.println("State -> CHECK_ACTIVITY");
      }
      break;

    case CHECK_ACTIVITY:
      if (isSitting) {
        sendWearableCommand(CMD_RESET);
        wearableEndReceived = false;
        currentState = ALARM;
        Serial0.println("State -> ALARM");
      }
      else {
        if (detectRealActivity()) {
          sendWearableCommand(CMD_RESET);
          wearableEndReceived = false;
          currentState = CAN_SIT_AGAIN;
          Serial0.println("State -> CAN_SIT_AGAIN");
        }
      }
      break;

    case CAN_SIT_AGAIN:
      digitalWrite(LED, HIGH);

      if (isSitting) {
        currentState = SITTING;
        sitStartTime = millis();
        Serial0.println("State -> SITTING");
      }
      break;
  }
}

bool readSeatOccupied() {
  static bool occupied = false;

  if (!occupied && value > cal.calHighThresh) {
    occupied = true;
  }
  else if (occupied && value < cal.calLowThresh) {
    occupied = false;
  }

  return occupied;
}

bool detectRealActivity() {
  return wearableEndReceived;
}

void updateLCD() {
  if (millis() - lastDisplayTime < 200) return;
  lastDisplayTime = millis();

  char line1[17];
  char line2[17];

  snprintf(line1, sizeof(line1), "State:%s", stateName(currentState));
  snprintf(line2, sizeof(line2), "T:%lus V:%d", alertTime / 1000, value);

  displayLCD(0, 0, line1);
  displayLCD(0, 1, line2);
}

const char* stateName(State s) {
  switch (s) {
    case EMPTY: return "EMPTY";
    case SITTING: return "SIT";
    case ALARM: return "ALARM";
    case CHECK_ACTIVITY: return "CHECK";
    case CAN_SIT_AGAIN: return "OK";
    default: return "UNK";
  }
}

void setupLCD(int sda, int scl) {
  Wire.begin(sda, scl);

  if (!lcd.begin(16, 2)) {
    Serial0.println("Could not init backpack. Check wiring.");
    while (1);
  }

  Serial0.println("LCD init done.");
  lcd.setBacklight(HIGH);
  lcd.clear();
}

int displayLCD(int col, int row, char string[]) {
  lcd.setCursor(0, row);

  char clearLine[17];
  memset(clearLine, ' ', 16);
  clearLine[16] = '\0';
  lcd.print(clearLine);

  lcd.setCursor(col, row);
  lcd.print(string);

  return 0;
}

int isButton1Pressed() {
  static ulong hitTime = 0;
  static ulong releaseTime = 0;
  static int heldFlag = 0;
  static int prevRead = HIGH;
  int Read = digitalRead(BUTTON1);
  int result = 0;

  if (Read == LOW && prevRead == HIGH) {
    hitTime = millis();
  }

  if (Read == LOW && prevRead == LOW) {
    if ((millis() - hitTime > debounceDelay) && !heldFlag) {
      heldFlag = 1;
      result = 1;
    }
  }

  if (Read == HIGH && prevRead == LOW) {
    releaseTime = millis();
  }

  if (Read == HIGH && prevRead == HIGH) {
    if (millis() - releaseTime > debounceDelay) {
      heldFlag = 0;
    }
  }

  prevRead = Read;
  return result;
}

int isButton2Pressed() {
  static ulong hitTime = 0;
  static ulong releaseTime = 0;
  static int heldFlag = 0;
  static int prevRead = HIGH;
  int Read = digitalRead(BUTTON2);
  int result = 0;

  if (Read == LOW && prevRead == HIGH) {
    hitTime = millis();
  }

  if (Read == LOW && prevRead == LOW) {
    if ((millis() - hitTime > debounceDelay) && !heldFlag) {
      heldFlag = 1;
      result = 1;
    }
  }

  if (Read == HIGH && prevRead == LOW) {
    releaseTime = millis();
  }

  if (Read == HIGH && prevRead == HIGH) {
    if (millis() - releaseTime > debounceDelay) {
      heldFlag = 0;
    }
  }

  prevRead = Read;
  return result;
}

int readHX711() {
  data = 0;

  for (int i = 0; i < 24; i++) {
    digitalWrite(HX711_SCK, HIGH);
    delayMicroseconds(2);
    data = (data << 1) | digitalRead(HX711_DOUT);
    digitalWrite(HX711_SCK, LOW);
    delayMicroseconds(2);
  }

  digitalWrite(HX711_SCK, HIGH);
  delayMicroseconds(2);
  digitalWrite(HX711_SCK, LOW);
  delayMicroseconds(2);

  digitalWrite(HX711_SCK, HIGH);
  delayMicroseconds(2);
  digitalWrite(HX711_SCK, LOW);
  delayMicroseconds(2);

  digitalWrite(HX711_SCK, HIGH);
  delayMicroseconds(2);
  digitalWrite(HX711_SCK, LOW);
  delayMicroseconds(2);

  data = data ^ 0x800000;
  return data;
}

int readAverage(int samplePeriod) {
  static uint samples[5] = {0};
  static uint counter = 0;

  samples[counter] = readHX711();
  counter++;
  counter %= 5;

  uint sum = 0;
  for (int i = 0; i < 5; i++) {
    sum += samples[i];
  }

  delay(samplePeriod);
  return sum / 5;
}

Calibration Calibrate() {
  cal.calLow = 0;
  cal.calHigh = 0;

  Serial0.println("Calibrating empty chair...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cal empty...");

  for (int i = 0; i < 10; i++) readAverage(100);
  for (int i = 0; i < 10; i++) cal.calLow += readAverage(100);
  cal.calLow /= 10;

  Serial0.println("Sit on chair now!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sit now");
  delay(5000);

  Serial0.println("Calibrating occupied chair...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cal sitting...");

  for (int i = 0; i < 30; i++) cal.calHigh += readAverage(100);
  cal.calHigh /= 30;

  cal.calLowThresh = (cal.calHigh - cal.calLow) * 0.2 + cal.calLow;
  cal.calHighThresh = (cal.calHigh - cal.calLow) * 0.6 + cal.calLow;

  printf0("calLow: %d\n", cal.calLow);
  printf0("calLowThresh: %d\n", cal.calLowThresh);
  printf0("calHigh: %d\n", cal.calHigh);
  printf0("calHighThresh: %d\n", cal.calHighThresh);

  Serial0.println("Calibration done.");
  return cal;
}
