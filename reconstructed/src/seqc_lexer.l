/* ============================================================================
 * Reconstructed from disassembly of _seqc_compiler.so
 * SeqC lexer — flex scanner for the SeqC language parser
 *
 * Original lexer: seqc_lex @0x2c7bb0 (6,902 bytes)
 * DFA: 177 states, 65 equivalence classes, 81 rules
 *
 * Flex DFA tables (binary addresses):
 *   yy_ec      @ 0x95fd40  uint8_t[256]   65 equivalence classes
 *   yy_accept  @ 0x95fe40  int16_t[177]   177 DFA states
 *   yy_chk     @ 0x95ffb0  int16_t[608]
 *   yy_base    @ 0x960470  int16_t[182]   177 states + 5 backing states
 *   yy_def     @ 0x9605e0  int16_t[182]
 *   yy_meta    @ 0x960750  uint8_t[65]    5 meta classes
 *   yy_nxt     @ 0x9607a0  int16_t[606]
 *
 * Action switch: jump table at 0x95fbf0, max action 82
 * ============================================================================ */

%{
#include "zhinst/seqc_parser_context.hpp"
#include "zhinst/expression.hpp"
#include "seqc_parser.tab.h"

/* bison-bridge expects YYSTYPE; with api.prefix {seqc}, bison generates SEQC_STYPE */
#define YYSTYPE SEQC_STYPE

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>

/* Access the parser context via yyextra */
#define CTX  (static_cast<zhinst::SeqcParserContext*>(yyextra))
%}

%option reentrant
%option bison-bridge
%option prefix="seqc"
%option noyywrap
%option nounput
%option noinput
%option extra-type="zhinst::SeqcParserContext*"

/* UTF-8 byte range definitions used in string literals */
U2      [\xc2-\xdf]
U3      [\xe0-\xef]
U4      [\xf0-\xf4]
UCONT   [\x80-\xbf]

%%

"/*"        {
                /* Rule 1: Block comment start — action @0x2c8c47 */
                if (!CTX->isComment()) {
                    CTX->startBlockComment();
                }
            }

"*/"        {
                /* Rule 2: Block comment end — action @0x2c8d63 */
                if (CTX->isComment()) {
                    CTX->endBlockComment();
                } else {
                    CTX->raiseError("unexpected token '*/'");
                }
            }

"//"        {
                /* Rule 3: Line comment start — action @0x2c8c1c */
                if (!CTX->isComment()) {
                    CTX->startLineComment();
                }
            }

"const"     { if (!CTX->isComment()) return KW_CONST;   }  /* Rule 4  — 0x2c8bac */
"cvar"      { if (!CTX->isComment()) return KW_CVAR;    }  /* Rule 5  — 0x2c8dbf */
"string"    { if (!CTX->isComment()) return KW_STRING;   }  /* Rule 6  — 0x2c8ddb */
"var"       { if (!CTX->isComment()) return KW_VAR;      }  /* Rule 7  — 0x2c8bc8 */
"void"      { if (!CTX->isComment()) return KW_VOID;     }  /* Rule 8  — 0x2c8be4 */
"wave"      { if (!CTX->isComment()) return KW_WAVE;     }  /* Rule 9  — 0x2c8d87 */
"break"     { if (!CTX->isComment()) return KW_BREAK;    }  /* Rule 10 — 0x2c8e67 */
"case"      { if (!CTX->isComment()) return KW_CASE;     }  /* Rule 11 — 0x2c8c72 */
"continue"  { if (!CTX->isComment()) return KW_CONTINUE; }  /* Rule 12 — 0x2c8df7 */
"default"   { if (!CTX->isComment()) return KW_DEFAULT;  }  /* Rule 13 — 0x2c8acc */
"do"        { if (!CTX->isComment()) return KW_DO;       }  /* Rule 14 — 0x2c8a94 */
"else"      { if (!CTX->isComment()) return KW_ELSE;     }  /* Rule 15 — 0x2c8ab0 */
"for"       { if (!CTX->isComment()) return KW_FOR;      }  /* Rule 16 — 0x2c8e13 */
"if"        { if (!CTX->isComment()) return KW_IF;       }  /* Rule 17 — 0x2c8caa */
"repeat"    { if (!CTX->isComment()) return KW_REPEAT;   }  /* Rule 18 — 0x2c8e9f */
"return"    { if (!CTX->isComment()) return KW_RETURN;   }  /* Rule 19 — 0x2c8f0b */
"switch"    { if (!CTX->isComment()) return KW_SWITCH;   }  /* Rule 20 — 0x2c8b74 */
"while"     { if (!CTX->isComment()) return KW_WHILE;    }  /* Rule 21 — 0x2c8e83 */

