// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// seqc_ast_nodes_evaluate.cpp — Phase 21h
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

#include "zhinst/seqc_ast_node.hpp"
#include "zhinst/asm_commands.hpp"
#include <cmath>

#include "zhinst/eval_results.hpp"
#include "zhinst/resources.hpp"
#include "zhinst/frontend_lowering.hpp"
#include "zhinst/compiler_message.hpp"
#include "zhinst/wavetable_front.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/waveform_generator.hpp"
#include "zhinst/custom_functions.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/types.hpp"

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
                ErrorMessages::format(ErrorMessageT(0x91), str(lhs), str(rhs)));
        return rhs;
    }

    if (static_cast<unsigned>(lhs) > 6 || static_cast<unsigned>(rhs) > 6)
        throw VarTypeException(
            ErrorMessages::format(ErrorMessageT(0x91), str(lhs), str(rhs)));

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
// Fully reconstructed in Phase 21h.9: NOT a .rodata lookup table — pure
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

namespace {

// ============================================================================
// Shared helpers — promoted from per-function lambdas (Phase 25d).
// These were identically defined in 5-11 operator evaluate() overrides each.
// ============================================================================

// isConstOrCvar — now in resources.hpp (Phase 25g); available via zhinst::isConstOrCvar.

// getBackReg — extract AsmRegister from values_.back(), or default(0).
// Binary uses `end - 8` trick to read the last 8 bytes of
// EvalResultValue (AsmRegister at +0x30 of the 0x38-byte struct).
inline AsmRegister getBackReg(EvalResults const& er) {
    if (er.values_.empty())
        return AsmRegister(0);
    return er.values_.back().reg_;
}

// rhsCount — size of rhs values vector.
inline size_t rhsCount(EvalResults const& rhs) {
    return rhs.values_.size();
}

// rhsTypeOrUnset — VarType of the single rhs value, or Unset if 0 or >1.
inline VarType rhsTypeOrUnset(EvalResults const& rhs) {
    if (rhs.values_.empty() || rhs.values_.size() > 1)
        return VarType_Unset;
    return rhs.values_.back().varType_;
}

// rhsSubOrDefault — VarSubType of the single rhs value, or Default if 0 or >1.
inline VarSubType rhsSubOrDefault(EvalResults const& rhs) {
    if (rhs.values_.empty() || rhs.values_.size() > 1)
        return VarSubType_Default;
    return rhs.values_.back().varSubType_;
}

// ============================================================================

// combineWaveforms @0x22c300 — combines two waveform-type EvalResults
// using the named operation (e.g., "add", "sub", "mul", "div").
//
// Signature (Itanium ABI sret):
//   rdi=sret, rsi=opName (string const&), rdx=lhsResult (EvalResults const&),
//   rcx=rhsResult (EvalResults const&), r8=ctx (FrontendLoweringContext&).
//
// Phase 21h.4-followup resolved: body implemented below (Phase 21h.8).
std::shared_ptr<EvalResults> combineWaveforms(
    std::string const& opName,
    EvalResults const& lhs,
    EvalResults const& rhs,
    FrontendLoweringContext& ctx);

// constWaveform @0x22c9f0 — creates a constant rectangular waveform
// EvalResults by calling WaveformGenerator::eval("rect", ...).
//
// Signature (Itanium ABI sret + xmm0 for double):
//   rdi=sret, esi=sampleLength, xmm0=value (double), rdx=ctx&.
//
// Phase 21h.4-followup resolved: body implemented below (Phase 21h.8).
std::shared_ptr<EvalResults> constWaveform(
    int sampleLength,
    double value,
    FrontendLoweringContext& ctx);

// scaleWaveform @0x228cc0 — negates a waveform's samples by applying a scale
// factor.  Used by SeqCMinus for Wave subtraction paths (negate rhs waveform
// before adding).
//
// Demangled signature:
//   (anonymous namespace)::scaleWaveform(int, shared_ptr<EvalResults>,
//                                        FrontendLoweringContext&)
//
// However, at both call sites (22d227 and 22e5c4) `esi` (the int parameter)
// is NOT explicitly set — the shared_ptr<EvalResults> ref is in rsi and
// ctx& is in rdx, suggesting the sret output occupies rdi and the int may
// be dead/unused.  We declare per the mangled name but note the discrepancy.
//
// Phase 21h.5-followup resolved: body implemented below (Phase 21h.8).
// Confirmed: int param is unused.
std::shared_ptr<EvalResults> scaleWaveform(
    int scaleFactor,
    std::shared_ptr<EvalResults> er,
    FrontendLoweringContext& ctx);

// scaleWaveform @0x2309e0 (2-arg overload) — scales a waveform by a scalar
// EvalResults value.  Used by SeqCMult for Wave*Const/Cvar and Const/Cvar*Wave
// paths.
//
// Demangled:
//   (anonymous namespace)::scaleWaveform(std::__1::shared_ptr<zhinst::EvalResults>,
//                                        std::__1::shared_ptr<zhinst::EvalResults>,
//                                        zhinst::FrontendLoweringContext&)
//
// First param is the scalar (Const/Cvar), second is the waveform.
// If the waveform's values_[0].subType == FunctionArg: passthrough (returns
// the waveform param unchanged).
// Otherwise: extracts values from both, builds vector, calls
// WaveformGenerator::eval("scale", values).
//
// Phase 21h.6-followup resolved: body implemented below (Phase 21h.8).
std::shared_ptr<EvalResults> scaleWaveform(
    std::shared_ptr<EvalResults> scalar,
    std::shared_ptr<EvalResults> wave,
    FrontendLoweringContext& ctx);

// computeMult @0x22fdf0 — complex shift-and-add multiplication for Var*Const
// paths.  Emits a sequence of ssl/addr/addi/asmZero instructions to implement
// integer multiplication by bit decomposition.
//
// Demangled:
//   (anonymous namespace)::computeMult(std::__1::shared_ptr<zhinst::EvalResults>,
//                                      std::__1::shared_ptr<zhinst::EvalResults>,
//                                      std::__1::shared_ptr<zhinst::Resources>,
//                                      zhinst::FrontendLoweringContext)  // by value!
//
// First param is always the Var-typed operand, second is the Const/Cvar-typed
// operand.  Call sites swap arguments to ensure this ordering.
//
// Algorithm:
//   1. Extract const value, floor() to integer, verify integrality via
//      floatEqual(value, floor(value)).
//   2. If non-integer or negative: error 0x8a.
//   3. If FunctionArg subtype: passthrough (return Var operand).
//   4. If multiplier == 0: emit asmZero(resultReg).
//   5. Else: shift-and-add loop (up to 32 bits) emitting ssl/addr
//      instructions.
//
// Phase 21h.6-followup resolved: full body implemented below (Phase 21h.8).
std::shared_ptr<EvalResults> computeMult(
    std::shared_ptr<EvalResults> varOperand,
    std::shared_ptr<EvalResults> constOperand,
    std::shared_ptr<Resources> res,
    FrontendLoweringContext ctx);  // by value (not reference)

// ---- Phase 22d: Additional helper forward declarations --------------------

// evalGreater @0x235ac0 — evaluates a > b for all VarType combinations.
// Signature (from mangled name):
//   (EvalResults const&, EvalResults const&, shared_ptr<Resources>, FrontendLoweringContext&)
std::shared_ptr<EvalResults> evalGreater(
    EvalResults const& lhs,
    EvalResults const& rhs,
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx);  // body below @0x235ac0 (5865B)

// evalLower @0x237440 — evaluates a < b for all VarType combinations.
// Dedicated function (NOT swapped-arg evalGreater). Used by SeqCLower, SeqCGEqual.
std::shared_ptr<EvalResults> evalLower(
    EvalResults const& lhs,
     EvalResults const& rhs,
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx);  // body below @0x237440 (6176B)

// evalEqual @0x239be0 — evaluates a == b for all VarType combinations.
std::shared_ptr<EvalResults> evalEqual(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    EvalResults const& lhs,
    EvalResults const& rhs);  // body below @0x239be0 (7703B)

// evalShift @0x232850 — evaluates shift (left or right) for all VarType combinations.
// isRight=false for <<, isRight=true for >>.
std::shared_ptr<EvalResults> evalShift(
    EvalResults const& lhs,
    EvalResults const& rhs,
    std::shared_ptr<Resources> res,
    bool isRight,
    FrontendLoweringContext& ctx);  // body below @0x232850 (11838B)

// evalAnd @0x23ea20 — evaluates bitwise a & b for all VarType combinations.
std::shared_ptr<EvalResults> evalAnd(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    EvalResults const& lhs,
    EvalResults const& rhs);  // body below @0x23ea20 (7674B)

// evalOr @0x240a30 — evaluates bitwise a | b for all VarType combinations.
std::shared_ptr<EvalResults> evalOr(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    EvalResults const& lhs,
    EvalResults const& rhs);  // body below @0x240a30 (7547B)

// valueToBool @0x242e40 — converts an EvalResults to a boolean representation.
// Used by invertBool, evalAnd, evalOr, SeqCLogAnd, SeqCLogOr.
// If input already has exactly 1 value with Stub subtype, returns it unchanged.
// Otherwise: creates fresh EvalResults, copies assemblers from input, dispatches
// on varType to produce a boolean (Var→emit register test, Const/Cvar→toInt).
std::shared_ptr<EvalResults> valueToBool(
    std::shared_ptr<EvalResults> result,
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx);

// invertBool @0x238fb0 — inverts a boolean EvalResults (0→1, 1→0).
// Used by SeqCLEqual (negate evalGreater), SeqCNEqual (negate evalEqual),
// and SeqCNotExpr.
std::shared_ptr<EvalResults> invertBool(
    std::shared_ptr<EvalResults> result,
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx);

// jumpIfZero @0x2149f0 — generates conditional branch instructions
// that skip to `label` when the condition value is zero.
//
// Demangled signature (anonymous namespace):
//   jumpIfZero(shared_ptr<EvalResults>, string const&, FrontendLoweringContext&)
//
// Despite the demangled name, the binary calling convention passes
// condResult->values_ begin/end iterators directly (compiler optimization
// for anonymous-namespace function).  Source-level reconstruction uses
// the shared_ptr signature; the compiler will handle member access.
//
// Algorithm:
//   1. If values_ empty → return empty vector.
//   2. If values_ has > 1 element → errorMessage(errMsg[0x7e]), return empty.
//   3. VarType == Var(2): emit brz(reg, label, false) → return {asm}.
//   4. VarType Const/Cvar ((|2)==6): toInt() → if zero, also emit
//      br(label, false) → return {asm}; if nonzero → return empty (no jump).
//   5. Otherwise → errorMessage(errMsg[0x7e]), return empty.
//
// Used 42 times across binary (SeqCIfCondition, SeqCIfElse, SeqCWhileLoop,
// SeqCDoWhile, SeqCRepeat, SeqCForLoop, etc.).
std::vector<AsmList::Asm> jumpIfZero(
    std::shared_ptr<EvalResults> condResult,
    std::string const& label,
    FrontendLoweringContext& ctx);

// ============================================================================
// Implementation bodies for the anonymous-namespace helper functions.
// ============================================================================

// scaleWaveform (1-arg overload) — @0x228cc0
//
// The int parameter (scaleFactor) is completely unused in the binary
// (its register is clobbered by operator new without being read).
// Creates a scalar EvalResults with value -1.0, then delegates to the
// 2-arg scaleWaveform overload.
std::shared_ptr<EvalResults> scaleWaveform(  // @0x228cc0
    int /*scaleFactor*/,
    std::shared_ptr<EvalResults> wave,
    FrontendLoweringContext& ctx)
{
    auto scalar = std::make_shared<EvalResults>();
    scalar->setValue(-1.0);                                          // @0x228d16
    return scaleWaveform(std::move(scalar), std::move(wave), ctx);   // @0x228d3b
}

// constWaveform — @0x22c9f0
//
// Creates a constant rectangular waveform by calling
// WaveformGenerator::eval("rect", {Value(sampleLength), Value(value)}).
// Two catch clauses handle WaveformGeneratorValueException (filter 2,
// caught first) and WaveformGeneratorException (filter 1).
std::shared_ptr<EvalResults> constWaveform(  // @0x22c9f0
    int sampleLength,
    double value,
    FrontendLoweringContext& ctx)
{
    auto result = std::make_shared<EvalResults>();                   // @0x22ca19

    std::vector<Value> values;
    values.emplace_back(sampleLength);                               // Value(int32_t)  @0x22ca52
    values.emplace_back(value);                                      // Value(double)    @0x22ca9f

    try {
        result = ctx.waveformGen->eval("rect", values);              // @0x22cb50
    } catch (WaveformGeneratorValueException const& e) {             // filter 2 (caught first)
        ctx.messages->errorMessage(std::string(e.what()), -1);       // @0x22cc14
    } catch (WaveformGeneratorException const& e) {                  // filter 1
        ctx.messages->errorMessage(std::string(e.what()), -1);       // @0x22ccba
    }

    return result;                                                   // @0x22cd19
}

// combineWaveforms — @0x22c300
//
// Combines two waveform-type EvalResults using the named operation
// (e.g. "add", "sub", "mul", "div") via WaveformGenerator::eval().
// Short-circuits if either operand's single value has VarSubType
// FunctionArg — returns a copy of that operand unchanged.
std::shared_ptr<EvalResults> combineWaveforms(  // @0x22c300
    std::string const& opName,
    EvalResults const& lhs,
    EvalResults const& rhs,
    FrontendLoweringContext& ctx)
{
    // FunctionArg passthrough: lhs
    if (lhs.values_.size() == 1 &&
        lhs.values_.back().varSubType_ == VarSubType_FunctionArg) {  // @0x22c33b
        return std::make_shared<EvalResults>(lhs);                   // copy-construct
    }

    // FunctionArg passthrough: rhs
    if (rhs.values_.size() == 1 &&
        rhs.values_.back().varSubType_ == VarSubType_FunctionArg) {  // @0x22c3a2
        return std::make_shared<EvalResults>(rhs);                   // copy-construct
    }

    // Normal path: extract values and call waveformGen->eval(opName, ...)
    auto result = std::make_shared<EvalResults>();                   // @0x22c3f5

    // Extract lhs value (default Value() if values_ is empty)
    Value lhsVal;
    if (!lhs.values_.empty()) {
        lhsVal = lhs.values_.back().value_;                          // @0x22c442
    }

    // Extract rhs value (default Value() if values_ is empty)
    Value rhsVal;
    if (!rhs.values_.empty()) {
        rhsVal = rhs.values_.back().value_;                          // @0x22c4e2
    }

    std::vector<Value> values;
    values.push_back(lhsVal);                                        // @0x22c530
    values.push_back(rhsVal);                                        // @0x22c57a

    try {
        result = ctx.waveformGen->eval(opName, values);              // @0x22c5e1
    } catch (WaveformGeneratorValueException const& e) {             // filter 2
        ctx.messages->errorMessage(std::string(e.what()), -1);       // @0x22c6a6
    } catch (WaveformGeneratorException const& e) {                  // filter 1
        ctx.messages->errorMessage(std::string(e.what()), -1);       // @0x22c74c
    }

    return result;                                                   // @0x22c7a6
}

// scaleWaveform (2-arg overload) — @0x2309e0
//
// Scales a waveform by a scalar value via WaveformGenerator::eval("scale",...).
// Short-circuits if the wave operand (2nd param) is a FunctionArg — returns
// the wave shared_ptr directly via move.
// Note: values are pushed wave-first, scalar-second into the vector.
std::shared_ptr<EvalResults> scaleWaveform(  // @0x2309e0
    std::shared_ptr<EvalResults> scalar,
    std::shared_ptr<EvalResults> wave,
    FrontendLoweringContext& ctx)
{
    // FunctionArg passthrough (wave only)
    if (wave->values_.size() == 1 &&
        wave->values_.back().varSubType_ == VarSubType_FunctionArg) {  // @0x230a2d
        return std::move(wave);                                      // @0x230a5d
    }

    // Normal path
    auto result = std::make_shared<EvalResults>();                   // @0x230a90

    // Extract wave value first, scalar second (this order matches binary)
    Value waveVal;
    if (!wave->values_.empty()) {
        waveVal = wave->values_.back().value_;                       // @0x230add
    }

    Value scalarVal;
    if (!scalar->values_.empty()) {
        scalarVal = scalar->values_.back().value_;                   // @0x230b56
    }

    std::vector<Value> values;
    values.push_back(waveVal);                                       // wave first
    values.push_back(scalarVal);                                     // scalar second

    try {
        result = ctx.waveformGen->eval("scale", values);             // @0x230c40
    } catch (WaveformGeneratorValueException const& e) {             // filter 2
        ctx.messages->errorMessage(std::string(e.what()), -1);       // @0x230d05
    } catch (WaveformGeneratorException const& e) {                  // filter 1
        ctx.messages->errorMessage(std::string(e.what()), -1);       // @0x230dab
    }

    return result;                                                   // @0x230e05
}

// computeMult — @0x22fdf0
//
// Complex shift-and-add multiplication for Var*Const paths.  Emits a
// sequence of ssl/addr/addi/asmZero instructions to implement integer
// multiplication by MSB-first bit decomposition.
//
// NOTE: ctx is passed by value (not reference) per the mangled signature.
// NOTE: res (shared_ptr<Resources>) is present in the signature for ABI
// compliance but is completely unused in the function body.
//
// Algorithm:
//   1. Extract const value → toDouble → floor → integrality check.
//   2. If non-integer or negative → error VarMultNatural (0x8a).
//   3. If either operand is FunctionArg → passthrough (return varOperand).
//   4. Extract multiplier as int, allocate result register.
//   5. Merge var operand assemblers into result.
//   6. If multiplier == 0 → emit asmZero.
//   7. Else → 32-iteration MSB-first shift-and-add loop.
std::shared_ptr<EvalResults> computeMult(  // @0x22fdf0
    std::shared_ptr<EvalResults> varOperand,
    std::shared_ptr<EvalResults> constOperand,
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext ctx)  // by value
{
    auto result = std::make_shared<EvalResults>();                    // @0x22fe1c

    // --- Step 1: integrality and non-negativity check ---
    Value constVal;
    if (!constOperand->values_.empty()) {
        constVal = constOperand->values_.back().value_;              // @0x22fe60
    }

    double origValue = constVal.toDouble();                          // @0x22feb8
    double floored = std::floor(origValue);                          // @0x22fee4

    if (!floatEqual(origValue, floored) || floored < 0.0) {          // @0x22fef4, @0x22ff00
        // Error 0x8a = VarMultNatural (138): multiplier must be natural number
        std::string msg = ErrorMessages::format(VarMultNatural);     // @0x22ff2e
        ctx.messages->errorMessage(msg, -1);                         // @0x22ff4f
        return result;
    }

    // --- Step 2: FunctionArg passthrough (check BOTH operands) ---
    if (!varOperand->values_.empty() &&
        varOperand->values_.back().varSubType_ == VarSubType_FunctionArg) {  // @0x22ffa0
        return std::move(varOperand);
    }
    if (!constOperand->values_.empty() &&
        constOperand->values_.back().varSubType_ == VarSubType_FunctionArg) {  // @0x22ffd0
        return std::move(varOperand);  // always returns varOperand, not constOperand
    }

    // --- Step 3: extract multiplier as int ---
    Value constVal2;
    if (!constOperand->values_.empty()) {
        constVal2 = constOperand->values_.back().value_;             // @0x230050
    }
    int multiplier = constVal2.toInt();                              // @0x230090

    // --- Step 4: allocate result register, set result type ---
    int regNum = Resources::getRegisterNumber();                     // @0x2300a3 (static)
    AsmRegister resultReg(regNum);
    result->setValue(VarType_Var, regNum);                            // @0x2300c0

    // --- Step 5: merge var operand's assemblers into result ---
    result->assemblers_.insert(
        result->assemblers_.end(),
        varOperand->assemblers_.begin(),
        varOperand->assemblers_.end());                              // @0x2300ff

    // --- Step 6: extract var register ---
    AsmRegister varReg;
    if (!varOperand->values_.empty()) {
        varReg = varOperand->values_.back().reg_;                    // @0x230137
    }

    // --- Step 7: emit multiplication instructions ---
    if (multiplier == 0) {                                           // @0x23014b
        // Special case: multiplier is zero → emit asmZero
        AsmList::Asm zeroAsm = ctx.asmCommands->asmZero(resultReg); // @0x230159
        result->assemblers_.push_back(std::move(zeroAsm));           // @0x23016f
    } else {
        // MSB-first shift-and-add loop (32 iterations)              // @0x230192
        bool firstBitSeen = false;

        for (int i = 0; i < 32; ++i) {                              // @0x230197
            if (firstBitSeen) {
                // Shift result left by 1: emit ssl(resultReg)
                AsmList::Asm sslAsm = ctx.asmCommands->ssl(resultReg);   // @0x2301b5
                result->assemblers_.push_back(std::move(sslAsm));         // @0x2301d9
            }

            if (multiplier < 0) {  // sign bit set → current MSB is 1  // @0x2301f4
                if (!firstBitSeen) {
                    // First '1' bit: copy var register → result register
                    // addi(dst, src, Immediate(0)) is a register copy
                    auto addiVec = ctx.asmCommands->addi(                 // @0x23021e
                        resultReg, varReg, Immediate(static_cast<int32_t>(0)));
                    for (auto& entry : addiVec) {
                        result->assemblers_.push_back(std::move(entry));  // @0x23025a
                    }
                } else {
                    // Subsequent '1' bit: accumulate via addr
                    AsmList::Asm addAsm = ctx.asmCommands->addr(          // @0x230290
                        resultReg, varReg);
                    result->assemblers_.push_back(std::move(addAsm));     // @0x2302b2
                }
                firstBitSeen = true;
            }

            multiplier <<= 1;                                            // @0x2302d3
        }
    }

    return result;
}

// ============================================================================
// valueToBool @0x242e40 — 2546 bytes (0x9f2)
//
// Converts an EvalResults to a boolean representation.
// If input already has exactly 1 value with subType == Stub, returns it
// unchanged (move-semantics shortcut).
// Otherwise: creates fresh EvalResults, copies assemblers_ from input,
// dispatches on varType to extract a boolean int (Var→register compare,
// Const/Cvar→toInt != 0), then setValue(VarType_Var, regNum).
//
// Called by: invertBool, evalAnd (2x), evalOr (2x), SeqCLogAnd (2x),
// SeqCLogOr (2x).
// ============================================================================

// valueToBool body moved after invertBool (below) — needs invertBool
// to call it, but invertBool also needs to be declared first. Both
// are forward-declared above; the body is placed after invertBool for
// readability.

// ============================================================================
// invertBool @0x238fb0 — 1589 bytes (0x635)
//
// Inverts a boolean EvalResults value: 0→1, 1→0.
//
// Algorithm:
// 1. If result is null → return empty make_shared<EvalResults>().
// 2. If result has exactly 1 value with subType == Stub → skip valueToBool.
//    Otherwise → call valueToBool(result, res, ctx) to normalize to boolean.
// 3. After normalization, check values_ count:
//    - If != 1 → emit error 0x10 (TypeMismatch) with str(varType).
// 4. Dispatch on varType:
//    - Var(2): emit xnori(newReg, lastReg, Immediate(-2)) to flip bit 0,
//              setValue(VarType_Var, newRegNum).
//    - Const(4) or Cvar(6): extract Value, toInt() ^ 1, setValue with
//              inverted Value.
//    - Other: emit error 0x10.
// 5. Return result (moved).
//
// Key constant: Immediate(-2) = 0xFFFFFFFE — xnori flips bit 0, which
// inverts a boolean {0,1} value.
// ============================================================================

std::shared_ptr<EvalResults> invertBool(                    // @0x238fb0
    std::shared_ptr<EvalResults> result,
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx)
{
    // 1. Null input → return fresh empty EvalResults.                @0x238fcd-239160
    if (!result) {
        return std::make_shared<EvalResults>();              // @0x239160-2391b4
    }

    // 2. Check if already boolean (Stub subtype shortcut).          @0x238fd9-239009
    {
        auto const& vals = result->values_;
        bool isStub = (vals.size() == 1 &&
                       vals[0].varSubType_ == VarSubType_Stub);
        if (!isStub) {
            // Convert to boolean via valueToBool.                   @0x23900f-239051
            result = valueToBool(std::move(result), std::move(res), ctx);
        }
    }

    // 3. Re-check values_ count after normalization.                @0x239123-23912d
    auto const& vals = result->values_;
    if (vals.size() != 1) {
        // Error: expected exactly 1 value for boolean inversion.    @0x23914e-239225
        VarType vt = vals.empty() ? VarType_Unset : vals[0].varType_;
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x10), str(vt));                  // @0x2391c8-2391d9
        ctx.messages->errorMessage(msg, -1);                // @0x2391ea
        return std::move(result);                           // @0x2394e7
    }

    VarType varType = vals[0].varType_;

    // 4. Dispatch on varType.
    if (varType == VarType_Var) {
        // Var path: emit xnori(destReg, srcReg, Immediate(-2)) to
        // flip bit 0 of the boolean register value.                 @0x239237
        int regNum = Resources::getRegisterNumber();        // @0x239237
        AsmRegister destReg(regNum);                        // @0x23923c-239242

        // Source register: last values_ entry's reg_ field
        // (or AsmRegister(0) if somehow empty — defensive).        @0x239260-2392c5
        AsmRegister srcReg = vals.empty() ? AsmRegister(0)
                                          : vals.back().reg_;

        // Emit xnori: ~(srcReg ^ 0xFFFFFFFE) = srcReg with bit 0 flipped.
        auto xnoriEntries = ctx.asmCommands->xnori(         // @0x2392d2-2392f4
            destReg, srcReg, Immediate(-2));

        // Splice xnori instructions into result->assemblers_.      @0x2392f9-239325
        result->assemblers_.insert(
            result->assemblers_.end(),
            xnoriEntries.begin(), xnoriEntries.end());

        // Store result: Var type with new register number.          @0x2393e5-2393fb
        result->setValue(VarType_Var, static_cast<int>(destReg));

    } else if ((static_cast<int>(varType) | 2) == 6) {
        // Const(4) or Cvar(6) path: extract value, invert boolean.  @0x239271-239296
        // Extract the value via which_ dispatch and call toInt().
        Value val = vals[0].value_;                         // @0x239282-2392a9
        int inverted = val.toInt() ^ 1;                     // @0x239462-239467

        // Build inverted Value (type=Specified(1), which=int, storage=inverted).
        Value invertedVal(inverted);                        // @0x23946a-239474

        // Get varType from values_[0] for the setValue call.        @0x2394a1-2394c6
        VarType vt = vals.empty() ? VarType_Unset : vals[0].varType_;
        result->setValue(vt, invertedVal);                  // @0x2394cd

    } else {
        // Unsupported type → error 0x10.                            @0x23914e path
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x10), str(varType));
        ctx.messages->errorMessage(msg, -1);
    }

    return std::move(result);                               // @0x2394e7-2394f5
}

// ============================================================================
// valueToBool @0x242e40 (2546B) — convert EvalResults to boolean.
//
// Algorithm (from binary):
// 1. If null → return fresh empty EvalResults.
// 2. Stub shortcut: if single value with varSubType_==Stub, move-return unchanged.
// 3. Create new EvalResults, copy assemblers_ from input.
// 4. Re-read values_. If count != 1 → error 0x11. If count == 1, dispatch:
//    - Var(2): emit asmZero(destReg) + brz(srcReg, label, false) +
//              asmOne(destReg) + asmLabel(label), then
//              setValue(VarType_Var, VarSubType_Stub, Value(int(0)), regNum).
//    - Const(4): toInt() != 0 → setValue(VarType_Const, Value(int(bool))).
//    - Cvar(6): same as Const but setValue(VarType_Cvar, ...).
//    - String(3): getValue().toString().length() != 0 →
//              setValue(VarType_Const, Value(int(bool))).
//    - Other: error 0x11 with str(varType).
// 5. Return new EvalResults.
//
// Note: shared_ptr<Resources> parameter is never dereferenced in the
// body — getRegisterNumber() and newLabel() are both static.
// ============================================================================

std::shared_ptr<EvalResults> valueToBool(                   // @0x242e40
    std::shared_ptr<EvalResults> result,
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx)
{
    // 1. Null input → return fresh empty EvalResults.               @0x242e5a-242f98
    if (!result) {
        return std::make_shared<EvalResults>();
    }

    // 2. Stub shortcut: single value with Stub subtype → return as-is.
    //                                                               @0x242e69-242eac
    {
        auto const& vals = result->values_;
        if (vals.size() == 1 &&
            vals[0].varSubType_ == VarSubType_Stub) {
            return std::move(result);                       // move-return
        }
    }

    // 3. Create new EvalResults, copy assemblers from input.        @0x242eb1-242f57
    auto out = std::make_shared<EvalResults>();
    out->assemblers_.insert(
        out->assemblers_.end(),
        result->assemblers_.begin(), result->assemblers_.end());

    // 4. Re-read values_ from the original result.                 @0x242f5c-242f69
    auto const& vals = result->values_;
    if (vals.size() != 1) {
        // count == 0 or count > 1 → error 0x11 with VarType_Unset. @0x242f87-243028
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x11), str(VarType_Unset));
        ctx.messages->errorMessage(msg, -1);
        return out;                                         // @0x24306d
    }

    VarType varType = vals[0].varType_;

    if (varType == VarType_Var) {
        // Var path: emit brz-based boolean test.                    @0x24308a-2433f4
        int regNum = Resources::getRegisterNumber();        // @0x24308a (static)
        AsmRegister destReg(regNum);                        // @0x24308f-243095

        std::string label = Resources::newLabel("bool");    // @0x24309a-2430bd (static)

        // Source register from input values (defensive default).    @0x243101-24318c
        AsmRegister srcReg = result->values_.empty()
                                 ? AsmRegister(0)
                                 : result->values_.back().reg_;

        // Emit 4 asm commands: zero → brz → one → label.
        auto asmZ   = ctx.asmCommands->asmZero(destReg);   // @0x2430e2-2430f1
        auto asmBrz = ctx.asmCommands->brz(srcReg, label, false); // @0x2431a3-2431b0
        auto asmO   = ctx.asmCommands->asmOne(destReg);    // @0x2431bc-2431c7
        auto asmLbl = ctx.asmCommands->asmLabel(label);    // @0x2431cc-2431de

        // Collect into temp array for contiguous insert.            @0x2431e3-24323a
        AsmList::Asm cmds[] = { std::move(asmZ), std::move(asmBrz),
                                std::move(asmO), std::move(asmLbl) };
        out->assemblers_.insert(
            out->assemblers_.end(),
            std::make_move_iterator(std::begin(cmds)),
            std::make_move_iterator(std::end(cmds)));

        // setValue(VarType_Var, VarSubType_Stub, Value(int(0)), regNum).
        //                                                           @0x2433b6-2433f4
        Value val(static_cast<int32_t>(0));
        out->setValue(VarType_Var, VarSubType_Stub, val, regNum);

    } else if (varType == VarType_Const) {
        // Const path: toInt() → bool.                               @0x243116-243540
        Value val = vals[0].value_;                         // copy Value
        int boolVal = (val.toInt() != 0) ? 1 : 0;          // @0x24350a-243513
        Value boolValue(boolVal);                           // @0x243516-24352d
        out->setValue(VarType_Const, boolValue);            // @0x24353b-243540

    } else if (varType == VarType_Cvar) {
        // Cvar path: same as Const but with VarType_Cvar.           @0x24314f-243619
        Value val = vals[0].value_;
        int boolVal = (val.toInt() != 0) ? 1 : 0;          // @0x2435e3-2435ec
        Value boolValue(boolVal);
        out->setValue(VarType_Cvar, boolValue);             // @0x243609-243619

    } else if (varType == VarType_String) {
        // String path: getValue().toString() → non-empty = true.    @0x243442-24358e
        Value val = result->getValue();                     // @0x24344d-243451
        std::string s = val.toString();                     // @0x243456-24345e
        int boolVal = s.empty() ? 0 : 1;                   // @0x24355a-243561
        Value boolValue(boolVal);
        out->setValue(VarType_Const, boolValue);            // @0x243585-24358e
        // Note: String converts to Const (not String) — matches binary.

    } else {
        // Unsupported type → error 0x11 with actual varType.        @0x243447→242f87
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x11), str(varType));
        ctx.messages->errorMessage(msg, -1);
    }

    return out;                                             // @0x24306d
}

// ============================================================================
// evalGreater @0x235ac0 (5865B) — evaluates a > b for all VarType combinations.
//
// Algorithm (from binary):
//   1. Create new EvalResults, copy assemblers_ from both lhs and rhs.
//   2. Check both operands have exactly 1 value (otherwise error 0x92).
//   3. Dispatch on (lhsVarType, rhsVarType):
//
//      Case A — lhs=Var, rhs=Const/Cvar:
//        tempReg = addi(getRegNum(), lhsReg, Immediate(-rhs.toInt()))
//        label = newLabel("true"), boolReg = getRegNum()
//        asmOne(boolReg) → brgz(tempReg, "true", false) → asmZero(boolReg) → asmLabel("true")
//        Result: 1 if lhs > rhs, else 0.
//
//      Case B — lhs=Const/Cvar, rhs=Var:
//        tempReg = addi(getRegNum(), rhsReg, Immediate(1 - lhs.toInt()))
//        label = newLabel("false"), boolReg = getRegNum()
//        asmZero(boolReg) → brgz(tempReg, "false", false) → asmOne(boolReg) → asmLabel("false")
//        Result: 1 if lhs > rhs, else 0.
//        (tempReg = rhs + 1 - lhs; if > 0 ⇒ rhs ≥ lhs ⇒ NOT greater → stays 0)
//
//      Case C — lhs=Var, rhs=Var:
//        tempReg = addi(getRegNum(), lhsReg, Immediate(0)) [copy lhs]
//        subr(tempReg, rhsReg) [tempReg = lhs - rhs]
//        label = newLabel("true"), boolReg = getRegNum()
//        asmOne(boolReg) → brgz(tempReg, "true", false) → asmZero(boolReg) → asmLabel("true")
//
//      Case D — lhs=Const/Cvar, rhs=Const/Cvar:
//        lhsDouble = lhs.toDouble(), rhsDouble = rhs.toDouble()
//        resultType = combine(lhsVarType, lhsVarType)
//        setValue(resultType, Stub, Value(int(lhsDouble > rhsDouble ? 1 : 0)))
//
//   4. Error: format(0x92, str(lhsVarType), str(rhsVarType)),
//             errorMessage(msg, -1).
//
// Note: shared_ptr<Resources> parameter is unused in the body —
// getRegisterNumber() and newLabel() are static.
// ============================================================================

