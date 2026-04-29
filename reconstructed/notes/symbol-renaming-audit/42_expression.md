# Batch 42 — expression

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 6 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 4;
> B3 (already resolved during Phase D/R): 1;
> B4 (wontfix / kept-as-is): 1.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/expression.hpp`
- `reconstructed/src/expression.cpp`

Cross-references consulted (read-only):
- `reconstructed/include/zhinst/seqc_ast_node.hpp` (downstream layout — SeqCAstNode
  members `valueCategory_`, `lineNr_`, `direction_`, `varType_`)
- `reconstructed/src/seqc_ast_node.cpp` (base ctor wiring)
- `reconstructed/src/compiler.cpp` (printAST uses field names)
- `reconstructed/src/seqc_parser.tab.c` / `seqc_parser.y` (parser uses
  `expr->valueType = 0` for assignment LHS)
- `reconstructed/notes/seqc_parser_grammar.md` (decodes `valueType=0` as
  `direction_=eIN`)
- `reconstructed/notes/frontend_lowering.md` (downstream consumers)

`nm --demangle _seqc_compiler.so` symbols relevant to §3 exclusion:

```
0x1bfa30 t zhinst::Expression::Expression(zhinst::Expression const&)
0x1bf260 t zhinst::createValue(zhinst::SeqcParserContext*, double)
0x1bf2d0 t zhinst::createString(zhinst::SeqcParserContext*, char const*)
0x1bf420 t zhinst::createVariable(zhinst::SeqcParserContext*, char const*)
0x1bf560 t zhinst::addVariableType(zhinst::SeqcParserContext*, zhinst::Expression*, zhinst::Expression*, bool)
0x1bf7c0 t zhinst::createVariableType(zhinst::SeqcParserContext*, zhinst::VarType)
0x1bf830 t zhinst::createOperator(zhinst::SeqcParserContext*, zhinst::Expression*, zhinst::Expression*, zhinst::EOperator)
0x1bf9c0 t zhinst::createAssignOperator(zhinst::SeqcParserContext*, zhinst::Expression*, zhinst::Expression*, zhinst::EOperator)
0x1bfb50 t zhinst::createArray(zhinst::SeqcParserContext*, zhinst::Expression*, zhinst::Expression*)
0x1bfb70 t zhinst::createListType(zhinst::SeqcParserContext*, zhinst::EOperationType, zhinst::Expression*, zhinst::Expression*)
0x1bfd00 t zhinst::createOrAppendArgList(zhinst::SeqcParserContext*, zhinst::Expression*, zhinst::Expression*)
0x1bfd20 t zhinst::createOrAppendListType(zhinst::SeqcParserContext*, zhinst::EOperationType, zhinst::Expression*, zhinst::Expression*)
0x1bfe00 t zhinst::createOrAppendDeclList(...)
0x1bfe20 t zhinst::createOrAppendParamList(...)
0x1bfe40 t zhinst::createOrAppendStmtList(...)
0x1bfe60 t zhinst::createFunctionCall(zhinst::SeqcParserContext*, zhinst::Expression*, zhinst::Expression*)
0x1c0000 t zhinst::createFunction(zhinst::SeqcParserContext*, zhinst::Expression*, zhinst::Expression*, zhinst::Expression*)
0x1c0330 t zhinst::createCommand(zhinst::SeqcParserContext*, zhinst::ECommandType, int, ...)
0x1c0530 t zhinst::createIf(...)
0x1c06c0 t zhinst::createIfElse(...)
0x1c08d0 t zhinst::createSwitch(...)
0x1c0a60 t zhinst::createCase(...)
0x1c0bf0 t zhinst::createCondExpression(...)
0x1c0e00 t zhinst::createFor(...)
0x1c1080 t zhinst::createWhile(...)
0x1c1210 t zhinst::createRepeat(...)
0x1c13a0 t zhinst::createDoWhile(...)
0x1c1530 t zhinst::str(zhinst::EOperationType)
0x1c1790 t zhinst::str(zhinst::EOperator)
0x1c18e0 t zhinst::str(zhinst::ECommandType)
0x1bef20 t zhinst::copyExpression(std::shared_ptr<zhinst::Expression>)
0x2ca1b0 t seqc_error(zhinst::SeqcParserContext*, zhinst::Expression**, void*, char const*)
```

Authoritative exclusions from `nm` (per RULES §3):

- **Type names** excluded from rename: `Expression`, `EOperationType`,
  `EOperator`, `ECommandType` (all appear inside mangled symbols above).
  `VarType` is excluded (declared in another batch but appears in
  `createVariableType` mangling).
- **Free-function names** excluded: every `create*`, `addVariableType`,
  `copyExpression`, `seqc_error`, the three `str(...)` overloads, the
  copy constructor.
- **Parameter names** are NOT encoded in mangling — all in scope.
- **Data members** are NOT encoded — all in scope.
- **Enum members** (e.g. `eCOMMAND`, `eADD`) — not encoded — in scope.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `Expression::operationType` | no | high | matches downstream usage; serializer-like `str()` | keep current (high) | not-misnomer |
| `Expression::valueCategory` | no | high | flows into `SeqCAstNode::valueCategory_` | keep current (high) | not-misnomer |
| `Expression::lineNumber` | no | high | `ctx->currentLineNumber()` provenance | keep current (high) | not-misnomer |
| `Expression::pad0C_` | unsure | low | structural padding slot | keep current, `unused0C_` | — |
| `Expression::value` | no | medium | holds numeric literal value | keep current (medium) | — |
| `Expression::name` | no | high | identifier/string text | keep current (high) | not-misnomer |
| `Expression::children` | no | high | child AST nodes vector | keep current (high) | not-misnomer |
| `Expression::operator_` | no | high | `str()` returns "+" / "-" etc. | keep current (high) | not-misnomer |
| `Expression::commandType` | no | high | `str()` returns "eIF" etc. | keep current (high) | not-misnomer |
| `Expression::varType` | no | high | matches `SeqCAstNode::varType_` | keep current (high) | not-misnomer |
| `Expression::valueType` | yes | high | actually `EDirection` (eIN/eOUT/eINOUT) | `direction`, `directionRaw`, keep current | — |
| `Expression::Expression(other)::other` | no | high | conventional copy-ctor name | keep current | — |
| `EOperator::eNONE` | unsure | low | not absent operator; means "no-op" | keep current | — |
| `ECommandType::eNOCMD` | no | medium | "no command" sentinel default | keep current | — |
| `EOperationType` enumerators | no | high | binary `str()` returns same names | keep current (high) | not-misnomer |
| `EOperator` enumerators | no | high | `str()` returns C operator strings | keep current (high) | not-misnomer |
| `ECommandType` enumerators | no | high | `str()` returns "eIF" etc. | keep current (high) | not-misnomer |
| `createValue::val` | no | medium | stored into `e->value` | keep current | — |
| `createString::s` | unsure | low | one-letter, but immediately copied to `name` | `text`, keep current | — |
| `createVariable::name` | no | high | stored into `e->name` | keep current | — |
| `addVariableType::expr` | no | medium | the AST node being annotated | keep current | — |
| `addVariableType::typeExpr` | no | high | source of `varType` | keep current | — |
| `addVariableType::isConst` | yes | medium | semantics inverted vs name | `keepTypeExpr`, `cascade`, keep current | in-scope (parameter name; nm preserves only the bool type) |
| `createVariableType::vt` | no | high | stored into `e->varType` | keep current | — |
| `createOperator::lhs` / `rhs` | no | high | binary-op operands | keep current | — |
| `createAssignOperator::lhs` / `rhs` / `op` | no | high | compound-assign operands | keep current | — |
| `createArray::lhs` / `rhs` | unsure | low | "array" creator; `lhs`=array expr, `rhs`=index | `arrayExpr`, `indexExpr`; keep current | — |
| `createListType::opType` | no | high | the `EOperationType` to set | keep current | — |
| `createListType::lhs` / `rhs` | no | medium | first two list members | keep current | — |
| `createOrAppendListType::lhs` / `rhs` | yes | medium | `lhs` may be the existing list; not a left operand | `existing`/`newItem`; keep current | — |
| `createOrAppend{Arg,Decl,Param,Stmt}List::lhs` / `rhs` | yes | medium | thin wrappers; same as above | `existing`/`newItem`; keep current | coordinated-rename |
| `createFunctionCall::func` | no | high | callee node | keep current | — |
| `createFunctionCall::args` | no | high | arglist node | keep current | — |
| `createFunction::nameExpr` | yes | high | actually the type/return-type expr | `returnTypeExpr`, `typeSpec` | — |
| `createFunction::params` | yes | high | actually the function declarator | `declarator`, `funcDecl` | coordinated-rename |
| `createFunction::body` | no | high | function body block | keep current | — |
| `createCommand::cmd` / `count` | no | high | matches usage | keep current | — |
| `createIf::cond` / `body` | no | high | matches usage | keep current | — |
| `createIfElse::cond` / `thenBody` / `elseBody` | no | high | matches usage | keep current | — |
| `createSwitch::val` / `body` | unsure | low | `val` is the switched-upon expression | `selector`, keep current | — |
| `createCase::val` / `body` | unsure | low | `val` is the case label expression | `label`, keep current | — |
| `createCondExpression::cond` / `trueBranch` / `falseBranch` | no | high | matches usage | keep current | — |
| `createFor::init` / `cond` / `incr` / `body` | no | high | matches C-for semantics | keep current | — |
| `createWhile::cond` / `body` | no | high | matches usage | keep current | — |
| `createRepeat::count` / `body` | no | high | matches usage | keep current | — |
| `createDoWhile::body` / `cond` | no | high | matches usage | keep current | — |
| `copyExpression::expr` | no | high | the node to deep-copy | keep current | — |
| `seqc_error::msg` | no | high | bison error string | keep current | — |
| `pushChild::vec` / `raw` | no | medium | helper-local; obvious | keep current | — |
| `allocExpression` (file-static) | no | low | unused helper | keep current | — |
| `makeCommandNode::ctx` / `cmd` | no | high | obvious | keep current | — |

## 3. Detailed findings

### Expression::valueType  [yes / high / —]

Evidence:
- `expression.hpp:125` — declared `int32_t valueType` at +0x54 with
  comment "always 2 in binary".
- `expression.cpp:181, 249, 412, 439` — every `create*` function that
  touches it sets it to `2`.
- `seqc_parser.tab.c:1736,1751,…,1841,1887` and `seqc_parser.y:311,322,…,426`
  — eleven sites in the parser write `expr->valueType = 0` after
  building a compound-assignment LHS or an `eASSIGN` with a declared
  variable on the left.
- `notes/seqc_parser_grammar.md:389` — explicit decode:
  ```
  movl $0x0, 0x54(%rcx)   ; $1->valueType = 0 (= eIN direction)
  ```
- `notes/seqc_parser_grammar.md:394` — "The LHS variable in an
  assignment gets `direction_ = eIN = 0`".
- `seqc_ast_node.hpp:106` — base ctor signature
  `SeqCAstNode(EValueCategory vc, int type, EDirection dir)`.
- `seqc_ast_node.hpp:152` — `EDirection direction_; // +0x10`.
- `seqc_ast_node.cpp:47-50` — base ctor stores its 3rd argument into
  `direction_`. The value flowing into that 3rd argument comes from
  `Expression::valueType` at AST-build time.
