// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Resources base class — all method implementations
// ============================================================================

#include "zhinst/resources.hpp"
#include "zhinst/seqc_ast_node.hpp"
#include "zhinst/error_messages.hpp"

#include <iostream>
#include <sstream>
#include <cstring>

namespace zhinst {

// ============================================================================
// ResourcesException — defined in error_messages.cpp to avoid ODR violations.
// ============================================================================

// ============================================================================
// Resources::Variable::~Variable — @0x1e4be0
//
// Disassembly (1e4be0..1e4c2e):
//   1. SSO check on name string at +0x38: if [rbx+0x38]&1, the string is
//      heap-allocated → call operator delete([rbx+0x48], cap&~1). The cap
//      qword sits at name+0x10 = +0x48 (libc++ long-form layout).
//   2. Load which_ from [rbx+0x10] (NOT +0x08 — confirms the embedded Value
//      lives at Variable+0x08, since Value::which_ is at Value+0x08).
//   3. Compute abs(which_); if < 3, return.
//   4. Else SSO check on variant string at +0x18: if [rbx+0x18]&1, tail-jmp
//      operator delete([rbx+0x28], cap&~1). The cap is the qword at
//      variant+0x10 = +0x28.
//
// Both std::string fields are libc++ short-form-or-long with cap stored at
// +0x10 within the string and the heap pointer at +0x18 — but the dtor reads
// rdi=heap-pointer from string+0x10 (mov rdi,[rbx+0x48] / [rbx+0x28]). This
// matches libc++'s arrangement when the long-form layout is laid out as
// (cap|1 @+0, size @+8, ptr @+16) and __l reads ptr from +0x10 of the
// short-form view... but the binary at f14c0 was rebuilt to use that order.
// ============================================================================
Resources::Variable::~Variable()  // @0x1e4be0
{
    // The `name` std::string member at +0x38 and the embedded `value`
    // Value at +0x08 are both destroyed automatically by the
    // compiler-generated subobject teardown. Value's destructor handles
    // the variant-payload string cleanup (when abs(which_) >= 3).
    //
    // Previously this dtor reached into the variant storage manually
    // because Variable was modeled with raw `flagWord/which_/variantStorage`
    // fields. After the Phase 20e-ii Batch 5a wrap-up cleanup that
    // promoted the +0x08..+0x2F block to a real `Value value` member,
    // no manual cleanup is needed.
}

// ============================================================================
// Resources::Function — @0x1eaa00 (ctor), @0x1ea820 (dtor)
//
// CORRECTED 2026-04-24 (Phase 20e-ii): The ctor takes FOUR args, not three.
// The 4th is a weak_ptr<Resources> for the parent scope; the function's own
// child scope is constructed inside the ctor via std::allocate_shared and
// stored at +0x60. The previous 3-arg signature was wrong.
//
// Real bodies for ctor/dtor/addArguments/addBody/addArgument/isSame/
// sameArgString live in src/resources_function.cpp (Phase 20e-ii Batch 5).
// The stubs below have been removed in favour of that dedicated TU to keep
// the per-method disasm context grouped.
// ============================================================================


// ============================================================================
// Resources::Resources — @0x1e3420
//
// Initializes all fields to zero/empty:
//   - Zeroes +0x08..+0x17 (enable_shared_from_this weak_ptr)
//   - Stores parent shared_ptr at +0x18 (from weak_ptr.lock() or empty)
//   - Copies name string to +0x28
//   - Stores weak_ptr arg at +0x40
//   - state_ = 0, returnType_ = 0, scopeBoundaryFlags_ = 0
//   - returnValue_ zeroed, returnReg_ = AsmRegister(0) = {0, true}
//   - All vectors empty
// ============================================================================
Resources::Resources(std::string const& name,  // @0x1e3420
                     std::weak_ptr<Resources> parent)
    : parent_()  // set below after parentWeak_ init
    , name_(name)
    , parentWeak_(parent)
    , state_(0)
    , returnType_(VarType_Unset)
    , returnValue_()
    , returnReg_(AsmRegister(0))  // Binary @0x1e34ae: AsmRegister(0) → {0, true}
    , scopeBoundaryFlags_(0)
    , pad_8A_{}
    , variables_()
    , functions_()
    , children_()
{
    // Binary @0x1e34f1: locks parentWeak_, then copies parent->parent_ into
    // this->parent_ (grandparent strong ref).  parentWeak_ is the direct
    // parent link (weak).
    if (auto p = parentWeak_.lock()) {
        parent_ = p->parent_;
    }
}

// ============================================================================
// Resources::~Resources — D1 @0x12a8f0, D0 @0x1f1150
//
// D1 (non-deleting):
//   1. Sets vptr to Resources vtable+0x10
//   2. Destroys children_ vector (each shared_ptr<Resources> released)
//   3. Destroys functions_ vector (each shared_ptr<Function> released)
//   4. Destroys variables_ vector (each Variable dtor runs — @0x1e4be0)
//   5. Destroys returnValue_ (Value dtor — frees string if variant holds one)
//   6. Destroys parentWeak_ (weak_ptr)
//   7. Destroys name_ (string)
//   8. Destroys parent_ (shared_ptr)
//   9. Destroys enable_shared_from_this weak_ptr at +0x08
//
// D0 (deleting): calls D1 then operator delete(this, 0xd8)
// ============================================================================
Resources::~Resources()  // D1 @0x12a8f0
{
    // All member destructors run in reverse declaration order.
}

// ============================================================================
// Resources::setState — @0x1e35f0
//
// If state_ == 0 (Unset), set to the requested value.
// Otherwise, force to 3 (Locked).
// ============================================================================
void Resources::setState(State s)  // @0x1e35f0
{
    if (state_ == 0) {
        state_ = static_cast<int32_t>(s);
    } else {
        state_ = static_cast<int32_t>(State::Locked);
    }
}

// ============================================================================
// Resources::hasMain — @0x1e3610
//
// Iterates functions_ vector. Returns true if any function has name == "main"
// (length 4, compared as 4 bytes).
// ============================================================================
bool Resources::hasMain() const  // @0x1e3610
{
    for (auto const& fn : functions_) {
        if (fn->name == "main") {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Resources::setReturnType — @0x1e3920
//
// Simply stores the value at +0x54.
// ============================================================================
void Resources::setReturnType(VarType type)  // @0x1e3920
{
    returnType_ = type;
}

// ============================================================================
// Resources::getReturnType — @0x1e3930
//
// If returnType_ != 0, return it.
// Otherwise, walk up via parentWeak_ (lock → call getReturnType virtually).
// If parentWeak_ expired or no parent, throw ResourcesException.
// ============================================================================
VarType Resources::getReturnType() const  // @0x1e3930
{
    if (returnType_ != VarType_Unset) {
        return returnType_;
    }

    // Walk up via parentWeak_ only (binary has no parent_ fallback)
    if (auto p = parentWeak_.lock()) {
        return p->getReturnType();
    }

    throw ResourcesException("getReturnType: no parent scope");
}

// ============================================================================
// Resources::setReturnValue(double) — @0x1e3ac0
//
// Constructs a Value with type=Double (3), which_=2, data=double.
// Then calls setReturnValue(Value const&).
// ============================================================================
void Resources::setReturnValue(double val)  // @0x1e3ac0
{
    Value v;
    v.type_ = ValueType::Double;
    v.which_ = 2;
    v.storage_.d = val;
    setReturnValue(v);
}

// ============================================================================
// Resources::setReturnValue(Value) — @0x1e3b30
//
// If scopeBoundaryFlags_ == 0 and returnType_ == 0, recurse to parent via parentWeak_.
// If parentWeak_ expired, falls through to store locally.
// Otherwise, store the value at +0x58 (returnValue_).
// ============================================================================
void Resources::setReturnValue(Value const& val)  // @0x1e3b30
{
    if (scopeBoundaryFlags_ == 0 && returnType_ == VarType_Unset) {
        // Walk up via parentWeak_ only (binary has no parent_ fallback)
        if (auto p = parentWeak_.lock()) {
            p->setReturnValue(val);
            return;
        }
    }
    returnValue_ = val;
}

// ============================================================================
// Resources::getReturnValue — @0x1e3d40
//
// Similar recursive pattern: if scopeBoundaryFlags_==0 and returnType_==0, walk parent.
// Copies returnValue_ from the scope that has returnType_ set.
// ============================================================================
Value Resources::getReturnValue()  // @0x1e3d40
{
    if (scopeBoundaryFlags_ == 0 && returnType_ == VarType_Unset) {
        // Walk up via parentWeak_ only (binary has no parent_ fallback)
        if (auto p = parentWeak_.lock()) {
            return p->getReturnValue();
        }
    }
    return returnValue_;
}

// ============================================================================
// Resources::setReturnReg — @0x1e3ed0
//
// Recursive if returnType_==0: walk parent.
// Constructs AsmRegister{reg, true} and stores at +0x80.
// ============================================================================
void Resources::setReturnReg(int reg)  // @0x1e3ed0
{
    if (returnType_ == VarType_Unset) {
        // Walk up via parentWeak_ only (binary has no parent_ fallback)
        if (auto p = parentWeak_.lock()) {
            p->setReturnReg(reg);
            return;
        }
    }
    returnReg_ = AsmRegister::Reg(reg);
}

// ============================================================================
// Resources::getReturnReg — @0x1e3fe0
//
// Similar recursive pattern; returns AsmRegister from scope with returnType_ set.
// ============================================================================
AsmRegister Resources::getReturnReg()  // @0x1e3fe0
{
    if (returnType_ == VarType_Unset) {
        // Walk up via parentWeak_ only (binary has no parent_ fallback)
        if (auto p = parentWeak_.lock()) {
            return p->getReturnReg();
        }
    }
    return returnReg_;
}

// ============================================================================
// Resources::getRegisterNumber — @0x1e4bb0
//
// Reads TLS regNumber (at TLS+0x48), increments it, returns the old value.
// This is how registers are allocated sequentially.
// ============================================================================
int Resources::getRegisterNumber()  // @0x1e4bb0
{
    int n = GlobalResources::regNumber;
    GlobalResources::regNumber = n + 1;
    return n;
}

// ============================================================================
// Resources::getVariable — @0x1eb0a0 (base virtual)
//
// Searches variables_ vector for matching name (compares string at
// Variable+0x38). If found, returns pointer to the Variable and OR's
// scopeBoundaryFlags_ into result->flags (at +0x50) with bit 0x51.
//
// If not found, walks parent via parentWeak_ (lock → call getVariable
// virtually). If parentWeak_ expired, tries parent_ at +0x18.
// Returns nullptr if variable not found anywhere.
// ============================================================================
Resources::Variable* Resources::getVariable(std::string const& name)  // @0x1eb0a0
{
    // Search local variables
    for (auto& var : variables_) {
        if (var.name == name) {
            // OR scopeBoundaryFlags_ into var.flags (specifically byte at +0x51
            // within the result struct — sets scope-inherited flags)
            var.flags |= static_cast<int16_t>(scopeBoundaryFlags_);
            return &var;
        }
    }

    // Walk parent scope
    Variable* result = nullptr;
    if (auto p = parentWeak_.lock()) {
        result = p->getVariable(name);
        if (result) {
            result->flags |= static_cast<int16_t>(scopeBoundaryFlags_);
        }
        return result;
    }

    if (parent_) {
        result = parent_->getVariable(name);
        if (result) {
            result->flags |= static_cast<int16_t>(scopeBoundaryFlags_);
        }
        return result;
    }

    return nullptr;
}

// ============================================================================
// Resources::print — @0x1ebbe0
//
// Calls parent->print() recursively, then prints this->toString().
// ============================================================================
void Resources::print()  // @0x1ebbe0
{
    // Binary uses parentWeak_ (locks +0x48, reads +0x40), not parent_
    if (auto p = parentWeak_.lock()) {
        p->print();
    }
    std::cout << toString();
}

// ============================================================================
// Resources::toString — @0x1ebcf0
//
// Builds a string representation of this scope's variables and functions.
//
// Format for each variable depends on varType (CORRECTED mapping per Phase
// 19c-followup, Finding 1):
//   2 (var):    "v: <name> (Reg: <reg.value>)\n"
//   3 (string): "s: <name> -> <value>\n"
//   4 (const):  "c: <name> -> <value>\n"
//   5 (wave):   "w: <name> -> <value>\n"
//   6 (cvar):   "cv: <name> -> <value>\n"
//   default:    "?: <name>\n"
//
// Also prints state (using jump table for values 1-3).
// ============================================================================
std::string Resources::toString() const  // @0x1ebcf0
{
    std::ostringstream os;

    // Print scope header
    os << "=== " << name_ << " ===\n";

    // Print state (if set)
    if (state_ >= 1 && state_ <= 3) {
        // Jump table at state_-1, prints corresponding state string
        const char* stateNames[] = {"Active", "Paused", "Locked"};
        os << "State: " << stateNames[state_ - 1] << "\n";
    }

    // Print variables
    for (auto const& var : variables_) {
        switch (var.type) {
            case VarType_Var:
                os << "v: " << var.name << " (Reg: " << var.reg.value << ")\n";
                break;
            case VarType_String:
                os << "s: " << var.name << " -> "; /* value */ os << "\n";
                break;
            case VarType_Const:
                os << "c: " << var.name << " -> "; /* value */ os << "\n";
                break;
            case VarType_Wave:
                os << "w: " << var.name << " -> "; /* value */ os << "\n";
                break;
            case VarType_Cvar:
                os << "cv: " << var.name << " -> "; /* value */ os << "\n";
                break;
            default:
                os << "?: " << var.name << "\n";
                break;
        }
    }

    return os.str();
}

// ============================================================================
// Resources::printAll — @0x1ec460
//
// Calls print() (which recurses to parents), then calls printScopes().
// ============================================================================
void Resources::printAll()  // @0x1ec460
{
    print();
    printScopes();
}

// ============================================================================
// Resources::printScopes — @0x1ec570
//
// Iterates children_ vector. For each child:
//   1. Prints child->toString()
//   2. Recursively calls child->printScopes()
// ============================================================================
void Resources::printScopes()  // @0x1ec570
{
    for (auto const& child : children_) {
        std::cout << child->toString();
        child->printScopes();
    }
}

// ============================================================================
// str(VarType) @0x247dd0 — convert VarType enum to string
//
// Verified against jump table at 0x95c2a0 (Phase 19c-followup, Finding 1).
// The default branch returns "notype" (NOT "?"); this matches the binary's
// fall-through path for unrecognised values, including the legacy Unset=0.
// ============================================================================
std::string str(VarType vt) {
    switch (vt) {
        case VarType_Void:   return "void";
        case VarType_Var:    return "var";
        case VarType_String: return "string";
        case VarType_Const:  return "const";
        case VarType_Wave:   return "wave";
        case VarType_Cvar:   return "cvar";
        case VarType_Unset:
        default:             return "notype";
    }
}

// str(VarSubType) @0x247ee0 — convert VarSubType enum to string
std::string str(VarSubType vst) {
    switch (vst) {
        case VarSubType_Default:     return "none";
        case VarSubType_Stub:        return "bool";
        case VarSubType_FunctionArg: return "arg";
        case VarSubType_Numeric:     return "vect";
        default:                     return {};
    }
}

// ============================================================================
// Resources::addConst — @0x1e7010
//
// Adds a const variable to variables_. The Variable record's `type` field at
// +0x00 is written as 4, which under the corrected VarType mapping (Phase
// 19c-followup, Finding 1) IS VarType_Const. The `subType` at +0x08 is
// written as 3 (VarSubType_Numeric, indicating a const-with-value form).
//
// Disassembly observations (1e7010..1e7331):
//   1. Loops variables_ checking for duplicate name; on hit jumps to throw
//      site at 1e7332 → ResourcesException(format(0xab=AlreadyDefined, name)).
//   2. If parent_ (rdi+0x18) non-null also calls variableExists(); same throw
//      on hit.
//   3. Builds a temporary Variable on the stack at [rbp-0x88]:
//        [rbp-0x88]  = 0x4   (type        — VarType_Const)
//        [rbp-0x80]  = 3     (subType     — VarSubType_Numeric, from edx)
//        [rbp-0x78]  = 2     (which_      — variant slot for double)
//        AsmRegister at [rbp-0x58] from AsmRegister(-1)
//        std::string copy of `name` at [rbp-0x50] (SSO-aware)
//   4. Builds an inline Value at [rbp-0xe0] with type=double, which_=2,
//      data=val, and writes it into the variant slot at +0x18 of the record.
//   5. Either writes directly into the vector slot (if capacity allows) or
//      calls __emplace_back_slow_path<Variable const&>.
//   6. The 4-way jump table at 0x95b0e8 dispatches the variant copy based
//      on |which_|: 0=int(4-byte), 1=bool(1-byte), 2=double(8-byte),
//      ≥3=string (calls __init_copy_ctor_external).
// ============================================================================
void Resources::addConst(std::string const& name, double val, VarSubType st)  // @0x1e7010
{
    // Duplicate check in current scope
    for (auto& var : variables_) {
        if (var.name == name) {
            throw ResourcesException(
                ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
        }
    }

    // Duplicate check in parent scope
    if (parent_ && parent_->variableExists(name)) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
    }

    // Build the Variable record. type=4 ⇒ VarType_Const under corrected
    // enum mapping; the binary writes the literal 4 at 1e70e3.
    //
    // CORRECTED Phase 20e-ii: the binary writes `st` (3rd arg) to +0x04
    // (subTypeRaw) at 1e71c7, and a *hardcoded* literal 3 to +0x08
    // (value.type_) at 1e717b. The previous reconstruction had these
    // backwards. See header comment on Variable layout for details.
    Variable v{};
    v.type        = VarType_Const;
    v.subTypeRaw  = st;                      // 1e71c7: caller's `st` → +0x04
    v.value.type_    = ValueType::Double;                       // 1e717b: hardcoded → +0x08 (numeric tag)
    v.value.which_      = 2;                       // variant slot for double (per 1e717b... wait, 1e7100 = which_=0 init; later set by variant_assign)
        v.reg        = AsmRegister::Invalid();  // AsmRegister(-1)
    v.name        = name;
    v.flags       = VarFlag_Written;  // 0x1e71c0: movb $0x1 — mark as initialized

    // Variant payload: store the double directly into the inline storage.
    // Layout matches a libc++ raw 16-byte buffer; the first 8 bytes are the
    // double value, the remaining 8 are the long-form-string ptr slot which
    // is unused for numeric payloads.
    std::memcpy(&v.value.storage_, &val, sizeof(double));

    variables_.push_back(v);
}

// ============================================================================
// Resources::readConst — @0x1e7d70
//
// sret return: rdi = EvalResultValue* out, rsi = this,
//              rdx = string const& name, ecx = EDirection
//
// Disassembly walk (1e7d70..1e7e6b for happy path):
//   1. Calls vtable[2] (offset +0x10) on `this` with arg = name → getVariable.
//      Result in [rbp-0x40].
//   2. nullptr → throw ResourcesException(format(0xb0=UninitializedVar, name))
//      at 1e7e6c.
//   3. If EDirection != 0 (Write path) AND var->flags byte at +0x50 == 0
//      → also throws UninitializedVar (same site at 1e7e6c).
//   4. Compares dword at var+0x00 with literal 4. Mismatch → throws
//      ResourcesException(format(0xaf=TypeMismatchWrite,
//                                str(VarType_Const), str(var->type))).
//      Under the corrected enum mapping (Phase 19c-followup, Finding 1),
//      tag 4 IS VarType_Const — there is no separate "record tag" enum.
//   5. Populates EvalResultValue at [rbx]:
//        +0x00 varType_    = 4 (VarType_Const)
//        +0x04..+0x0B [8 bytes copied from var+0x04 — covers subType +
//                     first 4 bytes of out's value_ field]
//        +0x10 which_      = |var->value.which_|
//        +0x18 variant data via 4-way jump table on |var->value.which_|
//        +0x30 reg_        = AsmRegister(-1)
// ============================================================================
EvalResultValue Resources::readConst(std::string const& name, EDirection dir)  // @0x1e7d70
{
    Variable* var = getVariable(name);

    // Miss → UninitializedVar
    if (!var) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }

    // Write-path requires the var to have been assigned (flags low byte != 0).
    if (dir != EDirection::eIN &&
        (var->flags & 0xFF) == 0) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }

    // Write-path requires the var to have been assigned (flags low byte != 0).
    if (dir != EDirection::eIN &&
        (var->flags & 0xFF) == 0) {

        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }

    // Type check: var->type must be VarType_Const (=4) for readConst.
    if (var->type != VarType_Const) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::TypeMismatchWrite,
                                  str(VarType_Const),
                                  str(var->type)));
    }

    EvalResultValue out{};
    out.varType_ = VarType_Const;

    // Copy subType and value type from variable into EvalResultValue.
    // The binary does a single 8-byte qword copy across var+0x04..+0x0C → out+0x04..+0x0C,
    // overlapping subTypeRaw (4 bytes) + value.type_ (4 bytes).
    out.varSubType_ = var->subTypeRaw;
    out.value_.type_ = var->value.type_;

    // Variant payload copy via |which_| dispatch
    int32_t w    = var->value.which_;
    int32_t absW = (w ^ (w >> 31));
    switch (absW) {
        case 0: {  // int — 4 bytes at value.storage_[0]
            int32_t i;
            std::memcpy(&i, &var->value.storage_, sizeof(i));
            out.value_.storage_.i = i;
            out.value_.which_     = 0;
            break;
        }
        case 1: {  // bool — 1 byte at value.storage_[0]
            // Binary reads byte at var+0x18 (= value.storage_ base), tests != 0.
            // On libstdc++ the bool member is at the same offset within the union.
            out.value_.storage_.b = var->value.storage_.b;
            out.value_.which_     = 1;
            break;
        }
        case 2: {  // double — 8 bytes at value.storage_[0]
            double d;
            std::memcpy(&d, &var->value.storage_, sizeof(d));
            out.value_.storage_.d = d;
            out.value_.which_     = 2;
            break;
        }
        default: { // string (absW >= 3) — copy libc++ std::string from variant
            // Variant storage holds a libc++ std::string at var+0x18 (24 bytes,
            // overlapping value.storage_[0..15] + pad_28). The destination is
            // the std::string embedded in out.value_.storage_ at out+0x18.
            // libc++ string layout (24B):
            //   short form: [0]=size<<1 (low bit clear), [1..22]=chars, [23]=0
            //   long  form: [0..7]=cap|1 (low bit set), [8..15]=size, [16..23]=ptr
            //
            // Disasm 1e7e06..1e7e25:
            //   test BYTE PTR [r12+0x18],0x1   ; SSO bit on source
            //   jne  1e7e2c                    ; long form → heap copy
            //   ; -- short path: bulk copy 24 bytes inline --
            //   mov  rcx,[r12+0x28] ; mov [rdi+0x10],rcx
            //   movups xmm0,[r12+0x18] ; movups [rdi],xmm0
            //   eax = 3
            // Disasm 1e7e2c..1e7e3b (long path):
            //   mov  rdx,[r12+0x20]            ; size
            //   mov  rsi,[r12+0x28]            ; data ptr
            //   call __init_copy_ctor_external ; constructs new string at out+0x18
            //   eax = abs(var->value.which_)         ; recomputed from [r12+0x10]
            uint8_t const* src = reinterpret_cast<uint8_t const*>(&var->value.storage_);
            uint8_t*       dst = reinterpret_cast<uint8_t*>(&out.value_.storage_);
            if ((src[0] & 1) == 0) {
                // Short / SSO form: bulk copy the 24-byte inline buffer.
                std::memcpy(dst,        src,        16);
                std::memcpy(dst + 0x10, src + 0x10, 8);
            } else {
                // Long form: source holds (cap|1, size, char*). Reconstruct as
                // a fresh libc++ string via placement new(dst, ptr, size).
                auto const& srcStr = *reinterpret_cast<std::string const*>(src);
                ::new (dst) std::string(srcStr.data(), srcStr.size());
            }
            out.value_.which_ = absW;  // 1e7e3b..1e7e4b: store recomputed |which_|
            break;
        }
    }

    out.reg_ = AsmRegister::Invalid();   // AsmRegister(-1)
    return out;
}

