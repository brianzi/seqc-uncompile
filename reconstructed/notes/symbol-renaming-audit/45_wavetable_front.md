# Batch 45 — wavetable_front

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 3 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 0;
> B3 (already resolved during Phase D/R): 1;
> B4 (wontfix / kept-as-is): 2.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/waveform/wavetable_front.hpp`
- `reconstructed/src/waveform/wavetable_front.cpp`
- `reconstructed/src/waveform/wavetable_manager_front.cpp`

Cross-references consulted:

- `reconstructed/include/zhinst/waveform/wavetable_ir.hpp` (sister type with the
  same two leading address fields, but named differently —
  `addressBase_` / `firstWaveformOffset_`).
- `reconstructed/src/waveform/wavetable_ir.cpp` (reads `frontMgr->lineNr_`,
  `frontMgr->waveformCounter_`; calls `getUniqueName(name, lineIdx, counter)`).
- `reconstructed/src/waveform/wavetable_manager_ir.cpp` (JSON serialiser /
  deserialiser; uses keys `"numDefs"` and `"numDefs2"` for the two
  leading int fields — see §3 detail block).
- `reconstructed/src/ast/seqc_ast_nodes_evaluate.cpp` (every AST evaluator
  prologue calls `ctx.wavetable->setLineNr(lineNr)` with the AST node's
  source line number — strong evidence for `lineNr_`).
- `reconstructed/src/codegen/compiler.cpp` (`Compiler::setLineNr` tail-calls
  `WavetableFront::setLineNr`).
- `reconstructed/src/waveform/waveform_front.cpp` and
  `reconstructed/notes/symbol-renaming-audit/17_waveform_front.md`
  (sister batch — `WaveformFront::values` was flagged `yes / medium`;
  this batch confirms the parallel field assignment site).
- `reconstructed/notes/symbol-renaming-audit/14_waveform.md` and
  `48_address_impl.md` (parent/wrapper-type audits).
- `nm --demangle _seqc_compiler.so` for symbol-table exclusions
  (output captured inline in the assistant context; key verifications
  reproduced in §5).

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `WavetableFront::deviceConstants_` | no | high | Used as DeviceConstants pointer | keep current | not-misnomer |
| `WavetableFront::address_` | unsure | low | Generic; partner unread | keep current; `addressBase_` | cross-batch-arbitration |
| `WavetableFront::address2_` | yes | medium | Never read; vague | `firstWaveformOffset_`, `addressCursor_`, keep current | cross-batch-arbitration |
| `WavetableFront::oss_` | unsure | low | Reserved storage; unused | keep current; `errorStream_` | — |
| `WavetableFront::cachedParser_` | no | high | Holds CachedParser; passed to CSV | keep current | not-misnomer |
| `WavetableFront::dioTableUsage_` | no | high | DIO table size accounting | keep current | not-misnomer |
| `WavetableFront::warningCallback_` | no | high | Warning sink callback | keep current | not-misnomer |
| `WavetableFront::warningCallbackStorage_` | unsure | low | Storage tail of std::function | keep current; merge into above | — |
| `WavetableFront::manager_` | no | high | Owns WavetableManager | keep current | not-misnomer |
| `WavetableFront::waveIndexTracker_` | no | high | Holds WaveIndexTracker | keep current | not-misnomer |
| `WavetableFront::WavetableFront::dc` | no | medium | DeviceConstants ref | keep current | — |
| `WavetableFront::WavetableFront::addr` | no | high | Initial address | keep current | — |
| `WavetableFront::WavetableFront::lineNr` | no | medium | Forwarded to CachedParser | keep current | — |
| `WavetableFront::WavetableFront::path` | no | high | Filesystem path arg | keep current | — |
| `WavetableFront::dummyWarning::msg` | no | high | Logged message | keep current | — |
| `WavetableFront::dummyWarning::level` | no | medium | Severity level (unused) | keep current | — |
| `WavetableFront::setWarningCallback::cb` | no | high | Callback being installed | keep current | — |
| `WavetableFront::getMemorySize::total` | no | high | Accumulator | keep current | — |
| `WavetableFront::getMemorySize::wf` / `it` / `dc` / `length` / `channels` / `bitsPerSample` / `alignedLen` / etc. | no | medium | Idiomatic locals | keep current | — |
| `WavetableFront::newEmptyWaveform::name` | no | high | Forwarded to manager | keep current | — |
| `WavetableFront::newWaveformFromFile(3-arg)::{name,filename,type}` | no | high | Forwarded verbatim | keep current | — |
| `WavetableFront::newWaveformFromFile(4-arg)::{name,signal,filename,type}` | no | high | Forwarded verbatim | keep current | — |
| `WavetableFront::newWaveform(4-arg)::{name,signal,funName,args}` | no | high | Forwarded to manager | keep current | — |
| `WavetableFront::newWaveform(3-arg)::{signal,funName,args}` | no | high | Same; auto-name path | keep current | — |
| `WavetableFront::newWaveform(3-arg)::baseIndex` | yes | medium | It's `lineNr_`, not index | `lineNr`, keep current | — |
| `WavetableFront::newWaveform(3-arg)::counter` / `uniqueName` | no | high | Self-evident | keep current | — |
| `WavetableFront::toString::{begin_ptr,end_ptr,oss}` | no | high | Idiomatic locals | keep current | — |
| `WavetableFront::loadWaveform::{wf,ptr,deviceType,cache}` | no | high | Self-evident | keep current | — |
| `WavetableFront::waveformExists::name` | no | high | Lookup key | keep current | — |
| `WavetableFront::getWaveformByName::name` | no | high | Optional lookup key | keep current | — |
| `WavetableFront::getWaveformByFunDescr::{funName,args}` | no | high | Forwarded | keep current | — |
| `WavetableFront::copyWaveform::src` | no | high | Source waveform | keep current | — |
| `WavetableFront::checkWaveformInitialized::name` | no | high | Lookup key | keep current | — |
| `WavetableFront::checkWaveformInitialized::{nameCopy,wf,wf2}` | unsure | low | Stub locals; unclear | keep current | — |
| `WavetableFront::getWaveformSampleLength::name` | no | high | Lookup key | keep current | — |
| `WavetableFront::updateDioTableUsage::{key,value}` | no | medium | Map key/value | keep current | — |
| `WavetableFront::assignWaveIndex::{wf,index,ptr,currentIndex}` | no | high | Self-evident | keep current | — |
| `WavetableFront::updateWave::{wf,newName}` | no | high | Forwarded | keep current | — |
| `WavetableFront::setLineNr::nr` | no | medium | Forwarded to manager_ | keep current | — |
| `WavetableFront::dummyWarning` (method) | — | — | In symbol table | — | — |
| `detail::WavetableManager<WaveformFront>::lineNr_` | yes | medium | Source line, not "numDefs" | keep current; `numDefs_` | cross-batch-arbitration |
| `detail::WavetableManager<WaveformFront>::waveformCounter_` | yes | medium | JSON key says "numDefs2" | keep current; `numDefs2_` | cross-batch-arbitration |
| `detail::WavetableManager<WaveformFront>::nameToIndex_` | no | high | name→vector index map | keep current | not-misnomer |
| `detail::WavetableManager<WaveformFront>::waveforms_` | no | high | Vector of WaveformFront | keep current | not-misnomer |
| `detail::WavetableManager<WaveformFront>::insertWaveform::wf` | no | high | The new waveform | keep current | — |
| `WavetableManager::newEmptyWaveform::{name,dc,baseIdx,counter,uniqueName,wf}` | no | medium | Self-evident; `baseIdx` see §3 | keep current | — |
| `WavetableManager::newWaveformFromFile(4-arg)::{name,filename,type,dc,nameCopy,it,existing,wf}` | no | medium | Forwarded args | keep current | — |
| `WavetableManager::newWaveformFromFile(6-arg)::{name,signal,addr,filename,type,dc}` | no | high | Match base layout | keep current | — |
| `WavetableManager::newWaveform(5-arg)::{name,signal,funName,args,dc}` | no | high | Forwarded | keep current | — |
| `WavetableManager::getWaveformForFront::{funName,args,wf_ptr,wf,match}` | no | high | Idiomatic | keep current | — |
| `WavetableManager::copyWaveform::{src,srcPtr,baseIdx,counter,uniqueName,copy}` | no | medium | `baseIdx` see §3 | keep current | — |
| `WavetableManager::updateWave::{wf,newName,ptr,oldName,oldIdx,it}` | no | high | Self-evident | keep current | — |
| `getUniqueName::{base,index,counter}` (free helper) | no | high | Format string `"__"+base+"_"+i+"_"+c` | keep current | — |

## 3. Detailed findings

### WavetableFront::address_ + WavetableFront::address2_  [unsure / low | yes / medium  /  cross-batch-arbitration]

Evidence:

- `wavetable_front.hpp:155-156`
  ```
  uint32_t address_;
  uint32_t address2_;
  ```
- `wavetable_front.cpp:57-58` (constructor body):
  ```
  address_ = addr.value;
  address2_ = addr.value;
  ```
- `wavetable_front.cpp:190` (sole read site of `address_`):
  ```
  manager_->newWaveformFromFile(
      name, signal, detail::AddressImpl<uint32_t>{address_},
      filename, type, *deviceConstants_);
  ```
- `wavetable_front.hpp:7-8` header-comment layout note:
  ```
  //   0x008: uint32_t addressValue_       (from AddressImpl<uint32_t>)
  //   0x00C: uint32_t addressValue2_      (duplicate/copy)
  ```
- Sister type: `wavetable_ir.hpp:78-79` (same two fields at the same
  offsets `+0x08`/`+0x0C`):
  ```
  uint32_t addressBase_;            // +0x08
  uint32_t firstWaveformOffset_;    // +0x0C
  ```
- `wavetable_ir.cpp:58-59,99-100` (per `48_address_impl.md:46`):
  `addressBase_(address.value), firstWaveformOffset_(address.value),`
- `nm --demangle`: neither field is exported (non-static instance
  members per RULES §3 — in scope).
- Project-wide grep for `address2_` (this report's own grep): the
  only occurrences are the **declaration** and the **single write**.
  There is no read site for `address2_` anywhere in `reconstructed/`.

Interpretation:

- `WavetableFront` and `WavetableIR` are sister classes that occupy
  the same two leading uint32_t slots, both initialised from the
  same `AddressImpl<uint32_t>` argument. In the IR the binary
  semantics have been pinned down: `addressBase_` is the constant
  base of the wavetable region, and `firstWaveformOffset_` advances
  as waveforms are placed (search for `addressBase_` / `firstWaveformOffset_`
  in `wavetable_ir.cpp` shows the cursor semantics). In the front
  reconstruction the second slot is *never read* — only ever
  written, and only in the constructor — so its dynamic role
  (cursor vs. duplicate-of-base) cannot be discriminated from the
  recon body alone.
- `address_` matches `addressBase_` mechanically (initialised to
  `addr.value`, passed to the manager when constructing a file-backed
  waveform that needs to know where the wavetable starts).
- The names `address_` and `address2_` are stylistic placeholders —
  generic, unmotivated, with `address2_` carrying no role hint at
  all — written before the IR pair was identified.

Judgement:

- `address2_` is a misnomer: it tells the reader nothing about the
  field's purpose, conflicts with the much more descriptive sister
  name `firstWaveformOffset_` at the same offset, and the recon
  has no read site that would let us pick the correct rename
  unilaterally (cf. RULES §4c).
- `address_` is acceptable as a generic name but is suspect by
  parallel: if the IR side is `addressBase_`, the front side
  almost certainly should be too.

Proposals:

- For `address_`: `keep current` (medium); `addressBase_` (medium) —
  if synthesis adopts the IR-parallel naming.
- For `address2_`: `firstWaveformOffset_` (medium) — consistent
  with `WavetableIR`; `addressCursor_` (low) — descriptive
  alternative; `keep current` (low) — only acceptable until
  arbitrated against `WavetableIR`.

Cross-reference:

- Counterpart `WavetableIR::addressBase_` and
  `WavetableIR::firstWaveformOffset_` belong to batch 46
  (wavetable_ir).

Locations consulted:

- declared: `include/zhinst/waveform/wavetable_front.hpp:155-156`
- written: `src/waveform/wavetable_front.cpp:57-58`
- read:    `src/waveform/wavetable_front.cpp:190` (only `address_`; no read of
  `address2_`)
- counterpart: `include/zhinst/waveform/wavetable_ir.hpp:78-79`;
  `src/waveform/wavetable_ir.cpp:58-59,99-100`

### WavetableFront::oss_  [unsure / low / —]

Evidence:

- `wavetable_front.hpp:158`
  `std::ostringstream oss_;`
  with layout-comment annotation `// 0x118 bytes`.
