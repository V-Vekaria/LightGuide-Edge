"""
LightGuide Edge - presentation figures
COM683 CW2 | Vishnu Vekariya | Ulster University

The handbook names "collection, processing and **visualisation** of data" as an assessed
component, and it is the one most often skipped. train_offline.py already emits confusion
matrices and dataset_report.py emits class balance; this fills in everything else the deck
needs, at presentation resolution.

    python tools/make_figures.py

Writes to reports/figures/:

    traces_by_class.png     what each class physically looks like over time
    feature_space.png       why the classifier works, in one picture
    model_comparison.png    M0-M3 side by side with error bars
    ablation.png            each sensor earning its place
    session_references.png  three sessions, three references - why deviations, not absolutes

Every figure is captioned in the deck by what to LOOK at, not by what it is
(docs/06-PRESENTATION-PLAN.md section 5).
"""

from __future__ import annotations

import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "data" / "raw"
PROCESSED = ROOT / "data" / "processed"
MODELS = ROOT / "models"
FIGURES = ROOT / "reports" / "figures"

CLASS_NAMES = ["optimal", "too_close", "too_far", "underlit", "overlit"]
# Colour-blind safe (Okabe-Ito). A deck projected in a lecture theatre loses subtle
# hues, and a marker may be colour-blind - neither is worth risking for prettier defaults.
COLOURS = ["#0072B2", "#D55E00", "#009E73", "#CC79A7", "#E69F00"]

plt.rcParams.update({
    "figure.dpi": 150,
    "font.size": 11,        # >= 16 pt once scaled onto a slide
    "axes.grid": True,
    "grid.alpha": 0.3,
    "axes.spines.top": False,
    "axes.spines.right": False,
})


def load_raw() -> pd.DataFrame:
    files = sorted(RAW.glob("session*_*.csv"))
    if not files:
        raise SystemExit(f"No captures in {RAW}")
    frames = []
    for f in files:
        df = pd.read_csv(f, comment="#")
        df["run"] = f.stem
        frames.append(df)
    df = pd.concat(frames, ignore_index=True)
    return df[df["dist_mm"] >= 0]