// ============================================================================
// Resources::newLabel — @0x1ec6b0  (static)
//
// Generates a unique label string by concatenating a prefix with the
// per-thread `labelIndex` counter, post-incrementing it.
//
// Disassembly walk:
//   - Constructs std::ostringstream on the stack at [rbp-0x130].
//   - If the input string is empty, writes the literal "label" (5 chars at
//     0x904fae) and falls through to also write the input contents from
//     base+1 (the increment at 1ec727 skips the SSO size byte). For an empty
//     input that base+1 read is a zero-length copy → no extra bytes.
//   - Otherwise writes the input string directly via __put_character_sequence.
//   - Calls __tls_get_addr(0xb7acf8) to obtain the per-thread state block,
//     reads dword at +0x4c (labelIndex), stores (labelIndex+1) back, then
//     streams the OLD value into the ostringstream via operator<<(int).
//   - Returns ostringstream::str().
//
// The TLS slot at +0x4c matches GlobalResources::labelIndex.
// ============================================================================
std::string Resources::newLabel(std::string const& base)  // @0x1ec6b0
{
    std::ostringstream os;
    if (base.empty()) {
        os << "label";
    } else {
        os << base;
    }
    int32_t idx = GlobalResources::labelIndex;
    GlobalResources::labelIndex = idx + 1;
    os << idx;
    return os.str();
}

