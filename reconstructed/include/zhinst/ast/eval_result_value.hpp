// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// EvalResultValue — 0x38 (56) bytes
//
// Typed value passed to all SeqC built-in functions and through the
// frontend lowering pipeline (vector<EvalResultValue> args). Embedded
// in EvalResults::values_.
//
// Offset  Size  Type        Name         Notes
// +0x00   4     VarType     varType_     outer type tag (Int=2, Double=3, String=4, ...)
// +0x04   4     VarSubType  varSubType_  sub-type qualifier (usually 0)
// +0x08   40    Value       value_       embedded variant (0x28 bytes)
// +0x30   8     AsmRegister reg_         register binding (default: value=-1, valid=false)
// sizeof(EvalResultValue) = 0x38 (libc++ / binary); 0x40 (libstdc++)
// ============================================================================
#pragma once

#include <cstdint>
#include "zhinst/asm/asm_register.hpp"
#include "zhinst/ast/value.hpp"

namespace zhinst {

// Forward declarations of the enums (defined in zhinst/types.hpp)
enum VarType : int32_t;
enum VarSubType : int32_t;

//! \brief Single typed value entry inside an `EvalResults`.
//!
//! `EvalResultValue` is the per-element payload of `EvalResults::values_`
//! and the element type of the argument vector passed to every SeqC
//! built-in function.  It pairs a `Value` with the outer-tag pair
//! `(varType_, varSubType_)` and an `AsmRegister` binding indicating the
//! assembler register currently holding the value (or an invalid register
//! when the value has not been materialised in hardware).
struct EvalResultValue {
    VarType     varType_;      // +0x00 — outer type tag
    VarSubType  varSubType_;   // +0x04 — sub-type qualifier
    Value       value_;        // +0x08 — embedded Value (0x28 bytes)
    AsmRegister reg_;          // +0x30 — register binding

    ~EvalResultValue();  // @0x15c820
};
// sizeof(EvalResultValue): 0x38 on libc++ (binary), 0x40 on libstdc++.
// The difference is from the embedded Value's std::string storage.
static_assert(sizeof(EvalResultValue) >= 0x38,
              "EvalResultValue must be at least 0x38 bytes");

} // namespace zhinst
