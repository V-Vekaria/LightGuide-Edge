# Distance Guide (Phase 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `firmware/03_inference` so the Nano compares live ultrasonic distance against a held reference and reports CORRECT / FAR / CLOSE on the OLED, the buzzer and the on-board LED.

**Architecture:** Four units. A pure decision function with no Arduino dependencies (`decision.h`), an on-device test harness that exercises it exhaustively (`self_test.h`), and one sketch (`03_inference.ino`) holding sensing, the two-mode state machine and the three output channels. The decision function is isolated on purpose: Phase 2 replaces its body with a TFLite classifier and nothing else moves.

**Tech Stack:** Arduino Nano 33 BLE (nRF52840, `arduino:mbed_nano:nano33ble`), arduino-cli 1.5.1, Adafruit_SSD1306 2.5.17, Adafruit_GFX 1.12.6. No new libraries.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-08-06-distance-guide-design.md`. Every value below is copied from it.
- Board FQBN is `arduino:mbed_nano:nano33ble`. Port is `COM4`.
- Pins, fixed by `docs/02-HARDWARE.md`: TRIG D2, ECHO D3, switch D7 (`INPUT_PULLUP`), buzzer D9, OLED A4/A5 at 0x3C, on-board RGB `LEDR`/`LEDG`/`LEDB` **active LOW**.
- Constants: `DEFAULT_REF_MM = 1000`, `TOL_ENTER_MM = 30`, `TOL_EXIT_MM = 40`, `REF_SAMPLES = 20`, `REF_MIN_VALID = 12`, `HOLD_MS = 2000`.
- Serial is 115200 baud. CSV columns are exactly `mode,ref_mm,live_mm,diff_mm,verdict,missed`.
- **Never use `delay()` in the main loop path.** Blocking the loop to sound a beep makes the display lag the sensor. `delay()` is permitted only inside one-shot, user-initiated actions (saving a reference), where nothing is being tracked.
- Comment style follows the existing sketches: explain *why*, not *what*. Match `firmware/01i_distance_only`.
- No host C++ compiler is installed, so the decision logic is tested **on the board**. `runDecisionSelfTest()` runs at every boot and prints its result over serial.
- Never write `PIN_LED` — the board core already defines it as `(13u)` and the collision produces a misleading compiler error.
- Only one program may hold COM4. Close any serial monitor before compiling or uploading.

---

## File Structure

| File | Responsibility |
|---|---|
| `firmware/03_inference/decision.h` | `Verdict` enum, `decide()`, `verdictName()`. Pure. No Arduino API. |
| `firmware/03_inference/self_test.h` | The decision case table and `runDecisionSelfTest(Print&)`. |
| `firmware/03_inference/03_inference.ino` | Pins, sensing, mode machine, button, OLED, buzzer, LED, serial. |
| `reports/phase1_distance_acceptance.md` | Measured evidence from the §8 test plan. |

`decision.h` and `self_test.h` sit inside the sketch folder, so the Arduino build picks them up from `#include "..."` with no configuration.

---

### Task 1: Pure decision logic, proved by an on-device self-test

The TDD cycle here is real: the first version of `decide()` deliberately ignores its history parameter, the self-test catches exactly the three hysteresis cases that fail because of it, and the fix makes them pass. That proves the test has teeth before anything depends on it.

**Files:**
- Create: `firmware/03_inference/decision.h`
- Create: `firmware/03_inference/self_test.h`
- Create: `firmware/03_inference/03_inference.ino`

**Interfaces:**
- Consumes: nothing.
- Produces: `enum Verdict { V_NO_ECHO, V_CORRECT, V_FAR, V_CLOSE }`; `Verdict decide(float live_mm, float ref_mm, Verdict prev)`; `const char *verdictName(Verdict)`; `int runDecisionSelfTest(Print &out)` returning the failure count.

- [ ] **Step 1: Write `decision.h` — deliberately without hysteresis**

Create `firmware/03_inference/decision.h`:

```c
/*
 * decision.h - the verdict, and nothing else.
 *
 * Deliberately free of every Arduino API: no pins, no Serial, no millis(). That
 * is what lets self_test.h exercise it exhaustively on the board, and it is what
 * makes the Phase 2 swap a change to one function body rather than a rewrite.
 */
#ifndef DECISION_H
#define DECISION_H

#include <math.h>

enum Verdict { V_NO_ECHO = 0, V_CORRECT, V_FAR, V_CLOSE };

const float TOL_ENTER_MM = 30.0f;
const float TOL_EXIT_MM  = 40.0f;

// `prev` is last loop's verdict. Passing it in rather than keeping it in a
// static keeps the function pure, so a test can drive it with any history.
inline Verdict decide(float live_mm, float ref_mm, Verdict prev) {
  if (live_mm < 0.0f) return V_NO_ECHO;

  const float diff = live_mm - ref_mm;
  const float mag  = fabsf(diff);

  if (mag <= TOL_ENTER_MM) return V_CORRECT;
  return (diff > 0.0f) ? V_FAR : V_CLOSE;
}

inline const char *verdictName(Verdict v) {
  switch (v) {
    case V_CORRECT: return "CORRECT";
    case V_FAR:     return "FAR";
    case V_CLOSE:   return "CLOSE";
    default:        return "NO_ECHO";
  }
}

#endif
```

- [ ] **Step 2: Write `self_test.h` with all 18 cases**

Create `firmware/03_inference/self_test.h`:

```c
/*
 * self_test.h - exercises decide() against a table of known-good answers.
 *
 * There is no host C++ compiler on this machine, so the test runs on the board
 * at every boot and reports over serial. That is not a compromise: it proves the
 * logic on the same toolchain and the same float behaviour that ships.
 */
#ifndef SELF_TEST_H
#define SELF_TEST_H

#include "decision.h"

struct DecisionCase {
  float       live_mm;
  float       ref_mm;
  Verdict     prev;
  Verdict     expect;
  const char *why;
};

const DecisionCase DECISION_CASES[] = {
  // Plain classification, with no history worth speaking of.
  { 1000, 1000, V_NO_ECHO, V_CORRECT, "dead on the reference" },
  { 1500, 1000, V_NO_ECHO, V_FAR,     "150 cm against a 100 cm reference" },
  {  700, 1000, V_NO_ECHO, V_CLOSE,   "70 cm against a 100 cm reference" },

  // Edges of the enter band. 30 mm is inside it, 31 mm is not.
  { 1030, 1000, V_FAR,   V_CORRECT, "+30 mm is the last CORRECT" },
  { 1031, 1000, V_FAR,   V_FAR,     "+31 mm is past the enter band" },
  {  970, 1000, V_CLOSE, V_CORRECT, "-30 mm is the last CORRECT" },
  {  969, 1000, V_CLOSE, V_CLOSE,   "-31 mm is past the enter band" },

  // Hysteresis. Same reading, different history, different answer - this pair
  // is the entire point of TOL_EXIT_MM and the reason the buzzer stays steady
  // when the operator parks on the boundary.
  { 1035, 1000, V_CORRECT, V_CORRECT, "+35 mm holds CORRECT once already CORRECT" },
  { 1035, 1000, V_FAR,     V_FAR,     "+35 mm cannot enter CORRECT from FAR" },
  {  965, 1000, V_CORRECT, V_CORRECT, "-35 mm holds CORRECT once already CORRECT" },
  {  965, 1000, V_CLOSE,   V_CLOSE,   "-35 mm cannot enter CORRECT from CLOSE" },

  // Edges of the exit band.
  { 1040, 1000, V_CORRECT, V_CORRECT, "+40 mm is the last hold" },
  { 1041, 1000, V_CORRECT, V_FAR,     "+41 mm finally releases to FAR" },
  {  959, 1000, V_CORRECT, V_CLOSE,   "-41 mm finally releases to CLOSE" },

  // No echo beats every history. A wrong CORRECT is worse than an honest
  // "I cannot tell you".
  {   -1, 1000, V_CORRECT, V_NO_ECHO, "no echo overrides CORRECT" },
  {   -1, 1000, V_FAR,     V_NO_ECHO, "no echo overrides FAR" },

  // A reference other than the default, so nothing is quietly hardcoded to 1000.
  {  800,  800, V_NO_ECHO, V_CORRECT, "a reference of 80 cm behaves the same" },
  {  900,  800, V_NO_ECHO, V_FAR,     "90 cm is FAR of an 80 cm reference" },
};

const int DECISION_CASE_COUNT = sizeof(DECISION_CASES) / sizeof(DECISION_CASES[0]);

// Returns the number of failures. Prints one line per failure, then a summary.
inline int runDecisionSelfTest(Print &out) {
  int failures = 0;

  for (int i = 0; i < DECISION_CASE_COUNT; i++) {
    const DecisionCase &c = DECISION_CASES[i];
    Verdict got = decide(c.live_mm, c.ref_mm, c.prev);
    if (got != c.expect) {
      failures++;
      out.print(F("  FAIL ["));
      out.print(i);
      out.print(F("] "));
      out.print(c.why);
      out.print(F(" - expected "));
      out.print(verdictName(c.expect));
      out.print(F(", got "));
      out.println(verdictName(got));
    }
  }

  out.print(F("DECISION SELF-TEST: "));
  out.print(DECISION_CASE_COUNT - failures);
  out.print('/');
  out.print(DECISION_CASE_COUNT);
  out.println(failures ? F(" passed - FAILURES PRESENT") : F(" passed - OK"));
  return failures;
}

#endif
```

