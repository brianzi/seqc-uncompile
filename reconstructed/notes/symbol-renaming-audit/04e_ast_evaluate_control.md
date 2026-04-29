# Batch 04e — ast_evaluate_control

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 4 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 3;
> B3 (already resolved during Phase D/R): 1;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

Sub-batch of the AST `evaluate()` audit covering control-flow and
function-related node `evaluate()` definitions in
`reconstructed/src/seqc_ast_nodes_evaluate.cpp`, **lines 5801–10137**.

## 1. Files considered

- `reconstructed/src/seqc_ast_nodes_evaluate.cpp` — lines 5801–10137 only.

Methods in this batch (16 total `evaluate()` defs + 5 helpers):

| # | Class / helper                | Lines       |
|---|-------------------------------|-------------|
| 1 | `SeqCReturnStatement::evaluate` | 5803–6074 |
| 2 | `SeqCArgList::evaluate`         | 6091–6146 |
| 3 | `SeqCDeclList::evaluate`        | 6154–6209 |
| 4 | `SeqCStmtList::evaluate`        | 6230–6322 |
| 5 | `SeqCFunctionCall::evaluate`    | 6354–6830 |
| 6 | `SeqCArray::evaluate`           | 6839–6968 |
| 7 | `SeqCIfCondition::evaluate`     | 6990–7149 |
| 8 | `SeqCCaseEntry::evaluate`       | 7174–7253 |
| 9 | `SeqCSwitchCase::hasCases / isSingleCase / singleCase / cases` | 7257–7284 |
| 10 | `(anon) evalCaseBody`          | 7300–7387 |
| 11 | `SeqCSwitchCase::evalCases`    | 7402–7489 |
| 12 | `SeqCSwitchCase::evaluate`     | 7538–8054 |
| 13 | `SeqCWhileLoop::evaluate`      | 8086–8312 |
| 14 | `SeqCDoWhile::evaluate`        | 8348–8638 |
| 15 | `SeqCRepeat::evaluate`         | 8671–8996 |
| 16 | `SeqCIfElse::evaluate`         | 9026–9288 |
| 17 | `SeqCCondExpr::evaluate`       | 9311–9652 |
| 18 | `SeqCFunction::evaluate`       | 9676–9840 |
| 19 | `SeqCForLoop::evaluate`        | 9858–10135 |

Cross-batch context:

- All 16 `evaluate(3-arg)` method names AND `SeqCSwitchCase::hasCases /
  isSingleCase / singleCase / cases / evalCases` AND the anonymous
  `evalCaseBody` are present in `nm --demangle _seqc_compiler.so`.
  Tier-1 §3-excluded from rename (see §1 nm verification below).
- The three formal parameters that recur on every method
  (`std::shared_ptr<Resources> res`, `FrontendLoweringContext& ctx`,
  `FrontendLoweringState& state`) are not encoded in the mangled
  signatures (§3 — only types are encoded). They are in scope and
  judged uniformly below; the field/type names themselves are audited
  in batch 32 (frontend_lowering).
- `EvalResults::hasError_` (yes/medium "returnEncountered_") was
  confirmed in batch 34. Many sites in this batch read or write
  `result->hasError_` / `childResult->hasError_` with the
  "return-was-encountered" semantics; that finding stands and is
  reinforced by §3 below (`SeqCStmtList::evaluate::childHadError`).
- `EvalResults::name_` (unsure/low) — every `evaluate()` here writes a
  source-text reconstruction into `result->name_` (e.g.
  `"if (" + condResult->name_ + ")"`, `"return"`, `"switch (...)"`,
  `"repeat(N)"`). Reinforces batch 34's role-(2) observation; does not
  add new evidence on top of what batch 34 already records.
- `FrontendLoweringState::pad10_` (yes/high), `strings` (yes/medium),
  `inLoop_` (unsure/medium), `inSwitch_` (no/high) all heavily
  exercised in this batch. The batch-04e source corroborates batch 32
  but adds no new arbitration material — see §1 verification below
  for specific use sites.
- `FrontendLoweringContext::channelGrouping` (yes/medium
  coordinated-rename, batch 32) is read three times in this batch as
  the loop-unroll iteration limit (lines 8202, 8501, 8779, 10010).
  Reinforces batch 32; nothing to add.
- `Prefetch::getUsedFourChannelMode` (batch 09) — not referenced in
  this range.

### Binary cross-checks performed (RULES §3)

```
$ nm --demangle _seqc_compiler.so | grep -E '::evaluate\(|evalCaseBody|::evalCases|::hasCases|::isSingleCase|::singleCase|SeqCSwitchCase::cases'
```

Tier-1 excluded names (free-function/method names in symbol table):

- `zhinst::SeqCReturnStatement::evaluate(…) const` @0x226a40
  (returnstatement evaluate symbol present — listed in 04d range
  context, included by line ranges only)
- `zhinst::SeqCArgList::evaluate` @0x211de0
- `zhinst::SeqCDeclList::evaluate` @0x2122f0
- `zhinst::SeqCStmtList::evaluate` @0x212800
- `zhinst::SeqCFunctionCall::evaluate` @0x20c6a0
- `zhinst::SeqCArray::evaluate` (3-arg) — verified by surrounding
  symbols (SeqCArray type appears in mangled signatures of
  `SeqCArray::~SeqCArray`, etc.)
