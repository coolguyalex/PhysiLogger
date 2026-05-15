#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>

#define MPU_ADDR  0x68
#define SD_CS_PIN 23

SdFat sd;
SdSpiConfig sdConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16), &SPI1);
SdFile logFile;

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Starting...");

  // ── MPU-6050 ────────────────────────────────────────
  Wire.begin();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  uint8_t mpuResult = Wire.endTransmission();
  Serial.print("MPU wake result = ");
  Serial.println(mpuResult);  // 0 = good

  // ── SD card ─────────────────────────────────────────
  if (!sd.begin(sdConfig)) {
    Serial.println("SD init failed!");
    return;
  }
  Serial.println("SD OK");

  if (!logFile.open("imu_log.csv", O_WRONLY | O_CREAT | O_APPEND)) {
    Serial.println("File open failed!");
    return;
  }
  Serial.println("File open OK");

  // Write header if file is new
  if (logFile.fileSize() == 0) {
    logFile.println("time_ms,ax,ay,az,gx,gy,gz");
    logFile.sync();
  }

  Serial.println("Setup complete — logging started");
}

void loop() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14);

  int16_t ax   = Wire.read() << 8 | Wire.read();
  int16_t ay   = Wire.read() << 8 | Wire.read();
  int16_t az   = Wire.read() << 8 | Wire.read();
  int16_t temp = Wire.read() << 8 | Wire.read();
  int16_t gx   = Wire.read() << 8 | Wire.read();
  int16_t gy   = Wire.read() << 8 | Wire.read();
  int16_t gz   = Wire.read() << 8 | Wire.read();

  char row[96];
  snprintf(row, sizeof(row), "%lu,%d,%d,%d,%d,%d,%d",
           millis(), ax, ay, az, gx, gy, gz);

  // Write to SD
  logFile.println(row);
  logFile.sync();

  // Mirror to serial
  Serial.println(row);

  delay(100);  // 10 Hz — safe starting rate
}
