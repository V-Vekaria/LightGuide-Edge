# M3 — Edge Impulse neural network

**Date:** 7 August 2026
**Public project:** https://studio.edgeimpulse.com/public/1082649/live
**Project ID:** 1082649 · `Vish_Vekzz / LightGuide-Edge`
**Target:** Arduino Nano 33 BLE Sense (Cortex-M4F 64 MHz)

This closes the CW1 promise of a TensorFlow Lite Micro model and satisfies the Code
submission's requirement for a public Edge Impulse project link. It also produced the
evidence that justifies **not** deploying it.

---

## 1. The comparison is like-for-like, deliberately

Edge Impulse was configured to see exactly what `tools/train_offline.py` sees. Nothing was
tuned to flatter either side.

| | scikit-learn pipeline | Edge Impulse |
|---|---|---|
| Input | `d_dist_mm`, `d_ldr` — deviations from the saved reference | identical |
| Sample rate | 8 Hz | 8 Hz (auto-detected) |
| Window | 10 samples | 1,250 ms = 10 samples |
| Stride | 5 samples | 625 ms = 5 samples |
| Training windows | **195** | **195** |
| Held-out test windows | **104** | **104** |
| Test set | session 3 | session 3 |

The window counts matching exactly — 195 and 104 — is the check that the two pipelines are
cutting the same data the same way. Any difference in results is therefore a difference in
*model*, not in preprocessing.

**Architecture:** Flatten (7 statistics × 2 axes = 14 features) → Dense 20 → Dense 10 → 5
classes. Edge Impulse's default for this input shape.

---

## 2. Results on the held-out session

| Metric | Value |
|---|---|
| Accuracy | **66.35%** |
| Weighted F1 | 0.68 |
| Weighted precision | 0.63 |
| Weighted recall | 0.76 |
| Area under ROC curve | 0.97 |

### Confusion matrix, session 3

| true \ predicted | OPTIMAL | OVERLIT | TOO_CLOSE | TOO_FAR | UNDERLIT | UNCERTAIN | F1 |
|---|---|---|---|---|---|---|---|
| **OPTIMAL** | 0% | 0% | 0% | 0% | 0% | **100%** | **0.00** |
| **OVERLIT** | 0% | **100%** | 0% | 0% | 0% | 0% | 1.00 |
| **TOO_CLOSE** | 0% | 0% | **66.7%** | 0% | 0% | 33.3% | 0.80 |
| **TOO_FAR** | 0% | 0% | 0% | **65%** | 0% | 35% | 0.79 |
| **UNDERLIT** | 0% | 0% | 0% | 0% | **100%** | 0% | 1.00 |

Macro-F1 = **0.72**.

---

## 3. The deployment decision, and why the data made it

| | Decision tree (deployed) | Edge Impulse NN |
|---|---|---|
| Held-out accuracy | **86.5%** | 66.35% |
| Held-out macro-F1 | **0.868** | 0.72 |
| `optimal` F1 | **works** | **0.00** |
| Model size | 5 leaves, ~10 lines of C | 14→20→10→5 network |
| Runtime dependency | **none** | TensorFlow Lite Micro |
| Interpretable | yes — thresholds in mm and counts | no |

**The neural network cannot recognise `optimal` at all.** Every single optimal window in the
held-out session came back `uncertain` — below the confidence threshold, so the model declines
to answer rather than answering wrongly.

That is the worst class to fail. `optimal` is the state the entire product exists to detect: it
is the moment the operator can stop adjusting. A model that never says "you are set up" cannot
drive this device, whatever its average accuracy.

The failure mode is honest — `uncertain` rather than a confident wrong answer — and the ROC AUC
of 0.97 shows the network has learned real structure. It is a *calibration* failure at the
centre of the feature space, where `optimal` sits surrounded on all four sides by the deviation
classes. The tree handles that region by construction: it partitions with axis-aligned cuts and
`optimal` is simply the box in the middle.

**Conclusion: the decision tree ships.** Not because neural networks are unsuitable, but
because on this data, at this scale, a 5-leaf tree is 20 percentage points more accurate, has
no library dependency, runs in microseconds, and gets right the one class that matters most.

CW1 promised INT8 TensorFlow Lite Micro. That model now exists and has been measured. Choosing
not to deploy it is a result, not an omission.

---

## 4. Limitations of this comparison

Stated so the claim is not overread:

1. **The Edge Impulse architecture is its default.** No hyperparameter search was run — no EON
   Tuner sweep, no layer-size exploration. A tuned network might close some of the gap. The
   claim here is not "neural networks lose", it is "the default network loses to a tree that
   costs a hundredth as much".
2. **195 training windows is a small dataset for a neural network** and a comfortable one for a
   depth-4 tree. That asymmetry favours the tree and is part of why it wins — which is itself
   the relevant engineering point for an embedded project with an hour of collected data.
3. **The `uncertain` bucket is a threshold artefact.** Edge Impulse applies a default confidence
   floor; a lower one would convert those into predictions and change the numbers. The
   underlying probabilities were not inspected.
4. **One seed.** The scikit-learn results are averaged over five seeds; this is a single run.