- `wavetable_front.cpp:60-62` constructor body:
  ```
  // Initialize ostringstream at this+0x10
  // new (&oss_) std::ostringstream();
  ```
  (the actual placement-new is commented out — recon-only.)
- No read or write of `oss_` anywhere else under `reconstructed/`
  (the in-method `toString()` declares its own local `std::ostringstream oss;`
  on the stack at `wavetable_front.cpp:222`).
- `nm`: instance member, not exported.

Interpretation:

- The 0x108-byte `ostringstream` slot is part of the binary layout but
  the reconstruction has not identified what code path consumes it.
  The recon code that does string-building uses a fresh stack-local
  stream rather than the member.

Judgement:

- The literal `oss_` is at most generic; without identified use sites
  in the recon, neither "keep current" nor any specific rename can
  be argued from observed semantics. Marked `unsure / low`.

Proposals:

- `keep current` (medium) — until a use site is found.
- `errorStream_` (low) — speculation that the binary uses it for
  buffered diagnostic formatting; not supported by recon.

Locations consulted:

- declared: `include/zhinst/waveform/wavetable_front.hpp:158`
- written:  `src/waveform/wavetable_front.cpp:60-62` (commented-out)

### WavetableFront::warningCallbackStorage_  [unsure / low / —]

Evidence:

- `wavetable_front.hpp:165-166`
  ```
  // 0x1B0: (internal function storage pointer — points to warningCallback_ SSO or heap)
  char warningCallbackStorage_[0x20];
  ```
- No read or write of `warningCallbackStorage_` anywhere else under
  `reconstructed/`.

Interpretation:

- A 32-byte tail buffer following `warningCallback_`. The header
  comment notes it as the SSO/heap pointer companion to the
  preceding `std::function`. Whether this is a genuinely separate
  field in the original or simply the rest of the `std::function`
  control block being modeled as opaque bytes is not determinable
  from the recon alone.

Judgement:

- Cannot judge as misnomer or non-misnomer without identifying the
  field's role; the name accurately reflects "the bytes that
  belong to the warning-callback storage region", which is mostly a
  recon-bookkeeping label rather than an original-source name.

Proposals:

- `keep current` (medium) — neutral, accurate to current model.
- Merge into `warningCallback_`'s declared type if the bytes are
  proven to belong to the `std::function` itself (would eliminate
  the symbol).

Locations consulted:

- declared: `include/zhinst/waveform/wavetable_front.hpp:165-166`

### WavetableFront::newWaveform(3-arg)::baseIndex  [yes / medium / —]

Evidence:

- `wavetable_front.cpp:210-217`:
  ```
  int baseIndex = manager_->lineNr_;
  int counter = manager_->waveformCounter_;
  manager_->waveformCounter_ = counter + 1;
  std::string uniqueName = getUniqueName(funName, baseIndex, counter);
  ```
