/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         SEQC_STYPE
/* Substitute the variable and function names.  */
#define yyparse         seqc_parse
#define yylex           seqc_lex
#define yyerror         seqc_error
#define yydebug         seqc_debug
#define yynerrs         seqc_nerrs

/* First part of user prologue.  */
#line 31 "seqc_parser.y"

#include "zhinst/expression.hpp"
#include "zhinst/seqc_parser_context.hpp"
#include "zhinst/resources.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

using namespace zhinst;

#line 89 "seqc_parser.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "seqc_parser.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_CONSTANT = 3,                   /* CONSTANT  */
  YYSYMBOL_IDENTIFIER = 4,                 /* IDENTIFIER  */
  YYSYMBOL_STRING_LITERAL = 5,             /* STRING_LITERAL  */
  YYSYMBOL_INC_OP = 6,                     /* INC_OP  */
  YYSYMBOL_DEC_OP = 7,                     /* DEC_OP  */
  YYSYMBOL_LSH_OP = 8,                     /* LSH_OP  */
  YYSYMBOL_RSH_OP = 9,                     /* RSH_OP  */
  YYSYMBOL_LE_OP = 10,                     /* LE_OP  */
  YYSYMBOL_GE_OP = 11,                     /* GE_OP  */
  YYSYMBOL_EQ_OP = 12,                     /* EQ_OP  */
  YYSYMBOL_NE_OP = 13,                     /* NE_OP  */
  YYSYMBOL_AND_OP = 14,                    /* AND_OP  */
  YYSYMBOL_OR_OP = 15,                     /* OR_OP  */
  YYSYMBOL_MUL_ASSIGN = 16,                /* MUL_ASSIGN  */
  YYSYMBOL_DIV_ASSIGN = 17,                /* DIV_ASSIGN  */
  YYSYMBOL_MOD_ASSIGN = 18,                /* MOD_ASSIGN  */
  YYSYMBOL_ADD_ASSIGN = 19,                /* ADD_ASSIGN  */
  YYSYMBOL_SUB_ASSIGN = 20,                /* SUB_ASSIGN  */
  YYSYMBOL_LSH_ASSIGN = 21,                /* LSH_ASSIGN  */
  YYSYMBOL_RSH_ASSIGN = 22,                /* RSH_ASSIGN  */
  YYSYMBOL_AND_ASSIGN = 23,                /* AND_ASSIGN  */
  YYSYMBOL_XOR_ASSIGN = 24,                /* XOR_ASSIGN  */
  YYSYMBOL_OR_ASSIGN = 25,                 /* OR_ASSIGN  */
  YYSYMBOL_KW_CONST = 26,                  /* KW_CONST  */
  YYSYMBOL_KW_CVAR = 27,                   /* KW_CVAR  */
  YYSYMBOL_KW_STRING = 28,                 /* KW_STRING  */
  YYSYMBOL_KW_VAR = 29,                    /* KW_VAR  */
  YYSYMBOL_KW_VOID = 30,                   /* KW_VOID  */
  YYSYMBOL_KW_WAVE = 31,                   /* KW_WAVE  */
  YYSYMBOL_KW_CASE = 32,                   /* KW_CASE  */
  YYSYMBOL_KW_DEFAULT = 33,                /* KW_DEFAULT  */
  YYSYMBOL_KW_IF = 34,                     /* KW_IF  */
  YYSYMBOL_KW_ELSE = 35,                   /* KW_ELSE  */
  YYSYMBOL_KW_SWITCH = 36,                 /* KW_SWITCH  */
  YYSYMBOL_KW_WHILE = 37,                  /* KW_WHILE  */
  YYSYMBOL_KW_DO = 38,                     /* KW_DO  */
  YYSYMBOL_KW_FOR = 39,                    /* KW_FOR  */
  YYSYMBOL_KW_CONTINUE = 40,               /* KW_CONTINUE  */
  YYSYMBOL_KW_BREAK = 41,                  /* KW_BREAK  */
  YYSYMBOL_KW_RETURN = 42,                 /* KW_RETURN  */
  YYSYMBOL_KW_REPEAT = 43,                 /* KW_REPEAT  */
  YYSYMBOL_44_ = 44,                       /* '('  */
  YYSYMBOL_45_ = 45,                       /* ')'  */
  YYSYMBOL_46_ = 46,                       /* '['  */
  YYSYMBOL_47_ = 47,                       /* ']'  */
  YYSYMBOL_48_ = 48,                       /* ','  */
  YYSYMBOL_49_ = 49,                       /* '-'  */
  YYSYMBOL_50_ = 50,                       /* '+'  */
  YYSYMBOL_51_ = 51,                       /* '~'  */
  YYSYMBOL_52_ = 52,                       /* '!'  */
  YYSYMBOL_53_ = 53,                       /* '*'  */
  YYSYMBOL_54_ = 54,                       /* '/'  */
  YYSYMBOL_55_ = 55,                       /* '%'  */
  YYSYMBOL_56_ = 56,                       /* '<'  */
  YYSYMBOL_57_ = 57,                       /* '>'  */
  YYSYMBOL_58_ = 58,                       /* '&'  */
  YYSYMBOL_59_ = 59,                       /* '^'  */
  YYSYMBOL_60_ = 60,                       /* '|'  */
  YYSYMBOL_61_ = 61,                       /* '?'  */
  YYSYMBOL_62_ = 62,                       /* ':'  */
  YYSYMBOL_63_ = 63,                       /* '='  */
  YYSYMBOL_64_ = 64,                       /* ';'  */
  YYSYMBOL_65_ = 65,                       /* '{'  */
  YYSYMBOL_66_ = 66,                       /* '}'  */
  YYSYMBOL_YYACCEPT = 67,                  /* $accept  */
  YYSYMBOL_primary_expression = 68,        /* primary_expression  */
  YYSYMBOL_postfix_expression = 69,        /* postfix_expression  */
  YYSYMBOL_argument_expression_list = 70,  /* argument_expression_list  */
  YYSYMBOL_unary_expression = 71,          /* unary_expression  */
  YYSYMBOL_multiplicative_expression = 72, /* multiplicative_expression  */
  YYSYMBOL_additive_expression = 73,       /* additive_expression  */
  YYSYMBOL_shift_expression = 74,          /* shift_expression  */
  YYSYMBOL_relational_expression = 75,     /* relational_expression  */
  YYSYMBOL_equality_expression = 76,       /* equality_expression  */
  YYSYMBOL_and_expression = 77,            /* and_expression  */
  YYSYMBOL_exclusive_or_expression = 78,   /* exclusive_or_expression  */
  YYSYMBOL_inclusive_or_expression = 79,   /* inclusive_or_expression  */
  YYSYMBOL_logical_and_expression = 80,    /* logical_and_expression  */
  YYSYMBOL_logical_or_expression = 81,     /* logical_or_expression  */
  YYSYMBOL_conditional_expression = 82,    /* conditional_expression  */
  YYSYMBOL_assignment_expression = 83,     /* assignment_expression  */
  YYSYMBOL_expression = 84,                /* expression  */
  YYSYMBOL_constant_expression = 85,       /* constant_expression  */
  YYSYMBOL_declaration = 86,               /* declaration  */
  YYSYMBOL_init_declarator_list = 87,      /* init_declarator_list  */
  YYSYMBOL_init_declarator = 88,           /* init_declarator  */
  YYSYMBOL_type_specifier = 89,            /* type_specifier  */
  YYSYMBOL_declarator = 90,                /* declarator  */
  YYSYMBOL_function_declarator = 91,       /* function_declarator  */
  YYSYMBOL_parameter_list = 92,            /* parameter_list  */
  YYSYMBOL_parameter_declaration = 93,     /* parameter_declaration  */
  YYSYMBOL_identifier = 94,                /* identifier  */
  YYSYMBOL_initializer = 95,               /* initializer  */
  YYSYMBOL_statement = 96,                 /* statement  */
  YYSYMBOL_unlabeled_statement = 97,       /* unlabeled_statement  */
  YYSYMBOL_labeled_statement = 98,         /* labeled_statement  */
  YYSYMBOL_compound_statement = 99,        /* compound_statement  */
  YYSYMBOL_compound_body = 100,            /* compound_body  */
  YYSYMBOL_expression_statement = 101,     /* expression_statement  */
  YYSYMBOL_selection_statement = 102,      /* selection_statement  */
  YYSYMBOL_iteration_statement = 103,      /* iteration_statement  */
  YYSYMBOL_jump_statement = 104,           /* jump_statement  */
  YYSYMBOL_translation_unit = 105,         /* translation_unit  */
  YYSYMBOL_external_declaration_list = 106, /* external_declaration_list  */
  YYSYMBOL_external_declaration = 107,     /* external_declaration  */
  YYSYMBOL_function_definition = 108       /* function_definition  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;



/* Unqualified %code blocks.  */
#line 64 "seqc_parser.y"

    int seqc_lex(SEQC_STYPE* lvalp, void* scanner);

