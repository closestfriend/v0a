# Forensic DSP Reconstruction of Antares Auto-Tune EFX 3 (2014): A Clean-Room Specification for an Aggressive/"Sticky" Vocal Pitch Corrector

## TL;DR

- **The engine to clone is the US5973252A pipeline** — autocorrelation period detection → nearest-scale-note target → one-pole smoothing of the period ratio (Retune Speed = the smoothing time constant, 0 ms = instantaneous replacement) → resample with integer cycle repeat/delete — with **no formant preservation**. Antares' own announcement copy states EFX 3 "uses the same core technology as Auto-Tune 7" (Sweetwater InSync), and Hildebrand confirmed by email that the shipping product "run[s] a simple rate converter… repeat a cycle (going sharper) or delete a cycle (going flatter)" and that "PSOLA is NOT used in Auto-Tune."
- **The "sticky/hard" character comes from three stacked causes**, not one knob: hard step-function quantization to the nearest enabled scale note (boundary ≈ 50 cents), the resample-only shifter that drags formants with pitch (no throat/formant model in EFX-class products), and near-zero smoothing at low Retune Speed. Community A/B reports of EFX 3 "note holding" and being "different" cluster around exactly these three.
- **Build it in the time domain with pitch-synchronous processing at ~one-to-a-few periods of latency.** Do not use a phase vocoder, long-window YIN, or a formant-preserving library as-is — those make the result "too clean/too modern." Expose developer controls (periodicity threshold, note-switch hysteresis/hold, octave-jump resistance, smoothing shape, cycle-crossfade length) so the aesthetic can be tuned by ear against historical recordings.

## 1. Executive conclusion

The characteristic aggressive/rigid/"sticky" sound of Auto-Tune EFX 3 is best reproduced by an **autocorrelation-based period tracker feeding a period-synchronous, resampling-with-cycle-repeat/delete pitch shifter**, driven by a **nearest-scale-note quantizer** and a **first-order (one-pole) smoothing of the resample-rate ratio** whose time constant is set by Retune Speed — with **no formant/spectral-envelope preservation**. This is exactly the architecture disclosed in Hildebrand's 1997 patent US5973252A (issued October 26, 1999), and Antares stated in its EFX 3 announcement that the plug-in "uses the same core technology as Auto-Tune 7." The 1997 patent mechanism (autocorrelation period detection + a resample rate converter that repeats a cycle when going sharp and drops a cycle when going flat + a `Decay` smoothing parameter) is therefore the single most defensible model for EFX 3's correction engine.

Three findings matter most for the build: (1) At Retune Speed 0 the correction is not merely "fast" — the manual says it produces "instantaneous transitions between notes" and "completely suppress[es] a vibrato," meaning the smoothing coefficient collapses to zero and the output period is replaced by the target period every update. (2) The "sticky"/robotic quality is a *joint* product of hard nearest-note quantization (a step function of detected pitch with a boundary near ±50 cents), the absence of formant correction (so shifted vowels take on the familiar timbre shift), and detector/period behavior — **not** any single "lock" parameter. (3) EFX 3's engine is monophonic, time-domain, and low-latency by construction; it is emphatically **not** a phase vocoder — a point Hildebrand made explicitly.

A crucial honesty caveat runs through this report: the public record cannot cleanly separate whether EFX 3's *pitch detector and shifter* are literally the 1997 patent code or the rewritten 2008 "Evo" engine. Antares marketed EFX 3 as "Auto-Tune 7 core technology," and Auto-Tune 7 (2010) inherited Evo's "complete overhaul" of the engine (2008). So EFX 3 almost certainly runs an *Evo/AT7-generation* implementation whose externally observable behavior still matches the 1997 patent's architecture but with more robust tracking. I flag where this ambiguity changes implementation decisions.

## 2. EFX 3 / Auto-Tune algorithm genealogy

### 2.1 Confirmed dates and lineage

