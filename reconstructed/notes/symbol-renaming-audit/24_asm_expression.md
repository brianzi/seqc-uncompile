# Batch 24 — asm_expression

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 2 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 2;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/asm_expression.hpp`
- `reconstructed/src/asm_expression.cpp`

Cross-read for usage:
- `reconstructed/src/asm_parser_context.cpp` (`addNode`, `addCommand`)
- `reconstructed/src/awg_assembler_impl_pipeline.cpp`
  (`assembleStringToExpressionsVec`, `assembleAsmList`, `assembleExpressions`)
- `reconstructed/src/asm_list.cpp` (`parseStringToAsmList`)
- `reconstructed/src/asm_optimize.cpp` (mentions `noOpt` semantics)
- `nm --demangle _seqc_compiler.so` for type/free-function exclusions.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AsmExpression` (type) | no | high | in binary symbol table | keep current (high) | not-misnomer |
| `AsmExprType` (enum) | no | medium | matches discriminator semantics | keep current (medium) | — |
| `AsmExprType::Container` | no | medium | dispatched as type==0 container | keep current (medium) | — |
| `AsmExprType::Register` | no | medium | type==1 reg, factory createRegister | keep current (medium) | — |
| `AsmExprType::Label` | no | medium | type==2 used for both names/labels | keep current (medium) | — |
| `AsmExprType::Integer` | no | medium | type==3 integer/immediate | keep current (medium) | — |
| `AsmExpression::type` | no | medium | discriminator, matches enum | keep current (medium) | — |
| `AsmExpression::name` | no | medium | command/register/label name | keep current (medium) | — |
| `AsmExpression::nopComment` | yes | medium | unread placeholder field | str2, secondaryName, keep current | in-scope (nm/strings: no hit → recon-introduced) |
| `AsmExpression::command` | no | medium | Assembler::Command enum value | keep current (medium) | — |
| `AsmExpression::value` | unsure | low | int/regNum/PC overload | immediateOrRegNum, keep current | — |
| `AsmExpression::children` | no | high | child operand vector | keep current (high) | — |
| `AsmExpression::labelIndex` | yes | medium | labelPc / opcode position not "index" | labelPc, opcodeIndex, keep current | coordinated-rename |
| `AsmExpression::labelPc()` (alias) | no | medium | accurately names same field | keep current (medium) | coordinated-rename |
| `AsmExpression::lineNumber()` (alias) | yes | high | not source line; opcode counter | remove alias / rename to labelPc | coordinated-rename |
| `AsmExpression::labelName` | no | high | label string field | keep current (high) | — |
| `AsmExpression::hasLabel` | no | medium | guards labelName destruction | keep current (medium) | — |
| `AsmExpression::labelType()` (alias) | yes | medium | bool, not a "type" | hasLabel (drop alias) | coordinated-rename |
| `AsmExpression::comment` | no | high | comment / JSON blob | keep current (high) | — |
| `AsmExpression::hasComment` | no | medium | guards comment destruction | keep current (medium) | — |
| `AsmExpression::noOpt()` (alias) | unsure | low | semantically true at use-sites | keep alias, keep current | — |
| `AsmExpression::isWaveformCmdOverride_` | no | high | matches asm_list usage | keep current (high) | — |
| `~AsmExpression` | no | high | in binary symbol table | keep current (high) | not-misnomer |
| `createValue` (free fn) | no | high | in binary symbol table | keep current (high) | not-misnomer |
| `createValue::value` (param) | no | medium | stored into expr->value | keep current (medium) | — |
| `createRegister` (free fn) | no | high | in binary symbol table | keep current (high) | not-misnomer |
| `createRegister::regNum` (param) | no | medium | stored into expr->value | keep current (medium) | — |
| `createName` (free fn) | no | high | in binary symbol table | keep current (high) | not-misnomer |
| `createName::name` (param) | no | medium | becomes expr->name | keep current (medium) | — |
| `createArgList` (free fn) | no | high | in binary symbol table | keep current (high) | not-misnomer |
| `createArgList::first` (param) | no | medium | first child pushed | keep current (medium) | — |
| `appendArgList` (free fn) | no | high | in binary symbol table | keep current (high) | not-misnomer |
| `appendArgList::list` (param) | no | medium | container being appended to | keep current (medium) | — |
| `appendArgList::child` (param) | no | medium | child being appended | keep current (medium) | — |
| `str` (free fn) | no | high | in binary symbol table | keep current (high) | not-misnomer |
| `str::expr` (param) | no | medium | expression to stringify | keep current (medium) | — |

## 3. Detailed findings

### AsmExpression (type)  [no / high / not-misnomer]

Evidence:
- `nm --demangle _seqc_compiler.so` → `zhinst::AsmExpression::~AsmExpression()`,
  `zhinst::createValue(int)`, `zhinst::createName(char const*)`, etc., all
  mention `AsmExpression` as a qualified type name in the mangled symbol.

