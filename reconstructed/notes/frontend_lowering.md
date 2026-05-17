# Frontend Lowering {#notes_frontend_lowering}

\warning This page is being reworked into a user-facing reference
during Phase D7-B.  The current contents below describe the data
types flowing through `FrontEndLoweringFacade::lower()` and the
`SeqCAstNode::evaluate()` virtual — **EvalResults**, **EvalResultValue**,
**Value**, **LowerResult**, **FrontendLoweringState** — but still
include reconstruction-narrative material (phase IDs, addresses) that
will be stripped in Phase B5 / B6.

## Types reconstructed

### EvalResults (0x80 bytes)

New file: `include/zhinst/ast/eval_results.hpp`

Central result type of the frontend lowering pipeline. Every SeqC
built-in function and every `SeqCAstNode::evaluate()` virtual returns
`shared_ptr<EvalResults>`. CustomFunctions dispatch map is typed as
`unordered_map<string, function<shared_ptr<EvalResults>(
vector<EvalResultValue> const&, shared_ptr<Resources>)>>`.

Layout:
```
+0x00  24  vector<EvalResultValue>     values_
+0x18  24  vector<AsmList::Asm>        assemblers_
+0x30   1  bool                        hasError_
+0x31   7  (padding)
+0x38  16  shared_ptr<Node>            node_
+0x48  16  shared_ptr<WaveformFront>   waveformFront_
+0x58  24  std::string                 name_
+0x70  16  shared_ptr<EvalResults>     arrayBacking_
Total: 0x80 (128 bytes)
Emplace alloc: 0x98 = 0x18 control block + 0x80 payload
```

Key methods (14 total):
- Ctor(VarType): @0x176bc0 — creates single-element values_ vector
- Copy ctor: @0x231c60 — deep-copies vectors, bumps refcounts
- Dtor: @0x16f3d0 — destroys in reverse field order
- getValue(): @0x211ab0 — returns Value (extracted from last EvalResultValue)
- addAssembler(): @0x15c1b0 — pushes to assemblers_ (stride 0xa8)
- 9 setValue() overloads (addresses scattered across 0x15a750..0x247600)

The `arrayBacking_` field at +0x70 is `shared_ptr<EvalResults>` (self-
referential). SeqCArray::evaluate @0x211140 writes a new EvalResults
into parent ER+0x70 at 0x211798/0x2117a0. SeqCAssign::evaluate @0x243e60
propagates it if non-null (check at 0x243f13).

### Value layout correction (0x20 → 0x28)

**CRITICAL FIX** in `value.hpp`. The Value class was reconstructed with
wrong field offsets:

Before (WRONG):
```
+0x00  4  ValueType type_
+0x04  4  int32_t which_      ← WRONG offset
+0x08  24 Storage              ← WRONG offset
Total: 0x20 (32 bytes)         ← WRONG size
```

After (CORRECT):
```
+0x00  4  ValueType type_
+0x04  4  (padding)
+0x08  4  int32_t which_      ← 4B further than claimed
+0x0C  4  (padding)
+0x10  24 Storage              ← 8B further than claimed
Total: 0x28 (40 bytes)
```

Evidence: Value::~Value @0x15a9c0 reads `which_` from `[rdi+0x08]`
(not `[rdi+0x04]`) and checks SSO at `[rdi+0x10]` (not `[rdi+0x08]`).
The padding exists because the Storage union contains `double` (8B
alignment), forcing `which_` onto an 8B boundary.

Impact: All existing code that accessed `value.which_` and
`value.storage_` by field name was generating wrong offsets. The static
lib build never caught this because no actual binary linkage occurs
against the real `.so`. Corrected.

### EvalResultValue field rename

`EvalResultValue` in `custom_functions.hpp` changed from opaque
`field_00/field_08/which_/data_/field_30` to:
```
+0x00  4  VarType     varType_
+0x04  4  VarSubType  varSubType_
+0x08  40 Value       value_      (embedded, 0x28 bytes)
+0x30  8  AsmRegister reg_        (default: value=-1, valid=false)
```
The dtor now delegates to `Value::~Value()` instead of duplicating the
free logic with raw pointer arithmetic.

### FrontendLoweringState::result resolved

Changed from `shared_ptr<void>` (type TBD) to `shared_ptr<Node>`.
Evidence: lower() @0x1c1fb6 copies state.result into sret[0], and the
caller Compiler::compile @0x11f92f stores sret[0] into `Compiler+0x28`
which is declared as `shared_ptr<Node> ast_`. Type must match.

### lower() return type corrected

Changed from `void` to `FrontEndLoweringFacade::LowerResult` (32B sret):
```
struct LowerResult {
    shared_ptr<Node>        astResult;    // from FrontendLoweringState.result
    shared_ptr<EvalResults> evalResult;   // from evaluate virtual sret
};
```

Previously claimed 64B / 4 shared_ptrs — that was WRONG. Only 32B /
2 shared_ptrs. The extra cleanup in the caller was for other stack
locals, not sret.

The `lower()` body in compiler.cpp now constructs FrontendLoweringContext
and FrontendLoweringState, with a TODO for wiring up the virtual call
once SeqCAstNode's evaluate vtable entry is declared.

## Files changed

### New
- `reconstructed/include/zhinst/ast/eval_results.hpp` — full EvalResults layout + 14 method decls
- `reconstructed/notes/frontend_lowering.md` — this file

### Modified
- `include/zhinst/ast/value.hpp` — layout corrected (0x20→0x28), static_assert updated
- `include/zhinst/runtime/custom_functions.hpp` — EvalResultValue fields renamed; added includes for value.hpp and asm_register.hpp
- `include/zhinst/ast/frontend_lowering.hpp` — FrontendLoweringState.result typed as shared_ptr<Node>; added Node forward decl
- `include/zhinst/codegen/compiler.hpp` — lower() return type changed to LowerResult; LowerResult struct added; EvalResults forward decl added
- `src/codegen/compiler.cpp` — lower() body partially implemented (Context+State setup, return); included frontend_lowering.hpp
- `src/runtime/custom_functions.cpp` — EvalResultValue dtor delegates to Value::~Value()

## Build

```
$ cmake --build .
[100%] Built target zhinst_seqc
```

One pre-existing warning (placement new for libc++ string into 24B Storage — documented ABI mismatch, not actionable).

## Remaining work

The TODO items for Phase 15a-i are fully addressed:
- [x] EvalResults layout
- [x] EvalResultValue layout (was already 0x38, now with real field names)
- [x] lower() sret aggregate type (corrected from 64B claim to 32B)
- [x] FrontendLoweringState::result pointee (shared_ptr<Node>)

Open ends that feed into future phases:
- SeqCVariable::evaluate @0x209ea0 is 3712 bytes — very large, needs dedicated sub-phase.
- Operator 5-arg evaluate bodies (22 implementations, SeqCPlus alone is 7344B).
- The method bodies for EvalResults (ctor, copy ctor, dtor, getValue, 9 setValues, addAssembler) are declared but not implemented. Low priority — the declarations are sufficient for type-checking.

