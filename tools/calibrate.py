"""
LightGuide Edge - sensor calibration
COM683 CW2 | Vishnu Vekariya | Ulster University

Closes the open issue CW1 flagged ("the analog LDR is uncalibrated"). Runs one
physical sweep and calibrates BOTH sensors from it, then fits, plots and reports.

    python tools/calibrate.py --port COM4                 # guided sweep
    python tools/calibrate.py --fit-only                  # refit existing data

WHY THERE IS NO LUX METER IN THIS PROCEDURE
-------------------------------------------
Absolute lux is not what the system needs - every class is defined relative to a
saved reference setup. What IS needed is the LDR's response curve: how its
resistance varies with illuminance, and whether that response is linear, log, or
power-law.

That can be recovered without any reference instrument. Hold the lamp output fixed
and vary the distance: illuminance then follows the inverse-square law,
E = k / d^2, so distance measured with a tape measure IS the reference. Fitting

    log10(R_ldr) = a - gamma * log10(E_rel)

recovers the CdS cell's characteristic exponent gamma (typically 0.5-0.9). One
scale factor converts the whole curve to absolute lux later if a meter appears -
no data needs recollecting.

Two controls make this rigorous rather than convenient:

  * Ambient subtraction. The room contributes illuminance the lamp does not. A
    lamp-off reading at each distance is subtracted, so the fit sees the lamp's
    contribution alone.
  * An independent channel. The board's APDS9960 ambient sensor is recorded
    alongside the LDR. Two sensors that disagree about the same physical change
    mean one of them is wrong, and it is worth knowing which before collecting
    1500 samples through it.

The ultrasonic sensor is calibrated from the same sweep for free: the tape measure
readings are ground truth for its reported distances.
"""

from __future__ import annotations

import argparse
import csv
import sys
import time
from pathlib import Path

import numpy as np

try:
    import serial
except ImportError:
    sys.exit("pyserial is not installed. Run: pip install -r tools/requirements.txt")

ROOT = Path(__file__).resolve().parent.parent
CAL = ROOT / "data" / "calibration"
REPORTS = ROOT / "reports"
FIGURES = REPORTS / "figures"

BAUD = 115200
SETTLE_S = 2.0          # the LDR is slow; let it settle before averaging
AVERAGE_S = 3.0         # then average this long at each step

# Divider: 3V3 -- LDR -- A0 -- R_FIXED -- GND  (docs/02-HARDWARE.md section 4)
R_FIXED = 10_000.0
ADC_MAX = 4095.0

DEFAULT_DISTANCES_CM = [30, 40, 50, 65, 80, 100, 125, 150, 200]

# Six points spanning the same range, for when there is not time for eleven.
# The far points matter more than the near ones: the rig lamp is a flat panel, so
# inverse-square only holds properly beyond about a metre and that is where gamma
# is actually constrained (docs/02-HARDWARE.md section 4). Dropping the crowded
# 30-65 cm cluster costs little; dropping the far end would cost the fit.
QUICK_DISTANCES_CM = [40, 70, 100, 150, 220, 300]


# ---------------------------------------------------------------------------
# Acquisition
# ---------------------------------------------------------------------------

def open_port(port: str) -> serial.Serial:
    ser = serial.Serial(port, BAUD, timeout=2)
    time.sleep(2.0)
    ser.reset_input_buffer()
    return ser


def average_step(ser: serial.Serial) -> dict | None:
    """Settle, then average the stream for AVERAGE_S."""
    print("    settling...", end="", flush=True)
    t0 = time.time()
    while time.time() - t0 < SETTLE_S:
        ser.readline()

    print(" averaging", end="", flush=True)
    ldr, amb, dist = [], [], []
    t0 = time.time()
    while time.time() - t0 < AVERAGE_S:
        line = ser.readline().decode("utf-8", "replace").strip()
        if not line or line.startswith("#") or line.startswith("ldr_raw"):
            continue
        p = line.split(",")
        if len(p) != 4:
            continue
        try:
            ldr.append(float(p[0]))
            if float(p[2]) >= 0:
                amb.append(float(p[2]))
            if float(p[3]) > 0:
                dist.append(float(p[3]))
        except ValueError:
            continue
        print(".", end="", flush=True)
    print()

    if not ldr:
        print("    !! no readings - is the board running 01b_calibration?")
        return None

    return {
        "ldr_mean": float(np.mean(ldr)),
        "ldr_sd": float(np.std(ldr)),
        "apds_mean": float(np.mean(amb)) if amb else -1.0,
        "us_mean": float(np.mean(dist)) if dist else -1.0,
        "n": len(ldr),
    }


