"""
LightGuide Edge - offline model comparison (M0-M3)
COM683 CW2 | Vishnu Vekariya | Ulster University

Implements docs/04-ML-PLAN.md. One command produces the comparison table, the
confusion matrices and the sensor ablation that drive the 20% ML row and the 20%
evaluation row.

    python tools/train_offline.py
    python tools/train_offline.py --test-session 3 --seeds 5

Design notes worth defending in the viva:

* Splitting is by SESSION, not at random. At 10 Hz consecutive samples are near
  duplicates, so a random split leaks near-identical rows into the test set and
  reports memorisation as accuracy. Cross-validation inside the training sessions
  is GROUPED BY RUN for the same reason.
* The scaler is fit on training data only. Fitting it on everything is the classic
  leakage bug and it inflates results quietly.
* Macro-F1 is the headline metric. Accuracy flatters whichever class happens to be
  most common; macro-F1 weights all six equally, which is what the application needs.
* Offline models are scikit-learn. The deployable network is trained and INT8
  quantised in Edge Impulse - these results establish which architecture family is
  worth deploying, not the deployed weights themselves.
"""

from __future__ import annotations

import argparse
import json
import warnings
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import (accuracy_score, classification_report,
                             confusion_matrix, f1_score)
from sklearn.model_selection import GroupKFold
from sklearn.neighbors import KNeighborsClassifier
from sklearn.neural_network import MLPClassifier, MLPRegressor
from sklearn.preprocessing import StandardScaler
from sklearn.tree import DecisionTreeClassifier, export_text

warnings.filterwarnings("ignore", category=UserWarning)

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "data" / "raw"
PROCESSED = ROOT / "data" / "processed"
REPORTS = ROOT / "reports"
FIGURES = REPORTS / "figures"
MODELS = ROOT / "models"

CLASS_NAMES = ["optimal", "too_close", "too_far", "underlit", "overlit"]

WINDOW = 10       # 10 samples at ~8 Hz = ~1.3 seconds
STRIDE = 5        # 50% overlap, applied within a run only

# Deviations from the saved reference, not absolute readings.
#
# This is the decision that makes the model match the product. `optimal` is not
# "1308 mm" - it is "at whatever the operator saved". Training on absolutes would
# produce a model that only works at one setup and breaks the moment the
# reference button is pressed. The three sessions deliberately use three
# different references (1308, 876 and 897 mm), so a model that had learned
# absolute positions could not possibly generalise across them.
CHANNELS = ["d_dist_mm", "d_ldr"]
SENSOR_GROUPS = {                      # for the ablation study
    "distance only":      ["d_dist_mm"],
    "light only":         ["d_ldr"],
    "distance + light":   ["d_dist_mm", "d_ldr"],
}


# ---------------------------------------------------------------------------
# Data
# ---------------------------------------------------------------------------

def load_raw() -> pd.DataFrame:
    files = sorted(RAW.glob("session*_*.csv"))
    if not files:
        raise SystemExit(f"No capture files in {RAW}. Run tools/capture.py first.")
    frames = []
    for f in files:
        # Each capture carries its reference on a leading `#` line, so the file
        # is self-describing; pandas skips it.
        df = pd.read_csv(f, comment="#")
        df["run"] = f.stem          # the grouping key for GroupKFold
        frames.append(df)
    return pd.concat(frames, ignore_index=True)


def clean(df: pd.DataFrame) -> pd.DataFrame:
    """Range-gate the ultrasonic channel and bridge short gaps within a run."""
    df = df.copy()
    # A failed echo is written as -1 in dist_mm and NA in d_dist_mm. Both have to
    # go, or the deviation column keeps a value derived from a reading that never
    # happened.
    df.loc[df["dist_mm"] < 0, ["dist_mm", "d_dist_mm"]] = np.nan
    # Bridge up to 2 consecutive failed echoes (~250 ms); longer gaps are dropped
    # rather than invented.
    for col in ("dist_mm", "d_dist_mm"):
        df[col] = df.groupby("run")[col].transform(
            lambda s: s.ffill(limit=2).bfill(limit=2))
    before = len(df)
    df = df.dropna(subset=["dist_mm", "d_dist_mm"])
    dropped = before - len(df)
    if dropped:
        print(f"  dropped {dropped} rows ({100*dropped/before:.1f}%) with unrecoverable echo gaps")
    return df


