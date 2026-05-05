// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// EvalResultValue — 0x38 (56) bytes
//
// Typed value passed to all SeqC built-in functions and through the
// frontend lowering pipeline (vector<EvalResultValue> args). Embedded
// in EvalResults::values_.
//
// Layout:
//   +0x00  4  VarType       varType_    — outer type tag (Int=2, Double=3,
//                                          String=4, ...)
//   +0x04  4  VarSubType    varSubType_ — sub-type qualifier (usually 0)
//   +0x08  40 Value         value_      — embedded variant (0x28 bytes)
//   +0x30  8  AsmRegister   reg_        — register binding
//                                          (default: value=-1, valid=false)
//
// CORRECTION 2026-04-23 (Phase 15a-i): Fields renamed from opaque
// field_00/field_08/which_/data_/field_30 to meaningful names.
// Evidence from:
//   - EvalResults::setValue(VarType, string) @0x20af20 stores VarType in
//     [erv+0x00] and VarSubType=0 in [erv+0x04]
//   - Dtor @0x15c820 reads Value.which_ at [this+0x10] = ERV+0x08+0x08
//   - setWaitCyclesReg @0x15ca90 reads AsmRegister at [base+0x68] =
//     ERV[1]+0x30
//   - AsmRegister::AsmRegister(int) called with esi=-1 at @0x15caea
//
// Extracted from custom_functions.{hpp,cpp} during Phase 16b
// file-organization split (audit Section C1). See notes/audit_phase16a.md.
// ============================================================================
#pragma once

#include <cstdint>
#include "zhinst/asm/asm_register.hpp"
#include "zhinst/ast/value.hpp"

namespace zhinst {

// Forward declarations of the enums (defined in zhinst/types.hpp)
enum VarType : int32_t;
enum VarSubType : int32_t;

struct EvalResultValue {
    VarType     varType_;      // +0x00 — outer type tag
    VarSubType  varSubType_;   // +0x04 — sub-type qualifier
    Value       value_;        // +0x08 — embedded Value (0x28 bytes)
    AsmRegister reg_;          // +0x30 — register binding

    ~EvalResultValue();  // @0x15c820
};
// sizeof(EvalResultValue): 0x38 on libc++ (binary), 0x40 on libstdc++.
// Difference is solely from embedded Value's std::string union member.
static_assert(sizeof(EvalResultValue) >= 0x38,
              "EvalResultValue must be at least 0x38 bytes");

} // namespace zhinst
