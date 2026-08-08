# What do I run?

> **Repo:** https://github.com/V-Vekaria/LightGuide-Edge (private)
> Push after each phase gate so work is never trapped on one machine:
> `git add -A && git commit -m "..." && git push`

Two kinds of thing live in this repo:

- **`firmware/*`** — Arduino sketches. These get *flashed to the board*. Only one can be on
  the board at a time; flashing a new one replaces the old.
- **`tools/*.py`** — Python scripts. These run *on the laptop* and talk to the board over
  USB serial.

**Only one program can use COM4 at a time.** If the Arduino IDE's Serial Monitor is open,
close it before running a Python tool or flashing — otherwise the port is busy and it fails.

---

## Right now — 7 August, three things left that need the rig

G0 passed on 5 August. Calibration, data, offline ML and the on-device product are done.
**Three gates remain and none of them can be done from the repo** — they need the hardware,
a tape measure, or an Edge Impulse account.

**1. Calibration sweep — Gate P1 · ~8 minutes**

This is the one CW1 flagged as an open issue, so it has to be closed. It is *not* about the
distance sensor reading correctly — it is about the LDR's response curve, γ. The distance
half comes free from the same sweep and turns "the tape agrees" into a slope, offset and
RMSE in millimetres.

```bash
arduino-cli upload -p COM4 --fqbn arduino:mbed_nano:nano33ble firmware/01b_calibration
```

```bash
py -3 tools/calibrate.py --port COM4 --quick
```

Needs a tape measure and a lamp you can switch on and off without moving it. Six distances,
lamp on and lamp off at each. Writes `reports/calibration.md`. Check γ lands in 0.5–0.9 and
R² ≥ 0.98.

**2. Online evaluation — Gate P7 · ~60 minutes**

The 20% Critical Evaluation row's differentiator, and the piece most projects never collect.

```bash
arduino-cli upload -p COM4 --fqbn arduino:mbed_nano:nano33ble firmware/03_inference
```

Hold **D7 for 2 s** to save a reference first, then:

```bash
py -3 tools/online_trial.py --port COM4 --trials 10
```

50 runs, prompted one at a time. Writes `reports/online_results.md` with per-sample and
per-run confusion matrices, measured inference latency and the footprint table. Rehearse the
flow with `--trials 2 --quick` first.

After each run the harness prints `!! STAGING:` if the recorded run does not match the
condition it asked for — stand off the reference mark for a lamp-only class, or a lamp
change too small to clear the ±5% band. **Fix the staging and repeat that trial when you
see it.** On 8 August this caught 14 runs staged ~30 cm out of position and 3 `overlit`
runs moved only one dimmer step (~+105 counts, inside the correct band and therefore not
`overlit` at all).

To re-collect only the affected classes rather than repeating all 40 runs:

```bash
py -3 tools/online_trial.py --port COM4 --trials 8 --classes 3,4
```

**Do not re-save the reference between sittings** — the runs are only comparable while
they share one baseline. Then combine whole classes from the sitting each was staged
correctly in, and re-validate:

```bash
py -3 tools/merge_online.py data/online/A.csv:0,1,2 data/online/B.csv:3 data/online/C.csv:4
```

`--analyse-only` then picks up the combined file, since it sorts last by name.

**3. Edge Impulse — Gate P5 · ~45 minutes**

A link to a **public** EI project is a required part of the Code component. The dataset is
already exported to `deliverables/edge_impulse/`. Follow `docs/11-EDGE-IMPULSE-STEPS.md`
step by step.

**Then package:**

```bash
py -3 tools/package_submission.py --ei-url https://studio.edgeimpulse.com/public/XXXXX/live
```

---

### Regenerating anything offline (no hardware needed)

```bash
py -3 tools/dataset_report.py && py -3 tools/train_offline.py && py -3 tools/make_figures.py
```

---

## Full firmware index

Flash whichever one matches what you're doing. All take the same command, just change the
folder name:

```bash
arduino-cli upload -p COM4 --fqbn arduino:mbed_nano:nano33ble firmware/<NAME>
```

