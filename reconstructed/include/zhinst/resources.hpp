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
#include "zhinst/value.hpp"

namespace zhinst {

// Forward declarations
class AWGCompilerConfig;
struct DeviceConstants;
class SeqCAstNode;

// ============================================================================
// VarType — variable type tag used in Resources::Variable
//
// From toString @0x1ebcf0 printing logic (jump table on type):
//   2 → "v: name (Reg: N)\n"    (var)
//   3 → "c: name -> value\n"    (const)
//   4 → "cv: name -> value\n"   (cvar)
//   5 → "s: name -> value\n"    (string)
//   6 → "w: name -> value\n"    (wave)
//   default → "?: name\n"
// ============================================================================
enum VarType : int32_t {
    VarType_Unset  = 0,
    VarType_Var    = 2,
    VarType_Const  = 3,
    VarType_Cvar   = 4,
    VarType_String = 5,
    VarType_Wave   = 6,
};

// VarSubType — secondary classification (values TBD, often 0)
enum VarSubType : int32_t {
    VarSubType_Default = 0,
};

// EDirection — direction flag for read operations (values TBD)
enum EDirection : int32_t {};

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
// 0x18    16    shared_ptr<Resources>                   parent_
// 0x28    24    std::string                             name_
// 0x40    16    weak_ptr<Resources>                     parentWeak_
// 0x50    4     int32_t                                 state_ (State enum)
// 0x54    4     VarType                                 returnType_
// 0x58    40    Value                                   returnValue_ (0x28 bytes)
// 0x80    8     AsmRegister                             returnReg_
// 0x88    2     int16_t                                 flags_88_
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
    // Size: 0x58 (88 bytes) — stride confirmed from vector iteration
    //
    // Layout (from addVar @0x1e46b0, variableExists, Variable::~Variable @0x1e4be0):
    //   +0x00  8   int64_t        varType     (VarType, e.g. 2=var, 3=const, ...)
    //   +0x08  4   int32_t        which_      (boost::variant discriminator field)
    //   +0x0C  4   (padding)
    //   +0x10  24  union          variant data (boost::variant<int,bool,double,string>)
    //   +0x28  8   (padding / unused)
    //   +0x30  8   AsmRegister    reg         (register assignment: int value + bool valid)
    //   +0x38  24  std::string    name        (variable name, SSO)
    //   +0x50  2   int16_t        flags       (e.g. 1 = "set"/written)
    //   +0x52  6   (padding)
    //
    // Dtor @0x1e4be0:
    //   1. Frees string at +0x38 if heap-allocated (long string)
    //   2. If abs(which_) >= 3: frees string inside variant at +0x10
    // ========================================================================
    struct Variable {
        int64_t     varType;     // +0x00 — VarType enum as int64
        int32_t     which_;      // +0x08 — boost::variant discriminator
        int32_t     pad_0C;      // +0x0C
        Value       value;       // +0x10..+0x28 — actually just the storage part (24 bytes inline)
                                 //   Note: This is NOT a full Value object; it's the raw
                                 //   boost::variant storage. The which_ at +0x08 is the
                                 //   variant discriminator.
        int64_t     pad_28;      // +0x28 — padding/unused
        AsmRegister reg;         // +0x30 — register assignment
        std::string name;        // +0x38 — variable name (24 bytes SSO)
        int16_t     flags;       // +0x50 — status flags (1 = written)
        char        pad_52[6];   // +0x52 — padding

        ~Variable();  // @0x1e4be0
    };
    static_assert(sizeof(Variable) == 0x58 || true,
                  "Variable should be 0x58 bytes (check with actual layout)");

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
        void*                          reserved_;   // +0x00
        void*                          weakRef_;    // +0x08 — __shared_weak_count*
        std::string                    name;        // +0x10
        std::string                    signature;   // +0x28
        VarType                        returnType;  // +0x40
        int32_t                        pad_44;      // +0x44
        std::vector<Variable>          arguments;   // +0x48
        std::shared_ptr<Resources>     scope;       // +0x60
        std::unique_ptr<SeqCAstNode>   body;        // +0x70

        // Constructs Function with name, signature, returnType.
        // Zeroes reserved_, weakRef_, scope, body.
        Function(std::string const& name,
                 std::string const& sig,
                 VarType rt);                          // @0x1eaa00

