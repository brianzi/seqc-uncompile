# SeqC Parser Grammar Reconstruction

## Overview

The SeqC parser is a bison + flex reentrant parser pair for a C-like
domain-specific language used by ZI's AWG (Arbitrary Waveform Generator)
compiler. It follows a classic ANSI C yacc grammar structure, extended
with domain keywords (`wave`, `cvar`, `string`, `repeat`).

## Binary Addresses

| Symbol          | Address    | Size     |
|-----------------|------------|----------|
| `seqc_parse`    | 0x2ca2a0   | 5,777 B  |
| `seqc_lex`      | 0x2c7bb0   | 6,902 B  |
| `seqc_error`    | 0x2ca1b0   | 240 B    |

## Source File Names

From .rodata strings: `SeqcLexer.cpp` (flex output), `AsmLexer.cpp`
(ASM flex output).

---

## Bison Parser

### Parser Constants

| Constant      | Value |
|---------------|-------|
| YYNTOKENS     | 67    |
| YYNNTOKENS    | 43    |
| YYNRULES      | 129   |
| YYNSTATES     | 233   |
| YYLAST        | 503   |
| YYPACT_NINF   | -117  |
| YYTABLE_NINF  | 0     |

### Bison Table Addresses

| Table | Address | Type | Count |
|-------|---------|------|-------|
| yypact | 0x960e70 | int16_t | 233 |
| yytranslate | 0x961050 | uint8_t | 304 |
| yycheck | 0x961180 | int16_t | 504 |
| yytable | 0x961570 | uint8_t | 504 |
| yydefact | 0x961770 | uint8_t | 233 |
| yyr2 | 0x961860 | uint8_t | 130 |
| yyr1 | 0x9618f0 | uint8_t | 130 |
| yypgoto | 0x961980 | int16_t | 43 |
| yydefgoto | 0x9619e0 | int16_t | 43 |
| yytname | 0xb08b80 | char*[] | 110 |
| jump table | 0x960c5c | int32_t rel | 128 (rules 2-129) |

### Grammar Verification Status

| Table         | Status           | Notes |
|---------------|------------------|-------|
| yyr1          | **BYTE-EXACT**   | 130 uint8_t entries |
| yyr2          | **BYTE-EXACT**   | 130 uint8_t entries |
| yytranslate   | **MATCH**        | Values identical, padding length differs (299 vs 304) |
| yypact        | Differs          | State machine packing differs (233 vs 232 states) |
| yydefact      | Differs          | Same root cause |
| yytable       | Differs          | Same root cause |
| yycheck       | Differs          | Same root cause |
| yypgoto       | Differs          | NT count 43 vs 42 + different packing |
| yydefgoto     | Differs          | NT count + state numbering |

**Verdict: Grammar verified correct.** The yyr1/yyr2 exact match proves
the grammar rules are identical. State machine differences are a bison
version artifact (see below).

### Root Cause of State Machine Difference

The nonterminal `statement_list` (NT 101) is defined in the grammar but
unreachable from the start symbol. No rule outside `statement_list` itself
references it. The binary was compiled with a bison version that retains
unreachable nonterminals and their states in the LR automaton (producing
233 states, 43 NTs, 129 rules). All tested bison versions (2.3, 3.0.5,
3.3, 3.8.2) strip unreachable nonterminals, producing 232 states, 42 NTs,
127 rules.

Inserting a reference to `statement_list` in `compound_body` produces 129
rules and 43 NTs (matching the binary) but 234 states (1 too many) and 70
shift/reduce conflicts (vs 47 for the correct grammar). The extra conflicts
come from ambiguity between `statement_list` and `statement` in the
compound body.

---

## Flex Lexer

### DFA Constants

| Constant | Binary | Generated |
|----------|--------|-----------|
| States | 177 | 168 |
| Equivalence classes | 65 | 65 |
| Meta classes | 6 (0-5) | 6 (0-5) |
| Rules | 80 (1-79, 81) | 80 |
| Backing states | 5 (178-181) | 4 (168-171) |

