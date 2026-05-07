// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// seqc_ast_eval_arithmetic.cpp — split from seqc_ast_nodes_evaluate.cpp
// evaluate() implementations: SeqCAstNode base, SeqCOperator, SeqCValue, SeqCVariable, SeqCAssign, SeqCPlus/Minus/Mult/Div
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



// ============================================================================
// SeqCAstNode::evaluate (3-arg) base — @0x209dc0
//
// Returns null shared_ptr (zeroes sret area).
// ============================================================================

std::shared_ptr<EvalResults> SeqCAstNode::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& /*ctx*/,
    FrontendLoweringState& /*state*/) const
{
    return nullptr;  // @0x209dc0
}

// ============================================================================
// SeqCOperator::evaluate (5-arg) base — @0x210a90
//
// Default implementation returns nullptr; concrete operator subclasses
// (SeqCPlus, SeqCMinus, etc.) override this for operator-specific logic.
// ============================================================================

std::shared_ptr<EvalResults> SeqCOperator::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& /*ctx*/,
    FrontendLoweringState& /*state*/,
    EvalResults const& /*lhsResult*/,
    EvalResults const& /*rhsResult*/) const
{
    return nullptr;  // @0x210a90
}

// ============================================================================
// SeqCOperator::evaluate (3-arg) template-method — @0x210aa0
//
// Template-method pattern:
//   1. Set lineNr on messages, state, wavetable
//   2. Allocate empty EvalResults
//   3. If varType_ != Unset and this is SeqCAssign, propagate varType_ to lhs
//   4. Evaluate lhs child (or make empty EvalResults)
//   5. Evaluate rhs child (or make empty EvalResults)
//   6. If both results non-null:
//      a. Extract VarType from last EvalResultValue of each
//      b. combine(lhsType, rhsType) → set on result
//      c. Call this->evaluate(5-arg) — virtual dispatch to concrete operator
//      d. Replace result with 5-arg return value
//   7. Catch CompilerException → report via errorMessage
//   8. Cleanup and return
//
// Stack frame: 0xb0 bytes. Exception personality: catch clause index 1.
// ============================================================================

std::shared_ptr<EvalResults> SeqCOperator::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{                                                           // @0x210aa0
    // 1. Set line number on subsystems.
    // Binary reads [this+0x0c] = lineNr_ field, used as line number.
    int lineNr = lineNr_;                                     // @0x210ac4
    ctx.messages->setLineNr(lineNr);                        // @0x210ace

    // Binary: [r15+0x8] = ctx.asmCommands.get() → write lineNr to +0x50
    // which is AsmCommands::wavetableFrontIndex_.
    // @0x210ad3-ad7: mov rax,[r15+0x8]; mov [rax+0x50],r12d
    ctx.asmCommands->setWavetableFrontIndex(lineNr);        // @0x210ad7

    ctx.wavetable->setLineNr(lineNr);                       // @0x210ae2

    // 2. Create empty result.
    auto result = std::make_shared<EvalResults>();           // @0x210aec-210b37

    // 3. If varType_ is set and this is a SeqCAssign, propagate to lhs.
    // Binary: dynamic_cast<SeqCAssign*>(this) at @0x210b55, then
    //         dynamic_cast<SeqCOperation*>(lhs_.get()) at @0x210b84,
    //         write varType_ to lhs at [rax+0x14].
    if (varType_ != VarType_Unset) {                        // @0x210b3b
        if (dynamic_cast<const SeqCAssign*>(this)) {        // @0x210b55
            if (auto* lhsNode = lhs()) {                    // @0x210b62
                // Propagate VarType to lhs — the binary casts to
                // SeqCOperation but writes to the base-class varType_
                // field at +0x14, which is the same for all subclasses.
                const_cast<SeqCAstNode*>(lhsNode)
                    ->setVarType(varType_);                  // @0x210b89
            }
        }
    }

    // 4. Evaluate lhs child.
    std::shared_ptr<EvalResults> lhsResult;                 // @[rbp-0x68]
    if (lhs()) {                                            // @0x210b90
        try {
            lhsResult = lhs()->evaluate(res, ctx, state);   // @0x210bd4
        } catch (ResourcesException const& e) {             // @0x210f72
            // Binary catches ResourcesException specifically (accesses msg_
            // at +0x08 directly, not via what() vtable call).
            // NOTE: In practice this catch is rarely hit because
            // SeqCVariable::evaluate has its own internal catch for
            // ResourcesException. This outer catch serves as a safety net.
            ctx.messages->errorMessage(
                std::string(e.what() ? e.what() : ""),
                lineNr_);                                      // @0x210fc0
        }
    }
    if (!lhsResult) {
        lhsResult = std::make_shared<EvalResults>();        // @0x210c0a
    }

    // 5. Evaluate rhs child.
    std::shared_ptr<EvalResults> rhsResult;                 // @[rbp-0x58]
    if (rhs()) {                                            // @0x210c65
        try {
            rhsResult = rhs()->evaluate(res, ctx, state);   // @0x210ca9
        } catch (ResourcesException const& e) {
            // Same pattern as LHS catch.
            ctx.messages->errorMessage(
                std::string(e.what() ? e.what() : ""),
                lineNr_);
        }
    }
    if (!rhsResult) {
        rhsResult = std::make_shared<EvalResults>();        // @0x210cdd
    }

    // 6. If both results are valid (non-null inner pointer), combine and
    //    dispatch to 5-arg virtual.
    // Binary checks: [rbp-0x68] != 0 && [rbp-0x58] != 0
    // (the EvalResults* inside the shared_ptr, not the control block).
    if (lhsResult && rhsResult) {                           // @0x210d62-d69
        // 6a. Extract VarType from last EvalResultValue of each result.
        // Binary: magic multiply by 0x6db6db6db6db6db7 (÷56 = sizeof EvalResultValue)
        //         to compute element count from byte diff; reads [end-0x38]
        //         which is the VarType at offset 0 of the last EvalResultValue.
        VarType lhsType = VarType_Unset;                    // edi=0
        VarType rhsType = VarType_Unset;                    // esi=0

        auto const& lv = lhsResult->values_;
        if (!lv.empty() && lv.size() <= 1) {                // @0x210d87-d9e
            lhsType = lv.back().varType_;                   // @0x210da0
        }

        auto const& rv = rhsResult->values_;
        if (!rv.empty() && rv.size() <= 1) {                // @0x210dad-dc1
            rhsType = rv.back().varType_;                   // @0x210dc3
        }

        // 6b. Combine VarTypes and set on result.
        VarType combined = combine(lhsType, rhsType);       // @0x210dc9
        result->setValue(combined);                          // @0x210dd3

        // 6c. Call 5-arg virtual evaluate via vtable[8] (+0x40).
        // Binary: mov r10,[rcx+0x40]; call r10 @0x210e14
        auto opResult = this->evaluate(                     // @0x210e14
            res, ctx, state, *lhsResult, *rhsResult);

        // 6d. Replace result with the 5-arg return value.
        result = std::move(opResult);                       // @0x210e17-e26
    }

    return result;                                          // @0x210f01-f15
}

// ============================================================================
// SeqCValue::evaluate (3-arg) — @0x213140
//
// Leaf node evaluate: wraps the literal value (double or string) held in
// this SeqCValue into an EvalResults.
//
// Tag dispatch (from tag_ at +0x30):
//   eDouble (1):  setValue(double), format via ostringstream precision=5,
//                 store formatted string in result->name_.
//   eString (0):  copy embedded string from payload_, check for matching
//                 quotes ('…' or "…"), strip matching quotes, call
//                 setValue(VarType_String, str), copy to result->name_.
//   other:        __throw_bad_variant_access()
//
// Stack frame: 0x128 bytes. Sret return (shared_ptr<EvalResults>).
// ============================================================================

std::shared_ptr<EvalResults> SeqCValue::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/) const
{                                                           // @0x213140
    // 1. Set line number on subsystems.
    int lineNr = lineNr_;                                     // @0x21315d
    ctx.messages->setLineNr(lineNr);                        // @0x213167
    ctx.asmCommands->setWavetableFrontIndex(lineNr);        // @0x21316c-213170
    ctx.wavetable->setLineNr(lineNr);                       // @0x21317b

    // 2. Allocate result.
    auto result = std::make_shared<EvalResults>();           // @0x213180-2131d0

    // 3. Dispatch on tag.
    Tag t = tag();                                          // @0x2131d8
    switch (t) {
    case Tag::eDouble: {                                    // @0x213211
        // 3a. Store the double value into result.
        double val = asDouble();
        result->setValue(val);                               // @0x21321a

        // 3b. Format the double as a string with precision 5, store in name_.
        // Binary constructs ostringstream @0x213226, sets precision to 5
        // via direct write to ios_base::__precision_ @0x213236, then
        // streams the double @0x21325a, extracts str(), and moves into
        // result->name_ @0x213380-2133af.
        std::ostringstream oss;                              // @0x213226
        oss.precision(5);                                    // @0x213236
        oss << val;                                          // @0x21325a
        result->name_ = oss.str();                          // @0x213385-2133af
        break;
    }

    case Tag::eString: {                                    // @0x2131e9
        // 3c. Copy the embedded string from payload_.
        // Binary copies the libc++ SSO string at this+0x18 into a local
        // string on stack (@0x2131e9-213296 for short path, @0x213282
        // for long/heap path via __init_copy_ctor_external).
        std::string str = asString();                       // @0x2131e9-213296

        // 3d. Check for matching quote characters and strip them.
        // Binary checks first char for ' (0x27) @0x2132b7 or " (0x22)
        // @0x2132c0. If first char is a quote, checks if last char matches.
        // If both match: strip first and last character (substr(1, len-2)).
        // If length after quote-match is 0: __throw_out_of_range @0x213628.
        if (!str.empty()) {                                 // @0x213296
            char first = str.front();                       // @0x2132b4
            if (first == '\'' || first == '"') {            // @0x2132b7, 0x2132c0
                if (str.back() == first) {                  // @0x2132df, 0x213461
                    // Matching quotes found — strip them.
                    // Binary throws out_of_range if size==0 @0x213472-213475
                    // (defensive; can't happen since front() succeeded).
                    str = str.substr(1, str.size() - 2);    // @0x21347b-213521
                }
            }
        }

        // 3e. Store string value in result.
        result->setValue(VarType_String, str);               // @0x213579
        result->name_ = str;                                // @0x21357e-2135e4
        break;
    }

    default:                                                // @0x21361e, 0x213623
        // Binary calls std::__1::__throw_bad_variant_access();
        // only reachable if tag_ is neither 0 nor 1.
        throw std::bad_variant_access();
    }

    return result;                                          // @0x213609
}

