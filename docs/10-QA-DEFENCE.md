# 10 — Q&A defence pack (10% "Understanding of Theoretical Knowledge")

5–10 minutes of questions after the presentation. The 1st-class descriptor is "answers all
questions **with explanations and elaboration**"; the High 1st adds "uses this knowledge to
**identify opportunities for innovation/creativity**".

Practical translation: **answer, explain the mechanism, then extend.** A correct one-line
answer scores in the 60s. The same answer plus *why* plus *what it implies for future work*
scores in the 80s. Drill the extension habit, not just the facts.

---

## 1. Questions you will almost certainly be asked

**"Why machine learning? The device already stores the reference setup — why not just
subtract and report the difference?"**

**The single most likely question in the whole viva.** Any marker looking at three sensors
and a saved reference will ask it. Answer in this order — concede first, then build.

*Concede the strong version.* For a bare lamp in one room with no modifier, reference
subtraction works nearly as well. Even the obvious coupling is solvable analytically:
illuminance ∝ 1/d², so `k = lux × d²` isolates whether the lamp's *output* changed
independently of its position. No ML needed for that case. **Say this out loud** — it shows
the alternative was actually considered rather than dismissed.

*Then give the four reasons it breaks:*

1. **The inverse-square law does not hold for real modifiers.** 1/d² describes a *point*
   source. A softbox or umbrella is an **extended area source**: at close range its falloff
   approaches 1/d, and the transition depends on the modifier's physical size. No clean
   closed form, and it differs per modifier. A model learned from the actual equipment
   captures the true relationship without deriving it. **This is the strongest single point.**
2. **Tilt couples into brightness.** Illuminance scales with the cosine of incidence angle,
   so tilt changes the light reading even when nothing moved and nothing dimmed. Three
   entangled variables, not two.
3. **The LDR is not a lux meter.** Power-law response (γ ≈ 0.7, measured — see
   `reports/calibration.md`), temperature-sensitive, spectrally non-flat: tungsten and LED
   at equal lux read differently. An analytical model needs all of that characterised.
4. **Thresholds do not generalise.** Bands tuned in one room drift with ambient, lamp and
   mounting. Hence three physically distinct sessions and a held-out test session.

*Then the point thresholds cannot touch:* a stored reference can only report a delta from
the reference. It **cannot report that the reading is meaningless**. Someone steps in front
of the sensor, the stand is knocked, the lamp is off — a threshold system confidently
reports "distance 200% high" and is useless. The autoencoder (M2), trained only on valid
setups, flags `UNKNOWN SETUP`. That is unsupervised learning doing something reference
subtraction structurally cannot, and it is the difference between a tool trusted on a paid
shoot and one that gets switched off.

*Then close with evidence, not assertion:* "M0 includes a hand-tuned threshold rule
evaluated on the same held-out session — it scored X, the MLP scored Y." **If the threshold
rule wins, report that.** The rubric asks you to compare methodological approaches, not to
conclude the most complex one won. An honest negative result scores better than a defended
one.

**"Why did you split by session instead of randomly?"**
At 10 Hz, consecutive samples are near-duplicates. A random split places near-identical rows
in train and test, so test accuracy measures memorisation rather than generalisation. A
held-out session tests what actually matters: does it work after the device is re-mounted in
a different room? *Extend:* the CV↔held-out gap quantifies exactly how session-dependent the
model is.

**"What does INT8 quantisation actually do?"**
Maps float32 tensors to 8-bit integers with a per-tensor scale factor and zero point, so a
real value ≈ `scale × (q − zero_point)`. Roughly 4× smaller, and usually *faster*, because
the Cortex-M4F runs integer CMSIS-NN kernels rather than float ops. Cost is a small
precision loss — ours was X%. *Extend:* quantisation-aware training would likely recover most
of that if the loss were material.

**"How much RAM does your model use, and what limits it?"**
TFLite Micro does no dynamic allocation — you pre-allocate a static **tensor arena** sized to
hold the largest set of simultaneously-live activations. Weights live in flash; activations
live in RAM. Ours: arena X KB of the 256 KB total, leaving Y for stack and buffers. *Extend:*
RAM, not flash, is what capped the sequence model's window length.

**"What happens if the device sees something that isn't one of your five classes?"**
This is the autoencoder's job, and it is the part to be proud of. A five-class softmax must
answer, so it answers confidently and wrongly. The autoencoder is trained only on `optimal`
data; when reconstruction error exceeds the 95th-percentile training threshold the device
displays `UNKNOWN SETUP`. *Extend:* honest uncertainty is a requirement for a tool anyone
would trust in paid work — a confident wrong answer costs a whole session.

**"What's the accuracy?"**
Never answer with one number. "Macro-F1 of X on a held-out session, versus Y under 5-fold
CV; the gap is session variation. Accuracy alone would read Z, but macro-F1 is the honest
metric because it weights all five classes equally and a missed `underlit` matters as much as
a missed `too_far`."

**"What's your latency?"**
Two numbers, and volunteer both. Inference is X ms measured with `micros()` over n=500. But
the number the *user* experiences is end-to-end sense-to-display, Y ms, which is dominated by
the ultrasonic ping (~60 ms minimum) not by the model. Quoting only the inference figure
would be an overclaim.