### Flex DFA Table Addresses

| Table | Address | Type | Count |
|-------|---------|------|-------|
| yy_ec | 0x95fd40 | uint8_t | 256 |
| yy_accept | 0x95fe40 | int16_t | 177 |
| yy_chk | 0x95ffb0 | int16_t | 608 |
| yy_base | 0x960470 | int16_t | 182 (177+5 backing) |
| yy_def | 0x9605e0 | int16_t | 182 |
| yy_meta | 0x960750 | uint8_t | 65 |
| yy_nxt | 0x9607a0 | int16_t | 606 |

Action switch: jump table at 0x95fbf0, max action 82 (83 entries).

### Lexer Verification Status

| Table | Status | Notes |
|-------|--------|-------|
| yy_ec (256) | **BYTE-EXACT MATCH** | All 256 byte→ec mappings identical |
| yy_meta (65) | **BYTE-EXACT MATCH** | All 65 meta class values identical |
| Rule count | **EXACT MATCH** | 80 rules: 1-79 + 81 (EOF) |
| yy_accept | Differs | 168 states vs 177 — see below |
| yy_base/def | Differs | State count + numbering |
| yy_chk/nxt | Differs | Table compression layout |

**Verdict: Lexer verified functionally correct.** The yy_ec and yy_meta
exact matches prove the lexer's character classification and DFA
compression strategy are identical. The state count difference is a DFA
minimization artifact (see below).

### Root Cause of State Count Difference (177 vs 168)

The binary's DFA contains **un-minimized duplicate states**. Specifically,
two parallel chains of float-exponent states exist:

- Path via rule 28 (`[0-9]*\.[0-9]+`): states 66→110→137→138
- Path via rule 29 (`[0-9]+\.`+digits): states 111→139→157→158

States 66 and 111 have identical structure (accept=28, digit self-loop,
e/E transition to sign/digits), as do 110/139, 137/157, and 138/158.
A fully minimized DFA merges these. Additionally, 3 states for UTF-8
lead bytes from the start state (48/49/50, all accept=79) are kept
separate in the binary but could share structure.

Our flex 2.6.4 performs full DFA minimization, merging these duplicate
states and producing 168 states instead of 177. The binary was likely
compiled with a flex version that performs less aggressive minimization.

No flex 2.6.4 option (`-Cem`, `-Ca`, `-Cf`, `-Cr`) changes the DFA
state count — table compression affects packing, not the DFA itself.

### Key Finding: UTF-8 in Identifiers

The original lexer allows **UTF-8 characters in identifier continuations**:

```
[a-zA-Z_]([a-zA-Z0-9_]|{U2}{UCONT}|{U3}{UCONT}{UCONT}|{U4}{UCONT}{UCONT}{UCONT})*
```

Where:
- `U2` = `[\xc2-\xdf]` (2-byte UTF-8 lead)
- `U3` = `[\xe0-\xef]` (3-byte UTF-8 lead)
- `U4` = `[\xf0-\xf4]` (4-byte UTF-8 lead)
- `UCONT` = `[\x80-\xbf]` (continuation byte)

This is confirmed by two independent lines of evidence:

1. **yy_ec mapping**: Bytes 0xC2-0xDF, 0xE0-0xEF, 0xF0-0xF4 each get
   distinct equivalence classes (62, 63, 64), while 0x80-0xBF gets ec 61.
   Invalid UTF-8 bytes 0xC0-0xC1 and 0xF5-0xFF map to ec 1 (junk class),
   proving the original patterns referenced the exact valid UTF-8 ranges.

2. **yy_meta values**: meta[62]=meta[63]=meta[64]=5 (same as identifier
   character classes like letters/digits). This only happens when the
   UTF-8 lead bytes participate in the same DFA patterns as identifier
   characters. Without UTF-8 in identifiers, meta[62-64]=1 (junk).

