#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BMP280.h>
#include "DFRobot_BMM350.h"
#include <ArduinoJson.h>

Adafruit_MPU6050 mpu;
Adafruit_BMP280 bmp;                        // BMP280 Barometer (Replaces BMP390)
DFRobot_BMM350_I2C bmm350(&Wire, 0x14);

// Standard sea-level pressure for altitude calculation (adjust for your location)
#define SEALEVELPRESSURE_HPA (1013.25)
bool bmp_status = false;

// RF Module — GPIO 33 (RX) and 32 (TX) are safe on all ESP32 variants
#define RXD2 32
#define TXD2 33

float roll  = 0;
float pitch = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  delay(500); // Short settle delay — reduced from 1000ms
  Serial.println("Initializing ESP32 CanSat Transmitter (BMP280)...");

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

  // --- BMP280 (Pressure / Temperature / Altitude Sensor) ---
  // Try default address 0x76 (most common for BMP280 modules), then fallback to 0x77
  bmp_status = bmp.begin(0x76);
  if (!bmp_status) bmp_status = bmp.begin(0x77);

  if (!bmp_status) {
    Serial.println("ERROR: BMP280 not found! Check wiring / I2C address (0x76/0x77).");
  } else {
    Serial.println("BMP280 initialized.");
    /* Recommended flight/weather sensor settings */
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_250); /* 250ms standby — faster than TX interval (400ms) */
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

// Non-blocking transmit timer — avoids blocking delay() in loop
unsigned long lastTxTime = 0;
const unsigned long TX_INTERVAL_MS = 400; // 2.5 Hz transmit rate

void loop() {

  // --- Non-blocking TX gate: only transmit every 400ms ---
  unsigned long now = millis();
  if (now - lastTxTime < TX_INTERVAL_MS) return;
  lastTxTime = now;

  // --- Read MPU-6050 ---
  sensors_event_t a, g, temp_mpu;
  mpu.getEvent(&a, &g, &temp_mpu);
  roll  = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / PI;
  pitch = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y
              + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;

  // --- Read BMP280 ---
  float temperature = 0, pressure = 0, altitude = 0;
  if (bmp_status) {
    temperature = bmp.readTemperature();                         // °C
    pressure    = bmp.readPressure() / 100.0F;                   // Pa → hPa
    altitude    = bmp.readAltitude(SEALEVELPRESSURE_HPA);         // metres
    if (isnan(temperature)) temperature = 0;
    if (isnan(pressure)) pressure = 0;
    if (isnan(altitude)) altitude = 0;
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
  Serial.print("TX @ ");
  Serial.print(TX_INTERVAL_MS);
  Serial.print("ms | ");
  Serial.println(output);
  // No delay() — millis() gate at top of loop handles timing
}
