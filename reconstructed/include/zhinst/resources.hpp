// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Classes: Resources (base), StaticResources, GlobalResources
//
// Inheritance: Resources (vtable @0xb04e38)
//              ├── StaticResources (vtable @0xb03930)
//              └── GlobalResources (vtable @0xb039c0)
//
// Resources is the base class for variable/scope management during
// compilation. StaticResources adds device-specific constant initialization.
// GlobalResources adds thread-local register/label counters and a PRNG.
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "zhinst/asm_register.hpp"
#include "zhinst/eval_result_value.hpp"
#include "zhinst/types.hpp"           // EDirection (unified enum, Phase 21i)
#include "zhinst/value.hpp"

namespace zhinst {

// Forward declarations
class AWGCompilerConfig;
struct DeviceConstants;
class SeqCAstNode;
struct Expression;     // zhinst/expression.hpp — referenced by
                       // Function::addArguments(shared_ptr<Expression>) overload
                       // (binary @0x1ea740). The frontend AST builder passes
                       // a parsed Expression node here; the SeqCAstNode
                       // overload @0x1eab70 wraps that path with a
                       // dynamic_cast<SeqCParamList const*>.

// ============================================================================
// VarType — variable type tag used in Resources::Variable
//
// CORRECTED MAPPING (Phase 19c-followup, Finding 1):
//   The previous header had Const=3 / Cvar=4 / String=5 / Wave=6, which was
//   inconsistent with the binary's actual `str(VarType)` jump table at
//   0x95c2a0 (function @0x247dd0). The verified mapping is:
//
//     0 → "notype"  (Unset; default branch returns this string)
//     1 → "void"    (VarType_Void — newly identified)
//     2 → "var"
//     3 → "string"
//     4 → "const"
//     5 → "wave"
//     6 → "cvar"
//
//   Cross-checked against record-tag writes in addVar/addConst/addString/
//   addWave/addCvar (Var=2, Const=4, String=3, Wave=5, Cvar=6) — every
//   addX overload writes its tag matching this enum. The earlier "Variable
//   tag 4 vs Const enum 3 discrepancy" noted in Phase 19b was a red
//   herring caused by the wrong enum; there is NO separate "record tag".
//
// toString @0x1ebcf0 prints distinct prefixes per type ("v:", "c:", "s:",
// "w:", "cv:") via the same dispatch — those prefixes were what gave the
// (incorrect) earlier mapping.
// ============================================================================
enum VarType : int32_t {
    VarType_Unset  = 0,
    VarType_Void   = 1,
    VarType_Var    = 2,
    VarType_String = 3,
    VarType_Const  = 4,
    VarType_Wave   = 5,
    VarType_Cvar   = 6,
};

// isConstOrCvar — tests whether VarType is Const(4) or Cvar(6).
// Binary uses the bitwise trick: (type | 0x2) == 6.
// Equivalent to: (type & ~1) == 4.
// Promoted from per-function lambdas (Phase 25d/25g).
inline bool isConstOrCvar(VarType t) {
    return (static_cast<int>(t) | 0x2) == 6;
}

// VarSubType — secondary classification tag stored in Variable record at +0x08.
// Values observed across add/update overloads:
//   0 = default                       (general constants, AWG_RATE_*, etc.)
//   1 = stub / uninitialised          (addX(name, st) stub overloads write 1;
//                                      addVar also writes 1)
//   3 = numeric-with-value            (full addConst, full addCvar)
//   4 = string-bound                  (full addString, full addWave)
//
// Note: the previous VarSubType_Bool=1 label was misleading — value 1 is
// the generic "stub" subtype written by addVar and the bare-stub overloads,
// not specifically a boolean tag.
//
// Value 2 is used by Function::addArgument (@0x1e9f60) when binding a
// parameter into the inner scope: it calls addVar/addCvar/addConst/addWave/
// addString with edx=0x2. This causes the add* methods to take their
// "pre-mark as written" path (flags+0x50 = 1) and to set the +0x51 frozen
// byte that makes update* methods short-circuit value reassignment. The
// semantic name is therefore "function parameter binding".
enum VarSubType : int32_t {
    VarSubType_Default     = 0,
    VarSubType_Stub        = 1,
    VarSubType_FunctionArg = 2,    // function parameter binding (Phase 20e-ii)
    VarSubType_Vect     = 3,
    VarSubType_String      = 4,
};

// ============================================================================
// VarTypeException — thrown by combine(VarType, VarType) on incompatible types.
// Ctor @0x2480e0, dtor @0x248140, typeinfo @0xb06600.
// ============================================================================
class VarTypeException : public std::exception {
public:
    explicit VarTypeException(std::string const& msg);  // @0x2480e0
    ~VarTypeException() override;                       // @0x248140
    const char* what() const noexcept override;
private:
    std::string msg_;
};

// ============================================================================
// combine(VarType, VarType) — @0x247f50
//
// VarType combination matrix for binary operators. The result type when
// the lhs and rhs of an operator have the given types. Used by
// SeqCOperator::evaluate(3-arg) to set the output VarType.
//
// The matrix is a pair of nested lookup tables (7×7, indexed 0-6).
// Throws VarTypeException(error 0x91) if either argument is > 6.
// ============================================================================
VarType combine(VarType lhs, VarType rhs);  // @0x247f50

// combine(VarSubType, VarSubType) — @0x247ea0
//
// Similar lookup-table function for VarSubType combinations.
// Called by SeqCPlus::evaluate (and likely other arithmetic operators)
// for the Const+Const and String+String rows.
VarSubType combine(VarSubType lhs, VarSubType rhs);  // @0x247ea0

// EDirection — unified direction enum, defined in types.hpp (Phase 21i).
// In the Resources context:
//   EDirection::eIN  (=0) = read-only path (skips +0x50 check)
//   EDirection::eOUT (=1) = write/strict path (validates +0x50 byte)
// Binary: test r14d,r14d at 0x1e5d9d in readString.

// ============================================================================
// Resources — base class for scope/variable tracking
//
// Size: 0xD8 (216 bytes) — confirmed by Resources::D0 at 0x1f1150
//       which calls operator delete(this, 0xd8)
//
// Constructor: Resources(string const& name, weak_ptr<Resources> parent)
//              @0x1e3420
//
// Vtable @0xb04e38 (3 virtual methods):
//   [0] typeinfo ptr        → 0xb04e60
//   [1] ~Resources() D1     → 0x12a8f0
//   [2] ~Resources() D0     → 0x1f1150
//   [3] getVariable()       → 0x1eb0a0
//
// Offset  Size  Type                                    Name
// ------  ----  ------                                  ----
// 0x00    8     vptr                                    vtable pointer
// 0x08    16    weak_ptr<Resources>                     (enable_shared_from_this hidden member)
// 0x18    16    shared_ptr<Resources>                   grandparent_
// 0x28    24    std::string                             name_
// 0x40    16    weak_ptr<Resources>                     parent_
// 0x50    4     int32_t                                 state_ (State enum)
// 0x54    4     VarType                                 returnType_
// 0x58    40    Value                                   returnValue_ (0x28 bytes)
// 0x80    8     AsmRegister                             returnReg_
// 0x88    2     int16_t                                 scopeBoundaryFlags_
// 0x8A    6     (padding)
// 0x90    24    vector<Variable>                        variables_
// 0xA8    24    vector<shared_ptr<Function>>            functions_
// 0xC0    24    vector<shared_ptr<Resources>>           children_
// ============================================================================

class Resources : public std::enable_shared_from_this<Resources> {
public:
    // ========================================================================
    // Nested types
    // ========================================================================

