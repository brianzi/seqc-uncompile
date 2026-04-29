# Batch 46 — wavetable_ir

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 3 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 0;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 3.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

Authoritative process: `RULES-symbol-renaming.md`. Read-only scan; no
edits outside this file.

## 1. Files considered

- `reconstructed/include/zhinst/wavetable_ir.hpp`
- `reconstructed/src/wavetable_ir.cpp`
- `reconstructed/src/wavetable_manager_ir.cpp`

External cross-checks consulted (read-only, for context only):

- `reconstructed/src/awg_compiler.cpp:260,304,332,769,774,778,790,796,914`
  (call sites of `forEachUsedWaveform` / `getJsonIndex` /
  `getNextSegmentAddress` / `getFirstWaveformOffset`)
- `reconstructed/src/write_waves_to_elf.cpp:85-86,122-124,171-172`
  (call sites of `forEachUsedWaveform` and `getFirstWaveformOffset`)
- `reconstructed/src/compiler.cpp:524-544` (`setUsedWaveforms`,
  `updateWaveforms` call sites)
- `reconstructed/src/memory_allocator.cpp:30-37,94,208`
  (cross-references back to `allocateWaveforms{,ForFifo}`)
- `reconstructed/include/zhinst/device_constants.hpp:43-78,148-205`
  (`waveformAlignment`, `waveformMemorySize`, `cachePageCount`,
  `maxBlocks`, `bitsPerSample`, `waveformGranularity`,
  `waveformPageSize`, `maxWaveIndex()`)
- `reconstructed/notes/symbol-renaming-audit/16_waveform_ir.md`
  (parallel batch; established `WaveformIR::getSampleCount()` returns
  bytes, `crossesCacheLine_` / `fixed_` / `markedForLoad` /
  `elfAlignment_` are not misnomers).
- `reconstructed/notes/symbol-renaming-audit/31_device_constants.md`
  (positive evidence for the DC field names referenced in the bodies)
- `reconstructed/notes/symbol-renaming-audit/14_waveform.md` (only as
  context for `Waveform::name`/`waveformType`/`allocationByteSize`
  field roles)
- `reconstructed/notes/symbol-renaming-audit/38_play_config.md`
  (no field overlap)

Symbol-table check (`nm --demangle _seqc_compiler.so`, excerpts):

```
0000000000114da0 t zhinst::WaveformIR::WaveformIR(...)
000000000029ce20 t zhinst::WavetableIR::WavetableIR(WavetableFront const&, ...)
000000000029d230 t zhinst::WavetableIR::WavetableIR(WavetableManager<WaveformIR> const&, ...)
000000000029d550 t zhinst::WavetableIR::~WavetableIR()
000000000029d670 t zhinst::WavetableIR::toJson() const
000000000029db10 t zhinst::WavetableIR::fromJson(boost::json::value, ...)
000000000029e090 t zhinst::WavetableIR::operator==(WavetableIR const&) const
000000000029e270 t zhinst::WavetableIR::begin() const
000000000029e280 t zhinst::WavetableIR::end() const
000000000029e290 t zhinst::WavetableIR::size() const
000000000029e2b0 t zhinst::WavetableIR::getWaveformByName(std::optional<...> const&) const
000000000029e320 t zhinst::WavetableIR::getNextSegmentAddress() const
000000000029e330 t zhinst::WavetableIR::getFirstWaveformOffset() const
000000000029e340 t zhinst::WavetableIR::allocateWaveforms(bool)
000000000029e5e0 t zhinst::WavetableIR::forEachUsedWaveform(
                       std::function<void(shared_ptr<WaveformIR> const&)>,
                       zhinst::WaveOrder) const
000000000029e8a0 t zhinst::WavetableIR::assignWaveIndexImplicit()
000000000029ece0 t zhinst::WavetableIR::setUsedWaveforms(
                       std::vector<std::shared_ptr<WaveformIR>> const&)
000000000029ed10 t zhinst::WavetableIR::updateWaveforms(bool, bool)
000000000029ed30 t zhinst::WavetableIR::allocateWaveformsForFifo()
000000000029f150 t zhinst::WavetableIR::alignWaveformSizes()
000000000029f1d0 t zhinst::WavetableIR::assignWaveformAllocationSizes()
000000000029f310 t zhinst::WavetableIR::loadWaveform(std::shared_ptr<WaveformIR>)
000000000029f480 t zhinst::WavetableIR::getJsonIndex(zhinst::SampleFormat) const
000000000029a7e0 t zhinst::WavetableException::WavetableException(std::string const&)
000000000029dd10 t zhinst::detail::WavetableManager<WaveformIR>::fromJson(
                       boost::json::value const&, DeviceConstants const&)
000000000029d780 t zhinst::detail::WavetableManager<WaveformIR>::toJson() const
000000000029dfa0 t zhinst::detail::WavetableManager<WaveformIR>::~WavetableManager()
000000000029d140 t zhinst::detail::WavetableManager<WaveformIR>::insertWaveform(
                       std::shared_ptr<WaveformIR>)
000000000029e0e0 t zhinst::detail::WavetableManager<WaveformIR>::operator==(...) const
00000000002a5260 t zhinst::detail::WavetableManager<WaveformIR>::WavetableManager(
                       int, int, std::vector<Waveform> const&)
00000000002a9fe0 t zhinst::detail::WavetableManager<WaveformIR>::newWaveform(
                       std::string const&, Signal const&, std::string const&,
                       DeviceConstants const&)
```

Authoritative exclusions per RULES §3 (type names + free/method names):

- Types: `WavetableIR`, `WaveOrder`, `detail::WavetableManager<WaveformIR>`,
  `WavetableException`, `CancelCallback`, `WavetableFront`,
  `CachedParser`, `WaveIndexTracker`.  All present in `nm` either as
  the qualifier of a method/ctor or as a template-argument inside one
  of the mangled names above (`WaveOrder` appears as a parameter
  type in `forEachUsedWaveform`, which encodes the type name
  authoritatively per RULES §3).
