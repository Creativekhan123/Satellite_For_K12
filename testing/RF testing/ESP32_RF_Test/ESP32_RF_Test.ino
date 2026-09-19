/*
 * RF Module Transmitter Test - ESP32 Dev V1
 * ==========================================
 * Sends the message "Hello" every 500ms via the RF module
 * using ESP32 Hardware Serial 2.
 * Also prints TX confirmation to Serial Monitor.
 *
 * Wiring (ESP32 Dev V1):
 *   RF Module VCC  --> 3.3V or 5V (check your module)
 *   RF Module GND  --> GND
 *   RF Module TX   --> GPIO 33 (ESP32 RX2)
 *   RF Module RX   --> GPIO 32 (ESP32 TX2)
 *
 * Open Serial Monitor at 115200 baud to see transmission log.
 */

// --- RF Module Pin Definitions (matches main transmitter) ---
#define RXD2 33   // ESP32 receives from RF module TX
#define TXD2 32   // ESP32 transmits to  RF module RX

// Message to send
#define TEST_MESSAGE "Hello"

// Interval between transmissions (milliseconds)
#define TX_INTERVAL_MS 500

unsigned long lastTxTime    = 0;
unsigned long messageCount  = 0;

void setup() {
  // Serial monitor
  Serial.begin(115200);
  delay(1000);

  // RF module serial port
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println("=========================================");
  Serial.println("  RF Transmitter Test - ESP32 Dev V1    ");
  Serial.println("=========================================");
  Serial.println("  Sending \"" TEST_MESSAGE "\" every 500 ms");
  Serial.println("  RF TX --> GPIO 32  |  RF RX <-- GPIO 33");
  Serial.println("-----------------------------------------");
  Serial.println();

  delay(500);
}

void loop() {
  unsigned long now = millis();

  // Send every TX_INTERVAL_MS
  if (now - lastTxTime >= TX_INTERVAL_MS) {
    lastTxTime = now;
    messageCount++;

    // Transmit the message over RF
    Serial2.println(TEST_MESSAGE);

    // Log to Serial Monitor
    Serial.print("[TX #");
    Serial.print(messageCount);
    Serial.print("]  Sent: \"");
    Serial.print(TEST_MESSAGE);
    Serial.print("\"  at ");
    Serial.print(now);
    Serial.println(" ms");
  }
}
