#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LiquidCrystal.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLEClient.h>
#include <cstdint>
#include <string.h>

// =========================
// Pin definitions
// =========================
#define PIN_DOUT    18
#define PIN_SCK     8
#define PIN_ALARM   2
#define PIN_BUTTON1 19
#define PIN_BUTTON2 20
#define PIN_SDA     4
#define PIN_SCL     5

// =========================
// BLE UUIDs (must match wearable)
// =========================
#define SERVICE_UUID      "12345678-1234-1234-1234-1234567890ab"
#define DATA_CHAR_UUID    "abcd1234-5678-1234-5678-1234567890ab"
#define CONTROL_CHAR_UUID "dcba4321-8765-4321-8765-ba0987654321"

// =========================
// Timing constants
// =========================
#define DEFAULT_ALERT_MS   (1UL * 15 * 1000)   // 15 s demo default
#define STEP_MS            (60UL * 1000)       // 1 min increments
#define MIN_ALERT_MS       (1UL * 15 * 1000)
#define MAX_ALERT_MS       (60UL * 60 * 1000)
#define EXERCISE_TARGET_MS (1UL * 15 * 1000)   // display target only; BLE END decides completion
#define COOLDOWN_MS        (5UL * 1000)
#define DEBOUNCE_MS        50UL

// =========================
// HX711 / seat tuning
// =========================
#define HX711_SAMPLE_INTERVAL_MS 100UL
#define HX711_BUFFER_SIZE        10
#define CAL_SAMPLES              64
#define SIT_THRESHOLD            100000L
#define RELEASE_THRESHOLD        60000L

// =========================
// States
// =========================
enum State { EMPTY, SITTING, STANDING_IDLE, ALARM, EXERCISING, COOLDOWN };
State currentState = EMPTY;

// =========================
// BLE packet definitions
// =========================
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
  uint8_t type;
  uint8_t nodeId;
  uint32_t packetId;
  uint32_t timestampMs;
  bool value;
};

// =========================
// Globals
// =========================
Adafruit_LiquidCrystal lcd(0);

unsigned long sitCredit       = 0;
unsigned long alertTime       = DEFAULT_ALERT_MS;
unsigned long activeMs        = 0;
unsigned long cooldownStart   = 0;
unsigned long lastTick        = 0;
unsigned long buttonMsgExpiry = 0;

// ----- Seat globals -----
int32_t hxBuffer[HX711_BUFFER_SIZE] = {0};
uint8_t hxIndex = 0;
bool hxBufferFull = false;
unsigned long lastHxSampleTime = 0;
int32_t baseline = 0;
int32_t lastAverage = 0;
bool seatOccupied = false;

// ----- BLE globals -----
BLEAdvertisedDevice* targetDevice = nullptr;
BLEClient* bleClient = nullptr;
BLERemoteCharacteristic* dataRemoteChar = nullptr;
BLERemoteCharacteristic* controlRemoteChar = nullptr;
BLEScan* bleScan = nullptr;

bool bleConnected = false;
bool bleShouldConnect = false;
volatile bool wearableEndReceived = false;
volatile bool wearableBoolReceived = false;
volatile bool lastWearableBoolValue = false;

// =========================
// Utility: button class inline
// =========================
class SimpleButton {
public:
  explicit SimpleButton(uint8_t pin) : pin(pin) {}

  void begin() {
    pinMode(pin, INPUT_PULLUP);
  }

  void update() {
    int reading = digitalRead(pin);
    unsigned long now = millis();

    if (reading != lastReading) {
      lastChangeTime = now;
      lastReading = reading;
    }

    if ((now - lastChangeTime) > DEBOUNCE_MS) {
      if (reading != stableState) {
        stableState = reading;
        if (stableState == LOW && !pressLatched) {
          pressedEvent = true;
          pressLatched = true;
        }
        if (stableState == HIGH) {
          pressLatched = false;
        }
      }
    }
  }

  bool isPressed() {
    bool out = pressedEvent;
    pressedEvent = false;
    return out;
  }

private:
  uint8_t pin;
  int lastReading = HIGH;
  int stableState = HIGH;
  bool pressLatched = false;
  bool pressedEvent = false;
  unsigned long lastChangeTime = 0;
};

