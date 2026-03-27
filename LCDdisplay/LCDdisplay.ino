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

  //Sets the position (cursor) for where text will be printed to, (A, B), A represents columns, B represents row
  //Our LCD display has 2 rows and 16 columns, Characters that go over the number of availible columns are just not printed
  //They do not overflow to the next row, keep that in mind
  lcd.setCursor(0, 0);      
  lcd.setBacklight(HIGH);       //Sets Backlight on so characters can be visible
  lcd.clear();
}

void loop() {
  lcd.clear();                  //Clears LCD if there is any text already being displayed
  lcd.setCursor(0,0);
  lcd.print(":)");    //Prints the word "Initializing" to the first row (row 0), taking up 12 columns (columns 0-11)
  delay(1000);                  //4 second delay
  lcd.clear();
  lcd.clear();                  //Clears LCD if there is any text already being displayed
  lcd.setCursor(0,0);
  lcd.print(":D");    //Prints the word "Initializing" to the first row (row 0), taking up 12 columns (columns 0-11)
  delay(1000);                  //4 second delay
  lcd.clear();         
 
}