- [ ] **Step 3: Write a minimal `03_inference.ino` that runs only the self-test**

Create `firmware/03_inference/03_inference.ino`:

```c
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

#include "decision.h"
#include "self_test.h"

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  runDecisionSelfTest(Serial);
}

void loop() {
}
```

- [ ] **Step 4: Compile, upload and watch the self-test FAIL**

Run:

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

Then:

```bash
arduino-cli monitor -p COM4 --config baudrate=115200
```

Expected — exactly three failures, because `decide()` currently ignores `prev`:

```
  FAIL [7] +35 mm holds CORRECT once already CORRECT - expected CORRECT, got FAR
  FAIL [9] -35 mm holds CORRECT once already CORRECT - expected CORRECT, got CLOSE
  FAIL [11] +40 mm is the last hold - expected CORRECT, got FAR
DECISION SELF-TEST: 15/18 passed - FAILURES PRESENT
```

If any *other* case fails, stop and fix the table or the logic before continuing — the test is wrong, not the code. Press Ctrl+C to leave the monitor and free COM4.

- [ ] **Step 5: Add hysteresis to `decide()`**

In `firmware/03_inference/decision.h`, replace the body of `decide()`. The two constant declarations above it gain a comment:

```c
// Enter CORRECT within +-30 mm; leave it only beyond +-40 mm. The 10 mm gap is
// hysteresis. With a single threshold, a target parked on the boundary flips the
// verdict several times a second: the display flickers and the buzzer chatters,
// which is the most visible way for a working device to look broken on camera.
const float TOL_ENTER_MM = 30.0f;
const float TOL_EXIT_MM  = 40.0f;

// `prev` is last loop's verdict. Passing it in rather than keeping it in a
// static keeps the function pure, so a test can drive it with any history.
inline Verdict decide(float live_mm, float ref_mm, Verdict prev) {
  if (live_mm < 0.0f) return V_NO_ECHO;

  const float diff = live_mm - ref_mm;
  const float mag  = fabsf(diff);
  const float band = (prev == V_CORRECT) ? TOL_EXIT_MM : TOL_ENTER_MM;

  if (mag <= band) return V_CORRECT;
  return (diff > 0.0f) ? V_FAR : V_CLOSE;
}
```

- [ ] **Step 6: Recompile, upload and watch all 18 pass**

Run:

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

Then:

```bash
arduino-cli monitor -p COM4 --config baudrate=115200
```

Expected, with no FAIL lines above it:

```
DECISION SELF-TEST: 18/18 passed - OK
```

- [ ] **Step 7: Commit**

```bash
git add firmware/03_inference
git commit -m "feat: distance verdict logic with self-test"
```

