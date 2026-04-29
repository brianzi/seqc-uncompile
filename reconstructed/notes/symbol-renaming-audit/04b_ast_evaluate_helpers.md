# Batch 04b — ast_evaluate_helpers

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 4 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 4;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

In scope:
- `reconstructed/src/seqc_ast_nodes_evaluate.cpp` lines **1–2610** only
  (everything before the first `SeqCAstNode::evaluate` body at line 2611).

Out-of-scope but consulted as context:
- `reconstructed/include/zhinst/resources.hpp` lines 110–141
  (`VarTypeException`, `combine` decls — referenced by in-scope defs).
- `reconstructed/include/zhinst/eval_results.hpp`
  (field-name reference for `values_`, `assemblers_`).
- `reconstructed/include/zhinst/frontend_lowering.hpp`
  (field-name reference for `messages`, `asmCommands`, `waveformGen`).
- `_seqc_compiler.so` symbol table (`nm --demangle`) — used to
  establish §3 exclusions for free-function and type names.

### Symbol-table verification (per §3)

`nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so`:

```
0x238fb0 t (anonymous namespace)::invertBool(...)
0x2149f0 t (anonymous namespace)::jumpIfZero(...)
0x22fdf0 t (anonymous namespace)::computeMult(...)
0x235ac0 t (anonymous namespace)::evalGreater(...)
0x242e40 t (anonymous namespace)::valueToBool(...)
0x22c9f0 t (anonymous namespace)::constWaveform(...)
0x228cc0 t (anonymous namespace)::scaleWaveform(int, ...)
0x2309e0 t (anonymous namespace)::scaleWaveform(shared_ptr, shared_ptr, ...)
0x22c300 t (anonymous namespace)::combineWaveforms(...)
0x21dcd0 t (anonymous namespace)::loopArgNodeAppend(...)
0x21dfa0 t (anonymous namespace)::loopBodyNodeAppend(...)
0x240a30 t (anonymous namespace)::evalOr(...)
0x23ea20 t (anonymous namespace)::evalAnd(...)
0x239be0 t (anonymous namespace)::evalEqual(...)
0x237440 t (anonymous namespace)::evalLower(...)
0x232850 t (anonymous namespace)::evalShift(...)
0x2ec050 t zhinst::floatEqual(double, double)
0x2480e0 t zhinst::VarTypeException::VarTypeException(...)
0x247ea0 t zhinst::combine(zhinst::VarSubType, zhinst::VarSubType)
0x247f50 t zhinst::combine(zhinst::VarType, zhinst::VarType)
0xb06600 d typeinfo for zhinst::VarTypeException
```

