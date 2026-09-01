# V0B Core and V1-Lab

This repository contains the verified V0A cycle resampler, the V0B automatic monophonic correction loop, and a deliberately minimal V1-Lab AU/VST3 wrapper for DAW auditioning. The offline renderer and plug-in both use the same `v0a_dsp` library; the plug-in does not duplicate the detector, quantizer, controllers, or resampler.

The core uses a fractional read head over a circular buffer. Constant read-rate drift is compensated with whole-input-period jumps:

- `ratio > 1`: repeat a cycle by subtracting one period from the read coordinate.
- `ratio < 1`: delete a cycle by adding one period to the read coordinate.

This is an original clean-room implementation guided by the authority and evidence rules in `docs/`.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The offline build has no third-party dependencies. It requires a C++20 compiler and CMake 3.22 or newer.

## V0B acceptance and automatic rendering

Run the decisive synthetic acceptance suite and produce its WAV/CSV artifacts:

```bash
./build/v0b_acceptance --output-dir renders/v0b
```

Render a mono WAV through the automatic loop:

```bash
./build/v0b_render \
  --input input.wav \
  --output output.wav \
  --diagnostics output.csv \
  --key C \
  --scale chromatic \
  --retune-ms 0
```

The V0B path includes F0/period detection, voiced gating, L-vs-2L arbitration, memoryless note targeting, Key/Scale and Target/Bypass/Remove semantics, Retune control, and diagnostic CSV output. Use `./build/v0b_render --help` for the full option list.

## Build and install V1-Lab

Point `JUCE_SOURCE_DIR` at a JUCE 7 checkout and build thin Apple Silicon bundles:

```bash
cmake -S . -B build-plugin \
  -DV0A_BUILD_V1_LAB=ON \
  -DJUCE_SOURCE_DIR=/path/to/JUCE \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-plugin --target V1Lab_AU V1Lab_VST3 --parallel
```

Build products:

```text
build-plugin/V1Lab_artefacts/Release/AU/V1-Lab.component
build-plugin/V1Lab_artefacts/Release/VST3/V1-Lab.vst3
```

Install for the current user:

```bash
ditto build-plugin/V1Lab_artefacts/Release/AU/V1-Lab.component \
  "$HOME/Library/Audio/Plug-Ins/Components/V1-Lab.component"
ditto build-plugin/V1Lab_artefacts/Release/VST3/V1-Lab.vst3 \
  "$HOME/Library/Audio/Plug-Ins/VST3/V1-Lab.vst3"
```

In Ableton Live, open Settings > Plug-ins and click Rescan. Hold Option while clicking Rescan for a deep rescan if the normal scan does not discover a newly installed local build.

At 48 kHz the live build reports a fixed 4112-sample (85.67 ms) latency. Bypass travels through the same aligned delay. The wide prepared detector range is intentional for live experimentation; the DSP has not been redesigned to reduce latency.

V1-Lab defaults to C Chromatic with every note Target, Retune 0 ms, Humanize 0%, Alto/Tenor voice bounds, Normal tracking, Linear interpolation, no splice crossfade, immediate-unity unvoiced handling, and zero target hysteresis/confirmation.

## Generate deterministic inputs

```bash
./build/v0a_generate_signals --output-dir renders/source
```

This creates one-second, 48 kHz, mono float32 WAV files at a known 220 Hz fundamental:

- `sine_220hz.wav`
- `saw_220hz.wav`
- `vowel_a_220hz.wav`

The adjacent manifest records generation parameters. Repeated runs are byte-identical.

## Render

```bash
./build/v0a_render \
  --input renders/source/vowel_a_220hz.wav \
  --output renders/examples/vowel_up.wav \
  --f0 220 \
  --ratio 1.059463 \
  --interpolator linear \
  --crossfade-samples 0 \
  --diagnostics renders/examples/vowel_up.csv
```

Use exactly one of:

```text
--f0 <positive Hz>
--period <positive full-rate samples>
```

Required options:

```text
--input <mono WAV>
--output <float32 mono WAV>
--ratio <positive read-rate ratio>
--interpolator linear|hermite|lagrange
```

Splice treatment accepts either:

```text
--crossfade-samples <nonnegative integer>
--crossfade-period-fraction <positive fraction>
```

The two crossfade forms are mutually exclusive. Omitting both is equivalent to zero samples. A configured crossfade must be shorter than one manual period.

Input decoding supports mono PCM16, PCM24, PCM32, and IEEE float32 WAV. Stereo and malformed/truncated inputs are rejected rather than silently converted.

## Diagnostics

`--diagnostics` writes the project-standard CSV columns. V0A fills manual-period fields and event counters; V0B additionally records detector, voicing, note-region, target, control-ratio, and cycle-event state.

The command summary reports frame count and total repeated/deleted cycles. For stationary period `P`, constant ratio `r`, and long render length `N`, event count approaches:

```text
N * abs(r - 1) / P
```

Automated tests verify the corresponding finite-render density bound.

## Scope & Test Signals

- **EVIDENCE-BASED:** variable-rate conversion with whole-cycle repeat/delete; zero crossfade must remain valid.
- **PLAUSIBLE RECONSTRUCTION:** a fixed safety delay and raw period-sized pointer trigger.
- **EXPERIMENTAL:** Linear/Hermite/Lagrange interpolation and linear-amplitude splice crossfade lengths.

The current splice alignment is the raw one-period jump. Zero-crossing and waveform-correlation alignment remain later experiments. The same-length output retains the initial latency/safety-buffer prefill; the renderer does not trim or conceal that delay.

## Example renders

See [renders/README.md](renders/README.md) for generated files, settings, and observed event counts.
