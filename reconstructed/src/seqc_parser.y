/* ============================================================================
 * Reconstructed from disassembly of _seqc_compiler.so
 * SeqC parser — bison grammar for the AWG sequencer C-like language
 *
 * Original parser: seqc_parse @0x2ca2a0 (5,777 bytes)
 * Grammar: 129 rules, 67 terminals, 43 nonterminals, 233 states
 * Jump table for semantic actions at 0x960c5c (128 entries, rules 2-129).
 *
 * Verification status:
 *   - yyr1 (rule→nonterminal map): BYTE-EXACT MATCH with binary
 *   - yyr2 (rule RHS lengths):     BYTE-EXACT MATCH with binary
 *   - yytranslate:                  MATCH (padding length differs)
 *   - State machine tables (yypact, yydefact, yytable, yycheck, yypgoto,
 *     yydefgoto): DIFFER due to statement_list nonterminal being unreachable.
 *     The binary was compiled with a bison version that keeps unreachable
 *     nonterminals in the automaton (producing 233 states); all available
 *     bison versions (2.3, 3.0.5, 3.3, 3.8.2) strip it (producing 232).
 *     The grammar itself is correct — only the LR state packing differs.
 *
 * The grammar follows classic ANSI C yacc structure (K&R style expression
 * precedence climbing) extended with domain-specific keywords:
 *   wave, cvar, string, var, repeat
 *
 * The parser is reentrant (bison 3.x %define api.pure full).
 * Parameters:
 *   ctx     — SeqcParserContext* (parser context)
 *   result  — Expression** (output: root of parse tree)
 *   scanner — void* (opaque flex scanner handle)
 * ============================================================================ */

%{
#include "zhinst/expression.hpp"
#include "zhinst/seqc_parser_context.hpp"
#include "zhinst/resources.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

using namespace zhinst;
%}

/* Bison configuration */
%define api.pure full
%define api.prefix {seqc_}

/* Parser parameters — matches seqc_parse(ctx, result, scanner) @0x2ca2a0 */
%parse-param {zhinst::SeqcParserContext* ctx}
%parse-param {zhinst::Expression** result}
%parse-param {void* scanner}

/* Lexer parameter */
%lex-param {void* scanner}

/* Ensure generated .tab.h is self-contained */
%code requires {
    namespace zhinst {
        class SeqcParserContext;
        struct Expression;
    }
}

/* Forward-declare seqc_lex after SEQC_STYPE is defined */
%code {
    int seqc_lex(SEQC_STYPE* lvalp, void* scanner);
}

/* Semantic value union */
%union {
    double                      dval;   /* CONSTANT */
    const char*                 sval;   /* IDENTIFIER, STRING_LITERAL */
    zhinst::Expression*         expr;   /* all nonterminals */
}

/* Token declarations */
%token <dval> CONSTANT
%token <sval> IDENTIFIER STRING_LITERAL
%token INC_OP DEC_OP LSH_OP RSH_OP
%token LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP
%token MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN SUB_ASSIGN
%token LSH_ASSIGN RSH_ASSIGN AND_ASSIGN XOR_ASSIGN OR_ASSIGN
%token KW_CONST KW_CVAR KW_STRING KW_VAR KW_VOID KW_WAVE
%token KW_CASE KW_DEFAULT KW_IF KW_ELSE KW_SWITCH
%token KW_WHILE KW_DO KW_FOR KW_CONTINUE KW_BREAK KW_RETURN
%token KW_REPEAT

/* Nonterminal types */
%type <expr> primary_expression postfix_expression argument_expression_list
%type <expr> unary_expression
%type <expr> multiplicative_expression additive_expression shift_expression
%type <expr> relational_expression equality_expression
%type <expr> and_expression exclusive_or_expression inclusive_or_expression
%type <expr> logical_and_expression logical_or_expression
%type <expr> conditional_expression assignment_expression expression
%type <expr> constant_expression declaration init_declarator_list init_declarator
%type <expr> type_specifier declarator function_declarator
%type <expr> parameter_list parameter_declaration identifier initializer
%type <expr> statement unlabeled_statement labeled_statement
%type <expr> compound_statement compound_body statement_list
%type <expr> expression_statement selection_statement iteration_statement
%type <expr> jump_statement translation_unit external_declaration_list
%type <expr> external_declaration function_definition

/* Start symbol */
%start translation_unit

%%

/* ========================================================================
 * Rules 2-5: primary_expression
 * ======================================================================== */