#line 236 "seqc_parser.tab.c"

#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined SEQC_STYPE_IS_TRIVIAL && SEQC_STYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  127
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   525

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  67
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  42
/* YYNRULES -- Number of rules.  */
#define YYNRULES  127
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  232

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   298


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    52,     2,     2,     2,    55,    58,     2,
      44,    45,    53,    50,    48,    49,     2,    54,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    62,    64,
      56,    63,    57,    61,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    46,     2,    47,    59,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    65,    60,    66,    51,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43
};

#if SEQC_DEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   114,   114,   116,   118,   120,   128,   130,   132,   134,
     136,   138,   146,   148,   156,   158,   160,   162,   164,   166,
     168,   176,   178,   180,   182,   190,   192,   194,   202,   204,
     206,   214,   216,   218,   220,   222,   230,   232,   234,   242,
     244,   252,   254,   262,   264,   272,   274,   282,   284,   292,
     294,   302,   304,   315,   321,   327,   333,   339,   345,   351,
     357,   363,   369,   381,   389,   399,   407,   409,   417,   419,
     431,   433,   435,   437,   439,   441,   449,   451,   453,   461,
     463,   471,   473,   481,   483,   491,   499,   507,   509,   517,
     519,   521,   523,   525,   533,   535,   537,   539,   547,   549,
     557,   559,   561,   563,   581,   583,   591,   593,   595,   603,
     605,   607,   609,   611,   613,   615,   623,   625,   627,   629,
     638,   639,   647,   649,   657,   659,   661,   670
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if SEQC_DEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "CONSTANT",
  "IDENTIFIER", "STRING_LITERAL", "INC_OP", "DEC_OP", "LSH_OP", "RSH_OP",
  "LE_OP", "GE_OP", "EQ_OP", "NE_OP", "AND_OP", "OR_OP", "MUL_ASSIGN",
  "DIV_ASSIGN", "MOD_ASSIGN", "ADD_ASSIGN", "SUB_ASSIGN", "LSH_ASSIGN",
  "RSH_ASSIGN", "AND_ASSIGN", "XOR_ASSIGN", "OR_ASSIGN", "KW_CONST",
  "KW_CVAR", "KW_STRING", "KW_VAR", "KW_VOID", "KW_WAVE", "KW_CASE",
  "KW_DEFAULT", "KW_IF", "KW_ELSE", "KW_SWITCH", "KW_WHILE", "KW_DO",
  "KW_FOR", "KW_CONTINUE", "KW_BREAK", "KW_RETURN", "KW_REPEAT", "'('",
  "')'", "'['", "']'", "','", "'-'", "'+'", "'~'", "'!'", "'*'", "'/'",
  "'%'", "'<'", "'>'", "'&'", "'^'", "'|'", "'?'", "':'", "'='", "';'",
  "'{'", "'}'", "$accept", "primary_expression", "postfix_expression",
  "argument_expression_list", "unary_expression",
  "multiplicative_expression", "additive_expression", "shift_expression",
  "relational_expression", "equality_expression", "and_expression",
  "exclusive_or_expression", "inclusive_or_expression",
  "logical_and_expression", "logical_or_expression",
  "conditional_expression", "assignment_expression", "expression",
  "constant_expression", "declaration", "init_declarator_list",
  "init_declarator", "type_specifier", "declarator", "function_declarator",
  "parameter_list", "parameter_declaration", "identifier", "initializer",
  "statement", "unlabeled_statement", "labeled_statement",
  "compound_statement", "compound_body", "expression_statement",
  "selection_statement", "iteration_statement", "jump_statement",
  "translation_unit", "external_declaration_list", "external_declaration",
  "function_definition", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-174)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     296,  -174,  -174,  -174,   458,   458,  -174,  -174,  -174,  -174,
    -174,  -174,   458,   -48,   -19,   -14,    -9,   346,     7,   -20,
      14,   100,    37,   458,   458,   458,   458,   458,  -174,   196,
    -174,    -1,   462,   -21,   -27,    39,    18,    64,    28,    30,
      27,    77,   -12,  -174,  -174,    31,  -174,    92,  -174,  -174,
    -174,  -174,  -174,  -174,  -174,  -174,  -174,    97,   296,  -174,
    -174,  -174,  -174,  -174,  -174,    40,   346,   458,   458,   458,
      71,   396,  -174,  -174,  -174,    45,   458,    65,  -174,  -174,
    -174,  -174,  -174,  -174,    92,  -174,   246,  -174,  -174,    49,
     458,   458,   458,   458,   458,   458,   458,   458,   458,   458,
     458,   458,   458,   458,   458,   458,   458,   458,   458,   458,
     458,   458,   458,   458,   458,   458,   458,   458,   458,   458,
     458,  -174,   -38,  -174,    48,    47,    -4,  -174,  -174,   346,
    -174,    87,    88,    89,    96,   313,   313,  -174,    98,  -174,
      95,  -174,  -174,  -174,  -174,   -37,  -174,    99,  -174,  -174,
    -174,  -174,  -174,  -174,  -174,  -174,  -174,  -174,  -174,  -174,
    -174,  -174,   -21,   -21,   -27,   -27,    39,    39,    39,    39,
      18,    18,    64,    28,    30,    27,    77,    80,    92,  -174,
     458,  -174,   186,   365,  -174,   346,   346,   346,   458,   425,
     446,   346,  -174,   458,  -174,   458,  -174,  -174,  -174,  -174,
      92,  -174,   -33,  -174,  -174,   106,   112,  -174,  -174,   109,
     346,   110,   346,   111,  -174,  -174,  -174,  -174,  -174,   237,
    -174,   346,    94,  -174,   346,  -174,   346,  -174,  -174,  -174,
    -174,  -174
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
     120,     3,    85,     4,     0,     0,    71,    72,    75,    70,
      73,    74,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   104,     0,
       6,    14,    21,    25,    28,    31,    36,    39,    41,    43,
      45,    47,    49,    51,    63,     0,   124,     0,     2,   125,
      87,    88,    89,    90,    91,    92,    93,     0,   121,   122,
     126,    15,    16,    21,    64,     0,    97,     0,     0,     0,
       0,     0,   116,   117,   118,     0,     0,     0,    17,    18,
      19,    20,    98,   100,     0,   101,     0,    10,    11,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   105,     0,    66,    68,     0,    76,     1,   123,    96,
      95,     0,     0,     0,     0,     0,     0,   119,     0,     5,
      76,    99,   102,   103,     8,     0,    12,     0,    55,    56,
      57,    53,    54,    61,    62,    58,    59,    60,    52,    22,
      23,    24,    27,    26,    29,    30,    34,    35,    32,    33,
      37,    38,    40,    42,    44,    46,    48,     0,     0,    65,
       0,   127,     0,     0,    94,     0,     0,     0,     0,     0,
       0,     0,     9,     0,     7,     0,    67,    86,    69,    80,
       0,    84,     0,    81,    78,     0,   106,   108,   109,     0,
       0,     0,     0,     0,   115,    13,    50,    83,    79,     0,
      77,     0,     0,   112,     0,   111,     0,    82,   107,   110,
     114,   113
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -174,  -174,  -174,  -174,    12,   -26,   -25,   -91,   -29,    50,
      44,    46,    54,    43,  -174,   -10,   -32,    69,   -17,     2,
    -174,   -11,     0,  -173,  -174,  -174,   -51,   -43,  -174,   -16,
    -174,  -174,    52,  -174,   -64,  -174,  -174,  -174,  -174,  -174,
     116,  -174
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,    30,    31,   145,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    65,    46,
     122,   123,    84,   124,   125,   202,   203,    48,   198,    49,
      50,    51,    52,    86,    53,    54,    55,    56,    57,    58,
      59,    60
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      47,    70,    64,   119,   126,    87,    88,   136,   192,   201,
     178,   193,   218,    85,    66,   219,    61,    62,   166,   167,
     168,   169,   105,   106,    63,    67,   179,   217,   109,   110,
      68,    83,   102,   103,   104,    69,    78,    79,    80,    81,
     182,   140,   183,    89,    72,    90,   201,   107,   108,   120,
     130,    71,     1,     2,     3,     4,     5,   146,    47,   148,
     149,   150,   151,   152,   153,   154,   155,   156,   157,   158,
     143,   189,   190,   135,   111,   112,   113,   114,    73,   162,
     163,    76,   164,   165,   170,   171,   115,   117,   142,   116,
      75,   118,    77,    23,   144,   121,     2,   127,    24,    25,
      26,    27,   129,     1,     2,     3,     4,     5,   134,   137,
     139,   180,    29,   184,   159,   160,   161,    63,    63,    63,
      63,    63,    63,    63,    63,    63,    63,    63,    63,    63,
      63,    63,   185,   186,   187,   140,   131,   132,   133,   140,
     188,   183,   195,   191,    23,   138,   194,   221,   197,    24,
      25,    26,    27,   220,   222,   224,   226,   140,   229,   147,
     173,   215,   176,   174,    74,   172,   205,   196,   227,   206,
     207,   208,   175,    64,   128,   214,   140,   181,     0,     0,
       0,     0,   200,     0,     0,   216,     0,     0,     0,   177,
       2,     0,     0,     0,   223,    63,   225,     0,     0,     1,
       2,     3,     4,     5,     0,   228,     0,    63,   230,     0,
     231,     0,     6,     7,     8,     9,    10,    11,     0,   200,
       0,     0,     6,     7,     8,     9,    10,    11,    12,    13,
      14,   199,    15,    16,    17,    18,    19,    20,    21,    22,
      23,     2,     0,     0,     0,    24,    25,    26,    27,     1,
       2,     3,     4,     5,     0,     0,     0,   209,   211,   213,
      28,    29,    82,     6,     7,     8,     9,    10,    11,     0,
       0,     0,     6,     7,     8,     9,    10,    11,    12,    13,
      14,     0,    15,    16,    17,    18,    19,    20,    21,    22,
      23,     0,     0,     0,     0,    24,    25,    26,    27,     1,
       2,     3,     4,     5,     0,     0,     0,     0,     0,     0,
      28,    29,   141,     0,     0,     0,     1,     2,     3,     4,
       5,     0,     6,     7,     8,     9,    10,    11,    12,    13,
      14,     0,    15,    16,    17,    18,    19,    20,    21,    22,
      23,     0,     0,     0,     0,    24,    25,    26,    27,     1,
       2,     3,     4,     5,     0,     0,     0,    23,     0,     0,
      28,    29,    24,    25,    26,    27,     0,     0,     1,     2,
       3,     4,     5,     0,     0,     0,     0,    28,    12,    13,
      14,     0,    15,    16,    17,    18,    19,    20,    21,    22,
      23,     0,     0,     0,     0,    24,    25,    26,    27,     1,
       2,     3,     4,     5,     0,     0,     0,     0,     0,    23,
      28,    29,   204,     0,    24,    25,    26,    27,     0,     0,
       0,     0,     6,     7,     8,     9,    10,    11,     1,     2,
       3,     4,     5,     0,     0,     0,     0,     0,     0,     0,
      23,     0,     0,     0,     0,    24,    25,    26,    27,     1,
       2,     3,     4,     5,     0,     0,     0,     0,     0,     0,
      28,     1,     2,     3,     4,     5,     0,     0,     0,    23,
     210,     0,     0,     0,    24,    25,    26,    27,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,     0,     0,
      23,   212,     0,     0,     0,    24,    25,    26,    27,     0,
       0,     0,    23,     0,     0,     0,     0,    24,    25,    26,
      27,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   101
};

