#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BMP3XX.h>
#include "DFRobot_BMM350.h"
#include <ArduinoJson.h>

Adafruit_MPU6050 mpu;
Adafruit_BMP3XX bmp;                        // Replaces BME688
DFRobot_BMM350_I2C bmm350(&Wire, 0x14);

// Standard sea-level pressure for altitude calculation (adjust for your location)
#define SEALEVELPRESSURE_HPA (1013.25)

// RF Module — GPIO 33 (RX) and 32 (TX) are safe on all ESP32 variants
#define RXD2 32
#define TXD2 33

float roll  = 0;
float pitch = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  delay(1000);
  Serial.println("Initializing ESP32 CanSat Transmitter...");

  Wire.begin(); // SDA = GPIO 21, SCL = GPIO 22

  // --- MPU-6050 (Attitude Sensor) ---
  if (!mpu.begin()) {
    Serial.println("ERROR: MPU6050 not found! Check wiring.");
  } else {
    Serial.println("MPU6050 initialized.");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  // --- BMP390 (Pressure / Temperature / Altitude Sensor) ---
  // Try default address 0x77, then alternative 0x76
  bool bmp_status = bmp.begin_I2C(0x77);
  if (!bmp_status) bmp_status = bmp.begin_I2C(0x76);

  if (!bmp_status) {
    Serial.println("ERROR: BMP390 not found! Check wiring.");
  } else {
    Serial.println("BMP390 initialized.");
    bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP3_ODR_50_HZ);
  }

  // --- BMM350 (Magnetometer / Compass) ---
  if (bmm350.begin() != 0) {
    Serial.println("ERROR: BMM350 not found! Check wiring.");
  } else {
    Serial.println("BMM350 initialized.");
    bmm350.setOperationMode(eBmm350NormalMode);
    bmm350.setPresetMode(BMM350_PRESETMODE_HIGHACCURACY, BMM350_DATA_RATE_25HZ);
    bmm350.setMeasurementXYZ();
  }
}

void loop() {

  // --- Read MPU-6050 ---
  sensors_event_t a, g, temp_mpu;
  mpu.getEvent(&a, &g, &temp_mpu);
  roll  = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / PI;
  pitch = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y
              + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;

  // --- Read BMP390 ---
  float temperature = 0, pressure = 0, altitude = 0;
  if (bmp.performReading()) {
    temperature = bmp.temperature;                         // °C
    pressure    = bmp.pressure / 100.0;                   // Pa → hPa
    altitude    = bmp.readAltitude(SEALEVELPRESSURE_HPA); // metres
  }

  // --- Read BMM350 (for compass heading only) ---
  sBmm350MagData_t magData = bmm350.getGeomagneticData();
  float magX = magData.x;
  float magY = magData.y;
  float magZ = magData.z;

  // Tilt-compensated heading using pitch + roll from MPU-6050
  float pitchRad = pitch * PI / 180.0;
  float rollRad  = roll  * PI / 180.0;
  float magX_comp = magX * cos(pitchRad)
                  + magY * sin(rollRad) * sin(pitchRad)
                  + magZ * cos(rollRad) * sin(pitchRad);
  float magY_comp = magY * cos(rollRad)
                  - magZ * sin(rollRad);
  float heading = atan2(-magY_comp, magX_comp) * 180.0 / PI;
  if (heading < 0) heading += 360.0;

  // --- Build and Send JSON ---
  StaticJsonDocument<256> doc;
  doc["temp"]    = temperature;
  doc["press"]   = pressure;
  doc["alt"]     = altitude;
  doc["roll"]    = roll;
  doc["pitch"]   = pitch;
  doc["heading"] = heading;

  String output;
  serializeJson(doc, output);

  Serial2.println(output);        // Send to RF module
  Serial.print("Sending: ");
  Serial.println(output);

  delay(400);
}
