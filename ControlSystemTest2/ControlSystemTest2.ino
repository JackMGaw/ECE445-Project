#include "SeatSensor.h"
#include "Alarm.h"
#include "Button.h"
#include "chairmount_BLE.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LiquidCrystal.h>
#include <cstdint>

// PIN DEFINITIONS
#define PIN_DOUT    2
#define PIN_SCK     1
#define PIN_ALARM   7
#define PIN_BUTTON1 5
#define PIN_BUTTON2 6
#define PIN_SDA     47
#define PIN_SCL     48

// TUNING CONSTANTS
#define DEFAULT_ALERT_MS   (15UL * 1000)
#define STEP_MS            (60UL * 1000)
#define MIN_ALERT_MS       (15UL * 1000)
#define MAX_ALERT_MS       (60UL * 60 * 1000)
#define EXERCISE_TARGET_MS (15UL * 1000)
#define COOLDOWN_MS        (5UL * 1000)

// STATES
enum State {EMPTY, SITTING, STANDING_IDLE, ALARM, EXERCISING, COOLDOWN};

// STATE MACHINE VARIABLES
State currentState = EMPTY;
unsigned long sitCredit = 0;
unsigned long alertTime = DEFAULT_ALERT_MS;
unsigned long activeMs = 0;
unsigned long cooldownStart = 0;
unsigned long lastTick = 0;
unsigned long buttonMsgExpiry = 0;

// OBJECTS
SeatSensor seat(PIN_DOUT, PIN_SCK);
Alarm alarm1(PIN_ALARM);
Button button1(PIN_BUTTON1);
Button button2(PIN_BUTTON2);
Adafruit_LiquidCrystal lcd(0);

// ---------- LCD HELPERS ----------

void lcdPrint(const char* line1, const char* line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
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
    case EXERCISING: return "EXERCISE";
    case COOLDOWN: return "COOLDOWN";
    default: return "UNKNOWN";
  }
}

void updateLCD() {
  if (millis() < buttonMsgExpiry) return;

  static unsigned long lastLCDUpdate = 0;
  if (millis() - lastLCDUpdate < 500UL) return;
  lastLCDUpdate = millis();

  char line1[17];
  char line2[17];
  char creditBuf[8];
  char alertBuf[8];

  bool bleConnected = ChairMountBLE::isConnected();
  bool exerciseDone = ChairMountBLE::hasExerciseCompleted();

  formatTime(sitCredit, creditBuf);
  formatTime(alertTime, alertBuf);

  switch (currentState) {
    case EMPTY:
      snprintf(line1, sizeof(line1), "EMPTY  BLE:%s", bleConnected ? "Y" : "N");
      snprintf(line2, sizeof(line2), "Timer %s", alertBuf);
      break;

    case SITTING:
      snprintf(line1, sizeof(line1), "SITTING BLE:%s", bleConnected ? "Y" : "N");
      snprintf(line2, sizeof(line2), "Sit %s/%s", creditBuf, alertBuf);
      break;

    case STANDING_IDLE:
      snprintf(line1, sizeof(line1), "STAND  BLE:%s", bleConnected ? "Y" : "N");
      snprintf(line2, sizeof(line2), "Credit %s", creditBuf);
      break;

    case ALARM:
      snprintf(line1, sizeof(line1), "ALARM  BLE:%s", bleConnected ? "Y" : "N");
      snprintf(line2, sizeof(line2), "Stand + move");
      break;

    case EXERCISING:{
      uint32_t packCount = ChairMountBLE::getPackCount();
      snprintf(line1, sizeof(line1), "MOVE   Done:%s", exerciseDone ? "Y" : "N");
      snprintf(line2, sizeof(line2), "BLE:%s pkts:%lu", bleConnected ? "Y" : "N", packCount);
      break;
    }
    case COOLDOWN:
      snprintf(line1, sizeof(line1), "Nice work!");
      snprintf(line2, sizeof(line2), "Resetting...");
      break;
  }

  lcdPrint(line1, line2);
}

// ---------- SEAT SENSOR ----------

void warmUpSeatSensor() {
  Serial.println("[SEAT] Warming up...");
  lcdPrint("Seat warming...", "");

  unsigned long start = millis();
  while (millis() - start < 1500) {
    seat.update();
    delay(10);
  }

  Serial.println("[SEAT] Ready");
  lcdPrint("Seat ready", "");
  delay(800);
}

// ---------- STATE MACHINE ----------

