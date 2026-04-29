# Batch 09 — prefetch (part 1)

## 1. Files considered

- `reconstructed/include/zhinst/prefetch.hpp`
- `reconstructed/src/prefetch.cpp`
- `reconstructed/src/prefetch_helpers.cpp` (skim only — see §5)
- `reconstructed/src/prefetch_emit.cpp` (skim only — see §5)
- `reconstructed/src/prefetch_print.cpp` (skim only — see §5)
- `reconstructed/src/prefetch_prepare.cpp` (countBranches, definePlaySize)
- `reconstructed/src/prefetch_placesingle.cpp` (skim only — see §5)
- `reconstructed/src/prefetch_splitplay.cpp` (skim only — see §5)

Cross-batch counterpart consulted:
`reconstructed/notes/symbol-renaming-audit/36_cache.md`.

Binary symbol table consulted via
`nm --demangle _seqc_compiler.so | grep Prefetch`. The following are
authoritative under §3 (excluded from rename):

- Type `zhinst::Prefetch`.
- Nested type `zhinst::Prefetch::PrefetcherNodeState` (appears in
  `unordered_map<…, Prefetch::PrefetcherNodeState>` mangled symbols
  — STL instantiations in the binary).
- All `Prefetch::*` method names listed in `prefetch.hpp` (every one
  appears as a `t` symbol in `nm` output).
- Static `zhinst::Prefetch::minIndexedSize` (appears as a `b` symbol).

What is NOT excluded: parameter names, member names, enum members
(none here), local variables, `PrefetcherNodeState` *member* names
(only the type itself is in the symbol table).

## 2. Overview

This batch is partial. The table below lists only the symbols actually
inspected in this round (Prefetch class members + ctor parameters +
PrefetcherNodeState members and alias methods). Method parameters of
the large methods, helper free-function parameters, and locals are
deferred to part 2 (see §5).

Group order: Prefetch fields → Prefetch ctor params → PrefetcherNodeState
fields → PrefetcherNodeState alias methods → Prefetch::isHirzel_/set
helpers → static `Prefetch::minIndexedSize` → nested `UsageEntry`
fields.

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `Prefetch::config_` | no | high | `AWGCompilerConfig const*` ptr | keep current (high) | — |
| `Prefetch::devConst_` | no | high | `DeviceConstants const*` ptr | keep current (high) | — |
| `Prefetch::nodeStates_` | no | high | map node → PNS | keep current (high) | — |
| `Prefetch::nameMap_` | no | high | name→bool eviction map | keep current (high) | — |
| `Prefetch::root_` | no | high | tree root node | keep current (high) | — |
| `Prefetch::asmCommands_` | no | high | shared\_ptr to AsmCommands | keep current (high) | — |
| `Prefetch::resources_` | no | high | shared\_ptr to Resources | keep current (high) | — |
| `Prefetch::cache_` | no | high | shared\_ptr to Cache | keep current (high) | — |
| `Prefetch::waveformMaps_` | no | medium | per-channel-group bimaps | keep current (medium) | — |
| `Prefetch::maxBranches_` | no | high | max branch depth, divisor of cache | keep current (high) | not-misnomer |
| `Prefetch::split_` | no | high | distinct from isHirzel\_ alias | keep current (high) | not-misnomer |
| `Prefetch::cwvfConfig_` | no | high | last-seen common PlayConfig | keep current (high) | — |
| `Prefetch::usageEntries_` | no | medium | log of emitted PlayConfigs | keep current (medium) | — |
| `Prefetch::lastCwvfNode_` | no | high | tracked node for cwvf init | keep current (high) | — |
| `Prefetch::globalCwvfValid_` | no | high | "cwvfConfig\_ usable globally" | keep current (high) | — |
| `Prefetch::wavetableIR_` | no | high | shared\_ptr to WavetableIR | keep current (high) | — |
| `Prefetch::logFunc_` | no | high | log callback | keep current (high) | — |
| `Prefetch::cancelCb_` | no | high | weak\_ptr to CancelCallback | keep current (high) | — |
| `Prefetch::minIndexedSize` (static) | unsure | low | threshold; provenance opaque | keep current, `minIndexedCacheSize` | out-of-scope (nm: zhinst::Prefetch::minIndexedSize defined) |
| `Prefetch::Prefetch::config` | no | high | initializes `config_` | keep current (high) | — |
| `Prefetch::Prefetch::devConst` | no | high | initializes `devConst_` | keep current (high) | — |
| `Prefetch::Prefetch::asmCommands` | no | high | initializes `asmCommands_` | keep current (high) | — |
| `Prefetch::Prefetch::root` | no | high | initializes `root_` | keep current (high) | — |
| `Prefetch::Prefetch::wavetableIR` | no | high | initializes `wavetableIR_` | keep current (high) | — |
| `Prefetch::Prefetch::logFunc` | no | high | initializes `logFunc_` | keep current (high) | — |
| `Prefetch::Prefetch::cancelCb` | no | high | initializes `cancelCb_` | keep current (high) | — |
| `PrefetcherNodeState::registerHirzel` | no | medium | Hirzel-path AsmRegister slot | keep current (medium) | — |
| `PrefetcherNodeState::registerCervino` | no | medium | Cervino-path AsmRegister slot | keep current (medium) | — |
| `PrefetcherNodeState::state` | yes | medium | only used as Cache::play state arg | `playState`, keep current | cross-batch-arbitration |
| `PrefetcherNodeState::branchCount` | no | high | counter in countBranches | keep current (high) | — |
| `PrefetcherNodeState::refTrack` | unsure | low | inc/dec ref count in optimize | keep current, `refCount` | — |
| `PrefetcherNodeState::pageSize` | yes | medium | stores `pagesNeeded` count | `pagesNeeded`, `numPages` | — |
| `PrefetcherNodeState::requiredSlots` | yes | medium | only used as `usedCache()` | `usedCache_`, keep current | coordinated-rename |
| `PrefetcherNodeState::cachePtr` | no | high | `shared_ptr<Cache::Pointer>` | keep current (high) | — |
| `PrefetcherNodeState::useDA` | unsure | low | initialized from `crossesCacheLine_` | keep current, `crossesCacheLine` | — |
| `PrefetcherNodeState::lengthReg()` (alias) | yes | high | aliases `registerHirzel`, not "length" | drop alias, use `registerHirzel` | coordinated-rename |
| `PrefetcherNodeState::counter()` (alias) | yes | high | aliases `state`, used as Cache state | drop alias, use `state` | coordinated-rename |
| `PrefetcherNodeState::playSize()` (alias) | yes | medium | aliases `pageSize`; both names suspect | drop alias, rename field | coordinated-rename |
| `PrefetcherNodeState::usedCache()` (alias) | no | high | accurate name for `requiredSlots` | drop alias, rename field to match | coordinated-rename |
| `Prefetch::isHirzel_()` (private alias) | yes | high | always returns `split_`; not Hirzel test | drop alias | — |
| `Prefetch::set_isHirzel_()` (private alias) | yes | high | always sets `split_` | drop alias | — |
| `UsageEntry::config` | no | high | first 0x20 bytes is PlayConfig | keep current (high) | — |