- `zhinst::SeqCIfCondition::evaluate` @0x2138e0
- `zhinst::SeqCCaseEntry::evaluate` @0x21aa40
- `zhinst::SeqCSwitchCase::hasCases / isSingleCase / singleCase /
  cases / evalCases / evaluate` @ 0x202760 / 0x202730 / 0x202770 /
  0x202700 / 0x216980 / 0x217a80
- `(anonymous namespace)::evalCaseBody(SeqCCaseEntry const&,
  vector<shared_ptr<EvalResults>>&, shared_ptr<Resources>,
  shared_ptr<EvalResults>, FrontendLoweringContext&,
  FrontendLoweringState&)` @0x216fc0 — present
- `zhinst::SeqCWhileLoop::evaluate` @0x21e130
- `zhinst::SeqCDoWhile::evaluate` @0x21fd00
- `zhinst::SeqCRepeat::evaluate` @0x221c10
- `zhinst::SeqCIfElse::evaluate` @0x214d50
- `zhinst::SeqCCondExpr::evaluate` @0x223d90
- `zhinst::SeqCFunction::evaluate` @0x20b200
- `zhinst::SeqCForLoop::evaluate` @0x21b680

The `evalCaseBody` symbol IS in the binary as
`(anonymous namespace)::evalCaseBody(...)`. Even though it is
file-static, its name is preserved in the static-symbol table; tier-1
excluded.