def fig_traces(df: pd.DataFrame) -> None:
    """One representative run per class, both channels, as deviations.

    This is the figure that ties an abstract label to a physical thing that happened
    in a room. It also makes the confounded design visible: `underlit` and `overlit`
    sit flat on the distance axis while swinging on light, and vice versa.
    """
    fig, axes = plt.subplots(2, len(CLASS_NAMES), figsize=(16, 5.5), sharex=True)

    for i, name in enumerate(CLASS_NAMES):
        runs = df[df["label"] == i]["run"].unique()
        if len(runs) == 0:
            continue
        run = df[df["run"] == runs[len(runs) // 2]]      # a middle run, not a cherry-picked one
        t = run["t_ms"].to_numpy() / 1000.0

        axes[0, i].plot(t, run["d_dist_mm"], color=COLOURS[i], lw=1.2)
        axes[0, i].axhline(0, color="k", lw=0.8, ls="--")
        axes[0, i].set_title(name, color=COLOURS[i], fontweight="bold")

        axes[1, i].plot(t, run["d_ldr"], color=COLOURS[i], lw=1.2)
        axes[1, i].axhline(0, color="k", lw=0.8, ls="--")
        axes[1, i].set_xlabel("time (s)")

    axes[0, 0].set_ylabel("distance deviation\nfrom reference (mm)")
    axes[1, 0].set_ylabel("light deviation\nfrom reference (counts)")

    # Shared limits, or the eye reads scale differences as class differences.
    for row in range(2):
        lo = min(ax.get_ylim()[0] for ax in axes[row])
        hi = max(ax.get_ylim()[1] for ax in axes[row])
        for ax in axes[row]:
            ax.set_ylim(lo, hi)

    fig.suptitle("What each class looks like: one representative run, deviations from the saved reference",
                 fontweight="bold")
    plt.tight_layout()
    plt.savefig(FIGURES / "traces_by_class.png", bbox_inches="tight")
    plt.close(fig)
    print("  traces_by_class.png")


def fig_feature_space() -> None:
    """The two features the decision tree actually splits on, with the splits drawn.

    Showing the learned thresholds on top of the data turns "the model got 0.868"
    into "here is the rule it found, and here is why it is the right rule". An
    interpretable model is an asset - use it.
    """
    tr = pd.read_csv(PROCESSED / "train.csv")
    te = pd.read_csv(PROCESSED / "test.csv")

    fig, ax = plt.subplots(figsize=(8, 6))
    for i, name in enumerate(CLASS_NAMES):
        s = tr[tr["label"] == i]
        ax.scatter(s["d_dist_mm_mean"], s["d_ldr_mean"], c=COLOURS[i], s=28,
                   alpha=0.75, label=f"{name} (train)", edgecolors="none")
        s = te[te["label"] == i]
        ax.scatter(s["d_dist_mm_mean"], s["d_ldr_mean"], c=COLOURS[i], s=52,
                   alpha=0.9, marker="^", edgecolors="k", linewidths=0.5)

    # Thresholds lifted from firmware/03_inference/model.h - the deployed rule.
    ax.axvline(-51.8, color="k", ls="--", lw=1.4)
    ax.axhline(-137.5, color="k", ls=":", lw=1.4)
    ax.axhline(141.5, color="k", ls=":", lw=1.4)
    ax.text(-51.8, ax.get_ylim()[1] * 0.96, " d_dist_mean = -51.8 mm",
            fontsize=9, va="top")
    ax.text(ax.get_xlim()[1] * 0.98, 141.5, "d_ldr = +141.5 ", fontsize=9,
            ha="right", va="bottom")
    ax.text(ax.get_xlim()[1] * 0.98, -137.5, "d_ldr = -137.5 ", fontsize=9,
            ha="right", va="top")

    ax.set_xlabel("mean distance deviation from reference (mm)")
    ax.set_ylabel("mean light deviation from reference (counts)")
    ax.set_title("Feature space and the deployed decision boundaries\n"
                 "circles = training sessions 1-2, triangles = held-out session 3",
                 fontweight="bold")
    ax.legend(loc="upper left", fontsize=9, framealpha=0.9)
    plt.tight_layout()
    plt.savefig(FIGURES / "feature_space.png", bbox_inches="tight")
    plt.close(fig)
    print("  feature_space.png")


def fig_model_comparison() -> None:
    """Held-out macro-F1 with the seed spread shown, not hidden in a table cell."""
    results = json.loads((MODELS / "results.json").read_text())
    names = list(results)
    f1 = [results[n]["f1"] for n in names]
    err = [results[n]["f1_std"] for n in names]
    cv = [results[n]["cv_mean"] for n in names]

    x = np.arange(len(names))
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.bar(x - 0.2, cv, 0.4, label="cross-validation (sessions 1-2)", color="#9ecae1")
    ax.bar(x + 0.2, f1, 0.4, yerr=err, capsize=4,
           label="held-out session 3", color="#0072B2")
    ax.set_xticks(x, [n.replace(" ", "\n", 1) for n in names], fontsize=9)
    ax.set_ylabel("macro-F1")
    ax.set_ylim(0, 1.0)
    ax.set_title("Five approaches, identical splits. The gap between the bars is the "
                 "generalisation cost.", fontweight="bold")
    ax.legend()
    plt.tight_layout()
    plt.savefig(FIGURES / "model_comparison.png", bbox_inches="tight")
    plt.close(fig)
    print("  model_comparison.png")


def fig_ablation() -> None:
    """Parsed from the report rather than recomputed, so the figure and the table
    can never disagree on a slide."""
    text = (ROOT / "reports" / "offline_results.md").read_text(encoding="utf-8")
    rows = []
    in_section = False
    for line in text.splitlines():
        if line.startswith("## Sensor ablation"):
            in_section = True
            continue
        if in_section and line.startswith("## "):
            break
        if in_section and line.startswith("|") and "---" not in line and "sensors" not in line:
            parts = [p.strip() for p in line.strip("|").split("|")]
            if len(parts) == 4:
                rows.append((parts[0], float(parts[3])))

    if not rows:
        print("  ablation.png SKIPPED - could not parse the ablation table")
        return

    labels = [r[0] for r in rows]
    vals = [r[1] for r in rows]
    fig, ax = plt.subplots(figsize=(7.5, 4.5))
    bars = ax.bar(labels, vals, color=["#D55E00", "#E69F00", "#009E73"][:len(vals)])
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v + 0.015, f"{v:.3f}",
                ha="center", fontweight="bold")
    ax.set_ylabel("held-out macro-F1")
    ax.set_ylim(0, 1.0)
    ax.set_title("Neither sensor is sufficient alone; together they are.\n"
                 "Same model, same splits.", fontweight="bold")
    plt.xticks(rotation=10)
    plt.tight_layout()
    plt.savefig(FIGURES / "ablation.png", bbox_inches="tight")
    plt.close(fig)
    print("  ablation.png")


def fig_session_references() -> None:
    """The justification for training on deviations instead of absolutes.

    Each session saved a different physical reference. Plotted as absolutes the
    classes overlap into mush; plotted as deviations they separate. This single
    figure defends the central modelling decision, so it is worth a slide of its own.
    """
    files = sorted(RAW.glob("session*_*.csv"))
    refs = {}
    for f in files:
        header = f.read_text(encoding="utf-8").splitlines()[0]
        if not header.startswith("#"):
            continue
        kv = dict(p.split("=", 1) for p in header.lstrip("# ").split() if "=" in p)
        s = int(kv.get("session", 0))
        refs.setdefault(s, (float(kv.get("ref_mm", 0)), float(kv.get("ref_ldr", 0))))

    df = load_raw()
    fig, axes = plt.subplots(1, 2, figsize=(13, 5))

    for i, name in enumerate(CLASS_NAMES):
        s = df[df["label"] == i]
        axes[0].scatter(s["dist_mm"], s["ldr"], c=COLOURS[i], s=6, alpha=0.35,
                        label=name, edgecolors="none")
        axes[1].scatter(s["d_dist_mm"], s["d_ldr"], c=COLOURS[i], s=6, alpha=0.35,
                        label=name, edgecolors="none")

    axes[0].set_xlabel("absolute distance (mm)")
    axes[0].set_ylabel("absolute light (counts)")
    axes[0].set_title("Absolute readings: one cluster per session,\n"
                      "so `optimal` has no single location", fontweight="bold")

    axes[1].set_xlabel("deviation from saved reference (mm)")
    axes[1].set_ylabel("deviation from saved reference (counts)")
    axes[1].set_title("Deviations: the sessions collapse onto each other,\n"
                      "distance and light separate along their own axes",
                      fontweight="bold")
    axes[1].legend(markerscale=3, fontsize=9)

    ref_txt = " · ".join(f"S{s}: {r[0]:.0f} mm / {r[1]:.0f} counts"
                         for s, r in sorted(refs.items()))
    fig.suptitle(f"Why the model is trained on deviations, not absolutes  —  {ref_txt}",
                 fontweight="bold")
    plt.tight_layout()
    plt.savefig(FIGURES / "session_references.png", bbox_inches="tight")
    plt.close(fig)
    print("  session_references.png")


def main() -> None:
    FIGURES.mkdir(parents=True, exist_ok=True)
    df = load_raw()
    print(f"Rendering figures from {len(df)} samples...")
    fig_traces(df)
    fig_feature_space()
    fig_model_comparison()
    fig_ablation()
    fig_session_references()
    print(f"\nAll figures -> {FIGURES}")


if __name__ == "__main__":
    main()
