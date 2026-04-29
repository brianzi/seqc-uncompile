# Batch 04c — ast_evaluate arithmetic & value evaluate() methods

Scope: `reconstructed/src/seqc_ast_nodes_evaluate.cpp` lines **2611–4690**
only. The file's anonymous-namespace helpers used by these methods
(`getBackReg`, `rhsCount`, `rhsTypeOrUnset`, `rhsSubOrDefault`) are
declared at lines 156–179, but their definitions are out-of-range; this
batch covers them only insofar as they are referenced from the in-range
methods (their *parameters* `er`, `rhs` were inspected in passing).

Per RULES §3, every `Class::evaluate` method name listed below is
present verbatim in the binary symbol table:

```
$ nm --demangle _seqc_compiler.so | grep '::evaluate('
0x209dc0  zhinst::SeqCAstNode::evaluate(...)
0x209db0  zhinst::SeqCCommand::evaluate(...)
0x209d10  zhinst::SeqCVariableType::evaluate(...)
0x209e50  zhinst::SeqCOperation::evaluate(...)
0x209ea0  zhinst::SeqCVariable::evaluate(...)
0x210a90  zhinst::SeqCOperator::evaluate(... 5-arg ...)
0x210aa0  zhinst::SeqCOperator::evaluate(... 3-arg ...)
0x211050  zhinst::SeqCParamList::evaluate(...)
0x213140  zhinst::SeqCValue::evaluate(...)
0x2130d0  zhinst::SeqCLabel::evaluate(...)
0x226890  zhinst::SeqCContinueStatement::evaluate(...)
0x226970  zhinst::SeqCBreakStatement::evaluate(...)
0x228e80  zhinst::SeqCPos::evaluate(...)
0x22a560  zhinst::SeqCNoCmd::evaluate(...)
0x22a600  zhinst::SeqCPlus::evaluate(...)
0x22cde0  zhinst::SeqCMinus::evaluate(...)
0x22ea70  zhinst::SeqCMult::evaluate(...)
0x231070  zhinst::SeqCDiv::evaluate(...)
0x2427b0  zhinst::SeqCXorExpr::evaluate(...)
0x243e60  zhinst::SeqCAssign::evaluate(...)
0x246270  zhinst::SeqCNoOp::evaluate(...)
```

All are excluded from rename. In-scope here: their **parameters** and
**body locals**.

## 1. Files considered

- `reconstructed/src/seqc_ast_nodes_evaluate.cpp` lines 2611–4690.
- Header context (declarations only, not in-scope for this batch):
  `reconstructed/include/zhinst/seqc_ast_node.hpp` (param names in
  header agree with .cpp: `res`, `ctx`, `state`, `lhsResult`,
  `rhsResult`).
