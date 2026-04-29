# Batch 10 — asm_commands

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 4 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 2; B2 (borderline, deferred): 2;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/asm_commands.hpp`
- `reconstructed/src/asm_commands.cpp`

Cross-file usage surveyed in: `prefetch_emit.cpp`, `prefetch_splitplay.cpp`,
`prefetch_placesingle.cpp`, `custom_functions_play.cpp`,
`custom_functions_playback.cpp`, `custom_functions_io.cpp`,
`play_config.cpp` (consumers of `PlayConfig` fields written by
`genPlayConfig`).

Symbol-table verification (`nm --demangle _seqc_compiler.so`):

- Type `zhinst::AsmCommands` — appears mangled inside `Compiler`,
  `CustomFunctions`, `Prefetch`, `FrontEndLoweringFacade`,
  `shared_ptr<AsmCommands>` → **excluded** (§3, tier 1).
- All wrapper methods declared in `asm_commands.hpp` (`prf`, `wprf`,
  `wwvfq`, `wwvf`, `wvf`, `wvfi`, `wvfs`, `wvft`, `cwvf`, `cwvfr`,
  `br`, `brz`, `brnz`, `brgz`, `alur`, `addr`, `subr`, `andr`, `orr`,
  `xnorr`, `ssl`, `ssr`, `aluiu`, `addiu`, `andiu`, `oriu`, `xnoriu`,
  `alui`, `addi`, `addi32`, `andi`, `ori`, `xnori`, `toInt32`, `ld`,
  `st`, `ldio`, `sdio`, `luser`, `suser`, `ltrig`, `strig`,
  `sinttrig`, `wtrig`, `wtrigi`, `sid`, `smap`, `ldiotrig`, `lcnt`,
  `trap`, `irpt`, `end`, `nop`, `syncCervino`, `unsyncCervino`,
  `asmSyncPlaceholderCervino`, `asmSyncHirzel`, `asmZero`, `asmOne`,
  `asmLabel`, `asmMessage`, `asmBranchNode`, `asmLoopNode`, `asmRate`,
  `asmSetPrecompFlags`, `asmSetVarPlaceholder`, `asmLockPlaceholder`,
  `asmUnlockPlaceholder`, `asmLoadPlaceholder`, `asmPrefetch`,
  `genPlayConfig`, `asmPlay`, `asmTable`, `asmWtrigLSPlaceholder`,
  `fb`) appear in `nm` qualified by `AsmCommands::` →
  **method names excluded** (§3, tier 1).
- The class has **no static data members** — `nm` shows nothing
  matching `AsmCommands::k*`/`*Shift`/`*Mask`. Nothing tier-1 to
  inspect there.
- Helpers `emitEntry`, `emitNodeEntry`, accessors
  `setWavetableFrontIndex`, `wavetableFrontIndex` — **not in `nm`**
  (header-inline / private; reconstruction). Names in scope.
- Namespace-anonymous constants (`kDioAddrLow`, `kDioAddrHigh`,
  `kIdAddrLow`, `kIdAddrHigh`, `kImm19HalfRange`,
  `kImm19MaxUnsigned`) — internal linkage; not in `nm`. In scope.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AsmCommands::impl_` (data member) | no | high | matches role and type | keep current (high) | — |
