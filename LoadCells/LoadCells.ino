#define HX711_DOUT 3
#define HX711_SCK  46
#define LED 1
#define printf(format, args...) do {char buf[256]; sprintf(buf, format, args); Serial.print(buf);} while (0);   //Creates printf for Arduino, prints to serial

typedef unsigned int uint;
int readHX711();
int readAverage(int samplePeriod);
int data;
int value;
int init_value;
int z = 0;
struct Calibration{     //
  int calLow;
  int calHigh;
  int calLowThresh;
  int calHighThresh;
} cal;

void setup() {
  Serial.begin(115200);         
  pinMode(HX711_SCK, OUTPUT);
  pinMode(HX711_DOUT, INPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(HX711_SCK, LOW);
  delay(1000);
  init_value = readHX711();
}

void loop() {
  //if (z & 2) Serial.println(value);
  if (z == 0) {
    Calibrate();
    printf("calLow: %d\ncalLowThresh: %d\ncalHigh: %d\ncalHighThresh: %d\n", cal.calLow, cal.calLowThresh, cal.calHigh, cal.calHighThresh);
  }
  z++;
  value = readAverage(100); // rolling avg. of 10 samples taken every 100ms
  if(value < (cal.calLowThresh)) digitalWrite(LED, LOW);
  else if (value > (cal.calHighThresh)) digitalWrite(LED, HIGH);
}

int readHX711() {
  data = 0;
  for (int i = 0; i < 24; i++) {  // read 24 bits
    digitalWrite(HX711_SCK, HIGH);
    delayMicroseconds(2);
    data = (data << 1) | digitalRead(HX711_DOUT);
    digitalWrite(HX711_SCK, LOW);
    delayMicroseconds(2);
  }
  digitalWrite(HX711_SCK, HIGH);  // 3 extra pulses = Channel A, gain 64
  delayMicroseconds(2);
  digitalWrite(HX711_SCK, LOW);
  delayMicroseconds(2);
  digitalWrite(HX711_SCK, HIGH);
  delayMicroseconds(2);
  digitalWrite(HX711_SCK, LOW);
  delayMicroseconds(2);
  digitalWrite(HX711_SCK, HIGH);
  delayMicroseconds(2);
  digitalWrite(HX711_SCK, LOW);
  delayMicroseconds(2);
  data = data ^ 0x800000;
  //Serial.print("RAW read: "); //debug
  //Serial.println(data);      //debug
  return data;
}

int readAverage(int samplePeriod) { // read over 10 samples
    static uint samples[10];
    static uint counter = 0;
    samples[counter] = readHX711();
    counter++;
    counter %= 10;
    uint sum = 0;
    for (int i=0;i<10;i++) sum += samples[i];
    delay(samplePeriod);
    return sum / 10;
}
Calibration Calibrate() { // Finds expected range of values to expect form load cell
  Serial.print("Calibrating... ");
  for(int i=0;i<10;i++) readAverage(100); // gets rid of trash data
  for(int i=0;i<10;i++) cal.calLow += readAverage(100);// Take larger average to get clean data
  cal.calLow /= 10;
  delay(5000);
  Serial.println("Ready!"); //maybe beep after?
  DetectSitCalibrate(); // Tell user to sit, exit function when sit detected
  delay(1000);
  Serial.print("Calibrating... ");
  for(int i=0;i<30;i++) cal.calHigh += readAverage(100);// Take even larger average to get clean data
  cal.calHigh /= 30;
  cal.calLowThresh = (cal.calHigh - cal.calLow) * 0.2 + cal.calLow;  // can change constant if need be
  cal.calHighThresh = (cal.calHigh - cal.calLow) * 0.6 + cal.calLow; // can change constant if need be
  Serial.println("Done!");
  return cal;
}

void DetectSitCalibrate() { //TODO: properly detect sit by finding large change in data
  Serial.println("Sit on chair now!");
  delay(3000);
};

// typedef struct{
//   int calLow,        0 //nobody sitting on it
//   int calHigh,       1 //someone sat on it
//   int calLowThresh,  2 
//   int calHighThresh  3
// } cal;

// 0--2---------3-------1--->