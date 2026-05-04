/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_SEQC_SEQC_PARSER_TAB_H_INCLUDED
# define YY_SEQC_SEQC_PARSER_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef SEQC_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define SEQC_DEBUG 1
#  else
#   define SEQC_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define SEQC_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined SEQC_DEBUG */
#if SEQC_DEBUG
extern int seqc_debug;
#endif
/* "%code requires" blocks.  */
#line 57 "seqc_parser.y"

    namespace zhinst {
        class SeqcParserContext;
        struct Expression;
    }

#line 64 "seqc_parser.tab.h"

/* Token kinds.  */
#ifndef SEQC_TOKENTYPE
# define SEQC_TOKENTYPE
  enum seqc_tokentype
  {
    SEQC_EMPTY = -2,
    SEQC_EOF = 0,                  /* "end of file"  */
    SEQC_error = 256,              /* error  */
    SEQC_UNDEF = 257,              /* "invalid token"  */
    CONSTANT = 258,                /* CONSTANT  */
    IDENTIFIER = 259,              /* IDENTIFIER  */
    STRING_LITERAL = 260,          /* STRING_LITERAL  */
    INC_OP = 261,                  /* INC_OP  */
    DEC_OP = 262,                  /* DEC_OP  */
    LSH_OP = 263,                  /* LSH_OP  */
    RSH_OP = 264,                  /* RSH_OP  */
    LE_OP = 265,                   /* LE_OP  */
    GE_OP = 266,                   /* GE_OP  */
    EQ_OP = 267,                   /* EQ_OP  */
    NE_OP = 268,                   /* NE_OP  */
    AND_OP = 269,                  /* AND_OP  */
    OR_OP = 270,                   /* OR_OP  */
    MUL_ASSIGN = 271,              /* MUL_ASSIGN  */
    DIV_ASSIGN = 272,              /* DIV_ASSIGN  */
    MOD_ASSIGN = 273,              /* MOD_ASSIGN  */
    ADD_ASSIGN = 274,              /* ADD_ASSIGN  */
    SUB_ASSIGN = 275,              /* SUB_ASSIGN  */
    LSH_ASSIGN = 276,              /* LSH_ASSIGN  */
    RSH_ASSIGN = 277,              /* RSH_ASSIGN  */
    AND_ASSIGN = 278,              /* AND_ASSIGN  */
    XOR_ASSIGN = 279,              /* XOR_ASSIGN  */
    OR_ASSIGN = 280,               /* OR_ASSIGN  */
    KW_CONST = 281,                /* KW_CONST  */
    KW_CVAR = 282,                 /* KW_CVAR  */
    KW_STRING = 283,               /* KW_STRING  */
    KW_VAR = 284,                  /* KW_VAR  */
    KW_VOID = 285,                 /* KW_VOID  */
    KW_WAVE = 286,                 /* KW_WAVE  */
    KW_CASE = 287,                 /* KW_CASE  */
    KW_DEFAULT = 288,              /* KW_DEFAULT  */
    KW_IF = 289,                   /* KW_IF  */
    KW_ELSE = 290,                 /* KW_ELSE  */
    KW_SWITCH = 291,               /* KW_SWITCH  */
    KW_WHILE = 292,                /* KW_WHILE  */
    KW_DO = 293,                   /* KW_DO  */
    KW_FOR = 294,                  /* KW_FOR  */
    KW_CONTINUE = 295,             /* KW_CONTINUE  */
    KW_BREAK = 296,                /* KW_BREAK  */
    KW_RETURN = 297,               /* KW_RETURN  */
    KW_REPEAT = 298                /* KW_REPEAT  */
  };
  typedef enum seqc_tokentype seqc_token_kind_t;
#endif

/* Value type.  */
#if ! defined SEQC_STYPE && ! defined SEQC_STYPE_IS_DECLARED
union SEQC_STYPE
{
#line 70 "seqc_parser.y"

    double                      dval;   /* CONSTANT */
    const char*                 sval;   /* IDENTIFIER, STRING_LITERAL */
    zhinst::Expression*         expr;   /* all nonterminals */

#line 130 "seqc_parser.tab.h"

};
typedef union SEQC_STYPE SEQC_STYPE;
# define SEQC_STYPE_IS_TRIVIAL 1
# define SEQC_STYPE_IS_DECLARED 1
#endif




int seqc_parse (zhinst::SeqcParserContext* ctx, zhinst::Expression** result, void* scanner);


#endif /* !YY_SEQC_SEQC_PARSER_TAB_H_INCLUDED  */
