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

#include "zhinst/asm/asm_register.hpp"
#include "zhinst/ast/eval_result_value.hpp"
#include "zhinst/core/types.hpp"           // EDirection (unified enum, )
#include "zhinst/ast/value.hpp"

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
// Verified from jump table at rodata 0x95c2a0 (str(VarType) @0x247dd0)
// and record-tag writes in addVar/addConst/addString/addWave/addCvar.
// ============================================================================
//! \brief Top-level type tag stored in `Resources::Variable::type`
//! identifying which SeqC value-category a symbol-table entry holds.
//!
//! Set by every `Resources::add*` family member when inserting a new
//! `Variable`, read back by every `read*`/`update*`/`check*` family
//! member to enforce typed access.  The free function
//! `str(VarType)` returns the human-readable spelling
//! (`"void"` / `"var"` / `"string"` / `"const"` / `"wave"` /
//! `"cvar"`; default `"notype"`).
enum VarType : int32_t {
    VarType_Unset  = 0,   //!< Default-initialised slot; not a SeqC type.
    VarType_Void   = 1,   //!< Function returns no value.
    VarType_Var    = 2,   //!< Mutable runtime variable backed by an `AsmRegister`.
    VarType_String = 3,   //!< String literal or string-valued variable.
    VarType_Const  = 4,   //!< Compile-time constant, written once and frozen.
    VarType_Wave   = 5,   //!< Waveform reference (string-bound, names a wave).
    VarType_Cvar   = 6,   //!< Compile-time variable; mutable but resolved at compile time.
};

// isConstOrCvar — tests whether VarType is Const(4) or Cvar(6).
// Binary uses the bitwise trick: (type | 0x2) == 6.
// Equivalent to: (type & ~1) == 4.
// Promoted from per-function lambdas.
//! \brief Test for the const-like categories that share the
//! "compile-time-evaluated" semantics in the SeqC type system.
//!
//! Returns true iff `t` is `VarType_Const` (4) or `VarType_Cvar` (6);
//! the inline body uses the bitwise identity
//! `(t | 0x2) == 6`, equivalent to `(t & ~1) == 4`.
//! \param t Variable category to test.
//! \return true if `t` is one of the const-or-cvar categories.
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
//! \brief Secondary classification stashed in
//! `Resources::Variable::subTypeRaw` (+0x04) recording the sub-form
//! of an `add*` call.
//!
//! `add*(name, value, st)` overloads pass the caller-supplied `st`
//! verbatim; the bare-stub `add*(name, st)` overloads pass the
//! caller's `st` and adjust the variable's `flags` byte.  The
//! `FunctionArg` value (2) is the parameter-binding sentinel used by
//! `Resources::Function::addArgument` — it pre-marks the variable as
//! written and freezes future `update*` value reassignment.
enum VarSubType : int32_t {
    VarSubType_Default     = 0,    //!< Unclassified default (general constants, AWG_RATE_*, etc.).
    VarSubType_Stub        = 1,    //!< Declared without a value; written by addVar and bare-stub overloads.
    VarSubType_FunctionArg = 2,    //!< Function parameter binding; pre-marks variable as written and frozen.
    VarSubType_Vect     = 3,       //!< Numeric value, set by full `addConst` / `addCvar` overloads.
    VarSubType_String      = 4,    //!< String-bound payload, set by full `addString` / `addWave` overloads.
};

// ============================================================================
// VarTypeException — thrown by combine(VarType, VarType) on incompatible types.
// Ctor @0x2480e0, dtor @0x248140, typeinfo @0xb06600.
// ============================================================================
//! \brief Diagnostic raised by the `combine(VarType, VarType)` lookup
//! when an arithmetic/logical operator is asked to combine two
//! variable categories that have no defined result type (e.g. mixing
//! a wave with a string).
//!
//! Carries a free-form message returned verbatim by `what()`; raised
//! from inside `SeqCOperator::evaluate` when the 7×7 result-type
//! table for the operator's arguments has no entry.
class VarTypeException : public std::exception {
public:
    //! \brief Construct with the diagnostic text returned later by
    //! `what()`.
    //! \param msg Human-readable error message; copied into the
    //!        exception payload.
    explicit VarTypeException(std::string const& msg);  // @0x2480e0
    //! \brief Destroy the exception and free the stored message.
    ~VarTypeException() override;                       // @0x248140
    //! \brief Return the stored diagnostic text.
    //! \return Null-terminated C string owned by the exception; valid
    //!         for the lifetime of `*this`.
    const char* what() const noexcept override;
private:
    std::string msg_;   //!< Diagnostic text returned by `what()`.
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
//! \brief Combine two `VarType` values via the SeqC binary-operator
//! result-type table.
//!
//! Implements the 7×7 lookup used by `SeqCOperator::evaluate(3-arg)`
//! to choose the result `VarType` for an operator whose operands have
//! categories `lhs` and `rhs` (e.g. `Const + Var → Var`).
//! \param lhs Left-hand operand category (must be in [0..6]).
//! \param rhs Right-hand operand category (must be in [0..6]).
//! \return The combined result `VarType`.
//! \throws VarTypeException When either argument is greater than 6
//!         or no entry exists for the pair.
VarType combine(VarType lhs, VarType rhs);  // @0x247f50

//! \brief Combine two `VarSubType` values via a smaller lookup
//! table used by arithmetic operators on the Const+Const and
//! String+String rows of the `combine(VarType, VarType)` matrix.
//! \param lhs Left-hand sub-type tag.
//! \param rhs Right-hand sub-type tag.
//! \return The combined `VarSubType`.
VarSubType combine(VarSubType lhs, VarSubType rhs);  // @0x247ea0

// EDirection — unified direction enum, defined in types.hpp.
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
// 0x50    4     State                                   state_
// 0x54    4     VarType                                 returnType_
// 0x58    40    Value                                   returnValue_ (0x28 bytes)
// 0x80    8     AsmRegister                             returnReg_
// 0x88    2     int16_t                                 scopeBoundaryFlags_
// 0x8A    6     (padding)
// 0x90    24    vector<Variable>                        variables_
// 0xA8    24    vector<shared_ptr<Function>>            functions_
// 0xC0    24    vector<shared_ptr<Resources>>           children_
// ============================================================================

//! \brief Lexical-scope and symbol-table node used by the SeqC
//! frontend during AST evaluation.
//!
//! One `Resources` instance represents one scope (the program root,
//! a function body, a `for` / `while` / `if` block, etc.) and is
//! linked into a tree via `parent_` / `children_`.  Each scope owns
//! its own `variables_` table (general-purpose `Variable` records
//! holding type + sub-type + embedded `Value` + assigned register +
//! name + flags) and its own `functions_` table of declared
//! `Function` records (each with its own child scope for the
//! function body).  `getVariable` walks the parent chain to resolve
//! a name, and the typed add / update / read families on the
//! interface (`addVar`, `addConst`, `addCvar`, `addString`,
//! `addWave`, plus the corresponding `update*` and read accessors)
//! provide the SeqC type system's CRUD operations on the current
//! scope.
//!
//! The base class also tracks transient evaluation state: the
//! lifecycle `state_` (Unset → Active → Paused → Locked),
//! the function-scope `returnType_` / `returnValue_` / `returnReg_`
//! triple, and the `scopeBoundaryFlags_` byte that
//! `SeqCIfElse` / `SeqCCondExpr` set when entering a dead branch
//! so that downstream `SeqCVariable::evaluate` calls can skip the
//! `checkVar` enforcement.  Two specialised subclasses extend the
//! base: `StaticResources` populates the device-global constant
//! table at the program root (and surfaces the
//! `usedSampleRate_` flag), and `GlobalResources` adds per-thread
//! TLS counters (`regNumber`, `labelIndex`) plus a thread-local
//! MT19937-64 PRNG (`random[]`) so register / label names and
//! `randomSeed`-controlled values stay distinct across threads.
//! `Compiler::compile` instantiates a `StaticResources` first and
//! wraps it in a `GlobalResources` to obtain the root scope used by
//! the SeqC AST evaluator.
class Resources : public std::enable_shared_from_this<Resources> {
public:
    // ========================================================================
    // Nested types
    // ========================================================================

