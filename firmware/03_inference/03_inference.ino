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

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  runDecisionSelfTest(Serial);
  selfTestReported = (bool)Serial;
  Serial.println(F("live_mm,missed"));
}

void loop() {
  // Booting takes less time than uploading and re-enumerating the USB port, so
  // the report in setup() usually goes nowhere. Print it once more the moment a
  // host actually appears - the result should be observable whenever you attach
  // a monitor, not only if you win a race against the board.
  if (!selfTestReported && Serial) {
    runDecisionSelfTest(Serial);
    Serial.println(F("live_mm,missed"));
    selfTestReported = true;
  }

  int missed = 0;
  float liveMm = readDistanceMm(missed);

  Serial.print(liveMm, 0);
  Serial.print(',');
  Serial.println(missed);
}