## Phase 21h — evaluate() body reconstruction

### Complete evaluate() address map (from binary symbols)

**3-arg evaluate overrides (32 implementations):**

| Class | Address | Size | Status |
|-------|---------|------|--------|
| SeqCVariableType | 0x209d10 | ~0xA0 | declared |
| SeqCCommand | 0x209db0 | 0x10 (stub) | declared |
| SeqCAstNode (base) | 0x209dc0 | 0x90 | **implemented** |
| SeqCOperation | 0x209e50 | 0x10 (stub) | declared |
| SeqCVariable | 0x209ea0 | 0xE80 | **implemented** |
| SeqCFunction | 0x20b200 | 0x14A0 | **done** |
| SeqCFunctionCall | 0x20c6a0 | 15220B | **done** — two paths: custom (CustomFunctions::call) and user-defined (getFunction, param binding 5-way switch, body evaluate, return value extraction). Disasm-audited: Var/Cvar split, return guard, error arg order, CFVE handler all corrected. |
| SeqCOperator (template) | 0x210aa0 | ~0x5AE | **implemented** |
| SeqCParamList | 0x211050 | — | declared |
| SeqCArray | 0x211140 | 2412B | **done** — NOT ICF'd. Array indexing: validates Wave+Const/Cvar operands, waveform lookup via getWaveformByName, bounds check (padded_length*channels*bitsPerSample/8), returns wave-with-index + Cvar sample value. |
| SeqCArgList | 0x211de0 | — | declared |
| SeqCDeclList | 0x2122f0 | — | declared |
| SeqCStmtList | 0x212800 | — | declared |
| SeqCLabel | 0x2130d0 | — | declared |
| SeqCValue | 0x213140 | 0x560 | **implemented** |
| SeqCIfCondition | 0x2138e0 | 0x1108 | **implemented** (Var+Const/Cvar paths, jumpIfZero helper) |
| SeqCIfElse | 0x214d50 | 0x1C2E | **implemented** (Var+Const/Cvar paths, 3-label structure) |
| SeqCSwitchCase | 0x217a80 | 11506 | **done** — 3-way dispatch (Var/Const+Cvar/error), hasCases/isSingleCase/singleCase/cases helpers + evalCaseBody @0x216fc0 + evalCases @0x216980 all implemented |
| SeqCCaseEntry | 0x21aa40 | 0xB1D | **implemented** (inSwitch_ guard, dynamic_cast label validation) |
| SeqCForLoop | 0x21b680 | 9794B | **done** — two-path: Cvar (toDouble+floatEqual unroll with node chaining, re-eval init/cond/incr each iter, iter limit 0x7b); Var (body+end labels, jumpIfZero, asmLoopNode, loopBodyRunsAtLeastOnce=(condNode==null\|\|jumpAsms.empty()), for-body sub-scope, br(forLabel), loopArgNodeAppend for init+cond+incr, loopBodyNodeAppend for body) |
| SeqCWhileLoop | 0x21e130 | 0x1BCD | **implemented** (Var+Cvar paths, loopArgNodeAppend/loopBodyNodeAppend helpers) |
| SeqCDoWhile | 0x21fd00 | 7952B | **done** — body-first, toDouble+floatEqual Cvar, brnz/br Var, loopBodyRunsAtLeastOnce=true |
| SeqCRepeat | 0x221c10 | ~250 | **done** — two-path: Cvar (count toInt, unroll with maybe_unroll+unroll scopes, atScopeBoundary, node chaining); Var (brz+asmLoopNode+addi(-1)+brgz counter pattern). Error 0xb7 for negative count, 0x7b for iter limit. |
| SeqCCondExpr | 0x223d90 | 11007 | done |
| SeqCContinueStatement | 0x226890 | — | declared |
| SeqCBreakStatement | 0x226970 | — | declared |
| SeqCReturnStatement | 0x226a50 | 0x1A90 | **implemented** (5-way return type dispatch) |
| SeqCNeg | 0x2284e0 | — | **implemented** |
| SeqCPos | 0x228e80 | — | declared |
| SeqCInv | 0x228f50 | — | **implemented** |
| SeqCNotExpr | 0x229980 | — | declared |
| SeqCNoCmd | 0x22a560 | — | declared |

**5-arg evaluate overrides (22 operator subclasses):**

| Class | Address | Status |
|-------|---------|--------|
| SeqCOperator (base) | 0x210a90 | **implemented** (returns nullptr) |
| SeqCPlus | 0x22a600 | **implemented** |
| SeqCMinus | 0x22cde0 | declared |
| SeqCMult | 0x22ea70 | declared |
| SeqCDiv | 0x231070 | declared |
| SeqCMod | 0x231ec0 | **done** |
| SeqCShiftR | 0x232630 | declared |
| SeqCShiftL | 0x235690 | declared |
| SeqCGreater | 0x2358b0 | declared |
| SeqCLower | 0x2371b0 | declared |
| SeqCLEqual | 0x238c60 | declared |
| SeqCGEqual | 0x2395f0 | declared |
| SeqCEqual | 0x2399c0 | declared |
| SeqCNEqual | 0x23ba00 | declared |
| SeqCInc | 0x23bd50 | declared |
| SeqCDec | 0x23d2b0 | declared |
| SeqCAndExpr | 0x23e810 | declared |
| SeqCOrExpr | 0x240820 | declared |
| SeqCXorExpr | 0x2427b0 | declared |
| SeqCLogAnd | 0x242820 | implemented |
| SeqCLogOr | 0x243840 | implemented |
| SeqCAssign | 0x243e60 | declared |
| SeqCNoOp | 0x246270 | declared |

### combine(VarType, VarType) @0x247f50

7×7 lookup matrix from .rodata at 0x95c2b8..0x95c37c. Row 0 (Unset)
passes through rhs. Throws `VarTypeException` with error 0x91 if
either arg > 6. Matrix is NOT symmetric (e.g. Const+String=Wave but
String+Const=String). Full table in `seqc_ast_nodes_evaluate.cpp`.

### SeqCValue::Tag enum (corrected)

- eString = 0 (not 3)
- eDouble = 1 (not 2)
- -1 = empty/none (dtor skips destruction)

Evidence: `cmp eax,0x1; je double_path; test eax,eax; jne bad_variant_access`
at 0x2131d8.

### AsmCommands+0x50 = wavetableFrontIndex_

Binary writes `lineNr` to `[ctx.asmCommands.get()+0x50]` in evaluate
prologue. This is `AsmCommands::wavetableFrontIndex_` (already named
in asm_commands.hpp). Setter: `setWavetableFrontIndex(int)`.

Used in: SeqCOperator::evaluate @0x210ad7, SeqCValue::evaluate @0x21316c.

