// ============================================================================
// toSeqCAst — convert Expression tree to SeqCAstNode tree
//
// Binary: zhinst::toSeqCAst @0x1f6240 (thin wrapper)
//         (anonymous namespace)::toSeqCAstRecursor @0x1f62c0 (~3.8K lines disasm)
//
// The function is a purely mechanical recursive tree transformation.
// Each Expression node maps to exactly one SeqCAstNode subclass based on
// operationType / commandType / operator_ fields.  Children are recursively
// converted.  On null input, returns nullptr.
//
// Dispatch tables (jump tables in .rodata):
//   operationType [0..12] @ 0x95b318
//   commandType   [0..16] @ 0x95b3a4
//   operator_     [0..21] @ 0x95b34c
// ============================================================================

#include "zhinst/seqc_ast_node.hpp"
#include "zhinst/expression.hpp"

#include <memory>
#include <vector>

namespace zhinst {

namespace {

// Helper: extract the 4 common SeqCAstNode ctor args from an Expression.
// Binary passes these by reference from Expression field offsets:
//   +0x04 → EValueCategory (valueCategory)
//   +0x08 → int            (lineNumber → SeqCAstNode::lineNr_)
//   +0x50 → VarType        (varType)
//   +0x54 → EDirection     (valueType field — actually stores direction)
struct ExprArgs {
    EValueCategory vc;
    int            lineNr;
    EDirection     dir;
    VarType        vt;

    explicit ExprArgs(Expression const& e)
        : vc(static_cast<EValueCategory>(e.valueCategory))
        , lineNr(e.lineNumber)
        , dir(static_cast<EDirection>(e.valueType))  // +0x54
        , vt(e.varType)                               // +0x50
    {}
};

// Forward declaration for recursion.
std::unique_ptr<SeqCAstNode> toSeqCAstRecursor(std::shared_ptr<Expression> expr);

// Helper: recurse on children[i].  Returns nullptr if index is out of bounds.
std::unique_ptr<SeqCAstNode> recurseChild(Expression const& e, size_t i) {
    if (i >= e.children.size()) return nullptr;
    return toSeqCAstRecursor(e.children[i]);
}

// Helper: recurse on all children, building a vector of unique_ptrs.
std::vector<std::unique_ptr<SeqCAstNode>> recurseAllChildren(Expression const& e) {
    std::vector<std::unique_ptr<SeqCAstNode>> result;
    result.reserve(e.children.size());
    for (auto const& child : e.children) {
        result.push_back(toSeqCAstRecursor(child));
    }
    return result;
}

// ============================================================================
// The big switch: operationType → commandType / operator_ → SeqCAstNode
// ============================================================================
std::unique_ptr<SeqCAstNode> toSeqCAstRecursor(std::shared_ptr<Expression> expr) {  // 0x1f62c0
    if (!expr) return nullptr;

    Expression const& e = *expr;
    ExprArgs a(e);

    switch (e.operationType) {

    // ---- eCOMMAND (0) → secondary dispatch on commandType ----
    case EOperationType::eCOMMAND:
        switch (e.commandType) {
        case ECommandType::eIF:  // 0 → SeqCIfCondition (2 children: cond, body)
            return std::make_unique<SeqCIfCondition>(
                a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));

        case ECommandType::eIFELSE:  // 1 → SeqCIfElse (3 children: cond, then, else)
            return std::make_unique<SeqCIfElse>(
                a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1), recurseChild(e, 2));

        case ECommandType::eSWITCH:  // 2 → SeqCSwitchCase (2 children: expr, body)
            return std::make_unique<SeqCSwitchCase>(
                a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));

        case ECommandType::eCASE:  // 3 → SeqCCaseEntry (2 children: label, body)
            return std::make_unique<SeqCCaseEntry>(
                a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));