std::shared_ptr<EvalResults> evalGreater(                   // @0x235ac0
    EvalResults const& lhs,
    EvalResults const& rhs,
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx)
{
    // 1. Create result, copy assemblers from both operands.        @0x235ae1-235b9a
    auto result = std::make_shared<EvalResults>();
    result->assemblers_.insert(
        result->assemblers_.end(),
        lhs.assemblers_.begin(), lhs.assemblers_.end());
    result->assemblers_.insert(
        result->assemblers_.end(),
        rhs.assemblers_.begin(), rhs.assemblers_.end());

    // 2. Extract operand info.                                     @0x235b9f-235bb3
    auto const& lhsVals = lhs.values_;
    auto const& rhsVals = rhs.values_;

    // Check that both have exactly 1 value; otherwise dispatch or error.
    bool lhsHas1 = (lhsVals.size() == 1);
    bool rhsHas1 = (rhsVals.size() == 1);

    VarType lhsType = lhsHas1 ? lhsVals[0].varType_ : VarType_Unset;
    VarType rhsType = rhsHas1 ? rhsVals[0].varType_ : VarType_Unset;

    auto isConstOrCvar = [](VarType t) -> bool {
        return (static_cast<int>(t) | 2) == 6;  // matches Const(4) or Cvar(6)
    };

    // ---- Case A: lhs=Var, rhs=Const/Cvar -----------------------  @0x235bd5-236389
    if (lhsHas1 && lhsType == VarType_Var &&
        rhsHas1 && isConstOrCvar(rhsType))
    {
        int tempRegNum = Resources::getRegisterNumber();    // @0x235c07
        AsmRegister tempReg(tempRegNum);                    // @0x235c12

        AsmRegister lhsReg = lhsVals.empty() ? AsmRegister(0)
                                              : lhsVals.back().reg_;  // @0x235c26-235e9a
        Value rhsVal = rhsVals.empty() ? Value() : rhsVals.back().value_;  // @0x235e9d-235ecc

        // addi(tempReg, lhsReg, Immediate(-rhs.toInt()))           @0x236762-236795
        auto addiAsms = ctx.asmCommands->addi(
            tempReg, lhsReg, Immediate(-rhsVal.toInt()));

        std::string label = Resources::newLabel("true");    // @0x2367fb-23681e
        int boolRegNum = Resources::getRegisterNumber();    // @0x236843
        AsmRegister boolReg(boolRegNum);                    // @0x23684e

        auto asm1 = ctx.asmCommands->asmOne(boolReg);      // @0x236865
        auto asm2 = ctx.asmCommands->brgz(tempReg, label, false);  // @0x236883
        auto asm3 = ctx.asmCommands->asmZero(boolReg);     // @0x23689a
        auto asm4 = ctx.asmCommands->asmLabel(label);       // @0x2368b1

        // Build temp AsmList with addi results + 4 branch commands.
        AsmList::Asm cmds[] = { std::move(asm1), std::move(asm2),
                                std::move(asm3), std::move(asm4) };

        // Insert addi results first.                               @0x236075-2360f9
        result->assemblers_.insert(
            result->assemblers_.end(),
            addiAsms.begin(), addiAsms.end());
        // Insert branch commands.                                  @0x2368b6-2368d2
        result->assemblers_.insert(
            result->assemblers_.end(),
            std::make_move_iterator(std::begin(cmds)),
            std::make_move_iterator(std::end(cmds)));

        // setValue(Var, Stub, Value(int(0)), boolRegNum)           @0x236300-236348
        Value zeroVal(static_cast<int32_t>(0));
        result->setValue(VarType_Var, VarSubType_Stub, zeroVal,
                         static_cast<int>(boolReg));

        // Append result assemblers to output.                      @0x23638d-2363af
        return result;
    }

    // ---- Case B: lhs=Const/Cvar, rhs=Var -----------------------  @0x235c4a-236a89
    if (lhsHas1 && isConstOrCvar(lhsType) &&
        rhsHas1 && rhsType == VarType_Var)
    {
        int tempRegNum = Resources::getRegisterNumber();    // @0x235c7c
        AsmRegister tempReg(tempRegNum);                    // @0x235c87

        AsmRegister rhsReg = rhsVals.empty() ? AsmRegister(0)
                                              : rhsVals.back().reg_;  // @0x235c99-235f11
        Value lhsVal = lhsVals.empty() ? Value() : lhsVals.back().value_;  // @0x235f14-235f42

        // addi(tempReg, rhsReg, Immediate(1 - lhs.toInt()))       @0x236b1a-236b56
        auto addiAsms = ctx.asmCommands->addi(
            tempReg, rhsReg, Immediate(1 - lhsVal.toInt()));

        std::string label = Resources::newLabel("false");   // @0x236bbc-236be1
        int boolRegNum = Resources::getRegisterNumber();    // @0x236c06
        AsmRegister boolReg(boolRegNum);                    // @0x236c11

        // Reversed order: asmZero first (assume false),
        // brgz skips to "false" label if NOT greater,
        // asmOne sets true, then label.
        auto asm1 = ctx.asmCommands->asmZero(boolReg);     // @0x236c28
        auto asm2 = ctx.asmCommands->brgz(tempReg, label, false);  // @0x236c46
        auto asm3 = ctx.asmCommands->asmOne(boolReg);      // @0x236c5d
        auto asm4 = ctx.asmCommands->asmLabel(label);       // @0x236c74

        AsmList::Asm cmds[] = { std::move(asm1), std::move(asm2),
                                std::move(asm3), std::move(asm4) };

        result->assemblers_.insert(
            result->assemblers_.end(),
            addiAsms.begin(), addiAsms.end());
        result->assemblers_.insert(
            result->assemblers_.end(),
            std::make_move_iterator(std::begin(cmds)),
            std::make_move_iterator(std::end(cmds)));

        Value zeroVal(static_cast<int32_t>(0));
        result->setValue(VarType_Var, VarSubType_Stub, zeroVal,
                         static_cast<int>(boolReg));        // @0x236d86-236dce

        return result;
    }

    // ---- Case C: lhs=Var, rhs=Var ------------------------------  @0x235cb2-236389
    if (lhsHas1 && lhsType == VarType_Var &&
        rhsHas1 && rhsType == VarType_Var)
    {
        int tempRegNum = Resources::getRegisterNumber();    // @0x235ce9
        AsmRegister tempReg(tempRegNum);                    // @0x235cf4

        AsmRegister lhsReg = lhsVals.empty() ? AsmRegister(0)
                                              : lhsVals.back().reg_;  // @0x235d05-235fda

        // addi(tempReg, lhsReg, Immediate(0)) — copy lhsReg      @0x235fef-236004
        auto addiAsms = ctx.asmCommands->addi(
            tempReg, lhsReg, Immediate(0));

        AsmRegister rhsReg = rhsVals.empty() ? AsmRegister(0)
                                              : rhsVals.back().reg_;  // @0x236042-236060

        // subr(tempReg, rhsReg) — tempReg = lhsReg - rhsReg      @0x236063-236075
        auto subrAsm = ctx.asmCommands->subr(tempReg, rhsReg);

        std::string label = Resources::newLabel("true");    // @0x236138-23615b
        int boolRegNum = Resources::getRegisterNumber();    // @0x236180
        AsmRegister boolReg(boolRegNum);                    // @0x23618b

        auto asm1 = ctx.asmCommands->asmOne(boolReg);      // @0x2361a2
        auto asm2 = ctx.asmCommands->brgz(tempReg, label, false);  // @0x2361c0
        auto asm3 = ctx.asmCommands->asmZero(boolReg);     // @0x2361d7
        auto asm4 = ctx.asmCommands->asmLabel(label);       // @0x2361ee

        AsmList::Asm cmds[] = { std::move(asm1), std::move(asm2),
                                std::move(asm3), std::move(asm4) };

        // Insert addi results, then subr, then branch commands.
        result->assemblers_.insert(
            result->assemblers_.end(),
            addiAsms.begin(), addiAsms.end());
        result->assemblers_.push_back(std::move(subrAsm));  // @0x236075-2360f9
        result->assemblers_.insert(
            result->assemblers_.end(),
            std::make_move_iterator(std::begin(cmds)),
            std::make_move_iterator(std::end(cmds)));       // @0x2361f3-236209

        Value zeroVal(static_cast<int32_t>(0));
        result->setValue(VarType_Var, VarSubType_Stub, zeroVal,
                         static_cast<int>(boolReg));        // @0x236300-236348

        return result;
    }

    // ---- Case D: lhs=Const/Cvar, rhs=Const/Cvar ----------------  @0x235d17-23670f
    if (lhsHas1 && isConstOrCvar(lhsType) &&
        rhsHas1 && isConstOrCvar(rhsType))
    {
        Value lhsVal = lhsVals.back().value_;               // @0x235d4e-236500
        Value rhsVal = rhsVals.back().value_;                // @0x236517-2365bc
        double lhsDouble = lhsVal.toDouble();               // @0x23650d
        double rhsDouble = rhsVal.toDouble();                // @0x2365c0

        // Determine output VarType via combine(lhsType, lhsType). @0x236653-236673
        // Binary uses only lhsType for both args.
        VarType resultType = combine(lhsType, lhsType);

        if (lhsDouble > rhsDouble) {                        // @0x236646: ucomisd
            Value oneVal(static_cast<int32_t>(1));
            result->setValue(resultType, VarSubType_Stub, oneVal);  // @0x23669d-2366ab
        } else {
            Value zeroVal(static_cast<int32_t>(0));
            result->setValue(resultType, VarSubType_Stub, zeroVal); // @0x2366fc-23670a
        }

        return result;                                       // @0x235e77
    }

    // ---- Error path: incompatible types -------------------------  @0x235d84-235e77
    {
        VarType lvt = (lhsVals.size() <= 1 && !lhsVals.empty())
                          ? lhsVals[0].varType_ : VarType_Unset;   // @0x235d84-235d8f
        VarType rvt = (rhsVals.size() <= 1 && !rhsVals.empty())
                          ? rhsVals[0].varType_ : VarType_Unset;   // @0x235d9e-235dbb

        std::string lhsStr = str(lvt);                      // @0x235d99
        std::string rhsStr = str(rvt);                       // @0x235dcb
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x92), lhsStr, rhsStr);            // @0x235dd0-235dea
        ctx.messages->errorMessage(msg, -1);                 // @0x235df6-235dfe
    }

    return result;                                           // @0x235e77
}

// ============================================================================
// evalLower @0x237440 (6176B, ends @0x238c5f) — evaluates a < b for all VarType combinations.
//
// Algorithm (from binary):
//   1. Create new EvalResults, copy assemblers_ from both lhs and rhs.
//   2. Check both operands have exactly 1 value (otherwise error 0x92).
//   3. Dispatch on (lhsVarType, rhsVarType):
//
//      Case A — lhs=Var, rhs=Const/Cvar:                         @0x2375a0
//        tempReg = addi(getRegNum(), lhsReg, Immediate(1 - rhs.toInt()))
//        label = newLabel("false"), boolReg = getRegNum()
//        asmZero(boolReg) → brgz(tempReg, "false", false) → asmOne(boolReg) → asmLabel("false")
//        Result: tempReg = lhs + 1 - rhs; if > 0 ⇒ lhs ≥ rhs ⇒ NOT less → stays 0
//
//      Case B — lhs=Const/Cvar, rhs=Var:                         @0x237690
//        tempReg = addi(getRegNum(), rhsReg, Immediate(-lhs.toInt()))
//        label = newLabel("true"), boolReg = getRegNum()
//        asmOne(boolReg) → brgz(tempReg, "true", false) → asmZero(boolReg) → asmLabel("true")
//        Result: tempReg = rhs - lhs; if > 0 ⇒ lhs < rhs → 1
//
//      Case C — lhs=Var, rhs=Var:                                 @0x237760
//        tempReg = addi(getRegNum(), rhsReg, Immediate(0)) [copy rhs]
//        subr(tempReg, lhsReg) [tempReg = rhs - lhs]
//        label = newLabel("true"), boolReg = getRegNum()
//        asmOne(boolReg) → brgz(tempReg, "true", false) → asmZero(boolReg) → asmLabel("true")
//        Result: tempReg = rhs - lhs; if > 0 ⇒ lhs < rhs → 1
//
//      Case D — lhs=Const/Cvar, rhs=Const/Cvar:                  @0x23778e
//        lhsDouble = lhs.toDouble(), rhsDouble = rhs.toDouble()
//        resultType = combine(lhsType, lhsType)
//        ucomisd: rhs > lhs ? setValue(1) : setValue(0)           @0x2380c2
//        (Uses rhsDouble > lhsDouble, which is equivalent to lhs < rhs.)
//
//   4. Error: format(0x92, str(lhsVarType), str(rhsVarType)),
//             errorMessage(msg, -1).
//
// Key differences from evalGreater:
//   Case A: evalGreater uses Immediate(-rhs.toInt()), label "true",
//           asmOne→brgz→asmZero→asmLabel.
//           evalLower uses Immediate(1-rhs.toInt()), label "false",
//           asmZero→brgz→asmOne→asmLabel.
//
//   Case B: evalGreater uses Immediate(1-lhs.toInt()), label "false",
//           asmZero→brgz→asmOne→asmLabel.
//           evalLower uses Immediate(-lhs.toInt()), label "true",
//           asmOne→brgz→asmZero→asmLabel.
//           (Exact mirror of evalGreater Case A.)
//
//   Case C: Identical to evalGreater Case C — same subtraction order,
//           same branch, same label name "true".
//
//   Case D: evalGreater tests lhsDouble > rhsDouble.
//           evalLower tests rhsDouble > lhsDouble (reversed operand order
//           in ucomisd).
// ============================================================================

std::shared_ptr<EvalResults> evalLower(                     // @0x237440
    EvalResults const& lhs,
    EvalResults const& rhs,
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx)
{
    // 1. Create result, copy assemblers from both operands.        @0x237461-23751f
    auto result = std::make_shared<EvalResults>();
    result->assemblers_.insert(
        result->assemblers_.end(),
        lhs.assemblers_.begin(), lhs.assemblers_.end());
    result->assemblers_.insert(
        result->assemblers_.end(),
        rhs.assemblers_.begin(), rhs.assemblers_.end());

    // 2. Extract operand info.                                     @0x237524-237537
    auto const& lhsVals = lhs.values_;
    auto const& rhsVals = rhs.values_;

    bool lhsHas1 = (lhsVals.size() == 1);
    bool rhsHas1 = (rhsVals.size() == 1);

    VarType lhsType = lhsHas1 ? lhsVals[0].varType_ : VarType_Unset;
    VarType rhsType = rhsHas1 ? rhsVals[0].varType_ : VarType_Unset;

    auto isConstOrCvar = [](VarType t) -> bool {
        return (static_cast<int>(t) | 2) == 6;
    };

    // ---- Case A: lhs=Var, rhs=Const/Cvar -----------------------  @0x2375a0
    if (lhsHas1 && lhsType == VarType_Var &&
        rhsHas1 && isConstOrCvar(rhsType))
    {
        int tempRegNum = Resources::getRegisterNumber();    // @0x2375a0 (call @0x237690 is Case B)
        AsmRegister tempReg(tempRegNum);

        // Label "false" for this case                              @0x2375b7
        std::string label = Resources::newLabel("false");   // @0x2375d8

        int boolRegNum = Resources::getRegisterNumber();    // @0x2375fd
        AsmRegister boolReg(boolRegNum);                    // @0x237608

        AsmRegister lhsReg = lhsVals.empty() ? AsmRegister(0)
                                              : lhsVals.back().reg_;  // @0x237623-237639
        Value rhsVal = rhsVals.empty() ? Value() : rhsVals.back().value_;  // @0x237908-23793a

        // addi(tempReg, lhsReg, Immediate(1 - rhs.toInt()))       @0x238237-238271
        // tempReg = lhs + 1 - rhs; if > 0 ⟹ lhs ≥ rhs ⟹ NOT less
        auto addiAsms = ctx.asmCommands->addi(
            tempReg, lhsReg, Immediate(1 - rhsVal.toInt()));

        auto asm1 = ctx.asmCommands->asmZero(boolReg);     // @0x2382e9
        auto asm2 = ctx.asmCommands->brgz(tempReg, label, false);  // @0x238307
        auto asm3 = ctx.asmCommands->asmOne(boolReg);      // @0x23831e
        auto asm4 = ctx.asmCommands->asmLabel(label);       // @0x238335

        AsmList::Asm cmds[] = { std::move(asm1), std::move(asm2),
                                std::move(asm3), std::move(asm4) };

        result->assemblers_.insert(
            result->assemblers_.end(),
            addiAsms.begin(), addiAsms.end());
        result->assemblers_.insert(
            result->assemblers_.end(),
            std::make_move_iterator(std::begin(cmds)),
            std::make_move_iterator(std::end(cmds)));       // @0x238356

        Value zeroVal(static_cast<int32_t>(0));
        result->setValue(VarType_Var, VarSubType_Stub, zeroVal,
                         static_cast<int>(boolReg));        // @0x2384a5-2384bf

        return result;
    }

    // ---- Case B: lhs=Const/Cvar, rhs=Var -----------------------  @0x237690
    if (lhsHas1 && isConstOrCvar(lhsType) &&
        rhsHas1 && rhsType == VarType_Var)
    {
        int tempRegNum = Resources::getRegisterNumber();    // @0x237690
        AsmRegister tempReg(tempRegNum);                    // @0x23769b

        // Label "true" for this case                               @0x2376a7
        std::string label = Resources::newLabel("true");    // @0x2376c6

        int boolRegNum = Resources::getRegisterNumber();    // @0x2376eb
        AsmRegister boolReg(boolRegNum);                    // @0x2376f6

        AsmRegister rhsReg = rhsVals.empty() ? AsmRegister(0)
                                              : rhsVals.back().reg_;  // @0x237711-237725
        Value lhsVal = lhsVals.empty() ? Value() : lhsVals.back().value_;  // @0x237978-2379ac

        // addi(tempReg, rhsReg, Immediate(-lhs.toInt()))           @0x238590-2385c7
        // tempReg = rhs - lhs; if > 0 ⟹ lhs < rhs → 1
        auto addiAsms = ctx.asmCommands->addi(
            tempReg, rhsReg, Immediate(-lhsVal.toInt()));

        auto asm1 = ctx.asmCommands->asmOne(boolReg);      // @0x23863f
        auto asm2 = ctx.asmCommands->brgz(tempReg, label, false);  // @0x23865d
        auto asm3 = ctx.asmCommands->asmZero(boolReg);     // @0x238674
        auto asm4 = ctx.asmCommands->asmLabel(label);       // @0x23868b

        AsmList::Asm cmds[] = { std::move(asm1), std::move(asm2),
                                std::move(asm3), std::move(asm4) };

        result->assemblers_.insert(
            result->assemblers_.end(),
            addiAsms.begin(), addiAsms.end());
        result->assemblers_.insert(
            result->assemblers_.end(),
            std::make_move_iterator(std::begin(cmds)),
            std::make_move_iterator(std::end(cmds)));       // @0x2386ac

        Value zeroVal(static_cast<int32_t>(0));
        result->setValue(VarType_Var, VarSubType_Stub, zeroVal,
                         static_cast<int>(boolReg));        // @0x2387e5

        return result;
    }

    // ---- Case C: lhs=Var, rhs=Var ------------------------------  @0x237760
    if (lhsHas1 && lhsType == VarType_Var &&
        rhsHas1 && rhsType == VarType_Var)
    {
        int tempRegNum = Resources::getRegisterNumber();    // @0x237760
        AsmRegister tempReg(tempRegNum);                    // @0x23776b

        AsmRegister lhsReg = lhsVals.empty() ? AsmRegister(0)
                                              : lhsVals.back().reg_;  // @0x23777c

        AsmRegister rhsReg = rhsVals.empty() ? AsmRegister(0)
                                              : rhsVals.back().reg_;  // @0x237ab4-237ace

        // addi(tempReg, rhsReg, Immediate(0)) — copy rhsReg       @0x237a44-237a69
        auto addiAsms = ctx.asmCommands->addi(
            tempReg, rhsReg, Immediate(0));

        // subr(tempReg, lhsReg) — tempReg = rhs - lhs             @0x237ace-237ae3
        auto subrAsm = ctx.asmCommands->subr(tempReg, lhsReg);

        // Label "true", same as evalGreater Case C                 @0x237bac-237bcf
        std::string label = Resources::newLabel("true");
        int boolRegNum = Resources::getRegisterNumber();    // @0x237bf4
        AsmRegister boolReg(boolRegNum);                    // @0x237bff

        auto asm1 = ctx.asmCommands->asmOne(boolReg);      // @0x237c16
        auto asm2 = ctx.asmCommands->brgz(tempReg, label, false);  // @0x237c34
        auto asm3 = ctx.asmCommands->asmZero(boolReg);     // @0x237c4b
        auto asm4 = ctx.asmCommands->asmLabel(label);       // @0x237c62

        AsmList::Asm cmds[] = { std::move(asm1), std::move(asm2),
                                std::move(asm3), std::move(asm4) };

        result->assemblers_.insert(
            result->assemblers_.end(),
            addiAsms.begin(), addiAsms.end());
        result->assemblers_.push_back(std::move(subrAsm));
        result->assemblers_.insert(
            result->assemblers_.end(),
            std::make_move_iterator(std::begin(cmds)),
            std::make_move_iterator(std::end(cmds)));       // @0x237c83

        Value zeroVal(static_cast<int32_t>(0));
        result->setValue(VarType_Var, VarSubType_Stub, zeroVal,
                         static_cast<int>(boolReg));        // @0x237dbc

        return result;
    }

    // ---- Case D: lhs=Const/Cvar, rhs=Const/Cvar ----------------  @0x23778e
    if (lhsHas1 && isConstOrCvar(lhsType) &&
        rhsHas1 && isConstOrCvar(rhsType))
    {
        Value lhsVal = lhsVals.back().value_;               // @0x2377c7-237f83
        Value rhsVal = rhsVals.back().value_;                // @0x237f9d-238041
        double lhsDouble = lhsVal.toDouble();               // @0x237f90
        double rhsDouble = rhsVal.toDouble();                // @0x238045

        VarType resultType = combine(lhsType, lhsType);    // @0x23811f / 2381a7

        // Binary: ucomisd xmm0(rhsDouble), [rbp-0x90](lhsDouble)  @0x2380c2
        // jbe → false path. So: rhsDouble > lhsDouble ⟹ true (lhs < rhs).
        if (rhsDouble > lhsDouble) {                        // @0x2380c2-2380d3
            Value oneVal(static_cast<int32_t>(1));
            result->setValue(resultType, VarSubType_Stub, oneVal);  // @0x23812e-238157
        } else {
            Value zeroVal(static_cast<int32_t>(0));
            result->setValue(resultType, VarSubType_Stub, zeroVal); // @0x2381b6-2381df
        }

        return result;
    }

    // ---- Error path: incompatible types -------------------------  @0x2377fa-2378fd
    {
        VarType lvt = (lhsVals.size() <= 1 && !lhsVals.empty())
                          ? lhsVals[0].varType_ : VarType_Unset;
        VarType rvt = (rhsVals.size() <= 1 && !rhsVals.empty())
                          ? rhsVals[0].varType_ : VarType_Unset;

        std::string lhsStr = str(lvt);                      // @0x23780f
        std::string rhsStr = str(rvt);                       // @0x237840
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x92), lhsStr, rhsStr);            // @0x23785f
        ctx.messages->errorMessage(msg, -1);                 // @0x23786e-237873
    }

    return result;                                           // @0x2378ec
}

// ============================================================================
// evalEqual @0x239be0 (7703B) — evaluates a == b for all VarType combinations.
//
// Algorithm (from binary):
//   1. Create new EvalResults, copy assemblers_ from both lhs and rhs.
//   2. Check both operands have exactly 1 value (otherwise error 0x92).
//   3. Dispatch on (lhsVarType, rhsVarType):
//
//      Case A — lhs=Var, rhs=Const/Cvar:
//        Extract rhs value, call rhs.toDouble().
//        Range check (constants from rodata @0x9562b8, @0x9562c0):
//          INT32_MAX (2147483647.0) and UINT32_MAX (4294967295.0).
//          When rhs.toDouble() is in (INT32_MAX, UINT32_MAX), negating
//          toInt() wraps incorrectly for the Immediate encoding, so the
//          binary uses a register-load + subr approach instead.
//        Sub-path 1 (rhs in (INT32_MAX, UINT32_MAX)):
//          addi(tempReg, lhsReg, Immediate(0)) [copy lhs]
//          rhsReg = addi(getRegNum(), AsmRegister(0), Immediate(rhs.toInt()))
//          subr(tempReg, rhsReg) → tempReg = lhs - rhs
//        Sub-path 2 (rhs outside that range):
//          addi(tempReg, lhsReg, Immediate(-rhs.toInt()))
//        Then: brz-based equality check.
//
//      Case B — lhs=Const/Cvar, rhs=Var:
//        addi(tempReg, rhsReg, Immediate(-lhs.toInt()))
//        brz-based equality check.
//
//      Case C — lhs=Var, rhs=Var:
//        addi(tempReg, lhsReg, Immediate(0)) [copy]
//        subr(tempReg, rhsReg) → tempReg = lhs - rhs
//        brz-based equality check.
//
//      Case D — lhs=Const/Cvar, rhs=Const/Cvar:
//        Value::operator== comparison.
//        setValue(VarType_Cvar, Stub, Value(int(equal ? 1 : 0)))
//
//   4. Error: format(0x92, str(lhsVarType), str(rhsVarType)),
//             errorMessage(msg, -1).
//
// Differences from evalGreater:
//   - Uses brz (branch if zero) instead of brgz for Var cases.
//   - Case A has a range check + two sub-paths (not present in evalGreater).
//   - Case D uses Value::operator== instead of toDouble() comparison.
//   - Case D hardcodes VarType_Cvar (not combine()) for result type.
//   - shared_ptr<Resources> parameter is unused (static methods only).
// ============================================================================

std::shared_ptr<EvalResults> evalEqual(                     // @0x239be0
    std::shared_ptr<Resources> /*res*/,
    FrontendLoweringContext& ctx,
    EvalResults const& lhs,
    EvalResults const& rhs)
{
    // 1. Create result, copy assemblers from both operands.        @0x239c01-239cbf
    auto result = std::make_shared<EvalResults>();
    result->assemblers_.insert(
        result->assemblers_.end(),
        lhs.assemblers_.begin(), lhs.assemblers_.end());
    result->assemblers_.insert(
        result->assemblers_.end(),
        rhs.assemblers_.begin(), rhs.assemblers_.end());

    // 2. Extract operand info.                                     @0x239cbf-239cd4
    auto const& lhsVals = lhs.values_;
    auto const& rhsVals = rhs.values_;

    bool lhsHas1 = (lhsVals.size() == 1);
    bool rhsHas1 = (rhsVals.size() == 1);

    VarType lhsType = lhsHas1 ? lhsVals[0].varType_ : VarType_Unset;
    VarType rhsType = rhsHas1 ? rhsVals[0].varType_ : VarType_Unset;

    auto isConstOrCvar = [](VarType t) -> bool {
        return (static_cast<int>(t) | 2) == 6;  // matches Const(4) or Cvar(6)
    };

    // ---- Case A: lhs=Var, rhs=Const/Cvar -----------------------  @0x239cf2-239d26
    if (lhsHas1 && lhsType == VarType_Var &&
        rhsHas1 && isConstOrCvar(rhsType))
    {
        int tempRegNum = Resources::getRegisterNumber();    // @0x23a6eb
        AsmRegister tempReg(tempRegNum);

        AsmRegister lhsReg = lhsVals.empty() ? AsmRegister(0)
                                              : lhsVals.back().reg_;
        Value rhsVal = rhsVals.empty() ? Value() : rhsVals.back().value_;

        double rhsDouble = rhsVal.toDouble();               // @0x23a6ac

        // Range check from rodata: constants at 0x9562b8 = 2147483647.0 (INT32_MAX),
        //                                       0x9562c0 = 4294967295.0 (UINT32_MAX).
        // When rhs is in (INT32_MAX, UINT32_MAX), -toInt() wraps incorrectly,
        // so use register-load + subr instead of immediate subtraction.  @0x23a717-23a72d
        static constexpr double kInt32MaxAsDouble = 2147483647.0;    // INT32_MAX
        static constexpr double kUint32MaxAsDouble = 4294967295.0;    // UINT32_MAX

        if (rhsDouble > kInt32MaxAsDouble && kUint32MaxAsDouble > rhsDouble) { // @0x23a72f
            // Sub-path 1: register-based subtraction.
            // addi(tempReg, lhsReg, Immediate(0)) — copy lhs.        @0x23a7f0-23a818
            auto addiAsms = ctx.asmCommands->addi(
                tempReg, lhsReg, Immediate(0));
            result->assemblers_.insert(
                result->assemblers_.end(),
                addiAsms.begin(), addiAsms.end());

            // Allocate register, load rhs constant into it.           @0x23a92c-23a948
            int rhsRegNum = Resources::getRegisterNumber();
            AsmRegister rhsConstReg(rhsRegNum);

            // Re-extract rhs value for toInt().                       @0x23a94d-23b15d
            Value rhsVal2 = rhsVals.empty() ? Value() : rhsVals.back().value_;
            auto addiAsms2 = ctx.asmCommands->addi(
                rhsConstReg, AsmRegister(0), Immediate(rhsVal2.toInt()));
            result->assemblers_.insert(
                result->assemblers_.end(),
                addiAsms2.begin(), addiAsms2.end());

            // subr(tempReg, rhsConstReg) → tempReg = lhs - rhs.      @0x23b267-23b299
            // NOTE: binary allocates a 3rd register for the subr src operand
            // (@0x23b272); analysis suggests this may be the same logical register
            // as rhsConstReg due to compiler register reuse.
            auto subrAsm = ctx.asmCommands->subr(tempReg, rhsConstReg);
            result->assemblers_.push_back(std::move(subrAsm));
        } else {
            // Sub-path 2: immediate-based subtraction.                @0x23afad-23afe6
            auto addiAsms = ctx.asmCommands->addi(
                tempReg, lhsReg, Immediate(-rhsVal.toInt()));
            result->assemblers_.insert(
                result->assemblers_.end(),
                addiAsms.begin(), addiAsms.end());
        }

        // Common: brz-based equality check.                           @0x23b359-23b576
        std::string label = Resources::newLabel("true");
        int boolRegNum = Resources::getRegisterNumber();
        AsmRegister boolReg(boolRegNum);

        auto asm1 = ctx.asmCommands->asmOne(boolReg);
        auto asm2 = ctx.asmCommands->brz(tempReg, label, false);
        auto asm3 = ctx.asmCommands->asmZero(boolReg);
        auto asm4 = ctx.asmCommands->asmLabel(label);

        AsmList::Asm cmds[] = { std::move(asm1), std::move(asm2),
                                std::move(asm3), std::move(asm4) };
        result->assemblers_.insert(
            result->assemblers_.end(),
            std::make_move_iterator(std::begin(cmds)),
            std::make_move_iterator(std::end(cmds)));

        Value zeroVal(static_cast<int32_t>(0));
        result->setValue(VarType_Var, VarSubType_Stub, zeroVal,
                         static_cast<int>(boolReg));

        return result;
    }

    // ---- Case B: lhs=Const/Cvar, rhs=Var -----------------------  @0x239da0-23aeab
    if (lhsHas1 && isConstOrCvar(lhsType) &&
        rhsHas1 && rhsType == VarType_Var)
    {
        int tempRegNum = Resources::getRegisterNumber();    // @0x239da6
        AsmRegister tempReg(tempRegNum);

        AsmRegister rhsReg = rhsVals.empty() ? AsmRegister(0)
                                              : rhsVals.back().reg_;
        Value lhsVal = lhsVals.empty() ? Value() : lhsVals.back().value_;

        // addi(tempReg, rhsReg, Immediate(-lhs.toInt()))              @0x23abfb-23ac23
        auto addiAsms = ctx.asmCommands->addi(
            tempReg, rhsReg, Immediate(-lhsVal.toInt()));
        result->assemblers_.insert(
            result->assemblers_.end(),
            addiAsms.begin(), addiAsms.end());

        // brz-based equality check.                                   @0x23ac89-23aeab
        std::string label = Resources::newLabel("true");
        int boolRegNum = Resources::getRegisterNumber();
        AsmRegister boolReg(boolRegNum);

        auto asm1 = ctx.asmCommands->asmOne(boolReg);
        auto asm2 = ctx.asmCommands->brz(tempReg, label, false);
        auto asm3 = ctx.asmCommands->asmZero(boolReg);
        auto asm4 = ctx.asmCommands->asmLabel(label);

        AsmList::Asm cmds[] = { std::move(asm1), std::move(asm2),
                                std::move(asm3), std::move(asm4) };
        result->assemblers_.insert(
            result->assemblers_.end(),
            std::make_move_iterator(std::begin(cmds)),
            std::make_move_iterator(std::end(cmds)));

        Value zeroVal(static_cast<int32_t>(0));
        result->setValue(VarType_Var, VarSubType_Stub, zeroVal,
                         static_cast<int>(boolReg));

        return result;
    }

    // ---- Case C: lhs=Var, rhs=Var ------------------------------  @0x239de8-23a431
    if (lhsHas1 && lhsType == VarType_Var &&
        rhsHas1 && rhsType == VarType_Var)
    {
        int tempRegNum = Resources::getRegisterNumber();    // @0x239e16
        AsmRegister tempReg(tempRegNum);

        AsmRegister lhsReg = lhsVals.empty() ? AsmRegister(0)
                                              : lhsVals.back().reg_;

        // addi(tempReg, lhsReg, Immediate(0)) — copy lhsReg.        @0x23a0b6-23a0db
        auto addiAsms = ctx.asmCommands->addi(
            tempReg, lhsReg, Immediate(0));

        AsmRegister rhsReg = rhsVals.empty() ? AsmRegister(0)
                                              : rhsVals.back().reg_;

        // subr(tempReg, rhsReg) — tempReg = lhsReg - rhsReg.        @0x23a13e-23a14f
        auto subrAsm = ctx.asmCommands->subr(tempReg, rhsReg);

        result->assemblers_.insert(
            result->assemblers_.end(),
            addiAsms.begin(), addiAsms.end());
        result->assemblers_.push_back(std::move(subrAsm));

        // brz-based equality check.                                   @0x23a20f-23a431
        std::string label = Resources::newLabel("true");
        int boolRegNum = Resources::getRegisterNumber();
        AsmRegister boolReg(boolRegNum);

        auto asm1 = ctx.asmCommands->asmOne(boolReg);
        auto asm2 = ctx.asmCommands->brz(tempReg, label, false);
        auto asm3 = ctx.asmCommands->asmZero(boolReg);
        auto asm4 = ctx.asmCommands->asmLabel(label);

        AsmList::Asm cmds[] = { std::move(asm1), std::move(asm2),
                                std::move(asm3), std::move(asm4) };
        result->assemblers_.insert(
            result->assemblers_.end(),
            std::make_move_iterator(std::begin(cmds)),
            std::make_move_iterator(std::end(cmds)));

        Value zeroVal(static_cast<int32_t>(0));
        result->setValue(VarType_Var, VarSubType_Stub, zeroVal,
                         static_cast<int>(boolReg));

        return result;
    }

    // ---- Case D: lhs=Const/Cvar, rhs=Const/Cvar ----------------  @0x239e4f-23aae2
    if (lhsHas1 && isConstOrCvar(lhsType) &&
        rhsHas1 && isConstOrCvar(rhsType))
    {
        Value lhsVal = lhsVals.back().value_;
        Value rhsVal = rhsVals.back().value_;

        // Uses Value::operator== for comparison.                      @0x23aa05
        // Unlike evalGreater which used toDouble(), equality uses the
        // Value-native comparison (handles string equality etc.).
        bool equal = (lhsVal == rhsVal);

        // Binary hardcodes VarType_Cvar for result (no combine() call).  @0x23aa6a-23aadd
        if (equal) {
            Value oneVal(static_cast<int32_t>(1));
            result->setValue(VarType_Cvar, VarSubType_Stub, oneVal);
        } else {
            Value zeroVal(static_cast<int32_t>(0));
            result->setValue(VarType_Cvar, VarSubType_Stub, zeroVal);
        }

        return result;
    }

    // ---- Error path: incompatible types -------------------------  @0x239ec3-239fbb
    {
        VarType lvt = (lhsVals.size() <= 1 && !lhsVals.empty())
                          ? lhsVals[0].varType_ : VarType_Unset;
        VarType rvt = (rhsVals.size() <= 1 && !rhsVals.empty())
                          ? rhsVals[0].varType_ : VarType_Unset;

        std::string lhsStr = str(lvt);
        std::string rhsStr = str(rvt);
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x92), lhsStr, rhsStr);
        ctx.messages->errorMessage(msg, -1);
    }

    return result;
}

