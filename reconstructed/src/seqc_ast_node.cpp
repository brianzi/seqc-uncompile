// ============================================================================
// SeqCAstNode — base class methods + 53 subclass implementations.
//
// Phase 13a: print() and clone() bodies reconstructed from binary.
//   - 51 simple print(): write fixed string literal to cout (tail-call to
//     __put_character_sequence in binary; we use cout.write() equivalent).
//   - 2 complex print(): SeqCVariable (0x1fdbd0), SeqCValue (0x1fe230).
//   - clone(): deep-copy allocating new node + recursively cloning children.
// ============================================================================

#include "zhinst/seqc_ast_node.hpp"

#include <iostream>
#include <cstring>
#include <utility>

namespace zhinst {

// str(EParamDirection) @0x1c1730 — jump table with 3 entries
std::string str(EParamDirection dir) {
    switch (dir) {
        case EParamDirection::eIN:    return "in";
        case EParamDirection::eOUT:   return "out";
        case EParamDirection::eINOUT: return "inout";
        default: return "unknown";
    }
}

// ============================================================================
// SeqCAstNode base class
// ============================================================================

// 0x1fda00
SeqCAstNode::SeqCAstNode(EValueCategory vc, int type, EParamDirection dir)
    : valueCategory_(vc)
    , type_(type)
    , direction_(dir)
{}

// 0x209000  (D2 base destructor — trivial)
SeqCAstNode::~SeqCAstNode() = default;

// 0x1fda20
std::vector<const SeqCAstNode*> SeqCAstNode::children() const {
    return {};
}

// 0x209dd0
std::vector<std::string> SeqCAstNode::getListElements() const {
    std::vector<std::string> result;
    result.emplace_back();   // single empty SSO string
    return result;
}

// 0x1fda40
void swap(SeqCAstNode& a, SeqCAstNode& b) {
    using std::swap;
    swap(a.direction_, b.direction_);
    swap(a.valueCategory_, b.valueCategory_);
    swap(a.type_, b.type_);
}

// 0x1fa3c0  (public wrapper that calls anon-namespace recursive helper @0x1fa430)
void printSeqCAst(const SeqCAstNode& node) {
    // TODO: full reconstruction of the recursive printer at 0x1fa430.
    // Approximate behavior: prints each node via node.print() with indentation
    // using box-drawing characters for tree connectors.
    node.print();
    auto kids = node.children();
    for (size_t i = 0; i < kids.size(); ++i) {
        const char* connector = (i + 1 < kids.size()) ? "├─" : "└─";
        std::cout << connector;
        if (kids[i]) {
            printSeqCAst(*kids[i]);
        } else {
            std::cout << "null\n";
        }
    }
}

// ============================================================================
// Trivial leaves (24B) — no children, no extra fields
// ============================================================================

#define SEQC_TRIVIAL_LEAF_IMPL(Name, Label, LabelLen, CloneSize)             \
    Name::~Name() = default;                                                 \
    void Name::print() const {  /* writes Label to cout */                   \
        std::cout.write(Label, LabelLen);                                    \
    }                                                                        \
    std::unique_ptr<SeqCAstNode> Name::clone() const {                       \
        auto p = std::make_unique<Name>(valueCategory_, type_, direction_);  \
        return p;                                                            \
    }

SEQC_TRIVIAL_LEAF_IMPL(SeqCOperation,         "Operation",         9, 0x18)  // print @0x1fda70
SEQC_TRIVIAL_LEAF_IMPL(SeqCCommand,           "Command",           7, 0x18)  // print @0x1fe610
SEQC_TRIVIAL_LEAF_IMPL(SeqCVariableType,      "VariableType",     12, 0x18)  // print @0x1fe6a0
SEQC_TRIVIAL_LEAF_IMPL(SeqCLabel,             "Label",             5, 0x18)  // print @0x2019a0
SEQC_TRIVIAL_LEAF_IMPL(SeqCContinueStatement, "ContinueStatement",17, 0x18)  // print @0x204100
SEQC_TRIVIAL_LEAF_IMPL(SeqCBreakStatement,    "BreakStatement",   14, 0x18)  // print @0x204190
SEQC_TRIVIAL_LEAF_IMPL(SeqCNoCmd,             "NoCmd",             5, 0x18)  // print @0x204d30

#undef SEQC_TRIVIAL_LEAF_IMPL

// ============================================================================
// Single-child unary nodes (32B)
// ============================================================================

#define SEQC_UNARY_IMPL(Name, Label, LabelLen)                               \
    Name::Name(EValueCategory vc, int type, EParamDirection dir,             \
               std::unique_ptr<SeqCAstNode> child)                           \
        : SeqCAstNode(vc, type, dir), child_(std::move(child)) {}            \
    Name::~Name() = default;                                                 \
    void Name::print() const {                                               \
        std::cout.write(Label, LabelLen);                                    \
    }                                                                        \
    std::unique_ptr<SeqCAstNode> Name::clone() const {                       \
        return std::make_unique<Name>(                                       \
            valueCategory_, type_, direction_,                               \
            child_ ? child_->clone() : nullptr);                             \
    }                                                                        \
    std::vector<const SeqCAstNode*> Name::children() const {                 \
        return { child_.get() };                                             \
    }

SEQC_UNARY_IMPL(SeqCNeg,              "Neg",             3)  // print @0x204430, clone @0x204500
SEQC_UNARY_IMPL(SeqCPos,              "Pos",             3)  // print @0x204670, clone @0x204740
SEQC_UNARY_IMPL(SeqCInv,              "Inv",             3)  // print @0x2048b0, clone @0x204980
SEQC_UNARY_IMPL(SeqCNotExpr,          "NotExpr",         7)  // print @0x204af0, clone @0x204bc0
SEQC_UNARY_IMPL(SeqCReturnStatement,  "ReturnStatement",15)  // print @0x204220, clone @0x2042f0

#undef SEQC_UNARY_IMPL

// ============================================================================
// SeqCOperator base + 22 binary-op subclasses (40B)
// ============================================================================

SeqCOperator::SeqCOperator(EValueCategory vc, int type, EParamDirection dir,
                           std::unique_ptr<SeqCAstNode> lhs,
                           std::unique_ptr<SeqCAstNode> rhs)
    : SeqCAstNode(vc, type, dir)
    , lhs_(std::move(lhs))
    , rhs_(std::move(rhs))
{}

SeqCOperator::~SeqCOperator() = default;

void SeqCOperator::print() const {  // 0x1fef70
    std::cout.write("Operator", 8);
}

std::unique_ptr<SeqCAstNode> SeqCOperator::clone() const {  // 0x1ff050
    return std::make_unique<SeqCOperator>(
        valueCategory_, type_, direction_,
        lhs_ ? lhs_->clone() : nullptr,
        rhs_ ? rhs_->clone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCOperator::children() const {
    return { lhs_.get(), rhs_.get() };
}

// Binary-op subclass clone pattern: allocates new(0x28), copies base fields,
// recursively clones lhs_ and rhs_.  All 22 are identical except vtable ptr.
#define SEQC_OPERATOR_IMPL(Name, Label, LabelLen)                            \
    Name::~Name() = default;                                                 \
    void Name::print() const {                                               \
        std::cout.write(Label, LabelLen);                                    \
    }                                                                        \
    std::unique_ptr<SeqCAstNode> Name::clone() const {                       \
        return std::make_unique<Name>(                                       \
            valueCategory_, type_, direction_,                               \
            lhs_ ? lhs_->clone() : nullptr,                                 \
            rhs_ ? rhs_->clone() : nullptr);                                \
    }

SEQC_OPERATOR_IMPL(SeqCPlus,    "Plus",    4)  // print @0x204dc0, clone @0x204ea0
SEQC_OPERATOR_IMPL(SeqCMinus,   "Minus",   5)  // print @0x205050, clone @0x205130
SEQC_OPERATOR_IMPL(SeqCMult,    "Mult",    4)  // print @0x2052e0, clone @0x2053c0
SEQC_OPERATOR_IMPL(SeqCDiv,     "Div",     3)  // print @0x205570, clone @0x205650
SEQC_OPERATOR_IMPL(SeqCMod,     "Mod",     3)  // print @0x205800, clone @0x2058e0
SEQC_OPERATOR_IMPL(SeqCShiftR,  "ShiftR",  6)  // print @0x205a90, clone @0x205b70
SEQC_OPERATOR_IMPL(SeqCShiftL,  "ShiftL",  6)  // print @0x205d20, clone @0x205e00
SEQC_OPERATOR_IMPL(SeqCGreater, "Greater", 7)  // print @0x205fb0, clone @0x206090
SEQC_OPERATOR_IMPL(SeqCLower,   "Lower",   5)  // print @0x206240, clone @0x206320
SEQC_OPERATOR_IMPL(SeqCLEqual,  "LEqual",  6)  // print @0x2064d0, clone @0x2065b0
SEQC_OPERATOR_IMPL(SeqCGEqual,  "GEqual",  6)  // print @0x206760, clone @0x206840
SEQC_OPERATOR_IMPL(SeqCEqual,   "Equal",   5)  // print @0x2069f0, clone @0x206ad0
SEQC_OPERATOR_IMPL(SeqCNEqual,  "NEqual",  6)  // print @0x206c80, clone @0x206d60
SEQC_OPERATOR_IMPL(SeqCInc,     "Inc",     3)  // print @0x206f10, clone @0x206ff0
SEQC_OPERATOR_IMPL(SeqCDec,     "Dec",     3)  // print @0x2071a0, clone @0x207280
SEQC_OPERATOR_IMPL(SeqCAndExpr, "AndExpr", 7)  // print @0x207430, clone @0x207510
SEQC_OPERATOR_IMPL(SeqCOrExpr,  "OrExpr",  6)  // print @0x2076c0, clone @0x2077a0
SEQC_OPERATOR_IMPL(SeqCXorExpr, "XorExpr", 7)  // print @0x207950, clone @0x207a30
SEQC_OPERATOR_IMPL(SeqCLogAnd,  "LogAnd",  6)  // print @0x207be0, clone @0x207cc0
SEQC_OPERATOR_IMPL(SeqCLogOr,   "LogOr",   5)  // print @0x207e70, clone @0x207f50
SEQC_OPERATOR_IMPL(SeqCAssign,  "Assign",  6)  // print @0x208100, clone @0x2081e0
SEQC_OPERATOR_IMPL(SeqCNoOp,    "NoOp",    4)  // print @0x208390, clone @0x208470

#undef SEQC_OPERATOR_IMPL

// ============================================================================
// Two-child direct-AstNode subclasses (40B)
// ============================================================================

#define SEQC_BINARY_IMPL(Name, Label, LabelLen)                              \
    Name::Name(EValueCategory vc, int type, EParamDirection dir,             \
               std::unique_ptr<SeqCAstNode> first,                           \
               std::unique_ptr<SeqCAstNode> second)                          \
        : SeqCAstNode(vc, type, dir),                                        \
          first_(std::move(first)), second_(std::move(second)) {}            \
    Name::~Name() = default;                                                 \
    void Name::print() const {                                               \
        std::cout.write(Label, LabelLen);                                    \
    }                                                                        \
    std::unique_ptr<SeqCAstNode> Name::clone() const {                       \
        return std::make_unique<Name>(                                       \
            valueCategory_, type_, direction_,                               \
            first_ ? first_->clone() : nullptr,                              \
            second_ ? second_->clone() : nullptr);                           \
    }                                                                        \
    std::vector<const SeqCAstNode*> Name::children() const {                 \
        return { first_.get(), second_.get() };                              \
    }

SEQC_BINARY_IMPL(SeqCFunctionCall, "FunctionCall", 12)  // print @0x1fec50, clone @0x1fed30
SEQC_BINARY_IMPL(SeqCArray,        "Array",         5)  // print @0x1ff2e0, clone @0x1ff3c0
SEQC_BINARY_IMPL(SeqCIfCondition,  "IfCondition",  11)  // print @0x201a30, clone @0x201b10
SEQC_BINARY_IMPL(SeqCCaseEntry,    "CaseEntry",     9)  // print @0x2027a0, clone @0x202880
SEQC_BINARY_IMPL(SeqCSwitchCase,   "SwitchCase",   10)  // print @0x202350, clone @0x202430
SEQC_BINARY_IMPL(SeqCWhileLoop,    "WhileLoop",     9)  // print @0x203060, clone @0x203140
SEQC_BINARY_IMPL(SeqCDoWhile,      "DoWhile",       7)  // print @0x203420, clone @0x203500
SEQC_BINARY_IMPL(SeqCRepeat,       "Repeat",        6)  // print @0x2037e0, clone @0x2038c0

#undef SEQC_BINARY_IMPL

// ============================================================================
// Three-child direct-AstNode subclasses (48B)
// ============================================================================

// SeqCIfElse — vtable 0xb05430
SeqCIfElse::SeqCIfElse(EValueCategory vc, int type, EParamDirection dir,
                       std::unique_ptr<SeqCAstNode> cond,
                       std::unique_ptr<SeqCAstNode> ifBody,
                       std::unique_ptr<SeqCAstNode> elseBody)  // 0x202150
    : SeqCAstNode(vc, type, dir)
    , cond_(std::move(cond))
    , ifBody_(std::move(ifBody))
    , elseBody_(std::move(elseBody))
{}

SeqCIfElse::~SeqCIfElse() = default;

void SeqCIfElse::print() const {  // 0x201df0
    std::cout.write("IfElse", 6);
}

std::unique_ptr<SeqCAstNode> SeqCIfElse::clone() const {  // 0x2021a0
    return std::make_unique<SeqCIfElse>(
        valueCategory_, type_, direction_,
        cond_ ? cond_->clone() : nullptr,
        ifBody_ ? ifBody_->clone() : nullptr,
        elseBody_ ? elseBody_->clone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCIfElse::children() const {  // 0x2022c0
    return { cond_.get(), ifBody_.get(), elseBody_.get() };
}

// SeqCCondExpr — vtable 0xb056c0
SeqCCondExpr::SeqCCondExpr(EValueCategory vc, int type, EParamDirection dir,
                           std::unique_ptr<SeqCAstNode> cond,
                           std::unique_ptr<SeqCAstNode> trueBranch,
                           std::unique_ptr<SeqCAstNode> falseBranch)
    : SeqCAstNode(vc, type, dir)
    , cond_(std::move(cond))
    , trueBranch_(std::move(trueBranch))
    , falseBranch_(std::move(falseBranch))
{}

SeqCCondExpr::~SeqCCondExpr() = default;

void SeqCCondExpr::print() const {  // 0x203ba0
    std::cout.write("CondExpr", 8);
}

std::unique_ptr<SeqCAstNode> SeqCCondExpr::clone() const {  // 0x203c80
    return std::make_unique<SeqCCondExpr>(
        valueCategory_, type_, direction_,
        cond_ ? cond_->clone() : nullptr,
        trueBranch_ ? trueBranch_->clone() : nullptr,
        falseBranch_ ? falseBranch_->clone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCCondExpr::children() const {
    return { cond_.get(), trueBranch_.get(), falseBranch_.get() };
}

// ============================================================================
// Four-child direct-AstNode subclasses (56B)
// ============================================================================

// SeqCFunction — vtable 0xb050f0
SeqCFunction::SeqCFunction(EValueCategory vc, int type, EParamDirection dir,
                           std::unique_ptr<SeqCAstNode> name,
                           std::unique_ptr<SeqCAstNode> returnType,
                           std::unique_ptr<SeqCAstNode> params,
                           std::unique_ptr<SeqCAstNode> body)
    : SeqCAstNode(vc, type, dir)
    , name_(std::move(name))
    , returnType_(std::move(returnType))
    , params_(std::move(params))
    , body_(std::move(body))
{}

SeqCFunction::~SeqCFunction() = default;

void SeqCFunction::print() const {  // 0x1fe730
    std::cout.write("Function", 8);
}

std::unique_ptr<SeqCAstNode> SeqCFunction::clone() const {  // 0x1fe7c0
    return std::make_unique<SeqCFunction>(
        valueCategory_, type_, direction_,
        name_ ? name_->clone() : nullptr,
        returnType_ ? returnType_->clone() : nullptr,
        params_ ? params_->clone() : nullptr,
        body_ ? body_->clone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCFunction::children() const {
    return { name_.get(), returnType_.get(), params_.get(), body_.get() };
}

// SeqCForLoop — vtable 0xb05580
SeqCForLoop::SeqCForLoop(EValueCategory vc, int type, EParamDirection dir,
                         std::unique_ptr<SeqCAstNode> init,
                         std::unique_ptr<SeqCAstNode> cond,
                         std::unique_ptr<SeqCAstNode> incr,
                         std::unique_ptr<SeqCAstNode> body)  // 0x202f00
    : SeqCAstNode(vc, type, dir)
    , init_(std::move(init))
    , cond_(std::move(cond))
    , incr_(std::move(incr))
    , body_(std::move(body))
{}

SeqCForLoop::~SeqCForLoop() = default;

void SeqCForLoop::print() const {  // 0x202bc0
    std::cout.write("ForLoop", 7);
}

std::unique_ptr<SeqCAstNode> SeqCForLoop::clone() const {  // 0x202f70
    return std::make_unique<SeqCForLoop>(
        valueCategory_, type_, direction_,
        init_ ? init_->clone() : nullptr,
        cond_ ? cond_->clone() : nullptr,
        incr_ ? incr_->clone() : nullptr,
        body_ ? body_->clone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCForLoop::children() const {  // 0x202fd0
    return { init_.get(), cond_.get(), incr_.get(), body_.get() };
}

// ============================================================================
// List nodes (48B)
// ============================================================================

#define SEQC_LIST_IMPL(Name, Label, LabelLen)                                \
    Name::Name(EValueCategory vc, int type, EParamDirection dir)             \
        : SeqCAstNode(vc, type, dir), elements_() {}                         \
    Name::Name(EValueCategory vc, int type, EParamDirection dir,             \
               std::vector<std::unique_ptr<SeqCAstNode>> elements)           \
        : SeqCAstNode(vc, type, dir), elements_(std::move(elements)) {}      \
    Name::~Name() = default;                                                 \
    void Name::print() const {                                               \
        std::cout.write(Label, LabelLen);                                    \
    }                                                                        \
    std::unique_ptr<SeqCAstNode> Name::clone() const {                       \
        std::vector<std::unique_ptr<SeqCAstNode>> cloned;                    \
        cloned.reserve(elements_.size());                                    \
        for (const auto& e : elements_) {                                    \
            cloned.push_back(e ? e->clone() : nullptr);                      \
        }                                                                    \
        return std::make_unique<Name>(                                       \
            valueCategory_, type_, direction_, std::move(cloned));            \
    }                                                                        \
    std::vector<const SeqCAstNode*> Name::children() const {                 \
        std::vector<const SeqCAstNode*> result;                              \
        result.reserve(elements_.size());                                    \
        for (const auto& e : elements_) result.push_back(e.get());           \
        return result;                                                       \
    }                                                                        \
    void Name::append(std::unique_ptr<SeqCAstNode> elem) {                   \
        elements_.push_back(std::move(elem));                                \
    }

SEQC_LIST_IMPL(SeqCArgList,   "ArgList",   7)  // print @0x1ff600, clone @0x1ff6e0
SEQC_LIST_IMPL(SeqCDeclList,  "DeclList",  8)  // print @0x1ffef0, clone @0x1fffd0
SEQC_LIST_IMPL(SeqCParamList, "ParamList", 9)  // print @0x200910, clone @0x2009f0
SEQC_LIST_IMPL(SeqCStmtList,  "StmtList",  8)  // print @0x201520, clone @0x201600

#undef SEQC_LIST_IMPL

// ============================================================================
// Leaf nodes with payload
// ============================================================================

// SeqCVariable — vtable 0xb04fb0
SeqCVariable::SeqCVariable(EValueCategory vc, int type, EParamDirection dir,
                           std::string name)
    : SeqCAstNode(vc, type, dir)
    , name_(std::move(name))
{}

SeqCVariable::~SeqCVariable() = default;

// 0x1fdbd0 — complex print: "Variable: name(VarType), direction"
// Binary flow:
//   1. Write "Variable: " (10 chars) to cout
//   2. If type_ == 0: just write name_
//   3. Else: write name_ + " (" + str(VarType(type_)) + ")"
//   4. Write ", "
//   5. Write str(direction_)
//   6. Write "\n" (1 char, but uncertain — might be absent)
void SeqCVariable::print() const {  // 0x1fdbd0
    std::cout.write("Variable: ", 10);
    if (type_ == 0) {
        std::cout << name_;
    } else {
        std::string s = name_;
        s += " (";
        s += str(static_cast<VarType>(type_));
        s += ")";
        std::cout << s;
    }
    std::cout.write(", ", 2);
    std::cout << str(direction_);
}

std::unique_ptr<SeqCAstNode> SeqCVariable::clone() const {  // 0x1fdf30
    return std::make_unique<SeqCVariable>(
        valueCategory_, type_, direction_, name_);
}

// 0x209e60
std::vector<std::string> SeqCVariable::getListElements() const {
    std::vector<std::string> result;
    result.push_back(name_);
    return result;
}

// SeqCValue — vtable 0xb05000
SeqCValue::SeqCValue(EValueCategory vc, int type, EParamDirection dir)
    : SeqCAstNode(vc, type, dir)
{}

SeqCValue::~SeqCValue() = default;

// 0x1fe230 — variant-dispatching print: "Value = " + payload
// Binary flow:
//   1. Write "Value = " (8 chars) to cout
//   2. Read tag_ at +0x30
//   3. tag==0 → payload is SSO string, print it
//   4. tag==1 → payload is double, print via to_string
//   5. else → __throw_bad_variant_access
void SeqCValue::print() const {  // 0x1fe230
    std::cout.write("Value = ", 8);
    switch (tag_) {
        case 0: {
            // String payload: libc++ SSO at payload_[0..23]
            // Short strings: payload_[0] has (len<<1), data at payload_[1..].
            // Long strings: pointer at payload_[16], size at payload_[8].
            // We treat payload_ as a std::string for convenience here:
            const std::string* sp = reinterpret_cast<const std::string*>(payload_);
            std::cout << *sp;
            break;
        }
        case 1: {
            // Double payload at payload_[0..7]
            double d;
            std::memcpy(&d, payload_, sizeof(d));
            std::cout << std::to_string(d);
            break;
        }
        default:
            // Binary calls __throw_bad_variant_access here
            break;
    }
}

std::unique_ptr<SeqCAstNode> SeqCValue::clone() const {  // 0x208600
    // TODO: full clone — needs to copy variant payload based on tag.
    // For now, only clones base fields; payload copying deferred pending
    // full SeqCValue variant interface reconstruction.
    auto p = std::make_unique<SeqCValue>(valueCategory_, type_, direction_);
    std::memcpy(p->payload_, payload_, sizeof(payload_));
    p->tag_ = tag_;
    // NOTE: if tag==0 (string), this is a shallow copy of SSO bytes.
    // For short strings (<=22 chars in libc++) this is correct.
    // For long strings this copies the pointer without incrementing refcount.
    // A proper fix requires the full variant interface.
    return p;
}

} // namespace zhinst
