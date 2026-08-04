# 02 — Hardware, pin map and safety

## 1. Bill of materials (as physically present)

| Item | Part | Role | Notes |
|---|---|---|---|
| MCU | **Arduino Nano 33 BLE Sense Lite** (nRF52840, Cortex-M4F @64 MHz, 1 MB flash / 256 KB RAM, u-blox NINA-B306) | Compute + inference | 3.3 V logic, **NOT 5 V tolerant**. Ships **castellated** — headers must be soldered on. |
| Distance | **HC-SR04** ultrasonic | Stand-to-subject distance | See §3 — voltage hazard |
| Brightness | **LDR** photoresistor | Incident light level | Needs a divider resistor; needs calibration |
| Tilt | **On-board IMU** (LSM9DS1 on Rev1 / BMI270+BMM150 on Rev2) | Head/stand angle | No wiring; revision detected by firmware |
| Display | **SSD1306 0.96" I²C OLED** | Corrective feedback | 4-wire I²C |
| Status | On-board LED / RGB | State indication | Built in |
| Alert | Piezo buzzer | Audible warning | **Not present in the current build** — see `AGENTS.md` §11.4 |

**On-board sensors — enumerated from the hardware, not the datasheet** (`firmware/00d_sensor_inventory`,
31 July). They sit on the internal I²C bus `Wire1`, **not** on A4/A5:

| Address | Device | Present? |
|---|---|---|
| 0x6B / 0x1E | LSM9DS1 accel+gyro / magnetometer | ✅ |
| 0x39 | APDS9960 ambient light, RGB, proximity | ✅ live data confirmed |
| 0x5C | LPS22HB pressure **+ on-chip temperature** | ✅ |
| 0x5F | HTS221 temperature/humidity | ⛔ **not fitted on the Lite** |

Because the HTS221 is absent, any ultrasonic temperature compensation must take its
reading from the **LPS22HB's** temperature channel instead. The APDS9960 *is* present, so
it remains available both as the LDR cross-check and as the fallback distance channel — worth mentioning in the deck as a design consideration
(why an external LDR at all? because it can be positioned at the subject plane, where the
light actually lands, rather than at the device).

---

## 2. Canonical pin map

Wire the build to match this. If the current wiring differs, **rewire to this map** rather
than editing the code — one canonical map keeps firmware, docs and the deck consistent.

| Signal | Nano pin | Notes |
|---|---|---|
| LDR divider tap | **A0** | LDR from 3V3 to A0, 10 kΩ from A0 to GND (see §4) |
| HC-SR04 `TRIG` | **D2** | 3.3 V output — accepted by the module's trigger input |
| HC-SR04 `ECHO` | **D3** | **via divider** — see §3 |
| HC-SR04 `VCC` | **3V3** | **not VUSB** — see §3 |
| HC-SR04 `GND` | GND | common ground |
| OLED `SDA` | **A4** | I²C, **address 0x3C confirmed on hardware** |
| OLED `SCL` | **A5** | I²C |
| OLED `VCC` / `GND` | 3V3 / GND | most SSD1306 modules accept 3.3 V |
| Buzzer (+) | **D9** | PWM-capable. ⚠️ **The wiring probe found D9 floating** — the buzzer is physically on some other pin. Confirm and update this row. |
| IMU | internal I²C | no wiring; **LSM9DS1, board is Rev1** |

Verified against hardware on 31 July with `firmware/00_wiring_probe`: D2 and D3 are
connected, D4–D12 are floating. Run that sketch any time the wiring is in doubt — it
reports what each pin is actually attached to, and distinguishes "miswired" from
"unpowered".

---

## 3. ⚠️ HC-SR04 voltage hazard — read before powering up

The nRF52840 is a 3.3 V part and **its GPIO is not 5 V tolerant**. A standard HC-SR04 driven
from 5 V drives its `ECHO` pin to 5 V. Connecting that directly to D3 can permanently damage
the microcontroller — sometimes immediately, sometimes as a slow degradation that shows up
later as a flaky pin, which is far worse during a 9-day project.

> **DECIDED 31 July: take Option B — power the module from `3V3`.** Bring-up found the
> module unpowered (`AGENTS.md` §13). Note that **the Nano 33 BLE's `VUSB` pin is not
> connected to USB 5 V by default** — a solder bridge underneath the board must be closed
> first — so wiring VCC to VUSB delivers nothing. Running from 3V3 both fixes the power
> problem and removes the 5 V hazard entirely.

**Pick one of these three. Do not skip this.**

**Option A — HC-SR04P (preferred if you have one).** The "P" variant is specified for
3.0–5.5 V. Power it from 3V3 and connect ECHO directly. Check the module's silkscreen.

