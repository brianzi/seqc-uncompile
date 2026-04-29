# Batch 04d — ast_evaluate_logical

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 5 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 1; B2 (borderline, deferred): 4;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

Comparison/logical/unary `evaluate()` overrides in
`reconstructed/src/seqc_ast_nodes_evaluate.cpp`, **lines 4691–5800**.
Covers `SeqCGreater`, `SeqCEqual`, `SeqCShiftL`, `SeqCShiftR`,
`SeqCAndExpr`, `SeqCOrExpr`, `SeqCLEqual`, `SeqCNEqual`, `SeqCLower`,
`SeqCGEqual`, `SeqCLogAnd`, `SeqCLogOr`, `SeqCMod`, `SeqCInc`,
`SeqCDec`, `SeqCNeg`, `SeqCInv`, `SeqCNotExpr`.

## 1. Files considered

- `reconstructed/src/seqc_ast_nodes_evaluate.cpp` lines 4691–5800
  (only the 18 `evaluate()` method bodies in this range).

Out of scope (handled by sibling batches):
- header `reconstructed/include/zhinst/seqc_ast_nodes.hpp` (04a)
- 04b/04c: lines 1–4690
- 04e: lines 5801+ (starts at `SeqCReturnStatement::evaluate`)

The 18 method names themselves are §3-excluded — confirmed against
`nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so`,
e.g.:

```
00000000002358b0 t zhinst::SeqCGreater::evaluate(... EvalResults const&, EvalResults const&) const
00000000002399c0 t zhinst::SeqCEqual::evaluate(... EvalResults const&, EvalResults const&) const
00000000002284e0 t zhinst::SeqCNeg::evaluate(...) const
0000000000228f50 t zhinst::SeqCInv::evaluate(...) const
0000000000229980 t zhinst::SeqCNotExpr::evaluate(...) const
000000000023bd50 t zhinst::SeqCInc::evaluate(...) const
000000000023d2b0 t zhinst::SeqCDec::evaluate(...) const
0000000000231ec0 t zhinst::SeqCMod::evaluate(...) const
0000000000242820 t zhinst::SeqCLogAnd::evaluate(...) const
0000000000243840 t zhinst::SeqCLogOr::evaluate(...) const
0000000000235690 t zhinst::SeqCShiftL::evaluate(...) const
0000000000232630 t zhinst::SeqCShiftR::evaluate(...) const
000000000023e810 t zhinst::SeqCAndExpr::evaluate(...) const
0000000000240820 t zhinst::SeqCOrExpr::evaluate(...) const
0000000000238c60 t zhinst::SeqCLEqual::evaluate(...) const
000000000023ba00 t zhinst::SeqCNEqual::evaluate(...) const
00000000002395f0 t zhinst::SeqCGEqual::evaluate(...) const
00000000002371b0 t zhinst::SeqCLower::evaluate(...) const
```

All 18 are mangled into the binary; method names are excluded.

Parameter *names* (`res`, `ctx`, `state`, `lhsResult`, `rhsResult`)
are **not** encoded by Itanium ABI mangling and remain in scope per
§3. Likewise, all local variables in these bodies are in scope.

## 2. Overview

The 5-arg binary-operator overloads share an identical signature:

```
evaluate(std::shared_ptr<Resources> res,
         FrontendLoweringContext& ctx,
         FrontendLoweringState& /*state*/,
         EvalResults const& lhsResult,
         EvalResults const& rhsResult) const
```

The 3-arg unary overloads (`SeqCNeg`, `SeqCInv`, `SeqCNotExpr`) share:

```
evaluate(std::shared_ptr<Resources> res,
         FrontendLoweringContext& ctx,
         FrontendLoweringState& state) const
```

