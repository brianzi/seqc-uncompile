// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CustomFunctions — implementation of 12 analyzed methods + exception classes.
//
// ~60 remaining SeqC built-in function bodies are return-nullptr stubs with addresses.
// ============================================================================

#include <boost/format.hpp>
#include <boost/regex.hpp>

#include "zhinst/custom_functions.hpp"
#include "zhinst/asm_commands.hpp"
#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/eval_results.hpp"
#include "zhinst/math_compiler.hpp"
#include "zhinst/eval_result_value.hpp"
#include "zhinst/node_map_data.hpp"
#include "zhinst/resources.hpp"
#include "zhinst/types.hpp"
#include "zhinst/wavetable_front.hpp"
#include "zhinst/waveform_front.hpp"
#include "zhinst/waveform_generator.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace zhinst {
// Forward declaration — defined in get_node_map.cpp
std::unique_ptr<NodeMap> getNodeMapForDevice(AwgDeviceType allowedDevTypes);
} // namespace zhinst

#include "zhinst/log_macros.hpp"
#include <unordered_map>

namespace zhinst {

extern ErrorMessages errMsg;  // at 0x95de60

// floatEqual @0x2ec050 — exact bitwise double equality (defined in waveform_generator.cpp).
bool floatEqual(double a, double b);

// ============================================================================
// AccessMode
// ============================================================================

// toString(AccessMode) — static table at .rodata 0x9573c0
std::string toString(AccessMode mode) {  // string table at 0x9573c0
    // SSO strings: [0]="soft", [1]="direct", [2]="custom"
    switch (mode) {
        case AccessMode::Soft:   return "soft";
        case AccessMode::Direct: return "direct";
        case AccessMode::Custom: return "custom";
        default: return "unknown";
    }
}

// ============================================================================
// CustomFunctionsException
// ============================================================================

CustomFunctionsException::CustomFunctionsException(std::string const& msg)  // @0x15a4c0
    : message_(msg) {}

CustomFunctionsException::~CustomFunctionsException() {}  // @0x15a520

const char* CustomFunctionsException::what() const noexcept {  // @0x16e710
    if (message_.empty())
        return "CustomFunctions Exception";
    return message_.c_str();
}

// ============================================================================
// CustomFunctionsValueException
// ============================================================================

CustomFunctionsValueException::CustomFunctionsValueException(  // @0x163d00
    std::string const& msg, size_t argIndex)
    : message_(msg), argIndex_(argIndex) {}

CustomFunctionsValueException::~CustomFunctionsValueException() {}  // @0x163d70

const char* CustomFunctionsValueException::what() const noexcept {  // @0x172fd0
    // NOTE: likely formats message_ + argIndex_ + varName_ together
    return message_.c_str();
}

void CustomFunctionsValueException::setVarName(std::string const& name) {  // @0x210750
    varName_ = name;
}

// ============================================================================
// CustomFunctions — constructor / destructor
// ============================================================================

CustomFunctions::CustomFunctions(  // @0x12bcf0 — 19KB, registers 81 built-in functions (76 + 5 aliases)
    AWGCompilerConfig const& config,
    DeviceConstants const& devConst,
    std::shared_ptr<WavetableFront> wavetable,
    std::shared_ptr<WaveformGenerator> waveformGen,
    std::shared_ptr<AsmCommands> asmCommands,
    std::function<void(std::string const&)> const& warningCb)
    : config_(&config),
      devConst_(&devConst),
      wavetableFront_(std::move(wavetable)),
      waveformGen_(std::move(waveformGen)),
      asmCommands_(std::move(asmCommands)),
      warningCallback_(warningCb)
{
    // Binary (0x12bcf0..0x130780, 19KB):
    //  1. Stores config_/devConst_ pointers
    //  2. Allocates and constructs a private WavetableFront at +0x30
    //     (0x220 bytes, shared_ptr_emplace)
    //  3. Copies WaveformGenerator/AsmCommands shared_ptrs
    //  4. Zero-initializes funcMap_ hash table at +0x60
    //  5. Constructs MathCompiler at +0xC8
    //  6. Copies warningCallback_ (std::function) to +0x190
    //  7. Initializes usedFeatures_ set at +0x1C8
    //  8. Registers all 87 built-in functions into funcMap_ via:
    //     - Build SSO string key on stack (mov with hex-encoded ASCII)
    //     - emplace_unique_key_args into funcMap_
    //     - Construct std::bind wrapper binding method pointer + this
    //     - Swap bound function into map entry
    //
    // Registration pattern (repeated 87 times):
    //   funcMap_["setDIO"] = std::bind(&CustomFunctions::setDIO, this, _1, _2);
    //   funcMap_["getDIO"] = std::bind(&CustomFunctions::getDIO, this, _1, _2);
    //   ... etc for all 87 functions listed below.
    //
    // Full function list (alphabetical):
    //   addNodeAccess, addSyncCommand, addWaitCycles, assignWaveIndex,
    //   at, configFreqSweep, configureFeedbackProcessing, error,
    //   executeTableEntry, generate, getAnaTrigger, getCnt, getDigTrigger,
    //   getDIO, getDIOTriggered, getFeedback, getPRNGValue, getQAResult,
    //   getSampleClock, getSweeperLength, getTrigger, getUserReg, getZSyncData,
    //   incrementSinePhase, info, lock, now, play, playAuxWave,
    //   playAuxWaveIndexed, playDIOWave, playHold, playIndexed, playWave,
    //   playWaveDIO, playWaveDigTrigger, playWaveIndexed, playWaveIndexedNow,
    //   playWaveNow, playWaveZSync, playZero, prefetch, prefetchIndexed,
    //   printF, randomSeed, resetOscPhase, resetRTLoggerTimestamp,
    //   setDIO, setDouble, setID, setInt, setInternalTrigger, setOscFreq,
    //   setPrecompClear, setPRNGRange, setPRNGSeed, setRate, setSinePhase,
    //   setSweepStep, setTrigger, setUserReg, setWaitCyclesReg, setWaveDIO,
    //   startQA, startQAMonitor, startQAResult, sync, unlock, wait,
    //   waitAnaTrigger, waitCntTrigger, waitDemodOscPhase, waitDemodSample,
    //   waitDigTrigger, waitDIOTrigger, waitOnGrid, waitOnSync,
    //   waitPlayQueueEmpty, waitQAResultTrigger, waitSineOscPhase,
    //   waitTimestamp, waitTrigger, waitWave, waitZSyncTrigger,
    //   writeLS64bit, writeToNode
    //
    // Registration block: 81 emplace calls in binary (0x12bf4a..0x130780).
    // Pattern: funcMap_["name"] = std::bind(&CustomFunctions::name, this, _1, _2);
    // Reconstructed as equivalent lambdas for readability.
    //
    // 76 standard bindings (string key == method name):
    auto reg = [this](const char* name, FuncType fn) { funcMap_[name] = std::move(fn); };
    auto bind = [this](auto method) -> FuncType {
        return [this, method](std::vector<EvalResultValue> const& a,
                              std::shared_ptr<Resources> r) {
            return (this->*method)(a, std::move(r));
        };
    };

    reg("setDIO",              bind(&CustomFunctions::setDIO));
    reg("getDIO",              bind(&CustomFunctions::getDIO));
    reg("getDIOTriggered",     bind(&CustomFunctions::getDIOTriggered));
    reg("getZSyncData",        bind(&CustomFunctions::getZSyncData));
    reg("getFeedback",         bind(&CustomFunctions::getFeedback));
    reg("setID",               bind(&CustomFunctions::setID));
    reg("assignWaveIndex",     bind(&CustomFunctions::assignWaveIndex));
    reg("prefetch",            bind(&CustomFunctions::prefetch));
    reg("prefetchIndexed",     bind(&CustomFunctions::prefetchIndexed));
    reg("playWave",            bind(&CustomFunctions::playWave));
    reg("playWaveNow",         bind(&CustomFunctions::playWaveNow));
    reg("playWaveIndexed",     bind(&CustomFunctions::playWaveIndexed));
    reg("playWaveIndexedNow",  bind(&CustomFunctions::playWaveIndexedNow));
    reg("playAuxWave",         bind(&CustomFunctions::playAuxWave));
    reg("playAuxWaveIndexed",  bind(&CustomFunctions::playAuxWaveIndexed));
    reg("playDIOWave",         bind(&CustomFunctions::playDIOWave));
    reg("playWaveDIO",         bind(&CustomFunctions::playWaveDIO));
    reg("playWaveZSync",       bind(&CustomFunctions::playWaveZSync));
    reg("playWaveDigTrigger",  bind(&CustomFunctions::playWaveDigTrigger));
    reg("playZero",            bind(&CustomFunctions::playZero));
    reg("playHold",            bind(&CustomFunctions::playHold));
    reg("wait",                bind(&CustomFunctions::wait));
    reg("waitWave",            bind(&CustomFunctions::waitWave));
    reg("waitTrigger",         bind(&CustomFunctions::waitTrigger));
    reg("waitAnaTrigger",      bind(&CustomFunctions::waitAnaTrigger));
    reg("waitDigTrigger",      bind(&CustomFunctions::waitDigTrigger));
    reg("waitOnGrid",          bind(&CustomFunctions::waitOnGrid));
    reg("waitOnSync",          bind(&CustomFunctions::waitOnSync));
    reg("waitDIOTrigger",      bind(&CustomFunctions::waitDIOTrigger));
    reg("waitZSyncTrigger",    bind(&CustomFunctions::waitZSyncTrigger));
    reg("waitCntTrigger",      bind(&CustomFunctions::waitCntTrigger));
    reg("waitDemodOscPhase",   bind(&CustomFunctions::waitDemodOscPhase));
    reg("waitSineOscPhase",    bind(&CustomFunctions::waitSineOscPhase));
    reg("waitTimestamp",       bind(&CustomFunctions::waitTimestamp));
    reg("resetOscPhase",       bind(&CustomFunctions::resetOscPhase));
    reg("setSinePhase",        bind(&CustomFunctions::setSinePhase));
    reg("incrementSinePhase",  bind(&CustomFunctions::incrementSinePhase));
    reg("waitDemodSample",     bind(&CustomFunctions::waitDemodSample));
    reg("waitPlayQueueEmpty",  bind(&CustomFunctions::waitPlayQueueEmpty));
    reg("setTrigger",          bind(&CustomFunctions::setTrigger));
    reg("getTrigger",          bind(&CustomFunctions::getTrigger));
    reg("setInternalTrigger",  bind(&CustomFunctions::setInternalTrigger));
    reg("getAnaTrigger",       bind(&CustomFunctions::getAnaTrigger));
    reg("getDigTrigger",       bind(&CustomFunctions::getDigTrigger));
    reg("setInt",              bind(&CustomFunctions::setInt));
    reg("setDouble",           bind(&CustomFunctions::setDouble));
    reg("randomSeed",          bind(&CustomFunctions::randomSeed));
    reg("generate",            bind(&CustomFunctions::generate));
    reg("setUserReg",          bind(&CustomFunctions::setUserReg));
    reg("getUserReg",          bind(&CustomFunctions::getUserReg));
    reg("getSweeperLength",    bind(&CustomFunctions::getSweeperLength));
    reg("setRate",             bind(&CustomFunctions::setRate));
    reg("setPrecompClear",     bind(&CustomFunctions::setPrecompClear));
    reg("setWaveDIO",          bind(&CustomFunctions::setWaveDIO));
    reg("now",                 bind(&CustomFunctions::now));
    reg("at",                  bind(&CustomFunctions::at));
    reg("error",               bind(&CustomFunctions::error));
    reg("info",                bind(&CustomFunctions::info));
    reg("lock",                bind(&CustomFunctions::lock));
    reg("unlock",              bind(&CustomFunctions::unlock));
    reg("sync",                bind(&CustomFunctions::sync));
    reg("getCnt",              bind(&CustomFunctions::getCnt));
    reg("waitQAResultTrigger", bind(&CustomFunctions::waitQAResultTrigger));
    reg("getQAResult",         bind(&CustomFunctions::getQAResult));
    reg("startQAResult",       bind(&CustomFunctions::startQAResult));
    reg("startQAMonitor",      bind(&CustomFunctions::startQAMonitor));
    reg("executeTableEntry",   bind(&CustomFunctions::executeTableEntry));
    reg("setPRNGSeed",         bind(&CustomFunctions::setPRNGSeed));
    reg("getPRNGValue",        bind(&CustomFunctions::getPRNGValue));
    reg("setPRNGRange",        bind(&CustomFunctions::setPRNGRange));
    reg("startQA",             bind(&CustomFunctions::startQA));
    reg("resetRTLoggerTimestamp", bind(&CustomFunctions::resetRTLoggerTimestamp));
    reg("configFreqSweep",     bind(&CustomFunctions::configFreqSweep));
    reg("setSweepStep",        bind(&CustomFunctions::setSweepStep));
    reg("setOscFreq",          bind(&CustomFunctions::setOscFreq));
    reg("configureFeedbackProcessing", bind(&CustomFunctions::configureFeedbackProcessing));

    // 5 aliases (different string key → same method as an existing primary entry):
    reg("setSeqIndex",                  bind(&CustomFunctions::setID));             // alias (confirmed)
    reg("setReadoutRegisterAddress",    bind(&CustomFunctions::setID));             // alias @0x12c352
    reg("waitOscPhaseOfDemod",          bind(&CustomFunctions::waitDemodOscPhase)); // alias @0x12d3df
    reg("setUser",                      bind(&CustomFunctions::setUserReg));        // alias @0x12dea1
    reg("getUser",                      bind(&CustomFunctions::getUserReg));        // alias @0x12df3b
}

CustomFunctions::~CustomFunctions() {}  // @0x127c90

// ============================================================================
// CustomFunctions — public API
// ============================================================================

bool CustomFunctions::functionExists(std::string const& name) const {  // @0x159410
    return funcMap_.count(name) > 0;
}

std::shared_ptr<EvalResults> CustomFunctions::call(  // @0x159470 — 3200 bytes
    std::string const& name,
    std::vector<EvalResultValue> const& args,
    std::shared_ptr<Resources> res)
{
    // Binary flow (0x159470..0x15a0f0):
    //  1. Look up name in aliasMap_ (+0x88) for alias/overload resolution
    //  2. If found and vector has 1 entry → format error with ErrorMessageT(0x37)
    //     (wrong arg count, 2-arg format: function name + expected count)
    //  3. If found and vector has 2 entries → format error with ErrorMessageT(0x38)
    //     (3-arg format: function name + both alias strings)
    //  4. On error, invoke error callback at +0x1B0
    //  5. Check config_->isRecompile flag (+0x19); if set → check field_B0 set
    //  6. Look up name in funcMap_ (+0x60)
    //  7. If found → dispatch: return it->second(args, res)
    //  8. If not in funcMap_ → try MathCompiler::functionExists (at +0xC8)
    //  9. If MathCompiler has it → extract doubles from args, call mathCompiler_.call()
    //     → wrap result in EvalResults with setValue(Double, Value(double))
    // 10. If not in MathCompiler either → call generateWaveform(name, args, res)
    //     (which prepends name as first arg and delegates to generate())

    // Step 1-4: alias resolution / argument count validation
    auto aliasIt = aliasMap_.find(name);
    if (aliasIt != aliasMap_.end()) {
        // NOTE: aliasMap_ is empty in this binary (confirmed Phase 13c).
        // If populated, the vector size selects error format 0x37 (1 entry) or 0x38 (2 entries).
    }

    // Step 6-7: dispatch via funcMap_
    auto it = funcMap_.find(name);
    if (it != funcMap_.end()) {
        return it->second(args, std::move(res));
    }

    // Step 8-9: try MathCompiler                                  @0x159814
    // Binary: compute arg count from vector, call functionExists(name, argCount, false)
    size_t argCount = args.size();
    if (mathCompiler_.functionExists(name, argCount, false)) {     // @0x159841
        // Extract args as doubles                                 @0x15986e
        std::vector<double> doubleArgs;
        doubleArgs.reserve(argCount);
        for (auto const& a : args) {
            // Binary: checks (varType | 2) == 6, i.e. varType is 4 (Const) or 6 (Cvar)
            // Then calls Value::toDouble on the embedded value at +0x08
            doubleArgs.push_back(a.value_.toDouble());             // @0x1598e5
        }
        // Call MathCompiler::call                                 @0x159a3f
        double result = mathCompiler_.call(name, doubleArgs);      // @0x159a3f
        // Wrap in EvalResults with setValue(Double)               @0x159a50
        auto er = std::make_shared<EvalResults>();
        er->setValue(VarType_Cvar, Value(result));
        return er;
    }

    // Step 10: fall through to waveform generation                @0x1599c8
    return generateWaveform(name, args, std::move(res));
}

// ============================================================================
// CustomFunctions — 12 analyzed methods
// ============================================================================

// checkPlayMinLength @0x15b100
// Compares length vs devConst_->waveformGranularity (+0x40).
// Warns via warningCallback_ with error 0xF5. Returns minPlayLength.
int CustomFunctions::checkPlayMinLength(int length) {  // @0x15b100
    int minLength = static_cast<int>(devConst_->waveformGranularity);
    if (length < minLength) {
        // @0x15b13a: format(0xF5, length, minLength)
        std::string msg = ErrorMessages::format(
            PlayLenBelowMin, length, minLength);
        // @0x15b15a: invoke warningCallback_ at +0x1b0
        warningCallback_(msg);
        return minLength;
    }
    return length;
}

// checkPlayAlignment @0x15b190
// Checks length % devConst_->waveformPageSize (+0x44).
// Rounds up if misaligned, warns with error 0xE7. Returns aligned length.
int CustomFunctions::checkPlayAlignment(int length) {  // @0x15b190
    int alignment = static_cast<int>(devConst_->waveformPageSize);
    if (alignment > 0 && (length % alignment) != 0) {
        int aligned = ((length / alignment) + 1) * alignment;
        // @0x15b1f0: format(0xE7, length, alignment, aligned)
        std::string msg = ErrorMessages::format(
            PlayLenNotAligned, length, alignment, aligned);
        // @0x15b210: invoke warningCallback_ at +0x1b0
        warningCallback_(msg);
        return aligned;
    }
    return length;
}

// oscMaskCheckGrimsel @0x15ba90
// Returns (mask >> devConst_->numDIOBits (+0x84)) == 0. Trivial.
bool CustomFunctions::oscMaskCheckGrimsel(unsigned int mask) {  // @0x15ba90
    return (mask >> devConst_->numDIOBits) == 0;
}

// oscMaskCheckHirzel @0x15bab0 — ~0x4A0 bytes (ends at 0x15bf50)
// Searches config_->includePaths for "MF"; if found, inserts into usedFeatures_
// and uses wider mask limits. Switches on numChannelGroups (dc) and awgIndex (gi).
bool CustomFunctions::oscMaskCheckHirzel(unsigned int mask) {  // @0x15bab0
    // @0x15bacd: construct SSO "MF" on stack (len=2, chars 'M','F')
    // @0x15bae0: iterate config_->includePaths (vector<string> at config+0x70)
    bool hasMF = false;
    for (auto const& s : config_->includePaths) {
        if (s == "MF") { hasMF = true; break; }
    }

    if (hasMF) {
        // @0x15bafa..15bb0b: insert "MF" into usedFeatures_ (+0x1C8)
        usedFeatures_.insert("MF");

        // @0x15bb30: mask limit 0xFFFF for MF
        if (mask > 0xFFFF)
            return false;  // @0x15bee5

        uint32_t dc = config_->numChannelGroups;  // +0x1c
        if (dc == 1) {
            uint32_t gi = config_->awgIndex;  // +0x20
            // Jump table @0x958e70, 4 cases
            switch (gi) {
            case 0: return mask < 0x10;              // @0x15bb69: cmp $0x10, setb
            case 1: return (mask & 0xFF0F) == 0;     // @0x15bd9c: test $0xff0f, sete
            case 2: return (mask & 0xF0FF) == 0;     // @0x15bd7e: test $0xf0ff, sete
            case 3: return (mask & 0xFFF) == 0;      // @0x15bd8d: test $0xfff, sete
            default:
                // @0x15bdab: log warning "Invalid group index: " + gi + "."
                LOG_WARNING("Invalid group index: " << config_->awgIndex << ".");
                break;
            }
        }
        // Fall through to dc==2 / dc==4 checks below (@0x15be27)
        if (dc == 2) {
            uint32_t gi = config_->awgIndex;
            // @0x15be36: switch on gi
            if (gi == 1) return (mask & 0xFF) == 0;     // @0x15be51: test r14b, sete
            if (gi == 0) return mask < 0x100;            // @0x15be42: cmp $0x100, setb
            // @0x15be5c: log warning for invalid gi
            LOG_WARNING("Invalid group index: " << config_->awgIndex << ".");
        }
        // @0x15bede: dc==4 → return true unconditionally
        if (dc == 4) return true;  // @0x15bee0: mov $0x1, al
        return false;  // @0x15bee5: xor eax, eax
    }

    // No MF path — @0x15bbec
    // @0x15bbec: mask limit 0xF
    if (mask > 0xF)
        return false;  // @0x15bee5

    uint32_t dc = config_->numChannelGroups;
    if (dc == 1) {
        uint32_t gi = config_->awgIndex;
        // Jump table @0x958e80, 4 cases
        switch (gi) {
        case 0: return mask < 2;              // @0x15bc1e: cmp $0x2, setb
        case 1: return (mask & 0xD) == 0;     // @0x15bc42: test $0xd, sete
        case 2: return (mask & 0xB) == 0;     // @0x15bc2a: test $0xb, sete
        case 3: return (mask & 0x7) == 0;     // @0x15bc36: test $0x7, sete
        default:
            // @0x15bc4e: log warning
            LOG_WARNING("Invalid group index: " << config_->awgIndex << ".");
            break;
        }
    }
    // @0x15bcd0: dc==2
    if (dc == 2) {
        uint32_t gi = config_->awgIndex;
        if (gi == 1) return (mask & 0x3) == 0;  // @0x15bcf1: test $0x3, sete
        if (gi == 0) return mask < 4;            // @0x15bce5: cmp $0x4, setb
        // @0x15bcfd: log warning
        LOG_WARNING("Invalid group index: " << config_->awgIndex << ".");
    }
    // @0x15bede: dc==4
    if (dc == 4) return true;
    return false;
}

// oscMaskSetAllHirzel @0x15bf50 — ~0x160 bytes (ends at 0x15c0b0)
// Computes all-oscillator mask based on numChannelGroups/awgIndex and "MF" feature.
unsigned int CustomFunctions::oscMaskSetAllHirzel() {  // @0x15bf50
    // @0x15bf59: construct SSO "MF" on stack
    // @0x15bf67: iterate config_->includePaths for "MF"
    bool hasMF = false;
    for (auto const& s : config_->includePaths) {
        if (s == "MF") { hasMF = true; break; }
    }

    uint32_t dc = config_->numChannelGroups;  // +0x1c
    uint32_t gi = config_->awgIndex;           // +0x20 (zero-extended byte)

    if (hasMF) {
        // @0x15bfd1: insert "MF" into usedFeatures_
        usedFeatures_.insert("MF");

        // @0x15bffe: switch on dc
        if (dc == 4) return 0xFFFF;              // @0x15c078
        if (dc == 2) return 0xFF << (gi * 8);    // @0x15c063: shl $0x3,cl; mov $0xff
        if (dc == 1) return 0xF << (gi * 4);     // @0x15c013: shl $0x2,cl; mov $0xf
        return 0;  // @0x15c041: xor eax,eax
    }

    // No MF path — @0x15c021
    if (dc == 4) return 0xF;                     // @0x15c057
    if (dc == 2) return 0x3 << (gi * 2);         // @0x15c04a: add cl,cl; mov $0x3
    if (dc == 1) return 1u << gi;                // @0x15c036: mov $0x1; shl cl
    return 0;
}

// oscMaskSetAllGrimsel @0x15c0b0
// Returns (1 << oscBitShift) - 1. Trivial.
unsigned int CustomFunctions::oscMaskSetAllGrimsel() {  // @0x15c0b0
    return (1u << devConst_->numDIOBits) - 1;
}

// addNodeAccess @0x15c6c0
// Inserts mode into accessModeMap_ (+0x128), then does
// nodeIndexMap_.emplace(item, nodeList_.size()) — stores the INDEX into nodeList_,
// NOT a hardware address. If the emplace inserted a new entry, pushes item onto nodeList_.
void CustomFunctions::addNodeAccess(NodeMapItem const& item, AccessMode mode) {  // @0x15c6c0
    // @0x15c6f5: accessModeMap_[item].insert(mode)
    accessModeMap_[item].insert(mode);

    // @0x15c771-0x15c7a1: nodeIndexMap_.emplace(item, nodeList_.size())
    uint32_t idx = static_cast<uint32_t>(nodeList_.size());
    auto [it, inserted] = nodeIndexMap_.emplace(item, idx);

    // @0x15c7a6: test $0x1, %dl — check if newly inserted
    if (inserted) {
        // @0x15c7bb-0x15c7f6: nodeList_.push_back(item) (with clone of data)
        nodeList_.push_back(item);
    }
}

// getWaitTime @0x163930
// Computes wait cycles from samples and rate.
// Device type 2: max(0, ((samples+7)<<rate+7)/8 - 3)
// Others: ((samples+3)<<rate+3)/4
int CustomFunctions::getWaitTime(int samples, int rate) {  // @0x163930
    int cl = std::max(0, rate);  // clamp rate to non-negative
    if (devConst_->deviceType == HDAWG) {
        // HDAWG path: left-shift (samples+7) by cl, then arithmetic right-shift by 3
        int val = ((samples + 7) << cl) >> 3;  // @0x163960: shl then sar 3
        return std::max(val, 4) - 3;            // clamp to >= 4, then subtract 3
    } else {
        return ((samples + 3) << cl) >> 2;      // @0x163980: shl then sar 2
    }
}

// initNodeMap @0x16b740
// Lazy-init. Dispatches to GetNodeMap<N>::get() based on deviceType.
void CustomFunctions::initNodeMap() {  // @0x16b740
    if (nodeMap_) return;  // already initialized
    nodeMap_ = getNodeMapForDevice(
        static_cast<AwgDeviceType>(devConst_->deviceType));
}

// getNodeAddress @0x16ba10
// Tries dynamic_cast<DirectAddrNodeMapData*>, else looks up nodeIndexMap_.
uint32_t CustomFunctions::getNodeAddress(NodeMapItem const& item) const {  // @0x16ba10
    if (auto* direct = dynamic_cast<DirectAddrNodeMapData*>(item.data)) {
        return direct->addr_;  // fast path: DirectAddrNodeMapData contains address at +0x08
    }
    // slow path: lookup in nodeIndexMap_ (+0x100)
    return nodeIndexMap_.at(item);  // throws std::out_of_range on miss
}

// getSampleClock @0x16ba80
// Reads "$DEVICE_SAMPLE_RATE" from resources_ (+0x10), fallback to devConst_->samplingRate (+0x70).
double CustomFunctions::getSampleClock() const {  // @0x16ba80
    // Binary first checks resources_ non-null, then variableExists("$DEVICE_SAMPLE_RATE").
    // If exists: reads the constant and extracts the double value.
    // Fallback: returns devConst_->samplingRate (+0x70).
    if (resources_ && resources_->variableExists("$DEVICE_SAMPLE_RATE")) {
        EvalResultValue erv = resources_->readConst("$DEVICE_SAMPLE_RATE",
                                                      EDirection::eOUT);  // @0x16bac0
        return erv.value_.toDouble();                                      // @0x16bad0
    }
    return devConst_->samplingRate;
}

// getAccessModes @0x16be50
// Looks up accessModeMap_ (+0x128). Returns const set<AccessMode>&.
std::set<AccessMode> const& CustomFunctions::getAccessModes(NodeMapItem const& item) const {  // @0x16be50
    auto it = accessModeMap_.find(item);
    if (it != accessModeMap_.end()) {
        return it->second;
    }
    // @0x16be80: throws std::out_of_range on miss (binary calls __throw_out_of_range)
    throw std::out_of_range("getAccessModes: item not found in accessModeMap_");
    static const std::set<AccessMode> empty;
    return empty;
}

// ============================================================================
// CustomFunctions — utility method stubs (not in the 12 analyzed set)
// ============================================================================

// checkFunctionSupported @0x15aeb0 — 0x150 bytes (ends at 0x15b000)
// Verifies that the current device (config_->deviceType, a single power-of-2
// AwgDeviceType value) is among the devices allowed by `allowedDevTypes` (a bitmask
// of AwgDeviceType values OR'd together).
// Test: if (config_->deviceType & ~allowedDevTypes) != 0, the device's bit is outside
// the allowed mask → throws CustomFunctionsException with error FuncNotSupported.
void CustomFunctions::checkFunctionSupported(std::string const& name, AwgDeviceType allowedDevTypes) {  // @0x15aeb0
    // @0x15aebd: rax = [this+0x00] (config_), edx = ~allowedDevTypes
    // @0x15aec2: test [rax+0x00], edx  → test config_->deviceType & ~allowedDevTypes
    uint32_t current = static_cast<uint32_t>(config_->deviceType);  // single power-of-2 device bit
    if ((current & ~static_cast<uint32_t>(allowedDevTypes)) == 0)
        return;  // @0x15aec6: early return — device is in the allowed mask

    // @0x15af14: esi = config_->deviceType (the actual device enum value)
    // @0x15af18: call AWGCompilerConfig::getAwgDeviceTypeString(AwgDeviceType)
    std::string devTypeStr = AWGCompilerConfig::getAwgDeviceTypeString(
        static_cast<AwgDeviceType>(current));  // @0x15af18

    // @0x15af29: esi = 0x49, call ErrorMessages::format<string,string>(0x49, name, devTypeStr)
    std::string msg = ErrorMessages::format(
        FuncNotSupported, name, devTypeStr);  // @0x15af2e

    // @0x15af3d: throw CustomFunctionsException(msg)
    throw CustomFunctionsException(msg);  // @0x15af42
}

// checkWaveformMinLengthTrig @0x15b000 — 0x100 bytes (ends at 0x15b100)
// If wf is non-null and config_->trigMinWaveformLength (+0x48) > wf->sampleLength (+0xd0),
// throws with error code 0xF3.
void CustomFunctions::checkWaveformMinLengthTrig(std::shared_ptr<WaveformFront> wf) {  // @0x15b000
    if (!wf)
        return;  // @0x15b013: je early return

    // @0x15b019: rcx = [this+0x08] (devConst_), ecx = [rcx+0x48] (playMinSamples)
    // @0x15b01c: cmp ecx, [rax+0xd0] (wf->signal.length_)
    int trigMinLen = static_cast<int>(devConst_->playMinSamples);
    int wfLen = static_cast<int>(wf->signal.length_);

    if (trigMinLen <= wfLen)
        return;  // @0x15b022: jg → falls through to throw path; jle returns

    // @0x15b043: esi = [devConst_+0x48] → to_string(trigMinLen)
    // @0x15b057: esi = 0xF3, call ErrorMessages::format<string>(0xF3, to_string(trigMinLen))
    std::string msg = ErrorMessages::format(
        MinWaveformLength,
        std::to_string(trigMinLen));  // @0x15b05c

    // @0x15b068: throw CustomFunctionsException(msg)
    throw CustomFunctionsException(msg);  // @0x15b06b
}

// checkOffspecWaveLength @0x15b370 — 0x2a0 bytes (ends at 0x15b610)
// Two cases:
//   Case 1 (expected > wf->sampleLength): wf name too long → warn with 0xF4
//   Case 2 (wf->sampleLength % alignment != 0): misaligned → warn with 0xE6
// Both warn via warningCallback_ (at this+0x1B0 vtable+0x30).
void CustomFunctions::checkOffspecWaveLength(std::shared_ptr<WaveformFront> wf, int expected) {  // @0x15b370
    if (!wf)
        return;  // @0x15b382: je exit

    // @0x15b38e: r14d = [wf+0xd0] (signal.length_)
    int wfLen = static_cast<int>(wf->signal.length_);

    if (expected > wfLen) {
        // Case 1: @0x15b398 jle → skip; falls through to warn path
        // @0x15b39a: read wf->name (string at wf+0x00)
        // @0x15b430: call to_string(expected)
        // @0x15b444: esi = 0xF4, call ErrorMessages::format<string, int, string>(0xF4, wfName, wfLen, to_string(expected))
        std::string const& wfName = wf->name;  // wf+0x00
        std::string msg = ErrorMessages::format(
            WaveformBelowMin, wfName, wfLen,
            std::to_string(expected));  // @0x15b44c

        // @0x15b451: warningCallback_ call via vtable+0x30
        if (!warningCallback_) std::__throw_bad_function_call();  // @0x15b550
        warningCallback_(msg);
        return;
    }

    // Case 2: check alignment
    // @0x15b3b4: r15d = devConst_->waveformPageSize
    int alignment = static_cast<int>(devConst_->waveformPageSize);

    if (wfLen % alignment == 0)
        return;  // @0x15b3c1: je exit

    // @0x15b3ca: if wfLen == 0 → roundedLen = 0
    unsigned int roundedLen = 0;
    if (wfLen != 0) {
        // @0x15b3d3: esi = wf->minLengthSamples ([wf+0x70])
        unsigned int minSamples = static_cast<unsigned int>(wf->minLengthSamples);
        // @0x15b3d6: eax = (unsigned)wfLen / alignment, edx = remainder
        unsigned int quotient = static_cast<unsigned int>(wfLen) / static_cast<unsigned int>(alignment);
        unsigned int remainder = static_cast<unsigned int>(wfLen) % static_cast<unsigned int>(alignment);
        // @0x15b3de: sbb r12d, -1 → r12d = quotient + (remainder > 0 ? 1 : 0)
        roundedLen = (quotient + (remainder > 0 ? 1 : 0)) * static_cast<unsigned int>(alignment);
        // @0x15b3ec: if minSamples > roundedLen → use minSamples
        if (minSamples > roundedLen)
            roundedLen = minSamples;
    }

    // @0x15b3f3: copy wf->name string
    std::string const& wfName = wf->name;
    // @0x15b4ec: esi = 0xE6, call ErrorMessages::format
    std::string msg = ErrorMessages::format(
        WaveNotAligned, wfName, wfLen, alignment, roundedLen);  // @0x15b4fa

    // @0x15b4ff: warningCallback_ call
    if (!warningCallback_) std::__throw_bad_function_call();  // @0x15b555
    warningCallback_(msg);
}

// optionAvailable @0x15b9c0 — 0xC3 bytes (ends at 0x15ba83)
// Searches config_->options vector (at config+0x70..config+0x78, vector<string> with 0x18 stride)
// for a matching string. If found, inserts into usedFeatures_ (+0x1C8) and returns true.
// If not found, returns false.
bool CustomFunctions::optionAvailable(std::string const& option) {  // @0x15b9c0
    // @0x15b9d4: rax = [this+0x00] (config_)
    // @0x15b9d5: r13 = includePaths.data(), r14 = includePaths.data() + includePaths.size()
    auto const& opts = config_->includePaths;

    // @0x15b9dc: if begin == end → not found
    // Linear scan comparing string lengths then memcmp @0x15ba1a
    bool found = false;
    for (auto const& it : opts) {
        if (it == option) {
            found = true;
            break;
        }
    }

    if (!found)
        return false;  // @0x15ba72: xor eax,eax

    // @0x15ba5c: insert option into usedFeatures_ (set<string> at this+0x1C8)
    // call std::__tree::__emplace_unique_key_args @0x176a30
    usedFeatures_.insert(option);  // @0x15ba69
    return true;  // @0x15ba6e: mov al, 1
}

// lookupNode @0x15c530 — 0x190 bytes (ends at 0x15c6c0)
// Calls initNodeMap(), then nodeMap_->retrieve(path).
// If retrieve returns valid (byte at result+0x18 != 0), copies the node data
// into the return value. If not found, throws CustomFunctionsValueException
// with error code 0x83.
NodeMapItem CustomFunctions::lookupNode(std::string const& path) {  // @0x15c530
    // @0x15c549: call initNodeMap()
    initNodeMap();  // @0x16b740

    // @0x15c54e: rsi = [this+0xF8] (nodeMap_.get())
    // @0x15c55c: call NodeMap::retrieve(path) @0x1c55d0
    // Returns a struct on stack: [rbp-0x38]=shared_ptr-like, [rbp-0x30]=int, [rbp-0x2c]=8bytes, [rbp-0x20]=valid_byte
    NodeMapItem result;

    // The retrieve call produces a NodeMapItem-like result struct at rbp-0x38
    // with a validity flag at rbp-0x20.
    auto retrieved = nodeMap_->retrieve(path);

    // @0x15c561: cmp BYTE [rbp-0x20], 0 → check valid flag
    if (retrieved.data) {  // valid
        // @0x15c567: rsi = [rbp-0x38] (NodeMapData*), call vtable+0x18 (clone/copy)
        // @0x15c574: copy remaining fields (type at rbp-0x30, address at rbp-0x2c)
        // @0x15c582: if byte==1 → release shared ownership of retrieved item
        return retrieved;  // @0x15c59f
    }

    // Not found path: @0x15c5ad
    // @0x15c5b2: allocate exception (0x40 bytes → CustomFunctionsValueException)
    // @0x15c5e7: copy path string
    // @0x15c5eb: esi = 0x83, call ErrorMessages::format<string>(0x83, path)
    std::string msg = ErrorMessages::format(
        NodeNotExist, path);  // @0x15c5f0

    // @0x15c5fc: construct CustomFunctionsValueException(msg, 0)
    // @0x15c61a: throw
    throw CustomFunctionsValueException(msg, 0);  // @0x15c601
}

// ============================================================================
// PlayArgs methods
// ============================================================================

// PlayArgs ctor @0x15d600 (168 bytes, ends at 0x15d6a8 approx)
PlayArgs::PlayArgs(AWGCompilerConfig const& config,
                   std::shared_ptr<WavetableFront> wavetable,
                   std::function<void(std::string const&)> reportWarning,
                   std::string const& cmdName,
                   bool indexed)
    : wavetable_(std::move(wavetable))                         // +0x00: copy shared_ptr, atomic inc
    , reportWarning_(std::move(reportWarning))                 // +0x10: copy std::function
    , cmdName_(cmdName)                                        // +0x40: copy string (SSO or heap)
    , channelsPerGroup_(config.channelsPerGroup[indexed ? 1 : 0])  // +0x58: uint16_t from config array
    , totalChannels_(static_cast<uint16_t>(
          channelsPerGroup_ * config.numChannelGroups))        // +0x5A
    , waveAssignments_(config.numChannelGroups)                // +0x60: N empty inner vectors
    , hasMarker_(false)                                        // +0x78
{
}

// PlayArgs dtor @0x15efe0 — compiler-generated default destructor.
// Destroys in reverse: waveAssignments_ (+0x60), cmdName_ (+0x40),
// reportWarning_ (+0x10), wavetable_ (+0x00).
PlayArgs::~PlayArgs() = default;

// PlayArgs::WaveAssignment copy ctor — @0x171c00
// Variant-aware copy of the EvalResultValue `value` field: dispatches on
// value.value_.which_ via jump table at 0x959034:
//   index 0 → int copy, 1 → double copy, 2 → string copy (placement new).
// Then copies the `bits` vector.
PlayArgs::WaveAssignment::WaveAssignment(WaveAssignment const& o)  // @0x171c00
    : bits(o.bits)                                                 // @0x171cb0: vector copy
{
    // Copy the VarType/VarSubType fields
    value.varType_    = o.value.varType_;
    value.varSubType_ = o.value.varSubType_;
    // Copy the AsmRegister
    value.reg_        = o.value.reg_;

    // Variant-aware copy of the embedded Value
    // @0x171c40: dispatch on o.value.value_.which_
    value.value_.type_  = o.value.value_.type_;
    value.value_.which_ = o.value.value_.which_;
    // Variant-aware copy keyed on which_:
    //   0 = int, 1 = bool, 2 = double, 3 = string
    // The original binary (libc++) uses a 3-way jump table at @0x171c40:
    //   index 0 → copy 4 bytes (int/bool),  index 1 → copy 8 bytes (double),
    //   index 2 → placement-new string.
    // But the binary's variant indices are 0,1,2 (libc++ boost::variant
    // with int,bool,double,string where int+bool share slot 0's copy).
    // Our libstdc++ Value uses which_ values 0..3.  Map accordingly:
    int decoded = (o.value.value_.which_ >> 31) ^ o.value.value_.which_;
    if (decoded >= 3) {
        // string — placement new                              // @0x171c80
        ::new (&value.value_.storage_.str)
            std::string(o.value.value_.storage_.str);
    } else if (decoded == 2) {
        // double                                              // @0x171c68
        value.value_.storage_.d = o.value.value_.storage_.d;
    } else {
        // int (0) or bool (1) — both fit in first 8 bytes    // @0x171c58
        std::memcpy(&value.value_.storage_, &o.value.value_.storage_,
                     sizeof(double));
    }
}

// PlayArgs::parse @0x15d7b0 (560 bytes, ends at 0x15d9e0)
//
// Pre-scans args to find the boundary between String/Const-typed args
// (implicit channel names) and other args (explicit channel assignments).
// Accumulates hasMarker_ from any arg with VarSubType==2.
// Dispatches to parseImplicitChannels or parseExplicitChannels based on
// the first arg's VarType.
// Returns iterator past last consumed arg.
std::vector<EvalResultValue>::const_iterator
PlayArgs::parse(std::vector<EvalResultValue> const& args) {   // @0x15d7b0
    if (args.empty()) {
        // @0x15d890: throw error 0x9d
        throw CustomFunctionsException(
            ErrorMessages::format(ExpectsWaveName, cmdName_));
    }

    // @0x15d7e0..0x15d817: Pre-scan loop.
    // Scan args to find boundary: boundary tracks "one past last Wave/String".
    // Binary: rbx starts at end, updated to (current+1) for each Wave/String.
    // Also accumulate hasMarker_ from VarSubType==2 entries.
    auto it = args.begin();
    auto end = args.end();
    VarType firstType = args.front().varType_;                 // @0x15d7ec

    auto boundary = end;                                       // rbx = end initially (@0x15d7dd)
    for (auto scan = it; scan != end; ++scan) {
        if (scan->varSubType_ == static_cast<VarSubType>(2)) {
            hasMarker_ = true;                                 // @0x15d819
        }
        if (scan->varType_ == VarType_Wave || scan->varType_ == VarType_String) {
            // Update boundary to one past this element
            boundary = scan + 1;                               // @0x15d7f0: lea 0x38(%rdi),%rbx
        }
    }

    // @0x15d81d..0x15d854: Dispatch on first arg's type
    int channelCount;
    if (firstType == VarType_Wave || firstType == VarType_String) {
        // Implicit channels — waveforms specified by name
        channelCount = parseImplicitChannels(it, boundary);    // @0x16fb30
    } else {
        // Explicit channels — channel assignments specified directly
        channelCount = parseExplicitChannels(it, boundary);    // @0x170000
    }

    // @0x15d832..0x15d848: Validate channel count
    if (channelCount > static_cast<int>(totalChannels_)) {     // @0x15d83c: cmp vs +0x5a
        // @0x15d85d: throw error 0x9e — too many channels
        throw CustomFunctionsValueException(
            ErrorMessages::format(TooManyChannels,
                                  cmdName_, static_cast<int>(totalChannels_), channelCount),
            0);
    }

    return boundary;
}

// PlayArgs::parseImplicitChannels @0x16fb30 (ends at 0x16fd7d)
//
// Iterates String/Const args sequentially, calling addChannelWave for each.
// For multi-channel waveforms (signal.channels() >= 2), creates synthetic
// Cvar-typed continuation entries for channels 2..N.
// Rejects VarType {2,4,6} with error 0xEE (bitmask 0x54).
int PlayArgs::parseImplicitChannels(
    std::vector<EvalResultValue>::const_iterator begin,
    std::vector<EvalResultValue>::const_iterator end) {               // @0x16fb30
    if (begin == end)
        return 0;

    int channelIdx = 0;

    for (auto it = begin; it != end; ++it) {
        int vt = static_cast<int>(it->varType_);

        // @0x16fb7a: reject VarType {2,4,6} via bitmask 0x54
        if (vt <= 6 && ((0x54 >> vt) & 1)) {
            throw CustomFunctionsValueException(
                ErrorMessages::format(MixedChannelNumbering, cmdName_),
                static_cast<size_t>(it - begin));
        }

        // @0x16fba0: addChannelWave for this arg
        addChannelWave(channelIdx, *it);

        int nextChannel = channelIdx + 1;

        // Convert value to string for waveform lookup
        std::string waveName = it->value_.toString();

        if (it->varSubType_ != static_cast<VarSubType>(2) && !waveName.empty()) {
            // Load waveform to check channel count
            auto wf = secureLoadWaveform(waveName, static_cast<size_t>(it - begin));

            if (wf->signal.channels() >= 2) {
                // Multi-channel waveform: create synthetic continuation entries
                channelIdx += 2;
                int chIdx = 2;
                int numCh = static_cast<int>(wf->signal.channels());

                while (chIdx < numCh) {
                    // Synthetic EvalResultValue with VarType=4 (Cvar)       @0x16fc60
                    EvalResultValue syntheticArg;
                    syntheticArg.varType_ = VarType_Const;
                    syntheticArg.varSubType_ = VarSubType(0);
                    syntheticArg.reg_ = AsmRegister(-1);

                    addChannelWave(channelIdx - 1, syntheticArg);  // @0x16fca5
                    channelIdx++;
                    chIdx++;
                }
                nextChannel = channelIdx;
            }
        }

        channelIdx = nextChannel;
    }

    return channelIdx;
}

// PlayArgs::parseExplicitChannels @0x170000 (ends at 0x170774)
//
// Interleaves channel-number Cvar args with waveform (Var/Wave) args.
// Channel numbers accumulate in a local unordered_map<int, vector<uint>>
// keyed by group index. When a non-Cvar waveform arg is hit, all accumulated
// channel assignments get paired with that waveform as WaveAssignments.
int PlayArgs::parseExplicitChannels(
    std::vector<EvalResultValue>::const_iterator begin,
    std::vector<EvalResultValue>::const_iterator end) {               // @0x170000
    if (begin == end)
        return 0;

    int channelCount = 0;
    // Local: maps group_index → list of 1-based in-group channel numbers
    std::unordered_map<int, std::vector<int>> groupChannels;

    auto it = begin;
    while (it != end) {
        int vt = static_cast<int>(it->varType_);

        // @0x170068: accept VarType {2,4,6} via bitmask 0x54
        if (vt > 6 || !((0x54 >> vt) & 1)) {
            throw CustomFunctionsValueException(
                ErrorMessages::format(MixedChannelNumbering, cmdName_),
                static_cast<size_t>(it - begin));
        }

        // Inner loop: consume channel-number (Cvar) args, then one waveform arg
        while (true) {
            if (it->varType_ == VarType_Const) {                 // @0x1700c9
                if (it->varSubType_ == static_cast<VarSubType>(2)) {
                    // Marker subtype — skip
                    ++it;
                    if (it == end) break;
                    continue;
                }
                // Extract 1-based channel number                @0x1700df
                int channelNum = it->value_.toInt();

                // Validate range
                if (channelNum <= 0 || channelNum > static_cast<int>(totalChannels_)) {
                    throw CustomFunctionsValueException(
                        ErrorMessages::format(InvalidChannel,
                                              channelNum, static_cast<int>(totalChannels_)),
                        static_cast<size_t>(it - begin));
                }

                // Compute group index and in-group channel      @0x170101
                int groupIdx = (channelNum - 1) / channelsPerGroup_;
                unsigned int inGroupChannel =
                    static_cast<unsigned int>((channelNum - 1) % channelsPerGroup_ + 1);

                // Check for duplicate
                auto& vec = groupChannels[groupIdx];
                for (auto ch : vec) {
                    if (ch == channelNum) {
                        throw CustomFunctionsValueException(
                            ErrorMessages::format(DuplicateChannel, channelNum),
                            static_cast<size_t>(it - begin));
                    }
                }
                vec.push_back(static_cast<int>(inGroupChannel));

                ++it;
                if (it == end) break;
                continue;
            }

            // Non-Cvar arg → waveform reference                @0x170284
            std::string waveName = it->value_.toString();

            if (it->varSubType_ != static_cast<VarSubType>(2) && !waveName.empty()) {
                auto wf = secureLoadWaveform(waveName, static_cast<size_t>(it - begin));
                // Explicit mode rejects multi-channel waveforms   @0x170310
                if (wf->signal.channels() >= 2) {
                    throw CustomFunctionsValueException(
                        ErrorMessages::format(WaveWrongChannels,
                                              waveName, 1,
                                              static_cast<int>(wf->signal.channels())),
                        static_cast<size_t>(it - begin));
                }
            }

            // Emit WaveAssignments for all accumulated channel groups  @0x170400+
            for (auto& [groupIdx, channels] : groupChannels) {
                auto& assignments = waveAssignments_[groupIdx];

                WaveAssignment wa;
                // (varType_/varSubType_ are inside wa.value — formerly tracked
                // separately as wa.type/wa.subType before WaveAssignment's
                // layout was corrected to start with EvalResultValue at +0.)
                wa.value = *it;
                wa.bits = std::move(channels);
                assignments.push_back(std::move(wa));

                channelCount++;
            }

            groupChannels.clear();
            ++it;
            break;  // back to outer loop
        }
    }

    return channelCount;
}

// PlayArgs::secureLoadWaveform @0x1711a0 — ~0x360 bytes (ends at ~0x171500)
// Loads a waveform by name, checks for CSV duplicates (warning 0xEB),
// and calls wavetableFront_->loadWaveform().
std::shared_ptr<WaveformFront> PlayArgs::secureLoadWaveform(
    std::string const& name, size_t /*argIndex*/) const {             // @0x1711a0
    // @0x1711c0: r12 = wavetable_ (from this+0x00, i.e. PlayArgs+0x00 = WavetableFront*)
    // @0x1711ec: construct optional<string> from name, call getWaveformByName
    std::optional<std::string> optName(name);
    auto wf = wavetable_->getWaveformByName(optName);

    // @0x17121c: null check — if wf is null, fall through to throw at 0x171415
    if (!wf) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(WaveformNotFound, name), 0);
    }

    // @0x171228: check wf->hasDuplicate_ (+0xDD, "hasDuplicate")
    if (wf->hasDuplicate_) {
        // @0x171235: read CSV source name from wf->file (+0x38)
        // Build strings for warning: csvName (from file->name_) and original name
        std::string csvName;
        if (wf->file) {
            csvName = wf->file->name;
        }
        // @0x1712e7: format error 0xEB with (original name, csv name)
        std::string msg = ErrorMessages::format(
            static_cast<ErrorMessageT>(0xEB), name, csvName);
        // @0x1712ec: call reportWarning_ (std::function at PlayArgs+0x10)
        if (reportWarning_) {
            reportWarning_(msg);
        }
    }

    // @0x171344: call wavetableFront_->loadWaveform(wf)
    wavetable_->loadWaveform(wf);

    return wf;
}

