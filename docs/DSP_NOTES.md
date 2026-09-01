# DSP Notes

## Scope

These notes define the initial math, invariants, and experiment seams. They do not assert undocumented EFX 3 constants. `ARCHITECTURE.md` controls architectural conflicts; `EVIDENCE_AND_HYPOTHESES.md` controls provenance.

## Conventions

Let:

- `fs` be the full audio sample rate in hertz.
- `P_in` be the detected input period in full-rate samples.
- `P_target` be the desired target period in full-rate samples.
- `f_in = fs / P_in`.
- `r_raw = P_in / P_target` be the fractional read rate.
- `r = 1` mean unity-rate reading.

When the desired pitch is sharper than the input, `P_target < P_in`, so `r_raw > 1`: the read head advances faster and must periodically repeat a whole input cycle to remain bounded. When the target is flatter, `r_raw < 1`: the read head advances slower and must periodically skip a whole cycle.

All conversions must reject non-finite values and non-positive periods/frequencies before logarithms or divisions.

## V0A cycle resampler

### State

At minimum:

- preallocated circular audio buffer
- integer write position
- wrapped fractional read position
- safe separation between read and write heads
- current full-rate period
- current read rate
- interpolation mode
- splice mode and crossfade configuration
- repeat/delete event counters

The buffer must be sized from the lowest supported F0, highest sample rate, required lookback/lookahead for interpolation, and a documented safety margin. Buffer size is an engineering parameter, not a historical latency claim.

### Per-sample operation

Conceptually:

```text
write input sample
read interpolated output at readPosition
readPosition += rate
if read head is too far ahead: readPosition -= period  // repeat cycle
if read head is too far behind: readPosition += period // delete cycle
wrap and validate positions
```

The actual trigger must use an unwrapped or signed-distance representation so circular wrapping cannot invert ahead/behind decisions. A splice jump is exactly one current period in the baseline. The implementation must specify whether `period` is sampled at event time or held over a detected cycle; that update policy is an experiment until measured.

Manual period/F0 and ratio inputs are authoritative in V0A. No hidden detector or target logic is permitted.

### Cycle-event-rate invariant

For a stationary input period `P`, constant non-unity rate `r`, and `N` output samples, uncompensated read/write drift is:

```text
drift(N) = (r - 1) * N
```

Each repeat/delete event compensates that drift by one period `P` in the opposite direction. If `K_N` is the event count, then asymptotically:

```text
K_N / N -> abs(r - 1) / P
```

Equivalently:

```text
K_N ~= N * abs(r - 1) / P
```

The exact integer count depends on initial read/write separation, the trigger threshold, sample-step overshoot, and the final incomplete drift interval. A stronger finite-render check follows from the bounded residual separation. If the residual separation can vary over an interval of width `B`, then, after accounting for initialization under the same convention:

```text
abs(K_N / N - abs(r - 1) / P) <= B / (N * P)
```

or use an explicitly derived tighter bound for the implemented trigger. The test must derive `B` from the actual allowed residual interval rather than choose a listening-based tolerance. This invariant verifies that period jumps compensate accumulated rate drift instead of merely keeping pointers bounded accidentally.

### Interpolation

Required selectable modes:

- Linear
- Cubic Hermite
- 4-point Lagrange

Each needs defined neighbor indices and wrap-safe reads. Linear is the simplest credible starting point; Hermite or Lagrange must not become the default merely because a metric shows less interpolation error.

### Splice treatment

Required experiments:

- crossfade lengths of 0, 2, 4, 8, and 16 samples
- crossfade lengths expressed as a fraction of the current period
- raw period jump
- nearest zero crossing
- nearest local waveform-correlation alignment point

`spliceCrossfadeSamples = 0` is a first-class baseline configuration. Any nonzero window must specify its curve and whether the two paths are amplitude-complementary or power-complementary. Exact historical alignment and crossfade behavior are unknown.

## Period detector

### Reference detector first

Implement an inspectable `O(N * L)` reference before recursive optimization. For candidate lag `L`, compare a waveform segment with its lagged copy over a window of roughly two periods.

A difference-family metric may be expressed as:

```text
D(L) = E(L) - 2 H(L)
```

where `E` is an energy term and `H` is a lagged cross-product term using the same window. Accept periodicity only when signal energy is sufficient and approximately:

```text
D(L) <= epsilon * E(L)
```

The precise normalization and indexing must be defined once and shared by the reference and optimized detectors. A negative `D` caused by an inconsistent energy definition is a warning that the algebra or window pairing is wrong, not evidence of exceptional periodicity.

### Recursive detector