| Sketch | Use it when |
|---|---|
| `00_wiring_probe` | "What is connected to which pin?" Tells miswired from unpowered. |
| `00b_pin_identify` | Finds which pin an LED / buzzer / switch is really on. |
| `00c_voltmeter` | Turns the Nano into a voltmeter. **Use this to check a rail is really 0 V.** |
| `00d_sensor_inventory` | Lists every on-board sensor, both I²C buses. |
| `01_sensor_check` | **Everyday bring-up.** All sensors streaming + honest presence checks. |
| `01b_calibration` | Required before `tools/calibrate.py`. |
| `01d_output_test` | Buzzer, LED, switch. |
| `01e_acceptance` | **Gate G0 full acceptance test.** Run after any wiring change. |
| `02_data_logger` | Required before `tools/capture.py`. Labelled 10 Hz capture. |
| `03_inference` | ✅ **The product.** Distance **and** light checked against one saved setup. Boots to `NO SETUP SAVED`; hold D7 for 2 s to capture both references. Two-row OLED, buzzer priority distance-then-light, LED green/red/blue. Runs the trained classifier from `model.h` alongside the `decide()` verdicts, times every inference with `micros()`, and records labelled runs carrying both the staged label and the live prediction. |

To watch serial output without a Python tool:

```bash
arduino-cli monitor -p COM4 --config baudrate=115200
```

---

## Full Python tool index

| Tool | What it does | Needs on the board |
|---|---|---|
| `tools/calibrate.py` | Guided sweep, fits the LDR curve, writes `reports/calibration.md` | `01b_calibration` |
| `tools/capture.py` | Records labelled runs into `data/raw/` | `02_data_logger` |
| `tools/dataset_report.py` | Gate G2 quality check — fails loudly if the dataset is short or unbalanced | nothing (reads files) |
| `tools/train_offline.py` | Trains M0–M3, baselines, ablation, confusion matrices | nothing (reads files) |
| `tools/online_trial.py` | **Gate G6.** Drives live trials, builds the online confusion matrix and latency table | `03_inference` |
| `tools/merge_online.py` | Combines online captures class-by-class, re-runs the staging check, records provenance | nothing (reads files) |
| `tools/make_figures.py` | Presentation figures: traces, feature space, model comparison, ablation | nothing (reads files) |
| `tools/export_edge_impulse.py` | Writes the EI upload set, train/test split preserved | nothing (reads files) |
| `tools/export_tree.py` | Regenerates `firmware/03_inference/model.h` from the trained tree | nothing (reads files) |
| `tools/record_footprint.py` | Compiles and records real flash/RAM into `reports/footprint.json` | nothing (compiles only) |
| `tools/package_submission.py` | Builds and verifies the D1 Code and D2 Dataset zips | nothing (reads files) |
| `tools/read_serial.ps1` | Captures serial output for N seconds and exits (see note below) | anything |

### Serial commands accepted by `03_inference`

| Key | Effect |
|---|---|
| `L0`–`L4` | Stage a class label for the next recorded run |
| `S<n>` | Set the session id |
| `R` | Record a run now (same as a short D7 press) |
| `T` | Print inference latency statistics |
| `?` | Print status and latency |

**Use `read_serial.ps1` when you need to *capture* output rather than watch it.**
`arduino-cli monitor` is interactive and never returns, so it cannot be scripted or logged:

```bash
powershell -ExecutionPolicy Bypass -File tools/read_serial.ps1 -Seconds 20
```

⚠️ If you write your own serial code, **assert DTR**. .NET's `SerialPort` leaves it off by
default, the Nano's native USB reads that as "no host attached", `while (!Serial)` never
completes, and the port looks completely dead while the board is running perfectly. That cost
two debugging rounds on 6 August.

Useful flags:

```bash
python tools/capture.py --list
```

```bash
python tools/calibrate.py --fit-only
```

```bash
python tools/dataset_report.py --min-per-class 200
```

---

## Which one is "the program"?

`firmware/03_inference` — sense, classify, guide, entirely on-device. It is the deliverable
and the thing you demo.

It exists and it runs. `model.h` carries a depth-4 decision tree with the training-time
z-scoring folded into the constants, so the device needs no scaler at run time.

Measured footprint is in `reports/footprint.json`, written by `tools/record_footprint.py`
from the linker's own output. **Re-run that after any firmware change** — the online-results
report and the submission README both read it, so the deck cannot go stale quietly.

Everything else in `firmware/` is either a diagnostic or a step in producing that model.

---

## If something goes wrong

| Symptom | Do this |
|---|---|
| Upload fails, port busy | Close the Arduino Serial Monitor |
| Upload fails, port not found | `python tools/capture.py --list` to see real ports |
| A sensor reads nonsense | Flash `01_sensor_check` — it names the fault |
| Not sure what's wired where | Flash `00_wiring_probe` |
| Suspect a power rail | Flash `00c_voltmeter`, jumper A1 to the point in question |
| Everything looks wrong at once | Suspect **ground** first — one bad rail caused three "separate" failures on 31 July |
