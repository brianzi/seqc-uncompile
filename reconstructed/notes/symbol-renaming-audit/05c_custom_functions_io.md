# Batch 05c — custom_functions_io (PARTIAL — lines 1..~2650)

## 1. Files considered

- `reconstructed/src/custom_functions_io.cpp` (3433 lines) — **partial** scan,
  see §5 for line-range coverage. Header (`custom_functions.hpp`) and other
  `custom_functions_*.cpp` files are **out of scope** per the assignment.

Symbol-table verification:

- All `CustomFunctions::<method>` definitions in this file appear (mangled
  with class qualifier and parameter types) in `nm --demangle
  _seqc_compiler.so`. Examples confirmed: `setDIO`, `getDIO`,
  `getDIOTriggered`, `getZSyncData`, `getFeedback`, `setID`,
  `assignWaveIndex`, `prefetch` (via `play`/SubFunc), `prefetchIndexed`,
  `wait`, `waitTrigger`, `waitAnaTrigger`, `waitDigTrigger`, `waitOnGrid`,
  `waitOnSync`, `waitDIOTrigger`, `waitZSyncTrigger`, `waitCntTrigger`,
  `waitDemodOscPhase`, `waitSineOscPhase`, `waitTimestamp`,
  `resetOscPhase`, `setSinePhase`, `incrementSinePhase`, `waitDemodSample`,
  `setTrigger`, `getTrigger`, `setInternalTrigger`, `getAnaTrigger`,
  `getDigTrigger`, `setInt`, `setDouble`, `generate`, `setUserReg`,
  `getUserReg`, `getSweeperLength`, `setPrecompClear`, `setWaveDIO`,
  `at`, `lock`, `unlock`, `getCnt`, `waitQAResultTrigger`, `getQAResult`,
  `startQAResult`, `startQAMonitor` — **method names excluded from rename**
  per RULES §3.
- The static guard `assignWaveIndex(...)::cLikeIdentifier` is present in
  the symbol table (`b 0000…b84740`), so the regex local
  **name `cLikeIdentifier` is also tier-1 excluded**.
- No exported symbol matches `appendSuser`, `kSuser*`, `kDev*` —
  these remain in scope.

## 2. Overview

Grouping: file-local helper first, then per-method (params → locals).
Most methods have the SAME parameter pair `(args, res)` — these are
reviewed once collectively in §3 / §4 to avoid 50× duplicate rows.