static const yytype_int16 yycheck[] =
{
       0,    17,    12,    15,    47,     6,     7,    71,    45,   182,
      48,    48,    45,    29,    62,    48,     4,     5,   109,   110,
     111,   112,    49,    50,    12,    44,    64,   200,    10,    11,
      44,    29,    53,    54,    55,    44,    24,    25,    26,    27,
      44,    84,    46,    44,    64,    46,   219,     8,     9,    61,
      66,    44,     3,     4,     5,     6,     7,    89,    58,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
      86,   135,   136,    71,    56,    57,    12,    13,    64,   105,
     106,    44,   107,   108,   113,   114,    58,    60,    86,    59,
      21,    14,    23,    44,    45,    64,     4,     0,    49,    50,
      51,    52,    62,     3,     4,     5,     6,     7,    37,    64,
      45,    63,    65,   129,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    45,    45,    45,   178,    67,    68,    69,   182,
      44,    46,    62,    45,    44,    76,    47,    35,   180,    49,
      50,    51,    52,    47,    45,    45,    45,   200,    64,    90,
     116,   193,   119,   117,    64,   115,   183,   178,   219,   185,
     186,   187,   118,   183,    58,   191,   219,   125,    -1,    -1,
      -1,    -1,   182,    -1,    -1,   195,    -1,    -1,    -1,   120,
       4,    -1,    -1,    -1,   210,   183,   212,    -1,    -1,     3,
       4,     5,     6,     7,    -1,   221,    -1,   195,   224,    -1,
     226,    -1,    26,    27,    28,    29,    30,    31,    -1,   219,
      -1,    -1,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    45,    36,    37,    38,    39,    40,    41,    42,    43,
      44,     4,    -1,    -1,    -1,    49,    50,    51,    52,     3,
       4,     5,     6,     7,    -1,    -1,    -1,   188,   189,   190,
      64,    65,    66,    26,    27,    28,    29,    30,    31,    -1,
      -1,    -1,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    -1,    -1,    -1,    -1,    49,    50,    51,    52,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,
      64,    65,    66,    -1,    -1,    -1,     3,     4,     5,     6,
       7,    -1,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    -1,    -1,    -1,    -1,    49,    50,    51,    52,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    44,    -1,    -1,
      64,    65,    49,    50,    51,    52,    -1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    -1,    -1,    64,    32,    33,
      34,    -1,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    -1,    -1,    -1,    -1,    49,    50,    51,    52,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,    44,
      64,    65,    47,    -1,    49,    50,    51,    52,    -1,    -1,
      -1,    -1,    26,    27,    28,    29,    30,    31,     3,     4,
       5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      44,    -1,    -1,    -1,    -1,    49,    50,    51,    52,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,
      64,     3,     4,     5,     6,     7,    -1,    -1,    -1,    44,
      45,    -1,    -1,    -1,    49,    50,    51,    52,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    -1,    -1,
      44,    45,    -1,    -1,    -1,    49,    50,    51,    52,    -1,
      -1,    -1,    44,    -1,    -1,    -1,    -1,    49,    50,    51,
      52,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    63
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,     4,     5,     6,     7,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    49,    50,    51,    52,    64,    65,
      68,    69,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    86,    89,    94,    96,
      97,    98,    99,   101,   102,   103,   104,   105,   106,   107,
     108,    71,    71,    71,    82,    85,    62,    44,    44,    44,
      96,    44,    64,    64,    64,    84,    44,    84,    71,    71,
      71,    71,    66,    86,    89,    96,   100,     6,     7,    44,
      46,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    63,    53,    54,    55,    49,    50,     8,     9,    10,
      11,    56,    57,    12,    13,    58,    59,    60,    14,    15,
      61,    64,    87,    88,    90,    91,    94,     0,   107,    62,
      96,    84,    84,    84,    37,    86,   101,    64,    84,    45,
      94,    66,    86,    96,    45,    70,    83,    84,    83,    83,
      83,    83,    83,    83,    83,    83,    83,    83,    83,    71,
      71,    71,    72,    72,    73,    73,    74,    74,    74,    74,
      75,    75,    76,    77,    78,    79,    80,    84,    48,    64,
      63,    99,    44,    46,    96,    45,    45,    45,    44,   101,
     101,    45,    45,    48,    47,    62,    88,    83,    95,    45,
      89,    90,    92,    93,    47,    85,    96,    96,    96,    84,
      45,    84,    45,    84,    96,    83,    82,    90,    45,    48,
      47,    35,    45,    96,    45,    96,    45,    93,    96,    64,
      96,    96
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    67,    68,    68,    68,    68,    69,    69,    69,    69,
      69,    69,    70,    70,    71,    71,    71,    71,    71,    71,
      71,    72,    72,    72,    72,    73,    73,    73,    74,    74,
      74,    75,    75,    75,    75,    75,    76,    76,    76,    77,
      77,    78,    78,    79,    79,    80,    80,    81,    81,    82,
      82,    83,    83,    83,    83,    83,    83,    83,    83,    83,
      83,    83,    83,    84,    85,    86,    87,    87,    88,    88,
      89,    89,    89,    89,    89,    89,    90,    90,    90,    91,
      91,    92,    92,    93,    93,    94,    95,    96,    96,    97,
      97,    97,    97,    97,    98,    98,    98,    98,    99,    99,
     100,   100,   100,   100,   101,   101,   102,   102,   102,   103,
     103,   103,   103,   103,   103,   103,   104,   104,   104,   104,
     105,   105,   106,   106,   107,   107,   107,   108
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     1,     1,     3,     1,     4,     3,     4,
       2,     2,     1,     3,     1,     2,     2,     2,     2,     2,
       2,     1,     3,     3,     3,     1,     3,     3,     1,     3,
       3,     1,     3,     3,     3,     3,     1,     3,     3,     1,
       3,     1,     3,     1,     3,     1,     3,     1,     3,     1,
       5,     1,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     1,     1,     3,     1,     3,     1,     3,
       1,     1,     1,     1,     1,     1,     1,     4,     3,     4,
       3,     1,     3,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     4,     3,     3,     2,     2,     3,
       1,     1,     2,     2,     1,     2,     5,     7,     5,     5,
       7,     6,     6,     7,     7,     5,     2,     2,     2,     3,
       0,     1,     1,     2,     1,     1,     1,     3
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = SEQC_EMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == SEQC_EMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (ctx, result, scanner, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use SEQC_error or SEQC_UNDEF. */
#define YYERRCODE SEQC_UNDEF


/* Enable debugging if requested.  */
#if SEQC_DEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, ctx, result, scanner); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, zhinst::SeqcParserContext* ctx, zhinst::Expression** result, void* scanner)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (ctx);
  YY_USE (result);
  YY_USE (scanner);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, zhinst::SeqcParserContext* ctx, zhinst::Expression** result, void* scanner)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep, ctx, result, scanner);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule, zhinst::SeqcParserContext* ctx, zhinst::Expression** result, void* scanner)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)], ctx, result, scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, ctx, result, scanner); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !SEQC_DEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !SEQC_DEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, zhinst::SeqcParserContext* ctx, zhinst::Expression** result, void* scanner)
{
  YY_USE (yyvaluep);
  YY_USE (ctx);
  YY_USE (result);
  YY_USE (scanner);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (zhinst::SeqcParserContext* ctx, zhinst::Expression** result, void* scanner)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = SEQC_EMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == SEQC_EMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, scanner);
    }

  if (yychar <= SEQC_EOF)
    {
      yychar = SEQC_EOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == SEQC_error)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = SEQC_UNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = SEQC_EMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* primary_expression: identifier  */
