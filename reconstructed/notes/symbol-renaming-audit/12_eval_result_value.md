# Batch 12 — eval_result_value

## 1. Files considered

- `reconstructed/include/zhinst/ast/eval_result_value.hpp`
- `reconstructed/src/ast/eval_result_value.cpp`

Cross-batch context: this batch sits adjacent to batch 11 (`Value`) and
batch 34 (`EvalResults`). The struct embeds a `Value` at +0x08 and is
itself stored in `std::vector<EvalResultValue>` inside
`EvalResults::values_`. No he-said/she-said arbitration was triggered:
the field naming in this batch is internally consistent and matches how
call sites refer to it (`erv.varType_`, `erv.value_`, `erv.reg_`, …).

Binary cross-checks performed:

```
$ nm --demangle _seqc_compiler.so | grep EvalResultValue
... t zhinst::EvalResultValue::~EvalResultValue()       # 0x15c820
... lots of free fns / methods taking EvalResultValue ...
```

So the type name `EvalResultValue` and the destructor are tier-1
excluded (RULES §3). `VarType` and `VarSubType` likewise appear in the
mangled symbol table (e.g. `EvalResults::setValue(VarType, …)`,
`Resources::addVar(…, VarSubType)`) — the embedded enum *types* are
excluded from rename, though the `varType_`/`varSubType_` *member
names* are not.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `EvalResultValue` (type) | no | high | type appears in `nm` | keep current | not-misnomer |
| `EvalResultValue::varType_` | no | high | matches `VarType` enum role | keep current | not-misnomer |
| `EvalResultValue::varSubType_` | no | high | matches `VarSubType` enum role | keep current | not-misnomer |
| `EvalResultValue::value_` | no | medium | embedded `Value` payload | keep current | — |
| `EvalResultValue::reg_` | no | medium | holds `AsmRegister` binding | keep current | — |
| `EvalResultValue::~EvalResultValue` | no | high | dtor in `nm` table | keep current | not-misnomer |

## 3. Detailed findings

### EvalResultValue (type)  [no / high / not-misnomer]

Evidence:
- `nm --demangle _seqc_compiler.so` lists, among many others:
  - `0x15c820 t zhinst::EvalResultValue::~EvalResultValue()`
  - `t zhinst::CustomFunctions::writeToNode(zhinst::EvalResultValue, zhinst::EvalResultValue, zhinst::EvalResultValue, std::shared_ptr<zhinst::Resources>)`
  - many `…(std::vector<zhinst::EvalResultValue> const&, …)` overloads.

Interpretation:
- The Itanium-mangled symbols encode the fully qualified type name
  `zhinst::EvalResultValue` verbatim, both as a value parameter type and
  inside `std::vector<…>` template args.

Judgement:
- Not a misnomer; the type name is part of the original binary symbol
  table and is excluded from rename per RULES §3 tier 1.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/ast/eval_result_value.hpp:42
- nm output checked above

### EvalResultValue::varType_  [no / high / not-misnomer]

Evidence:
- include/zhinst/ast/eval_result_value.hpp:43 — `VarType varType_; // +0x00 — outer type tag`.
- include/zhinst/runtime/resources.hpp defines `enum VarType` with members
  `VarType_Var=2`, `VarType_Const`, `VarType_String=4`, … and the type
  appears in mangled binary symbols (e.g.
  `EvalResults::setValue(VarType, …)`).
- Use sites read the field as the discriminator that decides which
  `Value` accessor to call:
  - `src/runtime/custom_functions_play.cpp:100` `if (arg1.varType_ == VarType_Var) …`
  - `src/runtime/custom_functions_play.cpp:491` `if (firstVal.varType_ != VarType_Const)`
  - `src/runtime/custom_functions_play.cpp:794` `VarType rateType = parseEnd[1].varType_;`
  - `src/runtime/custom_functions_play.cpp:1607,1635,1682,1705,1728,1751,1933,2012,2095,2121,2152` repeatedly cast `valRef.varType_` to `int` to switch on `Var`/`Const` etc.
  - `src/runtime/custom_functions_io.cpp:63` `if (static_cast<int>(arg.varType_) == 2)`.