All free-function names (`combine`, `combineWaveforms`, `constWaveform`,
`scaleWaveform`, `computeMult`, `valueToBool`, `invertBool`, `jumpIfZero`,
`evalGreater`, `evalLower`, `evalEqual`, `evalShift`, `evalAnd`, `evalOr`,
`loopArgNodeAppend`, `loopBodyNodeAppend`, `floatEqual`), the class name
`VarTypeException`, and its method names (`VarTypeException`, `~VarTypeException`,
`what`) are **excluded** from rename per §3.  In-scope items are therefore
restricted to **parameter names**, **data members**, **local variables**,
**named constants**, and any **lambda-local helpers** within these
functions' bodies.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `VarTypeException::msg_` | no | medium | name fits exception-text member | — | not-misnomer |
| `VarTypeException::VarTypeException::msg` | no | low | matches member, std-style | — | — |
| `combine(VarType,VarType)::lhs` | no | low | left operand of binary op | — | — |
| `combine(VarType,VarType)::rhs` | no | low | right operand of binary op | — | — |
| `combine(VarType,VarType)::combineTable` (local) | no | low | descriptive lookup table | — | — |
| `combine(VarSubType,VarSubType)::lhs` | no | low | left operand | — | — |
| `combine(VarSubType,VarSubType)::rhs` | no | low | right operand | — | — |
| `combineWaveforms::opName` | no | medium | passed to WaveformGenerator::eval | — | not-misnomer |
| `combineWaveforms::lhs` | no | low | left waveform operand | — | — |
| `combineWaveforms::rhs` | no | low | right waveform operand | — | — |
| `combineWaveforms::ctx` | no | low | FrontendLoweringContext | — | — |
| `constWaveform::sampleLength` | no | medium | becomes Value(int) length-arg | — | not-misnomer |
| `constWaveform::value` | unsure | low | generic in math context | sampleAmplitude (low), keep current (medium) | — |
| `constWaveform::ctx` | no | low | standard | — | — |
| `scaleWaveform(int,sp,ctx)::scaleFactor` | yes | medium | param dead; func always negates | unusedScaleFactor (low), removeParam (low), keep current (low) | — |
| `scaleWaveform(int,sp,ctx)::wave` | no | low | the waveform operand | — | — |
| `scaleWaveform(int,sp,ctx)::ctx` | no | low | standard | — | — |
| `scaleWaveform(sp,sp,ctx)::scalar` | no | medium | scalar Const/Cvar operand | — | not-misnomer |
| `scaleWaveform(sp,sp,ctx)::wave` | no | medium | wave operand, push order respected | — | not-misnomer |
| `scaleWaveform(sp,sp,ctx)::ctx` | no | low | standard | — | — |
| `computeMult::varOperand` | no | medium | always Var-typed | — | not-misnomer |
| `computeMult::constOperand` | no | medium | always Const/Cvar-typed | — | not-misnomer |
| `computeMult::res` | unsure | low | unused per body | unusedRes (low), keep current (medium) | — |
| `computeMult::ctx` | no | low | passed-by-value FrontendLoweringContext | — | — |
| `evalGreater::lhs` / `::rhs` / `::res` / `::ctx` | no | low | standard lhs,rhs,res,ctx pattern | — | — |
| `evalLower::lhs` / `::rhs` / `::res` / `::ctx` | no | low | standard | — | — |
| `evalEqual::lhs` / `::rhs` / `::res` / `::ctx` | no | low | standard | — | — |
| `evalEqual::kRangeLo` (local constant) | yes | medium | obscures INT32_MAX | kInt32MaxAsDouble (medium), kImmediateMaxPositive (low) | — |
| `evalEqual::kRangeHi` (local constant) | yes | medium | obscures UINT32_MAX | kUint32MaxAsDouble (medium), kImmediateMaxUnsigned (low) | — |
| `evalShift::lhs` / `::rhs` / `::res` / `::ctx` | no | low | standard | — | — |
| `evalShift::isRight` | no | medium | controls shift direction | — | not-misnomer |
| `evalAnd::lhs` / `::rhs` / `::res` / `::ctx` | no | low | standard | — | — |
| `evalOr::lhs` / `::rhs` / `::res` / `::ctx` | no | low | standard | — | — |
| `valueToBool::result` (param) | unsure | low | input named like output | input (low), value (low), keep current (medium) | — |
| `valueToBool::res` | no | low | unused, but standard name | — | — |
| `valueToBool::ctx` | no | low | standard | — | — |
| `valueToBool::out` (local) | no | medium | freshly constructed return | — | not-misnomer |
| `invertBool::result` (param) | unsure | low | input named like output | input (low), keep current (medium) | — |
| `invertBool::res` | no | low | passed through to valueToBool | — | — |
| `invertBool::ctx` | no | low | standard | — | — |
| `jumpIfZero::condResult` | no | medium | condition's EvalResults | — | not-misnomer |
| `jumpIfZero::label` | no | medium | branch target label | — | not-misnomer |
| `jumpIfZero::ctx` | no | low | standard | — | — |
| `loopArgNodeAppend::target` | no | low | host node receiving append | — | — |
| `loopArgNodeAppend::arg` | unsure | low | "arg" generic; loop-var typical | loopVarNode (low), keep current (medium) | cross-batch-arbitration |
| `loopBodyNodeAppend::target` | no | low | host node receiving append | — | — |
| `loopBodyNodeAppend::body` | no | medium | loop body subtree | — | not-misnomer |
| Lambda `isConstOrCvar` (multiple sites) | no | medium | exact predicate match | — | not-misnomer |
| Lambda `isVarOrConstOrCvar` (evalShift) | no | low | predicate tests Var/Const/Cvar | — | — |

## 3. Detailed findings

### VarTypeException::msg_  [no / medium / not-misnomer]

Evidence:
- `reconstructed/include/zhinst/resources.hpp:121`  `std::string msg_;`
- `reconstructed/src/seqc_ast_nodes_evaluate.cpp:54-55`  ctor stores
  the parameter into `msg_`.
- `:60`  `what()` returns `msg_.c_str()`.

