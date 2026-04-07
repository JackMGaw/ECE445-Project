#ifndef SEAT_SENSOR_H
#define SEAT_SENSOR_H

#include <Arduino.h>
#include <cstdint>

class SeatSensor {
    public:
        SeatSensor(uint8_t pinDout, uint8_t pinSck);
        void calibrate();
        bool update();
        bool isOccupied();
    private:
        uint8_t pinDout;               //HX711 data output
        uint8_t pinSck;                //HX711 clk input
        int32_t avgArr[10];        
        bool    arrFull;
        int32_t arrIndex;
        int32_t baseline;
        bool    calibrated;
        unsigned long lastSampleTime;
        int32_t lastAverage;

        int32_t readHX711();
        int32_t computeAvg();
        void    pushSample(int32_t sample);
};
#endif