def window_features(df: pd.DataFrame, channels: list[str]) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, list[str]]:
    """Summary statistics per window. Windows never straddle a run boundary."""
    X, y, groups, sessions = [], [], [], []
    names: list[str] = []

    for run, grp in df.groupby("run", sort=False):
        grp = grp.reset_index(drop=True)
        label = int(grp["label"].iloc[0])
        session = int(grp["session"].iloc[0])
        for start in range(0, len(grp) - WINDOW + 1, STRIDE):
            w = grp.iloc[start:start + WINDOW]
            if w["label"].nunique() != 1:
                continue                     # condition changed mid-window; discard
            feats, fnames = [], []
            for ch in channels:
                v = w[ch].to_numpy(dtype=float)
                feats += [v.mean(), v.std(), v.min(), v.max()]
                fnames += [f"{ch}_mean", f"{ch}_std", f"{ch}_min", f"{ch}_max"]
            if "amag" in w:
                feats.append(float(w["amag"].mean()))
                fnames.append("amag_mean")
            X.append(feats)
            y.append(label)
            groups.append(run)
            sessions.append(session)
            if not names:
                names = fnames

    return (np.asarray(X, dtype=float), np.asarray(y), np.asarray(groups),
            np.asarray(sessions), names)


# ---------------------------------------------------------------------------
# Models
# ---------------------------------------------------------------------------

def build_models(seed: int) -> dict[str, object]:
    """M0 baselines and M1. M2 is handled separately; M3 lives in Edge Impulse."""
    return {
        "M0a Decision Tree":       DecisionTreeClassifier(max_depth=6, random_state=seed),
        "M0b k-NN (k=5)":          KNeighborsClassifier(n_neighbors=5),
        "M0c Logistic Regression": LogisticRegression(max_iter=2000, random_state=seed),
        "M0d Random Forest":       RandomForestClassifier(n_estimators=100, random_state=seed),
        # early_stopping is deliberately OFF. It reserves 10% of training data as
        # an internal validation split, which on ~200 windows across 5 classes is
        # about 19 samples - too few to measure improvement against, so the net
        # stops after n_iter_no_change before it has learned anything. With it on,
        # this model scored macro-F1 0.064, well below the 0.20 that guessing
        # gives. Generalisation is protected by the held-out session and grouped
        # CV, not by an internal split this small.
        "M1  MLP (32-16)":         MLPClassifier(hidden_layer_sizes=(32, 16), max_iter=3000,
                                                 early_stopping=False, random_state=seed),
    }


def eval_model(model, Xtr, ytr, Xte, yte) -> dict:
    model.fit(Xtr, ytr)
    pred = model.predict(Xte)
    return {
        "accuracy": accuracy_score(yte, pred),
        "macro_f1": f1_score(yte, pred, average="macro"),
        "pred": pred,
    }


def cross_validate(model_fn, X, y, groups, seed: int, folds: int = 5) -> tuple[float, float]:
    """Grouped k-fold: whole runs stay together, so overlapping windows cannot
    straddle a fold boundary."""
    gkf = GroupKFold(n_splits=min(folds, len(np.unique(groups))))
    scores = []
    for tr, te in gkf.split(X, y, groups):
        scaler = StandardScaler().fit(X[tr])
        m = model_fn(seed)
        m.fit(scaler.transform(X[tr]), y[tr])
        scores.append(f1_score(y[te], m.predict(scaler.transform(X[te])), average="macro"))
    return float(np.mean(scores)), float(np.std(scores))


def autoencoder_novelty(Xtr_optimal, Xte, yte, seed: int) -> dict:
    """M2: train a reconstruction model on `optimal` only, then flag anything it
    reconstructs badly. This is the novelty gate that lets the device say
    UNKNOWN SETUP instead of returning a confident wrong class."""
    ae = MLPRegressor(hidden_layer_sizes=(8, 3, 8), max_iter=2000,
                      early_stopping=True, random_state=seed)
    ae.fit(Xtr_optimal, Xtr_optimal)

    train_err = np.mean((ae.predict(Xtr_optimal) - Xtr_optimal) ** 2, axis=1)
    threshold = float(np.percentile(train_err, 95))

    test_err = np.mean((ae.predict(Xte) - Xte) ** 2, axis=1)
    flagged = test_err > threshold

    # Treated as a binary detector: is this an `optimal` setup or not?
    is_optimal = (yte == 0)
    tp = int(np.sum(flagged & ~is_optimal))     # correctly flagged a deviation
    fn = int(np.sum(~flagged & ~is_optimal))
    fp = int(np.sum(flagged & is_optimal))      # false alarm on a good setup
    tn = int(np.sum(~flagged & is_optimal))

    precision = tp / (tp + fp) if tp + fp else 0.0
    recall = tp / (tp + fn) if tp + fn else 0.0
    f1 = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
    return {"threshold": threshold, "precision": precision, "recall": recall,
            "f1": f1, "tp": tp, "fp": fp, "tn": tn, "fn": fn}


# ---------------------------------------------------------------------------
# Figures
# ---------------------------------------------------------------------------

