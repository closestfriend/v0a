# Evidence and Hypotheses

## Purpose

This file prevents plausible engineering choices from hardening into invented history. It records what the project treats as evidence, where the supplied reports disagree, what the baseline does, and how an experiment could change that decision.

This is a clean-room behavioral reconstruction. It must not use proprietary binaries, extracted assets, private implementation details, copied branding, or copied source code.

## Authority and decision rules

Use this order for implementation decisions:

1. `DIRECTIVE_FOR_CODEX.md`
2. `docs/ARCHITECTURE.md` and this document
3. The supplied reports
4. New engineering inference

The directive is the product specification. A research report does not become authoritative because it uses confident language, equations, concrete constants, or implementation pseudocode.

When the reports disagree:

1. retain the disagreement in this register;
2. prefer an identifiable primary source over commentary or inference;
3. distinguish disclosed architecture from an inferred EFX 3 implementation detail;
4. select the directive's baseline; and
5. expose unresolved alternatives as disabled experiments when they are useful and safe.

## Provenance labels

| Label | Meaning | Permitted implementation treatment |
|---|---|---|
| EVIDENCE-BASED | Strong public evidence supports the behavior or architecture. | May define the baseline, while exact undisclosed constants remain separate. |
| PLAUSIBLE RECONSTRUCTION | DSP inference fits the evidence, but the exact historical mechanism is unknown. | May be the baseline when the directive selects it; label it and keep replaceable seams. |
| EXPERIMENTAL | A testable hypothesis without sufficient historical support. | Must be explicit, measurable, and disabled by default unless the directive says otherwise. |
| PRODUCT REQUIREMENT | The project requires this behavior without claiming EFX 3 provenance. | Implement normatively and describe it as an original product choice. |

Confidence words such as `HIGH`, `MEDIUM`, and `LOW` describe confidence in a claim. They do not replace the provenance label. A high-confidence product requirement is still not historical evidence.

## Source handling

The supplied reports relay several source types. Until a cited original is independently checked, the project should cite the report as the immediate source and avoid extending the reported claim.

| Source type | Typical use | Limitation |
|---|---|---|
| Project directive | Normative product and reconstruction rules | Deliberately selects baselines even where history is unresolved |
| EFX 3 manual or Antares product documentation | User-visible behavior: monophony, closest scale note, Retune 0, Humanize, Tracking, Voice Type | Usually does not disclose equations or constants |
| US5973252A | Period detector, rate-ratio smoothing, cycle repeat/delete architecture | Describes the original patent architecture, not proof that every EFX 3 internal remained literal |
| Hildebrand public technical statement | Rejects PSOLA; confirms simple rate converter with cycle repeat/delete | Does not enumerate all EFX 3 parameters |
| Antares EFX 3 announcement | Auto-Tune 7 core lineage and low-latency positioning | “Core” does not prove byte-for-byte or algorithm-for-algorithm identity |
| Later Classic Mode documentation | Secondary clue for older bright/hard behavior | Classic Mode recreates Auto-Tune 5, not EFX 3 |
| Reviews and technical journalism | Context and cross-checking | May paraphrase marketing or infer internals |
| Community reports and A/B impressions | Generate listening hypotheses | Subjective, uncontrolled, and often missing exact settings |
| Report pseudocode | Illustrate one reconstruction | Contains speculative constants and may contain implementation errors |

## Evidence ledger