- Cross-batch context only (read, not edited):
  - `notes/symbol-renaming-audit/11_value.md` (Value)
  - `notes/symbol-renaming-audit/12_eval_result_value.md` (EvalResultValue)
  - `notes/symbol-renaming-audit/34_eval_results.md` (EvalResults)
  - `notes/symbol-renaming-audit/32_frontend_lowering.md`
    (FrontendLoweringContext / FrontendLoweringState)
  - `notes/symbol-renaming-audit/06_asm_register.md` (AsmRegister)
  - `notes/symbol-renaming-audit/10_asm_commands.md` (AsmCommands)
  - `notes/symbol-renaming-audit/42_expression.md`

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| **SeqCAstNode::evaluate (base) — params** | | | | | |
| `SeqCAstNode::evaluate::res` | no | high | conventional Resources alias | — | — |
| `SeqCAstNode::evaluate::ctx` | no | high | matches FrontendLoweringContext | — | — |
| `SeqCAstNode::evaluate::state` | no | high | matches FrontendLoweringState | — | — |
| **SeqCOperator::evaluate (5-arg base) — params** | | | | | |
| `SeqCOperator::evaluate(5-arg)::lhsResult` | no | high | unused in base | — | — |
| `SeqCOperator::evaluate(5-arg)::rhsResult` | no | high | unused in base | — | — |
| **SeqCOperator::evaluate (3-arg template) — params + locals** | | | | | |
| `SeqCOperator::evaluate(3-arg)::res` | no | high | passed through unchanged | — | — |
| `SeqCOperator::evaluate(3-arg)::ctx` | no | high | used as context | — | — |
| `SeqCOperator::evaluate(3-arg)::state` | no | high | used as state | — | — |
| `SeqCOperator::evaluate(3-arg)::lineNr` | no | high | copied from `lineNr_` | — | — |
| `SeqCOperator::evaluate(3-arg)::result` | no | high | the EvalResults built | — | — |
| `SeqCOperator::evaluate(3-arg)::lhsResult` | no | high | child eval result | — | — |
| `SeqCOperator::evaluate(3-arg)::rhsResult` | no | high | child eval result | — | — |
| `SeqCOperator::evaluate(3-arg)::lhsType` | no | high | VarType extracted | — | — |
| `SeqCOperator::evaluate(3-arg)::rhsType` | no | high | VarType extracted | — | — |
| `SeqCOperator::evaluate(3-arg)::combined` | no | medium | result of combine() | — | — |
| `SeqCOperator::evaluate(3-arg)::lv` / `::rv` | unsure | low | terse ref aliases | `lhsValues`, `rhsValues` (low); keep current (medium) | — |
| `SeqCOperator::evaluate(3-arg)::opResult` | no | medium | virtual call result | — | — |
| **SeqCValue::evaluate — params + locals** | | | | | |
| `SeqCValue::evaluate::ctx` | no | high | conventional | — | — |
| `SeqCValue::evaluate::lineNr` | no | high | copied from lineNr_ | — | — |
| `SeqCValue::evaluate::result` | no | high | EvalResults built | — | — |
| `SeqCValue::evaluate::t` | unsure | low | one-letter; switch tag | `tag` (medium); keep current (low) | — |
| `SeqCValue::evaluate::val` | no | high | the double payload | — | — |
| `SeqCValue::evaluate::oss` | no | high | conventional ostringstream | — | — |
| `SeqCValue::evaluate::str` | no | medium | the string payload | — | — |
| `SeqCValue::evaluate::first` | no | high | first character | — | — |
| **SeqCVariable::evaluate — params + locals** | | | | | |
| `SeqCVariable::evaluate::res` | no | high | the Resources object | — | — |
| `SeqCVariable::evaluate::ctx` | no | high | conventional | — | — |
| `SeqCVariable::evaluate::lineNr` | no | high | from lineNr_ | — | — |
| `SeqCVariable::evaluate::name` | no | high | local copy of name_ | — | — |
| `SeqCVariable::evaluate::result` | no | high | EvalResults built | — | — |
| `SeqCVariable::evaluate::direction` | no | high | abs(direction_) used as EDirection | — | — |
| `SeqCVariable::evaluate::pushReadResult` | no | high | lambda; describes action | — | — |
| `SeqCVariable::evaluate::resolved` | no | high | resolved VarType from scope | — | — |
| `SeqCVariable::evaluate::rv` | no | medium | EvalResultValue from read* | — | — |
| `SeqCVariable::evaluate::reg` | no | high | register int from getRegister | — | — |
| `SeqCVariable::evaluate::msg` | no | high | error message string | — | — |
| `SeqCVariable::evaluate::wf` | no | high | shared_ptr<WaveformFront> | — | — |
| `SeqCVariable::evaluate::wfName` | no | high | the waveform's name | — | — |
| **SeqCAssign::evaluate — params + locals** | | | | | |
| `SeqCAssign::evaluate::res` | no | high | passed to update*() | — | — |
| `SeqCAssign::evaluate::ctx` | no | high | used widely | — | — |
| `SeqCAssign::evaluate::state` | no | high | unused but conventional | — | — |
| `SeqCAssign::evaluate::lhsResult` | no | high | the LHS eval result | — | — |
| `SeqCAssign::evaluate::rhsResult` | no | high | the RHS eval result | — | — |
| `SeqCAssign::evaluate::result` | no | high | the EvalResults returned | — | — |
| `SeqCAssign::evaluate::aux` | yes | medium | "aux" hides specific role | `effectiveLhs`, `lhsView` (medium); keep current (low) | — |
| `SeqCAssign::evaluate::lhsVar` | no | high | dynamic_cast<SeqCVariable*> of lhs | — | — |
| `SeqCAssign::evaluate::lhsType`/`lhsSub` | no | high | extracted from aux back | — | — |
| `SeqCAssign::evaluate::rhsIsConstOrCvar` | no | high | predicate, accurate | — | — |
| `SeqCAssign::evaluate::rhsType`/`rhsSub` | no | high | from rhs values back | — | — |
| `SeqCAssign::evaluate::name` | no | high | the LHS variable's name | — | — |
| `SeqCAssign::evaluate::lhsReg`/`rhsReg`/`zeroReg` | no | high | AsmRegisters | — | — |
| `SeqCAssign::evaluate::rhsVal` | no | high | the RHS Value | — | — |
| `SeqCAssign::evaluate::addiAsms` | no | medium | addi instruction list | — | — |
| `SeqCAssign::evaluate::placeholder` | no | high | asmSetVarPlaceholder asm | — | — |
| `SeqCAssign::evaluate::arr` | no | high | dynamic_cast<SeqCArray*> | — | — |
| `SeqCAssign::evaluate::arrVar` | no | high | dynamic_cast inside arr | — | — |
| `SeqCAssign::evaluate::arrName` | no | high | the array name | — | — |
| `SeqCAssign::evaluate::wf` | no | high | the waveform front | — | — |
| `SeqCAssign::evaluate::wfName` | no | high | waveform's name | — | — |
| `SeqCAssign::evaluate::idx` | no | high | sample index | — | — |
| `SeqCAssign::evaluate::val` | no | high | sample value (double) | — | — |
| `SeqCAssign::evaluate::sp`/`mp` | unsure | low | very terse pointers | `samplePtr`, `markerPtr` (medium); keep current (low) | — |
| `SeqCAssign::evaluate::rhsName` | no | high | name from rhs.toString | — | — |
| `SeqCAssign::evaluate::rt`/`rs` | yes | low | bare 2-letter names in catch | `rhsType`, `rhsSub` (medium); keep current (low) | — |
| `SeqCAssign::evaluate::e` (catch param) | no | high | conventional exception alias | — | — |
| **SeqCPlus::evaluate — locals** | | | | | |
| `SeqCPlus::evaluate::ctx` | no | high | conventional | — | — |
| `SeqCPlus::evaluate::lhsResult`/`rhsResult` | no | high | matches header | — | — |
| `SeqCPlus::evaluate::result` | no | high | the EvalResults built | — | — |
| `SeqCPlus::evaluate::lhsCount` | no | high | size of lhs values | — | — |
| `SeqCPlus::evaluate::lhsType`/`lhsSub` | no | high | extracted | — | — |
| `SeqCPlus::evaluate::lhsT`/`rhsT` | unsure | low | 1-letter names in error path | `lhsType`, `rhsType` (low); keep current (medium, scope-limited) | — |
| `SeqCPlus::evaluate::regNum` | no | high | register number int | — | — |
| `SeqCPlus::evaluate::resultReg`/`lhsReg`/`rhsReg` | no | high | AsmRegisters | — | — |
| `SeqCPlus::evaluate::rhsVal`/`lhsVal` | no | high | Value or double extract | — | — |
| `SeqCPlus::evaluate::asmOut`/`copyAsm`/`addAsm` | no | high | asm instructions | — | — |
| `SeqCPlus::evaluate::combinedType`/`combinedSub` | no | high | output of combine() | — | — |
| `SeqCPlus::evaluate::sum` | no | high | lhsVal + rhsVal | — | — |
| `SeqCPlus::evaluate::lhsStr`/`rhsStr` | no | high | string versions | — | — |
| `SeqCPlus::evaluate::wfName` | no | high | waveform name | — | — |
| `SeqCPlus::evaluate::length` | no | high | sample length | — | — |
| `SeqCPlus::evaluate::constVal` | no | high | const value double | — | — |
| `SeqCPlus::evaluate::constWf` | no | high | rectangular waveform | — | — |
| **SeqCMinus::evaluate — locals** | | | | | |
| (analogues of SeqCPlus) | no | high | same patterns, names | — | — |
| `SeqCMinus::evaluate::lhsT`/`rhsT` | unsure | low | as SeqCPlus | as SeqCPlus | — |
| `SeqCMinus::evaluate::rhsInt` | no | high | int extract from RHS | — | — |
| `SeqCMinus::evaluate::loadAsm`/`subAsm` | no | high | asm wrappers | — | — |
| `SeqCMinus::evaluate::diff` | no | high | lhsVal - rhsVal | — | — |
| `SeqCMinus::evaluate::rhsCopy`/`scaledRhs` | no | high | shared_ptr copies | — | — |
| **SeqCMult::evaluate — locals** | | | | | |
| (analogues of SeqCPlus) | no | high | same patterns, names | — | — |
| `SeqCMult::evaluate::lhsCopy`/`rhsCopy` | no | high | shared_ptr copies | — | — |
| `SeqCMult::evaluate::product` | no | high | lhsVal * rhsVal | — | — |
| **SeqCDiv::evaluate — locals** | | | | | |
| `SeqCDiv::evaluate::lhsCount`/`lhsType` | no | high | size and type | — | — |
| `SeqCDiv::evaluate::lhsT`/`rhsT` | unsure | low | as SeqCPlus | as SeqCPlus | — |
| `SeqCDiv::evaluate::rhsCheck` | yes | low | name says "check"; just rhs as double | `rhsDouble` (low); merge with `rhsDouble` below (medium); keep current (low) | — |
| `SeqCDiv::evaluate::rhsDouble` | no | high | RHS extracted as double | — | — |
| `SeqCDiv::evaluate::reciprocal` | no | high | 1/rhsDouble | — | — |
| `SeqCDiv::evaluate::scalarCopy`/`waveCopy` | no | high | shared_ptr copies | — | — |
| `SeqCDiv::evaluate::quotient` | no | high | lhsVal / rhsVal | — | — |
| `SeqCDiv::evaluate::combinedType`/`combinedSub` | no | high | from combine() | — | — |
| **Trivial-bodies cluster (SeqCCommand, SeqCOperation, SeqCLabel, SeqCXorExpr, SeqCNoOp, SeqCVariableType, SeqCNoCmd, SeqCPos, SeqCContinueStatement, SeqCBreakStatement, SeqCParamList) — params + locals** | | | | | |
| All `res`/`ctx`/`state`/`lhsResult`/`rhsResult` params | no | high | conventional, mostly unused | — | — |
| `SeqC{VariableType,NoCmd,Pos,ContinueStatement,BreakStatement}::evaluate::lineNr` | no | high | copied from lineNr_ | — | — |