Interpretation:
- The field's only role is to hold the message string for `what()`.

Judgement:
- `msg_` is conventional and accurate for `std::exception`-style classes;
  not a misnomer.

### combineWaveforms::opName  [no / medium / not-misnomer]

Evidence:
- `:191-195`  decl shows `std::string const& opName`.
- `:464`  `result = ctx.waveformGen->eval(opName, values);`
- All in-file call paths (in 04c/04d/04e — out of scope) pass literal
  strings like `"add"`, `"sub"`, `"mul"`, `"div"` per the comment at
  `:184`.

Interpretation:
- Parameter is forwarded as the operation name argument of
  `WaveformGenerator::eval`, which dispatches by string key.

Judgement:
- `opName` is exact; not a misnomer.

### constWaveform::value  [unsure / low / —]

Evidence:
- `:204-207`  decl `constWaveform(int sampleLength, double value, ctx)`.
- `:407`  `values.emplace_back(value);  // Value(double)` — the
  value becomes the second argument to `WaveformGenerator::eval("rect",
  ...)`.

Interpretation:
- For a `rect` waveform, the second operand is the rectangular
  amplitude/sample-value. `value` is not wrong but is generic.

Judgement:
- Borderline; readers must read the body to know the role.  Worth a
  low-confidence flag for "amplitude/sample value" but not strong enough
  to override `keep current`.

Proposals:
- `sampleAmplitude` (low)
- `sampleValue` (low)
- `keep current` (medium)

### scaleWaveform(int, shared_ptr, ctx)::scaleFactor  [yes / medium / —]

Evidence:
- `:382-390`  body: the `int` parameter is marked `/*scaleFactor*/`
  and is never read.  Function unconditionally constructs
  `auto scalar = std::make_shared<EvalResults>(); scalar->setValue(-1.0);`
  and delegates to the 2-arg overload.
- `:218-223`  comment block: "the int parameter is unused — its
  register is clobbered by operator new without being read."
- `nm` confirms the symbol's signature is
  `scaleWaveform(int, shared_ptr<EvalResults>, FrontendLoweringContext&)`
  — the parameter exists in the ABI but is dead in the body.

Interpretation:
- The "factor" name implies the multiplier comes from the int argument,
  but the multiplier is hard-coded to `-1.0`.  The function is
  effectively `negateWaveform(<unused-int>, wave, ctx)`.  Anyone
  reading a call site such as `scaleWaveform(0, w, ctx)` would expect
  the result to be `0 * w`, but the actual semantics is `-1 * w`.

Judgement:
- `scaleFactor` actively misrepresents the parameter's behaviour
  (it does not scale anything); a misnomer.  The right rename depends
  on whether the dead parameter should keep a descriptive name to
  flag its uselessness, or be renamed to the unused-marker convention.

Proposals:
- `unusedScaleFactor` (low) — keeps history but flags dead
- `unused` (low)
- `keep current` (low)
- (Independent: the function itself is more accurately
  `negateWaveform`, but the *function name* is excluded by §3.)

Locations consulted:
- declared+defined: src/seqc_ast_nodes_evaluate.cpp:222-227, 382-390
- callers: search of file shows only the 2-arg overload's name
  collides; the 1-arg overload's call sites are in 04c/d/e (not
  inspected here) — see comment at `:209-227`.

### scaleWaveform(sp, sp, ctx)::scalar / ::wave  [no / medium / not-misnomer]

Evidence:
- `:480-518`  body: short-circuits when `wave->values_.back().varSubType_
  == VarSubType_FunctionArg`; pushes `waveVal` first, then `scalarVal`,
  into the values vector for `WaveformGenerator::eval("scale", values)`.
- `:238-243`  comment: "First param is the scalar (Const/Cvar), second
  is the waveform."

Interpretation:
- Both parameter names match the dispatched roles of the two arguments
  to `WaveformGenerator::eval("scale", ...)`.

Judgement:
- Names accurately describe the semantic role; not misnomers.

### computeMult::varOperand / ::constOperand  [no / medium / not-misnomer]

Evidence:
- `:259-262`  comment: "First param is always the Var-typed operand,
  second is the Const/Cvar-typed operand. Call sites swap arguments to
  ensure this ordering."
- `:548`  `constVal = constOperand->values_.back().value_;`
- `:563`  `varOperand->values_.back().varSubType_ == VarSubType_FunctionArg`