## 3. Detailed findings

### Prefetch::maxBranches_  [no / high / not-misnomer]

Counterpart to batch 36's `Cache::allocate(5-arg)::pageSize` flag.

Evidence:
- `prefetch.hpp:266` `int32_t maxBranches_;  // +0xB8 (init=1, used by countBranches/definePlaySize)`
- `prefetch.cpp:43` `, maxBranches_(1)` — initialized to 1 in ctor.
- `prefetch_prepare.cpp:408-413` (in `countBranches`):
  ```
  int newCount = branchCount + numBranches - 1;
  if (newCount > this->maxBranches_)
      this->maxBranches_ = newCount;
  ```
  i.e. updated to the running maximum of `parent.branchCount + (#branches − 1)`.
- `prefetch_prepare.cpp:328` historical comment:
  `>>> DISCOVERY: +0xB8 is maxBranches_ (int), NOT pageSize_.`
- `prefetch_prepare.cpp:750`
  `int memPerPage = wfMemSize / this->maxBranches_;` — used as a
  divisor of waveform memory to compute per-branch page memory.
- `prefetch_prepare.cpp:762`
  `int memTotal = nodeLength * cc5 * this->maxBranches_;` — used as
  a multiplier representing the number of branches over which a wave
  must be resident.
- `prefetch.cpp:1683` passed to `Cache::allocate(5-arg)` as the
  argument named `pageSize` in cache.hpp:
  ```
  cachePtr->allocate(waveIR1, …, nameMap_, maxBranches_, split_);
  ```
- `cache.cpp:100` (per batch 36) consumes that argument as
  `freePages = freeMemory / pageSize` — i.e. divides cache memory by
  the same value, mirroring `prefetch_prepare.cpp:750`.
- Only one `int` field ever lives at Prefetch+0xB8; no alias
  competes for that slot.

Interpretation:
- Both producer and consumers in Prefetch treat the value as the
  maximum number of independent branch-paths through the tree, used
  as a divisor of cache memory to obtain a per-branch budget and as
  a multiplier to obtain a per-branch memory total.
- Cache uses the very same value as a divisor of cache memory
  (`freePages = freeMemory / pageSize`). Semantically this is
  `cacheBytes / branches = bytesPerBranch` — a "page" only in the
  sense that each branch gets one page-equivalent slot.
- The Prefetch-side name accurately describes the value's
  provenance and meaning. The Cache-side parameter name is the one
  that misleads.

Judgement:
- The Prefetch field name is correct; not a misnomer. The misnomer
  in this he-said/she-said is on the Cache side (already flagged in
  batch 36 as `Cache::allocate(5-arg)::pageSize`).

Proposals:
- keep current  (high)

Cross-reference:
- Counterpart `Cache::allocate(5-arg)::pageSize` — batch 36, where
  the misnomer should be resolved. Suggested cache-side rename:
  `numBranches` / `branches` / `bytesPerBranchDivisor`.

Locations consulted:
- declared: include/zhinst/prefetch.hpp:266
- used:     src/prefetch.cpp:43,1683;
            src/prefetch_prepare.cpp:320-328,408-413,748-765
- related:  src/cache.cpp:100,104 (via batch 36)

### Prefetch::split_  [no / high / not-misnomer]

Counterpart to batch 36's note that `Prefetch::split_` may be a
misnomer aliasing `isHirzel_`.

