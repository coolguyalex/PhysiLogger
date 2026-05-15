# PhysiLogger

**A wearable IMU data logger built on the Adafruit Adalogger RP2040.**

PhysiLogger captures 6-axis inertial measurement data (accelerometer + gyroscope) from an MPU-6050 sensor, writes it to a CSV file on a microSD card, and displays real-time acceleration statistics on a 0.96" OLED screen. Logging sessions are started and stopped with a single tactile push button held for 2 seconds.

Developed by Alexander Sousa.

---

## Table of Contents

1. [Hardware](#hardware)
2. [Wiring](#wiring)
3. [Software & Libraries](#software--libraries)
4. [Program Design](#program-design)
5. [OLED Display Layout](#oled-display-layout)
6. [CSV Output Format](#csv-output-format)
7. [Building From Scratch — Step by Step](#building-from-scratch--step-by-step)
8. [Known Roadblocks & Fixes](#known-roadblocks--fixes)
9. [Troubleshooting Reference](#troubleshooting-reference)

---

## Hardware

### Adafruit Feather RP2040 Adalogger
- **Product page:** https://www.adafruit.com/product/5980
- **MCU:** Raspberry Pi RP2040 — dual ARM Cortex-M0+ running at up to 133 MHz
- **RAM:** 264 kB on-chip SRAM
- **Flash:** External QSPI chip
- **Built-in SD card slot** — communicates over SPI1 (dedicated hardware SPI peripheral)
- **Built-in red LED** on GPIO13 (`LED_BUILTIN`)
- **Built-in BOOT button** on GPIO7
- **All GPIO is 3.3V logic**
- **Key pins used in this project:**

| Pin Label | GPIO | Function in this project |
|---|---|---|
| SDA | GPIO2 | I2C data (MPU-6050 + OLED) |
| SCL | GPIO3 | I2C clock (MPU-6050 + OLED) |
| SD_CS | GPIO23 | SD card chip select (SPI1) |
| SD_CLK | GPIO18 | SD card clock (SPI1) |
| SD_MOSI | GPIO19 | SD card data out (SPI1) |
| SD_MISO | GPIO20 | SD card data in (SPI1) |
| D10 | GPIO10 | Tactile button input |
| LED | GPIO13 | Status LED (built-in) |

> **Note:** The SD card pins are connected internally on the Adalogger PCB. You do not need to wire them.

---

### MPU-6050 Breakout Board (GY-521 or equivalent)
- **IC:** InvenSense MPU-6050
- **Interface:** I2C
- **Default I2C address:** `0x68` (AD0 pin low or floating). If AD0 is pulled high, address becomes `0x69`.
- **Supply voltage:** 3.3V
- **Axes:** 3-axis accelerometer + 3-axis gyroscope
- **Default accelerometer range:** ±2g (raw full-scale value: 16384 LSB/g)
- **Default gyroscope range:** ±250°/s
- **Breakout board notes:** Most GY-521 style breakout boards include SMD pull-up resistors on the SDA and SCL lines. Inspect the PCB — if there are resistors with traces leading to SDA and SCL, external pull-ups are not required.

**Pins used:**

| MPU-6050 Pin | Connect to |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | SDA (GPIO2) |
| SCL | SCL (GPIO3) |
| XDA | Not connected |
| XCL | Not connected |
| AD0 | Not connected (or GND for address 0x68) |
| INT | Not connected |

---

### 0.96" OLED Display (SSD1306)
- **Resolution:** 128 × 64 pixels, monochrome
- **Interface:** I2C
- **I2C address:** `0x3C` (most common) or `0x3D` (some modules)
- **Supply voltage:** 3.3V
- **Important physical characteristic:** These displays have a two-tone pixel layout baked into the hardware — the **top 16 rows of pixels are yellow**, and the **bottom 48 rows are blue**. This is fixed and cannot be changed in software. The UI in this project is deliberately designed around this boundary.

**Pins used:**

| OLED Pin | Connect to |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | SDA (GPIO2) |
| SCL | SCL (GPIO3) |

> Both the MPU-6050 and OLED share the same I2C bus (SDA/SCL). This is normal — I2C supports multiple devices on the same two wires, each addressed independently.

---

### Tactile Push Button
- Any standard normally-open momentary tactile switch
- **Wiring:** One leg to GPIO10, other leg to GND
- No external resistor needed — the RP2040's internal pull-up resistor is enabled in software (`INPUT_PULLUP`)
- When not pressed: GPIO10 reads HIGH. When pressed: GPIO10 reads LOW.

---

## Wiring

```
Adalogger RP2040          MPU-6050
─────────────────         ────────
3.3V ──────────────────── VCC
GND  ──────────────────── GND
SDA (GPIO2) ──────────── SDA
SCL (GPIO3) ──────────── SCL

Adalogger RP2040          OLED SSD1306
─────────────────         ────────────
3.3V ──────────────────── VCC
GND  ──────────────────── GND
SDA (GPIO2) ──────────── SDA
SCL (GPIO3) ──────────── SCL

Adalogger RP2040          Tactile Button
─────────────────         ──────────────
GPIO10 ────────────────── Leg A
GND    ────────────────── Leg B
```

> The SD card is built into the Adalogger — insert a FAT32-formatted microSD card into the slot on the underside of the board. No additional wiring is required.

---

## Software & Libraries

### Arduino IDE Setup
- **IDE version:** Arduino IDE 2.x
- **Board package:** Earle Philhower's RP2040 Arduino core
  - Install via: **File → Preferences → Additional Boards Manager URLs**
  - Add: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
  - Then: **Tools → Board → Boards Manager** → search `rp2040` → install **Raspberry Pi RP2040 Boards**
- **Board selection:** Tools → Board → Raspberry Pi RP2040 Boards → **Adafruit Feather RP2040**

### Libraries
Install all of the following via **Sketch → Include Library → Manage Libraries**:

| Library | Purpose | Notes |
|---|---|---|
| **SdFat** by Bill Greiman | SD card read/write | Do not use the built-in SD library — it does not support the SPI1 bus configuration required by the Adalogger |
| **Adafruit SSD1306** | OLED display driver | |
| **Adafruit GFX Library** | Graphics primitives used by SSD1306 | Installed as a dependency of SSD1306 |
| **Wire** (built-in) | I2C communication | Part of Arduino core, no installation needed |
| **SPI** (built-in) | SPI communication | Part of Arduino core, no installation needed |

> **Duplicate library warning:** When compiling you may see a message saying two copies of SdFat were found — one in your personal libraries folder and one bundled with the RP2040 board package. Arduino automatically uses the one in your personal libraries folder, which is correct. This warning is harmless.

---

## Program Design

### Architecture Overview

The sketch is a single-file Arduino program structured around a non-blocking main loop running at approximately 100 Hz (10 ms per iteration). All time-sensitive operations — button detection, LED blinking, IMU reading, SD writing, and display updating — are handled cooperatively within that loop without any blocking calls.

### Initialization (`setup`)

On boot the program:
1. Waits 3 seconds with a plain `delay(3000)` to allow the USB serial connection to stabilise
2. Configures the button pin with an internal pull-up resistor
3. Initialises the I2C bus with `Wire.begin()`
4. Initialises the OLED display
5. Wakes the MPU-6050 by writing `0x00` to its PWR_MGMT_1 register (`0x6B`), clearing the sleep bit
6. Initialises the SD card using SdFat with the SPI1 bus at 16 MHz
7. Sets status flags (`mpuReady`, `sdReady`) based on whether each peripheral initialised successfully
8. Draws the idle screen

If a peripheral fails to initialise, the sketch reports the error over serial and continues running. It does not halt. This keeps the USB connection alive so error messages remain readable.

### Button Logic (non-blocking)

The button is checked every loop iteration using three state variables:

- `buttonWasPressed` — was the button down on the previous loop iteration?
- `buttonPressStart` — the `millis()` timestamp of when the button was first pressed
- `holdFired` — has the 2-second action already been triggered for this press?

On each loop iteration:
- If the button just went down (transition from HIGH to LOW), `buttonPressStart` is recorded
- If the button has been held continuously for 2 seconds (`HOLD_MS`) and `holdFired` is false, the toggle action fires once and `holdFired` is set to prevent retriggering
- If the button is released before 2 seconds, nothing happens

This approach ensures IMU data continues to be read and logged without any interruption for the entire duration of a button press, right up until the 2-second threshold.

> **Why not use a blocking while loop for the button?** An earlier implementation used a blocking `while (buttonDown) { ... }` approach. This paused all logging during the press, meaning data collected while the user was holding the button was lost. The non-blocking approach above solves this entirely.

### MPU-6050 Raw Register Access

The MPU-6050 is read directly via I2C register access without any third-party IMU library. This was a deliberate decision after library-based approaches produced unreliable results on this hardware combination.

The MPU-6050 stores its output data in 16 consecutive registers starting at `0x3B`:

| Register | Data |
|---|---|
| 0x3B–0x3C | Accelerometer X (high byte, low byte) |
| 0x3D–0x3E | Accelerometer Y |
| 0x3F–0x40 | Accelerometer Z |
| 0x41–0x42 | Temperature |
| 0x43–0x44 | Gyroscope X |
| 0x45–0x46 | Gyroscope Y |
| 0x47–0x48 | Gyroscope Z |

Because the MPU-6050 auto-increments its internal register pointer, all 14 bytes (accel + temp + gyro) are read in a single I2C burst transaction, which is efficient and ensures all values are from the same sample instant.

Each 16-bit value is reconstructed by shifting the high byte left by 8 bits and ORing with the low byte:
```cpp
int16_t ax = Wire.read() << 8 | Wire.read();
```

### RSS Calculation

Root Sum Squared (RSS) acceleration is calculated each loop iteration as:

```
RSS = sqrt(ax² + ay² + az²) / 16384.0
```

Dividing by 16384 converts from raw LSB units to g-force (based on the default ±2g full-scale range where 1g = 16384 LSB).

Three RSS metrics are tracked:

- **Instantaneous RSS (`rssNow`):** Calculated fresh every loop iteration
- **Moving average (`rssMovAvg`):** Average of the last 20 RSS values, implemented as a circular buffer (`rssHistory[]`). Smooths out noise and spike artifacts.
- **Session maximum (`rssMax`):** The highest RSS value seen since the current logging session started. Reset to zero at the start of each new session.

### SD Card Logging

The SD card is accessed using the **SdFat** library configured for the Adalogger's dedicated SPI1 hardware peripheral:

```cpp
SdSpiConfig sdConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16), &SPI1);
```

Each logging session creates a new file named `log_<millis>.csv` where `<millis>` is the board's millisecond uptime counter at the moment logging starts. This guarantees unique filenames and prevents sessions from overwriting each other.

`logFile.sync()` is called after every row to flush the write buffer to the card. This is slightly slower than batching writes but protects against data loss if power is interrupted.

### LED Status Indicator

The built-in red LED on GPIO13 blinks at 500 ms intervals while a logging session is active. It is switched off immediately when logging stops. Blinking is implemented non-blocking using a `millis()` timestamp comparison — the LED state is simply toggled each time 500 ms has elapsed since the last toggle.

---

## OLED Display Layout

The display has a fixed hardware colour split: **top 16 pixels = yellow, pixels 16–63 = blue**. The UI is designed around this boundary.

### Idle Screen
```
┌─────────────────────────┐
│     PHYSILOGGER         │  ← yellow zone (y 0–15)
├─────────────────────────┤
│  by Alexander Sousa     │  ← blue zone (y 16–63)
├─────────────────────────┤
│    Hold to begin        │
│       logging...        │
└─────────────────────────┘
```

### Recording Screen
```
┌─────────────────────────┐
│    * Recording....      │  ← yellow zone (y 0–15)
├─────────────────────────┤
│ RSS: 1.023 g            │  ← blue zone (y 16–63)
│ AVG: 1.001 g            │
│ MAX: 1.187 g            │
├─────────────────────────┤
│     Hold to stop        │
└─────────────────────────┘
```

---

## CSV Output Format

Each logging session produces one file on the SD card named `log_<timestamp>.csv`.

**Header row:**
```
time_ms,ax,ay,az,gx,gy,gz,rss_g
```

**Column descriptions:**

| Column | Type | Description |
|---|---|---|
| `time_ms` | unsigned long | Board uptime in milliseconds at time of sample |
| `ax` | int16 | Raw accelerometer X (LSB, ±2g range, 16384 LSB/g) |
| `ay` | int16 | Raw accelerometer Y |
| `az` | int16 | Raw accelerometer Z |
| `gx` | int16 | Raw gyroscope X (LSB, ±250°/s range) |
| `gy` | int16 | Raw gyroscope Y |
| `gz` | int16 | Raw gyroscope Z |
| `rss_g` | float (4dp) | Instantaneous RSS acceleration in g |

**Converting raw accelerometer to g:**
```
acceleration_g = raw_value / 16384.0
```

**Converting raw gyroscope to °/s:**
```
angular_rate_dps = raw_value / 131.0
```

**Converting raw temperature to °C:**
```
temperature_c = raw_value / 340.0 + 36.53
```

---

## Building From Scratch — Step by Step

### 1. Install Arduino IDE
Download Arduino IDE 2.x from https://www.arduino.cc/en/software

### 2. Install the RP2040 Board Package
- Open Arduino IDE
- Go to **File → Preferences**
- In the *Additional boards manager URLs* field, paste:
  `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
- Go to **Tools → Board → Boards Manager**
- Search for `rp2040` and install **Raspberry Pi RP2040 Boards** by Earle F. Philhower III
- Select **Tools → Board → Raspberry Pi RP2040 Boards → Adafruit Feather RP2040**

### 3. Install Libraries
Go to **Sketch → Include Library → Manage Libraries** and install:
- `SdFat` by Bill Greiman
- `Adafruit SSD1306`
- `Adafruit GFX Library`

### 4. Prepare the SD Card
Format a microSD card as **FAT32**. Cards up to 32 GB work reliably. Insert into the slot on the underside of the Adalogger.

### 5. Wire the Hardware
Follow the wiring table in the [Wiring](#wiring) section above.

### 6. Flash the Sketch

**Method A — Direct upload (when it works):**
- Select the correct COM port under **Tools → Port**
- Click Upload

**Method B — UF2 drag and drop (most reliable):**
- Go to **Sketch → Export Compiled Binary** — this saves a `.uf2` file next to your `.ino` file inside the `build/` subfolder
- Hold the BOOT button on the board, press and release RESET, then release BOOT — a drive called `RPI-RP2` appears on your PC
- Drag the `.uf2` file onto the `RPI-RP2` drive
- The drive disappears automatically and the board reboots into your sketch

### 7. Verify Operation
- Open the Serial Monitor at 115200 baud
- After 3 seconds you should see startup messages confirming MPU-6050 OK, SD OK, and OLED OK
- The OLED idle screen should appear
- Hold the button for 2 seconds — logging begins, LED starts blinking, OLED switches to recording screen
- Hold again for 2 seconds — logging stops, LED off, idle screen returns
- Remove SD card and open the CSV on a PC to verify data

---

## Known Roadblocks & Fixes

### 1. `while(!Serial)` causes the board to become unresponsive

**Problem:** Using `while (!Serial) {}` in `setup()` to wait for the Serial Monitor blocks the CPU completely. On the RP2040, the USB stack runs on the same core and requires regular CPU time to service itself. Blocking the CPU starves the USB stack, causing the COM port to disappear from the IDE and the board to become unreachable without a manual BOOTSEL reset.

**Fix:** Replace `while(!Serial)` with a plain `delay(3000)`. This yields the CPU properly and keeps USB alive.

```cpp
// WRONG — starves USB on RP2040
while (!Serial) {}

// CORRECT
delay(3000);
```

---

### 2. `while(true){}` on error also kills USB

**Problem:** Using `while (true) {}` as an error halt (e.g. "SD init failed, halt forever") has the same effect — the CPU blocks, USB dies, the board becomes unresponsive, and you cannot read the error message that was printed just before the halt.

**Fix:** Replace all `while(true){}` halts with status flags. Let the sketch continue running so USB stays alive and error messages remain readable in the Serial Monitor.

```cpp
// WRONG
if (!sd.begin(sdConfig)) {
  Serial.println("SD failed!");
  while (true) {}
}

// CORRECT
if (!sd.begin(sdConfig)) {
  Serial.println("SD failed!");
  // set a flag and continue
  sdReady = false;
}
```

---

### 3. `Wire1` with manual pin assignment causes I2C lockup

**Problem:** The Earle Philhower RP2040 board package pre-configures the I2C buses based on the board variant selected. Calling `Wire1.setSDA()` / `Wire1.setSCL()` on an already-configured bus, or using the wrong bus object, causes I2C to lock up — often hanging on the very first `Wire1.setSDA()` call before any hardware communication occurs.

**Symptom:** Serial prints "Step 1: Serial OK" and then nothing further — hangs before even touching the MPU-6050.

**Fix:** Use `Wire` (not `Wire1`) without any manual pin assignment calls. The board package automatically maps `Wire` to GPIO2 (SDA) and GPIO3 (SCL) for the Adafruit Feather RP2040 variant.

```cpp
// WRONG — causes lockup
Wire1.setSDA(2);
Wire1.setSCL(3);
Wire1.begin();

// CORRECT — board package handles pin mapping
Wire.begin();
```

---

### 4. Arduino IDE fails to upload to RP2040 on Windows ("Serial port busy" / "No drive to deploy")

**Problem:** The Arduino IDE uses a 1200-baud serial touch to trigger a reset into bootloader mode before uploading. On the RP2040, which handles USB in software rather than via a dedicated USB chip, this handshake frequently fails on Windows. The COM port may also be held by the IDE's own serial monitor, causing a deadlock.

**Symptoms:**
- `Failed uploading: uploading error: exit status 1`
- `Port monitor error: command 'open' failed: Serial port busy`
- `No drive to deploy`

**Fix — reliable upload method:**
1. Go to **Sketch → Export Compiled Binary** to generate a `.uf2` file
2. Put the board into BOOTSEL mode: hold BOOT, press and release RESET, release BOOT
3. A drive named `RPI-RP2` appears on the PC
4. Drag and drop the `.uf2` file from the `build/` subfolder onto `RPI-RP2`
5. The board reboots automatically

**Fix — install Zadig driver (fixes auto-upload for most users):**
1. Download Zadig from https://zadig.akeo.ie
2. Put the board into BOOTSEL mode
3. Open Zadig → **Options → List All Devices**
4. Select `RP2 Boot` **Interface 1** (not Interface 0 — that is the mass storage interface)
5. Set the driver on the right side to **WinUSB**
6. Click **Install Driver**
7. Unplug and replug the board normally

---

### 5. SdFat — two libraries detected / `SdFile` has no `size()` method

**Problem:** The RP2040 board package ships its own bundled copy of SdFat. When you also have SdFat installed in your personal libraries folder, Arduino reports a duplicate library warning. Additionally, the `SdFile` class uses `fileSize()` not `size()`.

**Fix:** The duplicate warning is harmless — Arduino always uses the personal libraries copy. For the method name, use `fileSize()`:

```cpp
// WRONG
if (logFile.size() == 0) { ... }

// CORRECT
if (logFile.fileSize() == 0) { ... }
```

---

### 6. SD card requires explicit SPI1 configuration

**Problem:** The Adalogger's built-in SD card is wired to the RP2040's SPI1 hardware peripheral, not the default SPI0. Using the standard SD library or default SdFat configuration targets SPI0 and will fail to initialise.

**Fix:** Use `SdSpiConfig` with explicit `&SPI1` and `DEDICATED_SPI`:

```cpp
#define SD_CS_PIN 23
SdSpiConfig sdConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16), &SPI1);
SdFat sd;
sd.begin(sdConfig);
```

---

### 7. OLED text bleeds across yellow/blue colour boundary

**Problem:** The 0.96" two-tone OLED has a fixed hardware colour split at y=16. Any text or graphic element that straddles this boundary will appear partly yellow and partly blue, which looks unintentional and is hard to read.

**Fix:** Design UI layouts with the y=16 boundary explicitly in mind. Keep title/status content entirely within y=0–15 (yellow) and data content entirely within y=16–63 (blue). Draw a dividing line at y=16 to make the boundary feel intentional.

---

### 8. Blocking button detection interrupts data logging

**Problem:** An initial button implementation used a blocking `while (buttonDown) { ... }` loop to measure how long the button was held. This completely paused IMU reading and SD writing for the entire duration of the button press.

**Fix:** Implement non-blocking button detection using `millis()` timestamps tracked across loop iterations, as described in the [Button Logic](#button-logic-non-blocking) section above.

---

## Troubleshooting Reference

| Symptom | Most likely cause | Fix |
|---|---|---|
| Board becomes unresponsive after flashing | `while(!Serial)` or `while(true){}` in sketch | Use `delay(3000)`, remove blocking halts |
| Serial Monitor shows "port busy" | IDE holding COM port, or Zadig driver not installed | Use UF2 drag-and-drop; install Zadig |
| Only "Step 1: Serial OK" prints, then hangs | Wrong I2C bus (`Wire1` instead of `Wire`) | Use `Wire.begin()` with no pin assignment |
| OLED stays blank | Wrong I2C address | Change `OLED_ADDR` from `0x3C` to `0x3D` |
| MPU wake result ≠ 0 | MPU not responding | Check VCC, GND, SDA, SCL wiring |
| SD init failed | Wrong SPI bus, no card inserted, card not FAT32 | Use `SPI1` config; reformat card as FAT32 |
| CSV file has only header, no data | MPU or SD not ready flag preventing loop | Check serial output for specific error |
| RSS reads ~1.0 g at rest, all axes near zero | Correct — 1g is gravity on the Z axis | No action needed |
| RSS reads 0.0 constantly | MPU returning zeros — I2C reading but no ACK | Recheck wiring; verify pull-ups on SDA/SCL |
| Two SdFat libraries warning | Board package includes bundled SdFat | Warning is harmless, ignore it |
| `size()` not a member of `SdFile` | Wrong method name | Use `fileSize()` instead |

---

*PhysiLogger — developed iteratively with hardware debugging, driver troubleshooting, and incremental feature integration.*
