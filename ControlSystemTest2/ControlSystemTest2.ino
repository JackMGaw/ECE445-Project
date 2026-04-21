#include "SeatSensor.h"
#include "Alarm.h"
#include "Button.h"
#include "chairmount_BLE.h"
#include <Adafruit_LiquidCrystal.h>
#include <Wire.h>
#include <cstdint>
#include <string.h>

// PIN DEFINITIONS
#define PIN_DOUT    3
#define PIN_SCK     46
#define PIN_ALARM   2
#define PIN_BUTTON1 19
#define PIN_BUTTON2 20
#define PIN_SDA     4
#define PIN_SCL     5

// TUNING CONSTANTS
#define DEFAULT_ALERT_MS   (1UL * 15 * 1000)
#define STEP_MS            (60UL * 1000)
#define MIN_ALERT_MS       (1UL * 15 * 1000)
#define MAX_ALERT_MS       (60UL * 60 * 1000)
#define EXERCISE_TARGET_MS (1UL * 15 * 1000)
#define COOLDOWN_MS        (5UL * 1000)

enum State {EMPTY, SITTING, STANDING_IDLE, ALARM, EXERCISING, COOLDOWN};

State         currentState    = EMPTY;
unsigned long sitCredit       = 0;
unsigned long alertTime       = DEFAULT_ALERT_MS;
unsigned long activeMs        = 0;
unsigned long cooldownStart   = 0;
unsigned long lastTick        = 0;
unsigned long buttonMsgExpiry = 0;

SeatSensor             seat(PIN_DOUT, PIN_SCK);
Alarm                  alarm1(PIN_ALARM);
Button                 button1(PIN_BUTTON1);
Button                 button2(PIN_BUTTON2);
Adafruit_LiquidCrystal lcd(0);

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
  unsigned long mins     = totalSec / 60UL;
  unsigned long secs     = totalSec % 60UL;
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
      lcdPrint("Ready", ChairMountBLE::isConnected() ? "BLE linked" : "BLE scanning");
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
      lcdPrint("Move around!", ChairMountBLE::isConnected() ? "Wearable ready" : "No BLE link");
      break;

    case EXERCISING:
      if (ChairMountBLE::hasExerciseCompleted()) {
        lcdPrint("Exercise done", "Finishing...");
      } else {
        snprintf(line1, sizeof(line1), "Keep moving!");
        snprintf(line2, sizeof(line2), "Waiting IMU...");
        lcdPrint(line1, line2);
      }
      break;

    case COOLDOWN:
      lcdPrint("Nice work!", "Rest up.");
      break;
  }
}

void updateState() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastTick;
  lastTick = now;

  seat.update();
  button1.update();
  button2.update();
  ChairMountBLE::update();

  bool seated = seat.isOccupied();

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
      else                     sitCredit  = 0;

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
      if (!seated) {
        ChairMountBLE::clearExerciseCompletedFlag();
        ChairMountBLE::startExerciseSession();
        Serial.println(">>> State: ALARM -> EXERCISING");
        currentState = EXERCISING;
        activeMs     = 0;
      }
      break;

    case EXERCISING:
      activeMs += elapsed;

      if (seated) {
        ChairMountBLE::resetWearable();
        Serial.println(">>> State: EXERCISING -> ALARM (sat mid-exercise)");
        currentState = ALARM;
        activeMs     = 0;
        alarm1.alarmOn();
      } else if (ChairMountBLE::hasExerciseCompleted()) {
        ChairMountBLE::resetWearable();
        Serial.println(">>> State: EXERCISING -> COOLDOWN");
        alarm1.alarmOff();
        currentState  = COOLDOWN;
        cooldownStart = millis();
        sitCredit     = 0;
      } else if (activeMs >= EXERCISE_TARGET_MS && !ChairMountBLE::isConnected()) {
        Serial.println(">>> BLE not connected, still waiting for wearable confirmation");
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

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.begin(16, 2);
  lcd.setBacklight(HIGH);
  lcdPrint("Starting...", "");
  delay(2000);

  lcdPrint("Cal: empty chair", "Don't sit!");
  delay(3000);
  seat.calibrate();

  ChairMountBLE::begin("ChairMountReceiver");

  lcdPrint("System ready", "");
  delay(3000);

  lastTick = millis();
}

void loop() {
  updateState();
  updateLCD();
}
