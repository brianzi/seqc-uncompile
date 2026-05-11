// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// custom_functions_dio.cpp — split from custom_functions_io.cpp
// DIO and waveform-index methods: setDIO, getDIO, getDIOTriggered, getZSyncData, getFeedback, setID, assignWaveIndex, prefetch
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

std::shared_ptr<EvalResults> CustomFunctions::setDIO(                                                                                                                       // @0x130780 (~2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // Uses external triggering mode check instead of checkFunctionSupported
    checkExternalTriggeringMode(ExternalTriggeringMode::Dio);
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, std::string("setDIO")));

    // Check device type for high-bank DIO support
    bool isShf = isShfFamily();

    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto const& arg = args[0];

    if (arg.varType_ == VarType_Var) {
        // Var: use reg_, not value_.toInt() (same pattern as setID)
        AsmRegister reg = arg.reg_;
        auto asmEntry = asmCommands_->sdio(reg, isShf);
        results->assemblers_.push_back(std::move(asmEntry));
    } else if (isConstOrCvar(arg.varType_)) {
        // Variable — construct from immediate then sdio
        int regNum = Resources::getRegisterNumber();
        AsmRegister newReg(regNum);
        AsmRegister r0(0);
        auto addiEntries = asmCommands_->addi32(newReg, r0, Immediate(arg.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto asmEntry = asmCommands_->sdio(newReg, isShf);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, std::string("setDIO")));
    }

    // @0x130b95: Add node access for DIO output
    // Binary wraps lookupNode+addNodeAccess in try-catch; if node doesn't exist
    // (e.g. SHFQA which has no DIOOUTPUT in its node map), silently skip.
    // Catch at @0x130f5e-0x130f6c: begin_catch, end_catch, jmp epilogue.
    if (!isShf) {
        try {
            auto node = lookupNode(std::string("_/dios/0/output"));           // @0x130cd7
            addNodeAccess(node, static_cast<AccessMode>(2));                   // @0x130d34
        } catch (...) {
            // Silently ignore — node may not exist for this device type
        }
    }
    // SHF path (isShfFamily): no lookupNode call — binary jumps to 0x130d56 epilogue

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getDIO(                                                                                                                       // @0x131040 (~1KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkExternalTriggeringMode(ExternalTriggeringMode::Dio);
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, std::string("getDIO")));
    bool isShf = isShfFamily();
    auto results = std::make_shared<EvalResults>();
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    auto asmEntry = asmCommands_->ldio(reg, isShf);
    results->assemblers_.push_back(std::move(asmEntry));
    results->setValue(VarType_Var, regNum);
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getDIOTriggered(                                                                                                               // @0x131410 (~700B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkExternalTriggeringMode(ExternalTriggeringMode::Dio);
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsNoArgs, std::string("getDIOTriggered")));
    auto results = std::make_shared<EvalResults>();
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    auto asmEntry = asmCommands_->ldiotrig(reg);
    results->assemblers_.push_back(std::move(asmEntry));
    results->setValue(VarType_Var, regNum);
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getZSyncData(                                                                                                                // @0x1316f0 (~3KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("getZSyncData", kDevAllButUHF);

    // externalTriggeringMode_: None → set to ZSync; must be ZSync to proceed
    checkExternalTriggeringMode(ExternalTriggeringMode::ZSync);

    // Arg count check: deviceType==4 requires exactly 1 arg; others require 1-2
    auto deviceType = config_->deviceType;
    if (static_cast<int>(deviceType) == UHFQA) {
        if (args.size() != 1)
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExactArgs2,
                    std::string("getZSyncData"), 1, args.size()));
    } else {
        if (args.size() < 1 || args.size() > 2)
            throw CustomFunctionsException(
                ErrorMessages::format(FuncArgs2or3,
                    std::string("getZSyncData"), 1, 2, args.size()));
    }

    // Extract first arg value
    auto const& arg0 = args[0];
    int argVal = arg0.value_.toInt();

    // Try matching ZSYNC_DATA_RAW
    auto rawResult = res->readConst("ZSYNC_DATA_RAW", EDirection::eOUT);
    int rawVal = rawResult.value_.toInt();

    bool matched = (argVal == rawVal);
    bool matchedProcessedA = false;
    bool matchedProcessedB = false;

    if (!matched) {
        // Check if device supports ZSYNC_DATA_PROCESSED constants
        bool supportsProcessed = (deviceType == AwgDeviceType::HDAWG || deviceType == AwgDeviceType::SHFSG ||
                                  deviceType == AwgDeviceType::SHFQC_SG || deviceType == AwgDeviceType::SHFLI ||
                                  deviceType == AwgDeviceType::GHFLI || deviceType == AwgDeviceType::VHFLI);
        if (supportsProcessed) {
            // Try ZSYNC_DATA_PROCESSED_A
            auto procAResult = res->readConst("ZSYNC_DATA_PROCESSED_A", EDirection::eOUT);
            int procAVal = procAResult.value_.toInt();
            if (argVal == procAVal) {
                matchedProcessedA = true;
            } else {
                // Try ZSYNC_DATA_PROCESSED_B
                auto procBResult = res->readConst("ZSYNC_DATA_PROCESSED_B", EDirection::eOUT);
                int procBVal = procBResult.value_.toInt();
                matchedProcessedB = (argVal == procBVal);
            }
        }
    }

    if (!matched && !matchedProcessedA && !matchedProcessedB)
        throw CustomFunctionsException(
            ErrorMessages::format(InvalidZSyncData));

    // Create results and set wait cycles register
    auto results = std::make_shared<EvalResults>(VarType_Void);
    setWaitCyclesReg(args, results, res);

    // Get a register and emit the appropriate load instruction
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);

    if (static_cast<int>(deviceType) == SHFQA) {                                  // @0x131c53
        // Special device: ld(reg, 0x6a)
        auto asmEntry = asmCommands_->ld(reg, 0x6a);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        // Re-read consts to determine which variant matched for the ld dispatch
        auto rawResult2 = res->readConst("ZSYNC_DATA_RAW", EDirection::eOUT);
        int rawVal2 = rawResult2.value_.toInt();
        if (argVal == rawVal2) {                                              // @0x131d33
            // ZSYNC_DATA_RAW → ldiotrig
            auto asmEntry = asmCommands_->ldiotrig(reg);
            results->assemblers_.push_back(std::move(asmEntry));
        } else {
            // Try ZSYNC_DATA_PROCESSED_A
            auto procAResult2 = res->readConst("ZSYNC_DATA_PROCESSED_A", EDirection::eOUT);
            int procAVal2 = procAResult2.value_.toInt();
            if (argVal == procAVal2) {                                        // @0x131e23
                auto asmEntry = asmCommands_->ld(reg, 0x6b);
                results->assemblers_.push_back(std::move(asmEntry));
            } else {
                // Must be ZSYNC_DATA_PROCESSED_B
                auto procBResult2 = res->readConst("ZSYNC_DATA_PROCESSED_B", EDirection::eOUT);
                int procBVal2 = procBResult2.value_.toInt();
                if (argVal == procBVal2) {                                    // @0x131f80
                    auto asmEntry = asmCommands_->ld(reg, 0x6c);
                    results->assemblers_.push_back(std::move(asmEntry));
                }
            }
        }
    }

    results->setValue(VarType_Var, static_cast<int>(reg));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getFeedback(                                                                                                                 // @0x132420 (~4KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("getFeedback", kDevAllButUHF);

    // externalTriggeringMode_: None → set to ZSync; must be ZSync to proceed
    checkExternalTriggeringMode(ExternalTriggeringMode::ZSync);

    // Arg count check: deviceType==4 requires exactly 1 arg; others require 1-2
    auto deviceType = config_->deviceType;
    if (static_cast<int>(deviceType) == UHFQA) {
        if (args.size() != 1)
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExactArgs2,
                    std::string("getFeedback"), 1, args.size()));
    } else {
        if (args.size() < 1 || args.size() > 2)
            throw CustomFunctionsException(
                ErrorMessages::format(FuncArgs2or3,
                    std::string("getFeedback"), 1, 2, args.size()));
    }

    // Extract first arg value
    auto const& arg0 = args[0];
    int argVal = arg0.value_.toInt();

    // Try matching ZSYNC_DATA_RAW
    bool matched = false;
    {
        auto rawResult = res->readConst("ZSYNC_DATA_RAW", EDirection::eOUT);
        int rawVal = rawResult.value_.toInt();
        matched = (argVal == rawVal);
    }

    if (!matched) {
        // Check if device supports ZSYNC_DATA_PROCESSED constants (bitmask 0x4000000040004001)
        bool supportsZSync = (deviceType == AwgDeviceType::HDAWG || deviceType == AwgDeviceType::SHFSG ||
                              deviceType == AwgDeviceType::SHFQC_SG || deviceType == AwgDeviceType::SHFLI ||
                              deviceType == AwgDeviceType::GHFLI || deviceType == AwgDeviceType::VHFLI);
        if (supportsZSync) {
            auto procAResult = res->readConst("ZSYNC_DATA_PROCESSED_A", EDirection::eOUT);
            if (argVal == procAResult.value_.toInt()) {
                matched = true;
            } else {
                auto procBResult = res->readConst("ZSYNC_DATA_PROCESSED_B", EDirection::eOUT);
                matched = (argVal == procBResult.value_.toInt());
            }
        }

        // Additional check for deviceType == SHFQC_SG: try QA_DATA constants    // @0x13282f
        if (!matched && deviceType == AwgDeviceType::SHFQC_SG) {
            auto qaRawResult = res->readConst("QA_DATA_RAW", EDirection::eOUT);
            if (argVal == qaRawResult.value_.toInt()) {
                matched = true;
            } else {
                auto qaProcResult = res->readConst("QA_DATA_PROCESSED_D", EDirection::eOUT);
                matched = (argVal == qaProcResult.value_.toInt());
            }
        }
    }

    if (!matched)
        throw CustomFunctionsException(
            ErrorMessages::format(InvalidZSyncData));

    // Create results and set wait cycles register
    auto results = std::make_shared<EvalResults>(VarType_Void);
    setWaitCyclesReg(args, results, res);

    // Get a register and emit the appropriate load instruction
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);

    if (static_cast<int>(deviceType) == SHFQA) {                                  // @0x132ad8
        auto asmEntry = asmCommands_->ld(reg, 0x6a);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        // Re-read consts to determine which variant matched for the ld dispatch
        auto rawResult2 = res->readConst("ZSYNC_DATA_RAW", EDirection::eOUT);
        if (argVal == rawResult2.value_.toInt()) {                            // @0x132bbb
            auto asmEntry = asmCommands_->ldiotrig(reg);
            results->assemblers_.push_back(std::move(asmEntry));
        } else {
            auto procAResult2 = res->readConst("ZSYNC_DATA_PROCESSED_A", EDirection::eOUT);
            if (argVal == procAResult2.value_.toInt()) {                      // @0x132cab
                auto asmEntry = asmCommands_->ld(reg, 0x6b);
                results->assemblers_.push_back(std::move(asmEntry));
            } else {
                auto procBResult2 = res->readConst("ZSYNC_DATA_PROCESSED_B", EDirection::eOUT);
                if (argVal == procBResult2.value_.toInt()) {                  // @0x132da0
                    auto asmEntry = asmCommands_->ld(reg, 0x6c);
                    results->assemblers_.push_back(std::move(asmEntry));
                } else {
                    // QA_DATA variants
                    auto qaRaw2 = res->readConst("QA_DATA_RAW", EDirection::eOUT);
                    if (argVal == qaRaw2.value_.toInt()) {                    // @0x132e91
                        auto asmEntry = asmCommands_->ld(reg, 0xc0);
                        results->assemblers_.push_back(std::move(asmEntry));
                    } else {
                        auto qaProc2 = res->readConst("QA_DATA_PROCESSED_D", EDirection::eOUT);
                        if (argVal == qaProc2.value_.toInt()) {               // @0x132fd5
                            auto asmEntry = asmCommands_->ld(reg, 0xc1);
                            results->addAssembler(asmEntry);                  // @0x133008 uses addAssembler
                        }
                    }
                }
            }
        }
    }

    results->setValue(VarType_Var, static_cast<int>(reg));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setID(                                                                                                                        // @0x1334a0 (~2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setID", kDevHirzelAll);
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, std::string("setID")));
    bool isShf = isShfFamily();
    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto const& arg = args[0];
    if (arg.varType_ == VarType_Var) {
        AsmRegister reg = arg.reg_;                                              // Var: use reg_, not value_.toInt()
        auto asmEntry = asmCommands_->sid(reg, isShf);
        results->assemblers_.push_back(std::move(asmEntry));
    } else if (isConstOrCvar(arg.varType_)) {
        int regNum = Resources::getRegisterNumber();
        AsmRegister newReg(regNum);
        auto addiEntries = asmCommands_->addi(newReg, AsmRegister(0), Immediate(arg.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto asmEntry = asmCommands_->sid(newReg, isShf);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, std::string("setID")));
    }
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::assignWaveIndex(                                                                                                            // @0x133c40 (~5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("assignWaveIndex", kDevHirzelAll);

    // Copy args and extract optional name string                              // @0x133c93
    std::vector<EvalResultValue> argsCopy(args.begin(), args.end());
    auto optName = parseOptionalString(argsCopy);

    bool hasName = optName.has_value();                                        // @0x133d39: [rbp-0x80]==1

    if (hasName) {
        // Validate name against C-like identifier regex                       // @0x133d43
        // Pattern from .rodata @0x9005ed: "[a-zA-Z_][a-zA-Z0-9_]*"
        // BSS: b84730 (regex), b84740 (guard). Flags: 0 (perl).
        // Match flags: 0x400 (match_continuous).
        static boost::regex const cLikeIdentifier("[a-zA-Z_][a-zA-Z0-9_]*");
        boost::smatch m;
        if (!boost::regex_match(*optName, m, cLikeIdentifier)) {           // @0x133dc9
            // @0x134bf9: error 0xFB — throw CustomFunctionsException
            throw CustomFunctionsException(
                ErrorMessages::format(WaveIndexExceedsTable));
        }
        // Insert name into assignedWaveNames_                                   // @0x133e2b
        assignedWaveNames_.insert(*optName);
    }

    // Build PlayArgs and parse the argument list                              // @0x133eba
    auto const& config = *config_;
    PlayArgs playArgs(config, wavetableFront_,
                      warningCallback_, std::string("assignWaveIndex"), false);
    auto parseEnd = playArgs.parse(argsCopy);

    // The element returned by parse must be a var-type (or 2 → int);          // @0x133f52
    // extract the wave index from it
    if (!isConstOrCvar(parseEnd->varType_)) {                 // (varType | 2) must == 6, i.e., 4 or 6
        // @0x134c7a → 0x134d7d: error 0x95 (149) — OnlyConstWaveIndex
        throw CustomFunctionsException(
            ErrorMessages::format(OnlyConstWaveIndex,
                                  std::string("assignWaveIndex")));
    }
    int waveIndex = parseEnd->value_.toInt();                                  // @0x133f64

    // parseEnd + 1 must equal end of argsCopy                                 // @0x133f73
    // (verifies that the index was the last argument)

    // Create results                                                          // @0x133f7d
    auto results = std::make_shared<EvalResults>(VarType_Void);

    // getMaxSampleLength                                                      // @0x133fc7
    int64_t maxSampleLen = playArgs.getMaxSampleLength();

    // Build channel args by iterating wave assignments                        // @0x133fd6
    int deviceIdx = config.deviceIndex;  // [config+0x24]
    auto const& assignments = playArgs.waveAssignments_[deviceIdx];

    std::vector<EvalResultValue> channelArgs;
    uint32_t mask = kPlayTriggerMaskFull;                                                    // r12d = 0x3fff

    for (size_t i = 0; i < assignments.size(); ++i) {                          // @0x134055
        auto const& wa = assignments[i];
        if (wa.value.varType_ != VarType_Const) {                                                    // @0x13405d
            channelArgs.push_back(wa.value);
        }
        // Get waveform name from value                                        // @0x134079
        std::string waveName = wa.value.value_.toString();  // @0x134079: [wa+0x08] = value_ (Value at EvalResultValue+0x08)
        if (waveName.empty()) continue;

        // Iterate over channel bits and clear mask bits                        // @0x1340de
        auto const& bits = wa.bits;
        if (bits.empty()) continue;

        int shift = static_cast<int>(i) * 7;  // i*7 = i*8 - i (lea + sub)
        // SIMD loop clears bits in mask based on channel assignments           // @0x134114
        for (auto it = bits.begin(); it != bits.end(); ++it) {
            int bit = *it;
            uint32_t clearBit = ~(uint32_t(1) << ((bit - 1) + shift));
            mask &= clearBit;
        }
    }

    // mergeWaveforms                                                          // @0x134224
    // Disasm trace (all params now identified):
    //   rdx (args)  = [rbp-0xb0] = channelArgs
    //   rcx (short) = movsx [rax+0x14] = config.channelsPerGroup[0]
    //   r8b  (bool) = xor r8d,r8d → false
    //   r9   (str)  = name "assignWaveIndex"
    //   stack[2nd push] (int)  = QWORD [rbp-0x238] = (int)playArgs.getMaxSampleLength()
    //       (set @0x13400a after call to PlayArgs::getMaxSampleLength@0x133fce —
    //        same pattern as the play() call site at @0x15fa75)
    //   stack[1st push] (bool) = 0 (literal `push 0x0`) — assignWaveIndex
    //       always passes false, distinct from play() which computes it
    std::shared_ptr<WaveformFront> wf;
    if (!channelArgs.empty()) {
        short channelParam = static_cast<short>(config.channelsPerGroup[0]);   // [config+0x14]
        int maxSampleLength = static_cast<int>(maxSampleLen);
        wf = mergeWaveforms(channelArgs, channelParam, false,
                           std::string("assignWaveIndex"),
                           maxSampleLength,
                           false);
    }

    // genPlayConfig                                                           // @0x134387
    if (wf) {
        wf->used = true;  // [WaveformFront+0x48] = Waveform::used  // @0x1342f1

        // Check if channelArgs has >= 2 elements, get second arg name         // @0x1342f5
        bool singleChannel = false;                                            // @0x13435e: xor ebx,ebx
        if (channelArgs.size() >= 2) {
            std::string secondName = channelArgs[1].value_.toString();
            singleChannel = secondName.empty();
        }

        auto playConfig = asmCommands_->genPlayConfig(
            wf, singleChannel, true, false, false, -1, mask, false, 0);        // @0x1343bb

        // Build the config word from PlayConfig fields and store at wf->playConfig  // @0x1343c9
        // The binary reads each PlayConfig field, shifts by the corresponding
        // static Shift value, ANDs with the Mask, then ORs all 9 fields into
        // a single uint32_t.  This is exactly what encodeCwvf() does.
        // The defaultRate arg is -1 (matching genPlayConfig's rate arg above).
        wf->playConfig = playConfig.encodeCwvf(/*defaultRate=*/-1);               // @0x1344d5

        // wavetableFront_->assignWaveIndex(wf, waveIndex)                     // @0x134504
        wavetableFront_->assignWaveIndex(wf, waveIndex);

        // If name present: wavetableFront_->updateWave(wf, name)              // @0x13453a
        if (hasName) {
            wavetableFront_->updateWave(wf, *optName);
        }

        // Set wf->minLengthSamples from devConst_->waveformMinSamples (+0x4C)           // @0x1345b4
        if (wf) {
            wf->minLengthSamples = static_cast<int>(devConst_->waveformMinSamples);  // @0x1345c4
        }
    }

    // checkOffspecWaveLength                                                   // @0x1345e7
    if (wf) {
        checkOffspecWaveLength(wf, static_cast<int>(devConst_->waveformMinSamples));  // @0x1345f8
    }

    // asmLoadPlaceholder                                                      // @0x134625
    auto asmEntry = asmCommands_->asmLoadPlaceholder();

    // Copy waveform name into node->wavesPerDev[deviceIndex]                  // @0x134635
    if (wf) {
        int devIdx = config.deviceIndex;                                       // [config+0x24]
        asmEntry.node->wavesPerDev[devIdx] = wf->name;
    }

    // Set deviceIndex on the node                                             // @0x13472b
    asmEntry.node->deviceIndex = config.deviceIndex;

    // Chain node into results->node_ tree                                     // @0x13472e
    if (results->node_) {
        results->node_->next = asmEntry.node;                                  // @0x134751
    } else {
        results->node_ = asmEntry.node;                                        // @0x134776
    }

    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::prefetch(                                                                                                                    // @0x1351d0 (300B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("prefetch", HDAWG);
    return play(args, std::move(res), SubFunc::Prefetch);                      // forwards to play() with SubFunc 0
}
std::shared_ptr<EvalResults> CustomFunctions::prefetchIndexed(                                                                                                              // @0x135290 (100B)
    std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("prefetchIndexed", kDevNone);  // mask 0 → always unsupported
    return nullptr;  // unreachable — checkFunctionSupported throws
}
// @0x139760 (4640B, ends ~0x13a8a0)

} // namespace zhinst
