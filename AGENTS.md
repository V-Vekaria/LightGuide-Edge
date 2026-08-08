# AGENTS.md — LightGuide Edge (COM683 CW2)

> **This file is the contract.** Aims, scope and deliverables below are **LOCKED**.
> Anything not listed under "In scope" is out of scope until the CW2 deadline passes.
> Read this file first, every session, before touching anything else.

---

## 1. Identity

| Field | Value |
|---|---|
| Project | **LightGuide Edge** — embedded AI lighting-setup assistant for studio photography |
| Module | COM683 Edge & Embedded Intelligence, Ulster University |
| Student | Vishnu Vekariya · **B00969091** |
| Assessment | CW2 — Project Presentation & Demo, **60%** of module |
| **Submission deadline** | **12:00 noon, Sunday 9 August 2026** (Blackboard) |
| Oral defence | Week commencing 10 August 2026 — 12 min presentation + 5–10 min Q&A |
| Working days remaining at lock | **9** (locked 31 July 2026) |
| Repository | **https://github.com/V-Vekaria/LightGuide-Edge** — ⚠️ **PRIVATE**. Keep it private until after grading (31 Jul 2026): a public repo of in-progress coursework invites copying, and Ulster's misconduct process is uncomfortable for the original author too. |
| Target band | **1st (70–79%)** minimum on every rubric row; **High 1st (80–100%)** on ML, Evaluation and Demo |

---

## 2. Locked aims

> **AMENDED 7 August 2026 — the tilt channel is cut.** The aims below were locked on
> 31 July against six classes and three sensors. The delivered system covers **five
> classes across two sensors** (distance and brightness); `tilt_off` and the IMU channel
> were dropped. Rationale, cost and the wording to use in the deck are in
> `docs/12-SCOPE-CHANGE-TILT.md`. The amendment is recorded here rather than applied
> silently — the original text is struck through, not deleted.

**Primary aim (from CW1, amended):**
Build an embedded device that classifies studio-lighting **setup quality** in real time, entirely on-device, from distance / brightness / ~~tilt~~, and tells the photographer what to adjust.

**Secondary aims (from CW1, amended):**
1. Compare **multiple ML paradigms** — supervised, unsupervised (anomaly), and sequence-based — head to head.
2. Achieve **low-latency on-device inference with zero cloud dependency** at run time.
3. Deliver a **working prototype** covering ~~six~~ **five setup conditions** with fast, actionable feedback.

**SDG alignment (NEW — required by the handbook, absent from CW1; see `docs/08-ETHICS-SDG.md`):**
- **Primary: SDG 12 — Responsible Consumption and Production.** Repeated test-shot cycles keep 300–1000 W continuous / high-draw strobe lighting running longer than needed and shorten modifier and lamp service life. Getting the setup right first time cuts studio energy per shoot.
- **Secondary: SDG 8 — Decent Work and Economic Growth (target 8.3).** Freelancers and micro-studios absorb the cost of re-shoots directly; consistent setups protect their productivity.

---

## 3. Locked scope

### In scope
- Arduino Nano 33 BLE Sense (nRF52840) as the only compute target.
- Sensors: **HC-SR04 ultrasonic** (distance), **LDR** (brightness). ~~on-board IMU (tilt)~~ — cut 7 Aug, §12.
- Feedback: **SSD1306 I²C OLED**, on-board LED, buzzer *(buzzer optional — not present in the current physical build; see Risk R-04)*.
- **Five** classification classes (§4). ~~Six~~ — amended 7 Aug.
- **Four** ML approaches trained and compared (§5).
- Offline **and** online evaluation (§6).
- Four Blackboard artefacts + oral defence (§7).

### Explicitly out of scope (do not build)
- BLE companion mobile app (CW1 listed it as *future scope* only).
- Camera / image capture of any kind.
- Cloud inference, OTA updates, web dashboard.
- PCB design or enclosure fabrication beyond a clamp mock-up for the demo.
- Multi-light or multi-device setups.

---

## 4. Class definitions (5 classes — amended 7 Aug)

