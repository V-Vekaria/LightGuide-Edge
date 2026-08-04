/*
 * LightGuide Edge - 00d_sensor_inventory
 * COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University
 *
 * The board silkscreen reads "NANO 33 BLE SENSE LITE". Lite variants ship a
 * REDUCED on-board sensor set, so which sensors exist is a question to answer by
 * measurement, not by reading the product page. This matters concretely:
 * tools/calibrate.py uses the APDS9960 ambient channel as an independent
 * cross-check on the LDR, and 03-DATA-PROTOCOL.md lists the APDS proximity channel
 * as the fallback if the ultrasonic cannot be recovered. Both assume a chip that
 * may not be fitted.
 *
 * KEY DETAIL: on the Nano 33 BLE family the on-board sensors do NOT sit on the
 * A4/A5 pins. They are on a separate internal I2C bus, exposed as Wire1. Scanning
 * only Wire is why the earlier scan came up empty - it was looking at the wrong
 * bus entirely.
 *
 * Expected addresses:
 *   0x6B  LSM9DS1 accel/gyro      (Sense Rev1)
 *   0x1E  LSM9DS1 magnetometer
 *   0x68/0x69  BMI270             (Sense Rev2)
 *   0x10  BMM150 magnetometer     (Sense Rev2)
 *   0x39  APDS9960 light/proximity/gesture
 *   0x5F  HTS221 temperature/humidity
 *   0x5C  LPS22HB pressure
 *   0x44  HS3003 temp/humidity    (Rev2)
 */

#include <Wire.h>
#include <Arduino_APDS9960.h>
#include <Arduino_LSM9DS1.h>

const char* identify(uint8_t a) {
  switch (a) {
    case 0x10: return "BMM150 magnetometer (Rev2)";
    case 0x1E: return "LSM9DS1 magnetometer (Rev1)";
    case 0x39: return "APDS9960 light/proximity/gesture";
    case 0x44: return "HS3003 temp/humidity (Rev2)";
    case 0x5C: return "LPS22HB pressure";
    case 0x5F: return "HTS221 temp/humidity (Rev1)";
    case 0x68:
    case 0x69: return "BMI270 IMU (Rev2)";
    case 0x6B: return "LSM9DS1 accel/gyro (Rev1)";
    case 0x3C:
    case 0x3D: return "SSD1306 OLED (external)";
    default:   return "unknown";
  }
}

// Probe with a real payload; a zero-length write is not put on the bus by the
// mbed core, which is what made the first scan useless.
bool present(TwoWire &bus, uint8_t addr) {
  bus.beginTransmission(addr);
  bus.write((uint8_t)0x00);
  if (bus.endTransmission() == 0) return true;
  return bus.requestFrom((int)addr, 1) > 0;
}

int scan(TwoWire &bus, const char* label) {
  Serial.print(F("--- ")); Serial.print(label); Serial.println(F(" ---"));
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (present(bus, a)) {
      found++;
      Serial.print(F("  0x"));
      if (a < 16) Serial.print('0');
      Serial.print(a, HEX);
      Serial.print(F("  "));
      Serial.println(identify(a));
    }
  }
  if (!found) Serial.println(F("  (nothing responded)"));
  Serial.println();
  return found;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 8000) { }

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F(" ON-BOARD SENSOR INVENTORY"));
  Serial.println(F("=================================================="));
  Serial.println();

  Wire.begin();
  Wire1.begin();

  scan(Wire,  "Wire  (A4/A5 - external devices)");
  scan(Wire1, "Wire1 (internal - on-board sensors)");

  // Library-level checks are the authoritative answer: a chip can ACK on the bus
  // and still fail to initialise.
  Serial.println(F("--- library init checks ---"));

  Serial.print(F("  LSM9DS1 IMU  : "));
  Serial.println(IMU.begin() ? F("OK - present and working") : F("FAILED"));

  Serial.print(F("  APDS9960     : "));
  if (APDS.begin()) {
    Serial.println(F("OK - present"));
    Serial.println(F("    -> ambient-light cross-check in calibrate.py IS available"));
    Serial.println(F("    -> proximity fallback for distance IS available"));
    delay(200);
    unsigned long t0 = millis();
    while (!APDS.colorAvailable() && millis() - t0 < 2000) delay(10);
    if (APDS.colorAvailable()) {
      int r, g, b, a;
      APDS.readColor(r, g, b, a);
      Serial.print(F("    -> live reading  r=")); Serial.print(r);
      Serial.print(F(" g=")); Serial.print(g);
      Serial.print(F(" b=")); Serial.print(b);
      Serial.print(F(" ambient=")); Serial.println(a);
    } else {
      Serial.println(F("    -> begin() succeeded but no colour data arrived"));
    }
  } else {
    Serial.println(F("NOT PRESENT"));
    Serial.println(F("    -> NO independent cross-check for the LDR calibration."));
    Serial.println(F("       calibrate.py already degrades gracefully, but the"));
    Serial.println(F("       limitation must be stated in the evaluation slide."));
    Serial.println(F("    -> NO proximity fallback if the HC-SR04 stays dead."));
  }

  Serial.println();
  Serial.println(F("=== inventory complete ==="));
}

void loop() {
  delay(10000);
}