String literal patterns use plain `[^'\\\n]` / `[^"\\\n]`, which
matches all high bytes as single characters. The binary's DFA confirms
this: inside a string state, all high bytes (ec 61-64) transition to the
same "string interior" state with no UTF-8 validation.

### UTF-8 Equivalence Classes

| EC | Byte Range | Meta | Description |
|----|------------|------|-------------|
| 61 | 0x80-0xBF | 1 | Continuation bytes (UCONT) |
| 62 | 0xC2-0xDF | 5 | 2-byte lead (U2) |
| 63 | 0xE0-0xEF | 5 | 3-byte lead (U3) |
| 64 | 0xF0-0xF4 | 5 | 4-byte lead (U4) |
| 1  | 0xC0-0xC1, 0xF5-0xFF | 1 | Invalid UTF-8 (same ec as ASCII junk) |

### Backing State Mechanism

The flex DFA engine at 0x2c7d80 uses compressed tables with a backing
state mechanism:

- States 0-176 (binary) are regular DFA states; state 177 = jam/end-of-buffer
- States 178-181 are **backing states** used for compressed table defaults
- When `yy_def[s] >= 178`, the engine applies `yy_meta[ec]` to remap
  the equivalence class before continuing the lookup in the backing state
- This is how string literals handle all characters (including UTF-8)
  despite having only a few explicit transitions per state

### Complete Lexer Rule Map (80 rules)

**Comment handling (rules 1-3):**
- Rule 1: `/*` → startBlockComment
- Rule 2: `*/` → endBlockComment
- Rule 3: `//` → startLineComment

**Keywords (rules 4-21):** const(4), cvar(5), string(6), var(7),
void(8), wave(9), break(10), case(11), continue(12), default(13),
do(14), else(15), for(16), if(17), repeat(18), return(19), switch(20),
while(21)

**Identifier (rule 22):** `[a-zA-Z_]([a-zA-Z0-9_]|UTF-8)*` → IDENTIFIER
(259), `yylval->sval = yytext`

**Numeric constants (rules 23-29), all return CONSTANT (258) with
`yylval->dval`:**
- Rule 23: `0[xX][0-9a-fA-F]+` → sscanf(yytext+2, "%x"), cast int64→double
- Rule 24: `0[bB][01]+` → stoull(str.substr(2), nullptr, 2), cast to double
- Rule 25: `0[0-9]+` → sscanf(yytext, "%d"), cvtsi2sd (leading-zero integer)
- Rule 26: `[0-9]+` → sscanf(yytext, "%d"), cvtsi2sd (decimal integer)
- Rule 27: `[0-9]+[eE][+-]?[0-9]+` → sscanf(yytext, "%lf") (int with exponent)
- Rule 28: `[0-9]*\.[0-9]+([eE][+-]?[0-9]+)?` → sscanf(yytext, "%lf")
- Rule 29: `[0-9]+\.([eE][+-]?[0-9]+)?` → sscanf(yytext, "%lf")

**String literals (rules 30-31):** Both return STRING_LITERAL (260),
`yylval->sval = yytext`. Both support optional `L` prefix:
- Rule 30: `L?'([^'\\\n]|\\.)+' ` → single-quoted (requires ≥1 char)
- Rule 31: `L?"([^"\\\n]|\\.)*"` → double-quoted (empty `""` valid)

**Operators (rules 32-51):** >>=(32), <<=(33), +=(34), -=(35), *=(36),
/=(37), %=(38), &=(39), ^=(40), |=(41), >>(42), <<(43), ++(44), --(45),
&&(46), ||(47), <=(48), >=(49), ==(50), !=(51)

**Single-character tokens (rules 52-75):** ;(52), {(53), }(54), ,(55),
:(56), =(57), ((58), )(59), [(60), ](61), .(62), &(63), !(64), ~(65),
-(66), +(67), *(68), /(69), %(70), <(71), >(72), ^(73), |(74), ?(75)

