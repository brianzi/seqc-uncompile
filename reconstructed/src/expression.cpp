// ============================================================================
// Expression — SeqC parser AST node implementation
//
// Reconstructed from _seqc_compiler.so disassembly.
// Each function lists its binary address in a comment.
// ============================================================================

#include "zhinst/expression.hpp"
#include "zhinst/seqc_parser_context.hpp"

#include <cassert>
#include <cstdarg>
#include <cstring>

namespace zhinst {

// ============================================================================
// Helper: allocate an Expression with standard zero-init + default tail
// ============================================================================

static Expression* allocExpression() {
    // operator new(0x58) + zero-init + load {21, 16, 0, 2} into +0x48
    return new Expression();   // default ctor sets the .rodata pattern
}

// Helper: wrap a raw Expression* into a shared_ptr and push_back.
//
// Resolved #93: Binary allocates a 0x20-byte __shared_ptr_pointer control
// block (vptr → vtable for __shared_ptr_pointer<Expression*,
// shared_ptr<Expression>::__shared_ptr_default_delete, allocator<Expression>>
// at 0xb03730), stores the raw pointer at cb+0x18, then calls
// vector::push_back(shared_ptr&&).  The __on_zero_shared handler at
// 0x1296e0 invokes default_delete<Expression> at 0x129720, which
// recursively destroys children and frees the object.
//
// Ownership model: create* functions allocate with `new Expression()` and
// return a raw pointer.  pushChild transfers ownership into a shared_ptr
// with the standard default deleter.  No no-op deleter is involved.
static void pushChild(std::vector<std::shared_ptr<Expression>>& vec,
                      Expression* raw) {
    vec.push_back(std::shared_ptr<Expression>(raw));
}

// ============================================================================
// Copy constructor — 0x1bfa30
// ============================================================================

Expression::Expression(const Expression& other)  // 0x1bfa30
    : operationType(other.operationType)
    , valueCategory(other.valueCategory)
    , lineNumber(other.lineNumber)
    , pad0C_(other.pad0C_)
    , value(other.value)
    , name(other.name)
    , children()     // deep-copied below
    , operator_(other.operator_)
    , commandType(other.commandType)
    , varType(other.varType)
    , direction(other.direction)
{
    // Binary copies the first 24 bytes (3 qwords: operationType..value)
    // via movups, then copy-constructs name, then copies children by
    // iterating and atomic-incrementing each shared_ptr's refcount,
    // then copies the last 16 bytes (+0x48..+0x57) via movups.
    //
    // The children vector is a SHALLOW copy (refcount bump, not deep copy).
    if (!other.children.empty()) {
        children.reserve(other.children.size());
        for (const auto& child : other.children) {
            children.push_back(child);  // shared_ptr copy (refcount inc)
        }
    }
}

// ============================================================================
// Enum → string helpers
// ============================================================================

const char* str(EOperationType t) {  // 0x1c1530
    switch (t) {
        case EOperationType::eCOMMAND:      return "eCOMMAND";
        case EOperationType::eFUNCTION:     return "eFUNCTION";
        case EOperationType::eFUNCTIONCALL: return "eFUNCTIONCALL";
        case EOperationType::eVARIABLE:     return "eVARIABLE";
        case EOperationType::eOPERATOR:     return "eOPERATOR";
        case EOperationType::eARRAY:        return "eARRAY";
        case EOperationType::eARGLIST:      return "eARGLIST";
        case EOperationType::eDECLLIST:     return "eDECLLIST";
        case EOperationType::ePARAMLIST:    return "ePARAMLIST";
        case EOperationType::eSTMTLIST:     return "eSTMTLIST";
        case EOperationType::eLABEL:        return "eLABEL";
        case EOperationType::eVARIABLETYPE: return "eVARIABLETYPE";
        case EOperationType::eVALUE:        return "eVALUE";
    }
    return "UNKNOWN";
}

const char* str(EOperator op) {  // 0x1c1790
    switch (op) {
        case EOperator::eADD:    return "+";
        case EOperator::eSUB:    return "-";
        case EOperator::eMUL:    return "*";
        case EOperator::eDIV:    return "/";
        case EOperator::eMOD:    return "%";
        case EOperator::eSHL:    return "<<";
        case EOperator::eSHR:    return ">>";
        case EOperator::eGT:     return ">";
        case EOperator::eLT:     return "<";
        case EOperator::eLE:     return "<=";
        case EOperator::eGE:     return ">=";
        case EOperator::eEQ:     return "==";
        case EOperator::eNE:     return "!=";
        case EOperator::eINC:    return "++";
        case EOperator::eDEC:    return "--";
        case EOperator::eAND:    return "&";
        case EOperator::eOR:     return "|";
        case EOperator::eXOR:    return "^";
        case EOperator::eLAND:   return "&&";
        case EOperator::eLOR:    return "||";
        case EOperator::eASSIGN: return "=";
        case EOperator::eNONE:   return "";
    }
    return "UNKNOWN";
}

const char* str(ECommandType ct) {  // 0x1c18e0
    switch (ct) {
        case ECommandType::eIF:       return "eIF";
        case ECommandType::eIFELSE:   return "eIFELSE";
        case ECommandType::eSWITCH:   return "eSWITCH";
        case ECommandType::eCASE:     return "eCASE";
        case ECommandType::eFOR:      return "eFOR";
        case ECommandType::eWHILE:    return "eWHILE";
        case ECommandType::eDOWHILE:  return "eDOWHILE";
        case ECommandType::eREPEAT:   return "eREPEAT";
        case ECommandType::eCONDEXP:  return "eCONDEXP";
        case ECommandType::eCONTINUE: return "eCONTINUE";
        case ECommandType::eBREAK:    return "eBREAK";
        case ECommandType::eRETURN:   return "eRETURN";
        case ECommandType::eNEG:      return "eNEG";
        case ECommandType::ePOS:      return "ePOS";
        case ECommandType::eINV:      return "eINV";
        case ECommandType::eNOT:      return "eNOT";
        case ECommandType::eNOCMD:    return "eNOCMD";
    }
    return "UNKNOWN";
}

// ============================================================================
// createValue — 0x1bf260
// ============================================================================

Expression* createValue(SeqcParserContext* ctx, double val) {  // 0x1bf260
    auto* e = new Expression();
    // Binary writes 0x20000000c as a qword at +0x00:
    //   operationType = eVALUE (12 = 0xc), valueCategory = 2
    e->operationType = EOperationType::eVALUE;
    e->valueCategory = 2;
    e->value = val;                          // +0x10: movsd
    // +0x48: default from ctor {21, 16, 0, 2} — already set
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

// ============================================================================
// createString — 0x1bf2d0
// ============================================================================

Expression* createString(SeqcParserContext* ctx, const char* s) {  // 0x1bf2d0
    auto* e = new Expression();
    // Binary writes 0x20000000c at +0x00 (same as createValue)
    e->operationType = EOperationType::eVALUE;
    e->valueCategory = 2;
    // +0x48: loads from 0x8fc540 = {21, 16, 3, 2}
    e->operator_    = EOperator::eNONE;             // 21
    e->commandType  = ECommandType::eNOCMD;         // 16
    e->varType      = VarType_String;               // 3 — distinguishes string from numeric value
                                                     //     (correct under fixed VarType mapping;
                                                     //      previously mislabelled as VarType_Const
                                                     //      under the broken enum.)
    e->direction    = 2;
    // Construct name from the C string
    e->name = s;                                    // +0x18
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

// ============================================================================
// createVariable — 0x1bf420
// ============================================================================

Expression* createVariable(SeqcParserContext* ctx, const char* name) {  // 0x1bf420
    auto* e = new Expression();
    e->operationType = EOperationType::eVARIABLE;   // 3
    // valueCategory = 0 (default)
    // +0x48: default {21, 16, 0, 2} from 0x8fc530
    e->name = name;                                  // +0x18
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

// ============================================================================
// addVariableType — 0x1bf560
// ============================================================================

Expression* addVariableType(SeqcParserContext* ctx, Expression* expr,
                            Expression* typeExpr, bool isConst) {  // 0x1bf560
    if (!expr) {
        expr = new Expression();
        expr->operationType = EOperationType::eVARIABLETYPE;  // 0xb
    }
    if (!typeExpr) {
        typeExpr = new Expression();
        typeExpr->operationType = EOperationType::eVARIABLETYPE;
    }

    if (expr->operationType == EOperationType::eDECLLIST) {
        // Recurse on each child
        for (auto& child : expr->children) {
            addVariableType(ctx, child.get(), typeExpr, /*isConst=*/true);
        }
        if (!isConst) {
            // Destroy typeExpr
            delete typeExpr;
        }
    } else {
        // Copy varType from typeExpr to expr
        expr->varType = typeExpr->varType;          // +0x50
        if (!isConst) {
            delete typeExpr;
        } else {
            expr->lineNumber = ctx->currentLineNumber();
        }
    }
    return expr;
}

// ============================================================================
// createVariableType — 0x1bf7c0
// ============================================================================

Expression* createVariableType(SeqcParserContext* ctx, VarType vt) {  // 0x1bf7c0
    auto* e = new Expression();
    e->operationType = EOperationType::eVARIABLETYPE;  // 0xb
    // Binary writes 0x1000000015 as a qword at +0x48: operator_=21, commandType=1
    e->operator_    = EOperator::eNONE;               // 21
    e->commandType  = static_cast<ECommandType>(1);   // 1
    e->varType      = vt;                             // +0x50
    e->direction    = 2;                              // +0x54
    e->lineNumber   = ctx->currentLineNumber();
    return e;
}

// ============================================================================
// createOperator — 0x1bf830
// ============================================================================

Expression* createOperator(SeqcParserContext* ctx, Expression* lhs,
                           Expression* rhs, EOperator op) {  // 0x1bf830
    auto* e = new Expression();
    e->operationType = EOperationType::eOPERATOR;    // 4
    // Push lhs, rhs as children
    pushChild(e->children, lhs);
    pushChild(e->children, rhs);
    e->operator_ = op;                               // +0x48
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

// ============================================================================
// createAssignOperator — 0x1bf9c0
// ============================================================================

Expression* createAssignOperator(SeqcParserContext* ctx, Expression* lhs,
                                 Expression* rhs, EOperator op) {  // 0x1bf9c0
    // First create the compound operator (e.g. lhs + rhs)
    Expression* opExpr = createOperator(ctx, lhs, rhs, op);

    // Copy lhs to make a fresh node for the assignment target
    auto* lhsCopy = new Expression(*lhs);  // copy ctor @0x1bfa30

    // Create assignment: lhsCopy = opExpr  (EOperator::eASSIGN = 0x14 = 20)
    return createOperator(ctx, lhsCopy, opExpr, EOperator::eASSIGN);
}

// ============================================================================
// createListType — 0x1bfb70
// ============================================================================

Expression* createListType(SeqcParserContext* ctx, EOperationType opType,
                           Expression* lhs, Expression* rhs) {  // 0x1bfb70
    auto* e = new Expression();
    e->operationType = opType;
    // Push lhs and rhs as children
    pushChild(e->children, lhs);
    pushChild(e->children, rhs);
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

// ============================================================================
// createArray — 0x1bfb50  (thin wrapper → createListType with eARRAY)
// ============================================================================

Expression* createArray(SeqcParserContext* ctx, Expression* lhs,
                        Expression* rhs) {  // 0x1bfb50
    return createListType(ctx, EOperationType::eARRAY, lhs, rhs);
}

// ============================================================================
// createOrAppendListType — 0x1bfd20
// ============================================================================

Expression* createOrAppendListType(SeqcParserContext* ctx,
                                   EOperationType opType,
                                   Expression* lhs, Expression* rhs) {  // 0x1bfd20
    if (lhs != nullptr && lhs->operationType == opType) {
        // Append rhs to existing list
        pushChild(lhs->children, rhs);
        return lhs;
    }
    // Create a new list node wrapping both
    return createListType(ctx, opType, lhs, rhs);
}

// ============================================================================
// createOrAppend{ArgList,DeclList,ParamList,StmtList} — thin wrappers
// ============================================================================

Expression* createOrAppendArgList(SeqcParserContext* ctx,
                                  Expression* lhs, Expression* rhs) {  // 0x1bfd00
    return createOrAppendListType(ctx, EOperationType::eARGLIST, lhs, rhs);
}

Expression* createOrAppendDeclList(SeqcParserContext* ctx,
                                   Expression* lhs, Expression* rhs) {  // 0x1bfe00
    return createOrAppendListType(ctx, EOperationType::eDECLLIST, lhs, rhs);
}

Expression* createOrAppendParamList(SeqcParserContext* ctx,
                                    Expression* lhs, Expression* rhs) {  // 0x1bfe20
    return createOrAppendListType(ctx, EOperationType::ePARAMLIST, lhs, rhs);
}

Expression* createOrAppendStmtList(SeqcParserContext* ctx,
                                   Expression* lhs, Expression* rhs) {  // 0x1bfe40
    return createOrAppendListType(ctx, EOperationType::eSTMTLIST, lhs, rhs);
}

// ============================================================================
// createFunctionCall — 0x1bfe60
// ============================================================================

Expression* createFunctionCall(SeqcParserContext* ctx,
                               Expression* func, Expression* args) {  // 0x1bfe60
    auto* e = new Expression();
    // Binary writes 0x200000002 at +0x00: operationType=2, valueCategory=2
    e->operationType = EOperationType::eFUNCTIONCALL;
    e->valueCategory = 2;
    // +0x48 from 0x8fc530: {21, 16, 0, 2} — default from ctor
    pushChild(e->children, func);
    pushChild(e->children, args);
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

// ============================================================================
// createFunction — 0x1c0000
// ============================================================================

Expression* createFunction(SeqcParserContext* ctx, Expression* returnTypeExpr,
                           Expression* params, Expression* body) {  // 0x1c0000
    auto* e = new Expression();
    e->operationType = EOperationType::eFUNCTION;   // 1

    // Push params (the function_declarator, an eFUNCTIONCALL node containing
    // the function name and parameter declarations) as first child.
    // Note: despite the parameter name, 'params' is the function declarator ($2)
    // and 'nameExpr' is the type_specifier/return type ($1).
    pushChild(e->children, params);

    // Copy parameter declarations from params->children[1..] into e->children
    // (Binary iterates the declarator's children starting from index 1)
    if (params && params->children.size() > 1) {
        for (size_t i = 1; i < params->children.size(); ++i) {
            e->children.push_back(params->children[i]);
        }
        // Trim params's children to just element 0 (the function name)
        params->children.resize(1);
    }

    // Push body and return type (in that order based on disasm)
    pushChild(e->children, body);
    pushChild(e->children, returnTypeExpr);

    e->lineNumber = ctx->currentLineNumber();
    return e;
}

// ============================================================================
// createCommand — 0x1c0330  (variadic: takes N Expression* children)
// ============================================================================

Expression* createCommand(SeqcParserContext* ctx, ECommandType cmd,
                          int count, ...) {  // 0x1c0330
    auto* e = new Expression();
    e->operationType = EOperationType::eCOMMAND;    // 0
    // +0x48 = 0x15 (eNONE), +0x4C = cmd, +0x50 = 0, +0x54 = 2
    e->operator_    = EOperator::eNONE;
    e->commandType  = cmd;
    e->varType      = VarType_Unset;       // 0
    e->direction    = 2;

    va_list ap;
    va_start(ap, count);
    for (int i = 0; i < count; ++i) {
        Expression* child = va_arg(ap, Expression*);
        pushChild(e->children, child);
    }
    va_end(ap);

    e->lineNumber = ctx->currentLineNumber();
    return e;
}

// ============================================================================
// Convenience command creators — each sets a specific ECommandType
// and pushes a fixed number of children.
// ============================================================================

// Helper: create a command node with a .rodata-derived tag
static Expression* makeCommandNode(SeqcParserContext* ctx,
                                   ECommandType cmd) {
    auto* e = new Expression();
    e->operationType = EOperationType::eCOMMAND;
    e->operator_    = EOperator::eNONE;    // 21
    e->commandType  = cmd;
    e->varType      = VarType_Unset;       // 0
    e->direction    = 2;
    return e;
}

Expression* createIf(SeqcParserContext* ctx, Expression* cond,
                     Expression* body) {  // 0x1c0530
    // .rodata 0x8fc550: {21, 0, 0, 2} → commandType=eIF(0)
    auto* e = makeCommandNode(ctx, ECommandType::eIF);
    pushChild(e->children, cond);
    pushChild(e->children, body);
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

Expression* createIfElse(SeqcParserContext* ctx, Expression* cond,
                         Expression* thenBody,
                         Expression* elseBody) {  // 0x1c06c0
    // .rodata 0x8fc560: {21, 1, 0, 2} → commandType=eIFELSE(1)
    auto* e = makeCommandNode(ctx, ECommandType::eIFELSE);
    pushChild(e->children, cond);
    pushChild(e->children, thenBody);
    pushChild(e->children, elseBody);
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

Expression* createSwitch(SeqcParserContext* ctx, Expression* val,
                         Expression* body) {  // 0x1c08d0
    // .rodata 0x8fc570: {21, 2, 0, 2} → commandType=eSWITCH(2)
    auto* e = makeCommandNode(ctx, ECommandType::eSWITCH);
    pushChild(e->children, val);
    pushChild(e->children, body);
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

Expression* createCase(SeqcParserContext* ctx, Expression* val,
                       Expression* body) {  // 0x1c0a60
    // .rodata 0x8fc580: {21, 3, 0, 2} → commandType=eCASE(3)
    auto* e = makeCommandNode(ctx, ECommandType::eCASE);
    pushChild(e->children, val);
    if (body) {
        pushChild(e->children, body);
    }
    // Quirk: if body==nullptr, lineNumber = currentLineNumber() - 1
    int ln = ctx->currentLineNumber();
    e->lineNumber = (body == nullptr) ? (ln - 1) : ln;
    return e;
}

Expression* createCondExpression(SeqcParserContext* ctx, Expression* cond,
                                 Expression* trueBranch,
                                 Expression* falseBranch) {  // 0x1c0bf0
    // .rodata 0x8fc590: {21, 8, 0, 2} → commandType=eCONDEXP(8)
    auto* e = makeCommandNode(ctx, ECommandType::eCONDEXP);
    pushChild(e->children, cond);
    pushChild(e->children, trueBranch);
    pushChild(e->children, falseBranch);
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

Expression* createFor(SeqcParserContext* ctx, Expression* init,
                      Expression* cond, Expression* incr,
                      Expression* body) {  // 0x1c0e00
    // .rodata 0x8fc5a0: {21, 4, 0, 2} → commandType=eFOR(4)
    auto* e = makeCommandNode(ctx, ECommandType::eFOR);
    pushChild(e->children, init);
    pushChild(e->children, cond);
    pushChild(e->children, incr);
    pushChild(e->children, body);
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

Expression* createWhile(SeqcParserContext* ctx, Expression* cond,
                        Expression* body) {  // 0x1c1080
    // .rodata 0x8fc5b0: {21, 5, 0, 2} → commandType=eWHILE(5)
    auto* e = makeCommandNode(ctx, ECommandType::eWHILE);
    pushChild(e->children, cond);
    pushChild(e->children, body);
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

Expression* createRepeat(SeqcParserContext* ctx, Expression* count,
                         Expression* body) {  // 0x1c1210
    // .rodata 0x8fc5c0: {21, 7, 0, 2} → commandType=eREPEAT(7)
    auto* e = makeCommandNode(ctx, ECommandType::eREPEAT);
    pushChild(e->children, count);
    pushChild(e->children, body);
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

Expression* createDoWhile(SeqcParserContext* ctx, Expression* body,
                          Expression* cond) {  // 0x1c13a0
    // .rodata 0x8fc5d0: {21, 6, 0, 2} → commandType=eDOWHILE(6)
    // Note: body is pushed FIRST, then cond (reversed vs createWhile)
    auto* e = makeCommandNode(ctx, ECommandType::eDOWHILE);
    pushChild(e->children, body);
    pushChild(e->children, cond);
    e->lineNumber = ctx->currentLineNumber();
    return e;
}

// ============================================================================
// copyExpression — 0x1bef20  (deep recursive copy)
// ============================================================================

std::shared_ptr<Expression> copyExpression(
        std::shared_ptr<Expression> expr) {  // 0x1bef20
    if (!expr) {
        return nullptr;
    }

    // Allocate via make_shared (binary uses shared_ptr_emplace, 0x70 bytes)
    auto copy = std::make_shared<Expression>();

    // Copy scalar fields
    copy->operationType = expr->operationType;      // +0x00 (dword)
    copy->valueCategory = 0;                        // +0x04 zeroed initially
    copy->lineNumber    = expr->lineNumber;         // +0x08
    copy->value         = expr->value;              // +0x10
    copy->name          = expr->name;               // +0x18 (string copy)

    // Recursively deep-copy children
    for (const auto& child : expr->children) {
        copy->children.push_back(copyExpression(child));
    }

    // Copy tail fields (+0x48..+0x57, 16 bytes)
    copy->operator_    = expr->operator_;
    copy->commandType  = expr->commandType;
    copy->varType      = expr->varType;
    copy->direction    = expr->direction;

    // Copy +0x04 last (binary does this at the very end)
    copy->valueCategory = expr->valueCategory;

    return copy;
}

// ============================================================================
// seqc_error — 0x2ca1b0  (bison error callback)
// ============================================================================

int seqc_error(SeqcParserContext* ctx, Expression** /*lval*/,
               void* /*scanner*/, const char* msg) {  // 0x2ca1b0
    std::string s(msg);
    ctx->raiseError(s);
    ctx->setSyntaxError();
    return 1;
}

} // namespace zhinst