- The companion in the manager TU,
  `wavetable_manager_front.cpp:62-66`, calls the same identifier
  `baseIdx` (also fed from `lineNr_`).
- `getUniqueName` parameter list at `wavetable_helpers.hpp:28-32`:
  the second arg is named `index` and is rendered into the format
  string as `"__" + base + "_" + index + "_" + counter`.

Interpretation:

- The local copies `manager_->lineNr_` (a source-line number, per the
  many `setLineNr(astLineNr)` writes in `seqc_ast_nodes_evaluate.cpp`)
  into a variable called `baseIndex`. The value passed to
  `getUniqueName` therefore *embeds the source line* into the
  generated name as the "index" slot of the uniquification scheme.
- "baseIndex" suggests an array/vector index, which is not what is
  read into it.

Judgement:

- The local name is mildly misleading: the value is a line number
  borrowed by `getUniqueName` to play the role of an "index", not an
  index in any usual sense. Renaming to `lineNr` (or
  `lineNrAsIndex`) would describe provenance more accurately.

Proposals:

- `lineNr` (medium) — matches the source field.
- `keep current` (low) — consistent with `getUniqueName`'s
  parameter name and with `wavetable_manager_front.cpp::baseIdx`,
  which has the same issue.

Locations consulted:

- `src/waveform/wavetable_front.cpp:211-215`
- compare: `src/waveform/wavetable_manager_front.cpp:62-66`,
  `src/waveform/wavetable_manager_front.cpp:254-258`