| Symbol | Misnomer? | Conf | Justification (≤5w) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `appendSuser::vec` | unsure | low | Generic name, role clear | keep current; `assemblers` (low) | — |
| `appendSuser::cmds` | no | medium | Matches AsmCommands type | keep current | not-misnomer |
| `appendSuser::reg` | no | medium | Always a register operand | keep current | not-misnomer |
| `appendSuser::addr` | no | medium | Wraps detail::AddressImpl | keep current | not-misnomer |
| `appendSuser` (function) | no | medium | Emits suser AsmList entry | keep current | not-misnomer |
| `<method>::args` (universal) | no | high | Matches dispatch contract | keep current | not-misnomer |
| `<method>::res` (universal) | no | high | Resources& used as such | keep current | not-misnomer |
| `setDIO::supported` | yes | medium | Means "isShfFamily", not "supported" | `isShf`, `useShfDio` | — |
| `setDIO::reg` / `r0` / `newReg` | no | medium | Local AsmRegister handles | keep current | not-misnomer |
| `setDIO::regNum` | no | medium | Result of getRegisterNumber | keep current | not-misnomer |
| `setDIO::asmEntry` | no | medium | AsmList::Asm one-shot | keep current | not-misnomer |
| `getDIO::supported` | yes | medium | Same misuse as setDIO | `isShf` | coordinated-rename |
| `getZSyncData::matched` | no | medium | Bool match flag | keep current | — |
| `getZSyncData::matchedProcessedA/B` | no | medium | Specific match flags | keep current | — |
| `getZSyncData::supportsProcessed` | no | medium | Device supports proc constants | keep current | — |
| `getZSyncData::rawResult2`, `procAResult2`, `procBResult2` | yes | low | Re-read suffix `2` is noise | `rawConst`, `procAConst`, `procBConst` | — |
| `getZSyncData::deviceType` | no | high | Field name match | keep current | not-misnomer |
| `getFeedback::supportsZSync` | unsure | low | Same bitmask as ZSync set, but used to gate ZSyncData consts | `supportsProcessed` (low) | — |
| `setID::supported` | yes | medium | Means isShfFamily | `isShf` | coordinated-rename |
| `assignWaveIndex::optName` | no | high | std::optional<string> for name | keep current | not-misnomer |
| `assignWaveIndex::cLikeIdentifier` | no | high | Static regex, in symbol table | keep current | not-misnomer |
| `assignWaveIndex::argsCopy` | no | medium | Mutable local copy | keep current | — |
| `assignWaveIndex::playArgs` | no | high | Type matches | keep current | not-misnomer |
| `assignWaveIndex::parseEnd` | unsure | low | Iterator AT terminator, not "end" | `terminator`, `lastArgIt` | — |
| `assignWaveIndex::waveIndex` | no | high | Last-arg int, controlled wave idx | keep current | not-misnomer |
| `assignWaveIndex::maxSampleLen` | no | high | From getMaxSampleLength | keep current | not-misnomer |
| `assignWaveIndex::channelIndex` | yes | medium | Holds `config.deviceIndex`, not "channel" | `deviceIdx` | — |
| `assignWaveIndex::mask` | no | medium | Bit-mask 0x3fff, cleared per channel | keep current | — |
| `assignWaveIndex::shift` | no | medium | i*7 used as bit shift | keep current | — |
| `assignWaveIndex::singleChannel` | yes | low | True when 2nd name **empty** ⇒ name fits | keep current; `secondNameEmpty` (low) | — |
| `assignWaveIndex::wf` | no | high | shared_ptr<WaveformFront> | keep current | not-misnomer |
| `assignWaveIndex::devIdx` | no | high | config.deviceIndex copy | keep current | not-misnomer |
| `assignWaveIndex::channelParam` | yes | medium | Holds `channelsPerGroup[0]`, not "channel" | `channelsPerGroup0`, `channelsPerGrp` | — |
| `prefetch::res` | no | high | Forwarded to play | keep current | not-misnomer |
| `wait::isSimpleDevice` | yes | medium | Means "Hirzel-family" not "simple" | `isHirzel`, `isLegacyDev` | — |
| `wait::varType` | no | high | Holds VarType enum int | keep current | not-misnomer |
| `wait::val` / `dval` | unsure | low | Bare math name; OK in numeric branch | keep current | — |
| `wait::clockCycles` | yes | medium | Holds `triggerLatencyCycles`, not arbitrary "clock" | `triggerLatency`, `latencyCycles` | — |
| `wait::regNum`/`reg`/`zero`/`reg2`/`zero2`/`zero3`/`regNum2` | no | medium | Routine AsmRegister scaffolding | keep current | — |
| `wait::remaining` | no | high | Cycles minus latency | keep current | not-misnomer |
| `wait` `goto done;` (label `done`) | unsure | low | Label is a redundancy for already-checked guard | remove or `valid_done` | — |
| `waitTrigger::trigValue` | no | high | Trigger immediate | keep current | not-misnomer |
| `waitTrigger::trigAddr` | unsure | low | Comments call it "trigger address" but it's just args[1].toInt() (a value); naming asymmetry with `trigValue` is the whole point of the 2-arg form | keep current; `trigVal2` (low) | — |
| `waitTrigger::reg1`/`reg2`/`zero`/`zero2` | no | medium | Operands of wtrig | keep current | — |
| `waitTrigger::val0`/`val1` | no | medium | Equality compare of arg ints | keep current | — |
| `waitAnaTrigger::trigIndex` | no | high | 1 or 2 | keep current | not-misnomer |
| `waitAnaTrigger::erv` | no | medium | EvalResultValue from readConst | keep current | — |
| `waitAnaTrigger::trigVal` / `trigVal2` | yes | low | `trigVal2` is identical re-read of erv → name suggests different value | drop second `addi` or rename `trigValAgain` (low) | — |
| `waitDigTrigger::isSupported` (lambda) | no | medium | Predicate name fits | keep current | — |
| `waitDigTrigger::trigIdx` / `trigIndex` | yes | medium | Two locals with same role across branches | unify on `trigIndex` | coordinated-rename |
| `waitDigTrigger::arg1Bool` | yes | medium | True⇒same-reg, but name says nothing of intent | `useSameReg`, `sameRegister` | — |
| `waitOnGrid::trigConst` / `trigValue` | no | medium | EvalResultValue + extracted int | keep current | — |
| `waitDIOTrigger::supported` / `ecx` | yes | high | `ecx` is raw register-name leakage from disasm | `idx` (medium); `supported`→`useDirectPath` (low) | — |
| `waitDIOTrigger::trigImm` | no | medium | Immediate for addi | keep current | — |
| `waitZSyncTrigger::useZSyncConst` | no | high | Name matches role exactly | keep current | not-misnomer |
| `waitZSyncTrigger::supported` | yes | low | Same reuse pattern as waitDIOTrigger | `useDirectPath` (low) | — |
| `waitCntTrigger::counterIdx` | no | high | Counter index, range-checked 0..1 | keep current | not-misnomer |
| `waitCntTrigger::constName` | no | high | Built constant name | keep current | not-misnomer |
| `waitDemodOscPhase::trigIdx` | no | high | Demod-trigger 1..8 | keep current | not-misnomer |
| `waitDemodOscPhase::warnMsg` | no | high | Warning string | keep current | not-misnomer |
| `waitSineOscPhase::didReadConst` | no | medium | Tracks branch path to readConst | keep current | — |
| `waitSineOscPhase::isRegisterArg` | no | medium | Tracks register-typed arg branch | keep current | — |
| `waitSineOscPhase::oscIdx` | no | high | Oscillator index | keep current | not-misnomer |
| `resetOscPhase::oscMask`, `allMask`, `stAddr`, `regNum2`, `reg2` | no | medium | Routine names | keep current | — |
| `resetOscPhase::pathErv` / `valErv` / `typeErv` | no | high | Hand-built EvalResultValues for writeToNode args | keep current | not-misnomer |
| `setSinePhase::oscIndex` / `phaseVal` / `phase` | no | high | Roles match names | keep current | not-misnomer |
| `setSinePhase::nodeIdx` / `nodeOffset` | unsure | low | `nodeIdx` is overwritten by lea-derived value, then never used (path is built independently) | drop `nodeIdx`/`nodeOffset` (low) | — |
| `setSinePhase::resultPtr`, `rbx` | yes | high | Disasm-leakage names ("rbx") with no semantic role; also `resultPtr` is dead | drop both | — |
| `incrementSinePhase` (mirror of setSinePhase) | yes | medium | `nodeIdx`, `nodeOffset` dead in same way | drop dead locals | coordinated-rename |
| `waitDemodSample::trigIdx` / `trigConst` / `trigNames` | no | high | All accurate | keep current | not-misnomer |
| `waitDemodSample::regNum1/2/3, reg1/2/3, zero1/2/3` | no | medium | 3 register triple | keep current | — |
| `setTrigger::reg`/`newReg`/`asmEntry` | no | medium | Routine | keep current | — |
| `getTrigger::reg1`/`reg2`/`addiEntries`/`andrEntry` | no | high | Match strig/ltrig/andr roles | keep current | not-misnomer |
| `setInternalTrigger` (parallels setTrigger) | no | medium | Routine | keep current | — |
| `getAnaTrigger::localArg` | yes | medium | Holds either copy of arg OR readConst result — overloaded role | split into `argCopy` + `trigConst` | — |
| `getAnaTrigger::argVal` | no | high | int 1/2 dispatch value | keep current | not-misnomer |
| `getDigTrigger::localArg` / `resolvedArg` | no | high | Names already differentiate the dual role; comment confirms | keep current | not-misnomer |
| `getDigTrigger::label` | no | high | "dtzero" branch label | keep current | not-misnomer |
| `setInt::arg1Type` | no | high | Holds varType_ as int for bitmask | keep current | not-misnomer |
| `setInt::emptyErv` | yes | medium | Not actually empty — varType=String, value=1.0 | `defaultTypeArg`, `nodeTypeDefault` | — |
| `setDouble::arg2` | no | medium | 3rd arg with default | keep current | — |
| `setDouble::sz` | no | medium | size_t, used in bitmask test | keep current | — |
| `generate::firstArg` | no | high | Refers to args[0] | keep current | not-misnomer |
| `generate::localArgs` | no | medium | args[1..end] copy | keep current | — |
| `generate::values`, `name`, `genResult` | no | medium | Routine | keep current | — |
| `generate::erv` | no | medium | Each arg in loop | keep current | — |
| `setUserReg::arg0Val` | no | high | Address int from arg[0] | keep current | not-misnomer |
| `setUserReg::reg`/`reg2`/`luserEntry`/`trapEntry` | no | medium | Routine | keep current | — |
| `getUserReg::userRegIdx` / `userRegInt` | yes | low | Two locals same value, second redundant | drop `userRegInt` | — |
| `getUserReg::immVal` | unsure | low | Value is `seqClockDivider`, name `immVal` hides that | `clockDivider` | — |
| `getSweeperLength::sweepIdx` | no | high | Sweep index 0/1 | keep current | not-misnomer |
| `getSweeperLength::constResult` / `rc` / `constVal` | no | medium | Routine | keep current | — |
| `setPrecompClear::val` / `flag` | no | medium | val→bool→flag pattern | keep current | — |
| `at::warnMsg` / `arg0` / `reg` / `reg2` / `timeTrigErv` / `timeTrigVal` | no | high | Roles match | keep current | not-misnomer |
| `lock::name` / `optName` / `wf` | no | high | Wave name, optional, waveform | keep current | not-misnomer |
| `unlock` (mirror of lock) | no | high | Same | keep current | not-misnomer |
| `getCnt::counterIdx` | no | high | Counter index for lcnt | keep current | not-misnomer |
| `waitQAResultTrigger::erv` / `trigAddr` / `reg` / `zero` | no | medium | Routine; `trigAddr` matches comment "trigger address" | keep current | — |
| `getQAResult::reg`/`asmEntry` | no | medium | Routine | keep current | — |
| `startQAResult::qaIntAllErv` / `qaIntAll` | no | high | Default integration mask const | keep current | not-misnomer |
| `startQAResult::resultAddr` | no | high | Result address from arg[1] | keep current | not-misnomer |
| `startQAResult::imm` | unsure | low | Bare; computed as `(qaIntAll<<16)+resultAddr+0x10` | `startWord`, `cmdWord` (low) | — |
| `startQAMonitor::monitorVal` | no | high | Monitor argument | keep current | not-misnomer |

