#include "SeatSensor.h"
#include <cstdint>
#include <Arduino.h>

#define SAMPLE_INTERVAL 100     //ms between samples
#define CAL_SAMPLES     64      //Samples taken during calibration
#define SIT_THRESHOLD   100000  //Units above raw HX711 output that triggers seated condition
#define ARR_SIZE        10

#define LOW  0
#define HIGH 1

SeatSensor::SeatSensor(uint8_t pinDout, uint8_t pinSck){
    this->pinDout = pinDout;
    this->pinSck = pinSck;
    
    for(int i = 0; i < ARR_SIZE; i++) {this->avgArr[i] = 0;}
    this->arrFull = false;
    this->arrIndex = 0;
    this->lastAverage = 0;
    this->baseline = 0;
    this->calibrated = false;
    this->lastSampleTime = 0;

    pinMode(pinSck,  OUTPUT);
    pinMode(pinDout, INPUT);
    digitalWrite(pinSck, LOW);
}

int32_t SeatSensor::readHX711(){
    int32_t raw = 0;

    //24 pulses reads data from Dout
    for (int i=0; i<24; i++) {
        digitalWrite(this->pinSck, HIGH);
        delayMicroseconds(2);
        raw = (raw << 1) | digitalRead(this->pinDout);
        digitalWrite(this->pinSck, LOW);
        delayMicroseconds(2);
    }

    //1 extra pulse, sets gain to 128 on channel A
    digitalWrite(this->pinSck, HIGH);
    delayMicroseconds(2);
    digitalWrite(this->pinSck, LOW);
    delayMicroseconds(2);

    if (raw & 0x800000) {raw |= 0xFF000000;}  //Convert 24-bit 2s complement to signed 32-bit
    //Serial.print("RAW");
    //Serial.println(raw);
    return raw;
}

int32_t SeatSensor::computeAvg(){
    int count;
    if(arrFull) {count = ARR_SIZE;}
    else {count = arrIndex;}
    if (count == 0) {return 0;}
    int64_t sum = 0;  
    for (int i = 0; i < count; i++) {sum += avgArr[i];}
    return (sum / count);
}

void SeatSensor::pushSample(int32_t sample) {
    avgArr[arrIndex] = sample;
    arrIndex = (arrIndex + 1) % ARR_SIZE;
    if (arrIndex == 0) {arrFull = true;}        //Once samples is full, sets the buffer to full so we know avaerages are accurate 
}

void SeatSensor::calibrate(){
    int64_t sum = 0;                            //prevent overflow, use 64 bit int
    for(int i = 0; i < CAL_SAMPLES; i++){
        sum += readHX711();
        delay(20);                              //small delay
    }
    baseline = (int32_t) sum / CAL_SAMPLES;     //type cast that shit back to 32
    calibrated = true;
}

bool SeatSensor::update(){
    unsigned long now = millis();
    if(now - lastSampleTime < SAMPLE_INTERVAL) {return false;}
    lastSampleTime = now;

    int32_t raw = readHX711();
    pushSample(raw);
    lastAverage = computeAvg();
    //Serial.print("lastAverage");
    //Serial.println(lastAverage);
    return true;
}

bool SeatSensor::isOccupied(){
    if (!calibrated || !arrFull) return false;

    int32_t diff = lastAverage - baseline;
    if (diff < 0) diff = -diff;

    return diff > SIT_THRESHOLD;
}
