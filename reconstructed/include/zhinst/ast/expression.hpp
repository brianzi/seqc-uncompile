// ============================================================================
// Expression — SeqC parser AST node (plain struct, no vtable)
//
// Size: 0x58 (88) bytes.  Managed via shared_ptr<Expression>.
// Layout derived from copy ctor @0x1bfa30, create* functions, and
// copyExpression @0x1bef20.
//
// Enum values recovered from str(EOperationType) @0x1c1530,
// str(EOperator) @0x1c1790, str(ECommandType) @0x1c18e0.
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "zhinst/runtime/resources.hpp"  // for VarType enum (Unset/Var/Const/Cvar/String/Wave)
#include "zhinst/core/types.hpp"       // for EDirection enum

namespace zhinst {

class SeqcParserContext;  // forward decl — no header yet

// ---- Enums ----------------------------------------------------------------

// Expression::operationType (+0x00)  —  jump table @0x95a82c
//! \brief Top-level kind of an `Expression` parser-AST node.
//!
//! Stored in `Expression::operationType` and used as the primary
//! discriminator by the frontend lowering pass.  Together with
//! `EOperator` (binary/unary operator slot) and `ECommandType`
//! (control-flow / unary-statement slot) it forms the three
//! orthogonal tags carried by every parser node.
enum class EOperationType : int32_t {
    eCOMMAND       = 0,    //!< Control-flow / statement form (see `ECommandType`).
    eFUNCTION      = 1,    //!< Function definition (return type + params + body).
    eFUNCTIONCALL  = 2,    //!< Call site (callee + arg list).
    eVARIABLE      = 3,    //!< Variable reference by name.
    eOPERATOR      = 4,    //!< Binary/unary operator application (see `EOperator`).
    eARRAY         = 5,    //!< Array indexing (`a[i]`).
    eARGLIST       = 6,    //!< Argument list of a call.
    eDECLLIST      = 7,    //!< Comma-separated declarator list.
    ePARAMLIST     = 8,    //!< Parameter list of a function definition.
    eSTMTLIST      = 9,    //!< Sequence of statements.
    eLABEL         = 10,   //!< Label (e.g. `case` value).
    eVARIABLETYPE  = 11,   //!< Type-qualifier node (carries `VarType`).
    eVALUE         = 12,   //!< Numeric or string literal.
};

// Expression::operator_ (+0x48)  —  jump table @0x95a860
//! \brief Operator carried by `eOPERATOR` (and assignment-operator)
//! `Expression` nodes.  `eNONE` is the default for non-operator
//! nodes.  Renderable via `str(EOperator)`.
enum class EOperator : int32_t {
    eADD       = 0,   //!< `+`
    eSUB       = 1,   //!< `-`
    eMUL       = 2,   //!< `*`
    eDIV       = 3,   //!< `/`
    eMOD       = 4,   //!< `%`
    eSHL       = 5,   //!< `<<`
    eSHR       = 6,   //!< `>>`
    eGT        = 7,   //!< `>`
    eLT        = 8,   //!< `<`
    eLE        = 9,   //!< `<=`
    eGE        = 10,  //!< `>=`
    eEQ        = 11,  //!< `==`
    eNE        = 12,  //!< `!=`
    eINC       = 13,  //!< `++`
    eDEC       = 14,  //!< `--`
    eAND       = 15,  //!< `&`
    eOR        = 16,  //!< `|`
    eXOR       = 17,  //!< `^`
    eLAND      = 18,  //!< `&&`
    eLOR       = 19,  //!< `||`
    eASSIGN    = 20,  //!< `=`
    eNONE      = 21,  //!< unset / default
};

// Expression::commandType (+0x4C)  —  jump table @0x95a8b8
//! \brief Control-flow or unary-statement form carried by
//! `eCOMMAND` `Expression` nodes.  `eNOCMD` is the default for
//! non-command nodes.  Renderable via `str(ECommandType)`.
enum class ECommandType : int32_t {
    eIF        = 0,   //!< `if (cond) body`
    eIFELSE    = 1,   //!< `if (cond) thenBody else elseBody`
    eSWITCH    = 2,   //!< `switch (val) body`
    eCASE      = 3,   //!< `case val: body`
    eFOR       = 4,   //!< `for (init; cond; incr) body`
    eWHILE     = 5,   //!< `while (cond) body`
    eDOWHILE   = 6,   //!< `do body while (cond)`
    eREPEAT    = 7,   //!< `repeat (count) body`
    eCONDEXP   = 8,   //!< Conditional expression (`cond ? a : b`).
    eCONTINUE  = 9,   //!< `continue;`
    eBREAK     = 10,  //!< `break;`
    eRETURN    = 11,  //!< `return [val];`
    eNEG       = 12,  //!< Unary `-`.
    ePOS       = 13,  //!< Unary `+`.
    eINV       = 14,  //!< Bitwise `~`.
    eNOT       = 15,  //!< Logical `!`.
    eNOCMD     = 16,  //!< Unset / default.
};

// VarType used by createVariableType — stored at +0x50.
// Defined in resources.hpp (enum VarType : int32_t with values
// Unset=0, Var=2, Const=3, Cvar=4, String=5, Wave=6).
// Reconstructed binary writes enum values into this 4-byte slot.

// ---- Expression struct ----------------------------------------------------

// Plain struct, NO vtable.  Size = 0x58 (88 bytes).
//
// Offset  Size  Type                              Field
// ------  ----  --------------------------------  -----------------------
// +0x00     4   EOperationType                    operationType
// +0x04     4   int32_t                           valueCategory
//               (0 = default, 2 = rvalue — set for Value/String/FunctionCall)
// +0x08     4   int32_t                           lineNumber
// +0x0C     4   (padding — zeroed)
// +0x10     8   double                            value
// +0x18    24   std::string                       name   (libc++ SSO)
// +0x30    24   std::vector<shared_ptr<Expression>> children
// +0x48     4   EOperator                         operator_
// +0x4C     4   ECommandType                      commandType
// +0x50     4   VarType                           varType
// +0x54     4   int32_t                           direction  (EDirection enum; always 2 in binary)
//
//! \brief Concrete-syntax tree node produced by the SeqC bison parser.
//!
//! `Expression` is the uniform node type emitted by the flex/bison frontend
//! during parsing.  Every parser action — `createValue`, `createOperator`,
//! `createIfElse`, etc. — allocates one of these and links children via
//! `children`.  The kind of node is encoded by three orthogonal tags:
//! `operationType` (top-level category such as command, function call,
//! variable, value), `operator_` (binary/unary operator when applicable),
//! and `commandType` (control-flow or unary-statement form when applicable);
//! defaults are `eNONE`/`eNOCMD`.
//!
//! `Expression` is the *parser-side* AST and is later lowered into the
//! virtual `SeqCAstNode` hierarchy by the frontend lowering pass; consumers
//! manage instances through `std::shared_ptr<Expression>`.
struct Expression {
    EOperationType                               operationType;  // +0x00
    int32_t                                      valueCategory;  // +0x04
    int32_t                                      lineNumber;     // +0x08
    int32_t                                      pad0C_{};       // +0x0C
    double                                       value;          // +0x10
    std::string                                  name;           // +0x18
    std::vector<std::shared_ptr<Expression>>     children;       // +0x30
    EOperator                                    operator_;      // +0x48
    ECommandType                                 commandType;    // +0x4C
    VarType                                      varType;        // +0x50
    EDirection                                   direction;      // +0x54

