# Batch 09 — prefetch (part 2)

Continuation of `09_prefetch.md`. Part 1 covered all `Prefetch` and
`PrefetcherNodeState` data members, the four PNS legacy alias methods,
the two `Prefetch::isHirzel_()`/`set_isHirzel_()` private aliases,
the static `minIndexedSize`, ctor parameters, the nested `UsageEntry`
field, and three cross-batch arbitrations vs batch 36
(`maxBranches_`, `split_`, `state`).

This part covers:
- Final verdict on `Prefetch::minIndexedSize` under the §3 clarification.
- Method parameters of `Prefetch` methods (cross-cutting first, then
  per-method).
- The single file-scope free function in the helper files
  (`computeWaveformMemoryBytes`).
- A focused locals sweep, in particular the `useDA` clash flagged by
  part 1.

## 1. Files considered

- `reconstructed/include/zhinst/prefetch.hpp`
- `reconstructed/src/prefetch.cpp`
- `reconstructed/src/prefetch_helpers.cpp`
- `reconstructed/src/prefetch_emit.cpp`
- `reconstructed/src/prefetch_print.cpp`
- `reconstructed/src/prefetch_prepare.cpp`
- `reconstructed/src/prefetch_placesingle.cpp`
- `reconstructed/src/prefetch_splitplay.cpp`

Binary symbol table re-consulted via
`nm --demangle _seqc_compiler.so | grep Prefetch::`. Confirms
under the part-1-clarified §3:
- `0000000000b846d8 b zhinst::Prefetch::minIndexedSize` is a static
  data member exported as a `b` symbol — **tier-1 excluded** under §3.

## 2. Overview

Cross-cutting parameter findings live in §2A; per-method parameters
in §2B; free functions in §2C; locals in §2D.

### 2A. Cross-cutting parameter survey

The recurring parameters across the ~30 `Prefetch` methods cluster
into a small set:

| Parameter pattern | Type | Methods | Verdict |
|---|---|---|---|
| `node` | `shared_ptr<Node>` | almost every method | `no` / high — universally accurate |
| `out` | `AsmList*` | `placeCommands`, `placeSingleCommand` | `no` / medium — output sink |
| `asmList` | `AsmList*` (header `out`) | `findPlaceholder` (cpp body) | `no` / medium — header/cpp param-name disagreement only |
| `reg` | `AsmRegister` | `wvfImpl`, `wvfRegImpl`, `wvfs`, `insertPlay` | `no` / high — destination register |
| `offset` | `int` / `AsmRegister` | `wvfImpl`, `wvfRegImpl`, `wvfs` | `no` / high — byte/word offset for instruction |
| `indexed` | `bool` | `wvfImpl`, `wvfRegImpl` | `no` / high — picks `wvfi` vs `wvf` |
| `addr` | `AddressImpl<uint32_t>` | `clampToCache` | `no` / high |
| `cache` | `shared_ptr<Cache>` | `allocate` | `no` / high |
| `addrA`, `addrB` | `AddressImpl<uint32_t>` | `insertPlay` | `unsure` / low — two unnamed addresses |
| `flag` | `bool` | `assignLoad`, `insertPlay` | **see §3** — both are misnomers |

### 2B. Per-method parameter overview

