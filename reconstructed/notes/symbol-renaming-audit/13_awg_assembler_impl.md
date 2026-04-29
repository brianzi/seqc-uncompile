# Batch 13 — awg_assembler_impl

## 1. Files considered

- `reconstructed/include/zhinst/awg_assembler_impl.hpp`
- `reconstructed/src/awg_assembler_impl.cpp`
- `reconstructed/src/awg_assembler_impl_pipeline.cpp`
- `reconstructed/src/awg_assembler_opcodes.cpp`

Cross-referenced (read for context, not for findings):

- `reconstructed/include/zhinst/asm_expression.hpp` (field
  `isWaveformCmdOverride_`, accessor `noOpt()`/`labelType()`)
- `reconstructed/include/zhinst/asm_parser_context.hpp` (declared
  `parserCtx_` member type)
- Prior reports 33, 26, 25, 44, 10, 49, 06, 24, 23.

### Symbol-table results (`nm --demangle _seqc_compiler.so`)

All **method symbols** of `AWGAssemblerImpl` are present in `nm` and
are therefore tier-1 excluded from rename per RULES §3:

```
0x285180  AWGAssemblerImpl::AWGAssemblerImpl(DeviceConstants const&)
0x2853c0  AWGAssemblerImpl::~AWGAssemblerImpl()
0x285620  AWGAssemblerImpl::assembleExpressions(...)
0x285b20  AWGAssemblerImpl::evaluate(shared_ptr<AsmExpression> const&)
0x285bc0  AWGAssemblerImpl::getReport() const
0x285ec0  AWGAssemblerImpl::assembleFile(string const&)
0x286490  AWGAssemblerImpl::assembleString(string const&)
0x286ca0  AWGAssemblerImpl::getAST(string const&)
0x286e40  AWGAssemblerImpl::assembleStringToExpressionsVec(string const&)
0x2876a0  AWGAssemblerImpl::assembleAsmList(vector<Assembler> const&)
0x288560  AWGAssemblerImpl::setMemoryOffset(unsigned int)
0x288570  AWGAssemblerImpl::writeToFile(string const&)
0x288b50  AWGAssemblerImpl::printOpcode(int) const
0x289060  AWGAssemblerImpl::getOpcode() const
0x289070  AWGAssemblerImpl::errorMessage(string const&)
0x289190  AWGAssemblerImpl::parserMessage(int, string const&)
0x2892b0  AWGAssemblerImpl::getReg(...)
0x289370  AWGAssemblerImpl::getVal(..., int)
0x2895c0..0x28a610  AWGAssemblerImpl::opcode0..opcode5(...)
```

The class type name `AWGAssemblerImpl` and the nested type
`AWGAssemblerImpl::Message` are also present (the latter via
`std::vector<AWGAssemblerImpl::Message>...` symbols and the
`__emplace_back_slow_path<AWGAssemblerImpl::Message const&>` helper at
0x28ad80) — both tier-1 excluded.

Method declarations that **are NOT** in `nm`:
`parseLine`, `parseString`, `encodeExpressions`, `extractComment`.
These are reconstruction names (some, like `extractComment`, were
inlined by the compiler per the source comment at line 686). They
remain in scope.