---

### Task 2: Ultrasonic sensing

Lifted from `firmware/01i_distance_only`, which is already proven on this rig at 0% dropouts. Copy it rather than reinvent it.

**Files:**
- Modify: `firmware/03_inference/03_inference.ino`

**Interfaces:**
- Consumes: nothing from Task 1 yet.
- Produces: `float readDistanceMm(int &missed)` — returns millimetres, or `-1.0f` when every ping in the group failed; sets `missed` to the number of failed pings out of `ULTRA_SAMPLES`.

- [ ] **Step 1: Add the pins, constants and sensing functions**

In `firmware/03_inference/03_inference.ino`, insert after the `#include "self_test.h"` line:

```c
// ---- pins (docs/02-HARDWARE.md) ----------------------------------------
const int PIN_TRIG   = 2;
const int PIN_ECHO   = 3;

// ---- ultrasonic ---------------------------------------------------------
const int ULTRA_SAMPLES = 5;
const int PING_GAP_MS   = 10;
const unsigned long ECHO_START_TIMEOUT_US = 15000UL;
const unsigned long ECHO_HIGH_MAX_US      = 25000UL;
const float DIST_MIN_MM = 20.0f;
const float DIST_MAX_MM = 4000.0f;

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
```

- [ ] **Step 2: Initialise the pins and stream raw readings**

Replace `setup()` and `loop()` in `firmware/03_inference/03_inference.ino` with:

```c
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  runDecisionSelfTest(Serial);
  Serial.println(F("live_mm,missed"));
}

void loop() {
  int missed = 0;
  float liveMm = readDistanceMm(missed);

  Serial.print(liveMm, 0);
  Serial.print(',');
  Serial.println(missed);
}
```

- [ ] **Step 3: Upload and check the readings against a tape measure**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

```bash
arduino-cli monitor -p COM4 --config baudrate=115200
```

Expected: `DECISION SELF-TEST: 18/18 passed - OK`, then a stream of readings. Hold a flat target at a tape-measured 100 cm — values should sit near `1000` with `missed` at 0 or 1. Point the sensor at open air and confirm `-1`.

**If readings are wildly wrong or always `-1`:** the wiring is the first suspect, not this code. Flash `firmware/00_wiring_probe`, which distinguishes miswired from unpowered.

- [ ] **Step 4: Commit**

```bash
git add firmware/03_inference/03_inference.ino
git commit -m "feat: add ultrasonic sensing to inference sketch"
```

---

### Task 3: Verdict over serial, in GUIDE mode

No display and no sound yet. Getting the verdict correct on serial first means that when the OLED lands, any fault is in the rendering rather than the logic.

**Files:**
- Modify: `firmware/03_inference/03_inference.ino`

**Interfaces:**
- Consumes: `decide()`, `verdictName()` from Task 1; `readDistanceMm()` from Task 2.
- Produces: globals `Mode mode`, `float refMm`, `Verdict prevVerdict`; `void emit(float liveMm, Verdict v, int missed)`.

- [ ] **Step 1: Add the mode enum, state and reference constants**

In `firmware/03_inference/03_inference.ino`, insert after the ultrasonic constants block:

```c
// ---- reference ----------------------------------------------------------
// 100 cm is the operator's established working distance, so the device is
// useful the instant it powers up and a faulty switch cannot leave it unusable.
const float DEFAULT_REF_MM = 1000.0f;

enum Mode { MODE_GUIDE, MODE_SETUP };

Mode    mode        = MODE_GUIDE;
float   refMm       = DEFAULT_REF_MM;
Verdict prevVerdict = V_NO_ECHO;
```

- [ ] **Step 2: Add the serial emitter**

Insert after `readDistanceMm()`:

```c
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
```

- [ ] **Step 3: Wire the decision into the loop**

Replace `setup()` and `loop()` with:

```c
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  runDecisionSelfTest(Serial);
  Serial.println(F("mode,ref_mm,live_mm,diff_mm,verdict,missed"));
}

void loop() {
  int missed = 0;
  float liveMm = readDistanceMm(missed);
  Verdict v = decide(liveMm, refMm, prevVerdict);

  emit(liveMm, v, missed);
  prevVerdict = v;
}
```