// ============================================================================
// =========================  PHASE 20e-ii BATCH 1  ===========================
// Leaf methods: variableExists, variableExistsInScope, getVariableType,
//               getVariableSubType, constIsSet, variableDependsOnVar
// All have no Resources-internal callees other than the already-defined
// virtual getVariable and themselves.
// ============================================================================

// ============================================================================
// Resources::variableExists — @0x1e4230
//
// Returns true iff `name` is present in this->variables_ or in any parent
// scope. Walks parents via parentWeak_.lock() first, falling back to
// parent_ (matches the getVariable parent-walk pattern).
//
// Disassembly observations (1e4230..1e4376):
//   - 1e4248..1e425b: load variables_ begin/end from this+0x90 / this+0x98.
//   - 1e425b..1e4276: extract (size, data) from the input string `name`
//     accounting for libc++ SSO (low bit of byte[0] = long-form flag).
//   - 1e4280..1e42cd: per-Variable inline string compare against
//     var.name at +0x38 (size at +0x40 long-form / shifted SSO byte;
//     data at +0x48 long-form / +0x39 SSO). Match → bcmp == 0 → return
//     true.
//   - 1e42d3..1e433c: on local miss, lock parentWeak_ (weak_count* at
//     this+0x48). If lock yields a non-null sp AND its raw ptr at
//     this+0x40 is also non-null, recurse via variableExists (NOT the
//     virtual getVariable) and return that result. Otherwise fall back
//     to parent_ at this+0x18 and recurse there. If both routes are
//     null, return false.
//
// Note: this scans variables_ DIRECTLY (no vtable[+0x10] call). The
// virtual getVariable path is only used by readConst / getVariableType /
// getVariableSubType / constIsSet.
// ============================================================================
bool Resources::variableExists(std::string const& name) const  // @0x1e4230
{
    for (auto const& var : variables_) {
        if (var.name == name) {
            return true;
        }
    }
    if (auto p = parentWeak_.lock()) {
        return p->variableExists(name);
    }
    if (parent_) {
        return parent_->variableExists(name);
    }
    return false;
}

// ============================================================================
// Resources::variableExistsInScope — @0x1e4390
//
// Returns true iff `name` is present in this scope's variables_, OR — if
// this scope has a parent_ — present anywhere in the parent_ chain via
// parent_->variableExists(name).
//
// Disassembly observations (1e4390..1e4457):
//   - 1e43a5..1e441b: identical local-scan loop to variableExists.
//   - 1e4423..1e443e: on local miss, load parent_ from this+0x18; if
//     non-null TAIL-CALL parent_->variableExists(name) (the disasm
//     literally `jmp` to 0x1e4230, not back to itself).
//
// Surprising: the "InScope" name is misleading — when a parent exists,
// the fallback is a full multi-scope variableExists, not a strict
// single-scope check. Also: only parent_ is consulted here; unlike
// variableExists/variableDependsOnVar, parentWeak_ is NOT used.
// ============================================================================
bool Resources::variableExistsInScope(std::string const& name) const  // @0x1e4390
{
    for (auto const& var : variables_) {
        if (var.name == name) {
            return true;
        }
    }
    if (parent_) {
        return parent_->variableExists(name);
    }
    return false;
}

