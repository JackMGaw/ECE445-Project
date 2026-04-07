#include "Alarm.h"
#include <cstdint>
#include <Arduino.h>

#define LOW 0
#define HIGH 1

Alarm::Alarm(uint8_t pin){
  this->pin = pin;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void Alarm::alarmOn(){
  digitalWrite(pin, HIGH);
}

void Alarm::alarmOff(){
  digitalWrite(pin, LOW);
}
