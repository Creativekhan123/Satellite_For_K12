/*
 * RF Module Receiver Test - ESP32 Dev V1
 * ========================================
 * Listens for incoming data from an RF module and prints
 * every received message to the Serial Monitor.
 * Pair this with ESP32_RF_Test (transmitter) to verify
 * your RF link is working end-to-end.
 *
 * Wiring (ESP32 Dev V1):
 *   RF Module VCC  --> 3.3V or 5V (check your module)
 *   RF Module GND  --> GND
 *   RF Module TX   --> GPIO 33 (ESP32 RX2)
 *   RF Module RX   --> GPIO 32 (ESP32 TX2)
 *
 * Open Serial Monitor at 115200 baud to see received messages.
 */

// --- RF Module Pin Definitions ---
#define RXD2 33   // ESP32 receives from RF module TX
#define TXD2 32   // ESP32 transmits to  RF module RX

// How long to wait before reporting "no data" (milliseconds)
#define NO_DATA_TIMEOUT_MS 2000

unsigned long lastReceiveTime = 0;
unsigned long messageCount    = 0;
bool          waitingPrinted  = false;

void setup() {
  // Serial monitor
  Serial.begin(115200);
  delay(1000);

  // RF module serial port
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println("=========================================");
  Serial.println("  RF Receiver Test - ESP32 Dev V1       ");
  Serial.println("=========================================");
  Serial.println("  Listening on GPIO 33 (RX2)");
  Serial.println("  Baud rate: 9600");
  Serial.println("-----------------------------------------");
  Serial.println("  Waiting for incoming messages...");
  Serial.println();

  lastReceiveTime = millis();
}

void loop() {
  // --- Check for incoming data ---
  if (Serial2.available()) {
    String incoming = Serial2.readStringUntil('\n');
    incoming.trim(); // Strip \r and trailing spaces

    if (incoming.length() > 0) {
      messageCount++;
      lastReceiveTime = millis();
      waitingPrinted  = false;

      // Print received message with details
      Serial.print("[RX #");
      Serial.print(messageCount);
      Serial.print("]  Received: \"");
      Serial.print(incoming);
      Serial.print("\"  at ");
      Serial.print(lastReceiveTime);
      Serial.println(" ms");
    }
  }

  // --- No data timeout warning ---
  if ((millis() - lastReceiveTime >= NO_DATA_TIMEOUT_MS) && !waitingPrinted) {
    Serial.println("[INFO] No data received for 2 seconds. Check transmitter...");
    waitingPrinted = true;
  }
}
