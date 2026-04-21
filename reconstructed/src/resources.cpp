// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Resources base class — all method implementations
// ============================================================================

#include "zhinst/resources.hpp"

#include <iostream>
#include <sstream>

namespace zhinst {

// ============================================================================
// ResourcesException
// ============================================================================

ResourcesException::ResourcesException(std::string const& msg)  // @0x1e3a20
    : msg_(msg)
{}

ResourcesException::~ResourcesException() {}  // @0x1f12f0

const char* ResourcesException::what() const noexcept  // @0x1f1340
{
    return msg_.c_str();
}

// ============================================================================
// Resources::Variable::~Variable — @0x1e4be0
//
// 1. Frees string at +0x38 (name) if heap-allocated (SSO check: if
//    internal pointer != &short_buf, call operator delete)
// 2. Checks variant discriminator at +0x08: if abs(which_) >= 3,
//    the variant holds a string at +0x10, which also needs freeing
//    if heap-allocated.
// ============================================================================
Resources::Variable::~Variable()  // @0x1e4be0
{
    // name (std::string at +0x38) — destroyed automatically

    // variant string cleanup: if which_ indicates string type (abs >= 3),
    // destroy the string stored in the variant data at +0x10
    int w = which_ ^ (which_ >> 31);  // sign-magnitude decode
    if (w >= 3) {
        // Destroy the std::string embedded in variant storage at +0x10
        // (reinterpret_cast<std::string*>(&value.storage_))->~string();
    }
}

// ============================================================================
// Resources::Function — @0x1eaa00 (ctor), @0x1ea820 (dtor)
// ============================================================================

Resources::Function::Function(std::string const& n,  // @0x1eaa00
                              std::string const& sig,
                              VarType rt)
    : reserved_(nullptr)
    , weakRef_(nullptr)
    , name(n)
    , signature(sig)
    , returnType(rt)
    , pad_44(0)
    , arguments()
    , scope(nullptr)
    , body(nullptr)
{}

Resources::Function::~Function()  // @0x1ea820
{
    // 1. Delete body via unique_ptr (calls SeqCAstNode virtual dtor)
    // 2. Destroy scope (shared_ptr<Resources>)
    // 3. Destroy arguments vector (each Variable dtor runs)
    // 4. Destroy signature string
    // 5. Destroy name string
    // 6. Release weakRef_ (__shared_weak_count::__release_weak)
}

void Resources::Function::resetScope()  // @0x1eac80
{
    scope.reset();
}

void Resources::Function::addArguments(SeqCAstNode const& node)  // @0x1eab70
{
    // Extracts argument list from AST node and populates arguments vector.
    // Details depend on SeqCAstNode interface (not yet reconstructed).
}

void Resources::Function::addBody(SeqCAstNode const& node)  // @0x1ea7b0
{
    // Clones the AST node and stores in body unique_ptr.
    // Calls node's virtual clone method (vtable offset 0x30).
}

SeqCAstNode const* Resources::Function::getBody() const  // @0x1eab50
{
    return body.get();
}

// ============================================================================
// Resources::Resources — @0x1e3420
//
// Initializes all fields to zero/empty:
//   - Zeroes +0x08..+0x17 (enable_shared_from_this weak_ptr)
//   - Stores parent shared_ptr at +0x18 (from weak_ptr.lock() or empty)
//   - Copies name string to +0x28
//   - Stores weak_ptr arg at +0x40
//   - state_ = 0, returnType_ = 0, flags_88_ = 0
//   - returnValue_ zeroed, returnReg_ = {-1, false}
//   - All vectors empty
// ============================================================================
Resources::Resources(std::string const& name,  // @0x1e3420
                     std::weak_ptr<Resources> parent)
    : parent_(parent.lock())
    , name_(name)
    , parentWeak_(parent)
    , state_(0)
    , returnType_(VarType_Unset)
    , returnValue_()
    , returnReg_(AsmRegister::Invalid())
    , flags_88_(0)
    , pad_8A_{}
    , variables_()
    , functions_()
    , children_()
{}

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
// If parentWeak_ expired, try parent_.
// If no parent at all, throw ResourcesException.
// ============================================================================
VarType Resources::getReturnType() const  // @0x1e3930
{
    if (returnType_ != VarType_Unset) {
        return returnType_;
    }

    // Try parentWeak_ first
    if (auto p = parentWeak_.lock()) {
        return p->getReturnType();
    }

    // Fall back to parent_
    if (parent_) {
        return parent_->getReturnType();
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
// If flags_88_ == 0 and returnType_ == 0, recurse to parent via parentWeak_
// (same lock pattern as getReturnType).
// Otherwise, store the value at +0x58 (returnValue_).
// ============================================================================
void Resources::setReturnValue(Value const& val)  // @0x1e3b30
{
    if (flags_88_ == 0 && returnType_ == VarType_Unset) {
        // Walk up to parent
        if (auto p = parentWeak_.lock()) {
            p->setReturnValue(val);
            return;
        }
        if (parent_) {
            parent_->setReturnValue(val);
            return;
        }
    }
    returnValue_ = val;
}

// ============================================================================
// Resources::getReturnValue — @0x1e3d40
//
// Similar recursive pattern: if flags_88_==0 and returnType_==0, walk parent.
// Copies returnValue_ from the scope that has returnType_ set.
// ============================================================================
Value Resources::getReturnValue()  // @0x1e3d40
{
    if (flags_88_ == 0 && returnType_ == VarType_Unset) {
        if (auto p = parentWeak_.lock()) {
            return p->getReturnValue();
        }
        if (parent_) {
            return parent_->getReturnValue();
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
        if (auto p = parentWeak_.lock()) {
            p->setReturnReg(reg);
            return;
        }
        if (parent_) {
            parent_->setReturnReg(reg);
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
        if (auto p = parentWeak_.lock()) {
            return p->getReturnReg();
        }
        if (parent_) {
            return parent_->getReturnReg();
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
// flags_88_ into result->flags (at +0x50) with bit 0x51.
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
            // OR flags_88_ into var.flags (specifically byte at +0x51
            // within the result struct — sets scope-inherited flags)
            var.flags |= static_cast<int16_t>(flags_88_);
            return &var;
        }
    }

    // Walk parent scope
    Variable* result = nullptr;
    if (auto p = parentWeak_.lock()) {
        result = p->getVariable(name);
        if (result) {
            result->flags |= static_cast<int16_t>(flags_88_);
        }
        return result;
    }

    if (parent_) {
        result = parent_->getVariable(name);
        if (result) {
            result->flags |= static_cast<int16_t>(flags_88_);
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
    if (parent_) {
        parent_->print();
    }
    std::cout << toString();
}

// ============================================================================
// Resources::toString — @0x1ebcf0
//
// Builds a string representation of this scope's variables and functions.
//
// Format for each variable depends on varType:
//   2 (var):    "v: <name> (Reg: <reg.value>)\n"
//   3 (const):  "c: <name> -> <value>\n"
//   4 (cvar):   "cv: <name> -> <value>\n"
//   5 (string): "s: <name> -> <value>\n"
//   6 (wave):   "w: <name> -> <value>\n"
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
        switch (var.varType) {
            case VarType_Var:
                os << "v: " << var.name << " (Reg: " << var.reg.value << ")\n";
                break;
            case VarType_Const:
                os << "c: " << var.name << " -> "; /* value */ os << "\n";
                break;
            case VarType_Cvar:
                os << "cv: " << var.name << " -> "; /* value */ os << "\n";
                break;
            case VarType_String:
                os << "s: " << var.name << " -> "; /* value */ os << "\n";
                break;
            case VarType_Wave:
                os << "w: " << var.name << " -> "; /* value */ os << "\n";
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

} // namespace zhinst
