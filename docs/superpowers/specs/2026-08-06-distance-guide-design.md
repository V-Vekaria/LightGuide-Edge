# Distance Guide — Phase 1 design

**Date:** 6 August 2026
**Author:** Vishnu Vekariya (B00969091)
**Status:** approved, ready for implementation
**Scope:** distance channel only. Light is Phase 2 and is deliberately excluded.

---

## 1. What this is

The product tells the operator whether the rig is set up the same way it was last time.
It checks two things: **distance** and **light**. This spec covers distance only.

The device holds a **reference distance**. It compares the live ultrasonic reading against
that reference and gives one of three verdicts, on the OLED and through the buzzer:

| Verdict | Meaning | Operator action |
|---|---|---|
| `CORRECT` | live distance is inside the tolerance band | stop, you're set up |
| `FAR` | live distance is beyond the reference | move closer |
| `CLOSE` | live distance is inside the reference | move back |

A fourth internal state, `NO_ECHO`, covers the sensor returning nothing. It is not a verdict —
it is an honest "I cannot tell you", and it must never be silently rendered as one of the three.

### 1.1 Why threshold logic now, and why that is not the end state

The COM683 handbook requires machine learning (`docs/00-REQUIREMENTS-LOCKED.md` §3 — ML
implementation 20%, critical evaluation 20%). A threshold comparison contains no ML.

This phase is therefore built as **the scaffold the classifier drops into**, not as the
finished product. Every part of the sketch — sensing, state machine, OLED, buzzer, serial —
is the same in both versions. Only one function changes. See §4.

This is a sequencing decision, not a descoping one: it produces a working, demonstrable
device today, and reduces Monday's model integration to replacing a single function body.

---

## 2. Hardware used

All of it is already verified on the physical build (gate G0, 31 July).

| Part | Pin | Evidence |
|---|---|---|
| HC-SR04 `TRIG` / `ECHO` | D2 / D3 | 0% dropouts at 10 Hz |
| Piezo buzzer | D9 | confirmed audible at G0 |
| Tactile switch | D7, `INPUT_PULLUP` | 16 debounced presses |
| SSD1306 OLED | A4 / A5, addr 0x3C | confirmed on bus |
| On-board RGB LED | `LEDR`/`LEDG`/`LEDB` | no wiring, **active LOW** |

The HC-SR04 runs from 3V3 (hardware doc §3, Option B), so no level shifting is involved and
there is no 5 V hazard on D3.

No new hardware. No rewiring.

---

## 3. Structure

Built as `firmware/03_inference/03_inference.ino` — the sketch `RUN.md` already designates as
the product. One product sketch from the start avoids merging two sketches under deadline.

Four units, each with one job:

| Unit | Responsibility | Depends on |
|---|---|---|
| **Sensing** | `pingOnce()`, `readDistanceMm(&missed)` | HC-SR04 pins |
| **Reference** | `holdFired()`, `captureReference()`, `doSave()` | sensing |
| **Decision** | `decide(live, ref, prev) -> Verdict` | nothing — pure function |
| **Output** | `render()` OLED, `updateBuzzer()` / `updateLed()`, `emit()` serial | verdict + values |

The decision unit takes numbers and returns an enum. It touches no hardware, so it can be
reasoned about — and later replaced — without disturbing anything else.

### 3.1 Sensing

Lifted unchanged from `firmware/01i_distance_only`, which is already proven on this rig.

Five pings, 10 ms apart, **median** of the valid ones. Median rather than mean because
ultrasonic failures are wild outliers, not Gaussian noise — a single bad echo drags a mean
badly and a median ignores it entirely. Readings outside 20–4000 mm are discarded as invalid,
as are echoes that time out.

`readDistanceMm()` returns `-1` when every ping in the group failed, and reports the miss
count through an out-parameter so the display can distinguish "far away" from "not aimed at
anything".

### 3.2 One mode, one gesture

**Revised 6 August during implementation.** The original design had a separate SETUP mode
entered by a short press. It was dropped once the guide screen was running: that screen already
shows the live distance as `NOW xxxcm`, so the aiming display SETUP existed to provide was
redundant, and a second mode is a second thing to explain and to get wrong on camera.