primary_expression
    : identifier                                /* Rule 2 — passthrough */
        { $$ = $1; }
    | CONSTANT                                  /* Rule 3 — @0x2ca52a: createValue */
        { $$ = createValue(ctx, $1); }
    | STRING_LITERAL                            /* Rule 4 — @0x2ca548: createString */
        { $$ = createString(ctx, $1); }
    | '(' expression ')'                        /* Rule 5 — @0x2ca55a: $$ = $2 */
        { $$ = $2; }
    ;

/* ========================================================================
 * Rules 6-11: postfix_expression
 * ======================================================================== */
postfix_expression
    : primary_expression                        /* Rule 6 — passthrough */
        { $$ = $1; }
    | postfix_expression '[' expression ']'     /* Rule 7 — createArray */
        { $$ = createArray(ctx, $1, $3); }
    | postfix_expression '(' ')'                /* Rule 8 — createFunctionCall(name, NULL) */
        { $$ = createFunctionCall(ctx, $1, NULL); }
    | postfix_expression '(' argument_expression_list ')'  /* Rule 9 — createFunctionCall */
        { $$ = createFunctionCall(ctx, $1, $3); }
    | postfix_expression INC_OP                 /* Rule 10 — postfix ++ */
        { $$ = createOperator(ctx, $1, NULL, EOperator::eINC); }
    | postfix_expression DEC_OP                 /* Rule 11 — postfix -- */
        { $$ = createOperator(ctx, $1, NULL, EOperator::eDEC); }
    ;

/* ========================================================================
 * Rules 12-13: argument_expression_list
 * ======================================================================== */
argument_expression_list
    : assignment_expression                     /* Rule 12 */
        { $$ = $1; }
    | argument_expression_list ',' assignment_expression  /* Rule 13 — createOrAppendArgList */
        { $$ = createOrAppendArgList(ctx, $1, $3); }
    ;

/* ========================================================================
 * Rules 14-20: unary_expression
 * ======================================================================== */
unary_expression
    : postfix_expression                        /* Rule 14 — passthrough */
        { $$ = $1; }
    | INC_OP unary_expression                   /* Rule 15 — prefix ++ */
        { $$ = createOperator(ctx, NULL, $2, EOperator::eINC); }
    | DEC_OP unary_expression                   /* Rule 16 — prefix -- */
        { $$ = createOperator(ctx, NULL, $2, EOperator::eDEC); }
    | '-' unary_expression                      /* Rule 17 — unary negation */
        { $$ = createCommand(ctx, ECommandType::eNEG, 1, $2); }
    | '+' unary_expression                      /* Rule 18 — unary positive */
        { $$ = createCommand(ctx, ECommandType::ePOS, 1, $2); }
    | '~' unary_expression                      /* Rule 19 — bitwise NOT */
        { $$ = createCommand(ctx, ECommandType::eINV, 1, $2); }
    | '!' unary_expression                      /* Rule 20 — logical NOT */
        { $$ = createCommand(ctx, ECommandType::eNOT, 1, $2); }
    ;

/* ========================================================================
 * Rules 21-24: multiplicative_expression
 * ======================================================================== */
multiplicative_expression
    : unary_expression                          /* Rule 21 */
        { $$ = $1; }
    | multiplicative_expression '*' unary_expression  /* Rule 22 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eMUL); }
    | multiplicative_expression '/' unary_expression  /* Rule 23 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eDIV); }
    | multiplicative_expression '%' unary_expression  /* Rule 24 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eMOD); }
    ;

/* ========================================================================
 * Rules 25-27: additive_expression
 * ======================================================================== */
additive_expression
    : multiplicative_expression                 /* Rule 25 */
        { $$ = $1; }
    | additive_expression '+' multiplicative_expression  /* Rule 26 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eADD); }
    | additive_expression '-' multiplicative_expression  /* Rule 27 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eSUB); }
    ;

/* ========================================================================
 * Rules 28-30: shift_expression
 * ======================================================================== */
shift_expression
    : additive_expression                       /* Rule 28 */
        { $$ = $1; }
    | shift_expression LSH_OP additive_expression  /* Rule 29 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eSHL); }
    | shift_expression RSH_OP additive_expression  /* Rule 30 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eSHR); }
    ;

/* ========================================================================
 * Rules 31-35: relational_expression
 * ======================================================================== */