| ID | Claim | Classification | Confidence | Baseline consequence |
|---|---|---|---|---|
| EV-001 | EFX 3 correction is intended for monophonic sources and continuously compares detected pitch to notes in a user scale. | EVIDENCE-BASED | High | Build a monophonic detector and explicit scale target set. |
| EV-002 | The closest enabled scale note is the target; adjacent chromatic boundaries are nominally near 50 cents. | EVIDENCE-BASED | High | Use nearest-note selection in logarithmic pitch space and test 49/51-cent behavior. |
| EV-003 | Retune Speed 0 produces immediate note transitions and suppresses vibrato. | EVIDENCE-BASED | High | `tau = 0` must be a true immediate branch with no hidden floor. |
| EV-004 | The patent smooths the raw input-period/target-period resample ratio with a first-order recurrence. | EVIDENCE-BASED | High for patent architecture | Use ratio-domain one-pole as the project baseline. |
| EV-005 | The shifter is a rate converter that repeats a whole cycle when shifting sharper and deletes a whole cycle when shifting flatter. | EVIDENCE-BASED | High | Implement one-stream period-sized pointer corrections. |
| EV-006 | Hildebrand explicitly rejected Lent/PSOLA for Auto-Tune. | EVIDENCE-BASED | High | Do not use PSOLA/TD-PSOLA as the primary shifter. |
| EV-007 | The original patent detector is a time-domain autocorrelation/difference-family period detector with energy-relative periodicity. | EVIDENCE-BASED | High | Implement an inspectable time-domain reference detector. |
| EV-008 | The patent describes anti-aliased 8:1 analysis decimation, roughly two-period comparisons, quadratic refinement, and octave checking. | EVIDENCE-BASED | High for patent | Make these available in the historical detector path while keeping EFX 3 literal identity qualified. |
| EV-009 | EFX 3 was marketed as using Auto-Tune 7 core technology, whose lineage includes the Evo rewrite. | EVIDENCE-BASED | High for marketing lineage | Treat exact detector internals as unresolved rather than claiming literal 1997 code. |
| EV-010 | EFX-class processing omitted user formant correction and throat modeling. | EVIDENCE-BASED | High | Default to direct resampling with no envelope restoration. |
| EV-011 | Humanize differentiates short from sustained notes by slowing sustained-note correction. | EVIDENCE-BASED | High behavior, low exact mechanism | Build replaceable Multiplicative/Additive sustained-state modes; do not fix undocumented timing or Retune-0 interaction. |
| EV-012 | Tracking controls how much waveform variation is accepted as periodic, and Choosy is stricter. | EVIDENCE-BASED | High behavior | Map Normal/Choosy primarily to periodicity acceptance; values remain experimental. |
| EV-013 | Voice Type influences appropriate tracking range. | EVIDENCE-BASED | High behavior | Use configurable detector search bounds, not purported historical hardcoded ranges. |
| EV-014 | EFX 3's exact latency is publicly established. | EXPERIMENTAL / UNKNOWN | Low | Measure this implementation and never label an estimate as EFX 3 fact. |
| EV-015 | The Note Matrix's Target/Bypass/Remove semantics are required here. | PRODUCT REQUIREMENT | Normative | Use fixed ordinary chromatic cells for Bypass and nearest-Target capture for Target/Remove inputs, without historical attribution. |

## Preserved report disagreements

“Report A” is `research/Report_a_Auto-Tune EFX 3 DSP Reconstruction.md`. “Report B” is `research/Report_b_Forensic DSP Reconstruction of Antares Auto-Tun_e EFX 3 (2014) [B].md`.

