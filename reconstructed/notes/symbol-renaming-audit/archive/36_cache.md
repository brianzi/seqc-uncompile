# Batch 36 — cache (symbol renaming audit)

Files audited:
- `reconstructed/include/zhinst/cache.hpp`
- `reconstructed/src/cache.cpp`

## Symbols inspected

CacheException
- ctor param `msg`, member `message_`

Cache (class)
- ctor params: `size`, `pageSize`, `appendMode`
- members: `size_`, `pageSize_`, `appendMode_`, `pointers_`
- `getSize()`, `getPageSize()`, `getFreeMemory()`, `getScope()` — no params
- `allocate(5-arg)` params: `waveform`, `numSamples`, `nameMap`, `pageSize`, `split`
- `allocate(CacheType)` params: `waveform`, `numSamples`, `nameMap`, `cacheType`
- `getBestPosition` params: `numSamples`, `nameMap`, `appendMode`
- `memoryWrite(ptr)`, `stillInMemory(ptr)`, `reuse(ptr)`, `play(ptr, state)`,
  `resetPlay()`, `free(ptr)`, `print()`
- locals of note: in `Cache::allocate(5-arg)` —
  `freeMemory`, `freePages`, `numAllocs`, `halfSize`, `altAllocs`, `chunkSize`, `halfSz`
- in `Cache::allocate(CacheType)` — `pageSize`, `remainder`, `alignedSize`
- in `getBestPosition` — `begin`, `end`, `lastIt`, `endPos`, `freeSpace`,
  `totalSize`, `currentEnd`, `bestGap`, `bestPosition`, `mapIt`, `trailingEnd`,
  `trailingGap`
- in `memoryWrite` — `ptrPos`, `ptrEnd`, `existPos`, `existEnd`, `insertPos`
- in `play` — `pos`, `s`
- in `Pointer::str` — `stateNames`, `stateLens`

Cache::Pointer (struct)
- members: `position_`, `size_`, `hash_`, `numRepeats_`, `waveform_`, `state_`

Free constants
- `unusedCacheLine`

---

## Suspect blocks

### S1 — `Cache::allocate(5-arg)` parameter `pageSize`

- symbol:        `Cache::allocate(shared_ptr<WaveformIR>, AddressImpl<uint32_t>, unordered_map<string,bool> const&, int pageSize, bool split)`
- declared at:   `include/zhinst/cache.hpp:130-135` (param at line 134)
- defined/used:  `src/cache.cpp:86-137` (param used at line 100)
- call sites:    `src/prefetch.cpp:1657-1663` — passes `maxBranches_` (the
                 Prefetch member at +0xB8) into this argument; `src/cache.cpp:1433`
                 (comment) restates this.
- observations:
  * The body uses the parameter exclusively as a divisor in
    `freePages = freeMemory / pageSize` (`cache.cpp:100`). It is never
    interpreted as a sample/byte page size, never multiplied by 2, never
    used for alignment.
  * The only caller (`prefetch.cpp:1662`) passes `maxBranches_` — the maximum
    branch depth from `Prefetch::countBranches`. Confirmed by
    `prefetch_prepare.cpp:411-413` and `OVERVIEW_phases_1-12.md:395`
    ("+0xB8=maxBranches_, NOT pageSize_") — the field that used to be called
    `pageSize_` on `Prefetch` was renamed to `maxBranches_` after disassembly
    showed it had nothing to do with paging.
  * The shadowing of the member `Cache::pageSize_` (the actual page size,
    used for alignment in the `(CacheType)` overload) reinforces the misnaming.
- judgement:     The parameter name is a leftover from when the corresponding
                 caller field was misidentified as a "page size". The renaming
                 of the source field on `Prefetch` (to `maxBranches_`) was never
                 propagated to this parameter. Reading `cache.cpp:100` with the
                 current name suggests "divide by page size" which is wrong;
                 the correct reading is "divide free memory across max branches
                 to compute per-branch page budget".
