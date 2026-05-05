# Batch 17 — waveform_front

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 1 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 0;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 1.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/waveform/waveform_front.hpp`
- `reconstructed/src/waveform/waveform_front.cpp`

Cross-references consulted:
- `reconstructed/include/zhinst/waveform/waveform_ir.hpp` (sister extension layout)
- `reconstructed/src/waveform/wavetable_manager_front.cpp` (only writer of
  `hasDuplicate_`, `values`, the convenience setters)
- `reconstructed/src/runtime/custom_functions.cpp` (reader of `hasDuplicate_`)
- `reconstructed/src/waveform/waveform_generator.cpp` (mentions `useCount_`)
- `reconstructed/src/ast/seqc_ast_nodes_evaluate.cpp` (reader/writer of
  `dirty_`)
- `nm --demangle _seqc_compiler.so` for symbol-table exclusions.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `WaveformFront::useCount_` | unsure | low | Only init=1, never read/inc'd | keep current; `refCount_` | — |
| `WaveformFront::dirty_` | no | medium | Comment+accessor confirm | keep current | not-misnomer |
| `WaveformFront::hasDuplicate_` | no | high | Set on duplicate detection | keep current | not-misnomer |
| `WaveformFront::values` | yes | medium | Generic name; missing `_`; holds gen-args | `genArgs_`, `funDescrArgs_`, keep current | — |
| `WaveformFront::WaveformFront(src,name)::source` | no | medium | Source waveform copied | keep current | not-misnomer |
| `WaveformFront::WaveformFront(src,name)::newName` | no | high | Renamed wave name | keep current | not-misnomer |
| `WaveformFront::WaveformFront(name,type,dc)::name` | no | high | Stored as Waveform::name | keep current | not-misnomer |
| `WaveformFront::WaveformFront(name,type,dc)::type` | no | high | File::Type → waveformType | keep current | not-misnomer |
| `WaveformFront::WaveformFront(name,type,dc)::dc` | no | medium | DeviceConstants reference | keep current | not-misnomer |
| `WaveformFront::setHasDuplicate::v` | no | low | Generic but acceptable | keep current | — |
| `WaveformFront::setModified::v` | no | low | Generic but acceptable | keep current | — |
| `WaveformFront::setFunDescrName::s` | no | low | Generic but acceptable | keep current | — |
| `WaveformFront::setFile::f` | no | low | Generic; tiny inline | keep current | — |
| `WaveformFront::setName::n` | no | low | Generic; tiny inline | keep current | — |
| `WaveformFront::setWaveIndex::idx` | no | high | Index value | keep current | — |
| `WaveformFront::toString::typeStr` | no | high | Local string for type | keep current | — |
| `WaveformFront::toString::oss` | no | high | ostringstream local | keep current | — |

## 3. Detailed findings

### WaveformFront::useCount_  [unsure / low / —]

Evidence:
- `include/zhinst/waveform/waveform_front.hpp:56` — declaration
  `int  useCount_;                    // +0xD8  (init=1)`
- `src/waveform/waveform_front.cpp:60` — copy-rename ctor:
  `useCount_ = 1;            // mov DWORD PTR [rbx+0xd8], 1`
- `src/waveform/waveform_front.cpp:102` — 3-arg ctor:
  `this->useCount_ = 1;        // +0xD8  — note: 1, NOT 0 (IR writes 0 here)`
- `src/waveform/waveform_generator.cpp:279` (comment):
  "and bump its use-count (uint32_t at WaveformFront +0xd8)."
- `src/waveform/waveform_generator.cpp:305` (comment in the relevant body):
  `// wf->useCount++;  // TODO: expose use-count field`
- `nm`: no static / external symbol named `useCount_` (non-static
  member, not encoded — in scope per §3).

Interpretation:
- The field is initialised to 1 in both reconstructed constructors and
  is never read or written outside those constructors anywhere in the
  reconstructed sources surveyed. The only commentary about it
  (`waveform_generator.cpp:279`, `:305`) describes it as a use-count
  the binary increments on lookup hits — but that increment is **not
  yet implemented in recon**. So the name is supported only by
  source-comment evidence (soft per §3) and an unimplemented TODO.

