# 09 — References and literature plan

Two rubric rows depend on citations: **Description of Solution (10%)** requires justification
"drawing on relevant literature", and **Development/Implementation of ML (20%)** requires
justification "underpinned by relevant literature/experimentation".

> ⚠️ **Do not cite anything you have not opened and read.** Fabricated or half-remembered
> citations are the fastest way to lose credibility in an oral defence, where the marker can
> simply ask what the paper said. Everything below marked 🔎 is a *search task*, not a
> citation. Fill each one in with a real, read source before it goes on a slide.

---

## 1. Confirmed sources (from the COM683 handbook reading list)

These are the module's own set texts. Cite them for TinyML fundamentals — quantisation,
deployment, on-device constraints — and they are safe because they are prescribed reading.

- Iodice, G.M. (2023) *TinyML Cookbook: Combine artificial intelligence and ultra-low-power
  embedded devices to make the world smarter*. 2nd edn. Birmingham: Packt Publishing.
- Warden, P. and Situnayake, D. (2019) *TinyML: Machine Learning with TensorFlow Lite on
  Arduino and Ultra-Low-Power Microcontrollers*. Sebastopol, CA: O'Reilly Media.
- Situnayake, D. and Plunkett, J. (2022) *AI at the Edge*. Sebastopol, CA: O'Reilly Media.
- Buyya, R. and Srirama, S.N. (eds.) (2019) *Fog and Edge Computing: Principles and
  Paradigms*. Hoboken, NJ: John Wiley & Sons.

**Use them for these specific points:**
- Warden & Situnayake — why microcontroller ML is constrained by RAM rather than FLOPs; the TFLite Micro static tensor-arena model.
- Iodice — practical Nano 33 BLE Sense workflows, sensor data pipelines, quantisation in practice.
- Situnayake & Plunkett — deployment engineering and evaluating edge systems in the field (supports the *online* evaluation argument).
- Buyya & Srirama — the edge/fog/cloud placement argument for slide 3.

## 2. Vendor / technical documentation (verify each URL before citing)

- Arduino (n.d.) *Nano 33 BLE Sense* documentation — pinout, 3.3 V logic, on-board sensors.
- Nordic Semiconductor — nRF52840 datasheet: Cortex-M4F, 1 MB flash, 256 KB RAM.
- Edge Impulse documentation — DSP blocks, EON tuner, INT8 quantisation, Arduino library deployment.
- TensorFlow Lite for Microcontrollers documentation — tensor arena, supported ops.
- ARM CMSIS-NN — why quantised inference is fast on Cortex-M.
- Component datasheets: HC-SR04 (note the 5 V logic level — this is the citation behind the `02-HARDWARE.md` §3 safety warning) and the LDR/GL5528.

---

## 3. Literature to find 🔎 — target 10–15 peer-reviewed sources

Search **IEEE Xplore**, **ACM DL**, **ScienceDirect**, **Google Scholar**, and Ulster's
library (`ulster.keylinks.org`). Journals named in the handbook are a good starting point:
*Pervasive and Mobile Computing*, *IEEE Pervasive Computing*, *Personal and Ubiquitous Computing*.

| # | Theme | What you need it to support | Suggested queries |
|---|---|---|---|
| 1 | 🔎 TinyML survey | The state of ML on MCUs; positions the whole project | "TinyML survey", "machine learning microcontrollers review" |
| 2 | 🔎 Quantisation for MCUs | The INT8 accuracy/size trade-off claim | "post-training quantization int8 microcontroller", "quantization aware training edge" |
| 3 | 🔎 Multi-sensor fusion on MCUs | Why 3 sensors beat 1; supports the ablation | "sensor fusion embedded classification low power" |
| 4 | 🔎 Autoencoder anomaly detection on edge devices | **The M2 novelty-gate innovation claim** | "autoencoder anomaly detection microcontroller", "TinyML novelty detection", "open set recognition embedded" |
| 5 | 🔎 Open-set / out-of-distribution rejection | Why a closed-set softmax is unsafe in deployment | "open set recognition", "OOD detection classifier rejection" |
| 6 | 🔎 Ultrasonic ranging accuracy & limitations | The dropout and soft-surface findings | "HC-SR04 accuracy characterisation", "ultrasonic ranging error temperature" |
| 7 | 🔎 Ambient light sensing / LDR calibration | The calibration methodology | "photoresistor calibration lux", "ambient light sensor characterisation" |
| 8 | 🔎 IMU tilt estimation from accelerometer | The pitch/roll derivation | "accelerometer tilt estimation", "inclination sensing MEMS" |
| 9 | 🔎 Sensor data collection methodology / labelling quality | **The 15% data row** — cite a methodology, don't invent one | "sensor dataset collection protocol labelling quality", "activity recognition data collection methodology" |
| 10 | 🔎 Subject-wise vs random splitting / data leakage | **The session-split justification** — high value, this argument is well documented in HAR literature | "subject-wise cross validation leakage", "random split overestimates accuracy sensor data" |
| 11 | 🔎 Latency / energy benchmarking of edge ML | The online evaluation metrics | "benchmarking inference latency microcontroller", "energy TinyML benchmark", "MLPerf Tiny" |
| 12 | 🔎 Edge vs cloud trade-off | Slide 3's placement argument | "edge computing latency privacy trade-off" |
| 13 | 🔎 Adjacent applied system on comparable hardware | **The related-work comparison table** in `05-EVALUATION-PLAN.md` §3 | "Arduino Nano 33 BLE Sense classification", "nRF52840 TinyML application" |
| 14 | 🔎 Privacy of non-imaging sensing | The ethics argument | "privacy preserving sensing non-visual", "camera-free monitoring privacy" |
| 15 | 🔎 Studio lighting / photometry basics | The inverse-square law justification | inverse-square law; illuminance and distance (a photometry textbook is fine here) |

**Item 10 is the highest-value single citation in this list.** The human-activity-recognition
literature has documented the random-split leakage problem extensively, and citing it turns
"I split by session" from a personal preference into a methodologically-grounded decision —
which is precisely the difference between the 2:1 and 1st descriptors on the data row.

**Item 4 carries the innovation claim** and must be a real, read paper. If the search shows
this combination *is* common practice, adjust the claim honestly rather than defending it.

---

## 4. Referencing standard

**Harvard**, per the handbook. Consistency matters more than any individual entry.

- In-text: `(Warden and Situnayake, 2019)` · three or more authors: `(Iodice et al., 2023)`
- Reference list alphabetical by surname, on the final slide, left visible during Q&A.
- Web sources need an access date.
- Keep a running BibTeX/CSV in `docs/refs.bib` as you read, not at the end.

## 5. Working rule

Every citation must be **read, summarised in one sentence, and mapped to the specific claim
it supports.** Keep that mapping in this file. If a marker asks "what did that paper
actually find?", the answer needs to be immediate — it is 10% of the mark.

| Ref | One-line finding | Claim it supports | Read? |
|---|---|---|---|
| *(fill in as you read)* | | | ☐ |
