# Batch 53 — wave_index_tracker

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 4 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 4;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/waveform/wave_index_tracker.hpp`
- `reconstructed/src/waveform/wave_index_tracker.cpp`

Symbol-table check (excluded from rename per RULES §3, tier-1):

```
nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so
  zhinst::WaveIndexTracker::WaveIndexTracker(int)                            (0x29a5e0)
  zhinst::WaveIndexTracker::WaveIndexTracker<WaveformFront>(int, ...)        (0x29d000)
  zhinst::WaveIndexTracker::WaveIndexTracker<WaveformIR>(int, ...)           (0x29d410)
  zhinst::WaveIndexTracker::assign(int)                                      (0x29a600)
  zhinst::WaveIndexTracker::assignAuto(int)                                  (0x29a620)
  zhinst::WaveIndexTracker::usedWaveIndices() const                          (0x29a7d0)
  zhinst::WaveIndexTracker::getNextAutoIndex()                               (0x29a880)
  zhinst::WaveIndexTracker::hasGaps()                                        (0x29a8e0)
  zhinst::WavetableException::WavetableException(std::string const&)         (0x29a7e0)
  zhinst::WavetableException::~WavetableException()                          (0x29a840 / 0x29f980)
  zhinst::WavetableException::what() const                                   (0x29f9d0)
  typeinfo for zhinst::WavetableException                                    (0xb03368)
  vtable    for zhinst::WavetableException                                   (0xb078d0)
```

→ Type names `WaveIndexTracker`, `WavetableException` and all listed
method names are out of scope. Parameters and instance data members
remain in scope.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `WaveIndexTracker::maxIndex` (field) | unsure | low | naming-convention gap, semantics OK | `maxIndex_`, keep current | — |
| `WaveIndexTracker::indices_` (field) | unsure | low | accessor names it "WaveIndices" | `usedWaveIndices_`, `usedIndices_`, keep current | — |
| `WaveIndexTracker::autoIndex_` (field) | no | medium | matches usage everywhere | keep current | not-misnomer |
| `WaveIndexTracker::WaveIndexTracker(int)::maxIdx` (param) | unsure | low | shadows-style mismatch with member | `maxIndex`, keep current | — |
| `WaveIndexTracker::WaveIndexTracker<T>(int,…)::maxIdx` (param) | unsure | low | same as 1-arg ctor | `maxIndex`, keep current | — |
| `WaveIndexTracker::WaveIndexTracker<T>(int,…)::mgr` (param) | no | medium | clearly the manager | keep current | — |
| `WaveIndexTracker::WaveIndexTracker<T>(int,…)::wp` (local) | no | low | obvious loop var | keep current | — |
| `WaveIndexTracker::WaveIndexTracker<T>(int,…)::idx` (local) | no | low | obvious | keep current | — |
| `WaveIndexTracker::assign(int)::index` (param) | no | high | matches role exactly | keep current | not-misnomer |
| `WaveIndexTracker::assignAuto(int)::index` (param) | no | high | matches role exactly | keep current | not-misnomer |
| `WaveIndexTracker::assignAuto::it` (local) | no | low | obvious iterator | keep current | — |
| `WaveIndexTracker::hasGaps::it` (local) | no | low | obvious iterator | keep current | — |
| `WavetableException::message_` (field) | no | medium | matches usage (returned by `what()`) | keep current | not-misnomer |
| `WavetableException::WavetableException::msg` (param) | no | medium | matches usage | keep current | — |

(All method names and the type names `WaveIndexTracker`,
`WavetableException` are excluded per §3 — see §5.)

## 3. Detailed findings

### WaveIndexTracker::maxIndex (field)  [unsure / low / —]

Evidence:
- include/zhinst/waveform/wave_index_tracker.hpp:84  `int maxIndex;                   // +0x00`
- src/waveform/wave_index_tracker.cpp:55-56  ctor uses param `maxIdx` and inits
  `: maxIndex(maxIdx)` — note no trailing underscore on the member
  while the two siblings are `indices_` and `autoIndex_`.