Constants used (declared elsewhere; flagged here for cross-batch awareness only — **declarations not in this file**):

- `kSuserWaitCycles` (0x69), `kSuserTriggerLoad` (0x1a), `kSuserWaitOnSync`,
  `kSuserTimestamp`, `kSuserSinePhase0/1`, `kSuserSinePhaseInc0/1`,
  `kSuserNodeTag`, `kSuserNodeAddr`, `kSuserRTLoggerData`,
  `kSuserQAResult`, `kDevHirzel*`, `kDevCervino`, `kDevAllButUHF`,
  `kDevLIFamily`, `kDevAll`, `kDevSHFPlus`, `kDevPreSHFLI`, `kDevNone`.
- Not in `nm`; in scope as constants but their **declarations** live
  in `custom_functions.hpp` / `device_constants.hpp` and are out of
  scope for this batch (cross-ref to batch 31 device_constants and to
  the header batch).

## 3. Detailed findings

### `setDIO::supported` / `getDIO::supported` / `setID::supported` / others  [yes / medium / coordinated-rename]

Evidence:
- src/custom_functions_io.cpp:58 `bool supported = isShfFamily();`
- src/custom_functions_io.cpp:104 (getDIO) same
- src/custom_functions_io.cpp:344 (setID) same
- usage at :66, :86, :108, :349 — passed as second bool arg to
  `asmCommands_->sdio(reg, supported)`, `ldio(reg, supported)`,
  `sid(reg, supported)`. The comment block at :86–:94 explicitly says
  "SHF path (isShfFamily): no lookupNode call — binary jumps to …".

