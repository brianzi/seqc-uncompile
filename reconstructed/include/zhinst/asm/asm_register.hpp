// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmRegister — register identifier for the AWG processor
//
// NOTE: AsmRegister lives at **global scope** (not inside namespace zhinst)
// to match the binary's mangling.  A `using ::AsmRegister;` inside the
// zhinst namespace ensures all existing unqualified uses compile unchanged.
// ============================================================================
#pragma once

#include <cstdint>

// AsmRegister — 8-byte struct: {int value; bool valid; 3 bytes padding}
//
// Confirmed from disassembly: stored as 8 bytes in Assembler and Node
// fields. The valid flag is separate from the value (not just value == -1).
// Common pattern: valid=false, value=-1 for "no register".
struct AsmRegister {
    int value = -1;      // +0x00: register number (0-15), or -1 if unset
    bool valid = false;  // +0x04: whether this register slot is active
    // +0x05: 3 bytes padding to 8-byte alignment

    AsmRegister() = default;
    AsmRegister(int v, bool valid) : value(v), valid(valid) {}
    AsmRegister(int n) : value(n > 0 ? n : 0), valid(n >= 0) {}  // @0x28eb40: cmovg + setns

    // Named constants for convenience
    static AsmRegister Invalid() { return {-1, false}; }
    static AsmRegister Reg(int n) { return {n, true}; }

    // 0x28ebb0 — returns AsmRegister(INT_MAX, true): sentinel used by
    // splitConstRegisters to mark barrier entries that should be stripped.
    static AsmRegister magicSkipRegister() { return {0x7fffffff, true}; }

    bool isValid() const { return valid; }
    explicit operator int() const { return value; }

    bool operator==(const AsmRegister& o) const {
        // Binary semantics (0x28eb80): if both invalid, always equal
        // regardless of value. If both valid, compare values.
        // If one valid and one invalid, not equal.
        if (valid != o.valid) return false;
        if (!valid) return true;  // both invalid → equal
        return value == o.value;  // both valid → compare values
    }
    bool operator!=(const AsmRegister& o) const { return !(*this == o); }
};
static_assert(sizeof(AsmRegister) == 8, "AsmRegister must be 8 bytes");

namespace zhinst {

// Bring AsmRegister into zhinst:: so all existing unqualified uses work.
using ::AsmRegister;

// Free-function wrapper — many .cpp files call isValid(reg) as a free
// function rather than a member call. (Phase S.2 M5: removed the
// matching `toInt(AsmRegister)` wrapper; the binary only defines
// `operator int()`. Use `int(reg)` at call sites instead.)
inline bool isValid(AsmRegister r) { return r.isValid(); }

} // namespace zhinst