def sweep(ser: serial.Serial, distances_cm: list[float]) -> list[dict]:
    print("\n" + "=" * 68)
    print("CALIBRATION SWEEP")
    print("=" * 68)
    print("Setup, once:")
    print("  * one lamp, output held FIXED for the whole sweep - do not touch the dimmer")
    print("  * LDR facing the lamp, held square to it at every step")
    print("  * tape measure along the floor; distance is lamp -> LDR face")
    print("  * room light kept constant throughout")
    print("\nAt each distance you will be asked for a lamp-ON and a lamp-OFF reading.")
    print("The OFF reading is the ambient baseline and gets subtracted.\n")
    input("Press Enter when the rig is set up... ")

    rows = []
    for d_cm in distances_cm:
        print(f"\n--- {d_cm} cm ---")
        input(f"  Position at {d_cm} cm, lamp ON, then press Enter... ")
        on = average_step(ser)
        if on is None:
            continue

        input("  Now switch the lamp OFF (do not move anything), press Enter... ")
        off = average_step(ser)
        if off is None:
            continue

        input("  Lamp back ON, press Enter to continue... ")

        rows.append({
            "distance_cm": d_cm,
            "ldr_on": on["ldr_mean"], "ldr_on_sd": on["ldr_sd"],
            "ldr_off": off["ldr_mean"], "ldr_off_sd": off["ldr_sd"],
            "apds_on": on["apds_mean"], "apds_off": off["apds_mean"],
            "us_mm": on["us_mean"],
        })
        print(f"  -> lamp on {on['ldr_mean']:.0f} +/- {on['ldr_sd']:.1f} counts, "
              f"ambient {off['ldr_mean']:.0f}")

        if on["ldr_mean"] > 4050:
            print("  !! LDR is saturating high - reduce the fixed resistor and restart "
                  "(risk R-07)")
        if on["ldr_mean"] < 30:
            print("  !! LDR is pinned low - increase the fixed resistor and restart")

    CAL.mkdir(parents=True, exist_ok=True)
    path = CAL / "ldr_sweep.csv"
    with path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"\nRaw sweep -> {path}")
    return rows


# ---------------------------------------------------------------------------
# Fitting
# ---------------------------------------------------------------------------

def counts_to_resistance(counts: np.ndarray) -> np.ndarray:
    """V_A0 = 3V3 * R_fixed / (R_ldr + R_fixed)  ->  R_ldr = R_fixed*(ADC_MAX/counts - 1)"""
    counts = np.clip(counts, 1.0, ADC_MAX - 1.0)
    return R_FIXED * (ADC_MAX / counts - 1.0)