Reference setup is captured once by the user; every class is a deviation from it.

| # | Label | Physical condition | Primary driving sensor |
|---|---|---|---|
| 0 | `optimal` | Distance and brightness both inside the reference band | both |
| 1 | `too_close` | Light stand nearer than the reference band | ultrasonic |
| 2 | `too_far` | Light stand further than the reference band | ultrasonic |
| 3 | `underlit` | Output below reference (dimmed, off, blocked, modifier added) | LDR |
| 4 | `overlit` | Output above reference (dialled up, modifier removed) | LDR |
| ~~5~~ | ~~`tilt_off`~~ | ~~Head/stand angle deviates from reference aim~~ | **CUT 7 Aug — §12** |

**Rule:** classes are *not* pure single-sensor cases. Deliberately collect confounded examples
(e.g. `too_far` *and* dimmer, which also reads darker) so the model must learn the joint
distribution rather than a single threshold. This is what separates an ML solution from
three `if` statements — and the marker **will** ask.

---

## 5. Locked ML approaches (all four are trained and compared)

| ID | Paradigm | Model | Role |
|---|---|---|---|
| **M0** | Supervised, classical baseline | Decision Tree + k-NN + Logistic Regression (scikit-learn) | Floor. Proves the NN earns its cost. |
| **M1** | Supervised, deep | MLP classifier (Edge Impulse NN) | **Shipping model.** |
| **M2** | Unsupervised | Autoencoder, reconstruction-error anomaly detection | Rejects setups outside the five classes ("unknown"). |
| **M3** | Sequence | Windowed 1-D CNN / temporal model over an N-sample window | Captures settling & motion; tests whether time context helps. |

Every model reports: accuracy, macro-F1, per-class precision/recall, confusion matrix,
float32 → INT8 delta, flash + RAM footprint, on-device latency.

**Selection must be justified from literature *and* from the measured experiments** — the
80%+ band requires both. See `docs/09-REFERENCES.md`.

---

## 6. Locked evaluation protocol

Full detail in `docs/05-EVALUATION-PLAN.md`. Non-negotiable elements:

- **Offline:** 5-fold stratified CV + a **session-held-out** test set (never a random split — a random split leaks near-duplicate consecutive samples and inflates accuracy; say this out loud in the presentation).
- **Online:** on-device live confusion matrix from a scripted trial protocol, measured `micros()` latency, real flash/RAM from the build, throughput.
- **Ablation:** distance-only → light-only → both, to prove every sensor earns its place. (The `+IMU` arm is cut with the tilt channel, §12.)
- **Quantisation study:** float32 vs INT8 accuracy and size.

---

## 7. Locked deliverables

Blackboard file naming — **exact filenames, locked**:

| # | Component | Filename | Artefact | Status |
|---|---|---|---|---|
| D1 | Code | `VekariyaVishnuB00969091_Code.zip` | commented Arduino sketches + Edge Impulse–exported library + Python pipeline + **public Edge Impulse project link** | ☐ |
| D2 | Dataset | `VekariyaVishnuB00969091_Dataset.zip` | raw + processed train/test samples, with data dictionary | ☐ |
| D3 | Slides | `VekariyaVishnuB00969091_Slides.pptx` / `.pdf` | ~12 slides to the rubric map in `docs/06-PRESENTATION-PLAN.md` | ☐ |
| D4 | Video | `VekariyaVishnuB00969091_Video.mp4` | ~1 min **split-screen** demo, via the **Panopto** dropbox (not the file dropbox) | ☐ |
| D5 | — | — | 12-min oral defence + Q&A prep pack | ☐ |

Every one is compulsory; a missing component is a non-submission for that area.
Keep the Blackboard confirmation screenshot for each.

---

## 8. Rubric map — where each mark is won