| `AsmCommands::wavetable_` (data member) | no | medium | matches type and uses | keep current (high) | — |
| `AsmCommands::errorHandler_` (data member) | no | high | matches role | keep current (high) | — |
| `AsmCommands::wavetableFrontIndex_` (data member) | no | medium | name reused in setter | keep current (high) | — |
| `AsmCommands::numChannelGroups_` (data member) | no | high | initialised from `config.numChannelGroups` | keep current (high) | — |
| `AsmCommands::AsmCommands::config` (param) | no | high | conventional | keep current (high) | — |
| `AsmCommands::AsmCommands::wavetable` (param) | no | high | matches member | keep current (high) | — |
| `AsmCommands::AsmCommands::errorHandler` (param) | no | high | matches member | keep current (high) | — |
| `AsmCommands::prf::reg1`,`reg2` (params) | no | medium | first/second-register convention | keep current (high) | — |
| `AsmCommands::prf::intArg` (param) | yes | medium | always size, never generic | size, immediate | — |
| `AsmCommands::wvf::reg`/`dstReg`/`length` (params) | no | medium | matches role | keep current (high) | — |
| `AsmCommands::wvfi::reg`/`dstReg`/`length` (params) | no | medium | matches role | keep current (high) | — |
| `AsmCommands::wvfs::type`/`reg`/`length` (params) | no | medium | matches role and type | keep current (high) | — |
| `AsmCommands::wvft::reg`/`length` (params) | no | medium | matches role | keep current (high) | — |
| `AsmCommands::wvfs::r0`,`chosen` (locals) | no | low | role-descriptive | keep current (high) | — |
| `AsmCommands::cwvf::value` (param) | no | medium | generic, single use | keep current (high) | — |
| `AsmCommands::cwvfr::reg` (param) | no | high | matches role | keep current (high) | — |
| `AsmCommands::br::label`,`flag` (params) | yes | high | `flag` ≡ `isWaveformCmd` | label keep; flag→isWaveformCmd | coordinated-rename |
| `AsmCommands::brz::reg`,`label`,`flag` | yes | high | `flag` ≡ `isWaveformCmd` | reg/label keep; flag→isWaveformCmd | coordinated-rename |
| `AsmCommands::brnz::reg`,`label`,`flag` | yes | high | `flag` ≡ `isWaveformCmd` | flag→isWaveformCmd | coordinated-rename |
| `AsmCommands::brgz::reg`,`label`,`flag` | yes | high | `flag` ≡ `isWaveformCmd` | flag→isWaveformCmd | coordinated-rename |
| `AsmCommands::alur::cmd`,`dst`,`src` (params) | no | high | matches role | keep current (high) | — |
| `AsmCommands::addr/subr/andr/orr/xnorr::dst`,`src` | no | high | matches role | keep current (high) | — |
| `AsmCommands::ssl/ssr::reg` (param) | no | medium | sole operand register | keep current (high) | — |
| `AsmCommands::aluiu::cmd`,`dst`,`src`,`imm` | no | high | matches role | keep current (high) | — |
| `AsmCommands::addiu/andiu/oriu/xnoriu(Imm)::dst`,`src`,`imm` | no | high | matches role | keep current (high) | — |
| `AsmCommands::alui::cmd`,`dst`,`src`,`imm` | no | high | matches role | keep current (high) | — |
| `AsmCommands::alui::sval`,`uval`,`upper` (locals) | no | medium | conventional encoding names | keep current (high) | — |
| `AsmCommands::alui::loadInstr` (local) | no | medium | role-descriptive | keep current (high) | — |
| `AsmCommands::alui::regCmd` (local) | no | medium | role-descriptive | keep current (high) | — |
| `AsmCommands::alui::result` (local) | no | high | conventional | keep current (high) | — |
| `AsmCommands::addi/addi32/andi/ori/xnori(Imm)::dst`,`src`,`imm` | no | high | matches role | keep current (high) | — |
| `AsmCommands::addi32::uval`,`upper`,`instr`,`entry1`,`entry2` (locals) | no | medium | role-descriptive | keep current (high) | — |
| `AsmCommands::toInt32::val` (param) | no | high | matches type | keep current (high) | — |
| `AsmCommands::toInt32::d`,`msg` (locals) | no | medium | role-descriptive | keep current (high) | — |
| `AsmCommands::addi(Value)/andi(Value)/ori(Value)/xnori(Value)/addiu(Value)/andiu(Value)/oriu(Value)/xnoriu(Value)::dst,src,val,imm` | no | high | matches role | keep current (high) | — |
| `AsmCommands::ld/st/luser/suser::reg`,`addr` (params) | no | high | matches role | keep current (high) | — |
| `AsmCommands::ldio/sdio::reg`,`highBank` (params) | no | high | bank selector | keep current (high) | — |
| `AsmCommands::ltrig/strig/sinttrig::reg` (param) | no | high | matches role | keep current (high) | — |
| `AsmCommands::wtrig::r1`,`r2` (params) | no | medium | first/second-register convention | keep current (high) | — |
| `AsmCommands::wtrigi::value` (param) | no | medium | generic, single use | keep current (high) | — |
| `AsmCommands::sid::reg`,`highBank` (params) | no | high | bank selector | keep current (high) | — |
| `AsmCommands::smap::r1`,`r2`,`arg` (params) | yes | low | `arg` is a value/address | value, addr | — |
| `AsmCommands::smap::result`,`st1`,`st2` (locals) | no | medium | role-descriptive | keep current (high) | — |
| `AsmCommands::ldiotrig::reg` (param) | no | high | matches role | keep current (high) | — |
| `AsmCommands::lcnt::reg`,`addr` (params) | no | high | matches role | keep current (high) | — |
| `AsmCommands::syncCervino::reg1`,`reg2`,`flag` (params) | unsure | low | `flag` semantics opaque | flag candidates listed | — |
| `AsmCommands::syncCervino::result`,`tmp`,`tmp2` (locals) | no | medium | role-descriptive | keep current (high) | — |
| `AsmCommands::unsyncCervino::instr1`/`instr2`/`wfIndex1`/`wfIndex2`/`seqId1`/`seqId2`/`entry1`/`entry2`/`result` (locals) | no | medium | role-descriptive | keep current (high) | — |
| `AsmCommands::asmZero/asmOne::reg` (params) | no | high | matches role | keep current (high) | — |
| `AsmCommands::asmLabel::label` (param) | no | high | matches role | keep current (high) | — |
| `AsmCommands::asmMessage::msg`,`isError` (params) | no | high | matches role | keep current (high) | — |
| `AsmCommands::asmRate::rate` (param) | no | high | matches role | keep current (high) | — |
| `AsmCommands::asmSetPrecompFlags::flags` (param) | no | high | matches role | keep current (high) | — |
| `AsmCommands::asmSetVarPlaceholder::reg` (param) | no | medium | matches role | keep current (high) | — |
| `AsmCommands::asmLockPlaceholder::wvf`,`index` | no | medium | matches role | keep current (high) | — |
| `AsmCommands::asmUnlockPlaceholder::wvf`,`index` | no | medium | matches role | keep current (high) | — |
| `AsmCommands::asmPrefetch::wvf` (param) | no | high | matches role | keep current (high) | — |
| `AsmCommands::asmPrefetch::nameIndex` (param) | yes | medium | written to `deviceIndex` | deviceIndex | — |
| `AsmCommands::asmPrefetch::regVal` (param) | yes | medium | written to `lengthReg` | lengthReg, regNum | — |
| `AsmCommands::asmPrefetch::extraVal` (param) | yes | medium | written to `length` | length | — |
| `AsmCommands::genPlayConfig::wvf` (param) | no | high | matches role | keep current (high) | — |
| `AsmCommands::genPlayConfig::isHold` (param) | yes | medium | not a hold flag | singleChannel, masked | — |
| `AsmCommands::genPlayConfig::fourChannel` (param) | yes | low | unused inside body | unused/consider removal | — |
| `AsmCommands::genPlayConfig::isFourChannelBool` (param) | yes | medium | writes `PlayConfig::now` | playNow, now, isNow | cross-batch-arbitration |
| `AsmCommands::genPlayConfig::isBool` (param) | yes | high | writes `PlayConfig::hold` | hold, isHoldFlag | — |
| `AsmCommands::genPlayConfig::holdCount` (param) | yes | high | writes `PlayConfig::rate` | rate | cross-batch-arbitration |
| `AsmCommands::genPlayConfig::suppress` (param) | no | high | matches field | keep current (high) | — |
| `AsmCommands::genPlayConfig::isHoldMode` (param) | yes | medium | writes `PlayConfig::is4Channel` | is4Channel | cross-batch-arbitration |
| `AsmCommands::genPlayConfig::trigger` (param) | no | high | matches field | keep current (high) | — |
| `AsmCommands::genPlayConfig::config` (local) | no | high | matches role | keep current (high) | — |
| `AsmCommands::genPlayConfig::wf`/`channels`/`fullMask`/`channelMask`/`mbits`/`data`/`count`/`pairs`/`b0`/`b1`/`v0`/`v1`/`markerBits`/`adjusted`/`b`/`v` (locals) | no | medium | role-descriptive | keep current (high) | — |
| `AsmCommands::asmPlay::waveforms` (param) | no | high | matches role | keep current (high) | — |
| `AsmCommands::asmPlay::nameIndex` (param) | yes | medium | written to `deviceIndex` | deviceIndex, channelIndex | — |
| `AsmCommands::asmPlay::isHold` (param) | yes | medium | mirrors genPlayConfig::isHold | singleChannel | — |
| `AsmCommands::asmPlay::fourChannel` (param) | unsure | low | dual-purpose at call site | playNow-or-fourChannel | — |
| `AsmCommands::asmPlay::isBool` (param) | yes | medium | mirrors genPlayConfig::isBool | hold, isHoldFlag | — |
| `AsmCommands::asmPlay::holdCount` (param) | yes | high | call sites pass `rate` | rate | coordinated-rename |
| `AsmCommands::asmPlay::suppress` (param) | no | high | matches field | keep current (high) | — |
| `AsmCommands::asmPlay::isHoldMode` (param) | yes | medium | mirrors genPlayConfig::isHoldMode | is4Channel | coordinated-rename |
| `AsmCommands::asmPlay::reg` (param) | yes | medium | written to `lengthReg` | lengthReg, indexReg | — |
| `AsmCommands::asmPlay::regVal` (param) | yes | medium | written to `length` | length, waveIndex | — |
| `AsmCommands::asmPlay::reg2` (param) | yes | medium | written to `indexOffsetReg` | indexOffsetReg | — |
| `AsmCommands::asmPlay::trigger` (param) | no | high | matches field | keep current (high) | — |
| `AsmCommands::asmPlay::node`,`currentWvf`,`wfCopy`,`wf` (locals) | no | medium | role-descriptive | keep current (high) | — |
| `AsmCommands::asmTable::tableIndex`,`wvf`,`nameIndex`,`isHold`,`fourChannel`,`holdCount`,`suppress`,`isHoldMode`,`reg`,`regVal` | yes/no | medium | mirrors asmPlay set | rename in lockstep | coordinated-rename |
| `AsmCommands::asmWtrigLSPlaceholder::value` (param) | no | medium | matches role | keep current (high) | — |
| `AsmCommands::fb::value` (param) | no | medium | generic, single use | keep current (high) | — |
| `AsmCommands::setWavetableFrontIndex::value` (param) | no | high | conventional setter | keep current (high) | — |
| `AsmCommands::emitEntry(1-arg)::instr` (param) | no | high | matches type | keep current (high) | — |
| `AsmCommands::emitEntry(2-arg)::instr`,`overrideWavetableFront` (params) | no | high | matches role | keep current (high) | — |
| `AsmCommands::emitEntry::result` (local) | no | high | conventional | keep current (high) | — |
| `AsmCommands::emitNodeEntry::type` (param) | no | high | matches role/type | keep current (high) | — |
| `AsmCommands::emitNodeEntry::result` (local) | no | high | conventional | keep current (high) | — |
| `kDioAddrLow`/`kDioAddrHigh`/`kIdAddrLow`/`kIdAddrHigh` | no | high | matches comment+use | keep current (high) | — |
| `kImm19HalfRange`/`kImm19MaxUnsigned` | no | medium | matches encoding role | keep current (high) | — |