#line 115 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1432 "seqc_parser.tab.c"
    break;

  case 3: /* primary_expression: CONSTANT  */
#line 117 "seqc_parser.y"
        { (yyval.expr) = createValue(ctx, (yyvsp[0].dval)); }
#line 1438 "seqc_parser.tab.c"
    break;

  case 4: /* primary_expression: STRING_LITERAL  */
#line 119 "seqc_parser.y"
        { (yyval.expr) = createString(ctx, (yyvsp[0].sval)); }
#line 1444 "seqc_parser.tab.c"
    break;

  case 5: /* primary_expression: '(' expression ')'  */
#line 121 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[-1].expr); }
#line 1450 "seqc_parser.tab.c"
    break;

  case 6: /* postfix_expression: primary_expression  */
#line 129 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1456 "seqc_parser.tab.c"
    break;

  case 7: /* postfix_expression: postfix_expression '[' expression ']'  */
#line 131 "seqc_parser.y"
        { (yyval.expr) = createArray(ctx, (yyvsp[-3].expr), (yyvsp[-1].expr)); }
#line 1462 "seqc_parser.tab.c"
    break;

  case 8: /* postfix_expression: postfix_expression '(' ')'  */
#line 133 "seqc_parser.y"
        { (yyval.expr) = createFunctionCall(ctx, (yyvsp[-2].expr), NULL); }
