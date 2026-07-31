/*
 * LightGuide Edge - 01_sensor_check
 * COM683 CW2 | Vishnu Vekariya | Ulster University
 *
 * Hardware bring-up. Proves every sensor is alive and readable before any data is
 * collected. Run this first and satisfy gate G0 in docs/01-ROADMAP.md.
 *
 * What it does:
 *   1. Scans the I2C bus and reports which devices answered (identifies the board
 *      revision and confirms the OLED address).
 *   2. Initialises the IMU, the OLED and the analog front end.
 *   3. Streams distance / light / tilt to serial at 10 Hz, and mirrors a summary
 *      to the OLED.
 *
 * BEFORE POWERING UP: read docs/02-HARDWARE.md section 3. The nRF52840 is NOT 5V
 * tolerant. If the HC-SR04 runs from 5V, its ECHO pin needs a divider or the board
 * can be damaged.
 *
 * Wiring (canonical map, docs/02-HARDWARE.md section 2):
 *   LDR divider tap -> A0        HC-SR04 TRIG -> D2
 *   OLED SDA -> A4               HC-SR04 ECHO -> D3 (via divider if 5V supplied)
 *   OLED SCL -> A5               Buzzer (optional) -> D9
 */

// ---------------------------------------------------------------------------
// Board revision. The I2C scan below tells you which one you have:
//   0x6B present -> LSM9DS1  -> Rev1 -> leave this as is
//   0x68 or 0x69 -> BMI270   -> Rev2 -> change to 2 and re-flash
// The two IMU libraries both export an object called IMU, so only one can be
// included at a time.
// ---------------------------------------------------------------------------
#define BOARD_REV 1

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#if BOARD_REV == 1
  #include <Arduino_LSM9DS1.h>
#else
  #include <Arduino_BMI270_BMM150.h>
#endif

// --- pin map ---------------------------------------------------------------
const int PIN_LDR   = A0;
const int PIN_TRIG  = 2;
const int PIN_ECHO  = 3;
const int PIN_BUZZ  = 9;

// --- OLED ------------------------------------------------------------------
const int SCREEN_W = 128;
const int SCREEN_H = 64;
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
uint8_t oledAddr = 0x00;          // discovered by the I2C scan
bool    oledOk   = false;

// --- sampling --------------------------------------------------------------
const unsigned long SAMPLE_PERIOD_MS = 100;   // 10 Hz, see docs/03-DATA-PROTOCOL.md
const int   ULTRA_SAMPLES  = 5;               // median-of-5 rejects stray echoes
const int   LDR_SAMPLES    = 8;               // mean-of-8 smooths ADC noise
const int   PING_GAP_MS    = 10;              // let the previous echo die out
const float DIST_MIN_MM    = 20.0f;
const float DIST_MAX_MM    = 4000.0f;

// Echo timing bounds. NOT using pulseIn(): the mbed core's implementation does not
// honour its timeout argument reliably, and a missing echo stalled the loop for
// hundreds of ms per ping - which silently destroyed the 10 Hz sample rate. The
// manual version below is bounded at ~15 ms per failed ping.
const unsigned long ECHO_START_TIMEOUT_US = 15000UL;  // no echo began -> ~2.5 m limit
const unsigned long ECHO_HIGH_MAX_US      = 25000UL;  // echo stuck high -> bad reading

unsigned long lastSample = 0;
unsigned long dropouts   = 0;
unsigned long samples    = 0;

// ---------------------------------------------------------------------------
// I2C scan. Prints every address that acknowledges, with a guess at what it is.
//
// NOTE: the textbook scanner - beginTransmission(addr) then endTransmission() -
// does NOT work on the mbed core. mbed's Wire only puts bytes on the bus when
// there is a payload, so a zero-length write returns success for every address
// and the scan reports nothing useful. Writing one dummy byte forces a real
// transaction, and requestFrom() is used as a second opinion for read-only parts.
// ---------------------------------------------------------------------------
bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)0x00);              // a payload, so the bus is actually driven
  if (Wire.endTransmission() == 0) return true;
  return Wire.requestFrom((int)addr, 1) > 0;
}