Interpretation:
- The bool is *exclusively* a "is SHF family device" predicate: the
  selector for which suser/store address the AsmCommands variant emits.
  "supported" suggests a generic "is this operation supported here"
  check, but no fallback or error is keyed on the variable; only the
  dispatch flavor changes.

Judgement:
- Misleading. The name disguises a device-family predicate as a
  feature-availability check.

Proposals:
- `isShf` (medium)
- `useShfDio` / `shfVariant` (low) — kept as alternative because the
  AsmCommands API parameter on the receiving side may be named
  differently; cross-batch arbitration with batch 10 (asm_commands) /
  49 (asm_commands_impl) is appropriate before settling.
- keep current (low)

Cross-reference:
- AsmCommands::sdio / ldio / sid second parameter — see batches 10/49.

Locations consulted:
- src/custom_functions_io.cpp:49–112, 338–363.

### `wait::isSimpleDevice`  [yes / medium / —]

Evidence:
- src/custom_functions_io.cpp:565–574 "isSimpleDevice = false … if
  Hirzel mask matches → true; if VHFLI/GHFLI → true".
- :612–621 (numeric branch) duplicates the same bitmask; same name.
- The exact same check (deviceType bitmask 0x4000000040004041 with VHFLI/
  GHFLI fall-through) appears in `waitDIOTrigger` (:986–993) under name
  `supported`, in `waitZSyncTrigger` (:1068–1092) under `supported`,
  and in `waitDigTrigger` (:837–844) inside lambda `isSupported`.

Interpretation:
- All four predicates compute *the same device-family check*; only
  one ("isSimpleDevice") implies a "simple/complex" distinction, but
  there is no such distinction in usage — the difference is
  Hirzel/SHF family vs. Cervino. Names are inconsistent across
  sites for the same predicate.

Judgement:
- Misleading; "simple/complex" is invented. Inconsistency with peer
  sites is the high-confidence finding.

Proposals:
- `isHirzelFamily` (medium), `isLegacyDevice` (low),
  `useSimpleSuser` (low). Coordinated rename across the four sites
  is preferred.
- keep current (low).

Locations consulted:
- src/custom_functions_io.cpp:565–574, 612–621, 837–844, 986–993,
  1068–1092.

### `assignWaveIndex::channelIndex`  [yes / medium / —]

Evidence:
- src/custom_functions_io.cpp:416 `int channelIndex = config.deviceIndex;`
- :510 `int devIdx = config.deviceIndex;` — *same value*, *different
  name* a few dozen lines later in the same function.
- :515 `asmEntry.node->deviceIndex = config.deviceIndex;`

Interpretation:
- The first local is a copy of `config.deviceIndex`; the second local,
  same function, is named `devIdx` for the same field. The same field
  is assigned to `node->deviceIndex` immediately after. Calling it
  `channelIndex` is at odds with three nearby occurrences of the
  proper name.

Judgement:
- Misleading; the field is "deviceIndex" everywhere else.

Proposals:
- `deviceIdx` (medium), to match the second local and the field.
- `deviceIndex` (low) — collides with the field name in this method.

Locations consulted:
- src/custom_functions_io.cpp:416–417, 510–515.

### `assignWaveIndex::channelParam`  [yes / medium / —]

Evidence:
- :457 `short channelParam = static_cast<short>(config.channelsPerGroup[0]);`
- Passed as 2nd arg of `mergeWaveforms(channelArgs, channelParam, …)`
  whose declaration in nm is
  `mergeWaveforms(… const&, short, bool, … const&, int, bool)`.

