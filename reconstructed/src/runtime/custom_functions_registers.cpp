// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// custom_functions_registers.cpp — split from custom_functions_io.cpp
// Register, node-write, QA, PRNG, sweep, and feedback methods: setInt, setDouble, setUserReg, QA methods, PRNG, freq sweep
// ============================================================================

#include <boost/format.hpp>
#include <boost/regex.hpp>

#include "zhinst/runtime/custom_functions.hpp"
#include "zhinst/asm/asm_commands.hpp"
#include "zhinst/asm/asm_list.hpp"
#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/device/device_constants.hpp"
#include "zhinst/core/error_messages.hpp"
#include "zhinst/ast/eval_results.hpp"
#include "zhinst/ast/eval_result_value.hpp"
#include "zhinst/runtime/resources.hpp"
#include "zhinst/core/types.hpp"
#include "zhinst/waveform/wavetable_front.hpp"
#include "zhinst/waveform/waveform_front.hpp"
#include "zhinst/waveform/waveform_generator.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace zhinst {

namespace {
inline void appendSuser(std::vector<AsmList::Asm>& vec, std::shared_ptr<AsmCommands> const& cmds,
                        AsmRegister reg, detail::AddressImpl<unsigned int> addr) {
    vec.push_back(cmds->suser(reg, addr));
}
} // anonymous namespace

extern ErrorMessages errMsg;

