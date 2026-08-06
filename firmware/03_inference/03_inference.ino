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
const int PIN_LDR    = A0;

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
// (No default reference. See the `armed` flag below - the device is either set
// up or it is not, and it never invents a reference it was not given.)

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

// The device is either set up or it is not. A reference exists because the
// operator captured it, or the device says plainly that it has none. One rule
// for both channels, and no universal "correct" light level to invent - because
// none exists: the right ADC count depends entirely on the room, the lamp and
// where the LDR sits.
bool    armed        = false;
float   refMm        = 0.0f;
int     refLight     = 0;

Verdict prevDist     = V_NO_READ;
Verdict prevLight    = V_NO_READ;
bool    switchFaulty = false;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledOk = false;

bool          buzzerOn         = false;
unsigned long buzzerPhaseStart = 0;
bool          correctChirpDone = false;

// True once the self-test result has actually reached an attached host.
bool selfTestReported = false;

// One loop's worth of sensing from both channels.
struct Sample {
  float distMm;   // millimetres, or -1 if every ping failed
  int   missed;   // failed pings out of ULTRA_SAMPLES
  int   light;    // mean LDR counts, 0-4095
};

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

// Read the LDR continuously for `ms`, accumulating into the caller's totals.
//
// This runs in the gap the ultrasonic must leave between pings anyway, so the
// light channel costs no loop time at all. Spreading five short windows across
// the ping train also rejects 100 Hz mains ripple and PWM dimmer chop better
// than one contiguous window of the same total length would.
void accumulateLdr(unsigned long ms, unsigned long &acc, unsigned long &count) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    acc += analogRead(PIN_LDR);
    count++;
  }
}

