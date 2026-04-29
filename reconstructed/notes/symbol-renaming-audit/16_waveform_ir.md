# Batch 16 — waveform_ir

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 1 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 0;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 1.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

Authoritative process: `RULES-symbol-renaming.md`. Read-only scan; no
edits outside this file.

## 1. Files considered

- `reconstructed/include/zhinst/waveform_ir.hpp`
- `reconstructed/src/waveform_ir.cpp`

External cross-checks consulted (read-only, for context only):
- `reconstructed/src/wavetable_ir.cpp` (allocateWaveforms, sets `elfAlignment_`, reads `fixed_`/`crossesCacheLine_`)
- `reconstructed/src/elf_writer.cpp:141-189` (uses `elfAlignment_` as `set_align(...)`)
- `reconstructed/src/write_waves_to_elf.cpp:117-176` (uses `-elfAlignment_` as alignment mask)
- `reconstructed/src/awg_compiler.cpp:280-320,780` (reads `crossesCacheLine_`, `elfAlignment_`)
- `reconstructed/src/prefetch.cpp:695-790,2142` (reads/sets `markedForLoad`, `crossesCacheLine_`)
- `reconstructed/src/prefetch_prepare.cpp:140-160` (sets `markedForLoad`)
- `reconstructed/src/prefetch_helpers.cpp:625-647` (reads/sets `fixed_`)
- `reconstructed/notes/symbol-renaming-audit/31_device_constants.md`
  (positive evidence on `waveformAlignment` / `waveformElfAlignment`)
- `nm --demangle _seqc_compiler.so | grep WaveformIR`

Symbol-table check (excerpts; `nm --demangle`):
```
0000000000114da0 t zhinst::WaveformIR::WaveformIR(std::__1::shared_ptr<zhinst::WaveformFront>)
00000000002a9240 t zhinst::WaveformIR::WaveformIR(std::__1::shared_ptr<zhinst::Waveform>)
00000000002c5440 t zhinst::WaveformIR::toJsonElement(zhinst::SampleFormat)
```
- Type name `WaveformIR` and methods `WaveformIR(...)`, `toJsonElement` are
  authoritative — excluded from rename per RULES §3.
- The `(name, File::Type, DeviceConstants&)` ctor has **no** standalone
  binary symbol (inlined into the dispatcher at 0x2aa170 per the source
  comment) — its parameter names are reconstruction-only and in scope.
- `WaveformIR::isUsed()` and `WaveformIR::getSampleCount()` are not
  present in `nm --demangle` output (verified: empty grep) — they are
  reconstruction-only convenience accessors, in scope.
- No static data members on `WaveformIR` (none seen in `nm` output).
  All four data members below are non-static instance members; in scope.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `WaveformIR::markedForLoad` | no | medium | matches prefetch usage everywhere | keep current | not-misnomer |
| `WaveformIR::fixed_` | no | medium | placement-fixed flag, set in fixedwaves pass | keep current | not-misnomer |
| `WaveformIR::crossesCacheLine_` | no | medium | matches CL-crossing semantics | keep current | not-misnomer |
| `WaveformIR::elfAlignment_` | no | medium | used as alignment value/mask | keep current | not-misnomer |
| `WaveformIR::isUsed()` | no | low | thin wrapper over `used` | keep current | — |
| `WaveformIR::getSampleCount()` | yes | high | returns bytes, not samples | `getAllocationByteSize`, `getSizeBytes` | — |
| `WaveformIR(shared_ptr<WaveformFront>)::source` | no | medium | source object to copy from | keep current | — |
| `WaveformIR(shared_ptr<Waveform>)::source` | no | medium | source object to copy from | keep current | — |
| `WaveformIR(name,type,dc)::name` | no | high | written to `Waveform::name` | keep current | — |
| `WaveformIR(name,type,dc)::type` | no | high | written to `Waveform::waveformType` | keep current | — |
| `WaveformIR(name,type,dc)::dc` | no | medium | DeviceConstants&, used as such | keep current | — |
| `WaveformIR::toJsonElement::format` | unsure | low | param unused in recon body | keep current | in-scope (parameter name; nm only preserves SampleFormat type) |
| `WaveformIR::toJsonElement::result`/`oss`/`i`/`data`/`size`/`orResult`/`j`/`clz`/`markerBits`/`s`/`channels` | no | — | local vars match role | keep current | — |

## 3. Detailed findings

### WaveformIR::markedForLoad  [no / medium / not-misnomer]

Evidence:
- include/zhinst/waveform_ir.hpp:78 — declared `bool markedForLoad; // +0xD8`.
- src/prefetch_prepare.cpp:145,152: `waveform->markedForLoad = true;`
  inside the loop that "marks each waveform's markedForLoad" before
  load-node placement (binary 0x1c9850: `movb $0x1, 0xd8(%rax)`).
