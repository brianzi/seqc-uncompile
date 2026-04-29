# Batch 04a — seqc_ast_node base (header + ast_node.cpp + ast.cpp)

## 1. Files considered

- `reconstructed/include/zhinst/seqc_ast_node.hpp`
- `reconstructed/src/seqc_ast_node.cpp`
- `reconstructed/src/seqc_ast.cpp`

Out of scope (handled by 04b–04e):
- `reconstructed/src/seqc_ast_nodes_evaluate.cpp`. Its symbols are not flagged
  here, but it is consulted as evidence for callers of the in-scope members
  and methods.

Cross-references consulted (read-only):
- `reconstructed/include/zhinst/expression.hpp` (Expression layout that feeds
  the SeqCAstNode ctor via `toSeqCAstRecursor`)
- `reconstructed/include/zhinst/types.hpp` (`enum class EDirection`, `VarType`)
- `reconstructed/notes/symbol-renaming-audit/42_expression.md` (already
  established that `Expression::valueType` is actually `EDirection`; this batch
  inherits the dataflow into `SeqCAstNode::direction_`)

### `nm --demangle _seqc_compiler.so` results relevant to §3 exclusion

Authoritative type names (excluded from rename — appear inside mangled
symbols of methods/ctors):
- `SeqCAstNode`, `SeqCOperation`, `SeqCCommand`, `SeqCVariableType`,
  `SeqCLabel`, `SeqCContinueStatement`, `SeqCBreakStatement`, `SeqCNoCmd`,
  `SeqCNeg`, `SeqCPos`, `SeqCInv`, `SeqCNotExpr`, `SeqCReturnStatement`,
  `SeqCOperator`, `SeqCPlus`, `SeqCMinus`, `SeqCMult`, `SeqCDiv`, `SeqCMod`,
  `SeqCShiftR`, `SeqCShiftL`, `SeqCGreater`, `SeqCLower`, `SeqCLEqual`,
  `SeqCGEqual`, `SeqCEqual`, `SeqCNEqual`, `SeqCInc`, `SeqCDec`,
  `SeqCAndExpr`, `SeqCOrExpr`, `SeqCXorExpr`, `SeqCLogAnd`, `SeqCLogOr`,
  `SeqCAssign`, `SeqCNoOp`, `SeqCFunctionCall`, `SeqCArray`,
  `SeqCIfCondition`, `SeqCCaseEntry`, `SeqCSwitchCase`, `SeqCWhileLoop`,
  `SeqCDoWhile`, `SeqCRepeat`, `SeqCIfElse`, `SeqCCondExpr`, `SeqCFunction`,
  `SeqCForLoop`, `SeqCArgList`, `SeqCDeclList`, `SeqCStmtList`,
  `SeqCParamList`, `SeqCVariable`, `SeqCValue`, `EValueCategory`,
  `EDirection`, `VarType`.

Authoritative free-function / method names (excluded — every line below is
present verbatim in `nm --demangle`):

```
0x1c1730 zhinst::str(zhinst::EDirection)
0x1c16c0 zhinst::str(zhinst::EValueCategory)
0x1fa3c0 zhinst::printSeqCAst(zhinst::SeqCAstNode const&)
0x1fda40 zhinst::swap(zhinst::SeqCAstNode&, zhinst::SeqCAstNode&)
0x1f6240 zhinst::toSeqCAst(std::shared_ptr<zhinst::Expression>)
0x1f62c0 (anonymous namespace)::toSeqCAstRecursor(std::shared_ptr<zhinst::Expression>)
0x1fa430 (anonymous namespace)::printSeqCAst(zhinst::SeqCAstNode const&, std::string const&)

0x1fda00 SeqCAstNode::SeqCAstNode(EValueCategory, int, EDirection)
0x209dc0 SeqCAstNode::evaluate(...)
0x209dd0 SeqCAstNode::getListElements()
0x1fda20 SeqCAstNode::children()
~SeqCAstNode (D0/D2)
```

All ctors of all 53 derived classes are present in `nm`, all with
parameter-type signature `(EValueCategory, int, EDirection, VarType, …)` —
type names authoritative; **parameter names not encoded → in scope**.

Accessors confirmed in `nm` (excluded as method names):
- `cond()`, `body()`, `ifBody()`, `elseBody()`, `init()`, `incr()`, `expr()`,
  `lhs()`, `rhs()`, `funName()`, `arguments()`, `array()`, `index()`,
  `label()`, `cases()`, `singleCase()`, `isSingleCase()`, `hasCases()`,
  `validLabel()`, `hasLabel()`, `params()` (on SeqCArgList & SeqCParamList),
  `decls()`, `stmts()`, `retType()`, `call()`, `evalCases()`.