// ============================================================================
// evalShift @0x232850 (11838B) — evaluates shift (left or right) for all
// VarType combinations.
//
// Algorithm (from binary):
//   isRight: false = <<, true = >>.
//
//   1. Create new EvalResults, copy assemblers_ from LHS only (NOT rhs).
//   2. Check both operands have exactly 1 value (otherwise error 0xD4).
//   3. Dispatch on (lhsVarType, rhsVarType):
//
//      Path 1 — Both Const/Cvar (compile-time shift):
//        lhsInt = lhs.toInt(), rhsInt = rhs.toInt()
//        effectiveRight = isRight XOR (rhsInt < 0) — negative reverses
//        shiftAmt = abs(rhsInt) & 0x1f
//        result = effectiveRight ? (lhsInt >> shiftAmt) : (lhsInt << shiftAmt)
//        setValue(combine(lhsType,rhsType), combine(lhsSub,rhsSub), Value(result))
//
//      Path 2a — At least one Var, rhs=Const/Cvar (known shift amount):
//        Copy lhs assemblers to result (already done at step 1).
//        setValue(Var, combine(lhsSub,rhsSub), Value(0), getRegisterNumber())
//        If lhs is Var: lhsReg = lhs.back().reg_
//        If lhs is Const/Cvar: allocate register, emit addi(lhsReg, R0, lhsValue)
//        Emit addi(backReg, lhsReg, Immediate(0)) — copy lhs to result register
//        Compute effectiveRight, shiftAmt as in Path 1.
//        If shiftAmt == 0: return (copy suffices).
//        Else: emit shiftAmt copies of ssl(resultReg) or ssr(resultReg).
//
//      Path 2b — At least one Var, rhs=Var (runtime shift amount):
//        Same initial setup as 2a (copy assemblers, setValue, determine lhsReg).
//        rhsReg = rhs.back().reg_
//        Emit addi(backReg, lhsReg, Immediate(0)) — copy lhs to result register
//        Create labels: shiftLabel = newLabel("shift"), endLabel = newLabel("end")
//        Emit: brgz(rhsReg, shiftLabel, false) — if rhs > 0, jump to positive
//        Emit: brz(rhsReg, endLabel, false) — if rhs == 0, skip
//        Negative rhs block:
//          tempReg = getRegisterNumber()
//          asmZero(tempReg), subr(tempReg, rhsReg) → tempReg = -rhs
//          andi(tempReg, tempReg, Immediate(0x1f)) → mask to 5 bits
//          iloopLabel = newLabel("iloop"), asmLabel(iloopLabel)
//          isRight ? ssl(resultReg) : ssr(resultReg) — REVERSED direction
//          addi(tempReg, tempReg, Immediate(-1)), brgz(tempReg, iloopLabel, false)
//          brz(R0, endLabel, false) — unconditional jump to end
//        Positive rhs block:
//          asmLabel(shiftLabel)
//          andi(tempReg, rhsReg, Immediate(0x1f)) → mask rhs to 5 bits
//          loopLabel = newLabel("loop"), asmLabel(loopLabel)
//          isRight ? ssr(resultReg) : ssl(resultReg)
//          addi(tempReg, tempReg, Immediate(-1)), brgz(tempReg, loopLabel, false)
//        asmLabel(endLabel)
//
//      Path 3 — Error: format(0xD4, str(lhsVarType), str(rhsVarType)),
//               errorMessage(msg, -1).
//
// Note: shared_ptr<Resources> parameter is unused — getRegisterNumber()
// and newLabel() are static.
// ============================================================================

std::shared_ptr<EvalResults> evalShift(                     // @0x232850
    EvalResults const& lhs,
    EvalResults const& rhs,
    std::shared_ptr<Resources> /*res*/,
    bool isRight,
    FrontendLoweringContext& ctx)
{
    // 1. Create result, copy assemblers from LHS only.             @0x232874-23290f
    auto result = std::make_shared<EvalResults>();
    result->assemblers_.insert(
        result->assemblers_.end(),
        lhs.assemblers_.begin(), lhs.assemblers_.end());

    // 2. Extract operand info.                                     @0x232914-232928
    auto const& lhsVals = lhs.values_;
    auto const& rhsVals = rhs.values_;

    bool lhsHas1 = (lhsVals.size() == 1);
    bool rhsHas1 = (rhsVals.size() == 1);

    VarType    lhsType    = lhsHas1 ? lhsVals[0].varType_    : VarType_Unset;
    VarSubType lhsSubType = lhsHas1 ? lhsVals[0].varSubType_ : VarSubType_Default;
    VarType    rhsType    = rhsHas1 ? rhsVals[0].varType_    : VarType_Unset;
    VarSubType rhsSubType = rhsHas1 ? rhsVals[0].varSubType_ : VarSubType_Default;

    auto isConstOrCvar = [](VarType t) -> bool {
        return (static_cast<int>(t) | 2) == 6;  // matches Const(4) or Cvar(6)
    };

    auto isVarOrConstOrCvar = [](VarType t) -> bool {
        // bt 0x54, t — bits set at positions 2(Var), 4(Const), 6(Cvar)
        return t == VarType_Var || t == VarType_Const || t == VarType_Cvar;
    };

    // ---- Path 1: Both Const/Cvar (compile-time shift) ----------  @0x232960-232b75
    if (lhsHas1 && isConstOrCvar(lhsType) &&
        rhsHas1 && isConstOrCvar(rhsType))
    {
        int lhsInt = lhsVals.back().value_.toInt();         // @0x232976-232a1e
        int rhsInt = rhsVals.back().value_.toInt();         // @0x232a28-232acc

        // effectiveRight = isRight XOR (rhsInt < 0) — negative reverses direction
        bool effectiveRight = isRight ^ (rhsInt < 0);       // @0x232ad1-232ade
        int shiftAmt = std::abs(rhsInt) & 0x1f;            // @0x232ae0-232ae8

        int shifted;
        if (effectiveRight) {                                // @0x232aeb
            shifted = lhsInt >> shiftAmt;                    // @0x232af0 (arithmetic)
        } else {
            shifted = lhsInt << shiftAmt;                    // @0x232af6
        }

        VarType    resultType    = combine(lhsType, rhsType);     // @0x232b00
        VarSubType resultSubType = combine(lhsSubType, rhsSubType); // @0x232b12

        Value shiftedVal(static_cast<int32_t>(shifted));     // @0x232b25
        result->setValue(resultType, resultSubType, shiftedVal); // @0x232b35-232b4f

        return result;                                       // @0x232b75
    }

    // ---- Path 2: At least one Var --------------------------------
    // Guard: both operands must be Var/Const/Cvar.
    if (lhsHas1 && isVarOrConstOrCvar(lhsType) &&
        rhsHas1 && isVarOrConstOrCvar(rhsType))
    {
        VarSubType combinedSubType = combine(lhsSubType, rhsSubType); // @0x232b90

        // setValue(Var, combinedSubType, Value(0), getRegisterNumber())  @0x232bb0-232c2f
        int resultRegNum = Resources::getRegisterNumber();
        Value zeroVal(static_cast<int32_t>(0));
        result->setValue(VarType_Var, combinedSubType, zeroVal, resultRegNum);

        // Determine lhsReg.
        // If lhs is Var: use lhs.back().reg_                       @0x232c40-232c90
        // If lhs is Const/Cvar: allocate register, emit addi(reg, R0, lhsValue) @0x232c98-232e47
        AsmRegister lhsReg(0);
        if (lhsType == VarType_Var) {
            lhsReg = lhsVals.back().reg_;                   // @0x232c70
        } else {
            // Const/Cvar: load into a new register
            int loadRegNum = Resources::getRegisterNumber(); // @0x232ca8
            lhsReg = AsmRegister(loadRegNum);
            Value lhsVal = lhsVals.back().value_;            // @0x232cb0-232d4c

            // addi(lhsReg, AsmRegister(0), lhsValue) — the Value overload @0x27a020
            auto loadAsms = ctx.asmCommands->addi(
                lhsReg, AsmRegister(0), lhsVal);            // @0x232d5e-232e08
            result->assemblers_.insert(
                result->assemblers_.end(),
                loadAsms.begin(), loadAsms.end());           // @0x232e0c-232e47
        }

        // Get result register from back of values_ (just set by setValue).
        AsmRegister backReg = result->values_.back().reg_;   // @0x232e50

        // ---- Path 2a: rhs=Const/Cvar (known shift amount) ------  @0x232e60-2335e0
        if (isConstOrCvar(rhsType)) {
            // Emit addi(backReg, lhsReg, Immediate(0)) — copy lhs to result register
            auto copyAsms = ctx.asmCommands->addi(
                backReg, lhsReg, Immediate(0));              // @0x232e70-232ebc
            result->assemblers_.insert(
                result->assemblers_.end(),
                copyAsms.begin(), copyAsms.end());           // @0x232ec0-232f0a

            int rhsInt = rhsVals.back().value_.toInt();      // @0x232f14-232fba

            // effectiveRight = isRight XOR (rhsInt < 0)
            bool effectiveRight = isRight ^ (rhsInt < 0);    // @0x232fbf-232fcc
            int shiftAmt = std::abs(rhsInt) & 0x1f;         // @0x232fce-232fd6

            if (shiftAmt == 0) {
                return result;                               // @0x232fda — copy was sufficient
            }

            // Emit shiftAmt copies of ssl or ssr.                  @0x232fe0-233580
            for (int i = 0; i < shiftAmt; ++i) {
                if (effectiveRight) {
                    auto ssrAsm = ctx.asmCommands->ssr(backReg);  // @0x233020
                    result->assemblers_.push_back(std::move(ssrAsm));
                } else {
                    auto sslAsm = ctx.asmCommands->ssl(backReg);  // @0x233040
                    result->assemblers_.push_back(std::move(sslAsm));
                }
            }

            return result;                                   // @0x2335e0
        }

        // ---- Path 2b: rhs=Var (runtime shift amount) -----------  @0x2335e0-234e70
        {
            AsmRegister rhsReg = rhsVals.back().reg_;        // @0x2335f0

            // Emit addi(backReg, lhsReg, Immediate(0)) — copy lhs to result register
            auto copyAsms = ctx.asmCommands->addi(
                backReg, lhsReg, Immediate(0));              // @0x233600-23364c
            result->assemblers_.insert(
                result->assemblers_.end(),
                copyAsms.begin(), copyAsms.end());           // @0x233650-23369a

            // Create labels for control flow.
            std::string shiftLabel = Resources::newLabel("shift"); // @0x2336a0-2336c3
            std::string endLabel   = Resources::newLabel("end");   // @0x2336d8-2336fb

            // brgz(rhsReg, shiftLabel, false) — if rhs > 0, jump to positive path
            auto brgzAsm = ctx.asmCommands->brgz(
                rhsReg, shiftLabel, false);                  // @0x233700-233720
            result->assemblers_.push_back(std::move(brgzAsm));

            // brz(rhsReg, endLabel, false) — if rhs == 0, skip entirely
            auto brzAsm = ctx.asmCommands->brz(
                rhsReg, endLabel, false);                    // @0x233730-233750
            result->assemblers_.push_back(std::move(brzAsm));

            // ---- Negative rhs block (falls through when rhs < 0)  @0x233760-234100
            {
                int tempRegNum = Resources::getRegisterNumber(); // @0x233768
                AsmRegister tempReg(tempRegNum);

                // asmZero(tempReg), subr(tempReg, rhsReg) → tempReg = -rhs
                auto zeroAsm = ctx.asmCommands->asmZero(tempReg); // @0x233780
                auto subrAsm = ctx.asmCommands->subr(tempReg, rhsReg); // @0x233798
                result->assemblers_.push_back(std::move(zeroAsm));
                result->assemblers_.push_back(std::move(subrAsm));

                // andi(tempReg, tempReg, Immediate(0x1f)) → mask to 5 bits
                auto andiAsms = ctx.asmCommands->andi(
                    tempReg, tempReg, Immediate(0x1f));      // @0x2337c0-23380c
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    andiAsms.begin(), andiAsms.end());

                // iloopLabel = newLabel("iloop"), asmLabel(iloopLabel)
                std::string iloopLabel = Resources::newLabel("iloop"); // @0x233820-233843
                auto iloopLabelAsm = ctx.asmCommands->asmLabel(iloopLabel); // @0x233850
                result->assemblers_.push_back(std::move(iloopLabelAsm));

                // isRight → ssl (reversed!); !isRight → ssr (reversed!)  @0x233870-233d80
                if (isRight) {
                    auto sslAsm = ctx.asmCommands->ssl(backReg);
                    result->assemblers_.push_back(std::move(sslAsm));
                } else {
                    auto ssrAsm = ctx.asmCommands->ssr(backReg);
                    result->assemblers_.push_back(std::move(ssrAsm));
                }

                // addi(tempReg, tempReg, Immediate(-1))
                auto decAsms = ctx.asmCommands->addi(
                    tempReg, tempReg, Immediate(-1));        // @0x233d90-233ddc
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    decAsms.begin(), decAsms.end());

                // brgz(tempReg, iloopLabel, false) — loop while tempReg > 0
                auto loopBrgzAsm = ctx.asmCommands->brgz(
                    tempReg, iloopLabel, false);             // @0x233df0-233e10
                result->assemblers_.push_back(std::move(loopBrgzAsm));

                // brz(AsmRegister(0), endLabel, false) — unconditional jump to end
                auto uncondBrzAsm = ctx.asmCommands->brz(
                    AsmRegister(0), endLabel, false);        // @0x233e20-233e40
                result->assemblers_.push_back(std::move(uncondBrzAsm));
            }

            // ---- Positive rhs block --------------------------------  @0x234100-234e40
            {
                int tempRegNum = Resources::getRegisterNumber(); // @0x234108
                AsmRegister tempReg(tempRegNum);

                // asmLabel(shiftLabel)
                auto shiftLabelAsm = ctx.asmCommands->asmLabel(shiftLabel); // @0x234120
                result->assemblers_.push_back(std::move(shiftLabelAsm));

                // andi(tempReg, rhsReg, Immediate(0x1f)) → mask rhs to 5 bits
                auto andiAsms = ctx.asmCommands->andi(
                    tempReg, rhsReg, Immediate(0x1f));       // @0x234140-23418c
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    andiAsms.begin(), andiAsms.end());

                // loopLabel = newLabel("loop"), asmLabel(loopLabel)
                std::string loopLabel = Resources::newLabel("loop"); // @0x2341a0-2341c3
                auto loopLabelAsm = ctx.asmCommands->asmLabel(loopLabel); // @0x2341d0
                result->assemblers_.push_back(std::move(loopLabelAsm));

                // isRight → ssr(resultReg); !isRight → ssl(resultReg)  @0x2341f0-234780
                if (isRight) {
                    auto ssrAsm = ctx.asmCommands->ssr(backReg);
                    result->assemblers_.push_back(std::move(ssrAsm));
                } else {
                    auto sslAsm = ctx.asmCommands->ssl(backReg);
                    result->assemblers_.push_back(std::move(sslAsm));
                }

                // addi(tempReg, tempReg, Immediate(-1))
                auto decAsms = ctx.asmCommands->addi(
                    tempReg, tempReg, Immediate(-1));        // @0x234790-2347dc
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    decAsms.begin(), decAsms.end());

                // brgz(tempReg, loopLabel, false) — loop while tempReg > 0
                auto loopBrgzAsm = ctx.asmCommands->brgz(
                    tempReg, loopLabel, false);              // @0x2347f0-234810
                result->assemblers_.push_back(std::move(loopBrgzAsm));
            }

            // asmLabel(endLabel)                                    @0x234e40-234e60
            auto endLabelAsm = ctx.asmCommands->asmLabel(endLabel);
            result->assemblers_.push_back(std::move(endLabelAsm));

            return result;                                   // @0x234e70
        }
    }

    // ---- Path 3: Error — incompatible types ---------------------  @0x234e70-234f60
    {
        VarType lvt = (lhsVals.size() <= 1 && !lhsVals.empty())
                          ? lhsVals[0].varType_ : VarType_Unset;
        VarType rvt = (rhsVals.size() <= 1 && !rhsVals.empty())
                          ? rhsVals[0].varType_ : VarType_Unset;

        std::string lhsStr = str(lvt);
        std::string rhsStr = str(rvt);
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0xD4), lhsStr, rhsStr);           // @0x234ec0-234ee0
        ctx.messages->errorMessage(msg, -1);                 // @0x234ef0
    }

    return result;
}

// ============================================================================
// evalAnd @0x23ea20 (7674B) — evaluates bitwise a & b for all VarType
// combinations, with boolean short-circuit for Stub-subtype operands.
//
// Algorithm (from binary):
//   1. Create new EvalResults (assemblers NOT copied upfront — each path
//      copies only the operand(s) it needs).
//   2. Check both operands have exactly 1 value; dispatch on
//      (lhsVarType, rhsVarType):
//
//      Case A — lhs=Var, rhs=Const/Cvar:
//        A1 (rhs.subType==Stub, boolean): toBool(rhs) →
//            true  → valueToBool(copy(lhs)), copy its assemblers, use its reg
//            false → short-circuit: setValue(rhsType, Stub, rhsValue)
//        A2 (rhs.subType!=Stub, numeric): copy lhs assemblers,
//            andi(resultReg, lhsReg, rhsValue)
//
//      Case B — lhs=Const/Cvar, rhs=Var:
//        Mirror of Case A with lhs/rhs swapped.
//
//      Case C — lhs=Var, rhs=Var:
//        Copy both assemblers, addi(resultReg, lhsReg, 0), andr(resultReg, rhsReg)
//
//      Case D — lhs=Const/Cvar, rhs=Const/Cvar:
//        Compile-time: lhsInt & rhsInt.
//        Warning 0xFD if (lhsInt | rhsInt) < 0 (negative operand).
//
//      Case E — Error: format(0x8F, str(lhsVarType), str(rhsVarType)),
//               errorMessage(msg, -1).
//
// Note: shared_ptr<Resources> parameter is unused in the body —
// getRegisterNumber() is static.  Passed through to valueToBool.
// ============================================================================

std::shared_ptr<EvalResults> evalAnd(                       // @0x23ea20
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    EvalResults const& lhs,
    EvalResults const& rhs)
{
    // 1. Create result (no upfront assembler copy).                @0x23ea40-23ea80
    auto result = std::make_shared<EvalResults>();

    // 2. Extract operand info.                                     @0x23ea85-23ea99
    auto const& lhsVals = lhs.values_;
    auto const& rhsVals = rhs.values_;

    bool lhsHas1 = (lhsVals.size() == 1);
    bool rhsHas1 = (rhsVals.size() == 1);

    VarType    lhsType    = lhsHas1 ? lhsVals[0].varType_    : VarType_Unset;
    VarSubType lhsSubType = lhsHas1 ? lhsVals[0].varSubType_ : VarSubType_Default;
    VarType    rhsType    = rhsHas1 ? rhsVals[0].varType_    : VarType_Unset;
    VarSubType rhsSubType = rhsHas1 ? rhsVals[0].varSubType_ : VarSubType_Default;

    auto isConstOrCvar = [](VarType t) -> bool {
        return (static_cast<int>(t) | 2) == 6;  // matches Const(4) or Cvar(6)
    };

    // ---- Case A: lhs=Var, rhs=Const/Cvar ----------------------------
    if (lhsHas1 && lhsType == VarType_Var &&
        rhsHas1 && isConstOrCvar(rhsType))
    {
        // ---- A1: rhs.subType==Stub (boolean rhs) --------------------  @0x23eb10
        if (rhsSubType == VarSubType_Stub) {
            bool rhsBool = rhsVals.back().value_.toBool();   // @0x23eb20-23eb60
            if (rhsBool) {
                // true AND x  →  valueToBool(x)                @0x23eb70-23ec80
                auto lhsBool = valueToBool(
                    std::make_shared<EvalResults>(lhs), res, ctx);
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    lhsBool->assemblers_.begin(),
                    lhsBool->assemblers_.end());
                VarSubType combinedSub = combine(lhsSubType, rhsSubType);
                result->setValue(VarType_Var, combinedSub,
                    Value(static_cast<int32_t>(0)),
                    static_cast<int>(lhsBool->values_.back().reg_));
            } else {
                // false AND x  →  false (short-circuit)        @0x23ecc0-23ed00
                result->setValue(rhsType, VarSubType_Stub,
                    rhsVals.back().value_);
            }
            return result;
        }

        // ---- A2: rhs.subType!=Stub (numeric rhs) --------------------  @0x23ed10-23eea0
        // Copy lhs assemblers only.
        result->assemblers_.insert(
            result->assemblers_.end(),
            lhs.assemblers_.begin(), lhs.assemblers_.end());

        VarSubType combinedSub = combine(lhsSubType, rhsSubType); // @0x23ed40
        int resultRegNum = Resources::getRegisterNumber();         // @0x23ed50
        result->setValue(VarType_Var, combinedSub,
            Value(static_cast<int32_t>(0)), resultRegNum);

        AsmRegister resultReg = result->values_.back().reg_;
        AsmRegister lhsReg = lhsVals.back().reg_;
        Value rhsValue = rhsVals.back().value_;                    // @0x23ede0

        // andi(resultReg, lhsReg, rhsValue) — Value overload @0x27a400
        auto andiAsms = ctx.asmCommands->andi(
            resultReg, lhsReg, rhsValue);                          // @0x23ee10-23ee60
        result->assemblers_.insert(
            result->assemblers_.end(),
            andiAsms.begin(), andiAsms.end());                     // @0x23ee70-23eea0

        return result;
    }

    // ---- Case B: lhs=Const/Cvar, rhs=Var ----------------------------
    if (lhsHas1 && isConstOrCvar(lhsType) &&
        rhsHas1 && rhsType == VarType_Var)
    {
        // ---- B1: lhs.subType==Stub (boolean lhs) --------------------  @0x23eec0
        if (lhsSubType == VarSubType_Stub) {
            bool lhsBool = lhsVals.back().value_.toBool();   // @0x23eed0-23ef10
            if (lhsBool) {
                // true AND x  →  valueToBool(x)                @0x23ef20-23f030
                auto rhsBool = valueToBool(
                    std::make_shared<EvalResults>(rhs), res, ctx);
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    rhsBool->assemblers_.begin(),
                    rhsBool->assemblers_.end());
                VarSubType combinedSub = combine(lhsSubType, rhsSubType);
                result->setValue(VarType_Var, combinedSub,
                    Value(static_cast<int32_t>(0)),
                    static_cast<int>(rhsBool->values_.back().reg_));
            } else {
                // false AND x  →  false (short-circuit)        @0x23f070-23f0b0
                result->setValue(lhsType, VarSubType_Stub,
                    lhsVals.back().value_);
            }
            return result;
        }

        // ---- B2: lhs.subType!=Stub (numeric lhs) --------------------  @0x23f0c0-23f250
        // Copy rhs assemblers only.
        result->assemblers_.insert(
            result->assemblers_.end(),
            rhs.assemblers_.begin(), rhs.assemblers_.end());

        VarSubType combinedSub = combine(lhsSubType, rhsSubType); // @0x23f0f0
        int resultRegNum = Resources::getRegisterNumber();         // @0x23f100
        result->setValue(VarType_Var, combinedSub,
            Value(static_cast<int32_t>(0)), resultRegNum);

        AsmRegister resultReg = result->values_.back().reg_;
        AsmRegister rhsReg = rhsVals.back().reg_;
        Value lhsValue = lhsVals.back().value_;                    // @0x23f190

        // andi(resultReg, rhsReg, lhsValue) — Value overload @0x27a400
        auto andiAsms = ctx.asmCommands->andi(
            resultReg, rhsReg, lhsValue);                          // @0x23f1c0-23f210
        result->assemblers_.insert(
            result->assemblers_.end(),
            andiAsms.begin(), andiAsms.end());                     // @0x23f220-23f250

        return result;
    }

    // ---- Case C: lhs=Var, rhs=Var -----------------------------------  @0x23f260-23f430
    if (lhsHas1 && lhsType == VarType_Var &&
        rhsHas1 && rhsType == VarType_Var)
    {
        // Copy BOTH lhs and rhs assemblers.                        @0x23f270-23f310
        result->assemblers_.insert(
            result->assemblers_.end(),
            lhs.assemblers_.begin(), lhs.assemblers_.end());
        result->assemblers_.insert(
            result->assemblers_.end(),
            rhs.assemblers_.begin(), rhs.assemblers_.end());

        VarSubType combinedSub = combine(lhsSubType, rhsSubType); // @0x23f320
        int resultRegNum = Resources::getRegisterNumber();         // @0x23f330
        result->setValue(VarType_Var, combinedSub,
            Value(static_cast<int32_t>(0)), resultRegNum);

        AsmRegister resultReg = result->values_.back().reg_;
        AsmRegister lhsReg = lhsVals.back().reg_;
        AsmRegister rhsReg = rhsVals.back().reg_;

        // addi(resultReg, lhsReg, Immediate(0)) — copy lhs to result register
        auto copyAsms = ctx.asmCommands->addi(
            resultReg, lhsReg, Immediate(0));                      // @0x23f370-23f3b0
        result->assemblers_.insert(
            result->assemblers_.end(),
            copyAsms.begin(), copyAsms.end());

        // andr(resultReg, rhsReg) — bitwise AND (single Asm)
        auto andrAsm = ctx.asmCommands->andr(resultReg, rhsReg);  // @0x23f3c0-23f3e0
        result->assemblers_.push_back(std::move(andrAsm));         // @0x23f3f0-23f430

        return result;
    }

    // ---- Case D: lhs=Const/Cvar, rhs=Const/Cvar --------------------  @0x23f440-23f5f0
    if (lhsHas1 && isConstOrCvar(lhsType) &&
        rhsHas1 && isConstOrCvar(rhsType))
    {
        int lhsInt = lhsVals.back().value_.toInt();                // @0x23f450-23f4a0
        int rhsInt = rhsVals.back().value_.toInt();                // @0x23f4b0-23f500

        // Warning for negative operand(s): (lhsInt | rhsInt) < 0  @0x23f510
        if ((lhsInt | rhsInt) < 0) {
            std::string lhsStr = std::to_string(lhsInt);
            std::string rhsStr = std::to_string(rhsInt);
            std::string msg = ErrorMessages::format(
                ErrorMessageT(0xFD), lhsStr, rhsStr);             // @0x23f530-23f560
            ctx.messages->errorMessage(msg, -1);                   // @0x23f570
            // continues — not fatal
        }

        VarType    combinedType    = combine(lhsType, rhsType);   // @0x23f580
        VarSubType combinedSubType = combine(lhsSubType, rhsSubType); // @0x23f590

        result->setValue(combinedType, combinedSubType,
            Value(static_cast<int32_t>(lhsInt & rhsInt)));         // @0x23f5a0-23f5d0

        return result;                                              // @0x23f5f0
    }

    // ---- Case E: Error — incompatible types -------------------------  @0x23f600-23f6f0
    {
        VarType lvt = (lhsVals.size() <= 1 && !lhsVals.empty())
                          ? lhsVals[0].varType_ : VarType_Unset;
        VarType rvt = (rhsVals.size() <= 1 && !rhsVals.empty())
                          ? rhsVals[0].varType_ : VarType_Unset;

        std::string lhsStr = str(lvt);
        std::string rhsStr = str(rvt);
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x8F), lhsStr, rhsStr);                 // @0x23f680-23f6a0
        ctx.messages->errorMessage(msg, -1);                       // @0x23f6b0
    }

    return result;
}

// ============================================================================
// evalOr @0x240a30 (7547B) — evaluates bitwise a | b for all VarType
// combinations, with boolean short-circuit for Stub-subtype operands.
//
// Structure is a near-exact mirror of evalAnd with these substitutions:
//   - andr → orr,  andi → ori,  & → |
//   - Boolean short-circuit INVERTED:
//       AND: false&x=false (short-circuit), true&x=valueToBool(x)
//       OR:  true|x=true  (short-circuit), false|x=valueToBool(x)
//   - Type-mismatch error code: 0x8F (AND) → 0x90 (OR)
//   - Negative-operand warning code: 0xFD (same as AND)
//
// See evalAnd comment block above for full dispatch-path descriptions.
// ============================================================================

std::shared_ptr<EvalResults> evalOr(                        // @0x240a30
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    EvalResults const& lhs,
    EvalResults const& rhs)
{
    // 1. Create result (no upfront assembler copy).                @0x240a55-240aa1
    auto result = std::make_shared<EvalResults>();

    // 2. Extract operand info.                                     @0x240abe-240b37
    auto const& lhsVals = lhs.values_;
    auto const& rhsVals = rhs.values_;

    bool lhsHas1 = (lhsVals.size() == 1);
    bool rhsHas1 = (rhsVals.size() == 1);

    VarType    lhsType    = lhsHas1 ? lhsVals[0].varType_    : VarType_Unset;
    VarSubType lhsSubType = lhsHas1 ? lhsVals[0].varSubType_ : VarSubType_Default;
    VarType    rhsType    = rhsHas1 ? rhsVals[0].varType_    : VarType_Unset;
    VarSubType rhsSubType = rhsHas1 ? rhsVals[0].varSubType_ : VarSubType_Default;

    auto isConstOrCvar = [](VarType t) -> bool {
        return (static_cast<int>(t) | 2) == 6;  // matches Const(4) or Cvar(6)
    };

    // ---- Case A: lhs=Var, rhs=Const/Cvar ----------------------------
    if (lhsHas1 && lhsType == VarType_Var &&
        rhsHas1 && isConstOrCvar(rhsType))
    {
        // ---- A1: rhs.subType==Stub (boolean rhs) --------------------  @0x240b45
        if (rhsSubType == VarSubType_Stub) {
            bool rhsBool = rhsVals.back().value_.toBool();   // @0x241d92
            if (rhsBool) {
                // true OR x  →  true (short-circuit)           @0x241dd5
                result->setValue(rhsType, VarSubType_Stub,
                    rhsVals.back().value_);
            } else {
                // false OR x  →  valueToBool(x)                @0x241e2a
                auto lhsBool = valueToBool(
                    std::make_shared<EvalResults>(lhs), res, ctx);
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    lhsBool->assemblers_.begin(),
                    lhsBool->assemblers_.end());
                VarSubType combinedSub = combine(lhsSubType, rhsSubType);
                result->setValue(VarType_Var, combinedSub,
                    Value(static_cast<int32_t>(0)),
                    static_cast<int>(lhsBool->values_.back().reg_));
            }
            return result;
        }

        // ---- A2: rhs.subType!=Stub (numeric rhs) --------------------  @0x2410bc
        // Copy lhs assemblers only.
        result->assemblers_.insert(
            result->assemblers_.end(),
            lhs.assemblers_.begin(), lhs.assemblers_.end());

        VarSubType combinedSub = combine(lhsSubType, rhsSubType); // @0x241154
        int resultRegNum = Resources::getRegisterNumber();
        result->setValue(VarType_Var, combinedSub,
            Value(static_cast<int32_t>(0)), resultRegNum);

        AsmRegister resultReg = result->values_.back().reg_;
        AsmRegister lhsReg = lhsVals.back().reg_;
        Value rhsValue = rhsVals.back().value_;

        // ori(resultReg, lhsReg, rhsValue) — Value overload @0x27a7e0
        auto oriAsms = ctx.asmCommands->ori(
            resultReg, lhsReg, rhsValue);                         // @0x2422e1
        result->assemblers_.insert(
            result->assemblers_.end(),
            oriAsms.begin(), oriAsms.end());

        return result;
    }

    // ---- Case B: lhs=Const/Cvar, rhs=Var ----------------------------
    if (lhsHas1 && isConstOrCvar(lhsType) &&
        rhsHas1 && rhsType == VarType_Var)
    {
        // ---- B1: lhs.subType==Stub (boolean lhs) --------------------  @0x240c04
        if (lhsSubType == VarSubType_Stub) {
            bool lhsBool = lhsVals.back().value_.toBool();   // @0x241aa8
            if (lhsBool) {
                // true OR x  →  true (short-circuit)           @0x241ae8
                result->setValue(lhsType, VarSubType_Stub,
                    lhsVals.back().value_);
            } else {
                // false OR x  →  valueToBool(x)                @0x241b3d
                auto rhsBool = valueToBool(
                    std::make_shared<EvalResults>(rhs), res, ctx);
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    rhsBool->assemblers_.begin(),
                    rhsBool->assemblers_.end());
                VarSubType combinedSub = combine(lhsSubType, rhsSubType);
                result->setValue(VarType_Var, combinedSub,
                    Value(static_cast<int32_t>(0)),
                    static_cast<int>(rhsBool->values_.back().reg_));
            }
            return result;
        }

        // ---- B2: lhs.subType!=Stub (numeric lhs) --------------------  @0x240f89
        // Copy rhs assemblers only.
        result->assemblers_.insert(
            result->assemblers_.end(),
            rhs.assemblers_.begin(), rhs.assemblers_.end());

        VarSubType combinedSub = combine(lhsSubType, rhsSubType); // @0x241021
        int resultRegNum = Resources::getRegisterNumber();
        result->setValue(VarType_Var, combinedSub,
            Value(static_cast<int32_t>(0)), resultRegNum);

        AsmRegister resultReg = result->values_.back().reg_;
        AsmRegister rhsReg = rhsVals.back().reg_;
        Value lhsValue = lhsVals.back().value_;

        // ori(resultReg, rhsReg, lhsValue) — Value overload @0x27a7e0
        auto oriAsms = ctx.asmCommands->ori(
            resultReg, rhsReg, lhsValue);                         // @0x242146
        result->assemblers_.insert(
            result->assemblers_.end(),
            oriAsms.begin(), oriAsms.end());

        return result;
    }

    // ---- Case C: lhs=Var, rhs=Var -----------------------------------  @0x240c6e-240ca4
    if (lhsHas1 && lhsType == VarType_Var &&
        rhsHas1 && rhsType == VarType_Var)
    {
        // Copy BOTH lhs and rhs assemblers.                        @0x240cd4-240cfa
        result->assemblers_.insert(
            result->assemblers_.end(),
            lhs.assemblers_.begin(), lhs.assemblers_.end());
        result->assemblers_.insert(
            result->assemblers_.end(),
            rhs.assemblers_.begin(), rhs.assemblers_.end());

        VarSubType combinedSub = combine(lhsSubType, rhsSubType); // @0x240d56
        int resultRegNum = Resources::getRegisterNumber();
        result->setValue(VarType_Var, combinedSub,
            Value(static_cast<int32_t>(0)), resultRegNum);

        AsmRegister resultReg = result->values_.back().reg_;
        AsmRegister lhsReg = lhsVals.back().reg_;
        AsmRegister rhsReg = rhsVals.back().reg_;

        // addi(resultReg, lhsReg, Immediate(0)) — copy lhs to result register
        auto copyAsms = ctx.asmCommands->addi(
            resultReg, lhsReg, Immediate(0));                      // @0x241252
        result->assemblers_.insert(
            result->assemblers_.end(),
            copyAsms.begin(), copyAsms.end());

        // orr(resultReg, rhsReg) — bitwise OR (single Asm)
        auto orrAsm = ctx.asmCommands->orr(resultReg, rhsReg);    // @0x2413af
        result->assemblers_.push_back(std::move(orrAsm));

        return result;
    }

    // ---- Case D: lhs=Const/Cvar, rhs=Const/Cvar --------------------  @0x240e03-240e31
    if (lhsHas1 && isConstOrCvar(lhsType) &&
        rhsHas1 && isConstOrCvar(rhsType))
    {
        int lhsInt = lhsVals.back().value_.toInt();                // @0x24186a
        int rhsInt = rhsVals.back().value_.toInt();                // @0x2416b6

        // Warning for negative operand(s): (lhsInt | rhsInt) < 0  @0x2418a4
        if ((lhsInt | rhsInt) < 0) {
            std::string lhsStr = std::to_string(lhsInt);
            std::string rhsStr = std::to_string(rhsInt);
            std::string msg = ErrorMessages::format(
                ErrorMessageT(0xFD), lhsStr, rhsStr);             // @0x2418e9
            ctx.messages->errorMessage(msg, -1);
            // continues — not fatal
        }

        VarType    combinedType    = combine(lhsType, rhsType);   // @0x24199f
        VarSubType combinedSubType = combine(lhsSubType, rhsSubType); // @0x2419fa

        result->setValue(combinedType, combinedSubType,
            Value(static_cast<int32_t>(lhsInt | rhsInt)));         // @0x241a29

        return result;
    }

    // ---- Case E: Error — incompatible types -------------------------  @0x240e72-240f03
    {
        VarType lvt = (lhsVals.size() <= 1 && !lhsVals.empty())
                          ? lhsVals[0].varType_ : VarType_Unset;
        VarType rvt = (rhsVals.size() <= 1 && !rhsVals.empty())
                          ? rhsVals[0].varType_ : VarType_Unset;

        std::string lhsStr = str(lvt);
        std::string rhsStr = str(rvt);
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x90), lhsStr, rhsStr);                 // @0x240eea
        ctx.messages->errorMessage(msg, -1);                       // @0x240efe
    }

    return result;
}