relational_expression
    : shift_expression                          /* Rule 31 */
        { $$ = $1; }
    | relational_expression '<' shift_expression  /* Rule 32 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eLT); }
    | relational_expression '>' shift_expression  /* Rule 33 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eGT); }
    | relational_expression LE_OP shift_expression  /* Rule 34 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eLE); }
    | relational_expression GE_OP shift_expression  /* Rule 35 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eGE); }
    ;

/* ========================================================================
 * Rules 36-38: equality_expression
 * ======================================================================== */
equality_expression
    : relational_expression                     /* Rule 36 */
        { $$ = $1; }
    | equality_expression EQ_OP relational_expression  /* Rule 37 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eEQ); }
    | equality_expression NE_OP relational_expression  /* Rule 38 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eNE); }
    ;

/* ========================================================================
 * Rules 39-40: and_expression (bitwise AND)
 * ======================================================================== */
and_expression
    : equality_expression                       /* Rule 39 */
        { $$ = $1; }
    | and_expression '&' equality_expression    /* Rule 40 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eAND); }
    ;

/* ========================================================================
 * Rules 41-42: exclusive_or_expression (XOR)
 * ======================================================================== */
exclusive_or_expression
    : and_expression                            /* Rule 41 */
        { $$ = $1; }
    | exclusive_or_expression '^' and_expression  /* Rule 42 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eXOR); }
    ;

/* ========================================================================
 * Rules 43-44: inclusive_or_expression (bitwise OR)
 * ======================================================================== */
inclusive_or_expression
    : exclusive_or_expression                   /* Rule 43 */
        { $$ = $1; }
    | inclusive_or_expression '|' exclusive_or_expression  /* Rule 44 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eOR); }
    ;

/* ========================================================================
 * Rules 45-46: logical_and_expression
 * ======================================================================== */
logical_and_expression
    : inclusive_or_expression                    /* Rule 45 */
        { $$ = $1; }
    | logical_and_expression AND_OP inclusive_or_expression  /* Rule 46 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eLAND); }
    ;

/* ========================================================================
 * Rules 47-48: logical_or_expression
 * ======================================================================== */
logical_or_expression
    : logical_and_expression                    /* Rule 47 */
        { $$ = $1; }
    | logical_or_expression OR_OP logical_and_expression  /* Rule 48 */
        { $$ = createOperator(ctx, $1, $3, EOperator::eLOR); }
    ;

/* ========================================================================
 * Rules 49-50: conditional_expression (ternary)
 * ======================================================================== */
conditional_expression
    : logical_or_expression                     /* Rule 49 */
        { $$ = $1; }
    | logical_or_expression '?' expression ':' conditional_expression  /* Rule 50 */
        { $$ = createCondExpression(ctx, $1, $3, $5); }
    ;

/* ========================================================================
 * Rules 51-62: assignment_expression
 * ======================================================================== */
