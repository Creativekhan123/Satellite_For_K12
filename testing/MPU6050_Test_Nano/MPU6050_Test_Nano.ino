#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>

Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  
  Serial.println("Arduino Nano MPU6050 Test Started!");

  // Try to initialize!
  if (!mpu.begin()) {
    Serial.println("CRITICAL ERROR: Failed to find MPU6050 chip");
    Serial.println("Check your wiring: VCC to 5V (or 3.3V), GND to GND, SDA to A4, SCL to A5");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Accelerometer range set to: ");
  Serial.println("+-8G");

  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.print("Gyro range set to: ");
  Serial.println("+- 500 deg/s");

  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.print("Filter bandwidth set to: ");
  Serial.println("21 Hz");

  Serial.println("-------------------------------------");
  delay(100);
}

void loop() {
  /* Get new sensor events with the readings */
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  /* Print out the raw acceleration values */
  Serial.print("Accel X: ");
  Serial.print(a.acceleration.x);
  Serial.print(", Y: ");
  Serial.print(a.acceleration.y);
  Serial.print(", Z: ");
  Serial.print(a.acceleration.z);
  Serial.println(" m/s^2");

  /* Print out the raw gyro values */
  Serial.print("Gyro X: ");
  Serial.print(g.gyro.x);
  Serial.print(", Y: ");
  Serial.print(g.gyro.y);
  Serial.print(", Z: ");
  Serial.print(g.gyro.z);
  Serial.println(" rad/s");

  /* Calculate Pitch and Roll */
  float roll = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / PI;
  float pitch = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;

  Serial.print("---> Calculated PITCH: ");
  Serial.print(pitch);
  Serial.print(" deg, ROLL: ");
  Serial.print(roll);
  Serial.println(" deg");
  
  Serial.println("");
  
  // Wait half a second before reading again
  delay(500);
}
