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
#define EXERCISE_TARGET_MS (15UL * 1000)      // kept for display/debug only
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
  Serial.println("[SEAT] Warming up sensor buffer...");
  unsigned long start = millis();
  while (millis() - start < 1500) {
    seat.update();
    delay(10);
  }
  Serial.println("[SEAT] Warmup done");
}

const char* stateName(State s) {
  switch (s) {
    case EMPTY: return "EMPTY";
    case SITTING: return "SITTING";
    case STANDING_IDLE: return "STANDING_IDLE";
    case ALARM: return "ALARM";
    case EXERCISING: return "EXERCISING";
    case COOLDOWN: return "COOLDOWN";
    default: return "UNKNOWN";
  }
}

void printHeartbeat(bool seated) {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < 1000) return;
  lastPrint = millis();

  Serial.print("[HB] state=");
  Serial.print(stateName(currentState));
  Serial.print(" seated=");
  Serial.print(seated ? "YES" : "NO");
  Serial.print(" sitCredit=");
  Serial.print(sitCredit);
  Serial.print(" alertTime=");
  Serial.print(alertTime);
  Serial.print(" activeMs=");
  Serial.print(activeMs);
  Serial.print(" BLE=");
  Serial.print(ChairMountBLE::isConnected() ? "YES" : "NO");
  Serial.print(" exerciseDone=");
  Serial.println(ChairMountBLE::hasExerciseCompleted() ? "YES" : "NO");
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

  printHeartbeat(seated);

  // button handling
  if (sitCredit == 0) {
    if (button1.isPressed()) {
      alertTime += STEP_MS;
      if (alertTime > MAX_ALERT_MS) alertTime = MAX_ALERT_MS;
      Serial.print("[BTN] alertTime -> ");
      Serial.print(alertTime / 1000);
      Serial.println(" s");
    }

    if (button2.isPressed()) {
      if (alertTime > MIN_ALERT_MS) alertTime -= STEP_MS;
      Serial.print("[BTN] alertTime -> ");
      Serial.print(alertTime / 1000);
      Serial.println(" s");
    }
  } else {
    if (button1.isPressed() || button2.isPressed()) {
      Serial.println("[BTN] ignored because sitting already started");
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

      if (!seated) {
        if (ChairMountBLE::isConnected()) {
          ChairMountBLE::clearExerciseCompleted();
          ChairMountBLE::startExerciseSession();
          activeMs = 0;
          Serial.println(">>> State: ALARM -> EXERCISING");
          currentState = EXERCISING;
        } else {
          Serial.println("[ALARM] standing detected but BLE not connected yet");
        }
      }
      break;

    case EXERCISING:
      activeMs += elapsed;

      if (seated) {
        ChairMountBLE::resetWearable();
        ChairMountBLE::clearExerciseCompleted();
        Serial.println(">>> State: EXERCISING -> ALARM (sat mid-exercise)");
        currentState = ALARM;
        activeMs = 0;
        alarm1.alarmOn();
      } else if (ChairMountBLE::hasExerciseCompleted()) {
        ChairMountBLE::resetWearable();
        ChairMountBLE::clearExerciseCompleted();
        alarm1.alarmOff();
        Serial.println(">>> State: EXERCISING -> COOLDOWN");
        currentState = COOLDOWN;
        cooldownStart = millis();
        sitCredit = 0;
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
  delay(1000);

  Serial.println();
  Serial.println("[SYS] ControlSystemTest2 booting...");

  Serial.println("[SEAT] Calibrating empty chair, do not sit");
  delay(3000);
  seat.calibrate();

  warmUpSeatSensor();

  ChairMountBLE::begin();

  Serial.println("[SYS] System ready");
  lastTick = millis();
}

void loop() {
  updateState();
}
