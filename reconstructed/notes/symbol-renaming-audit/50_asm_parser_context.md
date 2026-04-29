# Batch 50 — asm_parser_context

## 1. Files considered

- `reconstructed/include/zhinst/asm_parser_context.hpp`
- `reconstructed/src/asm_parser_context.cpp`

Cross-referenced (read-only) for usage:
- `reconstructed/src/awg_assembler_impl_pipeline.cpp`
- `reconstructed/src/asm_parser.y`
- `reconstructed/src/asm_lexer.l`
- `reconstructed/include/zhinst/awg_assembler_impl.hpp`
- `reconstructed/include/zhinst/asm_expression.hpp`
- `reconstructed/include/zhinst/yy_fwd.hpp`
- `reconstructed/notes/asm_parser_grammar.md`

Authoritative symbol-table check (see §3 Coverage). `nm --demangle
_seqc_compiler.so` shows the following names verbatim:

- `zhinst::AsmParserContext` (type)
- `zhinst::AsmParserContext::Label` (nested type, ctor + `operator==`)
- All 22 methods at 0x28e7a0–0x28ead0 (`isComment`, `isLineComment`,
  `startLineComment`, `endLineComment`, `startBlockComment`,
  `endBlockComment`, `disableOpt`, `enableOpt`, `doOpt`,
  `hadSyntaxError`, `setSyntaxError`, `clearSyntaxError`,
  `currentLineNumber`, `incrementLineNumber`, `programCounter`,
  `incrementProgramCounter`, `trackedStringCopy`, `cleanStringCopies`,
  `addLabel`, `hasLabel`, `setErrorCallback`, `raiseError`)
- `zhinst::addNode(zhinst::AsmParserContext*, char const*)`
- `zhinst::addCommand(zhinst::AsmParserContext*, zhinst::AsmExpression*, zhinst::AsmExpression*, int, char const*)`
- `asmerror`, `asmparse`, `asmset_extra`, `asmlex_init_extra`

All type/method/free-function names above are therefore **excluded
from rename per RULES §3** and not flagged below.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| **AsmParserContext::Label (nested type)** | | | | | |
| `Label::pc` | no | high | matches `programCounter()` semantics | keep current (high) | not-misnomer |
| `Label::name` | no | high | label string from grammar | keep current (high) | not-misnomer |
| `Label::Label::pc` (param) | no | medium | mirrors field; ctor passthrough | keep current (medium) | — |
| `Label::Label::name` (param) | no | medium | mirrors field; ctor passthrough | keep current (medium) | — |
| `Label::operator==::other` (param) | no | high | conventional `operator==` name | keep current (high) | — |
| **AsmParserContext (fields)** | | | | | |
| `AsmParserContext::isComment_` | no | medium | tracks any-comment state | keep current (medium) | — |
| `AsmParserContext::isLineComment_` | no | high | matches `startLineComment` | keep current (high) | not-misnomer |
| `AsmParserContext::isBlockComment_` | no | high | matches `startBlockComment` | keep current (high) | not-misnomer |
| `AsmParserContext::hadSyntaxError_` | no | high | matches `hadSyntaxError()` | keep current (high) | not-misnomer |
| `AsmParserContext::doOpt_` | no | medium | matches `doOpt()` getter | keep current (medium) | — |
| `AsmParserContext::lineNumber_` | no | high | matches `currentLineNumber()` | keep current (high) | not-misnomer |
| `AsmParserContext::programCounter_` | no | high | matches `programCounter()` | keep current (high) | not-misnomer |
| `AsmParserContext::errorCallback_` | no | high | matches `setErrorCallback` | keep current (high) | not-misnomer |
| `AsmParserContext::stringCopies_` | no | high | strdup'd char* tracker | keep current (high) | not-misnomer |
| `AsmParserContext::labels_` | no | high | matches `addLabel`/`hasLabel` | keep current (high) | not-misnomer |
| **AsmParserContext (method params)** | | | | | |
| `AsmParserContext::trackedStringCopy::s` | unsure | low | very terse; clear by context | `text`, keep current | — |
| `AsmParserContext::addLabel::label` | no | high | label string | keep current (high) | — |
| `AsmParserContext::hasLabel::label` | no | high | label string | keep current (high) | — |
| `AsmParserContext::setErrorCallback::cb` | no | medium | conventional callback abbrev | keep current (medium) | — |
| `AsmParserContext::raiseError::msg` | no | high | error-message string | keep current (high) | — |
| **Free functions (params)** | | | | | |
| `addNode::ctx` | no | high | parser context | keep current (high) | — |
| `addNode::text` | unsure | low | placeholder line (split on `#`) | `placeholderLine`, keep current | — |
| `addCommand::ctx` | no | high | parser context | keep current (high) | — |
| `addCommand::cmd` | yes | medium | confusingly mixes types | `expr`, `node`, keep current | — |
| `addCommand::args` | yes | medium | actually a cmd token expression | `cmdToken`, keep current | cross-batch-arbitration |
| `addCommand::pc` | no | high | program counter | keep current (high) | — |
| `addCommand::label` | no | high | label C-string | keep current (high) | — |
| `asmerror::ctx` | no | high | parser context | keep current (high) | — |
| `asmerror::result` | no | medium | bison `*result` out-param | keep current (medium) | — |
| `asmerror::scanner` | no | high | flex scanner state | keep current (high) | — |
| `asmerror::msg` | no | high | bison error message | keep current (high) | — |
| `asmparse::ctx` / `result` / `scanner` | no | high | bison-generated signature | keep current (high) | — |
| `asmset_extra::ctx` / `scanner` | no | high | flex helper signature | keep current (high) | — |
| `asmlex_init_extra::ctx` / `scanner` | no | high | flex helper signature | keep current (high) | — |
| **.cpp locals worth noting** | | | | | |
| `addNode::node` (local) | no | high | the AsmExpression being built | keep current (high) | — |
| `addNode::input` / `parts` / `comment` (locals) | no | medium | descriptive of computation | keep current (medium) | — |
| `addCommand::fullName` / `spacePos` / `nameStr` / `resolved` / `labelStr` / `lbl` (locals) | no | medium | descriptive of computation | keep current (medium) | — |