The recurring parameters of every `evaluate(3)` (`res`, `ctx`,
`state`) and every helper's parameter names are NOT in the mangled
form — in scope.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `SeqC*::evaluate(3)` (16 method names) | no | high | all in `nm --demangle` | keep current | not-misnomer |
| `SeqCSwitchCase::hasCases / isSingleCase / singleCase / cases / evalCases` | no | high | all in `nm --demangle` | keep current | not-misnomer |
| `evalCaseBody` (anon helper) | no | high | symbol in `nm --demangle` | keep current | not-misnomer |
| **Common method parameters** (`res`, `ctx`, `state`) | no | medium | type-faithful, idiomatic | keep current | — |
| `SeqCReturnStatement::evaluate::lineNr` | no | high | ≡ `lineNr_` field | keep current | — |
| `SeqCReturnStatement::evaluate::resPtr` | no | low | non-owning view, terse | keep current | — |
| `SeqCReturnStatement::evaluate::result` | no | high | the produced EvalResults | keep current | — |
| `SeqCReturnStatement::evaluate::childResult` | no | high | child eval product | keep current | — |
| `SeqCReturnStatement::evaluate::returnType / childVarType / count` | no | medium | type-faithful | keep current | — |
| `SeqCReturnStatement::evaluate::returnReg / childReg / addiResult` | no | medium | type-faithful | keep current | — |
| `SeqCReturnStatement::evaluate::childVal / retVal / val2` | no | low–medium | local Value copies | `wave/Val` for val2 (low) | — |
| `SeqCReturnStatement::evaluate::brAsm` | no | high | branch instruction | keep current | — |
| `SeqCArgList/DeclList/StmtList::evaluate::lineNr` | no | high | ≡ `lineNr_` | keep current | — |
| `SeqCArgList/DeclList::evaluate::elems / hasError / i / childResult / msg` | no | medium | type-faithful | keep current | — |
| `SeqCArgList/DeclList/StmtList::evaluate::lineNr` (note) | unsure | low | comes from `this->type()` not `lineNr_` | `nodeLineNr` (low), keep | in-scope (parameter name; nm preserves only int type) |
| `SeqCStmtList::evaluate::childHadError` | yes | medium | also fires on return-stmt | `childUnwound`, keep | cross-batch-arbitration |
| `SeqCStmtList::evaluate::nextLineNr` | no | high | line for unreachable warning | keep current | — |
| `SeqCFunctionCall::evaluate::funName / outSig / exists` | no | medium | type-faithful | keep current | — |
| `SeqCFunctionCall::evaluate::funNameNode` | no | medium | AST node holding fn name | keep current | — |
| `SeqCFunctionCall::evaluate::retLabel / argResults / argNode` | no | medium | type-faithful | keep current | — |
| `SeqCFunctionCall::evaluate::argTypeSig / fullSig / overloads / overload` | no | medium | type-faithful | keep current | — |
| `SeqCFunctionCall::evaluate::func / funcScope / returnType / paramNames` | no | medium | type-faithful | keep current | — |
| `SeqCFunctionCall::evaluate::argCount / paramCount / param / paramType / paramName / argVal` | no | medium | type-faithful | keep current | — |
| `SeqCFunctionCall::evaluate::dstReg / srcReg / argType / instr` | no | medium | type-faithful | keep current | — |
| `SeqCFunctionCall::evaluate::bodyResult / bodyNode / savedReturnType / savedLineNr / savedInLoop` | no | medium | type-faithful | keep current | — |
| `SeqCFunctionCall::evaluate::argChildren / varName / it / idx / argExprNode / argExprName / msg` | no | low | type-faithful | keep current | — |
| `SeqCFunctionCall::evaluate::funLineNr` | no | medium | line number of fn-name node | keep current | — |
| `SeqCFunctionCall::evaluate::subScope / argValues / cfResult` | no | medium | type-faithful | keep current | — |
| `SeqCFunctionCall::evaluate::commaCount / truncPos / j` | no | low | local string-walk | keep current | — |
| `SeqCFunctionCall::evaluate::regNum / waveStr / strVal / val` | no | low | type-faithful | keep current | — |
| `SeqCFunctionCall::evaluate::argNode2 / e` | no | low | exception-handler locals | keep current | — |
| `SeqCArray::evaluate::arrayResult / indexResult / arrayVal / arrayValue / waveName` | no | medium | type-faithful | keep current | — |
| `SeqCArray::evaluate::wf / optName / indexValue / idxVal / idx` | no | medium | type-faithful | keep current | — |
| `SeqCArray::evaluate::channels / length / dc / arrayBound / granularity / pageSize / padded / totalSamples / bitsPerSample / totalBits` | no | medium | type-faithful | keep current | — |
| `SeqCArray::evaluate::indexedResult / sampleVal` | no | medium | type-faithful | keep current | — |
| `SeqCIfCondition::evaluate::endLabel / condScope / condResult / condVal / branchAsm / jumpAsms / ifLabel / ifLabelAsm / bodyScope / bodySubScope / bodyResult / endLabelAsm / val / intVal` | no | medium | type-faithful | keep current | — |
| `SeqCIfCondition::evaluate::msg` | no | high | error message string | keep current | — |
| `SeqCCaseEntry::evaluate::labelNode / labelResult / hasLbl / validLbl / caseVal / dVal / iVal / msg` | no | medium | type-faithful | keep current | — |
| `evalCaseBody::caseEntry / results / subRes / condResult / ctx / state` | no | medium | type-faithful | keep current | — |
| `evalCaseBody::caseResult / evalBody / condValue / caseValue / savedHasError / bodyResult` | no | medium | type-faithful | keep current | — |
| `SeqCSwitchCase::evalCases::subRes / condResult` | no | medium | type-faithful | keep current | — |
| `SeqCSwitchCase::evalCases::results / casesList / stmts / stmt / caseEntry / lastResult / lastEval / target / node / stmtResult / e / msg` | no | medium | type-faithful | keep current | — |
| `SeqCSwitchCase::evalCases::lastEval` | yes | medium | declared, never read | (delete), keep | — |
| `SeqCSwitchCase::evaluate::savedInSwitch / subRes / condResult / condVal / casesResult / switchAsms / caseAsms / bodyAsms / branchAsm / switchRegNum / switchReg / zeroReg / condReg / cyclesF3 / cyclesB0 / numCases / totalCycles / suserAsm / endLabel / hasDefault / caseRegNum / caseReg / caseLabel / condValues / condValue / caseValues / caseValue / negCaseVal / brzAsm / brzEnd / labelAsm / bodyRegNum / bodyReg / bodyZeroReg / readRV / readConstVal / wtrigAsm / nopAsm / defaultLabel / brzDef / endLabelAsm` | no | medium | type-faithful | keep current | — |
| `SeqCSwitchCase::evaluate::matchedConstResult / matchedDefaultResult / matchedHasError / matchedResult` | no | medium | type-faithful, doc'd | keep current | — |
| `SeqCSwitchCase::evaluate::usePad10` | no | medium | what `pad10_` triggers | keep current | — |
| `SeqCWhileLoop::evaluate::whileLabel / whileLabelAsm / subRes / condResult / bodyResult / accumulatedNode / iterCount / normalExit / condVal / unrollScope / bodyEval / current / loopLabel / endLabel / condResultSaved / jumpAsms / loopNodeAsm / loopLabelAsm / bodySubScope / brAsm / endLabelAsm / msg / msgs / it` | no | medium | type-faithful | keep current | — |
| `SeqCDoWhile::evaluate::doLabel / whileLabel / endLabel / doLabelAsm / subRes / condResult / condResult2 / whileLabelAsm / doEpilogue / accumulatedNode / iterCount / bodyEval / current / unrollScope / bodyResult / condVal / condResultSaved / jumpAsms / loopNodeAsm / varType / brnzAsm / brAsm / endLabelAsm / intVal / msgs / it` | no | medium | type-faithful | keep current | — |
| `SeqCRepeat::evaluate::countResult / counterReg / hasEndLabel / countVal / countInt / firstBodyResult / firstBodyEmpty / initAsms / lastVal / lastInt / accumulatedNode / bodyResult / bodyEval / unrollScope / maybeScope / current / savedLineNr / loopLabel / endLabel / brzAsm / loopLabelAsm / loopNodeAsm / countReg / decrAsms / brgzAsm / endLabelAsm / msgs / it` | no | medium | type-faithful | keep current | — |
| `SeqCRepeat::evaluate::hasEndLabel` | unsure | low | inverted-feel name | `needsCountCheck`, keep | — |
| `SeqCIfElse::evaluate::condScope / condResult / condVal / ifLabel / elseLabel / endLabel / branchAsm / jumpAsms / ifTrueScope / ifFalseScope / ifBodyResult / elseBodyResult / bodySubScope / elseSubScope / ifLabelAsm / brAsm / elseLabelAsm / endLabelAsm / val / intVal / deadResult / liveResult / deadScope / liveScope / msg` | no | medium | type-faithful | keep current | — |
| `SeqCCondExpr::evaluate::condResult / condVal / elseLabel / endLabel / branchAsm / savedCondResult / jumpAsms / ifScope / elseScope / ifResult / elseResult / regNum / regNum2 / resultReg / ifReg / elseReg / elseReg2 / ifReg2 / zeroReg / addiAsms / addi1 / brAsm / elseLabelAsm / endLabelAsm / val / intVal / vt` | no | medium | type-faithful | keep current | — |
| `SeqCFunction::evaluate::funNameVar / funName / signature / oss / varTypes / it / returnVarType / func / vtStr / bodyResult / ex / msg` | no | medium | type-faithful | keep current | — |
| `SeqCForLoop::evaluate::subRes / initResult / forLabel / forLabelAsm / condResult / condNode / incrResult / bodyResult / cvarRes / initEval / accumulatedNode / iterCount / hasErrorOrNull / condVal / unrollScope / bodyEval / current / bodyLabel / endLabel / condResultSaved / jumpAsms / bodyLabelAsm / loopNodeAsm / bodySubScope / r / brAsm / endLabelAsm / msgs / it` | no | medium | type-faithful | keep current | — |
| `SeqCForLoop::evaluate::hasErrorOrNull` | yes | low | declared, never read | (delete), keep | — |
| `SeqCForLoop::evaluate::r` (line 10096) | unsure | low | aliased `result.get()` once | inline, keep | — |
| `SeqCForLoop::evaluate::cvarRes` | no | low | sub-scope reassigned to subRes | keep current | — |