// ============================================================================
// SeqCVariable::evaluate (3-arg) — @0x209ea0
//
// Largest evaluate() in the binary (3712 bytes). Looks up `name_` in the
// Resources scope and produces an EvalResults describing the resolved
// variable. Behavior depends on `varType_` (this SeqCAstNode's varType_
// field at +0x14):
//
//   varType_ = Unset   → look up name in scope via getVariableType, then
//                        dispatch on the *returned* VarType:
//                          Var    → checkVar (if direction==Read &&
//                                   !res->testBoundary), then build
//                                   EvalResultValue from getRegister.
//                          String → readString → emplace EvalResultValue.
//                          Const  → readConst  → emplace EvalResultValue.
//                          Wave   → readWave   → emplace EvalResultValue.
//                          Cvar   → readCvar   → emplace EvalResultValue.
//                          other  → fall through (silently empty result).
//   varType_ = Void    → emit error message ErrorMessageT(0xE0).
//   varType_ = Var     → addVar(name, Stub) + getRegister + setValue(Var,reg)
//   varType_ = String  → addString(name, Stub)            + setValue(String)
//   varType_ = Const   → addConst(name, Stub)             + setValue(Const)
//   varType_ = Wave    → wavetable.newEmptyWaveform(name) +
//                        addWave(name, wfName) + setValue(Wave, wfName)
//   varType_ = Cvar    → addCvar(name, Stub)              + setValue(Cvar)
//
// After the dispatch, copy `name` into `result->name_` (offset +0x58 in
// EvalResults).
//
// The four `read*` paths share an identical inline EvalResultValue
// construction sequence (lines 0x20a1c7..0x20a3f2 in the binary):
//   1. Call res.read*(name, abs(direction)) with sret to local 56-byte
//      EvalResultValue at [rbp-0x60..-0x28].
//   2. Allocate a fresh 56-byte EvalResultValue on the heap (operator new).
//   3. Copy the returned EvalResultValue's outer fields:
//        new[+0x00] = ret.varType_   (varSubType_-bearing 8B from [rbp-0x60])
//        new[+0x08] = ret.varSubType_
//   4. Use a per-path inner jump table (one of @0x95b8c4, @0x95b8d4,
//      @0x95b8e4, @0x95b8f4) keyed on `abs(value.which_)` to copy the
//      Value payload at [new+0x18] (Int=4B, Bool=1B, Double=8B, String=16B+ptr).
//   5. Store ValueType into [new+0x10] and AsmRegister into [new+0x30].
//   6. Replace the EvalResults values_ vector contents with this single
//      element (manual destroy-old + assign new pointers to begin/end/cap).
//
// We faithfully expand each read path to a single push_back into
// `result->values_` since the binary's hand-rolled vector replacement
// is observationally equivalent to `values_.assign(1, std::move(rv))`.
// ============================================================================

std::shared_ptr<EvalResults> SeqCVariable::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/) const
{                                                           // @0x209ea0
    // 1. Set line number on subsystems.                    @0x209ec4-209ee7
    int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);
    ctx.asmCommands->setWavetableFrontIndex(lineNr);
    ctx.wavetable->setLineNr(lineNr);

    // 2. Local copy of `name_` for use as argument to all helpers.
    //    Binary copies SSO inline @0x209eee or via __init_copy_ctor_external
    //    @0x209f18. C++ string copy ctor handles both paths.
    std::string name = name_;                               // @0x209ee7-209f1d

    // 3. Allocate result.                                   @0x209f1d-209f87
    auto result = std::make_shared<EvalResults>();

    // 4. abs(direction()) — used as EDirection for all read* calls.
    //    Binary: ecx = direction(); edx = ecx; sar edx,31; xor edx,ecx
    //    @0x20a232 etc. (standard branchless abs of int32).
    auto direction = static_cast<EDirection>(
        std::abs(static_cast<int32_t>(direction_)));

    // 5. Helper to push a result value (the inline construction pattern
    //    used by every read* path in the binary). The binary does this
    //    via a fresh heap allocation + manual vector replacement; the
    //    observable effect is identical to `assign(1, std::move(rv))`.
    auto pushReadResult = [&](EvalResultValue rv) {
        result->values_.clear();
        result->values_.push_back(std::move(rv));
    };

    // 6. Dispatch on this->varType_ — first jump table @0x95b894 (7 entries).
    switch (varType_) {                                     // @0x209f87-209fa3

    case VarType_Unset: {                                   // @0x209fa5
        // 6a. Look up name in scope.
        VarType resolved = res->getVariableType(name);      // @0x209faf
        // Second jump table @0x95b8b0, indexed by (resolved - 2) clamped to 0..4.
        switch (resolved) {                                 // @0x209fb4-209fce

        case VarType_Var: {                                 // @0x209fd0
            // If direction!=0 (write) AND !res->testBoundary, call checkVar.
            // Binary: cmp [r14+0x10],0; je skip; cmp [rdi+0x88],0; jne skip
            //         @0x209fd0-209fe1.
            if (static_cast<int32_t>(direction_) != 0 &&
                !res->atScopeBoundary()) {
                res->checkVar(name);                        // @0x209fea
            }
            int reg = res->getRegister(name);               // @0x209ff9
            result->setValue(VarType_Var, reg);             // @0x20a008
            break;
        }

        case VarType_String: {                              // @0x20a36b
            EvalResultValue rv = res->readString(name, direction);
            pushReadResult(std::move(rv));
            break;
        }

        case VarType_Const: {                               // @0x20a253
            EvalResultValue rv = res->readConst(name, direction);
            pushReadResult(std::move(rv));
            break;
        }

        case VarType_Wave: {                                // @0x20a2df
            EvalResultValue rv = res->readWave(name, direction);
            pushReadResult(std::move(rv));
            break;
        }

        case VarType_Cvar: {                                // @0x20a1c7
            EvalResultValue rv = res->readCvar(name, direction);
            pushReadResult(std::move(rv));
            break;
        }

        default:
            // Unset/Void/anything else → fall through to common tail.
            // (Empty result with no values_, just the name set below.)
            break;
        }
        break;
    }

    case VarType_Void: {                                    // @0x20a058
        // 6b. Error path: format ErrorMessageT(0xE0) with `name` and emit
        //     via CompilerMessageCollection::errorMessage.
        //     Binary: ErrorMessages::format<string>(0xE0, name) @0x20a17b,
        //             errorMessage(msg, lineNr_) @0x20a18b.
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0xE0), name);                     // @0x20a17b
        ctx.messages->errorMessage(msg, lineNr_);             // @0x20a18b
        break;
    }

    case VarType_Var: {                                     // @0x20a089
        res->addVar(name, VarSubType_Default);              // @0x20a095 (binary: xor edx,edx = 0)
        int reg = res->getRegister(name);                   // @0x20a0a4
        result->setValue(VarType_Var, reg);                 // @0x20a0b3
        break;
    }

    case VarType_String: {                                  // @0x20a012
        res->addString(name, VarSubType_Stub);              // @0x20a01e
        result->setValue(VarType_String);                   // @0x20a02b
        break;
    }

    case VarType_Const: {                                   // @0x20a0bd
        res->addConst(name, VarSubType_Stub);               // @0x20a0c9
        result->setValue(VarType_Const);                    // @0x20a0d6
        break;
    }

    case VarType_Wave: {                                    // @0x20a0e0
        // Allocate an empty waveform and bind name → wfName.
        std::shared_ptr<WaveformFront> wf =
            ctx.wavetable->newEmptyWaveform(name);          // @0x20a0ef
        // The binary reads WaveformFront+0x00, which is Waveform::name
        // (std::string at offset +0x00, confirmed in waveform.hpp:99).
        // The waveform's name string is used as both the addWave argument
        // and the setValue payload.
        const std::string& wfName = wf->name;
        res->addWave(name, wfName);                         // @0x20a102
        result->setValue(VarType_Wave, wfName);             // @0x20a113
        break;
    }

    case VarType_Cvar: {                                    // @0x20a035
        res->addCvar(name, VarSubType_Stub);                // @0x20a041
        result->setValue(VarType_Cvar);                     // @0x20a04e
        break;
    }

    default:
        // varType_ > 6 — falls through error path in binary
        // (jump table off-end → 0x20a058). Already handled above.
        break;
    }

    // 7. Common tail: copy `name` into result->name_ (offset +0x58).
    //    Binary @0x20a793-20a80b: distinguishes SSO vs heap path for the
    //    destination string. C++ assignment handles both.
    result->name_ = name;                                   // @0x20a793-20a80b

    return result;                                          // @0x20a82b-20a843
}

// ============================================================================
// SeqCAssign::evaluate (5-arg) — @0x243e60 (5552 bytes)
//
// Implements `lhs = rhs` lowering. Dispatch is a flat cascade of paired
// (lhsType, rhsType) tests against `lhsResult.values_.back().varType_` and
// `rhsResult.values_.back().varType_`. The first matching pair wins; rows
// that fall off the cascade fault to a generic 0x8b error.
//
// Register/arg mapping (Itanium ABI sret + this shifts everything by 1):
//   rdi=hidden retptr, rsi=this, rdx=res(.ptr_), rcx=ctx, r8=state,
//   r9=lhsResult, [rbp+0x10]=rhsResult.
//
// See reconstructed/notes/frontend_lowering.md "SeqCAssign::evaluate"
// for the full 13-row action table and per-row binary-address breadcrumbs.
// ============================================================================