## 3. Detailed findings

### SeqCAssign::evaluate::aux  [yes / medium / —]

Evidence:
- src/seqc_ast_nodes_evaluate.cpp:3092
  `auto aux = std::make_shared<EvalResults>(lhsResult);`
- src/seqc_ast_nodes_evaluate.cpp:3094–3102 (block comment) explains:
  > "Allocate `aux = make_shared<EvalResults>(lhsResult)` — a copy of
  > the LHS, used internally so the dispatch can read lhs varType info
  > safely and so arrayBacking_/waveformFront_ propagation can happen
  > without mutating the caller's lhsResult."
- src/seqc_ast_nodes_evaluate.cpp:3100–3102
  `if (lhsResult.arrayBacking_) { aux = lhsResult.arrayBacking_; }`
  — `aux` is then re-pointed at `arrayBacking_` for indexed
  assignments, so it is conceptually "the effective LHS being
  assigned to", which is `lhsResult` for non-array cases and the
  inner indexed result for arrays.
- All subsequent reads (`aux->values_.back().varType_`,
  `aux->values_.back().reg_`, `aux->getValue().toInt()`,
  `aux->waveformFront_`) treat it as the canonical LHS view.

Interpretation:
- The local plays a single specific role: the LHS view that the
  dispatch works against. `aux` is the generic catch-all term for
  "auxiliary" and does not communicate that role; the source comment
  explicitly has to spell it out.