The `Assembler` element type used in `assembleAsmList`'s parameter
mangles to `zhinst::Assembler` in the symbol table — confirms
batch 33 / batch 26's finding that the reconstructed
`AssemblerInstr` should be `Assembler` (not re-flagged here; it is
not declared in this batch's files).

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `LabelBimap` (using-alias) | no | medium | descriptive structural alias | keep current (high) | not-misnomer |
| `AWGAssemblerImpl::Message` (nested type) | no | high | tier-1 binary symbol | keep current (high) | not-misnomer |
| `AWGAssemblerImpl::Message::code` (field) | yes | medium | dual line#/severity meaning | keep current (medium), `lineOrLevel` (medium) | — |
| `AWGAssemblerImpl::Message::text` (field) | no | high | message body string | keep current (high) | — |
| `AWGAssemblerImpl::deviceConstants_` | no | high | DeviceConstants pointer | keep current (high) | — |
| `AWGAssemblerImpl::filename_` | no | high | source filename string | keep current (high) | — |
| `AWGAssemblerImpl::asmSource_` | no | high | full asm source text | keep current (high) | — |
| `AWGAssemblerImpl::unusedStr038_` | unsure | low | no observed reader/writer | keep current (medium) | in-scope (nm/strings: no hit → recon-introduced) |
| `AWGAssemblerImpl::opcodes_` | no | high | vector of emitted opcodes | keep current (high) | — |
| `AWGAssemblerImpl::memoryOffset_` | no | high | matches setMemoryOffset | keep current (high) | — |
| `AWGAssemblerImpl::pad_memOffset_` | no | medium | alignment slot | keep current (high) | — |
| `AWGAssemblerImpl::currentLine_` | no | medium | per-line cursor | keep current (high) | — |
| `AWGAssemblerImpl::pad_currentLine_` | no | medium | alignment slot | keep current (high) | — |
| `AWGAssemblerImpl::sourceLines_` | no | high | per-line source vector | keep current (high) | — |
| `AWGAssemblerImpl::messages_` | no | high | error/warning vector | keep current (high) | — |
| `AWGAssemblerImpl::labelBimap_` | no | high | label↔index bimap | keep current (high) | — |
| `AWGAssemblerImpl::parserCtx_` | no | high | embedded AsmParserContext | keep current (high) | — |
| ctor::dc | no | high | DeviceConstants reference | keep current (high) | — |
| assembleFile::path | no | high | path forwarded as filename | keep current (high) | — |
| assembleString::src | no | high | conventional asm source name | keep current (high) | — |
| assembleString::expressions | no | high | local AST list | keep current (high) | — |
| assembleString::lineNumbers | yes | medium | only label lines pushed | `labelLineNumbers` (medium), keep current (low) | — |
| assembleString::line | no | high | one source line | keep current (high) | — |
| assembleString::ast | no | high | parsed expression | keep current (high) | — |
| getAST::line | no | high | text being parsed | keep current (high) | — |
| getAST::scanner | no | high | flex scanner handle | keep current (high) | — |
| getAST::buf | no | high | flex buffer handle | keep current (high) | — |
| getAST::rawExpr | no | high | raw parser output | keep current (high) | — |
| getAST::parseResult | no | high | asmparse return code | keep current (high) | — |
| getAST::result | no | high | shared_ptr being built | keep current (high) | — |
| assembleStringToExpressionsVec::src | no | high | asm source string | keep current (high) | — |
| assembleStringToExpressionsVec::result | no | high | the returned vector | keep current (high) | — |
| assembleStringToExpressionsVec::comment, nopExpr | no | high | obvious roles | keep current (high) | — |
| assembleAsmList::asmList | no | high | vector of Assembler items | keep current (high) | — |
| assembleAsmList::labelCounter | unsure | medium | counts non-LABEL emits | `instrIndex` (medium), keep current (medium) | — |
| assembleAsmList::lineNumbers, expressions | no | medium | mirror of assembleString | keep current (high) | — |
| assembleAsmList::instr | no | high | range-for of asmList | keep current (high) | — |
| assembleAsmList::expr / exprObj | no | medium | locally constructed AST | keep current (high) | — |
| assembleAsmList::labelStr | no | high | copy of instr.label | keep current (high) | — |
| assembleAsmList::regIdx, regExpr, valExpr, child, val, imm, instrStr, nameExpr | no | high | conventional / obvious | keep current (high) | — |
| assembleExpressions::expressions | no | high | input AST list | keep current (high) | — |
| assembleExpressions::lineNumbers | yes | medium | mismatched-length input | `labelLineNumbers` (medium), keep current (low) | cross-batch-arbitration |
| assembleExpressions::numExprs, opcode, i, e, labelIndex, labelName | no | high | trivial locals | keep current (high) | — |
| evaluate::expr / e / cmd / opcodeType | no | high | direct dispatch locals | keep current (high) | — |
| getReport::oss / msg | no | high | builder + element | keep current (high) | — |
| errorMessage::msg (param renamed `text` in body) | unsure | low | header `msg`, body `text` | unify on `text` (medium) | — |
| errorMessage::msg (local Message) | no | high | the constructed Message | keep current (high) | — |
| parserMessage::level | no | high | severity int written into code | keep current (high) | — |
| parserMessage::text / msg (local) | no | high | obvious | keep current (high) | — |
| writeToFile::outputPath | no | high | output file path | keep current (high) | — |
| writeToFile::elf, comment, commentOss, sectionName, basename, filenameStr, filenameSec, asmSec | no | high | obvious roles | keep current (high) | — |
| printOpcode::startIndex (header: `format`) | yes | high | offset added to displayed idx | `startIndex` (high) | coordinated-rename |
| printOpcode::idx, numOpcodes, labelName, labelBimap, it, numLines | no | high | obvious roles | keep current (high) | — |
| extractComment (method name) | no | high | substring after `//` | keep current (high) | — |
| extractComment::line, ::pos | no | high | obvious | keep current (high) | — |
| getReg::expr / e / regNum / dc | no | high | clear roles | keep current (high) | — |
| getVal::expr / bits | no | high | bit-width param | keep current (high) | — |
| getVal::e, val, mask, msg, it, labelMap | no | high | conventional | keep current (high) | — |
| opcode0..5::opcode (param) | unsure | medium | header says `base`, body `opcode` | `base` (low), keep current (medium) | — |
| opcode0..5::expr | no | high | the AST node | keep current (high) | — |
| opcode0..5::children_begin, children_end, byte_size | unsure | medium | sizes are *byte* counts not element counts | rename to `*Bytes` or use `.size()` (low), keep current (medium) | — |
| opcode4::kOpcodeGroup1Child / kOpcodeGroup2Child | unsure | low | fudge constants for jump table | descriptive name TBD; keep current (medium) | — |
| opcode4::idx, idxRotated, opcodeGroup, groupRotated, nChildren | no | medium | mechanical locals | keep current (high) | — |
| opcode3::isRegRegALU, remaining, reg2 | no | medium | obvious roles | keep current (high) | — |
| `parseLine`, `parseString`, `encodeExpressions` (header-only declarations) | unsure | low | declared, never defined or called | drop, or annotate (medium) | in-scope (no AWGAssemblerImpl::parseLine/parseString/encodeExpressions in nm) |

Group order: types → fields → constructors → public methods (each with
params then locals) → opcode helpers.

## 3. Detailed findings

### `LabelBimap` (using-alias)  [no / medium / not-misnomer]

Evidence:
- `awg_assembler_impl.hpp:21-23`:
  ```
  using LabelBimap = boost::bimaps::bimap<
      std::string,
      boost::bimaps::multiset_of<int>>;
  ```
- All uses (`labelBimap_`, `getLabelBimap()`, `getVal` lookup, label
  registration in `assembleExpressions`) treat the alias as exactly
  that boost type.
- The alias is not in `nm` (it is a template alias and so does not
  produce a symbol on its own).

Interpretation:
- The name describes both content (label name → index) and structural
  type (a bimap) and matches every observed use site.

Judgement:
- No.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/awg_assembler_impl.hpp:21-23
- used: hpp:156, hpp:170; pipeline.cpp:453,462; opcodes.cpp:141-143;
  pipeline.cpp:644-649

---

### `AWGAssemblerImpl::Message` (nested type) and its fields  [type=no / fields: code=yes (medium), text=no / not-misnomer for type]

Evidence (type):
- `nm` lines:
  `std::__1::vector<zhinst::AWGAssemblerImpl::Message, …>`,
  `__emplace_back_slow_path<zhinst::AWGAssemblerImpl::Message const&>`
  at 0x28ad80, dtor `~vector<AWGAssemblerImpl::Message …>` at 0x285340.
- The qualified name appears verbatim in mangled symbols.

Evidence (`code` field):
- Header comment (hpp:97-103) explicitly documents the dual purpose:
  written by `errorMessage` from `currentLine_`, written by
  `parserMessage` from the `level` argument; "the same field; the
  binary does not distinguish them. There is NO separate `lineNumber`
  slot."
- pipeline.cpp:557 `msg.code = currentLine_;` (errorMessage).
- pipeline.cpp:574 `msg.code = level;` (parserMessage).
- getReport prints it as `"Assembler message at " << msg.code << " : "`
  which works for the line# usage but reads oddly when the value is a
  severity level — confirming the meaning is contextual, not fixed.

Interpretation:
- The type name is authoritative. The member `code` holds a value
  whose meaning depends on which writer set it. `code` is generic
  enough to cover both meanings, but its name doesn't communicate
  the dual nature; readers expecting a numeric error-code may
  misinterpret the line-number case.

Judgement:
- Type: not a misnomer (positive evidence).
- `text`: not a misnomer (matches all use sites).
- `code`: yes — the name is misleading because at every error-message
  site the value is a *line number*, not an error code. The dual
  parser-severity use is the minority case.

Proposals (`code`):
- `lineOrLevel` (medium) — surfaces the dual role.
- `lineNumber` (low) — fits the majority site only.
- keep current (medium) — generic, but accepted convention for a
  "first int field of a message struct".

Locations consulted:
- declared: hpp:97-107
- used: pipeline.cpp:542-544, 555-561, 572-578

---

### `AWGAssemblerImpl::unusedStr038_`  [unsure / low / verify-not-original]

Evidence:
- hpp:162: `std::string unusedStr038_;  // 0x038 — no observed reader/writer`.
- The ctor and dtor (impl.cpp:10, impl.cpp:60) zero/destroy it but no
  other code in this batch reads or writes it.
- The header comment for the layout already flags: "no observed
  reader/writer in reconstructed methods".

Interpretation:
- The name is a placeholder describing the auditor's lack of
  knowledge. It might carry a real role uncovered by code paths the
  reconstruction does not implement (or the binary reads it
  somewhere not yet correlated).

Judgement:
- Unsure — the name is honest about its uncertainty but not informative.

Proposals:
- keep current (medium) until a reader is identified.

Status: `verify-not-original` — would benefit from a deeper trace of
the binary at offset 0x38 of the impl struct.

Locations consulted:
- declared: hpp:162; impl.cpp:10,60.

---

### `assembleString::lineNumbers` and `assembleExpressions::lineNumbers`  [yes / medium / cross-batch-arbitration]

Evidence:
- pipeline.cpp:118 `std::vector<uint64_t> lineNumbers;` declared as
  "line# for each expression".
- pipeline.cpp:142-145: only pushed when `ast->command == 2` (i.e.
  for **LABEL** expressions only).
- pipeline.cpp:280, 423: in `assembleAsmList` it IS pushed for every
  expression (one-to-one with the `expressions` vector).
- pipeline.cpp:439-441: the parameter to `assembleExpressions` is
  named `lineNumbers`.
- pipeline.cpp:467-470: indexed alongside `expressions` (`i++` per
  iteration), then `currentLine_ = lineNumbers[i]`.

Interpretation:
- In `assembleString`, the vector is shorter than `expressions`
  (only label-line entries), so indexing it inside
  `assembleExpressions` `1:1` against the expressions cannot be
  correct — the call from `assembleString` produces an
  out-of-bounds / shifted lookup unless the vector is unused for
  most of the loop.
- This may indicate the reconstruction in `assembleString` is wrong
  about *when* to push (the binary likely pushes per-line
  unconditionally), but the *name* is also misleading: the local in
  `assembleString` is in fact "lines for label definitions" while
  the local in `assembleExpressions` and `assembleAsmList` is "line
  for each expression". The same name covers two different things.

Judgement:
- Yes — the local in `assembleString` is misnamed for the role it
  plays in that function. The parameter in `assembleExpressions`
  is consistent with the assembleAsmList call site but inconsistent
  with the assembleString call site.

Proposals:
- assembleString::lineNumbers → `labelLineNumbers` (medium) — but
  this presumes the per-LABEL push is correct.
- keep current (low) — and instead investigate whether
  `assembleString`'s push logic is the real bug (out of audit
  scope; record as incidental).