std::shared_ptr<EvalResults> SeqCAssign::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{
    (void)state;  // state arg is consumed only via ctx.messages in the
                  // catch handler; the binary spills [rbp-0x110]=ctx and
                  // does not otherwise touch state. (Verified: every
                  // [rbp-0x110] reload in the disasm is treated as ctx*.)

    // 1. Allocate the result.                              // @0x243e87-243ed4
    auto result = std::make_shared<EvalResults>();

    // 2. Allocate `effectiveLhs = make_shared<EvalResults>(lhsResult)` — a copy of
    //    the LHS, used internally so the dispatch can read lhs varType
    //    info safely and so arrayBacking_/waveformFront_ propagation can
    //    happen without mutating the caller's lhsResult.   // @0x243edf-243f00
    auto effectiveLhs = std::make_shared<EvalResults>(lhsResult);

    // 3. If `lhsResult.arrayBacking_` (field at +0x70) is non-null,
    //    REPLACE effectiveLhs with lhsResult.arrayBacking_.          // @0x243f13-243f5c
    //    This is how array-indexed assignments work: SeqCArray::evaluate
    //    stores the indexed result (VarType_Wave) in arrayBacking_ of
    //    the outer result (VarType_Cvar). Here we swap effectiveLhs to point at
    //    the indexed result so the dispatch sees Wave, not Cvar.
    if (lhsResult.arrayBacking_) {
        effectiveLhs = lhsResult.arrayBacking_;
    }

    // 4. dynamic_cast<SeqCVariable*>(this->lhs()).          // @0x243f64-243f89
    //    Used as the source of `name` for every Resources::updateX() call
    //    below: the SeqCVariable's name_ field lives at +0x18 (hence the
    //    `add r12, 0x18` seen before each update call in the binary).
    SeqCVariable* lhsVar = dynamic_cast<SeqCVariable*>(
        const_cast<SeqCAstNode*>(lhs()));

    // 5. Read effectiveLhs.values_.back().varType_ (= lhsType).      // @0x243f91-243fc4
    //    The binary checks `effectiveLhs->values_.size() <= 1` via the ÷56
    //    magic-multiply pattern; if size > 1 OR empty, it falls to the
    //    common cleanup tail (treating as "no match").
    if (effectiveLhs->values_.empty() || effectiveLhs->values_.size() > 1) {
        return result;                                      // @0x245aa7
    }
    const VarType    lhsType = effectiveLhs->values_.back().varType_;
    const VarSubType lhsSub  = effectiveLhs->values_.back().varSubType_;

    // Helper to read rhsType under the same size guard. The binary
    // re-loads [rbp+0x10] for every row, then walks rhsResult.values_
    // and reads [rcx-0x38] for varType.
    // rhsTypeOrUnset — promoted to file-scope
    // rhsSubOrDefault — promoted to file-scope

    // The binary dispatches inside a try-block catching VarTypeException.
    try {
        // ====================================================================
        // Row 12: lhsType == Unset → silent no-op.          // @0x24454b
        // (Tested before/within the cascade; in the binary it's a
        // catch-all "default → cleanup" leg.)
        // ====================================================================
        if (lhsType == VarType_Unset) {
            return result;
        }

        const VarType rhsType = rhsTypeOrUnset(rhsResult);
        const VarSubType rhsSub = rhsSubOrDefault(rhsResult);

        // The `or ecx,0x2; cmp ecx,6` idiom: rhsType ∈ {Const(4), Cvar(6)}.
        const bool rhsIsConstOrCvar =
            (rhsType == VarType_Const) || (rhsType == VarType_Cvar);

        // Name source for update*() calls: lhsVar->name_.
        // The binary does NOT bail out early when lhsVar is null — the
        // null-check happens lazily inside each row that needs a name
        // (the `test r12,r12` / `add r12,0x18` pattern). For rows that
        // don't need a name (e.g. Wave[Numeric] in-place write), lhsVar
        // being null is perfectly fine.
        const std::string name = lhsVar ? lhsVar->name() : std::string();

        switch (lhsType) {

        // ================================================================
        // Row 1 & 2: lhsType == Var
        // ================================================================
        case VarType_Var: {
            if (rhsIsConstOrCvar) {
                // Row 1 (rhs ∈ {Const, Cvar}): emit addi to initialize
                // the variable register from the constant value, then
                // emit a SetVarPlaceholder.                 // @0x24400a-244039
                res->updateVar(name);                       // @0x24400a
                AsmRegister lhsReg = effectiveLhs->values_.back().reg_;
                AsmRegister zeroReg(0);
                Value rhsVal = rhsResult.getValue();
                // addi lhsReg, R0, constValue               // @0x245364 (addi with Value)
                auto addiAsms = ctx.asmCommands->addi(lhsReg, zeroReg, rhsVal);
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    addiAsms.begin(), addiAsms.end());
                // asmSetVarPlaceholder(lhsReg)              // @0x24549d
                auto placeholder = ctx.asmCommands->asmSetVarPlaceholder(lhsReg);
                result->assemblers_.push_back(std::move(placeholder));
                result->node_ = result->assemblers_.back().node;  // @0x245535
            } else if (rhsType == VarType_Var) {
                // Row 2 (rhs == Var): merge rhsResult.assemblers_ into
                // result->assemblers_, then emit addi(lhsReg, rhsReg, 0)
                // to copy the rhs value to the lhs register.
                //                                          // @0x24403e-244085
                result->assemblers_.insert(                 // @0x24403e
                    result->assemblers_.end(),
                    rhsResult.assemblers_.begin(),
                    rhsResult.assemblers_.end());
                res->updateVar(name);                       // @0x244085
                // Emit addi(lhsReg, rhsReg, Immediate(0)) — register copy
                //                                          // @0x24481f
                AsmRegister lhsReg = effectiveLhs->values_.back().reg_;
                AsmRegister rhsReg = rhsResult.values_.back().reg_;
                auto addiAsms = ctx.asmCommands->addi(lhsReg, rhsReg, Immediate(0));
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    addiAsms.begin(), addiAsms.end());
                // asmSetVarPlaceholder(lhsReg)              // @0x24495a
                auto placeholder = ctx.asmCommands->asmSetVarPlaceholder(lhsReg);
                result->assemblers_.push_back(std::move(placeholder));
                result->node_ = result->assemblers_.back().node;  // @0x245535
            } else {
                // Other rhs types (e.g. Wave): fall to error 0x8b.
                goto default_error;
            }
            break;
        }

        // ================================================================
        // Row 3: lhsType == Const, rhs ∈ {Const, Cvar}.    // @0x2440f4-245047
        // ================================================================
        case VarType_Const: {
            if (!rhsIsConstOrCvar) goto default_error;
            res->updateConst(name,
                             rhsResult.getValue().toDouble(),
                             lhsSub,
                             /*isCvar=*/false);
            // NOTE: No ASM emission in this row.  Const assignment is
            // compile-time — the binary extracts the rhs value via a
            // per-which jump table (@0x95c1cc), then calls setValue
            // to propagate the value into the result.       // @0x245047-24507e
            result->setValue(VarType_Const, lhsSub,
                             rhsResult.getValue());  // @0x2455d6
            break;
        }

        // ================================================================
        // Row 4: lhsType == Cvar, rhs ∈ {Const, Cvar}.     // @0x244174-2451ee
        // ================================================================
        case VarType_Cvar: {
            if (!rhsIsConstOrCvar) goto default_error;
            res->updateCvar(name,
                            rhsResult.getValue().toDouble(),
                            lhsSub);
            // NOTE: No ASM emission.  Same compile-time pattern as Row 3.
            result->setValue(VarType_Cvar, lhsSub,
                             rhsResult.getValue());  // @0x2457de
            break;
        }

        // ================================================================
        // Row 5: lhsType == String, rhs == String.         // @0x2441f4-245651
        // ================================================================
        case VarType_String: {
            if (rhsType != VarType_String) goto default_error;
            res->updateString(name,
                              rhsResult.getValue().toString(),
                              lhsSub);
            // NOTE: No ASM emission.  String assignment is compile-time.
            result->setValue(VarType_String, lhsSub,
                             rhsResult.getValue());  // @0x245913
            break;
        }

        // ================================================================
        // Rows 6-11: lhsType == Wave (subtype-driven sub-dispatch).
        // ================================================================
        case VarType_Wave: {
            switch (lhsSub) {

            case VarSubType_Vect: {
                // Row 6: Wave[Numeric] = Wave.             // @0x24426f-244546
                if (rhsType == VarType_Wave) {
                    auto wf = ctx.wavetable->copyWaveform(
                        rhsResult.waveformFront_);
                    // The lhs node is SeqCArray; its array() child is
                    // a SeqCVariable whose name_ is the array name.
                    SeqCArray* arr = dynamic_cast<SeqCArray*>(
                        const_cast<SeqCAstNode*>(lhs()));
                    std::string arrName = name;  // fallback
                    if (arr) {
                        const SeqCVariable* arrVar =
                            dynamic_cast<const SeqCVariable*>(arr->array());
                        if (arrVar) arrName = arrVar->name();
                    }
                    res->updateWave(name, arrName, rhsSub);
                    // Mark dirty: WaveformFront::dirty_ = 1 (+0xdc)
                    if (wf) wf->dirty_ = true;
                    break;
                }
                // Row 8: Wave[Numeric] = String → error 0x8b (mismatch).
                //                                          // @0x24438e-244398
                if (rhsType == VarType_String) {
                    ctx.messages->errorMessage(
                        ErrorMessages::format(ErrorMessageT(0x8b),
                                              str(VarType_String),
                                              str(VarType_Const)),
                        lineNr_);
                    break;
                }
                // Row 9: Wave[Numeric] = Var or Cvar → in-place numeric
                // write into result->waveformFront_.       // @0x244468-244c0d
                // Binary uses (rhsType | 2) == 6, matching Var(2), Const(4), Cvar(6).
                if (rhsType == VarType_Var || rhsType == VarType_Const || rhsType == VarType_Cvar) {
                    auto wf = effectiveLhs->waveformFront_;
                    if (wf) {
                        // idx = lhs.value.toInt(); val = rhs.value.toDouble();
                        const int64_t idx =
                            effectiveLhs->getValue().toInt();
                        const double val =
                            rhsResult.getValue().toDouble();
                        // Binary writes raw to signal.samples_[idx] and
                        // signal.markers_[idx], then sets dirty_=true.
                        // Binary address: @0x244957-244983.
                        // signal.samples_ is at Waveform+0x80 (Signal+0x00),
                        // signal.markers_ is at Waveform+0x98 (Signal+0x18).
                        auto* sp = wf->signal.samples_.data();
                        auto* mp = wf->signal.markers_.data();
                        if (sp) sp[idx] = val;
                        if (mp) mp[idx] = 0;
                        wf->dirty_ = true;
                    }
                    break;
                }
                // Row 7: Wave[Numeric] with rhs not Wave/String/Var/Cvar
                // — falls through to general error.
                goto default_error;
            }

            case VarSubType_FunctionArg: {
                // Row 10: Wave[FunctionArg] = (any).       // @0x244a58-244a8a
                auto wf = ctx.wavetable->newEmptyWaveform(name);
                const std::string& wfName = wf ? wf->name : name;
                res->updateWave(name, wfName, VarSubType_Default);
                result->setValue(VarType_Wave, VarSubType_FunctionArg,
                                 Value(wfName));
                break;
            }

            default: {
                // Row 11: Wave[other subtype] = (any).     // @0x244e3a-244ea0
                // name = rhs.toString(); if !waveformExists(name) error 0xe9
                // else updateWave(name, name, rhs.varSubType) +
                // result->setValue(Wave, rhs.varSubType, rhs.value).
                //
                // Special case: rhsSub == FunctionArg means the rhs is a wave
                // function parameter (empty waveform name). The lhs gets an
                // empty/unresolved result with FunctionArg subtype so downstream
                // consumers (e.g. playWave) know this wave is deferred.
                // We do NOT call updateWave here — the lhs variable keeps its
                // initial empty string value so getMaxSampleLength() skips it.
                if (rhsSub == VarSubType_FunctionArg) {
                    // Propagate FunctionArg subtype to lhs so downstream
                    // consumers (getMaxSampleLength, checkWaveformInitialized)
                    // skip this wave (varSubType_==2 → break in getMaxSampleLength).
                    res->updateWave(name, std::string{}, VarSubType_FunctionArg);
                    result->setValue(VarType_Wave, VarSubType_FunctionArg,
                                     Value(std::string{}));
                } else if (rhsType != VarType_Unset) {
                    const std::string rhsName =
                        rhsResult.getValue().toString();
                    if (!ctx.wavetable->waveformExists(rhsName)) {
                        ctx.messages->errorMessage(
                            ErrorMessages::format(ErrorMessageT(0xe9),
                                                  rhsName),
                            lineNr_);
                    } else {
                        res->updateWave(name, rhsName, rhsSub);
                        result->setValue(VarType_Wave, rhsSub,
                                         rhsResult.getValue());
                    }
                }
                break;
            }
            }  // switch (lhsSub)

            // NOTE(phase21h.3): In the binary, Wave[non-Numeric]=Wave has a
            // separate error 0x8b path @0x2442b9-2449a3 that fires before
            // the general default handler.  Our reconstruction routes it
            // through the default arm (row 11) which emits error 0xe9 via
            // waveformExists() instead.  Semantic difference: error code
            // 0x8b vs 0xe9 for this edge case.  Not functionally blocking.
            break;
        }

        default:
        default_error:
            // Row 13: general fallback — error 0x8b.       // @0x244593-24458c
            // (Only emit when rhs is non-Unset and the cascade fully
            // missed.)
            if (rhsTypeOrUnset(rhsResult) != VarType_Unset) {
                ctx.messages->errorMessage(
                    ErrorMessages::format(ErrorMessageT(0x8b),
                                          str(rhsTypeOrUnset(rhsResult)),
                                          str(lhsType)),
                    lineNr_);
            }
            break;
        }  // switch (lhsType)

        // Common tail @0x24594a-245a7e: build display name for the result.
        // Matches the pattern in other operators (SeqCPlus: " + ", etc.).
        result->name_ = name + " = " + rhsResult.name_;    // @0x24594a-245a7e

    } catch (VarTypeException& e) {
        // Catch handler @0x246041-2461a8. Reads e.msg_ (SSO or heap),
        // emits errorMessage on ctx.messages, then re-extracts type
        // info from RHS (not lhs — verified: the binary loads
        // [rbp+0x10]=rhsResult to walk values_) and propagates it to
        // the result via setValue(rt, rs, rhs.getValue()).
        ctx.messages->errorMessage(std::string(e.what()), -1);

        VarType    rt = rhsResult.values_.empty() ||
                        rhsResult.values_.size() > 1
                            ? VarType_Unset
                            : rhsResult.values_.back().varType_;
        VarSubType rs = rhsResult.values_.empty() ||
                        rhsResult.values_.size() > 1
                            ? VarSubType_Default
                            : rhsResult.values_.back().varSubType_;
        result->setValue(rt, rs, rhsResult.getValue());
        // (suppress unused warnings if compiled in some configs)
        (void)lhsSub;
    }

    // Common cleanup tail @0x245aa7-245aeb: effectiveLhs is released by RAII;
    // result is returned by value.
    return result;
}

