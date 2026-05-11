// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmRegister — register identifier for the AWG processor
//
// AsmRegister lives at **global scope** (not inside namespace zhinst)
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
//! \brief AWG processor register identifier with explicit valid flag.
//!
//! `AsmRegister` is the operand type used everywhere a single-instruction
//! register reference is needed.  It packs a register number with a
//! separate `valid` boolean so an unset slot can be represented
//! unambiguously without overloading a sentinel value: comparisons treat
//! all invalid registers as equal regardless of `value`.  Use the
//! `Invalid()` and `Reg(n)` factories at call sites; the
//! `magicSkipRegister()` factory returns a sentinel used internally by
//! the optimiser's register-allocation pass to mark barrier entries.
struct AsmRegister {
    //! \brief Register number (typically 0–15), or `-1` when unset.
    int value = -1;      // +0x00: register number (0-15), or -1 if unset
    //! \brief `true` when this slot refers to a real register;
    //!        `false` for the default-constructed "no register" sentinel.
    bool valid = false;  // +0x04: whether this register slot is active
    // +0x05: 3 bytes padding to 8-byte alignment

    //! \brief Default-construct an invalid (unset) register slot.
    AsmRegister() = default;
    //! \brief Construct from explicit `value` / `valid` components.
    //! \param v      Register number (caller-supplied; not validated).
    //! \param valid  Whether the slot should be marked active.
    AsmRegister(int v, bool valid) : value(v), valid(valid) {}
    //! \brief Construct from a single signed index: negative values
    //!        yield an invalid slot with `value == 0`; non-negative
    //!        values yield a valid slot carrying `max(n, 0)`.
    //!
    //! Reproduces the binary's `cmovg`/`setns` idiom verbatim.
    //!
    //! \param n  Signed register index; negative means "invalid".
    AsmRegister(int n) : value(n > 0 ? n : 0), valid(n >= 0) {}  // @0x28eb40: cmovg + setns

    // Named constants for convenience
    //! \brief Factory for the invalid sentinel (`{-1, false}`).
    //! \return  An `AsmRegister` with `valid == false` and `value == -1`.
    static AsmRegister Invalid() { return {-1, false}; }
    //! \brief Factory for a valid slot carrying register number `n`.
    //! \param n  Register number to store.
    //! \return  An `AsmRegister` with `valid == true` and `value == n`.
    static AsmRegister Reg(int n) { return {n, true}; }

    //! \brief Factory for the "unset second-register slot" used at
    //!        `asmPlay` and similar emit sites.
    //!
    //! Equivalent to the binary's `AsmRegister(-1)` single-arg
    //! construction, which the `cmovg`/`setns` idiom collapses to
    //! `{value=0, valid=false}` — distinct in stored bytes from
    //! `Invalid()` (`{-1, false}`) but indistinguishable under the
    //! equality rule that treats all invalid slots as equal.
    //!
    //! \return  An `AsmRegister` with `valid == false` and `value == 0`.
    static AsmRegister UnsetSlot() { return {0, false}; }

    //! \brief Sentinel register used internally by `splitConstRegisters`
    //!        to mark barrier entries that should be stripped.
    //!
    //! Carries `value == INT_MAX` and `valid == true`; not a real
    //! hardware register.
    //!
    //! \return  The barrier sentinel `AsmRegister`.
    // 0x28ebb0 — returns AsmRegister(INT_MAX, true): sentinel used by
    // splitConstRegisters to mark barrier entries that should be stripped.
    static AsmRegister magicSkipRegister() { return {0x7fffffff, true}; }

    //! \brief Returns whether the slot refers to a real register
    //!        (i.e. the `valid` flag is set).
    //! \return  `true` if `valid`, `false` otherwise.
    bool isValid() const { return valid; }
    //! \brief Explicit conversion to the underlying register number.
    //!        No validity check is performed — callers must consult
    //!        `isValid()` first when the slot may be unset.
    explicit operator int() const { return value; }

    //! \brief Equality comparison with the invalid-slot collapse rule.
    //!
    //! Two invalid slots compare equal regardless of `value`;
    //! otherwise both `valid` and `value` must match.
    //!
    //! \param o  Right-hand operand.
    //! \return  `true` when both slots are equivalent under the rule above.
    bool operator==(const AsmRegister& o) const {
        // Binary semantics (0x28eb80): if both invalid, always equal
        // regardless of value. If both valid, compare values.
        // If one valid and one invalid, not equal.
        if (valid != o.valid) return false;
        if (!valid) return true;  // both invalid → equal
        return value == o.value;  // both valid → compare values
    }
    //! \brief Negation of `operator==`.
    //! \param o  Right-hand operand.
    //! \return  `!(*this == o)`.
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
//! \brief Free-function form of `AsmRegister::isValid()`; returns
//!        `true` iff `r` was constructed with a real register index
//!        (i.e. the internal valid flag is set), `false` for the
//!        default-constructed sentinel that signals "no register".
//! \param r Register value to test.
//! \return Forwarded result of `r.isValid()`.
inline bool isValid(AsmRegister r) { return r.isValid(); }

} // namespace zhinst