### SeqCVariable::evaluate @0x209ea0 (3712 bytes — largest evaluate)

**Two cascaded jump tables** dispatch on `varType_` then on lookup result:

1. **Outer table @0x95b894** — 7 entries indexed by `this->varType_`:
   - 0 (Unset) → 0x209fa5: call `getVariableType(name)` and dispatch
                via inner table.
   - 1 (Void)  → 0x20a058: emit `ErrorMessageT(0xE0)` with name.
   - 2 (Var)   → 0x20a089: `addVar(name, Stub)` + `getRegister(name)` +
                `setValue(Var, reg)`.
   - 3 (Str)   → 0x20a012: `addString(name, Stub)` + `setValue(String)`.
   - 4 (Const) → 0x20a0bd: `addConst(name, Stub)` + `setValue(Const)`.
   - 5 (Wave)  → 0x20a0e0: `wavetable.newEmptyWaveform(name)` →
                `addWave(name, wfName)` + `setValue(Wave, wfName)`.
   - 6 (Cvar)  → 0x20a035: `addCvar(name, Stub)` + `setValue(Cvar)`.

2. **Inner table @0x95b8b0** — 5 entries indexed by `(returnedVarType - 2)`:
   - 0 (Var)   → 0x209fd0: if `direction_!=0 && !atScopeBoundary` →
                `checkVar(name)`; then `getRegister` + `setValue(Var, reg)`.
   - 1 (Str)   → 0x20a36b: `readString(name, abs(direction))` → push.
   - 2 (Const) → 0x20a253: `readConst(name, abs(direction))` → push.
   - 3 (Wave)  → 0x20a2df: `readWave(name, abs(direction))` → push.
   - 4 (Cvar)  → 0x20a1c7: `readCvar(name, abs(direction))` → push.

**EDirection passed to read* is `abs(direction_)`** — branchless via
`mov edx,ecx; sar edx,31; xor edx,ecx` @0x20a232 etc.

**Inline EvalResultValue construction (4 nearly-identical sequences,
~290B each)**: After each `read*` returns by sret to `[rbp-0x60]`, the
binary:
1. Allocates fresh 56B `EvalResultValue` on heap.
2. Copies `varType_/varSubType_` from sret into `[new+0x00..0x07]`.
3. Uses a per-path inner jump table (one of @0x95b8c4 readWave,
   @0x95b8d4 readString, @0x95b8e4 readCvar, @0x95b8f4 readConst)
   keyed on `abs(value.which_)` to copy the Value payload at
   `[new+0x18]` (Int=4B, Bool=1B, Double=8B, String=16B+ptr).
4. Stores `ValueType` into `[new+0x10]` and `AsmRegister` into `[new+0x30]`.
5. Replaces `result->values_` (vector) contents: destroys old strings
   in any old elements, frees old buffer, sets `begin/end/cap` to point
   to the new single-element buffer at `[ptr, ptr+0x38, ptr+0x38)`.

This is observationally equivalent to `result->values_.assign(1, std::move(rv))`
where `rv` is the EvalResultValue returned by `read*`. The
reconstruction collapses the inline pattern to that semantics.

**Common tail @0x20a793-20a843**: copies `name` into `result->name_`
(EvalResults +0x58), distinguishing SSO vs heap path; deletes the
local `name` string; returns the shared_ptr.

**New accessor**: `Resources::atScopeBoundary()` returns
`(flags_88_ & 0xFF) != 0` — wraps the `cmp BYTE PTR [rdi+0x88], 0`
gate at @0x209fda used by the Var read path.

### SeqCAssign::evaluate (5-arg) @0x243e60 (5552 bytes — second-largest evaluate)

**Function signature** (from arg slot mapping). Because the return is
a non-trivial 16-byte `shared_ptr<EvalResults>`, the Itanium ABI uses
a hidden first arg for the return slot, shifting all other args by one
register:
```
shared_ptr<EvalResults> evaluate(
    /* hidden retptr,                    rdi → r15 */
    /* implicit this (SeqCAssign*),      rsi → r14 */
    shared_ptr<Resources> res,        // rdx → r13   (passed by hidden ref under ABI)
    FrontendLoweringContext& ctx,     // rcx → [rbp-0x110]
    FrontendLoweringState& state,     // r8  (used briefly, then reloaded from stack)
    EvalResults const& lhsRes,        // r9  → r12 (initial), then aux-copy
    EvalResults const& rhsRes) const; //     → [rbp+0x10]   (5th non-this arg, on stack)
```

**Stack frame**: 0x268 bytes. Spills `ctx*` to `[rbp-0x110]`,
the `aux` shared_ptr<EvalResults> (a copy of **lhsRes**, used internally
for waveformFront/arrayBacking propagation without mutating the input)
to `[rbp-0x118/-0x120]`, and the returned `result` shared_ptr to
`[rbp-0x30]`. Many error-path string
locals at `[rbp-0x148]`, `[rbp-0x170]`, `[rbp-0x188]`, `[rbp-0x1a0]`,
`[rbp-0x1b8]`, `[rbp-0x1d0]`, `[rbp-0x1e8]`, `[rbp-0x200]`, `[rbp-0x218]`,
`[rbp-0x230]`, `[rbp-0x248]`, `[rbp-0x260]`. Local
`shared_ptr<WaveformFront>` at `[rbp-0x130]` (Wave path) and `[rbp-0x58]`
(other Wave subpaths). Catch landing pads jump to
`[rbp-0x30] (=result) ... → _Unwind_Resume` at @0x2461bd-2461cf.

**Prologue** (0x243e60..0x243f64):
1. Allocate `result = make_shared<EvalResults>()` → spilled to `[rbp-0x30]`.
2. Allocate `aux = make_shared<EvalResults>(lhsRes)` (copy ctor sourced
   from r12 = r9 = lhsRes) → spilled to `[rbp-0x120/-0x118]`. The aux
   copy exists so the function can safely look up
   `lhsRes.values_[0].varType_/.varSubType_` and propagate
   `arrayBacking_`/`waveformFront_` into the result without mutating
   the caller's lhsRes.
3. Conditionally (when `[rhsRes+0x70] != 0`) overwrite the aux's
   `arrayBacking_` shared_ptr (+0x70) with the rhsRes equivalent (a
   `shared_ptr<EvalResults>` refcount inc on rhs and dec on aux's
   prior value). NOTE: confirmed as `arrayBacking_` propagation —
   the +0x70 offset matches `shared_ptr<EvalResults> arrayBacking_`
   per the EvalResults header layout.
4. dynamic_cast<SeqCVariable*>(this->lhs()) at @0x243f84 — used later
   for the `name` operand to `Resources::updateX()` calls. The cast
   is to **SeqCVariable** specifically (not SeqCOperation as I first
   guessed), and the result is used as `&lhsVar->name_` (the name
   string lives at offset +0x18 in SeqCVariable, hence the
   `add r12, 0x18` seen before each `update*` call).

