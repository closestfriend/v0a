You are an expert C++ audio-DSP engineer with deep experience in JUCE, real-time audio, pitch detection, resampling, plugin architecture, and scientific/forensic software development.

Build an original clean-room macOS audio plugin that reconstructs the characteristic correction behavior of the circa-2014 Antares Auto-Tune EFX 3 generation as closely as can reasonably be inferred from public technical evidence.

This is NOT a request to copy Antares source code, branding, UI, assets, presets, proprietary binaries, or protected implementation details.

The objective is to implement an ORIGINAL plugin based on:

- publicly documented DSP principles,
- Harold/Andy Hildebrand's public pitch-correction patent,
- published Antares documentation,
- public technical statements,
- and experimentally testable hypotheses derived from forensic research.

The project should be treated as BOTH:

1. a usable low-latency monophonic vocal pitch corrector, and
2. a research instrument for exploring why older Auto-Tune-family processing had its particular hard, bright, "sticky" character.

The cardinal rule is:

DO NOT "IMPROVE" THE OLD DSP BY DEFAULT.

If a modern enhancement is useful, it must either:
- live behind an explicit developer/debug option, or
- be isolated so that the historically motivated baseline remains available unchanged.

We are optimizing for CHARACTERISTIC BEHAVIOR, not state-of-the-art pitch-correction quality.


==================================================
1. TARGET PLATFORM / BUILD
==================================================

Target:

- macOS
- Apple Silicon / arm64
- C++20 or current practical C++ standard
- JUCE
- CMake
- AU
- VST3

Initial processing can be internally mono.

Stereo instances may process:
- left channel only and duplicate,
- or independent dual-mono later.

Do not spend significant effort on visual polish until DSP V1 works.


==================================================
2. HIGH-CONFIDENCE DSP ARCHITECTURE
==================================================

Implement this conceptual signal path:

INPUT
  |
  v
light preprocessing
  |
  v
periodicity / voiced detection
  |
  v
period/F0 estimator
  |
  v
octave arbitration
  |
  v
pitch -> note-domain conversion
  |
  v
scale / note-matrix target logic
  |
  v
Retune Speed trajectory
  |
  v
Humanize modulation
  |
  v
period-synchronous variable-rate resampler
  |
  +--> whole-cycle repetition when required
  |
  +--> whole-cycle deletion when required
  |
  v
OUTPUT


The baseline must NOT use:

- phase-vocoder pitch shifting
- STFT-based pitch correction
- PSOLA / TD-PSOLA as the primary shifter
- spectral-envelope preservation
- automatic formant correction
- throat modeling
- Flex-Tune-style "close enough" tolerance
- neural pitch detection
- ML inference

The unusual part of the architecture is intentional.


==================================================
3. DEVELOPMENT PHILOSOPHY
==================================================

Keep three categories separate throughout the codebase and comments:

A. EVIDENCE-BASED
Strong public evidence supports this behavior.

B. PLAUSIBLE RECONSTRUCTION
Strong DSP inference, but exact implementation unknown.

C. EXPERIMENTAL
An intentionally exposed hypothesis for listening/testing.

Never write comments such as:

    // EFX3 used 15 ms hysteresis

unless evidence genuinely establishes that.

Instead write:

    // Experimental hypothesis: small target-note hysteresis.
    // No strong evidence that EFX3 used this.

Create docs/ARCHITECTURE.md containing a confidence table.

This epistemic separation is a core project requirement.


==================================================
4. PROJECT PHASES
==================================================

Do NOT immediately build the entire plugin.

Build and verify the engine in these stages.


------------------------------
PHASE V0A — "CURSED RESAMPLER"
------------------------------

Build an OFFLINE command-line/audio-test implementation first.

No automatic pitch detector.
No scale logic.
No Retune Speed.
No plugin UI.

Inputs:

- mono WAV
- known/manual fundamental period OR manual F0
- manual pitch ratio

Implement:

- circular/ring buffer
- fractional read pointer
- variable-rate resampling
- whole-cycle repetition
- whole-cycle deletion
- selectable interpolation method
- selectable cycle-splice treatment

Example ratios:

1.00000
1.03000
1.059463   // +1 semitone
0.943874   // -1 semitone

