// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmExpression — destructor and factory free functions
//
// Binary addresses noted in comments next to each function.
// ============================================================================

#include "zhinst/asm_expression.hpp"

namespace zhinst {

// ============================================================================
// AsmExpression::~AsmExpression() — 0x28b1f0
//
// Destroys fields in reverse layout order:
//   1. If hasComment (+0x98): destroy comment string at +0x80
//   2. If hasLabel (+0x78): destroy labelName string at +0x60
//   3. Destroy children vector at +0x40 (release shared_ptrs, free buffer)
//   4. Destroy str2 string at +0x20
//   5. Destroy name string at +0x08
//
// The binary checks the optional bool guards before touching the
// corresponding strings, matching std::optional<std::string> semantics.
// For the children vector, it iterates from end to begin, releasing
// each shared_ptr's control block (atomic decrement + destroy if zero),
// then frees the vector buffer.
// ============================================================================
AsmExpression::~AsmExpression() {  // 0x28b1f0
    // hasComment (+0x98) check: destroy comment string at +0x80
    // hasLabel (+0x78) check: destroy labelName string at +0x60
    // children vector (+0x40): iterate backward, release shared_ptrs, free buf
    // str2 (+0x20): destroy string
    // name (+0x08): destroy string
    //
    // All handled by compiler-generated member destructors in the correct
    // order. The binary manually inlines the destruction for optional fields.
}

// ============================================================================
// createValue(int value) — 0x28bb90
//
// Allocates 0xa8 bytes, sets type=3, zeros fields, sets value at +0x3C.
// ============================================================================
AsmExpression* createValue(int value) {  // 0x28bb90
    AsmExpression* expr = new AsmExpression();
    expr->type = 3;         // +0x00 = 3 (integer type)
    expr->value = value;    // +0x3C
    return expr;
}

// ============================================================================
// createRegister(int regNum) — 0x28bbf0
//
// Allocates 0xa8 bytes, sets type=1, zeros fields, sets value at +0x3C.
// ============================================================================
AsmExpression* createRegister(int regNum) {  // 0x28bbf0
    AsmExpression* expr = new AsmExpression();
    expr->type = 1;         // +0x00 = 1 (register type)
    expr->value = regNum;   // +0x3C
    return expr;
}

// ============================================================================
// createName(const char* name) — 0x28bc50
//
// Builds a std::string from the C string. If the resulting string is empty,
// returns nullptr. Otherwise allocates 0xa8 bytes, sets type=2, and moves
// the string into expr->name (+0x08).
// ============================================================================
AsmExpression* createName(const char* name) {  // 0x28bc50
    std::string s(name);
    if (s.empty()) {
        return nullptr;
    }

    AsmExpression* expr = new AsmExpression();
    expr->type = 2;         // +0x00 = 2 (label/name type)
    expr->name = std::move(s);  // +0x08
    return expr;
}

// ============================================================================
// createArgList(AsmExpression* first) — 0x28bdc0
//
// Allocates a new container AsmExpression (type=0), wraps `first` in a
// shared_ptr, and pushes it into the children vector at +0x40.
//
// The binary:
//   1. operator new(0xa8) → zero-init all fields, type=0
//   2. If first != null:
//      a. operator new(0x20) → create shared_ptr control block
//      b. Set control block vtable, store `first` pointer at +0x18
//      c. Push the shared_ptr (data=first, ctrl=block) into children vector
//      d. Release the local shared_ptr (atomic decrement)
//   3. Return the new container expression.
// ============================================================================
AsmExpression* createArgList(AsmExpression* first) {  // 0x28bdc0
    AsmExpression* list = new AsmExpression();
    list->type = 0;

    if (first) {
        // Wrap first in shared_ptr and push into children
        std::shared_ptr<AsmExpression> child(first);
        list->children.push_back(std::move(child));
    }

    return list;
}

// ============================================================================
// appendArgList(AsmExpression* list, AsmExpression* child) — 0x28bec0
//
// Appends `child` (wrapped in shared_ptr) to `list`'s children vector.
// If `list` is null, allocates a fresh container (type=0) first.
// If `child` is null, skips the append and just returns list.
//
// The binary:
//   1. If list == null: operator new(0xa8) → zero-init, type=0
//   2. If child != null:
//      a. Create shared_ptr control block (0x20 bytes)
//      b. Push shared_ptr into list->children (+0x40)
//      c. Release local shared_ptr
//   3. Return list.
// ============================================================================
AsmExpression* appendArgList(AsmExpression* list,  // 0x28bec0
                             AsmExpression* child) {
    if (!list) {
        list = new AsmExpression();
        list->type = 0;
    }

    if (child) {
        std::shared_ptr<AsmExpression> ptr(child);
        list->children.push_back(std::move(ptr));
    }

    return list;
}

}  // namespace zhinst