Judgement:
- The name `aux` is non-descript for the role this local actually
  plays (the effective LHS view).

Proposals:
- `effectiveLhs`  (medium)
- `lhsView`       (medium)
- `lhsForDispatch`(low)
- keep current    (low)

Locations consulted:
- declared: src/seqc_ast_nodes_evaluate.cpp:3092
- used:     src/seqc_ast_nodes_evaluate.cpp:3101, 3115, 3118–3119,
            3164, 3188, 3291, 3295

### SeqCAssign::evaluate::rt  /  ::rs  [yes / low / —]

Evidence:
- src/seqc_ast_nodes_evaluate.cpp:3385–3393  (catch handler):
  ```
  VarType    rt = rhsResult.values_.empty() || ... ? Unset : back().varType_;
  VarSubType rs = rhsResult.values_.empty() || ... ? Default: back().varSubType_;
  result->setValue(rt, rs, rhsResult.getValue());
  ```
- The same data is computed in normal flow as `rhsType` and `rhsSub`
  (line 3138–3139). The catch path uses different identifiers for the
  same concept.

Interpretation:
- `rt`/`rs` are 2-letter names that don't say "rhs" or "type/sub" and
  diverge from the established `lhsType/lhsSub`/`rhsType/rhsSub`
  vocabulary used elsewhere in the same function.