// ============================================================================
// Resources::getVariableType — @0x1e4460
//
// Looks up a variable by name (via the virtual getVariable, vtable[+0x10])
// and returns its type field. Throws ResourcesException(UnknownVar=0xb1)
// when the lookup misses.
//
// Disassembly observations (1e4460..1e4495 happy path):
//   - 1e4473..1e447d: indirect call via [this+0x00]+0x10 → virtual
//     getVariable(name).
//   - 1e4480..1e4489: nullptr → throw site at 1e4496; non-null → return
//     DWORD PTR [rax+0x00] (Variable::type).
//   - 1e4496..1e4501: ErrorMessages::format(0xb1 = UnknownVar, name),
//     ResourcesException, __cxa_throw with typeinfo at 0xb03380.
//
// 0xb1 = 177 = ErrorMessageT::UnknownVar (NOT UninitializedVar 0xb0=176,
// which is what readConst/constIsSet throw).
// ============================================================================
VarType Resources::getVariableType(std::string const& name)  // @0x1e4460
{
    Variable* var = getVariable(name);
    if (!var) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UnknownVar, name));
    }
    return var->type;
}

// ============================================================================
// Resources::getVariableSubType — @0x1e4580
//
// Same shape as getVariableType but returns the dword at Variable+0x04
// — i.e. the `subTypeRaw` field that holds the caller-supplied
// VarSubType arg from addX. Throws ResourcesException(UnknownVar=0xb1)
// on miss.
//
// Disassembly observations (1e4580..1e45b6 happy path):
//   - 1e4593..1e459d: virtual getVariable call (vtable[+0x10]).
//   - 1e45a0..1e45a9: nullptr → throw; non-null → `mov eax, DWORD PTR
//     [rax+0x4]`.
//   - 1e45ed..1e45fa: same UnknownVar (0xb1) format call as
//     getVariableType.
//
// HISTORICAL NOTE (Phase 20e-ii Batch 1 finding): the original header
// had `subType` documented at +0x08 with `pad_04` at +0x04. The
// disassembly shows the binary reads from +0x04 here, and the addX
// disasms (Batch 2) show callers writing the `st` arg to +0x04 with a
// HARDCODED secondary-tag literal at +0x08. The header has been
// corrected (subTypeRaw at +0x04, value.type_ at +0x08). This method
// returns the caller-supplied subtype, not the hardcoded tag.
// ============================================================================
VarSubType Resources::getVariableSubType(std::string const& name)  // @0x1e4580
{
    Variable* var = getVariable(name);
    if (!var) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UnknownVar, name));
    }
    return var->subTypeRaw;
}

// ============================================================================
// Resources::constIsSet — @0x1e8050
//
// Returns true iff the named variable's "set"/written flag (byte at
// Variable+0x50) is non-zero. Throws ResourcesException(UninitializedVar
// = 0xb0 = 176) when the variable does not exist anywhere in the scope
// chain.
//
// Disassembly observations (1e8050..1e8087 happy path):
//   - 1e8063..1e806d: virtual getVariable call (vtable[+0x10]).
//   - 1e8070..1e8079: null → throw; non-null → `movzx eax, BYTE PTR
//     [rax+0x50]`.
//   - 1e80be..1e80cb: ErrorMessages::format(0xb0 = UninitializedVar,
//     name).
//
// Despite the name, the throw uses UninitializedVar rather than
// UnknownVar — the original error model treats "asked whether a
// non-existent const is set" as an uninitialised-access bug, not a
// generic unknown-variable bug.
// ============================================================================
bool Resources::constIsSet(std::string const& name)  // @0x1e8050
{
    Variable* var = getVariable(name);
    if (!var) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    return (var->flags & 0xFF) != 0;
}

// ============================================================================
// Resources::variableDependsOnVar — @0x1e40e0
//
// Returns true iff `name` is found in some ancestor (or local) scope AND
// the originally-queried scope is in a non-default state (state_ != 0),
// OR a strict ancestor independently returned true. The state_ check is
// applied at every level of recursion that finds a local match.
//
// Disassembly observations (1e40e0..1e421a):
//   - 1e40f8..1e41a8: same SSO-aware local linear scan as
//     variableExists.
//   - 1e41d9..1e41e1: local-hit return =
//         setne r14b on (DWORD PTR [rbx+0x50] != 0)
//     where rbx+0x50 is `state_` of the *calling* Resources. So a
//     local match returns `(state_ != 0)`, NOT just `true`.
//   - 1e4183..1e41cd: on local miss, lock parentWeak_ (weak_count* at
//     this+0x48). If lock returns a usable sp with a non-null raw ptr
//     at this+0x40, recurse via variableDependsOnVar. After recursion:
//     r14 = recursiveResult OR (state_ != 0). If lock failed/expired,
//     the disasm DOES NOT fall back to parent_ — it returns 0 directly.
//     The parentWeak_ branch is the ONLY ancestor path here.
//
// Net behaviour:
//   * No name in chain → false.
//   * Name only in this scope → state_ != 0.
//   * Name in some ancestor → recurse: any ancestor frame's local hit
//     propagates up OR'd with each intermediate scope's (state_ != 0)
//     flag.
// ============================================================================
bool Resources::variableDependsOnVar(std::string const& name) const  // @0x1e40e0
{
    for (auto const& var : variables_) {
        if (var.name == name) {
            return state_ != 0;
        }
    }
    if (auto p = parentWeak_.lock()) {
        bool parentResult = p->variableDependsOnVar(name);
        return parentResult || (state_ != 0);
    }
    return false;
}

// ============================================================================
// =========================  PHASE 20e-ii BATCH 2  ===========================
// add* full forms: addString(name,val), addWave(name,val), addCvar(name,val,st),
//                  addConst(name, st) — the stub overload
// All follow the addConst(name, val, st) template above (lines 499–542).
// ============================================================================

// The addString/addWave bodies use placement-new to build a std::string
// inside Variable::value.storage_[16] + Variable::pad_28 (8 bytes more,
// overlapping the long-form ptr slot at +0x28 — see the documented
// layout in resources.hpp). The actual binary uses libc++ where
// sizeof(std::string)=24 bytes, fitting in 16+8. When we build with
// libstdc++ on the host the string is 32 bytes which overflows by 8;
// this is one manifestation of the libc++/libstdc++ ABI mismatch
// already tracked in notes/libcpp_abi.md. The
// reconstructed type layout is preserved for documentation; runtime
// instantiation requires the ABI unification work to be done first.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wplacement-new="

// ============================================================================
// Resources::addString — @0x1e5020
//
// Adds a string-typed variable to variables_. Variable.type = 3
// (VarType_String).
//
// Disassembly observations (1e5020..1e54a1):
//   1. Loops variables_ and (if parent_ non-null) calls
//      parent_->variableExists() checking for duplicate name; throws
//      ResourcesException(format(0xab=AlreadyDefined, name)) at 1e5353
//      (esi=0xab loaded at 1e53a2).
//   2. Builds a temporary Variable on the stack at [rbp-0x80]:
//        +0x00 type     = 3   (VarType_String, written as QWORD at 1e50f3
//                              which also zero-fills +0x04..+0x07).
//        +0x04 subTypeRaw = 0 from the QWORD write; this overload takes
//                            no `st` arg, so subTypeRaw stays 0
//                            (=VarSubType_Default). Note the val-string
//                            copy branch at 1e51ba writes 4 to +0x08
//                            (the value.type_), NOT to +0x04.
//        +0x08 value.type_ = 4   (=VarSubType_String literal, hardcoded at
//                              1e51ba).
//        +0x10 which_   = 3   (string-tag in variant slot — the variant
//                              fast-path stores abs(which_) at 1e52b5).
//        +0x30 reg      = AsmRegister(-1)
//        +0x38 name     = `name` (libc++ SSO copy)
//        +0x50 flags    = 1   (set/written, written at 1e5153).
//   3. Variant payload: libc++ std::string copy of `val` placed at
//      value.storage_ via __init_copy_ctor_external.
//   4. push_back into variables_ (fast path or __emplace_back_slow_path).
// ============================================================================
void Resources::addString(std::string const& name, std::string const& val)  // @0x1e5020
{
    for (auto& var : variables_) {
        if (var.name == name) {
            throw ResourcesException(
                ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
        }
    }
    if (parent_ && parent_->variableExists(name)) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
    }

    Variable v{};
    v.type       = VarType_String;
    v.subTypeRaw = VarSubType_Default;       // no `st` arg, +0x04 stays 0
    v.value.type_   = ValueType::String;                        // 1e51ba: hardcoded string-flag
    v.value.which_     = 3;                        // string-tag in variant slot
    v.reg        = AsmRegister::Invalid();
    v.name       = name;
    v.flags      = 1;                        // 1e5153: bit 0 set ("written")

    // Variant payload: libc++ string at value.storage_ via placement-new.
    ::new (&v.value.storage_) std::string(val);

    variables_.push_back(v);
}

// ============================================================================
// Resources::addWave — @0x1e6020
//
// Identical shape to addString but with type=5 (VarType_Wave). The string
// argument is the wave-name reference stored in the variant slot.
//
// Disassembly observations (1e6020..1e64a1) — byte-for-byte mirror of
// addString except the type immediate at 1e60f3 is 0x5 (vs 0x3). All
// other layout writes match: value.type_=4 at 1e61ba, which_=3, flags=1 at
// 1e6153, throw site uses esi=0xab=AlreadyDefined at 1e63a2.
// ============================================================================
void Resources::addWave(std::string const& name, std::string const& val)  // @0x1e6020
{
    for (auto& var : variables_) {
        if (var.name == name) {
            throw ResourcesException(
                ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
        }
    }
    if (parent_ && parent_->variableExists(name)) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
    }

    Variable v{};
    v.type       = VarType_Wave;
    v.subTypeRaw = VarSubType_Default;
    v.value.type_   = ValueType::String;                        // 1e61ba: hardcoded
    v.value.which_     = 3;                        // string-tag
    v.reg        = AsmRegister::Invalid();
    v.name       = name;
    v.flags      = 1;

    ::new (&v.value.storage_) std::string(val);

    variables_.push_back(v);
}

