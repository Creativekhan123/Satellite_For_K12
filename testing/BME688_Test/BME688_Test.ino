#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

Adafruit_BME680 bme; // I2C

void setup() {
  Serial.begin(9600);
  
  Serial.println("BME688 Sensor Test Started!");

  // Initialize the sensor
  if (!bme.begin()) {
    Serial.println("CRITICAL ERROR: Could not find a valid BME688 sensor!");
    Serial.println("Check your wiring: VCC to 3.3V/5V, GND to GND, SDA to A4, SCL to A5");
    while (1); // Halt execution if sensor is not found
  }

  Serial.println("BME688 Initialized successfully!");

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
  
  Serial.println("-------------------------------------");
}

void loop() {
  // Tell BME688 to begin measurement
  if (!bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    delay(2000);
    return;
  }

  Serial.print("Temperature = ");
  Serial.print(bme.temperature);
  Serial.println(" *C");

  Serial.print("Humidity = ");
  Serial.print(bme.humidity);
  Serial.println(" %");

  Serial.print("Pressure = ");
  Serial.print(bme.pressure / 100.0);
  Serial.println(" hPa");

  Serial.print("Gas Resistance = ");
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(" KOhms");

  Serial.println("-------------------------------------");
  
  delay(2000); // Wait 2 seconds before the next reading
}