## 3. Detailed findings

### AsmCommands::genPlayConfig::isFourChannelBool  [yes / medium / cross-batch-arbitration]

Evidence:
- `asm_commands.cpp:962-984`  declaration `bool isFourChannelBool` and
  the only write `config.now = isFourChannelBool;` at :984.
- `asm_commands.cpp:1082-1084`  `asmPlay`'s only call:
  `genPlayConfig(currentWvf, isHold, fourChannel, fourChannel, isBool, holdCount, suppress, isHoldMode, trigger);`
  → both `fourChannel` and `isFourChannelBool` slots are filled with
  the wrapper-side `fourChannel`.
- `asm_commands.cpp:1113-1115`  `asmTable`'s call: same shape
  (`fourChannel, fourChannel`), then `false`/`0` for the trailing
  args.
- `custom_functions_play.cpp:660`  `play()` passes `subFunc == SubFunc::Now`
  into asmPlay's `fourChannel` slot.
- `custom_functions_io.cpp:476-477`  `assignWaveIndex` calls
  `genPlayConfig(wf, singleChannel, true, false, false, -1, mask, false, 0);`
  — this is the only direct call site of `genPlayConfig`, and it
  passes literal `false` for `isFourChannelBool` and literal `true`
  for `fourChannel` (the two slots that asmPlay collapses to a single
  parameter). Here `fourChannel=true, isFourChannelBool=false` even
  though the surrounding code is computing a wave-index, not a
  four-channel mode.
- Counterpart: `play_config.cpp:73-74,112` field `PlayConfig::now`
  with comment "transient flag", JSON key `"now"`. Not directly
  verifiable as a `.rodata` string but consumers
  (`prefetch_placesingle.cpp:554,588,635,...`) read it as `bool is4Ch`.