- src/waveform/wave_index_tracker.cpp:88  `if (index >= maxIndex)` — used as an
  exclusive upper bound on inserted indices.
- src/core/error_messages.cpp:385  `m[250] = "waveform index exceeds
  wavetable size"` — error thrown when `index >= maxIndex` (per
  wave_index_tracker.cpp:88-91 and `ErrorMessageT(0xFA)=250`).
- notes/symbol-renaming-audit/31_device_constants.md:47  the value
  passed in is `DeviceConstants::maxWaveIndex()` (kept as-is in batch
  31).
- notes/struct_layouts.md:472  internal layout note already calls it
  `maxIndex_` with trailing underscore.

Interpretation:
- Semantic name "maxIndex" is consistent with usage and with the
  caller-side name `maxWaveIndex()`. The error string ("exceeds
  wavetable size") could equally read as either "maximum index" or
  "size", but the comparison is `index >= maxIndex`, i.e. an
  exclusive upper bound — same arithmetic as a size.
- Only structural oddity: missing trailing underscore that the two
  sibling members (`indices_`, `autoIndex_`) carry; the layout note
  already uses `maxIndex_`. This is convention, not a misnomer.

Judgement:
- Name is not misleading; only inconsistent with the trailing-`_`
  convention used by the other members.

Proposals:
- `maxIndex_`     (low) — pure convention alignment
- keep current   (medium)

Locations consulted:
- declared: include/zhinst/waveform/wave_index_tracker.hpp:84
- used:     src/waveform/wave_index_tracker.cpp:56,88
- callers:  31_device_constants.md (assignment site)

### WaveIndexTracker::indices_ (field)  [unsure / low / —]

Evidence:
- include/zhinst/waveform/wave_index_tracker.hpp:86  `std::set<int> indices_;`
- include/zhinst/waveform/wave_index_tracker.hpp:103  public accessor is
  `const std::set<int>& usedWaveIndices() const;`
- src/waveform/wave_index_tracker.cpp:103-106  `usedWaveIndices()` returns
  `indices_`.
- src/waveform/wave_index_tracker.cpp:80-95  `assignAuto` calls
  `indices_.find(index)`, `indices_.insert(index)` — set acts as the
  registry of *already-assigned* indices.
- src/waveform/wavetable_ir.cpp:507  external code peeks the set as
  `waveIndexTracker_.indices_` — same name used by client code.

Interpretation:
- The set holds the set of wave indices that have been assigned /
  are already in use; the public accessor name spells this out
  ("usedWaveIndices"). The field name `indices_` is generic and
  drops both "used" and "wave" qualifiers that the accessor and the
  enclosing class already carry. The accessor name (in the binary
  symbol table — excluded from rename) is the strongest evidence of
  what the field is.

Judgement:
- Name is technically accurate but loses the "used" qualifier the
  accessor uses; not misleading on its own (the class context
  already says "WaveIndex"), but a more specific name would match
  the accessor.

Proposals:
- `usedWaveIndices_`  (low) — match accessor name
- `usedIndices_`      (low) — class context provides "Wave"
- keep current        (medium)

Locations consulted:
- declared: include/zhinst/waveform/wave_index_tracker.hpp:86
- used:     src/waveform/wave_index_tracker.cpp:80,88,95,105,114,125,129
- ext use:  src/waveform/wavetable_ir.cpp:507

### WaveIndexTracker::autoIndex_ (field)  [no / medium / not-misnomer]

Evidence:
- include/zhinst/waveform/wave_index_tracker.hpp:87  `int autoIndex_;`
- src/waveform/wave_index_tracker.cpp:67  `autoIndex_ = 0;` inside `assign()`,
  i.e. assigning a specific index resets the auto-cursor.
- src/waveform/wave_index_tracker.cpp:114-116  `getNextAutoIndex()` advances
  `autoIndex_` past every entry already present in the set.
- src/waveform/wave_index_tracker.cpp:131  `hasGaps()` compares
  `autoIndex_ < *it` (max used index).
- src/waveform/wavetable_ir.cpp:508,516,531,556,561,569  external code uses
  `waveIndexTracker_.autoIndex_` as the running auto-assign cursor
  while filling in unused indices.

Interpretation:
- The field is consistently the "next index to hand out
  automatically" — it is read as a candidate, advanced past
  occupied positions, and reset when an explicit assignment
  happens. The name describes that role precisely, and the public
  method `getNextAutoIndex()` (excluded from rename) corroborates
  the same vocabulary.

Judgement:
- Name accurately describes the role.

Proposals:
- keep current   (high)

Locations consulted:
- declared: include/zhinst/waveform/wave_index_tracker.hpp:87
- used:     src/waveform/wave_index_tracker.cpp:54,58,67,112,114-116,129-131
- ext use:  src/waveform/wavetable_ir.cpp:508,516,531,556,561,569

### WaveIndexTracker::WaveIndexTracker(int)::maxIdx (param)  [unsure / low / —]

Evidence:
- src/waveform/wave_index_tracker.cpp:55  `WaveIndexTracker(int maxIdx)`
- src/waveform/wave_index_tracker.cpp:56  `: maxIndex(maxIdx)`
- include/zhinst/waveform/wave_index_tracker.hpp:90  declaration uses the
  spelling `int maxIndex` (different from the cpp param spelling).

Interpretation:
- The header and cpp disagree on the spelling (`maxIndex` vs
  `maxIdx`). Both convey the same meaning. Picking one — and
  matching the (also flagged) member naming choice — is purely a
  style / consistency decision.

Judgement:
- Not misleading; only inconsistent across header and cpp, and
  abbreviates "Index" for no apparent reason.

Proposals:
- `maxIndex`     (medium) — match the header spelling and member
- keep current   (low)

Locations consulted:
- declared: include/zhinst/waveform/wave_index_tracker.hpp:90
- defined:  src/waveform/wave_index_tracker.cpp:55-60

### WaveIndexTracker::WaveIndexTracker<T>(int, WavetableManager<T> const&)::maxIdx (param)  [unsure / low / —]

Evidence:
- src/waveform/wave_index_tracker.cpp:148-152  template ctor parameter named
  `maxIdx`, member init `: maxIndex(maxIdx)`.
- include/zhinst/waveform/wave_index_tracker.hpp:93-94  declaration uses
  `int maxIndex`.

Interpretation:
- Same situation as the 1-arg ctor parameter immediately above; the
  two ctors should agree.

Judgement:
- Not misleading; pure consistency issue.

Proposals:
- `maxIndex`    (medium) — match header spelling and 1-arg ctor
- keep current  (low)

Locations consulted:
- declared: include/zhinst/waveform/wave_index_tracker.hpp:93-94
- defined:  src/waveform/wave_index_tracker.cpp:147-163

### WavetableException::message_ (field)  [no / medium / not-misnomer]

Evidence:
- include/zhinst/waveform/wave_index_tracker.hpp:125  `std::string message_;`
- src/waveform/wave_index_tracker.cpp:18-19  ctor `: message_(msg)`.
- src/waveform/wave_index_tracker.cpp:35-41  `what()` returns
  `message_.c_str()` (or `""` if empty).

Interpretation:
- The field stores the human-readable exception message that
  `what()` exposes; that is exactly what the name says.

Judgement:
- Name matches role.

Proposals:
- keep current   (high)

Locations consulted:
- declared: include/zhinst/waveform/wave_index_tracker.hpp:125
- used:     src/waveform/wave_index_tracker.cpp:19,37,40

### WaveIndexTracker::assign(int)::index, assignAuto(int)::index (params)  [no / high / not-misnomer]

Evidence:
- src/waveform/wave_index_tracker.cpp:65-69  `void assign(int index)` resets
  `autoIndex_=0` then forwards to `assignAuto(index)`.
- src/waveform/wave_index_tracker.cpp:77-97  `int assignAuto(int index)`
  checks for duplicate, checks `index >= maxIndex`, then
  `indices_.insert(index)` and returns `index`.
- src/waveform/wavetable_ir.cpp:556  caller `waveIndexTracker_.assignAuto(autoIdx)`
  passes the integer wave index it wants registered.
- error_messages.cpp:384-385  the two thrown errors are
  "waveform index already used" / "waveform index exceeds wavetable
  size" — both speak about *the index being assigned*.

Interpretation:
- Despite the method name `assignAuto`, the parameter is the
  *explicit* index the caller wants to claim; both methods take a
  concrete index value and either record it or throw. The name
  `index` describes that value precisely.

Judgement:
- Parameter name accurately describes the value.

Proposals:
- keep current   (high)

Locations consulted:
- declared: include/zhinst/waveform/wave_index_tracker.hpp:97,100
- defined:  src/waveform/wave_index_tracker.cpp:65-69, 77-97
- callers:  src/waveform/wavetable_ir.cpp:556

## 4. Symbols inspected and judged routinely fine

- `WaveIndexTracker::WaveIndexTracker<T>(…)::mgr` — clearly the
  `WavetableManager<T>` instance whose `waveforms_` are scanned.
- `WaveIndexTracker::WaveIndexTracker<T>(…)::wp` — local in
  range-for over `mgr.waveforms_`; obvious shared-pointer alias
  ("waveform pointer"). No misuse.
- `WaveIndexTracker::WaveIndexTracker<T>(…)::idx` — local int
  holding the candidate `wp->waveIndex` value before insertion.
- `WaveIndexTracker::assignAuto::it` / `WaveIndexTracker::hasGaps::it`
  — short-lived iterators in 2-3-line scopes, idiomatic.
- `WavetableException::WavetableException::msg` — matches the
  `message_` member it initialises.

## 5. Coverage

- **Fully covered:** all instance data members of `WaveIndexTracker`
  and `WavetableException`; every parameter of every method
  defined in either translation unit; every meaningful local in
  `wave_index_tracker.cpp`.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type names `WaveIndexTracker`, `WavetableException` — present
    verbatim in `nm --demangle` output (typeinfo / mangled methods).
  - Method names `WaveIndexTracker(int)`, `WaveIndexTracker<T>(int,
    WavetableManager<T> const&)`, `assign(int)`, `assignAuto(int)`,
    `usedWaveIndices() const`, `getNextAutoIndex()`, `hasGaps()`,
    `WavetableException(string const&)`, `~WavetableException()`,
    `what() const` — all in the binary symbol table, tier-1
    excluded. (Notable observation, not a flag: the readability of
    `assignAuto` is a method-name concern; out of scope.)
  - Template type parameter `T` of the templated ctor — RULES §2
    excludes template parameters.
  - The local `tree` in `wavetable_ir.cpp` (cross-batch reference) is
    in batch 46's territory.

## 6. Cross-batch references

- `Waveform::waveIndex` — already audited in batch 14
  (`14_waveform.md:35`) as `not-misnomer / high` (JSON key
  `"waveIndex"`). The template ctor here reads `wp->waveIndex` →
  consistent with that finding. No counterpart action needed.
- `WavetableIR::waveIndexTracker_` and
  `WavetableFront::waveIndexTracker_` — counterpart fields living in
  batches 46 and 45 respectively. They name this class faithfully;
  no he-said/she-said inconsistency observed from this side.
- `DeviceConstants::maxWaveIndex()` — supplies the value of
  `WaveIndexTracker::maxIndex`. Audited in batch 31
  (`31_device_constants.md:47`) and kept; consistent with the
  `maxIndex` member spelling here.
