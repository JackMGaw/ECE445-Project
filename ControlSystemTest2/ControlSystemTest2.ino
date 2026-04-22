#include "SeatSensor.h"
#include "Alarm.h"
#include "Button.h"
#include "chairmount_BLE.h"
#include <Adafruit_LiquidCrystal.h>
#include <Wire.h>
#include <cstdint>
#include <string.h>

#define PIN_DOUT    3
#define PIN_SCK     46
#define PIN_ALARM   2
#define PIN_BUTTON1 19
#define PIN_BUTTON2 20
#define PIN_SDA     4
#define PIN_SCL     5

#define DEFAULT_ALERT_MS   (1UL * 15 * 1000)
#define STEP_MS            (60UL * 1000)
#define MIN_ALERT_MS       (1UL * 15 * 1000)
#define MAX_ALERT_MS       (60UL * 60 * 1000)
#define EXERCISE_TARGET_MS (1UL * 15 * 1000)
#define COOLDOWN_MS        (5UL * 1000)

enum State {EMPTY, SITTING, STANDING_IDLE, ALARM, EXERCISING, COOLDOWN};

State currentState = EMPTY;
unsigned long sitCredit = 0;
unsigned long alertTime = DEFAULT_ALERT_MS;
unsigned long activeMs = 0;
unsigned long cooldownStart = 0;
unsigned long lastTick = 0;
unsigned long buttonMsgExpiry = 0;
unsigned long lastHeartbeat = 0;

SeatSensor seat(PIN_DOUT, PIN_SCK);
Alarm alarm1(PIN_ALARM);
Button button1(PIN_BUTTON1);
Button button2(PIN_BUTTON2);
Adafruit_LiquidCrystal lcd(0);

const char* stateName(State s) {
  switch (s) {
    case EMPTY: return "EMPTY";
    case SITTING: return "SITTING";
    case STANDING_IDLE: return "STAND";
    case ALARM: return "ALARM";
    case EXERCISING: return "EXER";
    case COOLDOWN: return "COOL";
    default: return "UNK";
  }
}

void lcdPrint(const char* line1, const char* line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  Serial.print("[LCD] ");
  Serial.print(line1);
  Serial.print(" | ");
  Serial.println(line2);
}

void formatTime(unsigned long ms, char* buf) {
  unsigned long totalSec = ms / 1000UL;
  unsigned long mins = totalSec / 60UL;
  unsigned long secs = totalSec % 60UL;
  sprintf(buf, "%02lu:%02lu", mins, secs);
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
      formatTime(EXERCISE_TARGET_MS > activeMs ? (EXERCISE_TARGET_MS - activeMs) : 0, tBuf);
      snprintf(line1, sizeof(line1), "Keep moving!");
      snprintf(line2, sizeof(line2), "Done in: %s", tBuf);
      lcdPrint(line1, line2);
      break;
    case COOLDOWN:
      lcdPrint("Nice work!", "Rest up.");
      break;
  }
}

void printHeartbeat(bool seated) {
  if (millis() - lastHeartbeat < 1000UL) return;
  lastHeartbeat = millis();
  Serial.printf("[HB] state=%s seated=%s sitCredit=%lus alert=%lus active=%lus ble=%s end=%s\n",
    stateName(currentState),
    seated ? "YES" : "NO",
    sitCredit / 1000UL,
    alertTime / 1000UL,
    activeMs / 1000UL,
    ChairMountBLE::isConnected() ? "YES" : "NO",
    ChairMountBLE::hasExerciseCompleted() ? "YES" : "NO");
}

void updateState() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastTick;
  lastTick = now;

  seat.update();
  button1.update();
  button2.update();

  bool seated = seat.isOccupied();
  bool exerciseDone = ChairMountBLE::hasExerciseCompleted();
  printHeartbeat(seated);

  if (sitCredit == 0) {
    if (button1.isPressed()) {
      alertTime += STEP_MS;
      if (alertTime > MAX_ALERT_MS) alertTime = MAX_ALERT_MS;
      char buf[17];
      snprintf(buf, sizeof(buf), "Set: %lu min", alertTime / 60000UL);
      lcdPrint("Time increased!", buf);
      buttonMsgExpiry = millis() + 2000UL;
      Serial.printf("[BTN] Button1 pressed, alertTime=%lu sec\n", alertTime / 1000UL);
    }
    if (button2.isPressed()) {
      if (alertTime > MIN_ALERT_MS) alertTime -= STEP_MS;
      char buf[17];
      snprintf(buf, sizeof(buf), "Set: %lu min", alertTime / 60000UL);
      lcdPrint("Time decreased!", buf);
      buttonMsgExpiry = millis() + 2000UL;
      Serial.printf("[BTN] Button2 pressed, alertTime=%lu sec\n", alertTime / 1000UL);
    }
  } else {
    if (button1.isPressed() || button2.isPressed()) {
      lcdPrint("Can't alter time", "sitting began");
      buttonMsgExpiry = millis() + 2000UL;
      Serial.println("[BTN] Ignored button press after sitting began");
    }
  }

  switch (currentState) {
    case EMPTY:
      alarm1.alarmOff();
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
        alarm1.alarmOn();
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
      alarm1.alarmOn();
      if (!ChairMountBLE::isConnected()) {
        Serial.println("[ALARM] Waiting for BLE connection...");
      }
      if (!seated && ChairMountBLE::isConnected()) {
        Serial.println("[ALARM] User stood up, sending CMD_START");
        ChairMountBLE::clearExerciseCompleted();
        ChairMountBLE::startExerciseSession();
        alarm1.alarmOff();
        activeMs = 0;
        currentState = EXERCISING;
        Serial.println(">>> State: ALARM -> EXERCISING");
      }
      break;

    case EXERCISING:
      activeMs += elapsed;
      if (seated) {
        Serial.println(">>> State: EXERCISING -> ALARM (sat mid-exercise)");
        ChairMountBLE::resetWearable();
        activeMs = 0;
        currentState = ALARM;
        alarm1.alarmOn();
      } else if (exerciseDone) {
        Serial.println("[EXER] Wearable END received");
        ChairMountBLE::resetWearable();
        alarm1.alarmOff();
        currentState = COOLDOWN;
        cooldownStart = millis();
        sitCredit = 0;
        Serial.println(">>> State: EXERCISING -> COOLDOWN");
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

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("[SYS] Chair mount booting...");

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.begin(16, 2);
  lcd.setBacklight(HIGH);
  lcdPrint("Starting...", "");
  delay(1000);

  Serial.println("[SYS] Calibrating seat sensor, keep chair empty");
  lcdPrint("Cal: empty chair", "Don't sit!");
  delay(3000);
  seat.calibrate();
  Serial.println("[SYS] Seat calibration done");

  lcdPrint("System ready", "");
  delay(1000);

  ChairMountBLE::begin();
  lastTick = millis();
  lastHeartbeat = millis();
  Serial.println("[SYS] Setup complete");
}

void loop() {
  ChairMountBLE::update();
  updateState();
  updateLCD();
  delay(20);
}
