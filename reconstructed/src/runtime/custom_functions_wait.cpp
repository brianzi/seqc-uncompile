// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// custom_functions_wait.cpp — split from custom_functions_io.cpp
// Wait and trigger methods: wait, waitTrigger, all waitXxx variants, resetOscPhase, setSinePhase, waitDemodSample
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

std::shared_ptr<EvalResults> CustomFunctions::wait(
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x139782: args.size() == 1 (byte size 0x38)
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, "wait"));       // @0x13a5d5

    // @0x139792..0x13982c: extract arg[0] into local EvalResultValue
    auto const& arg = args[0];

    // @0x13985d..0x13986e: allocate EvalResults(VarType_Void)
    auto results = std::make_shared<EvalResults>(VarType_Void);                        // @0x139842

    // @0x13987b: check arg type — if type == 2 (register), emit warning and return
    if (arg.varType_ == VarType_Void) {                                       // @0x13987e: cmp eax,1
        // @0x139889: warning for bool type
        auto msg = ErrorMessages::format(FuncCalledWithLogical, "wait");  // @0x13989c
        if (warningCallback_)                                                        // @0x1398ac
            warningCallback_(msg);                                                   // @0x1398bf
    }

    // @0x1398e2: check varType
    VarType varType = arg.varType_;

    if (varType == VarType_Var) {
        // @0x1398e8: arg is a register — dispatch on device type
        auto devType = static_cast<int>(devConst_->deviceType);              // @0x1398f1
        bool isSimpleDevice = false;
        {
            int idx = devType - 2;
            if (idx >= 0 && idx <= 62) {
                uint64_t mask = 0x4000000040004041ULL;
                isSimpleDevice = (mask >> idx) & 1;
            }
        }
        if (devType == AwgDeviceType::VHFLI || devType == AwgDeviceType::GHFLI)  // @0x139b62-139b72
            isSimpleDevice = true;

        if (isSimpleDevice) {
            // @0x13991a: Hirzel path — emit suser(reg, 0x69)
            appendSuser(results->assemblers_, asmCommands_, arg.reg_, detail::AddressImpl<unsigned int>(kSuserWaitCycles)); // @0x139935
        } else {
            // @0x139b78: Cervino path — readConst("AWG_WAIT_TRIGGER"), suser(arg.reg_, 0x1a), wtrig(reg, reg)
            int regNum = Resources::getRegisterNumber();                          // @0x139b78
            AsmRegister reg(regNum);                                              // @0x139b83
            AsmRegister zero(0);                                                  // @0x139b9d

            auto erv = res->readConst("AWG_WAIT_TRIGGER",
                EDirection::eOUT);                                                // @0x139bcb
            int constVal = erv.value_.toInt();                                    // @0x139bd7

            auto addiEntries = asmCommands_->addi(reg, zero, Immediate(constVal)); // @0x139c01
            for (auto& e : addiEntries)
                results->assemblers_.push_back(std::move(e));                     // @0x139c06-139c31

            // suser(arg.reg_, kSuserTriggerLoad) — store wait count to addr 0x1a
            appendSuser(results->assemblers_, asmCommands_, arg.reg_, detail::AddressImpl<unsigned int>(kSuserTriggerLoad)); // @0x13a0ae

            // wtrig(reg, reg)                                                     @0x13a182
            auto wtrigEntry = asmCommands_->wtrig(reg, reg);
            results->assemblers_.push_back(std::move(wtrigEntry));
        }
    } else if (isConstOrCvar(varType)) {
        // @0x1399bc: numeric value — convert to double, check >= 0
        double val = arg.value_.toDouble();                                          // @0x1399c8
        if (val < 0.0)                                                               // @0x1399d0
            throw CustomFunctionsValueException(                                     // @0x13a682
                errMsg[WaitPositive], 0);

        // @0x1399da: get device type
        auto devType = static_cast<int>(devConst_->deviceType);                      // @0x1399dd

        // @0x1399e2..0x139914: check device type bitmask (HDAWG/UHFQA/SHFQA/SHFQC etc.)
        // Bitmask 0x4000000040004041 checks deviceType-2 for bits 0,6,30,62
        bool isSimpleDevice = false;
        {
            int idx = devType - 2;
            if (idx >= 0 && idx <= 62) {
                uint64_t mask = 0x4000000040004041ULL;
                isSimpleDevice = (mask >> idx) & 1;
            }
        }
        if (devType == AwgDeviceType::VHFLI || devType == AwgDeviceType::GHFLI)
            isSimpleDevice = true;

        if (isSimpleDevice) {
            // @0x1399ff: simple path — value fits in immediate, toDouble >= 0
            if (val < 0.0) goto done;  // already checked above                     // @0x139a13

            // @0x139a19: getRegisterNumber, build addi
            int regNum = Resources::getRegisterNumber();                             // @0x139a19
            AsmRegister reg(regNum);                                                 // @0x139a24
            AsmRegister zero(0);                                                     // @0x139a3e
            int intVal = arg.value_.toInt();                                         // @0x139a4a
            auto addiEntries = asmCommands_->addi(reg, zero, Immediate(intVal));     // @0x139a77
            for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));// @0x139a7f..0x139ab2

            // @0x139e8c: emit suser(reg, 0x69) — wait trigger
            appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserWaitCycles)); // @0x139ea0                   // @0x139ea5..0x139ec8
        } else {
            // @0x139cc9: complex path — need clock rate scaling
            double dval = arg.value_.toDouble();                                     // @0x139cd0

            // @0x139cd9: compare with DeviceConstants triggerLatencyCycles
            double clockCycles = static_cast<double>(devConst_->triggerLatencyCycles);           // @0x139ce0
            if (clockCycles >= dval) {
                // @0x139ee5: value <= clockCycles — emit nop loop
                for (int i = 0; i < arg.value_.toInt(); ++i) {                      // @0x139f01
                    auto nopEntry = asmCommands_->nop();                             // @0x139f26
                    results->assemblers_.push_back(std::move(nopEntry));             // @0x139f2b..0x139fb7
                }
            } else {
                // @0x139cef: readConst("AWG_WAIT_TRIGGER") for scaling
                int regNum = Resources::getRegisterNumber();                         // @0x139cef
                AsmRegister reg(regNum);                                             // @0x139cfa
                AsmRegister zero2(0);                                                // @0x139d14

                auto erv = res->readConst("AWG_WAIT_TRIGGER",
                    EDirection::eOUT);                                     // @0x139d42: readConst
                int constVal = erv.value_.toInt();                                   // @0x139d51

                auto addiEntries = asmCommands_->addi(
                    reg, zero2, Immediate(constVal));                                 // @0x139d7b
                for (auto& e : addiEntries)
                    results->assemblers_.push_back(std::move(e));                    // @0x139d80..0x139dab

                // @0x13a259: second register for remaining wait count
                int regNum2 = Resources::getRegisterNumber();                        // @0x13a259
                AsmRegister reg2(regNum2);                                           // @0x13a264

                // @0x13a28d: remaining = arg - triggerLatencyCycles
                int remaining = arg.value_.toInt() - static_cast<int>(devConst_->triggerLatencyCycles); // @0x13a29a
                AsmRegister zero3(0);                                                // @0x13a281
                auto addiEntries2 = asmCommands_->addi(
                    reg2, zero3, Immediate(remaining));                              // @0x13a2c5
                for (auto& e : addiEntries2)
                    results->assemblers_.push_back(std::move(e));                    // @0x13a2ca..0x13a2fb

                // @0x13a3dc: emit suser(reg2, 0x1a) — load remaining into trigger register
                appendSuser(results->assemblers_, asmCommands_, reg2, detail::AddressImpl<unsigned int>(kSuserTriggerLoad)); // @0x13a3ec

                // @0x13a4b6: emit wtrig(reg, reg) — wait on main trigger value
                auto wtrigEntry = asmCommands_->wtrig(reg, reg);                     // @0x13a4c0
                results->assemblers_.push_back(std::move(wtrigEntry));               // @0x13a4c5..0x13a54a
            }
        }
    } else {
        // @0x13a62f: unsupported type
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, "wait"));        // @0x13a652
    }

