# Batch 18 — seqc_parser_context

## 1. Files considered

- `reconstructed/include/zhinst/seqc_parser_context.hpp`
- `reconstructed/src/seqc_parser_context.cpp`

Cross-referenced (read-only) for usage:
- `reconstructed/src/seqc_lexer.l`
- `reconstructed/src/seqc_parser.y`
- `reconstructed/src/expression.cpp`
- `reconstructed/src/awg_assembler_impl_pipeline.cpp`
  (calls to `parserCtx_.setErrorCallback(...)`, `.hadSyntaxError()`)
- `reconstructed/src/compiler.cpp`,
  `reconstructed/include/zhinst/compiler.hpp`
  (owns `parserContext_`, exposes `hadSyntaxError()`)
- Sibling batch reports: `50_asm_parser_context.md`,
  `07_compiler.md`, `15_cached_parser.md`.

Authoritative symbol-table check.  `nm --demangle _seqc_compiler.so`
shows verbatim:

- `zhinst::SeqcParserContext` (class type) — appears in many
  parameter types (e.g. `seqc_parse(zhinst::SeqcParserContext*, ...)`,
  `zhinst::createValue(zhinst::SeqcParserContext*, double)`).
- All ten methods on the class:
  `currentLineNumber() const`         @0x247c80
  `incrementLineNumber()`             @0x247c90
  `isComment() const`                 @0x247bf0
  `startBlockComment()`               @0x247c40
  `endBlockComment()`                 @0x247c60
  `startLineComment()`                @0x247c00
  `endLineComment()`                  @0x247c20
  `hadSyntaxError() const`            @0x247ca0
  `setSyntaxError()`                  @0x247cb0
  `reset()`                           @0x247cc0
  `setErrorCallback(std::function<void(int, std::string const&)> const&)`
                                       @0x247a60
  `raiseError(std::string const&)`    @0x247ae0

These type/method names are therefore **excluded from rename per
RULES §3** and not flagged below.

The parser/lexer C entry points
(`seqc_parse`, `seqc_error`, `seqc_set_extra`, `seqc_lex_init_extra`)
also appear in the table but live outside this batch's two files.

Notable absence: there is no `clearSyntaxError()` on `SeqcParserContext`
(unlike `AsmParserContext`, which has one in nm at 0x28e890).  Reset
of `hadSyntaxError_` happens only via `reset()`.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| **SeqcParserContext (fields)** | | | | | |
| `SeqcParserContext::isComment_` | no | high | aggregate any-comment flag | keep current (high) | not-misnomer |
| `SeqcParserContext::blockComment_` | unsure | low | name lacks `is`/`in` prefix | `inBlockComment_`, `isBlockComment_`, keep current | coordinated-rename |
| `SeqcParserContext::lineComment_` | unsure | low | name lacks `is`/`in` prefix | `inLineComment_`, `isLineComment_`, keep current | coordinated-rename |
| `SeqcParserContext::hadSyntaxError_` | no | high | matches `hadSyntaxError()` getter | keep current (high) | not-misnomer |
| `SeqcParserContext::currentLineNumber_` | no | high | matches `currentLineNumber()` getter | keep current (high) | not-misnomer |
| `SeqcParserContext::pad_` | no | medium | reconstruction alignment filler | keep current (medium) | — |
| `SeqcParserContext::errorCallback_` | no | high | matches `setErrorCallback` setter | keep current (high) | not-misnomer |
| **SeqcParserContext (method params)** | | | | | |
| `SeqcParserContext::raiseError::msg` | no | high | error-message string | keep current (high) | — |
| `SeqcParserContext::setErrorCallback::cb` | no | medium | conventional callback abbrev | keep current (medium) | — |

No locals in this .cpp warrant flagging (every method body is
1–5 lines of trivial assignment / branch).

## 3. Detailed findings

### SeqcParserContext::isComment_  [no / high / not-misnomer]

Evidence:
- `seqc_parser_context.hpp:49` declares `uint8_t isComment_{0};`.
- `seqc_parser_context.cpp:37-40` — `isComment()` returns
  `isComment_ != 0`.  Method name is in the binary symbol table at
  0x247bf0 (authoritative).
