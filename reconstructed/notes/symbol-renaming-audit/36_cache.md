# Batch 36 — cache

## 1. Files considered

- `reconstructed/include/zhinst/cache.hpp`
- `reconstructed/src/cache.cpp`

Use-site survey across `reconstructed/src/**.cpp` and
`reconstructed/include/zhinst/**.hpp` (esp. `prefetch.cpp`,
`prefetch_splitplay.cpp`, `prefetch_placesingle.cpp`,
`prefetch_prepare.cpp`, `prefetch_helpers.cpp`).

Binary symbol table consulted via
`nm --demangle _seqc_compiler.so | grep …`. Confirmed exported
(authoritative under §3, **excluded** from rename):

- Types: `zhinst::Cache`, `zhinst::Cache::Pointer`,
  `zhinst::Cache::PointerState`, `zhinst::Cache::CacheType`,
  `zhinst::CacheException`.
- All Cache/CacheException methods listed in `cache.hpp` (verified
  in `nm` output).
- Constants: `zhinst::unusedCacheLine` (at 0x95deac, demangled
  symbol present), `zhinst::(anonymous namespace)::kCacheFormat`
  (at 0x95e8f8).

Tier-2 RTTI/log strings located via `strings _seqc_compiler.so`:
`"playing"`, `"last played"`, `"cache memory full"`,
`"Cache Exception"`, and the literal full mangled signature
`"Cache::Pointer::Ptr zhinst::Cache::getBestPosition(MemSize32, const std::unordered_map<std::string, bool> &, bool)"`.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `CacheException::message_` | no | medium | matches usage and what() | keep current (medium) | — |
| `CacheException::CacheException::msg` | no | high | direct ctor param of message | keep current (high) | — |
| `Cache::size_` | no | high | total cache size, divisor everywhere | keep current (high) | — |
| `Cache::pageSize_` | no | medium | initialized from devConst.sampleLength | keep current (medium) | not-misnomer |
| `Cache::appendMode_` | unsure | low | initialized from `config.isHirzel` | keep current, `isHirzel_` | — |
| `Cache::pointers_` | no | high | `vector<shared_ptr<Pointer>>` field | keep current (high) | — |
| `Cache::PointerState::Ready` | no | high | matches no string, but state 0 of fresh ptr | keep current (high) | — |
| `Cache::PointerState::LastPlayed` | no | high | `"last played"` in binary strings | keep current (high) | not-misnomer |
| `Cache::PointerState::Playing` | no | high | `"playing"` in binary strings | keep current (high) | not-misnomer |
| `Cache::PointerState::Free` | no | medium | matches free/idle slot semantic | keep current (medium) | — |
| `Cache::CacheType::Normal` | no | medium | non-aligned default branch | keep current (medium) | — |
| `Cache::CacheType::Aligned` | no | medium | doubles pageSize for alignment | keep current (medium) | — |
| `Cache::Pointer::position_` | no | high | start-of-allocation offset | keep current (high) | — |
| `Cache::Pointer::size_` | no | high | length of allocation | keep current (high) | — |
| `Cache::Pointer::hash_` | unsure | low | xor-mix of position bounds | keep current, `addrMask_`, `wrapMask_` | — |
| `Cache::Pointer::numRepeats_` | no | medium | counts split chunks at use sites | keep current (medium) | not-misnomer |
| `Cache::Pointer::waveform_` | no | high | `shared_ptr<WaveformIR>` payload | keep current (high) | — |
| `Cache::Pointer::state_` | no | high | holds `PointerState` enum | keep current (high) | — |
| `Cache::Cache::size` (ctor) | no | high | initializes `size_` | keep current (high) | — |
| `Cache::Cache::pageSize` (ctor) | no | high | initializes `pageSize_` | keep current (high) | — |
| `Cache::Cache::appendMode` (ctor) | unsure | low | passed `config.isHirzel` | keep current, `isHirzel` | — |
| `Cache::allocate(5-arg)::pageSize` | yes | medium | passed `Prefetch::maxBranches_` | `chunkCount`, keep current | cross-batch-arbitration |
| `Cache::allocate(5-arg)::numSamples` | yes | medium | bytes at call sites, not samples | `numBytes`, keep current | — |
| `Cache::allocate(5-arg)::nameMap` | no | high | name→bool waveforms-to-evict map | keep current (high) | — |
| `Cache::allocate(5-arg)::waveform` | no | high | `shared_ptr<WaveformIR>` | keep current (high) | — |
| `Cache::allocate(5-arg)::split` | no | medium | branches splitting/double-buffer logic | keep current (medium) | — |
| `Cache::allocate(5-arg)::freeMemory` (local) | no | high | computed from size_ minus used | keep current (high) | — |
| `Cache::allocate(5-arg)::numAllocs` (local) | no | medium | chunks-to-allocate count | keep current (medium) | — |
| `Cache::allocate(5-arg)::altAllocs` (local) | unsure | low | `numSamples / (size_/2)` | keep current, `minAllocs` | — |
| `Cache::allocate(5-arg)::halfSize` (local) | no | high | `size_ >> 1` | keep current (high) | — |
| `Cache::allocate(5-arg)::halfSz` (local) | no | medium | `ptr->size_ / 2` | keep current (medium) | — |
| `Cache::allocate(CacheType)::cacheType` | no | high | enum-typed alignment selector | keep current (high) | — |
| `Cache::allocate(CacheType)::pageSize` (local) | no | high | shadow of `pageSize_` then doubled | keep current (high) | — |
| `Cache::allocate(CacheType)::alignedSize` (local) | no | high | rounded-up `numSamples` | keep current (high) | — |
| `Cache::allocate(CacheType)::numSamples` | yes | medium | bytes at call sites | `numBytes`, keep current | — |
| `Cache::getBestPosition::numSamples` | yes | medium | bytes (forwarded from allocate) | `numBytes`, keep current | — |
| `Cache::getBestPosition::nameMap` | no | high | name→bool map; entries `=true` skipped | keep current (high) | — |
| `Cache::getBestPosition::appendMode` | yes | medium | recursion flag `false=fast,true=gap-scan` | `gapScan`, `searchAll` | — |
| `Cache::getBestPosition::bestPosition` (local) | unsure | low | uses `size_` as sentinel | keep current, `bestStart` | — |
| `Cache::play::ptr` | no | high | `shared_ptr<Pointer>` | keep current (high) | — |
| `Cache::play::state` | yes | medium | passed `nodeState.counter()` casted | `marker`, `freeFlag`, keep current | cross-batch-arbitration |
| `Cache::Pointer::str::stateNames` (local) | no | high | array of state strings | keep current (high) | — |
| `Cache::Pointer::str::stateLens` (local) | no | high | parallel length array | keep current (high) | — |
| `unusedCacheLine` | (out of scope) | — | exported in symbol table | — | — |

