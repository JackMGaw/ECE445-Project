#include "Alarm.h"
#include <cstdint>
#include <Arduino.h>

#define ALARM 2

Alarm myAlarm(ALARM);

void setup() {
  Serial.begin(115200);
}

void loop() {
  delay(1000);
  myAlarm.alarmOn();
  delay(1000);
  myAlarm.alarmOff();
}
