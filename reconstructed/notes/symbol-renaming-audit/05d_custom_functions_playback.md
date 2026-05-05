# Batch 05d — custom_functions_playback

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 5 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 5;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/src/runtime/custom_functions_playback.cpp` (1036 lines, in scope)

Out of scope (declarations only consulted, not audited):
- `reconstructed/include/zhinst/runtime/custom_functions.hpp` — header lives in
  another batch; method declarations are read-only context here.

## 2. Overview

All `CustomFunctions::*` method names in this file
(`playWave`, `playWaveNow`, `playWaveIndexed`, `playWaveIndexedNow`,
`playAuxWave`, `playAuxWaveIndexed`, `playWaveDigTrigger`, `playDIOWave`,
`playWaveDIO`, `playWaveZSync`, `playZero`, `playHold`, `waitWave`,
`waitPlayQueueEmpty`, `sync`, `randomSeed`, `now`, `error`, `info`,
`setRate`) appear verbatim in `nm --demangle _seqc_compiler.so`
(see lines under `zhinst::CustomFunctions::…` in §5). Per RULES §3
they are excluded from rename and not listed below.

The free function in the anonymous namespace, `appendSuser`, is a
recon-introduced helper (no symbol-table entry). Its name is in scope.

In-scope symbols are: parameters of every method body (`args`, `res`),
parameters of `appendSuser`, and local variables. The two member
parameters `args` and `res` are uniform across all 20 methods and
described as a single group.

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `CustomFunctions::*::args` (all 20 methods) | no | medium | matches usage, project idiom | keep current | not-misnomer |
| `CustomFunctions::*::res` (all 20 methods) | no | medium | resources passed through | keep current | not-misnomer |
| `appendSuser::vec` | unsure | low | generic name, single use | `out`, `assemblers`, keep current | — |
| `appendSuser::cmds` | no | low | refers to AsmCommands ptr | keep current | — |
| `appendSuser::reg` | no | low | AsmRegister value | keep current | — |
| `appendSuser::addr` | no | low | AddressImpl<unsigned int> | keep current | — |
| `playAuxWave::playArgs` | no | medium | local of type `PlayArgs` | keep current | — |
| `playAuxWave::parseEnd` | no | low | iterator, post-parse position | keep current | — |
| `playAuxWave::rate` | no | medium | sample-rate divisor | keep current | — |
| `playAuxWave::results` | no | medium | EvalResults return value | keep current | — |
| `playAuxWave::channelIndex` | no | medium | `config_->deviceIndex` | keep current | — |
| `playAuxWave::assignments` | no | medium | per-channel WaveAssignments | keep current | — |
| `playAuxWave::combinedWf` | no | medium | merged waveform output | keep current | — |
| `playAuxWave::mask` | unsure | low | dual-purpose: trigger & suppress | `suppressMask`, keep current | — |
| `playAuxWave::channelsPerGroup` | no | high | matches `config_->channelsPerGroup[1]` | keep current | not-misnomer |
| `playAuxWave::channelArgs` | no | medium | per-channel scattered values | keep current | — |
| `playAuxWave::filledCount` | no | medium | counts assigned slots | keep current | — |
| `playAuxWave::firstName` | no | low | first assignment's wave name | keep current | — |
| `playAuxWave::baseLen` | no | low | reference length for zeros() | keep current | — |
| `playAuxWave::zeroArgs` | no | low | args to `zeros` builtin | keep current | — |
| `playAuxWave::zeroWf` | no | medium | result of `zeros` call | keep current | — |
| `playAuxWave::zeroName` | no | medium | name of zero placeholder | keep current | — |
| `playAuxWave::channelsPerGroupS` | no | low | short cast of channelsPerGroup | keep current | — |
| `playAuxWave::expectedLen` | unsure | low | granularity not length | `granularity`, keep current | — |
| `playAuxWave::emitPlay` | no | high | gate around asmPlay emit | keep current | not-misnomer |
| `playAuxWave::waveforms` | no | medium | single-element WF vector | keep current | — |
| `playAuxWave::reg` | no | low | AsmRegister(0) | keep current | — |
| `playAuxWave::regInv` | no | medium | AsmRegister(-1) sentinel | keep current | — |
| `playAuxWave::asmEntry` | no | medium | AsmList::Asm result | keep current | — |
| `playAuxWave::tail` | no | medium | linked-list walker | keep current | — |
| `playAuxWave::argc` | no | medium | args.size() snapshot | keep current | — |
| `playDIOWave::playArgs` | no | medium | local of type `PlayArgs` | keep current | — |
| `playDIOWave::parseEnd` | no | low | iterator post-parse | keep current | — |
| `playDIOWave::rate` | no | medium | sample-rate divisor | keep current | — |
| `playDIOWave::results` | no | medium | EvalResults return | keep current | — |
| `playDIOWave::maxSampleLen` | yes | medium | declared, never used | drop, or `(void)` | — |
| `playDIOWave::channelIndex` | no | medium | `config_->deviceIndex` | keep current | — |
| `playDIOWave::assignments` | no | medium | WaveAssignments slot | keep current | — |
| `playDIOWave::channelArgs` | no | medium | non-marker values | keep current | — |
| `playDIOWave::mask` | unsure | low | DIO trigger bit field | `triggerMask`, keep current | — |
| `playDIOWave::dryRun` | yes | medium | bound to asmPlay isHold | `isHold`, `holdFlag`, keep current | — |
| `playDIOWave::shiftBits` | no | low | bit-shift amount | keep current | — |
| `playDIOWave::combinedWf` | no | medium | merged waveform | keep current | — |
| `playDIOWave::channelsPerGroup0` | no | low | `channelsPerGroup[0]` | keep current | — |
| `playDIOWave::expectedLen` | unsure | low | granularity not length | `granularity`, keep current | — |
| `playDIOWave::emitPlay` | no | high | guards asmPlay emission | keep current | not-misnomer |
| `playDIOWave::waveforms` | no | medium | single-elem WF vec | keep current | — |
| `playDIOWave::reg0`, `regInv` | no | medium | AsmRegister(0/-1) | keep current | — |
| `playDIOWave::asmEntry` | no | medium | AsmList::Asm result | keep current | — |
| `playDIOWave::argc` | no | medium | args.size() snapshot | keep current | — |
| `playWaveDIO::results` | no | medium | EvalResults return | keep current | — |
| `playWaveDIO::mask` | no | medium | `1 << numOutputPorts` | keep current | — |
| `playWaveDIO::asmEntry` | no | medium | wvft entry | keep current | — |
| `playWaveZSync::arg0` | no | medium | sole arg reference | keep current | — |
| `playWaveZSync::tableIndex` | unsure | medium | indexes constants, not table | `constId`, `selector`, keep current | — |
| `playWaveZSync::matched` | no | high | tracks readConst match | keep current | not-misnomer |
| `playWaveZSync::shift` | yes | medium | data pattern, not shift count | `dataPattern`, `zsyncCode`, keep current | — |
| `playWaveZSync::zsyncRaw`/`zsyncProcA`/`zsyncProcB` | no | high | mirrors readConst names | keep current | not-misnomer |
| `playWaveZSync::results` | no | medium | EvalResults return | keep current | — |
| `playWaveZSync::mask` | no | medium | wvft bitmask | keep current | — |
| `playWaveZSync::asmEntry` | no | medium | wvft entry | keep current | — |
| `waitWave/waitPlayQueueEmpty/sync::result` | no | medium | EvalResults return | keep current | — |
| `waitWave/waitPlayQueueEmpty::asm_entry` | unsure | low | snake_case in camel file | `asmEntry` | coordinated-rename |
| `now::result`, `now::reg` | no | low | local short names | keep current | — |
| `error/info::result`, `msg`, `asm_entry` | unsure | low | snake_case mix | `asmEntry` | coordinated-rename |
| `randomSeed::rng`, `rd` | no | low | random objects | keep current | — |
| `setRate::arg0`, `results`, `rate`, `asmEntry` | no | medium | match usage | keep current | — |
| `playZero::arg0` | no | medium | sole positional arg | keep current | — |
| `playZero::length` | no | high | sample length, validated by checkPlayMinLength | keep current | not-misnomer |
| `playZero::regNum` | no | medium | AsmRegister number | keep current | — |
| `playZero::rate` | no | medium | optional rate, default −1 | keep current | — |
| `playZero::emptyWfs`, `channelIndex`, `reg0`, `regArg`, `asmEntry`, `playNode`, `results` | no | medium | match usage | keep current | — |
| `playHold::arg0`, `length`, `regNum`, `rate`, `emptyWfs`, `channelIndex`, `reg0`, `regArg`, `asmEntry`, `playNode`, `results` | no | medium | mirror playZero | keep current | — |

## 3. Detailed findings

### CustomFunctions::*::args  [no / medium / not-misnomer]

Evidence:
- 20 methods take `std::vector<EvalResultValue> const& args`. nm:
  `…CustomFunctions::playWave(std::__1::vector<zhinst::EvalResultValue,…> const&,…)`.
- Body uses include `args.empty()` (l.130, 433, 670, 824, 836, 848, 859,
  877, 888, 935, 988), `args.size()` (l.137, 436, 744, 749, 911, 938,
  942, 962, 991), `args[0]` (l.753, 949, 1002, 915), and pass-through
  `play(args, …)` / `playIndexed(args, …)` (l.49, 55, 61, 67, 75, 81).

Interpretation:
- Every method receives the SeqC builtin's positional argument list.
  `args` accurately denotes that list.

Judgement:
- Not a misnomer; standard project idiom for builtin dispatchers.

### CustomFunctions::*::res  [no / medium / not-misnomer]

Evidence:
- Type `std::shared_ptr<Resources>`. Used as `res->readConst(...)` in
  playWaveZSync (l.773, 779, 785), `setWaitCyclesReg(args, results, res)`
  (l.806), `addSyncCommand(result, std::move(res))` (l.852), and
  `std::move(res)` forwarded into `play()`/`playIndexed()` (l.49, 55,
  …). Many bodies mark it `/*res*/` showing it is unused locally.

Interpretation:
- Short for `Resources` shared pointer. Usage is consistent with the
  project-wide convention seen in custom_functions.hpp:439-490.

Judgement:
- Not a misnomer.

### playDIOWave::maxSampleLen  [yes / medium / —]

Evidence:
- l.479: `int64_t maxSampleLen = playArgs.getMaxSampleLength();`
- l.480: `(void)maxSampleLen; // forwarded into asmPlay's trigger argument below.`
- The only subsequent use is the `(void)` cast. Neither `maxSampleLen`
  nor any value derived from it appears in the asmPlay arg list at
  l.607-619 — those use `mask` and `rate`, not `maxSampleLen`.
