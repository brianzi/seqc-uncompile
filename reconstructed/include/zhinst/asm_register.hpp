// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmRegister — register identifier for the AWG processor
// ============================================================================
#pragma once

#include <cstdint>

namespace zhinst {

// AsmRegister — 8-byte struct: {int value; bool valid; 3 bytes padding}
//
// Confirmed from disassembly: stored as 8 bytes in AssemblerInstr and Node
// fields. The valid flag is separate from the value (not just value == -1).
// Common pattern: valid=false, value=-1 for "no register".
struct AsmRegister {
    int value = -1;      // +0x00: register number (0-15), or -1 if unset
    bool valid = false;  // +0x04: whether this register slot is active
    // +0x05: 3 bytes padding to 8-byte alignment

    AsmRegister() = default;
    AsmRegister(int v, bool val) : value(v), valid(val) {}
    AsmRegister(int n) : value(n), valid(true) {}  // convenience ctor used throughout codebase

    // Named constants for convenience
    static AsmRegister Invalid() { return {-1, false}; }
    static AsmRegister Reg(int n) { return {n, true}; }

    bool isValid() const { return valid; }
    int toInt() const { return value; }
    explicit operator int() const { return value; }

    bool operator==(const AsmRegister& o) const {
        return value == o.value && valid == o.valid;
    }
    bool operator!=(const AsmRegister& o) const { return !(*this == o); }
};
static_assert(sizeof(AsmRegister) == 8, "AsmRegister must be 8 bytes");

// Free-function wrappers — many .cpp files call isValid(reg) and toInt(reg)
// as free functions rather than member calls.
inline bool isValid(AsmRegister r) { return r.isValid(); }
inline int toInt(AsmRegister r) { return r.toInt(); }

} // namespace zhinst
