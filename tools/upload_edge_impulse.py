"""
LightGuide Edge - upload the exported dataset to Edge Impulse
COM683 CW2 | Vishnu Vekariya B00969091 | Ulster University

    py -3 tools/upload_edge_impulse.py

Reads the project's ingestion API key from `.ei_key` in the repo root - a file
that is gitignored and never printed, so the key stays out of the transcript,
out of git and out of the submission zip.

Why the API rather than the Studio's upload button: the browser upload dialog is
a native file picker, and 45 files through it is 45 chances to mis-click. The
ingestion API takes the whole set in one pass and reports exactly what landed.

The files in deliverables/edge_impulse/ are already in Edge Impulse's Data
Acquisition JSON format - the same format the Studio itself writes - so no
conversion happens here. The training/testing split is the honest one: sessions
1 and 2 train, session 3 is held out, matching tools/train_offline.py exactly.
"""

from __future__ import annotations

import json
import sys
import urllib.error
import urllib.request
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXPORT = ROOT / "deliverables" / "edge_impulse"
KEY_FILE = ROOT / ".ei_key"
INGEST = "https://ingestion.edgeimpulse.com/api"


def read_key() -> str:
    if not KEY_FILE.exists():
        sys.exit(
            f"No API key found.\n\n"
            f"  1. Open https://studio.edgeimpulse.com/studio/1082649/keys\n"
            f"  2. Click the copy icon next to the API key\n"
            f"  3. Paste it into a new file at:  {KEY_FILE}\n\n"
            f"The file is gitignored and its contents are never printed."
        )
    key = KEY_FILE.read_text(encoding="utf-8").strip()
    if not key.startswith("ei_"):
        sys.exit("That does not look like an Edge Impulse API key (should start with 'ei_').")
    return key


def post_file(path: Path, label: str, category: str, key: str) -> tuple[bool, str]:
    """One multipart POST. Written by hand to keep this dependency-free."""
    boundary = uuid.uuid4().hex
    body = b"".join([
        f"--{boundary}\r\n".encode(),
        f'Content-Disposition: form-data; name="data"; filename="{path.name}"\r\n'.encode(),
        b"Content-Type: application/json\r\n\r\n",
        path.read_bytes(),
        f"\r\n--{boundary}--\r\n".encode(),
    ])

    req = urllib.request.Request(
        f"{INGEST}/{category}/files",
        data=body,
        headers={
            "Content-Type": f"multipart/form-data; boundary={boundary}",
            "x-api-key": key,
            "x-label": label,
            "x-disallow-duplicates": "1",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return r.status in (200, 201), f"HTTP {r.status}"
    except urllib.error.HTTPError as e:
        return False, f"HTTP {e.code}: {e.read().decode('utf-8', 'replace')[:200]}"
    except Exception as e:                       # noqa: BLE001
        return False, str(e)


def main() -> None:
    key = read_key()
    if not EXPORT.exists():
        sys.exit(f"No export found at {EXPORT}. Run tools/export_edge_impulse.py first.")

    ok = fail = 0
    failures: list[str] = []

    for category in ("training", "testing"):
        cat_dir = EXPORT / category
        if not cat_dir.exists():
            continue
        print(f"\n{category}:")
        for class_dir in sorted(cat_dir.iterdir()):
            if not class_dir.is_dir():
                continue
            files = sorted(class_dir.glob("*.json"))
            good = 0
            for f in files:
                success, msg = post_file(f, class_dir.name, category, key)
                if success:
                    good += 1
                    ok += 1
                else:
                    fail += 1
                    failures.append(f"{category}/{class_dir.name}/{f.name}: {msg}")
            print(f"  {class_dir.name:<12} {good}/{len(files)} uploaded")

    print(f"\nuploaded {ok}, failed {fail}")
    if failures:
        print("\nfailures:")
        for f in failures[:10]:
            print(f"  {f}")
    else:
        print("\nAll files landed. Check them at:")
        print("  https://studio.edgeimpulse.com/studio/1082649/acquisition/training")


if __name__ == "__main__":
    main()