// ============================================================================
// jumpIfZero — @0x2149f0 (760B)
//
// Generates conditional branch asm instructions that skip to `label`
// when the condition value is zero.
//
// Binary address annotations:
//   0x2149f0–0x214a13  Prologue, init return vector, empty-check
//   0x214a13–0x214a3a  Count check (must be exactly 1 value)
//   0x214a3a–0x214a8d  Error path: >1 value → errMsg[0x7e], errorMessage
//   0x214a8d–0x214baa  Var path: brz(reg, label, false)
//   0x214b1d–0x214c89  Const/Cvar path: toInt() → if zero, br(label, false)
//   0x214c8d–0x214c9c  Epilogue / return
//   0x214c9d–0x214ce6  Zero-const extra: unconditional br(label, false)
// ============================================================================
std::vector<AsmList::Asm> jumpIfZero(                                // @0x2149f0
    std::shared_ptr<EvalResults> condResult,
    std::string const& label,
    FrontendLoweringContext& ctx)
{
    std::vector<AsmList::Asm> result;                                // @0x214a05

    auto const& vals = condResult->values_;

    // 1. Empty values → return empty.                               // @0x214a10
    if (vals.empty()) return result;

    // 2. Must have exactly 1 value.                                 // @0x214a34
    if (vals.size() > 1) {
        // Error: condition must produce exactly one value.           // @0x214a3a
        ctx.messages->errorMessage(
            errMsg[ErrorMessageT(0x7e)], -1);                        // @0x214a7a
        return result;
    }

    auto const& v = vals.back();

    // 3. Var path: emit brz (branch-if-zero).                       // @0x214a8d
    if (v.varType_ == VarType_Var) {
        auto asm_brz = ctx.asmCommands->brz(
            v.reg_, label, false);                                   // @0x271e40
        result.push_back(std::move(asm_brz));                        // @0x214aae
        return result;
    }

    // 4. Const/Cvar path: evaluate at compile-time.                 // @0x214b1d
    if ((static_cast<int>(v.varType_) | 2) == 6) {
        Value val = v.value_;                                        // @0x214b2c
        int intVal = val.toInt();                                    // @0x214c3d
        if (intVal == 0) {                                           // @0x214c89
            // Constant is zero → unconditional branch to label.     // @0x214c9d
            auto asm_br = ctx.asmCommands->br(label, false);         // @0x271df0
            result.push_back(std::move(asm_br));                     // @0x214caf
        }
        // Non-zero constant → condition is truthy, no jump needed.
        return result;
    }

    // 5. Unsupported type → error.                                  // @0x214a3a (shared path)
    ctx.messages->errorMessage(
        errMsg[ErrorMessageT(0x7e)], -1);
    return result;
}

// ============================================================================
// loopArgNodeAppend @0x21dcd0 — attaches arg to target's loop->next chain.
//
// If target->loop is null, sets it directly and copies target->asmId to arg.
// If chain exists, copies leaf->asmId to arg and appends arg at end.
//
// Called from SeqCWhileLoop::evaluate (Var path), SeqCDoWhile, SeqCRepeat,
// SeqCForLoop.
// ============================================================================
void loopArgNodeAppend(                                          // @0x21dcd0
    std::shared_ptr<Node> target,
    std::shared_ptr<Node> arg)
{
    if (!arg || !target) return;                                 // @0x21dce1-21dcf3

    if (target->loop) {                                          // @0x21dcf9
        // Walk loop->next chain to leaf, copy asmId to arg.     // @0x21dd27-21dd8f
        {
            auto current = target->loop;
            while (current->next) current = current->next;
            arg->asmId = current->asmId;                         // @0x21dd8f-21dd92
        }
        // Walk chain again to leaf, attach arg at end.          // @0x21de0c-21de8b
        {
            auto current = target->loop;
            while (current->next) current = current->next;
            current->next = arg;                                 // @0x21de84-21de8b
        }
    } else {                                                     // @0x21df14
        target->loop = arg;                                      // @0x21df22
        target->loop->asmId = target->asmId;                     // @0x21df65-21df72
    }
}

// ============================================================================
// loopBodyNodeAppend @0x21dfa0 — attaches body to target's loop->next chain.
//
// If target->loop is null, sets it directly.
// If chain exists, walks to leaf and appends body as leaf->next.
//
// Called from SeqCWhileLoop::evaluate (Var path), SeqCDoWhile, SeqCRepeat,
// SeqCForLoop.
// ============================================================================
void loopBodyNodeAppend(                                         // @0x21dfa0
    std::shared_ptr<Node> target,
    std::shared_ptr<Node> body)
{
    if (!body || !target) return;                                // @0x21dfae-21dfc2

    if (target->loop) {                                          // @0x21dfcb
        auto current = target->loop;                             // @0x21dff1
        while (current->next) current = current->next;           // @0x21e000-21e04f
        current->next = body;                                    // @0x21e068
    } else {                                                     // @0x21e0c7
        target->loop = body;                                     // @0x21e0d5
    }
}

} // anonymous namespace

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
    // rhsTypeOrUnset — promoted to file-scope (Phase 25d)
    // rhsSubOrDefault — promoted to file-scope (Phase 25d)

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
                // Other rhs types: just updateVar, no asm.
                res->updateVar(name);
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
                if (rhsType != VarType_Unset) {
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
                                          str(lhsType),
                                          str(rhsTypeOrUnset(rhsResult))),
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
    // isConstOrCvar — promoted to file-scope (Phase 25d)

    // 3. Helper: extract AsmRegister from values_.back(), or default(0).
    //    Binary uses `end - 8` trick to read the last 8 bytes of
    //    EvalResultValue (AsmRegister at +0x30 of the 0x38-byte struct).
    // getBackReg — promoted to file-scope (Phase 25d)

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
        // rhsCount — promoted to file-scope (Phase 25d)
        // rhsTypeOrUnset — promoted to file-scope (Phase 25d)
        // rhsSubOrDefault — promoted to file-scope (Phase 25d)

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
    // isConstOrCvar — promoted to file-scope (Phase 25d)

    // 3. Helper: extract AsmRegister from values_.back(), or default(0).
    // getBackReg — promoted to file-scope (Phase 25d)

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
        // rhsCount — promoted to file-scope (Phase 25d)
        // rhsTypeOrUnset — promoted to file-scope (Phase 25d)

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
            // Merge rhsResult.assemblers_.                   // @0x22cf09-22cf3a
            result->assemblers_.insert(
                result->assemblers_.end(),
                rhsResult.assemblers_.begin(),
                rhsResult.assemblers_.end());

            // Merge lhsResult.assemblers_.                   // @0x22cff8-22d01b
            result->assemblers_.insert(
                result->assemblers_.end(),
                lhsResult.assemblers_.begin(),
                lhsResult.assemblers_.end());

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
            // Merge lhsResult.assemblers_.                   // [implicit: lhs has no asm for Const]
            // Merge rhsResult.assemblers_.                   // @0x22cf09-area (shared code)
            result->assemblers_.insert(
                result->assemblers_.end(),
                rhsResult.assemblers_.begin(),
                rhsResult.assemblers_.end());

            // Also merge lhsResult.assemblers_ (analogous to Var+Var Row 3).
            result->assemblers_.insert(
                result->assemblers_.end(),
                lhsResult.assemblers_.begin(),
                lhsResult.assemblers_.end());

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
    // isConstOrCvar — promoted to file-scope (Phase 25d)

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
        // rhsCount — promoted to file-scope (Phase 25d)
        // rhsTypeOrUnset — promoted to file-scope (Phase 25d)

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
    // isConstOrCvar — promoted to file-scope (Phase 25d)

    // 3. Read lhsResult.values_ size and type.               // @0x231101-231112
    const size_t lhsCount = lhsResult.values_.size();
    VarType lhsType = VarType_Unset;
    if (lhsCount == 1)
        lhsType = lhsResult.values_.back().varType_;

    // Helpers for rhs access.
    // rhsCount — promoted to file-scope (Phase 25d)
    // rhsTypeOrUnset — promoted to file-scope (Phase 25d)

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
// Phase 22d — Batch 1: Trivial evaluate() overrides
// ============================================================================

// ---- Returns nullptr (16B bodies) ----------------------------------------

// SeqCCommand::evaluate(3) — @0x209db0
// Returns null shared_ptr (zeroes 16 bytes via sret). Identical to base class.
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

// ---- Returns empty EvalResults (112B bodies) ------------------------------

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

// ---- setLineNr preamble + return empty EvalResults (160B bodies) ----------

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

// ---- setLineNr preamble + delegate to child (208B body) -------------------

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

// ---- Error emitters + return empty (224B bodies) --------------------------

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
    // @0x226970: GDB-confirmed — the binary emits error 0xd5 unconditionally.
    // There is no inSwitch_ guard: the first instructions are format(0xd5, "break")
    // followed immediately by errorMessage(...), with no branch beforehand.
    ctx.messages->errorMessage(                               // @0x2269aa
        ErrorMessages::format(ErrorMessageT(0xd5),            // @0x22699b
                              "break"),                        // rodata @0x905b73
        lineNr_);                                              // lineNr = this->lineNr_
    return std::make_shared<EvalResults>();
}

// ---- Throws CompilerException (240B body) ---------------------------------

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
// Phase 22d — Batch 2: Operator wrappers that delegate to helpers
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
// NOTE: The binary uses the same " & " separator as SeqCAndExpr —
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
// Remaining evaluate() overrides (originally Phase 22d stubs, now implemented)
// ============================================================================

// ---- Operator 5-arg (inline logic, not delegated to helpers) ---------------

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
// NOTE: The binary uses " && " — confirmed from DWORD 0x20262620.
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
// NOTE: The binary uses " && " in the name string (DWORD 0x20262620) —
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
    // NOTE: " && " confirmed — not " || ". Copy-paste bug in original.
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
    // isConstOrCvar — promoted to file-scope (Phase 25d)

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

    // ---- Path 1: lhsResult Var (post-increment) --------  @0x23be27
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
    // ---- Path 2: rhsResult Var (pre-increment) ----------  @0x23be9a
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
    // ---- Path 3: lhsResult Cvar (post-increment) --------  @0x23bf0f
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
    // ---- Path 4: rhsResult Cvar (pre-increment) ---------  @0x23bf6e
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
    // ---- Path 5: lhsResult other (error) ----------------  @0x23bfd2
    else if (lhsHas1 && lhsVals.back().varType_ != VarType_Unset) {
        VarType vt = lhsVals.back().varType_;
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT(0x6f),
                str(vt), lhsResult.name_), -1);                 // @0x23c398
    }
    // ---- Path 6: rhsResult other (error) ----------------  @0x23c034
    else if (rhsHas1 && rhsVals.back().varType_ != VarType_Unset) {
        VarType vt = rhsVals.back().varType_;
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT(0x6f),
                str(vt), rhsResult.name_), -1);                 // @0x23c5f8
    }
    // else: both empty/Unset — fall through to name_tail

    // ---- Name tail ------                                    @0x23cdb3
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

    // ---- Path 1: lhsResult Var (post-decrement) ---------
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
    // ---- Path 2: rhsResult Var (pre-decrement) ----------
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
    // ---- Path 3: lhsResult Cvar (post-decrement) --------
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
    // ---- Path 4: rhsResult Cvar (pre-decrement) ---------
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
    // ---- Path 5: lhsResult other (error) ----------------
    else if (lhsHas1 && lhsVals.back().varType_ != VarType_Unset) {
        VarType vt = lhsVals.back().varType_;
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT(0x70),
                str(vt), lhsResult.name_), -1);
    }
    // ---- Path 6: rhsResult other (error) ----------------
    else if (rhsHas1 && rhsVals.back().varType_ != VarType_Unset) {
        VarType vt = rhsVals.back().varType_;
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT(0x70),
                str(vt), rhsResult.name_), -1);
    }

    // ---- Name tail ------
    // Literal "--" at rodata (SeqCDec counterpart of SeqCInc's "++").
    result->name_ = lhsResult.name_ + rhsResult.name_ + "--";

    return result;
}

// ---- Unary 3-arg (all have inline logic) -----------------------------------

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
//   NOTE: Wave(5) is NOT supported by bitwise inversion — falls to error.
//   NOTE: Var case emits addi(tempReg, R0, Immediate(-1)) TWICE before the subr.
//         This may be for dual-issue pipelines or a codegen quirk.
//   NOTE: Const/Cvar case uses toInt() (not toDouble), stays in integer domain.
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
//                         NOTE: String/Wave path computes bool(x), not !x.
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
        // NOTE: Binary computes bool(x), not !x, for this path.
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

// ---- List nodes 3-arg ------------------------------------------------------

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

// ---- Two-child 3-arg -------------------------------------------------------

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
std::shared_ptr<EvalResults> SeqCFunctionCall::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // 0x20c6a0–0x20c6ee  Prologue: setLineNr on messages, asmCommands, wavetable
    const int lineNr = lineNr_;                                    // @0x20c6cb
    ctx.messages->setLineNr(lineNr);                             // @0x20c6d3
    ctx.asmCommands->setWavetableFrontIndex(lineNr);             // @0x20c6dc
    ctx.wavetable->setLineNr(lineNr);                            // @0x20c6e9

    // 0x20c6ee–0x20c74b  make_shared<EvalResults>
    auto result = std::make_shared<EvalResults>();                // @0x20c6f3

    // 0x20c752–0x20c78e  Get function name from function() child
    auto* funNameNode = funName();                                // @0x20c755
    std::string funName =
        static_cast<const SeqCVariable*>(funNameNode)->name();

    // 0x20c78e–0x20c7e6  Check if function exists in Resources
    std::string outSig;
    bool exists = res->functionExists(funName, outSig);          // @0x20c7bd

    if (exists) {
        // ================================================================
        // PATH A: Known user-defined function
        // ================================================================

        // 0x20c7ec–0x20c84f  Generate return label and push to pending labels
        std::string retLabel = res->newLabel(std::string("ret")); // @0x20c802
        state.labelStack.push_back(std::move(retLabel));             // @0x20c813

        // 0x20c84f–0x20c929  Evaluate arguments child (if present)
        std::shared_ptr<EvalResults> argResults;
        auto* argNode = arguments();                               // @0x20c867
        if (argNode) {
            argResults = argNode->evaluate(res, ctx, state);      // @0x20c8c1
        }

        // 0x20c929–0x20d18b  Build overload signature from arg types
        std::string argTypeSig;
        if (argResults) {
            std::ostringstream oss;                                // @0x20c948
            oss << "(";                                            // @0x20c953
            auto& vals = argResults->values_;
            for (size_t i = 0; i < vals.size(); ++i) {
                if (i > 0) oss << ", ";                            // @0x20c9a4
                oss << str(vals[i].varType_);                      // @0x20c9c2
            }
            oss << ")";                                            // @0x20cd57
            argTypeSig = oss.str();
        }

        // 0x20d18b  Look up the function definition
        auto func = res->getFunction(funName, argTypeSig);        // @0x20d19c

        if (!func) {
            // 0x20d2a2–0x20d82e  Function not found error
            std::string fullSig = funName + argTypeSig;
            if (argTypeSig.empty()) fullSig += "()";

            // 0x20d5da  Error 0x4C: function not found
            ctx.messages->errorMessage(
                ErrorMessages::format(ErrorMessageT(0x4C), fullSig), -1);

            // 0x20d63f  List possible overloads
            auto overloads = res->getPossibleFunctions(funName);  // @0x20d63f
            for (auto& overload : overloads) {
                ctx.messages->errorMessage(
                    std::string("\t") + overload, -1);             // @0x20d77c
            }

            // Build display name and return
            result->name_ = funName + "(" + argTypeSig + ")";
            return result;                                         // @0x20d80f
        }

        // 0x20d1b8  Reset function scope
        func->resetScope();                                       // @0x20d1b8

        VarType returnType = func->returnType;                    // @0x20d1c4
        auto funcScope = func->scope;                             // @0x20d1d0

        // Declared here for visibility in CFVE exception handler
        std::vector<std::string> paramNames;

        // 0x20d204  Append arg asm output to result
        if (argResults) {
            result->assemblers_.insert(result->assemblers_.end(),
                argResults->assemblers_.begin(),
                argResults->assemblers_.end());                   // @0x20d204

            // 0x20d238  Compare arg count vs param count
            size_t argCount = argResults->values_.size();
            size_t paramCount = func->arguments.size();
            if (argCount != paramCount) {
                // 0x20d2d9  Error 0x4D: wrong argument count
                ctx.messages->errorMessage(
                    ErrorMessages::format(ErrorMessageT(0x4D),
                        funName,
                        paramCount,
                        argCount), -1);                           // @0x20da14
                auto empty = std::make_shared<EvalResults>();
                return empty;
            }

            // 0x20dd4e–0x20ef93  Parameter binding loop
            //   Jump table dispatch at @0x95b924 on (paramType - 2), range 0..4
            for (size_t i = 0; i < paramCount; ++i) {
                auto& param = func->arguments[i];
                VarType paramType = param.type;
                std::string paramName = param.name;

                paramNames.push_back(paramName);

                auto& argVal = argResults->values_[i];

                try {                                              // @0x20ef0b
                switch (paramType) {
                case VarType_Var: {                                 // @0x20dde8 case 0
                    // Var: bind register via addi
                    AsmRegister dstReg = funcScope->getRegister(paramName);
                    VarType argType = argVal.varType_;

                    // Emit addi to copy arg register into param register
                    if (static_cast<int>(argType) == 2) {
                        // Arg is a register — addi(dst, argReg, Immediate(0))
                        AsmRegister srcReg = argVal.reg_;         // @0x20e0d5
                        auto instr = ctx.asmCommands->addi(
                            dstReg, srcReg, Immediate(0));        // @0x273d60
                        result->assemblers_.insert(
                            result->assemblers_.end(),
                            instr.begin(), instr.end());
                    } else if ((static_cast<int>(argType) | 2) == 6) {
                        // Arg is numeric — addi(dst, R0, Value)
                        auto instr = ctx.asmCommands->addi(
                            dstReg, AsmRegister(0), argVal.value_); // @0x27a020
                        result->assemblers_.insert(
                            result->assemblers_.end(),
                            instr.begin(), instr.end());
                    }
                    break;
                }

                case VarType_Cvar: {                               // @0x20de86 case 4
                    // Cvar: update compile-time value only (no register, no addi)
                    VarType argType = argVal.varType_;
                    if ((static_cast<int>(argType) | 2) == 6) {
                        double val = argVal.value_.toDouble();
                        funcScope->updateCvar(paramName, val, argVal.varSubType_);
                    } else {
                        ctx.messages->errorMessage(
                            ErrorMessages::format(ErrorMessageT(0x46),
                                str(argType), funName, paramName,
                                str(VarType_Cvar)), -1);
                    }
                    break;
                }

                case VarType_Const: {                              // @0x20dece
                    VarType argType = argVal.varType_;
                    if ((static_cast<int>(argType) | 2) == 6) {
                        double val = argVal.value_.toDouble();
                        funcScope->updateConst(paramName, val, argVal.varSubType_, true);
                    } else {
                        ctx.messages->errorMessage(
                            ErrorMessages::format(ErrorMessageT(0x46),
                                str(argType), funName, paramName,
                                str(VarType_Const)), -1);
                    }
                    break;
                }

                case VarType_Wave: {                               // @0x20df1b
                    VarType argType = argVal.varType_;
                    if (argType == VarType_Wave) {
                        std::string waveStr = argVal.value_.toString();
                        funcScope->updateWave(paramName, waveStr, argVal.varSubType_);
                    } else {
                        ctx.messages->errorMessage(
                            ErrorMessages::format(ErrorMessageT(0x46),
                                str(argType), funName, paramName,
                                str(VarType_Wave)), -1);
                    }
                    break;
                }

                case VarType_String: {                             // @0x20df69
                    VarType argType = argVal.varType_;
                    if (argType == VarType_String) {
                        std::string strVal = argVal.value_.toString();
                        funcScope->updateString(paramName, strVal, argVal.varSubType_);
                    } else {
                        ctx.messages->errorMessage(
                            ErrorMessages::format(ErrorMessageT(0x46),
                                str(argType), funName, paramName,
                                str(VarType_String)), -1);
                    }
                    break;
                }

                default:
                    // Unknown param type — error 0x46
                    ctx.messages->errorMessage(
                        ErrorMessages::format(ErrorMessageT(0x46),
                            str(argVal.varType_), funName, paramName,
                            str(paramType)), -1);
                    break;
                }
                } catch (const CompilerException& e) {            // @0x20ef0b
                    // Catch exceptions during parameter binding and report
                    ctx.messages->errorMessage(e.what(), -1);      // @0x20ef69
                }
            }
        }

        // 0x20ef98  If return type is Var, allocate a return register
        if (returnType == VarType_Var) {                           // @0x20ef98
            int regNum = funcScope->getRegisterNumber();           // @0x20efa5
            funcScope->setReturnReg(regNum);                       // @0x20efaf
        }

        // 0x20efb4–0x20f093  Execute function body
        std::shared_ptr<EvalResults> bodyResult;
        VarType savedReturnType = returnType;                      // @0x20efb4
        int savedLineNr = ctx.messages->lineNr();                  // @0x20efce

        // NOTE: state.inLoop_ at +0x30 is repurposed here as "inFunction" flag.
        // Binary reads/writes state+0x30 for this purpose.
        uint8_t savedInLoop = state.inLoop_;                       // @0x20efd6
        state.inLoop_ = 1;                                        // @0x20efdc

        try {
            auto bodyNode = func->getBody();                       // @0x20efed
            if (bodyNode) {
                bodyResult = bodyNode->evaluate(funcScope, ctx, state); // @0x20f02f
            }
        } catch (CustomFunctionsValueException& e) {               // selector 2
            // 0x20f9a1  Catch CustomFunctionsValueException during body eval
            // Get the argument expression children list
            std::vector<const SeqCAstNode*> argChildren;           // @0x20f9b6
            auto* argNode2 = arguments();                           // @0x1fef60
            if (argNode2) {
                argChildren = argNode2->children();                // vptr[2] @0x20f9d8
            }

            state.inLoop_ = savedInLoop;                           // @0x20f9ff

            // Search for the exception's varName_ in paramNames
            std::string varName = e.varName();                     // @0x20fa04
            auto it = std::find(paramNames.begin(),
                                paramNames.end(), varName);        // @0x20fa44

            if (it != paramNames.end()) {
                // Found: map parameter name back to argument expression
                if (state.inLoop_) {                               // @0x20fa70 check restored inLoop_
                    // In a nested function call — replace varName with
                    // the actual argument expression and rethrow
                    size_t idx = static_cast<size_t>(it - paramNames.begin());
                    // argChildren[idx]->name() or similar to get expression text
                    // Binary indexes into children vector at [rbp-0x230]
                    // and passes the string to setVarName
                    if (idx < argChildren.size()) {                // @0x20fa76
                        // Get the argument expression's display name
                        auto* argExprNode = argChildren[idx];
                        std::string argExprName =
                            static_cast<const SeqCVariable*>(argExprNode)->name();
                        e.setVarName(argExprName);                 // @0x210750
                    }
                    throw;                                         // @0x20fbd9 __cxa_rethrow
                } else {
                    // Not in nested call — report error and continue
                    ctx.messages->setLineNr(
                        static_cast<int>(savedReturnType));        // @0x20fab0 (r13d)
                    std::string msg = e.what() ? e.what() : "";
                    ctx.messages->errorMessage(msg, -1);           // @0x20fafb
                }
            } else {
                // Not found: report exception's own what() string
                // Binary reads e->what() at +0x08, builds string, errorMessage
                std::string msg = e.what() ? e.what() : "";
                ctx.messages->errorMessage(msg, -1);               // @0x20fb65
            }
            // 0x20fbc4: __cxa_end_catch, then jmp 0x20f098 (body merge)
        } catch (const CompilerException& e) {
            // 0x20fd4b  Catch CompilerException during body eval
            state.inLoop_ = savedInLoop;
            std::string msg = e.what() ? e.what() : "";
            ctx.messages->errorMessage(msg, -1);
            return result;
        }

        state.inLoop_ = savedInLoop;                              // restore

        // 0x20f09f–0x20f5a4  Merge body results
        if (bodyResult) {
            // Append body's asm output to result
            result->assemblers_.insert(result->assemblers_.end(),
                bodyResult->assemblers_.begin(),
                bodyResult->assemblers_.end());                   // @0x20f0af

            // Copy body's node_ shared_ptr to result
            result->node_ = bodyResult->node_;                    // @0x20f0dc
        }

        // Build display name
        if (argResults) {
            result->name_ = funName + "(" + argResults->name_ + ")";
        } else {
            result->name_ = funName + "()";
        }

        // Truncate display name if it has more than 10 commas   @0x20d877
        // (SIMD-accelerated comma counting in binary; simplified here)
        {
            size_t commaCount = 0;
            size_t truncPos = std::string::npos;
            for (size_t j = 0; j < result->name_.size(); ++j) {
                if (result->name_[j] == ',') {
                    ++commaCount;
                    if (commaCount == 10) {
                        truncPos = j;
                        break;
                    }
                }
            }
            if (truncPos != std::string::npos) {
                result->name_ = result->name_.substr(0, truncPos) + " ...)";
            }
        }

        // 0x20f2ec  Emit return label
        if (!state.labelStack.empty()) {
            std::string lastLabel = std::move(state.labelStack.back());
            state.labelStack.pop_back();

            auto labelAsm = ctx.asmCommands->asmLabel(lastLabel); // @0x2774e0
            result->assemblers_.push_back(labelAsm);
        }

        // 0x20f3f0  Set return value
        // Binary logic: extract if returnType==Void OR returnEncountered_==true.
        // For Void: setValue(VarType_Void,...) marks result type.
        // For hasError: records partial return state.
        // Non-void + no error: skip (result already populated by body eval).
        if (savedReturnType == VarType_Void ||
            (bodyResult && bodyResult->returnEncountered_)) {
            Value retVal = funcScope->getReturnValue();            // @0x1e3d40
            AsmRegister retReg = funcScope->getReturnReg();        // @0x1e3fe0
            result->setValue(savedReturnType, retVal,
                             retReg.value);                        // @0x2107b0
        }

        // 0x20f4b0  Epilogue — no setLineNr on normal path
        //   (binary only calls setLineNr inside exception handlers)

    } else {
        // ================================================================
        // PATH B: Unknown function — delegate to CustomFunctions
        // ================================================================
        // Binary wraps this path in a try block catching
        // CustomFunctionsException (selector 1), CustomFunctionsValueException
        // (selector 2), and CompilerException (selector 3).
        // @0x20fec6: catch(CustomFunctionsException&) → errorMessage(e.what(), lineNr_)
        try {

        // 0x20ca3f  Create sub-scope for evaluation
        auto subScope = res->createSubScope(funName);              // @0x20ca52

        // 0x20ca69  Evaluate arguments in sub-scope
        std::shared_ptr<EvalResults> argResults;
        std::vector<EvalResultValue> argValues;
        auto* argNode = arguments();                                // @0x20ca77
        if (argNode) {
            argResults = argNode->evaluate(subScope, ctx, state);  // @0x20cad4
        }

        // 0x20cb5d  If argResults exist, collect asm + values
        if (argResults) {
            // Append arg asm output to result
            result->assemblers_.insert(result->assemblers_.end(),
                argResults->assemblers_.begin(),
                argResults->assemblers_.end());                    // @0x20cb69

            // Collect arg values into local vector
            argValues.insert(argValues.end(),
                argResults->values_.begin(),
                argResults->values_.end());                        // @0x20cb93
        }

        // 0x20cbc2  Call CustomFunctions::call()
        auto cfResult = ctx.customFunctions->call(
            funName, argValues, res);                              // @0x20cc04

        // 0x20cc38  Merge CF result asm output
        result->assemblers_.insert(result->assemblers_.end(),
            cfResult->assemblers_.begin(),
            cfResult->assemblers_.end());                          // @0x20cc38

        // 0x20cc66  Assign CF result values to our result
        result->values_ = cfResult->values_;                       // @0x20cc6f

        // 0x20cc90  Copy node_ shared_ptr
        result->node_ = cfResult->node_;                           // @0x20cc90

        // 0x20ccd2  Copy waveformFront_ shared_ptr
        result->waveformFront_ = cfResult->waveformFront_;         // @0x20ccd2

        // Build display name and return
        if (argResults) {
            result->name_ = funName + "(" + argResults->name_ + ")";
        } else {
            result->name_ = funName + "()";
        }

        // Same comma-truncation logic as Path A
        {
            size_t commaCount = 0;
            size_t truncPos = std::string::npos;
            for (size_t j = 0; j < result->name_.size(); ++j) {
                if (result->name_[j] == ',') {
                    ++commaCount;
                    if (commaCount == 10) {
                        truncPos = j;
                        break;
                    }
                }
            }
            if (truncPos != std::string::npos) {
                result->name_ = result->name_.substr(0, truncPos) + " ...)";
            }
        }

        // 0x20d3e0: Copy result->name_ into waveformFront_->functionArgs
        // Binary: mov 0x48(%rbx),%rdi; add $0x20,%rdi; lea 0x58(%rbx),%rax
        // This sets wf->functionArgs = result->name_ (e.g. "zeros(64)")
        if (result->waveformFront_) {
            result->waveformFront_->functionArgs = result->name_;  // @0x20d3e0
        }

        } catch (CustomFunctionsValueException& e) {               // @0x20fe1e selector 2
            // If in a loop context, remap the variable name and rethrow
            if (state.inLoop_) {                                    // @0x20fe26: cmpb $0x1,0x30(%r12)
                // Get argument children, find the parameter index matching e.varName(),
                // replace with the argument expression name, and rethrow
                auto* argNode = arguments();
                if (argNode) {
                    auto argChildren = argNode->children();
                    std::string varName = e.varName();
                    // The binary indexes by e's internal index (offset +0x20)
                    size_t idx = e.argIndex();                       // @0x21001c: mov 0x20(%rbx),%rax
                    if (idx < argChildren.size()) {
                        auto* argExprNode = argChildren[idx];
                        std::string argExprName =
                            static_cast<const SeqCVariable*>(argExprNode)->name();
                        e.setVarName(argExprName);                  // @0x21002f
                    }
                }
                throw;                                              // @0x210034: __cxa_rethrow
            }
            // Not in loop — report error and continue                // @0x20fe32
            std::string msg = e.what() ? e.what() : "";
            int funLineNr = funNameNode ? funNameNode->lineNr() : lineNr_;  // @0x20fe72..20fe77
            ctx.messages->errorMessage(msg, funLineNr);             // @0x20fe81
        } catch (CustomFunctionsException const& e) {              // @0x20fec6 selector 1
            // Report error via errorMessage with this node's line number
            std::string msg = e.what() ? e.what() : "";
            ctx.messages->errorMessage(msg, lineNr_);              // @0x20ff1a
        } catch (CompilerException const& e) {                     // @0x20fdf0 selector 3
            ctx.messages->errorMessage(
                std::string(e.what() ? e.what() : ""),
                lineNr_);                                          // @0x20fe81
        }
    }

    return result;
}

