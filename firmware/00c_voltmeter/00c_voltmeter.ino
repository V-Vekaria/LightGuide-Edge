/*
 * LightGuide Edge - 00c_voltmeter
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * Turns the Nano into a multimeter so wiring questions can be MEASURED rather than
 * guessed at from photographs.
 *
 * HOW TO USE
 *   Put one end of a jumper into the Nano's A1 pin. Touch/plug the other end into
 *   whatever breadboard point you want to test. Read the voltage over serial.
 *   A2, A3, A6 and A7 work the same way if you want four probes at once.
 *
 * >>> SAFETY: these pins are 3.3 V maximum. NEVER probe a 5 V point. <<<
 *     Everything in this build should be 3.3 V, which is exactly what we are
 *     confirming. If a reading pins at 3.3 V, disconnect and check before assuming.
 *
 * WHAT THE READINGS MEAN
 *   3.25 - 3.30 V   a live 3V3 rail
 *   ~0.00 V         ground, or a dead rail
 *   1.5 - 1.8 V     the midpoint of a 1:1-ish divider, or an LED clamping
 *   floating/noisy  nothing connected (the value drifts and the spread is large)
 *
 * The spread (min/max over the sample window) is printed alongside the mean,
 * because a floating probe wanders while a real rail sits rock steady. That
 * distinction is the whole point - a single averaged number would hide it.
 */

const int PROBES[] = {A1, A2, A3, A6, A7};
const char* NAMES[] = {"A1", "A2", "A3", "A6", "A7"};
const int N = sizeof(PROBES) / sizeof(PROBES[0]);

const float VREF = 3.3f;
const float ADC_MAX = 4095.0f;
const int SAMPLES = 64;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 8000) { }
  analogReadResolution(12);
  for (int i = 0; i < N; i++) pinMode(PROBES[i], INPUT);

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F(" NANO AS VOLTMETER"));
  Serial.println(F("=================================================="));
  Serial.println(F(" Jumper from A1 (or A2/A3/A6/A7) to the point to test."));
  Serial.println(F(" MAX 3.3V - never probe a 5V point."));
  Serial.println();
  Serial.println(F(" WHAT TO MEASURE, in this order:"));
  Serial.println(F("  1. The Nano's own 3V3 pin      -> expect ~3.3V steady"));
  Serial.println(F("                                    (proves the probe works)"));
  Serial.println(F("  2. The breadboard + rail        -> is it actually fed?"));
  Serial.println(F("  3. The HC-SR04 VCC pin          -> is the module powered?"));
  Serial.println(F("  4. The HC-SR04 ECHO wire        -> idle should be ~0V"));
  Serial.println(F("  5. Each end of the resistors    -> reveals any divider"));
  Serial.println();
  Serial.println(F(" probe   volts    min     max     spread   reading"));
}

void loop() {
  for (int i = 0; i < N; i++) {
    int lo = 4095, hi = 0;
    long acc = 0;
    for (int s = 0; s < SAMPLES; s++) {
      int v = analogRead(PROBES[i]);
      acc += v;
      if (v < lo) lo = v;
      if (v > hi) hi = v;
      delayMicroseconds(150);
    }
    float mean = (acc / (float)SAMPLES) * VREF / ADC_MAX;
    float vlo = lo * VREF / ADC_MAX;
    float vhi = hi * VREF / ADC_MAX;
    float spread = vhi - vlo;

    Serial.print(F("  ")); Serial.print(NAMES[i]);
    Serial.print(F("   ")); Serial.print(mean, 3);
    Serial.print(F("V  ")); Serial.print(vlo, 3);
    Serial.print(F("  ")); Serial.print(vhi, 3);
    Serial.print(F("   ")); Serial.print(spread, 3);
    Serial.print(F("   "));

    // Interpretation. The spread is what separates "connected to something" from
    // "dangling in free air" - a floating input wanders, a driven node does not.
    // Threshold set from measured behaviour: unconnected pins on this board wander
    // by 0.10-1.0 V around ~0.3 V. Anything under ~0.09 V of spread is a real node.
    if (spread > 0.09f)              Serial.println(F("FLOATING - nothing connected"));
    else if (mean > 3.10f)           Serial.println(F("3V3 rail - live"));
    else if (mean < 0.15f)           Serial.println(F("GROUND (or a dead rail)"));
    else if (mean > 1.4f && mean < 2.0f) Serial.println(F("~half rail - divider midpoint?"));
    else                             Serial.println(F("intermediate - note the value"));
  }
  Serial.println();
  delay(1200);
}
