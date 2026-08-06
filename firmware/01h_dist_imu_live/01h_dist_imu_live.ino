/*
 * LightGuide Edge - 01h_dist_imu_live
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * Live distance and tilt on the OLED, with a rolling stability figure, so both
 * channels can be judged at the rig without a laptop.
 *
 * WHAT TO LOOK FOR
 *   Standing still, DIST should barely move. The "sd" figure is the standard
 *   deviation of the last 20 readings - that is the number to watch, not the
 *   instantaneous value. Under ~5 mm is excellent for an HC-SR04.
 *
 *   Tilting the rig should move PITCH/ROLL smoothly and they should return to the
 *   same values when you put it back. The board is mounted vertically, so pitch
 *   rests near 88 degrees rather than 0 - that is the mounting, not an error.
 *
 *   DROP is the percentage of pings that returned no echo. Above 10% means the
 *   sensor is not seeing a solid target: aim it at something flat and hard, at the
 *   same height as the sensor.
 *
 * Press the button to reset the statistics.
 */

#define BOARD_REV 1

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#if BOARD_REV == 1
  #include <Arduino_LSM9DS1.h>
#else
  #include <Arduino_BMI270_BMM150.h>
#endif

const int PIN_TRIG = 2, PIN_ECHO = 3, PIN_SWITCH = 7;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledOk = false;

const int ULTRA_SAMPLES = 5, PING_GAP_MS = 10;
const unsigned long ECHO_START_TIMEOUT_US = 15000UL;
const unsigned long ECHO_HIGH_MAX_US      = 25000UL;
const float DIST_MIN_MM = 20.0f, DIST_MAX_MM = 4000.0f;

// rolling window for the stability figure
const int WIN = 20;
float win[WIN];
int winN = 0, winI = 0;

unsigned long pings = 0, drops = 0;

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

// Median of 5, not mean: ultrasonic errors are wild outliers rather than gaussian
// noise, so a single bad echo would drag a mean badly while a median ignores it.
float readDistanceMm() {
  float v[ULTRA_SAMPLES];
  int n = 0;
  for (int i = 0; i < ULTRA_SAMPLES; i++) {
    pings++;
    float d = pingOnce();
    if (d > 0) v[n++] = d; else drops++;
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

float winSd(float &meanOut) {
  if (winN < 2) { meanOut = winN ? win[0] : 0; return 0; }
  float m = 0;
  for (int i = 0; i < winN; i++) m += win[i];
  m /= winN;
  float s = 0;
  for (int i = 0; i < winN; i++) s += (win[i] - m) * (win[i] - m);
  meanOut = m;
  return sqrt(s / winN);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }
  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_ECHO, INPUT); digitalWrite(PIN_TRIG, LOW);
  pinMode(PIN_SWITCH, INPUT_PULLUP);
  Wire.begin();
  IMU.begin();
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledOk) display.setTextColor(SSD1306_WHITE);
  Serial.println(F("dist_mm,sd_mm,pitch,roll,gx,gy,gz,drop_pct"));
}

void loop() {
  if (digitalRead(PIN_SWITCH) == LOW) {
    winN = winI = 0; pings = drops = 0;
    delay(250);
  }

  float dist = readDistanceMm();
  if (dist > 0) {
    win[winI] = dist;
    winI = (winI + 1) % WIN;
    if (winN < WIN) winN++;
  }

  float mean = 0;
  float sd = winSd(mean);

  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
  if (IMU.accelerationAvailable()) IMU.readAcceleration(ax, ay, az);
  if (IMU.gyroscopeAvailable())    IMU.readGyroscope(gx, gy, gz);
  float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI;
  float roll  = atan2(ay, az) * 180.0f / PI;

  // atan2 returns -180..+180. With the board mounted vertically, roll rests near
  // 180 - right on the discontinuity - so an unchanged orientation flips between
  // +179 and -179 and looks violently unstable. Remapping to 0..360 moves the
  // wrap point to 0/360, far from anything this rig actually sits at.
  //
  // For the ML features proper, angles should be encoded as sin/cos pairs: that
  // removes the discontinuity entirely rather than relocating it, which is the
  // standard treatment for circular quantities. Relocating is enough for a live
  // readout; sin/cos goes into the feature extractor.
  if (roll < 0) roll += 360.0f;
  float dropPct = pings ? (100.0f * drops / pings) : 0;

  Serial.print(dist, 1);   Serial.print(',');
  Serial.print(sd, 2);     Serial.print(',');
  Serial.print(pitch, 2);  Serial.print(',');
  Serial.print(roll, 2);   Serial.print(',');
  Serial.print(gx, 2);     Serial.print(',');
  Serial.print(gy, 2);     Serial.print(',');
  Serial.print(gz, 2);     Serial.print(',');
  Serial.println(dropPct, 1);

  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("DIST mm"));
    display.setCursor(66, 0);
    display.print(F("drop "));
    display.print(dropPct, 0);
    display.print('%');

    display.setTextSize(2);
    display.setCursor(0, 10);
    if (dist > 0) display.print(dist, 0); else display.print(F("----"));

    display.setTextSize(1);
    display.setCursor(74, 16);
    display.print(F("sd"));
    display.print(sd, 1);

    display.drawFastHLine(0, 29, 128, SSD1306_WHITE);

    display.setCursor(0, 33);
    display.print(F("pitch "));
    display.print(pitch, 1);
    display.setCursor(0, 42);
    display.print(F("roll  "));
    display.print(roll, 1);

    display.setCursor(0, 54);
    if (dist < 0)        display.print(F("NO ECHO - aim at target"));
    else if (sd < 5)     display.print(F("VERY STABLE"));
    else if (sd < 20)    display.print(F("stable"));
    else                 display.print(F("moving / noisy"));

    display.display();
  }
}
