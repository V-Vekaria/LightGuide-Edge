/*
 * LightGuide Edge - 01b_calibration
 * COM683 CW2 | Vishnu Vekariya | Ulster University
 *
 * Streams every light- and distance-related channel at once so a single physical
 * sweep calibrates BOTH sensors. tools/calibrate.py drives it.
 *
 * Channels:
 *   ldr_raw     external LDR on A0, 12-bit, mean of 16 reads
 *   apds_amb    on-board APDS9960 ambient light - an INDEPENDENT second opinion,
 *               so LDR behaviour can be cross-checked rather than taken on trust
 *   dist_mm     HC-SR04, median of 5 (may be -1 while the ECHO fault is unresolved;
 *               the calibration does not depend on it - the tape measure is the
 *               reference and is more accurate anyway)
 *
 * Why no lux meter: the classification is relative to a saved reference setup, so
 * absolute lux is not required. Holding the lamp output fixed and varying distance
 * makes illuminance a known quantity up to one scale factor (E proportional to
 * 1/d^2), which is enough to recover the LDR's response exponent. If a meter is
 * available later, one scale factor converts the whole curve to absolute units -
 * nothing has to be recollected.
 *
 * Commands:
 *   ?   print state
 *   (streams continuously otherwise)
 */

#define BOARD_REV 1

#include <Wire.h>
#include <Arduino_APDS9960.h>

const int PIN_LDR  = A0;
const int PIN_TRIG = 2;
const int PIN_ECHO = 3;

const int LDR_SAMPLES   = 16;    // more averaging than normal: this is a measurement,
                                 // not a live reading, so trade rate for precision
const int ULTRA_SAMPLES = 5;
const int PING_GAP_MS   = 10;
const unsigned long ECHO_START_TIMEOUT_US = 15000UL;
const unsigned long ECHO_HIGH_MAX_US      = 25000UL;
const float DIST_MIN_MM = 20.0f;
const float DIST_MAX_MM = 4000.0f;

const unsigned long PERIOD_MS = 200;   // 5 Hz is plenty for a static measurement
unsigned long lastSample = 0;
bool apdsOk = false;

float pingOnce() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(4);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long mark = micros();
  while (digitalRead(PIN_ECHO) == LOW) {
    if (micros() - mark > ECHO_START_TIMEOUT_US) return -1.0f;
  }
  unsigned long rise = micros();
  while (digitalRead(PIN_ECHO) == HIGH) {
    if (micros() - rise > ECHO_HIGH_MAX_US) return -1.0f;
  }
  float mm = (micros() - rise) * 0.1715f;
  if (mm < DIST_MIN_MM || mm > DIST_MAX_MM) return -1.0f;
  return mm;
}

float readDistanceMm() {
  float v[ULTRA_SAMPLES];
  int n = 0;
  for (int i = 0; i < ULTRA_SAMPLES; i++) {
    float d = pingOnce();
    if (d > 0) v[n++] = d;
    delay(PING_GAP_MS);
  }
  if (n == 0) return -1.0f;
  for (int i = 1; i < n; i++) {
    float k = v[i]; int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[n / 2];
}

// Mean and standard deviation together: the spread at each step is what tells us
// whether a reading is trustworthy, and it goes straight into the error bars on
// the calibration plot.
void readLdr(float &mean, float &sd) {
  long   acc = 0;
  double sq  = 0;
  int    v[32];
  for (int i = 0; i < LDR_SAMPLES; i++) {
    v[i] = analogRead(PIN_LDR);
    acc += v[i];
    delay(2);
  }
  mean = (float)acc / LDR_SAMPLES;
  for (int i = 0; i < LDR_SAMPLES; i++) sq += (v[i] - mean) * (v[i] - mean);
  sd = sqrt(sq / LDR_SAMPLES);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) { }

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  analogReadResolution(12);

  Wire.begin();
  apdsOk = APDS.begin();

  Serial.println(F("# LightGuide Edge calibration stream"));
  Serial.print  (F("# APDS9960 ambient channel: "));
  Serial.println(apdsOk ? F("available") : F("NOT available - LDR only, no cross-check"));
  Serial.println(F("# hold the lamp output FIXED; vary distance only"));
  Serial.println(F("ldr_raw,ldr_sd,apds_amb,dist_mm"));
}

void loop() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    Serial.print(F("# apds=")); Serial.println(apdsOk ? F("ok") : F("absent"));
  }

  unsigned long now = millis();
  if (now - lastSample < PERIOD_MS) return;
  lastSample = now;

  float ldr, sd;
  readLdr(ldr, sd);

  int amb = -1;
  if (apdsOk && APDS.colorAvailable()) {
    int r, g, b, a;
    APDS.readColor(r, g, b, a);
    amb = a;
  }

  float dist = readDistanceMm();

  Serial.print(ldr, 1);  Serial.print(',');
  Serial.print(sd, 2);   Serial.print(',');
  Serial.print(amb);     Serial.print(',');
  Serial.println(dist, 1);
}