Per-symbol parameter rows are not enumerated 18×; one representative
row per parameter name appears below, since each name is used
identically across all overloads in the batch.

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| (5-arg overloads, params identical across 15 methods) ||||||
| `SeqC<Op>::evaluate::res` | no | medium | std::shared_ptr<Resources> | keep current | not-misnomer |
| `SeqC<Op>::evaluate::ctx` | no | high | FrontendLoweringContext& | keep current | not-misnomer |
| `SeqC<Op>::evaluate::state` | no | medium | unused FrontendLoweringState& | keep current | not-misnomer |
| `SeqC<Op>::evaluate::lhsResult` | no | high | left-operand EvalResults | keep current | not-misnomer |
| `SeqC<Op>::evaluate::rhsResult` | no | high | right-operand EvalResults | keep current | not-misnomer |
| (3-arg unary overloads — `SeqCNeg`, `SeqCInv`, `SeqCNotExpr`) ||||||
| `SeqCNeg::evaluate::state` (and Inv/NotExpr) | no | high | passed to child evaluate | keep current | not-misnomer |
| (locals — by enclosing method) ||||||
| `SeqCMod::evaluate::result` | no | high | the constructed EvalResults | keep current | — |
| `SeqCMod::evaluate::lhsCount`, `rhsCount` | no | medium | values_.size() | keep current | — |
| `SeqCMod::evaluate::lhsType`, `rhsType` | no | high | VarType of operand | keep current | — |
| `SeqCMod::evaluate::errLhs`, `errLhs`/`errRhs` | unsure | low | duplicate-of-lhsType in error path | `lhsTypeForError`, keep current | — |
| `SeqCMod::evaluate::combinedType`, `combinedSub` | no | high | result of `combine(...)` | keep current | — |
| `SeqCMod::evaluate::lhsVal`, `rhsVal`, `modResult` | no | high | doubles for fmod | keep current | — |
| `SeqCInc::evaluate::lhsVals`, `rhsVals` | no | high | alias of `*Result.values_` | keep current | — |
| `SeqCInc::evaluate::lhsHas1`, `rhsHas1` | yes | medium | always true on use | `lhsHasOne`/`isSingle`, keep current | — |
| `SeqCInc::evaluate::newRegNum` | no | medium | int from getRegisterNumber | keep current | — |
| `SeqCInc::evaluate::asmCmd` | no | medium | aliased AsmCommands* | keep current | — |
| `SeqCInc::evaluate::newReg`, `srcReg`, `srcReg2`, `dstReg` | unsure | low | `srcReg2` re-read of srcReg | `srcRegReread`, keep current | — |
| `SeqCInc::evaluate::addiAsms` | no | low | AsmList from addi() | keep current | — |
| `SeqCInc::evaluate::oldVal`, `newVal`, `newRhsVal`, `newVal1`, `newVal2`, `rhsVal`, `rhsVal2` | unsure | low | many similar `*Val` locals | `oldCvarValue`, keep current | — |
| `SeqCInc::evaluate::updateOK` | no | medium | gates post-catch setValue | keep current | — |
| `SeqCInc::evaluate::vt` | no | medium | VarType used immediately | keep current | — |
| `SeqCDec::evaluate::*` (mirrors Inc) | (same as Inc) | | | (same proposals) | — |
| `SeqCNeg::evaluate::lineNr` | no | high | this->lineNr_ propagated | keep current | not-misnomer |
| `SeqCNeg::evaluate::child` | no | high | child AST node | keep current | not-misnomer |
| `SeqCNeg::evaluate::childResult` | no | high | child evaluate() return | keep current | not-misnomer |
| `SeqCNeg::evaluate::values` | no | medium | alias of childResult->values_ | keep current | — |
| `SeqCNeg::evaluate::count` | no | high | values.size() | keep current | — |
| `SeqCNeg::evaluate::vt` | no | medium | values.back().varType_ | keep current | — |
| `SeqCNeg::evaluate::tempReg`, `childReg` | no | high | new dest, child source register | keep current | — |
| `SeqCNeg::evaluate::asmCtx` | yes | medium | reference to AsmCommands, not "context" | `asmCmds`, keep current | — |
| `SeqCNeg::evaluate::zeroAsm`, `subAsm` | no | low | result of asmZero/subr | keep current | — |
| `SeqCNeg::evaluate::val`, `d` | unsure | low | `d` is bare-letter math name | `negatedDouble`, keep current | — |
| `SeqCNeg::evaluate::msg`, `result` | no | high | error msg, fresh EvalResults | keep current | — |
| `SeqCInv::evaluate::*` (mirrors Neg) | (same) | | | (same proposals) | — |
| `SeqCInv::evaluate::zeroReg` | no | high | AsmRegister(0) literal | keep current | — |
| `SeqCInv::evaluate::immNeg1` | no | high | Immediate(-1) | keep current | — |
| `SeqCInv::evaluate::asmList` | no | medium | AsmList from addi() | keep current | — |
| `SeqCInv::evaluate::intVal`, `inverted` | no | high | int payload, bitwise-NOT result | keep current | — |
| `SeqCNotExpr::evaluate::destReg`, `srcReg` | no | high | branch dest, source registers | keep current | — |
| `SeqCNotExpr::evaluate::nzeroLabel`, `endLabel` | no | high | label strings from `newLabel` | keep current | — |
| `SeqCNotExpr::evaluate::asm1..asm6` | yes | medium | numeric-suffix sequence | descriptive names, keep current | — |
| `SeqCNotExpr::evaluate::cmds` | unsure | low | C-array of moved Asms | `asmCmds`, keep current | — |
| `SeqCNotExpr::evaluate::intVal` | no | high | val.toInt() | keep current | — |
| `SeqCNotExpr::evaluate::zeroVal`, `oneVal` | no | high | Value(0)/Value(1) | keep current | — |
| `SeqCNotExpr::evaluate::s`, `len` | no | high | toString() text and size | keep current | — |
| `SeqC{LEqual,NEqual,GEqual}::evaluate::greaterResult` / `equalResult` / `lowerResult` | no | high | intermediate to invert | keep current | — |
| `SeqC{LogAnd,LogOr}::evaluate::lhsCopy`, `rhsCopy` | no | high | shared_ptr copies of operands | keep current | — |
| `SeqC{LogAnd,LogOr}::evaluate::lhsVals`, `rhsVals` | no | high | alias of values_ | keep current | — |

