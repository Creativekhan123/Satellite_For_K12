/*
 * BME688 Sensor Test - ESP32 Dev V1
 * ===================================
 * Reads and prints all available sensor values:
 *   - Temperature (°C & °F)
 *   - Humidity (%)
 *   - Pressure (hPa)
 *   - Approximate Altitude (m)
 *   - Gas Resistance (KOhms)
 *   - Air Quality Index (estimated)
 *
 * Wiring (ESP32 Dev V1):
 *   BME688 VCC  --> 3.3V
 *   BME688 GND  --> GND
 *   BME688 SDA  --> GPIO 21
 *   BME688 SCL  --> GPIO 22
 *
 * Library: Adafruit BME680 Library (works with BME688)
 *          Adafruit Unified Sensor
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

// --- Pin Definitions (ESP32 Dev V1 default I2C) ---
#define SDA_PIN 21
#define SCL_PIN 22

// Sea level pressure used for altitude calculation (hPa)
// Adjust this to your local sea-level pressure for accurate altitude
#define SEA_LEVEL_PRESSURE_HPA 1013.25

Adafruit_BME680 bme; // I2C mode

// --- Air Quality helper ---
// Returns a simple AQI label based on gas resistance
// Higher resistance = cleaner air
String getAirQuality(float gasResistanceKOhms) {
  if (gasResistanceKOhms > 300)       return "Excellent";
  else if (gasResistanceKOhms > 150)  return "Good";
  else if (gasResistanceKOhms > 75)   return "Fair";
  else if (gasResistanceKOhms > 30)   return "Poor";
  else                                 return "Very Poor";
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Allow serial monitor to connect

  Serial.println("========================================");
  Serial.println("   BME688 Sensor Test - ESP32 Dev V1   ");
  Serial.println("========================================");

  // Initialize I2C with ESP32 pins
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize BME688
  if (!bme.begin()) {
    Serial.println("[ERROR] Could not find a valid BME688 sensor!");
    Serial.println("Check wiring:");
    Serial.println("  VCC  -> 3.3V");
    Serial.println("  GND  -> GND");
    Serial.println("  SDA  -> GPIO 21");
    Serial.println("  SCL  -> GPIO 22");
    Serial.println("Halting...");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("[OK] BME688 sensor found and initialized!");
  Serial.println();

  // --- Sensor configuration ---
  bme.setTemperatureOversampling(BME680_OS_8X);   // High oversampling for accuracy
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);     // Smooth out pressure/temp noise
  bme.setGasHeater(320, 150);                     // 320°C for 150ms (gas sensing)

  Serial.println("Sensor configuration applied.");
  Serial.println("Starting readings in 2 seconds...");
  Serial.println("----------------------------------------");
  delay(2000);
}

void loop() {
  // Trigger a measurement
  if (!bme.performReading()) {
    Serial.println("[WARNING] Failed to perform reading. Retrying...");
    delay(2000);
    return;
  }

  // --- Derived values ---
  float tempF    = (bme.temperature * 9.0 / 5.0) + 32.0;
  float altitude = 44330.0 * (1.0 - pow((bme.pressure / 100.0) / SEA_LEVEL_PRESSURE_HPA, 0.1903));
  float gasKOhms = bme.gas_resistance / 1000.0;
  String airQuality = getAirQuality(gasKOhms);

  // --- Print all readings ---
  Serial.println("========== BME688 Readings ==========");

  Serial.print("  Temperature  : ");
  Serial.print(bme.temperature, 2);
  Serial.print(" °C  /  ");
  Serial.print(tempF, 2);
  Serial.println(" °F");

  Serial.print("  Humidity     : ");
  Serial.print(bme.humidity, 2);
  Serial.println(" %");

  Serial.print("  Pressure     : ");
  Serial.print(bme.pressure / 100.0, 2);
  Serial.println(" hPa");

  Serial.print("  Altitude     : ");
  Serial.print(altitude, 2);
  Serial.println(" m  (approx)");

  Serial.print("  Gas Resist.  : ");
  Serial.print(gasKOhms, 2);
  Serial.println(" KΩ");

  Serial.print("  Air Quality  : ");
  Serial.println(airQuality);

  Serial.println("-------------------------------------");
  Serial.println();

  delay(3000); // Wait 3 seconds before next reading
}