assignment_expression
    : conditional_expression                    /* Rule 51 */
        { $$ = $1; }
    | unary_expression '=' assignment_expression  /* Rule 52 */
        {
            /* Binary writes valueType/valueCategory to $1 (the LHS),
               NOT to $$ (the operator).  Confirmed at 0x2ca99c:
               mov -0x10(%rbx),%rcx reads $1, then writes 0x54 and 0x04.
               This sets the LHS variable to dir=eIN(0), vc=eLVALUE(1),
               which prevents checkVar from firing on uninitialized vars. */
            $1->valueType = 0;
            $1->valueCategory = 1;
            $$ = createOperator(ctx, $1, $3, EOperator::eASSIGN);
        }
    | unary_expression ADD_ASSIGN assignment_expression  /* Rule 53 */
        {
            $1->valueType = 0;
            $1->valueCategory = 1;
            $$ = createAssignOperator(ctx, $1, $3, EOperator::eADD);
        }
    | unary_expression SUB_ASSIGN assignment_expression  /* Rule 54 */
        {
            $1->valueType = 0;
            $1->valueCategory = 1;
            $$ = createAssignOperator(ctx, $1, $3, EOperator::eSUB);
        }
    | unary_expression MUL_ASSIGN assignment_expression  /* Rule 55 */
        {
            $1->valueType = 0;
            $1->valueCategory = 1;
            $$ = createAssignOperator(ctx, $1, $3, EOperator::eMUL);
        }
    | unary_expression DIV_ASSIGN assignment_expression  /* Rule 56 */
        {
            $1->valueType = 0;
            $1->valueCategory = 1;
            $$ = createAssignOperator(ctx, $1, $3, EOperator::eDIV);
        }
    | unary_expression MOD_ASSIGN assignment_expression  /* Rule 57 */
        {
            $1->valueType = 0;
            $1->valueCategory = 1;
            $$ = createAssignOperator(ctx, $1, $3, EOperator::eMOD);
        }
    | unary_expression AND_ASSIGN assignment_expression  /* Rule 58 */
        {
            $1->valueType = 0;
            $1->valueCategory = 1;
            $$ = createAssignOperator(ctx, $1, $3, EOperator::eAND);
        }
    | unary_expression XOR_ASSIGN assignment_expression  /* Rule 59 */
        {
            $1->valueType = 0;
            $1->valueCategory = 1;
            $$ = createAssignOperator(ctx, $1, $3, EOperator::eXOR);
        }
    | unary_expression OR_ASSIGN assignment_expression  /* Rule 60 */
        {
            $1->valueType = 0;
            $1->valueCategory = 1;
            $$ = createAssignOperator(ctx, $1, $3, EOperator::eOR);
        }
    | unary_expression LSH_ASSIGN assignment_expression  /* Rule 61 */
        {
            $1->valueType = 0;
            $1->valueCategory = 1;
            $$ = createAssignOperator(ctx, $1, $3, EOperator::eSHL);
        }
    | unary_expression RSH_ASSIGN assignment_expression  /* Rule 62 */
        {
            $1->valueType = 0;
            $1->valueCategory = 1;
            $$ = createAssignOperator(ctx, $1, $3, EOperator::eSHR);
        }
    ;

/* ========================================================================
 * Rule 63: expression (comma operator omitted — just assignment_expression)
 * ======================================================================== */
expression
    : assignment_expression                     /* Rule 63 */
        { $$ = $1; }
    ;

/* ========================================================================
 * Rule 64: constant_expression
 * ======================================================================== */
constant_expression
    : conditional_expression                    /* Rule 64 */
        { $$ = $1; }
    ;

/* ========================================================================
 * Rule 65: declaration (nonterminal 86)
 *   type_specifier init_declarator_list ';'
 *   Semantic action @0x2cb315: addVariableType(ctx, $1, $2, false)
 * ======================================================================== */
declaration
    : type_specifier init_declarator_list ';'   /* Rule 65 — addVariableType */
        { $$ = addVariableType(ctx, $2, $1, false); }
    ;

/* ========================================================================
 * Rules 66-67: init_declarator_list
 * ======================================================================== */
init_declarator_list
    : init_declarator                           /* Rule 66 */
        { $$ = $1; }
    | init_declarator_list ',' init_declarator  /* Rule 67 — createOrAppendDeclList */
        { $$ = createOrAppendDeclList(ctx, $1, $3); }
    ;

/* ========================================================================
 * Rules 68-69: init_declarator
 * ======================================================================== */
init_declarator
    : declarator                                /* Rule 68 */
        { $$ = $1; }
    | declarator '=' initializer                /* Rule 69 — assignment in declaration */
        {
            $$ = createOperator(ctx, $1, $3, EOperator::eASSIGN);
            $$->valueType = 0;
            $$->valueCategory = 1;
        }
    ;

/* ========================================================================
 * Rules 70-75: type_specifier
 * ======================================================================== */
type_specifier
    : KW_VAR                                    /* Rule 70 — VarType Var (2) */
        { $$ = createVariableType(ctx, VarType_Var); }
    | KW_CONST                                  /* Rule 71 — VarType Const (3) */
        { $$ = createVariableType(ctx, VarType_Const); }
    | KW_CVAR                                   /* Rule 72 — VarType Cvar (4) */
        { $$ = createVariableType(ctx, VarType_Cvar); }
    | KW_VOID                                   /* Rule 73 */
        { $$ = createVariableType(ctx, VarType_Void); }
    | KW_WAVE                                   /* Rule 74 — VarType Wave (6) */
        { $$ = createVariableType(ctx, VarType_Wave); }
    | KW_STRING                                 /* Rule 75 — VarType String (5) */
        { $$ = createVariableType(ctx, VarType_String); }
    ;

/* ========================================================================
 * Rules 76-78: declarator
 * ======================================================================== */