- `types.hpp:62-71` — `enum class EDirection : int32_t { eIN=0, eOUT=1,
  eINOUT=2 }`. Default is `2` (eINOUT); parser writes `0` (eIN) on
  assignment LHS. Matches the observed values 1:1.
- `seqc_ast_nodes_evaluate.cpp:2952` — downstream consumer: `if
  (static_cast<int32_t>(direction_) != 0 && …)`.

Interpretation:
- The field's default value (2) and the only mutated value (0) line up
  exactly with `EDirection::eINOUT` and `EDirection::eIN`. The downstream
  AST node stores this field as `EDirection direction_`. The current name
  "valueType" describes neither the storage's role nor its enum domain,
  and it conflicts with the existing `varType` field on the same struct
  (which **is** a type).

Judgement:
- Yes, this is a misnomer. The field is a parameter direction
  (`EDirection`), not a "type" of any kind.

Proposals:
- `direction`           (high)   — semantic name; type could change to
                                   `EDirection` later but that's outside
                                   audit scope.
- `directionRaw`        (medium) — if keeping `int32_t` storage to mirror
                                   binary layout literally.
- keep current          (low)

Locations consulted:
- declared: include/zhinst/expression.hpp:112,125
- written: src/expression.cpp:181,249,412,439,574,577;
  src/seqc_parser.tab.c:1736,1751,1761,1771,1781,1791,1801,1811,1821,1831,1841,1887;
  src/seqc_parser.y:311–376,426
