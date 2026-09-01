# V0A Cycle Resampler Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a deterministic offline mono-WAV renderer that performs manual period-synchronous variable-rate resampling with whole-cycle repeat/delete, selectable interpolation, zero-or-short splice crossfades, diagnostics, tests, and example renders.

**Architecture:** A dependency-free C++20 core owns a fixed circular buffer and unwrapped read/write coordinates. `CycleResampler` advances its fractional read head by a constant manual rate, compensates accumulated drift with exactly one-period jumps, and crossfades old/new read trajectories when configured. Small WAV, CLI, and signal-generator layers stay outside the DSP core.

**Tech Stack:** C++20, CMake 4.x-compatible syntax, Apple Clang 17, CTest, standard library only.

**Spec:** `docs/ARCHITECTURE.md`, `docs/DSP_NOTES.md`, `docs/EVIDENCE_AND_HYPOTHESES.md`, and `docs/TESTING.md`

## Global Constraints

- V0A is offline and internally mono.
- Inputs are a mono WAV, manual fundamental period or F0, and manual pitch ratio.
- No automatic pitch detector, scale logic, Retune Speed, plugin wrapper, or UI.
- Whole-cycle repeat/delete and a circular buffer are mandatory; do not substitute PSOLA, WSOLA, grains, STFT, or a phase vocoder.
- Linear, Cubic Hermite, and 4-point Lagrange interpolation are selectable.
- Crossfade lengths 0, 2, 4, 8, and 16 samples are valid; zero is not a special error case.
- The historical default interpolator, splice window, and alignment are not claimed.
- Identical input/configuration/block partition must render deterministically.
- No third-party runtime or test dependency.

## File map

- `CMakeLists.txt` — builds the core library, CLI tools, and CTest targets.
- `src/dsp/Interpolators.h` — wrap-safe Linear, Cubic Hermite, and 4-point Lagrange readers.
- `src/dsp/RingBuffer.h` — preallocated circular storage addressed by signed unwrapped sample index.
- `src/dsp/CycleResampler.h/.cpp` — manual-rate streaming engine, splice state, event counters, and diagnostics.
- `src/io/WavFile.h/.cpp` — mono PCM16/PCM24/PCM32 and float32 WAV reader; float32 WAV writer.
- `tools/offline_renderer/main.cpp` — validates arguments and renders input with a fixed manual period/F0 and ratio.
- `tools/signal_generator/main.cpp` — creates deterministic V0A sine, saw, and vowel-like fixtures.
- `tests/TestHarness.h` — dependency-free registration/assertion harness.
- `tests/test_interpolators.cpp` — interpolation and wrap behavior.
- `tests/test_cycle_resampler.cpp` — pointer, direction, crossfade-zero, determinism, and event-density behavior.
- `tests/test_wav_file.cpp` — WAV round-trip and malformed/unsupported input behavior.
- `README.md` — V0A build, CLI, test, diagnostic, and render instructions.
- `renders/` — generated examples, not hand-authored binary fixtures.

---

### Task 1: Build skeleton and dependency-free test harness

**Files:**
- Create: `CMakeLists.txt`
- Create: `tests/TestHarness.h`
- Create: `tests/test_interpolators.cpp`

**Interfaces:**
- Produces: CMake library target `v0a_dsp`; test executables registered with CTest; `TEST_CASE`, `REQUIRE`, and `REQUIRE_NEAR` test macros.

- [ ] **Step 1: Add only the build/test scaffolding**

Create a C++20 CMake project with `enable_testing()`, include `src`, and a tiny harness that returns nonzero when any registered real-behavior assertion fails. Do not add interpolation production code in this step.

- [ ] **Step 2: Create a failing interpolation compile test**

Write `tests/test_interpolators.cpp` to include `dsp/Interpolators.h`, construct a wrapped four-sample source, and assert integer-position Linear output and a known half-sample Linear result. This must fail because the header does not exist.

