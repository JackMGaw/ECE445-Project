#include "SeatSensor.h"
#include "Alarm.h"
#include "Button.h"
#include "chairmount_BLE.h"
#include <Arduino.h>
#include <cstdint>

// PIN DEFINITIONS
#define PIN_DOUT    18
#define PIN_SCK     8
#define PIN_ALARM   2
#define PIN_BUTTON1 19
#define PIN_BUTTON2 20

// TUNING CONSTANTS
#define DEFAULT_ALERT_MS   (15UL * 1000)      // demo: 15s
#define STEP_MS            (60UL * 1000)      // 1 min
#define MIN_ALERT_MS       (15UL * 1000)
#define MAX_ALERT_MS       (60UL * 60 * 1000)
#define EXERCISE_TARGET_MS (15UL * 1000)      // kept for compatibility
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

// OBJECTS
SeatSensor seat(PIN_DOUT, PIN_SCK);
Alarm alarm1(PIN_ALARM);
Button button1(PIN_BUTTON1);
Button button2(PIN_BUTTON2);

void warmUpSeatSensor() {
  Serial.println("[SEAT] Warming up...");
  unsigned long start = millis();
  while (millis() - start < 1500) {
    seat.update();
    delay(10);
  }
  Serial.println("[SEAT] Ready");
}

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
      Serial.print("[BTN] Alert time = ");
      Serial.print(alertTime / 1000);
      Serial.println(" s");
    }

    if (button2.isPressed()) {
      if (alertTime > MIN_ALERT_MS) alertTime -= STEP_MS;
      Serial.print("[BTN] Alert time = ");
      Serial.print(alertTime / 1000);
      Serial.println(" s");
    }
  } else {
    if (button1.isPressed() || button2.isPressed()) {
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

  Serial.println();
  Serial.println("[SYS] Booting...");
  Serial.println("[SEAT] Calibrating empty chair...");
  delay(3000);
  seat.calibrate();

  warmUpSeatSensor();

  ChairMountBLE::begin();

  Serial.println("[SYS] Ready");
  lastTick = millis();
}

void loop() {
  updateState();
}
