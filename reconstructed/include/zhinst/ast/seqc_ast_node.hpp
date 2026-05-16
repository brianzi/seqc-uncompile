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
//   +0x14: VarType         (4 bytes)  -- NOT padding; set by every
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

#include "zhinst/runtime/resources.hpp"  // pre-existing VarType enum used here

namespace zhinst {

// Forward declarations for evaluate() signature
class FrontendLoweringContext;
class FrontendLoweringState;
class EvalResults;

// ---- Enums ----------------------------------------------------------------

//! \brief Value-category tag carried by every SeqC AST node.
//!
//! Classifies the result the node yields when evaluated.  The enumerator
//! string forms produced by `str(EValueCategory)` are
//! `"eNOVALUECATEGORY"`, `"eLVALUE"`, and `"eRVALUE"`.
enum class EValueCategory : int32_t {
    eNOVALUECATEGORY = 0,   //!< \brief No category yet assigned (parser-default).
    eLVALUE          = 1,   //!< \brief Addressable result (assignable target).
    eRVALUE          = 2,   //!< \brief Non-addressable result (computed value).
};

// EDirection — unified enum, defined in types.hpp.
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
// ---- Vtable layout ----
//
// The declaration order of virtuals in the ORIGINAL source determines
// the vtable slot order. The order below matches the binary's vtable
// layout EXACTLY (evaluate first, dtors near the end):
//
//   vptr[0] (+0x00): evaluate(3-arg)      — the primary lowering dispatch
//   vptr[1] (+0x08): getListElements()    — returns vector<string>
//   vptr[2] (+0x10): children()           — returns vector<SeqCAstNode const*>
//   vptr[3] (+0x18): print()              [pure virtual in base]
//   vptr[4] (+0x20): doClone()              [pure virtual in base]
//   vptr[5] (+0x28): ~D2 dtor
//   vptr[6] (+0x30): ~D0 dtor
//   vptr[7] (+0x38): getVarTypes()        — returns vector<string>
//
// SeqCOperator adds:
//   vptr[8] (+0x40): evaluate(5-arg)      — binary-operator specialization
//
//! \brief Abstract base of the SeqC AST hierarchy.
//!
//! `SeqCAstNode` is the common interface for every node produced by
//! lowering a parsed `Expression` tree.  Each subclass overrides the
//! virtual `evaluate()` to drive the next phase of compilation: it
//! returns a `std::shared_ptr<EvalResults>` that bundles any synthesised
//! IR `Node`, emitted assembler instructions, constant value, and
//! waveform metadata for the subtree it represents.  Other virtuals are
//! `print()` for diagnostic dumps, `doClone()` for deep-copying subtrees,
//! `children()` for generic tree-walking, `getListElements()` and
//! `getVarTypes()` for parameter/argument list introspection.
//!
//! Common fields stored on every node are the parsed source line number,
//! the value category (lvalue/rvalue), the parameter direction
//! (in/out/inout), and the variable type tag.  Subclasses extend the
//! base layout with strongly-typed unique-ptr children: see the
//! family-specific groupings (`SEQC_TRIVIAL_LEAF`, `SEQC_UNARY`,
//! `SEQC_OPERATOR`, `SEQC_BINARY`, `SEQC_LIST`) below for the 53 concrete
//! node kinds.
class SeqCAstNode {
public:
    //! \brief Constructs the AST-node base with the value-category /
    //! source-line / parameter-direction trio used by every derived
    //! kind; derived constructors set `varType_` directly.
    //! \param vc     Value-category tag (lvalue / rvalue / ...).
    //! \param lineNr 1-based source line number for diagnostics.
    //! \param dir    Parameter-direction tag (in / out / inout).
    SeqCAstNode(EValueCategory vc, int lineNr, EDirection dir);   // 0x1fda00
    // Binary: base ctor takes 3 args; derived ctors all take VarType as 4th
    // and write it to varType_ directly (the binary inlines the base ctor).

    // --- Virtual methods in declaration (= vtable) order ---

    // vptr[0]: primary evaluate dispatch. Returns shared_ptr<EvalResults> via sret.
    // Base implementation @0x209dc0 returns null shared_ptr (zeroes 16 bytes).
    // Binary TU: SeqCAstNodesEvaluate.cpp (from _GLOBAL__sub_I_ symbol).
    //! \brief Lowers the subtree rooted at `*this`, returning the
    //! `EvalResults` produced by the evaluation.
    //! \details The base implementation returns a null shared_ptr;
    //! every concrete node kind overrides it to perform its specific
    //! lowering against `res` (symbol table), `ctx` (compiler
    //! subsystems), and `state` (mutable accumulator).
    //! \param res   Symbol-table / resource container shared across
    //!              the evaluation.
    //! \param ctx   Compiler subsystems handle (waveform manager,
    //!              assemblers, message sinks, ...).
    //! \param state Mutable per-evaluation accumulator threaded
    //!              through every node visit.
    //! \return The `EvalResults` produced by lowering this subtree
    //! (null from the base; non-null from every concrete override).
    virtual std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const;               // @0x209dc0

    // vptr[1]: default returns vector with single empty string.
    //! \brief Returns the textual element list this node contributes
    //! to a parameter-list / argument-list rendering; the base
    //! returns a single empty string and list-shaped derived nodes
    //! override it to return one element per child.
    //! \return Element strings contributed by this node (one empty
    //! string from the base; one entry per child from list nodes).
    virtual std::vector<std::string> getListElements() const;    // @0x209dd0

    // vptr[2]: default returns empty vector.
    //! \brief Returns the immediate child nodes for tree traversal;
    //! the base returns an empty vector and composite nodes
    //! override it to expose their owned children.
    //! \return Pointers to the immediate children (empty from the
    //! base; one entry per owned child from composite nodes).
    virtual std::vector<const SeqCAstNode*> children() const;    // @0x1fda20

    // vptr[3-4]: pure virtual — print/doClone.
    //! \brief Pretty-prints this node (and, by recursion, its
    //! subtree) to `std::cout`; pure-virtual, every concrete kind
    //! overrides it.
    virtual void print() const = 0;
    //! \brief Deep-clones this node; pure-virtual, derived kinds
    //! return a `std::unique_ptr` to a fresh copy of themselves
    //! including all owned children.
    //! \return Owning pointer to a deep copy of this node and its
    //! owned subtree.
    virtual std::unique_ptr<SeqCAstNode> doClone() const = 0;

    // vptr[5-6]: destructor (D2 + D0).
    //! \brief Virtual destructor; the base body is trivial — owned
    //! children, if any, are released by the concrete subclass.
    virtual ~SeqCAstNode();                                      // D2 @0x209000 (trivial)

    // vptr[7]: returns vector<string> of VarType names for parameter nodes.
    // Default implementation @0x1fdb40 (ICF-merged across trivial leaves):
    // returns vector with single element str(varType_).
    //! \brief Returns the variable-type names contributed by this
    //! node to a function-signature rendering; the base returns
    //! `{ str(varType_) }` and parameter-list / list nodes override
    //! it to concatenate their children's types.
    //! \return Variable-type names contributed by this node (one
    //! entry from the base; one per parameter from list nodes).
    virtual std::vector<std::string> getVarTypes() const;        // @0x1fdb40

    // Accessors
    //! \brief Returns the value-category tag (lvalue/rvalue/...)
    //! recorded at construction.
    //! \return The stored `EValueCategory`.
    EValueCategory  valueCategory() const { return valueCategory_; }
    //! \brief Returns the 1-based source line number recorded at
    //! construction.
    //! \return The 1-based source line number.
    int             lineNr()        const { return lineNr_; }
    //! \brief Returns the parameter-direction tag (in/out/inout)
    //! recorded at construction.
    //! \return The stored `EDirection`.
    EDirection direction()     const { return direction_; }
    //! \brief Returns the variable-type tag attached to this node.
    //! \return The stored `VarType`.
    VarType         varType()       const { return varType_; }
    //! \brief Overwrites this node's variable-type tag; used by the
    //! parser actions that resolve declarations after the AST has
    //! been built.
    //! \param vt New variable-type tag to install.
    void            setVarType(VarType vt) { varType_ = vt; }

protected:
    // Layout (must match binary — all 16 bytes after vptr):
    //! \brief Value-category tag (lvalue / rvalue / ...) supplied at
    //! construction and exposed via `valueCategory()`.
    EValueCategory  valueCategory_;  // +0x08
    //! \brief 1-based source line number recorded at construction;
    //! used by diagnostics and AST printing.
    //! \note `SeqCVariable::print()` reinterprets this slot as a
    //! `VarType` for display — an overloaded meaning in that one
    //! subclass.
    int             lineNr_;         // +0x0C  Source line number. (Resolves unknown #96.)
                                     //        Binary: SeqCVariable::print() casts this to VarType
                                     //        for display — overloaded meaning in that one subclass.
    //! \brief Parameter-direction tag (in / out / inout) supplied at
    //! construction and exposed via `direction()`.
    EDirection direction_;      // +0x10
    //! \brief Variable-type tag attached to this node; default-
    //! constructed and may be overwritten by `setVarType()`.
    VarType         varType_{};      // +0x14

