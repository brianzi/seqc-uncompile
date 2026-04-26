// ============================================================================
// SeqCAstNode — new SeqC AST hierarchy with virtual evaluate()
//
// 53 named subclasses (NOT in anonymous namespace as initially suspected).
// Vtables clustered at .rodata 0xb04f60..0xb060c8.
//
// Base layout (24 bytes / 0x18):
//   +0x00: vptr
//   +0x08: EValueCategory  (4 bytes)
//   +0x0C: int             type (4 bytes)
//   +0x10: EDirection      (4 bytes)  -- parameter direction (eIN/eOUT/eINOUT)
//   +0x14: VarType         (4 bytes)  -- Phase 21d: NOT padding; set by every
//                                        derived-class ctor (base ctor leaves it
//                                        uninitialized; subclass ctors all take
//                                        VarType as 4th explicit parameter)
//
// Inheritance summary:
//   - Direct AstNode subclasses (24B):  SeqCOperation, SeqCCommand,
//     SeqCVariableType, SeqCLabel, SeqCContinueStatement,
//     SeqCBreakStatement, SeqCNoCmd
//
//   - Single-child (32B):  SeqCNeg, SeqCPos, SeqCInv, SeqCNotExpr,
//     SeqCReturnStatement
//
//   - SeqCOperator base (40B) + 21 binary-op subclasses (vptr-only differences):
//     SeqCPlus, SeqCMinus, SeqCMult, SeqCDiv, SeqCMod, SeqCShiftR, SeqCShiftL,
//     SeqCGreater, SeqCLower, SeqCLEqual, SeqCGEqual, SeqCEqual, SeqCNEqual,
//     SeqCInc, SeqCDec, SeqCAndExpr, SeqCOrExpr, SeqCXorExpr, SeqCLogAnd,
//     SeqCLogOr, SeqCAssign, SeqCNoOp
//
//   - Two-child direct AstNode (40B):  SeqCFunctionCall, SeqCArray,
//     SeqCIfCondition, SeqCCaseEntry, SeqCSwitchCase, SeqCWhileLoop,
//     SeqCDoWhile, SeqCRepeat
//
//   - Three-child direct AstNode (48B):  SeqCIfElse, SeqCCondExpr
//
//   - Four-child direct AstNode (56B):  SeqCFunction, SeqCForLoop
//
//   - List nodes (48B, hold vector<unique_ptr<SeqCAstNode>>):
//     SeqCArgList, SeqCDeclList, SeqCParamList, SeqCStmtList
//
//   - Leaf nodes with payload:
//     SeqCVariable (48B, name string)
//     SeqCValue (56B, tagged value variant)
// ============================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "zhinst/resources.hpp"  // pre-existing VarType enum used here

namespace zhinst {

// Forward declarations for evaluate() signature
class FrontendLoweringContext;
class FrontendLoweringState;
class EvalResults;

// ---- Enums ----------------------------------------------------------------

enum class EValueCategory : int32_t {
    eNOVALUECATEGORY = 0,
    eLVALUE          = 1,
    eRVALUE          = 2,
};

// EDirection — unified enum, defined in types.hpp (Phase 21i).
// str() @0x1c1730 maps: 0→"in", 1→"out", 2→"inout".

// Free functions — enum to string
std::string str(EValueCategory vc);   // @0x1c16c0
std::string str(EDirection dir);      // @0x1c1730

// ---- Base class ----------------------------------------------------------

// SeqCAstNode — abstract base, 24 bytes (0x18).
// vtable at .rodata 0xb06618.
//
// D0 (deleting) destructor at 0x2462e0 is `ud2` — abstract, must never
// be deleted directly through base pointer with `delete`.
//
// ---- Vtable layout (Phase 21d — verified from binary vtable dumps) ----
//
// The declaration order of virtuals in the ORIGINAL source determines
// the vtable slot order. The order below matches the binary's vtable
// layout EXACTLY (evaluate first, dtors near the end):
//
//   vptr[0] (+0x00): evaluate(3-arg)      — the primary lowering dispatch
//   vptr[1] (+0x08): getListElements()    — returns vector<string>
//   vptr[2] (+0x10): children()           — returns vector<SeqCAstNode const*>
//   vptr[3] (+0x18): print()              [pure virtual in base]
//   vptr[4] (+0x20): clone()              [pure virtual in base]
//   vptr[5] (+0x28): ~D2 dtor
//   vptr[6] (+0x30): ~D0 dtor
//   vptr[7] (+0x38): getVarTypes()        — returns vector<string>
//
// SeqCOperator adds:
//   vptr[8] (+0x40): evaluate(5-arg)      — binary-operator specialization
//
class SeqCAstNode {
public:
    SeqCAstNode(EValueCategory vc, int type, EDirection dir);   // 0x1fda00
    // NOTE: base ctor takes 3 args; derived ctors all take VarType as 4th
    // and write it to varType_ directly (the binary inlines the base ctor).

