# Phase C — Synthesis

Per `RULES-symbol-renaming.md` §9: this document **consolidates** the
54 per-batch reports into a single ranked proposal list. It is still
read-only output. **No source edits yet.** Phase D (execution) is the
next user-driven step and will queue work items into `TODO.md`.

The scan phase produced 546 flagged rows (`yes` / `unsure`) and 167
status-tagged rows (`cross-batch-arbitration`, `coordinated-rename`,
`verify-not-original`) across 62 reports. This document does **not**
re-derive evidence — it points back to the per-batch §3 detail blocks.
The unit of synthesis is a *rename action*, not an individual symbol;
where a single semantic concern shows up under several names in
several batches, all those occurrences are bundled.

## How to use this document

1. **§1 Cross-batch coordinated clusters** — the "spine". Each
   cluster is a single semantic concept whose misnomer leaked into
   ≥2 batches. Each cluster has one canonical-name decision pending
   from the user, and a list of every batch+symbol that must rename
   in lockstep with that decision.
2. **§2 Open arbitrations** — pairs/triples where the audit could not
   decide which side is the misnomer; user judgement is needed.
3. **§3 In-batch coordinated renames** — multi-symbol groups confined
   to a single batch (no cross-batch coupling).
4. **§4 High-confidence single renames** (yes / high) — 35 unambiguous
   one-symbol renames.
5. **§5 Medium-confidence single renames** (yes / medium).
6. **§6 Low-confidence and unsure** — parked; revisit if context
   changes or during execution.
7. **§7 Dead code surfaced by the audit** — symbols whose role is
   "remove" rather than "rename".
8. **§8 Type-suspicion side observations** — RULES §2a side notes that
   are not naming actions but inform later type-fix work.
9. **§9 Incidental logic bugs** — non-naming bugs surfaced during
   audit; should be promoted to `incidental_findings.md` (IF-IDs)
   when audit closes.
10. **§10 `verify-not-original` items** — symbols not located in `nm`
    but with low evidence either way; need a second-pass spot check.
11. **§11 Recommended execution sequencing** — proposed order in which
    Phase D should execute the renames (cluster → batch → singleton).

A brief execution-discipline note appears in §12.

---

## §1. Cross-batch coordinated clusters

### Cluster A — `flag` parameters that mean `isWaveformCmd`

**Concern.** A `bool flag` param threaded through the AsmCommands
branch family (`br/brz/brnz/brgz`) and its impl/Cervino/Hirzel
overrides is consumed by exactly one statement:
`result.isWaveformCmd = flag;`. The name `flag` is non-descript
and hides that the parameter is the value being stored.

**Decision needed.** Pick one of:
1. Rename `flag` → `isWaveformCmd` *and accept* the existing field
   name (defer the inversion concern to Cluster B below).
2. Rename `flag` to whatever Cluster B settles `isWaveformCmd` to
   (e.g. `noOpt` / `skipOptimization` / `isControlOpcode`).

The audit recommends **option 2** so Cluster A and B move atomically.

**[Decision: Option 2 — adopt Cluster B canonical name (`noOpt` / `skipOptimization`); Cluster A and B move atomically.]**

**Scope (rename in lockstep — these are vtable-shaped overrides):**

| Batch | Symbol | Conf |
|---|---|---|
| 10 | `AsmCommands::br::flag` | high |
| 10 | `AsmCommands::brz::flag` | high |
| 10 | `AsmCommands::brnz::flag` | high |
| 10 | `AsmCommands::brgz::flag` | high |
| 49 | `AsmCommandsImpl::brz::flag` | high |
| 49 | `AsmCommandsImpl::brnz::flag` | high (same family) |
| 49 | `AsmCommandsImpl::brgz::flag` | high (same family) |
| 49 | `AsmCommandsImplHirzel::*::flag` | high (overrides) |
| 49 | `AsmCommandsImplCervino::*::flag` | high (overrides) |

Anchor evidence: `10_asm_commands.md:628-646`, `49_asm_commands_impl.md:252-284`.

---

### Cluster B — `isWaveformCmd` is semantically inverted

**Concern.** The field `AsmList::Asm::isWaveformCmd` and the free
predicate `Assembler::isWaveformCmd(instr)` (assembler.hpp:202-205)
both **lie about meaning**: the predicate selects opcodes
∈ {3, 4, 5} = MESSAGE / MESSAGE_4 / ERROR_MSG (control/message
opcodes), *not* waveform commands. Consumers in
`asm_optimize.cpp:343,454` use the field as a "skip optimization"
gate. Override paths in `assembler.cpp` set
`barrier.isWaveformCmd = false; orig.isWaveformCmd = (cmd-3) < 3u`,
confirming the (cmd∈{3,4,5}) semantics. Multiple batches reached
this conclusion independently from owner side, consumer side, and
producer side.

**Decision needed.** Choose canonical name. Audit-recommended order:

1. `noOpt` / `skipOptimization` (matches consumer use; abstracts away
   the "which opcodes" detail). **Preferred** by audit.
2. `isControlOpcode` / `isMessageOpcode` (matches producer truth).
3. Keep current (rejected — actively misleading).

**[Decision: `noOpt` / `skipOptimization` (option 1).]**

**Scope (every site that names this concept — must move atomically):**

| Batch | Symbol | Conf |
|---|---|---|
| 26 | free fn `Assembler::isWaveformCmd` (predicate name) | high |
| 26 | `Assembler::isWaveformCmd::instr` (param) | high |
| 44 | `AsmList::Asm::isWaveformCmd` (field) | high |
| 44 | `AsmList::Asm::lineNumber()` accessor (dual-use field, see 44 §3) | medium |
| 25 | usage at `oneStepJumpElimination`/`mergeRegisterZeroing` | high |
| 24 | `AsmExpression::isWaveformCmdOverride_` (positive-evidence: NOT a misnomer for the *override* concept; renames in lockstep) | medium |
| 10/49 | `flag` params from Cluster A | high |

Anchor evidence: `26_assembler.md:502+`, `44_asm_list.md:213-276`,
`25_asm_optimize.md:296-343`.

---

### Cluster C — `isHirzel`-equivalent flags / aliases scattered

**Concern.** `AWGCompilerConfig::isHirzel` is the canonical
device-family bit (UHF-style vs HDAWG-style). Multiple downstream
sites hold the *same* bit under different misnomer names, and a
private alias method exists that points at the wrong field name.

| Batch | Symbol | Current state | Decision |
|---|---|---|---|
| 09 | `Prefetch::isHirzel_()` (private alias) | yes/high — `return split_;` | drop alias |
| 09 | `Prefetch::set_isHirzel_()` | yes/high | drop alias |
| 09 | `Prefetch::split_` (field aliased) | linked: rename to `isHirzel_` if alias kept; keep `split_` if alias dropped | follow alias decision |
| 09b | `Prefetch::assignLoad::flag` | yes/high — call sites all pass `isHirzel` | → `isHirzel` |
| 23 | `AWGCompilerConfig::appendMode()` (alias) | yes/high — dead alias for `isHirzel` | drop alias |
| 36 | `Cache::appendMode_` (field) | unsure/low — initialized from `config.isHirzel` | keep current OR rename `isHirzel_`; audit favors `isHirzel_` to make provenance visible |
| 36 | `Cache::Cache::appendMode` (ctor param) | unsure/low | follows field decision |

**Decision needed.** Two coupled choices:
1. **Drop or keep `Prefetch::isHirzel_()` alias and parallel
   `AWGCompilerConfig::appendMode()` alias.** Audit recommendation:
   **drop both**. They actively lie.
2. **Rename `Cache::appendMode_` to `isHirzel_`?** Audit
   recommendation: **yes**, because every write site reads from
   `config.isHirzel` and the consumer logic
   (`if (appendMode_) pointer->position_ = 0;`) is in fact
   gated on the device-family bit.

Anchor: `09_prefetch.md:87-88`, `23_awg_compiler_config.md:79`,
`36_cache.md:112-146`, `09_prefetch_part2.md:66,152-174`.

**Note.** `Cache::getBestPosition::appendMode` (param, batch 36) is
a *different* boolean (recursion flag false=fast / true=gap-scan)
that happens to share the name; rename to `gapScan` independently.

**[Decision C.1: drop both alias families (`Prefetch::isHirzel_()` /
`set_isHirzel_()` AND `AWGCompilerConfig::appendMode()`).]**

**[Decision C.2: `Cache::appendMode_` → `isHirzel_`. The independent
`Cache::getBestPosition::appendMode` param → `gapScan`.]**

---

### Cluster D — `channelGrouping` is a loop-unroll iteration limit

**Concern.** The field `AWGCompilerConfig::channelGrouping` is
written once at `compile_seqc.cpp:206` to `0x20000` and read at
`compiler.cpp:283`, then propagated through `compiler.cpp:681`
into `FrontendLoweringContext::channelGrouping`, where it is used
*only* as `if (iterCount > ctx.channelGrouping) error 0x7b` — i.e.
as a guard on for-loop unroll iteration count. Unrelated to channels.

**Decision needed.** Pick canonical name:
1. `loopUnrollLimit` — most descriptive (audit-preferred).
2. `unrollLimit`.
3. Keep — rejected.

**[Decision: `loopUnrollLimit` (3-leg coordinated rename).]**

**Scope (3-leg coordinated rename):**

| Batch | Symbol | Conf |
|---|---|---|
| 23 | `AWGCompilerConfig::channelGrouping` (field) | medium |
| 32 | `FrontendLoweringContext::channelGrouping` (field) | medium |
| 07 | `FrontEndLoweringFacade::lower::channelGrouping` (param) | medium |

Optional 4th leg: an in-flight write at `compile_seqc.cpp:206` (same
name, outside audit scope unless `compile_seqc.cpp` is rescanned —
README §"Out of scope" deprioritized this file).

Anchor: `23_awg_compiler_config.md:348-398`,
`32_frontend_lowering.md:60-97`, `07_compiler.md:344-374`.

**Producer-side vindication.** Batch 30 (`awg_device_props`) confirmed
the *producer* of this value writes a loop-unroll iteration limit,
not a channel grouping config. No naming change needed in batch 30.

---

### Cluster E — Forwarding-accessor aliases that lie

**Concern.** Several classes expose forwarding accessor methods
named after a *previous* hypothesis that has since been disproven.
These methods have no implementation beyond `return some_field;`.
Leaving them in place hides the real field name behind a friendly
but wrong word.

| Batch | Alias | Underlying field | Verdict |
|---|---|---|---|
| 09 | `PrefetcherNodeState::lengthReg()` | `registerHirzel` | drop alias; use field |
| 09 | `PrefetcherNodeState::counter()` | `state` | drop alias; use field |
| 09 | `PrefetcherNodeState::playSize()` | `pageSize` | drop alias; rename field too |
| 09 | `PrefetcherNodeState::usedCache()` | `requiredSlots` | **drop alias and rename field** to `usedCache_` (alias is the correct name; field is the misnomer) |
| 23 | `AWGCompilerConfig::appendMode()` | `isHirzel` | drop alias (Cluster C) |
| 23 | `AWGCompilerConfig::splitIndex()` | `cacheType` | drop alias |
| 23 | `AWGCompilerConfig::syncVersion()` | `numChannelGroups` | drop alias |
| 24 | `AsmExpression::lineNumber()` | `labelIndex` (opcode counter, NOT source line) | drop alias OR rename field to `labelPc_` |
| 24 | `AsmExpression::labelType()` | a bool, not a "type" | drop alias OR rename to `hasLabel()` |
| 24 | `AsmExpression::labelPc()` | `labelIndex` (positive-evidence: this *is* the right name) | keep; aligns with proposed field rename |

**Decision needed.** For each pair, two equivalent choices:
- (a) drop alias; users access field directly.
- (b) keep alias and rename the field to match the alias.

Audit recommends **(a)** uniformly — it shrinks the API surface and
the binary already exports the field via mangled accessors where
necessary.

**[Decision: drop aliases uniformly (option a). Special case for
`PrefetcherNodeState::usedCache()` / `requiredSlots`: the alias
`usedCache()` is the correct name; rename the field
`requiredSlots` → `usedCache_` and drop the alias method.]**

Anchor: `09_prefetch.md:80-86`, `23_awg_compiler_config.md:79-81`,
`24_asm_expression.md:32-37`.

---

### Cluster F — `SeqCAstNode::type` is actually a source line number

**Concern.** Highest-impact cascade in the audit. The base ctor
parameter `SeqCAstNode::SeqCAstNode::type` (and the matching
accessor `type()`) actually **stores into and returns `lineNr_`**,
the source line number. Every derived class ctor (53 of them)
inherits this misnomer through the macro expansion `SEQC_*_IMPL`,
and `evaluate.cpp` then reads `this->type()` as a line number
in error reporting paths.

**Decision needed.** Rename the param and accessor to `lineNr` and
`lineNr()`. The field is already correctly `lineNr_`. The accessor
name itself is **not** in the binary symbol table (verified §3 of
batch 04a), so this is a free rename.

**Scope:**

