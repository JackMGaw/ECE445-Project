#include "Button.h"
#include <cstdint>
#include <Arduino.h>

#define LOW 0
#define HIGH 1

Button::Button(uint8_t pin){
  this->pin = pin;
  this->hitTime = 0;
  this->releaseTime = 0;
  this->heldFlag = 0;
  this->prevRead = HIGH;
  this->pressed = false;

  pinMode(pin, INPUT_PULLUP);
}

void Button::update(){
  int read = digitalRead(pin);    //Current reading of the button
  bool result = false;                     //Result to be returned at the end
 
  if (read == LOW && this->prevRead == HIGH) {this->hitTime = millis();}        //Rising edge, button press logic goes HIGH to LOW
  if (read == LOW && this->prevRead == LOW) {             
    if ((millis() - this->hitTime > Button::debounceDelay) && !this->heldFlag) {        //Debounce delay for button release
      this->heldFlag = true;                                                 //Flag for indicating button is held
      result = true;                                                   
    }
  }
  if (read == HIGH && this->prevRead == LOW) {this->releaseTime = millis();}    //Falling edge, button release logic goes LOW to HIGH
  if (read == HIGH && this->prevRead == HIGH){
      if(millis() - this->releaseTime > Button::debounceDelay) {this->heldFlag = 0;}    //Debounce delay for button release
  }
  this->prevRead = read;                                        //Set current read to prevRead for next function call
  this->pressed = result;                                          //returns 1, button has been pressed; returns 0, button has not been pressed or is still being held
}

bool Button::isPressed(){
  return this->pressed;
}

