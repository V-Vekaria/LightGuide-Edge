# Light Channel — Phase 2 design

**Date:** 6 August 2026
**Author:** Vishnu Vekariya (B00969091)
**Status:** approved, ready for implementation
**Builds on:** `2026-08-06-distance-guide-design.md` (Phase 1, complete)

Phase 1 shipped the distance channel. This adds the second and final check: whether the
lighting matches the saved setup. Tilt is not part of the product.

---

## 1. The sensor was chosen by measurement

`firmware/01g_light_compare` was written to decide between the external LDR and the on-board
APDS9960 on evidence. It did, and the evidence also uncovered a hardware fault.

### 1.1 The pulldown resistor was wrong

The LDR read a flat 343 counts and appeared dead. Working backwards through the divider,
343/4095 × 3.3 V = 0.276 V implies an LDR resistance of 10.9 kΩ against a 1 kΩ partner — and
`02-HARDWARE.md` §4 recorded the cell at 10.8 kΩ in room light, and predicted **~350 counts
with a 1 kΩ pulldown** versus ~1970 with the correct 10 kΩ.

The measurement matched the 1 kΩ prediction to within 2%. **The resistor swap that `RUN.md`
and `AGENTS.md` §11.6 flagged as blocking had never been carried out.** The sensor was never
broken; it had the wrong partner resistor.

After fitting the 10 kΩ:

| | 1 kΩ (before) | 10 kΩ (after) |
|---|---|---|
| Predicted resting level | ~350 | ~1970 |
| **Measured resting level** | **343** | **~1880** |
| Swing across a lamp sweep | 153 counts | **2,806 counts** |
| Percentage of ADC scale | 3.7% | **68.5%** |
| Clipping at either rail | — | **none** |
| Steady-state noise | ±3 | **±2 counts (0.05%)** |

Gate G0 failed this channel at 6% of scale (`LDR response FAIL`). It now measures 68.5% —
passed by more than eleven times the failing margin. **This closes the LDR issue CW1 raised
and CW2 was required to resolve.**

### 1.2 The LDR wins on evidence, not preference

Across the same 150-second sweep, with the APDS9960 deliberately boosted to 64× gain and a
~103 ms integration window so it was not judged while throttled:

| Sensor | Swing across the sweep |
|---|---|
| **LDR (external)** | 646 → 3452 — **2,806 counts** |
| APDS9960 (on-board) | 76 → 139 — **63 counts** |

The LDR tracked the lighting change; the on-board sensor essentially did not. The reason is
positional and is the argument `02-HARDWARE.md` §1 already made: the LDR sits **in the beam at
the subject plane**, the Nano sits off to one side. In an earlier sweep where light happened to
fall directly on the board, the same APDS gave a 440× range — its response depends on where the
controller is put, which disqualifies it for a rig that gets carried around.

**Decision: the external LDR on A0 is the light channel.** The APDS9960 remains available as an
independent cross-check for `tools/calibrate.py`.

---

## 2. Reading the LDR costs no loop time

Averaging the LDR over a 100 ms window, as `01g` does, would take the loop from 7.8 Hz to
about 4.4 Hz. That is too expensive.

The ultrasonic already spends **50 ms per loop idle**, sitting in `delay(PING_GAP_MS)` between
its five pings. The LDR is sampled in those gaps instead:

```
ping → sample LDR 10 ms → ping → sample LDR 10 ms → … ×5
     → median of 5 distances, mean of ~50 ms of light samples
```

Zero added loop time. It is also *better* than one contiguous window: five short windows spread
across 85 ms reject 100 Hz mains ripple and PWM dimmer chop more effectively than 50 ms taken in
a single block.

The ADC runs at 12-bit (`analogReadResolution(12)`), giving 0–4095.

---

## 3. One decision function, two vocabularies

### 3.1 The verdict enum becomes channel-neutral

`V_FAR` and `V_CLOSE` are meaningless for light. They are renamed:

```c
enum Verdict { V_NO_READ = 0, V_CORRECT, V_ABOVE, V_BELOW };
```

`V_ABOVE` means the live reading sits above the reference. Each channel renders it in its own
words:

| Verdict | Distance | Light |
|---|---|---|
| `V_CORRECT` | `CORRECT` | `CORRECT` |
| `V_ABOVE` | `FAR` | `BRIGHT` |
| `V_BELOW` | `CLOSE` | `DARK` |
| `V_NO_READ` | `NO ECHO` | `NO READ` |

More light gives higher ADC counts, so `V_ABOVE` correctly means brighter.

### 3.2 `decide()` takes its bands as arguments

```c
Verdict decide(float live, float ref, Verdict prev, float enterBand, float exitBand);
```

Still pure, still no Arduino API, still the single point where the Phase 3 classifier lands.
Distance passes fixed millimetre bands; light passes a percentage of its reference.

**This supersedes the three-argument form in the Phase 1 spec** (`…-distance-guide-design.md`
§4), where the bands were file-scope constants. The Phase 1 self-test cases are updated to the
new signature and the new enum names as part of this work — they must all still pass.

### 3.3 Constants

| Constant | Value | Meaning |
|---|---|---|
| `TOL_ENTER_MM` | 30 | distance: enter CORRECT within ±3 cm |
| `TOL_EXIT_MM` | 40 | distance: leave CORRECT beyond ±4 cm |
| `LIGHT_TOL_ENTER_PCT` | 0.05 | light: enter CORRECT within ±5% of reference |
| `LIGHT_TOL_EXIT_PCT` | 0.07 | light: leave CORRECT beyond ±7% of reference |

**Why a percentage for light and a fixed band for distance.** A 100-count deviation is 3% at a
reference of 3400 and 15% at 650 — a fixed band would be fussy in bright setups and sloppy in
dark ones for no principled reason. The LDR's response to illuminance is a power law, so
proportional tolerance is the physically honest choice.

**Why 5%.** At a typical reference near 2000 counts that is ±100 — roughly **17× the measured
±6 noise floor**, and comfortably tighter than the ~200-count gap measured between adjacent
dimmer settings, so a single notch on the dimmer registers as a deviation. This mirrors the
distance channel, where ±30 mm is 16σ.

Hysteresis works exactly as it does for distance: the exit band is wider than the enter band, so
a reading parked on the boundary cannot flip the verdict repeatedly.

---

## 4. References

### 4.1 Distance keeps a fixed default; light captures at boot

`DEFAULT_REF_MM = 1000` stays. 100 cm is the operator's real working distance, so it is a
meaningful constant.

**There is no equivalent constant for light.** The right ADC count depends entirely on the room,
the lamp and the LDR's placement — 2000 counts means nothing in the abstract. So the light
reference is captured from the first valid reading at power-up and printed to serial.

The principle is the same one that justified `DEFAULT_REF_MM`: the device must be useful the
instant it powers up. For distance that means a constant; for light it means self-referencing.
The button overrides both.

### 4.2 The button saves both channels together

Holding D7 for 2 s captures **both** references in one action — it is one setup, so it saves
atomically. Twenty samples: median for distance, mean for light.

The save is **refused**, leaving both previous references untouched, if either:

- fewer than 12 of the 20 samples returned a valid echo (as in Phase 1), or
- the captured light reference falls below 50 or above 4045 counts.

The second condition is new and deliberate: a reading pinned near either rail means a clipped
divider, which is precisely the fault diagnosed in §1.1. **This check would have caught it.**

---

## 5. Output

### 5.1 OLED — two halves, both always visible

```
DIST 100cm    now 104cm     <- size 1, y=0
      FAR                   <- size 2, y=9, centred
  [====|==  ]               <- 4 px bar, y=26
────────────────────────    <- rule, y=31
LGHT 2568     now 1840      <- size 1, y=33
      DARK                  <- size 2, y=42, centred
  [== |=====]               <- 4 px bar, y=59
```

Verdicts drop from size 3 to size 2 — 12 px per character, still clearly readable on camera.
`CORRECT` is 84 px of the available 128, so neither channel can clip.

Nothing is ever hidden. The whole system state fits in one video frame with no cuts.

### 5.2 Buzzer — distance first, then light

The operator sets a rig up in that order: position the stand, then dial the brightness. One
problem at a time.

Evaluated strictly in this order; the first matching rule wins:

| # | Condition | Sound |
|---|---|---|
| 1 | Distance is `V_NO_READ` | **silent** — no echo means no guidance to give |
| 2 | Distance is `V_ABOVE` / `V_BELOW` | distance pattern — 400 Hz / 80 ms every 600 ms (FAR), 1200 Hz / 60 ms every 200 ms (CLOSE) |
| 3 | Distance CORRECT, light is `V_NO_READ` | **silent** — a clipped divider is not a light verdict |
| 4 | Distance CORRECT, light `V_ABOVE` / `V_BELOW` | **double-blip** every 700 ms — two 40 ms tones 80 ms apart; 1000 Hz if BRIGHT, 500 Hz if DARK |
| 5 | Both CORRECT | one 1500 Hz, 150 ms chirp on entry, then silence |

Rules 1 and 3 are separate on purpose. An unreadable *distance* silences everything, because the
operator is being asked to move and the device cannot tell them where to. An unreadable *light*
silences only the light layer — the distance guidance above it has already been given.

The double-blip rhythm is what matters: it identifies the *channel* by ear before the operator
looks at the screen. Pitch then says which direction.

All timing remains scheduled from `millis()`. No `delay()` in the loop path.

### 5.3 On-board RGB LED

| State | Colour |
|---|---|
| Both CORRECT | green |
| Distance wrong | red |
| Distance CORRECT, light wrong | blue |

Active LOW, no wiring. Identifies the failing channel from across a room.

---

## 6. Serial

```
ref_mm,live_mm,diff_mm,dist_verdict,missed,ref_ldr,live_ldr,diff_ldr,light_verdict
```

Header printed once at boot and again when a host attaches. Reference changes and faults remain
`#` comment lines:

```
# LIGHT REFERENCE AUTO-SET 1874 counts
# REFERENCE SAVED 872 mm / 2103 counts
# SAVE REFUSED - not enough valid echoes
# SAVE REFUSED - light reading clipped (4095)
```

---

## 7. Error handling

| Condition | Behaviour |
|---|---|
| All 5 pings fail | distance `V_NO_READ`; light still evaluated and displayed |
| Light reading ≤ 50 or ≥ 4045 | light `V_NO_READ`, shown as `NO READ` — a clipped divider must not be reported as a verdict |
| Save gets < 12 valid echoes | refuse, error tone, both references survive |
| Save gets a clipped light reading | refuse, error tone, both references survive |
| OLED absent | `oledOk = false`; serial, buzzer and LED continue |
| Switch stuck LOW at boot | disabled for the run (as Phase 1) |

The Phase 1 rule still governs: **the device never states a verdict it cannot support.** The two
channels fail independently — a dead echo must not suppress a valid light verdict.

---

## 8. Test plan

**On-device self-test**, extended from 18 cases to at least 26. New cases cover: percentage
bands at three different reference levels (650, 2000, 3400), light hysteresis entering and
leaving CORRECT, `V_ABOVE`/`V_BELOW` polarity for light, and clipped-reading rejection.

**On hardware**, against the rig:

1. Distance channel still behaves exactly as Phase 1 — regression check.
2. Hold button: both references saved, both read CORRECT immediately after.
3. Dimmer up one notch → `BRIGHT`, blue LED, double-blip at 1000 Hz.
4. Dimmer down one notch → `DARK`, blue LED, double-blip at 500 Hz.
5. Cover the LDR → `DARK`, recovering when uncovered.
6. Move the target while light is correct → distance beeps take priority, light beeps resume once
   distance is CORRECT again.
7. Both correct → green LED, one chirp, then silence.
8. Attempt a save with the LDR unplugged → refused, both references survive.
9. Five minutes of live operation → no resets, loop rate still ≈7.8 Hz.

Measured values recorded to `reports/phase2_light_acceptance.md`.

---

## 9. Out of scope

- **Tilt / IMU** — dropped from the product. The two checks are distance and light.
- **Flash persistence** — references remain in RAM.
- **The classifier** — arrives at the `decide()` swap point in Phase 3.
- **Distance beam-ambiguity filter** — identified in the Phase 1 report §9, still not
  implemented, still worth doing.

---

## 10. Definition of done

- `firmware/03_inference` compiles and uploads.
- Self-test passes on-device with the extended case table.
- All nine hardware tests pass, with measured values recorded.
- Loop rate remains within 10% of the Phase 1 measured 7.8 Hz.
- `decide()` is still a pure function calling no hardware.