### detail::WavetableManager<WaveformFront>::lineNr_  +  ::waveformCounter_  [yes / medium / cross-batch-arbitration]

Evidence:

- `wavetable_front.hpp:79-81`
  ```
  // 0x00
  int lineNr_;
  // 0x04
  int waveformCounter_;
  ```
- `wavetable_front.cpp:380-382` `WavetableFront::setLineNr`:
  ```
  void WavetableFront::setLineNr(int nr) {
      manager_->lineNr_ = nr;
  }
  ```
- `seqc_ast_nodes_evaluate.cpp` — every AST evaluator prologue calls
  `ctx.wavetable->setLineNr(lineNr)` where `lineNr` originates from
  the AST node's `lineNr_` (e.g. `:5414`,`:5529`,`:5663`,`:6361`,
  `:6997`,`:7187`, …; many dozens of identical sites).
- Use as the "index" slot of `getUniqueName`:
  `wavetable_front.cpp:215`,
  `wavetable_manager_front.cpp:62-66,254-258`,
  `wavetable_ir.cpp:541-546`.
- `wavetable_manager_front.cpp:62-64`:
  ```
  int baseIdx = lineNr_;       // this+0x00
  int counter = waveformCounter_; // this+0x04
  waveformCounter_ = counter + 1;
  ```
- **Counter-evidence** from the IR sister code (same memory layout,
  same field semantics):
  - `wavetable_manager_ir.cpp:9-10` layout comment uses the labels
    `numDefs` and `numDefs2`.
  - `wavetable_manager_ir.cpp:58-62`: ctor parameters are
    `int numDefs, int numDefs2`, stored verbatim into
    `this->lineNr_` and `this->waveformCounter_`.
  - `wavetable_manager_ir.cpp:213-217`: `fromJson` reads
    `obj.at("numDefs").as_int64()` into `numDefs`, then
    `obj.at("numDefs2").as_int64()` into `numDefs2`, and constructs
    the manager from those.
  - `wavetable_manager_ir.cpp:243-245`: `toJson` emits
    ```
    {"numDefs",  lineNr_},
    {"numDefs2", waveformCounter_},
    ```
  These JSON keys are tier-2 positive evidence per RULES §4d
  ("member name appears verbatim as a JSON serializer key"). They
  point to the *opposite* names, `numDefs_` and `numDefs2_`.
- Archive trail: `notes/archive/TODO_phases_1-12.md:855` records the
  earlier deliberate rename `numDefs_ → lineNr_`,
  `numDefs2_ → waveformCounter_`, motivated by the
  `setLineNr`/`waveformCounter_++` usage observed above.
- `nm --demangle`: instance members of a template specialisation —
  not encoded as data symbols (RULES §3); in scope.

Interpretation:

- The two fields have **two competing strong signals**:
  1. The setter (`setLineNr`) and every AST-evaluator caller treat
     the first field as a *source line number*, and the manager
     post-increments the second field once per newly-created
     waveform — both consistent with the current names.
  2. The IR-side JSON serialiser/deserialiser, which is the
     authoritative round-trip used by `WavetableManager<WaveformIR>`,
     calls these very same fields `"numDefs"` and `"numDefs2"`.
- The serialiser JSON keys are the kind of evidence RULES §4d
  ranks at tier 2, while the setter-name and use-site evidence are
  tier 4 (consistent usage). Yet the two disagree; one of them is
  itself a misnomer (either the field names, or the JSON keys
  inherited from an earlier design).
