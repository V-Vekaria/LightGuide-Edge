# 10 — Q&A defence pack (10% "Understanding of Theoretical Knowledge")

5–10 minutes of questions after the presentation. The 1st-class descriptor is "answers all
questions **with explanations and elaboration**"; the High 1st adds "uses this knowledge to
**identify opportunities for innovation/creativity**".

Practical translation: **answer, explain the mechanism, then extend.** A correct one-line
answer scores in the 60s. The same answer plus *why* plus *what it implies for future work*
scores in the 80s. Drill the extension habit, not just the facts.

---

## 1. Questions you will almost certainly be asked

**"Why machine learning? Couldn't you do this with three if-statements?"**
The strongest question in the set — have this one word-perfect. Distance and brightness are
physically coupled through the inverse-square law: illuminance falls with the square of
distance, so moving the light further away *and* turning it up can leave the light reading
unchanged while the setup is wrong. Independent per-sensor thresholds cannot resolve that;
they need the joint distribution. *Then extend:* "and the M0 baseline quantifies this — a
threshold rule scored X, the MLP scored Y on the same held-out session."

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

**"What happens if the device sees something that isn't one of your six classes?"**
This is the autoencoder's job, and it is the part to be proud of. A six-class softmax must
answer, so it answers confidently and wrongly. The autoencoder is trained only on `optimal`
data; when reconstruction error exceeds the 95th-percentile training threshold the device
displays `UNKNOWN SETUP`. *Extend:* honest uncertainty is a requirement for a tool anyone
would trust in paid work — a confident wrong answer costs a whole session.

**"What's the accuracy?"**
Never answer with one number. "Macro-F1 of X on a held-out session, versus Y under 5-fold
CV; the gap is session variation. Accuracy alone would read Z, but macro-F1 is the honest
metric because it weights all six classes equally and a missed `tilt_off` matters as much as
a missed `too_far`."

**"What's your latency?"**
Two numbers, and volunteer both. Inference is X ms measured with `micros()` over n=500. But
the number the *user* experiences is end-to-end sense-to-display, Y ms, which is dominated by
the ultrasonic ping (~60 ms minimum) not by the model. Quoting only the inference figure
would be an overclaim.

**"Which sensor matters most?"**
Point at the ablation table. Distance-only gets X, +LDR gets Y, +IMU gets Z. *Extend:* if the
IMU contribution is small, the honest recommendation is a cheaper two-sensor variant.

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
