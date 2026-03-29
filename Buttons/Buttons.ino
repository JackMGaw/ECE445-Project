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
  if(isButton1Pressed() && isButton2Pressed()){                //Only detects a SINGLE button press. Holding the button will not be interpretted as additional presses
    Serial.println("Pressed");          
    digitalWrite(LED, LOW);
    delay(3000);
    }
  else{
    Serial.println("Not Pressed");}
    digitalWrite(LED, HIGH);
}

int isButton1Pressed(){
  static ulong hitTime = 0;           //Time when a Button read is LOW (pressed)   
  static int heldFlag = 0;            //Flag for knowing if button is being held down
  static int prevRead = HIGH;         //Reading of button from previous function call
  int Read = digitalRead(BUTTON1);    //Current reading of the button
  if (Read == LOW && prevRead == HIGH) {hitTime = millis();}   //Record time when button goes from HIGH to LOW
  
  if (Read == LOW && prevRead == LOW) {             
    if ((millis() - hitTime > debounceDelay) && !heldFlag) {
      return 1;                       //Returns 1, the button has been pressed
      heldFlag = 1;                   //flag for if button continues being held
    }
  return 0;
  }

  if (Read == HIGH && prevRead == LOW) {heldFlag = 0;}    //Falling edge, button no longer held
  prevRead = Read;                                        //Set current read to prevRead for next function call
  return 0;
}

int isButton2Pressed(){
  static ulong hitTime = 0;           //Time when a Button read is LOW (pressed)   
  static int heldFlag = 0;            //Flag for knowing if button is being held down
  static int prevRead = HIGH;         //Reading of button from previous function call
  int Read = digitalRead(BUTTON2);    //Current reading of the button
  if (Read == LOW && prevRead == HIGH) {hitTime = millis();}   //Record time when button goes from HIGH to LOW
  
  if (Read == LOW && prevRead == LOW) {             
    if ((millis() - hitTime > debounceDelay) && !heldFlag) {
      return 1;                       //Returns 1, the button has been pressed
      heldFlag = 1;                   //flag for if button continues being held
    }
  return 0;
  }

  if (Read == HIGH && prevRead == LOW) {heldFlag = 0;}    //Falling edge, button no longer held
  prevRead = Read;                                        //Set current read to prevRead for next function call
  return 0;
}