    // --- Virtual methods in declaration (= vtable) order ---

    // vptr[0]: primary evaluate dispatch. Returns shared_ptr<EvalResults> via sret.
    // Base implementation @0x209dc0 returns null shared_ptr (zeroes 16 bytes).
    // Binary TU: SeqCAstNodesEvaluate.cpp (from _GLOBAL__sub_I_ symbol).
    virtual std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const;               // @0x209dc0

    // vptr[1]: default returns vector with single empty string.
    virtual std::vector<std::string> getListElements() const;    // @0x209dd0

    // vptr[2]: default returns empty vector.
    virtual std::vector<const SeqCAstNode*> children() const;    // @0x1fda20

    // vptr[3-4]: pure virtual — print/clone.
    virtual void print() const = 0;
    virtual std::unique_ptr<SeqCAstNode> clone() const = 0;

    // vptr[5-6]: destructor (D2 + D0).
    virtual ~SeqCAstNode();                                      // D2 @0x209000 (trivial)

    // vptr[7]: returns vector<string> of VarType names for parameter nodes.
    // Default implementation @0x1fdb40 (ICF-merged across trivial leaves):
    // returns vector with single element str(varType_).
    virtual std::vector<std::string> getVarTypes() const;        // @0x1fdb40

    // Accessors
    EValueCategory  valueCategory() const { return valueCategory_; }
    int             type()          const { return lineNr_; }  // legacy accessor name
    int             lineNr()        const { return lineNr_; }
    EDirection direction()     const { return direction_; }
    VarType         varType()       const { return varType_; }
    void            setVarType(VarType vt) { varType_ = vt; }

protected:
    // Layout (must match binary — all 16 bytes after vptr):
    EValueCategory  valueCategory_;  // +0x08
    int             lineNr_;         // +0x0C  Source line number. (Resolves unknown #96.)
                                     //        NOTE: SeqCVariable::print() casts this to VarType
                                     //        for display — overloaded meaning in that one subclass.
    EDirection direction_;      // +0x10
    VarType         varType_{};      // +0x14  (Phase 21d: was "padding"; set by derived ctors)