[a-zA-Z_]([a-zA-Z0-9_]|{U2}{UCONT}|{U3}{UCONT}{UCONT}|{U4}{UCONT}{UCONT}{UCONT})*  {
                /* Rule 22: Identifier — action @0x2c8da3 */
                if (CTX->isComment()) { /* skip */ }
                else {
                    yylval->sval = yytext;
                    return IDENTIFIER;
                }
            }

0[xX][0-9a-fA-F]+  {
                /* Rule 23: Hex integer — action @0x2c8f27 -> 0x2c9340 */
                if (CTX->isComment()) { /* skip */ }
                else {
                    unsigned int tmp;
                    sscanf(yytext + 2, "%x", &tmp);
                    yylval->dval = static_cast<double>(static_cast<int64_t>(tmp));
                    return CONSTANT;
                }
            }

0[bB][01]+  {
                /* Rule 24: Binary integer — action @0x2c8ebb -> stoull */
                if (CTX->isComment()) { /* skip */ }
                else {
                    std::string s(yytext);
                    unsigned long long val = std::stoull(s.substr(2), nullptr, 2);
                    yylval->dval = static_cast<double>(val);
                    return CONSTANT;
                }
            }

0[0-9]+     {
                /* Rule 25: Leading-zero integer — action @0x2c8f5f -> 0x2c9290 */
                if (CTX->isComment()) { /* skip */ }
                else {
                    int tmp;
                    sscanf(yytext, "%d", &tmp);
                    yylval->dval = static_cast<double>(tmp);
                    return CONSTANT;
                }
            }

[0-9]+      {
                /* Rule 26: Decimal integer — action @0x2c8e4b -> 0x2c9290 */
                if (CTX->isComment()) { /* skip */ }
                else {
                    int tmp;
                    sscanf(yytext, "%d", &tmp);
                    yylval->dval = static_cast<double>(tmp);
                    return CONSTANT;
                }
            }

[0-9]+[eE][+-]?[0-9]+  {
                /* Rule 27: Integer with exponent — action @0x2c8f7b -> 0x2c9253 */
                if (CTX->isComment()) { /* skip */ }
                else {
                    sscanf(yytext, "%lf", &yylval->dval);
                    return CONSTANT;
                }
            }

[0-9]*\.[0-9]+([eE][+-]?[0-9]+)?  {
                /* Rule 28: Float (digits after dot) — action @0x2c8a5c -> 0x2c9253 */
                if (CTX->isComment()) { /* skip */ }
                else {
                    sscanf(yytext, "%lf", &yylval->dval);
                    return CONSTANT;
                }
            }

[0-9]+\.([eE][+-]?[0-9]+)?  {
                /* Rule 29: Float (dot suffix) — action @0x2c8b90 -> 0x2c9253 */
                if (CTX->isComment()) { /* skip */ }
                else {
                    sscanf(yytext, "%lf", &yylval->dval);
                    return CONSTANT;
                }
            }

L?'([^'\\\n]|\\.)+' {
                /* Rule 30: Single-quoted string — action @0x2c87bc -> 0x2c9276 */
                if (CTX->isComment()) { /* skip */ }
                else {
                    yylval->sval = yytext;
                    return STRING_LITERAL;
                }
            }

L?\"([^"\\\n]|\\.)*\" {
                /* Rule 31: Double-quoted string — action @0x2c87a0 -> 0x2c9276 */
                if (CTX->isComment()) { /* skip */ }
                else {
                    yylval->sval = yytext;
                    return STRING_LITERAL;
                }
            }