// ============================================================================
// Resources::addCvar — @0x1e8180
//
// Adds a cvar (compile-time variable) with a numeric value. type=6
// (VarType_Cvar). Like addConst(double, VarSubType) above, the variant
// slot holds a double (which_=2).
//
// Disassembly observations (1e8180..1e84a1):
//   1. Duplicate checks (current scope 1e81bc..1e8231; parent
//      1e823a..1e824d) → throw esi=0xab at 1e84a2.
//   2. Builds a temporary Variable at [rbp-0x88]:
//        +0x00 type       = 6   (VarType_Cvar).
//        +0x04 subTypeRaw = `st` arg (eax/edx, written at 1e8334).
//        +0x08 value.type_   = 3   (hardcoded at 1e82eb — Numeric tag).
//        +0x10 which_     = 0 init; variant_assign writes 2 (double).
//        +0x30 reg        = AsmRegister(-1).
//        +0x38 name       = `name` (libc++ SSO copy).
//        +0x50 flags      = 1   (1e8330: set/written).
//   3. Variant payload: 8-byte double via variant_assign case 2.
// ============================================================================
void Resources::addCvar(std::string const& name, double val, VarSubType st)  // @0x1e8180
{
    for (auto& var : variables_) {
        if (var.name == name) {
            throw ResourcesException(
                ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
        }
    }
    if (parent_ && parent_->variableExists(name)) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
    }

    Variable v{};
    v.type       = VarType_Cvar;
    v.subTypeRaw = st;                       // 1e8334: caller's `st`
    v.value.type_   = ValueType::Double;                        // 1e82eb: hardcoded Numeric
    v.value.which_     = 2;                        // variant slot for double
    v.reg        = AsmRegister::Invalid();
    v.name       = name;
    v.flags      = 1;                        // set/written

    std::memcpy(&v.value.storage_, &val, sizeof(double));

    variables_.push_back(v);
}

// ============================================================================
// Resources::addConst (stub overload) — @0x1e74e0
//
// "Declare without value" form of addConst. Builds a Variable with
// type=4 (VarType_Const), subTypeRaw=`st`, value.type_=1 (Stub-tag), and a
// zero-valued int payload (which_=0). The flags byte (+0x50) is set to
// 1 only when `st == 2` (`VarSubType_FunctionArg` — parameter binding
// path used by Function::addArgument @0x1e9f60); for the common Stub
// call path (st=1=Stub) flags=0, marking the var as "declared but
// unset" (readConst will reject it).
//
// Disassembly observations (1e74e0..1e7800):
//   1. Duplicate checks (current 1e7517..1e7591; parent 1e759a..1e75ad)
//      → throw esi=0xab at 1e77ff.
//   2. Builds a temporary Variable at [rbp-0x88]:
//        +0x00 type       = 4   (VarType_Const).
//        +0x04 subTypeRaw = `st` arg (1e768d).
//        +0x08 value.type_   = 1   (hardcoded at 1e7645 — Stub tag).
//        +0x10 which_     = 0   (int slot; payload also 0).
//        +0x30 reg        = AsmRegister(-1).
//        +0x38 name       = `name` (libc++ SSO copy).
//        +0x50 flags      = (st == 2 ? 1 : 0)   — 1e7693..1e7696:
//                           cmp eax,2 / sete BYTE PTR [rbp-0x38].
//
// The `st == 2` codepath (`VarSubType_FunctionArg`) that pre-marks
// the stub as written is the parameter-binding path: Function::addArgument
// @0x1e9f60 calls addConst(name, /*st=*/2) on the inner scope so the
// parameter slot appears written but cannot be re-bound (the +0x51 byte,
// also set on this path, gates update*-with-value short-circuit). See
// notes/unknowns.md #117.
// ============================================================================
void Resources::addConst(std::string const& name, VarSubType st)  // @0x1e74e0
{
    for (auto& var : variables_) {
        if (var.name == name) {
            throw ResourcesException(
                ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
        }
    }
    if (parent_ && parent_->variableExists(name)) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
    }

    Variable v{};
    v.type       = VarType_Const;
    v.subTypeRaw = st;                       // 1e768d
    v.value.type_   = ValueType::Int;                        // 1e7645: hardcoded Stub tag
    v.value.which_     = 0;                        // int slot
    v.reg        = AsmRegister::Invalid();
    v.name       = name;
    // No variant payload — already zero from value-init.
    v.flags      = (static_cast<int32_t>(st) == 2) ? 1 : 0;

    variables_.push_back(v);
}

#pragma GCC diagnostic pop

// ============================================================================
// addString / addWave / addCvar (stub overloads) — Phase 20e-ii Sub-phase 5b
//
// These three "declare without value" overloads are referenced by
// Function::addArgument @0x1e9f60 (with st=VarSubType_FunctionArg=2). They
// were declared in resources.hpp in earlier batches but never defined,
// leaving 3 undefined zhinst symbols in the static archive that surfaced
// when addArgument was reconstructed. Reconstructing them closes the gap.
//
// All three follow the addConst-stub template (@0x1e74e0, lines 1106-1130
// above), with two structural differences:
//
//   1. The Variable's `type` field differs:
//        addString → VarType_String (3)
//        addWave   → VarType_Wave   (5)
//        addCvar   → VarType_Cvar   (6)
//
//   2. The embedded Value's payload differs:
//        addString / addWave  →  empty std::string
//                                (value.type_=String=4, which_=3,
//                                 storage_=empty SSO string).
//                                Built in the disasm via
//                                boost::variant_assign at e.g. 0x1e5667 —
//                                we use `Value(std::string{})` which calls
//                                Value(string const&) @0x22c2b0 and
//                                produces the same in-memory shape.
//        addCvar              →  default-init Int=0
//                                (value.type_=Stub=1, which_=0,
//                                 storage_.i=0). Same as addConst stub.
//
// Disasm addresses:
//   addString stub @0x1e54f0 (~300 lines)
//   addWave   stub @0x1e64f0 (~300 lines)
//   addCvar   stub @0x1e8650 (~300 lines)
//
// All three start with the same duplicate-name guard (current scope +
// parent walk) that throws AlreadyDefined (esi=0xab=171) on collision,
// then build the Variable on the stack at [rbp-0x88..-0x30], then
// vector::push_back into variables_. The flags-byte computation is
// `(st == 2) ? 1 : 0` (cmp eax,2; sete BYTE PTR [rbp-0x38]).
//
// The disasm uses libc++'s vector fast-path/slow-path dispatch and a jump
// table on Value::which_ for the storage payload move. At the C++ level
// these are all `variables_.push_back(v)`.
// ============================================================================

void Resources::addString(std::string const& name, VarSubType st)  // @0x1e54f0
{
    for (auto& var : variables_) {
        if (var.name == name) {
            throw ResourcesException(
                ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
        }
    }
    if (parent_ && parent_->variableExists(name)) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
    }

    Variable v{};
    v.type       = VarType_String;
    v.subTypeRaw = st;
    v.value      = Value(std::string{});  // empty-string variant
    v.reg        = AsmRegister::Invalid();
    v.name       = name;
    v.flags      = (static_cast<int32_t>(st) == 2) ? 1 : 0;

    variables_.push_back(v);
}

void Resources::addWave(std::string const& name, VarSubType st)  // @0x1e64f0
{
    for (auto& var : variables_) {
        if (var.name == name) {
            throw ResourcesException(
                ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
        }
    }
    if (parent_ && parent_->variableExists(name)) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
    }

    Variable v{};
    v.type       = VarType_Wave;
    v.subTypeRaw = st;
    v.value      = Value(std::string{});  // empty-string variant
    v.reg        = AsmRegister::Invalid();
    v.name       = name;
    v.flags      = (static_cast<int32_t>(st) == 2) ? 1 : 0;

    variables_.push_back(v);
}

void Resources::addCvar(std::string const& name, VarSubType st)  // @0x1e8650
{
    for (auto& var : variables_) {
        if (var.name == name) {
            throw ResourcesException(
                ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
        }
    }
    if (parent_ && parent_->variableExists(name)) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
    }

    Variable v{};
    v.type            = VarType_Cvar;
    v.subTypeRaw      = st;
    v.value.type_     = ValueType::Int;  // Stub tag
    v.value.which_    = 0;                          // int slot
    v.reg             = AsmRegister::Invalid();
    v.name            = name;
    // No variant payload — value-initialised storage stays zero.
    v.flags           = (static_cast<int32_t>(st) == 2) ? 1 : 0;

    variables_.push_back(v);
}

// ============================================================================
// =========================  PHASE 20e-ii BATCH 3  ===========================
// addVar, updateVar, checkVar, updateString, updateWave, updateConst,
// updateCvar — variable mutation/check methods.
//
// Common patterns identified from disassembly:
//
//   * The update*/check* methods all start with a virtual getVariable() call
//     via [vptr+0x10] (slot 3 of the Resources vtable: ~D1, ~D0,
//     getVariable). The result is used to walk parent scopes — derived
//     classes (StaticResources) override getVariable to inject special
//     names. We model this with a polymorphic call, mirroring the binary.
//
//   * Variable.flags is a 16-bit field at +0x50:
//       low byte  (+0x50, bit 0) = "set/written" flag — checked by
//                                  constIsSet, written by add*/update* on
//                                  successful assignment.
//       high byte (+0x51)        = "frozen/parameter" flag — when non-zero
//                                  the update path SKIPS the value
//                                  assignment but still falls through to
//                                  mark the var as written. Likely set by
//                                  Function::addArgument so that argument
//                                  bindings appear written but cannot be
//                                  re-bound through the update path.
//
//   * The forced-reassignment guard in updateConst (`!force && (flags low
//     byte != 0)`) throws a no-args message at index 0x20 (=32) in errMsg.
//     The reconstructed ErrorMessageT enum names slot 32 as
//     `ConditionalNeedVarConst`, but the binary's actual string at index
//     32 is the "can't modify const" message. Either the slot name in the
//     reconstructed enum is wrong, or the message file just happens to
//     reuse this slot. To stay faithful to the binary we cast to slot 32
//     directly with a cast. See unknowns.md item #92.
//
//   * The dependency guard `variableDependsOnVar(name)` throws
//     `CantModifyVarInRepeat` (=228=0xe4). This makes sense: in a repeat
//     loop, mutating a variable that the loop iteration depends on would
//     change the loop bound mid-flight.
// ============================================================================

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wplacement-new="

