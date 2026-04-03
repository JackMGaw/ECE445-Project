#include <Wire.h>
#include <Adafruit_LiquidCrystal.h>

#define BUTTON1 19
#define BUTTON2 20
#define LED 1

#define SDA_PIN 4
#define SCL_PIN 5

#define HX711_DOUT 18
#define HX711_SCK  8

#define printf(format, args...) do {char buf[256]; sprintf(buf, format, args); Serial.print(buf);} while (0)

typedef unsigned int uint;      // I did this because I saw jack did this lol
typedef unsigned long ulong;

//LCD
Adafruit_LiquidCrystal lcd(0);
char buffer[17];

//button
const ulong debounceDelay = 50;

//load cell
int data;
int value;

struct Calibration {
  int calLow;
  int calHigh;
  int calLowThresh;
  int calHighThresh;
} cal;

//control system
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


ulong alertTime = 5000;         // 30s
ulong needActivityTime = 10000;  // 10s

//function declarations
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

// setup

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(LED, OUTPUT);

  pinMode(HX711_SCK, OUTPUT);
  pinMode(HX711_DOUT, INPUT);
  digitalWrite(HX711_SCK, LOW);

  setupLCD(SDA_PIN, SCL_PIN);

  lcd.setCursor(0, 0);
  lcd.print("Starting...");
  Serial.println("System starting...");

  delay(1000);

  Calibrate();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  Serial.println("System ready.");
  delay(1000);
}


// loop


void loop() {
  value = readAverage(20);

  // Button1: increase timer
  if (isButton1Pressed()) {
    alertTime += 10000; // +10s
    Serial.print("Alert time = ");
    Serial.println(alertTime);
  }

  // Button2: decrease timer
  if (isButton2Pressed()) {
    if (alertTime > 10000) {
      alertTime -= 10000; // -10s
    }
    Serial.print("Alert time = ");
    Serial.println(alertTime);
  }

  updateState();
  updateLCD();

  delay(20);
}


// state machine


void updateState() {
  bool isSitting = readSeatOccupied();

  switch (currentState) {
    case EMPTY:
      digitalWrite(LED, LOW);

      if (isSitting) {
        currentState = SITTING;
        sitStartTime = millis();
        Serial.println("State -> SITTING");
      }
      break;

    case SITTING:
      digitalWrite(LED, HIGH);

      if (!isSitting) {
        currentState = EMPTY;
        Serial.println("State -> EMPTY");
      }
      else if (millis() - sitStartTime >= alertTime) {
        currentState = ALARM;
        Serial.println("State -> ALARM");
      }
      break;

    case ALARM:
      // using LED instead of buzzer for now~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      digitalWrite(LED, (millis() / 300) % 2);

      if (!isSitting) {
        currentState = CHECK_ACTIVITY;
        activityStartTime = millis();
        Serial.println("State -> CHECK_ACTIVITY");
      }
      break;

    case CHECK_ACTIVITY:
      // keep alarm if resitted
      if (isSitting) {
        currentState = ALARM;
        Serial.println("State -> ALARM");
      }
      else {
        if (detectRealActivity()) {
          if (millis() - activityStartTime >= needActivityTime) {
            currentState = CAN_SIT_AGAIN;
            Serial.println("State -> CAN_SIT_AGAIN");
          }
        } else {
          // if no activity,restart timer
          activityStartTime = millis();
        }
      }
      break;

    case CAN_SIT_AGAIN:
      digitalWrite(LED, HIGH);

      if (isSitting) {
        currentState = SITTING;
        sitStartTime = millis();
        Serial.println("State -> SITTING");
      }
      break;
  }
}


// seat logic


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

// waiting for IMU logic~~~~~~~~~~~~~~~~~~~~
bool detectRealActivity() {
  return true;
}


// LCD


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
    Serial.println("Could not init backpack. Check wiring.");
    while (1);
  }

  Serial.println("LCD init done.");
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


// buttons


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


// HX711


int readHX711() {
  data = 0;

  for (int i = 0; i < 24; i++) {
    digitalWrite(HX711_SCK, HIGH);
    delayMicroseconds(2);
    data = (data << 1) | digitalRead(HX711_DOUT);
    digitalWrite(HX711_SCK, LOW);
    delayMicroseconds(2);
  }

  // Channel A, gain 64
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

  Serial.println("Calibrating empty chair...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cal empty...");

  for (int i = 0; i < 10; i++) readAverage(100);
  for (int i = 0; i < 10; i++) cal.calLow += readAverage(100);
  cal.calLow /= 10;

  Serial.println("Sit on chair now!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sit now");
  delay(5000);

  Serial.println("Calibrating occupied chair...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cal sitting...");

  for (int i = 0; i < 30; i++) cal.calHigh += readAverage(100);
  cal.calHigh /= 30;

  cal.calLowThresh = (cal.calHigh - cal.calLow) * 0.2 + cal.calLow;
  cal.calHighThresh = (cal.calHigh - cal.calLow) * 0.6 + cal.calLow;

  printf("calLow: %d\n", cal.calLow);
  printf("calLowThresh: %d\n", cal.calLowThresh);
  printf("calHigh: %d\n", cal.calHigh);
  printf("calHighThresh: %d\n", cal.calHighThresh);

  Serial.println("Calibration done.");
  return cal;
}
