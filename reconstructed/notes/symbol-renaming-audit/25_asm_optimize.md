# Batch 25 — asm_optimize

## 1. Files considered

- `reconstructed/include/zhinst/asm_optimize.hpp`
- `reconstructed/src/asm_optimize.cpp`

`nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so`
authoritatively names the type `zhinst::AsmOptimize`, the exception
type `zhinst::OptimizeException`, every method in scope, and the lambda
`registerAllocation::$_0`. It also names two type aliases nested in
`registerAllocation`: `LiveRange` and `PhysicalRegister` (visible in
the destructor symbol of `__split_buffer<...::LiveRange,...>` at
`281d80` and the `vector<...::PhysicalRegister>::~vector` at
`281840`). Those are tier-1 §3 type names — excluded.

What `nm` does NOT cover and is therefore in scope: parameter names,
data-member names, enumerator names of the two file-local enums
(`OptFlag`, `RegAction`), local variables (only when misleading), the
private `OptimizeException::message_` field, and the
`isWaveformCmd`/`wavetableFront`/`lineNumber` accessor names that
arrive here from the cross-batch cluster.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `OptFlag` (enum) | yes | medium | bitmask flags, type opaque | OptPassFlag, OptimizationPass (medium); keep (low) | — |
| `OptFlag::Opt_JumpElim..Opt_RegAlloc` (enumerators) | no | medium | comments match pass names | keep current (medium) | — |
| `RegAction` (enum) | no | medium | local helper, name fits | keep current (medium) | — |
| `RegAction::RegAction_NotFound..RegAction_ReadWritten` | yes | medium | redundant prefix, sentinel name | RegAction::None/Read/Written/Both (medium); keep (low) | — |
| `OptimizeException` (type) | no | high | name in nm | keep current (high) | not-misnomer |
| `OptimizeException::message_` (field) | no | high | only consumer is `what()` | keep current (high) | — |
| `OptimizeException::OptimizeException::msg` (param) | no | high | passed straight to `message_` | keep current (high) | — |
| `AsmOptimize::numPhysicalRegs_` (field) | no | high | matches usage in regAlloc | keep current (high) | — |
| `AsmOptimize::pad04_` (field) | no | high | layout padding | keep current (high) | — |
| `AsmOptimize::flags_` (field) | yes | medium | conflated bitmask + sentinel | optFlags_, passFlags_ (medium); keep (low) | — |
| `AsmOptimize::pad0C_` (field) | no | high | layout padding | keep current (high) | — |
| `AsmOptimize::asm_` (field) | no | medium | working copy of asm list | keep current (medium); workingList_ (low) | — |
| `AsmOptimize::errorCallback_` (field) | no | high | called in reportUserMessages | keep current (high) | — |
| `AsmOptimize::warningCallback_` (field) | no | high | called in reportUserMessages | keep current (high) | — |
| `AsmOptimize::cancel_` (field) | unsure | low | accessed once via lock | keep current (medium); cancelCallback_ (low) | — |
| `AsmOptimize::AsmOptimize::errorCallback,warningCallback,numPhysicalRegs,flags,cancel` (params) | mostly no | high | mirror fields | keep current (high); flags→optFlags (low) | — |
| `AsmOptimize::prepareResources::asmList` (param) | no | high | only `.maxRegister()` called | keep current (high) | — |
| `AsmOptimize::optimizePreWaveform::input` (param) | no | high | source of working copy | keep current (high) | — |
| `AsmOptimize::optimizePostWaveform::input` (param) | no | high | source of working copy | keep current (high) | — |
| `optimizePostWaveform::backup,newNumRegs,numRegs` (locals) | no | medium | descriptive | keep current (medium) | — |
| `AsmOptimize::isRead::instr,reg` (params) | no | high | conventional | keep current (high) | — |
| `AsmOptimize::isWritten::instr,reg` (params) | no | high | conventional | keep current (high) | — |
| `AsmOptimize::isLabelCalled::label,it` (params) | unsure | low | misleading: scans before `it` only | keep current; rename → `isLabelCalledBefore` (low); param `it`→`upTo` (low) | — |
| `AsmOptimize::getNextActionForReg::it,reg` (params) | no | medium | param names match role | keep current (medium) | — |
| `getNextActionForReg::result,prev,endIt,pos` (locals) | no | high | obvious | keep current (high) | — |
| `AsmOptimize::registerIsNeverWritten::list,reg,start,exclude` (params) | no | high | matches body | keep current (high) | — |
| `AsmOptimize::deadCodeElimination::afterBranch` (local) | no | high | exact role | keep current (high) | — |
| `AsmOptimize::oneStepJumpElimination::targetLabel,next,nextCmd` (locals) | no | high | descriptive | keep current (high) | — |
| `AsmOptimize::removeUnusedLabels::found,label,scan` (locals) | no | high | descriptive | keep current (high) | — |
| `AsmOptimize::mergeLabels::firstLabel,secondLabel,next,scan` (locals) | no | high | descriptive | keep current (high) | — |
| `AsmOptimize::mergeRegisterZeroing::prev,it` (locals) | no | high | conventional | keep current (high) | — |
| `AsmOptimize::removeUnusedRegs::cancelObj` (local) | no | medium | shared_ptr alias | keep current (medium) | — |
| `AsmOptimize::removeUnusedRegs::maxReg,writeOnlyRegs` (locals) | yes | medium | `writeOnlyRegs` is a workset | candidateDestRegs (medium); seenDestRegs (medium); keep (low) | — |
| `removeUnusedRegs::usageFlags,destSlot,destReg,scanIt,alreadyTracked,scanCmd,cmdPlus1,cmdType,regInfo,regNum` (locals) | no | high | descriptive | keep current (high) | — |
| `AsmOptimize::reportUserMessages::imm,msg,lineNr,cmd,it` (locals) | no | high | descriptive | keep current (high) | — |
| `AsmOptimize::simplifyAssign::it` (param) | no | high | iterator into `asm_` | keep current (high) | — |
| `simplifyAssign::next,canSimplify,destReg,zero,scan` (locals) | no | high | descriptive | keep current (high) | — |
| `AsmOptimize::registerAllocation::numRegs` (param) | no | high | drives loop bounds | keep current (high) | — |
| `registerAllocation::liveRanges,labelMap,branchRanges,physRegs,conflicts,physVreg,physPreg,vreg,preg,minPreg,maxVreg,minVreg,maxPregFirst,canMerge,insertPos,scanIdx,instrIdx,vr,br,scanIdx,vregReg,addToLiveRange,cancelLock,cancelObj,numPhysical,totalPhysical,numSlots,conflict,scanIt` (locals) | mostly no | medium | descriptive long names | keep current (medium); see §3 for `BranchRange::sourceIdx` | — |
| `registerAllocation::BranchRange::targetIdx,sourceIdx` (struct fields) | no | medium | reflect direction | keep current (medium) | — |
| `registerAllocation::$_0::reg,instrIdx` (lambda params) | no | high | obvious | keep current (high) | — |
| `LiveRange`, `PhysicalRegister` (type aliases inside registerAllocation) | n/a | high | nm encodes them | keep current (high) | not-misnomer |
| `AsmOptimize::splitConstRegisters::numRegs` (param) | no | high | matches usage | keep current (high) | — |
| `splitConstRegisters::tmpList,magicReg,newSeqId,barrier,orig,cmd,cmdPlus1,isSkipCmd,destReg,scanEnd,needsSplit,scanIt,canSplit,splitEnd,aborted,checkIt,chkCmdType,splitCount,entry,oldEnd,dit` (locals) | mostly no | medium | descriptive | keep current (medium); see §3 for `needsSplit` | — |
| `splitConstRegisters::needsSplit` (local) | yes | low | written but never read | drop or use; flag (low) | — |
| `AsmOptimize::splitReg::list,reg,start,end` (params) | no | high | matches body | keep current (high) | — |
| `splitReg::startOff,endOff,didSplit,allSplitOk,instrCount,newReg,cmd,cmdType,usesReg,ct2,startAsm,endAsm,instr,newRegNum` (locals) | no | high | descriptive | keep current (high) | — |
| `AsmOptimize::registerUpdate::indices,oldReg,newReg` (params) | no | high | obvious | keep current (high) | — |
| `registerUpdate::idx,instr,it` (locals) | no | high | obvious | keep current (high) | — |

