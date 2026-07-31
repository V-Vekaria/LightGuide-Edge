/*
 * LightGuide Edge - 02_data_logger
 * COM683 CW2 | Vishnu Vekariya | Ulster University
 *
 * Labelled data capture for the six setup classes. Streams one CSV row per sample
 * at 10 Hz over USB serial; tools/capture.py drives it and writes the files.
 *
 * The firmware deliberately does NOT decide when to record. The operator sets the
 * label before a run starts (docs/03-DATA-PROTOCOL.md section 5, control 4) and the
 * host tool handles warm-up discard and file naming. Keeping the labelling decision
 * with the operator and out of the firmware is what makes the labels trustworthy.
 *
 * Serial commands (newline terminated):
 *   L<0-5>   set the class label      e.g. L2
 *   S<n>     set the session id       e.g. S1
 *   G        go   - begin streaming rows
 *   X        stop - halt streaming
 *   ?        print current state
 *
 * Class labels (docs/AGENTS.md section 4):
 *   0 optimal   1 too_close   2 too_far   3 underlit   4 overlit   5 tilt_off
 *
 * BEFORE POWERING UP: read docs/02-HARDWARE.md section 3 (HC-SR04 voltage hazard).
 */

#define BOARD_REV 1     // 1 = LSM9DS1, 2 = BMI270/BMM150. See 01_sensor_check output.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#if BOARD_REV == 1
  #include <Arduino_LSM9DS1.h>
#else
  #include <Arduino_BMI270_BMM150.h>
#endif

const int PIN_LDR  = A0;
const int PIN_TRIG = 2;
const int PIN_ECHO = 3;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledOk = false;

const unsigned long SAMPLE_PERIOD_MS = 100;   // 10 Hz
const int  ULTRA_SAMPLES   = 5;
const int  LDR_SAMPLES     = 8;
const long ECHO_TIMEOUT_US = 25000L;
const float DIST_MIN_MM = 20.0f;
const float DIST_MAX_MM = 4000.0f;

const char* CLASS_NAMES[6] = {
  "optimal", "too_close", "too_far", "underlit", "overlit", "tilt_off"
};

int  label      = 0;
int  session    = 1;
bool streaming  = false;
unsigned long lastSample = 0;
unsigned long runStartMs = 0;
unsigned long rowCount   = 0;
unsigned long dropouts   = 0;

// --- sensing ---------------------------------------------------------------

float pingOnce() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(4);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long us = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (us == 0) return -1.0f;
  float mm = us * 0.1715f;                     // 343 m/s, out and back
  if (mm < DIST_MIN_MM || mm > DIST_MAX_MM) return -1.0f;
  return mm;
}

