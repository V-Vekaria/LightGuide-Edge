"""
LightGuide Edge - labelled capture listener for firmware/03_inference
COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University

Sits and listens. Every time you press the button at the rig it writes one run to
data/raw/ and prints a running tally.

    py -3 tools/collect.py --session 1

Why this rather than tools/button_capture.py: that one drives 02b_button_logger,
which records raw distance and raw LDR. Classes here are defined relative to a
saved reference, so the features that matter are the deviations - and the only
sketch that knows the reference is the product itself. Collecting with the
deployed pipeline also means the training features are produced by exactly the
code that produces inference features, so there is no train/serve skew.

While it runs you can type commands; they are forwarded to the board:
    L2      switch to class 2 (too_far)
    S3      switch to session 3
    ?       print status
    q       quit

Class labels: 0 optimal  1 too_close  2 too_far  3 underlit  4 overlit

Ctrl-C also stops it. Files land as:
    data/raw/session<N>_<label>_<name>_<timestamp>.csv
"""

from __future__ import annotations

import argparse
import re
import sys
import threading
from datetime import datetime
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is not installed. Run: py -3 -m pip install -r tools/requirements.txt")

RAW_DIR = Path(__file__).resolve().parent.parent / "data" / "raw"
BAUD = 115200

START_RE = re.compile(
    r"#\s*RUN_START\s+session=(\d+)\s+label=(\d+)\s+name=(\S+)\s+"
    r"ref_mm=(-?[\d.]+)\s+ref_ldr=(-?\d+)"
)
END_RE = re.compile(r"#\s*RUN_END\s+rows=(\d+)\s+dropouts=(\d+)")

_stop = threading.Event()


def forward_stdin(ser: serial.Serial) -> None:
    """Send typed commands to the board so the operator never needs a second window."""
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        if line.lower() in ("q", "quit", "exit"):
            _stop.set()
            return
        ser.write((line + "\n").encode())


def main() -> None:
    ap = argparse.ArgumentParser(description="Record labelled runs from 03_inference.")
    ap.add_argument("--port", default="COM4")
    ap.add_argument("--session", type=int, default=1)
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    args = ap.parse_args()

    if args.list:
        for p in list_ports.comports():
            print(f"{p.device}  {p.description}")
        return

    RAW_DIR.mkdir(parents=True, exist_ok=True)

    with serial.Serial(args.port, BAUD, timeout=1) as ser:
        ser.dtr = True          # the Nano's native USB needs DTR or it never talks
        ser.rts = True
        ser.reset_input_buffer()
        ser.write(f"S{args.session}\n".encode())

        print(f"Listening on {args.port}, session {args.session}.")
        print("Press the button at the rig to record a run. Type L0-L4 to change class, q to quit.\n")

        threading.Thread(target=forward_stdin, args=(ser,), daemon=True).start()

        fh = None
        tally: dict[str, int] = {}
        meta = None

        try:
            while not _stop.is_set():
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue

                m = START_RE.match(line)
                if m:
                    session, label, name, ref_mm, ref_ldr = m.groups()
                    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    path = RAW_DIR / f"session{session}_{label}_{name}_{stamp}.csv"
                    fh = path.open("w", newline="", encoding="utf-8")
                    # The reference is what every deviation in this file is measured
                    # against, so it travels with the data rather than in a notebook.
                    fh.write(f"# session={session} label={label} name={name} "
                             f"ref_mm={ref_mm} ref_ldr={ref_ldr}\n")
                    meta = (name, path)
                    print(f"  recording {name:<10} -> {path.name}")
                    continue

                m = END_RE.match(line)
                if m and fh:
                    rows, dropouts = int(m.group(1)), int(m.group(2))
                    fh.close()
                    fh = None
                    name, path = meta
                    tally[name] = tally.get(name, 0) + rows
                    flag = "  <-- DROPOUTS" if dropouts else ""
                    print(f"  saved     {name:<10} {rows} rows, {dropouts} dropouts{flag}")
                    print("  totals: " + ", ".join(f"{k}={v}" for k, v in sorted(tally.items())))
                    continue

                if line.startswith("#"):
                    print(line)
                    continue

                if fh:
                    fh.write(line + "\n")

        except KeyboardInterrupt:
            pass
        finally:
            if fh:
                fh.close()

        print("\nFinal totals (samples per class):")
        for k, v in sorted(tally.items()):
            print(f"  {k:<10} {v}")
        if not tally:
            print("  nothing recorded")


if __name__ == "__main__":
    main()