#line 1468 "seqc_parser.tab.c"
    break;

  case 9: /* postfix_expression: postfix_expression '(' argument_expression_list ')'  */
#line 135 "seqc_parser.y"
        { (yyval.expr) = createFunctionCall(ctx, (yyvsp[-3].expr), (yyvsp[-1].expr)); }
#line 1474 "seqc_parser.tab.c"
    break;

  case 10: /* postfix_expression: postfix_expression INC_OP  */
#line 137 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-1].expr), NULL, EOperator::eINC); }
#line 1480 "seqc_parser.tab.c"
    break;

  case 11: /* postfix_expression: postfix_expression DEC_OP  */
#line 139 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-1].expr), NULL, EOperator::eDEC); }
#line 1486 "seqc_parser.tab.c"
    break;

  case 12: /* argument_expression_list: assignment_expression  */
#line 147 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1492 "seqc_parser.tab.c"
    break;

  case 13: /* argument_expression_list: argument_expression_list ',' assignment_expression  */
#line 149 "seqc_parser.y"
        { (yyval.expr) = createOrAppendArgList(ctx, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1498 "seqc_parser.tab.c"
    break;

  case 14: /* unary_expression: postfix_expression  */
#line 157 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1504 "seqc_parser.tab.c"
    break;

  case 15: /* unary_expression: INC_OP unary_expression  */
#line 159 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, NULL, (yyvsp[0].expr), EOperator::eINC); }
#line 1510 "seqc_parser.tab.c"
    break;

  case 16: /* unary_expression: DEC_OP unary_expression  */
#line 161 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, NULL, (yyvsp[0].expr), EOperator::eDEC); }
#line 1516 "seqc_parser.tab.c"
    break;

  case 17: /* unary_expression: '-' unary_expression  */
#line 163 "seqc_parser.y"
        { (yyval.expr) = createCommand(ctx, ECommandType::eNEG, 1, (yyvsp[0].expr)); }
#line 1522 "seqc_parser.tab.c"
    break;

  case 18: /* unary_expression: '+' unary_expression  */
#line 165 "seqc_parser.y"
        { (yyval.expr) = createCommand(ctx, ECommandType::ePOS, 1, (yyvsp[0].expr)); }
#line 1528 "seqc_parser.tab.c"
    break;

  case 19: /* unary_expression: '~' unary_expression  */
#line 167 "seqc_parser.y"
        { (yyval.expr) = createCommand(ctx, ECommandType::eINV, 1, (yyvsp[0].expr)); }
#line 1534 "seqc_parser.tab.c"
    break;

  case 20: /* unary_expression: '!' unary_expression  */
#line 169 "seqc_parser.y"
        { (yyval.expr) = createCommand(ctx, ECommandType::eNOT, 1, (yyvsp[0].expr)); }
#line 1540 "seqc_parser.tab.c"
    break;

  case 21: /* multiplicative_expression: unary_expression  */
#line 177 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1546 "seqc_parser.tab.c"
    break;

  case 22: /* multiplicative_expression: multiplicative_expression '*' unary_expression  */
#line 179 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eMUL); }
#line 1552 "seqc_parser.tab.c"
    break;

  case 23: /* multiplicative_expression: multiplicative_expression '/' unary_expression  */
#line 181 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eDIV); }
#line 1558 "seqc_parser.tab.c"
    break;

  case 24: /* multiplicative_expression: multiplicative_expression '%' unary_expression  */
#line 183 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eMOD); }
#line 1564 "seqc_parser.tab.c"
    break;

  case 25: /* additive_expression: multiplicative_expression  */
#line 191 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1570 "seqc_parser.tab.c"
    break;

  case 26: /* additive_expression: additive_expression '+' multiplicative_expression  */
#line 193 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eADD); }
#line 1576 "seqc_parser.tab.c"
    break;

  case 27: /* additive_expression: additive_expression '-' multiplicative_expression  */
#line 195 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eSUB); }
#line 1582 "seqc_parser.tab.c"
    break;

  case 28: /* shift_expression: additive_expression  */
#line 203 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1588 "seqc_parser.tab.c"
    break;

  case 29: /* shift_expression: shift_expression LSH_OP additive_expression  */
#line 205 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eSHL); }
#line 1594 "seqc_parser.tab.c"
    break;

  case 30: /* shift_expression: shift_expression RSH_OP additive_expression  */
#line 207 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eSHR); }
#line 1600 "seqc_parser.tab.c"
    break;

  case 31: /* relational_expression: shift_expression  */
#line 215 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1606 "seqc_parser.tab.c"
    break;

  case 32: /* relational_expression: relational_expression '<' shift_expression  */
#line 217 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eLT); }
#line 1612 "seqc_parser.tab.c"
    break;

  case 33: /* relational_expression: relational_expression '>' shift_expression  */
#line 219 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eGT); }
#line 1618 "seqc_parser.tab.c"
    break;

  case 34: /* relational_expression: relational_expression LE_OP shift_expression  */
#line 221 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eLE); }
#line 1624 "seqc_parser.tab.c"
    break;

  case 35: /* relational_expression: relational_expression GE_OP shift_expression  */
#line 223 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eGE); }
#line 1630 "seqc_parser.tab.c"
    break;

  case 36: /* equality_expression: relational_expression  */
#line 231 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1636 "seqc_parser.tab.c"
    break;

  case 37: /* equality_expression: equality_expression EQ_OP relational_expression  */
#line 233 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eEQ); }
#line 1642 "seqc_parser.tab.c"
    break;

  case 38: /* equality_expression: equality_expression NE_OP relational_expression  */
#line 235 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eNE); }
#line 1648 "seqc_parser.tab.c"
    break;

  case 39: /* and_expression: equality_expression  */
#line 243 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1654 "seqc_parser.tab.c"
    break;

  case 40: /* and_expression: and_expression '&' equality_expression  */
#line 245 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eAND); }
#line 1660 "seqc_parser.tab.c"
    break;

  case 41: /* exclusive_or_expression: and_expression  */
#line 253 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1666 "seqc_parser.tab.c"
    break;

  case 42: /* exclusive_or_expression: exclusive_or_expression '^' and_expression  */
#line 255 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eXOR); }
#line 1672 "seqc_parser.tab.c"
    break;

  case 43: /* inclusive_or_expression: exclusive_or_expression  */
#line 263 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1678 "seqc_parser.tab.c"
    break;

  case 44: /* inclusive_or_expression: inclusive_or_expression '|' exclusive_or_expression  */
#line 265 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eOR); }
#line 1684 "seqc_parser.tab.c"
    break;

  case 45: /* logical_and_expression: inclusive_or_expression  */
#line 273 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1690 "seqc_parser.tab.c"
    break;

  case 46: /* logical_and_expression: logical_and_expression AND_OP inclusive_or_expression  */
#line 275 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eLAND); }
#line 1696 "seqc_parser.tab.c"
    break;

  case 47: /* logical_or_expression: logical_and_expression  */
#line 283 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1702 "seqc_parser.tab.c"
    break;

  case 48: /* logical_or_expression: logical_or_expression OR_OP logical_and_expression  */
#line 285 "seqc_parser.y"
        { (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eLOR); }
#line 1708 "seqc_parser.tab.c"
    break;

  case 49: /* conditional_expression: logical_or_expression  */
#line 293 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1714 "seqc_parser.tab.c"
    break;

  case 50: /* conditional_expression: logical_or_expression '?' expression ':' conditional_expression  */
#line 295 "seqc_parser.y"
        { (yyval.expr) = createCondExpression(ctx, (yyvsp[-4].expr), (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1720 "seqc_parser.tab.c"
    break;

  case 51: /* assignment_expression: conditional_expression  */
#line 303 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1726 "seqc_parser.tab.c"
    break;

  case 52: /* assignment_expression: unary_expression '=' assignment_expression  */
#line 305 "seqc_parser.y"
        {
            /* Binary writes valueType/valueCategory to $1 (the LHS),
               NOT to $$ (the operator).  Confirmed at 0x2ca99c:
               mov -0x10(%rbx),%rcx reads $1, then writes 0x54 and 0x04.
               This sets the LHS variable to dir=eIN(0), vc=eLVALUE(1),
               which prevents checkVar from firing on uninitialized vars. */
            (yyvsp[-2].expr)->valueType = 0;
            (yyvsp[-2].expr)->valueCategory = 1;
            (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eASSIGN);
        }
#line 1741 "seqc_parser.tab.c"
    break;

  case 53: /* assignment_expression: unary_expression ADD_ASSIGN assignment_expression  */
#line 316 "seqc_parser.y"
        {
            (yyvsp[-2].expr)->valueType = 0;
            (yyvsp[-2].expr)->valueCategory = 1;
            (yyval.expr) = createAssignOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eADD);
        }
#line 1751 "seqc_parser.tab.c"
    break;

  case 54: /* assignment_expression: unary_expression SUB_ASSIGN assignment_expression  */
#line 322 "seqc_parser.y"
        {
            (yyvsp[-2].expr)->valueType = 0;
            (yyvsp[-2].expr)->valueCategory = 1;
            (yyval.expr) = createAssignOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eSUB);
        }
#line 1761 "seqc_parser.tab.c"
    break;

  case 55: /* assignment_expression: unary_expression MUL_ASSIGN assignment_expression  */
#line 328 "seqc_parser.y"
        {
            (yyvsp[-2].expr)->valueType = 0;
            (yyvsp[-2].expr)->valueCategory = 1;
            (yyval.expr) = createAssignOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eMUL);
        }
#line 1771 "seqc_parser.tab.c"
    break;

  case 56: /* assignment_expression: unary_expression DIV_ASSIGN assignment_expression  */
#line 334 "seqc_parser.y"
        {
            (yyvsp[-2].expr)->valueType = 0;
            (yyvsp[-2].expr)->valueCategory = 1;
            (yyval.expr) = createAssignOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eDIV);
        }
#line 1781 "seqc_parser.tab.c"
    break;

  case 57: /* assignment_expression: unary_expression MOD_ASSIGN assignment_expression  */
#line 340 "seqc_parser.y"
        {
            (yyvsp[-2].expr)->valueType = 0;
            (yyvsp[-2].expr)->valueCategory = 1;
            (yyval.expr) = createAssignOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eMOD);
        }