float readDistanceMm() {
  float v[ULTRA_SAMPLES];
  int n = 0;
  for (int i = 0; i < ULTRA_SAMPLES; i++) {
    float d = pingOnce();
    if (d > 0) v[n++] = d;
    delay(12);
  }
  if (n == 0) return -1.0f;
  for (int i = 1; i < n; i++) {
    float k = v[i]; int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[n / 2];
}

int readLdrRaw() {
  long acc = 0;
  for (int i = 0; i < LDR_SAMPLES; i++) acc += analogRead(PIN_LDR);
  return (int)(acc / LDR_SAMPLES);
}

// --- display ---------------------------------------------------------------

void drawStatus(float dist, int ldr, float pitch) {
  if (!oledOk) return;
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print(F("S")); display.print(session);
  display.print(F("  L")); display.print(label);
  display.print(F(" ")); display.println(CLASS_NAMES[label]);
  display.println(streaming ? F("** RECORDING **") : F("idle"));
  display.print(F("rows ")); display.println(rowCount);
  display.print(F("dist "));
  if (dist < 0) display.println(F("---"));
  else        { display.print(dist, 0); display.println(F("mm")); }
  display.print(F("ldr  ")); display.println(ldr);
  display.print(F("pit  ")); display.println(pitch, 0);
  display.display();
}

void printState() {
  Serial.print(F("# session=")); Serial.print(session);
  Serial.print(F(" label="));    Serial.print(label);
  Serial.print(F(" ("));         Serial.print(CLASS_NAMES[label]);
  Serial.print(F(") streaming=")); Serial.print(streaming ? F("yes") : F("no"));
  Serial.print(F(" rows="));     Serial.print(rowCount);
  Serial.print(F(" dropouts=")); Serial.println(dropouts);
}

// --- commands --------------------------------------------------------------

void handleCommand(const String& cmd) {
  if (cmd.length() == 0) return;
  char c = toupper(cmd.charAt(0));

  if (c == 'L' && cmd.length() > 1) {
    int v = cmd.substring(1).toInt();
    if (v >= 0 && v <= 5) {
      label = v;
      Serial.print(F("# label -> ")); Serial.print(v);
      Serial.print(F(" ")); Serial.println(CLASS_NAMES[v]);
    } else {
      Serial.println(F("# ERROR label must be 0-5"));
    }
  } else if (c == 'S' && cmd.length() > 1) {
    session = cmd.substring(1).toInt();
    Serial.print(F("# session -> ")); Serial.println(session);
  } else if (c == 'G') {
    streaming  = true;
    runStartMs = millis();
    rowCount   = 0;
    dropouts   = 0;
    Serial.println(F("# GO"));
    Serial.println(F("t_ms,dist_mm,ldr_raw,ax,ay,az,gx,gy,gz,pitch,roll,amag,label,session"));
  } else if (c == 'X') {
    streaming = false;
    Serial.print(F("# STOP rows=")); Serial.print(rowCount);
    Serial.print(F(" dropouts=")); Serial.println(dropouts);
  } else if (c == '?') {
    printState();
  } else {
    Serial.println(F("# ERROR unknown command. Use L<0-5> S<n> G X ?"));
  }
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) { }

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  analogReadResolution(12);

  Wire.begin();

  if (!IMU.begin()) {
    Serial.println(F("# FATAL IMU did not start - check BOARD_REV"));
  }
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    oledOk = true;
    display.setTextColor(SSD1306_WHITE);
  }

  Serial.println(F("# LightGuide Edge data logger ready"));
  Serial.println(F("# commands: L<0-5> set label | S<n> session | G go | X stop | ? state"));
  printState();
}

void loop() {
  // Commands are read whether or not we are streaming, so a run can be stopped
  // mid-capture if the physical condition changes (protocol control 4).
  static String buf;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (buf.length()) { handleCommand(buf); buf = ""; }
    } else if (buf.length() < 16) {
      buf += ch;
    }
  }

  unsigned long now = millis();
  if (now - lastSample < SAMPLE_PERIOD_MS) return;
  lastSample = now;

  float dist = readDistanceMm();
  int   ldr  = readLdrRaw();

  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
  if (IMU.accelerationAvailable())    IMU.readAcceleration(ax, ay, az);
  if (IMU.gyroscopeAvailable())       IMU.readGyroscope(gx, gy, gz);

  float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI;
  float roll  = atan2(ay, az) * 180.0f / PI;
  float amag  = sqrt(ax * ax + ay * ay + az * az);   // 1.0 g at rest; flags movement

  if (streaming) {
    rowCount++;
    if (dist < 0) dropouts++;

    Serial.print(now - runStartMs); Serial.print(',');
    Serial.print(dist, 1);          Serial.print(',');
    Serial.print(ldr);              Serial.print(',');
    Serial.print(ax, 4);            Serial.print(',');
    Serial.print(ay, 4);            Serial.print(',');
    Serial.print(az, 4);            Serial.print(',');
    Serial.print(gx, 2);            Serial.print(',');
    Serial.print(gy, 2);            Serial.print(',');
    Serial.print(gz, 2);            Serial.print(',');
    Serial.print(pitch, 2);         Serial.print(',');
    Serial.print(roll, 2);          Serial.print(',');
    Serial.print(amag, 4);          Serial.print(',');
    Serial.print(label);            Serial.print(',');
    Serial.println(session);
  }

  static unsigned long lastDraw = 0;
  if (now - lastDraw > 500) { lastDraw = now; drawStatus(dist, ldr, pitch); }
}
