// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// EvalResultValue — implementation extracted from custom_functions.cpp
// during Phase 16b file-organization split (audit Section C1).
//
// See notes/audit_phase16a.md for context.
// ============================================================================

#include "zhinst/eval_result_value.hpp"

namespace zhinst {

EvalResultValue::~EvalResultValue() {  // @0x15c820
    // Delegates to Value::~Value() logic — if the embedded value's variant
    // holds a string (abs(which) >= 3), free the heap string if long.
    value_.~Value();
}

} // namespace zhinst