Judgement:
- Whether the field is genuinely a reference/use count or some other
  4-byte counter (e.g. an instance/version id) cannot be determined
  from the recon alone; per RULES §4b we mark `unsure` rather than
  overclaim. The size + init=1 pattern is at least consistent with
  a simple use-count.

Proposals:
- `keep current` (medium) — pending implementation of the hit-path
  increment that would confirm the name.
- `refCount_` (low) — clarifies it's a count of references, if a
  later pass confirms the increment-on-lookup semantics.

Locations consulted:
- declared: `include/zhinst/waveform/waveform_front.hpp:56`
- written: `src/waveform/waveform_front.cpp:60,102`
- referenced (comments only): `src/waveform/waveform_generator.cpp:279,305`

### WaveformFront::dirty_  [no / medium / not-misnomer]

Evidence:
- `include/zhinst/waveform/waveform_front.hpp:57`
  `bool dirty_;                     // +0xDC  (init=0; "isModified" in source comments)`
- Accessors at `:101-102`:
  `bool isModified() const      { return dirty_; }`
  `void setModified(bool v)     { dirty_ = v; }`
- `src/ast/seqc_ast_nodes_evaluate.cpp:3273-3274`:
  ```
  // Mark dirty: WaveformFront::dirty_ = 1 (+0xdc)
  if (wf) wf->dirty_ = true;
  ```
- `src/ast/seqc_ast_nodes_evaluate.cpp:3299,3307`:
  `// signal.markers_[idx], then sets dirty_=true.` /
  `wf->dirty_ = true;`
- `nm`: non-static member, in scope; not exported.

Interpretation:
- All write sites set the field after a mutating operation
  (writing into `signal.markers_[idx]`, etc.). Reads through the
  `isModified()` accessor have the same meaning. The two names
  ("dirty" and "isModified") are conventional synonyms for the
  same dirty-flag semantics.

Judgement:
- `dirty_` is an accurate, conventional name for the field; the
  `isModified()`/`setModified()` accessors are merely an alternate
  spelling of the same concept and do not contradict it.

Proposals:
- `keep current` (high)

Locations consulted:
- declared: `include/zhinst/waveform/waveform_front.hpp:57`
- accessors: `include/zhinst/waveform/waveform_front.hpp:101-102`
- written: `src/waveform/waveform_front.cpp:61,103`;
  `src/ast/seqc_ast_nodes_evaluate.cpp:3274,3307`

### WaveformFront::hasDuplicate_  [no / high / not-misnomer]

Evidence:
- `include/zhinst/waveform/waveform_front.hpp:58`
  `bool hasDuplicate_;                     // +0xDD  (copied; "hasDuplicate")`
- `src/waveform/waveform_front.cpp:64` — copy-rename ctor copies from source:
  `hasDuplicate_ = source->hasDuplicate_;  // movzx ecx, BYTE PTR [rax+0xdd]`
- `src/waveform/wavetable_manager_front.cpp:120-130`:
  ```
  existing.get()->setHasDuplicate(true); // +0xDD
  ...
  wf->setFile(std::make_shared<Waveform::File>(filename));
  ...
  wf->setHasDuplicate(true);  // +0xDD
  ```
  (the surrounding logic is the duplicate-name branch of
  `newWaveformFromFile` — when an existing waveform with the same
  filename is found, both the existing and the new entries are
  flagged.)
- `src/waveform/wavetable_manager_front.cpp:172-173`:
  `existing->setHasDuplicate(true);` / `wf->setHasDuplicate(true);`
- `src/runtime/custom_functions.cpp:1054-1055`:
  ```
  // @0x171228: check wf->hasDuplicate_ (+0xDD, "hasDuplicate")
  if (wf->hasDuplicate_) {
  ```
- `nm`: non-static member, in scope.

Interpretation:
- The flag is unambiguously set true exactly when the wavetable
  manager detects another waveform sharing the same name/file, and
  read at one site to gate behaviour for waveforms with duplicates.
  Both write and read sites use the literal phrase "hasDuplicate".

Judgement:
- Name fits usage exactly across all observed sites; positive
  evidence per §4d tier 4 (consistent unambiguous usage).