// SeqCArray::evaluate(3) — @0x211140, 2412B
//   Array indexing: arr[idx].
//   Evaluates array() and index() children. Validates:
//     - arrayResult has exactly 1 value of type Wave
//     - indexResult has exactly 1 value of type Const or Cvar
//   Looks up waveform by name from wavetable, validates index bounds,
//   returns wave-with-index (VarType_Wave) + sample value (VarType_Cvar).
std::shared_ptr<EvalResults> SeqCArray::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    const int lineNr = lineNr_;                                        // @0x211167
    ctx.messages->setLineNr(lineNr);                                 // @0x21116f
    ctx.asmCommands->setWavetableFrontIndex(lineNr);                 // @0x211174
    ctx.wavetable->setLineNr(lineNr);                                // @0x211188

    auto result = std::make_shared<EvalResults>();                   // @0x21118d

    if (!array()) {                                                  // @0x2111e5 (check index_ != null)
        return result;
    }

    // ---- Evaluate array child ----                                // @0x2111f0
    auto arrayResult = array()->evaluate(res, ctx, state);           // @0x2111f3..211238

    // ---- Evaluate index child ----                                // @0x21126a
    auto indexResult = index()->evaluate(res, ctx, state);           // @0x21126d..2112ab

    // ---- Validate array result: exactly 1 Wave value ----         // @0x2112dd
    if (!arrayResult ||
        arrayResult->values_.size() != 1 ||
        arrayResult->values_.back().varType_ != VarType_Wave) {      // @0x211316
        // Error 0xd: array operand must be a single wave
        std::string msg = ErrorMessages::format(                     // @0x211389
            ProgramTooLarge);
        ctx.messages->errorMessage(msg, -1);                         // @0x211400

        // ---- Cleanup indexResult + arrayResult, return result ---- // @0x21140e
        return result;
    }

    // ---- Validate index result: exactly 1 Const/Cvar value ----   // @0x211318
    if (!indexResult ||
        indexResult->values_.size() != 1 ||
        (indexResult->values_.back().varType_ | 0x2) != 0x6) {       // @0x211350
        // Error 0xe: array index must be a single const/cvar
        std::string msg = ErrorMessages::format(                     // @0x2113ca
            ArrayIndexNeedConst);
        ctx.messages->errorMessage(msg, -1);                         // @0x211400

        return result;
    }

    // ---- Extract array value (Wave) ----                          // @0x211352
    auto& arrayVal = arrayResult->values_.back();
    Value arrayValue = arrayVal.value_;                              // copy the embedded Value

    // ---- Convert array value to string (waveform name) ----       // @0x2114df
    std::string waveName = arrayValue.toString();                    // @0x15de50

    // ---- Look up waveform by name ----                            // @0x211546
    std::optional<std::string> optName(waveName);
    auto wf = ctx.wavetable->getWaveformByName(optName);             // @0x29c180

    if (!wf) {                                                       // @0x211568
        // Waveform not found — error 0xe9
        std::string msg = ErrorMessages::format(                     // @0x21160e
            WaveformNotFound, waveName);
        ctx.messages->errorMessage(msg, -1);                         // @0x21161f
        return result;
    }

    // ---- Extract index value and convert to int ----              // @0x21156f
    Value indexValue;
    if (indexResult->values_.empty()) {                               // @0x2115d0
        // Empty — default Value (0)
    } else {
        auto& idxVal = indexResult->values_.back();
        indexValue = idxVal.value_;                                   // copy
    }
    int idx = indexValue.toInt();                                     // @0x2116b9

    // ---- Compute array bound: byte count of padded waveform ----  // @0x2116e2
    uint16_t channels = wf->signal.channels_;                        // +0xC8
    uint32_t length = static_cast<uint32_t>(wf->signal.length_);     // +0xD0
    auto* dc = wf->deviceConstants;                                  // +0x78

    int arrayBound = 0;
    if (length != 0) {                                               // @0x2116f9
        uint32_t granularity = dc->maxWaveformLength;              // +0x40
        uint32_t pageSize = dc->grainSize;                    // +0x44
        // Pad length to multiple of pageSize, at least granularity
        uint32_t padded = ((length + pageSize - 1) / pageSize) * pageSize;
        if (granularity > padded) padded = granularity;              // @0x211713

        uint64_t totalSamples = static_cast<uint64_t>(padded) * channels;
        int64_t bitsPerSample = static_cast<int32_t>(dc->bitsPerSample);  // +0x50
        uint64_t totalBits = totalSamples * bitsPerSample;
        arrayBound = static_cast<int>((totalBits + 7) / 8);         // ceil bits→bytes
    }

    // ---- Bounds check ----                                        // @0x211736
    if (idx < 0 || idx >= arrayBound) {                              // @0x211739, @0x211742
        // Error 0xf: array index out of bounds
        std::string msg = ErrorMessages::format(                     // @0x2118d6
            ArraysOnlyWave);
        ctx.messages->errorMessage(msg, -1);                         // @0x21191a
        return result;
    }

    // ---- Create new result, set wave value with index ----        // @0x211748
    auto indexedResult = std::make_shared<EvalResults>();

    // Store indexedResult in result->arrayBacking_ (NOT result = indexedResult). // @0x211791
    // The binary stores the indexed EvalResults in arrayBacking_ of the
    // original result. SeqCAssign::evaluate later swaps effectiveLhs with
    // arrayBacking_ to dispatch on the indexed type (Wave) instead of
    // the outer type (Cvar).
    result->arrayBacking_ = indexedResult;

    // setValue(VarType_Wave, VarSubType_Vect, Value(int=index))   // @0x2117f7
    // Called on indexedResult (via result->arrayBacking_).
    indexedResult->setValue(VarType_Wave, VarSubType_Vect, Value(idx));   // @0x16bfb0

    // ---- Set waveformFront_ on indexedResult ----                 // @0x211822
    indexedResult->waveformFront_ = wf;                              // @0x21183c

    // ---- Read sample value from signal ----                       // @0x21187a
    wf->signal.checkAllocation();                                    // @0x246950
    double sampleVal = wf->signal.samples_[idx];                     // @0x211885 (double at samples_[idx])

    // setValue(VarType_Cvar, Value(double=sampleVal)) on the ORIGINAL result // @0x2118a9
    result->setValue(VarType_Cvar, Value(sampleVal));                 // @0x211b70

    return result;
}

// SeqCIfCondition::evaluate(3) — @0x2138e0, 4360B
//   Evaluates "if (cond) body" — no else branch.
//
//   Binary address annotations:
//     0x2138e0–0x213929  Prologue, setLineNr, make_shared<EvalResults>
//     0x21392a–0x2139a6  newLabel("end") → endLabel
//     0x2139c6–0x213a6d  Evaluate cond() in sub-scope("if-args") → condResult
//     0x213a6d–0x213ad3  Null condResult → errorMessage(0x27, "if"), cleanup
//     0x213af5–0x213c62  Build result name: "if (" + condResult->name_ + ")"
//     0x213c62–0x213cb0  Create sub-scope("if") → bodyScope
//     0x213cb0–0x213ce5  Validate: exactly 1 value, check varType
//     0x213ce5–0x213d6a  Error: wrong value count/type → errorMessage(0x27)
//     0x213d6a–0x2143c5  Var path: copy asms, branch node, jumpIfZero, label,
//                         setState(Active), evaluate body, merge, end label
//     0x213e1c–0x2146d3  Const/Cvar path: toInt(), if nonzero evaluate body,
//                         setValue, copy node, copy returnEncountered_
//     0x2146d3–0x214744  Cleanup and return
//
//   NOTE: Binary calls SeqCIfCondition::ifBody() @0x201de0; our SEQC_BINARY
//   macro generates body() — both access body_.get(), semantically identical.
std::shared_ptr<EvalResults> SeqCIfCondition::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // ---- Prologue: set lineNr on subsystems ----                  // @0x213907
    const int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);                                 // @0x21390f
    ctx.asmCommands->setWavetableFrontIndex(lineNr);                 // @0x213918
    ctx.wavetable->setLineNr(lineNr);                                // @0x213925

    // ---- Allocate result ----                                     // @0x21392a
    auto result = std::make_shared<EvalResults>();

    // ---- Generate end label ----                                  // @0x21399a
    std::string endLabel = Resources::newLabel("end");               // @0x1ec6b0

    // ---- Evaluate condition in sub-scope("if-args") ----          // @0x2139c6
    auto condScope = res->createSubScope("if-args");                 // @0x213a05
    auto condResult = cond()->evaluate(condScope, ctx, state);       // @0x213a29

    // ---- Null condResult check ----                               // @0x213a6d
    if (!condResult) {
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "if");                              // @0x213aad
        ctx.messages->errorMessage(msg, -1);                         // @0x213ac1
        return result;                                               // → @0x2146f1 cleanup
    }

    // ---- Build result name: "if (" + condResult->name_ + ")" --- // @0x213af5
    result->name_ = "if (" + condResult->name_ + ")";               // @0x213bda

    // ---- Create sub-scope("if") for body ----                     // @0x213c62
    auto bodyScope = res->createSubScope("if");                      // @0x213c8b

    // ---- Validate: exactly 1 value ----                           // @0x213cb0
    if (condResult->values_.empty() ||
        condResult->values_.size() != 1)                             // @0x213ccd (magic ÷7)
    {
        // Error: bad condition value count.                          // @0x213ce5
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "if");                              // @0x213cff
        ctx.messages->errorMessage(msg, cond()->lineNr());              // @0x213d19
        return result;                                               // → @0x2146d3 cleanup
    }

    auto const& condVal = condResult->values_.back();

    // ================================================================
    // Var path (varType == 2)                                       // @0x213d6a
    // ================================================================
    if (condVal.varType_ == VarType_Var) {
        // Copy condResult assemblers into result.                   // @0x213d74
        result->assemblers_.insert(
            result->assemblers_.end(),
            condResult->assemblers_.begin(),
            condResult->assemblers_.end());

        // Create branch node asm.                                   // @0x213db5
        AsmList::Asm branchAsm = ctx.asmCommands->asmBranchNode();   // @0x277950
        result->assemblers_.push_back(branchAsm);                    // @0x213dca

        // Assign branch node to result and set flag.                // @0x213e91
        result->node_ = branchAsm.node;                              // @0x213e91
        result->node_->branchMaySkipAllBodies = true;                // @0x213ecf

        // Generate jump-if-zero instructions.                       // @0x213f15
        auto jumpAsms = jumpIfZero(condResult, endLabel, ctx);       // @0x2149f0

        // Insert jump asm instructions into result assemblers.      // @0x213f41
        result->assemblers_.insert(
            result->assemblers_.end(),
            jumpAsms.begin(),
            jumpAsms.end());

        // Generate "if" label.                                      // @0x214043
        std::string ifLabel = Resources::newLabel("if");             // @0x1ec6b0
        AsmList::Asm ifLabelAsm =
            ctx.asmCommands->asmLabel(ifLabel);                      // @0x2774e0
        result->assemblers_.push_back(ifLabelAsm);                   // @0x21406c

        // Set bodyScope to Active state.                            // @0x21415b
        bodyScope->setState(Resources::State::Active);               // @0x1e35f0

        // Evaluate body (if present).                               // @0x21416f
        if (ifBody()) {                                              // @0x201de0
            // Create body sub-scope and evaluate.                   // @0x21417d
            auto bodySubScope = bodyScope->createSubScope("if-body"); // @0x2141bf
            auto bodyResult = ifBody()->evaluate(bodySubScope, ctx, state); // @0x2141e3

            if (bodyResult) {                                        // @0x21423b
                // Merge body assemblers.                            // @0x214240
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    bodyResult->assemblers_.begin(),
                    bodyResult->assemblers_.end());

                // Push bodyResult->node_ into result->node_->branches. // @0x21426a
                result->node_->branches.emplace_back(bodyResult->node_);
            }
        }

        // Generate end label asm.                                   // @0x2142b2
        AsmList::Asm endLabelAsm =
            ctx.asmCommands->asmLabel(endLabel);                     // @0x2774e0
        result->assemblers_.push_back(endLabelAsm);                  // @0x2142de

        return result;                                               // @0x2143c5 → @0x2146d3
    }

    // ================================================================
    // Const/Cvar path (varType | 2 == 6)                            // @0x213e1c
    // ================================================================
    if ((static_cast<int>(condVal.varType_) | 2) == 6) {
        // Extract value and evaluate at compile-time.               // @0x213e2b
        Value val = condVal.value_;                                  // @0x213e3e
        int intVal = val.toInt();                                    // @0x214428

        // If zero (false): skip body entirely.                      // @0x21446b
        if (intVal == 0) {
            return result;                                           // → @0x2146d3 cleanup
        }

        // Non-zero (true): evaluate body if present.                // @0x214473
        if (ifBody()) {                                              // @0x201de0
            // Evaluate body using bodyScope.                        // @0x214489
            auto bodyResult = ifBody()->evaluate(bodyScope, ctx, state); // @0x2144cc

            if (bodyResult) {                                        // @0x2144fe
                // Merge body assemblers.                            // @0x21450e
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    bodyResult->assemblers_.begin(),
                    bodyResult->assemblers_.end());

                // Copy value from body result.                      // @0x214542
                result->setValue(bodyResult->getValue());             // @0x21460e

                // Copy node.                                        // @0x214645
                result->node_ = bodyResult->node_;                   // @0x214662

                // Copy hasError flag.                               // @0x21468f
                result->returnEncountered_ = bodyResult->returnEncountered_;           // @0x2146a1
            }
        }

        return result;                                               // → @0x2146d3 cleanup
    }

    // ================================================================
    // Error: unsupported varType                                    // @0x213ce5 (shared path)
    // ================================================================
    {
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "if");                              // @0x213cff
        ctx.messages->errorMessage(msg, cond()->lineNr());              // @0x213d19
    }

    return result;                                                   // @0x2146d3
}

// SeqCCaseEntry::evaluate(3) — @0x21aa40, 2845B
//   Case entry in a switch statement.
//
//   Three helper methods (not in SEQC_BINARY, inlined here):
//     label()      @0x202b50: dynamic_cast<const SeqCValue*>(value())
//     hasLabel()   @0x202b80: value() != nullptr
//     validLabel() @0x202b90: dynamic_cast<const SeqCValue*>(value()) != nullptr
//
//   Algorithm:
//     1. Reject if not inside a switch (state.inSwitch_).
//     2. Prologue: setLineNr on messages, asmCommands+0x50, wavetable.
//     3. Evaluate label child (must be SeqCValue*).
//     4. Default case (no label) → return empty result.
//     5. Validate: exactly 1 Const-typed value in label result.
//     6. floatEqual(toDouble, (double)toInt) → warning if non-integer.
//     7. toInt() < 0 → throw.
//     8. setValue(caseVal), name_ = "case " + toString().
//
//   Error codes: 0x1d=CaseOutsideSwitch, 0x17=CaseNeedsConst,
//                0x1c=CaseRounded (warning), 0x1b=CasePositiveNatural.
//
//   NOTE: The binary extracts values_.back() 6 separate times; we call
//   getValue() once and reuse the Value for clarity.
std::shared_ptr<EvalResults> SeqCCaseEntry::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // ---- Switch context check ----                                // @0x21aa5b
    if (!state.inSwitch_) {
        throw CompilerException(                                     // @0x21b331
            errMsg[ErrorMessageT(0x1d)]);
    }

    // ---- Prologue: set lineNr on subsystems ----                  // @0x21aa6f
    const int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);                                 // @0x21aa7a
    ctx.asmCommands->setWavetableFrontIndex(lineNr);                 // @0x21aa84
    ctx.wavetable->setLineNr(lineNr);                                // @0x21aa95

    auto result = std::make_shared<EvalResults>();                   // @0x21aa9a

    // ---- Evaluate label child (if present) ----                   // @0x21aaf4
    auto* labelNode =
        dynamic_cast<const SeqCValue*>(label());                     // @0x202b50

    std::shared_ptr<EvalResults> labelResult;
    if (labelNode) {
        labelResult = labelNode->evaluate(res, ctx, state);          // @0x21ab34
    }

    // ---- Validate label presence ----                             // @0x21ab82
    const bool hasLbl = (label() != nullptr);                        // hasLabel() @0x202b80
    if (hasLbl) {
        const bool validLbl =                                        // validLabel() @0x202b90
            (dynamic_cast<const SeqCValue*>(label()) != nullptr);
        if (!validLbl) {
            throw CompilerException(                                 // @0x21b370
                errMsg[ErrorMessageT(0x17)]);
        }
    }

    // ---- Default case: no label → return empty result ----        // @0x21ab9e
    if (!hasLbl) {
        return result;                                               // @0x21b2c2
    }

    // ---- Validate label result ----                               // @0x21abae
    if (!labelResult ||                                              // @0x21abbb
        labelResult->values_.empty() ||                              // @0x21abc8
        labelResult->values_.size() != 1 ||                          // @0x21abe9 (magic ÷7)
        labelResult->values_.back().varType_ != VarType_Const)       // @0x21abf3
    {
        throw CompilerException(                                     // @0x21b306
            errMsg[ErrorMessageT(0x17)]);
    }

    // ---- Extract case value ----                                  // @0x21abfd
    Value caseVal = labelResult->getValue();                         // @0x211ab0

    // ---- Integrality check ----                                   // @0x21ac83
    double dVal = caseVal.toDouble();                                // @0x21ac87
    int    iVal = caseVal.toInt();                                   // @0x21ad41
    if (!floatEqual(dVal, static_cast<double>(iVal))) {              // @0x21ad52
        // Non-integer case value — emit warning.                    // @0x21adaf
        auto msg = ErrorMessages::format(
            ErrorMessageT(0x1c), dVal, iVal);                        // @0x21af31
        ctx.messages->warningMessage(msg, -1);                       // @0x21af47
    }

    // ---- Non-negative check ----                                  // @0x21b070
    if (caseVal.toInt() < 0) {                                       // @0x21b0a2
        throw CompilerException(                                     // @0x21b3b5
            ErrorMessages::format(
                ErrorMessageT(0x1b), caseVal.toInt()));              // @0x21b3e6
    }

    // ---- Set result value and name ----                           // @0x21b0aa
    result->setValue(caseVal);                                        // @0x21b160
    result->name_ = "case " + caseVal.toString();                    // @0x21b23f

    return result;                                                   // @0x21b2c2
}

// SeqCSwitchCase::hasCases() — @0x202760
//   Returns true if the cases child (body_/body()) is non-null.
bool SeqCSwitchCase::hasCases() const                                // @0x202760
{
    return body() != nullptr;                                        // cmp [rdi+0x20], 0; setne al
}

// SeqCSwitchCase::isSingleCase() — @0x202730
//   Returns true if body() is a single SeqCCaseEntry (not a SeqCStmtList).
bool SeqCSwitchCase::isSingleCase() const                            // @0x202730
{
    if (!body()) return false;
    return dynamic_cast<const SeqCCaseEntry*>(body()) != nullptr;
}

// SeqCSwitchCase::singleCase() — @0x202770
//   Returns body() cast to SeqCCaseEntry*, or nullptr.
const SeqCCaseEntry* SeqCSwitchCase::singleCase() const              // @0x202770
{
    if (!body()) return nullptr;
    return dynamic_cast<const SeqCCaseEntry*>(body());
}

// SeqCSwitchCase::cases() — @0x202700
//   Returns body() cast to SeqCStmtList*, or nullptr.
const SeqCStmtList* SeqCSwitchCase::cases() const                    // @0x202700
{
    if (!body()) return nullptr;
    return dynamic_cast<const SeqCStmtList*>(body());
}

// Anonymous namespace helper: evalCaseBody — @0x216fc0, ~2200B
//   Evaluates a single SeqCCaseEntry (label), performs value matching
//   between caseResult and condResult, and ALWAYS pushes caseResult
//   to the results vector.
//
//   The matching logic (comparing case value vs condition value) extracts
//   Value info but does NOT gate the push — every case entry produces a
//   result.  The caller (SeqCSwitchCase::evaluate) uses the varType and
//   value in each result to decide how to generate branch logic.
//
//   Special path: if condResult->values_ is empty, looks up error 0x19
//   and calls caseEntry.body()->evaluate() to recover body assemblers.
//   This path is not normally taken for Var-discriminant switches.
namespace {
void evalCaseBody(
    SeqCCaseEntry const& caseEntry,
    std::vector<std::shared_ptr<EvalResults>>& results,
    std::shared_ptr<Resources> subRes,
    std::shared_ptr<EvalResults> condResult,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state)                                    // @0x216fc0
{
    // ---- Step 1: Evaluate the case entry (label) ----             // @0x217020
    auto caseResult = caseEntry.evaluate(subRes, ctx, state);

    if (!caseResult) return;

    // ---- Step 2 + 3: Value matching and body evaluation ----       // @0x21706d
    // For Const/Cvar condResult: compare values and evaluate body if matched.
    // For Var/other condResult: always evaluate body.
    // Binary: step 2 (0x21706d-0x21781a) interleaves matching with body eval.

    bool evalBody = true;

    if (condResult && !condResult->values_.empty()) {
        if (condResult->values_.size() == 1 &&
            (condResult->values_.back().varType_ | 0x2) == 0x6) {
            // Const/Cvar single value — evaluate body only if values match
            evalBody = false;  // default: don't eval

            if (caseResult->values_.size() == 1 &&
                (caseResult->values_.back().varType_ | 0x2) == 0x6) {
                // Both Const/Cvar — compare toInt values              // @0x217809
                Value condValue;
                auto& condVals = condResult->values_;
                if (!condVals.empty())
                    condValue = condVals.back().value_;
                Value caseValue;
                auto& caseVals = caseResult->values_;
                if (!caseVals.empty())
                    caseValue = caseVals.back().value_;

                if (condValue.toInt() == caseValue.toInt()) {
                    // Match! Set scopeBoundary flag and evaluate body  // @0x21780e
                    subRes->setAtScopeBoundary(true);                   // 0x89 flag
                    evalBody = true;
                }
            }
            else if (caseResult->values_.empty()) {
                // Default case — evaluate body if no prior match       // @0x217682
                if (!subRes->atScopeBoundary()) {
                    subRes->setAtScopeBoundary(true);
                    evalBody = true;
                }
            }
            // Otherwise (multi-valued, etc): skip body
        }
    } else {
        // condResult is null or empty — error 0x19 if case has no values
        if (caseResult->values_.empty()) {                           // @0x217360
            auto const& msg = ErrorMessages::messages.at(
                ErrorMessageT(0x19));                                // @0x2173aa
            ctx.messages->errorMessage(msg, caseEntry.lineNr());    // @0x2173b8
        }
    }

    if (evalBody) {
        // Evaluate case body                                        // @0x217472
        bool savedHasError = caseResult->returnEncountered_;
        caseResult->returnEncountered_ = false;

        if (caseEntry.body()) {
            auto bodyResult = caseEntry.body()->evaluate(
                subRes, ctx, state);

            if (bodyResult) {
                caseResult->assemblers_.insert(
                    caseResult->assemblers_.end(),
                    bodyResult->assemblers_.begin(),
                    bodyResult->assemblers_.end());
                caseResult->returnEncountered_ = bodyResult->returnEncountered_;
                caseResult->waveformFront_ = bodyResult->waveformFront_;
                caseResult->node_ = bodyResult->node_;
            }
        }

        caseResult->returnEncountered_ = caseResult->returnEncountered_ && savedHasError;
    }

    // ---- ALWAYS push result ----                                  // @0x2175b0
    results.push_back(caseResult);
}
} // anonymous namespace