Cross-reference:
- The mismatch between `assembleString`'s push pattern and
  `assembleExpressions`'s consumption pattern is a candidate
  reconstruction bug; it should be triaged into `incidental_findings.md`
  separately from this rename audit (logged here for synthesis only).

Locations consulted:
- pipeline.cpp:117-145, 161, 278-286, 423, 430, 439-470.

---

### `assembleAsmList::labelCounter`  [unsure / medium / —]

Evidence:
- pipeline.cpp:288: `int labelCounter = 0;`
- pipeline.cpp:323: `exprObj->lineNumber() = labelCounter;`
  (set on **LABEL** expressions only).
- pipeline.cpp:418-420: increment **only** when
  `instr.cmd != Assembler::LABEL` — i.e. it counts non-LABEL
  instructions.

Interpretation:
- The variable counts emitted (non-label) instructions, then
  stamps the label expression with the current count. So the value
  it stores in label expressions is "index of next instruction
  this label points to" — i.e. an instruction index, not a
  label index. The name `labelCounter` suggests the opposite
  ("counter of labels seen").

Judgement:
- Unsure — the variable touches both worlds (only assigned to
  labels, only incremented for non-labels). The name describes
  neither role accurately; `instrIndex` would be closer to its
  semantic, but the assignment site couples it tightly to label
  data.

