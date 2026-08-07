"""
LightGuide Edge - record the real on-device footprint
COM683 CW2 | Vishnu Vekariya | Ulster University

    py -3 tools/record_footprint.py

Compiles firmware/03_inference and stores what the linker actually reports into
reports/footprint.json, which the online-evaluation report and the submission README
both read.

WHY THIS EXISTS AS A SCRIPT
---------------------------
`docs/04-ML-PLAN.md` section 5 says to read the footprint "from the build output, not
the estimate". That was being satisfied by hand-copying two numbers into two Python
files - which was already wrong once: adding storage.h moved flash from 117,696 to
120,896 bytes and nothing downstream noticed.

A number that appears on an assessed slide needs one source. This is it. Anything that
quotes the footprint reads this file, so changing the firmware and forgetting to update
the deck is no longer possible in the quiet direction.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKETCH = ROOT / "firmware" / "03_inference"
OUT = ROOT / "reports" / "footprint.json"

FQBN = "arduino:mbed_nano:nano33ble"


def main() -> None:
    print(f"Compiling {SKETCH.name} for {FQBN}...")
    proc = subprocess.run(
        ["arduino-cli", "compile", "--fqbn", FQBN, str(SKETCH)],
        capture_output=True, text=True)

    out = proc.stdout + proc.stderr
    if proc.returncode != 0:
        print(out.strip()[-2000:])
        sys.exit(f"\nBuild failed (exit {proc.returncode}). Footprint NOT updated - the "
                 "previous value in reports/footprint.json is left alone rather than "
                 "replaced with a guess.")

    # "Sketch uses 120896 bytes (12%) of program storage space. Maximum is 983040 bytes."
    flash = re.search(r"Sketch uses (\d+) bytes.*?Maximum is (\d+) bytes", out, re.S)
    # "Global variables use 46696 bytes (17%) of dynamic memory, leaving 215448 ...
    #  Maximum is 262144 bytes."
    ram = re.search(r"Global variables use (\d+) bytes.*?Maximum is (\d+) bytes", out, re.S)

    if not (flash and ram):
        print(out.strip()[-2000:])
        sys.exit("\nBuild succeeded but the footprint lines could not be parsed. "
                 "arduino-cli may have changed its output format - fix the regexes "
                 "above rather than typing the numbers in by hand.")

    flash_used, flash_max = int(flash.group(1)), int(flash.group(2))
    ram_used, ram_max = int(ram.group(1)), int(ram.group(2))

    data = {
        "sketch": SKETCH.name,
        "fqbn": FQBN,
        "recorded": datetime.now().isoformat(timespec="seconds"),
        "flash_bytes": flash_used,
        "flash_max_bytes": flash_max,
        "flash_pct": round(100.0 * flash_used / flash_max, 1),
        "ram_bytes": ram_used,
        "ram_max_bytes": ram_max,
        "ram_pct": round(100.0 * ram_used / ram_max, 1),
    }

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(data, indent=2), encoding="utf-8")

    print(f"\n  flash  {flash_used:,} of {flash_max:,} bytes  ({data['flash_pct']}%)")
    print(f"  RAM    {ram_used:,} of {ram_max:,} bytes  ({data['ram_pct']}%)")
    print(f"\nWritten to {OUT}")
    print("Re-run this after ANY firmware change, before rebuilding the deck or the zips.")


if __name__ == "__main__":
    main()