// SeqCSwitchCase::evalCases() — @0x216980, ~1500B
//   Evaluates each case entry under this switch, collecting results
//   for matching cases.
//
//   Two paths:
//   1. isSingleCase() — body is directly a SeqCCaseEntry → evalCaseBody
//   2. Multi-case — body is SeqCStmtList → iterate stmts, dynamic_cast
//      each to SeqCCaseEntry, call evalCaseBody. Non-CaseEntry children
//      (e.g. bare statements between cases) are evaluated and their
//      assemblers merged into the last result. CompilerException caught
//      and emitted per iteration.
//   If no results after iteration, emit warning 0x1a.
std::vector<std::shared_ptr<EvalResults>> SeqCSwitchCase::evalCases(
    std::shared_ptr<Resources> subRes,
    std::shared_ptr<EvalResults> condResult,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const                              // @0x216980
{
    std::vector<std::shared_ptr<EvalResults>> results;

    if (isSingleCase()) {                                            // @0x2169ba
        // Single case entry — evaluate directly
        evalCaseBody(*singleCase(), results,                         // @0x216a23
                     subRes, condResult, ctx, state);
        return results;
    }

    // Multi-case: get cases list                                    // @0x216a91
    const SeqCStmtList* casesList = cases();
    if (!casesList) return results;

    // Iterate case entries                                          // @0x216ae1
    auto& stmts = casesList->elements();
    for (auto& stmt : stmts) {
        if (!stmt) continue;

        // Try dynamic_cast to SeqCCaseEntry                        // @0x216af0
        auto* caseEntry = dynamic_cast<const SeqCCaseEntry*>(stmt.get());

        if (caseEntry) {
            // Valid case entry — evalCaseBody                       // @0x216b61
            try {
                evalCaseBody(*caseEntry, results,
                             subRes, condResult, ctx, state);
            } catch (CompilerException& e) {                         // @0x216dfc
                // Emit exception message and continue               // @0x216e39
                const char* msg = e.what();
                ctx.messages->errorMessage(
                    msg ? std::string(msg) : std::string(), -1);
            }
        } else {
            // Not a CaseEntry — evaluate as bare statement          // @0x216bb0
            if (results.empty()) continue;                           // @0x216bb7 (skip if no prior case)

            try {
                auto stmtResult = stmt->evaluate(subRes, ctx, state); // @0x216bf2

                if (stmtResult) {
                    // Merge assemblers into last result in vector    // @0x216c23
                    auto& lastResult = results.back();
                    // Access through last element of results vector
                    // results.back() is shared_ptr; [rbx+0x8]-0x10 is
                    // the second-to-last entry's raw EvalResults*
                    auto& target = results.back();
                    target->assemblers_.insert(
                        target->assemblers_.end(),
                        stmtResult->assemblers_.begin(),
                        stmtResult->assemblers_.end());

                    // Walk node chain to end, copy waveformFront    // @0x216c60
                    if (target->node_) {
                        auto node = target->node_;
                        while (node->next) node = node->next;       // @0x216cc1
                        // Copy waveformFront from stmtResult
                        target->waveformFront_ = stmtResult->waveformFront_;
                    } else {
                        target->waveformFront_ = stmtResult->waveformFront_;
                    }

                    // AND returnEncountered_ with stmtResult                 // @0x216d94
                    target->returnEncountered_ &= stmtResult->returnEncountered_;
                }
            } catch (CompilerException& e) {
                const char* msg = e.what();
                ctx.messages->errorMessage(
                    msg ? std::string(msg) : std::string(), -1);
            }
        }
    }

    // If no cases matched, emit warning 0x1a                        // @0x216e6f
    if (results.empty()) {
        std::string msg = ErrorMessages::format(
            NeedCaseBeforeStmt);
        ctx.messages->errorMessage(msg, -1);                         // @0x216ec3
    }

    return results;
}

// SeqCSwitchCase::evaluate(3) — @0x217a80, 11506B
//   Switch/case statement evaluation.
//   Three-way dispatch on condition varType: Var, Const/Cvar, or error.
//
//   Binary address annotations:
//     0x217a80–0x217acf  Prologue: setLineNr on messages, asmCommands, wavetable
//     0x217acf–0x217ad7  Save state.inSwitch_, set state.inSwitch_ = true
//     0x217adc–0x217b34  make_shared<EvalResults> → result
//     0x217b34–0x217b8c  createSubScope("switch") → subRes
//     0x217b8c–0x217bfd  cond()->evaluate(subRes, ctx, state) → condResult
//     0x217bfd–0x217e9c  Null/size check: condResult null or size>1 → error 0x27
//     0x217ea1–0x217ea5  Dispatch: varType==2 → Var path
//     0x218083–0x21808c  Dispatch: (varType|2)==6 → Const/Cvar path
//     0x217dcc           Default: error 0x27
//
//   Var path (@0x217eab–0x21a074):
//     setState(Active), hasCases → evalCases, merge condResult assemblers,
//     init 3 AsmLists (switch/case/body), asmBranchNode → push + copy node,
//     branchMaySkipAllBodies=true, getRegisterNumber → switchReg,
//     compute totalCycles, addi(switchReg, zeroReg, Immediate(totalCycles)),
//     suser(switchReg, AddressImpl(0x1a)), result->returnEncountered_=true,
//     endLabel=newLabel("end"), loop cases:
//       Const/Cvar case: getRegisterNumber→caseReg, newLabel("case"),
//         extract values, Immediate(-caseVal.toInt()), addi → switchAsms,
//         readConst("AWG_WAIT_TRIGGER", EDirection(1)) → addi → bodyAsms,
//         merge case assemblers → bodyAsms, wtrig → bodyAsms,
//         brz(zeroReg, endLabel, true) → bodyAsms, emplace branchChildren,
//         AND hasError
//       Default (varType==Unset): branchMaySkipAllBodies=false,
//         getRegisterNumber → defaultReg, readConst → addi → bodyAsms,
//         merge case assemblers → bodyAsms, wtrig → bodyAsms,
//         brz(zeroReg, endLabel/defaultLabel) → switchAsms
//       Other varType: error 0x17
//     After loop: nop → switchAsms (if no default: brz→endLabel),
//       insert bodyAsms → switchAsms, asmLabel(defaultLabel if present),
//       insert caseAsms → switchAsms, asmLabel(endLabel) → switchAsms,
//       merge all into result, AND hasError
//
//   Const/Cvar path (@0x218092–0x219b0f):
//     scopeBoundaryFlags_=1, hasCases → evalCases, scopeBoundaryFlags_=0,
//     check state.inFunctionDef_, loop cases:
//       if size==1 && (varType|2)==6: Value::operator== match →
//         matchedResult = case, AND hasError
//     After loop: if matchedResult, merge assemblers, copy hasError,
//       if hasError: setValue(getReturnValue()), copy node_
//
//   Epilogue: restore state.inSwitch_, cleanup, return
std::shared_ptr<EvalResults> SeqCSwitchCase::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // ---- Prologue: set lineNr on subsystems ----                  // @0x217a94
    const int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);                                 // @0x217aaf
    ctx.asmCommands->setWavetableFrontIndex(lineNr);                 // @0x217ab9
    ctx.wavetable->setLineNr(lineNr);                                // @0x217aca

    // ---- Save and set state.inSwitch_ ----                        // @0x217acf
    const bool savedInSwitch = state.inSwitch_;                      // +0x31
    state.inSwitch_ = true;                                          // @0x217ad7

    // ---- Allocate result ----                                     // @0x217adc
    auto result = std::make_shared<EvalResults>();

    // ---- Create sub-scope ----                                    // @0x217b34
    auto subRes = res->createSubScope("switch");                     // @0x217b67

    // ---- Evaluate condition ----                                  // @0x217b8c
    auto condResult = cond()->evaluate(subRes, ctx, state);          // @0x217bd8

    // ---- Null condResult check ----                               // @0x217bfd
    if (!condResult) {
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "switch");                          // @0x217e53
        ctx.messages->errorMessage(msg, -1);                         // @0x217e73
        state.inSwitch_ = savedInSwitch;                             // @0x217e1f
        return result;
    }

    // ---- Build result name: "switch (" + condResult->name_ + ")" // @0x217c0d
    result->name_ = "switch (" + condResult->name_ + ")";           // @0x217d20

    // ---- Validate: exactly 1 value ----                           // @0x217d90
    if (condResult->values_.empty() ||
        condResult->values_.size() != 1)                             // @0x217da1
    {
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "switch");                          // @0x217dd6
        ctx.messages->errorMessage(msg, -1);                         // @0x217df6
        state.inSwitch_ = savedInSwitch;                             // @0x217e1f
        return result;
    }

    auto const& condVal = condResult->values_.back();

    // ================================================================
    // Var path (varType == 2)                                       // @0x217ea1
    // ================================================================
    if (condVal.varType_ == VarType_Var) {
        subRes->setState(Resources::State::Active);                   // @0x217eb7

        // ---- Evaluate cases ----                                  // @0x217ec3
        std::vector<std::shared_ptr<EvalResults>> casesResult;
        if (hasCases()) {                                            // @0x217ec8
            casesResult = evalCases(subRes, condResult, ctx, state); // @0x217f30
        } else {
            // No cases — emit warning 0xd6.                         // @0x2185e2
            auto const& msg = ErrorMessages::messages.at(
                ErrorMessageT(0xd6));                                // @0x21865e
            ctx.messages->warningMessage(msg, -1);                   // @0x218677
            state.inSwitch_ = savedInSwitch;                         // @0x217e1f
            return result;
        }

        // ---- Merge condResult assemblers into result ----         // @0x217f8c
        result->assemblers_.insert(
            result->assemblers_.end(),
            condResult->assemblers_.begin(),
            condResult->assemblers_.end());

        // ---- Init three AsmLists ----                             // @0x217fce
        AsmList switchAsms;
        AsmList caseAsms;
        AsmList bodyAsms;

        // ---- asmBranchNode → push to result + copy node ----     // @0x218002
        AsmList::Asm branchAsm = ctx.asmCommands->asmBranchNode();   // @0x277950
        result->assemblers_.push_back(branchAsm);                    // @0x218023
        result->node_ = branchAsm.node;                              // @0x21870e

        // ---- branchMaySkipAllBodies = true ----                   // @0x218761
        result->node_->branchMaySkipAllBodies = true;

        // ---- Get switchReg and zeroReg ----                       // @0x218768
        int switchRegNum = res->getRegisterNumber();                 // @0x1e4bb0
        AsmRegister switchReg(switchRegNum);                         // @0x218776
        AsmRegister zeroReg(0);                                      // @0x2187a9

        // ---- Extract condition register from condResult ----
        // The binary uses the condition variable's register (e.g. R1 from addVar)
        // for the comparison addi instructions, NOT switchReg.
        AsmRegister condReg = condResult->values_.back().reg_;       // @0x218cf8 vicinity

        // ---- Compute total cycles ----                            // @0x21877b
        int cyclesF3 = Assembler::getCycles(
            Assembler::Command(0xf3000000));                         // @0x28fac0
        int cyclesB0 = Assembler::getCycles(
            Assembler::Command(0x40000000));                         // @0x28fac0
        int numCases = static_cast<int>(casesResult.size());
        int totalCycles = (numCases + 1) * (cyclesF3 + cyclesB0) - 4; // @0x2187c9

        // ---- addi(switchReg, zeroReg, Immediate(totalCycles)) → switchAsms // @0x2187e9
        {
            AsmList tmp = ctx.asmCommands->addi(
                switchReg, zeroReg, Immediate(totalCycles));          // @0x273d60
            switchAsms.insert(switchAsms.end(), tmp.begin(), tmp.end());
        }

        // ---- suser(switchReg, AddressImpl(0x1a)) → switchAsms --- // @0x21890c
        {
            AsmList::Asm suserAsm = ctx.asmCommands->suser(
                switchReg, detail::AddressImpl<unsigned int>(kSuserTriggerLoad));  // @0x274bc0
            switchAsms.push_back(suserAsm);                          // @0x21892e
        }

        // ---- result->returnEncountered_ = true ----                        // @0x2189e0
        result->returnEncountered_ = true;

        // ---- endLabel = newLabel("end") ----                      // @0x2189e4
        std::string endLabel = Resources::newLabel("end");           // @0x218a03

        // ---- Loop over casesResult entries ----                   // @0x218a28
        bool hasDefault = false;
        for (auto it = casesResult.begin(); it != casesResult.end(); ++it) {
            auto& caseEntry = *it;
            if (!caseEntry) continue;

            // Check: size==1 && (varType|2)==6 (Const/Cvar)         // @0x218ae7
            if (caseEntry &&
                caseEntry->values_.size() == 1 &&
                (caseEntry->values_.back().varType_ | 0x2) == 0x6)
            {
                // ---- Const/Cvar case entry ----                   // @0x218c80

                // Get new case register.                            // @0x218c8f
                int caseRegNum = res->getRegisterNumber();
                AsmRegister caseReg(caseRegNum);                     // @0x218c9d

                // Create case label.                                // @0x218ca2
                std::string caseLabel = Resources::newLabel("case"); // @0x218cc8
                //fprintf(stderr, "DBG case label: '%s' for varType=%d val=%d\n",
                //    caseLabel.c_str(),
                //    (int)caseEntry->values_.back().varType_,
                //    caseEntry->values_.back().value_.toInt());

                // Extract condResult last value → condValue.        // @0x218cf8
                auto& condValues = condResult->values_;
                Value condValue;
                if (!condValues.empty()) {
                    condValue = condValues.back().value_;
                }

                // Extract caseEntry last value → caseValue.         // @0x219092
                auto& caseValues = caseEntry->values_;
                Value caseValue;
                if (!caseValues.empty()) {
                    caseValue = caseValues.back().value_;
                }

                // addi(caseReg, condReg, Immediate(-caseValue.toInt())) → switchAsms // @0x219192
                {
                    int negCaseVal = -caseValue.toInt();              // @0x219199
                    AsmList tmp = ctx.asmCommands->addi(
                        caseReg, condReg, Immediate(negCaseVal));     // @0x2191c0
                    switchAsms.insert(switchAsms.end(), tmp.begin(), tmp.end());
                }

                // brz(caseReg, caseLabel, false) → switchAsms       // @0x2192fd
                {
                    AsmList::Asm brzAsm = ctx.asmCommands->brz(
                        caseReg, caseLabel, false);                  // @0x271e40
                    switchAsms.push_back(brzAsm);                    // @0x219302
                }

                // asmLabel(caseLabel) → bodyAsms                    // @0x2193d9
                {
                    AsmList::Asm labelAsm = ctx.asmCommands->asmLabel(
                        caseLabel);                                  // @0x2774e0
                    bodyAsms.push_back(labelAsm);                    // @0x2193eb
                }

                // Get another register for this case's body.        // @0x2194af
                int bodyRegNum = res->getRegisterNumber();
                AsmRegister bodyReg(bodyRegNum);                     // @0x2194bd
                AsmRegister bodyZeroReg(0);                          // @0x2194d6

                // readConst("AWG_WAIT_TRIGGER", EDirection(1)) → toInt // @0x219511
                EvalResultValue readRV = res->readConst("AWG_WAIT_TRIGGER",
                    EDirection(1));                                   // @0x1e7d70
                int readConstVal = readRV.value_.toInt();            // @0x21951d

                // addi(bodyReg, bodyZeroReg, Immediate(readConstVal)) → bodyAsms // @0x219547
                {
                    AsmList tmp = ctx.asmCommands->addi(
                        bodyReg, bodyZeroReg, Immediate(readConstVal));
                    bodyAsms.insert(bodyAsms.end(), tmp.begin(), tmp.end());
                }

                // wtrig(bodyReg, bodyReg) → bodyAsms                // @0x2196a6
                {
                    AsmList::Asm wtrigAsm = ctx.asmCommands->wtrig(
                        bodyReg, bodyReg);                           // @0x274f00
                    bodyAsms.push_back(wtrigAsm);                    // @0x2196b8
                }

                // Merge caseEntry assemblers → bodyAsms              // @0x21957a
                bodyAsms.insert(bodyAsms.end(),
                    caseEntry->assemblers_.begin(),
                    caseEntry->assemblers_.end());

                // brz(zeroReg, endLabel, true) → bodyAsms           // @0x219812
                {
                    AsmList::Asm brzEnd = ctx.asmCommands->brz(
                        AsmRegister(0), endLabel, true);             // @0x271e40
                    bodyAsms.push_back(brzEnd);                      // @0x219824
                }

                // emplace_back caseEntry->node_ to result->node_->branches // @0x2197cc
                result->node_->branches.emplace_back(caseEntry->node_);

                // AND hasError                                       // @0x2197d8
                result->returnEncountered_ &= caseEntry->returnEncountered_;

                hasDefault = false; // reset default tracking per iteration
            }
            else if (caseEntry &&
                     caseEntry->values_.size() == 1 &&
                     caseEntry->values_.back().varType_ == VarType_Unset)
            {
                // ---- Default case (varType==Unset/0) ----         // @0x218b18

                // branchMaySkipAllBodies = false                    // @0x218b23
                result->node_->branchMaySkipAllBodies = false;

                // Get register for default body.                    // @0x218b2a
                int defRegNum = res->getRegisterNumber();
                AsmRegister defReg(defRegNum);                       // @0x218b38
                AsmRegister defZeroReg(0);                           // @0x218b51

                // readConst("AWG_WAIT_TRIGGER", EDirection(1))   // @0x218b80
                EvalResultValue defReadRV = res->readConst("AWG_WAIT_TRIGGER",
                    EDirection(1));
                int defReadConstVal = defReadRV.value_.toInt();      // @0x218b91

                // addi → caseAsms                                   // @0x218bb9
                {
                    AsmList tmp = ctx.asmCommands->addi(
                        defReg, defZeroReg, Immediate(defReadConstVal));
                    caseAsms.insert(caseAsms.end(), tmp.begin(), tmp.end());
                }

                // wtrig(defReg, defReg) → caseAsms                  // @0x218dd7
                {
                    AsmList::Asm wtrigAsm = ctx.asmCommands->wtrig(
                        defReg, defReg);
                    caseAsms.push_back(wtrigAsm);
                }

                // Merge default case assemblers → caseAsms          // @0x218eac
                caseAsms.insert(caseAsms.end(),
                    caseEntry->assemblers_.begin(),
                    caseEntry->assemblers_.end());

                // brz(zeroReg, endLabel, true) → caseAsms           // @0x218f18
                {
                    AsmList::Asm brzEnd = ctx.asmCommands->brz(
                        AsmRegister(0), endLabel, true);
                    caseAsms.push_back(brzEnd);
                }

                // emplace_back to branches                          // @0x21900b
                result->node_->branches.emplace_back(caseEntry->node_);

                // Track hasError for next iteration                 // @0x21901a
                hasDefault = true;
            }
            else if (!caseEntry ||
                     caseEntry->values_.empty() ||
                     caseEntry->values_.size() > 1)
            {
                // Skip: no values or multi-valued (no-op for default-like)
                // branchMaySkipAllBodies = false for empty entries   // @0x218b18
                result->node_->branchMaySkipAllBodies = false;

                // Similar default handling — readConst + addi + merge + wtrig + brz
                int defRegNum = res->getRegisterNumber();
                AsmRegister defReg(defRegNum);
                AsmRegister defZeroReg(0);
                Value defReadVal = res->readConst("AWG_WAIT_TRIGGER",
                    EDirection(1)).value_;
                int defReadConstVal = defReadVal.toInt();
                {
                    AsmList tmp = ctx.asmCommands->addi(
                        defReg, defZeroReg, Immediate(defReadConstVal));
                    caseAsms.insert(caseAsms.end(), tmp.begin(), tmp.end());
                }
                {
                    AsmList::Asm wtrigAsm = ctx.asmCommands->wtrig(defReg, defReg);
                    caseAsms.push_back(wtrigAsm);
                }
                if (caseEntry) {
                    caseAsms.insert(caseAsms.end(),
                        caseEntry->assemblers_.begin(),
                        caseEntry->assemblers_.end());
                }
                {
                    AsmList::Asm brzEnd = ctx.asmCommands->brz(
                        AsmRegister(0), endLabel, true);
                    caseAsms.push_back(brzEnd);
                }
                if (caseEntry) {
                    result->node_->branches.emplace_back(caseEntry->node_);
                    result->returnEncountered_ &= caseEntry->returnEncountered_;
                }
                hasDefault = true;
            }
            else {
                // ---- Error: bad case value type ----              // @0x219023
                // varType is not Const/Cvar and not Unset → error 0x17
                auto const& msg = ErrorMessages::messages.at(
                    ErrorMessageT(0x17));                             // @0x21905c
                ctx.messages->errorMessage(msg, -1);                 // @0x219107
            }
        } // end for each case

        // ---- After loop: emit nop → switchAsms ----               // @0x218a48
        {
            AsmList::Asm nopAsm = ctx.asmCommands->nop();            // @0x275b30
            switchAsms.push_back(nopAsm);                            // @0x218a69
        }

        // ---- Create default label if there was a default ----
        std::string defaultLabel;
        if (hasDefault) {
            defaultLabel = Resources::newLabel("default");           // @0x2199ad
        }

        // ---- If no default: brz(zeroReg, endLabel, false) → switchAsms // @0x219a68
        if (!hasDefault) {
            AsmList::Asm brzEnd = ctx.asmCommands->brz(
                AsmRegister(0), endLabel, false);                    // @0x219a95
            switchAsms.push_back(brzEnd);                            // @0x219aa4
        } else {
            // brz(zeroReg, defaultLabel, false) → switchAsms        // @0x219a35
            AsmList::Asm brzDef = ctx.asmCommands->brz(
                AsmRegister(0), defaultLabel, false);
            switchAsms.push_back(brzDef);
        }

        // ---- Insert bodyAsms → switchAsms ----                    // @0x219b69
        switchAsms.insert(switchAsms.end(), bodyAsms.begin(), bodyAsms.end());

        // ---- If default exists: asmLabel(defaultLabel) → switchAsms // @0x219bb0
        if (hasDefault) {
            AsmList::Asm defLabelAsm = ctx.asmCommands->asmLabel(defaultLabel);
            switchAsms.push_back(defLabelAsm);
        }

        // ---- Insert caseAsms → switchAsms ----                    // @0x219c7c
        switchAsms.insert(switchAsms.end(), caseAsms.begin(), caseAsms.end());

        // ---- asmLabel(endLabel) → switchAsms ----                 // @0x219cb3
        {
            AsmList::Asm endLabelAsm = ctx.asmCommands->asmLabel(endLabel);
            switchAsms.push_back(endLabelAsm);
        }

        // ---- Merge switchAsms into result->assemblers_ ----       // @0x219d7b
        result->assemblers_.insert(
            result->assemblers_.end(),
            switchAsms.begin(),
            switchAsms.end());

        // ---- AND hasError with overall ----                       // @0x219db6
        result->returnEncountered_ &= result->returnEncountered_;

        // ---- Cleanup + jump to epilogue ----                      // @0x21a074
        state.inSwitch_ = savedInSwitch;
        return result;
    }

    // ================================================================
    // Const/Cvar path ((varType | 2) == 6)                          // @0x218083
    // ================================================================
    if ((condVal.varType_ | 0x2) == 0x6) {
        // ---- Set scopeBoundaryFlags_ = 1 ----                               // @0x218099
        subRes->setAtScopeBoundary(true);

        // ---- Evaluate cases ----                                  // @0x2180a7
        std::vector<std::shared_ptr<EvalResults>> casesResult;
        if (hasCases()) {                                            // @0x2180ac
            casesResult = evalCases(subRes, condResult, ctx, state); // @0x218114
        } else {
            // No cases — emit warning 0xd6.                         // @0x218624
            auto const& msg = ErrorMessages::messages.at(
                ErrorMessageT(0xd6));
            ctx.messages->warningMessage(msg, -1);                   // @0x218677
            state.inSwitch_ = savedInSwitch;
            return result;
        }

        // ---- Reset scopeBoundaryFlags_ = 0 ----                             // @0x218177
        subRes->setAtScopeBoundary(false);

        // ---- Init matchedResult variables ----                     // @0x21817e
        // Binary uses two separate shared_ptrs:
        //   -0x50(%rbp) = matchedConstResult (set at 0x218498 on Value::operator== match)
        //   -0x90(%rbp) = matchedDefaultResult (set at 0x218270 for default/non-Const cases)
        std::shared_ptr<EvalResults> matchedConstResult;   // -0x50(%rbp)
        std::shared_ptr<EvalResults> matchedDefaultResult;  // -0x90(%rbp)
        bool matchedHasError = false;

        // ---- Check state.inFunctionDef_ for conditional AND ----          // @0x21818c
        bool useInFunctionDef = (state.inFunctionDef_ == 1);
        if (useInFunctionDef) {
            result->returnEncountered_ = true;                                // @0x21819a
        }

        // ---- Loop over casesResult entries ----                   // @0x21819e
        for (auto& caseEntry : casesResult) {
            if (!caseEntry) continue;

            // Check: size==1 && (varType|2)==6 (Const/Cvar)         // @0x2181e9
            if (caseEntry->values_.size() == 1 &&
                (caseEntry->values_.back().varType_ | 0x2) == 0x6)
            {
                // Extract condResult value.                         // @0x218221
                Value condValue;
                auto& condVals = condResult->values_;
                if (!condVals.empty()) {
                    condValue = condVals.back().value_;
                }

                // Extract caseEntry value.                          // @0x218376
                Value caseValue;
                auto& caseVals = caseEntry->values_;
                if (!caseVals.empty()) {
                    caseValue = caseVals.back().value_;
                }

                // Compare: Value::operator==                        // @0x218429
                if (condValue == caseValue) {                        // @0x218494
                    // Match found — assign to -0x50(%rbp).          // @0x218498
                    matchedConstResult = caseEntry;
                }

                // AND hasError for all cases regardless.            // @0x2184da
                result->returnEncountered_ &= caseEntry->returnEncountered_;
            }
            else {
                // Default/unmatched case — assign to -0x90(%rbp).   // @0x218270
                matchedDefaultResult = caseEntry;
                matchedHasError = caseEntry->returnEncountered_;
            }
        } // end for

        // ---- After loop: prefer matchedConstResult, fallback to matchedDefaultResult ---- // @0x2184fc
        // Binary: mov -0x50(%rbp),%r14; test %r14,%r14; je ...
        // If numbered match found, use it; otherwise use default.
        std::shared_ptr<EvalResults> matchedResult =
            matchedConstResult ? matchedConstResult : matchedDefaultResult;

        if (matchedResult) {
            // Merge matchedResult assemblers into result.           // @0x218509
            result->assemblers_.insert(
                result->assemblers_.end(),
                matchedResult->assemblers_.begin(),
                matchedResult->assemblers_.end());

            // Check hasError for setValue.                          // @0x218544
            if (matchedResult->returnEncountered_) {                          // @0x21854b
                Value retVal = res->getReturnValue();                // @0x218560
                result->setValue(retVal);                             // @0x21856f
                if (!state.inFunctionDef_) {                                 // @0x218580
                    result->returnEncountered_ = true;                        // @0x21a17a
                    // Set hasError and skip node_ copy:
                    // r14=0 → r15=false → skip to cleanup
                    state.inSwitch_ = savedInSwitch;
                    return result;
                }
            }

            // Copy node_ from matchedResult.                        // @0x21858b
            result->node_ = matchedResult->node_;                    // @0x2185a5

            // Check state.inFunctionDef_ for final hasError AND.            // @0x21a12e
            if (useInFunctionDef) {
                result->returnEncountered_ &= matchedHasError;               // @0x21a13b
            }
        } else {
            // No match found — no assemblers, but check state.inFunctionDef_
            if (useInFunctionDef) {                                          // @0x21a12e
                result->returnEncountered_ &= matchedHasError;
            }
        }

        state.inSwitch_ = savedInSwitch;
        return result;
    }

    // ================================================================
    // Error path: unsupported varType                               // @0x217dcc
    // ================================================================
    {
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "switch");
        ctx.messages->errorMessage(msg, -1);
    }

    // ---- Epilogue ----                                            // @0x217e1b
    state.inSwitch_ = savedInSwitch;                                 // @0x217e1f
    return result;
}

// SeqCWhileLoop::evaluate(3) — @0x21e130, 7117B
//   While loop evaluation: two-path structure.
//
//   Binary address annotations:
//     0x21e130–0x21e17b  Prologue: setLineNr on messages, asmCommands, wavetable
//     0x21e17b–0x21e1cc  make_shared<EvalResults>() → result
//     0x21e1cc–0x21e224  createSubScope("while") → subRes
//     0x21e224–0x21e28c  cond()->evaluate(subRes, ctx, state) → condResult
//     0x21e28c–0x21e51b  Null check: condResult → errorMessage(0x27, "while")
//     0x21e297–0x21e3ad  newLabel("while") → asmLabel → push to result
//     0x21e3c0–0x21e3f5  Merge condResult->assemblers_ into result
//     0x21e3f5–0x21e5c5  Build name: "while (" + condResult->name_ + ")"
//     0x21e5ca–0x21e618  Dispatch: size==1 && varType==Cvar → Cvar path
//     0x21e618–0x21ee6c  Cvar path: compile-time loop unrolling
//     0x21ee6c–0x21f70f  Var path: runtime loop with asm branches
//     0x21f70f–0x21f7d5  Shared cleanup: hasError, return
//
//   Cvar path (0x21e618–0x21ee6c):
//     Iteratively evaluates the body in "unroll" sub-scopes, extracting the
//     condition value via toInt() each iteration. Builds a linked node chain
//     via Node::next. Exits on: condition==0, body hasError, null condResult
//     re-evaluation, or iteration count exceeding ctx.loopUnrollLimit (error
//     0x7b from ErrorMessages::messages BST).
//
//   Var path (0x21ee6c–0x21f70f):
//     Creates "loop" and "end" labels. Emits jumpIfZero → endLabel, then
//     asmLoopNode (with loopBodyRunsAtLeastOnce = jumpAsms.empty()),
//     asmLabel(loopLabel), setState(Active), evaluates body in "while-body"
//     sub-scope, br(loopLabel, false), asmLabel(endLabel).
//     Calls loopArgNodeAppend and loopBodyNodeAppend for node tree linkage.
std::shared_ptr<EvalResults> SeqCWhileLoop::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // ---- Prologue: set lineNr on subsystems ----                  // @0x21e158
    const int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);                                 // @0x21e162
    ctx.asmCommands->setWavetableFrontIndex(lineNr);                 // @0x21e16b
    ctx.wavetable->setLineNr(lineNr);                                // @0x21e176

    // ---- Allocate result ----                                     // @0x21e17b
    auto result = std::make_shared<EvalResults>();

    // ---- Create sub-scope("while") for condition ----             // @0x21e1cc
    auto subRes = res->createSubScope("while");                      // @0x1e36a0

    // ---- Evaluate condition ----                                  // @0x21e224
    auto condResult = cond()->evaluate(subRes, ctx, state);          // @0x2033b0

    // ---- Null condResult check ----                               // @0x21e28c
    if (!condResult) {
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "while");                           // @0x21e4f5
        ctx.messages->errorMessage(msg, -1);                         // @0x21e509
        return result;
    }

    // ---- Create while label, push asmLabel ----                   // @0x21e297
    std::string whileLabel = Resources::newLabel("while");           // @0x1ec6b0
    AsmList::Asm whileLabelAsm =
        ctx.asmCommands->asmLabel(whileLabel);                       // @0x2774e0
    result->assemblers_.push_back(whileLabelAsm);                    // @0x21e303

    // ---- Merge condResult assemblers into result ----             // @0x21e3c0
    result->assemblers_.insert(
        result->assemblers_.end(),
        condResult->assemblers_.begin(),
        condResult->assemblers_.end());                              // @0x21e3f0

    // ---- Build name: "while (" + condResult->name_ + ")" ----    // @0x21e3f5
    result->name_ = "while (" + condResult->name_ + ")";            // @0x21e559

    // ---- Initialize bodyResult for shared cleanup path ----
    std::shared_ptr<EvalResults> bodyResult;

    // ---- Dispatch: Cvar vs Var ----                               // @0x21e5ca
    if (condResult->values_.size() == 1 &&
        condResult->values_.back().varType_ == VarType_Cvar)         // @0x21e60e
    {
        // ============================================================
        // Cvar path: compile-time loop unrolling                   // @0x21e618
        // ============================================================
        std::shared_ptr<Node> accumulatedNode;                       // tracks root of chained nodes
        int iterCount = 0;                                           // [rbp-0x190]
        bool normalExit = false;  // true if exited by toInt()==0 or body hasError

        for (;;) {                                                   // loop top @0x21e649
            // Extract condition value → toInt()                     // @0x21e649-21e730
            Value condVal = condResult->values_.back().value_;
            if (condVal.toInt() == 0) {                              // @0x21e774
                normalExit = true;
                break;  // condition false → exit loop               // → @0x21f803
            }

            // Evaluate body in "unroll" sub-scope                   // @0x21e77c
            auto unrollScope = subRes->createSubScope("unroll");     // @0x21e7bd
            auto bodyEval = body()->evaluate(unrollScope, ctx, state); // @0x21e7dc

            // Node chaining: append bodyEval's node to accumulated chain
            if (bodyEval) {                                          // @0x21e827
                if (bodyResult && bodyResult->node_) {               // @0x21e830-21e844
                    // Walk bodyResult->node_->next chain to leaf    // @0x21e85d-21e877
                    auto current = bodyResult->node_;
                    while (current->next) current = current->next;   // @0x21e870
                    // Attach bodyEval->node_ at leaf                // @0x21e8c0-21e8e4
                    current->next = bodyEval->node_;
                }
            }

            // Update accumulated bodyResult                         // @0x21e950-21e97e
            bodyResult = bodyEval;

            if (bodyResult) {                                        // @0x21e9a7
                // Initialize accumulatedNode on first non-null      // @0x21e942
                if (!accumulatedNode) {
                    accumulatedNode = bodyResult->node_;              // @0x21ed1b-21ed38
                }

                // Merge assemblers into result                      // @0x21e9b7-21e9df
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    bodyResult->assemblers_.begin(),
                    bodyResult->assemblers_.end());

                // Check hasError on body result                     // @0x21e9e3
                if (bodyResult->returnEncountered_) {
                    // Extract value and set on result               // @0x21e9f8-21ede0
                    result->setValue(bodyResult->getValue());         // @0x21edec
                    normalExit = true;
                    break;  // r14d = 3 → exit, still copy node      // → @0x21f7f9
                }
            }

            // Re-evaluate condition                                 // @0x21ea30
            condResult = cond()->evaluate(subRes, ctx, state);       // @0x21ea79

            // Null condResult → error                               // @0x21eb05
            if (!condResult) {
                std::string msg = ErrorMessages::format(
                    ErrorMessageT(0x27), "while");                   // @0x21ec62
                ctx.messages->errorMessage(msg, cond()->lineNr());     // @0x21ec79
                break;  // normalExit stays false → no node copy     // r14d = 1
            }

            // Iteration limit check                                 // @0x21eb1a
            if (iterCount > ctx.loopUnrollLimit) {                   // @0x21eb1e
                // Error 0x7b: too many iterations (BST lookup)      // @0x21eb24
                auto const& msgs = ErrorMessages::messages;
                auto it = msgs.find(0x7b);
                if (it != msgs.end()) {
                    ctx.messages->errorMessage(it->second, -1);      // @0x21ebd9
                }
                break;  // normalExit stays false                    // r14d = 1
            }
            iterCount++;                                             // @0x21eba6
        }

        // ---- Post-loop: copy accumulated node into result ----    // @0x21f803
        if (normalExit && bodyResult) {                              // @0x21f80a-21f80d
            if (accumulatedNode) {                                   // @0x21f816
                result->node_ = accumulatedNode;                     // @0x21f830
            } else {
                result->node_ = bodyResult->node_;                   // @0x21f858
            }
        }

        // ---- hasError copy ----                                   // @0x21f70f
        if (normalExit) {
            result->returnEncountered_ = bodyResult
                ? bodyResult->returnEncountered_ : false;                     // @0x21f718-21f724
        }
    }
    else
    {
        // ============================================================
        // Var path: runtime loop with asm branches                 // @0x21ee6c
        // ============================================================

        // Create loop and end labels                                // @0x21ee6c-21eed6
        std::string loopLabel = Resources::newLabel("loop");         // @0x21ee92
        std::string endLabel = Resources::newLabel("end");           // @0x21eed6

        // Save condResult for later loopArgNodeAppend               // @0x21eefb
        auto condResultSaved = condResult;                           // @0x21ef03-21ef16

        // jumpIfZero → skip to endLabel if condition is zero        // @0x21ef29
        auto jumpAsms = jumpIfZero(condResult, endLabel, ctx);       // @0x2149f0

        // Merge jump instructions into result                       // @0x21ef60-21ef97
        result->assemblers_.insert(
            result->assemblers_.end(),
            jumpAsms.begin(),
            jumpAsms.end());

        // asmLoopNode — create loop node asm entry                  // @0x21efa7
        AsmList::Asm loopNodeAsm =
            ctx.asmCommands->asmLoopNode();                          // @0x277ad0

        // Set loopBodyRunsAtLeastOnce if jumpIfZero emitted nothing // @0x21efac-21efbe
        loopNodeAsm.node->loopBodyRunsAtLeastOnce = jumpAsms.empty();

        // Push loopNodeAsm and copy its node into result            // @0x21efc5
        result->assemblers_.push_back(loopNodeAsm);
        result->node_ = loopNodeAsm.node;                           // @0x21f04f

        // asmLabel(loopLabel) → push                                // @0x21f082-21f098
        AsmList::Asm loopLabelAsm =
            ctx.asmCommands->asmLabel(loopLabel);                    // @0x2774e0
        result->assemblers_.push_back(loopLabelAsm);                 // @0x21f09d

        // setState(Active) on sub-scope                             // @0x21f166-21f172
        subRes->setState(Resources::State::Active);                  // @0x1e35f0

        // Evaluate body (if present)                                // @0x21f177
        if (body()) {                                                // @0x203410
            // Create "while-body" sub-scope                         // @0x21f189
            auto bodySubScope =
                subRes->createSubScope("while-body");                // @0x21f1d2
            bodyResult =
                body()->evaluate(bodySubScope, ctx, state);          // @0x21f1f5

            if (bodyResult) {                                        // @0x21f2b6
                // Merge body assemblers into result                 // @0x21f2c2
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    bodyResult->assemblers_.begin(),
                    bodyResult->assemblers_.end());                  // @0x21f2e6
            }
        }

        // Unconditional branch back to whileLabel (not loopLabel)   // @0x21f2f5
        // GDB-verified: br() uses [rbp-0x160] = whileLabel, not loopLabel
        AsmList::Asm brAsm =
            ctx.asmCommands->br(whileLabel, false);                  // @0x271df0
        result->assemblers_.push_back(brAsm);                        // @0x21f30a

        // End label                                                 // @0x21f3d7
        AsmList::Asm endLabelAsm =
            ctx.asmCommands->asmLabel(endLabel);                     // @0x2774e0
        result->assemblers_.push_back(endLabelAsm);                  // @0x21f3ee

        // Node tree linkage                                         // @0x21f4b7
        if (condResultSaved) {
            loopArgNodeAppend(result->node_, condResultSaved->node_); // @0x21f508
        }
        if (bodyResult) {
            loopBodyNodeAppend(result->node_, bodyResult->node_);    // @0x21f5a7
        }

        // Copy hasError from bodyResult                             // @0x21f70f
        result->returnEncountered_ = bodyResult
            ? bodyResult->returnEncountered_ : false;                         // @0x21f718-21f724
    }

    return result;                                                   // @0x21f7c1
}