// PlayArgs::getMaxSampleLength @0x15d9f0 (908 bytes, ends at 0x15dd7c)
//
// Iterates all WaveAssignments, looks up each by name via
// wavetable_->getWaveformByName(), reads the waveform's Signal::length(),
// and returns the maximum across all entries.
int64_t PlayArgs::getMaxSampleLength() const {                 // @0x15d9f0
    uint32_t maxLen = 0;

    for (auto& inner : waveAssignments_) {                     // outer stride 0x18
        for (auto& wa : inner) {                               // inner stride 0x50
            if (wa.value.varSubType_ == 2)                               // @0x15da7f: marker → break inner
                break;
            if (wa.value.varType_ == 4)                                  // @0x15da86: type 4 → skip
                continue;

            // @0x15da8e: get wave name from EvalResultValue
            std::string name = wa.value.value_.toString();
            if (name.empty())
                continue;

            // @0x15db00: lookup waveform by name
            std::optional<std::string> optName(name);
            auto waveform = wavetable_->getWaveformByName(optName);  // @0x29c180

            if (!waveform) {
                // @0x15db35: throw error 0xe9 — waveform not found
                throw CustomFunctionsValueException(
                    ErrorMessages::format(WaveformNotFound, name), 0);
            }

            if (!waveform->file && waveform->funDescrName().empty()) {
                // @0x15db49: throw error 0xea — waveform has no data
                throw CustomFunctionsValueException(
                    ErrorMessages::format(UninitializedWaveform), 0);
            }

            // @0x15db5f: read signal length at waveform+0xD0 (= signal.length_)
            uint32_t len = static_cast<uint32_t>(waveform->signal.length());
            maxLen = std::max(maxLen, len);                    // @0x15db66
        }
    }
    return static_cast<int64_t>(maxLen);
}