Interpretation:
- Through `asmPlay`, `isFourChannelBool` ends up reflecting the
  semantic `subFunc == Now` flag (from `play()`'s call site). The
  field receiving this value is named `now`. The producer-side
  parameter `isFourChannelBool` does not match either:
  (a) the field it writes (`now`), or
  (b) the `subFunc==Now` semantics carried through from the most
  common caller. The only direct caller (`assignWaveIndex`) passes
  `false`. Inside the function body the parameter is used **only**
  to assign `config.now`.

Judgement:
- The parameter name disagrees with both the field it writes and the
  semantics carried through `asmPlay::fourChannel` from `play()`;
  whether the field name `now` or the parameter name should win is
  the open arbitration noted in batch 38.

Proposals:
- playNow / now / isNow  (medium) — aligns with the field.
- isFourChannelBool      (low)    — keep current; only viable if
  synthesis decides the field is the misnomer.

Cross-reference:
- Counterpart `PlayConfig::now` (field) flagged in
  `38_play_config.md` with status `cross-batch-arbitration`.

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:201-204
- defined: src/asm_commands.cpp:960-1044
- callers: src/asm_commands.cpp:1082-1084, 1113-1115;
  src/custom_functions_io.cpp:476

### AsmCommands::genPlayConfig::isHoldMode  [yes / medium / cross-batch-arbitration]

Evidence:
- `asm_commands.cpp:962-981` declaration `bool isHoldMode`, sole
  write `config.is4Channel = isHoldMode;` at :981.
- `asm_commands.cpp:1083-1084` `asmPlay` forwards its own
  `isHoldMode` parameter into this slot.
- `custom_functions_io.cpp:477`  `assignWaveIndex` passes literal
  `false` for `isHoldMode`.
- `custom_functions_play.cpp:1167`  `playIndexed` passes
  `(subFunc == SubFunc::Aux)` into the wrapper's `isHoldMode` slot.
- `custom_functions_playback.cpp:323`  aux play passes `true` (with
  `isHold=false`); `:614` passes `false`; `:973`,`:1023`
  (`playZero`/`playHold`) pass `false`.
- Counterpart: `play_config.cpp:108,147` JSON key `"is4Channel"`
  (verified verbatim in `.rodata`); consumers
  (`prefetch_helpers.cpp:213-216`,
  `prefetch.cpp:131,395,427,450`) read the field as the
  four-channel flag.

Interpretation:
- The field receives a value that, across call sites, is sometimes
  `false` (assignWaveIndex, simple play, playZero) and sometimes
  `subFunc == Aux` / hard-coded `true` for the "aux" / playIndexedAux
  path. The field's verified JSON key and the consumers'
  `is4Ch`/`fourChannel` interpretation contradict the parameter name
  `isHoldMode`. The parameter is used only to set this one field.

Judgement:
- The parameter name is at odds with the field it writes and with
  the JSON-anchored field semantics; whether the parameter name or
  the field name should be canonical is the open arbitration.

Proposals:
- is4Channel    (medium)
- isHoldMode    (low) — keep current.

Cross-reference:
- Counterpart `PlayConfig::is4Channel` (field) flagged
  `not-misnomer` in `38_play_config.md` with explicit
  `Cross-reference` to this parameter.

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:204
- defined: src/asm_commands.cpp:960-981
- callers: src/asm_commands.cpp:1083; src/custom_functions_io.cpp:477;
  src/custom_functions_play.cpp:1167;
  src/custom_functions_playback.cpp:315-327, 591-619, 970-974,
  1020-1024

### AsmCommands::genPlayConfig::holdCount  [yes / high / cross-batch-arbitration]

Evidence:
- `asm_commands.cpp:962-979` declaration `int holdCount`, sole write
  `config.rate = holdCount;` at :979.
- `asm_commands.cpp:1083`  `asmPlay` forwards its `holdCount` here.
- `custom_functions_io.cpp:477`  `assignWaveIndex` passes literal
  `-1` (commented "rate" / negative-as-default sentinel).
- `custom_functions_play.cpp:1165`  `playIndexed` passes `rate`.
- `custom_functions_playback.cpp:321`,`:613`,`:973`,`:1023`  every
  asmPlay caller passes `rate` (or a mask) into this slot.
- Counterpart: `play_config.cpp:106` JSON key `"rate"`; consumers
  treat the field as rate/`-1`-sentinel
  (`play_config.cpp:39,79,94,106,144`).

Interpretation:
- Every call site supplies a rate value (typically the
  `parseOptionalRate` result, with `-1` meaning "use default"). The
  field's name and JSON key say `rate`. The parameter name
  `holdCount` does not match any caller's actual value, the field
  it writes, or the field's serialized name. Inside the function
  body the parameter is used only for that one assignment.

Judgement:
- Misnomer; every signal points at "this is a rate".

Proposals:
- rate         (high)
- holdCount    (low) — keep current.

Cross-reference:
- Counterpart `PlayConfig::rate` (field) flagged `not-misnomer` in
  `38_play_config.md` with explicit `Cross-reference` to this
  parameter.

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:203
- defined: src/asm_commands.cpp:960-979
- callers: src/asm_commands.cpp:1083; src/custom_functions_io.cpp:477;
  src/custom_functions_play.cpp:1165;
  src/custom_functions_playback.cpp:309-321, 589-617, 962-973,
  1011-1023

### AsmCommands::genPlayConfig::isHold  [yes / medium / —]

Evidence:
- `asm_commands.cpp:962-1003` declaration `bool isHold`. Body uses:
  `:1003` `if (!isHold) channelMask = fullMask;`
  `:1039` `if (!isHold) adjusted = markerBits;`
- `custom_functions_io.cpp:470-477`  `assignWaveIndex` builds local
  `bool singleChannel = (channelArgs.size() < 2 || channelArgs[1].value_.toString().empty());`
  then passes it as `isHold`. The variable name and the way it is
  computed describe a "second channel is empty" predicate, not a
  hold flag.
- `custom_functions_playback.cpp:610` `play()`-family path:
  `isHold = dryRun` (a bool computed from "is the first arg's
  string-form empty"). Again, not a hold flag.
- `custom_functions_play.cpp:1162` `playIndexed`:
  `isHold = (subFunc == SubFunc::Aux)`.
- `custom_functions_playback.cpp:319,612,972,1022` other paths pass
  literal `false`.

Interpretation:
- Inside the body, the parameter gates two things:
  1. when `false`, expand `channelMask` from the
     `(channels==1 ? 2 : fullMask)` two-bit pattern back to the full
     channel bitmask;
  2. when `false`, drop the marker-bit `<<2` shift (use raw markerBits).
  Both behaviours are about "are we encoding a single-channel /
  small-mask play". Calls supply values that span "single channel"
  (assignWaveIndex), "dryRun" (play), `subFunc==Aux`, and constant
  `false`. None of the sites is a "hold" flag, and the body never
  refers to holding behaviour — `PlayConfig::hold` is set from the
  separate `isBool` parameter.

Judgement:
- Misnomer: the parameter is not a hold indicator; in the body it
  controls the small-channel/single-channel encoding shrink.

Proposals:
- singleChannel   (medium)
- shrinkChannels  (low)
- isHold          (low) — keep current.

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:202
- defined: src/asm_commands.cpp:962-1041
- callers: as listed above.

### AsmCommands::genPlayConfig::isBool  [yes / high / —]

Evidence:
- `asm_commands.cpp:962-985` declaration `bool isBool`, sole write
  `config.hold = isBool;` at :985.
- Caller list: every asmPlay/asmTable site passes a value
  semantically equivalent to "is this a hold play":
  `custom_functions_playback.cpp:1022`  playHold passes `true`
  (commented `isBool — binary 0x1391d7: push $0x1`);
  `custom_functions_playback.cpp:972`  playZero passes `false`;
  `custom_functions_play.cpp:1164`  playIndexed passes `false`;
  `custom_functions_io.cpp:477`  assignWaveIndex passes `false`.
- Counterpart: `PlayConfig::hold` field, JSON key `"hold"`
  (verified verbatim in `.rodata`, `38_play_config.md` row).

Interpretation:
- The parameter is used only to write `config.hold`. The verified
  field name is `hold`. The only caller that supplies `true` is
  `playHold`. The parameter name `isBool` is the textbook generic
  bool-named-after-its-type and carries no semantic content.

Judgement:
- Misnomer: this is the hold flag; the field even matches.

Proposals:
- hold          (high)
- isHoldFlag    (medium)
- isBool        (low) — keep current.

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:202
- defined: src/asm_commands.cpp:960-985
- callers: as above.

### AsmCommands::genPlayConfig::fourChannel  [yes / low / —]

Evidence:
- `asm_commands.cpp:960-1044` declaration `bool fourChannel`. Body
  contains **no read of this parameter**. (The field
  `PlayConfig::fourChannel`-style channel count lives in
  `is4Channel`, written from `isHoldMode`.)
- Comment in body, `:993` notes a previous incorrect analysis where
  `dummy = fourChannel`; that was struck out in favour of
  `dummy = (waveform == nullptr)`.
- Caller `asm_commands.cpp:1082-1084,1113-1115`: both `asmPlay` and
  `asmTable` pass their own `fourChannel` argument here AND into the
  next parameter (`isFourChannelBool`).

Interpretation:
- The parameter is unused inside `genPlayConfig`. Whether the binary
  uses it (and we missed a code path) is open; statically, it
  contributes nothing. Calling it `fourChannel` is technically
  arbitrary because nothing inside the function depends on it.

Judgement:
- Unsure-but-flagged: the name describes an intent that the body
  does not exhibit. Either it's a dead parameter (rename to
  `unused`/remove) or there is a missing branch that should consume
  it.

Proposals:
- keep current   (medium) — preserves the API shape until a missing
  branch is found.
- fourChannel_unused  (low) — explicit dead-flag marker.

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:202
- defined: src/asm_commands.cpp:960-1044

### AsmCommands::asmPlay::holdCount  [yes / high / coordinated-rename]

Evidence:
- `asm_commands.hpp:206-210` declaration `int holdCount`.
- `asm_commands.cpp:1083` only use: forwarded as `genPlayConfig`'s
  `holdCount`.
- All wrapper-call sites listed under `genPlayConfig::holdCount`
  pass `rate` here as well.

Interpretation:
- This wrapper parameter is a 1-for-1 forward of the misnamed
  `genPlayConfig::holdCount`. Same evidence applies.

Judgement:
- Same misnomer as `genPlayConfig::holdCount`.

Proposals:
- rate          (high) — rename in lockstep.
- holdCount     (low)

Cross-reference:
- Lockstep with `AsmCommands::genPlayConfig::holdCount`,
  `AsmCommands::asmTable::holdCount`, and field-side counterpart
  `PlayConfig::rate` (batch 38).

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:208
- defined: src/asm_commands.cpp:1046-1104

### AsmCommands::asmPlay::isHoldMode  [yes / medium / coordinated-rename]

Evidence:
- `asm_commands.hpp:208`, `asm_commands.cpp:1083` — wrapper forwards
  to `genPlayConfig::isHoldMode`.
- See `genPlayConfig::isHoldMode` evidence above for caller values.

Interpretation:
- Direct forward of the cross-batch-arbitration parameter.

Judgement:
- Same situation; resolution mirrors the producer-side decision.

Proposals:
- is4Channel    (medium)
- isHoldMode    (low)

Cross-reference:
- Lockstep with `genPlayConfig::isHoldMode`, `asmTable::isHoldMode`,
  and `PlayConfig::is4Channel` (batch 38).

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:208
- defined: src/asm_commands.cpp:1046-1104

### AsmCommands::asmPlay::isHold / asmPlay::isBool / asmPlay::fourChannel  [yes-yes-unsure / medium-medium-low / —]

Evidence:
- `asm_commands.hpp:207`, `asm_commands.cpp:1082-1084` — all three
  forward verbatim to `genPlayConfig`.
- Same caller-side evidence as the producer-side parameters above.

Interpretation:
- Mirror of the producer-side findings; same verdicts.

Judgement:
- `isHold` → singleChannel (mild misnomer);
  `isBool` → hold (clear misnomer);
  `fourChannel` → unsure (unused in producer body).

Proposals:
- isHold:      singleChannel (medium); keep current (low)
- isBool:      hold (high); keep current (low)
- fourChannel: keep current (medium); fourChannel_unused (low)

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:207
- defined: src/asm_commands.cpp:1046-1104

### AsmCommands::asmPlay::reg / regVal / reg2  [yes / medium / —]

Evidence:
- `asm_commands.hpp:209-210` `AsmRegister reg, int regVal, AsmRegister reg2`.
- `asm_commands.cpp:1087-1089`:
  `node->lengthReg = reg;`
  `node->indexOffsetReg = reg2;`
  `node->length = regVal;`
- `custom_functions_play.cpp:1163-1170` (playIndexed) labels them
  inline as `indexReg`, `waveIndex`, `regInv`.
- `custom_functions_playback.cpp:298-326` (playDIO‑Wave) labels them
  `reg`, `regVal=0`, `regInv` and uses them as a placeholder/no-op.
- `custom_functions_playback.cpp:582-619` (other site) uses `reg0`,
  `regVal=rate`, `regInv`.
- `custom_functions_playback.cpp:968-974,1018-1024` (playZero/Hold)
  use `reg0`, `regVal=length`, `regArg`.

Interpretation:
- The `Node` field names (`lengthReg`, `indexOffsetReg`, `length`)
  describe what these slots are. The wrapper-side names are generic
  `reg`/`regVal`/`reg2`. Across call sites the actual values bound
  are: a length register OR an index register OR `R0`; an int that
  is sometimes a length, sometimes a wave-index, sometimes a mask;
  and almost always `AsmRegister(-1)` (`regInv`) or an index-offset
  register.

Judgement:
- The wrapper-layer names obscure that the values flow into
  `Node::lengthReg`, `Node::length`, `Node::indexOffsetReg`. The
  naming inconsistency vs the destination fields is the issue.

Proposals:
- reg → lengthReg or indexReg     (medium)
- regVal → length or waveIndex    (medium)
- reg2 → indexOffsetReg            (medium)
- keep current                     (low) — wrapper's role is a
  conduit; field-side already says it.

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:206-210
- defined: src/asm_commands.cpp:1087-1089
- callers: as listed.

### AsmCommands::asmPlay::nameIndex  [yes / medium / —]

Evidence:
- `asm_commands.hpp:207` `int nameIndex`.
- `asm_commands.cpp:1064` only use: `node->deviceIndex = nameIndex;`.
- Callers: `custom_functions_play.cpp:647-664` uses local
  `playDeviceIndex` (= `channelIndex` or `config_->deviceIndex`);
  playZero/Hold (`custom_functions_playback.cpp:967,1017`) pass
  `config_->deviceIndex`.

Interpretation:
- The value is the device/channel-group index, written into a field
  literally called `deviceIndex`. The parameter name `nameIndex` is
  inherited from older "wave name index" reasoning and does not
  match the destination field or the actual call-site values.

Judgement:
- Mild misnomer — the field-side `Node::deviceIndex` is the
  authoritative semantic.

Proposals:
- deviceIndex     (medium)
- channelIndex    (low)
- nameIndex       (low) — keep current.

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:207
- defined: src/asm_commands.cpp:1063-1080
- callers: src/custom_functions_play.cpp:647-664;
  src/custom_functions_playback.cpp:967,1017.

### AsmCommands::asmTable parameters (group)  [yes-no / medium / coordinated-rename]

Evidence:
- `asm_commands.hpp:212-215` `asmTable(int tableIndex,
  std::shared_ptr<WaveformFront> wvf, int nameIndex, bool isHold,
  bool fourChannel, int holdCount, unsigned int suppress, bool
  isHoldMode, AsmRegister reg, int regVal)`.
- `asm_commands.cpp:1106-1129`. `:1113-1115`
  `node->config = genPlayConfig(wvf, isHold, fourChannel,
                                fourChannel, isHoldMode, holdCount,
                                suppress, false, 0);`
  i.e. the wrapper passes its `isHoldMode` into `genPlayConfig`'s
  **`isBool`** slot, not into `genPlayConfig::isHoldMode`.
  `:1117-1118` `node->lengthReg = reg; node->length = regVal;`
  `:1125-1126` `node->deviceIndex = nameIndex; node->tableIndex = tableIndex;`
- No reachable caller of `asmTable` was found in the in-repo source
  (`rg "asmCommands_->asmTable"` returns no hits).

Interpretation:
- `asmTable::tableIndex` matches `Node::tableIndex` — fine.
- `asmTable::isHold`, `isBool`, `fourChannel`, `holdCount`,
  `isHoldMode` — same name family as `asmPlay`, but wired
  differently into `genPlayConfig` (compare argument positions).
  Specifically, `asmTable::isHoldMode` is forwarded into
  `genPlayConfig::isBool`, which writes `PlayConfig::hold`. This
  contradicts the `asmPlay`-layer convention that `isHoldMode`
  forwards to `genPlayConfig::isHoldMode` and writes
  `PlayConfig::is4Channel`. Whether this is intentional or a
  reconstruction transcription error in `asmTable`'s genPlayConfig
  call cannot be decided here; either way, the parameter names are
  the same as `asmPlay`'s and inherit the same misnomers.
- `asmTable::reg`, `regVal`, `nameIndex` — same situation as
  `asmPlay`'s.

Judgement:
- Same misnomer-set as `asmPlay`; coordinated rename. The
  argument-routing mismatch in `genPlayConfig` is recorded for the
  synthesis step but is a code-correctness question, not a naming
  one (recorded in `incidental_findings.md` separately if pursued).

Proposals:
- Mirror `asmPlay`: `holdCount`→`rate`, `isHoldMode`→`is4Channel`,
  `isBool`→`hold`, `isHold`→`singleChannel`,
  `fourChannel`→`fourChannel_unused`, `nameIndex`→`deviceIndex`.

Cross-reference:
- Coordinated with `asmPlay` parameter set and with `genPlayConfig`
  parameter set (which holds the cross-batch-arbitration entries).

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:212-215
- defined: src/asm_commands.cpp:1106-1129

### AsmCommands::br/brz/brnz/brgz::flag  [yes / high / coordinated-rename]

Evidence:
- `asm_commands.hpp:59-62` four declarations all with `bool flag`.
- `asm_commands.cpp:162-164`  `br(label, flag)` →
  `brz(AsmRegister::Reg(0), label, flag);` (forward only).
- `asm_commands.cpp:170`  `brz` →
  `impl_->brz(reg, label, flag, wavetableFrontIndex_);`. Inside
  `AsmCommandsImpl{Cervino,Hirzel}::brz` (batch 49) the only use is
  `result.isWaveformCmd = flag;`.
- `asm_commands.cpp:188`  `brnz` body sets `result.isWaveformCmd = flag;`
  directly. Comment at :188: `// directly stored`.
- `asm_commands.cpp:207`  `brgz` body sets `result.isWaveformCmd = flag;`.
- All call sites pass literal `false`:
  `prefetch_splitplay.cpp:364`, `:385`, `:393`;
  `custom_functions_io.cpp:1906`, `:1968`.

Interpretation:
- The parameter's only effect at every layer (wrapper and impl) is
  to set `AsmList::Asm::isWaveformCmd`. The name `flag` is the
  generic bool-no-semantics name. Verbatim same finding as batch 49
  for `AsmCommandsImpl::brz::flag`.

Judgement:
- Misnomer; the value is `isWaveformCmd`.

Proposals:
- isWaveformCmd  (high)
- keep current   (low)

Cross-reference:
- Coordinated with `AsmCommandsImpl::brz::flag` (batch 49,
  `coordinated-rename`).

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:59-62
- defined: src/asm_commands.cpp:162-209
- callers: src/prefetch_splitplay.cpp:364,385,393;
  src/custom_functions_io.cpp:1906,1968

### AsmCommands::syncCervino::flag  [unsure / low / —]

Evidence:
- `asm_commands.hpp:163` `AsmList syncCervino(AsmRegister reg1,
  AsmRegister reg2, bool flag) const;`.
- `asm_commands.cpp:719-786`. `flag` selects between two distinct
  instruction sequences (8 vs 7 instructions) writing to user
  registers `0x44` vs `0x45` respectively. The body comment at
  `:701-718` documents both branches in depth.
- Caller: `prefetch_placesingle.cpp:1014`
  `asmCommands_->syncCervino(reg1, reg2, noSeq);` — local is named
  `noSeq` (a presumed "no sequencer" / "not-sequenced" bool).

Interpretation:
- The parameter selects between two sync sequences; whether it is
  semantically "no-sequencer flag", "host-vs-device", or something
  else cannot be decided from the in-tree evidence. The caller's
  local `noSeq` is the only hint and is itself reconstructed.

Judgement:
- Unsure: name is uninformative but the right replacement is not
  determinable from this batch.

Proposals:
- keep current   (medium)
- noSeq          (low) — match caller.

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:163
- defined: src/asm_commands.cpp:719-786
- callers: src/prefetch_placesingle.cpp:1014

### AsmCommands::asmPrefetch::nameIndex / regVal / extraVal  [yes / medium / —]

Evidence:
- `asm_commands.hpp:194-195`  `asmPrefetch(std::shared_ptr<WaveformFront> wvf,
  int nameIndex, int regVal, int extraVal)`.
- `asm_commands.cpp:941-954`:
  `result.node->lengthReg = static_cast<AsmRegister>(regVal);`
  `result.node->length = extraVal;`
  `result.node->wavesPerDev[nameIndex] = wvf->name;`
  `result.node->deviceIndex = nameIndex;`
- Callers `custom_functions_play.cpp:615,635`:
  `asmCommands_->asmPrefetch(combinedWf, channelIndex, 0, ...)` — the
  second param is a channel/device index.

Interpretation:
- `nameIndex` is written into `Node::deviceIndex`; calls supply a
  channel/device index. `regVal` is converted directly to an
  `AsmRegister` and stored in `lengthReg`. `extraVal` is stored in
  `Node::length`. None of the names match the destination fields or
  the supplied values.

Judgement:
- Misnomer-trio, parallel to the `asmPlay` `nameIndex/reg/regVal`
  family.

Proposals:
- nameIndex → deviceIndex  (medium)
- regVal    → lengthRegNum (medium)
- extraVal  → length        (medium)
- keep current             (low)

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:194-195
- defined: src/asm_commands.cpp:941-954
- callers: src/custom_functions_play.cpp:615,635

### AsmCommands::prf::intArg  [yes / medium / —]

Evidence:
- `asm_commands.hpp:44`  `prf(AsmRegister reg1, AsmRegister reg2, int intArg)`.
- `asm_commands.cpp:88-99`: `instr.outputs.emplace_back(intArg);`
  with comment `"// child[2]: 20-bit immediate"`.
- Callers (`prefetch_splitplay.cpp:376`,
  `prefetch_placesingle.cpp:213,312,333,346,352,701,832,944,1111`)
  pass things named `clampedSize`, `halfSize`, `cacheSize`,
  `prfSize`, `pageCount` — uniformly a transfer size.

Interpretation:
- The actual value is a prefetch size / 20-bit immediate. The
  parameter name `intArg` is the textbook "named after its type"
  generic.

Judgement:
- Misnomer.

Proposals:
- size       (medium)
- immediate  (low)
- keep current (low)

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:44
- defined: src/asm_commands.cpp:88-99
- callers: as listed.

### AsmCommands::smap::arg  [yes / low / —]

Evidence:
- `asm_commands.hpp:146`  `smap(AsmRegister r1, AsmRegister r2, int arg)`.
- `asm_commands.cpp:633`: `alui(Assembler::ADDI, r1, AsmRegister::Reg(0), Immediate(arg));`
  i.e. the value is an immediate to load into `r1`.
- No call site found in the surveyed files.

Interpretation:
- `arg` is the textbook generic name. The value is loaded as an
  immediate, but with no call sites to triangulate its meaning, the
  best replacement is uncertain.

Judgement:
- Mild misnomer (generic name); replacement uncertain without
  callers.

Proposals:
- value     (low)
- immediate (low)
- keep current (medium)

Locations consulted:
- declared: include/zhinst/asm_commands.hpp:146
- defined: src/asm_commands.cpp:627-644

## 4. Symbols inspected and judged routinely fine

Members:
- `impl_` — unique_ptr to `AsmCommandsImpl`; init from `getInstance`;
  used only via `impl_->...` for delegated opcodes. Name fits.
- `wavetable_` — shared_ptr to `WavetableFront`; held but not used
  inside the methods reconstructed in this TU; ctor takes
  `wavetable` and moves into the member. Name fits the type.
- `errorHandler_` — `std::function<void(const std::string&)>` used
  by `toInt32` to report overflow/underflow. Name fits.
- `wavetableFrontIndex_` — int at +0x50, used as the
  `wavetableFront` field of every emitted asm entry; mutated through
  `setWavetableFrontIndex(value)` (header comment). Name reused
  consistently across the impl batch (see batch 49 `lineNumber`
  forwarding). Mild noise: the same value is referred to as
  `lineNumber` at the impl layer, but the question of which name
  should win is external to this batch (already noted in batch 49).
- `numChannelGroups_` — int at +0x54, init from
  `config.numChannelGroups`, used only as the `numChannelGroups`
  ctor arg of `Node`. Name matches.

Constructor params:
- `config`, `wavetable`, `errorHandler` — all conventional
  forwarding-into-member names.

Wrapper params (mostly `reg`/`dst`/`src`/`imm`/`label`/`addr`/
`highBank`/`type`/`flags`/`rate`/`msg`/`isError`/`tableIndex`/`reg1`/
`reg2`/`r1`/`r2`/`addr`):
- All `dst`/`src`/`imm` triplets in the ALU family — assigned
  directly to `instr.regDst`/`regSrc`/`outputs` with matching field
  meanings; verified in `aluiu`, `alui`, `addi32` and the ALU-with-
  Value variants (which forward through `toInt32` then back to the
  Immediate variant).
- `cmd` parameters in `alur`/`aluiu`/`alui` — passed straight to
  `instr.cmd` and used in `commandToString(cmd)` for error messages.
- `wvf::reg`/`dstReg`/`length` and `wvfi`/`wvfs`/`wvft` analogues —
  match the wrapper-layer conventions (and, per batch 49, are the
  consistent layer that the impl layer mostly fails to mirror).
- `ld`/`st`/`luser`/`suser`/`ldio`/`sdio`/`sid`/`ltrig`/`strig`/
  `sinttrig`/`lcnt` — `reg`, `addr`, `highBank` all match the
  observable role exactly.
- `wtrig::r1,r2`, `wtrigi::value` — fine.
- `asmZero/asmOne::reg` — name matches "register to write 0/1 into".
- `asmLabel::label`, `asmMessage::msg`/`isError` — match.
- `asmRate::rate` — written to `result.node->globalRate`; matches.
- `asmSetPrecompFlags::flags` — written to
  `result.node->defaultPrecompFlags`; matches.
- `asmSetVarPlaceholder::reg` — written to `result.node->lengthReg`;
  the type is `AsmRegister`, the role is "set the variable
  placeholder's length register". Fine.
- `asmLockPlaceholder/asmUnlockPlaceholder::wvf,index` — match.
- `asmPrefetch::wvf` — fine; nameIndex/regVal/extraVal flagged in §3.
- `genPlayConfig::wvf,suppress,trigger` — match field names.
- `asmPlay::waveforms,suppress,trigger` — match field names.
- `asmTable::tableIndex,wvf,suppress` — match field names.
- `asmWtrigLSPlaceholder::value` — generic but single use (`+0x40`
  offset before storing as ST output); fine.
- `fb::value` — generic but the `Assembler::FB` immediate; fine.
- `setWavetableFrontIndex::value` — conventional setter name.

Helpers:
- `emitEntry`/`emitNodeEntry` and their locals (`result`, `instr`,
  `type`, `overrideWavetableFront`) — all names match role.

Locals:
- `prf`/`wprf`/`wwvf`/`wwvfq`/`wvf*`/`cwvf*`/`alur*`/`alui*`/`addi32`/
  every load-store/trigger/sync helper — `instr` (working
  `AssemblerInstr`), `result` (working entry/list), `r0`, `chosen`,
  `tmp`/`tmp2` (sub-results), `wfIndex1`/`wfIndex2`/`seqId1`/`seqId2`/
  `entry1`/`entry2`/`instr1`/`instr2` in `unsyncCervino`,
  `playNode`/`tail` etc. — each has an obvious role from the
  immediately surrounding lines.
- `genPlayConfig` body locals (`wf`, `channels`, `fullMask`,
  `channelMask`, `mbits`, `data`, `count`, `pairs`, `b0`, `b1`,
  `v0`, `v1`, `markerBits`, `adjusted`, `b`, `v`) — all describe
  the marker-pack/channel-mask derivation steps unambiguously.
- `asmPlay` locals (`node`, `currentWvf`, `wfCopy`, `wf`,
  `playDeviceIndex` at the call site) — all fine.

Namespace constants:
- `kDioAddrLow=0x20`, `kDioAddrHigh=0x1FE`, `kIdAddrLow=0x21`,
  `kIdAddrHigh=0x1FF` — the comments at declaration explicitly map
  each to "DIO register (low/high bank)" and "ID register
  (low/high bank)"; the `ldio/sdio/sid` users select one based on
  the `highBank` parameter. Names match.
- `kImm19HalfRange=0x7FFFF`, `kImm19MaxUnsigned=0xFFFFD` — used in
  the `(static_cast<uint32_t>(sval + kImm19HalfRange) <=
  kImm19MaxUnsigned)` 19-bit-signed range test in `alui`. Names
  describe their role.

## 5. Coverage

Fully covered:
- All 5 data members of `AsmCommands`.
- All ~78 wrapper method declarations and their parameter lists.
- All locals inside the methods that have any (in particular the
  large `alui`, `addi32`, `genPlayConfig`, `asmPlay`, `asmTable`,
  `syncCervino`, `unsyncCervino`).
- The two anonymous-namespace constant blocks at the top of
  `asm_commands.cpp`.
- Cross-file callers of `asmPlay`, `asmTable`, `genPlayConfig`,
  `asmPrefetch`, `prf`, `brz`/`brnz`/`brgz`/`br`, `wvf`/`wvfi`/
  `wvfs`/`wvft`, `wtrig`/`wtrigi`/`sid`/`smap`/`fb`/`asmMessage`/
  `syncCervino`/`asmRate` — sampled at the locations cited in §3
  and in the call-site grep run during the audit.
- The three play_config cross-batch counterparts inside
  `genPlayConfig`: `isFourChannelBool`, `isHoldMode`, `holdCount` —
  arbitration recorded in §3 with explicit `Cross-reference` lines.

Out of scope per §3:
- Type name `AsmCommands` (in `nm`).
- All ~78 wrapper method names (in `nm`).
- The constructor (in `nm`).
- The class has no static data members; nothing tier-1 there.

Deferred (cross-batch):
- The producer↔field arbitration on
  `isFourChannelBool`/`now`,
  `isHoldMode`/`is4Channel`,
  `holdCount`/`rate` is intentionally left as
  `cross-batch-arbitration`; this batch's recommendation in each
  proposal block carries the synthesis suggestion, but the final
  pick is for synthesis.
- Whether the wrapper-layer
  `wvf/wvfi::dstReg`/`length`/`reg` should rename in lockstep with
  the impl-layer `markerReg`/`waveIndex`/`waveReg` flagged in batch
  49 is left to synthesis (this batch agrees the wrapper naming is
  the preferred target where they conflict).

Not deferred but worth noting for synthesis:
- The `asmTable` body forwards its `isHoldMode` argument into
  `genPlayConfig::isBool` (not into `genPlayConfig::isHoldMode`).
  This is a code-correctness question (likely a transcription
  error in the reconstruction), not a naming question; flagged
  here so synthesis is aware that the `coordinated-rename`
  bundling for `asmPlay::isHoldMode` and `asmTable::isHoldMode`
  needs to be reconciled with that wiring before any rename is
  executed.
