# Bytes-vs-Samples Naming Audit (Phase 44 / IF-109)

## Scope

Audit of `size`, `length`, `numSamples`, `numBytes`, `offset` parameters
and field names across the wave-memory subsystem:

- `MemoryAllocator` and its callers
- `WaveformData` / `Waveform` / `WaveformIR` / `WaveformFront` structs
- `WaveformGenerator` signal-length paths
- `collectUsedWaves` / `assignWaveIndex` / prefetcher waveform sizing

This is a **rename-only** pass. Any arithmetic bug found is filed as a new
IF entry but **not fixed** in this audit.

## Prior history

Phase 44.1 was completed in the prior session per IF-109
(2026-05-05). The high-traffic identifiers were already renamed at that
point:

- `MemoryAllocator::memorySizeInSamples_` → `memorySizeInBytes_`
- `MemoryAllocator::cacheLineSize_`        → `cacheLineSizeBytes_`
- `Cache::allocate` / `getBestPosition` parameter `numSamples` → `numBytes`
- `prefetch.cpp` local `numSamplesForCache` → `numBytesForCache`

**TODO.md Phase 44.1-44.5 was never ticked, and no audit notes file
existed.** This document is the missing artefact for that phase plus the
44.2-44.4 follow-on sweep.

## Convention discovered

Across the wave-memory subsystem the convention now in force is:

| Quantity        | Recon name               | Unit                      |
|-----------------|--------------------------|---------------------------|
| Wave memory size| `memorySizeInBytes_`     | bytes                     |
| Cache-line size | `cacheLineSizeBytes_`    | bytes (typically 4096)    |
| Allocation size | `numBytes` / `numBytesForCache` / `byteSize_` / `allocationByteSize` / `elfAlignment_` | bytes |
| Per-waveform total | `WaveformIR::elfAlignment_` (+0xDC) / `Waveform::allocationByteSize` (+0x74) | bytes |
| `Signal::length_` (+0x50) | `length()` accessor   | **samples per channel** |
| `Waveform::minLengthSamples` (+0x70) | as named   | samples                   |
| `Signal::channels_` × `length_` × 2 | `byteSize_` for placeholders | bytes |
| `RawWave::size()` virtual | `size()`              | bytes (returns `data_.size() * sizeof(uint16_t)` for Hirzel/Cervino, `byteSize_` for placeholder) |
| `RawWavePlaceHolder::byteSize_` (+0x08) | as named | bytes                     |
| WaveformGenerator DSP `length` locals | as named | samples (matches the SeqC-user-facing parameter name `length`) |
| Prefetcher local `numBytesForCache` | as named | bytes                     |
| Prefetcher local `sizePerDevBytes` (was `sizePerDev`) | renamed in this pass | bytes (return of `Waveform::getSizePerDevice()`, which computes `ceil(rounded_samples * channels * bitsPerSample / 8)`) |

## Phase 44.2: Waveform / WaveformIR / WaveformFront fields

Surveyed `reconstructed/include/zhinst/waveform/{waveform,waveform_ir,waveform_front,signal,rawwave}.hpp`.

Result: **all field names are unambiguous**. The high-traffic fields
already carry explicit unit suffixes:

- `Waveform::minLengthSamples`     (samples, +0x70)
- `Waveform::allocationByteSize`   (bytes,   +0x74, JSON key
  "allocationSize" — JSON name is unitless but the recon field is
  correctly suffixed)
- `WaveformIR::elfAlignment_`      (bytes,   +0xDC)
- `RawWavePlaceHolder::byteSize_`  (bytes,   +0x08)
- `Signal::length_`                (samples per channel, +0x50)
- `Signal::channels_`              (channel count, +0x48)

The Waveform-base method `getSizePerDevice() -> uint32_t` returns
**bytes** (the doc comment in `waveform.cpp` explicitly says
`ceil(totalBits / 8)`). The method name itself does not encode the
unit, but the function-level doc is unambiguous and the only caller
(`prefetch_placesingle.cpp`) now stores into the renamed-this-pass
local `sizePerDevBytes`. **No header change made** — the symbol must
match the binary's mangled name `getSizePerDevice` for fidelity.

## Phase 44.3: WaveformGenerator signal-length paths

Surveyed `waveform_generator.hpp/.cpp` and `waveform_generator_dsp.cpp`.

Helper signatures with `int length`:

- `createDummyWaveform(int length)`
- `genericTriangle(int length, double amplitude, ...)`
- `interpolateLinear(int length, ...)`

All three are unambiguously **samples** (they feed `Signal` ctors and
SeqC builtins like `zeros(length)`). The parameter name `length`
matches the SeqC-user-facing argument name (e.g. `zeros(length)`,
`gauss(length, ...)`) which is the binary's intended convention.
**Renaming to `lengthSamples` would diverge from the
user-source-level naming.** Left as-is.

DSP-level locals such as

```cpp
int length = readInt(args[0], "length", 1, "zeros");
```

faithfully reproduce the SeqC parameter name they correspond to. **Not
renamed.**

`readWave(val, paramName, int expectedLength, funcName)`:
the third parameter is the int that gets stuffed into
`WaveformGeneratorValueException::dataValue_` on failure. Caller
audit:

| Caller (line)                                                    | Value passed              | Semantic           |
|------------------------------------------------------------------|---------------------------|--------------------|
| `waveform_generator_dsp.cpp:98` (scale wave)                     | `-1`                      | sentinel           |
| `waveform_generator_dsp.cpp:127` (add wave_1)                    | `-1`                      | sentinel           |
| `waveform_generator_dsp.cpp:140` (add wave_i)                    | `static_cast<int>(sum.size())` | running sample length |
| `waveform_generator_dsp.cpp:1308` (join)                         | `-1`                      | sentinel           |
| `waveform_generator_dsp.cpp:1657` (cut)                          | `-1`                      | sentinel           |
| `waveform_generator_dsp.cpp:1770` (flip)                         | `-1`                      | sentinel           |
| `waveform_generator_dsp.cpp:1799` (filter)                       | `1`                       | "expected length 1"? — single-sample expectation |

The semantic is **polymorphic** — sometimes a sentinel `-1`, sometimes
a running sample count, sometimes a literal `1`. Per the audit rule,
this is a "kept-polymorphic-with-comment" case. **No rename**;
clarifying behaviour would require a docstring update only — deferred
because (a) it does not affect the caller's understanding and (b) the
exception field's exact semantic is itself binary-derived and not
fully understood. Logged in this notes file in lieu of a code comment.

## Phase 44.4: Prefetcher / collectUsedWaves / wavetable sizing

Surveyed:

- `prefetch.cpp`, `prefetch_placesingle.cpp`, `prefetch_helpers.cpp`,
  `prefetch_prepare.cpp`, `prefetch_emit.cpp`
- `wavetable_ir.cpp`, `wavetable_front.cpp`,
  `wavetable_manager_ir.cpp`, `wavetable_manager_front.cpp`
- `awg_compiler.cpp` (waveform-collection path)

Findings:

1. `prefetch.cpp:1732..1773` — already uses `numBytesForCache`
   (renamed in IF-109 prior pass). **Verified.**
2. `prefetch_placesingle.cpp:613` — local `int sizePerDev` was the
   return of `getSizePerDevice()` (bytes). **Renamed → `sizePerDevBytes`**
   in this pass. The dependent local `int pageCount =
   sizePerDevBytes / pagesNeeded;` left as `pageCount` because it is
   semantically a count, not a byte/sample quantity (it is the number
   of pages `sizePerDevBytes` consumes).
3. `wavetable_ir.cpp:828, 866` — `size_t sampleCount = wf->signal.length();`
   already explicitly named in samples. **Verified.**
4. `awg_compiler.cpp` waveform-loop sites use `wfPtr->signal.length()`
   directly without renaming the result. The accessor name `length()`
   together with the per-call comment `// +0xD0` is unambiguous.
   **No rename.**

## Phase 44.1 cleanup: stale comments in `memory_allocator.cpp`

The IF-109 rename pass updated identifiers but left **5 stale
references** in comments to the old names. Fixed in this pass:

- Line 61:  `cacheLineSize_`        → `cacheLineSizeBytes_`
- Line 110: `memorySizeInSamples_` → `memorySizeInBytes_`
- Line 110: `cacheLineSize_`        → `cacheLineSizeBytes_`
- Line 111: `cacheLineSize_`        → `cacheLineSizeBytes_`
- Line 166: `memorySizeInSamples_` → `memorySizeInBytes_`
- Line 166: `cacheLineSize_`        → `cacheLineSizeBytes_`
- Line 263: `memorySizeInSamples_` → `memorySizeInBytes_`
- Line 300: `memorySizeInSamples_` → `memorySizeInBytes_`

(Comments only — no code change.)

## Suspected bugs filed

**None.** Every arithmetic site touching wave memory was inspected
for missing `*2` / `/2` conversions and all were consistent with the
binary's unit semantics. Specifically verified:

- `MemoryAllocator::Cache::allocate` arithmetic uses bytes throughout
  (`numBytes` parameter, `cacheLineSizeBytes_` modulus,
  `memorySizeInBytes_` wrap).
- `Waveform::getSizePerDevice` is `ceil(rounded_samples * channels *
  bitsPerSample / 8)` — bits-to-bytes conversion is explicit (`>> 3`).
- `RawWavePlaceHolder` constructor sets
  `byteSize_ = channels_ * length_ * 2` (samples → bytes for 16-bit).
  Verified at `signal.cpp:413`.
- `RawWaveHirzel16::size()` returns `data_.size() * sizeof(uint16_t)`
  i.e. samples → bytes. Verified at `rawwave.cpp:127-129`.

No new IF entries filed.

## Test status

After the 6 edits in this pass:

- Build: clean (no new warnings).
- Default manifest: 1600/1600.
- 10 named manifests:
  - core: 248/248
  - documentation: 92/92
  - errors: 6/6
  - labone: 14/14
  - large: 13/13
  - stress: 774/774
  - zhinst: 74/74
  - ziai: 459/459
  - ziasm: 468/468
  - zivibes: 259/259
  - **Subtotal: 2407/2407**
- **Grand total: 4007/4007 across 11 manifests.**

(The task spec's "2499/2499 across 6 manifests" appears to refer to
an older or differently-grouped manifest split. All 11 currently-
present manifests are clean.)

## Conclusion

Phase 44 closes with the convention `*_bytes` / `*Samples` / `length_
= samples-per-channel` consistently applied across the wave-memory
subsystem. The remaining ambiguity in
`WaveformGenerator::readWave`'s `expectedLength` parameter is
genuinely polymorphic across call sites and is documented here rather
than renamed.