    // Allow swap to access fields by reference
    friend void swap(SeqCAstNode& a, SeqCAstNode& b);            // 0x1fda40
};

static_assert(sizeof(SeqCAstNode) == 0x18,
              "SeqCAstNode must be exactly 0x18 (24) bytes");

// Free function — swaps the (valueCategory, lineNr) qword and direction.
// Note: does NOT swap vptrs.
//! \brief Field-level swap of two `SeqCAstNode` bases: exchanges
//! `valueCategory_`, `lineNr_`, and `direction_`.
//! \binarynote The vptrs are intentionally **not** swapped, so
//! the dynamic types of `a` and `b` are preserved across the
//! call; only the base-class scalar fields move.
//! \param a First node.
//! \param b Second node.
void swap(SeqCAstNode& a, SeqCAstNode& b);                       // 0x1fda40

// Free function — pretty-prints an AST tree to std::cout.
// Internally calls anonymous-namespace helper @0x1fa430 with empty indent.
//! \brief Pretty-prints the AST rooted at `node` to `std::cout`,
//! recursing through `children()` with increasing indentation.
//! \param node Root of the AST subtree to print.
void printSeqCAst(const SeqCAstNode& node);                      // 0x1fa3c0

// ============================================================================
// Direct AstNode subclasses with no extra fields (24 bytes, 0x18)
// ============================================================================

#define SEQC_TRIVIAL_LEAF(Name, VtableAddr)                                 \
    /*! \brief Trivial-leaf AST node — carries no children or extra        \
     *  payload beyond the standard `SeqCAstNode` base fields.             \
     *                                                                      \
     *  All six trivial-leaf kinds (`SeqCCommand`, `SeqCVariableType`,     \
     *  `SeqCLabel`, `SeqCContinueStatement`, `SeqCBreakStatement`,        \
     *  `SeqCNoCmd`) share an identical layout and an identical            \
     *  implementation modulo the printed label and the per-class vtable. \
     *  Total size is 0x18 bytes (statically asserted below).              \
     */                                                                     \
    class Name : public SeqCAstNode {                                       \
    public:                                                                 \
        /*! \brief Constructs the leaf with the base-class attributes;     \
         *  no further state is held.                                       \
         *  \param vc     Value category of the node.                       \
         *  \param lineNr 1-based source line number.                       \
         *  \param dir    Parameter-direction tag.                          \
         *  \param vt     Variable-type tag assigned to `varType_`.        \
         */                                                                 \
        Name(EValueCategory vc, int lineNr, EDirection dir,              \
             VarType vt)                                                    \
            : SeqCAstNode(vc, lineNr, dir) { varType_ = vt; }                \
        /*! \brief Copy constructor — copies the base AST attributes;      \
         *  no owned children to clone.                                     \
         *  \param o Source node copied from.                               \
         */                                                                 \
        Name(Name const& o);                                                \
        /*! \brief Copy-and-swap assignment.                                \
         *  \param o Source node, copied on entry and swapped on exit.     \
         *  \return Reference to `*this`.                                   \
         */                                                                 \
        Name& operator=(Name o);                                            \
        /*! \brief Default destructor. */                                   \
        ~Name() override;                                                   \
        /*! \brief Writes the class label to `std::cout` as a raw          \
         *  `std::cout.write()` with no trailing newline; used by the      \
         *  AST debug-print walker.                                         \
         */                                                                 \
        void print() const override;                                        \
        /*! \brief Polymorphic deep clone — returns a fresh instance of    \
         *  the same concrete leaf type with identical base attributes.   \
         *  \return Owning pointer to the cloned node.                      \
         */                                                                 \
        std::unique_ptr<SeqCAstNode> doClone() const override;                \
        /*! \brief Lowers the leaf for this AST kind, returning the        \
         *  `EvalResults` produced by its evaluation.                       \
         *  \param res   Shared compile resources (symbol tables, etc.).   \
         *  \param ctx   Lowering context (messages, asm commands, etc.). \
         *  \param state Per-call lowering state.                          \
         *  \return The `EvalResults` produced by lowering this node.       \
         */                                                                 \
        std::shared_ptr<EvalResults> evaluate(                              \
            std::shared_ptr<Resources> res,                                 \
            FrontendLoweringContext& ctx,                                    \
            FrontendLoweringState& state) const override;                   \
        /*! \brief ADL-friendly swap — exchanges only the                  \
         *  `SeqCAstNode` base subobject (trivial leaves carry no extra   \
         *  owned fields).                                                  \
         *  \param a First leaf to swap.                                    \
         *  \param b Second leaf to swap.                                   \
         */                                                                 \
        friend void swap(Name& a, Name& b);                                \
    };                                                                      \
    static_assert(sizeof(Name) == 0x18, #Name " must be 0x18 bytes")

// SeqCOperation — broken out of SEQC_TRIVIAL_LEAF for extra getVarTypes override.
// vtable @0xb04f60.
//! \brief Parameter-direction-aware leaf node.
//!
//! Trivial-leaf operation node that overrides `getVarTypes()` to expose
//! the parameter's direction (in/out/inout) alongside its variable type
//! when used as a function parameter slot.
class SeqCOperation : public SeqCAstNode {
public:
    //! \brief Constructs the operation leaf with the base AST attributes
    //! and the variable-type tag used by `getVarTypes()` to expose the
    //! parameter's type at signature-rendering time.
    //! \param vc     Value-category tag.
    //! \param lineNr 1-based source line number.
    //! \param dir    Parameter-direction tag (in/out/inout).
    //! \param vt     Variable-type tag assigned to `varType_`.
    SeqCOperation(EValueCategory vc, int lineNr, EDirection dir,
                  VarType vt);  // out-of-line for symbol emission
    //! \brief Copy constructor — copies the base AST attributes; no
    //! owned children to clone.
    //! \param o Source node copied from.
    SeqCOperation(SeqCOperation const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source node, copied on entry and swapped on exit.
    //! \return Reference to `*this`.
    SeqCOperation& operator=(SeqCOperation o);
    //! \brief Default destructor.
    ~SeqCOperation() override;
    //! \brief Writes the label `"Operation"` to `std::cout`.
    void print() const override;
    //! \brief Polymorphic deep clone — returns a fresh `SeqCOperation`
    //! with identical base attributes.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;
    //! \brief Lowers the operation node, returning the `EvalResults`
    //! produced by its evaluation.
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by lowering this node.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;
    //! \brief Returns `{ str(varType_) }` so the parameter's
    //! variable-type name participates in function-signature rendering.
    //! \return Single-element vector with the variable-type's string
    //! form.
    std::vector<std::string> getVarTypes() const override;  // @0x1fdb40
    //! \brief ADL-friendly swap — exchanges only the `SeqCAstNode`
    //! base subobject (operation nodes carry no extra owned fields).
    //! \param a First operation node.
    //! \param b Second operation node.
    friend void swap(SeqCOperation& a, SeqCOperation& b);
};
static_assert(sizeof(SeqCOperation) == 0x18, "SeqCOperation must be 0x18 bytes");

//! \brief Generic command leaf — placeholder node for parser-emitted
//! command statements that carry no operands of their own.
SEQC_TRIVIAL_LEAF(SeqCCommand,           0xb05050);
//! \brief Variable-type declarator leaf used to carry a parsed type
//! name (e.g. function return type) through the AST without operands.
SEQC_TRIVIAL_LEAF(SeqCVariableType,      0xb050a0);
//! \brief Identifier-label leaf used for labelled targets that need
//! no additional payload beyond the base `varType_` slot.
SEQC_TRIVIAL_LEAF(SeqCLabel,             0xb05390);
//! \brief `continue` statement leaf — jumps to the next loop iteration
//! at lowering time when emitted inside a loop body.
SEQC_TRIVIAL_LEAF(SeqCContinueStatement, 0xb05710);
//! \brief `break` statement leaf — exits the innermost loop or
//! `switch` body at lowering time.
SEQC_TRIVIAL_LEAF(SeqCBreakStatement,    0xb05760);
//! \brief No-op command leaf — produced by the parser as a placeholder
//! where a statement is grammatically required but semantically empty.
SEQC_TRIVIAL_LEAF(SeqCNoCmd,             0xb05940);

#undef SEQC_TRIVIAL_LEAF

// ============================================================================
// Single-child unary-expression-like nodes (32 bytes, 0x20)
//
// Layout: SeqCAstNode (24B) + unique_ptr<SeqCAstNode> child_ (8B)
// ============================================================================

#define SEQC_UNARY(Name, VtableAddr)                                        \
    /*! \brief Single-child unary AST node — wraps one owned operand      \
     *  expression as `child_` at offset +0x18.                            \
     *                                                                      \
     *  All five unary kinds (`SeqCNeg`, `SeqCPos`, `SeqCInv`,             \
     *  `SeqCNotExpr`, `SeqCReturnStatement`) share an identical layout   \
     *  and an identical implementation modulo the printed label, the    \
     *  per-class vtable, and the lowering rule in `evaluate()`.          \
     *  Total size is 0x20 bytes (statically asserted below).             \
     */                                                                     \
    class Name : public SeqCAstNode {                                       \
    public:                                                                 \
        /*! \brief Construct the unary node, taking ownership of the      \
         *  operand subtree.                                                \
         *  \param vc     Value category of the node.                       \
         *  \param lineNr 1-based source line number.                       \
         *  \param dir    Parameter-direction tag.                          \
         *  \param vt     Variable-type tag assigned to `varType_`.        \
         *  \param child  Owned operand subexpression; moved into          \
         *                `child_`.                                        \
         */                                                                 \
        Name(EValueCategory vc, int lineNr, EDirection dir,              \
             VarType vt,                                                    \
             std::unique_ptr<SeqCAstNode> child);                           \
        /*! \brief Deep-copy constructor — clones the operand via         \
         *  `doClone()` (null operands are preserved as null).             \
         *  \param o Source node copied from.                              \
         */                                                                 \
        Name(Name const& o);                                                \
        /*! \brief Copy-and-swap assignment.                                \
         *  \param o Source node, copied on entry and swapped on exit.     \
         *  \return Reference to `*this`.                                   \
         */                                                                 \
        Name& operator=(Name o);                                            \
        /*! \brief Default destructor; releases the owned operand. */      \
        ~Name() override;                                                   \
        /*! \brief Writes the class label to `std::cout`. */               \
        void print() const override;                                        \
        /*! \brief Polymorphic deep clone — returns a fresh instance     \
         *  of the same concrete unary kind with a cloned operand.        \
         *  \return Owning pointer to the cloned node.                      \
         */                                                                 \
        std::unique_ptr<SeqCAstNode> doClone() const override;                \
        /*! \brief Returns a one-entry vector with the operand pointer    \
         *  (may be null) for generic tree-walking.                        \
         *  \return Vector of borrowed child pointers (size 1).             \
         */                                                                 \
        std::vector<const SeqCAstNode*> children() const override;          \
        /*! \brief Lowers the unary form: evaluates the operand and       \
         *  applies the operator-specific arithmetic / side effect.        \
         *  \param res   Shared compile resources.                          \
         *  \param ctx   Lowering context.                                  \
         *  \param state Per-call lowering state.                          \
         *  \return The `EvalResults` produced by lowering this node.       \
         */                                                                 \
        std::shared_ptr<EvalResults> evaluate(                              \
            std::shared_ptr<Resources> res,                                 \
            FrontendLoweringContext& ctx,                                    \
            FrontendLoweringState& state) const override;                   \
        /*! \brief Borrowed pointer to the operand subexpression.          \
         *  \return Non-owning pointer to the held child (may be null).    \
         */                                                                 \
        const SeqCAstNode* expr() const { return child_.get(); }            \
        /*! \brief ADL-friendly swap — exchanges the base subobject and  \
         *  the operand pointer.                                            \
         *  \param a First node to swap.                                    \
         *  \param b Second node to swap.                                   \
         */                                                                 \
        friend void swap(Name& a, Name& b);                                \
    private:                                                                \
        /*! \brief Owned operand subexpression (`child_`); accessed       \
         *  through `expr()`.                                              \
         */                                                                 \
        std::unique_ptr<SeqCAstNode> child_;  /* +0x18 */                   \
    };                                                                      \
    static_assert(sizeof(Name) == 0x20, #Name " must be 0x20 bytes")

//! \brief Unary minus expression `-expr`.
SEQC_UNARY(SeqCNeg,             0xb05800);
//! \brief Unary plus expression `+expr`.
SEQC_UNARY(SeqCPos,             0xb05850);
//! \brief Bitwise complement expression `~expr`.
SEQC_UNARY(SeqCInv,             0xb058a0);
//! \brief Logical negation expression `!expr`.
SEQC_UNARY(SeqCNotExpr,         0xb058f0);
//! \brief `return expr;` statement — wraps the optional return-value
//! subexpression and signals the enclosing statement list to stop
//! evaluating subsequent statements.
SEQC_UNARY(SeqCReturnStatement, 0xb057b0);

#undef SEQC_UNARY

// ============================================================================
// SeqCOperator + 22 binary-op subclasses (40 bytes, 0x28)
//
// All 21 derived operator classes share the SeqCOperator vtable for cleanup
// (vptr reset to 0xb051a0 in their D0 dtors), have identical layout and
// only differ in their own vtable's print/doClone/evaluate overrides.
// ============================================================================

//! \brief Binary-operator AST node base.
//!
//! `SeqCOperator` is the shared base for all 22 binary operator and
//! assignment nodes (arithmetic, bitwise, comparison, logical,
//! increment/decrement, plain assignment, no-op).  Holds the left- and
//! right-hand subexpressions as owning unique-ptrs and adds an extra
//! virtual `evaluate()` overload that takes the already-lowered operand
//! results so the standard 3-argument `evaluate()` can lower the
//! operands once and then dispatch to the operator-specific arithmetic
//! in derived classes.
class SeqCOperator : public SeqCAstNode {
public:
    //! \brief Constructs the operator node, taking ownership of both
    //! operand subtrees.
    //! \param vc     Value category of the node.
    //! \param lineNr 1-based source line number.
    //! \param dir    Parameter-direction tag.
    //! \param vt     Variable-type tag assigned to `varType_`.
    //! \param lhs    Owned left-hand operand subtree.
    //! \param rhs    Owned right-hand operand subtree.
    SeqCOperator(EValueCategory vc, int lineNr, EDirection dir,
                 VarType vt,
                 std::unique_ptr<SeqCAstNode> lhs,
                 std::unique_ptr<SeqCAstNode> rhs);
    //! \brief Deep-copy constructor — clones both operand subtrees via
    //! `doClone()`.
    //! \param o Source node copied from.
    SeqCOperator(SeqCOperator const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source operator, copied on entry and swapped on exit.
    //! \return Reference to `*this`.
    SeqCOperator& operator=(SeqCOperator o);
    //! \brief Default destructor; releases both owned operands.
    ~SeqCOperator() override;
    //! \brief Writes the label `"Operator"` to `std::cout`; overridden
    //! by each concrete subclass to emit its own kind label.
    void print() const override;
    //! \brief Polymorphic deep clone — returns a fresh `SeqCOperator`
    //! base instance; overridden by each concrete subclass to clone as
    //! its own kind.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;
    //! \brief Returns the two operand pointers in declaration order
    //! (`lhs`, `rhs`).
    //! \return Two-entry vector of borrowed child pointers.
    std::vector<const SeqCAstNode*> children() const override;

    //! \brief Three-argument lowering entry point — lowers both
    //! operands once and dispatches to the concrete-kind 5-argument
    //! `evaluate()` with the already-lowered operand results.
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by lowering this operator.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x210aa0

    //! \brief Operator-specific evaluation overload — receives the
    //! already-lowered operand results and applies the concrete-kind
    //! arithmetic / comparison / assignment.  The base implementation
    //! returns a null `EvalResults`; every concrete operator subclass
    //! overrides it.
    //! \param res       Shared compile resources.
    //! \param ctx       Lowering context.
    //! \param state     Per-call lowering state.
    //! \param lhsResult `EvalResults` produced by lowering `lhs()`.
    //! \param rhsResult `EvalResults` produced by lowering `rhs()`.
    //! \return The `EvalResults` produced by applying this operator.
    virtual std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state,
        EvalResults const& lhsResult,
        EvalResults const& rhsResult) const;       // vptr[8]

    //! \brief Borrowed pointer to the left-hand operand subtree.
    //! \return Non-owning pointer to `lhs_` (may be null).
    const SeqCAstNode* lhs() const { return lhs_.get(); }
    //! \brief Borrowed pointer to the right-hand operand subtree.
    //! \return Non-owning pointer to `rhs_` (may be null).
    const SeqCAstNode* rhs() const { return rhs_.get(); }

    //! \brief ADL-friendly swap — exchanges the `SeqCAstNode` base
    //! subobject and both operand pointers.
    //! \param a First operator node.
    //! \param b Second operator node.
    friend void swap(SeqCOperator& a, SeqCOperator& b);

protected:
    //! \brief Owned left-hand operand subexpression; accessed through
    //! `lhs()`.
    std::unique_ptr<SeqCAstNode> lhs_;   // +0x18
    //! \brief Owned right-hand operand subexpression; accessed
    //! through `rhs()`.
    std::unique_ptr<SeqCAstNode> rhs_;   // +0x20
};

static_assert(sizeof(SeqCOperator) == 0x28,
              "SeqCOperator must be 0x28 (40) bytes");

#define SEQC_OPERATOR(Name, VtableAddr)                                     \
    /*! \brief Concrete binary-operator AST node.                          \
     *                                                                      \
     *  One of the 22 binary-operator / assignment / no-op subclasses of  \
     *  `SeqCOperator` (arithmetic, bitwise, shift, comparison, logical, \
     *  increment/decrement, plain assignment, no-op).  All 22 share the  \
     *  `SeqCOperator` layout (`lhs_` at +0x18, `rhs_` at +0x20, total    \
     *  size 0x28 bytes) and only differ in their per-class vtable —     \
     *  specifically in the overrides of `print()`, `doClone()`, and the \
     *  5-argument `evaluate()` that consumes already-lowered operand    \
     *  results.                                                           \
     */                                                                     \
    class Name : public SeqCOperator {                                      \
    public:                                                                 \
        using SeqCOperator::SeqCOperator;                                   \
        /*! \brief Deep-copy constructor — forwards to the              \
         *  `SeqCOperator` copy constructor (which clones both           \
         *  operands).                                                     \
         *  \param o Source node copied from.                             \
         */                                                                 \
        Name(Name const& o);                                                \
        /*! \brief Copy-and-swap assignment via the `SeqCOperator` base \
         *  swap.                                                           \
         *  \param o Source node, copied on entry and swapped on exit.     \
         *  \return Reference to `*this`.                                   \
         */                                                                 \
        Name& operator=(Name o);                                            \
        /*! \brief Default destructor; resets the vptr so the base       \
         *  `SeqCOperator` cleanup releases the owned operands.           \
         */                                                                 \
        ~Name() override;                                                   \
        /*! \brief Writes the class label to `std::cout`. */               \
        void print() const override;                                        \
        /*! \brief Polymorphic deep clone — returns a fresh instance     \
         *  of the same concrete operator kind with cloned `lhs_` and    \
         *  `rhs_`.                                                        \
         *  \return Owning pointer to the cloned node.                      \
         */                                                                 \
        std::unique_ptr<SeqCAstNode> doClone() const override;                \
        /*! \brief Operator-specific evaluation against already-lowered  \
         *  operand results: applies the kind's arithmetic / comparison /\
         *  bitwise / logical / assignment behaviour to `lhsResult` and  \
         *  `rhsResult` and returns the resulting `EvalResults`.          \
         *  \param res       Shared compile resources.                     \
         *  \param ctx       Lowering context.                             \
         *  \param state     Per-call lowering state.                      \
         *  \param lhsResult `EvalResults` produced by lowering `lhs()`.  \
         *  \param rhsResult `EvalResults` produced by lowering `rhs()`.  \
         *  \return The `EvalResults` produced by applying this operator. \
         */                                                                 \
        std::shared_ptr<EvalResults> evaluate(                              \
            std::shared_ptr<Resources> res,                                 \
            FrontendLoweringContext& ctx,                                    \
            FrontendLoweringState& state,                                   \
            EvalResults const& lhsResult,                                   \
            EvalResults const& rhsResult) const override;                   \
    };                                                                      \
    static_assert(sizeof(Name) == 0x28, #Name " must be 0x28 bytes")

//! \brief Addition / string-concatenation operator (`lhs + rhs`).
SEQC_OPERATOR(SeqCPlus,    0xb05990);
//! \brief Subtraction operator (`lhs - rhs`).
SEQC_OPERATOR(SeqCMinus,   0xb059e8);
//! \brief Multiplication operator (`lhs * rhs`).
SEQC_OPERATOR(SeqCMult,    0xb05a40);
//! \brief Division operator (`lhs / rhs`).
SEQC_OPERATOR(SeqCDiv,     0xb05a98);
//! \brief Modulo operator (`lhs % rhs`).
SEQC_OPERATOR(SeqCMod,     0xb05af0);
//! \brief Bitwise right-shift operator (`lhs >> rhs`).
SEQC_OPERATOR(SeqCShiftR,  0xb05b48);
//! \brief Bitwise left-shift operator (`lhs << rhs`).
SEQC_OPERATOR(SeqCShiftL,  0xb05ba0);
//! \brief Greater-than comparison operator (`lhs > rhs`).
SEQC_OPERATOR(SeqCGreater, 0xb05bf8);
//! \brief Less-than comparison operator (`lhs < rhs`).
SEQC_OPERATOR(SeqCLower,   0xb05c50);
//! \brief Less-than-or-equal comparison operator (`lhs <= rhs`).
SEQC_OPERATOR(SeqCLEqual,  0xb05ca8);
//! \brief Greater-than-or-equal comparison operator (`lhs >= rhs`).
SEQC_OPERATOR(SeqCGEqual,  0xb05d00);
//! \brief Equality comparison operator (`lhs == rhs`).
SEQC_OPERATOR(SeqCEqual,   0xb05d58);
//! \brief Inequality comparison operator (`lhs != rhs`).
SEQC_OPERATOR(SeqCNEqual,  0xb05db0);
//! \brief Post-increment / compound-assignment-style increment operator.
SEQC_OPERATOR(SeqCInc,     0xb05e08);
//! \brief Post-decrement / compound-assignment-style decrement operator.
SEQC_OPERATOR(SeqCDec,     0xb05e60);
//! \brief Bitwise AND operator (`lhs & rhs`).
SEQC_OPERATOR(SeqCAndExpr, 0xb05eb8);
//! \brief Bitwise OR operator (`lhs | rhs`).
SEQC_OPERATOR(SeqCOrExpr,  0xb05f10);
//! \brief Bitwise XOR operator (`lhs ^ rhs`).
SEQC_OPERATOR(SeqCXorExpr, 0xb05f68);
//! \brief Logical AND operator (`lhs && rhs`).
SEQC_OPERATOR(SeqCLogAnd,  0xb05fc0);
//! \brief Logical OR operator (`lhs || rhs`).
SEQC_OPERATOR(SeqCLogOr,   0xb06018);
//! \brief Plain assignment operator (`lhs = rhs`).
SEQC_OPERATOR(SeqCAssign,  0xb06070);
//! \brief No-op operator slot — placeholder used by the parser where
//! an operator node is grammatically required but has no semantic
//! effect.
SEQC_OPERATOR(SeqCNoOp,    0xb060c8);

#undef SEQC_OPERATOR

// ============================================================================
// Two-child direct-AstNode subclasses (40 bytes, 0x28)
//
// Layout: SeqCAstNode (24B) + 2 unique_ptrs at +0x18, +0x20
// ============================================================================

#define SEQC_BINARY(Name, FirstAccessor, SecondAccessor, F1, F2, VtableAddr) \
    /*! \brief Two-child direct-`SeqCAstNode` subclass — wraps a pair    \
     *  of owned subexpressions / subtrees as `F1##_` (+0x18) and        \
     *  `F2##_` (+0x20).                                                  \
     *                                                                     \
     *  Used by `SeqCIfCondition`, `SeqCWhileLoop`, `SeqCDoWhile` and    \
     *  `SeqCRepeat`; their roles differ only in the role names of the  \
     *  two children, the accessor names, the printed label, and the    \
     *  per-class lowering rule.  Total size is 0x28 bytes.              \
     */                                                                     \
    class Name : public SeqCAstNode {                                       \
    public:                                                                 \
        /*! \brief Construct the binary node, taking ownership of both   \
         *  subtrees.                                                      \
         *  \param vc     Value category of the node.                      \
         *  \param lineNr 1-based source line number.                      \
         *  \param dir    Parameter-direction tag.                         \
         *  \param vt     Variable-type tag assigned to `varType_`.       \
         *  \param F1     First child subtree; moved into `F1##_`.        \
         *  \param F2     Second child subtree; moved into `F2##_`.       \
         */                                                                 \
        Name(EValueCategory vc, int lineNr, EDirection dir,              \
             VarType vt,                                                    \
             std::unique_ptr<SeqCAstNode> F1,                               \
             std::unique_ptr<SeqCAstNode> F2);                              \
        /*! \brief Deep-copy constructor — clones both subtrees via      \
         *  `doClone()`.                                                   \
         *  \param o Source node copied from.                              \
         */                                                                 \
        Name(Name const& o);                                                \
        /*! \brief Copy-and-swap assignment.                               \
         *  \param o Source node, copied on entry and swapped on exit.    \
         *  \return Reference to `*this`.                                  \
         */                                                                 \
        Name& operator=(Name o);                                            \
        /*! \brief Default destructor; releases both owned children. */   \
        ~Name() override;                                                   \
        /*! \brief Writes the class label to `std::cout`. */              \
        void print() const override;                                        \
        /*! \brief Polymorphic deep clone — returns a fresh instance     \
         *  of the same concrete kind with cloned children.                \
         *  \return Owning pointer to the cloned node.                     \
         */                                                                 \
        std::unique_ptr<SeqCAstNode> doClone() const override;                \
        /*! \brief Returns a two-entry vector with both child pointers   \
         *  in declaration order (either may be null).                    \
         *  \return Vector of borrowed child pointers (size 2).            \
         */                                                                 \
        std::vector<const SeqCAstNode*> children() const override;          \
        /*! \brief Lowers this two-child node, evaluating both subtrees  \
         *  and applying the kind-specific lowering rule.                  \
         *  \param res   Shared compile resources.                         \
         *  \param ctx   Lowering context.                                 \
         *  \param state Per-call lowering state.                         \
         *  \return The `EvalResults` produced by lowering this node.      \
         */                                                                 \
        std::shared_ptr<EvalResults> evaluate(                              \
            std::shared_ptr<Resources> res,                                 \
            FrontendLoweringContext& ctx,                                    \
            FrontendLoweringState& state) const override;                   \
        /*! \brief Borrowed pointer to the first owned subtree.           \
         *  \return Non-owning pointer to `F1##_` (may be null).           \
         */                                                                 \
        const SeqCAstNode* FirstAccessor()  const { return F1##_.get(); }   \
        /*! \brief Borrowed pointer to the second owned subtree.          \
         *  \return Non-owning pointer to `F2##_` (may be null).           \
         */                                                                 \
        const SeqCAstNode* SecondAccessor() const { return F2##_.get(); }   \
        /*! \brief ADL-friendly swap — exchanges the base subobject and \
         *  both child pointers.                                           \
         *  \param a First node to swap.                                   \
         *  \param b Second node to swap.                                  \
         */                                                                 \
        friend void swap(Name& a, Name& b);                                \
    private:                                                                \
        /*! \brief Owned first subtree (`F1##_`); accessed through the   \
         *  first named getter.                                            \
         */                                                                 \
        std::unique_ptr<SeqCAstNode> F1##_;   /* +0x18 */                   \
        /*! \brief Owned second subtree (`F2##_`); accessed through the  \
         *  second named getter.                                           \
         */                                                                 \
        std::unique_ptr<SeqCAstNode> F2##_;   /* +0x20 */                   \
    };                                                                      \
    static_assert(sizeof(Name) == 0x28, #Name " must be 0x28 bytes")

// Forward declarations for typed children
class SeqCVariable;

// SeqCFunctionCall — broken out of SEQC_BINARY because funName_ is unique_ptr<SeqCVariable>.
// vtable @0xb05140.  Layout: SeqCAstNode(24B) + 2 unique_ptrs at +0x18, +0x20 = 0x28 bytes.
//! \brief Function-call expression node.
//!
//! Pairs the callee identifier (a `SeqCVariable` holding the function
//! name) with the argument list subtree.  Lowering looks the name up in
//! the user-defined function table and then in the `CustomFunctions`
//! built-in registry, evaluates the arguments, and dispatches the call.
class SeqCFunctionCall : public SeqCAstNode {
public:
    //! \brief Constructs the call node, taking ownership of the callee
    //! identifier and the argument-list subtree.
    //! \param vc      Value category of the call expression.
    //! \param lineNr  1-based source line number.
    //! \param dir     Parameter-direction tag.
    //! \param vt      Variable-type tag (the call's return-type tag).
    //! \param funName Owned callee identifier (function name).
    //! \param args    Owned argument-list subtree.
    SeqCFunctionCall(EValueCategory vc, int lineNr, EDirection dir,
                     VarType vt,
                     std::unique_ptr<SeqCVariable> funName,
                     std::unique_ptr<SeqCAstNode> args);
    //! \brief Deep-copy constructor — clones both the callee identifier
    //! and the argument-list subtree.
    //! \param o Source node copied from.
    SeqCFunctionCall(SeqCFunctionCall const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source call, copied on entry and swapped on exit.
    //! \return Reference to `*this`.
    SeqCFunctionCall& operator=(SeqCFunctionCall o);
    //! \brief Default destructor; releases both owned children.
    ~SeqCFunctionCall() override;
    //! \brief Writes the label `"FunctionCall"` to `std::cout`.
    void print() const override;
    //! \brief Polymorphic deep clone — returns a fresh
    //! `SeqCFunctionCall` with cloned callee and arguments.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;
    //! \brief Returns `{ funName(), arguments() }` in that order.
    //! \return Two-entry vector of borrowed child pointers.
    std::vector<const SeqCAstNode*> children() const override;
    //! \brief Lowers the call: resolves `funName()` against the
    //! user-defined function table and the built-in registry, evaluates
    //! the arguments, and dispatches to the resolved target.
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by lowering the call.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;
    //! \brief Borrowed pointer to the callee identifier.
    //! \return Non-owning pointer to the held `SeqCVariable` (may be
    //! null).
    const SeqCVariable* funName()    const { return funName_.get(); }
    //! \brief Borrowed pointer to the argument-list subtree.
    //! \return Non-owning pointer to the held argument-list node (may
    //! be null).
    const SeqCAstNode*  arguments()  const { return args_.get(); }
    //! \brief ADL-friendly swap — exchanges the base subobject, the
    //! callee pointer, and the argument-list pointer.
    //! \param a First call node.
    //! \param b Second call node.
    friend void swap(SeqCFunctionCall& a, SeqCFunctionCall& b);
private:
    //! \brief Owned callee identifier subtree; accessed via `funName()`.
    std::unique_ptr<SeqCVariable>  funName_;   /* +0x18 */
    //! \brief Owned argument-list subtree; accessed via `arguments()`.
    std::unique_ptr<SeqCAstNode>   args_;  /* +0x20 */
};
static_assert(sizeof(SeqCFunctionCall) == 0x28, "SeqCFunctionCall must be 0x28 bytes");

// SeqCArray — broken out of SEQC_BINARY because array_ is unique_ptr<SeqCVariable>.
// vtable @0xb051e8.  Layout identical (0x28 bytes).
//! \brief Array-element access expression `array[index]`.
//!
//! Pairs the array identifier (a `SeqCVariable`) with the index
//! subexpression.  Lowering resolves the array binding through the
//! `Resources` symbol table and emits the offset arithmetic to read or
//! write the addressed element.
class SeqCArray : public SeqCAstNode {
public:
    //! \brief Constructs the array-access node, taking ownership of
    //! both children.
    //! \param vc     Value category of the access expression.
    //! \param lineNr 1-based source line number.
    //! \param dir    Parameter-direction tag.
    //! \param vt     Variable-type tag.
    //! \param array  Owned array-identifier node.
    //! \param index  Owned index-expression subtree.
    SeqCArray(EValueCategory vc, int lineNr, EDirection dir,
              VarType vt,
              std::unique_ptr<SeqCVariable> array,
              std::unique_ptr<SeqCAstNode> index);
    //! \brief Deep-copy constructor — clones both children.
    //! \param o Source node copied from.
    SeqCArray(SeqCArray const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source array access, copied on entry and swapped on
    //! exit.
    //! \return Reference to `*this`.
    SeqCArray& operator=(SeqCArray o);
    //! \brief Default destructor; releases both owned children.
    ~SeqCArray() override;
    //! \brief Writes the label `"Array"` to `std::cout`.
    void print() const override;
    //! \brief Polymorphic deep clone — returns a fresh `SeqCArray`
    //! with cloned array identifier and index subtree.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;
    //! \brief Returns `{ array(), index() }` in that order.
    //! \return Two-entry vector of borrowed child pointers.
    std::vector<const SeqCAstNode*> children() const override;
    //! \brief Lowers the array-element access: resolves `array()`
    //! through the `Resources` symbol table and emits the offset
    //! arithmetic to read or write the addressed element.
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by lowering the access.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;
    //! \brief Borrowed pointer to the index-expression subtree.
    //! \return Non-owning pointer to the held index subtree (may be
    //! null).
    const SeqCAstNode*  index() const { return index_.get(); }
    //! \brief Borrowed pointer to the array-identifier node.
    //! \return Non-owning pointer to the held `SeqCVariable` (may be
    //! null).
    const SeqCVariable* array() const { return array_.get(); }
    //! \brief ADL-friendly swap — exchanges the base subobject, the
    //! array-identifier pointer, and the index pointer.
    //! \param a First array-access node.
    //! \param b Second array-access node.
    friend void swap(SeqCArray& a, SeqCArray& b);
private:
    //! \brief Owned array-identifier subtree; accessed via `array()`.
    std::unique_ptr<SeqCVariable>  array_;   /* +0x18 */
    //! \brief Owned index-expression subtree; accessed via `index()`.
    std::unique_ptr<SeqCAstNode>   index_;  /* +0x20 */
};
static_assert(sizeof(SeqCArray) == 0x28, "SeqCArray must be 0x28 bytes");

//! \brief Single-arm `if` condition node — wraps the condition
//! expression and the then-branch body when no `else` is present.
//! Lowering folds constant-true conditions to the body and constant-false
//! conditions to a no-op.
SEQC_BINARY(SeqCIfCondition,  cond,     ifBody,  cond, ifBody, 0xb053e0);

// SeqCCaseEntry — broken out of SEQC_BINARY for extra methods (validLabel, hasLabel).
// vtable @0xb05518.  Layout identical (0x28 bytes).
//! \brief Single `case`/`default` entry inside a switch body.
//!
//! Holds the case label expression (null for the `default` entry — see
//! `validLabel()` / `hasLabel()`) and the body to execute when matched.
//! Lowering rejects the node unless `state.inSwitch_` is set, ensuring
//! case entries appear only inside an enclosing `SeqCSwitchCase`.
class SeqCCaseEntry : public SeqCAstNode {
public:
    //! \brief Constructs the case-entry node, taking ownership of the
    //! optional label expression and the body subtree.
    //! \param vc     Value category of the entry.
    //! \param lineNr 1-based source line number.
    //! \param dir    Parameter-direction tag.
    //! \param vt     Variable-type tag.
    //! \param label  Owned label expression; null for the `default`
    //!               entry.
    //! \param body   Owned body subtree to execute when the entry
    //!               matches.
    SeqCCaseEntry(EValueCategory vc, int lineNr, EDirection dir,
                  VarType vt,
                  std::unique_ptr<SeqCAstNode> label,
                  std::unique_ptr<SeqCAstNode> body);
    //! \brief Deep-copy constructor — clones both children.
    //! \param o Source node copied from.
    SeqCCaseEntry(SeqCCaseEntry const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source entry, copied on entry and swapped on exit.
    //! \return Reference to `*this`.
    SeqCCaseEntry& operator=(SeqCCaseEntry o);
    //! \brief Default destructor; releases both owned children.
    ~SeqCCaseEntry() override;
    //! \brief Writes the label `"CaseEntry"` to `std::cout`.
    void print() const override;
    //! \brief Polymorphic deep clone — returns a fresh
    //! `SeqCCaseEntry` with cloned label and body.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;
    //! \brief Returns `{ label(), body() }` in that order.
    //! \return Two-entry vector of borrowed child pointers.
    std::vector<const SeqCAstNode*> children() const override;
    //! \brief Lowers the case entry; rejects the node unless
    //! `state.inSwitch_` is set so case entries are only accepted
    //! inside an enclosing `SeqCSwitchCase`.
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by lowering this entry.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;
    //! \brief Borrowed pointer to the label expression (null for the
    //! `default` entry).
    //! \return Non-owning pointer to the held label subtree.
    const SeqCAstNode* label()      const { return label_.get(); }
    //! \brief Returns the case body subtree.
    //! \return Non-owning pointer to the body subtree.
    const SeqCAstNode* body()       const;  // out-of-line for symbol emission
    //! \brief Returns true when this entry has an explicit label
    //! (i.e., it is a `case <expr>:` entry rather than `default:`).
    //! \return `true` when an explicit label is present.
    bool               validLabel() const;  // label_ != nullptr
    //! \brief Alias for `validLabel()` — true when an explicit
    //! label is present.
    //! \return Same as `validLabel()`.
    bool               hasLabel()   const;  // label_ != nullptr
    //! \brief ADL-friendly swap — exchanges the base subobject, the
    //! label pointer, and the body pointer.
    //! \param a First case-entry node.
    //! \param b Second case-entry node.
    friend void swap(SeqCCaseEntry& a, SeqCCaseEntry& b);
private:
    //! \brief Owned label expression; null for the `default` entry.
    //! Accessed via `label()`.
    std::unique_ptr<SeqCAstNode> label_;   /* +0x18 */
    //! \brief Owned body subtree executed when the entry matches;
    //! accessed via `body()`.
    std::unique_ptr<SeqCAstNode> body_;  /* +0x20 */
};
static_assert(sizeof(SeqCCaseEntry) == 0x28, "SeqCCaseEntry must be 0x28 bytes");
// Forward declaration for SeqCSwitchCase::cases() return type
class SeqCStmtList;
// SeqCSwitchCase — broken out of SEQC_BINARY for extra methods (hasCases, evalCases).
// vtable @0xb05480.  Layout identical to other SEQC_BINARY types (0x28 bytes).
//! \brief `switch` statement node.
//!
//! Pairs the switch condition expression with a body that is normally a
//! `SeqCStmtList` of `SeqCCaseEntry` children; the `cases()` /
//! `singleCase()` / `hasCases()` helpers normalise access to the cases
//! regardless of whether the body is a list or a single entry.  Sets
//! `state.inSwitch_` around case-entry evaluation; `evalCases()`
//! evaluates each case body in turn against the lowered condition value.
class SeqCSwitchCase : public SeqCAstNode {
public:
    //! \brief Constructs the switch node, taking ownership of the
    //! condition expression and the body subtree.
    //! \param vc     Value category of the switch expression.
    //! \param lineNr 1-based source line number.
    //! \param dir    Parameter-direction tag.
    //! \param vt     Variable-type tag.
    //! \param cond   Owned condition expression.
    //! \param body   Owned body — usually a `SeqCStmtList` of
    //!               `SeqCCaseEntry` children but a single entry is
    //!               accepted.
    SeqCSwitchCase(EValueCategory vc, int lineNr, EDirection dir,
                   VarType vt,
                   std::unique_ptr<SeqCAstNode> cond,
                   std::unique_ptr<SeqCAstNode> body);
    //! \brief Deep-copy constructor — clones both children.
    //! \param o Source node copied from.
    SeqCSwitchCase(SeqCSwitchCase const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source switch, copied on entry and swapped on exit.
    //! \return Reference to `*this`.
    SeqCSwitchCase& operator=(SeqCSwitchCase o);
    //! \brief Default destructor; releases both owned children.
    ~SeqCSwitchCase() override;
    //! \brief Writes the label `"SwitchCase"` to `std::cout`.
    void print() const override;
    //! \brief Polymorphic deep clone — returns a fresh
    //! `SeqCSwitchCase` with cloned condition and body.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;
    //! \brief Returns `{ cond(), body() }` in that order.
    //! \return Two-entry vector of borrowed child pointers.
    std::vector<const SeqCAstNode*> children() const override;
    //! \brief Lowers the switch: evaluates `cond()`, sets
    //! `state.inSwitch_` for the body, then calls `evalCases()` to run
    //! each case body against the condition value.
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by lowering this switch.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;

    //! \brief Borrowed pointer to the condition expression.
    //! \return Non-owning pointer to the held condition (may be null).
    const SeqCAstNode* cond() const { return cond_.get(); }
    //! \brief Borrowed pointer to the body subtree (list of case
    //! entries or a single entry).
    //! \return Non-owning pointer to the held body (may be null).
    const SeqCAstNode* body() const { return body_.get(); }

    // Switch-specific helpers
    //! \brief Returns true when the switch body holds at least one
    //! `SeqCCaseEntry` child (either as a single entry or as the
    //! non-empty `cases()` list).
    //! \return `true` when at least one case entry is present.
    bool hasCases() const;                                           // @0x202760
    //! \brief Returns true when the switch body is a single
    //! `SeqCCaseEntry` rather than a `SeqCStmtList` of entries.
    //! \return `true` when the body is a single case entry.
    bool isSingleCase() const;                                       // @0x202730
    //! \brief Returns the body cast to `SeqCCaseEntry*` when
    //! `isSingleCase()` is true; undefined otherwise.
    //! \return Non-owning pointer to the lone case entry.
    const SeqCCaseEntry* singleCase() const;                         // @0x202770
    //! \brief Returns the body cast to `SeqCStmtList*` when the
    //! body is a list of case entries; null when the body is a
    //! single entry.
    //! \return Non-owning pointer to the case-entry list, or null
    //! when the body is a single entry.
    const SeqCStmtList* cases() const;                               // @0x202700
    //! \brief Evaluates each case body in turn against the
    //! lowered condition value, returning one `EvalResults` per
    //! case in source order; `state.inSwitch_` must be set by the
    //! caller around the call so nested `break` statements
    //! validate.
    //! \param subRes     Symbol-table sub-scope used for the case
    //!                   bodies.
    //! \param condResult Lowered condition value supplied to each
    //!                   case body.
    //! \param ctx        Compiler subsystems handle.
    //! \param state      Mutable per-evaluation accumulator (must
    //!                   already have `inSwitch_` set).
    //! \return One `EvalResults` per case, in source order.
    std::vector<std::shared_ptr<EvalResults>> evalCases(
        std::shared_ptr<Resources> subRes,
        std::shared_ptr<EvalResults> condResult,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const;                         // @0x216980

    //! \brief ADL-friendly swap — exchanges the base subobject, the
    //! condition pointer, and the body pointer.
    //! \param a First switch node.
    //! \param b Second switch node.
    friend void swap(SeqCSwitchCase& a, SeqCSwitchCase& b);

private:
    //! \brief Owned switch-condition expression; accessed via `cond()`.
    std::unique_ptr<SeqCAstNode> cond_;   /* +0x18 */
    //! \brief Owned switch body — usually a `SeqCStmtList` of
    //! `SeqCCaseEntry` children but may be a single entry; accessed
    //! via `body()`.
    std::unique_ptr<SeqCAstNode> body_;  /* +0x20 */
};
static_assert(sizeof(SeqCSwitchCase) == 0x28, "SeqCSwitchCase must be 0x28 bytes");
//! \brief `while (cond) body` loop node — evaluates `body` repeatedly
//! while `cond` is non-zero; `state.inLoop_` is toggled around the body
//! to validate nested `break`/`continue`.
SEQC_BINARY(SeqCWhileLoop,    cond,     body,  cond, body, 0xb055d0);
//! \brief `do body while (cond);` loop node — like `SeqCWhileLoop`
//! but with body-first semantics; `body` is always evaluated at least
//! once before `cond` is tested.
SEQC_BINARY(SeqCDoWhile,      body,     cond,  body, cond, 0xb05620);
//! \brief `repeat (cond) body` loop node — evaluates `body` exactly
//! `cond` times; the body is unrolled when `cond` is a compile-time
//! constant (subject to the loop-unroll limit on the lowering context).
//!
//! \binarynote The first child is exposed as `cond()` (not `count()`)
//! to match the symbol the original binary publishes
//! (`SeqCRepeat::cond() const` at 0x203b80); semantically it is the
//! repeat-count expression, but the binary's accessor name is `cond`.
SEQC_BINARY(SeqCRepeat,       cond,     body,  cond, body, 0xb05670);

#undef SEQC_BINARY

// ============================================================================
// Three-child direct-AstNode subclasses (48 bytes, 0x30)
// ============================================================================

//! \brief `if`/`else` statement node.
//!
//! Three-child node holding the condition, the then-branch body, and the
//! else-branch body.  Lowering produces a `Branch` IR node with both
//! arms wired up; constant-folded conditions short-circuit to a single
//! arm.
class SeqCIfElse : public SeqCAstNode {
public:
    //! \brief Constructs the if/else node, taking ownership of the
    //! condition and both branch bodies.
    //! \param vc       Value category of the node.
    //! \param lineNr   1-based source line number.
    //! \param dir      Parameter-direction tag.
    //! \param vt       Variable-type tag.
    //! \param cond     Owned condition expression.
    //! \param ifBody   Owned then-branch subtree.
    //! \param elseBody Owned else-branch subtree.
    SeqCIfElse(EValueCategory vc, int lineNr, EDirection dir,
               VarType vt,
               std::unique_ptr<SeqCAstNode> cond,
               std::unique_ptr<SeqCAstNode> ifBody,
               std::unique_ptr<SeqCAstNode> elseBody);   // 0x202150
    //! \brief Deep-copy constructor — clones all three children.
    //! \param o Source node copied from.
    SeqCIfElse(SeqCIfElse const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source if/else, copied on entry and swapped on exit.
    //! \return Reference to `*this`.
    SeqCIfElse& operator=(SeqCIfElse o);
    //! \brief Default destructor; releases all three owned children.
    ~SeqCIfElse() override;
    //! \brief Writes the label `"IfElse"` to `std::cout`.
    void print() const override;                          // 0x201df0
    //! \brief Polymorphic deep clone — returns a fresh `SeqCIfElse`
    //! with cloned condition and both branch bodies.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;  // 0x2021a0
    //! \brief Returns `{ cond(), ifBody(), elseBody() }` in that
    //! order.
    //! \return Three-entry vector of borrowed child pointers.
    std::vector<const SeqCAstNode*> children() const override;  // 0x2022c0
    //! \brief Lowers the if/else: evaluates `cond()`, lowers each
    //! branch under the appropriate constant-folded path, and emits a
    //! `Branch` IR node when both arms remain live.
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by lowering this if/else.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x214d50

    //! \brief Borrowed pointer to the condition expression.
    //! \return Non-owning pointer to the held condition (may be null).
    const SeqCAstNode* cond()     const { return cond_.get(); }      // 0x202320
    //! \brief Borrowed pointer to the then-branch subtree.
    //! \return Non-owning pointer to the held then-branch (may be
    //! null).
    const SeqCAstNode* ifBody()   const { return ifBody_.get(); }    // 0x202330
    //! \brief Borrowed pointer to the else-branch subtree.
    //! \return Non-owning pointer to the held else-branch (may be
    //! null).
    const SeqCAstNode* elseBody() const { return elseBody_.get(); }  // 0x202340

    //! \brief ADL-friendly swap — exchanges the base subobject and
    //! all three child pointers.
    //! \param a First if/else node.
    //! \param b Second if/else node.
    friend void swap(SeqCIfElse& a, SeqCIfElse& b);

private:
    //! \brief Owned condition expression; accessed via `cond()`.
    std::unique_ptr<SeqCAstNode> cond_;       // +0x18
    //! \brief Owned then-branch subtree; accessed via `ifBody()`.
    std::unique_ptr<SeqCAstNode> ifBody_;     // +0x20
    //! \brief Owned else-branch subtree; accessed via `elseBody()`.
    std::unique_ptr<SeqCAstNode> elseBody_;   // +0x28
};
static_assert(sizeof(SeqCIfElse) == 0x30, "SeqCIfElse must be 0x30 bytes");

//! \brief Conditional expression node `cond ? a : b`.
//!
//! Three-child expression form of an if/else: evaluates the condition
//! and yields the value of the then-branch or the else-branch as the
//! expression result, instead of producing control-flow IR.
class SeqCCondExpr : public SeqCAstNode {
public:
    //! \brief Constructs the conditional expression, taking ownership
    //! of the condition and both result subexpressions.
    //! \param vc       Value category of the expression.
    //! \param lineNr   1-based source line number.
    //! \param dir      Parameter-direction tag.
    //! \param vt       Variable-type tag.
    //! \param cond     Owned condition expression.
    //! \param ifBody   Owned then-result subexpression.
    //! \param elseBody Owned else-result subexpression.
    SeqCCondExpr(EValueCategory vc, int lineNr, EDirection dir,
                 VarType vt,
                 std::unique_ptr<SeqCAstNode> cond,
                 std::unique_ptr<SeqCAstNode> ifBody,
                 std::unique_ptr<SeqCAstNode> elseBody);
    //! \brief Deep-copy constructor — clones all three children.
    //! \param o Source node copied from.
    SeqCCondExpr(SeqCCondExpr const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source expression, copied on entry and swapped on
    //! exit.
    //! \return Reference to `*this`.
    SeqCCondExpr& operator=(SeqCCondExpr o);
    //! \brief Default destructor; releases all three owned children.
    ~SeqCCondExpr() override;
    //! \brief Writes the label `"CondExpr"` to `std::cout`.
    void print() const override;
    //! \brief Polymorphic deep clone — returns a fresh `SeqCCondExpr`
    //! with cloned condition and both result subexpressions.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;
    //! \brief Returns `{ cond(), ifBody(), elseBody() }` in that
    //! order.
    //! \return Three-entry vector of borrowed child pointers.
    std::vector<const SeqCAstNode*> children() const override;
    //! \brief Lowers the conditional expression: evaluates `cond()`
    //! and yields the value of the then-branch or the else-branch
    //! subexpression as the expression result (instead of producing
    //! control-flow IR).
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by lowering this conditional
    //! expression.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x223d90

    //! \brief Borrowed pointer to the condition expression.
    //! \return Non-owning pointer to the held condition (may be null).
    const SeqCAstNode* cond()     const { return cond_.get(); }
    //! \brief Borrowed pointer to the then-branch subexpression.
    //! \return Non-owning pointer to the held then-result (may be
    //! null).
    const SeqCAstNode* ifBody()   const { return ifBody_.get(); }    // @0x2040e0
    //! \brief Borrowed pointer to the else-branch subexpression.
    //! \return Non-owning pointer to the held else-result (may be
    //! null).
    const SeqCAstNode* elseBody() const { return elseBody_.get(); }  // @0x2040f0

    //! \brief ADL-friendly swap — exchanges the base subobject and
    //! all three child pointers.
    //! \param a First conditional expression.
    //! \param b Second conditional expression.
    friend void swap(SeqCCondExpr& a, SeqCCondExpr& b);

private:
    //! \brief Owned condition expression; accessed via `cond()`.
    std::unique_ptr<SeqCAstNode> cond_;          // +0x18
    //! \brief Owned then-branch subexpression; accessed via `ifBody()`.
    std::unique_ptr<SeqCAstNode> ifBody_;        // +0x20
    //! \brief Owned else-branch subexpression; accessed via `elseBody()`.
    std::unique_ptr<SeqCAstNode> elseBody_;      // +0x28
};
static_assert(sizeof(SeqCCondExpr) == 0x30, "SeqCCondExpr must be 0x30 bytes");

// ============================================================================
// Four-child direct-AstNode subclasses (56 bytes, 0x38)
// ============================================================================

//! \brief User-defined SeqC function definition.
//!
//! Bundles the call-site signature node (a `SeqCFunctionCall` carrying
//! the function name), the parameter list, the function body, and the
//! return-type declarator.  Lowering registers the function in the
//! `Resources` symbol table and stashes the body for later inlining
//! when the function is called.
class SeqCFunction : public SeqCAstNode {
public:
    //! \brief Constructs the function-definition node, taking
    //! ownership of all four child subtrees.
    //! \param vc      Value category of the definition.
    //! \param lineNr  1-based source line number.
    //! \param dir     Parameter-direction tag.
    //! \param vt      Variable-type tag.
    //! \param call    Owned signature node — a `SeqCFunctionCall`
    //!                carrying the function name.
    //! \param params  Owned parameter-list subtree.
    //! \param body    Owned function-body subtree.
    //! \param retType Owned return-type declarator.
    SeqCFunction(EValueCategory vc, int lineNr, EDirection dir,
                 VarType vt,
                 std::unique_ptr<SeqCFunctionCall> call,
                 std::unique_ptr<SeqCAstNode> params,
                 std::unique_ptr<SeqCAstNode> body,
                 std::unique_ptr<SeqCVariableType> retType);
    //! \brief Deep-copy constructor — clones all four children.
    //! \param o Source node copied from.
    SeqCFunction(SeqCFunction const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source function, copied on entry and swapped on exit.
    //! \return Reference to `*this`.
    SeqCFunction& operator=(SeqCFunction o);
    //! \brief Default destructor; releases all four owned children.
    ~SeqCFunction() override;
    //! \brief Writes the label `"Function"` to `std::cout`.
    void print() const override;
    //! \brief Polymorphic deep clone — returns a fresh `SeqCFunction`
    //! with cloned signature, parameters, body, and return-type
    //! declarator.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;
    //! \brief Returns `{ call(), params(), body(), retType() }` in
    //! that order.
    //! \return Four-entry vector of borrowed child pointers.
    std::vector<const SeqCAstNode*> children() const override;
    //! \brief Lowers the function definition: registers the function
    //! in the `Resources` symbol table and stashes the body for later
    //! inlining at every call site.
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by lowering this definition.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x20b200

    //! \brief Borrowed pointer to the signature node (a
    //! `SeqCFunctionCall` carrying the function name).
    //! \return Non-owning pointer to the held signature (may be null).
    const SeqCFunctionCall* call()  const { return call_.get(); }         // @0x1fec10, +0x18
    //! \brief Borrowed pointer to the parameter-list subtree.
    //! \return Non-owning pointer to the held parameter list (may be
    //! null).
    const SeqCAstNode* params()     const { return params_.get(); }       // @0x1fec20, +0x20
    //! \brief Borrowed pointer to the function-body subtree.
    //! \return Non-owning pointer to the held body (may be null).
    const SeqCAstNode* body()       const { return body_.get(); }         // @0x1fec30, +0x28
    //! \brief Borrowed pointer to the return-type declarator.
    //! \return Non-owning pointer to the held return-type node (may
    //! be null).
    const SeqCVariableType* retType() const { return retType_.get(); }    // @0x1fec40, +0x30

    //! \brief ADL-friendly swap — exchanges the base subobject and
    //! all four child pointers.
    //! \param a First function node.
    //! \param b Second function node.
    friend void swap(SeqCFunction& a, SeqCFunction& b);

private:
    //! \brief Owned signature node — a `SeqCFunctionCall` carrying
    //! the function name; accessed via `call()`.
    std::unique_ptr<SeqCFunctionCall> call_;   // +0x18
    //! \brief Owned parameter-list subtree; accessed via `params()`.
    std::unique_ptr<SeqCAstNode> params_;      // +0x20
    //! \brief Owned function-body subtree; accessed via `body()`.
    std::unique_ptr<SeqCAstNode> body_;        // +0x28
    //! \brief Owned return-type declarator; accessed via `retType()`.
    std::unique_ptr<SeqCVariableType> retType_; // +0x30
};
static_assert(sizeof(SeqCFunction) == 0x38, "SeqCFunction must be 0x38 bytes");

//! \brief C-style `for` loop node.
//!
//! Four-child node holding the initialiser, condition, increment, and
//! body subexpressions.  Lowering toggles `state.inLoop_` around body
//! evaluation so nested `break`/`continue` statements can validate
//! their context, and unrolls the loop when the condition is constant
//! (subject to `FrontendLoweringContext::loopUnrollLimit`).
class SeqCForLoop : public SeqCAstNode {
public:
    //! \brief Constructs the for-loop node, taking ownership of all
    //! four child subtrees.
    //! \param vc     Value category of the loop.
    //! \param lineNr 1-based source line number.
    //! \param dir    Parameter-direction tag.
    //! \param vt     Variable-type tag.
    //! \param init   Owned initialiser subtree.
    //! \param cond   Owned condition expression.
    //! \param incr   Owned increment subtree.
    //! \param body   Owned loop-body subtree.
    SeqCForLoop(EValueCategory vc, int lineNr, EDirection dir,
                VarType vt,
                std::unique_ptr<SeqCAstNode> init,
                std::unique_ptr<SeqCAstNode> cond,
                std::unique_ptr<SeqCAstNode> incr,
                std::unique_ptr<SeqCAstNode> body);     // 0x202f00
    //! \brief Deep-copy constructor — clones all four children.
    //! \param o Source node copied from.
    SeqCForLoop(SeqCForLoop const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source loop, copied on entry and swapped on exit.
    //! \return Reference to `*this`.
    SeqCForLoop& operator=(SeqCForLoop o);
    //! \brief Default destructor; releases all four owned children.
    ~SeqCForLoop() override;
    //! \brief Writes the label `"ForLoop"` to `std::cout`.
    void print() const override;                         // 0x202bc0
    //! \brief Polymorphic deep clone — returns a fresh `SeqCForLoop`
    //! with cloned initialiser, condition, increment, and body.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override; // 0x202f70
    //! \brief Returns `{ init(), cond(), incr(), body() }` in that
    //! order.
    //! \return Four-entry vector of borrowed child pointers.
    std::vector<const SeqCAstNode*> children() const override;  // 0x202fd0
    //! \brief Lowers the loop: evaluates `init()` once, then iterates
    //! `cond()` / `body()` / `incr()` while toggling `state.inLoop_`
    //! around body evaluation so nested `break` / `continue`
    //! statements can validate their context.  Unrolls the loop when
    //! `cond()` is a compile-time constant (subject to
    //! `FrontendLoweringContext::loopUnrollLimit`).
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by lowering this loop.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x21b680

    //! \brief Borrowed pointer to the initialiser subtree.
    //! \return Non-owning pointer to the held initialiser (may be
    //! null).
    const SeqCAstNode* init() const { return init_.get(); }   // 0x203020
    //! \brief Borrowed pointer to the condition expression.
    //! \return Non-owning pointer to the held condition (may be null).
    const SeqCAstNode* cond() const { return cond_.get(); }   // 0x203030
    //! \brief Borrowed pointer to the increment subtree.
    //! \return Non-owning pointer to the held increment (may be null).
    const SeqCAstNode* incr() const { return incr_.get(); }   // 0x203040
    //! \brief Borrowed pointer to the loop-body subtree.
    //! \return Non-owning pointer to the held body (may be null).
    const SeqCAstNode* body() const { return body_.get(); }   // 0x203050

    //! \brief ADL-friendly swap — exchanges the base subobject and
    //! all four child pointers.
    //! \param a First loop node.
    //! \param b Second loop node.
    friend void swap(SeqCForLoop& a, SeqCForLoop& b);

private:
    //! \brief Owned initialiser subtree; accessed via `init()`.
    std::unique_ptr<SeqCAstNode> init_;   // +0x18
    //! \brief Owned loop-condition expression; accessed via `cond()`.
    std::unique_ptr<SeqCAstNode> cond_;   // +0x20
    //! \brief Owned increment subtree; accessed via `incr()`.
    std::unique_ptr<SeqCAstNode> incr_;   // +0x28
    //! \brief Owned loop-body subtree; accessed via `body()`.
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
    /*! \brief AST list node holding an ordered sequence of child nodes.    \
     *                                                                       \
     *  One of the three macro-generated list nodes (`SeqCArgList`,         \
     *  `SeqCDeclList`, `SeqCStmtList`).  All three share an identical      \
     *  layout — a single `std::vector<std::unique_ptr<SeqCAstNode>>` of    \
     *  owned children at offset +0x18 — and an identical implementation    \
     *  modulo the printed label and the named accessor.  Total size is    \
     *  0x30 bytes (statically asserted below).                             \
     */                                                                      \
    class Name : public SeqCAstNode {                                       \
    public:                                                                 \
        /*! \brief Construct an empty list with the given AST attributes.   \
         *                                                                   \
         *  Forwards `vc`, `lineNr`, `dir` to `SeqCAstNode` and assigns    \
         *  `vt` to `varType_`.  The element vector is default-constructed \
         *  empty; callers populate it via `append()`.                     \
         *                                                                   \
         *  \param vc     Value category of the list expression.            \
         *  \param lineNr Source line number from the parser.               \
         *  \param dir    Direction tag (input / output / inout).           \
         *  \param vt     Variable type carried in `varType_`.              \
         */                                                                  \
        Name(EValueCategory vc, int lineNr, EDirection dir, VarType vt); \
        /*! \brief Construct a list pre-populated with `elements`.          \
         *                                                                   \
         *  Same as the empty-ctor but moves `elements` into `elements_`.  \
         *  Ownership of every child node transfers to the new list.       \
         *                                                                   \
         *  \param vc       Value category of the list expression.          \
         *  \param lineNr   Source line number from the parser.             \
         *  \param dir      Direction tag (input / output / inout).         \
         *  \param vt       Variable type carried in `varType_`.            \
         *  \param elements Initial child nodes; ownership transfers.       \
         */                                                                  \
        Name(EValueCategory vc, int lineNr, EDirection dir, VarType vt,  \
             std::vector<std::unique_ptr<SeqCAstNode>> elements);           \
        /*! \brief Deep-copy constructor.                                   \
         *                                                                   \
         *  Copies the base AST attributes and clones every child by       \
         *  calling `doClone()` on each non-null element; null elements    \
         *  are preserved as null entries in the new vector.               \
         *                                                                   \
         *  \param o Source list to copy from.                              \
         */                                                                  \
        Name(Name const& o);                                                \
        /*! \brief Copy-and-swap assignment.                                \
         *                                                                   \
         *  `o` is taken by value (invoking the deep-copy ctor) and then   \
         *  swapped into `*this`, providing the strong exception guarantee.\
         *                                                                   \
         *  \param o Source list, copied on entry and swapped on exit.     \
         *  \return Reference to `*this`.                                   \
         */                                                                  \
        Name& operator=(Name o);                                            \
        /*! \brief Default destructor; releases owned child nodes. */       \
        ~Name() override;                                                   \
        /*! \brief Write the class label to `std::cout`.                    \
         *                                                                   \
         *  Emits the literal class tag — `"ArgList"`, `"DeclList"`, or    \
         *  `"StmtList"` — as a raw `std::cout.write()` with no trailing  \
         *  newline.  Used by the AST debug-print walker.                  \
         */                                                                  \
        void print() const override;                                        \
        /*! \brief Polymorphic deep clone.                                  \
         *                                                                   \
         *  Builds a new instance of the same concrete list type with     \
         *  identical AST attributes and a vector of cloned children      \
         *  produced by recursively calling `doClone()` on each element.  \
         *                                                                   \
         *  \return Owning pointer to a freshly-cloned list node.           \
         */                                                                  \
        std::unique_ptr<SeqCAstNode> doClone() const override;                \
        /*! \brief Raw, non-owning pointers to every child element.         \
         *                                                                   \
         *  Walks `elements_` in order and returns each `unique_ptr::get()`,\
         *  preserving null entries as null pointers.                      \
         *                                                                   \
         *  \return Vector of borrowed pointers to the owned children.      \
         */                                                                  \
        std::vector<const SeqCAstNode*> children() const override;          \
        /*! \brief Flatten string-form list elements from every child.      \
         *                                                                   \
         *  Iterates `elements_`, calls `getListElements()` on each child  \
         *  and concatenates the results in order.  Used by callers that  \
         *  need the printable form of nested list contents.               \
         *                                                                   \
         *  \return Concatenated string elements contributed by all       \
         *          children, in declaration order.                         \
         */                                                                  \
        std::vector<std::string> getListElements() const override;          \
        /*! \brief Evaluate every child in order, accumulating results.     \
         *                                                                   \
         *  For `SeqCArgList` and `SeqCDeclList`: evaluates each child,    \
         *  concatenates assemblers, values and names (comma-separated),  \
         *  and reports error 0x12 with the list label on a null child    \
         *  result.                                                         \
         *                                                                   \
         *  For `SeqCStmtList`: additionally skips null elements, catches  \
         *  child-thrown exceptions as line-less error messages, threads  \
         *  `EvalResults::node_` into a single linked chain, breaks on    \
         *  the first child whose `returnEncountered_` is set, extracts   \
         *  its return value via `Resources::setReturnValue()`, and emits \
         *  unreachable-code warning 0x22 if a `SeqCReturnStatement`      \
         *  precedes another statement.                                    \
         *                                                                   \
         *  \param res   Shared compile resources (symbol tables, etc.).   \
         *  \param ctx   Lowering context (messages, asm commands, etc.). \
         *  \param state Per-call lowering state.                          \
         *  \return Accumulated `EvalResults`; an empty result on failure  \
         *          for `SeqCArgList`/`SeqCDeclList`.                      \
         */                                                                  \
        std::shared_ptr<EvalResults> evaluate(                              \
            std::shared_ptr<Resources> res,                                 \
            FrontendLoweringContext& ctx,                                    \
            FrontendLoweringState& state) const override;                   \
                                                                             \
        /*! \brief Append a child element, transferring ownership.          \
         *                                                                   \
         *  Pushes `elem` onto the back of `elements_`; null pointers are \
         *  permitted and preserved.                                       \
         *                                                                   \
         *  \param elem Child node to take ownership of; may be null.       \
         */                                                                  \
        void append(std::unique_ptr<SeqCAstNode> elem);                     \
        /*! \brief Read-only access to the underlying owned-child vector.   \
         *  \return Const reference to the internal element storage.        \
         */                                                                  \
        const std::vector<std::unique_ptr<SeqCAstNode>>& elements() const { \
            return elements_;                                               \
        }                                                                   \
        /*! \brief Raw, non-owning pointers to every child (named alias).   \
         *                                                                   \
         *  Functionally equivalent to `children()` — walks `elements_`    \
         *  and returns each `get()`.  The named alias                     \
         *  (`params()` / `decls()` / `stmts()`) reflects the role of the  \
         *  list in its parent AST node and is the binding callers use.   \
         *                                                                   \
         *  \return Vector of borrowed pointers to the owned children.      \
         */                                                                  \
        std::vector<const SeqCAstNode*> NamedAccessor() const;              \
                                                                             \
        /*! \brief ADL-friendly swap.                                       \
         *                                                                   \
         *  Swaps the `SeqCAstNode` base subobject and `elements_`; used  \
         *  by `operator=` to implement copy-and-swap.                     \
         *                                                                   \
         *  \param a First list to swap.                                    \
         *  \param b Second list to swap.                                   \
         */                                                                  \
        friend void swap(Name& a, Name& b);                                \
    private:                                                                \
        /*! \brief Owned child nodes in declaration order; accessed       \
         *  through `elements()` / `children()` / the per-class named    \
         *  accessor.                                                     \
         */                                                                 \
        std::vector<std::unique_ptr<SeqCAstNode>> elements_;  /* +0x18 */   \
    };                                                                      \
    static_assert(sizeof(Name) == 0x30, #Name " must be 0x30 bytes")

//! \brief Argument-list node — holds the ordered list of argument
//! expressions of a `SeqCFunctionCall`.
SEQC_LIST(SeqCArgList,   params, 0xb05238);
//! \brief Declaration-list node — holds the ordered list of variable
//! declarations introduced by a `var`/parameter declaration block.
SEQC_LIST(SeqCDeclList,  decls,  0xb05288);
//! \brief Statement-list node — holds the ordered sequence of
//! statement subtrees in a block (function body, branch arm, loop
//! body, switch case, etc.).
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
//! \brief Function-definition parameter list.
//!
//! List node holding the parameter declarations of a `SeqCFunction`.
//! Differs from the generic `SEQC_LIST` family by exposing `params()`,
//! which yields raw `SeqCAstNode const*` pointers into the elements for
//! `Resources::Function::addArguments()` to iterate, and by overriding
//! `getVarTypes()` to return the parameter types in declaration order
//! for signature matching.
class SeqCParamList : public SeqCAstNode {
public:
    //! \brief Constructs an empty parameter list with the given base
    //! AST attributes; parameters are appended later via `append()`.
    //! \param vc     Value category of the parameter list.
    //! \param lineNr 1-based source line number.
    //! \param dir    Parameter-direction tag.
    //! \param vt     Variable-type tag assigned to `varType_`.
    SeqCParamList(EValueCategory vc, int lineNr, EDirection dir, VarType vt);
    //! \brief Constructs a parameter list pre-populated with the
    //! given element vector.
    //! \param vc       Value category of the parameter list.
    //! \param lineNr   1-based source line number.
    //! \param dir      Parameter-direction tag.
    //! \param vt       Variable-type tag.
    //! \param elements Initial parameter nodes; ownership transfers.
    SeqCParamList(EValueCategory vc, int lineNr, EDirection dir, VarType vt,
                  std::vector<std::unique_ptr<SeqCAstNode>> elements);
    //! \brief Deep-copy constructor — clones every parameter via
    //! `doClone()` (null entries are preserved as null).
    //! \param o Source node copied from.
    SeqCParamList(SeqCParamList const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source list, copied on entry and swapped on exit.
    //! \return Reference to `*this`.
    SeqCParamList& operator=(SeqCParamList o);
    //! \brief Default destructor; releases all owned parameter nodes.
    ~SeqCParamList() override;
    //! \brief Writes the label `"ParamList"` to `std::cout`.
    void print() const override;
    //! \brief Polymorphic deep clone — returns a fresh
    //! `SeqCParamList` with cloned parameter children.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;
    //! \brief Returns raw, non-owning pointers to every parameter
    //! element in declaration order.
    //! \return Vector of borrowed parameter pointers.
    std::vector<const SeqCAstNode*> children() const override;
    //! \brief Returns the string-form list elements contributed by
    //! each parameter, used by signature rendering.
    //! \return Concatenated string elements from all parameters.
    std::vector<std::string> getListElements() const override;   // @0x2007e0
    //! \brief Lowers the parameter list: evaluates each parameter in
    //! order and accumulates the results for the enclosing function
    //! definition.
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return Accumulated `EvalResults`.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x211050

    //! \brief Returns the parameter types in declaration order for
    //! signature matching.
    //! \return Variable-type names of every parameter, in order.
    std::vector<std::string> getVarTypes() const override;  // @0x200f20

    //! \brief Append a parameter node, transferring ownership.
    //! \param elem Parameter node to take ownership of (may be null).
    void append(std::unique_ptr<SeqCAstNode> elem);
    //! \brief Read-only access to the underlying owned-parameter
    //! vector.
    //! \return Const reference to the internal element storage.
    const std::vector<std::unique_ptr<SeqCAstNode>>& elements() const {
        return elements_;
    }

    //! \brief Raw, non-owning pointers to every parameter element —
    //! used by `Resources::Function::addArguments()` to iterate the
    //! parameter nodes.
    //! \return Vector of borrowed parameter pointers.
    std::vector<const SeqCAstNode*> params() const;  // @0x201050

    //! \brief ADL-friendly swap — exchanges the base subobject and
    //! the element vector.
    //! \param a First parameter list.
    //! \param b Second parameter list.
    friend void swap(SeqCParamList& a, SeqCParamList& b);

private:
    //! \brief Owned parameter nodes in declaration order; accessed
    //! via `elements()`, `children()`, and `params()`.
    std::vector<std::unique_ptr<SeqCAstNode>> elements_;  // +0x18
};
static_assert(sizeof(SeqCParamList) == 0x30,
              "SeqCParamList must be 0x30 bytes");

// SeqCParameter class REMOVED — VarType is now a proper field
// at +0x14 in the base class SeqCAstNode, accessible via varType(). The
// old placeholder's reinterpret_cast hack is no longer needed.

// ============================================================================
// Leaf nodes with payload
// ============================================================================

// SeqCVariable (48 bytes, 0x30) — name string at +0x18 (libc++ SSO, 24B)
//! \brief Identifier reference (variable, parameter, or function name).
//!
//! Carries a name string that lowering resolves through the `Resources`
//! symbol table to produce the corresponding value, register binding, or
//! function definition.  Reused as the callee field of
//! `SeqCFunctionCall` and as the array identifier of `SeqCArray`.
class SeqCVariable : public SeqCAstNode {
public:
    //! \brief Constructs the identifier reference, taking ownership of
    //! the name string.
    //! \param vc     Value category of the reference.
    //! \param lineNr 1-based source line number.
    //! \param dir    Parameter-direction tag.
    //! \param vt     Variable-type tag assigned to `varType_`.
    //! \param name   Identifier text; moved into `name_`.
    SeqCVariable(EValueCategory vc, int lineNr, EDirection dir,
                 VarType vt, std::string name);
    //! \brief Deep-copy constructor — copies the base attributes and
    //! the identifier string.
    //! \param o Source node copied from.
    SeqCVariable(SeqCVariable const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source identifier, copied on entry and swapped on
    //! exit.
    //! \return Reference to `*this`.
    SeqCVariable& operator=(SeqCVariable o);
    //! \brief Default destructor; releases the name string.
    ~SeqCVariable() override;
    //! \brief Writes the identifier text to `std::cout`.
    void print() const override;
    //! \brief Polymorphic deep clone — returns a fresh `SeqCVariable`
    //! with identical attributes and an identical name string.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;
    //! \brief Returns `{ name_ }` — the identifier text packaged as a
    //! single-element list.
    //! \return Single-entry vector with the identifier text.
    std::vector<std::string> getListElements() const override;   // 0x209e60
    //! \brief Lowers the identifier reference: looks up the name in
    //! the `Resources` symbol table and produces the corresponding
    //! value, register binding, or function definition.
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by resolving the
    //! identifier.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x209ea0

    //! \brief Returns the identifier text carried by this node.
    //! \return Reference to the stored identifier string.
    const std::string& name() const { return name_; }

    //! \brief ADL-friendly swap — exchanges the base subobject and
    //! the identifier string.
    //! \param a First identifier node.
    //! \param b Second identifier node.
    friend void swap(SeqCVariable& a, SeqCVariable& b);

private:
    //! \brief Identifier text owned by the node; accessed via
    //! `name()`.  Layout occupies 24 bytes under libc++ (binary) and
    //! 32 bytes under libstdc++ — both are accepted by the
    //! `static_assert` below.
    std::string name_;   // +0x18 (24B libc++ SSO; 32B with libstdc++)
};
// Size differs by libstdc++ vs libc++; both forms are accepted.
static_assert(sizeof(SeqCVariable) == 0x30 || sizeof(SeqCVariable) == 0x38,
              "SeqCVariable size mismatch — expected 0x30 (libc++) or 0x38 (libstdc++)");

// SeqCValue (56 bytes, 0x38) — tagged value variant.
// Layout: SeqCAstNode base (0x18) + payload_[24] (+0x18..+0x30) + tag_ (+0x30) + pad (+0x34).
// Binary dtor uses jump table @0xb065a0 to dispatch destruction
// based on tag at +0x30 (tag -1/0xFFFFFFFF = empty, skips destruction).
//! \brief Literal-value AST node (string or double).
//!
//! Tagged-payload leaf node carrying either a string literal or a
//! double-precision number, selected by `tag()` (`Tag::eString` or
//! `Tag::eDouble`).  An empty / default-constructed instance has
//! `tag_ == -1` and holds no value.  Lowering wraps the payload into
//! a `Value` and emits an `EvalResults` carrying that constant.
class SeqCValue : public SeqCAstNode {
public:
    //! \brief Discriminator identifying which alternative is held
    //! in `payload_`.
    enum class Tag : int32_t {
        //! \brief Payload holds a `std::string` at `payload_.str`.
        eString = 0,   // variant holds std::string at +0x18
        //! \brief Payload holds a `double` at `payload_.d`.
        eDouble = 1,   // variant holds double at +0x18
        // -1 (0xFFFFFFFF) = empty/none (from dtor — skips destruction)
    };

    //! \brief Constructs an empty (`tag_ == -1`) value node; the
    //! payload-bearing constructors are used when an actual
    //! literal is parsed.
    //! \param vc     Value-category tag (lvalue / rvalue / ...).
    //! \param lineNr 1-based source line number for diagnostics.
    //! \param dir    Parameter-direction tag.
    //! \param vt     Variable-type tag.
    SeqCValue(EValueCategory vc, int lineNr, EDirection dir, VarType vt);
    //! \brief Constructs a string-payload value node tagged
    //! `Tag::eString`.
    //! \param vc     Value-category tag.
    //! \param lineNr 1-based source line number for diagnostics.
    //! \param dir    Parameter-direction tag.
    //! \param vt     Variable-type tag.
    //! \param s      String payload moved into `payload_.s`.
    SeqCValue(EValueCategory vc, int lineNr, EDirection dir, VarType vt,
              std::string s);   // 0x1fd860 (make_unique callsite) — by value per binary
    //! \brief Constructs a double-payload value node tagged
    //! `Tag::eDouble`.
    //! \param vc     Value-category tag.
    //! \param lineNr 1-based source line number for diagnostics.
    //! \param dir    Parameter-direction tag.
    //! \param vt     Variable-type tag.
    //! \param d      Double payload stored in `payload_.d`.
    SeqCValue(EValueCategory vc, int lineNr, EDirection dir, VarType vt,
              double d);               // 0x1fd950 (make_unique callsite)
    //! \brief Deep-copy constructor — copies the base attributes and
    //! the payload according to `tag_`.
    //! \param o Source node copied from.
    SeqCValue(SeqCValue const& o);
    //! \brief Copy-and-swap assignment.
    //! \param o Source value, copied on entry and swapped on exit.
    //! \return Reference to `*this`.
    SeqCValue& operator=(SeqCValue o);
    //! \brief Destructor; destroys the active payload alternative
    //! based on `tag_` (an empty / default-constructed instance with
    //! `tag_ == -1` skips destruction).
    ~SeqCValue() override;
    //! \brief Writes the label `"Value"` to `std::cout`.
    void print() const override;
    //! \brief Polymorphic deep clone — returns a fresh `SeqCValue`
    //! with the same `tag_` and a copy of the payload.
    //! \return Owning pointer to the cloned node.
    std::unique_ptr<SeqCAstNode> doClone() const override;

    //! \brief Lowers the literal: wraps the payload into a `Value`
    //! and emits an `EvalResults` carrying that constant.
    //! \param res   Shared compile resources.
    //! \param ctx   Lowering context.
    //! \param state Per-call lowering state.
    //! \return The `EvalResults` produced by lowering this literal.
    std::shared_ptr<EvalResults> evaluate(
        std::shared_ptr<Resources> res,
        FrontendLoweringContext& ctx,
        FrontendLoweringState& state) const override;  // @0x213140

    //! \brief Returns the discriminator selecting the active
    //! payload alternative; `static_cast<Tag>(-1)` indicates the
    //! empty / default-constructed state.
    //! \return The active `Tag`, or `static_cast<Tag>(-1)` for the
    //! empty state.
    Tag tag() const { return static_cast<Tag>(tag_); }

    //! \brief Returns the held double; well-defined only when
    //! `tag() == Tag::eDouble`.
    //! \return The double stored in `payload_.d`.
    double asDouble() const {
        return payload_.d;
    }

    //! \brief Returns the held string by const reference;
    //! well-defined only when `tag() == Tag::eString`.
    //! \return Const reference to the string stored in
    //! `payload_.str`.
    const std::string& asString() const {
        return payload_.str;
    }

    //! \brief ADL-friendly swap — exchanges only `varType_` and the
    //! tagged payload (`payload_` + `tag_`); the base subobject
    //! (`valueCategory_`, `lineNr_`, `direction_`) is **not** swapped.
    //! See the definition's `\binarynote` for the rationale.
    //! \param a First value node.
    //! \param b Second value node.
    friend void swap(SeqCValue& a, SeqCValue& b);

    // Payload union — must accommodate std::string which is 24 bytes on
    // libc++ (binary) but 32 bytes on libstdc++ (reconstruction).
    // Using a real std::string member avoids the buffer-overflow that
    // occurred with char[24] on libstdc++.
    //! \brief Active-alternative storage for the literal payload —
    //! a raw union of `double` and `std::string` whose live member
    //! is selected by the enclosing `tag_` field.  The
    //! default/destructor are user-provided so the enclosing
    //! `SeqCValue` can manage the active alternative's lifetime
    //! according to `tag_`.
    union Payload {
        //! \brief Held when `tag_ == eDouble`.
        double      d;
        //! \brief Held when `tag_ == eString`.
        std::string str;

        //! \brief Default-constructs the union with the
        //! `double` alternative active (zero-initialised); the
        //! enclosing node sets `tag_` accordingly and replaces the
        //! alternative through placement-new when a string is
        //! installed.
        Payload() : d(0.0) {}
        //! \brief Trivial union destructor — the enclosing
        //! `SeqCValue` is responsible for destroying the active
        //! alternative based on `tag_` before this runs.
        ~Payload() {}
    };

private:
    //! \brief Storage for the active payload alternative; the live
    //! union member is selected by `tag_`.
    Payload  payload_;         // +0x18  (24 bytes libc++, 32 bytes libstdc++)
    //! \brief Discriminator that selects the active `payload_`
    //! alternative; `-1` denotes the empty / default-constructed
    //! state (in which case the destructor skips destruction).
    int32_t  tag_{-1};         // +0x30 (libc++) / +0x38 (libstdc++)
    //! \brief Tail padding word kept so the binary's slot layout is
    //! preserved; carries no semantic value.
    int32_t  pad34_{};         // +0x34 / +0x3C
};
// On libc++ (binary): sizeof == 0x38.  On libstdc++: sizeof >= 0x38 (typically 0x40).
static_assert(sizeof(SeqCValue) >= 0x38, "SeqCValue must be at least 0x38 bytes");

} // namespace zhinst