        case ECommandType::eFOR:  // 4 → SeqCForLoop (4 children: init, cond, incr, body)
            return std::make_unique<SeqCForLoop>(
                a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1),
                recurseChild(e, 2), recurseChild(e, 3));

        case ECommandType::eWHILE:  // 5 → SeqCWhileLoop (2 children: cond, body)
            return std::make_unique<SeqCWhileLoop>(
                a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));

        case ECommandType::eDOWHILE:  // 6 → SeqCDoWhile (2 children: body, cond)
            return std::make_unique<SeqCDoWhile>(
                a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));

        case ECommandType::eREPEAT:  // 7 → SeqCRepeat (2 children: count, body)
            return std::make_unique<SeqCRepeat>(
                a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));

        case ECommandType::eCONDEXP:  // 8 → SeqCCondExpr (3 children: cond, true, false)
            return std::make_unique<SeqCCondExpr>(
                a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1), recurseChild(e, 2));

        case ECommandType::eCONTINUE:  // 9 → SeqCContinueStatement (0 children)
            return std::make_unique<SeqCContinueStatement>(a.vc, a.lineNr, a.dir, a.vt);

        case ECommandType::eBREAK:  // 10 → SeqCBreakStatement (0 children)
            return std::make_unique<SeqCBreakStatement>(a.vc, a.lineNr, a.dir, a.vt);

        case ECommandType::eRETURN: {  // 11 → SeqCReturnStatement (1 child: expr)
            // Binary at 0x1f734c: recurses on children[0] if present, else
            // constructs SeqCReturnStatement with no child expression.
            // But the ctor requires a child — use nullptr if no children.
            auto child = recurseChild(e, 0);
            return std::make_unique<SeqCReturnStatement>(a.vc, a.lineNr, a.dir, a.vt,
                std::move(child));
        }

        case ECommandType::eNEG:  // 12 → SeqCNeg (1 child)
            return std::make_unique<SeqCNeg>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0));

        case ECommandType::ePOS:  // 13 → SeqCPos (1 child)
            return std::make_unique<SeqCPos>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0));

        case ECommandType::eINV:  // 14 → SeqCInv (1 child)
            return std::make_unique<SeqCInv>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0));

        case ECommandType::eNOT:  // 15 → SeqCNotExpr (1 child)
            return std::make_unique<SeqCNotExpr>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0));

        case ECommandType::eNOCMD:  // 16 → SeqCNoCmd (0 children)
            return std::make_unique<SeqCNoCmd>(a.vc, a.lineNr, a.dir, a.vt);

        default:
            return nullptr;  // unknown commandType
        }

    // ---- eFUNCTION (1) → SeqCFunction (4 children) ----
    // Binary at 0x1f69bc:
    //   children[0] → recurse → dynamic_cast<SeqCFunctionCall*>
    //   children[2] → recurse → body (SeqCAstNode)
    //   children[3] → recurse → dynamic_cast<SeqCVariableType*> (return type)
    //   children[1] → recurse → params (SeqCAstNode)
    case EOperationType::eFUNCTION: {
        // The binary does dynamic_cast checks and handles null results.
        // We reconstruct the same semantics: recurse all 4 children, then
        // build SeqCFunction.  The ctor takes:
        //   (vc, lineNr, dir, vt, unique_ptr<SeqCFunctionCall>, params, body, retType)
        auto call = recurseChild(e, 0);
        auto params = recurseChild(e, 1);
        auto body = recurseChild(e, 2);

        // children[3] may not exist (functions without explicit return type)
        std::unique_ptr<SeqCVariableType> retType;
        if (e.children.size() > 3) {
            auto retNode = recurseChild(e, 3);
            // dynamic_cast to SeqCVariableType — if it fails, retType stays null
            if (retNode) {
                auto* p = dynamic_cast<SeqCVariableType*>(retNode.get());
                if (p) {
                    retNode.release();
                    retType.reset(p);
                }
            }
        }

        // The call child should be a SeqCFunctionCall
        std::unique_ptr<SeqCFunctionCall> funcCall;
        if (call) {
            auto* p = dynamic_cast<SeqCFunctionCall*>(call.get());
            if (p) {
                call.release();
                funcCall.reset(p);
            }
        }

        return std::make_unique<SeqCFunction>(
            a.vc, a.lineNr, a.dir, a.vt,
            std::move(funcCall), std::move(params), std::move(body),
            std::move(retType));
    }

    // ---- eFUNCTIONCALL (2) → SeqCFunctionCall (2 children) ----
    // Binary at 0x1f674c:
    //   children[0] → recurse → dynamic_cast<SeqCVariable*> (function name)
    //   children[1] → recurse → arguments (SeqCAstNode)
    case EOperationType::eFUNCTIONCALL: {
        auto nameNode = recurseChild(e, 0);
        auto argsNode = recurseChild(e, 1);

        std::unique_ptr<SeqCVariable> funcName;
        if (nameNode) {
            auto* p = dynamic_cast<SeqCVariable*>(nameNode.get());
            if (p) {
                nameNode.release();
                funcName.reset(p);
            }
        }

        return std::make_unique<SeqCFunctionCall>(
            a.vc, a.lineNr, a.dir, a.vt,
            std::move(funcName), std::move(argsNode));
    }

    // ---- eVARIABLE (3) → SeqCVariable (name string, 0 children) ----
    case EOperationType::eVARIABLE:
        return std::make_unique<SeqCVariable>(
            a.vc, a.lineNr, a.dir, a.vt, e.name);

    // ---- eOPERATOR (4) → secondary dispatch on operator_ ----
    case EOperationType::eOPERATOR:
        switch (e.operator_) {
        case EOperator::eADD:    // 0 → SeqCPlus
            return std::make_unique<SeqCPlus>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eSUB:    // 1 → SeqCMinus
            return std::make_unique<SeqCMinus>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eMUL:    // 2 → SeqCMult
            return std::make_unique<SeqCMult>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eDIV:    // 3 → SeqCDiv
            return std::make_unique<SeqCDiv>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eMOD:    // 4 → SeqCMod
            return std::make_unique<SeqCMod>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eSHL:    // 5 → SeqCShiftL
            return std::make_unique<SeqCShiftL>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eSHR:    // 6 → SeqCShiftR
            return std::make_unique<SeqCShiftR>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eGT:     // 7 → SeqCGreater
            return std::make_unique<SeqCGreater>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eLT:     // 8 → SeqCLower
            return std::make_unique<SeqCLower>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eLE:     // 9 → SeqCLEqual
            return std::make_unique<SeqCLEqual>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eGE:     // 10 → SeqCGEqual
            return std::make_unique<SeqCGEqual>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eEQ:     // 11 → SeqCEqual
            return std::make_unique<SeqCEqual>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eNE:     // 12 → SeqCNEqual
            return std::make_unique<SeqCNEqual>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eINC:    // 13 → SeqCInc
            return std::make_unique<SeqCInc>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eDEC:    // 14 → SeqCDec
            return std::make_unique<SeqCDec>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eAND:    // 15 → SeqCAndExpr
            return std::make_unique<SeqCAndExpr>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eOR:     // 16 → SeqCOrExpr
            return std::make_unique<SeqCOrExpr>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eXOR:    // 17 → SeqCXorExpr
            return std::make_unique<SeqCXorExpr>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eLAND:   // 18 → SeqCLogAnd
            return std::make_unique<SeqCLogAnd>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eLOR:    // 19 → SeqCLogOr
            return std::make_unique<SeqCLogOr>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eASSIGN: // 20 → SeqCAssign
            return std::make_unique<SeqCAssign>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        case EOperator::eNONE:   // 21 → SeqCNoOp
            return std::make_unique<SeqCNoOp>(a.vc, a.lineNr, a.dir, a.vt,
                recurseChild(e, 0), recurseChild(e, 1));
        default:
            return nullptr;
        }

    // ---- eARRAY (5) → SeqCArray (2 children: variable, index) ----
    // Binary at 0x1f6a28:
    //   children[0] → recurse → dynamic_cast<SeqCVariable*>
    //   children[1] → recurse → index expression
    case EOperationType::eARRAY: {
        auto varNode = recurseChild(e, 0);
        auto indexNode = recurseChild(e, 1);

        // If dynamic_cast<SeqCVariable*> fails, binary sets r14=nullptr and
        // constructs SeqCArray with null variable. We mirror this.
        std::unique_ptr<SeqCVariable> var;
        if (varNode) {
            auto* p = dynamic_cast<SeqCVariable*>(varNode.get());
            if (p) {
                varNode.release();
                var.reset(p);
            }
        }

        return std::make_unique<SeqCArray>(
            a.vc, a.lineNr, a.dir, a.vt,
            std::move(var), std::move(indexNode));
    }

    // ---- eARGLIST (6) → SeqCArgList (vector of children) ----
    case EOperationType::eARGLIST:
        return std::make_unique<SeqCArgList>(a.vc, a.lineNr, a.dir, a.vt,
            recurseAllChildren(e));

    // ---- eDECLLIST (7) → SeqCDeclList (vector of children) ----
    case EOperationType::eDECLLIST:
        return std::make_unique<SeqCDeclList>(a.vc, a.lineNr, a.dir, a.vt,
            recurseAllChildren(e));

    // ---- ePARAMLIST (8) → SeqCParamList (vector of children) ----
    case EOperationType::ePARAMLIST:
        return std::make_unique<SeqCParamList>(a.vc, a.lineNr, a.dir, a.vt,
            recurseAllChildren(e));

    // ---- eSTMTLIST (9) → SeqCStmtList (vector of children) ----
    case EOperationType::eSTMTLIST:
        return std::make_unique<SeqCStmtList>(a.vc, a.lineNr, a.dir, a.vt,
            recurseAllChildren(e));

    // ---- eLABEL (10) → SeqCLabel (0 children) ----
    case EOperationType::eLABEL:
        return std::make_unique<SeqCLabel>(a.vc, a.lineNr, a.dir, a.vt);

    // ---- eVARIABLETYPE (11) → SeqCVariableType (0 children) ----
    case EOperationType::eVARIABLETYPE:
        return std::make_unique<SeqCVariableType>(a.vc, a.lineNr, a.dir, a.vt);

    // ---- eVALUE (12) → SeqCValue (string or double) ----
    // Binary at 0x1f6548: checks varType == VarType_String (3).
    // If string: SeqCValue(vc, lineNr, dir, vt, name)
    // If numeric: SeqCValue(vc, lineNr, dir, vt, value)
    case EOperationType::eVALUE:
        if (e.varType == VarType_String) {
            return std::make_unique<SeqCValue>(a.vc, a.lineNr, a.dir, a.vt, e.name);
        } else {
            return std::make_unique<SeqCValue>(a.vc, a.lineNr, a.dir, a.vt, e.value);
        }

    default:
        // Binary at 0x1f6538: returns nullptr for unknown operationType
        return nullptr;
    }
}

}  // anonymous namespace

// 0x1f6240 — thin wrapper that copies the shared_ptr and calls the recursor
std::shared_ptr<SeqCAstNode> toSeqCAst(std::shared_ptr<Expression> expr) {
    auto result = toSeqCAstRecursor(std::move(expr));
    // Convert unique_ptr to shared_ptr (the binary returns shared_ptr)
    return std::shared_ptr<SeqCAstNode>(std::move(result));
}

}  // namespace zhinst
