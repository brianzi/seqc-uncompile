// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// seqc_ast_eval_control.cpp — split from seqc_ast_nodes_evaluate.cpp
// evaluate() implementations: function calls, arrays, if/else, switch, loops, for
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
        // Binary: br() uses [rbp-0x160] = whileLabel.
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
    // Binary: second eval's result stored in temporary at [rbp-0x300] but
    // NEVER moved to the condResult slot at [rbp-0xf0]. The first eval's
    // condResult is used at the Cvar/Var dispatch below. The second eval
    // exists only for its side effects (e.g., register allocation).
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
    // The binary reads lineNr from the call_ child (SeqCFunctionCall), NOT from
    // this->lineNr_. SeqCFunction::lineNr_ is set to the line AFTER the closing
    // brace (where createFunction is called from the grammar), while
    // SeqCFunctionCall::lineNr_ is set at the opening of the declaration line
    // (where createFunctionCall fires). Using call_->lineNr() produces the correct
    // error line for e.g. FormatVarReturn(170) reported during addArguments().
    const int lineNr = call_ ? call_->lineNr() : lineNr_;
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
            ErrorMessages::format(ErrorMessageT::FuncPredefined, funName), lineNr_);
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