Only rows that warrant comment are shown; routine `no`/high rows are
listed bullet-style in §4.

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `Prefetch::assignLoad::flag` | yes | high | always called as `isHirzel` | `isHirzel`, `useHirzelRegister` | — |
| `Prefetch::insertPlay::flag` | yes | high | always called as `indexed` | `indexed` | — |
| `Prefetch::insertPlay::addrA` | unsure | low | "first half size / addr" | `firstAddr`, keep | — |
| `Prefetch::insertPlay::addrB` | unsure | low | "second / cache hash" | `secondAddr`, `xnorMask`, keep | — |
| `Prefetch::mergeLoads::dst` / `src` | no | high | header had `node`/`other`; cpp names accurate | keep current (cpp form) | — |
| `Prefetch::findPlaceholder::out`/`asmList` | unsure | low | header vs cpp disagree | unify on `asmList` | — |
| `Prefetch::print::indent` | no | high | passed to `cout.width(indent)` | keep current | — |
| `Prefetch::print::node` | no | high | falls back to `root_` if null | keep current | — |
| `Prefetch::removeBranches::stack` | no | high | `std::stack&`, push children | keep current | — |
| `Prefetch::sameLoads::a, b` | no | medium | symmetric comparison | keep current | — |
| `Prefetch::nodeByCachePointer::ptr` | no | high | `shared_ptr<Cache::Pointer>` | keep current | — |
| `Prefetch::wvfs::playDummyType` | no | high | type-typed enum | keep current | — |
| `Prefetch::getUsedWavesForDevice::deviceIdx` | no | high | indexes `waveformMaps_` | keep current | — |
| `Prefetch::findLockedPlay::waveform` | no | high | `shared_ptr<WaveformIR>` | keep current | — |
| `Prefetch::allocate::cache` | no | high | the `Cache` instance | keep current | — |

### 2C. Free functions

| Symbol | Misnomer? | Conf | Justification | Proposal(s) | Status |
|---|---|---|---|---|---|
| `computeWaveformMemoryBytes::wfm` | no | high | `WaveformIR const*`, the only input | keep current | — |

The only file-scope free function found across the six .cpp files is
`computeWaveformMemoryBytes` in `prefetch_emit.cpp:225` (declared
`static int`). All other top-level definitions in those files are
`Prefetch::*` methods. Part 1's hypothesis that the helper files are
method-only is therefore essentially correct; only this one helper
exists, and it has a single param that is not misleading.

### 2D. Locals

| Symbol | Misnomer? | Conf | Justification | Proposal(s) | Status |
|---|---|---|---|---|---|
| `Prefetch::optimize::useDA` (and 3 sibling locals) | no | medium | semantic role matches PNS field | keep current | — |
| `Prefetch::createLoad::useDA` (the `crossesCacheLine_` local) | no | medium | direct field assignment | keep current | — |

## 3. Detailed findings

### Prefetch::minIndexedSize (static)  [no / high / not-misnomer]

(Closes the part-1 `verify-not-original` flag.)

Evidence:
- `prefetch.hpp:250` `static int minIndexedSize;  // BSS at 0xb846d8`
- `nm --demangle _seqc_compiler.so` output:
  `0000000000b846d8 b zhinst::Prefetch::minIndexedSize`
  `0000000000b846e0 b guard variable for ...minIndexedSize`
- RULES §3 (post-clarification):
  > Static data members are tier-1 authoritative. … If `nm
  > --demangle` shows the static member as a data symbol, treat it
  > as **excluded** from rename, the same way a method or
  > free-function name is.

Interpretation:
- The mangled symbol `_ZN6zhinst8Prefetch15minIndexedSizeE` carries
  the spelling `minIndexedSize` verbatim; this is original.

Judgement:
- Not a misnomer (out of scope under §3 tier-1).

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/prefetch.hpp:250
- binary:   nm output line `b zhinst::Prefetch::minIndexedSize`

### Prefetch::assignLoad::flag  [yes / high / —]

Evidence:
- Definition `prefetch.cpp:2005-2007`:
  ```
  void Prefetch::assignLoad(std::shared_ptr<Node> node,
                            std::shared_ptr<Node> const& load,
                            bool flag) // 0x1d53a0
  ```
- Body at `prefetch.cpp:2021-2038` branches on `flag`:
  ```
  if (flag) {
      // isHirzel path
      … it->second.registerHirzel = …  // +0x20
  } else {
      // Cervino path
      … it->second.registerCervino = …  // +0x28
  }
  ```
- Every located call site passes the `isHirzel` predicate:
  - `prefetch.cpp:1362` `assignLoad(cloned, current, isHirzel);`
  - `prefetch.cpp:2122` `assignLoad(node, result, isHirzel);`
  - `prefetch.cpp:2185` `assignLoad(targetNode, dst, config_->isHirzel);`