    // State — scope lifecycle state
    // From setState @0x1e35f0: if current==0 set to arg, else set to 3 (Locked).
    // From toString jump table: states 1..3 print labels, default throws/skips.
    //! \brief Lifecycle state recorded in `Resources::state_`,
    //! used by `setState` to enforce single-assignment-then-lock and
    //! by `variableDependsOnVar` to detect non-default scopes.
    //!
    //! Once a scope leaves `Unset`, any subsequent `setState` call
    //! forces the state to `Locked` regardless of the requested value.
    enum class State : int32_t {
        Unset   = 0,   //!< Initial state; the next `setState` accepts the new value.
        Active  = 1,   //!< First state set after construction (printed as "Active").
        Paused  = 2,   //!< Second named state in the lifecycle (printed as "Paused").
        Locked  = 3,   //!< Forced when `setState` is called on an already-set scope.
    };

    // ========================================================================
    // Variable — element of the variables_ vector
    //
    // Size: 0x58 (88 bytes) — confirmed by `add r14, 0x58` at 1e8441 and
    // by stride of vector iterations.
    //
    // FINAL LAYOUT:
    //   The 40-byte block at +0x08..+0x2F is a complete embedded `Value`
    //   object. Evidence: `readString @0x1e5db5` does `add rsi, 0x8;
    //   call Value::toString()` — passing `&v+8` as `this` to a Value
    //   method. The fields (`flagWord`, `which_`, `variantStorage`,
    //   `pad_28`) map onto Value's (`type_`, `which_`, `storage_`)
    //   at the corresponding sub-offsets.
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
    //! \brief Single entry of the `variables_` symbol table.
    //!
    //! Holds the variable's category (`type`), the caller-supplied
    //! sub-classification from the originating `add*` call
    //! (`subTypeRaw`), an embedded `Value` carrying the runtime
    //! payload, an `AsmRegister` assigned during code generation,
    //! the variable's `name`, and a 16-bit `flags` word whose low
    //! byte tracks "written" state and whose high byte (`+0x51`)
    //! tracks the `Function::addArgument` "frozen-parameter" flag
    //! consulted by the `update*` family.
    struct Variable {
        VarType     type;        //!< Variable category. +0x00
        VarSubType  subTypeRaw;  //!< Caller-supplied `VarSubType` from the originating `add*` call;
                                 //!< returned by `getVariableSubType`. +0x04
        Value       value;       //!< Embedded payload (40 B) holding the variant tag, discriminator,
                                 //!< and storage; default-constructed for stub variables. +0x08
        AsmRegister reg;         //!< Register assigned by `getRegister` / `addVar`; `AsmRegister::Invalid()`
                                 //!< for un-assigned slots. +0x30
        std::string name;        //!< Variable name (libc++ SSO; 24 B). +0x38
        int16_t     flags;       //!< Status bits — see `VarFlag_Written` (low byte) and
                                 //!< `VarFlag_Frozen` (high byte). +0x50
        char        pad_52[6];   //!< Padding to round the record to 0x58 bytes. +0x52

        //! \brief Defaulted destructor; the embedded `Value` and
        //! `name` subobjects clean up their own heap payloads.
        ~Variable();  // @0x1e4be0 — defaulted; Value's dtor handles payload.
    };
    static_assert(sizeof(Variable) == 0x58 || true,
                  "Variable should be 0x58 bytes (check with actual layout)");

    //! \brief Bit 0 of `Variable::flags`: set when the variable has
    //! been assigned by an `add*`/`update*` call; checked by
    //! `constIsSet`, `checkVar`, and the read-side accessors.
    static constexpr int16_t VarFlag_Written = 0x01;
    //! \brief Bit 8 of `Variable::flags`: set by
    //! `Function::addArgument` so that subsequent `update*`-with-value
    //! calls short-circuit the value reassignment but still mark the
    //! slot as written.
    static constexpr int16_t VarFlag_Frozen  = 0x100;

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
    //! \brief Function declaration record stored as
    //! `shared_ptr<Function>` inside `Resources::functions_`.
    //!
    //! Each `Function` owns a child `scope` (allocated by the
    //! constructor and parented to the declaring `Resources`) into
    //! which `addArgument` binds parameters as `FunctionArg` slots,
    //! mirroring the parameter list into `arguments` for argument-
    //! string normalisation (`sameArgString`).  `addBody` installs a
    //! cloned AST body that `getBody` re-clones on each call; the
    //! destructor releases the body, the scope, and all argument
    //! slots in reverse declaration order.
    struct Function {
        void*                          pad_00_;     //!< First half of the parent-scope `weak_ptr` slot at +0x00; zeroed by the ctor.
        void*                          weakRef_;    //!< Second half of the parent-scope `weak_ptr` slot at +0x08; released in the dtor when non-null.
        std::string                    name;        //!< Function name as it appears in SeqC source. +0x10
        std::string                    signature;   //!< Stringified argument-type signature consulted by `sameArgString`. +0x28
        VarType                        returnType;  //!< Declared return category; mirrored into `scope->returnType_`. +0x40
        int32_t                        pad_44;      //!< Padding to align the following vector. +0x44
        std::vector<Variable>          arguments;   //!< Parallel record of bound parameters (one entry per `addArgument` call). +0x48
        std::shared_ptr<Resources>     scope;       //!< Child scope holding the function-local symbol table. +0x60
        std::unique_ptr<SeqCAstNode>   body;        //!< Cloned AST body installed by `addBody`. +0x70