// ============================================================================
// SeqCPlus::evaluate (5-arg) — @0x22a600 (7329 bytes)
//
// Implements `lhs + rhs` lowering.  Dispatch is a flat cascade of paired
// (lhsType, rhsType) tests against `lhsResult.values_.back().varType_` and
// `rhsResult.values_.back().varType_`.  The first matching pair wins;
// unmatched rows fall to a generic 0x73 error.
//
// Register/arg mapping (Itanium ABI sret + this shifts everything by 1):
//   rdi=hidden retptr, rsi=this, rdx=res(.ptr_), rcx=ctx, r8=state,
//   r9=lhsResult, [rbp+0x10]=rhsResult.
//
// Key differences from SeqCAssign::evaluate:
//   - Error code is 0x73 (not 0x8b)
//   - errorMessage uses line = -1 (not this->lineNr_)
//   - Uses combineWaveforms("add",...) for Wave+Wave
//   - Uses Resources::getRegisterNumber() for Var result register allocation
//   - Uses combine(VarType) and combine(VarSubType) for Const+Const row
//   - ASM: addi for Var+Const/Cvar, addr for Var+Var
//   - String+String: toString + string concatenation
//   - Common tail: result->name_ = lhsResult.name_ + " + " + rhsResult.name_
//
// See reconstructed/notes/frontend_lowering.md "SeqCPlus::evaluate"
// for the 8-row action table and binary-address breadcrumbs.
// ============================================================================