Interpretation:
- Parameter naming directly encodes the precondition that callers
  must satisfy.

Judgement:
- Not misnomers; in fact among the most informative parameter names in
  the file.

### computeMult::res  [unsure / low / —]

Evidence:
- `:541`  decl `std::shared_ptr<Resources> /*res*/`.
- `:527-528`  comment: "res (shared_ptr<Resources>) is present in the
  signature for ABI compliance but is completely unused in the function
  body."
- `nm` confirms `Resources` shared_ptr in the mangled signature.

Interpretation:
- Parameter is part of the ABI-fixed signature but unused.

Judgement:
- "res" is the codebase-wide convention for unused-but-required
  Resources slot (see also `valueToBool`, `evalGreater`, `evalLower`,
  etc.).  Renaming would diverge from the rest of the file's convention.
  Marked `unsure` because a future synthesis pass may wish to globally
  introduce a `/*unused-resources*/` style.

Proposals:
- `keep current` (medium)
- `unusedRes` (low)

### evalEqual::kRangeLo / ::kRangeHi  [yes / medium / —]

Evidence:
- `:1493-1494`  defined as `static constexpr double` with literal
  values `2147483647.0` and `4294967295.0`.
- `:1489-1492`  comment immediately above explicitly says these
  constants are `INT32_MAX` and `UINT32_MAX` from rodata @0x9562b8 /
  @0x9562c0.
- `:1496`  used in `if (rhsDouble > kRangeLo && kRangeHi > rhsDouble)`
  to detect rhs values that would overflow when negated for the
  Immediate encoding.

Interpretation:
- The names are generic ("range low/high") while the values have a
  specific, well-known meaning that the surrounding comment must
  re-explain.  The trigger condition is "rhs lies in the half-open
  interval `(INT32_MAX, UINT32_MAX]`", i.e. it fits in an unsigned 32-bit
  but is too large to negate as a signed 32-bit immediate.

Judgement:
- Both names are misnomers; they hide rather than describe the
  constants.

Proposals (paired):
- `kInt32MaxAsDouble` / `kUint32MaxAsDouble` (medium) — names mirror
  the values exactly.
- `kImmediateMaxPositive` / `kImmediateMaxUnsigned` (low) — describes
  the role rather than the value.
- Block-level §6 status: `coordinated-rename` — these two should be
  renamed together.

### evalShift::isRight  [no / medium / not-misnomer]

Evidence:
- `:1750`  decl `bool isRight`.
- `:1788`  `bool effectiveRight = isRight ^ (rhsInt < 0);`
- `:1792-1796`  branch picks `>>` (right shift, when `effectiveRight`)
  vs `<<` (left shift).
- `:1693`  comment: "isRight: false = `<<`, true = `>>`."

Interpretation:
- Parameter directly selects right vs left shift; usage is unambiguous.

Judgement:
- Name fits the operational meaning; not a misnomer.

### valueToBool::result  /  invertBool::result  [unsure / low / —]

Evidence:
- `:782`  `valueToBool(std::shared_ptr<EvalResults> result, ...)` — the
  parameter is the *input* whose value should be converted to a
  boolean.  Inside the body the constructed return value is named `out`
  (`:803`).
- `:679`  `invertBool(std::shared_ptr<EvalResults> result, ...)` — same
  pattern; the input is "the previous evaluation's result" but the
  function also returns an `EvalResults` named `result` directly
  (mutating it in place where possible).

Interpretation:
- Calling the input "result" is consistent with the surrounding
  convention that operands of these helpers are upstream evaluation
  results.  In `valueToBool` the contrast with `out` is mildly
  confusing; in `invertBool` the same name doubles as input *and*
  return value (the Var path mutates `result` and returns it).

Judgement:
- Borderline.  Within the file's idiom (input-as-`result`-of-prior-eval)
  the names are defensible; in isolation they read as if they were
  outputs.

Proposals:
- `keep current` (medium)
- `input` / `value` (low)

### jumpIfZero::condResult  [no / medium / not-misnomer]

Evidence:
- `:367-370`  decl `condResult` of `shared_ptr<EvalResults>`.
- `:2503`  `auto const& vals = condResult->values_;` — the parameter
  carries the *condition*'s evaluation result.
- `:365-366`  comment: "Used 42 times across binary
  (SeqCIfCondition, SeqCIfElse, SeqCWhileLoop, SeqCDoWhile, SeqCRepeat,
  SeqCForLoop, etc.)" — it is always the condition expression's eval
  output.