## 3. Detailed findings

### SeqC<Op>::evaluate::lhsResult, rhsResult  [no / high / not-misnomer]

Evidence:
- 4702–4703, 4717, 4732, 4747, 4762, 4779, 4795, 4813, 4836, 4853,
  4874, 4916, 4979, 5062, 5241 — all 5-arg overloads declare
  `EvalResults const& lhsResult, EvalResults const& rhsResult`.
- Universal usage pattern: read `.name_`, `.values_`, `.assemblers_`
  off both, e.g.
    - 4707  `result->name_ = lhsResult.name_ + " > " + rhsResult.name_;`
    - 4767  `... + " & " + rhsResult.name_;`
    - 4882  `auto const& lhsVals = lhsResult.values_;`
    - 4989  `const size_t lhsCount = lhsResult.values_.size();`
    - 5078  `lhsResult.assemblers_.begin(), lhsResult.assemblers_.end());`
- These two `EvalResults const&` references are forwarded into the
  `eval*` helpers as the *lhs/rhs of the operator*, e.g. 4705
  `evalGreater(lhsResult, rhsResult, ...)`.

Interpretation:
- The names exactly describe role (left/right operand result of an
  already-evaluated binary expression). The values_/name_/assemblers_
  access pattern is the canonical EvalResults consumption pattern
  documented in batch 34 (`34_eval_results.md`).

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Cross-reference:
- Batch 34 — `EvalResults` data members `values_`, `name_`,
  `assemblers_`.

Locations consulted:
- declared: 18 sites in src/seqc_ast_nodes_evaluate.cpp 4691–5800.
- used:     pervasively within each method.

### SeqC<Op>::evaluate::ctx  [no / high / not-misnomer]

Evidence:
- 4700, 4715, 4730, 4745, 4760, 4777, 4793, 4811, 4834, 4850, 4872,
  4914, 4977, 5060, 5239, 5410, 5525, 5659 — all overloads declare
  `FrontendLoweringContext& ctx`.
- Used as a frontend-lowering context: 5011 `ctx.messages->errorMessage(...)`,
  5084 `ctx.asmCommands.get()`, 5417 `ctx.wavetable->setLineNr(...)`,
  5421 `child->evaluate(res, ctx, state)`.

Interpretation:
- Standard short name for the FrontendLoweringContext threaded through
  every evaluate path; identical convention used elsewhere in the file.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: 18 sites in 4691–5800.

### SeqC<Op>::evaluate::res  [no / medium / not-misnomer]

Evidence:
- 4699, 4714 etc. — declared `std::shared_ptr<Resources> res`.
- Forwarded by value/move into helpers: 4706 `std::move(res)`, 4799
  `res, ctx`, 5155 `res.get()->updateCvar(...)`, 5183
  `res.get()->updateCvar(...)`.