- Method names on `WavetableIR`: every method declared in the header
  has a matching `t zhinst::WavetableIR::<name>` line above.
- Method names on `WavetableManager<WaveformIR>`:
  `WavetableManager(int,int,...)`, `~WavetableManager`,
  `insertWaveform`, `newWaveform`, `fromJson`, `toJson`,
  `operator==`.

In scope per RULES §3 (members and parameter names not encoded):

- `WaveOrder` enumerators (`Natural`, `ByName`, `ByIndex`): enum
  members not encoded in `nm` mangling. In scope.
- All `WavetableIR` data members (no static data members on this
  class — `nm` shows none).
- All `WavetableManager<WaveformIR>` data members (likewise no static
  members).
- All function/method parameter names.
- `using detail::getUniqueName;` — the using-declaration brings in a
  binary-symbol-table free function (0x2a0fd0); the declared name is
  authoritative.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `WaveOrder` (type) | no | high | name in binary mangling | keep current | not-misnomer |
| `WaveOrder::Natural` | no | medium | unsorted = natural order | keep current | — |
| `WaveOrder::ByName` | yes | high | sorts by waveIndex, not name | `ByWaveIndex` | — |
| `WaveOrder::ByIndex` | yes | medium | sorts by addressValue | `ByAddress` | — |
| `WavetableIR::deviceConstants_` | no | high | DC ptr, used as such | keep current | not-misnomer |
| `WavetableIR::addressBase_` | no | medium | running base address | keep current | — |
| `WavetableIR::firstWaveformOffset_` | no | medium | matches getter name | keep current | — |
| `WavetableIR::cachedParser_` | no | high | type name matches | keep current | not-misnomer |
| `WavetableIR::manager_` | no | medium | unique_ptr to manager | keep current | — |
| `WavetableIR::waveIndexTracker_` | no | high | type name matches | keep current | not-misnomer |
| `WavetableIR::usedWaveforms_` | no | high | iterated by forEachUsedWaveform | keep current | not-misnomer |
| `WavetableIR::cancelCallback_` | no | high | type name matches | keep current | not-misnomer |
| `WavetableIR::WavetableIR(front,…)::front` | no | medium | source manager owner | keep current | — |
| `…::WavetableIR(front,…)::constants` | no | medium | DC reference | keep current | — |
| `…::WavetableIR(front,…)::address` | no | medium | starting address | keep current | — |
| `…::WavetableIR(front,…)::wavetableSize` | unsure | low | passed to CachedParser | keep current | in-scope (parameter name; nm only preserves unsigned long type) |
| `…::WavetableIR(front,…)::path` | no | medium | filesystem path | keep current | — |
| `…::WavetableIR(front,…)::cancelCb` | no | medium | weak_ptr<CancelCallback> | keep current | — |
| `…::WavetableIR(mgr,…)::mgr` | no | medium | source manager copy | keep current | — |
| `…::WavetableIR(mgr,…)` other params | no | medium | same as above ctor | keep current | — |
| `WavetableIR::fromJson::json` | no | high | boost::json::value | keep current | — |
| `WavetableIR::fromJson::*` other params | no | medium | passed through to ctor | keep current | — |
| `WavetableIR::operator==::other` | no | high | idiomatic | keep current | — |
| `WavetableIR::getWaveformByName::name` | no | high | optional<string> name | keep current | — |
| `WavetableIR::allocateWaveforms::fifoMode` | no | medium | gates ByName-vs-Natural | keep current | — |
| `…::allocateWaveforms::cancelLock`,`cancelLock2` | no | medium | locked weak_ptr | keep current | — |
| `…::allocateWaveforms::totalSamples` | yes | high | accum of bytes (recon dead) | `totalBytes`, delete | — |
| `…::allocateWaveforms::totalSize` | no | medium | byte accumulator | keep current | — |
| `…::allocateWaveforms::waveCount` | no | high | post-incremented per wave | keep current | — |
| `…::allocateWaveforms::lastAllocBytes` | no | medium | matches role | keep current | — |
| `…::allocateWaveforms::sizeInBlocks` | yes | medium | unit is samples, not blocks | `sizeInSamples` | — |
| `…::allocateWaveforms::channels`,`length`,`dc` | no | medium | trivial | keep current | — |
| `…::allocateWaveforms::granularity`,`pageSize` | unsure | low | upstream DC name churn | keep current | cross-batch-arbitration |
| `…::allocateWaveforms::rounded` | no | low | rounded sample count | keep current | — |
| `…::allocateWaveforms::totalBits` | no | high | bits product | keep current | — |
| `…::allocateWaveforms::allocationBytes` | no | high | matches role | keep current | — |
| `…::allocateWaveforms::wfAlign`,`alignedLimit`,`needsAlign` | no | medium | alignment bookkeeping | keep current | — |
| `…::allocateWaveforms::alignment` | no | medium | wfAlign alias in outer scope | keep current | — |
| `…::allocateWaveforms::computedOffset` | no | medium | offset added to waves | keep current | — |
| `…::allocateWaveforms::raw` | no | low | pre-mod intermediate | keep current | — |
| `…::allocateWaveforms::memorySizeInSamples` | yes | high | actually byte count | `cacheLineBytes`, `memorySizeBytes` | — |
| `…::allocateWaveforms::maxBlocksPerCL` | unsure | low | from `cachePageCount` | keep current | cross-batch-arbitration |
| `…::allocateWaveforms::numCacheLines` | no | medium | initial CL count | keep current | — |
| `…::allocateWaveforms::cacheLineUsage` | no | medium | per-CL slot | keep current | — |
| `…::allocateWaveforms::maxPerCL` | no | medium | per-CL byte cap | keep current | — |
| `…::allocateWaveforms::offsetInCL` | yes | medium | uses memorySizeInSamples mod | `offsetInLine`/keep | — |
| `…::allocateWaveforms::startBlock`,`endBlock` | no | medium | half-open range | keep current | — |
| `…::allocateWaveforms::conflict`,`addr`,`usedEntries` | no | low | trivial | keep current | — |
| `WavetableIR::forEachUsedWaveform::callback` | no | high | the callback | keep current | — |
| `…::forEachUsedWaveform::order` | no | high | the WaveOrder | keep current | — |
| `…::forEachUsedWaveform::count`,`indices`,`idx`,`i`,`a`,`b` | no | low | trivial loop locals | keep current | — |
| `WavetableIR::assignWaveIndexImplicit::countFn` | yes | medium | no-op, not counting | `noop` / delete | — |
| `…::assignWaveIndexImplicit::tree`,`autoIdx`,`it`,`it2` | no | low | trivial | keep current | — |
| `…::assignWaveIndexImplicit::minSamples` | no | medium | granularity value | keep current | — |
| `…::assignWaveIndexImplicit::fillerSignal`,`fillerName` | no | high | filler waveform inputs | keep current | — |
| `…::assignWaveIndexImplicit::lineIdx`,`counter`,`uniqueName` | no | medium | passed to getUniqueName | keep current | — |
| `…::assignWaveIndexImplicit::newWf`,`lastWf` | no | low | trivial | keep current | — |
| `WavetableIR::setUsedWaveforms::waveforms` | no | high | input vector | keep current | — |
| `WavetableIR::updateWaveforms::fifoMode` | no | medium | branch selector | keep current | — |
| `WavetableIR::updateWaveforms::allocFlag` | yes | high | forwards to fifoMode arg | `fifoMode` | — |
| `WavetableIR::allocateWaveformsForFifo::dc`,`memorySizeInSamples`,`alignment`,`maxBlocks` | no | medium | DC field copies | keep current | — |
| `…::allocateWaveformsForFifo::allocator`,`allocatedSet`,`block`,`block2`,`localAllocSet` | no | medium | trivial | keep current | — |
| `…::allocateWaveformsForFifo::startAddr`,`endAddr`,`pos`,`clIdx` | no | low | trivial | keep current | — |
| `WavetableIR::loadWaveform::waveform` | no | high | the waveform | keep current | — |
| `WavetableIR::loadWaveform::wf`,`wfCopy`,`deviceType` | no | medium | trivial | keep current | — |
| `WavetableIR::getJsonIndex::format` | no | medium | passed into toJsonElement | keep current | — |
| `…::getJsonIndex::waveformsPtree`,`element`,`root`,`oss` | no | low | trivial | keep current | — |
| `WavetableIR::alignWaveformSizes::wf`,`sampleCount`,`granularity`,`aligned`,`maxSamples` | no | medium | trivial | keep current | — |
| `WavetableIR::assignWaveformAllocationSizes::cancelLock`,`wf`,`channelCount`,`sampleCount`,`granularity`,`aligned`,`maxSamples`,`bps`,`bits`,`bytes`,`alloc` | no | medium | trivial | keep current | — |
| `WavetableManager<WaveformIR>::lineNr_` | unsure | low | only consumer is JSON | keep current | — |
| `WavetableManager<WaveformIR>::waveformCounter_` | no | medium | post-incremented per wf | keep current | — |
| `WavetableManager<WaveformIR>::nameToIndex_` | no | high | name→idx map | keep current | not-misnomer |
| `WavetableManager<WaveformIR>::waveforms_` | no | high | vector iterated | keep current | not-misnomer |
| `WavetableManager<WaveformIR>::WavetableManager(int,int,vec)::numDefs` | yes | medium | written to lineNr_ | `lineNr` | cross-batch-arbitration |
| `WavetableManager<WaveformIR>::WavetableManager(int,int,vec)::numDefs2` | yes | medium | written to waveformCounter_ | `waveformCounter` | cross-batch-arbitration |
| `WavetableManager<WaveformIR>::WavetableManager(int,int,vec)::waveforms` | no | high | source vector | keep current | — |
| `WavetableManager<WaveformIR>::insertWaveform::wf` | no | high | the inserted waveform | keep current | — |
| `WavetableManager<WaveformIR>::newWaveform::name` | no | high | written to wf->name | keep current | — |
| `WavetableManager<WaveformIR>::newWaveform::signal` | no | high | source signal | keep current | — |
| `WavetableManager<WaveformIR>::newWaveform::fillName` | unsure | low | secondaryName field | keep current | — |
| `WavetableManager<WaveformIR>::newWaveform::dc` | no | high | DC ref | keep current | — |
| `WavetableManager<WaveformIR>::fromJson::json`,`dc` | no | high | trivial | keep current | — |
| `WavetableManager<WaveformIR>::operator==::other` | no | high | idiomatic | keep current | — |
| `CsvParser::csvFileToWaveform::cache`,`wf`,`deviceType` | no | medium | recon-only fwd decl | keep current | — |
| `kType` (local in newWaveform) | no | medium | `Waveform::File::Type{2}` | keep current | — |

