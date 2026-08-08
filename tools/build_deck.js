/**
 * LightGuide Edge - CW2 presentation deck
 * COM683 CW2 | Vishnu Vekariya | Ulster University
 *
 *   node tools/build_deck.js
 *
 * Builds deliverables/VekariyaVishnuB00969091_Slides.pptx from docs/06-PRESENTATION-PLAN.md.
 * Every number on a slide is copied from a report in reports/ - if a figure changes,
 * regenerate the report first and re-run this, never edit the .pptx by hand.
 *
 * Citations are limited to the four COM683 set texts. docs/09-REFERENCES.md marks
 * everything else as an unread search task, and an unread citation is worse than none
 * in a viva.
 */

const pptxgen = require("pptxgenjs");
const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const FIG = path.join(ROOT, "reports", "figures");
const RIG = path.join(FIG, "rig");
const OUT = path.join(ROOT, "deliverables", "VekariyaVishnuB00969091_Slides.pptx");

// Warm amber is the lamp; teal is the sensor/data side. The deck is about the
// coupling between those two things, so the palette carries it.
const INK = "141A2E";
const PAPER = "FFFFFF";
const SOFT = "F3F5F9";
const AMBER = "E08A2E";
const TEAL = "1F6F7A";
const MUTED = "6E7687";
const RED = "B3453B";

const HEAD = "Cambria";
const BODY = "Calibri";

const pres = new pptxgen();
pres.layout = "LAYOUT_WIDE";           // 13.333 x 7.5 - must precede addSlide
pres.author = "Vishnu Vekariya";
pres.title = "LightGuide Edge";

const W = 13.333;
const M = 0.6;                          // page margin
const CW = W - 2 * M;                   // content width

/** Photos are optional; the deck must build cleanly on a fresh clone. */
function rig(name) {
  const p = path.join(RIG, name);
  return fs.existsSync(p) ? p : null;
}
function fig(name) {
  const p = path.join(FIG, name);
  if (!fs.existsSync(p)) throw new Error(`missing figure: ${p}`);
  return p;
}

function titleOf(s, text, sub) {
  s.addText(text, {
    x: M, y: 0.42, w: CW, h: 0.62, fontFace: HEAD, fontSize: 34, bold: true,
    color: INK, margin: 0,
  });
  if (sub) {
    s.addText(sub, {
      x: M, y: 1.04, w: CW, h: 0.34, fontFace: BODY, fontSize: 14,
      color: MUTED, margin: 0,
    });
  }
}

/** Big number + label. The deck's repeated motif. */
function stat(s, x, y, w, value, label, tone) {
  // 40 pt Cambria digits overflow a 1.75" box at five characters and wrap mid
  // number ("0.9975" -> "0.997" / "5"), so the size follows the string length.
  const size = value.length <= 4 ? 34 : value.length <= 6 ? 27 : 22;
  s.addText(value, {
    x, y, w, h: 0.62, fontFace: HEAD, fontSize: size, bold: true,
    color: tone || TEAL, align: "center", margin: 0, wrap: false,
  });
  s.addText(label, {
    x, y: y + 0.6, w, h: 0.62, fontFace: BODY, fontSize: 11,
    color: MUTED, align: "center", margin: 0, valign: "top",
  });
}

function card(s, x, y, w, h, heading, body, tone) {
  s.addShape(pres.ShapeType.roundRect, {
    x, y, w, h, rectRadius: 0.08, fill: { color: SOFT },
    line: { color: SOFT, width: 0 },
  });
  s.addText(heading, {
    x: x + 0.22, y: y + 0.16, w: w - 0.44, h: 0.34, fontFace: BODY, fontSize: 14,
    bold: true, color: tone || INK, margin: 0,
  });
  s.addText(body, {
    x: x + 0.22, y: y + 0.52, w: w - 0.44, h: h - 0.7, fontFace: BODY, fontSize: 12,
    color: INK, margin: 0, valign: "top",
  });
}

function bullets(s, x, y, w, h, items, size) {
  s.addText(
    items.map((t, i) => ({
      text: t, options: { bullet: true, breakLine: i !== items.length - 1 },
    })),
    {
      x, y, w, h, fontFace: BODY, fontSize: size || 14, color: INK,
      margin: 0, paraSpaceAfter: 8, valign: "top",
    }
  );
}