There is now no mode. The device always guides, and the button always means one thing:

```
BOOT ──> guiding against DEFAULT_REF_MM (1000 mm)
             │  ^
   hold 2 s  │  │ save succeeds -> guide against the new reference
             v  │
          capture ┘
             │
             └──> too few valid echoes ──> error tone, old reference survives
```

**Boot loads `DEFAULT_REF_MM = 1000` (100 cm) immediately.** That is the operator's established
working distance, so the device is useful the instant it powers up and a faulty switch cannot
leave it unusable.

**Hold, not tap.** A tap is one accidental knock away from silently re-referencing to whatever
happens to be in front of the sensor, and the operator would not discover it until the verdicts
stopped making sense — during the video, most likely. Two seconds cannot happen by accident.

**Capturing a reference:** 20 readings over roughly 2 s, medianed, with a progress bar on the
OLED. If fewer than 12 return a valid distance the save is **refused**: the buzzer sounds a low
error tone, the screen says so, and the previous reference survives untouched. A reference
captured from a bad echo would corrupt every subsequent verdict while looking exactly like a
software fault, so it must fail loudly at the point of capture.

After a successful save the verdict history is cleared, so the new reference earns CORRECT from
scratch and chirps on arrival rather than sliding in silently.

---

## 4. The decision function

This is the single point at which the ML model will be introduced.

```c
enum Verdict { V_CORRECT, V_FAR, V_CLOSE, V_NO_ECHO };

// Phase 1
Verdict decide(float live_mm, float ref_mm);

// Phase 2 — same call site, same return type, same consumers
Verdict decide(const float *features);
```

Phase 1 logic:

```
if live_mm < 0            -> V_NO_ECHO
diff = live_mm - ref_mm
if |diff| within band     -> V_CORRECT
else if diff > 0          -> V_FAR
else                      -> V_CLOSE
```

### 4.1 Constants

| Constant | Value | Meaning |
|---|---|---|
| `DEFAULT_REF_MM` | 1000 | boot reference, 100 cm |
| `TOL_ENTER_MM` | 30 | must be within ±3 cm to *become* CORRECT |
| `TOL_EXIT_MM` | 40 | must exceed ±4 cm to *stop* being CORRECT |

At the default reference: **97–103 cm is CORRECT**, above 103 cm is FAR, below 97 cm is CLOSE.

±3 cm sits comfortably above the sensor's own noise — median-of-5 at 3.3 V settles to roughly
±1 cm on a flat target — while staying tight enough that reaching CORRECT reads as deliberate
rather than accidental.

### 4.2 Hysteresis

`TOL_ENTER_MM` and `TOL_EXIT_MM` differ on purpose. With a single threshold, standing at
exactly the boundary flips the verdict several times per second: the display flickers and the
buzzer chatters. The 10 mm gap between entering and leaving CORRECT removes it.

This is three lines of code and it is not cosmetic — boundary chatter is the most visible way
for a working device to look broken on camera.

---

## 5. Feedback

### 5.1 Buzzer (D9)

Rate encodes urgency; pitch encodes direction.

| Verdict | Tone | Duration | Repeat |
|---|---|---|---|
| `FAR` | 400 Hz | 80 ms | every 600 ms |
| `CLOSE` | 1200 Hz | 60 ms | every 200 ms |
| `CORRECT` | 1500 Hz | 150 ms | **once on entry, then silence** |
| `NO_ECHO` | — | — | silent |
| save refused | 200 Hz | 400 ms | once |

Silence means success. This is the parking-sensor convention, and it is also practical: a
continuous tone at CORRECT would cover the narration in a one-minute video.

All timing is scheduled from `millis()`. No `delay()` is used for beeps — blocking the loop to
sound a tone makes the display lag behind the sensor.

### 5.2 OLED

```
REF 100cm       NOW 112cm     <- size 1
       FAR                    <- size 3, centred
+12 cm  move closer           <- size 1
[======|===   ]               <- live position relative to reference
```

`CORRECT` at size 3 is 126 px of the 128 px width. If it clips on the hardware, that one word
drops to size 2; the layout is otherwise unchanged.

