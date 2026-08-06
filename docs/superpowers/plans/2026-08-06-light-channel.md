# Light Channel (Phase 2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the light channel to `firmware/03_inference` so the device checks both distance and brightness against one saved setup, reporting each on the OLED, the buzzer and the LED.

**Architecture:** The LDR is sampled inside the dead time the ultrasonic already spends between pings, so the light channel costs no loop rate. The existing pure `decide()` gains explicit band arguments, letting distance use fixed millimetres and light use a percentage of its reference through two thin wrappers. `DEFAULT_REF_MM` is removed: the device is either `UNSET` or `ARMED`.

**Tech Stack:** Arduino Nano 33 BLE (`arduino:mbed_nano:nano33ble`), arduino-cli 1.5.1, Adafruit_SSD1306 2.5.17, Adafruit_GFX 1.12.6. No new libraries.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-08-06-light-channel-design.md`. Every value below is copied from it.
- FQBN `arduino:mbed_nano:nano33ble`, port `COM4`, serial 115200.
- Pins: TRIG D2, ECHO D3, **LDR A0**, switch D7 (`INPUT_PULLUP`), buzzer D9, OLED A4/A5 at 0x3C, on-board RGB `LEDR`/`LEDG`/`LEDB` **active LOW**.
- ADC must run at 12-bit — `analogReadResolution(12)` in `setup()`. Without it the LDR reads 0–1023 and every band is wrong by 4×.
- Constants: `TOL_ENTER_MM = 30`, `TOL_EXIT_MM = 40`, `LIGHT_TOL_ENTER_PCT = 0.05`, `LIGHT_TOL_EXIT_PCT = 0.07`, `LIGHT_MIN_VALID = 50`, `LIGHT_MAX_VALID = 4045`, `REF_SAMPLES = 20`, `REF_MIN_VALID = 12`, `HOLD_MS = 2000`.
- CSV columns exactly: `ref_mm,live_mm,diff_mm,dist_verdict,missed,ref_ldr,live_ldr,diff_ldr,light_verdict`.
- **No `delay()` in the loop path.** Permitted only inside `doSave()`, a one-shot user action.
- **Loop rate must stay within 10% of 7.8 Hz.** Measure it; do not assume.
- Never write `PIN_LED` — the core defines it as `(13u)` and the collision gives a misleading error.
- Comments explain *why*, not *what*. Match the existing sketches.
- Capture serial with `powershell -ExecutionPolicy Bypass -File tools/read_serial.ps1 -Seconds N`. `arduino-cli monitor` is interactive and cannot be scripted.
- Only one program may hold COM4.

---

## File Structure

| File | Responsibility | Change |
|---|---|---|
| `firmware/03_inference/decision.h` | `Verdict` enum, generic `decide()`, the two channel wrappers, validity check, word tables. Pure — no Arduino API. | rewritten |
| `firmware/03_inference/self_test.h` | Three case tables (distance, light, validity) and the on-device runner. | rewritten |
| `firmware/03_inference/03_inference.ino` | Pins, dual sensing, `UNSET`/`ARMED` state, capture, OLED, buzzer, LED, serial. | extended |
| `reports/phase2_light_acceptance.md` | Measured evidence. | created |

---

### Task 1: Rename the verdict enum and generalise `decide()`

A refactor, not a feature. There is no red-green cycle here — the gate is that **all 18 existing distance cases still pass unchanged in meaning** after the rename and signature change. If any fail, the refactor broke the working channel and must be fixed before anything is built on top.

**Files:**
- Modify: `firmware/03_inference/decision.h`
- Modify: `firmware/03_inference/self_test.h`
- Modify: `firmware/03_inference/03_inference.ino`

**Interfaces:**
- Consumes: nothing.
- Produces: `enum Verdict { V_NO_READ, V_CORRECT, V_ABOVE, V_BELOW }`; `Verdict decide(float live, float ref, Verdict prev, float enterBand, float exitBand)`; `Verdict decideDistance(float liveMm, float refMm, Verdict prev)`; `Verdict decideLight(float live, float ref, Verdict prev)`; `bool lightReadingValid(int counts)`; `const char *distanceWord(Verdict)`; `const char *lightWord(Verdict)`.

- [ ] **Step 1: Rewrite `decision.h`**

Replace the whole of `firmware/03_inference/decision.h`:

```c
/*
 * decision.h - the verdict, and nothing else.
 *
 * Deliberately free of every Arduino API: no pins, no Serial, no millis(). That
 * is what lets self_test.h exercise it exhaustively on the board, and it is what
 * makes the Phase 3 swap a change to one function body rather than a rewrite.
 */
#ifndef DECISION_H
#define DECISION_H

#include <math.h>

// Channel-neutral on purpose. V_ABOVE means the live reading sits above the
// reference; each channel renders that in its own words, because "FAR" is
// meaningless for brightness and "BRIGHT" is meaningless for distance.
enum Verdict { V_NO_READ = 0, V_CORRECT, V_ABOVE, V_BELOW };

