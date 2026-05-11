// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmExpression — destructor and factory free functions
//
// Binary addresses noted in comments next to each function.
// ============================================================================

#include "zhinst/asm/asm_expression.hpp"

#include <sstream>
#include <string>

#include "zhinst/asm/assembler.hpp"
#include "zhinst/infra/logging.hpp"

namespace zhinst {

// ============================================================================
// AsmExpression::~AsmExpression() — 0x28b1f0
//
// Destroys fields in reverse layout order:
//   1. If hasComment (+0x98): destroy comment string at +0x80
//   2. If hasLabel (+0x78): destroy labelName string at +0x60
//   3. Destroy children vector at +0x40 (release shared_ptrs, free buffer)
//   4. Destroy nopComment string at +0x20
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
    // nopComment (+0x20): destroy string
    // name (+0x08): destroy string
    //
    // All handled by compiler-generated member destructors in the correct
    // order. The binary manually inlines the destruction for optional fields.
}

// ============================================================================
// createValue(int value) — 0x28bb90
//
// Allocates 0xa8 bytes, sets type=Integer, zeros fields, sets value at +0x3C.
// ============================================================================
AsmExpression* createValue(int value) {  // 0x28bb90
    AsmExpression* expr = new AsmExpression();
    expr->type = AsmExprType::Integer;
    expr->value = value;    // +0x3C
    return expr;
}

// ============================================================================
// createRegister(int regNum) — 0x28bbf0
//
// Allocates 0xa8 bytes, sets type=Register, zeros fields, sets value at +0x3C.
// ============================================================================
AsmExpression* createRegister(int regNum) {  // 0x28bbf0
    AsmExpression* expr = new AsmExpression();
    expr->type = AsmExprType::Register;
    expr->value = regNum;   // +0x3C
    return expr;
}

// ============================================================================
// createName(const char* name) — 0x28bc50
//
// Builds a std::string from the C string. If the resulting string is empty,
// returns nullptr. Otherwise allocates 0xa8 bytes, sets type=Label, and moves
// the string into expr->name (+0x08).
// ============================================================================
AsmExpression* createName(const char* name) {  // 0x28bc50
    std::string s(name);
    if (s.empty()) {
        return nullptr;
    }

    AsmExpression* expr = new AsmExpression();
    expr->type = AsmExprType::Label;
    expr->name = std::move(s);  // +0x08
    return expr;
}

// ============================================================================
// createArgList(AsmExpression* first) — 0x28bdc0
//
// Allocates a new container AsmExpression (type=Container), wraps `first` in a
// shared_ptr, and pushes it into the children vector at +0x40.
//
// The binary:
//   1. operator new(0xa8) → zero-init all fields, type=Container
//   2. If first != null:
//      a. operator new(0x20) → create shared_ptr control block
//      b. Set control block vtable, store `first` pointer at +0x18
//      c. Push the shared_ptr (data=first, ctrl=block) into children vector
//      d. Release the local shared_ptr (atomic decrement)
//   3. Return the new container expression.
// ============================================================================
AsmExpression* createArgList(AsmExpression* first) {  // 0x28bdc0
    AsmExpression* list = new AsmExpression();
    list->type = AsmExprType::Container;

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
// If `list` is null, allocates a fresh container (type=Container) first.
// If `child` is null, skips the append and just returns list.
//
// The binary:
//   1. If list == null: operator new(0xa8) → zero-init, type=Container
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
        list->type = AsmExprType::Container;
    }

    if (child) {
        std::shared_ptr<AsmExpression> ptr(child);
        list->children.push_back(std::move(ptr));
    }

    return list;
}

