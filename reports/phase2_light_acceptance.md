# Phase 2 — light channel acceptance

**Date:** 6 August 2026
**Sketch:** `firmware/03_inference` at commit `69dd142`
**Board:** Arduino Nano 33 BLE on COM4; HC-SR04 on D2/D3, LDR on A0 with a **10 kΩ** pulldown
**Spec:** `docs/superpowers/specs/2026-08-06-light-channel-design.md`

Evidence logs, all committed:

| Log | What it is |
|---|---|
| `data/calibration/light_channel_compare_20260806.csv` | LDR vs APDS9960, before the resistor fix |
| `data/calibration/light_channel_compare_boosted_20260806.csv` | as above, APDS boosted to 64× gain for a fair comparison |
| `data/calibration/light_sweep_10k_20260806.csv` | full lamp sweep after fitting the 10 kΩ |
| `data/online/phase2_live_2min_20260806.csv` | 2 min steady-state hold, both channels armed |

---

## 1. Decision self-test

```
DECISION SELF-TEST: 39/39 passed - OK
```

- **18 distance cases** — carried unchanged from Phase 1 through the enum rename, as a
  regression gate on the refactor.
- **14 light cases** — proportional bands at three reference levels (650, 2000, 3400 counts),
  both band edges, hysteresis entering and leaving CORRECT, and no-reading precedence.
- **7 validity cases** — the clipped-rail rejection.

The light cases were demonstrated to fail before they were made to pass. Setting
`LIGHT_TOL_ENTER_PCT` to 0.10 instead of 0.05 produced exactly the five predicted failures
(`34/39`); restoring it produced `39/39`. A test that has never failed proves nothing.

---

## 2. The light sensor was chosen by measurement, and a hardware fault was found doing it

### 2.1 The pulldown resistor was wrong, and had been since 31 July

The LDR read a flat 343 counts and appeared dead. Working backwards through the divider,
343/4095 × 3.3 V = 0.276 V implies a cell resistance of 10.9 kΩ against a **1 kΩ** partner.
`02-HARDWARE.md` §4 had measured the cell at 10.8 kΩ in room light and predicted **~350 counts
with 1 kΩ** versus ~1970 with the correct 10 kΩ.

The measurement matched the 1 kΩ prediction to within 2%. **The resistor swap that `RUN.md` and
`AGENTS.md` §11.6 both flagged as blocking had never been carried out.** The sensor was never
faulty — it had the wrong partner resistor, and the symptom looked identical to a dead sensor.

After fitting the 10 kΩ:

| | 1 kΩ (before) | 10 kΩ (after) |
|---|---|---|
| Predicted resting level | ~350 | ~1970 |
| **Measured resting level** | **343** | **~1880** |
| Swing across a lamp sweep | 153 counts | **2,806 counts** |
| Percentage of ADC scale | 3.7% | **68.5%** |
| Clipping at either rail | — | **none** |

Gate G0 failed this channel at 6% of scale (`LDR response FAIL`). It now measures 68.5% — passed
by more than eleven times the failing margin. **This closes the LDR issue CW1 raised and CW2 was
required to resolve.**

Both predictions — 350 and 1970 — were made from the datasheet model before the measurement.
Both landed within 5%. That is the divider model validated, not just a resistor changed.

### 2.2 The LDR beat the on-board sensor on evidence

Across the same 150-second sweep, with the APDS9960 deliberately boosted to 64× gain and a
~103 ms integration window so it was not judged while throttled by the library defaults:

| Sensor | Swing across the sweep |
|---|---|
| **LDR (external, A0)** | 646 → 3452 — **2,806 counts** |
| APDS9960 (on-board) | 76 → 139 — **63 counts** |

The LDR tracked the lighting change; the on-board sensor essentially did not, because it sits on
the Nano and the Nano sits off to one side of the beam. In an earlier sweep where light happened
to fall directly on the board, the same APDS gave a **440× range** — its response depends on
where the controller is placed, which disqualifies it for a rig that gets carried around.

**Decision: the external LDR is the light channel.** The APDS9960 remains available as an
independent cross-check.

---

## 3. Measured performance

From the 2-minute armed hold (`phase2_live_2min_20260806.csv`):

| Measurement | Value |
|---|---|
| Rows | 961 in 120 s → **8.01 Hz** |
| Dropped pings | **0 of 4,805 — 0.000%** |
| Reference held | 1026 mm / 2607 counts, unchanged across all 961 rows |
| Distance spread | 1020–1030 mm (10 mm) |
| **Light spread** | **2714–2718 counts (4 counts)** |
| Flash / RAM | **11% / 17%** |