- Inline comments at the call sites reinforce the meaning:
  `prefetch.cpp:1002` "calls `assignLoad(clone, currentNode,
  config_->isHirzel)`", `prefetch.cpp:1360`
  "`assignLoad(cloned, current, config_->isHirzel)`".

Interpretation:
- The argument is uniformly the `isHirzel` device-config bit at every
  located call site, and is used inside the body to select between
  the Hirzel-register slot (`registerHirzel`, +0x20) and the
  Cervino-register slot (`registerCervino`, +0x28).
- The name `flag` carries no information about which of the two
  device-family code paths the argument selects.

Judgement:
- Yes, `flag` is a generic placeholder name that obscures a
  device-family selector identical to `config_->isHirzel`.

Proposals:
- `isHirzel`              (high)
- `useHirzelRegister`     (medium)
- keep current            (low)

Locations consulted:
- declared/defined: src/prefetch.cpp:2005-2038
- called from:      src/prefetch.cpp:1362, 2122, 2185

### Prefetch::insertPlay::flag  [yes / high / —]

Evidence:
- Definition `prefetch_emit.cpp:714-741`:
  ```
  void Prefetch::insertPlay(AsmList& list, bool flag,
                            std::string const& name, AsmRegister reg,
                            detail::AddressImpl<uint32_t> addrA,
                            detail::AddressImpl<uint32_t> addrB) const
  …
      AsmList wvfInstructions = wvfImpl(reg, addrA, flag);  // 0x1df056
  ```
  Only use of `flag` inside the body is to pass it as the third
  argument of `wvfImpl(AsmRegister reg, int offset, bool indexed)`
  (definition at `prefetch_emit.cpp:640`).
- Both call sites in `prefetch_splitplay.cpp` pass an `indexed`
  variable:
  - line 328: `insertPlay(result, indexed, playLabel, playReg, halfSize, cacheHash);`
  - line 411: `insertPlay(result, indexed2, lastLabel, playReg2, remainder, position);`

Interpretation:
- The argument is the same `indexed` predicate as `wvfImpl::indexed`
  (selects `wvfi` vs `wvf`). Naming it `flag` here loses the type/role
  information that is preserved at every other layer of the call chain.

Judgement:
- Yes, `flag` is a misnomer; the same boolean is named `indexed`
  immediately above (call sites) and immediately below (`wvfImpl`).

Proposals:
- `indexed`     (high)
- keep current  (low)

Locations consulted:
- declared/defined: src/prefetch_emit.cpp:714-741
- called from:      src/prefetch_splitplay.cpp:328, 411
- forwarded into:   src/prefetch_emit.cpp:640 (`wvfImpl::indexed`)

### Prefetch::insertPlay::addrA / addrB  [unsure / low / —]

Evidence:
- Definition `prefetch_emit.cpp:714-741` uses `addrA` only as the
  `offset` argument to `wvfImpl`, and `addrB` only as the immediate
  in `xnori(reg, reg, Immediate(addrB))` on the non-Hirzel branch.
- Call site `prefetch_splitplay.cpp:328`:
  `insertPlay(result, indexed, playLabel, playReg, halfSize, cacheHash);`
- Call site `prefetch_splitplay.cpp:411`:
  `insertPlay(result, indexed2, lastLabel, playReg2, remainder, position);`
- The two call sites use semantically different pairs:
  - `(halfSize, cacheHash)` — first half play
  - `(remainder, position)` — last play in the split sequence

Interpretation:
- Across the two call sites the first-position argument is "size of
  this fragment" and the second is "cache-related immediate
  (hash / position)". The body uses position 1 as a wave-buffer offset
  and position 2 as the XNOR mask; both are addresses but with
  distinct semantic roles.
- The `addrA`/`addrB` names are generic A/B labels; `wvfOffset` and
  `xnorMask` would be more direct, but the source diversity at call
  sites (size, hash, remainder, position) makes a single faithful
  name hard to choose.

Judgement:
- Unsure: the names are uninformative but no single replacement is
  obviously better at every call site, and there is no he-said/she-said
  inconsistency to anchor a verdict.

Proposals:
- keep current             (medium)
- `wvfOffset`/`xnorMask`   (low) — most descriptive of body usage
- `firstAddr`/`secondAddr` (low) — least committal

Locations consulted:
- declared/defined: src/prefetch_emit.cpp:714-741
- called from:      src/prefetch_splitplay.cpp:328, 411

### Prefetch::findPlaceholder — header `out` vs cpp `asmList`  [unsure / low / —]

Evidence:
- Header `prefetch.hpp:220-221`:
  `AsmList::Asm* findPlaceholder(AsmList* out, std::shared_ptr<Node> node);`
- Cpp definition `prefetch_emit.cpp:95-96`:
  `AsmList::Asm* Prefetch::findPlaceholder(AsmList* asmList, std::shared_ptr<Node> node)`
- Body at `prefetch_emit.cpp:99-103` does a linear scan over `*asmList`
  for an entry with matching `sequenceId`; it does not write into the
  list.

Interpretation:
- The function reads from the list (no output written through it).
  `out` is misleading because it suggests an output sink.
  `asmList` is more accurate for this read-only-scan role.

Judgement:
- Header param name `out` is mildly misleading; the cpp name `asmList`
  is more accurate. The disagreement itself is also a code-hygiene
  issue.

Proposals:
- align both on `asmList`  (medium)
- keep header `out`        (low)

Locations consulted:
- declared: include/zhinst/prefetch.hpp:220
- defined:  src/prefetch_emit.cpp:95-108

### Local: `useDA` clash in optimize / optimizeCwvf / createLoad  [no / medium / —]

(Focused sweep requested by part 1.)

Evidence:
- PNS field `prefetch.hpp:145`: `bool useDA = false;`
- Local in `globalCwvf` body, `prefetch.cpp:102`:
  `bool useDA = devConst_->/*+0x88*/ hasPrecomp;`
  used at `prefetch.cpp:109` `if (useDA & 0x1) return;`
- Local in `optimizeCwvf`, `prefetch.cpp:319`:
  `bool useDA = devConst_->hasPrecomp;` used at
  `prefetch.cpp:324` `if (useDA) {`
- Local in `optimize` (Play case), `prefetch.cpp:521`:
  `bool useDA = devConst_->hasPrecomp;` used at line 525.
- Local in `optimize` (Loop-Play case), `prefetch.cpp:561`:
  `bool useDA = devConst_->hasPrecomp;` used at line 566.
- Local in `createLoad`-area code, `prefetch.cpp:786`:
  `bool useDA = waveformIR->crossesCacheLine_;`
  immediately followed by `it2->second.useDA = useDA;` (line 789),
  i.e. a pure rename for the field assignment.
- Inline-comment context (`prefetch.cpp:230, 318`) describes all the
  optimizer-side conditions as "useDA = the DA / precomp eligibility
  bit".

Interpretation:
- Two distinct value sources feed all "useDA" symbols:
  (a) `devConst_->hasPrecomp` — a per-device global "DA precompensation
      is enabled" bit, used in the optimizer eligibility checks; and
  (b) `WaveformIR::crossesCacheLine_` — a per-waveform "this
      waveform's data crosses a DA cache line" bit, copied into the
      PNS field to mark per-node DA handling.
- Both ultimately feed Hirzel-style "double-aligned / DA path"
  codegen. The local `useDA` names in `optimize`/`optimizeCwvf`
  consistently mean "is the DA precomp feature available on this
  device?", and the local at line 786 is a one-line stand-in for the
  field write. The four optimizer-side locals are scoped to their
  respective `case` blocks and never coexist with an in-scope `PNS`
  binding named `useDA`, so there is no actual shadowing bug.
- The clash with the PNS field is a *reading* hazard, not a semantic
  one: a careless reader can confuse `state.useDA` (PNS field, bound
  at line 786 / read at line 1749) with the unrelated locals. Both
  carry the same conceptual role ("DA path active for this
  thing") so the names are not actually wrong.

Judgement:
- Not a misnomer. The local name is consistent with the field name
  and the role both serve in DA / precomp gating. (Renaming to e.g.
  `precompEnabled` for the locals and `crossesCacheLine` for the
  field would aid readability, but this falls under "obviously
  misleading" only if one accepts that name-overlap with a class
  member is per se misleading — RULES §2 sets a higher bar than
  that.)

Proposals (locals):
- keep current                         (medium)
- rename four optimizer locals to `precompEnabled` (low)

Cross-reference:
- See part 1 PNS::useDA block (`unsure / low`); the field-side
  question of whether `useDA` is original or a recon guess is
  unaffected by this finding.

Locations consulted:
- declared: include/zhinst/prefetch.hpp:145 (field)
- locals:   src/prefetch.cpp:102, 319, 521, 561, 786

## 4. Symbols inspected and judged routinely fine

Per-method parameters, organised by method (each `node` parameter is
`shared_ptr<Node>` and used as the input tree-node argument; not
re-justified for each method):

- `Prefetch::globalCwvf::node`,
  `Prefetch::optimizeSync::node`,
  `Prefetch::optimizeCwvf::node`,
  `Prefetch::optimize::node`,
  `Prefetch::moveLoadsToFront::node`,
  `Prefetch::linkLoad::node`,
  `Prefetch::createLoad::node`,
  `Prefetch::collectUsedWaves::node`,
  `Prefetch::backwardTree::node`,
  `Prefetch::expandSetVar::node`,
  `Prefetch::findLockedPlay::node`,
  `Prefetch::needsNewCwvf::node`,
  `Prefetch::splitPlay::node`,
  `Prefetch::getUsedCache::node`,
  `Prefetch::countBranches::node`,
  `Prefetch::definePlaySize::node`,
  `Prefetch::prepareTree::node`,
  `Prefetch::placeCommands::node`,
  `Prefetch::placeSingleCommand::node`,
  `Prefetch::removeBranches::node`,
  `Prefetch::assignLoad::node`,
  `Prefetch::print::node` — all standard.
- `Prefetch::removeBranches::stack` — `std::stack&`, used only with
  `push()`. Name fits.
- `Prefetch::findLockedPlay::waveform` — `shared_ptr<WaveformIR>`
  searched for. Name fits.
- `Prefetch::assignLoad::load` — the load node being assigned. Fits.
- `Prefetch::mergeLoads::dst`, `src` — merge target/source Load nodes,
  named accurately in the .cpp (header was `node`/`other`).
- `Prefetch::allocate::cache` — `shared_ptr<Cache>`. Fits.
- `Prefetch::sameLoads::a`, `b` — symmetric comparison; A/B is
  acceptable for symmetric predicates.
- `Prefetch::nodeByCachePointer::ptr` — `shared_ptr<Cache::Pointer>`.
  Fits.
- `Prefetch::clampToCache::addr` — `AddressImpl<uint32_t>` clamped
  against cache bounds. Fits.
- `Prefetch::wvfImpl::reg, offset, indexed`,
  `Prefetch::wvfRegImpl::reg, offset, indexed`,
  `Prefetch::wvfs::playDummyType, reg, offset` — all faithful to
  the underlying `AsmCommands::wvf*` signatures.
- `Prefetch::insertPlay::list, name, reg` — the output AsmList,
  the asm label string, and the destination register. All accurate.
  (`flag`, `addrA`, `addrB` are flagged separately above.)
- `Prefetch::placeCommands::out`,
  `Prefetch::placeSingleCommand::out` — `AsmList*` written-into.
  Name fits the actual write/read pattern.
- `Prefetch::fillInPlaceholders::asmList` — the input AsmList copied
  into the new output. Fits.
- `Prefetch::print::indent` — column width for `cout.width(indent)`.
  Fits.
- `Prefetch::getUsedWavesForDevice::deviceIdx` — `size_t`, indexes
  `waveformMaps_`. Fits.
- `Prefetch::findPlaceholder::node` — `shared_ptr<Node>` whose
  `asmId` is searched for. Fits.

Free function:
- `computeWaveformMemoryBytes::wfm` (`prefetch_emit.cpp:225`) —
  `const WaveformIR*`, sole input, used to read channels/length/dc.
  Fits.

Locals worth a one-line mention (looked at, not flagged):
- `Prefetch::optimize`-block locals `useDA`, `hasDummyOrHold`,
  `isDummy`, `cwvf`, `globalRate`, `defaultPrecompFlags` —
  descriptive of their role; no misnomer.
- `Prefetch::sameLoads` locals `aWave`, `bWave`, `aDevIdx`, `bDevIdx`,
  `regsMatch`, `aHas`/`bHas` — all routine.
- `Prefetch::splitPlay` locals `totalLength`, `wfIR`, `devIdx`,
  `waveName`, `wfm`, `channels`, `numRepeats` — all routine.
- `Prefetch::wvfs` locals `regIsValid`, `tempReg`, `addiResult`,
  `lastDst`, `width`/`depth`/`auxW`/`rounded`/`totalBits`/
  `totalBytes` — all routine. The `width`/`depth`/`auxW` triple is
  borderline generic but is a faithful reading of
  `waveformGranularity` / `waveformPageSize` / `bitsPerSample`,
  and changing it would chase a `DeviceConstants`-side question
  that is out of this batch's scope.

## 5. Coverage

- **Fully covered:**
  - The `verify-not-original` flag from part 1 on
    `Prefetch::minIndexedSize` (resolved: tier-1 excluded).
  - All explicit method parameters of the ~30 `Prefetch` methods,
    including the dense ones (`placeSingleCommand`, `optimize`,
    `optimizeCwvf`, `allocate`, `splitPlay`, `insertPlay`, `wvfImpl`,
    `wvfRegImpl`, `wvfs`, `assignLoad`, `mergeLoads`, `print`,
    `findPlaceholder`).
  - File-scope free functions in
    `prefetch_helpers.cpp`/`emit.cpp`/`print.cpp`/`prepare.cpp`/
    `placesingle.cpp`/`splitplay.cpp` — there is exactly one
    (`computeWaveformMemoryBytes`); confirmed not-misnomer.
  - The focused `useDA` local-clash sweep called for in part 1.
  - Briefly inspected and routinely-fine locals across the surveyed
    methods (listed in §4).

  Agrees with part 1 on:
  - `Prefetch::isHirzel_`/`split_` understanding (the `flag` parameter
    of `assignLoad` is yet another instance of the same Hirzel-vs-not
    selector that part 1's `split_` analysis identified).
  - PNS::useDA being a "DA path" marker; this part adds that the
    homonymous locals do **not** constitute a separate bug.

- **Deferred to part 3 (if any):**
  - A *systematic* locals sweep across all six .cpp files
    (this part only chased the `useDA` clash plus locals
    encountered while reviewing parameters). No additional
    obviously-misleading locals were spotted in passing, but a
    full read of `placeSingleCommand` (1130 lines) and
    `splitPlay` (426 lines) was not done line-by-line and may
    yield more.
  - Cross-file consistency check between header parameter names
    and cpp-body parameter names beyond `findPlaceholder::out`
    /`asmList` and `mergeLoads::node,other` /`dst,src` (both
    flagged here). A header-vs-cpp diff for *all* methods may
    surface more.

- **Not covered (out of scope per RULES §2/§3):**
  - All `Prefetch::*` method names — confirmed in `nm` output as
    `t` symbols, excluded.
  - Type names `Prefetch`, `Prefetch::PrefetcherNodeState` — in
    `nm`, excluded.
  - Static data member `Prefetch::minIndexedSize` — in `nm` as a
    `b` symbol, tier-1 excluded under §3 clarification.
  - Template/STL parameter names, member type aliases (`WaveformBimap`),
    third-party (boost::bimaps) names.

- **Cross-batch arbitration outcomes added by this part:**
  - None new. The two new `flag`-parameter findings
    (`assignLoad::flag` = `isHirzel`, `insertPlay::flag` = `indexed`)
    are within-batch and resolved here. They reinforce part 1's
    conclusion that the Cache/Prefetch boundary uses many
    Hirzel-vs-Cervino selectors that are sometimes named after the
    flag's *source* (`isHirzel`) and sometimes after its *role*
    (`split_`, `appendMode_`); standardising on one would be a
    synthesis-time decision.

- **Status:** `complete` for the deferred items defined by part 1's
  §5. Only optional follow-ups (full line-by-line locals sweep of
  `placeSingleCommand`/`splitPlay`, header-vs-cpp parameter-name
  diff for the whole class) are uncovered, and they are *speculative*
  rather than known-deferred work.