- The trailing comment claims it is forwarded to `asmPlay`'s trigger,
  but `trigger=0` is hard-coded at l.619.

Interpretation:
- The variable is computed and immediately suppressed. The accompanying
  comment is incorrect. The name itself describes what
  `getMaxSampleLength()` returns, but the value is dead.

Judgement:
- Misleading: the comment promises it is "forwarded into asmPlay's
  trigger argument" but no code path consumes it. The name itself is
  fine for the value; the surrounding code is the bug.

Proposals:
- keep current name (high) — the name is accurate to the called method.
- delete the variable and the call (medium) — if reconstruction wants
  to elide a side-effect-free getter.
- Action item: investigate whether the binary actually consumes
  `getMaxSampleLength()` somewhere we missed (e.g. via a side effect
  inside `getMaxSampleLength`); if not, the call is dead. This is a
  reconstruction-correctness question, not a naming one.

### playDIOWave::dryRun  [yes / medium / —]

Evidence:
- l.493-494: `bool dryRun = true; // tracks whether merged-string is empty`.
- l.547: `dryRun = channelArgs[0].value_.toString().empty();`
- l.549: `dryRun = false;`
- l.585 (comment) and l.610: `/*isHold=*/dryRun` is bound directly to
  AsmCommands::asmPlay's `isHold` parameter. nm confirms asmPlay's
  3rd parameter is a `bool` (= isHold).
