# 04 — Machine learning plan (20% of CW2)

Rubric target: "implementation of **a number of** suitable ML approaches; **full**
justification underpinned by relevant literature/experimentation; **full understanding of
deployment/implementation** with **no weaknesses**."

Four models, one fair comparison, deployment understood down to the byte.

---

## 1. Preprocessing

Applied identically to every model — otherwise the comparison is meaningless.

1. **Warm-up trim** — drop the first 20 samples (2 s) of each run.
2. **Range gating** — reject out-of-range ultrasonic reads; forward-fill up to 2 samples, discard longer gaps.
3. **Calibration mapping** — `ldr_raw → ldr_lux`, `echo_us → dist_mm` via the Day-1 curves.
4. **Derived features** — `pitch`, `roll` from the accelerometer; `|a|` magnitude as a
   movement indicator (a stand being carried reads very differently from one standing still).
5. **Normalisation** — z-score using **training-set statistics only**. Scaler parameters are
   frozen and hard-coded into the firmware. *Fitting the scaler on the full dataset is the
   classic leakage bug; avoid it and then say that you avoided it.*
6. **Windowing** — 10 samples (1 s) with 50% overlap for M3. Overlap is applied **within a
   run only**, never across the train/test boundary.

## 2. Features

| Model | Feature vector |
|---|---|
| M0, M1, M2 | Per-window summary statistics: mean, std, min, max of `dist_mm`, `ldr_lux`, `pitch`, `roll`, plus mean `|a|` → **17 features** |
| M3 | Raw windowed sequence: 10 timesteps × 4 channels → 40 values, plus the DSP block's spectral features in Edge Impulse |

Summary statistics over a window are the standard tabular-sensor approach and they are cheap
on an MCU — the whole feature extraction is a handful of running accumulators, no buffers of
floats sitting in RAM. That cost argument is worth making explicitly: on a 256 KB part,
feature choice *is* an architecture decision.

---

## 3. The four models

### M0 — Classical baselines (scikit-learn)
Decision Tree (depth-limited), k-NN (k=5), Logistic Regression.

**Why:** they set the floor. If a depth-4 decision tree hits 94% then a neural network needs
to justify its existence, and a decision tree is *also* a perfectly deployable edge model —
it compiles to a few nested `if` statements and runs in microseconds. Presenting this
honestly is a strength, not a concession: it shows the model choice was driven by evidence
rather than by wanting to use a neural network.

The decision tree also gives free **interpretability** — plot it, and the split thresholds
should align with the physical tolerances from `03-DATA-PROTOCOL.md` §3. When they do, that
is independent confirmation the dataset is sane. Put the tree on a slide.

### M1 — MLP classifier (shipping model)
Dense 17 → 32 → 16 → 6, ReLU, softmax. Dropout 0.2, early stopping on validation loss.

**Why:** captures the non-linear coupling between distance and brightness (inverse-square)
that a linear model cannot, while staying small enough to quantise into a few KB. This is the
Edge Impulse NN block and the model that ships.

### M2 — Autoencoder (unsupervised anomaly detection)
17 → 8 → 3 → 8 → 17, trained **only on `optimal`** samples. Reconstruction error above a
threshold (95th percentile of the training reconstruction error) flags "not a known setup".

**Why:** a five-class softmax is forced to answer even when the truth is none of the five —
someone walks in front of the sensor, the light is off entirely, the device is knocked. A
classifier will confidently return a wrong class. The autoencoder gates that: when
reconstruction error is high the device shows `UNKNOWN SETUP` instead of a confident lie.

**This is the innovation claim.** Combining a supervised classifier with an unsupervised
novelty gate on a 256 KB microcontroller is not standard practice in TinyML coursework, it
addresses a real failure mode, and it maps directly onto the "highlighting the innovative
nature of the approach" language in the 80%+ band. Lead with it.

### M3 — Windowed sequence model
1-D CNN over 10×4 windows (or GRU if it fits). Conv1D(16, k=3) → pool → Conv1D(16, k=3) →
GlobalAvgPool → Dense(6).

**Why:** distinguishes *settling* from *settled*. A stand being moved produces a transient
that a per-sample classifier reads as a sequence of wrong answers; a sequence model can
recognise "in motion, wait". Also tests whether temporal context is worth the extra RAM —
and if the answer is "no", that is a legitimate, reportable finding.

M3 is the first item to cut if Day 3 overruns (`01-ROADMAP.md`).

---

## 4. Validation methodology

State all of this explicitly on the ML slide — the rubric names "validation methodology" as a
required element.

- **5-fold stratified cross-validation** on the training sessions (S1+S2) → mean ± std of macro-F1.
- **Session-held-out test** on S3, touched exactly once at the end. No hyperparameter
  tuning against it.
- **Grouped splitting** — folds are grouped by run, never by sample, so overlapping windows
  from one run cannot straddle a fold boundary.
- **Macro-F1 as the headline metric**, not accuracy. Accuracy flatters imbalanced classes;
  macro-F1 weights all five equally, which matches the application — missing `underlit` matters
  as much as missing `too_far`.
- **Baseline comparison:** majority-class and a hand-tuned threshold rule. If the ML model
  does not beat the threshold rule, that is the finding and it must be reported.
- **Seeded runs (n=5)** so reported differences are not noise. Report mean ± std.

### Ablation (drives the evaluation slide)

| Sensors | Features | Purpose |
|---|---|---|
| Ultrasonic only | 4 | Can distance alone do it? |
| + LDR | 8 | Does light add information? |
| ~~+ IMU~~ | ~~17~~ | **Cut with the tilt channel — see `docs/12-SCOPE-CHANGE-TILT.md`. Do not present this row.** |

Each sensor must earn its place. If a sensor adds under a point of macro-F1, say so — an
honest negative result reads as rigour, and it feeds directly into a future-work
recommendation about sensor selection for a cost-reduced version.

---

## 5. Deployment path

1. Train in Edge Impulse (mirroring the offline architecture so the comparison holds).
2. **INT8 post-training quantisation.** Record float32 → INT8 accuracy delta and size delta.
   Expect roughly 4× smaller with a sub-1% accuracy cost; report whatever actually happens.
3. Export the Arduino library, drop into `firmware/03_inference/`.
4. Read the real footprint from the build output, not the estimate:
   `arduino-cli compile` reports flash and RAM directly.
5. Instrument `micros()` around `run_classifier()` for true on-device latency.

### Deployment facts to be able to explain in the Q&A

- **TFLite Micro has no dynamic allocation.** All working memory comes from a static tensor
  arena you size yourself; too small and it fails at init, too large and it starves the stack.
- **INT8 quantisation** maps float ranges to 8-bit integers with a per-tensor scale and zero
  point. It shrinks the model ~4× and lets the Cortex-M4F use integer/DSP paths, which is why
  it is usually *faster*, not just smaller.
- **The M4F has a single-precision FPU** but no NEON and no INT8 SIMD — CMSIS-NN kernels are
  what make quantised inference fast on this part.
- **Flash vs RAM:** model weights live in flash, activations in RAM. RAM is the tighter
  constraint at 256 KB, and it is what caps the sequence model's window length.

---

## 6. Deliverables from this phase

```
tools/train_offline.py            all four models, one command
reports/offline_results.md        comparison table, CV, held-out test
reports/ablation.md               sensor-subset study
reports/figures/confusion_*.png   one per model
reports/figures/decision_tree.png interpretability figure
models/*.pkl, models/*.tflite     trained artefacts
```
