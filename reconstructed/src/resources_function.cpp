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

#include <cstring>
#include <memory>
#include <string>

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
// fields named `reserved_`/`weakRef_`; the disasm shows it is in fact a
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
    : reserved_(nullptr),
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
    // The ctor's mem-init list above zeroes parentScope (reserved_/weakRef_),
    // copies name + signature, default-constructs the arguments vector, and
    // allocates the scope shared_ptr. The body unique_ptr default-constructs
    // to nullptr. parentScope itself is NOT stored in the
    // reserved_/weakRef_ slots — those are zeroed by the disasm and the
    // captured weak_ptr is consumed by allocate_shared then dropped. The
    // physical weak_ptr layout at this+0x00 is currently "zeroed but unused";
    // see notes/struct_layouts.md for the open question on whether
    // reserved_/weakRef_ should be promoted to a real weak_ptr<Resources>.

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
//   call [rax+0x20]              ; clone() — sret return into rdi
//   mov rax, rbx                 ; return the sret slot
//
// Returns a CLONE of the body, not a borrow. The vtable+0x20 call is
// SeqCAstNode::clone() which returns `unique_ptr<SeqCAstNode>` (16B sret).
// Header was previously declared `SeqCAstNode const* getBody() const`
// (raw borrow); corrected to `unique_ptr<SeqCAstNode>` in Phase 20e-ii
// Batch 5a alongside this body.
//
// NOTE: this implementation deliberately dereferences `body` (which is
// itself a unique_ptr) and calls clone() on the pointee. If body is null
// the disasm crashes (no null check). We faithfully reproduce that — any
// caller that invokes getBody() on a Function with no body installed will
// segfault, matching the binary.
// ============================================================================
std::unique_ptr<SeqCAstNode> Resources::Function::getBody() const  // @0x1eab50
{
    return body->clone();
}

// ============================================================================
// Resources::Function::addBody — @0x1ea7b0
//
// Replace this->body with a clone of the given AST node. Frees the old
// body via its virtual deleting dtor.
//
// Disassembly walk (1ea7b0..1ea7ff happy path):
//   lea  rdi, [rbp-0x10]         ; sret slot for clone() result
//   mov  rax, [rsi]              ; rax = node.vptr
//   call [rax+0x20]              ; node.clone() → unique_ptr return via sret
//   mov  rax, [rbp-0x10]         ; rax = cloned raw ptr
//   mov  qword [rbp-0x10], 0     ; release ownership from temp (move)
//   mov  rdi, [rbx+0x70]         ; old body raw ptr
//   mov  [rbx+0x70], rax         ; install clone into body slot
//   if (rdi != null) call rdi.vtable[+0x30]   ; deleting dtor on old body
//
// The trailing dead block at 1ea7e2..1ea7f8 is the unwind landing pad's
// "double-free guard" — it tests an already-zeroed temp and skips. It is
// not exercised in normal flow; the C++ source `body = node.clone();`
// expands to exactly the happy path above.
//
// catch handler at 1ea800..1ea818: `__cxa_begin_catch; __cxa_end_catch`.
// This is the implicit catch-and-discard for a `try { ... } catch (...) {}`
// surrounding the clone+install — but the disasm only does begin/end with
// no body. In practice this is a noexcept-ish swallow. We don't model it
// in source — the move-assignment of unique_ptr is itself noexcept and
// the only throw point is node.clone(), which would propagate normally
// in real C++. The compiler appears to have generated this from a
// `try { ... } catch (...) {}` in a private helper that wraps the call;
// noted but not reproduced (low risk: differing exception semantics).
// ============================================================================
void Resources::Function::addBody(SeqCAstNode const& node)  // @0x1ea7b0
{
    body = node.clone();
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

} // namespace zhinst