| Rubric criterion | % | Where it is earned | Evidence file |
|---|---|---|---|
| Problem Overview | 5 | Slide 2, with scale + SDG framing and stated innovation opportunity | `docs/08-ETHICS-SDG.md` |
| Description of Solution | 10 | Slides 3–4, literature-justified, ethical/technical/social dimensions all covered | `docs/09-REFERENCES.md` |
| Methodology for Data Collection | 15 | Slides 5–6: protocol, calibration, labelling, quality controls, session design | `docs/03-DATA-PROTOCOL.md` |
| Development/Implementation of ML | 20 | Slides 7–9: 4 approaches, preprocessing, features, validation | `docs/04-ML-PLAN.md` |
| Critical Evaluation of Performance | 20 | Slides 10–12: offline **and** online, ablation, comparison vs related work, future work | `docs/05-EVALUATION-PLAN.md` |
| Understanding of Theoretical Knowledge | 10 | Q&A defence pack | `docs/10-QA-DEFENCE.md` |
| Demonstration of Solution | 10 | Slide 13 + video D4 + live demo, incl. stated limitations | `docs/06-PRESENTATION-PLAN.md` |
| Organisation and Coherence | 10 | Deck design, 12-min timing, dynamic content | `docs/06-PRESENTATION-PLAN.md` |

**The 80%+ band is unlocked by three words that appear across the rubric: *innovation*,
*literature-underpinned*, *advanced compared to related works*.** Every major claim in the
deck needs a citation or a measurement behind it.

---

## 9. Progress tracker

Update the status column at the end of every working session. Do not delete rows.

| Phase | Gate (must be true to pass) | Target date | Status |
|---|---|---|---|
| P0 Setup | Repo scaffolded, docs locked, board flashes and prints all 3 sensors | 31 Jul | ✅ **all three sensor channels working** — evidence: `reports/gate_G0_acceptance.md` |
| P0.1 Hardware | HC-SR04, LDR, OLED, IMU verified; pin map confirmed | 31 Jul | ✅ **G0 PASSED 5 Aug** — root cause was the 1 kΩ pulldown never swapped, not a dead cell (§11.6). After fitting 10 kΩ: swing 2,806 counts = 68.5% of scale. Evidence: `reports/gate_G0_rerun_2026-08-05.txt` |
| P0.2 Feedback devices | Buzzer, external LED, switch verified | 31 Jul | ✅ **done** — buzzer D9, LED D8, switch D7 (16 debounced presses). Buzzer **is** in the build; the R-04 "not fitted" note is obsolete |
| P1 Calibration | LDR→lux curve + ultrasonic→cm curve recorded against references | 1 Aug | ✅ **done 8 Aug** — closes the CW1 open issue. Six-point tape sweep, 40–200 cm, lamp on a second stand at LDR height. **γ = 0.623** (CdS band 0.5–0.9), **R² = 0.9975**; APDS9960 cross-check r = 0.9913. Ultrasonic **slope 1.0041, RMSE 7.2 mm out to 160 cm** — the 200 cm point was auto-excluded (reads +134 mm long; beam cone 527 mm vs a ~200 mm target, so the echo came off the background). See `reports/calibration.md` |
| P2 Logger | Labelled CSV capture over serial working end-to-end | 1 Aug | ✅ **done** — `02_data_logger` + `tools/capture.py`; capture is also built into `03_inference` |
| P3 Dataset | ≥250 samples/class × 6 classes, ≥3 sessions, held-out session reserved | 2 Aug | 🟡 **5 of 6 classes.** 1,832 samples, 45 runs, 332–393/class, 3 sessions, **0 dropouts, 0 NaNs**, session 3 held out. `tilt_off` never collected — see `docs/12-SCOPE-CHANGE-TILT.md`. Evidence: `reports/dataset_report.md` |
| P4 Offline ML | M0–M3 trained, CV + test metrics, confusion matrices, ablation | 3 Aug | ✅ **done** — M0a–d, M1, M2 novelty gate, M3 sequence, majority baseline, ablation. Best: decision tree, held-out macro-F1 **0.868**. `reports/offline_results.md` |
| P5 Edge Impulse | EI project public, model trained, INT8 quantised, library exported | 4 Aug | 🔴 **NOT DONE.** Required element of the Code component. Dataset is exported and ready at `deliverables/edge_impulse/`; follow `docs/11-EDGE-IMPULSE-STEPS.md` (~45 min) |
| P6 On-device | Inference + OLED/LED feedback loop running standalone | 5 Aug | ✅ **done** — `firmware/03_inference` classifies on-device from `model.h`. Measured footprint **120,896 B flash (12.3%)**, **46,696 B RAM (17.8%)** — regenerate with `py -3 tools/record_footprint.py`, never by hand |
| P7 Online eval | Live confusion matrix, latency, RAM/flash measured and tabulated | 6 Aug | ✅ **done 8 Aug** — 40 runs, 1,648 samples, 0 dropouts. Per-run and per-sample macro-F1 **1.000**; latency mean 20.8 µs / p95 21.0 µs. Collected in three sittings after `check_staging` caught 14 mis-staged lamp runs and 3 under-brightened `overlit` runs; merged by `tools/merge_online.py`. **The 1.000 is not comparable to the offline 0.868** — online staged prototypes ~300 mm out, the held-out session contains windows 33–38 mm out. See `reports/online_results.md` §"Are these two numbers comparable?" |
| P8 Deck + video | 12-min deck built, split-screen demo video cut | 7 Aug | ⬜ blocked on P1, P5, P7 producing their numbers |
| P9 Rehearse | 2 timed run-throughs, Q&A pack drilled | 8 Aug | ⬜ |
| P10 Submit | All 4 components uploaded, receipts screenshotted | **8 Aug (1 day early)** | ⬜ packaging script ready: `tools/package_submission.py --ei-url ...` |