// PlayArgs::addChannelWave @0x170ec0
//
// Appends a WaveAssignment entry to the appropriate channel group.
// Binary computes groupIdx = channel / channelsPerGroup_, slotInGroup =
// (channel % channelsPerGroup_) + 1, then manually deep-copies the
// EvalResultValue (type-dispatched Value union copy @0x170f26..170f9f)
// and builds a single-element bits vector containing slotInGroup.
void PlayArgs::addChannelWave(int channel, EvalResultValue const& val) {  // @0x170ec0
    if (channel >= static_cast<int>(totalChannels_))                      // @0x170ed8
        return;

    int cpg         = static_cast<int>(channelsPerGroup_);                // @0x170ee6
    int groupIdx    = channel / cpg;                                      // @0x170eea..170eee
    int slotInGroup = (channel % cpg) + 1;                                // @0x170ef3..170ef6

    WaveAssignment wa;
    wa.value = val;                    // deep copy (binary does field-by-field @0x170f18..170f9f)
    wa.bits  = {slotInGroup};          // single-element vector @0x170fbb..170fda

    waveAssignments_[static_cast<size_t>(groupIdx)]
        .push_back(std::move(wa));                                        // @0x170fe5
}

// ============================================================================
// Free functions
// ============================================================================

std::optional<std::string> parseOptionalString(std::vector<EvalResultValue>& args) {  // @0x15d3e0
    // Inspect last argument: if it is a String-typed value, pop it and
    // return its string representation; otherwise return nullopt.
    if (args.empty()) return std::nullopt;

    auto& last = args.back();                                            // @0x15d3f4: r14 = args.end()
    VarType lastVarType = last.varType_;                                 // @0x15d3f8: r15 = last.varType_

    // Copy the last element's Value for potential use                    // @0x15d407: switch on which_
    Value lastVal = last.value_;

    if (lastVarType == VarType_String) {                                 // @0x15d49a: cmp r15d, 3
        args.pop_back();                                                 // @0x15d4dd: shrink end by 0x38
        return lastVal.toString();                                       // @0x15d4ec: call Value::toString
    }

    return std::nullopt;                                                 // @0x15d530: set empty
}

