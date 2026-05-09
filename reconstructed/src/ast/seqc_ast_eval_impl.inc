// seqc_ast_eval_impl.inc — anonymous-namespace helpers for evaluate() TUs
// Include this file inside namespace zhinst { } in each seqc_ast_eval_*.cpp
// DO NOT include directly from headers.

// floatEqual is defined in waveform_generator.cpp (same TU in original binary).
// Forward-declare here so the anonymous-namespace helpers and evaluate() methods can use it.
bool floatEqual(double a, double b);

namespace {

// ============================================================================
// Shared helpers — promoted from per-function lambdas.
// These were identically defined in 5-11 operator evaluate() overrides each.
// ============================================================================

// isConstOrCvar — now in resources.hpp; available via zhinst::isConstOrCvar.

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
std::shared_ptr<EvalResults> computeMult(
    std::shared_ptr<EvalResults> varOperand,
    std::shared_ptr<EvalResults> constOperand,
    std::shared_ptr<Resources> res,
    FrontendLoweringContext ctx);  // by value (not reference)

// ---- : Additional helper forward declarations --------------------

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
// Binary: ctx is passed by value (not reference) per the mangled signature.
// Binary: res (shared_ptr<Resources>) is present in the signature for ABI
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

    } else if (isConstOrCvar(varType)) {
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
            // Binary: allocates a 3rd register for the subr src operand
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
    if (isConstOrCvar(v.varType_)) {
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
