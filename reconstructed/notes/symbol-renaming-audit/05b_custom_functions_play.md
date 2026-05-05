# Batch 05b — custom_functions_play

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 12 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 2; B2 (borderline, deferred): 10;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/src/runtime/custom_functions_play.cpp` (2478 lines)

Header `reconstructed/include/zhinst/runtime/custom_functions.hpp` was opened
read-only solely to confirm method declarations and signatures. All
header-side renaming is out of scope for this sub-batch (handled by the
header / class batch).

`nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so` was
consulted. All ten methods reconstructed in this file appear there
verbatim:

```
0000000000160e00 t zhinst::CustomFunctions::playIndexed(...)
0000000000164550 t zhinst::CustomFunctions::writeToNode(EvalResultValue, EvalResultValue, EvalResultValue, shared_ptr<Resources>)
000000000016dbb0 t zhinst::CustomFunctions::writeLS64bit(unsigned long, int, int, ...)
000000000016d8c0 t zhinst::CustomFunctions::addWaitCycles(int, ...)
000000000016bb30 t zhinst::CustomFunctions::addSyncCommand(...)
000000000015e060 t zhinst::CustomFunctions::mergeWaveforms(... const&, short, bool, ... const&, int, bool)
000000000015a9f0 t zhinst::CustomFunctions::generateWaveform(... const&, ... const&, ...)
000000000015ca90 t zhinst::CustomFunctions::setWaitCyclesReg(...)
000000000015f090 t zhinst::CustomFunctions::play(... const&, ..., zhinst::CustomFunctions::SubFunc)
000000000016c470 t zhinst::CustomFunctions::printF(... const&, ... const&)
```

→ All ten method names and the enclosing type `CustomFunctions` are
**excluded from rename per RULES §3 tier-1**. The file-static regex
objects (`absDevRegex`, `awgNodeRegex`, `sineNodeRegex`,
`oscselNodeRegex`) appear as function-static data symbols
(`b 0xb84748..0xb84790`) → also tier-1 authoritative; **excluded**.
Their guard-byte symbols at +0x10 confirm the same names.

In scope here: parameters of these methods, the file-static helper
`appendSuser`, and locally-named variables that may mislead a reader.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `appendSuser::list` / `::vec` / `::cmds` / `::reg` / `::addr` | no | medium | params match usage one-to-one | — | not-misnomer |
| `setWaitCyclesReg::args` | no | medium | EvalResultValue arg vector | — | — |
| `setWaitCyclesReg::results` | no | medium | output EvalResults sink | — | — |
| `setWaitCyclesReg::res` | unsure | low | unused in body | `keep current`, `unused` | — |
| `setWaitCyclesReg::devType` (local) | no | medium | reads `config_->deviceType` | — | — |
| `setWaitCyclesReg::supported` (local) | no | medium | bitmask membership flag | — | — |
| `setWaitCyclesReg::shifted` (local) | yes | low | meaningless name; semantic role | `devTypeBiased`, `keep current` | — |
| `setWaitCyclesReg::waitReg` (local) | no | high | passed to suser(0x6F=kSuserWaitLegacy) | — | — |
| `setWaitCyclesReg::immVal` (local) | no | medium | const int loaded via addi | — | — |
| `setWaitCyclesReg::regNum` / `newReg` (locals) | no | medium | fresh AsmRegister allocation | — | — |
| `mergeWaveforms::args` | no | medium | wave-arg vector | — | — |
| `mergeWaveforms::channelCount` | no | medium | matches Phase 7 channel cap | — | — |
| `mergeWaveforms::useYSuffix` | unsure | low | also gates merge/interleave | `useYSuffix`, `interleaveSecondary` | cross-batch-arbitration |
| `mergeWaveforms::callerName` | no | medium | error-format key arg | — | — |
| `mergeWaveforms::requestedLength` | no | medium | compared to maxSampleLen | — | — |
| `mergeWaveforms::useFunDescrPath` | unsure | low | path-A vs path-B selector | `keep current`, `useExplicitFunDescr` | — |
| `mergeWaveforms::values` (local) | no | high | vector<Value> for generator | — | — |
| `mergeWaveforms::maxSampleLen` (local) | no | medium | tracks max getWaveformSampleLength | — | — |
| `mergeWaveforms::wfName` (local) | no | medium | per-arg waveform name | — | — |
| `mergeWaveforms::funDescr` (local) | no | medium | func-descriptor key | — | — |
| `mergeWaveforms::multiValue` (local) | no | medium | values.size()>=2 flag | — | — |
| `mergeWaveforms::optName` (local) | no | medium | arg of getWaveformByName | — | — |
| `mergeWaveforms::result` (local) | no | medium | sret slot | — | — |
| `mergeWaveforms::funDescr2` (local) | yes | low | duplicates outer `funDescr` | `funDescrA`, `keep current` | — |
| `mergeWaveforms::sig` (local) | no | medium | Signal returned by generator | — | — |
| `mergeWaveforms::factory` (local) | no | medium | std::function factory | — | — |
| `mergeWaveforms::actualChannels` (local) | no | high | matches Signal::channels() | — | — |
| `mergeWaveforms::requested` (local) | unsure | low | shadows `channelCount` | `requestedChannels`, `keep current` | — |
| `mergeWaveforms::wf` (local) | no | low | brief WaveformFront* alias | — | — |
| `play::args` / `::res` | no | medium | standard pattern | — | — |
| `play::subFunc` | no | medium | matches enum class param | — | — |
| `play::cmdName` (local) | no | medium | string fed to error format | — | — |
| `play::argsCopy` (local) | no | medium | local mutable copy | — | — |
| `play::playLength` (local) | yes | medium | not length — DigTrigger first arg | `firstArgVal`, `digTriggerCount` | — |
| `play::firstVal` (local) | no | medium | front of argsCopy | — | — |
| `play::playArgs` (local) | no | medium | PlayArgs instance | — | — |
| `play::parseEnd` (local) | no | medium | PlayArgs::parse return iterator | — | — |
| `play::rate` (local) | no | medium | parseOptionalRate result | — | — |
| `play::results` (local) | no | medium | shared EvalResults out | — | — |
| `play::maxSampleLen` (local) | no | medium | PlayArgs::getMaxSampleLength | — | — |
| `play::numChannels` (local) | yes | low | actually `numChannelGroups` | `numChannelGroups`, `keep current` | — |
| `play::channelIndex` (local) | yes | low | actually `deviceIndex` | `deviceIndex`, `keep current` | — |
| `play::channelWaveforms` (local) | no | medium | per-channel results | — | — |
| `play::mask` (local) | yes | low | trigger-bit mask | `triggerMask`, `keep current` | — |
| `play::ch` (loop) | no | medium | channel index | — | — |
| `play::assignments` (local) | no | medium | WaveAssignment vector ref | — | — |
| `play::channelArgs` / `waIndex` / `wa` | no | medium | clear in context | — | — |
| `play::name` (local) | unsure | low | only used to gate mask code | `keep current`, `waveName` | — |
| `play::shift` (local) | no | medium | bit shift = waIndex*7 | — | — |
| `play::isSecondaryChannel` (local) | unsure | low | passed as `useFunDescrPath` | `keep current`, `useFunDescrPath` | cross-batch-arbitration |
| `play::maxSampleLength` (inner local) | no | low | shadow of outer maxSampleLen | `keep current` | — |
| `play::combinedWf` (local) | no | medium | merged waveform for channelIndex | — | — |
| `play::asmEntry` / `wfCopy` / `playDeviceIndex` | no | medium | clear in context | — | — |
| `play::reg0` / `regInv` (locals) | yes | low | placeholder zero / -1 regs | `regZero`/`regInvalid`, `keep current` | — |
| `playIndexed::args` / `::res` / `::subFunc` | no | medium | same as `play()` | — | — |
| `playIndexed::cmdName` | no | medium | error-format key | — | — |
| `playIndexed::indexed` (local) | unsure | low | passed as PlayArgs ctor flag | `isAux`, `keep current` | cross-batch-arbitration |
| `playIndexed::parseEnd` (local) | no | medium | parse-iterator | — | — |
| `playIndexed::vt0` / `vt1` (locals) | no | low | bit-test inputs | — | — |
| `playIndexed::rateBegin` (local) | yes | medium | not "rate" — index/length-skipped iter | `parseEndAfterIndexLen`, `keep current` | — |
| `playIndexed::rate` (local) | no | medium | parseOptionalRate result | — | — |
| `playIndexed::waveIndex` (local) | yes | medium | reads parseEnd[1] (length, not index) | `lengthSamples`, `waveIndex` | — |
| `playIndexed::warning` (local) | no | medium | warning message string | — | — |
| `playIndexed::deviceIndex` (local) | no | medium | matches config_->deviceIndex | — | — |
| `playIndexed::regZero` (local) | no | medium | constant-0 register | — | — |
| `playIndexed::channelArgs` / `triggerMask` | no | medium | clear | — | — |
| `playIndexed::firstWA` / `baseLen` | no | medium | length probe inputs | — | — |
| `playIndexed::combined` (local) | no | medium | merged waveform | — | — |
| `playIndexed::channelCount` / `useYSuffix` | no | medium | passed-through to mergeWaveforms | — | — |
| `playIndexed::indexReg` (local) | no | medium | register holding wave index | — | — |
| `playIndexed::offsetVarType` (local) | no | medium | parseEnd[0].varType_ | — | — |
| `playIndexed::regNum` / `regZeroForAddi` | no | medium | clear allocation | — | — |
| `playIndexed::rateImm` (local) | yes | low | Immediate built from `rate`, but rate is samples-offset here | `keep current`, `offsetImm` | — |
| `playIndexed::addiEntries` / `placeholderEntry` | no | medium | clear | — | — |
| `playIndexed::expectedLen` (local) | no | medium | devConst_->waveformGranularity | — | — |
| `playIndexed::regInv` (local) | yes | low | -1 sentinel register | `regInvalid`, `keep current` | — |
| `playIndexed::waveforms` / `playEntry` | no | medium | clear | — | — |
| `writeToNode::path` / `::val` / `::type` / `::res` | no | medium | match disasm SysV mapping | — | — |
| `writeToNode::results` (local) | no | medium | shared output | — | — |
| `writeToNode::pathStr` (local) | no | high | regex_match input | — | — |
| `writeToNode::matches` (local) | no | high | boost::cmatch buffer | — | — |
| `writeToNode::devIdStr` / `requestedDev` | no | medium | parsed device-id | — | — |
| `writeToNode::node` (local) | no | medium | NodeMapItem from lookup | — | — |
| `writeToNode::chIdxStr` / `channelIdx` | no | medium | parsed channel index | — | — |
| `writeToNode::numGroups` (local) | no | medium | config_->numChannelGroups | — | — |
| `writeToNode::oscIdxStr` / `oscIdx` | no | medium | parsed osc index | — | — |
| `writeToNode::divisor` (local) | no | low | brief, scoped | — | — |
| `writeToNode::tagMF` (local) | no | high | inserted as "MF" feature key | — | — |
| `writeToNode::accessMode` (local) | unsure | low | computed from `node.hasFast` | `keep current`, `mode` | cross-batch-arbitration |
| `writeToNode::regNum` / `destReg` / `localList` | no | medium | clear | — | — |
| `writeToNode::valRef` (local) | no | low | const-ref alias | — | — |
| `writeToNode::useFastJt` (local) | no | medium | which jump table to use | — | — |
| `writeToNode::addr` (local) | no | high | passed to addi as Immediate | — | — |
| `writeToNode::emitWarnAndReturn` (local) | no | medium | gates warning + early return | — | — |
| `writeToNode::direct` (local) | no | medium | dynamic_cast result | — | — |
| `writeToNode::mapIt` (local) | no | medium | unordered_map iterator | — | — |
| `writeToNode::d` / `d2` / `d3` / `dHi` / `dF2` (locals) | yes | low | shadow / meaningless | `valDouble`, `keep current` | coordinated-rename |
| `writeToNode::intVal` / `low32` / `high32` / `fbits` / `phaseInt` / `freqInt` / `freqVal` / `phaseVal` / `freqHigh32` / `freq2` / `clk` / `clk2` / `clkF` / `valStr` / `msg` / `f` / `phaseF` / `phaseD` / `freqD` / `rawBits` / `rawHi` / `truncated` (locals) | no | low | self-explanatory in narrow scope | — | — |
| `writeToNode::vec` / `v` (per-block locals) | yes | low | nondescript; many uses | `addiVec`, `keep current` | coordinated-rename |
| `addSyncCommand::results` / `::res` | no | medium | output sink + Resources | — | — |
| `addSyncCommand::deviceType` (local) | no | medium | config_->deviceType | — | — |
| `addSyncCommand::asm_entry` (local) | no | low | clear in context | — | — |
| `printF::args` | no | medium | format args | — | — |
| `printF::funcName` (parameter) | yes | high | unused in body, marked `/*funcName*/` | `keep current`, `funcName` | not-misnomer |
| `printF::fmtArg` / `fmtStr` / `formatter` (locals) | no | high | direct boost::format use | — | — |
| `printF::arg` / `varType` / `s` / `val` / `rounded` (locals) | no | medium | clear | — | — |
| `addWaitCycles::cycles` / `::results` / `::res` | no | high | passes immediate to addi | — | — |
| `addWaitCycles::regNum` / `reg` / `zero` / `addiResult` | no | medium | clear | — | — |
| `writeLS64bit::value` / `::reg1` / `::reg2` / `::results` / `::res` | yes | medium | reg1/reg2 are user-store **addresses**, not registers | `addrLow`/`addrHigh`, `keep current` | — |
| `writeLS64bit::regNum` / `reg` / `zero` / `lowBits` / `highBits` / `addiLow` / `addiHigh` (locals) | no | medium | clear | — | — |
| `generateWaveform::name` / `::args` / `::res` | no | medium | leading string + args | — | — |
| `generateWaveform::newArgs` (local) | no | medium | extended args vector | — | — |
| `generateWaveform::nameVal` (local) | no | medium | constructed first-arg EvalResultValue | — | — |
| `generateWaveform::result` (local) | no | medium | sret slot | — | — |
| `generateWaveform::e` (catch params) | no | medium | standard catch | — | — |

## 3. Detailed findings

### appendSuser params (anonymous-namespace helper)  [no / medium / not-misnomer]

Evidence:
- src/runtime/custom_functions_play.cpp:41-48  two overloads:
  `appendSuser(AsmList& list, ...)` and
  `appendSuser(std::vector<AsmList::Asm>& vec, ...)`.
- Both bodies do exactly `list.append(cmds->suser(reg, addr))` /
  `vec.push_back(cmds->suser(reg, addr))`.
- Call sites in this file (>50): the first arg is consistently either
  `results->assemblers_` (vector), `localList` (AsmList), or a
  destination accumulator.

Interpretation:
- Each parameter is used exactly as its name suggests; the two
  overloads differ only by container type, hence the names `list`
  vs `vec`.

Judgement: no.

Locations consulted:
- defined: src/runtime/custom_functions_play.cpp:40-49
- used:    same file (50+ call sites)

### setWaitCyclesReg::res  [unsure / low / —]

Evidence:
- src/runtime/custom_functions_play.cpp:65 declared as
  `std::shared_ptr<Resources> res`.
- The reconstructed body never references `res`.
- The binary block @0x15cd58 just moves the EvalResults shared_ptr to
  the sret slot; no resource handle is read.

Interpretation:
- The `res` parameter is unused at the source level here. Either the
  binary truly ignored its third arg (no observable use in the
  reconstruction), or a resource read was elided during reconstruction.

Judgement: unsure — name is conventional and reused across many sister
functions (addWaitCycles, writeLS64bit), so keeping it is harmless.

Proposals:
- keep current (medium)
- mark `[[maybe_unused]]` to flag for later check (low)

### setWaitCyclesReg::shifted  [yes / low / —]

Evidence:
- src/runtime/custom_functions_play.cpp:75-80
  `uint32_t shifted = devType - 2;` then `kCheckPlaySupportedMask >> shifted`.
- Comment @line 77: "after subtracting 2".

Interpretation:
- The local is the bit-index used to test membership in the supported
  device-type bitmask; "shifted" describes its mechanical role but not
  its semantic identity.

Judgement: weakly misleading — "shifted" tells the reader nothing
about the value's meaning.

Proposals:
- `devTypeBiased` (low)
- `devTypeBitIndex` (low)
- keep current (medium)

### mergeWaveforms::useYSuffix  [unsure / low / cross-batch-arbitration]

Evidence:
- src/runtime/custom_functions_play.cpp:160 declared `bool useYSuffix`.
- Used at line 282 to pick between `"playWave"` and `"playWaveI"`
  funDescr — but the suffix is **"I"**, not "Y".
- Used again at lines 398-407 to dispatch
  `interleave` (true) vs `merge` (false) factory.

Interpretation:
- Both call sites suggest the bool is a single "is interleave / Aux"
  flag. The "Y" in the name doesn't match either string literal
  ("playWaveI" — capital I). The function descriptor literal in the
  binary at @0x15e32c..0x15e377 is recorded in code comments as
  "playWaveI" (line 280-282); there is no "Y" suffix in evidence.
- The play() call site (line 580-587) passes `false` literally; the
  playIndexed() call site (line 1001) passes `(subFunc == SubFunc::Aux)`.

Judgement: unsure / likely misleading. The symbol is the
"interleave / aux" channel-pairing selector, not a "Y suffix" toggle.

Proposals:
- `interleavePair` (low)
- `auxPair` (low)
- keep current (low)

Cross-reference:
- Counterpart play()::isSecondaryChannel local (line 580) is bound to
  the same parameter slot. Header param is `param3` (out of scope here).

### mergeWaveforms::useFunDescrPath  [unsure / low / —]

Evidence:
- src/runtime/custom_functions_play.cpp:162 declared `bool useFunDescrPath`.
- Used at line 379 to enter Sub-path A (explicit `getWaveformByFunDescr`
  + `newWaveform`) vs Sub-path B (`getOrCreateWaveform` factory).
- Inline comment @line 358-362 admits the precise predicate is still
  under investigation: "[rbp-0x48] origin is tracked as 21a-followup".

Interpretation:
- The name describes the observed effect (which path is taken) but the
  underlying meaning is unconfirmed. play() passes `isSecondaryChannel`
  (line 587); playIndexed() passes `false` (line 1004).

Judgement: unsure — keep until the binary-level predicate is
identified.

Proposals:
- keep current (medium)
- `useExplicitFunDescr` (low)

### mergeWaveforms::funDescr2  [yes / low / —]

Evidence:
- src/runtime/custom_functions_play.cpp:382 `std::string funDescr2 = "playWave";`
  inside the Sub-path-A branch.
- Distinct from outer `funDescr` (line 273) but holds the same kind of
  value.

Interpretation:
- The "2" suffix is a placeholder — the locals serve the same role in
  different scopes.

Judgement: mildly misleading.

Proposals:
- `funDescrA` (low)
- keep current (low)

### play::playLength  [yes / medium / —]

Evidence:
- src/runtime/custom_functions_play.cpp:487 `int playLength = 0;`
- Set only on the DigTrigger path: `playLength = firstVal.value_.toInt();`
  (line 495), validated `< 3` (line 496).
- Passed to `asmPlay(..., playLength, ...)` at line 663 as the
  `regVal` slot per Phase 16's mapping documented in playIndexed
  (lines 1144-1145: "stack[5] = waveIndex → regVal").

Interpretation:
- For DigTrigger this argument is a digital-trigger code, not a
  duration in samples. The `< 3` lower-bound check supports the
  trigger-code reading (sample-length values would not be capped at 3).
- The same asmPlay slot is named `waveIndex`/`regVal` in playIndexed.

Judgement: yes — name implies duration; value is a per-call code.

Proposals:
- `firstArgVal` (low)
- `digTriggerCode` (medium)

### play::numChannels  [yes / low / —]

Evidence:
- src/runtime/custom_functions_play.cpp:520 `int numChannels = config_->numChannelGroups;`

Interpretation:
- The local renames the config field on read. The field, per
  awg_compiler_config.hpp header, is `numChannelGroups` (values 1, 2, 4).

Judgement: minor misnomer; "channels" and "channelGroups" are
distinct in the config (`channelsPerGroup` is a separate field).

Proposals:
- `numChannelGroups` (medium)
- keep current (low)

### play::channelIndex  [yes / low / —]

Evidence:
- src/runtime/custom_functions_play.cpp:521 `int channelIndex = config_->deviceIndex;`
- Used as a "reference channel" comparator (line 539) and as the
  primary-channel selector (line 592).

Interpretation:
- `deviceIndex` is the canonical name in `awg_compiler_config.hpp` and
  in playIndexed (line 850). Within play() it is rebadged as
  `channelIndex`. The two names refer to the same field.

Judgement: yes — collision with the canonical field name causes
confusion when read alongside playIndexed().

Proposals:
- `deviceIndex` (medium)
- keep current (low)

### play::mask  [yes / low / —]

Evidence:
- src/runtime/custom_functions_play.cpp:526 `int mask = 0x3FFF;`
- Used at line 549 `mask &= ~(1 << ((b - 1) + shift));` then passed to
  `asmPlay(..., static_cast<unsigned int>(mask), ...)` at line 661 as
  the `suppress` (trigger-mask) parameter.
- playIndexed has the analogous local explicitly named `triggerMask`
  (line 863).

Interpretation:
- Both `mask` (play) and `triggerMask` (playIndexed) are the same
  per-channel trigger-suppression bitmask. Naming should match.

Judgement: yes — minor.

Proposals:
- `triggerMask` (medium)
- keep current (low)

### play::isSecondaryChannel  [unsure / low / cross-batch-arbitration]

Evidence:
- src/runtime/custom_functions_play.cpp:580
  `bool isSecondaryChannel = (ch != channelIndex);`
- Passed at line 587 as the `useFunDescrPath` parameter of
  `mergeWaveforms`.

Interpretation:
- The local's name describes the predicate; the parameter slot it
  fills (`useFunDescrPath`) names the role at the receiver. Both names
  are tentative — see mergeWaveforms::useFunDescrPath above.

Judgement: unsure — both sides under-specified.

Proposals:
- keep current (medium)
- `useFunDescrPath` (low)

Cross-reference:
- Counterpart `mergeWaveforms::useFunDescrPath` is in this same batch
  (block above). When that role is decided, this local's name follows.

### play::reg0 / play::regInv  [yes / low / —]

Evidence:
- src/runtime/custom_functions_play.cpp:654 `AsmRegister reg0(0);`
- src/runtime/custom_functions_play.cpp:655 `AsmRegister regInv(-1);`
- playIndexed uses `regZero` (line 851) and `regInv` (line 1154).

Interpretation:
- "reg0" and "regZero" both name the constant-zero register
  placeholder. The "Inv" suffix is shorthand for "invalid" / sentinel
  (-1).

Judgement: minor inconsistency between `play::reg0` and
`playIndexed::regZero`; both should probably read `regZero`.

Proposals:
- `regZero` / `regInvalid` (low)
- keep current (low)

### playIndexed::indexed (PlayArgs ctor flag)  [unsure / low / cross-batch-arbitration]

Evidence:
- src/runtime/custom_functions_play.cpp:745 `bool indexed = (subFunc == SubFunc::Aux);`
- Passed as the 5th ctor arg of `PlayArgs` (line 747).
- Comment at lines 740-744 lists known callers with values:
  `play()=false, playAuxWave=true, playDIOWave=false, assignWaveIndex=false,
   playIndexed=(subFunc==Aux)`.

Interpretation:
- The local truly tracks "is this the Aux sub-function?" at this call
  site, but the receiver param of PlayArgs is named differently. The
  predicate value here equals `(subFunc == Aux)`, not "is this an
  Indexed call" (since playIndexed always passes false for non-Aux
  sub-funcs).

Judgement: unsure — the local is consistent with the existing
non-trivial caller-table comment but the name "indexed" is dubious in
this function name (`playIndexed`!).

Proposals:
- `isAux` (medium)
- keep current (low)

Cross-reference:
- Counterpart `PlayArgs` ctor parameter lives in the PlayArgs batch.

### playIndexed::rateBegin  [yes / medium / —]

Evidence:
- src/runtime/custom_functions_play.cpp:778 `auto rateBegin = parseEnd + 2;`
- Comment immediately above (lines 776-777): "parseOptionalRate
  receives parseEnd+2 (past the index/length args)."
- Passed as the iterator argument of `parseOptionalRate(...)` at
  line 779.

Interpretation:
- The iterator points past two consumed positional args
  (offset + length). It marks where rate-parsing begins, but the name
  obscures the "+2 skip" structure that the comment makes explicit.

Judgement: yes — slight; "rate" refers to the consumer, not to the
position of the args being skipped.

Proposals:
- `rateArgsBegin` (medium)
- `parseEndAfterIndexLen` (medium)
- keep current (low)

### playIndexed::waveIndex  [yes / medium / —]

Evidence:
- src/runtime/custom_functions_play.cpp:812
  `int waveIndex = parseEnd[1].value_.toInt();   // @0x161228 — length arg`