Proposals:
- `instrIndex` or `nextInstrIndex` (medium)
- keep current (medium)

Locations consulted:
- pipeline.cpp:288, 323, 418-420.

---

### `printOpcode::startIndex` (header param `format`)  [yes / high / coordinated-rename]

Evidence:
- hpp:123: `void printOpcode(int format) const;`
- pipeline.cpp:635: `void AWGAssemblerImpl::printOpcode(int startIndex) const`
- pipeline.cpp:663,672: `(idx + startIndex)` printed as the displayed
  hex address.
- The parameter is added to the loop counter to produce the displayed
  instruction address — clearly a base-offset, not a "format" selector.
- Batch 33 flagged the wrapper-side `printOpcode::format` as `unsure`
  pending impl evidence.

Interpretation:
- The header declaration uses `format`; the implementation uses
  `startIndex`. The implementation name accurately describes the
  arithmetic. The header is a leftover misnaming.

Judgement:
- Yes — header parameter `format` is the misnomer; the impl name
  `startIndex` is correct.

Proposals:
- Header `format` → `startIndex` (high) — coordinated with the
  wrapper at `AWGAssembler::printOpcode(int format)`
  (`include/zhinst/awg_assembler.hpp:31`,
  `src/awg_assembler.cpp:74-77`).

