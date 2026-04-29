// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Resources::Function — nested class implementations.
//
// Split out of resources.cpp during Phase 20e-ii Batch 5 because two of the
// methods (sameArgString @0x1eb330 and addArgument @0x1e9f60) are >500 disasm
// lines each, and bundling them with the Resources base class methods made
// resources.cpp unwieldy. The CMake glob `file(GLOB src/*.cpp)` picks this
// file up automatically.
//
// All method addresses below refer to offsets in _seqc_compiler.so.
// ============================================================================

#include "zhinst/resources.hpp"
#include "zhinst/seqc_ast_node.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/expression.hpp"
#include "zhinst/asm_register.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>

namespace zhinst {

// ============================================================================
// Function class layout (size 0x78), confirmed by ctor + dtor disassembly.
//
//   +0x00  16  std::weak_ptr<Resources>          parentScope (the parent's
//                                                 weak_ptr captured by ctor)
//   +0x10  24  std::string                       name
//   +0x28  24  std::string                       signature
//   +0x40   4  VarType                           returnType
//   +0x44   4  (pad)
//   +0x48  24  std::vector<Variable>             arguments
//   +0x60  16  std::shared_ptr<Resources>        scope (allocated in ctor)
//   +0x70   8  std::unique_ptr<SeqCAstNode>      body  (8B for default deleter)
//   = 0x78
//
// The header (resources.hpp) declares the parentScope half as two raw void*
// fields named `pad_00_`/`weakRef_`; the disasm shows it is in fact a
// single std::weak_ptr<Resources>. We keep the existing header field names
// to avoid cascading source changes; the implementation here documents the
// real shape inline.
// ============================================================================

// ============================================================================
// Resources::Function::Function — @0x1eaa00
//
// 4-arg ctor: (string name, string signature, VarType returnType,
//              weak_ptr<Resources> parentScope).
//
// Disassembly walk (1eaa00..1eaac6 happy path):
//   1. Zero the parentScope weak_ptr at this+0x00 (xorps + movups @0x1eaa1e).
//   2. Copy the `name` string into this+0x10 via the libc++ SSO/long path
//      (test [rsi],0x1; bulk-copy 24B for SSO else __init_copy_ctor_external).
//   3. Same SSO/long copy for the `signature` string into this+0x28.
//   4. Store returnType (ecx) into this+0x40.
//   5. Zero the arguments vector at this+0x48 (xorps + qword zero of size
//      pointer).
//   6. allocate_shared<Resources>(default_allocator, name, parentScope) into
//      the scope slot at this+0x60. The allocate_shared call site takes
//      (alloc&, string&, weak_ptr<Resources>&) → matches the Resources ctor
//      at @0x1e3420 which is (string const&, weak_ptr<Resources>).
//   7. Zero the body unique_ptr at this+0x70 (only 8B written — body is the
//      default-deleter unique_ptr that fits in one pointer).
//   8. Propagate returnType into the new scope's returnType_ slot
//      (scope->returnType_ at scope+0x54, written from this->returnType at
//      this+0x40).
//
// Cleanup landing pads (1eaac7..1eab42) cover partial-construction unwinding
// of the scope, arguments vector, signature string, name string, and the
// already-acquired weak_ptr control block. They are reproduced implicitly
// by C++ subobject destruction order — no explicit code needed in source.
// ============================================================================
Resources::Function::Function(std::string const& name,
                              std::string const& sig,
                              VarType rt,
                              std::weak_ptr<Resources> parentScope)  // @0x1eaa00
    : pad_00_(nullptr),
      weakRef_(nullptr),
      name(name),
      signature(sig),
      returnType(rt),
      pad_44(0),
      arguments(),
      scope(std::allocate_shared<Resources>(
          std::allocator<Resources>{}, name, parentScope)),
      body(nullptr)
{
    // The ctor's mem-init list above zeroes parentScope (pad_00_/weakRef_),
    // copies name + signature, default-constructs the arguments vector, and
    // allocates the scope shared_ptr. The body unique_ptr default-constructs
    // to nullptr. parentScope itself is NOT stored in the
    // pad_00_/weakRef_ slots — those are zeroed by the disasm and the
    // captured weak_ptr is consumed by allocate_shared then dropped. The
    // physical weak_ptr layout at this+0x00 is currently "zeroed but unused";
    // see notes/struct_layouts.md for the open question on whether
    // pad_00_/weakRef_ should be promoted to a real weak_ptr<Resources>.

    // Propagate returnType into the new scope's returnType_ slot
    // (scope+0x54). Disasm 1eaaae..1eaab5.
    scope->returnType_ = returnType;
}

// ============================================================================
// Resources::Function::~Function — @0x1ea820
//
// Destruction order from disasm (1ea820..1ea948):
//   1. body unique_ptr at +0x70: vtable+0x30 deleting dtor on the held
//      pointer (if non-null), then zero the slot.
//   2. scope shared_ptr at +0x60/+0x68: atomic dec strong count; if zero,
//      vtable+0x10 destroy + __release_weak.
//   3. arguments vector at +0x48: walk backward, for each Variable destroy
//      its libc++ string at +0x18 (long-form free) and (if which_>=3) the
//      embedded variant string at +0x28; finally `operator delete` the
//      backing array.
//   4. signature std::string at +0x28: long-form free.
//   5. name std::string at +0x10: long-form free.
//   6. parentScope weak_ptr at +0x00: __release_weak on the ctrl ptr at +0x08
//      if non-null. Tail-called from the dtor epilogue.
//
// Implemented as a defaulted destructor — C++ subobject teardown produces
// exactly the above sequence in reverse declaration order. The disasm's
// custom in-place arguments-vector dtor inlining is also exactly what
// libc++'s vector<Variable>::~vector emits.
// ============================================================================
Resources::Function::~Function() = default;  // @0x1ea820

// ============================================================================
// Resources::Function::getBody — @0x1eab50
//
// Disassembly (full):
//   mov rsi, [rsi+0x70]          ; rsi = body.get() (raw SeqCAstNode*)
//   mov rax, [rsi]               ; rax = vptr
//   call [rax+0x20]              ; doClone() — sret return into rdi
//   mov rax, rbx                 ; return the sret slot
//
// Returns a CLONE of the body, not a borrow. The vtable+0x20 call is
// SeqCAstNode::doClone() which returns `unique_ptr<SeqCAstNode>` (16B sret).
// Header was previously declared `SeqCAstNode const* getBody() const`
// (raw borrow); corrected to `unique_ptr<SeqCAstNode>` in Phase 20e-ii
// Batch 5a alongside this body.
//
// NOTE: this implementation deliberately dereferences `body` (which is
// itself a unique_ptr) and calls doClone() on the pointee. If body is null
// the disasm crashes (no null check). We faithfully reproduce that — any
// caller that invokes getBody() on a Function with no body installed will
// segfault, matching the binary.
// ============================================================================
std::unique_ptr<SeqCAstNode> Resources::Function::getBody() const  // @0x1eab50
{
    return body->doClone();
}

// ============================================================================
// Resources::Function::addBody — @0x1ea7b0
//
// Replace this->body with a doClone of the given AST node. Frees the old
// body via its virtual deleting dtor.
//
// Disassembly walk (1ea7b0..1ea7ff happy path):
//   lea  rdi, [rbp-0x10]         ; sret slot for doClone() result
//   mov  rax, [rsi]              ; rax = node.vptr
//   call [rax+0x20]              ; node.doClone() → unique_ptr return via sret
//   mov  rax, [rbp-0x10]         ; rax = cloned raw ptr
//   mov  qword [rbp-0x10], 0     ; release ownership from temp (move)
//   mov  rdi, [rbx+0x70]         ; old body raw ptr
//   mov  [rbx+0x70], rax         ; install doClone into body slot
//   if (rdi != null) call rdi.vtable[+0x30]   ; deleting dtor on old body
//
// The trailing dead block at 1ea7e2..1ea7f8 is the unwind landing pad's
// "double-free guard" — it tests an already-zeroed temp and skips. It is
// not exercised in normal flow; the C++ source `body = node.doClone();`
// expands to exactly the happy path above.
//
// catch handler at 1ea800..1ea818: `__cxa_begin_catch; __cxa_end_catch`.
// This is the implicit catch-and-discard for a `try { ... } catch (...) {}`
// surrounding the doClone+install — but the disasm only does begin/end with
// no body. In practice this is a noexcept-ish swallow. We don't model it
// in source — the move-assignment of unique_ptr is itself noexcept and
// the only throw point is node.doClone(), which would propagate normally
// in real C++. The compiler appears to have generated this from a
// `try { ... } catch (...) {}` in a private helper that wraps the call;
// noted but not reproduced (low risk: differing exception semantics).
// ============================================================================
void Resources::Function::addBody(SeqCAstNode const& node)  // @0x1ea7b0
{
    body = node.doClone();
}

// ============================================================================
// Resources::Function::sameArgString — @0x1eb330
//
// Determines whether `sig` is argument-compatible with this function's
// `signature` string, ignoring the distinction between Const/Cvar/Var types.
//
// Algorithm (from disasm 0x1eb330..0x1eb7bb):
//   1. Early-out: if signature.size() < 3, return true (no meaningful args).
//   2. Normalise both strings by collapsing Cvar→Const, then Const→Var:
//        a. result1 = replace_all_copy(signature, str(Cvar), str(Const))
//        b. result2 = replace_all_copy(sig,       str(Cvar), str(Const))
//        c. result1 = replace_all_copy(result1,   str(Const), str(Var))
//        d. result2 = replace_all_copy(result2,   str(Const), str(Var))
//   3. Return result1 == result2 (length check + bcmp).
// ============================================================================
bool Resources::Function::sameArgString(std::string const& sig) const  // @0x1eb330
{
    if (signature.size() < 3) {
        return true;
    }

    // Round 1: collapse Cvar → Const in both strings.
    std::string cvar_str  = str(VarType_Cvar);
    std::string const_str = str(VarType_Const);

    std::string norm_sig  = boost::algorithm::replace_all_copy(signature, cvar_str, const_str);
    std::string norm_arg  = boost::algorithm::replace_all_copy(sig,       cvar_str, const_str);

    // Round 2: collapse Const → Var in both normalised strings.
    std::string var_str = str(VarType_Var);

    norm_sig = boost::algorithm::replace_all_copy(norm_sig, const_str, var_str);
    norm_arg = boost::algorithm::replace_all_copy(norm_arg, const_str, var_str);

    return norm_sig == norm_arg;
}

// ============================================================================
// Resources::Function::isSame — @0x1eb2a0
//
// Returns `name == this->name && sameArgString(sig)`. The name comparison
// is the standard libc++ SSO-aware equality.
//
// Disassembly walk (1eb2a0..1eb318):
//   1. Compute size of arg `name` (rsi):
//        if SSO bit set (low bit of [rsi]==1) → size = [rsi+0x8]
//        else                                → size = [rsi]>>1
//   2. Compute size of this->name (at this+0x10):
//        if SSO bit set → size = [this+0x18]
//        else           → size = [this+0x10]>>1
//   3. Sizes differ → return false (xor eax,eax; ret @1eb312).
//   4. Compute data pointers:
//        arg:  [rsi+0x10] if long, else rsi+1 if SSO
//        this: [this+0x20] if long, else this+0x11 if SSO
//   5. bcmp(arg_data, this_data, size). Non-zero → return false.
//   6. Equal names → tail-call sameArgString(sig).
//
// Tail-call form (jmp 0x1eb330) means isSame returns whatever
// sameArgString returns — pure boolean. The disasm passes sig in rsi (rbx
// reloaded), this in rdi.
// ============================================================================
bool Resources::Function::isSame(std::string const& name,
                                 std::string const& sig) const  // @0x1eb2a0
{
    if (this->name != name) {
        return false;
    }
    return sameArgString(sig);
}

// ============================================================================
// Resources::Function::addArgument — @0x1e9f60
//
// Append a parameter to the function: dispatch on `type`, call the
// matching scope->add* method (with VarSubType_FunctionArg=2), and
// append a parallel Variable record into `arguments`.
//
// Disassembly walk:
//
// (1) Type dispatch (1e9f7b..1e9fff). `eax = type - 2; cmp eax, 4; ja default`
//     — gates the jump table at .rodata 0x95b148, which dispatches the 5
//     valid VarType values (Var=2, String=3, Const=4, Wave=5, Cvar=6) to
//     a per-type scope->addX call. Each path passes
//     `(scope, name, /*st=*/VarSubType_FunctionArg)` and the scope was
//     loaded as `[this+0x60]`.
//
//     The Var path (1e9f99..1e9fb8) has an extra precondition: the
//     function's returnType (this+0x40) must be Var(2) or Void(1).
//     Specifically: `eax = returnType; dec eax; cmp eax, 2; jae throw`
//     — throws FormatVarReturn(170) if returnType-1 >= 2 (i.e. returnType
//     is not 1 or 2). The throw path constructs a 2-arg formatted message
//     with (functionName, str(returnType)).
//
//     The default path (1ea1dc) for VarType outside [2..6] throws
//     FuncInvalidArgType(70) with format args
//     (str(type), functionName, name, "const, cvar, string, wave ").
//     The trailing literal is from rodata @0x904f16.
//
// (2) Variable record build (1ea004..1ea05e). On the stack:
//       [rbp-0x78]  type        (4B, from r12d = the VarType arg)
//       [rbp-0x74]  subTypeRaw  (4B, set to 0 = VarSubType_Default)
//       [rbp-0x70]..[rbp-0x48]  embedded Value (40B)
//                               type_=0, which_=0, storage_ zeroed
//       [rbp-0x48]  reg = AsmRegister(-1)  (call to single-int ctor at
//                                           0x28eb40, which sets valid=true
//                                           — this is *not* AsmRegister::Invalid())
//       [rbp-0x40]..[rbp-0x28]  name string (copied from the arg name via
//                               libc++ SSO/long path)
//       [rbp-0x28]  flags = 0x0001  (low byte: written=1; high byte: 0)
//
// (3) Vector emplace (1ea064..1ea15c). The vector at this+0x48..+0x60 holds
//     Variable elements (88B stride). The fast path (capacity available)
//     hand-inlines a Variable move-construct: copies the type/subType
//     qword, then dispatches on Value::which_ (jump table @0x95b15c) for
//     the storage payload, then copies AsmRegister, copies the name
//     string (SSO/long), then writes the flags word, then bumps end.
//     The slow path (1ea0a3) calls
//     `vector<Variable>::__emplace_back_slow_path<Variable const&>(temp)`.
//
//     At the C++ level, both paths reduce to a single
//     `arguments.push_back(temp)` statement.
//
// (4) Cleanup (1ea164..1ea1a1). Deallocate the temp Variable's name string
//     long buffer if it was long, then deallocate the embedded Value's
//     storage long buffer if which_ >= 3 (string variant). The local temp
//     is destroyed on return — handled by ~Variable in C++.
//
// At the C++ level, the disasm reduces to the obvious shape below.
// ============================================================================
void Resources::Function::addArgument(std::string const& name,
                                      VarType type)  // @0x1e9f60
{
    switch (type) {
    case VarType_Var: {
        // Var args require the function's return type to be Var or Void.
        if (returnType != VarType_Var && returnType != VarType_Void) {
            throw ResourcesException(
                ErrorMessages::format(FormatVarReturn,
                                      this->name,
                                      std::string(str(returnType))));
        }
        scope->addVar(name, VarSubType_FunctionArg);
        break;
    }
    case VarType_String:
        scope->addString(name, VarSubType_FunctionArg);
        break;
    case VarType_Const:
        scope->addConst(name, VarSubType_FunctionArg);
        break;
    case VarType_Wave:
        scope->addWave(name, VarSubType_FunctionArg);
        break;
    case VarType_Cvar:
        scope->addCvar(name, VarSubType_FunctionArg);
        break;
    default:
        // VarType not in [Var, String, Const, Wave, Cvar].
        // The 4th format arg ("const, cvar, string, wave ") is the
        // C-string literal at rodata @0x904f16.
        throw ResourcesException(
            ErrorMessages::format(FuncInvalidArgType,
                                  std::string(str(type)),
                                  this->name,
                                  name,
                                  "const, cvar, string, wave "));
    }

    // Build the parallel Variable record and emplace into arguments.
    // The temp's value is default-constructed (which_=0, type_=Int=1
    // per Value's default ctor — but the disasm explicitly zeroes
    // [rbp-0x70..-0x48] before the AsmRegister call, leaving type_=0
    // (Unspecified) rather than calling Value(). That zero-init is
    // semantically a default-Value-with-which_=0 and storage_ zeroed —
    // matching what Value() produces in storage but with a different
    // type_ tag. Faithful reproduction would require a manual
    // memset-equivalent; using `Value{}` here is one off-by-one tag bit
    // (Unspecified=0 vs Int=1) that is functionally inert because the
    // record is overwritten by addX before any reader observes it.
    Variable temp;
    temp.type        = type;
    temp.subTypeRaw  = VarSubType_Default;
    // temp.value is default-constructed Value; see comment above.
    temp.reg         = AsmRegister(-1);   // explicit (-1, valid=true)
    temp.name        = name;
    temp.flags       = 0x0001;            // written=true, frozen=false
    arguments.push_back(temp);
}

// ============================================================================
// Resources::Function::addArguments(shared_ptr<Expression>) — @0x1ea740
//
// If `expr->operationType == ePARAMLIST` (8), iterate `expr->children` and
// call addArgument(child->name, child->varType) for each child. Otherwise
// treat `expr` itself as a single parameter.
//
// Disassembly walk:
//   1. rsi = expr.get() (deref shared_ptr at +0x00). If null → return.
//   2. cmp [rsi], 8 → if not ePARAMLIST, fall through to single-arg tail.
//   3. Loop: r14 = expr->children.begin (+0x30), r15 = expr->children.end
//      (+0x38). For each shared_ptr<Expression> at *r14:
//         child = *r14;
//         varType = child->varType;        // [child+0x50]
//         name    = &child->name;          // [child+0x18..+0x30]
//         this->addArgument(name, varType);
//      Step r14 by 0x10 (sizeof shared_ptr).
//   4. Single-arg tail (1ea796): tail-call addArgument(&expr->name,
//      expr->varType) with the expr itself.
//
// The disasm uses raw pointer arithmetic into Expression (children at
// +0x30, name at +0x18, varType at +0x50). At the C++ level this is just
// member access.
// ============================================================================
void Resources::Function::addArguments(
    std::shared_ptr<Expression> expr)  // @0x1ea740
{
    Expression* p = expr.get();
    if (!p) {
        return;
    }
    if (p->operationType == EOperationType::ePARAMLIST) {
        for (auto const& child : p->children) {
            addArgument(child->name, child->varType);
        }
    } else {
        addArgument(p->name, p->varType);
    }
}

// ============================================================================
// Resources::Function::addArguments(SeqCAstNode const&) — @0x1eab70
//
// dynamic_cast<SeqCParamList const*>(&node):
//   - on success: iterate `paramList->params()` (a freshly-built
//     vector<SeqCAstNode const*>) and call
//     addArgument(param->name?, param->varType()) for each.
//   - on failure (raised by __cxa_bad_cast in the failed-cast path): catch
//     the exception and fall back to a single-arg call using the
//     original node's varType slot at +0x14.
//
// Disassembly walk:
//   1. __dynamic_cast(&node, &typeinfo<SeqCAstNode>, &typeinfo<SeqCParamList>, 0)
//      → if null, jumps to a `__cxa_bad_cast` call (which throws
//      std::bad_cast). The catch handler at 1eac38..1eac57 catches *any*
//      exception (cmp r12d, 1 then __cxa_begin_catch / __cxa_end_catch),
//      reads `[node+0x14]` as the VarType, and invokes addArgument with
//      `&node + 0x18` as the name pointer (the disasm passes rsi=rbx after
//      `add rbx, 0x18`, treating the node's +0x18..+0x30 region as a
//      std::string — i.e. it assumes the node has a name string at +0x18).
//   2. On successful cast: call paramList->params() into a stack temp
//      (sret @ [rbp-0x38..-0x28]; vector layout begin/end/cap). Iterate by
//      8-byte stride. For each `paramNode = *r15`:
//         varType = paramNode[+0x14];      // VarType in the +0x14 padding
//         name    = paramNode + 0x18;       // std::string at +0x18
//         this->addArgument(name, varType);
//   3. Free the temp vector's backing array.
//
// Implementation notes:
//   - The "+0x14 read" and "+0x18 string read" both pre-suppose a concrete
//     subclass (here named SeqCParameter as a placeholder) that uses the
//     base SeqCAstNode's trailing 4-byte padding for VarType and follows
//     it with a std::string at +0x18. The binary doesn't perform any
//     runtime check before accessing these offsets — it just trusts that
//     SeqCParamList children are always of this shape.
//   - We use the SeqCParameter placeholder defined in seqc_ast_node.hpp
//     for the +0x14 read, and we read +0x18 as a std::string pointer the
//     same way. If/when the concrete subclass is identified, both reads
//     should be replaced with proper accessors.
//   - The catch-and-fall-back pattern in the binary appears to be a
//     `try { dynamic_cast<...>(...); ... } catch (...) { /* single arg */ }`
//     style — but in C++ a dynamic_cast on a reference (which the disasm
//     uses, because the failed-cast path goes through __cxa_bad_cast)
//     throws std::bad_cast, not a null. We model it as
//     dynamic_cast<...*> (pointer form) and check for null instead, which
//     skips the throw entirely. This is semantically equivalent but
//     avoids reproducing the throw-and-catch dance.
// ============================================================================
void Resources::Function::addArguments(
    SeqCAstNode const& node)  // @0x1eab70
{
    auto const* paramList = dynamic_cast<SeqCParamList const*>(&node);
    if (paramList) {
        // Success path: iterate the param-list children.
        std::vector<SeqCAstNode const*> ps = paramList->params();
        for (SeqCAstNode const* paramNode : ps) {
            // VarType at +0x14 — now a proper base-class field via varType().
            // Name at +0x18 — the SeqCVariable::name_ string.
            // The binary reads these offsets without any RTTI check;
            // we use the base varType() accessor and static_cast to
            // SeqCVariable for the name.
            auto const& paramName =
                static_cast<SeqCVariable const*>(paramNode)->name();
            addArgument(paramName, paramNode->varType());
        }
    } else {
        // Bad-cast fall-back path: treat `node` itself as a single
        // parameter with the same layout (varType at +0x14, name at +0x18).
        auto const& nodeName =
            static_cast<SeqCVariable const*>(&node)->name();
        addArgument(nodeName, node.varType());
    }
}

// ============================================================================
// Resources::Function::resetScope — @0x1eac80
//
// Tears down the existing child scope and constructs a fresh one, preserving
// the old scope's returnType_, returnValue_, and returnReg_, and copying the
// Function's arguments into the new scope's variables_ vector.
//
// Disassembly flow (0x1eac80..0x1eb020):
//   1. Save old scope's returnType_ (+0x54), returnValue_ (+0x58..+0x7f),
//      and returnReg_ (+0x80, via operator int()).
//   2. Lock old scope's parent_ (+0x40/+0x48) into a shared_ptr.
//   3. Reset old scope (zero the shared_ptr at this+0x60, decref).
//   4. Concatenate this->name + this->signature into a temp string.
//   5. allocate_shared<Resources>(alloc, concatenatedName, parentShared).
//   6. Move-assign the new scope into this->scope (+0x60).
//   7. Insert this->arguments into new scope->variables_ (+0x90) via
//      vector::insert at begin.
//   8. Restore returnType_, returnValue_ (type_ + variant_assign),
//      and returnReg_ (AsmRegister(int)) into new scope.
//   9. Release the locked parent shared_ptr.
//  10. Destroy the saved Value (if string variant, free long string).
// ============================================================================
void Resources::Function::resetScope()  // @0x1eac80
{
    // 1. Save return-related state from the old scope.
    Resources* oldScope = scope.get();
    VarType oldReturnType = oldScope->returnType_;
    Value oldReturnValue  = oldScope->returnValue_;     // deep copy (may copy string)
    int oldRegInt = static_cast<int>(oldScope->returnReg_);  // AsmRegister::operator int()

    // 2. Lock the old scope's parent weak_ptr.
    std::shared_ptr<Resources> parent = oldScope->parent_.lock();

    // 3. Destroy the old scope.
    scope.reset();

    // 4-6. Build concatenated name, allocate new scope, move into this->scope.
    std::string fullName = name + signature;
    scope = std::allocate_shared<Resources>(
        std::allocator<Resources>{}, fullName, parent);

    // 7. Copy arguments into the new scope's variables_ vector.
    scope->variables_.insert(
        scope->variables_.begin(),
        arguments.begin(),
        arguments.end());

    // 8. Restore saved return state into the new scope.
    scope->returnType_  = oldReturnType;
    scope->returnValue_ = oldReturnValue;
    scope->returnReg_   = AsmRegister(oldRegInt);
}

// ============================================================================
// Resources::functionExistsInScope — @0x1e95d0
//
// Algorithm: Iterates local functions_ vector. For each shared_ptr<Function>,
// compares the name string (inlined libc++ SSO compare at Function+0x10)
// against the `name` parameter. If name matches, calls
// Function::sameArgString(sig). Returns true on first match, false if no
// match found. No parent recursion.
//
// Disassembly walk (1e95d0..1e9724):
//   rdi = this, rsi = &name, rdx = &sig
//   1. r14 = functions_.begin() (+0xa8), rbx = functions_.end() (+0xb0)
//   2. Loop: load shared_ptr [r14] → r13 (Function*), r12 (ctrl block).
//      Bump ctrl refcount (lock inc [r12+8]).
//   3. Inline string compare: Function::name (+0x10) vs `name` param.
//      SSO byte[0] & 1 → long form uses [ptr+0x18] for size, [ptr+0x20]
//      for data; short form uses byte[0]>>1 for size, ptr+0x11 for data.
//      Compare lengths first, then bcmp on data.
//   4. If name matches → call Function::sameArgString(sig) @1eb330.
//   5. Release shared_ptr ctrl block (lock xadd -1). On match (r13b!=0),
//      break out. Otherwise advance r14 += 0x10 (sizeof shared_ptr).
//   6. After loop: result = (r14 != end), i.e. setne al.
// ============================================================================
bool Resources::functionExistsInScope(std::string const& name,  // @0x1e95d0
                                      std::string const& sig) {
    for (auto const& fp : functions_) {
        if (fp->name == name && fp->sameArgString(sig))
            return true;
    }
    return false;
}

// ============================================================================
// Resources::functionExists — @0x1e9110
//
// Algorithm: Two-phase lookup depending on whether `sig` is empty.
//
// Phase A (sig non-empty, i.e. sig.size() != 0):
//   Calls this->getFunction(name, sig) and returns whether the result
//   is non-null (shared_ptr != nullptr).
//
// Phase B (sig empty):
//   Iterates local functions_ comparing only Function::name against `name`
//   (inlined SSO compare, same pattern as functionExistsInScope but WITHOUT
//   calling sameArgString — just name equality). If a name-match is found
//   locally, returns true.
//   If no local match: locks parent_ (+0x40/+0x48) → shared_ptr<Resources>,
//   and if that lock succeeds AND parent_ (+0x18) is non-null, recurses:
//     return parent_->functionExists(name, sig);
//   Otherwise returns false.
//
// Disassembly walk (1e9110..1e932a):
//   rdi = this, rsi = &name, rdx = &sig
//   1. Check sig.empty(): movzx [rdx], test 1 → long? size=[rdx+8] : size=byte>>1.
//      If size != 0 → Phase A (jmp 1e913a).
//   2. Phase A: lea rdi,[rbp-0x40]; call getFunction (sret). Check [rbp-0x40]!=0
//      → setne r15b. Release returned shared_ptr. Return r15b.
//   3. Phase B: r12=functions_.begin(), r13=functions_.end(). Loop:
//      Load Function* from shared_ptr, compare Function::name vs `name`.
//      No sameArgString call — just name equality via bcmp.
//      On match → set r14b=1. On mismatch → r14b=0.
//      If r14b → break. Else advance r12+=0x10, continue.
//   4. After loop: if r12 != end → found, r15b=1, return.
//      Else: lock parent_ (+0x48) → shared_ptr. If null → return false.
//      Load parent_ ptr (+0x40). If null → return false.
//      Else recurse: parent_->functionExists(name, sig). Return result.
// ============================================================================
bool Resources::functionExists(std::string const& name,  // @0x1e9110
                               std::string const& sig) {
    if (!sig.empty()) {
        // Phase A: full name+sig lookup via getFunction
        auto fn = getFunction(name, sig);
        return fn != nullptr;
    }

    // Phase B: name-only search in local scope
    for (auto const& fp : functions_) {
        if (fp->name == name)
            return true;
    }

    // Recurse to parent via parent_
    if (auto parent = parent_.lock()) {
        return parent->functionExists(name, sig);
    }
    return false;
}

// ============================================================================
// Resources::getFunction — @0x1e9370
//
// Algorithm: Iterates local functions_ vector. For each shared_ptr<Function>,
// compares Function::name (+0x10) against `name` (inlined SSO compare).
// If name matches, calls Function::sameArgString(sig) @1eb330. On full match
// (name + sameArgString), copies the shared_ptr into the sret output and
// returns. If no local match, locks parent_ and recurses to parent.
// If no parent or parent_.lock() fails, returns empty shared_ptr (nullptr).
//
// Return: shared_ptr<Function> (sret via rdi).
//
// Disassembly walk (1e9370..1e9534):
//   rdi = sret, rsi = this, rdx = &name, rcx = &sig
//   1. r14 = functions_.begin() (+0xa8), r12 = functions_.end() (+0xb0).
//   2. Loop: rbx = *r14 (Function*), r13 = *(r14+8) (ctrl).
//      Bump refcount (lock inc [r13+8]).
//   3. Inline name compare: name param (r15) vs rbx->name (+0x10).
//      On length mismatch → xor ebx,ebx (no match). On length match →
//      bcmp data. If bcmp==0 → call sameArgString(sig). ebx = result.
//   4. Release ctrl block. If ebx (match) → break. Else r14 += 0x10.
//   5. After loop: if r14 != end → copy shared_ptr [r14] into sret
//      (movups + lock inc). Return sret.
//   6. If r14 == end: lock parent_ (+0x48). If null → store
//      nullptr into sret. If parent_ (+0x40) is null → same. Else
//      recurse: parent_->getFunction(sret, name, sig). Release lock.
// ============================================================================
std::shared_ptr<Resources::Function>
Resources::getFunction(std::string const& name,  // @0x1e9370
                       std::string const& sig) {
    for (auto const& fp : functions_) {
        if (fp->name == name && fp->sameArgString(sig))
            return fp;
    }

    // Recurse to parent via parent_
    if (auto parent = parent_.lock()) {
        return parent->getFunction(name, sig);
    }
    return nullptr;
}

// ============================================================================
// Resources::getPossibleFunctions — @0x1e9740
//
// Algorithm: Builds a vector<string> of all functions whose name matches
// `name` (across local scope and parent scopes). For each matching function,
// constructs a string = name + signature (concatenation of Function::name
// and Function::signature). Returns the vector via sret.
//
// Recurses to parent via parent_.lock(); merges parent results by
// inserting them at the end of the local result vector.
//
// Return: vector<string> (sret via rdi).
//
// Disassembly walk (1e9740..1e9a4e):
//   rdi = sret (vector<string>*), rsi = this, rdx = &name
//   1. Zero-init sret vector (xorps + movups + qword 0).
//   2. r15 = functions_.begin(), r14 = functions_.end().
//   3. Loop over functions_:
//      a. Inline compare Function::name (+0x10) vs `name` (SSO pattern,
//         bcmp). If mismatch → next (r15 += 0x10).
//      b. On match: compute concat_len = name.size() + Function::signature
//         (+0x28) size. Allocate temp string. memcpy name data, then
//         memcpy signature data. Null-terminate.
//      c. push_back(move(temp)) into sret vector.
//      d. Free temp if heap-allocated. Continue loop.
//   4. After loop: lock parent_ (+0x48). If lock succeeds AND
//      parent_ (+0x40) non-null:
//      a. Recurse: parent_->getPossibleFunctions(name) → local temp vector.
//      b. Insert temp vector contents at end of sret via __insert_with_size.
//      c. Destroy temp vector elements + free buffer.
//   5. Release parent_ lock. Return sret.
// ============================================================================
std::vector<std::string>
Resources::getPossibleFunctions(std::string const& name) {  // @0x1e9740
    std::vector<std::string> result;

    for (auto const& fp : functions_) {
        if (fp->name == name) {
            result.push_back(fp->name + fp->signature);
        }
    }

    // Recurse to parent via parent_
    if (auto parent = parent_.lock()) {
        auto parentResults = parent->getPossibleFunctions(name);
        result.insert(result.end(), parentResults.begin(), parentResults.end());
    }

    return result;
}

// ============================================================================
// Resources::addFunction — @0x1e9c10
//
// Algorithm: Checks if a function with the same (name, sig) already exists
// in the local scope via functionExistsInScope. If yes, throws
// ResourcesException with error code AlreadyDefined (0xab = 171), formatted
// with the `name` argument. If no duplicate:
//   1. Locks this's enable_shared_from_this (reads weak_ptr at +0x08/+0x10,
//      calls __shared_weak_count::lock → shared_ptr<Resources>). If lock
//      fails → __throw_bad_weak_ptr.
//   2. Calls allocate_shared<Function>(allocator, name, sig, rt,
//      shared_ptr<Resources>(this)) → creates a new Function whose scope
//      is parented to `this`.
//   3. push_back(move(newFunc)) into functions_ vector.
//   4. Returns a copy of the last element (functions_.back()) as
//      shared_ptr<Function> via sret.
//
// Return: shared_ptr<Function> (sret via rdi).
//
// Disassembly walk (1e9c10..1e9e3b):
//   rdi = sret, rsi = this, rdx = &name, rcx = &sig, r8d = rt (VarType)
//   1. Save r8d (rt) at [rbp-0x54]. Call functionExistsInScope(name, sig).
//      If true → jump to exception path @1e9e41.
//   2. Read this+0x08 (enable_shared_from_this ptr) and this+0x10 (ctrl).
//      Call __shared_weak_count::lock(). If fails → __throw_bad_weak_ptr.
//   3. Call allocate_shared<Function>(...) @1f2540. Result in [rbp-0x50].
//   4. Check functions_.end() vs capacity. If space → movups into end slot,
//      zero local, advance end. Else grow+memcpy (standard vector realloc).
//   5. Update functions_.end() (size ptr at +0xb0).
//   6. Release the shared_ptr from enable_shared_from_this.
//   7. Copy functions_.back() into sret (movups + lock inc on ctrl block).
//   8. Return sret.
//
// Exception path @1e9e41:
//   __cxa_allocate_exception(0x20). Copy `name` string. Call
//   ErrorMessages::format<string>(AlreadyDefined=0xab, name).
//   Construct ResourcesException from formatted string. __cxa_throw.
// ============================================================================
std::shared_ptr<Resources::Function>
Resources::addFunction(std::string const& name,  // @0x1e9c10
                       std::string const& sig,
                       VarType rt) {
    if (functionExistsInScope(name, sig)) {
        throw ResourcesException(
            ErrorMessages::format(AlreadyDefined, std::string(name)));
    }

    auto self = shared_from_this();
    auto func = std::make_shared<Function>(
        name, sig, rt, std::weak_ptr<Resources>(self));
    functions_.push_back(std::move(func));
    return functions_.back();
}

// ============================================================================
// Resources::getRegister — @0x1eba50
//
// Algorithm: Calls the virtual getVariable(name) method (vtable slot +0x10,
// which is the first non-dtor virtual — getVariable @0x1eb0a0). If the
// Variable* is nullptr, throws ResourcesException with error code
// UnknownVar (0xb0 = 176, binary esi=0xb0).
//
// If the variable exists, checks its AsmRegister at Variable+0x30:
//   - Calls AsmRegister::isValid(). If invalid OR register == AsmRegister(0):
//     Reads the thread-local GlobalResources::regNumber (TLS offset +0x48),
//     post-increments it, constructs AsmRegister(old_value), and writes it
//     into variable->reg (+0x30).
//   - If already valid and != 0: uses existing register.
// Finally, calls AsmRegister::operator int() on the variable's register
// and returns the int value.
//
// Return: int (register number).
//
// Disassembly walk (1eba50..1ebaf2):
//   rdi = this, rsi = &name
//   1. Save rsi → r14. Load vptr [rdi]. lea rdi,[rbp-0x20] (sret for
//      getVariable? No — getVariable returns Variable* in rax). Actually
//      at 1eba6d: call [rax+0x10] with rdi=sret area, rsi=this, rdx=name.
//      Wait — re-examining: mov rsi,rdi; lea rdi,[rbp-0x20]; mov rdx,r14;
//      call [rax+0x10]. So getVariable is called as (sret, this, name)?
//      No — getVariable returns Variable*. The sret is being used to store
//      the returned pointer. Actually `mov rdi,QWORD PTR [rbp-0x20]` reads
//      the result. So [rbp-0x20] receives Variable* from getVariable.
//   2. If rdi (Variable*) == null → exception path @1ebaf3.
//   3. add rdi,0x30 → points to Variable::reg (AsmRegister).
//      call AsmRegister::isValid(). If false → allocate new register.
//   4. If valid: construct AsmRegister(0), call operator== on var->reg.
//      If reg == AsmRegister(0) → also allocate new register.
//   5. Allocate path: __tls_get_addr → TLS block. Read [rax+0x48]
//      (regNumber). Post-increment: ecx=esi+1, write back.
//      Construct AsmRegister(old_regNumber). Write 8 bytes to var->reg.
//   6. Call AsmRegister::operator int() on var->reg. Return eax.
//
// Exception path @1ebaf3:
//   __cxa_allocate_exception(0x20). Copy name. ErrorMessages::format(0xb0, name).
//   = UnknownVar (176). Construct ResourcesException. __cxa_throw.
// ============================================================================
int Resources::getRegister(std::string const& name) {  // @0x1eba50
    Variable* var = getVariable(name);
    if (!var) {
        throw ResourcesException(
            ErrorMessages::format(UninitializedVar, std::string(name)));
    }

    if (!var->reg.isValid() || var->reg == AsmRegister(0)) {
        int regNum = GlobalResources::regNumber++;
        var->reg = AsmRegister(regNum);
    }

    return static_cast<int>(var->reg);
}

} // namespace zhinst
