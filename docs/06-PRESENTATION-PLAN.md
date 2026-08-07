# 06 — Presentation plan (Organisation 10% + Demo 10%)

**~12 minutes + 5–10 min Q&A.** 13 content slides + title + references.
Rubric target: "clear, concise, and **professional** presentation that incorporated **a range
of dynamic content**"; High 1st adds **creative** dynamic content.

---

## 1. Slide-by-slide map

| # | Slide | Rubric row | Time | Must contain |
|---|---|---|---|---|
| 1 | Title | — | 0:15 | Project, name, module, one-line hook |
| 2 | **The problem** | Problem 5% | 0:45 | The specific problem, its scale, **SDG 12/8 framing**, and an explicit "opportunity for innovation" line (that phrase unlocks the top band) |
| 3 | **Solution & why it works** | Solution 10% | 1:00 | Sense→classify→guide loop; **inverse-square coupling** as the reason ML beats thresholds; ≥2 literature citations |
| 4 | **Context: ethical / technical / social** | Solution 10% | 0:45 | No images, no audio, no personal data — privacy by construction; energy/SDG; cost and accessibility for freelancers |
| 5 | **Data collection methodology** | Data 15% | 1:00 | 5 classes, session design, **why session-split not random**, 8 Hz justification, photos of the physical rig |
| 6 | **Data quality controls** | Data 15% | 1:00 | Calibration curves (LDR **and** ultrasonic), median/mean filtering, range gating + dropout rate, confounded samples, class balance plot |
| 7 | **ML: preprocessing & features** | ML 20% | 0:45 | Windowing (10 samples, 50% overlap), **8 features** (mean/std/min/max x 2 channels), deviations not absolutes, scaler frozen on train only (leakage avoided) |
| 8 | **ML: four approaches** | ML 20% | 1:15 | M0 baseline / M1 MLP / M2 autoencoder / M3 sequence — and *why each* |
| 9 | **ML: validation & deployment** | ML 20% | 1:00 | 5-fold grouped CV + held-out session; INT8 quantisation; tensor arena; footprint |
| 10 | **Results: offline** | Eval 20% | 1:15 | Comparison table, confusion matrices, CV↔held-out gap **explained** |
| 11 | **Results: online** | Eval 20% | 1:15 | Live confusion matrix vs offline, latency (inference **and** end-to-end), RAM/flash, throughput |
| 12 | **Ablation, failure analysis & related work** | Eval 20% | 1:00 | Sensor ablation, failure photos, comparison-to-literature table |
| 13 | **Demo** | Demo 10% | 1:30 | Live device + video fallback; show `UNKNOWN SETUP` **only if it fires reliably** (M2 recall is 0.169 - see section 3); state limitations out loud |
| 14 | Conclusion & future work | Eval/Solution | 0:45 | Specific future work tied to measured limitations |
| 15 | References | — | — | Harvard, on screen during Q&A |

**Total ≈ 12:15.** Trim slide 4 and slide 12 first if over.

---

## 2. "Dynamic content" — what actually counts

The rubric rewards a *range* of dynamic content, and creative content at the top band. Cheap
and effective, in rough order of impact per minute spent:

1. **Live hardware demo** — the single highest-value item. Bring the rig.
2. **Split-screen demo video** as the fallback (and a required deliverable anyway).
3. **Animated build-up of the confusion matrix** — offline, then online overlaid.
4. **Photographs of each physical class condition** next to its sensor trace. Ties the
   abstract labels to physical reality and takes minutes to assemble.
5. **The decision tree figure** — an interpretable model as a visual is unusual in these decks.
6. **A short "what happens when it fails" clip** — the `too_far` trace oscillating at long
   range while every other class sits flat (`reports/figures/traces_by_class.png`). It is a
   real measured failure mode and it explains the 0.65 `too_far` recall on the same slide.

Avoid: walls of bullets, unlabelled axes, screenshots of code that nobody can read, and any
slide with a number on it that you cannot source when asked.

---

## 3. Demo script (~1 min video, and the live version)

Split-screen: hardware left, OLED close-up or serial output right.

| t | Action | On screen |
|---|---|---|
| 0:00 | Hold D7 for 2 s to save the reference | `REFERENCE SAVED` |
| 0:08 | Device settled at the reference | `OPTIMAL` |
| 0:16 | Push the stand ~30 cm closer | `TOO CLOSE — MOVE BACK` |
| 0:26 | Pull the stand back past the reference | `TOO FAR — MOVE IN` |
| 0:36 | Return to reference, then dim the lamp | `UNDERLIT — INCREASE OUTPUT` |
| 0:46 | Brighten the lamp above reference | `OVERLIT — REDUCE OUTPUT` |
| 0:54 | Restore the reference setup | `OPTIMAL` |

Under 60 seconds, shows **all five classes**, and the hardware is visibly responding to a
physical stimulus — which is exactly what the handbook specifies for the video component.

**Shoot it so the OLED is legible.** That is the whole deliverable: split-screen, hardware
on one side, OLED close-up on the other. If only one thing is in focus, make it the OLED.

The `UNKNOWN SETUP` novelty case is worth showing **if it triggers reliably on the day** —
walk a hand in front of the sensor. Do not spend the shoot fighting it: M2's recall is 0.169
(`reports/offline_results.md`), so it fires on well under half of genuine deviations. If it
does not appear in two takes, drop it from the video and describe it on the slide instead.
Promising it on screen and having it not fire is worse than not promising it.

**Record this on Day 7 even if the live demo is expected to work.** Equipment fails in
presentation rooms.

---

## 4. Delivery

- **Rehearse twice, timed.** Overrunning is the most common avoidable loss on the
  Organisation row.
- Speaker notes per slide, but do not read them.
- Open with the problem in one sentence, not with an agenda slide.
- Say the words "opportunity for innovation" and "novel" where they are true — the top band
  language is explicit about it, and markers tick against the words they are given.
- Have `10-QA-DEFENCE.md` drilled. The theory row is 10% and it is decided entirely in Q&A.
- Bring: the rig, a USB cable, a laptop with the deck **and** the video offline, a phone
  hotspot, and the PDF export in case PowerPoint misbehaves.

---

## 5. Deck build rules

- 16:9, dark title and conclusion slides, light content slides.
- Body text ≥16 pt. If it does not fit at 16 pt, it belongs in the speaker notes.
- Every figure gets a caption stating what to look at, not just what it is.
- Every claim gets a citation `(Author, Year)` or a measurement `(n=120, mean 3.2 ms)`.
- Replace **every** `[ADD %]` placeholder inherited from the CW1 deck. A placeholder left on
  a slide in the defence is the single most damaging thing that can appear on screen.