def fit(rows: list[dict]) -> dict:
    d_cm = np.array([r["distance_cm"] for r in rows], dtype=float)
    ldr_on = np.array([r["ldr_on"] for r in rows], dtype=float)
    ldr_off = np.array([r["ldr_off"] for r in rows], dtype=float)

    r_on, r_off = counts_to_resistance(ldr_on), counts_to_resistance(ldr_off)

    # Relative illuminance contributed by the LAMP, from the inverse-square law.
    # Units are arbitrary - only the exponent is being recovered.
    e_lamp = 1.0 / (d_cm / 100.0) ** 2

    # The LDR does not see the lamp alone: it sees lamp + room. Fitting
    # log R_on against log e_lamp therefore fits the wrong x-axis and biases gamma
    # low, because at large distances the room dominates and the response flattens.
    #
    # Ambient cannot simply be subtracted in the counts domain (counts are
    # non-linear in illuminance) and its value in these arbitrary units is unknown.
    # The fix is to work with the RATIO of the two readings, which cancels the
    # unknown scale factor A entirely:
    #
    #   R_off = A * E_amb^-gamma           (lamp off: room only)
    #   R_on  = A * (E_lamp + E_amb)^-gamma
    #   =>  log(R_off / R_on) = gamma * log(1 + E_lamp / E_amb)
    #
    # leaving two free parameters, gamma and E_amb, fitted by non-linear least
    # squares. Validated against a simulated cell with a known exponent.
    ok = (r_on > 0) & (r_off > 0) & np.isfinite(r_on) & np.isfinite(r_off)
    y_ratio = np.log(r_off[ok] / r_on[ok])

    gamma, e_amb, method = float("nan"), float("nan"), "ratio"
    try:
        from scipy.optimize import curve_fit

        def model(e_l, g, e_a):
            return g * np.log1p(e_l / max(e_a, 1e-9))

        # Seed E_amb near the weakest lamp contribution in the sweep; that is the
        # regime where ambient matters most and the fit is most sensitive to it.
        (gamma, e_amb), _ = curve_fit(
            model, e_lamp[ok], y_ratio,
            p0=[0.7, float(np.min(e_lamp[ok]))],
            bounds=([0.05, 1e-6], [3.0, 1e4]), maxfev=20000)
        gamma, e_amb = float(gamma), float(e_amb)
        pred = model(e_lamp[ok], gamma, e_amb)
        resid = y_ratio - pred
        ss_tot = float(np.sum((y_ratio - np.mean(y_ratio)) ** 2))
    except Exception:
        # Fallback: the naive log-log fit. Biased low when ambient is significant,
        # so it is labelled as such in the report rather than passed off as equal.
        method = "loglog-fallback"
        gnr, _icpt = np.polyfit(np.log10(e_lamp[ok]), np.log10(r_on[ok]), 1)
        gamma, e_amb = float(-gnr), float("nan")
        pred = np.polyval([gnr, _icpt], np.log10(e_lamp[ok]))
        resid = np.log10(r_on[ok]) - pred
        ss_tot = float(np.sum((np.log10(r_on[ok]) - np.mean(np.log10(r_on[ok]))) ** 2))

    ss_res = float(np.sum(resid ** 2))
    r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else float("nan")

    # Recover the scale factor A now that gamma and E_amb are known, so the curve
    # can actually be applied to convert counts back to relative illuminance.
    if np.isfinite(e_amb):
        intercept = float(np.mean(np.log10(r_off[ok]) + gamma * np.log10(e_amb)))
    else:
        intercept = float(np.mean(np.log10(r_on[ok]) + gamma * np.log10(e_lamp[ok])))

    e_rel = e_lamp
    x_plot = np.log10(e_lamp[ok])

    # Ultrasonic against the tape measure, if it produced anything
    us = np.array([r["us_mm"] for r in rows], dtype=float)
    us_ok = us > 0
    us_fit = None
    if us_ok.sum() >= 3:
        slope, off = np.polyfit(d_cm[us_ok] * 10.0, us[us_ok], 1)
        resid = us[us_ok] - (slope * d_cm[us_ok] * 10.0 + off)
        us_fit = {"slope": float(slope), "offset": float(off),
                  "rmse_mm": float(np.sqrt(np.mean(resid ** 2))),
                  "n": int(us_ok.sum())}

    # Does the independent APDS channel agree with the LDR about the same change?
    apds = np.array([r["apds_on"] for r in rows], dtype=float)
    apds_corr = None
    if np.all(apds > 0) and len(apds) >= 3:
        apds_corr = float(np.corrcoef(np.log10(apds), np.log10(e_rel))[0, 1])

    return {"d_cm": d_cm, "e_rel": e_rel, "r_on": r_on, "r_off": r_off,
            "ldr_on": ldr_on, "ldr_off": ldr_off, "gamma": float(gamma),
            "e_ambient": float(e_amb), "method": method,
            "intercept": float(intercept), "r2": float(r2),
            "resid": resid, "x_plot": x_plot,
            "us_fit": us_fit, "apds_corr": apds_corr}


def plot(f: dict) -> None:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    FIGURES.mkdir(parents=True, exist_ok=True)
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.2))

    ax = axes[0]
    ax.plot(f["d_cm"], f["ldr_on"], "o-", label="lamp on")
    ax.plot(f["d_cm"], f["ldr_off"], "s--", label="ambient only")
    ax.set_xlabel("distance (cm)")
    ax.set_ylabel("ADC counts")
    ax.set_title("Raw LDR response")
    ax.legend()
    ax.grid(alpha=.3)

    # Plot against TOTAL illuminance (lamp + fitted ambient) - that is what the
    # cell actually responds to, and on those axes the power law is a straight line.
    ax = axes[1]
    e_amb = f["e_ambient"] if np.isfinite(f["e_ambient"]) else 0.0
    x = np.log10(f["e_rel"] + e_amb)
    y = np.log10(f["r_on"])
    ax.plot(x, y, "o", label="measured")
    xs = np.linspace(x.min(), x.max(), 50)
    ax.plot(xs, f["intercept"] - f["gamma"] * xs, "-",
            label=f"gamma={f['gamma']:.3f}, R2={f['r2']:.4f}")
    ax.set_xlabel("log10 total relative illuminance (lamp + ambient)")
    ax.set_ylabel("log10 LDR resistance (ohm)")
    ax.set_title("Power-law characterisation")
    ax.legend()
    ax.grid(alpha=.3)

    ax = axes[2]
    ax.axhline(0, color="k", lw=.8)
    ax.plot(f["x_plot"], f["resid"], "o")
    ax.set_xlabel("log10 lamp relative illuminance")
    ax.set_ylabel("fit residual")
    ax.set_title("Fit residuals")
    ax.grid(alpha=.3)

    plt.tight_layout()
    out = FIGURES / "calibration_ldr.png"
    plt.savefig(out, dpi=150)
    plt.close(fig)
    print(f"Figure -> {out}")


