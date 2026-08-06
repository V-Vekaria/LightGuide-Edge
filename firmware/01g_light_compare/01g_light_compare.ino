/*
 * LightGuide Edge - 01g_light_compare
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * Shows the external LDR and the on-board APDS9960 side by side, so the light
 * channel can be chosen on evidence rather than intent.
 *
 * Context: the LDR has repeatedly proved to be an intermittent connection - it
 * responds, then sits at a fixed value regardless of illumination. Its legs are a
 * friction fit in jumper sockets on a rig that gets carried around. The APDS9960
 * is soldered to the Nano and has no wiring at all, so it cannot fail that way.
 *
 * The decision rule is simply which one tracks the panel. Whichever shows the
 * larger, more repeatable swing becomes the light channel for the dataset.
 *
 * Press the button to reset the min/max on both.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino_APDS9960.h>

const int PIN_LDR = A0;
const int PIN_SWITCH = 7;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledOk = false, apdsOk = false;

int ldrMin = 4095, ldrMax = 0;
int ambMin = 99999, ambMax = -1;

// Average over a 100 ms window.
//
// An LED panel does not emit steady light: it dims by PWM, and mains-powered
// supplies add 100 Hz ripple on top. A short averaging window catches a different
// slice of that waveform each time, and as the phase drifts against the sample
// rate the mean glides smoothly up and down over seconds - which looks exactly
// like a faulty sensor even though nothing has moved.
//
// 100 ms spans 10 full cycles of 100 Hz mains ripple and hundreds of cycles of a
// typical PWM dimmer, so the flicker integrates away and what remains is the
// actual light level. This is the same reason a camera at 1/1000 s shows banding
// that the eye never sees.
const unsigned long AVG_WINDOW_MS = 100;

int readLdr() {
  unsigned long t0 = millis();
  unsigned long acc = 0;
  unsigned long n = 0;
  while (millis() - t0 < AVG_WINDOW_MS) {
    acc += analogRead(PIN_LDR);
    n++;
  }
  return n ? (int)(acc / n) : 0;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }
  analogReadResolution(12);
  pinMode(PIN_SWITCH, INPUT_PULLUP);
  Wire.begin();
  apdsOk = APDS.begin();
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledOk) display.setTextColor(SSD1306_WHITE);
  Serial.println(F("ldr,ldr_swing,apds,apds_swing"));
}

void loop() {
  if (digitalRead(PIN_SWITCH) == LOW) {
    ldrMin = 4095; ldrMax = 0; ambMin = 99999; ambMax = -1;
    delay(250);
  }

  int ldr = readLdr();
  if (ldr < ldrMin) ldrMin = ldr;
  if (ldr > ldrMax) ldrMax = ldr;

  int amb = -1;
  if (apdsOk && APDS.colorAvailable()) {
    int r, g, b, a;
    APDS.readColor(r, g, b, a);
    amb = a;
    if (amb < ambMin) ambMin = amb;
    if (amb > ambMax) ambMax = amb;
  }

  int ldrSwing = ldrMax - ldrMin;
  int ambSwing = (ambMax >= 0) ? ambMax - ambMin : 0;

  Serial.print(ldr);       Serial.print(',');
  Serial.print(ldrSwing);  Serial.print(',');
  Serial.print(amb);       Serial.print(',');
  Serial.println(ambSwing);

  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.print(F("LDR (wired)"));
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.print(ldr);
    display.setTextSize(1);
    display.setCursor(70, 16);
    display.print(F("sw")); display.print(ldrSwing);

    display.drawFastHLine(0, 30, 128, SSD1306_WHITE);

    display.setCursor(0, 34);
    display.print(F("APDS (on-board)"));
    display.setTextSize(2);
    display.setCursor(0, 44);
    if (amb >= 0) display.print(amb); else display.print(F("--"));
    display.setTextSize(1);
    display.setCursor(70, 50);
    display.print(F("sw")); display.print(ambSwing);

    display.display();
  }
}