- [ ] **Step 3: Configure and verify RED**

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_interpolators
```

Expected: CMake and the harness configure successfully; compilation fails only because `dsp/Interpolators.h` is missing.

### Task 2: Interpolators and signed ring addressing

**Files:**
- Create: `src/dsp/Interpolators.h`
- Create: `src/dsp/RingBuffer.h`
- Modify: `tests/test_interpolators.cpp`

**Interfaces:**
- Produces: `enum class InterpolationMethod { Linear, CubicHermite, Lagrange4 };`
- Produces: `float interpolate(const RingBuffer&, double position, InterpolationMethod);`
- Produces: `RingBuffer(std::size_t capacity)`, `write(std::int64_t absoluteIndex, float)`, and `read(std::int64_t absoluteIndex) const`.

- [ ] **Step 1: Extend RED tests**

Add literal expectations for integer positions, a Linear midpoint, constant-signal preservation for all three modes, negative absolute-index wrapping, and buffer-end wrapping. Name each test after the wrong branch/indexing mutation it catches.

- [ ] **Step 2: Verify RED**

Build and run `ctest --test-dir build -R interpolators --output-on-failure`. Expected: failures for missing ring/interpolator behavior.

- [ ] **Step 3: Implement minimal production headers**

Implement floor-based fractional decomposition, positive modulo for signed indices, wrap-safe neighbor reads, Catmull-Rom-style cubic Hermite, and standard four-point Lagrange coefficients. Keep all storage fixed after construction.

- [ ] **Step 4: Verify GREEN**

Run the interpolation test and then full CTest. Expected: all current tests pass with no warnings.

### Task 3: CycleResampler core with drift accounting

**Files:**
- Create: `src/dsp/CycleResampler.h`
- Create: `src/dsp/CycleResampler.cpp`
- Create: `tests/test_cycle_resampler.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `RingBuffer`, `InterpolationMethod`.
- Produces:

```cpp
struct CycleResamplerConfig {
    double periodSamples;
    double rate;
    std::size_t bufferCapacity;
    double safetyDelaySamples;
    InterpolationMethod interpolation;
    std::size_t spliceCrossfadeSamples;
};

struct CycleResamplerStats {
    std::uint64_t repeatedCycles;
    std::uint64_t deletedCycles;
    double residualSeparation;
    double minimumResidual;
    double maximumResidual;
};

class CycleResampler {
public:
    explicit CycleResampler(const CycleResamplerConfig&);
    float processSample(float input);
    const CycleResamplerStats& stats() const noexcept;
    void reset() noexcept;
};
```

- [ ] **Step 1: Write RED tests for validation and unity**

Tests reject nonpositive/nonfinite period or rate, undersized buffers, and crossfades not shorter than the period. Test that unity-rate constant and impulse-index signals maintain frame count, produce no steady-state cycle events, and remain finite.

- [ ] **Step 2: Verify RED**

Build `test_cycle_resampler`. Expected: missing class/API compile failure.

- [ ] **Step 3: Implement validation, fixed-delay initialization, and unity path**

Use signed unwrapped `writePosition` and double `readPosition`, pre-zero the ring, establish `safetyDelaySamples`, and update statistics without allocation in `processSample`.

- [ ] **Step 4: Verify GREEN for validation/unity**

Run `ctest --test-dir build -R cycle_resampler --output-on-failure`.

- [ ] **Step 5: Write RED tests for repeat/delete direction and exact jumps**

For stationary manual period, assert `rate > 1` increments only `repeatedCycles`, `rate < 1` increments only `deletedCycles`, and each event changes the unwrapped read coordinate by exactly one period opposite accumulated drift.

- [ ] **Step 6: Verify RED, then implement one-period compensation**

Trigger when residual separation leaves the allowed one-period interval. For positive drift subtract `periodSamples`; for negative drift add it. Update min/max residual after compensation.

- [ ] **Step 7: Verify GREEN**

Run the cycle test and full CTest.

- [ ] **Step 8: Write RED event-density tests**

Use hand-derived `expectedDensity = abs(r - 1) / P` for several literal `(P, r, N)` cases, both directions, close-to-unity ratios, fractional periods, and changing caller chunk sizes. Derive `B` from the configured residual interval and assert the finite-render density bound from `DSP_NOTES.md`.

- [ ] **Step 9: Verify RED catches wrong event sizing**

Temporarily the test must fail until statistics and trigger residual bounds are correct; confirm the failure reports measured and expected density.

- [ ] **Step 10: Implement/refine minimal event accounting and verify GREEN**

Do not tune a tolerance by ear. Use the mathematically derived residual width and rerun full CTest.