- consumed (downstream): src/seqc_ast_node.cpp:47-50;
  include/zhinst/seqc_ast_node.hpp:106,152;
  src/seqc_ast_nodes_evaluate.cpp:2928,2952

### Expression::valueCategory  [no / high / not-misnomer]

Evidence:
- `expression.cpp:158,173,359` — set to `2` for `eVALUE`,
  `eFUNCTIONCALL`, `eVALUE`-string nodes; otherwise default `0`.
- `seqc_ast_node.hpp:65,148` — downstream: `enum class EValueCategory`,
  field `EValueCategory valueCategory_; // +0x08`.
- `seqc_ast_node.cpp:48,84` — base ctor stores arg `vc` into
  `valueCategory_`; swap pairs them.
- The 1:1 dataflow `Expression::valueCategory → SeqCAstNode::valueCategory_`
  is direct.

Interpretation:
- Field name matches downstream type (`EValueCategory`) and field name
  (`valueCategory_`) verbatim. The two values seen (0, 2) are inside
  the enum.

Judgement:
- No. Cross-class consistency with a named enum is strong evidence the
  current name is correct.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/expression.hpp:116
- written: src/expression.cpp:158,173,359,560,577
- downstream: include/zhinst/seqc_ast_node.hpp:65,148;
  src/seqc_ast_node.cpp:47-50,84