## 3. Detailed findings

### Cache::pageSize_  [no / medium / not-misnomer]

Evidence:
- `cache.hpp:159`  `int32_t pageSize_;  // +0x04`
- `cache.cpp:45`   `, pageSize_(pageSize)`  (ctor init)
- `cache.cpp:147` `uint32_t pageSize = pageSize_;`
- `cache.cpp:149` `pageSize = pageSize_ * 2;  // shl by 1` (alignment doubling for `CacheType::Aligned`)
- `prefetch.cpp:58-61` Cache ctor call: `std::make_shared<Cache>(devConst.waveformMemorySize, devConst.sampleLength, config.isHirzel)`.
- `device_constants.hpp:46` `// 0x10  4  uint32_t  sampleLength  Cache ctor arg2`
- `device_constants.hpp:149` `uint32_t sampleLength;  // +0x10  Cache ctor arg`

Interpretation:
- The field is initialized from `DeviceConstants::sampleLength`, a
  device-wide quantum used as the base alignment unit.
- It is consumed inside Cache as the divisor that rounds allocations
  up (`numSamples + pageSize - remainder`) and that multiplies for
  `CacheType::Aligned`.
- This matches what "page size" means in this code: the granularity
  of memory allocation in the cache.

Judgement:
- The name accurately reflects what is stored; no misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/cache.hpp:159
- used:     src/cache.cpp:45,58,78,147,149; src/prefetch.cpp:58-61

### Cache::appendMode_ / Cache::Cache::appendMode  [unsure / low / —]

