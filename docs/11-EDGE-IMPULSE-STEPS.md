# 11 — Edge Impulse: exact steps

**Why this is not optional.** `00-REQUIREMENTS-LOCKED.md` §2 quotes the handbook: the Code
`.zip` must contain the Arduino library **and a link to the public Edge Impulse project**.
A missing EI link is a missing element of a compulsory component. CW1 also promised
"INT8-quantised TFLite Micro on the Nano", and slide 9 needs quantisation, tensor arena and
footprint numbers that only exist once this is done.

**Time: ~45 minutes**, most of it waiting for training.

The dataset is already exported and formatted. Run this if you have not:

```bash
py -3 tools/export_edge_impulse.py
```

That writes `deliverables/edge_impulse/training/` (30 runs, sessions 1–2) and
`deliverables/edge_impulse/testing/` (15 runs, session 3).

---

## 1. Create the project

1. Sign in at **https://studio.edgeimpulse.com** (free account).
2. **Create new project** → name it `LightGuide-Edge` → **Developer** (free) tier.
3. Leave it private for now; step 7 makes it public.

## 2. Upload the data

1. **Data acquisition** → **Add data** → **Upload data**.
2. Upload the **training** folder first:
   - Select all files under `deliverables/edge_impulse/training/` (all five class folders).
   - Upload into category: **Training**
   - Label: **Infer from filename** — the exporter puts the class in the folder and the
     filename (`session1_0_optimal_...`), so this works. **Check the labels afterwards.**
3. Repeat for `deliverables/edge_impulse/testing/` into category **Testing**.

> **Do not press "Perform train/test split".** It splits at random, which puts near-identical
> 8 Hz neighbours on both sides of the boundary and reports memorisation as accuracy. The
> directory split already holds session 3 out, which is the whole point of the offline design.

Expected after upload: **30 training samples, 15 testing samples**, five labels.

## 3. Create the impulse

**Impulse design** → **Create impulse**:

| Block | Setting | Why |
|---|---|---|
| Time series data | Window size **1250 ms**, window increase **625 ms**, frequency **8 Hz** | Reproduces the offline `WINDOW=10 / STRIDE=5` exactly, so the comparison is fair |
| Processing block | **Flatten** | Computes mean/std/min/max per axis — the same eight features as the offline pipeline |
| Learning block | **Classification** | Five classes |

Save impulse.

> If you pick **Spectral Analysis** instead of Flatten, you are no longer mirroring the
> offline features and the comparison stops being like-for-like. Flatten is the correct
> choice here and you should be ready to say why in the viva.

## 4. Generate features

**Flatten** → tick **mean, std, min, max** (untick the rest) → **Save parameters** →
**Generate features**.

Look at the feature explorer plot. It should show the same structure as
`reports/figures/feature_space.png`: light classes separating vertically, distance classes
horizontally. If it does not, the upload labels are wrong — fix that before training.

## 5. Train

**Classifier** → set:

- Training cycles: **100**
- Learning rate: **0.0005**
- Architecture: **Dense 32 → Dense 16** (mirrors offline M1)

**Start training.** Record from the results panel:

- [ ] Float32 accuracy: ______
- [ ] Confusion matrix (screenshot → `reports/figures/ei_confusion.png`)

## 6. Quantise and read the profiler

1. In the same panel switch the model type to **Quantized (int8)**.
2. Record, because slide 9 needs all of these:

| Metric | Float32 | INT8 |
|---|---|---|
| Accuracy | | |
| Latency (est.) | | |
| Peak RAM | | |
| Flash | | |

The **float32 → INT8 accuracy delta** is the number the ML plan asks for. Expect roughly
4× smaller for under 1% accuracy cost — **report whatever actually happens**, including if
it gets worse.

3. **Model testing** → **Classify all**. This runs the held-out session 3. Record that
   accuracy — it is directly comparable to the offline **0.868** macro-F1.

## 7. Make it public — required

**Dashboard** → scroll to **Make this project public** → confirm.

Copy the public URL and paste it in three places:

- [ ] `AGENTS.md` §1 identity table
- [ ] `deliverables/README-CODE.txt` (the packaging script writes a placeholder)
- [ ] The references/demo slide

> Making it public exposes the dataset. That is required by the handbook and is fine —
> it is your own data, contains no personal information, no images and no audio
> (`docs/08-ETHICS-SDG.md`).

## 8. Export the Arduino library

**Deployment** → **Arduino library** → select **Quantized (int8)** → **Build**.

Save the downloaded `.zip` to `deliverables/edge_impulse_arduino_library.zip`. The packaging
script picks it up from there.

---

## What to do if the EI model is worse than the decision tree

Very possible — a depth-4 tree on eight clean features is a strong baseline, and 195 training
windows is a small set for a neural network.

**This is a result, not a problem.** Report it:

> "The deployed classifier is the decision tree, not the neural network. On identical
> splits the tree reached 0.868 held-out macro-F1 against the quantised network's [X],
> at a fraction of the flash and with rules that can be read and checked against the
> physical tolerances in the data protocol. Choosing the simpler model on evidence is
> the finding."

That reads as engineering judgement and hits "compare different methodological approaches"
harder than quietly shipping whichever model happened to win. The EI project still exists,
is still public, and the comparison is still real — which is what the handbook asks for.

---

## Checklist

- [ ] Project created
- [ ] 30 training / 15 testing uploaded, labels verified
- [ ] Impulse: 1250 ms window, 625 ms stride, Flatten, Classification
- [ ] Features generated, explorer looks like the offline feature space
- [ ] Trained; float32 accuracy recorded
- [ ] INT8 quantised; delta, RAM, flash, latency recorded
- [ ] Model testing run on session 3
- [ ] **Project made public, URL recorded**
- [ ] Arduino library exported to `deliverables/edge_impulse_arduino_library.zip`