Accessors NOT in `nm` (in scope — inline/header only):
- `SeqCAstNode::valueCategory()`, `::type()`, `::lineNr()`, `::direction()`,
  `::varType()`, `::setVarType()`, `SeqCValue::tag()`, `SeqCValue::asDouble()`,
  `SeqCValue::asString()`, `SeqCVariable::name()`,
  `SeqCArgList/DeclList/ParamList/StmtList::elements()` (and `::append()`).

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `SeqCAstNode::valueCategory_` | no | high | matches enum & `EValueCategory` | keep current | not-misnomer |
| `SeqCAstNode::lineNr_` | no | high | source line; `(line: …)` printer | keep current | not-misnomer |
| `SeqCAstNode::direction_` | no | high | holds `EDirection` enum | keep current | not-misnomer |
| `SeqCAstNode::varType_` | no | high | holds `VarType`; matches name | keep current | not-misnomer |
| `SeqCAstNode::SeqCAstNode::vc` | no | high | passed to `valueCategory_` | keep current | — |
| `SeqCAstNode::SeqCAstNode::type` | yes | high | actually source line; stored in `lineNr_` | `lineNr` | coordinated-rename |
| `SeqCAstNode::SeqCAstNode::dir` | no | high | passed to `direction_` | keep current | — |
| `SeqCAstNode::type()` (accessor) | yes | high | "legacy" alias for `lineNr_` | remove; use `lineNr()` | coordinated-rename |
| `SeqCAstNode::lineNr()` | no | high | accurate accessor for `lineNr_` | keep current | not-misnomer |
| `SeqCAstNode::valueCategory()` | no | high | obvious | keep current | — |
| `SeqCAstNode::direction()` | no | high | obvious | keep current | — |
| `SeqCAstNode::varType()` | no | high | obvious | keep current | — |
| `SeqCAstNode::setVarType::vt` | no | high | typical setter param | keep current | — |
| `swap(SeqCAstNode&,&)::a / ::b` | no | high | conventional swap params | keep current | — |
| Every derived ctor `int type` param | yes | high | inherits §`SeqCAstNode::type` misnomer | `lineNr` | coordinated-rename |
| Every derived ctor `EValueCategory vc` | no | high | passed to base | keep current | — |
| Every derived ctor `EDirection dir` | no | high | passed to base | keep current | — |
| Every derived ctor `VarType vt` | no | high | written to `varType_` | keep current | — |
| `SeqC*::child_` (5 unary) | no | medium | sole child of unary node | keep current | — |
| `SeqCOperator::lhs_` / `rhs_` | no | high | binary-op operands | keep current | not-misnomer |
| `SeqC*::first_` / `second_` (2-child binaries: SeqCFunctionCall, SeqCArray, SeqCIfCondition, SeqCCaseEntry, SeqCSwitchCase, SeqCWhileLoop, SeqCDoWhile, SeqCRepeat) | yes | medium | generic; named accessor exists | rename per node, see §3 | coordinated-rename |
| `SeqCIfElse::cond_` / `ifBody_` / `elseBody_` | no | high | matches accessors in nm | keep current | not-misnomer |
| `SeqCCondExpr::cond_` / `ifBody_` / `elseBody_` | unsure | medium | accessors in nm; but these are ternary branches | keep current; `then_`/`else_` | — |
| `SeqCFunction::call_` / `params_` / `body_` / `retType_` | no | high | match nm accessors | keep current | not-misnomer |
| `SeqCForLoop::init_` / `cond_` / `incr_` / `body_` | no | high | match nm accessors | keep current | not-misnomer |
| `SeqC*List::elements_` | no | high | obvious | keep current | — |
| `SeqCVariable::name_` | no | high | identifier text | keep current | not-misnomer |
| `SeqCValue::payload_` | no | medium | tagged-variant storage | keep current | — |
| `SeqCValue::tag_` | no | high | dispatches Tag enum | keep current | not-misnomer |
| `SeqCValue::pad34_` | unsure | low | structural pad slot | keep current; `reserved34_` | — |
| `SeqCValue::Payload::d` / `::str` | no | high | union members | keep current | — |
| `SeqCValue::Tag::eString` / `::eDouble` | no | high | matches dtor jump table | keep current | not-misnomer |
| `EValueCategory` enumerators | no | high | `str()` returns same names | keep current | not-misnomer |
| Macros `SEQC_TRIVIAL_LEAF`, `SEQC_UNARY`, `SEQC_OPERATOR`, `SEQC_BINARY`, `SEQC_LIST` (+ `_IMPL` siblings) | no | medium | descriptive of code emitted | keep current | — |
| Macro param `Name`, `VtableAddr`, `Label`, `LabelLen`, `CloneSize`, `FirstAccessor`, `SecondAccessor`, `NamedAccessor` | no | high | match macro body | keep current | — |
| `ExprArgs` (anon helper struct) | no | high | local helper bundling 4 args | keep current | — |
| `ExprArgs::vc` / `lineNr` / `dir` / `vt` | no | high | wrap base ctor args | keep current | — |
| `recurseChild::e` / `i` | no | high | obvious | keep current | — |
| `recurseAllChildren::e` | no | high | obvious | keep current | — |
| `toSeqCAstRecursor::expr` | no | high | obvious | keep current | — |
| `toSeqCAst::expr` | no | high | obvious | keep current | — |
| `printSeqCAst::node` | no | high | obvious | keep current | — |
| `printSeqCAstImpl::node` / `prefix` | no | high | recursive helper | keep current | — |
| `printSeqCAstImpl::kids` / `i` / `isLast` / `childPrefix` | no | high | obvious local vars | keep current | — |
| `printSeqCAstImpl::emptyPrefix` | no | medium | descriptive | keep current | — |
| `str(EValueCategory)::vc` | no | high | obvious | keep current | — |
| `str(EDirection)::dir` | no | high | obvious | keep current | — |
| `swap(SeqCValue&,&)::a` / `::b` / `atag` / `btag` / `tmp` | no | high | local swap mechanics | keep current | — |
| Per-derived `swap(Name&, Name&)::a` / `::b` | no | high | conventional | keep current | — |
| Copy-ctor parameters `o` (across 53 classes) | no | high | conventional `other` short form | keep current | — |
| List ctor `elements` | no | high | written to `elements_` | keep current | — |
| Unary ctor `child` | no | high | moved into `child_` | keep current | — |
| `SeqCOperator` ctor `lhs` / `rhs` | no | high | match `lhs_` / `rhs_` | keep current | — |
| Two-child ctor `first` / `second` | yes | medium | generic; named accessors give role | per-node rename, see §3 | coordinated-rename |
| `SeqCIfElse` ctor `cond` / `ifBody` / `elseBody` | no | high | match field names | keep current | — |
| `SeqCCondExpr` ctor `cond` / `ifBody` / `elseBody` | unsure | medium | parallels SeqCIfElse | keep current; `then_`/`else_` | — |
| `SeqCFunction` ctor `call` / `params` / `body` / `retType` | no | high | match field names | keep current | — |
| `SeqCForLoop` ctor `init` / `cond` / `incr` / `body` | no | high | match field names | keep current | — |
| `SeqCFunctionCall` ctor `first` / `second` | yes | high | accessors `funName()` / `arguments()` exist | `funName` / `args` | coordinated-rename |
| `SeqCArray` ctor `first` / `second` | yes | high | accessors `array()` / `index()` exist | `array` / `index` | coordinated-rename |
| `SeqCCaseEntry` ctor `first` / `second` | yes | high | accessors `label()` / `body()` exist | `label` / `body` | coordinated-rename |
| `SeqCSwitchCase` ctor `first` / `second` | yes | high | accessors `cond()` / `body()` exist | `cond` / `body` | coordinated-rename |
| `SeqCIfCondition` ctor `first` / `second` (via `SEQC_BINARY_IMPL`) | yes | high | accessors `cond()` / `ifBody()` exist | `cond` / `ifBody` | coordinated-rename |
| `SeqCWhileLoop` ctor `first` / `second` | yes | high | accessors `cond()` / `body()` exist | `cond` / `body` | coordinated-rename |
| `SeqCDoWhile` ctor `first` / `second` | yes | high | accessors `body()` / `cond()` exist; first IS body | `body` / `cond` | coordinated-rename |
| `SeqCRepeat` ctor `first` / `second` | yes | high | accessors `cond()` / `body()` exist (count, body) | `count` / `body` | coordinated-rename |
| `SeqCValue` ctor `s` / `d` | no | high | one-letter but obvious by overload | keep current | — |
| `SeqCVariable` ctor `name` | no | high | written to `name_` | keep current | — |