Evidence:
- `prefetch.hpp:267` `bool split_;  // +0xBC (init=false)`
- `prefetch.cpp:46` `, split_(false)`
- `prefetch.cpp:2263-2265` (in `placeLoads`):
  ```
  if (required <= cacheSize || config_->appendMode()) {
      split_ = true;
      auto newRoot = moveLoadsToFront(root_);
      ...
  }
  ```
  i.e. set true precisely when entering the splitting / move-loads
  code path. Note: `config_->appendMode()` (the Hirzel test) is just
  one of two triggers; `required <= cacheSize` is the other.
- `prefetch.cpp:1607-1620` (in `allocate`): `split_` gates whether to
  proceed with the cache-allocation path even when no play exists or
  when playNode is null.
- `prefetch.cpp:1684` passed to `Cache::allocate(5-arg)` as its
  `split` parameter — semantic match.
- `prefetch_placesingle.cpp:676,732` `if (split_)` / `if (regVal != 0
  && !split_)` — split-vs-non-split codegen branches.
- `prefetch.hpp:301-311` records that the legacy `isHirzel_()` /
  `set_isHirzel_()` methods are *aliases* over the same `split_`
  byte; they were misidentified during initial reconstruction.

Interpretation:
- `split_` is set true whenever the prefetcher commits to the
  load-splitting path, which on Hirzel devices happens
  unconditionally (because `config_->appendMode()` is true) but on
  non-Hirzel devices also happens whenever `required <= cacheSize`.
  So the flag is genuinely a "splitting mode is active" bit, not a
  Hirzel device test.
- The historical `isHirzel_` accessor is the misnomer — it returns
  the splitting flag, which is correlated with but not equivalent to
  device family.
- Consumer use sites read it as a "are we in split mode?" predicate
  (allocation paths, cwvf placement) rather than as a device-family
  test.

Judgement:
- The field name `split_` is correct; the alias `isHirzel_()` is the
  misnomer (and is a pure alias method, addressed below).

Proposals:
- keep current  (high)

Cross-reference:
- Counterpart `Cache::appendMode_` — batch 36 (also unsure there).
  These are different-but-related: `Cache::appendMode_` carries the
  raw Hirzel bit forever; `Prefetch::split_` is a derived superset.
- Counterpart `Prefetch::isHirzel_()` — see block below.

Locations consulted:
- declared: include/zhinst/prefetch.hpp:267
- used:     src/prefetch.cpp:46,1607-1620,1684,2263-2265,2305;
            src/prefetch_placesingle.cpp:676,729,732,808

### PrefetcherNodeState::state  [yes / medium / cross-batch-arbitration]

Counterpart to batch 36's `Cache::play::state` flag.

Evidence:
- `prefetch.hpp:138` `int32_t state = 3;  // +0x10 (3=unloaded)`
- `prefetch.hpp:154-155` aliases:
  ```
  int32_t& counter()        { return state; }
  int32_t  counter() const  { return state; }
  ```
- `prefetch.cpp:1773-1777` (Table case in `allocate`):
  ```
  auto pointerState = static_cast<Cache::PointerState>(
      curState.counter());
  cachePtr->play(loadCachePtr, pointerState);
  ```
- `prefetch.cpp:1812-1815` (Play case in `allocate`): same pattern.
- The only writes to this field located in this batch's pass are the
  default-init `= 3` in the struct and `mov [rax+0x30],0` references
  in disassembly comments (e.g. prefetch.hpp:118, prefetch.cpp:1188-1194
  set it to 0 via the `counter()` alias indirectly).
- No site reads it as a four-state ready/last-played/playing/free
  enum within Prefetch itself; all such interpretation happens
  inside Cache after the cast.

Interpretation:
- The field is named `state`, declared `int32_t`, but used at every
  located call site by being cast to `Cache::PointerState` and
  passed to `Cache::play`.
- Conversely Cache::play (per batch 36) ignores all state values
  except `Free`, treating the parameter as a 2-valued switch.
- Either side is half-wrong: in Prefetch it is a state-counter that
  doubles as the Cache::play argument; in Cache it is a single
  bit-marker. Naming it `state` is consistent with the Cache enum
  type but obscures that within Prefetch it functions as a
  monotonically-modified counter (the "counter()" alias), and the
  initial value `3` is `Cache::PointerState::Free` — treated by
  Cache::play as "make this LastPlayed" rather than as "this slot is
  free".
- The `counter()` alias name is also misleading: the field is not
  incremented as a counter in any site found; it is set to discrete
  values that come from the `PointerState` enum.

Judgement:
- Yes, the field name `state` is technically faithful to the type
  but the legacy `counter()` alias and the cross-boundary mismatch
  with `Cache::play::state` indicate the situation is not yet
  cleanly resolved; arbitration cannot conclude in this batch
  alone.

Proposals:
- keep current  (medium) — `state` is at least the correct enum
  semantic on the Prefetch side
- `playState`   (low)
- `cacheState`  (low)
- drop the `counter()` alias regardless

Cross-reference:
- Counterpart `Cache::play::state` — batch 36 (cross-batch-arbitration).
- Closely tied to the disposition of `Cache::PointerState::Free`
  (initial "fresh" sentinel) — see batch 36.

