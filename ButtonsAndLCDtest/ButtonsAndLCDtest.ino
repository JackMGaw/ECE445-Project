#include <Wire.h>                    //Included so we can properly route the SDA and SCL for I2C communication
#include <Adafruit_LiquidCrystal.h>
#define BUTTON1 19
#define BUTTON2 20
#define LED 1
#define SDA 4                        //Arbitrary pins I chose, you can change these at your convenience.
#define SCL 5
typedef unsigned int uint;            //I did this cause I hate writing out the whole data type
typedef unsigned long ulong; 

Adafruit_LiquidCrystal lcd(0);   
const ulong debounceDelay = 50;       //50ms to wait for button debounce
int count = 5;
char buffer[17];

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON1, INPUT_PULLUP);     //Sets it as pullup, when unconnected and experinceing high impedance, pin reads HIGH, when connected to ground the pin read LOW
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  setupLCD(SDA, SCL);
  lcd.print(count);
}

void loop() {
  if(isButton1Pressed()){
    count++;
    sprintf(buffer, "%d", count);  //Converts count to a char array and puts it in char buffer
    displayLCD(0, 0, buffer); 
  }
  if(isButton2Pressed()){
    count--;
      sprintf(buffer, "%d", count);
    displayLCD(0, 0, buffer); 
  }      
}

//////////////////////FUNCTIONS FROM BUTTONS AND LCD FILES, COPIED AND PASTED INTO HERE
//BUTTONS BELOW
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

//LCD BELOW
void setupLCD(int sda, int scl){
  Wire.begin(sda, scl);                                           //Assigns which wires are SDA and SCL for I2C communicaiton
  if (!lcd.begin(16, 2)) {                                        //Begins the lcd and sets the cols and rows
    Serial.println("Could not init backpack. Check wiring.");
    while(1);
  }
  Serial.println("Backpack init'd.");
  
  lcd.setCursor(0, 0);          //lcd.setCursor(Col, Row) sets the position where text will be printed to display.
  lcd.setBacklight(HIGH);       //Sets backlight on so characters can be visible
  lcd.clear();                  //Clears screen of any previous text
};

int displayLCD(int col, int row, char string[]){          //0 for no error rasied, 1 for error rasied
  lcd.setCursor(0, row);  
  char clearLine[17];                                     //Creates clearLine to clear a specific line at a time
  memset(clearLine, ' ', 16);
  clearLine[16] = '\0';
  lcd.print(clearLine); 
  lcd.setCursor(col, row);
  lcd.print(string);
  if(sizeof(string) - col > 15){                          //Checks if string overflows the display, returns 1 if so, 0 if not
    Serial.println("String too big for screen");
    return 1;
  }
  return 0;
}