function caption(s, x, y, w, text) {
  s.addText(text, {
    x, y, w, h: 0.34, fontFace: BODY, fontSize: 10, italic: true,
    color: MUTED, margin: 0,
  });
}

/* ------------------------------------------------------------------ 1 title */
{
  const s = pres.addSlide();
  s.background = { color: INK };
  s.addText("LightGuide Edge", {
    x: M, y: 2.25, w: CW, h: 0.95, fontFace: HEAD, fontSize: 52, bold: true,
    color: PAPER, margin: 0,
  });
  s.addText("A camera-free device that tells a photographer how to fix their lighting — in 21 microseconds, on a microcontroller.", {
    x: M, y: 3.25, w: 9.6, h: 0.7, fontFace: BODY, fontSize: 17, color: AMBER, margin: 0,
  });
  s.addText("Vishnu Vekariya  ·  B00969091  ·  COM683 Edge and Embedded Intelligence  ·  CW2", {
    x: M, y: 4.55, w: CW, h: 0.4, fontFace: BODY, fontSize: 13, color: "9AA3B2", margin: 0,
  });
  s.addNotes("Open with the problem in one sentence, not an agenda. 12 minutes, 15 slides.");
}

/* ---------------------------------------------------------------- 2 problem */
{
  const s = pres.addSlide();
  titleOf(s, "The problem", "Small-studio and freelance photographers have no feedback loop on lighting setup");
  bullets(s, M, 1.68, 6.5, 4.1, [
    "Lighting is set by eye, then re-set by eye at every session. Nothing records what \"correct\" was.",
    "Getting it wrong costs a reshoot: wasted travel, wasted studio hire, wasted energy — SDG 12, responsible consumption.",
    "The people worst affected are freelancers and small businesses without an assistant or a light meter — SDG 8, decent work and productive employment.",
    "Existing aids are cameras or phone apps: they need a lens pointed at the subject, which is a privacy problem in client work.",
  ]);
  card(s, 7.4, 1.68, 5.33, 2.35,
    "The opportunity for innovation",
    "A sub-£30 device that senses geometry and light directly, judges the setup against a saved reference, and guides the operator back — with no camera, no cloud and no personal data.",
    AMBER);
  stat(s, 7.4, 4.35, 1.6, "£30", "bill of materials");
  stat(s, 9.2, 4.35, 1.6, "0", "images captured");
  stat(s, 11.0, 4.35, 1.7, "5", "setup faults detected");
  caption(s, 7.4, 5.75, 5.33, "Cost and privacy are the two barriers this removes.");
  s.addNotes("Say the phrase 'opportunity for innovation' out loud — the top band language is explicit. Ground SDG 12 in reshoots, SDG 8 in freelancers without an assistant.");
}

/* --------------------------------------------------------------- 3 solution */
{
  const s = pres.addSlide();
  titleOf(s, "The solution, and why machine learning earns its place", "Sense → classify → guide, as a closed loop on the device");

  const steps = [
    ["1  Sense", "Ultrasonic range and an LDR, sampled at 8 Hz. The LDR is read inside the ultrasonic ping gaps so the two channels never contend."],
    ["2  Classify", "A window of 10 samples becomes 8 features, classified on-device into one of five setup states."],
    ["3  Guide", "OLED text, an LED and a buzzer whose pitch encodes direction and rate encodes urgency."],
  ];
  steps.forEach((st, i) => card(s, M + i * 4.15, 1.68, 3.9, 1.9, st[0], st[1], TEAL));

  card(s, M, 3.86, 8.1, 3.05,
    "Why not a pair of thresholds?",
    "Because the two channels are physically coupled. Illuminance falls with the square of distance, so moving the stand changes the light reading even when the lamp has not been touched. A fixed light threshold therefore fires on a distance error, and a fixed distance threshold misses a lighting one. The classifier learns the joint region instead — and the ablation on slide 12 measures exactly what that coupling is worth: 0.41 and 0.46 macro-F1 for each channel alone, 0.87 together.",
    AMBER);
  card(s, 8.95, 3.86, 3.78, 3.05,
    "Placed at the edge, deliberately",
    "Inference is 21 µs against a 125 ms sensing period, so the loop is bound by physics, not compute. Nothing leaves the device — the privacy argument and the latency argument point the same way here (Buyya and Srirama, 2019; Situnayake and Plunkett, 2022).",
    TEAL);
  s.addNotes("The inverse-square coupling is the intellectual core of the project. If you only get one idea across, make it this one.");
}