SimpleButton button1(PIN_BUTTON1);
SimpleButton button2(PIN_BUTTON2);

// =========================
// Utility: alarm helpers
// =========================
void alarmOn() {
  digitalWrite(PIN_ALARM, HIGH);
}

void alarmOff() {
  digitalWrite(PIN_ALARM, LOW);
}

// =========================
// LCD helpers
// =========================
void lcdPrint(const char* line1, const char* line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);

  Serial.print("[");
  Serial.print(line1);
  Serial.print(" | ");
  Serial.print(line2);
  Serial.println("]");
}

void formatTime(unsigned long ms, char* buf) {
  unsigned long totalSec = ms / 1000UL;
  unsigned long mins = totalSec / 60UL;
  unsigned long secs = totalSec % 60UL;
  sprintf(buf, "%02lu:%02lu", mins, secs);
}

const char* stateName(State s) {
  switch (s) {
    case EMPTY: return "EMPTY";
    case SITTING: return "SITTING";
    case STANDING_IDLE: return "STANDING";
    case ALARM: return "ALARM";
    case EXERCISING: return "EXERC";
    case COOLDOWN: return "COOLDOWN";
    default: return "UNK";
  }
}

void updateLCD() {
  if (millis() < buttonMsgExpiry) return;

  static unsigned long lastLCDUpdate = 0;
  if (millis() - lastLCDUpdate < 500UL) return;
  lastLCDUpdate = millis();

  char line1[17];
  char line2[17];
  char tBuf[8];
  char aBuf[8];

  switch (currentState) {
    case EMPTY:
      lcdPrint("Ready", "Waiting...");
      break;

    case SITTING:
      formatTime(sitCredit, tBuf);
      formatTime(alertTime, aBuf);
      snprintf(line1, sizeof(line1), "Sitting");
      snprintf(line2, sizeof(line2), "%s / %s", tBuf, aBuf);
      lcdPrint(line1, line2);
      break;

    case STANDING_IDLE:
      formatTime(sitCredit, tBuf);
      snprintf(line1, sizeof(line1), "Standing");
      snprintf(line2, sizeof(line2), "Credit: %s", tBuf);
      lcdPrint(line1, line2);
      break;

    case ALARM:
      lcdPrint("Move around!", "Exercise needed");
      break;

    case EXERCISING:
      formatTime((activeMs < EXERCISE_TARGET_MS) ? (EXERCISE_TARGET_MS - activeMs) : 0, tBuf);
      snprintf(line1, sizeof(line1), "Keep moving!");
      snprintf(line2, sizeof(line2), "Done in: %s", tBuf);
      lcdPrint(line1, line2);
      break;

    case COOLDOWN:
      lcdPrint("Nice work!", "Rest up.");
      break;
  }
}

// =========================
// HX711 / seat helpers
// =========================
int32_t readHX711() {
  int32_t raw = 0;

  for (int i = 0; i < 24; i++) {
    digitalWrite(PIN_SCK, HIGH);
    delayMicroseconds(2);
    raw = (raw << 1) | digitalRead(PIN_DOUT);
    digitalWrite(PIN_SCK, LOW);
    delayMicroseconds(2);
  }

  // one extra pulse for channel A, gain 128
  digitalWrite(PIN_SCK, HIGH);
  delayMicroseconds(2);
  digitalWrite(PIN_SCK, LOW);
  delayMicroseconds(2);

  if (raw & 0x800000) {
    raw |= 0xFF000000;
  }

  return raw;
}

void pushHxSample(int32_t sample) {
  hxBuffer[hxIndex] = sample;
  hxIndex = (hxIndex + 1) % HX711_BUFFER_SIZE;
  if (hxIndex == 0) hxBufferFull = true;
}

int32_t computeHxAverage() {
  int count = hxBufferFull ? HX711_BUFFER_SIZE : hxIndex;
  if (count <= 0) return 0;

  int64_t sum = 0;
  for (int i = 0; i < count; i++) sum += hxBuffer[i];
  return (int32_t)(sum / count);
}