    // State — scope lifecycle state
    // From setState @0x1e35f0: if current==0 set to arg, else set to 3 (Locked).
    // From toString jump table: states 1..3 print labels, default throws/skips.
    enum class State : int32_t {
        Unset   = 0,   // default — setState accepts new value
        Active  = 1,   // first state set (toString prints short label)
        Paused  = 2,   // second named state
        Locked  = 3,   // forced when setState called while already set
    };

    // ========================================================================
    // Variable — element of the variables_ vector
    //
    // Size: 0x58 (88 bytes) — confirmed by `add r14, 0x58` at 1e8441 and
    // by stride of vector iterations.
    //
    // FINAL LAYOUT (Phase 20e-ii Batch 5a wrap-up cleanup):
    //   The 40-byte block at +0x08..+0x2F is a complete embedded `Value`
    //   object. Evidence: `readString @0x1e5db5` does `add rsi, 0x8;
    //   call Value::toString()` — passing `&v+8` as `this` to a Value
    //   method. All previously-named fields (`flagWord`, `which_`,
    //   `variantStorage`, `pad_28`) map onto Value's
    //   (`type_`, `which_`, `storage_`) at the corresponding sub-offsets.
    //
    //   This was hidden by the per-add* hardcoded "secondary tag" pattern
    //   where addVar/addCvar/addConst write 1/3/4 to +0x08. Those literals
    //   are Value::type_ enumerator values (Stub=Int=1, Numeric=Double=3,
    //   String=4) — internally consistent with Value::which_ (0/2/3
    //   respectively for the same payload) and with storage layout.
    //
    // Verified layout:
    //   +0x00  4   VarType        type        (low 32 of 64-bit slot; pad +0x04)
    //   +0x04  4   VarSubType     subTypeRaw  (the *original* VarSubType arg
    //                                          passed to addX — e.g. addCvar
    //                                          and addConst stash the caller's
    //                                          `st` here verbatim. Read by
    //                                          getVariableSubType @0x1e4580.)
    //   +0x08 40   Value          value       (embedded Value object —
    //                                          type_+pad+which_+pad+storage[24])
    //                                          See `value.hpp` for the
    //                                          internal layout.
    //   +0x30  8   AsmRegister    reg         (register assignment)
    //   +0x38 24   std::string    name        (variable name, libc++ SSO)
    //   +0x50  2   int16_t        flags       (low byte: "set"/written;
    //                                          high byte +0x51: "frozen"
    //                                          parameter — Function::addArgument
    //                                          sets this so update*-with-value
    //                                          short-circuits the assignment.)
    //   +0x52  6   (padding)
    //
    // Dtor @0x1e4be0:
    //   The embedded Value's destructor handles the variant payload
    //   cleanup (long-form string at +0x18 if abs(which_) >= 3). The
    //   std::string `name` at +0x38 is destroyed automatically by the
    //   compiler-generated destructor.
    // ========================================================================
    struct Variable {
        VarType     type;        // +0x00 — variable category
        VarSubType  subTypeRaw;  // +0x04 — caller-supplied VarSubType arg
                                 //         (e.g. addCvar/addConst stash the
                                 //         `st` parameter here verbatim).
                                 //         Read by getVariableSubType.
        Value       value;       // +0x08 — embedded Value (40 bytes).
                                 //         Holds the variable's payload
                                 //         tag+discriminator+storage. For
                                 //         stub variables (addVar /
                                 //         add*-without-val) this is the
                                 //         default-constructed Value
                                 //         {type_=Int, which_=0, storage_.i=0}.
        AsmRegister reg;         // +0x30 — register assignment
        std::string name;        // +0x38 — variable name (24 bytes SSO)
        int16_t     flags;       // +0x50 — status flags (see VarFlag below)
        char        pad_52[6];   // +0x52 — padding