## 3. Detailed findings

### AsmParserContext::isComment_  [no / medium / —]

Evidence:
- `asm_parser_context.hpp:73` declares `bool isComment() const;`
  (in symbol table, authoritative).
- `asm_parser_context.cpp:46-48` — `isComment()` returns `isComment_`.
- `asm_parser_context.cpp:57-62` — `startLineComment()` sets
  `isComment_ = true; isLineComment_ = true;` only if
  `!isBlockComment_`.
- `asm_parser_context.cpp:75-80` — `startBlockComment()` sets
  `isComment_ = true; isBlockComment_ = true;` only if
  `!isLineComment_`.
- `asm_parser_context.cpp:128-135` — `clearSyntaxError()` zeroes the
  first 4 bytes including `isComment_`.

Interpretation:
- `isComment_` is set whenever either kind of comment becomes active
  and cleared when the corresponding kind ends. It is the "any
  comment in progress" aggregate flag, parallel to the more specific
  `isLineComment_` / `isBlockComment_` fields.

Judgement:
- No: the name correctly describes "currently inside any comment".

Proposals:
- keep current (medium)

Locations consulted:
- declared: include/zhinst/asm_parser_context.hpp:118
- used:     src/asm_parser_context.cpp:46–88,131

### AsmParserContext::doOpt_  [no / medium / —]

Evidence:
- Method `doOpt()` returns this field; method name is in the binary
  symbol table at 0x28e820 (authoritative, §3 RULES).
- `disableOpt()` sets it false; `enableOpt()` sets it true
  (asm_parser_context.cpp:96–103).
- Grammar uses: `asm_parser.y:88` calls `ctx->disableOpt()` for
  `inst DISABLE_OPT`; `awg_assembler_impl_pipeline.cpp:137,234` reads
  `!parserCtx_.doOpt()` into `ast->noOpt()`.
- The serialization side emits the literal string `" #disableOpt"`
  (`asm_list.cpp:201`), establishing "opt" = optimization.

Interpretation:
- The field stores a boolean "optimization is enabled for the next
  instruction". The verbal getter form `doOpt` is in the binary, so
  naming the field `doOpt_` to match is consistent with the
  established convention.

