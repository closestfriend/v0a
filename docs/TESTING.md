# Testing

## Testing philosophy

Testing serves two distinct goals:

1. Prove that the implementation is deterministic, safe, and matches the project contract.
2. Discriminate among historical hypotheses without declaring the cleanest-sounding result authentic.

Every render used as evidence records the build identifier, sample rate, block partition, input hash, parameter set, interpolation mode, splice mode, detector mode, and hypothesis IDs. Lossless WAV and CSV are paired by basename.

## Test layers

| Layer | Purpose | Runs when |
|---|---|---|
| Unit | Verify conversions, state machines, interpolation, and pointer math | Every change |
| Property/invariant | Exercise broad ratios, periods, block sizes, and boundaries | Every DSP change |
| Deterministic render | Lock numerical/audio behavior for a fixed configuration | Every DSP change |
| Signal analysis | Verify expected F0, formant, splice, and voiced behavior | Phase acceptance and DSP changes |
| Real-time safety | Detect allocations, bounds errors, locks, and timing regressions | Before V1 acceptance |
| Plugin validation | Validate AU/VST3 loading, latency, state, and automation | V1 builds |
| Forensic/oracle | Compare competing hypotheses against trusted external material | V2 or when an oracle is available |
| Listening | Judge character and musical usefulness | After objective checks, never as sole provenance evidence |

## Deterministic signal corpus

Generate at least the following signals from code with fixed seeds and explicit sample rate, duration, amplitude, phase, and fade policy:

```text
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
```

Add `vocal_fry_synthetic.wav` when its generation model and expected ambiguity are documented. White-noise components use an explicit seeded generator. Generated WAV metadata or an adjacent manifest records all parameters; filenames alone are insufficient.

The signal generator itself is tested for deterministic hashes, expected frame counts, channel count, sample rate, peak bounds, and known analytic F0 where applicable.

## V0A acceptance plan

V0A uses manual period/F0 and ratio only. Automatic pitch detection, scale targeting, Retune Speed, and UI must be absent from this path.

### Required ratio matrix

Render at minimum:

```text
1.000000
1.030000
1.059463  // approximately +1 semitone
0.943874  // approximately -1 semitone
```

Run each non-unity ratio with:

- Linear, Cubic Hermite, and 4-point Lagrange interpolation;
- crossfade samples 0, 2, 4, 8, and 16; and
- selected period-fraction crossfades.

Use sine, saw, and at least one synthetic vowel at several manual periods. Include ratios above and below unity so both cycle operations execute.

### V0A unit tests

- Ring indices wrap correctly at both ends.
- Every interpolator reads valid wrapped neighbors.
- Linear interpolation returns exact stored samples at integer positions.
- Constant input remains constant across interpolation and splices within floating-point tolerance.
- Unity rate maintains bounded read/write separation and produces zero steady-state repeat/delete events.
- A forced ahead condition changes the unwrapped read position by exactly `-period` for repetition.
- A forced behind condition changes it by exactly `+period` for deletion.
- Fractional periods and read positions remain finite through long renders.
- Crossfade length zero never enters a divide-by-zero or stale-fade state.
- Crossfade lengths larger than the currently allowed splice window are rejected or clamped outside the audio callback by a documented policy.
- Invalid manual F0, period, or ratio fails clearly in the CLI rather than reaching DSP math.

### V0A property and stress tests

Sweep:

- supported sample rates: 44.1, 48, 88.2, and 96 kHz;
- block partitions including 1 sample, odd sizes, and changing offline chunk sizes;
- periods spanning configured voice/instrument limits;
- ratios within the documented supported shift interval;
- buffer wrap events and long render durations.

Assert:

- no out-of-bounds access under sanitizers;
- no NaN or infinity;
- no pointer runaway;
- exact output frame count equals input frame count;
- deterministic counters and sample output for identical inputs/configuration;
- repeat events occur for `r > 1` and delete events for `r < 1` under the convention in `DSP_NOTES.md`;
- event counts keep read/write separation bounded without unexplained multi-period jumps.
- for every stationary non-unity `(P, r)` case, event density converges toward `abs(r - 1) / P`.