- [ ] **Step 11: Write RED tests for zero and nonzero crossfade**

Assert zero length never divides by zero or activates fade state. For a nonzero fade, assert the first post-event output follows the old trajectory, the fade ends after exactly the configured length, and deterministic output is preserved.

- [ ] **Step 12: Implement dual-read-trajectory splice crossfade and verify GREEN**

At an event retain the pre-jump read trajectory, jump the main read head exactly one period, blend old to new for the configured sample count, and forbid overlapping splice fades by configuration/trigger invariant. Run full CTest.

### Task 4: WAV boundary

**Files:**
- Create: `src/io/WavFile.h`
- Create: `src/io/WavFile.cpp`
- Create: `tests/test_wav_file.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `AudioBuffer { std::uint32_t sampleRate; std::vector<float> mono; }`.
- Produces `AudioBuffer readMonoWav(const std::filesystem::path&)` supporting mono PCM16/24/32 and IEEE float32.
- Produces `void writeMonoFloatWav(path, sampleRate, std::span<const float>)`.

- [ ] **Step 1: Write RED round-trip and rejection tests**

Use test-created temporary files. Assert float32 round-trip sample rate/frame count/literal samples, PCM16 decoding, rejection of stereo, malformed chunks, unsupported compression, and truncated data.

- [ ] **Step 2: Verify RED**

Expected: missing WAV API compile failure.

- [ ] **Step 3: Implement bounded RIFF chunk parsing and float writer**

Parse little-endian fields explicitly, skip unknown chunks with word alignment, validate lengths before reads, normalize integer PCM correctly, and throw descriptive `std::runtime_error` outside DSP code.

- [ ] **Step 4: Verify GREEN under CTest**

Run WAV tests and full CTest.

### Task 5: Offline renderer CLI and diagnostics

**Files:**
- Create: `tools/offline_renderer/main.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/test_offline_renderer.cpp`

**Interfaces:**
- Produces executable `v0a_render`.
- Accepts `--input`, `--output`, exactly one of `--period`/`--f0`, `--ratio`, `--interpolator linear|hermite|lagrange`, `--crossfade-samples` or `--crossfade-period-fraction`, and optional `--diagnostics` CSV.

- [ ] **Step 1: Write RED process-level CLI tests**

Invoke the built executable on a temporary mono WAV. Assert invalid/missing arguments fail, `--period` and `--f0` are mutually exclusive, output frame count equals input, and diagnostics report repeat/delete totals matching the selected direction.

- [ ] **Step 2: Verify RED**

Expected: executable/behavior missing.

- [ ] **Step 3: Implement strict parsing and render loop**

Convert F0 to `sampleRate / f0`, derive fractional crossfade samples once before processing, select the interpolator, render one sample at a time, and write summary/event diagnostics without putting file I/O in `CycleResampler`.

- [ ] **Step 4: Verify GREEN**

Run CLI tests and full CTest.

### Task 6: Deterministic signal generator and example renders

**Files:**
- Create: `tools/signal_generator/main.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/test_signal_generator.cpp`
- Create: `README.md`

**Interfaces:**
- Produces executable `v0a_generate_signals --output-dir <path>`.
- Produces fixed 48 kHz float32 mono sine, saw, and vowel-like stationary fixtures with a manifest.

- [ ] **Step 1: Write RED generator integration test**

Run the tool twice into separate temporary directories and assert matching hashes/bytes, sample rates, channels, frame counts, and bounded peaks for every generated fixture.

- [ ] **Step 2: Verify RED**

Expected: generator executable missing.

- [ ] **Step 3: Implement deterministic generation**

Use analytic phase accumulation and fixed literal formant-filter coefficients or documented deterministic resonators. Do not use randomness for V0A fixtures.

- [ ] **Step 4: Verify GREEN**

Run generator tests and full CTest.

- [ ] **Step 5: Document and render examples**

Document exact build/test/CLI commands and provenance labels. Generate example shifts for ratios `1.000000`, `1.030000`, `1.059463`, and `0.943874`, including zero and short crossfade variants, into `renders/` with adjacent diagnostics.

- [ ] **Step 6: Final verification**

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Then inspect generated WAV headers, diagnostic event density, output lengths, and repository status. Record any audible or technical problems without promoting an experiment into the baseline.