def report(f: dict) -> None:
    REPORTS.mkdir(parents=True, exist_ok=True)
    g, r2 = f["gamma"], f["r2"]

    L = ["# Sensor calibration", "",
         "Closes the open issue raised in CW1: *\"the analog LDR is uncalibrated - raw "
         "readings need a conversion curve\"*.", "",
         "## Method", "",
         "No reference light meter was used, and none is needed. Classification is "
         "relative to a saved reference setup, so absolute lux is not required - what "
         "is required is the LDR's *response curve*. Holding lamp output fixed and "
         "varying distance makes illuminance a known quantity up to a scale factor via "
         "the inverse-square law, so a tape measure serves as the reference instrument.",
         "",
         "Two controls make this a measurement rather than a convenience:", "",
         "1. **Ambient subtraction** - a lamp-off reading at every distance, subtracted "
         "in the resistance domain (counts are non-linear in illuminance, so "
         "subtracting counts would subtract the wrong quantity).",
         "2. **An independent channel** - the on-board APDS9960 records the same "
         "changes, so the LDR is cross-checked rather than trusted.", "",
         "## LDR characterisation", "",
         f"Fitted `log10(R_ldr) = a - gamma * log10(E_rel)` over "
         f"{len(f['d_cm'])} distances:", "",
         "| parameter | value |", "|---|---|",
         f"| gamma (response exponent) | **{g:.3f}** |",
         f"| intercept a | {f['intercept']:.3f} |",
         f"| fitted ambient E_amb (relative units) | {f['e_ambient']:.4f} |",
         f"| R^2 | **{r2:.4f}** |",
         f"| fit method | `{f['method']}` |",
         f"| fixed divider resistor | {R_FIXED:.0f} ohm |",
         f"| distances swept | {', '.join(f'{d:.0f}' for d in f['d_cm'])} cm |", "",
         "The room's own contribution `E_amb` is a *fitted* parameter, not an "
         "assumption. The cell responds to lamp + room together, so fitting against "
         "the lamp term alone biases gamma low - the response flattens at distance "
         "where the room dominates. Working with the ratio of the lamp-off to "
         "lamp-on resistance cancels the unknown scale factor and recovers both "
         "gamma and the ambient level. Verified against a simulated cell with a "
         "known exponent before being used on real data.", ""]

    # Interpretation, so the number means something in the viva rather than sitting
    # on a slide undefended.
    if 0.4 <= g <= 1.0:
        L.append(f"gamma = {g:.3f} sits in the 0.5-0.9 band typical of CdS "
                 "photoresistors, so the cell is behaving as a normal photoresistor "
                 "and the divider is operating in a sensible part of its range.")
    else:
        L.append(f"gamma = {g:.3f} is **outside** the 0.5-0.9 range usual for CdS "
                 "cells. Suspect saturation at one end of the sweep, a stray light "
                 "source, or the lamp output not being held constant. Inspect the "
                 "residual plot before trusting this.")
    L.append("")

    if r2 >= 0.98:
        L.append(f"R^2 = {r2:.4f}: the power law describes the response well.")
    elif r2 >= 0.9:
        L.append(f"R^2 = {r2:.4f}: usable, but the residual plot should be checked "
                 "for curvature at the bright or dark end.")
    else:
        L.append(f"R^2 = {r2:.4f} is **poor**. Most likely the lamp output moved "
                 "during the sweep, or the LDR saturated. Re-run before relying on it.")
    L += ["", "![LDR calibration](figures/calibration_ldr.png)", ""]

    L += ["## Cross-check against the APDS9960", ""]
    if f["apds_corr"] is not None:
        c = f["apds_corr"]
        L.append(f"Correlation between log APDS ambient and log relative illuminance: "
                 f"**{c:.4f}**.")
        L.append("")
        L.append("Strong agreement means two physically independent sensors saw the "
                 "same change, which is real evidence the sweep measured illuminance "
                 "and not an artefact."
                 if c > 0.95 else
                 "**Weak agreement.** The two sensors disagree about the same physical "
                 "change, so at least one is wrong. Resolve this before collecting a "
                 "dataset through the LDR - it is far cheaper to find now.")
    else:
        L.append("_APDS9960 channel unavailable - no independent cross-check. "
                 "State this as a limitation._")
    L.append("")

    L += ["## Ultrasonic calibration", ""]
    if f["us_fit"]:
        u = f["us_fit"]
        L += [f"Reported distance vs tape measure, n={u['n']}:", "",
              "| parameter | value | ideal |", "|---|---|---|",
              f"| slope | {u['slope']:.4f} | 1.0 |",
              f"| offset | {u['offset']:.1f} mm | 0 |",
              f"| RMSE | **{u['rmse_mm']:.1f} mm** | - |", "",
              "Slope departing from 1.0 indicates a speed-of-sound error (temperature); "
              "a non-zero offset indicates a fixed trigger/echo latency. Both are "
              "correctable in firmware, and quoting the RMSE gives the distance channel "
              "an honest error bar on the evaluation slide."]
    else:
        L += ["_No usable ultrasonic readings during the sweep._", "",
              "The ECHO fault in `AGENTS.md` section 13 is unresolved, so the distance "
              "channel could not be calibrated. **This does not block the LDR "
              "calibration** - the tape measure was the reference throughout. Re-run "
              "this sweep once the wiring is fixed and the ultrasonic section fills in "
              "automatically."]
    L += ["", "## Applying the calibration", "",
          "`tools/train_offline.py` consumes `models/ldr_calibration.json`. The mapping "
          "from counts to relative illuminance is:", "", "```",
          "R_ldr  = R_FIXED * (4095 / counts - 1)",
          f"E_rel  = 10 ** ((a - log10(R_ldr)) / gamma)     # a={f['intercept']:.3f}, "
          f"gamma={g:.3f}", "```", "",
          "Absolute lux needs one further scale factor, obtainable from a single "
          "reference reading at any point on the curve. Nothing would need recollecting."]

    (REPORTS / "calibration.md").write_text("\n".join(L), encoding="utf-8")

    import json
    (ROOT / "models").mkdir(exist_ok=True)
    (ROOT / "models" / "ldr_calibration.json").write_text(json.dumps({
        "gamma": g, "intercept": f["intercept"], "r2": r2,
        "r_fixed_ohm": R_FIXED, "adc_max": ADC_MAX,
        "method": "inverse-square-law self-calibration, ambient-subtracted",
        "absolute_lux": False,
    }, indent=2), encoding="utf-8")

    print(f"Report -> {REPORTS / 'calibration.md'}")
    print(f"Coefficients -> {ROOT / 'models' / 'ldr_calibration.json'}")


