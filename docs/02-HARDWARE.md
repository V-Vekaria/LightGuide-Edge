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

### 2.0 Physical pin positions — count, don't guess

Pin *names* are not printed on the top of the board, so counting position from the USB end
is the reliable way to find a pin. Position 1 is the pin nearest the USB connector.
Identical across the whole Nano 33 BLE family.

| Pos | LEFT side (USB at top) | | Pos | RIGHT side |
|---|---|---|---|---|
| 1 | D13 | | 1 | D12 |
| **2** | **3V3 ← power for everything** | | 2 | D11 |
| 3 | AREF | | 3 | D10 |
| **4** | **A0 ← LDR tap** | | **4** | **D9 ← buzzer** |
| 5 | A1 | | 5 | D8 |
| 6 | A2 | | 6 | D7 |
| 7 | A3 | | 7 | D6 |
| **8** | **A4 ← OLED SDA** | | 8 | D5 |
| **9** | **A5 ← OLED SCL** | | 9 | D4 |
| 10 | A6 | | **10** | **D3 ← HC-SR04 ECHO** |
| 11 | A7 | | **11** | **D2 ← HC-SR04 TRIG** |
| 12 | 5V ⚠️ see §3 | | **12** | **GND ← nearest ground** |
| 13 | RESET | | 13 | RESET |
| **14** | **GND** | | 14 | D1/RX |
| 15 | VIN | | 15 | D0/TX |

Two things worth knowing from this layout:

- **`GND` at right-side position 12 sits directly below D2/D3**, so the HC-SR04's ground is
  a one-row jump from its signal pins. Use that one, not the left-side GND.
- **`3V3` is left-side position 2**, right next to the USB end — an easy pin to miscount to,
  because position 1 (D13) is immediately above it.

### 2.0b Full build sheet — rails first

Wire the power rails before anything else, then every module takes power from the rails
instead of competing for the Nano's single 3V3 and GND pins.

**Step 1 — rails**

| Wire | From | To |
|---|---|---|
| Red | `3V3` (left, pos 2) | breadboard **+ rail** |
| Black | `GND` (right, pos 12) | breadboard **− rail** |

⚠️ Breadboard rails are frequently **split at the midpoint**. Bridge both halves of each
rail if modules are spread along the board — a jumper landing in the unfed half is dead,
and it looks identical to a wiring mistake.

**Step 2 — HC-SR04** ✅ *verified working 31 July, 0% dropouts*

| Wire | To | Position |
|---|---|---|
| `VCC` | + rail | — |
| `GND` | − rail | — |
| `TRIG` | `D2` | right, 11th from USB |
| `ECHO` | `D3` | right, 10th from USB |

**Step 3 — LDR divider**

| Wire | To | Position |
|---|---|---|
| LDR leg 1 | + rail | — |
| LDR leg 2 **+** resistor leg 1 | `A0` | left, 4th from USB |
| Resistor leg 2 | − rail | — |

**Use 10 kΩ.** ⚠️ A note on how this was got wrong, because it is a useful lesson: A0 was
observed pinned at 4091/4095 with the 10 kΩ fitted, and the 10 kΩ was blamed. The real
cause was the breadboard `−` rail sitting at 3.3 V (§13.4 of `AGENTS.md`) — the divider had
no ground, so *no* resistor value could have worked. **A symptom was diagnosed instead of
its cause, and a correct component was replaced.**

Once ground was fixed, the LDR measured ≈10.8 kΩ in normal room light, which makes 10 kΩ
the right partner:

| Pulldown | Predicted A0 in room light | Verdict |
|---|---|---|
| 1 kΩ | ~350 counts (9% of scale) | too small — cramped at the bottom |
| **10 kΩ** | **~1970 counts (48% of scale)** | **use this — near mid-scale, swings both ways** |
| 4.7 kΩ | ~1050 counts | usable fallback |

Bands: 10 kΩ = brown · black · orange.

**Step 4 — OLED** *(demo only, not needed for the dataset)*

**This module's silkscreen order, read off the hardware 31 July: `VCC · GND · SCL · SDA`.**
Note **SCL comes before SDA** — the reverse of what most tutorials assume. Wiring the 3rd
pin to A4 and the 4th to A5 swaps the bus and the display never answers.

| Module pin | Position on header | To | Nano position |
|---|---|---|---|
| `VCC` | 1st | `3V3` | left, 2nd from USB |
| `GND` | 2nd | `GND` | right, 12th from USB |
| `SCL` | 3rd | **`A5`** | left, 9th from USB |
| `SDA` | 4th | **`A4`** | left, 8th from USB |

⚠️ **Wire these directly to Nano pins, not via the breadboard rails.** On 31 July the
`−` rail measured **3.30 V, not ground** (`AGENTS.md` §13.4). A module with 3.3 V on both
VCC and GND has no potential difference across it and cannot power up — the same fault
plausibly explains both the dead OLED and the dead LDR divider.