- Adjacent comment `length arg` directly contradicts the local name
  `waveIndex`.
- Used at line 820 to early-exit on zero (warning 0x9c documented as
  "length is zero" — `LengthIsZero` constant @line 822).
- Compared at line 1021 `waveIndex > combined->signal.length()` —
  the comparison is "samples requested vs samples available".

Interpretation:
- `parseEnd[1]` is the length (in samples), not the wave index. The
  name is a hold-over from earlier reconstruction iterations. The
  comment, the error-message constant, and the comparison all agree
  it is a length.

Judgement: yes.

Proposals:
- `lengthSamples` (medium)
- `waveLength` (medium)
- keep current (low)

### writeToNode::accessMode  [unsure / low / cross-batch-arbitration]

Evidence:
- src/runtime/custom_functions_play.cpp:1511
  `AccessMode accessMode = static_cast<AccessMode>(node.hasFast);`
- Comment @lines 1454-1457 / 1509-1510: "either (a) the field is
  overloaded and AccessMode is encoded as a 0/1 byte that doubles as
  a hasFast indicator, or (b) hasFast IS the AccessMode and our typing
  is wrong".

Interpretation:
- The local cast is admitted by the in-source comment to be uncertain.
  Whether `node.hasFast` is the access mode or a separate bool is
  unresolved.