/* ---------------------------------------------------------------- 4 context */
{
  const s = pres.addSlide();
  titleOf(s, "Ethical, technical and social context", "What the design chooses not to do is as important as what it does");
  const items = [
    ["Ethical — privacy by construction", "No camera, no microphone, no images, no personal data. The device cannot identify a person because it never senses one: it measures distance to a surface and light falling on a cell. Privacy is a property of the sensor choice, not of a policy.", TEAL],
    ["Technical — the constraint is RAM", "46.7 KB of 256 KB static RAM and 12.3% of flash. On microcontrollers the binding limit is memory, not arithmetic (Warden and Situnayake, 2019), which is why an 8-feature tree beats a neural network here.", INK],
    ["Social — who it is for", "Aimed at freelancers and small studios, the users least able to absorb a reshoot. A sub-£30 device with no subscription and no account is accessible in a way a metered app is not.", AMBER],
  ];
  items.forEach((it, i) => card(s, M, 1.7 + i * 1.78, CW, 1.62, it[0], it[1], it[2]));
  s.addNotes("Trim this slide first if running over time.");
}

/* ------------------------------------------------------------------ 5 data */
{
  const s = pres.addSlide();
  titleOf(s, "Data collection methodology", "1,832 labelled samples · 45 runs · 3 sessions · 5 classes · 0 dropouts, 0 NaNs");

  const photo = rig("rig_overview.jpg") || rig("rig_overview.png");
  const textW = photo ? 7.4 : CW;

  bullets(s, M, 1.68, textW, 3.05, [
    "Five classes: optimal, too_close, too_far, underlit, overlit. Each defined relative to a saved reference setup, not to an absolute distance — so a reference saved next week still works.",
    "Three sessions on different days, with the rig re-referenced each time. Session 3 is held out entirely and never seen during training.",
    "8 Hz sampling: fast enough to catch the operator settling, slow enough that the ultrasonic ping and the LDR settling do not contend.",
    "The staged label is written by the host at the moment of the trial, so ground truth is recorded, not reconstructed from memory afterwards.",
  ], 13);

  card(s, M, 4.9, textW, 2.02,
    "Why session-split, not random",
    "At 8 Hz consecutive samples are near-duplicates. A random split puts near-identical windows on both sides and reports a score that measures memorisation. Splitting by session — and grouping cross-validation by run — makes the test set answer the question that matters: does this work on a day it has not seen?",
    RED);

  if (photo) {
    s.addImage({ path: photo, x: 8.25, y: 1.68, w: 4.48, h: 4.1, sizing: { type: "contain", w: 4.48, h: 4.1 } });
    caption(s, 8.25, 5.88, 4.48, "The rig as used for collection: lamp and sensor board on one stand.");
  }
  s.addNotes("The session-split argument is the highest-value point on the 15% data row. Say it explicitly.");
}

/* -------------------------------------------------------------- 6 controls */
{
  const s = pres.addSlide();
  titleOf(s, "Data quality controls", "Calibration against an independent reference, then filtering, gating and audit");

  s.addImage({ path: fig("calibration_ldr.png"), x: M, y: 1.62, w: 7.3, h: 2.55, sizing: { type: "contain", w: 7.3, h: 2.55 } });
  caption(s, M, 4.24, 7.3, "Six-point sweep, 40–200 cm, lamp output held fixed; distance is the reference instrument via the inverse-square law.");

  stat(s, M, 4.78, 1.8, "0.623", "LDR γ (CdS band 0.5–0.9)");
  stat(s, 2.45, 4.78, 1.8, "0.9975", "R² of the power-law fit");
  stat(s, 4.35, 4.78, 1.8, "0.9913", "APDS9960 cross-check r");
  stat(s, 6.25, 4.78, 1.8, "7.2 mm", "ultrasonic RMSE ≤160 cm");

  card(s, 8.15, 1.62, 4.58, 2.55,
    "Ambient is fitted, not assumed",
    "A lamp-off reading at every distance gives the room's own contribution, subtracted in the resistance domain — counts are non-linear in illuminance, so subtracting counts would subtract the wrong quantity.",
    TEAL);
  card(s, 8.15, 4.42, 4.58, 2.53,
    "A measurement, not a convenience",
    "One point at 200 cm read 134 mm long while every other read 1–22 mm short. The beam cone is 527 mm wide there against a 200 mm target, so the echo came off the wall behind. It is auto-flagged and excluded, and the report shows it rather than hiding it.",
    RED);
  s.addNotes("If asked why no lux meter: absolute lux is not needed because every class is relative to a saved reference. What is needed is the response curve, and distance gives that for free.");
}

