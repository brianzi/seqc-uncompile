# SeqC Language: Non-Features and Semantically Rejected Constructs {#notes_seqc_language_features_excluded}

\note **Reverse-engineering reference material.** This page is part of
the `reconstructed/notes/` set: deep-dive technical notes for
contributors working on the reconstruction. It cites binary addresses,
opcodes, and disassembly observations directly so they remain
discoverable from the rendered site. The standard documentation-voice
rules for API briefs (no binary citations outside `\binarynote`) do
**not** apply to this page.

This file documents constructs that **look like** they should be valid
SeqC but are **either not in the language at all** or **rejected by a
semantic pass after grammatical acceptance**. It exists so that future
coverage analyses don't keep flagging them as "low-coverage gaps" and
generating wasted stress rounds against them.

**Source of truth**: confirmed against
`reconstructed/src/ast/seqc_lexer.l`, `reconstructed/src/ast/seqc_parser.y`,
and the original binary's behavior (the `coverage_round_2_tier2`
manifest group exercises all the rejection paths below
byte-identically).

## How to use this file

When a coverage analysis surfaces a "language feature with low
test density", **check this file first**. If the feature is listed here,
the low coverage is by design — those tests would only exercise
error-path agreement, not codegen.

When adding a NEW finding, append a row to the appropriate section
with: feature name, exact symptom, where rejected (lexer / parser /
semantic), and the reference test file under
`tests/cases/stress/coverage_round_2/` (or successor) that demonstrates it.

---

## Category A — Not in the language at all (lexer/parser doesn't recognize)

These constructs have **no grammar production**. The lexer doesn't even
tokenize them. Any "low coverage" stat for them is meaningless.

| Construct | Notes |
|---|---|
| **C-style casts** `(int)x`, `(double)x`, `(long)x`, `(short)x` | No cast tokens in `seqc_lexer.l`. No type keywords `int`/`double`/`long`/`short`/`float` exist. SeqC type system is `var`/`cvar`/`const`/`wave`/`string`/`void` only. Implicit conversions are the only mechanism. |
| **Preprocessor** `#define`, `#include`, `#pragma`, `#ifdef`, `#ifndef`, `#if`, `#else`, `#endif` | No `#`-handling code anywhere in `seqc_lexer.l`. SeqC has **no preprocessor**. |
| **C-style typed user functions** `int foo() { … }`, `double bar() { … }`, `float baz() { … }` | No primitive type keywords exist. The valid form is `var foo() { return …; }` — and that form is **already well-covered** (~20 files in `tests/cases/zivibes/` and `hdawg_*func*.seqc`). |

## Category B — Tokenized + parsed, but semantically rejected

These pass the lexer (have a `KW_*` token) and pass the parser (have a
grammar rule), but a downstream pass — semantic analysis or codegen —
rejects them with a specific error message. Both the original binary
and the reconstruction reject them identically.

| Construct | Where rejected | Error message | Notes |
|---|---|---|---|
| `continue;` (in any loop: for/while/do-while/repeat) | semantic pass on `ECommandType::eCONTINUE` | `"continue statement is not supported"` | Tokenized as `KW_CONTINUE` (lexer rule 12, line 84). Parsed by `jump_statement` rule 118 (`seqc_parser.y:635`). |
| `break;` (in any loop) | semantic pass on `ECommandType::eBREAK` | `"break statement is not supported"` | Tokenized as `KW_BREAK` (lexer rule 10, line 82). Parsed by `jump_statement` rule 119 (`seqc_parser.y:637`). |
| `goto LABEL;` | not in grammar | (lexer doesn't have `KW_GOTO`) | Despite the codebase having 153 `goto` sites in **C++** reconstruction (see `goto_policy.md`), `goto` is **not a SeqC language feature**. |
| `var arr[N] = { 1, 2, 3, … };` (brace-list array initializer) | parser | `"syntax error, unexpected '{'"` | Parser has `Rule 77: identifier '[' constant_expression ']'` for sized array declarators, but the binary explicitly disallows the brace-list initializer form. Use per-element assignment instead. |
| `var x /= y;`, `var x %= y;` | semantic pass on `var` operands | `"division is not supported for var, only for const"` | The `/=` and `%=` operators only work in const-evaluated contexts. The other compound assigns (`+=`, `-=`, `*=`, `&=`, `|=`, `^=`, `<<=`, `>>=`) DO work on `var`. |
| `var * var` (integer multiplication of two variables) | semantic pass | `"register multiplication not supported in ISA"` | See `reconstructed/src/ast/seqc_ast_eval_arithmetic.cpp:1553` comment. Multiplication requires at least one const operand. |
| `var / var` (integer division of two variables) | semantic pass | `"integer division not supported in the ISA"` | See `reconstructed/src/ast/seqc_ast_eval_arithmetic.cpp:1747` comment. |
| `string + string`, `string * int` | semantic pass | (no string operators) | Strings are construction/passing only — no arithmetic. |

## Category C — Valid syntax, but assignment-as-expression yields `notype`

These compile, but using an assignment as an expression (e.g. inside a
condition or as an operand) produces `notype` (no usable result type),
which then causes downstream errors. This is **legal C but invalid SeqC
semantics**.

| Construct | Symptom |
|---|---|
| `if ((y = y + 1) < 4)` (assign in condition) | `notype` from the `(y = y+1)` subexpression |
| `(1 \|\| (ev = 1))` (assign as short-circuit operand) | `notype` from the `(ev=1)` subexpression |
| `c *= (k = k + 4)` (assign as compound-assign RHS) | `notype` from the `(k=k+4)` subexpression |
| `prod *= j;` where both are `var` | "multiplication not supported" — falls under Category B |

## Category D — Confirmed-supported features (NOT non-features)

These are first-class supported. Listed here to prevent re-flagging:

- **`var foo() { return …; }`** — typed user functions with return value (use `var`, not `int`/`double`)
- **`do { … } while (…);`** — do-while loops
- **Pre/post `++`/`--`** in all positions
- **Compound assigns** that do NOT involve var-var division/mul: `+=`, `-=`, `*=` (with literal RHS), `&=`, `|=`, `^=`, `<<=`, `>>=`
- **Logical `!`, `&&`, `||`**
- **Ternary `? :`**
- **Bitwise `~`, `<<`, `>>`**
- **Block comments `/* … */`** (in addition to `//`)
- **`var x = 1.5;`** double initializer (and the surrounding double-arithmetic paths)
- **`switch`/`case`/`default`** statements
- **`return` with value** (rule 121) and **`return;`** (rule 120)

---

## Coverage tooling guidance

A simple regex/token-frequency analysis over the `tests/cases/` corpus
will continue to flag the Category A and Category B items as "ZERO" or
"LOW" coverage. **This is not signal**. Future coverage rounds should:

1. Filter out tokens listed here before scoring.
2. Distinguish "feature exists but is rare" from "feature is rejected by
   design". Only the former is worth a stress round.
3. When in doubt, check `seqc_lexer.l` for the `KW_*` token and
   `seqc_parser.y` for a grammar rule. If neither exists, the construct
   is Category A and not worth testing (except as a documentation
   artifact).

---

## Reference test files

Demonstrations live under `tests/cases/stress/coverage_round_2/`. Both
the original binary and the reconstruction agree byte-for-byte on the
rejection of every Category B and Category C construct listed above.
See manifest group `coverage_round_2_tier2` for the full enumeration.

The `_fixed` forks demonstrate the Category C patterns alongside their
corrected counterparts.
