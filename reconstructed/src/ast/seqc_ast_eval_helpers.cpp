// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// seqc_ast_eval_helpers.cpp — split from seqc_ast_nodes_evaluate.cpp
// Helper functions: combine(), waveform arithmetic, operator helpers, control-flow helpers
// ============================================================================

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <variant>
#include <vector>
#include <cmath>

#include "zhinst/ast/seqc_ast_node.hpp"
#include "zhinst/asm/asm_commands.hpp"
#include "zhinst/ast/eval_results.hpp"
#include "zhinst/runtime/resources.hpp"
#include "zhinst/ast/frontend_lowering.hpp"
#include "zhinst/core/compiler_message.hpp"
#include "zhinst/waveform/wavetable_front.hpp"
#include "zhinst/core/error_messages.hpp"
#include "zhinst/waveform/waveform_generator.hpp"
#include "zhinst/runtime/custom_functions.hpp"
#include "zhinst/device/device_constants.hpp"
#include "zhinst/core/types.hpp"

// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// seqc_ast_nodes_evaluate.cpp
//
// Binary TU: SeqCAstNodesEvaluate.cpp (from _GLOBAL__sub_I_ symbol).
//
// Contains all evaluate() virtual method implementations for the SeqCAstNode
// hierarchy, plus the combine(VarType, VarType) lookup-table function and
// VarTypeException.
//
// Key addresses:
//   SeqCAstNode::evaluate(3-arg) base         @0x209dc0 (returns nullptr)
//   SeqCOperator::evaluate(5-arg) base        @0x210a90 (returns nullptr)
//   SeqCOperator::evaluate(3-arg) template    @0x210aa0 (dispatches to 5-arg)
//   combine(VarType, VarType)                 @0x247f50
//   VarTypeException ctor                     @0x2480e0
//   VarTypeException dtor                     @0x248140
// ============================================================================

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "zhinst/ast/seqc_ast_node.hpp"
#include "zhinst/asm/asm_commands.hpp"
#include <cmath>

#include "zhinst/ast/eval_results.hpp"
#include "zhinst/runtime/resources.hpp"
#include "zhinst/ast/frontend_lowering.hpp"
#include "zhinst/core/compiler_message.hpp"
#include "zhinst/waveform/wavetable_front.hpp"
#include "zhinst/core/error_messages.hpp"
#include "zhinst/waveform/waveform_generator.hpp"
#include "zhinst/runtime/custom_functions.hpp"
#include "zhinst/device/device_constants.hpp"
#include "zhinst/core/types.hpp"

namespace zhinst {

// floatEqual @0x2ec050 — exact IEEE-754 bitwise double equality.
// Defined in waveform_generator.cpp; forward-declared here for SeqCDiv.
bool floatEqual(double a, double b);

// ============================================================================
// VarTypeException — @0x2480e0 (ctor), @0x248140 (dtor)
// ============================================================================

VarTypeException::VarTypeException(std::string const& msg)  // @0x2480e0
    : msg_(msg) {}

VarTypeException::~VarTypeException() = default;  // @0x248140

const char* VarTypeException::what() const noexcept {
    return msg_.c_str();
}

// ============================================================================
// combine(VarType, VarType) — @0x247f50
//
// 7x7 lookup matrix decoded from binary .rodata tables.
// Row = lhs VarType (0-6), column = rhs VarType (0-6).
// Throws VarTypeException(error 0x91) if either arg > 6.
//
// Matrix (verified from rodata at 0x95c2b8..0x95c37c):
//
//         Unset Void  Var   Str   Const Wave  Cvar
// Unset   [passthrough rhs]
// Void    1     1     2     3     2     5     2
// Var     2     1     2     3     2     5     2
// String  3     1     2     3     3     5     3
// Const   4     1     2     5     4     5     6
// Wave    5     1     2     5     5     5     5
// Cvar    6     1     2     5     6     5     6
// ============================================================================

VarType combine(VarType lhs, VarType rhs) {  // @0x247f50
    // Row 0 (Unset): passthrough — return rhs unchanged.
    if (lhs == VarType_Unset) {
        if (static_cast<unsigned>(rhs) > 6)
            throw VarTypeException(
                ErrorMessages::format(CantCombineTypes, str(lhs), str(rhs)));
        return rhs;
    }

    if (static_cast<unsigned>(lhs) > 6 || static_cast<unsigned>(rhs) > 6)
        throw VarTypeException(
            ErrorMessages::format(CantCombineTypes, str(lhs), str(rhs)));

    // Rows 1-6: static lookup tables from binary .rodata.
    // combineTable[lhs-1][rhs] — indexed by (lhs - 1) since row 0 is handled above.
    static constexpr int32_t combineTable[6][7] = {
        // Void(1):    {Unset, Void, Var, Str, Const, Wave, Cvar}
        { 1, 1, 2, 3, 2, 5, 2 },   // @0x95c2d4
        // Var(2):
        { 2, 1, 2, 3, 2, 5, 2 },   // @0x95c2f0
        // String(3):
        { 3, 1, 2, 3, 3, 5, 3 },   // @0x95c30c
        // Const(4):
        { 4, 1, 2, 5, 4, 5, 6 },   // @0x95c328
        // Wave(5):
        { 5, 1, 2, 5, 5, 5, 5 },   // @0x95c360
        // Cvar(6):
        { 6, 1, 2, 5, 6, 5, 6 },   // @0x95c344
    };

    return static_cast<VarType>(combineTable[lhs - 1][rhs]);
}

// ============================================================================
// combine(VarSubType, VarSubType) — @0x247ea0
//
// Lookup-table function analogous to combine(VarType, VarType) but for
// VarSubType combinations.  Called by SeqCPlus::evaluate (and likely
// other arithmetic operators) for the Const+Const and String+String rows.
//
// Fully reconstructed in NOT a .rodata lookup table — pure
// conditional logic with priority-based semantics (60 bytes).
// ============================================================================

VarSubType combine(VarSubType lhs, VarSubType rhs) {  // @0x247ea0
    if (lhs == VarSubType_Default) return rhs;                       // @0x247ea4
    if (rhs == VarSubType_Default) return lhs;                       // @0x247ea8
    // Priority-based combine (not a lookup table):
    // FunctionArg(2) dominates everything; Stub(1) dominates Numeric/String;
    // all other combinations (Numeric+Numeric, String+String, etc.) → Default.
    if (lhs == VarSubType_FunctionArg || rhs == VarSubType_FunctionArg)
        return VarSubType_FunctionArg;                               // @0x247eac-0x247ebf
    if (lhs == VarSubType_Stub || rhs == VarSubType_Stub)
        return VarSubType_Stub;                                      // @0x247ec1-0x247ecf
    return VarSubType_Default;                                       // @0x247ecf (cl=0)
}

// ============================================================================
// Anonymous-namespace helpers — file-local utilities shared by the
// arithmetic operator evaluate() overrides (SeqCPlus, SeqCMinus, etc.).
// ============================================================================

} // namespace zhinst
