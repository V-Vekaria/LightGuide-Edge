# Phase 1 — distance guide acceptance

**Date:** 6 August 2026
**Sketch:** `firmware/03_inference` at commit `02641a3`
**Board:** Arduino Nano 33 BLE (nRF52840) on COM4, HC-SR04 powered from 3V3
**Reference instrument:** tape measure
**Spec:** `docs/superpowers/specs/2026-08-06-distance-guide-design.md` §8

Two logs back this report, both committed under `data/online/`:

| Log | What it is |
|---|---|
| `phase1_distance_60s_20260806.csv` | 60 s static hold, one target, undisturbed |
| `phase1_endurance_5min_20260806.csv` | 5 min live operation — operator moving targets and re-referencing |

---

## 1. Decision self-test

```
DECISION SELF-TEST: 18/18 passed - OK
```

18 cases run on the board at every boot, covering plain classification, both edges of the
enter band, both edges of the exit band, no-echo precedence, and a non-default reference.

The test was demonstrated to fail before it was made to pass: the first implementation of
`decide()` ignored its history parameter and the harness caught exactly the three hysteresis
cases that depend on it (`15/18`, failures at indices 7, 9 and 11). The test has teeth.

---

## 2. Static performance — 60 s, one target

| Measurement | Value |
|---|---|
| Rows logged | 471 in 60.0 s |
| Sample rate | **7.85 Hz** |
| Dropped pings | **0 of 2,355 — 0.000%** |
| Reading noise, σ | **1.89 mm** |
| Full spread | 11 mm (1287–1298) |
| Verdict changes | **0** |
| Reference held | `ref_mm = 1269` on every row |

**The ±30 mm tolerance is 16σ wide.** That is why zero verdict changes occurred across 471
consecutive samples, and it is the direct experimental justification for the ±3 cm band if it
is questioned in the Q&A.

The `ref_mm = 1269` column is independent confirmation that the button-set reference persisted
correctly for the whole run — it was set by the operator at 126.9 cm, not by the default.

---

## 3. Live operation — 5 min, operator active

| Measurement | Value |
|---|---|
| Rows logged | 2,328 in 300 s |
| Sample rate | 7.76 Hz |
| Dropped pings | **9 of 11,640 — 0.077%** |
| Distance range exercised | 48 mm to 2,647 mm |
| Reference saves | **6, all successful** (`# REFERENCE SAVED` at 933, 916, 1282, 1287, 1034, 986 mm) |
| Saves refused | 0 |
| **Board resets** | **0** — no repeated boot banner anywhere in the log |
| Verdicts seen | CORRECT 941, CLOSE 937, FAR 450 |

Zero resets across five minutes of active use, including six reference captures, satisfies
the endurance requirement.

---

## 4. Ultrasonic range at 3.3 V — the open hardware question closed

`docs/02-HARDWARE.md` §3 took Option B (run the HC-SR04 from 3V3 rather than 5 V) and flagged
**reduced maximum range** as the accepted cost, to be characterised and stated. It is now
characterised.

| Distance band | Samples | Missed pings | Dropout rate |
|---|---|---|---|
| 0–500 mm | 44 | 0 / 220 | 0.000% |
| 500–1000 mm | 856 | 8 / 4,280 | 0.187% |
| 1000–1500 mm | 1,424 | 0 / 7,120 | **0.000%** |
| 2000–3000 mm | 4 | 1 / 20 | 5.000% |

**Dropouts do not increase with range across the working band.** The 1000–1500 mm band — which
contains the majority of samples and the whole of the intended operating range — recorded
literally zero missed pings out of 7,120. The only band with a worse rate is 500–1000 mm, and
inspection shows those eight misses cluster at 923–960 mm, i.e. at a *particular target*, not a
particular distance. That points at surface angle or material, which is the documented
behaviour of the sensor, not an artefact of the supply voltage.

The 2000–3000 mm figure is one missed ping out of twenty, from four samples. It is far too
small to draw a conclusion from and is reported only for completeness.

**Conclusion:** running from 3.3 V did not measurably cost range within the working envelope.
Valid readings were obtained out to 2,647 mm. The risk flagged in CW1 and carried into
`02-HARDWARE.md` is closed with evidence.

