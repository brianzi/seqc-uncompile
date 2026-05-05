# ASM Parser Grammar Reconstruction

## Overview

The binary contains two flex/bison parser pairs: a **SeqC parser** (full
C-like language, ~65 rules, ~111 tokens) and an **ASM parser** (tiny
assembly language, 19 rules, 8 tokens). This document covers the ASM
parser reconstruction.

Both parsers are **reentrant** (flex `%option reentrant`, bison
`%define api.pure`). The ASM parser uses bison 3.x (evidenced by
`yypcontext_t` in its `yysyntax_error` signature).

## Binary Addresses

| Symbol             | Address    | Size     |
|--------------------|------------|----------|
| `asmparse`         | 0x292b50   | 1,522 B  |
| `asmlex`           | 0x290f90   | 4,018 B  |
| `asmerror`         | 0x292a60   | 240 B    |
| `asmlex_init_extra`| 0x292960   | 0x55     |
| `asmlex_destroy`   | 0x2929c0   | 0x79     |

### Bison Tables (rodata, static/local linkage)

| Table        | Address    | Size    | Element Type |
|--------------|------------|---------|--------------|
| yytranslate  | 0x95db30   | 264 B   | uint8_t      |
| yypact       | 0x95db10   | 22 B    | int8_t       |
| yytable      | 0x95dc60   | 18 B    | int8_t       |
| yycheck      | 0x95dc40   | 18 B    | int8_t       |
| yydefact     | 0x95dc80   | 22 B    | uint8_t      |
| yypgoto      | 0x95dcd4   | 8 B     | int8_t       |
| yydefgoto    | 0x95dcdc   | 8 B     | int8_t       |
| yyr1         | 0x95dcc0   | 20 B    | uint8_t      |
| yyr2         | 0x95dca0   | 20 B    | uint8_t      |
| yytname      | 0xb07100   | 160 B   | char* ptrs   |

### Flex DFA Tables (rodata, static/local linkage)

| Table      | Address    | Size    | Element Type |
|------------|------------|---------|--------------|
| yy_ec      | 0x95d620   | 256 B   | uint8_t      |
| yy_accept  | 0x95d720   | 112 B   | int16_t      |
| yy_chk     | 0x95d790   | 240 B   | int16_t      |
| yy_base    | 0x95d880   | 128 B   | int16_t      |
| yy_def     | 0x95d900   | 128 B   | int16_t      |
| yy_meta    | 0x95d980   | 33 B    | uint8_t      |
| yy_nxt     | 0x95d9b0   | 240 B   | int16_t      |

Lexer: 53 DFA states, 33 equivalence classes, 19 lexer rules.

## Token Set (from yytname)

### Terminals (internal indices 0-10)

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

### Nonterminals (internal indices 11-18)

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

## Reconstructed Grammar

Derived from yyr1/yyr2 arrays, yydefact reductions per state, and
parse-table tracing (yypact/yytable/yycheck/yydefgoto/yypgoto).

