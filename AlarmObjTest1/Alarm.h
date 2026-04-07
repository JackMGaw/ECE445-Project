#ifndef ALARM_H
#define ALARM_H
#include <cstdint>
#include <Arduino.h>

class Alarm{
  public:
    Alarm(uint8_t pin);
    void alarmOn();
    void alarmOff();
  private:
    uint8_t pin;
};

#endif