"""
LightGuide Edge - dataset quality gate
COM683 CW2 | Vishnu Vekariya | Ulster University

Checks data/raw/ against gate G2 in docs/01-ROADMAP.md and the quality controls in
docs/03-DATA-PROTOCOL.md, then writes reports/dataset_report.md plus the class-balance
figure. Exits non-zero if a gate fails, so it can be trusted as a gate rather than a
suggestion.

Usage:
    python tools/dataset_report.py
    python tools/dataset_report.py --min-per-class 200
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pandas as pd

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "data" / "raw"
REPORTS = ROOT / "reports"
FIGURES = REPORTS / "figures"

CLASS_NAMES = ["optimal", "too_close", "too_far", "underlit", "overlit", "tilt_off"]


def load_raw() -> pd.DataFrame:
    files = sorted(RAW.glob("session*_*.csv"))
    if not files:
        sys.exit(f"No capture files in {RAW}. Run tools/capture.py first.")
    frames = []
    for f in files:
        df = pd.read_csv(f)
        df["source_file"] = f.name
        frames.append(df)
    return pd.concat(frames, ignore_index=True)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-per-class", type=int, default=250,
                    help="gate threshold; the roadmap contingency allows dropping to 200")
    ap.add_argument("--min-sessions", type=int, default=3)
    args = ap.parse_args()

    df = load_raw()
    lines: list[str] = ["# Dataset report", ""]
    failures: list[str] = []

    lines.append(f"- Capture files: **{df['source_file'].nunique()}**")
    lines.append(f"- Total samples: **{len(df)}**")
    lines.append("")

    # --- class balance ---
    lines.append("## Class balance")
    lines.append("")
    lines.append("| label | class | samples | sessions | status |")
    lines.append("|---|---|---|---|---|")
    counts = df.groupby("label").size()
    for i, name in enumerate(CLASS_NAMES):
        n = int(counts.get(i, 0))
        sess = df.loc[df["label"] == i, "session"].nunique() if n else 0
        ok = n >= args.min_per_class and sess >= args.min_sessions
        if not ok:
            failures.append(f"class {i} ({name}): {n} samples across {sess} sessions")
        lines.append(f"| {i} | {name} | {n} | {sess} | {'PASS' if ok else 'FAIL'} |")
    lines.append("")

    # Imbalance matters more than raw count - a model trained on a skewed set will
    # look fine on accuracy while quietly failing the rare classes.
    if len(counts) and counts.min() > 0:
        ratio = counts.max() / counts.min()
        lines.append(f"Imbalance ratio (max/min): **{ratio:.2f}**")
        if ratio > 1.5:
            failures.append(f"class imbalance ratio {ratio:.2f} exceeds 1.5")
        lines.append("")

    # --- sessions ---
    lines.append("## Sessions")
    lines.append("")
    lines.append("| session | samples | classes present |")
    lines.append("|---|---|---|")
    for s, grp in df.groupby("session"):
        lines.append(f"| {s} | {len(grp)} | {sorted(grp['label'].unique())} |")
    n_sessions = df["session"].nunique()
    if n_sessions < args.min_sessions:
        failures.append(f"only {n_sessions} sessions, need {args.min_sessions}")
    lines.append("")

    # --- data health ---
    lines.append("## Data health")
    lines.append("")
    nan_total = int(df.isna().sum().sum())
    lines.append(f"- NaN cells: **{nan_total}**")
    if nan_total:
        failures.append(f"{nan_total} NaN cells present")

    if "dist_mm" in df:
        drop = int((df["dist_mm"] < 0).sum())
        pct = 100.0 * drop / len(df)
        lines.append(f"- Ultrasonic dropouts: **{drop} ({pct:.1f}%)**")
        if pct > 10:
            lines.append("  - above the 10% threshold in risk R-08; report this as a finding")

    if "ldr_raw" in df:
        lo = int((df["ldr_raw"] <= 5).sum())
        hi = int((df["ldr_raw"] >= 4090).sum())
        lines.append(f"- LDR pinned low: **{lo}**, pinned high: **{hi}**")
        if lo + hi > 0.02 * len(df):
            # A saturated LDR carries no information at all in that region.
            failures.append("LDR saturating - change the divider resistor (risk R-07) "
                            "and re-record the calibration")
    lines.append("")

    # --- per-class channel summary, the blind spot-check from the protocol ---
    lines.append("## Per-class channel summary")
    lines.append("")
    lines.append("Sanity-check these against the physical conditions you staged. "
                 "`too_close` must show a lower mean distance than `too_far`; "
                 "`underlit` must show a lower mean LDR than `overlit`. "
                 "If not, runs were mislabelled.")
    lines.append("")
    cols = [c for c in ("dist_mm", "ldr_raw", "pitch", "roll") if c in df]
    summary = df[df["dist_mm"] >= 0].groupby("label")[cols].mean().round(1)
    summary.index = [CLASS_NAMES[i] for i in summary.index]
    lines.append(summary.to_markdown())
    lines.append("")

    # --- figure ---
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        FIGURES.mkdir(parents=True, exist_ok=True)
        fig, ax = plt.subplots(figsize=(7, 4))
        names = [CLASS_NAMES[i] for i in sorted(counts.index)]
        ax.bar(names, [counts[i] for i in sorted(counts.index)])
        ax.axhline(args.min_per_class, ls="--", color="crimson",
                   label=f"target {args.min_per_class}/class")
        ax.set_ylabel("samples")
        ax.set_title("Class balance")
        ax.legend()
        plt.xticks(rotation=20, ha="right")
        plt.tight_layout()
        out = FIGURES / "class_balance.png"
        plt.savefig(out, dpi=150)
        plt.close(fig)
        lines.append(f"![class balance](figures/class_balance.png)")
        lines.append("")
    except ImportError:
        lines.append("_matplotlib not installed - figure skipped_")
        lines.append("")

    # --- verdict ---
    lines.append("## Gate G2")
    lines.append("")
    if failures:
        lines.append("**FAIL**")
        lines.extend(f"- {f}" for f in failures)
    else:
        lines.append("**PASS** - dataset meets the collection protocol.")

    REPORTS.mkdir(parents=True, exist_ok=True)
    (REPORTS / "dataset_report.md").write_text("\n".join(lines), encoding="utf-8")

    print("\n".join(lines[-12:]))
    print(f"\nWritten to {REPORTS / 'dataset_report.md'}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