Cross-reference:
- Batch 33 (awg_assembler) — wrapper side; deferred to this batch.

Locations consulted:
- declared: hpp:123, awg_assembler.hpp:31
- used: pipeline.cpp:635-682; awg_assembler.cpp:74-77

---

### `errorMessage::msg` vs `errorMessage::text` (parameter)  [unsure / low / —]

Evidence:
- hpp:152 `void errorMessage(std::string const& msg);`
- pipeline.cpp:554 `void AWGAssemblerImpl::errorMessage(const std::string& text)`.
- Parameter name is consistent within either site but inconsistent
  between the declaration and the definition.
- Compare `parserMessage`: hpp:153 `parserMessage(int level, std::string const& msg)`
  vs pipeline.cpp:572 `parserMessage(int level, const std::string& text)`.

Interpretation:
- Same divergence as for `printOpcode`: header-side and impl-side
  parameter names disagree. The body uses `text` consistently and
  `text` is what the value actually is (the message body distinct
  from severity/code). `msg` is generic.

Judgement:
- Unsure — both names are defensible; the inconsistency is the
  finding, not the wrongness of either.

Proposals:
- Unify on `text` in both header and impl (medium).
- keep current (low) — neither is misleading on its own.

Locations consulted:
- hpp:152-153; pipeline.cpp:554-565, 572-579.

---

### opcode helpers `opcode0..opcode5` parameter `opcode` vs header `base`  [unsure / medium / —]

Evidence:
- hpp:129-134:
  ```
  uint64_t opcode0(unsigned int base, std::shared_ptr<AsmExpression> const& expr);
  ...
  uint64_t opcode5(unsigned int base, ...);
  ```
- opcodes.cpp:187-651: every definition uses the parameter name
  `opcode` instead. E.g. opcodes.cpp:187:
  `uint64_t AWGAssemblerImpl::opcode0(unsigned int opcode, ...)`.