- `seqc_parser_context.cpp:46-50` — `startBlockComment()` sets
  `blockComment_ = 1; isComment_ = 1;`.
- `seqc_parser_context.cpp:56-60` — `endBlockComment()` sets
  `blockComment_ = 0; isComment_ = 0;`.
- `seqc_parser_context.cpp:66-70` — `startLineComment()` sets
  `lineComment_ = 1; isComment_ = 1;`.
- `seqc_parser_context.cpp:76-80` — `endLineComment()` sets
  `lineComment_ = 0; isComment_ = 0;`.
- All call sites (`seqc_lexer.l:55,62,71,76–198,…`) read it as
  "are we currently inside any comment?" to decide whether to skip
  a token.

Interpretation:
- The byte at +0x00 is the aggregate "any comment in progress" flag,
  set whenever either kind of comment opens and cleared when either
  kind closes.  Naming it `isComment_` matches the authoritative
  getter `isComment()` in the binary.

Judgement:
- No: the name is a verbatim mirror of the binary getter and
  describes the role precisely.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/seqc_parser_context.hpp:49
- used:     src/seqc_parser_context.cpp:37–80;
            src/seqc_lexer.l:55,62,71,76–256

### SeqcParserContext::blockComment_ and SeqcParserContext::lineComment_  [unsure / low / coordinated-rename]

Evidence:
- `seqc_parser_context.hpp:50-51`:
  `uint8_t blockComment_{0}; uint8_t lineComment_{0};`.
- Their only writers are `startBlockComment`/`endBlockComment` and
  `startLineComment`/`endLineComment` (cpp:46–80) — boolean flags
  set to 1/0.
- `reset()` (cpp:106-113) zeros both.
- They are never read except via `reset()` and the start/end
  methods themselves; no getter on the class exposes them.
- The sibling class `AsmParserContext` names the equivalent fields
  `isLineComment_` and `isBlockComment_` (with the `is` prefix) and
  exposes an `isLineComment()` getter (batch 50, §3 finding on
  `AsmParserContext::isLineComment_`).

Interpretation:
- These are pure boolean state flags.  C++ conventions usually
  spell such flags with an `is`/`in` prefix when the field is a
  noun-like identifier (`block`/`line`); `blockComment_` reads
  like a noun (the comment object itself) rather than a boolean
  ("currently in a block comment").  The aggregate `isComment_`
  in the same struct does carry the prefix, making this internally
  inconsistent.
- Whether the original binary used the prefix here is not
  knowable from `nm` (non-static data members are not encoded).

Judgement:
- Unsure: the names are functional but inconsistent with the
  sibling field `isComment_` in the same struct and with the
  matching fields in `AsmParserContext`.  The role is unambiguous
  from context.

Proposals:
- `inBlockComment_` / `inLineComment_` (low) — symmetric, reads as
  boolean state.
- `isBlockComment_` / `isLineComment_` (low) — matches
  `AsmParserContext` and the `isComment_` sibling field.
- keep current (medium) — non-static fields are unverifiable
  against the binary; current names are not actively misleading.

Cross-reference:
- `AsmParserContext::isLineComment_` / `isBlockComment_` in batch
  50 (already judged `not-misnomer`).  If consistency across the
  two parser contexts is desired, this batch's two fields should
  follow that convention.

Locations consulted:
- declared: include/zhinst/seqc_parser_context.hpp:50–51
- used:     src/seqc_parser_context.cpp:46–80,106–113

### SeqcParserContext::hadSyntaxError_  [no / high / not-misnomer]

Evidence:
- `seqc_parser_context.hpp:52`: `uint8_t hadSyntaxError_{0};`.
- `hadSyntaxError() const` (cpp:86-89) returns
  `hadSyntaxError_ != 0`.  Method name is in the binary symbol
  table at 0x247ca0 (authoritative).
- `setSyntaxError()` (cpp:95-98) sets the field to 1; method name
  is in the binary symbol table at 0x247cb0 (authoritative).
