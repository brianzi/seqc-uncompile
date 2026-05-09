// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// seqc_ast_eval_simple.cpp — split from seqc_ast_nodes_evaluate.cpp
// evaluate() implementations: simple/unary/binary operators, statements, loops helpers
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

namespace zhinst {

#include "seqc_ast_eval_impl.inc"


std::shared_ptr<EvalResults> SeqCCommand::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& /*ctx*/,
    FrontendLoweringState& /*state*/) const
{                                                           // @0x209db0
    return nullptr;                                         // xorps+movups→sret
}

// SeqCOperation::evaluate(3) — @0x209e50
// Returns null shared_ptr. Same as SeqCCommand above.
std::shared_ptr<EvalResults> SeqCOperation::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& /*ctx*/,
    FrontendLoweringState& /*state*/) const
{                                                           // @0x209e50
    return nullptr;
}

// --- Returns empty EvalResults (112B bodies) ---

// SeqCLabel::evaluate(3) — @0x2130d0
// Allocates a default-constructed EvalResults, returns it.
std::shared_ptr<EvalResults> SeqCLabel::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& /*ctx*/,
    FrontendLoweringState& /*state*/) const
{                                                           // @0x2130d0
    return std::make_shared<EvalResults>();                  // @0x2130de
}

// SeqCXorExpr::evaluate(5) — @0x2427b0
// Returns empty EvalResults. XOR expression not implemented in lowering.
std::shared_ptr<EvalResults> SeqCXorExpr::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& /*ctx*/,
    FrontendLoweringState& /*state*/,
    EvalResults const& /*lhsResult*/,
    EvalResults const& /*rhsResult*/) const
{                                                           // @0x2427b0
    return std::make_shared<EvalResults>();                  // @0x2427be
}

// SeqCNoOp::evaluate(5) — @0x246270
// Returns empty EvalResults. No-op operator placeholder.
std::shared_ptr<EvalResults> SeqCNoOp::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& /*ctx*/,
    FrontendLoweringState& /*state*/,
    EvalResults const& /*lhsResult*/,
    EvalResults const& /*rhsResult*/) const
{                                                           // @0x246270
    return std::make_shared<EvalResults>();                  // @0x24627e
}

// --- setLineNr preamble + return empty EvalResults (160B bodies) ---

// SeqCVariableType::evaluate(3) — @0x209d10
// Sets line number on messages, asmCommands, and wavetable, then returns
// an empty EvalResults.
std::shared_ptr<EvalResults> SeqCVariableType::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/) const
{                                                           // @0x209d10
    int lineNr = lineNr_;                                     // [rsi+0xc]
    ctx.messages->setLineNr(lineNr);                        // @0x209d2a
    ctx.asmCommands->setWavetableFrontIndex(lineNr);        // @0x209d33
    ctx.wavetable->setLineNr(lineNr);                       // @0x209d3e
    return std::make_shared<EvalResults>();                  // @0x209d48
}

// SeqCNoCmd::evaluate(3) — @0x22a560
// Sets line number on subsystems, returns empty EvalResults.
std::shared_ptr<EvalResults> SeqCNoCmd::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/) const
{                                                           // @0x22a560
    int lineNr = lineNr_;                                     // [rsi+0xc]
    ctx.messages->setLineNr(lineNr);                        // @0x22a57a
    ctx.asmCommands->setWavetableFrontIndex(lineNr);        // @0x22a583
    ctx.wavetable->setLineNr(lineNr);                       // @0x22a58e
    return std::make_shared<EvalResults>();                  // @0x22a598
}

// --- setLineNr preamble + delegate to child (208B body) ---

// SeqCPos::evaluate(3) — @0x228e80
// Sets line number, then delegates to child_->evaluate() via virtual dispatch.
// Binary uses local accessor SeqCPos::expr() @0x2048a0 (= child_.get()).
std::shared_ptr<EvalResults> SeqCPos::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{                                                           // @0x228e80
    int lineNr = lineNr_;                                     // [rsi+0xc]
    ctx.messages->setLineNr(lineNr);                        // @0x228eab
    ctx.asmCommands->setWavetableFrontIndex(lineNr);        // @0x228eb4
    ctx.wavetable->setLineNr(lineNr);                       // @0x228ebf

    // Binary: call SeqCPos::expr() → child_.get(), then virtual dispatch
    // through child's vptr[0] (evaluate 3-arg).             @0x228ec7-228ef8
    return expr()->evaluate(std::move(res), ctx, state);
}

// --- Error emitters + return empty (224B bodies) ---

// SeqCContinueStatement::evaluate(3) — @0x226890
// Emits error 0xd5 (StatementNotSupported) with "continue", returns empty.
std::shared_ptr<EvalResults> SeqCContinueStatement::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/) const
{                                                           // @0x226890
    ctx.messages->errorMessage(                             // @0x2268ca
        ErrorMessages::format(ErrorMessageT(0xd5),          // @0x2268bb
                              "continue"),                   // rodata @0x905b6a
        lineNr_);                                             // lineNr = this->lineNr_
    return std::make_shared<EvalResults>();                  // @0x2268eb
}

// SeqCBreakStatement::evaluate(3) — @0x226970
// Emits error 0xd5 (StatementNotSupported) with "break", returns empty.
// BUT: if state.inSwitch_ is true, break is allowed in switch statements.
std::shared_ptr<EvalResults> SeqCBreakStatement::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // @0x226970: binary emits error 0xd5 unconditionally.
    // There is no inSwitch_ guard: the first instructions are format(0xd5, "break")
    // followed immediately by errorMessage(...), with no branch beforehand.
    ctx.messages->errorMessage(                               // @0x2269aa
        ErrorMessages::format(ErrorMessageT(0xd5),            // @0x22699b
                              "break"),                        // rodata @0x905b73
        lineNr_);                                              // lineNr = this->lineNr_
    return std::make_shared<EvalResults>();
}

// --- Throws CompilerException (240B body) ---

// SeqCParamList::evaluate(3) — @0x211050
// Unconditionally throws CompilerException — a param list should never
// be directly evaluated.
std::shared_ptr<EvalResults> SeqCParamList::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& /*ctx*/,
    FrontendLoweringState& /*state*/) const
{                                                           // @0x211050
    throw CompilerException(                                // @0x2110dd
        "Internal compiler error: type param list should "
        "never need to be evaluated.");                     // 0x4b bytes @0x905a43
}

// ============================================================================
//  — Batch 2: Operator wrappers that delegate to helpers
//
// These 5-arg evaluate overrides delegate the actual computation to
// anonymous-namespace helper functions, then build the name_ string
// from lhsResult.name_ + " OP " + rhsResult.name_.
// ============================================================================

// SeqCGreater::evaluate(5) — @0x2358b0
// Calls evalGreater, builds name with " > ".
std::shared_ptr<EvalResults> SeqCGreater::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x2358b0
    auto result = evalGreater(lhsResult, rhsResult,
                              std::move(res), ctx);         // @0x2358ea
    result->name_ = lhsResult.name_ + " > " + rhsResult.name_;  // @0x235907-235a52
    return result;
}

// SeqCEqual::evaluate(5) — @0x2399c0
// Calls evalEqual, builds name with " == ".
std::shared_ptr<EvalResults> SeqCEqual::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x2399c0
    auto result = evalEqual(std::move(res), ctx,
                            lhsResult, rhsResult);          // @0x2399fd
    result->name_ = lhsResult.name_ + " == " + rhsResult.name_; // @0x239a1a-239b6b
    return result;
}

// SeqCShiftL::evaluate(5) — @0x235690
// Calls evalShift with isRight=false, builds name with " << ".
std::shared_ptr<EvalResults> SeqCShiftL::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x235690
    auto result = evalShift(lhsResult, rhsResult,
                            std::move(res), false, ctx);    // @0x2356cf (ecx=0)
    result->name_ = lhsResult.name_ + " << " + rhsResult.name_; // @0x2356ec-23583d
    return result;
}

// SeqCShiftR::evaluate(5) — @0x232630
// Calls evalShift with isRight=true, builds name with " >> ".
std::shared_ptr<EvalResults> SeqCShiftR::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x232630
    auto result = evalShift(lhsResult, rhsResult,
                            std::move(res), true, ctx);     // @0x232672 (ecx=1)
    result->name_ = lhsResult.name_ + " >> " + rhsResult.name_; // @0x23268f-2327e0
    return result;
}

// SeqCAndExpr::evaluate(5) — @0x23e810
// Calls evalAnd, builds name with " & ".
std::shared_ptr<EvalResults> SeqCAndExpr::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x23e810
    auto result = evalAnd(std::move(res), ctx,
                          lhsResult, rhsResult);            // @0x23e84e
    result->name_ = lhsResult.name_ + " & " + rhsResult.name_;  // @0x23e86b-23e9b6
    return result;
}

// SeqCOrExpr::evaluate(5) — @0x240820
// Calls evalOr, builds name with " & ".
// Binary: uses the same " & " separator as SeqCAndExpr —
// likely a copy-paste bug in the original source. Faithfully reproduced.
std::shared_ptr<EvalResults> SeqCOrExpr::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x240820
    auto result = evalOr(std::move(res), ctx,
                         lhsResult, rhsResult);             // @0x24085e
    result->name_ = lhsResult.name_ + " & " + rhsResult.name_;  // @0x24087b-2409c6
    return result;                                          // NOTE: " & " confirmed from DWORD 0x202620
}