| Topic | Report A | Report B | Governing decision |
|---|---|---|---|
| Literal engine identity | Often treats the patent pipeline and EFX 3 implementation as effectively identical; at points equates the EFX 3 path with later Classic behavior. | Separates externally visible patent architecture from unresolved Evo/Auto-Tune 7 detector internals; treats Classic Mode as an Auto-Tune 5 clue. | Preserve the patent-shaped architecture, but label literal EFX 3 internals as PLAUSIBLE RECONSTRUCTION. Classic Mode is secondary evidence only. |
| Target hysteresis | Asserts an 8-cent deadband, 2–3-period confirmation, and up-to-30-ms hold with high confidence. Its pseudocode fixes three frames. | Says memoryless nearest-note is most likely and any deadband/hold is speculative. | Baseline is memoryless. Hysteresis and confirmation default to zero; no historical constants are claimed. |
| Retune smoothing domain | Ranks cents-domain filtering first and period-domain filtering lower in narrative; its pseudocode filters cents. | Reads the patent as one-pole smoothing of the resample-rate ratio, with cents-domain filtering an alternative. | Follow the directive: ratio/period-ratio one-pole is the baseline; cents and slew are experiments. |
| Retune mapping | Supplies formulas and treats milliseconds approximately as `tau`; pseudocode adds a small zero threshold. | Calls exact milliseconds-to-coefficient mapping unresolved. | True zero is exact; every nonzero mapping is a replaceable hypothesis. Do not introduce a hidden minimum. |
| Humanize details | Presents stability-window, dwell, and ramp equations; pseudocode uses 80 ms onset, 150 ms ramp, and an additive 250 ms contribution. | Supports the behavior but marks onset, shape, and Retune-0 interaction unresolved. The directive's illustrative formula is multiplicative. | Preserve only the behavioral intent. Multiplicative and additive composition are separate experiments; all timing, window, and amount constants are EXPERIMENTAL. |
| Tracking epsilon | Suggests an internal aggressive value around 25 / threshold around 0.25. | Supports epsilon semantics but not the EFX 3 numeric mapping. | Normal and Choosy values are configurable reconstruction parameters, never historical facts. |
| Voice ranges and filters | Gives specific lag/range tables and claims filter configurations. | Supports Voice Type changing search bounds but treats exact implementation cautiously. | Use configurable engineering ranges and document their rationale. |
| Splice | Favors Hermite, correlation/zero-crossing alignment, and a roughly 3 ms crossfade in pseudocode. | Supports a short tunable raised-cosine example of 5–20% of period but marks details uncertain. | Zero crossfade is valid and initially central. Interpolator, alignment, curve, sample length, and fraction are separate experiments. |
| Unvoiced behavior | Says the engine may retain the previous ratio briefly and decay to unity; pseudocode instead jumps a delayed read pointer to dry. | Says failed periodicity passes uncorrected at unity. | Baseline is ImmediateUnity with latency continuity. HoldLastRatio and FadeToUnity are disabled experiments. |
| Latency | Makes several incompatible concrete estimates: zero host report, 1–2 periods/3–10 ms, 10–15 ms safety buffer, and specific other-product figures. | Explicitly says no precise official EFX 3 latency is known. | Treat exact EFX 3 latency as unknown. Measure implementation latency in samples. |
| Decimator | Specifies an 8th-order IIR in the behavioral block but uses a single-pole example in pseudocode. | Supports anti-aliased 8:1 patent decimation without claiming a filter design. | Anti-aliasing is required if decimating; topology/order are reconstruction choices verified by alias tests. |
| Resampler direction/pseudocode | Narrative follows patent semantics; pseudocode comments and ratio naming contain potentially inconsistent sharp/flat interpretations. | Defines `r = P_in/P_target` and repeat/delete behavior consistently. | Use the convention in `DSP_NOTES.md` and verify direction with sine tests before relying on comments. |

## Hypothesis registry

Every hypothesis has a stable ID so diagnostic CSVs, renders, and listening notes can name the exact variation.

