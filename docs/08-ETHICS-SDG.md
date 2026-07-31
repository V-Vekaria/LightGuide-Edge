# 08 — SDG alignment and ethical / social / technical context

Two rubric rows depend on this document: **Description of Solution (10%)**, which requires
sensitivity to "ethical, technical, **and** social dimensions", and **Problem Overview (5%)**,
where the top band wants "opportunities for innovation … highlighted".

The handbook also states the solution "should be aligned to address challenges within one of
the 17 UN Sustainable Development Goals". **CW1 did not state an SDG.** This is the single
clearest gap between the CW1 deck and the assessment brief, and closing it is cheap.

---

## 1. SDG alignment

### Primary — SDG 12: Responsible Consumption and Production

The link is energy and equipment lifetime, and it is real rather than decorative:

- Studio lighting is high-draw. Continuous LED/tungsten heads commonly run 150–1000 W, and
  modelling lamps on strobes run continuously through a session. Every additional test-shot
  cycle spent hunting for a setup keeps that load on for longer.
- Re-shoots caused by inconsistent lighting multiply the whole session's footprint — lights,
  air conditioning, travel to and from the studio, and the studio's booked hours.
- Repeated repositioning wears stands, modifiers and lamp elements; flash tubes have a finite
  rated flash count and every wasted test frame spends some of it.

**Target 12.2** (sustainable management and efficient use of natural resources) and
**target 12.5** (waste reduction through prevention and reuse) both apply. The mechanism is
straightforward: getting the setup right first time reduces the energy and consumables spent
per usable image.

*Be honest about the magnitude.* A single device saves a modest amount of energy. The
argument that carries weight is the **class of intervention** — cheap, low-power sensing that
prevents wasted work — and the fact that the device itself runs on a coin-cell-class power
budget, so it does not spend more than it saves. State it that way and it is defensible.
Overclaiming a small device as a climate solution is exactly the kind of thing that invites a
sharp Q&A question.

### Secondary — SDG 8: Decent Work and Economic Growth (target 8.3)

Target 8.3 covers support for productivity and the growth of micro- and small enterprises.
Freelance photographers and small studios absorb the cost of re-shoots personally — unpaid
hours, lost bookings, and client confidence. A ~£25 device that makes setups repeatable
lowers a real barrier for people without an assistant or a memory of exactly where the stand
was last Tuesday.

---

## 2. Ethical dimensions

**Privacy by construction.** The device captures distance, light level and orientation. No
images, no audio, no personal data — ever, by design, not by policy. The obvious alternative
solution to this problem is a camera-based one, and a camera in a working studio raises
immediate issues: client work under NDA, model releases, GDPR obligations around images of
identifiable people, and the storage and transmission of all of it.

Choosing a non-imaging sensor suite is therefore an **ethical design decision, not just a
technical one**, and it should be presented as such. It is also the strongest possible
demonstration of the edge-AI privacy argument: this system cannot leak personal data because
it never acquires any.

**No cloud dependency at inference.** Training used Edge Impulse; deployment does not phone
home. Nothing about a client's shoot leaves the device.

**Transparency about failure.** A confident wrong answer is worse than an admission of
uncertainty — a photographer who trusts a wrong `OPTIMAL` wastes the whole session. The
autoencoder novelty gate (`04-ML-PLAN.md` M2) exists specifically so the device can say
`UNKNOWN SETUP` rather than guess. **Designing for honest uncertainty is an ethical choice
implemented in the architecture**, and it is worth naming as such on the slide.

**Data provenance.** All data was collected by the author on the author's own equipment. No
human subjects, no third-party data. Any shared data would have to be declared during the
data-collection section of the presentation, per the handbook. *(Currently: none shared.)*

---

## 3. Social dimensions

- **Accessibility and cost.** The BOM is roughly £25–35 in off-the-shelf parts. That is
  meaningfully different from a professional light meter (£150–400) and puts repeatable
  lighting within reach of students, hobbyists and photographers in lower-income contexts.
- **Educational value.** Photography students learn lighting by feel and repetition. A device
  that names *what is wrong and by how much* turns implicit craft knowledge into explicit
  feedback — the same argument made for feedback tools in skills training generally.
- **Deskilling — the honest counter-argument.** A tool that tells you where to put the light
  might discourage developing the eye for it. The defensible position is that the device
  targets *reproducing a setup you already designed*, not designing it — it automates the
  tedious half, not the creative half. **Raise this objection yourself before a marker does.**
  Naming the strongest argument against your own work is a first-class move.
- **Accessibility for disabled practitioners.** Explicit numeric feedback and an audible
  alert path support photographers with low vision or limited mobility, for whom repeatedly
  walking to and from a stand to judge by eye is a real cost.

---

## 4. Technical dimensions

Covered in depth in `02-HARDWARE.md` §6 and `05-EVALUATION-PLAN.md`. Summary for the slide:

- 256 KB RAM caps model size — architecture is constrained by the platform, not by taste.
- INT8 quantisation trades a small accuracy loss for a ~4× size reduction.
- The LDR is spectrally non-flat: identical lux from tungsten and LED sources read differently.
- Ultrasonic sensing is unreliable against soft, angled or absorptive surfaces — which is
  most of a photography studio's modifiers.
- Session-to-session generalisation is the hardest technical problem here, which is why the
  test set is a held-out session rather than a random split.

---

## 5. The innovation claim (say this explicitly — the top band requires it)

> Existing lighting tools record *digital* state — colour temperature, dimmer level, channel
> assignments. None guide the *physical* placement of the light itself. LightGuide Edge
> closes that gap with a multi-sensor model running entirely on a £25 microcontroller, and
> pairs a supervised classifier with an unsupervised novelty detector so that the device
> reports honest uncertainty instead of a confident wrong answer — an architecture that is
> uncommon in TinyML deployments at this scale and directly addresses the failure mode that
> would otherwise make the tool untrustworthy in professional use.

Two distinct claims, both defensible, both testable: **the application gap**, and **the
classifier + novelty-gate architecture on a constrained device**.
