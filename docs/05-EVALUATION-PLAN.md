# 05 — Evaluation plan (20% of CW2)

Rubric target: "deep and elegant … includes, deeply and extensively, **all aspects of
performance** … **extensive, specific** considerations of need for further work", and for
80%+: "results demonstrate the innovative solution **has advanced compared to related works**."

The handbook requires evaluation **"both off and online"**. Online evaluation is the piece
most projects omit; it is where this project separates itself.

---

## 1. Offline evaluation

| Metric | Why it is here |
|---|---|
| Accuracy | Expected, but never reported alone |
| **Macro-F1** | Headline. Weights all five classes equally |
| Per-class precision / recall | Shows *which* class fails, which drives the failure analysis |
| Confusion matrix (normalised) | One per model |
| 5-fold CV mean ± std | Shows stability, not a lucky split |
| Held-out session score | The honest number. Expect it lower than CV — explain the gap |
| float32 vs INT8 | The cost of deployment |
| Model size (KB) | Flash budget |
| Training time | Minor, but completes the picture |

**Report the CV/held-out gap rather than hiding it.** A drop from CV to a held-out session
is exactly what session-wise splitting is designed to expose, and the size of the gap
quantifies how much the model depends on session-specific conditions. Explaining that gap is
a more sophisticated result than a suspiciously high single number, and markers know it.

---

## 2. Online evaluation (the differentiator)

Everything below is measured on the device, in the room, with the model deployed.

### 2.1 Live confusion matrix
**Protocol:** 20 trials × 6 classes = **120 live runs**. For each trial: physically stage the
condition, wait for the reading to settle, record what the device says against the ground
truth you staged. Log via serial to `data/online/live_trials.csv`.

Then put the offline and online confusion matrices **side by side on one slide**. Any class
that degrades between them is a finding, and the explanation for each degradation is the
"deep analysis" the rubric asks for.

### 2.2 Latency
- Inference time via `micros()` around `run_classifier()` — report mean, p95, max over ≥500 inferences.
- **End-to-end sense→display latency** — the number a user actually experiences, which
  includes the ~60 ms ultrasonic ping and the OLED I²C write. Report both, and be explicit
  that the second is the honest one. Reporting only inference time is a common overclaim.
- Throughput in inferences/second.

### 2.3 Memory footprint
From the actual build, not the estimate:
```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble firmware/03_inference
```
Report flash used / 1 MB and RAM used / 256 KB, split into model vs firmware where possible,
plus the tensor arena size and the headroom left.

### 2.4 Power (best effort)
Measure current draw with a USB power meter or a shunt if available; otherwise estimate from
the datasheet figures and **label it clearly as an estimate**. An honest estimate labelled as
such beats an unsourced precise-looking number.

### 2.5 Robustness probes
Short, cheap experiments that produce disproportionately good slide content:
- **Ambient light sweep** — does accuracy hold under room light vs daylight vs dark?
- **Surface variation** — ultrasonic against fabric, a softbox, a bare wall.
- **Re-mount test** — unclamp, re-clamp, re-run. Does it survive being physically moved?
- **Novelty test** — walk in front of the sensor and confirm the autoencoder fires
  `UNKNOWN SETUP` rather than a confident wrong class. **This is the money shot for the demo
  video.** It is the visible payoff of the M1+M2 architecture and it takes five seconds to
  show.

### 2.6 Task-level evaluation
Time taken to restore a disturbed setup to `optimal`, with and without the device, n≥5 per
condition. Even a tiny sample answers the question the whole project exists to answer — does
it actually help? Report the mean and the spread, and be upfront that n=5 supports a
direction, not a claim of significance.

---

## 3. Comparison against related work

The 80%+ band requires showing the solution "has advanced compared to related works". Build
one table:

| Work | Sensors | Platform | Task | Accuracy | Latency | On-device? |
|---|---|---|---|---|---|---|
| *(3–5 rows from `09-REFERENCES.md`)* | | | | | | |
| **LightGuide Edge** | US + LDR | nRF52840 | 5-class setup quality | *measured* | *measured* | Yes |

The honest framing: no directly comparable published system exists for *physical lighting
setup guidance* — that gap is itself the contribution. Compare instead against adjacent
TinyML multi-sensor classification work on comparable hardware, and be explicit that the
comparison is by analogy on platform and method rather than on task. Claiming a like-for-like
win against a differently-scoped paper is the kind of overclaim that gets picked apart in the
Q&A; naming the limits of the comparison is what a first-class answer looks like.

---

## 4. Failure analysis

For every misclassification in the online trials, record the condition and the predicted
class, then group them. Expected failure modes, to be confirmed or refuted by the data:

- `too_far` vs `underlit` — physically coupled through the inverse-square law; the confounded
  training samples exist specifically to break this tie.
- `optimal` vs `too_far` near the tolerance boundary — where the label
  itself is genuinely ambiguous.
- Ultrasonic dropouts against soft or angled surfaces.

**Put the failure cases on a slide with a photograph of the physical condition that caused
them.** Concrete failure analysis reads as mastery. Vague "some misclassifications occurred"
reads as a 2:1.

---

## 5. Future work — specific, not generic

The rubric wants "extensive, **specific** considerations of need for further work". Generic
future work ("more data, better models") scores nothing. Each item below must be tied to a
measured limitation:

1. **Replace the LDR with a spectrally-aware sensor** (TCS34725 / on-board APDS9960 colour
   channels) — driven by the measured error under different colour-temperature sources.
2. **Temperature-compensate the ultrasonic** using the **LPS22HB's on-chip temperature
   channel** — driven by the measured drift, if any. (Not the HTS221: it is not fitted on
   the Sense *Lite*. Verified by bus enumeration, `docs/02-HARDWARE.md` §1.)
3. **Add magnetometer yaw** to close the pan blind spot named in `02-HARDWARE.md` §6.
4. **On-device learning of the reference setup** — few-shot personalisation per studio,
   driven by the observed session-to-session gap.
5. **Whatever the ablation shows** — if the IMU adds little, propose a cheaper two-sensor
   variant and quantify the saving.
6. **BLE companion app** — CW1's stated future scope; out of CW2 scope by design.

---

## 6. Outputs

```
reports/offline_results.md
reports/online_results.md
reports/ablation.md
reports/failure_analysis.md
reports/comparison_related_work.md
reports/figures/*.png
data/online/live_trials.csv
```

Every number that appears on a slide must be traceable to one of these files. If it is not
in `reports/`, it does not go on a slide.