Interpretation:
- Name encodes both that it is a condition (`cond`) and that it is the
  EvalResults thereof (`Result`).

Judgement:
- Accurate; not a misnomer.

### loopArgNodeAppend::arg  [unsure / low / cross-batch-arbitration]

Evidence:
- `:2554-2577`  body: appends `arg` to `target->loop->next` chain.
- `:2551-2552`  comment: called from `SeqCWhileLoop::evaluate`,
  `SeqCDoWhile`, `SeqCRepeat`, `SeqCForLoop` — these call sites are in
  04c/d/e and not inspected here, but the name in those callers is
  almost certainly something like the loop variable / iteration argument
  Node.
- Companion function `loopBodyNodeAppend::body` (`:2588-2601`) uses the
  more specific name `body` for the same role on a sibling chain.

Interpretation:
- `arg` is generic; the asymmetry with the companion's `body` parameter
  hints that there is a more specific name for what `arg` actually
  represents (likely the loop-control node — the for-loop's
  init/update arg, or the condition node).  Without inspecting the call
  sites in 04c/d/e, this batch cannot decide.

Judgement:
- Suspicious but not concluded.  Flag for arbitration with the SeqC*Loop
  evaluate methods in 04c/d/e.

Proposals:
- `keep current` (medium)
- `loopVarNode` / `iterArg` (low) — depends on caller-side semantics

Cross-reference:
- Counterpart call sites in batches 04c / 04d / 04e
  (`SeqCWhileLoop::evaluate`, `SeqCDoWhile::evaluate`,
  `SeqCRepeat::evaluate`, `SeqCForLoop::evaluate`).

### Lambda `isConstOrCvar`  [no / medium / not-misnomer]

Evidence:
- Repeated definition in `evalGreater` (`:949-951`), `evalLower`
  (`:1216-1218`), `evalEqual` (`:1472-1474`), `evalShift` (`:1771-1773`),
  `evalAnd` (`:2073-2075`), `evalOr` (`:2295-2297`).  Body is
  identical: `return (static_cast<int>(t) | 2) == 6;`.
- `Const(4) | 2 = 6` and `Cvar(6) | 2 = 6`; no other VarType returns 6
  after `|2`.
- Note at `:151`: a non-lambda `isConstOrCvar` is exported from
  `resources.hpp` (Phase 25g) — these locals are leftover lambdas.

Interpretation:
- Predicate name precisely matches the predicate body and matches an
  already-existing namespace-scope helper.

Judgement:
- Not a misnomer.  (Separately, these duplicate lambdas are a code-hygiene
  issue, not a naming one — out of scope for this audit.)

## 4. Symbols inspected and judged routinely fine

Parameters with the standard `lhs` / `rhs` / `res` / `ctx` quartet across
`evalGreater`, `evalLower`, `evalEqual`, `evalShift`, `evalAnd`, `evalOr`:
- `lhs`, `rhs` — left/right `EvalResults const&` operands; usage in body
  matches the names exactly.
- `res` — `shared_ptr<Resources>`, unused per body comments at
  `:919-921`, `:1192-1194`, `:1444-1445`, `:1742-1743`, `:2048-2049`;
  name is the file-wide convention.
- `ctx` — `FrontendLoweringContext&`; passed to `ctx.asmCommands->...`,
  `ctx.messages->errorMessage`, `ctx.waveformGen->eval`. Standard.

Locals in the case-by-case dispatchers:
- `tempReg`, `boolReg`, `lhsReg`, `rhsReg`, `destReg`, `srcReg`,
  `resultReg`, `backReg`, `tempRegNum`, `boolRegNum`, `regNum`,
  `loadRegNum`, `rhsRegNum`, `resultRegNum` — all use the
  `<role>Reg`/`<role>RegNum` convention; consistent and accurate.
- `lhsVal`, `rhsVal`, `lhsVal2`, `rhsVal2`, `constVal`, `constVal2`,
  `lhsValue`, `rhsValue`, `scalarVal`, `waveVal` — all
  `Value`-typed locals named for their operand role.
- `lhsDouble`, `rhsDouble`, `origValue`, `floored`, `multiplier`,
  `shiftAmt`, `effectiveRight`, `lhsInt`, `rhsInt`, `shifted`,
  `inverted`, `boolVal`, `intVal` — all describe the value role.
