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
enum class EOperationType : int32_t {
    eCOMMAND       = 0,
    eFUNCTION      = 1,
    eFUNCTIONCALL  = 2,
    eVARIABLE      = 3,
    eOPERATOR      = 4,
    eARRAY         = 5,
    eARGLIST       = 6,
    eDECLLIST      = 7,
    ePARAMLIST     = 8,
    eSTMTLIST      = 9,
    eLABEL         = 10,
    eVARIABLETYPE  = 11,
    eVALUE         = 12,
};

// Expression::operator_ (+0x48)  —  jump table @0x95a860
enum class EOperator : int32_t {
    eADD       = 0,   // "+"
    eSUB       = 1,   // "-"
    eMUL       = 2,   // "*"
    eDIV       = 3,   // "/"
    eMOD       = 4,   // "%"
    eSHL       = 5,   // "<<"
    eSHR       = 6,   // ">>"
    eGT        = 7,   // ">"
    eLT        = 8,   // "<"
    eLE        = 9,   // "<="
    eGE        = 10,  // ">="
    eEQ        = 11,  // "=="
    eNE        = 12,  // "!="
    eINC       = 13,  // "++"
    eDEC       = 14,  // "--"
    eAND       = 15,  // "&"
    eOR        = 16,  // "|"
    eXOR       = 17,  // "^"
    eLAND      = 18,  // "&&"
    eLOR       = 19,  // "||"
    eASSIGN    = 20,  // "="
    eNONE      = 21,  // "" (unset / default)
};

// Expression::commandType (+0x4C)  —  jump table @0x95a8b8
enum class ECommandType : int32_t {
    eIF        = 0,
    eIFELSE    = 1,
    eSWITCH    = 2,
    eCASE      = 3,
    eFOR       = 4,
    eWHILE     = 5,
    eDOWHILE   = 6,
    eREPEAT    = 7,
    eCONDEXP   = 8,
    eCONTINUE  = 9,
    eBREAK     = 10,
    eRETURN    = 11,
    eNEG       = 12,
    ePOS       = 13,
    eINV       = 14,
    eNOT       = 15,
    eNOCMD     = 16,
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

std::string str(EOperationType t);   // 0x1c1530
std::string str(EOperator op);       // 0x1c1790
std::string str(ECommandType ct);    // 0x1c18e0

// ---- Free functions: parser actions ("create*") ---------------------------

// Returns raw Expression* — callers wrap in shared_ptr.

Expression* createValue(SeqcParserContext* ctx, double val);                     // 0x1bf260
Expression* createString(SeqcParserContext* ctx, const char* s);                 // 0x1bf2d0
Expression* createVariable(SeqcParserContext* ctx, const char* name);            // 0x1bf420
Expression* addVariableType(SeqcParserContext* ctx, Expression* expr,
                            Expression* typeExpr, bool isConst);                 // 0x1bf560
Expression* createVariableType(SeqcParserContext* ctx, VarType vt);              // 0x1bf7c0
Expression* createOperator(SeqcParserContext* ctx, Expression* lhs,
                           Expression* rhs, EOperator op);                       // 0x1bf830
Expression* createAssignOperator(SeqcParserContext* ctx, Expression* lhs,
                                 Expression* rhs, EOperator op);                 // 0x1bf9c0
Expression* createArray(SeqcParserContext* ctx, Expression* lhs,
                        Expression* rhs);                                        // 0x1bfb50
Expression* createListType(SeqcParserContext* ctx, EOperationType opType,
                           Expression* lhs, Expression* rhs);                    // 0x1bfb70
Expression* createOrAppendListType(SeqcParserContext* ctx,
                                   EOperationType opType,
                                   Expression* lhs, Expression* rhs);            // 0x1bfd20
Expression* createOrAppendArgList(SeqcParserContext* ctx,
                                  Expression* lhs, Expression* rhs);             // 0x1bfd00
Expression* createOrAppendDeclList(SeqcParserContext* ctx,
                                   Expression* lhs, Expression* rhs);            // 0x1bfe00
Expression* createOrAppendParamList(SeqcParserContext* ctx,
                                    Expression* lhs, Expression* rhs);           // 0x1bfe20
Expression* createOrAppendStmtList(SeqcParserContext* ctx,
                                   Expression* lhs, Expression* rhs);            // 0x1bfe40
Expression* createFunctionCall(SeqcParserContext* ctx,
                               Expression* func, Expression* args);              // 0x1bfe60
Expression* createFunction(SeqcParserContext* ctx, Expression* returnTypeExpr,
                           Expression* params, Expression* body);                // 0x1c0000
Expression* createCommand(SeqcParserContext* ctx, ECommandType cmd,
                           int count, ...);                                      // 0x1c0330
Expression* createIf(SeqcParserContext* ctx, Expression* cond,
                     Expression* body);                                          // 0x1c0530
Expression* createIfElse(SeqcParserContext* ctx, Expression* cond,
                         Expression* thenBody, Expression* elseBody);            // 0x1c06c0
Expression* createSwitch(SeqcParserContext* ctx, Expression* val,
                         Expression* body);                                      // 0x1c08d0
Expression* createCase(SeqcParserContext* ctx, Expression* val,
                       Expression* body);                                        // 0x1c0a60
Expression* createCondExpression(SeqcParserContext* ctx, Expression* cond,
                                 Expression* trueBranch,
                                 Expression* falseBranch);                       // 0x1c0bf0
Expression* createFor(SeqcParserContext* ctx, Expression* init,
                      Expression* cond, Expression* incr,
                      Expression* body);                                         // 0x1c0e00
Expression* createWhile(SeqcParserContext* ctx, Expression* cond,
                        Expression* body);                                       // 0x1c1080
Expression* createRepeat(SeqcParserContext* ctx, Expression* count,
                         Expression* body);                                      // 0x1c1210
Expression* createDoWhile(SeqcParserContext* ctx, Expression* body,
                          Expression* cond);                                     // 0x1c13a0

// Deep copy (recursive) — 0x1bef20
std::shared_ptr<Expression> copyExpression(std::shared_ptr<Expression> expr);

// Bison error callback — 0x2ca1b0
int seqc_error(SeqcParserContext* ctx, Expression** /*lval*/,
               void* /*scanner*/, const char* msg);

} // namespace zhinst