// SeqCLEqual::evaluate(5) — @0x238c60
// Calls evalGreater then invertBool (a <= b ≡ !(a > b)).
// Builds name with " <= ".
std::shared_ptr<EvalResults> SeqCLEqual::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x238c60
    auto greaterResult = evalGreater(lhsResult, rhsResult,
                                     res, ctx);             // @0x238ca4
    auto result = invertBool(std::move(greaterResult),
                             std::move(res), ctx);          // @0x238d11
    result->name_ = lhsResult.name_ + " <= " + rhsResult.name_; // @0x238dd5-238f0e
    return result;
}

// SeqCNEqual::evaluate(5) — @0x23ba00
// Calls evalEqual then invertBool (a != b ≡ !(a == b)).
// Builds name with " != ".
std::shared_ptr<EvalResults> SeqCNEqual::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x23ba00
    auto equalResult = evalEqual(res, ctx,
                                 lhsResult, rhsResult);     // @0x23ba42
    auto result = invertBool(std::move(equalResult),
                             std::move(res), ctx);          // @0x23baaf
    result->name_ = lhsResult.name_ + " != " + rhsResult.name_; // @0x23bb73-23bd3d
    return result;
}

// ============================================================================
// Remaining evaluate() overrides (originally  stubs, now implemented)
// ============================================================================

// --- Operator 5-arg (inline logic, not delegated to helpers) ---

// SeqCLower::evaluate(5) — @0x2371b0 (649B)
// Calls evalLower, builds name with " < ".
std::shared_ptr<EvalResults> SeqCLower::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x2371b0
    auto result = evalLower(lhsResult, rhsResult,
                            std::move(res), ctx);            // @0x2371ea
    result->name_ = lhsResult.name_ + " < " + rhsResult.name_;  // @0x237207-237430
    return result;
}

// SeqCGEqual::evaluate(5) — @0x2395f0 (968B)
// Calls evalLower then invertBool (a >= b ≡ !(a < b)).
// Builds name with " >= ".
std::shared_ptr<EvalResults> SeqCGEqual::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x2395f0
    auto lowerResult = evalLower(lhsResult, rhsResult,
                                 res, ctx);                  // @0x239634
    auto result = invertBool(std::move(lowerResult),
                             std::move(res), ctx);           // @0x2396a1
    result->name_ = lhsResult.name_ + " >= " + rhsResult.name_; // @0x239765-2395e0
    return result;
}

// SeqCLogAnd::evaluate(5) — @0x242820, 1562B
// Logical AND (&&).
// 1. Copy-constructs both operands.
// 2. Converts each to boolean via valueToBool (skipping if already Stub-subtype).
// 3. Delegates to evalAnd on the boolean results.
// 4. Builds name: lhsResult.name_ + " && " + rhsResult.name_.
// Binary: uses " && " — confirmed from DWORD 0x20262620.
std::shared_ptr<EvalResults> SeqCLogAnd::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x242820
    // 1. Copy both operands.                               // @0x242847-2428a0
    auto lhsCopy = std::make_shared<EvalResults>(lhsResult);
    auto rhsCopy = std::make_shared<EvalResults>(rhsResult);

    // 2. Convert LHS to boolean (skip if already Stub).    // @0x2428ad-2429ec
    auto const& lhsVals = lhsResult.values_;
    if (!(lhsVals.size() == 1 &&
          lhsVals[0].varSubType_ == VarSubType_Stub)) {
        lhsCopy = valueToBool(std::move(lhsCopy),
                              res, ctx);                    // @0x242925
    }

    // 3. Convert RHS to boolean (skip if already Stub).    // @0x2429ec-242b22
    auto const& rhsVals = rhsResult.values_;
    if (!(rhsVals.size() == 1 &&
          rhsVals[0].varSubType_ == VarSubType_Stub)) {
        rhsCopy = valueToBool(std::move(rhsCopy),
                              res, ctx);                    // @0x242a5c
    }

    // 4. Evaluate AND on the boolean results.              // @0x242b22-242b52
    auto result = evalAnd(std::move(res), ctx,
                          *lhsCopy, *rhsCopy);              // @0x242b52

    // 5. Build name: lhs " && " rhs.                       // @0x242b6e-242c6b
    result->name_ = lhsResult.name_ + " && " + rhsResult.name_;

    return result;                                          // @0x242d36
}

// SeqCLogOr::evaluate(5) — @0x243840, 1562B
// Logical OR (||).
// Mirror of SeqCLogAnd — identical structure, calls evalOr instead of evalAnd.
// Binary: uses " && " in the name string (DWORD 0x20262620) —
// this is a copy-paste bug in the original source. Faithfully reproduced.
std::shared_ptr<EvalResults> SeqCLogOr::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{                                                           // @0x243840
    // 1. Copy both operands.                               // @0x243867-2438c0
    auto lhsCopy = std::make_shared<EvalResults>(lhsResult);
    auto rhsCopy = std::make_shared<EvalResults>(rhsResult);

    // 2. Convert LHS to boolean (skip if already Stub).    // @0x2438cd-243a0c
    auto const& lhsVals = lhsResult.values_;
    if (!(lhsVals.size() == 1 &&
          lhsVals[0].varSubType_ == VarSubType_Stub)) {
        lhsCopy = valueToBool(std::move(lhsCopy),
                              res, ctx);                    // @0x243945
    }

    // 3. Convert RHS to boolean (skip if already Stub).    // @0x243a0c-243b42
    auto const& rhsVals = rhsResult.values_;
    if (!(rhsVals.size() == 1 &&
          rhsVals[0].varSubType_ == VarSubType_Stub)) {
        rhsCopy = valueToBool(std::move(rhsCopy),
                              res, ctx);                    // @0x243a7c
    }

    // 4. Evaluate OR on the boolean results.               // @0x243b42-243b72
    auto result = evalOr(std::move(res), ctx,
                         *lhsCopy, *rhsCopy);               // @0x243b72

    // 5. Build name: lhs " && " rhs.                       // @0x243b8e-243c8b
    // Binary: " && " confirmed — not " || ". Copy-paste bug in original.
    result->name_ = lhsResult.name_ + " && " + rhsResult.name_;

    return result;                                          // @0x243d56
}

// SeqCMod::evaluate (5-arg) — @0x231ec0 (1892 bytes)
//
// Implements `lhs % rhs` lowering.  Much simpler than SeqCDiv:
//   - Only Const/Cvar % Const/Cvar is supported.
//   - Uses fmod() (floating-point modulo), NOT integer %.
//   - No divide-by-zero check (fmod handles ±inf/NaN per IEEE 754).
//   - No Var paths, no Wave paths — all non-Const/Cvar → error 0x8e.
//   - Single error path via ErrorMessages::format(0x8e, ...).
//   - Name separator: " % " (DWORD 0x202520).
//
// Binary address annotations:
//   0x231ec0–0x231f38  Prologue, allocate result EvalResults
//   0x231f3c–0x231f88  Validate lhsCount==1, lhsType ∈ {Const,Cvar}
//   0x231f8c–0x231fc1  Validate rhsCount==1, rhsType ∈ {Const,Cvar}
//   0x231fc7–0x231fe5  combine(VarType, VarType)          @0x247f50
//   0x231fef–0x232042  combine(VarSubType, VarSubType)    @0x247ea0
//   0x232049–0x23235d  Extract lhsValue, call toDouble()  @0x15a560
//   0x23236a–0x232436  Extract rhsValue, call toDouble()  @0x15a560
//   0x232446           call fmod()                        @0xcb0c0
//   0x23244b–0x23246b  Build Value(fmod_result), call setValue @0x16bfb0
//   0x23208c–0x232186  Error path: format(0x8e) + errorMessage
//   0x232186–0x2322c0  Name tail: lhsName + " % " + rhsName
//   0x2322c0–0x2322d4  Epilogue, return
// ============================================================================

std::shared_ptr<EvalResults> SeqCMod::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{
    // 1. Allocate empty result.                              // @0x231ee2–0x231f38
    auto result = std::make_shared<EvalResults>();

    // 2. Validate both operands: count==1 and type ∈ {Const(4), Cvar(6)}.
    //    (type | 0x2) == 6 is the test.                      // @0x231f3c–0x231fc1
    // isConstOrCvar — promoted to file-scope

    const size_t lhsCount = lhsResult.values_.size();
    const size_t rhsCount = rhsResult.values_.size();

    VarType lhsType = VarType_Unset;
    if (lhsCount == 1)
        lhsType = lhsResult.values_.back().varType_;
    VarType rhsType = VarType_Unset;
    if (rhsCount == 1)
        rhsType = rhsResult.values_.back().varType_;

    if (lhsCount != 1 || !isConstOrCvar(lhsType) ||
        rhsCount == 0 || rhsCount > 1 || !isConstOrCvar(rhsType))
    {
        // Error 0x8e: unsupported type combination.          // @0x23208c–0x232186
        //   lhsType/rhsType default to Unset(0) when count != 1.
        VarType errLhs = VarType_Unset;                       // @0x2320a1 (xor esi,esi)
        if (lhsCount == 1)
            errLhs = lhsResult.values_.back().varType_;       // @0x232098
        VarType errRhs = VarType_Unset;
        if (rhsCount == 1)
            errRhs = rhsResult.values_.back().varType_;       // @0x2320d9

        ctx.messages->errorMessage(                           // @0x232116
            ErrorMessages::format(ErrorMessageT(0x8e),        // @0x232105
                                  str(errLhs), str(errRhs)),
            -1);
        goto name_tail;
    }

    {
        // 3. Both Const/Cvar: combine types, fmod, setValue. // @0x231fc7–0x23246b
        VarType combinedType = combine(lhsType, rhsType);     // @0x231fe0
        VarSubType combinedSub = combine(                      // @0x23203d
            lhsResult.values_.back().varSubType_,
            rhsResult.values_.back().varSubType_);

        double lhsVal =                                        // @0x23235d
            lhsResult.getValue().toDouble();
        double rhsVal =                                        // @0x232436
            rhsResult.getValue().toDouble();
        double modResult = std::fmod(lhsVal, rhsVal);          // @0x232446

        result->setValue(combinedType, combinedSub,            // @0x23246b
                         Value(modResult));
    }

name_tail:
    // 4. Build name: lhsName + " % " + rhsName.             // @0x232186–0x2322a2
    //    DWORD literal 0x202520 = " % \0".                   // @0x23222c
    result->name_ = lhsResult.name_ + " % " + rhsResult.name_;

    return result;                                             // @0x2322c0
}

