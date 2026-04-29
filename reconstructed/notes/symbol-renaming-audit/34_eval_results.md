# Batch 34 — eval_results

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 2 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 1;
> B3 (already resolved during Phase D/R): 1;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/eval_results.hpp`
- `reconstructed/src/eval_results.cpp`

Cross-batch context: batch 11 (`Value`) and batch 12 (`EvalResultValue`)
are immediately adjacent — `EvalResults::values_` is a
`vector<EvalResultValue>` and `getValue()` returns the embedded `Value`
of the last element. Batch 12's audit found all `EvalResultValue` field
names to be type-faithful and not misnomers; the parameter / member
naming on the `EvalResults` side is consistent with that finding (see
the `setValue(VarType, Value const&)` family below).

Binary cross-checks performed (RULES §3):

```
$ nm --demangle _seqc_compiler.so | grep "zhinst::EvalResults::"
0x176bc0  zhinst::EvalResults::EvalResults(zhinst::VarType)
0x231c60  zhinst::EvalResults::EvalResults(zhinst::EvalResults const&)
0x16f3d0  zhinst::EvalResults::~EvalResults()
0x15c1b0  zhinst::EvalResults::addAssembler(zhinst::AsmList::Asm const&)
0x211ab0  zhinst::EvalResults::getValue() const
0x15a750  zhinst::EvalResults::setValue(zhinst::Value const&)
0x20ad20  zhinst::EvalResults::setValue(zhinst::VarType)
0x15c850  zhinst::EvalResults::setValue(zhinst::VarType, int)
0x2136a0  zhinst::EvalResults::setValue(double)
0x20af20  zhinst::EvalResults::setValue(zhinst::VarType, std::string const&)
0x211b70  zhinst::EvalResults::setValue(zhinst::VarType, zhinst::Value const&)
0x2107b0  zhinst::EvalResults::setValue(zhinst::VarType, zhinst::Value const&, int)
0x16bfb0  zhinst::EvalResults::setValue(zhinst::VarType, zhinst::VarSubType, zhinst::Value const&)
0x247600  zhinst::EvalResults::setValue(zhinst::VarType, zhinst::VarSubType, zhinst::Value const&, int)
0x16f380  std::__1::__shared_ptr_emplace<zhinst::EvalResults, …>::~__shared_ptr_emplace()
```

Tier-1 (RULES §3):

- The type name `zhinst::EvalResults` is excluded — appears in mangled
  names of every method above, plus in many free functions (e.g.
  `(anonymous namespace)::evalGreater(zhinst::EvalResults const&, …)`).
- Every method and constructor name listed above is excluded — these
  are the names from the binary symbol table (qualified by the class).
- The destructor `~EvalResults` is excluded for the same reason.

Method **parameter** names (e.g. `EvalResults::addAssembler::entry`) and
**data member** names (e.g. `EvalResults::values_`, `arrayBacking_`)
are not encoded in mangled names and remain in scope.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `EvalResults` (type) | no | high | type appears in `nm` | keep current | not-misnomer |
| `EvalResults::values_` | no | high | vector<EvalResultValue> payload | keep current | not-misnomer |
| `EvalResults::assemblers_` | no | medium | vector<AsmList::Asm> push-back target | keep current | — |
| `EvalResults::hasError_` | yes | medium | flags return-stmt, not error | `returnEncountered_`, `unwindReturn_`, keep current | — |
| `EvalResults::node_` | no | low | shared_ptr<Node> set by AST eval | keep current | — |
| `EvalResults::waveformFront_` | no | high | holds shared_ptr<WaveformFront> | keep current | — |
| `EvalResults::name_` | unsure | low | dual-use label/expression-text | `label_`, keep current | — |
| `EvalResults::arrayBacking_` | no | medium | array-index backing pointer | keep current | — |
| `EvalResults(VarType)::type` | no | high | stored as `varType_` | keep current | — |
| `EvalResults(EvalResults const&)::other` | no | high | std. copy-ctor source name | keep current | — |
| `addAssembler::entry` | no | medium | pushed onto assemblers_ | keep current | — |
| `setValue(Value const&)::val` | no | high | copied into ev.value_ | keep current | — |
| `setValue(VarType)::type` | no | high | stored as varType_ | keep current | — |
| `setValue(VarType,int)::type` | no | high | stored as varType_ | keep current | — |
| `setValue(VarType,int)::reg` | no | high | passed to AsmRegister(int) | keep current | — |
| `setValue(double)::val` | no | high | copied into Value(double) | keep current | — |
| `setValue(VarType,string)::type` | no | high | stored as varType_ | keep current | — |
| `setValue(VarType,string)::s` | unsure | low | terse, but local idiom | `text`, keep current | — |
| `setValue(VarType,Value)::type` | no | high | stored as varType_ | keep current | — |
| `setValue(VarType,Value)::val` | no | high | copied into ev.value_ | keep current | — |
| `setValue(VarType,Value,int)::reg` | no | high | wrapped in AsmRegister | keep current | — |
| `setValue(VarType,VarSubType,Value)::type/sub/val` | no | high | direct field stores | keep current | — |
| `setValue(VarType,VarSubType,Value,int)::reg` | no | high | wrapped in AsmRegister | keep current | — |
| `getValue()` (no parameters) | — | — | n/a | n/a | — |
| `makeERV` (anonymous-ns helper) | no | low | local helper, post-recon | keep current | — |
| `makeERV::type` / `::sub` / `::val` | no | high | direct field stores | keep current | — |

Group ordering inside the table follows §8a (fields → ctors → methods,
each method's params after the method).

## 3. Detailed findings

### EvalResults (type)  [no / high / not-misnomer]

Evidence:
- `nm --demangle _seqc_compiler.so` lists `zhinst::EvalResults::~EvalResults()`
  at 0x16f3d0 and the name `zhinst::EvalResults` appears in the mangled
  parameter types of dozens of free functions and methods, e.g.
  `(anonymous namespace)::evalGreater(zhinst::EvalResults const&, …)`,
  `(anonymous namespace)::scaleWaveform(std::shared_ptr<zhinst::EvalResults>, …)`,
  `zhinst::SeqCAssign::evaluate(…, zhinst::EvalResults const&, zhinst::EvalResults const&) const`.

Interpretation:
- The fully-qualified type name `zhinst::EvalResults` is recorded in
  the original binary's symbol table (RULES §3 tier 1, "Class/struct/
  enum type names — authoritative").

Judgement:
- Not a misnomer; type name excluded from rename.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/eval_results.hpp:70
- nm output above

---

### EvalResults::values_  [no / high / not-misnomer]

Evidence:
- include/zhinst/eval_results.hpp:74 — `std::vector<EvalResultValue> values_;`
- The vector type appears verbatim in many mangled symbols, e.g.
  `zhinst::CustomFunctions::setUserReg(std::vector<zhinst::EvalResultValue, …> const&, …)`,
  i.e. the public dispatch signature for built-ins is
  `(vector<EvalResultValue> const&, shared_ptr<Resources>) -> shared_ptr<EvalResults>`
  — the same vector type backs the field here.
- Read site: src/eval_results.cpp:182-183 — `getValue()` returns
  `values_.back().value_`.
- Write sites: every `setValue(...)` overload assigns a
  `std::vector<EvalResultValue>{...}` into `values_`
  (src/eval_results.cpp:117, 128, 150, 195-197, 209, 222-225,
  236-238, 254, 270).
- Cross-batch consistency: batch 12 (`EvalResultValue`) found the
  members `varType_`, `varSubType_`, `value_`, `reg_` to be
  type-faithful; the plural `values_` is the one-element-per-`set*`
  vector that holds them.

Interpretation:
- The field stores the result-value payload of an `EvalResults`
  instance; its type and contents are exactly what the name implies,
  and the name aligns with the cross-batch type name `EvalResultValue`.

Judgement:
- Not a misnomer.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/eval_results.hpp:74
- read:    src/eval_results.cpp:182,183
- written: src/eval_results.cpp:67,117,128,150,195,209,222,236,254,270
- nm output above

---

### EvalResults::assemblers_  [no / medium]

Evidence:
- include/zhinst/eval_results.hpp:75 — `std::vector<AsmList::Asm> assemblers_;`
- src/eval_results.cpp:101-104 — `addAssembler(entry)` calls
  `assemblers_.push_back(entry)`.
- Header note: "addAssembler @0x15c1b0 pushes to vector at +0x18 with
  element stride 0xa8" (matches `sizeof(AsmList::Asm)`).
- Other recon code accumulates instructions through this field
  indirectly via `addAssembler` (e.g. src/custom_functions_io.cpp:327
  `results->addAssembler(asmEntry);`) and via copy-ctor and dtor walks
  of the vector (header ll. 33-35).

Interpretation:
- The field contains a list of `AsmList::Asm` entries — each `Asm`
  itself wraps a single sequencer instruction plus context. The
  English plural `assemblers_` matches the element type's namespace
  (`AsmList::Asm`), even if "assembler instructions" or
  "asmInstructions_" would be a more conventional plural for
  individual instructions. No use site contradicts the current name.

Judgement:
- Not a misnomer; the name is faithful to the element type's namespace
  although a more conventional alternative exists.

Proposals:
- keep current (medium)
- `asmInstructions_` (low) — slightly more idiomatic plural; not
  recommended without a stronger signal.

Locations consulted:
- declared: include/zhinst/eval_results.hpp:75
- written via push_back: src/eval_results.cpp:103
- written elsewhere: src/custom_functions_io.cpp:327; many other
  `results->addAssembler(...)` call sites (45+ across recon).

---

### EvalResults::hasError_  [yes / medium]

Evidence:
- include/zhinst/eval_results.hpp:76 — `bool hasError_ = false;`
- `reconstructed/notes/incidental_findings.md` IF-30 (status: fixed):
  > The reconstruction had `hasError_ = true` inside the node-chaining
  > block (whenever `childResult->node_` was non-null). The binary
  > (0x212800) has two completely separate paths: childHadError==0
  > does name-building + node-chaining only; childHadError==1 does
  > return-value extraction + hasError_=true + break.
- src/seqc_ast_nodes_evaluate.cpp:5793-5831 (SeqCReturnStatement::
  evaluate): "sets `result->hasError_ = true` to signal 'return was
  encountered' to the enclosing statement-list".
- src/seqc_ast_nodes_evaluate.cpp:6306 — `result->hasError_ = true;`
  is set when a child evaluated to a `SeqCReturnStatement`.
- src/seqc_ast_nodes_evaluate.cpp:6125, 6188, 6267 — propagation:
  `result->hasError_ = result->hasError_ || childResult->hasError_;`.
  The local that drives the break is named `childHadError`.
- The flag is consumed solely as a control-flow signal: after a child
  with `hasError_==true` is seen, the parent breaks out of its loop
  (the "return seen, stop executing further siblings" semantics).
- No call site in `reconstructed/` raises a real diagnostic on the
  basis of `hasError_`; error messages are emitted through
  `Resources::compilerMessage`, not via this flag.

Interpretation:
- The field is set true to signal "a `return` statement was reached",
  and is propagated upward by statement-list evaluators to terminate
  the surrounding block. It is not set on any genuine evaluation
  error; the binary uses a separate diagnostic channel
  (`compilerMessage`) for that. The current name conflates "early
  unwind on return" with "compilation error".

Judgement:
- Yes, the name misleadingly suggests an error condition when the
  field actually flags a normal `return`-statement unwind.

Proposals:
- `returnEncountered_` (medium) — describes the trigger condition
  exactly as IF-30 phrased it.
- `unwindReturn_` (medium) — captures the propagation semantics.
- keep current (low) — only if synthesis judges the surrounding
  comments adequate compensation for the misleading name.

Locations consulted:
- declared: include/zhinst/eval_results.hpp:76
- written: src/seqc_ast_nodes_evaluate.cpp:6072, 6125, 6188, 6267,
  6306; src/eval_results.cpp:69 (default-init false)
- read:    src/seqc_ast_nodes_evaluate.cpp:6125, 6188, 6267
- discussed: notes/incidental_findings.md IF-23, IF-30

---

### EvalResults::node_  [no / low]

Evidence:
- include/zhinst/eval_results.hpp:77 — `std::shared_ptr<Node> node_;`
- The header note (ll. 33-34) says destruction order destroys
  "shared_ptr +0x38" alongside the other shared_ptrs, confirming the
  type and offset.
- Notes refer to "childResult->node_" as a non-null shared_ptr<Node>
  used by the statement-list evaluator's name-building path
  (incidental_findings IF-23, IF-30; lines 6125, 6267 in
  seqc_ast_nodes_evaluate.cpp).
- `Node` is the AST-node base type (RULES §3 tier 1 — appears in many
  mangled symbols; type itself is excluded but irrelevant here as we
  are auditing the field name).

Interpretation:
- The field carries a `shared_ptr<Node>`, used by parents to walk
  back through the AST during name-building / statement chaining. The
  name is a literal, type-matching one-word abbreviation. Confidence
  is low rather than high because it is a generic word and there is
  no binary-string evidence of the original member name.

Judgement:
- Not a misnomer; matches the field's static type and observed use.

Proposals:
- keep current (low)

Locations consulted:
- declared: include/zhinst/eval_results.hpp:77
- header note: include/zhinst/eval_results.hpp:33-35
- referenced: notes/incidental_findings.md IF-23, IF-30

---

### EvalResults::waveformFront_  [no / high]

Evidence:
- include/zhinst/eval_results.hpp:78 — `std::shared_ptr<WaveformFront> waveformFront_;`
- src/waveform_generator.cpp:409-410 — comment
  `// 0x25c63c-0x25c640: store wf into results->waveformFront_ (+0x48)`
  and the code `results->waveformFront_ = std::move(wf);`.