Judgement: unsure — pending NodeMapItem/AccessMode arbitration.

Proposals:
- keep current (medium)
- `mode` (low)

Cross-reference:
- Counterpart `NodeMapItem::hasFast` and the `AccessMode` enum in
  batches 27 (node_map_data) / 20 (node) — defer.

### writeLS64bit::reg1 / writeLS64bit::reg2  [yes / medium / —]

Evidence:
- src/runtime/custom_functions_play.cpp:2347 declared
  `void CustomFunctions::writeLS64bit(unsigned long value, int reg1, int reg2, ...)`.
- Both are passed verbatim as the address operand to `appendSuser(...,
  detail::AddressImpl<unsigned int>(static_cast<unsigned int>(reg1)))`
  (lines 2366-2367, 2380-2381).
- The neighbouring helpers `addWaitCycles` and `setWaitCyclesReg` use
  these same suser-address slots as `kSuserWaitCycles` (0x69),
  `kSuserWaitLegacy` (0x6F), etc. — i.e. user-store address codes,
  not register numbers.

Interpretation:
- The two `int` parameters are user-store **address codes** that go
  into the suser instruction's address field, not AWG register
  identifiers. The names `reg1`/`reg2` strongly suggest the latter.

Judgement: yes — names imply registers, the values are addresses.

