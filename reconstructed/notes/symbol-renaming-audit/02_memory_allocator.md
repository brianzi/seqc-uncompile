# Batch 02 — memory_allocator

## 1. Files considered

- `reconstructed/include/zhinst/memory_allocator.hpp`
- `reconstructed/src/memory_allocator.cpp`

Cross-referenced (read-only):
- `reconstructed/include/zhinst/device_constants.hpp`
- `reconstructed/src/wavetable_ir.cpp` (only consumer of the public API)
- `reconstructed/notes/symbol-renaming-audit/archive/36_cache.md`
  (cross-batch context for `unusedCacheLine` and `numSamples`/byte-vs-sample
  unit question)

`nm --demangle _seqc_compiler.so` symbols verified:
- `zhinst::MemoryAllocator` — type **excluded**
- `zhinst::MemoryBlock` — type **excluded**
- `zhinst::MemoryAllocator::~MemoryAllocator()` — method **excluded**
- `zhinst::MemoryAllocator::allocateCLAligned(unsigned int)` — method
  name appears inside the lambda mangling, **excluded**
- `zhinst::MemoryAllocator::allocateReloadingCL(unsigned int, std::set<unsigned long, ...>)`
  — method name appears inside the lambda mangling, **excluded**
- `zhinst::MemoryAllocator::allocateFirstSuitableFreeBlock<...>` —
  method name appears in three template instantiation symbols,
  **excluded**

No static data members appear; the constructor and the inline
accessors `hasFreeBlocks`/`lastFreeBlock`/`maxFreeBlockStart` have no
standalone symbol (constructor is inlined per the header comment;
accessors are inline in the header). They are consequently **in scope**
for renaming as new code, but their names are routine and there is no
he-said/she-said with the binary.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `MemoryBlock::start` | no | medium | matches binary return ABI | keep current | not-misnomer |
| `MemoryBlock::end` | no | medium | matches binary return ABI | keep current | not-misnomer |
| `MemoryBlock::flags` | unsure | low | only 2 bits ever set | `flags`, `validityFlags` | — |
| `MemoryAllocator::deviceConstants_` | no | high | type-suffixed, faithful | keep current | not-misnomer |
| `MemoryAllocator::startOffset_` | no | medium | matches ctor arg | keep current | — |
| `MemoryAllocator::lastAllocEnd_` | yes | medium | never updated post-init | `tailRegionEnd_`, `memoryEndAddr_` | — |
| `MemoryAllocator::memorySizeInSamples_` | unsure | low | unit assumption convergent | keep current, `memorySize_` | cross-batch-arbitration |
| `MemoryAllocator::cacheLineSize_` | no | medium | role matches name | keep current | — |
| `MemoryAllocator::maxBlocksPerCL_` | yes | medium | semantics inverted | `clsPerAllocation_`, `maxCLsPerBlock_` | — |
| `MemoryAllocator::pad_1C_` | no | high | trailing struct padding | keep current | — |
| `MemoryAllocator::cacheLineUsage_` | no | medium | per-CL ownership map | keep current | — |
| `MemoryAllocator::numCacheLines_` | yes | medium | decremented as remaining count | `numFreeCacheLines_`, `remainingCLs_` | — |
| `MemoryAllocator::pad_3C_` | no | high | trailing struct padding | keep current | — |
| `MemoryAllocator::freeBlocks_` | no | medium | matches algorithm | keep current | — |
| `MemoryAllocator::MemoryAllocator::dc` | no | medium | conventional short name | keep current, `deviceConstants` | — |
| `MemoryAllocator::MemoryAllocator::startOffset` | no | medium | flows to startOffset_ | keep current | — |
| `MemoryAllocator::allocateCLAligned::size` | unsure | low | bytes-vs-samples convergent | keep current | cross-batch-arbitration |
| `MemoryAllocator::allocateReloadingCL::size` | unsure | low | bytes-vs-samples convergent | keep current | cross-batch-arbitration |
| `MemoryAllocator::allocateReloadingCL::usedAddrs` | yes | medium | values are CL indices | `usedCLIndices`, `usedCacheLines` | — |
| `MemoryAllocator::allocateFirstSuitableFreeBlock::pred` | no | medium | template predicate | keep current | — |
| lambda params `blockStart`/`blockEnd` (×3 lambdas) | no | medium | match deque entry fields | keep current | — |

## 3. Detailed findings

### MemoryBlock::flags  [unsure / low / —]