- l.597 (comment): `isHold = dryRun (r8b from [rbp-0x60])`.

Interpretation:
- The flag is computed from the first channel arg's string-emptiness,
  then forwarded as the AsmPlay `isHold` boolean. The current name
  `dryRun` does not describe the asmPlay `isHold` semantics, and the
  initial-value comment ("tracks whether merged-string is empty") does
  not match an isHold meaning either. This is exactly the §4c pattern:
  same flag is named `dryRun` here and `isHold` at the call site.

Judgement:
- Misnomer: the local name is divorced from its sole consumer's
  semantics. Whether the underlying meaning is "dry run" or "hold"
  cannot be decided from this batch alone; the asmPlay parameter is
  the authoritative side (it lives in batch 10 / AsmCommands).

Proposals:
- `isHold` (medium) — match the call-site parameter.
- `holdFlag` (low)
- keep current (low) — only justified if asmPlay's `isHold` parameter
  itself is the misnomer.

Cross-reference:
- Counterpart `AsmCommands::asmPlay::isHold` (parameter name) — batch
  10 (asm_commands).

### playWaveZSync::shift  [yes / medium / —]

Evidence:
- l.770: `int shift = 0;`
- l.777, 783, 789: assigned `1`, `9`, `0xd`.
- l.717-719 (comment block above the function):
  > mask = shift << numOutputPorts (NOT 1 << (numOutputPorts + shift) —
  > the binary literally shifts the shift-value: `shl eax, cl` where
  > eax ∈ {1, 9, 0xd}). For shift=1 these are equivalent, but for
  > shift=9 (1001b) and shift=0xd (1101b) the multi-bit pattern is
  > preserved into the high portion of the mask.