std::shared_ptr<EvalResults> SeqCPlus::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{
    // Neither `this`, `res`, nor `state` are used in the binary:
    // - this (rsi) is never read after prologue — the 5-arg evaluate
    //   body has no member-variable accesses.
    // - res (rdx) is consumed only as the shared_ptr control-block bump
    //   during the ABI argument pass; no Resources method is called.
    // - state (r8) is similarly unused.

    // 1. Allocate empty result.                              // @0x22a61d-22a677
    auto result = std::make_shared<EvalResults>();

    // 2. Helper: (type | 0x2) == 6 ↔ type ∈ {Const(4), Cvar(6)}.
    // isConstOrCvar — promoted to file-scope

    // 3. Helper: extract AsmRegister from values_.back(), or default(0).
    //    Binary uses `end - 8` trick to read the last 8 bytes of
    //    EvalResultValue (AsmRegister at +0x30 of the 0x38-byte struct).
    // getBackReg — promoted to file-scope

    // 4. Read lhsResult.values_ size.                        // @0x22a67c-22a694
    //    If empty, go directly to the default error with lhsType = Unset.
    //    If size > 1, also go to default error with lhsType = Unset.
    //    (Binary: magic multiply ÷56 = sizeof EvalResultValue.)
    const size_t lhsCount = lhsResult.values_.size();
    if (lhsCount == 0 || lhsCount > 1) {
        // Default error: lhsType = Unset.                    // @0x22a76b→22accb
        VarType lhsT = VarType_Unset;
        VarType rhsT = VarType_Unset;
        if (!rhsResult.values_.empty() && rhsResult.values_.size() <= 1)
            rhsT = rhsResult.values_.back().varType_;
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT(0x73),
                                  str(lhsT), str(rhsT)),
            -1);                                              // @0x22ad3a
        goto name_tail;
    }

    {
        // lhsResult.values_.size() == 1 guaranteed past this point.
        const VarType    lhsType = lhsResult.values_.back().varType_;
        const VarSubType lhsSub  = lhsResult.values_.back().varSubType_;

        // Helpers for rhs access.
        // rhsCount — promoted to file-scope
        // rhsTypeOrUnset — promoted to file-scope
        // rhsSubOrDefault — promoted to file-scope

        // ================================================================
        // Row 1: lhsType == Var(2), rhsType ∈ {Const(4), Cvar(6)}
        //                                                    @0x22a6bc
        // Merge lhsResult.assemblers_ → result; getRegisterNumber();
        // setValue(Var, regNum); ASM addi(resultReg, lhsReg, rhsValue).
        // ================================================================
        if (lhsType == VarType_Var &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            isConstOrCvar(rhsTypeOrUnset(rhsResult)))
        {
            result->assemblers_.insert(                       // @0x22a6fe-22a72b
                result->assemblers_.end(),
                lhsResult.assemblers_.begin(),
                lhsResult.assemblers_.end());

            int regNum = Resources::getRegisterNumber();      // @0x22a738
            result->setValue(VarType_Var, regNum);             // @0x22a747

            // ASM tail: extract registers + rhsValue, then addi.
            AsmRegister resultReg = getBackReg(*result);      // @0x22af3c-22af4a
            AsmRegister lhsReg   = getBackReg(lhsResult);    // @0x22af4e-22af75
            // Extract rhs Value via jump table @0x95bc9c.    // @0x22af79-22b7a1
            Value rhsVal = rhsResult.getValue();
            auto asmOut = ctx.asmCommands->addi(              // @0x22b7be
                resultReg, lhsReg, rhsVal);
            result->assemblers_.insert(                       // @0x22b7ea
                result->assemblers_.end(),
                asmOut.begin(), asmOut.end());
            goto name_tail;
        }

        // ================================================================
        // Row 2: lhsType ∈ {Const(4), Cvar(6)}, rhsType == Var(2)
        //                                                    @0x22a778
        // Mirror of Row 1 with lhs/rhs swapped.
        // ================================================================
        if (isConstOrCvar(lhsType) &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_Var)
        {
            result->assemblers_.insert(                       // @0x22a7b6-22a7e3
                result->assemblers_.end(),
                rhsResult.assemblers_.begin(),
                rhsResult.assemblers_.end());

            int regNum = Resources::getRegisterNumber();      // @0x22a7f0
            result->setValue(VarType_Var, regNum);             // @0x22a7ff

            // ASM tail: addi(resultReg, rhsReg, lhsValue).
            AsmRegister resultReg = getBackReg(*result);      // @0x22b025-22b033
            AsmRegister rhsReg   = getBackReg(rhsResult);    // @0x22b037-22b05e
            Value lhsVal = lhsResult.getValue();       // @0x22b062-22b9ba
            auto asmOut = ctx.asmCommands->addi(              // @0x22b9d7
                resultReg, rhsReg, lhsVal);
            result->assemblers_.insert(                       // @0x22ba03
                result->assemblers_.end(),
                asmOut.begin(), asmOut.end());
            goto name_tail;
        }

        // ================================================================
        // Row 3: lhsType == Var(2), rhsType == Var(2)
        //                                                    @0x22a82d
        // Merge both sides' assemblers; allocate new register; emit
        // addi(resultReg, lhsReg, Immediate(0)) as MOV, then
        // addr(resultReg, rhsReg) to add.
        // ================================================================
        if (lhsType == VarType_Var &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_Var)
        {
            // Merge lhsResult.assemblers_.                   // @0x22a86e-22a89b
            result->assemblers_.insert(
                result->assemblers_.end(),
                lhsResult.assemblers_.begin(),
                lhsResult.assemblers_.end());

            // Merge rhsResult.assemblers_.                   // @0x22a8a0-22a8ca
            result->assemblers_.insert(
                result->assemblers_.end(),
                rhsResult.assemblers_.begin(),
                rhsResult.assemblers_.end());

            int regNum = Resources::getRegisterNumber();      // @0x22a8d3
            result->setValue(VarType_Var, regNum);             // @0x22a8e2

            // addi(resultReg, lhsReg, Immediate(0)) — copy lhs → result.
            AsmRegister resultReg = getBackReg(*result);      // @0x22b1bb-22b1cc
            AsmRegister lhsReg   = getBackReg(lhsResult);    // @0x22b1d3-22b1f4
            auto copyAsm = ctx.asmCommands->addi(             // @0x22b21c
                resultReg, lhsReg, Immediate(0));
            result->assemblers_.insert(                       // @0x22b248
                result->assemblers_.end(),
                copyAsm.begin(), copyAsm.end());

            // addr(resultReg, rhsReg) — add rhs register to result.
            // Re-read resultReg after potential realloc in the insert.
            resultReg = getBackReg(*result);                  // @0x22b30e-22b33d
            AsmRegister rhsReg = getBackReg(rhsResult);      // @0x22b340-22b361
            AsmList::Asm addAsm = ctx.asmCommands->addr(      // @0x22b371
                resultReg, rhsReg);
            result->assemblers_.push_back(addAsm);            // @0x22b37a-22b3fb

            goto name_tail;
        }

        // ================================================================
        // Row 4: lhsType ∈ {Const(4), Cvar(6)},
        //        rhsType ∈ {Const(4), Cvar(6)}
        //                                                    @0x22a910
        // combine(VarType), combine(VarSubType), then double addition.
        // ================================================================
        if (isConstOrCvar(lhsType) &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            isConstOrCvar(rhsTypeOrUnset(rhsResult)))
        {
            VarType    combinedType = combine(lhsType,        // @0x22a975
                                              rhsTypeOrUnset(rhsResult));
            VarSubType combinedSub  = combine(lhsSub,         // @0x22a9d2
                                              rhsSubOrDefault(rhsResult));

            // Extract Values and add as doubles.             // @0x22b590-22b675
            double lhsVal = lhsResult.getValue().toDouble();
            double rhsVal = rhsResult.getValue().toDouble();
            double sum    = lhsVal + rhsVal;                  // addsd @0x22b675

            result->setValue(combinedType, combinedSub,       // @0x22b6a5
                             Value(sum));
            goto name_tail;
        }

        // ================================================================
        // Row 5: lhsType == String(3), rhsType == String(3)
        //                                                    @0x22aa25
        // String concatenation: toString(lhs) + toString(rhs).
        // ================================================================
        if (lhsType == VarType_String &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_String)
        {
            // Extract Values, convert to strings, concatenate.
            std::string lhsStr = lhsResult.getValue()         // @0x22b8d0-22b8db
                                     .toString();
            std::string rhsStr = rhsResult.getValue()          // @0x22baf3-22bb01
                                     .toString();
            lhsStr.append(rhsStr);                             // @0x22bb2a

            VarSubType combinedSub = combine(lhsSub,           // @0x22bc9b
                                             rhsSubOrDefault(rhsResult));
            result->setValue(VarType_String, combinedSub,      // @0x22bcb1
                             Value(lhsStr));
            goto name_tail;
        }

        // ================================================================
        // Row 6: lhsType == Wave(5), rhsType == Wave(5)
        //                                                    @0x22aa88
        // combineWaveforms("add", lhsResult, rhsResult, ctx).
        // ================================================================
        if (lhsType == VarType_Wave &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_Wave)
        {
            result = combineWaveforms(                         // @0x22aaf0
                std::string("add"), lhsResult, rhsResult, ctx);
            goto name_tail;
        }

        // ================================================================
        // Row 7: lhsType == Wave(5),
        //        rhsType ∈ {Const(4), Cvar(6)}
        //                                                    @0x22ab83
        // If lhsSub == FunctionArg: passthrough (copy lhsResult).
        // Else: constWaveform(length, rhsDouble, ctx) then
        //       combineWaveforms("add", lhsResult, constWf, ctx).
        // ================================================================
        if (lhsType == VarType_Wave &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            isConstOrCvar(rhsTypeOrUnset(rhsResult)))
        {
            if (lhsSub == VarSubType_FunctionArg) {            // @0x22abc9-22abce
                // FunctionArg passthrough.                    @0x22abd4-22ac08
                result = std::make_shared<EvalResults>(lhsResult);
                goto name_tail;
            }

            // Non-FunctionArg: create constant waveform from rhs value
            // and combine it with the lhs waveform.           // @0x22afdf-22b1b6
            std::string wfName =
                lhsResult.getValue().toString();        // @0x22bd0d-22bd27
            int length =
                ctx.wavetable->getWaveformSampleLength(wfName);// @0x22bd27
            double constVal =
                rhsResult.getValue().toDouble();        // @0x22be4b
            auto constWf = constWaveform(length, constVal, ctx);// @0x22be5d
            result = combineWaveforms(                         // @0x22be7a
                std::string("add"), lhsResult, *constWf, ctx);
            goto name_tail;
        }

        // ================================================================
        // Row 8: lhsType ∈ {Const(4), Cvar(6)},
        //        rhsType == Wave(5)
        //                                                    @0x22ac4c
        // Mirror of Row 7: passthrough if rhsSub == FunctionArg,
        // else constWaveform from lhs and combine.
        // ================================================================
        if (isConstOrCvar(lhsType) &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_Wave)
        {
            if (rhsSubOrDefault(rhsResult) == VarSubType_FunctionArg) { // @0x22ac82-22ac86
                // FunctionArg passthrough.                    @0x22ac8c-22acb2
                result = std::make_shared<EvalResults>(rhsResult);
                goto name_tail;
            }

            // Non-FunctionArg: create constant waveform from lhs value
            // and combine it with the rhs waveform.           // @0x22b0c8-22b191
            std::string wfName =
                rhsResult.getValue().toString();        // @0x22b0e3-22b0fe
            int length =
                ctx.wavetable->getWaveformSampleLength(wfName);// @0x22b0fe
            double constVal =
                lhsResult.getValue().toDouble();        // @0x22b14f
            auto constWf = constWaveform(length, constVal, ctx);// @0x22b161
            result = combineWaveforms(                         // @0x22b17f
                std::string("add"), *constWf, rhsResult, ctx);
            goto name_tail;
        }

        // ================================================================
        // Default: error 0x73.                               @0x22acbc
        // ================================================================
        {
            VarType lhsT = lhsType;                           // @0x22acbf-22acc7
            VarType rhsT = rhsTypeOrUnset(rhsResult);                  // @0x22acd7-22acfd
            ctx.messages->errorMessage(                        // @0x22ad3a
                ErrorMessages::format(ErrorMessageT(0x73),
                                      str(lhsT), str(rhsT)),
                -1);
        }
    }

name_tail:
    // Common tail @0x22adb3-22af0d: construct
    //   result->name_ = lhsResult.name_ + " + " + rhsResult.name_
    //
    // Binary builds this by:
    //   1. Copy lhsResult.name_ into a local string.
    //   2. Append literal " + " (DWORD 0x202b20 = " + \0" at +len).
    //   3. Append rhsResult.name_.
    //   4. Move-assign into result->name_.
    result->name_ = lhsResult.name_ + " + " + rhsResult.name_;  // @0x22adb3-22af0d

    return result;                                            // @0x22af0d-22af21
}

// ============================================================================
// SeqCMinus::evaluate (5-arg) — @0x22cde0 (7312 bytes)
//
// Implements `lhs - rhs` lowering.  Dispatch is a flat cascade of paired
// (lhsType, rhsType) tests against `lhsResult.values_.back().varType_` and
// `rhsResult.values_.back().varType_`.  The first matching pair wins;
// unmatched rows fall to a generic 0x74 error.
//
// Register/arg mapping (Itanium ABI sret + this shifts everything by 1):
//   rdi=hidden retptr, rsi=this, rdx=res(.ptr_), rcx=ctx, r8=state,
//   r9=lhsResult, [rbp+0x10]=rhsResult.
//
// Key differences from SeqCPlus::evaluate:
//   - Error code is 0x74 (not 0x73)
//   - No String+String row (string subtraction is not supported)
//   - Var-Var uses subr instead of addr
//   - Var-Const/Cvar negates rhs value via `neg eax` then uses
//     addi(resultReg, lhsReg, Immediate(-rhsInt))
//   - Const/Cvar-Var uses two-step: addi(resultReg, R0, lhsValue) then
//     subr(resultReg, rhsReg)
//   - Wave-Wave uses scaleWaveform to negate rhs before combineWaveforms("add")
//   - Wave-Const/Cvar negates rhsDouble via xorpd with sign-bit then constWaveform
//   - Const/Cvar-Wave creates constWaveform from lhs (no negation), then
//     scaleWaveform to negate rhs waveform, then combineWaveforms("add")
//   - No FunctionArg passthrough in Wave+Const/Cvar or Const/Cvar+Wave paths
//   - Common tail: " - " separator instead of " + "
//
// See reconstructed/notes/frontend_lowering.md "SeqCMinus::evaluate"
// for the 7-row action table and binary-address breadcrumbs.
// ============================================================================