Proposals:
- `keep current` (high)

Locations consulted:
- declared: `include/zhinst/waveform/waveform_front.hpp:58`
- written: `src/waveform/waveform_front.cpp:64,104`;
  `src/waveform/wavetable_manager_front.cpp:123,130,172,173`
- read: `src/runtime/custom_functions.cpp:1055`

### WaveformFront::values  [yes / medium / —]

Evidence:
- `include/zhinst/waveform/waveform_front.hpp:60`
  `std::vector<Value> values;           // +0xE0  (each Value is 0x28 bytes)`
- `src/waveform/wavetable_manager_front.cpp:198`:
  `wf->values = args;                     // vector<Value> at +0xE0`
  (in `getWaveformForFront` / `newWaveform` — `args` is the vector of
  `Value`s passed to a waveform-generator function, e.g. the `values`
  vector built up in `seqc_ast_nodes_evaluate.cpp:405-407,459-461`).
- `src/waveform/wavetable_manager_front.cpp:225-233`:
  ```
  if (wf->values.size() != args.size()) continue;
  ...
  if (!(wf->values[i] == args[i])) { ... }
  ```
  (used as the lookup key in `getWaveformByFunDescr`: a stored wave
  matches when its name equals `funDescrName` AND its `values` equal
  the requested `args`.)
- `src/waveform/waveform_front.cpp:70` — copy-rename ctor copies the vector:
  `values = std::vector<Value>(source->values.begin(), source->values.end());`
- All other extension fields use the trailing-underscore convention
  (`useCount_`, `dirty_`, `hasDuplicate_`); `values` does not.
- `nm`: non-static member, in scope.

Interpretation:
- The field stores the **argument-`Value` vector** that, together with
  `Waveform::funDescrName` (the generator-function name), identifies a
  generator-produced waveform for cache lookup. It is not "the
  waveform's sample values" or "the values of this waveform" — it is
  the *generator arguments*.
- Independently, the lone missing trailing underscore on this member
  is a stylistic inconsistency with the surrounding extension fields.

Judgement:
- The bare name `values` is generic enough to mislead a reader
  unfamiliar with the cache-key role into thinking these are sample
  values; a name that conveys "the generator-call args used to
  build this waveform" would be more accurate.

Proposals:
- `genArgs_` (medium) — short, unambiguous "generator arguments".
- `funDescrArgs_` (medium) — matches the partner field
  `Waveform::funDescrName` ("genFunc" key) explicitly.
- `keep current` (low) — only acceptable if simultaneously renamed
  to `values_` for stylistic consistency.

Locations consulted:
- declared: `include/zhinst/waveform/waveform_front.hpp:60`
- written: `src/waveform/waveform_front.cpp:70`;
  `src/waveform/wavetable_manager_front.cpp:198`
- read: `src/waveform/wavetable_manager_front.cpp:225,233`

### WaveformFront::WaveformFront(source, newName)::source  [no / medium / not-misnomer]

Evidence:
- `src/waveform/waveform_front.cpp:55-71` — the body uses `source` exclusively
  to (a) forward as the `Waveform`-base copy-source and (b) read
  `source->hasDuplicate_` and `source->values` to copy them into
  `*this`. The header declaration matches.

Interpretation:
- The parameter is the prototype `WaveformFront` whose data is being
  copy-renamed into a new instance. "source" is the conventional name
  for that role.

Judgement:
- Fits usage; not a misnomer.

Proposals:
- `keep current` (high)

Locations consulted:
- `include/zhinst/waveform/waveform_front.hpp:84`
- `src/waveform/waveform_front.cpp:55-71`

### WaveformFront::WaveformFront(source, newName)::newName  [no / high / not-misnomer]

Evidence:
- `src/waveform/waveform_front.cpp:57` — passed to `Waveform`-base copy-rename
  ctor: `Waveform(std::shared_ptr<Waveform>(source), std::string(newName))`.

Interpretation:
- Used solely as the new name to set on the renamed copy.

Judgement:
- Name fits usage exactly.

Proposals:
- `keep current` (high)

### WaveformFront::WaveformFront(name, type, dc)::{name,type,dc}  [no / high|medium / not-misnomer]

