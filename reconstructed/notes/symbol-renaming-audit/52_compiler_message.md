# Batch 52 — compiler_message

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 3 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 2;
> B3 (already resolved during Phase D/R): 1;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/core/compiler_message.hpp`
- `reconstructed/src/core/compiler_message.cpp`

Binary symbol table consulted:
`nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so`.

Authoritatively present in the symbol table (excluded from rename per §3):

- Type names: `zhinst::CompilerMessage` (via vector instantiations),
  `zhinst::CompilerMessage::CompilerMessageType`,
  `zhinst::CompilerMessageCollection`, `zhinst::CompilerException`.
- Method names: `CompilerMessage::str(bool) const`,
  `CompilerMessageCollection::compilerMessage`, `errorMessage`,
  `warningMessage`, `infoMessage`, `parserMessage`, `reset`,
  `setLineNr`, `lineNr() const`, `messages() const`,
  `hadCompilerError() const`,
  `CompilerException::CompilerException`, `~CompilerException`,
  `what() const`.

In-scope per §2: enum members, parameter names, data members, the
`showLine` parameter to `str`.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `CompilerMessage::CompilerMessageType::Error` | no | high | matches `"Compiler Error"` rodata | keep current | not-misnomer |
| `CompilerMessage::CompilerMessageType::Warning` | no | high | matches `"Warning"` rodata | keep current | not-misnomer |
| `CompilerMessage::CompilerMessageType::Info` | no | high | matches `"Info"` rodata | keep current | not-misnomer |
| `CompilerMessage::type` | no | medium | dispatched on as type | keep current | — |
| `CompilerMessage::lineNr` | unsure | low | naked vs `lineNr_` convention | keep current, `line_nr` | — |
| `CompilerMessage::message` | no | medium | holds message text | keep current | — |
| `CompilerMessage::str::showLine` | yes | high | semantics inverted vs name | `hideLine`, `omitLine`, `noLine` | — |
| `CompilerMessageCollection::compilerMessage::type` | no | low | dispatched as type | keep current | — |
| `CompilerMessageCollection::compilerMessage::line` | no | medium | line number, stored as such | keep current | — |
| `CompilerMessageCollection::compilerMessage::msg` | no | medium | message text payload | keep current | — |
| `CompilerMessageCollection::compilerMessage::text` (local) | no | low | post-strip message text | keep current | — |
| `CompilerMessageCollection::errorMessage::msg` | no | medium | message text payload | keep current | — |
| `CompilerMessageCollection::errorMessage::line` | no | medium | line number, fallback to `lineNr_` | keep current | — |
| `CompilerMessageCollection::warningMessage::msg` | no | medium | message text | keep current | — |
| `CompilerMessageCollection::warningMessage::line` | no | medium | line number | keep current | — |
| `CompilerMessageCollection::infoMessage::msg` | no | medium | message text | keep current | — |
| `CompilerMessageCollection::infoMessage::line` | no | medium | line number | keep current | — |
| `CompilerMessageCollection::parserMessage::line` | unsure | low | only call site is unverified | keep current, `code`, `errorCode` | in-scope (parameter name; nm only preserves int type) |
| `CompilerMessageCollection::parserMessage::msg` | no | low | passed as message body | keep current | — |
| `CompilerMessageCollection::messages_` | no | medium | matches `messages()` accessor | keep current | — |
| `CompilerMessageCollection::lineNr_` | no | medium | matches `lineNr()`/`setLineNr` | keep current | — |
| `CompilerMessageCollection::hadError_` | no | medium | matches `hadCompilerError()` | keep current | — |
| `CompilerMessageCollection::setLineNr::nr` | unsure | low | terse abbreviation | keep current, `lineNr` | — |
| `CompilerException::CompilerException::msg` | no | medium | stored as exception text | keep current | — |
| `CompilerException::message_` | no | medium | returned by `what()` | keep current | — |

## 3. Detailed findings

### CompilerMessage::CompilerMessageType::Error / Warning / Info  [no / high / not-misnomer]

Evidence:
- src/core/compiler_message.cpp:21 `static const char* typeNames[] = { "Compiler Error", "Warning", "Info" };`
- The string table lookup is indexed by the enum value, so `Error==0`,
  `Warning==1`, `Info==2` line up with positions 0,1,2 of `typeNames`.

Interpretation:
- Each enumerator's name matches the human-readable label that
  `CompilerMessage::str()` prints for it. The labels are baked into the
  binary's `.rodata`, so per §4d tier 2 they are strong positive
  evidence the names faithfully describe the semantics.

Judgement:
- Not a misnomer.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/core/compiler_message.hpp:20-24
- used:     src/core/compiler_message.cpp:18-38, src/ast/seqc_ast_nodes_evaluate.cpp (via `errorMessage`)

### CompilerMessage::lineNr  [unsure / low / —]

Evidence:
- include/zhinst/core/compiler_message.hpp:27 `int lineNr;` (public POD field, no trailing underscore).
- include/zhinst/core/compiler_message.hpp:53 `int lineNr_;` on `CompilerMessageCollection` (private, with underscore).
- src/core/compiler_message.cpp:57 `if (it->lineNr == line && ...)` — read site.
- src/core/compiler_message.cpp:64 `messages_.push_back(CompilerMessage{type, line, ...});` — write site as 2nd struct field.

Interpretation:
- The semantic content of the name (a line number) matches usage
  exactly. Only inconsistency is stylistic: the rest of the codebase
  uses a `_`-suffix for private members. `CompilerMessage` is a
  public-fielded POD struct, so omitting the underscore is internally
  consistent with `type` and `message` on the same struct.

Judgement:
- Not misleading about meaning; only weak stylistic concern relative to
  sibling class `CompilerMessageCollection::lineNr_`.

Proposals:
- keep current (medium)
- `line_nr` (low) — matches the stylistic spelling used in some
  reconstructed JSON keys; would still be in scope as a public field.

Locations consulted:
- declared: include/zhinst/core/compiler_message.hpp:27
- used:     src/core/compiler_message.cpp:30,57,64

### CompilerMessage::str::showLine  [yes / high / —]

Evidence:
- include/zhinst/core/compiler_message.hpp:31 `std::string str(bool showLine) const;`
- src/core/compiler_message.cpp:30 `if (!showLine && lineNr > 0) { oss << " (line: " << lineNr << ")"; }`
- src/codegen/awg_compiler.cpp:1206 `oss << msg.str(/*showLine=*/false) << "\n";  // @0x1040a1: str(false), @0x1040d4: "\n"`