Interpretation:
- Short common abbreviation for the `Resources` shared_ptr that is
  carried through evaluate chains. It is not used for anything other
  than the Resources shared_ptr.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)
- `resources`  (low) — only marginally clearer.

### SeqC<Op>::evaluate::state  [no / medium / not-misnomer]

Evidence:
- 4701 etc. — declared `FrontendLoweringState& /*state*/` for
  binary overloads (parameter is unused in this batch's bodies).
- 5411, 5526, 5660 — non-`/* */`-commented in the unary overloads,
  passed straight through to `child->evaluate(res, ctx, state)`
  (5421, 5536, 5670).

Interpretation:
- Name matches the type and the upstream/downstream parameter name
  used by other evaluate overloads. Unused-in-this-overload status
  doesn't make the name misleading.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

### SeqCInc::evaluate::lhsHas1 / rhsHas1 (and same in SeqCDec)  [yes / medium]

Evidence:
- 5069  `bool lhsHas1 = !lhsVals.empty() && lhsVals.size() <= 1;`
- 5070  `bool rhsHas1 = !rhsVals.empty() && rhsVals.size() <= 1;`
- All uses (5073, 5115, 5146, 5177, 5207, 5214 — and the parallel
  SeqCDec block at 5252, 5290, 5318, 5345, 5372, 5379) are of the
  form `if (lhsHas1 && lhsVals.back().varType_ == ...)`.
- The expression `!empty() && size() <= 1` is true iff `size() == 1`.

Interpretation:
- The literal `1` in the name is correct for the *intent* (exactly
  one element), but the suffix `Has1` reads as "has-one-or-more"
  to many readers; the actual condition is "has exactly one
  element". The boolean is semantically `isSingleValue`.

Judgement:
- Mild misnomer: name suggests existence/count without conveying
  the exactly-one constraint.

Proposals:
- `lhsIsSingle` / `rhsIsSingle`  (medium)
- `lhsHasOne` / `rhsHasOne`      (medium)
- keep current                    (low)

Locations consulted:
- declared: src/seqc_ast_nodes_evaluate.cpp:5069, 5070, 5248, 5249.
- used:     5073, 5115, 5146, 5177, 5207, 5214, 5252, 5290, 5318,
            5345, 5372, 5379.

### SeqCInc / SeqCDec ::evaluate::srcReg2 (and dstReg)  [unsure / low]

Evidence:
- 5102–5103  `// Re-read srcReg from lhsResult (binary re-reads).`
  followed by `AsmRegister srcReg2 = lhsVals.empty() ? AsmRegister(0)
  : lhsVals.back().reg_;`
- 5107  `auto addiAsms = asmCmd->addi(srcReg2, srcReg2, Immediate(1));`
- 5131–5134  `dstReg`/`srcReg2` both re-read from `rhsVals.back().reg_`.
- 5277, 5305, 5306 — same pattern in SeqCDec.

Interpretation:
- `srcReg2` is a re-read of the same register as `srcReg`, only
  introduced because the binary literally reloads the value. The
  `2` suffix is a serial-number, not a semantic name. Inside path 2
  the variable is also passed as the *destination* of `addi`, so
  the suffix is doubly misleading.

Judgement:
- Names reflect a faithful binary reproduction artifact rather than
  a semantically distinct register. Borderline misnomer.

Proposals:
- `srcRegReread`  (low)
- `srcReg`        (low) — would conflict with prior local; would
   require rewriting the surrounding scope. Possibly cleaner if the
   re-read is collapsed in a future cleanup pass.
- keep current    (medium)

Cross-reference:
- Batch 06 — `AsmRegister`.

Locations consulted:
- src/seqc_ast_nodes_evaluate.cpp:5102, 5107, 5131–5138, 5277,
  5282, 5303–5310.

### SeqCMod::evaluate::errLhs / errRhs  [unsure / low]

Evidence:
- 5004–5009  In the error path, locals `errLhs` / `errRhs` are
  initialised to `VarType_Unset` and then set to the operand's
  varType_ if `count == 1` — duplicating the work already done for
  `lhsType` / `rhsType` at lines 4992–4997.

Interpretation:
- The duplication is faithful to the binary (`@0x2320a1` / `@0x2320d9`
  re-load the values inside the error block). The `err` prefix is
  not strictly inaccurate — it does flag "value used in error
  formatting" — but it conceals that the value is the same VarType
  computed two scopes up.

Judgement:
- Names are not actively misleading; the prefix is informative
  about the consumer. Mild noise only.

Proposals:
- `lhsTypeForError` / `rhsTypeForError`  (low)
- keep current                           (medium)

### SeqCNeg::evaluate::asmCtx (and SeqCInv::evaluate::asmCtx)  [yes / medium]

Evidence:
- 5457  `auto& asmCtx = *ctx.asmCommands;` — type is `AsmCommands&`.
- 5460  `auto zeroAsm = asmCtx.asmZero(tempReg);`
- 5470  `auto subAsm = asmCtx.subr(tempReg, childReg);`
- 5571  `auto& asmCtx = *ctx.asmCommands;` — same pattern in SeqCInv.
- 5577, 5588, 5601 — used purely as the AsmCommands receiver.

Interpretation:
- `Ctx`/`Context` is reserved in this codebase for the
  `FrontendLoweringContext` (named `ctx` 5 lines above). Calling a
  reference to `AsmCommands` `asmCtx` invents a non-existent
  "AsmContext" type and clashes with the established `ctx` term.

Judgement:
- Misleading: this is an AsmCommands alias, not a context.

Proposals:
- `asmCmds`     (medium)
- `asmCommands` (medium) — matches both the field name on
  `FrontendLoweringContext` (`ctx.asmCommands`) and other call sites
  in the file (e.g. `SeqCInc` uses `asmCmd`, see next finding).
- keep current  (low)

Cross-reference:
- Batch 10 — `AsmCommands`.

Locations consulted:
- declared: src/seqc_ast_nodes_evaluate.cpp:5457, 5571.
- used:     5460, 5470, 5577, 5588, 5601.

### SeqCInc::evaluate::asmCmd  vs  SeqCNeg::evaluate::asmCtx  [no / low]

Evidence:
- 5084  `AsmCommands* asmCmd = ctx.asmCommands.get();`
- 5128, 5261, 5301 — same pattern in SeqCInc/SeqCDec.
- 5457  `auto& asmCtx = *ctx.asmCommands;` (in SeqCNeg).

Interpretation:
- Within the same file two different aliases are used for the same
  underlying object: `asmCmd` (Inc/Dec) vs `asmCtx` (Neg/Inv). The
  former is a faithful abbreviation; the latter is misleading (see
  prior block).

Judgement:
- `asmCmd` is fine; the inconsistency itself is not a renaming
  problem on this side, only on the `asmCtx` side.

Proposals:
- keep current  (high)

Cross-reference:
- prior `SeqCNeg::evaluate::asmCtx` block (same batch).

### SeqCNotExpr::evaluate::asm1 .. asm6  [yes / medium]

Evidence:
- 5717–5722:
    ```
    auto asm1 = ctx.asmCommands->asmZero(destReg);
    auto asm2 = ctx.asmCommands->brz(srcReg, nzeroLabel, false);
    auto asm3 = ctx.asmCommands->br(endLabel, false);
    auto asm4 = ctx.asmCommands->asmLabel(nzeroLabel);
    auto asm5 = ctx.asmCommands->asmOne(destReg);
    auto asm6 = ctx.asmCommands->asmLabel(endLabel);
    ```
- 5724–5726  packed into `AsmList::Asm cmds[]` and inserted in order.

Interpretation:
- Numeric-suffix names hide the very different semantics
  (zero-load / branch-if-zero / branch / label / one-load / label).
  Each one is single-use, so descriptive names would not collide
  and would make the emitted instruction sequence self-documenting.

Judgement:
- Generic numeric suffixes on functionally-distinct values are a
  classic §4a non-descript-name signal.

Proposals:
- `zeroAsm`, `brzAsm`, `brAsm`, `nzeroLabelAsm`, `oneAsm`,
  `endLabelAsm`  (medium)
- keep current  (low)

Locations consulted:
- src/seqc_ast_nodes_evaluate.cpp:5717–5731.

### SeqC{LEqual,NEqual,GEqual}::evaluate::greaterResult / equalResult / lowerResult  [no / high]

Evidence:
- 4798  `auto greaterResult = evalGreater(lhsResult, rhsResult, res, ctx);`
- 4816  `auto equalResult = evalEqual(res, ctx, lhsResult, rhsResult);`
- 4855  `auto lowerResult = evalLower(lhsResult, rhsResult, res, ctx);`
- 4800, 4818, 4857 — each is immediately consumed by `invertBool(...)`.

Interpretation:
- The temporary's name precisely identifies which `eval*` produced
  it before inversion — useful for readers because the surrounding
  method's operator (`<=`, `!=`, `>=`) is the inverted form.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

### SeqC{LogAnd,LogOr}::evaluate::lhsCopy, rhsCopy  [no / high]

Evidence:
- 4878–4879  `auto lhsCopy = std::make_shared<EvalResults>(lhsResult);
              auto rhsCopy = std::make_shared<EvalResults>(rhsResult);`
- 4885  `lhsCopy = valueToBool(std::move(lhsCopy), res, ctx);`
- 4898  `auto result = evalAnd(std::move(res), ctx, *lhsCopy, *rhsCopy);`
- 4920–4921, 4927, 4940 — mirror in SeqCLogOr.

Interpretation:
- These are literal copies of the operand `EvalResults` made so they
  can be re-pointed by `valueToBool`. The name is exact.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

## 4. Symbols inspected and judged routinely fine

Looked at, no separate block needed:

- `result` (every method): always the EvalResults being constructed
  and returned; matches usage.
- `SeqCMod::evaluate::lhsCount`, `rhsCount`, `lhsType`, `rhsType`,
  `combinedType`, `combinedSub`, `lhsVal`, `rhsVal`, `modResult` —
  names match what they hold and how they are consumed.
- `SeqCInc::evaluate::lhsVals`, `rhsVals`, `newRegNum`, `oldVal`,
  `newVal`, `newVal1`, `newVal2`, `newRhsVal`, `vt`, `updateOK` —
  short but accurate.
- `SeqCDec::evaluate::*` — direct mirror of SeqCInc; no
  per-symbol differences in naming.
- `SeqCNeg`/`SeqCInv`/`SeqCNotExpr::evaluate::lineNr`, `child`,
  `childResult`, `values`, `count`, `vt`, `tempReg`, `childReg`,
  `destReg`, `srcReg`, `nzeroLabel`, `endLabel`, `intVal`, `inverted`,
  `s`, `len`, `val`, `zeroVal`, `oneVal`, `msg` — names match either
  the type, the immediate computation, or the documented role.
- `addiAsms`, `zeroAsm`, `subAsm`, `asmList` — local AsmList
  temporaries; transparent enough at single use.
- `lineNr` reads `this->lineNr_` (member of `SeqCAstNode` — out of
  this batch).

## 5. Coverage

- **Fully covered:** all 18 `evaluate()` method bodies in
  src/seqc_ast_nodes_evaluate.cpp lines 4691–5800
  (`SeqCGreater`, `SeqCEqual`, `SeqCShiftL`, `SeqCShiftR`,
   `SeqCAndExpr`, `SeqCOrExpr`, `SeqCLEqual`, `SeqCNEqual`,
   `SeqCLower`, `SeqCGEqual`, `SeqCLogAnd`, `SeqCLogOr`, `SeqCMod`,
   `SeqCInc`, `SeqCDec`, `SeqCNeg`, `SeqCInv`, `SeqCNotExpr`).
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Method names of all 18 `SeqC*::evaluate` overloads — confirmed
    in `nm --demangle _seqc_compiler.so` (see §1).
  - Member access targets — `EvalResults::values_`, `name_`,
    `assemblers_`, `arrayBacking_` (batch 34); `Value` /
    `EvalResultValue` members (batches 11, 12); `AsmRegister`,
    `Immediate` (batch 06); `AsmCommands` methods (batch 10);
    `FrontendLoweringContext::messages`/`asmCommands`/`wavetable`
    and `Resources::*` (other batches).
  - The signatures and parameter names of helper functions
    `evalGreater`, `evalLower`, `evalEqual`, `evalShift`, `evalAnd`,
    `evalOr`, `valueToBool`, `invertBool`, `scaleWaveform`,
    `isConstOrCvar` — defined earlier in the file, owned by another
    batch (04b/04c per the assignment).
  - Enum names (`VarType_*`, `VarSubType_*`) and `ErrorMessageT` —
    owned elsewhere.
