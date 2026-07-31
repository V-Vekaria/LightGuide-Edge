# 01 — Roadmap (LOCKED)

**Locked 31 July 2026. Deadline 12:00 noon 9 August 2026. Nine days.**
Target submission: **8 August**, one day early, so a failed upload is recoverable.

Each day has a **gate**. A gate is a binary fact with evidence attached. If a gate fails,
the *contingency* fires the same day — do not roll a slipped gate forward silently.

---

## Day 0 — Thu 31 Jul · Setup & hardware truth

**Goal:** the board tells us what it actually is, and nothing is at risk of frying.

- [x] Extract CW2 spec + rubric, reconcile against CW1, scaffold repo, lock docs
- [ ] Flash `firmware/01_sensor_check` → I²C scan, IMU revision, all 3 sensors printing
- [ ] Confirm the pin map in `02-HARDWARE.md` matches the physical build (rewire to match)
- [ ] **Resolve the HC-SR04 ECHO level question** (`AGENTS.md` §11.2) — blocking
- [ ] Fill in B-number in `AGENTS.md`
- [ ] `git init`, first commit

**Gate G0:** serial monitor shows plausible, stable `distance_mm`, `ldr_raw`, `ax/ay/az`
at 10 Hz for 60 s with no resets, and the ECHO line is confirmed ≤3.3 V.

**Contingency:** if HC-SR04 cannot be made safe today, fall back to the **on-board APDS9960
proximity channel** for distance for a day so data collection is not blocked, and note the
substitution in the deck.

---

## Day 1 — Fri 1 Aug · Calibration & logger

**Goal:** raw counts become physical units, and labelled capture works end to end.

- [ ] **Ultrasonic calibration:** measure 10 known distances (tape measure, 20–200 cm), 30 readings each → fit + residual plot → `data/calibration/ultrasonic.csv`
- [ ] **LDR calibration:** measure ≥8 light levels against a reference lux source (phone lux app is acceptable — *name the app and phone model in the deck*) → fit log-linear curve → `data/calibration/ldr.csv`. **This closes the open issue CW1 flagged.**
- [ ] Flash `firmware/02_data_logger` — CSV over serial at a fixed rate, with a label channel
- [ ] `tools/capture.py` records a labelled session to `data/raw/`
- [ ] Sensor noise characterisation: 5 min static capture → per-channel σ, drift

**Gate G1:** a 60-second labelled session lands in `data/raw/` as valid CSV with no dropped
rows, and both calibration curves are plotted in `reports/figures/`.

**Contingency:** if the lux reference is unavailable, calibrate *relatively* (known dimmer
steps at fixed distance) and state the limitation explicitly rather than skipping it.

---

## Day 2 — Sat 2 Aug · Dataset collection ⚠️ highest-risk day

**Goal:** ample, high-quality, honestly-split data. This is 15% of the mark on its own and
it gates everything downstream.

- [ ] Collect **≥250 samples per class × 6 classes** (≥1500 total), per `03-DATA-PROTOCOL.md`
- [ ] Across **≥3 physically distinct sessions** — vary ambient light, surface, stand position, time of day
- [ ] Reserve **session 3 entirely** as the held-out test set. Never mix it into training.
- [ ] Include the confounded cases (§4 of `AGENTS.md`) — e.g. `too_far` + brighter
- [ ] Log a session sheet per session: ambient conditions, reference setup, anything unusual
- [ ] Build `data/processed/` + data dictionary; plot class balance and per-class traces

**Gate G2:** `tools/dataset_report.py` prints ≥250/class, ≥3 sessions, zero NaNs, and the
held-out session is disjoint.

**Contingency:** if time runs short, cut to 200/class **evenly across all six** rather than
250 on four classes and 80 on two. Balance matters more than raw volume. Never fabricate
or duplicate samples to hit the count — that is academic misconduct, and augmentation must
be declared as augmentation.

---

## Day 3 — Sun 3 Aug · Offline ML

**Goal:** four approaches, one honest comparison table.

- [ ] Preprocessing + feature extraction per `04-ML-PLAN.md` (windowing, scaling, stats)
- [ ] **M0** classical baselines: Decision Tree, k-NN, Logistic Regression
- [ ] **M1** MLP classifier
- [ ] **M2** autoencoder anomaly detector (train on `optimal` only; threshold on reconstruction error)
- [ ] **M3** windowed sequence model
- [ ] 5-fold stratified CV **and** session-held-out test for every model
- [ ] **Ablation:** distance-only → +LDR → +IMU
- [ ] Confusion matrices, per-class P/R/F1, learning curves → `reports/figures/`

**Gate G3:** `reports/offline_results.md` contains one comparison table covering all four
models on identical splits, plus the ablation table.

**Contingency:** M3 is the droppable one. If the sequence model eats the day, ship M0/M1/M2
and present M3 as *attempted, with the reason it was cut* — an honest negative result still
scores under "compare different methodological approaches".