std::shared_ptr<EvalResults> CustomFunctions::setTrigger(                                                                                                                   // @0x1454c0 (1552B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setTrigger", kDevPreSHFLI);
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(SetTriggerArgs));
    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto const& arg = args[0];
    if (arg.varType_ == VarType_Var) {
        AsmRegister reg = arg.reg_;
        auto asmEntry = asmCommands_->strig(reg);
        results->assemblers_.push_back(std::move(asmEntry));
    } else if (isConstOrCvar(arg.varType_)) {
        int regNum = Resources::getRegisterNumber();
        AsmRegister newReg(regNum);
        auto addiEntries = asmCommands_->addi(newReg, AsmRegister(0), Immediate(arg.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto asmEntry = asmCommands_->strig(newReg);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::format(SetTriggerArgs));
    }
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getTrigger(                                                                                                                   // @0x145ad0 (~1.6KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // No checkFunctionSupported — validates arg directly
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, std::string("getTrigger")));
    auto const& arg = args[0];
    if (!isConstOrCvar(arg.varType_))
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, std::string("getTrigger")));
    auto results = std::make_shared<EvalResults>();
    int regNum1 = Resources::getRegisterNumber();
    int regNum2 = Resources::getRegisterNumber();
    AsmRegister reg1(regNum1);
    AsmRegister reg2(regNum2);
    // addi(reg2, R0, Immediate(arg.toInt())) → ltrig(reg1) → andr(reg1, reg2)
    auto addiEntries = asmCommands_->addi(reg2, AsmRegister(0), Immediate(arg.value_.toInt()));
    for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
    auto ltrigEntry = asmCommands_->ltrig(reg1);
    results->assemblers_.push_back(std::move(ltrigEntry));
    auto andrEntry = asmCommands_->andr(reg1, reg2);
    results->assemblers_.push_back(std::move(andrEntry));
    results->setValue(VarType_Var, regNum1);
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setInternalTrigger(                                                                                                           // @0x146140 (~1.5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setInternalTrigger", kDevLIFamily);
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(SetInternalTriggerArgs));
    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto const& arg = args[0];
    if (arg.varType_ == VarType_Var) {
        // Var arg: use the register binding directly (mirrors setTrigger @0x1455e5).
        // Calling arg.value_.toInt() here would throw "unspecified value type"
        // because var-typed EvalResultValues have no constant value. (IF-203)
        AsmRegister reg = arg.reg_;
        auto asmEntry = asmCommands_->sinttrig(reg);
        results->assemblers_.push_back(std::move(asmEntry));
    } else if (isConstOrCvar(arg.varType_)) {
        int regNum = Resources::getRegisterNumber();
        AsmRegister newReg(regNum);
        auto addiEntries = asmCommands_->addi(newReg, AsmRegister(0), Immediate(arg.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto asmEntry = asmCommands_->sinttrig(newReg);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::format(SetInternalTriggerArgs));
    }
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getAnaTrigger(                                                                                                         // @0x146740 (~3.2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("getAnaTrigger", kDevCervino);
    if (args.size() != 1)                                                                          // @0x1467af: cmp rax,0x38
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, std::string("getAnaTrigger")));
    auto const& arg = args[0];
    if (!isConstOrCvar(arg.varType_))                                                // @0x14685a: and eax,0xfffffffd; cmp eax,0x4
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, std::string("getAnaTrigger")));
    // Read the trigger constant based on arg value (1 or 2)
    EvalResultValue localArg = arg;                                                                // @0x1468b1..1468cb: copy arg, call toInt
    int argVal = localArg.value_.toInt();
    if (argVal == 1) {                                                                             // @0x1468d5: cmp eax,0x1
        auto erv = res->readConst("AWG_ANA_TRIGGER1", EDirection::eOUT);                           // @0x146907: call readConst
        localArg = erv;
    } else if (argVal == 2) {                                                                      // @0x1468d2: cmp eax,0x2
        auto erv = res->readConst("AWG_ANA_TRIGGER2", EDirection::eOUT);                           // @0x14695e: call readConst
        localArg = erv;
    } else {
        // deviceType==2 check: if arg is in range 3..9, fall through to register allocation
        if (config_->deviceType != AwgDeviceType::HDAWG)                                          // @0x1469e5: cmp [rcx],0x2
            throw CustomFunctionsException(
                ErrorMessages::format(IndexMustBe,
                    std::string("getAnaTrigger"), std::string("HDAWG")));
        if (static_cast<unsigned int>(argVal - 3) >= 6u)                                           // @0x1469f1: add eax,0xfffffffd; cmp eax,0x6
            throw CustomFunctionsException(
                ErrorMessages::format(IndexMustBe,
                    std::string("getAnaTrigger"), std::string("SHFSG/SHFQC")));
    }
    // Allocate two registers and build the EvalResults                                            // @0x1469fa
    int regNum1 = Resources::getRegisterNumber();
    int regNum2 = Resources::getRegisterNumber();
    AsmRegister reg1(regNum1);
    AsmRegister reg2(regNum2);
    auto results = std::make_shared<EvalResults>();                                                // @0x146a1a: new 0x98
    // addi(reg2, R0, constVal) — binary passes reg2 (mask reg) as dst    // @0x146ace: call addi
    auto addiEntries = asmCommands_->addi(reg2, AsmRegister(0), Immediate(localArg.value_.toInt()));
    for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
    // ltrig(reg1)                                                                                 // @0x146be4: call ltrig
    auto ltrigEntry = asmCommands_->ltrig(reg1);
    results->assemblers_.push_back(std::move(ltrigEntry));
    // andr(reg1, reg2)                                                                            // @0x146cd6: call andr
    auto andrEntry = asmCommands_->andr(reg1, reg2);
    results->assemblers_.push_back(std::move(andrEntry));
    // newLabel("atzero") → brz(reg1, label, false) → asmOne(reg1) → asmLabel(label)              // @0x146dda..146e4d
    std::string label = Resources::newLabel("atzero");
    auto brzEntry = asmCommands_->brz(reg1, label, false);
    auto oneEntry = asmCommands_->asmOne(reg1);
    auto lblEntry = asmCommands_->asmLabel(label);
    // Batch insert 3 asm entries                                                                  // @0x146e78: init_with_size 3
    std::vector<AsmList::Asm> batch = {std::move(brzEntry), std::move(oneEntry), std::move(lblEntry)};
    results->assemblers_.insert(results->assemblers_.end(),
        std::make_move_iterator(batch.begin()), std::make_move_iterator(batch.end()));
    results->setValue(VarType_Var, static_cast<int>(reg1));                                         // @0x146ff1: call setValue(VarType,int)
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getDigTrigger(                                                                                                         // @0x147420 (~3.2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // Binary: no checkFunctionSupported at top — validates arg first, then deviceType
    if (args.size() != 1)                                                                          // @0x147442: cmp rax,0x38
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, std::string("getDigTrigger")));
    auto const& arg = args[0];
    if (!isConstOrCvar(arg.varType_))                                                // @0x14750e: and eax,0xfffffffd; cmp eax,0x4
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, std::string("getDigTrigger")));
    // Read the trigger constant based on arg value (1 or 2).
    // The binary uses TWO EvalResultValues: `localArg` (copy of arg, used only
    // for the argVal dispatch) and `resolvedArg` (populated only by readConst
    // for arg=1/2; stays default-initialized otherwise). The addi emission
    // reads from resolvedArg, NOT localArg. For arg >= 3 on HDAWG, resolvedArg
    // retains its default (Value(0)), so the mask is always 0.
    EvalResultValue localArg = arg;
    EvalResultValue resolvedArg{};                                                                 // default: value_.toInt() == 0
    resolvedArg.value_ = Value(static_cast<int32_t>(0));                                           // ensure toInt() returns 0
    int argVal = localArg.value_.toInt();                                                          // @0x147570: call toInt
    if (argVal == 1) {                                                                             // @0x147581: cmp eax,0x1
        resolvedArg = res->readConst("AWG_DIG_TRIGGER1", EDirection::eOUT);                        // @0x1475b1: call readConst
    } else if (argVal == 2) {                                                                      // @0x14757f: cmp eax,0x2
        resolvedArg = res->readConst("AWG_DIG_TRIGGER2", EDirection::eOUT);                        // @0x14760e: call readConst
    } else {
        // deviceType==2 check                                                                     // @0x14768c..14769e
        if (config_->deviceType != AwgDeviceType::HDAWG)
            throw CustomFunctionsException(
                ErrorMessages::format(IndexMustBe,
                    std::string("getDigTrigger"), std::string("HDAWG")));
        if (static_cast<unsigned int>(argVal - 3) >= 6u)
            throw CustomFunctionsException(
                ErrorMessages::format(IndexMustBe,
                    std::string("getDigTrigger"), std::string("SHFSG/SHFQC")));
    }
    // Allocate two registers and build the EvalResults                                            // @0x1476a4
    int regNum1 = Resources::getRegisterNumber();
    int regNum2 = Resources::getRegisterNumber();
    AsmRegister reg1(regNum1);
    AsmRegister reg2(regNum2);
    auto results = std::make_shared<EvalResults>();
    // addi(reg2, R0, constVal) → ltrig(reg1) → andr(reg1, reg2)
    // reg2 holds the trigger mask, reg1 receives the trigger register value.
    // The result (masked trigger) ends up in reg1.                            // @0x147930..147ac9
    auto addiEntries = asmCommands_->addi(reg2, AsmRegister(0), Immediate(resolvedArg.value_.toInt()));
    for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
    auto ltrigEntry = asmCommands_->ltrig(reg1);
    results->assemblers_.push_back(std::move(ltrigEntry));
    auto andrEntry = asmCommands_->andr(reg1, reg2);
    results->assemblers_.push_back(std::move(andrEntry));
    // newLabel("dtzero") → brz → asmOne → asmLabel                                               // @0x147a5e..147ac9
    std::string label = Resources::newLabel("dtzero");
    auto brzEntry = asmCommands_->brz(reg1, label, false);
    auto oneEntry = asmCommands_->asmOne(reg1);
    auto lblEntry = asmCommands_->asmLabel(label);
    std::vector<AsmList::Asm> batch = {std::move(brzEntry), std::move(oneEntry), std::move(lblEntry)};
    results->assemblers_.insert(results->assemblers_.end(),
        std::make_move_iterator(batch.begin()), std::make_move_iterator(batch.end()));
    results->setValue(VarType_Var, static_cast<int>(reg1));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setInt(                                                                                                                // @0x1480d0 (~2.5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("setInt", static_cast<AwgDeviceType>(UHFLI | HDAWG | UHFQA));
    if (args.size() != 2)                                                                          // @0x148131: cmp rax,0x70
        throw CustomFunctionsException(
            ErrorMessages::format(SetIntArgs));
    auto const& arg0 = args[0];
    auto const& arg1 = args[1];
    // Validate: arg0 must be string (varType_==3)                                                 // @0x1483bc: cmp [rbp-0x98],0x3
    if (arg0.varType_ != VarType_String)
        throw CustomFunctionsException(
            ErrorMessages::format(SetIntArgs));
    // Validate: arg1 must be numeric (bitmask 0x54: bits 2,4,6 = types 2,4,6)                    // @0x1483d5..1483dd: bt 0x54,ecx
    int arg1Type = static_cast<int>(arg1.varType_);
    if (arg1Type > 6 || !((0x54 >> arg1Type) & 1))
        throw CustomFunctionsException(
            ErrorMessages::format(SetIntVarConstSecond, std::string("setInt")));
    // Call writeToNode(arg0, arg1, defaultEvalResultValue, res)                                   // @0x1486f2: call writeToNode
    EvalResultValue defaultTypeArg{};
    defaultTypeArg.varType_ = VarType_String;
    defaultTypeArg.value_ = Value(1.0);
    defaultTypeArg.varSubType_ = VarSubType(2);
    return writeToNode(arg0, arg1, defaultTypeArg, std::move(res));    // @0x1486f2
}
std::shared_ptr<EvalResults> CustomFunctions::setDouble(                                                                                                             // @0x148ac0 (~3.3KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("setDouble", static_cast<AwgDeviceType>(UHFLI | HDAWG | UHFQA));
    // Accept 2 or 3 args: (size & ~1) == 2                                                       // @0x148b36..148b3a: and rcx,0xfffffffffffffffe; cmp rcx,0x2
    size_t sz = args.size();
    if ((sz & ~static_cast<size_t>(1)) != 2)
        throw CustomFunctionsException(
            ErrorMessages::format(SetDoubleArgs));
    auto const& arg0 = args[0];
    auto const& arg1 = args[1];
    // Default 3rd arg: VarType_Const, double 1.0, VarSubType(2)                                      // @0x148d88..148dd0
    EvalResultValue arg2{};
    arg2.varType_ = VarType_Const;
    arg2.value_ = Value(1.0);
    arg2.varSubType_ = VarSubType(2);
    // If 3 args provided, copy the 3rd arg over                                                   // @0x148df6: cmp rax,0xa8
    if (args.size() == 3) {
        arg2 = args[2];
    }
    // Validate: arg0 must be string (varType_==3)                                                 // @0x148e33: cmp [rbp-0x98],0x3
    if (arg0.varType_ != VarType_String)
        throw CustomFunctionsException(
            ErrorMessages::format(SetDoubleArgs));
    // Validate: arg1 must be numeric (bitmask 0x54)                                               // @0x148e43..148e51: bt 0x54,eax
    int arg1Type = static_cast<int>(arg1.varType_);
    if (arg1Type > 6 || !((0x54 >> arg1Type) & 1))
        throw CustomFunctionsException(
            ErrorMessages::format(SetDoubleVarConstSecond, std::string("setDouble")));
    // Validate: arg2.varType_ must be int type ((varType & ~1) == 4)                              // @0x148e5a..148e63: and eax,0xfffffffd; cmp eax,0x4
    if (!isConstOrCvar(arg2.varType_))
        throw CustomFunctionsException(
            ErrorMessages::format(SetDoubleConstThird, std::string("setDouble")));
    // Call writeToNode(arg0, arg1, arg2, res)                                                     // @0x149186: call writeToNode
    return writeToNode(arg0, arg1, arg2, std::move(res));       // @0x149186
}
std::shared_ptr<EvalResults> CustomFunctions::generate(                                                                                                              // @0x149940 (~2.8KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // No checkFunctionSupported
    if (args.empty())                                                                              // @0x14995e: cmp rbx,[rdx+0x8]
        throw CustomFunctionsException(
            ErrorMessages::format(GenerateExpectsString));
    auto const& firstArg = args[0];
    // First arg must be string (varType_==3)                                                      // @0x149a23: cmp eax,0x3
    if (firstArg.varType_ != VarType_String)
        throw CustomFunctionsException(
            ErrorMessages::format(GenerateExpectsString));
    // If single arg and first arg has VarSubType==2 (marker), return early                        // @0x149a4b: cmp rax,0x1; jbe
    if (args.size() <= 1) {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncNoArgsGiven,
                firstArg.value_.toString()));
    }
    // Check if second arg has VarSubType==2 → early return with setValue                          // @0x149bb1: cmp [rbx-0x28],0x2 (varType_); 149bbb: cmp [r12+0x4],0x2 (varSubType_)
    if (args[1].varType_ == VarType_Var && args[1].varSubType_ == VarSubType_FunctionArg) {  // @0x149bc1: je 149d57
        auto results = std::make_shared<EvalResults>();
        Value emptyVal;                                                                            // @0x149d61..149d77: VarType_Const, default Value
        results->setValue(VarType_Wave, VarSubType(2), emptyVal);                                    // @0x149d8c: call setValue(VarType,VarSubType,Value)
        return results;
    }
    // Build EvalResults and gather args[1..end] into vector<Value>, skipping VarSubType==2        // @0x149a55..149c48
    auto results = std::make_shared<EvalResults>();
    // Copy args[1..end] into local vector
    std::vector<EvalResultValue> localArgs(args.begin() + 1, args.end());
    // Build Value vector from localArgs, skipping entries with VarSubType==2
    std::vector<Value> values;
    values.reserve(localArgs.size());
    for (auto const& erv : localArgs) {
        if (erv.varSubType_ == VarSubType_FunctionArg) {                                             // @0x149bad: cmp [rbx-0x28],0x2
            // Marker entry with VarSubType==2 → throw error 0x67                                  // @0x149f92: throw 0x67
            throw CustomFunctionsException(
                ErrorMessages::format(CantCallWithVar,
                    firstArg.value_.toString()));
        }
        values.push_back(erv.value_);                                                              // @0x149bd4..149bf4: copy Value
    }
    // Call waveformGen_->eval(name, values)                                                       // @0x149c4f..149c78
    std::string name = firstArg.value_.toString();                                                 // @0x149c5e: call Value::toString
    auto genResult = waveformGen_->eval(name, values);                                             // @0x149c78: call WaveformGenerator::eval
    // Move result into output                                                                     // @0x149c8e..149c99
    return genResult;
}
std::shared_ptr<EvalResults> CustomFunctions::setUserReg(                                                                                                            // @0x14a420 (~4KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("setUserReg", kDevAll);
    if (args.size() != 2)                                                                          // @0x14a48d: cmp rax,0x70
        throw CustomFunctionsException(
            ErrorMessages::format(SetUserRegArgs));
    auto const& arg0 = args[0];
    auto const& arg1 = args[1];
    // Validate: arg0 must be int type ((varType & ~1) == 4)                                       // @0x14a5f2..14a5f5: and eax,0xfffffffd; cmp eax,0x4
    // Throw site @0x14b22b: error 0xc5 = SetUserRegConstFirst ("expects a const as first argument")
    if (!isConstOrCvar(arg0.varType_))
        throw CustomFunctionsException(
            ErrorMessages::format(SetUserRegConstFirst));
    // Range-check arg0 against devConst_->memoryDepth                                             // @0x14a60a..14a624
    // Throw site @0x14b264: error 0xc6 = SetUserRegRange ("register must be in the range of 0 to 15")
    // thrown as CustomFunctionsValueException with argIndex = 1.
    int arg0Val = arg0.value_.toInt();
    if (static_cast<int64_t>(arg0Val) >= static_cast<int64_t>(devConst_->memoryDepth) || arg0Val < 0) {
        // Check if varSubType_ == 2 (allows bypass)                                               // @0x14a62d: cmp [rbp-0x11c],0x2
        if (arg0.varSubType_ != VarSubType_FunctionArg)
            throw CustomFunctionsValueException(
                ErrorMessages::get(SetUserRegRange), 1);
    }
    // Create results with VarType_Void                                                              // @0x14a633..14a65e
    auto results = std::make_shared<EvalResults>(VarType_Void);
    int regNum = Resources::getRegisterNumber();                                                   // @0x14a66e: call getRegisterNumber
    AsmRegister reg(regNum);
    // Branch on arg1 type                                                                         // @0x14a67e: cmp eax,0x2
    if (arg1.varType_ == VarType_Var) {
        // arg1 is a register: suser(arg1.reg_, arg0.toInt())                                            // @0x14a6b3: call suser
        appendSuser(results->assemblers_, asmCommands_, arg1.reg_, detail::AddressImpl<unsigned int>(arg0.value_.toInt()));
    } else if (isConstOrCvar(arg1.varType_)) {                                     // @0x14a71e: and eax,0xfffffffd; cmp eax,0x4
        // arg1 is int: addi(reg, R0, arg1.toInt()) then suser(reg, arg0.toInt())                  // @0x14a774: call addi; @0x14a8d4: call suser
        auto addiEntries = asmCommands_->addi(reg, AsmRegister(0), Immediate(arg1.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(arg0.value_.toInt()));
    } else {
        // Throw site @0x14b2a8: error 0xc8 = SetUserRegVarConst ("expects a var or const as second argument")
        throw CustomFunctionsException(
            ErrorMessages::format(SetUserRegVarConst));
    }
    // addi(reg, R0, Imm(0xb)) + suser(reg, 0x10)                                                 // @0x14a9b8..14aaf9
    {
        auto addiEntries = asmCommands_->addi(reg, AsmRegister(0), Immediate(0xb));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserNodeTag));
    }
    // addi(reg, R0, arg0.toInt()) + suser(reg, 0x11)                                             // @0x14abee..14ad29
    {
        auto addiEntries = asmCommands_->addi(reg, AsmRegister(0), Immediate(arg0.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserNodeAddr));
    }
    // If deviceType==2: luser(reg2, 0) + suser(reg2, 0) + addSyncCommand                         // @0x14adec: cmp [rax],0x2
    if (config_->deviceType == HDAWG) {
        int regNum2 = Resources::getRegisterNumber();                                              // @0x14adf5: call getRegisterNumber
        AsmRegister reg2(regNum2);
        auto luserEntry = asmCommands_->luser(reg2, detail::AddressImpl<unsigned int>(0));          // @0x14ae1d: call luser(reg2, 0)
        results->assemblers_.push_back(std::move(luserEntry));
        appendSuser(results->assemblers_, asmCommands_, reg2, detail::AddressImpl<unsigned int>(0)); // @0x14aef5: call suser(reg2, 0)
    }
    // trap()                                                                                      // @0x14afc7: call trap
    {
        auto trapEntry = asmCommands_->trap();
        results->assemblers_.push_back(std::move(trapEntry));
    }
    // If numChannelGroups >= 2: addSyncCommand(results, res)                                      // @0x14b08a: cmp [rax+0x1c],0x2
    if (config_->numChannelGroups >= 2) {
        addSyncCommand(results, std::move(res));                                                   // @0x14b0e0: call addSyncCommand
    }
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getUserReg(  // @0x14b480 (2070B, ends ~0x14bca0)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x14b4a4..0x14b4db: checkFunctionSupported("getUserReg", 0x1ff)
    checkFunctionSupported("getUserReg", kDevAll);

    // @0x14b4e0..0x14b4ee: args.size() == 1
    if (args.size() != 1) {                                                          // @0x14b4ee: jne error
        throw CustomFunctionsException(
            ErrorMessages::get(GetUserRegArgs));                   // @0x14badb
    }

    // @0x14b4f4..0x14b557: allocate EvalResults (default ctor, no VarType arg)
    auto results = std::make_shared<EvalResults>();

    // @0x14b563..0x14b5ec: copy arg[0] into local EvalResultValue
    EvalResultValue arg0 = args[0];

    // @0x14b5e9..0x14b5ef: check arg is int type ((varType & ~1) == 4)
    if (!isConstOrCvar(arg0.varType_)) {                               // @0x14b5ef: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, "getUserReg"));  // @0x14bb21
    }

    // @0x14b5f5..0x14b617: range check: 0 <= arg.toInt() < devConst_->memoryDepth (+0x30)
    int userRegIdx = arg0.value_.toInt();                                            // @0x14b5fc
    if (static_cast<int64_t>(userRegIdx) >= static_cast<int64_t>(devConst_->memoryDepth) // @0x14b607: cmp [rcx+0x30], rax
        || userRegIdx < 0) {                                                         // @0x14b615: test eax,eax; jns ok
        if (arg0.varType_ != VarType_Var) {                                  // @0x14b619: cmp [rbp-0x7c], 2; jne throw
            throw CustomFunctionsValueException(
                ErrorMessages::get(GetUserRegRange), 1);            // @0x14bb6f
        }
        // varType==2 means the value is runtime-determined; allow through
    }

    // @0x14b623: getRegisterNumber
    int regNum = Resources::getRegisterNumber();                                     // @0x14b623
    AsmRegister reg(regNum);                                                         // @0x14b62e

    // @0x14b633..0x14b652: luser(reg, userRegIdx). Phase S.2 M5
    // dropped the redundant `int userRegInt = arg0.value_.toInt()` —
    // the binary calls `Value::toInt()` again here, but the result
    // is identical to `userRegIdx` computed at the range check above.
    auto luserEntry = asmCommands_->luser(reg, detail::AddressImpl<unsigned int>(userRegIdx)); // @0x14b652
    results->assemblers_.push_back(std::move(luserEntry));

    // @0x14b72d..0x14b747: setValue(VarType_Var, regNum)
    results->setValue(VarType_Var, static_cast<int>(reg));                             // @0x14b747

    // @0x14b74c..0x14b752: if config_->deviceType == 2 (HDAWG)
    if (static_cast<int>(config_->deviceType) == 2) {                                // @0x14b752: jne skip_hdawg
        // @0x14b758: second getRegisterNumber
        int regNum2 = Resources::getRegisterNumber();                                // @0x14b758
        AsmRegister reg2(regNum2);                                                   // @0x14b763

        // @0x14b768..0x14b7ab: addi(reg2, AsmRegister(0), Immediate(devConst_->seqClockDivider))
        AsmRegister zero(0);                                                         // @0x14b779
        int immVal = devConst_->seqClockDivider;                                     // @0x14b782: [rax+0x68]
        auto addiEntries = asmCommands_->addi(reg2, zero, Immediate(immVal));        // @0x14b7ab

        // @0x14b7b0..0x14b7e2: insert addi results into results->assemblers_
        for (auto& e : addiEntries)
            results->assemblers_.push_back(std::move(e));

        // @0x14b8b5..0x14b8c9: suser(reg2, 0x69)
        appendSuser(results->assemblers_, asmCommands_, reg2, detail::AddressImpl<unsigned int>(kSuserWaitCycles)); // @0x14b8c9
    }

    // @0x14b9a3: if config_->numChannelGroups >= 2
    if (config_->numChannelGroups >= 2) {                                                  // @0x14b9a7: jl skip_sync
        // @0x14b9ad..0x14b9f9: addSyncCommand(results, res)
        addSyncCommand(results, res);                                                // @0x14b9f9
    }

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getSweeperLength(  // @0x14bca0 (1734B, ends ~0x14c370)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x14bcc0..0x14bceb: checkFunctionSupported("getSweeperLength", 0x5)
    checkFunctionSupported("getSweeperLength", kDevCervino);

    // @0x14bcf0..0x14bcfe: args.size() == 1
    if (args.size() != 1) {                                                          // @0x14bcfe: jne error
        throw CustomFunctionsException(
            ErrorMessages::get(GetSweeperLenArgs));                   // @0x14c167
    }

    // @0x14bd04..0x14bda3: copy arg[0] into local EvalResultValue
    EvalResultValue arg0 = args[0];

    // @0x14bda3..0x14bda9: check arg is int type ((varType & ~1) == 4)
    if (!isConstOrCvar(arg0.varType_)) {                               // @0x14bda9: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, "getSweeperLength")); // @0x14c1ad
    }

    // @0x14bdaf..0x14bdc1: range check: arg.toInt() must be <= 1 (or varType==2 fallback)
    int sweepIdx = arg0.value_.toInt();                                              // @0x14bdb9
    if (sweepIdx != 1) {                                                             // @0x14bdc1: cmp eax,1; je ok
        if (arg0.varType_ != VarType_Var) {                                  // @0x14bdc3: cmp varType, 2; jne throw
            throw CustomFunctionsValueException(
                ErrorMessages::get(GetSweeperLenArg), 1);            // @0x14c1f8
        }
    }

    // @0x14bdd0..0x14bdfa: build intermediate EvalResultValue for readConst result
    // Initialize an empty EvalResultValue with AsmRegister(-1) placeholder
    EvalResultValue constResult;
    constResult.varType_ = VarType_Unset;
    constResult.varSubType_ = VarSubType(0);

    // @0x14bdff..0x14bf04: choose constant name based on sweepIdx
    sweepIdx = arg0.value_.toInt();                                                  // @0x14be02: second toInt call
    if (sweepIdx == 2) {
        // @0x14be10..0x14be89: readConst("AWG_USERREG_SWEEP_COUNT1", EDirection::eOUT)
        EvalResultValue rc = res->readConst("AWG_USERREG_SWEEP_COUNT1",
                                             EDirection::eOUT);            // @0x14be61
        constResult.varType_ = rc.varType_;
        constResult.value_ = rc.value_;
    } else {
        // @0x14be8b..0x14bf04: readConst("AWG_USERREG_SWEEP_COUNT0", EDirection::eOUT)
        EvalResultValue rc = res->readConst("AWG_USERREG_SWEEP_COUNT0",
                                             EDirection::eOUT);            // @0x14bedc
        constResult.varType_ = rc.varType_;
        constResult.value_ = rc.value_;
    }

    // @0x14bf58: getRegisterNumber
    int regNum = Resources::getRegisterNumber();                                     // @0x14bf58
    AsmRegister reg(regNum);                                                         // @0x14bf63

    // @0x14bf68..0x14bfce: allocate EvalResults (default ctor)
    auto results = std::make_shared<EvalResults>();

    // @0x14bfd2..0x14bff1: luser(reg, constResult.value_.toInt())
    int constVal = constResult.value_.toInt();                                       // @0x14bfdd
    auto luserEntry = asmCommands_->luser(reg, detail::AddressImpl<unsigned int>(constVal)); // @0x14bff1
    results->assemblers_.push_back(std::move(luserEntry));

    // @0x14c0cb..0x14c0e1: setValue(VarType_Var, regNum)
    results->setValue(VarType_Var, static_cast<int>(reg));                             // @0x14c0e1

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setPrecompClear(                                                                                                              // @0x14c720 (~1KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setPrecompClear", HDAWG);
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(SetPrecompOneConst));
    if (!isConstOrCvar(args[0].varType_))
        throw CustomFunctionsException(
            ErrorMessages::format(SetPrecompConst));
    auto results = std::make_shared<EvalResults>(VarType_Void);
    int val = args[0].value_.toInt();
    unsigned int flag = (val != 0) ? 1u : 0u;
    auto asmEntry = asmCommands_->asmSetPrecompFlags(flag);
    results->node_ = asmEntry.node;  // chain into AST next-chain (original @0x14c940)
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setWaveDIO(                                                                                                                   // @0x14cae0 (200B)
    std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) {
    // Unconditionally throws — function is deprecated/unimplemented in binary
    throw CustomFunctionsException("setWaveDIO is not supported");
}
std::shared_ptr<EvalResults> CustomFunctions::at(                                                                                                                             // @0x14ce30 (~2.5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x14ce54..0x14ce7a: checkFunctionSupported("at", 0x5)
    checkFunctionSupported("at", kDevCervino);

    // @0x14ce7f..0x14ce8d: args.size() == 1 (byte size == 0x38)
    if (args.size() != 1)                                                                        // @0x14ce89: cmp rax,0x38
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstConst, "at"));                      // @0x14d5f1

    // @0x14ce93..0x14cf2e: copy arg[0] into local
    EvalResultValue arg0 = args[0];                                                              // @0x14ce93

    // @0x14cf35..0x14cf8a: if varType == 1 (bool), warn with error 0x36
    if (arg0.varType_ == VarType_Void) {                                                  // @0x14cf35: cmp eax,1
        std::string warnMsg = ErrorMessages::format(FuncCalledWithLogical, "at");     // @0x14cf41..0x14cf4d
        if (warningCallback_) {                                                                  // @0x14cf59
            warningCallback_(warnMsg);                                                               // @0x14cf6c: call [rax+0x30]
        }
    }

    // @0x14cf8f..0x14cff6: make_shared<EvalResults>() — default ctor, NOT VarType_Void
    auto results = std::make_shared<EvalResults>();                                               // @0x14cf94

    // @0x14cffa..0x14d007: check arg0.varType_ == VarType_Var (register type)
    if (arg0.varType_ == VarType_Var) {                                                  // @0x14d000: cmp eax,2
        // @0x14d00d..0x14d024: suser(arg0.asmRegister_, 0x1d) — arg already in register
        appendSuser(results->assemblers_, asmCommands_, arg0.reg_, detail::AddressImpl<unsigned int>(kSuserRTLoggerData)); // @0x14d024                                   // @0x14d029..0x14d090
    } else {
        // @0x14d095..0x14d0fb: int type path — getRegisterNumber, addi, then suser
        if (!isConstOrCvar(arg0.varType_))                                        // @0x14d095: and eax,0xfffffffd; cmp eax,4
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "at"));                  // @0x14d648

        int regNum = Resources::getRegisterNumber();                                             // @0x14d0a1
        AsmRegister reg(regNum);                                                                 // @0x14d0ac

        AsmRegister zero(0);                                                                     // @0x14d0c2
        int val = arg0.value_.toInt();                                                           // @0x14d0ce
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(val));                        // @0x14d0fb

        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));                // @0x14d100..0x14d136

        // @0x14d22b..0x14d243: suser(reg, 0x1d)
        appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserRTLoggerData)); // @0x14d243                                   // @0x14d248..0x14d2a8
    }

    // @0x14d303..0x14d311: second getRegisterNumber → reg2
    int regNum2 = Resources::getRegisterNumber();                                                // @0x14d303
    AsmRegister reg2(regNum2);                                                                   // @0x14d311

    // @0x14d316..0x14d360: readConst("AWG_TIME_TRIGGER", EDirection::eOUT), addi
    AsmRegister zero2(0);                                                                        // @0x14d32e
    EvalResultValue timeTrigErv = res->readConst("AWG_TIME_TRIGGER", EDirection::eOUT); // @0x14d360
    int timeTrigVal = timeTrigErv.value_.toInt();                                                // @0x14d36c
    auto addiEntries2 = asmCommands_->addi(reg2, zero2, Immediate(timeTrigVal));                 // @0x14d396

    for (auto& e : addiEntries2) results->assemblers_.push_back(std::move(e));                   // @0x14d39b..0x14d3cb

    // @0x14d4ce..0x14d4e7: wtrig(reg2, reg2) — both operands same register
    auto wtrigEntry = asmCommands_->wtrig(reg2, reg2);                                           // @0x14d4e7
    results->assemblers_.push_back(std::move(wtrigEntry));                                       // @0x14d4ec..0x14d55b

    return results;                                                                              // @0x14d5bd
}
std::shared_ptr<EvalResults> CustomFunctions::lock(                                                                                                                         // @0x14dc70 (1286B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("lock", kDevCervino);
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(LockArgs));
    if (args[0].varType_ != VarType_Wave)
        throw CustomFunctionsException(
            ErrorMessages::format(LockOnlyWave));
    std::string name = args[0].value_.toString();
    std::optional<std::string> optName(name);
    auto wf = wavetableFront_->getWaveformByName(optName);
    if (!wf)
        throw CustomFunctionsValueException(
            ErrorMessages::format(WaveformNotExist, name), 0);
    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto asmEntry = asmCommands_->asmLockPlaceholder(wf, config_->deviceIndex);
    results->node_ = asmEntry.node;  // @0x14de27-0x14de47: results->node_ = asmEntry.node
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::unlock(                                                                                                                       // @0x14e180 (1295B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("unlock", kDevCervino);
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(UnlockArgs));
    if (args[0].varType_ != VarType_Wave)
        throw CustomFunctionsException(
            ErrorMessages::format(UnlockOnlyWave));
    std::string name = args[0].value_.toString();
    std::optional<std::string> optName(name);
    auto wf = wavetableFront_->getWaveformByName(optName);
    if (!wf)
        throw CustomFunctionsValueException(
            ErrorMessages::format(WaveformNotExist, name), 0);
    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto asmEntry = asmCommands_->asmUnlockPlaceholder(wf, config_->deviceIndex);
    results->node_ = asmEntry.node;  // @0x14e33d-0x14e35c: results->node_ = asmEntry.node
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getCnt(  // @0x14e8d0 (1258B, ends ~0x14edba)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // @0x14e8ed..0x14e91d: checkFunctionSupported("getCnt", 0x2)
    checkFunctionSupported("getCnt", HDAWG);

    // @0x14e922..0x14e930: args.size() == 1
    if (args.size() != 1) {                                                          // @0x14e930: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, "getCnt"));      // @0x14ebc5
    }

    // @0x14e936..0x14e93c: config_->deviceType == 2 (HDAWG)
    if (static_cast<int>(config_->deviceType) != 2) {                                // @0x14e93c: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstConst, "getCnt"));      // @0x14ec1a
    }

    // @0x14e942..0x14e99a: allocate EvalResults (default ctor)
    auto results = std::make_shared<EvalResults>();

    // @0x14e9a1..0x14ea26: copy arg[0] into local EvalResultValue
    EvalResultValue arg0 = args[0];

    // @0x14ea2f..0x14ea35: check arg is int type ((varType & ~1) == 4)
    if (!isConstOrCvar(arg0.varType_)) {                               // @0x14ea35: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, "getCnt"));      // @0x14ec6f
    }

    // @0x14ea3b..0x14ea4e: range check: counterIdx < numCounters
    int counterIdx = arg0.value_.toInt();                                            // @0x14ea42
    if (counterIdx >= devConst_->numCounters) {                                      // @0x14ea4e: jl ok
        if (arg0.varType_ != VarType_Var) {                                  // @0x14ea50: cmp varType, 2; jne throw
            throw CustomFunctionsValueException(
                ErrorMessages::get(GetCntRange), 0);            // @0x14ecba
        }
        // varType==2 means runtime-determined; allow through
    }

    // @0x14ea5a: getRegisterNumber
    int regNum = Resources::getRegisterNumber();                                     // @0x14ea5a
    AsmRegister reg(regNum);                                                         // @0x14ea65

    // @0x14ea6a..0x14ea89: lcnt(reg, counterIdx)
    counterIdx = arg0.value_.toInt();                                                // @0x14ea75: second toInt call
    auto lcntEntry = asmCommands_->lcnt(reg, detail::AddressImpl<unsigned int>(counterIdx)); // @0x14ea89
    results->assemblers_.push_back(std::move(lcntEntry));

    // @0x14eb54..0x14eb6a: setValue(VarType_Var, regNum)
    results->setValue(VarType_Var, static_cast<int>(reg));                             // @0x14eb6a

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::waitQAResultTrigger(                                    // @0x14edc0
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {

    // @0x14ede0-0x14ee19: build function name string and check device support
    checkFunctionSupported("waitQAResultTrigger", UHFQA);                    // @0x14ee19: call checkFunctionSupported

    // @0x14ee1e-0x14ee25: args must be empty (args.end == args.begin)
    if (!args.empty())                                                                                 // @0x14ee25: jne 0x14f1ff (throw)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, "waitQAResultTrigger"));           // @0x14f1ff-0x14f24a

    // @0x14ee2b-0x14ee5d: allocate EvalResults with VarType::Integer
    auto results = std::make_shared<EvalResults>(VarType_Void);                                          // @0x14ee30: new(0x98); @0x14ee55: EvalResults(VarType_Void)

    // @0x14ee61-0x14eea2: build local EvalResultValue (erv) with varType=4 (for readConst)
    //   erv at [rbp-0x90]: varType=4, which_=1, value int=0, other fields=0
    //   erv2 at [rbp-0x88]: varType=1, which_=0, value int=0

    // @0x14eea9-0x14eeb6: create placeholder register AsmRegister(-1)
    //   AsmRegister(-1) stored at [rbp-0x60]

    // @0x14eebb-0x14eee8: readConst("AWG_DEMOD_TRIGGER2", EDirection::eOUT) via res
    auto erv = res->readConst("AWG_DEMOD_TRIGGER2", EDirection::eOUT);                                // @0x14eee8: call Resources::readConst

    // @0x14eeed-0x14ef12: copy readConst result into local erv2 (varType, which_, value)
    //   copies varType, which_, and variant value from readConst result
    // @0x14ef17-0x14ef1e: extract AsmRegister from readConst result into [rbp-0x60]

    int trigAddr = erv.value_.toInt();                                                                 // @0x14ef9b: call Value::toInt()

    // @0x14ef22-0x14ef66: cleanup readConst temporary strings

    // @0x14ef6b-0x14ef79: allocate a new register
    int regNum = Resources::getRegisterNumber();                                                       // @0x14ef6e: call getRegisterNumber @0x1e4bb0
    AsmRegister reg(regNum);                                                                           // @0x14ef79: call AsmRegister(int)

    // @0x14ef7e: load asmCommands_ from this->offset_0x50
    // @0x14ef86: load register value from [rbp-0x48]
    // @0x14ef8a-0x14ef93: AsmRegister(0) — zero register
    AsmRegister zero(0);                                                                               // @0x14ef93: call AsmRegister(0)

    // @0x14ef98-0x14efc8: addi(reg, R0, Immediate(trigAddr))
    //   loads the trigger address value into the register
    auto addiEntries = asmCommands_->addi(reg, zero, Immediate(trigAddr));                             // @0x14efc8: call AsmCommands::addi @0x273d60

    // @0x14efcd-0x14effe: insert addi results into results->assemblers_
    for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));                          // @0x14effe: call vector::insert_with_size

    // @0x14f003-0x14f09a: cleanup addi temporary vector (destroy Asm elements, free)

    // @0x14f09f-0x14f0c8: destroy Immediate temporary (vtable dispatch if needed)

    // @0x14f0d2-0x14f0eb: wtrig(reg, reg) — write trigger with same register for both operands
    auto asmEntry = asmCommands_->wtrig(reg, reg);                                                    // @0x14f0eb: call AsmCommands::wtrig @0x274f00

    // @0x14f0f0-0x14f176: push_back wtrig result into results->assemblers_
    results->assemblers_.push_back(std::move(asmEntry));                                               // @0x14f0f0-0x14f176

    // @0x14f1b1-0x14f1b8: destroy Assembler temp
    // @0x14f1bd-0x14f1ed: cleanup local EvalResultValue (erv2), string destruction

    return results;                                                                                    // @0x14f1ed-0x14f1fe: epilogue + ret
}
std::shared_ptr<EvalResults> CustomFunctions::getQAResult(                                                                                                                  // @0x14f380 (700B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("getQAResult", UHFQA);
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, std::string("getQAResult")));
    auto results = std::make_shared<EvalResults>();
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    auto asmEntry = asmCommands_->ld(reg, detail::AddressImpl<unsigned int>(kSuserQAResult));
    results->assemblers_.push_back(std::move(asmEntry));
    results->setValue(VarType_Var, regNum);
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::startQAResult(                                                                                                             // @0x14f620 (~2.7KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("startQAResult", UHFQA);                               // @0x14f67a

    // @0x14f67f: max 2 args (throws 0x45 if >= 3)
    if (args.size() >= 3)                                                                                   // @0x14f688
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsMaxArgs, std::string("startQAResult"),
                                  2, static_cast<int>(args.size())));

    // @0x14f6a2: readConst("QA_INT_ALL", EDirection(1)) — default integration mask
    auto qaIntAllErv = res->readConst("QA_INT_ALL", EDirection::eOUT);                            // @0x14f6d4
    int qaIntAll = qaIntAllErv.value_.toInt();                                                              // @0x14f6e0

    int resultAddr = 0;                                                                                     // @0x14f700: [rbp-0x100] = 0

    // @0x14f70c: if args not empty, first arg overrides qaIntAll
    if (!args.empty()) {
        auto const& arg0 = args[0];
        if (!isConstOrCvar(arg0.varType_))                                                   // @0x14f71e: type check
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, std::string("startQAResult")));
        qaIntAll = arg0.value_.toInt();                                                                     // @0x14f72a

        // @0x14f736: if 2nd arg exists
        if (args.size() >= 2) {
            auto const& arg1 = args[1];
            if (!isConstOrCvar(arg1.varType_))                                               // @0x14f748
                throw CustomFunctionsException(
                    ErrorMessages::format(FuncExpectsConst, std::string("startQAResult")));
            resultAddr = arg1.value_.toInt();                                                               // @0x14f754
        }
    }

    auto results = std::make_shared<EvalResults>(VarType_Void);                                               // @0x14f760

    int regNum = Resources::getRegisterNumber();                                                            // @0x14f810
    AsmRegister reg(regNum);
    AsmRegister zero(0);

    // First pass: addi(reg, R0, (qaIntAll << 16) + resultAddr + 0x10) + strig(reg)                        // @0x14f83c
    {
        int imm = (qaIntAll << 16) + resultAddr + 0x10;
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(imm));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));                           // @0x14f870: insert

        auto asmEntry = asmCommands_->strig(reg);                                                          // @0x14f940: strig
        results->assemblers_.push_back(std::move(asmEntry));
    }

    // Second pass: addi(reg, R0, resultAddr) + strig(reg)                                                  // @0x14fa10
    {
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(resultAddr));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

        auto asmEntry = asmCommands_->strig(reg);                                                          // @0x14fae0: strig
        results->assemblers_.push_back(std::move(asmEntry));
    }

    return results;                                                                                         // @0x14fb88
}
std::shared_ptr<EvalResults> CustomFunctions::startQAMonitor(                                                                                                               // @0x1500b0 (~2.1KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("startQAMonitor", UHFQA);                              // @0x150110

    // @0x150115: max 1 arg (throws 0x45 if >= 2)
    if (args.size() >= 2)                                                                                   // @0x15011e
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsMaxArgs, std::string("startQAMonitor"),
                                  1, static_cast<int>(args.size())));

    int monitorVal = 0;                                                                                     // @0x150140

    // @0x15014c: if 1 arg, extract value
    if (!args.empty()) {
        auto const& arg0 = args[0];
        if (!isConstOrCvar(arg0.varType_))                                                   // @0x15015e
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, std::string("startQAMonitor")));
        monitorVal = arg0.value_.toInt();                                                                   // @0x15016a
    }

    auto results = std::make_shared<EvalResults>(VarType_Void);                                               // @0x150178

    int regNum = Resources::getRegisterNumber();                                                            // @0x150228
    AsmRegister reg(regNum);
    AsmRegister zero(0);

    // First pass: addi(reg, R0, monitorVal + 0x20) + strig(reg)                                           // @0x150254
    {
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(monitorVal + 0x20));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

        auto asmEntry = asmCommands_->strig(reg);                                                          // @0x150324
        results->assemblers_.push_back(std::move(asmEntry));
    }

    // Second pass: addi(reg, R0, monitorVal) + strig(reg)                                                  // @0x1503f4
    {
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(monitorVal));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

        auto asmEntry = asmCommands_->strig(reg);                                                          // @0x1504c4
        results->assemblers_.push_back(std::move(asmEntry));
    }

    return results;                                                                                         // @0x15056c
}
std::shared_ptr<EvalResults> CustomFunctions::executeTableEntry(                                                                                                            // @0x150900 (~2.7KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("executeTableEntry", kDevHirzel);                         // @0x15096a

    // @0x15096f: at least 1 arg required
    if (args.empty())                                                                                       // @0x150978
        throw CustomFunctionsException(
            ErrorMessages::format(FuncMinArgs, std::string("executeTableEntry"),
                                  1, 0));

    auto const& arg0 = args[0];                                                                             // @0x1509a0

    auto results = std::make_shared<EvalResults>(VarType_Void);                                               // @0x1509b0

    // @0x150a60: handle wait cycles from remaining args
    setWaitCyclesReg(args, results, res);                                                                   // @0x150a68

    VarType argType = arg0.varType_;                                                          // @0x150a70

    // Binary dispatch (0x150b35..0x151060):
    //   1. varType==4 → try const-match (ZSYNC_DATA_RAW etc.)
    //      On match → wvft with shifted constant.
    //      On no match AND deviceType==SHFQC_SG → try QA_DATA constants.
    //      On no match → fall through to step 2 (re-dispatch).
    //   2. varType==2 → register path
    //   3. (varType & ~2)==4 → numeric path (direct table entry index)
    //   4. else → throw

    bool constMatched = false;
    if (argType == VarType_Const) {
        // Integer constant path                                                                             // @0x150a80
        int tableIndex = arg0.value_.toInt();                                                               // @0x150a8c

        // Try matching known ZSYNC/QA data constants
        auto zsyncRaw = res->readConst("ZSYNC_DATA_RAW", EDirection::eOUT);                       // @0x150ac0
        if (tableIndex == zsyncRaw.value_.toInt()) {
            // shift = 1                                                                                     // @0x150ad0
            auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                1 << devConst_->execTableIndexBits);                                                             // @0x150ae0: 1 << execTableIndexBits
            results->assemblers_.push_back(std::move(asmEntry));
            constMatched = true;
        } else {
            auto zsyncProcA = res->readConst("ZSYNC_DATA_PROCESSED_A", EDirection::eOUT);         // @0x150b60
            if (tableIndex == zsyncProcA.value_.toInt()) {
                // shift = 9                                                                                 // @0x150b70
                auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                    9 << devConst_->execTableIndexBits);                                                      // @0x150b80
                results->assemblers_.push_back(std::move(asmEntry));
                constMatched = true;
            } else {
                auto zsyncProcB = res->readConst("ZSYNC_DATA_PROCESSED_B", EDirection::eOUT);     // @0x150c00
                if (tableIndex == zsyncProcB.value_.toInt()) {
                    // shift = 0xd                                                                           // @0x150c10
                    auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                        0xd << devConst_->execTableIndexBits);                                                 // @0x150c20
                    results->assemblers_.push_back(std::move(asmEntry));
                    constMatched = true;
                }
                if (!constMatched && config_->deviceType == SHFQC_SG) {
                    // Binary @0x151254: QA_DATA_RAW / QA_DATA_PROCESSED_D have no case in the
                    // const dispatch table for SHFQC_SG — the original crashes with map::at
                    // (binary bug, m[47] absent from binary's own error map). We throw
                    // UnknownError47 explicitly so the error path is faithful; unlike the
                    // binary we have defined m[47] with a message string.
                    auto qaRaw = res->readConst("QA_DATA_RAW", EDirection::eOUT);           // @0x150cd0
                    if (tableIndex == qaRaw.value_.toInt()) {
                        throw CustomFunctionsException(
                            ErrorMessages::get(UnknownError47));                            // @0x151254
                    }
                }
                // Otherwise: fall through to re-dispatch                                   // @0x150eb3→0x150feb
            }
        }
    }

    if (!constMatched) {
        // Re-dispatch: register or numeric path                                                             // @0x150feb
        if (argType == VarType_Var) {
            // Register path                                                                                 // @0x150e80
            auto asmEntry = asmCommands_->wvft(arg0.reg_,
                1 << (devConst_->execTableIndexBits + 1));                                                      // @0x150dfc: 1 << (execTableIndexBits + 1)
            results->assemblers_.push_back(std::move(asmEntry));
        } else if (isConstOrCvar(argType)) {
            // Numeric (int/uint) path — direct table entry index                                            // @0x150f00
            int entryIndex = arg0.value_.toInt();                                                           // @0x150f10
            if (entryIndex < 0)
                throw CustomFunctionsValueException(
                    ErrorMessages::get(ExecTableInvalidIndex), 0);                                   // @0x150f40
            if ((entryIndex >> devConst_->execTableIndexBits) != 0)                                             // @0x150f50: validate index fits
                throw CustomFunctionsValueException(
                    ErrorMessages::get(ExecTableInvalidIndex), 0);                                   // @0x150f80
            auto asmEntry = asmCommands_->wvft(AsmRegister(0), entryIndex);                                // @0x150fa0
            results->assemblers_.push_back(std::move(asmEntry));
        } else {
            throw CustomFunctionsException(
                ErrorMessages::get(ExecTableExpectsArg));                                          // @0x151000
        }
    }

    return results;                                                                                         // @0x151060
}
std::shared_ptr<EvalResults> CustomFunctions::setPRNGSeed(                                                                                                               // @0x1513e0 (~1.6KB, ends ~0x151a68)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("setPRNGSeed", kDevHirzel);                           // @0x151435

    // @0x151443: exactly 1 argument required
    if (args.size() != 1)                                                                               // @0x151447: cmp rax, 0x38
        throw CustomFunctionsException(
            ErrorMessages::format(SetTriggerArgs));                                   // @0x15189c

    auto results = std::make_shared<EvalResults>(VarType_Void);                                           // @0x15144d

    VarType argType = args[0].varType_;                                                   // @0x151502: [rbp-0x78]

    if (argType == VarType_Var) {
        // Variable path (VarType_Var) — emit suser using the bound register.    // @0x151518
        // Binary @0x151507 loads args[0].reg_ from [rbx+0x30] into rdx; rdx is
        // preserved across the cmp/jne and passed as the AsmRegister arg to
        // suser at @0x151528. (See IF-119 — prior recon constructed an
        // AsmRegister from value_.toInt(), which is wrong for VarType_Var.)
        appendSuser(results->assemblers_, asmCommands_, args[0].reg_, detail::AddressImpl<unsigned int>(kSuserPrngSeed)); // @0x151528
    } else if (isConstOrCvar(argType)) {
        // Float/double path — range-check then load via register                                        // @0x1515a9
        double dval = args[0].value_.toDouble();                                                        // @0x1515bd

        if (dval < 0.0)                                                                                 // @0x1515ca
            throw CustomFunctionsValueException(
                ErrorMessages::format(PrngSeedPositive), 0);                            // @0x1518db
        if (dval == 0.0)                                                                                // @0x1515dc
            throw CustomFunctionsValueException(
                ErrorMessages::format(PrngSeedZero), 0);                            // @0x151908
        if (dval > 4294967295.0)                                                                        // @0x1515f1: constant at 0x956038
            throw CustomFunctionsValueException(
                ErrorMessages::format(PrngSeedMax), 0);                            // @0x151949

        int regNum = Resources::getRegisterNumber();                                                    // @0x1515ff
        AsmRegister seedReg(regNum);
        AsmRegister zero(0);
        int seedVal = args[0].value_.toInt();                                                           // @0x151625
        auto addiEntries = asmCommands_->addi(seedReg, zero, Immediate(seedVal));                       // @0x15164f
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));                       // @0x151689

        appendSuser(results->assemblers_, asmCommands_, seedReg, detail::AddressImpl<unsigned int>(kSuserPrngSeed)); // @0x15180a                                            // @0x15180f
    }

    return results;                                                                                      // @0x15178e
}
std::shared_ptr<EvalResults> CustomFunctions::getPRNGValue(                                                                                                                 // @0x151a70 (600B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("getPRNGValue", kDevHirzel);
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, std::string("getPRNGValue")));
    auto results = std::make_shared<EvalResults>();
    int regNum = Resources::getRegisterNumber();                                   // @0x151b34
    AsmRegister reg(regNum);
    auto asmEntry = asmCommands_->luser(reg, detail::AddressImpl<unsigned int>(kSuserPrngValue));  // @0x151b44
    results->assemblers_.push_back(std::move(asmEntry));
    results->setValue(VarType_Var, regNum);                                         // @0x151c1b
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setPRNGRange(                                                                                                              // @0x151ce0 (~2.4KB, ends ~0x152683)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setPRNGRange", kDevHirzel);                           // @0x151d3a

    // @0x151d3f: exactly 2 arguments required
    if (args.size() != 2)                                                                               // @0x151d48: cmp rax, 0x70
        throw CustomFunctionsException(
            ErrorMessages::format(PrngRangeArgs));                                   // @0x1524a4

    auto results = std::make_shared<EvalResults>(VarType_Void);                                           // @0x151d52

    // @0x151eeb: both args must NOT be type 2 (integer literal → accepted, float needs validation)
    // Actually: type check validates non-float integer types
    // @0x151f0b-0x151f69: range and ordering validation.
    // Phase S.2 M5: hoisted `rangeMin`/`rangeMax` here — the binary
    // re-runs `Value::toInt()` four times, which the prior recon mirrored
    // via temporary `val0`/`val1` locals followed by re-assignment to
    // `rangeMin`/`rangeMax`. Both pairs hold the same values, so we
    // collapse to a single pair.
    int rangeMin = args[0].value_.toInt();                                                              // @0x151f0b
    if (rangeMin < 0)                                                                                   // @0x151f12
        throw CustomFunctionsValueException(
            ErrorMessages::format(PrngRangeArgs), 0);                                // @0x152463
    int rangeMax = args[1].value_.toInt();                                                              // @0x151f22
    if (rangeMax < 0)                                                                                   // @0x151f29
        throw CustomFunctionsValueException(
            ErrorMessages::format(PrngRangeArgs), 0);

    if (rangeMin > 0xFFFE)                                                                              // @0x151f37: cmp eax, 0xfffe; jg
        throw CustomFunctionsValueException(
            ErrorMessages::format(PrngRangeArgs), 0);
    if (rangeMax >= 0xFFFF)                                                                             // @0x151f4a: cmp eax, 0xffff; jge
        throw CustomFunctionsValueException(
            ErrorMessages::format(PrngRangeArgs), 0);

    if (rangeMin > rangeMax)                                                                            // @0x151f69
        throw CustomFunctionsException(
            ErrorMessages::format(PrngRangeArgs));                                   // @0x152522

    int regNum = Resources::getRegisterNumber();                                                        // @0x151f8f
    AsmRegister reg(regNum);

    // Instruction 1: addi reg, R0, #rangeMin → suser reg, 0x75 (PRNG range offset)                   // @0x151fb7
    {
        AsmRegister zero(0);
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(rangeMin));                          // @0x151fe4
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));                       // @0x15200d
    }
    {
        appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserPrngRangeLow)); // @0x15210d                                            // @0x15211c
    }

    // Instruction 2: addi reg, R0, #(rangeMax - rangeMin + 1) → suser reg, 0x76 (PRNG range span)    // @0x1521e5
    {
        AsmRegister zero(0);
        int span = rangeMax - rangeMin + 1;                                                             // @0x1521ea
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(span));                              // @0x152218
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));                       // @0x152244
    }
    {
        appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserPrngRangeSpan)); // @0x152328                                            // @0x15233c
    }

    return results;                                                                                      // @0x15244d
}
std::shared_ptr<EvalResults> CustomFunctions::startQA(                                                                                                                   // @0x152690 (~6.2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("startQA", kDevQA);                                     // @0x1526f0

    // @0x1526f5: validate arg count — max depends on device type
    size_t maxArgs = (config_->deviceType == SHFQA) ? 5 : 4;                       // @0x152700
    if (args.size() > maxArgs)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsMaxArgs, std::string("startQA"),
                                  static_cast<int>(maxArgs),
                                  static_cast<int>(args.size())));

    // @0x152740: validate all provided args are int type ((varType | 2) == 6 → types 4,5,6,7)
    for (size_t i = 0; i < args.size(); ++i) {                                                             // @0x152748
        if (!isConstOrCvar(args[i].varType_))
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, std::string("startQA")));
    }

    auto results = std::make_shared<EvalResults>(VarType_Void);                                               // @0x152790

    // Defaults
    int integrationWeightsMask = 0;                                                                         // only used for deviceType == SHFQA
    int qaIntAll = 0;
    int monitorEnable = 0;
    int resultLengthShift = 0;
    int resultAddr = 0;

    // @0x152840: if deviceType == 8 (SHFQA), read QA_GEN_ALL and extract first arg as weights mask
    bool qaGenAllEnabled = false;
    if (config_->deviceType == SHFQA) {                                            // @0x152840
        auto qaGenAllErv = res->readConst("QA_GEN_ALL", EDirection::eOUT);                        // @0x152870
        int qaGenAll = qaGenAllErv.value_.toInt();                                                          // @0x152880

        if (!args.empty()) {
            integrationWeightsMask = args[0].value_.toInt();                                                // @0x1528a0
            if (~qaGenAll & integrationWeightsMask)                                                         // @0x1528b0: validate mask
                throw CustomFunctionsException(
                    ErrorMessages::format(FuncExpectsConst, std::string("startQA")));
        }
        // qaGenAllEnabled is set later, after qaIntAll is finalized                                          // bit 30 flag
    }

    // @0x152900: read QA_INT_ALL
    auto qaIntAllErv = res->readConst("QA_INT_ALL", EDirection::eOUT);                            // @0x152930
    qaIntAll = qaIntAllErv.value_.toInt();                                                                  // @0x152940

    // @0x152960: args[1] (or args[0] for non-SHFQA) — integration trigger mask
    size_t intTrigIdx = (config_->deviceType == SHFQA) ? 1 : 0;
    if (args.size() > intTrigIdx) {
        int intTrigMask = args[intTrigIdx].value_.toInt();                                                  // @0x152970
        if (~qaIntAll & intTrigMask)                                                                        // @0x152980: validate
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, std::string("startQA")));
        qaIntAll = intTrigMask;                                                                             // override with user value
    }

    // qaGenAllEnabled: set based on whether qaIntAll is nonzero (for SHFQA only)
    if (config_->deviceType == SHFQA) {
        qaGenAllEnabled = (qaIntAll != 0);                                                                  // @0x1529b0 (approx)
    }

    // @0x1529c0: optional monitor enable arg
    size_t monIdx = intTrigIdx + 1;
    if (args.size() > monIdx)
        monitorEnable = args[monIdx].value_.toInt();                                                        // @0x1529d0

    // @0x1529e0: optional result length shift arg
    size_t rlsIdx = monIdx + 1;
    if (args.size() > rlsIdx) {
        resultLengthShift = args[rlsIdx].value_.toInt();                                                    // @0x1529f0
        if (resultLengthShift >= 32)                                                                        // @0x152a00
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, std::string("startQA")));
    }

    // @0x152a40: optional result address arg
    size_t raIdx = rlsIdx + 1;
    if (args.size() > raIdx) {
        resultAddr = args[raIdx].value_.toInt();                                                            // @0x152a50
        if (resultAddr >= 256)                                                                              // @0x152a60
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, std::string("startQA")));
    }

    AsmRegister zero(0);

    if (config_->deviceType == SHFQA) {                                            // @0x152f2c: SHFQA path
        // @0x152aa0: first register — integration weights / result address composite
        {
            int regNum = Resources::getRegisterNumber();                                                    // @0x152aa4
            AsmRegister reg(regNum);
            int imm = (resultAddr << 24) | integrationWeightsMask;                                         // @0x152ac0
            auto addiEntries = asmCommands_->addi(reg, zero, Immediate(imm));                              // @0x152ae0
            for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

            appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserQAWeights)); // @0x152bc0
        }

        // @0x152c80: second register — integration trigger / monitor / gen composite
        {
            int regNum = Resources::getRegisterNumber();                                                    // @0x152c84
            AsmRegister reg(regNum);
            int imm = (resultLengthShift << 22) | qaIntAll
                      | (monitorEnable << 31)
                      | (qaGenAllEnabled ? (1 << 30) : 0);                                                 // @0x152cc0
            auto addiEntries = asmCommands_->addi(reg, zero, Immediate(imm));                              // @0x152ce0
            for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

            appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserQATrigger)); // @0x152dc0
        }
    } else if (config_->deviceType == UHFQA) {                                     // @0x1533df: UHFQA path
        // UHFQA arg layout:
        //   args[0] → qaIntAll (integration trigger mask)
        //   args[1] → monitorEnable
        //   args[2] → resultLengthShift, used as the sid (reg33) value
        //   args[3] → resultAddr, OR'd into composite bit0 and stored by strig2 (reg34)
        //
        // composite = (qaIntAll << 16) | ((qaIntAll != 0) << 4) | ((monitorEnable != 0) << 5) | resultAddr
        // sid stores resultLengthShift (args[2], 0 if not provided)
        // strig2 stores resultAddr (args[3], 0 if not provided)

        // @0x153600: sid(reg, false) — st(reg, 0x21) — stores resultLengthShift (args[2])
        {
            int regNum = Resources::getRegisterNumber();                                                    // @0x153608
            AsmRegister reg(regNum);
            int composite = (qaIntAll << 16) | ((qaIntAll != 0 ? 1 : 0) << 4)
                          | ((monitorEnable != 0 ? 1 : 0) << 5)
                          | resultAddr;                                                                     // @0x153638

            // @0x153670: load resultLengthShift into register for sid
            auto sidEntries = asmCommands_->addi(reg, zero, Immediate(resultLengthShift));                 // @0x153670
            for (auto& e : sidEntries) results->assemblers_.push_back(std::move(e));
            results->assemblers_.push_back(asmCommands_->sid(reg, false));                                  // @0x153690

            auto addi32Entries = asmCommands_->addi32(reg, zero, Immediate(composite));                    // @0x1536e0
            for (auto& e : addi32Entries) results->assemblers_.push_back(std::move(e));

            results->assemblers_.push_back(asmCommands_->strig(reg));                                      // @0x1537a5
        }

        // @0x153880: second strig — stores resultAddr (args[3], 0 if not provided)
        {
            int regNum = Resources::getRegisterNumber();                                                    // @0x153888
            AsmRegister reg(regNum);
            auto addiEntries = asmCommands_->addi(reg, zero, Immediate(resultAddr));                       // @0x1538e0
            for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

            results->assemblers_.push_back(asmCommands_->strig(reg));                                      // @0x1539c5
        }
    }

    return results;                                                                                         // @0x153580
}
std::shared_ptr<EvalResults> CustomFunctions::resetRTLoggerTimestamp(                                                                                                        // @0x153f90 (700B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("resetRTLoggerTimestamp", kDevHirzelPlusUHFQA);
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, std::string("resetRTLoggerTimestamp")));
    auto results = std::make_shared<EvalResults>(VarType_Void);
    AsmRegister reg(0);
    // Address depends on device type: 0x62 for UHFQA (type 4), else 0x6d
    unsigned int addr = (config_->deviceType == UHFQA) ? 0x62u : 0x6du;
    auto asmEntry = asmCommands_->st(reg, detail::AddressImpl<unsigned int>(addr));
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::configFreqSweep(                                                                                                            // @0x154240 (~5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("configFreqSweep", kDevSHFPlus);

    // Requires exactly 3 args (0xa8 bytes / sizeof(EvalResultValue))          // @0x1542b3
    if (args.size() != 3)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, std::string("configFreqSweep")));

    auto results = std::make_shared<EvalResults>(VarType_Void);

    // Extract args[0], args[1], args[2] — the three are treated as EvalResultValues
    auto const& arg0 = args[0];  // oscillator index
    auto const& arg1 = args[1];  // start frequency
    auto const& arg2 = args[2];  // step frequency (or end frequency)

    // Check none are VarType_Var (register-bound)                              // @0x15468c
    if (arg0.varType_ == VarType_Var ||
        arg1.varType_ == VarType_Var ||
        arg2.varType_ == VarType_Var) {
        throw CustomFunctionsException(                                          // @0x1551b9
            ErrorMessages::format(FuncExpects3Const, std::string("configFreqSweep")));
    }

    // arg0 must be non-negative and < devConst_->field_84 - 1                 // @0x1546bf
    double oscIndex = arg0.value_.toDouble();
    if (oscIndex < 0.0)
        throw CustomFunctionsException(
            ErrorMessages::format(GetUserRegArgs));           // @0x1550ed

    if (oscIndex > static_cast<double>(devConst_->numDIOBits - 1))
        throw CustomFunctionsException(
            ErrorMessages::format(GetUserRegArgs));

    // arg1: start frequency — validate absolute value <= some limit           // @0x154701
    double startFreq = arg1.value_.toDouble();
    // abs(startFreq) must not exceed a constant from rodata                    // @0x154711
    // (likely max frequency from device specs)

    // arg2: step frequency — similar validation                               // @0x15471f
    double stepFreq = arg2.value_.toDouble();

    // Convert start frequency: toFrequency(startFreq, getSampleClock())       // @0x154740
    double sampleClock = getSampleClock();
    uint64_t startFreqEncoded = NodeMap::toFrequency(startFreq, sampleClock);  // @0x154762

    // writeLS64bit(startFreqEncoded, kSuserSweepStartLo, kSuserSweepStartHi, results, res)  // @0x1547ba
    writeLS64bit(startFreqEncoded, kSuserSweepStartLo, kSuserSweepStartHi, results, res);

    // Convert step frequency: toFrequency(stepFreq, getSampleClock())         // @0x154852
    double sampleClock2 = getSampleClock();
    uint64_t stepFreqEncoded = NodeMap::toFrequency(stepFreq, sampleClock2);   // @0x154876

    // writeLS64bit(stepFreqEncoded, kSuserSweepStepLo, kSuserSweepStepHi, results, res)  // @0x1548c4
    writeLS64bit(stepFreqEncoded, kSuserSweepStepLo, kSuserSweepStepHi, results, res);

    // Get register, addi(reg, R0, arg0.toInt()), then suser(reg, 0x8c)        // @0x154961
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    AsmRegister r0(0);
    int oscIntVal = arg0.value_.toInt();
    auto addiEntries = asmCommands_->addi(reg, r0, Immediate(oscIntVal));
    for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

    auto suserEntry = asmCommands_->suser(reg, kSuserSweepOscIdx);            // @0x154aec
    results->assemblers_.push_back(std::move(suserEntry));

    // addWaitCycles(10, results, res)                                         // @0x154c0d
    addWaitCycles(10, results, res);

    // Device-type-dependent node path construction:                           // @0x154c8d
    auto devType = config_->deviceType;
    std::string nodePath;
    if (devType == AwgDeviceType::SHFLI || devType == AwgDeviceType::GHFLI || devType == AwgDeviceType::VHFLI) {
        // @0x154cb3: "oscs/" + to_string(oscIntVal) + "/freq"
        nodePath = "oscs/" + std::to_string(oscIntVal) + "/freq";              // @0x154cca
    } else if (devType == AwgDeviceType::SHFSG || devType == AwgDeviceType::SHFQC_SG) {
        // @0x154de9: "sgchannels/" + to_string(awgIndex) + "/oscs/" + to_string(oscIntVal) + "/freq"
        std::string awgIdx = std::to_string(config_->awgIndex);                // @0x154df3 [config+0x20]
        nodePath = "sgchannels/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    }
    if (!nodePath.empty()) {
        auto nodeItem = lookupNode(nodePath);                                  // @0x154d7a / @0x154f72
        addNodeAccess(nodeItem, static_cast<AccessMode>(2));                   // @0x154d8b / @0x154f83
    }

    // @0x154fbf..0x155066: EvalResultValue cleanup (SSO string frees) + epilogue → ret @0x155066

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setSweepStep(                                                                                                               // @0x155640 (~5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("setSweepStep", kDevSHFPlus);

    // Requires exactly 2 args (0x70 bytes / sizeof(EvalResultValue))               // @0x1556a2
    if (args.size() != 2)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNArgs,
                                  std::string("setSweepStep"), 2, args.size()));     // @0x156586

    auto results = std::make_shared<EvalResults>(VarType_Void);                        // @0x1556b8

    auto const& arg0 = args[0];  // oscillator index
    auto const& arg1 = args[1];  // sweep step value

    // arg0 must not be register-bound                                               // @0x155853
    if (arg0.varType_ == VarType_Var)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstVar,
                                  std::string("setSweepStep")));                     // @0x1565f9

    // arg0 must be in range [0, devConst_->numDIOBits - 1]                          // @0x155860
    double oscIndex = arg0.value_.toDouble();
    if (oscIndex < 0.0)
        throw CustomFunctionsValueException(
            ErrorMessages::format(InvalidArgValue, 0,
                                  std::string("setSweepStep")), 0);                  // @0x15652f
    if (oscIndex > static_cast<double>(devConst_->numDIOBits - 1))
        throw CustomFunctionsValueException(
            ErrorMessages::format(InvalidArgValue, 0,
                                  std::string("setSweepStep")), 0);

    // Branch on arg1.varType                                                        // @0x1558a9
    if (arg1.varType_ == VarType_Var) {
        // arg1 is register-bound: suser(arg1.register, 0x8d)                       // @0x1558b2
        auto suserEntry = asmCommands_->suser(arg1.reg_, kSuserSweepControl);
        results->assemblers_.push_back(std::move(suserEntry));
    } else if (isConstOrCvar(arg1.varType_)) {
        // arg1 is a numeric value: validate >= 0, then addi                         // @0x15594e
        double stepVal = arg1.value_.toDouble();
        if (stepVal < 0.0)
            throw CustomFunctionsValueException(
                ErrorMessages::format(InvalidArgValue, 1,
                                      std::string("setSweepStep")), 0);              // @0x156649

        int regNum1 = Resources::getRegisterNumber();                                // @0x15596b
        AsmRegister reg1(regNum1);
        AsmRegister r0(0);
        int stepInt = arg1.value_.toInt();
        auto addiEntries = asmCommands_->addi(reg1, r0, Immediate(stepInt));
        results->assemblers_.insert(results->assemblers_.end(),
                                    addiEntries.begin(), addiEntries.end());          // @0x1559c6

        // Emit suser(reg1, 0x8d) for the sweep step value                          // @0x155a??
        auto suserEntry1 = asmCommands_->suser(reg1, kSuserSweepControl);
        results->assemblers_.push_back(std::move(suserEntry1));
    }

    // Common path: addi(reg2, R0, arg0.toInt()) into assemblers_                    // @0x155ac8
    int regNum2 = Resources::getRegisterNumber();
    AsmRegister reg2(regNum2);
    AsmRegister r0(0);
    int oscIntVal = arg0.value_.toInt();                                             // @0x155b02
    auto addiEntries2 = asmCommands_->addi(reg2, r0, Immediate(oscIntVal));
    results->assemblers_.insert(results->assemblers_.end(),
                                addiEntries2.begin(), addiEntries2.end());            // @0x155b1f

    // setValue(VarType_Var, regNum2)                                                  // @0x155c41
    results->setValue(VarType_Var, static_cast<int>(reg2));

    // suser(reg2, 0x8c) -> push to assemblers_                                     // @0x155c50
    auto suserEntry2 = asmCommands_->suser(reg2, kSuserSweepOscIdx);
    results->assemblers_.push_back(std::move(suserEntry2));

    // addWaitCycles(10, results, res)                                               // @0x155d78
    addWaitCycles(10, results, res);

    // Device-type-dependent node path construction                                  // @0x155e06
    auto devTypeSS = config_->deviceType;
    std::string nodePath;
    if (devTypeSS == AwgDeviceType::SHFQA) {
        // SHFQA: "qachannels/<awgIndex>/oscs/<oscIndex>/freq"                       // @0x1560fb
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "qachannels/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (devTypeSS == AwgDeviceType::SHFSG || devTypeSS == AwgDeviceType::SHFQC_SG) {
        // SHFSG/SHFQC_SG: "sgchannels/<awgIndex>/oscs/<oscIndex>/freq"             // @0x155e2b
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "sgchannels/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (devTypeSS == AwgDeviceType::HDAWG) {
        // HDAWG: "generators/<awgIndex>/oscs/<oscIndex>/freq"                       // @0x1560fb
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "generators/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (devTypeSS == AwgDeviceType::SHFLI || devTypeSS == AwgDeviceType::GHFLI || devTypeSS == AwgDeviceType::VHFLI) {
        // SHFLI/GHFLI/VHFLI: "oscs/<oscIndex>/freq"                               // @0x156019
        nodePath = "oscs/" + std::to_string(oscIntVal) + "/freq";
    }
    if (!nodePath.empty()) {
        auto nodeItem = lookupNode(nodePath);
        addNodeAccess(nodeItem, static_cast<AccessMode>(2));
    }

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setOscFreq(                                                                                                                 // @0x156a70 (~5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("setOscFreq", kDevSHFPlus);

    // Requires exactly 2 args (0x70 bytes / sizeof(EvalResultValue))               // @0x156ad1
    if (args.size() != 2)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNArgs,
                                  std::string("setOscFreq"), 2, args.size()));       // @0x1579e4

    auto results = std::make_shared<EvalResults>(VarType_Void);                        // @0x156ae7

    auto const& arg0 = args[0];  // oscillator index
    auto const& arg1 = args[1];  // frequency

    // Neither arg may be register-bound (VarType == 2)                              // @0x156c88
    if (arg0.varType_ == VarType_Var ||
        arg1.varType_ == VarType_Var) {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstVar,
                                  std::string("setOscFreq")));                       // @0x157a54
    }

    // arg0 must be in range [0, devConst_->numDIOBits - 1]                          // @0x156c9e
    double oscIndex = arg0.value_.toDouble();
    if (oscIndex < 0.0)
        throw CustomFunctionsValueException(
            ErrorMessages::format(InvalidArgValue, 0,
                                  std::string("setOscFreq")), 0);                    // @0x15798d
    if (oscIndex > static_cast<double>(devConst_->numDIOBits - 1))
        throw CustomFunctionsValueException(
            ErrorMessages::format(InvalidArgValue, 0,
                                  std::string("setOscFreq")), 0);

    // First register: addi(reg1, R0, Immediate(0)), then suser(reg1, 0x8d)        // @0x156ce1
    int regNum1 = Resources::getRegisterNumber();
    AsmRegister reg1(regNum1);
    {
        AsmRegister r0(0);
        auto addiEntries = asmCommands_->addi(reg1, r0, Immediate(0));               // @0x156d35
        results->assemblers_.insert(results->assemblers_.end(),
                                    addiEntries.begin(), addiEntries.end());
    }

    auto suserEntry1 = asmCommands_->suser(reg1, kSuserSweepControl);               // @0x156e4c
    results->assemblers_.push_back(std::move(suserEntry1));

    // Convert frequency: toFrequency(arg1.toDouble(), getSampleClock())            // @0x156f28
    double freq = arg1.value_.toDouble();
    double sampleClock = getSampleClock();
    uint64_t freqEncoded = NodeMap::toFrequency(freq, sampleClock);                  // @0x156f4c

    // writeLS64bit(freqEncoded, kSuserSweepStartLo, kSuserSweepStartHi, results, res)  // @0x156f8e
    writeLS64bit(freqEncoded, kSuserSweepStartLo, kSuserSweepStartHi, results, res);

    // Second register: addi(reg2, R0, Immediate(arg0.toInt())), then suser(reg2, 0x8c)  // @0x15703f
    int regNum2 = Resources::getRegisterNumber();
    AsmRegister reg2(regNum2);
    {
        AsmRegister r0(0);
        int oscIntVal = arg0.value_.toInt();                                         // @0x157079
        auto addiEntries2 = asmCommands_->addi(reg2, r0, Immediate(oscIntVal));      // @0x1570ad
        results->assemblers_.insert(results->assemblers_.end(),
                                    addiEntries2.begin(), addiEntries2.end());
    }

    // suser(reg2, 0x8c) -> push to assemblers_                                     // @0x1571b9
    auto suserEntry2 = asmCommands_->suser(reg2, kSuserSweepOscIdx);
    results->assemblers_.push_back(std::move(suserEntry2));

    // addWaitCycles(10, results, res)                                               // @0x1572de
    addWaitCycles(10, results, res);

    // Device-type-dependent node path construction                                  // @0x15736c
    int oscIntVal = arg0.value_.toInt();
    auto devTypeOF = config_->deviceType;
    std::string nodePath;
    if (devTypeOF == AwgDeviceType::SHFQA) {
        // SHFQA: "qachannels/<awgIndex>/oscs/<oscIndex>/freq"                       // @0x157391
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "qachannels/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (devTypeOF == AwgDeviceType::SHFSG || devTypeOF == AwgDeviceType::SHFQC_SG) {
        // SHFSG/SHFQC_SG: "sgchannels/<awgIndex>/oscs/<oscIndex>/freq"             // @0x155e2b (same string)
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "sgchannels/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (devTypeOF == AwgDeviceType::HDAWG) {
        // HDAWG: "generators/<awgIndex>/oscs/<oscIndex>/freq"                       // @0x15765e
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "generators/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (devTypeOF == AwgDeviceType::SHFLI || devTypeOF == AwgDeviceType::GHFLI || devTypeOF == AwgDeviceType::VHFLI) {
        // SHFLI/GHFLI/VHFLI: "oscs/<oscIndex>/freq"                               // @0x15757c
        nodePath = "oscs/" + std::to_string(oscIntVal) + "/freq";
    }
    if (!nodePath.empty()) {
        auto nodeItem = lookupNode(nodePath);
        addNodeAccess(nodeItem, static_cast<AccessMode>(2));
    }

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::configureFeedbackProcessing(                                                                                                    // @0x157e60 (~5.6KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("configureFeedbackProcessing", kDevHirzel);  // @0x157ec7

    // Requires exactly 4 args (0xe0 bytes / sizeof(EvalResultValue))               // @0x157ee8
    if (args.size() != 4)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNArgs,
                                  std::string("configureFeedbackProcessing"),
                                  4, args.size()));                                  // @0x158dbd

    auto results = std::make_shared<EvalResults>(VarType_Void);                        // @0x157efd

    auto const& sourceArg = args[0];  // source (feedback source index)
    auto const& shiftArg = args[1];  // shift
    auto const& numBitsArg = args[2];  // number of bits
    auto const& thresholdArg = args[3];  // threshold

    // Args 1, 2 (from arg1.varType), and 3 must not be register-bound              // @0x1584ba
    if (shiftArg.varType_ == VarType_Var ||
        numBitsArg.varType_ == VarType_Var ||
        thresholdArg.varType_ == VarType_Var) {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstVar,
                                  std::string("configureFeedbackProcessing")));      // @0x158e29
    }

    // Build set of valid source IDs based on devConst_->execTableIndexBits                   // @0x1584da
    int srcBase = 1 << static_cast<int>(devConst_->execTableIndexBits);                         // @0x1584e9
    int src1 = srcBase + 1;
    int src2 = srcBase + 2;

    std::unordered_set<int> validSources;
    validSources.insert(src1);
    validSources.insert(src2);

    // If deviceType == 0x20, also add shift+4                                       // @0x158558
    if (static_cast<int>(config_->deviceType) == 0x20) {
        int src3 = srcBase + 4;
        validSources.insert(src3);
    }

    // Validate arg0 (source) is in the valid set                                    // @0x15857d
    int sourceVal = sourceArg.value_.toInt();
    if (validSources.find(sourceVal) == validSources.end()) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(InvalidArgValue, 0,
                                  std::string("configureFeedbackProcessing")), 0);   // @0x158c27
    }

    // Validate arg1 (shift): must be in [0, 32)                                     // @0x158697
    int shiftVal = shiftArg.value_.toInt();
    if (shiftVal < 0 || shiftVal >= 0x20) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(InvalidArgValue, 1,
                                  std::string("configureFeedbackProcessing")), 0);   // @0x158ca9
    }

    // Validate arg2 (number of bits): must be in (0, 17)                            // @0x1586c0
    int numBitsVal = numBitsArg.value_.toInt();
    if (numBitsVal <= 0 || numBitsVal >= 0x11) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(InvalidArgValue, 2,
                                  std::string("configureFeedbackProcessing")), 0);   // @0x158d05
    }

    // Validate arg3 (threshold): must be in [0, 0x1000)                             // @0x1586e1
    int thresholdVal = thresholdArg.value_.toInt();
    if (thresholdVal < 0 || thresholdVal >= 0x1000) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(InvalidArgValue, 3,
                                  std::string("configureFeedbackProcessing")), 0);   // @0x158d61
    }

    // Build map: source ID -> mode (used for the encoding)                          // @0x158706
    // numBitsVal was already read into r13d at 0x15870e
    std::unordered_map<int, int> sourceToMode;
    sourceToMode[src1]  = 0;
    sourceToMode[src2]  = 1;
    if (static_cast<int>(config_->deviceType) == 0x20) {
        sourceToMode[srcBase + 4] = 2;
    }

    // Look up the mode for the given source                                         // @0x1587a4
    int mode = sourceToMode.at(sourceVal);

    // Encode the fb instruction argument:                                           // @0x158917
    //   bits[22:21] = mode & 0x3
    //   bits[20:16] = shiftVal & 0x1f
    //   bits[15:12] = (numBitsVal - 1) & 0xf   (encoded as ((numBitsVal<<12)+0xf000)&0xffff)
    //   bits[11:0]  = thresholdVal & 0xfff
    int encoded = ((mode & 0x3) << 21) |
                  ((shiftVal & 0x1f) << 16) |
                  (static_cast<uint16_t>((numBitsVal << 12) + 0xf000)) |
                  (thresholdVal & 0xfff);

    auto fbEntry = asmCommands_->fb(encoded);                                        // @0x15896f
    results->assemblers_.push_back(std::move(fbEntry));

    return results;
}

} // namespace zhinst