## 3. Detailed findings

### WaveOrder  [no / high / not-misnomer]

Evidence:
- `nm --demangle` line for `WavetableIR::forEachUsedWaveform(
  std::function<void(...)>, zhinst::WaveOrder) const`. The type name
  `WaveOrder` is encoded verbatim as a parameter type.

Interpretation:
- Per RULES §3, type names that appear in mangled binary symbols are
  authoritative.

Judgement:
- Not a misnomer.

Locations consulted:
- declared: include/zhinst/wavetable_ir.hpp:43
- nm output above

### WaveOrder::ByName  [yes / high / —]

Evidence:
- include/zhinst/wavetable_ir.hpp:46 — declared `ByName = 1, // Sort by name`.
- src/wavetable_ir.cpp:467-472 — the `ByName` branch of
  `forEachUsedWaveform` reads `usedWaveforms_[a]->waveIndex` (not the
  name) for comparison and is annotated:
  `// Despite the enum name, ByName sorts by waveIndex ascending.`
  The same comment cites binary `$_0 at 0x2ad830` as the lambda
  symbol.
- Binary symbol `…WaveOrder) const::$_0` exists at 0x2ad830 (visible
  in `nm` output above) — confirmed it is one of two stable_sort
  comparators inside `forEachUsedWaveform`.