// SeqCInc::evaluate(5) — @0x23bd50, 5464B
//   Increment operator (++ prefix and postfix).
//
//   lhsResult = left operand (populated for post-increment: x++)
//   rhsResult = right operand (populated for pre-increment: ++x)
//
//   6-path dispatch + shared name tail:
//     1. lhsResult Var  (post-increment): copy old value to new reg, increment original
//     2. rhsResult Var  (pre-increment):  increment register in-place, result = same reg
//     3. lhsResult Cvar (post-increment): result = old value, updateCvar(+1)
//     4. rhsResult Cvar (pre-increment):  updateCvar(+1), result = new value
//     5. lhsResult other non-zero type:   error 0x6f
//     6. rhsResult other non-zero type:   error 0x6f
//   Name tail: result->name_ = lhsResult.name_ + rhsResult.name_ + "++"
//
std::shared_ptr<EvalResults> SeqCInc::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{
    auto result = std::make_shared<EvalResults>();             // @0x23bd75

    auto const& lhsVals = lhsResult.values_;
    auto const& rhsVals = rhsResult.values_;
    bool lhsHas1 = !lhsVals.empty() && lhsVals.size() <= 1;
    bool rhsHas1 = !rhsVals.empty() && rhsVals.size() <= 1;

    // --- Path 1: lhsResult Var (post-increment) --- @0x23be27
    if (lhsHas1 && lhsVals.back().varType_ == VarType_Var) {

        // Copy lhsResult assemblers into result.              @0x23be32
        result->assemblers_.insert(
            result->assemblers_.end(),
            lhsResult.assemblers_.begin(), lhsResult.assemblers_.end());

        // Allocate new register; result points to new reg.    @0x23be5c
        int newRegNum = Resources::getRegisterNumber();
        result->setValue(VarType_Var, newRegNum);               // @0x23be6b

        AsmCommands* asmCmd = ctx.asmCommands.get();            // @0x23be70

        // Get newReg from result, srcReg from lhsResult.      @0x23be7f
        AsmRegister newReg = result->values_.empty()
            ? AsmRegister(0) : result->values_.back().reg_;
        AsmRegister srcReg = lhsVals.empty()
            ? AsmRegister(0) : lhsVals.back().reg_;

        // addi(newReg, srcReg, Immediate(0)): copy old value. @0x23c0e9-23c102
        {
            auto addiAsms = asmCmd->addi(newReg, srcReg, Immediate(0));
            result->assemblers_.insert(
                result->assemblers_.end(),
                addiAsms.begin(), addiAsms.end());
            // Binary: Immediate dtor writes back to vtable at b06660+index*8 (no-op in reconstruction) @0x23c1c6
        }

        // Re-read srcReg from lhsResult (binary re-reads).   @0x23c1fa
        AsmRegister srcReg2 = lhsVals.empty()
            ? AsmRegister(0) : lhsVals.back().reg_;

        // addi(srcReg, srcReg, Immediate(1)): increment.     @0x23c261-23c27b
        {
            auto addiAsms = asmCmd->addi(srcReg2, srcReg2, Immediate(1));
            result->assemblers_.insert(
                result->assemblers_.end(),
                addiAsms.begin(), addiAsms.end());
            // Binary: Immediate dtor vtable store-back (no-op in reconstruction) @0x23c346
        }
    }
    // --- Path 2: rhsResult Var (pre-increment) --- @0x23be9a
    else if (rhsHas1 && rhsVals.back().varType_ == VarType_Var) {

        // Copy rhsResult assemblers into result.              @0x23bec8
        result->assemblers_.insert(
            result->assemblers_.end(),
            rhsResult.assemblers_.begin(), rhsResult.assemblers_.end());

        // srcReg = rhsResult register.  Result uses SAME register.
        AsmRegister srcReg = rhsVals.empty()
            ? AsmRegister(0) : rhsVals.back().reg_;             // @0x23bf04
        result->setValue(VarType_Var,
            static_cast<int>(srcReg));                          // @0x23c443

        AsmCommands* asmCmd = ctx.asmCommands.get();            // @0x23c452

        // Re-read registers from rhsResult (binary reads twice).
        AsmRegister dstReg = rhsVals.empty()
            ? AsmRegister(0) : rhsVals.back().reg_;             // @0x23c46a
        AsmRegister srcReg2 = rhsVals.empty()
            ? AsmRegister(0) : rhsVals.back().reg_;             // @0x23c47b

        // addi(srcReg, srcReg, Immediate(1)): increment in-place. @0x23c4c7-23c4e2
        {
            auto addiAsms = asmCmd->addi(dstReg, srcReg2, Immediate(1));
            result->assemblers_.insert(
                result->assemblers_.end(),
                addiAsms.begin(), addiAsms.end());
            // Binary: Immediate dtor vtable store-back (no-op in reconstruction) @0x23c5a6
        }
    }
    // --- Path 3: lhsResult Cvar (post-increment) --- @0x23bf0f
    else if (lhsHas1 && lhsVals.back().varType_ == VarType_Cvar) {

        // Store OLD value in result.                           @0x23c752-23c77d
        double oldVal = lhsVals.back().value_.toDouble();
        result->setValue(VarType_Cvar, Value(oldVal));

        // Compute new value = old + 1.0, update cvar.         @0x23c99a-23c9b3
        double newVal = lhsVals.back().value_.toDouble() + 1.0;
        try {
            res.get()->updateCvar(lhsResult.name_, newVal,
                                  VarSubType_Default);          // @0x23c9b3
        } catch (...) {
            // Exception handler (filter==2, likely VarTypeException). @0x23cc08
            // Error 0x6f with rhsResult info.
            VarType vt = VarType_Unset;
            if (!rhsVals.empty() && rhsVals.size() <= 1)        // @0x23cc1a
                vt = rhsVals.back().varType_;
            ctx.messages->errorMessage(
                ErrorMessages::format(ErrorMessageT(0x6f),
                    str(vt), rhsResult.name_), -1);             // @0x23cc98

            // Catch handler falls through to rhsResult Cvar setValue.
            // Re-extract value from rhsResult, toDouble()+1, setValue.  @0x23c8f9
            Value rhsVal;
            if (!rhsVals.empty())
                rhsVal = rhsVals.back().value_;
            double newRhsVal = rhsVal.toDouble() + 1.0;         // @0x23cd32
            result->setValue(VarType_Cvar, Value(newRhsVal));    // @0x23cd59
        }
    }
    // --- Path 4: rhsResult Cvar (pre-increment) --- @0x23bf6e
    else if (rhsHas1 && rhsVals.back().varType_ == VarType_Cvar) {

        // Compute new value = old + 1.0, update cvar first.   @0x23c8a1-23c8be
        double newVal1 = rhsVals.back().value_.toDouble() + 1.0;
        bool updateOK = true;
        try {
            res.get()->updateCvar(rhsResult.name_, newVal1,
                                  VarSubType_Default);          // @0x23c8be
        } catch (...) {
            // Exception handler (filter==2). @0x23d088
            // Error 0x6f with lhsResult info.
            VarType vt = VarType_Unset;
            if (!lhsVals.empty() && lhsVals.size() <= 1)        // @0x23d09d
                vt = lhsVals.back().varType_;
            ctx.messages->errorMessage(
                ErrorMessages::format(ErrorMessageT(0x6f),
                    str(vt), lhsResult.name_), -1);             // @0x23d119
            updateOK = false;  // skip setValue, go to name_tail @0x23d163→23cdb3
        }

        if (updateOK) {
            // Store NEW value in result.                       @0x23cd26-23cd59
            Value rhsVal2;
            if (!rhsVals.empty())
                rhsVal2 = rhsVals.back().value_;
            double newVal2 = rhsVal2.toDouble() + 1.0;
            result->setValue(VarType_Cvar, Value(newVal2));
        }
    }
    // --- Path 5: lhsResult other (error) --- @0x23bfd2
    else if (lhsHas1 && lhsVals.back().varType_ != VarType_Unset) {
        VarType vt = lhsVals.back().varType_;
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT(0x6f),
                str(vt), lhsResult.name_), -1);                 // @0x23c398
    }
    // --- Path 6: rhsResult other (error) --- @0x23c034
    else if (rhsHas1 && rhsVals.back().varType_ != VarType_Unset) {
        VarType vt = rhsVals.back().varType_;
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT(0x6f),
                str(vt), rhsResult.name_), -1);                 // @0x23c5f8
    }
    // else: both empty/Unset — fall through to name_tail

    // --- Name tail --- @0x23cdb3
    // Literal "++" at rodata 0x905bae.
    result->name_ = lhsResult.name_ + rhsResult.name_ + "++";

    return result;                                              // @0x23cf38
}

