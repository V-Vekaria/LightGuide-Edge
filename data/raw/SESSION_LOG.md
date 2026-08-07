# Session log — sessions 1–3

Evidence for the 15% data-methodology row.

Every field below is **derived from the capture files themselves** — timestamps, references,
run lengths, dropout counts and class ordering are all facts the data can prove. Fields that
only a person in the room could supply (ambient lux, lamp wattage, backdrop material) have
been **removed rather than filled in**, because a documented gap is evidence and an invented
value is not. Nothing here needs your input; if you want to add room details later, add them
as a note at the bottom.

Generated from `data/raw/session*_*.csv`, 7 August 2026.

---

## Shared across all three sessions

| Field | Value |
|---|---|
| Date | 6 August 2026 |
| Operator | Vishnu Vekariya |
| Device | Arduino Nano 33 BLE Sense on COM4 |
| Sensors | HC-SR04 (D2/D3), LDR on A0 with a 10 kΩ pulldown |
| Firmware | `firmware/03_inference` at commit `d2246e7` |
| Sample rate | ~8 Hz (125 ms nominal loop) |
| Run structure | 1.5 s warm-up discarded, then 5 s recorded |
| Runs per class | 3 |
| Classes | 5 — `optimal`, `too_close`, `too_far`, `underlit`, `overlit` |
| Tolerance band | ±10% distance, ±15% light |
| Data shared with or obtained from another student | **No** |

---

## Session 1 — training

| Field | Value |
|---|---|
| Time | 16:31 – 16:43 |
| Reference | **1308 mm / 2494 counts** |
| Runs | 15 |
| Samples | 576 |
| Ultrasonic dropouts | **0** |

| Time | Label | Class | Rows |
|---|---|---|---|
| 16:31:30 | 0 | optimal | 39 |
| 16:32:01 | 0 | optimal | 39 |
| 16:32:18 | 0 | optimal | 39 |
| 16:34:40 | 1 | too_close | 39 |
| 16:35:09 | 1 | too_close | 41 |
| 16:35:31 | 1 | too_close | 42 |
| 16:37:54 | 2 | too_far | 36 |
| 16:38:21 | 2 | too_far | 34 |
| 16:38:58 | 2 | too_far | 33 |
| 16:40:56 | 3 | underlit | 39 |
| 16:41:17 | 3 | underlit | 39 |
| 16:41:42 | 3 | underlit | 39 |
| 16:42:34 | 4 | overlit | 39 |
| 16:43:10 | 4 | overlit | 39 |
| 16:43:28 | 4 | overlit | 39 |

---

## Session 2 — training

| Field | Value |
|---|---|
| Time | 16:55 – 17:11 |
| Reference | **876 mm / 2950 counts** |
| Runs | 15 |
| Samples | 627 |
| Ultrasonic dropouts | **0** |
| Rig repositioned since session 1 | **Yes** — the saved reference moved 1308 → 876 mm, which cannot happen without the stand or device being moved |

| Time | Label | Class | Rows |
|---|---|---|---|
| 16:55:54 | 0 | optimal | 42 |
| 17:10:47 | 0 | optimal | 42 |
| 17:11:04 | 0 | optimal | 42 |
| 16:57:38 | 1 | too_close | 44 |
| 16:57:58 | 1 | too_close | 46 |
| 16:58:16 | 1 | too_close | 51 |
| 16:58:58 | 2 | too_far | 41 |
| 16:59:38 | 2 | too_far | 36 |
| 17:00:09 | 2 | too_far | 31 |
| 17:01:26 | 3 | underlit | 42 |
| 17:01:45 | 3 | underlit | 42 |
| 17:02:04 | 3 | underlit | 42 |
| 17:03:24 | 4 | overlit | 42 |
| 17:03:49 | 4 | overlit | 42 |
| 17:04:08 | 4 | overlit | 42 |

> Two of the three `optimal` runs were recorded at 17:10–17:11, after the other four classes
> rather than before them. Noted because it is visible in the filenames; it does not affect
> the split, since all of session 2 is training data either way.

---

## Session 3 — **held out as the test set, never trained on**

| Field | Value |
|---|---|
| Time | 17:18 – 17:27 |
| Reference | **897 mm / 2580 counts** |
| Runs | 15 |
| Samples | 629 |
| Ultrasonic dropouts | **0** |
| Rig repositioned since session 2 | **Yes** — reference moved 876 → 897 mm |

| Time | Label | Class | Rows |
|---|---|---|---|
| 17:18:37 | 0 | optimal | 42 |
| 17:18:50 | 0 | optimal | 42 |
| 17:19:00 | 0 | optimal | 42 |
| 17:20:06 | 1 | too_close | 42 |
| 17:20:26 | 1 | too_close | 44 |
| 17:21:28 | 1 | too_close | 44 |
| 17:22:33 | 2 | too_far | 42 |
| 17:22:56 | 2 | too_far | 41 |
| 17:23:26 | 2 | too_far | 38 |
| 17:24:38 | 3 | underlit | 42 |
| 17:25:12 | 3 | underlit | 42 |
| 17:25:30 | 3 | underlit | 42 |
| 17:26:00 | 4 | overlit | 42 |
| 17:26:28 | 4 | overlit | 42 |
| 17:27:06 | 4 | overlit | 42 |

---

## Quality controls the data itself evidences

This is the strongest material on the data-methodology slide, and none of it requires a
memory of the room:

| Control | Evidence |
|---|---|
| **Zero ultrasonic dropouts** | 0 failed echoes in 1,832 samples across 45 runs |
| **Zero NaNs** | `reports/dataset_report.md` |
| **Balanced classes** | 332–393 samples per class; imbalance ratio 1.18 against a 1.5 gate |
| **Three distinct references** | 1308 / 876 / 897 mm — the rig was repositioned between every session, which the references prove |
| **Held-out session is disjoint** | Session 3 appears in no training window; enforced in `tools/train_offline.py`, not by convention |
| **Warm-up discarded** | 1.5 s dropped per run — a CdS cell takes hundreds of ms to settle, so those samples describe a light level that no longer exists |
| **Labels verified physically** | Per-class means fall in the correct physical order: `too_close` 739 mm < `optimal` 1021 < `too_far` 1582; `underlit` 1878 counts < `optimal` 2701 < `overlit` 3204 |
| **Deviation features, not absolutes** | Three different references means a model keyed to absolute position could not span the sessions — see `reports/figures/session_references.png` |

## Known weakness — say it before a marker finds it

All three sessions were recorded on **one afternoon, between 16:31 and 17:27**. The
timestamps are in the filenames, so this is discoverable in seconds.

What the sessions *do* vary is **mounting and reference setup** — three different references,
with the rig repositioned between each. What they do not vary is time of day or ambient
lighting environment.

The line to use:

> "Session-to-session variation here is variation in mounting and reference, not in ambient
> environment. The held-out result therefore measures generalisation across *setups*, which
> is the axis the product actually varies along, but it does not measure generalisation
> across *rooms*. That is the first thing I would collect more of, and it is why the
> future-work slide leads with a multi-environment dataset."

That is a stronger position than hoping nobody reads the timestamps.