int getPlayRate(EvalResultValue const& val, std::string const& name, bool strict) {  // @0x163730
    if (isConstOrCvar(val.varType_)) {                                   // @0x16373f: or eax,2; cmp eax,6
        int rate = val.value_.toInt();                                    // @0x16374d: call Value::toInt
        if (strict) rate -= 2;                                           // @0x163752: lea ecx,[rax-2]; cmovne
        return rate;                                                     // @0x16375a
    }
    // Not a const/cvar: throw with error 0x9f
    throw CustomFunctionsException(                                      // @0x163770
        ErrorMessages::format(static_cast<ErrorMessageT>(0x9f), name, str(val.varType_)));
}

// ============================================================================
// NodeMap::toFrequency / toPhase / pauPoffIwrap   @0x1c5630, @0x1c5680, @0x1c5650
//
// Disasm-derived (Phase 20d):
//   toFrequency: (int64_t)(freq * 2^48 / sampleClock)
//                Constant at .rodata 0x956078 = 0x42f0000000000000 = 2^48 (double).
//                cvttsd2si rax,xmm0 → returns signed int64; bit pattern reused as
//                uint64_t for protocol encoding. Header keeps uint64_t to match
//                callers in CustomFunctions.
//   toPhase: roundf(phase * (2^23 / 360.0f)) → int32, then 23-bit two's-complement
//            wrap via the same logic as pauPoffIwrap (mask 0x3fffff for positive,
//            sign-extend with 0xffc00000 for negative). Constant at .rodata
//            0x8fd2b4 = 0x46b60b61 ≈ 23301.689f = 2^23 / 360.
//   pauPoffIwrap: 23-bit two's-complement wrap, identical wrap as toPhase tail.
// ============================================================================