- [ ] **Step 4: Upload and verify all three verdicts on hardware**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

```bash
arduino-cli monitor -p COM4 --config baudrate=115200
```

Check each of these against a tape measure:

| Target at | Expected verdict column |
|---|---|
| 100 cm | `CORRECT` |
| 150 cm | `FAR` |
| 70 cm | `CLOSE` |
| sensor covered | `NO_ECHO`, `live_mm` of `-1`, `diff_mm` of `NA` |

Then the hysteresis check that matters: hold the target steady at **103 cm** for 15 seconds. The verdict column must stay on one value for the whole time. Occasional single-row flips caused by real sensor noise are acceptable; alternating every row is not, and means `prevVerdict` is not being fed back.

- [ ] **Step 5: Commit**

```bash
git add firmware/03_inference/03_inference.ino
git commit -m "feat: report distance verdict over serial"
```

---

### Task 4: OLED display

**Files:**
- Modify: `firmware/03_inference/03_inference.ino`

**Interfaces:**
- Consumes: `mode`, `refMm`, `verdictName()`, `readDistanceMm()`.
- Produces: `void printCentred(const char *s, int size, int y)`, `void drawBar(float liveMm)`, `void render(float liveMm, Verdict v, int missed)`.

- [ ] **Step 1: Add the display includes and object**

At the top of `firmware/03_inference/03_inference.ino`, above `#include "decision.h"`:

```c
#include <string.h>          // strlen, used by printCentred
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
```

And with the other globals:

```c
Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledOk = false;
```

- [ ] **Step 2: Add the rendering functions**

Insert before `setup()`:

```c
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
```

- [ ] **Step 3: Initialise the OLED and call `render()`**

In `setup()`, after `digitalWrite(PIN_TRIG, LOW);`:

```c
  Wire.begin();
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledOk) display.setTextColor(SSD1306_WHITE);
  else        Serial.println(F("# OLED not found at 0x3C - continuing without it"));
```

In `loop()`, add the render call immediately before `emit(...)`:

```c
  render(liveMm, v, missed);
```

- [ ] **Step 4: Upload and check the screen**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

Look at the panel, not the serial output. Confirm all four:
- At 100 cm: `REF 100cm  NOW 100cm`, the word `CORRECT` large and centred and **not clipped at either edge**, `+0cm hold it`, and the bar marker sitting on the centre tick.
- At 150 cm: `FAR`, `+50cm move closer`, marker pinned to the right end.
- At 70 cm: `CLOSE`, `-30cm move back`, marker pinned to the left end.
- Covered: `NO ECHO`, `aim at target`, `NOW --`, no marker.

If `CORRECT` clips, change only the `printCentred(..., 3, 18)` call to size `2` and re-upload.

- [ ] **Step 5: Commit**

```bash
git add firmware/03_inference/03_inference.ino
git commit -m "feat: OLED verdict display"
```

---

### Task 5: Buzzer and status LED

**Files:**
- Modify: `firmware/03_inference/03_inference.ino`

**Interfaces:**
- Consumes: `Verdict`.
- Produces: `void updateBuzzer(Verdict v, bool verdictChanged)`, `void updateLed(Verdict v)`, `void silenceBuzzer()`.

- [ ] **Step 1: Add the buzzer pin and its state**

With the other pin constants:

```c
const int PIN_SWITCH = 7;
const int PIN_BUZZ   = 9;
```

With the other globals:

```c
bool          buzzerOn         = false;
unsigned long buzzerPhaseStart = 0;
bool          correctChirpDone = false;
```

- [ ] **Step 2: Add the sound and light functions**

Insert before `setup()`:

```c
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
```

- [ ] **Step 3: Initialise the pins and call both from the loop**

In `setup()`, after the OLED block:

```c
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(LEDR, OUTPUT); pinMode(LEDG, OUTPUT); pinMode(LEDB, OUTPUT);
  digitalWrite(LEDR, HIGH); digitalWrite(LEDG, HIGH); digitalWrite(LEDB, HIGH);
```