// SeqCDec::evaluate(5) — @0x23d2b0, 5464B
//   Decrement operator (-- prefix and postfix).
//   Structurally identical to SeqCInc with:
//     - Immediate(0)/Immediate(1) → Immediate(0)/Immediate(-1)
//     - toDouble() + 1.0 → toDouble() - 1.0
//     - "++" → "--"
//     - error code 0x6f → 0x70
//
std::shared_ptr<EvalResults> SeqCDec::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{
    auto result = std::make_shared<EvalResults>();             // @0x23d2d5

    auto const& lhsVals = lhsResult.values_;
    auto const& rhsVals = rhsResult.values_;
    bool lhsHas1 = !lhsVals.empty() && lhsVals.size() <= 1;
    bool rhsHas1 = !rhsVals.empty() && rhsVals.size() <= 1;

    // --- Path 1: lhsResult Var (post-decrement) ---
    if (lhsHas1 && lhsVals.back().varType_ == VarType_Var) {

        result->assemblers_.insert(
            result->assemblers_.end(),
            lhsResult.assemblers_.begin(), lhsResult.assemblers_.end());

        int newRegNum = Resources::getRegisterNumber();
        result->setValue(VarType_Var, newRegNum);

        AsmCommands* asmCmd = ctx.asmCommands.get();

        AsmRegister newReg = result->values_.empty()
            ? AsmRegister(0) : result->values_.back().reg_;
        AsmRegister srcReg = lhsVals.empty()
            ? AsmRegister(0) : lhsVals.back().reg_;

        // addi(newReg, srcReg, Immediate(0)): copy old value.
        {
            auto addiAsms = asmCmd->addi(newReg, srcReg, Immediate(0));
            result->assemblers_.insert(
                result->assemblers_.end(),
                addiAsms.begin(), addiAsms.end());
            // Binary: Immediate dtor vtable store-back (no-op in reconstruction)
        }

        AsmRegister srcReg2 = lhsVals.empty()
            ? AsmRegister(0) : lhsVals.back().reg_;

        // addi(srcReg, srcReg, Immediate(-1)): decrement.
        {
            auto addiAsms = asmCmd->addi(srcReg2, srcReg2, Immediate(-1));
            result->assemblers_.insert(
                result->assemblers_.end(),
                addiAsms.begin(), addiAsms.end());
            // Binary: Immediate dtor vtable store-back (no-op in reconstruction)
        }
    }
    // --- Path 2: rhsResult Var (pre-decrement) ---
    else if (rhsHas1 && rhsVals.back().varType_ == VarType_Var) {

        result->assemblers_.insert(
            result->assemblers_.end(),
            rhsResult.assemblers_.begin(), rhsResult.assemblers_.end());

        AsmRegister srcReg = rhsVals.empty()
            ? AsmRegister(0) : rhsVals.back().reg_;
        result->setValue(VarType_Var,
            static_cast<int>(srcReg));

        AsmCommands* asmCmd = ctx.asmCommands.get();

        AsmRegister dstReg = rhsVals.empty()
            ? AsmRegister(0) : rhsVals.back().reg_;
        AsmRegister srcReg2 = rhsVals.empty()
            ? AsmRegister(0) : rhsVals.back().reg_;

        // addi(srcReg, srcReg, Immediate(-1)): decrement in-place.
        {
            auto addiAsms = asmCmd->addi(dstReg, srcReg2, Immediate(-1));
            result->assemblers_.insert(
                result->assemblers_.end(),
                addiAsms.begin(), addiAsms.end());
            // Binary: Immediate dtor vtable store-back (no-op in reconstruction)
        }
    }
    // --- Path 3: lhsResult Cvar (post-decrement) ---
    else if (lhsHas1 && lhsVals.back().varType_ == VarType_Cvar) {

        double oldVal = lhsVals.back().value_.toDouble();
        result->setValue(VarType_Cvar, Value(oldVal));

        double newVal = lhsVals.back().value_.toDouble() - 1.0;
        try {
            res.get()->updateCvar(lhsResult.name_, newVal,
                                  VarSubType_Default);
        } catch (...) {
            // Exception handler (filter==2). Error 0x70 with rhsResult info.
            VarType vt = VarType_Unset;
            if (!rhsVals.empty() && rhsVals.size() <= 1)
                vt = rhsVals.back().varType_;
            ctx.messages->errorMessage(
                ErrorMessages::format(ErrorMessageT(0x70),
                    str(vt), rhsResult.name_), -1);

            // Catch falls through to rhsResult Cvar setValue.
            Value rhsVal;
            if (!rhsVals.empty())
                rhsVal = rhsVals.back().value_;
            double newRhsVal = rhsVal.toDouble() - 1.0;
            result->setValue(VarType_Cvar, Value(newRhsVal));
        }
    }
    // --- Path 4: rhsResult Cvar (pre-decrement) ---
    else if (rhsHas1 && rhsVals.back().varType_ == VarType_Cvar) {

        double newVal1 = rhsVals.back().value_.toDouble() - 1.0;
        bool updateOK = true;
        try {
            res.get()->updateCvar(rhsResult.name_, newVal1,
                                  VarSubType_Default);
        } catch (...) {
            // Exception handler (filter==2). Error 0x70 with lhsResult info.
            VarType vt = VarType_Unset;
            if (!lhsVals.empty() && lhsVals.size() <= 1)
                vt = lhsVals.back().varType_;
            ctx.messages->errorMessage(
                ErrorMessages::format(ErrorMessageT(0x70),
                    str(vt), lhsResult.name_), -1);
            updateOK = false;  // skip setValue, go to name_tail
        }

        if (updateOK) {
            Value rhsVal2;
            if (!rhsVals.empty())
                rhsVal2 = rhsVals.back().value_;
            double newVal2 = rhsVal2.toDouble() - 1.0;
            result->setValue(VarType_Cvar, Value(newVal2));
        }
    }
    // --- Path 5: lhsResult other (error) ---
    else if (lhsHas1 && lhsVals.back().varType_ != VarType_Unset) {
        VarType vt = lhsVals.back().varType_;
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT(0x70),
                str(vt), lhsResult.name_), -1);
    }
    // --- Path 6: rhsResult other (error) ---
    else if (rhsHas1 && rhsVals.back().varType_ != VarType_Unset) {
        VarType vt = rhsVals.back().varType_;
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT(0x70),
                str(vt), rhsResult.name_), -1);
    }

    // --- Name tail ---
    // Literal "--" at rodata (SeqCDec counterpart of SeqCInc's "++").
    result->name_ = lhsResult.name_ + rhsResult.name_ + "--";

    return result;
}

// --- Unary 3-arg (all have inline logic) ---