The patent-motivated optimized form updates sliding energy/correlation accumulators rather than recomputing every lag. It must not replace the reference implementation until both agree on deterministic signals within documented tolerance.

The historically motivated analysis path low-pass filters before approximately 8:1 decimation. The anti-alias filter design, decimation factor at high sample rates, and literal applicability to the Evo/Auto-Tune 7-generation detector are reconstruction questions. Keep full-rate and decimated period units explicit:

```text
P_full = P_analysis * decimationFactor
f = fs / P_full
```

### Lag refinement

For the integer minimum `L0` with neighboring metric samples `d[-1]`, `d[0]`, and `d[+1]`, use a guarded three-point quadratic interpolation. Clamp the fractional offset to the local interval and fall back to `L0` when curvature is degenerate or non-finite.

### Octave arbitration

Compare plausible fundamental candidates such as `L` and `2L` using the same consistently normalized periodicity measure. Do not blindly choose the strongest harmonic. Baseline arbitration should remain local and inspectable.

Developer parameters may control octave-jump resistance, candidate preference, and minimum confidence for changes. These must not become a long-lookahead modern tracking state machine. Tracking Choosy primarily tightens periodicity acceptance; exact epsilon and arbitration mappings remain experimental.

### Voiced decision

Voiced acceptance requires both:

- energy above a configurable threshold; and
- periodicity within the configured threshold.

The result reports period, frequency, periodicity, confidence, and voiced state separately. Confidence is not interchangeable with periodicity unless the mapping is explicitly defined.

## Pitch and note domain

Use logarithmic pitch for note selection:

```text
midi = 69 + 12 * log2(f / 440)
centsFromA4 = 1200 * log2(f / 440)
f = 440 * 2^((midi - 69) / 12)
```

The reference tuning frequency may later be configurable, but A4 = 440 Hz is the initial product default rather than a claim about every historical preset.

### Target selection

First map the detected MIDI pitch to an ordinary chromatic note cell independently of the Target set:

```text
cellNote = floor(detectedMidi + 0.5)
cellPitchClass = positiveModulo(cellNote, 12)
```

This defines cell `m` as the half-open interval `[m - 0.5, m + 0.5)`, or ±50 cents with the upper midpoint assigned to the next note. This boundary convention is a deterministic product rule, not a historical claim.

Then apply the matrix in two stages:

```text
if state[cellPitchClass] == Bypass:
    correction ratio = 1
else:
    target = nearest note whose mode is Target
```

The state meanings are therefore:

```text
Target = may capture pitch during nearest-Target search
Bypass = owns its fixed chromatic cell, but never corrects it
Remove = owns no region and is never a target
```

Bypass cells do not grow when neighboring notes are removed. A pitch whose chromatic cell is Target or Remove proceeds to nearest-Target search over all octaves. Notes in Bypass or Remove mode are not candidates. With removed notes, Target capture regions widen to the midpoint between the remaining Targets.

For exact equal-distance Target ties, the project resolves upward to the higher MIDI note. This guarantees deterministic output but is not established EFX 3 behavior. If the matrix contains no Target notes, return unity correction plus an explicit `NoTargetAvailable` diagnostic rather than choosing an arbitrary pitch.

Developer targeting parameters begin disabled:

```text
dbg_noteHysteresisCents = 0
dbg_noteConfirmPeriods = 0
```

An implementation must not allocate or search an unbounded note set in the audio callback. Searching a fixed local range or precomputed target map is acceptable.

## Retune trajectory

The patent-motivated baseline smooths the read-rate ratio:

```text
r_raw = P_in / P_target
alpha = exp(-dt / tau)
r[n] = alpha * r[n-1] + (1 - alpha) * r_raw[n]
```

At Retune Speed 0:

```text
tau = 0
alpha = 0
r[n] = r_raw[n]
```

There is no hidden 1 ms minimum, transient softening, or state reset at a target-note transition. Use a direct branch for `tau <= 0` rather than relying on numerically awkward exponential limits.

The exact mapping from the displayed Retune Speed value to `tau` is unknown. `tau ~= retuneSpeedMilliseconds` is a useful starting hypothesis only and must be replaceable. Developer alternatives may filter in ratio/period-ratio, cents, or use a linear slew limiter:

```cpp
enum class RetuneDomain
{
    PeriodRatio,
    Cents,
    LinearSlew
};
```

Only `PeriodRatio` is the baseline. Tests must discriminate exponential from linear motion and sample-updated from period-updated trajectories.

## Humanize