For the event-density assertion, let `K_N` be the repeat or delete count after `N` measured output samples. Exclude or separately account for initialization, derive residual-separation width `B` from the implemented trigger bounds, and assert:

```text
abs(K_N / N - abs(r - 1) / P) <= B / (N * P)
```

Run progressively longer renders and verify that absolute density error does not exceed the derived finite-render bound and trends toward zero. Include fractional `P`, ratios close to unity, both drift directions, buffer wrapping, and block-partition changes. This is a mathematical drift-compensation test, not a golden event count tied to one initial pointer offset.

### V0A signal measurements

- Estimate output F0 on the stable interior of sine renders and compare its direction and approximate ratio to the requested shift.
- Inspect logged splice positions against waveform plots for saw and vowel inputs.
- Measure discontinuity magnitude and high-frequency residual around each splice for every crossfade/interpolator combination.
- Compare formant peak motion on synthetic vowels; the baseline should move the envelope with pitch rather than hold it stationary.
- Listen only after verifying direction, frame count, pointer safety, and event logs.

V0A passes when manual shifting is continuous, duration stays synchronized through repeat/delete events, invariants hold, and the output is recognizably a primitive period-synchronous processor rather than a phase-vocoder render. No one interpolation or crossfade is declared historically correct by this milestone.

## V0B unit tests

### Pitch and note math

- Hz-to-MIDI and MIDI-to-Hz round-trip over the supported frequency range.
- Cents conversions and octave/semitone reference ratios.
- Nearest Target selection across positive and negative MIDI values.
- Deterministic tie behavior at an exact midpoint.
- Removed notes are never selected.
- Bypass uses the fixed nearest-chromatic-note cell defined in `DSP_NOTES.md` and produces unity correction throughout that cell.
- Removing a note widens neighboring capture regions correctly.
- Scale presets initialize the expected 12-state matrix.
- Manual note edits place the matrix in Custom state.
- Changing Key or Scale reloads the preset and clears custom overrides.

### Boundary behavior

For two adjacent enabled chromatic targets, test detected pitch at:

- `-51`, `-49`, `+49`, and `+51` cents around target centers as applicable;
- the exact 50-cent midpoint using the documented tie rule; and
- slow upward and downward crossings.

With baseline hysteresis and confirmation disabled, upward/downward switch positions should differ only by numerical and detector error, not an intentional deadband or delay.

### Three-state cell semantics

Configure E, F#, and G as Targets, then set F separately to each state. Test detected pitch at `F - 49 cents`, exact F, and `F + 49 cents`:

| F state | `F - 49c` | `F` | `F + 49c` |
|---|---|---|---|
| Target | Target F | Target F | Target F |
| Bypass | Unity | Unity | Unity |
| Remove | Target E | Target F# by the upward exact-tie rule | Target F# |

Also test the half-open chromatic-cell boundaries: `F - 50c` belongs to the F cell, while `F + 50c` belongs to the F# cell. Changing other Target/Remove states must not resize F's Bypass cell. A matrix with no Target notes must return unity plus `NoTargetAvailable`.

### Retune controller

- Retune 0 sets the smoothed ratio to raw ratio on the current update.
- Retune 0 has no 1 ms or epsilon-derived minimum.
- A nonzero one-pole converges monotonically toward a constant positive target ratio.
- The measured discrete response matches the implemented coefficient equation.
- A target change does not reset trajectory state.
- Ratio-domain, cents-domain, and linear-slew modes are distinguishable on a wide pitch step.
- Sample-update and period-update modes produce expected, separately snapshotted traces.

### Detector

- Clean sine F0 is detected over the configured range.
- Saw F0 selects the fundamental rather than a strong harmonic on the controlled corpus.
- Quadratic refinement reduces or does not materially worsen clean-sine period error versus integer lag.
- `L` versus `2L` arbitration handles a controlled weak-fundamental signal.
- Silence, low energy, and white noise are unvoiced.
- Tone-plus-noise crosses the voiced decision monotonically as the configured test SNR changes, subject to documented exceptions.
- Reference and recursive detectors agree on period, metric, and voiced decision within specified numerical tolerance on identical frames.
- Decimated analysis rejects out-of-band aliases in dedicated anti-alias tests.