def plot_confusion(cm, title, path) -> None:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    cmn = cm.astype(float) / np.maximum(cm.sum(axis=1, keepdims=True), 1)
    fig, ax = plt.subplots(figsize=(6, 5))
    im = ax.imshow(cmn, cmap="Blues", vmin=0, vmax=1)
    ax.set_xticks(range(len(CLASS_NAMES)), CLASS_NAMES, rotation=45, ha="right")
    ax.set_yticks(range(len(CLASS_NAMES)), CLASS_NAMES)
    ax.set_xlabel("predicted")
    ax.set_ylabel("actual")
    ax.set_title(title)
    for i in range(cm.shape[0]):
        for j in range(cm.shape[1]):
            ax.text(j, i, f"{cmn[i, j]:.2f}", ha="center", va="center",
                    color="white" if cmn[i, j] > 0.5 else "black", fontsize=8)
    fig.colorbar(im, ax=ax, fraction=0.046)
    plt.tight_layout()
    plt.savefig(path, dpi=150)
    plt.close(fig)


# ---------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--test-session", type=int, default=3,
                    help="session held out entirely as the test set")
    ap.add_argument("--seeds", type=int, default=5,
                    help="repeats, so reported differences are not noise")
    args = ap.parse_args()

    for d in (PROCESSED, REPORTS, FIGURES, MODELS):
        d.mkdir(parents=True, exist_ok=True)

    print("Loading raw captures...")
    df = clean(load_raw())
    print(f"  {len(df)} samples across {df['run'].nunique()} runs, "
          f"sessions {sorted(df['session'].unique())}")

    X, y, groups, sessions, feat_names = window_features(df, CHANNELS)
    print(f"  {len(X)} windows x {X.shape[1]} features")

    test_mask = sessions == args.test_session
    if not test_mask.any():
        raise SystemExit(
            f"Session {args.test_session} has no data. The held-out session must exist - "
            f"see docs/03-DATA-PROTOCOL.md section 2. Sessions found: {sorted(set(sessions))}")

    Xtr_raw, ytr, gtr = X[~test_mask], y[~test_mask], groups[~test_mask]
    Xte_raw, yte = X[test_mask], y[test_mask]
    print(f"  train {len(Xtr_raw)} windows (sessions "
          f"{sorted(set(sessions[~test_mask]))}), held-out test {len(Xte_raw)} "
          f"(session {args.test_session})")

    # Scaler fit on training data only. This line is the anti-leakage control.
    scaler = StandardScaler().fit(Xtr_raw)
    Xtr, Xte = scaler.transform(Xtr_raw), scaler.transform(Xte_raw)

    lines = ["# Offline results", "",
             f"Held-out test session: **{args.test_session}** · "
             f"train windows: **{len(Xtr)}** · test windows: **{len(Xte)}** · "
             f"seeds: **{args.seeds}**", "",
             "Split is session-wise, not random: at 10 Hz consecutive samples are near "
             "duplicates, so a random split would leak near-identical windows into the "
             "test set. Cross-validation is grouped by run for the same reason.", "",
             "## Model comparison", "",
             "| model | CV macro-F1 (mean±std) | test accuracy | test macro-F1 |",
             "|---|---|---|---|"]

    results: dict[str, dict] = {}
    for name in build_models(0):
        accs, f1s = [], []
        best_pred = None
        for seed in range(args.seeds):
            m = build_models(seed)[name]
            r = eval_model(m, Xtr, ytr, Xte, yte)
            accs.append(r["accuracy"])
            f1s.append(r["macro_f1"])
            if seed == 0:
                best_pred = r["pred"]
                if name.startswith("M0a"):
                    (REPORTS / "decision_tree.txt").write_text(
                        export_text(m, feature_names=feat_names), encoding="utf-8")

        cv_mean, cv_std = cross_validate(
            lambda s, n=name: build_models(s)[n], Xtr_raw, ytr, gtr, seed=0)

        results[name] = {"cv_mean": cv_mean, "cv_std": cv_std,
                         "acc": float(np.mean(accs)), "acc_std": float(np.std(accs)),
                         "f1": float(np.mean(f1s)), "f1_std": float(np.std(f1s))}
        lines.append(f"| {name} | {cv_mean:.3f} ± {cv_std:.3f} | "
                     f"{np.mean(accs):.3f} ± {np.std(accs):.3f} | "
                     f"{np.mean(f1s):.3f} ± {np.std(f1s):.3f} |")

        cm = confusion_matrix(yte, best_pred, labels=range(len(CLASS_NAMES)))
        slug = name.split()[0].lower()
        plot_confusion(cm, f"{name} - held-out session {args.test_session}",
                       FIGURES / f"confusion_{slug}.png")
        print(f"  {name:<26} CV {cv_mean:.3f}  test F1 {np.mean(f1s):.3f}")

    lines += ["", "Confusion matrices: `reports/figures/confusion_*.png`.",
              "Decision tree rules: `reports/decision_tree.txt` - check the split "
              "thresholds against the physical tolerances in the data protocol. When "
              "they agree, that is independent evidence the dataset is sane.", ""]

    # --- the CV / held-out gap is a result, not an embarrassment ---
    best = max(results, key=lambda k: results[k]["f1"])
    gap = results[best]["cv_mean"] - results[best]["f1"]
    lines += ["## Generalisation gap", "",
              f"Best model: **{best}**. CV macro-F1 {results[best]['cv_mean']:.3f} vs "
              f"held-out {results[best]['f1']:.3f} — a gap of **{gap:+.3f}**.", "",
              "This gap is what session-wise splitting exists to expose: it quantifies "
              "how much performance depends on conditions specific to the training "
              "sessions. A random split would have hidden it.", ""]

    # --- M2 novelty gate ---
    print("Training M2 autoencoder novelty gate...")
    ae = autoencoder_novelty(Xtr[ytr == 0], Xte, yte, seed=0)
    lines += ["## M2 - autoencoder novelty gate", "",
              "Trained on `optimal` windows only; flags anything it reconstructs badly. "
              "Threshold = 95th percentile of the training reconstruction error.", "",
              f"| metric | value |", "|---|---|",
              f"| threshold (MSE) | {ae['threshold']:.4f} |",
              f"| precision | {ae['precision']:.3f} |",
              f"| recall | {ae['recall']:.3f} |",
              f"| F1 | {ae['f1']:.3f} |",
              f"| TP / FP / TN / FN | {ae['tp']} / {ae['fp']} / {ae['tn']} / {ae['fn']} |", "",
              "On-device this gates the classifier: high reconstruction error shows "
              "`UNKNOWN SETUP` rather than a confident wrong class.", ""]
    print(f"  precision {ae['precision']:.3f}  recall {ae['recall']:.3f}")

    # --- ablation ---
    print("Running sensor ablation...")
    lines += ["## Sensor ablation", "",
              "Does each sensor earn its place? Same model (M1), same splits.", "",
              "| sensors | features | CV macro-F1 | test macro-F1 |", "|---|---|---|---|"]
    for gname, chans in SENSOR_GROUPS.items():
        Xa, ya, ga, sa, _ = window_features(df, chans)
        tm = sa == args.test_session
        sc = StandardScaler().fit(Xa[~tm])
        m = build_models(0)["M1  MLP (32-16)"]
        m.fit(sc.transform(Xa[~tm]), ya[~tm])
        f1 = f1_score(ya[tm], m.predict(sc.transform(Xa[tm])), average="macro")
        cvm, _ = cross_validate(lambda s: build_models(s)["M1  MLP (32-16)"],
                                Xa[~tm], ya[~tm], ga[~tm], seed=0)
        lines.append(f"| {gname} | {Xa.shape[1]} | {cvm:.3f} | {f1:.3f} |")
        print(f"  {gname:<24} test F1 {f1:.3f}")
    lines += ["", "If a sensor adds little, say so and propose the cheaper variant in "
              "future work. An honest negative result is still a result.", ""]

    # --- per-class detail for the best model ---
    m = build_models(0)[best]
    m.fit(Xtr, ytr)
    lines += [f"## Per-class detail - {best}", "", "```",
              classification_report(yte, m.predict(Xte), labels=range(len(CLASS_NAMES)),
                                    target_names=CLASS_NAMES, zero_division=0), "```", ""]

    (REPORTS / "offline_results.md").write_text("\n".join(lines), encoding="utf-8")

    # Persist artefacts. The scaler parameters get hard-coded into the firmware, so
    # they must be saved with the model that expects them.
    np.savez(MODELS / "scaler.npz", mean=scaler.mean_, scale=scaler.scale_,
             feature_names=np.array(feat_names))
    (MODELS / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")

    pd.DataFrame(Xtr_raw, columns=feat_names).assign(label=ytr).to_csv(
        PROCESSED / "train.csv", index=False)
    pd.DataFrame(Xte_raw, columns=feat_names).assign(label=yte).to_csv(
        PROCESSED / "test.csv", index=False)

    print(f"\nWritten: {REPORTS / 'offline_results.md'}")
    print(f"         {PROCESSED / 'train.csv'} and test.csv (upload these to Edge Impulse)")
    print(f"\nGate G3: best model {best}, held-out macro-F1 {results[best]['f1']:.3f}")


if __name__ == "__main__":
    main()