// Median of 5 for distance. Ultrasonic errors are wild outliers, not gaussian
// noise, so one bad echo would drag a mean badly while a median ignores it.
// Light is a mean, because its noise genuinely is small and symmetric.
Sample sense() {
  Sample s;
  float v[ULTRA_SAMPLES];
  int n = 0;
  s.missed = 0;

  unsigned long ldrAcc = 0, ldrCount = 0;

  for (int i = 0; i < ULTRA_SAMPLES; i++) {
    float d = pingOnce();
    if (d > 0) v[n++] = d; else s.missed++;
    accumulateLdr(PING_GAP_MS, ldrAcc, ldrCount);
  }

  s.light = ldrCount ? (int)(ldrAcc / ldrCount) : 0;

  if (n == 0) {
    s.distMm = -1;
    return s;
  }

  for (int i = 1; i < n; i++) {
    float k = v[i]; int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  s.distMm = v[n / 2];
  return s;
}

// One CSV row per loop. This is the log the online evaluation is computed from
// later, which is why it carries the raw miss count as well as the verdict - a
// row that cannot be trusted must be identifiable after the fact. Reference
// changes are marked by a `#` comment line rather than a column, so the moment
// the operator re-referenced is findable in the log.
// Before a setup is captured there is no reference, so the difference columns
// print NA rather than subtracting zero, and the verdict columns say UNSET
// rather than NO ECHO - the echo is fine, there is simply nothing to judge it
// against, and a log that conflates those two is a log that misleads later.
void emit(const Sample &s, Verdict vd, Verdict vl) {
  Serial.print(refMm, 0);            Serial.print(',');
  Serial.print(s.distMm, 0);         Serial.print(',');
  if (armed && s.distMm > 0) Serial.print(s.distMm - refMm, 0); else Serial.print(F("NA"));
  Serial.print(',');
  Serial.print(armed ? distanceWord(vd) : "UNSET");
  Serial.print(',');
  Serial.print(s.missed);            Serial.print(',');
  Serial.print(refLight);            Serial.print(',');
  Serial.print(s.light);             Serial.print(',');
  if (armed) Serial.print(s.light - refLight); else Serial.print(F("NA"));
  Serial.print(',');
  Serial.println(armed ? lightWord(vl) : "UNSET");
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
// along it. `span` is the deviation that reaches either end, so each channel
// picks a scale that suits it.
void drawBar(int y, float live, float ref, float span) {
  const int H = 4;
  display.drawRect(0, y, 128, H, SSD1306_WHITE);
  display.drawFastVLine(64, y, H, SSD1306_WHITE);

  if (live <= 0 || span <= 0) return;

  int x = 64 + (int)((live - ref) * 64.0f / span);
  if (x < 2)   x = 2;
  if (x > 125) x = 125;
  display.fillRect(x - 1, y + 1, 3, H - 2, SSD1306_WHITE);
}

void render(const Sample &s, Verdict vd, Verdict vl) {
  if (!oledOk) return;
  display.clearDisplay();

  if (!armed) {
    // This screen's job is to help the operator position the rig and set the
    // lamp before capturing, so the live numbers are the content.
    printCentred("NO SETUP SAVED", 1, 0);
    printCentred("hold button 2s", 1, 10);
    display.drawFastHLine(0, 22, 128, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(0, 28);
    display.print(F("d "));
    if (s.distMm > 0) display.print(s.distMm / 10.0f, 0); else display.print(F("--"));
    display.setTextSize(1);
    display.print(F(" cm"));

    display.setTextSize(2);
    display.setCursor(0, 46);
    display.print(F("l "));
    display.print(s.light);

    display.display();
    return;
  }

  // Distance, top half.
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("DIST ")); display.print(refMm / 10.0f, 0); display.print(F("cm"));
  display.setCursor(74, 0);
  if (s.distMm > 0) display.print(s.distMm / 10.0f, 0); else display.print(F("--"));
  display.print(F("cm"));
  printCentred(distanceWord(vd), 2, 9);
  drawBar(26, s.distMm, refMm, 200.0f);

  display.drawFastHLine(0, 31, 128, SSD1306_WHITE);

  // Light, bottom half.
  display.setTextSize(1);
  display.setCursor(0, 33);
  display.print(F("LGHT ")); display.print(refLight);
  display.setCursor(80, 33);
  display.print(s.light);
  printCentred(lightWord(vl), 2, 42);
  // Span is a quarter of the reference, so the marker reaches an end at the
  // same proportional deviation whatever the light level - matching the
  // proportional tolerance band.
  drawBar(59, (float)s.light, (float)refLight, refLight * 0.25f);

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
// Distance first, then light. That is the order a rig is actually set up in -
// position the stand, then dial the brightness - so the operator is only ever
// asked to fix one thing at a time, and the sound is never ambiguous about
// which channel it means.
//
// Rules are evaluated in order; the first match wins.
//   0. not armed           -> silent, there is nothing to guide towards
//   1. distance unreadable -> silent, cannot say which way to move
//   2. distance wrong      -> distance pattern
//   3. light unreadable    -> silent, a clipped divider is not a verdict
//   4. light wrong         -> double-blip
//   5. both correct        -> one chirp, then silence
//
// Rules 1 and 3 are separate on purpose. An unreadable distance silences
// everything, because the operator is being asked to move and the device cannot
// tell them where to. An unreadable light silences only the light layer - the
// distance guidance above it has already been given.
void updateBuzzer(Verdict vd, Verdict vl) {
  static Verdict lastD     = V_NO_READ;
  static Verdict lastL     = V_NO_READ;
  static bool    lastArmed = false;

  const unsigned long now = millis();

  if (vd != lastD || vl != lastL || armed != lastArmed) {
    silenceBuzzer();
    buzzerPhaseStart = now;
    correctChirpDone = false;
    lastD = vd; lastL = vl; lastArmed = armed;
  }

  if (!armed || vd == V_NO_READ) { silenceBuzzer(); return; }

  if (vd != V_CORRECT) {
    const unsigned int  freq  = (vd == V_ABOVE) ? 400 : 1200;
    const unsigned long onMs  = (vd == V_ABOVE) ? 80  : 60;
    const unsigned long perMs = (vd == V_ABOVE) ? 600 : 200;

    unsigned long phase = now - buzzerPhaseStart;
    if (phase >= perMs) { buzzerPhaseStart = now; phase = 0; }

    if (phase < onMs) { if (!buzzerOn) { tone(PIN_BUZZ, freq); buzzerOn = true; } }
    else              { if (buzzerOn) silenceBuzzer(); }
    return;
  }

  if (vl == V_NO_READ) { silenceBuzzer(); return; }

  if (vl != V_CORRECT) {
    // Two 40 ms tones 80 ms apart, repeating every 700 ms. The rhythm is what
    // identifies the light channel by ear before the operator looks up; pitch
    // then says which direction.
    const unsigned int freq = (vl == V_ABOVE) ? 1000 : 500;

    unsigned long phase = now - buzzerPhaseStart;
    if (phase >= 700) { buzzerPhaseStart = now; phase = 0; }

    const bool on = (phase < 40) || (phase >= 80 && phase < 120);
    if (on) { if (!buzzerOn) { tone(PIN_BUZZ, freq); buzzerOn = true; } }
    else    { if (buzzerOn) silenceBuzzer(); }
    return;
  }

  // Both correct: one chirp on arrival, then silence. Silence-means-success is
  // the parking-sensor convention, and a continuous tone would cover the
  // narration in the demo video.
  if (correctChirpDone) { if (buzzerOn) silenceBuzzer(); return; }
  if (!buzzerOn) {
    tone(PIN_BUZZ, 1500);
    buzzerOn = true;
    buzzerPhaseStart = now;
  } else if (now - buzzerPhaseStart >= 150) {
    silenceBuzzer();
    correctChirpDone = true;
  }
}

// Green both right, red distance wrong, blue light wrong, off when unset. The
// on-board RGB is active LOW, needs no wiring, and is the output that actually
// reads on camera from across a room - the colour names the failing channel
// before the operator is close enough to read the screen.
void updateLed(Verdict vd, Verdict vl) {
  bool r = false, g = false, b = false;

  if (armed) {
    if      (vd == V_ABOVE || vd == V_BELOW)     r = true;
    else if (vl == V_ABOVE || vl == V_BELOW)     b = true;
    else if (vd == V_CORRECT && vl == V_CORRECT) g = true;
  }

  digitalWrite(LEDR, r ? LOW : HIGH);
  digitalWrite(LEDG, g ? LOW : HIGH);
  digitalWrite(LEDB, b ? LOW : HIGH);
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

// Returns true and fills both references, or returns false and changes nothing.
// Twenty samples: median for distance, mean for light.
bool captureReference(float &outMm, int &outLight) {
  float v[REF_SAMPLES];
  long  lightAcc = 0;
  int   n = 0;

  for (int i = 0; i < REF_SAMPLES; i++) {
    Sample smp = sense();
    float d = smp.distMm;
    if (d > 0) v[n++] = d;
    lightAcc += smp.light;

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

  if (n < REF_MIN_VALID) return false;

  // A reference pinned near either rail is a clipped divider, not a light
  // level. Accepting it would bake a wiring fault into every later verdict.
  int light = (int)(lightAcc / REF_SAMPLES);
  if (!lightReadingValid(light)) return false;

  for (int i = 1; i < n; i++) {
    float k = v[i]; int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  outMm    = v[n / 2];
  outLight = light;
  return true;
}

// delay() is acceptable here and nowhere else in this sketch: saving is a
// one-shot action the operator asked for, and nothing is being tracked while it
// runs. In the main loop the same call would make the display lag the sensor.
void doSave() {
  silenceBuzzer();

  float newMm    = 0.0f;
  int   newLight = 0;

  if (!captureReference(newMm, newLight)) {
    // Both failure modes reach the operator the same way, because the remedy is
    // the same: aim it properly and try again. The serial log keeps the
    // distinction for later diagnosis.
    Serial.println(F("# SAVE REFUSED - bad echoes or clipped light reading"));
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
    return;                       // both previous references survive untouched
  }

  refMm    = newMm;
  refLight = newLight;
  armed    = true;

  Serial.print(F("# REFERENCE SAVED "));
  Serial.print(refMm, 0);   Serial.print(F(" mm / "));
  Serial.print(refLight);   Serial.println(F(" counts"));

  tone(PIN_BUZZ, 1200); delay(80); noTone(PIN_BUZZ); delay(60);
  tone(PIN_BUZZ, 1600); delay(80); noTone(PIN_BUZZ);

  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 4);  display.print(F("SAVED"));
    display.setTextSize(1);
    display.setCursor(0, 28); display.print(F("dist  "));
    display.print(refMm / 10.0f, 0); display.print(F(" cm"));
    display.setCursor(0, 42); display.print(F("light "));
    display.print(refLight);
    display.display();
    delay(1600);
  }

  // Forget both histories so the new setup earns its verdicts from scratch and
  // CORRECT chirps on arrival rather than sliding in silently.
  prevDist  = V_NO_READ;
  prevLight = V_NO_READ;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  // 12-bit, not the 10-bit default. Every light band is a percentage of the
  // reference, so a quarter-scale reading would not be wrong by a constant -
  // it would quietly change what the percentages mean.
  analogReadResolution(12);

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
  Serial.println(F("ref_mm,live_mm,diff_mm,dist_verdict,missed,ref_ldr,live_ldr,diff_ldr,light_verdict"));
}

void loop() {
  // Booting takes less time than uploading and re-enumerating the USB port, so
  // the report in setup() usually goes nowhere. Print it once more the moment a
  // host actually appears - the result should be observable whenever you attach
  // a monitor, not only if you win a race against the board.
  if (!selfTestReported && Serial) {
    runDecisionSelfTest(Serial);
    Serial.println(F("ref_mm,live_mm,diff_mm,dist_verdict,missed,ref_ldr,live_ldr,diff_ldr,light_verdict"));
    selfTestReported = true;
  }

  if (holdFired()) {
    doSave();
    return;
  }

  Sample s = sense();

  // Until a setup is captured there is nothing to compare against, so both
  // channels stay unread rather than being judged against a made-up reference.
  Verdict vd = V_NO_READ, vl = V_NO_READ;
  if (armed) {
    vd = decideDistance(s.distMm, refMm, prevDist);
    vl = decideLight(lightReadingValid(s.light) ? (float)s.light : -1.0f,
                     (float)refLight, prevLight);
  }

  render(s, vd, vl);
  updateBuzzer(vd, vl);
  updateLed(vd, vl);
  emit(s, vd, vl);

  prevDist  = vd;
  prevLight = vl;
}
