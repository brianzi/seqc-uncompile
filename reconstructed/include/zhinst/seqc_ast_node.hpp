// ============================================================================
// SeqCAstNode — new SeqC AST hierarchy with virtual lower()
//
// 53 named subclasses (NOT in anonymous namespace as initially suspected).
// Vtables clustered at .rodata 0xb04f60..0xb060c8.
//
// Base layout (24 bytes):
//   +0x00: vptr
//   +0x08: EValueCategory  (4 bytes)
//   +0x0C: int             type (4 bytes)
//   +0x10: EParamDirection (4 bytes)  -- see naming note below
//   +0x14: 4 bytes padding (alignment)
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
#include <memory>
#include <string>
#include <vector>

#include "zhinst/resources.hpp"  // pre-existing VarType enum used here

namespace zhinst {

// ---- Enums ----------------------------------------------------------------
//
// NOTE on naming: a separate `EDirection` (2-value Read/Write) already
// exists in resources.hpp from earlier reconstruction.  That name was
// admittedly inferred (resources.hpp:65: "No EDirection symbolic constants
// exist in the binary's symbol table").  This AST-side direction enum
// *does* have binary-confirmed values (3 entries; str()@0x1c1730 with a
// jump table), but rather than rename the entrenched 4-caller resources
// version we disambiguate the AST-side one as `EParamDirection`.  If the
// binary's true symbol for either is later recovered, a follow-up
// rename pass should reconcile them.

enum class EValueCategory : int32_t {
    eNOVALUECATEGORY = 0,
    eLVALUE          = 1,
    eRVALUE          = 2,
};

// str() @0x1c1730  — 3 values
enum class EParamDirection : int32_t {
    eIN    = 0,
    eOUT   = 1,
    eINOUT = 2,
};

// Free functions — enum to string
std::string str(EValueCategory vc);   // @0x1c16c0
std::string str(EParamDirection dir); // @0x1c1730

// ---- Base class ----------------------------------------------------------

// SeqCAstNode — abstract base, 24 bytes (0x18).
// vtable at .rodata 0xb06618.
//
// D0 (deleting) destructor at 0x2462e0 is `ud2` — abstract, must never
// be deleted directly through base pointer with `delete`.
class SeqCAstNode {
public:
    SeqCAstNode(EValueCategory vc, int type, EParamDirection dir);   // 0x1fda00
    virtual ~SeqCAstNode();                                      // D2 @0x209000 (trivial)

    // Virtual interface (vtable layout, slot order from inspection of
    // printSeqCAst calls on vtable[0x10] and vtable[0x18]):
    //   slot 0 (+0x00): RTTI offset (typeinfo)
    //   slot 1 (+0x08): typeinfo ptr
    //   slot 2 (+0x10): D2 base destructor
    //   slot 3 (+0x18): D0 deleting destructor
    //   slot 4 (+0x20): print()
    //   slot 5 (+0x28): clone()
    //   slot 6 (+0x30): children()             — returns vector<SeqCAstNode const*>
    //   slot 7 (+0x38): getListElements()      — returns vector<string>
    //   slot 8 (+0x40): evaluate(...)
    //
    // (These slot numbers are approximate; verified for slots 6 and 7 only
    //  from printSeqCAst @0x1fa3c0 and getListElements @0x209dd0.)

    virtual void print() const = 0;
    virtual std::unique_ptr<SeqCAstNode> clone() const = 0;
    virtual std::vector<const SeqCAstNode*> children() const;    // 0x1fda20 (default: empty)
    virtual std::vector<std::string> getListElements() const;    // 0x209dd0 (default: {""})
    // virtual ??? evaluate(...) const;  // TODO: signature TBD

    // Accessors
    EValueCategory valueCategory() const { return valueCategory_; }
    int            type()           const { return type_; }
    EParamDirection direction()      const { return direction_; }

protected:
    // Layout (must match binary):
    EValueCategory valueCategory_;   // +0x08
    int            type_;            // +0x0C  (int — purpose unclear, set from ctor arg #2)
    EParamDirection direction_;       // +0x10
    // 4 bytes padding to 0x18

    // Allow swap to access fields by reference
    friend void swap(SeqCAstNode& a, SeqCAstNode& b);            // 0x1fda40
};

static_assert(sizeof(SeqCAstNode) == 0x18,
              "SeqCAstNode must be exactly 0x18 (24) bytes");

// Free function — swaps the (valueCategory, type) qword and direction.
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
        using SeqCAstNode::SeqCAstNode;                                     \
        ~Name() override;                                                   \
        void print() const override;                                        \
        std::unique_ptr<SeqCAstNode> clone() const override;                \
        /* TODO: full method bodies */                                      \
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
        Name(EValueCategory vc, int type, EParamDirection dir,                   \
             std::unique_ptr<SeqCAstNode> child);                           \
        ~Name() override;                                                   \
        void print() const override;                                        \
        std::unique_ptr<SeqCAstNode> clone() const override;                \
        std::vector<const SeqCAstNode*> children() const override;          \
        const SeqCAstNode* child() const { return child_.get(); }           \
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
    SeqCOperator(EValueCategory vc, int type, EParamDirection dir,
                 std::unique_ptr<SeqCAstNode> lhs,
                 std::unique_ptr<SeqCAstNode> rhs);
    ~SeqCOperator() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;
    std::vector<const SeqCAstNode*> children() const override;