void calibrateSeat() {
  int64_t sum = 0;
  Serial.println("[SEAT] Calibrating empty chair...");
  lcdPrint("Cal: empty chair", "Don't sit!");
  delay(3000);

  for (int i = 0; i < CAL_SAMPLES; i++) {
    sum += readHX711();
    delay(20);
  }

  baseline = (int32_t)(sum / CAL_SAMPLES);

  for (int i = 0; i < HX711_BUFFER_SIZE; i++) {
    hxBuffer[i] = baseline;
  }
  hxIndex = 0;
  hxBufferFull = true;
  lastAverage = baseline;
  seatOccupied = false;

  Serial.print("[SEAT] baseline=");
  Serial.println(baseline);
}

void updateSeat() {
  unsigned long now = millis();
  if (now - lastHxSampleTime < HX711_SAMPLE_INTERVAL_MS) return;
  lastHxSampleTime = now;

  int32_t raw = readHX711();
  pushHxSample(raw);
  lastAverage = computeHxAverage();

  int32_t diff = lastAverage - baseline;
  int32_t absDiff = diff >= 0 ? diff : -diff;

  // Use absolute deviation so either polarity works.
  if (!seatOccupied && absDiff > SIT_THRESHOLD) {
    seatOccupied = true;
  } else if (seatOccupied && absDiff < RELEASE_THRESHOLD) {
    seatOccupied = false;
  }
}

bool isSeatOccupied() {
  return seatOccupied;
}

// =========================
// BLE helpers
// =========================
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      Serial.println("[BLE] Wearable found");
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
    Serial.print("[BLE] Bad packet length=");
    Serial.println((unsigned)length);
    return;
  }

  MotionPacket pkt;
  memcpy(&pkt, pData, sizeof(pkt));

  if (pkt.type == PACKET_BOOL) {
    wearableBoolReceived = true;
    lastWearableBoolValue = pkt.value;
    Serial.printf("[BLE RX] BOOL node=%u pkt=%lu value=%s\n",
      pkt.nodeId,
      pkt.packetId,
      pkt.value ? "TRUE" : "FALSE"
    );
  } else if (pkt.type == PACKET_END) {
    wearableEndReceived = true;
    Serial.printf("[BLE RX] END node=%u pkt=%lu\n", pkt.nodeId, pkt.packetId);
  } else {
    Serial.printf("[BLE RX] Unknown type=%u\n", pkt.type);
  }
}

void startBleScan() {
  if (bleConnected) return;
  Serial.println("[BLE] Scanning...");
  bleScan->start(5, false);
}

bool connectToWearable() {
  if (targetDevice == nullptr) return false;

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
  Serial.println("[BLE] Connected to wearable");
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
    Serial.println("[BLE] Disconnected");
    startBleScan();
  }
}

void sendWearableCommand(uint8_t cmd) {
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
  }
}

bool getWearableActive() {
  return wearableEndReceived;
}

