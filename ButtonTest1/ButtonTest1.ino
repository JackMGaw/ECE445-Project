#include "Button.h"
#include <cstdint>

#define BUTTON1 19
#define BUTTON2 20
#define LED 1

Button button1(BUTTON1);

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
}

void loop() {
  delay(100);
  digitalWrite(LED, LOW);
  button1.update();

  if(button1.isPressed()){
    digitalWrite(LED, HIGH);
    Serial.println("Pressed");
    delay(3000);
  }
}