// SeqCNeg::evaluate(3) — @0x2284e0, 2009B
//   Arithmetic negation (-x). Standalone 3-arg unary evaluate.
//
//   Algorithm:
//     1. Set lineNr on messages, state, wavetable.
//     2. Evaluate child via this->expr() (virtual dispatch slot 0).
//     3. Dispatch on childResult->values_.back().varType_:
//        - Var(2):       asmZero(tempReg); subr(tempReg, childReg) → 0-x
//        - Const(4)/Cvar(6): toDouble() → XOR sign bit → setValue(double)
//        - Wave(5):      scaleWaveform(-1, childResult, ctx)
//        - Other/null:   error 0x7c
//     4. Return mutated childResult (or error EvalResults).
//
std::shared_ptr<EvalResults> SeqCNeg::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // 1. Set line number.                                     // @0x228504
    int lineNr = this->lineNr_;                                  // [rsi+0x0c] = SeqCAstNode::lineNr_
    ctx.messages->setLineNr(lineNr);                           // @0x22850c
    ctx.asmCommands->setWavetableFrontIndex(lineNr);             // @0x228515: [r14+0x8]->+0x50
    ctx.wavetable->setLineNr(lineNr);                          // @0x22851e

    // 2. Evaluate child.                                      // @0x228523
    SeqCAstNode* child = const_cast<SeqCAstNode*>(this->expr()); // @0x228526
    auto childResult = child->evaluate(res, ctx, state);       // @0x22855d (virtual slot 0)

    // 3. Check result and dispatch.
    if (!childResult) {                                        // @0x228582
        // Null result — error 0x7c with VarSubType(0).        // @0x2285ea
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x7c), str(VarSubType(0)));          // @0x2285f3
        ctx.messages->errorMessage(msg, -1);                   // @0x228620
        auto result = std::make_shared<EvalResults>();         // @0x228661
        return result;                                         // @0x228b4a
    }

    auto& values = childResult->values_;
    size_t count = values.size();
    if (count == 0) {                                          // @0x228591
        // Empty values — same null-result error path.         // @0x2286ec
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x7c), str(VarType_Unset));             // @0x22870e
        ctx.messages->errorMessage(msg, -1);                   // @0x228722
        return childResult;                                    // @0x228b4a
    }

    VarType vt = values.back().varType_;                       // @0x228767: [rbx-0x38]

    if (count > 1) {                                           // @0x2285b3
        // Multiple values — error 0x7c.                       // @0x2286f1
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x7c), str(vt));                     // @0x22870e
        ctx.messages->errorMessage(msg, -1);                   // @0x228722
        return childResult;                                    // @0x228b4a
    }

    // Single-value dispatch.
    if (vt == VarType_Var) {                                   // @0x228767: cmp 2
        // Var: emit asmZero + subr (0 - x).
        AsmRegister tempReg(Resources::getRegisterNumber());   // @0x228771
        auto& asmCtx = *ctx.asmCommands;                      // @0x228789: [r14+0x8]

        // asmZero(tempReg): loads zero into tempReg.          // @0x228798
        auto zeroAsm = asmCtx.asmZero(tempReg);
        childResult->assemblers_.push_back(zeroAsm);           // @0x2287ad

        // Get child's register from last EvalResultValue.     // @0x2288b1
        AsmRegister childReg(0);
        if (!values.empty() && values.begin() != values.end()) {
            childReg = values.back().reg_;                     // @0x2288bf: [r12+0x08]-0x08
        }

        // subr(tempReg, childReg): tempReg = tempReg - childReg. // @0x2288e3
        auto subAsm = asmCtx.subr(tempReg, childReg);
        childResult->assemblers_.push_back(subAsm);            // @0x2288e8

        // Update result type.                                 // @0x2289b9
        childResult->setValue(VarType_Var,
            static_cast<int>(tempReg));                        // @0x2289be
        return childResult;

    } else if ((static_cast<int>(vt) | 2) == 6) {             // @0x22880c: Const(4) or Cvar(6)
        // Const/Cvar: extract Value, convert to double, negate.
        Value val = values.back().value_;                      // @0x22881b

        // toDouble() on the extracted value.                  // @0x228b17
        double negatedDouble = val.toDouble();

        // Negate: XOR sign bit (IEEE 754 sign flip).          // @0x228b20
        negatedDouble = -negatedDouble;

        // Store back as double.                               // @0x228b2a
        childResult->setValue(negatedDouble);
        return childResult;                                    // @0x228b4a

    } else if (vt == VarType_Wave) {                           // @0x2289c3: cmp 5
        // Wave: scale waveform by -1.                         // @0x2289fa
        auto result = scaleWaveform(0, childResult, ctx);      // @0x2289fa (scaleWaveform @0x228cc0)
        return result;                                         // @0x228b4a

    } else {
        // Unsupported type — error 0x7c.                      // @0x2285b9
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x7c), str(vt));                     // @0x22870e
        ctx.messages->errorMessage(msg, -1);                   // @0x228722
        return childResult;                                    // @0x228b4a
    }
}

// SeqCInv::evaluate(3) — @0x228f50, 2608B
//   Bitwise inversion (~x). Standalone 3-arg unary evaluate.
//
//   Algorithm:
//     1. Set lineNr on messages, state, wavetable.
//     2. Evaluate child via this->expr() (virtual dispatch slot 0).
//     3. Dispatch on childResult->values_.back().varType_:
//        - Var(2):       addi(tempReg, R0, -1) [×2]; subr(tempReg, childReg) → -1-x = ~x
//        - Const(4)/Cvar(6): toInt(); warn if negative (0xfe); NOT; setValue(Value)
//        - Other/null:   error 0x77
//     4. Return mutated childResult (or error EvalResults).
//
//   Binary: Wave(5) is NOT supported by bitwise inversion — falls to error.
//   Binary: Var case emits addi(tempReg, R0, Immediate(-1)) TWICE before the subr.
//         This may be for dual-issue pipelines or a codegen quirk.
//   Binary: Const/Cvar case uses toInt() (not toDouble), stays in integer domain.
//
std::shared_ptr<EvalResults> SeqCInv::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // 1. Set line number.                                     // @0x228f74
    int lineNr = this->lineNr_;                                  // [rsi+0x0c] = SeqCAstNode::lineNr_
    ctx.messages->setLineNr(lineNr);                           // @0x228f7c
    ctx.asmCommands->setWavetableFrontIndex(lineNr);             // @0x228f85: [r14+0x8]->+0x50
    ctx.wavetable->setLineNr(lineNr);                          // @0x228f8e

    // 2. Evaluate child.                                      // @0x228f93
    SeqCAstNode* child = const_cast<SeqCAstNode*>(this->expr()); // @0x228f96
    auto childResult = child->evaluate(res, ctx, state);       // @0x228fcd (virtual slot 0)

    // 3. Check result and dispatch.
    if (!childResult) {                                        // @0x228ff2
        // Null result — error 0x77 with VarSubType(0).        // @0x22905a
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x77), str(VarSubType(0)));          // @0x229079
        ctx.messages->errorMessage(msg, -1);                   // @0x229096
        auto result = std::make_shared<EvalResults>();         // @0x2290e0
        return result;                                         // @0x229779
    }

    auto& values = childResult->values_;
    size_t count = values.size();
    if (count == 0) {                                          // @0x229001
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x77), str(VarType_Unset));             // @0x22918e
        ctx.messages->errorMessage(msg, -1);                   // @0x2291a7
        return childResult;                                    // @0x229779
    }

    VarType vt = values.back().varType_;                       // @0x2291f5: [rbx-0x38]

    if (count > 1) {                                           // @0x229023
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x77), str(vt));                     // @0x22918e
        ctx.messages->errorMessage(msg, -1);                   // @0x2291a7
        return childResult;                                    // @0x229779
    }

    // Single-value dispatch.
    if (vt == VarType_Var) {                                   // @0x2291f5: cmp 2
        // Var: emit addi(temp, R0, -1) twice, then subr(temp, child).
        // ~x = -1 - x in two's complement.
        AsmRegister tempReg(Resources::getRegisterNumber());   // @0x2291ff
        auto& asmCtx = *ctx.asmCommands;                      // @0x229213

        // First addi(tempReg, R0, Immediate(-1)).             // @0x229254
        {
            AsmRegister zeroReg(0);                            // @0x229224
            Immediate immNeg1(-1);                             // @0x229238
            auto asmList = asmCtx.addi(tempReg, zeroReg, immNeg1); // @0x229254
            // Insert into childResult->assemblers_ at end.    // @0x22928a
            childResult->assemblers_.insert(
                childResult->assemblers_.end(),
                asmList.begin(), asmList.end());
        }                                                      // @0x229374 (cleanup)

        // Second addi(tempReg, R0, Immediate(-1)).            // @0x2293eb
        {
            AsmRegister zeroReg(0);                            // @0x2293bb
            Immediate immNeg1(-1);                             // @0x2293cf
            auto asmList = asmCtx.addi(tempReg, zeroReg, immNeg1); // @0x2293eb
            childResult->assemblers_.insert(
                childResult->assemblers_.end(),
                asmList.begin(), asmList.end());
        }                                                      // @0x2294bf (cleanup)

        // Get child's register.                               // @0x229501
        AsmRegister childReg(0);
        if (!values.empty() && values.begin() != values.end()) {
            childReg = values.back().reg_;                     // @0x22950b
        }

        // subr(tempReg, childReg): tempReg = tempReg - childReg. // @0x229533
        auto subAsm = asmCtx.subr(tempReg, childReg);
        childResult->assemblers_.push_back(subAsm);            // @0x229538

        // Update result type.                                 // @0x2295fd
        childResult->setValue(VarType_Var,
            static_cast<int>(tempReg));
        return childResult;                                    // @0x229602

    } else if ((static_cast<int>(vt) | 2) == 6) {             // @0x22930f: Const(4) or Cvar(6)
        // Const/Cvar: extract Value, convert to int, bitwise NOT.
        Value val = values.back().value_;                      // @0x22931e

        // toInt() on the extracted value.                     // @0x22966f
        int intVal = val.toInt();

        // Warn if negative (error 0xfe).                      // @0x2296ad
        if (intVal < 0) {
            auto msg = ErrorMessages::format(
                ErrorMessageT(0xfe), std::to_string(intVal));  // @0x2296d7
            ctx.messages->errorMessage(msg, -1);               // @0x2296eb
        }

        // Bitwise NOT.                                        // @0x229730
        int inverted = ~intVal;

        // Construct Value with int payload and store.         // @0x229733
        Value result(inverted);                                // which_=0, lineNr_=1 (int)
        childResult->setValue(result);                          // @0x229759
        return childResult;                                    // @0x229779

    } else {
        // Unsupported type (including Wave) — error 0x77.     // @0x229029
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x77), str(vt));                     // @0x22918e
        ctx.messages->errorMessage(msg, -1);                   // @0x2291a7
        return childResult;                                    // @0x229779
    }
}