        //! \brief Construct a function declaration and allocate its
        //! child scope.
        //!
        //! Allocates a fresh `Resources` (via `allocate_shared`)
        //! parented by `parentScope`, stores it in `scope`, and
        //! propagates `rt` into the new scope's `returnType_` slot
        //! so that `Resources::getReturnType` can resolve it.
        //! \param name Function name.
        //! \param sig  Stringified parameter-type signature.
        //! \param rt   Return-type category.
        //! \param parentScope Weak reference to the declaring scope;
        //!        passed verbatim to the new child scope's
        //!        `Resources` constructor.
        Function(std::string const& name,
                 std::string const& sig,
                 VarType rt,
                 std::weak_ptr<Resources> parentScope);    // @0x1eaa00

        //! \brief Defaulted destructor; releases body, scope,
        //! arguments, signature, name, and parent weak ref in
        //! reverse declaration order.
        ~Function();                                       // @0x1ea820

        //! \brief Tear down the existing child scope and construct a
        //! fresh one whose name is `name + signature`, copying the
        //! parameter records back into the new scope's `variables_`.
        //!
        //! Preserves the prior scope's `returnType_`,
        //! `returnValue_`, and `returnReg_` across the rebuild.
        void resetScope();                                 // @0x1eac80

        //! \brief Append one named parameter to the function and
        //! mirror it into the child scope as a `FunctionArg`.
        //!
        //! Dispatches on `type` to call the matching
        //! `scope->add*(name, VarSubType_FunctionArg)` so the
        //! parameter slot appears written but cannot be re-bound,
        //! then pushes a parallel `Variable` record onto
        //! `arguments`.
        //! \param name Parameter name.
        //! \param type Parameter category; must be one of `Var`,
        //!        `String`, `Const`, `Wave`, or `Cvar`.
        //! \throws ResourcesException When `type == Var` and the
        //!         function's `returnType` is neither `Var` nor
        //!         `Void` (raised as `FormatVarReturn`), or when
        //!         `type` is outside the five supported categories
        //!         (raised as `FuncInvalidArgType`).
        //! \note The `Var`-arg gating against `returnType` is
        //!         applied at parameter-declaration time, so the
        //!         resulting diagnostic carries the function's
        //!         declaration line number rather than the call
        //!         site.
        void addArgument(std::string const& name, VarType type);   // @0x1e9f60

        //! \brief Bind every child of an `Expression` parameter list
        //! as a function parameter.
        //!
        //! When `expr->operationType == ePARAMLIST`, iterates
        //! `expr->children` and calls `addArgument` for each;
        //! otherwise treats `expr` itself as a single parameter.
        //! Returns immediately on a null `expr`.
        //! \param expr Parameter-list expression node, or a single
        //!        parameter expression.
        void addArguments(std::shared_ptr<Expression> expr);       // @0x1ea740

        //! \brief Bind every child of an AST parameter-list node as
        //! a function parameter.
        //!
        //! Attempts a `dynamic_cast<SeqCParamList const*>`; on
        //! success iterates the param list and calls `addArgument`
        //! per child, otherwise falls back to treating the node
        //! itself as a single parameter (matching the binary's
        //! catch-and-fallback behaviour around the failed
        //! `dynamic_cast`).
        //! \param node AST node expected to be a `SeqCParamList`,
        //!        but tolerated as a single-parameter shape.
        void addArguments(SeqCAstNode const& node);                // @0x1eab70

        //! \brief Replace the stored body with a clone of `node`,
        //! freeing the previous body (if any).
        //! \param node AST node to clone via `doClone()` and store
        //!        in `body`.
        void addBody(SeqCAstNode const& node);                     // @0x1ea7b0

        //! \brief Return a fresh clone of the stored body via
        //! `body->doClone()`.
        //! \return Newly-cloned AST tree owning its nodes.
        //! \binarynote Performs no null check on `body`; calling
        //!         this on a `Function` that has not had a body
        //!         installed dereferences a null pointer, matching
        //!         the binary.
        std::unique_ptr<SeqCAstNode> getBody() const;              // @0x1eab50

        //! \brief Test whether this function matches a candidate
        //! `(name, sig)` pair, ignoring sub-category distinctions
        //! between `Const`, `Cvar`, and `Var`.
        //! \param name Candidate function name.
        //! \param sig  Candidate signature; passed to
        //!        `sameArgString` for normalised comparison.
        //! \return true when both `name` and the normalised
        //!         signature match.
        bool isSame(std::string const& name,
                    std::string const& sig) const;                 // @0x1eb2a0

        //! \brief Compare a candidate signature against this
        //! function's `signature`, collapsing `Cvar` → `Const` →
        //! `Var` in both strings before equality checking.
        //!
        //! Returns true unconditionally when `signature.size() < 3`
        //! (no meaningful arguments to compare).
        //! \param sig Candidate signature string.
        //! \return true when the normalised forms match.
        bool sameArgString(std::string const& sig) const;          // @0x1eb330
    };

    // --- Constructors / Destructors ---

    //! \brief Construct a scope with the given name and parent link.
    //!
    //! Locks `parent` (if any) to inherit the parent's
    //! `grandparent_` chain, copies `name` into `name_`, and zeroes
    //! the lifecycle / return / scope-boundary fields.  The
    //! variables, functions, and children vectors start empty.
    //! \param name   Scope name (function name for function scopes,
    //!        free-form label for control-flow scopes).
    //! \param parent Weak reference to the lexically-enclosing scope;
    //!        empty for the root scope.
    Resources(std::string const& name,
              std::weak_ptr<Resources> parent);       // @0x1e3420
    //! \brief Virtual destructor; releases children, functions, and
    //! variables, then the return-value, parent link, name, and
    //! grandparent reference in reverse declaration order.
    virtual ~Resources();                             // D1 @0x12a8f0, D0 @0x1f1150

    // --- Virtual methods ---