Legend: ⬜ not started · 🟡 in progress · ✅ done · 🔴 blocked

---

## 10. Session log

| Date | What changed | Next action |
|---|---|---|
| 2026-08-07 | **Pre-deck audit.** Fixed two silent bugs in `dataset_report.py` (the `#` header line was parsed as column names, so Gate G2 had never run; the LDR saturation check looked for a column name the logger does not write and was skipping in silence). Added M3 sequence model and the majority-class baseline the ML plan asked for. Added five presentation figures. Instrumented `03_inference` with `micros()` inference timing and a serial run trigger. Wrote `tools/online_trial.py`, `tools/export_edge_impulse.py`, `tools/make_figures.py`, `tools/package_submission.py`. Documented the tilt scope gap | **P1 calibration sweep, P5 Edge Impulse, P7 online trials** — all three need hardware or an account and none can be done from the repo |
| 2026-07-31 | Requirements + rubric extracted from handbook; CW1 promises reconciled; repo scaffolded; docs locked; bring-up firmware + capture tool + offline pipeline written and smoke-tested end to end on synthetic data | — |
| 2026-07-31 | Flashed `01_sensor_check` to COM4. **Rev1 confirmed, IMU/OLED/LDR all verified working. Ultrasonic ECHO stuck HIGH — blocker, see §13.** Fixed two mbed-core firmware bugs (`pulseIn` timeout, I²C scanner) | — |
| 2026-07-31 | Built `01b_calibration` + `tools/calibrate.py`: meter-free LDR characterisation via the inverse-square law, ambient fitted as a free parameter, APDS9960 cross-check, ultrasonic calibrated from the same sweep. Fitter validated against a simulated cell (worst error 0.009 across a γ/ambient/noise grid) | — |
| 2026-07-31 | HC-SR04 rewired by user — ECHO now idles LOW correctly, loop recovered to 128 ms. Built `00_wiring_probe`. B-number confirmed, submission filenames locked | — |
| 2026-07-31 | **Acceptance test run** (`01e_acceptance`). 9/9 components electrically working; LED and buzzer confirmed by operator. **One open item: LDR pulldown is 1 kΩ and must be 10 kΩ** — the channel uses only 6% of ADC scale, which would weaken separation of `underlit`/`optimal`/`overlit`. Full report in `reports/gate_G0_acceptance.md` | **Swap the resistor, re-run `01e_acceptance`, then `tools/calibrate.py`** |
| 2026-07-31 | ✅ **Full hardware bring-up complete.** All eight components verified on hardware: HC-SR04 (D2/D3, 0% dropouts @10 Hz), LDR (A0), IMU, OLED (0x3C), buzzer (D9), external LED (D8), switch (D7, debounced), plus on-board APDS9960 and RGB LED. Every CW1-promised component is present and working | **Run `tools/calibrate.py`, then the data logger — Day 1 of the roadmap** |
| 2026-07-31 | Bring-up all but the switch. HC-SR04 (0% dropouts, 10 Hz), IMU, OLED (0x3C), LDR divider, buzzer (D9) and external LED (D8) all verified on hardware. Root cause of the long OLED/LDR hunt was the breadboard `−` rail sitting at 3.3 V (§13.4). Switch still not completing to ground — optional, does not gate anything | **Fix the ground rail, then run `tools/calibrate.py`** |
| 2026-08-05 | **Audit before starting P1.** Repo state checked against the tracker: P1–P10 all unstarted (`data/calibration/`, `data/raw/*.csv`, `models/`, `firmware/03_inference/` all empty). Corrected two tracker claims that the evidence does not support: **G0 was recorded as passed but its own raw output says `NOT PASSED` / `LDR response FAIL`** (§11.6), and the switch count was 28 vs 16 measured. Closed the buzzer open item (§11.4) — D9 confirmed audible at G0. Verified toolchain unchanged: `01b_calibration` compiles 9% flash / 17% RAM, all Python deps present. Rig light (dimmable LED panel, 2000–10000 K) ordered, arrives evening 5 Aug | **Swap the LDR pulldown to 10 kΩ, re-run `01e_acceptance` for `LDR response PASS`, then `tools/calibrate.py`** |
| 2026-07-31 | Built `00c_voltmeter` and `00d_sensor_inventory`. **Scanned `Wire1` and found the on-board sensors the earlier scan missed.** Board confirmed as Sense **Lite**: HTS221 absent, APDS9960 present and returning live data. **Measured that the LDR is not connected to A0, and retracted the false-positive OLED verification.** Pattern is unambiguous: 100% of on-board devices work, 0% of external ones do → single fault in the Nano-to-breadboard path, most likely unsoldered castellated headers (§13) | **Check the Nano's header pins are soldered and seated, then re-run `00d_sensor_inventory` and `00_wiring_probe`** |

