// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CustomFunctions — I/O and configuration methods: setDIO, getDIO,
// getDIOTriggered, getZSyncData, getFeedback, setID, assignWaveIndex,
// prefetch, prefetchIndexed, wait, all wait/trigger variants, oscillator,
// QA, PRNG, sweeper, feedback configuration.
//
// Split from custom_functions.cpp during Phase 22b.
// ============================================================================

#include <boost/format.hpp>
#include <boost/regex.hpp>

#include "zhinst/custom_functions.hpp"
#include "zhinst/asm_commands.hpp"
#include "zhinst/asm_list.hpp"
#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/eval_results.hpp"
#include "zhinst/eval_result_value.hpp"
#include "zhinst/resources.hpp"
#include "zhinst/types.hpp"
#include "zhinst/wavetable_front.hpp"
#include "zhinst/waveform_front.hpp"
#include "zhinst/waveform_generator.hpp"

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


// ---- Remaining stubs (bodies not yet reconstructed) -----------------------
// Each stub notes its binary address and size where known.

std::shared_ptr<EvalResults> CustomFunctions::setDIO(                                                                                                                       // @0x130780 (~2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // Uses external triggering mode check instead of checkFunctionSupported
    checkExternalTriggeringMode(ExternalTriggeringMode::Dio);
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, std::string("setDIO")));

    // Check device type for high-bank DIO support
    bool supported = isShfFamily();

    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto const& arg = args[0];

    if (static_cast<int>(arg.varType_) == 2) {
        // Var: use reg_, not value_.toInt() (same pattern as setID)
        AsmRegister reg = arg.reg_;
        auto asmEntry = asmCommands_->sdio(reg, supported);
        results->assemblers_.push_back(std::move(asmEntry));
    } else if (isConstOrCvar(arg.varType_)) {
        // Variable — construct from immediate then sdio
        int regNum = Resources::getRegisterNumber();
        AsmRegister newReg(regNum);
        AsmRegister r0(0);
        auto addiEntries = asmCommands_->addi32(newReg, r0, Immediate(arg.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto asmEntry = asmCommands_->sdio(newReg, supported);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, std::string("setDIO")));
    }

    // @0x130b95: Add node access for DIO output
    // Binary wraps lookupNode+addNodeAccess in try-catch; if node doesn't exist
    // (e.g. SHFQA which has no DIOOUTPUT in its node map), silently skip.
    // Catch at @0x130f5e-0x130f6c: begin_catch, end_catch, jmp epilogue.
    if (!supported) {
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
    bool supported = isShfFamily();
    auto results = std::make_shared<EvalResults>();
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    auto asmEntry = asmCommands_->ldio(reg, supported);
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
        int dt = static_cast<int>(deviceType);
        bool supportsProcessed = (dt == 2 || dt == 16 || dt == 32 || dt == 64 ||
                                  dt == AwgDeviceType::GHFLI || dt == AwgDeviceType::VHFLI);
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
        int dt = static_cast<int>(deviceType);
        bool supportsZSync = (dt == 2 || dt == 16 || dt == 32 || dt == 64 ||
                              dt == AwgDeviceType::GHFLI || dt == AwgDeviceType::VHFLI);
        if (supportsZSync) {
            auto procAResult = res->readConst("ZSYNC_DATA_PROCESSED_A", EDirection::eOUT);
            if (argVal == procAResult.value_.toInt()) {
                matched = true;
            } else {
                auto procBResult = res->readConst("ZSYNC_DATA_PROCESSED_B", EDirection::eOUT);
                matched = (argVal == procBResult.value_.toInt());
            }
        }

        // Additional check for deviceType == 0x20: try QA_DATA constants    // @0x13282f
        if (!matched && dt == 0x20) {
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
    bool supported = isShfFamily();
    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto const& arg = args[0];
    if (static_cast<int>(arg.varType_) == 2) {
        AsmRegister reg = arg.reg_;                                              // Var: use reg_, not value_.toInt()
        auto asmEntry = asmCommands_->sid(reg, supported);
        results->assemblers_.push_back(std::move(asmEntry));
    } else if (isConstOrCvar(arg.varType_)) {
        int regNum = Resources::getRegisterNumber();
        AsmRegister newReg(regNum);
        auto addiEntries = asmCommands_->addi(newReg, AsmRegister(0), Immediate(arg.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto asmEntry = asmCommands_->sid(newReg, supported);
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
    if ((static_cast<int>(parseEnd->varType_) | 0x2) != 0x6) {                 // (varType | 2) must == 6, i.e., 4 or 6
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
    int channelIndex = config.deviceIndex;  // [config+0x24]
    auto const& assignments = playArgs.waveAssignments_[channelIndex];

    std::vector<EvalResultValue> channelArgs;
    uint32_t mask = 0x3fff;                                                    // r12d = 0x3fff

    for (size_t i = 0; i < assignments.size(); ++i) {                          // @0x134055
        auto const& wa = assignments[i];
        if (wa.value.varType_ != 4) {                                                    // @0x13405d
            channelArgs.push_back(wa.value);
        }
        // Get waveform name from value                                        // @0x134079
        std::string waveName = wa.value.value_.toString();  // @0x134079: [wa+0x08] = value_ (Value at EvalResultValue+0x08)
        if (waveName.empty()) continue;

        // Iterate over channel bits and clear mask bits                        // @0x1340de
        auto const& bits = wa.bits;
        if (bits.empty()) continue;

        int shift = static_cast<int>(i) * 7;  // lea eax, [r14*8]; sub eax, r14d → i*7
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

        // Build the config word from PlayConfig fields and store at wf->playWord  // @0x1343c9
        // The binary reads each PlayConfig field, shifts by the corresponding
        // static Shift value, ANDs with the Mask, then ORs all 9 fields into
        // a single uint32_t.  This is exactly what encodeCwvf() does.
        // The defaultRate arg is -1 (matching genPlayConfig's rate arg above).
        wf->playWord = playConfig.encodeCwvf(/*defaultRate=*/-1);               // @0x1344d5

        // wavetableFront_->assignWaveIndex(wf, waveIndex)                     // @0x134504
        wavetableFront_->assignWaveIndex(wf, waveIndex);

        // If name present: wavetableFront_->updateWave(wf, name)              // @0x13453a
        if (hasName) {
            wavetableFront_->updateWave(wf, *optName);
        }

        // Set wf->seqRegWidth from devConst_->waveformMinSamples (+0x4C)           // @0x1345b4
        if (wf) {
            wf->seqRegWidth = static_cast<int>(devConst_->waveformMinSamples);  // @0x1345c4
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
    checkFunctionSupported("prefetch", static_cast<AwgDeviceType>(HDAWG));
    return play(args, std::move(res), SubFunc::Prefetch);                      // forwards to play() with SubFunc 0
}
std::shared_ptr<EvalResults> CustomFunctions::prefetchIndexed(                                                                                                              // @0x135290 (100B)
    std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("prefetchIndexed", kDevNone);  // mask 0 → always unsupported
    return nullptr;  // unreachable — checkFunctionSupported throws
}
// @0x139760 (4640B, ends ~0x13a8a0)
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
    if (static_cast<int>(arg.varType_) == 1) {                                       // @0x13987e: cmp eax,1
        // @0x139889: warning for bool type
        auto msg = ErrorMessages::format(FuncCalledWithLogical, "wait");  // @0x13989c
        if (warningCallback_)                                                        // @0x1398ac
            warningCallback_(msg);                                                   // @0x1398bf
    }

    // @0x1398e2: check varType
    auto varType = static_cast<int>(arg.varType_);

    if (varType == 2) {
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
    } else if ((varType & ~1) == 4) {
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
    if ((static_cast<int>(args[0].varType_) & ~1) != 4 ||                                       // @0x13adbf: cmp eax, 4; jne 0x13b2b7
        (static_cast<int>(args[1].varType_) & ~1) != 4)                                         // @0x13add1: cmp eax, 4; jne 0x13b2b7
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
    if ((static_cast<int>(args[0].varType_) & ~1) != 4 ||                                       // @0x13b68a: and eax,0xfffffffd; cmp eax,4
        (static_cast<int>(args[1].varType_) & ~1) != 4)                                         // @0x13b69c: same check on arg[1]
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

    // @0x13ba7e..0x13ba89: getRegisterNumber → second register (for trigger address)
    int regNum2 = Resources::getRegisterNumber();                                                // @0x13ba7e
    AsmRegister reg2(regNum2);                                                                   // @0x13ba89

    // @0x13ba8e..0x13bae3: addi(reg2, AsmRegister(0), Immediate(erv value))
    AsmRegister zero2(0);                                                                        // @0x13baaa
    int trigVal2 = erv.value_.toInt();                                                           // @0x13bab6
    auto addiEntries2 = asmCommands_->addi(reg2, zero2, Immediate(trigVal2));                    // @0x13bae3

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
        return dt == 0x80 || dt == 0x100;
    };

    if (isSupported(devType)) {
        // Supported-device path: 1 arg → dynamic string + asmWtrigLSPlaceholder
        if (args.size() != 1)                                                                    // @0x13c158: cmp rdx,0x38
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitDigTrigger"));      // @0x13c1dc
        EvalResultValue arg0 = args[0];                                                          // @0x13c16f..0x13c273: copy arg[0]
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                        // @0x13c294: and eax,0xfffffffd; cmp eax,4
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
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                        // @0x13c6a0
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitDigTrigger"));
        if ((static_cast<int>(arg1.varType_) & ~1) != 4)
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
        bool arg1Bool = arg1.value_.toBool();                                            // @0x13c985

        if (arg1Bool) {
            // Same-value path: wtrig(reg1, reg1)
            auto wtrigEntry = asmCommands_->wtrig(reg1, reg1);                           // @0x13caac (same-reg path)
            results->assemblers_.push_back(std::move(wtrigEntry));
        } else {
            // Different-value path: allocate reg2, addi, wtrig(reg1, reg2)
            int regNum2 = Resources::getRegisterNumber();                                // @0x13c9be
            AsmRegister reg2(regNum2);
            AsmRegister zero2(0);
            int trigVal2 = erv.value_.toInt();
            auto addiEntries2 = asmCommands_->addi(reg2, zero2, Immediate(trigVal2));
            for (auto& e : addiEntries2) results->assemblers_.push_back(std::move(e));

            auto wtrigEntry = asmCommands_->wtrig(reg1, reg2);
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
    unsigned ecx = static_cast<unsigned>(deviceType) - 2;
    if (ecx <= 0x3e) {                                                           // @0x13d6b6: cmp ecx, 0x3e; ja
        supported = ((0x4000000040004041ULL >> ecx) & 1) != 0;                   // @0x13d6c9: bt rdx, rcx
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
    checkFunctionSupported("waitCntTrigger", static_cast<AwgDeviceType>(HDAWG));

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
    if ((static_cast<int>(arg0.varType_) & ~1) != 4) {                               // @0x13e5cb: jne error
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
    checkFunctionSupported("waitDemodOscPhase", static_cast<AwgDeviceType>(UHFLI));

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
    if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                            // @0x13edae: and eax,0xfffffffd; cmp eax,4
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
            ErrorMessages::get(NotSupportedGrouping));                               // @0x13fdf3

    // @0x13f7f1..0x13f87a: build EvalResultValue locals, make_shared<EvalResults>(VarType_Void)
    auto results = std::make_shared<EvalResults>(VarType_Void);                                    // @0x13f875

    auto devType = static_cast<int>(devConst_->deviceType);                                      // @0x13f887

    bool didReadConst = false;
    bool isRegisterArg = false;
    EvalResultValue erv;

    if (devType == 2) {
        // @0x13f88f..0x13f8a5: device type 2 (HDAWG): requires 1 arg
        if (args.size() != 1)                                                                    // @0x13f8a1: cmp rax,0x38
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitSineOscPhase"));    // @0x13fe5e

        EvalResultValue arg0 = args[0];                                                          // @0x13f8af..0x13f946
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                        // @0x13f94d
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConstConst, "waitSineOscPhase"));    // @0x13feb1

        int oscIdx = arg0.value_.toInt();                                                        // @0x13f960
        if (oscIdx == 1) {                                                                       // @0x13f968
            erv = res->readConst("AWG_DEMOD_TRIGGER1_INDEX", EDirection::eOUT);        // @0x13f9f6..0x13fa4f
            didReadConst = true;
        } else if (oscIdx == 2) {                                                                // @0x13f971
            erv = res->readConst("AWG_DEMOD_TRIGGER2_INDEX", EDirection::eOUT);        // @0x13f977..0x13f9d0
            didReadConst = true;
        } else if (static_cast<int>(arg0.varType_) == 2) {                                      // @0x13fac2: register type
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
    checkFunctionSupported("waitTimestamp", static_cast<AwgDeviceType>(HDAWG));
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
            if ((static_cast<int>(arg.varType_) & ~1) != 4)                          // @0x140a2b
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
    } else if (devType >= 2 && devType <= 0x20) {
        // @0x14044e: Hirzel device types (HDAWG, UHFQA, UHFLI, etc.)
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
            if ((static_cast<int>(arg.varType_) & ~1) != 4)                          // @0x14101a
                throw CustomFunctionsException(
                    ErrorMessages::format(FuncExpectsSingleArg, "resetOscPhase"));

            oscMask = arg.value_.toInt();                                            // @0x141051
            if (!oscMaskCheckHirzel(oscMask))                                        // @0x14105c
                throw CustomFunctionsException(
                    ErrorMessages::format(FuncExpectsSingleArg, "resetOscPhase"));
        }

        // @0x14109f..0x141138: build EvalResultValues and call writeToNode.
        // GDB-verified (2026-04-29): the Hirzel path does NOT emit st(reg,0x5f)
        // directly. Instead it constructs EvalResultValues for path="oscs/phasereset",
        // val=oscMask, type=default, and delegates to writeToNode which handles
        // the node address resolution and commit protocol.
        {
            // Build path EvalResultValue: varType_=VarType_String, value_="oscs/phasereset"
            // GDB-verified: path.varType_=3 (String), path.varSubType_=0
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

    // Phase 1: handle deviceType == 2 (HDAWG) — 2-arg path
    if (devType == 2) {                                                              // @0x141e8c: cmp eax,2
        // @0x141e99: need 2 args
        if (args.size() != 2)                                                        // @0x141eae: cmp rax,0x70
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsTwoConst, "setSinePhase")); // @0x142a8b

        auto const& arg0 = args[0];  // oscillator index
        auto const& arg1 = args[1];  // phase value

        // @0x142018: validate both args are integer/numeric types
        if ((static_cast<int>(arg0.varType_) & ~1) != 4 ||                           // @0x142021
            (static_cast<int>(arg1.varType_) & ~1) != 4)                             // @0x142033
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

        // @0x14232d..0x142369: compute node index = sineNodeBase * maxBlocks + waveformElfAlignment
        int nodeIdx = devConst_->sineNodeBase * devConst_->maxBlocks + devConst_->waveformElfAlignment; // @0x14232d

        // @0x142369..0x1426ef: add oscIndex*2 to nodeIdx, then lookup/access nodes
        nodeIdx = oscIndex + nodeIdx * 2;                                            // @0x142369: lea r12d,[r12+rbx*2]

        // @0x1426bf: node lookup and access
        int nodeOffset = devConst_->sineNodeBase;                                        // @0x1426c2

        // @0x1426fa..0x142750: build path "sines/" + to_string(awgIndex) + "/phaseshift"
        // Binary @0x1426bf: loads config_->awgIndex, then:
        //   @0x142709: insert "sines/" prefix, @0x142740: append "/phaseshift"
        auto path = "sines/" + std::to_string(static_cast<unsigned long>(oscIndex)) + "/phaseshift";
        auto node = lookupNode(path);                                                // @0x1427bd
        addNodeAccess(node, AccessMode::Custom);                                      // @0x1427ce
    }

    // Phase 2: handle deviceType == 0x20 or 0x10 (SHFSG, SHFQA)
    if (devType == AwgDeviceType::SHFQC_SG || devType == AwgDeviceType::SHFSG) {                                        // @0x14239b, 0x1423a0
        // @0x1423a6: need 1 arg (phase only, no osc index)
        if (args.size() != 1)                                                        // @0x1423bb: cmp rax,0x38
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsOneConst, "setSinePhase")); // @0x1429e8

        auto const& arg = args[0];

        // @0x14244a: validate arg type
        if ((static_cast<int>(arg.varType_) & ~1) != 4)                              // @0x14244a
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst, "setSinePhase")); // @0x142a3b

        // @0x142450: build addi for phase
        int regNum = Resources::getRegisterNumber();                                 // @0x142450
        AsmRegister reg(regNum);                                                     // @0x14245e

        double phaseVal = arg.value_.toDouble();                                     // @0x142467
        unsigned int phase = static_cast<unsigned int>(
            NodeMap::toPhase(static_cast<float>(phaseVal)));                          // @0x142470

        auto resultPtr = results.get();
        auto rbx = asmCommands_;
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

    // Phase 1: handle deviceType == 2 (HDAWG) — 2-arg path
    if (devType == 2) {                                                              // @0x142e38: cmp eax,2
        // @0x142e45: need 2 args
        if (args.size() != 2)                                                        // @0x142e5a: cmp rax,0x70
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsTwoConst, "incrementSinePhase")); // @0x143a3b

        auto const& arg0 = args[0];  // oscillator index
        auto const& arg1 = args[1];  // phase increment

        // Validate both args are integer/numeric types
        if ((static_cast<int>(arg0.varType_) & ~1) != 4 ||
            (static_cast<int>(arg1.varType_) & ~1) != 4)
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

    // Phase 2: handle deviceType == 0x20 or 0x10 (SHFSG, SHFQA)
    if (devType == AwgDeviceType::SHFQC_SG || devType == AwgDeviceType::SHFSG) {
        // Need 1 arg
        if (args.size() != 1)
            throw CustomFunctionsException(
                ErrorMessages::format(ExpectsOneConst, "incrementSinePhase"));

        auto const& arg = args[0];

        if ((static_cast<int>(arg.varType_) & ~1) != 4)
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

    // Phase 3: for deviceType == 2 — node path construction and lookup
    if (devType == 2) {
        auto path = "sines/" + std::to_string(static_cast<unsigned long>(config_->awgIndex)) + "/phaseshift";
        auto node = lookupNode(path);
        addNodeAccess(node, AccessMode::Custom);
    }

    // Phase 4: for deviceType == 0x20 or 0x10
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
    checkFunctionSupported("waitDemodSample", static_cast<AwgDeviceType>(UHFLI));      // @0x143dba

    // @0x143dbf: args.size() == 1 (byte size 0x38)
    if (args.size() != 1)                                                            // @0x143dcd
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsSingleArg, "waitDemodSample")); // @0x144f15

    // @0x143dd3..0x143e58: extract arg[0]
    auto const& arg = args[0];

    // @0x143e74: validate arg type
    if ((static_cast<int>(arg.varType_) & ~1) != 4)                                  // @0x143e7a
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
std::shared_ptr<EvalResults> CustomFunctions::setTrigger(                                                                                                                   // @0x1454c0 (1552B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setTrigger", kDevPreSHFLI);
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(SetTriggerArgs, std::string("setTrigger")));
    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto const& arg = args[0];
    if (static_cast<int>(arg.varType_) == 2) {
        AsmRegister reg(arg.value_.toInt());
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
            ErrorMessages::format(SetTriggerArgs, std::string("setTrigger")));
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
    if ((static_cast<int>(arg.varType_) & ~1) != 4)
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
            ErrorMessages::format(SetInternalTriggerArgs, std::string("setInternalTrigger")));
    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto const& arg = args[0];
    if (static_cast<int>(arg.varType_) == 2) {
        AsmRegister reg(arg.value_.toInt());
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
            ErrorMessages::format(SetInternalTriggerArgs, std::string("setInternalTrigger")));
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
    if ((static_cast<int>(arg.varType_) & ~1) != 4)                                                // @0x14685a: and eax,0xfffffffd; cmp eax,0x4
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
        if (config_->deviceType != 2)                                                              // @0x1469e5: cmp [rcx],0x2
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
    // NOTE: no checkFunctionSupported at top — validates arg first, then deviceType
    if (args.size() != 1)                                                                          // @0x147442: cmp rax,0x38
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, std::string("getDigTrigger")));
    auto const& arg = args[0];
    if ((static_cast<int>(arg.varType_) & ~1) != 4)                                                // @0x14750e: and eax,0xfffffffd; cmp eax,0x4
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
        if (config_->deviceType != 2)
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
            ErrorMessages::format(SetIntArgs, std::string("setInt")));
    auto const& arg0 = args[0];
    auto const& arg1 = args[1];
    // Validate: arg0 must be string (varType_==3)                                                 // @0x1483bc: cmp [rbp-0x98],0x3
    if (static_cast<int>(arg0.varType_) != 3)
        throw CustomFunctionsException(
            ErrorMessages::format(SetIntArgs, std::string("setInt")));
    // Validate: arg1 must be numeric (bitmask 0x54: bits 2,4,6 = types 2,4,6)                    // @0x1483d5..1483dd: bt 0x54,ecx
    int arg1Type = static_cast<int>(arg1.varType_);
    if (arg1Type > 6 || !((0x54 >> arg1Type) & 1))
        throw CustomFunctionsException(
            ErrorMessages::format(SetIntVarConstSecond, std::string("setInt")));
    // Call writeToNode(arg0, arg1, defaultEvalResultValue, res)                                   // @0x1486f2: call writeToNode
    EvalResultValue emptyErv{};
    emptyErv.varType_ = VarType_String;
    emptyErv.value_ = Value(1.0);
    emptyErv.varSubType_ = VarSubType(2);
    return writeToNode(arg0, arg1, emptyErv, std::move(res));    // @0x1486f2
}
std::shared_ptr<EvalResults> CustomFunctions::setDouble(                                                                                                             // @0x148ac0 (~3.3KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("setDouble", static_cast<AwgDeviceType>(UHFLI | HDAWG | UHFQA));
    // Accept 2 or 3 args: (size & ~1) == 2                                                       // @0x148b36..148b3a: and rcx,0xfffffffffffffffe; cmp rcx,0x2
    size_t sz = args.size();
    if ((sz & ~static_cast<size_t>(1)) != 2)
        throw CustomFunctionsException(
            ErrorMessages::format(SetDoubleArgs, std::string("setDouble")));
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
    if (static_cast<int>(arg0.varType_) != 3)
        throw CustomFunctionsException(
            ErrorMessages::format(SetDoubleArgs, std::string("setDouble")));
    // Validate: arg1 must be numeric (bitmask 0x54)                                               // @0x148e43..148e51: bt 0x54,eax
    int arg1Type = static_cast<int>(arg1.varType_);
    if (arg1Type > 6 || !((0x54 >> arg1Type) & 1))
        throw CustomFunctionsException(
            ErrorMessages::format(SetDoubleVarConstSecond, std::string("setDouble")));
    // Validate: arg2.varType_ must be int type ((varType & ~1) == 4)                              // @0x148e5a..148e63: and eax,0xfffffffd; cmp eax,0x4
    if ((static_cast<int>(arg2.varType_) & ~1) != 4)
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
    if (static_cast<int>(firstArg.varType_) != 3)
        throw CustomFunctionsException(
            ErrorMessages::format(GenerateExpectsString));
    // If single arg and first arg has VarSubType==2 (marker), return early                        // @0x149a4b: cmp rax,0x1; jbe
    if (args.size() <= 1) {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncNoArgsGiven,
                firstArg.value_.toString()));
    }
    // Check if second arg has VarSubType==2 → early return with setValue                          // @0x149bb1: cmp [rbx-0x28],0x2 (varType_); 149bbb: cmp [r12+0x4],0x2 (varSubType_)
    if (static_cast<int>(args[1].varType_) == 2 && static_cast<int>(args[1].varSubType_) == 2) {  // @0x149bc1: je 149d57
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
        if (static_cast<int>(erv.varSubType_) == 2) {                                             // @0x149bad: cmp [rbx-0x28],0x2
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
            ErrorMessages::format(SetUserRegArgs, std::string("setUserReg")));
    auto const& arg0 = args[0];
    auto const& arg1 = args[1];
    // Validate: arg0 must be int type ((varType & ~1) == 4)                                       // @0x14a5f2..14a5f5: and eax,0xfffffffd; cmp eax,0x4
    if ((static_cast<int>(arg0.varType_) & ~1) != 4)
        throw CustomFunctionsException(
            ErrorMessages::format(SetUserRegArgs, std::string("setUserReg")));
    // Range-check arg0 against devConst_->memoryDepth                                             // @0x14a60a..14a624
    int arg0Val = arg0.value_.toInt();
    if (static_cast<int64_t>(arg0Val) >= static_cast<int64_t>(devConst_->memoryDepth) || arg0Val < 0) {
        // Check if varSubType_ == 2 (allows bypass)                                               // @0x14a62d: cmp [rbp-0x11c],0x2
        if (static_cast<int>(arg0.varSubType_) != 2)
            throw CustomFunctionsException(
                ErrorMessages::format(SetUserRegArgs, std::string("setUserReg")));
    }
    // Create results with VarType_Void                                                              // @0x14a633..14a65e
    auto results = std::make_shared<EvalResults>(VarType_Void);
    int regNum = Resources::getRegisterNumber();                                                   // @0x14a66e: call getRegisterNumber
    AsmRegister reg(regNum);
    // Branch on arg1 type                                                                         // @0x14a67e: cmp eax,0x2
    if (static_cast<int>(arg1.varType_) == 2) {
        // arg1 is a register: suser(arg1.reg_, arg0.toInt())                                            // @0x14a6b3: call suser
        appendSuser(results->assemblers_, asmCommands_, arg1.reg_, detail::AddressImpl<unsigned int>(arg0.value_.toInt()));
    } else if (isConstOrCvar(arg1.varType_)) {                                     // @0x14a71e: and eax,0xfffffffd; cmp eax,0x4
        // arg1 is int: addi(reg, R0, arg1.toInt()) then suser(reg, arg0.toInt())                  // @0x14a774: call addi; @0x14a8d4: call suser
        auto addiEntries = asmCommands_->addi(reg, AsmRegister(0), Immediate(arg1.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        appendSuser(results->assemblers_, asmCommands_, reg, detail::AddressImpl<unsigned int>(arg0.value_.toInt()));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::format(SetUserRegArgs, std::string("setUserReg")));
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
    if ((static_cast<int>(arg0.varType_) & ~1) != 4) {                               // @0x14b5ef: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, "getUserReg"));  // @0x14bb21
    }

    // @0x14b5f5..0x14b617: range check: 0 <= arg.toInt() < devConst_->memoryDepth (+0x30)
    int userRegIdx = arg0.value_.toInt();                                            // @0x14b5fc
    if (static_cast<int64_t>(userRegIdx) >= static_cast<int64_t>(devConst_->memoryDepth) // @0x14b607: cmp [rcx+0x30], rax
        || userRegIdx < 0) {                                                         // @0x14b615: test eax,eax; jns ok
        if (static_cast<int>(arg0.varType_) != 2) {                                  // @0x14b619: cmp [rbp-0x7c], 2; jne throw
            throw CustomFunctionsValueException(
                ErrorMessages::get(GetUserRegRange), 1);            // @0x14bb6f
        }
        // varType==2 means the value is runtime-determined; allow through
    }

    // @0x14b623: getRegisterNumber
    int regNum = Resources::getRegisterNumber();                                     // @0x14b623
    AsmRegister reg(regNum);                                                         // @0x14b62e

    // @0x14b633..0x14b652: luser(reg, userRegIdx)
    int userRegInt = arg0.value_.toInt();                                             // @0x14b63e
    auto luserEntry = asmCommands_->luser(reg, detail::AddressImpl<unsigned int>(userRegInt)); // @0x14b652
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
    if ((static_cast<int>(arg0.varType_) & ~1) != 4) {                               // @0x14bda9: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, "getSweeperLength")); // @0x14c1ad
    }

    // @0x14bdaf..0x14bdc1: range check: arg.toInt() must be <= 1 (or varType==2 fallback)
    int sweepIdx = arg0.value_.toInt();                                              // @0x14bdb9
    if (sweepIdx != 1) {                                                             // @0x14bdc1: cmp eax,1; je ok
        if (static_cast<int>(arg0.varType_) != 2) {                                  // @0x14bdc3: cmp varType, 2; jne throw
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
    checkFunctionSupported("setPrecompClear", static_cast<AwgDeviceType>(HDAWG));
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(SetPrecompOneConst, std::string("setPrecompClear")));
    if ((static_cast<int>(args[0].varType_) & ~1) != 4)
        throw CustomFunctionsException(
            ErrorMessages::format(SetPrecompConst, std::string("setPrecompClear")));
    auto results = std::make_shared<EvalResults>(VarType_Void);
    int val = args[0].value_.toInt();
    unsigned int flag = (val != 0) ? 1u : 0u;
    auto asmEntry = asmCommands_->asmSetPrecompFlags(flag);
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
    if (static_cast<int>(arg0.varType_) == 1) {                                                  // @0x14cf35: cmp eax,1
        std::string warnMsg = ErrorMessages::format(FuncCalledWithLogical, "at");     // @0x14cf41..0x14cf4d
        if (warningCallback_) {                                                                  // @0x14cf59
            warningCallback_(warnMsg);                                                               // @0x14cf6c: call [rax+0x30]
        }
    }

    // @0x14cf8f..0x14cff6: make_shared<EvalResults>() — default ctor, NOT VarType_Void
    auto results = std::make_shared<EvalResults>();                                               // @0x14cf94

    // @0x14cffa..0x14d007: check arg0.varType_ == 2 (register type)
    if (static_cast<int>(arg0.varType_) == 2) {                                                  // @0x14d000: cmp eax,2
        // @0x14d00d..0x14d024: suser(arg0.asmRegister_, 0x1d) — arg already in register
        appendSuser(results->assemblers_, asmCommands_, arg0.reg_, detail::AddressImpl<unsigned int>(kSuserRTLoggerData)); // @0x14d024                                   // @0x14d029..0x14d090
    } else {
        // @0x14d095..0x14d0fb: int type path — getRegisterNumber, addi, then suser
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                        // @0x14d095: and eax,0xfffffffd; cmp eax,4
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
            ErrorMessages::format(LockArgs, std::string("lock")));
    if (static_cast<int>(args[0].varType_) != 5)
        throw CustomFunctionsException(
            ErrorMessages::format(LockOnlyWave, std::string("lock")));
    std::string name = args[0].value_.toString();
    std::optional<std::string> optName(name);
    auto wf = wavetableFront_->getWaveformByName(optName);
    if (!wf)
        throw CustomFunctionsValueException(
            ErrorMessages::format(WaveformNotExist, name), 0);
    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto asmEntry = asmCommands_->asmLockPlaceholder(wf, config_->deviceIndex);
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::unlock(                                                                                                                       // @0x14e180 (1295B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("unlock", kDevCervino);
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(UnlockArgs, std::string("unlock")));
    if (static_cast<int>(args[0].varType_) != 5)
        throw CustomFunctionsException(
            ErrorMessages::format(UnlockOnlyWave, std::string("unlock")));
    std::string name = args[0].value_.toString();
    std::optional<std::string> optName(name);
    auto wf = wavetableFront_->getWaveformByName(optName);
    if (!wf)
        throw CustomFunctionsValueException(
            ErrorMessages::format(WaveformNotExist, name), 0);
    auto results = std::make_shared<EvalResults>(VarType_Void);
    auto asmEntry = asmCommands_->asmUnlockPlaceholder(wf, config_->deviceIndex);
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getCnt(  // @0x14e8d0 (1258B, ends ~0x14edba)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // @0x14e8ed..0x14e91d: checkFunctionSupported("getCnt", 0x2)
    checkFunctionSupported("getCnt", static_cast<AwgDeviceType>(HDAWG));

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
    if ((static_cast<int>(arg0.varType_) & ~1) != 4) {                               // @0x14ea35: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, "getCnt"));      // @0x14ec6f
    }

    // @0x14ea3b..0x14ea4e: range check: counterIdx < numCounters
    int counterIdx = arg0.value_.toInt();                                            // @0x14ea42
    if (counterIdx >= devConst_->numCounters) {                                      // @0x14ea4e: jl ok
        if (static_cast<int>(arg0.varType_) != 2) {                                  // @0x14ea50: cmp varType, 2; jne throw
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
    checkFunctionSupported("waitQAResultTrigger", static_cast<AwgDeviceType>(UHFQA));                    // @0x14ee19: call checkFunctionSupported

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
    checkFunctionSupported("getQAResult", static_cast<AwgDeviceType>(UHFQA));
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
    checkFunctionSupported("startQAResult", static_cast<AwgDeviceType>(UHFQA));                               // @0x14f67a

    // @0x14f67f: max 2 args (throws 0x45 if >= 3)
    if (args.size() >= 3)                                                                                   // @0x14f688
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsMaxArgs, std::string("startQAResult")));

    // @0x14f6a2: readConst("QA_INT_ALL", EDirection(1)) — default integration mask
    auto qaIntAllErv = res->readConst("QA_INT_ALL", EDirection::eOUT);                            // @0x14f6d4
    int qaIntAll = qaIntAllErv.value_.toInt();                                                              // @0x14f6e0

    int resultAddr = 0;                                                                                     // @0x14f700: [rbp-0x100] = 0

    // @0x14f70c: if args not empty, first arg overrides qaIntAll
    if (!args.empty()) {
        auto const& arg0 = args[0];
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                                   // @0x14f71e: type check
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst));
        qaIntAll = arg0.value_.toInt();                                                                     // @0x14f72a

        // @0x14f736: if 2nd arg exists
        if (args.size() >= 2) {
            auto const& arg1 = args[1];
            if ((static_cast<int>(arg1.varType_) & ~1) != 4)                                               // @0x14f748
                throw CustomFunctionsException(
                    ErrorMessages::format(FuncExpectsConst));
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
    checkFunctionSupported("startQAMonitor", static_cast<AwgDeviceType>(UHFQA));                              // @0x150110

    // @0x150115: max 1 arg (throws 0x45 if >= 2)
    if (args.size() >= 2)                                                                                   // @0x15011e
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsMaxArgs, std::string("startQAMonitor")));

    int monitorVal = 0;                                                                                     // @0x150140

    // @0x15014c: if 1 arg, extract value
    if (!args.empty()) {
        auto const& arg0 = args[0];
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                                   // @0x15015e
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst));
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
            ErrorMessages::format(FuncMinArgs, std::string("executeTableEntry")));

    auto const& arg0 = args[0];                                                                             // @0x1509a0

    auto results = std::make_shared<EvalResults>(VarType_Void);                                               // @0x1509b0

    // @0x150a60: handle wait cycles from remaining args
    setWaitCyclesReg(args, results, res);                                                                   // @0x150a68

    int argType = static_cast<int>(arg0.varType_);                                                          // @0x150a70

    // Binary dispatch (0x150b35..0x151060):
    //   1. varType==4 → try const-match (ZSYNC_DATA_RAW etc.)
    //      On match → wvft with shifted constant.
    //      On no match AND deviceType==SHFQC_SG → try QA_DATA constants.
    //      On no match → fall through to step 2 (re-dispatch).
    //   2. varType==2 → register path
    //   3. (varType & ~2)==4 → numeric path (direct table entry index)
    //   4. else → throw

    bool constMatched = false;
    if (argType == 4) {
        // Integer constant path                                                                             // @0x150a80
        int tableIndex = arg0.value_.toInt();                                                               // @0x150a8c

        // Try matching known ZSYNC/QA data constants
        auto zsyncRaw = res->readConst("ZSYNC_DATA_RAW", EDirection::eOUT);                       // @0x150ac0
        if (tableIndex == zsyncRaw.value_.toInt()) {
            // shift = 1                                                                                     // @0x150ad0
            auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                1 << devConst_->numOutputPorts);                                                             // @0x150ae0: 1 << numOutputPorts
            results->assemblers_.push_back(std::move(asmEntry));
            constMatched = true;
        } else {
            auto zsyncProcA = res->readConst("ZSYNC_DATA_PROCESSED_A", EDirection::eOUT);         // @0x150b60
            if (tableIndex == zsyncProcA.value_.toInt()) {
                // shift = 9                                                                                 // @0x150b70
                auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                    9 << devConst_->numOutputPorts);                                                      // @0x150b80
                results->assemblers_.push_back(std::move(asmEntry));
                constMatched = true;
            } else {
                auto zsyncProcB = res->readConst("ZSYNC_DATA_PROCESSED_B", EDirection::eOUT);     // @0x150c00
                if (tableIndex == zsyncProcB.value_.toInt()) {
                    // shift = 0xd                                                                           // @0x150c10
                    auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                        0xd << devConst_->numOutputPorts);                                                 // @0x150c20
                    results->assemblers_.push_back(std::move(asmEntry));
                    constMatched = true;
                } else if (config_->deviceType == static_cast<AwgDeviceType>(SHFQC_SG)) {
                    // SHFQC_SG-specific constants                                                           // @0x150ca0
                    auto qaRaw = res->readConst("QA_DATA_RAW", EDirection::eOUT);                 // @0x150cd0
                    if (tableIndex == qaRaw.value_.toInt()) {
                        auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                            0xe << devConst_->numOutputPorts);                                             // @0x150cf0
                        results->assemblers_.push_back(std::move(asmEntry));
                        constMatched = true;
                    } else {
                        auto qaProcD = res->readConst("QA_DATA_PROCESSED_D", EDirection::eOUT);   // @0x150d60
                        if (tableIndex == qaProcD.value_.toInt()) {
                            auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                                0x10 << devConst_->numOutputPorts);                                          // @0x150d80
                            results->assemblers_.push_back(std::move(asmEntry));
                            constMatched = true;
                        }
                        // If no QA match either: fall through to re-dispatch below            // @0x150eb3→0x150feb
                    }
                }
                // If no const match and not SHFQC_SG: fall through to re-dispatch             // @0x150eb3→0x150feb
            }
        }
    }

    if (!constMatched) {
        // Re-dispatch: register or numeric path                                                             // @0x150feb
        if (argType == 2) {
            // Register path                                                                                 // @0x150e80
            auto asmEntry = asmCommands_->wvft(arg0.reg_,
                1 << (devConst_->numOutputPorts + 1));                                                      // @0x150dfc: 1 << (numOutputPorts + 1)
            results->assemblers_.push_back(std::move(asmEntry));
        } else if ((argType & ~2) == 4) {
            // Numeric (int/uint) path — direct table entry index                                            // @0x150f00
            int entryIndex = arg0.value_.toInt();                                                           // @0x150f10
            if (entryIndex < 0)
                throw CustomFunctionsValueException(
                    ErrorMessages::get(ExecTableInvalidIndex), 0);                                   // @0x150f40
            if ((entryIndex >> devConst_->numOutputPorts) != 0)                                             // @0x150f50: validate index fits
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

    int argType = static_cast<int>(args[0].varType_);                                                   // @0x151502: [rbp-0x78]

    if (argType == 2) {
        // Integer literal path — emit suser directly with value as immediate                            // @0x151518
        appendSuser(results->assemblers_, asmCommands_, AsmRegister(args[0].value_.toInt()), detail::AddressImpl<unsigned int>(kSuserPrngSeed)); // @0x151528                                                     // PRNG seed register                                             // @0x151535
    } else if ((argType & ~2) == 4) {
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
    // @0x151f0b-0x151f69: range and ordering validation
    int val0 = args[0].value_.toInt();                                                                  // @0x151f0b
    if (val0 < 0)                                                                                       // @0x151f12
        throw CustomFunctionsValueException(
            ErrorMessages::format(PrngRangeArgs), 0);                                // @0x152463
    int val1 = args[1].value_.toInt();                                                                  // @0x151f22
    if (val1 < 0)                                                                                       // @0x151f29
        throw CustomFunctionsValueException(
            ErrorMessages::format(PrngRangeArgs), 0);

    if (val0 > 0xFFFE)                                                                                 // @0x151f37: cmp eax, 0xfffe; jg
        throw CustomFunctionsValueException(
            ErrorMessages::format(PrngRangeArgs), 0);
    if (val1 >= 0xFFFF)                                                                                 // @0x151f4a: cmp eax, 0xffff; jge
        throw CustomFunctionsValueException(
            ErrorMessages::format(PrngRangeArgs), 0);

    int rangeMin = args[0].value_.toInt();                                                              // @0x151f58
    int rangeMax = args[1].value_.toInt();                                                              // @0x151f62
    if (rangeMin > rangeMax)                                                                            // @0x151f69
        throw CustomFunctionsException(
            ErrorMessages::format(PrngRangeArgs));                                   // @0x152522

    rangeMin = args[0].value_.toInt();                                                                  // @0x151f72
    rangeMax = args[1].value_.toInt();                                                                  // @0x151f7c

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
    size_t maxArgs = (config_->deviceType == static_cast<AwgDeviceType>(8)) ? 5 : 4;                       // @0x152700
    if (args.size() > maxArgs)
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsMaxArgs, std::string("startQA")));

    // @0x152740: validate all provided args are int type ((varType | 2) == 6 → types 4,5,6,7)
    for (size_t i = 0; i < args.size(); ++i) {                                                             // @0x152748
        if ((static_cast<int>(args[i].varType_) | 2) != 6)
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst));
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
    if (config_->deviceType == static_cast<AwgDeviceType>(8)) {                                            // @0x152840
        auto qaGenAllErv = res->readConst("QA_GEN_ALL", EDirection::eOUT);                        // @0x152870
        int qaGenAll = qaGenAllErv.value_.toInt();                                                          // @0x152880

        if (!args.empty()) {
            integrationWeightsMask = args[0].value_.toInt();                                                // @0x1528a0
            if (~qaGenAll & integrationWeightsMask)                                                         // @0x1528b0: validate mask
                throw CustomFunctionsException(
                    ErrorMessages::format(FuncExpectsConst));
        }
        // qaGenAllEnabled is set later, after qaIntAll is finalized                                          // bit 30 flag
    }

    // @0x152900: read QA_INT_ALL
    auto qaIntAllErv = res->readConst("QA_INT_ALL", EDirection::eOUT);                            // @0x152930
    qaIntAll = qaIntAllErv.value_.toInt();                                                                  // @0x152940

    // @0x152960: args[1] (or args[0] for non-SHFQA) — integration trigger mask
    size_t intTrigIdx = (config_->deviceType == static_cast<AwgDeviceType>(8)) ? 1 : 0;
    if (args.size() > intTrigIdx) {
        int intTrigMask = args[intTrigIdx].value_.toInt();                                                  // @0x152970
        if (~qaIntAll & intTrigMask)                                                                        // @0x152980: validate
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst));
        qaIntAll = intTrigMask;                                                                             // override with user value
    }

    // qaGenAllEnabled: set based on whether qaIntAll is nonzero (for SHFQA only)
    if (config_->deviceType == static_cast<AwgDeviceType>(8)) {
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
                ErrorMessages::format(FuncExpectsConst));
    }

    // @0x152a40: optional result address arg
    size_t raIdx = rlsIdx + 1;
    if (args.size() > raIdx) {
        resultAddr = args[raIdx].value_.toInt();                                                            // @0x152a50
        if (resultAddr >= 256)                                                                              // @0x152a60
            throw CustomFunctionsException(
                ErrorMessages::format(FuncExpectsConst));
    }

    AsmRegister zero(0);

    if (config_->deviceType == static_cast<AwgDeviceType>(8)) {                                            // @0x152f2c: SHFQA path
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
    } else if (config_->deviceType == static_cast<AwgDeviceType>(4)) {                                     // @0x1533df: UHFQA path
        // @0x153600: sid(reg, false) — st(reg, 0x21)
        {
            int regNum = Resources::getRegisterNumber();                                                    // @0x153608
            AsmRegister reg(regNum);
            // UHFQA composite: (qaIntAll << 16) | ((qaIntAll != 0) << 4) | (monitorEnable << 5) | resultAddr
            int composite = (qaIntAll << 16) | ((qaIntAll != 0 ? 1 : 0) << 4)                              // @0x153638
                          | (monitorEnable << 5) | resultAddr;                                              // @0x153659
            // @0x153670: zero the register before sid — binary emits addi Rn, R0, 0
            auto zeroEntries = asmCommands_->addi(reg, zero, Immediate(0));                                // @0x153670
            for (auto& e : zeroEntries) results->assemblers_.push_back(std::move(e));
            results->assemblers_.push_back(asmCommands_->sid(reg, false));                                  // @0x153690

            auto addi32Entries = asmCommands_->addi32(reg, zero, Immediate(composite));                    // @0x1536e0
            for (auto& e : addi32Entries) results->assemblers_.push_back(std::move(e));

            results->assemblers_.push_back(asmCommands_->strig(reg));                                      // @0x1537a5
        }

        // @0x153880: result address via addi + strig
        // Binary @0x153889: loads [rbp-0x11c] (= resultAddr, NOT resultLengthShift)
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
    unsigned int addr = (config_->deviceType == static_cast<AwgDeviceType>(4)) ? 0x62u : 0x6du;
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
    if (static_cast<int>(arg0.varType_) == 2 ||
        static_cast<int>(arg1.varType_) == 2 ||
        static_cast<int>(arg2.varType_) == 2) {
        // NOTE: error code for register-bound args not confirmed;
        //       using 0x3e as approximation.                                  // @0x1551b9
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConst, std::string("configFreqSweep")));
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

    // writeLS64bit(startFreqEncoded, 0x8e, 0x8f, results, res)                // @0x1547ba
    writeLS64bit(startFreqEncoded, 0x8e, 0x8f, results, res);

    // Convert step frequency: toFrequency(stepFreq, getSampleClock())         // @0x154852
    double sampleClock2 = getSampleClock();
    uint64_t stepFreqEncoded = NodeMap::toFrequency(stepFreq, sampleClock2);   // @0x154876

    // writeLS64bit(stepFreqEncoded, 0x90, 0x91, results, res)                 // @0x1548c4
    writeLS64bit(stepFreqEncoded, 0x90, 0x91, results, res);

    // Get register, addi(reg, R0, arg0.toInt()), then suser(reg, 0x8c)        // @0x154961
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    AsmRegister r0(0);
    int oscIntVal = arg0.value_.toInt();
    auto addiEntries = asmCommands_->addi(reg, r0, Immediate(oscIntVal));
    for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

    auto suserEntry = asmCommands_->suser(reg, 0x8c);                         // @0x154aec
    results->assemblers_.push_back(std::move(suserEntry));

    // addWaitCycles(10, results, res)                                         // @0x154c0d
    addWaitCycles(10, results, res);

    // Device-type-dependent node path construction:                           // @0x154c8d
    auto dt = static_cast<int>(config_->deviceType);
    std::string nodePath;
    if (dt == 0x40 || dt == 0x80 || dt == 0x100) {
        // @0x154cb3: "oscs/" + to_string(oscIntVal) + "/freq"
        nodePath = "oscs/" + std::to_string(oscIntVal) + "/freq";              // @0x154cca
    } else if (dt == 0x10 || dt == 0x20) {
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
    if (static_cast<int>(arg0.varType_) == 2)
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
    if (static_cast<int>(arg1.varType_) == 2) {
        // arg1 is register-bound: suser(arg1.register, 0x8d)                       // @0x1558b2
        auto suserEntry = asmCommands_->suser(arg1.reg_, kSuserSweepControl);
        results->assemblers_.push_back(std::move(suserEntry));
    } else if ((static_cast<int>(arg1.varType_) & ~2) == 4) {
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
    auto dt = static_cast<int>(config_->deviceType);
    std::string nodePath;
    if (dt == 0x8) {
        // SHFQA: "qachannels/<awgIndex>/oscs/<oscIndex>/freq"                       // @0x1560fb
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "qachannels/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (dt == 0x10 || dt == 0x20) {
        // SHFSG/SHFQC_SG: "sgchannels/<awgIndex>/oscs/<oscIndex>/freq"             // @0x155e2b
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "sgchannels/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (dt == 0x2) {
        // HDAWG: "generators/<awgIndex>/oscs/<oscIndex>/freq"                       // @0x1560fb
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "generators/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (dt == AwgDeviceType::SHFLI || dt == AwgDeviceType::GHFLI || dt == AwgDeviceType::VHFLI) {
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
    if (static_cast<int>(arg0.varType_) == 2 ||
        static_cast<int>(arg1.varType_) == 2) {
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

    auto suserEntry1 = asmCommands_->suser(reg1, 0x8d);                             // @0x156e4c
    results->assemblers_.push_back(std::move(suserEntry1));

    // Convert frequency: toFrequency(arg1.toDouble(), getSampleClock())            // @0x156f28
    double freq = arg1.value_.toDouble();
    double sampleClock = getSampleClock();
    uint64_t freqEncoded = NodeMap::toFrequency(freq, sampleClock);                  // @0x156f4c

    // writeLS64bit(freqEncoded, 0x8e, 0x8f, results, res)                          // @0x156f8e
    writeLS64bit(freqEncoded, 0x8e, 0x8f, results, res);

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
    auto dt = static_cast<int>(config_->deviceType);
    std::string nodePath;
    if (dt == 0x8) {
        // SHFQA: "qachannels/<awgIndex>/oscs/<oscIndex>/freq"                       // @0x157391
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "qachannels/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (dt == 0x10 || dt == 0x20) {
        // SHFSG/SHFQC_SG: "sgchannels/<awgIndex>/oscs/<oscIndex>/freq"             // @0x155e2b (same string)
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "sgchannels/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (dt == 0x2) {
        // HDAWG: "generators/<awgIndex>/oscs/<oscIndex>/freq"                       // @0x15765e
        std::string awgIdx = std::to_string(config_->awgIndex);
        nodePath = "generators/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (dt == AwgDeviceType::SHFLI || dt == AwgDeviceType::GHFLI || dt == AwgDeviceType::VHFLI) {
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

    auto const& arg0 = args[0];  // source (feedback source index)
    auto const& arg1 = args[1];  // shift
    auto const& arg2 = args[2];  // number of bits
    auto const& arg3 = args[3];  // threshold

    // Args 1, 2 (from arg1.varType), and 3 must not be register-bound              // @0x1584ba
    if (static_cast<int>(arg1.varType_) == 2 ||
        static_cast<int>(arg2.varType_) == 2 ||
        static_cast<int>(arg3.varType_) == 2) {
        throw CustomFunctionsException(
            ErrorMessages::format(FuncExpectsConstVar,
                                  std::string("configureFeedbackProcessing")));      // @0x158e29
    }

    // Build set of valid source IDs based on devConst_->numOutputPorts                   // @0x1584da
    int shift = 1 << static_cast<int>(devConst_->numOutputPorts);                         // @0x1584e9
    int src1 = shift + 1;
    int src2 = shift + 2;

    std::unordered_set<int> validSources;
    validSources.insert(src1);
    validSources.insert(src2);

    // If deviceType == 0x20, also add shift+4                                       // @0x158558
    if (static_cast<int>(config_->deviceType) == 0x20) {
        int src3 = shift + 4;
        validSources.insert(src3);
    }

    // Validate arg0 (source) is in the valid set                                    // @0x15857d
    int sourceVal = arg0.value_.toInt();
    if (validSources.find(sourceVal) == validSources.end()) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(InvalidArgValue, 0,
                                  std::string("configureFeedbackProcessing")), 0);   // @0x158c27
    }

    // Validate arg1 (shift): must be in [0, 32)                                     // @0x158697
    int shiftVal = arg1.value_.toInt();
    if (shiftVal < 0 || shiftVal >= 0x20) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(InvalidArgValue, 1,
                                  std::string("configureFeedbackProcessing")), 0);   // @0x158ca9
    }

    // Validate arg2 (number of bits): must be in (0, 17)                            // @0x1586c0
    int numBitsVal = arg2.value_.toInt();
    if (numBitsVal <= 0 || numBitsVal >= 0x11) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(InvalidArgValue, 2,
                                  std::string("configureFeedbackProcessing")), 0);   // @0x158d05
    }

    // Validate arg3 (threshold): must be in [0, 0x1000)                             // @0x1586e1
    int thresholdVal = arg3.value_.toInt();
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
        sourceToMode[shift + 4] = 2;
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
