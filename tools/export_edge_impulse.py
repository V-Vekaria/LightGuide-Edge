"""
LightGuide Edge - Edge Impulse dataset export
COM683 CW2 | Vishnu Vekariya | Ulster University

The Code submission must carry a link to a public Edge Impulse project
(docs/00-REQUIREMENTS-LOCKED.md section 2). This produces the files to upload, with the
train/test split preserved exactly as it is offline, so the EI result and the offline
result are comparable rather than merely adjacent.

    python tools/export_edge_impulse.py

Writes deliverables/edge_impulse/:

    training/<class>/<run>.json   sessions 1-2
    testing/<class>/<run>.json    session 3, the held-out session

WHY WHOLE RUNS AND NOT WINDOWS
------------------------------
Edge Impulse does its own windowing inside the impulse. Uploading pre-cut windows would
window them twice and quietly destroy the correspondence with the offline pipeline.
Uploading whole runs and setting EI's window length to 1250 ms with a 625 ms stride
reproduces the offline WINDOW=10 / STRIDE=5 at ~8 Hz.

WHY THE SPLIT IS SET BY DIRECTORY AND NOT BY EI'S SPLIT BUTTON
--------------------------------------------------------------
EI's automatic split is random. A random split over 8 Hz samples puts near-identical
neighbours on both sides of the boundary and reports memorisation as accuracy - the
same leak documented in tools/train_offline.py. Session 3 goes to testing/ and nothing
else does.
"""

from __future__ import annotations

import json
from pathlib import Path

import pandas as pd

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "data" / "raw"
OUT = ROOT / "deliverables" / "edge_impulse"

CLASS_NAMES = ["optimal", "too_close", "too_far", "underlit", "overlit"]
TEST_SESSION = 3
INTERVAL_MS = 125.0          # ~8 Hz, matching the logger


def main() -> None:
    files = sorted(RAW.glob("session*_*.csv"))
    if not files:
        raise SystemExit(f"No captures in {RAW}")

    # Clear stale exports file-by-file rather than removing the tree. The repo lives
    # inside a OneDrive folder, and OneDrive holds directory handles while it syncs -
    # shutil.rmtree() intermittently dies with WinError 5 on the directory rmdir even
    # though every file inside was deleted fine. Overwriting the files achieves the
    # same thing and cannot fail that way.
    for stale in OUT.rglob("*.json"):
        stale.unlink()
    counts = {"training": 0, "testing": 0}

    for f in files:
        df = pd.read_csv(f, comment="#")
        # A failed echo is -1, a value the sensor never actually measured. It was
        # dropped offline, so it is dropped here too - otherwise EI trains on a
        # sentinel the offline model never saw.
        df = df[df["dist_mm"] >= 0]
        if df.empty:
            continue

        session = int(df["session"].iloc[0])
        label = CLASS_NAMES[int(df["label"].iloc[0])]
        split = "testing" if session == TEST_SESSION else "training"

        dest = OUT / split / label
        dest.mkdir(parents=True, exist_ok=True)

        # EI's data-acquisition JSON format. Deviations from the saved reference are
        # the sensor values, exactly as in the offline feature pipeline - absolutes
        # would tie the model to one physical setup.
        payload = {
            "protected": {"ver": "v1", "alg": "none"},
            "signature": "0" * 64,      # EI accepts an unsigned upload over the API
            "payload": {
                "device_name": "LightGuide-Edge-Nano33BLE",
                "device_type": "ARDUINO_NANO_33_BLE_SENSE",
                "interval_ms": INTERVAL_MS,
                "sensors": [
                    {"name": "d_dist_mm", "units": "mm"},
                    {"name": "d_ldr", "units": "counts"},
                ],
                "values": [[float(a), float(b)] for a, b in
                           zip(df["d_dist_mm"], df["d_ldr"])],
            },
        }
        (dest / f"{f.stem}.json").write_text(json.dumps(payload), encoding="utf-8")
        counts[split] += 1

    print(f"Training runs: {counts['training']}  (sessions 1-2)")
    print(f"Testing  runs: {counts['testing']}  (session {TEST_SESSION}, held out)")
    print(f"\nWritten to {OUT}")
    print("\nNext: docs/11-EDGE-IMPULSE-STEPS.md")


if __name__ == "__main__":
    main()