| ID | Hypothesis | Default | Controls | Discriminating test | Promotion criterion |
|---|---|---|---|---|---|
| HYP-001 | EFX 3 used a nonzero target-note hysteresis deadband. | Disabled, 0 cents | `dbg_noteHysteresisCents` | Slow triangular modulation across a 50-cent boundary; compare up/down switch points. | Repeatable primary-oracle asymmetry not explained by detector error. |
| HYP-002 | A candidate target required multiple accepted periods before switching. | Disabled, 0 periods | `dbg_noteConfirmPeriods` | Slow glissando and abrupt pitch step; measure switch delay in accepted periods. | Consistent period-count delay across levels and F0s on a trusted oracle. |
| HYP-003 | Low-confidence voiced input held the previous target or ratio. | Disabled | `dbg_unvoicedMode`, hold/fade ms | Tone-to-noise and vowel-to-/s/ transitions; inspect rate and output spectrum. | Oracle shows a repeatable non-unity tail distinguishable from detector latency. |
| HYP-004 | Normal/Choosy map directly to two epsilon constants. | Values unset pending calibration | `dbg_trackingEpsilonNormal`, `dbg_trackingEpsilonChoosy` | Tone-plus-noise, breathy synthetic signal, octave-prone harmonics. | Values reproduce acceptance and wrong-octave tradeoff across a corpus. |
| HYP-005 | Retune Speed's displayed milliseconds equal the one-pole `tau`. | Candidate initial mapping only | tau multiplier/map | Pitch step at several displayed values; fit exponential time constants. | Fitted oracle curve matches identity mapping within defined measurement uncertainty. |
| HYP-006 | Retune trajectory updates once per detected period rather than per sample. | Per-sample baseline | update mode/interval | Pitch step and slow glide; inspect staircase microstructure and event timing. | Oracle transition residuals show period-locked updates. |
| HYP-007 | Humanize is triggered by target-note duration alone. | Conservative reconstruction pending calibration | onset/ramp/multiplier/stability window | Long steady note, late-onset vibrato, and stable-target wandering input. | One state definition predicts all oracle vibrato-depth envelopes. |
| HYP-008 | Splices use a nonzero fixed-sample crossfade. | 0 samples | crossfade samples and curve | Sustained saw shifts across sample rates/F0s; inspect discontinuity spectra. | Oracle splice signature scales in samples rather than period fraction. |
| HYP-009 | Splice duration is a fraction of the current period. | Disabled | crossfade period fraction | Repeat HYP-008 across multiple F0s. | Oracle signature scales with period and rejects fixed-sample model. |
| HYP-010 | Splice alignment uses nearest zero crossing or correlation point. | Raw jump baseline | alignment mode/search span | Known asymmetric waveform with logged splice events. | Event phase distribution matches one alignment rule robustly. |
| HYP-011 | Cubic Hermite or 4-point Lagrange was the historical interpolator. | No historical default claimed | interpolation mode | High-frequency sine sweeps and harmonic signals; compare residual spectra. | Trusted oracle residuals favor one response over linear and alternatives. |
| HYP-012 | The EFX 3 detector literally uses patent E/H recursion and 8:1 decimation. | Reference difference detector first | detector implementation, decimation | Detector parity plus marginal/breathy/octave-prone corpus. | Oracle behavior and verified primary evidence both support literal identity. |
| HYP-013 | A specific fixed latency characterizes EFX 3. | Unknown | implementation buffer/latency mode | Impulse and correlated dry/processed onset measurement. | Reproducible measurement from a verified EFX 3 instance and settings. |
| HYP-014 | Later Classic Mode is a sufficiently close sonic proxy. | Secondary comparison only | none in baseline | Identical corpus through Classic Mode, known EFX 3, and reconstruction. | Multidimensional results show equivalence within preregistered tolerances. |
| HYP-015 | Humanize composes multiplicatively with the base Retune time constant. | Unresolved; selectable | `dbg_humanizeTauMode=Multiplicative`, onset/ramp/factor | Retune 0, Humanize 0 versus nonzero, sustained vowel with late-onset vibrato. | Verified oracle remains fully immediate at Retune 0 despite nonzero Humanize, while nonzero Retune cases fit the same factor model. |
| HYP-016 | Humanize adds a sustained-note time constant to the base Retune time constant. | Unresolved; selectable | `dbg_humanizeTauMode=Additive`, onset/ramp/additional tau | Same paired Retune-0 render as HYP-015; measure post-onset trajectory and vibrato depth. | Verified oracle shows Humanize-dependent relaxation at Retune 0 that the multiplicative model cannot produce. |

## Product requirements that are not historical claims

- The original three-state Target/Bypass/Remove Note Matrix and its accessibility behavior.
- A collapsible Developer/Forensic panel.
- Diagnostic CSV export with the fields specified in the directive.
- Multiple interpolation and splice modes.
- Deterministic synthetic-signal generation and offline rendering.
- AU and VST3 delivery on macOS Apple Silicon.

These requirements may be discussed as design facts, but comments and documentation must not imply that the EFX 3 UI or DSP exposed the same features.

## Evidence promotion procedure

Before changing a classification or baseline:

1. Record the source or oracle, exact product/version, sample rate, settings, host, and signal.
2. Preserve the dry input and raw processed output losslessly.
3. State the competing hypotheses before inspecting the result.
4. Use a measurement that could falsify the preferred hypothesis.
5. Repeat across more than one F0, level, and signal class when detector behavior is involved.
6. Separate measurement uncertainty from algorithm variability.
7. Update this ledger, `ARCHITECTURE.md`, tests, and developer defaults together.

Subjective preference can justify keeping an experiment as a creative option. It cannot, by itself, establish historical provenance.
