# Architecture

## Status and authority

This document is a normative architecture contract for an original, clean-room pitch-correction project. It is not a claim about Antares source code or undocumented implementation details.

When sources conflict, use this order:

1. `DIRECTIVE_FOR_CODEX.md`
2. `docs/ARCHITECTURE.md` and `docs/EVIDENCE_AND_HYPOTHESES.md`
3. The supplied research reports
4. Engineering inference

The research reports are reference material. Their proposed constants and pseudocode are not implementation requirements.

The project uses these provenance labels:

- **EVIDENCE-BASED** — supported by strong public evidence such as the EFX 3 manual, Hildebrand patent, or Hildebrand's public technical statement.
- **PLAUSIBLE RECONSTRUCTION** — consistent with the evidence and DSP constraints, but the exact historical implementation is unknown.
- **EXPERIMENTAL** — an intentionally selectable hypothesis for measurement or listening tests; it is not part of the historical baseline unless evidence later supports it.
- **PRODUCT REQUIREMENT** — required by this project for usability or research, without implying that EFX 3 behaved this way.

Every implementation choice that is not plainly mechanical must carry one of these labels in its design notes or nearby code comments. Numeric reconstruction parameters must not be described as historical values unless a primary source establishes them.

## Objective

Build two things in deliberate sequence:

1. A usable low-latency monophonic vocal pitch corrector.
2. A research instrument for testing why older Auto-Tune-family processing sounded rigid, bright, fast, sticky, and occasionally imperfect.

The target is characteristic behavior rather than transparent modern correction. The historically motivated baseline must remain available unchanged when experimental or modernized alternatives are added.

## Platform and delivery

- macOS on Apple Silicon (`arm64`)
- C++20 or the current practical C++ standard
- CMake and JUCE
- AU and VST3 for V1
- Internally mono initially
- Stereo policy may begin as left-channel processing duplicated to both outputs; independent dual-mono is deferred

## Normative signal path

```text
input
  -> light preprocessing
  -> periodicity and voiced detection
  -> period / F0 estimation
  -> octave arbitration
  -> logarithmic note-domain conversion
  -> scale and note-matrix targeting
  -> Retune Speed trajectory
  -> Humanize modulation
  -> period-synchronous variable-rate resampler
       -> whole-cycle repetition when the read head advances too far
       -> whole-cycle deletion when the read head falls too far behind
  -> output
```

The baseline must not use:

- phase-vocoder or STFT pitch correction
- PSOLA or TD-PSOLA as the primary shifter
- granular or WSOLA shifting
- spectral-envelope or automatic formant preservation
- throat modeling
- Flex-Tune-style tolerance
- melody or transition prediction
- neural pitch detection or other ML inference

The current scope also excludes automatic key detection, harmony generation, MIDI note control, graphical pitch editing, a vocal-effects rack, oversampling without demonstrated need, elaborate preset management, cloud services, telemetry, authentication, copy protection, an updater, and a fancy installer.

## Phase boundaries

### V0A — offline cycle resampler

V0A isolates the sonic core. It accepts a mono WAV, a manual fundamental period or F0, and a manual pitch ratio. It contains no automatic detector, scale logic, Retune Speed, plugin wrapper, or UI.

Required components:

- bounded circular buffer
- fractional read pointer
- continuous variable-rate resampling
- whole-cycle repetition and deletion
- selectable Linear, Cubic Hermite, and 4-point Lagrange interpolation
- selectable splice treatment, including exactly zero crossfade samples
- deterministic offline rendering and diagnostics

V0A is complete only when time remains synchronized through repeat/delete events, buffer invariants hold, and the output exposes the intended primitive period-synchronous character.

### V0B — offline hard tuner

V0B adds the transparent reference pitch detector, octave arbitration, chromatic nearest-note targeting, Retune Speed 0, and voiced/unvoiced handling. It remains offline and deterministic.

The baseline target selector is memoryless. Hysteresis and confirmation counts remain disabled experiments.

### V1 — JUCE plugin

V1 wraps the verified engine as AU and VST3 and adds Key, Scale, Retune Speed, Humanize, Voice Type, Tracking Normal/Choosy, Note Matrix, and Bypass. UI polish is subordinate to DSP correctness.

### V1.5 — developer/forensic panel

Uncertain detector, targeting, trajectory, splice, and unvoiced parameters become inspectable and selectable without changing the normal-user baseline.

### V2 — forensic refinement

V2 compares measurable output behavior against public demonstrations, historical bounces, dry/processed pairs when available, Auto-Tune 7-era material, and later Classic Mode only as secondary evidence. Aesthetic preference alone does not promote an experiment into the baseline.