Locations consulted:
- declared: include/zhinst/prefetch.hpp:138,154-155
- used:     src/prefetch.cpp:1188-1194,1773-1777,1812-1815;
            indirect comments at prefetch.hpp:117-118

### PrefetcherNodeState::pageSize / playSize() alias  [yes / medium / coordinated-rename]

Evidence:
- `prefetch.hpp:141` `int32_t pageSize = 1;  // +0x1C`
- `prefetch.hpp:156-157` aliases:
  ```
  int32_t& playSize()        { return pageSize; }
  int32_t  playSize() const  { return pageSize; }
  ```
- `prefetch_prepare.cpp:768-806` (in `definePlaySize`):
  ```
  if (pagesNeeded >= 2) {
      ...
      nsIt->second.playSize() = pagesNeeded;     // +0x3c
      ...
      ppIt->second.playSize() = pagesNeeded;     // parent
  }
  ```
- `prefetch_placesingle.cpp:484` `if (loadSt2.pageSize >= 2)` —
  consumed as a count, threshold-tested against 2.
- `prefetch_placesingle.cpp:616-620`:
  ```
  int pageSize = 1;
  ...
  pageSize = nodeStates_.at(node).pageSize;
  int pageCount = sizePerDev / pageSize;
  ```
  — used as a divisor of byte-size to obtain a `pageCount`. This
  matches "pages-needed" semantic, not "page size in bytes".
- `prefetch_placesingle.cpp:642`
  `int configPageField = nodeStates_[node].pageSize;`
- `prefetch.cpp:1188-1194,1252-1265`:
  `if (it->second.pageSize >= 2)` and
  `pagesNeeded = byteSize / state.pageSize`.
- `prefetch_helpers.cpp:495`
  `if (itB->second.pageSize != itA->second.pageSize)`
  — equality comparison of two PNS slots.

Interpretation:
- Producer (`definePlaySize`) computes `pagesNeeded` (a *count* of
  pages) and writes it into the slot through the `playSize()` alias.
- Consumers read the slot, thresholdtest it `>= 2`, and divide
  byte-sizes by it to get further page counts. The arithmetic is
  consistent only if the value is a *count of pages*, not a
  size-in-bytes-per-page.
- Both names are wrong:
  - `pageSize` suggests a per-page byte size, which it isn't.
  - `playSize` suggests a play-length, which it also isn't (the
    actual play length lives in `Node::length`).
- The most accurate name is `pagesNeeded` or `numPages`.

Judgement:
- Yes, both the field name and the alias name misdescribe the
  contents (which is a count of pages required by the waveform).

Proposals:
- `pagesNeeded`     (medium)
- `numPages`        (medium)
- `pageCount`       (low)
- keep current      (low)
- in any case: drop the `playSize()` alias

Cross-reference:
- Coupled with the `playSize() / pageSize` alias-method block below.

Locations consulted:
- declared: include/zhinst/prefetch.hpp:141,156-157
- used:     src/prefetch_prepare.cpp:768-806;
            src/prefetch_placesingle.cpp:484,616-620,642;
            src/prefetch.cpp:1188-1194,1252-1265;
            src/prefetch_helpers.cpp:495

### PrefetcherNodeState::requiredSlots / usedCache() alias  [yes / medium / coordinated-rename]

Evidence:
- `prefetch.hpp:142` `int32_t requiredSlots = 0;  // +0x20`
- `prefetch.hpp:158-159` aliases:
  ```
  int32_t& usedCache()        { return requiredSlots; }
  int32_t  usedCache() const  { return requiredSlots; }
  ```
- `prefetch.cpp:67` ctor comment:
  `// nodeStates_[root_].usedCache() = cacheSize;  // at +0x40 in hash node value`
- `prefetch.cpp:1044-1053` (in `optimize`):
  ```
  int usedCache = state.usedCache();          // 0x1cdcfd: +0x40
  ...
  nodeStates_[current].usedCache() -= used2;  // 0x1cddab
  ```
- `prefetch.cpp:1224`
  `int parentUsedCache = it->second.usedCache();`
- The field is read and written exclusively through the
  `usedCache()` alias; no use site found that reads `requiredSlots`
  by its declared name.

Interpretation:
- Every located call site uses the slot to track *how much cache is
  currently used* (initialized to cacheSize at the root, decremented
  as allocations are committed).
- The declared field name `requiredSlots` is the misnomer; the alias
  `usedCache()` faithfully describes the semantic.
- The two should be unified: rename the underlying field to
  `usedCache_` (or similar) and drop the alias.

Judgement:
- Yes, the underlying field name is misleading; the alias is correct.

Proposals:
- rename field `usedCache_`   (medium); drop alias
- rename field `cacheUsage_`  (low); drop alias
- keep current                (low)

Cross-reference:
- Couples with the `usedCache()` alias-method block (alias = correct,
  field = misnomer).

Locations consulted:
- declared: include/zhinst/prefetch.hpp:142,158-159
- used:     src/prefetch.cpp:67,1044-1053,1224;
            src/prefetch_placesingle.cpp:1044 (sites grouped under "usedCache")

### PrefetcherNodeState::lengthReg() alias / `registerHirzel`  [yes / high / coordinated-rename]

Evidence:
- `prefetch.hpp:136-137`:
  ```
  AsmRegister registerHirzel;    // +0x00
  AsmRegister registerCervino;   // +0x08
  ```