- Per RULES §4c (he-said/she-said arbitration), the inconsistency
  is the high-confidence finding; deciding which side is wrong is
  a separate, lower-confidence question that this batch cannot
  settle on its own. Note also that these are the **front-side**
  members (`WavetableManager<WaveformFront>`); the JSON serialiser
  lives in the **IR** specialisation and uses the same template
  field names. Synthesis must consider both specialisations
  together and decide whether to (a) keep `lineNr_/waveformCounter_`
  (and concede that the JSON keys are stale/legacy), or (b) revert
  to `numDefs_/numDefs2_` (and concede that the names cosmetically
  diverge from `setLineNr`).

Judgement:

- The fields are flagged because the existing names contradict the
  authoritative serializer keys for the same memory; this is a
  cross-batch arbitration to be resolved between this batch and
  batch 46 (and possibly batch 43 — wavetable_helpers — if the
  helper inherits naming conventions from the manager).

Proposals:

- For both fields: `keep current` (medium) — relies on the
  setter/use evidence and treats the JSON keys as legacy strings.
- For `lineNr_`: `numDefs_` (low) — matches JSON key but contradicts
  `setLineNr` and the consistent line-number write pattern; would
  require a parallel rename of `setLineNr` to remain coherent.
- For `waveformCounter_`: `numDefs2_` (low) — same reasoning.

Cross-reference:

- Counterpart `detail::WavetableManager<WaveformIR>::lineNr_`,
  `::waveformCounter_`, plus the JSON key strings `"numDefs"` and
  `"numDefs2"`, belong to batch 46 (wavetable_ir).

Locations consulted:

- declared: `include/zhinst/waveform/wavetable_front.hpp:79-81`
- written:  `src/waveform/wavetable_front.cpp:380-382`;
            `src/waveform/wavetable_manager_front.cpp:63-64,255-256`;
            `src/waveform/wavetable_manager_ir.cpp:61-62`
- read:     `src/waveform/wavetable_front.cpp:211-215`;
            `src/waveform/wavetable_manager_front.cpp:62,254`;
            `src/waveform/wavetable_ir.cpp:72-73,541-546`
- JSON evidence: `src/waveform/wavetable_manager_ir.cpp:213-217,243-245`
- AST callers: `src/ast/seqc_ast_nodes_evaluate.cpp` — many sites
  (e.g. `:5417`,`:5532`,`:5666`,`:6363`,`:6999`,`:7189`,`:7547`,
  `:8095`,`:8357`,`:8680`,`:9035`,`:9320`,`:9685`,`:9867`).

### WaveformFront::values cross-reference

Confirms batch 17's flag — this batch's
`wavetable_manager_front.cpp:198` is the unique writer:
```
wf->values = args;                     // vector<Value> at +0xE0
```
and `:225-233` is one of the readers in the cache lookup. No new
information beyond what batch 17 already recorded; no separate
block needed here.

Cross-reference:

- See batch 17 (`17_waveform_front.md`, "WaveformFront::values"
  block).

## 4. Symbols inspected and judged routinely fine

Header layout and recon-bookkeeping items:

- `WavetableFront::deviceConstants_` — read on every delegating
  newWaveform path as the `DeviceConstants` reference; name
  matches `Waveform::deviceConstants` (also a pointer).
- `WavetableFront::cachedParser_` — declared as `char[0x60]`
  recon-byte buffer reinterpreted to `CachedParser&` at
  `wavetable_front.cpp:248` for `CsvParser::csvFileToWaveform`;
  name matches its true type.
- `WavetableFront::dioTableUsage_` — a `std::map<size_t,size_t>`
  written at `:335`, summed at `:339-341`, and compared against
  `deviceConstants_->maxDioTableEntries()` at `:344`. The accessor
  method `updateDioTableUsage` uses the same word — name matches
  exactly.
- `WavetableFront::warningCallback_` — written by
  `setWarningCallback` (`:108-109`) and used as the warning sink
  installed via the public API; defaults to `dummyWarning`.
- `WavetableFront::manager_` — the owned `WavetableManager*`;
  every method body that needs the waveform list, the name map,
  or the line/counter pair goes through `manager_->…`.
- `WavetableFront::waveIndexTracker_` — opaque buffer for the
  `WaveIndexTracker` instance; `WaveIndexTracker` is itself in
  the symbol table.
