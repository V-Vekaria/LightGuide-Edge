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

// Twenty medians get medianed again to set a new reference. Twelve of them must
// come back valid or the save is refused: a reference captured from a bad echo
// would corrupt every later verdict while looking exactly like a software fault.
const int REF_SAMPLES   = 20;
const int REF_MIN_VALID = 12;

// ---- button -------------------------------------------------------------
// Hold rather than tap. A tap is one accidental knock away from silently
// re-referencing to whatever happens to be in front of the sensor, which you
// would not notice until the verdicts stopped making sense.
const unsigned long HOLD_MS        = 2000;
const unsigned long STUCK_CHECK_MS = 3000;

float   refMm       = DEFAULT_REF_MM;
Verdict prevVerdict = V_NO_READ;
bool    switchFaulty = false;

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
// later, which is why it carries the raw miss count as well as the verdict - a
// row that cannot be trusted must be identifiable after the fact. Reference
// changes are marked by a `#` comment line rather than a column, so the moment
// the operator re-referenced is findable in the log.
void emit(float liveMm, Verdict v, int missed) {
  Serial.print(refMm, 0);
  Serial.print(',');
  Serial.print(liveMm, 0);
  Serial.print(',');
  if (liveMm > 0) Serial.print(liveMm - refMm, 0); else Serial.print(F("NA"));
  Serial.print(',');
  Serial.print(distanceWord(v));
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
  printCentred(distanceWord(v), 3, 18);

  // How far out, and which way to move.
  display.setTextSize(1);
  display.setCursor(0, 46);
  if (v == V_NO_READ) {
    display.print(F("aim at target"));
  } else {
    float diffCm = (liveMm - refMm) / 10.0f;
    if (diffCm >= 0) display.print('+');
    display.print(diffCm, 0);
    display.print(F("cm "));
    if      (v == V_ABOVE)   display.print(F("move closer"));
    else if (v == V_BELOW) display.print(F("move back"));
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

  if (v == V_NO_READ) {
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

  const unsigned int  freq  = (v == V_ABOVE) ? 400 : 1200;
  const unsigned long onMs  = (v == V_ABOVE) ? 80  : 60;
  const unsigned long perMs = (v == V_ABOVE) ? 600 : 200;

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
  digitalWrite(LEDR, (v == V_ABOVE || v == V_BELOW) ? LOW : HIGH);
  digitalWrite(LEDG, (v == V_CORRECT)             ? LOW : HIGH);
  digitalWrite(LEDB, HIGH);
}

// ---- setting the reference ----------------------------------------------

// Fires once when the button has been held for HOLD_MS, and not again until it
// is released and pressed afresh. Non-blocking, because the loop has to keep
// reading the sensor while the operator is holding the button down.
bool holdFired() {
  static bool          wasDown = false;
  static unsigned long downAt  = 0;
  static bool          fired   = false;

  if (switchFaulty) return false;

  const bool down = (digitalRead(PIN_SWITCH) == LOW);
  const unsigned long now = millis();

  if (!down)    { wasDown = false; return false; }
  if (!wasDown) { wasDown = true; downAt = now; fired = false; return false; }

  if (!fired && (now - downAt) >= HOLD_MS) {
    fired = true;
    return true;
  }
  return false;
}

// Returns the new reference in mm, or -1 if too few pings came back valid.
float captureReference() {
  float v[REF_SAMPLES];
  int n = 0;

  for (int i = 0; i < REF_SAMPLES; i++) {
    int missed = 0;
    float d = readDistanceMm(missed);
    if (d > 0) v[n++] = d;

    if (oledOk) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 8);
      display.print(F("SAVING"));
      display.setTextSize(1);
      display.setCursor(0, 32);
      display.print(n); display.print(F(" of ")); display.print(i + 1);
      display.print(F(" good"));
      display.drawRect(0, 48, 128, 10, SSD1306_WHITE);
      display.fillRect(2, 50, (int)((124L * (i + 1)) / REF_SAMPLES), 6, SSD1306_WHITE);
      display.display();
    }
  }

  if (n < REF_MIN_VALID) return -1.0f;

  for (int i = 1; i < n; i++) {
    float k = v[i]; int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[n / 2];
}

// delay() is acceptable here and nowhere else in this sketch: saving is a
// one-shot action the operator asked for, and nothing is being tracked while it
// runs. In the main loop the same call would make the display lag the sensor.
void doSave() {
  silenceBuzzer();
  float r = captureReference();

  if (r < 0.0f) {
    Serial.println(F("# SAVE REFUSED - not enough valid echoes"));
    tone(PIN_BUZZ, 200); delay(400); noTone(PIN_BUZZ);
    if (oledOk) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 10);
      display.print(F("SAVE"));
      display.setCursor(0, 30);
      display.print(F("FAILED"));
      display.setTextSize(1);
      display.setCursor(0, 54);
      display.print(F("aim at target, retry"));
      display.display();
      delay(1800);
    }
    return;                       // the old reference survives untouched
  }

  refMm = r;
  Serial.print(F("# REFERENCE SAVED "));
  Serial.print(refMm, 0);
  Serial.println(F(" mm"));

  tone(PIN_BUZZ, 1200); delay(80); noTone(PIN_BUZZ); delay(60);
  tone(PIN_BUZZ, 1600); delay(80); noTone(PIN_BUZZ);

  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 12);
    display.print(F("SAVED"));
    display.setTextSize(3);
    display.setCursor(0, 34);
    display.print(refMm / 10.0f, 0);
    display.setTextSize(1);
    display.setCursor(96, 44);
    display.print(F("cm"));
    display.display();
    delay(1400);
  }

  // Forget the history so the new reference earns its verdict from scratch and
  // CORRECT chirps on arrival rather than sliding in silently.
  prevVerdict = V_NO_READ;
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

  pinMode(PIN_SWITCH, INPUT_PULLUP);

  // A switch stuck LOW would re-reference over and over and make the device
  // useless. A healthy released switch reads HIGH and leaves this immediately;
  // only a genuinely stuck one costs the full three seconds.
  switchFaulty = true;
  unsigned long t0 = millis();
  while (millis() - t0 < STUCK_CHECK_MS) {
    if (digitalRead(PIN_SWITCH) == HIGH) { switchFaulty = false; break; }
    delay(10);
  }
  if (switchFaulty) Serial.println(F("# switch stuck LOW - disabled for this run"));

  runDecisionSelfTest(Serial);
  selfTestReported = (bool)Serial;
  Serial.println(F("ref_mm,live_mm,diff_mm,verdict,missed"));
}

void loop() {
  // Booting takes less time than uploading and re-enumerating the USB port, so
  // the report in setup() usually goes nowhere. Print it once more the moment a
  // host actually appears - the result should be observable whenever you attach
  // a monitor, not only if you win a race against the board.
  if (!selfTestReported && Serial) {
    runDecisionSelfTest(Serial);
    Serial.println(F("ref_mm,live_mm,diff_mm,verdict,missed"));
    selfTestReported = true;
  }

  if (holdFired()) {
    doSave();
    return;
  }

  int missed = 0;
  float liveMm = readDistanceMm(missed);
  Verdict v = decideDistance(liveMm, refMm, prevVerdict);

  render(liveMm, v, missed);
  updateBuzzer(v, v != prevVerdict);
  updateLed(v);
  emit(liveMm, v, missed);

  prevVerdict = v;
}
