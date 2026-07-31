# LightGuide Edge

**An embedded AI lighting-setup assistant for studio photography.**
COM683 Edge & Embedded Intelligence — Coursework 2 (60%) — Ulster University.

Photographers can't reliably recreate a physical lighting setup — distance, brightness and
angle — from one session to the next. Lighting apps store *digital* state; none guide the
physical placement of the stand. LightGuide Edge senses distance, light level and tilt,
classifies the setup against a saved reference with a model running entirely on an Arduino
Nano 33 BLE Sense, and tells the photographer what to adjust.

---

## Read this first

**[`AGENTS.md`](AGENTS.md)** — the contract. Locked aims, scope, deliverables, and the
progress tracker. Start every session there.

| Doc | What it holds |
|---|---|
| [`docs/00-REQUIREMENTS-LOCKED.md`](docs/00-REQUIREMENTS-LOCKED.md) | CW2 spec and the full marking rubric, with the 1st / High-1st band language |
| [`docs/01-ROADMAP.md`](docs/01-ROADMAP.md) | Day-by-day plan to the 9 August deadline, with gates and contingencies |
| [`docs/02-HARDWARE.md`](docs/02-HARDWARE.md) | Pin map, BOM, **and the HC-SR04 voltage hazard — read before powering up** |
| [`docs/03-DATA-PROTOCOL.md`](docs/03-DATA-PROTOCOL.md) | Data collection methodology (15% of the mark) |
| [`docs/04-ML-PLAN.md`](docs/04-ML-PLAN.md) | Four models, features, validation, deployment (20%) |
| [`docs/05-EVALUATION-PLAN.md`](docs/05-EVALUATION-PLAN.md) | Offline **and** online evaluation (20%) |
| [`docs/06-PRESENTATION-PLAN.md`](docs/06-PRESENTATION-PLAN.md) | Slide-by-slide map with timings, and the demo script |
| [`docs/07-RISK-REGISTER.md`](docs/07-RISK-REGISTER.md) | What can go wrong and what to do about it |
| [`docs/08-ETHICS-SDG.md`](docs/08-ETHICS-SDG.md) | SDG alignment, ethical / social / technical context |
| [`docs/09-REFERENCES.md`](docs/09-REFERENCES.md) | Literature plan — 10–15 sources to find and read |
| [`docs/10-QA-DEFENCE.md`](docs/10-QA-DEFENCE.md) | Viva prep (10% of the mark, decided entirely in Q&A) |

---

## Layout

```
firmware/00_wiring_probe/   what is connected to which pin? (no multimeter needed)
firmware/01_sensor_check/   hardware bring-up + diagnostics (compiles: 11% flash, 17% RAM)
firmware/01b_calibration/   LDR + ultrasonic calibration    (compiles:  9% flash, 17% RAM)
firmware/02_data_logger/    labelled 10 Hz CSV capture      (compiles: 11% flash, 17% RAM)
firmware/03_inference/      on-device model + feedback      (built on day 5)
tools/capture.py            drives the logger, writes data/raw/
tools/calibrate.py          guided sweep, curve fitting, calibration report
tools/dataset_report.py     dataset quality gate G2
tools/train_offline.py      M0-M3 comparison, ablation, confusion matrices
data/                       raw/ processed/ calibration/
models/  reports/  deliverables/
```

## Quick start

Install the Python side:

```bash
pip install -r tools/requirements.txt
```

Flash the bring-up sketch and confirm every sensor is alive:

```bash
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble firmware/01_sensor_check
```

```bash
arduino-cli upload -p COM4 --fqbn arduino:mbed_nano:nano33ble firmware/01_sensor_check
```

Watch the stream (the I²C scan at the top tells you the board revision and the OLED address):

```bash
arduino-cli monitor -p COM4 --config baudrate=115200
```

Calibrate both sensors in one guided sweep (flash `01b_calibration` first — no lux meter
needed, a tape measure is the reference):

```bash
python tools/calibrate.py --port COM4
```

Capture labelled data:

```bash
python tools/capture.py --port COM4 --session 1 --interactive
```

Check the dataset against gate G2:

```bash
python tools/dataset_report.py
```

Train and compare all offline models:

```bash
python tools/train_offline.py
```

---

## Status

Deadline **12:00 noon, 9 August 2026**; target submission **8 August**.
Live progress is tracked in [`AGENTS.md`](AGENTS.md) §9 — that table is the single source
of truth, not this README.

## Author

Vishnu Vekariya · BSc (Hons) Computing Systems · Ulster University, School of Computing.
Individual assignment; all work is the author's own.
