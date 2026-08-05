/*
 * LightGuide Edge - 02b_button_logger
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * Button-triggered labelled capture. Replaces the free-running logger for real
 * collection: you stage a physical condition, press the button, and exactly one
 * clean run is recorded. Nothing is captured while you are walking around, moving
 * the stand, or adjusting the dimmer - so the dataset contains only settled,
 * deliberately-staged conditions.
 *
 * This also removes the laptop from the loop. The OLED shows the current class, so
 * the whole collection can be done at the stand with the laptop just listening.
 *
 * CONTROLS
 *   SHORT press (< 700 ms)  ->  capture one run
 *                               2 s warm-up discarded, then 3 s recorded (30 samples)
 *   LONG  press (> 700 ms)  ->  cycle to the next class label
 *
 * FEEDBACK
 *   OLED    current label, run count, and status
 *   LED     on while recording
 *   Buzzer  one beep at start, two at end
 *
 * SERIAL PROTOCOL (tools/button_capture.py parses this)
 *   # RUN_START label=2 name=too_far session=1
 *   t_ms,dist_mm,ldr_raw,ax,ay,az,gx,gy,gz,pitch,roll,amag,label,session
 *   ...rows...
 *   # RUN_END rows=30 dropouts=0
 *
 * SERIAL COMMANDS
 *   S<n>   set session id      L<0-5> set label      ?  status
 */

#define BOARD_REV 1

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#if BOARD_REV == 1
  #include <Arduino_LSM9DS1.h>
#else
  #include <Arduino_BMI270_BMM150.h>
#endif

const int PIN_LDR = A0, PIN_TRIG = 2, PIN_ECHO = 3;
const int PIN_SWITCH = 7, PIN_EXT_LED = 8, PIN_BUZZ = 9;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledOk = false;

const unsigned long SAMPLE_PERIOD_MS = 100;   // 10 Hz
const unsigned long WARMUP_MS        = 2000;  // discarded: LDR settles, first pings unreliable
const unsigned long RECORD_MS        = 3000;  // 30 samples per run
const unsigned long LONG_PRESS_MS    = 700;
const int  ULTRA_SAMPLES = 5, LDR_SAMPLES = 8, PING_GAP_MS = 10;
const unsigned long ECHO_START_TIMEOUT_US = 15000UL;
const unsigned long ECHO_HIGH_MAX_US      = 25000UL;
const float DIST_MIN_MM = 20.0f, DIST_MAX_MM = 4000.0f;

const char* CLASS_NAMES[6] = {
  "optimal", "too_close", "too_far", "underlit", "overlit", "tilt_off"
};

int label = 0, session = 1;
int runCount[6] = {0, 0, 0, 0, 0, 0};

// ---------------------------------------------------------------- sensing

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

float readDistanceMm() {
  float v[ULTRA_SAMPLES];
  int n = 0;
  for (int i = 0; i < ULTRA_SAMPLES; i++) {
    float d = pingOnce();
    if (d > 0) v[n++] = d;
    delay(PING_GAP_MS);
  }
  if (n == 0) return -1;
  for (int i = 1; i < n; i++) {          // insertion sort, n <= 5
    float k = v[i]; int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[n / 2];
}

int readLdr() {
  long a = 0;
  for (int i = 0; i < LDR_SAMPLES; i++) a += analogRead(PIN_LDR);
  return a / LDR_SAMPLES;
}

// ---------------------------------------------------------------- display

void drawIdle() {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("S")); display.print(session);
  display.print(F("  class ")); display.println(label);
  display.setTextSize(2);
  display.setCursor(0, 12);
  display.println(CLASS_NAMES[label]);
  display.setTextSize(1);
  display.setCursor(0, 34);
  display.print(F("runs: ")); display.println(runCount[label]);
  display.setCursor(0, 48);
  display.println(F("tap=rec  hold=next"));
  display.display();
}

void drawStatus(const __FlashStringHelper* msg, int n) {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 4);
  display.println(msg);
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.println(CLASS_NAMES[label]);
  if (n >= 0) { display.setCursor(0, 44); display.print(F("samples: ")); display.println(n); }
  display.display();
}

// ---------------------------------------------------------------- capture