- `prefetch.hpp:152-153` aliases:
  ```
  AsmRegister& lengthReg()        { return registerHirzel; }
  AsmRegister const& lengthReg() const { return registerHirzel; }
  ```
- `prefetch.hpp:113-114` comment:
  `registerHirzel  ALSO reused as "lengthReg" for length tracking — same 8-byte slot, verified mov [rax+0x20],rcx at 0x1cb03d in definePlaySize`
- `prefetch_prepare.cpp:778-799` (in `definePlaySize`):
  ```
  AsmRegister lengthReg(regNum);
  ...
  nsIt2->second.lengthReg() = lengthReg;          // +0x20 (= registerHirzel)
  ...
  pIt2->second.lengthReg() = pIt->second.lengthReg();
  ```
- `prefetch_placesingle.cpp:147-156,205-206,285,330-388,896-909` —
  large number of sites read through `nodeStates_[node].registerHirzel`
  directly to emit Hirzel-specific instructions (`addi`, `prf`, etc.),
  not as a generic "length" register.
- `prefetch.cpp:781,2028,2114` — similar direct uses.

Interpretation:
- The same physical slot is used for two purposes: (a) the "Hirzel
  device" register on Hirzel codegen paths, (b) the "length tracking"
  register written by `definePlaySize` and read on indexed-play
  paths. These are the same register because indexed-play is itself
  a Hirzel feature; there is no semantic conflict, only an
  alias naming inconsistency.
- Most use sites favor `registerHirzel`; a small number favor
  `lengthReg`. The alias is the source of confusion.
- The `registerHirzel` name is accurate (consistent with
  `registerCervino`) and matches the larger codegen pattern.

Judgement:
- Yes, the alias `lengthReg()` misnames the slot; the underlying
  field name is correct.

Proposals:
- drop `lengthReg()` / `lengthReg() const` aliases  (high);
  consumers should use `registerHirzel` directly
- keep field name `registerHirzel`                  (high)

Cross-reference:
- Pairs with `playSize()`, `counter()`, `usedCache()` aliases — all
  legacy wrappers over the canonical fields.

Locations consulted:
- declared: include/zhinst/prefetch.hpp:136,152-153
- used:     src/prefetch_prepare.cpp:778-799;
            src/prefetch.cpp:781,2028,2114;
            src/prefetch_placesingle.cpp:147-156,205-206,285,330-388,896-909

### PrefetcherNodeState::counter() alias / `state`  [yes / high / coordinated-rename]

Evidence:
- `prefetch.hpp:138,154-155` (see PNS::state block above).
- `prefetch.cpp:1773-1777,1812-1815` use `curState.counter()` to
  feed `Cache::play(...)`'s `state` parameter.
- `prefetch.hpp:117-118` comment:
  `state ALSO consumed as "counter" — same field, verified mov [rax+0x30],0 at 0x1ce77a and mov edx,[rax+0x30] at 0x1d1153`
- No use site located that *increments* the value (which is what
  "counter" suggests); writes are assignments to enum-valued
  literals (0, 3, etc.).

Interpretation:
- The alias `counter()` was a placeholder name from before the
  field was understood as a `PointerState`. The underlying name
  `state` is the correct one.
- No semantic disagreement remains within Prefetch; only the alias
  is misleading.

Judgement:
- Yes, the alias `counter()` is misleading; field name is correct.

Proposals:
- drop `counter()` aliases               (high)
- keep field name `state`                (medium — see PNS::state
  block: cross-batch arbitration may revise this)

Cross-reference:
- Pairs with `lengthReg()`, `playSize()`, `usedCache()` aliases.
- See PNS::state cross-batch arbitration with `Cache::play::state`.

Locations consulted:
- declared: include/zhinst/prefetch.hpp:138,154-155
- used:     src/prefetch.cpp:1773-1777,1812-1815

### Prefetch::isHirzel_() / set_isHirzel_() (private aliases)  [yes / high / —]

Evidence:
- `prefetch.hpp:301-311`:
  ```
  // Forwarding accessor for legacy `isHirzel_` references in
  // placeSingleCommand. Verified at 0x1d9f65 and 0x1dabb9: cmp BYTE
  // [r15+0xbc],<imm> reads the same byte as `split_` ... The .cpp
  // uses with name `isHirzel_` were misidentified — they really
  // test split mode.
  bool isHirzel_() const { return split_; }
  void set_isHirzel_(bool v) { split_ = v; }
  ```
- No call sites in `reconstructed/` use these aliases (grep for
  `isHirzel_()` finds zero invocations under `src/`); the .cpp uses
  that originally named them have already been migrated to direct
  reads of `split_`.
- The header comment itself states the alias was created from a
  misidentification.

Interpretation:
- The accessors are unused leftovers documenting a known
  reconstruction error. They don't test "is the device Hirzel"; they
  read the splitting-mode flag.

Judgement:
- Yes, both accessor names are misnomers (and unused — pure dead
  vocabulary in the public-private API surface).

Proposals:
- delete both aliases entirely  (high) — keep the comment that
  documents the original confusion, but remove the methods

Cross-reference:
- See `Prefetch::split_` block above; see `Cache::appendMode_` /
  `AWGCompilerConfig::isHirzel` (batch 36 + the awg_compiler_config
  batch) for the actual Hirzel test path.