- Write site: `src/runtime/custom_functions_play.cpp:2429`
  `nameVal.varType_ = VarType_String;` (with the comment "first arg
  must be string type for generate()").

Interpretation:
- The field stores a value of enum type `VarType` and is consistently
  used as the outer type discriminator that selects the right branch in
  the `Value` payload.

Judgement:
- Not a misnomer; the name precisely names the field's type and role.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/ast/eval_result_value.hpp:43
- used: src/runtime/custom_functions_play.cpp:97,100,491,536,761,769,794,909,1075,1289,1607,1635,1682,1705,1728,1751,1825,1838,1933,2012,2095,2121,2152,2267,2429; src/runtime/custom_functions_io.cpp:63,68; many more (842 grep hits across `reconstructed/`).

### EvalResultValue::varSubType_  [no / high / not-misnomer]

Evidence:
- include/zhinst/ast/eval_result_value.hpp:44 — `VarSubType varSubType_; // +0x04 — sub-type qualifier`.
- include/zhinst/runtime/resources.hpp:102-109 defines `enum VarSubType` with
  members `VarSubType_Default=0`, `VarSubType_Stub=1`,
  `VarSubType_FunctionArg=2`, `VarSubType_Numeric=3`,
  `VarSubType_String=4`. The enum type appears as an argument in
  multiple binary symbols (e.g. `Resources::addVar(…, VarSubType)`).
- Read site: `src/runtime/custom_functions_play.cpp:1289`
  `if (static_cast<int>(path.varSubType_) == 2) …` — reads it as the
  enum discriminator that determines whether the path argument needs
  special handling.
- Write site: `src/runtime/custom_functions_play.cpp:2430`
  `nameVal.varSubType_ = VarSubType_Numeric;`.

Interpretation:
- The field's static type is `VarSubType`, every read either compares
  it to a `VarSubType_*` member or casts to `int` for a discriminator
  switch, and the only writes assign `VarSubType_*` enumerators.

Judgement:
- Not a misnomer; name correctly describes the field's enum type and
  role as a sub-type qualifier.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/ast/eval_result_value.hpp:44
- used: src/runtime/custom_functions_play.cpp:1280,1289,2430

### EvalResultValue::value_  [no / medium]

Evidence:
- include/zhinst/ast/eval_result_value.hpp:45 — `Value value_; // +0x08 — embedded Value`.
- The field holds an embedded `zhinst::Value` (variant) payload. Use
  sites consistently call `Value` accessors on it:
  - `src/runtime/custom_functions_play.cpp:200` `std::string wfName = erv.value_.toString();`
  - `:218` `values.push_back(erv.value_);`
  - `:495` `playLength = firstVal.value_.toInt();`
  - `:812` `int waveIndex = parseEnd[1].value_.toInt();`
  - many more (`.value_.toInt()`, `.value_.toDouble()`, `.value_.toString()`).
- Write sites: `:2431` `nameVal.value_ = Value(name);`.
- The header comment dated 2026-04-23 (Phase 15a-i) explicitly notes
  that the field was renamed from `data_` to `value_` based on
  disassembly evidence (the embedded `Value`'s discriminator at
  `[this+0x10] = ERV+0x08+0x08`).

Interpretation:
- Field is of type `Value`, accessed exclusively via `Value`'s public
  interface; the name `value_` is a faithful, type-matching label.
  Confidence is medium rather than high because there is no
  binary-symbol-table evidence that this exact member name was used in
  the original sources (member names are not encoded; RULES §3); and
  `value_` is generic English, so other names ("payload_",
  "variantValue_") would also be defensible. However nothing in the
  use pattern argues against the current name.

Judgement:
- Not a misnomer; the name correctly describes the embedded `Value`
  payload.

Proposals:
- keep current (medium)

Locations consulted:
- declared: include/zhinst/ast/eval_result_value.hpp:45
- used: src/runtime/custom_functions_play.cpp:200,218,495,541,812,878,914,944,949,1296,1610,1611,1614,1638,1639,1643,1648,1684,1689,1707,1712,1730,1735,1753,1758,1825,1838,1857,1874,1953,2031,2054,2055,2057,2062,2160,2258,2272,2277,2431; src/runtime/custom_functions_io.cpp:73,428; many more.

### EvalResultValue::reg_  [no / medium]

Evidence:
- include/zhinst/ast/eval_result_value.hpp:46 — `AsmRegister reg_; // +0x30 — register binding`.
- All read sites assign to a local `AsmRegister` or pass to
  `appendSuser(...)` which expects an `AsmRegister`:
  - `src/runtime/custom_functions_play.cpp:102` `waitReg = arg1.reg_;`
  - `:1081` `indexReg = parseEnd[0].reg_;`
  - `:1629` `appendSuser(localList, asmCommands_, valRef.reg_, …);`
  - `:1674,1990,2006,2087,2116,2154` similar `appendSuser(..., valRef.reg_, …)` calls.
  - `src/runtime/custom_functions_io.cpp:65` `AsmRegister reg = arg.reg_;`.
- Write site: `:2432` `nameVal.reg_ = AsmRegister(-1);` (default
  unbound register sentinel).
- The header comment ties the field to disassembly:
  `setWaitCyclesReg @0x15ca90 reads AsmRegister at [base+0x68] = ERV[1]+0x30`.

Interpretation:
- The field is statically typed `AsmRegister`, always assigned an
  `AsmRegister` value, and consumed by APIs that require an
  `AsmRegister`. The name `reg_` is a short-form alias for "register
  binding".

Judgement:
- Not a misnomer; abbreviation matches the field's type and role.
  Medium (not high) because `reg_` is a generic three-letter
  abbreviation chosen during reconstruction (header comment "Fields
  renamed from opaque field_00/…/field_30") rather than recovered from
  the binary; any of `register_`, `regBinding_`, `asmReg_` would also
  fit. No evidence contradicts the current name.

Proposals:
- keep current (medium)
- `asmReg_` (low) — slight readability gain; not recommended

Locations consulted:
- declared: include/zhinst/ast/eval_result_value.hpp:46
- used: src/runtime/custom_functions_play.cpp:97,102,1067,1081,1522,1601,1629,1673-1674,1990,2006,2087,2094,2116,2154,2432; src/runtime/custom_functions_io.cpp:65

### EvalResultValue::~EvalResultValue  [no / high / not-misnomer]

Evidence:
- `nm --demangle _seqc_compiler.so` →
  `0x15c820 t zhinst::EvalResultValue::~EvalResultValue()`.

Interpretation:
- Method name is fixed by the C++ destructor naming rule and the type
  name (which is itself in the symbol table).

Judgement:
- Not a misnomer.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/ast/eval_result_value.hpp:48
- defined: src/ast/eval_result_value.cpp:13
- nm output above

## 4. Symbols inspected and judged routinely fine

(Nothing else in scope; the two files in this batch declare only the
type itself, four data members, and the destructor, all addressed
above.)

## 5. Coverage

- **Fully covered:** every named symbol in
  `include/zhinst/ast/eval_result_value.hpp` and
  `src/ast/eval_result_value.cpp` (the type, four members, the dtor).
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - The type name `EvalResultValue`, the embedded enum types
    `VarType` and `VarSubType`, the embedded `Value` and `AsmRegister`
    type names, and `EvalResultValue::~EvalResultValue` — all appear
    (directly or via mangled parameter types) in the binary symbol
    table; tier-1 excluded by RULES §3. They are listed above with
    `not-misnomer` blocks for record-keeping rather than as rename
    candidates.
  - Forward declarations `VarType` / `VarSubType` in the header are
    redeclarations of types defined in `zhinst/types.hpp` /
    `zhinst/resources.hpp` — out of scope here; will be covered by the
    relevant types batch.
  - The field comments referencing `field_00/field_08/which_/data_/field_30`
    document a *prior* (already-completed) Phase 15a-i rename and are
    not current symbol names.

No cross-batch arbitration was raised: there is no
field/parameter naming inconsistency between this struct and the
counterparts in batch 11 (`Value`, embedded as the `value_` field) or
batch 34 (`EvalResults`, holds a `vector<EvalResultValue>`); use sites
agree across batches.
