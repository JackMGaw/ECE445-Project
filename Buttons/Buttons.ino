#define BUTTON1 19
#define BUTTON2 20
#define LED 1

typedef unsigned int uint;            //I did this cause I hate writing out the whole data type
typedef unsigned long ulong; 
  
const ulong debounceDelay = 50;       //50ms to wait for button debounce

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON1, INPUT_PULLUP);     //Sets it as pullup, when unconnected and experinceing high impedance, pin reads HIGH, when connected to ground the pin read LOW
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
}

void loop() {
  digitalWrite(LED, HIGH);
  if(isButton1Pressed() || isButton2Pressed()){
    digitalWrite(LED, LOW);
    delay(3000);
  }
  
}

int isButton1Pressed(){               
  static ulong hitTime = 0;           //Time when a transition HIGH to LOW occurs
  static ulong releaseTime = 0;       //Time when a transition LOW to HIGH occurs
  static int heldFlag = 0;            //Flag for knowing if button is being held down
  static int prevRead = HIGH;         //Reading of button from previous function call
  int Read = digitalRead(BUTTON1);    //Current reading of the button
  int result = 0;                     //Result to be returned at the end
 
  if (Read == LOW && prevRead == HIGH) {hitTime = millis();}        //Rising edge, button press logic goes HIGH to LOW
  if (Read == LOW && prevRead == LOW) {             
    if ((millis() - hitTime > debounceDelay) && !heldFlag) {        //Debounce delay for button release
      heldFlag = 1;                                                 //Flag for indicating button is held
      result = 1;                                                   
    }
  }
  if (Read == HIGH && prevRead == LOW) {releaseTime = millis();}    //Falling edge, button release logic goes LOW to HIGH
  if (Read == HIGH && prevRead == HIGH){
      if(millis() - releaseTime > debounceDelay) {heldFlag = 0;}    //Debounce delay for button release
  }
  prevRead = Read;                                        //Set current read to prevRead for next function call
  return result;                                          //returns 1, button has been pressed; returns 0, button has not been pressed or is still being held
}

int isButton2Pressed(){               
  static ulong hitTime = 0;           //Time when a transition HIGH to LOW occurs
  static ulong releaseTime = 0;       //Time when a transition LOW to HIGH occurs
  static int heldFlag = 0;            //Flag for knowing if button is being held down
  static int prevRead = HIGH;         //Reading of button from previous function call
  int Read = digitalRead(BUTTON2);    //Current reading of the button
  int result = 0;                     //Result to be returned at the end
 
  if (Read == LOW && prevRead == HIGH) {hitTime = millis();}        //Rising edge, button press logic goes HIGH to LOW
  if (Read == LOW && prevRead == LOW) {             
    if ((millis() - hitTime > debounceDelay) && !heldFlag) {        //Debounce delay for button release
      heldFlag = 1;                                                 //Flag for indicating button is held
      result = 1;                                                   
    }
  }
  if (Read == HIGH && prevRead == LOW) {releaseTime = millis();}    //Falling edge, button release logic goes LOW to HIGH
  if (Read == HIGH && prevRead == HIGH){
      if(millis() - releaseTime > debounceDelay) {heldFlag = 0;}    //Debounce delay for button release
  }
  prevRead = Read;                                        //Set current read to prevRead for next function call
  return result;                                          //returns 1, button has been pressed; returns 0, button has not been pressed or is still being held
}