- l.810: `int mask = shift << devConst_->numOutputPorts;` — `shift`
  is the *value being shifted*, not the shift amount.

Interpretation:
- `shift` is the LHS operand of `<<`, i.e. the data pattern (one of
  three ZSYNC encoding constants), not a shift count. The comment in
  the function-prologue explicitly flags the naming pitfall.

Judgement:
- Misnomer. The value is a 4-bit data pattern; calling it `shift`
  invites the exact misreading the comment warns against.

Proposals:
- `dataPattern` (medium)
- `zsyncCode` (medium) — matches the readConst constant family
  `ZSYNC_DATA_*`.
- `pattern` (low)
- keep current (low) — would require the comment as a permanent guard.

### playWaveZSync::tableIndex  [unsure / medium / —]

Evidence:
- l.763: `int tableIndex = arg0.value_.toInt();`
- l.775, 781, 787: `if (tableIndex == zsyncRaw.value_.toInt())` etc.
  Compared against the integer values held by `ZSYNC_DATA_RAW`,
  `ZSYNC_DATA_PROCESSED_A`, `ZSYNC_DATA_PROCESSED_B`.
- The comments at l.703 and l.709 use the phrase "table index" but
  neither here nor at the comparison sites is there any
  table — just three discrete enum-like constants.

Interpretation:
- The integer is a constant-id selector among three values, not an
  index into any container. The "table" in the name is unsupported by
  the code. `executeTableEntry` in another method does index a table;
  the name may have been carried over by analogy.

Judgement:
- Probably misleading; cannot prove without seeing the original error
  message string. Marking unsure.

Proposals:
- `constId` (medium)
- `selector` (low)
- `zsyncSelector` (low)
- keep current (low)

### playAuxWave::mask & playDIOWave::mask  [unsure / low / —]

