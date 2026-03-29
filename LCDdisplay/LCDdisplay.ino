#include <Wire.h>                    //Included so we can properly route the SDA and SCL for I2C communication
#include <Adafruit_LiquidCrystal.h>
#include <string.h>                  //Standard C++ library
#define SDA 4                        
#define SCL 5
  
Adafruit_LiquidCrystal lcd(0);       //Sets default address #0 (A0-A2 not jumpered)  

void setup() {
  Serial.begin(115200);
  setupLCD(SDA, SCL);                //Takes the pin values for the SDA and SCL lines  
}

void loop() {
  displayLCD(0, 0, "Hello");
  delay(1000);
  displayLCD(0, 1, "World");
  delay(1000);
  displayLCD(0, 0, "Good");
  delay(1000);
  displayLCD(0, 1, "Day");
  delay(1000);
}

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