Judgement:
- No: name is consistent with the authoritative getter and with the
  externally-visible `#disableOpt` token.

Proposals:
- keep current (medium)
- (alternatively `optimize_` / `optEnabled_` would read more
  naturally as a noun, but would diverge from the binary getter
  name; not flagging.)

Locations consulted:
- declared: include/zhinst/asm_parser_context.hpp:122
- used:     src/asm_parser_context.cpp:96–108;
            src/awg_assembler_impl_pipeline.cpp:137,234;
            src/asm_parser.y:88,94

### AsmParserContext::trackedStringCopy::s  [unsure / low / —]

Evidence:
- `asm_parser_context.hpp:103` and `.cpp:168`: `char* trackedStringCopy(const char* s)`.
- Caller `asm_lexer.l:131,148`: `yylval->sval = CTX->trackedStringCopy(yytext);`
  — `yytext` is the matched flex token text.

Interpretation:
- The argument is the lexer's `yytext` C-string buffer that needs to
  be `strdup`'d. `s` is a generic single-letter name; the role
  ("token text to copy") is clear from one-line context but not from
  the parameter name alone.

Judgement:
- Unsure: the parameter name is generic but its locality (one-line
  body, declaration is the docstring) makes the cost of changing it
  low and the value also low.

Proposals:
- `text` (low)
- `token` (low)
- keep current (low)

Locations consulted:
- declared: include/zhinst/asm_parser_context.hpp:103
- used:     src/asm_parser_context.cpp:168;
            src/asm_lexer.l:131,148

### addNode::text  [unsure / low / —]

Evidence:
- `asm_parser.y:109`: `$$ = zhinst::addNode(ctx, $1)` from rule
  `inst → placeholder` where `placeholder → PLACEHOLDER_LINE`
  (notes/asm_parser_grammar.md:101,145).
- The grammar token is `PLACEHOLDER_LINE` — i.e., a `placeholder
  …#…` line consisting of a placeholder marker followed by a `#`
  and a comment.
- `asm_parser_context.cpp:251-284` splits the input on `'#'` and
  treats `parts[1]` as the trailing comment.

Interpretation:
- The "text" passed in is specifically the matched
  `PLACEHOLDER_LINE` token, not arbitrary text. The current name
  `text` is correct but very generic given the highly specialised
  role.

Judgement:
- Unsure: name fits, but a more specific name would prevent future
  readers from assuming `addNode` accepts arbitrary text.

Proposals:
- `placeholderLine` (low)
- `tokenText` (low)
- keep current (medium)

Locations consulted:
- declared: include/zhinst/asm_parser_context.hpp:138
- defined:  src/asm_parser_context.cpp:251
- used:     src/asm_parser.y:109;
            notes/asm_parser_grammar.md:145

### addCommand::cmd and addCommand::args  [yes / medium / cross-batch-arbitration]

Evidence:
- Signature (`asm_parser_context.hpp:150-154`):
  `addCommand(ctx, AsmExpression* cmd, AsmExpression* args, int pc, const char* label)`.
- Grammar callsites in `asm_parser.y`:
    - rule 6 `inst → cmd`: `addCommand(ctx, NULL, $1, pc, NULL)`
      — the `cmd` grammar non-terminal `$1` is passed as the
      C++ `args` parameter.
    - rule 7 `inst → cmd argList`: `addCommand(ctx, $2, $1, pc, NULL)`
      — `$1` (cmd grammar value) → C++ `args`; `$2` (argList grammar
      value) → C++ `cmd`.
    - rule 8 `inst → lbl cmd`: `addCommand(ctx, NULL, $2, pc, $1)`
      — `$2` (cmd grammar value) → C++ `args`.
    - rule 9 `inst → lbl cmd argList`: `addCommand(ctx, $3, $2, pc, $1)`
      — `$2` (cmd) → C++ `args`; `$3` (argList) → C++ `cmd`.
- Inside the body (`asm_parser_context.cpp:340-362`), the parameter
  named `args` is what carries the **command-name token** — its
  `.name` is parsed for the first space and then resolved via
  `Assembler::commandFromString()`. This is the *command identifier
  expression*, not the argument list.
