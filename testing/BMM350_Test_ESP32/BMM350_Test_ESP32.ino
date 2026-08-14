#include <Wire.h>
#include "DFRobot_BMM350.h"

// The primary I2C address is 0x14.
// If this fails, change the below line to 0x15.
#define BMM350_I2C_ADDR 0x14

// Create the BMM350 object using the ESP32's Hardware I2C interface (Wire)
DFRobot_BMM350_I2C bmm(&Wire, BMM350_I2C_ADDR);

void setup() {
  // ESP32 standard serial baud rate
  Serial.begin(115200);
  
  // Small delay to allow the serial monitor to catch up
  delay(1000);
  Serial.println("DFRobot BMM350 Magnetometer Test for ESP32 Started!");

  // Initialize the sensor
  // begin() returns 0 on success, non-zero on failure
  if (bmm.begin() != 0) {
    Serial.println("CRITICAL ERROR: Failed to find BMM350 sensor at address 0x14!");
    Serial.println("1. Check wiring: VCC to 3.3V, GND to GND, SDA to GPIO 21, SCL to GPIO 22.");
    Serial.println("2. Try changing 'BMM350_I2C_ADDR' at the top of this file from 0x14 to 0x15.");
    while (1) {
      delay(10); // Halt execution
    }
  }

  Serial.println("BMM350 Magnetometer Initialized successfully!");
  
  // WAKE UP SENSOR: The BMM350 defaults to "Suspend Mode" (sleep) on power up. 
  // We MUST set it to Normal Mode to get real magnetic data!
  bmm.setOperationMode(eBmm350NormalMode);
  bmm.setPresetMode(BMM350_PRESETMODE_HIGHACCURACY, BMM350_DATA_RATE_25HZ);
  bmm.setMeasurementXYZ(); // Enable all 3 axes

  Serial.println("-------------------------------------");
}

void loop() {
  // Read the geomagnetic data from the sensor
  sBmm350MagData_t magData = bmm.getGeomagneticData();
  
  // Print the X, Y, and Z axes in microTeslas (uT)
  Serial.print("Mag X: ");
  Serial.print(magData.x);
  Serial.print(" uT,  ");
  
  Serial.print("Mag Y: ");
  Serial.print(magData.y);
  Serial.print(" uT,  ");
  
  Serial.print("Mag Z: ");
  Serial.print(magData.z);
  Serial.println(" uT");

  Serial.println("-------------------------------------");
  
  // Wait 1 second before the next reading
  delay(1000);
}