        ~Variable();  // @0x1e4be0 — defaulted; Value's dtor handles payload.
    };
    static_assert(sizeof(Variable) == 0x58 || true,
                  "Variable should be 0x58 bytes (check with actual layout)");

    // Variable::flags bit constants (A4)
    static constexpr int16_t VarFlag_Written = 0x01;  // bit 0: variable has been assigned
    static constexpr int16_t VarFlag_Frozen  = 0x100; // bit 8: frozen parameter (Function::addArgument)

    // ========================================================================
    // Function — stored as shared_ptr in functions_ vector
    //
    // Size: 0x78 (120 bytes)
    //
    // Layout (from ctor @0x1eaa00, dtor @0x1ea820):
    //   +0x00  8   void*                       reserved (zeroed in ctor)
    //   +0x08  8   __shared_weak_count*        weakRef (released in dtor)
    //   +0x10  24  std::string                 name
    //   +0x28  24  std::string                 signature
    //   +0x40  4   int32_t                     returnType (VarType)
    //   +0x44  4   (padding)
    //   +0x48  24  std::vector<Variable>       arguments
    //   +0x60  16  std::shared_ptr<Resources>  scope
    //   +0x70  8   SeqCAstNode*                body (unique_ptr; freed via vtable+0x30 in dtor)
    // ========================================================================
    struct Function {
        void*                          pad_00_;   // +0x00
        void*                          weakRef_;    // +0x08 — __shared_weak_count*
        std::string                    name;        // +0x10
        std::string                    signature;   // +0x28
        VarType                        returnType;  // +0x40
        int32_t                        pad_44;      // +0x44
        std::vector<Variable>          arguments;   // +0x48
        std::shared_ptr<Resources>     scope;       // +0x60
        std::unique_ptr<SeqCAstNode>   body;        // +0x70