## 3. Detailed findings

### SeqCAstNode::SeqCAstNode::type and SeqCAstNode::type() (accessor)  [yes / high / coordinated-rename]

Evidence:
- `seqc_ast_node.hpp:106` —
  `SeqCAstNode(EValueCategory vc, int type, EDirection dir);   // 0x1fda00`
- `seqc_ast_node.cpp:47-51` — base ctor body:
  ```
  SeqCAstNode::SeqCAstNode(EValueCategory vc, int type, EDirection dir)
      : valueCategory_(vc)
      , lineNr_(type)
      , direction_(dir)
  {}
  ```
- `seqc_ast_node.hpp:140` — accessor: `int type() const { return lineNr_; }
  // legacy accessor name`
- `seqc_ast_node.hpp:141` — accessor: `int lineNr() const { return lineNr_; }`
- `seqc_ast_node.hpp:149` — field declaration: `int lineNr_; // +0x0C  Source
  line number.`
- `seqc_ast_node.cpp:94` — printer: `std::cout << " (line: " << node.type()
  << ")"` (wraps the `type()` value as the source line).
- `seqc_ast_nodes_evaluate.cpp:6097,6160,6236,6293,7032,7145,8197,9062,9142,9284`
  — every one of the 10 call sites that uses `type()` immediately treats the
  value as a source-line number, e.g.
  ```
  ctx.messages->errorMessage(msg, cond()->type());   // @0x213d19
  int lineNr = this->type();
  int nextLineNr = elems[i + 1]->type();             // @0x212b7e
  ```
