#define BUT1 19
#define BUT2 20
#define LED 1
typedef unsigned int uint;        //I did this cause I hate writing out the whole data type
typedef unsigned long ulong; 

const ulong debounceDelay = 50;   //50ms to wait for button debounce
int prevBut1State = HIGH;       //Sets last stable state to HIGH, since this is when button is not pressed, static so it persists between repeated function calls
ulong int prevBut1Time = 0;

void setup() {
  Serial.begin(115200);
  pinMode(BUT1, INPUT_PULLUP);    //Sets it as pullup, when unconnected and experinceing high impedance, pin reads HIGH, when connected to ground the pin read LOW
  pinMode(LED, OUTPUT);
}

void loop() {
  if(isBut1Pressed()){
    Serial.println("Pressed");
    digitalWrite(LED, LOW);
    }
  else{
    Serial.println("Not Pressed");}
    digitalWrite(LED, HIGH);
}

int isBut1Pressed(){
  ulong currentTime = millis();
  int but1Reading = digitalRead(BUT1);
  static int currentBut1State;
  if(but1Reading != lastStableState) {lastDebounce = millis();}  //millis calls the time in ms since the program has started, returns ulong
  
  if(millis() - prevBut1Time > debounceDelay) {
    if(but1Reading == LOW && lastStableState == HIGH) {
      
  }
  lastStableState = but1Reading;
  return 0;
}

