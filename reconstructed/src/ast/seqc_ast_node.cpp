// ============================================================================
// SeqCAstNode — base class methods + 53 subclass implementations.
//
// print() and doClone() bodies reconstructed from binary.
//   - 51 simple print(): write fixed string literal to cout (tail-call to
//     __put_character_sequence in binary; we use cout.write() equivalent).
//   - 2 complex print(): SeqCVariable (0x1fdbd0), SeqCValue (0x1fe230).
//   - doClone(): deep-copy allocating new node + recursively cloning children.
// ============================================================================

#include "zhinst/ast/seqc_ast_node.hpp"

#include <iostream>
#include <cstring>
#include <utility>

#include "zhinst/ast/eval_results.hpp"
#include "zhinst/ast/frontend_lowering.hpp"

namespace zhinst {

// str(EValueCategory) @0x1c16c0 — 3 cases
//! \brief Returns the canonical string form of an `EValueCategory`:
//! `"eNOVALUECATEGORY"`, `"eLVALUE"`, or `"eRVALUE"`.
//! \param vc Value-category tag to render.
//! \return Canonical string form of `vc`, or an empty string for any
//! value outside the recognised set.
std::string str(EValueCategory vc) {
    switch (vc) {
        case EValueCategory::eNOVALUECATEGORY: return "eNOVALUECATEGORY";
        case EValueCategory::eLVALUE:            return "eLVALUE";
        case EValueCategory::eRVALUE:            return "eRVALUE";
        default: return {};
    }
}

// str(EDirection) @0x1c1730 — jump table with 3 entries
//! \brief Returns the canonical string form of an `EDirection`:
//! `"in"`, `"out"`, or `"inout"`.
//! \param dir Parameter-direction tag to render.
//! \return Canonical string form of `dir`, or the literal `"unknown"`
//! for any value outside the recognised set.
std::string str(EDirection dir) {
    switch (dir) {
        case EDirection::eIN:    return "in";
        case EDirection::eOUT:   return "out";
        case EDirection::eINOUT: return "inout";
        default: return "unknown";
    }
}

// ============================================================================
// SeqCAstNode base class
// ============================================================================

// 0x1fda00
SeqCAstNode::SeqCAstNode(EValueCategory vc, int lineNr, EDirection dir)
    : valueCategory_(vc)
    , lineNr_(lineNr)
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

// Base evaluate() moved to seqc_ast_nodes_evaluate.cpp (binary TU:
// SeqCAstNodesEvaluate.cpp).

// 0x1fdb40 — returns vector with single element str(varType_).
// ICF-merged across all trivial leaves in the binary.
std::vector<std::string> SeqCAstNode::getVarTypes() const
{
    std::vector<std::string> result;
    result.push_back(str(varType_));
    return result;
}

// 0x1fda40
void swap(SeqCAstNode& a, SeqCAstNode& b) {
    using std::swap;
    swap(a.direction_, b.direction_);
    swap(a.valueCategory_, b.valueCategory_);
    swap(a.lineNr_, b.lineNr_);
}

// 0x1fa3c0  (public wrapper that calls anon-namespace recursive helper @0x1fa430)
namespace {
// @0x1fa430 — recursive AST tree printer using box-drawing connectors
void printSeqCAstImpl(SeqCAstNode const& node, std::string const& prefix) {
    // Print node type name via virtual print(), then " (line: <lineNr_>)\n"
    node.print();                                                  // @0x1fa44a: call *0x18(%rax)
    std::cout << " (line: " << node.lineNr() << ")" << "\n";         // @0x1fa454..0x1fa497

    // Get children via virtual children()                         // @0x1fa4a6: call *0x10(%rax)
    auto kids = node.children();
    for (size_t i = 0; i < kids.size(); ++i) {
        // Print prefix (indent string)                            // @0x1fa538
        std::cout << prefix;
        // Print connector: "`-" for last child, "|-" for others   // @0x1fa54e
        bool isLast = (i + 1 >= kids.size());
        std::cout << (isLast ? "`-" : "|-");
        if (kids[i]) {
            // Build new prefix: append "  " for last, "| " for others
            // (deduced from the recursive call pattern with prefix+1)
            std::string childPrefix = prefix + (isLast ? "  " : "| ");
            printSeqCAstImpl(*kids[i], childPrefix);               // @0x1fa630 (recursive call)
        } else {
            std::cout << "NULL\n";                                 // @0x1fa590
        }
    }
}
}  // anonymous namespace

void printSeqCAst(const SeqCAstNode& node) {                      // @0x1fa3c0
    std::string emptyPrefix;
    printSeqCAstImpl(node, emptyPrefix);
}

// ============================================================================
// Trivial leaves (24B) — no children, no extra fields
// ============================================================================

#define SEQC_TRIVIAL_LEAF_IMPL(Name, Label, LabelLen, CloneSize)             \
    Name::Name(Name const& o) : SeqCAstNode(o.valueCategory_, o.lineNr_,      \
        o.direction_) { varType_ = o.varType_; }                              \
    Name& Name::operator=(Name o) { swap(*this, o); return *this; }           \
    Name::~Name() = default;                                                 \
    void Name::print() const {  /* writes Label to cout */                   \
        std::cout.write(Label, LabelLen);                                    \
    }                                                                        \
    std::unique_ptr<SeqCAstNode> Name::doClone() const {                       \
        return std::make_unique<Name>(valueCategory_, lineNr_, direction_,     \
                                     varType_);                              \
    }                                                                        \
    void swap(Name& a, Name& b) {                                           \
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));    \
    }

// SeqCOperation — broken out of SEQC_TRIVIAL_LEAF for getVarTypes override
SeqCOperation::SeqCOperation(EValueCategory vc, int lineNr, EDirection dir,
                             VarType vt)
    : SeqCAstNode(vc, lineNr, dir)
{
    varType_ = vt;
}