- `seqc_ast_nodes_evaluate.cpp:7358,6816` — parallel call sites using the
  newer accessor: `caseEntry.lineNr()`, `funNameNode->lineNr()`.
- `seqc_ast.cpp:31-46` — `ExprArgs` helper documents the dataflow:
  `+0x08 → int (lineNumber → SeqCAstNode::lineNr_)`. The ctor's second arg
  is fed from `Expression::lineNumber`, which batch 42 confirmed is the
  source line number sourced from `ctx->currentLineNumber()`.
- `nm --demangle`:
  `0x1fda00 zhinst::SeqCAstNode::SeqCAstNode(EValueCategory, int, EDirection)`
  — the second positional parameter type is `int`; its **name** is not
  encoded (RULES §3) so it is in scope.

Interpretation:
- The base ctor's second parameter is declared `int type`, but the body
  unconditionally stores it into the field named `lineNr_`, which is
  documented and used everywhere as the source line number. The accessor
  `type()` is explicitly annotated "legacy accessor name" and is the older
  of the two redundant getters; usage at every call site shows the value
  is a line number, never a "type" of any kind. The dataflow chain is
  `Expression::lineNumber → SeqCAstNode ctor "type" arg → lineNr_ →
  type()/lineNr() → error-message source line`.

Judgement:
- Yes. Both the parameter name `type` (in the ctor signature) and the
  accessor `type()` are misnomers. The field already has a correct name
  (`lineNr_`). The redundant accessor `type()` exists only as a transitional
  artifact and should be removed; all 10 call sites of `type()` should be
  switched to `lineNr()`.

Proposals (`SeqCAstNode::SeqCAstNode::type`):
- `lineNr`             (high)
- `lineNumber`         (medium) — matches Expression-side name
- keep current         (low)

Proposals (`SeqCAstNode::type()` accessor):
- remove accessor; use `lineNr()` only (high)
- keep both as transitional alias (low)

Status: `coordinated-rename` — the parameter `type` appears on **53 derived
ctors** as well (all of which forward `type` to `SeqCAstNode(vc, type, dir)`)
plus on the inline `setVarType`-adjacent code paths. A consistent rename
would touch every ctor signature and the wrapper code in `seqc_ast.cpp`
(`ExprArgs::lineNr`, `recurseChild`, etc., already use the correct
terminology — only the derived-class ctor signatures need updating).

Cross-reference:
- Already raised in batch 42 (`expression.md`, §5 Coverage / Deferred):
  "The `int type` parameter of `SeqCAstNode::SeqCAstNode` is clearly
  misnamed (the body stores it into `lineNr_`) but lives in another batch
  (seqc_ast_node) — flagged here only for cross-batch context".

Locations consulted:
- declared: include/zhinst/seqc_ast_node.hpp:106, 140-141, 149
  (and the same `int type` in 51 SEQC_*-macro-instantiated ctors plus the
  6 hand-written ctors at lines 197, 269, 387, 414, 443, 473, 521, 552, 587,
  621, 663, 706, 749, 785-789)
- defined: src/seqc_ast_node.cpp:47-51 (and forwarded from every derived
  ctor at lines 142, 180, 219, 314, 347, 386, 427, 479, 527, 579, 636, 692,
  696, 798, 856, 864, 874)
- consumed: src/seqc_ast_node.cpp:94 (printSeqCAstImpl);
  src/seqc_ast_nodes_evaluate.cpp:6097, 6160, 6236, 6293, 7032, 7145, 8197,
  9025 (comment), 9062, 9142, 9284

### SeqCAstNode::valueCategory_, ::lineNr_, ::direction_, ::varType_  [no / high / not-misnomer]

Evidence:
- `seqc_ast_node.hpp:148-153` — declarations with byte offsets and types:
  `EValueCategory valueCategory_; // +0x08`, `int lineNr_; // +0x0C`,
  `EDirection direction_; // +0x10`, `VarType varType_; // +0x14`.