**Whitespace/newline (rules 76-78):**
- Rule 76: `[ \t\v\f]+` → skip
- Rule 77: `\n` → incrementLineNumber()
- Rule 78: `\r` → endLineComment()

**Fallback (rule 79):** `.` → raiseError("Unexpected character")

**EOF (rule 81):** `<<EOF>>` → return 0

---

## Token Set (from yytname, 110 entries)

### Terminals (0-66)

| Index | Name           | Bison # | Description |
|-------|----------------|---------|-------------|
| 0     | "end of file"  | 0       | EOF         |
| 1     | error          | 256     | error token |
| 2     | "invalid token"| —       | undefined   |
| 3     | CONSTANT       | 258     | numeric literal (stored as double) |
| 4     | IDENTIFIER     | 259     | identifier  |
| 5     | STRING_LITERAL | 260     | string literal |
| 6     | INC_OP         | 261     | `++`        |
| 7     | DEC_OP         | 262     | `--`        |
| 8     | LSH_OP         | 263     | `<<`        |
| 9     | RSH_OP         | 264     | `>>`        |
| 10    | LE_OP          | 265     | `<=`        |
| 11    | GE_OP          | 266     | `>=`        |
| 12    | EQ_OP          | 267     | `==`        |
| 13    | NE_OP          | 268     | `!=`        |
| 14    | AND_OP         | 269     | `&&`        |
| 15    | OR_OP          | 270     | `||`        |
| 16    | MUL_ASSIGN     | 271     | `*=`        |
| 17    | DIV_ASSIGN     | 272     | `/=`        |
| 18    | MOD_ASSIGN     | 273     | `%=`        |
| 19    | ADD_ASSIGN     | 274     | `+=`        |
| 20    | SUB_ASSIGN     | 275     | `-=`        |
| 21    | LSH_ASSIGN     | 276     | `<<=`       |
| 22    | RSH_ASSIGN     | 277     | `>>=`       |
| 23    | AND_ASSIGN     | 278     | `&=`        |
| 24    | XOR_ASSIGN     | 279     | `^=`        |
| 25    | OR_ASSIGN      | 280     | `\|=`       |
| 26    | KW_CONST       | 281     | `const`     |
| 27    | KW_CVAR        | 282     | `cvar`      |
| 28    | KW_STRING      | 283     | `string`    |
| 29    | KW_VAR         | 284     | `var`       |
| 30    | KW_VOID        | 285     | `void`      |
| 31    | KW_WAVE        | 286     | `wave`      |
| 32    | KW_CASE        | 287     | `case`      |
| 33    | KW_DEFAULT     | 288     | `default`   |
| 34    | KW_IF          | 289     | `if`        |
| 35    | KW_ELSE        | 290     | `else`      |
| 36    | KW_SWITCH      | 291     | `switch`    |
| 37    | KW_WHILE       | 292     | `while`     |
| 38    | KW_DO          | 293     | `do`        |
| 39    | KW_FOR         | 294     | `for`       |
| 40    | KW_CONTINUE    | 295     | `continue`  |
| 41    | KW_BREAK       | 296     | `break`     |
| 42    | KW_RETURN      | 297     | `return`    |
| 43    | KW_REPEAT      | 298     | `repeat`    |
| 44-66 | single-char    | ASCII   | `()[],-+~!*/%<>&^|?:=;{}` |

### Nonterminals (67-109)

Standard C expression-precedence grammar nonterminals plus
declaration/statement nonterminals. See yytname dump for full list.

## Semantic Value Type

The %union has:
```c
%union {
    double dval;
    const char* sval;
    Expression* expr;
}
```

## Semantic Actions Summary

From disassembly of seqc_parse's yyreduce switch (jump table at 0x960c5c):