(See §3 below for `childHadError` and the few items flagged `yes` /
`unsure`. The remaining table rows are §4-routine: matched
type-faithful local naming with no contradicting use-site signal.)

## 3. Detailed findings

### Common method parameters (`res`, `ctx`, `state`) on every `evaluate(3)`  [no / medium]

Evidence:

- All 16 `evaluate(3)` definitions in this range share the signature
  `(std::shared_ptr<Resources> res, FrontendLoweringContext& ctx,
   FrontendLoweringState& state) const` (lines 5803-5806, 6091-6094,
  6154-6157, 6230-6233, 6354-6357, 6839-6842, 6990-6993, 7174-7177,
  7538-7541, 8086-8089, 8348-8351, 8671-8674, 9026-9029, 9311-9314,
  9676-9679, 9858-9861).
- `res` is consumed via `res->createSubScope(...)`,
  `res->newLabel(...)`, `res->getReturnType()`,
  `res->setReturnValue(...)`, `res->functionExists(...)`,
  `res->getFunction(...)`, `res->addFunction(...)`,
  `res->setAtScopeBoundary(...)`, etc. — all `Resources` API calls.
- `ctx` is dereferenced as `ctx.messages->...`,
  `ctx.asmCommands->...`, `ctx.wavetable->...`,
  `ctx.customFunctions->...`, `ctx.channelGrouping`. Each accesses a
  named field on `FrontendLoweringContext` (batch 32).
- `state` is dereferenced as `state.strings`, `state.inLoop_`,
  `state.inSwitch_`, `state.pad10_` — fields audited in batch 32.
- All three names appear identically across every `evaluate(3)` and
  across helpers (`evalCaseBody`, `evalCases`).

Interpretation:

- `res` is a conventional shorthand for `Resources` (the type whose
  shared_ptr the parameter holds); the role is "the active scope's
  resource bag". `ctx` and `state` are the same shorthand for
  `FrontendLoweringContext` and `FrontendLoweringState`. Each
  parameter is consumed solely as the corresponding type's API
  surface; no use site contradicts the name.

Judgement:

- Not misnomers; names mirror the static type and observed usage.
  Confidence is medium (not high) because no naming evidence for the
  original parameter names exists in the binary; they are
  reconstruction-side conventions.

Proposals:

- keep current (medium)

Locations consulted:

- declared on every `evaluate(3)` listed above; the same convention
  is also used in batch 32 helpers and in batches 04b/04c/04d
  evaluate methods (consistency, low-risk).

---

### `SeqCArgList / DeclList / StmtList::evaluate::lineNr`  [unsure / low / verify-not-original]

Evidence:

- `SeqCArgList::evaluate` (line 6097)
  `int lineNr = this->type();` then
  `ctx.messages->setLineNr(lineNr);`.
- `SeqCDeclList::evaluate` (line 6160) and `SeqCStmtList::evaluate`
  (line 6236) repeat the same pattern: `int lineNr = this->type();`.
- All other `evaluate(3)` in the file use
  `const int lineNr = lineNr_;` (e.g. line 5809, 6360, 6996, 7186,
  7544, 8092, 8354, 8677, 9032, 9317, 9682, 9864).
