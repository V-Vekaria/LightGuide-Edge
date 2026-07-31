# Session sheet — template

Copy this block for **every** collection session. It is not paperwork for its own sake:
it is the evidence behind the 15% data-methodology mark, and it is what lets you explain
an anomaly three days later when you have forgotten the room you were in.

Fill it in **before** the session, not from memory afterwards.

---

## Session `<id>`

| Field | Value |
|---|---|
| Date / time | |
| Room / location | |
| Ambient light source(s) | e.g. overhead LED panel, daylight through north window |
| Ambient lux (device off) | |
| Studio light: type / power | e.g. 60 W COB LED, softbox modifier |
| Backdrop / reflecting surface | e.g. white wall, grey paper, black cloth |
| Device mounting | e.g. clamped to stand pole 1.1 m from floor |
| **Device re-mounted since last session?** | yes / no — should be **yes** (protocol §2) |
| Reference `dist_mm` | |
| Reference `ldr_raw` | |
| Reference `pitch` / `roll` | |
| Tolerance band used | ±10% dist, ±15% light, ±5° tilt |
| Photo taken of the rig? | yes / no — should be **yes** |
| Operator | Vishnu Vekariya |
| Data shared with / from another student? | **no** — if this ever becomes yes, it must be declared in the presentation |

### Runs

| # | Label | Class | Duration | Notes (variation used, anything odd) |
|---|---|---|---|---|
| 1 | | | | |
| 2 | | | | |
| 3 | | | | |

### Confounded runs collected (protocol §4 — mandatory)

| Label | Confound applied | Why it matters |
|---|---|---|
| `too_far` | light turned **up** | reads bright but is mispositioned |
| `too_close` | light **dimmed** | reads normal but is mispositioned |
| | | |

### Anomalies / anything unusual

> e.g. "ultrasonic dropped out badly against the softbox — ~18% dropouts on runs 7–9"
> Write these down. They become the failure-analysis slide.

### Post-session checklist

- [ ] Files present in `data/raw/` with the right session id in the filename
- [ ] `python tools/dataset_report.py` run and reviewed
- [ ] 10 random samples per class eyeballed against the class they claim (protocol §5 control 8)
- [ ] `data/` backed up
- [ ] Photos saved to `reports/figures/rig/`