- `nm --demangle`: `zhinst::str(zhinst::EDirection)` and
  `zhinst::str(zhinst::EValueCategory)` are both authoritative free-function
  names whose enum-typed parameters appear in mangling — confirms the
  two enum types and matches the field naming verbatim.
- `seqc_ast_node.cpp:33-40` — `str(EDirection)` returns "in"/"out"/"inout".
  `str(EValueCategory)` returns "eNOVALUECATEGORY"/"eLVALUE"/"eRVALUE".
  These strings live in the binary's `.rodata` (RULES §4d tier-2 evidence).
- `seqc_ast.cpp:34-46` — `ExprArgs` documents the field roles in source
  (`vc → valueCategory_`, `lineNr → lineNr_`, `dir → direction_`,
  `vt → varType_`).
- `seqc_ast_nodes_evaluate.cpp:9744` — `returnVarType = retType()->varType();`
  — `varType_` is read out via `varType()` and used as a `VarType`.
- `seqc_ast_nodes_evaluate.cpp:2687` — `…->setVarType(varType_);` — written
  back as a `VarType`.
- `seqc_ast_node.cpp:81-86` — `swap(a,b)` swaps `direction_`,
  `valueCategory_`, `lineNr_` by name, matching the binary's swap.
- Batch 42 (`expression.md`, blocks `Expression::valueCategory`,
  `Expression::lineNumber`, `Expression::valueType`) traces every write of
  these four upstream fields and confirms 1:1 dataflow into the
  identically-named `SeqCAstNode` fields.

Interpretation:
- All four field names match (a) the type of value they store (per the
  `EValueCategory`/`EDirection`/`VarType` mangling and the `str()` helpers),
  (b) every read/write site, and (c) the upstream `Expression` field names
  established in batch 42.

Judgement:
- No. None of the four base-class fields is a misnomer. Cross-class
  consistency with named enums and faithful `str()` strings is strong
  positive evidence (RULES §4d tier-2).

Proposals:
- keep current (high) for all four.

Locations consulted:
- declared: include/zhinst/seqc_ast_node.hpp:148-153
- written: src/seqc_ast_node.cpp:47-51 (ctor), 81-86 (swap),
  126, 134, 150, 155, 162, 184, 196, 227, 244, 279, 320, 333, 369, 408,
  448, 489, 507, 537, 555, 591, 612, 648, 667, 695, 699, 703, 719, 803,
  811, 838, 860, 868, 878, 887 (every derived ctor / clone / copy-ctor)
- consumed: src/seqc_ast_node.cpp:94 (printer);
  src/seqc_ast_nodes_evaluate.cpp:2687, 9744 etc.

### SeqC*-binary `first_` / `second_` field family + matching ctor params  [yes / medium / coordinated-rename]

Evidence:
- `seqc_ast_node.hpp:354-378` — `SEQC_BINARY` macro defines fields as
  `first_` / `second_` and introduces a `FirstAccessor` / `SecondAccessor`
  parameter that gives them roles per node:
  `SEQC_BINARY(SeqCIfCondition,  cond,     ifBody,  …);`
  `SEQC_BINARY(SeqCWhileLoop,    cond,     body,    …);`
  `SEQC_BINARY(SeqCDoWhile,      body,     cond,    …);`
  `SEQC_BINARY(SeqCRepeat,       cond,     body,    …);`
- `seqc_ast_node.hpp:385-466` — hand-written near-duplicates that *also*
  use `first_`/`second_` privately while exposing role-named accessors:
  - `SeqCFunctionCall`: `funName()` → `first_`, `arguments()` → `second_`.
  - `SeqCArray`: `array()` → `first_`, `index()` → `second_`.
  - `SeqCCaseEntry`: `label()` → `first_`, `body()` → `second_`.
  - `SeqCSwitchCase`: `cond()` → `first_`, `body()` → `second_`.
- `nm --demangle` confirms every accessor in the list above as an
  authoritative method name (RULES §3 tier-1) — so the accessors define
  the canonical role of each slot.
- `seqc_ast.cpp:81-159` — call sites in `toSeqCAstRecursor` consistently
  build the nodes with `recurseChild(e, 0)` / `recurseChild(e, 1)` whose
  meaning is dictated by the accessor names above (e.g. eIF: cond, body;
  eDOWHILE: body, cond — note the swap, in line with `SeqCDoWhile`'s
  `body()=first_`).
- `seqc_ast_nodes_evaluate.cpp` uses the accessors (`cond()`, `body()`,
  `funName()`, `arguments()`, `array()`, `index()`, `label()`,
  `ifBody()`, `cases()` …) — never `first_`/`second_` directly — confirming
  the accessor names are the canonical role labels.

