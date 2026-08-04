# Gate G0 — hardware acceptance

**Date:** 31 July 2026 · **Student:** Vishnu Vekariya B00969091
**Test:** `firmware/01e_acceptance` · **Raw output:** `gate_G0_acceptance.txt`
**Board:** Arduino Nano 33 BLE Sense **Lite** (nRF52840, LSM9DS1 / Rev1-class IMU), COM4

---

## Result

| Component | Pin(s) | Verdict | Evidence |
|---|---|---|---|
| OLED SSD1306 | A4 / A5 | ✅ PASS | ACKs at 0x3C, driver initialised |
| IMU LSM9DS1 | internal `Wire1` | ✅ PASS | 119 Hz, \|a\| = 0.946 g at rest |
| APDS9960 | internal `Wire1` | ✅ PASS | live ambient = 114 |
| HC-SR04 | D2 / D3 | ✅ PASS | **0% dropouts** over 100 samples, 152–167 mm |
| LDR — range | A0 | ✅ PASS | resting 340/4095, not clipped |
| Switch | D7 | ✅ PASS | 16 debounced presses |
| External LED | D8 | ✅ PASS | confirmed blinking by operator |
| Buzzer | D9 | ✅ PASS | confirmed audible by operator |
| **LDR — light response** | A0 | ⚠️ **MARGINAL** | swing 255 counts (229–484) = 6% of scale |

**9 of 9 components electrically working. One component *value* needs changing.**

---

## The open item

The LDR responds to light correctly — 229→484 when covered — so the circuit is sound.
The problem is proportioning, not wiring.

From the measured resting point: 340 counts → 0.274 V across a 1 kΩ pulldown implies
**R_LDR ≈ 11 kΩ** in the working light level. The partner resistor should be of the same
order, so 10 kΩ is the correct choice and 1 kΩ compresses everything into the bottom of
the range.

| Pulldown | Resting | Swing when covered |
|---|---|---|
| 1 kΩ (fitted) | 340 (8% of scale) | **255 counts** |
| **10 kΩ (required)** | ~1950 (48% of scale) | **~1580 counts** |

**Roughly 6× more usable signal.** This matters directly to the classifier: `underlit`,
`optimal` and `overlit` are separated along the light axis, and compressing all three into
6% of the ADC range weakens their separability — an effect that worsens after INT8
quantisation and would be invisible once the dataset had been collected.

**Action:** replace the 1 kΩ (brown·black·red) with the 10 kΩ (brown·black·orange),
re-run `01e_acceptance`, and confirm `LDR response PASS`.

---

## Why this test exists

The LDR channel *worked*. It responded to light, produced plausible numbers, and any
informal check would have passed it. Only a threshold on dynamic range revealed that it was
using 6% of the available scale.

Catching it here costs one resistor. Catching it after Saturday's collection would cost the
entire dataset — and it would most likely never have been caught at all, appearing instead
as an unexplained ceiling on model accuracy.

This is the argument for acceptance gates with numeric criteria over "it seems to work",
and it belongs on the data-methodology slide.

---

## Faults found and fixed during bring-up

Recorded because each one is a genuine engineering finding, and the viva rewards being able
to explain them (`docs/10-QA-DEFENCE.md`).

1. **One rail fault presented as three separate failures.** The breadboard `−` rail was
   carrying 3.3 V, not ground. That single wire produced a dead OLED, a dead LDR divider,
   and a resistor that appeared to be the wrong value. Hours went into chasing the symptoms
   individually; measuring the rail found the cause in seconds.
2. **A library's `begin()` returning `true` is not evidence a device exists.**
   `Adafruit_SSD1306::begin()` allocates its buffer and pushes init commands without
   checking for an ACK, so it reported a healthy display on an empty bus. Presence is now
   verified by a real bus transaction before the driver is trusted.
3. **`pulseIn()` does not honour its timeout on the mbed core.** A missing ultrasonic echo
   stalled the sample loop from 100 ms to 3.7 s, silently destroying the 10 Hz rate the
   entire data protocol depends on. Replaced with bounded manual `micros()` timing.
4. **On-board sensors live on `Wire1`, not `Wire`.** Scanning only the external bus made the
   board look empty and sent the diagnosis down the wrong path.
5. **`PIN_LED` is already defined by the board core** as `(13u)`; using it as a variable
   name produces a compiler error pointing at the core's header rather than the sketch.
