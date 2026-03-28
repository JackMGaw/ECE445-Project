// This is the chairmount_ble.ino file
// The purpose of this file is to control the chair mount via Bluetooth.

#include <BluetoothSerial.h> // Include Bluetooth library
BluetoothSerial SerialBT; // Create Bluetooth Serial object

void setup() {
  Serial.begin(115200); // Begin serial communication at 115200 baud
  SerialBT.begin("Chair Mount"); // Start Bluetooth with device name "Chair Mount"
  Serial.println("Bluetooth Started"); // Print Bluetooth status
}

void loop() {
  if (SerialBT.available()) { // Check if data is available from Bluetooth
    char incomingData = SerialBT.read(); // Read data from Bluetooth
    // Process incoming data
  }
}