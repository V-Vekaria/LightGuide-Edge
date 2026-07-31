# 03 — Data collection methodology (15% of CW2)

The rubric asks for a "fully detailed, excellent methodology … providing ample high-quality
data … all elements explained and justified drawing on appropriate literature/experimentation",
with **creativity/innovation** for the top band. Quality controls are the marks here, not
sample count. Follow this document literally and the 15% is defensible.

---

## 1. What one sample is

| Field | Unit | Source |
|---|---|---|
| `t_ms` | ms | `millis()` since session start |
| `dist_mm` | mm | HC-SR04, median of 5 pings |
| `ldr_raw` | counts (0–4095) | A0, 12-bit ADC, mean of 8 reads |
| `ldr_lux` | lux (est.) | calibration curve applied off-device |
| `ax, ay, az` | g | IMU accelerometer |
| `gx, gy, gz` | °/s | IMU gyroscope |
| `pitch, roll` | ° | derived from accelerometer |
| `label` | 0–5 | set by the operator before the run |
| `session` | int | session id |

**Sampling rate: 10 Hz.** Chosen deliberately, not by default — the HC-SR04 needs ~60 ms
between pings to avoid hearing its own previous echo, and the LDR's response time is tens of
ms. 10 Hz sits comfortably above both while remaining faster than a human repositions a
light stand. *Justify this in the deck; "10 Hz because it seemed fine" is a 2:1 answer.*

---

## 2. Session design — the part that earns the marks

**Never collect all six classes in one continuous sitting and then split randomly.**
Consecutive 10 Hz samples are near-duplicates; a random split puts near-identical rows in
both train and test and inflates test accuracy dramatically. Session-wise splitting is the
honest alternative, and saying so demonstrates exactly the methodological understanding the
rubric is probing.

**Plan: three sessions, deliberately varied.**

| Session | Purpose | Deliberate variation |
|---|---|---|
| S1 | Train | Baseline: evening, room lights on, white wall, stand position A |
| S2 | Train | Daylight through window, different wall/backdrop, stand position B, device re-mounted |
| S3 | **Held-out test — never trained on** | Different room or time of day, re-mounted device, different reference setup |

Re-mounting the device between sessions matters: if the device is never moved, the model can
memorise a fixed mounting geometry and will collapse the moment it is clamped on again in the
defence room. Re-mounting is cheap insurance and is a legitimate "creativity in methodology"
point.

**Per class, per session: ~85 samples** → 3 × 85 ≈ 255 per class → **≥250/class, ≥1500 total**,
matching the CW1 promise. At 10 Hz that is ~8.5 s of capture per class per session — the
whole dataset is well under an hour of actual recording. The time cost is in setup and
repositioning, so budget the full day.

---

## 3. Defining the reference setup

Classes are relative to a reference, so the reference must be recorded, not remembered.

1. Place the light stand at the intended position. Record `dist_mm`, `ldr_raw`, `pitch`, `roll`.
2. Write them on the session sheet **and** photograph the physical setup with a tape measure in frame.
3. Define the `optimal` band: ±10% on distance, ±15% on brightness, ±5° on tilt.
   These tolerances are a design decision — state them and justify them against what a
   photographer can actually perceive (a ~15% light change is roughly a fifth of a stop,
   near the threshold of visible difference in a final image; a 10% distance change alters
   illuminance by ~20% under the inverse-square law, which *is* visible).

The inverse-square relationship is worth putting on a slide: illuminance falls with the
square of distance, so distance and brightness are **physically coupled**. That coupling is
precisely why a single-sensor threshold fails and a multi-sensor learned model is justified.
It is the strongest argument in the whole project for using ML at all — lead with it.

---

## 4. Class recipes

Produce each condition physically. Vary *how* you produce it within a class, so the model
learns the condition rather than one specific staging of it.

| Label | How to produce | Vary within class |
|---|---|---|
| `optimal` | Reference position, reference output | Small jitter inside the tolerance band |
| `too_close` | Move stand 15–40% nearer | Several distances across the range |
| `too_far` | Move stand 15–50% further | Several distances |
| `underlit` | Dim the light / add diffusion / partially block | Different dim levels and causes |
| `overlit` | Increase output / remove diffusion / add reflector | Different levels and causes |
| `tilt_off` | Angle the head 8–30° off aim | Both directions, pitch and roll |

**Confounded cases are mandatory.** Collect examples such as `too_far` *with the light
turned up* (reads bright but is mispositioned) and `too_close` *dimmed* (reads normal but is
mispositioned). Without these the model can settle on a brightness threshold and appear to
work — until the demo. Roughly 20% of each positional class should be confounded, and this
should be stated explicitly: it is the difference between a dataset that flatters the model
and one that tests it.

---

## 5. Quality controls (name every one of these in the deck)

1. **Warm-up discard** — drop the first 2 s of every run; the LDR settles and the first
   ultrasonic pings are unreliable.
2. **Median-of-5 ultrasonic, mean-of-8 ADC** — on-device filtering at the source.
3. **Range gate** — reject `dist_mm` outside 20–4000 mm as a failed echo, log the rejection
   rate. A reported dropout rate is evidence of rigour, not weakness.
4. **Operator label discipline** — the label is set *before* the run starts and the run is
   discarded if the condition changes mid-capture. No post-hoc relabelling.
5. **Session sheet** — one per session: date, time, room, ambient lux, light source type and
   power, reference values, anything anomalous. Template in `data/raw/SESSION_SHEET.md`.
6. **Photograph every physical setup.** These become slide material and they make the
   methodology auditable.
7. **Balance check** — `tools/dataset_report.py` fails loudly if any class is under quota.
8. **Blind spot-check** — after collection, plot 10 random samples per class and confirm they
   look like the class they claim to be. Mislabelled data is the most expensive error in the
   whole pipeline and the cheapest to catch here.

---

## 6. Ethics of the data

No human subjects, no images, no audio, no personal data — the dataset is physical sensor
readings of lighting equipment. That is a genuinely strong privacy position and it should be
stated, not assumed: the design *cannot* leak personal data because it never captures any.
This is a substantive contrast with camera-based or microphone-based alternatives, and it
directly addresses the "ethical dimensions" the rubric asks for.

If any photograph in the deck shows a person, get their consent and say so on the slide.

**If data is shared with or received from another student, the handbook requires this to be
declared during the data-collection section of the presentation.** Currently: no shared data.

---

## 7. Files produced

```
data/raw/session_<id>_<label>_<timestamp>.csv   one file per run
data/raw/SESSION_SHEET.md                       operator log
data/calibration/ultrasonic.csv                 known distance vs reading
data/calibration/ldr.csv                        reference lux vs ADC counts
data/processed/train.csv, test.csv              session-split, features extracted
data/processed/DATA_DICTIONARY.md               every column, unit, range, provenance
```

The dataset zip submitted to Blackboard must contain **both** raw and processed data plus
the data dictionary. Raw data alone makes the work unreproducible; processed data alone
makes it unverifiable.
