#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>

#define MPU_ADDR    0x68
#define SD_CS_PIN   23
#define BUTTON_PIN  10
#define HOLD_MS     2000  // hold duration to start/stop logging

SdFat sd;
SdSpiConfig sdConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16), &SPI1);
SdFile logFile;

bool logging    = false;  // are we currently logging?
bool sdReady    = false;
bool mpuReady   = false;

// ── Button: returns true if button held for HOLD_MS ──────────
bool buttonHeld() {
  if (digitalRead(BUTTON_PIN) != LOW) return false;  // not even pressed
  unsigned long pressStart = millis();
  while (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - pressStart >= HOLD_MS) return true;
    delay(10);
  }
  return false;  // released before HOLD_MS
}

// ── Open a new log file ───────────────────────────────────────
bool startLogging() {
  // Use a timestamp-style name based on millis to avoid overwriting
  char filename[20];
  snprintf(filename, sizeof(filename), "log_%lu.csv", millis());

  if (!logFile.open(filename, O_WRONLY | O_CREAT | O_APPEND)) {
    Serial.println("ERROR: Could not open log file");
    return false;
  }
  logFile.println("time_ms,ax,ay,az,gx,gy,gz");
  logFile.sync();
  Serial.print("Logging started → ");
  Serial.println(filename);
  return true;
}

// ── Close the log file ────────────────────────────────────────
void stopLogging() {
  logFile.sync();
  logFile.close();
  Serial.println("Logging stopped");
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Starting...");

  // ── Button ───────────────────────────────────────────────
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Button ready");

  // ── MPU-6050 ────────────────────────────────────────────
  Wire.begin();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  uint8_t mpuResult = Wire.endTransmission();
  if (mpuResult == 0) {
    Serial.println("MPU-6050 OK");
    mpuReady = true;
  } else {
    Serial.print("MPU error: ");
    Serial.println(mpuResult);
  }

  // ── SD card ─────────────────────────────────────────────
  if (!sd.begin(sdConfig)) {
    Serial.println("SD init failed!");
  } else {
    Serial.println("SD OK");
    sdReady = true;
  }

  Serial.println("Hold button 2 seconds to start/stop logging");
}

void loop() {
  // ── Check for button hold ─────────────────────────────
  if (buttonHeld()) {
    if (!logging) {
      // Start a new session
      if (sdReady && mpuReady) {
        logging = startLogging();
      } else {
        Serial.println("Cannot log — check MPU and SD");
      }
    } else {
      // Stop current session
      stopLogging();
      logging = false;
    }
    // Wait for button release before continuing
    while (digitalRead(BUTTON_PIN) == LOW) delay(10);
  }

  // ── Log data if session is active ─────────────────────
  if (logging) {
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

    logFile.println(row);
    logFile.sync();
    Serial.println(row);

    delay(100);  // 10 Hz
  }
}