- confidence:    high
- proposals:     `maxBranches`, `branchCount`, `pagesPerBranch` *(naming should
                 mirror whatever name is settled on for `Prefetch::maxBranches_`)*

### S2 — `Cache::allocate(5-arg)` parameter `split`

- symbol:        `Cache::allocate(..., bool split)`
- declared at:   `include/zhinst/cache.hpp:135`
- defined/used:  `src/cache.cpp:91, 104`
- call site:     `src/prefetch.cpp:1663` — passes `Prefetch::split_` (+0xBC).
- observations:
  * Body at `cache.cpp:104`: `if (split || numSamples < freePages) { allocate
    Normal (full waveform, no chunking) } else { compute chunkSize and allocate
    Aligned chunk }`.
  * Thus when `split == true`, the function takes the **non-splitting**
    branch (single Normal allocation). When `split == false` and the waveform
    is too big, it actually splits.
  * Caller `Prefetch::split_` (`prefetch.cpp:46, 1595, 1604, 2241`) is set
    `true` at `0x1cbfda` after `placeLoads` decides it cannot fit waveforms;
    `prefetch.cpp:2198` comment "set split_ = true and..." matches the
    fallback semantics. So `split_` on Prefetch may itself be a misnomer
    aliasing `isHirzel_` (see `prefetch.hpp:303-311`).
- judgement:     The parameter name is at best ambiguous and at worst inverted
                 relative to its in-body effect: passing `true` *avoids*
                 splitting in this function. Whether the right rename is
                 `noSplit`, `singleAllocation`, or something matching the
                 (also-suspect) caller field needs Prefetch-side resolution.
                 Flag with low/medium confidence to defer to synthesis.
- confidence:    medium
- proposals:     *(none with confidence; consider `noSplit` / `forceSingle` /
                 same name as resolved Prefetch field)*
- status:        verify-not-original *(parameter name may have been chosen to
                 mirror the caller field; both should be resolved together)*

### S3 — `Cache::getBestPosition` parameter `appendMode`

- symbol:        `Cache::getBestPosition(AddressImpl<uint32_t> numSamples, unordered_map<string,bool> const& nameMap, bool appendMode)`
- declared at:   `include/zhinst/cache.hpp:144-147`
- defined/used:  `src/cache.cpp:174-296`
- call sites:    `src/cache.cpp:160` — `getBestPosition(alignedSize, nameMap, false)`
                 (only external call); `src/cache.cpp:229` — recursive call with
                 `true` after the fast-path fails.
- observations:
  * The class already has a member `Cache::appendMode_` (`hpp:160`) checked
    at `cache.cpp:190` — and it has *different* semantics: when set, the
    function unconditionally returns `position=0`.
  * The parameter is not "the" append mode at all: it controls a fast-path /
    fallback within the gap-search path. `false` selects "try appending after
    last allocation"; `true` selects "scan all gaps for best fit".
    (`cache.cpp:212` vs `cache.cpp:230-294`.)
  * The function shadows the member name with the parameter, and the comment
    at `cache.cpp:189-200` explicitly distinguishes "this->appendMode_" from
    the parameter, indicating the author already noticed the confusion.
  * Comment at `cache.cpp:198-200` describes the parameter's actual role:
    "appendMode==false: try to place at end of last allocation (fast path);
    appendMode==true: gap-scan all pointers for best-fit gap."  The label
    is essentially backwards from intuition: the *append-after-last* path
    is the `false` value.
- judgement:     The name is misleading on two counts: (a) it shadows a member
                 of the same name with different semantics, and (b) the
                 boolean polarity is counter-intuitive — `false` is the
                 append-at-end strategy. A name that describes "do a full
                 gap scan" or "fast path tried already" would be much clearer.
- confidence:    high
- proposals:     `gapScan`, `forceGapScan`, `fallback` *(default-call passes
                 false → "no gap scan yet, try fast path"; recursive call
                 passes true → "fast path failed, do gap scan")*

### S4 — `Cache::play` parameter `state`