Evidence:
- playAuxWave l.181: `unsigned int mask = 0x3FFF;`. l.269: `mask = 0x3FC3;`.
  l.322: `/*suppress=*/mask` — passed as asmPlay's `suppress` parameter.
- playDIOWave l.492: `int mask = 0x3FFF;`. l.513: `mask &= ~(0x40 << shiftBits);`.
  l.613-614: `holdCount=mask, suppress=(unsigned int)mask` — the same
  variable is passed to TWO asmPlay parameters.
- nm: asmPlay's signature shows
  `…asmPlay(…, int /*holdCount*/, unsigned int /*suppress*/, …)`.

Interpretation:
- In playAuxWave the variable is exclusively the `suppress` argument.
  In playDIOWave it is dual-purposed (holdCount + suppress). Calling
  both `mask` is technically accurate (both are bitmask values) but
  loses the role distinction.

Judgement:
- Borderline. `mask` is correct in genus (a bitmask), under-specific
  in role.

Proposals:
- playAuxWave: `suppressMask` (medium), keep current (medium).
- playDIOWave: keep current (medium) — the dual purpose makes any
  role-name partial. `triggerMask` (low) reflects the
  `~(0x40 << 7b)` clearing pattern derived from marker bits.

### playAuxWave::expectedLen & playDIOWave::expectedLen  [unsure / low / —]

Evidence:
- playAuxWave l.276-277: `int expectedLen = static_cast<int>(devConst_->waveformGranularity);` then `checkOffspecWaveLength(combinedWf, expectedLen);`.
- playDIOWave l.558-559: identical pattern.
- The header (custom_functions.hpp:361) declares
  `void checkOffspecWaveLength(std::shared_ptr<WaveformFront> wf, int expected);`
  — second parameter is `expected`, not `expectedLen`.

Interpretation:
- The value is `waveformGranularity`, used as a divisibility check.
  "Expected length" is loose: the function checks that the waveform
  length is a multiple of this granularity, not that it equals it.

Judgement:
- The name follows the called function's parameter name but slightly
  oversimplifies the semantics. Low-confidence misnomer.

Proposals:
- `granularity` (medium) — matches the source field.
- keep current (medium) — matches `checkOffspecWaveLength`'s
  parameter name.

### waitWave/waitPlayQueueEmpty/error/info::asm_entry  [unsure / low / coordinated-rename]

Evidence:
- l.828, 840, 892, 901 use snake_case `asm_entry`. The same role is
  named `asmEntry` elsewhere in the file (l.315, 607, 683, 813, 924,
  970, 1020) and across the project (camelCase is the dominant style).

Interpretation:
- Inconsistent style within the same file. Not a semantic misnomer.

Judgement:
- Stylistic inconsistency; coordinated rename to `asmEntry` would
  unify the file.

Proposals:
- `asmEntry` for all five sites (medium).

### appendSuser::vec  [unsure / low / —]

Evidence:
- l.35-38: `inline void appendSuser(std::vector<AsmList::Asm>& vec, … ) { vec.push_back(cmds->suser(reg, addr)); }`
- Sole call: l.882 inside `now()`, passing `result->assemblers_`.

Interpretation:
- Generic name for an output container. `assemblers` would tie it to
  the EvalResults field that is always passed. `out` is a common
  convention for output references.

Judgement:
- Not strongly misleading; `vec` is a one-letter-removed `std::vector`
  hint. Low-confidence.

Proposals:
- `assemblers` (low) — matches the only call site's argument name.
- `out` (low)
- keep current (low)

### playAuxWave::emitPlay & playDIOWave::emitPlay  [no / high / not-misnomer]

Evidence:
- Identical role at l.283-286 and l.566-568:
  `bool emitPlay = (combinedWf != nullptr) || config_->isHirzel;`
  followed by `if (emitPlay) { … asmPlay … }`.
- Comments at l.279-282 and l.563-565 describe the bool as the
  "emit-guard" / "skips the asmPlay entirely when …".

Interpretation:
- Variable controls a single conditional: whether to emit the asmPlay
  instruction. Name describes that exactly.