std::shared_ptr<EvalResults> SeqCMinus::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{
    // Neither `this`, `res`, nor `state` are used in the binary.

    // 1. Allocate empty result.                              // @0x22cdfd-22ce56
    auto result = std::make_shared<EvalResults>();

    // 2. Helper: (type | 0x2) == 6 ↔ type ∈ {Const(4), Cvar(6)}.
    // isConstOrCvar — promoted to file-scope

    // 3. Helper: extract AsmRegister from values_.back(), or default(0).
    // getBackReg — promoted to file-scope

    // 4. Read lhsResult.values_ size.                        // @0x22ce5a-22ce72
    //    If empty → default error with lhsType = Unset.
    //    If size > 1 → also default error.
    const size_t lhsCount = lhsResult.values_.size();
    if (lhsCount == 0 || lhsCount > 1) {
        // Default error: lhsType = Unset.                    // @0x22d05f→22d441
        VarType lhsT = VarType_Unset;
        VarType rhsT = VarType_Unset;
        if (!rhsResult.values_.empty() && rhsResult.values_.size() <= 1)
            rhsT = rhsResult.values_.back().varType_;
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT(0x74),
                                  str(lhsT), str(rhsT)),
            -1);                                              // @0x22d4ba
        goto name_tail;
    }

    {
        // lhsResult.values_.size() == 1 guaranteed past this point.
        const VarType    lhsType = lhsResult.values_.back().varType_;
        const VarSubType lhsSub  = lhsResult.values_.back().varSubType_;

        // Helpers for rhs access.
        // rhsCount — promoted to file-scope
        // rhsTypeOrUnset — promoted to file-scope

        // ================================================================
        // Row 3: lhsType == Var(2), rhsType == Var(2)
        //                                                    @0x22ce9e→22cf73
        // Merge both sides' assemblers; allocate new register; emit
        // addi(resultReg, lhsReg, Immediate(0)) as MOV, then
        // subr(resultReg, rhsReg) to subtract.
        //
        // NOTE: This row is checked FIRST in the binary (before Var+Const),
        // because the binary tests lhsType==Var && rhsType has size guard &&
        // rhsType==Var before falling to the Var+Const/Cvar check.
        // ================================================================
        if (lhsType == VarType_Var &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_Var)
        {
            // Merge lhsResult.assemblers_ FIRST.            // @0x22cfc2-22cff3
            // (Path B in binary: Var-Var Row 3 reads lhsResult from
            //  -0x70(%rbp) and inserts that vector first.)
            result->assemblers_.insert(
                result->assemblers_.end(),
                lhsResult.assemblers_.begin(),
                lhsResult.assemblers_.end());

            // Merge rhsResult.assemblers_ SECOND.            // @0x22cff8-22d01b
            result->assemblers_.insert(
                result->assemblers_.end(),
                rhsResult.assemblers_.begin(),
                rhsResult.assemblers_.end());

            int regNum = Resources::getRegisterNumber();      // @0x22d020
            result->setValue(VarType_Var, regNum);             // @0x22d02f

            // addi(resultReg, lhsReg, Immediate(0)) — copy lhs → result.
            AsmRegister resultReg = getBackReg(*result);      // @0x22d870-22d87e
            AsmRegister lhsReg   = getBackReg(lhsResult);    // @0x22d882-22d8a3

            auto copyAsm = ctx.asmCommands->addi(             // @0x22d8cf
                resultReg, lhsReg, Immediate(0));
            result->assemblers_.insert(                       // @0x22d902
                result->assemblers_.end(),
                copyAsm.begin(), copyAsm.end());

            // subr(resultReg, rhsReg) — subtract rhs register from result.
            resultReg = getBackReg(*result);                  // @0x22d9d3-22d9fb
            AsmRegister rhsReg = getBackReg(rhsResult);      // @0x22d9ff-22da20
            AsmList::Asm subAsm = ctx.asmCommands->subr(      // @0x22da31
                resultReg, rhsReg);
            result->assemblers_.push_back(subAsm);            // @0x22da40-22da6b

            goto name_tail;
        }

        // ================================================================
        // Row 1: lhsType == Var(2), rhsType ∈ {Const(4), Cvar(6)}
        //                                                    @0x22cecc→22cf09
        //                                                    (asm tail @0x22d705→22dde6)
        // Merge lhsResult.assemblers_ → result; getRegisterNumber();
        // setValue(Var, regNum); negate rhs value to int, then
        // addi(resultReg, lhsReg, Immediate(-rhsInt)).
        // ================================================================
        if (lhsType == VarType_Var &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            isConstOrCvar(rhsTypeOrUnset(rhsResult)))
        {
            result->assemblers_.insert(                       // @0x22d6bb-22d6ec
                result->assemblers_.end(),
                lhsResult.assemblers_.begin(),
                lhsResult.assemblers_.end());

            int regNum = Resources::getRegisterNumber();      // @0x22d6f1
            result->setValue(VarType_Var, regNum);             // @0x22d700

            // ASM tail: extract registers, negate rhs.toInt(), emit addi.
            AsmRegister resultReg = getBackReg(*result);      // @0x22d705-22d722
            AsmRegister lhsReg   = getBackReg(lhsResult);    // @0x22d739-22d75d

            // Extract rhs Value, convert to int, negate.     // @0x22d761-22dd96
            int rhsInt = rhsResult.getValue().toInt();
            rhsInt = -rhsInt;                                 // neg eax @0x22dd96

            auto asmOut = ctx.asmCommands->addi(              // @0x22ddb9
                resultReg, lhsReg, Immediate(rhsInt));
            result->assemblers_.insert(                       // @0x22dde6
                result->assemblers_.end(),
                asmOut.begin(), asmOut.end());
            goto name_tail;
        }

        // ================================================================
        // Row 4: lhsType ∈ {Const(4), Cvar(6)},
        //        rhsType ∈ {Const(4), Cvar(6)}
        //                                                    @0x22d06a→22d12f
        // combine(VarType), combine(VarSubType), then double subtraction.
        // ================================================================
        if (isConstOrCvar(lhsType) &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            isConstOrCvar(rhsTypeOrUnset(rhsResult)))
        {
            VarType    combinedType = combine(lhsType,        // @0x22d0cd
                                              rhsTypeOrUnset(rhsResult));
            VarSubType combinedSub  = combine(lhsSub,         // @0x22d12a
                                              [&]() -> VarSubType {
                if (rhsResult.values_.empty() || rhsResult.values_.size() > 1)
                    return VarSubType_Default;
                return rhsResult.values_.back().varSubType_;
            }());

            // Extract Values and subtract as doubles.        // @0x22defe-22dfe0
            double lhsVal = lhsResult.getValue().toDouble();
            double rhsVal = rhsResult.getValue().toDouble();
            double diff   = lhsVal - rhsVal;                  // subsd @0x22dfe0

            result->setValue(combinedType, combinedSub,       // @0x22e010
                             Value(diff));
            goto name_tail;
        }

        // ================================================================
        // Row 2: lhsType ∈ {Const(4), Cvar(6)}, rhsType == Var(2)
        //                                                    @0x22cf78→22cff3
        //                                                    (asm tail @0x22e219→22e394)
        // Merge both sides' assemblers; allocate new register; emit
        // addi(resultReg, AsmRegister(0), lhsValue) to load const into reg,
        // then subr(resultReg, rhsReg) to subtract the variable.
        //
        // NOTE: Unlike SeqCPlus, commutativity doesn't hold for subtraction,
        // so we need the two-step approach: load const, then subtract var.
        // ================================================================
        if (isConstOrCvar(lhsType) &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_Var)
        {
            // Merge rhsResult.assemblers_.                   // @0x22cf09-22cf3a
            // (Path A in binary: Const/Cvar-Var Row 2 inserts only
            //  rhsResult.assemblers_; lhsResult is Const/Cvar so has
            //  no assemblers to merge.)
            result->assemblers_.insert(
                result->assemblers_.end(),
                rhsResult.assemblers_.begin(),
                rhsResult.assemblers_.end());

            int regNum = Resources::getRegisterNumber();      // @0x22d020 (shared)
            result->setValue(VarType_Var, regNum);             // @0x22d02f (shared)

            // addi(resultReg, AsmRegister(0), lhsValue) — load const into reg.
            AsmRegister resultReg = getBackReg(*result);      // @0x22e336-22e35e
            AsmRegister zeroReg(0);                           // AsmRegister(0) is R0
            Value lhsVal = lhsResult.getValue();       // @0x22d80e-22d841 (value extract)
            auto loadAsm = ctx.asmCommands->addi(             // @0x22e231
                resultReg, zeroReg, lhsVal);
            result->assemblers_.insert(                       // @0x22e264
                result->assemblers_.end(),
                loadAsm.begin(), loadAsm.end());

            // subr(resultReg, rhsReg) — subtract rhs register.
            resultReg = getBackReg(*result);                  // @0x22e336-22e35e
            AsmRegister rhsReg = getBackReg(rhsResult);      // @0x22e362-22e383
            AsmList::Asm subAsm = ctx.asmCommands->subr(      // @0x22e394
                resultReg, rhsReg);
            result->assemblers_.push_back(subAsm);            // @0x22e3a3-22e413

            goto name_tail;
        }

        // ================================================================
        // Row 5: lhsType == Wave(5), rhsType == Wave(5)
        //                                                    @0x22d181→22d314
        // scaleWaveform to negate rhs waveform, then
        // combineWaveforms("add", lhsResult, scaledRhs, ctx).
        //
        // NO FunctionArg passthrough (unlike SeqCPlus).
        // ================================================================
        if (lhsType == VarType_Wave &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_Wave)
        {
            // Copy rhsResult into a new shared_ptr<EvalResults>.  // @0x22d1d8-22d204
            auto rhsCopy = std::make_shared<EvalResults>(rhsResult);
            // Scale (negate) the rhs waveform.               // @0x22d227
            auto scaledRhs = scaleWaveform(0, rhsCopy, ctx);
            // Combine: "add" lhs + negated rhs.              // @0x22d245
            result = combineWaveforms(
                std::string("add"), lhsResult, *scaledRhs, ctx);
            goto name_tail;
        }

        // ================================================================
        // Row 6: lhsType == Wave(5),
        //        rhsType ∈ {Const(4), Cvar(6)}
        //                                                    @0x22d334→22d314 (cleanup shared)
        //                                                    (body @0x22dc3b→22e4c4)
        // NO FunctionArg passthrough (unlike SeqCPlus).
        // toString → getWaveformSampleLength, negate rhsDouble via xorpd
        // with sign-bit, constWaveform(len, -rhsDouble, ctx), then
        // combineWaveforms("add", lhsResult, constWf, ctx).
        // ================================================================
        if (lhsType == VarType_Wave &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            isConstOrCvar(rhsTypeOrUnset(rhsResult)))
        {
            // Get waveform sample length from lhs waveform name.
            std::string wfName =
                lhsResult.getValue().toString();        // @0x22dc4c
            int length =
                ctx.wavetable->getWaveformSampleLength(wfName);// @0x22dc58

            // Extract rhs value as double and negate it.     // @0x22e485-22e491
            double constVal =
                rhsResult.getValue().toDouble();
            constVal = -constVal;                              // xorpd with sign-bit @0x22e491

            auto constWf = constWaveform(length, constVal, ctx);// @0x22e4a6
            result = combineWaveforms(                         // @0x22e4c4
                std::string("add"), lhsResult, *constWf, ctx);
            goto name_tail;
        }

        // ================================================================
        // Row 7: lhsType ∈ {Const(4), Cvar(6)},
        //        rhsType == Wave(5)
        //                                                    @0x22d3b2→22e5e5
        // Create constWaveform from lhs value (no negation), then
        // scaleWaveform to negate rhs waveform, then
        // combineWaveforms("add", constWf, scaledRhs, ctx).
        //
        // NO FunctionArg passthrough (unlike SeqCPlus).
        // ================================================================
        if (isConstOrCvar(lhsType) &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_Wave)
        {
            // Get waveform sample length from rhs waveform name.
            std::string wfName =
                rhsResult.getValue().toString();        // @0x22e0cc
            int length =
                ctx.wavetable->getWaveformSampleLength(wfName);// @0x22e0df

            // Extract lhs value as double (no negation).     // @0x22e548
            double constVal =
                lhsResult.getValue().toDouble();
            auto constWf = constWaveform(length, constVal, ctx);// @0x22e562

            // Copy rhsResult and negate via scaleWaveform.   // @0x22e575-22e5c4
            auto rhsCopy = std::make_shared<EvalResults>(rhsResult);
            auto scaledRhs = scaleWaveform(0, rhsCopy, ctx);  // @0x22e5c4

            result = combineWaveforms(                         // @0x22e5e5
                std::string("add"), *constWf, *scaledRhs, ctx);
            goto name_tail;
        }

        // ================================================================
        // Default: error 0x74.                               @0x22d42f→22d4ba
        // ================================================================
        {
            VarType lhsT = lhsType;
            VarType rhsT = rhsTypeOrUnset(rhsResult);
            ctx.messages->errorMessage(                        // @0x22d4ba
                ErrorMessages::format(ErrorMessageT(0x74),
                                      str(lhsT), str(rhsT)),
                -1);
        }
    }