Interpretation:
- The pair `first_`/`second_` is a deliberate, generic placeholder name
  generated by a layout-sharing macro. The slot's *role* is faithfully
  encoded only in each class's accessor names, which the binary symbol
  table elevates to authoritative tier-1 evidence.
- For the hand-written near-duplicates that don't go through the macro
  (`SeqCFunctionCall`, `SeqCArray`, `SeqCCaseEntry`, `SeqCSwitchCase`),
  the same `first_`/`second_` appears in private fields and in ctor
  parameter lists — a missed opportunity to use the role names directly.
- Same for the matching ctor parameters `first` / `second`: the bison-side
  callers in `seqc_ast.cpp` always know the role; the parameter names
  obscure it.

Judgement:
- Yes. The `first_`/`second_` field name family and the corresponding ctor
  parameters `first`/`second` are misnomers in every node class where the
  accessor pair already supplies a clear role-based name. Confidence is
  medium for the field name (because keeping a uniform private storage
  name has a defensible argument) and high for the ctor parameter rename.
  Practical proposal: rename only the **ctor parameters** to the role
  names (low risk, mechanical) and leave the macro-generated `first_`/
  `second_` storage if a single-name layout is wanted across nodes.

Proposals:
- ctor params: rename to role-name pair per class (high)
  - `SeqCFunctionCall(... funName, args)`
  - `SeqCArray(... array, index)`
  - `SeqCCaseEntry(... label, body)`
  - `SeqCSwitchCase(... cond, body)`
  - `SeqCIfCondition(... cond, ifBody)`
  - `SeqCWhileLoop(... cond, body)`
  - `SeqCDoWhile(... body, cond)`
  - `SeqCRepeat(... cond, body)`  [or `count`, `body` — see below]
- private fields: also rename to role-name pair per class (medium)
- keep current (low)

Sub-finding — `SeqCRepeat` accessor name `cond()` is itself questionable:
the SeqC `repeat (N) { body }` construct has no condition — the first
slot is the iteration **count**. Batch 42 records `createRepeat::count` as
correct on the Expression side. Marked here as low-confidence because
the accessor `cond()` is in `nm` (excluded from rename) — but the *ctor
parameter* and *private field* are not, and should be `count` to match
the upstream and downstream usage.

Status: `coordinated-rename` — touches 8 classes. Done together with the
parameter rename (since each class's ctor body must move from
`first_(std::move(first))` to `<role>_(std::move(<role>))`).

Locations consulted:
- declared: include/zhinst/seqc_ast_node.hpp:354-378 (macro), 383-512
  (hand-written near-duplicates).
- defined: src/seqc_ast_node.cpp:313-471 (macro impl + hand-written impls
  for SeqCFunctionCall, SeqCArray, SeqCCaseEntry, SeqCSwitchCase).
- callers: src/seqc_ast.cpp (constructs every binary node);
  src/seqc_ast_nodes_evaluate.cpp (consumes via accessors).

### SeqCAstNode::SeqCAstNode::vc, ::dir (and derived ctor counterparts)  [no / high / —]

Evidence:
- `seqc_ast_node.hpp:106` — base ctor `(EValueCategory vc, int type,
  EDirection dir)`.
- `seqc_ast_node.cpp:47-51` — body assigns `valueCategory_(vc)` and
  `direction_(dir)`.
- All 53 derived ctors forward `vc` and `dir` unchanged
  (e.g. `SeqCOperator::SeqCOperator(... vc, type, dir, vt, lhs, rhs)`).
- `seqc_ast.cpp:34-46` — `ExprArgs::vc`, `ExprArgs::dir` mirror the same
  short names from the call site.

Interpretation:
- Two-letter abbreviations of the underlying enum types, used uniformly
  across the entire node hierarchy. No risk of confusion in context.

Judgement:
- No. Routine.

Proposals:
- keep current (high). (Also acceptable: spell out as `valueCategory`,
  `direction` for header readability — low improvement, high churn.)

### SeqCValue::pad34_  [unsure / low / —]

Evidence:
- `seqc_ast_node.hpp:828` — `int32_t pad34_{}; // +0x34 / +0x3C`.
- `seqc_ast_node.cpp:889, 956` — copied across in copy-ctor and swap; never
  read or written via a meaningful path.
- Mirrors `Expression::pad0C_` flagged in batch 42 (same low-confidence
  status).

Interpretation:
- Acts as 4 bytes of structural alignment between `tag_` (+0x30) and the
  end of the object. The `pad`-prefix tells readers it is not semantic;
  the suffix `34_` records the byte offset.

