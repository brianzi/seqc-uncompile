# Batch 35 — rawwave

## 1. Files considered

- `reconstructed/include/zhinst/waveform/rawwave.hpp`
- `reconstructed/src/waveform/rawwave.cpp`

Cross-reference reads:
- `reconstructed/include/zhinst/waveform/signal.hpp` (Signal members `samples_`,
  `markers_`, `markerBits_`, `channels_`, `length_`, `reserveOnly_`)
- `reconstructed/src/waveform/signal.cpp` (`Signal::getRawData` — sole construction
  site for all three RawWave subclasses)
- `reconstructed/src/waveform/util_wave.cpp` (`double2awg`, `double2awg1m`,
  `double2awg16` definitions)
- `reconstructed/src/io/cached_parser.cpp` (sole caller of `util::wave::hash`)
- `reconstructed/src/io/elf_writer.cpp`,
  `reconstructed/src/waveform/write_waves_to_elf.cpp` (RawWave consumers — only
  use `size()` and `ptr()` via the abstract base)
- `reconstructed/notes/struct_layouts.md` (struct offsets)

Symbol-table verification (`nm --demangle _seqc_compiler.so | grep
RawWave|util::wave|MarkerBits`) — all of the following are excluded
from rename per RULES §3:

- Types: `RawWave`, `RawWavePlaceHolder`, `RawWaveHirzel16`,
  `RawWaveCervino` (typeinfo + vtable symbols present).
- Methods: `~RawWavePlaceHolder`, `~RawWaveHirzel16`, `~RawWaveCervino`,
  `RawWavePlaceHolder::size`, `RawWavePlaceHolder::ptr`,
  `RawWaveHirzel16::size`, `RawWaveHirzel16::ptr`,
  `RawWaveCervino::size`, `RawWaveCervino::ptr`,
  `RawWaveHirzel16::RawWaveHirzel16` (3-arg ctor mangled).
- Free functions: `util::wave::double2awg`, `util::wave::double2awg1m`,
  `util::wave::double2awg16`, `util::wave::hash`.
- Type alias `MarkerBitsPerChannel` is referenced in mangled names of
  `Signal::Signal(...,MarkerBitsPerChannel const&)` and
  `RawWaveHirzel16::RawWaveHirzel16(...,MarkerBitsPerChannel const&)`,
  so the alias name itself is encoded — excluded.

`nm` shows no static data members in scope for any of these classes.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `util::wave::double2awg::sample` | no | high | matches semantics + comments | keep current | not-misnomer |
| `util::wave::double2awg::marker` | no | high | low-2-bit marker payload | keep current | not-misnomer |
| `util::wave::double2awg1m::sample` | no | medium | matches usage | keep current | — |
| `util::wave::double2awg1m::marker` | no | medium | low-1-bit marker payload | keep current | — |
| `util::wave::double2awg16::sample` | no | medium | matches usage | keep current | — |
| `util::wave::hash::filePath` | no | high | passed to file-open | keep current | not-misnomer |
| `RawWavePlaceHolder::byteSize_` | no | high | bytes = ch*len*2 | keep current | not-misnomer |
| `RawWavePlaceHolder::buffer_` | no | low | generic but accurate | keep current | — |
| `RawWaveHirzel16::data_` | no | low | generic; conventional | keep current | — |
| `RawWaveHirzel16::RawWaveHirzel16::samples` | no | high | vector\<double\> waveform | keep current | not-misnomer |
| `RawWaveHirzel16::RawWaveHirzel16::markers` | no | medium | per-sample marker bytes | keep current | — |
| `RawWaveHirzel16::RawWaveHirzel16::markerBits` | no | high | per-channel bit-count | keep current | not-misnomer |
| `RawWaveHirzel16::RawWaveHirzel16::markerMode` (local) | no | low | invented in recon | keep current | — |
| `RawWaveCervino::data_` | no | low | generic; conventional | keep current | — |

## 3. Detailed findings

### `util::wave::double2awg::sample`  [no / high / not-misnomer]

Evidence:
- `util_wave.cpp:41` `uint16_t double2awg(double sample, unsigned int marker)`
- `util_wave.cpp:44` `if (sample > 1.0)` — clamped to ±1.0 then scaled by 8191.
- `util_wave.cpp:29` comment `// 14-bit signed waveform sample with 2 marker bits`.
- Call sites pass an element from `Signal::samples_` (a
  `std::vector<double>` of waveform samples), e.g. `signal.cpp:432`
  `raw->data_[i] = util::wave::double2awg(samples_[i], markers_[i]);`
  and `rawwave.cpp:111`.