- src/prefetch.cpp:746: `if (!waveformIR->markedForLoad) { … skip }`
  inside `moveLoadsToFront` — skips waveforms that don't need a load
  inserted.
- src/prefetch.cpp:2142: `wfm->markedForLoad = true;` (binary 0x1d4e13).
- This bool is independent of `Waveform::used` (+0x48); both are read
  separately at distinct sites (e.g. wavetable_ir.cpp:278 reads `used`,
  prefetch sites read `markedForLoad`).

Interpretation:
- All write sites set the flag at the points where the prefetch passes
  decide a waveform must be loaded; all read sites consult the flag to
  gate emission of `Load` nodes. The name describes precisely that role.

Judgement:
- The name accurately reflects the observed role. Not a misnomer.

Proposals:
- keep current  (medium)

Locations consulted:
- declared: include/zhinst/waveform_ir.hpp:78
- written:  src/prefetch_prepare.cpp:145,152; src/prefetch.cpp:2142
- read:     src/prefetch.cpp:746

### WaveformIR::fixed_  [no / medium / not-misnomer]

Evidence:
- include/zhinst/waveform_ir.hpp:79 — declared `bool fixed_; // +0xD9`
  with comment "placement-fixed; partitions FIFO alloc".
- src/prefetch_helpers.cpp:629-644 — within `Prefetch::determineFixedWaves`
  (per OVERVIEW + binary 0x1cb9bf etc.): reads `if (wfm->fixed_) continue`
  to skip already-fixed waves; sets `wfm->fixed_ = true` only when
  `allocationByteSize < maxBlocks * waveformAlignment`.
- src/wavetable_ir.cpp:629,671 — `if (!wf->fixed_)` / `if (wf->fixed_)`
  used to partition fixed-vs-free waveforms in `allocateWaveformsForFifo`.

Interpretation:
- The flag is set by the "fix this waveform's placement" pass
  (`determineFixedWaves`), and read by allocator passes that route
  fixed-placement waveforms differently from free ones. Name describes
  exactly that role.

Judgement:
- Not a misnomer. The trailing `_` is conventional for member naming
  here.

Proposals:
- keep current  (medium)

Locations consulted:
- declared: include/zhinst/waveform_ir.hpp:79
- written:  src/prefetch_helpers.cpp:644
- read:     src/prefetch_helpers.cpp:630; src/wavetable_ir.cpp:629,671

### WaveformIR::crossesCacheLine_  [no / medium / not-misnomer]

Evidence:
- include/zhinst/waveform_ir.hpp:80-85 — declared with detailed comment
  "set true for filler waveforms; for normal waveforms = bit 8 of the
  MemoryAllocator block.flags … cleared when a waveform is reloaded
  into a slot that does not straddle a cache line".
- src/wavetable_ir.cpp:433 — `wf->crossesCacheLine_ = true;` for filler
  waveform path.
- src/wavetable_ir.cpp:555 — `lastWf->crossesCacheLine_ = true;` with
  comment "fillers always span a CL boundary".
- src/wavetable_ir.cpp:656,680 — `wf->crossesCacheLine_ = (block.flags >> 8) & 1`
  copying MemoryAllocator block flags bit 8 into the field.
- src/wavetable_ir.cpp:694 — `wf->crossesCacheLine_ = 0;` with comment
  "reloaded, no CL crossing".
