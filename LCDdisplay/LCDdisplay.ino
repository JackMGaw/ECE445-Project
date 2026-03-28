#include <Wire.h>                    //Included so we can properly route the SDA and SCL for I2C communication
#include <Adafruit_LiquidCrystal.h>
#define SDA 4                        //Arbitrary pins I chose, you can change these at your convenience.
#define SCL 5

//Connect via I2C, default address #0 (A0-A2 not jumpered), we will keep address as 0, dont worry much about this
Adafruit_LiquidCrystal lcd(0);     

void setup() {
  Serial.begin(115200);
 
  //Function call Wire.begin() takes in two ints and assigns them as the pins for SDA and SCL
  //This is because it normally runs on arduino hardware, meaning we must reroute them for our specific MCU
  Wire.begin(SDA, SCL);        

  //Set up the LCD's number of columns and rows:
  if (!lcd.begin(16, 2)) {
    Serial.println("Could not init backpack. Check wiring.");
    while(1);
  }
  Serial.println("Backpack init'd.");

  /*lcd.setCursor(Col, Row) Sets the position (cursor) for where text will be printed to our LCD display has 2 rows and 16 columns.
  Our LCD display has 2 rows and 16 columns, characters that go over the number of availible columns are just not printed, they
  do not overflow to the next row, keep that in mind*/
  lcd.setCursor(0, 0);      
  lcd.setBacklight(HIGH);       //Sets backlight on so characters can be visible
  lcd.clear();
}

void loop() {
  lcd.setCursor(0,0);           //Sets cursor to col 0, row 0
  lcd.print(":)");              //Prints ":)" to the first row (row 0), taking up 2 columns (columns 0-1)
  lcd.setCursor(0,1);           //Sets cursor to col 0, row 1
  lcd.print(":D");              //Prints ":D" to the second row (row 1), taking up 2 columns (columns 0-1)
  delay(1000);                  //1 second delay
  lcd.clear();                  //Clears LCD screen of all text
  lcd.setCursor(0,0);            //Sets cursor to col 0, row 0
  lcd.print(":D");              //Prints ":D" to the first row (row 0), taking up 2 columns (columns 0-1)
  lcd.setCursor(0,1);           //Sets cursor to col 0, row 1
  lcd.print(":)");              //Prints ":)" to the second row (row 1), taking up 2 columns (columns 0-1)
  delay(1000);                  //1 second delay
  lcd.clear();                  //Clears LCD screen of all text
 
}