Proposals:
- `addrLow`, `addrHigh` (medium)
- `addrLow32`, `addrHigh32` (medium)
- keep current (low)

### printF::funcName  [no / high / not-misnomer]

Evidence:
- src/runtime/custom_functions_play.cpp:2247
  `std::string CustomFunctions::printF(... const& args, std::string const& /*funcName*/)`.
- Header (line 410-411) declares the second parameter as `fmt`.
- Reconstruction reads `fmtStr` from `args[0]` (line 2258), not from the
  second parameter; the parameter is unused.

Interpretation:
- The .cpp parameter name `funcName` (commented-out) reflects the
  caller's role — passing the SeqC function being executed for error
  messages. The header's `fmt` is misleading; the parameter is NOT the
  format string. The .cpp side is closer to the truth.

Judgement: no — the parameter name `funcName` correctly identifies
the role even though the body never reads it (errors are formatted
without it in the current reconstruction).

Proposals:
- keep current (medium)

Cross-reference:
- Header parameter `fmt` (out of scope here) is the misnomer; this
  side is positive evidence that `funcName` is the right name.

### writeToNode `d` / `vec` family  [yes / low / coordinated-rename]

Evidence:
- The 6 typeIdx switch arms in writeToNode reuse the local names
  `d`, `d2`, `d3`, `dHi`, `dF2`, `clk`, `clk2`, `f`, `vec`, `v`,
  `valStr`, `msg` etc. across branches that compute either a frequency
  (typeIdx=4), a phase (typeIdx=5), a raw double (typeIdx=3), a float
  (typeIdx=2), or just an int (typeIdx=0,1).