Evidence:
- `memory_allocator.hpp:32` — `uint32_t flags;     // bit 0 = valid, bit 8 = crossesCacheLine`
- `memory_allocator.cpp:144` — `return {aligned, aligned + size, 1};  // valid, no crossesCacheLine`
- `memory_allocator.cpp:192` — `uint32_t flags = 1 | (1 << 8);  // valid + crossesCacheLine`
- `memory_allocator.cpp:283,289,311,316` — when used as a free-list
  entry, the field is always set to literal `0`.
- `memory_allocator.cpp:18-28` (header comment) — describes the field
  as the low byte being a validity indicator and bit 8 being a
  cross-CL marker; this matches the binary return convention
  `dl = flags`.

Interpretation:
- The field carries a tiny, fixed bit-set (just two bits ever) and is
  used both as the "is this allocation valid?" return signal and as
  a property bit on the resulting allocation.

Judgement:
- The name `flags` is generic but accurate; a more specific name would
  arguably be more readable, but no observation contradicts the
  current name.

Proposals:
- keep current  (medium)
- `validityFlags`  (low) — somewhat narrower; partly inaccurate since
  bit 8 is a property, not a validity bit.

Locations consulted:
- declared: `include/zhinst/memory_allocator.hpp:32`
- used: `src/memory_allocator.cpp:115-194,272-323`

### MemoryAllocator::deviceConstants_  [no / high / not-misnomer]

Evidence:
- `memory_allocator.hpp:95` — `const DeviceConstants*  deviceConstants_;`
- `memory_allocator.cpp:48,52-54,116,119,137,154,157,216` — every read
  accesses fields of `DeviceConstants` (`waveformMemorySize`,
  `waveformAlignment`, `cachePageCount`, `waveformElfAlignment`,
  `maxBlocks`).
- `nm` confirms `zhinst::DeviceConstants` is the binary's type name.

Interpretation:
- The pointer is exclusively used to read `DeviceConstants` fields; the
  type-suffixed convention is consistent with the same field name in
  every other class (`Compiler`, `WavetableIR`, `WavetableFront`,
  `AwgCompiler`).

Judgement:
- Name accurately describes the field's role.

Proposals:
- keep current  (high)

Locations consulted:
- declared: `include/zhinst/memory_allocator.hpp:95`
- used: `src/memory_allocator.cpp:48-66,116-194,210-247`

### MemoryAllocator::lastAllocEnd_  [yes / medium / —]

Evidence:
- `memory_allocator.hpp:48,97` — declared with comment
  `0xFFFFFFFF sentinel initially`.
- `memory_allocator.cpp:51` — initialized to `0xFFFFFFFFu` in ctor and
  not assigned anywhere else in the file.
- `memory_allocator.cpp:301-321` — the only use site in the
  reconstruction is as `tailEnd`, the upper bound of a "tail region"
  searched after the deque is exhausted: `pred(tailStart, tailEnd)`,
  with `tailStart = freeBlocks_.empty() ? startOffset_ : freeBlocks_.back().end`.
  Comment at line 297-300 explicitly says it is "the end bound", and
  cites GDB verification.
- No code under `reconstructed/src/` writes to this field after the
  ctor.

Interpretation:
- The current name implies the field tracks the end of the most
  recently performed allocation (a moving high-water mark). The actual
  observed role is a static upper bound for the searchable memory
  region, initialised to a sentinel that is presumably overwritten
  somewhere not yet reconstructed (or is the genuine end-of-memory
  address used as-is when the underlying `uint32_t` view of memory is
  end-exclusive at `0xFFFFFFFF`).
- Either way, the observed behaviour does not match the words "last
  alloc end".

Judgement:
- Misnomer: the field is not updated on allocation and is used as the
  region's upper bound, not as an allocation-tracking pointer.

Proposals:
- `tailRegionEnd_`  (medium) — describes role at the only use site.
- `memoryEndAddr_`  (low) — if the sentinel is in fact the end address.
- `regionEnd_`      (low)

Locations consulted:
- declared: `include/zhinst/memory_allocator.hpp:97`
- used: `src/memory_allocator.cpp:51,301-306`

### MemoryAllocator::memorySizeInSamples_  [unsure / low / cross-batch-arbitration]