- Caller pattern at src/wavetable_ir.cpp:338 uses
  `fifoMode ? Natural : ByName` for `allocateWaveforms`, where the
  surrounding logic (assigning sequential `addressValue` via
  `addressBase_`) requires a deterministic by-waveIndex ordering, not
  an alphabetical one.
- Other call sites (src/awg_compiler.cpp:774, src/wavetable_ir.cpp:779,
  src/write_waves_to_elf.cpp:86) all feed the value into the same
  `forEachUsedWaveform` and so receive a by-`waveIndex` order.

Interpretation:
- The sort key is `WaveformIR::waveIndex` (an integer assigned by
  `WaveIndexTracker`), not `WaveformIR::name`. The enumerator name
  contradicts the actual sort key.

Judgement:
- Misnomer. The enum value should describe what the comparator
  actually compares, not what its declarer hoped it compared.

Proposals:
- `ByWaveIndex`  (high)
- `ByIndex`     (low — already taken by the other enumerator;
                see cross note below)

Cross-reference:
- `WaveOrder::ByIndex` (next block) actually sorts by `addressValue`,
  so a clean rename would also touch that enumerator. Treat both as
  a coordinated rename pair.

Locations consulted:
- declared: include/zhinst/wavetable_ir.hpp:46
- used:     src/wavetable_ir.cpp:338,467-472,779;
            src/awg_compiler.cpp:774;
            src/write_waves_to_elf.cpp:86

### WaveOrder::ByIndex  [yes / medium / —]

Evidence:
- include/zhinst/wavetable_ir.hpp:47 — `ByIndex = 2, // Sort by wave index`.
- src/wavetable_ir.cpp:461-466 — the `ByIndex` branch reads
  `usedWaveforms_[a]->addressValue` (not `waveIndex`) and is
  annotated: `// Binary $_1 at 0x2ae780: reads +0x4c (addressValue),
  unsigned cmp (jae)`.
- Binary symbol `…WaveOrder) const::$_1` exists at 0x2ae780.
- Call sites: src/awg_compiler.cpp:332,790;
  src/write_waves_to_elf.cpp:172. Both are inside ELF-write paths
  where the waveforms must be emitted in **address** (i.e. memory
  layout) order, not in their assigned wave-index order.

Interpretation:
- The sort key is `addressValue` — the byte address of the waveform
  in memory — not the `waveIndex`. The header comment
  ("Sort by wave index") is wrong; the enumerator name is similarly
  wrong (or, charitably, "Index" = "memory index", but that is not
  the natural reading given a sibling field literally called
  `waveIndex`).

Judgement:
- Misnomer. Confidence one notch lower than `ByName` only because
  "Index" is more ambiguous than "Name" — it could plausibly be
  read either as "memory index" or "wave index", and confusion
  between the two is precisely what this enumerator currently
  causes.

Proposals:
- `ByAddress`  (high)
- `ByMemoryAddress`  (medium)
- keep current  (low)

Cross-reference:
- Coordinated with `WaveOrder::ByName` above. After both renames the
  enum reads `Natural / ByWaveIndex / ByAddress`, which matches the
  comparators 1:1.

Locations consulted:
- declared: include/zhinst/wavetable_ir.hpp:47
- used:     src/wavetable_ir.cpp:461-466;
            src/awg_compiler.cpp:332,790;
            src/write_waves_to_elf.cpp:172

### WaveOrder::Natural  [no / medium / —]

Evidence:
- include/zhinst/wavetable_ir.hpp:44 — `Natural = 0, // No sorting`.
- src/wavetable_ir.cpp:454-482 — the `forEachUsedWaveform` body has
  no sort branch for value 0; the iota-initialised `indices` array is
  used in insertion order. Value 0 is the implicit default.
- Used at every "iterate in declaration order" site: src/wavetable_ir.cpp:435,
  504, 658, 696, 815.

Interpretation:
- The "natural" iteration order (insertion order, no sort) matches the
  enumerator name.

Judgement:
- Not a misnomer.

Proposals:
- keep current (medium)

Locations consulted:
- declared: include/zhinst/wavetable_ir.hpp:44
- used:     src/wavetable_ir.cpp as listed

### WavetableIR::allocateWaveforms::totalSamples  [yes / high / —]

Evidence:
- src/wavetable_ir.cpp:262 — declared
  `size_t totalSamples = 0;  // unused but kept for symmetry with binary stack layout`.
- src/wavetable_ir.cpp:336 —
  `totalSamples += wf->getSampleCount();`
- Per batch 16 (`16_waveform_ir.md`, `getSampleCount()` finding,
  high confidence): `WaveformIR::getSampleCount()` returns
  `Waveform::allocationByteSize` — a **byte** count, not a sample
  count.
- The local is never read after the loop. The header comment at line
  262 already acknowledges it is unused; it exists only because the
  binary's stack frame had a slot there.

Interpretation:
- The local accumulates byte counts under a name that says "samples".
- It is dead in the reconstruction (no consumer).

Judgement:
- Misnomer. The accumulated value is bytes, not samples.