**"Which sensor matters most?"**
Point at the ablation table. Neither is sufficient alone — distance-only reaches **0.410**
macro-F1 and light-only **0.461**, both barely above the five-class chance floor. Together
they reach **0.868**. *Extend:* that is the argument for a multi-sensor device rather than a
cheaper single-sensor one, and it is measured rather than assumed.

**"Why five classes? Your proposal said six."** ← near-certain, have this ready
Straight answer, no hedging: "The sixth class was `tilt_off`, driven by the on-board IMU. It
was cut. Two hardware faults consumed the days budgeted for it — the ultrasonic ECHO line and
a wrong LDR pulldown resistor, both documented with the measurements that found them. Adding
the class late would have meant recollecting all three sessions, because the feature vector
changes for every sample. I chose to hold the five classes to the full quality gate —
332–393 samples each, three sessions, zero dropouts, zero NaNs, a genuinely held-out test
session — rather than ship six thin ones. The IMU is on the board and initialised; the cost
is recollection, not hardware, and it is the first thing on the future-work slide."

*Why this works:* it names the decision, gives the real reason, shows the trade was
considered rather than forgotten, and lands on a costed future-work item. The Demo and
Evaluation rows both explicitly reward understanding of limitations.

**"Your threshold rule beats your ML model. Why use ML at all?"** ← ask yourself this before they do
The honest answer, and it is a good one: "On this dataset it does, and I report that in the
results rather than hiding it. But the comparison is circular — each class was *staged*
against the same ±30 mm and ±5% bands the rule encodes, so the rule is scored against its own
definition and cannot lose. What that score measures is the consistency of my staging
protocol, not the rule's intelligence. Distinguishing the two approaches needs conditions the
rule was never tuned for: confounded setups, transitions, and the settling transients that
only appear in the online evaluation. And the rule has no answer at all for `UNKNOWN SETUP` —
a threshold cannot say 'this is unlike anything I was trained on'. That is what M2 adds."

**"How would you improve this?"**
Only give answers tied to a measured limitation (`05-EVALUATION-PLAN.md` §5). Generic answers
score nothing here.

**"What's novel about this?"**
Two claims, stated plainly: the application gap (lighting tools store digital state, none
guide physical placement), and the classifier + novelty-gate architecture on a 256 KB part.
Both defensible, both testable.

---

## 2. Theory you must be able to explain from scratch

| Topic | Be able to explain |
|---|---|
| Edge vs cloud AI | Latency, privacy, connectivity, per-inference cost, and the cases where cloud genuinely wins |
| TFLite Micro | No dynamic allocation, no OS dependency, static arena, op resolver, interpreter |
| Quantisation | Scale/zero-point, post-training vs quantisation-aware, why integer ops are faster on M4F |
| MLP | Forward pass, ReLU, softmax, cross-entropy, backprop at a conceptual level |
| Autoencoder | Bottleneck, reconstruction loss, why it detects novelty, how the threshold is chosen |
| 1-D CNN | Kernels over time, shared weights, why it suits sequences |
| Overfitting | Symptoms, dropout, early stopping, why held-out sessions expose it |
| Cross-validation | k-fold, stratification, **grouped** CV and why grouping matters here |
| Precision/recall/F1 | Definitions, macro vs weighted, when accuracy misleads |
| Confusion matrix | Reading it, and what off-diagonal structure tells you physically |
| Feature scaling | Why, and why the scaler is fit on training data only |
| Data leakage | The three ways it happens here: random split, scaler on full data, overlapping windows across folds |
| Inverse-square law | Illuminance ∝ 1/d² — the physics that motivates the whole design |
| Sampling rate | Nyquist, and the 60 ms ultrasonic constraint that set 10 Hz |
| ADC | 12-bit resolution, reference voltage, why a divider is needed for an LDR |
| I²C | Addressing, why the OLED is at 0x3C, bus sharing with the on-board sensors |
| nRF52840 | Cortex-M4F, 64 MHz, 1 MB flash, 256 KB RAM, **3.3 V and not 5 V tolerant** |

---

## 3. Questions that are traps

**"Isn't 250 samples per class quite small?"**
Don't get defensive. Yes — and the mitigation is deliberate: session-wise splitting so the
number is not inflated, balanced classes, and confounded cases so the difficulty is real. The
honest framing is that this is enough to demonstrate the method and to compare four
approaches fairly; a production system would need many more sessions across many studios,
which is exactly what the future-work slide says.

**"Your accuracy seems very high — are you sure there's no leakage?"**
Have the answer ready: session-held-out test, grouped CV, scaler fit on training only,
windows never straddling a split. If the number *is* suspiciously high, say what you checked.

**"Why not just use the on-board APDS9960 for both light and proximity?"**
Good question, real answer: the APDS9960 measures light *at the device*, but what matters is
light *at the subject plane*. A separately-placeable LDR can be positioned where the light
actually lands. The APDS9960 remains the fallback distance channel and a useful cross-check.

**"Did the buzzer get built?"**
If it did not: say so plainly, state that CW1 listed it and it was cut, explain the OLED and
LED cover the feedback path, and move on. Owning a cut scores better than pretending.

---

## 4. Drill routine (Day 8)

1. Read §1 aloud, answering from memory before reading the answer.
2. Have someone ask five questions cold and time your answers — 30–60 s each is the target.
3. For every answer, force a "…and that means we could…" extension. That habit is worth the
   difference between the 70s and the 80s on this row.
4. Know where every number on every slide came from. If you cannot source it, remove it.