name_tail:
    // Common tail @0x22d533-22d696: construct
    //   result->name_ = lhsResult.name_ + " - " + rhsResult.name_
    //
    // Binary builds this by:
    //   1. Copy lhsResult.name_ into a local string.
    //   2. Append literal " - " (DWORD 0x202d20 = " - \0" at +len).  @0x22d602
    //   3. Append rhsResult.name_.
    //   4. Move-assign into result->name_.
    result->name_ = lhsResult.name_ + " - " + rhsResult.name_;  // @0x22d533-22d696

    return result;                                            // @0x22d696-22d6aa
}

// ============================================================================
// SeqCMult::evaluate (5-arg) — @0x22ea70 (9728 bytes)
//
// Implements `lhs * rhs` lowering.  Dispatch is a flat cascade of paired
// (lhsType, rhsType) tests against `lhsResult.values_.back().varType_` and
// `rhsResult.values_.back().varType_`.  The first matching pair wins;
// unmatched rows fall to a generic 0x8c error.
//
// Register/arg mapping (Itanium ABI sret + this shifts everything by 1):
//   rdi=hidden retptr, rsi=this, rdx=res(.ptr_), rcx=ctx, r8=state,
//   r9=lhsResult, [rbp+0x10]=rhsResult.
//
// Key differences from SeqCPlus/SeqCMinus::evaluate:
//   - Error code is 0x8c
//   - No String*String row (string multiplication is not supported)
//   - No Var*Var row (register multiplication not supported in ISA)
//   - Var*Const/Cvar and Const/Cvar*Var use computeMult() instead of
//     individual asm instructions — shift-and-add multiplication by bit
//     decomposition of the integer constant
//   - Wave*Wave uses combineWaveforms("multiply", ...) instead of "add"
//   - Wave*Const/Cvar and Const/Cvar*Wave use scaleWaveform 2-arg overload
//     @0x2309e0 (scalar, wave, ctx) — no FunctionArg passthrough at this level
//   - Common tail: " * " separator
//
// See reconstructed/notes/frontend_lowering.md "SeqCMult::evaluate"
// for the 6-row action table and binary-address breadcrumbs.
// ============================================================================

std::shared_ptr<EvalResults> SeqCMult::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{
    // Neither `this` nor `state` are used in the binary.
    // `res` is passed through to computeMult() for Var paths.

    // 1. Allocate empty result.                              // @0x22ea8d-22eaea
    auto result = std::make_shared<EvalResults>();

    // 2. Helper: (type | 0x2) == 6 ↔ type ∈ {Const(4), Cvar(6)}.
    // isConstOrCvar — promoted to file-scope

    // 3. Read lhsResult.values_ size.                        // @0x22eaee-22eb06
    //    If empty → default error with lhsType = Unset.
    //    If size > 1 → also default error.
    const size_t lhsCount = lhsResult.values_.size();
    if (lhsCount == 0 || lhsCount > 1) {
        // Default error: lhsType = Unset.
        VarType lhsT = VarType_Unset;
        VarType rhsT = VarType_Unset;
        if (!rhsResult.values_.empty() && rhsResult.values_.size() <= 1)
            rhsT = rhsResult.values_.back().varType_;
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT(0x8c),
                                  str(lhsT), str(rhsT)),
            -1);                                              // @0x22f3e2
        goto name_tail;
    }

    {
        // lhsResult.values_.size() == 1 guaranteed past this point.
        const VarType lhsType = lhsResult.values_.back().varType_;

        // Helpers for rhs access.
        // rhsCount — promoted to file-scope
        // rhsTypeOrUnset — promoted to file-scope

        // ================================================================
        // Row 2: lhsType ∈ {Const(4), Cvar(6)}, rhsType == Var(2)
        //                                                    @0x22eb2d→22ebc3
        // computeMult(rhsCopy, lhsCopy, res, ctx) — Var operand first.
        // The binary SWAPS: the Var operand is always passed as the first
        // param to computeMult.
        // ================================================================
        if (isConstOrCvar(lhsType) &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_Var)
        {
            auto rhsCopy = std::make_shared<EvalResults>(rhsResult);  // @0x22eb7e
            auto lhsCopy = std::make_shared<EvalResults>(lhsResult);  // @0x22eba4
            result = computeMult(rhsCopy, lhsCopy, res, ctx); // @0x22ebc3
            goto name_tail;
        }

        // ================================================================
        // Row 1: lhsType == Var(2),
        //        rhsType ∈ {Const(4), Cvar(6)}
        //                                                    @0x22eb0b→22f621
        // computeMult(lhsCopy, rhsCopy, res, ctx) — Var operand first.
        // ================================================================
        if (lhsType == VarType_Var &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            isConstOrCvar(rhsTypeOrUnset(rhsResult)))
        {
            auto lhsCopy = std::make_shared<EvalResults>(lhsResult);  // @0x22f5d4
            auto rhsCopy = std::make_shared<EvalResults>(rhsResult);  // @0x22f5fa
            result = computeMult(lhsCopy, rhsCopy, res, ctx); // @0x22f621
            goto name_tail;
        }

        // ================================================================
        // Row 3: lhsType ∈ {Const(4), Cvar(6)},
        //        rhsType ∈ {Const(4), Cvar(6)}
        //                                                    @0x22ee95→22eff8
        // combine(VarType), combine(VarSubType), then double
        // multiplication via mulsd.
        // ================================================================
        if (isConstOrCvar(lhsType) &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            isConstOrCvar(rhsTypeOrUnset(rhsResult)))
        {
            VarType combinedType = combine(                    // @0x22eeef
                lhsType,
                rhsResult.values_.back().varType_);
            VarSubType combinedSub = combine(                  // @0x22ef12
                lhsResult.values_.back().varSubType_,
                rhsResult.values_.back().varSubType_);

            // Extract double values and multiply.             // @0x22ef31-22efa3
            double lhsVal = lhsResult.getValue().toDouble();
            double rhsVal = rhsResult.getValue().toDouble();
            double product = lhsVal * rhsVal;                  // mulsd @0x22efa3

            result->setValue(combinedType, combinedSub,        // @0x22efac
                             Value(product));
            goto name_tail;
        }

        // ================================================================
        // Row 4: lhsType == Wave(5), rhsType == Wave(5)
        //                                                    @0x22eff8→22f103
        // combineWaveforms("multiply", lhsResult, rhsResult, ctx).
        // ================================================================
        if (lhsType == VarType_Wave &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_Wave)
        {
            result = combineWaveforms(                         // @0x22f063
                std::string("multiply"), lhsResult, rhsResult, ctx);
            goto name_tail;
        }

        // ================================================================
        // Row 5: lhsType ∈ {Const(4), Cvar(6)},
        //        rhsType == Wave(5)
        //                                                    @0x22f103→22f29f
        // scaleWaveform(lhsCopy, rhsCopy, ctx) — scalar first, wave second.
        // Uses the 2-arg overload @0x2309e0.
        // ================================================================
        if (isConstOrCvar(lhsType) &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            rhsTypeOrUnset(rhsResult) == VarType_Wave)
        {
            auto lhsCopy = std::make_shared<EvalResults>(lhsResult);  // @0x22f166
            auto rhsCopy = std::make_shared<EvalResults>(rhsResult);  // @0x22f18c
            result = scaleWaveform(lhsCopy, rhsCopy, ctx);    // @0x22f1a9
            goto name_tail;
        }

        // ================================================================
        // Row 6: lhsType == Wave(5),
        //        rhsType ∈ {Const(4), Cvar(6)}
        //                                                    @0x22f29f→22f3e2
        // scaleWaveform(rhsCopy, lhsCopy, ctx) — SWAPPED: scalar first,
        // wave second.  Uses the 2-arg overload @0x2309e0.
        // ================================================================
        if (lhsType == VarType_Wave &&
            rhsCount(rhsResult) > 0 && rhsCount(rhsResult) <= 1 &&
            isConstOrCvar(rhsTypeOrUnset(rhsResult)))
        {
            auto rhsCopy = std::make_shared<EvalResults>(rhsResult);  // @0x22f302
            auto lhsCopy = std::make_shared<EvalResults>(lhsResult);  // @0x22f328
            result = scaleWaveform(rhsCopy, lhsCopy, ctx);    // @0x22f345
            goto name_tail;
        }

        // ================================================================
        // Default: error 0x8c.                               @0x22f3e2→22f4c9
        // ================================================================
        {
            VarType lhsT = lhsType;
            VarType rhsT = rhsTypeOrUnset(rhsResult);
            ctx.messages->errorMessage(                        // @0x22f465
                ErrorMessages::format(ErrorMessageT(0x8c),
                                      str(lhsT), str(rhsT)),
                -1);
        }
    }

name_tail:
    // Common tail @0x22f4c9-22f62e: construct
    //   result->name_ = lhsResult.name_ + " * " + rhsResult.name_
    //
    // Binary builds this by:
    //   1. Copy lhsResult.name_ into a local string.
    //   2. Append literal " * " (DWORD 0x202a20 = " * \0" at +len).  @0x22f597
    //   3. Append rhsResult.name_.
    //   4. Move-assign into result->name_.
    result->name_ = lhsResult.name_ + " * " + rhsResult.name_;  // @0x22f4c9-22f62e

    return result;                                            // @0x22f62e
}