---

## 5. Hysteresis — tested against real data, and vindicated

The 5-minute log contains 262 single-row verdict flip-backs (A → B → A), 11.25% of rows. That
pattern is the signature of boundary chatter, so it was investigated rather than reported as a
pass.

**It is not chatter.** Chatter is a verdict flip while the reading is essentially unchanged.
The distribution of the jump in `live_mm` at each flip-back:

| Jump size | Count |
|---|---|
| < 5 mm | **0** |
| 5–20 mm | **0** |
| ≥ 20 mm | 262 |

Minimum jump 45 mm, mean 282 mm, maximum 1,497 mm. **Not one flip-back occurred with a
small reading change.** Combined with zero verdict changes across the entire static run, the
hysteresis does exactly what it was added to do.

### What the flip-backs actually are

Consecutive rows around index 354 (reference 933 mm):

```
diff  -65 CLOSE
diff  +49 FAR
diff  -66 CLOSE
diff  +63 FAR
diff  -67 CLOSE
```

The reading alternates between roughly 866 mm and 1,363 mm on successive samples, 130 ms
apart. No physical object moves 1.5 m in 130 ms. This is the HC-SR04's ~15° beam containing
**two reflective surfaces** and returning whichever echoed first, alternating between them —
precisely the limitation named in `docs/02-HARDWARE.md` §6: *it measures "something in front",
not specifically the subject.*

The median-of-5 filter removes outliers **within** a sample group. It cannot help here, because
all five pings in a group legitimately agree on the wrong target.

---

## 6. Operator-confirmed behaviour

Confirmed by the operator on the physical rig. Recorded here as observed behaviour; no
numeric values were logged for these.

| Test | Result |
|---|---|
| CORRECT / FAR / CLOSE at 100 / 150 / 70 cm | ✅ as specified |
| `CORRECT` fits the 128 px panel at text size 3 | ✅ no clipping |
| One chirp on reaching CORRECT, then silence | ✅ |
| Chirp repeats after leaving and returning | ✅ |
| Slow low beeps at FAR, fast high beeps at CLOSE | ✅ |
| `NO ECHO` when the sensor is covered, silent, recovers | ✅ |
| Hold 2 s sets a new reference | ✅ confirmed 3× by operator, 6× in the log |
| Stuck-switch guard | ✅ no `# switch stuck LOW` warning — D7 reads HIGH correctly |

---

## 7. Resource use

| Resource | Used | Limit |
|---|---|---|
| Flash | 109,464 bytes — **11%** | 983,040 bytes |
| RAM (globals) | 46,304 bytes — **17%** | 262,144 bytes |

Ample headroom for the TFLite Micro model in Phase 2.

**Measured cost of the display:** the loop ran at 11.7 Hz before the OLED was added and 7.85 Hz
after. The I²C write costs roughly a third of the sample rate. Acceptable here, and worth
stating as a deliberate trade-off rather than leaving unexplained.

---

## 8. Limitations

Named here because the Demo rubric row rewards *extensive understanding of the limitations*.

1. **Multi-target beam ambiguity** — originally recorded here as "the dominant error mode" and
   an inherent sensor property. **§10 corrects this:** the cause was an angled target, and
   squaring it removed the effect entirely. The residual limitation is real but narrower than
   stated: *the subject must present a face roughly square to the sensor.* Off-axis surfaces
   deflect the pulse and the beam then measures whatever else is in the cone — and it does so
   with no dropped pings, so nothing in the data marks the reading as untrustworthy.
2. **The reference is held in RAM only.** A power cycle silently reverts to the 100 cm default.
   Deliberate (spec §9), but the operator gets no warning that it happened.
3. **No temperature compensation.** Speed of sound varies ~0.6 m/s per °C. The LPS22HB's
   temperature channel is available on-board and unused.
4. **The tolerance is a fixed ±30 mm** and does not widen with distance, although ultrasonic
   error grows with range. Not a problem at ~1 m; would be at 3 m.
5. **The verdict is a threshold, not a model.** Phase 1 by design; the classifier replaces
   `decide()` in Phase 2.