### Expression::lineNumber  [no / high / not-misnomer]

Evidence:
- `expression.cpp:161,184,198,232,250,266,…` — every site writes
  `e->lineNumber = ctx->currentLineNumber();`.
- `compiler.cpp:198` — `std::cout << " line=" << expr->lineNumber;`.
- Downstream maps to `SeqCAstNode::lineNr_` (different name but same
  semantic — the AST-side rename is a separate audit batch).

Interpretation:
- Source provenance and downstream usage both clearly mean "source line
  number".

Judgement:
- No.

Proposals:
- keep current (high)

### Expression::operationType, ::commandType, ::varType, ::operator_, ::children, ::name, ::value  [no / high / not-misnomer]

Evidence:
- `str(EOperationType)` @ 0x1c1530 returns `"eCOMMAND"`, `"eFUNCTION"`,
  …, identical to enum-member names → faithful binary `.rodata`
  strings (RULES §4d tier-2).
- `str(EOperator)` @ 0x1c1790 returns `"+"`, `"-"`, `"*"`, etc.
- `str(ECommandType)` @ 0x1c18e0 returns `"eIF"`, `"eIFELSE"`, …
- `compiler.cpp:194-203` — `printAST` field-by-field uses the names
  directly (`expr->operationType`, `expr->operator_`, `expr->commandType`,
  `expr->varType`).
- `varType` matches downstream `SeqCAstNode::varType_` (same name).
- `children` is a `std::vector<std::shared_ptr<Expression>>` populated
  exclusively via `pushChild` from parser productions.
- `name` is the identifier text for variables / function names / string
  literals. `createString::s`, `createVariable::name`, `createFunction`
  all assign to it.
- `value` holds the parsed numeric literal (`createValue::val`).

Interpretation:
- All seven names match both their internal usage and the downstream
  AST node naming. The `str()` overloads' return strings are tier-2
  positive evidence (faithful binary strings).

Judgement:
- No, none of these are misnomers.

Proposals:
- keep current (high) for all seven.

### Expression::pad0C_  [unsure / low / —]

Evidence:
- `expression.hpp:118` — declared `int32_t pad0C_{};` with comment
  "padding — zeroed".
- `expression.cpp:52` — copied through in copy ctor (`pad0C_(other.pad0C_)`).
- No other read or write site found.

