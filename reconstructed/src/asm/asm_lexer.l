/* ============================================================================
 * Reconstructed from disassembly of _seqc_compiler.so
 * ASM lexer — flex scanner for the AWGAssembler's assembly language
 *
 * Original lexer: asmlex @0x290f90 (4,018 bytes)
 * DFA: 53 states, 33 equivalence classes, 19 rules
 *
 * Token return values (from disassembly):
 *   REGISTER       = 258  (R<n>, case-insensitive, sscanf "R%d")
 *   STRING         = 259  (identifier / opcode name, trackedStringCopy)
 *   STRING_LITERAL = 260  (quoted string, strdup)
 *   NUMBER         = 261  (decimal %d or hex 0x%x)
 *   PLACEHOLDER_LINE = 262
 *   DISABLE_OPT    = 263
 *   ':'            = 58
 *   ','            = 44
 * ============================================================================ */

%{
#include "zhinst/asm/asm_parser_context.hpp"
#include "zhinst/asm/asm_expression.hpp"
#include "asm_parser.tab.h"

/* bison-bridge expects YYSTYPE; with api.prefix {asm}, bison generates ASMSTYPE */
#define YYSTYPE ASMSTYPE

#include <cstdio>
#include <cstring>
#include <cctype>

/* Access the parser context via yyextra */
#define CTX  (static_cast<zhinst::AsmParserContext*>(yyextra))
%}

%option reentrant
%option bison-bridge
%option prefix="asm"
%option noyywrap
%option nounput
%option noinput
%option extra-type="zhinst::AsmParserContext*"

%%

"/*"    {
            /* Block comment start — @0x291d16 */
            if (!CTX->isComment()) {
                CTX->startBlockComment();
            }
        }

"*/"    {
            /* Block comment end — @0x291c1e */
            if (CTX->isComment()) {
                CTX->endBlockComment();
            } else {
                CTX->raiseError("unexpected token '*/'");
            }
        }

"//"    {
            /* Line comment start — @0x291cd8 */
            if (!CTX->isComment()) {
                CTX->startLineComment();
            }
        }

\n      {
            /* Newline — @0x291c05 (DISABLE_OPT) / @0x291bf4 (end line comment) */
            if (CTX->isLineComment()) {
                CTX->endLineComment();
            } else if (!CTX->isComment()) {
                return DISABLE_OPT;
            }
        }

[0-9]+  {
            /* Decimal integer — @0x291cfd */
            if (CTX->isComment()) { /* skip */ }
            else {
                sscanf(yytext, "%d", &yylval->ival);
                return NUMBER;
            }
        }

0[xX][0-9a-fA-F]+  {
            /* Hex integer — @0x291c49 */
            if (CTX->isComment()) { /* skip */ }
            else {
                sscanf(yytext, "%i", &yylval->ival);
                return NUMBER;
            }
        }

","     {
            /* Comma — @0x291cbf */
            if (!CTX->isComment()) return ',';
        }

":"     {
            /* Colon — @0x291d54 */
            if (!CTX->isComment()) return ':';
        }

[Rr][0-9]+  {
            /* Register — @0x291d3b */
            if (CTX->isComment()) { /* skip */ }
            else {
                char buf[32];
                strncpy(buf, yytext, sizeof(buf)-1);
                buf[sizeof(buf)-1] = '\0';
                buf[0] = toupper(buf[0]);
                sscanf(buf, "R%d", &yylval->ival);
                return REGISTER;
            }
        }

\"[^\"]*\"  {
            /* Quoted string literal — @0x291dc5 */
            if (CTX->isComment()) { /* skip */ }
            else {
                yylval->sval = strdup(yytext);
                return STRING_LITERAL;
            }
        }

disable_opt {
            /* DISABLE_OPT keyword — recognized as STRING unless special */
            if (CTX->isComment()) { /* skip */ }
            else {
                yylval->sval = CTX->trackedStringCopy(yytext);
                return STRING;
            }
        }

placeholder_line {
            /* PLACEHOLDER_LINE keyword */
            if (CTX->isComment()) { /* skip */ }
            else {
                return PLACEHOLDER_LINE;
            }
        }

[a-zA-Z_][a-zA-Z0-9_]* {
            /* Identifier / opcode name — @0x291bdb */
            if (CTX->isComment()) { /* skip */ }
            else {
                yylval->sval = CTX->trackedStringCopy(yytext);
                return STRING;
            }
        }

[ \t]+  {
            /* Whitespace — skip — @0x291150 */
        }

.       {
            /* Unexpected character — @0x291c62 */
            if (!CTX->isComment()) {
                CTX->raiseError("Unexpected character");
            }
        }

<<EOF>> {
            /* End of input — @0x291dc1 */
            return 0;
        }

%%
