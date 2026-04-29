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

### Arbitration 4 — `Compiler::usedSampleRate_` vs `StaticResources::usedSampleRate_`

The two fields mirror each other; `Compiler`'s is never written. One
of them is dead, or one is read by a path the audit didn't locate.

**Decision.** Investigate at execution time. Not safe to rename
either without resolving.

Cross-refs: `07_compiler.md:72`, `19a_resources.md:188`.

### Arbitration 5 — `NodeMapItem::hasFast` is conflated with `AccessMode`

Used at `addNodeAccess` call sites where the same int slot is
sometimes a bool (`hasFast`) and sometimes an `AccessMode` enum
value. Either two semantically distinct things share one field, or
one of the two readings is wrong.

**Decision.** Investigate; possibly type fix rather than rename.
Anchor: `27_node_map_data.md:65`.

### Arbitration 6 — `expression.cpp::createOrAppend{Arg,Decl,Param,Stmt}List::lhs/rhs`

Thin wrapper functions with `lhs`/`rhs` params; semantically the
roles are "existing list" / "new item". Decide between
`existing/newItem` (descriptive) vs keep `lhs/rhs` (consistent
with surrounding eval functions).

Anchor: `42_expression.md:103`.

### Arbitration 7 — `mergeWaveforms::useYSuffix` (batch 05b)

Param also gates the merge/interleave step, not just the suffix.
Two plausible names (`useYSuffix` vs `interleaveSecondary`) under
different readings.

Anchor: `05b_custom_functions_play.md:55`.

### Arbitration 8 — `addCommand::cmd` / `args` swap (50 ↔ 24)

Names suggest one role; grammar/body shows the other. Either swap
the names or rename `args` → `cmdToken` and `cmd` → `argList`.

Anchor: `50_asm_parser_context.md:68`.

### Arbitration 9 — `AsmList::Asm::wavetableFront` (44, dual-purpose)

Field is used both as a `WavetableFront*` (semantic) and as a
"line number cache slot" (via the alias accessor `lineNumber()`).
Either split into two fields or accept dual-purpose with a new name
(`slot0x88` / `wavetableFrontIndex_`).

Anchor: `44_asm_list.md:34-37`.

### Arbitration 10 — `Cache::play::state` and `Cache::allocate(5-arg)::pageSize`

Two cache-API params receive cross-batch-arbitration flags from 36
because their values come from `Prefetch`-side fields that have
their own pending decisions (`PrefetcherNodeState::counter()` /
`Prefetch::maxBranches_`). Resolve after Cluster E is decided.

Anchor: `36_cache.md:55,75`.

### Arbitration 11 — `loopArgNodeAppend::arg` (04b)

Generic param name; cross-batch-arbitration to where loop-arg nodes
are created (likely 04a/04e SeqCFor handling).

Anchor: `04b_ast_evaluate_helpers.md:105`.

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

## §6. Low-confidence and unsure (parked)

38 yes/low + 188 unsure rows. These are the *long tail*: stylistic
underscore consistency, cosmetic abbreviations, names that fit usage
*locally* but might be improved given fuller context. Recommendation:
**defer to a future style/cosmetic pass** after the high/medium
renames have settled and the codebase is more legible.

A few unsure flags carry semantic risk and *should* be revisited:

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

## §10. `verify-not-original` items (need second-pass `nm` check)

Some symbols' provenance was ambiguous — agents could not conclusively
verify whether the symbol came from the binary (excluded from rename)
or was recon-introduced (in scope). Synthesis re-checks these against
the binary symbol table at execution time. ~20 items, all flagged
in the per-batch reports with status `verify-not-original`. Notable:

- `Prefetch::minIndexedSize` (static) — already confirmed §3-excluded
  in 09b, can drop the flag.
- `Compiler::flags_`, `reserved18_`, `sourceFiles_`.
- `MathCompiler::functionExists::strict` (polarity-inverted bool).
- `AsmExpression::nopComment`.
- `LowerResult` type.

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

**Commit 6 — Cluster B (`isWaveformCmd` semantic inversion).**
Affects `AsmList::Asm` field, free predicate, every consumer in
`asm_optimize.cpp` and `asm_commands.cpp`. **Decide canonical name
first** — see Open Arbitration counterpart.

**Commit 7 — Cluster A (`flag` → cluster-B-canonical).** Strict
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