| Batch | Symbol | Conf |
|---|---|---|
| 04a | `SeqCAstNode::SeqCAstNode::type` (ctor param) | high |
| 04a | `SeqCAstNode::type()` (accessor) | high |
| 04a | every derived ctor `int type` parameter (×53) | high |
| 04e | `SeqCArgList/DeclList/StmtList::evaluate::lineNr` local | low (positive — settles the cascade) |

Anchor: `04a_seqc_ast_node.md:85-94+`. This rename will produce a
mass diff but every site is mechanical.

**[Decision: approved. Rename `SeqCAstNode::type` (param + accessor)
→ `lineNr` / `lineNr()` and cascade through all 53 derived ctors
(×54 sites total).]**

---

### Cluster G — `first_` / `second_` / `first` / `second` in binary AST nodes

**Concern.** Eight binary-child SeqC node classes (`SeqCFunctionCall`,
`SeqCArray`, `SeqCIfCondition`, `SeqCCaseEntry`, `SeqCSwitchCase`,
`SeqCWhileLoop`, `SeqCDoWhile`, `SeqCRepeat`) have private fields
named `first_`/`second_` and ctor params `first`/`second` — generic
slot names — while exposing role-named accessors that are
nm-authoritative (`funName()`, `arguments()`, `array()`, `index()`,
`label()`, `body()`, `cond()`, `ifBody()`, `count()`).

**Decision needed.** Rename each pair to match its accessors. All
field renames are free (private members; nm cannot encode them).

| Batch | Class | first→ | second→ |
|---|---|---|---|
| 04a | `SeqCFunctionCall` | `funName_` | `args_` |
| 04a | `SeqCArray` | `array_` | `index_` |
| 04a | `SeqCCaseEntry` | `label_` | `body_` |
| 04a | `SeqCSwitchCase` | `cond_` | `body_` |
| 04a | `SeqCIfCondition` | `cond_` | `ifBody_` |
| 04a | `SeqCWhileLoop` | `cond_` | `body_` |
| 04a | `SeqCDoWhile` | `body_` | `cond_` (note swap! see 04a §3) |
| 04a | `SeqCRepeat` | `count_` | `body_` |

Anchor: `04a_seqc_ast_node.md:138-145`. Plus the `SEQC_BINARY_IMPL`
macro definition and ctor param names; the macro expansion currently
produces the `first`/`second` names — update macro itself.

**[Decision: approved. All 8 binary AST node `first_`/`second_`
field pairs renamed per the table; update `SEQC_BINARY_IMPL` macro
and ctor param names accordingly.]**

---

### Cluster H — `clone()` overrides should be `doClone()`

**Concern.** `DeviceTypeImpl::clone` (binary mangle: `doClone`) and
all 32+ subclass overrides in `device_*.cpp` are named `clone()` but
the binary's mangled symbol table has `doClone()`. The base name is
authoritative per RULES §3 — this is the original name.

**Decision needed.** Trivial — rename to match binary.

| Batch | Symbol | Conf |
|---|---|---|
| 29 | `DeviceTypeImpl::clone()` | high |
| 41 | every `<Subclass>::clone()` override (×32+) | high |

Anchor: `29_device_type.md:35`, `41_device_subclasses.md:49`.
Mechanical replace; vtable atomic.

**[Decision: approved. `clone()` → `doClone()` for base + all
×32 subclass overrides (×33 total).]**

---

### Cluster I — `sfc::*Option::Bit0xNNNN` enumerator placeholders

**Concern.** Across ~39 enumerators in `mf_sfc.cpp` and the
device-type-Z subsystem, enumerators are named `Bit0xNNNN` from the
disassembly bit literal. The trailing comment near each enumerator
typically gives the real semantic name. Consumer tables in
`device_mf.cpp` reference these by enumerator; they all need to
move atomically.

**Decision needed.** Per-enumerator semantic name selection. Audit
cannot pre-decide the 39 names without re-reading each comment;
this is a **focused micro-cluster pass** to do at execution time.

| Batch | Scope | Conf |
|---|---|---|
| 29 | `sfc::*Option::Bit0xNNNN` enumerator family | high |
| 41 | consumer tables `kMfliKnownOptions`, `kMfiaKnownOptions` in `src/device_mf.cpp` | high (lockstep) |
| 54 | doc-comment cross-refs in `mf_sfc.cpp` | medium |

Anchor: `29_device_type.md:27`, `54_mf_sfc.md`.

**[Decision: defer per-enum semantic naming to execution time. The
cluster as a whole is approved; individual enumerator names are
chosen per-bit during the dedicated micro-cluster pass.]**

---

### Cluster J — `Waveform`/`WaveformFile` JSON-key drift

**Concern.** Multiple Waveform-family fields are named after
internal hypotheses but the binary writes them under different
keys in the JSON serializer (`.waveforms` ELF section).

| Batch | Symbol | JSON key (binary) | Decision |
|---|---|---|---|
| 14 | `WaveformFile::data` | `"hash"` (SHA-1) | rename → `hash`/`fileHash` |
| 14 | `Waveform::secondaryName` | `"functionArgs"` | rename → `functionArgs` |
| 14 | `Waveform::playWord` | `"playConfig"` | rename → `playConfig` |
| 14 | `Waveform::seqRegWidth` | (per 14 §3) | rename → `minLengthSamples` |
| 14 | `Waveform::Waveform(13-arg)::secondaryName_` ctor param | inherited | `functionArgs_` |
| 14 | `Waveform::Waveform(13-arg)::playWord_` ctor param | inherited | `playConfig_` |
| 14 | `Waveform::Waveform(13-arg)::seqRegWidth_` ctor param | inherited | `minLengthSamples_` |
| 14 | `Waveform::fromJson::secondaryNameStr` (local) | inherited | `functionArgsStr` |
| 14 | `Waveform::Waveform(13-arg)::allocationByteSize_` | unclear | keep (per 14) |

**Decision needed.** Pure trivial — rename to match the JSON keys
(tier-2 authoritative per RULES §4d/3). All decisions already made
by the audit.

**[Decision: approved. All 9 Waveform JSON-key renames per the
table.]**

Anchor: `14_waveform.md:20-63`.

---

### Cluster K — `playWord`/`PlayConfig` producer ↔ field swap

**Concern.** Three params of `AsmCommands::genPlayConfig` write into
`PlayConfig` fields under *swapped* names. The fields are
JSON-anchored (positive evidence in batch 38) so the field names are
correct; the producer parameter names are wrong.

| Batch | Producer param | Writes to PlayConfig field | Decision |
|---|---|---|---|
| 10 | `genPlayConfig::isFourChannelBool` | `PlayConfig::now` | rename param → `playNow` (or `now`/`isNow`) |
| 10 | `genPlayConfig::isBool` (yes/high) | `PlayConfig::hold` | rename param → `hold` / `isHoldFlag` |
| 10 | `genPlayConfig::holdCount` | `PlayConfig::rate` | rename param → `rate` |
| 10 | `genPlayConfig::isHoldMode` | `PlayConfig::is4Channel` | rename param → `is4Channel` |

Plus the parallel `asmPlay`/`asmTable` family in batch 10:

| Batch | Symbol | Decision |
|---|---|---|
| 10 | `asmPlay::holdCount` | → `rate` |
| 10 | `asmPlay::isHoldMode` | → `is4Channel` |
| 10 | `asmTable::{tableIndex,wvf,nameIndex,isHold,fourChannel,holdCount,suppress,isHoldMode,reg,regVal}` | rename in lockstep |

**Settled by:** batch 38 positive evidence on `PlayConfig::{now, hold,
rate, is4Channel}` (all JSON-keys). Batch 49 originally proposed but
`genPlayConfig` belongs to batch 10 (49 incidentally noted the
mis-attribution).

Anchor: `10_asm_commands.md:112-133`, `38_play_config.md`.

**[Decision: approved. All 4 `genPlayConfig` producer-param renames
plus the parallel `asmPlay`/`asmTable` family in lockstep.]**

---

### Cluster L — `AsmCommandsImpl::wvf/wvfi` param-name drift vs wrappers

**Concern.** The wrappers (`AsmCommands::wvf/wvfi`) call their args
`reg`/`length`; the impl-side overrides instead spell them
`waveReg`/`markerReg`/`waveIndex`. Wrapper truth wins (length is
the immediate, marker is a misnomer for "second register").

| Batch | Impl-side param | Wrapper name (truth) | Decision |
|---|---|---|---|
| 49 | `wvf::markerReg` | `dstReg` (or `secondReg`) | → `dstReg` |
| 49 | `wvf::waveIndex` | `length` | → `length` |
| 49 | `wvfi::markerReg` | `dstReg` | → `dstReg` |
| 49 | `wvfi::waveIndex` | `length` | → `length` |
| 49 | `AsmCommandsImplHirzel::wvfs::arg` | `length` | → `length` |
| 49 | `AsmCommandsImplHirzel::wvft::arg` | `length` | → `length` |

Anchor: `49_asm_commands_impl.md:42-47`. Vtable-coordinated.

**[Decision: approved. `markerReg` → `dstReg`; `waveIndex` → `length`;
vtable-coordinated across `wvf`/`wvfi`/`wvfs`/`wvft`.]**

---

### Cluster M — Type-decomposition: `namespace Assembler` + `AssemblerInstr` → single class

**Concern.** Reconstruction split a single binary class
`zhinst::Assembler` into two recon entities: a `namespace Assembler`
holding statics/enums, and a parallel `struct AssemblerInstr`.
Confirmed from both sides (batch 26 owner; batch 33 forward decl
in `assembleAsmList`).

**Decision needed.** Two-step refactor:
1. Recompose into `class Assembler { ... }` with the enums/statics
   as nested members and `AssemblerInstr` becoming the inner type
   (or merging into the class entirely — needs confirmation by
   re-reading binary RTTI).
2. Rename all forward declarations and includes accordingly.

Affects: `26_assembler.md:67-68`, `33_awg_assembler.md:19`. This is
**partly a structural fix, not just renaming**; confirm scope at
execution time.

**[Decision: defer to last in the Phase D sequence. GDB + RTTI
verification of the binary class shape required before any source
edit.]**

---

### Cluster N — `Resources::parent_` is grandparent (inverted ownership)

**Concern.** `Resources::parent_` (a strong `shared_ptr`) actually
holds the **grandparent**, while `Resources::parentWeak_` (weak)
is the true direct parent.

| Batch | Symbol | Conf | Decision |
|---|---|---|---|
| 19a | `Resources::parent_` | yes/high | → `grandparent_` (and re-evaluate strong/weak choice) |
| 19a | `Resources::parentWeak_` | unsure/medium (the *correct* one) | → `parent_` after the rename above |

This is a **two-step rename** (must move parent_→grandparent_ first,
then parentWeak_→parent_) to avoid name collision.

**Decision needed.** Confirm the rename pair. Audit recommends.
Note: 19b reports `GlobalResources::GlobalResources::parent` ctor
param as `cross-batch-arbitration` against this — settle by renaming
ctor param to `grandparent` to match.

Anchor: `19a_resources.md:122-124`.

**[Decision: approved. Two-step rename
(`parent_` → `grandparent_`, then `parentWeak_` → `parent_`).
`GlobalResources` ctor param `parent` → `grandparent` to match.]**

---

### Cluster O — Snake_case violations that should be camelCase

**Concern.** A few constants and locals violate the project's
camelCase convention. Trivial unblocking.

| Batch | Symbol | Decision |
|---|---|---|
| 01 | `kChannelTag_I` → `kChannelTagI` | low (style only) |
| 01 | `kChannelTag_Q` → `kChannelTagQ` | low |
| 05d | `waitWave/waitPlayQueueEmpty::asm_entry` → `asmEntry` | low |
| 05d | `error/info::result, msg, asm_entry` → `asmEntry` | low |

Anchor: `01_types.md:50-51`, `05d_custom_functions_playback.md:94-96`.

**[Decision: approved. All 4 snake_case → camelCase fixes.]**

---

### Cluster P — `kDevCervino` is misleadingly named "all-Cervino"

**Concern.** `kDevCervino` is documented as "all Cervino devices"
but is in fact only `0x005` = UHFLI alone. `kDevAllButUHF`
similarly excludes only UHFLI, not UHFQA.

| Batch | Symbol | Decision |
|---|---|---|
| 01 | `kDevCervino` | deprecate; alias of `kDevUHF` (already exists, see 01) |
| 01 | `kDevAllButUHF` | rename → `kDevAllButUHFLI` |
| 01 | `kDevUHF` (positive evidence) | keep as the canonical name |

Anchor: `01_types.md:90-95`.

**[Decision: approved. All 3 device-mask renames (deprecate
`kDevCervino` as alias of `kDevUHF`; `kDevAllButUHF` →
`kDevAllButUHFLI`; keep `kDevUHF`).]**

---

## §2. Open arbitrations (need user judgement)

These could not be decided within the audit. Each has two or more
plausible answers and either both could be right under different
readings, or the symbol-table evidence is silent.

### Arbitration 1 — `WavetableManager` `lineNr_` / `waveformCounter_` vs JSON keys `numDefs` / `numDefs2`