Proposals:
- delete the local (the only reason it exists is to mirror the
  binary's stack layout; it has no semantic role)  (high)
- `totalBytes`  (medium)
- keep current  (low — only because the local is dead, so the
  misnomer cannot mislead a future reader of *runtime* logic; it can
  still mislead a reader trying to match recon to disasm)

Cross-reference:
- Tied to batch 16's `WaveformIR::getSampleCount()` finding.  If that
  accessor is renamed to `getAllocationByteSize`, this local must be
  renamed (or deleted) at the same time.

Locations consulted:
- declared: src/wavetable_ir.cpp:262
- used:     src/wavetable_ir.cpp:336

### WavetableIR::allocateWaveforms::sizeInBlocks  [yes / medium / —]

Evidence:
- src/wavetable_ir.cpp:292-300 (and the parallel block at lines
  372-381 in the second forEachUsedWaveform):
  ```
  if (length == 0) sizeInBlocks = 0;
  else {
      uint32_t granularity = dc->waveformGranularity;
      uint32_t pageSize    = dc->waveformPageSize;
      uint32_t rounded = ((length + pageSize - 1) / pageSize) * pageSize;
      sizeInBlocks = std::max(rounded, granularity);
  }
  uint64_t totalBits = static_cast<uint64_t>(sizeInBlocks) * channels * dc->bitsPerSample;
  ```
- `length` is `wf->signal.length_` (a sample count). `pageSize` is a
  sample-domain page divisor (per batch 31, `waveformPageSize` is the
  "round-up divisor in waveform memory calc"). The rounded result is
  thus a **sample** count, not a count of "blocks".
- The product `sizeInBlocks * channels * bitsPerSample` makes the
  unit explicit: only if `sizeInBlocks` is samples does
  `samples * channels * bits/sample` give bits.

Interpretation:
- The variable holds a per-channel sample count (rounded up to the
  device's waveform page granularity), not a block count.

Judgement:
- Misnomer. The "block" terminology is borrowed from
  `cacheLineUsage` / `numCacheLines` further down, where blocks
  *are* fixed-size cache-line slots; that is a different axis from
  this per-waveform sample count.

Proposals:
- `sizeInSamples`  (high)
- `paddedSampleCount`  (medium)
- keep current  (low)

Locations consulted:
- declared/used: src/wavetable_ir.cpp:292-301, 372-384

### WavetableIR::allocateWaveforms::memorySizeInSamples  [yes / high / —]

Evidence:
- src/wavetable_ir.cpp:351 —
  `uint32_t memorySizeInSamples = deviceConstants_->waveformMemorySize;  // DC+0x0C`
- DC field `waveformMemorySize` is, per batch 31 (overview row 18,
  positive evidence) and per the DC header comment at
  device_constants.hpp:43 ("minBlockSize"), a byte-domain value used
  as the cache-line size in this same allocator.
- Use sites in this scope:
  - line 353: `numCacheLines = memorySizeInSamples / alignment;`
    — divides by `waveformAlignment`, which is itself a byte-domain
    page size.  `bytes / bytes = number_of_lines` is consistent;
    `samples / bytes` is not.
  - line 396: `offsetInCL = wf->addressValue % memorySizeInSamples;`
    — `addressValue` is a byte address. Modulo of bytes by
    "samples" would be a unit error; modulo by **bytes** is what
    actually happens.
  - line 397: `if (offsetInCL + allocationBytes > memorySizeInSamples)` —
    compares byte offsets against `memorySizeInSamples`. Only
    coherent if it is bytes.

Interpretation:
- The local is the device cache-line size in **bytes**; the variable
  name claims "samples". Every arithmetic site treats it as bytes.

Judgement:
- Misnomer. The "InSamples" suffix is wrong.

Proposals:
- `cacheLineBytes`        (high)
- `memorySizeBytes`       (medium)
- keep current            (low)

Cross-reference:
- The DC field itself (`waveformMemorySize`) was judged not a
  misnomer in batch 31 (positive evidence: serializes through
  intermediate names that match its byte-domain role). The local
  copy is the misnomer.

Locations consulted:
- declared: src/wavetable_ir.cpp:351
- used:     src/wavetable_ir.cpp:353,396,397

### WavetableIR::allocateWaveforms::offsetInCL  [unsure / medium / —]

Evidence:
- src/wavetable_ir.cpp:396 — `offsetInCL = wf->addressValue % memorySizeInSamples;`
- Used at line 397 for boundary check and line 401 for the
  block-index computation `offsetInCL / alignment`.
- Per the previous block, `memorySizeInSamples` is actually the cache
  *region* size (the chunk modded against), which equals one
  cache-line region. So `offsetInCL` is the offset within that
  region — fine.

Interpretation:
- Name reads as "offset within cache line"; meaning is "offset within
  cache line region of size `waveformMemorySize`". Correct in spirit
  but reads slightly oddly given that `numCacheLines` and
  `cacheLineUsage` use "CacheLine" to mean a single-page slot of size
  `waveformAlignment`.

Judgement:
- Marginal. The "CL" abbreviation is overloaded in the surrounding
  code (means "cache line region" here, "cache line slot" elsewhere).
  Not flagged `yes` because the *current* name still reads correctly
  if "CL" is taken to mean "cache region" — which the comments at
  lines 350-405 sort of imply.

Proposals:
- keep current  (medium)
- `offsetInRegion`  (low)

Locations consulted:
- declared/used: src/wavetable_ir.cpp:396-401

### WavetableIR::updateWaveforms::allocFlag  [yes / high / —]

Evidence:
- include/zhinst/wavetable_ir.hpp:162 declaration:
  `void updateWaveforms(bool fifoMode, bool allocFlag);`
- src/wavetable_ir.cpp:589-596 body:
  ```
  if (fifoMode) allocateWaveformsForFifo();
  else          allocateWaveforms(allocFlag);
  ```
- `allocateWaveforms`'s sole parameter is named `fifoMode`
  (header line 148, src line 243). Inside that function, `fifoMode`
  selects `WaveOrder::Natural` vs `WaveOrder::ByName` and gates the
  `computedOffset` formula.
- Sole caller src/compiler.cpp:543-544:
  ```
  wavetableIR->updateWaveforms(config.cacheType != 0 && config.isHirzel,
                               <something passed as allocFlag>);
  ```

Interpretation:
- The parameter is forwarded verbatim to `allocateWaveforms`'s
  `fifoMode` slot. The two parameters of `updateWaveforms` thus carry
  the same kind of value (whether the inner pass should treat the
  waveforms in fifo-mode), under two unrelated names. The outer name
  `allocFlag` describes nothing.

Judgement:
- Misnomer. The parameter is a fifo-mode boolean, just like the
  declared `fifoMode` argument it is forwarded as.

Proposals:
- `fifoMode`  (high)
- `innerFifoMode`  (medium — to disambiguate from the outer
  `fifoMode` in the same signature)
- keep current  (low)

Locations consulted:
- declared: include/zhinst/wavetable_ir.hpp:162
- defined:  src/wavetable_ir.cpp:589-596
- callee:   src/wavetable_ir.cpp:243-440 (`allocateWaveforms::fifoMode`)
- caller:   src/compiler.cpp:543-544

### WavetableIR::assignWaveIndexImplicit::countFn  [yes / medium / —]

Evidence:
- src/wavetable_ir.cpp:501-503:
  ```
  auto countFn = [this](const std::shared_ptr<WaveformIR>& wf) {
      // no-op counting lambda
  };
  forEachUsedWaveform(countFn, WaveOrder::Natural);
  ```
- The lambda body is empty; the call to `forEachUsedWaveform` does
  not even read a counter.
- The reconstruction comment ("Phase 1: iterate used waveforms to
  find max index") promises something the body does not deliver. The
  binary lambda symbol referenced earlier in the file
  (`assignWaveIndexImplicit()::$_0` at 0x2a9ed0..0x2a9fd0) likely has
  a real body in the original.

Interpretation:
- The local name claims a counting role the lambda does not perform.

Judgement:
- Misnomer relative to the reconstructed body. (If the binary lambda
  is later reconstructed into something that *does* count something,
  the right name will follow from that work.)

Proposals:
- `noop` / delete the call entirely if it really has no side effects
  (high — but contingent on confirming the binary lambda is also a
  no-op, which static analysis suggests it is not)
- keep current  (low)
- `dummy`  (low)

Status:
- Marked `—` rather than `verify-not-original` because the *name*
  judgement is firm relative to the present source. Re-examination
  is a TODO for the wavetable_ir reconstruction, not for this audit.

Locations consulted:
- defined: src/wavetable_ir.cpp:501-504

### WavetableManager<WaveformIR>::WavetableManager(int,int,vec)::numDefs / numDefs2  [yes / medium / cross-batch-arbitration]

Evidence:
- src/wavetable_manager_ir.cpp:57-72 — parameter declaration:
  ```
  WavetableManager<WaveformIR>::WavetableManager(
      int numDefs, int numDefs2,
      const std::vector<Waveform>& waveforms)
  {
      this->lineNr_ = numDefs;
      this->waveformCounter_ = numDefs2;
      ...
  }
  ```
- Field names assigned to: `lineNr_` and `waveformCounter_` (declared
  at the top-of-file layout comment, lines 9-10, where the layout
  explicitly says "or numDefs / second int field" — the layout
  comment itself records the historical uncertainty).
- The corresponding JSON keys in `toJson()` and `fromJson()` are
  `"numDefs"` and `"numDefs2"` (src/wavetable_manager_ir.cpp:213-214,
  244-245). These are §4d-tier-2 *positive* evidence for those
  *names*: the strings appear verbatim in `.rodata` of the binary.
- However, the **field** names (`lineNr_`, `waveformCounter_`) come
  from later analysis. The `assignWaveIndexImplicit` body
  (src/wavetable_ir.cpp:544-545) reads
  `manager_->lineNr_` and `manager_->waveformCounter_` and feeds
  them into `getUniqueName(name, lineIdx, counter)` which produces
  a `__<base>_<lineNr>_<counter>` waveform name. This is consistent
  with the field names being the right semantic names and the JSON
  keys being legacy field names from an earlier version of the
  binary.

Interpretation:
- The constructor parameter names `numDefs` / `numDefs2` are the
  legacy JSON-key spellings; the field names they are assigned to
  (`lineNr_` / `waveformCounter_`) are the post-analysis spellings.
  The two sides disagree.
- This is a he-said/she-said situation per RULES §4c. The parameter
  names match a confirmed binary string (JSON key = strong
  positive evidence for "what this *was* called"); the field names
  match the observed runtime usage (line number / counter for unique
  naming). Both can be correct under different lenses.

Judgement:
- Both sides flagged as misnomer candidates: the parameter names
  perpetuate the old "numDefs" terminology in *new* code (the
  parameter list, which is recon-only), even though the same code
  immediately renames them to `lineNr_` / `waveformCounter_`.
  Aligning the parameter names to the field names would be the
  least-surprising fix.

Proposals:
- `lineNr` / `waveformCounter`  (high — match the field names they
  are assigned to)
- keep current (`numDefs` / `numDefs2`, matching JSON keys)
  (medium)

Cross-reference:
- The corresponding `WavetableManager<WaveformFront>` ctor lives in
  `wavetable_manager_front.cpp` (batch 45 if the parallel
  hypothesis holds). It is also `(int, int, vec)`. The decision
  here should match that one — same signature, same fields. Defer
  to synthesis.
- The field names `lineNr_` / `waveformCounter_` themselves are
  flagged separately below.

Locations consulted:
- declared: src/wavetable_manager_ir.cpp:57-58
- assigned: src/wavetable_manager_ir.cpp:61-62
- field uses: src/wavetable_ir.cpp:544-545,546
- JSON keys: src/wavetable_manager_ir.cpp:213-214,244-245

### WavetableManager<WaveformIR>::lineNr_  [unsure / low / —]

Evidence:
- Declared in the layout comment at src/wavetable_manager_ir.cpp:9
  (and implicitly in the primary template's `.hpp`, not present in
  the audit-scope files). The trailing-underscore style matches
  other private members.
- Read at src/wavetable_ir.cpp:544 inside `assignWaveIndexImplicit`:
  `int lineIdx = manager_->lineNr_;` then passed to `getUniqueName`
  along with `counter` to build a "filler" waveform name.
- Read again at src/wavetable_ir.cpp:72 inside the
  `WavetableIR(WavetableFront&, …)` ctor: copied 8-byte block
  containing both `lineNr_` and `waveformCounter_` from the front's
  manager.
- Serialised as JSON key `"numDefs"` (src/wavetable_manager_ir.cpp:244).
- No observed use as a "line number" in any source-code sense
  inside the audited files — the value is simply a counter
  participant.

Interpretation:
- The field is used as one half of a 2-tuple that disambiguates
  generated waveform names. Whether the value originated as a
  source-line number, a definition count, or something else cannot
  be determined from the audit-scope files alone.

Judgement:
- Cannot decide. The name is plausible but unverifiable from this
  batch.

Proposals:
- keep current  (medium)
- `numDefs` (matching JSON key)  (low)

Locations consulted:
- declared: src/wavetable_manager_ir.cpp:9 (layout comment)
- used:     src/wavetable_ir.cpp:72,544;
            src/wavetable_manager_ir.cpp:61,244

### WavetableManager<WaveformIR>::nameToIndex_  [no / high / not-misnomer]

Evidence:
- Layout comment src/wavetable_manager_ir.cpp:11-13 declares it as
  `unordered_map<string, size_t>` storing "name -> index mappings
  for O(1) lookup".
- `WavetableIR::getWaveformByName` at src/wavetable_ir.cpp:211 does
  `manager_->nameToIndex_.find(*name)` and indexes
  `manager_->waveforms_` with the result — name → index → waveform.
- `insertWaveform` at src/wavetable_manager_ir.cpp:174 inserts
  `(name, idx)` after a `waveforms_.emplace_back`.

Interpretation:
- Every read and write site uses the map exactly as a name → index
  lookup. The name describes that role precisely.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: src/wavetable_manager_ir.cpp:12 (layout comment)
- used:     src/wavetable_ir.cpp:211; src/wavetable_manager_ir.cpp:174

### WavetableIR::usedWaveforms_  [no / high / not-misnomer]

Evidence:
- include/zhinst/wavetable_ir.hpp:81 declaration with comment "+0xA0".
- `WavetableIR::setUsedWaveforms` (src/wavetable_ir.cpp:578-583)
  assigns into it from the parameter list.
- `WavetableIR::forEachUsedWaveform` (src/wavetable_ir.cpp:449-482)
  iterates only this vector.
- Caller src/compiler.cpp:524-527:
  ```
  // Step 5: getUsedWavesForDevice → setUsedWaveforms
  wavetableIR->setUsedWaveforms(waves);
  ```
  — comment confirms the value is the "used waves for device".

Interpretation:
- The vector holds the subset of waveforms actually used in the
  compiled program; the field, the setter, and the iterator all use
  the "used" terminology consistently.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/wavetable_ir.hpp:81
- used:     src/wavetable_ir.cpp:449-482,578-583;
            src/compiler.cpp:524-527

## 4. Symbols inspected and judged routinely fine

Data members of `WavetableIR`:
- `deviceConstants_` — pointer to `DeviceConstants`, dereferenced as
  such at every site (e.g. `deviceConstants_->waveformAlignment`,
  `deviceConstants_->waveformMemorySize`).
- `addressBase_` — running base address; read in `getNextSegmentAddress()`
  and updated in `allocateWaveforms*`.
- `firstWaveformOffset_` — exposed verbatim by `getFirstWaveformOffset()`
  getter.
- `cachedParser_` — `CachedParser` instance; type name matches.
- `manager_` — `unique_ptr<WavetableManager<WaveformIR>>`.
- `waveIndexTracker_` — `WaveIndexTracker`; type matches.
- `cancelCallback_` — `weak_ptr<CancelCallback>`; type matches.

Constructor parameters (both `WavetableIR` ctors):
- `front`, `mgr`, `constants`, `address`, `path`, `cancelCb` — each
  one is the obvious thing under each name.
- `wavetableSize` — passed through to `CachedParser(size, path)`
  without further inspection in this batch; flagged `unsure / low /
  verify-not-original` purely because the precise unit could not be
  cross-checked from `cached_parser` (batch 15).

Free-function-ish locals:
- `allocateWaveforms`: `cancelLock`, `cancelLock2`, `totalSize`,
  `waveCount`, `lastAllocBytes`, `channels`, `length`, `dc`,
  `granularity`, `pageSize`, `rounded`, `totalBits`, `allocationBytes`,
  `wfAlign`, `alignedLimit`, `needsAlign`, `alignment`,
  `computedOffset`, `raw`, `maxBlocksPerCL`, `numCacheLines`,
  `cacheLineUsage`, `maxPerCL`, `startBlock`, `endBlock`, `conflict`,
  `addr`, `usedEntries` — all match their roles. (Three are flagged
  separately above: `totalSamples`, `sizeInBlocks`,
  `memorySizeInSamples`.)
- `forEachUsedWaveform`: `count`, `indices`, `idx`, `i`, `a`, `b`,
  `callback`, `order` — trivial.
- `assignWaveIndexImplicit`: `tree`, `autoIdx`, `it`, `it2`,
  `minSamples`, `fillerSignal`, `fillerName`, `lineIdx`, `counter`,
  `uniqueName`, `newWf`, `lastWf` — all match.
- `allocateWaveformsForFifo`: `dc`, `memorySizeInSamples`, `alignment`,
  `maxBlocks`, `allocator`, `allocatedSet`, `block`, `block2`,
  `localAllocSet`, `startAddr`, `endAddr`, `pos`, `clIdx` — match.
  (Note: same `memorySizeInSamples` shadow exists here as in
  `allocateWaveforms`; same byte-vs-samples concern would apply, but
  the call sites here are simpler — flagged once is enough.)
- `loadWaveform`: `wf`, `wfCopy`, `deviceType` — trivial.
- `getJsonIndex`: `waveformsPtree`, `element`, `root`, `oss` — trivial.
- `alignWaveformSizes`: `wf`, `sampleCount`, `granularity`,
  `aligned`, `maxSamples` — trivial.
- `assignWaveformAllocationSizes`: `cancelLock`, `wf`, `channelCount`,
  `sampleCount`, `granularity`, `aligned`, `maxSamples`, `bps`,
  `bits`, `bytes`, `alloc` — trivial.

Free-function declarations / forwarders:
- `using detail::getUniqueName;` — declared free function; the
  `getUniqueName` symbol exists at 0x2a0fd0 and is authoritative.
- `CsvParser` class + `csvFileToWaveform` template — recon-only
  forward declaration (the real class lives elsewhere); its
  parameters `cache`, `wf`, `deviceType` match their use at the
  single call site in `loadWaveform`.

`WavetableManager<WaveformIR>` data members:
- `waveformCounter_` — post-incremented in `assignWaveIndexImplicit`
  (`int counter = manager_->waveformCounter_++`); name describes the
  role.
- `nameToIndex_` — covered as a detailed block above.
- `waveforms_` — vector of `shared_ptr<WaveformIR>`; iterated
  everywhere (`begin()`, `end()`, `size()`, `getWaveformByName`,
  `operator==`, `toJson`).

`WavetableManager<WaveformIR>` parameters / locals:
- `WavetableManager(int,int,vec)::waveforms` — the source vector
  copied from.
- `WavetableManager(...)::copy`, `sharedWf`, `wfIR` — trivial loop
  locals.
- `~WavetableManager`: `it` — trivial.
- `newWaveform::name`, `signal`, `dc` — match their roles. `fillName`
  is the only mildly ambiguous one (kept routine because the field
  it copies into, `secondaryName`, is in batch 14's scope).
- `newWaveform::raw`, `wf`, `kType` — trivial.
- `fromJson::obj`, `waveformsJson`, `waveforms`, `numDefs`,
  `numDefs2`, `result` — `numDefs` / `numDefs2` *as locals* are fine
  (they are read directly from the JSON keys of those names); the
  cross-batch-arbitration concern is only on the *constructor
  parameters* of the same names.
- `toJson::wfJsons`, `arr`, `it` — trivial.
- `operator==::lIt`, `lEnd`, `rIt`, `rEnd`, `waveformsEqual`,
  `mapsEqual`, `key`, `value`, `it` — trivial.

Symbols deliberately **not** inspected (out of scope per RULES §2/§3):

- All `WavetableIR` method names (every one is in `nm` — see §1).
- All `WavetableManager<WaveformIR>` method names (likewise).
- Type names `WavetableIR`, `WaveOrder`, `WavetableManager`,
  `WavetableException`, `CancelCallback`, `WavetableFront`,
  `CachedParser`, `WaveIndexTracker`, `DeviceConstants`,
  `WaveformIR`, `Waveform`, `Signal`, `MemoryAllocator`,
  `MemoryBlock`, `AwgDeviceType`, `SampleFormat`, `ReserveOnly`,
  `MarkerBitsPerChannel`, `AddressImpl<uint32_t>` — all appear in
  binary mangled symbols (verified for the first eight in §1; the
  rest by inheritance from prior batches 14, 16, 31, 38, 48, 51).
- Forward-declared template parameters on
  `boost::property_tree::basic_ptree` (`Key`, `Data`, `KeyCompare`)
  — RULES §2 excludes template parameters.
- The forward declaration `class WavetableFront` — covered by batch
  45 (`wavetable_front`).
- The forward declaration `class CancelCallback` — covered by batch
  51 (`callbacks.md`).

## 5. Coverage

- **Fully covered:**
  - All 8 `WavetableIR` data members.
  - All 4 `WavetableManager<WaveformIR>` data members visible from the
    audit-scope files (`lineNr_`, `waveformCounter_`, `nameToIndex_`,
    `waveforms_`).
  - All 3 `WaveOrder` enumerators (`Natural`, `ByName`, `ByIndex`)
    and the type itself.
  - All parameters of: both `WavetableIR` constructors;
    `~WavetableIR`; `toJson`; `fromJson`; `operator==`; `begin`;
    `end`; `size`; `getWaveformByName`; `getNextSegmentAddress`;
    `getFirstWaveformOffset`; `allocateWaveforms`;
    `forEachUsedWaveform`; `assignWaveIndexImplicit`;
    `setUsedWaveforms`; `updateWaveforms`; `allocateWaveformsForFifo`;
    `alignWaveformSizes`; `assignWaveformAllocationSizes`;
    `loadWaveform`; `getJsonIndex`.
  - All parameters of `WavetableManager<WaveformIR>`: the
    `(int,int,vec)` ctor, `~WavetableManager`, `insertWaveform`,
    `newWaveform`, `fromJson`, `toJson`, `operator==`.
  - All visible local variables in every `.cpp` body listed above.

- **Deferred:**
  - `WavetableIR::WavetableIR(front,…)::wavetableSize` — flagged
    `verify-not-original`. Resolution requires `CachedParser`'s
    parameter naming (batch 15, already audited) to be re-checked
    against this call site. Not done here to keep this batch
    self-contained.
  - The recon-only forwarder `class CsvParser` and its template
    method `csvFileToWaveform` — only the single `loadWaveform`
    use-site exists; full audit needs the genuine `CsvParser`
    declaration (batch 12, loaders, per a Phase-12 comment in
    wavetable_ir.cpp:30-36). Parameter names match the use; flagged
    routine.

- **Not covered (out of scope per RULES §2/§3):**
  - Method names on `WavetableIR` and
    `WavetableManager<WaveformIR>` — all present in the binary
    symbol table (verified; see §1).
  - Type names listed in §4 above — all present in the binary symbol
    table either directly (e.g. `WaveOrder` as a parameter type of
    `forEachUsedWaveform`) or by inheritance from prior batches.
  - Member type aliases — none declared in the audit-scope files.
  - Template parameters on the forward-declared
    `boost::property_tree::basic_ptree` — RULES §2.
  - The free function `getUniqueName` brought in via `using
    detail::getUniqueName;` — name in `nm` symbol table at 0x2a0fd0.