declarator
    : identifier                                /* Rule 76 */
        { $$ = $1; }
    | identifier '[' constant_expression ']'    /* Rule 77 — array declarator */
        { $$ = createArray(ctx, $1, $3); }
    | identifier '[' ']'                        /* Rule 78 — unsized array */
        { $$ = createArray(ctx, $1, NULL); }
    ;

/* ========================================================================
 * Rules 79-80: function_declarator
 * ======================================================================== */
function_declarator
    : identifier '(' parameter_list ')'         /* Rule 79 */
        { $$ = createFunctionCall(ctx, $1, $3); }
    | identifier '(' ')'                        /* Rule 80 */
        { $$ = createFunctionCall(ctx, $1, NULL); }
    ;

/* ========================================================================
 * Rules 81-82: parameter_list
 * ======================================================================== */
parameter_list
    : parameter_declaration                     /* Rule 81 */
        { $$ = $1; }
    | parameter_list ',' parameter_declaration  /* Rule 82 */
        { $$ = createOrAppendParamList(ctx, $1, $3); }
    ;

/* ========================================================================
 * Rules 83-84: parameter_declaration
 * ======================================================================== */
parameter_declaration
    : type_specifier declarator                 /* Rule 83 */
        { $$ = addVariableType(ctx, $2, $1, false); }
    | declarator                                /* Rule 84 */
        { $$ = $1; }
    ;

/* ========================================================================
 * Rule 85: identifier
 * ======================================================================== */
identifier
    : IDENTIFIER                                /* Rule 85 — createVariable */
        { $$ = createVariable(ctx, $1); }
    ;

/* ========================================================================
 * Rule 86: initializer
 * ======================================================================== */
initializer
    : assignment_expression                     /* Rule 86 */
        { $$ = $1; }
    ;

/* ========================================================================
 * Rules 87-88: statement
 * ======================================================================== */
statement
    : unlabeled_statement                       /* Rule 87 */
        { $$ = $1; }
    | labeled_statement                         /* Rule 88 */
        { $$ = $1; }
    ;

/* ========================================================================
 * Rules 89-93: unlabeled_statement
 * ======================================================================== */
unlabeled_statement
    : compound_statement                        /* Rule 89 */
        { $$ = $1; }
    | expression_statement                      /* Rule 90 */
        { $$ = $1; }
    | selection_statement                       /* Rule 91 */
        { $$ = $1; }
    | iteration_statement                       /* Rule 92 */
        { $$ = $1; }
    | jump_statement                            /* Rule 93 */
        { $$ = $1; }
    ;

/* ========================================================================
 * Rules 94-97: labeled_statement
 * ======================================================================== */
labeled_statement
    : KW_CASE constant_expression ':' statement /* Rule 94 — createCase(val, body) */
        { $$ = createCase(ctx, $2, $4); }
    | KW_DEFAULT ':' statement                  /* Rule 95 — createCase(NULL, body) */
        { $$ = createCase(ctx, NULL, $3); }
    | KW_CASE constant_expression ':'           /* Rule 96 — createCase(val, NULL) */
        { $$ = createCase(ctx, $2, NULL); }
    | KW_DEFAULT ':'                            /* Rule 97 — createCase(NULL, NULL) */
        { $$ = createCase(ctx, NULL, NULL); }
    ;

/* ========================================================================
 * Rules 98-99: compound_statement
 * ======================================================================== */
compound_statement
    : '{' '}'                                   /* Rule 98 — empty block */
        { $$ = NULL; }
    | '{' compound_body '}'                     /* Rule 99 */
        { $$ = $2; }
    ;

/* ========================================================================
 * Rules 100-103: compound_body
 * ======================================================================== */
compound_body
    : declaration                               /* Rule 100 */
        { $$ = $1; }
    | statement                                 /* Rule 101 */
        { $$ = $1; }
    | compound_body declaration                 /* Rule 102 */
        { $$ = createOrAppendStmtList(ctx, $1, $2); }
    | compound_body statement                   /* Rule 103 */
        { $$ = createOrAppendStmtList(ctx, $1, $2); }
    ;

/* ========================================================================
 * Rules 104-105: statement_list (used in some contexts)
 * ======================================================================== */
statement_list
    : statement                                 /* Rule 104 */
        { $$ = $1; }
    | statement_list statement                  /* Rule 105 */
        { $$ = createOrAppendStmtList(ctx, $1, $2); }
    ;

/* ========================================================================
 * Rules 106-107: expression_statement
 * ======================================================================== */