- See lines 1610-1611, 1638-1640, 1689-1690, 1712-1714, 1735-1738,
  1758-1760, 1825-1828, 1838-1840, 1853-1860, 1874-1877, 2031-2035,
  2054-2065.

Interpretation:
- `d` is a value of unspecified semantic role at each occurrence. In
  the case-2 fast/slow paths it is "the float to send"; in case-4 it
  is "the frequency in Hz"; in case-5 it is "the phase in radians".
  Reading any one block in isolation, the name says nothing. `vec`
  for the addi-result vector is similarly nondescript.

Judgement: low-confidence misnomers; coordinated rename across all
arms would add real value.

Proposals:
- per-block: `freqHz`, `phaseRad`, `doubleVal`, `clkHz`, etc.
- `addiVec` for the addi-result vectors (low)
- keep current (medium) — the locals are tightly scoped per case

## 4. Symbols inspected and judged routinely fine

- `setWaitCyclesReg::args`, `::results` — standard names matching usage.
- `setWaitCyclesReg::devType`, `::supported`, `::waitReg`, `::immVal`,
  `::regNum`, `::newReg` — names match usage.
- `mergeWaveforms::args`, `::callerName`, `::requestedLength`,
  `::values`, `::maxSampleLen`, `::wfName`, `::funDescr`,
  `::multiValue`, `::optName`, `::result`, `::sig`, `::factory`,
  `::actualChannels`, `::wf` — clear in context.
