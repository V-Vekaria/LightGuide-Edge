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
| Target band | **1st (70–79%)** minimum on every rubric row; **High 1st (80–100%)** on ML, Evaluation and Demo |

---

## 2. Locked aims

**Primary aim (from CW1, unchanged):**
Build an embedded device that classifies studio-lighting **setup quality** in real time, entirely on-device, from distance / brightness / tilt, and tells the photographer what to adjust.

**Secondary aims (from CW1, unchanged):**
1. Compare **multiple ML paradigms** — supervised, unsupervised (anomaly), and sequence-based — head to head.
2. Achieve **low-latency on-device inference with zero cloud dependency** at run time.
3. Deliver a **working prototype** covering **six setup conditions** with fast, actionable feedback.

**SDG alignment (NEW — required by the handbook, absent from CW1; see `docs/08-ETHICS-SDG.md`):**
- **Primary: SDG 12 — Responsible Consumption and Production.** Repeated test-shot cycles keep 300–1000 W continuous / high-draw strobe lighting running longer than needed and shorten modifier and lamp service life. Getting the setup right first time cuts studio energy per shoot.
- **Secondary: SDG 8 — Decent Work and Economic Growth (target 8.3).** Freelancers and micro-studios absorb the cost of re-shoots directly; consistent setups protect their productivity.

---

## 3. Locked scope

### In scope
- Arduino Nano 33 BLE Sense (nRF52840) as the only compute target.
- Sensors: **HC-SR04 ultrasonic** (distance), **LDR** (brightness), **on-board IMU** (tilt).
- Feedback: **SSD1306 I²C OLED**, on-board LED, buzzer *(buzzer optional — not present in the current physical build; see Risk R-04)*.
- **Six** classification classes (§4).
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

## 4. Locked class definitions (6 classes)

Reference setup is captured once by the user; every class is a deviation from it.

| # | Label | Physical condition | Primary driving sensor |
|---|---|---|---|
| 0 | `optimal` | Distance, brightness and tilt all inside the reference band | all three |
| 1 | `too_close` | Light stand nearer than the reference band | ultrasonic |
| 2 | `too_far` | Light stand further than the reference band | ultrasonic |
| 3 | `underlit` | Output below reference (dimmed, off, blocked, modifier added) | LDR |
| 4 | `overlit` | Output above reference (dialled up, modifier removed) | LDR |
| 5 | `tilt_off` | Head/stand angle deviates from reference aim | IMU |

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
| **M2** | Unsupervised | Autoencoder, reconstruction-error anomaly detection | Rejects setups outside the six classes ("unknown"). |
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
- **Ablation:** distance-only → +LDR → +IMU, to prove every sensor earns its place.
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
| P0 Setup | Repo scaffolded, docs locked, board flashes and prints all 3 sensors | 31 Jul | 🟡 IMU + LDR + OLED verified; ultrasonic failing |
| P0.1 Hardware safety | HC-SR04 ECHO level-shift verified; IMU revision identified; pin map confirmed | 31 Jul | 🔴 **ECHO stuck HIGH — see §13** |
| P1 Calibration | LDR→lux curve + ultrasonic→cm curve recorded against references | 1 Aug | ⬜ |
| P2 Logger | Labelled CSV capture over serial working end-to-end | 1 Aug | ⬜ |
| P3 Dataset | ≥250 samples/class × 6 classes, ≥3 sessions, held-out session reserved | 2 Aug | ⬜ |
| P4 Offline ML | M0–M3 trained, CV + test metrics, confusion matrices, ablation | 3 Aug | ⬜ |
| P5 Edge Impulse | EI project public, model trained, INT8 quantised, library exported | 4 Aug | ⬜ |
| P6 On-device | Inference + OLED/LED feedback loop running standalone | 5 Aug | ⬜ |
| P7 Online eval | Live confusion matrix, latency, RAM/flash measured and tabulated | 6 Aug | ⬜ |
| P8 Deck + video | 12-min deck built, split-screen demo video cut | 7 Aug | ⬜ |
| P9 Rehearse | 2 timed run-throughs, Q&A pack drilled | 8 Aug | ⬜ |
| P10 Submit | All 4 components uploaded, receipts screenshotted | **8 Aug (1 day early)** | ⬜ |

Legend: ⬜ not started · 🟡 in progress · ✅ done · 🔴 blocked

---

## 10. Session log

| Date | What changed | Next action |
|---|---|---|
| 2026-07-31 | Requirements + rubric extracted from handbook; CW1 promises reconciled; repo scaffolded; docs locked; bring-up firmware + capture tool + offline pipeline written and smoke-tested end to end on synthetic data | — |
| 2026-07-31 | Flashed `01_sensor_check` to COM4. **Rev1 confirmed, IMU/OLED/LDR all verified working. Ultrasonic ECHO stuck HIGH — blocker, see §13.** Fixed two mbed-core firmware bugs (`pulseIn` timeout, I²C scanner) | — |
| 2026-07-31 | Built `01b_calibration` + `tools/calibrate.py`: meter-free LDR characterisation via the inverse-square law, ambient fitted as a free parameter, APDS9960 cross-check, ultrasonic calibrated from the same sweep. Fitter validated against a simulated cell (worst error 0.009 across a γ/ambient/noise grid) | — |
| 2026-07-31 | HC-SR04 rewired by user — ECHO now idles LOW correctly, loop recovered to 128 ms. Distance still failing; built `00_wiring_probe`, which diagnosed the module as **unpowered** (D2/D3 clamped low = dead chip signature). B-number confirmed, submission filenames locked | **Move HC-SR04 VCC to the 3V3 pin (§13), re-run the probe, then the calibration sweep** |

---

## 11. Open decisions

1. ~~B-number~~ → ✅ **B00969091** (confirmed 31 Jul). Exact submission filenames locked in §7.
2. ~~HC-SR04 supply voltage~~ → superseded by the live fault in §13.
3. ~~Board revision~~ → ✅ **Rev1 confirmed.** `IMU.begin()` succeeded via
   `Arduino_LSM9DS1` at 119 Hz. Both sketches are correctly set to `BOARD_REV 1`.
4. **Buzzer** — user confirms it is wired, but the probe shows **D9 floating**, so it is
   not on D9. Which pin? (A0–A3/A6/A7 were not swept.) Update the pin map in
   `docs/02-HARDWARE.md` §2 once known. 🔴
5. ~~Reference light meter~~ → ✅ **Not needed.** `tools/calibrate.py` recovers the LDR's
   response exponent using the inverse-square law as the reference, with a tape measure and
   a lamp-off ambient reading. Absolute lux is not the quantity this system needs. See
   `docs/02-HARDWARE.md` §4. If a meter turns up later, one reading rescales the existing
   curve — no recollection.

---

## 13. 🔴 BLOCKER — HC-SR04 has no power

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
