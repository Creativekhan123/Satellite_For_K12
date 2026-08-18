#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BME680.h>
#include "DFRobot_BMM350.h"
#include <ArduinoJson.h>

Adafruit_MPU6050 mpu;
Adafruit_BME680 bme; // I2C
DFRobot_BMM350_I2C bmm350(&Wire, 0x14);

// RF Module connected to Hardware Serial2
// We use pins 33 and 32 because they are safe on all ESP32 variants (including WROVERs)
#define RXD2 33
#define TXD2 32
float roll = 0;
float pitch = 0;

void setup() {
  // Serial monitor for PC
  Serial.begin(115200);
  
  // Initialize Hardware Serial 2 for the RF module
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); 
  
  // Small delay to let Serial Monitor catch up before printing
  delay(1000);
  Serial.println("Initializing ESP32 CanSat Transmitter...");

  // Initialize I2C for ESP32 (SDA = GPIO 21, SCL = GPIO 22 by default)
  Wire.begin();

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip! Check I2C wiring.");
  } else {
    Serial.println("MPU6050 initialized.");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }
  
  Serial.println("Attempting to connect to BME688...");
  bool bme_status = bme.begin(0x76);  // Try 0x76 first (common for BME688)
  if (!bme_status) {
    bme_status = bme.begin(0x77);     // Try 0x77 next (default Adafruit)
  }

  if (!bme_status) {
    Serial.println("Could not find a valid BME688 sensor! Check I2C wiring or address.");
  } else {
    Serial.println("BME688 initialized.");
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms
  }
  
  if (bmm350.begin() != 0) {
    Serial.println("Could not find BMM350 sensor at 0x14! Check I2C wiring.");
  } else {
    Serial.println("BMM350 initialized.");
    bmm350.setOperationMode(eBmm350NormalMode);
    bmm350.setPresetMode(BMM350_PRESETMODE_HIGHACCURACY, BMM350_DATA_RATE_25HZ);
    bmm350.setMeasurementXYZ();
  }
}

void loop() {
  
  
  // Read MPU6050 Data
  sensors_event_t a, g, temp_mpu;
  mpu.getEvent(&a, &g, &temp_mpu);
  
  // Calculate pitch and roll in degrees
  roll  = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / PI;
  pitch = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;
  
  float temperature = 0, humidity = 0, pressure = 0, gas = 0;
  
  // Read BME688 Data
  if (bme.performReading()) {
    temperature = bme.temperature;
    humidity    = bme.humidity;
    pressure    = bme.pressure / 100.0;    // Pa → hPa
    gas         = bme.gas_resistance / 1000.0; // Ω → kΩ
  }
  
  // Read BMM350 Data
  sBmm350MagData_t magData = bmm350.getGeomagneticData();
  float magX = magData.x;
  float magY = magData.y;
  float magZ = magData.z;

  // ---- Tilt-Compensated Compass Heading ----
  // Convert pitch & roll to radians
  float pitchRad = pitch * PI / 180.0;
  float rollRad  = roll  * PI / 180.0;

  // Correct the raw mag readings for the tilt of the sensor
  float magX_comp = magX * cos(pitchRad) + magZ * sin(pitchRad);
  float magY_comp = magX * sin(rollRad)  * sin(pitchRad)
                  + magY * cos(rollRad)
                  - magZ * sin(rollRad)  * cos(pitchRad);

  // Calculate heading angle 0-360°
  float heading = atan2(-magY_comp, magX_comp) * 180.0 / PI;
  if (heading < 0) heading += 360.0;
  // ------------------------------------------
  
  // Construct JSON
  StaticJsonDocument<512> doc;
  doc["temp"]    = temperature;
  doc["hum"]     = humidity;
  doc["press"]   = pressure;
  doc["gas"]     = gas;
  doc["roll"]    = roll;
  doc["pitch"]   = pitch;
  doc["magX"]    = magX;
  doc["magY"]    = magY;
  doc["magZ"]    = magZ;
  doc["heading"] = heading;   // <-- NEW: tilt-compensated compass heading
  
  String output;
  serializeJson(doc, output);
  
  // Send over RF and print to Serial for debugging
  Serial2.println(output);
  Serial.print("Sending: ");
  Serial.println(output);
  
  delay(1000);
}