void scanI2C() {
  Serial.println(F("--- I2C scan ---"));
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    if (i2cPresent(addr)) {
      found++;
      Serial.print(F("  0x"));
      if (addr < 16) Serial.print('0');
      Serial.print(addr, HEX);
      Serial.print(F("  "));
      switch (addr) {
        case 0x3C:
        case 0x3D: Serial.print(F("SSD1306 OLED")); oledAddr = addr; break;
        case 0x6B: Serial.print(F("LSM9DS1 accel/gyro  -> board is REV1")); break;
        case 0x1E: Serial.print(F("LSM9DS1 magnetometer")); break;
        case 0x68:
        case 0x69: Serial.print(F("BMI270 IMU          -> board is REV2")); break;
        case 0x10: Serial.print(F("BMM150 magnetometer -> board is REV2")); break;
        case 0x39: Serial.print(F("APDS9960 light/proximity (on-board)")); break;
        case 0x5F: Serial.print(F("HTS221 temp/humidity (on-board)")); break;
        case 0x5C: Serial.print(F("LPS22HB pressure (on-board)")); break;
        default:   Serial.print(F("unknown")); break;
      }
      Serial.println();
    }
  }
  if (found == 0) {
    Serial.println(F("  nothing answered."));
    Serial.println(F("  Do NOT trust this on its own: the mbed core's Wire returns"));
    Serial.println(F("  unreliable status for probe transactions, and the on-board"));
    Serial.println(F("  sensors sit on a separate internal bus, not on A4/A5. The"));
    Serial.println(F("  authoritative checks are the IMU and OLED init lines below -"));
    Serial.println(F("  if those report OK, the bus is fine regardless of this scan."));
  }
  Serial.println(F("----------------"));
}

// ---------------------------------------------------------------------------
// One ultrasonic ping. Returns distance in mm, or -1 on timeout / out of range.
// ---------------------------------------------------------------------------
float pingOnce() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(4);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // Wait for the echo pulse to begin.
  unsigned long mark = micros();
  while (digitalRead(PIN_ECHO) == LOW) {
    if (micros() - mark > ECHO_START_TIMEOUT_US) return -1.0f;   // nothing came back
  }

  // Time how long it stays high.
  unsigned long rise = micros();
  while (digitalRead(PIN_ECHO) == HIGH) {
    if (micros() - rise > ECHO_HIGH_MAX_US) return -1.0f;        // stuck high
  }
  unsigned long us = micros() - rise;

  // 343 m/s at 20 C, out and back: mm = us * 0.343 / 2
  float mm = us * 0.1715f;
  if (mm < DIST_MIN_MM || mm > DIST_MAX_MM) return -1.0f;
  return mm;
}

// Median of ULTRA_SAMPLES pings. Median, not mean, because ultrasonic errors are
// wild outliers rather than gaussian noise - one bad echo would drag a mean badly.
float readDistanceMm() {
  float v[ULTRA_SAMPLES];
  int n = 0;
  for (int i = 0; i < ULTRA_SAMPLES; i++) {
    float d = pingOnce();
    if (d > 0) v[n++] = d;
    delay(PING_GAP_MS);
  }
  if (n == 0) return -1.0f;

  for (int i = 1; i < n; i++) {               // insertion sort, n <= 5
    float k = v[i];
    int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[n / 2];
}

int readLdrRaw() {
  long acc = 0;
  for (int i = 0; i < LDR_SAMPLES; i++) acc += analogRead(PIN_LDR);
  return (int)(acc / LDR_SAMPLES);
}

// ---------------------------------------------------------------------------
// ECHO health check. A healthy HC-SR04 holds ECHO LOW when idle and pulses it
// HIGH only in response to a trigger. If it reads HIGH before we have triggered
// anything, the pin is either floating (not wired to ECHO) or being held high by
// something else - and every subsequent ping will burn the full timeout, quietly
// destroying the sample rate. Catch it here and say so, loudly.
// ---------------------------------------------------------------------------
void checkEchoLine() {
  Serial.println(F("--- ECHO line check ---"));
  int highCount = 0;
  for (int i = 0; i < 20; i++) { if (digitalRead(PIN_ECHO) == HIGH) highCount++; delay(5); }

  if (highCount == 0) {
    Serial.println(F("[ OK ] ECHO idles LOW - wiring looks correct."));
  } else if (highCount >= 18) {
    Serial.println(F("[FAIL] ECHO is stuck HIGH before any trigger was sent."));
    Serial.println(F("       Distance readings will all fail and each ping will"));
    Serial.println(F("       burn the full timeout. Most likely causes, in order:"));
    Serial.println(F("        1. D3 is not actually wired to the module's ECHO pin"));
    Serial.println(F("           (a floating input reads high) - check the pin map"));
    Serial.println(F("           in docs/02-HARDWARE.md section 2."));
    Serial.println(F("        2. The HC-SR04 has no power - check VCC and GND."));
    Serial.println(F("        3. ECHO is at 5V and the pin's protection diode is"));
    Serial.println(F("           clamping it. POWER DOWN AND MEASURE - the nRF52840"));
    Serial.println(F("           is not 5V tolerant. See docs/02-HARDWARE.md sec 3."));
  } else {
    Serial.print(F("[WARN] ECHO is unstable ("));
    Serial.print(highCount);
    Serial.println(F("/20 reads high) - suspect a loose jumper."));
  }
  Serial.println(F("-----------------------"));
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) { }      // wait for USB, but don't hang forever

  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F(" LightGuide Edge - sensor check"));
  Serial.print  (F(" Built for board REV")); Serial.println(BOARD_REV);
  Serial.println(F("========================================"));

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  digitalWrite(PIN_TRIG, LOW);

  analogReadResolution(12);                   // 0-4095 instead of the default 0-1023

  Wire.begin();
  scanI2C();
  checkEchoLine();

  // --- IMU ---
  if (!IMU.begin()) {
    Serial.println(F("[FAIL] IMU did not start."));
    Serial.println(F("       If the scan showed 0x68/0x69, set BOARD_REV to 2 and re-flash."));
  } else {
    Serial.print(F("[ OK ] IMU running, accel sample rate "));
    Serial.print(IMU.accelerationSampleRate());
    Serial.println(F(" Hz"));
  }

  // --- OLED ---
  if (oledAddr == 0x00) oledAddr = 0x3C;      // scan found nothing, try the common one
  if (display.begin(SSD1306_SWITCHCAPVCC, oledAddr)) {
    oledOk = true;
    Serial.print(F("[ OK ] OLED at 0x")); Serial.println(oledAddr, HEX);
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("LightGuide Edge"));
    display.println(F("sensor check"));
    display.display();
  } else {
    Serial.println(F("[FAIL] OLED did not start - check A4/A5 and the address above."));
  }

  Serial.println();
  Serial.println(F("Streaming at 10 Hz. Sanity checks to run now:"));
  Serial.println(F("  - hold a wall at 50 cm  -> dist_mm should read about 500"));
  Serial.println(F("  - cover the LDR         -> ldr_raw should drop sharply"));
  Serial.println(F("  - tilt the board        -> pitch/roll should track it"));
  Serial.println();
  Serial.println(F("t_ms,dist_mm,ldr_raw,ax,ay,az,pitch,roll,dropout_pct"));
}