/* -------------------------------------------------------------- 7 features */
{
  const s = pres.addSlide();
  titleOf(s, "Preprocessing and features", "Two channels, ten samples, eight numbers");

  card(s, M, 1.68, 3.85, 2.45, "Windowing",
    "10 samples at 8 Hz = 1.25 s, 50% overlap (stride 5). Long enough to average out a bad echo, short enough that the operator is not left waiting.", TEAL);
  card(s, 4.62, 1.68, 3.85, 2.45, "Deviations, not absolutes",
    "Both channels are stored as a difference from the saved reference: d_dist_mm and d_ldr. The model learns how far off the rig is, not where it happens to be standing.", AMBER);
  card(s, 8.63, 1.68, 4.1, 2.45, "Eight features",
    "mean, std, min, max — for each of the two channels. Chosen to be computable in a single pass on-device with no buffering beyond the window itself.", TEAL);

  card(s, M, 4.42, 6.05, 2.52, "Leakage avoided at every step",
    "The scaler is fitted on training data only and frozen before it ever touches the test session. Cross-validation is grouped by run, so no two windows from the same run can land on opposite sides of a fold.", RED);
  card(s, 6.87, 4.42, 5.86, 2.52, "Readings the model was never trained on are refused",
    "A failed echo was dropped from the dataset rather than interpolated, so the firmware also refuses to push one into the classifier. The device shows the channel as unread instead of inventing a value — the training and deployment paths make the same choice.", INK);
  s.addNotes("The frozen-scaler and grouped-CV points are what separates a 2:1 from a 1st on the ML row. State both.");
}

/* ------------------------------------------------------------- 8 approaches */
{
  const s = pres.addSlide();
  titleOf(s, "Four modelling approaches, and why each", "Nothing was tuned to flatter one side");
  const rows = [
    ["M0  Classical baselines", "Decision tree, k-NN, logistic regression, random forest. A tree is interpretable and its split thresholds can be checked against the physical tolerances — when they agree, that is independent evidence the dataset is sane.", TEAL],
    ["M1  MLP (32-16)", "The obvious neural baseline on the same 8 features. Tests whether a learned non-linear boundary beats an axis-aligned one on this problem.", INK],
    ["M2  Autoencoder novelty gate", "Trained on optimal windows only. High reconstruction error means \"this is not a setup I recognise\", so the device can say UNKNOWN SETUP rather than confidently pick a wrong class.", AMBER],
    ["M3  Sequence model / Edge Impulse", "Same windows, same splits — but 20 raw timestep values instead of 8 summary statistics, trained as an INT8 TFLM network in Edge Impulse.", TEAL],
  ];
  rows.forEach((r, i) => card(s, M + (i % 2) * 6.35, 1.68 + Math.floor(i / 2) * 2.72, 6.05, 2.52, r[0], r[1], r[2]));
  s.addNotes("M2 is the closest thing here to a novelty claim — but its recall is 0.169, so present it as a partial result, not a success.");
}