Replace `loop()` with:

```c
void loop() {
  int missed = 0;
  float liveMm = readDistanceMm(missed);
  Verdict v = decide(liveMm, refMm, prevVerdict);

  render(liveMm, v, missed);
  updateBuzzer(v, v != prevVerdict);
  updateLed(v);
  emit(liveMm, v, missed);

  prevVerdict = v;
}
```

- [ ] **Step 4: Upload and verify by ear**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

| Target at | Expected sound | Expected LED |
|---|---|---|
| 150 cm | slow low beeps, roughly one every 0.6 s | red |
| 70 cm | fast high beeps, roughly five a second | red |
| 100 cm | **one** short high chirp on arrival, then silence | green |
| covered | silence | off |

Two things specifically to confirm, because they are the failure modes:
- Sitting at 100 cm for 30 s produces **no** repeat chirp.
- Moving away and returning to 100 cm produces the chirp **again** — that proves `correctChirpDone` is reset on the transition rather than latched forever.

- [ ] **Step 5: Commit**

```bash
git add firmware/03_inference/03_inference.ino
git commit -m "feat: buzzer and status LED feedback"
```

---

### Task 6: Button, SETUP mode and reference capture

**Files:**
- Modify: `firmware/03_inference/03_inference.ino`

**Interfaces:**
- Consumes: `readDistanceMm()`, `silenceBuzzer()`, `mode`, `refMm`, `prevVerdict`.
- Produces: `enum BtnEvent { BTN_NONE, BTN_SHORT, BTN_HOLD }`, `BtnEvent pollButton()`, `void setMode(Mode m)`, `float captureReference()`, `void doSave()`, `void renderSetup(float liveMm)`.

- [ ] **Step 1: Add the button and reference constants**

With the other constants:

```c
// ---- reference capture --------------------------------------------------
const int REF_SAMPLES   = 20;
const int REF_MIN_VALID = 12;

// ---- button -------------------------------------------------------------
const unsigned long HOLD_MS        = 2000;
const unsigned long DEBOUNCE_MS    = 30;
const unsigned long STUCK_CHECK_MS = 3000;

enum BtnEvent { BTN_NONE, BTN_SHORT, BTN_HOLD };
```

With the other globals:

```c
bool switchFaulty = false;
```

- [ ] **Step 2: Add the button poller**

Insert before `setup()`:

```c
// One button, one gesture per mode. Non-blocking, because the loop must keep
// reading the sensor while the operator is holding the button down.
BtnEvent pollButton() {
  static bool          wasDown   = false;
  static unsigned long downAt    = 0;
  static bool          holdFired = false;

  if (switchFaulty) return BTN_NONE;

  const bool down = (digitalRead(PIN_SWITCH) == LOW);
  const unsigned long now = millis();

  if (down && !wasDown) {
    wasDown = true;
    downAt = now;
    holdFired = false;
  } else if (down && !holdFired && (now - downAt) >= HOLD_MS) {
    // Fires while the button is still held, so the operator gets feedback at
    // two seconds rather than on release.
    holdFired = true;
    return BTN_HOLD;
  } else if (!down && wasDown) {
    wasDown = false;
    const unsigned long held = now - downAt;
    if (!holdFired && held >= DEBOUNCE_MS) return BTN_SHORT;
  }

  return BTN_NONE;
}
```

- [ ] **Step 3: Add mode switching, capture and the SETUP screen**

Insert after `pollButton()`:

```c
void setMode(Mode m) {
  mode = m;
  silenceBuzzer();
  // Reset the history so returning to GUIDE re-earns its verdict from scratch,
  // and CORRECT chirps again rather than arriving silently.
  prevVerdict = V_NO_ECHO;
}

void renderSetup(float liveMm) {
  if (!oledOk) return;
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SETUP  (ref "));
  display.print(refMm / 10.0f, 0);
  display.print(F("cm)"));

  display.setTextSize(4);
  display.setCursor(0, 14);
  if (liveMm > 0) display.print(liveMm / 10.0f, 0); else display.print(F("--"));
  display.setTextSize(2);
  display.setCursor(90, 26);
  display.print(F("cm"));

  display.setTextSize(1);
  display.setCursor(0, 48);
  display.print(F("hold 2s = save"));
  display.setCursor(0, 56);
  display.print(F("tap = cancel"));

  display.display();
}

// Twenty medians, medianed again. Returns -1 if too few came back valid: a
// reference captured from a bad echo would corrupt every later verdict while
// looking exactly like a software fault, so it must fail at the point of capture.
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
  float r = captureReference();

  if (r < 0.0f) {
    Serial.println(F("# SAVE REFUSED - not enough valid echoes"));
    tone(PIN_BUZZ, 200); delay(400); noTone(PIN_BUZZ);
    if (oledOk) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 12);
      display.print(F("SAVE"));
      display.setCursor(0, 32);
      display.print(F("FAILED"));
      display.setTextSize(1);
      display.setCursor(0, 54);
      display.print(F("aim at target, retry"));
      display.display();
      delay(1800);
    }
    return;                       // stay in SETUP
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
    display.setCursor(0, 16);
    display.print(F("SAVED"));
    display.setTextSize(3);
    display.setCursor(0, 36);
    display.print(refMm / 10.0f, 0);
    display.setTextSize(1);
    display.setCursor(96, 46);
    display.print(F("cm"));
    display.display();
    delay(1400);
  }

  setMode(MODE_GUIDE);
}
```

- [ ] **Step 4: Detect a stuck switch at boot**

In `setup()`, after the LED initialisation:

```c
  pinMode(PIN_SWITCH, INPUT_PULLUP);

  // A switch held LOW forever would spam mode changes and make the device
  // unusable. A healthy released switch reads HIGH and leaves this immediately;
  // only a genuinely stuck one costs the full three seconds.
  unsigned long t0 = millis();
  switchFaulty = true;
  while (millis() - t0 < STUCK_CHECK_MS) {
    if (digitalRead(PIN_SWITCH) == HIGH) { switchFaulty = false; break; }
    delay(10);
  }
  if (switchFaulty) Serial.println(F("# switch stuck LOW - disabled for this run"));
```

- [ ] **Step 5: Handle the button events in the loop**

Replace `loop()` with:

```c
void loop() {
  BtnEvent ev = pollButton();
  if (ev == BTN_SHORT) {
    setMode(mode == MODE_GUIDE ? MODE_SETUP : MODE_GUIDE);
    return;
  }
  if (ev == BTN_HOLD && mode == MODE_SETUP) {
    doSave();
    return;
  }

  int missed = 0;
  float liveMm = readDistanceMm(missed);
  Verdict v = decide(liveMm, refMm, prevVerdict);

  if (mode == MODE_GUIDE) {
    render(liveMm, v, missed);
    updateBuzzer(v, v != prevVerdict);
    updateLed(v);
    prevVerdict = v;
  } else {
    renderSetup(liveMm);
  }

  emit(liveMm, v, missed);
}
```

- [ ] **Step 6: Upload and test the whole button flow**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

```bash
arduino-cli monitor -p COM4 --config baudrate=115200
```

Work through all five:
1. **Enter SETUP** — tap the button in GUIDE. Screen switches to the big live number, buzzer goes quiet, CSV `mode` column reads `SETUP`.
2. **Cancel** — tap again. Back to GUIDE with `REF` unchanged.
3. **Save** — in SETUP, aim at a target at 80 cm, hold 2 s. Progress screen appears, two rising chirps, `SAVED 80`, then GUIDE showing `REF 80cm`. Serial prints `# REFERENCE SAVED`.
4. **New reference works** — 80 cm now reads `CORRECT`; 100 cm now reads `FAR`.
5. **Refused save** — enter SETUP aimed at open air, hold 2 s. Low error tone, `SAVE FAILED`, mode stays SETUP, serial prints `# SAVE REFUSED`. Then tap to cancel and confirm `REF` is still 80 cm.

- [ ] **Step 7: Commit**

```bash
git add firmware/03_inference/03_inference.ino
git commit -m "feat: reference capture via button"
```

---

