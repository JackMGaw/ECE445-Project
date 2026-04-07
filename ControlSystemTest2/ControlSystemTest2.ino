#include "SeatSensor.h"
#include "Alarm.h"
#include "Button.h"
#include <Adafruit_LiquidCrystal.h>   //Adafruit LCD display library
#include <Wire.h>                     //Needed for LCD I2C 
#include <cstdint>
#include <string.h> 

//PIN DEFINITIONS
#define PIN_DOUT    3
#define PIN_SCK     46
#define PIN_ALARM   2
#define PIN_BUTTON1 19
#define PIN_BUTTON2 20
#define PIN_SDA     4
#define PIN_SCL     5

//TUNING CONSTANTS (these are in ms and are unsigned longs)
#define DEFAULT_ALERT_MS   (1UL * 15 * 1000)      //Default time between exercises, 20 min                 //AUGMNTED TO 15SEC FOR DEMO CHANGE BACK LATER
#define STEP_MS            (60UL * 1000)          //1 min increments
#define MIN_ALERT_MS       (1UL  * 15 * 1000)     //Floor: 5 min                                          //AUGMNTED TO 15SEC FOR DEMO CHANGE BACK LATER
#define MAX_ALERT_MS       (60UL * 60 * 1000)     //Ceiling: 60 min
#define EXERCISE_TARGET_MS (1UL  * 15 * 1000)     //5 min of movement                                     //AUGMNTED TO 15SEC FOR DEMO CHANGE BACK LATER
#define COOLDOWN_MS        (5UL * 1000)           //5s display

//STATES
enum State {EMPTY, SITTING, STANDING_IDLE, ALARM, EXERCISING, COOLDOWN};

//STATE MACHINE VARIABLES
State         currentState   = EMPTY;
unsigned long sitCredit      = 0;       //ms of net sitting time
unsigned long alertTime      = DEFAULT_ALERT_MS;
unsigned long activeMs       = 0;       //ms of confirmed exercise
unsigned long cooldownStart  = 0;
unsigned long lastTick       = 0;
unsigned long buttonMsgExpiry= 0;      //Controls how long button message shows on LCD

//OBJECT DECLARATIONS
SeatSensor              seat(PIN_DOUT, PIN_SCK);
Alarm                   alarm1(PIN_ALARM);
Button                  button1(PIN_BUTTON1);   //Increase alert time
Button                  button2(PIN_BUTTON2);   //Decrease alert time
Adafruit_LiquidCrystal  lcd(0);

//HANDLE BLE (replace in future once BLE has been solved)   <--- TEMPORARY!!!
bool getWearableActive() {
  return true;
}

//LCD HELPER FUNCTIONS (write into a class?)
//Formats two strings to LCD, clears the LCD prior to printing
void lcdPrint(const char* line1, const char* line2 = "") {      //creates char*, line2 = "" is a default arg incase 2nd arg is not filled
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);

  //print to serial monitoring for debugging
  Serial.print("[");
  Serial.print(line1);
  Serial.print(" | ");
  Serial.print(line2);
  Serial.println("]");
}

//Formats milliseconds into "MM:SS" in the provided buffer (needs 6+ chars)
void formatTime(unsigned long ms, char* buf) {      
  unsigned long totalSec = ms / 1000UL;
  unsigned long mins     = totalSec / 60UL;
  unsigned long secs     = totalSec % 60UL;
  sprintf(buf, "%02lu:%02lu", mins, secs);      //sprintf(destination buf, )
}

//Handles updates to LCD (avoid flickering with too many updates)
void updateLCD() {
  if (millis() < buttonMsgExpiry) return;        // If a button message is actively showing, don't overwrite it

  static unsigned long lastLCDUpdate = 0;
  if (millis() - lastLCDUpdate < 500UL) return;  //avoids flickering
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
      formatTime(EXERCISE_TARGET_MS - activeMs, tBuf);
      snprintf(line1, sizeof(line1), "Keep moving!");
      snprintf(line2, sizeof(line2), "Done in: %s", tBuf);
      lcdPrint(line1, line2);
      break;

    case COOLDOWN:
      lcdPrint("Nice work!", "Rest up.");
      break;
  }
}


//STATE MACHINE
void updateState() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastTick;
  lastTick = now;

  seat.update();
  button1.update();
  button2.update();

  bool seated  = seat.isOccupied();
  bool wearableActive = getWearableActive();

  // Button handling: only allowed when sitCredit is zero, prints to lcd and serial monitor wh
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
      if (!seated && wearableActive) {
        alarm1.alarmOff();      
        Serial.println(">>> State: ALARM -> EXERCISING");
        currentState = EXERCISING;
        activeMs     = 0;
      }
      break;

    case EXERCISING:
      activeMs += elapsed;
      if (seated) {
        Serial.println(">>> State: EXERCISING -> ALARM (sat mid-exercise)");
        currentState = ALARM;
        activeMs     = 0;
        alarm1.alarmOn();
      } else if (activeMs >= EXERCISE_TARGET_MS) {
        Serial.println(">>> State: EXERCISING -> COOLDOWN");
        alarm1.alarmOff();
        currentState  = COOLDOWN;
        cooldownStart = millis();
        sitCredit     = 0;
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

  //Set up LCD
  Wire.begin(PIN_SDA, PIN_SCL); 
  lcd.begin(16, 2);
  lcd.setBacklight(HIGH);
  lcdPrint("Starting...", "");
  delay(2000);

  //Calibrate seat sensor (tells user via LCD to not sit down)
  lcdPrint("Cal: empty chair", "Don't sit!");
  delay(3000);
  seat.calibrate();

  lcdPrint("System ready", "");
  delay(3000);

  lastTick = millis();
}

void loop() {
  updateState();
  updateLCD();
}