The objective is to hear and inspect the character of the resynthesis algorithm in isolation.

IMPORTANT:

Do NOT automatically add a sophisticated splice crossfade.

Developer parameter:

    spliceCrossfadeSamples = 0

must be valid.

Test:

0
2
4
8
16 samples
and percentage-of-period variants.

Also support multiple interpolators:

    Linear
    Cubic Hermite
    4-point Lagrange

Start from the simplest credible interpolation.

The agent must not assume that cleaner interpolation is more authentic.


------------------------------
PHASE V0B — OFFLINE HARD TUNER
------------------------------

Add:

- pitch detector
- nearest-note target selection
- chromatic scale
- Retune Speed = 0 only
- voiced/unvoiced handling

Required tests:

1. sine glissando
   -> hard semitone staircase

2. saw glissando
   -> same pitch staircase

3. synthesized vowel glissando
   -> staircase plus moving formants

4. controlled vibrato
   -> strong flattening at Retune 0

5. white noise
   -> bypass / no arbitrary pitched correction

6. +/-49 and +/-51 cent tests
   -> target boundary behavior around midpoint


------------------------------
PHASE V1 — REAL JUCE PLUGIN
------------------------------

Once V0 works:

Create AU + VST3 plugin.

User controls:

- Key
- Scale
- Retune Speed
- Humanize
- Voice Type
- Tracking: Normal / Choosy
- Keyboard Display / Note Matrix
- Bypass

No Auto-Motion.
No harmony generator.
No Auto-Key.
No graphical correction editor.
No throat model.
No formant correction.
No AI features.

Keep this thing focused.


------------------------------
PHASE V1.5 — DEVELOPER MODE
------------------------------

Add a collapsible Developer / Forensic panel.

Expose uncertain DSP parameters so they can be tuned by listening and measurement.

Do not clutter the normal UI with these controls.


------------------------------
PHASE V2 — FORENSIC REFINEMENT
------------------------------

Use:

- historical vocal bounces known to use EFX3
- any available dry/processed stem pairs
- public EFX3 demonstrations
- Auto-Tune 7-era recordings
- later Classic Mode only as secondary evidence

Compare:

- F0 trajectories
- transition timing
- detector behavior
- formant motion
- cycle-splice artifacts
- consonant handling
- vibrato attenuation

Do not optimize merely by subjective "sounds good."

Prefer measurable hypotheses.


==================================================
5. PITCH DETECTOR
==================================================

The baseline detector should be a short-window TIME-DOMAIN period detector inspired by the public Hildebrand patent architecture.

Pitch is represented primarily as waveform PERIOD in samples.

Implement a transparent reference detector first.

For candidate lag L:

    compare successive waveform periods
    calculate a difference / autocorrelation-derived metric
    find the best lag
    test periodicity relative to signal energy
    refine period with quadratic interpolation

Conceptually:

    D(L) = E(L) - 2H(L)

Periodic if approximately:

    D(L) <= epsilon * E(L)

Use roughly a two-period analysis behavior.

A historically motivated detector path should optionally use an anti-aliased ~8:1 decimated analysis stream.

IMPORTANT:

For the first working implementation, a naive O(N*L) calculation is acceptable.

DO NOT prematurely optimize into complicated recursive accumulators.

Correctness and inspectability come first.

Later implement the recursive E/H form as an optimized detector and verify both implementations against identical test signals.


Detector result structure:

struct PitchDetection
{
    double periodSamples;
    double frequencyHz;
    double periodicity;
    double confidence;
    bool voiced;
};


Support:

- minimum lag
- maximum lag
- energy threshold
- periodicity epsilon
- quadratic/sub-sample lag refinement


==================================================
6. OCTAVE ARBITRATION
==================================================

Implement a candidate L vs 2L fundamental-period comparison.

The detector should not blindly pick the strongest harmonic.

Expose developer controls for:

- octave-jump resistance
- candidate octave preference
- minimum confidence required for octave changes

However:

Do not introduce heavy temporal intelligence in the baseline.

"Choosy" should primarily make period acceptance stricter / more conservative rather than secretly invoking a giant modern pitch-tracking state machine.


==================================================
7. VOICE TYPE
==================================================

Normal UI presets:

- Soprano
- Alto / Tenor
- Low Male
- Instrument
- Bass Instrument if useful

These primarily alter detector period/frequency search bounds.

Do NOT hardcode undocumented frequency values as historical truth.

Store the actual ranges in configuration and expose them in Developer Mode.

Example architecture:

struct VoiceRange
{
    double minimumHz;
    double maximumHz;
};


==================================================
8. TRACKING — NORMAL / CHOOSY
==================================================

Implement Tracking primarily as periodicity tolerance.

Conceptually:

    periodic = difference <= epsilon * energy;

Normal and Choosy map to different epsilon values.

Expected relationship:

    epsilonChoosy < epsilonNormal

because Choosy should demand greater waveform periodicity / similarity.

Actual numeric values are reconstruction parameters.

Expose:

    dbg_trackingEpsilonNormal
    dbg_trackingEpsilonChoosy

Do not pretend we know the exact EFX3 constants.


==================================================
9. NOTE REPRESENTATION / SCALE SYSTEM
==================================================

Internally represent pitch logarithmically.

Useful conversion:

    midi = 69 + 12 * log2(f / 440)

or cents equivalent.

Implement:

- chromatic pitch classes
- major
- natural minor
- common scale presets
- custom note state matrix

Scale presets should merely initialize the note matrix.

After loading a scale, individual pitch classes remain editable.


==================================================
10. KEYBOARD DISPLAY / NOTE MATRIX
==================================================

THIS IS AN IMPORTANT USER-FACING FEATURE.

We do NOT want to visually copy Auto-Tune EFX 3.

Design an original note-selection interface using a compact one-octave keyboard / note matrix representing all 12 chromatic pitch classes:

C
C#
D
D#
E
F
F#
G
G#
A
A#
B

It should visually read as a keyboard, but use an original graphic style.

Required state model:

enum class NoteMode
{
    Target,
    Bypass,
    Remove
};


------------------------------
TARGET
------------------------------

This pitch class is an enabled correction target.

Visual requirement:

- BLUE / accent-colored key
- clearly active

If detected pitch lies in its quantization region, pitch correction may target this note.


------------------------------
BYPASS
------------------------------

This pitch class remains a recognized pitch region, but when the detected input belongs to this note region the audio passes WITHOUT pitch correction.

Visually:

- normal white/black keyboard coloring
- clear secondary indicator that state is BYPASS

This is semantically different from Remove.


------------------------------
REMOVE
------------------------------

This note is NOT an available pitch target.

Its pitch territory is therefore captured by the nearest remaining valid target note(s).

Visually:

- clearly muted / crossed / dimmed / hollowed
- but still legible as the underlying chromatic pitch

Do not simply hide the note.


------------------------------
INTERACTION
------------------------------

Each key must be clickable.

A click may cycle:

    Target -> Bypass -> Remove -> Target

OR use another very obvious interaction if more usable.

If cycling states, show the current state clearly.

Accessibility:

- each key must also be keyboard-focusable
- expose pitch name and state to accessibility APIs
- tooltips/status text can say:
    "F — Removed"
    "F# — Target"
    "G — Bypass"

Scale presets initialize states.

Example:

Key: E
Scale: Natural Minor

Target:
    E F# G A B C D

Non-scale notes may initialize as Remove.

The user must then be able to override any individual pitch class manually.


------------------------------
DSP SEMANTICS
------------------------------

Do NOT collapse Bypass and Remove into the same operation.

Pseudo-logic:

Detected pitch
    |
    +--> falls inside BYPASS pitch region?
    |        |
    |        +--> output pitch correction ratio = unity
    |
    +--> otherwise
             |
             +--> find nearest TARGET note
             |
             +--> REMOVE notes are never selected


Because this behavior is an explicit PRODUCT REQUIREMENT rather than something we have fully established historically, document it as such.

The UI should NOT mimic Antares:
- no copied layout
- no copied typography
- no copied knob appearance
- no copied icons
- no copied branding
- no copied color scheme beyond the requested semantic use of blue for active target notes


==================================================
11. TARGET-NOTE QUANTIZATION
==================================================

Baseline historical hypothesis:

Memoryless nearest enabled target note.

Do:

    target = nearestTargetNote(detectedPitch);

