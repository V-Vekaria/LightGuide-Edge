/*
 * LightGuide Edge - 03_inference
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * Phase 1: the distance channel.
 *
 * The device holds a reference distance and tells the operator whether the rig
 * is back where it was - CORRECT, FAR (move closer) or CLOSE (move back).
 *
 * The verdict itself lives in decision.h as a pure function. Phase 2 replaces
 * that one function body with the trained classifier; everything in this file
 * stays as it is.
 */

#include <string.h>          // strlen, used by printCentred
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "decision.h"
#include "self_test.h"

// ---- pins (docs/02-HARDWARE.md) ----------------------------------------
const int PIN_TRIG   = 2;
const int PIN_ECHO   = 3;
const int PIN_SWITCH = 7;
const int PIN_BUZZ   = 9;

// ---- ultrasonic ---------------------------------------------------------
const int ULTRA_SAMPLES = 5;
const int PING_GAP_MS   = 10;
const unsigned long ECHO_START_TIMEOUT_US = 15000UL;
const unsigned long ECHO_HIGH_MAX_US      = 25000UL;
const float DIST_MIN_MM = 20.0f;
const float DIST_MAX_MM = 4000.0f;

// ---- reference ----------------------------------------------------------
// 100 cm is the operator's established working distance, so the device is
// useful the instant it powers up and a faulty switch cannot leave it unusable.
const float DEFAULT_REF_MM = 1000.0f;

enum Mode { MODE_GUIDE, MODE_SETUP };

Mode    mode        = MODE_GUIDE;
float   refMm       = DEFAULT_REF_MM;
Verdict prevVerdict = V_NO_ECHO;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledOk = false;

bool          buzzerOn         = false;
unsigned long buzzerPhaseStart = 0;
bool          correctChirpDone = false;

// True once the self-test result has actually reached an attached host.
bool selfTestReported = false;

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

// Median of 5. Ultrasonic errors are wild outliers, not gaussian noise, so one
// bad echo would drag a mean badly while a median simply ignores it.
float readDistanceMm(int &missed) {
  float v[ULTRA_SAMPLES];
  int n = 0;
  missed = 0;

  for (int i = 0; i < ULTRA_SAMPLES; i++) {
    float d = pingOnce();
    if (d > 0) v[n++] = d; else missed++;
    delay(PING_GAP_MS);
  }
  if (n == 0) return -1;

  for (int i = 1; i < n; i++) {
    float k = v[i]; int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[n / 2];
}

// One CSV row per loop. This is the log the online evaluation is computed from
// later, which is why it carries the mode and the raw miss count as well as the
// verdict - a row that cannot be trusted must be identifiable after the fact.
void emit(float liveMm, Verdict v, int missed) {
  Serial.print(mode == MODE_GUIDE ? F("GUIDE") : F("SETUP"));
  Serial.print(',');
  Serial.print(refMm, 0);
  Serial.print(',');
  Serial.print(liveMm, 0);
  Serial.print(',');
  if (liveMm > 0) Serial.print(liveMm - refMm, 0); else Serial.print(F("NA"));
  Serial.print(',');
  Serial.print(verdictName(v));
  Serial.print(',');
  Serial.println(missed);
}

// ---- display ------------------------------------------------------------

// The Adafruit font is 6 px wide per character at size 1, so the pixel width of
// a string is length * 6 * size. Centring by hand would have to be redone every
// time a word changed length.
void printCentred(const char *s, int size, int y) {
  int w = (int)strlen(s) * 6 * size;
  int x = (128 - w) / 2;
  if (x < 0) x = 0;
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(s);
}

// A ruler. The reference sits at the centre tick and the live reading slides
// along it. Full width spans +-200 mm, wide enough to watch yourself approach
// without the marker pinned to an end for most of the walk.
void drawBar(float liveMm) {
  const int BAR_Y = 56, BAR_H = 8;
  display.drawRect(0, BAR_Y, 128, BAR_H, SSD1306_WHITE);
  display.drawFastVLine(64, BAR_Y, BAR_H, SSD1306_WHITE);

  if (liveMm <= 0) return;

  int x = 64 + (int)((liveMm - refMm) * 64.0f / 200.0f);
  if (x < 2)   x = 2;
  if (x > 125) x = 125;
  display.fillRect(x - 1, BAR_Y + 2, 3, BAR_H - 4, SSD1306_WHITE);
}

void render(float liveMm, Verdict v, int missed) {
  if (!oledOk) return;
  display.clearDisplay();

  // What we are aiming for, and what we have.
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("REF ")); display.print(refMm / 10.0f, 0); display.print(F("cm"));
  display.setCursor(68, 0);
  display.print(F("NOW "));
  if (liveMm > 0) display.print(liveMm / 10.0f, 0); else display.print(F("--"));

  // The verdict, as large as the panel allows. CORRECT at size 3 is 126 px of
  // the 128 available - if it clips on the hardware, drop this one call to 2.
  printCentred(v == V_NO_ECHO ? "NO ECHO" : verdictName(v), 3, 18);

  // How far out, and which way to move.
  display.setTextSize(1);
  display.setCursor(0, 46);
  if (v == V_NO_ECHO) {
    display.print(F("aim at target"));
  } else {
    float diffCm = (liveMm - refMm) / 10.0f;
    if (diffCm >= 0) display.print('+');
    display.print(diffCm, 0);
    display.print(F("cm "));
    if      (v == V_FAR)   display.print(F("move closer"));
    else if (v == V_CLOSE) display.print(F("move back"));
    else if (missed > 1)   display.print(F("hold it (noisy)"));
    else                   display.print(F("hold it"));
  }

  drawBar(liveMm);
  display.display();
}