- Inside the bodies the value is OR'd with shifted register / value
  bits and returned: the value functions as the *base opcode* of the
  instruction word being assembled.

Interpretation:
- The parameter is genuinely the base opcode pattern that gets
  enriched with operand bits before being returned. The header name
  `base` describes that role; the body name `opcode` is generic and
  collides with the conceptual return value (also "an opcode").
- Header and body disagree.

Judgement:
- Unsure — body's `opcode` is acceptable but header's `base` is
  more informative; the inconsistency is the actionable finding.

Proposals:
- Unify on `base` across header + body (medium); reserve `opcode`
  for the return value's local where applicable.
- keep current (medium).

Locations consulted:
- hpp:129-134; opcodes.cpp:187-651 (all six definitions).

---

### opcode helpers' `children_begin / children_end / byte_size`  [unsure / medium / —]

Evidence:
- opcodes.cpp:212-214 (and parallel lines in opcode2/opcode3/opcode5):
  ```
  auto* children_begin = e->children.data();
  auto* children_end = children_begin + e->children.size();
  size_t byte_size = reinterpret_cast<const char*>(children_end)
                   - reinterpret_cast<const char*>(children_begin);
  ```
- The "size" here is computed in **bytes**, not elements: subsequent
  comparisons use `0x10`, `0x20`, `0x30`, `0x40` — multiples of
  `sizeof(shared_ptr) == 16`.
- Comments at lines 251, 253, 261, etc. all spell out "0x40 bytes =
  4 children" — confirming the auditor was aware of the unit.

Interpretation:
- The names `children_begin/_end` are accurate. `byte_size` is
  literally the byte size, which the author flagged in the comments;
  the variable is then compared against byte-magnitudes that match
  the binary's literal disassembly. The name is correct but the
  whole pattern propagates a binary-flavoured byte-arithmetic that
  obscures the simple "child count" intent.

Judgement:
- Unsure — names are honest about being byte counts, but they
  perpetuate an awkward style. Not strictly misnomers.

Proposals:
- Replace whole pattern with `e->children.size()` × elementsize and
  rename to `nChildren` (low) — out of pure-rename scope.
- keep current (medium).

Locations consulted:
- opcodes.cpp:212-214, 254-262, 315-326, 622-624.

---

### opcode4 constants `kOpcodeGroup1Child`, `kOpcodeGroup2Child`  [unsure / low / —]

Evidence:
- opcodes.cpp:455 `constexpr unsigned int kOpcodeGroup2Child = 0x0D000000u;`
- opcodes.cpp:533 `constexpr unsigned int kOpcodeGroup1Child = 0x0E000000u;`
- Used as additive offsets to bring an opcode-high-byte into a
  jump-table index range (rotated by 8 bits afterwards).

Interpretation:
- The names suggest these constants "select" 1-child or 2-child
  opcode groups, but they are arithmetic adjustments tied to the
  jump-table base (binary 0x95d0ac and a sibling table). Their role
  is "shift the opcode high-byte such that the smallest valid
  opcode maps to index 0".

Judgement:
- Unsure — the names hint at the right grouping but don't describe
  what the value actually does.

Proposals:
- `kOpcode2ChildBaseAdjust` / `kOpcode1ChildBaseAdjust` (low)
- keep current (medium).

Locations consulted:
- opcodes.cpp:455-457, 533-535.

---

### `parseLine`, `parseString`, `encodeExpressions` (header declarations)  [unsure / low / verify-not-original]

Evidence:
- hpp:137-140 declares three methods: `parseLine`, `parseString`,
  `encodeExpressions`.
- `nm --demangle _seqc_compiler.so | grep -E
  "AWGAssemblerImpl::(parseLine|parseString|encodeExpressions)"` — no
  results.
- No `.cpp` file in the batch defines them; no caller is visible.

Interpretation:
- These method names are reconstruction guesses for entry points that
  do not exist in the binary as separate symbols. They may be
  inlined-only (like `extractComment`), or they may be conjectural
  decompositions of code that lives entirely inside `assembleString`
  / `getAST`.

Judgement:
- Unsure — names cannot be checked because the methods have no
  observed implementation; they may even be spurious declarations.