For adjacent chromatic target notes, nominal midpoint is approximately 50 cents.

Do NOT add hidden musical intelligence.

Do NOT add Flex-Tune behavior.

Do NOT add melody prediction.

Do NOT add note-transition prediction.


Developer options:

    dbg_noteHysteresisCents = 0
    dbg_noteConfirmPeriods = 0

Default values mean DISABLED.

This matters.

There is currently insufficient evidence that EFX3 itself used a substantial target-note hysteresis state machine.

We want the ability to EXPERIMENT with one later without baking the hypothesis into the baseline.


==================================================
12. RETUNE SPEED
==================================================

Use the patent-motivated baseline:

rawRate = inputPeriod / targetPeriod

Then smooth the RESAMPLE RATE RATIO using a first-order filter.

Conceptually:

    alpha = exp(-dt / tau)

    rate[n] =
        alpha * rate[n-1]
        + (1-alpha) * rawRate[n]

Retune Speed 0:

    alpha = 0
    rate = rawRate immediately

No artificial 1 ms minimum.

No hidden transient smoothing.

Do not reset smoothing state merely because the target note changed unless experimental evidence later supports it.


Developer selectable alternatives:

enum class RetuneDomain
{
    PeriodRatio,
    Cents,
    LinearSlew
};

Default:

    PeriodRatio

The alternatives exist for forensic comparison, not because they are assumed historical.


Unknown:

The exact mapping between the EFX3 Retune Speed numeric value and mathematical time constant tau.

Therefore implement the mapping cleanly and make it replaceable.

Possible initial assumption:

    tau ~= retuneSpeedMilliseconds

but clearly label this as a reconstruction hypothesis.


==================================================
13. HUMANIZE
==================================================

High-confidence behavioral goal:

Humanize keeps short-note correction fast while allowing sustained notes to use effectively slower correction.

Implement conservatively:

    effectiveTau = baseTau;

    if (sameTargetDuration > humanizeOnset)
        effectiveTau *= humanizeFunction(...);


Unknowns:

- exact sustain threshold
- whether stability is measured from input F0 or target identity
- step vs ramp
- ramp duration
- maximum slowdown
- exact interaction with vibrato

Do not hardcode speculative constants as historical values.

Expose:

    dbg_humanizeOnsetMs
    dbg_humanizeRampMs
    dbg_humanizeMultiplier
    dbg_humanizeStabilityWindowCents


==================================================
14. PERIOD-SYNCHRONOUS RESAMPLER
==================================================

THIS IS THE CORE SONIC COMPONENT.

Maintain at least:

- circular audio buffer
- write/input pointer
- fractional read/output pointer
- current detected period
- current resample rate


Conceptually:

    readPosition += rate;

When temporal alignment requires the output to reuse a cycle:

    readPosition -= cyclePeriod;

When temporal alignment requires a cycle to be skipped:

    readPosition += cyclePeriod;


Do this at period-synchronous boundaries as robustly as possible.

Do NOT replace this mechanism with PSOLA.

Do NOT replace it with grains/WSOLA.

Do NOT replace it with a phase vocoder.


==================================================
15. FORMANTS
==================================================

BASELINE:

NO formant preservation.

Variable-rate resampling should naturally move the spectral envelope with pitch.

This is intentional.

Developer Mode may eventually contain:

    dbg_formantPreservation = 0.0

but there is no need to implement a formant-preserving system in early versions.

If implemented later, 0.0 must retain the baseline engine unchanged.


==================================================
16. CYCLE SPLICES
==================================================

Exact splice treatment is uncertain.

Therefore expose the behavior.

Developer parameters:

    dbg_spliceCrossfadeSamples
    dbg_spliceCrossfadeFractionOfPeriod
    dbg_spliceAlignmentMode

Crossfade = 0 MUST be supported.

Possible alignment experiments:

- raw period jump
- nearest zero crossing
- nearest waveform correlation point

Do not assume the most transparent variant is the correct one.


==================================================
17. INTERPOLATION
==================================================

Support:

- Linear
- Cubic Hermite
- 4-point Lagrange

Expose in Developer Mode.

Do not assume the fanciest interpolation sounds most authentic.

All methods must be safe under real-time processing.