---

## 11. Open decisions

1. ~~B-number~~ → ✅ **B00969091** (confirmed 31 Jul). Exact submission filenames locked in §7.
2. ~~HC-SR04 supply voltage~~ → superseded by the live fault in §13.
3. ~~Board revision~~ → ✅ **Rev1 confirmed.** `IMU.begin()` succeeded via
   `Arduino_LSM9DS1` at 119 Hz. Both sketches are correctly set to `BOARD_REV 1`.
4. ~~Buzzer pin~~ → ✅ **D9 confirmed** (31 Jul, `01e_acceptance`): driven and confirmed
   audible by the operator. The earlier "D9 floating" probe reading was taken while the
   breadboard `−` rail sat at 3.3 V (§13.4), which is why it looked unconnected. The
   §13.3 table is superseded on this point.
5. ~~Reference light meter~~ → ✅ **Not needed.** `tools/calibrate.py` recovers the LDR's
   response exponent using the inverse-square law as the reference, with a tape measure and
   a lamp-off ambient reading. Absolute lux is not the quantity this system needs. See
   `docs/02-HARDWARE.md` §4. If a meter turns up later, one reading rescales the existing
   curve — no recollection.

6. **LDR pulldown value — 🔴 OPEN, blocks P1 calibration.** The only recorded acceptance
   run (`reports/gate_G0_acceptance.txt`, 31 Jul) ends `*** GATE G0 NOT PASSED ***` with
   `LDR response FAIL`: the channel swings 255 counts (229–484), 6% of ADC scale. The
   report's required action — swap the 1 kΩ pulldown (brown·black·red) for 10 kΩ
   (brown·black·orange) and re-run `01e_acceptance` — has **no passing evidence on record**.

   This is not cosmetic. `tools/calibrate.py` hardcodes `R_FIXED = 10_000.0`, so calibrating
   against a physically-fitted 1 kΩ makes every `counts → R_ldr` conversion wrong by 10× and
   γ meaningless. It also compresses `underlit` / `optimal` / `overlit` into 6% of the range,
   which worsens after INT8 quantisation and would surface only as an unexplained accuracy
   ceiling — after the dataset was collected and too late to fix.

   **Gate: `01e_acceptance` must print `LDR response PASS` before `tools/calibrate.py` runs.**