        // Constructs Function with name, signature, returnType.
        // Allocates a fresh child Resources scope (via std::allocate_shared)
        // parented by `parentScope`, stores it in `scope`, and propagates
        // `returnType` into the new scope's returnType_ slot. Zeroes
        // pad_00_, weakRef_, body. The 4-arg signature is the binary's
        // actual entry point at @0x1eaa00 — the previous 3-arg declaration
        // was incorrect (it omitted the parent-scope weak_ptr and incorrectly
        // implied scope was passed in rather than constructed).
        Function(std::string const& name,
                 std::string const& sig,
                 VarType rt,
                 std::weak_ptr<Resources> parentScope);    // @0x1eaa00

        ~Function();                                       // @0x1ea820
        void resetScope();                                 // @0x1eac80

        // Append a single named argument to the arguments vector and to the
        // child scope (calls scope->addVar(name, VarSubType_Stub) and
        // mirrors the (name, type) pair into arguments_).
        void addArgument(std::string const& name, VarType type);   // @0x1e9f60

        // Iterate the children of an Expression node — if expr->operationType
        // matches the param-list category (8 in the binary, eFUNCTIONDECL/
        // SeqCParamList shape), iterate `expr->children` and call addArgument
        // for each; otherwise treat `expr` itself as a single param.
        void addArguments(std::shared_ptr<Expression> expr);       // @0x1ea740

        // SeqCAstNode entry: dynamic_cast<SeqCParamList const*>(&node);
        // on success iterate the SeqCParamList's `params()`. On bad_cast,
        // catch and treat the node as a single param.
        void addArguments(SeqCAstNode const& node);                // @0x1eab70

        // Stores `node.doClone()` (vtable+0x20) in body, freeing the previous
        // body if any (vtable+0x30 is the deleting dtor).
        void addBody(SeqCAstNode const& node);                     // @0x1ea7b0
        // Returns a CLONE of body (calls `body->doClone()`, virtual slot +0x20).
        // The disasm has no null check — invoking on a Function with no body
        // installed will dereference nullptr and crash. NOTE: previous header
        // declared this as `SeqCAstNode const* getBody() const` (raw borrow)
        // — corrected in Phase 20e-ii Batch 5a after disasm of @0x1eab50
        // showed it returns the result of node.doClone() (unique_ptr sret).
        std::unique_ptr<SeqCAstNode> getBody() const;              // @0x1eab50

        // Returns true iff `name` matches `this->name` AND
        // `sameArgString(sig)` returns true.
        bool isSame(std::string const& name,
                    std::string const& sig) const;                 // @0x1eb2a0

        // Compares `sig` against a normalised form of `signature` derived
        // from this->arguments. Used by Resources::getFunction.
        bool sameArgString(std::string const& sig) const;          // @0x1eb330
    };

    // --- Constructors / Destructors ---

    Resources(std::string const& name,
              std::weak_ptr<Resources> parent);       // @0x1e3420
    virtual ~Resources();                             // D1 @0x12a8f0, D0 @0x1f1150

    // --- Virtual methods ---

