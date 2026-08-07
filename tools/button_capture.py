"""
LightGuide Edge - button-triggered capture listener
COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University

Companion to firmware/02b_button_logger. Sits and listens; every time you press
the button at the stand it writes one run to data/raw/ and prints a live tally.

    python tools/button_capture.py --port COM4 --session 1

Why this instead of tools/capture.py: the operator stays at the rig rather than
at the laptop, and nothing is recorded while walking around or adjusting the
light. Every saved file is a deliberately staged, settled condition - which is
the quality control the data-collection rubric row is actually asking about.

Ctrl-C to stop. Files land as:
    data/raw/session<N>_<label>_<name>_<timestamp>.csv
"""

from __future__ import annotations

import argparse
import re
import sys
import time
from datetime import datetime
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial not installed. Run: pip install -r tools/requirements.txt")

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "data" / "raw"
CLASS_NAMES = ["optimal", "too_close", "too_far", "underlit", "overlit"]
TARGET_PER_CLASS = 9      # 9 runs x 30 samples = 270 samples, clears the 250 target


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="serial port, e.g. COM4")
    ap.add_argument("--session", type=int, default=1)
    ap.add_argument("--target", type=int, default=TARGET_PER_CLASS,
                    help="runs per class to aim for in this session")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    if args.list:
        for p in list_ports.comports():
            print(f"  {p.device:<8} {p.description}")
        return
    if not args.port:
        sys.exit("--port required (try --list)")

    RAW.mkdir(parents=True, exist_ok=True)
    ser = serial.Serial(args.port, 115200, timeout=2)
    time.sleep(2.0)
    ser.reset_input_buffer()
    ser.write(f"S{args.session}\n".encode())
    ser.flush()

    tally = {i: 0 for i in range(6)}
    # count anything already collected for this session so a resumed session
    # continues the tally rather than restarting it
    for f in RAW.glob(f"session{args.session}_*.csv"):
        m = re.match(rf"session{args.session}_(\d)_", f.name)
        if m:
            tally[int(m.group(1))] += 1

    print(f"\nListening on {args.port}, session {args.session}")
    print("At the rig:  TAP = record a run     HOLD = next class")
    print("Ctrl-C to stop.\n")
    if any(tally.values()):
        print("resuming - runs already on disk:",
              ", ".join(f"{CLASS_NAMES[i]}={n}" for i, n in tally.items() if n))
        print()

    header: list[str] | None = None
    rows: list[str] = []
    meta: dict | None = None

    try:
        while True:
            line = ser.readline().decode("utf-8", "replace").strip()
            if not line:
                continue

            if line.startswith("# RUN_START"):
                m = re.search(r"label=(\d+) name=(\S+) session=(\d+)", line)
                if m:
                    meta = {"label": int(m.group(1)), "name": m.group(2),
                            "session": int(m.group(3))}
                    rows, header = [], None
                    print(f"  ● recording  {meta['name']} ...", end="", flush=True)
                continue

            if line.startswith("# RUN_END"):
                if meta is None or header is None or not rows:
                    print("  (empty run, discarded)")
                    meta = None
                    continue
                m = re.search(r"rows=(\d+) dropouts=(\d+)", line)
                drops = int(m.group(2)) if m else 0
                stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                path = (RAW / f"session{meta['session']}_{meta['label']}"
                              f"_{meta['name']}_{stamp}.csv")
                path.write_text("\n".join([",".join(header)] + rows) + "\n",
                                encoding="utf-8")
                tally[meta["label"]] += 1

                pct = 100.0 * drops / max(len(rows), 1)
                warn = "  !! high dropout" if pct > 10 else ""
                print(f" saved {len(rows)} samples, {drops} dropouts{warn}")
                done = tally[meta["label"]]
                bar = "#" * done + "." * max(0, args.target - done)
                print(f"    {meta['name']:<10} [{bar}] {done}/{args.target}")
                total = sum(tally.values())
                if total % 6 == 0:
                    print("    ---- tally:",
                          "  ".join(f"{CLASS_NAMES[i]}={tally[i]}" for i in range(6)))
                meta = None
                continue

            if line.startswith("#"):
                continue
            if line.startswith("t_ms,"):
                header = line.split(",")
                continue
            if meta is not None and header is not None:
                p = line.split(",")
                if len(p) == len(header):
                    rows.append(line)

    except KeyboardInterrupt:
        print("\n\nsession summary")
        print("-" * 46)
        for i in range(6):
            n = tally[i]
            flag = "OK " if n >= args.target else "-- "
            print(f"  {flag}{CLASS_NAMES[i]:<12} {n} runs  ({n*30} samples)")
        print("-" * 46)
        print(f"  total {sum(tally.values())} runs, ~{sum(tally.values())*30} samples")
        print("\nNext: python tools/dataset_report.py")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