Judgement:
- Unsure. Not actively misleading. Could be either compiler-inserted
  padding (no slot) or a deliberately reserved field; no evidence either
  way is in scope of this batch.

Proposals:
- keep current (high)
- `reserved34_` (low) — neutral about whether semantic
- `unused34_`  (low)

### SeqCCondExpr::cond_ / ifBody_ / elseBody_  [unsure / medium / —]

Evidence:
- `seqc_ast_node.hpp:550-578` — three children named `cond_`, `ifBody_`,
  `elseBody_`, with public accessors of the same name.
- `nm --demangle`: `SeqCCondExpr::cond()`, `::ifBody()`, `::elseBody()`
  are all authoritative method names (excluded from rename).
- `seqc_ast.cpp:122-125` — call site for `eCONDEXP`:
  `SeqCCondExpr(... recurseChild(e, 0), recurseChild(e, 1),
  recurseChild(e, 2))` for the C ternary `?:`.

Interpretation:
- `SeqCCondExpr` represents the C ternary `cond ? trueBranch : falseBranch`,
  not an `if/else` statement. Re-using `ifBody_` / `elseBody_` from
  `SeqCIfElse` is harmless — same children-of-a-conditional pattern — but
  obscures the ternary semantics slightly.
- The accessor names `ifBody`/`elseBody` are in `nm` (authoritative). The
  fields can be renamed independently of the accessors but the convention
  in this file is field name = accessor name.

Judgement:
- Unsure. Neither name is wrong; the alternative `then_` / `else_` (or
  `trueBranch_` / `falseBranch_`) would document the ternary nature
  better, but the binary already chose the `ifBody/elseBody` accessor
  names for both classes.

Proposals:
- keep current (high) — matches binary accessor names
- `then_` / `else_` (low)
- `trueBranch_` / `falseBranch_` (low)

### Macro families SEQC_TRIVIAL_LEAF / SEQC_UNARY / SEQC_OPERATOR / SEQC_BINARY / SEQC_LIST  [no / medium / —]

Evidence:
- `seqc_ast_node.hpp:174-191` — `SEQC_TRIVIAL_LEAF(Name, VtableAddr)`
  defines a 24-byte direct-AstNode subclass with no extra fields.
- Lines 228-249, 305-321, 354-378, 660-688 — analogous pattern, each
  with macro-name self-describing the layout family.
- `seqc_ast_node.cpp:125-139, 179-205, 267-282, 313-344, 692-743` —
  matching `_IMPL` macros generate ctor / dtor / clone / print / swap.

Interpretation:
- Macro names accurately describe the layout family they emit. The macro
  parameters (`Name`, `VtableAddr`, `Label`, `LabelLen`, `CloneSize`,
  `FirstAccessor`, `SecondAccessor`, `NamedAccessor`) all match their
  use inside the macro body verbatim.

Judgement:
- No. Routine.

Proposals:
- keep current (high).

(`CloneSize` is unused — passed to `SEQC_TRIVIAL_LEAF_IMPL` but never
referenced inside the macro body. Dead parameter; not a naming issue.)

## 4. Symbols inspected and judged routinely fine

- `EValueCategory` enum + members (`eNOVALUECATEGORY`, `eLVALUE`,
  `eRVALUE`) — `str(EValueCategory)` returns identical names verbatim
  (RULES §4d tier-2 — `.rodata` strings).
- `SeqCValue::Tag` + members (`eString`, `eDouble`) — comments document
  binary dtor jump table @0xb065a0; tag values 0/1 match.
- All accessor methods present in `nm` (excluded from rename per RULES §3):
  `cond`, `body`, `ifBody`, `elseBody`, `init`, `incr`, `expr`, `lhs`,
  `rhs`, `funName`, `arguments`, `array`, `index`, `label`, `cases`,
  `singleCase`, `isSingleCase`, `hasCases`, `validLabel`, `hasLabel`,
  `params` (×2), `decls`, `stmts`, `retType`, `call`, `evalCases`.
  `validLabel`/`hasLabel` are duplicate methods both present in the
  binary at distinct addresses (0x202b90, 0x202b80) — both authoritative.
- `printSeqCAst` and its anon-ns helper `printSeqCAstImpl::prefix`,
  `kids`, `i`, `isLast`, `childPrefix`, `emptyPrefix` — recursive AST
  printer; all locals descriptive.
- `SeqCAstNode::children()`, `::getListElements()`, `::getVarTypes()`,
  `::evaluate()` — virtual methods; method names in `nm` so excluded.
- `SeqCAstNode::valueCategory()`, `::lineNr()`, `::direction()`,
  `::varType()`, `::setVarType()` — accessors not in `nm` but match field
  names directly; routine.