// Distance: fixed bands. Enter CORRECT within +-30 mm, leave it only beyond
// +-40 mm. The 10 mm gap is hysteresis - with a single threshold, a target
// parked on the boundary flips the verdict several times a second.
const float TOL_ENTER_MM = 30.0f;
const float TOL_EXIT_MM  = 40.0f;

// Light: proportional bands. A fixed count band would be 3% of a bright
// reference and 15% of a dim one, fussy in one setup and sloppy in another for
// no principled reason. 5% of a typical 2000-count reference is +-100, about
// 17x the measured +-6 noise floor and well inside the ~200-count gap measured
// between adjacent dimmer settings.
const float LIGHT_TOL_ENTER_PCT = 0.05f;
const float LIGHT_TOL_EXIT_PCT  = 0.07f;

// A reading pinned near either rail means a clipped divider, not a dark or
// bright room. This is the exact fault that made the LDR look dead on 6 August.
const int LIGHT_MIN_VALID = 50;
const int LIGHT_MAX_VALID = 4045;

inline bool lightReadingValid(int counts) {
  return counts > LIGHT_MIN_VALID && counts < LIGHT_MAX_VALID;
}

// `prev` is last loop's verdict for this channel. Passing it in rather than
// keeping it in a static keeps the function pure, so a test can drive it with
// any history. A negative `live` means the channel could not be read at all.
inline Verdict decide(float live, float ref, Verdict prev,
                      float enterBand, float exitBand) {
  if (live < 0.0f) return V_NO_READ;

  const float diff = live - ref;
  const float mag  = fabsf(diff);
  const float band = (prev == V_CORRECT) ? exitBand : enterBand;

  if (mag <= band) return V_CORRECT;
  return (diff > 0.0f) ? V_ABOVE : V_BELOW;
}

inline Verdict decideDistance(float liveMm, float refMm, Verdict prev) {
  return decide(liveMm, refMm, prev, TOL_ENTER_MM, TOL_EXIT_MM);
}

inline Verdict decideLight(float live, float ref, Verdict prev) {
  return decide(live, ref, prev,
                ref * LIGHT_TOL_ENTER_PCT,
                ref * LIGHT_TOL_EXIT_PCT);
}

inline const char *distanceWord(Verdict v) {
  switch (v) {
    case V_CORRECT: return "CORRECT";
    case V_ABOVE:   return "FAR";
    case V_BELOW:   return "CLOSE";
    default:        return "NO ECHO";
  }
}

inline const char *lightWord(Verdict v) {
  switch (v) {
    case V_CORRECT: return "CORRECT";
    case V_ABOVE:   return "BRIGHT";
    case V_BELOW:   return "DARK";
    default:        return "NO READ";
  }
}

#endif
```

- [ ] **Step 2: Migrate the 18 distance cases in `self_test.h`**

In `firmware/03_inference/self_test.h`, the case struct and table keep the same 18 entries with the same numbers — only the enum names change (`V_NO_ECHO`→`V_NO_READ`, `V_FAR`→`V_ABOVE`, `V_CLOSE`→`V_BELOW`), and the runner calls `decideDistance`. Replace the struct, table and runner:

```c
struct DecisionCase {
  float       live;
  float       ref;
  Verdict     prev;
  Verdict     expect;
  const char *why;
};

const DecisionCase DISTANCE_CASES[] = {
  // Plain classification, with no history worth speaking of.
  { 1000, 1000, V_NO_READ, V_CORRECT, "dead on the reference" },
  { 1500, 1000, V_NO_READ, V_ABOVE,   "150 cm against a 100 cm reference" },
  {  700, 1000, V_NO_READ, V_BELOW,   "70 cm against a 100 cm reference" },

  // Edges of the enter band. 30 mm is inside it, 31 mm is not.
  { 1030, 1000, V_ABOVE, V_CORRECT, "+30 mm is the last CORRECT" },
  { 1031, 1000, V_ABOVE, V_ABOVE,   "+31 mm is past the enter band" },
  {  970, 1000, V_BELOW, V_CORRECT, "-30 mm is the last CORRECT" },
  {  969, 1000, V_BELOW, V_BELOW,   "-31 mm is past the enter band" },

  // Hysteresis. Same reading, different history, different answer.
  { 1035, 1000, V_CORRECT, V_CORRECT, "+35 mm holds CORRECT once already CORRECT" },
  { 1035, 1000, V_ABOVE,   V_ABOVE,   "+35 mm cannot enter CORRECT from FAR" },
  {  965, 1000, V_CORRECT, V_CORRECT, "-35 mm holds CORRECT once already CORRECT" },
  {  965, 1000, V_BELOW,   V_BELOW,   "-35 mm cannot enter CORRECT from CLOSE" },

  // Edges of the exit band.
  { 1040, 1000, V_CORRECT, V_CORRECT, "+40 mm is the last hold" },
  { 1041, 1000, V_CORRECT, V_ABOVE,   "+41 mm finally releases to FAR" },
  {  959, 1000, V_CORRECT, V_BELOW,   "-41 mm finally releases to CLOSE" },

  // No reading beats every history.
  {   -1, 1000, V_CORRECT, V_NO_READ, "no echo overrides CORRECT" },
  {   -1, 1000, V_ABOVE,   V_NO_READ, "no echo overrides FAR" },

  // A reference other than the default, so nothing is hardcoded to 1000.
  {  800,  800, V_NO_READ, V_CORRECT, "a reference of 80 cm behaves the same" },
  {  900,  800, V_NO_READ, V_ABOVE,   "90 cm is FAR of an 80 cm reference" },
};

