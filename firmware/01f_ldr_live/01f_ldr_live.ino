/*
 * LightGuide Edge - 01f_ldr_live
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * Big live LDR reading on the OLED, so the divider can be debugged at the rig
 * without a laptop. Wiggle a wire and the number reacts immediately.
 *
 * WHAT TO LOOK FOR
 *   Cover the LDR with a finger -> the number MUST drop by hundreds.
 *   If covering it does nothing, the circuit is open no matter how plausible
 *   the number looks. An analog pin always returns a value; a plausible value
 *   is not evidence of a connection.
 *
 * The status line is driven by the swing since power-up, not by the absolute
 * value, because a floating pin sits at a perfectly believable mid-scale number.
 *
 * Press the button to reset min/max.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const int PIN_LDR = A0;
const int PIN_SWITCH = 7;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledOk = false;

int vMin = 4095, vMax = 0;

int readLdr() {
  // Spread the reads over ~20 ms rather than taking them back-to-back. LED panels
  // dim by PWM; sampling faster than one PWM cycle returns whatever phase it
  // happened to catch, which looks like noise and hides the real mean.
  long acc = 0;
  const int N = 40;
  for (int i = 0; i < N; i++) { acc += analogRead(PIN_LDR); delayMicroseconds(500); }
  return (int)(acc / N);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }
  analogReadResolution(12);
  pinMode(PIN_SWITCH, INPUT_PULLUP);
  Wire.begin();
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledOk) display.setTextColor(SSD1306_WHITE);
  Serial.println(F("ldr_raw,min,max,swing"));
}

void loop() {
  if (digitalRead(PIN_SWITCH) == LOW) { vMin = 4095; vMax = 0; delay(200); }

  int v = readLdr();
  if (v < vMin) vMin = v;
  if (v > vMax) vMax = v;
  int swing = vMax - vMin;

  Serial.print(v);      Serial.print(',');
  Serial.print(vMin);   Serial.print(',');
  Serial.print(vMax);   Serial.print(',');
  Serial.println(swing);

  if (oledOk) {
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("LDR LIVE"));
    display.setCursor(74, 0);
    display.print(F("/4095"));

    // big current value
    display.setTextSize(3);
    display.setCursor(0, 11);
    display.print(v);

    // bar
    int w = (int)((long)v * 128 / 4095);
    display.drawRect(0, 38, 128, 8, SSD1306_WHITE);
    display.fillRect(0, 38, w, 8, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 49);
    display.print(F("lo")); display.print(vMin);
    display.print(F(" hi")); display.print(vMax);

    display.setCursor(0, 57);
    if (swing > 800)      display.print(F("CONNECTED - good swing"));
    else if (swing > 200) display.print(F("weak - aim at panel"));
    else                  display.print(F("COVER IT to test"));

    display.display();
  }
}