### Unvoiced handling

- Baseline ImmediateUnity produces raw ratio 1 on the first rejected update.
- The latency-matched dry/unity path has no timeline jump at voiced/unvoiced transitions.
- HoldLastRatio and FadeToUnity are inactive at default settings.
- Noise is never sent to arbitrary musical targets in the baseline.

## V0B behavioral renders

1. **Sine glissando:** output F0 forms a chromatic staircase with transitions at nominal midpoints.
2. **Saw glissando:** output follows the same target staircase without systematic octave capture.
3. **Synthetic vowel glissando:** F0 follows the staircase while formant peaks move with the applied pitch ratio.
4. **Controlled vibrato:** Retune 0 strongly flattens voiced vibrato around a fixed target.
5. **White noise:** detector reports unvoiced and correction remains unity.
6. **49/51-cent tones:** select the expected sides of the midpoint.
7. **Pitch steps:** expose trajectory shape, settling time, and any period-quantized updates.
8. **Vowel/consonant transitions:** expose unvoiced gating, catches, clicks, and unintended buffer jumps.
9. **Amplitude ramp at fixed F0:** separates energy-gate effects from pitch changes.

V0B passes when clean glissandi produce the expected staircase, Retune 0 is genuinely abrupt, vibrato is heavily flattened, formants audibly/measurably move, and unvoiced noise is not forced into pitched correction.

## Humanize composition experiments (V1/V1.5)

Render the same sustained synthetic vowel with late-onset vibrato under:

```text
Retune Speed = 0
Humanize = 0 and a documented nonzero value
HumanizeTauMode = Multiplicative and Additive
```

Keep detector, target, onset, ramp, amount, signal, and block partition identical. Assert the model-level distinction:

- **Multiplicative:** with `baseTau = 0`, nonzero Humanize leaves `effectiveTau = 0`; Humanize 0 and nonzero traces remain identical after accounting for diagnostics that do not affect audio.
- **Additive:** with `baseTau = 0`, nonzero Humanize may produce `effectiveTau > 0` after sustain onset; it must remain identical to Humanize 0 before onset and then show the configured relaxation afterward.

Measure effective time constant, smoothed ratio, output F0, and late-vibrato depth. The test proves that both hypotheses are implemented distinctly; it does not select a historical winner. If a verified EFX 3 oracle becomes available, run the same matrix and use the post-onset Retune-0 behavior to discriminate HYP-015 from HYP-016.

## Diagnostic CSV checks

Each offline CSV contains, in order:

```text
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
```

Tests verify header names/order, row count policy, monotonic time, finite numeric fields when defined, explicit representation of undefined F0, valid note modes, unity raw ratio for Bypass/unvoiced baseline cases, and monotonic repeat/delete counters.

## Real-time and plugin tests

### Real-time safety

- Instrument allocation in `processBlock` and assert zero allocations after preparation.
- Run AddressSanitizer and UndefinedBehaviorSanitizer test targets where toolchain support permits.
- Exercise maximum blocks, minimum blocks, parameter automation, buffer wraps, and prepare/release cycles.
- Verify no mutex acquisition, filesystem work, logging, or GUI access on the audio thread.
- Confirm diagnostics use bounded atomic/lock-free transfer and cannot block processing.

### Plugin behavior

- AU passes Apple's Audio Unit validation tool.
- VST3 passes the available validator.
- Both formats instantiate in Ableton Live on Apple Silicon.
- Mono and declared stereo layouts behave as documented.
- Reported latency matches an impulse/correlation measurement at each supported sample rate.
- Bypass is latency-compensated and click behavior is tested.
- State save/restore preserves Key, Scale/Custom matrix, Retune Speed, Humanize, Voice Type, Tracking, and developer settings.
- Automation is sample/block safe and does not allocate.
- Target, Bypass, and Remove produce distinct verified DSP results.
- Every note key is keyboard-focusable and exposes pitch name and state to accessibility APIs.