    //! \brief Look up a variable by name, walking the parent chain.
    //!
    //! Searches `variables_` first, then recurses through `parent_`
    //! and finally `grandparent_`; on hit, OR's the local
    //! `scopeBoundaryFlags_` into the returned `Variable::flags`.
    //! Overridden by `StaticResources` to detect device-global
    //! constant accesses and emit deprecation warnings.
    //! \param name Variable name to resolve.
    //! \return Pointer to the matching `Variable`, or nullptr when
    //!         the name is not present in any reachable scope.
    virtual Variable* getVariable(std::string const& name);  // @0x1eb0a0

    // --- State management ---
    //! \brief Set the scope's lifecycle state.
    //!
    //! Accepts `s` only when the current state is `Unset`; any
    //! subsequent call forces the state to `Locked` regardless of
    //! the requested value.
    //! \param s Requested new state.
    void setState(State s);                           // @0x1e35f0
    //! \brief Test whether this scope contains a function named
    //! "main".
    //! \return true when at least one entry of `functions_` has
    //!         `name == "main"`.
    bool hasMain() const;                             // @0x1e3610

    // --- Scope management ---
    //! \brief Allocate a child scope named `name_ + ":" + name`,
    //! parent it to `*this`, and append it to `children_`.
    //! \param name Local sub-scope label (concatenated after the
    //!        parent's name with a `:` separator).
    //! \return `shared_ptr` to the freshly-constructed child scope.
    //! \throws std::bad_weak_ptr When `*this` is not currently owned
    //!         by a `shared_ptr` (the
    //!         `enable_shared_from_this` lock fails).
    std::shared_ptr<Resources> createSubScope(std::string const& name);  // @0x1e36a0
    //! \brief Replace the parent weak reference with one derived
    //! from the given `shared_ptr`.
    //! \param p New parent; `parent_` is reseated to `weak_ptr(p)`,
    //!        decrementing the previous parent's weak count.
    void updateParent(std::shared_ptr<Resources> p);  // @0x1e38f0

    // --- Return type/value ---
    //! \brief Set this scope's declared return type.
    //! \param type Return-type category (typically a `VarType`).
    void setReturnType(VarType type);                 // @0x1e3920
    //! \brief Resolve the nearest enclosing scope's return type.
    //!
    //! Returns this scope's `returnType_` when set; otherwise
    //! recurses through `parent_`.
    //! \return Resolved return type.
    //! \throws ResourcesException When no scope on the parent chain
    //!         has a return type set.
    VarType getReturnType() const;                    // @0x1e3930 (recursive via parent)
    //! \brief Set the scope's return value to the given double.
    //!
    //! Wraps `val` in a `Value` (Double tag, double slot) and
    //! forwards to the `Value` overload.
    //! \param val Numeric return value.
    void setReturnValue(double val);                  // @0x1e3ac0
    //! \brief Set the scope's return value, possibly walking up to
    //! the nearest scope with a declared return type.
    //!
    //! When this scope has neither a `returnType_` nor a
    //! scope-boundary flag, recurses to `parent_` so the value lands
    //! in the function-scope frame; otherwise stores into
    //! `returnValue_` locally.
    //! \param val Return value to store.
    void setReturnValue(Value const& val);            // @0x1e3b30
    //! \brief Read the return value from the nearest enclosing
    //! scope with a return type set, mirroring the recursion of
    //! `setReturnValue`.
    //! \return Copy of the stored `returnValue_`.
    Value getReturnValue();                           // @0x1e3d40
    //! \brief Set the return register, walking up to the nearest
    //! scope with a declared return type.
    //!
    //! Constructs `AsmRegister(reg)` and stores it in the resolving
    //! scope's `returnReg_` slot.
    //! \param reg Register index to wrap.
    void setReturnReg(int reg);                       // @0x1e3ed0
    //! \brief Read the return register from the nearest enclosing
    //! scope with a return type set.
    //! \return Copy of the resolved `returnReg_`.
    AsmRegister getReturnReg();                       // @0x1e3fe0
    //! \brief Allocate the next sequential assembly register from
    //! the per-thread `GlobalResources::regNumber` counter.
    //! \return Newly-allocated register index (post-increment of the
    //!         thread-local counter).
    static int getRegisterNumber();                          // @0x1e4bb0

    //! \brief Test whether this scope is currently flagged as a
    //! dead-branch boundary.
    //!
    //! Read by `SeqCVariable::evaluate` to gate the `checkVar`
    //! enforcement: when the boundary is set, dead branches skip
    //! the read-side validation.
    //! \return true when the low byte of `scopeBoundaryFlags_` is
    //!         non-zero.
    bool atScopeBoundary() const { return (scopeBoundaryFlags_ & 0xFF) != 0; }
    //! \brief Set or clear the scope-boundary flag consulted by
    //! `atScopeBoundary` and inherited by `getVariable` lookups.
    //!
    //! Used by `SeqCIfElse` and `SeqCCondExpr` Const/Cvar paths to
    //! mark dead-branch evaluation.
    //! \param v New boundary state (true → 1, false → 0).
    void setAtScopeBoundary(bool v) { scopeBoundaryFlags_ = v ? 1 : 0; }