Cross-batch cluster reference rows (symbols owned by other batches but
relied on heavily here):

| Symbol | Misnomer? | Conf | Justification (≤5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AsmList::Asm::isWaveformCmd` (used at `oneStepJumpElimination`, `mergeRegisterZeroing`) | yes | high | here used as "skip-opt" gate | renameSee §3 | cross-batch-arbitration |
| `AsmList::Asm::lineNumber()` (used in `reportUserMessages`) | no | high | accessor faithfully named | keep current (high) | not-misnomer |
| `AsmList::Asm::wavetableFront` (touched in `splitConstRegisters`) | no | high | matches barrier-clone copying | keep current (high) | — |

## 3. Detailed findings

### `OptFlag` (enum) and `AsmOptimize::flags_`  [yes / medium / —]

Evidence:
- `asm_optimize.hpp:38-44`:
  ```cpp
  enum OptFlag : uint32_t {
      Opt_JumpElim     = 0x01,
      Opt_LabelCleanup = 0x02,
      Opt_DeadCode     = 0x04,
      Opt_MergeZero    = 0x08,
      Opt_RegAlloc     = 0x10,
  };
  ```
- `asm_optimize.cpp:233`  `if (flags_ & 0x04) { deadCodeElimination(); }`
- `asm_optimize.cpp:249-265`  `if (flags_ & 0x01)` … `if (flags_ & 0x02)` …
  `if (flags_ & 0x08)` … `if (flags_ & 0x10)` — five bits each gating
  one optimization pass.
- `asm_optimize.cpp:535-536`  `if (!(flags_ & 0x08))` — same field
  re-checked inside `removeUnusedRegs` to gate the write-only analysis.
- `asm_optimize.cpp:661,676`  `if (flags_) { it->assembler.cmd =
  Assembler::INVALID; }` — same field also used as a generic non-zero
  truthy test in `reportUserMessages`, which conflates "any pass
  enabled" with "should I delete this MESSAGE/ERROR_MSG".

Interpretation:
- `OptFlag` is a bitmask of optimization-pass-enable bits. The naming
  pattern `Opt_<Pass>` is awkward (mixes scoping concerns into the
  enumerator names) but the meaning of each bit is unambiguous.
- The field `flags_` and the parameter `flags` faithfully store such
  a bitmask, but the name has nothing to say about what kind of flags;
  inside `reportUserMessages` it is even tested as a plain truthy
  value — which is opaque without knowing the flag layout.

Judgement:
- `OptFlag` and `flags_`: the names are too generic for a bitmask
  whose bits gate optimizer passes. Misnomer-by-vagueness, not
  by lying. Medium confidence.

Proposals:
- `OptFlag` → `OptPassFlag`  (medium)
- `OptFlag` → `OptimizationPass`  (medium)
- `flags_`  → `optFlags_` / `passFlags_`  (medium)
- keep current  (low)

Locations consulted:
- declared:  include/zhinst/asm_optimize.hpp:38-44, 183
- used:      src/asm_optimize.cpp:47, 233, 249-280, 535, 661, 676

### `RegAction::RegAction_NotFound..RegAction_ReadWritten`  [yes / medium / —]

Evidence:
- `asm_optimize.hpp:47-52`:
  ```cpp
  enum RegAction : int {
      RegAction_NotFound    = 0,
      RegAction_Read        = 1,
      RegAction_Written     = 2,
      RegAction_ReadWritten = 3,
  };
  ```
- The enumerators are not actually used anywhere in the body — the
  body of `getNextActionForReg` returns raw `0/1/2/3` literals
  (`asm_optimize.cpp:144,160,167,174,177,180`).

Interpretation:
- The enumerator name redundantly repeats the enum name as a prefix
  (`RegAction::RegAction_*`), which is the Old-school C-style
  workaround for unscoped enums and is undesirable on a
  non-`enum class` type that already namespaces its members behind
  the type tag in the report (and could/should be `enum class`).

Judgement:
- The enumerator names are misleading by redundancy. The enum *type*
  name is fine.

Proposals:
- `enum class RegAction { None, Read, Written, Both }`  (medium)
- keep current  (low)

Locations consulted:
- declared:  include/zhinst/asm_optimize.hpp:47-52
- used:      no symbolic uses in src/asm_optimize.cpp (raw integers
  used instead at 144, 160, 167, 174, 177, 180).

### `AsmOptimize::cancel_`  [unsure / low / —]

Evidence:
- `asm_optimize.hpp:189`  `std::shared_ptr<CancelCallback> cancel_;`
- `asm_optimize.cpp:506,977`  `auto cancelObj = cancel_;` (copies
  shared_ptr).
- `asm_optimize.cpp:513-517,998-1000`  branch-comment placeholder for
  the binary's `isCancelled()` virtual call — never spelled out as a
  real call site in the recon.

Interpretation:
- The field stores a callback object; `cancel_` reads naturally as
  "the cancel callback handle", but there is no in-batch evidence
  proving the type's class name (`CancelCallback`) ever appears in
  the binary. Without that anchor, both the name `cancel_` and a
  longer `cancelCallback_` are equally defensible.

Judgement:
- Unsure. The name is not actively misleading.

Proposals:
- keep current  (medium)
- `cancelCallback_`  (low)

Locations consulted:
- declared:  include/zhinst/asm_optimize.hpp:189
- used:      src/asm_optimize.cpp:506, 977

### `AsmOptimize::isLabelCalled` (method) and `::isLabelCalled::it` (param)  [unsure / low / —]

Evidence:
- `asm_optimize.hpp:114-115`  comment: "Check if any branch/jump
  instruction **after** 'it' references the given label".
- `asm_optimize.cpp:121`  body: `for (auto pos = asm_.cbegin(); pos
  != it; ++pos)` — the loop runs **from begin to it**, i.e. *before*
  it, the opposite of the header's comment.
- Single non-trivial caller: `deadCodeElimination` at
  `asm_optimize.cpp:305`  `if (!isLabelCalled(it->assembler.label,
  it))` — passing the iterator pointing at the LABEL instruction
  whose reachability we want to test.

Interpretation:
- The header comment in this very file is wrong about direction
  (`§4a` lists "comment describes a meaning that conflicts with what
  the name suggests" as a flag-worthy observation).
- The method name `isLabelCalled` does not describe the asymmetric
  "before `it`" behaviour. The parameter `it` doesn't say "upper
  bound for the scan".

Judgement:
- The method name is incomplete (silent on the directionality). The
  parameter `it` is a generic iterator name for what is actually a
  scan upper-bound. Low confidence — the asymmetry is not screaming
  loud, but the conflict between the header comment and the body is.

Proposals:
- `isLabelCalled` → `isLabelCalledBefore`  (low)
- `isLabelCalled::it` → `upTo`  (low)
- keep current  (medium) — and instead fix the comment

Locations consulted:
- declared:  include/zhinst/asm_optimize.hpp:114-115
- defined:   src/asm_optimize.cpp:119-131
- used:      src/asm_optimize.cpp:305 (deadCodeElimination)

### `AsmOptimize::removeUnusedRegs::writeOnlyRegs` (local)  [yes / medium / —]

Evidence:
- `asm_optimize.cpp:509`  declaration:
  `std::vector<AsmRegister> writeOnlyRegs;`.
- `asm_optimize.cpp:561-573`  every dest register encountered is
  checked against this vector and pushed in **before** any use-check
  determines whether it is actually write-only.
- `asm_optimize.cpp:619-631`  the *actual* "this register is never
  read" decision is in the inner forward scan, after which only
  selected entries get `*destSlot = AsmRegister(-1)` and the
  instruction is killed.

Interpretation:
- `writeOnlyRegs` is filled with **all candidate dest registers
  encountered**, regardless of whether they turn out write-only. It
  is a "have I already analyzed this register?" workset, not a list
  of write-only registers. The name lies about its contents.

Judgement:
- Misnomer: this is a deduplication set of analysed dest registers,
  not a set of write-only registers.

Proposals:
- `analyzedDestRegs`   (medium)
- `seenDestRegs`       (medium)
- keep current         (low)

Locations consulted:
- declared:  src/asm_optimize.cpp:509
- used:      src/asm_optimize.cpp:561-573

### `AsmOptimize::splitConstRegisters::needsSplit` (local)  [yes / low / —]

Evidence:
- `asm_optimize.cpp:1252`  `bool needsSplit = false;`
- `asm_optimize.cpp:1253-1280`  the inner scan loop never assigns
  `needsSplit`. The variable is never read either; the
  split-or-not decision uses `canSplit` and `aborted` instead
  (lines 1285-1320).

Interpretation:
- A dead local with a name suggesting a control role it does not
  actually play. The reconstruction may have lost the flag's
  assignment, or the flag was always dead.

Judgement:
- Misnomer in the weak sense (the name claims a role; the variable
  has none). Most cleanly fixed by removing the variable rather
  than renaming.

Proposals:
- delete the variable           (medium)
- keep current                  (low)

Locations consulted:
- src/asm_optimize.cpp:1252-1320

### `AsmList::Asm::isWaveformCmd` (cross-batch use here)  [yes / high / cross-batch-arbitration]

Evidence:
- `asm_optimize.cpp:343` (oneStepJumpElimination):
  `if (it->isWaveformCmd) continue;` — guards the rewrite of
  branch/jump opcodes (BRZ/BRNZ/BRGZ/JMP). The intent is "this
  instruction must not be optimized" — i.e. it is a *no-opt* flag.
- `asm_optimize.cpp:454` (mergeRegisterZeroing):
  `if (it->isWaveformCmd) continue;` — same guard before merging
  ADDI+ADDIU.
- `asm_optimize.cpp:1148-1149,1197,1210,1222` — barrier entries
  produced by `splitConstRegisters` are constructed with
  `barrier.isWaveformCmd = false` and originals with
  `orig.isWaveformCmd = (cmd-3) < 3u` (i.e. true for opcodes
  MESSAGE / 4 / ERROR_MSG — exactly the opcodes
  `Assembler::isWaveformCmd(instr)` (free function in batch 26)
  flags). These opcodes are **not** waveform commands; the
  predicate is the same misnamed predicate flagged in batches 26
  and 10 / 49.
- batch 26 (`26_assembler.md:502-563`) records the field
  `Asm::isWaveformCmd` and the predicate `isWaveformCmd(instr)` as
  "yes / high / cross-batch-arbitration", with proposed renames
  `isControlOpcode` / `isMessageOpcode` / `noOpt`.

Interpretation:
- Inside `AsmOptimize`, the field is consumed exclusively as a
  "skip optimization on this instruction" gate
  (`asm_optimize.cpp:343,454`). This is a strong second-batch
  confirmation of the semantic-inversion concern from batch 26:
  the field is true on MESSAGE/4/ERROR_MSG (which are not
  waveform commands) and is read as "do not transform".

Judgement:
- The cross-batch finding stands and is reinforced. The name
  `isWaveformCmd` lies about both what sets it and what the
  optimizer does with it.

Proposals (carry forward; final form belongs to batch 26 + asm_list):
- `isWaveformCmd` (field) → `noOpt` / `skipOptimization`   (high)
- `isWaveformCmd` (free fn in `assembler.hpp`) → `isControlOpcode`
  / `isMessageOpcode`                                       (high)
- keep current                                              (low)

Cross-reference:
- Batch 26 (`26_assembler.md`, free fn `isWaveformCmd` and field
  `Asm::isWaveformCmd`).
- Batch 10 (`10_asm_commands.md`, `AsmCommands::br/brz/brnz/brgz::flag`
  → `isWaveformCmd` proposal — needs re-evaluation if the field
  itself is renamed to `noOpt`).
- Batch 49 (`AsmCommandsImpl::brz::flag` arbitration counterpart).
- Batch 44 / asm_list (the field's owning header).

Locations consulted:
- src/asm_optimize.cpp:343, 454, 1148-1149, 1197, 1210, 1222
- include/zhinst/asm_list.hpp:61, 88-92
- (cross-batch) include/zhinst/assembler.hpp `isWaveformCmd` inline.

### `AsmList::Asm::lineNumber()` (cross-batch use here)  [no / high / not-misnomer]

Evidence:
- `asm_list.hpp:71-72`  accessor returning the same `int&` as
  `wavetableFront` — header explicitly documents the dual-purpose
  storage.
- `asm_optimize.cpp:653,669`:
  ```cpp
  int lineNr = it->lineNumber();   // for ERROR_MSG and MESSAGE
  errorCallback_(msg, lineNr);     // / warningCallback_(msg, lineNr);
  ```
- The compiler warning-/error-callback signature is `void(const
  std::string&, int)`. The `int` is consistently named `lineNr`
  here, and `lineNumber()` is the only path that produces it for
  user diagnostics.

Interpretation:
- The accessor name precisely describes the role of the storage
  when read by `reportUserMessages`. Recording as positive
  evidence in case anyone later "fixes" it to match
  `wavetableFront`.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared:  include/zhinst/asm_list.hpp:71-72
- used:      src/asm_optimize.cpp:653, 669

## 4. Symbols inspected and judged routinely fine

- `OptimizeException` (type), `OptimizeException::message_` (field),
  ctor param `msg`, `what()` body — all match the `nm`-anchored type
  and conventional STL-exception idiom.
- `AsmOptimize` (type) — in nm; tier-1 excluded.
- All method names (`prepareResources`, `optimizePreWaveform`,
  `optimizePostWaveform`, `isRead`, `isWritten`, `isLabelCalled`,
  `getNextActionForReg`, `registerIsNeverWritten`,
  `deadCodeElimination`, `oneStepJumpElimination`,
  `removeUnusedLabels`, `mergeLabels`, `mergeRegisterZeroing`,
  `removeUnusedRegs`, `reportUserMessages`, `simplifyAssign`,
  `registerAllocation`, `splitConstRegisters`, `splitReg`,
  `registerUpdate`) — in nm; tier-1 excluded.
- `numPhysicalRegs_`, `pad04_`, `pad0C_`, `errorCallback_`,
  `warningCallback_` — names match consumption.
- `asm_` (the working list field) — used only as the optimizer's
  working buffer; one-character name is brief but accurate.
- `AsmOptimize::AsmOptimize` parameters (`errorCallback`,
  `warningCallback`, `numPhysicalRegs`, `flags`, `cancel`) — all
  passed through to the like-named member; only `flags`/`flags_`
  carries the "too generic" critique already lodged above.
- `prepareResources::asmList`, `optimizePreWaveform::input`,
  `optimizePostWaveform::input` — descriptive.
- `isRead::instr,reg`, `isWritten::instr,reg`,
  `getNextActionForReg::it,reg`,
  `registerIsNeverWritten::list,reg,start,exclude` — match bodies.
- `deadCodeElimination::afterBranch` — exact role.
- All locals in `oneStepJumpElimination`, `removeUnusedLabels`,
  `mergeLabels`, `mergeRegisterZeroing`, `simplifyAssign` —
  descriptive (`targetLabel`, `firstLabel`/`secondLabel`,
  `prev`/`it`, `next`, `canSimplify`, `destReg`, `zero`, `scan`,
  `found`, …).
- `removeUnusedRegs` locals other than `writeOnlyRegs` —
  `cancelObj`, `maxReg`, `regInfo`, `regNum`, `cmdType`,
  `cmdPlus1`, `destSlot`, `destReg`, `usageFlags`, `scanIt`,
  `alreadyTracked`, `scanCmd`, `prev` — all descriptive.
- `reportUserMessages` locals (`imm`, `msg`, `lineNr`) — match the
  `int(const std::string&, int)` callback contract.
- `registerAllocation` locals — `numSlots`, `liveRanges`,
  `addToLiveRange`, `instrIdx`, `labelMap`, `branchRanges`,
  `BranchRange::targetIdx`/`sourceIdx`, `vregReg`, `conflict`,
  `physRegs`, `conflicts`, `cancelLock`, `cancelObj`,
  `numPhysical`, `totalPhysical`, `physVreg`, `physPreg`, `vreg`,
  `preg`, `canMerge`, `insertPos`, `minPreg`, `maxVreg`,
  `maxPregFirst`, `minVreg` — all descriptive of a graph-coloring
  allocator. The lambda `addToLiveRange` is correctly named for
  what it does. (`registerAllocation::$_0::reg`,`instrIdx`
  similarly.)
- `LiveRange` and `PhysicalRegister` — encoded in nm via
  `__split_buffer<...::LiveRange,...>` and
  `vector<...::PhysicalRegister>`; tier-1 excluded.
- `splitConstRegisters` locals other than `needsSplit` —
  `tmpList`, `magicReg`, `newSeqId`, `barrier`, `orig`,
  `isSkipCmd`, `destReg`, `scanEnd`, `scanIt`, `canSplit`,
  `splitEnd`, `aborted`, `checkIt`, `chkCmdType`, `splitCount`,
  `entry`, `oldEnd`, `dit` — all descriptive.
- `splitReg` locals — `startOff`/`endOff` (vector-reallocation-
  safe offsets), `didSplit`, `allSplitOk`, `instrCount`,
  `newReg`/`newRegNum`, `cmdType`, `usesReg`, `ct2`, `startAsm`,
  `endAsm`, `instr` — all match body.
- `registerUpdate::indices,oldReg,newReg,idx,instr,it` —
  conventional; matches body.

## 5. Coverage

- **Fully covered:** All public and private methods of `AsmOptimize`,
  the nested `OptimizeException` type, the file-local enums
  `OptFlag` and `RegAction`, every parameter of every signature in
  `asm_optimize.hpp`, every reachable local in `asm_optimize.cpp`,
  and the data members at +0x00..+0x90.
- **Deferred:** None within the file pair.
- **Not covered (out of scope per RULES §2/§3):**
  - All method names of `zhinst::AsmOptimize` and
    `zhinst::OptimizeException` — present verbatim in
    `nm --demangle _seqc_compiler.so` (e.g.
    `zhinst::AsmOptimize::registerAllocation(unsigned long)` at
    `0x27ebb0`, `zhinst::OptimizeException::what() const` at
    `0x281e90`).
  - The class names `AsmOptimize` and `OptimizeException` — both
    encoded by mangled symbols above (tier-1 §3).
  - The nested type aliases `LiveRange` and `PhysicalRegister` —
    encoded as nested type names in mangled symbols of
    `__split_buffer<...::LiveRange,...>` (`0x281d80`) and
    `std::vector<...::PhysicalRegister, …>` (`0x281840`); tier-1.
  - `registerAllocation::$_0` (lambda) — name is a compiler-mangled
    artefact, not a project-defined identifier.
- **Cross-batch deferred:** the field `AsmList::Asm::isWaveformCmd`
  (declared in `include/zhinst/asm_list.hpp:61`, batch 44) and the
  free predicate `isWaveformCmd(const AssemblerInstr&)` (batch 26)
  are flagged for arbitration; both are observed and reinforced
  here but final renaming belongs to those batches' arbitration.