    // Default-initialise to the binary's .rodata pattern {21, 16, 0, 2}
    Expression()
        : operationType(EOperationType::eCOMMAND)
        , valueCategory(0)
        , lineNumber(0)
        , value(0.0)
        , operator_(EOperator::eNONE)         // 21
        , commandType(ECommandType::eNOCMD)   // 16
        , varType(VarType_Unset)
        , direction(EDirection::eINOUT)
    {}

    // Copy ctor — 0x1bfa30
    Expression(const Expression& other);
};

// Binary size is 0x58 (88 bytes) with libc++ 24-byte SSO string.
// With libstdc++ (32-byte string), sizeof is 0x60 (96 bytes).
static_assert(sizeof(Expression) == 0x58 || sizeof(Expression) == 0x60,
              "Expression size mismatch — expected 0x58 (libc++) or 0x60 (libstdc++)");

// ---- Enum → string helpers (for debug printing) ---------------------------

//! \brief Renders an `EOperationType` as its enumerator name (e.g.
//! `"eVALUE"`); used for debug logging and error messages.
std::string str(EOperationType t);   // 0x1c1530
//! \brief Renders an `EOperator` as the source operator token it
//! corresponds to (e.g. `"+"`, `"<<"`); `eNONE` renders as `""`.
std::string str(EOperator op);       // 0x1c1790
//! \brief Renders an `ECommandType` as its enumerator name (e.g.
//! `"eIF"`); used for debug logging and error messages.
std::string str(ECommandType ct);    // 0x1c18e0

// ---- Free functions: parser actions ("create*") ---------------------------

// Returns raw Expression* — callers wrap in shared_ptr.

//! \brief Parser action: builds a numeric-literal `eVALUE`
//! `Expression` carrying `val` and the current source line number.
Expression* createValue(SeqcParserContext* ctx, double val);                     // 0x1bf260
//! \brief Parser action: builds a string-literal `eVALUE`
//! `Expression` (with `varType = VarType_String`) carrying `s` as
//! its name and the current source line number.
Expression* createString(SeqcParserContext* ctx, const char* s);                 // 0x1bf2d0
//! \brief Parser action: builds an `eVARIABLE` `Expression`
//! referencing the named identifier.
Expression* createVariable(SeqcParserContext* ctx, const char* name);            // 0x1bf420
//! \brief Parser action: stamps the type carried by `typeExpr` onto
//! `expr` (or every child of `expr` when `expr` is an `eDECLLIST`),
//! creating placeholder nodes for nullptr inputs and disposing of
//! `typeExpr` unless `isConst` requests it be kept.
Expression* addVariableType(SeqcParserContext* ctx, Expression* expr,
                            Expression* typeExpr, bool isConst);                 // 0x1bf560
//! \brief Parser action: builds a free-standing `eVARIABLETYPE`
//! `Expression` carrying the given `VarType`.
Expression* createVariableType(SeqcParserContext* ctx, VarType vt);              // 0x1bf7c0
//! \brief Parser action: builds an `eOPERATOR` `Expression` with
//! the given operator and `[lhs, rhs]` as its child operands.
Expression* createOperator(SeqcParserContext* ctx, Expression* lhs,
                           Expression* rhs, EOperator op);                       // 0x1bf830
//! \brief Parser action: builds a compound-assignment `eOPERATOR`
//! `Expression` (e.g. `+=`, `<<=`) by combining `op` with `eASSIGN`.
Expression* createAssignOperator(SeqcParserContext* ctx, Expression* lhs,
                                 Expression* rhs, EOperator op);                 // 0x1bf9c0
//! \brief Parser action: builds an `eARRAY` indexing `Expression`
//! with `[lhs, rhs]` as its child operands.
Expression* createArray(SeqcParserContext* ctx, Expression* lhs,
                        Expression* rhs);                                        // 0x1bfb50
//! \brief Parser action: builds a list-kind `Expression` of the
//! given `EOperationType` (e.g. `eARGLIST`, `eDECLLIST`,
//! `ePARAMLIST`, `eSTMTLIST`) with `[lhs, rhs]` as its initial
//! children.
Expression* createListType(SeqcParserContext* ctx, EOperationType opType,
                           Expression* lhs, Expression* rhs);                    // 0x1bfb70
//! \brief Parser action: appends `rhs` to `lhs` if `lhs` is already
//! a list of `opType`, otherwise constructs a fresh list via
//! `createListType`.
Expression* createOrAppendListType(SeqcParserContext* ctx,
                                   EOperationType opType,
                                   Expression* lhs, Expression* rhs);            // 0x1bfd20
//! \brief Parser action: append-or-create wrapper specialised for
//! `eARGLIST`.
Expression* createOrAppendArgList(SeqcParserContext* ctx,
                                  Expression* lhs, Expression* rhs);             // 0x1bfd00
//! \brief Parser action: append-or-create wrapper specialised for
//! `eDECLLIST`.
Expression* createOrAppendDeclList(SeqcParserContext* ctx,
                                   Expression* lhs, Expression* rhs);            // 0x1bfe00
//! \brief Parser action: append-or-create wrapper specialised for
//! `ePARAMLIST`.
Expression* createOrAppendParamList(SeqcParserContext* ctx,
                                    Expression* lhs, Expression* rhs);           // 0x1bfe20
//! \brief Parser action: append-or-create wrapper specialised for
//! `eSTMTLIST`.
Expression* createOrAppendStmtList(SeqcParserContext* ctx,
                                   Expression* lhs, Expression* rhs);            // 0x1bfe40
//! \brief Parser action: builds an `eFUNCTIONCALL` `Expression`
//! pairing the callee `func` with its `args` (an `eARGLIST`).
Expression* createFunctionCall(SeqcParserContext* ctx,
                               Expression* func, Expression* args);              // 0x1bfe60
//! \brief Parser action: builds an `eFUNCTION` `Expression`
//! grouping a return-type expression, parameter list, and body.
Expression* createFunction(SeqcParserContext* ctx, Expression* returnTypeExpr,
                           Expression* params, Expression* body);                // 0x1c0000
//! \brief Parser action: builds an `eCOMMAND` `Expression` of the
//! given `ECommandType` from `count` variadic child operands.  Used
//! by the `createIf` / `createWhile` / etc. helpers below as well
//! as directly for the no-operand control-flow forms (`continue`,
//! `break`, `return`).
Expression* createCommand(SeqcParserContext* ctx, ECommandType cmd,
                           int count, ...);                                      // 0x1c0330
//! \brief Parser action: builds an `eIF` command from condition
//! and body.
Expression* createIf(SeqcParserContext* ctx, Expression* cond,
                     Expression* body);                                          // 0x1c0530
//! \brief Parser action: builds an `eIFELSE` command from
//! condition, then-body, and else-body.
Expression* createIfElse(SeqcParserContext* ctx, Expression* cond,
                         Expression* thenBody, Expression* elseBody);            // 0x1c06c0
//! \brief Parser action: builds an `eSWITCH` command from
//! discriminant value and body.
Expression* createSwitch(SeqcParserContext* ctx, Expression* val,
                         Expression* body);                                      // 0x1c08d0
//! \brief Parser action: builds an `eCASE` command (label value +
//! body) inside a `switch`.
Expression* createCase(SeqcParserContext* ctx, Expression* val,
                       Expression* body);                                        // 0x1c0a60
//! \brief Parser action: builds an `eCONDEXP` command (ternary
//! `cond ? trueBranch : falseBranch`).
Expression* createCondExpression(SeqcParserContext* ctx, Expression* cond,
                                 Expression* trueBranch,
                                 Expression* falseBranch);                       // 0x1c0bf0
//! \brief Parser action: builds an `eFOR` command from init /
//! cond / incr / body.
Expression* createFor(SeqcParserContext* ctx, Expression* init,
                      Expression* cond, Expression* incr,
                      Expression* body);                                         // 0x1c0e00
//! \brief Parser action: builds an `eWHILE` command from cond and
//! body.
Expression* createWhile(SeqcParserContext* ctx, Expression* cond,
                        Expression* body);                                       // 0x1c1080
//! \brief Parser action: builds an `eREPEAT` command from a count
//! expression and body (`repeat (count) body`).
Expression* createRepeat(SeqcParserContext* ctx, Expression* count,
                         Expression* body);                                      // 0x1c1210
//! \brief Parser action: builds an `eDOWHILE` command from body
//! and cond (`do body while (cond)`).
Expression* createDoWhile(SeqcParserContext* ctx, Expression* body,
                          Expression* cond);                                     // 0x1c13a0

//! \brief Recursive deep copy of an `Expression` tree — clones
//! every node and the entire `children` subtree as fresh
//! `shared_ptr<Expression>` instances.
std::shared_ptr<Expression> copyExpression(std::shared_ptr<Expression> expr);

//! \brief Bison parser error callback invoked by the generated
//! parser when a syntax error is detected.  Forwards `msg` to
//! `ctx->raiseError()` and then sets the parser-context syntax-error
//! flag via `ctx->setSyntaxError()`.  The `lval` and `scanner`
//! parameters are unused.
//! \return Always 1 (the value the bison runtime expects on error).
int seqc_error(SeqcParserContext* ctx, Expression** /*lval*/,
               void* /*scanner*/, const char* msg);

} // namespace zhinst