const int DISTANCE_CASE_COUNT = sizeof(DISTANCE_CASES) / sizeof(DISTANCE_CASES[0]);

inline int runDistanceCases(Print &out) {
  int failures = 0;
  for (int i = 0; i < DISTANCE_CASE_COUNT; i++) {
    const DecisionCase &c = DISTANCE_CASES[i];
    Verdict got = decideDistance(c.live, c.ref, c.prev);
    if (got != c.expect) {
      failures++;
      out.print(F("  FAIL dist["));   out.print(i);
      out.print(F("] "));             out.print(c.why);
      out.print(F(" - expected "));   out.print(distanceWord(c.expect));
      out.print(F(", got "));         out.println(distanceWord(got));
    }
  }
  return failures;
}

inline int runDecisionSelfTest(Print &out) {
  int failures = runDistanceCases(out);
  int total    = DISTANCE_CASE_COUNT;

  out.print(F("DECISION SELF-TEST: "));
  out.print(total - failures);
  out.print('/');
  out.print(total);
  out.println(failures ? F(" passed - FAILURES PRESENT") : F(" passed - OK"));
  return failures;
}
```

- [ ] **Step 3: Update the two call sites in the sketch**

In `firmware/03_inference/03_inference.ino`, `decide(liveMm, refMm, prevVerdict)` becomes `decideDistance(...)`, and `verdictName` becomes `distanceWord`. Three edits:

In `emit()`:
```c
  Serial.print(distanceWord(v));
```

In `render()`:
```c
  printCentred(v == V_NO_READ ? "NO ECHO" : distanceWord(v), 3, 18);
```

In `loop()`:
```c
  Verdict v = decideDistance(liveMm, refMm, prevVerdict);
```

Then replace every remaining `V_NO_ECHO` with `V_NO_READ`, `V_FAR` with `V_ABOVE`, and `V_CLOSE` with `V_BELOW` throughout the file — they appear in `render()`, `updateBuzzer()`, `updateLed()`, `setMode`-equivalent resets and `doSave()`.

- [ ] **Step 4: Upload and confirm the regression gate**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

```bash
powershell -ExecutionPolicy Bypass -File tools/read_serial.ps1 -Seconds 6
```

Expected, with no FAIL lines:

```
DECISION SELF-TEST: 18/18 passed - OK
```

**If any case fails, stop.** The rename has changed behaviour, which it must not. Compare the failing case against the table above before continuing.

Also confirm on the rig that the distance channel behaves exactly as before — move the target and check FAR / CLOSE / CORRECT, the beeps and the LED are unchanged.

- [ ] **Step 5: Commit**

```bash
git add firmware/03_inference
git commit -m "refactor: channel-neutral verdicts, banded decide"
```

---

### Task 2: Light decision logic, test-first

Genuine red-green: the light cases call `decideLight()` and `lightReadingValid()`, which Task 1 added to `decision.h`. To prove the tests can fail, they are written and run against a deliberately wrong band constant first.

**Files:**
- Modify: `firmware/03_inference/self_test.h`
- Modify: `firmware/03_inference/decision.h`

**Interfaces:**
- Consumes: `decideLight`, `lightReadingValid`, `lightWord` from Task 1.
- Produces: `runLightCases(Print&)`, `runValidityCases(Print&)`, both returning a failure count and both called by `runDecisionSelfTest`.

- [ ] **Step 1: Break the light band on purpose**

In `firmware/03_inference/decision.h`, temporarily set:

```c
const float LIGHT_TOL_ENTER_PCT = 0.10f;
```

This is a scaffold to prove the new tests have teeth. Step 5 puts it back.

- [ ] **Step 2: Add the light and validity cases to `self_test.h`**

Insert after `runDistanceCases()`:

```c
// Light uses percentage bands, so the same absolute deviation is CORRECT at a
// bright reference and not at a dim one. Cases at 650, 2000 and 3400 counts
// cover the range the LDR actually produced during the 6 August sweep.
const DecisionCase LIGHT_CASES[] = {
  // ref 2000 -> enter +-100, exit +-140
  { 2000, 2000, V_NO_READ, V_CORRECT, "dead on the reference" },
  { 2100, 2000, V_NO_READ, V_CORRECT, "+100 is the last CORRECT at ref 2000" },
  { 2101, 2000, V_NO_READ, V_ABOVE,   "+101 is BRIGHT at ref 2000" },
  { 1900, 2000, V_NO_READ, V_CORRECT, "-100 is the last CORRECT at ref 2000" },
  { 1899, 2000, V_NO_READ, V_BELOW,   "-101 is DARK at ref 2000" },

  // Hysteresis: same reading, different history, different answer.
  { 2130, 2000, V_CORRECT, V_CORRECT, "+130 holds CORRECT once already CORRECT" },
  { 2130, 2000, V_ABOVE,   V_ABOVE,   "+130 cannot enter CORRECT from BRIGHT" },
  { 2141, 2000, V_CORRECT, V_ABOVE,   "+141 finally releases to BRIGHT" },
  { 1859, 2000, V_CORRECT, V_BELOW,   "-141 finally releases to DARK" },

  // The whole point of proportional bands: 150 counts is fine at a bright
  // reference and 40 counts is not at a dim one.
  {  680,  650, V_NO_READ, V_CORRECT, "+30 is CORRECT at ref 650 (band 32.5)" },
  {  690,  650, V_NO_READ, V_ABOVE,   "+40 is BRIGHT at ref 650 (band 32.5)" },
  { 3550, 3400, V_NO_READ, V_CORRECT, "+150 is CORRECT at ref 3400 (band 170)" },
  { 3600, 3400, V_NO_READ, V_ABOVE,   "+200 is BRIGHT at ref 3400 (band 170)" },

  // An unreadable channel beats every history.
  {   -1, 2000, V_CORRECT, V_NO_READ, "clipped reading overrides CORRECT" },
};