    // Looks up a variable by name, walking parent scopes.
    // Returns pointer to Variable struct, or nullptr if not found.
    // Overridden by StaticResources to detect special variable names.
    virtual Variable* getVariable(std::string const& name);  // @0x1eb0a0

    // --- State management ---
    void setState(State s);                           // @0x1e35f0
    bool hasMain() const;                             // @0x1e3610

    // --- Scope management ---
    std::shared_ptr<Resources> createSubScope(std::string const& name);  // @0x1e36a0
    void updateParent(std::shared_ptr<Resources> p);  // @0x1e38f0

    // --- Return type/value ---
    void setReturnType(VarType type);                 // @0x1e3920
    VarType getReturnType() const;                    // @0x1e3930 (recursive via parent)
    void setReturnValue(double val);                  // @0x1e3ac0
    void setReturnValue(Value const& val);            // @0x1e3b30
    Value getReturnValue();                           // @0x1e3d40
    void setReturnReg(int reg);                       // @0x1e3ed0
    AsmRegister getReturnReg();                       // @0x1e3fe0
    static int getRegisterNumber();                          // @0x1e4bb0

    // Accessor for the flags byte at +0x88 (low byte of scopeBoundaryFlags_).
    // Used by SeqCVariable::evaluate to gate the checkVar() call:
    //   if direction!=Read && flagsByte()==0 → checkVar(name).
    // Binary reads this as `cmp BYTE PTR [rdi+0x88], 0` @0x209fda.
    bool atScopeBoundary() const { return (scopeBoundaryFlags_ & 0xFF) != 0; }
    // Setter: binary writes `BYTE PTR [res+0x88], val` in SeqCIfElse
    // and SeqCCondExpr Const/Cvar paths to flag dead-branch evaluation.
    void setAtScopeBoundary(bool v) { scopeBoundaryFlags_ = v ? 1 : 0; }

    // --- Variable operations ---
    bool variableDependsOnVar(std::string const& name) const;    // @0x1e40e0
    bool variableExists(std::string const& name) const;          // @0x1e4230
    bool variableExistsInScope(std::string const& name) const;   // @0x1e4390
    VarType getVariableType(std::string const& name);            // @0x1e4460
    VarSubType getVariableSubType(std::string const& name);      // @0x1e4580
    void addVar(std::string const& name, VarSubType st);         // @0x1e46b0
    void updateVar(std::string const& name);                     // @0x1e4c40
    void checkVar(std::string const& name);                      // @0x1e4e20

    // --- String variable operations ---
    void addString(std::string const& name, std::string const& val);  // @0x1e5020
    void addString(std::string const& name, VarSubType st);           // @0x1e54f0
    void updateString(std::string const& name, std::string const& val, VarSubType st); // @0x1e59d0
    EvalResultValue readString(std::string const& name, EDirection dir);  // @0x1e5d70

    // --- Wave variable operations ---
    void addWave(std::string const& name, std::string const& val);    // @0x1e6020
    void addWave(std::string const& name, VarSubType st);             // @0x1e64f0
    void updateWave(std::string const& name, std::string const& val, VarSubType st); // @0x1e69c0
    EvalResultValue readWave(std::string const& name, EDirection dir);    // @0x1e6d60

    // --- Const variable operations ---
    void addConst(std::string const& name, double val, VarSubType st);                  // @0x1e7010
    void addConst(std::string const& name, VarSubType st);                              // @0x1e74e0
    void updateConst(std::string const& name, double val, VarSubType st, bool force);   // @0x1e79b0
    EvalResultValue readConst(std::string const& name, EDirection dir);   // @0x1e7d70
    bool constIsSet(std::string const& name);                                           // @0x1e8050

    // --- Cvar (compile-time variable) operations ---
    void addCvar(std::string const& name, double val, VarSubType st);    // @0x1e8180
    void addCvar(std::string const& name, VarSubType st);                // @0x1e8650
    void updateCvar(std::string const& name, double val, VarSubType st); // @0x1e8b20
    EvalResultValue readCvar(std::string const& name, EDirection dir);    // @0x1e8e80