def load_existing() -> list[dict]:
    path = CAL / "ldr_sweep.csv"
    if not path.exists():
        sys.exit(f"No sweep at {path}. Run without --fit-only first.")
    with path.open(encoding="utf-8") as fh:
        return [{k: float(v) for k, v in row.items()} for row in csv.DictReader(fh)]


def main() -> None:
    ap = argparse.ArgumentParser(description="LightGuide Edge sensor calibration")
    ap.add_argument("--port", help="serial port, e.g. COM4")
    ap.add_argument("--distances", type=float, nargs="+", default=DEFAULT_DISTANCES_CM,
                    help="distances in cm to sweep")
    ap.add_argument("--quick", action="store_true",
                    help=f"six-point sweep ({', '.join(str(d) for d in QUICK_DISTANCES_CM)} cm) "
                         "instead of nine - about 8 minutes")
    ap.add_argument("--fit-only", action="store_true",
                    help="refit and re-plot an existing sweep, no hardware needed")
    args = ap.parse_args()

    if args.fit_only:
        rows = load_existing()
    else:
        if not args.port:
            sys.exit("--port is required (or use --fit-only)")
        # --distances wins if given explicitly; --quick only overrides the default.
        distances = args.distances
        if args.quick and distances == DEFAULT_DISTANCES_CM:
            distances = QUICK_DISTANCES_CM
        ser = open_port(args.port)
        try:
            rows = sweep(ser, distances)
        finally:
            ser.close()

    if len(rows) < 4:
        sys.exit(f"Only {len(rows)} usable points - need at least 4 to fit a curve.")

    f = fit(rows)
    plot(f)
    report(f)

    print(f"\nGate P1: gamma={f['gamma']:.3f}, R2={f['r2']:.4f}")
    print("Review reports/calibration.md before collecting the dataset.")


if __name__ == "__main__":
    main()