Evidence:
- `cache.hpp:160`  `bool appendMode_;  // +0x08`
- `cache.cpp:46`   `, appendMode_(appendMode)`
- `cache.cpp:189-194` `if (appendMode_) { pointer->position_ = 0; pointer->size_ = numSamples; return pointer; }`
- `prefetch.cpp:61` `cache_ = std::make_shared<Cache>(…, config.isHirzel);`
- `awg_compiler_config.hpp:124` `bool appendMode() const { return isHirzel; }`
- `awg_compiler_config.hpp:101` comment: `appendMode : was actually isHirzel test at +0x18`
- `prefetch.hpp:310` `bool isHirzel_() const { return split_; }`

Interpretation:
- The field's value comes from `config.isHirzel`, a device-family
  flag.
- Inside `Cache::getBestPosition`, the field gates an "always place
  at offset 0" code path (no gap-scan) — i.e. behavior associated
  with Hirzel-class devices.
- The local field-internal use is consistent with "append/overwrite
  at position 0", but the *provenance* of the value is a device
  family marker, not a placement-mode toggle.
- The companion file `awg_compiler_config.hpp` already records that
  `appendMode` was a misread for `isHirzel`.

Judgement:
- Unsure: usage inside Cache fits "append mode" but the value's true
  provenance is `isHirzel`; cannot decide here whether the in-Cache
  semantic is genuinely an append-mode flag *or* a Hirzel test.

Proposals:
- keep current        (medium)
- `isHirzel_` / `isHirzel`  (low) — would track the originating field name

Cross-reference:
- `AWGCompilerConfig::isHirzel` and `Prefetch::split_` /
  `Prefetch::isHirzel_()` — see batch covering `awg_compiler_config`
  and batch 09 (prefetch).

Locations consulted:
- declared: include/zhinst/cache.hpp:160
- used:     src/cache.cpp:46,78,189,190; src/prefetch.cpp:61
- related:  include/zhinst/awg_compiler_config.hpp:101,124;
            include/zhinst/prefetch.hpp:303-311

### Cache::Pointer::numRepeats_  [no / medium / not-misnomer]

Evidence:
- `cache.hpp:107` `uint32_t numRepeats_;  // +0x0C`
- `cache.cpp:133` `ptr->numRepeats_ = numSamples / halfSz + 1;`
  (set when waveform must be chunked across several aligned pages)
- `prefetch_splitplay.cpp:138`
  `uint32_t numRepeats = pns.cachePtr->numRepeats_;`
- `prefetch_splitplay.cpp:148` `if (totalLength < numRepeats * halfSize)` —
  used as the count of half-page chunks.
- `prefetch_placesingle.cpp:248-251`
  `uint32_t numRepeatsI = cachePtrI->numRepeats_;`
  `if (numRepeatsI >= 2) emit wwvf` — wwvf is emitted when the
  waveform needs to wrap (more than one repeat).

Interpretation:
- Set in the double-buffering branch as `numSamples / halfPage + 1`,
  i.e. the number of half-page chunks the sequencer must replay.
- Read at use sites multiplied by `halfSize` to get total length, and
  thresholded against 2 to gate prefetch behavior.
- Both producer and consumers treat the value as a chunk/repeat
  count.

Judgement:
- The name correctly describes the value; no misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/cache.hpp:107
- used:     src/cache.cpp:133,185;
            src/prefetch_splitplay.cpp:137-156;
            src/prefetch_placesingle.cpp:248,325

### Cache::Pointer::hash_  [unsure / low / —]