- **Auto-Tune 1** — introduced 1997 (EFX 3 manual: "In 1997, Antares first introduced the ground-breaking Auto-Tune Pitch Correcting Plug-In"). Wikipedia gives an initial release date of September 19, 1997.
- **Auto-Tune 5** — ~2006; introduced "improvements in performance with marginal quality (noisy, poorly isolated) audio" and a redesigned UI. This is the version Antares now emulates as "Classic Mode."
- **Auto-Tune Evo (v6)** — August 2008. Introduced "Evo Voice Processing Technology." Sound on Sound: "the underlying pitch correction engine has undergone a complete overhaul, to improve the end results and to take advantage of the increasingly powerful processing available in modern personal computers." Added real-time pitch shifting, formant correction, and throat modeling to the *full* product (Native versions only, derived from the Avox 2 bundle). MusicRadar noted Evo was "noticeably more accurate than previous versions," which had "sometimes had trouble with octave tracking."
- **Auto-Tune 7** — 2010. Retained Evo Voice Processing and added integrated Time correction.
- **Auto-Tune EFX 3** — announced April 22, 2014 (Antares press release, Scotts Valley CA). Manual Rev 1.1, PN P21032-0214-M01. Added variable Retune Speed and Humanize (EFX/EFX 2 had only a 3-position correction switch), plus Auto-Motion.
- **Auto-Tune 8** — mid-2010s; added Flex-Tune. (One retailer listing describes EFX 3 as "same core technology as Auto-Tune 8," conflicting with Antares' own "Auto-Tune 7" statement — see disagreement in §11.)
- **Auto-Tune Pro / Artist / EFX+** — 2018 onward. Pro and Artist added **Classic Mode** ("the Auto-Tune 5 sound"). EFX+ is the modern successor to EFX 3.

### 2.2 The engine-generation question (the central ambiguity)

Antares' EFX 3 marketing copy and manual both say the pitch correction "works" identically to Auto-Tune 7, and Sweetwater's announcement coverage states verbatim: "Auto-Tune EFX3 uses the same core technology as Auto-Tune 7, but adds new variable Retune Speed and Humanize… controls for more natural, realistic pitch correction." Because Auto-Tune 7 carries the 2008 Evo engine, EFX 3 is best modeled as **Evo/AT7-generation code**. However:

- The **externally observable architecture** (autocorrelation period detection, nearest-note target, smoothed resample ratio, cycle repeat/delete, resample-only shifting with no formant model in the EFX line) is unchanged from the 1997 patent.
- What Evo changed, per Antares, was **robustness of pitch detection/tracking on marginal audio and octave errors**, not the fundamental correction geometry.
- EFX-class products (Access/EFX/EFX 3) **never had formant/throat processing** — that was a Native-only Evo feature. So EFX 3's *shifter* is the plain resampler, which is the 1997 patent's shifter.

**Verdict:** For correction *geometry and texture*, model the 1997 patent. For *detector robustness*, assume an Evo-grade tracker. The evidence cannot resolve whether the detector is literally the patent's E()/H() functions or a refined descendant; I mark detector internals MEDIUM/SPECULATIVE accordingly.

### 2.3 Algorithm lineage table

| Version | Year | Engine generation | Formant/throat | What changed (correction-relevant) | Evidence class |
|---|---|---|---|---|---|
| Auto-Tune 1 | 1997 | US5973252A patent engine | none | Autocorrelation detect + resample/cycle repeat-delete + Decay smoothing | (1) Antares + (2) patent |
| Auto-Tune 5 | ~2006 | Patent engine, improved detect | none (in AT5) | Better marginal-audio detection; "Classic Mode" now emulates this | (1) Antares |
| Auto-Tune Evo (6) | 2008 | "Evo Voice Processing" rewrite | added (Native only) | "Complete overhaul" of detect/correct; real-time shift, formant, throat | (1) Antares/SOS |
| Auto-Tune 7 | 2010 | Evo engine + Time | added (Native only) | Time correction; EFX 3's cited "core technology" | (1) Antares |
| **EFX 3** | **2014** | **"same core as AT7" (Evo-gen)** | **none** | **Variable Retune Speed + Humanize + Auto-Motion** | **(1) Antares press + manual** |
| Auto-Tune 8 | ~2015 | Evo-gen + Flex-Tune | added (full) | Flex-Tune transparent correction | (1) Antares |
| Pro/Artist | 2018+ | Modern + **Classic Mode (=AT5)** | added | Classic Mode = legacy AT5 algorithm reintroduced "by public demand" | (1) Antares/SOS |
| EFX+ | 2018+ | Modern engine | throat/formant | EFX 3 successor; higher latency (2.5/58.2 ms) | (1) Antares/(7) forums |

## 3. Primary-source evidence

### 3.1 The EFX 3 manual (Rev 1.1, PN P21032-0214-M01, ©2014) — passages and DSP implications

- **Pitch correction (p.5):** *"Auto-Tune EFX 3 pitch correction works by continuously tracking the pitch of your vocal and comparing it to the notes in the (user-customizable) scale of your song. The scale note closest to the input pitch is identified as the target note. If the vocal's pitch exactly matches the target note, no correction is applied. If the vocal's pitch varies from the target note, either sharp or flat, the output pitch is smoothly corrected to the target note."*
  → **Implication:** target = argmin over enabled scale notes of |detected − note| in log-frequency; correction magnitude = (target − detected); "smoothly" = the one-pole smoothing of the resample ratio.
- **Auto-Tune Vocal Effect / quantization (p.5):** *"the Auto-Tune Vocal Effect limits each note to an exact pitch, stripping out any variation, as well as forcing instantaneous transitions between notes … For the Auto-Tune Vocal Effect, the correction is instantaneous."*
  → **Implication:** at Retune 0 the smoothing coefficient = 0; output period ← target period every update; vibrato and drift are removed because the controller tracks the target, not the input deviation.
- **Retune Speed (p.6):** *"A value of 0 will cause instantaneous changes from one tone to another and will completely suppress a vibrato. If you're going for the Auto-Tune Vocal Effect, use 0. Values from 10 to 50 are typical for vocal pitch correction. Larger values let through more vibrato and other interpretative pitch gestures but also slow down how rapidly pitch corrections are made."*
  → **Implication:** Retune Speed is a time constant; larger = slower first-order approach to target; it governs both note-transition abruptness and vibrato pass-through (the same lag filters both).
- **Tracking (p.6):** *"leaving the Tracking switch at its default setting of 'Normal' will ensure accurate pitch detection and tracking. In the rare event that you encounter a problem (e.g., a vocal playing back at the wrong octave), moving the switch to 'Choosy' should make everything right."*
  → **Implication:** Tracking = the periodicity-confidence / octave-decision threshold (the patent's `eps` and the L-vs-2L fundamental test). EFX 3 exposes it only as a 2-way switch, not the 0–100 slider of the full product. "Wrong octave" is the classic first-harmonic/missing-fundamental error the patent explicitly addresses.
- **Vocal Type (p.6):** Soprano / Alto-Tenor / Baritone-Bass / Instrument. → **Implication:** sets the detector's period search range and octave priors (the patent restricts L to 2–110 on downsampled data).
- **Scale/Key + Note buttons (pp.6–7):** notes can be removed from the scale, forcing output to remaining notes. → **Implication:** the quantizer target set is the enabled-note mask; removing notes widens neighbors' capture zones, producing bigger jumps.
- **Chromatic tip (p.9):** *"If you are doing pitch correction and the singer is never more than 49 cents off (i.e., just less than half a semi-tone …) everything should work just fine."* → **Implication:** the note-decision boundary sits at ~50 cents (half a semitone) between adjacent enabled notes.
- **Humanize (p.7):** *"The Humanize function differentiates between short and sustained notes and lets you apply a slower Retune Speed just to the sustained notes … The higher the Humanize setting, the more the Retune Speed is slowed for sustained notes."*
  → **Implication:** Humanize does **not** change quantization or correction amount; it *increases the retune time constant selectively during the sustained portion of long notes*, after a note has been held. Short notes keep the fast global Retune Speed.

### 3.2 US5973252A "Pitch detection and intonation correction apparatus and method" (Hildebrand; priority Oct 27 1997; issued Oct 26 1999; originally Auburn Audio Technologies, now Antares)

**Abstract mechanism (verbatim):** *"utilizes data derived from an auto-correlation function of the waveform to continuously determine the period of the waveform. The period of the waveform is then compared to a desired period or periods (such as found in a scale). The ratio of the waveform period and the desired period is computed to re-sample the waveform. This ratio is smoothed over time to remove instantaneous output pitch changes. The ratio is used to resample the input waveform."*

Key disclosed elements:

- **Representation:** samples in a circular buffer; pitch ≡ period of repetition L (in samples), a floating-point `Cycle_period`.
- **Period estimation:** two accumulator functions E and H updated per sample: `E_i(L)=E_{i-1}(L)+x_i² − x_{i-2L}²` and `H_i(L)=H_{i-1}(L)+x_i·x_{i-L} − x_{i-L}·x_{i-2L}`; the period is the L minimizing `E_i(L) − 2H_i(L)`. Periodicity test: `E_i(L) − 2H_i(L) ≤ eps·E_i(L)`, where "eps" is a small user number (0.0–0.40). Smaller eps = stricter cycle-similarity. This is the efficiency Wikipedia (citing the patent) describes as a simplification that "changed a million multiply adds into just four."
- **Detection vs correction modes:** detection runs on **8:1 anti-alias-downsampled** data over L=2..110 (≈50–2756 Hz at 44.1k); correction tracks a small window of L around the current period, updated during interrupts (every 5th sample), shifting the E/H window as the period drifts.
- **Octave-error handling:** after finding Lmin1, the algorithm tests whether the fundamental was missed (weak-fundamental vowels) by checking L vs 2L on the *non-downsampled* data and choosing the L giving the smaller `E()−2H()`. This is exactly what EFX 3's "Choosy" Tracking hardens.
- **Sub-sample refinement:** quadratic (3-point) interpolation of the `E()−H()` minimum near Lmin → floating-point period `Pmin`.
- **Correction geometry:** `Resample_Raw_Rate = Cycle_period / desired_Cycle_period`; `desired_Cycle_period` = period of the closest scale note (or MIDI note).
- **Smoothing (the Retune Speed mechanism, verbatim):** *"The variable, Decay, is between zero and one and is set by the user. A value of zero cause Resample_Rate1 to equal Resample_Raw_Rate, giving instantaneous pitch changes. A value close to one causes a lot of smoothing, making pitch changes gradual."* → first-order lag on the rate ratio.
- **Pitch shifter (cycle repeat/delete, verbatim):** *"the output data pointer will occasionally move ahead of the input data pointer, in which case exactly one cycle period will be subtracted from the output pointer. This allows a cycle of data to be repeated"* (sharp), and *"the output data pointer will occasionally fall significantly behind the input data pointer, in which case exactly one cycle period will be added to the output pointer. This causes a cycle of data to be dropped from the output"* (flat). Output samples are interpolated at `Output_addr − 5`.
- **No formant preservation:** the shifter is a pure resampler on one buffer; resampling scales the spectral envelope with the pitch (formants move). The prior-art discussion explicitly *rejects* the Lent/PSOLA windowing that would preserve formants ("the amplitude spectra is augmented, resulting in unnatural sounds").
- **No FFT:** the patent explicitly argues FFT overlap-add shifting is unsuitable ("input data windowing and subsequent window overlap cross-fade computations are non-ideal operations that introduce distortions").
- **Latency framing:** *"the pitch detection is virtually instantaneous. The device can detect the repetition in a periodic sound within a few cycles."*

### 3.3 Hildebrand's own technical statements (the decisive primary source)

- **Andy Hildebrand, email to Sean Costello (Valhalla DSP), June 2014 — verbatim:** *"No, I don't use the Lent algorithm: way too imprecise… The math in the patent is absolutely precise to what I do… I run a simple rate converter from that point and when I have to repeat a cycle (going sharper) or delete a cycle (going flatter) I can because I know exactly what the period is at every instant… So, PSOLA is NOT used in Auto-Tune."* This is the single most valuable confirmation: the shipping product **is** the patent's rate-converter-with-cycle-repeat/delete, it is **not** PSOLA, and it is **not** a phase vocoder. (It also means the widely repeated "Auto-Tune = Lent/PSOLA" framing, including Valhalla's own 2009 post, is *wrong* per the inventor — preserve this disagreement.)
- **PBS NOVA Q&A (July 7, 2009) and NPR (2004):** Hildebrand confirms autocorrelation pitch detection and the geophysical/seismic lineage. Note **Britannica erroneously says Auto-Tune uses the FFT** — this contradicts the patent and Hildebrand and should be treated as a secondary-source error.

### 3.4 Tracking definition from Antares (full-product manuals, shared engine)

Antares support/manual text (Auto-Tune Pro, Artist, Realtime Advanced): *"The Tracking control determines how much variation is allowed in the waveform for Auto-Tune to still consider it periodic. In most cases, the Tracking should be left at its default value of 50. A noisier signal or a vocal performance that is unusually breathy may require a more 'relaxed' setting (higher Tracking value). If you're hearing artefacts such as clicks or pops, try setting the Tracking to a 'choosier' setting (lower Tracking value)."* This is the plain-language description of the patent's `eps` periodicity threshold. EFX 3 collapses this to Normal/Choosy.

## 4. Pitch-detection reconstruction

**Representation:** time-domain period L in samples (not frequency, not FFT bins). **Method:** difference-function/normalized-autocorrelation minimum via the patent's E/H accumulators (behaviorally equivalent to a YIN-style difference function running continuously per-sample). **Window/cycles:** ~2 periods (the E/H functions accumulate "over two periods, 2L"); the method "continuously compar[es] each cycle with the previous cycle." **Octave handling:** L-vs-2L fundamental test on full-rate data; "Choosy" tightens acceptance. **Confidence:** the `eps` threshold — periodicity accepted only if `E−2H ≤ eps·E`; on failure the system returns to detection mode and passes audio uncorrected (Resample_Rate ← 1). **Sub-sample refinement:** 3-point quadratic interpolation. **Voiced/unvoiced:** low-energy or non-periodic frames fail the test → no correction → consonants/breath pass through unprocessed (a key part of the sound: sibilants and plosives untouched, vowels snap).

Confidence: **HIGH** that the architecture is period-domain autocorrelation-family; **MEDIUM** that EFX 3 uses literally the E/H recursion vs an Evo refinement.

## 5. Note-selection reconstruction

**Nearest-note quantizer:** convert detected period → log-frequency (cents); target = nearest enabled scale note; decision boundary between two adjacent enabled notes ≈ **50 cents** (manual: correction "just works" if "never more than 49 cents off"). Removing a note from the scale absorbs its capture zone into neighbors, widening jumps.

**Older-algorithm behavior (the objective):** treat the target as a **memoryless step function of instantaneous detected pitch** — there is *no* evidence of "musically intelligent" note-hold or transition-smart logic in EFX 3 (that is later Flex-Tune territory, explicitly absent from EFX-class products). This is why a slow glide across a semitone boundary makes the output **jump abruptly**: the quantizer switches the instant detected pitch crosses 50 cents, and at low Retune Speed the shifter snaps.

**Candidate state machines (ranked):**
1. **Pure memoryless nearest-note (most likely for EFX 3):** `target = quantize(detectedCents)` every update; no hysteresis; only the Retune-Speed lag softens the audible jump. Explains the "sticky then sudden" character and vibrato flattening at low speed.
2. **Nearest-note + confidence gate:** target updates only when `periodicityConfidence > threshold`; on low confidence, hold previous target. Plausibly present via the detection/correction mode switch (patent returns to detection on failure).
3. **Nearest-note + tiny hysteresis:** a small dead-band around the 50-cent boundary to prevent chatter when the singer sits exactly between notes. Not documented; propose as a *developer control*, off by default to match the old sound.

Conceptual variables to expose: `currentTarget, candidateTarget, candidateConfidence, switchThreshold, holdDuration, voicedConfidence, previousPeriod, maxOctaveJump`.

Confidence: **HIGH** on memoryless nearest-note + 50-cent boundary; **SPECULATIVE** on any hysteresis/hold in EFX 3 (probably none).

## 6. Retune Speed reconstruction

**Model:** first-order (one-pole) exponential lag applied to the **resample-rate ratio** (equivalently to corrected pitch in log-frequency), per the patent's `Decay` recursion. The patent prose is verbatim confirmed; Antares documents modern Retune Speed **in milliseconds** ("Retune Speed controls how rapidly the pitch correction is applied … a value of zero will cause immediate changes from one pitch to another"). No Antares source states a formal τ or closed-form equation, so the exponential form is a strong inference, not a quoted formula.

**Ranked mathematical candidates (equations for C++).** Let `r_raw[n] = P_in[n]/P_target[n]` (or work in cents: `e[n] = target_cents − in_cents`):

1. **One-pole lag (most likely):** `α = exp(−T_sample / τ)`, with τ from Retune Speed ms (`τ ≈ RetuneSpeed_ms`, 0 → α=0); `r[n] = α·r[n−1] + (1−α)·r_raw[n]`. At RS=0, α=0 → instantaneous. Matches patent Decay and manual "instantaneous at 0."
2. **Cents-domain one-pole:** identical filter on log-frequency error: `out_cents[n] = out_cents[n−1] + (1−α)(target − out_cents[n−1])`. Equivalent audible behavior; slightly different large-jump trajectory.
3. **Slew-limit (cents/second):** `out += clamp(target−out, −slew·T, +slew·T)`. Produces a linear "portamento"; less consistent with the patent's multiplicative smoothing. Offer as alternate shape.
4. **Update-once-per-period:** because the patent updates period tracking periodically (not every sample) and repeats/deletes whole cycles, the *effective* correction is quantized to cycle boundaries. Candidate explanation for micro-stutter at fast settings; expose as `pitchUpdateInterval`.

**Key behaviors to replicate:** (a) the same lag filters vibrato and note transitions (manual ties them together); (b) RS=0 truly means immediate target replacement, not a 1 ms floor; (c) a target-note change does **not** reset the filter state (it keeps lagging toward the new target — producing the smooth-then-snap feel); (d) there is *no hidden minimum smoothing* at 0 in the classic algorithm (Antares' Classic Mode is noted for "a more noticeable attack and shift between pitches at faster retune speeds," i.e., less hidden smoothing than the modern engine).

Confidence: **HIGH** first-order lag; **MEDIUM** exact ms↔α mapping; **MEDIUM** cycle-boundary quantization of updates.

## 7. Humanize reconstruction

**What Antares claims (EFX 3 manual + shared across versions):** Humanize applies a **slower Retune Speed only during the sustained portion of longer notes**, leaving short notes at the fast global speed so they stay in tune while long notes keep natural variation. Auto-Tune Realtime/Pro manuals repeat: "applies a slower Retune Speed only during the sustained portion of longer notes."

**Inferred mechanism:** track how long the current target note has been held (`noteHoldTime`). While short (attack/onset), use global `RetuneSpeed`. Once held beyond a threshold, progressively increase the effective time constant by a factor scaling with Humanize. It is based on **target-note duration/stability**, not on reducing correction amount, and it **preserves deviation from center pitch** on sustains (vibrato returns) rather than detuning the note center.

```
effectiveTau = baseTau(RetuneSpeed);
if (noteHoldTime > humanizeOnset)
    effectiveTau *= (1 + Humanize * f(noteHoldTime));  // progressive ramp
alpha = exp(-Tsample / max(effectiveTau, epsilon));
```
Confidence: **HIGH** on intent/behavior (documented); **MEDIUM** on ramp shape (progressive vs step) and onset threshold.

## 8. Pitch-shifting / resynthesis reconstruction

**Ranked conclusion:**

1. **(A) Period-synchronous resampling with integer cycle repeat/delete — WINNER.** Directly disclosed in the patent and directly confirmed by Hildebrand ("simple rate converter… repeat a cycle going sharper… delete a cycle going flatter… PSOLA is NOT used"). Time-domain, low-latency (~a few periods), low CPU (feasible on a Motorola 56k DSP in 1997, trivial on 2014 CPUs), monophonic. Preserves waveform shape within a cycle; **does not preserve formants** (envelope scales with pitch). Produces the classic texture; the cycle insert/delete points are candidate sources of micro-glitches at large shifts.
2. **(B) PSOLA/TD-PSOLA — EXPLICITLY REJECTED BY THE INVENTOR, but a useful implementation reference.** Valhalla DSP originally framed Hildebrand's method as Lent/PSOLA-like, but Hildebrand rejected this in 2014 ("way too imprecise"). True PSOLA windows two-period grains and overlap-adds and *can* preserve formants; Hildebrand's method **resamples** (so formants move) and repeats/deletes *whole cycles*. Use PSOLA code only as a structural reference for period-synchronous bookkeeping — not as the algorithm.
3. **(D) Variable-rate resampling + period tracking — essentially (A)** described differently; the "rate converter" *is* variable-rate resampling with cycle bookkeeping to keep in/out pointers bounded.
4. **(C) Granular OLA — possible but not what Antares did**; would blur transients differently.
5. **(E) Phase vocoder / STFT — REJECTED.** Patent argues against FFT overlap-add; Hildebrand says he runs a rate converter, not a vocoder. A phase vocoder sounds "too smooth/too modern" and smears transients — the opposite of the sticky sound.
6. **(F) Hybrid — later products only** (Evo added formant/throat to the *full* product; EFX line stayed resample-only).

**How to interpret "repeat/delete cycles" literally and robustly:** maintain an input read pointer advancing at the resample rate `r = P_in/P_target`. When output would overrun input, subtract one `Cycle_period` from the output pointer (repeat the last cycle → raises pitch); when output underruns, add one `Cycle_period` (drop a cycle → lowers pitch). Apply a short raised-cosine cross-fade (developer-tunable `cycleCrossfadeLength`, e.g. 5–20% of a period) at each splice to suppress the boundary discontinuity while keeping the characteristic grain.

Confidence: **HIGH** architecture; **HIGH** no-formant; **MEDIUM** exact splice/crossfade details.

## 9. Formant / unvoiced handling

**EFX 3 has no formant correction, no throat modeling, no spectral-envelope preservation.** Antares' version-comparison grid lists "Automatic Formant Correction" and "Adjustable Throat Modeling" as features of **Live/Pro/Artist** but **not** the EFX/Access class. Formant/throat entered the *full* product with Evo (2008, "derived from the Avox 2 plug-in") and are Native-only. Because the EFX shifter resamples, **formants shift with pitch** — up-shifts brighten/"chipmunk," down-shifts darken. **This is central to recreating the sound: do not add formant preservation to the default path.** Expose `formantPreservationStrength` as a developer control defaulting to **0**.

**Unvoiced/sibilant/consonant handling:** frames failing the periodicity test are passed through uncorrected (detection mode, Resample_Rate=1). Consonants and breath keep their natural pitch/formants while vowels snap — the abrupt vowel-onset "catch" is part of the aesthetic. "Consonant tearing" can occur when the detector briefly mis-locks at a voiced/unvoiced boundary.

Confidence: **HIGH**.

## 10. Artifact analysis (each artifact mapped to a stage)

| Artifact | Most likely stage | Mechanism | Keep? |
|---|---|---|---|
| Pitch snapping / robotic steps | note selector + retune (RS≈0) | memoryless quantizer + α≈0 | **Yes** (core sound) |
| Vibrato flattening | retune controller | one-pole lag removes slow deviation | **Yes** at low RS |
| Formant "chipmunk"/darkening | pitch shifter | resample scales envelope | **Yes** (EFX signature) |
| Period-boundary clicks/graininess | pitch shifter | cycle repeat/delete splice | Partially — tune crossfade |
| Octave jumps / wrong-octave | detector | L vs 2L fundamental error | Reduce via "Choosy"/eps |
| Detector hesitation / notes not registering | detector confidence | eps too strict / low input level (Gearspace: notes "don't register" unless preamp driven harder) | Expose threshold |
| Micro-stutter at fast settings | retune update cadence | cycle-quantized updates | Optional |
| Consonant tearing | voiced/unvoiced boundary | brief mis-lock | Minimize |
| Pitch flutter | detector + no hysteresis | target chatter near 50-cent boundary | Optional dead-band |

Community-reported EFX 3 clicks/pops with Choosy off, and "notes not registering" unless the preamp is driven harder, both map to the periodicity threshold / input-level dependence of the detector — detector imperfections that are part of the character.

## 11. EFX 3 vs later Antares behavior

- **Classic Mode = the backwards clue.** Antares reintroduced the **Auto-Tune 5** algorithm as "Classic Mode" in Pro/Artist "by public demand" (Sound on Sound). Sweetwater's Quickstart Guide describes it verbatim: "Classic Mode, which is an Auto-Tune 5 recreation that sounds slightly brighter and has a more noticeable attack and shift between pitches at faster retune speeds." Black Ghost Audio adds it gives "a prominent attack and transition between notes at fast Retune Speeds." This tells us what Antares itself considers characteristic of the *old* algorithm: **brighter tone, harder/more pronounced note transitions, lower latency, less hidden smoothing.** EFX 3 (Evo/AT7-gen) sits between AT5-Classic and the fully modern engine but is much closer to the hard/bright end than Flex-Tune-era products.
- **What Classic Mode changes:** Antares does not publish internals, but reviewer/support descriptions indicate it changes **correction timing/attack and tonal brightness**; there is no claim it changes the *detector* or adds/removes formant processing. Safest reading: Classic Mode restores the older, less-smoothed correction trajectory and brighter resynthesis.
- **Latency:** EFX+ = **2.5 ms low-latency / 58.2 ms otherwise** at 44.1 kHz (KVR, matching the "2.3 ms" figure elsewhere); Access = **0 ms** at 44.1 kHz; EFX 3 is described only as **"ultra-low latency"** by Antares (press release) and "zero latency" by user aggregators (Equipboard) — **no precise official EFX 3 latency figure exists.** Antares currently cites **2.5 ms** for AutoTune 2026's real-time correction.

## 12. Community A/B evidence (clustered into testable hypotheses)

Subjective reports are not facts, but recurring independent themes convert to hypotheses:

- **"EFX 3 preferred over EFX+ for tuning stability & note holding" (Equipboard aggregation of forum/Reddit):** → Hypothesis: EFX 3's memoryless quantizer + low hidden smoothing locks harder than modern Flex-Tune engines. Testable via F0-trajectory step sharpness.
- **"EFX 1 is 'dumber' and better for future/trap vocals; EFX 2/3 are 'too sensitive'" (Gearspace):** → Hypothesis: later EFX detectors track more nuance/faster, passing more micro-variation; the "dumber" earlier detector produced blunter, more locked output. Testable via detector-bandwidth comparison.
- **"EFX 3 sought for its distinct sound over EFX+/Pro" (Equipboard):** → Hypothesis: EFX+/modern engine added smoothing/formant behavior that softened the snap. Testable via spectral brightness + transition sharpness A/B.
- **Classic vs modern (Sound on Sound, Black Ghost Audio, Antares/Sweetwater):** consistent across unrelated sources → **brighter + harder transitions + lower latency** for the old algorithm. This is the strongest cross-source consensus and the best target for the reconstruction.

Disagreements to preserve: one retailer says EFX 3 = "Auto-Tune 8 core," Antares says "Auto-Tune 7"; Britannica says FFT, the patent/Hildebrand say autocorrelation+resampler; Valhalla's 2009 post framed the shifter as PSOLA, Hildebrand's 2014 email says "PSOLA is NOT used." Do not flatten these.

## 13. Competing DSP hypotheses (what distinguishes them)

| Hypothesis | Supports | Discriminating experiment |
|---|---|---|
| Resample + cycle repeat/delete (patent + Hildebrand 2014) | patent, Hildebrand email, low latency, no-formant | On a sustained shifted vowel, look for **formants moving with pitch** and periodic cycle-splice signatures in the spectrogram |
| PSOLA with formant preservation | Valhalla 2009 framing (retracted by inventor) | If formants **stayed put** under shift → PSOLA-preserving; EFX has no formant feature, so expect formants move → favors resample |
| Phase vocoder | (none credible; Britannica error) | Transient smearing, phasey artifacts on consonants → absent in Auto-Tune → rejects vocoder |
| Slew-limit vs one-pole retune | ambiguous | Feed a step in detected pitch; measure whether approach is **exponential** (one-pole) or **linear** (slew) |

## 14. Confidence-ranked behavioral specification

Pipeline: INPUT → preprocessing → voiced/unvoiced → period/F0 estimation → confidence logic → nearest-note quantizer → (minimal/none) persistence → Retune-Speed one-pole trajectory → Humanize → period-synchronous resample+cycle repeat/delete → (no formant) → OUTPUT.

| Block | Algorithm | State | Update | Math | Latency | Confidence |
|---|---|---|---|---|---|---|
| Preprocess | DC block + light HPF; optional 8:1 downsample for detect | filter state | per sample | IIR | ~0 | HIGH |
| Voiced/unvoiced | energy + periodicity (eps) | energy acc | per sample/frame | E−2H ≤ eps·E | intrinsic | HIGH |
| Period est. | E/H autocorr minimum + quadratic interp | E(),H() arrays, EH_offset | per few samples | patent eqs | ~2 periods | HIGH arch / MED internals |
| Octave logic | L vs 2L fundamental test; "Choosy"=stricter | candidate Ls | per detect | compare E−2H | — | HIGH |
| Quantizer | nearest enabled scale note in cents; boundary 50¢ | scale mask | per update | argmin | 0 | HIGH |
| Persistence | none (memoryless) by default | currentTarget | per update | step | 0 | MED (likely none) |
| Retune trajectory | one-pole lag on rate ratio; RS(ms)→α | r[n−1] | per sample | r=α r+(1−α)r_raw | 0 | HIGH form / MED map |
| Humanize | lengthen τ on sustained held notes | noteHoldTime | per sample | τ scaling | 0 | HIGH intent |
| Shifter | resample + integer cycle repeat/delete + short xfade | in/out ptrs, Cycle_period | per sample | pointer math + interp | ~1–3 periods | HIGH arch |
| Formant | **none** (default) | — | — | — | 0 | HIGH |

## 15. C++ pseudocode

```cpp
// ---- Pitch detector (period-domain, patent-style) ----
struct Detect { double cyclePeriod; double confidence; bool voiced; };

Detect processPitchFrame(const float* x, int i, DetState& s, const VoiceRange& vr) {
    for (int L = vr.Lmin; L <= vr.Lmax; ++L) {              // update E(L),H(L)
        s.E[L] += x[i]*x[i]         - x[i-2*L]*x[i-2*L];
        s.H[L] += x[i]*x[i-L]       - x[i-L]*x[i-2*L];
    }
    int Lbest   = argmin_L( s.E[L] - 2*s.H[L] );
    double v    = s.E[Lbest] - 2*s.H[Lbest];
    bool periodic = (v <= s.eps * s.E[Lbest]);              // eps = Tracking/Choosy
    Lbest       = resolveOctave(Lbest, s);                  // L vs 2L fundamental test
    double Pmin = quadraticInterpMinimum(s, Lbest);         // sub-sample
    return { Pmin, 1.0 - v/std::max(1e-9, s.E[Lbest]), periodic && energyOK(s) };
}

// ---- Target-note quantizer (memoryless nearest note) ----
double chooseTargetPitchCents(double inCents, const ScaleMask& scale) {
    return nearestEnabledNoteCents(inCents, scale);         // boundary ~50 cents
}

// ---- Target persistence (default: none; optional dev hysteresis) ----
void updateTargetState(TargetState& t, double cand, double conf, const Dev& dev) {
    if (!dev.useHysteresis) { t.current = cand; return; }
    if (cand != t.current) {
        if (std::abs(cand - t.current) > dev.switchThresholdCents
            && conf > dev.confThresh && ++t.hold > dev.holdSamples) {
            t.current = cand; t.hold = 0;
        }
    } else t.hold = 0;
}

// ---- Retune controller (one-pole lag on the rate ratio) ----
double calculateCorrectedRate(double inPeriod, double targetPeriod,
                              double& rState, double retuneMs, double Ts) {
    double rRaw = inPeriod / targetPeriod;
    double tau  = retuneMs * 1e-3;                          // 0 -> instantaneous
    double a    = (tau <= 0) ? 0.0 : std::exp(-Ts / tau);
    rState = a*rState + (1.0 - a)*rRaw;                     // patent "Decay" smoothing
    return rState;
}

// ---- Humanize (lengthen tau on sustained held notes) ----
double applyHumanize(double retuneMs, double holdSec, double humanize, const Dev& d) {
    if (holdSec <= d.humanizeOnsetSec) return retuneMs;
    double ramp = std::min(1.0, (holdSec - d.humanizeOnsetSec)/d.humanizeRampSec);
    return retuneMs * (1.0 + humanize * ramp);
}

// ---- Period-synchronous shifter (resample + cycle repeat/delete) ----
float processPitchPeriod(RingBuf& buf, PtrState& p, double rate,
                         double cyclePeriod, const Dev& dev) {
    p.out += rate;                                          // advance read pointer
    if (rate > 1.0 && (p.out > p.in))               p.out -= cyclePeriod;  // repeat (sharp)
    if (rate < 1.0 && (p.in - p.out > cyclePeriod)) p.out += cyclePeriod;  // drop  (flat)
    float y = interpolate(buf, p.out - 5.0);               // fractional read (patent: -5)
    return applyCycleCrossfade(y, buf, p, dev.crossfadeFrac);
}

// ---- Complete block loop ----
void processBlock(float* io, int N, State& S, const Dev& dev) {
    for (int n = 0; n < N; ++n) {
        S.ring.push(io[n]); S.in += 1.0;
        Detect d = processPitchFrame(S.ring.data(), S.i++, S.det, S.range);
        if (!d.voiced) { io[n] = passthrough(S.ring, S.p); resetRate(S); continue; }
        double inCents = periodToCents(d.cyclePeriod);
        double tgt     = chooseTargetPitchCents(inCents, S.scale);
        updateTargetState(S.tgt, tgt, d.confidence, dev);
        double rMs  = applyHumanize(S.retuneMs, S.noteHoldSec, S.humanize, dev);
        double rate = calculateCorrectedRate(d.cyclePeriod,
                        centsToPeriod(S.tgt.current), S.rState, rMs, S.Ts);
        io[n] = processPitchPeriod(S.ring, S.p, rate, d.cyclePeriod, dev);
        updateHoldTimers(S, tgt);
    }
}
```

## 16. Prototype architecture (JUCE/C++)

Single `AudioProcessor`, mono path (duplicate for stereo/dual-mono). One ring buffer sized for the lowest supported F0 at the highest sample rate (≈50 Hz at 96 kHz ⇒ ~1920 samples/period ⇒ buffer ≥ ~4–6k samples, a few periods deep). Detector runs continuously; correction pointer logic per sample. Report a small fixed latency (~1–3 periods; target ≤ ~5 ms at 44.1/48 k) via `setLatencySamples`. Support 44.1/48/88.2/96 kHz; scale the L search range and the τ↔α mapping by sample rate. Parameters via `AudioProcessorValueTreeState`.

## 17. Validation / testing strategy (without EFX 3)

1. **Public EFX 3 demo recordings** (e.g., Equipboard-listed usages such as Denis Kenzo's "#Muz Кухня" segment and Mont Duamel livestream; YouTube EFX 3 tutorials): extract output F0 trajectories, measure **transition sharpness** across semitone boundaries and **vibrato suppression** at RS≈0. Caveat: lossy audio — do not over-interpret.
2. **Auto-Tune 7 / Classic-Mode recordings:** compare your engine's spectral **brightness** and **attack** against Classic Mode's documented "brighter, more pronounced transitions."
3. **Commercial stems known to use EFX 3-class tuning:** look for F0 step functions, the formant-moves-with-pitch signature, and cycle-splice spectral marks.
4. **Discriminating measurements:** (a) formant trajectory under shift (moves ⇒ resample, not PSOLA-preserving); (b) step-response of pitch (exponential ⇒ one-pole; linear ⇒ slew); (c) absence of vocoder phase smearing on consonants.

## 18. Unknowns requiring future experimentation

- Whether EFX 3's detector is literally the patent E/H recursion or an Evo refinement (affects octave-error and breathy-vocal behavior). **SPECULATIVE.**
- Exact Retune-Speed ms↔α mapping and whether updates are cycle-quantized (affects micro-stutter). **MEDIUM.**
- Humanize ramp shape and onset threshold. **MEDIUM.**
- Whether any small hysteresis exists at the 50-cent boundary. **SPECULATIVE (probably none).**
- Precise EFX 3 latency (no official figure). **UNKNOWN.**

## 19. Future oracle test suite (if EFX 3 access is obtained)

Synthetic corpus, all rendered through EFX 3 at fixed settings (RS=0 and RS=20; Humanize 0 and mid; Normal and Choosy):
- Sine and sawtooth glissandi (slow, spanning several semitones) → measure quantization boundary (expect ~50¢) and transition trajectory.
- Synthesized vowels (known formants) at fixed pitch offsets **±10, ±25, ±49, ±51 cents** from a scale tone → confirm boundary and correction amount; track formant motion under correction.
- Controlled vibrato (5–6 Hz, ±30–100 cents) → measure vibrato suppression vs Retune Speed and Humanize.
- Instantaneous note steps and octave jumps → measure settling shape (exponential vs linear) and octave-error rate.
- Breath/noise mixed with tone, vocal fry, consonant–vowel transitions → map voiced/unvoiced gating and tearing.
- Amplitude changes at constant F0 → confirm level-dependence of detection (Gearspace-reported).

**Measure from output:** F0(t), formant tracks, spectrogram splice signatures, latency (impulse test), and correction-amount readout — enough to identify detector window, eps, note boundary, retune shape, and shifter type quickly.

## 20. Recommended V0/V1/V2 implementation plan

### If I were implementing the first prototype tomorrow
Simplest architecture with the highest probability of the EFX 3 aesthetic:
- **Pitch detection:** continuous period-domain autocorrelation/difference-function (patent E/H or an equivalent normalized-difference tracker), ~2-period window, quadratic sub-sample refinement, `eps`/energy voiced gate, L-vs-2L octave check.
- **Frame/window:** detector spans ~2 periods; correction is per-sample pointer logic (not block-quantized) initially.
- **Target logic:** memoryless nearest enabled scale note, 50-cent boundary, scale mask; **no** musical smart logic.
- **Retune Speed:** one-pole lag on the period-ratio, `α = exp(−Ts/τ)`, τ from ms (0 ⇒ α=0 ⇒ instant).
- **Humanize:** lengthen τ on sustained held notes after an onset threshold.
- **Pitch shifting:** period-synchronous resample with integer cycle repeat (sharp)/delete (flat), short raised-cosine splice crossfade.
- **Voiced/unvoiced:** pass unvoiced through uncorrected.
- **Formant:** none (resample-only) — this is deliberate and central to the sound.
- **Latency:** ~1–3 periods (≤ ~5 ms at 44.1/48 k).
- **JUCE:** single processor, one ring buffer, APVTS params, `setLatencySamples`.

**PROTOTYPE V0 (proof of concept):** offline, mono, fixed key/scale, RS=0 only. Autocorrelation period → nearest note → resample+cycle repeat/delete. Goal: demonstrate the hard snap and formant-moves-with-pitch on a sustained vowel. Success = audible classic snap and correct 50-cent boundary on a sine glissando.

**PROTOTYPE V1 (usable mono tuner):** real-time JUCE plugin; variable Retune Speed (one-pole), Humanize, Normal/Choosy tracking, editable scale, vocal-type range presets; splice crossfade; unvoiced passthrough. Goal: usable on real vocals with the sticky character.

**PROTOTYPE V2 (forensic refinement):** add cycle-quantized update option, boundary dead-band option, octave-jump resistance, tunable eps, brightness match to Classic Mode; validate against demo/stem F0 trajectories; A/B against historical recordings.

**Developer/debugging controls to expose (even though EFX 3 never did):** detector window/period range; periodicity threshold (`eps`); note-switch hysteresis + hold time; octave-jump resistance / `maxOctaveJump`; correction smoothing **shape** (one-pole vs slew vs cents-domain); pitch-update interval (per-sample vs per-cycle); cycle-crossfade length; unvoiced hold behavior; formant preservation strength (**default 0**). These let you tune by ear against memory and historical recordings — the whole point being to rediscover *why the old system sounded the way it did*, not to build the cleanest modern corrector.

## 21. Annotated bibliography

- **Auto-Tune EFX 3 Owner's Manual, Rev 1.1, PN P21032-0214-M01 (©2014 Antares).** Primary source for the pitch-correction description, Retune Speed 0 behavior, Tracking Normal/Choosy, Vocal Type, the 50-cent tip, and Humanize. Evidence class (1).
- **US5973252A, Hildebrand, priority 1997-10-27, issued 1999-10-26 (Auburn/Antares).** The canonical algorithm: E/H autocorrelation period detection, resample-rate ratio, `Decay` smoothing, cycle repeat/delete, explicit rejection of FFT and Lent/PSOLA windowing. Class (2).
- **Antares announcement (April 22, 2014) via Sweetwater InSync / Everything Recording.** "Auto-Tune EFX3 uses the same core technology as Auto-Tune 7"; "ultra-low latency"; Humanize description. Class (1).
- **Andy Hildebrand email to Sean Costello, June 2014 (Valhalla DSP).** Decisive: "I run a simple rate converter… repeat a cycle (going sharper) or delete a cycle (going flatter)… PSOLA is NOT used in Auto-Tune." Also Valhalla's 2009 (now-superseded) Lent/PSOLA framing. Class (3).
- **PBS NOVA Q&A with Hildebrand (2009); NPR (2004).** Autocorrelation origins, seismic lineage. Class (3).
- **Sound on Sound reviews (Auto-Tune Evo; Auto-Tune Pro; original Auto-Tune; EFX+).** Evo "complete engine overhaul"; Classic Mode = reintroduced AT5 algorithm, modern engine "far more natural" by comparison; Tracking fader semantics. Class (5).
- **Antares support + Pro/Artist/Realtime Advanced manuals (via Antares, UAD, Sweetwater).** Tracking = "how much variation allowed … still consider it periodic"; Retune Speed in milliseconds; Humanize = slower speed on the sustained portion. Class (1).
- **Sweetwater SweetCare Quickstart + Black Ghost Audio.** Classic Mode "sounds slightly brighter and has a more noticeable attack and shift between pitches at faster retune speeds." Class (1)/(5).
- **KVR forum (latency).** EFX+ 2.5 ms / 58.2 ms; Access 0 ms @ 44.1k. Class (7).
- **Gearspace / Equipboard aggregations.** EFX 1 "dumber" / EFX 2-3 "too sensitive"; EFX 3 "note holding / tuning stability" preferred over EFX+. Subjective; Class (7)/(8) — hypotheses only.
- **TD-PSOLA implementations (sannawag/TD-PSOLA; tdpsola Rust crate); Lent 1989 CMJ.** Reference building blocks for period-synchronous processing; they preserve formants (unlike EFX) and use OLA (which Hildebrand rejected), so use only as structural references, not the algorithm. Class (4).
- **YIN/pYIN/SWIPE literature.** Alternative period detectors; long-window versions risk "too clean/too modern" tracking — use short-window, low-latency variants only. Class (4).
- **Britannica "Auto-Tune."** States FFT — contradicts the patent and Hildebrand; flagged as a secondary-source error. Class (5, unreliable on this point).

---

### A closing note on epistemic honesty
The strongest, best-sourced claims are: (i) the correction *architecture* is the 1997 patent's autocorrelation-period + resample-with-cycle-repeat/delete pipeline (confirmed by the inventor in 2014); (ii) EFX 3 shares Auto-Tune 7's engine (Antares); (iii) EFX-class products have no formant model (Antares feature grid); (iv) RS=0 means truly instantaneous target replacement (manual); (v) the 50-cent boundary and Humanize semantics (manual). The genuinely underdetermined points — whether the detector internals are the literal 1997 E/H code or an Evo-era rewrite, the exact ms→coefficient mapping, and the precise EFX 3 latency — are called out as such and are exactly what a future oracle test on a working EFX 3 instance (§19) would resolve.