### Task 7: Acceptance run and recorded evidence

The project convention is that a gate is a binary fact with evidence attached. This task produces the evidence.

**Files:**
- Create: `reports/phase1_distance_acceptance.md`
- Modify: `RUN.md`

**Interfaces:**
- Consumes: the finished sketch.
- Produces: nothing consumed by later tasks.

- [ ] **Step 1: Run the full §8 test plan and record measured values**

Execute all eight tests from the spec's §8 against a tape measure, writing down the actual numbers as you go — not "passed", but what the screen and the serial log said.

```bash
arduino-cli monitor -p COM4 --config baudrate=115200
```

- [ ] **Step 2: Write the report**

Create `reports/phase1_distance_acceptance.md` using this structure, replacing every bracketed field with a measured value:

```markdown
# Phase 1 — distance guide acceptance

**Date:** [date]
**Sketch:** `firmware/03_inference` at commit [short sha]
**Board:** Arduino Nano 33 BLE on COM4
**Reference instrument:** tape measure
**Spec:** `docs/superpowers/specs/2026-08-06-distance-guide-design.md` §8

## Self-test

`DECISION SELF-TEST: [n]/18 passed`

## Results

| # | Test | Expected | Observed | Pass |
|---|---|---|---|---|
| 1 | Boot baseline | GUIDE, REF 100cm | [what appeared] | [Y/N] |
| 2 | 100 cm | CORRECT, green, one chirp | [live_mm read, LED, sound] | [Y/N] |
| 2 | 150 cm | FAR, red, slow low beeps | [live_mm read] | [Y/N] |
| 2 | 70 cm | CLOSE, red, fast high beeps | [live_mm read] | [Y/N] |
| 3 | 103 cm / 97 cm | last CORRECT positions | [live_mm read, verdict] | [Y/N] |
| 3 | 105 cm / 95 cm | not CORRECT | [live_mm read, verdict] | [Y/N] |
| 4 | Hysteresis, 15 s at 103 cm | no flicker, no chatter | [flips counted in the log] | [Y/N] |
| 5 | Sensor covered | NO ECHO, silent, recovers | [what appeared] | [Y/N] |
| 6 | Re-reference to 80 cm | REF 80cm, 80 reads CORRECT | [saved value] | [Y/N] |
| 7 | Refused save at open air | error tone, stays SETUP | [what appeared] | [Y/N] |
| 8 | 5 min endurance | no reset, no stuck tone | [rows logged, resets seen] | [Y/N] |

## Measured

- Sketch size: [flash %] flash, [RAM %] RAM (from the compile output)
- Loop rate: [rows/s from the serial log]
- Miss rate at 100 cm: [total missed / total pings over 60 s]

## Limitations observed

[Anything that behaved worse than the spec assumed — beam angle catching a
nearer object, jitter at longer range, anything else. This section is worth
marks; the Demo rubric row explicitly rewards understanding of limitations.]
```

- [ ] **Step 3: Add the sketch to the RUN.md index**

In `RUN.md`, in the firmware index table, replace the `03_inference` row with:

```markdown
| `03_inference` | **The product.** Distance guide: CORRECT / FAR / CLOSE against a saved reference. Phase 1 uses thresholds; the classifier replaces `decide()` in Phase 2. |
```

- [ ] **Step 4: Commit**

```bash
git add reports/phase1_distance_acceptance.md RUN.md
git commit -m "test: phase 1 distance acceptance evidence"
```

---

## Coverage against the spec

| Spec section | Task |
|---|---|
| §3.1 Sensing | 2 |
| §3.2 Modes, capture, refusal | 6 |
| §4 Decision function, constants, hysteresis | 1 |
| §5.1 Buzzer | 5 |
| §5.2 OLED | 4 |
| §5.3 RGB LED | 5 |
| §6 Serial CSV | 3 |
| §7 Error handling — no echo | 3, 4, 5 |
| §7 Error handling — refused save | 6 |
| §7 Error handling — OLED absent | 4 |
| §7 Error handling — stuck switch | 6 |
| §8 Test plan | 7 |
| §10 Definition of done | 7 |
