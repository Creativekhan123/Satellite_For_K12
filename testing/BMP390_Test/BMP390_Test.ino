/*
 * BMP390 Barometric Pressure Sensor Test - ESP32 Dev V1
 * =======================================================
 * Reads and prints all available sensor values:
 *   - Temperature    (°C & °F)
 *   - Pressure       (hPa & Pa & mmHg)
 *   - Altitude       (meters & feet, approximate)
 *
 * Wiring (ESP32 Dev V1 - I2C):
 *   BMP390 VCC  --> 3.3V
 *   BMP390 GND  --> GND
 *   BMP390 SDA  --> GPIO 21
 *   BMP390 SCL  --> GPIO 22
 *   BMP390 SDO  --> GND  (sets I2C address to 0x76)
 *              OR
 *   BMP390 SDO  --> 3.3V (sets I2C address to 0x77)
 *
 * Library: Adafruit BMP3XX Library
 *          Adafruit Unified Sensor
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"

// --- Pin Definitions (ESP32 Dev V1 default I2C) ---
#define SDA_PIN 21
#define SCL_PIN 22

// --- I2C Address ---
// 0x77 if SDO pin is HIGH (or floating)
// 0x76 if SDO pin is LOW (connected to GND)
#define BMP390_I2C_ADDR 0x77

// Sea level pressure for altitude calculation (hPa)
// Adjust to your local sea-level pressure for accurate altitude
#define SEA_LEVEL_PRESSURE_HPA 1013.25

Adafruit_BMP3XX bmp;

// Tracks the number of successful readings
unsigned long readingCount = 0;

void setup() {
  Serial.begin(115200);
  delay(1000); // Allow serial monitor to connect

  Serial.println("=========================================");
  Serial.println("  BMP390 Sensor Test - ESP32 Dev V1     ");
  Serial.println("=========================================");

  // Initialize I2C with ESP32 pins
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize BMP390
  // Try primary address; if it fails try the alternate
  if (!bmp.begin_I2C(BMP390_I2C_ADDR)) {
    Serial.print("[INFO] Not found at 0x");
    Serial.print(BMP390_I2C_ADDR, HEX);
    Serial.println(". Trying alternate address 0x76...");

    if (!bmp.begin_I2C(0x76)) {
      Serial.println("[ERROR] Could not find a valid BMP390 sensor!");
      Serial.println("Check wiring:");
      Serial.println("  VCC  -> 3.3V");
      Serial.println("  GND  -> GND");
      Serial.println("  SDA  -> GPIO 21");
      Serial.println("  SCL  -> GPIO 22");
      Serial.println("  SDO  -> GND (addr 0x76) or 3.3V (addr 0x77)");
      Serial.println("Halting...");
      while (1) {
        delay(1000);
      }
    }
    Serial.println("[OK] BMP390 found at address 0x76");
  } else {
    Serial.println("[OK] BMP390 found at address 0x77");
  }

  // --- Sensor configuration (high precision mode) ---
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);

  Serial.println("Sensor configuration applied (High Precision Mode).");
  Serial.println("Starting readings in 2 seconds...");
  Serial.println("-----------------------------------------");
  delay(2000);
}

void loop() {
  // Trigger a measurement
  if (!bmp.performReading()) {
    Serial.println("[WARNING] Failed to perform reading. Retrying...");
    delay(2000);
    return;
  }

  readingCount++;

  // --- Derived values ---
  float tempF      = (bmp.temperature * 9.0 / 5.0) + 32.0;
  float pressHpa   = bmp.pressure / 100.0;
  float pressMmhg  = pressHpa * 0.750062;           // hPa to mmHg
  float altMeters  = bmp.readAltitude(SEA_LEVEL_PRESSURE_HPA);
  float altFeet    = altMeters * 3.28084;

  // --- Print all readings ---
  Serial.println("========== BMP390 Readings ==========");
  Serial.print("  Reading #      : ");
  Serial.println(readingCount);

  Serial.println();
  Serial.println("  [ Temperature ]");
  Serial.print("    Celsius    : ");
  Serial.print(bmp.temperature, 2);
  Serial.println(" °C");
  Serial.print("    Fahrenheit : ");
  Serial.print(tempF, 2);
  Serial.println(" °F");

  Serial.println();
  Serial.println("  [ Pressure ]");
  Serial.print("    hPa        : ");
  Serial.print(pressHpa, 4);
  Serial.println(" hPa");
  Serial.print("    Pa         : ");
  Serial.print(bmp.pressure, 2);
  Serial.println(" Pa");
  Serial.print("    mmHg       : ");
  Serial.print(pressMmhg, 2);
  Serial.println(" mmHg");

  Serial.println();
  Serial.println("  [ Altitude (approx) ]");
  Serial.print("    Meters     : ");
  Serial.print(altMeters, 2);
  Serial.println(" m");
  Serial.print("    Feet       : ");
  Serial.print(altFeet, 2);
  Serial.println(" ft");

  Serial.println("--------------------------------------");
  Serial.println();

  delay(2000); // Wait 2 seconds before next reading
}