// ============================================================================
// Resources::addVar — @0x1e46b0
//
// Adds a generic Var (type=2, VarType_Var) to variables_. The variant
// payload is initialized to a zero-int Value (which_=0, value=0). Unlike
// addCvar/addConst which stash the caller's `st` arg in subTypeRaw, addVar
// keeps subTypeRaw=0 (the QWORD write at 1e4783 sets type=2 / subTypeRaw=0
// in one shot) and only consults `st` to compute the flags byte:
//   flags = (st == 2) ? 1 : 0
// — same `st==2` codepath as addConst-stub: this is the
// `VarSubType_FunctionArg` parameter-binding path used by
// Function::addArgument @0x1e9f60. See notes/unknowns.md #117/#118.
//
// Disassembly observations (1e46b0..1e4ba9):
//   1. Duplicate-name check: linear scan of variables_ (same SSO-aware
//      compare as variableExists). On hit → throw esi=0xab=AlreadyDefined
//      via the shared exception-build trampoline at 1e4a00.
//   2. Parent check: parent_ at +0x18; if non-null call
//      parent_->variableExists(name) (NOT virtual — direct call to the
//      base impl at 1e4230); on hit → throw 0xab.
//   3. Builds Variable v at [rbp-0x88]:
//        +0x00 type       = 2   (1e4783 QWORD = 2 — clears subTypeRaw
//                                in same write).
//        +0x04 subTypeRaw = 0   (from QWORD write).
//        +0x08 value.type_   = 1   (1e4815 — Stub tag, hardcoded).
//        +0x10 which_     = 0 init; variant_assign writes 0 (int slot).
//        +0x30 reg        = AsmRegister(-1) (1e47a7).
//        +0x38 name       = `name` (libc++ SSO copy).
//        +0x50 flags      = (st == 2) ? 1 : 0   (1e488c..1e4890:
//                            cmp eax,2 / sete BYTE PTR [rbp-0x38]).
//   4. Variant payload: variant_assign with source Value{which=0, int=0}
//      stored at [rbp-0xd0..rbp-0xb8]. variant_assign is the boost-style
//      copy-assign that handles the int/bool/double/string union; here it
//      writes the int slot.
//   5. push_back into variables_ (fast path or __emplace_back_slow_path).
// ============================================================================
void Resources::addVar(std::string const& name, VarSubType st)  // @0x1e46b0
{
    for (auto& var : variables_) {
        if (var.name == name) {
            throw ResourcesException(
                ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
        }
    }
    if (parent_ && parent_->variableExists(name)) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::AlreadyDefined, name));
    }

    Variable v{};
    v.type       = VarType_Var;
    v.subTypeRaw = VarSubType_Default;       // QWORD write at 1e4783 zeros +0x04
    v.value.type_   = ValueType::Int;                        // 1e4815: hardcoded Stub tag
    v.value.which_     = 0;                        // int slot
    v.reg        = AsmRegister(GlobalResources::regNumber++);  // @0x1e485a-0x1e4888
    v.name       = name;
    // Variant payload is value-initialized to int 0 (which_=0, payload=0).
    // No explicit write needed — Variable v{} above zeros value.storage_.
    v.flags      = (static_cast<int32_t>(st) == 2) ? 1 : 0;

    variables_.push_back(v);
}

// ============================================================================
// Resources::updateVar — @0x1e4c40
//
// Marks an existing Var as "written" (flags low-byte set). Throws
// UninitializedVar (0xb0) if the name is unknown (getVariable returns
// nullptr) and TypeMismatchWrite (0xaf) if the variable exists but is not
// a Var (type != 2 / VarType_Var).
//
// Disassembly observations (1e4c40..1e4e1b):
//   1. Calls vtable[+0x10] (= getVariable) on `this`, storing the result
//      at [rbp-0x38] (1e4c5d).
//   2. Tests for null → 1e4c7d throws UninitializedVar(name) via the
//      single-arg ErrorMessages::format with esi=0xb0 (1e4d25).
//   3. cmp DWORD PTR [rax], 0x2 (1e4c69) — if v->type != VarType_Var,
//      throw TypeMismatchWrite (0xaf=175) with str(VarType_Var) and
//      str(v->type) as args (1e4cb3..1e4cdd).
//   4. On success: mov BYTE PTR [rax+0x50], 0x1 (1e4c6e) — sets the low
//      byte of `flags` to mark "written". Note: a BYTE store, not a WORD
//      store, so the high byte at +0x51 (the "frozen" flag) is preserved.
//   5. Returns void.
// ============================================================================
void Resources::updateVar(std::string const& name)  // @0x1e4c40
{
    Variable* v = getVariable(name);
    if (!v) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (v->type != VarType_Var) {
        throw ResourcesException(ErrorMessages::format(
            ErrorMessageT::TypeMismatchWrite,
            str(VarType_Var),
            str(v->type)));
    }
    // Set low byte of flags only (preserves +0x51 "frozen" bit).
    v->flags = (v->flags & 0xFF00) | VarFlag_Written;
}

// ============================================================================
// Resources::checkVar — @0x1e4e20
//
// Read-side validation for a Var: throws UninitializedVar (0xb0) if the
// name is unknown or the var is unwritten (flags low byte == 0), and
// throws TypeMismatchRead (0xae=174) if the var is a String (type==3).
// Used by AsmGen / expression evaluation when reading a value.
//
// Note the asymmetry vs updateVar:
//   * updateVar validates "must be Var" (rejects everything else).
//   * checkVar validates "must NOT be String" (other non-Var types like
//     Const/Wave/Cvar pass through). The disasm at 1e4e4f only tests
//     `type == 3`. So checkVar accepts Var, Const, Wave, Cvar — but not
//     String. The caller is presumed to have already filtered for value
//     categories where this distinction matters.
//
// Disassembly observations (1e4e20..1e5012):
//   1. Calls vtable[+0x10] = getVariable into [rbp-0x70] (1e4e3d).
//   2. Null check + flags-low-byte check: if v==nullptr OR
//      [v+0x50]==0 → throw UninitializedVar(name) (0xb0 at 1e4e9d).
//   3. cmp DWORD PTR [rax], 0x3 (1e4e4f) — if type==String, throw
//      TypeMismatchRead (0xae) with str(VarType_Var) expected and
//      str(VarType_String) actual (literal 2 and 3 to the str() calls
//      at 1e4ee2 and 1e4ef8).
// ============================================================================
void Resources::checkVar(std::string const& name)  // @0x1e4e20
{
    Variable* v = getVariable(name);
    if (!v ||
        (v->flags & 0xFF) == 0) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (v->type == VarType_String) {
        throw ResourcesException(ErrorMessages::format(
            ErrorMessageT::TypeMismatchRead,
            str(VarType_Var),
            str(VarType_String)));
    }
}

// ============================================================================
// Resources::updateString — @0x1e59d0
//
// Updates an existing String variable's value. Validates type, repeat-
// dependency, and the +0x51 "frozen" flag before writing.
//
// Algorithm (1e59d0..1e5d62):
//   1. v = getVariable(name) via vtable[+0x10]; if null → throw
//      UninitializedVar(name) (0xb0 at 1e5be5).
//   2. if (v->type != 3 [String]) → throw TypeMismatchWrite (0xaf at
//      1e5b38) with str(VarType_String, v->type).
//   3. if (variableDependsOnVar(name)) → throw CantModifyVarInRepeat
//      (0xe4=228 at 1e5b98) with str(VarType_String). The disasm passes
//      a single argument string here.
//   4. if ([v+0x51] != 0) skip the value write (frozen-arg path,
//      1e5a2a..1e5ab6) and jump straight to the "mark written" step.
//   5. v->value.type_ = ValueType::String (hardcoded String tag at 1e5a34/1e5a7d).
//   6. variant_assign(&v->variant, &Value{which=3, string=val}) at
//      1e5a84 — copies a libc++ std::string into the variant slot.
//   7. v->subTypeRaw = st (1e5ab3).
//   8. v->flags low byte = 1 (1e5ab6).
//
// Notes:
//   * The Value built on the stack at [rbp-0x48..-0x30] uses which_=3
//      (string discriminator) and payload at [rbp-0x40..-0x30] which is
//      the libc++ string. The stack temporary is destroyed via the
//      variant cleanup pattern at 1e5a98 (operator delete on long-form
//      string buffer if SSO bit is clear).
// ============================================================================
void Resources::updateString(std::string const& name,
                             std::string const& val,
                             VarSubType st)  // @0x1e59d0
{
    Variable* v = getVariable(name);
    if (!v) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (v->type != VarType_String) {
        throw ResourcesException(ErrorMessages::format(
            ErrorMessageT::TypeMismatchWrite,
            str(VarType_String),
            str(v->type)));
    }
    if (variableDependsOnVar(name)) {
        throw ResourcesException(ErrorMessages::format(
            ErrorMessageT::CantModifyVarInRepeat,
            str(VarType_String)));
    }

    // The +0x51 high byte of `flags` is the "frozen" guard. When non-zero
    // the update *skips* the value assignment but still marks the var as
    // written below. This is the path used after Function::addArgument
    // binds parameters.
    bool const frozen =
        (v->flags & VarFlag_Frozen) != 0;
    if (!frozen) {
        v->value.type_ = ValueType::String;                      // String tag
        ::new (&v->value.storage_) std::string(val);  // variant slot copy
        v->value.which_     = 3;
        v->subTypeRaw = st;
    }
    v->flags = (v->flags & 0xFF00) | VarFlag_Written;
}

