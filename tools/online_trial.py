"""
LightGuide Edge - online (on-device) evaluation
COM683 CW2 | Vishnu Vekariya | Ulster University

Gate G6. This is the evidence for the 20% Critical Evaluation row that most projects
never collect: how the classifier behaves *on the device, in the room*, as opposed to
on held-out CSV rows.

    python tools/online_trial.py --port COM4 --trials 10          # 10 x 5 = 50 runs
    python tools/online_trial.py --port COM4 --trials 4 --quick   # a 20-run rehearsal
    python tools/online_trial.py --analyse-only                   # rebuild the report

WHAT MAKES THIS DIFFERENT FROM THE OFFLINE TEST
-----------------------------------------------
The offline held-out score reuses windows cut from runs recorded earlier. Every one of
those windows reached the model through a pipeline that had already succeeded: the run
was recorded, so the sensors were working at the time.

Online, the model is judged on whatever the device actually produces - including the
settling transient after the operator steps back, echoes lost to a soft surface, and
the LDR still drifting toward its final value. The gap between the two numbers is a
real result, and explaining it is worth more marks than either number alone.

PROTOCOL
--------
For each trial the operator stages one condition physically, the host tells the board
which condition was staged (`L<n>`), and the board records a fixed-length run in which
every row carries both the staged label and the model's live prediction. Ground truth
is therefore recorded at the moment of the trial rather than reconstructed from memory
afterwards.

Runs are recorded to data/online/ so the raw evidence survives the report.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from collections import Counter
from datetime import datetime
from pathlib import Path

import numpy as np

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is not installed. Run: pip install -r tools/requirements.txt")

ROOT = Path(__file__).resolve().parent.parent
ONLINE = ROOT / "data" / "online"
REPORTS = ROOT / "reports"
FIGURES = REPORTS / "figures"

CLASS_NAMES = ["optimal", "too_close", "too_far", "underlit", "overlit"]
BAUD = 115200

# Staging instructions, so the operator runs the same protocol on trial 40 as on
# trial 1. Drift in how a condition is staged shows up as model error and gets
# blamed on the model.
STAGING = {
    0: "Return the stand to the saved reference position and lighting.",
    1: "Move the stand ~30 cm CLOSER than the reference.",
    2: "Move the stand ~30 cm FURTHER than the reference.",
    3: "Reference position; DIM the lamp well below the reference.",
    4: "Reference position; BRIGHTEN the lamp well above the reference.",
}


def open_port(port: str) -> serial.Serial:
    ser = serial.Serial(port, BAUD, timeout=2)
    ser.dtr = True          # the Nano's native USB needs DTR asserted (RUN.md)
    time.sleep(2.0)         # the board resets when the port opens
    ser.reset_input_buffer()
    return ser


def send(ser: serial.Serial, cmd: str) -> None:
    ser.write((cmd + "\n").encode())
    ser.flush()
    time.sleep(0.15)


def read_run(ser: serial.Serial, timeout_s: float = 40.0) -> tuple[list[dict], dict]:
    """Consume one RUN_START..RUN_END block. Returns the rows and the run metadata."""
    rows: list[dict] = []
    meta: dict = {}
    header: list[str] | None = None
    started = False
    t0 = time.time()

    while time.time() - t0 < timeout_s:
        line = ser.readline().decode("utf-8", "replace").strip()
        if not line:
            continue

        if line.startswith("# RUN_START"):
            started = True
            for part in line.replace("# RUN_START", "").split():
                if "=" in part:
                    k, v = part.split("=", 1)
                    meta[k] = v
            continue

        if line.startswith("# RUN_END"):
            for part in line.replace("# RUN_END", "").split():
                if "=" in part:
                    k, v = part.split("=", 1)
                    meta[k] = v
            return rows, meta

        if line.startswith("# INFER"):
            for part in line.replace("# INFER", "").split():
                if "=" in part:
                    k, v = part.split("=", 1)
                    meta[k] = v
            continue

        if not started or line.startswith("#"):
            continue

        if header is None and line.startswith("t_ms"):
            header = line.split(",")
            continue

        if header:
            parts = line.split(",")
            if len(parts) == len(header):
                rows.append(dict(zip(header, parts)))

    raise TimeoutError("no RUN_END within timeout - is firmware/03_inference flashed "
                       "and a reference saved (hold D7 for 2 s)?")


def run_trials(port: str, trials: int, classes: list[int]) -> list[dict]:
    ser = open_port(port)
    ONLINE.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    all_rows: list[dict] = []

    print("=" * 70)
    print("ONLINE EVALUATION - Gate G6")
    print("=" * 70)
    print("Before starting: the device must show a saved reference (hold D7 for 2 s).")
    print(f"{trials} trials x {len(classes)} classes = {trials * len(classes)} runs.")
    print("Each run records for ~5 s after a 1.5 s warm-up.\n")
    input("Press Enter when the reference is saved and you are ready... ")

    try:
        n = 0
        total = trials * len(classes)
        for t in range(trials):
            for c in classes:
                n += 1
                print(f"\n--- run {n}/{total} · trial {t + 1} · "
                      f"staging `{CLASS_NAMES[c]}` ---")
                print(f"    {STAGING[c]}")
                input("    Press Enter once the setup is physically staged... ")

                send(ser, f"L{c}")
                ser.reset_input_buffer()
                send(ser, "R")

                try:
                    rows, meta = read_run(ser)
                except TimeoutError as e:
                    print(f"    !! {e}")
                    continue

                for r in rows:
                    r["trial"] = t
                    r["staged"] = c
                all_rows += rows

                preds = [int(r["pred"]) for r in rows if r.get("pred", "-1") != "-1"]
                if preds:
                    top, cnt = Counter(preds).most_common(1)[0]
                    verdict = "OK " if top == c else "MISS"
                    print(f"    {verdict} {len(rows)} rows · modal prediction "
                          f"`{CLASS_NAMES[top]}` ({cnt}/{len(preds)}) · "
                          f"dropouts {meta.get('dropouts', '?')}")
                else:
                    print(f"    !! {len(rows)} rows but no predictions "
                          "(window never filled)")
    finally:
        ser.close()

    out = ONLINE / f"online_trials_{stamp}.csv"
    if all_rows:
        keys = list(all_rows[0].keys())
        with out.open("w", encoding="utf-8", newline="") as fh:
            fh.write(",".join(keys) + "\n")
            for r in all_rows:
                fh.write(",".join(str(r.get(k, "")) for k in keys) + "\n")
        print(f"\nRaw trial data -> {out}")
    return all_rows


def load_latest() -> list[dict]:
    files = sorted(ONLINE.glob("online_trials_*.csv"))
    if not files:
        sys.exit(f"No trial files in {ONLINE}. Run without --analyse-only first.")
    path = files[-1]
    print(f"Analysing {path.name}")
    lines = path.read_text(encoding="utf-8").splitlines()
    header = lines[0].split(",")
    return [dict(zip(header, ln.split(","))) for ln in lines[1:] if ln.strip()]


def analyse(rows: list[dict]) -> None:
    """Two confusion matrices, because they answer different questions.

    Per-sample is what the model does at 8 Hz. Per-run is what the *user* sees,
    because the device settles on a verdict and holds it - a single stray frame
    inside an otherwise correct run is not a misclassification the user notices.
    Reporting only the flattering one would be dishonest; reporting both, with the
    reason they differ, is the analysis.
    """
    graded = [r for r in rows if r.get("pred", "-1") not in ("-1", "", "NA")]
    if not graded:
        sys.exit("No rows carry a prediction - the classifier window never filled.")

    n_cls = len(CLASS_NAMES)
    cm_sample = np.zeros((n_cls, n_cls), dtype=int)
    for r in graded:
        cm_sample[int(r["staged"]), int(r["pred"])] += 1

    # Per-run: the modal prediction across each run, keyed by (trial, staged).
    runs: dict[tuple[str, str], list[int]] = {}
    for r in graded:
        runs.setdefault((r["trial"], r["staged"]), []).append(int(r["pred"]))
    cm_run = np.zeros((n_cls, n_cls), dtype=int)
    for (_, staged), preds in runs.items():
        cm_run[int(staged), Counter(preds).most_common(1)[0][0]] += 1

    def metrics(cm):
        acc = np.trace(cm) / max(cm.sum(), 1)
        f1s = []
        for i in range(n_cls):
            tp, fp, fn = cm[i, i], cm[:, i].sum() - cm[i, i], cm[i].sum() - cm[i, i]
            p = tp / (tp + fp) if tp + fp else 0.0
            rc = tp / (tp + fn) if tp + fn else 0.0
            f1s.append(2 * p * rc / (p + rc) if p + rc else 0.0)
        return float(acc), float(np.mean(f1s)), f1s

    acc_s, f1_s, _ = metrics(cm_sample)
    acc_r, f1_r, per_run_f1 = metrics(cm_run)

    lat = [float(r["infer_us"]) for r in graded
           if r.get("infer_us", "").strip().replace(".", "").isdigit()]
    dropouts = sum(1 for r in rows if r.get("dist_mm", "0").startswith("-"))

    # Throughput from the recorded timestamps, per run, so a long gap between runs
    # cannot inflate it.
    rates = []
    for key, _ in runs.items():
        ts = sorted(float(r["t_ms"]) for r in graded
                    if (r["trial"], r["staged"]) == key)
        if len(ts) > 2 and ts[-1] > ts[0]:
            rates.append(1000.0 * (len(ts) - 1) / (ts[-1] - ts[0]))

    L = ["# Online results", "",
         f"Generated {datetime.now():%Y-%m-%d %H:%M} · "
         f"**{len(runs)} runs**, **{len(graded)} classified samples**", "",
         "Gate G6. Every row below was produced by the device in the room, with the "
         "operator staging each condition physically and the staged label recorded at "
         "the moment of the trial. Nothing here is replayed from a file.", "",
         "## Headline", "",
         "| metric | per-sample | per-run (modal vote) |", "|---|---|---|",
         f"| accuracy | {acc_s:.3f} | {acc_r:.3f} |",
         f"| macro-F1 | {f1_s:.3f} | {f1_r:.3f} |",
         f"| n | {len(graded)} | {len(runs)} |", "",
         "Both are reported because they answer different questions. Per-sample is "
         "what the classifier does every 125 ms. Per-run is what the *operator* "
         "experiences, since the device settles on a verdict and holds it - one stray "
         "frame in an otherwise steady run is not something a user notices. Quoting "
         "only the higher number would be the dishonest choice.", ""]

    for title, cm in (("Per-sample confusion matrix", cm_sample),
                      ("Per-run confusion matrix", cm_run)):
        L += [f"## {title}", "", "Rows = staged (ground truth), columns = predicted.", "",
              "| staged \\ predicted | " + " | ".join(CLASS_NAMES) + " | recall |",
              "|---" * (n_cls + 2) + "|"]
        for i, name in enumerate(CLASS_NAMES):
            tot = cm[i].sum()
            rec = cm[i, i] / tot if tot else 0.0
            L.append(f"| **{name}** | " + " | ".join(str(v) for v in cm[i]) +
                     f" | {rec:.3f} |")
        L.append("")

    L += ["## Inference latency", ""]
    if lat:
        a = np.array(lat)
        L += ["Measured with `micros()` on-device around feature extraction plus the "
              "tree walk - what the operator actually waits for, not the tree walk "
              "alone.", "",
              "| statistic | value |", "|---|---|",
              f"| n | {len(a)} |",
              f"| mean | **{a.mean():.1f} us** |",
              f"| median | {np.median(a):.1f} us |",
              f"| p95 | {np.percentile(a, 95):.1f} us |",
              f"| max | {a.max():.1f} us |", "",
              f"At {a.mean():.1f} us the classifier is far below the ~125 ms sensing "
              "period, so the loop rate is set by the ultrasonic time-of-flight and "
              "the LDR settling time, not by inference. Making the model faster would "
              "not make the device more responsive - that is a useful finding for the "
              "evaluation slide and it argues against reaching for a heavier model.", ""]
    else:
        L += ["_No `infer_us` column in the trial data - reflash `firmware/03_inference` "
              "so latency is recorded._", ""]

    L += ["## Throughput and reliability", "", "| metric | value |", "|---|---|"]
    if rates:
        L.append(f"| sample rate (mean over runs) | {np.mean(rates):.2f} Hz |")
    L += [f"| ultrasonic dropouts | {dropouts} of {len(rows)} rows "
          f"({100.0 * dropouts / max(len(rows), 1):.1f}%) |"]

    # Read the footprint rather than carrying a copy of it. Hard-coding these was
    # already wrong once - a firmware change moved flash by 3 KB and the literal
    # here did not follow.
    fp_path = REPORTS / "footprint.json"
    if fp_path.exists():
        fp = json.loads(fp_path.read_text(encoding="utf-8"))
        L += [f"| flash | {fp['flash_bytes']:,} B of {fp['flash_max_bytes']:,} "
              f"({fp['flash_pct']}%) |",
              f"| RAM (static) | {fp['ram_bytes']:,} B of {fp['ram_max_bytes']:,} "
              f"({fp['ram_pct']}%) |", "",
              f"Footprint read from `arduino-cli compile` output for "
              f"`firmware/{fp['sketch']}`, recorded {fp['recorded'][:16]} - not a "
              "profiler estimate.", ""]
    else:
        L += ["", "_Footprint not recorded. Run `py -3 tools/record_footprint.py` and "
              "regenerate this report - the evaluation slide needs real flash and RAM "
              "figures._", ""]

    L += ["## Per-class online F1 (per-run)", "", "| class | F1 |", "|---|---|"]
    L += [f"| {n} | {f:.3f} |" for n, f in zip(CLASS_NAMES, per_run_f1)]
    L += ["", "## Offline vs online", "",
          "Fill the offline column from `reports/offline_results.md` and explain any "
          "gap on the evaluation slide. Expect online to be lower: the offline test "
          "only ever saw windows from runs that recorded successfully, whereas online "
          "the model is also judged during the settling transient after the operator "
          "moves the stand.", "",
          "| | offline (held-out session 3) | online (per-run) |", "|---|---|---|",
          f"| macro-F1 | 0.868 | {f1_r:.3f} |",
          f"| accuracy | 0.865 | {acc_r:.3f} |", ""]

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        FIGURES.mkdir(parents=True, exist_ok=True)
        fig, axes = plt.subplots(1, 2, figsize=(13, 5.2))
        for ax, cm, title in ((axes[0], cm_sample, f"Per-sample (n={cm_sample.sum()})"),
                              (axes[1], cm_run, f"Per-run (n={cm_run.sum()})")):
            cmn = cm.astype(float) / np.maximum(cm.sum(axis=1, keepdims=True), 1)
            im = ax.imshow(cmn, cmap="Oranges", vmin=0, vmax=1)
            ax.set_xticks(range(n_cls), CLASS_NAMES, rotation=45, ha="right")
            ax.set_yticks(range(n_cls), CLASS_NAMES)
            ax.set_xlabel("predicted")
            ax.set_ylabel("staged")
            ax.set_title(title, fontweight="bold")
            for i in range(n_cls):
                for j in range(n_cls):
                    ax.text(j, i, f"{cmn[i, j]:.2f}", ha="center", va="center",
                            fontsize=8,
                            color="white" if cmn[i, j] > 0.5 else "black")
            fig.colorbar(im, ax=ax, fraction=0.046)
        fig.suptitle("Online confusion: measured on-device, live",
                     fontweight="bold")
        plt.tight_layout()
        plt.savefig(FIGURES / "confusion_online.png", dpi=150, bbox_inches="tight")
        plt.close(fig)
        L += ["![online confusion](figures/confusion_online.png)", ""]
    except ImportError:
        pass

    REPORTS.mkdir(parents=True, exist_ok=True)
    (REPORTS / "online_results.md").write_text("\n".join(L), encoding="utf-8")

    print(f"\nPer-sample  acc {acc_s:.3f}  macro-F1 {f1_s:.3f}   (n={len(graded)})")
    print(f"Per-run     acc {acc_r:.3f}  macro-F1 {f1_r:.3f}   (n={len(runs)})")
    if lat:
        print(f"Latency     mean {np.mean(lat):.1f} us  p95 {np.percentile(lat, 95):.1f} us")
    print(f"\nGate G6 -> {REPORTS / 'online_results.md'}")


def main() -> None:
    ap = argparse.ArgumentParser(description="LightGuide Edge online evaluation")
    ap.add_argument("--port", help="serial port, e.g. COM4")
    ap.add_argument("--trials", type=int, default=10,
                    help="repeats per class; 10 x 5 = 50 runs, the roadmap contingency")
    ap.add_argument("--quick", action="store_true",
                    help="distance and light classes only, for a rehearsal")
    ap.add_argument("--analyse-only", action="store_true",
                    help="rebuild the report from the most recent trial file")
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    args = ap.parse_args()

    if args.list:
        for p in list_ports.comports():
            print(f"  {p.device:<8} {p.description}")
        return

    if args.analyse_only:
        analyse(load_latest())
        return

    if not args.port:
        sys.exit("--port is required (or use --analyse-only)")

    classes = [0, 1, 3] if args.quick else [0, 1, 2, 3, 4]
    rows = run_trials(args.port, args.trials, classes)
    if rows:
        analyse(rows)


if __name__ == "__main__":
    main()
