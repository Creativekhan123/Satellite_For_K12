#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"

// Adjust this value to your local sea level pressure for accurate altitude calculation
#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BMP3XX bmp; // I2C

void setup() {
  Serial.begin(9600);
  
  Serial.println("BMP390 Barometric Pressure Sensor Test Started!");

  // Initialize the sensor via I2C
  // The default I2C address for BMP390 is usually 0x77. 
  // If yours is 0x76, you would use: bmp.begin_I2C(0x76)
  if (!bmp.begin_I2C()) {   
    Serial.println("CRITICAL ERROR: Could not find a valid BMP390 sensor!");
    Serial.println("Check your wiring: VCC to 3.3V/5V, GND to GND, SDA to A4, SCL to A5");
    while (1); // Halt execution if sensor is not found
  }

  Serial.println("BMP390 Initialized successfully!");

  // Set up oversampling and filter initialization for high precision
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);
  
  Serial.println("-------------------------------------");
}

void loop() {
  // Tell BMP390 to begin measurement
  if (!bmp.performReading()) {
    Serial.println("Failed to perform reading :(");
    delay(2000);
    return;
  }

  // Read and print temperature
  Serial.print("Temperature = ");
  Serial.print(bmp.temperature);
  Serial.println(" *C");

  // Read and print pressure (divided by 100 to convert Pascals to hPa)
  Serial.print("Pressure = ");
  Serial.print(bmp.pressure / 100.0);
  Serial.println(" hPa");

  // Calculate and print approximate altitude based on sea level pressure
  Serial.print("Approx. Altitude = ");
  Serial.print(bmp.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.println("-------------------------------------");
  
  delay(2000); // Wait 2 seconds before the next reading
}