V1 passes only when formats validate, Ableton can instantiate the plugin, automation/state work, real-time vocal use is stable, Note Matrix semantics are correct, and normal operation has no detected allocations or buffer errors.

## Forensic experiments without an EFX 3 oracle

Use public demonstrations and historical material only to rank hypotheses, with source and codec limitations recorded.

- **F0 transition derivative:** compare step sharpness and settling shape on documented Retune settings.
- **Vibrato attenuation:** measure depth before/after and its time evolution on sustained notes.
- **Formant motion:** track known synthetic or identifiable vowel formants relative to correction ratio.
- **Splice signature:** inspect waveform and spectrogram neighborhoods around logged repeat/delete events.
- **Consonant behavior:** compare transient duplication, tearing, and unity passthrough signatures.
- **Octave errors:** log conditions around raspy, fry, and weak-fundamental sources.

Lossy examples can suggest a test but cannot establish sample-level splice or latency details.

Later Classic Mode is only a secondary comparison because it represents Auto-Tune 5 rather than EFX 3. “Brighter” or “harder” is converted into spectral/transition measurements before it influences a DSP parameter.

## Future oracle protocol

If a verified working EFX 3 instance becomes available:

1. Record exact plugin version, host, OS, sample rate, buffer size, controls, scale matrix, and reported latency.
2. Render the deterministic corpus losslessly at Retune 0 and representative nonzero values, Humanize 0 and mid values, and Normal/Choosy.
3. Include dry-through/bypass and unity-correction controls.
4. Measure F0, switch boundaries, trajectory shape, vibrato attenuation, formant tracks, splice neighborhoods, voiced decisions, and latency.
5. Fit competing models without changing the corpus after seeing results.
6. Repeat across at least two F0 regions and multiple signal levels.
7. Store raw results separately from interpretation.
8. Update hypothesis IDs and evidence classifications only after replication.

High-value discriminating tests:

- triangular modulation around a midpoint for hysteresis;
- abrupt pitch steps for one-pole versus slew and update cadence;
- late-onset vibrato for Humanize onset/ramp;
- tone/noise and vowel/fricative transitions for unvoiced hold;
- asymmetric periodic waveforms at multiple F0s for splice alignment and crossfade scaling;
- impulse/correlation measurement for exact latency.

## Regression artifact policy

Do not commit a large render corpus casually. Keep compact golden numerical traces and short lossless fixtures sufficient to detect behavior changes. Large regenerated WAVs are build artifacts unless a specific oracle render must be preserved as evidence.

When a golden output changes, the review must state:

- which architecture or hypothesis ID changed;
- why the previous result was wrong or intentionally superseded;
- which objective measurements changed; and
- whether the historical baseline remains selectable unchanged.

## Phase checklist

### V0A

- [ ] Manual ratio only; no hidden detector/scale logic
- [ ] Both repeat and delete paths tested
- [ ] Three interpolators tested
- [ ] Zero and short crossfades tested
- [ ] Deterministic WAV/CSV output
- [ ] Bounds, finite-value, duration, and pointer invariants pass

### V0B

- [ ] Reference detector and voiced gate pass corpus tests
- [ ] Octave arbitration tested
- [ ] Memoryless chromatic targeting passes 49/51-cent tests
- [ ] Retune 0 is immediate
- [ ] Vibrato flattening and formant motion measured
- [ ] Noise remains unvoiced/unpitched

### V1

- [ ] AU and VST3 validate and instantiate
- [ ] Declared channel behavior and latency are accurate
- [ ] No detected audio-thread allocations or unsafe operations
- [ ] Automation and state restoration pass
- [ ] Target/Bypass/Remove are DSP-distinct and accessible

### V2

- [ ] Every tuning change names a hypothesis ID
- [ ] Measurements accompany listening judgments
- [ ] Source/oracle limitations are recorded
- [ ] Baseline remains available unchanged