// ---------------------------------------------------------------------------
void loop() {
  // Send any character to re-run the startup diagnostics. Useful because the
  // boot-time output scrolls past before a serial monitor has finished attaching.
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    Serial.println();
    scanI2C();
    checkEchoLine();
    Serial.println(F("t_ms,dist_mm,ldr_raw,ax,ay,az,pitch,roll,dropout_pct"));
  }

  unsigned long now = millis();
  if (now - lastSample < SAMPLE_PERIOD_MS) return;
  lastSample = now;

  float dist = readDistanceMm();
  int   ldr  = readLdrRaw();

  float ax = 0, ay = 0, az = 0;
  if (IMU.accelerationAvailable()) IMU.readAcceleration(ax, ay, az);

  // Tilt from gravity. atan2 keeps this well behaved through the full range,
  // unlike asin which loses resolution near the extremes.
  float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI;
  float roll  = atan2(ay, az) * 180.0f / PI;

  samples++;
  if (dist < 0) dropouts++;
  float dropPct = (100.0f * dropouts) / samples;

  Serial.print(now);          Serial.print(',');
  Serial.print(dist, 1);      Serial.print(',');
  Serial.print(ldr);          Serial.print(',');
  Serial.print(ax, 3);        Serial.print(',');
  Serial.print(ay, 3);        Serial.print(',');
  Serial.print(az, 3);        Serial.print(',');
  Serial.print(pitch, 1);     Serial.print(',');
  Serial.print(roll, 1);      Serial.print(',');
  Serial.println(dropPct, 1);

  // Refresh the OLED once a second - the I2C write is slow and would otherwise
  // eat into the sample period.
  static unsigned long lastDraw = 0;
  if (oledOk && now - lastDraw > 1000) {
    lastDraw = now;
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println(F("SENSOR CHECK"));
    display.print(F("dist "));
    if (dist < 0) display.println(F("--- mm"));
    else        { display.print(dist, 0); display.println(F(" mm")); }
    display.print(F("ldr  ")); display.println(ldr);
    display.print(F("pit  ")); display.println(pitch, 0);
    display.print(F("rol  ")); display.println(roll, 0);
    display.print(F("drop ")); display.print(dropPct, 0); display.println('%');
    display.display();
  }
}