- `reset()` (cpp:106-113) zeros it.
- Read by `Compiler::hadSyntaxError()` (compiler.cpp:605-608) which
  delegates to `parserContext_.hadSyntaxError()`; in turn called by
  `awg_assembler_impl_pipeline.cpp:264,488,588` and
  `awg_compiler.cpp:745,995`.

Interpretation:
- The field name is a verbatim mirror of the authoritative getter
  in the binary.  Semantics — "set once, never cleared except by
  full `reset()`" — match the verb form `had`.

Judgement:
- No: name precisely matches getter and observed write-once
  semantics.

Proposals:
- keep current (high)

Cross-reference:
- `Compiler::hadSyntaxError()` (batch 07) — confirmed mirror of this
  field's getter; not a misnomer there either.
- `AsmParserContext::hadSyntaxError_` (batch 50) — same name,
  also `not-misnomer`.

Locations consulted:
- declared: include/zhinst/seqc_parser_context.hpp:52
- used:     src/seqc_parser_context.cpp:86–98,106–113;
            src/compiler.cpp:605–608;
            src/awg_assembler_impl_pipeline.cpp:264,488,588

### SeqcParserContext::currentLineNumber_  [no / high / not-misnomer]

Evidence:
- `seqc_parser_context.hpp:54`: `int32_t currentLineNumber_{1};`.
- Returned verbatim by `currentLineNumber() const` (cpp:19-22),
  whose name is in the binary symbol table at 0x247c80
  (authoritative).
- Bumped by `incrementLineNumber()` (cpp:28-31), in symbol table
  at 0x247c90.
- `reset()` resets it to **1** (cpp:112), establishing 1-based
  semantics.
- Used in `raiseError` (cpp:137) as the first argument to the
  error callback — the line number passed to the user's reporter.
- Lexer increments via `CTX->incrementLineNumber()`
  (`seqc_lexer.l:245`).

Interpretation:
- Field is a 1-based current source-line counter for the parser,
  matching the authoritative getter name verbatim.

Judgement:
- No.

Proposals:
- keep current (high)

Cross-reference:
- `AsmParserContext::lineNumber_` (batch 50) is the sibling field
  but is named without the `current` prefix (its getter is also
  `currentLineNumber()`).  Not flagged here — both names are
  defensible — but worth noting if a project-wide consistency pass
  is later attempted.

Locations consulted:
- declared: include/zhinst/seqc_parser_context.hpp:54
- used:     src/seqc_parser_context.cpp:19–31,106–113,137;
            src/seqc_lexer.l:245

### SeqcParserContext::pad_  [no / medium / —]

Evidence:
- `seqc_parser_context.hpp:55-56`:
  `// +0x08 — padding to align std::function at +0x10`
  `char pad_[8]{};`
- Header-comment block at the top of the file documents the layout
  with "+0x08 [8 bytes padding]".
- The byte range is never read or written by any reconstructed
  method; it exists solely so the subsequent `errorCallback_`
  member lands at the binary-observed offset +0x10.

Interpretation:
- A reconstruction-only filler array whose role is alignment.  The
  name `pad_` accurately advertises this; it is the same convention
  used elsewhere in the codebase (e.g. `AwgAssemblerImpl::pad_*`,
  `Value::pad_04_`).

Judgement:
- No: name matches purpose and project convention for layout
  filler.

Proposals:
- keep current (medium)

Locations consulted:
- declared: include/zhinst/seqc_parser_context.hpp:55–56

### SeqcParserContext::errorCallback_  [no / high / not-misnomer]

Evidence:
- `seqc_parser_context.hpp:58`:
  `std::function<void(int, const std::string&)> errorCallback_;`
- Mutated only by `setErrorCallback(...)` (cpp:120-124); setter
  name is in the binary symbol table at 0x247a60 (authoritative).
- Invoked in `raiseError(msg)` (cpp:136-138) with
  `(currentLineNumber_, msg)` — i.e., as a syntax-error reporter.
- Set externally at three sites in
  `awg_assembler_impl_pipeline.cpp:123,213,283`, each of which
  passes a lambda whose body forwards to the surrounding error
  reporter.

Interpretation:
- Field stores the syntax-error reporting callback, exactly as the
  authoritative setter name implies.