const int LIGHT_CASE_COUNT = sizeof(LIGHT_CASES) / sizeof(LIGHT_CASES[0]);

inline int runLightCases(Print &out) {
  int failures = 0;
  for (int i = 0; i < LIGHT_CASE_COUNT; i++) {
    const DecisionCase &c = LIGHT_CASES[i];
    Verdict got = decideLight(c.live, c.ref, c.prev);
    if (got != c.expect) {
      failures++;
      out.print(F("  FAIL light[")); out.print(i);
      out.print(F("] "));            out.print(c.why);
      out.print(F(" - expected "));  out.print(lightWord(c.expect));
      out.print(F(", got "));        out.println(lightWord(got));
    }
  }
  return failures;
}

struct ValidityCase { int counts; bool expect; const char *why; };

// A clipped rail is a wiring fault, not a light level. Reporting 4095 as
// "very bright" would have hidden the 1k-pulldown fault instead of exposing it.
const ValidityCase VALIDITY_CASES[] = {
  {    0, false, "0 counts is a shorted or dead divider" },
  {   50, false, "50 is the exclusive lower bound" },
  {   51, true,  "51 is the first accepted reading" },
  { 2000, true,  "a normal mid-scale reading" },
  { 4044, true,  "4044 is the last accepted reading" },
  { 4045, false, "4045 is the exclusive upper bound" },
  { 4095, false, "4095 counts is a rail, not a room" },
};

const int VALIDITY_CASE_COUNT = sizeof(VALIDITY_CASES) / sizeof(VALIDITY_CASES[0]);

inline int runValidityCases(Print &out) {
  int failures = 0;
  for (int i = 0; i < VALIDITY_CASE_COUNT; i++) {
    const ValidityCase &c = VALIDITY_CASES[i];
    bool got = lightReadingValid(c.counts);
    if (got != c.expect) {
      failures++;
      out.print(F("  FAIL valid[")); out.print(i);
      out.print(F("] "));            out.print(c.why);
      out.print(F(" - expected "));  out.print(c.expect ? F("valid") : F("invalid"));
      out.print(F(", got "));        out.println(got ? F("valid") : F("invalid"));
    }
  }
  return failures;
}
```

- [ ] **Step 3: Call the new suites from the runner**

Replace `runDecisionSelfTest()` in `firmware/03_inference/self_test.h`:

```c
inline int runDecisionSelfTest(Print &out) {
  int failures = runDistanceCases(out)
               + runLightCases(out)
               + runValidityCases(out);
  int total    = DISTANCE_CASE_COUNT + LIGHT_CASE_COUNT + VALIDITY_CASE_COUNT;

  out.print(F("DECISION SELF-TEST: "));
  out.print(total - failures);
  out.print('/');
  out.print(total);
  out.println(failures ? F(" passed - FAILURES PRESENT") : F(" passed - OK"));
  return failures;
}
```

- [ ] **Step 4: Upload and watch the light cases FAIL**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

```bash
powershell -ExecutionPolicy Bypass -File tools/read_serial.ps1 -Seconds 6
```

Expected — the 10% enter band wrongly accepts deviations it should reject, so exactly these five
fail:

```
  FAIL light[2] +101 is BRIGHT at ref 2000 - expected BRIGHT, got CORRECT
  FAIL light[4] -101 is DARK at ref 2000 - expected DARK, got CORRECT
  FAIL light[6] +130 cannot enter CORRECT from BRIGHT - expected BRIGHT, got CORRECT
  FAIL light[10] +40 is BRIGHT at ref 650 (band 32.5) - expected BRIGHT, got CORRECT
  FAIL light[12] +200 is BRIGHT at ref 3400 (band 170) - expected BRIGHT, got CORRECT