    const SeqCAstNode* lhs() const { return lhs_.get(); }
    const SeqCAstNode* rhs() const { return rhs_.get(); }

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
        ~Name() override;                                                   \
        void print() const override;                                        \
        std::unique_ptr<SeqCAstNode> clone() const override;                \
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
        Name(EValueCategory vc, int type, EParamDirection dir,                   \
             std::unique_ptr<SeqCAstNode> first,                            \
             std::unique_ptr<SeqCAstNode> second);                          \
        ~Name() override;                                                   \
        void print() const override;                                        \
        std::unique_ptr<SeqCAstNode> clone() const override;                \
        std::vector<const SeqCAstNode*> children() const override;          \
        const SeqCAstNode* FirstAccessor()  const { return first_.get(); }  \
        const SeqCAstNode* SecondAccessor() const { return second_.get(); } \
    private:                                                                \
        std::unique_ptr<SeqCAstNode> first_;   /* +0x18 */                  \
        std::unique_ptr<SeqCAstNode> second_;  /* +0x20 */                  \
    };                                                                      \
    static_assert(sizeof(Name) == 0x28, #Name " must be 0x28 bytes")

SEQC_BINARY(SeqCFunctionCall, function, args,  0xb05140);
SEQC_BINARY(SeqCArray,        index,    array, 0xb051e8);
SEQC_BINARY(SeqCIfCondition,  cond,     body,  0xb053e0);
SEQC_BINARY(SeqCCaseEntry,    value,    body,  0xb05518);
SEQC_BINARY(SeqCSwitchCase,   value,    body,  0xb05480);
SEQC_BINARY(SeqCWhileLoop,    cond,     body,  0xb055d0);
SEQC_BINARY(SeqCDoWhile,      body,     cond,  0xb05620);
SEQC_BINARY(SeqCRepeat,       count,    body,  0xb05670);

#undef SEQC_BINARY

// ============================================================================
// Three-child direct-AstNode subclasses (48 bytes, 0x30)
// ============================================================================

class SeqCIfElse : public SeqCAstNode {
public:
    SeqCIfElse(EValueCategory vc, int type, EParamDirection dir,
               std::unique_ptr<SeqCAstNode> cond,
               std::unique_ptr<SeqCAstNode> ifBody,
               std::unique_ptr<SeqCAstNode> elseBody);   // 0x202150
    ~SeqCIfElse() override;
    void print() const override;                          // 0x201df0
    std::unique_ptr<SeqCAstNode> clone() const override;  // 0x2021a0
    std::vector<const SeqCAstNode*> children() const override;  // 0x2022c0

    const SeqCAstNode* cond()     const { return cond_.get(); }      // 0x202320
    const SeqCAstNode* ifBody()   const { return ifBody_.get(); }    // 0x202330
    const SeqCAstNode* elseBody() const { return elseBody_.get(); }  // 0x202340

private:
    std::unique_ptr<SeqCAstNode> cond_;       // +0x18
    std::unique_ptr<SeqCAstNode> ifBody_;     // +0x20
    std::unique_ptr<SeqCAstNode> elseBody_;   // +0x28
};
static_assert(sizeof(SeqCIfElse) == 0x30, "SeqCIfElse must be 0x30 bytes");

class SeqCCondExpr : public SeqCAstNode {
public:
    SeqCCondExpr(EValueCategory vc, int type, EParamDirection dir,
                 std::unique_ptr<SeqCAstNode> cond,
                 std::unique_ptr<SeqCAstNode> trueBranch,
                 std::unique_ptr<SeqCAstNode> falseBranch);
    ~SeqCCondExpr() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;
    std::vector<const SeqCAstNode*> children() const override;

    const SeqCAstNode* cond()        const { return cond_.get(); }
    const SeqCAstNode* trueBranch()  const { return trueBranch_.get(); }
    const SeqCAstNode* falseBranch() const { return falseBranch_.get(); }

private:
    std::unique_ptr<SeqCAstNode> cond_;          // +0x18
    std::unique_ptr<SeqCAstNode> trueBranch_;    // +0x20
    std::unique_ptr<SeqCAstNode> falseBranch_;   // +0x28
};
static_assert(sizeof(SeqCCondExpr) == 0x30, "SeqCCondExpr must be 0x30 bytes");

// ============================================================================
// Four-child direct-AstNode subclasses (56 bytes, 0x38)
// ============================================================================

class SeqCFunction : public SeqCAstNode {
public:
    SeqCFunction(EValueCategory vc, int type, EParamDirection dir,
                 std::unique_ptr<SeqCAstNode> name,
                 std::unique_ptr<SeqCAstNode> returnType,
                 std::unique_ptr<SeqCAstNode> params,
                 std::unique_ptr<SeqCAstNode> body);
    ~SeqCFunction() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;
    std::vector<const SeqCAstNode*> children() const override;

    const SeqCAstNode* name()       const { return name_.get(); }
    const SeqCAstNode* returnType() const { return returnType_.get(); }
    const SeqCAstNode* params()     const { return params_.get(); }
    const SeqCAstNode* body()       const { return body_.get(); }

private:
    std::unique_ptr<SeqCAstNode> name_;        // +0x18
    std::unique_ptr<SeqCAstNode> returnType_;  // +0x20
    std::unique_ptr<SeqCAstNode> params_;      // +0x28
    std::unique_ptr<SeqCAstNode> body_;        // +0x30
};
static_assert(sizeof(SeqCFunction) == 0x38, "SeqCFunction must be 0x38 bytes");

class SeqCForLoop : public SeqCAstNode {
public:
    SeqCForLoop(EValueCategory vc, int type, EParamDirection dir,
                std::unique_ptr<SeqCAstNode> init,
                std::unique_ptr<SeqCAstNode> cond,
                std::unique_ptr<SeqCAstNode> incr,
                std::unique_ptr<SeqCAstNode> body);     // 0x202f00
    ~SeqCForLoop() override;
    void print() const override;                         // 0x202bc0
    std::unique_ptr<SeqCAstNode> clone() const override; // 0x202f70
    std::vector<const SeqCAstNode*> children() const override;  // 0x202fd0

    const SeqCAstNode* init() const { return init_.get(); }   // 0x203020
    const SeqCAstNode* cond() const { return cond_.get(); }   // 0x203030
    const SeqCAstNode* incr() const { return incr_.get(); }   // 0x203040
    const SeqCAstNode* body() const { return body_.get(); }   // 0x203050

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

#define SEQC_LIST(Name, VtableAddr)                                         \
    class Name : public SeqCAstNode {                                       \
    public:                                                                 \
        Name(EValueCategory vc, int type, EParamDirection dir);                  \
        Name(EValueCategory vc, int type, EParamDirection dir,                   \
             std::vector<std::unique_ptr<SeqCAstNode>> elements);           \
        ~Name() override;                                                   \
        void print() const override;                                        \
        std::unique_ptr<SeqCAstNode> clone() const override;                \
        std::vector<const SeqCAstNode*> children() const override;          \
                                                                            \
        void append(std::unique_ptr<SeqCAstNode> elem);                     \
        const std::vector<std::unique_ptr<SeqCAstNode>>& elements() const { \
            return elements_;                                               \
        }                                                                   \
                                                                            \
    private:                                                                \
        std::vector<std::unique_ptr<SeqCAstNode>> elements_;  /* +0x18 */   \
    };                                                                      \
    static_assert(sizeof(Name) == 0x30, #Name " must be 0x30 bytes")

SEQC_LIST(SeqCArgList,   0xb05238);
SEQC_LIST(SeqCDeclList,  0xb05288);
SEQC_LIST(SeqCParamList, 0xb052d8);
SEQC_LIST(SeqCStmtList,  0xb05340);

#undef SEQC_LIST

// ============================================================================
// Leaf nodes with payload
// ============================================================================

// SeqCVariable (48 bytes, 0x30) — name string at +0x18 (libc++ SSO, 24B)
class SeqCVariable : public SeqCAstNode {
public:
    SeqCVariable(EValueCategory vc, int type, EParamDirection dir,
                 std::string name);
    ~SeqCVariable() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;
    std::vector<std::string> getListElements() const override;   // 0x209e60

    const std::string& name() const { return name_; }

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
        eNone   = 0,
        eInt    = 1,
        eDouble = 2,
        eString = 3,
        // TODO: full enumeration TBD
    };

    SeqCValue(EValueCategory vc, int type, EParamDirection dir);
    ~SeqCValue() override;
    void print() const override;
    std::unique_ptr<SeqCAstNode> clone() const override;

    // TODO: value accessors / variant interface
private:
    // Layout (approximate, from D0 dtor at 0x208aa0):
    //   +0x18: 16 bytes payload (string SSO header / int / double)
    //   +0x28: 8 bytes payload tail
    //   +0x30: int32_t tag       (0xFFFFFFFF = none, otherwise jump-table index)
    //   +0x34: 4 bytes padding
    char     payload_[24]{};   // +0x18..+0x30
    int32_t  tag_{-1};         // +0x30
    int32_t  pad34_{};         // +0x34
};
// Size differs by libstdc++ vs libc++ if payload_ uses std::string internally;
// with raw char[24] it's exact at 0x38.
static_assert(sizeof(SeqCValue) == 0x38, "SeqCValue must be 0x38 bytes");

} // namespace zhinst