Evidence:
- `memory_allocator.hpp:49,98` — comment `DC->waveformMemorySize (+0x0C)`.
- `memory_allocator.cpp:52` — initialized from
  `dc->waveformMemorySize` (DeviceConstants +0x0C, documented at
  `device_constants.hpp:148` as "minBlockSize / cacheSize" with values
  0x20000–0x100000).
- `memory_allocator.cpp:128,169,174-175,232` — used as the modulus
  divisor `(addr % memorySizeInSamples_)` to wrap absolute addresses
  before indexing `cacheLineUsage_`. Pairs with `cacheLineSize_` to
  produce a slot index.
- Cross-batch context: batch 36 (`archive/36_cache.md`) judged the
  analogous `numSamples` parameters in `Cache` to be **byte counts**.
  `DeviceConstants::waveformMemorySize` is also used as `cacheSize`
  in the cache code path.

Interpretation:
- Same source field, different name suffix. Whether the value is
  measured in "samples" or "bytes" is unresolved within this batch:
  the only operations on it (`%`, `/`) do not constrain units.

Judgement:
- Cannot decide from this batch alone whether the `InSamples` suffix
  is correct or convergent with batch 36's "bytes" reading.

Proposals:
- keep current  (low)
- `memorySize_`  (low) — drop the unit suffix until arbitrated.

Cross-reference:
- batch 36 `Cache::numSamples` parameter and any `Cache` field sourced
  from `DeviceConstants::waveformMemorySize`.

Locations consulted:
- declared: `include/zhinst/memory_allocator.hpp:98`
- used: `src/memory_allocator.cpp:52,65,128,169,174-175,232`

### MemoryAllocator::maxBlocksPerCL_  [yes / medium / —]

Evidence:
- `memory_allocator.hpp:51,100` — comment "DC->cachePageCount or
  maxBlocks"; declared `uint32_t maxBlocksPerCL_`.
- `memory_allocator.cpp:54` — initialized from `dc->cachePageCount`
  (DeviceConstants +0x18, values 4 / 104 / 0x2000 / 0x6000).
- `memory_allocator.cpp:170` — `uint32_t numSlots = maxBlocksPerCL_ * cacheLineSize_;`
  immediately clamped `if (numSlots > size) numSlots = size;`. Used
  as a *byte/sample* extent of the allocation.
- `memory_allocator.cpp:227` — `uint32_t numCLsNeeded = maxBlocksPerCL_;`
  followed by a loop incrementing `addr += align` (one CL per
  iteration). The local rename `numCLsNeeded` directly contradicts
  the field's own name.
- `wavetable_ir.cpp:352` — same DC field is named locally as
  `maxBlocksPerCL` (matching the field) but is then used in
  `allocateWaveformsForFifo` to compute extents the same way.