Interpretation:
- The double argument is a single waveform sample value in the [-1, 1]
  range. Header comments and code uniformly treat it as such.

Judgement:
- Name accurately describes its role; not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/waveform/rawwave.hpp:30
- defined:  src/waveform/util_wave.cpp:41
- used:     src/waveform/signal.cpp:432; src/waveform/rawwave.cpp:111-112

### `util::wave::double2awg::marker`  [no / high / not-misnomer]

Evidence:
- `util_wave.cpp:39` comment `// Pack: (result << 2) | (marker & 0x3)`.
- `util_wave.cpp:50` `(marker & 0x3u) + (rounded * 4)`.
- Call sites pass per-sample marker bytes from `Signal::markers_`
  (`vector<uint8_t>`), implicitly widened to `unsigned`.

Interpretation:
- The argument carries the marker bits to be packed into the low two
  bits of the encoded 16-bit word.

Judgement:
- Name matches the bit-packing semantics; not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/waveform/rawwave.hpp:30
- defined:  src/waveform/util_wave.cpp:41-51
- used:     src/waveform/signal.cpp:432; src/waveform/rawwave.cpp:111

### `util::wave::hash::filePath`  [no / high / not-misnomer]

Evidence:
- Header comment (`rawwave.hpp:33-36`): "SHA-256 of file contents at
  filePath. Returns 8 uint32 words ... Empty vector on file-open
  failure."
- Sole caller (`cached_parser.cpp:496`):
  `return util::wave::hash(filePath);` inside `CachedParser::getHash`,
  where `filePath` is itself a path passed in from cache lookup.

Interpretation:
- The string is opened as a file, so it is unambiguously a path.

Judgement:
- Name fits role precisely; not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/waveform/rawwave.hpp:36
- used:     src/io/cached_parser.cpp:496

### `RawWavePlaceHolder::byteSize_`  [no / high / not-misnomer]

Evidence:
- `signal.cpp:411-413` comment + assignment:
  `// byteSize_ at offset +0x08 = channels_ * length_ * 2`
  `placeholder->byteSize_ = static_cast<size_t>(channels_) * length_ * 2;`
  (samples are 16-bit, so `*2` is bytes).
- `rawwave.cpp:32` `return byteSize_;` — returned directly from
  `size()` whose contract is "byte count" for ELF `.wf_*` writing
  (`write_waves_to_elf.cpp:164`: `call rawData->size() ... add to ebx`
  used to size memory).
- `rawwave.cpp:46-54` `ptr()` resizes `buffer_` to `byteSize_` and
  returns `data()` — confirming it is the buffer length in bytes.

Interpretation:
- Field is consistently a byte count, computed as channels × length × 2
  and exposed verbatim through `size()`.

Judgement:
- Name correctly identifies the unit (bytes). Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/waveform/rawwave.hpp:82
- read:     src/waveform/rawwave.cpp:32, 48-51
- written:  src/waveform/signal.cpp:413

### `RawWaveHirzel16::RawWaveHirzel16::markers`  [no / medium / —]

Evidence:
- `rawwave.cpp:80` `std::vector<uint8_t> const& markers`.
- Indexed in lockstep with `samples` at `rawwave.cpp:106,112` —
  `markers[i]` paired with `samples[i]`.
- Bound at the call site (`signal.cpp:419`) to `Signal::markers_`,
  which `signal.hpp:39` documents as a per-sample marker byte vector
  parallel to `samples_`.

Interpretation:
- Parameter is the per-sample marker stream. Name `markers` faithfully
  matches the field it is bound to and the usage pattern.

Judgement:
- Name fits; not a misnomer. Confidence is medium because `markers`
  is a generic plural and the more specific name "markers per sample"
  is left implicit, but the binding to `Signal::markers_` makes it
  consistent.

Proposals:
- keep current  (medium)

Locations consulted:
- declared: include/zhinst/waveform/rawwave.hpp:111
- defined:  src/waveform/rawwave.cpp:78-115
- caller:   src/waveform/signal.cpp:419

### `RawWaveHirzel16::RawWaveHirzel16::markerBits`  [no / high / not-misnomer]

Evidence:
- Type is `MarkerBitsPerChannel` (= `vector<uint8_t>`) — alias name
  itself encodes the semantic.
- Used (rawwave.cpp:93-95) only to compute
  `markerMode |= (markerBits[i] & 0x03)` — i.e. each entry is
  inspected for *which marker bit positions are active on that
  channel*, not for sample values. Length is bounded by channel count,
  not sample count (rawwave.cpp:84 still uses `samples.size()` for
  data length).