// SeqCNotExpr::evaluate(3) — @0x229980 (3027B, ends @0x22a553)
//   Logical NOT (!x). Fully inline — does NOT call invertBool/valueToBool.
//
//   Algorithm:
//     1. Set lineNr on messages, asmCommands, wavetable.
//     2. Evaluate child via this->child() (virtual dispatch slot 0).
//     3. Dispatch on childResult->values_.back().varType_:
//        - Var(2):       6-instruction branch sequence:
//                         asmZero(dest); brz(src, nzero); br(end);
//                         nzero: asmOne(dest); end:
//        - Const(4)/Cvar(6): toInt() → nonzero ? setValue(Value(0)) : setValue(Value(1))
//        - String(3)/Wave(5): toString() → nonempty ? setValue(Value(1)) : setValue(Value(0))
//                         Binary: String/Wave path computes bool(x), not !x.
//                         This appears to be a bug in the original binary.
//        - Other/null:   error 0x86
//     4. Return mutated childResult (or error EvalResults).
//
std::shared_ptr<EvalResults> SeqCNotExpr::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // 1. Set line number.                                     // @0x2299a7
    int lineNr = this->lineNr_;                                  // [rsi+0x0c] = SeqCAstNode::lineNr_
    ctx.messages->setLineNr(lineNr);                           // @0x2299b1
    ctx.asmCommands->setWavetableFrontIndex(lineNr);           // @0x2299b6: [r14+0x8]->+0x50
    ctx.wavetable->setLineNr(lineNr);                          // @0x2299c5

    // 2. Evaluate child.                                      // @0x2299ca
    SeqCAstNode* child = const_cast<SeqCAstNode*>(this->expr()); // @0x2299cd
    auto childResult = child->evaluate(res, ctx, state);       // @0x229a01 (virtual slot 0)

    // 3. Check result and dispatch.
    if (!childResult) {                                        // @0x229a2d
        // Null result — error 0x86 with VarSubType(0).        // @0x229a95
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x86), str(VarSubType(0)));          // @0x229aa1
        ctx.messages->errorMessage(msg, -1);                   // @0x229acd
        auto result = std::make_shared<EvalResults>();         // @0x229b12
        return result;                                         // @0x229c2a
    }

    auto& values = childResult->values_;
    size_t count = values.size();
    if (count == 0) {                                          // @0x229a39
        // Empty values — error 0x86.                          // @0x229ba4
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x86), str(VarType_Unset));             // @0x229bb0
        ctx.messages->errorMessage(msg, -1);                   // @0x229bdc
        return childResult;                                    // @0x229c2a
    }

    VarType vt = values.back().varType_;                       // @0x229c3f: [rbx-0x38]

    if (count > 1) {                                           // @0x229a5e
        // Multiple values — error 0x86 with VarType_Unset.       // @0x229a64
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x86), str(VarType_Unset));             // @0x229bb0
        ctx.messages->errorMessage(msg, -1);                   // @0x229bdc
        return childResult;                                    // @0x229c2a
    }

    // Single-value dispatch.
    if (vt == VarType_Var) {                                   // @0x229c3f: cmp 2
        // Var: 6-instruction NOT branch sequence.             // @0x229c49
        AsmRegister destReg(Resources::getRegisterNumber());   // @0x229c49
        std::string nzeroLabel = Resources::newLabel("nzero"); // @0x229c7e
        std::string endLabel = Resources::newLabel("end");     // @0x229cc2

        // Get source register from last value.               // @0x229d0a
        AsmRegister srcReg(0);
        if (!values.empty() && values.begin() != values.end()) {
            srcReg = values.back().reg_;                       // @0x229d17
        }

        // Emit:  asmZero(dest); brz(src, nzero); br(end);
        //        nzero: asmOne(dest); end:                    // @0x229cfa-229de8
        auto asm1 = ctx.asmCommands->asmZero(destReg);        // @0x229cfa
        auto asm2 = ctx.asmCommands->brz(srcReg, nzeroLabel, false);  // @0x229d84
        auto asm3 = ctx.asmCommands->br(endLabel, false);     // @0x229da0
        auto asm4 = ctx.asmCommands->asmLabel(nzeroLabel);    // @0x229db7
        auto asm5 = ctx.asmCommands->asmOne(destReg);         // @0x229dce
        auto asm6 = ctx.asmCommands->asmLabel(endLabel);      // @0x229de8

        AsmList::Asm cmds[] = { std::move(asm1), std::move(asm2),
                                std::move(asm3), std::move(asm4),
                                std::move(asm5), std::move(asm6) };

        childResult->assemblers_.insert(
            childResult->assemblers_.end(),
            std::make_move_iterator(std::begin(cmds)),
            std::make_move_iterator(std::end(cmds)));          // @0x229e43

        childResult->setValue(VarType_Var,
            static_cast<int>(destReg));                        // @0x22a04f
        return childResult;                                    // @0x229c2a

    } else if ((static_cast<int>(vt) | 2) == 6) {             // @0x229d1d: Const(4) or Cvar(6)
        // Const/Cvar: toInt(), invert boolean.
        Value val = values.back().value_;                      // @0x229d2d

        int intVal = val.toInt();                              // @0x22a139

        if (intVal != 0) {                                     // @0x22a178
            // !nonzero = 0.                                   // @0x22a17c
            Value zeroVal(static_cast<int32_t>(0));
            childResult->setValue(zeroVal);                     // @0x22a1a1
        } else {
            // !0 = 1.                                         // @0x22a1ab
            Value oneVal(static_cast<int32_t>(1));
            childResult->setValue(oneVal);                      // @0x22a1d0
        }
        return childResult;                                    // @0x229c2a

    } else if (vt == VarType_Wave || vt == VarType_String) {   // @0x22a08b-22a095
        // String/Wave: toString(), check length.
        // Binary: computes bool(x), not !x, for this path.
        // nonempty → 1, empty → 0. This is likely a bug in the
        // original source — it should probably be inverted.   // @0x22a09b
        Value val = values.back().value_;                      // @0x22a09b

        std::string s = val.toString();                        // @0x22a246
        size_t len = s.size();                                 // @0x22a24b-22a257

        if (len != 0) {                                        // @0x22a2a4
            // Nonempty → 1 (BUG: should be 0 for NOT).       // @0x22a2a9
            Value oneVal(static_cast<int32_t>(1));
            childResult->setValue(oneVal);                      // @0x22a2ce
        } else {
            // Empty → 0 (BUG: should be 1 for NOT).          // @0x22a309
            Value zeroVal(static_cast<int32_t>(0));
            childResult->setValue(zeroVal);                     // @0x22a32e
        }
        return childResult;                                    // @0x229c2a

    } else {
        // Unsupported type — error 0x86.                      // @0x229a64 (via 22a095)
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x86), str(vt));                     // @0x229bb0
        ctx.messages->errorMessage(msg, -1);                   // @0x229bdc
        return childResult;                                    // @0x229c2a
    }
}