Interpretation:
- The value is `channelsPerGroup[0]`. "channelParam" tells the reader
  nothing — it just restates "this is the parameter slot". The
  matching declaration parameter in `mergeWaveforms` is anonymous in
  the symbol table (parameter names not encoded), so this is a free
  rename.

Judgement:
- Misleading and content-free.

Proposals:
- `channelsPerGroup0` (medium), `channelsPerGrp` (low).
- keep current (low).

Cross-reference:
- `CustomFunctions::mergeWaveforms` 2nd parameter (lives in different
  TU, custom_functions.cpp or custom_functions_play.cpp); cross-batch
  arbitration applies if that TU is audited separately.

### `setSinePhase::resultPtr` / `setSinePhase::rbx`  [yes / high / —]

Evidence:
- src/custom_functions_io.cpp:1568–1569
  ```
  auto resultPtr = results.get();
  auto rbx = asmCommands_;
  ```
- Neither variable is read after this point (subsequent code uses
  `results->...` and `asmCommands_->...` directly).

Interpretation:
- `rbx` is a register name leaked from disassembly; `resultPtr` is
  dead. Both serve no role in the source.

Judgement:
- Misleading (`rbx`) and dead — both should be removed.

Proposals:
- delete both lines (high).
- If kept for some reason, rename `rbx` → `cmds` (low).

Locations consulted:
- src/custom_functions_io.cpp:1568–1576.

### `setSinePhase::nodeIdx` / `nodeOffset` and `incrementSinePhase` mirrors  [yes / medium / coordinated-rename]

Evidence:
- :1530 `int nodeIdx = devConst_->sineNodeBase * devConst_->maxBlocks
  + devConst_->waveformElfAlignment;`
- :1533 `nodeIdx = oscIndex + nodeIdx * 2;`
- :1536 `int nodeOffset = devConst_->sineNodeBase;`
- After :1536 the `path` is built independently from a string literal
  `"sines/" + to_string(oscIndex) + "/phaseshift"` (:1541) — neither
  `nodeIdx` nor `nodeOffset` is used.
- Mirror pattern in `incrementSinePhase` (:1642–:1643, :1674).

Interpretation:
- Both locals are dead in the reconstructed source. They appear to
  be partial reconstruction artifacts of an arithmetic that the
  binary performs but whose result the binary then overwrites.

Judgement:
- The names themselves are not actively misleading, but the locals
  are dead and should be removed in coordination across both
  methods.

Proposals:
- Remove dead locals in both methods (medium).
- If kept (because the arithmetic is intended to be wired into the
  path eventually), rename to `_unusedNodeIdx` etc. (low).

### `getAnaTrigger::localArg`  [yes / medium / —]

Evidence:
- src/custom_functions_io.cpp:1870–1877 `EvalResultValue localArg = arg;
  int argVal = localArg.value_.toInt(); … localArg = erv;` — overwritten
  with the readConst result for argVal in {1,2}.
- :1896 `addi(reg2, …, Immediate(localArg.value_.toInt()))` reads from
  the **post-overwrite** value (the trigger constant for `argVal == 1
  || argVal == 2`, otherwise the original arg).

Interpretation:
- `localArg` plays two roles: dispatch input *and* trigger constant.
  The same dual-role pattern was avoided in `getDigTrigger` where two
  separate locals (`localArg` and `resolvedArg`) are used and the
  comment at :1927 calls this out explicitly.

Judgement:
- Misleading; the variable's identity changes mid-function.

Proposals:
- Split into `argCopy` (initial) + `trigConst` (post-readConst)
  (medium), mirroring `getDigTrigger`.
- keep current (low).

Cross-reference:
- `getDigTrigger::localArg` / `resolvedArg` (same file) — already
  uses the cleaner pattern. Coordinate.

### `setInt::emptyErv`  [yes / medium / —]

Evidence:
- :1995–:1999
  ```
  EvalResultValue emptyErv{};
  emptyErv.varType_ = VarType_String;
  emptyErv.value_ = Value(1.0);
  emptyErv.varSubType_ = VarSubType(2);
  return writeToNode(arg0, arg1, emptyErv, std::move(res));
  ```

Interpretation:
- "empty" is contradicted by the three explicit field assignments.
  This object is the *default 3rd argument* to `writeToNode`, mirroring
  `setDouble::arg2` at :2012–:2015 (which is named accurately as
  the third arg with default).

Judgement:
- Misleading; the value is not empty.

Proposals:
- `defaultTypeArg` (medium), `nodeTypeDefault` (low),
  `arg2` (low — to mirror `setDouble`).

### `getUserReg::userRegIdx` / `userRegInt`  [yes / low / —]

Evidence:
- :2174 `int userRegIdx = arg0.value_.toInt();`
- :2189 `int userRegInt = arg0.value_.toInt();` — second `toInt()`
  call producing the same value (binary may have re-fetched but the
  reconstruction collapses to one logical value).
- Only `userRegInt` is used at :2190 in the `luser` call.