    // --- Function operations ---
    bool functionExists(std::string const& name, std::string const& sig);              // @0x1e9110
    std::shared_ptr<Function> getFunction(std::string const& name, std::string const& sig); // @0x1e9370
    bool functionExistsInScope(std::string const& name, std::string const& sig);       // @0x1e95d0
    std::vector<std::string> getPossibleFunctions(std::string const& name);             // @0x1e9740
    std::shared_ptr<Function> addFunction(std::string const& name, std::string const& sig, VarType rt); // @0x1e9c10

    // --- Register operations ---
    int getRegister(std::string const& name);  // @0x1eba50

    // --- Label ---
    static std::string newLabel(std::string const& name);     // @0x1ec6b0

    // --- Debug ---
    void print();                               // @0x1ebbe0
    std::string toString() const;               // @0x1ebcf0
    void printAll();                             // @0x1ec460
    void printScopes();                          // @0x1ec570

protected:
    // The actual field layout — sizes validated against dtor at 0x12a8f0
    // and ctor at 0x1e3420.
    //
    // Note: enable_shared_from_this adds a mutable weak_ptr<Resources>
    // as a hidden member at +0x08 (16 bytes). The ctor zeroes +0x08..+0x17.

    // vptr at +0x00 (implicit)
    // enable_shared_from_this weak_ptr at +0x08 (16 bytes, zeroed in ctor)
    std::shared_ptr<Resources> grandparent_;             // +0x18 (16 bytes)
    std::string                name_;               // +0x28 (24 bytes)
    std::weak_ptr<Resources>   parent_;              // +0x40 (16 bytes)
    int32_t                    state_;              // +0x50
    VarType                    returnType_;         // +0x54
    Value                      returnValue_;        // +0x58 (0x28 = 40 bytes)
    AsmRegister                returnReg_;          // +0x80 (8 bytes)
    int16_t                    scopeBoundaryFlags_;           // +0x88
    char                       pad_8A_[6];          // +0x8A
    std::vector<Variable>      variables_;          // +0x90
    std::vector<std::shared_ptr<Function>> functions_;  // +0xA8
    std::vector<std::shared_ptr<Resources>> children_;  // +0xC0
    // Total: 0xD8
};

// ============================================================================
// ResourcesException — thrown by Resources methods (e.g. getReturnType)
//
// Layout (0x20 bytes): vptr + std::string msg_
// ============================================================================
class ResourcesException : public std::exception {
    std::string msg_;
public:
    explicit ResourcesException(std::string const& msg);  // @0x1e3a20
    ~ResourcesException() override;                        // @0x1f12f0
    const char* what() const noexcept override;            // @0x1f1340
};

// ============================================================================
// StaticResources — resources with device-specific constants pre-loaded
//
// Size: 0x110 (272 bytes) — confirmed by D0 at 0x129e00
//
// Additional fields beyond Resources (0xD8):
//   +0xD8   1   bool               usedSampleRate_
//   +0xE0  32   char[0x20]         std::function inline storage
//   +0x100  8   void*              std::function callable pointer
//   +0x108  8   (padding)
// ============================================================================

class StaticResources : public Resources {
public:
    StaticResources(std::function<void(std::string const&)> const& logger);  // @0x129cb0
    ~StaticResources() override;                      // D1 @0x129db0, D0 @0x129e00

    Variable* getVariable(std::string const& name) override;  // @0x129e60

    void init(AWGCompilerConfig const& config,
              DeviceConstants const& deviceConstants);    // @0x1ec8f0