Interpretation:
- The class name appears verbatim in the binary's mangled symbols.

Judgement:
- Authoritative: type name is from the original binary; out of rename scope.

Locations consulted:
- declared: include/zhinst/asm_expression.hpp:104
- nm: `_seqc_compiler.so` symbol table.

### AsmExpression::nopComment  [yes / medium / verify-not-original]

Evidence:
- include/zhinst/asm_expression.hpp:108 — `std::string nopComment;  // +0x20  secondary string (NOP comment text)`
- src/asm_expression.cpp:32 — `// nopComment (+0x20): destroy string` (only
  destructor mention).
- `grep` for `nopComment` across `reconstructed/` produces zero
  read/write sites in implementation code; the only matches are the
  declaration, two destructor comments, and the placeholder rename
  table at `notes/placeholder_field_names.md:26` (`str2 → nopComment`,
  classified medium-confidence).
- src/awg_assembler_impl_pipeline.cpp:248 — at NOP-marker construction,
  comment text is assigned to `nopExpr->comment` (the `+0x80` field),
  not to `+0x20`.
- src/asm_list.cpp:410 — in the NOP marker case (`exprCmd == 4`), the
  consumer also reads `expr->comment` (`+0x80`), not the `+0x20` string.

Interpretation:
- The header's claim that `+0x20` holds the "NOP comment text" is not
  supported by any reader/writer in the reconstructed code. The actual
  NOP comment lives in the `comment` field (`+0x80`). The `+0x20`
  string is allocated and destroyed by the compiler-synthesized member
  functions but never observably read or written. The rename history
  in `placeholder_field_names.md` records this as a *medium*-confidence
  guess from `str2`.

Judgement:
- The current name asserts a role (NOP comment) for which there is no
  use-site evidence; it is a misleading label on an apparently-unused
  slot.

Proposals:
- `str2`                            (medium) — restore the agnostic
  placeholder until a true reader/writer is found.
- `secondaryName`                   (low)    — telegraphic, parallels
  `name` at `+0x08`; still speculative.
- keep current (`nopComment`)       (low)    — only viable if an actual
  binary writer for `+0x20` is located on second look (e.g. inside
  the bison-generated `asmparse`, which is not reconstructed).

Status `verify-not-original`: writer/reader may exist inside the
unreconstructed `asmparse` (0x292b50, ~217 KB). Synthesis should
re-verify against the binary before committing to a rename.

Locations consulted:
- declared: include/zhinst/asm_expression.hpp:108
- destroyed: src/asm_expression.cpp:32 (comment only)
- grep `nopComment`: only declaration + dtor comment + placeholder note.

### AsmExpression::value  [unsure / low / —]

Evidence:
- include/zhinst/asm_expression.hpp:110 — `int32_t value; // +0x3C
  integer value / register number / PC`.
- src/asm_expression.cpp:47 — `expr->value = value;` for type==3
  (integer immediate).
- src/asm_expression.cpp:59 — `expr->value = regNum;` for type==1
  (register number).
- src/asm_parser_context.cpp:338 — `cmd->value = pc;` in `addCommand`
  (program-counter snapshot).
- src/asm_list.cpp:482, 496-512, 526 — read as `e->value` and used
  variously as integer, `AsmRegister(...)` argument, and `Immediate(...)`
  argument depending on parent `command`.

Interpretation:
- One slot, three role overloads keyed by the discriminator `type`
  (and `command` in the addCommand case). The name `value` is generic
  enough to cover all three, but does not signal the overload.

Judgement:
- Unsure: a more specific name (e.g. `valueOrReg`) would not capture
  the third (PC) usage either; `value` is honest about the union-like
  nature.

Proposals:
- keep current (`value`)        (medium)
- `immediateOrRegNum`           (low)    — explicit but excludes PC use
- `payloadInt`                  (low)

Locations consulted:
- declared: include/zhinst/asm_expression.hpp:110
- written:  src/asm_expression.cpp:47,59; src/asm_parser_context.cpp:338
- read:     src/asm_list.cpp:482,496-512,526; src/awg_assembler_impl_pipeline.cpp (via getReg/getVal — symbol-table only).

### AsmExpression::labelIndex / labelPc() / lineNumber()  [coordinated-rename]

Evidence:
- include/zhinst/asm_expression.hpp:112 — `int32_t labelIndex; // +0x58
  label index/pc/counter` and the doc-comment block at lines 32–41
  explicitly states the field is "ALSO referred to as 'labelPc' or
  'lineNumber' in different reconstructed call sites — they are
  aliases."
- include/zhinst/asm_expression.hpp:129–132 — three aliases:
  `labelPc()`, `lineNumber()` declared as forwarding accessors.