/* ------------------------------------------------------------ 9 validation */
{
  const s = pres.addSlide();
  titleOf(s, "Validation and deployment", "Grouped 5-fold CV, a held-out session, and a footprint measured rather than estimated");

  bullets(s, M, 1.68, 6.5, 2.85, [
    "5-fold cross-validation grouped by run; 195 training windows, 104 held-out test windows from session 3.",
    "Five seeds per model — the reported spread is real variance, not a single lucky run.",
    "Deployed model is the decision tree, exported to model.h as plain C: no library dependency, no tensor arena, no quantisation error.",
    "Footprint read from the compiler, not estimated — regenerate with tools/record_footprint.py.",
  ], 13);

  stat(s, M, 4.9, 2.15, "120,896 B", "flash — 12.3% of 983,040");
  stat(s, 2.85, 4.9, 2.15, "46,696 B", "static RAM — 17.8% of 256 KB");
  stat(s, 5.1, 4.9, 2.15, "20.8 µs", "mean inference, p95 21.0");

  card(s, 7.4, 1.68, 5.33, 2.95,
    "The INT8 network exists — and was not deployed",
    "CW1 promised an INT8 TensorFlow Lite Micro model. It was built in Edge Impulse on identical windows (195 train / 104 test on both sides) and measured. The tree beat it by 20 percentage points, so the tree shipped. Choosing not to deploy is a result, not an omission.",
    AMBER);
  card(s, 7.4, 4.9, 5.33, 2.05,
    "Public project",
    "studio.edgeimpulse.com/public/1082649/live — Vish_Vekzz / LightGuide-Edge, open without a login.",
    TEAL);
  s.addNotes("Have the EI page open in a tab in case they want to see it during Q&A.");
}

/* -------------------------------------------------------- 10 offline results */
{
  const s = pres.addSlide();
  titleOf(s, "Results — offline", "Held-out session 3, never seen in training");

  s.addTable([
    [{ text: "model", options: { bold: true } }, { text: "CV macro-F1", options: { bold: true } }, { text: "test macro-F1", options: { bold: true } }],
    ["M0a Decision Tree", "0.882 ± 0.237", "0.868"],
    ["M0d Random Forest", "0.896 ± 0.207", "0.868"],
    ["M1  MLP (32-16)", "0.858 ± 0.138", "0.860"],
    ["M0c Logistic Regression", "0.849 ± 0.148", "0.786"],
    ["M0b k-NN (k=5)", "0.784 ± 0.161", "0.800"],
    ["Majority baseline", "—", "0.067"],
  ], {
    x: M, y: 1.68, w: 6.3, colW: [2.9, 1.85, 1.55], fontFace: BODY, fontSize: 13,
    border: { type: "solid", color: "DCE1E9", pt: 1 }, align: "left",
    fill: { color: PAPER }, rowH: 0.42,
  });

  card(s, M, 5.0, 6.3, 1.95,
    "The generalisation gap is the point",
    "CV 0.882 against held-out 0.868 — a gap of +0.014. Session-wise splitting exists to expose exactly this number; a random split would have hidden it and reported something flattering and false.",
    RED);

  s.addImage({ path: fig("confusion_m0a.png"), x: 7.15, y: 1.68, w: 5.58, h: 4.05, sizing: { type: "contain", w: 5.58, h: 4.05 } });
  caption(s, 7.15, 5.85, 5.58, "Every error is a distance class absorbed into optimal: too_close recall 0.67, too_far 0.65, both channels' lighting classes 1.00.");
  s.addNotes("Optimal has precision 0.60 with recall 1.00 — it over-predicts. That is the near-boundary region, and it sets up slide 11.");
}

/* --------------------------------------------------------- 11 online results */
{
  const s = pres.addSlide();
  titleOf(s, "Results — online, on the device, in the room", "40 staged runs · 1,648 classified samples · 0 dropouts");

  stat(s, M, 1.62, 1.95, "1.000", "per-run macro-F1", TEAL);
  stat(s, 2.65, 1.62, 1.95, "20.8 µs", "mean inference");
  stat(s, 4.7, 1.62, 1.95, "8.09 Hz", "sustained sample rate");

  card(s, M, 3.35, 6.0, 3.6,
    "Why 1.000 is NOT better than the offline 0.868",
    "The two evaluations sample different regions. Online, every off-reference run was staged against a floor mark ~300 mm out, with a within-class spread of 2 mm. The held-out session contains windows only 33–38 mm past a 30 mm tolerance — the boundary region, which is precisely where slide 10's errors live. The online score is a property of the protocol, not evidence the device improved.",
    RED);

  s.addImage({ path: fig("confusion_online.png"), x: 6.95, y: 1.62, w: 5.78, h: 4.0, sizing: { type: "contain", w: 5.78, h: 4.0 } });
  caption(s, 6.95, 5.75, 5.78, "What it does establish: the deployed model reproduces its intended behaviour on every prototypical staging, with ground truth recorded at the trial.");
  s.addNotes("Do not oversell this. Volunteering the caveat before you are asked is worth more than the 1.000 itself. The honest next measurement is a boundary sweep at +4, +6, +10 cm.");
}