## Component boundaries

| Component | Responsibility | Consumes | Produces |
|---|---|---|---|
| Preprocessor | Optional DC/light filtering and anti-aliased analysis decimation | Input samples, sample rate | Full-rate audio plus optional analysis stream |
| PitchDetector | Estimate period and periodicity without musical smoothing | Analysis samples, search bounds, thresholds | `PitchDetection` |
| OctaveResolver | Compare fundamental candidates such as `L` and `2L` | Detector candidates and metrics | Accepted period or rejection |
| NoteQuantizer | Apply pitch-class modes and find a target | Detected pitch, key/scale/note matrix | Target pitch or unity-correction decision |
| RetuneController | Convert raw period ratio into the correction trajectory | Input period, target period, Retune Speed | Smoothed resample rate |
| HumanizeController | Slow correction selectively on sustained notes | Base time constant, target duration/stability, Humanize | Effective time constant |
| CycleResampler | Perform fractional reading and period-sized pointer correction | Audio stream, rate, period, splice/interpolation configuration | Shifted audio and splice counters |
| Diagnostics | Publish bounded numerical state without touching the real-time filesystem | Snapshot fields | Atomically readable UI data or offline CSV |

The initial detector result contract is:

```cpp
struct PitchDetection
{
    double periodSamples;
    double frequencyHz;
    double periodicity;
    double confidence;
    bool voiced;
};
```

Period in full-rate samples is the canonical detector/resampler representation. Frequency, MIDI, and cents are derived views.

## Note-matrix contract

The one-octave matrix has 12 pitch classes and three DSP-distinct modes:

```cpp
enum class NoteMode
{
    Target,
    Bypass,
    Remove
};
```

- **Target:** an eligible correction destination.
- **Bypass:** owns its ordinary ±50-cent chromatic cell and explicitly produces unity correction for input in that cell; it is not a correction destination for other cells.
- **Remove:** never an eligible target and owns no bypass capture region; input in its ordinary chromatic cell proceeds to the nearest remaining Target note.

First assign the detected logarithmic pitch to its nearest chromatic note cell, independent of the Target set. Bypass is evaluated from that cell before nearest-Target search. Remove and Bypass must never collapse to the same internal state. These semantics and the three-state original interface are **PRODUCT REQUIREMENTS**, not established historical EFX 3 behavior.

Scale presets initialize the matrix. Any subsequent manual edit places it in Custom state. Changing Key or Scale loads the selected preset and clears prior custom overrides.

## Baseline state behavior

- Pitch targeting is memoryless nearest enabled Target in logarithmic pitch space.
- Bypass ownership uses fixed ordinary chromatic cells, not Voronoi regions derived from the currently enabled Target notes.
- Adjacent chromatic targets have a nominal midpoint at 50 cents.
- `dbg_noteHysteresisCents = 0` and `dbg_noteConfirmPeriods = 0` disable target persistence.
- Retune Speed 0 applies the raw period ratio immediately; there is no artificial minimum time constant.
- A target change does not reset the trajectory state.
- Unreliable/unvoiced input uses unity correction and a latency-matched dry or unity-rate resampled path.
- The baseline shifter does not preserve formants.
- A splice crossfade of zero samples is valid.
- For stationary period `P` and constant rate `r`, long-run repeat/delete event density converges to `abs(r - 1) / P`; bounded pointers alone are not sufficient evidence of correct drift compensation.

## Real-time rules

Inside `processBlock`:

- no heap allocation or dynamic container growth
- no locks
- no filesystem access or disk logging
- no GUI calls
- no unbounded work
- parameters are read atomically or through a real-time-safe snapshot

Allocate buffers in `prepareToPlay`. Publish diagnostics through fixed-size lock-free or atomic state. Report actual fixed host latency with `AudioProcessor::setLatencySamples`; do not claim zero latency merely because the host reports none for a historical product.

## Developer/forensic control surface

V1.5 exposes uncertainty without cluttering the normal UI. At minimum it groups:

- **Detector:** minimum/maximum F0, decimation factor, Normal/Choosy periodicity epsilon, energy threshold, pitch-update interval, and octave-jump resistance.
- **Targeting:** hysteresis cents, confirmation periods, and confidence threshold.
- **Retune:** smoothing domain, time-constant mapping multiplier, minimum time constant, and update mode. The historical baseline still permits a true zero.
- **Humanize:** onset, ramp, multiplier/additional time constant, stability window, and Multiplicative/Additive time-constant composition mode.
- **Resampler:** interpolator, splice crossfade samples/fraction, and alignment mode.
- **Unvoiced:** ImmediateUnity/HoldLastRatio/FadeToUnity mode plus hold/fade durations.
- **Formant:** preservation amount only if implemented, defaulting to exactly zero.
- **Diagnostics:** detected F0/MIDI/cents, period, periodicity, confidence, voiced state, target/note mode, raw/smoothed ratio, and repeat/delete counts.