**The light channel's noise floor is ±2 counts.** Against a reference of 2607, the ±5% tolerance
band is ±130 counts — **65× the noise**. For comparison the distance band is ±30 mm against a
0.6 mm σ, or 50×. Both channels have bands far wider than their noise, which is why neither
flickers.

### 3.1 The light channel costs no loop time

The LDR is sampled inside the 10 ms gaps the ultrasonic must leave between pings, rather than in
a window of its own. Measured loop rate with both channels running is **8.01 Hz**, against
7.85 Hz for Phase 1 with distance alone.

The light channel is therefore free, and the small variation between the two figures is not
attributable to it: loop time depends on **how far away the target is**, because sound takes
time to return. A target at 729 mm produced 8.95 Hz and one at 1269 mm produced 7.85 Hz — about
3 ms less echo wait per ping, five pings per loop, on a ~127 ms loop. "7.8 Hz" was never a fixed
property of the device.

---

## 4. Behaviour confirmed on the rig

Confirmed by the operator on the physical hardware. **These were verified visually and audibly,
not captured in a log** — the 2-minute recording is a steady-state hold, so it evidences the
noise figures above and not the verdict transitions.

| Test | Result |
|---|---|
| Boot into `UNSET` — no verdicts, no beeps, LED off, live readings shown | ✅ |
| Hold button 2 s saves **both** references atomically | ✅ |
| Two-row display, distance above, light below | ✅ |
| Brighter than reference → `BRIGHT` | ✅ |
| Dimmer than reference → `DARK` | ✅ |
| Both correct → green LED, one chirp, then silence | ✅ |
| Distance wrong → red LED, distance beep pattern | ✅ |
| Light wrong → blue LED, double-blip | ✅ |
| Both wrong → distance takes priority | ✅ |
| No text clipped, no overlap between halves | ✅ |

**An unplanned benefit:** the fitted OLED is a two-colour panel — the top band renders yellow and
the remainder blue. The distance block therefore draws yellow and the light block blue, giving
the two channels a free visual separation that reads well on camera. Not designed; worth keeping.

---

## 5. Limitations

1. **The subject must present a face roughly square to the sensor.** Established by controlled
   experiment during this phase — see `phase1_distance_acceptance.md` §10. An angled target
   deflects the pulse and the beam then measures background objects, with **no dropped pings**,
   so nothing in the data marks the reading as untrustworthy. This is the single most important
   operating constraint on the product.
2. **References are held in RAM.** A power cycle returns the device to `UNSET`. Flash
   persistence was designed (spec §9) and deliberately deferred — see §6.
3. **The LDR is spectrally non-flat and slow.** A CdS cell responds differently to tungsten, LED
   and daylight at identical illuminance, so the reference is only valid while the lamp's colour
   temperature is held fixed. Settling time was not separately characterised.
4. **The light reference is position-dependent.** Moving the LDR invalidates the saved setup as
   surely as changing the lamp does, and the device cannot distinguish the two.
5. **No temperature compensation** on the ultrasonic. Speed of sound varies ~0.6 m/s per °C; the
   LPS22HB's temperature channel is on the board and unused.
6. **The verdicts are thresholds, not a model.** This is the Phase 3 work.
7. **The verdict transitions are operator-confirmed, not logged.** A capture exercising all four
   verdicts would be stronger evidence and is worth recording during the demo video.

---

## 6. Deferred, with reasons

**Flash persistence** — writing the saved setup to nRF52840 flash so it survives a power cycle.
Fully designed: `UNSET`/`ARMED` exists precisely so persistence adds only "load at boot" and
"store on save", `Arduino_CRC32` is installed to checksum the block, and the mbed core provides
both `FlashIAP` and `KVStore`. **Deferred on marks, not on difficulty** — it is a genuine product
improvement that carries no weight in the assessment rubric, and machine learning, which carries
40%, was outstanding.

**Multiple saved setups** — recalling one of several stored configurations. One setup covers the
stated use case; multiple presets are future work.

---

## 7. Verdict

Phase 2 meets its definition of done:

- ✅ Compiles and uploads; 11% flash, 17% RAM
- ✅ Self-test passes 39/39 on-device at every boot
- ✅ All behaviour in §4 confirmed on hardware
- ✅ Loop rate 8.01 Hz, inside the ±10% budget
- ✅ `decide()` remains a pure function calling no hardware — still the single ML swap point

**Both checks the project promised — distance and light — are working on real hardware.**