/* -------------------------------------------------- 12 ablation / failure */
{
  const s = pres.addSlide();
  titleOf(s, "Ablation, failure analysis and limitations", "What each sensor is worth, and where the device is known to break");

  s.addImage({ path: fig("ablation.png"), x: M, y: 1.62, w: 5.6, h: 3.4, sizing: { type: "contain", w: 5.6, h: 3.4 } });
  caption(s, M, 5.12, 5.6, "Distance alone 0.410, light alone 0.461, both 0.868 — the coupling from slide 3, measured.");

  const fails = [
    ["Near the tolerance boundary", "too_close and too_far fall into optimal at 0.67 and 0.65 recall. Errors are concentrated within tens of millimetres of the band edge.", RED],
    ["The novelty gate under-fires", "M2 precision 1.000 but recall 0.169 — when it says UNKNOWN it is right, but it stays silent on most genuine deviations. Reported as a partial result.", AMBER],
    ["Ultrasonic beyond ~160 cm", "With a 200 mm target the 527 mm beam cone picks up the background. Measured, not assumed.", INK],
    ["tilt_off never collected", "Scope was cut to five classes; documented in docs/12-SCOPE-CHANGE-TILT.md rather than quietly dropped.", MUTED],
  ];
  fails.forEach((f, i) => card(s, 6.4, 1.62 + i * 1.34, 6.33, 1.22, f[0], f[1], f[2]));
  s.addNotes("Volunteer all four. A limitation you raise yourself reads as rigour; the same one raised by the marker reads as an oversight.");
}

/* ------------------------------------------------------------------ 13 demo */
{
  const s = pres.addSlide();
  s.background = { color: INK };
  s.addText("Demo", {
    x: M, y: 0.5, w: CW, h: 0.8, fontFace: HEAD, fontSize: 38, bold: true, color: PAPER, margin: 0,
  });
  s.addText("Live device — video fallback ready", {
    x: M, y: 1.2, w: CW, h: 0.4, fontFace: BODY, fontSize: 15, color: AMBER, margin: 0,
  });

  const seq = [
    ["0:00", "Hold D7 for 2 s", "REFERENCE SAVED"],
    ["0:08", "Settled at the reference", "OPTIMAL"],
    ["0:16", "Stand ~30 cm closer", "TOO CLOSE — MOVE BACK"],
    ["0:26", "Stand back past the reference", "TOO FAR — MOVE IN"],
    ["0:36", "Return, then dim the lamp", "UNDERLIT — INCREASE OUTPUT"],
    ["0:46", "Brighten above the reference", "OVERLIT — REDUCE OUTPUT"],
    ["0:54", "Restore the reference setup", "OPTIMAL"],
  ];
  seq.forEach((r, i) => {
    const y = 2.15 + i * 0.66;
    s.addText(r[0], { x: M, y, w: 0.75, h: 0.42, fontFace: BODY, fontSize: 13, color: MUTED, margin: 0 });
    s.addText(r[1], { x: 1.4, y, w: 5.1, h: 0.42, fontFace: BODY, fontSize: 13, color: PAPER, margin: 0 });
    s.addText(r[2], { x: 6.7, y, w: 6.0, h: 0.42, fontFace: BODY, fontSize: 13, bold: true, color: AMBER, margin: 0 });
  });
  s.addText("All five classes in under 60 seconds, hardware visibly responding to a physical change.", {
    x: M, y: 6.72, w: CW, h: 0.4, fontFace: BODY, fontSize: 12, italic: true, color: "9AA3B2", margin: 0,
  });
  s.addNotes("UNKNOWN SETUP only if it fires in the first two attempts — M2 recall is 0.169. Promising it and having it not fire is worse than not promising it. State the boundary limitation out loud during the demo.");
}