- src/awg_compiler.cpp:288,312 — `if (wf->crossesCacheLine_) {…}` used
  to gate alignment-handling in cache emission (comment "If
  crossesCacheLine_: insert alignedAddr into set, then accumulate").
- src/prefetch.cpp:786 — `bool useDA = waveformIR->crossesCacheLine_;`
  read inside Load-emission logic.
- Binary cross-check: comments cite addresses 0x2a9e47, 0x2aa88b,
  0x2ad023, 0x2ad0ac for write sites and 0x119101 for the awg_compiler
  read; consistent.

Interpretation:
- Every write site is a place where the waveform's address-block has
  been determined to cross (or not cross) a cache-line boundary — either
  because it is a filler (forced true) or because the allocator's bit 8
  indicates it. Every read site uses that bit to decide whether
  cache-line-crossing handling is needed.

Judgement:
- Name unambiguously matches its observed role. Not a misnomer.

Proposals:
- keep current  (medium)

Locations consulted:
- declared: include/zhinst/waveform_ir.hpp:80
- written:  src/wavetable_ir.cpp:433,555,656,680,694
- read:     src/awg_compiler.cpp:312; src/prefetch.cpp:786

### WaveformIR::elfAlignment_  [no / medium / not-misnomer]

Evidence:
- include/zhinst/waveform_ir.hpp:87 — declared `int32_t elfAlignment_;
  // +0xDC per-waveform allocation size (from DC)`.  ← header comment
  is internally inconsistent with how the field is used (see below).
- src/elf_writer.cpp:163-164 — `uint32_t alignment = wfPtr->elfAlignment_;
  seg->set_align(alignment);` — passed directly as ELFIO segment
  alignment.
- src/elf_writer.cpp:174 — also fed to `ddSec->set_addr_align(alignment)`.
- src/write_waves_to_elf.cpp:149 — `uint32_t alignMask =
  static_cast<uint32_t>(-wf->elfAlignment_); uint32_t padding = gap & alignMask;`
  i.e. used as a power-of-two alignment mask.
- src/awg_compiler.cpp:784 — same `-elfAlignment_` mask pattern.
- src/wavetable_ir.cpp:325 — `wf->elfAlignment_ = dc->waveformAlignment;
  // DC+0x14`.
- src/waveform_ir.cpp:37,59,126 — written from `dc->waveformElfAlignment`
  (DC+0x24) at construction time.
- Cross-batch (batch 31, `device_constants.md`): the *source* field
  `DeviceConstants::waveformElfAlignment` was flagged `yes / medium`
  as a possibly misleading "ELF" qualifier (also used in
  node-index arithmetic). On the **WaveformIR** side, however, the
  field is consumed exclusively as an alignment value passed to ELF
  writer and as an alignment mask in ELF write-out. No usage as a
  size, count, or any other quantity.

Interpretation:
- Despite the in-source comment claiming "per-waveform allocation
  size", every read site uses the field as an alignment value (either
  passed to `set_align`/`set_addr_align`, or two's-complement-negated
  to form an alignment mask). The "Alignment" suffix in the name is
  factually correct. The "Elf" prefix is correct on this class because
  every read site is in ELF-writing code paths
  (`elf_writer`, `write_waves_to_elf`, the cache-emit branch of
  `awg_compiler`).

Judgement:
- The name accurately describes the role at every read site on this
  class. The misleading "ELF" qualifier exists upstream on
  `DeviceConstants::waveformElfAlignment`, not here. The hpp:44
  comment "per-waveform allocation size" is stale and contradicts the
  uses, but the *name* itself is fine.

Proposals:
- keep current  (medium)
- (out-of-band note) update header comment at hpp:44/87 to "alignment
  used by ELF writer for this waveform's PT_LOAD segment".

Cross-reference:
- Source field `DeviceConstants::waveformElfAlignment` audited in
  batch 31 (proposal: `nodeIndexBase` / `wordAlignment`). If that
  rename happens, the assignment in the WaveformFront / Waveform
  ctors here changes mechanically; the WaveformIR field name itself
  does not need to change.

Locations consulted:
- declared: include/zhinst/waveform_ir.hpp:87
- written:  src/waveform_ir.cpp:37,59,126; src/wavetable_ir.cpp:325
- read:     src/elf_writer.cpp:163,174; src/write_waves_to_elf.cpp:149;
            src/awg_compiler.cpp:784

### WaveformIR::getSampleCount()  [yes / high / —]

Evidence:
- include/zhinst/waveform_ir.hpp:93 —
  `int getSampleCount() const { return allocationByteSize; }
   // Waveform::allocationByteSize +0x74`.
- `Waveform::allocationByteSize` is genuinely a byte count: the value
  written into it at src/wavetable_ir.cpp:302-304,333 is
  `((totalBits + 7) / 8)` further rounded to 64, i.e. **bytes**, not
  samples. Comment at src/wavetable_ir.cpp:333 reads "store computed
  size".
