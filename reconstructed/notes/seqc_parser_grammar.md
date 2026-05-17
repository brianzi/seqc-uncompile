# SeqC Parser Grammar {#notes_seqc_parser_grammar}

The SeqC parser is a **bison + flex** reentrant pair for a C-like
domain-specific language: ANSI-C-style expression precedence with
domain keywords (`wave`, `cvar`, `string`, `repeat`).  This page
documents the token set, the lexer rules, the semantic-value
type, and the AST-building actions invoked at each grammar
reduction.

The parser produces an AST that is consumed by the front-end
lowering pass; see \ref notes_frontend_lowering.

## Lexer

The lexer is a flex DFA with **80 rules** and a 65-entry character
equivalence-class table.

### Rule groups

**Comment handling (rules 1-3):**

- Rule 1: `/*` → start block comment
- Rule 2: `*/` → end block comment
- Rule 3: `//` → start line comment

**Keywords (rules 4-21):** `const`, `cvar`, `string`, `var`,
`void`, `wave`, `break`, `case`, `continue`, `default`, `do`,
`else`, `for`, `if`, `repeat`, `return`, `switch`, `while`.

**Identifier (rule 22):**
```
[a-zA-Z_]([a-zA-Z0-9_]|{U2}{UCONT}|{U3}{UCONT}{UCONT}|{U4}{UCONT}{UCONT}{UCONT})*
```
returning `IDENTIFIER` with the matched text stored in
`yylval->sval`.

**Numeric constants (rules 23-29)** — all return `CONSTANT` with
the value stored in `yylval->dval` as a `double`:

| Rule | Pattern                                            | Notes                              |
|------|----------------------------------------------------|------------------------------------|
| 23   | `0[xX][0-9a-fA-F]+`                                | Hex (int → double)                 |
| 24   | `0[bB][01]+`                                       | Binary (int → double)              |
| 25   | `0[0-9]+`                                          | Leading-zero integer (decimal)     |
| 26   | `[0-9]+`                                           | Decimal integer                    |
| 27   | `[0-9]+[eE][+-]?[0-9]+`                            | Int with exponent (float)          |
| 28   | `[0-9]*\.[0-9]+([eE][+-]?[0-9]+)?`                 | Float, dotted                      |
| 29   | `[0-9]+\.([eE][+-]?[0-9]+)?`                       | Float, trailing dot                |

**String literals (rules 30-31)** — both return `STRING_LITERAL`,
both accept an optional `L` prefix:

- Rule 30: `L?'([^'\\\n]|\\.)+'` — single-quoted, must be non-empty.
- Rule 31: `L?"([^"\\\n]|\\.)*"` — double-quoted, `""` is valid.

**Operators (rules 32-51):** `>>=`, `<<=`, `+=`, `-=`, `*=`, `/=`,
`%=`, `&=`, `^=`, `|=`, `>>`, `<<`, `++`, `--`, `&&`, `||`, `<=`,
`>=`, `==`, `!=`.

**Single-character tokens (rules 52-75):** `; { } , : = ( ) [ ] .
& ! ~ - + * / % < > ^ | ?`.

**Whitespace / newline (rules 76-78):**

- Rule 76: `[ \t\v\f]+` → skip.
- Rule 77: `\n` → increment line counter.
- Rule 78: `\r` → end line comment.

**Fallback (rule 79):** `.` → "Unexpected character" error.

**EOF (rule 81):** `<<EOF>>` → return 0.

### UTF-8 in identifiers

The lexer accepts **valid UTF-8 in identifier continuations** —
the second-and-later characters of an identifier may be any
2-, 3-, or 4-byte UTF-8 sequence.  The leading character must
still be ASCII `[a-zA-Z_]`.

The character equivalence-class table reserves dedicated classes
for each UTF-8 lead-byte range:

| EC | Byte Range            | Description                              |
|----|-----------------------|------------------------------------------|
| 61 | `0x80`–`0xBF`         | Continuation byte                        |
| 62 | `0xC2`–`0xDF`         | 2-byte lead (U2)                         |
| 63 | `0xE0`–`0xEF`         | 3-byte lead (U3)                         |
| 64 | `0xF0`–`0xF4`         | 4-byte lead (U4)                         |
| 1  | `0xC0`–`0xC1`, `0xF5`–`0xFF` | Invalid UTF-8 (junk)              |

String literal bodies match every high byte as a single character
with no UTF-8 validation — invalid byte sequences are passed
through verbatim inside `"…"` and `'…'`.

## Token Set

The grammar declares 67 terminals.  The first six and the
identifier / literal tokens are listed below; the remainder are
keywords and single-character punctuation.