void captureRun() {
  Serial.print(F("# RUN_START label=")); Serial.print(label);
  Serial.print(F(" name=")); Serial.print(CLASS_NAMES[label]);
  Serial.print(F(" session=")); Serial.println(session);
  Serial.println(F("t_ms,dist_mm,ldr_raw,ax,ay,az,gx,gy,gz,pitch,roll,amag,label,session"));

  tone(PIN_BUZZ, 1400); delay(120); noTone(PIN_BUZZ);

  // Warm-up: sampled but discarded, so the LDR settles and the first ultrasonic
  // pings (which are unreliable) never reach the dataset.
  drawStatus(F("WARM UP"), -1);
  unsigned long t0 = millis();
  while (millis() - t0 < WARMUP_MS) { readDistanceMm(); readLdr(); }

  digitalWrite(PIN_EXT_LED, HIGH);
  digitalWrite(LEDR, LOW);                   // on-board red = recording
  drawStatus(F("REC"), 0);

  unsigned long start = millis(), lastSample = 0;
  int rows = 0, drops = 0;
  while (millis() - start < RECORD_MS) {
    if (millis() - lastSample < SAMPLE_PERIOD_MS) continue;
    lastSample = millis();

    float dist = readDistanceMm();
    int   ldr  = readLdr();
    float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
    if (IMU.accelerationAvailable()) IMU.readAcceleration(ax, ay, az);
    if (IMU.gyroscopeAvailable())    IMU.readGyroscope(gx, gy, gz);
    float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI;
    float roll  = atan2(ay, az) * 180.0f / PI;
    float amag  = sqrt(ax * ax + ay * ay + az * az);

    if (dist < 0) drops++;
    rows++;

    Serial.print(millis() - start); Serial.print(',');
    Serial.print(dist, 1);   Serial.print(',');
    Serial.print(ldr);       Serial.print(',');
    Serial.print(ax, 4);     Serial.print(',');
    Serial.print(ay, 4);     Serial.print(',');
    Serial.print(az, 4);     Serial.print(',');
    Serial.print(gx, 2);     Serial.print(',');
    Serial.print(gy, 2);     Serial.print(',');
    Serial.print(gz, 2);     Serial.print(',');
    Serial.print(pitch, 2);  Serial.print(',');
    Serial.print(roll, 2);   Serial.print(',');
    Serial.print(amag, 4);   Serial.print(',');
    Serial.print(label);     Serial.print(',');
    Serial.println(session);

    if (rows % 5 == 0) drawStatus(F("REC"), rows);
  }

  digitalWrite(PIN_EXT_LED, LOW);
  digitalWrite(LEDR, HIGH);
  runCount[label]++;

  Serial.print(F("# RUN_END rows=")); Serial.print(rows);
  Serial.print(F(" dropouts=")); Serial.println(drops);

  tone(PIN_BUZZ, 2000); delay(90); noTone(PIN_BUZZ); delay(60);
  tone(PIN_BUZZ, 2400); delay(90); noTone(PIN_BUZZ);

  drawStatus(F("SAVED"), rows);
  delay(900);
  drawIdle();
}

// ---------------------------------------------------------------- commands

void handleCommand(const String& c) {
  if (!c.length()) return;
  char k = toupper(c.charAt(0));
  if (k == 'S') { session = c.substring(1).toInt(); Serial.print(F("# session=")); Serial.println(session); }
  else if (k == 'L') {
    int v = c.substring(1).toInt();
    if (v >= 0 && v <= 5) { label = v; Serial.print(F("# label=")); Serial.println(CLASS_NAMES[label]); }
  } else if (k == '?') {
    Serial.print(F("# session=")); Serial.print(session);
    Serial.print(F(" label=")); Serial.print(label);
    Serial.print(F(" (")); Serial.print(CLASS_NAMES[label]); Serial.println(F(")"));
    for (int i = 0; i < 6; i++) {
      Serial.print(F("#   ")); Serial.print(CLASS_NAMES[i]);
      Serial.print(F(": ")); Serial.print(runCount[i]); Serial.println(F(" runs"));
    }
  }
  drawIdle();
}

// ----------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) { }

  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_ECHO, INPUT); digitalWrite(PIN_TRIG, LOW);
  pinMode(PIN_SWITCH, INPUT_PULLUP);
  pinMode(PIN_EXT_LED, OUTPUT); pinMode(PIN_BUZZ, OUTPUT);
  digitalWrite(PIN_EXT_LED, LOW);
  pinMode(LEDR, OUTPUT); pinMode(LEDG, OUTPUT); pinMode(LEDB, OUTPUT);
  digitalWrite(LEDR, HIGH); digitalWrite(LEDG, HIGH); digitalWrite(LEDB, HIGH);
  analogReadResolution(12);

  Wire.begin();
  IMU.begin();
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledOk) display.setTextColor(SSD1306_WHITE);

  Serial.println(F("# LightGuide Edge - button logger ready"));
  Serial.println(F("# TAP = record one run   HOLD = next class"));
  drawIdle();
}

void loop() {
  static String buf;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') { if (buf.length()) { handleCommand(buf); buf = ""; } }
    else if (buf.length() < 16) buf += ch;
  }

  // Press classification: measure how long the button is held, then act on
  // RELEASE. Deciding on release is what lets one button do two jobs.
  static bool wasDown = false;
  static unsigned long downAt = 0;
  bool isDown = (digitalRead(PIN_SWITCH) == LOW);

  if (isDown && !wasDown) {
    downAt = millis();
    wasDown = true;
  } else if (isDown && wasDown && millis() - downAt > LONG_PRESS_MS) {
    // Show the pending action while still held, so the intent is visible before
    // committing to it.
    drawStatus(F("NEXT >"), -1);
  } else if (!isDown && wasDown) {
    unsigned long held = millis() - downAt;
    wasDown = false;
    if (held < 40) return;                         // debounce
    if (held >= LONG_PRESS_MS) {
      label = (label + 1) % 6;
      Serial.print(F("# label -> ")); Serial.println(CLASS_NAMES[label]);
      tone(PIN_BUZZ, 900); delay(70); noTone(PIN_BUZZ);
      drawIdle();
    } else {
      captureRun();
    }
  }
}