Judgement:
- Not a misnomer.

### playWaveZSync::matched & zsyncRaw/zsyncProcA/zsyncProcB  [no / high / not-misnomer]

Evidence:
- l.769-792: `matched` is set to `true` in three branches, used to
  gate `if (!matched) throw`.
- The three `zsync*` locals hold the result of
  `res->readConst("ZSYNC_DATA_RAW", …)`, `..._PROCESSED_A`,
  `..._PROCESSED_B`. The RTTI/.rodata strings appear at l.773, 779,
  785 and the local names mirror them faithfully.

Interpretation:
- The constant-string names tier-2 evidence (RULES §4d) and the local
  identifiers track them.

Judgement:
- Not misnomers.

### playZero::length  [no / high / not-misnomer]

Evidence:
- l.947: `int length = 0;`
- l.955-957: `length = arg0.value_.toInt(); checkPlayMinLength(length); length = checkPlayAlignment(length);`
- The header (custom_functions.hpp:359-360) declares
  `int checkPlayMinLength(int length);` and
  `int checkPlayAlignment(int length);` — both parameters named
  `length`. The role is sample-count (per `playZero` semantics — it
  plays N zero samples).

Interpretation:
- Local mirrors the parameter name on every call. Semantic role is
  established in the helper functions.

Judgement:
- Not a misnomer.

## 4. Symbols inspected and judged routinely fine

(No recordable evidence beyond "name fits usage".)

- `playArgs` (playAuxWave, playDIOWave): exact type name.
- `parseEnd`: post-parse iterator returned by `playArgs.parse(args)`.
- `rate`: used only as the rate argument forwarded to mergeWaveforms /
  asmPlay / getPlayRate.
- `results` / `result`: the EvalResults shared_ptr returned at end.
- `channelIndex`: copy of `config_->deviceIndex`.
- `assignments`: alias for `playArgs.waveAssignments_[channelIndex]`.
- `combinedWf`: result of `mergeWaveforms`.
- `waveforms`: single-element vector wrapping `combinedWf`.
- `reg`, `reg0`, `regInv`, `regArg`: AsmRegister temporaries with
  literal initializers (0, 0, -1, regNum).
- `asmEntry`: AsmList::Asm value returned by asmPlay/wvft/wwvf/etc.
- `tail` / `playNode`: linked-list manipulation locals; usage is
  unambiguous.
- `argc`: standard short for arg count; matches usage.
- `firstName`, `baseLen`, `zeroArgs`, `zeroWf`, `zeroName`: the
  `zeros()`-placeholder construction chain.
- `channelArgs`: per-channel scattered EvalResultValue list.
- `channelsPerGroup`, `channelsPerGroup0`, `channelsPerGroupS`:
  sourced directly from `config_->channelsPerGroup[i]`.
- `filledCount`: counts assignments in playAuxWave's padding pass.
- `shiftBits` (playDIOWave): bit-shift amount computed `b*7`.
- `mask` (playWaveDIO, playWaveZSync simple cases): wvft argument.
- `tableIndex`, `arg0`, `matched`, `shift` already covered above.
- `setRate::arg0`, `setRate::rate`: trivially named.
- `playHold` mirrors `playZero` symbol-for-symbol; same judgements.
- `now::reg`: AsmRegister(0) literal.
- `randomSeed::rng`, `randomSeed::rd`: standard random-pipeline names.
- `error/info::msg`: string returned by `printF`.

## 5. Coverage

**Fully covered:**
- All 20 method bodies in `custom_functions_playback.cpp`.
- The anonymous-namespace helper `appendSuser`.
- Every local variable and parameter in those functions.

**Deferred:**
- Member field names referenced via `this->` (e.g. `config_`,
  `wavetableFront_`, `warningCallback_`, `devConst_`, `asmCommands_`,
  `waveformGen_`): these are CustomFunctions data members defined in
  the header, owned by another batch (custom_functions.hpp).