expression_statement
    : ';'                                       /* Rule 106 — empty statement */
        { $$ = NULL; }
    | expression ';'                            /* Rule 107 */
        { $$ = $1; }
    ;

/* ========================================================================
 * Rules 108-110: selection_statement
 * ======================================================================== */
selection_statement
    : KW_IF '(' expression ')' statement                         /* Rule 108 — createIf */
        { $$ = createIf(ctx, $3, $5); }
    | KW_IF '(' expression ')' statement KW_ELSE statement      /* Rule 109 — createIfElse */
        { $$ = createIfElse(ctx, $3, $5, $7); }
    | KW_SWITCH '(' expression ')' statement    /* Rule 110 — createSwitch */
        { $$ = createSwitch(ctx, $3, $5); }
    ;

/* ========================================================================
 * Rules 111-117: iteration_statement
 * ======================================================================== */
iteration_statement
    : KW_WHILE '(' expression ')' statement     /* Rule 111 — createWhile */
        { $$ = createWhile(ctx, $3, $5); }
    | KW_DO statement KW_WHILE '(' expression ')' ';'  /* Rule 112 — createDoWhile */
        { $$ = createDoWhile(ctx, $2, $5); }
    | KW_FOR '(' expression_statement expression_statement ')' statement     /* Rule 113 — for(init;cond;)body */
        { $$ = createFor(ctx, $3, $4, NULL, $6); }
    | KW_FOR '(' declaration expression_statement ')' statement            /* Rule 114 — for(decl cond;)body */
        { $$ = createFor(ctx, $3, $4, NULL, $6); }
    | KW_FOR '(' expression_statement expression_statement expression ')' statement  /* Rule 115 — for(init;cond;incr)body */
        { $$ = createFor(ctx, $3, $4, $5, $7); }
    | KW_FOR '(' declaration expression_statement expression ')' statement  /* Rule 116 — for(decl cond;incr)body */
        { $$ = createFor(ctx, $3, $4, $5, $7); }
    | KW_REPEAT '(' expression ')' statement    /* Rule 117 — createRepeat */
        { $$ = createRepeat(ctx, $3, $5); }
    ;

/* ========================================================================
 * Rules 118-121: jump_statement
 * ======================================================================== */
jump_statement
    : KW_CONTINUE ';'                           /* Rule 118 */
        { $$ = createCommand(ctx, ECommandType::eCONTINUE, 0); }
    | KW_BREAK ';'                              /* Rule 119 */
        { $$ = createCommand(ctx, ECommandType::eBREAK, 0); }
    | KW_RETURN ';'                             /* Rule 120 — return void */
        { $$ = createCommand(ctx, ECommandType::eRETURN, 0); }
    | KW_RETURN expression ';'                  /* Rule 121 — return expr */
        { $$ = createCommand(ctx, ECommandType::eRETURN, 1, $2); }
    ;

/* ========================================================================
 * Rules 122-123: translation_unit (start symbol)
 * ======================================================================== */
translation_unit
    : /* empty */                               /* Rule 122 */
        { *result = NULL; }
    | external_declaration_list                  /* Rule 123 */
        { *result = $1; }
    ;

/* ========================================================================
 * Rules 124-125: external_declaration_list
 * ======================================================================== */
external_declaration_list
    : external_declaration                      /* Rule 124 */
        { $$ = $1; }
    | external_declaration_list external_declaration  /* Rule 125 */
        { $$ = createOrAppendStmtList(ctx, $1, $2); }
    ;

/* ========================================================================
 * Rules 126-128: external_declaration
 * ======================================================================== */
external_declaration
    : declaration                               /* Rule 126 */
        { $$ = $1; }
    | statement                                 /* Rule 127 */
        { $$ = $1; }
    | function_definition                       /* Rule 128 */
        { $$ = $1; }
    ;

/* ========================================================================
 * Rule 129: function_definition (RHS=3)
 *   Semantic action @0x2cad59: createFunction(ctx, $1, $2, $3)
 * ======================================================================== */
function_definition
    : type_specifier function_declarator compound_statement  /* Rule 129 */
        { $$ = createFunction(ctx, $1, $2, $3); }
    ;

%%

/* Error handler — seqc_error @0x2ca1b0 */
int seqc_error(zhinst::SeqcParserContext* ctx,
               zhinst::Expression** result,
               void* scanner,
               const char* msg) {
    (void)result;
    (void)scanner;
    ctx->raiseError(std::string(msg));
    ctx->setSyntaxError();
    return 1;
}