// ============================================================================
// str(const std::shared_ptr<AsmExpression>&) — 0x28cd20
//
// Renders one node of an AsmExpression tree into a human-readable form.
//
// Format:
//
//     <head> (<tag>)\n
//       <child0>
//       <child1>
//       ...
//
// where <head> depends on the node's type discriminator:
//
//   AsmExprType::Container (0): commandToString(command)
//   AsmExprType::Register  (1): "R" + value (as decimal int)
//   AsmExprType::Label     (2): name (the std::string field at +0x08)
//   AsmExprType::Integer   (3): value (as decimal int, no prefix)
//   any other value:           emits a Debug log record
//                              "Unknown AsmOperationType with code <n>."
//                              and contributes nothing to the head text.
//
// and <tag> is a short label for the discriminator:
//
//   Container -> "cmd"
//   Register  -> "reg"
//   Label     -> "name"
//   Integer   -> "value"
//   any other -> "?"
//
// Each child in `children` is recursively stringified and prepended
// with two spaces of indentation, on its own line; the recursion is
// straight depth-first with no further indentation tracking, so deeper
// nesting still produces only the leading two spaces per line.
//
// Binary specifics worth noting:
//
//   - The outer dispatch jumps to a 4-entry table at .rodata 0x95d59c
//     covering type values 0..3. Type==3 (Integer) reuses the tail of
//     the type==1 (Register) arm by jumping into it just past the
//     "R" prefix emission, so an Integer's head is the bare value.
//   - The unknown-type arm constructs a `LogRecord(Severity(1))`
//     (== Severity::Debug per the reconstructed enum) and only writes
//     into it when the record was actually opened by the filter,
//     gating each operator<< on cmpq $0, -0x148(%rbp) checks.
//   - The inner dispatch (tag selection) jumps to a second 4-entry
//     table at .rodata 0x95d5ac and writes the small SSO bytes for
//     "cmd"/"reg"/"name"/"value"/"?" directly into a stack-allocated
//     std::string before streaming it; the unknown-type arm writes
//     "?" with SSO size byte 0x02 (size==1).
//   - The children loop iterates while r13 < (end-begin)/16, where
//     +0x40 is begin and +0x48 is end of the libc++ shared_ptr vector
//     (16-byte stride). The recursive call passes &begin[i] (an
//     lvalue reference to the element's shared_ptr), matching the
//     `const std::shared_ptr<AsmExpression>&` parameter form.
//
// All field offsets used here are verified against asm_expression.hpp:
// type @+0x00, name @+0x08, command @+0x38, value @+0x3c, children @+0x40.
// ============================================================================
//! \unverifiable Unverifiable from SeqC: the function has no external
//! caller in the binary (only a recursive self-call inside its body),
//! so no `compile_seqc` input drives a difftest of its output.
//! Reconstructed from disassembly; whitespace and capitalisation
//! cannot be confirmed against any reference output.
std::string str(const std::shared_ptr<AsmExpression>& expr) {  // 0x28cd20
    std::ostringstream out;

    // --- Head: rendering depends on the node type. ---
    switch (expr->type) {
    case AsmExprType::Container:
        // Print the command opcode's textual mnemonic.
        out << Assembler::commandToString(
                static_cast<Assembler::Command>(expr->command));
        break;
    case AsmExprType::Register:
        // Register reference: literal "R" followed by the register
        // index stored in `value`.
        out << "R" << expr->value;
        break;
    case AsmExprType::Label:
        // Label / name token: print the identifier text.
        out << expr->name;
        break;
    case AsmExprType::Integer:
        // Integer immediate: bare decimal value, no prefix.
        out << expr->value;
        break;
    default: {
        // Out-of-range discriminator: log a Debug record and emit no
        // head text. The binary tests the LogRecord's open flag
        // before each streamed token; using natural operator<< here
        // is equivalent because LogRecord::operator<< is itself a
        // no-op when the record was filtered out at construction.
        logging::detail::LogRecord rec(logging::Severity::Debug);
        rec << "Unknown AsmOperationType with code "
            << static_cast<int>(expr->type) << ".";
        break;
    }
    }

    // --- Tag suffix: " (<tag>)\n" identifying the discriminator. ---
    const char* tag;
    switch (expr->type) {
    case AsmExprType::Container: tag = "cmd";   break;
    case AsmExprType::Register:  tag = "reg";   break;
    case AsmExprType::Label:     tag = "name";  break;
    case AsmExprType::Integer:   tag = "value"; break;
    default:                     tag = "?";     break;
    }
    out << " (" << tag << ")\n";

    // --- Children: each rendered on its own indented line. ---
    for (const auto& child : expr->children) {
        out << "  " << str(child);
    }

    return out.str();
}

}  // namespace zhinst