// SeqCReturnStatement::evaluate(3) — @0x226a50, 6800B
//   Return statement handling.
//
//   Two main branches:
//     1. No child expression (bare `return;`): checks return type == Void.
//     2. Has child expression: evaluates child, dispatches on return type
//        (Var, Const/Cvar, String, Wave) to copy/emit the return value.
//
//   Common tail: emits br(state.labelStack.back()) if inside a function scope,
//     sets result->returnEncountered_ = true to signal "return was encountered" to
//     the caller (e.g. SeqCStmtList checks this to stop processing).
//
//   Error codes:
//     0xb2 — bare return in non-void function
//     0xb4 — unsupported return type (catch-all)
//     0xb5 — return type / child value type mismatch
//     0xb6 — null child evaluation result
//     0xe9 — waveform name not found in wavetable
//
std::shared_ptr<EvalResults> SeqCReturnStatement::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // ---- Prologue: set lineNr on subsystems ----                 // @0x226a64
    const int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);                                // @0x226a7f
    ctx.asmCommands->setWavetableFrontIndex(lineNr);                // @0x226a88
    ctx.wavetable->setLineNr(lineNr);                               // @0x226a95

    auto result = std::make_shared<EvalResults>();                  // @0x226a9a
    Resources* resPtr = res.get();

    // ================================================================
    // Branch 1: No child expression (bare `return;`)               // @0x226b01
    // ================================================================
    if (!expr()) {
        VarType returnType = resPtr->getReturnType();               // @0x226bd5

        if (returnType != VarType_Void) {                           // @0x226bdd
            // Error 0xb2: bare return in non-void function.        // @0x226ccf
            auto msg = ErrorMessages::format(
                ErrorMessageT(0xb2), str(returnType));              // @0x226cff
            ctx.messages->errorMessage(msg, -1);                    // @0x226d13
            return std::make_shared<EvalResults>();                 // @0x226d58
        }

        result->name_ = "return";                                   // @0x226bea
        goto return_tail;                                           // @0x2271dc
    }

    // ================================================================
    // Branch 2: Has child expression — evaluate it                 // @0x226b10
    // ================================================================
    {
        auto childResult = expr()->evaluate(res, ctx, state);      // @0x226b13..226b56

        if (!childResult) {                                         // @0x226b7f
            // Null child result — BST lookup for error 0xb6.       // @0x226dd4
            ctx.messages->errorMessage(
                ErrorMessages::messages.at(
                    static_cast<int>(ErrorMessageT(0xb6))),
                -1);                                                // @0x226e27
            return result;                                          // @0x226e2c
        }

        // Copy child assemblers into result.                       // @0x226b88
        result->assemblers_.insert(
            result->assemblers_.end(),
            childResult->assemblers_.begin(),
            childResult->assemblers_.end());

        // Build name: "return " + childResult->name_.              // @0x226bbc
        result->name_ = "return " + childResult->name_;

        VarType returnType = resPtr->getReturnType();               // @0x226eaa

        // ============================================================
        // Dispatch on return type
        // ============================================================

        if (returnType == VarType_Var) {
            // ---- Var return type ----                             // @0x226eb2
            size_t count = childResult->values_.size();
            VarType childVarType = VarType_Unset;
            if (count == 1) {
                childVarType = childResult->values_.back().varType_;
            }

            if (count == 1 &&
                (childVarType == VarType_Var ||
                 (static_cast<int>(childVarType) | 2) == 6))       // Const(4)|2==6, Cvar(6)|2==6
            {                                                       // @0x227243
                AsmRegister returnReg = resPtr->getReturnReg();     // @0x227246

                // Re-check: register copy only for exact Var type. // @0x227289
                if (childVarType == VarType_Var) {
                    // Register-to-register copy via Immediate(0).  // @0x2272a6
                    AsmRegister childReg =
                        childResult->values_.back().reg_;           // @0x22729b
                    auto addiResult = ctx.asmCommands->addi(
                        returnReg, childReg, Immediate(0));         // @0x2272c4
                    result->assemblers_.insert(
                        result->assemblers_.end(),
                        addiResult.begin(), addiResult.end());
                    // Binary: Immediate dtor vtable store-back (no-op in reconstruction)
                    //       @0x227c50
                } else {
                    // Value-based load for Const/Cvar child.       // @0x2274c6
                    Value childVal = childResult->getValue();       // @0x2274ed (manual extraction)
                    auto addiResult = ctx.asmCommands->addi(
                        returnReg, AsmRegister(0), childVal);       // @0x227af4
                    result->assemblers_.insert(
                        result->assemblers_.end(),
                        addiResult.begin(), addiResult.end());
                }
                goto return_tail;                                   // @0x227c05/227c5d
            }

            // Mismatch: error 0xb5.                                // @0x226eea
            {
                auto msg = ErrorMessages::format(
                    ErrorMessageT(0xb5),
                    str(returnType), str(childVarType));            // @0x226f63
                ctx.messages->errorMessage(msg, -1);                // @0x226f77
                return std::make_shared<EvalResults>();             // @0x226fb6
            }
        }

        if (returnType == VarType_Const ||
            returnType == VarType_Cvar) {
            // ---- Const/Cvar return type ----                     // @0x226fc5
            size_t count = childResult->values_.size();
            if (count == 1) {
                VarType cvt = childResult->values_.back().varType_;
                if (cvt == VarType_Const || cvt == VarType_Cvar) {  // @0x22747c
                    // Match: extract value, set on result + resources.
                    {
                        Value childVal = childResult->getValue();   // @0x227425
                        result->setValue(childVal);                  // @0x227434
                    }
                    {
                        Value retVal = childResult->getValue();     // @0x227453
                        resPtr->setReturnValue(retVal);             // @0x227462
                    }
                    goto return_tail;                               // @0x227477
                }
            }

            // Mismatch: error 0xb5.                                // @0x227015
            VarType childVarType = VarType_Unset;
            if (count == 1)
                childVarType = childResult->values_.back().varType_;
            {
                auto msg = ErrorMessages::format(
                    ErrorMessageT(0xb5),
                    str(returnType), str(childVarType));            // @0x22708f
                ctx.messages->errorMessage(msg, -1);                // @0x2270a3
                return std::make_shared<EvalResults>();             // @0x2270e2
            }
        }

        if (returnType == VarType_String) {
            // ---- String return type ----                         // @0x2273c6
            size_t count = childResult->values_.size();
            VarType childVarType = VarType_Unset;
            if (count == 1)
                childVarType = childResult->values_.back().varType_;

            if (childVarType == returnType) {                       // @0x227412
                // Match: extract value, set on result + resources.
                {
                    Value childVal = childResult->getValue();       // @0x22741a
                    result->setValue(childVal);                      // @0x227434
                }
                {
                    Value retVal = childResult->getValue();         // @0x227453
                    resPtr->setReturnValue(retVal);                 // @0x227462
                }
                goto return_tail;                                   // @0x227477
            }

            // Mismatch: error 0xb5.                                // @0x227665
            {
                auto msg = ErrorMessages::format(
                    ErrorMessageT(0xb5),
                    str(returnType), str(childVarType));            // @0x2276de
                ctx.messages->errorMessage(msg, -1);                // @0x2276f2
                return std::make_shared<EvalResults>();             // @0x227731
            }
        }

        if (returnType == VarType_Wave) {
            // ---- Wave return type ----                           // @0x227523
            size_t count = childResult->values_.size();

            if (count == 1) {
                VarType cvt = childResult->values_.back().varType_;

                if (cvt == VarType_Wave) {                          // @0x227a59
                    // Exact Wave match: getValue → setValue + setReturnValue.
                    {
                        Value childVal = childResult->getValue();   // @0x227a6a
                        result->setValue(childVal);                  // @0x227a79
                    }
                    {
                        Value retVal = childResult->getValue();     // @0x227a9c
                        resPtr->setReturnValue(retVal);             // @0x227aab
                    }
                    goto return_tail;                               // @0x227ab7→22746e→227477
                }

                if (cvt == VarType_String) {                        // @0x227e13
                    VarSubType childSub =
                        childResult->values_.back().varSubType_;
                    if (childSub == VarSubType_FunctionArg) {       // @0x227e21
                        // String with FunctionArg subtype: skip value copy,
                        // just signal return.
                        goto return_tail;                           // @0x227cc7
                    }

                    // Check if waveform name exists.               // @0x227e27
                    {
                        Value childVal = childResult->getValue();   // @0x227e32
                        std::string waveName = childVal.toString(); // @0x227e42
                        bool exists = ctx.wavetable->waveformExists(
                            waveName);                              // @0x227e4e

                        if (exists) {                               // @0x227e78
                            // Waveform found: set with Wave type.  // @0x227f4e
                            VarSubType wvSub = VarSubType_Default;
                            if (childResult->values_.size() == 1)
                                wvSub = childResult->values_.back().varSubType_;
                            Value wvVal = childResult->getValue();
                            result->setValue(
                                VarType_Wave, wvSub, wvVal);       // @0x227f6b
                            {
                                Value retVal = result->getValue();
                                resPtr->setReturnValue(retVal);     // @0x227f98
                            }
                        } else {
                            // Waveform not found: error 0xe9.      // @0x227eb6
                            Value val2 = childResult->getValue();   // @0x227ec4
                            auto msg = ErrorMessages::format(
                                ErrorMessageT(0xe9),
                                val2.toString());                   // @0x227eec
                            ctx.messages->errorMessage(msg, -1);    // @0x227efd
                        }
                    }
                    goto return_tail;                               // @0x227f45→227cc7
                }
                // Neither Wave nor String: fall through to error.
            }

            // Count != 1 or unsupported child type: error 0xb5.    // @0x22756a
            VarType childVarType = VarType_Unset;
            if (count == 1)
                childVarType = childResult->values_.back().varType_;
            {
                auto msg = ErrorMessages::format(
                    ErrorMessageT(0xb5),
                    str(returnType), str(childVarType));            // @0x2275e0
                ctx.messages->errorMessage(msg, -1);                // @0x2275f4
                return std::make_shared<EvalResults>();             // @0x227633
            }
        }

        // ---- Catch-all: unsupported return type — error 0xb4 ---- // @0x227740
        {
            VarType childVarType = VarType_Unset;
            if (childResult->values_.size() == 1)
                childVarType = childResult->values_.back().varType_;
            auto msg = ErrorMessages::format(
                ErrorMessageT(0xb4), str(childVarType));            // @0x227799
            ctx.messages->errorMessage(msg, -1);                    // @0x2277ad
            return std::make_shared<EvalResults>();                 // @0x2277f2
        }
    }

return_tail:                                                        // @0x227cc7
    // Emit branch instruction if inside a function scope.          // @0x227cf3
    if (!state.labelStack.empty()) {                                   // @0x227d05
        auto brAsm = ctx.asmCommands->br(
            state.labelStack.back(), false);                           // @0x227d20
        result->assemblers_.push_back(std::move(brAsm));            // @0x227d2f
    }

    // Signal "return was encountered" to callers (e.g. SeqCStmtList). @0x227dea
    result->returnEncountered_ = true;
    return result;                                                  // @0x227dfe
}

// --- List nodes 3-arg ---

// SeqCArgList::evaluate(3) — @0x211de0 (1284B)
//
// Iterates elements_, evaluates each child, accumulates results.
// Name is built as comma-separated concatenation of child names.
// On null child result: error 0x12 with "arglist".
//
// Binary address annotations:
//   0x211de0–0x211e8f  Prologue, set lineNr, allocate result
//   0x211e8f–0x211ea2  Get children (params() accessor), empty check
//   0x211ea2–0x2121eb  Iteration loop: evaluate, accumulate, name build
//   0x212005–0x212028  Null-result error path: format(0x12, "arglist")
//   0x2121f1–0x21223d  Cleanup and return
// ============================================================================
std::shared_ptr<EvalResults> SeqCArgList::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // 1. Set line number.                                        @0x211e0b
    int lineNr = this->lineNr();
    ctx.messages->setLineNr(lineNr);
    ctx.asmCommands->setWavetableFrontIndex(lineNr);
    ctx.wavetable->setLineNr(lineNr);

    // 2. Allocate empty result.                                  @0x211e35
    auto result = std::make_shared<EvalResults>();

    // 3. Iterate elements.                                       @0x211e8f
    auto const& elems = elements();
    if (elems.empty()) return result;

    bool hasError = false;
    for (size_t i = 0; i < elems.size(); ++i) {                // @0x211ea2
        auto childResult = elems[i]->evaluate(res, ctx, state); // @0x211efb

        if (childResult) {                                       // @0x211f23
            // Accumulate assemblers and values.                  @0x211f42-211f8e
            result->assemblers_.insert(
                result->assemblers_.end(),
                childResult->assemblers_.begin(),
                childResult->assemblers_.end());
            result->values_.insert(
                result->values_.end(),
                childResult->values_.begin(),
                childResult->values_.end());

            // Sticky-OR hasError.                                @0x211f98-211fa7
            result->returnEncountered_ = result->returnEncountered_ || childResult->returnEncountered_;

            // Build comma-separated name.                        @0x211faf
            if (i != 0) {
                result->name_ += ", " + childResult->name_;      // @0x211fc5
            } else {
                result->name_ = childResult->name_;              // @0x212060
            }
        } else {
            // Null result — error 0x12.                          @0x212005-212028
            std::string msg = ErrorMessages::format(
                ErrorMessageT(0x12), std::string("arglist"));
            ctx.messages->errorMessage(msg, -1);
            hasError = true;
        }
    }

    if (hasError) {
        return std::make_shared<EvalResults>();                  // @0x21220b
    }
    return result;                                               // @0x21223d
}

