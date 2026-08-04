/*
 * LightGuide Edge - 00b_pin_identify
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * Interactive pin identification. A passive scan cannot see an LED, a buzzer or a
 * switch reliably, so this drives each pin in turn and asks YOU what happened.
 *
 * Three phases, each announced over serial:
 *   1. OUTPUT sweep  - drives each pin HIGH for 1.5 s. Watch for the LED lighting.
 *   2. TONE sweep    - plays a tone on each pin. Listen for the buzzer.
 *   3. INPUT watch   - monitors every pin for 20 s. Press the switch; the pin that
 *                      changes is reported.
 *
 * Open the Serial Monitor at 115200 and watch the board while it runs.
 *
 * Skipped deliberately: A0 (LDR divider), A4/A5 (OLED I2C). Driving those would
 * disturb hardware already known to be working.
 */

const int PINS[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, A1, A2, A3, A6, A7};
const char* NAMES[] = {"D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D10",
                       "D11", "D12", "A1", "A2", "A3", "A6", "A7"};
const int N = sizeof(PINS) / sizeof(PINS[0]);

void allInputs() {
  for (int i = 0; i < N; i++) pinMode(PINS[i], INPUT);
}

void phaseOutput() {
  Serial.println();
  Serial.println(F("=== PHASE 1: LED sweep ==="));
  Serial.println(F("Each pin is driven HIGH for 1.5 s."));
  Serial.println(F("WATCH THE BOARD. Note the pin name shown when the LED lights."));
  Serial.println();
  delay(2500);

  for (int i = 0; i < N; i++) {
    allInputs();
    Serial.print(F("  driving ")); Serial.print(NAMES[i]); Serial.println(F(" HIGH..."));
    pinMode(PINS[i], OUTPUT);
    digitalWrite(PINS[i], HIGH);
    delay(1500);
    digitalWrite(PINS[i], LOW);
    pinMode(PINS[i], INPUT);
    delay(400);
  }
  allInputs();
  Serial.println(F("  ...LED sweep done."));
}

void phaseTone() {
  Serial.println();
  Serial.println(F("=== PHASE 2: buzzer sweep ==="));
  Serial.println(F("A 2 kHz tone is played on each pin for 1.2 s. LISTEN."));
  Serial.println();
  delay(2500);

  for (int i = 0; i < N; i++) {
    allInputs();
    Serial.print(F("  tone on ")); Serial.print(NAMES[i]); Serial.println(F("..."));
    pinMode(PINS[i], OUTPUT);
    tone(PINS[i], 2000);
    delay(1200);
    noTone(PINS[i]);
    digitalWrite(PINS[i], LOW);
    pinMode(PINS[i], INPUT);
    delay(400);
  }
  allInputs();
  Serial.println(F("  ...buzzer sweep done."));
}

void phaseInput() {
  Serial.println();
  Serial.println(F("=== PHASE 3: switch watch ==="));
  Serial.println(F("PRESS AND RELEASE THE SWITCH a few times over the next 20 s."));
  Serial.println();
  delay(1500);

  int base[16];
  for (int i = 0; i < N; i++) {
    pinMode(PINS[i], INPUT_PULLUP);
  }
  delay(50);
  for (int i = 0; i < N; i++) base[i] = digitalRead(PINS[i]);

  bool seen[16];
  for (int i = 0; i < N; i++) seen[i] = false;

  unsigned long t0 = millis();
  while (millis() - t0 < 20000) {
    for (int i = 0; i < N; i++) {
      if (digitalRead(PINS[i]) != base[i] && !seen[i]) {
        seen[i] = true;
        Serial.print(F("  *** change detected on ")); Serial.print(NAMES[i]);
        Serial.println(F("  <-- likely the switch"));
      }
    }
    delay(2);
  }

  bool any = false;
  for (int i = 0; i < N; i++) if (seen[i]) any = true;
  if (!any) {
    Serial.println(F("  no pin changed. Either the switch is not on a scanned pin,"));
    Serial.println(F("  or it is wired between two rails rather than to a GPIO."));
  }
  allInputs();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 8000) { }
  allInputs();
  Serial.println();
  Serial.println(F("================================================"));
  Serial.println(F(" PIN IDENTIFICATION - watch and listen to the board"));
  Serial.println(F("================================================"));
}

void loop() {
  phaseOutput();
  phaseTone();
  phaseInput();

  Serial.println();
  Serial.println(F("=== pass complete - repeating in 8 s ==="));
  Serial.println(F("Tell the assistant which pin name matched the LED, the buzzer"));
  Serial.println(F("and the switch, and the pin map will be updated."));
  delay(8000);
}