// ---- sound and light ----------------------------------------------------

void silenceBuzzer() {
  noTone(PIN_BUZZ);
  buzzerOn = false;
}

// Rate encodes urgency, pitch encodes direction, and CORRECT is silent after a
// single chirp. Silence-means-success is the parking-sensor convention, and it
// is also practical: a continuous tone would cover the narration in the video.
//
// Every transition is scheduled off millis(). Sounding a tone with delay() would
// block the loop and make the display lag the sensor by the length of the beep.
void updateBuzzer(Verdict v, bool verdictChanged) {
  unsigned long now = millis();

  if (verdictChanged) {
    silenceBuzzer();
    buzzerPhaseStart = now;
    correctChirpDone = false;
  }

  if (v == V_NO_ECHO) {
    silenceBuzzer();
    return;
  }

  if (v == V_CORRECT) {
    if (correctChirpDone) return;
    if (!buzzerOn) {
      tone(PIN_BUZZ, 1500);
      buzzerOn = true;
      buzzerPhaseStart = now;
    } else if (now - buzzerPhaseStart >= 150) {
      silenceBuzzer();
      correctChirpDone = true;
    }
    return;
  }

  const unsigned int  freq  = (v == V_FAR) ? 400 : 1200;
  const unsigned long onMs  = (v == V_FAR) ? 80  : 60;
  const unsigned long perMs = (v == V_FAR) ? 600 : 200;

  unsigned long phase = now - buzzerPhaseStart;
  if (phase >= perMs) {
    buzzerPhaseStart = now;
    phase = 0;
  }

  if (phase < onMs) {
    if (!buzzerOn) { tone(PIN_BUZZ, freq); buzzerOn = true; }
  } else {
    if (buzzerOn) silenceBuzzer();
  }
}

// The on-board RGB LED is active LOW, so LOW turns a channel on. It needs no
// wiring and it is the output that actually reads on camera across a room.
void updateLed(Verdict v) {
  digitalWrite(LEDR, (v == V_FAR || v == V_CLOSE) ? LOW : HIGH);
  digitalWrite(LEDG, (v == V_CORRECT)             ? LOW : HIGH);
  digitalWrite(LEDB, HIGH);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  Wire.begin();
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledOk) display.setTextColor(SSD1306_WHITE);
  else        Serial.println(F("# OLED not found at 0x3C - continuing without it"));

  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(LEDR, OUTPUT); pinMode(LEDG, OUTPUT); pinMode(LEDB, OUTPUT);
  digitalWrite(LEDR, HIGH); digitalWrite(LEDG, HIGH); digitalWrite(LEDB, HIGH);

  runDecisionSelfTest(Serial);
  selfTestReported = (bool)Serial;
  Serial.println(F("mode,ref_mm,live_mm,diff_mm,verdict,missed"));
}

void loop() {
  // Booting takes less time than uploading and re-enumerating the USB port, so
  // the report in setup() usually goes nowhere. Print it once more the moment a
  // host actually appears - the result should be observable whenever you attach
  // a monitor, not only if you win a race against the board.
  if (!selfTestReported && Serial) {
    runDecisionSelfTest(Serial);
    Serial.println(F("mode,ref_mm,live_mm,diff_mm,verdict,missed"));
    selfTestReported = true;
  }

  int missed = 0;
  float liveMm = readDistanceMm(missed);
  Verdict v = decide(liveMm, refMm, prevVerdict);

  render(liveMm, v, missed);
  updateBuzzer(v, v != prevVerdict);
  updateLed(v);
  emit(liveMm, v, missed);

  prevVerdict = v;
}