">>="       { if (!CTX->isComment()) return RSH_ASSIGN;  }  /* Rule 32 — 0x2c8c8e */
"<<="       { if (!CTX->isComment()) return LSH_ASSIGN;  }  /* Rule 33 — 0x2c8a40 */
"+="        { if (!CTX->isComment()) return ADD_ASSIGN;   }  /* Rule 34 — 0x2c8ae8 */
"-="        { if (!CTX->isComment()) return SUB_ASSIGN;   }  /* Rule 35 — 0x2c8880 */
"*="        { if (!CTX->isComment()) return MUL_ASSIGN;   }  /* Rule 36 — 0x2c8feb */
"/="        { if (!CTX->isComment()) return DIV_ASSIGN;   }  /* Rule 37 — 0x2c88b8 */
"%="        { if (!CTX->isComment()) return MOD_ASSIGN;   }  /* Rule 38 — 0x2c8810 */
"&="        { if (!CTX->isComment()) return AND_ASSIGN;   }  /* Rule 39 — 0x2c88d4 */
"^="        { if (!CTX->isComment()) return XOR_ASSIGN;   }  /* Rule 40 — 0x2c8f97 */
"|="        { if (!CTX->isComment()) return OR_ASSIGN;    }  /* Rule 41 — 0x2c8ce2 */
">>"        { if (!CTX->isComment()) return RSH_OP;       }  /* Rule 42 — 0x2c8b3c */
"<<"        { if (!CTX->isComment()) return LSH_OP;       }  /* Rule 43 — 0x2c89ec */
"++"        { if (!CTX->isComment()) return INC_OP;       }  /* Rule 44 — 0x2c8998 */
"--"        { if (!CTX->isComment()) return DEC_OP;       }  /* Rule 45 — 0x2c8fb3 */
"&&"        { if (!CTX->isComment()) return AND_OP;       }  /* Rule 46 — 0x2c882c */
"||"        { if (!CTX->isComment()) return OR_OP;        }  /* Rule 47 — 0x2c8944 */
"<="        { if (!CTX->isComment()) return LE_OP;        }  /* Rule 48 — 0x2c8b58 */
">="        { if (!CTX->isComment()) return GE_OP;        }  /* Rule 49 — 0x2c8864 */
"=="        { if (!CTX->isComment()) return EQ_OP;        }  /* Rule 50 — 0x2c8fcf */
"!="        { if (!CTX->isComment()) return NE_OP;        }  /* Rule 51 — 0x2c8b20 */

";"         { if (!CTX->isComment()) return ';';  }  /* Rule 52 — 0x2c8a08 */
"{"         { if (!CTX->isComment()) return '{';  }  /* Rule 53 — 0x2c8960 */
"}"         { if (!CTX->isComment()) return '}';  }  /* Rule 54 — 0x2c89d0 */
","         { if (!CTX->isComment()) return ',';  }  /* Rule 55 — 0x2c8e2f */
":"         { if (!CTX->isComment()) return ':';  }  /* Rule 56 — 0x2c8a78 */
"="         { if (!CTX->isComment()) return '=';  }  /* Rule 57 — 0x2c8cc6 */
"("         { if (!CTX->isComment()) return '(';  }  /* Rule 58 — 0x2c88f0 */
")"         { if (!CTX->isComment()) return ')';  }  /* Rule 59 — 0x2c8928 */
"["         { if (!CTX->isComment()) return '[';  }  /* Rule 60 — 0x2c9007 */
"]"         { if (!CTX->isComment()) return ']';  }  /* Rule 61 — 0x2c889c */
"."         { if (!CTX->isComment()) return '.';  }  /* Rule 62 — 0x2c9023 */
"&"         { if (!CTX->isComment()) return '&';  }  /* Rule 63 — 0x2c8a24 */
"!"         { if (!CTX->isComment()) return '!';  }  /* Rule 64 — 0x2c903f */
"~"         { if (!CTX->isComment()) return '~';  }  /* Rule 65 — 0x2c89b4 */
"-"         { if (!CTX->isComment()) return '-';  }  /* Rule 66 — 0x2c87f4 */
"+"         { if (!CTX->isComment()) return '+';  }  /* Rule 67 — 0x2c9077 */
"*"         { if (!CTX->isComment()) return '*';  }  /* Rule 68 — 0x2c8c00 */
"/"         { if (!CTX->isComment()) return '/';  }  /* Rule 69 — 0x2c8b04 */
"%"         { if (!CTX->isComment()) return '%';  }  /* Rule 70 — 0x2c897c */
"<"         { if (!CTX->isComment()) return '<';  }  /* Rule 71 — 0x2c87bc */
">"         { if (!CTX->isComment()) return '>';  }  /* Rule 72 — 0x2c905b */
"^"         { if (!CTX->isComment()) return '^';  }  /* Rule 73 — 0x2c87d8 */
"|"         { if (!CTX->isComment()) return '|';  }  /* Rule 74 — 0x2c890c */
"?"         { if (!CTX->isComment()) return '?';  }  /* Rule 75 — 0x2c8848 */

[ \t\x0b\x0c]+  {
                /* Rule 76: Whitespace — skip (includes \v and \f) */
            }

\n          {
                /* Rule 77: Newline — action @0x2c8f43 */
                CTX->incrementLineNumber();
            }

\r          {
                /* Rule 78: Carriage return — action @0x2c8f4b */
                CTX->endLineComment();
            }

.           {
                /* Rule 79: Unexpected character — action @0x2c8cfe */
                if (!CTX->isComment()) {
                    CTX->raiseError("Unexpected character");
                }
            }

<<EOF>>     {
                /* Rule 81: End of input — action @0x2c7e9f / 0x2c9237 */
                return 0;
            }

%%