- src/seqc_ast_nodes_evaluate.cpp:3261, 3288, 3291 — propagation in
  SeqCAssign: `aux->waveformFront_` and `rhsResult.waveformFront_`
  feed `result->waveformFront_`.
- src/seqc_ast_nodes_evaluate.cpp:6791 —
  `result->waveformFront_->secondaryName = result->name_;` (e.g.
  "zeros(64)") — clearly accesses a `WaveformFront`.
- The type name `WaveformFront` is in the binary symbol table (e.g.
  `zhinst::WaveformFront::WaveformFront(std::shared_ptr<zhinst::WaveformFront>, std::string const&)`),
  so the field's static type is fixed; the member name itself merely
  echoes the type.

Interpretation:
- The field's static type is `shared_ptr<WaveformFront>`; the member
  name is a direct lower-camel transliteration of the type name. Use
  sites are uniformly `waveform`-related (assignments from waveform
  generators, propagation through waveform-arithmetic operators).

Judgement:
- Not a misnomer.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/eval_results.hpp:78
- written: src/waveform_generator.cpp:410; src/seqc_ast_nodes_evaluate.cpp:3261,3288
- read:    src/seqc_ast_nodes_evaluate.cpp:3291,6791

---

### EvalResults::name_  [unsure / low]

Evidence:
- include/zhinst/eval_results.hpp:79 — `std::string name_;`
- Used in three semantically different ways at write sites:
  1. **Identifier name** — set to the SeqC variable identifier when
     evaluating a variable declaration:
     - src/seqc_ast_nodes_evaluate.cpp:3052 `result->name_ = name;`
       (the declaration's identifier, fed in as the local `name`).
     - read at 5155, 5183 as `lhsResult.name_` / `rhsResult.name_`
       passed to `res.get()->updateCvar(<name>, …)` — this is a real
       variable-table lookup key.
  2. **Reconstructed source-text** — set to the textual form of the
     evaluated expression:
     - src/seqc_ast_nodes_evaluate.cpp:3735 `lhsResult.name_ + " + " + rhsResult.name_`
     - 4090 `… + " - " + …`, 4295 `… + " * " + …`, 4525 `… + " / " + …`,
       4707, 4722, 4737, 4752, 4767, 4784, 4802, 4820, 4841, 4859,
       4902, 4945, 5038, 5224, 5388 (all comparison/logical/shift/
       bitwise/inc/dec operators).
     - Control-flow nodes: 7020 `"if (" + condResult->name_ + ")"`,
       7572 `"switch (…)"`, 8127, 8633 `"while (…)"`, 8706 `"repeat(…)"`,
       7250 `"case " + caseVal.toString()`.
     - Function calls: 6427, 6661, 6663, 6764, 6766
       `funName + "(" + argResults->name_ + ")"`.
     - `"return"` / `"return " + childResult->name_` at 5831, 5857.
  3. **Waveform secondary-name source** — at line 6791
     `result->waveformFront_->secondaryName = result->name_;` — the
     accumulated text (e.g. `"zeros(64)"`) is copied into the
     waveform's diagnostic name.
- Read for length/truncation: lines 6671-6681, 6773-6783 walk
  `result->name_` byte-by-byte to truncate long argument lists.
- Cross-batch reference: notes/symbol-renaming-audit/14_waveform.md:207
  refers to the same field as "the textual form" of an expression.

Interpretation:
- The field is a polysemous textual label: sometimes a true
  identifier (variable name), sometimes a source-text reconstruction
  of the expression that produced this `EvalResults`, sometimes the
  string later copied into `WaveformFront::secondaryName`. "Name" is
  defensible as an umbrella for all three uses, but it is also
  generic enough that nothing in the name signals the
  source-text-reconstruction role that dominates the operator-evaluator
  call sites. No naming evidence from the binary is available to
  decide either way (member names are not encoded; RULES §3).

Judgement:
- Unsure: the name fits roles (1) and (3) cleanly but is a weak
  match for role (2), which is the majority of write sites.

Proposals:
- keep current (medium) — defensible as a generic label spanning all
  three uses; minimal-change option.
- `label_` (low) — slightly more neutral umbrella; loses the
  identifier connotation that role (1) genuinely needs.
- `displayText_` (low) — matches the dominant source-text role but
  fits role (1) less well (variable names are not just for display).

Locations consulted:
- declared: include/zhinst/eval_results.hpp:79
- written: src/seqc_ast_nodes_evaluate.cpp:2813, 2843, 3052, 3375,
  3735, 4090, 4295, 4525, 4707-4859, 4902, 4945, 5038, 5224, 5388,
  5831, 5857, 6129, 6131, 6192, 6194, 6273, 6275, 6427, 6661, 6663,
  6681, 6764, 6766, 6783, 7020, 7250, 7572, 8127, 8633, 8706 (124+
  call sites total)
- read:    same lines, plus 5155, 5183, 6671, 6773, 6791
- cross-ref: notes/symbol-renaming-audit/14_waveform.md:207

---

### EvalResults::arrayBacking_  [no / medium]

Evidence:
- include/zhinst/eval_results.hpp:80 — `std::shared_ptr<EvalResults> arrayBacking_;`
- Header comment ll. 36-39:
  > SeqCArray::evaluate @0x211140 writes shared_ptr<EvalResults> into
  > ER+0x70 at 0x211798/0x2117a0 — identifies the arrayBacking_ field.
  > SeqCAssign::evaluate @0x243e60 propagates +0x70 from rhs if
  > non-null (check at 0x243f13).
- src/seqc_ast_nodes_evaluate.cpp:3094-3101:
  > 3. If `lhsResult.arrayBacking_` (field at +0x70) is non-null,
  >    REPLACE aux with lhsResult.arrayBacking_.   // @0x243f13-243f5c
  > 4. SeqCArray::evaluate (the rhs of `arr[i] = expr;`) … stores the
  >    indexed result (VarType_Wave) in arrayBacking_ of …
  > `if (lhsResult.arrayBacking_) { aux = lhsResult.arrayBacking_; }`
- notes/frontend_lowering.md:43-45: "The `arrayBacking_` field at +0x70
  is `shared_ptr<EvalResults>` (self-referential)."
- The type is `shared_ptr<EvalResults>` (i.e. self-referential), and
  it is only ever populated for VarType_Wave results that came from an
  array-index expression, then consumed by `SeqCAssign` to track the
  underlying array element.

Interpretation:
- The field carries the array element's "backing" `EvalResults` so
  that an outer assignment can locate the actual storage being
  written. The name accurately captures both the array-context
  (set by `SeqCArray::evaluate`) and the backing-pointer role
  (used by `SeqCAssign::evaluate` to redirect to the underlying
  array slot). No alternative noun fits the observed usage better.

Judgement:
- Not a misnomer.

Proposals:
- keep current (medium)

Locations consulted:
- declared: include/zhinst/eval_results.hpp:80
- written: header note at line 36 (binary @0x211798/0x2117a0)
- read:    src/seqc_ast_nodes_evaluate.cpp:3100, 3101 (and binary
  @0x243f13)
- cross-ref: notes/frontend_lowering.md:43-45

---

### EvalResults::EvalResults(VarType type)  [param `type`: no / high]

Evidence:
- src/eval_results.cpp:66-67 — the parameter is forwarded directly to
  `makeERV(type)`, which sets `ev.varType_ = type;` (line 36).

Interpretation:
- `type` is stored verbatim into a field whose own name (`varType_`)
  is reviewed in batch 12 and judged faithful. Naming the constructor
  parameter `type` is the obvious local form.

Judgement:
- Not a misnomer.

Proposals:
- keep current (high)

Locations consulted:
- defined: src/eval_results.cpp:66
- declared: include/zhinst/eval_results.hpp:85

---

### EvalResults::EvalResults(EvalResults const& other)  [param `other`: no / high]

Evidence:
- src/eval_results.cpp:161-169 — every member is initialized from
  `other.<member>` in declaration order; this is a textbook copy ctor.

Interpretation:
- `other` is the conventional name for the source object in a copy
  constructor (used throughout the C++ standard library and idiomatic
  C++ codebases).

Judgement:
- Not a misnomer.

Proposals:
- keep current (high)

Locations consulted:
- defined: src/eval_results.cpp:161

---

### EvalResults::addAssembler::entry  [no / medium]

Evidence:
- src/eval_results.cpp:101-104 — the parameter is pushed into the
  `assemblers_` vector via `assemblers_.push_back(entry);`.
- Type is `AsmList::Asm const&` (one assembler-list entry).

Interpretation:
- The parameter is one element of `assemblers_`; "entry" is a generic
  but accurate label for "single element being added to a collection".
  Confidence is medium (not high) because no naming evidence for the
  original parameter exists; alternatives like `asm` (reserved word!),
  `instruction`, or `item` would also fit, but none clearly improves
  on `entry`.

Judgement:
- Not a misnomer.

Proposals:
- keep current (medium)

Locations consulted:
- defined: src/eval_results.cpp:101
- declared: include/zhinst/eval_results.hpp:118

---

### EvalResults::setValue family — parameter names

The eight non-trivial `setValue` overloads share a common pattern: the
parameters are stored directly into the corresponding `EvalResultValue`
fields whose names are already audited in batch 12 and judged faithful
(`varType_`, `varSubType_`, `value_`, `reg_`). The parameter names in
this class echo those field names:

| Overload | Param | Stored as | Match? |
|---|---|---|---|
| `setValue(Value const& val)` | `val` | `ev.value_ = val` (via Value-ctor of ERV) | yes |
| `setValue(VarType type)` | `type` | `ev.varType_ = type` | yes |
| `setValue(VarType type, int reg)` | `type` | `ev.varType_ = type` | yes |
|  | `reg` | `ev.reg_ = AsmRegister(reg)` | yes |
| `setValue(double val)` | `val` | wrapped into `Value(val)` then `ev.value_ = …` | yes |
| `setValue(VarType type, std::string const& s)` | `type` | `ev.varType_ = type` | yes |
|  | `s` | wrapped into `Value(s)` then `ev.value_ = …` | terse but ok |
| `setValue(VarType, Value const& val)` | `type/val` | direct field stores | yes |
| `setValue(VarType, Value const& val, int reg)` | `type/val/reg` | direct field stores | yes |
| `setValue(VarType, VarSubType sub, Value const&)` | `type/sub/val` | direct field stores | yes |
| `setValue(VarType, VarSubType sub, Value const&, int reg)` | `type/sub/val/reg` | direct field stores | yes |

Cross-batch confirmation: at every `setValue(VarType_Var, …)` call
site found in `reconstructed/src/`, the `int` argument is named
`regNum`, `reg`, `newRegNum` or similar (44 hits across
custom_functions_io.cpp and seqc_ast_nodes_evaluate.cpp), all
consistent with the parameter name `reg`. This corroborates batch 12's
finding for `EvalResultValue::reg_`.

Detailed blocks are omitted for these parameters (RULES §8b: routine
"name fits usage" entries belong in §4 / table). The single parameter
on which there is anything to say is the string parameter:

#### EvalResults::setValue(VarType, std::string const&)::s  [unsure / low]

Evidence:
- src/eval_results.cpp:234-238 — `s` is wrapped in `Value(s)` and
  stored in `ev.value_`.
- Type is `std::string const&`.
- Other overloads use `val` for the embedded-Value source; this one
  uses `s` for what is logically the same role (the value being
  stored).

Interpretation:
- `s` is the C-tradition single-letter name for a string parameter.
  In the surrounding code that mostly uses `val` for Value-source
  arguments, the choice is locally inconsistent but not actively
  misleading.

Judgement:
- Unsure: not wrong, but mildly inconsistent with the sister
  overloads' `val` naming.

Proposals:
- keep current (medium)
- `text` (low) — clearer but adds no semantic information beyond what
  the type already conveys.
- `val` (low) — would harmonize with the other overloads.

Locations consulted:
- declared: include/zhinst/eval_results.hpp:106
- defined:  src/eval_results.cpp:234

---

### Anonymous-namespace helper `makeERV`  [no / low]

Evidence:
- src/eval_results.cpp:33-50 — defined in an anonymous namespace as a
  factory for default-initialized `EvalResultValue` instances; used
  by several `setValue` overloads (lines 67, 117, 128, 196, 209, 224,
  237).
- The helper does not exist in the binary (`nm` shows no
  corresponding symbol; it is an inline reconstruction convenience).

Interpretation:
- This is a recon-only helper; its name is a private, mechanical
  abbreviation ("make EvalResultValue"). Audit scope (RULES §2)
  covers free-function names, but the post-recon-only nature and the
  contained anonymous namespace make this a low-risk symbol.

Judgement:
- Not a misnomer; abbreviation is accurate to its construction role.

Proposals:
- keep current (low)
- `makeEvalResultValue` (low) — fully spelled-out variant; readability
  trade-off only.

Locations consulted:
- defined: src/eval_results.cpp:33, 43

## 4. Symbols inspected and judged routinely fine

- `EvalResults::EvalResults()` — defaulted; no parameters.
- `EvalResults::~EvalResults()` — defaulted in source; tier-1 excluded
  via `nm`.
- `EvalResults::operator=(EvalResults const&)` — `= delete`; no
  parameter to name.
- `EvalResults::getValue() const` — no parameters; return type and
  semantics match name.
- `setValue(...)` parameters `type`, `sub`, `val`, `reg` across the
  eight non-string overloads (covered as a group above; routine
  type-faithful naming).
- `makeERV` parameters `type`, `sub`, `val` — direct stores into
  identically-named ERV fields.

## 5. Coverage

- **Fully covered:** every named symbol declared in
  `include/zhinst/eval_results.hpp` (1 type, 7 fields, 14 method
  signatures with all of their named parameters) and every named
  symbol defined in `src/eval_results.cpp` (the same plus the
  anonymous-namespace `makeERV` helper).
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - The type name `EvalResults` and the destructor `~EvalResults` —
    appear in the binary symbol table; tier-1 excluded. Listed above
    with `not-misnomer` block.
  - Every method name (`addAssembler`, `getValue`, all nine `setValue`
    overloads, both constructors) — same; tier-1 excluded.
  - Forward-declared types `Node`, `WaveformFront`, `EvalResultValue`,
    `VarType`, `VarSubType` — defined in their own headers and
    audited in their respective batches (batch 12 for `EvalResultValue`,
    batch 20 for `Node`, batch 17 for `WaveformFront`).
  - The header note text ("Layout (Phase 15a-i, 2026-04-23) …") and
    the file-comment-block addresses are documentation, not symbols.

No `cross-batch-arbitration` was triggered: the parameter names in
this batch agree with the field names audited in batch 12, and there
is no he-said/she-said inconsistency between the `EvalResults` side
and the `EvalResultValue` side. The single `yes` finding
(`hasError_`) and the single `unsure` finding (`name_`) are both
self-contained within this batch — synthesis can decide them on the
evidence here without cross-batch coordination.
