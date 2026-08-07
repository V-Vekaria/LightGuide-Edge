# 00 — CW2 Requirements (LOCKED)

Source: *S3 COM683 Module Handbook 2526* §4, "Coursework 2 — Project Presentation and Demo [60%]".
Everything below is quoted-or-paraphrased from the handbook. Do not re-interpret it.

---

## 1. What the handbook demands

> "you are required to implement the solution outlined in the project proposal. Projects
> must be carried out on **real hardware and software testbeds**. Components of the
> assessment will require **collection, processing and visualisation of data**, development
> and implementation of **machine learning algorithms**, a **critical evaluation** of its
> performance and presentation of the findings. Projects must include elements of **both
> embedded systems and machine learning**."

Four requirements that are easy to lose marks on and are often skipped:

| Requirement | Our answer |
|---|---|
| **Visualisation of data** — explicitly named, often forgotten | Dedicated figures: class distributions, feature scatter/pair plots, time-series traces per class, confusion matrices, ablation bars. `reports/figures/` |
| Real hardware testbed | Nano 33 BLE Sense + HC-SR04 + LDR + IMU + OLED, all physically present |
| Both embedded **and** ML | On-device TFLite Micro inference, not a laptop demo |
| SDG alignment (stated in the CW overview) | SDG 12 primary, SDG 8 secondary — see `08-ETHICS-SDG.md` |

---

## 2. Submission components (all compulsory, separate Blackboard areas)

Naming: `SurnameFirstNameBNumber_Component` → `VekariyaVishnuB00XXXXXX_Code`

1. **Code** — `.zip` containing the Arduino library **and a link to the public Edge Impulse project**. Must be commented.
2. **Dataset** — `.zip` containing **test and train samples**.
3. **Slides** — `.ppt`/`.pptx` or `.pdf`.
4. **Video** — approx. **1 minute**, a **split-screen** demo showing the sensor/hardware
   responding to an input stimulus. Submitted via the **Panopto Media Assignment Dropbox**
   (not the normal file dropbox).

> "You should check that the submission can be opened as corrupted files will be treated as
> a non-submission. You should keep a receipt (screenshot) of the confirmation provided by
> Blackboard as proof of submission."

**Deadline: 12:00 noon, 9 August 2026.** Presentation: week commencing 10 August.

---

## 3. Presentation content spec (indicative slide counts are from the handbook)

| Slides | Content required | Weight |
|---|---|---|
| 1 | Brief summary of the specific problem the solution addresses | **5%** |
| 2 | Description of the solution and how it *specifically* addresses the identified problem. **Must draw on relevant literature** and consider **ethical, technical and social** contextual factors | **10%** |
| 2 | Methodology for collecting the dataset, **including the steps taken to ensure high-quality data was collected and labelled** | **15%** |
| 3 | Implementation of ML: **preprocessing, feature extraction, modelling approaches, validation methodology** | **20%** |
| 3 | Critical evaluation: **deep analysis of ML results, both offline and online**; **compare different methodological approaches**; **identify avenues for future improvement** | **20%** |
| 1 | Demonstration of the solution (video or live) | **10%** |
| — | Understanding of theoretical knowledge (assessed in Q&A) | **10%** |
| — | Quality of presentation: structure and delivery | **10%** |

Total ≈ 12 content slides. Presentation is **approx. 12 minutes**, then **5–10 minutes of questions**.

---

## 4. Rubric — the 70–79% (1st) and 80–100% (High 1st) bands verbatim

The target is: **hit 1st on every row, High 1st on ML / Evaluation / Demo.**

### Problem Overview — 5%
- **1st:** stated clearly and described comprehensively, delivering all relevant information necessary for full understanding.
- **High 1st:** …*plus* **opportunities for innovation are highlighted**.

### Description of Solution — 10%
- **1st:** deep comprehension of the problem; justification of the selected solution **drawing on relevant literature**; sensitive to **all** contextual factors — ethical, technical and social.
- **High 1st:** …*plus* **highlighting the innovative nature of the approach**.

### Methodology for Data Collection — 15%
- **1st:** fully detailed, excellent methodology providing **ample high-quality data**; all elements explained and justified **drawing on appropriate literature/experimentation**.
- **High 1st:** …*plus* methods demonstrate **creativity/innovation**.

### Development/Implementation of ML — 20%
- **1st:** implementation of **a number of suitable ML approaches**; justification for their selection **underpinned by relevant literature/experimentation**; **full understanding of deployment/implementation**.
- **High 1st:** …**full** justification, **no weaknesses**.

### Critical Evaluation of Performance — 20%
- **1st:** evaluation is **deep and elegant**, includes **all aspects of performance** deeply and extensively; reviews results relative to the problem with **extensive, specific** considerations of further work.
- **High 1st:** …*plus* results demonstrate the **innovative solution has advanced compared to related works**.

### Understanding of Theoretical Knowledge — 10%
- **1st:** full knowledge; answers all questions **with explanations and elaboration**.
- **High 1st:** …*plus* uses that knowledge to **identify opportunities for innovation/creativity**.

### Demonstration of Solution — 10%
- **1st:** demo showcases **all aspects** of how the solution functions; **extensive understanding of the limitations and how it may be improved**.
- **High 1st:** …showcasing all aspects **in a creative way**.

### Organisation and Coherence — 10%
- **1st:** clear, concise, **professional** presentation incorporating **a range of dynamic content**; logical, interesting sequence.
- **High 1st:** …**creative** dynamic content.

---

## 5. What this means operationally

Five behaviours separate a 2:1 from a 1st here. Build them in from day one:

1. **Every claim carries a citation or a measurement.** "Low latency" is a 2:1. "3.2 ms mean inference, n=500, measured with `micros()` — 40× faster than the 130 ms round-trip we measured against a cloud endpoint" is a 1st.
2. **Compare, don't just report.** The rubric says *compare different methodological approaches* twice. Four models, one table, one honest winner.
3. **Online evaluation is explicitly required** and is the single most commonly missing piece. A live on-device confusion matrix is the differentiator.
4. **Name the limitations before the marker does.** The Demo row rewards *extensive understanding of the limitations*. A slide that says "here is where it fails and why" scores higher than one that hides it.
5. **Innovation must be stated, not implied.** Say the words: what is novel here versus the related work in `09-REFERENCES.md`.

---

## 6. Reconciliation with CW1 — what was promised

Everything CW1 promised is carried forward unchanged. Three items need attention:

| CW1 promise | CW2 status |
|---|---|
| Six setup conditions | ⚠️ **amended to five** — `tilt_off` cut 7 Aug, see `docs/12-SCOPE-CHANGE-TILT.md`. Declare it on the data slide; do not let a marker find it |
| 250+ labelled samples per class | ✅ carried as the P3 gate |
| Three ML paradigms compared head-to-head | ✅ carried, **plus** a classical baseline (M0) |
| INT8-quantised TFLite Micro on the Nano | ✅ carried |
| Accuracy · F1 · latency · memory footprint | ✅ carried, extended with online metrics |
| OLED + LED + **buzzer** output | ⚠️ buzzer not in the physical build — see `AGENTS.md` §11.4 |
| LDR uncalibrated (named as an open issue in CW1) | ⚠️ **must be closed in CW2** — calibration curve is a P1 gate. CW1 flagged it; CW2 must show it resolved. Markers remember. |
| No SDG stated | 🔴 **gap** — added in CW2, see `08-ETHICS-SDG.md` |
| Results slide placeholders `[ADD %]` | 🔴 must be filled with real measurements |