Judgement:
- The names are mildly misleading (or at least inconsistent) because
  the rest of the function uses `rhsType`/`rhsSub` for exactly this
  pair of values.

Proposals:
- `rhsType` / `rhsSub`            (medium)
- keep current                    (low)

Note: rename would also align with the file-scope helpers
`rhsTypeOrUnset` / `rhsSubOrDefault`.

Locations consulted:
- declared: src/seqc_ast_nodes_evaluate.cpp:3385, 3389
- used:     src/seqc_ast_nodes_evaluate.cpp:3393

### SeqCAssign::evaluate::sp  /  ::mp  [unsure / low / —]

Evidence:
- src/seqc_ast_nodes_evaluate.cpp:3303–3306
  ```
  auto* sp = wf->signal.samples_.data();
  auto* mp = wf->signal.markers_.data();
  if (sp) sp[idx] = val;
  if (mp) mp[idx] = 0;
  ```
- Names appear nowhere else in the function.

Interpretation:
- `sp` and `mp` are 2-letter names. In the very narrow scope (4
  lines) the meaning is clear from context (samples_/markers_), but
  outside any one-line read window the names give no hint.

Judgement:
- Borderline. Scope is tiny and adjacent to the named members, so
  not strictly misleading.

Proposals:
- `samplePtr` / `markerPtr`   (medium)
- keep current                (medium)

### SeqCDiv::evaluate::rhsCheck  [yes / low / —]

Evidence:
- src/seqc_ast_nodes_evaluate.cpp:4415–4417
  ```
  double rhsCheck = rhsResult.getValue().toDouble();   // @0x231751
  if (floatEqual(rhsCheck, 0.0)) { ... }
  ```
- src/seqc_ast_nodes_evaluate.cpp:4485–4487 (10 lines later, in the
  Wave÷Const path):
  ```
  double rhsDouble = rhsResult.getValue().toDouble();
  double reciprocal = 1.0 / rhsDouble;
  ```