- src/asm_parser_context.cpp:382 — `cmd->labelPc() = lbl.pc;` (write
  via `labelPc` alias from `addCommand`; comment "+0x58 (labelPc is
  alias for labelIndex)").
- src/awg_assembler_impl_pipeline.cpp:323 —
  `exprObj->lineNumber() = labelCounter;` where `labelCounter` is
  *not* a source line number; the surrounding code at lines 412–420
  shows it is incremented only for non-LABEL instructions, i.e. it
  tracks the **opcode position** (PC).
- src/awg_assembler_impl_pipeline.cpp:459 — first label-collection
  pass: `int labelIndex = e->lineNumber();` immediately reassigned to
  a variable named `labelIndex`, contradicting the alias name used in
  the read.

Interpretation:
- The single field at `+0x58` is consistently used as a label's
  program-counter / opcode-index value. The `labelPc()` alias names it
  correctly. The `lineNumber()` alias is actively misleading: it is
  *not* a source-code line number (those flow through `lineNumbers[]`
  in `assembleExpressions` and through `currentLine_`), and the one
  read site at line 459 immediately renames the value back to
  `labelIndex`. The base field `labelIndex` is acceptable but the
  `Pc` qualifier is more precise.

Judgement:
- The base name `labelIndex` is borderline (mild misnomer; "index"
  suggests array position rather than PC). The `lineNumber()` alias
  is a real misnomer. The `labelPc()` alias is correct.

Proposals (coordinated):
- Rename base field `labelIndex` → `labelPc`     (medium)
- Drop the `lineNumber()` alias entirely         (high)
- Drop the `labelPc()` alias (becomes the field) (high)
- keep current set (`labelIndex` + 3 aliases)    (low)

Cross-reference:
- `lineNumber` parameter name appears across batch 49
  (`AsmCommandsImpl::*::lineNumber`) and batch 10. Those batches
  concluded the parameter feeds into `wavetableFront` of `AsmList::Asm`
  and kept the name; that is a *separate* `lineNumber` (the
  AsmList-side line / wavetableFront alias). The misnomer here is the
  `AsmExpression::lineNumber()` alias only — no cross-batch conflict.

Locations consulted:
- declared: include/zhinst/asm_expression.hpp:112,129–132
- written:  src/asm_parser_context.cpp:382;
            src/awg_assembler_impl_pipeline.cpp:323
- read:     src/awg_assembler_impl_pipeline.cpp:459
- doc:      include/zhinst/asm_expression.hpp:32–41

### AsmExpression::labelType() (alias)  [yes / medium / coordinated-rename]

Evidence:
- include/zhinst/asm_expression.hpp:135–136 — `bool& labelType()`
  forwarding to `hasLabel`.
- src/asm_list.cpp:339 — `if (expr->labelType()) { ... instr.label =
  expr->labelName; }` — used as a boolean predicate.
- src/awg_assembler_impl_pipeline.cpp:326,332,458 — `if
  (exprObj->labelType() == 1)`, `exprObj->labelType() = 1;`, `if
  (e->labelType() == 1)`. Treated as bool-valued integer (compared
  to 1, assigned 1).
- include/zhinst/asm_expression.hpp:115 — underlying field is `bool
  hasLabel; // +0x78  true if label data present`.

Interpretation:
- All read/write sites use the value as a boolean. The alias name
  `labelType` (a noun suggesting an enum/category) is at odds with
  the actual boolean semantics; `hasLabel` is correct.

Judgement:
- The `labelType()` alias is a misnomer; the underlying `hasLabel`
  field is correct.

Proposals (coordinated with `labelIndex` block):
- Drop the `labelType()` alias; convert callers to `hasLabel`  (medium)
- keep both                                                    (low)

Locations consulted:
- declared: include/zhinst/asm_expression.hpp:115,135–136
- read/written: src/asm_list.cpp:339; src/awg_assembler_impl_pipeline.cpp:326,332,458

### AsmExpression::noOpt() (alias)  [unsure / low / —]

Evidence:
- include/zhinst/asm_expression.hpp:118,133–134 — `bool hasComment;
  // +0x98 ("noOpt" flag)` with alias `bool& noOpt()` returning
  `hasComment`.
- src/awg_assembler_impl_pipeline.cpp:234 — `ast->noOpt() =
  !parserCtx_.doOpt();` (set the bit when optimization is disabled).
- src/asm_list.cpp:543 — `if (expr->noOpt()) { ... boost::json::parse(
  expr->comment); ... entry.isWaveformCmd = true; }`.
- src/asm_optimize.cpp:342,453 — comments "Skip instructions with noOpt
  flag at +0xA0" — guard for the optimizer.

Interpretation:
- The semantics at use-sites are "this expression carries a noOpt /
  do-not-optimize JSON payload in `comment`". The alias `noOpt` is
  faithful to the intent of the writers and readers; `hasComment` is
  the literal layout description (set iff the comment string was
  written). The two are coextensive in the current code.

