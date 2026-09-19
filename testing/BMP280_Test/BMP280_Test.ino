/*
 * BMP280 Barometric Pressure & Temperature Sensor Test - ESP32 Dev V1
 * ====================================================================
 * Reads and prints all available sensor values from BMP280:
 *   - Temperature    (°C & °F)
 *   - Pressure       (hPa, Pa, mmHg)
 *   - Altitude       (meters & feet, approximate)
 *
 * Wiring (ESP32 Dev V1 - I2C):
 *   BMP280 VCC  --> 3.3V
 *   BMP280 GND  --> GND
 *   BMP280 SDA  --> GPIO 21
 *   BMP280 SCL  --> GPIO 22
 *   BMP280 SDO  --> GND (sets I2C address to 0x76) [Default on most modules]
 *              OR
 *   BMP280 SDO  --> 3.3V (sets I2C address to 0x77)
 *
 * Library required:
 *   - Adafruit BMP280 Library
 *   - Adafruit Unified Sensor
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

#define SDA_PIN 21
#define SCL_PIN 22

// Sea level pressure for altitude calculation (hPa)
#define SEA_LEVEL_PRESSURE_HPA 1013.25

Adafruit_BMP280 bmp;
unsigned long readingCount = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=========================================");
  Serial.println("  BMP280 Sensor Test - ESP32 Dev V1     ");
  Serial.println("=========================================");

  Wire.begin(SDA_PIN, SCL_PIN);

  // Try 0x76 first (most common for standalone BMP280 modules), then 0x77
  bool status = bmp.begin(0x76);
  if (!status) {
    Serial.println("[INFO] Not found at 0x76. Trying alternate address 0x77...");
    status = bmp.begin(0x77);
  }

  if (!status) {
    Serial.println("\n[ERROR] Could not find a valid BMP280 sensor at 0x76 or 0x77!");
    Serial.println("Performing automatic I2C Bus Scan...");
    byte count = 0;
    for (byte i = 1; i < 127; i++) {
      Wire.beginTransmission(i);
      if (Wire.endTransmission() == 0) {
        Serial.print("  -> Found I2C device at address: 0x");
        if (i < 16) Serial.print("0");
        Serial.println(i, HEX);
        count++;
      }
    }
    if (count == 0) {
      Serial.println("  -> No I2C devices detected on the bus at all.");
      Serial.println("Check hardware wiring:");
      Serial.println("  - BMP280 VCC -> ESP32 3.3V");
      Serial.println("  - BMP280 GND -> ESP32 GND");
      Serial.println("  - BMP280 SDA -> ESP32 GPIO 21");
      Serial.println("  - BMP280 SCL -> ESP32 GPIO 22");
    }
    Serial.println("Halting. Fix wiring and reset.");
    while (1) delay(1000);
  }

  Serial.println("[OK] BMP280 detected and initialized successfully!");

  /* Set up oversampling and filter for flight / weather telemetry */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  Serial.println("Starting continuous readings...\n");
}

void loop() {
  float tempC      = bmp.readTemperature();
  float tempF      = (tempC * 9.0 / 5.0) + 32.0;
  float pressPa    = bmp.readPressure();
  float pressHpa   = pressPa / 100.0F;
  float pressMmHg  = pressPa * 0.00750062;
  float altMeters  = bmp.readAltitude(SEA_LEVEL_PRESSURE_HPA);
  float altFeet    = altMeters * 3.28084;

  readingCount++;

  Serial.println("========== BMP280 Readings ==========");
  Serial.print("Reading #       : ");
  Serial.println(readingCount);

  Serial.print("Temperature     : ");
  Serial.print(tempC, 2);
  Serial.print(" °C  |  ");
  Serial.print(tempF, 2);
  Serial.println(" °F");

  Serial.print("Pressure        : ");
  Serial.print(pressHpa, 2);
  Serial.print(" hPa  |  ");
  Serial.print(pressPa, 1);
  Serial.print(" Pa  |  ");
  Serial.print(pressMmHg, 2);
  Serial.println(" mmHg");

  Serial.print("Altitude (approx): ");
  Serial.print(altMeters, 2);
  Serial.print(" m   |  ");
  Serial.print(altFeet, 2);
  Serial.println(" ft");

  Serial.println("=====================================\n");

  delay(1000);
}