Interpretation:
- Acts as 4 bytes of structural padding between `lineNumber` (+0x08)
  and `value` (+0x10). The trailing underscore signals "private/
  pseudo-field"; the `pad` prefix tells readers this is not a semantic
  field.

Judgement:
- Unsure. The name is not misleading per se, but `pad0C_` blurs the
  line between "natural alignment padding the compiler would insert
  anyway" and "a real binary slot". If the binary explicitly zeroes
  +0x0C in `operator new`-zero-init, it might be a deliberately
  reserved field rather than padding; no evidence either way in scope.

Proposals:
- keep current     (medium)
- `reserved0C_`    (low) — neutral about whether it is padding or
                            unused-but-reserved storage.
- `unused0C_`      (low)

### addVariableType::isConst  [yes / medium / verify-not-original]

Evidence:
- `expression.hpp:162` — declared `bool isConst`.
- `expression.cpp:206-235` —
  ```
  if (expr->operationType == EOperationType::eDECLLIST) {
      for (auto& child : expr->children) {
          addVariableType(ctx, child.get(), typeExpr, /*isConst=*/true);
      }
      if (!isConst) { delete typeExpr; }
  } else {
      expr->varType = typeExpr->varType;
      if (!isConst) { delete typeExpr; }
      else { expr->lineNumber = ctx->currentLineNumber(); }
  }
  ```
- `seqc_parser.tab.c:1861,1973` — both grammar call sites pass `false`.
- The recursive self-call passes `true`.