    // Allow swap to access fields by reference
    friend void swap(SeqCAstNode& a, SeqCAstNode& b);            // 0x1fda40
};

static_assert(sizeof(SeqCAstNode) == 0x18,
              "SeqCAstNode must be exactly 0x18 (24) bytes");

// Free function — swaps the (valueCategory, lineNr) qword and direction.
// Note: does NOT swap vptrs.
void swap(SeqCAstNode& a, SeqCAstNode& b);                       // 0x1fda40

// Free function — pretty-prints an AST tree to std::cout.
// Internally calls anonymous-namespace helper @0x1fa430 with empty indent.
void printSeqCAst(const SeqCAstNode& node);                      // 0x1fa3c0

// ============================================================================
// Direct AstNode subclasses with no extra fields (24 bytes, 0x18)
// ============================================================================

#define SEQC_TRIVIAL_LEAF(Name, VtableAddr)                                 \
    class Name : public SeqCAstNode {                                       \
    public:                                                                 \
        Name(EValueCategory vc, int type, EDirection dir,              \
             VarType vt)                                                    \
            : SeqCAstNode(vc, type, dir) { varType_ = vt; }                \
        Name(Name const& o) : SeqCAstNode(o.valueCategory_, o.lineNr_,       \
            o.direction_) { varType_ = o.varType_; }                        \
        Name& operator=(Name o) { swap(*this, o); return *this; }           \
        ~Name() override;                                                   \
        void print() const override;                                        \
        std::unique_ptr<SeqCAstNode> clone() const override;                \
        std::shared_ptr<EvalResults> evaluate(                              \
            std::shared_ptr<Resources> res,                                 \
            FrontendLoweringContext& ctx,                                    \
            FrontendLoweringState& state) const override;                   \
        friend void swap(Name& a, Name& b) {                               \
            swap(static_cast<SeqCAstNode&>(a),                              \
                 static_cast<SeqCAstNode&>(b));                             \
        }                                                                   \
    };                                                                      \
    static_assert(sizeof(Name) == 0x18, #Name " must be 0x18 bytes")

SEQC_TRIVIAL_LEAF(SeqCOperation,         0xb04f60);
SEQC_TRIVIAL_LEAF(SeqCCommand,           0xb05050);
SEQC_TRIVIAL_LEAF(SeqCVariableType,      0xb050a0);
SEQC_TRIVIAL_LEAF(SeqCLabel,             0xb05390);
SEQC_TRIVIAL_LEAF(SeqCContinueStatement, 0xb05710);
SEQC_TRIVIAL_LEAF(SeqCBreakStatement,    0xb05760);
SEQC_TRIVIAL_LEAF(SeqCNoCmd,             0xb05940);

#undef SEQC_TRIVIAL_LEAF

// ============================================================================
// Single-child unary-expression-like nodes (32 bytes, 0x20)
//
// Layout: SeqCAstNode (24B) + unique_ptr<SeqCAstNode> child_ (8B)
// ============================================================================

#define SEQC_UNARY(Name, VtableAddr)                                        \
    class Name : public SeqCAstNode {                                       \
    public:                                                                 \
        Name(EValueCategory vc, int type, EDirection dir,              \
             VarType vt,                                                    \
             std::unique_ptr<SeqCAstNode> child);                           \
        Name(Name const& o);                                                \
        Name& operator=(Name o) { swap(*this, o); return *this; }           \
        ~Name() override;                                                   \
        void print() const override;                                        \
        std::unique_ptr<SeqCAstNode> clone() const override;                \
        std::vector<const SeqCAstNode*> children() const override;          \
        std::shared_ptr<EvalResults> evaluate(                              \
            std::shared_ptr<Resources> res,                                 \
            FrontendLoweringContext& ctx,                                    \
            FrontendLoweringState& state) const override;                   \
        const SeqCAstNode* expr() const { return child_.get(); }            \
        friend void swap(Name& a, Name& b) {                               \
            swap(static_cast<SeqCAstNode&>(a),                              \
                 static_cast<SeqCAstNode&>(b));                             \
            std::swap(a.child_, b.child_);                                  \
        }                                                                   \
    private:                                                                \
        std::unique_ptr<SeqCAstNode> child_;  /* +0x18 */                   \
    };                                                                      \
    static_assert(sizeof(Name) == 0x20, #Name " must be 0x20 bytes")

SEQC_UNARY(SeqCNeg,             0xb05800);
SEQC_UNARY(SeqCPos,             0xb05850);
SEQC_UNARY(SeqCInv,             0xb058a0);
SEQC_UNARY(SeqCNotExpr,         0xb058f0);
SEQC_UNARY(SeqCReturnStatement, 0xb057b0);

#undef SEQC_UNARY

// ============================================================================
// SeqCOperator + 22 binary-op subclasses (40 bytes, 0x28)
//
// All 21 derived operator classes share the SeqCOperator vtable for cleanup
// (vptr reset to 0xb051a0 in their D0 dtors), have identical layout and
// only differ in their own vtable's print/clone/evaluate overrides.
// ============================================================================

class SeqCOperator : public SeqCAstNode {
public:
    SeqCOperator(EValueCategory vc, int type, EDirection dir,
                 VarType vt,
                 std::unique_ptr<SeqCAstNode> lhs,
                 std::unique_ptr<SeqCAstNode> rhs);
    SeqCOperator(SeqCOperator const& o);
    SeqCOperator& operator=(SeqCOperator o) { swap(*this, o); return *this; }
    ~SeqCOperator() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;
    std::vector<const SeqCAstNode*> children() const override;

    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x210aa0

    virtual std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state,
        EvalResults const& lhsResult,
        EvalResults const& rhsResult) const;       // vptr[8]

    const SeqCAstNode* lhs() const { return lhs_.get(); }
    const SeqCAstNode* rhs() const { return rhs_.get(); }

    friend void swap(SeqCOperator& a, SeqCOperator& b) {
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
        std::swap(a.lhs_, b.lhs_);
        std::swap(a.rhs_, b.rhs_);
    }

protected:
    std::unique_ptr<SeqCAstNode> lhs_;   // +0x18
    std::unique_ptr<SeqCAstNode> rhs_;   // +0x20
};

static_assert(sizeof(SeqCOperator) == 0x28,
              "SeqCOperator must be 0x28 (40) bytes");

#define SEQC_OPERATOR(Name, VtableAddr)                                     \
    class Name : public SeqCOperator {                                      \
    public:                                                                 \
        using SeqCOperator::SeqCOperator;                                   \
        Name(Name const& o) : SeqCOperator(o) {}                            \
        Name& operator=(Name o) {                                            \
            swap(static_cast<SeqCOperator&>(*this),                          \
                 static_cast<SeqCOperator&>(o));                             \
            return *this; }                                                  \
        ~Name() override;                                                   \
        void print() const override;                                        \
        std::unique_ptr<SeqCAstNode> clone() const override;                \
        std::shared_ptr<EvalResults> evaluate(                              \
            std::shared_ptr<Resources> res,                                 \
            FrontendLoweringContext& ctx,                                    \
            FrontendLoweringState& state,                                   \
            EvalResults const& lhsResult,                                   \
            EvalResults const& rhsResult) const override;                   \
    };                                                                      \
    static_assert(sizeof(Name) == 0x28, #Name " must be 0x28 bytes")

SEQC_OPERATOR(SeqCPlus,    0xb05990);
SEQC_OPERATOR(SeqCMinus,   0xb059e8);
SEQC_OPERATOR(SeqCMult,    0xb05a40);
SEQC_OPERATOR(SeqCDiv,     0xb05a98);
SEQC_OPERATOR(SeqCMod,     0xb05af0);
SEQC_OPERATOR(SeqCShiftR,  0xb05b48);
SEQC_OPERATOR(SeqCShiftL,  0xb05ba0);
SEQC_OPERATOR(SeqCGreater, 0xb05bf8);
SEQC_OPERATOR(SeqCLower,   0xb05c50);
SEQC_OPERATOR(SeqCLEqual,  0xb05ca8);
SEQC_OPERATOR(SeqCGEqual,  0xb05d00);
SEQC_OPERATOR(SeqCEqual,   0xb05d58);
SEQC_OPERATOR(SeqCNEqual,  0xb05db0);
SEQC_OPERATOR(SeqCInc,     0xb05e08);
SEQC_OPERATOR(SeqCDec,     0xb05e60);
SEQC_OPERATOR(SeqCAndExpr, 0xb05eb8);
SEQC_OPERATOR(SeqCOrExpr,  0xb05f10);
SEQC_OPERATOR(SeqCXorExpr, 0xb05f68);
SEQC_OPERATOR(SeqCLogAnd,  0xb05fc0);
SEQC_OPERATOR(SeqCLogOr,   0xb06018);
SEQC_OPERATOR(SeqCAssign,  0xb06070);
SEQC_OPERATOR(SeqCNoOp,    0xb060c8);

#undef SEQC_OPERATOR

// ============================================================================
// Two-child direct-AstNode subclasses (40 bytes, 0x28)
//
// Layout: SeqCAstNode (24B) + 2 unique_ptrs at +0x18, +0x20
// ============================================================================

#define SEQC_BINARY(Name, FirstAccessor, SecondAccessor, VtableAddr)        \
    class Name : public SeqCAstNode {                                       \
    public:                                                                 \
        Name(EValueCategory vc, int type, EDirection dir,              \
             VarType vt,                                                    \
             std::unique_ptr<SeqCAstNode> first,                            \
             std::unique_ptr<SeqCAstNode> second);                          \
        Name(Name const& o);                                                \
        Name& operator=(Name o) { swap(*this, o); return *this; }           \
        ~Name() override;                                                   \
        void print() const override;                                        \
        std::unique_ptr<SeqCAstNode> clone() const override;                \
        std::vector<const SeqCAstNode*> children() const override;          \
        std::shared_ptr<EvalResults> evaluate(                              \
            std::shared_ptr<Resources> res,                                 \
            FrontendLoweringContext& ctx,                                    \
            FrontendLoweringState& state) const override;                   \
        const SeqCAstNode* FirstAccessor()  const { return first_.get(); }  \
        const SeqCAstNode* SecondAccessor() const { return second_.get(); } \
        friend void swap(Name& a, Name& b) {                               \
            swap(static_cast<SeqCAstNode&>(a),                              \
                 static_cast<SeqCAstNode&>(b));                             \
            std::swap(a.first_, b.first_);                                  \
            std::swap(a.second_, b.second_);                                \
        }                                                                   \
    private:                                                                \
        std::unique_ptr<SeqCAstNode> first_;   /* +0x18 */                  \
        std::unique_ptr<SeqCAstNode> second_;  /* +0x20 */                  \
    };                                                                      \
    static_assert(sizeof(Name) == 0x28, #Name " must be 0x28 bytes")

SEQC_BINARY(SeqCFunctionCall, funName,  arguments,  0xb05140);
SEQC_BINARY(SeqCArray,        index,    array, 0xb051e8);
SEQC_BINARY(SeqCIfCondition,  cond,     ifBody,  0xb053e0);
SEQC_BINARY(SeqCCaseEntry,    label,    body,  0xb05518);
// Forward declaration for SeqCSwitchCase::cases() return type
class SeqCStmtList;
// SeqCSwitchCase — broken out of SEQC_BINARY for extra methods (hasCases, evalCases).
// vtable @0xb05480.  Layout identical to other SEQC_BINARY types (0x28 bytes).
class SeqCSwitchCase : public SeqCAstNode {
public:
    SeqCSwitchCase(EValueCategory vc, int type, EDirection dir,
                   VarType vt,
                   std::unique_ptr<SeqCAstNode> first,
                   std::unique_ptr<SeqCAstNode> second);
    SeqCSwitchCase(SeqCSwitchCase const& o);
    SeqCSwitchCase& operator=(SeqCSwitchCase o) { swap(*this, o); return *this; }
    ~SeqCSwitchCase() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;
    std::vector<const SeqCAstNode*> children() const override;
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;

    const SeqCAstNode* cond() const { return first_.get(); }
    const SeqCAstNode* body() const { return second_.get(); }

    // Switch-specific helpers
    bool hasCases() const;                                           // @0x202760
    bool isSingleCase() const;                                       // @0x202730
    const SeqCCaseEntry* singleCase() const;                         // @0x202770
    const SeqCStmtList* cases() const;                               // @0x202700
    std::vector<std::shared_ptr<EvalResults>> evalCases(
        std::shared_ptr<Resources> subRes,
        std::shared_ptr<EvalResults> condResult,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const;                         // @0x216980

    friend void swap(SeqCSwitchCase& a, SeqCSwitchCase& b) {
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
        std::swap(a.first_, b.first_);
        std::swap(a.second_, b.second_);
    }

private:
    std::unique_ptr<SeqCAstNode> first_;   /* +0x18 */
    std::unique_ptr<SeqCAstNode> second_;  /* +0x20 */
};
static_assert(sizeof(SeqCSwitchCase) == 0x28, "SeqCSwitchCase must be 0x28 bytes");
SEQC_BINARY(SeqCWhileLoop,    cond,     body,  0xb055d0);
SEQC_BINARY(SeqCDoWhile,      body,     cond,  0xb05620);
SEQC_BINARY(SeqCRepeat,       cond,     body,  0xb05670);

#undef SEQC_BINARY

// ============================================================================
// Three-child direct-AstNode subclasses (48 bytes, 0x30)
// ============================================================================

class SeqCIfElse : public SeqCAstNode {
public:
    SeqCIfElse(EValueCategory vc, int type, EDirection dir,
               VarType vt,
               std::unique_ptr<SeqCAstNode> cond,
               std::unique_ptr<SeqCAstNode> ifBody,
               std::unique_ptr<SeqCAstNode> elseBody);   // 0x202150
    SeqCIfElse(SeqCIfElse const& o);
    SeqCIfElse& operator=(SeqCIfElse o) { swap(*this, o); return *this; }
    ~SeqCIfElse() override;
    void print() const override;                          // 0x201df0
    std::unique_ptr<SeqCAstNode> clone() const override;  // 0x2021a0
    std::vector<const SeqCAstNode*> children() const override;  // 0x2022c0
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x214d50

    const SeqCAstNode* cond()     const { return cond_.get(); }      // 0x202320
    const SeqCAstNode* ifBody()   const { return ifBody_.get(); }    // 0x202330
    const SeqCAstNode* elseBody() const { return elseBody_.get(); }  // 0x202340

    friend void swap(SeqCIfElse& a, SeqCIfElse& b) {
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
        std::swap(a.cond_, b.cond_);
        std::swap(a.ifBody_, b.ifBody_);
        std::swap(a.elseBody_, b.elseBody_);
    }

private:
    std::unique_ptr<SeqCAstNode> cond_;       // +0x18
    std::unique_ptr<SeqCAstNode> ifBody_;     // +0x20
    std::unique_ptr<SeqCAstNode> elseBody_;   // +0x28
};
static_assert(sizeof(SeqCIfElse) == 0x30, "SeqCIfElse must be 0x30 bytes");

class SeqCCondExpr : public SeqCAstNode {
public:
    SeqCCondExpr(EValueCategory vc, int type, EDirection dir,
                 VarType vt,
                 std::unique_ptr<SeqCAstNode> cond,
                 std::unique_ptr<SeqCAstNode> ifBody,
                 std::unique_ptr<SeqCAstNode> elseBody);
    SeqCCondExpr(SeqCCondExpr const& o);
    SeqCCondExpr& operator=(SeqCCondExpr o) { swap(*this, o); return *this; }
    ~SeqCCondExpr() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;
    std::vector<const SeqCAstNode*> children() const override;
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x223d90

    const SeqCAstNode* cond()     const { return cond_.get(); }
    const SeqCAstNode* ifBody()   const { return ifBody_.get(); }    // @0x2040e0
    const SeqCAstNode* elseBody() const { return elseBody_.get(); }  // @0x2040f0

    friend void swap(SeqCCondExpr& a, SeqCCondExpr& b) {
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
        std::swap(a.cond_, b.cond_);
        std::swap(a.ifBody_, b.ifBody_);
        std::swap(a.elseBody_, b.elseBody_);
    }

private:
    std::unique_ptr<SeqCAstNode> cond_;          // +0x18
    std::unique_ptr<SeqCAstNode> ifBody_;        // +0x20
    std::unique_ptr<SeqCAstNode> elseBody_;      // +0x28
};
static_assert(sizeof(SeqCCondExpr) == 0x30, "SeqCCondExpr must be 0x30 bytes");

// ============================================================================
// Four-child direct-AstNode subclasses (56 bytes, 0x38)
// ============================================================================

class SeqCFunction : public SeqCAstNode {
public:
    SeqCFunction(EValueCategory vc, int type, EDirection dir,
                 VarType vt,
                 std::unique_ptr<SeqCFunctionCall> call,
                 std::unique_ptr<SeqCAstNode> params,
                 std::unique_ptr<SeqCAstNode> body,
                 std::unique_ptr<SeqCAstNode> retType);
    SeqCFunction(SeqCFunction const& o);
    SeqCFunction& operator=(SeqCFunction o) { swap(*this, o); return *this; }
    ~SeqCFunction() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;
    std::vector<const SeqCAstNode*> children() const override;
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x20b200

    const SeqCFunctionCall* call()  const { return call_.get(); }    // @0x1fec10, +0x18
    const SeqCAstNode* params()     const { return params_.get(); }  // @0x1fec20, +0x20
    const SeqCAstNode* body()       const { return body_.get(); }    // @0x1fec30, +0x28
    const SeqCAstNode* retType()    const { return retType_.get(); } // @0x1fec40, +0x30

    friend void swap(SeqCFunction& a, SeqCFunction& b) {
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
        std::swap(a.call_, b.call_);
        std::swap(a.params_, b.params_);
        std::swap(a.body_, b.body_);
        std::swap(a.retType_, b.retType_);
    }

private:
    std::unique_ptr<SeqCFunctionCall> call_;   // +0x18
    std::unique_ptr<SeqCAstNode> params_;      // +0x20
    std::unique_ptr<SeqCAstNode> body_;        // +0x28
    std::unique_ptr<SeqCAstNode> retType_;     // +0x30
};
static_assert(sizeof(SeqCFunction) == 0x38, "SeqCFunction must be 0x38 bytes");

class SeqCForLoop : public SeqCAstNode {
public:
    SeqCForLoop(EValueCategory vc, int type, EDirection dir,
                VarType vt,
                std::unique_ptr<SeqCAstNode> init,
                std::unique_ptr<SeqCAstNode> cond,
                std::unique_ptr<SeqCAstNode> incr,
                std::unique_ptr<SeqCAstNode> body);     // 0x202f00
    SeqCForLoop(SeqCForLoop const& o);
    SeqCForLoop& operator=(SeqCForLoop o) { swap(*this, o); return *this; }
    ~SeqCForLoop() override;
    void print() const override;                         // 0x202bc0
    std::unique_ptr<SeqCAstNode> clone() const override; // 0x202f70
    std::vector<const SeqCAstNode*> children() const override;  // 0x202fd0
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x21b680

    const SeqCAstNode* init() const { return init_.get(); }   // 0x203020
    const SeqCAstNode* cond() const { return cond_.get(); }   // 0x203030
    const SeqCAstNode* incr() const { return incr_.get(); }   // 0x203040
    const SeqCAstNode* body() const { return body_.get(); }   // 0x203050

    friend void swap(SeqCForLoop& a, SeqCForLoop& b) {
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
        std::swap(a.init_, b.init_);
        std::swap(a.cond_, b.cond_);
        std::swap(a.incr_, b.incr_);
        std::swap(a.body_, b.body_);
    }

private:
    std::unique_ptr<SeqCAstNode> init_;   // +0x18
    std::unique_ptr<SeqCAstNode> cond_;   // +0x20
    std::unique_ptr<SeqCAstNode> incr_;   // +0x28
    std::unique_ptr<SeqCAstNode> body_;   // +0x30
};
static_assert(sizeof(SeqCForLoop) == 0x38, "SeqCForLoop must be 0x38 bytes");

// ============================================================================
// List nodes (48 bytes, 0x30)
//
// Hold a vector<unique_ptr<SeqCAstNode>> at +0x18..+0x30.
// vector layout (24 bytes): begin/end/cap pointers.
// ============================================================================

#define SEQC_LIST(Name, NamedAccessor, VtableAddr)                           \
    class Name : public SeqCAstNode {                                       \
    public:                                                                 \
        Name(EValueCategory vc, int type, EDirection dir, VarType vt); \
        Name(EValueCategory vc, int type, EDirection dir, VarType vt,  \
             std::vector<std::unique_ptr<SeqCAstNode>> elements);           \
        Name(Name const& o);                                                \
        Name& operator=(Name o) { swap(*this, o); return *this; }           \
        ~Name() override;                                                   \
        void print() const override;                                        \
        std::unique_ptr<SeqCAstNode> clone() const override;                \
        std::vector<const SeqCAstNode*> children() const override;          \
        std::vector<std::string> getListElements() const override;          \
        std::shared_ptr<EvalResults> evaluate(                              \
            std::shared_ptr<Resources> res,                                 \
            FrontendLoweringContext& ctx,                                    \
            FrontendLoweringState& state) const override;                   \
                                                                            \
        void append(std::unique_ptr<SeqCAstNode> elem);                     \
        const std::vector<std::unique_ptr<SeqCAstNode>>& elements() const { \
            return elements_;                                               \
        }                                                                   \
        std::vector<const SeqCAstNode*> NamedAccessor() const;              \
                                                                            \
        friend void swap(Name& a, Name& b) {                               \
            swap(static_cast<SeqCAstNode&>(a),                              \
                 static_cast<SeqCAstNode&>(b));                             \
            std::swap(a.elements_, b.elements_);                            \
        }                                                                   \
    private:                                                                \
        std::vector<std::unique_ptr<SeqCAstNode>> elements_;  /* +0x18 */   \
    };                                                                      \
    static_assert(sizeof(Name) == 0x30, #Name " must be 0x30 bytes")

SEQC_LIST(SeqCArgList,   params, 0xb05238);
SEQC_LIST(SeqCDeclList,  decls,  0xb05288);
SEQC_LIST(SeqCStmtList,  stmts,  0xb05340);

#undef SEQC_LIST

// ----------------------------------------------------------------------------
// SeqCParamList — like the SEQC_LIST classes above, but with one extra
// public method `params()` that returns a vector<SeqCAstNode const*> of raw
// pointers into elements_. Used by Resources::Function::addArguments(node)
// @0x1eab70 to iterate the parameter nodes.
//
// vtable @0xb052d8.
// ----------------------------------------------------------------------------
class SeqCParamList : public SeqCAstNode {
public:
    SeqCParamList(EValueCategory vc, int type, EDirection dir, VarType vt);
    SeqCParamList(EValueCategory vc, int type, EDirection dir, VarType vt,
                  std::vector<std::unique_ptr<SeqCAstNode>> elements);
    SeqCParamList(SeqCParamList const& o);
    SeqCParamList& operator=(SeqCParamList o) { swap(*this, o); return *this; }
    ~SeqCParamList() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;
    std::vector<const SeqCAstNode*> children() const override;
    std::vector<std::string> getListElements() const override;   // @0x2007e0
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x211050

    std::vector<std::string> getVarTypes() const override;  // @0x200f20

    void append(std::unique_ptr<SeqCAstNode> elem);
    const std::vector<std::unique_ptr<SeqCAstNode>>& elements() const {
        return elements_;
    }

    std::vector<const SeqCAstNode*> params() const;  // @0x201050

    friend void swap(SeqCParamList& a, SeqCParamList& b) {
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
        std::swap(a.elements_, b.elements_);
    }

private:
    std::vector<std::unique_ptr<SeqCAstNode>> elements_;  // +0x18
};
static_assert(sizeof(SeqCParamList) == 0x30,
              "SeqCParamList must be 0x30 bytes");

// SeqCParameter class REMOVED in Phase 21d — VarType is now a proper field
// at +0x14 in the base class SeqCAstNode, accessible via varType(). The
// old placeholder's reinterpret_cast hack is no longer needed.

// ============================================================================
// Leaf nodes with payload
// ============================================================================

// SeqCVariable (48 bytes, 0x30) — name string at +0x18 (libc++ SSO, 24B)
class SeqCVariable : public SeqCAstNode {
public:
    SeqCVariable(EValueCategory vc, int type, EDirection dir,
                 VarType vt, std::string name);
    SeqCVariable(SeqCVariable const& o);
    SeqCVariable& operator=(SeqCVariable o) { swap(*this, o); return *this; }
    ~SeqCVariable() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;
    std::vector<std::string> getListElements() const override;   // 0x209e60
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x209ea0

    const std::string& name() const { return name_; }

    friend void swap(SeqCVariable& a, SeqCVariable& b) {
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
        std::swap(a.name_, b.name_);
    }

private:
    std::string name_;   // +0x18 (24B libc++ SSO; 32B with libstdc++)
};
// Size differs by libstdc++ vs libc++; both forms are accepted.
static_assert(sizeof(SeqCVariable) == 0x30 || sizeof(SeqCVariable) == 0x38,
              "SeqCVariable size mismatch — expected 0x30 (libc++) or 0x38 (libstdc++)");

// SeqCValue (56 bytes, 0x38) — tagged value variant.
// Layout TBD; binary uses jump table @0xb065a0 to dispatch destruction
// based on tag at +0x30 (value 0xFFFFFFFF means "no destructor").
class SeqCValue : public SeqCAstNode {
public:
    enum class Tag : int32_t {
        eString = 0,   // variant holds std::string at +0x18
        eDouble = 1,   // variant holds double at +0x18
        // -1 (0xFFFFFFFF) = empty/none (from dtor — skips destruction)
    };

    SeqCValue(EValueCategory vc, int type, EDirection dir, VarType vt);
    SeqCValue(EValueCategory vc, int type, EDirection dir, VarType vt,
              std::string const& s);   // 0x1fd860 (make_unique callsite)
    SeqCValue(EValueCategory vc, int type, EDirection dir, VarType vt,
              double d);               // 0x1fd950 (make_unique callsite)
    SeqCValue(SeqCValue const& o);
    SeqCValue& operator=(SeqCValue o) { swap(*this, o); return *this; }
    ~SeqCValue() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;

    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x213140

    Tag tag() const { return static_cast<Tag>(tag_); }

    double asDouble() const {
        double d;
        std::memcpy(&d, payload_, sizeof(double));
        return d;
    }

    const std::string& asString() const {
        return *reinterpret_cast<const std::string*>(payload_);
    }

    friend void swap(SeqCValue& a, SeqCValue& b) {
        swap(static_cast<SeqCAstNode&>(a), static_cast<SeqCAstNode&>(b));
        // Swap raw payload bytes + tag
        char tmp[24];
        std::memcpy(tmp, a.payload_, 24);
        std::memcpy(a.payload_, b.payload_, 24);
        std::memcpy(b.payload_, tmp, 24);
        std::swap(a.tag_, b.tag_);
        std::swap(a.pad34_, b.pad34_);
    }

private:
    char     payload_[24]{};   // +0x18..+0x30
    int32_t  tag_{-1};         // +0x30
    int32_t  pad34_{};         // +0x34
};
// Size differs by libstdc++ vs libc++ if payload_ uses std::string internally;
// with raw char[24] it's exact at 0x38.
static_assert(sizeof(SeqCValue) == 0x38, "SeqCValue must be 0x38 bytes");

} // namespace zhinst
