// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// EvalResultValue — implementation extracted from custom_functions.cpp
// file-organization split (audit Section C1).
//
// See notes/audit_phase16a.md for context.
// ============================================================================

#include "zhinst/ast/eval_result_value.hpp"

namespace zhinst {

EvalResultValue::~EvalResultValue() {  // @0x15c820
    // The binary (libc++) explicitly destroys the embedded Value here.
    // Under libstdc++, the compiler-generated member destructor handles
    // Value's destruction, so we must NOT call ~Value() manually to
    // avoid a double-free.  The custom dtor is kept only for ABI parity
    // with the binary symbol at 0x15c820.
}

} // namespace zhinst