Interpretation:
- Two locals of the same value; the second's name says nothing about
  role beyond "it's an int from arg".

Judgement:
- Mildly misleading and redundant.

Proposals:
- Drop `userRegInt`, reuse `userRegIdx` (medium).

### `appendSuser` (file-static helper)  [no / medium / not-misnomer]

Evidence:
- src/custom_functions_io.cpp:37–41 — wraps a single
  `cmds->suser(reg, addr)` call and pushes the result onto `vec`.
- All call sites (e.g. :578, :594, :636, :677, :1383, :1411, :1523,
  :1526, :1576, :1636, :1638, :1672, :2109, :2114, :2123, :2129,
  :2137, :2212, :2339, :2356) pass `(results->assemblers_,
  asmCommands_, <reg>, AddressImpl<unsigned int>(<addr>))`.

Interpretation:
- Function name verbatim describes what it does: append a `suser`
  AsmCommands entry. Parameters match by type and role.

Judgement:
- Name is correct.

### `<method>::args`, `<method>::res`  [no / high / not-misnomer]

Evidence:
- The CustomFunctions dispatch contract is `(std::vector<EvalResultValue>
  const& args, std::shared_ptr<Resources> res)` — replicated 50× in
  this file.
- Symbol table shows the parameter *types* but not names; nothing
  contradicts.

Interpretation:
- `args` and `res` are the canonical names for the dispatcher's
  argument list and the Resources handle. Renaming would also have to
  cascade through `custom_functions.cpp`, `custom_functions_play.cpp`,
  the dispatcher in batch 51 (callbacks), and probably `Resources`.

Judgement:
- Names are correct and conventional.

## 4. Symbols inspected and judged routinely fine

(Bullet form. ≤ one short sentence each.)

- `setDIO::arg`, `getZSyncData::arg0`, `getFeedback::arg0` — references
  to the validated single argument; role-faithful.
- `setDIO::regNum`, `setID::regNum`, etc. — the `int` from
  `Resources::getRegisterNumber()`; convention throughout codebase.
- `<*>::reg`, `<*>::zero`, `<*>::reg1/reg2/zero1/zero2` — ad-hoc
  AsmRegister handles; role obvious from context.
- `<*>::asmEntry`, `<*>::wtrigEntry`, `<*>::ltrigEntry`, `<*>::brzEntry`,
  `<*>::oneEntry`, `<*>::lblEntry`, `<*>::lcntEntry`, `<*>::luserEntry`,
  `<*>::stEntry`, `<*>::nopEntry`, `<*>::andrEntry`, `<*>::trapEntry`
  — single-emit AsmList::Asm temporaries.
- `<*>::addiEntries`, `addiEntries1/2/3` — `vector<AsmList::Asm>` from
  multi-emit `addi`; consistent.
- `<*>::results` — universal `shared_ptr<EvalResults>`; matches type.
- `assignWaveIndex::config`, `assignments`, `wa`, `bits`, `bit`,
  `clearBit`, `secondName`, `playConfig`, `path` — all role-clear.
- `getZSyncData::dt`, `getFeedback::dt` — short for `deviceType` int
  copy; OK in this scope.
- `wait::warningCallback_` (member) — out of scope (member, in header).
- `waitTrigger::trigValue` — the immediate loaded for the trigger.
- `waitDIOTrigger::deviceType` — local int copy; role match.
- `waitZSyncTrigger::supportedMask`, `idx` — bitmask scaffolding.
- `waitCntTrigger::trigValue`, `constName` — straightforward.
- `waitDemodOscPhase::trigVal`, `regNum`, `reg`, `zero` — routine.
- `waitSineOscPhase::devType`, `args.empty()` branches — names OK.
- `resetOscPhase::devType`, `oscMask`, `allMask`, `stAddr`,
  `addiEntries2`, `regNum2`, `reg2`, `zero2` — routine.
- `setSinePhase::oscIndex`, `phaseVal`, `phase`, `nodeIdx` (other than
  the dead-local concern), `path`, `node` — names match.
- `incrementSinePhase` mirrors `setSinePhase`; same judgement.
- `waitDemodSample::trigNames` (static array) — OK.
- `setTrigger::reg`, `newReg`, `addiEntries`, `asmEntry` — routine.
- `getTrigger::reg1/reg2`, `addiEntries`, `ltrigEntry`, `andrEntry`,
  `regNum1/2` — routine.