---

## 13. 🔴 BLOCKER — no electrical path between the Nano and the breadboard

**Superseded diagnosis below.** The earlier "HC-SR04 unpowered" call was too narrow, and
the "OLED verified" claim was wrong — see §13.1 for the correction.

### The measured pattern (31 July)

| Device | Bus / pin | Status | Evidence |
|---|---|---|---|
| LSM9DS1 IMU | Wire1 0x6B/0x1E | ✅ works | live accel, 119 Hz |
| APDS9960 | Wire1 0x39 | ✅ works | live colour r=79 g=65 b=43 ambient=161 |
| LPS22HB pressure | Wire1 0x5C | ✅ present | ACKs on bus |
| HTS221 temp/humidity | Wire1 0x5F | ⛔ **not fitted** | absent — this is the "Lite" reduction |
| LDR | A0 | ❌ not connected | A0 statistically identical to floating A1/A2 |
| SSD1306 OLED | Wire (A4/A5) | ❌ not connected | nothing ACKs on the external bus |
| HC-SR04 | D2/D3 | ❌ no response | no echo on any trigger pin |
| LED / switch / buzzer | D2–D12 | ❌ not connected | all floating |

**Every on-board device works. Zero external devices work.** That is one fault, not six.

### Diagnosis

The Nano 33 BLE Sense Lite ships **castellated**. If male header pins are not soldered on,
the board rests on the breadboard and makes contact with **nothing** — jumpers sit in rows
adjacent to the board, but no row reaches the board's pads. This explains every symptom at
once, including why USB-powered on-board sensors are perfectly healthy.

**Check first: are header pins soldered to the Nano and seated in the breadboard?**

Secondary possibilities, if the headers *are* soldered:
1. The board straddles the centre channel incorrectly, so each side's pins share rows.
2. The breadboard's + / − rails are not jumpered to the Nano's `3V3` / `GND`.
3. Rails are split mid-board and the jumpers feed the wrong half.

### Verifying the fix

`firmware/00c_voltmeter` turns the Nano into a multimeter — jumper A1 to any point and read
the voltage. Probe in this order: the Nano's own `3V3` pin (proves the probe works), then
the breadboard + rail, then each module's VCC. The first point that reads ~0 V instead of
~3.3 V is where the path breaks.

---

## 13.4 The breadboard `−` rail is at 3.3 V, not ground

**Measured 31 July** with `00c_voltmeter` / the A0 probe: the rail being used as ground
reads **3.30 V**. It is tied to 3V3, not to GND.

This is a single fault that plausibly explains **both** remaining failures:

- **LDR divider dead.** The "pulldown" resistor runs from A0 to a 3.3 V rail, so A0 is held
  at supply from both sides. Nothing divides, and light level cannot affect the reading —
  observed as A0 pinned at 4080–4095 with a 15-count swing while covered.
- **OLED dead.** With `GND` on that rail, the module sees 3.3 V on both VCC and GND. No
  potential difference, no power, no ACK on the bus.

**Fix: wire external modules directly to the Nano's own `3V3` and `GND` pins and bypass the
rails entirely** until the rails are re-jumpered and re-verified. Fewer contacts, and it
removes the failing element from the circuit rather than debugging around it.