Interpretation:
- The branch that emits the `" (line: N)"` substring is taken when
  `showLine` is **false**, not true. The only call site in the project
  passes `false` and the disassembled annotation confirms the call is
  `str(false)`. So under the current name, callers who want the line to
  appear must pass `false`, and callers who pass `true` get no line.

Judgement:
- The parameter name asserts the opposite of what the code does; this
  is a misnomer.

Proposals:
- `hideLine` (high) — direct inversion, matches the gating polarity.
- `omitLine` (medium)
- `noLine` (medium)
- keep current (low) — only if a future pass demonstrates the
  reconstruction has the polarity backwards (no evidence so far).

Locations consulted:
- declared: include/zhinst/core/compiler_message.hpp:31
- defined:  src/core/compiler_message.cpp:18-38
- used:     src/codegen/awg_compiler.cpp:1206

### CompilerMessageCollection::parserMessage::line  [unsure / low / verify-not-original]

Evidence:
- include/zhinst/core/compiler_message.hpp:43 `void parserMessage(int line, const std::string& msg);`
- src/core/compiler_message.cpp:87-90 forwards to
  `compilerMessage(CompilerMessage::Error, line, msg)` and explicitly
  does **not** set `hadError_`.
- A site-wide grep finds no external call sites of
  `CompilerMessageCollection::parserMessage` in `reconstructed/src/`;
  only `AWGAssemblerImpl::parserMessage` (a different method on a
  different class, src/codegen/awg_assembler_impl_pipeline.cpp:572) is called.
- That sibling `AWGAssemblerImpl::parserMessage(int level, std::string const& msg)`
  uses parameter name `level` and is used at lines 124/214/284 of
  awg_assembler_impl_pipeline.cpp passing a value named `code`.
