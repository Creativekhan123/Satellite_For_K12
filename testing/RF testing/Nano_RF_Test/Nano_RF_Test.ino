#include <SoftwareSerial.h>

// Create a virtual serial port: RX = Pin D2, TX = Pin D3
SoftwareSerial hc12(2, 3);

void setup() {
  // Start serial communication with the HC-12 radio at 9600 baud
  hc12.begin(9600);
  Serial.begin(9600);

}

void loop() {
  // Send the text "hello" over the radio
  hc12.println("hello");
  Serial.println("Hellow printed");
  // Wait for 3 seconds (3000 milliseconds)
  delay(3000);
}