| Name           | Meaning                                  |
|----------------|------------------------------------------|
| `CONSTANT`     | Numeric literal (stored as `double`)     |
| `IDENTIFIER`   | Identifier                               |
| `STRING_LITERAL` | String literal                         |
| `INC_OP`       | `++`                                     |
| `DEC_OP`       | `--`                                     |
| `LSH_OP`       | `<<`                                     |
| `RSH_OP`       | `>>`                                     |
| `LE_OP`        | `<=`                                     |
| `GE_OP`        | `>=`                                     |
| `EQ_OP`        | `==`                                     |
| `NE_OP`        | `!=`                                     |
| `AND_OP`       | `&&`                                     |
| `OR_OP`        | `\|\|`                                    |
| `MUL_ASSIGN` … `OR_ASSIGN` | Compound-assignment operators (`*=` … `\|=`) |
| `KW_CONST` … `KW_REPEAT`   | Keyword tokens (`const`, `cvar`, `string`, `var`, `void`, `wave`, `case`, `default`, `if`, `else`, `switch`, `while`, `do`, `for`, `continue`, `break`, `return`, `repeat`) |
| Single-char    | `( ) [ ] , - + ~ ! * / % < > & ^ \| ? : = ; { }` |

## Semantic Value Type

```c
%union {
    double       dval;
    const char*  sval;
    Expression*  expr;
}
```

## AST construction

Each grammar reduction calls one of the AST-builder helpers on the
parser context:

### Expressions

| Production                | Action                                                |
|---------------------------|-------------------------------------------------------|
| Binary operator           | `createOperator(ctx, $1, $3, EOperator)`              |
| Compound assignment       | `createAssignOperator(ctx, $1, $3, EOperator)`        |
| Unary prefix              | `createCommand(ctx, ECommandType, 1, $2)`             |
| Postfix `++` / `--`       | `createOperator(ctx, $1, NULL, eINC/eDEC)`            |
| Array subscript           | `createArray(ctx, $1, $3)`                            |
| Function call             | `createFunctionCall(ctx, name, args)`                 |
| Conditional `?:`          | `createCondExpression(ctx, $1, $3, $5)`               |

### Statements

| Production       | Action                                              |
|------------------|-----------------------------------------------------|
| `if`             | `createIf(ctx, $3, $5)`                             |
| `if … else`      | `createIfElse(ctx, $3, $5, $7)`                     |
| `switch`         | `createSwitch(ctx, $3, $5)`                         |
| `while`          | `createWhile(ctx, $3, $5)`                          |
| `do … while`     | `createDoWhile(ctx, $2, $5)`                        |
| `for` (4 forms)  | `createFor(ctx, $3, $5, $7, $9)` and variants       |
| `repeat`         | `createRepeat(ctx, $3, $5)`                         |
| `case` / `default` | `createCase(ctx, $2, $4)` / `createCase(ctx, NULL, $3)` |
| `continue` / `break` | `createCommand(ctx, eCONTINUE/eBREAK, 0)`       |
| `return`         | `createCommand(ctx, eRETURN, 1, $2)`                |

### Declarations & functions

| Production         | Action                                                |
|--------------------|-------------------------------------------------------|
| Variable type      | `createVariableType(ctx, VarType)`                    |
| Declaration        | `addVariableType(ctx, $2, $1, isConst)`               |
| Initialiser        | `createOperator(ctx, $1, $3, eASSIGN)` (with stmt flags) |
| Function definition| `createFunction(ctx, signature, params, body)`        |
| List append        | `createOrAppendArgList`, `createOrAppendDeclList`,    |
|                    | `createOrAppendStmtList`                              |

### Assignment-side modification

For assignment productions (rules 52–62), the LHS expression
(`$1`) has its `direction_` field set to `eIN = 0` and
`valueCategory_` set to `eLVALUE = 1`.  This propagates through
the shared-pointer expression tree into the operator result.

The effect: the LHS variable in `var i; i = 0;` is *not*
subject to the `checkVar` uninitialised-use check in
`SeqCVariable::evaluate` — that check is gated on
`direction_ != 0`, so the parser-set `direction_ = 0` deliberately
suppresses the false-positive uninitialised-use error on the
first assignment.

## Reconstructed sources

| File                                       | Description                              |
|--------------------------------------------|------------------------------------------|
| `src/ast/seqc_parser.y`                    | Bison grammar                            |
| `src/ast/seqc_lexer.l`                     | Flex scanner                             |
| `src/ast/seqc_parser.tab.c` / `.tab.h`     | Generated parser + token enum            |
| `src/ast/seqc_lexer.c`                     | Generated scanner                        |
| `include/zhinst/ast/seqc_parser_context.hpp` | Parser context (AST builder methods)   |
| `src/ast/seqc_parser_context.cpp`          | Context implementation                   |

## See also

- \ref notes_seqc_language_features_excluded — language features
  the parser accepts but the front-end / back-end reject.
- \ref notes_custom_functions — `playWave(...)` and friends; how a
  parsed call expression reaches its emit implementation.
- \ref notes_frontend_lowering — what happens to the AST after
  parsing.