// ============================================================================
// SeqCDiv::evaluate (5-arg) — @0x231070 (3664 bytes)
//
// Implements `lhs / rhs` lowering.  Unlike Plus/Minus/Mult, this operator
// has NO Var paths (integer division not supported in the ISA), includes
// divide-by-zero checks using floatEqual(), and uses three distinct error
// reporting mechanisms (direct BST lookup, errMsg[], ErrorMessages::format).
//
// Register/arg mapping (Itanium ABI sret + this shifts everything by 1):
//   rdi=hidden retptr, rsi=this, rdx=res(.ptr_), rcx=ctx, r8=state,
//   r9=lhsResult, [rbp+0x10]=rhsResult.
//
// Binary quirk: copies rhsResult to a stack local at [rbp-0x198] at function
// entry (EvalResults copy ctor @0x231c60).  All rhs reads go through the
// copy, including the name_tail.  In the Wave÷Const/Cvar path the copy is
// mutated (setValue(reciprocal)) before being wrapped in a shared_ptr.  We
// avoid the early copy in reconstruction and only copy where needed.
//
// Key structural differences from other arithmetic operators:
//   - Error code 0x8d for default (type mismatch); 0xdf for Var in either
//     operand; 0x2a for Const/Cvar÷Wave; 0x29 for divide-by-zero.
//   - Var(2) in EITHER operand → error 0xdf (direct BST lookup on
//     ErrorMessages::messages, NOT ErrorMessages::format).
//   - Const/Cvar ÷ Wave → error 0x2a (direct BST lookup).
//   - Divide-by-zero → error 0x29 (BST lookup in Const÷Const path;
//     errMsg[0x29] in Wave÷Const path — both resolve to same string).
//   - Const/Cvar ÷ Const/Cvar → floatEqual(rhs, 0.0) check, then
//     combine(VarType), combine(VarSubType), toDouble() division, setValue.
//   - Wave ÷ Const/Cvar → floatEqual zero check, then compute reciprocal
//     (1.0 / rhs), modify a copy of rhsResult via setValue(double), then
//     scaleWaveform(reciprocalCopy, waveCopy, ctx) using 2-arg overload
//     @0x2309e0.
//   - NO Wave ÷ Wave path (falls to default error).
//   - NO String paths.
//   - Common tail: " / " separator (DWORD 0x202f20).
//
// See reconstructed/notes/frontend_lowering.md "SeqCDiv::evaluate"
// for the full action table and binary-address breadcrumbs.
// ============================================================================

std::shared_ptr<EvalResults> SeqCDiv::evaluate(
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& /*state*/,
    EvalResults const& lhsResult,
    EvalResults const& rhsResult) const
{
    // Neither `this`, `res`, nor `state` are used in the binary.

    // 1. Allocate empty result.                              // @0x23108d-2310e7
    auto result = std::make_shared<EvalResults>();

    // 2. Helper: (type | 0x2) == 6 ↔ type ∈ {Const(4), Cvar(6)}.
    // isConstOrCvar — promoted to file-scope

    // 3. Read lhsResult.values_ size and type.               // @0x231101-231112
    const size_t lhsCount = lhsResult.values_.size();
    VarType lhsType = VarType_Unset;
    if (lhsCount == 1)
        lhsType = lhsResult.values_.back().varType_;

    // Helpers for rhs access.
    // rhsCount — promoted to file-scope
    // rhsTypeOrUnset — promoted to file-scope

    // ================================================================
    // Row 1 & 2: Var(2) in either operand → error 0xdf.
    //                                                    @0x23112b→2312b4
    // Binary searches ErrorMessages::messages BST for key 0xdf and
    // passes the string directly to errorMessage (no format).
    // If lhsType==Var → jump to BST search (rhs not checked).
    // If lhsType!=Var but rhsType==Var → also jump to BST search.
    // ================================================================
    if ((lhsCount == 1 && lhsType == VarType_Var) ||           // @0x23112b
        (rhsCount(rhsResult) == 1 && rhsTypeOrUnset(rhsResult) == VarType_Var))  // @0x231158
    {
        // Direct BST lookup for 0xdf → errorMessage(string, -1).
        ctx.messages->errorMessage(                            // @0x2312bd
            ErrorMessages::messages.at(static_cast<int>(
                ErrorMessageT(0xdf))),
            -1);
        goto name_tail;
    }

    // ================================================================
    // Size guard: lhsCount must be exactly 1 for remaining rows.
    //                                                    @0x2311b2→2312ad
    // If lhsCount == 0 or > 1, fall directly to default error with
    // lhsType = VarType_Unset.
    // ================================================================
    if (lhsCount == 0 || lhsCount > 1) {
        VarType rhsT = rhsTypeOrUnset(rhsResult);
        ctx.messages->errorMessage(                            // @0x23139a
            ErrorMessages::format(ErrorMessageT(0x8d),
                                  str(VarType_Unset), str(rhsT)),
            -1);
        goto name_tail;
    }

    {
        // lhsCount == 1 guaranteed past this point.
        // lhsType ∉ {Var, Unset} guaranteed (Var caught above, Unset only if count!=1).

        // ================================================================
        // Row 3: lhsType ∈ {Const(4), Cvar(6)},
        //        rhsType ∈ {Const(4), Cvar(6)}
        //                                                    @0x2311cf→231a66
        // Divide-by-zero check via floatEqual(rhs, 0.0):
        //   - If zero: BST search for 0x29 → errorMessage → name_tail.
        //   - Else: combine types, toDouble division, setValue.
        // ================================================================
        if (isConstOrCvar(lhsType) &&
            rhsCount(rhsResult) == 1 && isConstOrCvar(rhsTypeOrUnset(rhsResult)))
        {
            // Extract rhs value for zero check.              // @0x231200→23174a
            double rhsCheck =
                rhsResult.getValue().toDouble();        // @0x231751
            if (floatEqual(rhsCheck, 0.0)) {                   // @0x23175a
                // Divide-by-zero: BST search for 0x29.       // @0x2317a1→2318d9
                ctx.messages->errorMessage(                    // @0x2318e5
                    ErrorMessages::messages.at(static_cast<int>(
                        ErrorMessageT(0x29))),
                    -1);
                goto name_tail;
            }

            // Not zero: combine types and divide.
            VarType combinedType = combine(                    // @0x231836
                lhsType,
                rhsResult.values_.back().varType_);
            VarSubType combinedSub = combine(                  // @0x231892
                lhsResult.values_.back().varSubType_,
                rhsResult.values_.back().varSubType_);

            double lhsVal =
                lhsResult.getValue().toDouble();        // @0x231979
            double rhsVal =
                rhsResult.getValue().toDouble();        // @0x231a35
            double quotient = lhsVal / rhsVal;                 // divsd @0x231a42

            result->setValue(combinedType, combinedSub,        // @0x231a66
                             Value(quotient));
            goto name_tail;
        }

        // ================================================================
        // Row 4: lhsType ∈ {Const(4), Cvar(6)}, rhsType == Wave(5)
        //                                                    @0x23123b→2312b4
        // Error 0x2a — can't divide by a waveform.
        // Direct BST lookup for 0x2a → errorMessage(string, -1).
        // ================================================================
        if (isConstOrCvar(lhsType) &&
            rhsCount(rhsResult) == 1 && rhsTypeOrUnset(rhsResult) == VarType_Wave)
        {
            ctx.messages->errorMessage(                        // @0x2312bd
                ErrorMessages::messages.at(static_cast<int>(
                    ErrorMessageT(0x2a))),
                -1);
            goto name_tail;
        }

        // ================================================================
        // Row 5: lhsType == Wave(5),
        //        rhsType ∈ {Const(4), Cvar(6)}
        //                                                    @0x2312c7→2316f5
        // Division by scalar: compute reciprocal (1.0 / rhs), then
        // scaleWaveform(reciprocalCopy, waveCopy, ctx).
        // Divide-by-zero check: if zero → error 0x29 via errMsg[0x29].
        // ================================================================
        if (lhsType == VarType_Wave &&
            rhsCount(rhsResult) == 1 && isConstOrCvar(rhsTypeOrUnset(rhsResult)))
        {
            // Divide-by-zero check.                           // @0x2312f3→231628
            double rhsCheck =
                rhsResult.getValue().toDouble();        // @0x231628
            if (floatEqual(rhsCheck, 0.0)) {                   // @0x231631
                // error 0x29 via errMsg[0x29].                // @0x231664→2318dd
                ctx.messages->errorMessage(                    // @0x2318e5
                    errMsg[ErrorMessageT(0x29)], -1);
                goto name_tail;
            }

            // Not zero: compute reciprocal and scale.
            // Binary: rhsCopy.getValue().toDouble() → 1.0/val → rhsCopy.setValue(reciprocal)
            // Then make_shared copies and scaleWaveform.
            double rhsDouble =
                rhsResult.getValue().toDouble();        // @0x231694
            double reciprocal = 1.0 / rhsDouble;               // divsd @0x2316a1

            // Binary modifies its stack copy of rhsResult: setValue(reciprocal).
            // We create a shared_ptr copy and modify that instead.
            auto scalarCopy =
                std::make_shared<EvalResults>(rhsResult);      // @0x2316cc
            scalarCopy->setValue(reciprocal);                   // @0x2316b0

            auto waveCopy =
                std::make_shared<EvalResults>(lhsResult);      // @0x2316db

            result = scaleWaveform(scalarCopy, waveCopy, ctx); // @0x2316f5
            goto name_tail;
        }

        // ================================================================
        // Default: error 0x8d.                               @0x231328→23139a
        // Uses ErrorMessages::format with str(lhsType) and str(rhsType).
        // ================================================================
        {
            VarType lhsT = lhsType;
            VarType rhsT = rhsTypeOrUnset(rhsResult);
            ctx.messages->errorMessage(                        // @0x23139a
                ErrorMessages::format(ErrorMessageT(0x8d),
                                      str(lhsT), str(rhsT)),
                -1);
        }
    }

name_tail:
    // Common tail @0x23140a-0x23155a: construct
    //   result->name_ = lhsResult.name_ + " / " + rhsResult.name_
    //
    // Binary builds this by:
    //   1. Copy lhsResult.name_ into a local string.
    //   2. Append literal " / " (DWORD 0x202f20 = " / \0").  @0x2314d9
    //   3. Append rhsResult.name_ (binary reads from stack copy).
    //   4. Move-assign into result->name_.
    result->name_ = lhsResult.name_ + " / " + rhsResult.name_;  // @0x23140a-23155a

    return result;                                            // @0x231566-23157b
}

// ============================================================================
//  — Batch 1: Trivial evaluate() overrides
// ============================================================================

// ---- Returns nullptr (16B bodies) ----------------------------------------

// SeqCCommand::evaluate(3) — @0x209db0
// Returns null shared_ptr (zeroes 16 bytes via sret). Identical to base class.

} // namespace zhinst
