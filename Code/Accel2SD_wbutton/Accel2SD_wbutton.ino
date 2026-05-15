#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>

#define MPU_ADDR    0x68
#define SD_CS_PIN   23
#define BUTTON_PIN  10
#define HOLD_MS     2000
#define LED_PIN     LED_BUILTIN

SdFat sd;
SdSpiConfig sdConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16), &SPI1);
SdFile logFile;

bool logging  = false;
bool sdReady  = false;
bool mpuReady = false;

unsigned long lastBlinkTime = 0;
bool ledState = false;

// ── Non-blocking button state ─────────────────────────────────
unsigned long buttonPressStart = 0;   // when the button was first pressed
bool buttonWasPressed = false;        // was button down last loop?
bool holdFired = false;               // did we already trigger this hold?

bool startLogging() {
  char filename[20];
  snprintf(filename, sizeof(filename), "log_%lu.csv", millis());
  if (!logFile.open(filename, O_WRONLY | O_CREAT | O_APPEND)) {
    Serial.println("ERROR: Could not open log file");
    return false;
  }
  logFile.println("time_ms,ax,ay,az,gx,gy,gz");
  logFile.sync();
  Serial.print("Logging started -> ");
  Serial.println(filename);
  return true;
}

void stopLogging() {
  logFile.sync();
  logFile.close();
  digitalWrite(LED_PIN, LOW);
  ledState = false;
  Serial.println("Logging stopped");
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Starting...");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

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

  if (!sd.begin(sdConfig)) {
    Serial.println("SD init failed!");
  } else {
    Serial.println("SD OK");
    sdReady = true;
  }

  Serial.println("Hold button 2 seconds to start/stop logging");
}

void loop() {
  bool buttonDown = (digitalRead(BUTTON_PIN) == LOW);

  // ── Button press started ────────────────────────────────────
  if (buttonDown && !buttonWasPressed) {
    buttonPressStart = millis();
    holdFired = false;
  }

  // ── Button held long enough — trigger once ──────────────────
  if (buttonDown && !holdFired &&
      (millis() - buttonPressStart >= HOLD_MS)) {
    holdFired = true;
    if (!logging) {
      if (sdReady && mpuReady) {
        logging = startLogging();
      } else {
        Serial.println("Cannot log — check MPU and SD");
      }
    } else {
      stopLogging();
      logging = false;
    }
  }

  buttonWasPressed = buttonDown;

  // ── Blink LED while logging ─────────────────────────────────
  if (logging) {
    if (millis() - lastBlinkTime >= 500) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      lastBlinkTime = millis();
    }

    // ── Read and log MPU data ─────────────────────────────────
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
  }

  delay(10);  // tight loop — 100Hz when logging
}