- `SeqCAstNode::type()` returns the AST node's source-line number
  (binary inspection, batch 04a), not a "type" enumerator. So the
  three list-node sites compute the same value as `lineNr_` — they
  just go through the virtual `type()` accessor.

Interpretation:

- The local name `lineNr` does describe what the variable holds; the
  oddity is on the right-hand side (calling it `type()` is the
  misnamed accessor, not the local). The local is consistent with
  every other `evaluate(3)` in the file.

Judgement:

- The local `lineNr` is fine; the `type()` call name is the actual
  problem and lives in batch 04a (header).

Proposals:

- keep current (medium) for the local
- Cross-reference: `SeqCAstNode::type()` accessor — flag in batch 04a.

Locations consulted:

- src/seqc_ast_nodes_evaluate.cpp:6097, 6160, 6236
- and contrast: 5809, 6360, 6996 (sister methods using `lineNr_`)

---

### `SeqCStmtList::evaluate::childHadError`  [yes / medium / cross-batch-arbitration]

Evidence:

- src/seqc_ast_nodes_evaluate.cpp:6254 — `bool childHadError = false;`
- 6267 — `childHadError = childResult->hasError_;`
- 6269 — `if (!childHadError) { ... build name + chain node_ ... }`
- 6288 — `else { ... unreachable-code warning + extract return value
  + result->hasError_ = true; }`
- 6314 — set `childHadError = true;` only on null `childResult`
  (which is also a real error, not a return).
- 6318 — `if (childHadError) break;` — sole consumer.
- The "child had an error" branch at line 6288 is the **return
  statement detection** path: it tests
  `dynamic_cast<const SeqCReturnStatement*>(elems[i].get())` for the
  unreachable-code warning, and stores `setReturnValue(...)` into
  `Resources`. Both are "unwind on `return;`" semantics, not error
  semantics.
- File comment at line 6216 calls this exactly the right name:
  > Return statement detection via dynamic_cast: if a child returns
  > with `hasError_==true` ...
- This is the read-side counterpart of `EvalResults::hasError_`
  (batch 34: yes/medium, proposed `returnEncountered_` /
  `unwindReturn_`).
- `incidental_findings.md` IF-30 (batch 34) states: "the binary
  (0x212800) has two completely separate paths: childHadError==0
  does name-building + node-chaining only; childHadError==1 does
  return-value extraction + hasError_=true + break."

Interpretation:

- `childHadError` is set true in two distinct cases: (a) child
  evaluation returned a true `hasError_` (i.e. a `return` statement
  unwind, per batch 34) OR (b) child evaluation returned nullptr
  (a real error, line 6310-6315). Case (a) dominates the comments
  and use sites; the local name conflates the unwind signal with
  the genuine-error signal — the same defect as the field it
  copies.

Judgement:

- Yes, `childHadError` is misleading for the same reason as
  `EvalResults::hasError_`: it primarily flags "child encountered a
  return statement", not an error. Proposed rename should track
  whatever batch 34's `hasError_` arbitration settles on.

Proposals:

- `childUnwound` (medium) — matches batch 34's `unwindReturn_`
  proposal.
- `childReturnEncountered` (medium) — matches batch 34's
  `returnEncountered_` proposal.
- keep current (low) — only if `hasError_` itself is kept.

Cross-reference:

- Counterpart `EvalResults::hasError_` in batch 34 (eval_results).

Locations consulted:

- declared: src/seqc_ast_nodes_evaluate.cpp:6254
- written: 6254, 6267, 6314
- read:    6269, 6318
- discussed: notes/incidental_findings.md IF-30; batch 34 §3
  `EvalResults::hasError_`

---

### `SeqCSwitchCase::evalCases::lastEval`  [yes / medium]

Evidence:

- src/seqc_ast_nodes_evaluate.cpp:7449-7450 —
  ```cpp
  auto& lastResult = results.back();
  auto lastEval = *lastResult;  // get the EvalResults pointed to
  ```
- The variable `lastEval` (a copy of `*results.back()`) is **never
  read** in the surrounding block. The next line takes
  `auto& target = results.back();` (line 7454) and writes through
  `target->...`. `lastResult` is also unused.

Interpretation:

- `lastEval` and `lastResult` are dead stores. They appear to be
  reconstruction debris from a half-finished idea ("get last result
  to merge into") that was completed via `target` directly. The
  copy-construction of `lastEval = *lastResult` even has cost
  (full `EvalResults` deep copy) at runtime.

Judgement:

- Yes, the locals are misnomers in the sense that they have no
  consumed role at all; the names suggest they hold something
  meaningful but they are unused.

Proposals:

- delete both (`lastResult` and `lastEval`) (medium) — recon-only
  cleanup; not a rename.
- keep current (low) — only if the synthesis prefers no behavior
  change pending verification that the dead copy is truly absent
  from the binary.

Locations consulted:

- src/seqc_ast_nodes_evaluate.cpp:7449-7458

---

### `SeqCRepeat::evaluate::hasEndLabel`  [unsure / low]

Evidence:

- src/seqc_ast_nodes_evaluate.cpp:8715
  `bool hasEndLabel = true;  // controls brz emission + endLabel`
  `// [rbp-0x110]`
- Set true at line 8715 (default) and 8901, 8912.
- Set false at line 8830 with comment "count known >= 2, no brz needed".
- Read at lines 8920 (`if (hasEndLabel) { endLabel = ...; brz ... }`)
  and 8939
  (`loopNodeAsm.node->loopBodyRunsAtLeastOnce = !hasEndLabel;`)
  and 8976 (`if (hasEndLabel) { ... asmLabel(endLabel) ... }`).

Interpretation:

- The flag means "we need a count-check at loop entry, so we need an
  end-label to skip to". When `false`, the compiler statically
  proved the body runs at least once (hence
  `loopBodyRunsAtLeastOnce = !hasEndLabel`). The literal name
  "hasEndLabel" describes a side-effect (does the asm have an end
  label), not the underlying decision (do we need an entry-time
  count check).

Judgement:

- Unsure: the name is technically accurate to its mechanical effect
  but obscures the semantic decision. The read at line 8939 is the
  giveaway — using `!hasEndLabel` to mean "body runs at least once"
  is the inverse of what the name suggests at first glance.

Proposals:

- `needsCountCheck` (low) — captures the decision, not its effect.
- `loopMayBeSkipped` (low) — same idea, different framing.
- keep current (medium) — defensible as a recon-side effect-name.

Locations consulted:

- src/seqc_ast_nodes_evaluate.cpp:8715, 8830, 8901, 8912, 8920,
  8939, 8976

---

### `SeqCForLoop::evaluate::hasErrorOrNull`  [yes / low]

Evidence:

- src/seqc_ast_nodes_evaluate.cpp:9941
  `int hasErrorOrNull = 0;  // accumulated error flag`
- The variable is never written or read after initialization in the
  visible source (lines 9941-10027).

Interpretation:

- A declared-but-unused recon variable. The name advertises a role
  ("accumulated error flag, ORed with null-result detection") that
  it never plays in the visible code.

Judgement:

- Yes, it is a misnomer in the same sense as `lastEval` above:
  the name describes a role the variable never fulfils.

Proposals:

- delete (medium) — recon-only cleanup, not a rename.
- keep current (low) — only if synthesis prefers stability.

Locations consulted:

- src/seqc_ast_nodes_evaluate.cpp:9941

---

### `SeqCForLoop::evaluate::r`  [unsure / low]

Evidence:

- src/seqc_ast_nodes_evaluate.cpp:10095-10101
  ```cpp
  {
      auto r = result.get();
      r->assemblers_.insert(
          r->assemblers_.end(),
          incrResult->assemblers_.begin(),
          incrResult->assemblers_.end());
  }
  ```
- Single-use scoped alias for `result.get()`.

Interpretation:

- A single-letter local with no signal beyond "raw pointer to the
  thing we just dereferenced". The other 12 occurrences of inserting
  into `result->assemblers_` in this function (e.g. lines 10040-10043,
  10052-10055, 10086-10090) just write `result->assemblers_.insert(...)`.

Judgement:

- Unsure: not actively misleading, but inconsistent with the rest
  of the function's idiom.

Proposals:

- inline `result.get()` (low) — match surrounding idiom.
- keep current (low).

Locations consulted:

- src/seqc_ast_nodes_evaluate.cpp:10095-10101

---

### `SeqCFunctionCall::evaluate::funcScope`  [no / medium / not-misnomer]

Recorded as positive evidence because the same
`std::shared_ptr<Resources>` is passed under several different names
through the call graph and the consistent naming here is helpful.

Evidence:

- src/seqc_ast_nodes_evaluate.cpp:6435 —
  `auto funcScope = func->scope;` (after looking up the function).
- Used as `funcScope->getRegister(paramName)` (6475),
  `funcScope->updateCvar/Const/Wave/String(...)` (6503-6545),
  `funcScope->getRegisterNumber()` (6572),
  `funcScope->setReturnReg(regNum)` (6573),
  `bodyNode->evaluate(funcScope, ctx, state)` (6589),
  `funcScope->getReturnValue() / getReturnReg()` (6701-6702).
- The field this is sourced from, `Function::scope`, is itself the
  `Resources` sub-scope created when the function was registered
  (see `SeqCFunction::evaluate` at line 9766 calling
  `func->scope->updateParent(res)`).

Interpretation:

- The shared_ptr<Resources> is consistently named `funcScope` here,
  matching the role it plays (the registers/variables/labels scope
  of the function being called). Better than the generic `scope` or
  `resources` the local could have used.

Judgement:

- Not a misnomer.

Proposals:

- keep current (high)

Locations consulted:

- src/seqc_ast_nodes_evaluate.cpp:6435 and downstream uses
  6475-6705

---

## 4. Symbols inspected and judged routinely fine

### Method names (tier-1 §3 excluded — bullets only for completeness)

- `SeqCReturnStatement::evaluate(3)` — in nm.
- `SeqCArgList::evaluate(3)`, `SeqCDeclList::evaluate(3)`,
  `SeqCStmtList::evaluate(3)`, `SeqCFunctionCall::evaluate(3)`,
  `SeqCArray::evaluate(3)`, `SeqCIfCondition::evaluate(3)`,
  `SeqCCaseEntry::evaluate(3)`, `SeqCSwitchCase::evaluate(3)`,
  `SeqCWhileLoop::evaluate(3)`, `SeqCDoWhile::evaluate(3)`,
  `SeqCRepeat::evaluate(3)`, `SeqCIfElse::evaluate(3)`,
  `SeqCCondExpr::evaluate(3)`, `SeqCFunction::evaluate(3)`,
  `SeqCForLoop::evaluate(3)` — all in nm.
- `SeqCSwitchCase::hasCases / isSingleCase / singleCase / cases /
  evalCases` — all in nm.
- `(anon) evalCaseBody` — in nm as
  `(anonymous namespace)::evalCaseBody(...)`.

### Routine type-faithful locals (one-line each; not exhaustive)

- All `result`, `bodyResult`, `condResult`, `ifResult`, `elseResult`,
  `argResults`, `countResult`, `incrResult`, `initResult`,
  `caseResult`, `labelResult`, `arrayResult`, `indexResult`,
  `liveResult`, `deadResult`, `matchedConstResult`,
  `matchedDefaultResult`, `cfResult`, `bodyEval` —
  `shared_ptr<EvalResults>` named for the AST-child whose product
  it stores. No use site contradicts.
- `*Label` strings (`endLabel`, `loopLabel`, `whileLabel`, `forLabel`,
  `doLabel`, `ifLabel`, `elseLabel`, `bodyLabel`, `caseLabel`,
  `defaultLabel`, `retLabel`) — each created via
  `Resources::newLabel(<descriptor>)` with the same descriptor
  string. Type-faithful.
- `*Asm` / `*LabelAsm` (`branchAsm`, `brAsm`, `brnzAsm`, `brzAsm`,
  `brzDef`, `brzEnd`, `brgzAsm`, `loopNodeAsm`, `loopLabelAsm`,
  `nopAsm`, `whileLabelAsm`, `doLabelAsm`, `forLabelAsm`,
  `bodyLabelAsm`, `endLabelAsm`, `elseLabelAsm`, `ifLabelAsm`,
  `defLabelAsm`, `wtrigAsm`, `suserAsm`, `labelAsm`) — each holds
  `AsmList::Asm`. Type-faithful.
- Register locals (`switchReg`, `caseReg`, `bodyReg`, `bodyZeroReg`,
  `defReg`, `defZeroReg`, `condReg`, `counterReg`, `countReg`,
  `dstReg`, `srcReg`, `returnReg`, `childReg`, `resultReg`,
  `ifReg`, `ifReg2`, `elseReg`, `elseReg2`, `retReg`, `zeroReg`)
  — all `AsmRegister`. Idiomatic.
- Loop counter locals (`iterCount`, `commaCount`, `count`,
  `countInt`, `numCases`, `argCount`, `paramCount`, `idx`, `i`,
  `j`, `regNum`, `regNum2`, `caseRegNum`, `bodyRegNum`,
  `defRegNum`, `switchRegNum`, `cyclesF3`, `cyclesB0`,
  `totalCycles`, `negCaseVal`, `readConstVal`, `defReadConstVal`,
  `intVal`, `lastInt`, `nextLineNr`, `savedLineNr`) — type-faithful.
- Boolean flags (`exists`, `hasError`, `hasLbl`, `validLbl`,
  `evalBody`, `firstBodyEmpty`, `hasDefault`, `normalExit`,
  `doEpilogue`, `usePad10`, `savedHasError`, `matchedHasError`,
  `savedInSwitch`, `savedInLoop`) — each a one-bit role described
  by its name; consumed once or twice. No contradiction.
- AST-child accessor results (`funNameNode`, `funNameVar`, `argNode`,
  `argNode2`, `condNode`, `bodyNode`, `argExprNode`, `argChildren`,
  `casesList`, `stmts`, `stmt`, `caseEntry`, `labelNode`, `elems`,
  `param`, `argVal`, `dc`, `wf`) — type-faithful pointers/refs.
- Strings (`funName`, `outSig`, `argTypeSig`, `fullSig`, `varName`,
  `argExprName`, `paramName`, `waveName`, `waveStr`, `strVal`,
  `vtStr`, `signature`, `msg`, `cycles*`, `overload`, `overloads`)
  — type-faithful.
- Sub-scope shared_ptrs (`subRes`, `condScope`, `bodyScope`,
  `bodySubScope`, `unrollScope`, `maybeScope`, `cvarRes`,
  `subScope`, `ifTrueScope`, `ifFalseScope`, `ifScope`,
  `elseScope`, `liveScope`, `deadScope`, `elseSubScope`) —
  `shared_ptr<Resources>` from `createSubScope("<descriptor>")`.
- Value-domain locals (`childVal`, `retVal`, `val`, `val2`,
  `caseVal`, `condVal`, `condValue`, `caseValue`, `dVal`, `iVal`,
  `arrayVal`, `arrayValue`, `indexValue`, `idxVal`, `defReadVal`,
  `lastVal`, `ifVal`, `elseVal`, `vt`, `cvt`, `varType`,
  `argType`, `paramType`, `returnType`, `savedReturnType`,
  `childVarType`, `returnVarType`, `wvSub`, `childSub`) —
  `Value` / `VarType` / `VarSubType` locals. Type-faithful.
- Anonymous-namespace `evalCaseBody`'s parameters
  (`caseEntry, results, subRes, condResult, ctx, state`) —
  same convention as the calling `evalCases`.
- `SeqCSwitchCase::evalCases` parameters (`subRes`, `condResult`,
  `ctx`, `state`) — same convention.
- `SeqCFunction::evaluate::ex` (caught CompilerException) — idiomatic.

## 5. Coverage

- **Fully covered:** all 16 `evaluate(3)` method definitions in the
  assigned range, plus the supporting helpers (`SeqCSwitchCase::
  hasCases / isSingleCase / singleCase / cases / evalCases`, and
  the anonymous-namespace `evalCaseBody`). Every named symbol
  declared inside those bodies (parameters + locals) is judged in
  §2 / §4, with `yes`/`unsure` rows expanded in §3.
- **Deferred:** none — the batch was completed in one pass.
- **Not covered (out of scope per RULES §2/§3):**
  - All 16 `evaluate(3)` method names — tier-1 in `nm --demangle`.
  - The five `SeqCSwitchCase` accessor names — tier-1.
  - `(anon) evalCaseBody` — tier-1 (file-static symbol present
    in nm).
  - Class names `SeqCReturnStatement`, `SeqCArgList`, `SeqCDeclList`,
    `SeqCStmtList`, `SeqCFunctionCall`, `SeqCArray`,
    `SeqCIfCondition`, `SeqCCaseEntry`, `SeqCSwitchCase`,
    `SeqCWhileLoop`, `SeqCDoWhile`, `SeqCRepeat`, `SeqCIfElse`,
    `SeqCCondExpr`, `SeqCFunction`, `SeqCForLoop`,
    `FrontendLoweringContext`, `FrontendLoweringState`, `Resources`,
    `EvalResults`, `EvalResultValue`, `Value`, `VarType`,
    `VarSubType`, `AsmList`, `AsmList::Asm`, `AsmRegister`,
    `Immediate`, `CompilerException`, `CustomFunctions(Value)?Exception`,
    `Node`, `Function`, `WaveformFront`, `SeqCCaseEntry`,
    `SeqCStmtList`, `SeqCAstNode`, `SeqCValue`, `SeqCVariable`,
    `SeqCVariableType` — tier-1; member-fields of these types are
    audited in their own batches (32, 34, 11/12, 04a, 09, 23, 28).
  - Field accesses on those types (`->messages`, `->asmCommands`,
    `->wavetable`, `->customFunctions`, `->channelGrouping`,
    `->strings`, `->inLoop_`, `->inSwitch_`, `->pad10_`,
    `->values_`, `->assemblers_`, `->name_`, `->node_`,
    `->hasError_`, `->waveformFront_`, `->arrayBacking_`,
    `->varType_`, `->varSubType_`, `->value_`, `->reg_`,
    `->next`, `->branches`, `->branchMaySkipAllBodies`,
    `->loopBodyRunsAtLeastOnce`, `->scope`, `->arguments`,
    `->returnType`, `->signal`, `->deviceConstants`,
    `->channels_`, `->length_`, `->samples_`,
    `->waveformGranularity`, `->waveformPageSize`,
    `->bitsPerSample`, `->secondaryName`) — these are all
    member-field references; the field names themselves are owned
    by other batches (34, 17, 12, 32, 14/16, 31, 09).
  - `ErrorMessageT` enumerators (`FuncNoName`, `FuncPredefined`,
    `FuncEmpty`, `FuncNoReturn`, `WaveformNotFound`,
    `ProgramTooLarge`, `ArrayIndexNeedConst`, `ArraysOnlyWave`,
    `NeedCaseBeforeStmt`) — owned by batch 08 (error_messages).
  - Constants `kSuserTriggerLoad` — owned by batch 01 (types).
  - `EDirection`, `Resources::State::Active` — owned by their
    own batches.
  - `SEQC_BINARY` accessors (`expr()`, `body()`, `cond()`,
    `arguments()`, `function()`, `funName()`, `array()`,
    `index()`, `params()`, `retType()`, `init()`, `incr()`,
    `ifBody()`, `elseBody()`, `value()`, `label()`, `count()`,
    `elements()`, `decls()`, `stmts()`, `call()`, `name()`,
    `varType()`, `lineNr()`, `type()`, `getVarTypes()`,
    `children()`) — generated AST accessors; their names are
    audited in batch 04a (header).

No `coordinated-rename` triggered within this batch alone. One
`cross-batch-arbitration` recorded:

- `SeqCStmtList::evaluate::childHadError` ↔ `EvalResults::hasError_`
  (batch 34): the local copies the field semantically and shares the
  same misnomer; arbitration deferred to synthesis. Whichever name
  batch 34 settles on for the field should propagate to this local.

One `verify-not-original` recorded:

- `SeqCArgList / DeclList / StmtList::evaluate::lineNr` — the local
  is fine; the underlying issue is in the `type()` accessor name in
  batch 04a, which the auditor could not modify here.