- `mergeWaveforms` parameter names (`param3`, `param6` show in
  call-site comments) — owned by the `mergeWaveforms` definition
  batch (likely 05a/b custom_functions other splits or the header).
- `PlayArgs::hasMarker_`, `PlayArgs::waveAssignments_`,
  `WaveAssignment::value`, `WaveAssignment::bits`,
  `EvalResultValue::varType_`, `EvalResultValue::value_`,
  `EvalResultValue::reg_`: data members of types defined elsewhere
  (PlayArgs in 38 / play_config; EvalResultValue/Value in eval_result_value
  batches; WaveAssignment in wave_index_tracker / play-args header).
- `EvalResults::node_`, `EvalResults::assemblers_`: data members of
  EvalResults (its own batch).
- `AsmList::Asm` member `node`: owned by AsmList batch.
- `Node::next`, `Node::last`: owned by Node batch.

**Not covered (out of scope per RULES §2/§3):**
- Method names of `CustomFunctions` (all 20 listed in §2) — present
  in the symbol table at the addresses shown in the file's leading
  comments; verified via
  `nm --demangle _seqc_compiler.so | grep CustomFunctions::play…`
  which lists `playWave`, `playWaveNow`, `playWaveIndexed`,
  `playWaveIndexedNow`, `playAuxWave`, `playAuxWaveIndexed`,
  `playWaveDigTrigger`, `playDIOWave`, `playWaveDIO`, `playWaveZSync`,
  `playZero`, `playHold`, `waitWave`, `waitPlayQueueEmpty`, `sync`,
  `randomSeed`, `now`, `error`, `info`, `setRate`. All excluded.
- Type names appearing in this file (`CustomFunctions`,
  `EvalResultValue`, `EvalResults`, `Resources`, `PlayArgs`,
  `WaveformFront`, `AsmRegister`, `AsmCommands`, `AsmList::Asm`,
  `Value`, `Node`, `VarType`, `AwgDeviceType`, `ExternalTriggeringMode`,
  `CustomFunctionsException`, `WaveformGenerator`, `GlobalResources`,
  `ErrorMessages`): all appear in mangled symbols in nm output (e.g.
  `zhinst::CustomFunctions::…`, `zhinst::AsmCommands::asmPlay`,
  `zhinst::parseOptionalRate(__wrap_iter<zhinst::EvalResultValue
  const*>, …)`, `zhinst::CustomFunctions::mergeWaveforms(…
  zhinst::EvalResultValue …)`). Excluded per RULES §3.
- Free function `parseOptionalRate` — appears in nm at 0x163980;
  excluded.
- Helper method names called in this file (`mergeWaveforms`,
  `checkFunctionSupported`, `checkPlayMinLength`,
  `checkPlayAlignment`, `checkOffspecWaveLength`,
  `checkExternalTriggeringMode`, `setWaitCyclesReg`, `addSyncCommand`,
  `printF`, `getPlayRate`, `play`, `playIndexed`): defined
  outside this file. Method names in nm; not audited here.
- Enum members `VarType_Void`, `VarType_Var`, `VarType_String`,
  `SubFunc::Default`, `SubFunc::Now`, `SubFunc::Aux`,
  `SubFunc::DigTrigger`, `ExternalTriggeringMode::Dio`,
  `ExternalTriggeringMode::ZSync`, `EDirection::eOUT`: defined in
  the header and audited in the corresponding header batch.
- Constants `kDevHirzel`, `kDevHirzelAll`, `kDevAll`, `kSuserNow`,
  `FuncMinArgs`, `FuncExpectsNoArgs`, `FuncExpectsMaxArgs`,
  `FuncArgs2or3`, `FuncExpectsConst`, `SampleRateTooHigh`,
  `DioSampleRateTooHigh`, `InvalidZSyncData`, `FormatFuncArgs`,
  `SetRateOneConst`, `SetRateConst`: namespace-scope constants
  declared in headers (`device_constants.hpp`,
  `error_messages.hpp`, etc.) — owned by their own batches.