---

## Day 4 — Mon 4 Aug · Edge Impulse & quantisation

- [ ] Upload dataset to Edge Impulse, **preserving the same train/test split** used offline
- [ ] Build impulse: DSP block + NN classifier; mirror the offline architecture so the comparison stays fair
- [ ] Train, then **INT8 quantise**; record the float32 → INT8 accuracy delta
- [ ] Read the EI profiler: latency estimate, peak RAM, flash
- [ ] Export the Arduino library → `firmware/03_inference/`
- [ ] **Make the EI project public and record the URL** — it is a required submission element

**Gate G4:** quantised model exported, EI project public, float-vs-INT8 delta recorded.

**Contingency:** if EI fights back, deploy the TFLite model directly with the already-installed
`Arduino_TensorFlowLite` library. Keep the EI link requirement in mind — a public EI project
must still exist even if the shipped model came from elsewhere.

---

## Day 5 — Tue 5 Aug · On-device integration

- [ ] `firmware/03_inference`: sense → preprocess → infer → decide → display
- [ ] OLED shows class + the specific corrective instruction ("MOVE BACK 15 cm")
- [ ] LED colour = state; buzzer if fitted
- [ ] Autoencoder gate: if reconstruction error > threshold → show `UNKNOWN SETUP` instead of a wrong confident answer
- [ ] Reference-capture mode: long-press / serial command stores the reference setup
- [ ] Instrument `micros()` around inference; print latency every N cycles

**Gate G5:** device runs **standalone on USB power with no serial monitor attached** and
responds correctly to all six conditions by hand.

**Contingency:** if RAM is tight, drop M3's window length or reduce hidden units — do not
drop the OLED, it is the demo.

---

## Day 6 — Wed 6 Aug · Online evaluation

**Goal:** the evidence for the 20% Critical Evaluation row that most students never collect.

- [ ] Scripted online trial: **20 trials × 6 classes = 120 live runs**, ground truth logged by hand
- [ ] Build the **online confusion matrix**; compare it against the offline one and *explain the gap*
- [ ] Measure: mean/p95 inference latency, end-to-end sense→display latency, throughput
- [ ] Record real flash + RAM from the build output, not the estimate
- [ ] Time-to-correct-adjustment: how long a user takes to reach `optimal` with vs without the device (even n=5 is a result)
- [ ] Failure analysis: characterise every misclassification

**Gate G6:** `reports/online_results.md` holds the live confusion matrix, latency
distribution and footprint table.

**Contingency:** if 120 trials is too many, do 10×6 = 60 and report the confidence interval
honestly. A small n with stated uncertainty beats a large n that was rushed.

---

## Day 7 — Thu 7 Aug · Deck & demo video

- [ ] Build the 12-slide deck to `06-PRESENTATION-PLAN.md` — every `[ADD %]` placeholder from CW1 replaced with a real number
- [ ] Shoot the **~1 min split-screen** video: hardware on one side, OLED/serial output on the other, showing response to stimulus
- [ ] Render figures at presentation resolution
- [ ] Write speaker notes with per-slide timings

**Gate G7:** deck exports cleanly to PDF; video is ≤1 min and plays in Panopto.

---

## Day 8 — Fri 8 Aug · Rehearse & SUBMIT

- [ ] Two timed run-throughs — must land inside 12 minutes
- [ ] Drill `10-QA-DEFENCE.md` (the 10% theory row is won or lost here)
- [ ] Package D1 Code zip (with EI link), D2 Dataset zip, D3 Slides, D4 Video
- [ ] **Upload all four. Screenshot every confirmation.**
- [ ] Verify each uploaded file re-downloads and opens

**Gate G8:** four Blackboard receipts saved in `deliverables/receipts/`.

---

## Sat 9 Aug — 12:00 noon · Hard deadline

Buffer day only. Nothing new gets built. If everything went to plan this day is unused —
which is the point.

---

## Week of 10 Aug — Oral defence

12-minute presentation + 5–10 min Q&A. Bring the hardware. Have the live demo ready **and**
the video as a fallback in case the setup misbehaves in the room.

---

## Critical path

```
G0 hardware ──▶ G1 calibration ──▶ G2 dataset ──▶ G3 offline ML ──▶ G4 EI ──▶ G5 device ──▶ G6 online ──▶ G7 deck ──▶ G8 submit
   Thu            Fri                Sat  ⚠️        Sun              Mon        Tue           Wed          Thu        Fri
```

**Everything downstream of G2 is blocked by G2.** The dataset is the bottleneck. If Saturday
slips, cut model M3 and online trial count — never cut the dataset, the on-device demo, or
the rehearsal.

## Protected items (cut last, in this order)

1. Dataset quality and balance (G2)
2. On-device working demo (G5)
3. The deck and the 12-minute timing (G7/G8)
4. Online evaluation (G6)
5. Model M3 (first to go)
