#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 bme; // I2C

void setup() {
  Serial.begin(9600);
  
  Serial.println("BME280 Sensor Test Started!");

  // Initialize the sensor. 
  // Common BME280 I2C addresses are 0x76 or 0x77.
  bool status = bme.begin(0x76);  
  if (!status) {
    // If 0x76 fails, try the alternative address
    status = bme.begin(0x77); 
    if (!status) {
      Serial.println("CRITICAL ERROR: Could not find a valid BME280 sensor!");
      Serial.println("Check your wiring: VCC to 3.3V/5V, GND to GND, SDA to A4, SCL to A5");
      while (1); // Halt execution if sensor is not found
    }
  }

  Serial.println("BME280 Initialized successfully!");
  Serial.println("-------------------------------------");
}

void loop() {
  // Read and print temperature
  Serial.print("Temperature = ");
  Serial.print(bme.readTemperature());
  Serial.println(" *C");

  // Read and print humidity
  Serial.print("Humidity = ");
  Serial.print(bme.readHumidity());
  Serial.println(" %");

  // Read and print pressure (divided by 100 to convert Pascals to hPa)
  Serial.print("Pressure = ");
  Serial.print(bme.readPressure() / 100.0F);
  Serial.println(" hPa");

  // Calculate and print approximate altitude (assumes standard sea level pressure 1013.25)
  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(1013.25));
  Serial.println(" m");

  Serial.println("-------------------------------------");
  
  delay(2000); // Wait 2 seconds before the next reading
}