SeqCOperation::SeqCOperation(SeqCOperation const& o)
    : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_) { varType_ = o.varType_; }
SeqCOperation& SeqCOperation::operator=(SeqCOperation o) { swap(*this, o); return *this; }
SeqCOperation::~SeqCOperation() = default;
void SeqCOperation::print() const { std::cout.write("Operation", 9); }  // @0x1fda70
std::unique_ptr<SeqCAstNode> SeqCOperation::doClone() const {
    return std::make_unique<SeqCOperation>(valueCategory_, lineNr_, direction_, varType_);
}
//! \brief ADL-friendly free-function swap for `SeqCOperation`:
//! exchanges the `SeqCAstNode` base subobject of `a` and `b`
//! (operation nodes carry no extra owned fields).
//! \param a First operation node.
//! \param b Second operation node.
void swap(SeqCOperation& a, SeqCOperation& b) {
    swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
}
std::vector<std::string> SeqCOperation::getVarTypes() const {
    std::vector<std::string> result;
    result.push_back(str(varType_));
    return result;
}

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
    Name::Name(EValueCategory vc, int lineNr, EDirection dir,             \
               VarType vt,                                                   \
               std::unique_ptr<SeqCAstNode> child)                           \
        : SeqCAstNode(vc, lineNr, dir), child_(std::move(child))               \
    { varType_ = vt; }                                                       \
    Name::Name(Name const& o)                                                \
        : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_),              \
          child_(o.child_ ? o.child_->doClone() : nullptr)                     \
    { varType_ = o.varType_; }                                               \
    Name& Name::operator=(Name o) { swap(*this, o); return *this; }          \
    Name::~Name() = default;                                                 \
    void Name::print() const {                                               \
        std::cout.write(Label, LabelLen);                                    \
    }                                                                        \
    std::unique_ptr<SeqCAstNode> Name::doClone() const {                       \
        return std::make_unique<Name>(                                       \
            valueCategory_, lineNr_, direction_, varType_,                     \
            child_ ? child_->doClone() : nullptr);                             \
    }                                                                        \
    std::vector<const SeqCAstNode*> Name::children() const {                 \
        return { child_.get() };                                             \
    }                                                                        \
    void swap(Name& a, Name& b) {                                           \
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));    \
        std::swap(a.child_, b.child_);                                       \
    }

SEQC_UNARY_IMPL(SeqCNeg,              "Neg",             3)  // print @0x204430, doClone @0x204500
SEQC_UNARY_IMPL(SeqCPos,              "Pos",             3)  // print @0x204670, doClone @0x204740
SEQC_UNARY_IMPL(SeqCInv,              "Inv",             3)  // print @0x2048b0, doClone @0x204980
SEQC_UNARY_IMPL(SeqCNotExpr,          "NotExpr",         7)  // print @0x204af0, doClone @0x204bc0
SEQC_UNARY_IMPL(SeqCReturnStatement,  "ReturnStatement",15)  // print @0x204220, doClone @0x2042f0

#undef SEQC_UNARY_IMPL

// ============================================================================
// SeqCOperator base + 22 binary-op subclasses (40B)
// ============================================================================

SeqCOperator::SeqCOperator(EValueCategory vc, int lineNr, EDirection dir,
                           VarType vt,
                           std::unique_ptr<SeqCAstNode> lhs,
                           std::unique_ptr<SeqCAstNode> rhs)
    : SeqCAstNode(vc, lineNr, dir)
    , lhs_(std::move(lhs))
    , rhs_(std::move(rhs))
{
    varType_ = vt;
}

SeqCOperator::~SeqCOperator() = default;

SeqCOperator::SeqCOperator(SeqCOperator const& o)
    : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_)
    , lhs_(o.lhs_ ? o.lhs_->doClone() : nullptr)
    , rhs_(o.rhs_ ? o.rhs_->doClone() : nullptr)
{ varType_ = o.varType_; }

void SeqCOperator::print() const {  // 0x1fef70
    std::cout.write("Operator", 8);
}