- Bound at call site to `Signal::markerBits_` (signal.hpp:40), a
  separate field from `markers_`.

Interpretation:
- The argument describes marker-bit usage *per channel*, distinct from
  the per-sample `markers` stream.

Judgement:
- Name correctly distinguishes this from the per-sample `markers`
  parameter; not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/waveform/rawwave.hpp:112
- defined:  src/waveform/rawwave.cpp:78-115
- caller:   src/waveform/signal.cpp:419
- counterpart field: signal.hpp:40

### `RawWaveHirzel16::RawWaveHirzel16::markerMode` (local)  [no / low / —]

Evidence:
- `rawwave.cpp:92` `uint8_t markerMode = 0;`
- Loop at `rawwave.cpp:93-95` ORs all `markerBits[i] & 0x03` into it.
- Switch on values 0, 1, ≥2 picks `double2awg16`, `double2awg1m`,
  `double2awg` respectively.

Interpretation:
- The local is a 2-bit summary of which marker-bit positions are in
  use across any channel; it selects the encoding path. The name was
  invented in reconstruction (no original binary local-variable
  symbols).

Judgement:
- Descriptive enough for the role; not flagged. Low confidence
  because alternatives like `markerBitsUnion` or `anyMarkerBits`
  could be marginally clearer, but no concrete evidence of confusion
  in current usage.

Proposals:
- keep current  (low)
- `markerBitsUnion` (low)

Locations consulted:
- declared/used: src/waveform/rawwave.cpp:92-115

## 4. Symbols inspected and judged routinely fine

- `util::wave::double2awg1m::sample` / `marker` — same semantics as the
  2-bit variant, comments at `util_wave.cpp:54-73` are explicit.
- `util::wave::double2awg16::sample` — single sample, no marker; matches
  use at `rawwave.cpp:100`.
- `RawWavePlaceHolder::buffer_` — generic but accurate
  (`vector<char>` lazily grown to `byteSize_`); no contradicting
  evidence.
- `RawWaveHirzel16::data_` and `RawWaveCervino::data_` — both are the
  encoded `vector<uint16_t>` returned via `ptr()`. Generic but
  consistent with the `data()` accessor on `std::vector` and with the
  abstract `ptr()` contract.

## 5. Coverage

**Fully covered:**
- All in-scope symbols in `rawwave.hpp` and `rawwave.cpp`:
  - 4 free-function parameters across `double2awg`, `double2awg1m`,
    `double2awg16`, plus the parameter of `util::wave::hash`.
  - 3 instance fields (`RawWavePlaceHolder::byteSize_`,
    `RawWavePlaceHolder::buffer_`, `RawWaveHirzel16::data_`,
    `RawWaveCervino::data_`).
  - 3 ctor parameters of `RawWaveHirzel16::RawWaveHirzel16`.
  - 1 reconstruction-introduced local (`markerMode`).
- Sample-units / scaling watchout from prompt: confirmed `byteSize_`
  is indeed bytes (×2 for 16-bit samples) and that `double2awg*`
  conversion uses ±8191 full scale (per `util_wave.cpp:36-37`,
  consistent with prior IF-45 finding).
- I/Q channel indexing: not surfaced as a separate concept here —
  channel multiplicity is handled upstream in `Signal`; rawwave only
  sees a flat sample/marker stream.

**Deferred:** none.

**Not covered (out of scope per RULES §2/§3):**
- All four type names (`RawWave`, `RawWavePlaceHolder`,
  `RawWaveHirzel16`, `RawWaveCervino`) — present as typeinfo/vtable
  symbols in the binary.
- All method names (`size`, `ptr`, destructors) — present in mangled
  symbol table.
- Free-function names (`double2awg`, `double2awg1m`, `double2awg16`,
  `hash`) — present in mangled symbol table.
- The `MarkerBitsPerChannel` type alias — encoded in mangled names of
  `Signal::Signal(...,MarkerBitsPerChannel const&)` and
  `RawWaveHirzel16::RawWaveHirzel16(...,MarkerBitsPerChannel const&)`.
- Template parameters — none in this file.
- No macros are defined in `rawwave.hpp`.
- No namespace-scope constants are defined in this batch (the
  `kFullScale = 8191.0` constants live in `util_wave.cpp`, batch for
  `util_wave`, not here).

No cross-batch arbitrations needed — the field/parameter naming
between `RawWaveHirzel16` ctor and its sole caller `Signal::getRawData`
is fully consistent (`samples` ↔ `samples_`, `markers` ↔ `markers_`,
`markerBits` ↔ `markerBits_`).