- Conversely, the parameter named `cmd` is the AsmExpression that
  receives the populated command value; `cmd->command = resolved;`
  (line 361). When NULL, a fresh AsmExpression is allocated for it
  to be populated. This is more accurately the "result/operand
  expression" being built.
- `notes/asm_parser_grammar.md:172-183` already documents the
  inversion explicitly ("the call is `addCommand(ctx, NULL, $2, pc,
  $1)` where `$2` is the cmd grammar value").

Interpretation:
- The two parameter names appear swapped relative to the grammar
  semantics: in the grammar, `cmd` denotes the command-name token
  and `argList` denotes the operand list, but in the C++ signature,
  the parameter named `args` receives the cmd-token expression and
  the parameter named `cmd` receives the operand/argList expression.
- This is an internal inconsistency (the code works correctly), but
  it is genuinely misleading: a reader of the C++ signature alone
  would expect `cmd` to be the command token and `args` to be the
  operands, which is the opposite of the actual behaviour.

Judgement:
- Yes: the parameter names misdescribe what each parameter carries,
  as confirmed by both the grammar callsites and the function body
  itself.

Proposals:
- Swap names: rename current `args` → `cmd`, current `cmd` → `args`
  (high) — matches grammar nomenclature and body semantics.
- Or rename current `args` → `cmdToken` (or `cmdExpr`) and current
  `cmd` → `argList` (or `operands`) (medium) — more explicit, less
  collision with member `AsmExpression::command`.
- keep current (low) — only if the grammar nomenclature itself is
  considered the authority and we should leave the C++ signature
  alone.

Cross-reference:
- `AsmExpression` (batch 24, asm_expression) for member naming
  (`AsmExpression::command`, `AsmExpression::name`). Whichever
  rename direction is chosen here should be consistent with how
  `AsmExpression` is described in batch 24, since the inversion
  bears on which AsmExpression role each name implies.
- Grammar/lexer audit (if any), since the canonical names `cmd`
  / `argList` are also the bison non-terminal names.

Locations consulted:
- declared: include/zhinst/asm_parser_context.hpp:150–154
- defined:  src/asm_parser_context.cpp:321–388
- used:     src/asm_parser.y:114,119,124,129;
            notes/asm_parser_grammar.md:146–183

### AsmParserContext::stringCopies_  [no / high / not-misnomer]

Evidence:
- `asm_parser_context.cpp:168-172` — `trackedStringCopy(s)` does
  `strdup(s); stringCopies_.push_back(copy);`
- `asm_parser_context.cpp:176-181` — `cleanStringCopies()` iterates
  the vector and `free()`s each element.
- Type: `std::vector<char*>` at +0x40.

Interpretation:
- The vector literally holds the `strdup`'d C-string copies that
  the parser needs to free at end of parse. The name precisely
  describes content and purpose.

Judgement:
- No: name matches role and content exactly.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/asm_parser_context.hpp:127
- used:     src/asm_parser_context.cpp:168–181

### AsmParserContext::labels_  [no / high / not-misnomer]

Evidence:
- `asm_parser_context.hpp:128`: `std::unordered_set<std::string>
  labels_;`.
- `addLabel` (cpp:188-190) calls `labels_.emplace(label)`.
- `hasLabel` (cpp:194-196) calls `labels_.find(label)`.

Interpretation:
- A set used solely for label-name dedup in `addCommand`. The name
  is exactly what the field is.

Judgement:
- No: precise.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/asm_parser_context.hpp:128
- used:     src/asm_parser_context.cpp:188–196;
            src/asm_parser_context.cpp (addCommand):368,373

### AsmParserContext::errorCallback_  [no / high / not-misnomer]

Evidence:
- Set by `setErrorCallback(cb)` (method name in binary symbol
  table).
- Invoked in `raiseError(msg)` with `(lineNumber_, msg)`
  (cpp:212-222).
- Type matches: `std::function<void(int, std::string const&)>`.

Interpretation:
- Field stores the syntax-error reporting callback. Name aligns
  with the authoritative setter name.

Judgement:
- No.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/asm_parser_context.hpp:126
- used:     src/asm_parser_context.cpp:204–222;
            src/awg_assembler_impl_pipeline.cpp:123,213,283

### AsmParserContext::lineNumber_ and programCounter_  [no / high / not-misnomer]

Evidence:
- Getters `currentLineNumber()` / `programCounter()` and bumpers
  `incrementLineNumber()` / `incrementProgramCounter()` are all
  in the binary symbol table.
- `clearSyntaxError()` resets `lineNumber_` to 1
  (cpp:128-135).
- Used by `raiseError` (`errorCallback_(lineNumber_, msg);`,
  cpp:214) and by `addCommand` (pc → cmd->value, .cpp:338).

Interpretation:
- Names match the authoritative getter names exactly and reflect
  observed semantics (1-based source line counter; sequencer
  program-counter slot).

Judgement:
- No.

Proposals:
- keep current (high) for both.

Locations consulted:
- declared: include/zhinst/asm_parser_context.hpp:124,125
- used:     src/asm_parser_context.cpp:128–158,214,338

### asmerror::result  [no / medium / —]

Evidence:
- Bison-generated parser signature
  (`asmparse(ctx, AsmExpression** result, scanner)`); `asmerror`
  shares the same signature shape.
- `yy_fwd.hpp:27` declares the same `result` parameter.

Interpretation:
- `result` is the bison `*$$` out-pointer. Matches bison
  convention.

Judgement:
- No.

Proposals:
- keep current (medium)

Locations consulted:
- declared: include/zhinst/asm_parser_context.hpp:163–166;
            include/zhinst/yy_fwd.hpp:23–27

## 4. Symbols inspected and judged routinely fine

- `Label::Label::pc`, `Label::Label::name` — pass-through ctor
  parameters mirroring the field names.
- `Label::operator==::other` — standard `operator==` parameter
  convention.
- `addLabel::label`, `hasLabel::label` — single string parameter
  whose role is the function name; clear.
- `setErrorCallback::cb` — conventional abbreviation for
  callback object.
- `raiseError::msg` — single-string error message.
- All flex/bison-style free-function parameters (`asmerror::scanner`,
  `asmerror::msg`, `asmparse::ctx`/`result`/`scanner`, `asmset_extra::*`,
  `asmlex_init_extra::*`) — match flex/bison library convention,
  in scope per RULES but indistinguishable from third-party API
  shape.
- `addNode::ctx`, `addCommand::ctx`, `addCommand::pc`,
  `addCommand::label` — direct, accurate.
- Local variables in `addNode` (`node`, `input`, `parts`,
  `comment`) and `addCommand` (`fullName`, `spacePos`, `nameStr`,
  `resolved`, `labelStr`, `lbl`) — descriptive of the local
  computation; not "obviously misleading".
- `trackedStringCopy::copy` (local) — clear from one-line body.
- `cleanStringCopies::p` (range-for local for `char*`) — fine for
  one-line scope.

## 5. Coverage

- **Fully covered:** every in-scope symbol declared in
  `include/zhinst/asm_parser_context.hpp` and defined in
  `src/asm_parser_context.cpp` — all data members, all method
  parameters, all relevant locals, free-function parameters
  (`addNode`, `addCommand`, `asmerror`, `asmparse`, `asmset_extra`,
  `asmlex_init_extra`), and the nested `Label` type's members and
  ctor parameters.
- **Deferred:** none. The `addCommand::cmd`/`args` finding raises a
  cross-batch arbitration with batch 24 (asm_expression) and
  potentially with the asm grammar/lexer batches; resolution is
  left to synthesis.
- **Not covered (out of scope per RULES §2/§3):**
  - Type names `AsmParserContext`, `AsmParserContext::Label`,
    `AsmExpression` — present in the binary symbol table.
  - All 22 method names of `AsmParserContext` and the four free
    functions (`addNode`, `addCommand`, `asmerror`, `asmparse`,
    `asmset_extra`, `asmlex_init_extra`) — present in the binary
    symbol table per the §1 listing.
  - Generated bison/flex internals (`yy_*`, `ASMSTYPE`,
    `asmlex`, `asmlex_destroy`, etc.) — third-party generated
    code per RULES §2.