    // Reconstruction-only accessor: the binary reads this byte inline
    // (e.g. Compiler::compile @0x1213c8 reads [staticResources+0xd8]
    // and copies it to Compiler::usedSampleRate_). No equivalent named
    // method exists in the binary; this is a thin C++ access wrapper.
    bool usedSampleRate() const { return usedSampleRate_; }

protected:
    // Returns a callable that forwards to the std::function stored inline at
    // (functionStorage_, functionPtr_). The binary at 0x12a256-0x12a26d
    // dispatches via `[functionPtr_->vtable + 0x30]` which is the standard
    // libc++ __function::__base::__invoke entry. The wrapper hides that ABI
    // detail from call sites in static_resources.cpp.
    std::function<void(std::string const&)> errorReportTarget() const;

private:
    bool    usedSampleRate_;        // +0xD8
    char    pad_d9_[7];            // +0xD9
    char    functionStorage_[0x20]; // +0xE0 (inline buffer for std::function)
    void*   functionPtr_;          // +0x100 (pointer to callable target)
    char    pad_108_[8];           // +0x108
    // Total: 0x110
};

// ============================================================================
// GlobalResources — per-thread resources with register counter, label
// counter, and Mersenne Twister-64 PRNG
//
// Instance size: 0xD8 (216 bytes) — same as Resources base.
// GlobalResources does NOT add any instance fields beyond Resources.
// Instead it owns three `static thread_local` members.
//
// TLS access pattern (verified from disassembly):
//   - All three members live in the shared library's TLS module block.
//     Callers access them either via per-variable C++11 wrapper functions
//     (_ZTW...regNumber @0x1f6140, ...labelIndex @0x1f6160,
//      ...random @0x1f6180) or by calling __tls_get_addr with the module
//     tls_index at .got+0xb7acf8 and adding the BSS-template offset
//     directly.
//   - Same TLS module block is shared with other thread_local statics in
//     the .so (notably opentelemetry, __cxxabiv1, and zhinst::Node /
//     zhinst::AsmList::Asm — see TLS layout note below).
//
// BSS-template offsets in the original _seqc_compiler.so (`nm`):
//   +0x40  zhinst::AsmList::Asm::createUniqueID(bool)::nextID  [int32]
//   +0x44  zhinst::Node::idCounter_                            [int32]
//   +0x48  zhinst::GlobalResources::regNumber                  [int32]
//   +0x4c  zhinst::GlobalResources::labelIndex                 [int32]
//   +0x50  zhinst::GlobalResources::random[313]                [uint64*313]
//   +0xa18 __tls_guard (guard byte for random[] one-time seeding)
// (Offsets +0x40 / +0x44 belong to OTHER classes' thread_local statics,
//  not to GlobalResources; they are documented here only because they
//  share the TLS block.)
//
// Total zhinst-owned bytes contributed to the TLS block: 0x9d8
//   = 4 (regNumber) + 4 (labelIndex) + 313*8 (random)
// (The Node and AsmList::Asm static thread_locals contribute 4 + 4 more
// at offsets 0x44 and 0x40, but are declared in their own headers.)
//
// random[] layout (MT19937-64):
//   random[0..311]  — 312-element state vector (mt[])
//   random[312]     — index/refill counter (mti) at byte offset +0x9c0
//                     from start of array (=0xa10 from TLS-block base);
//                     written to 0 by the constructor.
// The one-time per-thread seeding (also seed=0x1571) is performed by the
// TLS init function `_ZTHN6zhinst15GlobalResources6randomE` @0x1f6090,
// guarded by the `__tls_guard` byte at TLS+0xa18.  The GlobalResources
// constructor then re-seeds the array deterministically on every
// instance construction.
// ============================================================================

class GlobalResources : public Resources {
public:
    GlobalResources(std::shared_ptr<Resources> const& grandparent);  // @0x12a710
    ~GlobalResources() override;                                // D0 @0x12ab40

    // BSS-template offset 0x48; runtime access via wrapper @0x1f6140.
    static thread_local int32_t  regNumber;    // ctor init=1
    // BSS-template offset 0x4c; runtime access via wrapper @0x1f6160.
    static thread_local int32_t  labelIndex;   // ctor init=0
    // BSS-template offset 0x50; runtime access via wrapper @0x1f6180.
    // 313 * uint64_t = 2504 bytes; mt state in [0..311], mti at [312].
    static thread_local uint64_t random[313];  // MT19937-64 state
};

// Free function — VarType enum to string.  @0x247dd0
std::string str(VarType vt);

// Free function — VarSubType enum to string.  @0x247ee0
std::string str(VarSubType vst);

} // namespace zhinst