- symbol:        `Cache::play(shared_ptr<Pointer> ptr, PointerState state)`
- declared at:   `include/zhinst/cache.hpp:152`
- defined/used:  `src/cache.cpp:388-418`
- call sites:    `src/prefetch.cpp:1756, 1794` — both pass
                 `static_cast<Cache::PointerState>(curState.counter())`.
- observations:
  * The body never assigns the parameter as the new state. It is used
    only as a discriminator at `cache.cpp:411`: `if (state == Free) →
    new state := LastPlayed; else → new state := Playing`.
  * Callers pass `nodeState.counter()` (an int, semantically "remaining
    iterations"), reinterpreted as `PointerState`. Thus 0 maps to `Ready`
    in the enum but is not interpreted as `Ready` here — it triggers the
    `Playing` branch (since `Ready != Free`).
  * The intent appears to be "is this the last play?" — caller's counter
    of 0 (loop exhausted) should mean "mark LastPlayed" via the `Free`
    enum value's numeric position; non-zero (more iterations) means
    "Playing". The conversion through `PointerState::Free` is an
    obfuscation in the caller, not really a state value.
- judgement:     The parameter name `state` suggests "the new state to set",
                 which is wrong on two counts: (1) it is only ever compared
                 to `Free`, never assigned, (2) the call-site value is a
                 counter, not a state. A name like `isLast`, `lastPlay`, or
                 `remainingCount` would describe the actual usage.
- confidence:    medium
- proposals:     `isLast` (bool; would change the type), `triggerState`,
                 `lastPlayMarker`
- status:        verify-not-original *(the original symbol name on the
                 binary may itself be `state`; if so this is excluded)*

### S5 — `Cache::Pointer::hash_` (data member)

- symbol:        `Cache::Pointer::hash_`
- declared at:   `include/zhinst/cache.hpp:106`
- defined/used:  written at `src/cache.cpp:132, 184`; read at
                 `src/prefetch_splitplay.cpp:325` (used as
                 "the start-address used by the hash lookup").
- observations:
  * Assigned at `cache.cpp:132` as
    `~(position_ ^ (position_ + halfSz))`. This is a small bit-twiddling
    formula that produces a derived 32-bit address used for cache-line
    wrapping in double-buffered playback, not a hash in the
    typical sense (no key, no collision space, no canonical hash function).
  * Read in `prefetch_splitplay.cpp:325` and stored as `cacheHash` which
    is then emitted into the assembler stream as an address constant for
    the `prf` (prefetch) opcode.
  * Note `numRepeats_` (next slot) is the matching companion: chunk count
    for the streamer. Together they parameterise sequencer-side
    double-buffer wrap-around.
- judgement:     "hash" misdescribes a derived prefetch wrap-address. The
                 field is an address mask/wraparound constant for the
                 sequencer's prefetch unit. However the binary may well
                 use the name `hash` for this, and the formula does
                 superficially resemble a hash (xor + complement). Hold
                 with low confidence.
- confidence:    low
- proposals:     *(none — usage observed but original-binary name not
                 verified; could legitimately be `hash`)*
- status:        verify-not-original

### S6 — `unusedCacheLine` (file-scope constant)

- symbol:        `zhinst::unusedCacheLine`
- declared at:   `include/zhinst/cache.hpp:166`
- defined/used:  declared `static constexpr uint32_t = 0xFFFFFFFF`. No
                 reference under `reconstructed/src/` matches the symbol
                 (`grep` returns only the declaration site and the
                 `magic_numbers_proposal.md` reference to
                 `memory_allocator.cpp:51,66,109,143`, where the constant
                 is **not currently used by name** — those sites use the
                 literal `0xFFFFFFFF`).
  Original binary location noted in the header: `0x95deac`
  (and `unknowns_full_1-116.md:352` records the address as item #65).
- observations:
  * The name uses "cache line", a CPU-microarchitecture term, but the
    binary symbol lives next to other waveform-cache state. The actual
    usage in the original (per `magic_numbers_proposal.md`) is as a
    sentinel for an "unallocated entry" in MemoryAllocator's tables, not
    a CPU cache-line indicator.
  * The constant has zero in-tree uses today — it cannot be confirmed
    from call-site context.
- judgement:     Name is plausibly a faithful binary symbol (it has a
                 specific .rodata address) but reads as a microarch term
                 that does not match its true role. Cannot decide from
                 usage alone.
- confidence:    low
- proposals:     *(none)*
- status:        verify-not-original

### S7 — `Cache::allocate(5-arg)` local `chunkSize` and friends

- symbol:        locals `numAllocs`, `halfSize`, `altAllocs`, `chunkSize`,
                 `halfSz` in `Cache::allocate(5-arg)`
- declared at:   `src/cache.cpp:117-133`
- observations:
  * `numAllocs`, `altAllocs` are integer counts of chunks. The variables
    `halfSize` (line 119, `size_ >> 1`) and `halfSz` (line 131, `ptr->size_/2`)
    are two different "halves" of two different things: the cache total
    and the allocated chunk respectively.
  * Comment at line 111-116 describes the algorithm reasonably and refers
    to `unknowns.md #63`.
- judgement:     Reusing both `halfSize` and `halfSz` as locals in the same
                 scope a few lines apart is a readability hazard but does
                 not actively mislead about what the value is. Marginal —
                 noting for completeness, not flagging.
- confidence:    low
- proposals:     *(none — local-variable cleanup only)*

---

## Symbols inspected and judged fine (no flag)

- `CacheException::message_`, ctor param `msg` — straightforward; matches
  `std::exception` convention.
- `Cache` ctor params `size`, `pageSize`, `appendMode` and corresponding
  members `size_`, `pageSize_`, `appendMode_` — pageSize_ here is the
  *real* alignment unit (used at `cache.cpp:147-149` and doubled for
  `Aligned`); appendMode_ behaviour matches the name (offset-zero
  placement when set, `cache.cpp:190-194`); size_ is the total cache
  capacity used as the upper bound throughout. Names match usage.
- `pointers_` — vector of allocated Pointer entries kept sorted by
  position. Name is generic but accurate.
- `Pointer::position_`, `size_`, `numRepeats_`, `waveform_`, `state_`
  — all match observed usage; `numRepeats_` is consistently consumed
  in `prefetch_splitplay.cpp:138-156` as a chunk-repeat count
  (verified previously in `OVERVIEW_phases_1-12.md:639`).
- `getBestPosition` locals `bestGap`, `bestPosition`, `currentEnd`,
  `trailingEnd`, `trailingGap` — accurate descriptions of the gap-scan
  algorithm.
- `memoryWrite` locals `ptrPos`, `ptrEnd`, `existPos`, `existEnd`,
  `insertPos` — accurate.
- `nameMap` parameter on the three Cache methods — matches the
  `Prefetch::nameMap_` member it is passed from (`prefetch.cpp:1661`,
  `prefetch.hpp:65, 260`); the name mirrors the (already-audited)
  caller field.
- `cacheType` parameter on `allocate(CacheType)` — matches the
  `CacheType` enum.
- `Pointer::str()` locals `stateNames`, `stateLens` — accurate.

## Coverage

- **Fully covered:** every parameter, every data member, and every
  obviously load-bearing local in both `cache.hpp` and `cache.cpp`.
  Six suspect blocks raised (S1 high, S3 high; S2/S4 medium; S5/S6 low),
  plus one note-only entry (S7).
- **Deferred to synthesis:** S2 (`split`) and S5 (`hash_`) and S6
  (`unusedCacheLine`) are tagged `verify-not-original` because the binary
  may carry these names directly. The audit cannot confirm without RTTI
  or symbol-table evidence.
- **Not covered (out of scope):** function/type names (`memoryWrite`,
  `stillInMemory`, `reuse`, `getBestPosition`, `getScope`, `Pointer`,
  etc. — these are class/method names, excluded per RULES section
  "Scope"). The `Pointer::str()` static `stateNames`/`stateLens` arrays
  are file-scope helpers, not parameters/members; left untouched.