- Sole call site src/wavetable_ir.cpp:336:
  `totalSamples += wf->getSampleCount();` — accumulates into a local
  named `totalSamples` (declared line 262 with comment "unused but
  kept for symmetry with binary stack layout"; not actually consumed).
- Header comment at line 93 self-documents that the function returns
  `allocationByteSize`.
- Not present in `nm --demangle` output (reconstruction-only inline
  accessor). In scope per RULES §3.

Interpretation:
- The function returns a byte count but is named "sample count". A
  sample is a 16-bit unit (or larger for multi-channel/multi-bit
  formats). Bytes ≠ samples in any case where bitsPerSample > 8 or
  channels > 1.

Judgement:
- Misnomer with high confidence: name says "samples", value is bytes.
  The single call site (which feeds an unused accumulator) does not
  exercise the bug, but the name actively misleads any future caller.

Proposals:
- `getAllocationByteSize`  (high)
- `getSizeBytes`           (medium)
- delete the accessor (the only caller is dead code per the line-262
  comment) and use `allocationByteSize` directly  (low)

Locations consulted:
- declared: include/zhinst/waveform_ir.hpp:93
- used:     src/wavetable_ir.cpp:336

### WaveformIR::isUsed()  [no / low / —]

Evidence:
- include/zhinst/waveform_ir.hpp:92 — `bool isUsed() const { return used; }`
  thin forwarder to `Waveform::used` at +0x48.
- src/wavetable_ir.cpp:770 — `if (!waveform->isUsed() || …)` — used
  exactly as a "is this waveform used" query.
- Not in `nm --demangle` output (reconstruction-only).

Interpretation:
- One-line wrapper that returns the `used` member. Name is consistent
  with the field. Sole call site uses it idiomatically.

Judgement:
- Name is fine. Listed only for completeness because the parallel
  accessor `getSampleCount()` is a misnomer.

Proposals:
- keep current  (low)

Locations consulted:
- declared: include/zhinst/waveform_ir.hpp:92
- used:     src/wavetable_ir.cpp:770

### WaveformIR::toJsonElement::format  [unsure / low / verify-not-original]

Evidence:
- src/waveform_ir.cpp:151 — parameter declared
  `WaveformIR::toJsonElement(SampleFormat format) const`.
- Reconstructed body (lines 152-218) never references `format`.
- Binary symbol `zhinst::WaveformIR::toJsonElement(zhinst::SampleFormat)`
  exists (0x2c5440); the parameter is part of the public method
  signature (the type is encoded in the mangling, the *name* is not).
- The reconstructed body is acknowledged as approximate
  (no method-body comment claims byte-perfect reconstruction).

Interpretation:
- The parameter name cannot be misleading vs. observed body usage if
  it has no observed body usage. The original binary very likely uses
  `format` to select sample-format-dependent output (the surrounding
  marker_bits computation is sample-format dependent in the original).
  The name `format` is generic but matches the type `SampleFormat`.

Judgement:
- Cannot judge definitively from the reconstruction; name is at worst
  generic. Flagged `unsure` with `verify-not-original` so that
  synthesis can recheck once the toJsonElement reconstruction is
  fleshed out.

Proposals:
- keep current  (medium)
- `sampleFormat`  (low)

Locations consulted:
- declared/defined: src/waveform_ir.cpp:151

## 4. Symbols inspected and judged routinely fine

- `WaveformIR(std::shared_ptr<WaveformFront>)::source` — copy-source
  pointer; idiomatic.
- `WaveformIR(std::shared_ptr<Waveform>)::source` — same.
- `WaveformIR(const std::string&, Waveform::File::Type, const DeviceConstants&)::name`
  — written into `Waveform::name`; perfect match.
- `WaveformIR(...)::type` — written into `Waveform::waveformType`;
  matches.
- `WaveformIR(...)::dc` — `DeviceConstants&`, idiomatic short name for
  this type seen across the codebase (e.g. wavetable_ir.cpp).
- `toJsonElement` locals: `result` (the ptree being built), `oss`
  (ostringstream for marker_bits), `i`/`j` (loop indices), `data`
  (pointer to markerBits buffer), `size` (length of markerBits buffer),
  `orResult` (OR-accumulator over low 2 bits), `clz` (leading-zero
  count), `markerBits` (computed bit-width), `s` (string repr of
  markerBits), `channels` (signal.channels()). All match their roles.

Symbols deliberately **not** inspected (out of scope per RULES §3):
- Type name `WaveformIR` — present in `nm` symbol table.
- Method names `WaveformIR(...)`, `toJsonElement` — present in `nm`.
- Member type alias `WaveformIR::ptree = boost::property_tree::basic_ptree<…>`
  — RULES §2 excludes member type aliases.
- Forward-declared `boost::property_tree::basic_ptree` template params
  `Key`, `Data`, `KeyCompare` — template parameters, RULES §2.
- Forward declaration `struct WaveformFront` — type from another batch
  (#17).

## 5. Coverage

- **Fully covered:**
  - Data members: `markedForLoad`, `fixed_`, `crossesCacheLine_`,
    `elfAlignment_` (4/4).
  - Inline accessors: `isUsed()`, `getSampleCount()` (2/2).
  - Constructor parameters: all three constructors (`source`, `source`,
    `name`/`type`/`dc`) and the `toJsonElement::format` parameter (5/5).
  - Locals in the one in-file body that has them
    (`toJsonElement`): all 11 in §4.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type name `WaveformIR` — `nm` exclusion.
  - Method names `WaveformIR(…)`, `toJsonElement` — `nm` exclusion.
  - Member type alias `ptree` — RULES §2.
  - Forward declarations / template params on
    `boost::property_tree::basic_ptree` — RULES §2.
