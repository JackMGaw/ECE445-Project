#include "SeatSensor.h"
#include <cstdint>

#define HX711_DOUT      3
#define HX711_SCK       46
#define LED             1

typedef unsigned long ulong;
typedef unsigned int  uint;

SeatSensor seat(HX711_DOUT, HX711_SCK);

void setup() {
    Serial.begin(115200);
    seat.calibrate();           //MUST calibrate before using
    pinMode(LED, OUTPUT);
}

void loop() {
    seat.update();                  
    if (seat.isOccupied()) {digitalWrite(LED, HIGH);}
    else {digitalWrite(LED, LOW);}
}
