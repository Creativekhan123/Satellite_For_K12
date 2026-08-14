// ESP32 Hardware Serial 2
// Connect RF Module TX to GPIO 33
// Connect RF Module RX to GPIO 32
#define RXD2 33
#define TXD2 32

void setup() {
  // Serial monitor for viewing on PC
  Serial.begin(9600);
  
  // Hardware Serial for RF module
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  // Adding a small delay so the serial monitor catches the first print
  delay(1000);
  Serial.println("ESP32 RF Receiver Test Started!");
  Serial.println("Waiting for data...");
}

void loop() {
  // Check if data is coming from the RF module
  if (Serial2.available()) {
    // Read the incoming text until it sees a new line
    String incoming = Serial2.readStringUntil('\n');
    incoming.trim(); // Remove any extra hidden characters/spaces
    
    if (incoming.length() > 0) {
      Serial.print("Received: ");
      Serial.println(incoming);
    }
  }
}