The documented behavioral target is: short notes retain the base Retune Speed while sustained notes receive effectively slower correction. Public evidence does not establish how Humanize composes with the base time constant, especially when Retune Speed is 0.

Keep two explicit reconstruction hypotheses:

```cpp
enum class HumanizeTauMode
{
    Multiplicative,
    Additive
};
```

```text
HUMANIZE_MULTIPLICATIVE
effectiveTau = baseTau * factor(duration, amount)

HUMANIZE_ADDITIVE
effectiveTau = baseTau + additionalTau(duration, amount)
```

Before sustain onset, both modes return `baseTau`. After onset, the multiplicative mode necessarily remains zero when `baseTau = 0`; the additive mode may produce a positive effective time constant. This difference is intentional and observable. Neither mode may be treated as historically correct without discriminating evidence.

Unknowns include:

- whether sustain is based on target identity, input-pitch stability, or both
- onset threshold
- step versus ramp
- ramp duration and curve
- maximum multiplier
- multiplicative versus additive composition, including behavior when `baseTau = 0`
- exact vibrato interaction

Expose composition mode, onset, ramp, multiplier or additional time constant, and stability window as experimental developer parameters. Do not embed values copied from Report A's pseudocode as facts.

## Unvoiced and consonant handling

The selected baseline is:

```text
if not reliably voiced:
    rate -> 1 immediately
    output -> latency-matched dry or unity-rate resampled audio
```

This avoids arbitrarily quantizing noise. The transition must not change the declared latency or jump to an unrelated buffer position.

Selectable experiments:

```cpp
enum class UnvoicedMode
{
    ImmediateUnity,
    HoldLastRatio,
    FadeToUnity
};
```

Hold and fade durations default to zero. Report A proposes a short last-ratio hold, while Report B and the project directive select immediate unity; this disagreement must remain observable rather than silently resolved in code.

## Formants

Do not estimate, remove, or restore a spectral envelope in the baseline. Direct variable-rate resampling moves formant locations with pitch. That motion is a desired signature, not a defect to correct.

If formant preservation is ever added, it is an isolated experiment with amount `0.0` preserving the baseline bit-for-bit where practical.

## Latency

Detector lookback, buffer safety separation, interpolation lookahead, and splice alignment determine implementation latency. A low-frequency input can require multiple long periods, so latency must be defined by a stable maximum or an explicitly documented mode; it cannot vary silently without matching the dry path.

Measure and report actual samples at every supported sample rate. Historical descriptions such as “ultra-low latency,” community claims of zero samples, and estimates of one-to-three periods do not establish an exact EFX 3 value.

## Diagnostic fields

Offline processing should be able to emit:

```text
time_seconds,input_f0_hz,input_pitch_cents,period_samples,
periodicity,confidence,voiced,note_region,note_mode,target_midi,
target_f0_hz,raw_ratio,smoothed_ratio,output_f0_estimate,
cycle_repetitions,cycle_deletions
```

Event counters must be monotonic within a render. CSV emission is offline-only; real-time code publishes a bounded snapshot.

## Baseline versus alternatives

| Stage | Baseline | Selectable experiment |
|---|---|---|
| Detector calculation | Transparent short-window time-domain difference metric | Recursive equivalent after parity tests |
| Analysis rate | Full-rate reference; patent-motivated decimated path available | Decimation/filter variants |
| Targeting | Memoryless nearest Target | Deadband and confirmation periods |
| Retune | Ratio-domain one-pole; true zero | Cents-domain one-pole or linear slew |
| Humanize | Documented sustained-note slowdown intent; composition unresolved | Multiplicative/Additive composition plus onset/stability/ramp variants |
| Interpolation | Simplest credible selectable default | Hermite/Lagrange comparison |
| Splice | Whole-period jump; zero crossfade valid | Crossfade/alignment variants |
| Unvoiced | Immediate unity | Hold last ratio or fade |
| Formant | No preservation | Isolated preservation amount, default zero |

## Numerical and real-time invariants

- Every ring-buffer access wraps into `[0, size)`.
- Interpolation always has valid wrapped neighbors.
- Read/write separation stays inside documented safe bounds.
- A splice changes the unwrapped read position by exactly one accepted period.
- Period, target period, rate, and coefficients remain finite and positive.
- Unity ratio produces no repeat/delete events after initialization.
- For stationary non-unity rate and period, repeat/delete density converges to `abs(rate - 1) / period` within the derived finite-render residual bound.
- Processing is deterministic for identical input, settings, block partition, and sample rate.
- `processBlock` performs no allocation, locking, file I/O, logging, or GUI work.
- Parameter changes cannot expose partially updated compound configuration.