/* ------------------------------------------------------------ 14 conclusion */
{
  const s = pres.addSlide();
  s.background = { color: INK };
  s.addText("Conclusion", {
    x: M, y: 0.55, w: CW, h: 0.8, fontFace: HEAD, fontSize: 38, bold: true, color: PAPER, margin: 0,
  });
  s.addText("A £30 camera-free device that classifies five lighting-setup faults on-device at 20.8 µs, held-out macro-F1 0.868, using 12.3% of flash.", {
    x: M, y: 1.35, w: 11.0, h: 0.75, fontFace: BODY, fontSize: 16, color: AMBER, margin: 0,
  });

  const fw = [
    ["Boundary sweep", "Stage too_far at +4, +6 and +10 cm instead of +30 and measure where on-device accuracy actually breaks. The offline test cannot answer this — it depends on settling behaviour a CSV replay never sees."],
    ["Fix the novelty gate", "M2 recall 0.169 is too low to ship. Either lower the threshold and re-measure precision, or replace the autoencoder with a distance-to-training-manifold test."],
    ["Widen the ultrasonic target", "A larger or textured reference surface should extend usable range past the measured 160 cm limit."],
    ["Collect tilt_off", "The sixth class was cut for scope. The IMU is already on the board and already read."],
  ];
  fw.forEach((f, i) => {
    const x = M + (i % 2) * 6.35;
    const y = 2.5 + Math.floor(i / 2) * 2.0;
    s.addShape(pres.ShapeType.roundRect, {
      x, y, w: 6.05, h: 1.78, rectRadius: 0.08,
      fill: { color: "1E2540" }, line: { color: "1E2540", width: 0 },
    });
    s.addText(f[0], { x: x + 0.22, y: y + 0.14, w: 5.6, h: 0.3, fontFace: BODY, fontSize: 13, bold: true, color: AMBER, margin: 0 });
    s.addText(f[1], { x: x + 0.22, y: y + 0.46, w: 5.6, h: 1.18, fontFace: BODY, fontSize: 11, color: "C9D0DD", margin: 0 });
  });
  s.addText("Every item above is tied to a number measured in this project, not a wish list.", {
    x: M, y: 6.72, w: CW, h: 0.35, fontFace: BODY, fontSize: 11, italic: true, color: "9AA3B2", margin: 0,
  });
  s.addNotes("Close on the boundary sweep — it shows you know what your own headline number does not prove.");
}

/* ------------------------------------------------------------ 15 references */
{
  const s = pres.addSlide();
  titleOf(s, "References", "Harvard · left on screen during Q&A");
  const refs = [
    "Buyya, R. and Srirama, S.N. (eds.) (2019) Fog and Edge Computing: Principles and Paradigms. Hoboken, NJ: John Wiley & Sons.",
    "Iodice, G.M. (2023) TinyML Cookbook: Combine artificial intelligence and ultra-low-power embedded devices to make the world smarter. 2nd edn. Birmingham: Packt Publishing.",
    "Situnayake, D. and Plunkett, J. (2022) AI at the Edge. Sebastopol, CA: O'Reilly Media.",
    "Warden, P. and Situnayake, D. (2019) TinyML: Machine Learning with TensorFlow Lite on Arduino and Ultra-Low-Power Microcontrollers. Sebastopol, CA: O'Reilly Media.",
  ];
  s.addText(refs.map((t, i) => ({ text: t, options: { breakLine: i !== refs.length - 1 } })), {
    x: M, y: 1.75, w: CW, h: 3.2, fontFace: BODY, fontSize: 14, color: INK,
    margin: 0, paraSpaceAfter: 12, valign: "top",
  });
  card(s, M, 5.15, CW, 1.75,
    "Technical documentation",
    "Arduino Nano 33 BLE Sense documentation · Nordic Semiconductor nRF52840 datasheet · Edge Impulse documentation · TensorFlow Lite for Microcontrollers documentation · HC-SR04 and GL5528 component datasheets.",
    MUTED);
  s.addNotes("Each set text maps to a specific claim: Warden and Situnayake for the RAM-bound argument, Situnayake and Plunkett for online evaluation, Buyya and Srirama for edge placement, Iodice for the Nano workflow. Be ready to say what each supports.");
}

pres.writeFile({ fileName: OUT }).then(() => {
  console.log("wrote", OUT);
  if (!rig("rig_overview.jpg") && !rig("rig_overview.png")) {
    console.log("NOTE: reports/figures/rig/rig_overview.jpg not found - slide 5 built without the rig photo.");
  }
});
