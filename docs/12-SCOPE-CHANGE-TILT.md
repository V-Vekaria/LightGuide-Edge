# 12 — Scope change: the tilt channel

**Status: ✅ DECIDED 7 August 2026 — Option B, the channel is cut.**

The decision is applied throughout the repo. Every item on the checklist at the bottom of
this file is done: the contract, the ML plan, the evaluation plan, the data protocol, the
presentation plan, the capture tools and the Q&A pack all now say five classes and two
sensors. Gate G2 passes. Nothing left in the repo claims six.

**What you need from this file now** is the wording in *Option B* below — that is what goes
on the data-collection, ML and future-work slides, and the prepared answer is already in
`docs/10-QA-DEFENCE.md` under *"Why five classes? Your proposal said six."*

The rest of the file is kept as the record of why the decision was made and what it cost.

---

`AGENTS.md` §3 and §4 lock **six** classes and **three** sensors. What exists is **five**
classes and **two** sensors. `class 5 tilt_off` has zero samples, the IMU is not in the
capture format (`pitch` and `roll` are absent from every file in `data/raw/`), and the
ablation in `reports/offline_results.md` runs distance / light / both — there is no `+IMU`
row.

This is the single largest gap between the locked contract and the build. It is also the
gap most likely to be found, because:

- `tools/capture.py` still advertises `5 tilt_off` in its class list;
- `docs/06-PRESENTATION-PLAN.md` §3 scripts the demo video to show `TILT OFF — ADJUST ANGLE`
  at t=0:30;
- `docs/04-ML-PLAN.md` §4 justifies macro-F1 with "missing `tilt_off` matters as much as
  missing `too_far`";
- CW1 promised it, and `00-REQUIREMENTS-LOCKED.md` §6 lists it as carried forward.

An undeclared scope reduction against your own written contract reads far worse than a
declared one. Pick one of the two options below and commit to it.

---

## Option A — collect it (about 45 minutes)

Worth it if the evening is free. It restores the locked scope, gives the ablation its third
row, and makes the demo script runnable as written.

What it needs:

1. **Firmware.** `03_inference` does not log IMU. Add `pitch` / `roll` to `Sample`, to the
   run CSV header and to the row print — the LSM9DS1 is already initialised and
   `01_sensor_check` reads it, so this is plumbing, not new work. Budget ~1.6 ms per read
   (`AGENTS.md` §11), which fits inside the 125 ms loop.
2. **Retrain.** `CHANNELS` in `tools/train_offline.py` gains `pitch` and `roll`;
   `SENSOR_GROUPS` gains the `+IMU` row. `CLASS_NAMES` grows to six in three files
   (`train_offline.py`, `dataset_report.py`, `make_figures.py`) and in `model.h`.
3. **Recollect.** Six classes × 3 runs × 3 sessions. The existing five-class data stays
   valid *only if* the IMU columns can be back-filled — they cannot, so **all three
   sessions must be recaptured**. That is the real cost: roughly 45–60 minutes of
   recapture, not 15.
4. Re-export to Edge Impulse, re-run the online trials.

> **This is the honest cost.** Adding one class means recollecting the whole dataset,
> because the feature vector changes for every sample. If that does not fit before the
> deadline, take Option B — a rushed six-class dataset collected at 11 pm is worse than a
> clean five-class one, and Gate G2 exists to stop exactly that trade.

---

## Option B — declare the cut (about 10 minutes)

The defensible choice if time is short, and it costs less than it looks. Use this wording.

### On the data-collection slide

> "The locked design specified six classes across three sensors. Five were collected. The
> tilt channel was cut when the ultrasonic ECHO fault and the LDR pulldown fault consumed
> the two days budgeted for it — both are documented in the risk register with the
> measurements that found them. Rather than collect a thin sixth class in the time left, I
> held the five to the full quality gate: 332–393 samples each, three sessions, zero
> dropouts, zero NaNs, a genuinely held-out test session."

### On the ML slide

> "The ablation therefore compares distance-only, light-only and both. Neither sensor is
> sufficient alone — 0.410 and 0.461 macro-F1 — and together they reach 0.868. The IMU row
> is missing from that table and I am not going to claim a result I did not measure."

### On the future-work slide

> "Tilt is the first thing I would add, and the ablation is the reason it is worth adding
> rather than an assumption: both existing channels earn their place, so a third
> independent axis is the natural next gain. The IMU is already on the board and already
> initialised — the cost is recollection, not hardware."

### Why this scores better than hiding it

The rubric's Demonstration row rewards **"extensive understanding of the limitations and how
it may be improved"**, and the Evaluation row rewards **"extensive, specific considerations
of further work"**. A named, costed, measured limitation is evidence for both. A quietly
dropped requirement is evidence for neither, and invites the one question you cannot answer.

---

## Applied — every one of these is done (7 Aug)

- [x] `AGENTS.md` §2, §3, §4 — aims, scope and the class table marked **amended**, original
      text struck through rather than deleted so the change is visible
- [x] `AGENTS.md` §5, §6 — M2 description and the ablation arms
- [x] `docs/00-REQUIREMENTS-LOCKED.md` §6 — the CW1 reconciliation row
- [x] `docs/03-DATA-PROTOCOL.md` — class count and the `optimal` band definition
- [x] `docs/04-ML-PLAN.md` — macro-F1 justification, the softmax argument, the `+IMU`
      ablation row struck out
- [x] `docs/05-EVALUATION-PLAN.md` — metric definition, related-work comparison row, the
      confusion pair that referenced tilt
- [x] `docs/06-PRESENTATION-PLAN.md` — video script rewritten for five classes, slide 5
      class count, slide 7 feature count corrected to 8, slide 13 no longer mandates the
      `UNKNOWN SETUP` case
- [x] `docs/10-QA-DEFENCE.md` — prepared answer added, plus one for the threshold-baseline
      result
- [x] `tools/capture.py`, `tools/button_capture.py` — `CLASS_NAMES` and the 0-5 label
      range checks, which would otherwise offer a label the firmware rejects
- [x] `tools/dataset_report.py` — class 5 removed, so **Gate G2 now reports PASS**
- [x] `README.md`, `tools/train_offline.py` — remaining prose references

Nothing in the repo still claims six classes or three sensors.