- Both locals hold the same conceptual value (rhs as double), differ
  only by which row's zero-check / arithmetic uses them.

Interpretation:
- `rhsCheck` describes the *use* (a check) rather than the value
  itself. The cousin local `rhsDouble` ten lines down describes the
  type and is more consistent with the rest of the file.

Judgement:
- Mild misnomer: the value is the RHS as a double; whether it is
  used to check zero or to compute a reciprocal is incidental.

Proposals:
- `rhsDouble`        (low — would shadow nothing but parallels the
                          name on line 4485)
- keep current       (low)

Locations consulted:
- declared/used: src/seqc_ast_nodes_evaluate.cpp:4415, 4417, 4473, 4475

### SeqCOperator::evaluate(3-arg)::lv  /  ::rv  [unsure / low / —]

Evidence:
- src/seqc_ast_nodes_evaluate.cpp:2740–2747
  ```
  auto const& lv = lhsResult->values_;
  if (!lv.empty() && lv.size() <= 1) { lhsType = lv.back().varType_; }
  auto const& rv = rhsResult->values_;
  if (!rv.empty() && rv.size() <= 1) { rhsType = rv.back().varType_; }
  ```

Interpretation:
- `lv`/`rv` are 2-letter aliases for `lhsResult->values_` /
  `rhsResult->values_`. Used 4 times in a 6-line block. Not strongly
  misleading, but `lhsValues`/`rhsValues` would read identically and
  match `values_` member naming.

Judgement:
- Borderline. Tiny scope; not flagged as a clear misnomer.

Proposals:
- `lhsValues` / `rhsValues`   (low)
- keep current                (medium)

### SeqCValue::evaluate::t  [unsure / low / —]

Evidence:
- src/seqc_ast_nodes_evaluate.cpp:2798
  `Tag t = tag();`
- Used as the switch selector at 2799.

Interpretation:
- One-letter local for a Tag returned from `tag()`. Idiomatic in a
  3-line scope but not informative.

Judgement:
- Borderline. Suggesting `tag` would shadow the member function but
  that's a non-issue inside the body where the call has already
  returned.

Proposals:
- `tag`           (medium)
- keep current    (low)

## 4. Symbols inspected and judged routinely fine

- All `res`, `ctx`, `state` parameters across all 21 method bodies in
  range — names match header (`seqc_ast_node.hpp:117–289`) and the
  conventions established by all sibling evaluate() overrides.