// =========================
// State machine
// =========================
void updateState() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastTick;
  lastTick = now;

  updateSeat();
  button1.update();
  button2.update();
  bleLoop();

  bool seated = isSeatOccupied();
  bool wearableActive = getWearableActive();

  static unsigned long lastHb = 0;
  if (millis() - lastHb >= 1000) {
    lastHb = millis();
    Serial.printf("[HB] state=%s rawAvg=%ld base=%ld absDiff=%ld seated=%s ble=%s end=%s\n",
      stateName(currentState),
      (long)lastAverage,
      (long)baseline,
      (long)((lastAverage >= baseline) ? (lastAverage - baseline) : (baseline - lastAverage)),
      seated ? "YES" : "NO",
      bleConnected ? "YES" : "NO",
      wearableEndReceived ? "YES" : "NO");
  }

  if (sitCredit == 0) {
    if (button1.isPressed()) {
      alertTime += STEP_MS;
      if (alertTime > MAX_ALERT_MS) alertTime = MAX_ALERT_MS;

      char buf[17];
      snprintf(buf, sizeof(buf), "Set: %lu min", alertTime / 60000UL);
      lcdPrint("Time increased!", buf);
      buttonMsgExpiry = millis() + 2000UL;

      Serial.print("Button1 pressed: alertTime -> ");
      Serial.print(alertTime / 60000UL);
      Serial.println(" min");
    }
    if (button2.isPressed()) {
      if (alertTime > MIN_ALERT_MS) alertTime -= STEP_MS;

      char buf[17];
      snprintf(buf, sizeof(buf), "Set: %lu min", alertTime / 60000UL);
      lcdPrint("Time decreased!", buf);
      buttonMsgExpiry = millis() + 2000UL;

      Serial.print("Button2 pressed: alertTime -> ");
      Serial.print(alertTime / 60000UL);
      Serial.println(" min");
    }
  } else {
    if (button1.isPressed() || button2.isPressed()) {
      lcdPrint("Can't alter time", "sitting began");
      buttonMsgExpiry = millis() + 2000UL;
      Serial.println("Button pressed but ignored: sitting already started");
    }
  }

  switch (currentState) {
    case EMPTY:
      alarmOff();
      if (seated) {
        Serial.println(">>> State: EMPTY -> SITTING");
        currentState = SITTING;
      }
      break;

    case SITTING:
      sitCredit += elapsed;
      if (sitCredit > alertTime) sitCredit = alertTime;

      if (!seated) {
        Serial.println(">>> State: SITTING -> STANDING_IDLE");
        currentState = STANDING_IDLE;
      } else if (sitCredit >= alertTime) {
        Serial.println(">>> State: SITTING -> ALARM");
        currentState = ALARM;
        alarmOn();
      }
      break;

    case STANDING_IDLE:
      if (sitCredit > elapsed) sitCredit -= elapsed;
      else sitCredit = 0;

      if (seated) {
        Serial.println(">>> State: STANDING_IDLE -> SITTING");
        currentState = SITTING;
      } else if (sitCredit == 0) {
        Serial.println(">>> State: STANDING_IDLE -> EMPTY");
        currentState = EMPTY;
      }
      break;

    case ALARM:
      alarmOn();
      if (!seated) {
        wearableEndReceived = false;
        wearableBoolReceived = false;
        sendWearableCommand(CMD_START);
        alarmOff();
        Serial.println(">>> State: ALARM -> EXERCISING");
        currentState = EXERCISING;
        activeMs = 0;
      }
      break;

    case EXERCISING:
      activeMs += elapsed;

      if (seated) {
        Serial.println(">>> State: EXERCISING -> ALARM (sat mid-exercise)");
        currentState = ALARM;
        activeMs = 0;
        wearableEndReceived = false;
        alarmOn();
        sendWearableCommand(CMD_RESET);
      } else if (wearableActive) {
        Serial.println(">>> State: EXERCISING -> COOLDOWN");
        alarmOff();
        currentState = COOLDOWN;
        cooldownStart = millis();
        sitCredit = 0;
        sendWearableCommand(CMD_RESET);
        wearableEndReceived = false;
      }
      break;

    case COOLDOWN:
      if (millis() - cooldownStart >= COOLDOWN_MS) {
        Serial.println(">>> State: COOLDOWN -> EMPTY");
        currentState = EMPTY;
      }
      break;
  }
}

// =========================
// Setup / loop
// =========================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_ALARM, OUTPUT);
  alarmOff();

  pinMode(PIN_SCK, OUTPUT);
  pinMode(PIN_DOUT, INPUT);
  digitalWrite(PIN_SCK, LOW);

  button1.begin();
  button2.begin();

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.begin(16, 2);
  lcd.setBacklight(HIGH);
  lcdPrint("Starting...", "");
  delay(2000);

  calibrateSeat();

  BLEDevice::init("ChairMountReceiver");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  bleScan->setActiveScan(true);
  bleScan->setInterval(100);
  bleScan->setWindow(80);
  startBleScan();

  lcdPrint("System ready", "");
  delay(1000);

  lastTick = millis();
}

void loop() {
  updateState();
  updateLCD();
  delay(10);
}