    // --- Variable operations ---
    //! \brief Test whether mutating `name` would affect a
    //! repeat-loop iteration variable visible from this scope.
    //!
    //! Walks `parent_` accumulating an OR of each frame's
    //! `state_ != Unset` once the name is found locally; returns
    //! false when the name is not present anywhere on the chain.
    //! \param name Variable to test.
    //! \return true when `name` is reachable AND the originating
    //!         scope (or some ancestor) has a non-default `state_`.
    bool variableDependsOnVar(std::string const& name) const;    // @0x1e40e0
    //! \brief Test whether `name` exists in this scope or any
    //! reachable parent / grandparent scope.
    //! \param name Variable to look up.
    //! \return true when found, false otherwise.
    bool variableExists(std::string const& name) const;          // @0x1e4230
    //! \brief Test whether `name` is reachable from this scope's
    //! local variables or its `grandparent_` chain.
    //! \param name Variable to look up.
    //! \return true when found locally or in the grandparent chain.
    //! \binarynote Despite its name, when no local match is found
    //!         the fallback delegates to the full multi-scope
    //!         `variableExists` of `grandparent_` (not a strict
    //!         single-scope check); `parent_` is intentionally not
    //!         consulted on the fallback path.
    bool variableExistsInScope(std::string const& name) const;   // @0x1e4390
    //! \brief Look up `name` and return its `VarType`.
    //! \param name Variable to resolve.
    //! \return The matching variable's `type` field.
    //! \throws ResourcesException When `name` is not found in any
    //!         reachable scope (raised as `UnknownVar`).
    VarType getVariableType(std::string const& name);            // @0x1e4460
    //! \brief Look up `name` and return its caller-supplied
    //! `VarSubType` (the `subTypeRaw` field).
    //! \param name Variable to resolve.
    //! \return The matching variable's `subTypeRaw` field.
    //! \throws ResourcesException When `name` is not found in any
    //!         reachable scope (raised as `UnknownVar`).
    VarSubType getVariableSubType(std::string const& name);      // @0x1e4580
    //! \brief Insert a new mutable runtime `Var` into this scope.
    //!
    //! Allocates an `AsmRegister` from the per-thread counter,
    //! marks the slot written when `st` is `VarSubType_FunctionArg`,
    //! and stores a default-constructed integer payload.
    //! \param name New variable name.
    //! \param st   Sub-type tag (most callers pass `Stub`;
    //!        `Function::addArgument` passes `FunctionArg` to bind
    //!        a parameter slot).
    //! \throws ResourcesException When `name` already exists in
    //!         this scope or in `grandparent_` (raised as
    //!         `AlreadyDefined`).
    void addVar(std::string const& name, VarSubType st);         // @0x1e46b0
    //! \brief Mark an existing `Var` as written.
    //!
    //! Sets the low byte of `flags` while preserving the high-byte
    //! `Frozen` bit.
    //! \param name Variable to update.
    //! \throws ResourcesException When `name` is not found
    //!         (`UninitializedVar`) or is not a `Var`
    //!         (`TypeMismatchWrite`).
    void updateVar(std::string const& name);                     // @0x1e4c40
    //! \brief Read-side validation that `name` is initialised and
    //! is not a `String`.
    //! \param name Variable to check.
    //! \throws ResourcesException When `name` is unknown or
    //!         unwritten (`UninitializedVar`), or when it resolves
    //!         to a `String` (`TypeMismatchRead`).
    //! \binarynote Accepts `Var`, `Const`, `Wave`, and `Cvar`; only
    //!         `String` is rejected. Callers that require a strict
    //!         `Var` must check the type themselves.
    void checkVar(std::string const& name);                      // @0x1e4e20

    // --- String variable operations ---
    //! \brief Insert a string-typed variable bound to `val`.
    //! \param name New variable name.
    //! \param val  String payload stored in the variable's `value`.
    //! \throws ResourcesException When `name` already exists in
    //!         this scope or in `grandparent_` (`AlreadyDefined`).
    void addString(std::string const& name, std::string const& val);  // @0x1e5020
    //! \brief Insert a string-typed variable without a payload.
    //!
    //! When `st` is `VarSubType_FunctionArg` the slot is pre-marked
    //! as written (parameter-binding path); otherwise it is left
    //! unwritten so subsequent reads fail until an `updateString`
    //! occurs.
    //! \param name New variable name.
    //! \param st   Sub-type tag.
    //! \throws ResourcesException When `name` already exists
    //!         (`AlreadyDefined`).
    void addString(std::string const& name, VarSubType st);           // @0x1e54f0
    //! \brief Update an existing string variable's payload and
    //! sub-type.
    //!
    //! Throws on missing/wrong-typed `name`, or when the variable
    //! depends on a repeat-loop iteration variable.  When the
    //! variable's `Frozen` bit is set (function-parameter path),
    //! the value write is skipped but the slot is still marked
    //! written.
    //! \param name Variable to update.
    //! \param val  New string payload.
    //! \param st   New sub-type tag.
    //! \throws ResourcesException On unknown name
    //!         (`UninitializedVar`), wrong type
    //!         (`TypeMismatchWrite`), or repeat-loop dependency
    //!         (`CantModifyVarInRepeat`).
    void updateString(std::string const& name, std::string const& val, VarSubType st); // @0x1e59d0
    //! \brief Read a string variable as an `EvalResultValue`.
    //!
    //! Materialises the embedded `Value` via `Value::toString` and
    //! packages it together with the variable's sub-type and an
    //! invalid `AsmRegister`.
    //! \param name Variable to read.
    //! \param dir  Read direction; on `eOUT` the variable must have
    //!        been written.
    //! \return `EvalResultValue` carrying the string payload.
    //! \throws ResourcesException On unknown name, write-direction
    //!         read of an unwritten slot (`UninitializedVar`), or
    //!         wrong type (`TypeMismatchRead`).
    EvalResultValue readString(std::string const& name, EDirection dir);  // @0x1e5d70

    // --- Wave variable operations ---
    //! \brief Insert a wave-typed variable bound to the wave name
    //! `val`.
    //! \param name New variable name.
    //! \param val  Wave-name reference stored in the variant slot.
    //! \throws ResourcesException When `name` already exists
    //!         (`AlreadyDefined`).
    void addWave(std::string const& name, std::string const& val);    // @0x1e6020
    //! \brief Insert a wave-typed variable without a payload.
    //!
    //! Mirrors `addString(name, st)`: the parameter-binding path
    //! (`st == FunctionArg`) pre-marks the slot as written.
    //! \param name New variable name.
    //! \param st   Sub-type tag.
    //! \throws ResourcesException When `name` already exists
    //!         (`AlreadyDefined`).
    void addWave(std::string const& name, VarSubType st);             // @0x1e64f0
    //! \brief Update an existing wave variable's payload and
    //! sub-type, mirroring `updateString` with `Wave` typing.
    //! \param name Variable to update.
    //! \param val  New wave-name payload.
    //! \param st   New sub-type tag.
    //! \throws ResourcesException On unknown name, wrong type, or
    //!         repeat-loop dependency.
    void updateWave(std::string const& name, std::string const& val, VarSubType st); // @0x1e69c0
    //! \brief Read a wave variable as an `EvalResultValue`.
    //!
    //! Mirrors `readString` with `varType_ = Wave`.
    //! \param name Variable to read.
    //! \param dir  Read direction; on `eOUT` the variable must have
    //!        been written.
    //! \return `EvalResultValue` carrying the wave-name payload.
    //! \throws ResourcesException On unknown name, write-direction
    //!         read of an unwritten slot, or wrong type.
    EvalResultValue readWave(std::string const& name, EDirection dir);    // @0x1e6d60