**Step 5 — buzzer, LED and switch** *(the feedback loop; needed for the Day-5 demo)*

These three sit on adjacent pins, so they form a tidy block on the right-hand side.

| Component | Connection | Pin | Position |
|---|---|---|---|
| Buzzer `+` | direct | `D9` | right, 4th from USB |
| Buzzer `−` | direct | `GND` | right, 12th |
| LED anode (**long leg**) | via **220 Ω** | `D8` | right, 5th from USB |
| LED cathode (short leg, flat notch) | direct | `GND` | — |
| Switch leg 1 | direct | `D7` | right, 6th from USB |
| Switch leg 2 (**diagonal**) | direct | `GND` | — |

- **The LED must have its 220 Ω** (red · red · brown). Direct to a GPIO it draws too much
  current and can damage the pin.
- **The switch needs no resistor** — firmware uses `INPUT_PULLUP`, so the switch simply
  shorts D7 to ground. This also avoids a floating input, which would trigger at random.
  On a 4-pin tactile switch the pins are internally paired; use **diagonally opposite**
  corners and the pairing cannot be got wrong.
- The **on-board RGB LED** (`LEDR`/`LEDG`/`LEDB`, active LOW) remains available with no
  wiring as a second status channel, and is used alongside the external LED.

Verify with `firmware/01d_output_test`, which blinks the LED, fades it via PWM, sounds the
buzzer at three frequencies, and reports debounced switch presses.

⚠️ **Do not name a variable `PIN_LED`** in any sketch — the board core already defines it as
`(13u)`, and the collision produces a confusing "expected unqualified-id" error pointing at
the core's own header rather than your code. Use `PIN_EXT_LED`.

### 2.1 Minimum wiring to start collecting data

Given the deadline, wire this much **first** and start the dataset. Only the ultrasonic and
the LDR feed the model; the OLED, buzzer and switch are output devices needed for the Day-5
demo, not for the Day-2 dataset. Seven connections unblock the critical path:

| From | To |
|---|---|
| HC-SR04 `VCC` | `3V3` (left, pos 2) |
| HC-SR04 `GND` | `GND` (right, pos 12) |
| HC-SR04 `TRIG` | `D2` (right, pos 11) |
| HC-SR04 `ECHO` | `D3` (right, pos 10) |
| LDR leg 1 | `3V3` |
| LDR leg 2 + 10 kΩ | `A0` (left, pos 4) |
| 10 kΩ other leg | `GND` |

**Use the on-board RGB LED instead of an external one.** `LEDR`/`LEDG`/`LEDB` are on
P0.24/P0.16/P0.06 and need no wiring at all — the external LED and its resistor can come
out of the build entirely. Every connection deleted is one that cannot fail. (Note the
on-board RGB LED is **active LOW** on this board: `digitalWrite(LEDR, LOW)` turns red *on*.)

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
| Buzzer (+) | **D9** ✅ | right side, position 4 |
| External LED | **D8** ✅ | right side, position 5, via 220 Ω |
| Switch | **D7** ✅ | right side, position 6, to GND, `INPUT_PULLUP` |
| Status LED | **on-board RGB** | `LEDR`/`LEDG`/`LEDB`, **no wiring**, active LOW |
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

**Source geometry constrains the sweep.** The inverse-square reference is *point-source*
physics. The rig light is a flat LED panel — an extended **area source** — whose falloff
approaches `1/d` close in and only settles to `1/d²` at roughly five times the panel's
largest dimension (≈1 m for a 20 cm panel). Sweep points inside that near field bias γ low,
so the default 30–200 cm ladder puts five of its nine points in the wrong regime.

Run the sweep out far enough that the clean regime has points to fit:

```bash
python tools/calibrate.py --port COM4 --distances 30 40 50 65 80 100 125 150 200 250 300
```

Fit γ on the far points; keep the near points as *measured evidence of the departure*. That
plot is the direct experimental backing for the area-source argument in
`docs/10-QA-DEFENCE.md` §1 — which that file already calls "the strongest single point" for
why a learned model beats thresholds — and it belongs on slide 6.

**Hold colour temperature fixed.** A CdS cell's spectral response is far from flat, so a
2000–10000 K adjustable panel changes the LDR reading at *identical* illuminance. Choose one
CCT (5600 K), record it on the session sheet, and never alter it — across calibration *and*
every collection session. Drift between sessions turns the session-held-out test set into a
different sensor regime and collapses the headline metric for a reason that is untraceable
after the fact. The **dimmer** is the exception: fixed during calibration, varied during
collection because it is the instrument that produces `underlit` and `overlit`.

LDRs are also slow (tens to hundreds of ms) and temperature-sensitive — both are legitimate
limitations to name on the evaluation slide. The slowness is helpful with a PWM-dimmed LED
panel: the cell integrates the chop, and the firmware's 16-sample average over ~32 ms
integrates it again. The APDS9960 is much faster, so if its readings look erratic at low
dimmer settings that is flicker, not a fault.

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
