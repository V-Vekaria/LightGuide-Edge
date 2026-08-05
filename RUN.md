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

## Right now — the next thing to run

**1. Confirm the hardware after the 10 kΩ resistor swap**

```bash
arduino-cli upload -p COM4 --fqbn arduino:mbed_nano:nano33ble firmware/01e_acceptance
```

Watch the serial output. You want `*** GATE G0 PASSED ***` with `LDR response PASS`.

**Do not skip this.** The only acceptance run on record ends `GATE G0 NOT PASSED` with
`LDR response FAIL`. `tools/calibrate.py` assumes a 10 kΩ pulldown (`R_FIXED`), so
calibrating with the 1 kΩ still fitted makes γ meaningless — and it would show up months
later as an unexplained accuracy ceiling, not as an error. See `AGENTS.md` §11.6.

**2. Then calibrate (Day 1)**

```bash
python tools/calibrate.py --port COM4 --distances 30 40 50 65 80 100 125 150 200 250 300
```

The extra far points are deliberate: the rig light is a flat panel, so inverse-square only
holds beyond ~1 m and γ must be fitted out there. `docs/02-HARDWARE.md` §4 explains why, and
why the near points are still worth collecting.

Flash `firmware/01b_calibration` first — the script tells you if it isn't running. Guided
sweep, ~10 minutes, needs a tape measure and a lamp you can switch on and off.

**3. Then collect data (Day 2 — Saturday, the big one)**

```bash
arduino-cli upload -p COM4 --fqbn arduino:mbed_nano:nano33ble firmware/02_data_logger
```

```bash
python tools/capture.py --port COM4 --session 1 --interactive
```

**4. Check the dataset passed its quality gate**

```bash
python tools/dataset_report.py
```

**5. Train and compare all four models (Day 3)**

```bash
python tools/train_offline.py
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
| `03_inference` | ⬜ **The actual product.** Written Tue 5 Aug — needs the trained model first. |

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
| `tools/train_offline.py` | Trains M0–M3, ablation, confusion matrices | nothing (reads files) |

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

It does not exist yet, and cannot: it needs the trained model and the frozen scaler
constants baked into it, which only exist after Sunday's training and Monday's Edge Impulse
export. Everything before it is either a diagnostic or a step in producing that model.

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