- `setInternalTrigger` — same as setTrigger.
- `getAnaTrigger::reg1/reg2`, `addiEntries`, `ltrigEntry`, `andrEntry`,
  `brzEntry`, `oneEntry`, `lblEntry`, `batch`, `label` — routine
  (label string `"atzero"` is content of the asm label, not the
  variable's name).
- `getDigTrigger::reg1/reg2`, asm temporaries, `label` (`"dtzero"`)
  — routine.
- `setInt::arg0`, `arg1`, `arg1Type` — routine.
- `setDouble::arg0`, `arg1`, `arg2`, `arg1Type`, `sz` — routine.
- `generate::firstArg`, `localArgs`, `values`, `name`, `genResult`,
  `erv` — routine.
- `setUserReg::arg0`, `arg1`, `regNum`, `reg`, `regNum2`, `reg2`,
  `addiEntries`, `luserEntry`, `trapEntry`, `arg0Val` — routine.
- `getSweeperLength::arg0`, `sweepIdx`, `constResult`, `rc`,
  `regNum`, `reg`, `constVal`, `luserEntry` — routine.
- `setPrecompClear::val`, `flag`, `asmEntry` — routine.
- `at::arg0`, `warnMsg`, `regNum/reg/zero/regNum2/reg2/zero2`,
  `addiEntries`, `addiEntries2`, `wtrigEntry`, `timeTrigErv`,
  `timeTrigVal`, `val` — routine.
- `lock::name`, `optName`, `wf`, `asmEntry` — routine.
- `unlock` — same as lock.
- `getCnt::arg0`, `counterIdx`, `regNum`, `reg`, `lcntEntry` — routine.
- `waitQAResultTrigger::erv`, `trigAddr`, `regNum`, `reg`, `zero`,
  `addiEntries`, `asmEntry` — routine.
- `getQAResult::regNum`, `reg`, `asmEntry` — routine.
- `startQAResult::qaIntAllErv`, `qaIntAll`, `resultAddr`, `arg0`,
  `arg1`, `regNum`, `reg`, `zero`, `imm`, `addiEntries`, `asmEntry`
  — routine.
- `startQAMonitor::monitorVal`, `arg0`, `regNum`, `reg`, `zero`,
  `addiEntries`, `asmEntry` — routine.
- All `kSuser*` and `kDev*` constant *uses* in this file — only the
  references appear here; declarations elsewhere. No misuse observed
  at any call site.
- All `VarType_*` enum references — verified against `nm` (these
  enumerator values are widely used; their declarations live in
  `types.hpp`, audited in batch 01).

Symbols **not looked at** in this batch — see §5.

## 5. Coverage

### Fully covered (lines 1..2649)

File-local helper:
- `appendSuser` (anonymous-namespace inline) at lines 36–41.

Methods examined in detail (parameters, locals, all interior names):

| # | Method | Lines |
|---|---|---|
| 1 | `setDIO` | 49–97 |
| 2 | `getDIO` | 98–112 |
| 3 | `getDIOTriggered` | 113–126 |
| 4 | `getZSyncData` | 127–225 |
| 5 | `getFeedback` | 226–337 |
| 6 | `setID` | 338–363 |
| 7 | `assignWaveIndex` | 364–526 |
| 8 | `prefetch` | 527–531 |
| 9 | `prefetchIndexed` | 532–536 |
| 10 | `wait` | 538–693 |
| 11 | `waitTrigger` | 694–761 |
| 12 | `waitAnaTrigger` | 762–831 |
| 13 | `waitDigTrigger` | 832–936 |
| 14 | `waitOnGrid` | 937–951 |
| 15 | `waitOnSync` | 952–963 |
| 16 | `waitDIOTrigger` | 964–1040 |
| 17 | `waitZSyncTrigger` | 1041–1135 |
| 18 | `waitCntTrigger` | 1136–1191 |
| 19 | `waitDemodOscPhase` | 1192–1253 |
| 20 | `waitSineOscPhase` | 1254–1313 |
| 21 | `waitTimestamp` | 1314–1323 |
| 22 | `resetOscPhase` | 1325–1473 |
| 23 | `setSinePhase` | 1475–1585 |
| 24 | `incrementSinePhase` | 1588–1692 |
| 25 | `waitDemodSample` | 1695–1783 |
| 26 | `setTrigger` | 1784–1808 |
| 27 | `getTrigger` | 1809–1833 |
| 28 | `setInternalTrigger` | 1834–1858 |
| 29 | `getAnaTrigger` | 1859–1915 |
| 30 | `getDigTrigger` | 1916–1976 |
| 31 | `setInt` | 1977–2000 |
| 32 | `setDouble` | 2001–2035 |
| 33 | `generate` | 2036–2081 |
| 34 | `setUserReg` | 2082–2149 |
| 35 | `getUserReg` | 2150–2222 |
| 36 | `getSweeperLength` | 2223–2290 |
| 37 | `setPrecompClear` | 2291–2306 |
| 38 | `setWaveDIO` | 2307–2311 |
| 39 | `at` | 2312–2376 |
| 40 | `lock` | 2377–2396 |
| 41 | `unlock` | 2397–2416 |
| 42 | `getCnt` | 2417–2469 |
| 43 | `waitQAResultTrigger` | 2470–2532 |
| 44 | `getQAResult` | 2533–2546 |
| 45 | `startQAResult` | 2547–2606 |
| 46 | `startQAMonitor` | 2607–2649 |

### Deferred — to be picked up in `05c_custom_functions_io_part2.md`

Lines 2650–3433. Methods deferred:

| # | Method | Approx. lines | Binary addr |
|---|---|---|---|
| 47 | `executeTableEntry` | 2653–2758 | @0x150900 |
| 48 | `setPRNGSeed` | 2759–2800 | @0x1513e0 |
| 49 | `getPRNGValue` | 2801–2814 | @0x151a70 |
| 50 | `setPRNGRange` | 2815–2879 | @0x151ce0 |
| 51 | `startQA` | 2880–3021 | @0x152690 |
| 52 | `resetRTLoggerTimestamp` | 3022–3035 | @0x153f90 |
| 53 | `configFreqSweep` | 3036–3127 | @0x154240 |
| 54 | `setSweepStep` | 3128–3230 | @0x155640 |
| 55 | `setOscFreq` | 3231–3330 | @0x156a70 |
| 56 | `configureFeedbackProcessing` | 3331–3433 | @0x157e60 |

**Why deferred:** context budget. The 46 methods above produced enough
material for one report; the remainder (10 methods, ~780 lines) — most
notably the very large `startQA` (~6.2 KB), `configFreqSweep` (~5 KB),
`setSweepStep` (~5 KB), `setOscFreq` (~5 KB), and
`configureFeedbackProcessing` (~5.6 KB) — warrant a fresh subagent.
The PRNG group (`setPRNGSeed`, `getPRNGValue`, `setPRNGRange`)
particularly likely contains misnamed range/seed locals worth a careful
look.

### Not covered (out of scope per RULES §2/§3)

- All `CustomFunctions::<method>` *names* — present in the symbol table
  (mangled with class qualifier and parameter types). Per §3 these are
  authoritative and excluded from rename. Confirmed for every method in
  the §5 "fully covered" table.
- `CustomFunctions` member fields (`asmCommands_`, `config_`,
  `devConst_`, `wavetableFront_`, `waveformGen_`, `warningCallback_`,
  `errMsg`, `assignedWaveNames_`, `externalTriggeringMode_`, etc.) —
  declared in the header (`custom_functions.hpp`), out of scope for
  this batch. Their *uses* here (member-access expressions) are not
  themselves renameable symbols.
- `CustomFunctions` nested types (`SubFunc`, `PlayArgs`,
  `ExternalTriggeringMode`, `AccessMode`) — declared in header, out of
  scope here.
- `CustomFunctions::lookupNode`, `addNodeAccess`, `mergeWaveforms`,
  `checkFunctionSupported`, `checkExternalTriggeringMode`,
  `setWaitCyclesReg`, `addSyncCommand`, `oscMaskCheckHirzel`,
  `oscMaskSetAllHirzel`, `checkOffspecWaveLength`, `writeToNode`,
  `play`, `isShfFamily`, `isConstOrCvar` — method *names* in symbol
  table (some confirmed via `nm` query above). Their parameter
  *names* live in their definitions, mostly in
  `custom_functions.cpp` / other split files — out of scope.
- `kSuser*` and `kDev*` constants — uses only here; declarations live
  in `device_constants.hpp` / `custom_functions.hpp`. Cross-batch:
  batch 31 (device_constants), and the header batch when it is
  scheduled.
- `VarType_*`, `VarSubType`, `EDirection::eOUT`, `AwgDeviceType::*`,
  `AccessMode::*`, `ExternalTriggeringMode::*` — enumerator *names*
  declared elsewhere; uses here are routine and cross-checked in
  prior batches (01, 23, 27, 29).
- `AsmRegister`, `AsmList::Asm`, `Immediate`, `EvalResultValue`,
  `EvalResults`, `Value`, `Resources`, `WaveformFront`, `NodeMap`,
  `PlayArgs` — *type* names; out of scope (declared in headers,
  some in symbol table).
- All `ErrorMessages::format` enum-id arguments (`FuncExpectsSingleArg`,
  `FuncExpectsConst`, `FuncExpectsConstConst`, `FuncExpectsNoArgs`,
  `FuncCalledWithLogical`, `InvalidZSyncData`, `WaitPositive`,
  `OnlyConstWaveIndex`, `WaveIndexExceedsTable`, `IndexMustBe`,
  `NotSupportedGrouping`, `SineGenIndex`, `SetTriggerArgs`,
  `SetInternalTriggerArgs`, `SetIntArgs`, `SetIntVarConstSecond`,
  `SetDoubleArgs`, `SetDoubleVarConstSecond`, `SetDoubleConstThird`,
  `GenerateExpectsString`, `FuncNoArgsGiven`, `CantCallWithVar`,
  `SetUserRegArgs`, `GetUserRegArgs`, `GetUserRegRange`,
  `GetSweeperLenArgs`, `GetSweeperLenArg`, `SetPrecompOneConst`,
  `SetPrecompConst`, `LockArgs`, `LockOnlyWave`, `UnlockArgs`,
  `UnlockOnlyWave`, `WaveformNotExist`, `GetCntRange`,
  `FuncExpectsMaxArgs`, `ExpectsTwoConst`, `ExpectsOneConst`) —
  enumerators declared in `error_messages.hpp` / handled in
  batch 08. Out of scope for this file.