Evidence:
- `cache.hpp:106` `uint32_t hash_;  // +0x08`
- `cache.cpp:132` `ptr->hash_ = ~(ptr->position_ ^ (ptr->position_ + halfSz));`
- `cache.cpp:184` `pointer->hash_ = 0;`
- No reads of `hash_` found anywhere under `reconstructed/src/**.cpp`
  outside cache.cpp itself (grep for `->hash_` in cache.hpp consumers
  returned no hits in this batch's survey scope).

Interpretation:
- Computed by xor-ing `position_` with `position_+halfSz` and
  bitwise-negating: this is an address-wrap mask, not a hash in the
  hash-table / digest sense (which would be a one-way function).
- Existing `cache.cpp:115` comment already characterizes it as
  "set up hash_ for address wrapping" — i.e. the author also
  hesitated about the name.

Judgement:
- Unsure: the value is plausibly a wrap-mask used by the sequencer's
  address logic, but no consumer site was located in this batch to
  prove the precise role; flagging as a low-confidence rename
  candidate rather than asserting a misnomer.

Proposals:
- keep current  (medium)
- `wrapMask_`   (low)
- `addrMask_`   (low)

Locations consulted:
- declared: include/zhinst/cache.hpp:106
- used:     src/cache.cpp:115,132,184

### Cache::allocate(5-arg)::pageSize  [yes / medium / cross-batch-arbitration]

Evidence:
- `cache.hpp:130-135` declaration: `… int pageSize, bool split);`
- `cache.cpp:100` `uint32_t freePages = freeMemory / pageSize;`
- `cache.cpp:104` `if (split || numSamples < freePages)`
- `prefetch.cpp:1677-1684` 5-arg call site:
  ```
  // Args: waveIR1, numSamplesForCache, nameMap_, maxBranches_, split_
  cachePtr->allocate(waveIR1, …, nameMap_, maxBranches_, split_);
  ```
- `prefetch.hpp:266` `int32_t maxBranches_;  // +0xB8 (init=1, used by countBranches/definePlaySize)`
- `prefetch_prepare.cpp:328` discovery comment:
  `>>> DISCOVERY: +0xB8 is maxBranches_, NOT pageSize_.`

Interpretation:
- Inside the 5-arg `Cache::allocate`, the parameter `pageSize` is
  used **only** as the divisor `freeMemory / pageSize → freePages`
  and then in the comparison `numSamples < freePages`. That is a
  page-size semantic locally.
- The single in-tree caller passes `Prefetch::maxBranches_` for this
  argument. `maxBranches_` is documented as a branch-depth counter
  (init=1), not a page size.
- One side is misnamed: either Cache's parameter is not really a
  page size (it's a branch/repeat count from prefetch), or
  `maxBranches_` is mis-typed/named in prefetch and is in fact a
  page count.

Judgement:
- Yes, names disagree across the call boundary; arbitration cannot
  be resolved within this batch and depends on whether the prefetch
  audit confirms `maxBranches_` semantics.

Proposals:
- keep current   (medium) — if `Prefetch::maxBranches_` is the
  misnamed side
- `chunkCount`   (low)
- `pagesPerChunk`(low)

Cross-reference:
- Counterpart `Prefetch::maxBranches_` — batch 09 (prefetch).

Locations consulted:
- declared: include/zhinst/cache.hpp:130-135
- used:     src/cache.cpp:86-100,104; src/prefetch.cpp:1677-1684
- related:  include/zhinst/prefetch.hpp:266;
            src/prefetch_prepare.cpp:321-328,411

### Cache::allocate(5-arg)::numSamples / Cache::allocate(CacheType)::numSamples / Cache::getBestPosition::numSamples  [yes / medium / —]

Evidence:
- `cache.hpp:132,139,145` parameter type `detail::AddressImpl<uint32_t>`
  named `numSamples`.
- `cache.cpp:151` `uint32_t remainder = numSamples % pageSize;` (used
  as a memory offset, not as a sample count).
- `prefetch.cpp:1650`
  `numSamplesForCache = playLen * channels * 2;`
- `prefetch.cpp:1672-1673`
  `numSamplesForCache = totalBits / 8 + (totalBits % 8 >= 1 ? 1 : 0);`
- The Cache's `size_` field comes from
  `DeviceConstants::waveformMemorySize` (a memory size in bytes/words,
  see `device_constants.hpp:44`).

Interpretation:
- Call sites compute the value in **bytes**: `playLen * channels * 2`
  (samples × channels × 2 bytes-per-sample) and `ceil(bits/8)`.
- Inside Cache the value is divided by and added to other sizes
  measured against `size_` (a memory size). Sample units would not
  type-check here.
- `numSamples` is therefore a unit-mismatched name for what is
  really a byte/memory count.

Judgement:
- Yes, the name implies a sample count but the value is bytes.

Proposals:
- `numBytes`     (medium)
- `byteSize`     (medium)
- keep current   (low) — only justifiable if the historical name
  is preserved via tier-1 evidence not located in this batch

Locations consulted:
- declared: include/zhinst/cache.hpp:132,139,145
- used:     src/cache.cpp:88-156,174-194; src/prefetch.cpp:1643-1684

### Cache::getBestPosition::appendMode  [yes / medium / —]

Evidence:
- `cache.hpp:147` `bool appendMode);  // 0x282cf0` (3rd parameter)
- `cache.cpp:177` parameter signature (parameter named `appendMode`).
- `cache.cpp:212` `if (!appendMode) { … fast path: append after last … if not enough room return getBestPosition(numSamples, nameMap, /*appendMode=*/true); }`
- `cache.cpp:230-265` the `else` branch (when `appendMode==true`)
  performs a full **gap-scan** across all live pointers.
- Distinct from the `Cache::appendMode_` member, which is checked
  separately at line 190.

Interpretation:
- When this parameter is `false`, the code does the append-at-end
  fast path; when `true`, it does an exhaustive gap-search. The
  meaning is therefore the *opposite* of "append": `true` = "search
  the whole space, do not just append".
- The recursive self-call at line 229 explicitly switches to
  `true` when there is not enough room to append — i.e. it falls
  back to gap search.

Judgement:
- Yes, the parameter name suggests append-style placement but the
  branch it gates is the full gap-scan / search-all path.

Proposals:
- `gapScan`   (medium)
- `searchAll` (low)
- `fullScan`  (low)
- keep current (low) — only if the name is inherited verbatim from
  the original (no tier-1 evidence found in this batch).

Locations consulted:
- declared: include/zhinst/cache.hpp:147
- used:     src/cache.cpp:174-296

### Cache::play::state  [yes / medium / cross-batch-arbitration]

Evidence:
- `cache.hpp:152` `void play(std::shared_ptr<Pointer> ptr, PointerState state);`
- `cache.cpp:411-414`
  ```
  if (state == PointerState::Free) {
      (*it)->state_ = PointerState::LastPlayed;
  } else {
      (*it)->state_ = PointerState::Playing;
  }
  ```
  → only the `Free`/non-`Free` distinction is tested; the parameter
  acts as a two-valued switch, not a destination state.
- `prefetch.cpp:1773-1777` and `prefetch.cpp:1812-1815` — at both
  call sites the value passed is `curState.counter()` cast to
  `Cache::PointerState`:
  ```
  auto pointerState = static_cast<Cache::PointerState>(curState.counter());
  cachePtr->play(loadCachePtr, pointerState);
  ```
- `prefetch.hpp:154-155`
  ```
  int32_t& counter()       { return state; }
  int32_t  counter() const { return state; }
  ```

Interpretation:
- The parameter is declared as `PointerState`, but Cache::play uses
  it only as `==Free ? LastPlayed : Playing` — i.e. a binary marker
  ("did we just play it for the last time, or are we still
  playing?").
- Callers pass a counter value, not a state, but coerced into the
  enum via `static_cast`. The actual numeric values (counter low
  bits) likely encode whether this is the final play.
- One side is misnamed: either Cache's `state` parameter is
  really a marker / "playMode" / "isLast" flag, or
  `PrefetcherNodeState::counter` is misnamed and is in fact a state
  enum.

Judgement:
- Yes, names disagree across the call boundary; cannot be resolved
  in this batch alone.

Proposals:
- keep current   (low)
- `mode`         (low)
- `isLastPlay`   (low) — if the binary marker semantic is correct
- `marker`       (low)

Cross-reference:
- Counterpart `PrefetcherNodeState::counter()` /
  `PrefetcherNodeState::state` — batch 09 (prefetch).

Locations consulted:
- declared: include/zhinst/cache.hpp:152
- used:     src/cache.cpp:388-418; src/prefetch.cpp:1770-1777,1810-1815

### Cache::PointerState::LastPlayed / Playing  [no / high / not-misnomer]

Evidence:
- `cache.hpp:76-81` enum declaration with hand-comment
  `1 = LastPlayed ("last played")`, `2 = Playing ("playing")`.
- `cache.cpp:479` `static const char* stateNames[] = {"ready", "last played", "playing", "free"};`
- `strings _seqc_compiler.so` lists `"playing"` and `"last played"`
  as literal strings in the binary's `.rodata`.

Interpretation:
- The string table inside `Cache::Pointer::str()` matches strings
  found verbatim in the original binary, indexed by the enum value.
- The enum-member names are an exact CamelCase rendering of those
  authoritative log strings.

Judgement:
- The enum-member names match tier-2 evidence; not misnomers.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/cache.hpp:76-81
- used:     src/cache.cpp:65,96,164,381,388-417,421-431,479-487
- binary:   strings(_seqc_compiler.so) → "playing", "last played"

## 4. Symbols inspected and judged routinely fine

- `CacheException::CacheException::msg` — direct ctor parameter
  whose value initializes `message_`; name fits.
- `CacheException::message_` — held string, returned by `what()`.
- `CacheException::what()::*` — no parameters.
- `Cache::size_` — total cache memory, divisor and free-memory base
  throughout cache.cpp.
- `Cache::pointers_` — `vector<shared_ptr<Pointer>>` of live
  allocations.
- `Cache::Cache::size`, `Cache::Cache::pageSize` — straightforward
  initializers of like-named fields.
- `Cache::Pointer::position_`, `Cache::Pointer::size_`,
  `Cache::Pointer::waveform_`, `Cache::Pointer::state_` — all match
  their use.
- `Cache::PointerState::Ready`, `Cache::PointerState::Free` —
  match contextual usage (initial / freed slot); no contradicting
  evidence.
- `Cache::CacheType::Normal`, `Cache::CacheType::Aligned` — selector
  for whether `pageSize_` is doubled in `allocate(CacheType)`.
- `Cache::allocate(5-arg)::waveform`, `…::nameMap`, `…::split` —
  documented usage matches name (waveform pointer, eviction map,
  split flag).
- `Cache::allocate(5-arg)` locals: `freeMemory`, `freePages`,
  `result`, `numAllocs`, `halfSize`, `chunkSize`, `ptr`, `halfSz` —
  all match their computed expressions; `numAllocs` is plausibly
  the chunk count.
- `Cache::allocate(CacheType)::cacheType`, `…::waveform`,
  `…::nameMap`; locals `pageSize`, `remainder`, `alignedSize`,
  `result`, `ptr` — match.
- `Cache::getBestPosition::nameMap` — name → bool map; entries with
  value `true` are skipped during gap calculation, matching its
  documented role as the eviction set.
- `Cache::getBestPosition` locals: `pointer`, `begin`, `end`,
  `lastIt`, `lastPtr`, `endPos`, `freeSpace`, `totalSize`,
  `currentEnd`, `bestGap`, `gap`, `mapIt`, `trailingEnd`,
  `trailingGap` — all faithfully named for their contents.
- `Cache::memoryWrite::ptr` and locals
  `ptrPos`, `ptrEnd`, `existPos`, `existEnd`, `insertPos`, `it` —
  match.
- `Cache::stillInMemory::ptr`; locals `existName`, `targetName` —
  match.
- `Cache::reuse::ptr`; same locals — match.
- `Cache::play::ptr`; locals `s`, `pos`, `it` — match.
- `Cache::resetPlay`, `Cache::print`, `Cache::Pointer::str` — no
  parameters of concern; locals `oss`, `stateNames`, `stateLens`
  match.
- `Cache::free::ptr`; locals `found`, `it` — match.

## 5. Coverage

- **Fully covered:** every in-scope symbol in `cache.hpp` and
  `cache.cpp`: all parameters of free functions and methods; all
  data members of `Cache`, `Cache::Pointer`, `CacheException`; all
  enum members of `Cache::PointerState` and `Cache::CacheType`;
  obviously-misleading locals.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type names `Cache`, `Cache::Pointer`, `CacheException`,
    `Cache::PointerState`, `Cache::CacheType` — all appear in the
    original binary's mangled symbol table; excluded by §3.
  - All Cache/CacheException method names — present in symbol
    table; excluded.
  - Constants `unusedCacheLine` (exported as
    `zhinst::unusedCacheLine` at 0x95deac) and `kCacheFormat`
    (exported as
    `zhinst::(anonymous namespace)::kCacheFormat` at 0x95e8f8) —
    both authoritative per binary; excluded.
  - Template parameter names, member type aliases, third-party
    headers — out of scope per §2.
- **Cross-batch arbitration pending** (see §3):
  - `Cache::allocate(5-arg)::pageSize` ↔ `Prefetch::maxBranches_`
    (batch 09).
  - `Cache::play::state` ↔ `PrefetcherNodeState::counter` /
    `state` (batch 09).
- **Status:** `complete`.