#line 1791 "seqc_parser.tab.c"
    break;

  case 58: /* assignment_expression: unary_expression AND_ASSIGN assignment_expression  */
#line 346 "seqc_parser.y"
        {
            (yyvsp[-2].expr)->valueType = 0;
            (yyvsp[-2].expr)->valueCategory = 1;
            (yyval.expr) = createAssignOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eAND);
        }
#line 1801 "seqc_parser.tab.c"
    break;

  case 59: /* assignment_expression: unary_expression XOR_ASSIGN assignment_expression  */
#line 352 "seqc_parser.y"
        {
            (yyvsp[-2].expr)->valueType = 0;
            (yyvsp[-2].expr)->valueCategory = 1;
            (yyval.expr) = createAssignOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eXOR);
        }
#line 1811 "seqc_parser.tab.c"
    break;

  case 60: /* assignment_expression: unary_expression OR_ASSIGN assignment_expression  */
#line 358 "seqc_parser.y"
        {
            (yyvsp[-2].expr)->valueType = 0;
            (yyvsp[-2].expr)->valueCategory = 1;
            (yyval.expr) = createAssignOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eOR);
        }
#line 1821 "seqc_parser.tab.c"
    break;

  case 61: /* assignment_expression: unary_expression LSH_ASSIGN assignment_expression  */
#line 364 "seqc_parser.y"
        {
            (yyvsp[-2].expr)->valueType = 0;
            (yyvsp[-2].expr)->valueCategory = 1;
            (yyval.expr) = createAssignOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eSHR);
        }
#line 1831 "seqc_parser.tab.c"
    break;

  case 62: /* assignment_expression: unary_expression RSH_ASSIGN assignment_expression  */
#line 370 "seqc_parser.y"
        {
            (yyvsp[-2].expr)->valueType = 0;
            (yyvsp[-2].expr)->valueCategory = 1;
            (yyval.expr) = createAssignOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eSHL);
        }
#line 1841 "seqc_parser.tab.c"
    break;

  case 63: /* expression: assignment_expression  */
#line 382 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1847 "seqc_parser.tab.c"
    break;

  case 64: /* constant_expression: conditional_expression  */
#line 390 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1853 "seqc_parser.tab.c"
    break;

  case 65: /* declaration: type_specifier init_declarator_list ';'  */