- `play::args`, `::res`, `::subFunc`, `::cmdName`, `::argsCopy`,
  `::firstVal`, `::playArgs`, `::parseEnd`, `::rate`, `::results`,
  `::maxSampleLen`, `::channelWaveforms`, `::ch`, `::assignments`,
  `::channelArgs`, `::wa`, `::waIndex`, `::shift`,
  `::maxSampleLength`, `::combinedWf`, `::asmEntry`, `::wfCopy`,
  `::playDeviceIndex` — clear in context.
- `playIndexed::args`, `::res`, `::subFunc`, `::cmdName`,
  `::parseEnd`, `::vt0`, `::vt1`, `::rate`, `::warning`,
  `::deviceIndex`, `::regZero`, `::channelArgs`, `::triggerMask`,
  `::firstWA`, `::baseLen`, `::combined`, `::channelCount`,
  `::useYSuffix`, `::indexReg`, `::offsetVarType`, `::regNum`,
  `::regZeroForAddi`, `::addiEntries`, `::placeholderEntry`,
  `::expectedLen`, `::waveforms`, `::playEntry` — clear.
- `writeToNode::path`, `::val`, `::type`, `::res`, `::results`,
  `::pathStr`, `::matches`, `::devIdStr`, `::requestedDev`, `::node`,
  `::chIdxStr`, `::channelIdx`, `::numGroups`, `::oscIdxStr`,
  `::oscIdx`, `::divisor`, `::tagMF`, `::regNum`, `::destReg`,
  `::localList`, `::valRef`, `::useFastJt`, `::addr`,
  `::emitWarnAndReturn`, `::direct`, `::mapIt` — clear.