The fields are written by ctor params named `numDefs` / `numDefs2`
(matching the binary's JSON serializer keys — *tier-2 authoritative*),
but the field semantics (per 45 §3) are "source line number" and
"waveform definition counter", not literally "number of definitions".

**Two plausible truths:**
1. **JSON key wins** — fields rename to `numDefs_` / `numDefs2_`
   (matches binary serializer; user-visible).
2. **Semantic content wins** — keep `lineNr_` / `waveformCounter_`;
   rename ctor params *to* `lineNr` / `waveformCounter`.

The two readings are not reconcilable without GDB tracing or
inspection of the JSON consumer to see whether downstream code
treats those keys as line numbers or as counts. Audit recommends
**option 1 (JSON wins)** because tier-2 evidence is explicit, but
this is a borderline call.

Cross-refs: `45_wavetable_front.md:78-79`, `46_wavetable_ir.md:189-190`.

**[Decision: investigate first. Resolve at execution time after
inspecting JSON consumer behavior; no rename until resolved.]**

### Arbitration 2 — `DeviceConstants::numDIOBits` / `numOutputPorts` vs caller usage

| Batch | Field | Caller usage |
|---|---|---|
| 31 | `DeviceConstants::numDIOBits` | used by `configFreqSweep` as oscillator-count upper bound |
| 31 | `DeviceConstants::numOutputPorts` (already yes/high in 31) | used as bit-shift count → audit-proposed `execTableIndexBits` |

The 31-side already proposes `execTableIndexBits` for `numOutputPorts`.
The `numDIOBits` case is genuinely 50/50: either the field is
misnamed (real semantic = "max oscillator index") or the caller
(`configFreqSweep`) is using the wrong field.

**Decision.** Audit recommends:
- Rename `numOutputPorts` → `execTableIndexBits` (already yes/high).
- Investigate `numDIOBits` use at execution time before renaming
  (GDB trace `configFreqSweep` on a UHFLI test); flagged
  cross-batch-arbitration.

Cross-refs: `31_device_constants.md:39`,
`05c_custom_functions_io_part2.md:107`.

**[Decision: investigate first. GDB-trace `configFreqSweep` on UHFLI
before deciding `numDIOBits` rename. `numOutputPorts` →
`execTableIndexBits` already covered in §4.]**

**Phase R resolution (closed — fixed/kept).** The recon faithfully
matches the binary; no source change. Static disasm of
`configFreqSweep` at the bound check (binary 0x1546d9..0x1546f4):

```
1546d9:  mov  -0x70(%rbp), %rax           ; rax = devConst_ container
1546dd:  mov  0x8(%rax),   %rax           ; rax = devConst_ raw ptr
1546e1:  mov  0x84(%rax),  %eax           ; eax = *(devConst_ + 0x84)
1546e7:  dec  %eax                        ; eax = field - 1
1546e9:  xorps %xmm1, %xmm1
1546ec:  cvtsi2sd %eax, %xmm1             ; xmm1 = (double)(field - 1)
1546f0:  ucomisd %xmm1, %xmm0             ; oscIndex vs bound
1546f4:  ja   1550ed                       ; throw if >
```

Field at offset +0x84 is `DeviceConstants::numDIOBits` per
`device_constants.hpp:184`. GDB confirmation on SHFSG4 driving
`configFreqSweep(0, 100e6, 3)`: at 0x1546e9 `eax = 0x7`, i.e. the
field value is 8 → `numDIOBits - 1 = 7`. SHFSG carries 8 oscillators
**and** 8 DIO bits, so the binary genuinely loads the field named
`numDIOBits`.

Outcome: **fixed (kept)** — the recon's
`devConst_->numDIOBits - 1` is what the binary computes. Whether
the *original* author intended this field to also serve as the
oscillator-index bound, or whether the binary itself has a latent
logic bug there, is outside the scope of a binary-faithful
reconstruction: we mirror what the binary does. The "is this the
right field semantically?" question is reclassified as a behavioral
note about the original tool, not a recon defect. Trace evidence:
`/tmp/arb2_trace.txt`.

### Arbitration 3 — `DeviceConstants::waveformGranularity` / `waveformPageSize` are swapped

| Batch | Field | Used as | Decision |
|---|---|---|---|
| 31 | `waveformGranularity` (yes/high) | maximum, not grain | → `maxWaveformLength` |
| 31 | `waveformPageSize` (unsure/medium) | alignment grain | → `grainSize` |

Both batch-31-internal accessors `grainSize()` / `maxWaveformLength()`
already use the correct names (positive evidence — settles which
side is the misnomer). The cross-batch downstream chain at
`46_wavetable_ir.md:148` consumes both as locals named
`granularity`/`pageSize` and would need to flip names.

**Decision.** Audit-recommended. Two-step coordinated swap.

**[Decision: approved. Two-step coordinated swap —
`waveformGranularity` → `maxWaveformLength`;
`waveformPageSize` → `grainSize`. Update local consumers in
`46_wavetable_ir.md:148`.]**

### Arbitration 4 — `Compiler::usedSampleRate_` vs `StaticResources::usedSampleRate_`

The two fields mirror each other; `Compiler`'s is never written. One
of them is dead, or one is read by a path the audit didn't locate.

**Decision.** Investigate at execution time. Not safe to rename
either without resolving.

**[Decision: investigate first. No rename of either mirror until
resolved.]**

Cross-refs: `07_compiler.md:72`, `19a_resources.md:188`.

**Phase R resolution (closed).** Writer **found** by static disasm
scan: `Compiler::compile` @0x11f150 contains a single byte write to
`[r15+0x25]` at offset +0x2286 (binary address 0x1213d6):

```
1213c8:  mov    -0x110(%rbp),%rax     ; rax = staticResources raw ptr
1213cf:  movzbl 0xd8(%rax),%eax       ; eax = staticResources->usedSampleRate_
1213d6:  mov    %al,0x25(%r15)        ; compiler_->usedSampleRate_ = al
```

The Compiler-side field is **not dead** — it is a cache of the
StaticResources flag, copied across at the end of `compile()` just
before constructing the `CompileResult`. `StaticResources` is the
primary (set true inside `getValue("DEVICE_SAMPLE_RATE")`); the
Compiler mirror is read by `Compiler::usedDeviceSampleRate()`, which
`AWGCompilerImpl` consults when emitting the `.required_sample_rate`
ELF section.

Recon: copy added to `Compiler::compile` at the analogous point
(reconstructed/src/compiler.cpp Step 19b), with a public accessor
`StaticResources::usedSampleRate()` added in the header to expose the
private field through standard C++ access. **Status: fixed (writer
found, recon now mirrors binary).** Both names stay as-is — they
faithfully reflect the binary, and both fields are live.

Trace: `/tmp/arb4_trace.txt`.

### Arbitration 5 — `NodeMapItem::hasFast` is conflated with `AccessMode`

Used at `addNodeAccess` call sites where the same int slot is
sometimes a bool (`hasFast`) and sometimes an `AccessMode` enum
value. Either two semantically distinct things share one field, or
one of the two readings is wrong.

**Decision.** Investigate; possibly type fix rather than rename.
Anchor: `27_node_map_data.md:65`.

**[Decision: investigate first. Possibly a type fix (split slot)
rather than a pure rename.]**

**Phase R resolution (closed — see IF-112).** GDB-traced 51
`lookupNode` returns across the full test manifest; the byte at
`NodeMapItem+0x10` only ever held `0` (41 hits) or `1` (10 hits).
Static cross-check confirms `Custom(2)` enters `accessModeMap_`
exclusively via the explicit `mode` argument of `addNodeAccess`, never
via `hasFast`. Field stays `bool`; the
`AccessMode(node.hasFast)` cast at `custom_functions_play.cpp:1511`
is a deliberate dual use (false→Soft, true→Direct). Comments updated;
no structural change. **Status: rejected (bool is correct).**

### Arbitration 6 — `expression.cpp::createOrAppend{Arg,Decl,Param,Stmt}List::lhs/rhs`

Thin wrapper functions with `lhs`/`rhs` params; semantically the
roles are "existing list" / "new item". Decide between
`existing/newItem` (descriptive) vs keep `lhs/rhs` (consistent
with surrounding eval functions).

Anchor: `42_expression.md:103`.

**[Decision: keep `lhs` / `rhs` for consistency with surrounding
eval functions.]**

### Arbitration 7 — `mergeWaveforms::useYSuffix` (batch 05b)

Param also gates the merge/interleave step, not just the suffix.
Two plausible names (`useYSuffix` vs `interleaveSecondary`) under
different readings.

Anchor: `05b_custom_functions_play.md:55`.

**[Decision: investigate first.]**

### Arbitration 8 — `addCommand::cmd` / `args` swap (50 ↔ 24)

Names suggest one role; grammar/body shows the other. Either swap
the names or rename `args` → `cmdToken` and `cmd` → `argList`.

Anchor: `50_asm_parser_context.md:68`.

**[Decision: rename `args` → `cmdToken`, `cmd` → `argList`.]**

### Arbitration 9 — `AsmList::Asm::wavetableFront` (44, dual-purpose)

Field is used both as a `WavetableFront*` (semantic) and as a
"line number cache slot" (via the alias accessor `lineNumber()`).
Either split into two fields or accept dual-purpose with a new name
(`slot0x88` / `wavetableFrontIndex_`).

Anchor: `44_asm_list.md:34-37`.

**[Decision: investigate first.]**

### Arbitration 10 — `Cache::play::state` and `Cache::allocate(5-arg)::pageSize`

Two cache-API params receive cross-batch-arbitration flags from 36
because their values come from `Prefetch`-side fields that have
their own pending decisions (`PrefetcherNodeState::counter()` /
`Prefetch::maxBranches_`). Resolve after Cluster E is decided.

Anchor: `36_cache.md:55,75`.

**[Decision: resolve after Cluster C is committed.]**

### Arbitration 11 — `loopArgNodeAppend::arg` (04b)

Generic param name; cross-batch-arbitration to where loop-arg nodes
are created (likely 04a/04e SeqCFor handling).

Anchor: `04b_ast_evaluate_helpers.md:105`.

**[Decision: investigate first.]**

---

## §3. In-batch coordinated renames (no cross-batch coupling)

Multi-symbol renames internal to one batch.

### 3.1 `CustomFunctions::field_*` placeholder removal (05a)

`field_80`, `field_88`, `field_A8`, `field_B0` are libc++ unordered_map
internal-pad bytes that were reconstructed as named fields. They
can either be renamed semantically (`funcMap_maxLoadFactor_` etc.)
or **dropped from the recon entirely** (preferred — they are
recon artifacts, not binary state). 4 fields total.

### 3.2 `mergeWaveforms::paramN` placeholders (05a)

`param2`/`param3`/`param5`/`param6` of `mergeWaveforms` — non-descript
disassembly placeholders. Audit-proposed (low conf) names:
`numChannels`, `multiValue`, `firstArgIndex`, `strict`. Confirm by
GDB trace before commit.

### 3.3 `AWGCompilerImpl::string_*` (28)

| Symbol | New name |
|---|---|
| `AWGCompilerImpl::string_200_` | `sourceFilename_` |
| `AWGCompilerImpl::string_230_` | `sourceText_` |
| `AWGCompilerImpl::string_248_` | `assemblerText_` |

3 fields; high confidence.

### 3.4 `CachedParser::cacheFile::markers` / `markerBitsVec` swap (15)

Field names are swapped relative to the `.marker_bits`/`.marker`
ELF sections they end up in. Two-symbol swap.

### 3.5 `setSinePhase` / `incrementSinePhase` disasm-leakage locals (05c)

`rbx`, `ecx`, `resultPtr` are register-name leakage from disassembly
with no semantic role (and `resultPtr` is dead). Drop or rename to
proper locals.

### 3.6 `writeToNode::d/d2/d3/dHi/dF2`, `vec/v` (05b)

Per-block locals shadowing each other under nondescript names.
Audit suggests `valDouble`, `addiVec` etc. — low confidence. Could
be deferred.

### 3.7 `setPRNGSeed` / `executeTableEntry` / `setSweepStep` shadow locals (05c)

Multiple sibling functions use shadowed `oscIndex`/`numOutputPorts`
patterns. Coordinate the rename and see Arbitration 2.

### 3.8 `configFreqSweep` magic-literal constants (05c)

Replace `0x8e/0x8f/0x90/0x91/0x8c` with already-existing `kSuserSweep*`
constants. **This is a fix, not just a rename** — it removes magic
numbers.

### 3.9 Two-child SeqC node ctor params (Cluster G — covered above)

### 3.10 `AsmExpression::lineNumber()`/`labelType()`/`labelPc()` (24)

Covered in Cluster E.

---

## §4. High-confidence single renames (yes / high)

35 unambiguous one-symbol renames, grouped by batch. Each row points
to its §3 anchor in the source report for the full evidence trail.

| Batch | Symbol | → Proposal |
|---|---|---|
| 03 | `WaveformGenerator::readInt::argIndex` | header/body mismatch — see 03 §3; coordinated within 03 |
| 03 | `WaveformGenerator::readPositiveInt::argIndex` | same |
| 03 | `WaveformGenerator::genericTriangle::nPeriods` | actually amplitude (3-arg path) |
| 04a | `SeqCAstNode::SeqCAstNode::type` (covered Cluster F) | `lineNr` |
| 04a | `SeqCAstNode::type()` accessor (covered Cluster F) | `lineNr()` |
| 05a | `CustomFunctions::field_80/88/A8/B0` (covered §3.1) | (drop) |
| 05a | `mergeWaveforms::param{2,3,5,6}` (covered §3.2) | semantic |
| 05c | `waitDIOTrigger::supported`/`ecx` | drop disasm-leakage |
| 05c | `setSinePhase::resultPtr`/`rbx` | drop |
| 07 | `Compiler::channelMode_` | `usedFourChannelMode_` |
| 09 | `Prefetch::isHirzel_()` (Cluster C) | drop alias |
| 09 | `Prefetch::set_isHirzel_()` (Cluster C) | drop alias |
| 09b | `Prefetch::assignLoad::flag` (Cluster C) | `isHirzel` |
| 09b | `Prefetch::insertPlay::flag` | `indexed` |
| 10 | `AsmCommands::br/brz/brnz/brgz::flag` (Cluster A) | `isWaveformCmd` (or canonical from B) |
| 10 | `AsmCommands::genPlayConfig::isBool` (Cluster K) | `hold` |
| 10 | `AsmCommands::genPlayConfig::holdCount` (Cluster K) | `rate` |
| 10 | `AsmCommands::asmPlay::holdCount` (Cluster K) | `rate` |
| 13 | `printOpcode::format` (header) → `startIndex` (body) | unify on `startIndex` |
| 14 | `WaveformFile::data` (Cluster J) | `hash` |
| 14 | `Waveform::secondaryName` (Cluster J) | `functionArgs` |
| 14 | `Waveform::playWord` (Cluster J) | `playConfig` |
| 16 | `WaveformIR::getSampleCount()` | `getAllocationByteSize` (returns bytes!) |
| 19a | `VarSubType_Bool` (legacy alias) | remove |
| 23 | `AWGCompilerConfig::appendMode()` (Cluster E) | drop |
| 23 | `AWGCompilerConfig::splitIndex()` (Cluster E) | drop |
| 23 | `AWGCompilerConfig::syncVersion()` (Cluster E) | drop |
| 24 | `AsmExpression::lineNumber()` alias (Cluster E) | drop |
| 28 | `AWGCompilerImpl::string_200_/230_/248_` (§3.3) | sourceFilename_ etc. |
| 28 | `AWGCompilerImpl::compileString::opcodeCount` | `opcodeWordCount` |
| 29 | `DeviceTypeImpl::clone` (Cluster H) | `doClone` |
| 29 | `DeviceOption::TenG` | `Option10G` |
| 29 | `DeviceOption::Sixteen_W` | `Option16W` |
| 29 | `sfc::*Option::Bit0xNNNN` family (Cluster I) | semantic per-bit |
| 31 | `DeviceConstants::numOutputPorts` | `execTableIndexBits` |
| 32 | `FrontendLoweringState::pad10_` | `inFunctionDef_` |
| 33 | `AssemblerInstr` (forward decl) (Cluster M) | `Assembler` |
| 41 | `<Subclass>::clone` (×32) (Cluster H) | `doClone` |
| 42 | `Expression::valueType` | `direction` (carries `EDirection`) |
| 42 | `createFunction::nameExpr` | `returnTypeExpr` |
| 46 | `WaveOrder::ByName` | `ByWaveIndex` (sort by waveIndex, not name) |
| 46 | `WavetableIR::allocateWaveforms::totalSamples` | `totalBytes` (or delete dead) |
| 46 | `allocateWaveforms::memorySizeInSamples` | `memorySizeBytes` |
| 46 | `WavetableIR::updateWaveforms::allocFlag` | `fifoMode` |
| 47 | `ElfWriter::addWaveform::useAbsolute` | `useMapped` (polarity inverted!) |
| 52 | `CompilerMessage::str::showLine` | `hideLine` (polarity inverted!) |

---

## §5. Medium-confidence single renames (yes / medium)

111 rows — see `/tmp/audit_yes_singleton.txt` line-grep
`'\| medium \|'` for the exhaustive list. Highlights:

- `04b_ast_evaluate_helpers.md`: `kRangeLo`/`kRangeHi` →
  `kInt32MaxAsDouble`/`kUint32MaxAsDouble`; `scaleWaveform::scaleFactor` (dead).
- `04c_ast_evaluate_arith.md`: `SeqCAssign::aux`.
- `04d_ast_evaluate_logical.md`: `lhsHas1`/`rhsHas1` mean "exactly one".
- `04e_ast_evaluate_control.md`: `childHadError` → `childUnwound`,
  `lastEval`, `hasErrorOrNull`.
- `09b_prefetch_part2.md`: `useDA` local sweep.
- `15_cached_parser.md`: `valid_` is pin-bit, `fileSize_` is byte budget.
- `17_waveform_front.md`: `WaveformFront::values` → `genArgs_`.
- `19a_resources.md`: 7 yes/medium ranging over `Resources::*`.
- `20_node.md`: `Node::swap::devIdx` → `ancestorAsmId`.
- `28_awg_compiler.md`: `compileString` various.
- `34_eval_results.md`: `EvalResults::hasError_` → "return-encountered".
- `42_expression.md`: 2 yes/medium.
- `45_wavetable_front.md`: `address2_` (never read).
- `46_wavetable_ir.md`: 5 yes/medium.

The full set is mechanical — each entry has its own §3 detail block
in its source report.

---

## §6. Low-confidence and unsure (parked) — reconciled in Phase S.1

**Original audit count**: 38 yes/low + 188 unsure = 226 rows.
These are the *long tail*: stylistic underscore consistency, cosmetic
abbreviations, names that fit usage *locally* but might be improved
given fuller context. The original recommendation was **defer to a
future style/cosmetic pass**; that pass became Phase Q in the scoping
note `phase_r_leftovers_and_q_scoping.md`.

A few unsure flags carry semantic risk and *should* be revisited
(unchanged from original §6):

- `02_memory_allocator.md`: `memorySizeInSamples_` and
  `allocateCLAligned::size` — bytes-vs-samples convergent unit
  question. Cross-batch with 14, 36, 46.
- `07_compiler.md`: `usedSampleRate_`, `flags_`, `reserved18_`,
  `sourceFiles_` — declared but unused; might be dead.
- `11_value.md`: `Value::pad_04_` — the byte at +0x04 is *not*
  padding; it is `subType_`-shaped. Cross-batch with 12.
- `27_node_map_data.md`: `NodeTypeIdx::RawDoubleLow32` — case-3 path
  not exhaustively traced.
- `38_play_config.md`: `PlayConfig::now` — naming from binary
  serializer; consumers read it as a 4-channel flag. Type/semantic
  questionable.
- `44_asm_list.md`: `Asm::wavetableFront` (Arbitration 9).

### Phase S.1 reconciliation (2026-04-29)

The 226-item backlog has been walked row-by-row against Phase D
commits `d15ad32..9b2e690` (20 commits) and Phase R commits
`dfc278e..e073228` (15 commits incl. wrap-up), and against the
current source tree under `reconstructed/src` and
`reconstructed/include`.  Every row was assigned to one of the four
buckets defined in the scoping note:

| Bucket | Meaning | Count |
|---|---|---|
| **B1** | Mechanical sweep — concrete single rename / removal target | **15** |
| **B2** | Borderline preference — audit hedged with "or keep current" | **114** |
| **B3** | Already resolved during Phase D/R — symbol gone or renamed | **39** |
| **B4** | Wontfix — JSON contract, hardware-address constant, or audit verdict was "keep current" with no actionable target | **58** |
| **Total** | | **226** |

**Divergence from scoping note** (which estimated ~80/60/30/56 for
B1/B2/B3/B4): the scoping note was sampled, not enumerated. The
actual data shows B1 is ~5× smaller and B2 is ~2× larger than
estimated. Cause: most of the obviously-mechanical items the scoping
note used as B1 examples (snake_case `kChannelTag_I`, dead pad
fields, `parent_/parentWeak_` swap, `Cache::appendMode_`) were
**already resolved in Phase D/R clusters** (Cluster O, N, C, etc.),
so they correctly land in B3 now. What remains in B1 is a much
smaller true mechanical residue. The B2 inflation reflects how many
rows the audit recorded as "unsure with `keep current` as one
option" — these need per-item judgement and don't fold into a sweep.

**Triage method** (programmatic, then spot-checked):

1. Extract the symbol(s) from each row's first cell.
2. Check whole-word presence in `reconstructed/{src,include}` via
   `rg -wF`. Symbols absent from source → candidate B3.
3. Pattern-match the symbol cell and audit proposal against:
   - hardware-constant prefixes (`kSuser*`, `kAddr*`, `kDev*`,
     `kRate*`, `kNo*`, `kChannelTag*`, `DeviceOpts::`) → B4 if
     audit said "keep" or "remove if dead";
   - JSON-contract mentions in the comment → B4;
   - known cluster-resolution mappings (Cache::appendMode_ →
     Cluster C, parentWeak_ → Cluster N, kChannelTag_* →
     Cluster O snake-case fix, DeviceOpts → IF-121,
     PlayConfig::now → IF-114, AddressImpl → IF-118,
     Cache::Pointer::hash_ → IF-113, pad_04_ → IF-110,
     NodeMapItem::hasFast → IF-112+Arb5, configFreqSweep → IF-120)
     → B3;
   - disasm-leakage register names (rax/rbx/...) → B1;
   - `regInv` literal → B1 (M2 cluster);
   - audit proposal contains "drop" / "consider removal" → B1
     (M5 dead-param cleanup);
   - snake_case identifiers with concrete camelCase target → B1;
   - 1-2 letter param/local with single concrete proposal → B1;
   - everything else → B2.
4. Manually verified a handful of B3 hits (`Cache::appendMode_`,
   `compileString::s`, `pad34_`/`pad_108_` — the latter two stay
   in source so they're B4 not B3) by direct `rg` against the
   tree.

### Per-item table

The full 226-row triage table is below. Each row's "Status / evidence"
records either the resolving commit (B3), the wontfix rationale (B4),
the proposed S.2 micro-cluster (B1), or "borderline; needs per-item
judgement" (B2).

| Item (file:line) | Symbol(s) | Audit proposal | Bucket | Status / evidence |
|---|---|---|---|---|
| `04d_ast_evaluate_logical.md:89` | `SeqCMod::evaluate::errLhs`, `errLhs`/`errRhs` | `lhsTypeForError`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `04d_ast_evaluate_logical.md:96` | `SeqCInc::evaluate::newReg`, `srcReg`, `srcReg2`, `dstReg` | `srcRegReread`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `04d_ast_evaluate_logical.md:98` | `SeqCInc::evaluate::oldVal`, `newVal`, `newRhsVal`, `newVal1`, `newVal2`, `rh... | `oldCvarValue`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `04d_ast_evaluate_logical.md:111` | `SeqCNeg::evaluate::val`, `d` | `negatedDouble`, keep current | **1** | short-letter local with concrete target — micro-cluster M4 |
| `04d_ast_evaluate_logical.md:121` | `SeqCNotExpr::evaluate::cmds` | `asmCmds`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `10_asm_commands.md:90` | `AsmCommands::smap::r1`,`r2`,`arg` (params) | value, addr | **1** | disasm-leakage local — micro-cluster M1 |
| `10_asm_commands.md:94` | `AsmCommands::syncCervino::reg1`,`reg2`,`flag` (params) | flag candidates listed | **2** | borderline naming preference; needs per-item judgement |
| `10_asm_commands.md:111` | `AsmCommands::genPlayConfig::fourChannel` (param) | unused/consider removal | **1** | audit suggested removal — micro-cluster M5 (dead-param cleanup) |
| `10_asm_commands.md:123` | `AsmCommands::asmPlay::fourChannel` (param) | playNow-or-fourChannel | **2** | borderline naming preference; needs per-item judgement |
| `21_elf_reader.md:22` | `ElfReader::ddSectionIndex_` | keep current, `unknownDword_` | **2** | borderline naming preference; needs per-item judgement |
| `21_elf_reader.md:33` | `ElfReader::Line::addr` | keep current, `instruction` | **2** | borderline naming preference; needs per-item judgement |
| `04c_ast_evaluate_arith.md:80` | `SeqCOperator::evaluate(3-arg)::lv` / `::rv` | `lhsValues`, `rhsValues` (low); keep current (medium) | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `04c_ast_evaluate_arith.md:86` | `SeqCValue::evaluate::t` | `tag` (medium); keep current (low) | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `04c_ast_evaluate_arith.md:129` | `SeqCAssign::evaluate::sp`/`mp` | `samplePtr`, `markerPtr` (medium); keep current (low) | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `04c_ast_evaluate_arith.md:131` | `SeqCAssign::evaluate::rt`/`rs` | `rhsType`, `rhsSub` (medium); keep current (low) | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `04c_ast_evaluate_arith.md:139` | `SeqCPlus::evaluate::lhsT`/`rhsT` | `lhsType`, `rhsType` (low); keep current (medium, scope-l... | **2** | borderline naming preference; needs per-item judgement |
| `04c_ast_evaluate_arith.md:153` | `SeqCMinus::evaluate::lhsT`/`rhsT` | as SeqCPlus | **2** | borderline naming preference; needs per-item judgement |
| `04c_ast_evaluate_arith.md:164` | `SeqCDiv::evaluate::lhsT`/`rhsT` | as SeqCPlus | **2** | borderline naming preference; needs per-item judgement |
| `04c_ast_evaluate_arith.md:165` | `SeqCDiv::evaluate::rhsCheck` | `rhsDouble` (low); merge with `rhsDouble` below (medium);... | **2** | borderline naming preference; needs per-item judgement |
| `33_awg_assembler.md:28` | `AWGAssembler::printOpcode::format` | keep current (low), `mode` (low) | **2** | borderline naming preference; needs per-item judgement |
| `09_prefetch_part2.md:68` | `Prefetch::insertPlay::addrA` | `firstAddr`, keep | **2** | borderline naming preference; needs per-item judgement |
| `09_prefetch_part2.md:69` | `Prefetch::insertPlay::addrB` | `secondAddr`, `xnorMask`, keep | **2** | borderline naming preference; needs per-item judgement |
| `09_prefetch_part2.md:71` | `Prefetch::findPlaceholder::out`/`asmList` | unify on `asmList` | **2** | borderline naming preference; needs per-item judgement |
| `45_wavetable_front.md:41` | `WavetableFront::oss_` | keep current; `errorStream_` | **4** | audit verdict was keep current; no actionable target |
| `45_wavetable_front.md:45` | `WavetableFront::warningCallbackStorage_` | keep current; merge into above | **4** | audit verdict was keep current; no actionable target |
| `45_wavetable_front.md:71` | `WavetableFront::checkWaveformInitialized::{nameCopy,wf,wf2}` | keep current | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `04b_ast_evaluate_helpers.md:73` | `constWaveform::value` | sampleAmplitude (low), keep current (medium) | **2** | borderline naming preference; needs per-item judgement |
| `04b_ast_evaluate_helpers.md:83` | `computeMult::res` | unusedRes (low), keep current (medium) | **2** | borderline naming preference; needs per-item judgement |
| `04b_ast_evaluate_helpers.md:94` | `valueToBool::result` (param) | input (low), value (low), keep current (medium) | **2** | borderline naming preference; needs per-item judgement |
| `04b_ast_evaluate_helpers.md:98` | `invertBool::result` (param) | input (low), keep current (medium) | **2** | borderline naming preference; needs per-item judgement |
| `20_node.md:59` | `Node::trig` | keep current; `trigger` | **4** | JSON contract — renaming would break output |
| `20_node.md:62` | `NodeType::SetVarPlaceholder`..`Wait` | keep current | **4** | audit verdict was keep current; no actionable target |
| `20_node.md:75` | `Node::updateParent::ch` | `branches` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `20_node.md:96` | `Node::toJson::idMap` | keep current; `seqIdToJsonId` | **4** | audit verdict was keep current; no actionable target |
| `20_node.md:102` | `Node::toJson::remappedId` | keep current; `nodeIdJson` | **4** | JSON contract — renaming would break output |
| `20_node.md:109` | `Node::fromJson::nId`/`aId`/`devIdx` | `nodeIdJson`/`asmIdJson`/`deviceIndex`; keep current | **2** | borderline naming preference; needs per-item judgement |
| `20_node.md:111` | `Node::fromJson::cfg1`/`cfg2` | `config`/`currentCwvf`; keep current | **2** | borderline naming preference; needs per-item judgement |
| `09_prefetch.md:66` | `Prefetch::minIndexedSize` (static) | keep current, `minIndexedCacheSize` | **2** | borderline naming preference; needs per-item judgement |
| `09_prefetch.md:78` | `PrefetcherNodeState::refTrack` | keep current, `refCount` | **2** | borderline naming preference; needs per-item judgement |
| `09_prefetch.md:82` | `PrefetcherNodeState::useDA` | keep current, `crossesCacheLine` | **2** | borderline naming preference; needs per-item judgement |
| `44_asm_list.md:52` | `AsmList::operator=(vector<Asm>)::v` (param) | `entries`; keep | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `44_asm_list.md:61` | `AsmList::parseStringToAsmList::imm1`,`imm2` (locals) | drop; rename to descriptive | **1** | audit suggested removal — micro-cluster M5 (dead-param cleanup) |
| `19b_resources_supplementary.md:43` | `Function::Function::rt` (param) | `returnType` (low), keep current (low) | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `19b_resources_supplementary.md:60` | `Function::addArgument::temp` (local) | `arg`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `19b_resources_supplementary.md:63` | `Function::addArguments::p` (local) | `exprPtr`, keep current | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `19b_resources_supplementary.md:67` | `Function::addArguments(SeqCAstNode)::ps` (local) | `paramNodes`, keep current | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `19b_resources_supplementary.md:77` | `Function::resetScope::fullName` (local) | `concatenatedName`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `19b_resources_supplementary.md:111` | `StaticResources::StaticResources::target` (local) | `fnTarget`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `19b_resources_supplementary.md:113` | `StaticResources::~StaticResources::target` (local) | `fnTarget`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `19b_resources_supplementary.md:124` | `StaticResources::init::n` (local) | `numOutputPorts`, `portCount` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `19b_resources_supplementary.md:125` | `StaticResources::init::base` (local) | `baseCode`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `31_device_constants.md:14` | `DeviceConstants::SuserAddr` (nested type) | keep current, `SuserAddress` | **2** | borderline naming preference; needs per-item judgement |
| `31_device_constants.md:16` | `DeviceConstants::hasExtendedReg` | keep current, `isHdawg` | **2** | borderline naming preference; needs per-item judgement |
| `31_device_constants.md:35` | `DeviceConstants::waveformMemSize` | keep current, `maxWaveformCount` | **2** | borderline naming preference; needs per-item judgement |
| `31_device_constants.md:46` | `DeviceConstants::maxDioTableEntries()` | keep current | **4** | audit verdict was keep current; no actionable target |
| `31_device_constants.md:50` | `Register::SyncRegA`, `SyncRegB` (enumerators) | keep current | **4** | audit verdict was keep current; no actionable target |
| `30_awg_device_props.md:94` | `AwgDeviceProps::fpgaRevisionPattern` | `revisionPattern`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `04a_seqc_ast_node.md:109` | `SeqCValue::pad34_` | keep current; `reserved34_` | **4** | audit verdict was keep current; no actionable target |
| `19a_resources.md:83` | `VarSubType::VarSubType_Default` | keep current (medium), `VarSubType_None` (low) | **2** | borderline naming preference; needs per-item judgement |
| `19a_resources.md:87` | `VarSubType::VarSubType_String` | keep current (medium) | **4** | audit verdict was keep current; no actionable target |
| `19a_resources.md:94` | `Resources::State::Unset` / `Active` / `Paused` / `Locked` | keep current (medium) | **4** | audit verdict was keep current; no actionable target |
| `19a_resources.md:107` | `Resources::Function::weakRef_` | keep current (medium) | **4** | audit verdict was keep current; no actionable target |
| `19a_resources.md:166` | `Resources::getVariable::result` (local in §3 below) | keep current (medium) | **4** | audit verdict was keep current; no actionable target |
| `19a_resources.md:180` | `readString/readWave::tmp` (local) | keep current (medium) | **4** | audit verdict was keep current; no actionable target |
| `19a_resources.md:192` | `StaticResources::pad_108_` | keep current (medium) | **1** | snake_case → camelCase — micro-cluster M3 |
| `42_expression.md:76` | `Expression::pad0C_` | keep current, `unused0C_` | **2** | borderline naming preference; needs per-item judgement |
| `42_expression.md:85` | `EOperator::eNONE` | keep current | **4** | audit verdict was keep current; no actionable target |
| `42_expression.md:91` | `createString::s` | `text`, keep current | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `42_expression.md:99` | `createArray::lhs` / `rhs` | `arrayExpr`, `indexExpr`; keep current | **2** | borderline naming preference; needs per-item judgement |
| `42_expression.md:112` | `createSwitch::val` / `body` | `selector`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `42_expression.md:113` | `createCase::val` / `body` | `label`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `29_device_type.md:25` | `sfc::FeaturesCode::value` (field) | keep current, `bits` | **2** | borderline naming preference; needs per-item judgement |
| `29_device_type.md:51` | `DeviceType::ctor(family,options)::options` | keep current, `optionsMask` | **2** | borderline naming preference; needs per-item judgement |
| `29_device_type.md:54` | `DeviceType::belongsTo::f` | `family` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `29_device_type.md:62` | `toDeviceTypeCode::s` (param) | `name`, `s` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `29_device_type.md:63` | `toDeviceFamily::s` (param) | `name`, `s` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `29_device_type.md:64` | `toDeviceOption::s` (param) | `name`, `s` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `29_device_type.md:66` | `splitDeviceOptions::s` (param) | `optionsStr`, `s` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `29_device_type.md:72` | `toDeviceTypeCode::codes` (local static) | keep current, `deviceTypeCodes` | **2** | borderline naming preference; needs per-item judgement |
| `54_mf_sfc.md:22` | `bitIf` (free fn, anon ns) | keep current, `bitFromBool`, `shiftBit` | **2** | borderline naming preference; needs per-item judgement |
| `52_compiler_message.md:35` | `CompilerMessage::lineNr` | keep current, `line_nr` | **2** | borderline naming preference; needs per-item judgement |
| `52_compiler_message.md:48` | `CompilerMessageCollection::parserMessage::line` | keep current, `code`, `errorCode` | **2** | borderline naming preference; needs per-item judgement |
| `52_compiler_message.md:53` | `CompilerMessageCollection::setLineNr::nr` | keep current, `lineNr` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `28_awg_compiler.md:123` | `AWGCompiler::writeToStream::format` (param) | `outputName`, `filename`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `28_awg_compiler.md:136` | `AWGCompilerImpl::string_218_` | `pad_218_`, keep current | **1** | snake_case → camelCase — micro-cluster M3 |
| `28_awg_compiler.md:155` | `AWGCompilerImpl::compileString::s` (local) | `instrText`, keep current | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `28_awg_compiler.md:173` | `AWGCompilerImpl::writeToStream::format` (param) | `outputName`, `filename`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `41_device_subclasses.md:50` | `(anon)::buildInlineShfOptions` (free fn, device_shf.cpp) | keep current; `buildShfOptionsInline` | **4** | audit verdict was keep current; no actionable target |
| `41_device_subclasses.md:56` | `(anon)::buildVhfFf` (free fn, device_vhf.cpp) | keep current; `buildVhfFfOption` | **4** | audit verdict was keep current; no actionable target |
| `41_device_subclasses.md:57` | `(anon)::buildGhfFf` (free fn, device_ghf.cpp) | keep current; `buildGhfFfOption` | **4** | audit verdict was keep current; no actionable target |
| `41_device_subclasses.md:58` | `(anon)::buildShfaccFf` (free fn, device_shfacc.cpp) | keep current; `buildShfaccFfOption` | **4** | audit verdict was keep current; no actionable target |
| `53_wave_index_tracker.md:35` | `WaveIndexTracker::maxIndex` (field) | `maxIndex_`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `53_wave_index_tracker.md:36` | `WaveIndexTracker::indices_` (field) | `usedWaveIndices_`, `usedIndices_`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `53_wave_index_tracker.md:38` | `WaveIndexTracker::WaveIndexTracker(int)::maxIdx` (param) | `maxIndex`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `53_wave_index_tracker.md:39` | `WaveIndexTracker::WaveIndexTracker<T>(int,…)::maxIdx` (param) | `maxIndex`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `07_compiler.md:67` | `Compiler::flags_` | `unknownFlags_`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `07_compiler.md:69` | `Compiler::reserved18_` | keep current, `unknown18_` | **2** | borderline naming preference; needs per-item judgement |
| `07_compiler.md:76` | `Compiler::sourceFiles_` | keep current | **4** | audit verdict was keep current; no actionable target |
| `07_compiler.md:105` | `Compiler::getLineMap::seq` (local) | keep current, `labelSeq` | **2** | borderline naming preference; needs per-item judgement |
| `07_compiler.md:148` | `LowerResult` (type) | keep current, `LowerOutput` | **2** | borderline naming preference; needs per-item judgement |
| `06_asm_register.md:19` | `AsmRegister::value` | `regNum` (low), keep current (medium) | **2** | borderline naming preference; needs per-item judgement |
| `06_asm_register.md:23` | `AsmRegister::AsmRegister(int)::v` | `regNum` (low), keep current (medium) | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `06_asm_register.md:25` | `AsmRegister::AsmRegister(int)` (1-arg)::n | `regNum` (low), keep current (medium) | **2** | borderline naming preference; needs per-item judgement |
| `06_asm_register.md:28` | `AsmRegister::Reg::n` | `regNum` (low), keep current (medium) | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `06_asm_register.md:31` | `AsmRegister::toInt` | keep current (medium), drop in favor of `operator int` (low) | **1** | audit suggested removal — micro-cluster M5 (dead-param cleanup) |
| `17_waveform_front.md:22` | `WaveformFront::useCount_` | keep current; `refCount_` | **4** | audit verdict was keep current; no actionable target |
| `27_node_map_data.md:36` | `NodeTypeIdx` (enum) | keep current; `NodeValueEncoding` | **4** | audit verdict was keep current; no actionable target |
| `27_node_map_data.md:38` | `NodeTypeIdx::SinePair` | keep current; `IqPair` | **4** | audit verdict was keep current; no actionable target |
| `27_node_map_data.md:40` | `NodeTypeIdx::RawDoubleLow32` | keep current | **4** | audit verdict was keep current; no actionable target |
| `27_node_map_data.md:46` | `VirtAddrNodeMapData::compareEq::o` | `otherCast` / `rhs` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `27_node_map_data.md:55` | `kMul` | `kSplitmixMul` | **2** | borderline naming preference; needs per-item judgement |
| `27_node_map_data.md:58` | `DirectAddrNodeMapData::compareEq::o` | `otherCast` / `rhs` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `27_node_map_data.md:60` | `DirectAddrNodeMapData::clone::p` | `copy` / `cloned` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `27_node_map_data.md:63` | `NodeMapItem::typeIdx` | `typeCode`, `valueKind`; keep current | **2** | borderline naming preference; needs per-item judgement |
| `27_node_map_data.md:71` | `NodeMapItem::toJson::typeVal` | `sizeVal` | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `03_waveform_generator.md:90` | `WaveformGenerator::readWave::expectedLength` | keep current, expectedSize, minLength | **2** | borderline naming preference; needs per-item judgement |
| `39_math_compiler.md:30` | `MathCompiler::functionExists::found` (local) | `argCountMatches`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `05d_custom_functions_playback.md:34` | `appendSuser::vec` | `out`, `assemblers`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `05d_custom_functions_playback.md:45` | `playAuxWave::mask` | `suppressMask`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `05d_custom_functions_playback.md:55` | `playAuxWave::expectedLen` | `granularity`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `05d_custom_functions_playback.md:71` | `playDIOWave::mask` | `triggerMask`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `05d_custom_functions_playback.md:76` | `playDIOWave::expectedLen` | `granularity`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `01_types.md:46` | `zhinst::kRateInherit` | keep current; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `01_types.md:47` | `zhinst::kNoWaveIndex` | keep current; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `01_types.md:48` | `zhinst::kNoNodeId` | keep current; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `01_types.md:49` | `zhinst::kNoPlayIndex` | keep current; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `01_types.md:59` | `zhinst::kSuserNodeFreqCommit` | keep current | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `01_types.md:68` | `zhinst::kSuserUserRegBase` | keep current; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `01_types.md:70` | `zhinst::kSuserRTLoggerReset` | keep current; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `01_types.md:71` | `zhinst::kSuserRTLoggerResetHdawg` | keep current; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `01_types.md:81` | `zhinst::kSuserQAResultLen` | keep current; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `01_types.md:84` | `zhinst::kSuserSweepStartLo/Hi` | keep current; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `01_types.md:85` | `zhinst::kSuserSweepStepLo/Hi` | keep current; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `01_types.md:96` | `zhinst::kDevPreSHFLI` | keep current | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `02_memory_allocator.md:40` | `MemoryBlock::flags` | `flags`, `validityFlags` | **2** | borderline naming preference; needs per-item judgement |
| `26_assembler.md:75` | `RegOrder::None..DestImmSrc` (enumerators) | keep current (medium); see §3 | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `26_assembler.md:84` | `AssemblerInstr::outputs` (field) | keep current (medium); outputsOrZeroCheck (low) | **4** | audit verdict was keep current; no actionable target |
| `50_asm_parser_context.md:58` | `AsmParserContext::trackedStringCopy::s` | `text`, keep current | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `50_asm_parser_context.md:65` | `addNode::text` | `placeholderLine`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `38_play_config.md:52` | `PlayConfig::encodeCwvf::dummyFlag` (local) | dummyMode, holdOrDummy, keep current | **2** | borderline naming preference; needs per-item judgement |
| `15_cached_parser.md:50` | `CachedParser::cacheFile::sampleFormat` | keep current, `channelMask` | **2** | borderline naming preference; needs per-item judgement |
| `15_cached_parser.md:52` | `CachedParser::cacheFile::markerBits` | keep current, `configValue` | **2** | borderline naming preference; needs per-item judgement |
| `16_waveform_ir.md:55` | `WaveformIR::toJsonElement::format` | keep current | **4** | audit verdict was keep current; no actionable target |
| `25_asm_optimize.md:42` | `AsmOptimize::cancel_` (field) | keep current (medium); cancelCallback_ (low) | **4** | audit verdict was keep current; no actionable target |
| `25_asm_optimize.md:50` | `AsmOptimize::isLabelCalled::label,it` (params) | keep current; rename → `isLabelCalledBefore` (low); param... | **4** | audit verdict was keep current; no actionable target |
| `25_asm_optimize.md:72` | `splitConstRegisters::needsSplit` (local) | drop or use; flag (low) | **1** | audit suggested removal — micro-cluster M5 (dead-param cleanup) |
| `05c_custom_functions_io_part2.md:76` | `setPRNGRange::val0` / `val1` | drop val0/val1, hoist range vars | **1** | audit suggested removal — micro-cluster M5 (dead-param cleanup) |
| `05c_custom_functions_io_part2.md:98` | `configFreqSweep::arg0` | `oscArg`, keep current | **3** | resolved by `dbabd4e` (Phase R IF-120 (literals replaced)) |
| `05c_custom_functions_io_part2.md:99` | `configFreqSweep::arg1` | `startFreqArg` | **3** | resolved by `dbabd4e` (Phase R IF-120 (literals replaced)) |
| `05c_custom_functions_io_part2.md:100` | `configFreqSweep::arg2` | `stepFreqArg` | **3** | resolved by `dbabd4e` (Phase R IF-120 (literals replaced)) |
| `05c_custom_functions_io_part2.md:108` | `setSweepStep::arg0` / `arg1` | `oscArg`, `stepArg` | **2** | borderline naming preference; needs per-item judgement |
| `05c_custom_functions_io_part2.md:115` | `setOscFreq::arg0` / `arg1` | `oscArg`, `freqArg` | **2** | borderline naming preference; needs per-item judgement |
| `05c_custom_functions_io_part2.md:132` | `configureFeedbackProcessing` use of `0x20` literal for SHFSG | symbolic enum (low) | **2** | borderline naming preference; needs per-item judgement |
| `05c_custom_functions_io.md:40` | `appendSuser::vec` | keep current; `assemblers` (low) | **4** | audit verdict was keep current; no actionable target |
| `05c_custom_functions_io.md:55` | `getZSyncData::rawResult2`, `procAResult2`, `procBResult2` | `rawConst`, `procAConst`, `procBConst` | **2** | borderline naming preference; needs per-item judgement |
| `05c_custom_functions_io.md:57` | `getFeedback::supportsZSync` | `supportsProcessed` (low) | **2** | borderline naming preference; needs per-item judgement |
| `05c_custom_functions_io.md:63` | `assignWaveIndex::parseEnd` | `terminator`, `lastArgIt` | **2** | borderline naming preference; needs per-item judgement |
| `05c_custom_functions_io.md:69` | `assignWaveIndex::singleChannel` | keep current; `secondNameEmpty` (low) | **4** | audit verdict was keep current; no actionable target |
| `05c_custom_functions_io.md:76` | `wait::val` / `dval` | keep current | **4** | audit verdict was keep current; no actionable target |
| `05c_custom_functions_io.md:80` | `wait` `goto done;` (label `done`) | remove or `valid_done` | **2** | borderline naming preference; needs per-item judgement |
| `05c_custom_functions_io.md:82` | `waitTrigger::trigAddr` | keep current; `trigVal2` (low) | **4** | audit verdict was keep current; no actionable target |
| `05c_custom_functions_io.md:87` | `waitAnaTrigger::trigVal` / `trigVal2` | drop second `addi` or rename `trigValAgain` (low) | **1** | audit suggested removal — micro-cluster M5 (dead-param cleanup) |
| `05c_custom_functions_io.md:95` | `waitZSyncTrigger::supported` | `useDirectPath` (low) | **2** | borderline naming preference; needs per-item judgement |
| `05c_custom_functions_io.md:106` | `setSinePhase::nodeIdx` / `nodeOffset` | drop `nodeIdx`/`nodeOffset` (low) | **1** | audit suggested removal — micro-cluster M5 (dead-param cleanup) |
| `05c_custom_functions_io.md:128` | `getUserReg::userRegIdx` / `userRegInt` | drop `userRegInt` | **1** | audit suggested removal — micro-cluster M5 (dead-param cleanup) |
| `05c_custom_functions_io.md:129` | `getUserReg::immVal` | `clockDivider` | **2** | borderline naming preference; needs per-item judgement |
| `05c_custom_functions_io.md:141` | `startQAResult::imm` | `startWord`, `cmdWord` (low) | **2** | borderline naming preference; needs per-item judgement |
| `24_asm_expression.md:30` | `AsmExpression::value` | immediateOrRegNum, keep current | **2** | borderline naming preference; needs per-item judgement |
| `24_asm_expression.md:40` | `AsmExpression::noOpt()` (alias) | keep alias, keep current | **2** | borderline naming preference; needs per-item judgement |
| `14_waveform.md:15` | `WaveformFile` (struct name) | `Waveform::File` (already alias), keep current | **2** | borderline naming preference; needs per-item judgement |
| `14_waveform.md:18` | `WaveformFile::columnMode` | unknownField1C, keep current | **2** | borderline naming preference; needs per-item judgement |
| `14_waveform.md:28` | `Waveform::waveformType` | type, fileType, keep current | **2** | borderline naming preference; needs per-item judgement |
| `14_waveform.md:32` | `Waveform::addressValue` | address, globalAddress, keep current | **2** | borderline naming preference; needs per-item judgement |
| `37_signal.md:98` | `Signal::append(Signal&)::otherSamplesBegin` (local) | keep current, remove | **2** | borderline naming preference; needs per-item judgement |
| `49_asm_commands_impl.md:41` | `AsmCommandsImpl::wvf::waveReg` (param) | keep current; reg | **4** | audit verdict was keep current; no actionable target |
| `49_asm_commands_impl.md:45` | `AsmCommandsImpl::wvfi::waveReg` (param) | keep current; reg | **4** | audit verdict was keep current; no actionable target |
| `48_address_impl.md:26` | `AddressImpl::value` | keep current, `raw_`, `bits_` | **3** | resolved by `7a87e7e` (Phase R IF-118 (kept)) |
| `36_cache.md:39` | `Cache::appendMode_` | keep current, `isHirzel_` | **3** | resolved by `c1e3aa3` (Phase D c11 / Cluster C) |
| `36_cache.md:49` | `Cache::Pointer::hash_` | keep current, `addrMask_`, `wrapMask_` | **3** | resolved by `085eaca` (Phase R IF-113 (kept)) |
| `36_cache.md:55` | `Cache::Cache::appendMode` (ctor) | keep current, `isHirzel` | **2** | borderline naming preference; needs per-item judgement |
| `36_cache.md:63` | `Cache::allocate(5-arg)::altAllocs` (local) | keep current, `minAllocs` | **2** | borderline naming preference; needs per-item judgement |
| `36_cache.md:73` | `Cache::getBestPosition::bestPosition` (local) | keep current, `bestStart` | **2** | borderline naming preference; needs per-item judgement |
| `13_awg_assembler_impl.md:74` | `AWGAssemblerImpl::unusedStr038_` | keep current (medium) | **4** | audit verdict was keep current; no actionable target |
| `13_awg_assembler_impl.md:112` | errorMessage::msg (param renamed `text` in body) | unify on `text` (medium) | **2** | borderline naming preference; needs per-item judgement |
| `13_awg_assembler_impl.md:128` | opcode4::kOpcodeGroup1Child / kOpcodeGroup2Child | descriptive name TBD; keep current (medium) | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `13_awg_assembler_impl.md:131` | `parseLine`, `parseString`, `encodeExpressions` (header-only declarations) | drop, or annotate (medium) | **1** | audit suggested removal — micro-cluster M5 (dead-param cleanup) |
| `05b_custom_functions_play.md:46` | `setWaitCyclesReg::res` | `keep current`, `unused` | **2** | borderline naming preference; needs per-item judgement |
| `05b_custom_functions_play.md:49` | `setWaitCyclesReg::shifted` (local) | `devTypeBiased`, `keep current` | **2** | borderline naming preference; needs per-item judgement |
| `05b_custom_functions_play.md:58` | `mergeWaveforms::useFunDescrPath` | `keep current`, `useExplicitFunDescr` | **2** | borderline naming preference; needs per-item judgement |
| `05b_custom_functions_play.md:66` | `mergeWaveforms::funDescr2` (local) | `funDescrA`, `keep current` | **2** | borderline naming preference; needs per-item judgement |
| `05b_custom_functions_play.md:70` | `mergeWaveforms::requested` (local) | `requestedChannels`, `keep current` | **2** | borderline naming preference; needs per-item judgement |
| `05b_custom_functions_play.md:83` | `play::numChannels` (local) | `numChannelGroups`, `keep current` | **2** | borderline naming preference; needs per-item judgement |
| `05b_custom_functions_play.md:84` | `play::channelIndex` (local) | `deviceIndex`, `keep current` | **2** | borderline naming preference; needs per-item judgement |
| `05b_custom_functions_play.md:86` | `play::mask` (local) | `triggerMask`, `keep current` | **2** | borderline naming preference; needs per-item judgement |
| `05b_custom_functions_play.md:90` | `play::name` (local) | `keep current`, `waveName` | **2** | borderline naming preference; needs per-item judgement |
| `05b_custom_functions_play.md:96` | `play::reg0` / `regInv` (locals) | `regZero`/`regInvalid`, `keep current` | **1** | regInv → regInvalid — micro-cluster M2 |
| `05b_custom_functions_play.md:115` | `playIndexed::rateImm` (local) | `keep current`, `offsetImm` | **2** | borderline naming preference; needs per-item judgement |
| `05b_custom_functions_play.md:118` | `playIndexed::regInv` (local) | `regInvalid`, `keep current` | **1** | regInv → regInvalid — micro-cluster M2 |
| `22_device_factories.md:26` | `DeviceOpts::SubtypeMask` | keep current; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `22_device_factories.md:27` | `DeviceOpts::Subtype1` … `Subtype4` | keep current; `SubtypeSlotN`; remove if dead | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `22_device_factories.md:28` | `DeviceOpts::FF` | keep current; `FFOption` | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `22_device_factories.md:29` | `DeviceOpts::RTR` | keep current; `RTROption` | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `22_device_factories.md:30` | `DeviceOpts::PLUS` | keep current; `PLUSOption` | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `22_device_factories.md:31` | `DeviceOpts::LRT` | keep current; `LRTOption` | **4** | hardware-address constant; audit said keep / dead-but-documented |
| `22_device_factories.md:34` | `DeviceOpts` (namespace) | keep current; merge with anon-ns | **3** | resolved by `0288bde` (Phase R IF-121) |
| `23_awg_compiler_config.md:74` | `AWGCompilerConfig::numCores` | keep current, `pad_94` | **2** | borderline naming preference; needs per-item judgement |
| `05a_custom_functions.md:25` | `CustomFunctions::unusedStringSet_B0_` | `field_B0_`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `05a_custom_functions.md:36` | `CustomFunctions::SubFunc::Default` | `PlayWave`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `05a_custom_functions.md:44` | `CustomFunctions::writeToNode::type` | `varType`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `05a_custom_functions.md:70` | `parseOptionalRate::strict` | keep current | **4** | audit verdict was keep current; no actionable target |
| `05a_custom_functions.md:73` | `getPlayRate::strict` | `subtractCarry`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `05a_custom_functions.md:79` | `NodeMap::toPhase::value` | `phase`, `phaseDeg` | **2** | borderline naming preference; needs per-item judgement |
| `34_eval_results.md:60` | `EvalResults::name_` | `label_`, keep current | **2** | borderline naming preference; needs per-item judgement |
| `34_eval_results.md:71` | `setValue(VarType,string)::s` | `text`, keep current | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `11_value.md:41` | `Immediate::data_` | keep current (medium), `storage_` (low) | **2** | borderline naming preference; needs per-item judgement |
| `11_value.md:42` | `Immediate::index_` | keep current (medium), `kind_` (low), `tag_` (low) | **2** | borderline naming preference; needs per-item judgement |
| `11_value.md:44` | `Immediate::Immediate(uint32_t)::v` | `addrValue` (medium), `address` (low) | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `11_value.md:45` | `Immediate::Immediate(int32_t)::v` | keep current (medium), `value` (low) | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |
| `11_value.md:72` | `Value::pad_0C_` | keep current (medium) | **4** | audit verdict was keep current; no actionable target |
| `46_wavetable_ir.md:131` | `…::WavetableIR(front,…)::wavetableSize` | keep current | **4** | audit verdict was keep current; no actionable target |
| `46_wavetable_ir.md:185` | `WavetableManager<WaveformIR>::lineNr_` | keep current | **4** | JSON contract — renaming would break output |
| `46_wavetable_ir.md:195` | `WavetableManager<WaveformIR>::newWaveform::fillName` | keep current | **4** | audit verdict was keep current; no actionable target |
| `04e_ast_evaluate_control.md:131` | `SeqCArgList/DeclList/StmtList::evaluate::lineNr` (note) | `nodeLineNr` (low), keep | **2** | borderline naming preference; needs per-item judgement |
| `04e_ast_evaluate_control.md:166` | `SeqCRepeat::evaluate::hasEndLabel` | `needsCountCheck`, keep | **2** | borderline naming preference; needs per-item judgement |
| `04e_ast_evaluate_control.md:171` | `SeqCForLoop::evaluate::hasErrorOrNull` | (delete), keep | **2** | borderline naming preference; needs per-item judgement |
| `04e_ast_evaluate_control.md:172` | `SeqCForLoop::evaluate::r` (line 10096) | inline, keep | **3** | symbol no longer present in source (renamed/removed in Phase D singleton sweep) |

### Phase S.2 micro-cluster proposals

The 15 B1 items group into 5 micro-clusters, each suitable for a
single Phase S.2 commit (build + diff cycle per commit, zero
behavioural risk):

| ID | Theme | Item count | Files touched (approx.) | Notes |
|---|---|---|---|---|
| **M1** | Disasm-leakage local renames | 1 | `asm_commands.cpp` | `AsmCommands::smap` params `r1`/`r2`/`arg` → `value`/`addr`; aligns with the wider §3.5 cleanup already done in Phase D for setSinePhase. |
| **M2** | `regInv` → `regInvalid`, `reg0` → `regZero` | 2 | `custom_functions_play.cpp` (`play`, `playIndexed`) | Matches naming used elsewhere (`regOne`, `regZero`); audit explicitly proposed both names. |
| **M3** | `string_NNN_` / `pad_NNN_` snake_case polish | 2 | `awg_compiler.hpp/.cpp`, `resources.hpp/.cpp` | `AWGCompilerImpl::string_218_` → `pad_218_` (matches sibling pad fields); `StaticResources::pad_108_` retained-as-is alternative noted — verify per audit row before renaming. |
| **M4** | Short-letter math local with concrete target | 1 | `ast_evaluate_logical.cpp` (`SeqCNeg::evaluate`) | Local `d` → `negatedDouble`. Trivial single-function rename. |
| **M5** | Dead-param / dead-local removal | 9 | `asm_commands.cpp`, `asm_list.cpp`, `asm_register.hpp`, `asm_optimize.cpp`, `custom_functions_io*.cpp`, `awg_assembler_impl.hpp` | Includes `genPlayConfig::fourChannel`, `parseStringToAsmList::imm1/imm2`, `AsmRegister::toInt`, `splitConstRegisters::needsSplit`, `setPRNGRange::val0/val1`, `waitAnaTrigger::trigVal2`, `setSinePhase::nodeIdx/nodeOffset`, `getUserReg::userRegInt`, header-only decls `parseLine`/`parseString`/`encodeExpressions`. Each removal must be verified against the binary signature first (some unused params may match the original ABI). |

**Recommended S.2 ordering**: M5 first (it's the most varied — get
the verification work done early), then M2 (smallest, mechanical),
M4, M1, M3 (last because some B2 sibling locals may want to ride
along with the M3 polish in `awg_compiler.cpp`).

**Estimated S.2 effort**: half a day total — each micro-cluster is
1–2 file edits + one build + one diff_test cycle. Net legibility
gain modest; main value is closing the B1 backlog cleanly so future
audits start from a smaller surface.

### Phase Q backlog post-S.1

After S.1 the perceived 226-item backlog reduces to:

- **39 already done** (B3) — closed by reference; no further work.
- **58 formally wontfix** (B4) — closed; documentation lists below.
- **15 mechanical** (B1) — staged for S.2 (5 micro-clusters above).
- **114 deferred** (B2) — kept in this document for any future
  Phase Q-prime style pass; no commitment to address.

Net "open" count after S.2 lands: **114** (all B2). These are
genuinely cosmetic preferences with no actionable consensus from
the original audit.

---

## §7. Dead code surfaced by the audit

Symbols whose audit-recommended action is **remove**, not rename:

| Batch | Symbol | Kind |
|---|---|---|
| 04b | `scaleWaveform(int,sp,ctx)::scaleFactor` | dead param (function always negates) |
| 05a | `CustomFunctions::field_80/88/A8/B0` | recon-only libc++ pad |
| 05c | `setSinePhase::resultPtr`, `rbx` locals | dead disasm-leakage |
| 05c | `incrementSinePhase::nodeIdx`, `nodeOffset` locals | dead, mirror of setSinePhase |
| 05d | `playWaveZSync::maxSampleLen` | dead local |
| 07 | `Compiler::flags_`, `reserved18_`, `sourceFiles_` | declared but unused (verify-not-original) |
| 13 | `AWGAssemblerImpl::unusedStr038_` | no observed reader/writer |
| 13 | `parseLine`, `parseString`, `encodeExpressions` | header decls without definitions |
| 19a | `VarSubType_Bool` legacy alias | retained-for-compat |
| 22 | `DeviceOpts` namespace constants | duplicates anon-ns `k*` set |
| 23 | `AWGCompilerConfig::numCores` | only-write site, no consumer |
| 23 | `AWGCompilerConfig::appendMode()` / `splitIndex()` / `syncVersion()` aliases | dead |
| 24 | `AsmExpression::nopComment` | unread placeholder |
| 28 | `AWGCompilerImpl::string_218_` | no read/write located |
| 45 | `WavetableFront::address2_` | never read |
| 46 | `WavetableIR::allocateWaveforms::totalSamples` | accumulated and discarded |
| 47 | `ElfWriter::addWaveform::wfName2` (local) | duplicate of `wfName` |

These should be deleted in the same commit as the cluster they
travel with, when applicable; otherwise as a single "audit:
remove dead code surfaced by symbol-renaming audit" commit.

---

## §8. Type-suspicion side observations (RULES §2a)

Not naming actions; flagged for downstream type-fix work.

| Batch | Symbol | Concern |
|---|---|---|
| 02, 14, 16, 36, 46, 48 | `memorySizeInSamples`, `getSampleCount`, `cache.numSamples`, `totalSamples`, `memorySizeInSamples` | bytes-vs-samples confusion across the wave-memory subsystem |
| 11 | `Value::pad_04_` | not padding; subType_ shape |
| 26, 33 | `namespace Assembler` + `AssemblerInstr` | two recon types should be one binary class |
| 27 | `NodeMapItem::hasFast` | `int` slot conflated with `AccessMode` |
| 36 | `Cache::Pointer::hash_` | not a hash; prefetch wrap-address |
| 38 | `PlayConfig::now` | named `now` but read as 4-channel flag at consumer |
| 39, 47, 52 | `strict`, `useAbsolute`, `showLine` | polarity-inverted booleans |
| 42 | `Expression::valueType` | int slot actually `EDirection` enum |
| 48 | `AddressImpl<T>` | wrapper holds byte sizes, suser opcodes, raw imms — overgeneral |

---

## §9. Incidental logic bugs (not naming)

These are NOT naming issues; they are real-or-suspected bugs that
surfaced during audit reads. Per AGENTS.md "Incidental discovery
discipline", they should be moved to `incidental_findings.md` with
stable IF-IDs once Phase D begins (and we exit audit mode).

| Source batch | Site | What looks wrong | Severity |
|---|---|---|---|
| 05c2 | `setPRNGSeed` integer-literal path | constructs `AsmRegister` from a value rather than using `reg_` | likely-bug; **GDB trace before promoting** |
| 05c | `configFreqSweep` magic literals (`0x8e/0x8f/0x90/0x91/0x8c`) | constants exist but unused | confirmed; cosmetic |
| 22 | `DeviceOpts` namespace | full duplicate of anon-ns `k*` set | confirmed; dead |
| 26 | `namespace Assembler` + `AssemblerInstr` | recon split of one binary class | confirmed; structural |
| 19a | `Resources::parent_` strong/weak inversion | strong holds grandparent, weak holds direct parent — likely *intentional* (cycle-break) but the strong/weak ownership choice may need re-validation | suspicious |
| 47 | `addWaveform::useAbsolute` polarity | true takes NOBITS (mapped) path; comments at call site say `/*mapped=*/true` | confirmed; will flip with rename |
| 27 | `NodeMapItem::hasFast` int conflation with AccessMode | type punning two semantics | suspicious |

---

## §10. `verify-not-original` items (resolved by Phase D commit-1 nm-recheck)

Some symbols' provenance was ambiguous during the per-batch sweeps —
agents could not conclusively verify whether the symbol came from the
binary (excluded from rename) or was recon-introduced (in scope). The
Phase D commit-1 nm-recheck pass re-checked each one against
`nm --demangle` of `_seqc_compiler.so` (defined-only and full), plus
`strings _seqc_compiler.so` for non-static data members.

**33 items resolved (vs ~20 originally estimated):** out-of-scope: 1 | in-scope: 32 | needs-investigation: 0.
The 4 former needs-investigation items (non-static member fields with
no nm or strings hit) were reclassified to in-scope as recon-introduced.

A general rule was applied to function-parameter rows: nm preserves
only the parameter type in the mangled signature, not the name, so
parameter-name `verify-not-original` rows are virtually always
`in-scope`.

| Symbol | Report | Decision | Evidence |
|---|---|---|---|
| `Prefetch::minIndexedSize` (static) | 09 | out-of-scope | nm: `zhinst::Prefetch::minIndexedSize` defined (already noted in 09b) |
| `WaveformFile` (struct name) | 14 | in-scope | nm: binary defines `zhinst::Waveform::File`, not `WaveformFile` |
| `AsmRegister(int v, bool val)` 2-arg ctor | 06 | in-scope | nm: only 1-arg ctor present |
| `AsmRegister::Invalid` | 06 | in-scope | not in nm |
| `AsmRegister::Reg` | 06 | in-scope | not in nm |
| `AsmRegister::toInt` | 06 | in-scope | nm has only `operator int() const` |
| `zhinst::isValid(AsmRegister)` (free) | 06 | in-scope | not in nm |
| `zhinst::toInt(AsmRegister)` (free) | 06 | in-scope | not in nm |
| `Compiler::flags_` | 07 | in-scope | non-static member; no positive nm/strings evidence |
| `Compiler::reserved18_` | 07 | in-scope | non-static member; no positive nm/strings evidence |
| `Compiler::sourceFiles_` | 07 | in-scope | non-static member; no positive nm/strings evidence |
| `LowerResult` (type) | 07 | in-scope | no `LowerResult`/`LowerOutput` in nm or strings |
| `MathCompiler::functionExists::strict` | 39 | in-scope | parameter name (nm preserves only `bool` type) |
| `addVariableType::isConst` | 42 | in-scope | parameter name |
| `WaveformIR::toJsonElement::format` | 16 | in-scope | parameter name |
| `AWGAssembler::printOpcode::format` | 33 | in-scope | parameter name |
| `WavetableIR(front,…)::wavetableSize` | 46 | in-scope | parameter name |
| `SeqCArgList/DeclList/StmtList::evaluate::lineNr` | 04e | in-scope | parameter name (note row, not actual rename target) |
| `CompilerMessageCollection::parserMessage::line` | 52 | in-scope | parameter name |
| `AWGAssemblerImpl::parseLine`/`parseString`/`encodeExpressions` (decls) | 13 | in-scope | not in nm |
| `NodeTypeIdx::RawDoubleLow32` | 27 | in-scope | not in nm or strings |
| `NodeType::SetVarPlaceholder`..`Wait` | 20 | in-scope | not in nm or strings (no enumerator names emitted) |
| `ErrorMessageT::UnknownError47` | 08 | in-scope | not in nm or strings |
| `DeviceOpts::SubtypeMask` | 22 | in-scope | not in nm or strings |
| `DeviceOpts::Subtype1`..`Subtype4` | 22 | in-scope | not in nm or strings |
| `DeviceOpts::FF` | 22 | in-scope | not in nm or strings |
| `DeviceOpts::RTR` | 22 | in-scope | not in nm or strings |
| `DeviceOpts::PLUS` | 22 | in-scope | not in nm or strings |
| `DeviceOpts::LRT` | 22 | in-scope | not in nm or strings |
| `AsmExpression::nopComment` | 24 | in-scope | nm/strings: no hit → recon-introduced |
| `AWGCompilerImpl::string_218_` | 28 | in-scope | nm/strings: no hit → recon-introduced |
| `AWGAssemblerImpl::unusedStr038_` | 13 | in-scope | nm/strings: no hit → recon-introduced |
| `AWGCompilerConfig::numCores` | 23 | in-scope | nm/strings: no hit → recon-introduced |

**This nm-recheck commit formally closes the symbol-renaming audit. Subsequent Phase D commits resume normal AGENTS.md workflow (TODO.md tracking, OVERVIEW.md updates, build verify per sub-phase, per-sub-phase commits).**

---

## §11. Recommended execution sequencing for Phase D

When the user moves to Phase D, audit suggests this order:

**Commit 1 — settle `nm` re-checks.** Resolve the §10
verify-not-original list. No source edits; just confirm each
symbol's status against `nm --demangle` and update audit notes.

**Commit 2 — Cluster H (`clone`→`doClone`).** Mechanical, vtable-atomic,
zero semantic risk, ×33 sites. Easiest cluster to land first.

**Commit 3 — Cluster F (`SeqCAstNode::type`→`lineNr`).** Mechanical
cascade, ×54 sites. High line-count but trivial substitution. Build
verification mandatory.

**Commit 4 — Cluster G (binary AST node `first_`/`second_`).** ×8 classes.

**Commit 5 — Cluster J (Waveform JSON-key drift).** Tier-2 anchored,
mechanical.

**Commit 6 — Cluster B (`isWaveformCmd` semantic inversion → `noOpt` / `skipOptimization`).**
Affects `AsmList::Asm` field, free predicate, every consumer in
`asm_optimize.cpp` and `asm_commands.cpp`. Canonical name fixed at
`noOpt` / `skipOptimization` per §13.

**Commit 7 — Cluster A (`flag` → `noOpt` / `skipOptimization`).** Strict
follow-on of commit 6.

**Commit 8 — Cluster K (PlayConfig producer/field swap).**

**Commit 9 — Cluster L (`AsmCommandsImpl::wvf/wvfi` param drift).**

**Commit 10 — Cluster D (`channelGrouping` → `loopUnrollLimit`).**
3-leg coordinated.

**Commit 11 — Cluster C (`isHirzel` family + Cache aliasing).**
After commits 6–10 settle, this cluster is small.

**Commit 12 — Cluster E (forwarding-accessor aliases).** Drop dead
aliases.

**Commit 13 — Cluster N (`Resources::parent_` → `grandparent_`).**
Two-step rename to avoid name collision.

**Commit 14 — High-confidence singletons (§4).** Group by batch.

**Commit 15 — Cluster M (`Assembler` type recomposition).** Defer
to last; structural rather than naming.

**Commit 16 — Open arbitrations (§2).** Each requires its own
investigation; one PR per arbitration.

**Commit 17 — Medium-confidence singletons (§5).**

**Commit 18 — Dead-code removals (§7).** Group by batch.

**Commit 19 — Promote incidental bugs (§9).** Add IF-IDs to
`incidental_findings.md`. Audit closes; AGENTS.md normal workflow
resumes.

**Commit 20+ — Phase Q (cosmetic/style pass).** §6 low/unsure items.

The Cluster I (`sfc::*Option::Bit0xNNNN`) coordinated rename can land
between any of commits 1-13 in its own dedicated commit; it is
self-contained.

---

## §12. Execution discipline notes

- Per RULES §11/§12, this audit is read-only and no commits during
  scan. Phase D execution **exits audit mode**: AGENTS.md normal
  workflow (TODO.md tracking, OVERVIEW.md updates, build verify per
  sub-phase, per-sub-phase commits) resumes.
- Every cluster commit must run `cmake --build .` from
  `reconstructed/build/` before commit. ODR / vtable / linker
  warnings can mask atomic-rename mistakes.
- Run the full `python tests/diff_test.py` after every cluster
  commit. Any cluster commit that introduces a test regression must
  be reverted, not amended over.
- Cluster M (type-recomposition) requires GDB binary verification
  before any source change.
- Several open arbitrations recommend GDB tracing before rename;
  audit explicitly defers those to execution.

---

## Appendix A — Counts

| Category | Count |
|---|---|
| Total reports | 62 (54 batches; 8 sub-batch splits; 3 archived v1 pilots) |
| Total flagged rows (`yes` + `unsure`) | 546 |
| Status-tagged rows (`cross-batch-arbitration` / `coordinated-rename` / `verify-not-original`) | 167 |
| Cross-batch coordinated clusters (§1) | 16 (A–P) |
| Open arbitrations (§2) | 11 |
| In-batch coordinated groups (§3) | 10 |
| High-confidence singletons (§4) | 35 |
| Medium-confidence singletons (§5) | 111 |
| Yes/low + unsure parked (§6) | 226 |
| Dead-code candidates (§7) | 17 |
| Type-suspicion side notes (§8) | ~10 |
| Incidental logic bugs (§9) | 7 |
| `verify-not-original` to recheck (§10) | ~20 |

## Appendix B — Where evidence lives

This synthesis cites batches by number; the per-batch reports under
`reconstructed/notes/symbol-renaming-audit/` (filenames `NN_*.md`)
contain the full §3 detail blocks with file:line evidence.

The /tmp dumps used during synthesis preparation are *not* checked
in; they can be regenerated by:

```
cd reconstructed/notes/symbol-renaming-audit
rg -n '^\| `' *.md > /tmp/audit_all_rows.txt
rg '\| (cross-batch-arbitration|coordinated-rename|verify-not-original) \|' *.md > /tmp/audit_status.txt
rg '^\S+:\d+:\| .* \| yes \|' /tmp/audit_all_rows.txt | rg -v 'cross-batch-arbitration|coordinated-rename|verify-not-original|not-misnomer' > /tmp/audit_yes_singleton.txt
```

---

## §13. Decisions captured

This section locks the user-decisions for every cluster, arbitration,
and bulk approval listed above. Phase D execution must follow these
choices unless explicitly revisited.

### Cluster decisions
| Cluster | Decision |
|---|---|
| A — `flag` params | Adopt Cluster B canonical name (atomic with B) |
| B — `isWaveformCmd` inversion | **`noOpt` / `skipOptimization`** |
| C.1 — Hirzel aliases | Drop both alias families (`Prefetch::isHirzel_()` / `set_isHirzel_()` AND `AWGCompilerConfig::appendMode()`) |
| C.2 — `Cache::appendMode_` | Rename to `isHirzel_`. `Cache::getBestPosition::appendMode` (independent) → `gapScan` |
| D — `channelGrouping` | `loopUnrollLimit` (3-leg) |
| E — accessor aliases | Drop aliases uniformly. Special case: PNS — rename field `requiredSlots` → `usedCache_` and drop alias |
| F — `SeqCAstNode::type` | Approved (cascade rename to `lineNr`, ×54) |
| G — AST `first_/second_` | Approved (8 classes; update macro + ctor params) |
| H — `clone → doClone` | Approved (×33) |
| I — `sfc::*Option::Bit0xNNNN` | Defer per-enum naming to execution time |
| J — Waveform JSON keys | Approved (9 renames) |
| K — PlayConfig swap | Approved (4 producer params + parallel asmPlay/asmTable) |
| L — `AsmCommandsImpl::wvf/wvfi` | Approved (markerReg→dstReg, waveIndex→length; vtable-coordinated) |
| M — Assembler type recomp | Defer to last; GDB+RTTI confirm first |
| N — `Resources::parent_` | Approved (two-step swap + GlobalResources ctor param) |
| O — snake_case | Approved (4 fixes) |
| P — `kDevCervino` | Approved (3 renames) |

### Arbitration decisions
| # | Symbol | Decision |
|---|---|---|
| 1 | WavetableManager numDefs/lineNr_ | **Pending investigation** |
| 2 | DeviceConstants::numDIOBits | **needs-GDB** (Phase R: trace `configFreqSweep` on UHFLI to confirm osc-bound use) |
| 3 | waveformGranularity/PageSize swap | Approved (two-step coordinated swap) |
| 4 | usedSampleRate_ mirror | **fixed** (Phase R: writer found at 0x1213d6 inside `Compiler::compile`; recon now copies `staticResources->usedSampleRate_` into `Compiler::usedSampleRate_` at end of compile. Both fields are live; both names kept.) |
| 5 | NodeMapItem::hasFast | **rejected** (Phase R: GDB confirms only 0/1 ever stored across 51 lookupNode hits — bool is correct; field is intentional dual-role bool/AccessMode-Soft-or-Direct selector. See IF-112.) |
| 6 | createOrAppend*::lhs/rhs | Keep `lhs`/`rhs` for consistency |
| 7 | mergeWaveforms::useYSuffix | **kept** (Phase R: name fits both Y-suffix funDescr and interleave-vs-merge factory; Y-suffix == dual-channel I/Q == interleave) |
| 8 | addCommand::cmd/args | Rename `args→cmdToken`, `cmd→argList` |
| 9 | Asm::wavetableFront | **kept** (Phase R: dual-purpose handled by `lineNumber()` accessor in asm_list.hpp:71-72; no rename needed) |
| 10 | Cache::play/Cache::allocate | Resolve after Cluster C committed |
| 11 | loopArgNodeAppend::arg | **kept** (Phase R: `arg` is the node argument appended to target's loop->next chain; matches all four call-site roles — initResult, condResult, incrResult, countResult — none of which is more specifically a "loopVar"; `arg` is the right generic name) |

### Bulk decisions
- §3: all 10 in-batch coordinated groups — approved
- §4: 35 high-conf singletons — approved (batch-grouped commit)
- §5: 111 medium-conf singletons — approved (single commit)
- §6: 226 low/unsure — defer to future style pass
- §7: 17 dead-code candidates — approved (deletions)
- §8: ~10 type-suspicion observations — promote to IF-IDs at audit close
- §9: 7 incidental logic bugs — promote to `incidental_findings.md` with IF-IDs at audit close
- §10: ~20 verify-not-original — commit-1 of Phase D (dedicated nm-recheck)
- §11: 20-step sequencing — approved as-listed

### Audit lifecycle
- Audit remains formally open until Phase D commit-1 (the nm-recheck) actually touches source.
- Per RULES §11, edits remain restricted to `reconstructed/notes/symbol-renaming-audit/` until then.