// SeqCDoWhile::evaluate(3) — @0x21fd00, 7952B
//   Do-while loop evaluation: two-path structure (body-first).
//
//   Binary address annotations:
//     0x21fd00–0x21fd4a  Prologue: setLineNr on messages, asmCommands, wavetable
//     0x21fd4a–0x21fdb5  make_shared<EvalResults>() → result
//     0x21fdbc–0x21fe92  Three labels: newLabel("do"), newLabel("while"), newLabel("end")
//     0x21fe97–0x21ff61  asmLabel(doLabel) → push to result
//     0x21ff6d–0x21ffc0  createSubScope("dowhile") → subRes
//     0x21ffc5–0x21ffcc  setAtScopeBoundary(true)
//     0x21ffd3–0x220060  cond()->evaluate → condResult, null check → error(0x27, "do")
//     0x220066–0x2200a2  Cvar precheck: skip setState if size==1 && Cvar
//     0x2200a7–0x2200b8  Reset atScopeBoundary, init bodyResult
//     0x2200bf–0x2201e6  First body evaluation in "dowhile-body" scope + merge assemblers
//     0x2201eb–0x22029c  Re-evaluate cond → fresh condResult
//     0x22029c–0x220436  asmLabel(whileLabel) → push to result
//     0x220442–0x220475  Dispatch: size==1 && varType==Cvar → Cvar path
//     0x22047b–0x220cb2  Cvar path: compile-time unrolling (body first, cond at bottom)
//     0x220cb9–0x2213bb  Var path: runtime loop with asm branches
//     0x221108–0x2216d3  Shared epilogue: hasError, name ("while (...)")
//     0x2216d6–0x22181b  Cleanup: label strings, shared_ptrs, return
//
//   Cvar path (0x22047b–0x220cb2):
//     Iteratively evaluates body in "unroll" sub-scopes, chaining nodes.
//     Condition checked at bottom via toDouble() + floatEqual(0.0).
//     Exits on: condition==0, body hasError, null condResult re-evaluation,
//     or iteration count exceeding ctx.loopUnrollLimit (error 0x7b).
//
//   Var path (0x220cb9–0x2213bb):
//     Merges condResult assemblers, emits asmLoopNode with
//     loopBodyRunsAtLeastOnce=true (always for do-while). Dispatches on
//     condResult type: Var → brnz(reg, doLabel), Const/Cvar → toInt()
//     non-zero → br(doLabel). Bad type/count → error 0x7e.
//     Then asmLabel(endLabel), loopArgNodeAppend, loopBodyNodeAppend.
std::shared_ptr<EvalResults> SeqCDoWhile::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // ---- Prologue: set lineNr on subsystems ----                  // @0x21fd2b
    const int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);                                 // @0x21fd33
    ctx.asmCommands->setWavetableFrontIndex(lineNr);                 // @0x21fd38
    ctx.wavetable->setLineNr(lineNr);                                // @0x21fd45

    // ---- Allocate result ----                                     // @0x21fd4a
    auto result = std::make_shared<EvalResults>();

    // ---- Three labels: do, while, end ----                        // @0x21fdbc
    std::string doLabel    = Resources::newLabel("do");               // @0x21fde1
    std::string whileLabel = Resources::newLabel("while");            // @0x21fe27
    std::string endLabel   = Resources::newLabel("end");              // @0x21fe6b

    // ---- asmLabel(doLabel) → push to result ----                  // @0x21fea2
    AsmList::Asm doLabelAsm =
        ctx.asmCommands->asmLabel(doLabel);                           // @0x2774e0
    result->assemblers_.push_back(doLabelAsm);                        // @0x21feba

    // ---- Create sub-scope("dowhile") ----                         // @0x21ff92
    auto subRes = res->createSubScope("dowhile");                     // @0x1e36a0

    // ---- Set atScopeBoundary = true ----                          // @0x21ffc5
    subRes->setAtScopeBoundary(true);

    // ---- Evaluate condition initially ----                        // @0x21ffd3
    auto condResult = cond()->evaluate(subRes, ctx, state);           // @0x21fff4

    // ---- Null condResult check ----                               // @0x22005d
    if (!condResult) {
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "do");                               // @0x22033e
        ctx.messages->errorMessage(msg, -1);                          // @0x220356
        return result;
    }

    // ---- Cvar precheck: skip setState if Cvar ----                // @0x220066
    if (!(condResult->values_.size() == 1 &&
          condResult->values_.back().varType_ == VarType_Cvar))       // @0x220090
    {
        subRes->setState(Resources::State::Active);                   // @0x2200a2
    }

    // ---- Reset atScopeBoundary = false ----                       // @0x2200a7
    subRes->setAtScopeBoundary(false);

    // ---- Initialize bodyResult ----                               // @0x2200b8
    std::shared_ptr<EvalResults> bodyResult;

    // ---- First body evaluation (do-while: body runs first) ----   // @0x2200bf
    if (body()) {
        auto bodySubScope =
            subRes->createSubScope("dowhile-body");                   // @0x220113
        bodyResult =
            body()->evaluate(bodySubScope, ctx, state);               // @0x22014b
    }

    // ---- Merge first body assemblers into result ----             // @0x2201b4
    if (bodyResult) {
        result->assemblers_.insert(
            result->assemblers_.end(),
            bodyResult->assemblers_.begin(),
            bodyResult->assemblers_.end());                           // @0x2201e6
    }

    // ---- Re-evaluate condition (result discarded) ----              // @0x2201eb
    // GDB-verified: the binary stores the second eval's result in a
    // temporary at [rbp-0x300] but NEVER moves it to the condResult
    // slot at [rbp-0xf0]. The first eval's condResult is the one
    // used at the Cvar/Var dispatch below. The second eval exists
    // only for its side effects (e.g., register allocation).
    {
        auto condResult2 = cond()->evaluate(subRes, ctx, state);      // @0x22023b
        (void)condResult2;  // intentionally discarded
    }

    // ---- asmLabel(whileLabel) → push to result ----               // @0x22029c
    AsmList::Asm whileLabelAsm =
        ctx.asmCommands->asmLabel(whileLabel);                        // @0x2774e0
    result->assemblers_.push_back(whileLabelAsm);                     // @0x2202b7

    // ---- Dispatch: Cvar vs Var ----                               // @0x220442
    bool doEpilogue = true;  // controls hasError + name building

    if (condResult &&
        condResult->values_.size() == 1 &&
        condResult->values_.back().varType_ == VarType_Cvar)          // @0x220471
    {
        // ============================================================
        // Cvar path: compile-time loop unrolling (body-first)       // @0x22047b
        // ============================================================
        std::shared_ptr<Node> accumulatedNode;                        // [rbp-0x2f0]
        int iterCount = 0;                                            // [rbp-0x170]
        doEpilogue = false;  // only set true on success exits

        for (;;) {                                                    // loop top @0x2204a8
            // Evaluate body in "unroll" sub-scope                   // @0x2204a8
            std::shared_ptr<EvalResults> bodyEval;
            if (body()) {
                auto unrollScope =
                    subRes->createSubScope("unroll");                 // @0x220501
                bodyEval =
                    body()->evaluate(unrollScope, ctx, state);        // @0x220520
            }

            // Node chaining + accumulatedNode tracking              // @0x220571
            if (bodyEval && bodyResult && bodyResult->node_) {
                // Walk old bodyResult->node_->next chain to leaf    // @0x2205b3
                auto current = bodyResult->node_;
                while (current->next) current = current->next;        // @0x2206e0
                // Attach bodyEval->node_ at leaf                    // @0x2205ca
                current->next = bodyEval->node_;

                // Initialize accumulatedNode from old bodyResult    // @0x22066d
                if (!accumulatedNode) {
                    accumulatedNode = bodyResult->node_;               // @0x22067b
                }
            }

            // Update bodyResult                                     // @0x22074a
            bodyResult = bodyEval;

            // Merge assemblers + check hasError                     // @0x2207b6
            if (bodyResult) {
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    bodyResult->assemblers_.begin(),
                    bodyResult->assemblers_.end());                   // @0x2207e4

                if (bodyResult->returnEncountered_) {                          // @0x2207e9
                    result->setValue(bodyResult->getValue());          // @0x220b00
                    doEpilogue = true;
                    break;  // exit with hasError (r12d = 2)
                }
            }

            // Re-evaluate condition                                 // @0x220840
            condResult = cond()->evaluate(subRes, ctx, state);

            // Null condResult → error                               // @0x22091a
            if (!condResult) {
                std::string msg = ErrorMessages::format(
                    ErrorMessageT(0x27), "do");                       // @0x220a14
                ctx.messages->errorMessage(msg, -1);                  // @0x220a28
                break;  // doEpilogue stays false
            }

            // Iteration limit check                                 // @0x220928
            if (iterCount > ctx.loopUnrollLimit) {                    // @0x220932
                auto const& msgs = ErrorMessages::messages;
                auto it = msgs.find(0x7b);
                if (it != msgs.end()) {
                    ctx.messages->errorMessage(it->second, -1);       // @0x220993
                }
                break;  // doEpilogue stays false
            }
            iterCount++;                                              // @0x22098a

            // Extract condition value → toDouble() + floatEqual     // @0x220b81
            Value condVal = condResult->values_.back().value_;
            if (floatEqual(condVal.toDouble(), 0.0)) {                // @0x220c69
                doEpilogue = true;
                break;  // condition false → exit loop
            }
        }

        // ---- Post-loop: copy accumulated node ----                // @0x220ff7
        if (doEpilogue && bodyResult) {
            if (accumulatedNode) {
                result->node_ = accumulatedNode;                      // @0x221016
            } else {
                result->node_ = bodyResult->node_;                    // @0x221093
            }
        }
    }
    else
    {
        // ============================================================
        // Var path: runtime loop with asm branches                 // @0x220cb9
        // ============================================================

        // Merge condResult assemblers into result                   // @0x220cb9
        if (condResult) {
            result->assemblers_.insert(
                result->assemblers_.end(),
                condResult->assemblers_.begin(),
                condResult->assemblers_.end());                       // @0x220ce8
        }

        // asmLoopNode — create loop node asm entry                  // @0x220ced
        AsmList::Asm loopNodeAsm =
            ctx.asmCommands->asmLoopNode();                           // @0x277ad0

        // loopBodyRunsAtLeastOnce = true (always for do-while)      // @0x220d04
        loopNodeAsm.node->loopBodyRunsAtLeastOnce = true;

        // Push loopNodeAsm and copy its node into result            // @0x220d0b
        result->assemblers_.push_back(loopNodeAsm);
        result->node_ = loopNodeAsm.node;                            // @0x220d88

        // Save condResult for loopArgNodeAppend                     // @0x220ddc
        auto condResultSaved = condResult;

        // ---- Jump dispatch: branch back to doLabel if cond true --
        std::vector<AsmList::Asm> jumpAsms;                           // @0x220e0c

        if (condResult) {                                             // @0x220e1e
            if (condResult->values_.size() == 1) {                    // @0x220e35
                auto varType =
                    condResult->values_.back().varType_;               // @0x220f6f

                if (varType == VarType_Var) {                         // @0x220f7c
                    // brnz(reg, doLabel, false)                     // @0x220f85
                    AsmList::Asm brnzAsm = ctx.asmCommands->brnz(
                        condResult->values_.back().reg_,
                        doLabel, false);                               // @0x271f30
                    jumpAsms.push_back(brnzAsm);
                }
                else if (varType == VarType_Const ||
                         varType == VarType_Cvar)                     // @0x220f72
                {
                    // Extract value → toInt()                       // @0x220fbb
                    Value condVal =
                        condResult->values_.back().value_;
                    int intVal = condVal.toInt();                      // @0x221141
                    if (intVal != 0) {                                 // @0x221189
                        // Non-zero → br(doLabel, false)             // @0x221191
                        AsmList::Asm brAsm =
                            ctx.asmCommands->br(doLabel, false);
                        jumpAsms.push_back(brAsm);
                    }
                }
                else {
                    // Bad type → error 0x7e                         // @0x220e3f
                    auto const& msgs = ErrorMessages::messages;
                    auto it = msgs.find(0x7e);
                    if (it != msgs.end()) {
                        ctx.messages->errorMessage(it->second, -1);   // @0x220f5c
                    }
                }
            }
            else if (condResult->values_.size() > 1) {
                // Multiple values → error 0x7e                      // @0x220e3f
                auto const& msgs = ErrorMessages::messages;
                auto it = msgs.find(0x7e);
                if (it != msgs.end()) {
                    ctx.messages->errorMessage(it->second, -1);
                }
            }
        }

        // Merge jump asms into result                               // @0x2211f8
        result->assemblers_.insert(
            result->assemblers_.end(),
            jumpAsms.begin(),
            jumpAsms.end());

        // asmLabel(endLabel) → push to result                       // @0x2212ee
        AsmList::Asm endLabelAsm =
            ctx.asmCommands->asmLabel(endLabel);                      // @0x2774e0
        result->assemblers_.push_back(endLabelAsm);                   // @0x221309

        // Node tree linkage                                         // @0x2213c0
        if (condResultSaved) {
            loopArgNodeAppend(result->node_,
                              condResultSaved->node_);                // @0x221414
        }
        if (bodyResult) {
            loopBodyNodeAppend(result->node_,
                               bodyResult->node_);                    // @0x2214b6
        }
    }

    // ---- Shared epilogue: hasError + name ----                    // @0x221108
    if (doEpilogue) {
        result->returnEncountered_ = bodyResult
            ? bodyResult->returnEncountered_ : false;                          // @0x221561

        // Build name: "while (" + condResult->name_ + ")"          // @0x221568
        if (condResult) {
            result->name_ = "while (" + condResult->name_ + ")";     // @0x221666
        }
    }

    return result;                                                    // @0x22181b
}

// SeqCRepeat::evaluate(3) — @0x221c10, 8567B
//   Repeat loop evaluation: two-path structure.
//
//   Binary address annotations:
//     0x221c10–0x221c5f  Prologue: setLineNr on messages, asmCommands, wavetable
//     0x221c5f–0x221cab  make_shared<EvalResults>() → result
//     0x221cab–0x221cee  createSubScope("repeat") → subRes
//     0x221d0e–0x221d78  count()->evaluate(subRes, ctx, state) → countResult
//     0x221d78–0x221e75  Null check → errorMessage(0x27, "repeat")
//     0x221f2c–0x221f5c  Merge countResult assemblers into result
//     0x221f68–0x2220db  Build name: "repeat(" + countResult->name_ + ")"
//     0x2220db–0x2220e9  getRegisterNumber() → AsmRegister
//     0x2220ee–0x2222c3  Dispatch: size==1 → Cvar/Const toInt(), Var path
//     0x22212b–0x222c53  Var path: runtime loop with counter register
//     0x222f2a–0x2238dd  Cvar path: compile-time loop unrolling
//     0x222e0e–0x222ef6  Shared epilogue: hasError, return
//
//   Var path (0x22212b–0x222c53):
//     Creates "loop" and optionally "end" labels. Emits brz(register, endLabel)
//     for the initial count check. asmLabel(loopLabel), asmLoopNode() with
//     loopBodyRunsAtLeastOnce = !hasEndLabel. setState(Active), evaluate body
//     in sub-scope. addi(register, register, -1) for counter decrement,
//     brgz(register, loopLabel) to loop back. asmLabel(endLabel).
//     loopArgNodeAppend + loopBodyNodeAppend for node linkage.
//
//   Cvar path (0x222f2a–0x2238dd):
//     Extracts count via toInt(). If <0 → error 0xb7. If >= 2, checks vs
//     ctx.loopUnrollLimit (error 0x7b). First body eval in "maybe_unroll"
//     scope with atScopeBoundary toggling + lineNr save/restore. Then loops
//     remaining iterations in "unroll" scopes, chaining nodes. Copies
//     accumulatedNode or bodyResult->node_ into result.
std::shared_ptr<EvalResults> SeqCRepeat::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // ---- Prologue: set lineNr on subsystems ----                  // @0x221c37
    const int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);                                 // @0x221c41
    ctx.asmCommands->setWavetableFrontIndex(lineNr);                 // @0x221c46
    ctx.wavetable->setLineNr(lineNr);                                // @0x221c52

    // ---- Allocate result ----                                     // @0x221c5a
    auto result = std::make_shared<EvalResults>();

    // ---- Create sub-scope("repeat") for count eval ----           // @0x221cab
    auto subRes = res->createSubScope("repeat");                     // @0x1e36a0

    // ---- Evaluate count() ----                                    // @0x221d0e
    auto countResult = count()->evaluate(subRes, ctx, state);

    // ---- Null countResult check ----                              // @0x221d78
    if (!countResult) {
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "repeat");                          // @0x221e21
        ctx.messages->errorMessage(msg, -1);                         // @0x221e47
        return result;
    }

    // ---- Merge countResult assemblers into result ----            // @0x221f2c
    result->assemblers_.insert(
        result->assemblers_.end(),
        countResult->assemblers_.begin(),
        countResult->assemblers_.end());                             // @0x221f5c

    // ---- Build name: "repeat(" + countResult->name_ + ")" ----   // @0x221f68
    result->name_ = "repeat(" + countResult->name_ + ")";           // @0x2220db

    // ---- Get register for counter ----                            // @0x2220db
    AsmRegister counterReg(Resources::getRegisterNumber());          // @0x2220e9

    // ---- Initialize bodyResult for shared cleanup path ----
    std::shared_ptr<EvalResults> bodyResult;

    // ---- Dispatch: Cvar/Const vs Var ----                         // @0x2220ee
    bool hasEndLabel = true;  // controls brz emission + endLabel    // [rbp-0x110]

    if (countResult->values_.size() == 1 &&
        (countResult->values_.back().varType_ == VarType_Cvar ||
         countResult->values_.back().varType_ == VarType_Const))     // @0x2222c3
    {
        // ============================================================
        // Cvar/Const path: extract count via toInt()                // @0x2222d5
        // ============================================================
        Value countVal = countResult->values_.back().value_;
        int countInt = countVal.toInt();                              // @0x15c250

        if (countInt < 0) {                                          // @0x221f1d → @0x2226e4
            // Negative repeat count → error 0xb7                    // @0x2226e4
            auto const& msgs = ErrorMessages::messages;
            auto it = msgs.find(0xb7);
            if (it != msgs.end()) {
                ctx.messages->errorMessage(it->second, -1);          // @0x222737
            }
            return result;                                           // → @0x222e98
        }

        // ---- Cvar unrolling path ----                             // @0x222f76
        std::shared_ptr<Node> accumulatedNode;
        bodyResult.reset();

        if (countInt < 2) {                                          // @0x222f76
            // 0 or 1 iterations — extract value for single-iter    // @0x222f7b
            // (accumulatedNode stays null, bodyResult stays null)

            // Extract last value for setValue on result             // @0x222f8c
            Value lastVal = countResult->values_.back().value_;
            int lastInt = lastVal.toInt();                           // @0x2233c3

            // ---- Cvar single-iteration / zero-iteration path ---- // @0x22341d
            if (countInt > 0) {                                      // @0x22341d
                // Single iteration (countInt == 1)
                if (body()) {                                        // @0x223426
                    auto unrollScope =
                        subRes->createSubScope("unroll");            // @0x223471
                    bodyResult =
                        body()->evaluate(unrollScope, ctx, state);   // @0x2234a1
                }

                // Node chaining for first iteration (nothing to chain to)
                // bodyResult = bodyEval (just set above)

                // Update accumulated bodyResult tracking
                if (bodyResult) {
                    // Merge assemblers                              // @0x2236b8
                    result->assemblers_.insert(
                        result->assemblers_.end(),
                        bodyResult->assemblers_.begin(),
                        bodyResult->assemblers_.end());

                    // Check hasError                                // @0x2236e9
                    if (bodyResult->returnEncountered_) {
                        result->setValue(bodyResult->getValue());     // @0x223850
                    }
                }
            }
        }
        else {
            // countInt >= 2: evaluate body FIRST to catch unsupported
            // statements (like break) before checking loopUnrollLimit.
            // This matches binary behavior: "break statement is not supported"
            // comes BEFORE "too many iterations" error.
            int savedLineNr = ctx.messages->lineNr();                // @0x222fe3
            subRes->setAtScopeBoundary(true);                        // @0x222ff2

            std::shared_ptr<EvalResults> firstBodyResult;
            if (body()) {
                auto maybeScope =
                    subRes->createSubScope("maybe_unroll");          // @0x223045
                firstBodyResult =
                    body()->evaluate(maybeScope, ctx, state);         // @0x223078
            }

            subRes->setAtScopeBoundary(false);
            ctx.messages->setLineNr(savedLineNr);                    // @0x223160
            ctx.asmCommands->setWavetableFrontIndex(savedLineNr);    // @0x223169
            ctx.wavetable->setLineNr(savedLineNr);                   // @0x223174

            // NOW check loopUnrollLimit AFTER body eval              // @0x222fcf
            // BUT skip if body eval already produced an error
            // (e.g., "break statement is not supported")
            if (!ctx.messages->hadCompilerError() &&
                countInt > ctx.loopUnrollLimit) {                  // @0x222fd5
                // Error 0x7b: too many iterations (BST lookup)      // @0x2231d6
                auto const& msgs = ErrorMessages::messages;
                auto it = msgs.find(0x7b);
                if (it != msgs.end()) {
                    ctx.messages->errorMessage(it->second, -1);
                }
                return result;
            }

            // Check if firstBodyResult has empty assemblers          // @0x223179
            // (firstBodyResult already computed above, before limit check)
            bool firstBodyEmpty = !firstBodyResult ||
                firstBodyResult->assemblers_.begin() ==
                firstBodyResult->assemblers_.end();

            bodyResult = firstBodyResult;                             // @0x223196

            bodyResult = firstBodyResult;                             // @0x223196

            if (!firstBodyEmpty) {
                // ---- Runtime loop path ----                        // @0x2231d2
                // Body produces assemblers → generate hardware loop.
                // Initialize counter with countInt, then jump to shared
                // var_path which re-evaluates body inside loop structure.
                auto initAsms = ctx.asmCommands->addi(
                    counterReg, AsmRegister::Reg(0),
                    Immediate(countInt));                             // @0x2231e8
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    initAsms.begin(),
                    initAsms.end());

                hasEndLabel = false;  // count known >= 2, no brz needed
                goto var_path;                                       // @0x223214 → 0x22212f
            }

            // ---- Unroll path (firstBodyEmpty == true) ----         // @0x222f7b
            // Iterate remaining count-1 times
            for (int i = countInt - 1; i > 0; --i) {                 // @0x223730

                std::shared_ptr<EvalResults> bodyEval;
                if (body()) {                                        // @0x223426
                    auto unrollScope =
                        subRes->createSubScope("unroll");            // @0x223471
                    bodyEval =
                        body()->evaluate(unrollScope, ctx, state);   // @0x2234a1
                }

                // Node chaining                                     // @0x2234e5
                if (bodyEval && bodyResult && bodyResult->node_) {   // @0x2234f5
                    auto current = bodyResult->node_;
                    while (current->next) current = current->next;   // @0x223540
                    current->next = bodyEval->node_;                 // @0x2235b2

                    if (!accumulatedNode) {
                        accumulatedNode = bodyResult->node_;          // @0x223790
                    }
                }

                // Update bodyResult                                 // @0x223650
                bodyResult = bodyEval;

                if (bodyResult) {
                    // Merge assemblers                              // @0x2236b8
                    result->assemblers_.insert(
                        result->assemblers_.end(),
                        bodyResult->assemblers_.begin(),
                        bodyResult->assemblers_.end());

                    // Check hasError                                // @0x2236e9
                    if (bodyResult->returnEncountered_) {
                        result->setValue(bodyResult->getValue());     // @0x223850
                        break;
                    }
                }
            }
        }

        // ---- Post-loop: copy accumulated node ----                // @0x2238dd
        if (bodyResult) {
            if (accumulatedNode) {
                result->node_ = accumulatedNode;                     // @0x22390a
            } else {
                result->node_ = bodyResult->node_;                   // @0x22391d
            }
        }
    }
    else if (countResult->values_.size() == 1 &&
             countResult->values_.back().varType_ == VarType_Var)    // @0x222792
    {
        // ============================================================
        // Var path (known single Var value): runtime loop           // @0x22279d
        // ============================================================
        // addi(asmCommands, countResult->reg_, counterReg, Immediate(0))
        // to initialize counter register from count value
        AsmRegister countReg = countResult->values_.back().reg_;
        auto initAsms = ctx.asmCommands->addi(
            counterReg, countReg, Immediate(0));                     // @0x2227d0
        result->assemblers_.insert(
            result->assemblers_.end(),
            initAsms.begin(),
            initAsms.end());                                         // @0x222806

        hasEndLabel = true;
        // Fall through to shared Var path at label-based section below
        // (The binary jumps to 0x22212f which is the shared Var path)
        // We implement the shared Var path inline here:
        goto var_path;                                               // matches binary jmp to 0x22212f
    }
    else
    {
        // ============================================================
        // Var path (no value or multiple values): runtime loop      // @0x22212b
        // ============================================================
        hasEndLabel = true;                                          // default

    var_path:
        // Create "loop" label                                       // @0x222139
        std::string loopLabel = Resources::newLabel("loop");         // @0x22215f

        // Optionally create "end" label and emit brz               // @0x2221a0
        std::string endLabel;
        if (hasEndLabel) {
            endLabel = Resources::newLabel("end");                   // @0x2221c5

            // brz(counterReg, endLabel, false) → push              // @0x22224f
            AsmList::Asm brzAsm = ctx.asmCommands->brz(
                counterReg, endLabel, false);
            result->assemblers_.push_back(brzAsm);                   // @0x222254
        }

        // asmLabel(loopLabel) → push                                // @0x22235c
        AsmList::Asm loopLabelAsm =
            ctx.asmCommands->asmLabel(loopLabel);                    // @0x2774e0
        result->assemblers_.push_back(loopLabelAsm);                 // @0x222377

        // asmLoopNode()                                             // @0x222439
        AsmList::Asm loopNodeAsm =
            ctx.asmCommands->asmLoopNode();                          // @0x277ad0

        // loopBodyRunsAtLeastOnce = !hasEndLabel                    // @0x222444
        loopNodeAsm.node->loopBodyRunsAtLeastOnce = !hasEndLabel;

        // Push loopNodeAsm and copy its node into result            // @0x22245e
        result->assemblers_.push_back(loopNodeAsm);
        result->node_ = loopNodeAsm.node;                           // @0x2224d8

        // setState(Active) on sub-scope                             // @0x222509
        subRes->setState(Resources::State::Active);                  // @0x1e35f0

        // Evaluate body                                             // @0x22251a
        bodyResult = body()->evaluate(subRes, ctx, state);           // @0x222562

        // Merge body assemblers into result                         // @0x2225ed
        if (bodyResult) {
            result->assemblers_.insert(
                result->assemblers_.end(),
                bodyResult->assemblers_.begin(),
                bodyResult->assemblers_.end());                      // @0x222629

            // Extract bodyResult value → setValue on result          // @0x22262e
            result->setValue(bodyResult->getValue());                 // @0x22290b
        }

        // addi(asmCommands, counterReg, counterReg, Immediate(-1)) // @0x222942
        auto decrAsms = ctx.asmCommands->addi(
            counterReg, counterReg, Immediate(-1));                  // @0x222974
        result->assemblers_.insert(
            result->assemblers_.end(),
            decrAsms.begin(),
            decrAsms.end());                                         // @0x2229aa

        // brgz(counterReg, loopLabel, false) → push                // @0x222aa1
        AsmList::Asm brgzAsm = ctx.asmCommands->brgz(
            counterReg, loopLabel, false);                           // @0x272140
        result->assemblers_.push_back(brgzAsm);                      // @0x222aa6

        // Optionally emit endLabel                                  // @0x222b62
        if (hasEndLabel) {
            AsmList::Asm endLabelAsm =
                ctx.asmCommands->asmLabel(endLabel);                 // @0x222b77
            result->assemblers_.push_back(endLabelAsm);              // @0x222b8a
        }

        // Node tree linkage                                         // @0x222c53
        if (countResult) {
            loopArgNodeAppend(result->node_, countResult->node_);    // @0x222ca7
        }
        if (bodyResult) {
            loopBodyNodeAppend(result->node_, bodyResult->node_);    // @0x222d49
        }
    }

    // ---- Shared epilogue: copy hasError ----                      // @0x222e4e
    result->returnEncountered_ = bodyResult
        ? bodyResult->returnEncountered_ : false;                             // @0x222e5a

    return result;                                                   // @0x222ef6
}

// ---- Three-child 3-arg -----------------------------------------------------

// SeqCIfElse::evaluate(3) — @0x214d50, 7214B
//   Evaluates "if (cond) ifBody else elseBody".
//
//   Binary address annotations:
//     0x214d50–0x214da3  Prologue, setLineNr, make_shared<EvalResults>
//     0x214df5–0x214e5b  Evaluate cond() in sub-scope("if-args") → condResult
//     0x214ea6–0x214ee5  Null condResult → errorMessage(0x27, "if")
//     0x214eef–0x215026  Build result name: "if (" + condResult->name_ + ")"
//     0x215046–0x215075  Validate: exactly 1 value, check varType
//     0x21507b–0x2150b3  Error: wrong value count/type → errorMessage(0x27)
//     0x215140–0x215c82  Var path: 3 labels (if/else/end), branch node,
//                         jumpIfZero→elseLabel, both bodies in sub-scopes,
//                         if-label+ifBody, br→endLabel, else-label+elseBody,
//                         end-label, hasError AND
//     0x2152c9–0x2164fb  Const/Cvar path: scopeBoundaryFlags_=1, dead branch eval,
//                         scopeBoundaryFlags_=0, live branch eval, setValue, node_,
//                         hasError, conditional AND (state.inFunctionDef_)
//     0x2150d8–0x21511c  Cleanup and return
//
//   Key differences from SeqCIfCondition:
//     - Three labels (if, else, end) instead of two.
//     - jumpIfZero targets elseLabel, not endLabel.
//     - br(endLabel, false) unconditional jump after if-body.
//     - Both branches always evaluated (including dead in Const/Cvar path).
//     - hasError = AND of both branches (both must error for result to error).
//     - Mid-function setWavetableFrontIndex(cond()->lineNr()).
std::shared_ptr<EvalResults> SeqCIfElse::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // ---- Prologue: set lineNr on subsystems ----                  // @0x214d77
    const int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);                                 // @0x214d7f
    ctx.asmCommands->setWavetableFrontIndex(lineNr);                 // @0x214d88
    ctx.wavetable->setLineNr(lineNr);                                // @0x214d91

    // ---- Allocate result ----                                     // @0x214d96
    auto result = std::make_shared<EvalResults>();

    // ---- Evaluate condition in sub-scope("if-args") ----          // @0x214df5
    auto condScope = res->createSubScope("if-args");                 // @0x214e38
    auto condResult = cond()->evaluate(condScope, ctx, state);       // @0x214e5b

    // ---- Null condResult check ----                               // @0x214ea6
    if (!condResult) {
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "if");                              // @0x214ed1
        ctx.messages->errorMessage(msg, -1);                         // @0x214ee5
        return result;
    }

    // ---- Build result name: "if (" + condResult->name_ + ")" --- // @0x214eef
    result->name_ = "if (" + condResult->name_ + ")";               // @0x214fcf

    // ---- Validate: exactly 1 value ----                           // @0x215046
    if (condResult->values_.empty() ||
        condResult->values_.size() != 1)                             // @0x21506d (magic ÷7)
    {
        // Error: bad condition value count.                          // @0x21507b
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "if");                              // @0x215098
        ctx.messages->errorMessage(msg, cond()->lineNr());              // @0x2150b3
        return result;
    }

    auto const& condVal = condResult->values_.back();

    // ================================================================
    // Var path (varType == 2)                                       // @0x215140
    // ================================================================
    if (condVal.varType_ == VarType_Var) {
        // Copy condResult assemblers into result.                   // @0x21514a
        result->assemblers_.insert(
            result->assemblers_.end(),
            condResult->assemblers_.begin(),
            condResult->assemblers_.end());

        // Generate three labels: if, else, end.                     // @0x21517a
        std::string ifLabel   = Resources::newLabel("if");           // @0x21519f
        std::string elseLabel = Resources::newLabel("else");         // @0x2151ea
        std::string endLabel  = Resources::newLabel("end");          // @0x21522e

        // Create branch node asm.                                   // @0x215253
        AsmList::Asm branchAsm = ctx.asmCommands->asmBranchNode();   // @0x277950
        result->assemblers_.push_back(branchAsm);                    // @0x21526a

        // Assign branch node to result.                             // @0x21532e
        result->node_ = branchAsm.node;

        // Generate jump-if-zero to elseLabel (not endLabel).        // @0x2153a9
        auto jumpAsms = jumpIfZero(condResult, elseLabel, ctx);      // @0x2149f0

        // Insert jump asm instructions into result assemblers.      // @0x2153c6
        result->assemblers_.insert(
            result->assemblers_.end(),
            jumpAsms.begin(),
            jumpAsms.end());

        // ---- Evaluate if-true body ----                           // @0x2154be
        auto ifTrueScope = res->createSubScope("if-true");           // @0x2154f8
        ifTrueScope->setState(Resources::State::Active);             // @0x215530

        std::shared_ptr<EvalResults> ifBodyResult;
        if (ifBody()) {                                              // @0x215539
            auto bodySubScope =
                ifTrueScope->createSubScope("if-true-body");         // @0x215591
            ifBodyResult =
                ifBody()->evaluate(bodySubScope, ctx, state);        // @0x2155b0
        }

        // ---- Evaluate else body ----                              // @0x21560d
        auto ifFalseScope = res->createSubScope("if-false");         // @0x215644
        ifFalseScope->setState(Resources::State::Active);            // @0x215675

        std::shared_ptr<EvalResults> elseBodyResult;
        if (elseBody()) {                                            // @0x21567e
            auto elseSubScope =
                ifFalseScope->createSubScope("if-false-body");       // @0x2156d3
            elseBodyResult =
                elseBody()->evaluate(elseSubScope, ctx, state);      // @0x2156f2
        }

        // ---- Emit ifLabel asm ----                                // @0x21574f
        AsmList::Asm ifLabelAsm =
            ctx.asmCommands->asmLabel(ifLabel);                      // @0x2774e0
        result->assemblers_.push_back(ifLabelAsm);                   // @0x21576d

        // ---- Merge if-body result ----                            // @0x215826
        if (ifBodyResult) {
            result->assemblers_.insert(
                result->assemblers_.end(),
                ifBodyResult->assemblers_.begin(),
                ifBodyResult->assemblers_.end());

            result->node_->branches.emplace_back(                    // @0x215870
                ifBodyResult->node_);
        } else {
            result->node_->branchMaySkipAllBodies = true;            // @0x215877
        }

        // ---- Mid-function setWavetableFrontIndex ----             // @0x215882
        ctx.asmCommands->setWavetableFrontIndex(cond()->lineNr());     // @0x215892

        // ---- Unconditional branch to endLabel ----                // @0x215895
        AsmList::Asm brAsm =
            ctx.asmCommands->br(endLabel, false);                    // @0x2158a9
        result->assemblers_.push_back(brAsm);                        // @0x2158ae

        // ---- Emit elseLabel asm ----                              // @0x21595b
        AsmList::Asm elseLabelAsm =
            ctx.asmCommands->asmLabel(elseLabel);                    // @0x215980
        result->assemblers_.push_back(elseLabelAsm);                 // @0x215985

        // ---- Merge else-body result ----                          // @0x215a3e
        if (elseBodyResult) {
            result->assemblers_.insert(
                result->assemblers_.end(),
                elseBodyResult->assemblers_.begin(),
                elseBodyResult->assemblers_.end());

            result->node_->branches.emplace_back(                    // @0x215a88
                elseBodyResult->node_);
        } else {
            result->node_->branchMaySkipAllBodies = true;            // @0x215a8f
        }

        // ---- Emit endLabel asm ----                               // @0x215a9a
        AsmList::Asm endLabelAsm =
            ctx.asmCommands->asmLabel(endLabel);                     // @0x215aac
        result->assemblers_.push_back(endLabelAsm);                  // @0x215ab1

        // ---- hasError: AND of both branches ----                  // @0x215b2f
        // Result has error only if both branches error.
        if (ifBodyResult && elseBodyResult &&
            ifBodyResult->returnEncountered_)
        {
            result->returnEncountered_ = elseBodyResult->returnEncountered_;           // @0x215b4d
        } else {
            result->returnEncountered_ = false;                               // @0x215b53
        }

        return result;
    }

    // ================================================================
    // Const/Cvar path (varType | 2 == 6)                            // @0x2152c9
    // ================================================================
    if ((static_cast<int>(condVal.varType_) | 2) == 6) {
        // Extract condition value at compile-time.                  // @0x2152d8
        Value val = condVal.value_;
        int intVal = val.toInt();                                    // @0x215d81

        // Set dead-branch flag on resources.                        // @0x215dc3
        // Binary: mov BYTE PTR [res.get()+0x88], 0x1
        res->setAtScopeBoundary(true);

        std::shared_ptr<EvalResults> deadResult;
        std::shared_ptr<EvalResults> liveResult;

        if (intVal != 0) {
            // ---- Nonzero (true): else is dead, if is live ----    // @0x215dd6
            // Evaluate dead branch (else) first.
            if (elseBody()) {                                        // @0x215dda
                auto deadScope =
                    res->createSubScope("if-false");                 // @0x215e25
                deadResult =
                    elseBody()->evaluate(deadScope, ctx, state);     // @0x215e40
            }

            // Reset dead-branch flag.                               // @0x215f73
            // Binary: mov BYTE PTR [res.get()+0x88], 0x0
            res->setAtScopeBoundary(false);

            // Evaluate live branch (if).                            // @0x215f7e
            if (ifBody()) {                                          // @0x215f82
                auto liveScope =
                    res->createSubScope("if-true");                  // @0x215fd0
                liveResult =
                    ifBody()->evaluate(liveScope, ctx, state);       // @0x215fef
            }
        } else {
            // ---- Zero (false): if is dead, else is live ----      // @0x215e9e
            // Evaluate dead branch (if) first.
            if (ifBody()) {                                          // @0x215ea2
                auto deadScope =
                    res->createSubScope("if-true");                  // @0x215ef0
                deadResult =
                    ifBody()->evaluate(deadScope, ctx, state);       // @0x215f0f
            }

            // Reset dead-branch flag.                               // @0x2160e7
            res->setAtScopeBoundary(false);

            // Evaluate live branch (else).                          // @0x2160fc
            if (elseBody()) {                                        // @0x216100
                auto liveScope =
                    res->createSubScope("if-false");                 // @0x21614b
                liveResult =
                    elseBody()->evaluate(liveScope, ctx, state);     // @0x21616a
            }
        }

        // ---- Extract results from live branch ----                // @0x216040
        if (liveResult) {
            // Merge live branch assemblers.
            result->assemblers_.insert(
                result->assemblers_.end(),
                liveResult->assemblers_.begin(),
                liveResult->assemblers_.end());

            // Copy value from live result.                          // @0x216343
            result->setValue(liveResult->getValue());

            // Copy hasError flag.                                   // @0x216384
            result->returnEncountered_ = liveResult->returnEncountered_;

            // Copy node.                                            // @0x216393
            result->node_ = liveResult->node_;
        }

        // ---- Conditional hasError AND logic ----                  // @0x2163d6
        // When state.inFunctionDef_ is set, override returnEncountered_ with AND of
        // both branches (result has error only if both error).
        // Binary: cmp BYTE PTR [state+0x10], 0x1
        if (static_cast<uint8_t>(state.inFunctionDef_) == 1) {
            if (liveResult && deadResult &&
                liveResult->returnEncountered_)
            {
                result->returnEncountered_ = deadResult->returnEncountered_;
            } else {
                result->returnEncountered_ = false;
            }
        }

        return result;
    }

    // ================================================================
    // Error: unsupported varType                                    // @0x21507b (shared path)
    // ================================================================
    {
        std::string msg = ErrorMessages::format(
            ErrorMessageT(0x27), "if");                              // @0x215098
        ctx.messages->errorMessage(msg, cond()->lineNr());              // @0x2150b3
    }

    return result;                                                   // @0x2150d8
}