- `addSyncCommand::results`, `::res`, `::deviceType`, `::asm_entry`
  — clear (nb. `asm_entry` uses snake_case; cosmetic only).
- `printF::args`, `::fmtArg`, `::fmtStr`, `::formatter`, `::arg`,
  `::varType`, `::s`, `::val`, `::rounded` — clear.
- `addWaitCycles::cycles`, `::results`, `::res`, `::regNum`, `::reg`,
  `::zero`, `::addiResult` — clear.
- `writeLS64bit::value`, `::results`, `::res`, `::regNum`, `::reg`,
  `::zero`, `::lowBits`, `::highBits`, `::addiLow`, `::addiHigh` —
  clear; only `reg1`/`reg2` flagged in §3.
- `generateWaveform::name`, `::args`, `::res`, `::newArgs`,
  `::nameVal`, `::result`, catch param `e` — clear.

## 5. Coverage

**Fully covered (parameters + meaningful locals):**

- Anonymous-namespace helper `appendSuser` (both overloads) —
  custom_functions_play.cpp:40-49.
- `CustomFunctions::setWaitCyclesReg` — lines 63-128.
- `CustomFunctions::mergeWaveforms` — lines 158-452.
- `CustomFunctions::play` — lines 454-681.
- `CustomFunctions::playIndexed` — lines 683-1216.
- `CustomFunctions::writeToNode` — lines 1269-2196 (incl. file-static
  regex-name confirmation against nm).