- `device_constants.hpp:153` — DC field is `cachePageCount` ("cache
  pages (cacheType=0)").

Interpretation:
- The field's value is a count of *cache pages / cache lines per
  allocation* (line 227's loop iterates that many times, advancing
  one CL per step). The current name reads as the inverse ("blocks
  per CL"), and one of the field's own use sites locally renames it
  to `numCLsNeeded` to make the algorithm legible.

Judgement:
- Misnomer: the noun and the qualifier are inverted relative to the
  observed semantics.

Proposals:
- `clsPerAllocation_`   (medium)
- `maxCLsPerBlock_`     (medium) — same idea, dual phrasing.
- `cachePageCount_`     (medium) — mirrors the `DeviceConstants`
  source field name.
- keep current          (low)

Locations consulted:
- declared: `include/zhinst/memory_allocator.hpp:100`
- used: `src/memory_allocator.cpp:54,170,227-231`

### MemoryAllocator::numCacheLines_  [yes / medium / —]

Evidence:
- `memory_allocator.hpp:54,103` — comment
  `numCacheLines_  memorySizeInSamples / cacheLineSize`.
- `memory_allocator.cpp:65` — initialized in ctor as
  `numCacheLines_ = memorySizeInSamples_ / cacheLineSize_;`.
- `memory_allocator.cpp:186` — `numCacheLines_ -= (endSlot - startSlot);`
  — the field is **decremented** every time CL slots are claimed in
  Phase 2 of `allocateCLAligned`.
- `memory_allocator.cpp:174-175` — the *constant* "total number of
  cache lines" computed for the bounds check is recomputed as
  `memorySizeInSamples_ / cacheLineSize_`, not read from
  `numCacheLines_`. So the field is *not* used as the total.

Interpretation:
- The name `numCacheLines_` reads as a constant total (number of CLs
  in memory), but the field is mutated as allocations are made. The
  total is recomputed inline where needed. The field therefore tracks
  a *running count* — most consistent with "remaining free cache
  lines".

Judgement:
- Misnomer: name suggests an immutable total, but the field is a
  decrementing free-CL counter.

Proposals:
- `numFreeCacheLines_`  (medium)
- `remainingCLs_`       (medium)
- `freeCLCount_`        (low)
- keep current          (low)

Locations consulted:
- declared: `include/zhinst/memory_allocator.hpp:103`
- used: `src/memory_allocator.cpp:57,65,186`

### MemoryAllocator::cacheLineUsage_  [no / medium / —]

Evidence:
- `memory_allocator.hpp:53,102` — comment "per-CL owner; 0xFFFFFFFF=free".
- `memory_allocator.cpp:66` — initialized to a vector of
  `numCacheLines_` slots, all `0xFFFFFFFF`.
- `memory_allocator.cpp:131,177,183` — accessed/updated as a CL→owner
  map (slot index = `(addr % memorySizeInSamples_) / cacheLineSize_`,
  value = the cache-line base address that owns that slot, or the
  free sentinel).

Interpretation:
- The container stores per-cache-line ownership state, with the
  free-sentinel `0xFFFFFFFF`. Direct match to the name.

Judgement:
- Name accurately describes role.

Proposals:
- keep current  (medium)
- `clOwner_`    (low) — slightly more precise but less obvious.

Locations consulted:
- declared: `include/zhinst/memory_allocator.hpp:102`
- used: `src/memory_allocator.cpp:66,128-131,177,183`

### MemoryAllocator::allocateCLAligned::size  [unsure / low / cross-batch-arbitration]

Evidence:
- `memory_allocator.hpp:69` — `MemoryBlock allocateCLAligned(unsigned int size);`
- `wavetable_ir.cpp:633,675` — every call site passes
  `wf->allocationByteSize` (declared `int allocationByteSize` at
  `waveform.hpp:111`, JSON-serialized as `"allocationSize"` per
  `waveform.cpp:91`).
- `memory_allocator.cpp:123,140,144,156,162,193` — used as a length
  to compare against and add to the `aligned` address. Same arithmetic
  domain as `cacheLineSize_` (= `waveformAlignment`, e.g. 4096 on HDAWG).
- Cross-batch context: batch 36 judged analogous `numSamples`
  parameters in `Cache` to be byte counts; the suffix on
  `allocationByteSize` agrees with that, but the rest of the allocator
  arithmetic uses sample-count-shaped values.

Interpretation:
- The unit (bytes vs samples) is convergent across the cache and
  allocator code paths and was judged unresolved in batch 36. The
  bare name `size` does not assert a unit, so it is not strictly
  misleading on its own.

Judgement:
- Cannot decide from this batch; defer to the cross-batch
  arbitration kicked off by batch 36.

Proposals:
- keep current  (medium)

Cross-reference:
- batch 36 `Cache::numSamples` family.

Locations consulted:
- declared: `include/zhinst/memory_allocator.hpp:69`
- used: `src/memory_allocator.cpp:96-194`; called from
  `src/wavetable_ir.cpp:633,675`.

### MemoryAllocator::allocateReloadingCL::size  [unsure / low / cross-batch-arbitration]

Evidence:
- `memory_allocator.hpp:75` — `unsigned int size`.
- `wavetable_ir.cpp:687` — call site passes
  `wf->allocationByteSize, localAllocSet`.
- `memory_allocator.cpp:220,241` — used as a length that must fit in
  `blockEnd - aligned`.

Interpretation:
- Same unit ambiguity as `allocateCLAligned::size` above.

Judgement:
- Same as above; defer.

Proposals:
- keep current  (medium)

Cross-reference:
- batch 36 `Cache::numSamples` family.

Locations consulted:
- declared: `include/zhinst/memory_allocator.hpp:75`
- used: `src/memory_allocator.cpp:210-247`; called from
  `src/wavetable_ir.cpp:687`.

### MemoryAllocator::allocateReloadingCL::usedAddrs  [yes / medium / —]

Evidence:
- `memory_allocator.hpp:76` — `std::set<unsigned long> const& usedAddrs`.
- `memory_allocator.cpp:212` — declared with parameter comment
  `// CL indices of existing waveforms`.
- `memory_allocator.cpp:232-237` — the value compared against the set
  is computed as `key = (addr % memorySizeInSamples_) / cacheLineSize_`,
  i.e. a *cache-line index* derived from the address, not an address
  itself. The set is keyed by CL index.

Interpretation:
- The parameter holds CL indices, not addresses. The author's own
  comment at line 212 contradicts the parameter's name. The lookup
  arithmetic confirms the comment.

Judgement:
- Misnomer: values are CL indices, not addresses.

Proposals:
- `usedCLIndices`     (medium)
- `usedCacheLines`    (medium)
- `usedCLSet`         (low)
- keep current        (low)

Locations consulted:
- declared: `include/zhinst/memory_allocator.hpp:76`
- used: `src/memory_allocator.cpp:212,232-237`; called from
  `src/wavetable_ir.cpp:687`.

### Cross-batch-context check: `unusedCacheLine` (batch 36)

Confirmed the hypothesis recorded in batch 36
(`archive/36_cache.md:213-215`): the literal `0xFFFFFFFFu` written into
`cacheLineUsage_` on construction (line 66) and tested at line 131
(`cacheLineUsage_[slot] != clBase` for occupied; rejection of free
slots is the inversion of "occupied != free sentinel"), as well as
lines 177 (`cacheLineUsage_[s] != 0xFFFFFFFF`) and the dtor's untouched
sentinel, is exactly the "unallocated entry" sentinel for
`MemoryAllocator`. If the existing constant `zhinst::unusedCacheLine`
is to be reused, replacing all four literal `0xFFFFFFFFu` occurrences
in `memory_allocator.cpp` would be an action item for synthesis (the
current code uses bare literals).

This is a positive cross-batch confirmation, not a finding on a symbol
in this batch — recorded here for synthesis.

## 4. Symbols inspected and judged routinely fine

- `MemoryBlock::start`, `MemoryBlock::end` — match the binary return
  convention `rax = start | (end << 32)` (header comment lines 25-28)
  and are used consistently as a half-open `[start, end)` range.
- `MemoryAllocator::startOffset_` — initialized verbatim from the
  ctor's `startOffset` parameter; only use site at line 303 reads it
  as the initial `tailStart` of the searchable region.
- `MemoryAllocator::pad_1C_`, `MemoryAllocator::pad_3C_` — explicit
  alignment-padding fields between `uint32_t` and `vector`/`deque`,
  faithful to the documented +0x1C / +0x3C gaps.
- `MemoryAllocator::cacheLineSize_` — initialized from
  `waveformAlignment` (DC+0x14, values 16/48/4096); used as the
  cache-line stride in every slot-index computation. Name matches role.
- `MemoryAllocator::freeBlocks_` — `std::deque<MemoryBlock>` of free
  ranges, iterated, split, inserted as the algorithm name suggests.
- Constructor params `dc`, `startOffset` — short and match field
  initializations.
- Lambda params `blockStart`, `blockEnd` — receive `MemoryBlock::start`
  and `MemoryBlock::end` of the candidate free block; names mirror the
  struct field names.
- Template param `Pred` (out of scope per RULES §2) and runtime
  parameter `pred` — conventional names for a callable predicate.
- Inline accessors `hasFreeBlocks`, `lastFreeBlock`, `maxFreeBlockStart`
  and their local `m`, `b`: trivially correct.

## 5. Coverage

- **Fully covered:** every named symbol declared in
  `memory_allocator.hpp`/`memory_allocator.cpp` — fields, parameters
  of the constructor, three public methods, the template method, and
  the local lambda parameters. Cross-checked against `nm` for binary
  exclusions and against the only consumer (`wavetable_ir.cpp`) for
  parameter usage.
- **Deferred:** unit arbitration on
  `MemoryAllocator::memorySizeInSamples_`,
  `MemoryAllocator::allocateCLAligned::size`, and
  `MemoryAllocator::allocateReloadingCL::size` — convergent with
  batch 36; flagged `cross-batch-arbitration`.
- **Not covered (out of scope per RULES §2/§3):**
  - Type names `MemoryAllocator`, `MemoryBlock` — present in `nm`.
  - Method names `~MemoryAllocator`, `allocateCLAligned`,
    `allocateReloadingCL`, `allocateFirstSuitableFreeBlock` — present
    in `nm` (the last as the qualifier of the three template
    instantiation symbols).
  - Template parameter `Pred`.
  - Standard-library `std::deque`/`std::vector`/`std::set` and their
    methods.