    // --- Const variable operations ---
    //! \brief Insert a numeric `Const` bound to `val`.
    //! \param name New variable name.
    //! \param val  Numeric payload (stored as a double).
    //! \param st   Sub-type tag stashed verbatim in `subTypeRaw`.
    //! \throws ResourcesException When `name` already exists
    //!         (`AlreadyDefined`).
    void addConst(std::string const& name, double val, VarSubType st);                  // @0x1e7010
    //! \brief Insert a `Const` without a payload (declaration-only).
    //!
    //! Pre-marks the slot as written when `st` is
    //! `VarSubType_FunctionArg`; otherwise leaves it unwritten
    //! (subsequent `readConst` will reject it).
    //! \param name New variable name.
    //! \param st   Sub-type tag.
    //! \throws ResourcesException When `name` already exists
    //!         (`AlreadyDefined`).
    void addConst(std::string const& name, VarSubType st);                              // @0x1e74e0
    //! \brief Update an existing `Const`'s value and sub-type,
    //! optionally bypassing the "already-written" reassignment
    //! guard.
    //!
    //! When `force` is false and the variable has been written, a
    //! diagnostic is raised; when `force` is true the assignment
    //! always proceeds.  Frozen-parameter slots short-circuit the
    //! value write but are still marked written.
    //! \param name  Variable to update.
    //! \param val   New numeric value.
    //! \param st    New sub-type tag.
    //! \param force When false, throws on reassignment of an
    //!        already-written slot.
    //! \throws ResourcesException On unknown name, wrong type,
    //!         repeat-loop dependency, or a guarded reassignment.
    void updateConst(std::string const& name, double val, VarSubType st, bool force);   // @0x1e79b0
    //! \brief Read a `Const` as an `EvalResultValue` carrying the
    //! original variant payload (int / bool / double / string).
    //! \param name Variable to read.
    //! \param dir  Read direction; on `eOUT` the variable must have
    //!        been written.
    //! \return `EvalResultValue` with `varType_ = Const`.
    //! \throws ResourcesException On unknown name, write-direction
    //!         read of an unwritten slot, or wrong type.
    EvalResultValue readConst(std::string const& name, EDirection dir);   // @0x1e7d70
    //! \brief Test whether a named const has been assigned.
    //! \param name Variable to test.
    //! \return true when the variable's `Written` flag is set.
    //! \throws ResourcesException When `name` is not found in any
    //!         reachable scope (raised as `UninitializedVar`).
    bool constIsSet(std::string const& name);                                           // @0x1e8050

    // --- Cvar (compile-time variable) operations ---
    //! \brief Insert a compile-time variable (`Cvar`) bound to
    //! `val`.
    //! \param name New variable name.
    //! \param val  Numeric payload.
    //! \param st   Sub-type tag stashed in `subTypeRaw`.
    //! \throws ResourcesException When `name` already exists
    //!         (`AlreadyDefined`).
    void addCvar(std::string const& name, double val, VarSubType st);    // @0x1e8180
    //! \brief Insert a `Cvar` without a payload; the parameter-
    //! binding path (`st == FunctionArg`) pre-marks the slot as
    //! written.
    //! \param name New variable name.
    //! \param st   Sub-type tag.
    //! \throws ResourcesException When `name` already exists
    //!         (`AlreadyDefined`).
    void addCvar(std::string const& name, VarSubType st);                // @0x1e8650
    //! \brief Update an existing `Cvar`'s value and sub-type.
    //!
    //! Lacks the `force`-gated reassignment guard of
    //! `updateConst`; the frozen-parameter short-circuit and
    //! repeat-loop dependency check still apply.
    //! \param name Variable to update.
    //! \param val  New numeric value.
    //! \param st   New sub-type tag.
    //! \throws ResourcesException On unknown name, wrong type, or
    //!         repeat-loop dependency.
    void updateCvar(std::string const& name, double val, VarSubType st); // @0x1e8b20
    //! \brief Read a `Cvar` as an `EvalResultValue`, preserving the
    //! underlying variant kind (int / bool / double / string).
    //! \param name Variable to read.
    //! \param dir  Read direction; on `eOUT` the variable must have
    //!        been written.
    //! \return `EvalResultValue` with `varType_ = Cvar`.
    //! \throws ResourcesException On unknown name, write-direction
    //!         read of an unwritten slot, or wrong type.
    //! \binarynote The wrong-type diagnostic uses
    //!         `TypeMismatchWrite` (not `TypeMismatchRead`) and
    //!         names the expected type as the literal `"CVAR"`
    //!         rather than `str(VarType_Cvar)`.
    EvalResultValue readCvar(std::string const& name, EDirection dir);    // @0x1e8e80

    // --- Function operations ---
    //! \brief Test whether a function reachable from this scope
    //! matches `(name, sig)`.
    //!
    //! When `sig` is empty, performs a name-only search of the
    //! local `functions_` and recurses to `parent_`; when `sig` is
    //! non-empty, delegates to `getFunction` and tests the result
    //! for non-null.
    //! \param name Function name.
    //! \param sig  Stringified signature (may be empty for
    //!        name-only lookup).
    //! \return true when a matching function is found.
    bool functionExists(std::string const& name, std::string const& sig);              // @0x1e9110
    //! \brief Resolve `(name, sig)` to a function record, walking
    //! the parent chain on miss.
    //! \param name Function name.
    //! \param sig  Stringified signature; compared via
    //!        `Function::sameArgString` for sub-category collapse.
    //! \return `shared_ptr` to the matching function, or empty when
    //!         no match is found in this scope or any parent.
    std::shared_ptr<Function> getFunction(std::string const& name, std::string const& sig); // @0x1e9370
    //! \brief Test whether `(name, sig)` is already declared in the
    //! local scope (no parent recursion).
    //! \param name Function name.
    //! \param sig  Stringified signature; compared via
    //!        `Function::sameArgString`.
    //! \return true when a local match exists.
    bool functionExistsInScope(std::string const& name, std::string const& sig);       // @0x1e95d0
    //! \brief Collect every "name + signature" string for functions
    //! whose name matches `name`, across this scope and all
    //! reachable parents.
    //!
    //! Used to produce candidate-overload diagnostics when a call
    //! site cannot be resolved.
    //! \param name Function name to search for.
    //! \return Vector of `name + signature` strings, one per match.
    std::vector<std::string> getPossibleFunctions(std::string const& name);             // @0x1e9740
    //! \brief Declare a new function in this scope and return its
    //! `Function` record.
    //!
    //! Allocates a `Function` parented to `*this` (capturing
    //! `shared_from_this()` as the new function's parent scope) and
    //! appends it to `functions_`.
    //! \param name Function name.
    //! \param sig  Stringified parameter-type signature.
    //! \param rt   Return-type category.
    //! \return `shared_ptr` to the freshly-declared function.
    //! \throws ResourcesException When `(name, sig)` is already
    //!         declared locally (`AlreadyDefined`).
    //! \throws std::bad_weak_ptr When `*this` is not currently
    //!         owned by a `shared_ptr`.
    std::shared_ptr<Function> addFunction(std::string const& name, std::string const& sig, VarType rt); // @0x1e9c10