// ============================================================================
// Resources::updateWave — @0x1e69c0
//
// Byte-for-byte mirror of updateString with two literal swaps:
//   * type-check immediate at 1e69fa = 0x5 (vs 0x3 for String).
//   * str(VarType) calls use esi=0x5 (1e6afa, 1e6b73) for the
//     "expected" string in error formatting.
// All other offsets, error codes (0xb0/0xaf/0xe4), tag (4), discriminator
// (3), and the frozen-flag short-circuit are identical to updateString.
// ============================================================================
void Resources::updateWave(std::string const& name,
                           std::string const& val,
                           VarSubType st)  // @0x1e69c0
{
    Variable* v = getVariable(name);
    if (!v) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (v->type != VarType_Wave) {
        throw ResourcesException(ErrorMessages::format(
            ErrorMessageT::TypeMismatchWrite,
            str(VarType_Wave),
            str(v->type)));
    }
    if (variableDependsOnVar(name)) {
        throw ResourcesException(ErrorMessages::format(
            ErrorMessageT::CantModifyVarInRepeat,
            str(VarType_Wave)));
    }

    bool const frozen =
        (v->flags & VarFlag_Frozen) != 0;
    if (!frozen) {
        v->value.type_ = ValueType::String;                      // String-bound payload tag
        ::new (&v->value.storage_) std::string(val);
        v->value.which_     = 3;
        v->subTypeRaw = st;
    }
    v->flags = (v->flags & 0xFF00) | VarFlag_Written;
}

// ============================================================================
// Resources::updateConst — @0x1e79b0
//
// Updates an existing Const (numeric) variable. Has an extra `force`
// parameter that gates the "already-written reassignment" check.
//
// Algorithm (1e79b0..1e7d6e):
//   1. v = getVariable(name) via vtable[+0x10]; null → UninitializedVar
//      (0xb0 at 1e7bee).
//   2. v->type != 4 (Const) → TypeMismatchWrite (0xaf at 1e7aff) with
//      str(VarType_Const) and str(v->type).
//   3. variableDependsOnVar(name) → CantModifyVarInRepeat (0xe4 at
//      1e7b5f) with str(VarType_Const).
//   4. if (!force && [v+0x50] != 0) → throw errMsg[32] (no args, fetched
//      directly via the global errMsg singleton's operator[]). Slot 32
//      in the binary is the "can't modify const" message; the
//      reconstructed enum currently labels slot 32 as
//      `ConditionalNeedVarConst` which is a mislabel — see
//      see unknowns.md item for details.
//   5. if ([v+0x51] != 0) skip the value write (frozen, jump to mark-
//      written at 1e7a7d).
//   6. v->value.type_ = ValueType::Double (Numeric tag at 1e7a40).
//   7. variant_assign(&v->variant, &Value{which=2, double=val}) at
//      1e7a4b — writes the double slot.
//   8. v->subTypeRaw = st (1e7a7a).
//   9. v->flags low byte = 1 (1e7a7d).
// ============================================================================
void Resources::updateConst(std::string const& name,
                            double val,
                            VarSubType st,
                            bool force)  // @0x1e79b0
{
    Variable* v = getVariable(name);
    if (!v) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (v->type != VarType_Const) {
        throw ResourcesException(ErrorMessages::format(
            ErrorMessageT::TypeMismatchWrite,
            str(VarType_Const),
            str(v->type)));
    }
    if (variableDependsOnVar(name)) {
        throw ResourcesException(ErrorMessages::format(
            ErrorMessageT::CantModifyVarInRepeat,
            str(VarType_Const)));
    }
    if (!force &&
        (v->flags & 0xFF) != 0) {
        // No-args message at slot 32. The enum name for slot 32
        // (`ConditionalNeedVarConst`) may not match the binary's actual
        // string here; see unknowns.md item #92.
        throw ResourcesException(
            errMsg[static_cast<ErrorMessageT>(32)]);
    }

    bool const frozen =
        (v->flags & VarFlag_Frozen) != 0;
    if (!frozen) {
        v->value.type_ = ValueType::Double;                      // Numeric tag
        std::memcpy(&v->value.storage_, &val, sizeof(double));
        v->value.which_     = 2;                    // double slot
        v->subTypeRaw = st;
    }
    v->flags = (v->flags & 0xFF00) | VarFlag_Written;
}

// ============================================================================
// Resources::updateCvar — @0x1e8b20
//
// Mirror of updateConst without the `force` parameter (and therefore
// without the "already-written reassignment" guard at slot 32). The
// type-check immediate is 0x6 (VarType_Cvar) and str() calls use esi=0x6.
// All other behaviour matches updateConst exactly.
//
// Disassembly observations (1e8b20..1e8e72):
//   * Same 4-step prologue (getVariable, null/UninitializedVar,
//     type/TypeMismatchWrite, variableDependsOnVar/CantModifyVarInRepeat).
//   * Same +0x51 frozen short-circuit then assignment of value.type_=3 +
//     double payload + subTypeRaw=st.
//   * Same final BYTE store at +0x50.
// ============================================================================
void Resources::updateCvar(std::string const& name,
                           double val,
                           VarSubType st)  // @0x1e8b20
{
    Variable* v = getVariable(name);
    if (!v) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (v->type != VarType_Cvar) {
        throw ResourcesException(ErrorMessages::format(
            ErrorMessageT::TypeMismatchWrite,
            str(VarType_Cvar),
            str(v->type)));
    }
    if (variableDependsOnVar(name)) {
        throw ResourcesException(ErrorMessages::format(
            ErrorMessageT::CantModifyVarInRepeat,
            str(VarType_Cvar)));
    }

    bool const frozen =
        (v->flags & VarFlag_Frozen) != 0;
    if (!frozen) {
        v->value.type_ = ValueType::Double;                      // Numeric tag
        std::memcpy(&v->value.storage_, &val, sizeof(double));
        v->value.which_     = 2;                    // double slot
        v->subTypeRaw = st;
    }
    v->flags = (v->flags & 0xFF00) | VarFlag_Written;
}

// ============================================================================
// Resources::readString — @0x1e5d70
//
// Read a VarType_String (=3) variable. Compared to readConst, the type-tag
// shape is fixed (string is always the libc++ std::string slot), so the
// disasm calls Value::toString() on the embedded Value at var+0x8 and
// stores the resulting string into out.value_.storage_ (at out+0x18).
//
// Disassembly walk (1e5d70..1e5e3f for happy path):
//   1. vtable[2] (offset +0x10) on `this` with arg = name → getVariable.
//   2. nullptr → throw UninitializedVar(name) (1e5e40).
//   3. dir != Read AND var->flags byte at +0x50 == 0 → same throw.
//   4. var->type (dword at +0x00) != 3 → throw TypeMismatchRead(0xae)
//      with "expected" = str(VarType_Var=2), "actual" = str(var->type).
//      Note 1: the binary really uses esi=0x2 here (str(VarType_Var)) — a
//      semantically odd "expected var got string" message, but faithful.
//      Note 2: error code 0xae is TypeMismatchRead, distinct from the 0xaf
//      TypeMismatchWrite used by readConst/readCvar.
//   5. Happy path:
//        rsi = var + 8                          ; embedded Value*
//        Value::toString(out_str=[rbp-0x30])    ; libc++ string ABI
//        out.varType_      = 3   (VarType_String)
//        out.value_.type_  = 4   (Value::String tag — same value as value.type_
//                                  for "string" Variables)
//        out+0x18 = the toString result (24-byte SSO bulk copy or
//                   __init_copy_ctor_external on the long form)
//        out.value_.which_ = 3   (string slot)
//        out.reg_          = AsmRegister(-1)
//        out.subType_      = var->subTypeRaw (var+0x4 → out+0x4)
//      The toString temporary is then deleted (long-form only).
// ============================================================================
EvalResultValue Resources::readString(std::string const& name, EDirection dir)  // @0x1e5d70
{
    Variable* var = getVariable(name);

    if (!var) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (dir != EDirection::eIN &&
        (var->flags & 0xFF) == 0) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (var->type != VarType_String) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::TypeMismatchRead,
                                  str(VarType_Var),     // esi=0x2 in disasm
                                  str(var->type)));
    }

    // Call Value::toString() on embedded Value at var+8 to materialize a
    // libc++ std::string. The disasm passes `add rsi,0x8` then call
    // Value::toString(sret).
    Value* embedded = &var->value;
    std::string tmp = embedded->toString();

    EvalResultValue out{};
    out.varType_     = VarType_String;          // out+0x00 = 3
    out.value_.type_ = ValueType::String;       // out+0x08 = String tag (=4)

    // Move/copy the libc++ string into out.value_.storage_ (at out+0x18).
    // The disasm does: short form → bulk 24-byte copy; long form →
    // __init_copy_ctor_external. For the C++ source we just placement-new
    // a copy into the 24-byte storage slot at out+0x18.
    {
        uint8_t* dst = reinterpret_cast<uint8_t*>(&out.value_.storage_);
        ::new (dst) std::string(tmp);
    }

    out.value_.which_ = 3;                       // out+0x10 = string slot
    out.reg_          = AsmRegister::Invalid();  // AsmRegister(-1)
    out.varSubType_   = var->subTypeRaw;         // out+0x04 ← var+0x04
    return out;
}