Evidence:
- `src/waveform/waveform_front.cpp:82-99`:
  - `this->name             = name;` — stored verbatim into
    `Waveform::name`.
  - `this->waveformType     = type;` — stored verbatim into
    `Waveform::waveformType` (`File::Type`).
  - `this->seqRegWidth      = static_cast<int>(dc.waveformGranularity);`
    and `this->deviceConstants  = &dc;` — `dc` used as a
    `DeviceConstants` reference both for a pulled-out field and
    by-pointer storage.

Interpretation:
- Each parameter is used directly with the meaning its name implies.

Judgement:
- All three fit usage; not misnomers.

Proposals:
- `keep current` (high) for `name`, `type`; (medium) for `dc` — the
  conventional but terse two-letter name is well-understood here.

### WaveformFront::toString::typeStr  /  ::oss  [no / high / —]

Evidence:
- `src/waveform/waveform_front.cpp:23-39` — `oss` is the
  `std::ostringstream` accumulating the formatted string;
  `typeStr` is the local set per `switch` arm to the literal
  `"CSV"`/`"RAW"`/`"GEN"`/`"UNDEF"` and then streamed.

Interpretation:
- Both names describe their roles directly.

Judgement:
- Fit usage; not misnomers.

Proposals:
- `keep current` (high)

## 4. Symbols inspected and judged routinely fine

- `WaveformFront` (type name) — appears in `nm --demangle` as
  `zhinst::WaveformFront::*` (e.g. `0x2a2510 t zhinst::WaveformFront::WaveformFront(...)`,
  `0x2c5120 t zhinst::WaveformFront::toString() const`); type names
  are excluded per RULES §3.
- `WaveformFront::WaveformFront(source,newName)` (method) — encoded
  in `nm`; out of scope for renaming.
- `WaveformFront::~WaveformFront()` — implicit; method name out of
  scope per §3 (encoded via `__on_zero_shared`).
- `WaveformFront::toString` (method) — encoded in `nm` at `0x2c5120`;
  out of scope.
- Convenience accessors `setHasDuplicate`, `hasDuplicate`,
  `isModified`, `setModified`, `funDescrName`, `setFunDescrName`,
  `setFile`, `setName`, `setWaveIndex` (methods) — these inline
  one-liners do **not** appear in `nm` because they are
  recon-only convenience helpers added to keep call sites compiling
  during the audit (see header comment at lines 92-95). Their
  parameters (`v`, `s`, `f`, `n`, `idx`) are minimal one-liners
  with self-evident intent; flagged in §2 as `no / low` and not
  worth dedicated blocks.
- Header layout-comment numerals (`+0xD8`, `+0xDC`, `+0xDD`,
  `+0xE0`, `+0xF8`) — not symbols.

## 5. Coverage

- **Fully covered:** all symbols declared or defined in
  `waveform_front.hpp` and `waveform_front.cpp` per RULES §2:
  the data members (`useCount_`, `dirty_`, `hasDuplicate_`,
  `values`), all constructor parameters (3-arg and copy-rename),
  destructor, `toString` and its locals, and every convenience
  accessor's parameter. Layout-comment "speculative fields removed"
  list at lines 35-42 of the header is documentation of *removed*
  symbols, not current symbols, so not in scope.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type name `WaveformFront` — in `nm`.
  - Method names `WaveformFront::WaveformFront(shared_ptr,string const&)`,
    `WaveformFront::~WaveformFront`, `WaveformFront::toString` — in
    `nm`.
  - Inherited base-class fields used directly in `waveform_front.cpp`
    (`name`, `waveformType`, `secondaryName`, `file`, `used`,
    `addressValue`, `funDescrName`, `playWord`, `waveIndex`,
    `seqRegWidth`, `allocationByteSize`, `deviceConstants`,
    `signal.length_`, `signal.channels_`) — owned by `Waveform`,
    belong to a different batch.
  - The 3-arg constructor `WaveformFront(name, type, dc)` body — the
    *constructor itself* is inlined into the dispatcher per the
    header comment, so there is no separate symbol; we still audited
    the parameters since the recon provides a body that makes them
    in-scope identifiers.
