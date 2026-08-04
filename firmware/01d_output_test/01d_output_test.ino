/*
 * LightGuide Edge - 01d_output_test
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * Verifies the three output/input devices that complete the feedback loop:
 * buzzer, external LED, and the reference-capture switch.
 *
 * Wiring (docs/02-HARDWARE.md section 2):
 *   Buzzer +   -> D9   (right side, 4th pin from USB)   - PWM capable
 *   LED anode  -> D8   via 220R (right side, 5th)       - long leg
 *   LED cathode-> GND
 *   Switch     -> D7   (right side, 6th) and GND        - internal pull-up used
 *
 * The switch uses INPUT_PULLUP, so it reads HIGH when open and LOW when pressed.
 * No external pull-down resistor is needed, and a floating input - which would
 * otherwise trigger randomly - is avoided.
 */

const int PIN_BUZZ   = 9;
const int PIN_EXT_LED    = 8;
const int PIN_SWITCH = 7;

int  pressCount = 0;
bool lastState  = HIGH;
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_MS = 40;

void banner(const char* s) {
  Serial.println();
  Serial.print(F(">>> ")); Serial.println(s);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 8000) { }

  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_EXT_LED, OUTPUT);
  pinMode(PIN_SWITCH, INPUT_PULLUP);
  digitalWrite(PIN_EXT_LED, LOW);

  // On-board RGB is active LOW - drive HIGH to keep it off.
  pinMode(LEDR, OUTPUT); pinMode(LEDG, OUTPUT); pinMode(LEDB, OUTPUT);
  digitalWrite(LEDR, HIGH); digitalWrite(LEDG, HIGH); digitalWrite(LEDB, HIGH);

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F(" OUTPUT DEVICE TEST - watch and listen"));
  Serial.println(F("=================================================="));

  // --- LED ---
  banner("TEST 1: external LED on D8 - blinking 5 times");
  for (int i = 0; i < 5; i++) {
    digitalWrite(PIN_EXT_LED, HIGH); Serial.println(F("    LED ON"));  delay(500);
    digitalWrite(PIN_EXT_LED, LOW);  Serial.println(F("    LED off")); delay(400);
  }
  Serial.println(F("    If it never lit: check polarity (long leg via 220R to D8)"));

  // --- LED brightness ramp, proves PWM works for the feedback design ---
  banner("TEST 2: LED fade up and down");
  for (int b = 0; b <= 255; b += 5)  { analogWrite(PIN_EXT_LED, b); delay(8); }
  for (int b = 255; b >= 0; b -= 5)  { analogWrite(PIN_EXT_LED, b); delay(8); }
  digitalWrite(PIN_EXT_LED, LOW);

  // --- buzzer ---
  banner("TEST 3: buzzer on D9 - three rising beeps");
  int notes[] = {1000, 1500, 2200};
  for (int i = 0; i < 3; i++) {
    Serial.print(F("    beep ")); Serial.print(notes[i]); Serial.println(F(" Hz"));
    tone(PIN_BUZZ, notes[i]);
    delay(350);
    noTone(PIN_BUZZ);
    delay(200);
  }
  Serial.println(F("    Silent? A passive buzzer needs tone(); an ACTIVE buzzer"));
  Serial.println(F("    only responds to digitalWrite HIGH - trying that now."));
  digitalWrite(PIN_BUZZ, HIGH); delay(400); digitalWrite(PIN_BUZZ, LOW);

  // --- switch ---
  banner("TEST 4: switch on D7 - press it a few times");
  Serial.println(F("    Idle reads HIGH; pressing shorts it to GND (LOW)."));
  Serial.print(F("    Current state: "));
  Serial.println(digitalRead(PIN_SWITCH) == HIGH ? F("HIGH (released) - correct")
                                                 : F("LOW - stuck pressed, or wired to GND permanently"));
  Serial.println();
  Serial.println(F("    Press the switch now. Each press lights the LED and beeps."));
}

void loop() {
  int reading = digitalRead(PIN_SWITCH);

  if (reading != lastState) lastDebounce = millis();

  static int stable = HIGH;
  if (millis() - lastDebounce > DEBOUNCE_MS && reading != stable) {
    stable = reading;
    if (stable == LOW) {
      pressCount++;
      Serial.print(F("    *** PRESS #")); Serial.print(pressCount);
      Serial.println(F(" detected ***"));
      digitalWrite(PIN_EXT_LED, HIGH);
      digitalWrite(LEDG, LOW);          // on-board green, active low
      tone(PIN_BUZZ, 1800);
    } else {
      Serial.println(F("        released"));
      digitalWrite(PIN_EXT_LED, LOW);
      digitalWrite(LEDG, HIGH);
      noTone(PIN_BUZZ);
    }
  }
  lastState = reading;

  // Heartbeat so a silent, dark board is distinguishable from a crashed one.
  static unsigned long last = 0;
  if (millis() - last > 3000) {
    last = millis();
    Serial.print(F("    (waiting - switch reads "));
    Serial.print(digitalRead(PIN_SWITCH) == HIGH ? F("HIGH/released") : F("LOW/pressed"));
    Serial.print(F(", presses so far: ")); Serial.print(pressCount);
    Serial.println(F(")"));
  }
}