No developer value is historical merely because it has a useful default.

## Confidence table

| ID | Architectural statement | Status | Basis and consequence |
|---|---|---|---|
| A01 | EFX 3-class correction is monophonic and tracks the closest enabled scale note. | EVIDENCE-BASED | EFX 3 manual behavior; implement monophonic tracking and nearest-note geometry. |
| A02 | The core shifter is a simple rate converter with whole-cycle repeat/delete, not PSOLA. | EVIDENCE-BASED | Hildebrand patent and 2014 technical statement; this is the baseline resynthesizer. |
| A03 | Period estimation is time-domain, autocorrelation/difference-family, with energy-relative periodicity. | EVIDENCE-BASED | Patent architecture; exact EFX 3/Evo internals remain unresolved. |
| A04 | A roughly two-period comparison, quadratic lag refinement, and `L` versus `2L` arbitration are historically motivated. | EVIDENCE-BASED for patent architecture; PLAUSIBLE RECONSTRUCTION for literal EFX 3 implementation | Implement transparently first and retain replaceable detector boundaries. |
| A05 | An anti-aliased approximately 8:1 decimated analysis path is historically motivated. | EVIDENCE-BASED for the patent; PLAUSIBLE RECONSTRUCTION for EFX 3 | Provide as an optional baseline detector path; do not claim Evo used the identical filter. |
| A06 | Retune Speed uses first-order smoothing of the resample-rate ratio and 0 means immediate replacement. | EVIDENCE-BASED | Patent recursion and manual behavior; use ratio-domain one-pole by default. |
| A07 | The exact Retune Speed value-to-time-constant mapping is known. | EXPERIMENTAL | It is not known; keep the mapping replaceable and record the assumption. |
| A08 | Humanize slows correction selectively on sustained notes. | EVIDENCE-BASED | Manual behavior. Onset, stability measure, ramp, composition rule, and Retune-0 interaction are not established. |
| A09 | EFX 3 used a specific target deadband, confirmation count, or voiced hold duration. | EXPERIMENTAL | Report A asserts constants that Report B disputes and primary evidence does not establish. Defaults remain disabled. |
| A10 | Tracking controls allowed cycle-to-cycle waveform variation; Choosy is stricter. | EVIDENCE-BASED behavior; PLAUSIBLE RECONSTRUCTION mapping | Map to periodicity acceptance conservatively; numeric epsilon values are experimental. |
| A11 | Voice Type changes detector search constraints. | EVIDENCE-BASED behavior; PLAUSIBLE RECONSTRUCTION ranges | Store ranges as configuration and expose them in Developer Mode. |
| A12 | The baseline has no formant correction or throat model, so resampling moves the spectral envelope. | EVIDENCE-BASED | Preserve this behavior; any later preservation amount defaults to zero and is isolated. |
| A13 | Exact interpolation and splice alignment/crossfade are known. | EXPERIMENTAL | Compare Linear, Hermite, Lagrange and zero/short crossfades; cleaner is not presumed more authentic. |
| A14 | Unvoiced input immediately returns to unity correction. | PLAUSIBLE RECONSTRUCTION selected by project directive | Default to ImmediateUnity; hold/fade alternatives are experiments. |
| A15 | Exact EFX 3 latency is known or truly zero. | EXPERIMENTAL / UNKNOWN | Measure this implementation and report it accurately; do not copy unsupported historical claims. |
| A16 | Target/Bypass/Remove note modes and this project's keyboard UI are required without historical attribution. | PRODUCT REQUIREMENT | Implement their stated DSP/accessibility semantics without imitating Antares visuals. |
| A17 | Later Classic Mode is an exact oracle for EFX 3. | EXPERIMENTAL secondary comparison | It models Auto-Tune 5, not EFX 3; use only as a tonal/transition clue. |

## Change discipline

A change may enter the historical baseline only when it:

1. follows the directive;
2. is supported by primary evidence or clearly identified patent behavior;
3. passes deterministic regression tests; and
4. does not silently enable an experimental parameter.

Experiments must have explicit defaults, diagnostics, and a test capable of distinguishing them from the baseline.