```
Rule  0: (padding)
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

### Verification via Table Data

```
yyr1 = [0, 11, 12, 12, 12, 13, 13, 13, 13, 13, 13, 14, 15, 16, 17, 17, 17, 18, 18, 18]
yyr2 = [0,  2,  0,  2,  1,  1,  1,  2,  2,  3,  1,  1,  2,  1,  1,  3,  1,  1,  1,  1]
```

Parse-state traces confirming each rule:
- State 0: shifts STRING→1, PLACEHOLDER_LINE→2; default reduces rule 2
- State 1: on ':' shifts→8 (label); default reduces rule 13 (cmd→STRING)
- State 2: reduces rule 11 (placeholder→PLACEHOLDER_LINE)
- State 4: on DISABLE_OPT shifts→10; default reduces rule 4 (line→inst)
- State 5: reduces rule 5 (inst→placeholder)
- State 6: on STRING shifts→11 (start cmd after lbl); default reduces rule 10 (inst→lbl)
- State 7: on REGISTER/STRING/STRING_LITERAL/NUMBER shifts; default reduces rule 6 (inst→cmd)
- State 8: reduces rule 12 (lbl→STRING ':')
- State 10: reduces rule 3 (line→inst DISABLE_OPT)
- State 12: on args shifts; default reduces rule 8 (inst→lbl cmd)
- State 17: on ',' shifts→20; default reduces rule 7 (inst→cmd argList)
- State 19: on ',' shifts→20; default reduces rule 9 (inst→lbl cmd argList)
- State 20: shifts REGISTER→13, STRING→14, NUMBER→16
- State 21: reduces rule 15 (argList→argList ',' arg)

## Semantic Actions (from asmparse disassembly)

The rule-action dispatch is a jump table at 0x95daa0 (18 entries,
rules 2-19).

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

### Semantic Type (%union / ASMSTYPE)

Based on the token return values and nonterminal usage:
```c
%union {
    int                ival;   /* REGISTER (reg number), NUMBER */
    char*              sval;   /* STRING, STRING_LITERAL, lbl, placeholder */
    AsmExpression*     expr;   /* cmd, argList, arg, inst */
}
```

### addCommand Parameter Mapping

The `addCommand(ctx, cmd, args, pc, label)` function:
- `cmd` (AsmExpression*): output node — gets type=0, value=pc, command enum
- `args` (AsmExpression*): provides instruction name via args->name
- `label` (const char*): optional label string

In rule 8 (`inst → lbl cmd`), the call is `addCommand(ctx, NULL, $2, pc, $1)`:
- `args = $2` = cmd nonterminal (AsmExpression* with command name)
- `label = $1` = lbl (char* label name)

In rule 9 (`inst → lbl cmd argList`), the call is `addCommand(ctx, $3, $2, pc, $1)`:
- `cmd = $3` = argList (AsmExpression* with children, becomes output node)
- `args = $2` = cmd nonterminal (AsmExpression* with command name)
- `label = $1` = lbl (char* label name)

## Lexer Token Patterns

### Token Recognition (from DFA analysis and lexer disassembly)

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

### Equivalence Classes (notable)

| EC | Characters                |
|----|---------------------------|
| 3  | `\n`                      |
| 4  | space                     |
| 5  | `"`                       |
| 6  | `#`                       |
| 7  | `*`                       |
| 8  | `+`, `-`                  |
| 9  | `,`                       |
| 10 | `/`                       |
| 11 | `0`                       |
| 12 | `1`-`9`                   |
| 13 | `:`                       |
| 14 | `A`-`F` (hex letters)     |
| 15 | `G`-`Z` except `O`,`R`,`S`|
| 16 | `O`                       |
| 17 | `R` (for register prefix) |
| 18 | `\`                       |
| 19-32 | Individual lowercase letters: a,b,c,d,e,f,h,i,l,o,p,r,s,t,x |

The fine-grained splitting of lowercase letters suggests the DFA
partially recognizes keywords (e.g. `disable_opt`, `placeholder`)
directly in the lexer.

### Comment Handling

All token-returning rules check `ctx->isComment()` first. If in a
comment, the token is discarded and the lexer loops. Comment state
is tracked via:
- `startLineComment()` / `endLineComment()` — `//` to `\n`
- `startBlockComment()` / `endBlockComment()` — `/*` to `*/`
- Encountering `*/` outside a comment raises "unexpected token '*/'"

## Reconstruction Plan

### Status: COMPLETE

All bison parse tables generated from the reconstructed `asm_parser.y`
match the binary's tables byte-for-byte:

| Table        | Match |
|--------------|-------|
| yyr1         | EXACT |
| yyr2         | EXACT |
| yytname      | EXACT |
| yytranslate  | EXACT |
| yypact       | EXACT |
| yytable      | EXACT |
| yycheck      | EXACT |
| yydefact     | EXACT |
| yypgoto      | EXACT |
| yydefgoto    | EXACT |

### Files Created

- `src/asm/asm_parser.y` — bison grammar (19 rules, semantic actions)
- `src/asm/asm_lexer.l` — flex lexer (token patterns, comment handling)
- CMakeLists.txt updated with flex/bison build rules

### Key Build Notes

- Generated .c files compiled as C++ (`LANGUAGE CXX` property) since
  semantic actions reference C++ types (AsmExpression*, std::string)
- `%code requires` block in .y exports forward declarations into .tab.h
- `#define YYSTYPE ASMSTYPE` in .l bridges flex's bison-bridge to the
  prefixed union name

## Comparison: SeqC Parser (Future Work)

The SeqC parser is much larger but follows the same architecture:
- ~111 tokens including C operators and DSP keywords (KW_WAVE, KW_CVAR, etc.)
- ~43 nonterminals following classic ANSI C yacc grammar structure
- Standard expression-precedence grammar (multiplicative, additive, etc.)
- Uses `SeqcParserContext` instead of `AsmParserContext`
- Entry: `seqc_parse` at 0x2ca2a0 (5,777 B)
- Reconstruction would follow the same methodology but require
  significantly more effort
