/*
 * LightGuide Edge - 01e_acceptance
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * Full-system acceptance test for gate G0 (docs/01-ROADMAP.md). Runs every channel
 * and prints a pass/fail verdict per component, so "the hardware works" is a claim
 * backed by output rather than an impression.
 *
 * PHASE A - automatic. No user action. Checks OLED on the bus, IMU, APDS9960, the
 *           ultrasonic dropout rate over 100 real samples, and whether the LDR sits
 *           in a usable part of its range.
 * PHASE B - interactive, 25 s. Asks for a light change and a button press, and
 *           detects both automatically. Drives the LED and buzzer for the operator
 *           to confirm by eye and ear.
 *
 * Save the serial output as evidence - it is the artefact behind the gate.
 */

#define BOARD_REV 1

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino_APDS9960.h>
#if BOARD_REV == 1
  #include <Arduino_LSM9DS1.h>
#else
  #include <Arduino_BMI270_BMM150.h>
#endif

const int PIN_LDR = A0, PIN_TRIG = 2, PIN_ECHO = 3;
const int PIN_SWITCH = 7, PIN_EXT_LED = 8, PIN_BUZZ = 9;

const unsigned long ECHO_START_TIMEOUT_US = 15000UL;
const unsigned long ECHO_HIGH_MAX_US      = 25000UL;
const float DIST_MIN_MM = 20.0f, DIST_MAX_MM = 4000.0f;

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// results
bool rOled = false, rImu = false, rApds = false, rUltra = false;
bool rLdrRange = false, rLdrResponds = false, rSwitch = false;
float ultraDropPct = 0;
int   ldrMin = 4095, ldrMax = 0, ldrRest = 0;
int   distSamples = 0;
float distMin = 99999, distMax = 0;

bool i2cPresent(uint8_t a) {
  Wire.beginTransmission(a);
  Wire.write((uint8_t)0);
  if (Wire.endTransmission() == 0) return true;
  return Wire.requestFrom((int)a, 1) > 0;
}

float pingOnce() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(4);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long m = micros();
  while (digitalRead(PIN_ECHO) == LOW)  if (micros() - m > ECHO_START_TIMEOUT_US) return -1;
  unsigned long r = micros();
  while (digitalRead(PIN_ECHO) == HIGH) if (micros() - r > ECHO_HIGH_MAX_US) return -1;
  float mm = (micros() - r) * 0.1715f;
  return (mm < DIST_MIN_MM || mm > DIST_MAX_MM) ? -1 : mm;
}

int readLdr() {
  long a = 0;
  for (int i = 0; i < 8; i++) a += analogRead(PIN_LDR);
  return a / 8;
}

void line() { Serial.println(F("--------------------------------------------------")); }

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 8000) { }

  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_ECHO, INPUT); digitalWrite(PIN_TRIG, LOW);
  pinMode(PIN_EXT_LED, OUTPUT); pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_SWITCH, INPUT_PULLUP);
  digitalWrite(PIN_EXT_LED, LOW); digitalWrite(PIN_BUZZ, LOW);
  pinMode(LEDR, OUTPUT); pinMode(LEDG, OUTPUT); pinMode(LEDB, OUTPUT);
  digitalWrite(LEDR, HIGH); digitalWrite(LEDG, HIGH); digitalWrite(LEDB, HIGH);
  analogReadResolution(12);
  Wire.begin();

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F(" LIGHTGUIDE EDGE - GATE G0 ACCEPTANCE TEST"));
  Serial.println(F(" B00969091 | COM683 CW2"));
  Serial.println(F("=================================================="));
  Serial.println();
  Serial.println(F("PHASE A - automatic checks"));
  line();

  // --- OLED ---
  Serial.print(F("  OLED (A4/A5)      : "));
  if (i2cPresent(0x3C) || i2cPresent(0x3D)) {
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      rOled = true;
      Serial.println(F("PASS - ACKs at 0x3C and driver initialised"));
      display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1); display.setCursor(0, 0);
      display.println(F("GATE G0")); display.println(F("acceptance test"));
      display.println(F("running..."));
      display.display();
    } else Serial.println(F("FAIL - ACKs but driver refused"));
  } else Serial.println(F("FAIL - nothing on the external I2C bus"));

  // --- IMU ---
  Serial.print(F("  IMU (LSM9DS1)     : "));
  if (IMU.begin()) {
    float ax, ay, az; delay(50);
    if (IMU.accelerationAvailable()) IMU.readAcceleration(ax, ay, az);
    float mag = sqrt(ax * ax + ay * ay + az * az);
    // At rest the accelerometer must measure 1 g. Anything else means it is not
    // really reading the sensor.
    if (mag > 0.85f && mag < 1.15f) {
      rImu = true;
      Serial.print(F("PASS - ")); Serial.print(IMU.accelerationSampleRate());
      Serial.print(F(" Hz, |a|=")); Serial.print(mag, 3); Serial.println(F(" g"));
    } else {
      Serial.print(F("FAIL - |a|=")); Serial.print(mag, 3);
      Serial.println(F(" g, expected ~1.0"));
    }
  } else Serial.println(F("FAIL - begin() failed"));

  // --- APDS9960 ---
  Serial.print(F("  APDS9960          : "));
  if (APDS.begin()) {
    unsigned long t0 = millis();
    while (!APDS.colorAvailable() && millis() - t0 < 2000) delay(10);
    if (APDS.colorAvailable()) {
      int r, g, b, a; APDS.readColor(r, g, b, a);
      rApds = true;
      Serial.print(F("PASS - live ambient=")); Serial.println(a);
    } else Serial.println(F("FAIL - no data"));
  } else Serial.println(F("FAIL - not present"));

  // --- ultrasonic, 100 real samples ---
  Serial.print(F("  HC-SR04           : "));
  int drops = 0;
  for (int i = 0; i < 100; i++) {
    float d = pingOnce();
    if (d < 0) drops++;
    else { distSamples++; if (d < distMin) distMin = d; if (d > distMax) distMax = d; }
    delay(12);
  }
  ultraDropPct = drops;
  if (drops <= 10) {
    rUltra = true;
    Serial.print(F("PASS - ")); Serial.print(drops);
    Serial.print(F("% dropouts, range ")); Serial.print(distMin, 0);
    Serial.print(F("-")); Serial.print(distMax, 0); Serial.println(F(" mm"));
  } else {
    Serial.print(F("FAIL - ")); Serial.print(drops); Serial.println(F("% dropouts (limit 10%)"));
  }

  // --- LDR resting range ---
  Serial.print(F("  LDR range         : "));
  ldrRest = readLdr();
  if (ldrRest > 60 && ldrRest < 4000) {
    rLdrRange = true;
    Serial.print(F("PASS - resting at ")); Serial.print(ldrRest);
    Serial.print(F("/4095 (")); Serial.print(100.0f * ldrRest / 4095.0f, 0);
    Serial.println(F("% of scale, not clipped)"));
  } else {
    Serial.print(F("FAIL - pinned at ")); Serial.print(ldrRest);
    Serial.println(F(" - no usable dynamic range"));
  }

  line();
  Serial.println();
  Serial.println(F("PHASE B - interactive, 25 seconds. Please:"));
  Serial.println(F("   1. COVER the LDR with your hand, then uncover"));
  Serial.println(F("   2. PRESS the switch a few times"));
  Serial.println(F("   3. WATCH the red LED and LISTEN for the buzzer"));
  Serial.println();
}