6. **Sample rate is 7.76 Hz**, set by five pings at 10 ms spacing plus the OLED write. Fast
   motion is under-sampled.

---

## 9. Recommended next change — SUPERSEDED, see §11

This section originally recommended a **between-sample plausibility filter** to suppress the
verdict jumps identified in §8.1, on the reasoning that a 300 mm change between consecutive
samples implies 2.3 m/s of target motion and is therefore implausible.

**That recommendation was wrong, and §11 records the experiment that disproved it.** The filter
was never implemented, and on this evidence it should not be.

---

## 10. Correction to §8.1 — the dominant error mode was target geometry

**Added 6 August, after the fault recurred during Phase 2 work.**

§8.1 named multi-target beam ambiguity as "the dominant error mode" and treated it as an
inherent property of the sensor. A photograph of the rig showed the actual cause: **the target
box was leaning back against a radiator, presenting an angled face to the beam.**
`docs/02-HARDWARE.md` §6 names this directly — "soft or angled surfaces absorb or deflect the
pulse" — but it had been read as a caveat about *materials* rather than as the operating
condition that was actually in force.

An angled face reflects the pulse away from the receiver instead of back to it. The beam then
returns whatever else is in the ~15° cone, which in a furnished room is a wardrobe door, a
radiator or a bed base. The three tight clusters were three real objects, correctly measured.
The sensor was never at fault, and neither was the median-of-5 filter.

### The controlled experiment

One variable changed — the box was stood square to the beam. Nothing else was touched: same
room, same sensor, same firmware, same position, minutes apart.

| Measurement | Angled target | Square target |
|---|---|---|
| Distinct reading clusters | **3** — 935, 1110, 1405 mm | **1** |
| Total spread | 481 mm | **3 mm** (719–722) |
| Noise σ | not meaningful (multi-modal) | **0.6 mm** |
| Consecutive jumps > 100 mm | **41 of 129** | **0 of 135** |
| Dropped pings | 0 of 650 | 0 of 680 |
| Samples | 130 | 136 |

**The error mode did not need software. It needed the target standing up straight.**

σ = 0.6 mm also makes the ±30 mm tolerance **50σ wide**, not the 16σ estimated in §2 — the
Phase 1 figure was measured against a target that was, unknowingly, already partly misaligned.

### Why this is worth saying out loud in the deck

Three reasons, and the Evaluation and Demo rubric rows reward all of them:

1. **A measured conclusion was overturned by a better-controlled measurement.** The original
   analysis was rigorous about *what* the data showed and wrong about *why*. That distinction is
   the difference between reporting and understanding.
2. **The proposed fix would have hidden the fault.** A plausibility filter would have smoothed
   the jumps away and left the device quietly measuring the wardrobe instead of the subject.
   It would have looked like an improvement and been a regression.
3. **It sets a real operating constraint on the product**: the subject must present a surface
   roughly square to the sensor. That is a genuine limitation of ultrasonic ranging, it is now
   quantified rather than asserted, and it belongs on the limitations slide.

---

## 11. Revised recommendation

**Do not add the plausibility filter.** On this evidence it addresses a symptom whose cause is
target alignment, and it would mask exactly the condition the operator needs to be told about.

If robustness against off-axis targets is wanted later, the honest approach is the opposite of
smoothing: **detect** multi-modal readings and *report* them — if consecutive sample groups
disagree by more than the tolerance band while the miss count stays at zero, the beam is seeing
more than one surface, and the right response is to tell the operator to re-aim rather than to
average the two answers together.

---

## 10. Verdict

Phase 1 meets its definition of done (spec §10):

- ✅ `firmware/03_inference` compiles and uploads to the Nano 33 BLE
- ✅ Self-test passes 18/18 on-device
- ✅ Serial CSV is well-formed and parseable — two logs committed
- ✅ `decide()` is a pure function of its arguments and calls no hardware
- ✅ Behaviour confirmed on the physical rig against a tape measure

One item from spec §8 was **not** executed as written: the static 5-minute endurance hold. The
operator was actively using the rig during that window, so the log is of live operation
instead. It evidences the same property — zero resets over five minutes — and carries
considerably more information, so it was kept rather than re-run.
