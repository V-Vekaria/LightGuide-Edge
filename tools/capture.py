"""
LightGuide Edge - labelled data capture
COM683 CW2 | Vishnu Vekariya | Ulster University

Drives firmware/02_data_logger over serial and writes one CSV per run to data/raw/.
Implements the host side of docs/03-DATA-PROTOCOL.md: warm-up discard, range gating
report, and file naming that keeps session and label in the filename so a mislabelled
run is obvious on disk.

Usage:
    python tools/capture.py --port COM4 --session 1 --label 2 --seconds 10
    python tools/capture.py --port COM4 --session 1 --interactive
    python tools/capture.py --list

Class labels: 0 optimal  1 too_close  2 too_far  3 underlit  4 overlit  5 tilt_off
"""

from __future__ import annotations

import argparse
import csv
import sys
import time
from datetime import datetime
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is not installed. Run: pip install -r tools/requirements.txt")

CLASS_NAMES = ["optimal", "too_close", "too_far", "underlit", "overlit", "tilt_off"]
BAUD = 115200
WARMUP_S = 2.0  # discarded: the LDR settles and early pings are unreliable
RAW_DIR = Path(__file__).resolve().parent.parent / "data" / "raw"


def list_serial_ports() -> None:
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found.")
        return
    print("Available ports:")
    for p in ports:
        print(f"  {p.device:<8} {p.description}")


def open_port(port: str) -> serial.Serial:
    ser = serial.Serial(port, BAUD, timeout=2)
    time.sleep(2.0)  # the Nano resets when the port opens
    ser.reset_input_buffer()
    return ser


def send(ser: serial.Serial, cmd: str) -> None:
    ser.write((cmd + "\n").encode())
    ser.flush()
    time.sleep(0.1)


def capture_run(ser: serial.Serial, session: int, label: int, seconds: float) -> Path | None:
    """Capture one labelled run. Returns the path written, or None if nothing usable."""
    name = CLASS_NAMES[label]
    print(f"\n=== session {session} | label {label} ({name}) | {seconds:.0f}s ===")
    print("Stage the physical condition now. Capture starts in 3s...")
    for i in (3, 2, 1):
        print(f"  {i}...", end="", flush=True)
        time.sleep(1)
    print()

    send(ser, f"S{session}")
    send(ser, f"L{label}")
    ser.reset_input_buffer()
    send(ser, "G")

    rows: list[list[str]] = []
    header: list[str] | None = None
    dropouts = 0
    t_start = time.time()
    warmup_until = t_start + WARMUP_S

    print(f"  warm-up ({WARMUP_S:.0f}s, discarded)...", end="", flush=True)
    warmup_done = False

    while time.time() - t_start < seconds + WARMUP_S:
        line = ser.readline().decode("utf-8", "replace").strip()
        if not line:
            continue
        if line.startswith("#"):
            continue
        if line.startswith("t_ms,"):
            header = line.split(",")
            continue
        if time.time() < warmup_until:
            continue
        if not warmup_done:
            print(" done. RECORDING", end="", flush=True)
            warmup_done = True

        parts = line.split(",")
        if header is None or len(parts) != len(header):
            continue  # partial line, usually the first one after the header
        rows.append(parts)
        if parts[1].startswith("-"):
            dropouts += 1
        if len(rows) % 20 == 0:
            print(".", end="", flush=True)

    send(ser, "X")
    print()

    if not rows or header is None:
        print("  !! no rows captured - check the wiring and try 01_sensor_check")
        return None

    drop_pct = 100.0 * dropouts / len(rows)
    print(f"  {len(rows)} rows, {dropouts} ultrasonic dropouts ({drop_pct:.1f}%)")
    if drop_pct > 10:
        print("  !! dropout rate above 10% - see risk R-08. Re-aim the sensor or")
        print("     note the surface; a soft modifier absorbs the pulse.")

    RAW_DIR.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    path = RAW_DIR / f"session{session}_{label}_{name}_{stamp}.csv"
    with path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(header)
        w.writerows(rows)
    print(f"  -> {path.relative_to(RAW_DIR.parent.parent)}")
    return path


def interactive(ser: serial.Serial, session: int, seconds: float) -> None:
    print("\nInteractive capture. Enter a label 0-5 to record a run, or q to quit.")
    for i, n in enumerate(CLASS_NAMES):
        print(f"  {i} {n}")
    while True:
        raw = input("\nlabel> ").strip().lower()
        if raw in ("q", "quit", "exit"):
            break
        if not raw.isdigit() or not 0 <= int(raw) <= 5:
            print("  enter 0-5, or q")
            continue
        capture_run(ser, session, int(raw), seconds)


def main() -> None:
    ap = argparse.ArgumentParser(description="LightGuide Edge labelled capture")
    ap.add_argument("--port", help="serial port, e.g. COM4")
    ap.add_argument("--session", type=int, default=1, help="session id (see data protocol)")
    ap.add_argument("--label", type=int, choices=range(6), help="class label 0-5")
    ap.add_argument("--seconds", type=float, default=10.0, help="capture length after warm-up")
    ap.add_argument("--interactive", action="store_true", help="prompt for labels in a loop")
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    args = ap.parse_args()

    if args.list:
        list_serial_ports()
        return
    if not args.port:
        list_serial_ports()
        sys.exit("\n--port is required (or use --list)")

    ser = open_port(args.port)
    try:
        if args.interactive:
            interactive(ser, args.session, args.seconds)
        elif args.label is not None:
            capture_run(ser, args.session, args.label, args.seconds)
        else:
            sys.exit("give --label, or use --interactive")
    finally:
        try:
            send(ser, "X")
        finally:
            ser.close()


if __name__ == "__main__":
    main()
