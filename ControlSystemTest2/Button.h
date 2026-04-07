#ifndef BUTTON_H
#define BUTTON_H

#include <cstdint>
#include <Arduino.h>

class Button{
    public:
        Button(uint8_t pin);
        bool isPressed();
        void update();
    private:
        const unsigned long debounceDelay = 50;   //50ms to wait for button debounce
        unsigned long hitTime;                    //Time when a transition HIGH to LOW occurs
        unsigned long releaseTime;                //Time when a transition LOW to HIGH occurs
        bool heldFlag;                            //Flag for knowing if button is being held down
        int prevRead;                             //Reading of button from previous function call
        uint8_t pin;
        bool pressed;
};

#endif