// SeqCDeclList::evaluate(3) — @0x2122f0 (1284B)
//
// Byte-for-byte identical to SeqCArgList except:
//   - accessor: decls() instead of params()  (functionally, elements())
//   - error string: "decllist" instead of "arglist"
// ============================================================================
std::shared_ptr<EvalResults> SeqCDeclList::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // 1. Set line number.                                        @0x21231b
    int lineNr = this->lineNr();
    ctx.messages->setLineNr(lineNr);
    ctx.asmCommands->setWavetableFrontIndex(lineNr);
    ctx.wavetable->setLineNr(lineNr);

    // 2. Allocate empty result.                                  @0x212345
    auto result = std::make_shared<EvalResults>();

    // 3. Iterate elements.                                       @0x21239f
    auto const& elems = elements();
    if (elems.empty()) return result;

    bool hasError = false;
    for (size_t i = 0; i < elems.size(); ++i) {                // @0x2123b2
        auto childResult = elems[i]->evaluate(res, ctx, state); // @0x21240b

        if (childResult) {
            // Accumulate assemblers and values.                  @0x212452-21249e
            result->assemblers_.insert(
                result->assemblers_.end(),
                childResult->assemblers_.begin(),
                childResult->assemblers_.end());
            result->values_.insert(
                result->values_.end(),
                childResult->values_.begin(),
                childResult->values_.end());

            // Sticky-OR hasError.                                @0x2124a8-2124b7
            result->returnEncountered_ = result->returnEncountered_ || childResult->returnEncountered_;

            // Build comma-separated name.                        @0x2124bf
            if (i != 0) {
                result->name_ += ", " + childResult->name_;
            } else {
                result->name_ = childResult->name_;
            }
        } else {
            // Null result — error 0x12.                          @0x212515-212538
            std::string msg = ErrorMessages::format(
                ErrorMessageT(0x12), std::string("decllist"));
            ctx.messages->errorMessage(msg, -1);
            hasError = true;
        }
    }

    if (hasError) {
        return std::make_shared<EvalResults>();
    }
    return result;
}

// SeqCStmtList::evaluate(3) — @0x212800 (2245B)
//
// Same core loop as SeqCArgList/SeqCDeclList, but with extra logic:
//   - Null child pointer check (ArgList/DeclList don't check).
//   - Return statement detection via dynamic_cast: if a child returns
//     with returnEncountered_==true and the CURRENT child is a SeqCReturnStatement,
//     emit unreachable-code warning (msg 0x22) for the NEXT child's line.
//   - Return value extraction: on each iteration, extracts the last value
//     from childResult and stores it in Resources via setReturnValue().
//   - Loop breaks immediately when any child returns returnEncountered_==true.
//
// Binary address annotations:
//   0x212800–0x21285a  Prologue, set lineNr, allocate result
//   0x21285a–0x2128e4  Get children (stmts() accessor), null-check
//   0x2128e4–0x212ec4  Iteration loop body: evaluate, accumulate, name
//   0x2129cf–0x212b90  Return stmt detection + unreachable code warning
//   0x212b90–0x212e84  Return value extraction + force hasError
//   0x212ec4–0x212f9e  Loop termination on child hasError
// ============================================================================
std::shared_ptr<EvalResults> SeqCStmtList::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // 1. Set line number.                                        @0x21282b
    int lineNr = this->lineNr();
    ctx.messages->setLineNr(lineNr);
    ctx.asmCommands->setWavetableFrontIndex(lineNr);
    ctx.wavetable->setLineNr(lineNr);

    // 2. Allocate empty result.                                  @0x21285a
    auto result = std::make_shared<EvalResults>();

    // 3. Iterate elements.                                       @0x2128b4
    auto const& elems = elements();
    if (elems.empty()) return result;

    for (size_t i = 0; i < elems.size(); ++i) {                // @0x2128c2
        // Null child check (StmtList-specific).                  @0x2128e4
        if (!elems[i]) continue;

        std::shared_ptr<EvalResults> childResult;
        try {
            childResult = elems[i]->evaluate(res, ctx, state); // @0x21291b
        } catch (std::exception const& e) {
            ctx.messages->errorMessage(std::string(e.what()), -1);
            continue;
        }

        bool childUnwound = false;

        if (childResult) {
            // Accumulate assemblers and values.                  @0x212962-2129ae
            result->assemblers_.insert(
                result->assemblers_.end(),
                childResult->assemblers_.begin(),
                childResult->assemblers_.end());
            result->values_.insert(
                result->values_.end(),
                childResult->values_.begin(),
                childResult->values_.end());

            childUnwound = childResult->returnEncountered_;               // @0x2129c5-2129cc

            if (!childUnwound) {
                // --- childUnwound == 0 path ---                @0x2129cf→0x212a80
                // Build comma-separated name.                    @0x2129b8/0x212a80
                if (i != 0) {
                    result->name_ += ", " + childResult->name_;
                } else {
                    result->name_ = childResult->name_;
                }

                // Chain node_ from child into result.            @0x212c59-212dd1
                if (childResult->node_) {
                    if (!result->node_) {
                        result->node_ = childResult->node_;
                    } else {
                        auto tail = result->node_;
                        while (tail->next) tail = tail->next;
                        tail->next = childResult->node_;
                    }
                }
            } else {
                // --- childUnwound == 1 path ---                @0x2129cf fall-through
                // Unreachable code after return statement check. @0x2129d5
                if (i + 1 < elems.size()) {
                    if (dynamic_cast<const SeqCReturnStatement*>(elems[i].get())) {
                        int nextLineNr = elems[i + 1]->lineNr();   // @0x212b7e
                        std::string const& warnMsg =
                            ErrorMessages::get(0x22);             // @0x212a29-212a76
                        ctx.messages->warningMessage(warnMsg, nextLineNr); // @0x212b86
                    }
                }

                // Extract return value from child result.        @0x212b90-212e4d
                if (!childResult->values_.empty()) {
                    res->setReturnValue(childResult->values_.back().value_); // @0x212e4d
                } else {
                    res->setReturnValue(Value(static_cast<int32_t>(0)));     // @0x212bd9
                }
                result->returnEncountered_ = true;                         // @0x212e84
            }

        } else {
            // Null result — error 0x12.                          @0x212905
            std::string msg = ErrorMessages::format(
                ErrorMessageT(0x12), std::string("stmtlist"));
            ctx.messages->errorMessage(msg, -1);
            childUnwound = true;
        }

        // Break loop on child error.                             @0x212ec4-212f9e
        if (childUnwound) break;
    }

    return result;                                               // @0x212fa0
}

// --- Two-child 3-arg ---

// SeqCFunctionCall::evaluate(3) — @0x20c6a0, 15220B
//   Evaluates a function call expression.
//   Two-path dispatch:
//     (A) Known function (functionExists == true):
//         Generate return label, evaluate args, build overload signature,
//         look up Function, bind params, execute body, emit return label,
//         set return value.
//     (B) Unknown function (functionExists == false):
//         Create sub-scope, evaluate args in sub-scope, delegate to
//         CustomFunctions::call().
//
//   Binary address annotations:
//     0x20c6a0–0x20c74b  Prologue, setLineNr, make_shared<EvalResults>
//     0x20c752–0x20c78e  Get funName string from function() child
//     0x20c78e–0x20c7e6  functionExists(funName, outSig) → branch
//     0x20c7ec–0x20c84f  [Path A] newLabel("ret"), push to state.labelStack
//     0x20c84f–0x20c929  [Path A] Evaluate arguments child (if non-null)
//     0x20c929–0x20d18b  [Path A] Build overload signature from arg types
//     0x20ca3f–0x20cd33  [Path B] createSubScope, evaluate args, CustomFunctions::call
//     0x20cd33–0x20dc1e  Build result->name_ = funName + "(" + signature + ")"
//     0x20d18b–0x20d1b2  [Path A] getFunction(funName, signature) → func
//     0x20d2a2–0x20d82e  [Path A] Function not found: error 0x4C + list overloads
//     0x20d2d9           [Path A] Arg count mismatch: error 0x4D
//     0x20dd4e–0x20ef93  [Path A] Parameter binding loop (jump table @0x95b924)
//     0x20ef98–0x20efaf  [Path A] If returnType==Var: getRegisterNumber→setReturnReg
//     0x20efb4–0x20f093  [Path A] Save inLoop, execute body->evaluate(funcScope)
//     0x20f09f–0x20f5a4  [Path A] Merge body result, emit return label, set return value
//     0x20f96e–0x210214  Exception handlers (CustomFunctionsValueException, CompilerException)

} // namespace zhinst
