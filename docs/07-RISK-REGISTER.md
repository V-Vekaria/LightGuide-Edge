# 07 — Risk register

Nine days, one person, physical hardware. Review this at the start of every working session.

| ID | Risk | Likelihood | Impact | Mitigation | Trigger → action |
|---|---|---|---|---|---|
| **R-01** | **HC-SR04 5 V ECHO damages the nRF52840** | Medium | **Fatal** — no board, no project | Divider or 3.3 V supply verified *before* first long run (`02-HARDWARE.md` §3) | Any flaky GPIO behaviour → stop, measure ECHO with a meter |
| **R-02** | Dataset collection overruns Saturday | **High** | Blocks everything downstream | Session plan pre-written; classes staged in a fixed order; capture script ready the night before | Behind by 14:00 → drop to 200/class evenly, keep all 6 classes |
| **R-03** | Edge Impulse deployment friction (export, arena size, library conflicts) | Medium | Delays G4/G5 by a day | `Arduino_TensorFlowLite` already installed as a direct-deploy fallback | Stuck >3 h → switch to direct TFLite deployment, still publish the EI project |
| **R-04** | Buzzer absent from the build | High (already true) | Cosmetic — CW1 promised it | Ship OLED + LED; state the substitution openly in the deck | Decide by Day 5; do not let it block the demo |
| **R-05** | Model overfits to one session; collapses in the defence room | Medium | Embarrassing live failure | Session-wise split, re-mounting between sessions, S3 held out entirely | Held-out F1 far below CV → collect a 4th session in a new room |
| **R-06** | RAM exhaustion — tensor arena vs stack | Medium | Runtime crash, hard to debug | Size the arena empirically; keep M3 windows short; measure real RAM from the build | Init failure → shrink arena, shrink model, drop M3 |
| **R-07** | LDR saturates at the studio's actual light levels | Medium | Brightness classes become unusable | Check headroom during Day-1 calibration; swap the partner resistor if pinned | Readings pinned at 0 or full scale → change divider resistor, re-calibrate |
| **R-08** | Ultrasonic dropouts against soft modifiers (softbox, umbrella) | **High** | Noisy distance channel | Median-of-5 on-device, range gating, dropout rate logged and reported | >10% dropout → report it as a finding and add the APDS9960 proximity channel |
| **R-09** | Live demo fails in the presentation room | Medium | Up to 10% of the mark | Video fallback recorded Day 7; arrive early; bring spare cable | Anything odd in setup → switch to video without hesitation, keep talking |
| **R-10** | Presentation overruns 12 minutes | Medium | Organisation row + content gets cut | Two timed rehearsals; slides 4 and 12 marked as droppable | Rehearsal >12:30 → cut slide 4 |
| **R-11** | Blackboard upload fails at the deadline | Low | **Non-submission** | Submit **8 August**, a day early; verify every file re-downloads and opens | Any upload error → re-zip, re-upload, screenshot the receipt |
| **R-12** | Scope creep (BLE app, enclosure, extra sensors) | **High** | Eats the buffer | `AGENTS.md` §3 out-of-scope list is binding | Any new idea → write it in future work, do not build it |
| **R-13** | Illness / equipment loss in a 9-day window | Low | Severe | Submit early; commit after every gate; back up `data/` to OneDrive continuously | Any lost day → cut M3 and the robustness probes first |

---

## Standing rules

- **Back up `data/` after every collection session.** The dataset is the only artefact that
  cannot be regenerated from a keyboard. Everything else — models, figures, slides — can be
  rebuilt in hours. Losing the dataset on Day 6 ends the project.
- **Commit after every gate.** `git` is free insurance.
- **Never report a gate as passed without the output that proves it.**