void updateState() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastTick;
  lastTick = now;

  ChairMountBLE::update();

  seat.update();
  button1.update();
  button2.update();

  bool seated = seat.isOccupied();

  if (sitCredit == 0) {
    if (button1.isPressed()) {
      alertTime += STEP_MS;
      if (alertTime > MAX_ALERT_MS) alertTime = MAX_ALERT_MS;

      char buf[17];
      snprintf(buf, sizeof(buf), "Timer: %lus", alertTime / 1000UL);
      lcdPrint("Time increased", buf);
      buttonMsgExpiry = millis() + 2000UL;

      Serial.print("[BTN] Alert time = ");
      Serial.print(alertTime / 1000);
      Serial.println(" s");
    }

    if (button2.isPressed()) {
      if (alertTime > MIN_ALERT_MS) alertTime -= STEP_MS;

      char buf[17];
      snprintf(buf, sizeof(buf), "Timer: %lus", alertTime / 1000UL);
      lcdPrint("Time decreased", buf);
      buttonMsgExpiry = millis() + 2000UL;

      Serial.print("[BTN] Alert time = ");
      Serial.print(alertTime / 1000);
      Serial.println(" s");
    }
  } else {
    if (button1.isPressed() || button2.isPressed()) {
      lcdPrint("Timer locked", "Already sitting");
      buttonMsgExpiry = millis() + 2000UL;
      Serial.println("[BTN] Ignored while sitting");
    }
  }

  switch (currentState) {
    case EMPTY:
      alarm1.alarmOff();
      if (seated) {
        Serial.println("[STATE] EMPTY -> SITTING");
        currentState = SITTING;
      }
      break;

    case SITTING:
      sitCredit += elapsed;
      if (sitCredit > alertTime) sitCredit = alertTime;

      if (!seated) {
        Serial.println("[STATE] SITTING -> STANDING_IDLE");
        currentState = STANDING_IDLE;
      } else if (sitCredit >= alertTime) {
        Serial.println("[STATE] SITTING -> ALARM");
        currentState = ALARM;
        alarm1.alarmOn();
      }
      break;

    case STANDING_IDLE:
      if (sitCredit > elapsed) sitCredit -= elapsed;
      else sitCredit = 0;

      if (seated) {
        Serial.println("[STATE] STANDING_IDLE -> SITTING");
        currentState = SITTING;
      } else if (sitCredit == 0) {
        Serial.println("[STATE] STANDING_IDLE -> EMPTY");
        currentState = EMPTY;
      }
      break;

    case ALARM:
      alarm1.alarmOn();

      if (!seated) {
        if (ChairMountBLE::isConnected()) {
          ChairMountBLE::clearExerciseCompleted();
          ChairMountBLE::resetPackCount();
          ChairMountBLE::startExerciseSession();
          activeMs = 0;
          Serial.println("[STATE] ALARM -> EXERCISING");
          currentState = EXERCISING;
        } else {
          Serial.println("[ALARM] Waiting for BLE connection");
        }
      }
      break;

    case EXERCISING:
      activeMs += elapsed;
      alarm1.alarmOff();
      if (seated) {
        ChairMountBLE::resetWearable();
        ChairMountBLE::clearExerciseCompleted();
        Serial.println("[STATE] EXERCISING -> ALARM");
        currentState = ALARM;
        activeMs = 0;
        alarm1.alarmOn();
      } else if (ChairMountBLE::hasExerciseCompleted()) {
        ChairMountBLE::resetWearable();
        ChairMountBLE::clearExerciseCompleted();
        alarm1.alarmOff();
        Serial.println("[STATE] EXERCISING -> COOLDOWN");
        currentState = COOLDOWN;
        cooldownStart = millis();
        sitCredit = 0;
      }
      break;

    case COOLDOWN:
      if (millis() - cooldownStart >= COOLDOWN_MS) {
        Serial.println("[STATE] COOLDOWN -> EMPTY");
        currentState = EMPTY;
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.begin(16, 2);
  lcd.setBacklight(HIGH);

  lcdPrint("Booting...", "");
  Serial.println();
  Serial.println("[SYS] Booting...");

  lcdPrint("Cal empty chair", "Do not sit");
  Serial.println("[SEAT] Calibrating empty chair...");
  delay(3000);
  seat.calibrate();

  warmUpSeatSensor();

  lcdPrint("Starting BLE...", "");
  ChairMountBLE::begin();

  lcdPrint("System ready", "");
  Serial.println("[SYS] Ready");
  delay(1000);

  lastTick = millis();
}

void loop() {
  updateState();
  updateLCD();
}