namespace {
// 23-bit two's-complement wrap, used by both toPhase and pauPoffIwrap.
// Special-case: 0x400000 (sign bit alone) is left as-is (not sign-extended).
inline uint32_t wrap23(uint32_t v) {
    if (v == 0x400000u) return 0x400000u;
    if (v & 0x400000u) return v | 0xffc00000u;
    return v & 0x3fffffu;
}
} // namespace

// NodeMap::retrieve — @0x1c55d0
// Looks up path in entries_ map. Returns the NodeMapItem if found,
// or an empty NodeMapItem (data=nullptr) if not found.
NodeMapItem NodeMap::retrieve(std::string const& path) const {  // @0x1c55d0
    auto it = entries_.find(path);
    if (it != entries_.end()) {
        return it->second;
    }
    NodeMapItem empty{};
    empty.data = nullptr;
    return empty;
}

uint64_t NodeMap::toFrequency(double freq, double sampleClock) {  // @0x1c5630
    // 2^48 = 281474976710656.0
    constexpr double kTwoPow48 = 281474976710656.0;
    return static_cast<uint64_t>(static_cast<int64_t>(freq * kTwoPow48 / sampleClock));
}

int NodeMap::toPhase(float value) {  // @0x1c5680
    // 2^23 / 360 ≈ 23301.689453125f (exact bit pattern 0x46b60b61)
    constexpr float kPhaseScale = 23301.689453125f;
    int32_t scaled = static_cast<int32_t>(std::roundf(value * kPhaseScale));
    return static_cast<int>(wrap23(static_cast<uint32_t>(scaled)));
}