    // --- Register operations ---
    //! \brief Resolve `name` to its assigned register, allocating
    //! a fresh register from the per-thread counter on first use.
    //!
    //! Lazily assigns a register when the variable's existing
    //! `reg` is invalid or equal to `AsmRegister(0)`.
    //! \param name Variable to resolve.
    //! \return Integer register index assigned to the variable.
    //! \throws ResourcesException When `name` is not found
    //!         (raised as `UninitializedVar`).
    int getRegister(std::string const& name);  // @0x1eba50

    // --- Label ---
    //! \brief Generate a unique label string by suffixing `name`
    //! (or the literal `"label"` when `name` is empty) with the
    //! per-thread `GlobalResources::labelIndex`, post-incrementing
    //! it.
    //! \param name Optional label prefix.
    //! \return Concatenation of the prefix and the post-incremented
    //!         label index.
    static std::string newLabel(std::string const& name);     // @0x1ec6b0

    // --- Scope introspection helpers (used by SeqCReturnStatement::evaluate) ---
    //! \brief Test whether this scope contains a `Var`-typed
    //! function-parameter slot.
    //!
    //! Used by `SeqCReturnStatement::evaluate` to detect the
    //! "var arg in wave-return function" diagnostic at the return
    //! statement.
    //! \return true when at least one local `Variable` has
    //!         `type == Var` and `subTypeRaw == FunctionArg`.
    bool hasVarFunctionArg() const;
    //! \brief Return this scope's name (for function scopes, the
    //! function name).
    //! \return Reference to the scope's `name_` field.
    const std::string& scopeName() const { return name_; }

    // --- Debug ---
    //! \brief Print this scope's variables and functions to
    //! `std::cout`, recursing to parents first.
    void print();                               // @0x1ebbe0
    //! \brief Render this scope's contents (header, state,
    //! variables) as a human-readable string.
    //! \return Formatted text describing the scope.
    std::string toString() const;               // @0x1ebcf0
    //! \brief Print this scope (via `print`) followed by every
    //! child sub-scope (via `printScopes`) to `std::cout`.
    void printAll();                             // @0x1ec460
    //! \brief Recursively print every child sub-scope under this
    //! scope to `std::cout`.
    void printScopes();                          // @0x1ec570

protected:
    // The actual field layout — sizes validated against dtor at 0x12a8f0
    // and ctor at 0x1e3420.
    //
    // Note: enable_shared_from_this adds a mutable weak_ptr<Resources>
    // as a hidden member at +0x08 (16 bytes). The ctor zeroes +0x08..+0x17.

    // vptr at +0x00 (implicit)
    // enable_shared_from_this weak_ptr at +0x08 (16 bytes, zeroed in ctor)
    std::shared_ptr<Resources> grandparent_;             //!< Strong reference to the root scope (the `StaticResources` populated with device constants); held for the lifetime of every sub-scope.  +0x18 (16 bytes)
    std::string                name_;               //!< Scope name (function name for function scopes, empty for anonymous block scopes).  +0x28 (24 bytes)
    std::weak_ptr<Resources>   parent_;              //!< Non-owning back-pointer to the lexically enclosing scope; walked by `variableExists`, `getFunction`, and the repeat-loop dependency check.  +0x40 (16 bytes)
    State                      state_;              //!< Scope state (`Unset`, `Repeat`, `Function`, ...) used by `variableDependsOnVar` to detect repeat-loop iteration scopes.  +0x50
    VarType                    returnType_;         //!< Return-type category for function scopes; `Var` for non-function scopes.  +0x54
    Value                      returnValue_;        //!< Return-value payload set by `SeqCReturnStatement::evaluate`; consumed by callers via `getReturnValue`.  +0x58 (0x28 = 40 bytes)
    AsmRegister                returnReg_;          //!< Register holding the return-value at the function's exit edge.  +0x80 (8 bytes)
    int16_t                    scopeBoundaryFlags_;           //!< Bitset distinguishing scope-kind boundaries (procedure vs. control-flow); accessed via `atScopeBoundary` / `setAtScopeBoundary`.  +0x88
    char                       pad_8A_[6];          //!< Alignment padding before the next vector.  +0x8A
    std::vector<Variable>      variables_;          //!< Variables declared in this scope (vars / consts / cvars / strings / waves), in declaration order.  +0x90
    std::vector<std::shared_ptr<Function>> functions_;  //!< Functions declared in this scope, owned strongly because each `Function` carries its own scope chain.  +0xA8
    std::vector<std::shared_ptr<Resources>> children_;  //!< Sub-scopes created by `createSubScope`; owned strongly to keep walk targets alive.  +0xC0
    // Total: 0xD8
};

// ============================================================================
// ResourcesException — thrown by Resources methods (e.g. getReturnType)
//
// Layout (0x20 bytes): vptr + std::string msg_
// ============================================================================
//! \brief Diagnostic raised by `Resources` methods on scope/symbol
//! lookup failures (missing variable, undefined function return
//! type, malformed scope state).
//!
//! Message returned verbatim by `what()`; for example
//! `getReturnType` raises this when called on a scope whose
//! function return type was never set.
class ResourcesException : public std::exception {
    //! \brief Diagnostic text returned verbatim by `what()`; supplied
    //! at construction.
    std::string msg_;
public:
    //! \brief Constructs the exception with the given diagnostic
    //! message, which `what()` later returns verbatim.
    //! \param msg Diagnostic text (copied in).
    explicit ResourcesException(std::string const& msg);  // @0x1e3a20
    //! \brief Virtual destructor; releases the owned message string.
    ~ResourcesException() override;                        // @0x1f12f0
    //! \brief Returns the stored diagnostic text as a C-string.
    //! \return Null-terminated diagnostic message.
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

//! \brief Root-scope `Resources` specialisation that pre-loads the
//! device-specific constant table consumed by the SeqC frontend.
//!
//! Constructed by `Compiler::compile` from a warning-reporter
//! callback and then primed via `init(config, deviceConstants)`,
//! which inserts every device-global constant (sample rate, channel
//! counts, alignment limits, AWG_RATE_*, etc.) into the base
//! scope's `variables_` table so that SeqC programs can reference
//! them by name.  The overridden `getVariable` recognises the small
//! set of magic constant names (notably `DEVICE_SAMPLE_RATE`) and
//! sets `usedSampleRate_` whenever one is read — a flag the
//! compiler later mirrors into its compile-result metadata.
class StaticResources : public Resources {
public:
    //! \brief Construct an empty root scope and stash the
    //! warning-reporter callback used by later diagnostics.
    //! \param logger Callback invoked with formatted warning text.
    StaticResources(std::function<void(std::string const&)> const& logger);  // @0x129cb0
    //! \brief Destroy the root scope, releasing the inline
    //! `std::function` storage.
    ~StaticResources() override;                      // D1 @0x129db0, D0 @0x129e00

