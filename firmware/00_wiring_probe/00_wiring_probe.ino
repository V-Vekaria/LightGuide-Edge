/*
 * LightGuide Edge - 00_wiring_probe
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * Answers "what is actually connected to which pin?" without a multimeter.
 * Written during bring-up when the HC-SR04 would not respond; it identified an
 * unpowered module in one pass. Keep it - it is faster than re-reading jumper
 * wires every time something goes quiet.
 *
 * READING THE OUTPUT
 *   floating          nothing attached to that pin
 *   held LOW          tied to GND, OR a driven-low output, OR an UNPOWERED chip
 *                     (protection structures clamp the pins of a dead chip to
 *                     ground - this is how "no power" is distinguished from
 *                     "miswired")
 *   held HIGH         tied to a supply rail or a driven-high output
 *
 * A powered sensor's INPUT pin (e.g. HC-SR04 TRIG) should read FLOATING, because
 * a live CMOS input is high impedance. If a pin you believe is an input reads
 * "held LOW", suspect power before suspecting the wiring.
 *
 * Two jobs:
 *  1. Characterise every digital pin by reading it with the internal pull-up and
 *     then the pull-down. The pair of results says what the pin is connected to:
 *       pullup HIGH  + pulldown LOW   -> floating (nothing attached)
 *       pullup LOW   + pulldown LOW   -> externally held LOW (GND, or a driven-low output)
 *       pullup HIGH  + pulldown HIGH  -> externally held HIGH (a supply rail, or driven high)
 *  2. Pulse each candidate TRIG pin and watch EVERY other pin for a response, so a
 *     (trig, echo) pair is found wherever it actually lives - not just on D2/D3.
 */

const int PINS[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
const int N = sizeof(PINS) / sizeof(PINS[0]);
const unsigned long WIDE_TIMEOUT_US = 60000UL;

void characterise() {
  Serial.println(F("pin,pullup,pulldown,verdict"));
  for (int i = 0; i < N; i++) {
    int p = PINS[i];

    pinMode(p, INPUT_PULLUP);
    delay(3);
    int up = 0;
    for (int k = 0; k < 15; k++) { up += digitalRead(p); delayMicroseconds(200); }

    pinMode(p, INPUT_PULLDOWN);
    delay(3);
    int dn = 0;
    for (int k = 0; k < 15; k++) { dn += digitalRead(p); delayMicroseconds(200); }

    pinMode(p, INPUT);

    bool upHigh = up > 12, dnHigh = dn > 12;
    Serial.print(p); Serial.print(',');
    Serial.print(upHigh ? F("HIGH") : F("LOW")); Serial.print(',');
    Serial.print(dnHigh ? F("HIGH") : F("LOW")); Serial.print(',');
    if (upHigh && !dnHigh)      Serial.println(F("floating - nothing attached"));
    else if (!upHigh && !dnHigh) Serial.println(F("held LOW externally <-- connected"));
    else if (upHigh && dnHigh)   Serial.println(F("held HIGH externally <-- connected"));
    else                         Serial.println(F("inconsistent"));
  }
}

void findPair() {
  Serial.println();
  Serial.println(F("Pulsing each pin as TRIG, watching all others for a response:"));
  for (int t = 0; t < N; t++) {
    int trig = PINS[t];

    for (int i = 0; i < N; i++) if (PINS[i] != trig) pinMode(PINS[i], INPUT);
    pinMode(trig, OUTPUT);
    digitalWrite(trig, LOW);
    delay(5);

    // baseline
    int base[N];
    for (int i = 0; i < N; i++) base[i] = (PINS[i] == trig) ? 0 : digitalRead(PINS[i]);

    digitalWrite(trig, HIGH); delayMicroseconds(10); digitalWrite(trig, LOW);

    bool changed[N];
    for (int i = 0; i < N; i++) changed[i] = false;
    unsigned long mark = micros();
    while (micros() - mark < WIDE_TIMEOUT_US) {
      for (int i = 0; i < N; i++) {
        if (PINS[i] == trig) continue;
        if (digitalRead(PINS[i]) != base[i]) changed[i] = true;
      }
    }

    bool any = false;
    for (int i = 0; i < N; i++) if (changed[i]) any = true;
    if (any) {
      Serial.print(F("  TRIG=D")); Serial.print(trig); Serial.print(F(" -> response on"));
      for (int i = 0; i < N; i++) if (changed[i]) { Serial.print(F(" D")); Serial.print(PINS[i]); }
      Serial.println();
    }
    pinMode(trig, INPUT);
    delay(60);
  }
  Serial.println(F("  (no line above = no pin pair responded)"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) { }
  Serial.println();
  Serial.println(F("=== WIRING MAP PROBE ==="));
}

void loop() {
  characterise();
  findPair();
  Serial.println(F("--- pass complete ---"));
  Serial.println();
  delay(4000);
}