Judgement:
- No.

Proposals:
- keep current (high)

Cross-reference:
- `AsmParserContext::errorCallback_` (batch 50) — same name and
  role; also `not-misnomer`.

Locations consulted:
- declared: include/zhinst/seqc_parser_context.hpp:58
- used:     src/seqc_parser_context.cpp:120–145;
            src/awg_assembler_impl_pipeline.cpp:123,213,283

### SeqcParserContext::setErrorCallback::cb  [no / medium / —]

Evidence:
- `seqc_parser_context.hpp:44-45` and `.cpp:120-124`:
  `void setErrorCallback(std::function<void(int, const std::string&)> cb);`
- One-line body: `errorCallback_ = std::move(cb);`.
- The mangled `nm` signature is
  `setErrorCallback(std::function<...> const&)` — the parameter is
  passed *by const reference* in the binary, not by value.  This is
  a type observation (per §2a), not a name issue; recorded here for
  the record so the type discrepancy is not lost.

Interpretation:
- `cb` is the conventional one-letter abbreviation for "callback";
  body locality is one line, role is unambiguous from the function
  name `setErrorCallback`.

Judgement:
- No.

Proposals:
- keep current (medium)
- alternatives `callback`, `errorCallback` would be slightly more
  descriptive but redundant with the function name; not flagging.

Locations consulted:
- declared: include/zhinst/seqc_parser_context.hpp:44–45
- defined:  src/seqc_parser_context.cpp:120–124

### SeqcParserContext::raiseError::msg  [no / high / —]

Evidence:
- `seqc_parser_context.hpp:32` and `.cpp:134`:
  `void raiseError(const std::string& msg);`
- Body forwards to the callback as
  `errorCallback_(currentLineNumber_, msg)` (cpp:137) and to
  `std::clog` as `<< "[Line N]: " << msg` (cpp:144).
- Callers at `seqc_parser.y:687`, `seqc_lexer.l:65,256`,
  `expression.cpp:589` all pass a human-readable diagnostic
  string.

Interpretation:
- `msg` is the conventional name for an error-message string and
  matches every observed call site.

Judgement:
- No.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/seqc_parser_context.hpp:32
- defined:  src/seqc_parser_context.cpp:134–145
- used:     src/seqc_parser.y:687;
            src/seqc_lexer.l:65,256;
            src/expression.cpp:589

## 4. Symbols inspected and judged routinely fine

- All locals in `.cpp` — every method body is 1–5 lines of either
  trivial assignment, a comparison, or a single forwarding call.
  No local variables introduced, hence none to flag.
- The class type name `SeqcParserContext` itself — present in the
  binary symbol table (out of scope per RULES §3).
- All ten method names — present in the binary symbol table
  (out of scope per RULES §3).

## 5. Coverage

- **Fully covered:** every in-scope symbol declared in
  `include/zhinst/seqc_parser_context.hpp` and defined in
  `src/seqc_parser_context.cpp` — all seven data members
  (`isComment_`, `blockComment_`, `lineComment_`,
  `hadSyntaxError_`, `currentLineNumber_`, `pad_`,
  `errorCallback_`) and both method parameters
  (`raiseError::msg`, `setErrorCallback::cb`).  No locals exist
  in either file that warrant inspection.
- **Deferred:** none.  The `blockComment_` / `lineComment_`
  finding is flagged `coordinated-rename` purely because the
  two fields should be renamed together (or not at all) for
  symmetry; resolution is left to synthesis with optional
  cross-reference to `AsmParserContext` (batch 50) for project
  consistency.
- **Not covered (out of scope per RULES §2/§3):**
  - Type name `SeqcParserContext` — present in the binary
    symbol table.
  - All ten method names of `SeqcParserContext` — present in the
    binary symbol table per the §1 listing.
  - The flex/bison free entry points (`seqc_parse`, `seqc_error`,
    `seqc_set_extra`, `seqc_lex_init_extra`) live in generated
    `.l`/`.y`/`.tab.c` sources, not in this batch's two files,
    and have already been seen by the generated-code exemption
    (RULES §2) elsewhere.
