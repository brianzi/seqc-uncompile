# Frontend Lowering Data Model

Data types flowing through `FrontEndLoweringFacade::lower()` and the
`SeqCAstNode::evaluate()` virtual: **EvalResults**, **EvalResultValue**,
**Value**, **LowerResult**, **FrontendLoweringState**.

Reconstructed in Phase 15a-i (2026-04-23).

## Types reconstructed

### EvalResults (0x80 bytes)

New file: `include/zhinst/eval_results.hpp`

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
- getValue(): @0x211ab0 — returns last EvalResultValue from values_
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
- `reconstructed/include/zhinst/eval_results.hpp` — full EvalResults layout + 14 method decls
- `reconstructed/notes/frontend_lowering.md` — this file

### Modified
- `include/zhinst/value.hpp` — layout corrected (0x20→0x28), static_assert updated
- `include/zhinst/custom_functions.hpp` — EvalResultValue fields renamed; added includes for value.hpp and asm_register.hpp
- `include/zhinst/frontend_lowering.hpp` — FrontendLoweringState.result typed as shared_ptr<Node>; added Node forward decl
- `include/zhinst/compiler.hpp` — lower() return type changed to LowerResult; LowerResult struct added; EvalResults forward decl added
- `src/compiler.cpp` — lower() body partially implemented (Context+State setup, return); included frontend_lowering.hpp
- `src/custom_functions.cpp` — EvalResultValue dtor delegates to Value::~Value()

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
- SeqCAstNode vtable[0] (the 3-arg evaluate virtual) is not yet declared. Needs Phase 15a #1 completion (lower() return type wiring).
- The method bodies for EvalResults (ctor, copy ctor, dtor, getValue, 9 setValues, addAssembler) are declared but not implemented. Low priority — the declarations are sufficient for type-checking.
