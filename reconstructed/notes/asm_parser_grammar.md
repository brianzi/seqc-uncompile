# ASM Parser Grammar {#notes_asm_parser_grammar}

## Overview

The compiler contains two flex/bison parser pairs: a **SeqC
parser** (full C-like language, ~65 rules, ~111 tokens; see
\ref notes_seqc_parser_grammar) and an **ASM parser** (tiny
assembly language, 19 rules, 8 tokens).  This page covers the
ASM parser — the one consumed by `Compiler::stepParseAsm` for
hand-written `.seqasm` instruction sources.

Both parsers are **reentrant** (flex `%option reentrant`, bison
`%define api.pure`).  The ASM parser uses bison 3.x.

## Token Set

### Terminals

| Index | Name             | Bison Token # | Semantic Type    |
|-------|------------------|---------------|------------------|
| 0     | "end of file"    | 0             | —                |
| 1     | error            | 256           | —                |
| 2     | "invalid token"  | —             | —                |
| 3     | REGISTER         | 258           | int (reg number) |
| 4     | STRING           | 259           | char* (strdup'd) |
| 5     | STRING_LITERAL   | 260           | char* (strdup'd) |
| 6     | NUMBER           | 261           | int              |
| 7     | PLACEHOLDER_LINE | 262           | —                |
| 8     | DISABLE_OPT      | 263           | —                |
| 9     | `':'`            | 58            | —                |
| 10    | `','`            | 44            | —                |

### Nonterminals

| Index | Name        |
|-------|-------------|
| 11    | $accept     |
| 12    | line        |
| 13    | inst        |
| 14    | placeholder |
| 15    | lbl         |
| 16    | cmd         |
| 17    | argList     |
| 18    | arg         |

## Grammar

```
Rule  1: $accept → line $end
Rule  2: line → ε                         /* empty line */
Rule  3: line → inst DISABLE_OPT
Rule  4: line → inst
Rule  5: inst → placeholder
Rule  6: inst → cmd                       /* bare command, no args */
Rule  7: inst → cmd argList               /* command with arguments */
Rule  8: inst → lbl cmd                   /* label + bare command */
Rule  9: inst → lbl cmd argList           /* label + command + args */
Rule 10: inst → lbl                       /* bare label definition */
Rule 11: placeholder → PLACEHOLDER_LINE
Rule 12: lbl → STRING ':'
Rule 13: cmd → STRING
Rule 14: argList → arg
Rule 15: argList → argList ',' arg
Rule 16: argList → STRING_LITERAL         /* string literal as sole arg */
Rule 17: arg → REGISTER
Rule 18: arg → NUMBER
Rule 19: arg → STRING
```

## Semantic Actions

The rule-action dispatch is a jump table over rules 2-19.

| Rule | Grammar                     | Action                                                |
|------|-----------------------------|-------------------------------------------------------|
| 2    | line → ε                    | `*result = NULL`                                      |
| 3    | line → inst DISABLE_OPT     | `*result = $1; ctx->disableOpt(); ctx->incrementLineNumber()` |
| 4    | line → inst                 | `*result = $1; ctx->enableOpt(); ctx->incrementLineNumber()` |
| 5    | inst → placeholder          | `addNode(ctx, $1)`                                    |
| 6    | inst → cmd                  | `pc = ctx->programCounter(); addCommand(ctx, NULL, $1, pc, NULL)` |
| 7    | inst → cmd argList          | `pc = ctx->programCounter(); addCommand(ctx, $2, $1, pc, NULL)` |
| 8    | inst → lbl cmd              | `pc = ctx->programCounter(); addCommand(ctx, NULL, $2, pc, $1)` |
| 9    | inst → lbl cmd argList      | `pc = ctx->programCounter(); addCommand(ctx, $3, $2, pc, $1)` |
| 10   | inst → lbl                  | `pc = ctx->programCounter(); ctx->incrementProgramCounter()` |
| 11   | placeholder → PLACEHOLDER   | `$$ = $1; ctx->programCounter()`                      |
| 12   | lbl → STRING ':'            | `$$ = $1; ctx->programCounter()`                      |
| 13   | cmd → STRING                | `$$ = createName($1)`                                 |
| 14   | argList → arg               | `$$ = createArgList($1)`                              |
| 15   | argList → argList ',' arg   | `$$ = appendArgList($1, $3)`                          |
| 16   | argList → STRING_LITERAL    | `$$ = createName($1)`                                 |
| 17   | arg → REGISTER              | `$$ = createRegister($1)`                             |
| 18   | arg → NUMBER                | `$$ = createValue($1)`                                |
| 19   | arg → STRING                | `$$ = createName($1)`                                 |

### Semantic Type (`%union` / `ASMSTYPE`)

```c
%union {
    int                ival;   /* REGISTER (reg number), NUMBER */
    char*              sval;   /* STRING, STRING_LITERAL, lbl, placeholder */
    AsmExpression*     expr;   /* cmd, argList, arg, inst */
}
```

### `addCommand` parameter mapping

`addCommand(ctx, cmd, args, pc, label)`:

- `cmd` (`AsmExpression*`): output node — gets `type=0`, `value=pc`,
  command enum.
- `args` (`AsmExpression*`): provides instruction name via
  `args->name`.
- `label` (`const char*`): optional label string.

In rule 8 (`inst → lbl cmd`), the call is
`addCommand(ctx, NULL, $2, pc, $1)`:

- `args = $2` = cmd nonterminal (`AsmExpression*` with command name).
- `label = $1` = lbl (label name).

In rule 9 (`inst → lbl cmd argList`), the call is
`addCommand(ctx, $3, $2, pc, $1)`:

- `cmd = $3` = argList (`AsmExpression*` with children, becomes
  output node).
- `args = $2` = cmd nonterminal (`AsmExpression*` with command name).
- `label = $1` = lbl (label name).

## Lexer Token Patterns

Lexer rules and the tokens they emit:

| Flex Rule | Pattern                    | Token Returned | Value |
|-----------|----------------------------|----------------|-------|
| 1         | `"/*"`                     | (start block comment) | — |
| 2         | `"*/"`                     | (end block comment) | — |
| 3         | `"//"`                     | (start line comment) | — |
| 4         | `\n` (outside line comment)| DISABLE_OPT (263) or newline handling | — |
| 5         | `[0-9]+`                   | NUMBER (261) | sscanf("%d") |
| 6         | `","`                      | ',' (44)     | — |
| 7         | `":"`                      | ':' (58)     | — |
| 8         | `[Rr][0-9]+`              | REGISTER (258) | toupper+sscanf("R%d") |
| 9,10      | `[a-zA-Z_][a-zA-Z0-9_]*`  | STRING (259) | trackedStringCopy |
| 11        | `\n` (in line comment)     | (end line comment) | — |
| 12        | `[ \t]+`                   | (whitespace, skip) | — |
| 13        | `.`                        | (unexpected char error) | — |
| 14        | `\"[^\"]*\"`               | STRING_LITERAL (262) | strdup(yytext) |
| 15        | `0[xX][0-9a-fA-F]+`       | NUMBER (261) | sscanf("0x%x") |
| 16        | (fatal error)              | — | — |
| 17        | `<<EOF>>`                  | (buffer switch) | — |
| 18        | (end of input)             | 0 (EOF)      | — |

### Comment handling

All token-returning rules check `ctx->isComment()` first.  If
in a comment, the token is discarded and the lexer loops.
Comment state is tracked via:

- `startLineComment()` / `endLineComment()` — `//` to `\n`.
- `startBlockComment()` / `endBlockComment()` — `/*` to `*/`.
- Encountering `*/` outside a comment raises `"unexpected token '*/'"`.

## Build notes

- Generated `.c` files compiled as C++ (`LANGUAGE CXX` property
  in the build system), since semantic actions reference C++
  types (`AsmExpression*`, `std::string`).
- The `%code requires` block in the `.y` source exports forward
  declarations into `.tab.h`.
- `#define YYSTYPE ASMSTYPE` in the `.l` source bridges flex's
  bison-bridge to the prefixed union name.