void loop() {
  static unsigned long t0 = 0;
  static bool started = false, done = false;
  if (!started) { started = true; t0 = millis(); }
  if (done) { delay(1000); return; }

  unsigned long el = millis() - t0;

  // LDR response
  int v = readLdr();
  if (v < ldrMin) ldrMin = v;
  if (v > ldrMax) ldrMax = v;

  // switch
  static int lastSw = HIGH;
  int sw = digitalRead(PIN_SWITCH);
  if (sw == LOW && lastSw == HIGH) { rSwitch = true; Serial.println(F("   [switch press detected]")); }
  lastSw = sw;

  // LED + buzzer activity so the operator has something to see and hear
  static unsigned long lastB = 0;
  static bool on = false;
  if (millis() - lastB > 900) {
    lastB = millis();
    on = !on;
    digitalWrite(PIN_EXT_LED, on);
    digitalWrite(LEDB, on ? LOW : HIGH);
    if (on) tone(PIN_BUZZ, 1600); else noTone(PIN_BUZZ);
  }

  if (el > 25000) {
    done = true;
    noTone(PIN_BUZZ);
    digitalWrite(PIN_EXT_LED, LOW);
    digitalWrite(LEDB, HIGH);

    int swing = ldrMax - ldrMin;
    rLdrResponds = swing > 300;

    Serial.println();
    line();
    Serial.println(F("PHASE B RESULTS"));
    line();
    Serial.print(F("  LDR light response: "));
    Serial.print(rLdrResponds ? F("PASS - swing ") : F("FAIL - swing only "));
    Serial.print(swing);
    Serial.print(F(" counts (")); Serial.print(ldrMin);
    Serial.print(F("-")); Serial.print(ldrMax); Serial.println(F(")"));

    Serial.print(F("  Switch (D7)       : "));
    Serial.println(rSwitch ? F("PASS - press detected") : F("FAIL - no press seen"));

    Serial.println();
    line();
    Serial.println(F("GATE G0 SUMMARY"));
    line();
    Serial.print(F("  OLED           ")); Serial.println(rOled ? F("PASS") : F("FAIL"));
    Serial.print(F("  IMU            ")); Serial.println(rImu ? F("PASS") : F("FAIL"));
    Serial.print(F("  APDS9960       ")); Serial.println(rApds ? F("PASS") : F("FAIL"));
    Serial.print(F("  HC-SR04        ")); Serial.println(rUltra ? F("PASS") : F("FAIL"));
    Serial.print(F("  LDR range      ")); Serial.println(rLdrRange ? F("PASS") : F("FAIL"));
    Serial.print(F("  LDR response   ")); Serial.println(rLdrResponds ? F("PASS") : F("FAIL"));
    Serial.print(F("  Switch         ")); Serial.println(rSwitch ? F("PASS") : F("FAIL"));
    Serial.println(F("  LED + buzzer   confirm by eye/ear (driven above)"));
    line();

    bool all = rOled && rImu && rApds && rUltra && rLdrRange && rLdrResponds && rSwitch;
    Serial.println();
    if (all) {
      Serial.println(F("  *** GATE G0 PASSED - all automatic checks green ***"));
      Serial.println(F("  Next: python tools/calibrate.py --port COM4"));
    } else {
      Serial.println(F("  *** GATE G0 NOT PASSED - see FAIL lines above ***"));
    }
    Serial.println();

    if (rOled) {
      display.clearDisplay(); display.setCursor(0, 0); display.setTextSize(1);
      display.println(all ? F("GATE G0: PASS") : F("GATE G0: FAIL"));
      display.print(F("dist ")); display.print(distMin, 0); display.print(F("-"));
      display.print(distMax, 0); display.println(F("mm"));
      display.print(F("ldr  ")); display.print(ldrMin); display.print(F("-"));
      display.println(ldrMax);
      display.print(F("drop ")); display.print(ultraDropPct, 0); display.println(F("%"));
      display.display();
    }
  }
}
