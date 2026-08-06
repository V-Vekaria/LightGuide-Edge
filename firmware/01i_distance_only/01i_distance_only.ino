/*
 * LightGuide Edge - 01i_distance_only
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * Distance and nothing else. Big number on the OLED, in centimetres.
 * Use it to aim the sensor and set up the reference position.
 *
 * The small "miss" figure counts pings that got no echo. It is only there to
 * tell you the sensor is actually pointed at your target - a big number with a
 * high miss count is not a real measurement.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const int PIN_TRIG = 2, PIN_ECHO = 3;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledOk = false;

const int  ULTRA_SAMPLES = 5;
const int  PING_GAP_MS   = 10;
const unsigned long ECHO_START_TIMEOUT_US = 15000UL;
const unsigned long ECHO_HIGH_MAX_US      = 25000UL;
const float DIST_MIN_MM = 20.0f, DIST_MAX_MM = 4000.0f;

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
  Wire.begin();
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledOk) display.setTextColor(SSD1306_WHITE);
  Serial.println(F("dist_mm,dist_cm,missed"));
}

void loop() {
  int missed = 0;
  float mm = readDistanceMm(missed);

  Serial.print(mm, 1);        Serial.print(',');
  Serial.print(mm / 10.0, 1); Serial.print(',');
  Serial.println(missed);

  if (oledOk) {
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("DISTANCE"));

    display.setTextSize(4);
    display.setCursor(0, 16);
    if (mm > 0) display.print(mm / 10.0, 0);
    else        display.print(F("--"));

    display.setTextSize(2);
    display.setCursor(90, 28);
    display.print(F("cm"));

    display.setTextSize(1);
    display.setCursor(0, 54);
    if (mm < 0)         display.print(F("no echo - aim at target"));
    else if (missed > 1) display.print(F("miss "));
    else                 display.print(F("ok"));
    if (mm > 0 && missed > 1) { display.print(missed); display.print(F("/5 - re-aim")); }

    display.display();
  }
}