std::unique_ptr<SeqCAstNode> SeqCOperator::doClone() const {  // 0x1ff050
    return std::make_unique<SeqCOperator>(
        valueCategory_, lineNr_, direction_, varType_,
        lhs_ ? lhs_->doClone() : nullptr,
        rhs_ ? rhs_->doClone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCOperator::children() const {
    return { lhs_.get(), rhs_.get() };
}

SeqCOperator& SeqCOperator::operator=(SeqCOperator o) {
    swap(*this, o); return *this;
}

//! \brief ADL-friendly free-function swap for `SeqCOperator`:
//! exchanges the `SeqCAstNode` base subobject and the two operand
//! pointers `lhs_` / `rhs_` of `a` and `b`.
//! \param a First operator node.
//! \param b Second operator node.
void swap(SeqCOperator& a, SeqCOperator& b) {
    swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
    std::swap(a.lhs_, b.lhs_);
    std::swap(a.rhs_, b.rhs_);
}

// SeqCOperator evaluate stubs moved to seqc_ast_nodes_evaluate.cpp.

// Binary-op subclass doClone pattern: allocates new(0x28), copies base fields,
// recursively clones lhs_ and rhs_.  All 22 are identical except vtable ptr.
#define SEQC_OPERATOR_IMPL(Name, Label, LabelLen)                            \
    Name::Name(Name const& o) : SeqCOperator(o) {}                          \
    Name& Name::operator=(Name o) {                                          \
        swap(static_cast<SeqCOperator&>(*this),                              \
             static_cast<SeqCOperator&>(o));                                 \
        return *this; }                                                      \
    Name::~Name() = default;                                                 \
    void Name::print() const {                                               \
        std::cout.write(Label, LabelLen);                                    \
    }                                                                        \
    std::unique_ptr<SeqCAstNode> Name::doClone() const {                       \
        return std::make_unique<Name>(                                       \
            valueCategory_, lineNr_, direction_, varType_,                     \
            lhs_ ? lhs_->doClone() : nullptr,                                 \
            rhs_ ? rhs_->doClone() : nullptr);                                \
    }

SEQC_OPERATOR_IMPL(SeqCPlus,    "Plus",    4)  // print @0x204dc0, doClone @0x204ea0
SEQC_OPERATOR_IMPL(SeqCMinus,   "Minus",   5)  // print @0x205050, doClone @0x205130
SEQC_OPERATOR_IMPL(SeqCMult,    "Mult",    4)  // print @0x2052e0, doClone @0x2053c0
SEQC_OPERATOR_IMPL(SeqCDiv,     "Div",     3)  // print @0x205570, doClone @0x205650
SEQC_OPERATOR_IMPL(SeqCMod,     "Mod",     3)  // print @0x205800, doClone @0x2058e0
SEQC_OPERATOR_IMPL(SeqCShiftR,  "ShiftR",  6)  // print @0x205a90, doClone @0x205b70
SEQC_OPERATOR_IMPL(SeqCShiftL,  "ShiftL",  6)  // print @0x205d20, doClone @0x205e00
SEQC_OPERATOR_IMPL(SeqCGreater, "Greater", 7)  // print @0x205fb0, doClone @0x206090
SEQC_OPERATOR_IMPL(SeqCLower,   "Lower",   5)  // print @0x206240, doClone @0x206320
SEQC_OPERATOR_IMPL(SeqCLEqual,  "LEqual",  6)  // print @0x2064d0, doClone @0x2065b0
SEQC_OPERATOR_IMPL(SeqCGEqual,  "GEqual",  6)  // print @0x206760, doClone @0x206840
SEQC_OPERATOR_IMPL(SeqCEqual,   "Equal",   5)  // print @0x2069f0, doClone @0x206ad0
SEQC_OPERATOR_IMPL(SeqCNEqual,  "NEqual",  6)  // print @0x206c80, doClone @0x206d60
SEQC_OPERATOR_IMPL(SeqCInc,     "Inc",     3)  // print @0x206f10, doClone @0x206ff0
SEQC_OPERATOR_IMPL(SeqCDec,     "Dec",     3)  // print @0x2071a0, doClone @0x207280
SEQC_OPERATOR_IMPL(SeqCAndExpr, "AndExpr", 7)  // print @0x207430, doClone @0x207510
SEQC_OPERATOR_IMPL(SeqCOrExpr,  "OrExpr",  6)  // print @0x2076c0, doClone @0x2077a0
SEQC_OPERATOR_IMPL(SeqCXorExpr, "XorExpr", 7)  // print @0x207950, doClone @0x207a30
SEQC_OPERATOR_IMPL(SeqCLogAnd,  "LogAnd",  6)  // print @0x207be0, doClone @0x207cc0
SEQC_OPERATOR_IMPL(SeqCLogOr,   "LogOr",   5)  // print @0x207e70, doClone @0x207f50
SEQC_OPERATOR_IMPL(SeqCAssign,  "Assign",  6)  // print @0x208100, doClone @0x2081e0
SEQC_OPERATOR_IMPL(SeqCNoOp,    "NoOp",    4)  // print @0x208390, doClone @0x208470

#undef SEQC_OPERATOR_IMPL

// ============================================================================
// Two-child direct-AstNode subclasses (40B)
// ============================================================================

#define SEQC_BINARY_IMPL(Name, F1, F2, Label, LabelLen)                      \
    Name::Name(EValueCategory vc, int lineNr, EDirection dir,             \
               VarType vt,                                                   \
               std::unique_ptr<SeqCAstNode> F1,                              \
               std::unique_ptr<SeqCAstNode> F2)                              \
        : SeqCAstNode(vc, lineNr, dir),                                        \
          F1##_(std::move(F1)), F2##_(std::move(F2))                         \
    { varType_ = vt; }                                                       \
    Name::Name(Name const& o)                                                \
        : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_),              \
          F1##_(o.F1##_ ? o.F1##_->doClone() : nullptr),                      \
          F2##_(o.F2##_ ? o.F2##_->doClone() : nullptr)                       \
    { varType_ = o.varType_; }                                               \
    Name& Name::operator=(Name o) { swap(*this, o); return *this; }          \
    Name::~Name() = default;                                                 \
    void Name::print() const {                                               \
        std::cout.write(Label, LabelLen);                                    \
    }                                                                        \
    std::unique_ptr<SeqCAstNode> Name::doClone() const {                       \
        return std::make_unique<Name>(                                       \
            valueCategory_, lineNr_, direction_, varType_,                     \
            F1##_ ? F1##_->doClone() : nullptr,                                \
            F2##_ ? F2##_->doClone() : nullptr);                               \
    }                                                                        \
    std::vector<const SeqCAstNode*> Name::children() const {                 \
        return { F1##_.get(), F2##_.get() };                                 \
    }                                                                        \
    void swap(Name& a, Name& b) {                                           \
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));    \
        std::swap(a.F1##_, b.F1##_);                                         \
        std::swap(a.F2##_, b.F2##_);                                         \
    }

// SeqCFunctionCall — broken out of SEQC_BINARY_IMPL: funName_ is unique_ptr<SeqCVariable>.
SeqCFunctionCall::SeqCFunctionCall(EValueCategory vc, int lineNr, EDirection dir,
                                   VarType vt,
                                   std::unique_ptr<SeqCVariable> funName,
                                   std::unique_ptr<SeqCAstNode> args)
    : SeqCAstNode(vc, lineNr, dir),
      funName_(std::move(funName)), args_(std::move(args))
{ varType_ = vt; }

SeqCFunctionCall::SeqCFunctionCall(SeqCFunctionCall const& o)
    : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_),
      funName_(o.funName_ ? std::unique_ptr<SeqCVariable>(
          static_cast<SeqCVariable*>(o.funName_->doClone().release())) : nullptr),
      args_(o.args_ ? o.args_->doClone() : nullptr)
{ varType_ = o.varType_; }

SeqCFunctionCall& SeqCFunctionCall::operator=(SeqCFunctionCall o) { swap(*this, o); return *this; }
SeqCFunctionCall::~SeqCFunctionCall() = default;

void SeqCFunctionCall::print() const { std::cout.write("FunctionCall", 12); }  // @0x1fec50

std::unique_ptr<SeqCAstNode> SeqCFunctionCall::doClone() const {  // @0x1fed30
    return std::make_unique<SeqCFunctionCall>(
        valueCategory_, lineNr_, direction_, varType_,
        funName_ ? std::unique_ptr<SeqCVariable>(
            static_cast<SeqCVariable*>(funName_->doClone().release())) : nullptr,
        args_ ? args_->doClone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCFunctionCall::children() const {
    return { funName_.get(), args_.get() };
}

//! \brief ADL-friendly free-function swap for `SeqCFunctionCall`:
//! exchanges the `SeqCAstNode` base subobject, the callee identifier
//! pointer `funName_`, and the argument-list pointer `args_` of `a`
//! and `b`.
//! \param a First function-call node.
//! \param b Second function-call node.
void swap(SeqCFunctionCall& a, SeqCFunctionCall& b) {
    swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
    std::swap(a.funName_, b.funName_);
    std::swap(a.args_, b.args_);
}

// SeqCArray — broken out of SEQC_BINARY_IMPL: array_ is unique_ptr<SeqCVariable>.
SeqCArray::SeqCArray(EValueCategory vc, int lineNr, EDirection dir,
                     VarType vt,
                     std::unique_ptr<SeqCVariable> array,
                     std::unique_ptr<SeqCAstNode> index)
    : SeqCAstNode(vc, lineNr, dir),
      array_(std::move(array)), index_(std::move(index))
{ varType_ = vt; }

SeqCArray::SeqCArray(SeqCArray const& o)
    : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_),
      array_(o.array_ ? std::unique_ptr<SeqCVariable>(
          static_cast<SeqCVariable*>(o.array_->doClone().release())) : nullptr),
      index_(o.index_ ? o.index_->doClone() : nullptr)
{ varType_ = o.varType_; }

SeqCArray& SeqCArray::operator=(SeqCArray o) { swap(*this, o); return *this; }
SeqCArray::~SeqCArray() = default;

void SeqCArray::print() const { std::cout.write("Array", 5); }  // @0x1ff2e0

std::unique_ptr<SeqCAstNode> SeqCArray::doClone() const {  // @0x1ff3c0
    return std::make_unique<SeqCArray>(
        valueCategory_, lineNr_, direction_, varType_,
        array_ ? std::unique_ptr<SeqCVariable>(
            static_cast<SeqCVariable*>(array_->doClone().release())) : nullptr,
        index_ ? index_->doClone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCArray::children() const {
    return { array_.get(), index_.get() };
}

//! \brief ADL-friendly free-function swap for `SeqCArray`:
//! exchanges the `SeqCAstNode` base subobject, the array-identifier
//! pointer `array_`, and the index-expression pointer `index_` of `a`
//! and `b`.
//! \param a First array-access node.
//! \param b Second array-access node.
void swap(SeqCArray& a, SeqCArray& b) {
    swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
    std::swap(a.array_, b.array_);
    std::swap(a.index_, b.index_);
}

SEQC_BINARY_IMPL(SeqCIfCondition,  cond, ifBody, "IfCondition",  11)  // print @0x201a30, doClone @0x201b10

// SeqCCaseEntry — broken out of SEQC_BINARY_IMPL for extra methods.
SeqCCaseEntry::SeqCCaseEntry(EValueCategory vc, int lineNr, EDirection dir,
                             VarType vt,
                             std::unique_ptr<SeqCAstNode> label,
                             std::unique_ptr<SeqCAstNode> body)
    : SeqCAstNode(vc, lineNr, dir),
      label_(std::move(label)), body_(std::move(body))
{ varType_ = vt; }

SeqCCaseEntry::SeqCCaseEntry(SeqCCaseEntry const& o)
    : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_),
      label_(o.label_ ? o.label_->doClone() : nullptr),
      body_(o.body_ ? o.body_->doClone() : nullptr)
{ varType_ = o.varType_; }

SeqCCaseEntry& SeqCCaseEntry::operator=(SeqCCaseEntry o) { swap(*this, o); return *this; }
SeqCCaseEntry::~SeqCCaseEntry() = default;

void SeqCCaseEntry::print() const { std::cout.write("CaseEntry", 9); }  // @0x2027a0

std::unique_ptr<SeqCAstNode> SeqCCaseEntry::doClone() const {  // @0x202880
    return std::make_unique<SeqCCaseEntry>(
        valueCategory_, lineNr_, direction_, varType_,
        label_ ? label_->doClone() : nullptr,
        body_ ? body_->doClone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCCaseEntry::children() const {
    return { label_.get(), body_.get() };
}

const SeqCAstNode* SeqCCaseEntry::body() const { return body_.get(); }
bool SeqCCaseEntry::validLabel() const { return label_ != nullptr; }
bool SeqCCaseEntry::hasLabel()   const { return label_ != nullptr; }

//! \brief ADL-friendly free-function swap for `SeqCCaseEntry`:
//! exchanges the `SeqCAstNode` base subobject, the label-expression
//! pointer `label_`, and the case-body pointer `body_` of `a` and
//! `b`.
//! \param a First case-entry node.
//! \param b Second case-entry node.
void swap(SeqCCaseEntry& a, SeqCCaseEntry& b) {
    swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
    std::swap(a.label_, b.label_);
    std::swap(a.body_, b.body_);
}

SEQC_BINARY_IMPL(SeqCSwitchCase,   cond, body, "SwitchCase",   10)  // print @0x202350, doClone @0x202430
SEQC_BINARY_IMPL(SeqCWhileLoop,    cond, body, "WhileLoop",     9)  // print @0x203060, doClone @0x203140
SEQC_BINARY_IMPL(SeqCDoWhile,      body, cond, "DoWhile",       7)  // print @0x203420, doClone @0x203500
SEQC_BINARY_IMPL(SeqCRepeat,       cond,  body, "Repeat",        6)  // print @0x2037e0, doClone @0x2038c0; cond() @0x203b80, body() @0x203b90

#undef SEQC_BINARY_IMPL

// ============================================================================
// Three-child direct-AstNode subclasses (48B)
// ============================================================================

// SeqCIfElse — vtable 0xb05430
SeqCIfElse::SeqCIfElse(EValueCategory vc, int lineNr, EDirection dir,
                       VarType vt,
                       std::unique_ptr<SeqCAstNode> cond,
                       std::unique_ptr<SeqCAstNode> ifBody,
                       std::unique_ptr<SeqCAstNode> elseBody)  // 0x202150
    : SeqCAstNode(vc, lineNr, dir)
    , cond_(std::move(cond))
    , ifBody_(std::move(ifBody))
    , elseBody_(std::move(elseBody))
{
    varType_ = vt;
}

SeqCIfElse::~SeqCIfElse() = default;

SeqCIfElse::SeqCIfElse(SeqCIfElse const& o)
    : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_)
    , cond_(o.cond_ ? o.cond_->doClone() : nullptr)
    , ifBody_(o.ifBody_ ? o.ifBody_->doClone() : nullptr)
    , elseBody_(o.elseBody_ ? o.elseBody_->doClone() : nullptr)
{ varType_ = o.varType_; }

void SeqCIfElse::print() const {  // 0x201df0
    std::cout.write("IfElse", 6);
}

std::unique_ptr<SeqCAstNode> SeqCIfElse::doClone() const {  // 0x2021a0
    return std::make_unique<SeqCIfElse>(
        valueCategory_, lineNr_, direction_, varType_,
        cond_ ? cond_->doClone() : nullptr,
        ifBody_ ? ifBody_->doClone() : nullptr,
        elseBody_ ? elseBody_->doClone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCIfElse::children() const {  // 0x2022c0
    return { cond_.get(), ifBody_.get(), elseBody_.get() };
}

SeqCIfElse& SeqCIfElse::operator=(SeqCIfElse o) { swap(*this, o); return *this; }

//! \brief ADL-friendly free-function swap for `SeqCIfElse`:
//! exchanges the `SeqCAstNode` base subobject, the condition
//! pointer `cond_`, the then-branch pointer `ifBody_`, and the
//! else-branch pointer `elseBody_` of `a` and `b`.
//! \param a First if/else node.
//! \param b Second if/else node.
void swap(SeqCIfElse& a, SeqCIfElse& b) {
    swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
    std::swap(a.cond_, b.cond_);
    std::swap(a.ifBody_, b.ifBody_);
    std::swap(a.elseBody_, b.elseBody_);
}

// SeqCCondExpr — vtable 0xb056c0
SeqCCondExpr::SeqCCondExpr(EValueCategory vc, int lineNr, EDirection dir,
                           VarType vt,
                           std::unique_ptr<SeqCAstNode> cond,
                           std::unique_ptr<SeqCAstNode> ifBody,
                           std::unique_ptr<SeqCAstNode> elseBody)
    : SeqCAstNode(vc, lineNr, dir)
    , cond_(std::move(cond))
    , ifBody_(std::move(ifBody))
    , elseBody_(std::move(elseBody))
{
    varType_ = vt;
}

SeqCCondExpr::~SeqCCondExpr() = default;

SeqCCondExpr::SeqCCondExpr(SeqCCondExpr const& o)
    : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_)
    , cond_(o.cond_ ? o.cond_->doClone() : nullptr)
    , ifBody_(o.ifBody_ ? o.ifBody_->doClone() : nullptr)
    , elseBody_(o.elseBody_ ? o.elseBody_->doClone() : nullptr)
{ varType_ = o.varType_; }

void SeqCCondExpr::print() const {  // 0x203ba0
    std::cout.write("CondExpr", 8);
}

std::unique_ptr<SeqCAstNode> SeqCCondExpr::doClone() const {  // 0x203c80
    return std::make_unique<SeqCCondExpr>(
        valueCategory_, lineNr_, direction_, varType_,
        cond_ ? cond_->doClone() : nullptr,
        ifBody_ ? ifBody_->doClone() : nullptr,
        elseBody_ ? elseBody_->doClone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCCondExpr::children() const {
    return { cond_.get(), ifBody_.get(), elseBody_.get() };
}

SeqCCondExpr& SeqCCondExpr::operator=(SeqCCondExpr o) { swap(*this, o); return *this; }

//! \brief ADL-friendly free-function swap for `SeqCCondExpr`:
//! exchanges the `SeqCAstNode` base subobject, the condition pointer
//! `cond_`, the then-branch pointer `ifBody_`, and the else-branch
//! pointer `elseBody_` of `a` and `b`.
//! \param a First conditional-expression node.
//! \param b Second conditional-expression node.
void swap(SeqCCondExpr& a, SeqCCondExpr& b) {
    swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
    std::swap(a.cond_, b.cond_);
    std::swap(a.ifBody_, b.ifBody_);
    std::swap(a.elseBody_, b.elseBody_);
}

// ============================================================================
// Four-child direct-AstNode subclasses (56B)
// ============================================================================

// SeqCFunction — vtable 0xb050f0
SeqCFunction::SeqCFunction(EValueCategory vc, int lineNr, EDirection dir,
                           VarType vt,
                           std::unique_ptr<SeqCFunctionCall> call,
                           std::unique_ptr<SeqCAstNode> params,
                           std::unique_ptr<SeqCAstNode> body,
                           std::unique_ptr<SeqCVariableType> retType)
    : SeqCAstNode(vc, lineNr, dir)
    , call_(std::move(call))
    , params_(std::move(params))
    , body_(std::move(body))
    , retType_(std::move(retType))
{
    varType_ = vt;
}

SeqCFunction::~SeqCFunction() = default;

SeqCFunction::SeqCFunction(SeqCFunction const& o)
    : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_)
    , call_(o.call_ ? std::unique_ptr<SeqCFunctionCall>(
          static_cast<SeqCFunctionCall*>(o.call_->doClone().release())) : nullptr)
    , params_(o.params_ ? o.params_->doClone() : nullptr)
    , body_(o.body_ ? o.body_->doClone() : nullptr)
    , retType_(o.retType_ ? std::unique_ptr<SeqCVariableType>(
          static_cast<SeqCVariableType*>(o.retType_->doClone().release())) : nullptr)
{ varType_ = o.varType_; }

void SeqCFunction::print() const {  // 0x1fe730
    std::cout.write("Function", 8);
}

std::unique_ptr<SeqCAstNode> SeqCFunction::doClone() const {  // 0x1fe7c0
    return std::make_unique<SeqCFunction>(
        valueCategory_, lineNr_, direction_, varType_,
        call_ ? std::unique_ptr<SeqCFunctionCall>(
            static_cast<SeqCFunctionCall*>(call_->doClone().release())) : nullptr,
        params_ ? params_->doClone() : nullptr,
        body_ ? body_->doClone() : nullptr,
        retType_ ? std::unique_ptr<SeqCVariableType>(
            static_cast<SeqCVariableType*>(retType_->doClone().release())) : nullptr);
}

std::vector<const SeqCAstNode*> SeqCFunction::children() const {
    return { call_.get(), params_.get(), body_.get(), retType_.get() };
}

SeqCFunction& SeqCFunction::operator=(SeqCFunction o) { swap(*this, o); return *this; }

//! \brief ADL-friendly free-function swap for `SeqCFunction`:
//! exchanges the `SeqCAstNode` base subobject, the call-site pointer
//! `call_`, the parameter-list pointer `params_`, the body pointer
//! `body_`, and the return-type pointer `retType_` of `a` and `b`.
//! \param a First function-definition node.
//! \param b Second function-definition node.
void swap(SeqCFunction& a, SeqCFunction& b) {
    swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
    std::swap(a.call_, b.call_);
    std::swap(a.params_, b.params_);
    std::swap(a.body_, b.body_);
    std::swap(a.retType_, b.retType_);
}

// SeqCForLoop — vtable 0xb05580
SeqCForLoop::SeqCForLoop(EValueCategory vc, int lineNr, EDirection dir,
                         VarType vt,
                         std::unique_ptr<SeqCAstNode> init,
                         std::unique_ptr<SeqCAstNode> cond,
                         std::unique_ptr<SeqCAstNode> incr,
                         std::unique_ptr<SeqCAstNode> body)  // 0x202f00
    : SeqCAstNode(vc, lineNr, dir)
    , init_(std::move(init))
    , cond_(std::move(cond))
    , incr_(std::move(incr))
    , body_(std::move(body))
{
    varType_ = vt;
}

SeqCForLoop::~SeqCForLoop() = default;

SeqCForLoop::SeqCForLoop(SeqCForLoop const& o)
    : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_)
    , init_(o.init_ ? o.init_->doClone() : nullptr)
    , cond_(o.cond_ ? o.cond_->doClone() : nullptr)
    , incr_(o.incr_ ? o.incr_->doClone() : nullptr)
    , body_(o.body_ ? o.body_->doClone() : nullptr)
{ varType_ = o.varType_; }

void SeqCForLoop::print() const {  // 0x202bc0
    std::cout.write("ForLoop", 7);
}

std::unique_ptr<SeqCAstNode> SeqCForLoop::doClone() const {  // 0x202f70
    return std::make_unique<SeqCForLoop>(
        valueCategory_, lineNr_, direction_, varType_,
        init_ ? init_->doClone() : nullptr,
        cond_ ? cond_->doClone() : nullptr,
        incr_ ? incr_->doClone() : nullptr,
        body_ ? body_->doClone() : nullptr);
}

std::vector<const SeqCAstNode*> SeqCForLoop::children() const {  // 0x202fd0
    return { init_.get(), cond_.get(), incr_.get(), body_.get() };
}

SeqCForLoop& SeqCForLoop::operator=(SeqCForLoop o) { swap(*this, o); return *this; }

//! \brief ADL-friendly free-function swap for `SeqCForLoop`:
//! exchanges the `SeqCAstNode` base subobject, the initialiser
//! pointer `init_`, the condition pointer `cond_`, the increment
//! pointer `incr_`, and the body pointer `body_` of `a` and `b`.
//! \param a First for-loop node.
//! \param b Second for-loop node.
void swap(SeqCForLoop& a, SeqCForLoop& b) {
    swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
    std::swap(a.init_, b.init_);
    std::swap(a.cond_, b.cond_);
    std::swap(a.incr_, b.incr_);
    std::swap(a.body_, b.body_);
}

// ============================================================================
// List nodes (48B)
// ============================================================================

#define SEQC_LIST_IMPL(Name, Label, LabelLen)                                \
    Name::Name(EValueCategory vc, int lineNr, EDirection dir, VarType vt) \
        : SeqCAstNode(vc, lineNr, dir), elements_()                            \
    { varType_ = vt; }                                                       \
    Name::Name(EValueCategory vc, int lineNr, EDirection dir, VarType vt, \
               std::vector<std::unique_ptr<SeqCAstNode>> elements)           \
        : SeqCAstNode(vc, lineNr, dir), elements_(std::move(elements))         \
    { varType_ = vt; }                                                       \
    Name::Name(Name const& o)                                                \
        : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_)               \
    {                                                                        \
        varType_ = o.varType_;                                               \
        elements_.reserve(o.elements_.size());                               \
        for (const auto& e : o.elements_)                                    \
            elements_.push_back(e ? e->doClone() : nullptr);                   \
    }                                                                        \
    Name::~Name() = default;                                                 \
    void Name::print() const {                                               \
        std::cout.write(Label, LabelLen);                                    \
    }                                                                        \
    std::unique_ptr<SeqCAstNode> Name::doClone() const {                       \
        std::vector<std::unique_ptr<SeqCAstNode>> cloned;                    \
        cloned.reserve(elements_.size());                                    \
        for (const auto& e : elements_) {                                    \
            cloned.push_back(e ? e->doClone() : nullptr);                      \
        }                                                                    \
        return std::make_unique<Name>(                                       \
            valueCategory_, lineNr_, direction_, varType_,                     \
            std::move(cloned));                                              \
    }                                                                        \
    std::vector<const SeqCAstNode*> Name::children() const {                 \
        std::vector<const SeqCAstNode*> result;                              \
        result.reserve(elements_.size());                                    \
        for (const auto& e : elements_) result.push_back(e.get());           \
        return result;                                                       \
    }                                                                        \
    void Name::append(std::unique_ptr<SeqCAstNode> elem) {                   \
        elements_.push_back(std::move(elem));                                \
    }                                                                        \
    std::vector<std::string> Name::getListElements() const {                 \
        std::vector<std::string> result;                                     \
        for (const auto& e : elements_) {                                    \
            auto elems = e->getListElements();                               \
            for (auto& s : elems) result.push_back(std::move(s));            \
        }                                                                    \
        return result;                                                       \
    }                                                                        \
    Name& Name::operator=(Name o) { swap(*this, o); return *this; }          \
    void swap(Name& a, Name& b) {                                           \
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));    \
        std::swap(a.elements_, b.elements_);                                 \
    }

SEQC_LIST_IMPL(SeqCArgList,   "ArgList",   7)  // print @0x1ff600, doClone @0x1ff6e0
SEQC_LIST_IMPL(SeqCDeclList,  "DeclList",  8)  // print @0x1ffef0, doClone @0x1fffd0
SEQC_LIST_IMPL(SeqCParamList, "ParamList", 9)  // print @0x200910, doClone @0x2009f0
SEQC_LIST_IMPL(SeqCStmtList,  "StmtList",  8)  // print @0x201520, doClone @0x201600

#undef SEQC_LIST_IMPL

// Named accessors for list nodes — return raw pointer vectors like params()
// @0x1ffd40
std::vector<const SeqCAstNode*> SeqCArgList::params() const {
    std::vector<const SeqCAstNode*> result;
    for (const auto& e : elements_) result.push_back(e.get());
    return result;
}
// @0x200630
std::vector<const SeqCAstNode*> SeqCDeclList::decls() const {
    std::vector<const SeqCAstNode*> result;
    for (const auto& e : elements_) result.push_back(e.get());
    return result;
}
// @0x201330
std::vector<const SeqCAstNode*> SeqCStmtList::stmts() const {
    std::vector<const SeqCAstNode*> result;
    for (const auto& e : elements_) result.push_back(e.get());
    return result;
}

// @0x201050
std::vector<const SeqCAstNode*> SeqCParamList::params() const
{
    std::vector<const SeqCAstNode*> result;
    for (const auto& e : elements_) {
        result.push_back(e.get());
    }
    return result;
}

// @0x200f20 — SeqCParamList::getVarTypes override.
// Iterates elements_, reads each child's varType(), converts to string.
std::vector<std::string> SeqCParamList::getVarTypes() const
{
    std::vector<std::string> result;
    for (const auto& e : elements_) {
        result.push_back(str(e->varType()));
    }
    return result;
}

// ============================================================================
// Leaf nodes with payload
// ============================================================================

// SeqCVariable — vtable 0xb04fb0
SeqCVariable::SeqCVariable(EValueCategory vc, int lineNr, EDirection dir,
                           VarType vt, std::string name)
    : SeqCAstNode(vc, lineNr, dir)
    , name_(std::move(name))
{
    varType_ = vt;
}

SeqCVariable::~SeqCVariable() = default;

SeqCVariable::SeqCVariable(SeqCVariable const& o)
    : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_)
    , name_(o.name_)
{ varType_ = o.varType_; }

// 0x1fdbd0 — complex print: "Variable: name(VarType), direction"
// Binary flow:
//   1. Write "Variable: " (10 chars) to cout
//   2. If lineNr_ == 0: just write name_
//   3. Else: write name_ + " (" + str(VarType(lineNr_)) + ")"
//   4. Write ", "
//   5. Write str(direction_)
//   6. Write "\n" (1 char, but uncertain — might be absent)
void SeqCVariable::print() const {  // 0x1fdbd0
    std::cout.write("Variable: ", 10);
    if (varType_ == VarType_Unset) {
        std::cout << name_;
    } else {
        std::string s = name_;
        s += " (";
        s += str(varType_);
        s += ")";
        std::cout << s;
    }
    std::cout.write(" [", 2);
    std::cout << str(direction_);
    std::cout.write("]", 1);
}

std::unique_ptr<SeqCAstNode> SeqCVariable::doClone() const {  // 0x1fdf30
    return std::make_unique<SeqCVariable>(
        valueCategory_, lineNr_, direction_, varType_, name_);
}

// 0x209e60
std::vector<std::string> SeqCVariable::getListElements() const {
    std::vector<std::string> result;
    result.push_back(name_);
    return result;
}

SeqCVariable& SeqCVariable::operator=(SeqCVariable o) { swap(*this, o); return *this; }

//! \brief ADL-friendly free-function swap for `SeqCVariable`:
//! exchanges the `SeqCAstNode` base subobject and the identifier
//! string `name_` of `a` and `b`.
//! \param a First identifier node.
//! \param b Second identifier node.
void swap(SeqCVariable& a, SeqCVariable& b) {
    swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
    std::swap(a.name_, b.name_);
}

// SeqCValue — vtable 0xb05000
SeqCValue::SeqCValue(EValueCategory vc, int lineNr, EDirection dir,
                     VarType vt)
    : SeqCAstNode(vc, lineNr, dir)
{
    varType_ = vt;
}

// String-value ctor — binary callsite at 0x1f656e (make_unique<SeqCValue>(..., string))
SeqCValue::SeqCValue(EValueCategory vc, int lineNr, EDirection dir,
                     VarType vt, std::string s)
    : SeqCAstNode(vc, lineNr, dir)
{
    varType_ = vt;
    tag_ = 0;  // eString
    new (&payload_.str) std::string(std::move(s));
}

// Double-value ctor — binary callsite at 0x1f6f22 (make_unique<SeqCValue>(..., double&))
SeqCValue::SeqCValue(EValueCategory vc, int lineNr, EDirection dir,
                     VarType vt, double d)
    : SeqCAstNode(vc, lineNr, dir)
{
    varType_ = vt;
    tag_ = 1;  // eDouble
    payload_.d = d;
}

// Copy-ctor: tag-dispatched payload copy. @0x208600 area.
SeqCValue::SeqCValue(SeqCValue const& o)
    : SeqCAstNode(o.valueCategory_, o.lineNr_, o.direction_)
{
    varType_ = o.varType_;
    tag_ = o.tag_;
    pad34_ = o.pad34_;
    if (o.tag_ == 0) {
        new (&payload_.str) std::string(o.payload_.str);
    } else {
        payload_.d = o.payload_.d;
    }
}

SeqCValue::~SeqCValue() {  // D2 @0x1fe510
    if (tag_ == 0) {
        payload_.str.~basic_string();
    }
    tag_ = -1;  // @0x1fe542: movl $0xffffffff, 0x30(%rbx)
}

// 0x1fe230 — variant-dispatching print: "Value = " + payload
void SeqCValue::print() const {  // 0x1fe230
    std::cout.write("Value = ", 8);
    switch (tag_) {
        case 0:
            std::cout << payload_.str;
            break;
        case 1:
            std::cout << std::to_string(payload_.d);
            break;
        default:
            break;
    }
}

std::unique_ptr<SeqCAstNode> SeqCValue::doClone() const {  // 0x208600
    return std::make_unique<SeqCValue>(*this);
}

SeqCValue& SeqCValue::operator=(SeqCValue o) { swap(*this, o); return *this; }

//! \brief ADL-friendly free-function swap for `SeqCValue`:
//! exchanges the variable-type tag and the tagged `payload_` /
//! `tag_` pair (string or double) of `a` and `b`.
//! \details Performs a tag-aware payload exchange: when an operand
//! holds a string, the string is move-constructed into a temporary
//! and the source is destroyed in place, preserving the variant
//! invariant in both halves of the swap.
//! \binarynote Unlike the other `SeqCAstNode`-family swaps, this
//! overload deliberately does **not** exchange the full base
//! subobject — only `varType_` and the tagged payload move; the
//! `valueCategory_`, `lineNr_`, and `direction_` fields stay with
//! their respective objects.
//! \param a First literal-value node.
//! \param b Second literal-value node.
void swap(SeqCValue& a, SeqCValue& b) {  // 0x1fe410
    // Binary only swaps varType_ (+0x14) and the variant payload (+0x18).
    // It does NOT swap the other base class fields.
    std::swap(a.varType_, b.varType_);

    // Swap payloads with tag awareness (equivalent to variant::swap)
    int atag = a.tag_, btag = b.tag_;

    // Move a's payload to tmp
    SeqCValue::Payload tmp;
    if (atag == 0) {
        new (&tmp.str) std::string(std::move(a.payload_.str));
        a.payload_.str.~basic_string();
    } else {
        tmp.d = a.payload_.d;
    }

    // Move b's payload to a
    if (btag == 0) {
        new (&a.payload_.str) std::string(std::move(b.payload_.str));
        b.payload_.str.~basic_string();
    } else {
        a.payload_.d = b.payload_.d;
    }

    // Move tmp to b
    if (atag == 0) {
        new (&b.payload_.str) std::string(std::move(tmp.str));
        tmp.str.~basic_string();
    } else {
        b.payload_.d = tmp.d;
    }

    std::swap(a.tag_, b.tag_);
    std::swap(a.pad34_, b.pad34_);
}

} // namespace zhinst