`NO_ECHO` replaces the verdict line with `NO ECHO` and the hint line with `aim at target`.

### 5.3 On-board RGB LED

Green at CORRECT, red otherwise, off at `NO_ECHO`. Active LOW. No wiring, and it is the output
that actually reads on camera from across a room.

---

## 6. Serial output

CSV at roughly 11 Hz (measured):

```
ref_mm,live_mm,diff_mm,verdict,missed
```

A header line is printed once at boot. Reference changes are marked by a `#` comment line
rather than a column, so the moment the operator re-referenced stays findable in the log:

```
# REFERENCE SAVED 872 mm
# SAVE REFUSED - not enough valid echoes
# switch stuck LOW - disabled for this run
```

This costs nothing to add now and is the log the **online evaluation** is computed from later —
`docs/00-REQUIREMENTS-LOCKED.md` §5 identifies online evaluation as the single most commonly
missing component of this coursework.

The self-test result is also reprinted the moment a serial host attaches. Booting takes less
time than uploading and re-enumerating the USB port, so the report in `setup()` otherwise goes
nowhere and the result is only observable if you win a race against the board.

---

## 7. Error handling

| Condition | Behaviour |
|---|---|
| All 5 pings fail | `NO_ECHO`; buzzer silent, LED off, OLED says `aim at target` |
| 2+ of 5 pings miss | verdict still given, OLED shows the miss count as a caution |
| Reference save gets < 12 valid readings | refuse, error tone, previous reference survives |
| OLED absent at boot | `oledOk = false`; serial and buzzer continue working |
| Switch reads LOW continuously for 3 s at boot | treated as faulty and disabled for the run, so it cannot spam mode changes. The boot reference is already loaded, so the device stays fully usable |

The unifying rule: **the device never states a verdict it cannot support.** A wrong CORRECT is
worse than an honest `NO ECHO`.

---

## 8. Test plan

Executed against a tape measure on the physical rig, not in simulation.

1. **Baseline** — power on. Confirm boot into GUIDE, `REF 100cm`, verdict tracks a target moved
   by hand.
2. **Three verdicts** — target at 100 cm → `CORRECT`, green LED, one chirp then silence.
   Move to 150 cm → `FAR`, slow low beeps. Move to 70 cm → `CLOSE`, fast high beeps.
3. **Band edges** — 103 cm and 97 cm are the last CORRECT positions; 105 cm and 95 cm are not.
4. **Hysteresis** — hold the target at exactly 103 cm for 15 s. The verdict must not flicker
   and the buzzer must not chatter.
5. **No echo** — cover the sensor. `NO ECHO` within one loop, buzzer silent, no crash, and
   recovery when uncovered.
6. **Re-reference** — place a target at 80 cm and hold D7 for 2 s. Confirm the SAVING progress
   bar, two rising chirps, `SAVED 80`, then `REF 80cm`, and that 80 cm now reads CORRECT while
   100 cm reads FAR.
7. **Refused save** — aim at open air and hold D7 for 2 s. Must refuse with the low error tone
   and `SAVE FAILED`, and the previous reference must survive unchanged.
8. **Endurance** — 5 minutes continuous. No resets, no stuck tone, no display corruption.

Each test records its observed values into `reports/` as evidence, in keeping with the project
convention that a gate is a binary fact with evidence attached.

---

## 9. Explicitly out of scope

Named so they are not quietly added:

- **Light channel** — Phase 2. The choice between the external LDR and the on-board APDS9960 is
  still open and is being decided by `firmware/01g_light_compare`.
- **Tilt / IMU** — dropped from the product. The two checks are distance and light.
- **Flash persistence** — the reference lives in RAM and is lost on reset. `DEFAULT_REF_MM`
  covers the normal case. Writing to nRF52840 flash is an untested dependency and is not worth
  the risk this close to the deadline.
- **The classifier** — arrives at §4's swap point once the model is trained.

---

## 10. Definition of done

- `firmware/03_inference` compiles and uploads to the Nano 33 BLE.
- All eight tests in §8 pass on hardware, with measured values recorded.
- Serial CSV is well-formed and parseable.
- `decide()` is a pure function of its arguments, calling no hardware.