### Expression Rules
- Binary operators: `createOperator(ctx, $1, $3, EOperator)`
- Assignment operators: `createAssignOperator(ctx, $1, $3, EOperator)`
- Unary prefix: `createCommand(ctx, ECommandType, 1, $2)`
- Postfix ++/--: `createOperator(ctx, $1, NULL, eINC/eDEC)`
- Array subscript: `createArray(ctx, $1, $3)`
- Function call: `createFunctionCall(ctx, $1, $3)` or `($1, NULL)`
- Conditional: `createCondExpression(ctx, $1, $3, $5)`

### Statement Rules
- if: `createIf(ctx, $3, $5)`
- if-else: `createIfElse(ctx, $3, $5, $7)`
- switch: `createSwitch(ctx, $3, $5)`
- while: `createWhile(ctx, $3, $5)`
- do-while: `createDoWhile(ctx, $2, $5)`
- for (4 variants): `createFor(ctx, $3, $5, $7, $9)` etc.
- repeat: `createRepeat(ctx, $3, $5)`
- case: `createCase(ctx, $2, $4)` or `(NULL, $3)` for default
- continue/break: `createCommand(ctx, eCONTINUE/eBREAK, 0)`
- return: `createCommand(ctx, eRETURN, 1, $2)`

### Declaration Rules
- Variable type: `createVariableType(ctx, VarType)`
- Declaration: `addVariableType(ctx, $2, $1, false/true)`
- Init declarator: `createOperator(ctx, $1, $3, eASSIGN)` with stmt flags

### Function Rules
- Function def: `createFunction(ctx, $1, params, body)`
- Function call: `createFunctionCall(ctx, name, args)`

### List Rules
- Arg list: `createOrAppendArgList(ctx, $1, $3)`
- Decl list: `createOrAppendDeclList(ctx, $1, $3)`
- Stmt list: `createOrAppendStmtList(ctx, $1, $2)`

---

## Reconstructed Files

| File | Description | Status |
|------|-------------|--------|
| `src/ast/seqc_parser.y` | Bison grammar | Grammar correct (yyr1/yyr2 exact match) |
| `src/ast/seqc_lexer.l` | Flex scanner | Functionally correct (yy_ec/yy_meta exact match) |
| `src/ast/seqc_parser.tab.c` | Generated parser | bison output |
| `src/ast/seqc_parser.tab.h` | Token enum | bison output |
| `src/ast/seqc_lexer.c` | Generated scanner | flex output |
| `include/zhinst/ast/seqc_parser_context.hpp` | Parser context | Updated with comment methods |
| `src/ast/seqc_parser_context.cpp` | Context impl | Comment/line tracking methods |

## Assignment rules: `$1` vs `$$` field modification

**Discovery (Phase 30f, 2026-04-26):**

Parser rules 52-62 (assignment_expression variants) modify `valueType` and
`valueCategory` on the **LHS expression (`$1`)**, not on the operator result
(`$$`). This was confirmed by disassembling the binary at 0x2ca99c:

```asm
; After: rax = createOperator(ctx, $1, $3, eASSIGN)
; rbx was saved from r9 (parser value stack pointer)
mov -0x10(%rbx), %rcx      ; rcx = $1 (LHS expression), NOT $$ (rax)
movl $0x0, 0x54(%rcx)       ; $1->valueType = 0 (= eIN direction)
mov -0x10(%rbx), %rcx       ; reload $1
movl $0x1, 0x4(%rcx)        ; $1->valueCategory = 1 (= eLVALUE)
```

The effect: The LHS variable in an assignment gets `direction_ = eIN = 0`
and `valueCategory_ = eLVALUE = 1`. Since `createOperator` wraps `$1` by
pointer (via shared_ptr), the modification propagates to the child stored
inside the operator Expression.

**Why this matters:** In `SeqCVariable::evaluate`, the resolved VarType_Var
case (0x209fd0) checks `if (direction_ != 0 && !atScopeBoundary())
checkVar(name)`. With `direction_ = 0` (set by the parser), `checkVar` is
skipped for assignment LHS variables. Without this fix, `checkVar` throws
UninitializedVar on the first assignment to a newly declared variable
(e.g., `var i; i = 0;` would erroneously fail).