#line 400 "seqc_parser.y"
        { (yyval.expr) = addVariableType(ctx, (yyvsp[-1].expr), (yyvsp[-2].expr), false); }
#line 1859 "seqc_parser.tab.c"
    break;

  case 66: /* init_declarator_list: init_declarator  */
#line 408 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1865 "seqc_parser.tab.c"
    break;

  case 67: /* init_declarator_list: init_declarator_list ',' init_declarator  */
#line 410 "seqc_parser.y"
        { (yyval.expr) = createOrAppendDeclList(ctx, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1871 "seqc_parser.tab.c"
    break;

  case 68: /* init_declarator: declarator  */
#line 418 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1877 "seqc_parser.tab.c"
    break;

  case 69: /* init_declarator: declarator '=' initializer  */
#line 420 "seqc_parser.y"
        {
            (yyval.expr) = createOperator(ctx, (yyvsp[-2].expr), (yyvsp[0].expr), EOperator::eASSIGN);
            (yyval.expr)->valueType = 0;
            (yyval.expr)->valueCategory = 1;
        }
#line 1887 "seqc_parser.tab.c"
    break;

  case 70: /* type_specifier: KW_VAR  */
#line 432 "seqc_parser.y"
        { (yyval.expr) = createVariableType(ctx, VarType_Var); }
#line 1893 "seqc_parser.tab.c"
    break;

  case 71: /* type_specifier: KW_CONST  */
#line 434 "seqc_parser.y"
        { (yyval.expr) = createVariableType(ctx, VarType_Const); }
#line 1899 "seqc_parser.tab.c"
    break;

  case 72: /* type_specifier: KW_CVAR  */
#line 436 "seqc_parser.y"
        { (yyval.expr) = createVariableType(ctx, VarType_Cvar); }
#line 1905 "seqc_parser.tab.c"
    break;

  case 73: /* type_specifier: KW_VOID  */
#line 438 "seqc_parser.y"
        { (yyval.expr) = createVariableType(ctx, VarType_Unset); }
#line 1911 "seqc_parser.tab.c"
    break;

  case 74: /* type_specifier: KW_WAVE  */
#line 440 "seqc_parser.y"
        { (yyval.expr) = createVariableType(ctx, VarType_Wave); }
#line 1917 "seqc_parser.tab.c"
    break;

  case 75: /* type_specifier: KW_STRING  */
#line 442 "seqc_parser.y"
        { (yyval.expr) = createVariableType(ctx, VarType_String); }
#line 1923 "seqc_parser.tab.c"
    break;

  case 76: /* declarator: identifier  */
#line 450 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1929 "seqc_parser.tab.c"
    break;

  case 77: /* declarator: identifier '[' constant_expression ']'  */
#line 452 "seqc_parser.y"
        { (yyval.expr) = createArray(ctx, (yyvsp[-3].expr), (yyvsp[-1].expr)); }
#line 1935 "seqc_parser.tab.c"
    break;

  case 78: /* declarator: identifier '[' ']'  */
#line 454 "seqc_parser.y"
        { (yyval.expr) = createArray(ctx, (yyvsp[-2].expr), NULL); }
#line 1941 "seqc_parser.tab.c"
    break;

  case 79: /* function_declarator: identifier '(' parameter_list ')'  */
#line 462 "seqc_parser.y"
        { (yyval.expr) = createFunctionCall(ctx, (yyvsp[-3].expr), (yyvsp[-1].expr)); }
#line 1947 "seqc_parser.tab.c"
    break;

  case 80: /* function_declarator: identifier '(' ')'  */
#line 464 "seqc_parser.y"
        { (yyval.expr) = createFunctionCall(ctx, (yyvsp[-2].expr), NULL); }
#line 1953 "seqc_parser.tab.c"
    break;

  case 81: /* parameter_list: parameter_declaration  */
#line 472 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1959 "seqc_parser.tab.c"
    break;

  case 82: /* parameter_list: parameter_list ',' parameter_declaration  */
#line 474 "seqc_parser.y"
        { (yyval.expr) = createOrAppendParamList(ctx, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 1965 "seqc_parser.tab.c"
    break;

  case 83: /* parameter_declaration: type_specifier declarator  */
#line 482 "seqc_parser.y"
        { (yyval.expr) = addVariableType(ctx, (yyvsp[0].expr), (yyvsp[-1].expr), false); }
#line 1971 "seqc_parser.tab.c"
    break;

  case 84: /* parameter_declaration: declarator  */
#line 484 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1977 "seqc_parser.tab.c"
    break;

  case 85: /* identifier: IDENTIFIER  */
#line 492 "seqc_parser.y"
        { (yyval.expr) = createVariable(ctx, (yyvsp[0].sval)); }
#line 1983 "seqc_parser.tab.c"
    break;

  case 86: /* initializer: assignment_expression  */
#line 500 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1989 "seqc_parser.tab.c"
    break;

  case 87: /* statement: unlabeled_statement  */
#line 508 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 1995 "seqc_parser.tab.c"
    break;

  case 88: /* statement: labeled_statement  */
#line 510 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2001 "seqc_parser.tab.c"
    break;

  case 89: /* unlabeled_statement: compound_statement  */
#line 518 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2007 "seqc_parser.tab.c"
    break;

  case 90: /* unlabeled_statement: expression_statement  */
#line 520 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2013 "seqc_parser.tab.c"
    break;

  case 91: /* unlabeled_statement: selection_statement  */
#line 522 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2019 "seqc_parser.tab.c"
    break;

  case 92: /* unlabeled_statement: iteration_statement  */
#line 524 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2025 "seqc_parser.tab.c"
    break;

  case 93: /* unlabeled_statement: jump_statement  */
#line 526 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2031 "seqc_parser.tab.c"
    break;

  case 94: /* labeled_statement: KW_CASE constant_expression ':' statement  */
#line 534 "seqc_parser.y"
        { (yyval.expr) = createCase(ctx, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2037 "seqc_parser.tab.c"
    break;

  case 95: /* labeled_statement: KW_DEFAULT ':' statement  */
#line 536 "seqc_parser.y"
        { (yyval.expr) = createCase(ctx, NULL, (yyvsp[0].expr)); }
#line 2043 "seqc_parser.tab.c"
    break;

  case 96: /* labeled_statement: KW_CASE constant_expression ':'  */
#line 538 "seqc_parser.y"
        { (yyval.expr) = createCase(ctx, (yyvsp[-1].expr), NULL); }
#line 2049 "seqc_parser.tab.c"
    break;

  case 97: /* labeled_statement: KW_DEFAULT ':'  */
#line 540 "seqc_parser.y"
        { (yyval.expr) = createCase(ctx, NULL, NULL); }
#line 2055 "seqc_parser.tab.c"
    break;

  case 98: /* compound_statement: '{' '}'  */
#line 548 "seqc_parser.y"
        { (yyval.expr) = NULL; }
#line 2061 "seqc_parser.tab.c"
    break;

  case 99: /* compound_statement: '{' compound_body '}'  */
#line 550 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[-1].expr); }
#line 2067 "seqc_parser.tab.c"
    break;

  case 100: /* compound_body: declaration  */
#line 558 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2073 "seqc_parser.tab.c"
    break;

  case 101: /* compound_body: statement  */
#line 560 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2079 "seqc_parser.tab.c"
    break;

  case 102: /* compound_body: compound_body declaration  */
#line 562 "seqc_parser.y"
        { (yyval.expr) = createOrAppendStmtList(ctx, (yyvsp[-1].expr), (yyvsp[0].expr)); }
#line 2085 "seqc_parser.tab.c"
    break;

  case 103: /* compound_body: compound_body statement  */
#line 564 "seqc_parser.y"
        { (yyval.expr) = createOrAppendStmtList(ctx, (yyvsp[-1].expr), (yyvsp[0].expr)); }
#line 2091 "seqc_parser.tab.c"
    break;

  case 104: /* expression_statement: ';'  */
#line 582 "seqc_parser.y"
        { (yyval.expr) = NULL; }
#line 2097 "seqc_parser.tab.c"
    break;

  case 105: /* expression_statement: expression ';'  */
#line 584 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[-1].expr); }
#line 2103 "seqc_parser.tab.c"
    break;

  case 106: /* selection_statement: KW_IF '(' expression ')' statement  */
#line 592 "seqc_parser.y"
        { (yyval.expr) = createIf(ctx, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2109 "seqc_parser.tab.c"
    break;

  case 107: /* selection_statement: KW_IF '(' expression ')' statement KW_ELSE statement  */
#line 594 "seqc_parser.y"
        { (yyval.expr) = createIfElse(ctx, (yyvsp[-4].expr), (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2115 "seqc_parser.tab.c"
    break;

  case 108: /* selection_statement: KW_SWITCH '(' expression ')' statement  */
#line 596 "seqc_parser.y"
        { (yyval.expr) = createSwitch(ctx, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2121 "seqc_parser.tab.c"
    break;

  case 109: /* iteration_statement: KW_WHILE '(' expression ')' statement  */
#line 604 "seqc_parser.y"
        { (yyval.expr) = createWhile(ctx, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2127 "seqc_parser.tab.c"
    break;

  case 110: /* iteration_statement: KW_DO statement KW_WHILE '(' expression ')' ';'  */
#line 606 "seqc_parser.y"
        { (yyval.expr) = createDoWhile(ctx, (yyvsp[-5].expr), (yyvsp[-2].expr)); }
#line 2133 "seqc_parser.tab.c"
    break;

  case 111: /* iteration_statement: KW_FOR '(' expression_statement expression_statement ')' statement  */
#line 608 "seqc_parser.y"
        { (yyval.expr) = createFor(ctx, (yyvsp[-3].expr), (yyvsp[-2].expr), NULL, (yyvsp[0].expr)); }
#line 2139 "seqc_parser.tab.c"
    break;

  case 112: /* iteration_statement: KW_FOR '(' declaration expression_statement ')' statement  */
#line 610 "seqc_parser.y"
        { (yyval.expr) = createFor(ctx, (yyvsp[-3].expr), (yyvsp[-2].expr), NULL, (yyvsp[0].expr)); }
#line 2145 "seqc_parser.tab.c"
    break;

  case 113: /* iteration_statement: KW_FOR '(' expression_statement expression_statement expression ')' statement  */
#line 612 "seqc_parser.y"
        { (yyval.expr) = createFor(ctx, (yyvsp[-4].expr), (yyvsp[-3].expr), (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2151 "seqc_parser.tab.c"
    break;

  case 114: /* iteration_statement: KW_FOR '(' declaration expression_statement expression ')' statement  */
#line 614 "seqc_parser.y"
        { (yyval.expr) = createFor(ctx, (yyvsp[-4].expr), (yyvsp[-3].expr), (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2157 "seqc_parser.tab.c"
    break;

  case 115: /* iteration_statement: KW_REPEAT '(' expression ')' statement  */
#line 616 "seqc_parser.y"
        { (yyval.expr) = createRepeat(ctx, (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 2163 "seqc_parser.tab.c"
    break;

  case 116: /* jump_statement: KW_CONTINUE ';'  */
#line 624 "seqc_parser.y"
        { (yyval.expr) = createCommand(ctx, ECommandType::eCONTINUE, 0); }
#line 2169 "seqc_parser.tab.c"
    break;

  case 117: /* jump_statement: KW_BREAK ';'  */
#line 626 "seqc_parser.y"
        { (yyval.expr) = createCommand(ctx, ECommandType::eBREAK, 0); }
#line 2175 "seqc_parser.tab.c"
    break;

  case 118: /* jump_statement: KW_RETURN ';'  */
#line 628 "seqc_parser.y"
        { (yyval.expr) = createCommand(ctx, ECommandType::eRETURN, 0); }
#line 2181 "seqc_parser.tab.c"
    break;

  case 119: /* jump_statement: KW_RETURN expression ';'  */
#line 630 "seqc_parser.y"
        { (yyval.expr) = createCommand(ctx, ECommandType::eRETURN, 1, (yyvsp[-1].expr)); }
#line 2187 "seqc_parser.tab.c"
    break;

  case 120: /* translation_unit: %empty  */
#line 638 "seqc_parser.y"
        { *result = NULL; }
#line 2193 "seqc_parser.tab.c"
    break;

  case 121: /* translation_unit: external_declaration_list  */
#line 640 "seqc_parser.y"
        { *result = (yyvsp[0].expr); }
#line 2199 "seqc_parser.tab.c"
    break;

  case 122: /* external_declaration_list: external_declaration  */
#line 648 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2205 "seqc_parser.tab.c"
    break;

  case 123: /* external_declaration_list: external_declaration_list external_declaration  */
#line 650 "seqc_parser.y"
        { (yyval.expr) = createOrAppendStmtList(ctx, (yyvsp[-1].expr), (yyvsp[0].expr)); }
#line 2211 "seqc_parser.tab.c"
    break;

  case 124: /* external_declaration: declaration  */
#line 658 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2217 "seqc_parser.tab.c"
    break;

  case 125: /* external_declaration: statement  */
#line 660 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2223 "seqc_parser.tab.c"
    break;

  case 126: /* external_declaration: function_definition  */
#line 662 "seqc_parser.y"
        { (yyval.expr) = (yyvsp[0].expr); }
#line 2229 "seqc_parser.tab.c"
    break;

  case 127: /* function_definition: type_specifier function_declarator compound_statement  */
#line 671 "seqc_parser.y"
        { (yyval.expr) = createFunction(ctx, (yyvsp[-2].expr), (yyvsp[-1].expr), (yyvsp[0].expr)); }
#line 2235 "seqc_parser.tab.c"
    break;


#line 2239 "seqc_parser.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == SEQC_EMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (ctx, result, scanner, YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= SEQC_EOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == SEQC_EOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, ctx, result, scanner);
          yychar = SEQC_EMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, ctx, result, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (ctx, result, scanner, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != SEQC_EMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, ctx, result, scanner);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, ctx, result, scanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 674 "seqc_parser.y"


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