- `WavetableFront` (type) — appears in `nm --demangle` with full
  qualifier (e.g. `0x29ab10 t zhinst::WavetableFront::WavetableFront(...)`);
  excluded per RULES §3.
- All `WavetableFront::…` method names listed in §1's `nm` digest
  (constructor, destructor, `dummyWarning`, `begin`, `end`,
  `setWarningCallback`, `getMemorySize`, `newEmptyWaveform`,
  `newWaveformFromFile` (both overloads), `newWaveform` (both
  overloads), `toString`, `loadWaveform`, `waveformExists`,
  `getWaveformByName`, `getWaveformByFunDescr`, `copyWaveform`,
  `checkWaveformInitialized`, `getWaveformSampleLength`,
  `updateDioTableUsage`, `assignWaveIndex`, `updateWave`,
  `setLineNr`) — all encoded in the symbol table; method
  *names* out of scope per RULES §3.
- All `detail::WavetableManager<WaveformFront>::…` method names
  (`~WavetableManager`, `insertWaveform`, `newEmptyWaveform`,
  both `newWaveformFromFile` overloads, `newWaveform`,
  `getWaveformForFront`, `copyWaveform`, `updateWave`) — all
  encoded in `nm --demangle`; method names out of scope.
- Constructor and method *parameters* of the type
  `Waveform::File::Type type` consistently named `type` — fits the
  base type's enum role.
- `WavetableFront::dummyWarning::msg` / `::level` — `level` is
  `/*level*/` (unused) but the comment preserves its conceptual
  role; matches the binary's signature of a warning sink.
- `WavetableFront::getMemorySize` body locals — `total`, `begin`,
  `end`, `it`, `wf`, `channels`, `length`, `dc`, `wfMaxCap`,
  `wfGrain`, `rounded`, `alignedLen`, `bitsPerSample`, `totalBits`,
  `bytes`, `wfMaxCap2`, `wfGrain2`, `rounded2`, `aligned2`,
  `bits2`, `b2` — every local maps directly to its arithmetic role
  in the granularity / bitsPerSample / channels memory-size
  computation.
- `WavetableFront::loadWaveform` body locals — `ptr`, `deviceType`,
  `cache`, `e` — all idiomatic and one-use.
- `WavetableFront::waveformExists::name`, `getWaveformByName::name`,
  `getWaveformByFunDescr::funName`, `getWaveformByFunDescr::args`,
  `copyWaveform::src`, `assignWaveIndex::wf`,
  `assignWaveIndex::index`, `updateWave::wf`, `updateWave::newName`,
  `setLineNr::nr`, `updateDioTableUsage::key`,
  `updateDioTableUsage::value`, `getWaveformSampleLength::name`,
  `checkWaveformInitialized::name` — all forwarded directly to
  manager methods or used as map lookup keys with the obvious
  meaning.
- `WavetableFront::checkWaveformInitialized::nameCopy` /
  `getWaveformSampleLength::nameCopy` — recon-only stub locals
  modeling the by-value string passed to the not-yet-implemented
  `checkWaveformInit` helper; flagged `unsure / low` because the
  helper is unimplemented and may rename them.
- `detail::WavetableManager<WaveformFront>::nameToIndex_` — every
  read/write site uses it as the name→vector-index hash table
  (lookup, insertion in `insertWaveform`, removal in
  `updateWave`); name is exact.
- `detail::WavetableManager<WaveformFront>::waveforms_` — the
  vector of `shared_ptr<WaveformFront>`; iterated in `begin`,
  `end`, `getMemorySize`, `toString`, `getWaveformByName`,
  `getWaveformForFront`; name is exact.
- All `detail::WavetableManager` method body locals — `idx`,
  `name`, `oldName`, `oldIdx`, `it`, `existing`, `wf`, `wf_ptr`,
  `srcPtr`, `copy`, `match`, `nameCopy` — idiomatic for the
  hash-map / vector operations in use.
- `getUniqueName` (free helper in `wavetable_helpers.hpp`) — already
  audited in batch 43 territory; the parameter names `base`,
  `index`, `counter` are consistent with the format string
  `"__" + base + "_" + index + "_" + counter` derived from the
  `.rodata` constants (header lines 20-22).