==================================================
18. UNVOICED / CONSONANT HANDLING
==================================================

Baseline:

If no reliable periodic pitch is detected:

    correction ratio -> unity
    output -> delayed dry / unity-rate resampled signal

Consonants, breath and noise should therefore generally escape pitch quantization.

Developer alternatives:

enum class UnvoicedMode
{
    ImmediateUnity,
    HoldLastRatio,
    FadeToUnity
};

Controls:

    dbg_unvoicedMode
    dbg_unvoicedHoldMs
    dbg_unvoicedFadeMs

Default:

    ImmediateUnity

Again: alternatives are hypotheses.


==================================================
19. LATENCY
==================================================

Optimize for low latency, but do NOT make unsupported claims such as "exactly zero latency."

Detector latency inherently depends on observing waveform periods.

Target practical behavior:

approximately a few pitch periods.

Use:

    AudioProcessor::setLatencySamples(...)

accurately.

Do not lie to the DAW about latency.


==================================================
20. DEVELOPER / FORENSIC PANEL
==================================================

Expose at minimum:

DETECTOR
- minimum F0
- maximum F0
- decimation factor
- periodicity epsilon
- energy threshold
- pitch update interval
- octave-jump resistance

TARGETING
- hysteresis cents
- note confirmation periods
- confidence threshold

RETUNE
- smoothing domain
- tau mapping multiplier
- minimum tau
- update mode

HUMANIZE
- onset ms
- ramp ms
- multiplier
- stability window cents

RESAMPLER
- interpolator
- cycle splice crossfade samples
- cycle splice crossfade fraction
- alignment mode

UNVOICED
- mode
- hold ms
- fade ms

FORMANT
- preservation amount, if ever implemented
- DEFAULT = 0

DIAGNOSTICS
- current detected F0
- detected MIDI/cents
- confidence
- periodicity
- target pitch
- note mode
- raw correction ratio
- smoothed ratio
- voiced/unvoiced
- number of repeated cycles
- number of deleted cycles


==================================================
21. TEST SIGNAL GENERATOR
==================================================

Create tools/generate_test_signals.py OR an equivalent C++ utility.

Generate:

sine_gliss_C4_D4.wav
saw_gliss_C4_D4.wav

sine_minus49c.wav
sine_plus49c.wav
sine_minus51c.wav
sine_plus51c.wav

vibrato_5hz_30c.wav
vibrato_5hz_100c.wav

pitch_step_100c.wav
pitch_step_700c.wav
pitch_step_octave.wav

vowel_a_gliss.wav
vowel_i_gliss.wav

tone_plus_noise.wav
vowel_to_s.wav
s_to_vowel.wav

amplitude_ramp_constant_f0.wav

If practical:
vocal_fry_synthetic.wav


==================================================
22. DIAGNOSTIC EXPORT
==================================================

Offline processing must optionally emit CSV:

time_seconds
input_f0_hz
input_pitch_cents
period_samples
periodicity
confidence
voiced
note_region
note_mode
target_midi
target_f0_hz
raw_ratio
smoothed_ratio
output_f0_estimate
cycle_repetitions
cycle_deletions

This is essential.

We need to diagnose the system numerically rather than tuning everything by vague listening.


==================================================
23. AUTOMATED TESTS
==================================================

Add tests for:

- Hz <-> MIDI conversion
- cents conversion
- nearest target note
- removed notes never selected
- bypass notes produce unity correction
- scale preset initialization
- custom note overrides
- 49/51 cent boundary
- Retune 0 immediate response
- one-pole convergence
- detector on clean sine
- detector octave handling
- unvoiced rejection
- ring-buffer safety
- no allocations in processBlock
- no out-of-bounds reads
- deterministic offline renders


==================================================
24. SUGGESTED REPOSITORY STRUCTURE
==================================================