**Option B — run a standard HC-SR04 from 3.3 V.** Many standard modules still work,
with reduced maximum range and slightly noisier readings. This is acceptable here — our
working range is ~20–200 cm, well inside what a 3.3 V-fed module manages. **Characterise
the reduced range during calibration and state it in the deck** as a design trade-off.

**Option C — 5 V supply with a divider on ECHO.** If range at 3.3 V proves inadequate:

```
HC-SR04 ECHO ──[ 1 kΩ ]──┬── Nano D3
                         │
                     [ 2 kΩ ]
                         │
                        GND
```

5 V × 2/(1+2) = **3.33 V**. Any ~1:2 ratio works (2.2 k/3.9 k etc.). `TRIG` needs no
shifting — the module reads 3.3 V as a valid high.

**Verify before extended use:** with the module powered and idle, measure ECHO to GND with a
multimeter, then during a ping. It must never exceed 3.3 V.

---

## 4. LDR divider

An LDR is a variable resistor, not a voltage source. It needs a fixed partner resistor to
form a divider:

```
3V3 ──[ LDR ]──┬── A0
               │
          [ 10 kΩ ]
               │
              GND
```

With this orientation the ADC reading **rises with light**. A 10 kΩ partner suits a common
GL5528 (≈10–20 kΩ at moderate indoor light, dropping to hundreds of Ω in bright light),
placing the response near mid-scale for studio levels. If readings saturate at the top or
bottom of the range under your actual lighting, swap the partner resistor and re-record the
calibration — **note the value you used in the deck**, because it defines the operating range.

The Nano's ADC is 10-bit by default (0–1023). `analogReadResolution(12)` gives 0–4095 and
a little more headroom for the model; the firmware sets 12-bit and the docs assume it.

**Calibration** (P1 gate, closes the open issue CW1 raised) — `tools/calibrate.py`:

A CdS photoresistor follows a power law, `R = A · E^(−γ)`, with γ typically 0.5–0.9. What
the calibration recovers is γ and A, and it does so **without a lux meter**: with lamp
output held fixed, illuminance follows the inverse-square law, so distance measured with a
tape measure is the reference. The cell sees lamp *plus* room, so a lamp-off reading is
taken at every distance and the ambient level is fitted as a free parameter — working with
the ratio `R_off/R_on` cancels the unknown scale factor and leaves γ and `E_amb`:

```
log(R_off / R_on) = γ · log(1 + E_lamp / E_amb)
```

Fitting against the lamp term alone instead biases γ low, because the response flattens at
distance where the room dominates. The on-board APDS9960 records the same sweep as an
independent cross-check. The fitter was validated against a simulated cell with a planted
exponent: worst-case error 0.009 over a grid of γ, ambient level and noise.

LDRs are also slow (tens to hundreds of ms) and temperature-sensitive — both are legitimate
limitations to name on the evaluation slide.

---

## 5. Startup checklist

1. Nothing powered. Verify the divider resistors are physically in place (§3, §4).
2. Confirm GND is common between the Nano, HC-SR04, LDR and OLED.
3. Confirm the OLED is on **A4/A5**, not on D-pins.
4. Power via USB. The green power LED should be steady.
5. Flash `firmware/01_sensor_check`. It runs an I²C scan, prints the detected IMU revision
   and streams all three sensor channels.
6. Sanity-check against reality: put a wall at 50 cm and confirm ~500 mm; cover the LDR and
   confirm the reading collapses; tilt the board and confirm the accelerometer axes respond.

**A sensor that reads plausibly is not the same as a sensor that reads correctly.** The
calibration step on Day 1 is what turns one into the other, and it is worth marks.

---

## 6. Known hardware limitations (say these out loud in the deck)

- **HC-SR04 beam angle is ~15°** and it returns the *nearest* echo in that cone — it measures
  "something in front", not specifically the subject. Soft or angled surfaces absorb or
  deflect the pulse.
- **Ultrasonic speed of sound varies with temperature** (~0.6 m/s per °C). The **LPS22HB's
  on-chip temperature channel** can compensate — *not* the HTS221, which is not fitted on
  the Lite variant. Whether we compensate is a design decision worth stating either way.
- **LDR is uncalibrated, slow, and spectrally non-flat** — it responds differently to
  tungsten, LED and daylight-balanced sources at the same lux. This is a genuine limitation
  of the sensor choice and a good future-work item (a TCS34725 or the on-board APDS9960
  colour channels would address it).
- **No absolute orientation reference** — the IMU gives tilt relative to gravity, so yaw
  (pan) is not observable without the magnetometer. Rev2 has one; Rev1's LSM9DS1 does too.
  Pan-only misalignment is therefore a known blind spot.
- **3.3 V operation reduces ultrasonic range** if Option B is taken.