- Header layout-comment offsets (`+0x00`, `+0x04`, `+0x08`,
  `+0x0C`, `+0x10`, `+0x118`, `+0x178`, `+0x190`, …) — not symbols.

## 5. Coverage

- **Fully covered:** all in-scope symbols declared or defined in
  `wavetable_front.hpp`, `wavetable_front.cpp`, and
  `wavetable_manager_front.cpp`:
  - `WavetableFront` data members
    (`deviceConstants_`, `address_`, `address2_`, `oss_`,
    `cachedParser_`, `dioTableUsage_`, `warningCallback_`,
    `warningCallbackStorage_`, `manager_`, `waveIndexTracker_`).
  - All constructor parameters (`dc`, `addr`, `lineNr`, `path`).
  - All public-method parameters (`name`, `signal`, `funName`,
    `args`, `filename`, `type`, `wf`, `index`, `nr`, `key`,
    `value`, `msg`, `level`, `cb`, `src`, `newName`).
  - All function locals named in the recon bodies (`total`,
    `begin`, `end`, `it`, `wf`, `length`, `channels`,
    `bitsPerSample`, `alignedLen`, `wfMaxCap`, `wfGrain`,
    `rounded`, `totalBits`, `bytes`, `wfMaxCap2`, `wfGrain2`,
    `rounded2`, `aligned2`, `bits2`, `b2`, `dc`, `baseIndex`,
    `counter`, `uniqueName`, `begin_ptr`, `end_ptr`, `oss`,
    `ptr`, `deviceType`, `cache`, `nameCopy`, `wf2`, `idx`,
    `currentIndex`, `copy`).
  - `detail::WavetableManager<WaveformFront>` data members
    (`lineNr_`, `waveformCounter_`, `nameToIndex_`,
    `waveforms_`).
  - All `WavetableManager` method parameters and locals
    (`name`, `dc`, `signal`, `addr`, `filename`, `type`,
    `funName`, `args`, `wf`, `src`, `newName`, `baseIdx`,
    `counter`, `uniqueName`, `nameCopy`, `it`, `existing`,
    `idx`, `wf_ptr`, `srcPtr`, `oldName`, `oldIdx`, `match`,
    `ptr`).
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type names `WavetableFront`, `detail::WavetableManager` — both
    present in `nm --demangle` output (e.g.
    `zhinst::WavetableFront::WavetableFront(...)`,
    `zhinst::detail::WavetableManager<zhinst::WaveformFront>::insertWaveform(...)`);
    excluded per §3.
  - All public method names of `WavetableFront` and of the
    `WavetableManager<WaveformFront>` template specialisation —
    encoded in the symbol table (see the `nm` block reproduced in
    §4); excluded per §3.
  - `WaveIndexTracker` (type) and its method names
    (`assignAuto`, `getNextAutoIndex`, `assign`, `hasGaps`,
    `usedWaveIndices`, `WaveIndexTracker(int)`,
    `WaveIndexTracker<WaveformFront>(int, WavetableManager<WaveformFront> const&)`)
    — all in `nm`; field used here only as an opaque
    `char waveIndexTracker_[0x28]` storage, so its internal
    members are out of scope for *this* batch.
  - `CachedParser` (type) and its members — used only via
    `reinterpret_cast<CachedParser*>` of an opaque buffer; owned
    by batch 15 (`15_cached_parser.md`).
  - `WaveformFront`-base inherited fields (`waveformType`,
    `signal.length_`, `signal.channels_`, `funDescrName`,
    `waveIndex`, `name`, `values`) read or written via accessors
    in the `.cpp` files — owned by batches 14 and 17.
  - `Signal::checkAllocation`, `Signal::samples_` etc. — owned by
    batch 37.
  - `getUniqueName` (free helper in `wavetable_helpers.hpp`) and
    its parameters — primarily owned by batch 43
    (`wavetable_helpers`); referenced here as cross-evidence only.
  - `DeviceConstants::waveformGranularity`, `::waveformPageSize`,
    `::bitsPerSample`, `::deviceType`, `::maxDioTableEntries()` —
    owned by batch 31.
  - `boost::filesystem::path` parameter and library symbols — third
    party.