- `SeqCValue::tag()`, `::asDouble()`, `::asString()` — match payload
  semantics.
- `SeqCVariable::name()` — returns `name_`.
- `SeqC*List::elements()`, `::append()` — match `elements_` storage and
  conventional list operation names.
- `SeqCOperator::lhs_` / `rhs_` and matching ctor params `lhs` / `rhs` —
  textbook binary operator naming; match `lhs()` / `rhs()` accessors.
- `SeqCIfElse::cond_` / `ifBody_` / `elseBody_`, `SeqCFunction::call_` /
  `params_` / `body_` / `retType_`, `SeqCForLoop::init_` / `cond_` /
  `incr_` / `body_` — fields whose names match the corresponding `nm`
  accessor methods verbatim (positive evidence — keep).
- `SeqCArgList`, `SeqCDeclList`, `SeqCParamList`, `SeqCStmtList` —
  `elements_`-only fields; ctor params `elements` correct.
- `SeqCVariable::name_` and ctor param `name` — written / read identically.
- `SeqCValue::Payload::d`, `::str` — union members; one-letter `d` for
  `double` is conventional in this codebase.
- `SeqCValue` ctor params `s` / `d` — overload-disambiguated
  (`std::string` ctor vs `double` ctor); short single-letter is
  acceptable in the overload context.
- `SeqCValue::tag_`, `payload_` — tag-discriminated variant fields, both
  used consistently with the names.
- `swap(SeqCAstNode&,&)::a` / `::b`, plus per-derived `swap(...)` —
  conventional swap parameter names.
- Copy-ctor parameter `o` (53 classes) — conventional short for "other".
- All `_IMPL` macro parameters (`Name`, `Label`, `LabelLen`) — descriptive
  inside macro body.
- `ExprArgs` helper struct + members `vc` / `lineNr` / `dir` / `vt` —
  bundles the four common base-ctor args; names obvious.
- `recurseChild(e, i)`, `recurseAllChildren(e)`, `toSeqCAstRecursor(expr)`,
  `toSeqCAst(expr)`, `printSeqCAst(node)` — helper free functions; all
  parameter names obvious.
- `str(EValueCategory)::vc`, `str(EDirection)::dir` — short
  enum-conventional names.
- `swap(SeqCValue&,&)::atag`, `::btag`, `::tmp` — local mechanics for
  tagged-variant swap; obvious.

## 5. Coverage

- **Fully covered:**
  - `SeqCAstNode` base class: every field, every method's parameters, every
    accessor.
  - All 53 derived classes: ctor parameters, copy-ctor parameter, private
    field names, accessor return names (with the §3 exclusion noted).
  - Two enum types (`EValueCategory`, `SeqCValue::Tag`) and every member.
  - All free functions in `seqc_ast_node.cpp` (`str(EValueCategory)`,
    `str(EDirection)`, `printSeqCAst`, anon `printSeqCAstImpl`,
    `swap(SeqCAstNode&,&)`).
  - All free functions and the anon-ns helpers in `seqc_ast.cpp`
    (`toSeqCAst`, `toSeqCAstRecursor`, `recurseChild`, `recurseAllChildren`,
    `ExprArgs`).
  - All 5 layout-family macros and their `_IMPL` siblings (parameter
    names).

- **Deferred:**
  - The entire `seqc_ast_nodes_evaluate.cpp` translation unit — owned by
    sub-batches 04b/04c/04d/04e. Its symbols (e.g. `evalCaseBody` locals,
    `SeqCSwitchCase::evaluate` locals, the many private virtual evaluate
    overrides) were consulted as evidence here but not flagged.
  - Whether `SeqCAstNode::type()` should be **removed** (vs. just deprecated)
    is implicit in the proposal but is a downstream code change, not a
    naming change per se. Synthesis can decide.
  - Whether `SeqCValue::pad34_`'s storage type or presence is correct —
    out of scope per RULES §2a (type-suspicion may only be a side
    observation; recorded inside the §3 block).

- **Not covered (out of scope per RULES §2/§3):**
  - All 53 class names (`SeqCAstNode`, …, `SeqCValue`) — present in `nm`
    mangled symbols, excluded.
  - `EValueCategory`, `EDirection`, `VarType` type names — present in
    mangled signatures, excluded.
  - All free-function and method names listed in §1 (`str`, `swap`,
    `printSeqCAst`, `toSeqCAst`, `toSeqCAstRecursor`, every ctor / dtor /
    `evaluate` / `print` / `clone` / `children` / `getListElements` /
    `getVarTypes` / accessor) — present in `nm`, excluded.
  - Static `SeqCAstNode` data members — none exist.
  - `#pragma once`, `static_assert`, `#define`/`#undef` not naming a macro
    of our own — not symbols.