DECISION SELF-TEST: 34/39 passed - FAILURES PRESENT
```

Note the broken constant also makes the enter band (200) *wider* than the exit band (140) at a
reference of 2000, which inverts the hysteresis. Case 6 is the one that catches that, and it is
worth understanding why it fails rather than just watching the count.

All 18 distance cases and all 7 validity cases must still pass. If a distance case fails here, Task 1 was not completed correctly.

- [ ] **Step 5: Restore the correct band**

In `firmware/03_inference/decision.h`:

```c
const float LIGHT_TOL_ENTER_PCT = 0.05f;
```

- [ ] **Step 6: Upload and confirm all 39 pass**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

```bash
powershell -ExecutionPolicy Bypass -File tools/read_serial.ps1 -Seconds 6
```

Expected:

```
DECISION SELF-TEST: 39/39 passed - OK
```

- [ ] **Step 7: Commit**

```bash
git add firmware/03_inference
git commit -m "feat: light verdict logic with proportional bands"
```

---

### Task 3: Sample the LDR in the ultrasonic's dead time

**Files:**
- Modify: `firmware/03_inference/03_inference.ino`

**Interfaces:**
- Consumes: nothing from Task 2.
- Produces: `struct Sample { float distMm; int missed; int light; }`; `Sample sense()` replacing `readDistanceMm(int&)`.

- [ ] **Step 1: Add the LDR pin and the sample struct**

In `firmware/03_inference/03_inference.ino`, with the other pin constants:

```c
const int PIN_LDR = A0;
```

Above `pingOnce()`:

```c
// One loop's worth of sensing from both channels.
struct Sample {
  float distMm;   // millimetres, or -1 if every ping failed
  int   missed;   // failed pings out of ULTRA_SAMPLES
  int   light;    // mean LDR counts, 0-4095
};
```

- [ ] **Step 2: Replace `readDistanceMm()` with `sense()`**

Delete `readDistanceMm()` entirely and put this in its place:

```c
// Read the LDR continuously for `ms`, returning the mean.
//
// This runs in the gap the ultrasonic must leave between pings anyway, so the
// light channel costs no loop time at all. Spreading five short windows across
// the ping train also rejects 100 Hz mains ripple and PWM dimmer chop better
// than one contiguous window of the same total length would.
unsigned long accumulateLdr(unsigned long ms, unsigned long &count) {
  unsigned long t0 = millis();
  unsigned long acc = 0;
  while (millis() - t0 < ms) {
    acc += analogRead(PIN_LDR);
    count++;
  }
  return acc;
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
    ldrAcc += accumulateLdr(PING_GAP_MS, ldrCount);
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
```

- [ ] **Step 3: Set 12-bit ADC and update the call sites**

In `setup()`, immediately after `Serial.begin` block and before the OLED init:

```c
  analogReadResolution(12);
```

**This is not optional.** Without it `analogRead` returns 0–1023 and every percentage band is computed against a quarter-scale reading.

In `loop()`, replace the sensing lines:

```c
  Sample s = sense();
  float liveMm = s.distMm;
  int   missed = s.missed;
```

In `captureReference()`, replace its inner read:

```c
    Sample s = sense();
    float d = s.distMm;
```

- [ ] **Step 4: Print the light value temporarily and check the loop rate**

Scaffolding, removed again in Task 4 when `emit()` is rewritten to take the whole `Sample`. Add
near the other globals:

```c
int s_lastLight = 0;
```

Set it in `loop()` immediately after the `sense()` call:

```c
  s_lastLight = s.light;
```

And in `emit()`, replace the final line — `Serial.println(missed);` — with:

```c
  Serial.print(missed);
  Serial.print(',');
  Serial.println(s_lastLight);
```

so the temporary column lands at the end of the row rather than in the middle of it.

- [ ] **Step 5: Upload and verify both channels and the loop rate**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

```bash
powershell -ExecutionPolicy Bypass -File tools/read_serial.ps1 -Seconds 30
```

Three checks:
1. **Light column tracks the lamp.** Cover the LDR — the value should fall sharply. Uncover — it recovers. Expect roughly 1800–2000 in normal room light with the 10 kΩ fitted.
2. **Distance column is unchanged** — still reads correctly against a tape measure, `missed` still 0.
3. **Loop rate is still ~7.8 Hz.** Count the rows and divide by 30. Anything below 7.0 Hz means the LDR sampling is not fitting inside the ping gaps and must be investigated before continuing.

- [ ] **Step 6: Commit**

```bash
git add firmware/03_inference/03_inference.ino
git commit -m "feat: sample LDR inside ultrasonic ping gaps"
```

---

### Task 4: `UNSET`/`ARMED` state, dual references and serial

**Files:**
- Modify: `firmware/03_inference/03_inference.ino`

**Interfaces:**
- Consumes: `Sample sense()`, `decideDistance`, `decideLight`, `lightReadingValid`, `distanceWord`, `lightWord`.
- Produces: globals `bool armed`, `float refMm`, `int refLight`, `Verdict prevDist`, `Verdict prevLight`; `void emit(const Sample&, Verdict, Verdict)`.

- [ ] **Step 1: Replace the reference globals**

In `firmware/03_inference/03_inference.ino`, delete the `DEFAULT_REF_MM` constant and the `refMm` / `prevVerdict` globals, and put in their place:

```c
// No defaults. The device is either set up or it is not - a reference exists
// because the operator captured it, or the device says plainly that it has
// none. One rule for both channels, and no universal "correct" light level to
// invent, because none exists: the right ADC count depends entirely on the
// room, the lamp and where the LDR sits.
bool    armed     = false;
float   refMm     = 0.0f;
int     refLight  = 0;

Verdict prevDist  = V_NO_READ;
Verdict prevLight = V_NO_READ;
```

Delete the now-unused `int s_lastLight` and its assignment from Task 3 — `emit()` takes the whole `Sample` instead.

- [ ] **Step 2: Rewrite `emit()` for both channels**

```c
// One CSV row per loop. This is the log the online evaluation is computed from
// later, which is why it carries the raw miss count and both raw readings as
// well as the verdicts - a row that cannot be trusted must be identifiable
// after the fact. Reference changes are marked by `#` comment lines.
void emit(const Sample &s, Verdict vd, Verdict vl) {
  Serial.print(refMm, 0);           Serial.print(',');
  Serial.print(s.distMm, 0);        Serial.print(',');
  if (s.distMm > 0) Serial.print(s.distMm - refMm, 0); else Serial.print(F("NA"));
  Serial.print(',');
  Serial.print(distanceWord(vd));   Serial.print(',');
  Serial.print(s.missed);           Serial.print(',');
  Serial.print(refLight);           Serial.print(',');
  Serial.print(s.light);            Serial.print(',');
  if (armed) Serial.print(s.light - refLight); else Serial.print(F("NA"));
  Serial.print(',');
  Serial.println(lightWord(vl));
}
```

Update both header prints in `setup()` and `loop()`:

```c
  Serial.println(F("ref_mm,live_mm,diff_mm,dist_verdict,missed,ref_ldr,live_ldr,diff_ldr,light_verdict"));
```

- [ ] **Step 3: Capture both references, and refuse a clipped one**

Replace `captureReference()` and `doSave()`:

```c
// Returns true and fills the two references, or returns false and changes
// nothing. Twenty samples: median for distance, mean for light.
bool captureReference(float &outMm, int &outLight) {
  float v[REF_SAMPLES];
  long  lightAcc = 0;
  int   n = 0;

  for (int i = 0; i < REF_SAMPLES; i++) {
    Sample s = sense();
    if (s.distMm > 0) v[n++] = s.distMm;
    lightAcc += s.light;

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

void showSaveFailed(const __FlashStringHelper *why) {
  Serial.print(F("# SAVE REFUSED - ")); Serial.println(why);
  tone(PIN_BUZZ, 200); delay(400); noTone(PIN_BUZZ);
  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 10); display.print(F("SAVE"));
    display.setCursor(0, 30); display.print(F("FAILED"));
    display.setTextSize(1);
    display.setCursor(0, 54); display.print(why);
    display.display();
    delay(1800);
  }
}

// delay() is acceptable here and nowhere else in this sketch: saving is a
// one-shot action the operator asked for and nothing is being tracked while it
// runs. In the main loop the same call would make the display lag the sensor.
void doSave() {
  silenceBuzzer();

  float newMm = 0.0f;
  int   newLight = 0;

  if (!captureReference(newMm, newLight)) {
    // Both failure modes are reported the same way to the operator, because the
    // remedy is the same: aim it properly and try again. The serial log keeps
    // the distinction for later diagnosis.
    showSaveFailed(F("aim at target, retry"));
    return;                              // both previous references survive
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

  // Forget both histories so the new setup earns its verdicts from scratch.
  prevDist  = V_NO_READ;
  prevLight = V_NO_READ;
}
```

- [ ] **Step 4: Rewrite `loop()` around the two states**

```c
void loop() {
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
```

`render`, `updateBuzzer` and `updateLed` get their new signatures in Tasks 5 and 6. For this task, stub them so the sketch compiles and the serial output can be checked:

```c
void render(const Sample &s, Verdict vd, Verdict vl) { (void)s; (void)vd; (void)vl; }
void updateBuzzer(Verdict vd, Verdict vl) { (void)vd; (void)vl; }
void updateLed(Verdict vd, Verdict vl) { (void)vd; (void)vl; }
```

Delete the old three-argument `render`, the old `updateBuzzer(Verdict, bool)` and the old `updateLed(Verdict)` — they are replaced in the next two tasks.

- [ ] **Step 5: Upload and verify the state machine over serial**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

```bash
powershell -ExecutionPolicy Bypass -File tools/read_serial.ps1 -Seconds 30
```

Check, in order:
1. Before pressing anything: `dist_verdict` and `light_verdict` both read `NO ECHO` / `NO READ`, `ref_mm` is `0`, `diff_ldr` is `NA`. That is `UNSET`.
2. Hold the button 2 s. Serial prints `# REFERENCE SAVED <mm> mm / <counts> counts`.
3. Both verdicts immediately read `CORRECT`, and `diff_ldr` becomes a number near 0.
4. Cover the LDR → `light_verdict` becomes `DARK` while `dist_verdict` stays `CORRECT`.
5. Move the target → `dist_verdict` changes while `light_verdict` stays `CORRECT`. **The two channels must move independently.**

- [ ] **Step 6: Commit**

```bash
git add firmware/03_inference/03_inference.ino
git commit -m "feat: unset/armed state with dual references"
```

---

### Task 5: Two-row OLED

**Files:**
- Modify: `firmware/03_inference/03_inference.ino`

**Interfaces:**
- Consumes: `Sample`, `Verdict`, `armed`, `refMm`, `refLight`, `distanceWord`, `lightWord`, `printCentred`.
- Produces: `void render(const Sample&, Verdict, Verdict)`, `void drawBar(int y, float live, float ref, float span)`.

- [ ] **Step 1: Generalise `drawBar` for both channels**

Replace the existing `drawBar`:

```c
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
```

- [ ] **Step 2: Replace `render()`**

```c
void render(const Sample &s, Verdict vd, Verdict vl) {
  if (!oledOk) return;
  display.clearDisplay();

  if (!armed) {
    // The job of this screen is to help the operator position the rig and set
    // the lamp before capturing, so the live numbers are the content.
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
```

- [ ] **Step 3: Upload and inspect the panel**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

Look at the screen, not the serial:
1. Before saving: `NO SETUP SAVED` / `hold button 2s`, and both live numbers large and updating.
2. After holding the button: two rows, `CORRECT` in both halves, both markers centred.
3. Move the target: top word changes, top marker moves, **bottom half unaffected**.
4. Cover the LDR: bottom word becomes `DARK`, bottom marker moves left, **top half unaffected**.
5. No text clipped at either edge, and no overlap between the halves.

- [ ] **Step 4: Commit**

```bash
git add firmware/03_inference/03_inference.ino
git commit -m "feat: two-row OLED for distance and light"
```

---

### Task 6: Buzzer arbitration and three-state LED

**Files:**
- Modify: `firmware/03_inference/03_inference.ino`

**Interfaces:**
- Consumes: `Verdict`, `armed`, `silenceBuzzer`.
- Produces: `void updateBuzzer(Verdict, Verdict)`, `void updateLed(Verdict, Verdict)`.

No new globals are needed. The double-blip is derived from `millis()` arithmetic against
`buzzerPhaseStart`, which already exists — a counter tracking which half of the blip is due
would be redundant state that could drift out of step with the clock.

- [ ] **Step 1: Replace `updateBuzzer()`**

```c
// Distance first, then light. That is the order a rig is actually set up in -
// position the stand, then dial the brightness - so the operator is only ever
// asked to fix one thing at a time, and the sound is never ambiguous about
// which channel it means.
//
// Rules are evaluated in order; the first match wins.
//   0. not armed              -> silent, there is nothing to guide towards
//   1. distance unreadable    -> silent, cannot say which way to move
//   2. distance wrong         -> distance pattern
//   3. light unreadable       -> silent, a clipped divider is not a verdict
//   4. light wrong            -> double-blip
//   5. both correct           -> one chirp, then silence
//
// Everything is scheduled off millis(). Sounding a tone with delay() would
// block the loop and make the display lag the sensor by the length of the beep.
void updateBuzzer(Verdict vd, Verdict vl) {
  static Verdict lastD = V_NO_READ;
  static Verdict lastL = V_NO_READ;
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
```

- [ ] **Step 3: Replace `updateLed()`**

```c
// Green both right, red distance wrong, blue light wrong, off when unset. The
// on-board RGB is active LOW, needs no wiring, and is the output that actually
// reads on camera from across a room - the colour names the failing channel
// before the operator is close enough to read the screen.
void updateLed(Verdict vd, Verdict vl) {
  bool r = false, g = false, b = false;

  if (armed) {
    if (vd == V_ABOVE || vd == V_BELOW)      r = true;
    else if (vl == V_ABOVE || vl == V_BELOW) b = true;
    else if (vd == V_CORRECT && vl == V_CORRECT) g = true;
  }

  digitalWrite(LEDR, r ? LOW : HIGH);
  digitalWrite(LEDG, g ? LOW : HIGH);
  digitalWrite(LEDB, b ? LOW : HIGH);
}
```

- [ ] **Step 4: Upload and verify by ear and eye**

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --port COM4 --upload "firmware/03_inference"
```

| Do this | Expect |
|---|---|
| Before saving | silent, LED off |
| Save, then leave everything alone | one chirp, then silence, **green** |
| Move target away | slow low beeps, **red** |
| Move target back, then change the lamp brighter | **double-blip** at 1000 Hz, **blue** |
| Dim the lamp below reference | double-blip at 500 Hz, **blue** |
| Move the target *while* the light is wrong | beeping switches to the distance pattern and red — **distance takes priority** |
| Fix distance, light still wrong | double-blip and blue return |
| Cover the ultrasonic | silent |

The priority test is the important one: with both channels wrong, only the distance pattern should sound.

- [ ] **Step 5: Commit**

```bash
git add firmware/03_inference/03_inference.ino
git commit -m "feat: buzzer arbitration and channel-coded LED"
```

---

### Task 7: Acceptance run and recorded evidence

**Files:**
- Create: `reports/phase2_light_acceptance.md`
- Modify: `RUN.md`

**Interfaces:**
- Consumes: the finished sketch.
- Produces: nothing consumed later.

- [ ] **Step 1: Run the nine hardware tests from spec §8**

Work through them against the rig, writing down actual values.

- [ ] **Step 2: Capture a five-minute live log**

```bash
powershell -ExecutionPolicy Bypass -File tools/read_serial.ps1 -Seconds 300 > data/online/phase2_live_5min.csv
```

Use the rig normally while it records: save a setup, move the target, change the lamp, cover the LDR.

- [ ] **Step 3: Write the report**

Create `reports/phase2_light_acceptance.md`, replacing every bracketed field with a measured value:

```markdown
# Phase 2 — light channel acceptance

**Date:** [date]
**Sketch:** `firmware/03_inference` at commit [short sha]
**Board:** Arduino Nano 33 BLE on COM4, LDR on A0 with a 10 kΩ pulldown
**Spec:** `docs/superpowers/specs/2026-08-06-light-channel-design.md` §8

## Self-test

`DECISION SELF-TEST: [n]/39 passed`
(18 distance, 14 light, 7 validity)

## Results

| # | Test | Expected | Observed | Pass |
|---|---|---|---|---|
| 1 | Boot into UNSET | no verdicts, no beeps, LED off | [what appeared] | [Y/N] |
| 2 | Hold button saves both | both CORRECT after | [mm and counts saved] | [Y/N] |
| 3 | Dimmer up one notch | BRIGHT, blue, 1000 Hz double-blip | [ldr reading] | [Y/N] |
| 4 | Dimmer down one notch | DARK, blue, 500 Hz double-blip | [ldr reading] | [Y/N] |
| 5 | Cover the LDR | DARK, recovers when uncovered | [ldr reading] | [Y/N] |
| 6 | Move target while light wrong | distance beeps take priority | [what was heard] | [Y/N] |
| 7 | Both correct | green, one chirp, then silence | [what happened] | [Y/N] |
| 8 | Save with LDR unplugged | refused, references survive | [what appeared] | [Y/N] |
| 9 | 5 min live operation | no resets, rate ~7.8 Hz | [rows, rate, resets] | [Y/N] |

## Measured

- Sketch size: [flash %] flash, [RAM %] RAM
- Loop rate: [Hz] — Phase 1 measured 7.85 Hz, budget is ±10%
- LDR resting level: [counts] of 4095
- LDR noise at a fixed level: [±counts] over [n] samples
- Distance channel regression: [still 0% dropouts? verdicts unchanged?]

## Limitations observed

[Anything that behaved worse than the spec assumed. Candidates: LDR settling
time when the lamp changes suddenly; whether the ±5% band is too tight or too
loose in practice; whether the double-blip is distinguishable from the distance
beeps at a distance; any interaction between the two channels.]
```

- [ ] **Step 4: Update `RUN.md`**

Replace the `03_inference` row in the firmware index:

```markdown
| `03_inference` | ✅ **The product.** Distance *and* light checked against one saved setup. Hold D7 for 2 s to capture. Phase 1–2 decide by threshold; the classifier replaces `decide()` in Phase 3. |
```

- [ ] **Step 5: Commit**

```bash
git add reports/phase2_light_acceptance.md RUN.md data/online/
git commit -m "test: phase 2 light acceptance evidence"
```

---

## Coverage against the spec

| Spec section | Task |
|---|---|
| §1 Sensor choice (already evidenced) | — decided before this plan |
| §2 LDR sampled in ping gaps | 3 |
| §3.1 Channel-neutral enum, word tables | 1 |
| §3.2 Banded `decide()`, two wrappers | 1 |
| §3.3 Constants | 1, 2 |
| §4.1 `UNSET` / `ARMED` model | 4 |
| §4.2 Atomic dual save, both refusal conditions | 4 |
| §5.1 Two-row OLED and the UNSET screen | 5 |
| §5.2 Buzzer arbitration, all six rules | 6 |
| §5.3 Three-state LED | 6 |
| §6 Serial CSV | 4 |
| §7 Error handling — independent channel failure | 4 |
| §7 Error handling — clipped light rejected | 2 (logic), 4 (wiring) |
| §8 Test plan | 7 |
| §10 Definition of done | 7 |