/
  CMakeLists.txt
  README.md

  docs/
    ARCHITECTURE.md
    DSP_NOTES.md
    EVIDENCE_AND_HYPOTHESES.md
    TESTING.md

  src/
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h

    dsp/
      PitchDetector.cpp
      PitchDetector.h
      OctaveResolver.cpp
      OctaveResolver.h
      NoteQuantizer.cpp
      NoteQuantizer.h
      RetuneController.cpp
      RetuneController.h
      HumanizeController.cpp
      HumanizeController.h
      CycleResampler.cpp
      CycleResampler.h
      RingBuffer.cpp
      RingBuffer.h
      Interpolators.h

    model/
      Scale.cpp
      Scale.h
      NoteMatrix.cpp
      NoteMatrix.h
      VoiceRange.cpp
      VoiceRange.h

    ui/
      NoteMatrixComponent.cpp
      NoteMatrixComponent.h
      DeveloperPanel.cpp
      DeveloperPanel.h

  tools/
    offline_renderer/
    signal_generator/

  tests/
    dsp/
    signals/
    renders/


==================================================
25. UI DIRECTION
==================================================

Do NOT replicate Auto-Tune EFX3 visually.

The plugin should feel like an original technical instrument.

Priorities:

- simple
- flat
- legible
- compact
- no glossy skeuomorphic plugin design
- no giant photoreal knobs
- no visual clutter

Primary visual object:

THE 12-NOTE KEYBOARD / NOTE MATRIX.

It should immediately communicate:

- what notes are legal targets
- what notes bypass correction
- what notes are removed

Use blue/accent color for enabled target keys.

Preserve natural white/black keyboard distinction where practical for bypass states.

Removed notes should remain spatially visible but clearly unavailable.

Key and Scale selectors should update the note matrix immediately.

Manual note edits should not mysteriously disappear.

If the user changes Key/Scale after manual editing, choose a predictable policy:
- load the new preset and reset custom overrides,
OR
- prompt/preserve custom mode.

For V1, simplest predictable policy:
changing Key or Scale loads that scale preset;
manual edits after that place the matrix in "Custom" state.


==================================================
26. REAL-TIME AUDIO RULES
==================================================

Inside processBlock:

- no heap allocations
- no locks
- no filesystem work
- no logging to disk
- no GUI calls
- no dynamic container resizing
- parameters read atomically / safely

Prepare buffers in prepareToPlay.

Keep diagnostic data in lock-free/simple atomically readable structures for UI visualization.


==================================================
27. SUCCESS CRITERIA
==================================================

V0A succeeds when:

- manual pitch shifting works continuously
- waveform duration remains synchronized through cycle repeat/delete
- no crashes or pointer runaway
- shifts sound like an intentionally primitive period-synchronous processor rather than a phase vocoder

V0B succeeds when:

- clean glissandi produce the expected note staircase
- Retune 0 creates genuinely abrupt target locking
- vibrato is heavily flattened
- formants audibly move with corrected pitch
- unvoiced noise is not forcibly tuned

V1 succeeds when:

- AU and VST3 validate
- Ableton Live can instantiate the plugin
- automation works
- low-latency real-time vocal use is stable
- Keyboard Display / Note Matrix works correctly
- Target / Bypass / Remove are DSP-distinct
- no allocations/glitches occur under normal use

The subjective V1 target is:

"rigid, bright, quick, occasionally slightly wrong, and musically useful"

NOT:

"transparent state-of-the-art vocal correction."


==================================================
28. IMPORTANT ANTI-SCOPE-CREEP RULES
==================================================

Do not add:

- automatic key detection
- neural pitch tracking
- formant correction
- harmony generation
- MIDI note control
- graphical pitch editing
- vocal effects rack
- oversampling unless demonstrated necessary
- elaborate preset system
- cloud features
- telemetry
- authentication
- copy protection
- account system
- updater
- fancy installer

unless explicitly requested later.

We want to understand the DSP first.


==================================================
29. FIRST TASK
==================================================

Begin by creating:

1. repository skeleton
2. ARCHITECTURE.md with evidence / inference / experimental classification
3. deterministic synthetic test-signal generator
4. offline V0A CycleResampler
5. CLI renderer
6. tests for the resampler
7. several rendered WAV examples

Do NOT begin with the JUCE UI.

After V0A is working, provide:

- build instructions
- a short explanation of the resampler
- file paths for generated test renders
- any audible/technical problems you encountered
- which choices are currently evidence-based vs experimental

Then proceed to V0B unless blocked by a concrete technical failure.

The goal is to leave me with a WORKING prototype that I can modify, break, inspect, and learn from — not merely an architecture proposal.