**Try-block dispatch** (0x243f64..0x245a06): a flat cascade of paired
`(lhsType, rhsType)` tests. The lhsType is read at `[rax-0x38]`
(`aux.values_.back().varType_`, where `aux` is the lhsRes copy and
`rax = aux.values_.end()`); the rhsType is read by re-loading rhsRes
from `[rbp+0x10]`, then walking `rhsRes.values_` and reading
`[rcx-0x38]` (last value's varType). The `or ecx,0x2; cmp ecx,6`
idiom matches `rhsType ∈ {Const(4), Cvar(6)}` — important: NOT
`{Var,Cvar}`. Verify: `2|2=2`, `4|2=6`, `6|2=6`. So Var(2) is
explicitly excluded by that idiom.

The cascade structure is **outer dispatch on lhsType, inner test on
rhsType compatibility**: the binary tries pairs in source order and
the first matching pair wins.

**Per-pair action table** (the binary tries each row in order; first
match wins; mismatched pairs that drop to error paths emit `0x8b`):

| # | lhsType | lhsSubType | rhsType | Action | Address |
|--:|---------|-----------|---------|--------|---------|
| 1 | Var(2)  | any       | Const(4) or Cvar(6) | `updateVar(name)`; **no asm copy** | 243fca→24400a |
| 2 | Var(2)  | any       | Var(2)              | `updateVar(name)` + `__insert_with_size` to merge `rhsRes.assemblers_` into `result->assemblers_` | 24403e→244085 |
| 3 | Const(4)| any       | Const(4) or Cvar(6) | `updateConst(name, rhs.toDouble(), lhs.varSubType, false)` (Value extracted via per-which jump table @0x95c1cc into Value scratch at `[rbp-0xf0]`) | 2440f4→245047 |
| 4 | Cvar(6) | any       | Const(4) or Cvar(6) | `updateCvar(name, rhs.toDouble(), lhs.varSubType)` (jump table @0x95c1ec) | 244174→2451ee |
| 5 | String(3)| any      | String(3)           | `updateString(name, rhs.toString(), lhs.varSubType)` + `result->setValue(String, sub, value)` (jump table @0x95c20c, jump table @0x95c21c for sub-resolution) | 2441f4→245651 |
| 6 | Wave(5) | Numeric(3)| Wave(5)             | `WavetableFront::copyWaveform(rhsRes.waveformFront_)` → local; decrement `WaveformFront +0xd8` refcount; `dynamic_cast<SeqCArray*>(this->lhs())` → `array()` → `Resources::updateWave(name, arrayName, lhs.varSubType)`; set `WaveformFront +0xdc = 1` | 24426f→244546 |
| 7 | Wave(5) | non-Numeric| Wave(5)            | **Error 0x8b** with `str(Const)` & `str(rhs.varType_or_Unset)` | 2442b9→2449a3 |
| 8 | Wave(5) | Numeric(3)| String(3)           | **Error 0x8b** with `str(String)` & `str(Const)` (mismatch: Wave[Numeric] can't take String rhs) | 24438e→244398 |
| 9 | Wave(5) | Numeric(3)| Var(2) or Cvar(6)   | In-place numeric write: `wf = result->waveformFront_`; `idx = lhsRes.value.toInt()`; `val = rhsRes.value.toDouble()`; `wf->numericData_[idx] = val`; `wf->validMask_[idx] = 0`; `wf->[+0xdc] = 1` | 244468→244c0d |
| 10| Wave(5) | FunctionArg(2)| any (no rhs check) | `WavetableFront::newEmptyWaveform(name)` → local wf; `Resources::updateWave(name, wfName, Default)`; `result->setValue(Wave, FunctionArg, Value(wfName))` | 244a58→244a8a |
| 11| Wave(5) | other     | any                 | `name = rhs.toString()`; `if (!WavetableFront::waveformExists(name))` then **error 0xe9** with `str(name)` (1-arg format); else `Resources::updateWave(name, rhsName, rhs.varSubType)` + `result->setValue(Wave, rhs.varSubType, rhsValue)` | 244e3a→244ea0 |
| 12| Unset(0)| —         | any                 | Silent no-op → jump straight to cleanup tail @0x245aa7 | 24454b→245aa7 |
| 13| any (default) | — | non-Unset, non-empty | **Error 0x8b** with `str(lhsType)` & `str(rhsType_or_Unset)` (general fallback) | 244593→24458c |

**Empty/size>1 guards**: every pair test first checks
`rhsRes.values_.size() <= 1` (using the `÷56` magic-multiply pattern
`movabs rdi, 0x6db6db6db6db6db7; imul rdx,rdi; cmp rdx,1`). If
`values_.size() > 1`, the dispatch falls through past all pair tests
(treating it as "no match" → error path). If `values_.empty()`, also
falls through.

**Rows 3-5 (Const/Cvar/String) — CORRECTION (2026-04-24)**: earlier
analysis claimed these rows emit `addi(R0, R0, ...)` and
`asmSetVarPlaceholder(R0)` instructions. This was **completely wrong**.
Binary re-analysis confirmed that rows 3-5 perform **no ASM emission
at all**. They are compile-time constant assignments:
- Row 3 (Const): `updateConst()` → `result->setValue(VarType_Const, lhsSub, value)`
- Row 4 (Cvar): `updateCvar()` → `result->setValue(VarType_Cvar, lhsSub, value)`
- Row 5 (String): `updateString()` → `result->setValue(VarType_String, lhsSub, value)`
All three share a common tail: `result->name_ = name + " = " + rhsResult.name_`
@0x24594a. The addresses previously cited as addi/asmSetVarPlaceholder
call sites are actually in Row 1/2 (Var paths) or later rows.

**Catch handler for VarTypeException** (@0x246041..0x2461a8):
```
catch (VarTypeException& e) {
    string msg = e.msg_;  // SSO or heap path
    ctx.compilerMessages->errorMessage(msg, -1);
    // Re-extract type info from RHS (not lhs) so the error result
    // carries the rhs's type forward for downstream lowering:
    VarType    rt = rhsRes.values_.empty() || rhsRes.values_.size()>1
                    ? VarType_Unset : rhsRes.values_.back().varType_;
    VarSubType rs = (same guard) ? VarSubType_Default
                                 : rhsRes.values_.back().varSubType_;
    result->setValue(rt, rs, rhsRes.getValue());
    // (then continue to common cleanup tail)
}
```

This means even on type-mismatch errors, the result EvalResults gets
the **rhs** type info populated so downstream lowering can continue
with type recovery from the rvalue side.

**Common cleanup tail** @0x245aa7..0x245aeb:
- Releases the `aux` (lhsRes-copy) shared_ptr<EvalResults> at `[rbp-0x118]`.
- Returns `result` from `[rbp-0x30]` via the hidden retptr at `[rbp-0x30]`.
- 0x268-byte stack adjust + pop r15/r14/r13/r12/rbx/rbp + ret.

**Inner jump table addresses** (for which→Value-payload-copy):
- `0x95c1bc` — Var-asm immediate value-type dispatch (4 entries: Int/Bool/Double/String)
- `0x95c1cc` — Const value-type dispatch
- `0x95c1dc` — Const post-update value-type dispatch
- `0x95c1ec` — Cvar value-type dispatch
- `0x95c1fc` — Cvar post-update value-type dispatch
- `0x95c20c` — String value-type dispatch
- `0x95c21c` — String sub-resolution post-update jump table

All keyed on `abs(value.which_)` (sign-collapsed via
`mov edx,ecx; sar edx,31; xor edx,ecx`). Payload sizes: Int=4B,
Bool=1B, Double=8B, String=16B+ptr.

**WaveformFront layout discoveries** (from row 9 in-place write path):
- `WaveformFront +0x80` = `double* numericData_` (write `[rcx+rax*8]=xmm0`)
- `WaveformFront +0x98` = `bool* validMask_` (write `[rcx+rax*1]=0`)
- `WaveformFront +0xd8` = `int refcount_or_dirty_` (decremented in
  copyWaveform path; gates other behavior)
- `WaveformFront +0xdc` = `bool dirty_` (set to 1 after numeric write
  AND after copyWaveform completes)

### SeqCPlus::evaluate (5-arg) @0x22a600 (7329 bytes — third-largest evaluate)

**Function signature**: same sret+this register shift as SeqCAssign
(rdi=retptr, rsi=this, rdx=res, rcx=ctx, r8=state, r9=lhsResult,
[rbp+0x10]=rhsResult).

**Key design difference from SeqCAssign**: `this` (rsi) is NEVER read —
no member-variable access. Error code is **0x73** (not 0x8b). The operator
does not write back to variables; it only produces a new EvalResults.

**Size guard**: if `lhsResult.values_.size() == 0` or `> 1`, skips ALL
rows and emits error 0x73 with `lhsType=VarType_Unset`. Same guard
applies to rhsResult within each row. The `rcx` register holding the
lhs size is preserved across the entire cascade for repeated checks.

**Per-pair action table** (8 rows + default error path):

| # | lhsType | rhsType | Action | Address |
|--:|---------|---------|--------|---------|
| 1 | Var(2) | Const/Cvar | merge lhs.asm; getRegisterNumber(); setValue(Var,regNum); addi(resultReg,lhsReg,rhsValue) | 22a6bc |
| 2 | Const/Cvar | Var(2) | merge rhs.asm; getRegisterNumber(); setValue(Var,regNum); addi(resultReg,rhsReg,lhsValue) | 22a778 |
| 3 | Var(2) | Var(2) | merge both.asm; getRegisterNumber(); setValue(Var,regNum); addi(resultReg,lhsReg,Imm(0)); addr(resultReg,rhsReg) | 22a82d |
| 4 | Const/Cvar | Const/Cvar | combine(VarType), combine(VarSubType); lhs.toDouble()+rhs.toDouble(); setValue(combined,combinedSub,Value(sum)) | 22a910 |
| 5 | String(3) | String(3) | lhs.toString()+rhs.toString() concatenation; combine(VarSubType); setValue(String,combinedSub,Value(concat)) | 22aa25 |
| 6 | Wave(5) | Wave(5) | combineWaveforms("add", lhs, rhs, ctx) | 22aa88 |
| 7 | Wave(5) | Const/Cvar | FunctionArg→passthrough; else constWaveform(len,rhsDouble,ctx)+combineWaveforms("add",lhs,constWf,ctx) | 22ab83 |
| 8 | Const/Cvar | Wave(5) | FunctionArg→passthrough; else constWaveform(len,lhsDouble,ctx)+combineWaveforms("add",constWf,rhs,ctx) | 22ac4d |
| dflt | any | any | error **0x73** via str(lhsType), str(rhsType), errorMessage(msg, -1) | 22acbc |

**Common tail @0x22adb3**: `result->name_ = lhsResult.name_ + " + " + rhsResult.name_`
(literal 0x202b20 = " + ").

**Row 3 Var+Var detail**: binary emits `addi(resultReg, lhsReg,
Immediate(0))` as a MOV, then `addr(resultReg, rhsReg)` to add. This is
a 2-instruction sequence: copy lhs into fresh register, then add rhs.
After the addi merge, registers are re-read from `values_.back().reg_`
(because the vector insert may have reallocated).

**Anonymous-namespace helpers** (bodies deferred):
- `combineWaveforms` @0x22c300: `(sret, string const& opName, EvalResults const& lhs, EvalResults const& rhs, FrontendLoweringContext& ctx)` → `shared_ptr<EvalResults>`. For SeqCPlus opName="add". Likely "sub"/"mul"/"div" for other operators.
- `constWaveform` @0x22c9f0: `(sret, int sampleLength, xmm0=double value, FrontendLoweringContext& ctx)` → `shared_ptr<EvalResults>`. Creates constant rectangular waveform via WaveformGenerator.
- `scaleWaveform` @0x228cc0: `(int scaleFactor, shared_ptr<EvalResults>, FrontendLoweringContext&)` → `shared_ptr<EvalResults>`. Negates a waveform's samples. Used by SeqCMinus for Wave subtraction (negate rhs before adding). ABI discrepancy noted: at both call sites (22d227 and 22e5c4), `esi` (the int parameter) is NOT explicitly set — the shared_ptr ref occupies rsi and ctx& occupies rdx. The `int` may be dead/unused.

**New declaration**: `combine(VarSubType, VarSubType)` @0x247ea0 — analogous
to `combine(VarType, VarType)` but for VarSubType. Lookup table from .rodata
not yet extracted (stubbed).

**Unique called functions** (zhinst symbols only):
- `Resources::updateVar/updateConst/updateCvar/updateString/updateWave`
- `WavetableFront::newEmptyWaveform/copyWaveform/waveformExists`
- `EvalResults::EvalResults(const&)`, `getValue()`,
  `setValue(VarType, VarSubType, Value const&)`
- `Value::Value(string const&)`, `toDouble()`, `toInt()`, `toString()`,
  `~Value()`
- `Immediate::Immediate(int)`, `~Immediate()`
- `AsmRegister::AsmRegister(int)`
- `AsmCommands::addi(AsmRegister, AsmRegister, Immediate) const`
- `AsmCommands::addi(AsmRegister, AsmRegister, Value) const`
- `AsmCommands::asmSetVarPlaceholder(AsmRegister) const`
- `Assembler::Assembler(const&)`, `~Assembler()`,
  `AsmList::~AsmList()`, `AsmList::Asm::~Asm()`
- `SeqCOperator::lhs()`, `SeqCArray::array()`
- `ErrorMessages::format<string,string>(0x8b, ...)` and
  `format<string>(0xe9, ...)`
- `CompilerMessageCollection::errorMessage(string, int)`
- `str(VarType)`

### SeqCMinus::evaluate (5-arg) @0x22cde0 (7312 bytes)

**Function signature**: same sret+this register shift as other operators
(rdi=retptr, rsi=this, rdx=res, rcx=ctx, r8=state, r9=lhsResult,
[rbp+0x10]=rhsResult).

**Key design differences from SeqCPlus**:
- Error code is **0x74** (not 0x73)
- **No String+String row** — string subtraction is not supported
- Var-Var uses **subr** instead of addr
- Var-Const/Cvar **negates** rhs value via `neg eax` then uses
  `addi(resultReg, lhsReg, Immediate(-rhsInt))`
- Const/Cvar-Var uses **two-step**: `addi(resultReg, R0, lhsValue)` then
  `subr(resultReg, rhsReg)` — cannot use single addi due to non-commutativity
- Wave-Wave uses **scaleWaveform** to negate rhs waveform before
  `combineWaveforms("add", lhs, scaledRhs, ctx)`
- Wave-Const/Cvar **negates rhsDouble** via `xorpd xmm0,[signbit]` @0x22e491
  then `constWaveform(len, -rhsDouble, ctx)` + `combineWaveforms("add")`
- Const/Cvar-Wave: `constWaveform(len, lhsDouble, ctx)` (NO negation of lhs),
  then **scaleWaveform** to negate rhs wave, then `combineWaveforms("add")`
- **No FunctionArg passthrough** in any Wave path (unlike SeqCPlus rows 7-8)
- Common tail: `" - "` literal (0x202d20) instead of `" + "`

**Size guard**: identical to SeqCPlus — `values_.size() == 0` or `> 1` →
error with VarType_Unset.

**Per-pair action table** (7 rows + default error path):

| # | lhsType | rhsType | Action | Address |
|--:|---------|---------|--------|---------|
| 1 | Var(2) | Const/Cvar | merge lhs.asm; getRegisterNumber(); setValue(Var,regNum); rhsInt=-rhs.toInt(); addi(resultReg,lhsReg,Immediate(rhsInt)) | 22cecc→22ddb9 |
| 2 | Const/Cvar | Var(2) | merge rhs+lhs.asm; getRegisterNumber(); setValue(Var,regNum); addi(resultReg,R0,lhsValue); subr(resultReg,rhsReg) | 22cf78→22e394 |
| 3 | Var(2) | Var(2) | merge rhs+lhs.asm; getRegisterNumber(); setValue(Var,regNum); addi(resultReg,lhsReg,Imm(0)); subr(resultReg,rhsReg) | 22ce9e→22da31 |
| 4 | Const/Cvar | Const/Cvar | combine(VarType), combine(VarSubType); lhs.toDouble()-rhs.toDouble(); setValue(combined,combinedSub,Value(diff)) | 22d06a→22e010 |
| 5 | Wave(5) | Wave(5) | EvalResults copy of rhs; scaleWaveform(0,rhsCopy,ctx); combineWaveforms("add",lhs,scaledRhs,ctx) | 22d181→22d245 |
| 6 | Wave(5) | Const/Cvar | toString→getWaveformSampleLength; xorpd negates rhsDouble; constWaveform(len,-rhsDouble,ctx); combineWaveforms("add",lhs,constWf,ctx) | 22d334→22e4c4 |
| 7 | Const/Cvar | Wave(5) | toString→getWaveformSampleLength of rhs; constWaveform(len,lhsDouble,ctx) (NO negation); copy rhs; scaleWaveform(0,rhsCopy,ctx); combineWaveforms("add",constWf,scaledRhs,ctx) | 22d3b2→22e5e5 |
| dflt | any | any | error **0x74** via str(lhsType), str(rhsType), errorMessage(msg, -1) | 22d42f→22d4ba |

**Common tail @0x22d533**: `result->name_ = lhsResult.name_ + " - " + rhsResult.name_`
(literal 0x202d20 = " - ").

**Row 3 Var-Var detail**: binary merges BOTH assembler vectors (rhs first
at @0x22cf09, lhs second at @0x22cff8), allocates a new register, then
emits `addi(resultReg, lhsReg, Immediate(0))` as a MOV followed by
`subr(resultReg, rhsReg)` to subtract. This is the same 2-instruction
pattern as SeqCPlus Row 3, but with subr replacing addr.

**Row 1 Var-Const/Cvar detail**: binary extracts rhs value via the
per-which jump table, calls `value.toInt()` @0x22dd91, then `neg eax`
@0x22dd96 to negate the integer. Then `Immediate(negated)` @0x22dda4 and
`addi(resultReg, lhsReg, Immediate)` @0x22ddb9. This is subtraction
implemented as addition of the negated immediate.

**Row 2 Const/Cvar-Var detail**: cannot use a single addi(Value) because
subtraction is non-commutative. Instead: load the constant into a fresh
register via `addi(resultReg, AsmRegister(0), lhsValue)` @0x22e231, then
`subr(resultReg, rhsReg)` @0x22e394 to subtract the variable.

**scaleWaveform call-site detail**: at both call sites for Row 5 (22d227)
and Row 7 (22e5c4), the binary does:
1. `make_shared<EvalResults>(rhsResult)` — copy ctor into new shared_ptr
2. Call `scaleWaveform(int, shared_ptr<EvalResults>, ctx&)` — see ABI
   discrepancy note in helper catalog above.
3. The result (scaledRhs) is passed to combineWaveforms as the rhs operand.

---

### SeqCMult::evaluate — @0x22ea70 (9728 bytes)

Implements `lhs * rhs` lowering.  Dispatch is a flat cascade of paired
(lhsType, rhsType) tests.  Error code: **0x8c**.

**Key structural differences from SeqCPlus/SeqCMinus**:
- No String*String row (string multiplication unsupported).
- No Var*Var row (register multiplication not in ISA).
- Var*Const/Cvar paths delegate to `computeMult()` @0x22fdf0, a complex
  shift-and-add multiplication algorithm, instead of individual asm
  instructions.  Call sites always pass the Var operand first.
- Wave*Wave uses combineWaveforms("multiply",...) — string literal
  "multiply" confirmed from movabs 0x796c7069746c756d.
- Wave*Const/Cvar and Const/Cvar*Wave use a **second scaleWaveform
  overload** @0x2309e0 that takes `(shared_ptr<EvalResults> scalar,
  shared_ptr<EvalResults> wave, FrontendLoweringContext&)`.  Scalar
  always first, wave always second (call sites swap).
- Common tail: `" * "` separator (literal 0x202a20).

| # | lhsType | rhsType | Action | Address |
|--:|---------|---------|--------|---------|
| 1 | Var(2) | Const/Cvar | make_shared copies of both; computeMult(lhsCopy, rhsCopy, res, ctx) — Var first | 22eb0b→22f621 |
| 2 | Const/Cvar | Var(2) | make_shared copies of both; computeMult(rhsCopy, lhsCopy, res, ctx) — Var first (SWAPPED) | 22eb2d→22ebc3 |
| 3 | Const/Cvar | Const/Cvar | combine(VarType), combine(VarSubType); lhs.toDouble()*rhs.toDouble() via mulsd; setValue(combined,combinedSub,Value(product)) | 22ee95→22eff8 |
| 4 | Wave(5) | Wave(5) | combineWaveforms("multiply", lhsResult, rhsResult, ctx) | 22eff8→22f103 |
| 5 | Const/Cvar | Wave(5) | make_shared copies; scaleWaveform(lhsCopy, rhsCopy, ctx) @2309e0 — scalar first | 22f103→22f29f |
| 6 | Wave(5) | Const/Cvar | make_shared copies; scaleWaveform(rhsCopy, lhsCopy, ctx) @2309e0 — scalar first (SWAPPED) | 22f29f→22f3e2 |
| dflt | any | any | error **0x8c** via str(lhsType), str(rhsType), errorMessage(msg, -1) | 22f3e2→22f4c9 |

**Common tail @0x22f4c9**: `result->name_ = lhsResult.name_ + " * " + rhsResult.name_`
(literal 0x202a20 = " * ").

#### computeMult @0x22fdf0 — shift-and-add multiplication

Demangled: `(anonymous namespace)::computeMult(shared_ptr<EvalResults>,
shared_ptr<EvalResults>, shared_ptr<Resources>, FrontendLoweringContext&)`

First param = Var operand, second = Const/Cvar operand.

Algorithm:
1. Extract const value, call `toDouble()` then `floor()`.
2. Check integrality via `floatEqual(value, floor(value))`.
3. If non-integer or negative: error **0x8a**.
4. If FunctionArg subtype: passthrough (return Var operand).
5. Extract value as `toInt()` → multiplier (r12d).
6. Merge Var operand's assemblers into result.
7. `Resources::getRegisterNumber()` → regNum; `setValue(Var, regNum)`.
8. If multiplier == 0: emit `asmZero(resultReg)`.
9. If multiplier != 0: shift-and-add loop (up to 32 iterations):
   - Test each bit of multiplier from MSB to LSB.
   - Emit `ssl(resultReg)` for shifts.
   - Emit `addr(resultReg, sourceReg)` for adds.
   - Uses indirect vtable dispatch for some operations.

Body deferred to 21h.8.

#### scaleWaveform 2-arg overload @0x2309e0

Demangled: `(anonymous namespace)::scaleWaveform(shared_ptr<EvalResults>,
shared_ptr<EvalResults>, FrontendLoweringContext&)`

First param = scalar (Const/Cvar), second = waveform (Wave).

Behavior:
- If wave's `values_[0].subType == FunctionArg(2)`: passthrough (return wave).
- Otherwise: extract values from both params, build vector, call
  `WaveformGenerator::eval("scale", values)`.

Body deferred to 21h.8.

---

### SeqCDiv::evaluate — @0x231070 (3664 bytes)

Implements `lhs / rhs` lowering.  Structurally very different from all
other arithmetic operators.  Error code: **0x8d** (default), plus
**0xdf** (Var), **0x2a** (Const/Wave), **0x29** (divide-by-zero).

**Key structural differences from Plus/Minus/Mult**:
- NO Var paths — any Var(2) in either operand → error 0xdf (direct BST
  lookup on `ErrorMessages::messages`, NOT `ErrorMessages::format`).
- NO Wave÷Wave path (falls to default error).
- NO String paths.
- Divide-by-zero checking via `floatEqual(rhs_value, 0.0)` in both
  Const/Cvar÷Const/Cvar and Wave÷Const/Cvar paths.
- Const/Cvar÷Wave → error 0x2a (direct BST lookup) — can't divide by wave.
- Wave÷Const/Cvar computes reciprocal `1.0 / rhs`, modifies a copy via
  `EvalResults::setValue(double)`, then `scaleWaveform(reciprocal, wave,
  ctx)` using the 2-arg overload @0x2309e0.
- Three distinct error mechanisms: direct BST lookup (0xdf, 0x2a, 0x29),
  `errMsg[ErrorMessageT(0x29)]` operator[], `ErrorMessages::format(0x8d)`.
- Binary makes a stack copy of rhsResult at function entry (@0x2310f2);
  all rhs reads go through the copy.  Reconstruction avoids the early copy.
- Common tail: `" / "` separator (literal 0x202f20).

| # | lhsType | rhsType | Action | Address |
|--:|---------|---------|--------|---------|
| 1 | Var(2) | * | error 0xdf — direct BST on ErrorMessages::messages; errorMessage(string, -1) | 23112b→2312b4 |
| 2 | * | Var(2) | error 0xdf — same as row 1 (binary checks rhs after lhs fails) | 231158→2312b4 |
| 3 | Const/Cvar | Const/Cvar | extract rhs.toDouble(); floatEqual(val,0)→error 0x29 (BST); else combine(VarType), combine(VarSubType), lhs.toDouble()/rhs.toDouble() via divsd, setValue | 2311cf→231a66 |
| 4 | Const/Cvar | Wave(5) | error 0x2a — direct BST on ErrorMessages::messages; errorMessage(string, -1) | 23123b→2312b4 |
| 5 | Wave(5) | Const/Cvar | extract rhs.toDouble(); floatEqual(val,0)→error 0x29 (errMsg[]); else getValue().toDouble(), reciprocal=1.0/rhs, rhsCopy.setValue(reciprocal), scaleWaveform(reciprocal, wave, ctx) @2309e0 | 2312c7→2316f5 |
| dflt | * | * | error 0x8d — ErrorMessages::format(0x8d, str(lhsType), str(rhsType)), errorMessage(msg, -1) | 231328→23139a |

**Common tail @0x23140a**: `result->name_ = lhsResult.name_ + " / " + rhsResult.name_`
(literal 0x202f20 = " / ").

**Divide-by-zero detail (Row 3, Const÷Const)**: binary extracts rhs value
from rhsCopy.values_.back().value_ directly (NOT via getValue()), calls
toDouble(), then `floatEqual(val, 0.0)` @0x23175a.  If zero: BST search
of ErrorMessages::messages for key 0x29 @0x2317a1-2318d9, errorMessage.
If not zero: re-extracts both values via getValue().value_.toDouble() for
the actual division.

**Divide-by-zero detail (Row 5, Wave÷Const)**: same extraction pattern
from rhsCopy.values_.back().value_, then `floatEqual(val, 0.0)` @0x231631.
If zero: `errMsg[ErrorMessageT(0x29)]` @0x231673 (different access
mechanism, same error string).  If not zero: `rhsCopy.getValue().toDouble()`
@0x231694, `1.0 / val` @0x2316a1, `rhsCopy.setValue(reciprocal)` @0x2316b0
(EvalResults::setValue(double) @0x2136a0), then make_shared copies and
scaleWaveform.

**Register stash**: rbx=rdi(retptr), r14=r9(lhsResult), r15=rcx(ctx),
r12=[rbp+0x10](rhsResult → then repurposed to rhsCopy.values_.end).

---

### SeqCMod::evaluate — @0x231ec0 (1892 bytes)

Implements `lhs % rhs` lowering.  The simplest arithmetic operator — only
one compute path, one error path, and uses `fmod()` from libm.

**Key differences from SeqCDiv**:
- Only Const/Cvar % Const/Cvar is supported; all other combinations error.
- Uses `fmod()` @0xcb0c0, not integer `%`.
- No divide-by-zero check (fmod returns NaN/±inf per IEEE 754).
- No Var-specific error, no Wave paths, no BST lookups.
- Single error code: **0x8e** via `ErrorMessages::format`.
- Name separator: `" % "` (DWORD 0x202520).

| # | lhsType | rhsType | Action | Address |
|--:|---------|---------|--------|---------|
| 1 | Const/Cvar | Const/Cvar | combine(VarType), combine(VarSubType), toDouble() both, fmod(), setValue | 231fc7→23246b |
| dflt | * | * | error 0x8e — ErrorMessages::format(0x8e, str(lhsType), str(rhsType)), errorMessage(msg, -1) | 23208c→232186 |

**Common tail @0x232186**: `result->name_ = lhsResult.name_ + " % " + rhsResult.name_`

**Register stash**: r14=rdi(retptr), r13=rcx(ctx), r12→result EvalResults*,
rbx=r9(lhsResult) then repurposed, [rbp-0x60]=lhsResult ptr,
[rbp+0x10]=rhsResult, [rbp-0x64]=combinedType, r13d=combinedSub.

---

## Anonymous-namespace helper functions (Phase 21h.8)

Five file-local helper functions used by the arithmetic operator evaluate()
methods. All implemented in `seqc_ast_nodes_evaluate.cpp` between the
forward declarations and the `} // anonymous namespace` closing brace.

### scaleWaveform (1-arg) @0x228cc0 — 434 bytes

Simplest helper. The `int scaleFactor` parameter is completely unused
(register clobbered by `operator new` before any read). Creates a
`make_shared<EvalResults>()`, calls `setValue(-1.0)` (loads constant
from rodata 0x956068), then delegates to the 2-arg overload:
`return scaleWaveform(scalar, wave, ctx);`.

### constWaveform @0x22c9f0 — ~1000 bytes

Creates a constant rectangular waveform. Builds `vector<Value>` with two
elements: `Value(sampleLength)` (int32) then `Value(value)` (double).
Calls `ctx.waveformGen->eval("rect", values)`. Inline SSO string "rect"
has 0x08 header (len=4). Two catch clauses:
1. `WaveformGeneratorValueException` (filter 2, caught first)
2. `WaveformGeneratorException` (filter 1)
Both extract `e.what()` into `std::string` and call
`ctx.messages->errorMessage(str, -1)`.

### combineWaveforms @0x22c300 — ~1750 bytes

Combines two waveform-type EvalResults using the named operation. Two
FunctionArg passthrough checks (lhs first, then rhs): if
`values_.size() == 1 && values_.back().varSubType_ == VarSubType_FunctionArg`
→ return `make_shared<EvalResults>(operand)` (copy-construct). Normal
path: extract `values_.back().value_` from both (default `Value()` if
empty), push into `vector<Value>`, call `ctx.waveformGen->eval(opName, values)`.
Same two-clause exception handling pattern.

### scaleWaveform (2-arg) @0x2309e0 — ~1600 bytes

Scales a waveform by a scalar. FunctionArg passthrough on **wave only**
(not scalar): `wave->values_.size() == 1 && varSubType_ == FunctionArg`
→ return `std::move(wave)` (move, zeroing source). Normal path: extract
values **wave-first, scalar-second** into vector (order matches "scale"
function's expected parameter order), call
`ctx.waveformGen->eval("scale", values)`. Inline SSO string "scale"
has 0x0a header (len=5). Same exception handling.

### computeMult @0x22fdf0 — ~3000 bytes (most complex)

Shift-and-add integer multiplication for Var*Const paths.

**Key ABI detail**: `ctx` is passed **by value** (not reference) per the
mangled signature. The `shared_ptr<Resources> res` parameter is present
for ABI compliance but completely unused.

Algorithm:
1. Extract const operand value → `toDouble()` → `floor()` →
   `floatEqual(origValue, floor(value))`. Also check `floor(value) < 0.0`.
2. If non-integer or negative: error 0x8a (`VarMultNatural` = 138) via
   `ErrorMessages::format(VarMultNatural)` → `ctx.messages->errorMessage()`.
3. FunctionArg passthrough: check **both** var and const operand's
   `varSubType_` for `== VarSubType_FunctionArg`. If either matches,
   return `std::move(varOperand)` (always varOperand, never constOperand).
4. Extract const value → `toInt()` → `multiplier`.
5. `Resources::getRegisterNumber()` (static) → `AsmRegister(regNum)` →
   `result->setValue(VarType_Var, regNum)`.
6. Merge var operand's `assemblers_` into result via `insert()`.
7. Extract `varReg` from `varOperand->values_.back().reg_`.
8. If `multiplier == 0`: `ctx.asmCommands->asmZero(resultReg)`.
9. Else: **32-iteration MSB-first shift-and-add loop**:
   ```
   for i = 0..31:
       if firstBitSeen: emit ssl(resultReg)
       if current high bit is 1 (multiplier < 0):
           if !firstBitSeen: emit addi(resultReg, varReg, Immediate(0))  // copy
           else: emit addr(resultReg, varReg)                            // accumulate
           firstBitSeen = true
       multiplier <<= 1
   ```
   - `addi` returns `vector<AsmList::Asm>` → iterated and pushed
   - `ssl` and `addr` return single `AsmList::Asm` → pushed directly
   - The `multiplier < 0` test on signed int checks the MSB (sign bit)
     to detect "current high bit is 1"

### combine(VarSubType, VarSubType) @0x247ea0 — 60 bytes

NOT a .rodata lookup table (unlike `combine(VarType, VarType)` which uses
a 7×7 matrix). This is pure conditional logic with priority-based
semantics:

1. `Default(0)` is identity: if either operand is Default, return the other.
2. `FunctionArg(2)` dominates: if either is FunctionArg, return FunctionArg.
3. `Stub(1)` dominates Numeric/String: if either is Stub, return Stub.
4. All other combinations (Numeric+Numeric, String+String, etc.) → Default(0).

Binary implementation uses `sete`/`or`/`jne` pattern — no memory access,
no jump table. The entire function is 60 bytes of register logic.