Locations consulted:
- declared: include/zhinst/prefetch.hpp:310-311
- used:     none in src/ (verified via grep `isHirzel_(`)

### PrefetcherNodeState::useDA  [unsure / low / —]

Evidence:
- `prefetch.hpp:145` `bool useDA = false;  // +0x38`
- `prefetch.cpp:786-789` (in load creation):
  ```
  bool useDA = waveformIR->crossesCacheLine_;  // 0x1ccd0a
  ...
  it2->second.useDA = useDA;                    // 0x1ccd37
  ```
- `prefetch_placesingle.cpp:205-206`:
  ```
  if (stateDA.useDA) {                       // 0x1d8625: +0x58 in hash_node = +0x38 in PNS
      AsmRegister regH2 = ... registerHirzel;
  }
  ```
- `prefetch.cpp:1749` "Check nodeStates_[curNode].useDA flag" comment.
- Other Prefetch use sites refer to a *separate* local
  `useDA = devConst_->hasPrecomp` (e.g. prefetch.cpp:102, 319, 521,
  561), which is *not* this PNS field — it is the global precomp
  flag from devConst_. Easy to confuse the two on casual reading.

Interpretation:
- The field is set from `WaveformIR::crossesCacheLine_`, an attribute
  of the waveform. The "DA" suffix likely stands for some
  device-architecture acronym (DA = "double-aligned"? "delayed
  acquisition"? — unclear from this batch's vantage point) that has
  nothing obviously to do with cache-line crossing.
- Given the field is read at a single Hirzel codegen site and is
  named opaquely, two things are true: (a) the value's provenance
  (`crossesCacheLine_`) suggests the more accurate name would be
  `crossesCacheLine`, and (b) the symbol clash with the unrelated
  local `useDA = hasPrecomp` is an existing reading hazard.

Judgement:
- Unsure: insufficient evidence in this batch to determine whether
  `useDA` is a faithful name from the original binary (it could be
  an authentic terminology this codebase uses for "double-aligned
  waveform" handling) or a pure reconstruction guess.

Proposals:
- keep current        (medium) — if `useDA` is original
- `crossesCacheLine`  (low) — match the source field
- `doubleAligned`     (low)

Cross-reference:
- Worth checking whether `Prefetch::*::useDA` (the local) is a
  separate bug; it shares a name with this field but is sourced
  from `devConst_->hasPrecomp`.
- Tied to `WaveformIR::crossesCacheLine_` (a different batch).

Locations consulted:
- declared: include/zhinst/prefetch.hpp:145
- used:     src/prefetch.cpp:786-789,1749;
            src/prefetch_placesingle.cpp:205-206

### PrefetcherNodeState::refTrack  [unsure / low / —]

Evidence:
- `prefetch.hpp:140` `int32_t refTrack = 0;  // +0x18`
- `prefetch.cpp:1285,1303,1335` — in `optimize`:
  ```
  if (pState.refTrack > 0) { ... }       // 0x1cecab
  nodeStates_[parent].refTrack++;        // 0x1cef3f
  nodeStates_[current].refTrack--;       // 0x1ceb11
  ```

Interpretation:
- The field is monotonically incremented and decremented and tested
  against zero — classic reference-count pattern.
- "refTrack" is semi-descriptive; "refCount" would be the
  conventional name.

Judgement:
- Unsure: the name describes the pattern (tracking refs) but is
  unconventional. May be authentic from the binary; cannot decide
  here.

Proposals:
- keep current  (medium)
- `refCount`    (low)

Locations consulted:
- declared: include/zhinst/prefetch.hpp:140
- used:     src/prefetch.cpp:1285,1294,1303,1335

### Prefetch::minIndexedSize (static)  [unsure / low / verify-not-original]

Evidence:
- `prefetch.hpp:250` `static int minIndexedSize;  // BSS at 0xb846d8`
- `nm` shows `0000000000b846d8 b zhinst::Prefetch::minIndexedSize`
  and `0000000000b846e0 b guard variable for ...minIndexedSize` —
  the symbol IS exported with this exact spelling. This is therefore
  authoritative under §3 (member data names ARE encoded in mangled
  static-member symbols).
- Used as a threshold in `prefetch_placesingle.cpp:679,776,793,811`
  to decide between indexed/non-indexed play codegen, comparing
  against `cachePtr->size_` (a byte size).

Interpretation:
- The mangled symbol carries the name `minIndexedSize`. Per §3
  (data members and static-member names are *not* normally encoded
  in member-name form — wait: static class members ARE, because
  they have addresses and external linkage). The `nm` output is the
  authoritative source.
- The compared values are byte sizes, not "indexed sizes"; the name
  could be more precisely `minIndexedCacheSize`. But given the
  symbol-table presence, this is excluded under §3 tier-1.

Judgement:
- Unsure / out-of-scope: the name appears in the binary's symbol
  table for a static data member, which §3 treats as authoritative
  for *static* member names (they have addresses and external
  linkage). Pending coordinator confirmation that §3's "data member
  names not encoded" rule is overridden by external-linkage statics.

Proposals:
- keep current  (high) — likely original

Locations consulted:
- declared: include/zhinst/prefetch.hpp:250
- used:     src/prefetch_placesingle.cpp:679,776,793,811
- binary:   nm output line `b zhinst::Prefetch::minIndexedSize`

### Prefetch::usageEntries_  [no / medium / —]

Evidence:
- `prefetch.hpp:289` `vector<UsageEntry> usageEntries_;  // +0xE0`
- `prefetch_placesingle.cpp:418` `usageEntries_.push_back(cwvfConfig_);`
- `prefetch_placesingle.cpp:452` `usageEntries_.push_back(*pc);`
- `prefetch_placesingle.cpp:1034` `usageEntries_.push_back(npSync->config);`
- `prefetch_helpers.cpp:197,215` `for (auto const& entry : usageEntries_)` —
  scans for matching PlayConfigs.

Interpretation:
- The vector accumulates `UsageEntry` records (each containing a
  `PlayConfig`) representing PlayConfigs already emitted/in-use, and
  is scanned at later sites to detect repeat usage. Name fits.

Judgement:
- No misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/prefetch.hpp:289
- used:     src/prefetch_placesingle.cpp:418,452,1034;
            src/prefetch_helpers.cpp:182,197,207,215

### Prefetch::cwvfConfig_, lastCwvfNode_, globalCwvfValid_  [no / high / —]

Evidence:
- `prefetch.hpp:269,290,291`:
  ```
  PlayConfig            cwvfConfig_;       // +0xC0
  shared_ptr<Node>      lastCwvfNode_;     // +0xF8
  bool                  globalCwvfValid_;  // +0x108
  ```
- `prefetch.cpp:115-122` (in `globalCwvf`):
  ```
  if (cwvfConfig_.channelMask == -1) {  // sentinel = uninitialized
      cwvfConfig_ = n->config;
      lastCwvfNode_ = node;
      globalCwvfValid_ = true;
      return;
  }
  ```
- `prefetch.cpp:128-145` field-by-field comparison of `cwvfConfig_`
  vs `n->config`; on mismatch sets `globalCwvfValid_ = false`.
- `prefetch_placesingle.cpp:406-418`:
  ```
  if (!globalCwvfValid_) ...
  Node* lastCwvf = lastCwvfNode_.get();
  cwvfConfig_ = lastCwvf->config;
  usageEntries_.push_back(cwvfConfig_);
  ```
- `prefetch_emit.cpp:147` `if (rawNode == root_.get() && !globalCwvfValid_)`.

Interpretation:
- The trio is a coherent "common waveform format" tracker:
  - `cwvfConfig_` holds the PlayConfig last established as the
    common one;
  - `lastCwvfNode_` holds the node it came from;
  - `globalCwvfValid_` records whether the common config is still
    valid (uniform across all seen Play nodes).
- All three names accurately describe the role.

Judgement:
- No misnomer.

Proposals:
- keep current  (high) — for all three.

Locations consulted:
- declared: include/zhinst/prefetch.hpp:269,290,291
- used:     src/prefetch.cpp:55-56,115-145;
            src/prefetch_placesingle.cpp:365,406-418,445-446;
            src/prefetch_emit.cpp:133,147

## 4. Symbols inspected and judged routinely fine

Prefetch class fields (no recordable evidence either way; all
straightforwardly typed and used as their names suggest):
- `Prefetch::config_` — `AWGCompilerConfig const*`, only read.
- `Prefetch::devConst_` — `DeviceConstants const*`, only read.
- `Prefetch::nodeStates_` — `unordered_map<shared_ptr<Node>,
  PrefetcherNodeState>`; all use sites look up node→state.
- `Prefetch::nameMap_` — `unordered_map<string, bool>`; same role
  as `Cache::allocate(5-arg)::nameMap` (eviction map). Passed to
  Cache by reference.
- `Prefetch::root_` — root tree node.
- `Prefetch::asmCommands_` — assembler-command factory.
- `Prefetch::resources_` — register-allocator handle (held as
  shared_ptr; consumers call `resources_->getRegisterNumber()`).
- `Prefetch::cache_` — the `Cache` instance owned by this prefetcher.
- `Prefetch::waveformMaps_` — vector sized to `numChannelGroups`,
  each entry is the per-group bimap of waveform-name ↔ index.
- `Prefetch::wavetableIR_` — held WavetableIR.
- `Prefetch::logFunc_` — log callback; called in `placeLoads`.
- `Prefetch::cancelCb_` — cancellation callback weak_ptr; not
  inspected in detail in this batch (no use sites surveyed).

PrefetcherNodeState fields whose name fits use:
- `registerHirzel`, `registerCervino` — Hirzel/Cervino code-path
  AsmRegisters. (See lengthReg-alias block; the slots themselves are
  fine — only the legacy alias misleads.)
- `branchCount` — written/read in countBranches as the per-node
  branch depth count. Name fits exactly.
- `cachePtr` — `shared_ptr<Cache::Pointer>` payload. Name fits.

Constructor parameters of Prefetch — all are direct initializers of
the like-named member (`config → config_`, `devConst → devConst_`,
`asmCommands → asmCommands_`, `root → root_`, `wavetableIR →
wavetableIR_`, `logFunc → logFunc_`, `cancelCb → cancelCb_`).

`UsageEntry::config` — declared `PlayConfig config;` and is the
only field; constructor copies a PlayConfig into it. Name fits.

## 5. Coverage

- **Fully covered:**
  - `prefetch.hpp` — every Prefetch class data member, every
    Prefetch ctor parameter, every PrefetcherNodeState data member
    and alias method, the two private `isHirzel_` aliases, the
    static `minIndexedSize`, the nested `UsageEntry` field.
  - In `prefetch.cpp`: ctor + the Cache::play / Cache::allocate
    boundary; sufficient survey of `globalCwvf`, `optimize`,
    `placeLoads`, `definePlaySize`, `countBranches`, and the
    `allocate` 5-arg call site to ground the cross-batch
    arbitration verdicts.

- **Deferred to part 2:**
  - **Method parameters of all Prefetch methods** other than the
    constructor. In particular:
    - `Prefetch::prepareTree::node`, `countBranches::node`,
      `definePlaySize::node`, `optimize::node`, `optimizeSync::node`,
      `optimizeCwvf::node`, `globalCwvf::node`, `allocate::node`,
      `allocate::cache`, `collectUsedWaves::node`, `linkLoad::node`,
      `removeBranches::node,stack`, `expandSetVar::node`,
      `findLockedPlay::node,waveform`, `createLoad::node`,
      `mergeLoads::node,other`, `assignLoad::node,load,flag`,
      `globalCwvf::node`, `backwardTree::node`, `sameLoads::a,b`,
      `nodeByCachePointer::ptr`, `print::node,indent`,
      `getUsedCache::node`, `moveLoadsToFront::node`,
      `getUsedWavesForDevice::deviceIdx`, `needsNewCwvf::node`,
      `splitPlay::node`, `insertPlay::list,flag,name,reg,addrA,addrB`,
      `clampToCache::addr`, `wvfImpl::reg,offset,indexed`,
      `wvfRegImpl::reg,offset,indexed`, `wvfs::type,reg,offset`,
      `placeCommands::out,node`, `placeSingleCommand::out,node`,
      `findPlaceholder::out,node`, `fillInPlaceholders::asmList`,
      `print` indent param.
  - **Helper free-function and locals review** in `prefetch_helpers.cpp`,
    `prefetch_emit.cpp`, `prefetch_print.cpp`,
    `prefetch_placesingle.cpp`, `prefetch_splitplay.cpp`, and
    `prefetch_prepare.cpp` (other than the
    `definePlaySize`/`countBranches` core already covered).
  - **Locals** flagged as obviously misleading anywhere in the seven
    .cpp files — none located in the brief skim, but a focused pass
    is needed (e.g. `useDA` shadowing between the PNS field and the
    `devConst_->hasPrecomp` local in optimize/optimizeCwvf is a
    likely candidate already noted under PNS::useDA).

  Suggested approach for part 2: split into two sub-batches if the
  context is again tight —
  - 09b: method parameters + obviously-misleading locals across all
    seven .cpp files.
  - 09c: any free functions discovered in the helpers/emit/print
    files (none confirmed in this pass — these may be method-only
    files).

- **Not covered (out of scope per RULES §2/§3):**
  - Type names `Prefetch`, `Prefetch::PrefetcherNodeState` — both
    appear in the binary's mangled symbol table; excluded under §3.
  - All Prefetch method names (`Prefetch::*`) — every one appears in
    `nm` output as a `t` symbol; excluded.
  - Template/STL parameter names, member type aliases, third-party
    headers — out of scope per §2.
  - `Prefetch::minIndexedSize` is *probably* excluded as well — the
    static-member symbol IS in the binary; flagged as
    `verify-not-original` pending coordinator decision on whether
    static-member names fall under §3 tier-1.

- **Cross-batch arbitration outcome (vs batch 36):**
  - `Cache::allocate(5-arg)::pageSize` ↔ `Prefetch::maxBranches_`:
    **Cache side is the misnomer**, Prefetch side is correct.
    Recommended rename target on the Cache side: `numBranches` or
    similar, NOT `chunkCount` / `pagesPerChunk` (those don't
    capture the branch-divisor semantic).
  - `Cache::play::state` ↔ `PrefetcherNodeState::counter()`/`state`:
    **Both partially wrong.** The PNS alias `counter()` is the
    obvious misnomer (no counter behavior at any site); the field
    itself (`state`, type `int32_t`) is technically faithful to the
    Cache enum but used in only a 2-valued way by Cache::play. The
    Cache-side parameter name is the more misleading one (it gates
    a binary marker). Recommend: drop `counter()` alias on Prefetch
    side; rename Cache parameter to e.g. `markFreed` / `playMode`.
  - `Prefetch::split_` aliasing `isHirzel_`: **`split_` is correct**;
    the `isHirzel_()`/`set_isHirzel_()` aliases should be removed
    (already unused in src/). The cache-batch's
    `Cache::appendMode_ → isHirzel_` proposal is independent — that
    field really *is* the Hirzel bit verbatim from `config.isHirzel`;
    `Prefetch::split_` is a derived flag with a wider trigger set
    (`required <= cacheSize OR appendMode()`).

- **Status:** `partial` — completed Prefetch class members, ctor
  params, PrefetcherNodeState members and aliases, and all
  cross-batch arbitrations vs batch 36. Method parameters,
  free-function parameters in the helper/emit/print/placesingle/
  splitplay files, and an explicit locals sweep are deferred to
  part 2 as outlined above.