- `CustomFunctions::addSyncCommand` — lines 2205-2231.
- `CustomFunctions::printF` — lines 2246-2308.
- `CustomFunctions::addWaitCycles` — lines 2316-2337.
- `CustomFunctions::writeLS64bit` — lines 2347-2383.
- `CustomFunctions::generateWaveform` — lines 2393-2461.

**Deferred:** none.

**Not covered (out of scope per RULES §2/§3):**

- All ten `CustomFunctions::*` method names — present in the symbol
  table (tier-1 authoritative).
- The four function-static `boost::regex` objects in `writeToNode`
  (`absDevRegex`, `awgNodeRegex`, `sineNodeRegex`,
  `oscselNodeRegex`) — present as static data symbols at
  `0xb84748..0xb84790` with matching guard-byte symbols (tier-1
  authoritative per RULES §3).
- `floatEqual` (declared at line 52, defined elsewhere) — out of scope
  per assignment; addressed in batch 03 (waveform_generator).
- `kSuser*` constants used throughout — defined in `types.hpp`,
  audited in batch 01.
- Header-side parameter names of `mergeWaveforms` (`param2`, `param3`,
  `param5`, `param6`) — header is out of scope for this sub-batch;
  the .cpp definition supplies usable names already (`channelCount`,
  `useYSuffix`, `requestedLength`, `useFunDescrPath`).
- All other `CustomFunctions` per-method `.cpp` files (other 05x
  sub-batches).
