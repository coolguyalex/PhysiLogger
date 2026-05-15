#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── Pins & config ─────────────────────────────────────────────
#define MPU_ADDR      0x68
#define SD_CS_PIN     23
#define BUTTON_PIN    10
#define HOLD_MS       2000
#define LED_PIN       LED_BUILTIN
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C  // try 0x3D if screen stays blank

// ── Objects ───────────────────────────────────────────────────
SdFat sd;
SdSpiConfig sdConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16), &SPI1);
SdFile logFile;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── State ─────────────────────────────────────────────────────
bool logging        = false;
bool sdReady        = false;
bool mpuReady       = false;

// ── Button ────────────────────────────────────────────────────
unsigned long buttonPressStart = 0;
bool buttonWasPressed = false;
bool holdFired        = false;

// ── LED blink ─────────────────────────────────────────────────
unsigned long lastBlinkTime = 0;
bool ledState = false;

// ── RSS tracking ──────────────────────────────────────────────
#define RSS_WINDOW 20
float rssHistory[RSS_WINDOW];
int   rssIndex   = 0;
int   rssFilled  = 0;      // how many slots have real data
float rssMax     = 0.0f;
float rssMovAvg  = 0.0f;
float rssNow     = 0.0f;

// ── IMU values (global so display can read them) ──────────────
int16_t ax, ay, az, gx, gy, gz;

// ─────────────────────────────────────────────────────────────
void resetRssTracking() {
  rssIndex  = 0;
  rssFilled = 0;
  rssMax    = 0.0f;
  rssMovAvg = 0.0f;
  rssNow    = 0.0f;
  memset(rssHistory, 0, sizeof(rssHistory));
}

void updateRss() {
  // Raw RSS — divide by 16384.0 to convert to g (±2g default range)
  rssNow = sqrt((float)ax*ax + (float)ay*ay + (float)az*az) / 16384.0f;

  // Update max
  if (rssNow > rssMax) rssMax = rssNow;

  // Rolling window
  rssHistory[rssIndex] = rssNow;
  rssIndex = (rssIndex + 1) % RSS_WINDOW;
  if (rssFilled < RSS_WINDOW) rssFilled++;

  // Recalculate moving average
  float sum = 0;
  for (int i = 0; i < rssFilled; i++) sum += rssHistory[i];
  rssMovAvg = sum / rssFilled;
}

void drawIdleScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // ── Yellow zone (y 0-15) — title ─────────────────────────
  display.setTextSize(1);
  display.setCursor(16, 4);
  display.println("PHYSILOGGER");

  // ── Blue zone (y 16-63) ───────────────────────────────────
  display.drawLine(0, 16, 127, 16, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(4, 20);
  display.println("by Alexander Sousa");

  display.drawLine(0, 34, 127, 34, SSD1306_WHITE);

  display.setCursor(12, 40);
  display.println("Hold to begin");
  display.setCursor(24, 52);
  display.println("logging...");

  display.display();
}

void drawRecordingScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // ── Yellow zone (y 0-15) — status ────────────────────────
  display.setTextSize(1);
  display.setCursor(20, 4);
  display.println("* Recording....");

  // ── Blue zone (y 16-63) — data ───────────────────────────
  char buf[32];

  snprintf(buf, sizeof(buf), "RSS: %.3f g", rssNow);
  display.setCursor(0, 18);
  display.print(buf);

  snprintf(buf, sizeof(buf), "AVG: %.3f g", rssMovAvg);
  display.setCursor(0, 30);
  display.print(buf);

  snprintf(buf, sizeof(buf), "MAX: %.3f g", rssMax);
  display.setCursor(0, 42);
  display.print(buf);

  // ── Bottom of blue zone — hold to stop ───────────────────
  display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
  display.setCursor(16, 57);
  display.println("Hold to stop");

  display.display();
}

// ─────────────────────────────────────────────────────────────
bool startLogging() {
  resetRssTracking();
  char filename[20];
  snprintf(filename, sizeof(filename), "log_%lu.csv", millis());
  if (!logFile.open(filename, O_WRONLY | O_CREAT | O_APPEND)) {
    Serial.println("ERROR: Could not open log file");
    return false;
  }
  logFile.println("time_ms,ax,ay,az,gx,gy,gz,rss_g");
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

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Starting...");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // ── OLED ────────────────────────────────────────────────
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed!");
  } else {
    Serial.println("OLED OK");
    display.clearDisplay();
    display.display();
  }

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

  // ── SD ──────────────────────────────────────────────────
  if (!sd.begin(sdConfig)) {
    Serial.println("SD init failed!");
  } else {
    Serial.println("SD OK");
    sdReady = true;
  }

  drawIdleScreen();
  Serial.println("Ready — hold button 2s to start/stop logging");
}

// ─────────────────────────────────────────────────────────────
void loop() {
  bool buttonDown = (digitalRead(BUTTON_PIN) == LOW);

  // ── Button logic ─────────────────────────────────────────
  if (buttonDown && !buttonWasPressed) {
    buttonPressStart = millis();
    holdFired = false;
  }

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
      drawIdleScreen();
    }
  }

  buttonWasPressed = buttonDown;

  // ── LED blink ────────────────────────────────────────────
  if (logging && millis() - lastBlinkTime >= 500) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    lastBlinkTime = millis();
  }

  // ── Read MPU ─────────────────────────────────────────────
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14);

  ax   = Wire.read() << 8 | Wire.read();
  ay   = Wire.read() << 8 | Wire.read();
  az   = Wire.read() << 8 | Wire.read();
  int16_t temp = Wire.read() << 8 | Wire.read();
  gx   = Wire.read() << 8 | Wire.read();
  gy   = Wire.read() << 8 | Wire.read();
  gz   = Wire.read() << 8 | Wire.read();

  updateRss();

  // ── Log and display ──────────────────────────────────────
  if (logging) {
    char row[112];
    snprintf(row, sizeof(row), "%lu,%d,%d,%d,%d,%d,%d,%.4f",
             millis(), ax, ay, az, gx, gy, gz, rssNow);
    logFile.println(row);
    logFile.sync();
    Serial.println(row);
    drawRecordingScreen();
  }

  delay(10);  // 100 Hz loop
}