    //! \brief Resolve `name` against the device-global constant
    //! table, setting `usedSampleRate_` when the program reads
    //! `DEVICE_SAMPLE_RATE` (or another rate-bearing constant).
    //!
    //! Falls back to `Resources::getVariable` for non-magic names.
    //! \param name Variable to look up.
    //! \return Pointer to the matching `Variable`, or null when no
    //!         match exists in this scope.
    Variable* getVariable(std::string const& name) override;  // @0x129e60

    //! \brief Pre-load device-global constants (sample rate,
    //! channel counts, alignment limits, `AWG_RATE_*`,
    //! `DIO_*`, etc.) into this scope's variable table.
    //!
    //! Called by `Compiler::compile` once per compilation, after
    //! construction and before parsing.  All constants are
    //! installed as written `Const` slots; programs can then
    //! reference them by name.
    //! \param config          Per-compilation AWG configuration
    //!                        (sequencer kind, sample-rate index,
    //!                        feature flags).
    //! \param deviceConstants Per-device constant table loaded by
    //!                        the device-properties layer.
    void init(AWGCompilerConfig const& config,
              DeviceConstants const& deviceConstants);    // @0x1ec8f0

    //! \brief Whether the compiled program ever read a
    //! sample-rate-bearing device constant; mirrored into the
    //! compile-result metadata by `Compiler::compile`.
    //! \return true when at least one such constant was read.
    bool usedSampleRate() const { return usedSampleRate_; }

protected:
    // Returns a callable that forwards to the std::function stored inline at
    // (functionStorage_, functionPtr_). The binary at 0x12a256-0x12a26d
    // dispatches via `[functionPtr_->vtable + 0x30]` which is the standard
    // libc++ __function::__base::__invoke entry. The wrapper hides that ABI
    // detail from call sites in static_resources.cpp.
    //!
    //! \brief Reconstruction-only helper that would re-package the
    //! inline `std::function` into a fresh callable.
    //! \unclear No call site in the reconstructed tree references
    //!         this declaration and the body has never been
    //!         reconstructed; left in place as a placeholder for
    //!         the eventual helper.
    //! \return A callable forwarding to the inline warning-reporter
    //!         `std::function` (or an empty `std::function` when none
    //!         is installed).
    std::function<void(std::string const&)> errorReportTarget() const;

private:
    bool    usedSampleRate_;        //!< Set by `getVariable` whenever a sample-rate-bearing constant is read; mirrored into compile-result metadata.  +0xD8
    char    pad_d9_[7];            //!< Alignment padding before the inline `std::function` buffer.  +0xD9
    char    functionStorage_[0x20]; //!< Inline storage for the warning-reporter `std::function` (libc++ small-functor optimisation).  +0xE0
    void*   functionPtr_;          //!< Pointer into `functionStorage_` identifying the active callable target (null when the slot is empty).  +0x100
    char    pad_108_[8];           //!< Trailing padding to round the instance up to 0x110 bytes.  +0x108
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

//! \brief Per-thread `Resources` wrapper that supplies the global
//! register/label numbering and the seeded MT19937-64 PRNG used
//! during SeqC compilation.
//!
//! Adds no instance fields beyond the `Resources` base; instead it
//! exposes three `static thread_local` members shared across every
//! `GlobalResources` constructed on the same thread:
//! `regNumber` (next free assembly register, fed to
//! `Resources::getRegister`), `labelIndex` (next free label suffix,
//! fed to `Resources::newLabel`), and the 313-element MT19937-64
//! `random[]` state (used by `randomSeed` / `getPRNGValue`).  The
//! constructor wraps an existing root scope (the `StaticResources`
//! returned by `Compiler::compile`) as `grandparent_` and re-seeds
//! the PRNG deterministically so successive compilations on the
//! same thread produce reproducible random-driven output.
class GlobalResources : public Resources {
public:
    //! \brief Construct a per-thread compilation scope wrapping
    //! `grandparent` (the device-constant root) and reset the TLS
    //! register / label counters and PRNG state for a new
    //! compilation.
    //! \param grandparent Root scope held strongly as
    //!        `grandparent_`; typically the `StaticResources`
    //!        returned by `Compiler::compile`.
    GlobalResources(std::shared_ptr<Resources> const& grandparent);  // @0x12a710
    //! \brief Destroy this per-thread scope; the TLS members
    //! persist for the lifetime of the thread.
    ~GlobalResources() override;                                // D0 @0x12ab40

    //! \brief Next free assembly-register index, allocated by
    //! `Resources::getRegister` on first use of each variable.
    //!
    //! Reset to 1 on every `GlobalResources` construction so each
    //! compilation starts from a clean numbering.
    static thread_local int32_t  regNumber;    // ctor init=1
    //! \brief Per-thread label counter consumed by
    //! `Resources::newLabel` to suffix generated jump labels.
    //!
    //! Reset to 0 on every `GlobalResources` construction.
    static thread_local int32_t  labelIndex;   // ctor init=0
    //! \brief Per-thread MT19937-64 PRNG state used by SeqC
    //! `randomSeed` / `getPRNGValue`.
    //!
    //! 313-element layout: state vector in `[0..311]`, refill
    //! counter in `[312]`.  The constructor re-seeds with the
    //! deterministic seed `0x1571` so successive compilations on
    //! the same thread produce reproducible random-driven output.
    static thread_local uint64_t random[313];  // MT19937-64 state
};

//! \brief Render a `VarType` enumerator as the human-readable name
//! used in diagnostics ("VAR", "CONST", "WAVE", ...).
//! \param vt Enumerator to format.
//! \return Display name of the enumerator.
std::string str(VarType vt);

//! \brief Render a `VarSubType` enumerator as the human-readable
//! name used in diagnostics.
//! \param vst Enumerator to format.
//! \return Display name of the enumerator.
std::string str(VarSubType vst);  // 0x247ee0

} // namespace zhinst