done:
    // @0x13a5c0: return results
    return results;                                                                  // @0x13a5c3..0x13a5d4
}
std::shared_ptr<EvalResults> CustomFunctions::waitTrigger(                                                                                                                   // @0x13abf0 (1734B, ends ~0x13b2b6)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // @0x13ac0d-0x13ac36: construct SSO string "waitTrigger"
    // @0x13ac49: call checkFunctionSupported("waitTrigger", AwgDeviceType(0x5))
    checkFunctionSupported("waitTrigger", kDevCervino);

    // @0x13ac4e-0x13ac5c: args.size() check — (end - begin) must equal 0x70 = 2 * sizeof(EvalResultValue)
    if (args.size() != 2)                                                                       // @0x13ac5c: jne 0x13b309 (error)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstConst, "waitTrigger"));             // @0x13b309-0x13b354

    // @0x13ac62-0x13acf0: extract args[0] → local EvalResultValue copy (value at rbp-0x160..rbp-0x150 region)
    // @0x13ad01-0x13adaf: extract args[1] → local EvalResultValue copy (value at rbp-0x128..rbp-0x118 region)

    // @0x13adb6-0x13add4: validate both args are integer types: (varType & ~1) == 4
    if (!isConstOrCvar(args[0].varType_) ||                                       // @0x13adbf: cmp eax, 4; jne 0x13b2b7
        !isConstOrCvar(args[1].varType_))                                         // @0x13add1: cmp eax, 4; jne 0x13b2b7
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstConst, "waitTrigger"));             // @0x13b2b7-0x13b304

    // @0x13adda-0x13ae06: allocate EvalResults with VarType_Void
    auto results = std::make_shared<EvalResults>(VarType_Void);                                    // @0x13addf: new(0x98); @0x13ae06: EvalResults::EvalResults(VarType_Void)

    // @0x13ae17: allocate register for trigger value
    int regNum1 = Resources::getRegisterNumber();                                                // @0x13ae17: call 0x1e4bb0
    AsmRegister reg1(regNum1);                                                                   // @0x13ae22: AsmRegister(int)

    // @0x13ae3c-0x13ae78: addi(reg1, R0, Immediate(args[0].value_.toInt()))
    //   loads the trigger value into reg1
    AsmRegister zero(0);                                                                         // @0x13ae3c: AsmRegister(0)
    int trigValue = args[0].value_.toInt();                                                      // @0x13ae4b: call Value::toInt()
    auto addiEntries1 = asmCommands_->addi(reg1, zero, Immediate(trigValue));                    // @0x13ae78: call AsmCommands::addi @0x273d60

    // @0x13aeb3: insert addi results into results->assemblers_
    for (auto& e : addiEntries1) results->assemblers_.push_back(std::move(e));                   // @0x13ae84-0x13aeb3: vector insert

    // @0x13af7e-0x13af99: compare args[0].value_.toInt() == args[1].value_.toInt()
    int val0 = args[0].value_.toInt();                                                           // @0x13af81: call Value::toInt()
    int val1 = args[1].value_.toInt();                                                           // @0x13af92: call Value::toInt()

    if (val0 == val1) {                                                                          // @0x13af99: jne 0x13afe6
        // Same value for both args — reuse reg1 for both wtrig operands
        // @0x13afb5: asmCommands_->wtrig(reg1, reg1)
        auto asmEntry = asmCommands_->wtrig(reg1, reg1);                                        // @0x13afb5: call AsmCommands::wtrig @0x274f00
        results->assemblers_.push_back(std::move(asmEntry));                                     // @0x13afc2-0x13afe1: push_back
    } else {
        // Different values — allocate second register for trigger address
        // @0x13afe6: call Resources::getRegisterNumber()
        int regNum2 = Resources::getRegisterNumber();                                            // @0x13afe6: call 0x1e4bb0
        AsmRegister reg2(regNum2);                                                               // @0x13aff1: AsmRegister(int)

        // @0x13b017-0x13b044: addi(reg2, R0, Immediate(args[1].value_.toInt()))
        //   loads the trigger address into reg2
        AsmRegister zero2(0);                                                                    // @0x13b00f: AsmRegister(0)
        int trigAddr = args[1].value_.toInt();                                                   // @0x13b017: call Value::toInt()
        auto addiEntries2 = asmCommands_->addi(reg2, zero2, Immediate(trigAddr));                // @0x13b044: call AsmCommands::addi @0x273d60

        // @0x13b07e: insert addi results into results->assemblers_
        for (auto& e : addiEntries2) results->assemblers_.push_back(std::move(e));               // @0x13b049-0x13b07e: vector insert

        // @0x13b17e: asmCommands_->wtrig(reg1, reg2)
        auto asmEntry = asmCommands_->wtrig(reg1, reg2);                                        // @0x13b17e: call AsmCommands::wtrig @0x274f00
        results->assemblers_.push_back(std::move(asmEntry));                                     // @0x13b183-0x13b1a1: push_back
    }

    // @0x13b2a2: return results (via r13 out-param)
    return results;                                                                              // @0x13b2a2-0x13b2b6: epilogue + ret
}
std::shared_ptr<EvalResults> CustomFunctions::waitAnaTrigger(                                                                                                                  // @0x13b4b0 (~3KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x13b4d1..0x13b514: checkFunctionSupported("waitAnaTrigger", 0x5)
    checkFunctionSupported("waitAnaTrigger", kDevCervino);

    // @0x13b519..0x13b529: args.size() == 2 (byte size == 0x70)
    if (args.size() != 2)                                                                        // @0x13b529: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstConst, "waitAnaTrigger"));          // @0x13be56

    // @0x13b52f..0x13b6a2: copy both args into locals, validate both are int types
    if (!isConstOrCvar(args[0].varType_) ||                                       // @0x13b68a: and eax,0xfffffffd; cmp eax,4
        !isConstOrCvar(args[1].varType_))                                         // @0x13b69c: same check on arg[1]
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstConst, "waitAnaTrigger"));          // @0x13be01

    // @0x13b6a8..0x13b6d4: allocate EvalResults(VarType_Void)
    auto results = std::make_shared<EvalResults>(VarType_Void);                                    // @0x13b6cf

    // @0x13b6e4..0x13b79d: build two local EvalResultValues for readConst results,
    //   with AsmRegister(-1) placeholders

    // @0x13b7a2..0x13b7bd: switch on args[0].value_.toInt(): 1 → AWG_ANA_TRIGGER1, 2 → AWG_ANA_TRIGGER2
    int trigIndex = args[0].value_.toInt();                                                      // @0x13b7ae: call Value::toInt()
    EvalResultValue erv;
    if (trigIndex == 1) {                                                                        // @0x13b7bd: cmp eax,1; jne
        // @0x13b7c3..0x13b817: readConst("AWG_ANA_TRIGGER1", EDirection::eOUT)
        erv = res->readConst("AWG_ANA_TRIGGER1", EDirection::eOUT);                    // @0x13b7e9
    } else if (trigIndex == 2) {                                                                 // @0x13b7b8: cmp eax,2; je
        // @0x13b819..0x13b86d: readConst("AWG_ANA_TRIGGER2", EDirection::eOUT)
        erv = res->readConst("AWG_ANA_TRIGGER2", EDirection::eOUT);                    // @0x13b83f
    } else {
        // @0x13b8c6..0x13b8dc: unsupported trigger index — check deviceType == 2 and range 3..8
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstConst, "waitAnaTrigger"));          // @0x13bea8
    }

    // @0x13b86d..0x13b8e2: copy readConst AsmRegister into local; check args[1].toBool()

    // @0x13b929..0x13b934: getRegisterNumber → first register (for trigger value)
    int regNum1 = Resources::getRegisterNumber();                                                // @0x13b929
    AsmRegister reg1(regNum1);                                                                   // @0x13b934

    // @0x13b939..0x13b983: addi(reg1, AsmRegister(0), Immediate(erv.value_.toInt()))
    AsmRegister zero(0);                                                                         // @0x13b94e
    int trigVal = erv.value_.toInt();                                                             // @0x13b956
    auto addiEntries1 = asmCommands_->addi(reg1, zero, Immediate(trigVal));                      // @0x13b983

    // @0x13b988..0x13b9bf: insert addi results into results->assemblers_
    for (auto& e : addiEntries1) results->assemblers_.push_back(std::move(e));                   // @0x13b9bf

    // @0x13ba7e..0x13ba89: getRegisterNumber → second register (for wait condition)
    int regNum2 = Resources::getRegisterNumber();                                                // @0x13ba7e
    AsmRegister reg2(regNum2);                                                                   // @0x13ba89

    // @0x13ba8e..0x13bae3: addi(reg2, AsmRegister(0), Immediate(args[1] wait flag))
    AsmRegister zero2(0);                                                                        // @0x13baaa
    // reg2 carries args[1] (the wait condition: 0=don't wait, 1=wait), NOT the trigger address.
    // The trigger address (trigVal) was already loaded into reg1 above.
    int waitFlag = args[1].value_.toInt();
    auto addiEntries2 = asmCommands_->addi(reg2, zero2, Immediate(waitFlag));                    // @0x13bae3

    // @0x13bae8..0x13bb0f: insert addi results into results->assemblers_
    for (auto& e : addiEntries2) results->assemblers_.push_back(std::move(e));                   // @0x13bb0f

    // @0x13bbde..0x13bbfc: wtrig(reg1, reg2)
    auto wtrigEntry = asmCommands_->wtrig(reg1, reg2);                                           // @0x13bbfc
    results->assemblers_.push_back(std::move(wtrigEntry));

    // @0x13bd84: return results
    return results;                                                                              // @0x13bd84
}
std::shared_ptr<EvalResults> CustomFunctions::waitDigTrigger(                                                                                                                   // @0x13c110 (~4KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x13c132..0x13c14d: device type bitmask check for supported devices
    // Supported: deviceType in {2,3,8,16,18,32,64} (via bt 0x4000000040004041) OR 0x80 OR 0x100
    auto devType = static_cast<int>(devConst_->deviceType);                                      // @0x13c137
    auto isSupported = [](int dt) -> bool {                                                      // @0x13c137..0x13c1b9
        int idx = dt - 2;
        if (idx >= 0 && idx <= 62) {
            uint64_t mask = 0x4000000040004041ULL;
            return (mask >> idx) & 1;
        }
        return dt == AwgDeviceType::GHFLI || dt == AwgDeviceType::VHFLI;
    };

    if (isSupported(devType)) {
        // Supported-device path: 1 arg → dynamic string + asmWtrigLSPlaceholder
        if (args.size() != 1)                                                                    // @0x13c158: cmp rdx,0x38
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitDigTrigger"));      // @0x13c1dc
        EvalResultValue arg0 = args[0];                                                          // @0x13c16f..0x13c273: copy arg[0]
        if (!isConstOrCvar(arg0.varType_))                                        // @0x13c294: and eax,0xfffffffd; cmp eax,4
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitDigTrigger"));

        auto results = std::make_shared<EvalResults>(VarType_Void);                                // @0x13c2a0..0x13c2cf

        int trigIdx = arg0.value_.toInt();                                                       // @0x13c2e1

        // @0x13c304..0x13c30d: check trigIdx in {1, 2} (lea eax,[rbx-1]; cmp eax,1; ja error)
        if (trigIdx < 1 || trigIdx > 2)                                                          // @0x13c307
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitDigTrigger"));      // @0x13cbef

        // @0x13c313..0x13c3ad: build "AWG_DIG_TRIGGER" + to_string(trigIdx) + "_INDEX"
        std::string constName = "AWG_DIG_TRIGGER" + std::to_string(trigIdx) + "_INDEX";          // @0x13c31d..0x13c36f
        EvalResultValue erv = res->readConst(constName, EDirection::eOUT);              // @0x13c3a8

        // @0x13c423..0x13c434: asmWtrigLSPlaceholder(erv.value_.toInt())
        int constVal = erv.value_.toInt();                                                       // @0x13c3b9
        auto asmEntry = asmCommands_->asmWtrigLSPlaceholder(constVal);                           // @0x13c434
        results->assemblers_.push_back(std::move(asmEntry));                                     // @0x13c47c..0x13c4dc

        return results;                                                                          // @0x13c5bd
    } else {
        // Unsupported-device path: 2 args → readConst + addi + wtrig (like waitAnaTrigger)
        if (args.size() != 2)                                                                    // @0x13c1c6: cmp rax,0x70
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitDigTrigger"));

        EvalResultValue arg0 = args[0];                                                          // @0x13c5e9..0x13c68f: copy arg[0]
        EvalResultValue arg1 = args[1];                                                          // copy arg[1]
        if (!isConstOrCvar(arg0.varType_))                                        // @0x13c6a0
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitDigTrigger"));
        if (!isConstOrCvar(arg1.varType_))
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitDigTrigger"));

        auto results = std::make_shared<EvalResults>(VarType_Void);

        // @0x13c6ac..0x13c706: build two local EvalResultValues with AsmRegister(-1)
        // @0x13c712..0x13c7a3: switch on arg0.toInt(): 1 → AWG_DIG_TRIGGER1, 2 → AWG_DIG_TRIGGER2
        int trigIndex = arg0.value_.toInt();                                                     // @0x13c2e1 (second path)
        EvalResultValue erv;
        if (trigIndex == 1) {                                                                    // @0x13c717
            erv = res->readConst("AWG_DIG_TRIGGER1", EDirection::eOUT);                // @0x13c720..0x13c74c
        } else if (trigIndex == 2) {                                                             // @0x13c712
            erv = res->readConst("AWG_DIG_TRIGGER2", EDirection::eOUT);                // @0x13c777..0x13c7a3
        } else {
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitDigTrigger"));      // @0x13cc9d
        }

        // @0x13c7cc..0x13c7da: copy readConst register into local
        // Emit addi for trigger value, then check if args[1].toBool() → same reg path
        int regNum1 = Resources::getRegisterNumber();                                    // @0x13c7cc
        AsmRegister reg1(regNum1);
        AsmRegister zero1(0);
        int trigVal1 = erv.value_.toInt();
        auto addiEntries1 = asmCommands_->addi(reg1, zero1, Immediate(trigVal1));
        for (auto& e : addiEntries1) results->assemblers_.push_back(std::move(e));

        // @0x13c985: check args[1].value_.toBool() — if true, reuse same register
        bool useSameReg = arg1.value_.toBool();                                            // @0x13c985

        if (useSameReg) {
            // Same-value path: wtrig(reg1, reg1)
            auto wtrigEntry = asmCommands_->wtrig(reg1, reg1);                           // @0x13caac (same-reg path)
            results->assemblers_.push_back(std::move(wtrigEntry));
        } else {
            // Different-value path (wait=0): wtrig(reg1, R0) — R0 is always 0, no addi needed
            AsmRegister r0(0);
            auto wtrigEntry = asmCommands_->wtrig(reg1, r0);
            results->assemblers_.push_back(std::move(wtrigEntry));
        }

        return results;
    }
}
std::shared_ptr<EvalResults> CustomFunctions::waitOnGrid(                                                                                                                   // @0x13d000 (900B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("waitOnGrid", kDevLIFamily);
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, std::string("waitOnGrid")));
    auto results = std::make_shared<EvalResults>(VarType_Void);
    // @0x13d0d0: readConst("AWG_GRID_TRIGGER", eOUT) → trigger value
    EvalResultValue trigConst = res->readConst("AWG_GRID_TRIGGER",
                                                EDirection::eOUT);               // @0x13d0d0
    int trigValue = trigConst.value_.toInt();                                     // @0x13d0e0
    auto asmEntry = asmCommands_->asmWtrigLSPlaceholder(trigValue);              // @0x13d0f0
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::waitOnSync(                                                                                                                   // @0x13d3a0 (600B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("waitOnSync", kDevLIFamily);
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, std::string("waitOnSync")));
    auto results = std::make_shared<EvalResults>(VarType_Void);
    AsmRegister reg(0);
    auto asmEntry = asmCommands_->st(reg, detail::AddressImpl<unsigned int>(kSuserWaitOnSync));
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::waitDIOTrigger(  // @0x13d630 (1726B, ends ~0x13dcf0)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x13d64d: check externalTriggeringMode_ (+0x1c0)
    checkExternalTriggeringMode(ExternalTriggeringMode::Dio);

    // @0x13d66b: check args.empty() — args.end != args.begin means non-empty → throw 0x42
    if (!args.empty()) {                                                          // @0x13d672: jne error
        // @0x13db06: throw format(0x42, "waitDIOTrigger")
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, "waitDIOTrigger"));  // @0x13db26
    }

    // @0x13d678: allocate EvalResults(VarType_Void)
    auto results = std::make_shared<EvalResults>(VarType_Void);                    // @0x13d67d..0x13d6a2

    // @0x13d6ae: deviceType = config_->deviceType  ([r14] → [rax])
    int deviceType = static_cast<int>(config_->deviceType);                      // @0x13d6b1: [rax]

    // @0x13d6b3: check deviceType in supported set via bit-test
    // Supported: {2,3,8,16,18,32,64} via bitmask 0x4000000040004041 on (devType-2)
    // Also 0x80 and 0x100 via explicit compares
    bool supported = false;
    unsigned idx = static_cast<unsigned>(deviceType) - 2;
    if (idx <= 0x3e) {                                                           // @0x13d6b6: cmp ecx, 0x3e; ja
        supported = ((0x4000000040004041ULL >> idx) & 1) != 0;                   // @0x13d6c9: bt rdx, rcx
    }
    if (!supported) {
        if (deviceType == GHFLI || deviceType == VHFLI)                           // @0x13d82f..0x13d83f
            supported = true;
    }

    if (supported) {
        // === Supported device path: readConst + asmWtrigLSPlaceholder ===

        // @0x13d6d3: rsi = res.get() (from shared_ptr [r12])
        // @0x13d6d7..0x13d708: construct SSO string "AWG_MAP_TRIGGER_INDEX" (21 chars, tag=0x2a)
        //   and call res->readConst("AWG_MAP_TRIGGER_INDEX", EDirection::eOUT)
        EvalResultValue trigConst = res->readConst("AWG_MAP_TRIGGER_INDEX",
                                                    EDirection::eOUT);  // @0x13d708: EDirection::eOUT=1
        int trigValue = trigConst.value_.toInt();                                 // @0x13d714: Value::toInt()

        // @0x13d765: call asmWtrigLSPlaceholder(trigValue)
        auto asmEntry = asmCommands_->asmWtrigLSPlaceholder(trigValue);          // @0x13d773

        // @0x13d7ae..0x13d818: push_back asmEntry into results->assemblers_
        results->assemblers_.push_back(std::move(asmEntry));                     // @0x13d7c1..0x13d818
    } else {
        // === Unsupported device path: getRegisterNumber + addi + wtrig ===

        // @0x13d845: allocate a register
        int regNum = Resources::getRegisterNumber();                             // @0x13d845: call 0x1e4bb0
        AsmRegister reg(regNum);                                                 // @0x13d850: AsmRegister(int)

        // @0x13d872..0x13d8aa: readConst("AWG_MAP_TRIGGER", EDirection::eOUT)
        //   SSO string at rbp-0x60: tag=0x1e (len=15), "AWG_MAP_TRIGGER"
        EvalResultValue trigConst = res->readConst("AWG_MAP_TRIGGER",
                                                    EDirection::eOUT); // @0x13d8aa
        int trigImm = trigConst.value_.toInt();                                  // @0x13d8b6: Value::toInt()

        // @0x13d8c7..0x13d8e4: addi(reg, AsmRegister(0), Immediate(trigImm))
        AsmRegister zero(0);                                                     // @0x13d86d
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(trigImm));    // @0x13d8e4: call AsmCommands::addi

        // @0x13d8e9..0x13d914: insert addi results into results->assemblers_
        for (auto& e : addiEntries)
            results->assemblers_.push_back(std::move(e));                        // @0x13d914: vector::insert

        // @0x13daac..0x13dac1: wtrig(reg, reg) — same register for both operands
        auto wtrigEntry = asmCommands_->wtrig(reg, reg);                         // @0x13dac1: call AsmCommands::wtrig

        // @0x13dad2..0x13daeb: push_back wtrig entry
        results->assemblers_.push_back(std::move(wtrigEntry));                   // @0x13dae6
    }

    // @0x13da08: return results
    return results;                                                              // @0x13da08..0x13da1c
}
std::shared_ptr<EvalResults> CustomFunctions::waitZSyncTrigger(  // @0x13dcf0 (1890B, ends ~0x13e460)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x13dd10..0x13dd3b: checkFunctionSupported("waitZSyncTrigger", 0x1fe)
    checkFunctionSupported("waitZSyncTrigger", kDevAllButUHF);

    // @0x13dd40: check externalTriggeringMode_ (+0x1c0)
    checkExternalTriggeringMode(ExternalTriggeringMode::ZSync);

    // @0x13dd5f: check args.empty()
    if (!args.empty()) {                                                              // @0x13dd66: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, "waitZSyncTrigger")); // @0x13e278
    }

    // @0x13dd6c: allocate EvalResults(VarType_Void)
    auto results = std::make_shared<EvalResults>(VarType_Void);                        // @0x13dd71..0x13dd96

    // @0x13dda2: deviceType = config_->deviceType
    int deviceType = static_cast<int>(config_->deviceType);                          // @0x13dda5

    // @0x13dda7..0x13de41: device-type dispatch via jump table on (devType-2)
    // Supported devices for the direct path: 0x40, 0x80, 0x100,
    // plus jump-table cases for devType values 2..0x20
    // Two groups: some go to heap-string "AWG_ZSYNC_TRIGGER_INDEX" (23 chars),
    // others go to SSO "AWG_MAP_TRIGGER_INDEX" (21 chars).
    // Unsupported devices fall through to the addi+wtrig path.

    bool supported = false;
    bool useZSyncConst = false;  // true → "AWG_ZSYNC_TRIGGER_INDEX", false → "AWG_MAP_TRIGGER_INDEX"

    if (deviceType <= 0x3f) {
        unsigned idx = static_cast<unsigned>(deviceType) - 2;
        if (idx <= 0x1e) {
            // Jump table at 0x958924 dispatches per device type.
            // Supported devices: HDAWG(2), SHFQA(8), SHFSG(16), SHFQC_SG(32)
            // HDAWG uses "AWG_MAP_TRIGGER_INDEX".
            // SHFQA, SHFSG, SHFQC_SG use "AWG_ZSYNC_TRIGGER_INDEX".
            constexpr uint64_t supportedMask = 0x4000000040004041ULL;
            if ((supportedMask >> idx) & 1) {
                supported = true;
                // SHFQA(8), SHFSG(16), SHFQC_SG(32) → ZSYNC; HDAWG(2) → MAP
                if (deviceType != HDAWG) {
                    useZSyncConst = true;
                }
            }
        }
    } else {
        if (deviceType == SHFLI || deviceType == GHFLI || deviceType == VHFLI) {       // @0x13de30..0x13de41
            supported = true;
            // SHFLI, GHFLI, VHFLI use "AWG_MAP_TRIGGER_INDEX"
        }
    }

    if (supported) {
        // === Supported device path: readConst + asmWtrigLSPlaceholder ===
        const char* constName = useZSyncConst
            ? "AWG_ZSYNC_TRIGGER_INDEX"
            : "AWG_MAP_TRIGGER_INDEX";
        EvalResultValue trigConst = res->readConst(constName,
                                                    EDirection::eOUT); // @0x13de7c
        int trigValue = trigConst.value_.toInt();                                     // @0x13de88

        // @0x13dee7: call asmWtrigLSPlaceholder(trigValue)
        auto asmEntry = asmCommands_->asmWtrigLSPlaceholder(trigValue);              // @0x13dee7

        // @0x13deec..0x13e00d: store result into assemblers_ + push into results
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        // === Unsupported device path: getRegisterNumber + addi + wtrig ===

        // @0x13e022: allocate a register
        int regNum = Resources::getRegisterNumber();                                 // @0x13e022
        AsmRegister reg(regNum);                                                     // @0x13e02d

        // @0x13e032..0x13e087: readConst("AWG_MAP_TRIGGER", EDirection::eOUT)
        EvalResultValue trigConst = res->readConst("AWG_MAP_TRIGGER",
                                                    EDirection::eOUT);     // @0x13e087
        int trigImm = trigConst.value_.toInt();                                      // @0x13e093

        // @0x13e09f..0x13e0c1: addi(reg, AsmRegister(0), Immediate(trigImm))
        AsmRegister zero(0);
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(trigImm));        // @0x13e0c1

        // @0x13e0c6..0x13e0f1: insert addi results into results->assemblers_
        for (auto& e : addiEntries)
            results->assemblers_.push_back(std::move(e));

        // @0x13e1fe..0x13e213: wtrig(reg, reg)
        auto wtrigEntry = asmCommands_->wtrig(reg, reg);                             // @0x13e213
        results->assemblers_.push_back(std::move(wtrigEntry));
    }

    // @0x13e00d: return results
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::waitCntTrigger(  // @0x13e460 (1336B, ends ~0x13eba0)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x13e481..0x13e4c0: checkFunctionSupported("waitCntTrigger", 0x2)
    checkFunctionSupported("waitCntTrigger", HDAWG);

    // @0x13e4c5..0x13e4d1: args.size() == 1
    if (args.size() != 1) {                                                          // @0x13e4d1: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, "waitCntTrigger")); // @0x13e8c2
    }

    // @0x13e4d7..0x13e4dd: config_->deviceType == 2 (HDAWG)
    if (static_cast<int>(config_->deviceType) != 2) {                                // @0x13e4dd: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, "waitCntTrigger")); // @0x13e917
    }

    // @0x13e4e3..0x13e510: allocate EvalResults(VarType_Void)
    auto results = std::make_shared<EvalResults>(VarType_Void);

    // @0x13e520..0x13e5c5: copy arg[0] into local EvalResultValue
    EvalResultValue arg0 = args[0];                                                  // deep copy via Value switch

    // @0x13e5c5..0x13e5cb: check arg is int type ((varType & ~1) == 4)
    if (!isConstOrCvar(arg0.varType_)) {                               // @0x13e5cb: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstConst, "waitCntTrigger")); // @0x13e96c
    }

    // @0x13e5d1..0x13e5e0: range check: arg.toInt() must be <= 1
    int counterIdx = arg0.value_.toInt();                                            // @0x13e5d8
    if (static_cast<unsigned>(counterIdx) > 1) {                                     // @0x13e5e0: ja error
        throw CustomFunctionsException(
            ErrorMessages::format(IndexMustBe,
                                  "waitCntTrigger", "either 0 or 1"));               // @0x13e9c3
    }

    // @0x13e5e6..0x13e674: build constant name dynamically
    // to_string(counterIdx) → "0" or "1"
    // insert(0, "AWG_CNT_TRIGGER", 15) → "AWG_CNT_TRIGGER0" or "AWG_CNT_TRIGGER1"
    // append("_INDEX", 6) → "AWG_CNT_TRIGGER0_INDEX" or "AWG_CNT_TRIGGER1_INDEX"
    std::string constName = std::to_string(counterIdx);                              // @0x13e5f3
    constName.insert(0, "AWG_CNT_TRIGGER", 15);                                     // @0x13e60d
    constName.append("_INDEX", 6);                                                   // @0x13e63f

    // @0x13e661..0x13e680: readConst(constName, EDirection::eOUT)
    EvalResultValue trigConst = res->readConst(constName,
                                                EDirection::eOUT);         // @0x13e674
    int trigValue = trigConst.value_.toInt();                                         // @0x13e680

    // @0x13e6eb..0x13e6f9: asmWtrigLSPlaceholder(trigValue)
    auto asmEntry = asmCommands_->asmWtrigLSPlaceholder(trigValue);                  // @0x13e6f9
    results->assemblers_.push_back(std::move(asmEntry));

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::waitDemodOscPhase(                                                                                                               // @0x13eba0 (~3KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x13ebc4..0x13ebf1: checkFunctionSupported("waitDemodOscPhase", 0x1)
    checkFunctionSupported("waitDemodOscPhase", UHFLI);

    // @0x13ebf6..0x13ec20: check args.size() == 1 or 2
    if (args.size() == 2) {                                                                      // @0x13ec1c: cmp rax,2
        // @0x13ec26..0x13eca4: 2 args → warn via warningCallback_
        std::string warnMsg = "waitDemodOscPhase: second argument is ignored";                   // @0x13ec42..0x13ec86 (heap string, 70 bytes)
        if (warningCallback_) {                                                                  // @0x13ec91: test rdi,rdi
            warningCallback_(warnMsg);                                                           // @0x13eca4: call [rax+0x30]
        }
    } else if (args.size() != 1) {                                                               // @0x13ec16
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstConst, "waitDemodOscPhase"));       // @0x13f532
    }

    // @0x13ecc7..0x13edae: copy arg[0] into local, check int type
    EvalResultValue arg0 = args[0];                                                              // @0x13ecc7
    if (!isConstOrCvar(arg0.varType_))                                            // @0x13edae: and eax,0xfffffffd; cmp eax,4
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstConst, "waitDemodOscPhase"));       // @0x13f4e0

    // @0x13edba..0x13ee0d: build two EvalResultValue locals with AsmRegister(-1)
    // @0x13ee12..0x13ee42: make_shared<EvalResults>(VarType_Void)
    auto results = std::make_shared<EvalResults>(VarType_Void);                                    // @0x13ee37

    // @0x13ee4f..0x13ee5c: switch on arg0.value_.toInt() - 1 (8-way jump table)
    int trigIdx = arg0.value_.toInt();                                                           // @0x13ee52
    EvalResultValue erv;
    switch (trigIdx) {                                                                           // @0x13ee57: dec eax; cmp eax,7; ja error
        case 1: erv = res->readConst("AWG_DEMOD_TRIGGER1", EDirection::eOUT); break;   // @0x13ee79
        case 2: erv = res->readConst("AWG_DEMOD_TRIGGER2", EDirection::eOUT); break;   // @0x13f059
        case 3: erv = res->readConst("AWG_DEMOD_TRIGGER3", EDirection::eOUT); break;   // @0x13ef39
        case 4: erv = res->readConst("AWG_DEMOD_TRIGGER4", EDirection::eOUT); break;   // @0x13ef99
        case 5: erv = res->readConst("AWG_DEMOD_TRIGGER5", EDirection::eOUT); break;   // @0x13eed9
        case 6: erv = res->readConst("AWG_DEMOD_TRIGGER6", EDirection::eOUT); break;   // @0x13f0b9
        case 7: erv = res->readConst("AWG_DEMOD_TRIGGER7", EDirection::eOUT); break;   // @0x13f116
        case 8: erv = res->readConst("AWG_DEMOD_TRIGGER8", EDirection::eOUT); break;   // @0x13eff9
        default:
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitDemodOscPhase"));   // @0x13f477
    }

    // @0x13f1c8..0x13f1d3: getRegisterNumber → reg
    int regNum = Resources::getRegisterNumber();                                                 // @0x13f1c8
    AsmRegister reg(regNum);                                                                     // @0x13f1d3

    // @0x13f1d8..0x13f222: addi(reg, AsmRegister(0), Immediate(erv.value_.toInt()))
    AsmRegister zero(0);                                                                         // @0x13f1e9
    int trigVal = erv.value_.toInt();                                                             // @0x13f1f5
    auto addiEntries = asmCommands_->addi(reg, zero, Immediate(trigVal));                        // @0x13f222

    // @0x13f227..0x13f258: insert addi results into results->assemblers_
    for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

    // @0x13f29b..0x13f2d9: wtrig(reg, reg) — NOTE: both operands are the same register
    auto wtrigEntry = asmCommands_->wtrig(reg, reg);                                             // @0x13f2d9
    results->assemblers_.push_back(std::move(wtrigEntry));

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::waitSineOscPhase(                                                                                                                // @0x13f790 (~2.5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x13f7b4..0x13f7df: checkFunctionSupported("waitSineOscPhase", 0x1f2)
    checkFunctionSupported("waitSineOscPhase", kDevHirzel);

    // @0x13f7e4..0x13f7eb: check config_->numChannelGroups >= 2 → throw 0xde
    if (config_->numChannelGroups >= 2)                                                          // @0x13f7e7: cmp [rax+0x1c],2
        throw CustomFunctionsException(
            ErrorMessages::format(NotSupportedGrouping,
                                  std::string("waitSineOscPhase"),
                                  config_->numChannelGroups));                       // @0x13fdf3

    // @0x13f7f1..0x13f87a: build EvalResultValue locals, make_shared<EvalResults>(VarType_Void)
    auto results = std::make_shared<EvalResults>(VarType_Void);                                    // @0x13f875

    auto devType = static_cast<int>(devConst_->deviceType);                                      // @0x13f887

    bool didReadConst = false;
    bool isRegisterArg = false;
    EvalResultValue erv;

    if (devType == AwgDeviceType::HDAWG) {
        // @0x13f88f..0x13f8a5: device type 2 (HDAWG): requires 1 arg
        if (args.size() != 1)                                                                    // @0x13f8a1: cmp rax,0x38
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitSineOscPhase"));    // @0x13fe5e

        EvalResultValue arg0 = args[0];                                                          // @0x13f8af..0x13f946
        if (!isConstOrCvar(arg0.varType_))                                        // @0x13f94d
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitSineOscPhase"));    // @0x13feb1

        int oscIdx = arg0.value_.toInt();                                                        // @0x13f960
        if (oscIdx == 1) {                                                                       // @0x13f968
            erv = res->readConst("AWG_DEMOD_TRIGGER1_INDEX", EDirection::eOUT);        // @0x13f9f6..0x13fa4f
            didReadConst = true;
        } else if (oscIdx == 2) {                                                                // @0x13f971
            erv = res->readConst("AWG_DEMOD_TRIGGER2_INDEX", EDirection::eOUT);        // @0x13f977..0x13f9d0
            didReadConst = true;
        } else if (arg0.varType_ == VarType_Var) {                                      // @0x13fac2: register type
            isRegisterArg = true;
        } else {
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitSineOscPhase"));    // @0x13ff52
        }
    } else {
        // @0x13fb18..0x13fb26: device types 0x10 or 0x20 (SHF family) with 0 args
        if (devType == AwgDeviceType::SHFSG || devType == AwgDeviceType::SHFQC_SG) {                                                // @0x13fb1b,0x13fb1d
            if (args.empty()) {                                                                  // @0x13fb2b: cmp rax,[r12]
                erv = res->readConst("AWG_DEMOD_TRIGGER1_INDEX", EDirection::eOUT);    // @0x13fb35..0x13fb91
                didReadConst = true;
            }
        }
    }

    // @0x13fc15..0x13fc31: asmWtrigLSPlaceholder(erv.value_.toInt())
    int constVal = erv.value_.toInt();                                                           // @0x13fc20
    auto asmEntry = asmCommands_->asmWtrigLSPlaceholder(constVal);                               // @0x13fc31
    results->assemblers_.push_back(std::move(asmEntry));                                         // @0x13fc72..0x13fce3

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::waitTimestamp(                                                                                                                // @0x1401c0 (500B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("waitTimestamp", HDAWG);
    // Binary @0x1401c0: no args check — accepts 0 or more args, ignores them all
    auto results = std::make_shared<EvalResults>();
    AsmRegister reg(0);
    auto asmEntry = asmCommands_->st(reg, detail::AddressImpl<unsigned int>(kSuserTimestamp));  // @0x14028a
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
// @0x1403b0 (~6.5KB, ends ~0x141a00)
std::shared_ptr<EvalResults> CustomFunctions::resetOscPhase(
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x1403d0..0x14040f: checkFunctionSupported("resetOscPhase", 0x1fe)
    checkFunctionSupported("resetOscPhase", kDevAllButUHF);      // @0x14040f

    // @0x140414: get device type
    auto devType = static_cast<int>(devConst_->deviceType);                          // @0x140418

    // Device-dependent handling
    if (devType & kDevSHFPlus) {
        // @0x14052f: SHF+ path (SHFQA, SHFSG, SHFQC_SG, SHFLI, GHFLI, VHFLI)
        // Check args count
        if (args.size() >= 2) {
            // @0x141811: too many args
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsSingleArg, "resetOscPhase"));
        }
        if (args.size() == 1) {
            // @0x140560: extract arg[0] value
            auto const& arg = args[0];
            if (!isConstOrCvar(arg.varType_))                          // @0x140a2b
                throw CustomFunctionsException(
                    ErrorMessages::format(FuncExpectsSingleArg, "resetOscPhase"));

            int oscMask = arg.value_.toInt();                                        // @0x140a3b
            // @0x140a47: validate mask against devConst numDIOBits (osc bit width)
            if ((oscMask >> devConst_->numDIOBits) != 0)                             // @0x140a53
                throw CustomFunctionsException(
                    ErrorMessages::format(FuncExpectsSingleArg, "resetOscPhase"));

            // @0x140a8d: allocate register, build addi + st + suser sequence
            int regNum = Resources::getRegisterNumber();                             // @0x140a8d
            AsmRegister reg(regNum);                                                 // @0x140a98

            auto results = std::make_shared<EvalResults>();                           // @0x140aa2

            // @0x140b1d..0x140b46: addi(reg, R0, Immediate(oscMask))
            AsmRegister zero(0);                                                     // @0x140b18
            auto addiEntries = asmCommands_->addi(reg, zero, Immediate(oscMask));    // @0x140b46
            for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));// @0x140b4b..0x140b81

            // @0x140c55..0x140c77: st(reg, addr) — address depends on deviceType==8
            int stAddr = (devConst_->deviceType == SHFQA) ? 0x7a : 0x78;                // @0x140c5a: cmp [rax],8; sete
            auto stEntry = asmCommands_->st(reg, detail::AddressImpl<unsigned int>(stAddr)); // @0x140c77
            results->assemblers_.push_back(std::move(stEntry));                      // @0x140c7c..0x140d35

            // @0x140d4e: check devConst_->numAWGCores > 0
            if (devConst_->numAWGCores > 0) {                                       // @0x140d52
                // @0x140d58: additional addi+suser for sideband
                int regNum2 = Resources::getRegisterNumber();                        // @0x140d58
                AsmRegister reg2(regNum2);                                           // @0x140d66
                AsmRegister zero2(0);                                                // @0x140d87
                auto addiEntries2 = asmCommands_->addi(
                    reg2, zero2, Immediate(static_cast<int>(devConst_->numAWGCores))); // @0x140db9
                for (auto& e : addiEntries2)
                    results->assemblers_.push_back(std::move(e));                    // @0x140dbe..0x140de9

                // @0x140ebd..0x140ed0: suser(reg2, 0x69)
                appendSuser(results->assemblers_, asmCommands_, reg2, detail::AddressImpl<unsigned int>(kSuserWaitCycles)); // @0x140ed0               // @0x140ed5..0x140f5d
            }

            return results;                                                          // @0x140f9c
        } else {
            // @0x14059e: no args — use all-osc mask
            int allMask = (1 << devConst_->numDIOBits) - 1;                          // @0x1405af
            // Fall through to common path below with allMask
            int regNum = Resources::getRegisterNumber();                             // @0x140a8d (same path)
            AsmRegister reg(regNum);
            auto results = std::make_shared<EvalResults>();

            AsmRegister zero(0);
            auto addiEntries = asmCommands_->addi(reg, zero, Immediate(allMask));
            for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

            int stAddr = (devConst_->deviceType == SHFQA) ? 0x7a : 0x78;
            auto stEntry = asmCommands_->st(reg, detail::AddressImpl<unsigned int>(stAddr));
            results->assemblers_.push_back(std::move(stEntry));

            if (devConst_->numAWGCores > 0) {
                int regNum2 = Resources::getRegisterNumber();
                AsmRegister reg2(regNum2);
                AsmRegister zero2(0);
                auto addiEntries2 = asmCommands_->addi(
                    reg2, zero2, Immediate(static_cast<int>(devConst_->numAWGCores)));
                for (auto& e : addiEntries2)
                    results->assemblers_.push_back(std::move(e));
                appendSuser(results->assemblers_, asmCommands_, reg2, detail::AddressImpl<unsigned int>(kSuserWaitCycles));
            }

            return results;
        }
    } else if (devType == static_cast<int>(AwgDeviceType::UHFQA)) {
        // @0x1405ba: UHFQA-specific path — pulse-reset via st(reg,0x5f); st(R0,0x5f)
        // Binary: UHFQA takes a separate jump-table branch from the HDAWG/Hirzel path.
        // Accepts no arguments; emits:
        //   addi R_n, R0, 1
        //   st   R_n, 0x5f    (phasereset = 1)
        //   st   R0,  0x5f    (phasereset = 0, i.e. pulse-reset)
        // No node write — UHFQA node map does not contain "oscs/phasereset".
        if (args.size() != 0) {                                                       // @0x1405c3: jne error
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsSingleArg, "resetOscPhase"));
        }

        auto results = std::make_shared<EvalResults>();                               // @0x1405ce: new EvalResults

        int regNum = Resources::getRegisterNumber();                                  // @0x140628
        AsmRegister reg(regNum);                                                      // @0x140633
        AsmRegister zero(0);                                                          // @0x140647

        // addi R_n, R0, 1  (load constant 1)                                        // @0x140658
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(1));               // @0x140679
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

        // st R_n, 0x5f  (assert phasereset)                                         // @0x140858
        auto stEntry1 = asmCommands_->st(reg, detail::AddressImpl<unsigned int>(0x5f));
        results->assemblers_.push_back(std::move(stEntry1));

        // st R0, 0x5f   (deassert phasereset — pulse)                               // @0x14095c
        auto stEntry2 = asmCommands_->st(AsmRegister(0), detail::AddressImpl<unsigned int>(0x5f));
        results->assemblers_.push_back(std::move(stEntry2));

        return results;
    } else if (devType >= 2 && devType <= 0x20) {
        // @0x14044e: Hirzel device types (HDAWG only in practice; UHFQA handled above)
        if (args.size() >= 2) {
            // @0x1418e3: too many args
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsSingleArg, "resetOscPhase"));
        }

        int oscMask;
        if (args.size() == 0) {
            // @0x140775: no args — use oscMaskSetAllHirzel()
            oscMask = oscMaskSetAllHirzel();                                         // @0x140787
        } else {
            // @0x1404da: 1 arg — extract and validate
            auto const& arg = args[0];
            if (!isConstOrCvar(arg.varType_))                          // @0x14101a
                throw CustomFunctionsException(
                    ErrorMessages::format(FuncExpectsSingleArg, "resetOscPhase"));

            oscMask = arg.value_.toInt();                                            // @0x141051
            if (!oscMaskCheckHirzel(oscMask))                                        // @0x14105c
                throw CustomFunctionsException(
                    ErrorMessages::format(FuncExpectsSingleArg, "resetOscPhase"));
        }

        // @0x14109f..0x141138: build EvalResultValues and call writeToNode.
        // Binary: Hirzel path does NOT emit st(reg,0x5f) directly. Constructs
        // EvalResultValues for path="oscs/phasereset", val=oscMask, type=default,
        // and delegates to writeToNode for node address resolution and commit.
        {
            // Build path EvalResultValue: varType_=VarType_String(3), varSubType_=0, value_="oscs/phasereset"
            EvalResultValue pathErv{};                                                   // @0x14109f
            pathErv.varType_ = VarType_String;                                           // @0x1410b1
            pathErv.value_ = Value(std::string("oscs/phasereset"));                      // @0x1410bb..0x1410dd
            pathErv.varSubType_ = VarSubType(0);                                         // @0x1410e8

            // Build val EvalResultValue: varType_=VarType_Const, value_=oscMask
            EvalResultValue valErv{};
            valErv.varType_ = VarType_Const;
            valErv.value_ = Value(oscMask);
            valErv.varSubType_ = VarSubType(0);

            // Build type EvalResultValue: varType_=VarType_Const, default
            EvalResultValue typeErv{};
            typeErv.varType_ = VarType_Const;
            typeErv.value_ = Value(0);
            typeErv.varSubType_ = VarSubType(0);

            return writeToNode(pathErv, valErv, typeErv, std::move(res));
        }
    } else {
        // @0x141964: unsupported device type
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, "resetOscPhase"));
    }
}
// @0x141df0 (~4KB, ends ~0x142d50)
std::shared_ptr<EvalResults> CustomFunctions::setSinePhase(
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // @0x141e11..0x141e49: checkFunctionSupported("setSinePhase", 0x1f2)
    checkFunctionSupported("setSinePhase", kDevHirzel);       // @0x141e49

    // @0x141e4e..0x141e7a: allocate EvalResults(VarType_Void)
    auto results = std::make_shared<EvalResults>(VarType_Void);                        // @0x141e53

    // @0x141e8a: get device type
    auto devType = static_cast<int>(devConst_->deviceType);

    // --- 1. Handle deviceType == HDAWG — 2-arg path ---
    if (devType == AwgDeviceType::HDAWG) {                                                              // @0x141e8c: cmp eax,2
        // @0x141e99: need 2 args
        if (args.size() != 2)                                                        // @0x141eae: cmp rax,0x70
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsTwoConst, "setSinePhase")); // @0x142a8b

        auto const& arg0 = args[0];  // oscillator index
        auto const& arg1 = args[1];  // phase value

        // @0x142018: validate both args are integer/numeric types
        if (!isConstOrCvar(arg0.varType_) ||                           // @0x142021
            !isConstOrCvar(arg1.varType_))                             // @0x142033
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, "setSinePhase")); // @0x142942

        // @0x14203d: validate oscillator index 0 or 1
        int oscIndex = arg0.value_.toInt();                                          // @0x142043
        if (oscIndex < 0 || oscIndex >= 2)                                           // @0x142047, 0x142058
            throw CustomFunctionsException(
                ErrorMessages::format(SineGenIndex, "setSinePhase")); // @0x142995

        // @0x14205e: build addi for phase value
        int regNum = Resources::getRegisterNumber();                                 // @0x14205e
        AsmRegister reg(regNum);                                                     // @0x142069

        // @0x142075: convert phase: toDouble → float → toPhase
        double phaseVal = arg1.value_.toDouble();                                    // @0x142075
        int phase = NodeMap::toPhase(static_cast<float>(phaseVal));                  // @0x14207e

        AsmRegister zero(0);                                                         // @0x142097
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(phase));          // @0x1420c5
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));    // @0x1420ca..0x142100

        // @0x1421d6: emit suser — address depends on oscIndex
        if (oscIndex == 0) {
            // @0x142229: suser(reg, 0x70)
            appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserSinePhase0)); // @0x14223d                     // @0x142242..0x142265
        } else {
            // @0x1421e6: suser(reg, 0x71)
            appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserSinePhase1)); // @0x1421fa                     // @0x1421ff..0x142227
        }

        // @0x14232d..0x1426ef: the binary computes a node index
        // (`sineNodeBase * maxBlocks + waveformElfAlignment`, then
        // `oscIndex + idx * 2`) and a `nodeOffset = sineNodeBase`,
        // but neither value escapes the function — both are scratch
        // for an inlined helper that the recon expresses as the
        // textual `lookupNode(path)` call below. Phase S.2 M5
        // dropped the dead locals.

        // @0x1426fa..0x142750: build path "sines/" + to_string(awgIndex) + "/phaseshift"
        // Binary @0x1426bf: loads config_->awgIndex, then:
        //   @0x142709: insert "sines/" prefix, @0x142740: append "/phaseshift"
        auto path = "sines/" + std::to_string(static_cast<unsigned long>(oscIndex)) + "/phaseshift";
        auto node = lookupNode(path);                                                // @0x1427bd
        addNodeAccess(node, AccessMode::Custom);                                      // @0x1427ce
    }

    // --- 2. Handle deviceType == 0x20 or 0x10 (SHFSG, SHFQA) ---
    if (devType == AwgDeviceType::SHFQC_SG || devType == AwgDeviceType::SHFSG) {                                        // @0x14239b, 0x1423a0
        // @0x1423a6: need 1 arg (phase only, no osc index)
        if (args.size() != 1)                                                        // @0x1423bb: cmp rax,0x38
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsOneConst, "setSinePhase")); // @0x1429e8

        auto const& arg = args[0];

        // @0x14244a: validate arg type
        if (!isConstOrCvar(arg.varType_))                              // @0x14244a
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, "setSinePhase")); // @0x142a3b

        // @0x142450: build addi for phase
        int regNum = Resources::getRegisterNumber();                                 // @0x142450
        AsmRegister reg(regNum);                                                     // @0x14245e

        double phaseVal = arg.value_.toDouble();                                     // @0x142467
        unsigned int phase = static_cast<unsigned int>(
            NodeMap::toPhase(static_cast<float>(phaseVal)));                          // @0x142470

        auto asmCommands = asmCommands_;
        AsmRegister zero(0);                                                         // @0x142493
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(phase));          // @0x1424c1
        for (auto& e : addiEntries)
            results->assemblers_.push_back(std::move(e));                            // @0x1424c6..0x1424fb

        // @0x1425da: suser(reg, 0x70)
        appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserSinePhase0)); // @0x1425ed                         // @0x1425f2..0x14266d

        // @0x14281d..0x142873: build path "sgchannels/" + to_string(awgIndex) + "/sines/0/phaseshift"
        auto path = "sgchannels/" + std::to_string(config_->awgIndex) + "/sines/0/phaseshift";
        auto node = lookupNode(path);                                                // @0x1428e0
        addNodeAccess(node, AccessMode::Custom);                                      // @0x1428f1
    }

    return results;                                                                  // @0x14292d..0x142941
}
// @0x142da0 (~4KB, ends ~0x143d00)
// Very similar structure to setSinePhase, but uses suser codes 0x72/0x73 instead of 0x70/0x71
std::shared_ptr<EvalResults> CustomFunctions::incrementSinePhase(
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // @0x142dc1..0x142df5: checkFunctionSupported("incrementSinePhase", 0x1f2)
    checkFunctionSupported("incrementSinePhase", kDevHirzel); // @0x142df5

    // @0x142dfa..0x142e26: allocate EvalResults(VarType_Void)
    auto results = std::make_shared<EvalResults>(VarType_Void);                        // @0x142dff

    // @0x142e36: get device type
    auto devType = static_cast<int>(devConst_->deviceType);

    // --- 1. Handle deviceType == HDAWG — 2-arg path ---
    if (devType == AwgDeviceType::HDAWG) {                                                              // @0x142e38: cmp eax,2
        // @0x142e45: need 2 args
        if (args.size() != 2)                                                        // @0x142e5a: cmp rax,0x70
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsTwoConst, "incrementSinePhase")); // @0x143a3b

        auto const& arg0 = args[0];  // oscillator index
        auto const& arg1 = args[1];  // phase increment

        // Validate both args are integer/numeric types
        if (!isConstOrCvar(arg0.varType_) ||
            !isConstOrCvar(arg1.varType_))
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, "incrementSinePhase")); // @0x1438f2

        // Validate oscillator index 0 or 1
        int oscIndex = arg0.value_.toInt();
        if (oscIndex < 0 || oscIndex >= 2)
            throw CustomFunctionsException(
                ErrorMessages::format(SineGenIndex, "incrementSinePhase")); // @0x143945

        // Build addi for phase increment value
        int regNum = Resources::getRegisterNumber();
        AsmRegister reg(regNum);

        // Convert phase: toDouble → float → toPhase
        double phaseVal = arg1.value_.toDouble();
        int phase = NodeMap::toPhase(static_cast<float>(phaseVal));

        AsmRegister zero(0);
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(phase));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

        // Emit suser — address depends on oscIndex
        // Uses 0x72 for osc 0, 0x73 for osc 1 (vs 0x70/0x71 for setSinePhase)
        if (oscIndex == 0) {
            appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserSinePhaseInc0)); // @0x1431e8
        } else {
            appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserSinePhaseInc1)); // @0x1431a5
        }

        // Compute node index
        int nodeIdx = devConst_->sineNodeBase * devConst_->maxBlocks + devConst_->waveformElfAlignment;
        nodeIdx = oscIndex + nodeIdx * 2;
    }

    // --- 2. Handle deviceType == 0x20 or 0x10 (SHFSG, SHFQA) ---
    if (devType == AwgDeviceType::SHFQC_SG || devType == AwgDeviceType::SHFSG) {
        // Need 1 arg
        if (args.size() != 1)
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsOneConst, "incrementSinePhase"));

        auto const& arg = args[0];

        if (!isConstOrCvar(arg.varType_))
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, "incrementSinePhase"));

        int regNum = Resources::getRegisterNumber();
        AsmRegister reg(regNum);

        double phaseVal = arg.value_.toDouble();
        unsigned int phase = static_cast<unsigned int>(
            NodeMap::toPhase(static_cast<float>(phaseVal)));

        AsmRegister zero(0);
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(phase));
        for (auto& e : addiEntries)
            results->assemblers_.push_back(std::move(e));

        // @0x14359d: suser(reg, 0x72)
        appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(kSuserSinePhaseInc0)); // @0x143598

        int nodeOffset = devConst_->sineNodeBase;
    }

    // --- 3. For deviceType == HDAWG — node path construction and lookup ---
    if (devType == AwgDeviceType::HDAWG) {
        int oscIndex = args[0].value_.toInt();
        auto path = "sines/" + std::to_string(static_cast<unsigned long>(oscIndex)) + "/phaseshift";
        auto node = lookupNode(path);
        addNodeAccess(node, AccessMode::Custom);
    }

    // --- 4. For deviceType == 0x20 or 0x10 ---
    if (devType == AwgDeviceType::SHFQC_SG || devType == AwgDeviceType::SHFSG) {
        auto path = "sgchannels/" + std::to_string(config_->awgIndex) + "/sines/0/phaseshift";
        auto node = lookupNode(path);
        addNodeAccess(node, AccessMode::Custom);
    }

    return results;
}
// @0x143d50 (~5.2KB, ends ~0x145200)
// VERY COMPLEX — many device-type branches, 8 readConst calls for AWG_DEMODRATE_TRIGGER1..8
std::shared_ptr<EvalResults> CustomFunctions::waitDemodSample(
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x143d74..0x143dba: checkFunctionSupported("waitDemodSample", 0x1)
    checkFunctionSupported("waitDemodSample", UHFLI);      // @0x143dba

    // @0x143dbf: args.size() == 1 (byte size 0x38)
    if (args.size() != 1)                                                            // @0x143dcd
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, "waitDemodSample")); // @0x144f15

    // @0x143dd3..0x143e58: extract arg[0]
    auto const& arg = args[0];

    // @0x143e74: validate arg type
    if (!isConstOrCvar(arg.varType_))                                  // @0x143e7a
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, "waitDemodSample")); // @0x144f67

    // @0x143e80..0x143eba: build EvalResultValue locals for readConst results
    // Setup: two local EvalResultValues with default values
    // varType_ = 1, value_ = 0, address_ = 4, etc.

    // @0x143f1d: switch on arg value (demod trigger index 1-8)
    int trigIdx = arg.value_.toInt();

    // @0x143f20: validate range — must be 1..8 (lea eax,[rbx-1]; cmp eax,7; ja error)
    if (trigIdx < 1 || trigIdx > 8)                                                  // @0x143f20: ja 0x144715
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, "waitDemodSample"));

    // @0x143f26..0x1446d0: switch table — 8 cases, each does readConst for the corresponding trigger
    EvalResultValue erv;
    static const char* trigNames[] = {
        "AWG_DEMODRATE_TRIGGER1", "AWG_DEMODRATE_TRIGGER2",
        "AWG_DEMODRATE_TRIGGER3", "AWG_DEMODRATE_TRIGGER4",
        "AWG_DEMODRATE_TRIGGER5", "AWG_DEMODRATE_TRIGGER6",
        "AWG_DEMODRATE_TRIGGER7", "AWG_DEMODRATE_TRIGGER8"
    };

    // Each case at @0x143f3d, @0x14402d, @0x14411d, @0x14420d, @0x1442fd, @0x1443ed, @0x1444dd, @0x1445cd
    // calls: res->readConst(trigNames[trigIdx-1], EDirection(1))
    erv = res->readConst(trigNames[trigIdx - 1], EDirection::eOUT);             // @0x143f72..0x144602

    // @0x1446d0..0x144715: extract readConst result value

    // @0x1447fd: build assembly — allocate registers and emit addi + wtrig
    auto results = std::make_shared<EvalResults>(VarType_Void);

    // @0x1447fd: register for trigger constant
    int regNum1 = Resources::getRegisterNumber();                                    // @0x1447fd (approx)
    AsmRegister reg1(regNum1);                                                       // @0x144810
    AsmRegister zero1(0);                                                            // @0x144830

    int trigConst = erv.value_.toInt();                                              // @0x14484d (approx)
    auto addiEntries1 = asmCommands_->addi(reg1, zero1, Immediate(trigConst));       // @0x144869
    for (auto& e : addiEntries1) results->assemblers_.push_back(std::move(e));

    // @0x14497f: second register for trigger address
    int regNum2 = Resources::getRegisterNumber();                                    // @0x14497f
    AsmRegister reg2(regNum2);                                                       // @0x1449a9
    AsmRegister zero2(0);                                                            // @0x1449c2 (approx)

    // @0x1449de: addi for second register (same trigger constant)
    auto addiEntries2 = asmCommands_->addi(reg2, zero2, Immediate(trigConst));       // @0x1449de
    for (auto& e : addiEntries2) results->assemblers_.push_back(std::move(e));

    // @0x144aef: third register for combined value
    int regNum3 = Resources::getRegisterNumber();                                    // @0x144aef
    AsmRegister reg3(regNum3);                                                       // @0x144b19
    AsmRegister zero3(0);                                                            // @0x144b36 (approx)

    // @0x144b52: addi for third register (zero value)
    auto addiEntries3 = asmCommands_->addi(reg3, zero3, Immediate(0));               // @0x144b52
    for (auto& e : addiEntries3) results->assemblers_.push_back(std::move(e));

    // @0x144c7b: wtrig(reg1, reg2) — wait for demod trigger
    auto wtrigEntry = asmCommands_->wtrig(reg1, reg2);                               // @0x144c7b
    results->assemblers_.push_back(std::move(wtrigEntry));

    // @0x144d77: wtrig(reg1, reg3) — second wait
    auto wtrigEntry2 = asmCommands_->wtrig(reg1, reg3);                              // @0x144d77
    results->assemblers_.push_back(std::move(wtrigEntry2));

    // @0x144e00..0x145200: compiler-generated exception cleanup / landing pads only
    // (Assembler dtor, SSO string cleanup, __cxa_throw for the two error paths above).
    // No additional logic beyond what is already reconstructed.

    return results;                                                                  // @0x144efc
}

} // namespace zhinst