Secondary suspicion for the LDR, still unconfirmed: **its two legs may be in the same
breadboard row**, which shorts it out (holes a–e of one numbered row are common). A shorted
LDR ties A0 straight to the + rail and produces exactly the observed rock-steady 3.30 V,
independent of light. Every component must have its two legs in *different* numbered rows.

## 13.5 OLED module pin order — read off the hardware

This module's silkscreen: **`VCC · GND · SCL · SDA`**. **SCL is 3rd, SDA is 4th** — the
reverse of the common assumption. Wiring pin 3 → A4 and pin 4 → A5 swaps the I²C lines and
the display never responds, with no error to diagnose from. See `docs/02-HARDWARE.md` §2.0b
step 4 for the full mapping.

## 13.1 Correction — the OLED was never verified

`01_sensor_check` reported `[ OK ] OLED at 0x3C`. **That was a false positive.**
`Adafruit_SSD1306::begin()` returns success once it has allocated its frame buffer and sent
init commands; it does **not** check that any device acknowledged on the bus. A correct
scan of `Wire` finds nothing on A4/A5.

Two lessons worth carrying into the deck, because both are real engineering findings:

1. **A library's `begin()` returning true is not evidence a device exists.** Only a
   round-trip that reads data back proves presence. The APDS9960 check in
   `00d_sensor_inventory` does exactly that — it reports a live colour reading, not just
   an init result.
2. **The on-board sensors are on `Wire1`, not `Wire`.** The Nano 33 BLE family puts its
   internal sensors on a separate I²C bus. Scanning only `Wire` is why the first scan
   looked empty and sent the diagnosis down the wrong path for a round.

---

## 13.2 Impact of the "Lite" variant

Confirmed by the user and by the bus scan: this is a **Nano 33 BLE Sense Lite**.
The **HTS221 temperature/humidity sensor is not fitted**.

- `docs/02-HARDWARE.md` §6 and `docs/05-EVALUATION-PLAN.md` §5 both proposed using the
  HTS221 to temperature-compensate the ultrasonic speed of sound. **That chip does not
  exist on this board.** The idea survives, though: the **LPS22HB (0x5C) has an on-chip
  temperature output**, so retarget the compensation there and say so explicitly.
- **The APDS9960 IS present and working.** So the LDR cross-check in `tools/calibrate.py`
  and the proximity fallback for distance both remain available. Good news for two
  contingency plans that depended on it.

---

## 13.3 Original (superseded) HC-SR04 note

**Diagnosed on hardware, 31 July, via `firmware/00_wiring_probe`.**

Everything else on the board is verified working:

| Subsystem | Status | Evidence |
|---|---|---|
| IMU (LSM9DS1, Rev1) | ✅ | `accel sample rate 119.00 Hz`; az ≈ 0.96 g flat, pitch/roll ≈ 0 |
| OLED SSD1306 | ✅ | `[ OK ] OLED at 0x3C` |
| LDR on A0 | ✅ | ~290–375 of 4095 in current room light, responsive |
| **HC-SR04** | 🔴 | unpowered — see below |
| Buzzer | ❓ | **not on D9** (probe says floating). Where is it? |

### The evidence

After rewiring, ECHO on D3 correctly idles LOW (it was stuck HIGH before), and the loop
recovered to 128 ms from 3.7 s. But distance still returned −1 on every read, and a sweep
of D2–D12 as candidate TRIG pins produced no echo on any pin pair.

The wiring probe then gave the decisive result:

```
2,LOW,LOW,held LOW externally  <-- connected
3,LOW,LOW,held LOW externally  <-- connected
4..12  floating - nothing attached
```

**D2 is TRIG, which is an *input* on the HC-SR04.** A powered module presents a
high-impedance input, so D2 should read **floating**. Both D2 and D3 being clamped to
ground, with zero response to any trigger, is the signature of an **unpowered chip** — the
protection structures pull a dead chip's pins to ground.

So the wiring is right and the module is dead on the bench. Power, not pins.

### The fix

**Put the module's VCC on the `3V3` pin** and confirm its GND shares the Nano's ground rail.

