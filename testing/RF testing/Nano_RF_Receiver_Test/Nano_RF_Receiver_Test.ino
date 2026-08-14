#include <SoftwareSerial.h>

// Create a virtual serial port: RX = Pin D2, TX = Pin D3
SoftwareSerial hc12(2, 3);

void setup() {
  // Start communication with your computer's Serial Monitor
  Serial.begin(9600);

  // Start serial communication with the HC-12 radio
  hc12.begin(9600);
}

void loop() {
  // Check if any message has arrived from the radio
  if (hc12.available()) {
    // Read the text line sent by the transmitter
    String incomingMessage = hc12.readStringUntil('\n');

    // Print the received message on your computer screen
    Serial.println(incomingMessage);
  }
  else{
  Serial.println("No Data Available");
  delay(1000);
}
}