// ============================================================================
// parseOptionalRate (free function)   @0x163980
//
// Disasm-derived (Phase 20d). The parameter naming in the header — (begin, end,
// parseEnd, name, strict) — is misleading: in the binary the third iterator
// (rdx) is the *parse cursor* that the caller passes in (typically the result of
// PlayArgs::parse()). The function checks whether exactly ONE EvalResultValue
// (size 0x38) remains between that cursor and `end`, and if so calls
// getPlayRate() on it. Otherwise, if the cursor already equals `end` (no
// remaining args), it returns the "no-rate" sentinel computed from `strict`.
//
// Return value when no optional rate is present:
//   strict == true  →  ((1 - 1) | 5) = 5
//   strict == false →  ((0 - 1) | 5) = 0xffffffff
// (Encoded in the original via `dec eax; or eax,0x5` after eax=strict.)
//
// If the cursor + 1 element != end (i.e. extra unparsed args remain), throws
// CustomFunctionsValueException(ErrorMessages::format(0xee, name), itemIndex)
// where itemIndex = (cursor - begin) / sizeof(EvalResultValue).
// ============================================================================

int parseOptionalRate(
        std::vector<EvalResultValue>::const_iterator begin,
        std::vector<EvalResultValue>::const_iterator end,
        std::vector<EvalResultValue>::const_iterator parseEnd,
        std::string const& cmdName,
        bool strict) {
    int rate;
    auto cursor = parseEnd;
    if (static_cast<size_t>(end - cursor) == 1) {
        rate = getPlayRate(*cursor, cmdName, strict);
        ++cursor;
    } else {
        // No optional rate value present at this position. Encode "absent"
        // sentinel: strict→5, !strict→0xffffffff (matches original
        // `dec eax; or eax,0x5` on `eax=strict`).
        rate = static_cast<int>((static_cast<unsigned>(strict ? 1u : 0u) - 1u) | 5u);
    }
    if (cursor != end) {
        // Extra unparsed args remain — error out.
        size_t itemIndex = static_cast<size_t>(cursor - begin);
        throw CustomFunctionsValueException(
            ErrorMessages::format(MixedChannelNumbering, cmdName),
            itemIndex);
    }
    return rate;
}

// Not in binary — extracted from ~10 duplicate sites in custom_functions_io.cpp
// and custom_functions_playback.cpp for readability.
void CustomFunctions::checkExternalTriggeringMode(ExternalTriggeringMode expected) {
    if (externalTriggeringMode_ == ExternalTriggeringMode::None)
        externalTriggeringMode_ = expected;
    else if (externalTriggeringMode_ != expected)
        throw CustomFunctionsException(ErrorMessages::format(DioZsyncMixed));
}

// Not in binary — extracted from 3 sites in custom_functions_io.cpp.
bool CustomFunctions::isShfFamily() const {
    auto dt = config_->deviceType;
    return dt == static_cast<AwgDeviceType>(SHFLI) ||
           dt == static_cast<AwgDeviceType>(VHFLI) ||
           dt == static_cast<AwgDeviceType>(GHFLI);
}

} // namespace zhinst