The likely cause is board-specific and catches people out: **on the Nano 33 BLE, the `VUSB`
pin is NOT connected to USB 5 V by default.** A solder bridge on the underside of the board
has to be closed to enable it. VCC on VUSB therefore delivers nothing.

Running from 3V3 is also the *safe* choice — no 5 V ever reaches ECHO, so the level-shifting
hazard in `docs/02-HARDWARE.md` §3 disappears entirely. The cost is reduced maximum range,
which is well outside our 20–200 cm working band and is characterised during calibration
anyway. **Take Option B in `docs/02-HARDWARE.md` §3 and close this out.**

### Verifying the fix

```bash
arduino-cli upload -p COM4 --fqbn arduino:mbed_nano:nano33ble firmware/00_wiring_probe
```

D2 should flip to `floating` once the module is powered. Then re-flash `01_sensor_check`
and confirm `[ OK ] ECHO idles LOW` plus real distances. Send any character to re-run its
diagnostics without reflashing.

**Contingency if the HC-SR04 cannot be recovered:** the on-board APDS9960 proximity channel
substitutes for distance at short range (`docs/01-ROADMAP.md` Day 0). It changes the sensor
story but does not block the dataset — and a documented sensor substitution with a stated
reason is perfectly respectable in the deck.

---

## 14. Verified environment (31 July)

| Tool | Version / value |
|---|---|
| `arduino-cli` | 1.5.1 — `C:\Users\vishn\Downloads\arduino-cli_1.5.1_Windows_64bit\arduino-cli.exe` |
| Core | `arduino:mbed_nano` 4.6.0 |
| FQBN | `arduino:mbed_nano:nano33ble` |
| Port | **COM4** |
| Python | 3.14.5 — `C:\Users\vishn\AppData\Local\Python\pythoncore-3.14-64\python.exe` |
| Packages | numpy, pandas, scikit-learn, scipy, matplotlib, pyserial, tabulate ✅ |
| Libraries | Adafruit SSD1306/GFX/BusIO, Arduino_LSM9DS1, Arduino_BMI270_BMM150, Arduino_TensorFlowLite 2.4.0 ✅ |

**Baseline footprint** (`01_sensor_check`, measured): **112 KB flash (11%)**, **46 KB RAM
(17%)**, leaving ~215 KB RAM for the model and tensor arena. Quote this as the starting
budget on the deployment slide.

**Measured per-operation cost on this board** (timing probe, useful for the latency slide):

| Operation | Cost |
|---|---|
| `digitalRead()` | ~1.5 µs |
| `micros()` | ~9.4 µs — surprisingly expensive; avoid in tight loops |
| `analogRead()` | ~30 µs |
| IMU accel read | ~1.6 ms — the dominant per-sample sensor cost |

### Two firmware bugs found and fixed during bring-up

Both are worth a sentence in the deck: they show the platform was engineered, not assumed.

1. **`pulseIn()` does not honour its timeout on the mbed core.** A missing echo stalled the
   loop for seconds per sample, silently destroying the 10 Hz sample rate the entire data
   protocol depends on. Replaced with a bounded manual `micros()` measurement.
2. **The textbook I²C scanner does not work on the mbed core.** A zero-length write is never
   put on the bus, so the scan reports nothing even when devices are present and working.
   The authoritative checks are the explicit `IMU.begin()` / `display.begin()` results —
   the scan output is advisory only and now says so.

---

## 12. Working rules for any agent/session on this repo

- **Never** widen scope. If it is not in §3 "In scope", it does not get built.
- **Never** report a phase gate as passed without the command output that proves it.
- Every number that appears on a slide must be traceable to a file in `reports/`.
- Commit after each phase gate. Conventional Commits, no AI attribution.
- Time is the binding constraint, not capability. If a task threatens the 9-day window,
  cut the *nice-to-have* and protect P3 (dataset), P6 (on-device), P8 (deck).
- The three highest-risk items are, in order: **hardware damage (§11.2)**, **dataset
  collection time**, and **Edge Impulse deployment friction**. Front-load all three.