Judgement:
- Unsure: the alias is more semantically accurate than the field
  name itself, but the field name is a legitimate "guard for the
  optional string" pattern. No strong reason to flip.

Proposals:
- keep current (both)              (medium)
- Promote alias to field name; rename `hasComment` → `noOpt` (low) —
  would require updating the dtor pattern's "if hasX destroy stringX"
  reading.

Locations consulted:
- declared: include/zhinst/asm_expression.hpp:118,133–134
- written:  src/awg_assembler_impl_pipeline.cpp:234
- read:     src/asm_list.cpp:543; src/asm_optimize.cpp:342,453

### AsmExpression::isWaveformCmdOverride_  [no / high / —]

Evidence:
- include/zhinst/asm_expression.hpp:120 — declared.
- src/asm_list.cpp:568–570 — `// 0x26789d: Check
  expr->isWaveformCmdOverride_ (byte at +0xA0)\n if
  (expr->isWaveformCmdOverride_) { entry.isWaveformCmd = true; }`.
- notes/placeholder_field_names.md:19 — high-confidence rename from
  `field_A0` → `isWaveformCmdOverride_` already adopted.

Interpretation:
- The single read site sets `entry.isWaveformCmd = true;` precisely
  when the flag is set, exactly matching the name "isWaveformCmd
  override".

Judgement:
- Name accurately reflects the only observed use. Not a misnomer.

Locations consulted:
- declared: include/zhinst/asm_expression.hpp:120
- read:     src/asm_list.cpp:568–570

## 4. Symbols inspected and judged routinely fine

- `AsmExpression::type` — discriminator field, matches `AsmExprType`
  values at every factory and consumer.
- `AsmExpression::name` — string set by `createName` (label/name),
  also re-used by `addCommand` to extract command token; "name" is
  acceptable for both.
- `AsmExpression::command` — stores `Assembler::Command` enum value
  from `addCommand`; readers in asm_list.cpp:363,372,467 confirm.
- `AsmExpression::children` — `vector<shared_ptr<AsmExpression>>`,
  populated by `createArgList`/`appendArgList`; readers iterate as
  child operand list.
- `AsmExpression::labelName` — `std::string` written from label string
  in `addCommand` and `assembleAsmList`; read in
  asm_list.cpp:346 and awg_assembler_impl_pipeline.cpp:460. Name fits.
- `AsmExpression::hasLabel` — guard for `labelName` destruction.
  Matches `hasComment` pattern.
- `AsmExpression::comment` — written in `addNode` and
  `assembleStringToExpressionsVec` (line 231,248); read in asm_list.cpp
  for NOP and JSON paths. Name fits.
- `AsmExpression::hasComment` — guard for `comment` destruction; same
  pattern as `hasLabel`.
- `AsmExprType::{Container,Register,Label,Integer}` and the enum
  itself — match values 0/1/2/3 with the only construction sites
  in `createArgList`/`createRegister`/`createName`/`createValue`.
- `~AsmExpression` — name from binary symbol table.
- `createValue`, `createRegister`, `createName`, `createArgList`,
  `appendArgList`, `str` — all in binary symbol table; out of scope.
- `createValue::value`, `createRegister::regNum`,
  `createName::name`, `createArgList::first`, `appendArgList::list`,
  `appendArgList::child`, `str::expr` — each parameter has a single
  use that matches its name (assigned to corresponding field /
  pushed into children / used as the stringification target).

## 5. Coverage

- **Fully covered:** `asm_expression.hpp` and `asm_expression.cpp` in
  scope. Free-function bodies for `createValue`, `createRegister`,
  `createName`, `createArgList`, `appendArgList`, `~AsmExpression` are
  all reconstructed and surveyed; `str` is declared in this header but
  defined elsewhere — the parameter name `expr` was inspected at
  declaration only (definition not located in `asm_expression.cpp`;
  symbol at 0x28cd20 confirms presence of free-function definition).
- **Deferred:** None for the in-scope files. The `addNode`/`addCommand`
  free functions are declared in this header's logical orbit but are
  defined in `asm_parser_context.cpp` and belong to **batch 50**
  (asm_parser_context). Their parameters were not re-audited here;
  `addCommand`'s `cmd` (an output expression) and `args` (an input
  containing the name) are notable role-asymmetric param names that
  batch 50 should consider.
- **Not covered (out of scope per RULES §2/§3):** `AsmExpression`
  type name, `~AsmExpression`, `createValue`, `createRegister`,
  `createName`, `createArgList`, `appendArgList`, `str` —
  authoritatively present in the binary symbol table (verified via
  `nm --demangle _seqc_compiler.so`). Their parameter *names* remain
  in scope and were inspected (see §4).
