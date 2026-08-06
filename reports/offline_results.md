# Offline results

Held-out test session: **3** · train windows: **195** · test windows: **104** · seeds: **5**

Split is session-wise, not random: at 10 Hz consecutive samples are near duplicates, so a random split would leak near-identical windows into the test set. Cross-validation is grouped by run for the same reason.

## Model comparison

| model | CV macro-F1 (mean±std) | test accuracy | test macro-F1 |
|---|---|---|---|
| M0a Decision Tree | 0.882 ± 0.237 | 0.865 ± 0.000 | 0.868 ± 0.000 |
| M0b k-NN (k=5) | 0.784 ± 0.161 | 0.798 ± 0.000 | 0.800 ± 0.000 |
| M0c Logistic Regression | 0.849 ± 0.148 | 0.798 ± 0.000 | 0.786 ± 0.000 |
| M0d Random Forest | 0.896 ± 0.207 | 0.865 ± 0.000 | 0.868 ± 0.000 |
| M1  MLP (32-16) | 0.858 ± 0.138 | 0.858 ± 0.015 | 0.860 ± 0.015 |

Confusion matrices: `reports/figures/confusion_*.png`.
Decision tree rules: `reports/decision_tree.txt` - check the split thresholds against the physical tolerances in the data protocol. When they agree, that is independent evidence the dataset is sane.

## Generalisation gap

Best model: **M0a Decision Tree**. CV macro-F1 0.882 vs held-out 0.868 — a gap of **+0.014**.

This gap is what session-wise splitting exists to expose: it quantifies how much performance depends on conditions specific to the training sessions. A random split would have hidden it.

## M2 - autoencoder novelty gate

Trained on `optimal` windows only; flags anything it reconstructs badly. Threshold = 95th percentile of the training reconstruction error.

| metric | value |
|---|---|
| threshold (MSE) | 0.7617 |
| precision | 1.000 |
| recall | 0.169 |
| F1 | 0.289 |
| TP / FP / TN / FN | 14 / 0 / 21 / 69 |

On-device this gates the classifier: high reconstruction error shows `UNKNOWN SETUP` rather than a confident wrong class.

## Sensor ablation

Does each sensor earn its place? Same model (M1), same splits.

| sensors | features | CV macro-F1 | test macro-F1 |
|---|---|---|---|
| distance only | 4 | 0.448 | 0.410 |
| light only | 4 | 0.501 | 0.461 |
| distance + light | 8 | 0.858 | 0.868 |

If a sensor adds little, say so and propose the cheaper variant in future work. An honest negative result is still a result.

## Per-class detail - M0a Decision Tree

```
              precision    recall  f1-score   support

     optimal       0.60      1.00      0.75        21
   too_close       1.00      0.67      0.80        21
     too_far       1.00      0.65      0.79        20
    underlit       1.00      1.00      1.00        21
     overlit       1.00      1.00      1.00        21

    accuracy                           0.87       104
   macro avg       0.92      0.86      0.87       104
weighted avg       0.92      0.87      0.87       104

```