- `addiAsms`, `addiAsms2`, `subrAsm`, `andiAsms`, `oriAsms`, `orrAsm`,
  `andrAsm`, `decAsms`, `xnoriEntries`, `copyAsms`, `loadAsms`,
  `zeroAsm`, `iloopLabelAsm`, `loopLabelAsm`, `endLabelAsm`,
  `shiftLabelAsm`, `loopBrgzAsm`, `uncondBrzAsm`, `brgzAsm`, `brzAsm`,
  `addAsm`, `sslAsm`, `ssrAsm`, `zeroAsm`, `asm1..asm4`, `cmds[]`,
  `asmZ/asmBrz/asmO/asmLbl`, `asm_brz`, `asm_br` — name encodes the
  emitted instruction.
- `combinedSub`, `combinedSubType`, `combinedType`, `resultType`,
  `resultSubType`, `firstBitSeen`, `equal`, `isStub`, `lhsBool`,
  `rhsBool`, `lhsHas1`, `rhsHas1` — descriptive and usage-matching.
- `vals`, `lhsVals`, `rhsVals` — references into the operand
  `values_` vectors; consistent.
- `varType`, `lhsType`, `rhsType`, `lhsSubType`, `rhsSubType`,
  `lvt`/`rvt` (in error paths) — straightforward.
- `label`, `iloopLabel`, `loopLabel`, `shiftLabel`, `endLabel` — clear.
- `result` (output local in `evalGreater/Lower/Equal/Shift/And/Or`
  and arithmetic helpers) — describes the constructed return; standard.
- `out` (in `valueToBool`) — distinguishes the freshly-constructed
  return EvalResults from the input parameter `result`; helpful
  disambiguator.

Top-level helpers:
- `combine(VarType,VarType)::lhs/::rhs` and the
  `combine(VarSubType,VarSubType)` parameter pair — standard binary-op
  parameter names.
- `combine(VarType,VarType)::combineTable` — local 6×7 lookup table;
  name is descriptive.
- `combineWaveforms::lhs/::rhs/::ctx` — `EvalResults const&` operands
  and standard ctx.
- `constWaveform::sampleLength/::ctx` — fits the `Value(int)` first
  argument to `eval("rect", ...)`.
- `loopBodyNodeAppend::target/::body` — describes the host node and
  the loop body subtree being appended.
- `VarTypeException::what` (method, excluded), and the constructor
  parameter `msg` — both standard.

## 5. Coverage

**Fully covered:**
- `seqc_ast_nodes_evaluate.cpp` lines 1–2610 (anonymous-namespace helper
  forward declarations and bodies, plus `VarTypeException`,
  `combine(VarType,VarType)`, `combine(VarSubType,VarSubType)`, and the
  forward declaration of `floatEqual`).
- All in-scope parameter names, data members, locals, and lambda-local
  helpers within those bodies.
- Two named local constants (`evalEqual::kRangeLo` and `::kRangeHi`).

**Deferred:**
- Arbitration of `loopArgNodeAppend::arg` to its call sites in
  `SeqC*Loop::evaluate` overrides (batches 04c / 04d / 04e).
- Whether the codebase-wide convention should rename the unused
  `Resources` slot uniformly (would touch many helpers across
  multiple batches; defer to synthesis).

**Not covered (out of scope per §2/§3):**
- All free-function and method names listed in §1's `nm` extract:
  `combine`, `combineWaveforms`, `constWaveform`, `scaleWaveform`,
  `computeMult`, `valueToBool`, `invertBool`, `jumpIfZero`,
  `evalGreater`, `evalLower`, `evalEqual`, `evalShift`, `evalAnd`,
  `evalOr`, `loopArgNodeAppend`, `loopBodyNodeAppend`, `floatEqual`,
  `VarTypeException::VarTypeException`, `VarTypeException::~VarTypeException`,
  `VarTypeException::what` — excluded per §3 (mangled symbols present
  in `_seqc_compiler.so`).
- Type name `VarTypeException` — excluded per §3 (typeinfo present
  at `0xb06600`).
- Lines 2611+ of the same file (handled by 04c/04d/04e — `SeqCAstNode`
  and per-operator/per-statement `evaluate` overrides).
- The `SeqCAstNode` header — handled by 04a.
- Standard library constructs (`std::shared_ptr`, `std::vector`, etc.)
  and the standard exception interface (`what`, `noexcept`).