- The mangled symbol `CompilerMessageCollection::parserMessage(int, std::string const&)`
  is in the binary, so the parameter ordering is authoritative.

Interpretation:
- The first int is forwarded into the `line` slot of a
  `CompilerMessage`, which strongly suggests it is in fact a line
  number. However, the sibling `parserMessage` on `AWGAssemblerImpl`
  uses the same param-name slot for a `code`/`level`, not a line
  number; the function name is identical and the duplication of name
  raises the question of whether the int here is actually a line at
  all, or an error code that just happens to be plumbed through the
  line slot.
- No call site in the project disambiguates; only the binary itself
  could.

Judgement:
- Cannot be settled from this batch. Name may be correct (matches the
  field it lands in) or may be misleading (if the convention from the
  binary is "code, like the assembler version").

Proposals:
- keep current (medium)
- `code` (low) — only if synthesis/verification confirms the
  caller-side convention.

Cross-reference:
- Sibling method `AWGAssemblerImpl::parserMessage(int level, std::string const&)`
  — declared include/zhinst/codegen/awg_assembler_impl.hpp:153, defined
  src/codegen/awg_assembler_impl_pipeline.cpp:572.

Locations consulted:
- declared: include/zhinst/core/compiler_message.hpp:43
- defined:  src/core/compiler_message.cpp:87-90
- sibling:  include/zhinst/codegen/awg_assembler_impl.hpp:153, src/codegen/awg_assembler_impl_pipeline.cpp:124,214,284,572

### CompilerMessageCollection::setLineNr::nr  [unsure / low / —]

Evidence:
- include/zhinst/core/compiler_message.hpp:47 `void setLineNr(int nr) { lineNr_ = nr; }`
- The setter is declared inline; the only thing the parameter does is
  get assigned to `lineNr_`.

Interpretation:
- `nr` is an abbreviation for "number" and ties to the `Nr` suffix in
  the method/field name, so it is locally consistent. As a standalone
  parameter name, however, `nr` is mildly cryptic.

Judgement:
- Not actively misleading; mild stylistic concern only.

Proposals:
- keep current (medium)
- `lineNr` (low) — would shadow the field of the same base name; only
  worth doing in a rename pass that also addresses the field name.

Locations consulted:
- declared: include/zhinst/core/compiler_message.hpp:47

## 4. Symbols inspected and judged routinely fine

- `CompilerMessage::type` — generic word but the field literally holds
  a `CompilerMessageType` value; idiomatic.
- `CompilerMessage::message` — holds the message text; matches usage.
- `CompilerMessageCollection::compilerMessage::{type,line,msg}` — each
  argument is forwarded into the same-named struct slot.
- `compilerMessage::text` (local in src/core/compiler_message.cpp:50) — a
  local `std::string` that is `msg` after stripping a trailing newline;
  name fits.
- `CompilerMessageCollection::errorMessage::{msg,line}`,
  `warningMessage::{msg,line}`, `infoMessage::{msg,line}`,
  `parserMessage::msg` — all used as message text and line number with
  the documented `-1` sentinel falling back to `lineNr_`.
- `CompilerMessageCollection::messages_` — backs `messages()`;
  underscore convention matches other private members.
- `CompilerMessageCollection::lineNr_` — written by `setLineNr`, read by
  `lineNr()` and the message helpers' default branch.
- `CompilerMessageCollection::hadError_` — set by `errorMessage`,
  cleared by `reset`, queried by `hadCompilerError()`; semantics fit.
- `CompilerException::CompilerException::msg` — stored verbatim as
  `message_` and returned via `what()`.
- `CompilerException::message_` — only data member; returned by `what()`.

## 5. Coverage

- **Fully covered:** every parameter, data member, enum member, and
  local declared in `compiler_message.hpp` / `compiler_message.cpp`.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):** all type names
  (`CompilerMessage`, `CompilerMessageType`, `CompilerMessageCollection`,
  `CompilerException`), all method names (`str`, `compilerMessage`,
  `errorMessage`, `warningMessage`, `infoMessage`, `parserMessage`,
  `reset`, `setLineNr`, `lineNr`, `messages`, `hadCompilerError`,
  `what`, ctor/dtor) — verified present in
  `nm --demangle _seqc_compiler.so`.
