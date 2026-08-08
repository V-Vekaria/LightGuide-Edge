"""
LightGuide Edge - combine online trial captures into one graded set
COM683 CW2 | Vishnu Vekariya | Ulster University

Why this exists
---------------
The 8 August online trial was collected in three sittings, because an automated
staging check (online_trial.check_staging) caught two operator errors that would
otherwise have been reported as a model result:

  1. From trial 1 onward the stand stopped being returned to the reference mark
     for the lamp-only classes, so 14 `underlit`/`overlit` runs were recorded
     ~30 cm out of position. Those classes ended up separated on both channels
     at once, which is trivially easy and inflated the score to a meaningless
     1.000.
  2. On the re-run, three `overlit` runs were staged one dimmer step up (~+105
     counts). That is *inside* the device's +-5% correct band, so those runs did
     not stage `overlit` at all. Two steps (~+245) clears it.

Each affected class was re-collected against the same saved reference (997 mm /
2505 counts) - the reference was deliberately never re-saved, so every run in
the combined set is measured against one baseline and the sittings are directly
comparable.

Nothing here re-labels or repairs a run. It selects whole classes from the
capture in which they were staged correctly, and records which file each run
came from so the provenance survives into the report.

Usage
-----
    python tools/merge_online.py \
        data/online/online_trials_20260808_113613.csv:0,1,2 \
        data/online/online_trials_20260808_121055.csv:3 \
        data/online/online_trials_20260808_121855.csv:4
"""

from __future__ import annotations

import sys
from datetime import datetime
from pathlib import Path

import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))
from online_trial import CLASS_NAMES, check_staging  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
ONLINE = ROOT / "data" / "online"


def parse_spec(spec: str) -> tuple[Path, list[int]]:
    """`path.csv:0,1,2` -> (path, [0, 1, 2]). Windows drive letters survive the
    rsplit because we only ever split on the last colon."""
    path_s, _, classes_s = spec.rpartition(":")
    if not path_s:
        sys.exit(f"expected PATH:CLASSES, got {spec!r}")
    try:
        classes = [int(c) for c in classes_s.split(",") if c.strip()]
    except ValueError:
        sys.exit(f"class list must be integers, got {classes_s!r}")
    path = Path(path_s)
    if not path.is_absolute():
        path = ROOT / path
    if not path.exists():
        sys.exit(f"no such file: {path}")
    return path, classes


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit(__doc__)

    frames: list[pd.DataFrame] = []
    seen: dict[int, str] = {}

    for spec in sys.argv[1:]:
        path, classes = parse_spec(spec)
        df = pd.read_csv(path, dtype=str)
        for c in classes:
            if c in seen:
                sys.exit(f"class {c} ({CLASS_NAMES[c]}) taken from two files: "
                         f"{seen[c]} and {path.name}")
            part = df[df["staged"].astype(int) == c].copy()
            if part.empty:
                sys.exit(f"{path.name} holds no runs for class {c}")
            part["source"] = path.name
            frames.append(part)
            seen[c] = path.name
            runs = part.groupby(["trial", "staged"]).ngroups
            print(f"  {CLASS_NAMES[c]:<10} {runs:>2} runs, {len(part):>4} rows  "
                  f"<- {path.name}")

    missing = [CLASS_NAMES[c] for c in range(len(CLASS_NAMES)) if c not in seen]
    if missing:
        sys.exit(f"combined set is missing classes: {', '.join(missing)}")

    out_df = pd.concat(frames, ignore_index=True)

    # Re-run the staging check over the combined set. A merge that quietly keeps
    # a bad run is worse than no merge, because the provenance makes it look
    # audited.
    flagged = 0
    for (t, s), g in out_df.groupby(["trial", "staged"]):
        for w in check_staging(g.to_dict("records"), int(s)):
            flagged += 1
            print(f"  !! trial {t} {CLASS_NAMES[int(s)]}: {w}")

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out = ONLINE / f"online_trials_{stamp}_combined.csv"
    out_df.to_csv(out, index=False)

    runs = out_df.groupby(["trial", "staged"]).ngroups
    print(f"\n{runs} runs, {len(out_df)} rows, {flagged} staging warnings")
    print(f"wrote {out.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