// SeqCCondExpr::evaluate(3) — @0x223d90, 11007B
//   Ternary conditional expression: cond ? trueBranch : falseBranch
//   Three major paths based on condition varType:
//     1. Var (varType==2): runtime branching with register copy via addi
//     2. Const/Cvar (varType|2==6): compile-time evaluation, both branches
//        evaluated for side effects, correct one's value used
//     3. Other: error 0x1f (values size != 1) or error 0xe4 (type mismatch)
//
//   Binary address annotations:
//     0x223d90–0x223dce  Prologue: setLineNr on messages, asmCommands, wavetable
//     0x223dd3–0x223e20  make_shared<EvalResults>() → result
//     0x223e31–0x223e36  cond()->evaluate(res, ctx, state) → condResult
//     0x223e91–0x223ecf  Null/empty condResult → error 0x1f
//     0x224020–0x224028  condResult.values_.back().varType_ == Var → Var path
//     0x22417e–0x224187  condResult.values_.back().varType_ | 2 == 6 → Const/Cvar path
//     0x224020–0x225b15  Var path: labels, branchNode, jumpIfZero, sub-scopes,
//                         register allocation, addi copy for each branch
//     0x22418d–0x22556f  Const/Cvar path: extract value→toInt, evaluate both
//                         branches, use correct result's getValue()→setValue
//     0x22556f–0x2261c9  Name construction + cleanup for all paths
//     0x2257ee–0x225934  Error 0xe4 (type mismatch between branches)
std::shared_ptr<EvalResults> SeqCCondExpr::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // ---- Prologue: set lineNr on subsystems ----                  // @0x223db4
    const int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);                                 // @0x223dbc
    ctx.asmCommands->setWavetableFrontIndex(lineNr);                 // @0x223dc0
    ctx.wavetable->setLineNr(lineNr);                                // @0x223dc8

    // ---- Allocate result ----                                     // @0x223dd3
    auto result = std::make_shared<EvalResults>();

    // ---- Evaluate condition ----                                  // @0x223e31
    auto condResult = cond()->evaluate(res, ctx, state);             // virtual call

    // ---- Null/empty condResult → error 0x1f ----                  // @0x223e91
    if (!condResult || condResult->values_.empty() ||
        condResult->values_.size() != 1)
    {
        // Lookup error message 0x1f in ErrorMessages::messages map   // @0x223ecf
        ctx.messages->errorMessage(
            ErrorMessages::messages[ErrorMessageT(0x1f)], -1);       // @0x223f14
        result = std::make_shared<EvalResults>();
        return result;
    }

    auto const& condVal = condResult->values_.back();

    // ================================================================
    // Var path (varType == 2)                                       // @0x224020
    // ================================================================
    if (condVal.varType_ == VarType_Var) {
        // Generate "else" and "end" labels.                         // @0x22402e
        std::string elseLabel = Resources::newLabel("else");         // @0x224054
        std::string endLabel  = Resources::newLabel("end");          // @0x224095

        // Merge cond assemblers into result.                        // @0x2240c1
        result->assemblers_.insert(
            result->assemblers_.end(),
            condResult->assemblers_.begin(),
            condResult->assemblers_.end());

        // Emit branch node.                                         // @0x224107
        AsmList::Asm branchAsm = ctx.asmCommands->asmBranchNode();   // @0x277950
        result->assemblers_.push_back(branchAsm);                    // @0x224113

        // Store branch node in result.                              // @0x2241d9
        result->node_ = branchAsm.node;

        // Save condResult for jumpIfZero.                           // @0x224237
        auto savedCondResult = condResult;

        // jumpIfZero → elseLabel.                                   // @0x224256
        auto jumpAsms = jumpIfZero(condResult, elseLabel, ctx);      // @0x2149f0
        result->assemblers_.insert(
            result->assemblers_.end(),
            jumpAsms.begin(),
            jumpAsms.end());

        // ---- Evaluate trueBranch (ifBody) in sub-scope("if") ---- // @0x224372
        auto ifScope = res->createSubScope("if");                    // @0x22439f
        auto ifResult = ifBody()->evaluate(ifScope, ctx, state); // @0x2243c8

        // ---- Evaluate falseBranch (elseBody) in sub-scope("else") // @0x224417
        auto elseScope = res->createSubScope("else");                // @0x22444f
        auto elseResult = elseBody()->evaluate(elseScope, ctx, state); // @0x224471

        // ---- Allocate result register ----                        // @0x2244c0
        int regNum = Resources::getRegisterNumber();                 // @0x1e4bb0
        result->setValue(VarType_Var, regNum);                       // @0x15c850

        // ---- Process ifResult (trueBranch result) ----            // @0x2244db
        if (ifResult) {
            auto const& ifVal = ifResult->values_.back();
            if (ifResult->values_.size() >= 2) {
                // Multiple values: error 0xe4 (type mismatch)       // @0x224519
                VarType vt = (ifResult->values_.size() > 1)
                    ? static_cast<VarType>(0) : ifVal.varType_;
                ctx.messages->errorMessage(
                    ErrorMessages::format(ErrorMessageT(0xe4), str(vt)), -1);
                // Fall through to name construction                 // @0x2257ee
            } else if (ifVal.varType_ == VarType_Var) {
                // Var: merge assemblers, then addi to copy register  // @0x2245ae
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    ifResult->assemblers_.begin(),
                    ifResult->assemblers_.end());

                // Get result last register and ifResult last register // @0x2245e8
                AsmRegister resultReg = result->values_.empty()
                    ? AsmRegister(0) : result->values_.back().reg_;
                AsmRegister ifReg = ifResult->values_.empty()
                    ? AsmRegister(0) : ifResult->values_.back().reg_;

                // addi(asmCommands, resultReg, ifReg, Immediate(0)) // @0x224bf2
                auto addiAsms = ctx.asmCommands->addi(
                    resultReg, ifReg, Immediate(0));
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    addiAsms.begin(),
                    addiAsms.end());
            } else if ((static_cast<int>(ifVal.varType_) | 2) == 6) {
                // Const/Cvar: extract value→toInt→Immediate→addi    // @0x22464e
                AsmRegister resultReg = result->values_.empty()
                    ? AsmRegister(0) : result->values_.back().reg_;
                AsmRegister zeroReg(0);

                Value val = ifVal.value_;
                int intVal = val.toInt();
                auto addiAsms = ctx.asmCommands->addi(
                    resultReg, zeroReg, Immediate(intVal));
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    addiAsms.begin(),
                    addiAsms.end());
            } else {
                // Unsupported type: error 0xe4                      // @0x224519
                ctx.messages->errorMessage(
                    ErrorMessages::format(ErrorMessageT(0xe4),
                        str(ifVal.varType_)), -1);
            }
        }

        // ---- Unconditional branch to endLabel ----                // @0x22556f
        AsmList::Asm brAsm =
            ctx.asmCommands->br(endLabel, false);                    // @0x271df0
        result->assemblers_.push_back(brAsm);                        // @0x225590

        // ---- Emit elseLabel ----                                  // @0x225650
        AsmList::Asm elseLabelAsm =
            ctx.asmCommands->asmLabel(elseLabel);                    // @0x2774e0
        result->assemblers_.push_back(elseLabelAsm);                 // @0x22566a

        // ---- Process elseResult (falseBranch result) ----         // @0x225723
        if (elseResult) {
            auto const& elseVal = elseResult->values_.back();
            if (elseResult->values_.size() >= 2) {
                // Multiple values: error 0xe4                       // @0x225761
                VarType vt = (elseResult->values_.size() > 1)
                    ? static_cast<VarType>(0) : elseVal.varType_;
                ctx.messages->errorMessage(
                    ErrorMessages::format(ErrorMessageT(0xe4), str(vt)), -1);
            } else if (elseVal.varType_ == VarType_Var) {
                // Var: merge assemblers + addi copy                 // @0x225934
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    elseResult->assemblers_.begin(),
                    elseResult->assemblers_.end());

                AsmRegister resultReg = result->values_.empty()
                    ? AsmRegister(0) : result->values_.back().reg_;
                AsmRegister elseReg = elseResult->values_.empty()
                    ? AsmRegister(0) : elseResult->values_.back().reg_;

                auto addiAsms = ctx.asmCommands->addi(
                    resultReg, elseReg, Immediate(0));
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    addiAsms.begin(),
                    addiAsms.end());
            } else if ((static_cast<int>(elseVal.varType_) | 2) == 6) {
                // Const/Cvar: extract→toInt→addi                    // @0x22599b
                AsmRegister resultReg = result->values_.empty()
                    ? AsmRegister(0) : result->values_.back().reg_;
                AsmRegister zeroReg(0);

                Value val = elseVal.value_;
                int intVal = val.toInt();
                auto addiAsms = ctx.asmCommands->addi(
                    resultReg, zeroReg, Immediate(intVal));
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    addiAsms.begin(),
                    addiAsms.end());
            } else {
                // Unsupported type: error 0xe4                      // @0x225761
                ctx.messages->errorMessage(
                    ErrorMessages::format(ErrorMessageT(0xe4),
                        str(elseVal.varType_)), -1);
            }
        }

        // ---- Emit endLabel ----                                   // @0x225d53
        AsmList::Asm endLabelAsm =
            ctx.asmCommands->asmLabel(endLabel);                     // @0x2774e0
        result->assemblers_.push_back(endLabelAsm);                  // @0x225d6e

        // ---- Build result name ----                               // @0x225e27
        // "(" + condResult->name_ + ") ? " + ifResult->name_ + " : " + elseResult->name_
        result->name_ = "(" + condResult->name_ + ") ? " +
            (ifResult  ? ifResult->name_  : std::string()) + " : " +
            (elseResult ? elseResult->name_ : std::string());

        return result;
    }

    // ================================================================
    // Const/Cvar path (varType | 2 == 6)                            // @0x22417e
    // ================================================================
    if ((static_cast<int>(condVal.varType_) | 2) == 6) {
        // Extract condition value at compile time.                  // @0x22418d
        Value val = condVal.value_;
        int intVal = val.toInt();                                    // @0x2246a2

        std::shared_ptr<EvalResults> ifResult;
        std::shared_ptr<EvalResults> elseResult;

        if (intVal != 0) {
            // ---- Nonzero (true): evaluate elseBody first (dead), then ifBody (live)
            // Evaluate dead branch (else) first for side effects.   // @0x2246e5
            {
                auto elseScope = res->createSubScope("else");        // @0x22471a
                elseResult = elseBody()->evaluate(elseScope, ctx, state); // @0x224739
            }
            // Evaluate live branch (if).                            // @0x224788
            {
                auto ifScope = res->createSubScope("if");            // @0x2247bc
                ifResult = ifBody()->evaluate(ifScope, ctx, state); // @0x2247de
            }

            // Use ifResult value.                                   // @0x22482d
            if (ifResult) {
                auto const& ifVal = ifResult->values_.back();
                if (ifResult->values_.size() < 2) {
                    // Single value: dispatch on varType              // @0x224aa7
                    if (ifVal.varType_ == VarType_Var) {
                        // Var: merge assemblers, getRegisterNumber, addi
                        result->assemblers_.insert(
                            result->assemblers_.end(),
                            ifResult->assemblers_.begin(),
                            ifResult->assemblers_.end());
                        int regNum2 = Resources::getRegisterNumber();
                        result->setValue(VarType_Var, regNum2);

                        AsmRegister resultReg = result->values_.back().reg_;
                        AsmRegister ifReg = ifResult->values_.empty()
                            ? AsmRegister(0) : ifResult->values_.back().reg_;
                        AsmRegister elseReg2 = elseResult
                            ? (elseResult->values_.empty()
                                ? AsmRegister(0)
                                : elseResult->values_.back().reg_)
                            : AsmRegister(0);

                        // addi for both branches                    // @0x224de2
                        auto addi1 = ctx.asmCommands->addi(
                            resultReg, ifReg, Immediate(0));
                        result->assemblers_.insert(
                            result->assemblers_.end(),
                            addi1.begin(), addi1.end());
                    } else {
                        // Non-Var (Const/Cvar/etc): use actual varType // @0x224b9b
                        result->setValue(
                            ifVal.varType_,
                            static_cast<VarSubType>(0),
                            ifResult->getValue());
                    }
                } else {
                    // Multiple values: setValue with VarType(0)      // @0x224872
                    result->setValue(
                        static_cast<VarType>(0),
                        static_cast<VarSubType>(0),
                        ifResult->getValue());
                }
            }
        } else {
            // ---- Zero (false): evaluate ifBody first (dead), then elseBody (live)
            // Evaluate dead branch (if) first for side effects.     // @0x2248c6
            {
                auto ifScope = res->createSubScope("if");            // @0x2248fa
                ifResult = ifBody()->evaluate(ifScope, ctx, state); // @0x224917
            }
            // Evaluate live branch (else).                          // @0x224968
            {
                auto elseScope = res->createSubScope("else");        // @0x22499d
                elseResult = elseBody()->evaluate(elseScope, ctx, state); // @0x2249bf
            }

            // Use elseResult value.                                 // @0x224a0e
            if (elseResult) {
                auto const& elseVal = elseResult->values_.back();
                if (elseResult->values_.size() < 2) {
                    // Single value: dispatch on varType
                    if (elseVal.varType_ == VarType_Var) {
                        result->assemblers_.insert(
                            result->assemblers_.end(),
                            elseResult->assemblers_.begin(),
                            elseResult->assemblers_.end());
                        int regNum2 = Resources::getRegisterNumber();
                        result->setValue(VarType_Var, regNum2);

                        AsmRegister resultReg = result->values_.back().reg_;
                        AsmRegister elseReg = elseResult->values_.empty()
                            ? AsmRegister(0) : elseResult->values_.back().reg_;
                        AsmRegister ifReg2 = ifResult
                            ? (ifResult->values_.empty()
                                ? AsmRegister(0)
                                : ifResult->values_.back().reg_)
                            : AsmRegister(0);

                        auto addi1 = ctx.asmCommands->addi(
                            resultReg, elseReg, Immediate(0));
                        result->assemblers_.insert(
                            result->assemblers_.end(),
                            addi1.begin(), addi1.end());
                    } else {
                        // Non-Var: use actual varType/subType
                        result->setValue(
                            elseVal.varType_,
                            static_cast<VarSubType>(0),
                            elseResult->getValue());
                    }
                } else {
                    // Multiple values: setValue with VarType(0)
                    result->setValue(
                        static_cast<VarType>(0),
                        static_cast<VarSubType>(0),
                        elseResult->getValue());
                }
            }
        }

        // ---- Build result name ----                               // @0x224e45
        // "(" + condResult->name_ + ") ? " + ifResult->name_ + " : " + elseResult->name_
        result->name_ = "(" + condResult->name_ + ") ? " +
            (ifResult  ? ifResult->name_  : std::string()) + " : " +
            (elseResult ? elseResult->name_ : std::string());

        return result;
    }

    // ================================================================
    // Error: unsupported varType                                    // @0x223ecf (shared)
    // ================================================================
    {
        ctx.messages->errorMessage(
            ErrorMessages::messages[ErrorMessageT(0x1f)], -1);
    }

    return result;
}

// ---- Four-child 3-arg ------------------------------------------------------

// SeqCFunction::evaluate(3) — @0x20b200, 5080B
//   Function definition registration.  Registers the function name, param
//   signature, and body with Resources.  If the function is named "main",
//   evaluates its body inline; otherwise stores the body for later invocation.
//
//   Binary address annotations:
//     0x20b200–0x20b254  Prologue: setLineNr on messages, asmCommands, wavetable
//     0x20b254–0x20b29c  make_shared<EvalResults>() → result
//     0x20b29c–0x20b2e2  call_ null check → FuncNoName error path
//     0x20b2ab–0x20b357  Get function name from call()->funName()->name()
//     0x20b357–0x20b398  customFunctions->functionExists(funName) → FuncPredefined error
//     0x20b398–0x20b4b0  Build param signature: getVarTypes on params, join with ", "
//     0x20b703–0x20b724  Get return VarType from retType() (Void if null)
//     0x20b724–0x20b74f  res->addFunction(funName, signature, returnVarType) → func
//     0x20b76f–0x20b784  func->addArguments(*params()) if params != null
//     0x20b784–0x20b834  body() null → FuncEmpty error path (3-arg format)
//     0x20b834–0x20bafe  body() exists: "main" check, evaluate, check return type
//     0x20beb3–0x20c013  Null bodyResult warnings, create empty replacement result
//     0x20c018–0x20c063  Exception: call_ funName null → CompilerException
//     0x20c2e0–0x20c37b  catch(CompilerException): report error, return result
std::shared_ptr<EvalResults> SeqCFunction::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // 0x20b200–0x20b254  Prologue
    const int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);
    ctx.asmCommands->setWavetableFrontIndex(lineNr);
    ctx.wavetable->setLineNr(lineNr);

    // 0x20b254–0x20b29c  make_shared<EvalResults>
    auto result = std::make_shared<EvalResults>();

    // 0x20b29c  Check call_ (the SeqCFunctionCall child)
    if (!call_) {
        // 0x20b2e2–0x20b341  FuncNoName error
        ctx.messages->errorMessage(
            ErrorMessages::messages[ErrorMessageT::FuncNoName], -1);
        return result;
    }

    // 0x20b2ab–0x20b357  Get function name string
    //   Binary calls SeqCFunctionCall::funName() @0x1fef50 which returns +0x18
    //   (first child = SeqCVariable holding the function name).
    //   Our macro names it function().
    auto* funNameVar = call()->funName();
    if (!funNameVar) {
        // 0x20c018–0x20c063  CompilerException if funName is null
        throw CompilerException(std::string(
            "SeqCFunction::evaluate: funName is null"));  // @0x905a05
    }
    const std::string funName =
        static_cast<const SeqCVariable*>(funNameVar)->name();

    // 0x20b357–0x20b398  Check if it's a predefined function
    if (ctx.customFunctions->functionExists(funName)) {
        // 0x20b4c9–0x20b535  FuncPredefined error
        ctx.messages->errorMessage(
            ErrorMessages::format(ErrorMessageT::FuncPredefined, funName), -1);
        return result;
    }

    // 0x20b398–0x20b5fb  Build param signature
    std::string signature;
    if (params()) {
        // 0x20b3b5–0x20b4b0  call getVarTypes on params (virtual dispatch)
        auto varTypes = params()->getVarTypes();

        // 0x20b3e4–0x20b488  Build "(" + join(varTypes, ", ") + ")"
        std::ostringstream oss;
        oss << "(";
        for (auto it = varTypes.begin(); it != varTypes.end(); ++it) {
            if (it != varTypes.begin()) {
                oss << ", ";
            }
            oss << *it;
        }
        oss << ")";
        signature = oss.str();
    }

    // 0x20b703–0x20b724  Get return type
    VarType returnVarType;
    if (retType()) {
        // retType() is a SeqCVariableType; its varType is stored at +0x14.
        // Binary reads 0x14(%rax) at @0x20b718 — this is the varType_ field,
        // NOT lineNr_ at +0x0C (which lineNr() returns).
        returnVarType = retType()->varType();
    } else {
        returnVarType = VarType_Void;
    }

    // 0x20b724–0x20b74f  Register function with resources
    auto func = res->addFunction(funName, signature, returnVarType);

    // 0x20b76f–0x20b784  Add arguments if present
    if (params()) {
        func->addArguments(*params());
    }

    // 0x20b784  Check body
    if (!body()) {
        // 0x20b834–0x20baa8  No body → FuncEmpty warning (3-arg format)
        std::string vtStr = str(returnVarType);
        ctx.messages->warningMessage(
            ErrorMessages::format(ErrorMessageT::FuncEmpty, vtStr, funName, signature), -1);
        // Fall through to cleanup
    } else {
        // 0x20b795–0x20b7e7  Update function scope's parent to current resources
        func->scope->updateParent(res);

        // 0x20b816  Body exists — check if function is "main"
        std::shared_ptr<EvalResults> bodyResult;

        if (funName.size() == 4 &&
            funName == "main") {
            // 0x20bbd7–0x20bcf5  "main" function: evaluate body directly
            // No addBody, no state flag changes
            try {
                bodyResult = body()->evaluate(res, ctx, state);
            } catch (const CompilerException& ex) {
                // 0x20c2e0–0x20c37b  Catch: report error, return result
                const char* msg = ex.what();
                ctx.messages->errorMessage(
                    (msg && *msg) ? std::string(msg) : std::string(""), -1);
                return result;
            }

            if (bodyResult) {
                // 0x20bc7a–0x20bcf5  Copy node from bodyResult to result;
                // merge assemblers
                result->node_ = bodyResult->node_;

                // Insert bodyResult assemblers at end of result assemblers
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    bodyResult->assemblers_.begin(),
                    bodyResult->assemblers_.end());
            }
        } else {
            // 0x20b894–0x20b958  Non-main function: addBody + set state flag
            func->addBody(*body());
            state.inFunctionDef_ = 1;   // 0x20b8ae: [r13+0x10] = 1 — "inside function def"

            try {
                bodyResult = body()->evaluate(func->scope, ctx, state);
            } catch (const CompilerException& ex) {
                // 0x20c2e0–0x20c37b  Catch: report error, return result
                const char* msg = ex.what();
                ctx.messages->errorMessage(
                    (msg && *msg) ? std::string(msg) : std::string(""), -1);
                return result;
            }

            state.inFunctionDef_ = 0;   // 0x20b954: [rax+0x10] = 0
        }

        if (bodyResult) {
            // 0x20b961–0x20b9a1  Check return type
            if (returnVarType != VarType_Void && !bodyResult->returnEncountered_) {
                // 0x20bd1d–0x20bdb2  FuncNoReturn error (2-arg format)
                std::string vtStr = str(returnVarType);
                ctx.messages->errorMessage(
                    ErrorMessages::format(ErrorMessageT::FuncNoReturn,
                                          funName, vtStr),
                    lineNr_);
            }
        } else {
            // bodyResult is null
            if (funName.size() == 4 &&
                funName == "main") {
                // 0x20be0e–0x20beb3  "main" null: two FuncEmpty warnings
                ctx.messages->warningMessage(
                    ErrorMessages::format(ErrorMessageT::FuncEmpty, funName), -1);
            }
            // 0x20beb3–0x20bf58  FuncEmpty warning + create new empty result
            ctx.messages->warningMessage(
                ErrorMessages::format(ErrorMessageT::FuncEmpty, funName), -1);
            result = std::make_shared<EvalResults>();
        }
    }

    return result;
}

// SeqCForLoop::evaluate(3) — @0x21b680, 9794B
//   For loop evaluation: two-path structure (Cvar compile-time unroll vs Var runtime).
//
//   Binary address annotations:
//     0x21b680–0x21b6c3  Prologue: setLineNr on messages, asmCommands, wavetable
//     0x21b6c3–0x21b718  make_shared<EvalResults>() → result
//     0x21b718–0x21b76f  createSubScope("for") → subRes
//     0x21b76f–0x21b8a5  Evaluate init() → initResult (or empty EvalResults if null)
//     0x21b8a5–0x21b9c7  newLabel("for") → asmLabel → push to result
//     0x21b9c7–0x21bad2  Evaluate cond() → condResult (or empty EvalResults if null)
//     0x21bad2–0x21bbd7  Evaluate incr() → incrResult (or empty EvalResults if null)
//     0x21bbd7–0x21be4f  Build name: "for (" + initResult->name_ + "; " + condResult->name_ + "; " + incrResult->name_ + ")"
//     0x21be4f–0x21be91  Dispatch: values_.size()==1 && Cvar → Cvar path
//     0x21be91–0x21d6f0  Cvar path: compile-time loop unrolling
//     0x21c8fc–0x21d072  Var path: runtime loop with asm branches
//     0x21d072–0x21d4ee  Shared epilogue: loopArgNodeAppend, loopBodyNodeAppend, hasError
std::shared_ptr<EvalResults> SeqCForLoop::evaluate(
    std::shared_ptr<Resources> res,
    FrontendLoweringContext& ctx,
    FrontendLoweringState& state) const
{
    // ---- Prologue: set lineNr on subsystems ----                  // @0x21b6a5
    const int lineNr = lineNr_;
    ctx.messages->setLineNr(lineNr);                                 // @0x21b6ad
    ctx.asmCommands->setWavetableFrontIndex(lineNr);                 // @0x21b6b6
    ctx.wavetable->setLineNr(lineNr);                                // @0x21b6c3

    // ---- Allocate result ----                                     // @0x21b6c8
    auto result = std::make_shared<EvalResults>();

    // ---- Create sub-scope("for") ----                             // @0x21b738
    auto subRes = res->createSubScope("for");                        // @0x1e36a0

    // ---- Evaluate init() if present ----                          // @0x21b77d
    std::shared_ptr<EvalResults> initResult;
    if (init()) {                                                    // @0x21b782
        initResult = init()->evaluate(subRes, ctx, state);           // @0x21b7c9
        // Merge init assemblers into result                         // @0x21b813
        result->assemblers_.insert(
            result->assemblers_.end(),
            initResult->assemblers_.begin(),
            initResult->assemblers_.end());                          // @0x21b847
    } else {
        initResult = std::make_shared<EvalResults>();                // @0x21b84e
    }

    // ---- Create for label, push asmLabel ----                     // @0x21b8a5
    std::string forLabel = Resources::newLabel("for");               // @0x1ec6b0
    AsmList::Asm forLabelAsm =
        ctx.asmCommands->asmLabel(forLabel);                         // @0x2774e0
    result->assemblers_.push_back(forLabelAsm);                      // @0x21b910

    // ---- Evaluate cond() if present ----                          // @0x21b9dd
    std::shared_ptr<EvalResults> condResult;
    auto condNode = cond();                                          // @0x21b9e1
    if (condNode) {                                                  // @0x21b9ec
        condResult = condNode->evaluate(subRes, ctx, state);         // @0x21ba2f
    } else {
        condResult = std::make_shared<EvalResults>();                // @0x21ba7b
    }

    // ---- Evaluate incr() if present ----                          // @0x21badc
    std::shared_ptr<EvalResults> incrResult;
    if (incr()) {                                                    // @0x21bae8
        incrResult = incr()->evaluate(subRes, ctx, state);           // @0x21bb2e
    } else {
        incrResult = std::make_shared<EvalResults>();                // @0x21bb80
    }

    // ---- Build name ----                                          // @0x21bbee
    result->name_ = "for (" + initResult->name_ + "; "
        + condResult->name_ + "; "
        + incrResult->name_ + ")";                                   // @0x21bdd7

    // ---- Initialize bodyResult for shared cleanup path ----
    std::shared_ptr<EvalResults> bodyResult;

    // ---- Dispatch: Cvar vs Var ----                               // @0x21be59
    if (condResult->values_.size() == 1 &&
        condResult->values_.back().varType_ == VarType_Cvar)         // @0x21be8d
    {
        // ============================================================
        // Cvar path: compile-time loop unrolling                   // @0x21be97
        // ============================================================

        // Clear result assemblers (start fresh after name build)    // @0x21be97
        result->assemblers_.clear();                                 // @0x21be9f-21bea8

        // Create new sub-scope for Cvar unrolling                   // @0x21beac
        auto cvarRes = res->createSubScope("for");                   // @0x1e36a0
        subRes = cvarRes;  // replace subRes                         // @0x21beeb

        // Re-evaluate init() in new scope                           // @0x21bf6e
        if (init()) {
            auto initEval = init()->evaluate(subRes, ctx, state);    // @0x21bfab
        }

        std::shared_ptr<Node> accumulatedNode;                       // [rbp-0x290]
        int iterCount = 0;                                           // [rbp-0xa8]
        int hasErrorOrNull = 0;                                      // accumulated error flag

        for (;;) {                                                   // loop top @0x21c0a9
            // Extract condition value → toDouble() + floatEqual     // @0x21c0a9-21c196
            Value condVal = condResult->values_.back().value_;
            if (floatEqual(condVal.toDouble(), 0.0)) {               // @0x21c191
                // Condition is zero → normal exit                   // @0x21d6aa
                if (bodyResult) {
                    if (accumulatedNode) {
                        result->node_ = accumulatedNode;             // @0x21d6da
                    } else {
                        result->node_ = bodyResult->node_;           // @0x21d6f2
                    }
                }
                // hasError copy                                     // @0x21d4d3
                result->returnEncountered_ = bodyResult
                    ? bodyResult->returnEncountered_ : false;
                break;
            }

            // Evaluate body in "unroll" sub-scope                   // @0x21c1d4
            auto unrollScope = subRes->createSubScope("unroll");     // @0x21c20c
            auto bodyEval = body()->evaluate(unrollScope, ctx, state); // @0x21c228

            // Node chaining                                         // @0x21c27c
            if (bodyEval && bodyResult && bodyResult->node_) {
                // Walk bodyResult->node_->next chain to leaf        // @0x21c2ac
                auto current = bodyResult->node_;
                while (current->next) current = current->next;       // @0x21c2c0
                // Attach bodyEval->node_ at leaf                    // @0x21c310
                current->next = bodyEval->node_;

                // Initialize accumulatedNode from old bodyResult    // @0x21c392
                if (!accumulatedNode) {
                    accumulatedNode = bodyResult->node_;              // @0x21c7a5
                }
            }

            // Update bodyResult                                     // @0x21c3b0
            bodyResult = bodyEval;

            // Merge assemblers + check hasError                     // @0x21c41c
            if (bodyResult) {
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    bodyResult->assemblers_.begin(),
                    bodyResult->assemblers_.end());                   // @0x21c448

                if (bodyResult->returnEncountered_) {                         // @0x21c44d
                    // hasError: setValue + exit                      // @0x21c870
                    result->setValue(bodyResult->getValue());
                    // Copy node                                     // @0x21d6aa (same path)
                    if (bodyResult) {
                        if (accumulatedNode) {
                            result->node_ = accumulatedNode;
                        } else {
                            result->node_ = bodyResult->node_;
                        }
                    }
                    result->returnEncountered_ = bodyResult->returnEncountered_;
                    break;
                }
            }

            // Re-evaluate incr()                                    // @0x21c4a0
            incrResult = incr()->evaluate(subRes, ctx, state);

            // Iteration limit check                                 // @0x21c528
            iterCount++;
            if ((iterCount - 1) > ctx.loopUnrollLimit) {             // @0x21c533
                // Error 0x7b: too many iterations (BST lookup)      // @0x21c53d
                auto const& msgs = ErrorMessages::messages;
                auto it = msgs.find(0x7b);
                if (it != msgs.end()) {
                    ctx.messages->errorMessage(it->second, -1);      // @0x21c599
                }
                break;  // no epilogue
            }

            // Re-evaluate cond()                                    // @0x21c61f
            condResult = cond()->evaluate(subRes, ctx, state);

            // Null condResult check                                 // @0x21c700
            if (!condResult) {
                break;  // no epilogue
            }
        }
    }
    else
    {
        // ============================================================
        // Var path: runtime loop with asm branches                 // @0x21c8fc
        // ============================================================

        // Create body and end labels                                // @0x21c8fc-21c966
        std::string bodyLabel = Resources::newLabel("body");         // @0x21c922
        std::string endLabel = Resources::newLabel("end");           // @0x21c966

        // Merge condResult assemblers into result                   // @0x21c98b
        result->assemblers_.insert(
            result->assemblers_.end(),
            condResult->assemblers_.begin(),
            condResult->assemblers_.end());                          // @0x21c9b8

        // Save condResult for later loopArgNodeAppend               // @0x21c9c4
        auto condResultSaved = condResult;

        // jumpIfZero → skip to endLabel if condition is zero        // @0x21c9e9
        auto jumpAsms = jumpIfZero(condResult, endLabel, ctx);       // @0x2149f0

        // Merge jump instructions into result                       // @0x21ca20
        result->assemblers_.insert(
            result->assemblers_.end(),
            jumpAsms.begin(),
            jumpAsms.end());                                         // @0x21ca51

        // asmLabel(bodyLabel) → push to result                      // @0x21ca56
        AsmList::Asm bodyLabelAsm =
            ctx.asmCommands->asmLabel(bodyLabel);                    // @0x2774e0
        result->assemblers_.push_back(bodyLabelAsm);                 // @0x21ca7d

        // setState(Active) on sub-scope                             // @0x21cc09
        subRes->setState(Resources::State::Active);                  // @0x1e35f0

        // asmLoopNode — create loop node asm entry                  // @0x21cc17
        AsmList::Asm loopNodeAsm =
            ctx.asmCommands->asmLoopNode();                          // @0x277ad0

        // Set loopBodyRunsAtLeastOnce                               // @0x21cc27
        loopNodeAsm.node->loopBodyRunsAtLeastOnce =
            (condNode == nullptr) || jumpAsms.empty();               // @0x21cc3f

        // Push loopNodeAsm and copy its node into result            // @0x21cc5a
        result->assemblers_.push_back(loopNodeAsm);
        result->node_ = loopNodeAsm.node;                           // @0x21ccce

        // Evaluate body (if present)                                // @0x21cd17
        if (body()) {                                                // @0x21cd23
            // Create "for-body" sub-scope                           // @0x21cd32
            auto bodySubScope =
                subRes->createSubScope("for-body");                  // @0x21cd66
            bodyResult =
                body()->evaluate(bodySubScope, ctx, state);          // @0x21cd88

            if (bodyResult) {                                        // @0x21ce33
                // Merge body assemblers into result                 // @0x21ce37
                result->assemblers_.insert(
                    result->assemblers_.end(),
                    bodyResult->assemblers_.begin(),
                    bodyResult->assemblers_.end());                  // @0x21ce66
            }
        }

        // Merge incrResult assemblers into result                   // @0x21ce71
        {
            auto r = result.get();
            r->assemblers_.insert(
                r->assemblers_.end(),
                incrResult->assemblers_.begin(),
                incrResult->assemblers_.end());                      // @0x21cea3
        }

        // Unconditional branch back to for label                    // @0x21ceac
        AsmList::Asm brAsm =
            ctx.asmCommands->br(forLabel, false);                    // @0x271df0
        result->assemblers_.push_back(brAsm);                        // @0x21cecf

        // End label                                                 // @0x21cf8e
        AsmList::Asm endLabelAsm =
            ctx.asmCommands->asmLabel(endLabel);                     // @0x2774e0
        result->assemblers_.push_back(endLabelAsm);                  // @0x21cfa9

        // Node tree linkage: loopArgNodeAppend                      // @0x21d072
        if (initResult) {
            loopArgNodeAppend(result->node_, initResult->node_);     // @0x21d0c6
        }
        if (condResultSaved) {
            loopArgNodeAppend(result->node_, condResultSaved->node_); // @0x21d15c
        }
        if (incrResult) {
            loopArgNodeAppend(result->node_, incrResult->node_);     // @0x21d2d5
        }

        // loopBodyNodeAppend                                        // @0x21d331
        if (bodyResult) {
            loopBodyNodeAppend(result->node_, bodyResult->node_);    // @0x21d377
        }

        // Copy hasError from bodyResult                             // @0x21d4d3
        result->returnEncountered_ = bodyResult
            ? bodyResult->returnEncountered_ : false;
    }

    return result;                                                   // @0x21d68f
}

} // namespace zhinst