// ============================================================================
// Resources::readWave — @0x1e6d60
//
// Byte-for-byte identical to readString except:
//   * type tag = 5 (VarType_Wave); both compare and "expected" arg use 0x5.
//   * out.varType_ = 5 (VarType_Wave).
// All other slots match readString (value.type_=4, value.which_=3, the
// libc++ string is the toString result, subType copied from var+0x4).
// ============================================================================
EvalResultValue Resources::readWave(std::string const& name, EDirection dir)  // @0x1e6d60
{
    Variable* var = getVariable(name);

    if (!var) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (dir != EDirection::eIN &&
        (var->flags & 0xFF) == 0) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (var->type != VarType_Wave) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::TypeMismatchRead,
                                  str(VarType_Wave),     // esi=0x5 in disasm
                                  str(var->type)));
    }

    Value* embedded = &var->value;
    std::string tmp = embedded->toString();

    EvalResultValue out{};
    out.varType_     = VarType_Wave;
    out.value_.type_ = ValueType::String;        // =4

    {
        uint8_t* dst = reinterpret_cast<uint8_t*>(&out.value_.storage_);
        ::new (dst) std::string(tmp);
    }

    out.value_.which_ = 3;
    out.reg_          = AsmRegister::Invalid();
    out.varSubType_   = var->subTypeRaw;
    return out;
}

// ============================================================================
// Resources::readCvar — @0x1e8e80
//
// Read a VarType_Cvar (=6) variable. Unlike readString/readWave (which
// flatten via Value::toString to a string), readCvar preserves the
// underlying variant kind: it copies var.value_.type_ (dword at var+0x8)
// straight into out.value_.type_ (at out+0x8) and dispatches on |var->value.which_|
// to copy the inline payload — the same 4-way jump as readConst.
//
// Disassembly walk (1e8e80..1e8f85 for happy path):
//   1. vtable[2] on `this` → getVariable.
//   2. nullptr → throw UninitializedVar.
//   3. dir != Read AND var->flags[+0x50] == 0 → same throw.
//   4. var->type (dword +0x00) != 6 → throw TypeMismatchWrite(0xaf) with
//      "expected" = the C string literal at 0x904f11 ("CVAR") and
//      "actual" = str(var->type). Note: 0xaf NOT 0xae — readCvar uses
//      TypeMismatchWrite where readString/readWave use TypeMismatchRead.
//   5. Happy path:
//        out.varType_     = 6                      (out+0x00)
//        out.value_.type_ = var->value.type_          (out+0x08 ← var+0x08)
//        switch (|var->value.which_|):                   (jump table @0x95b138)
//          0 (int):    out+0x18 = *(int*)(var+0x18)
//          1 (bool):   out+0x18 = *(uint8_t*)(var+0x18)
//          2 (double): out+0x18 = *(double*)(var+0x18)
//          3+ (str):   __init_copy_ctor_external from libc++ string at var+0x18
//        out.value_.which_ = (matched switch case index, or |which_| for str)
//        out.reg_          = AsmRegister(-1)
//        out.subType_      = var->subTypeRaw       (out+0x04 ← var+0x04)
// ============================================================================
EvalResultValue Resources::readCvar(std::string const& name, EDirection dir)  // @0x1e8e80
{
    Variable* var = getVariable(name);

    if (!var) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (dir != EDirection::eIN &&
        (var->flags & 0xFF) == 0) {
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::UninitializedVar, name));
    }
    if (var->type != VarType_Cvar) {
        // The "expected" arg is the C string literal "CVAR" (rodata @0x904f11),
        // not str(VarType_Cvar). format<char const*, std::string> overload.
        throw ResourcesException(
            ErrorMessages::format(ErrorMessageT::TypeMismatchWrite,
                                  "CVAR",
                                  str(var->type)));
    }

    EvalResultValue out{};
    out.varType_     = VarType_Cvar;                 // out+0x00 = 6
    out.value_.type_ = static_cast<ValueType>(var->value.type_);  // out+0x08 ← var+0x08

    int32_t w    = var->value.which_;
    int32_t absW = (w ^ (w >> 31));
    switch (absW) {
        case 0: {  // int
            int32_t i;
            std::memcpy(&i, &var->value.storage_, sizeof(i));
            out.value_.storage_.i = i;
            out.value_.which_     = 0;
            break;
        }
        case 1: {  // bool
            out.value_.storage_.b = var->value.storage_.b;
            out.value_.which_     = 1;
            break;
        }
        case 2: {  // double
            double d;
            std::memcpy(&d, &var->value.storage_, sizeof(d));
            out.value_.storage_.d = d;
            out.value_.which_     = 2;
            break;
        }
        default: {  // libc++ string (absW >= 3) — same SSO/long handling as readConst
            uint8_t const* src = reinterpret_cast<uint8_t const*>(&var->value.storage_);
            uint8_t*       dst = reinterpret_cast<uint8_t*>(&out.value_.storage_);
            if ((src[0] & 1) == 0) {
                std::memcpy(dst,        src,        16);
                std::memcpy(dst + 0x10, src + 0x10, 8);
            } else {
                auto const& srcStr = *reinterpret_cast<std::string const*>(src);
                ::new (dst) std::string(srcStr.data(), srcStr.size());
            }
            out.value_.which_ = absW;
            break;
        }
    }

    out.reg_     = AsmRegister::Invalid();
    out.varSubType_ = var->subTypeRaw;
    return out;
}

// ============================================================================
// Resources::createSubScope — @0x1e36a0
//
// Construct a child Resources whose name is `this->name_ + ":" + name`,
// whose parent is `shared_from_this()`, and append it into this->children_.
// Returns the new shared_ptr<Resources>.
//
// Disassembly walk (1e36a0..1e3861 for happy path):
//   1. Build full name string in [rbp-0x50]:
//        a. SSO/long-aware copy of this->name_ (at this+0x28) into a fresh
//           local string of size (parentLen + 1). The "+1" reserves room for
//           the trailing ":".
//        b. Write byte ':' (0x3a) at offset parentLen.  (The disasm writes
//           a 16-bit 0x003a — ':' followed by a NUL — but std::string::append
//           below overwrites the NUL.)
//        c. SSO/long-aware std::string::append(name) — appends the child
//           local name. Result string is moved into [rbp-0x70]/+0x60 by
//           the swap idiom that nulls out [rbp-0x50] (`xorps; movups`).
//   2. Lock self via the enable_shared_from_this hidden weak_ptr at
//      this+0x08/+0x10. If the lock fails throw bad_weak_ptr (1e387b).
//   3. allocate_shared<Resources>(default_allocator,
//                                 fullName,
//                                 weak_ptr<Resources>(self)).
//      The allocate_shared call site at 0x1f23a0 takes (alloc&, string const&,
//      shared_ptr<Resources>) — the parent argument is a shared_ptr that
//      is implicitly converted to weak_ptr inside the Resources ctor.
//   4. Release the locked self shared_ptr (lock decrements weak count via
//      `lock xadd; if zero call dtor + __release_weak`).
//   5. Free the temporary fullName string (long-form path).
//   6. emplace_back the new shared_ptr<Resources> into this->children_
//      (vector at this+0xC0).
//   7. Return the new shared_ptr<Resources> (rax = rbx, the sret slot).
//
// Length-overflow path (1e3880) calls __throw_length_error.
// ============================================================================
std::shared_ptr<Resources>
Resources::createSubScope(std::string const& name)  // @0x1e36a0
{
    // Build "<parent>:<name>".
    std::string fullName;
    fullName.reserve(name_.size() + 1 + name.size());
    fullName.append(name_);
    fullName.push_back(':');
    fullName.append(name);

    // Lock self via enable_shared_from_this. The disasm reads the hidden
    // weak_ptr at this+0x08/+0x10 directly and calls __shared_weak_count::lock.
    // shared_from_this() is the C++ surface for the same operation; it
    // throws bad_weak_ptr when no shared_ptr owns *this — matching the
    // 1e387b fallback in the disasm.
    std::shared_ptr<Resources> self = shared_from_this();

    // allocate_shared<Resources>(allocator, fullName, weak_ptr<Resources>(self)).
    // The Resources ctor (0x1e3420) takes (string const&, weak_ptr<Resources>).
    auto child = std::allocate_shared<Resources>(
        std::allocator<Resources>{},
        fullName,
        std::weak_ptr<Resources>(self));

    // Append to this->children_ (vector at +0xC0).
    children_.emplace_back(child);
    return child;
}

// ============================================================================
// Resources::updateParent — @0x1e38f0
//
// Replace this->parentWeak_ (the weak_ptr at +0x40/+0x48) with a weak_ptr
// derived from the by-value shared_ptr argument.
//
// Disassembly (1e38f0..1e391c, full body):
//   * Read the passed shared_ptr's (ptr, ctrl) pair at [rsi]/[rsi+8].
//   * If ctrl != null, lock-inc the WEAK count at ctrl+0x10.
//   * Read the OLD ctrl from [rdi+0x48] (parentWeak_.ctrl_).
//   * Overwrite [rdi+0x40..+0x50) with the new (ptr, ctrl) pair.
//   * If old ctrl != null, tail-call __shared_weak_count::__release_weak()
//     to decrement the previous weak count.
// This is exactly `parentWeak_ = p;` (weak_ptr assignment from shared_ptr).
// ============================================================================
void Resources::updateParent(std::shared_ptr<Resources> p)  // @0x1e38f0
{
    parentWeak_ = p;
}

#pragma GCC diagnostic pop

} // namespace zhinst