- `lhsResult`/`rhsResult` parameters of every 5-arg `evaluate` —
  match the header (line 289–290, 318–319) and the role ("the result
  of evaluating each child") is unambiguous.
- All `lineNr` locals (SeqCOperator, SeqCValue, SeqCVariable,
  SeqCVariableType, SeqCNoCmd, SeqCPos, SeqCContinueStatement,
  SeqCBreakStatement) — copy of `lineNr_` member; consistent across
  the file.
- All `result` locals — every method allocates `result =
  make_shared<EvalResults>()` and returns it; uniformly used.
- `lhsType`/`rhsType`/`lhsSub`/`rhsSub` extracts in SeqCAssign,
  SeqCPlus, SeqCMinus, SeqCMult, SeqCDiv — consistent vocabulary,
  match the dispatch they drive.
- `lhsCount` in SeqCPlus/Minus/Mult/Div — straightforward
  `lhsResult.values_.size()`.
- `regNum`, `lhsReg`, `rhsReg`, `resultReg`, `zeroReg` — every
  AsmRegister local is named for which side it represents (see batch
  06_asm_register).
- `rhsVal`, `lhsVal`, `rhsInt`, `rhsDouble`, `constVal`, `sum`,
  `diff`, `product`, `quotient`, `reciprocal` — straightforward
  numeric extractions / arithmetic outputs, names match the operation.
- `lhsStr`/`rhsStr` (SeqCPlus String+String row) — `toString()`
  results.
- `length`, `wfName`, `constWf` (constWaveform path in
  SeqCPlus/Minus/Mult/Div) — match the `constWaveform()` helper
  signature in the same file.
- `combinedType`, `combinedSub` — outputs of `combine(VarType,...)`
  and `combine(VarSubType,...)`; match the helper names directly.
- `lhsCopy`, `rhsCopy`, `scalarCopy`, `waveCopy`, `scaledRhs` —
  shared_ptr copies created for handing off to `combineWaveforms`,
  `scaleWaveform`, or `computeMult`.
- `asmOut`, `addiAsms`, `loadAsm`, `copyAsm`, `subAsm`, `addAsm`,
  `placeholder` — asm-instruction return locals; each is named
  after the asm op it carries.
- SeqCAssign locals `lhsVar` (dynamic_cast to SeqCVariable*),
  `arr`/`arrVar`/`arrName`, `wf`/`wfName`, `idx`/`val`, `name`,
  `rhsName`, `rhsIsConstOrCvar` — all describe their role
  precisely.
- SeqCVariable locals `name`, `direction`, `pushReadResult`
  (lambda), `resolved`, `rv` (single-letter EvalResultValue from
  `read*()`), `reg`, `msg`, `wf`, `wfName` — all locally
  unambiguous.
- SeqCValue locals `val`, `oss`, `str`, `first` — match math /
  string-formatting use directly.
- All catch-block parameters `e` (SeqCAssign,
  SeqCOperator::evaluate-3) — conventional alias for an exception
  reference.
- Trivial-bodies cluster (`SeqCCommand`, `SeqCOperation`,
  `SeqCLabel`, `SeqCXorExpr`, `SeqCNoOp`, `SeqCVariableType`,
  `SeqCNoCmd`, `SeqCPos`, `SeqCContinueStatement`,
  `SeqCBreakStatement`, `SeqCParamList`): every method takes the
  standard `(res, ctx, state)` or `(res, ctx, state, lhsResult,
  rhsResult)` parameter list with no body locals worth a separate
  blockup (only `lineNr` in the setLineNr-preamble cluster, plus
  pass-through arguments).

## 5. Coverage

**Fully covered:**
- Every method body and parameter list in
  `seqc_ast_nodes_evaluate.cpp` lines 2611–4690, comprising:
  - `SeqCAstNode::evaluate` (3-arg base)            [2611–2617]
  - `SeqCOperator::evaluate` (5-arg base)           [2626–2634]
  - `SeqCOperator::evaluate` (3-arg template)       [2656–2764]
  - `SeqCValue::evaluate`                           [2783–2854]
  - `SeqCVariable::evaluate`                        [2905–3055]
  - `SeqCAssign::evaluate`                          [3073–3401]
  - `SeqCPlus::evaluate`                            [3429–3738]
  - `SeqCMinus::evaluate`                           [3771–4093]
  - `SeqCMult::evaluate`                            [4123–4298]
  - `SeqCDiv::evaluate`                             [4340–4528]
  - Trivial-bodies cluster                          [4536–4686]
- Cross-checked all 21 `Class::evaluate` symbols against
  `nm --demangle _seqc_compiler.so`; all are §3-excluded.

**Deferred:**
- Helper-function bodies in the anonymous namespace
  (`getBackReg`, `rhsCount`, `rhsTypeOrUnset`, `rhsSubOrDefault`)
  declared at lines 156–179 — out of this batch's line range; their
  parameter names (`er`, `rhs`) were inspected through reference
  only, no flag raised. Their full audit belongs to the batch
  covering lines 1–2610 (04b).

**Not covered (out of scope per RULES §2/§3):**
- All `Class::evaluate` method names — present in
  `nm --demangle _seqc_compiler.so` (table at top of report);
  excluded by §3.
- The header file `seqc_ast_node.hpp` — assigned to batch 04a;
  consulted here only for cross-check that param names in .cpp
  agree with .hpp declarations (they do).
- Lines 1–2610 (batch 04b) and 4691+ (batches 04d, 04e).