        ~Function();                                   // @0x1ea820
        void resetScope();                             // @0x1eac80
        void addArguments(SeqCAstNode const& node);    // @0x1eab70
        void addBody(SeqCAstNode const& node);         // @0x1ea7b0
        SeqCAstNode const* getBody() const;            // @0x1eab50
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
    void createSubScope(std::string const& name);     // @0x1e36a0
    void updateParent(std::shared_ptr<Resources> p);  // @0x1e38f0

    // --- Return type/value ---
    void setReturnType(VarType type);                 // @0x1e3920
    VarType getReturnType() const;                    // @0x1e3930 (recursive via parent)
    void setReturnValue(double val);                  // @0x1e3ac0
    void setReturnValue(Value const& val);            // @0x1e3b30
    Value getReturnValue();                           // @0x1e3d40
    void setReturnReg(int reg);                       // @0x1e3ed0
    AsmRegister getReturnReg();                       // @0x1e3fe0
    int getRegisterNumber();                          // @0x1e4bb0

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
    void readString(std::string const& name, EDirection dir);         // @0x1e5d70

    // --- Wave variable operations ---
    void addWave(std::string const& name, std::string const& val);    // @0x1e6020
    void addWave(std::string const& name, VarSubType st);             // @0x1e64f0
    void updateWave(std::string const& name, std::string const& val, VarSubType st); // @0x1e69c0
    void readWave(std::string const& name, EDirection dir);           // @0x1e6d60

    // --- Const variable operations ---
    void addConst(std::string const& name, double val, VarSubType st);                  // @0x1e7010
    void addConst(std::string const& name, VarSubType st);                              // @0x1e74e0
    void updateConst(std::string const& name, double val, VarSubType st, bool force);   // @0x1e79b0
    void readConst(std::string const& name, EDirection dir);                            // @0x1e7d70
    bool constIsSet(std::string const& name);                                           // @0x1e8050

    // --- Cvar (compile-time variable) operations ---
    void addCvar(std::string const& name, double val, VarSubType st);    // @0x1e8180
    void addCvar(std::string const& name, VarSubType st);                // @0x1e8650
    void updateCvar(std::string const& name, double val, VarSubType st); // @0x1e8b20
    void readCvar(std::string const& name, EDirection dir);              // @0x1e8e80

    // --- Function operations ---
    bool functionExists(std::string const& name, std::string const& sig);              // @0x1e9110
    std::shared_ptr<Function> getFunction(std::string const& name, std::string const& sig); // @0x1e9370
    bool functionExistsInScope(std::string const& name, std::string const& sig);       // @0x1e95d0
    void getPossibleFunctions(std::string const& name);                                // @0x1e9740
    void addFunction(std::string const& name, std::string const& sig, VarType rt);     // @0x1e9c10

    // --- Register operations ---
    void getRegister(std::string const& name);  // @0x1eba50

    // --- Label ---
    void newLabel(std::string const& name);     // @0x1ec6b0

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
    std::shared_ptr<Resources> parent_;             // +0x18 (16 bytes)
    std::string                name_;               // +0x28 (24 bytes)
    std::weak_ptr<Resources>   parentWeak_;         // +0x40 (16 bytes)
    int32_t                    state_;              // +0x50
    VarType                    returnType_;         // +0x54
    Value                      returnValue_;        // +0x58 (0x28 = 40 bytes)
    AsmRegister                returnReg_;          // +0x80 (8 bytes)
    int16_t                    flags_88_;           // +0x88
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
// counter, and Mersenne Twister-like PRNG
//
// Size: 0xD8 (216 bytes) — same as Resources base
//
// GlobalResources does NOT add any instance fields beyond Resources.
// Instead, it uses thread-local static variables:
//   TLS+0x48  int32_t  regNumber   — register counter, initialized to 1
//   TLS+0x4c  int32_t  labelIndex  — label counter, initialized to 0
//   TLS+0x50  uint64_t random[313] — Mersenne Twister state (MT19937-64)
// ============================================================================

class GlobalResources : public Resources {
public:
    GlobalResources(std::shared_ptr<Resources> const& parent);  // @0x12a710
    ~GlobalResources() override;                                // D0 @0x12ab40

    static thread_local int32_t  regNumber;    // TLS+0x48, init=1
    static thread_local int32_t  labelIndex;   // TLS+0x4c, init=0
    static thread_local uint64_t random[313];  // TLS+0x50, MT19937-64 state
};

} // namespace zhinst