Interpretation:
- The flag does not gate "is this a const variable". It controls
  ownership/lifetime of `typeExpr` (true = caller keeps it /
  `addVariableType` is being called from within the recursive
  `eDECLLIST` walk; false = `addVariableType` owns and deletes
  `typeExpr` on exit) and also gates the `lineNumber` update
  (true = update; false = don't).
- The two top-level call sites in the bison grammar always pass
  `false` (variable type and parameter type productions). The recursive
  call passes `true` to indicate "I am the inner walk; do not destroy
  `typeExpr`".
- The current name `isConst` strongly suggests const-ness, which the
  body does not check or set.

Judgement:
- Yes, almost certainly a misnomer. The flag is an internal recursion
  marker / ownership flag, not a const-ness marker. Confidence is
  medium because the binary may have used a `const`-related identifier
  for an overloaded purpose; we cannot inspect parameter names.

Proposals:
- `keepTypeExpr`   (medium) — true ⇒ caller retains ownership.
- `recursing`      (low)    — true ⇒ called from within the eDECLLIST loop.
- `cascade`        (low)
- keep current     (low)

Status: `verify-not-original` — parameter names are never in the symbol
table, but worth a synthesis-level double-check that no header or
documentation string makes this name authoritative.

Locations consulted:
- declared: include/zhinst/expression.hpp:161-162
- defined: src/expression.cpp:206-236
- called: src/seqc_parser.tab.c:1861,1973; src/seqc_parser.y (corresponding
  rules); recursive at src/expression.cpp:220

### createFunction::nameExpr and ::params  [yes / high / coordinated-rename]

Evidence:
- `expression.hpp:185` — declared
  `Expression* createFunction(SeqcParserContext*, Expression* name,
  Expression* params, Expression* body);` (header still says `name`).
- `expression.cpp:371-398` —
  ```
  Expression* createFunction(..., Expression* nameExpr,
                                  Expression* params, Expression* body) {
      …
      // Note: despite the parameter name, 'params' is the function
      // declarator ($2) and 'nameExpr' is the type_specifier/return
      // type ($1).
      pushChild(e->children, params);
      …
      if (params && params->children.size() > 1) { … }
      pushChild(e->children, body);
      pushChild(e->children, nameExpr);   // pushed last as the return type
  }
  ```
- `seqc_parser.y:` — bison rule passes `$1` (type_specifier), `$2`
  (function_declarator), `$3` (compound_statement).
- `notes/incidental_findings.md:813` — recorded item: "Wrong arg mapping
  in `createFunction`: used `type_specifier` instead [of declarator]".

Interpretation:
- The author of the recon already noted in a code comment that both
  parameter names are misleading. `nameExpr` is the return-type
  expression, not a name; `params` is a declarator (an `eFUNCTIONCALL`
  node carrying both the function's name and its parameter list), not a
  raw parameter list.

Judgement:
- Both names are misnomers. High confidence — confirmed by a comment in
  the source itself and by an incidental finding.

Proposals (`nameExpr`):
- `returnTypeExpr` (high)
- `typeSpec`       (medium)
- keep current     (low)

Proposals (`params`):
- `declarator`     (high)
- `funcDecl`       (medium)
- keep current     (low)

Status: `coordinated-rename` — the two should be renamed together so the
header-declaration parameter names (`name`, `params`) and the definition
parameter names (`nameExpr`, `params`) become consistent.

Locations consulted:
- declared: include/zhinst/expression.hpp:185-186
- defined: src/expression.cpp:371-398
- called: src/seqc_parser.tab.c:2237; src/seqc_parser.y rule
  function_definition

### createOrAppendListType::lhs / ::rhs (and 4 wrappers)  [yes / medium / coordinated-rename]

Evidence:
- `expression.cpp:314-324`:
  ```
  if (lhs != nullptr && lhs->operationType == opType) {
      pushChild(lhs->children, rhs);
      return lhs;
  }
  return createListType(ctx, opType, lhs, rhs);
  ```
- The four wrappers (`createOrAppendArgList`, …`DeclList`, …`ParamList`,
  …`StmtList`) all forward `lhs`, `rhs` unchanged.

Interpretation:
- `lhs` is overloaded: when it already has matching `operationType`, it
  is the *existing list to extend*; otherwise it is the *first member*
  of a new pair. The "left operand" reading (which `lhs` suggests) only
  fits the second branch.
- `rhs` is always the *new item* being appended.

Judgement:
- Yes. `lhs`/`rhs` are appropriate for true binary operators (e.g.
  `createOperator`) but misleading for an append-or-create function
  where the first argument may be an accumulator.

Proposals:
- `existing` / `newItem`  (medium) — describes both branches.
- `acc` / `item`          (low)
- `head` / `tail`         (low)
- keep current            (low)

Status: `coordinated-rename` — applying to `createOrAppendListType`
naturally cascades to all four wrappers.

Locations consulted:
- declared: include/zhinst/expression.hpp:172-182
- defined: src/expression.cpp:314-348

### createArray::lhs / ::rhs  [unsure / low / —]

Evidence:
- `expression.cpp:305-308` — thin wrapper:
  `createListType(ctx, EOperationType::eARRAY, lhs, rhs);`
- `seqc_parser.tab.c:1460,1937,1943` — called as
  `createArray(ctx, $arrayExpr, $indexExpr)` for `arr[i]` indexing and
  `createArray(ctx, $arrayExpr, NULL)` for `arr[]`.

Interpretation:
- `lhs`/`rhs` are reasonable but obscure: `lhs` is the array variable
  expression; `rhs` is the index (or NULL). The names do not lie, but
  do not aid the reader either.

Judgement:
- Unsure. Worth flagging quietly; not strong enough to override
  consistency with `createListType`/`createOperator`.

Proposals:
- keep current (medium)
- `arrayExpr` / `indexExpr` (low)

### createString::s  [unsure / low / —]

Evidence:
- `expression.cpp:169` — `createString(SeqcParserContext* ctx, const char* s)`.
- `expression.cpp:183` — `e->name = s;`.

Interpretation:
- One-letter parameter; immediately copied to `name`. Not actively
  misleading, just terse.

Judgement:
- Unsure / borderline.

Proposals:
- `text`        (low)
- `literal`     (low)
- keep current  (medium)

### EOperator::eNONE  [unsure / low / —]

Evidence:
- `expression.hpp:66` — `eNONE = 21,  // "" (unset / default)`.
- `str(EOperator)::eNONE` returns `""` (empty string).
- Used as the default `operator_` in non-operator nodes.

Interpretation:
- The name says "no operator", which fits the default usage. Could
  arguably be `eUnset` or `eNoOp`, but `eNONE` is consistent with
  `ECommandType::eNOCMD` and is a common pattern for sentinel
  enumerators.

Judgement:
- Unsure. No active misleading; flagged only for completeness.

Proposals:
- keep current (high)

## 4. Symbols inspected and judged routinely fine

- `Expression::operator_` — trailing underscore avoids C++ keyword
  collision; field clearly holds an `EOperator`.
- `Expression::Expression(const Expression& other)::other` — conventional.
- `EOperationType` enumerators (`eCOMMAND`, `eFUNCTION`,
  `eFUNCTIONCALL`, `eVARIABLE`, `eOPERATOR`, `eARRAY`, `eARGLIST`,
  `eDECLLIST`, `ePARAMLIST`, `eSTMTLIST`, `eLABEL`, `eVARIABLETYPE`,
  `eVALUE`) — `str()` returns the same names verbatim (tier-2
  positive evidence).
- `EOperator` enumerators (`eADD`…`eASSIGN`) — `str()` returns the
  matching C operator characters; one-to-one with the source language.
- `ECommandType` enumerators (`eIF`…`eNOCMD`) — `str()` returns the
  same names verbatim.
- `createValue::val` — stored into `e->value`.
- `createValue::ctx`, and every other `ctx` parameter — universal
  abbreviation for the parser context throughout this file.
- `createVariable::name` — stored into `e->name`.
- `createVariableType::vt` — short for `varType`; stored into
  `e->varType`.
- `addVariableType::expr`, `::typeExpr` — clear roles.
- `createOperator::lhs`, `::rhs`, `::op` — true binary operator.
- `createAssignOperator::lhs`, `::rhs`, `::op` — true compound-assign.
- `createListType::opType` — the `EOperationType` to set.
- `createListType::lhs`, `::rhs` — the first two list members.
- `createFunctionCall::func`, `::args` — clear roles.
- `createCommand::cmd`, `::count` — variadic-helper meaning is obvious.
- `createIf::cond`, `::body`; `createIfElse::cond`, `::thenBody`,
  `::elseBody`; `createCondExpression::cond`, `::trueBranch`,
  `::falseBranch`; `createFor::init`, `::cond`, `::incr`, `::body`;
  `createWhile::cond`, `::body`; `createRepeat::count`, `::body`;
  `createDoWhile::body`, `::cond`; `createSwitch::val`, `::body`;
  `createCase::val`, `::body` — all match standard control-flow
  vocabulary.
- `copyExpression::expr` — the node to deep-copy.
- `seqc_error::ctx`, `::msg` — bison-style error callback.
- `pushChild::vec`, `::raw` — file-static helper, tiny scope.
- `allocExpression` — file-static, currently unused (dead helper).
- `makeCommandNode::ctx`, `::cmd` — file-static helper, obvious.
- `str(EOperationType)::t`, `str(EOperator)::op`, `str(ECommandType)::ct` —
  conventional one/two-letter names for enum-to-string helpers.

## 5. Coverage

- **Fully covered:**
  - All data members of `Expression`.
  - All enum types (`EOperationType`, `EOperator`, `ECommandType`) and
    every enumerator therein.
  - Every parameter of every free function and the copy constructor in
    `expression.{hpp,cpp}`.
  - Two file-static helpers (`pushChild`, `makeCommandNode`,
    `allocExpression`).
- **Deferred:**
  - The `int type` parameter of `SeqCAstNode::SeqCAstNode` is clearly
    misnamed (the body stores it into `lineNr_`) but lives in another
    batch (seqc_ast_node) — flagged here only for cross-batch context;
    not part of this report's findings.
  - Whether `Expression::valueType`'s storage type should change from
    `int32_t` to `EDirection` — typing decisions are out of scope per
    RULES §2a; recorded as side observation in the §3
    `Expression::valueType` block.
- **Not covered (out of scope per RULES §2/§3):**
  - The struct/class type names `Expression`, `EOperationType`,
    `EOperator`, `ECommandType`: present in `nm` mangled symbols, so
    excluded.
  - All free-function names (`createValue`, `createString`, …,
    `copyExpression`, `seqc_error`, the three `str` overloads): present
    in `nm`, excluded.
  - `Expression::Expression(const Expression&)` copy-constructor name:
    present in `nm`, excluded.
  - `VarType` and its enumerators: belong to another batch
    (`resources.hpp`).
  - Macro `static_assert` arguments and `#pragma once` — not symbols.