Proposals:
- Resolve at synthesis: either delete the declarations if they
  represent no real code, or rename them to whatever the inlined
  helpers actually do (medium).
- keep current (low).

Status: `verify-not-original` — pending decision on whether the
declarations correspond to anything in the binary.

Locations consulted:
- hpp:137-141; whole-batch grep showed no definitions or calls.

---

## 4. Symbols inspected and judged routinely fine

- `deviceConstants_`, `filename_`, `asmSource_`, `opcodes_`,
  `memoryOffset_`, `currentLine_`, `sourceLines_`, `messages_`,
  `labelBimap_`, `parserCtx_` — all match every observed read/write
  site directly, with documented offsets in the header comment block.
- `pad_memOffset_`, `pad_currentLine_` — alignment slots; names tell
  the truth.
- `Message::text` — only ever the message string body.
- ctor parameter `dc`, all `std::string const&`/`std::vector<...>
  const&` parameters that are immediately stored or forwarded
  (`path`, `src`, `outputPath`, `asmList`, `expressions`).
- `getReport::oss`, `assembleString::iss`,
  `assembleStringToExpressionsVec::iss` — conventional `i/oss`
  stream names.
- `assembleAsmList::instr`, `imm`, `regIdx`, `regExpr`, `valExpr`,
  `child`, `val`, `instrStr`, `nameExpr` — short scope, names match
  the type of object they hold.
- `evaluate::e`, `cmd`, `opcodeType` — direct dispatch locals.
- `getReg::regNum`, `dc` — obvious.
- `getVal::it`, `val`, `mask`, `labelMap` — obvious bimap-lookup
  locals.
- `opcode3::isRegRegALU`, `remaining`, `reg2` — descriptive locals.
- `opcode4::idx`, `idxRotated`, `opcodeGroup`, `groupRotated`,
  `nChildren`, `actualChildren` — mechanical locals matching the
  jump-table dispatch in the disassembly.
- `opcode5::val` — single use, obvious.
- `printOpcode::idx`, `numOpcodes`, `numLines`, `labelName`,
  `labelBimap`, `it` — match their roles in the print loop.
- `writeToFile::elf`, `comment`, `commentOss`, `sectionName`,
  `basename`, `filenameStr`, `filenameSec`, `asmSec` — straightforward
  ELF-section assembly locals.
- `extractComment` (method name) and its locals (`pos`, `line`) — the
  function genuinely extracts a comment string; symbol-table absent
  because compiler inlined it.

## 5. Coverage

**Fully covered:**
- All in-scope members declared in `awg_assembler_impl.hpp` (fields,
  nested type `Message`, `LabelBimap` alias).
- All in-scope parameters in the header.
- Every named local of every method body in the three `.cpp` files
  was visited. Most are bullet-listed in §4.

**Deferred:**
- `unusedStr038_`: would benefit from a deeper binary trace to
  identify any reader.
- `assembleString::lineNumbers` (and the parameter
  `assembleExpressions::lineNumbers`): the *name* finding is
  recorded; the underlying control-flow mismatch is a separate
  reconstruction-correctness issue and belongs in
  `incidental_findings.md` (not edited as part of this read-only
  audit, per AGENTS.md and RULES §11).
- `parseLine` / `parseString` / `encodeExpressions` declarations:
  flagged `verify-not-original`.

**Not covered (out of scope per RULES §2/§3):**
- All `AWGAssemblerImpl` method names — every one is in
  `nm --demangle _seqc_compiler.so` (see §1 listing). Tier-1 excluded.
- Class type name `AWGAssemblerImpl`: tier-1 excluded.
- Nested type `AWGAssemblerImpl::Message`: tier-1 excluded
  (positive-evidence block in §3).
- Forward-declared type `AssemblerInstr` (used in
  `assembleAsmList`'s parameter): batch 33 / 26 already cover this.
  Not re-flagged here.
- Forward-declared types `DeviceConstants`, `AsmExpression`: their
  audits live in their own batches.
- Member types of embedded `AsmParserContext` / `Message::text` /
  `vector<...>` etc. — stdlib / project types audited elsewhere.
- Boost bimap internals (template and typename machinery) —
  third-party, out of scope per RULES §2.
