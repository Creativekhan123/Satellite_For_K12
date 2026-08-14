#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BME680.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

Adafruit_MPU6050 mpu;
Adafruit_BME680 bme; // I2C

// RF Module connected to D2 (RX) and D3 (TX)
SoftwareSerial rfSerial(2, 3); 

void setup() {
  Serial.begin(9600);
  rfSerial.begin(9600);
  
  Serial.println("Initializing CanSat Transmitter...");

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
  } else {
    Serial.println("MPU6050 initialized.");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }
  
  if (!bme.begin()) {
    Serial.println("Could not find a valid BME688 sensor, check wiring!");
  } else {
    Serial.println("BME688 initialized.");
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms
  }
}

void loop() {
  float roll = 0;
  float pitch = 0;
  
  // Read MPU6050 Data
  sensors_event_t a, g, temp_mpu;
  if (mpu.getEvent(&a, &g, &temp_mpu)) {
    // Calculate pitch and roll in degrees
    roll = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / PI;
    pitch = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;
  }
  
  float temperature = 0, humidity = 0, pressure = 0, gas = 0;
  
  // Read BME688 Data
  if (bme.performReading()) {
    temperature = bme.temperature;
    humidity = bme.humidity;
    pressure = bme.pressure / 100.0; // convert Pa to hPa
    gas = bme.gas_resistance / 1000.0; // convert Ohms to kOhms
  }
  
  // Construct JSON
  StaticJsonDocument<256> doc; 
  doc["temp"] = temperature;
  doc["hum"] = humidity;
  doc["press"] = pressure;
  doc["gas"] = gas;
  doc["roll"] = roll;
  doc["pitch"] = pitch;
  
  String output;
  serializeJson(doc, output);
  
  // Send data over RF and print to Serial for debugging
  rfSerial.println(output);
  Serial.print("Sending to RF: ");
  Serial.println(output);
  
  delay(1000); // Transmit every 1 second
}
