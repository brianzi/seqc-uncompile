// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CustomFunctions — implementation of 12 analyzed methods + exception classes.
//
// ~60 remaining SeqC built-in function bodies are return-nullptr stubs with addresses.
// ============================================================================

#include <boost/format.hpp>

#include "zhinst/custom_functions.hpp"
#include "zhinst/asm_commands.hpp"
#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/eval_results.hpp"
#include "zhinst/eval_result_value.hpp"
#include "zhinst/node_map_data.hpp"
#include "zhinst/resources.hpp"
#include "zhinst/types.hpp"
#include "zhinst/wavetable_front.hpp"
#include "zhinst/waveform_front.hpp"
#include "zhinst/waveform_generator.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace zhinst {

extern ErrorMessages errMsg;  // at 0x95de60

// ============================================================================
// NodeMap — forward-declared in custom_functions.hpp, defined here so that
// std::unique_ptr<NodeMap> in CustomFunctions can be instantiated in this TU.
//
// Layout (24 bytes, from ~CustomFunctions @127e44 which destroys *nodeMap_
// via __tree<__value_type<string, NodeMapItem>>::destroy on [r14+0x8] then
// `operator delete(r14, 24)`):
//   +0x00  ?     unknown header (likely vptr or first __tree member)
//   +0x08  ?     start of std::map<std::string, NodeMapItem> (3-pointer __tree)
//
// Treat as a wrapper around std::map<std::string, NodeMapItem>.
// ============================================================================
class NodeMap {
public:
    NodeMap() = default;
    ~NodeMap() = default;

    // NOTE: actual NodeMap exposes retrieve()/contains()/etc. — see node_map_data.hpp
    // Method observed: NodeMap::retrieve(string const&) const @0x1c55d0
    //   — returns std::pair<NodeMapData*, NodeMapItem-tail-fields> or similar.
    NodeMapItem retrieve(std::string const& path) const;  // @0x1c55d0

    static int toPhase(float value);  // @0x1c5680
    static uint64_t toFrequency(double freq, double sampleClock);  // @0x1c5630

    std::map<std::string, NodeMapItem> entries_;
};

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

    // 2 confirmed aliases (different string key → same method):
    reg("setSeqIndex",                  bind(&CustomFunctions::assignWaveIndex));   // alias
    reg("setReadoutRegisterAddress",    bind(&CustomFunctions::setUserReg));        // alias (target inferred from binary context)

    // NOTE: 3 more alias entries exist in binary (81 total emplaces, 78 accounted for).
    // Exact string keys and targets require deeper register tracking at 12c352, 12dfe2, 12e082.
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
    //  5. Look up name in funcMap_ (+0x60)
    //  6. If found → dispatch: return it->second(args, res)
    //  7. If not found → throw CustomFunctionsException("Unknown function: " + name)

    // Step 1-4: alias resolution / argument count validation
    auto aliasIt = aliasMap_.find(name);
    if (aliasIt != aliasMap_.end()) {
        // NOTE: aliasMap_ is empty in this binary (confirmed Phase 13c).
        // If populated, the vector size selects error format 0x37 (1 entry) or 0x38 (2 entries).
    }

    // Step 5-7: dispatch
    auto it = funcMap_.find(name);
    if (it == funcMap_.end()) {
        throw CustomFunctionsException("Unknown function: " + name);
    }
    return it->second(args, std::move(res));
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
            static_cast<ErrorMessageT>(0xF5), length, minLength);
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
            static_cast<ErrorMessageT>(0xE7), length, alignment, aligned);
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

// oscMaskCheckHirzel @0x15bab0
// Complex — searches features vector for "MF", inserts into usedFeatures_,
// switches on deviceClass/groupIndex. Returns bool.
bool CustomFunctions::oscMaskCheckHirzel(unsigned int mask) {  // @0x15bab0
    // Complex — checks AWGCompilerConfig features for "MF", then switches
    // on deviceClass/groupIndex. Binary @0x15bab0 is ~1.2KB.
    // Stub: conservative — returns false (no oscillator mask valid).
    (void)mask;
    return false;
}

// oscMaskSetAllHirzel @0x15bf50
// Computes all-oscillator mask based on deviceClass/groupIndex and "MF" feature.
unsigned int CustomFunctions::oscMaskSetAllHirzel() {  // @0x15bf50
    // Complex — ~600B function with feature lookup and device class switch.
    // Stub: conservative maximum mask.
    return 0xFFFFFFFF;
}

// oscMaskSetAllGrimsel @0x15c0b0
// Returns (1 << oscBitShift) - 1. Trivial.
unsigned int CustomFunctions::oscMaskSetAllGrimsel() {  // @0x15c0b0
    return (1u << devConst_->numDIOBits) - 1;
}

// addNodeAccess @0x15c6c0
// Inserts into accessModeMap_ (+0x128), nodeAddressMap_ (+0x100), nodeList_ (+0x150).
void CustomFunctions::addNodeAccess(NodeMapItem const& item, AccessMode mode) {  // @0x15c6c0
    accessModeMap_[item].insert(mode);
    uint32_t addr = item.hasFast ? item.fastAddr : getNodeAddress(item);
    nodeAddressMap_[item] = addr;
    // Add to nodeList_ if not already present
    bool found = false;
    for (auto const& existing : nodeList_) {
        if (existing == item) { found = true; break; }
    }
    if (!found) {
        nodeList_.push_back(item);
    }
}

// getWaitTime @0x163930
// Computes wait cycles from samples and rate.
// Device type 2: max(0, ((samples+7)<<rate+7)/8 - 3)
// Others: ((samples+3)<<rate+3)/4
int CustomFunctions::getWaitTime(int samples, int rate) {  // @0x163930
    int cl = std::max(0, rate);  // clamp rate to non-negative
    if (devConst_->deviceType == 2) {
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
    // Binary dispatches to GetNodeMap<N>::get() based on devConst_->deviceType.
    // Each template specialization constructs a different NodeMap with
    // device-specific node definitions. ~30+ template instantiations.
    // Stub: left unimplemented (requires all GetNodeMap<> specializations).
}

// getNodeAddress @0x16ba10
// Tries dynamic_cast<DirectAddrNodeMapData*>, else looks up nodeAddressMap_.
uint32_t CustomFunctions::getNodeAddress(NodeMapItem const& item) const {  // @0x16ba10
    if (auto* direct = dynamic_cast<DirectAddrNodeMapData*>(item.data)) {
        return direct->addr_;  // fast path: DirectAddrNodeMapData contains address at +0x08
    }
    // slow path: lookup in nodeAddressMap_ (+0x100)
    return nodeAddressMap_.at(item);  // throws std::out_of_range on miss
}

// getSampleClock @0x16ba80
// Reads "$DEVICE_SAMPLE_RATE" from resources_ (+0x10), fallback to devConst_->samplingRate (+0x70).
double CustomFunctions::getSampleClock() const {  // @0x16ba80
    // Binary first checks resources_ non-null, then variableExists("$DEVICE_SAMPLE_RATE").
    // If exists: reads the constant and extracts the double value.
    // Fallback: returns devConst_->samplingRate (+0x70).
    // NOTE: readConst returns void in our reconstruction (writes to internal state);
    // the actual mechanism for extracting the value is not yet fully understood.
    // For now, use the fallback path directly.
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
// Checks config_->supportedDeviceTypes (bitmask at config+0x00) against ~devType.
// If (supportedDeviceTypes & ~devType) != 0, throws CustomFunctionsException with
// the function name and device type string.
void CustomFunctions::checkFunctionSupported(std::string const& name, AwgDeviceType devType) {  // @0x15aeb0
    // @0x15aebd: rax = [this+0x00] (config_), edx = ~devType
    // @0x15aec2: test [rax+0x00], edx  → test config_->supportedDeviceTypes & ~devType
    uint32_t supported = *reinterpret_cast<uint32_t const*>(config_);  // config_+0x00 is supportedDeviceTypes bitmask
    if ((supported & ~static_cast<uint32_t>(devType)) == 0)
        return;  // @0x15aec6: early return

    // @0x15af14: esi = config_->supportedDeviceTypes (the actual bitmask value, used as AwgDeviceType enum)
    // @0x15af18: call AWGCompilerConfig::getAwgDeviceTypeString(AwgDeviceType)
    std::string devTypeStr = AWGCompilerConfig::getAwgDeviceTypeString(
        static_cast<AwgDeviceType>(supported));  // @0x15af18

    // @0x15af29: esi = 0x49, call ErrorMessages::format<string,string>(0x49, name, devTypeStr)
    std::string msg = ErrorMessages::format(
        static_cast<ErrorMessageT>(0x49), name, devTypeStr);  // @0x15af2e

    // @0x15af3d: throw CustomFunctionsException(msg)
    throw CustomFunctionsException(msg);  // @0x15af42
}

// checkWaveformMinLengthTrig @0x15b000 — 0x100 bytes (ends at 0x15b100)
// If wf is non-null and config_->trigMinWaveformLength (+0x48) > wf->sampleLength (+0xd0),
// throws with error code 0xF3.
void CustomFunctions::checkWaveformMinLengthTrig(std::shared_ptr<WaveformFront> wf) {  // @0x15b000
    if (!wf)
        return;  // @0x15b013: je early return

    // @0x15b019: rcx = [this+0x08] (devConst_), ecx = [rcx+0x48] (trigMinWaveformLength)
    // @0x15b01c: cmp ecx, [rax+0xd0] (wf->sampleLength)
    int trigMinLen = *reinterpret_cast<int const*>(
        reinterpret_cast<char const*>(devConst_) + 0x48);
    int wfLen = *reinterpret_cast<int const*>(
        reinterpret_cast<char const*>(wf.get()) + 0xd0);

    if (trigMinLen <= wfLen)
        return;  // @0x15b022: jg → falls through to throw path; jle returns

    // @0x15b043: esi = [devConst_+0x48] → to_string(trigMinLen)
    // @0x15b057: esi = 0xF3, call ErrorMessages::format<string>(0xF3, to_string(trigMinLen))
    std::string msg = ErrorMessages::format(
        static_cast<ErrorMessageT>(0xF3),
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

    // @0x15b38e: r14d = [wf+0xd0] (sampleLength)
    int wfLen = *reinterpret_cast<int const*>(
        reinterpret_cast<char const*>(wf.get()) + 0xd0);

    if (expected > wfLen) {
        // Case 1: @0x15b398 jle → skip; falls through to warn path
        // @0x15b39a: read wf->name (string at wf+0x00)
        // @0x15b430: call to_string(expected)
        // @0x15b444: esi = 0xF4, call ErrorMessages::format<string, int, string>(0xF4, wfName, wfLen, to_string(expected))
        std::string const& wfName = *reinterpret_cast<std::string const*>(wf.get());  // wf+0x00
        std::string msg = ErrorMessages::format(
            static_cast<ErrorMessageT>(0xF4), wfName, wfLen,
            std::to_string(expected));  // @0x15b44c

        // @0x15b451: warningCallback_ call via vtable+0x30
        // warningCallback_ is at this+0x190, callable ptr at this+0x1B0
        auto* cbPtr = *reinterpret_cast<void**>(
            reinterpret_cast<char*>(this) + 0x1B0);
        if (!cbPtr) std::__throw_bad_function_call();  // @0x15b550
        warningCallback_(msg);
        return;
    }

    // Case 2: check alignment
    // @0x15b3b4: r15d = devConst_->waveformPageSize ([devConst_+0x44])
    int alignment = *reinterpret_cast<int const*>(
        reinterpret_cast<char const*>(devConst_) + 0x44);

    if (wfLen % alignment == 0)
        return;  // @0x15b3c1: je exit

    // @0x15b3ca: if wfLen == 0 → roundedLen = 0
    unsigned int roundedLen = 0;
    if (wfLen != 0) {
        // @0x15b3d3: esi = wf->minSamples ([wf+0x70])
        unsigned int minSamples = *reinterpret_cast<unsigned int const*>(
            reinterpret_cast<char const*>(wf.get()) + 0x70);
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
    std::string const& wfName = *reinterpret_cast<std::string const*>(wf.get());
    // @0x15b4ec: esi = 0xE6, call ErrorMessages::format<string, int, int, unsigned>(0xE6, wfName, wfLen, alignment, roundedLen)
    std::string msg = ErrorMessages::format(
        static_cast<ErrorMessageT>(0xE6), wfName, wfLen, alignment, roundedLen);  // @0x15b4fa

    // @0x15b4ff: warningCallback_ call
    auto* cbPtr2 = *reinterpret_cast<void**>(
        reinterpret_cast<char*>(this) + 0x1B0);
    if (!cbPtr2) std::__throw_bad_function_call();  // @0x15b555
    warningCallback_(msg);
}

// optionAvailable @0x15b9c0 — 0xC3 bytes (ends at 0x15ba83)
// Searches config_->options vector (at config+0x70..config+0x78, vector<string> with 0x18 stride)
// for a matching string. If found, inserts into usedFeatures_ (+0x1C8) and returns true.
// If not found, returns false.
bool CustomFunctions::optionAvailable(std::string const& option) {  // @0x15b9c0
    // @0x15b9d4: rax = [this+0x00] (config_)
    // @0x15b9d5: r13 = [rax+0x70] (options_.begin), r14 = [rax+0x78] (options_.end)
    auto const* cfg = config_;
    auto const* optBegin = reinterpret_cast<std::string const*>(
        reinterpret_cast<char const*>(cfg) + 0x70);
    auto const* optEnd = reinterpret_cast<std::string const*>(
        reinterpret_cast<char const*>(cfg) + 0x78);

    // @0x15b9dc: if begin == end → not found
    // Linear scan comparing string lengths then memcmp @0x15ba1a
    bool found = false;
    for (auto const* it = optBegin; it != optEnd; ++it) {
        if (*it == option) {
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
        static_cast<ErrorMessageT>(0x83), path);  // @0x15c5f0

    // @0x15c5fc: construct CustomFunctionsValueException(msg, 0)
    // @0x15c61a: throw
    throw CustomFunctionsValueException(msg, 0);  // @0x15c601
}

// setWaitCyclesReg @0x15ca90 — 0x2e8 bytes (ends at 0x15cd78)
// Checks device type bitmask for supported types (0x2,0x3,0x10,0x11,0x20,0x40,0x80,0x100).
// Requires exactly 2 args (vector size == 0x70 = 2 * sizeof(EvalResultValue) where stride=0x38).
// If first arg is an AsmRegister (type==2 at arg+0x38), uses it directly.
// Otherwise, converts arg to int and allocates a new register via Resources::getRegisterNumber(),
// emits ADDI instruction to load the immediate value.
// Finally emits SUSER instruction with address 0x6F.
// Moves the EvalResults shared_ptr from res to the output (return via first arg rdi).
void CustomFunctions::setWaitCyclesReg(std::vector<EvalResultValue> const& args,
                                        std::shared_ptr<EvalResults> results,
                                        std::shared_ptr<Resources> res) {  // @0x15ca90
    // @0x15caad: rax = [this+0x00] (config_), eax = [rax+0x00] (supportedDeviceTypes)
    uint32_t devType = *reinterpret_cast<uint32_t const*>(config_);

    // @0x15cab4: ecx = devType - 2, cmp ecx, 0x3E
    // @0x15caba: bt 0x4000000040004041, ecx → checks bits 0,6,14,16,30,62
    // which correspond to devType values: 2,3,8,16,18,32,64
    // @0x15cb06: also checks devType == 0x100 and 0x80
    // If none match, skip to move-results-and-return.
    bool supported = false;
    uint32_t shifted = devType - 2;
    if (shifted <= 0x3E) {
        uint64_t mask = 0x4000000040004041ULL;
        supported = (mask >> shifted) & 1;
    }
    if (!supported) {
        if (devType == 0x100 || devType == 0x80)
            supported = true;
    }

    if (supported) {
        // @0x15cad3: check args.size() == 2 (vector byte size == 0x70)
        // sizeof(EvalResultValue) == 0x38, so 0x70 / 0x38 == 2
        if ((reinterpret_cast<char const*>(args.data() + args.size()) -
             reinterpret_cast<char const*>(args.data())) != 0x70) {
            // size mismatch → skip to move-and-return @0x15cd58
        } else {
            // @0x15cae5: construct AsmRegister(-1) as default waitReg
            AsmRegister waitReg(-1);  // @0x15cae5

            // @0x15caef: rdi = args[0] (first element)
            // @0x15caf3: check args[0].asmRegType (at arg+0x38) == 2
            auto const& firstArg = args[0];
            int argType = *reinterpret_cast<int const*>(
                reinterpret_cast<char const*>(&firstArg) + 0x38);

            if (argType == 2) {
                // @0x15caf9: rdx = [arg+0x68] → use existing register directly
                // The AsmRegister value is stored at arg+0x68 within the EvalResultValue
                waitReg = *reinterpret_cast<AsmRegister const*>(
                    reinterpret_cast<char const*>(&firstArg) + 0x68);  // @0x15cafd
            } else {
                // @0x15cb19: arg+0x40 → call Value::toInt()
                int immVal = *reinterpret_cast<int const*>(
                    reinterpret_cast<char const*>(&firstArg) + 0x40);  // simplified; real calls Value::toInt @0x15c250

                // @0x15cb25: call Resources::getRegisterNumber()
                // (This allocates a fresh register from the resource pool)
                // int regNum = res->getRegisterNumber();  // @0x1e4bb0
                AsmRegister newReg(0);  // placeholder for getRegisterNumber() result
                waitReg = newReg;

                // @0x15cb46: r12 = res.get() (from shared_ptr [rbx])
                // @0x15cb4a: r14 = asmCommands_.get() (from [this+0x50])
                // @0x15cb50: construct AsmRegister(0) as source reg
                // @0x15cb5c: construct Immediate(immVal)
                // @0x15cb76: call AsmCommands::addi(asmCommands_, waitReg, AsmRegister(0), Immediate(immVal))
                // asmCommands_->addi(waitReg, AsmRegister(0), Immediate(immVal));  // @0x273d60

                // @0x15cbad: insert resulting asm instructions into res->asmList
                // (calls vector::insert_with_size @0x1266d0)

                // @0x15cc51..15cc70: release register if not -1
                // (calls vtable dispatch on function pointer table at 0xb03dc0)
            }

            // @0x15cc7b: r14 = res.get()
            // @0x15cc82: rsi = asmCommands_.get()
            // @0x15cc89: ecx = 0x6F (address constant for SUSER instruction)
            // @0x15cc8e: call AsmCommands::suser(asmCommands_, waitReg, Address(0x6F))
            // asmCommands_->suser(waitReg, Address(0x6F));  // @0x274bc0

            // @0x15cc93..15cd48: append resulting asm to res->asmList, release temporaries
        }
    }

    // @0x15cd58: move results shared_ptr to output (return value via hidden first arg)
    // movups xmm0, [rbx]; movups [rdi], xmm0; xorps+movups zero rbx
    // This is the shared_ptr move: *outPtr = std::move(results);
}

// mergeWaveforms @0x15e060 — 2956 bytes (0x15e060..0x15eb1c)
//
// Returns a shared_ptr<WaveformFront> (via sret in rdi) representing the
// merged waveform from the provided argument values.
//
// Signature (from mangled symbol):
//   shared_ptr<WaveformFront> mergeWaveforms(
//       vector<EvalResultValue> const& args, short param2, bool param3,
//       string const& name, int param5, bool param6)
//
// Calls:
//   Value::toString() const                      @0x15de50
//   WavetableFront::getWaveformSampleLength()    @0x29c860
//   WavetableFront::getWaveformByName()          @0x29c180  (optional<string> const&)
//   WavetableFront::getWaveformByFunDescr()      @0x29c1f0
//   WavetableFront::newWaveform()                @0x29bce0
//   WaveformGenerator::grow()                    @0x260640
//   WaveformGenerator::merge()                   @0x25f5c0
//   WaveformGenerator::interleave()              @0x258140
//   WaveformGenerator::getOrCreateWaveform()     @0x25bca0
//   vector<Value>::reserve()                     @0x163e30
//   ErrorMessages::format(0xEF, string)          — empty waveform values error
//   ErrorMessages::format(0x9E, string, short, ushort) — channel count mismatch
//
// Error codes: 0xEF (empty values vector), 0x9E (channel count mismatch)
//
// NOTE: ~3KB function with 7 phases. Structural outline only.
// Full implementation requires PlayArgs and WaveformGenerator complete integration.
std::shared_ptr<WaveformFront> CustomFunctions::mergeWaveforms(
    std::vector<EvalResultValue> const& /*args*/,
    short /*deviceType*/, bool /*isSecondary*/,
    std::string const& /*name*/,
    int /*param5*/, int64_t /*maxSampleLen*/)
{  // @0x15e060
    // See conversation context for full 7-phase structural analysis:
    //   Phase 1: Collect Value objects, track max waveform length
    //   Phase 2: Empty check → throw 0xEF
    //   Phase 3: Append length Value if param5 > maxSampleLen
    //   Phase 4: Single vs multi-value dispatch
    //   Phase 5: Build "playWave"/"playWaveY" funcDescr
    //   Phase 6: getWaveformByFunDescr / getOrCreateWaveform
    //   Phase 7: Channel count validation → throw 0x9E
    throw CustomFunctionsException("mergeWaveforms: not yet reconstructed");  // stub
    return nullptr;
}

std::shared_ptr<EvalResults> CustomFunctions::play(
    std::vector<EvalResultValue> const& args,
    std::shared_ptr<Resources> res,
    SubFunc subFunc) {  // @0x15f090  (7536 bytes, ends at 0x160e00)

    // === Step 1: Build command name from SubFunc ===                @0x15f0c3
    std::string cmdName;
    switch (static_cast<int>(subFunc)) {
    case 0: cmdName = "prefetch";            break;                  // @0x15f0e0
    case 1: cmdName = "playWaveNow";         break;                  // @0x15f0f7  (was SubFunc::Now mapped wrong — binary says 0→prefetch, 1→Now)
    case 2: cmdName = "playWave";            break;                  // @0x15f115
    case 3: cmdName = "playWaveDigTrigger";  break;                  // @0x15f12c
    default:
        // @0x15f145: binary logs warning about unknown SubFunc
        cmdName = "playWave";
        break;
    }

    // === Step 2: Empty-args guard ===                              @0x15f1be
    if (args.empty()) {
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3d),
                                  cmdName, 1, static_cast<int>(args.size())));  // @0x16080d
    }

    // === Step 3: Copy args; DigTrigger extracts play-length ===    @0x15f1ce
    std::vector<EvalResultValue> argsCopy(args);                     // @0x15f225
    int playLength = 0;
    if (subFunc == SubFunc::DigTrigger) {                            // @0x15f264
        auto const& firstVal = argsCopy.front();
        // Type check: must be int-like
        if (firstVal.varType_ != VarType(4) /* VarType_Const (corrected mapping) */) {
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), cmdName));  // @0x1608e4
        }
        playLength = firstVal.value_.toInt();                        // @0x15f32b
        if (playLength < 3) {
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xd7), cmdName));  // @0x160940
        }
        // Strip first arg
        argsCopy.erase(argsCopy.begin());                            // @0x15f340
    }

    // === Step 4: Construct PlayArgs ===                            @0x15f4b6
    PlayArgs playArgs(*config_, wavetableFront_, warningCallback_,
                      cmdName, false);                               // @0x15f53c
    auto parseEnd = playArgs.parse(argsCopy);                        // @0x15f5ad
    int rate = parseOptionalRate(argsCopy.cbegin(), argsCopy.cend(),
                                 parseEnd, cmdName, false);          // @0x15f5ca

    // === Step 5: Create EvalResults ===                            @0x15f5d2
    auto results = std::make_shared<EvalResults>(VarType(1));        // @0x15f5ee
    // NOTE: The binary checks playArgs+0x1a8 for dryRun_, but we now know
    // PlayArgs is only 0x80 bytes. The +0x1a8 offset was a stack-local in
    // the original play() frame (config_->someField), NOT a PlayArgs member.
    // For now, skip the dryRun check — the normal path continues.

    // === Step 6: Per-channel loop ===                              @0x15f628
    int64_t maxSampleLen = playArgs.getMaxSampleLength();            // @0x15f62f
    int numChannels = config_->numChannelGroups;                     // +0x1c  @0x15f642
    int channelIndex = config_->deviceIndex;                         // +0x24

    std::vector<std::shared_ptr<WaveformFront>> channelWaveforms(
        static_cast<size_t>(numChannels));                           // @0x15f679

    int mask = 0x3FFF;                                               // @0x15f6bb

    for (int ch = 0; ch < numChannels; ++ch) {                      // @0x15f6e6
        auto& assignments = playArgs.waveAssignments_[ch];
        if (assignments.empty())
            continue;

        std::vector<EvalResultValue> channelArgs;
        for (auto& wa : assignments) {                               // stride 0x50  @0x15f7ac
            if (wa.type == 4 && ch == channelIndex) {
                // Marker processing: extract name, clear mask bits    @0x15f8ac
                std::string name = wa.value.value_.toString();
                if (!name.empty()) {
                    // TODO: SIMD mask-clearing loop @0x15f949
                    // Clears bits in mask based on marker name
                }
            } else {
                channelArgs.push_back(wa.value);
            }
        }

        // @0x15fa5f: merge waveforms for this channel
        if (!channelArgs.empty()) {
            channelWaveforms[ch] = mergeWaveforms(
                channelArgs,
                static_cast<short>(config_->deviceType),
                ch != channelIndex,
                cmdName, 0, maxSampleLen);                           // @0x15fa5f
        }
    }

    // === Step 7: Validation ===                                    @0x15fc27
    auto combinedWf = channelWaveforms[channelIndex];
    if (subFunc == SubFunc::DigTrigger) {
        checkWaveformMinLengthTrig(combinedWf);                      // @0x15fc8f
    }
    if (combinedWf) {
        checkOffspecWaveLength(combinedWf, 0 /* config.field_40_ */); // @0x15fcef
    }

    // === Step 8: Assembly generation ===                           @0x15fd0f
    if (subFunc != SubFunc::Default) {  // not prefetch-only
        if (combinedWf) {
            // @0x15fd35
            auto wfCopy = channelWaveforms;                          // @0x15fd57
            asmCommands_->asmPrefetch(combinedWf, channelIndex, 0,
                                      static_cast<int>(maxSampleLen));  // @0x15fe58

            // Build waveform name optionals for asmPlay
            // @0x15fe8c..0x16019f
            AsmRegister reg0(0);                                     // @0x16019f
            AsmRegister regInv(-1);                                  // @0x1601ad

            auto asmEntry = asmCommands_->asmPlay(
                std::move(wfCopy), channelIndex,
                (mask & 1) != 0,
                subFunc == SubFunc::Now,
                false, rate, 0, false,
                reg0, playLength, regInv,
                static_cast<unsigned int>(maxSampleLen));            // @0x160209

            // @0x160284: store into results
            results->assemblers_.push_back(std::move(asmEntry));
        }
    } else {
        // Prefetch-only path
        if (combinedWf) {
            asmCommands_->asmPrefetch(combinedWf, channelIndex, 0,
                                      static_cast<int>(maxSampleLen));
        }
    }

    // === Step 9: Return ===                                        @0x1607a4
    return results;
}

std::shared_ptr<EvalResults> CustomFunctions::playIndexed(
    std::vector<EvalResultValue> const& args,
    std::shared_ptr<Resources> res,
    SubFunc subFunc) {  // @0x160e00  (6428 bytes, ends at 0x16271c)

    // === Step 1: Build command name from SubFunc ===                @0x160e31
    std::string cmdName;
    switch (static_cast<int>(subFunc)) {
    case 1: cmdName = "playWaveIndexed";    break;                   // @0x160e43
    case 2: cmdName = "playAuxWaveIndexed"; break;                   // @0x160e68
    case 3: cmdName = "playWaveIndexedNow"; break;                   // @0x160e84
    default:
        // @0x160e9d: binary logs warning
        cmdName = "playWaveIndexed";
        break;
    }

    // === Step 2: Arg count guard ===
    if (args.size() < 2) {
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3d),
                                  cmdName, 2, static_cast<int>(args.size())));  // @0x162797
    }

    // === Step 3: Construct PlayArgs ===                            @0x160f16
    // indexed = true for all except Aux (subFunc 2)
    bool indexed = (subFunc != SubFunc::Aux);                        // @0x160faa
    PlayArgs playArgs(*config_, wavetableFront_, warningCallback_,
                      cmdName, indexed);                             // @0x160fd1

    playArgs.parse(args);                                            // @0x16104c

    // Extract rate from remaining args
    // TODO: the binary does additional arg count comparison against 0x6f (111)
    //       and AsmRegister extraction at @0x1610b2, @0x1610f3
    int rate = 0;  // parseOptionalRate(...)                         // @0x1611af

    // Type check on first arg — must be int-like (VarType 4 or 5?)
    auto const& firstArg = args.front();
    if (firstArg.varType_ != VarType(5) && firstArg.varType_ != VarType(4)) {
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), cmdName));  // @0x1627c5
    }

    // === Step 4: Create EvalResults and extract wave index ===     @0x1611ed
    auto results = std::make_shared<EvalResults>(VarType(1));
    int waveIndex = firstArg.value_.toInt();                         // @0x161228

    // === Step 5: Build AsmRegister and call asmTable ===           @0x16127b
    AsmRegister indexReg(waveIndex);
    int channelIndex = config_->deviceIndex;

    // Indexed play uses asmTable instead of asmPlay
    auto asmEntry = asmCommands_->asmTable(
        waveIndex, nullptr /* combinedWf */,
        channelIndex,
        false /* isHold */, false /* fourChannel */,
        rate, 0 /* suppress */, false /* isHoldMode */,
        indexReg, 0 /* regVal */);                                   // TODO: refine args

    results->assemblers_.push_back(std::move(asmEntry));

    return results;
}

// writeToNode @0x164550 — ~23KB (0x164550..0x16a300+)
//
// Uses 4 static regex objects (guard vars at b84758, b84770, b84788, b847a0):
//   absDevRegex:    matches absolute device path
//   awgNodeRegex:   matches AWG node path
//   sineNodeRegex:  matches sine/oscillator node path
//   oscselNodeRegex: matches oscillator-select node path
//
// Flow:
//   1. Extract path string from args[0], value from args[1], optional type from args[2]
//   2. Match path against static regexes via boost::regex_match
//   3. Call lookupNode(path) → NodeMapItem
//   4. Call addNodeAccess(item, AccessMode::Soft)
//   5. Get register via Resources::getRegisterNumber()
//   6. Switch on node type (6 cases) dispatching to AsmCommands::addi() or
//      AsmCommands::suser() with different address constants
//   7. Handle frequency via getSampleClock() + NodeMap::toFrequency(),
//      phase via NodeMap::toPhase(), float via cvtsd2ss
//   8. Error code: node value mismatch throws at 0x164a03
//
// This function is ~23KB of dense regex + switch dispatch code.
// Full reconstruction deferred — structural outline only.
void CustomFunctions::writeToNode(EvalResultValue /*path*/, EvalResultValue /*val*/,
                                   EvalResultValue /*type*/, std::shared_ptr<Resources> /*res*/) {  // @0x164550
    // TODO: 23KB function — structural reconstruction pending.
    // See analysis notes in conversation context for full control-flow outline.
    // Key components: 4 static boost::regex objects, lookupNode(), addNodeAccess(),
    // 6-case switch on node type, AsmCommands::addi/suser dispatch.
    throw CustomFunctionsException("writeToNode: not yet reconstructed");
}

// addSyncCommand @0x16bb30 — 608 bytes (0x16bb30..0x16bd8f)
//
// Checks device type from EvalResults[0]:
//   deviceType == 2 (Hirzel): calls AsmCommands::asmSyncHirzel() @0x279d00
//   deviceType == 1 (Cervino): calls AsmCommands::asmSyncPlaceholderCervino() @0x279b80,
//                              stores shared_ptr in results (+0x38/+0x40)
// In both cases, appends the asm entry to results->assemblers_.
void CustomFunctions::addSyncCommand(std::shared_ptr<EvalResults> results,
                                      std::shared_ptr<Resources> res) {  // @0x16bb30
    // @0x16bb5e: eax = results->values_[0].value_.toInt() — device type
    int deviceType = results->values_[0].value_.toInt();

    if (deviceType == 2) {
        // Hirzel path @0x16bb74
        // @0x16bba8: call AsmCommands::asmSyncHirzel() @0x279d00
        auto asm_entry = asmCommands_->asmSyncHirzel();  // @0x279d00
        // @0x16bc3b: push_back into results->assemblers_ (+0x18)
        results->assemblers_.push_back(std::move(asm_entry));
    } else if (deviceType == 1) {
        // Cervino path @0x16bc56
        // @0x16bc80: call AsmCommands::asmSyncPlaceholderCervino() @0x279b80
        auto asm_entry = asmCommands_->asmSyncPlaceholderCervino();  // @0x279b80
        // @0x16bcc8: store shared_ptr into results->node_ (+0x38/+0x40)
        // (The placeholder carries a shared_ptr<Node> that needs to be
        //  propagated to the results for later sync resolution.)
        results->node_ = asm_entry.node;  // @0x16bcc8
        // @0x16bd15: push_back into results->assemblers_
        results->assemblers_.push_back(std::move(asm_entry));
    }
    // @0x16bd60..0x16bd8f: move res shared_ptr into return (cleanup)
}

// printF @0x16c470 — 2370 bytes (0x16c470..0x16d380-ish)
//
// Implements printf-style string formatting using boost::basic_format.
//
// First arg must be VarType==3 (string) — used as format string.
// Remaining args fed to formatter:
//   VarType 3 (string) → string feed
//   VarType 4 or 6 (double) → checks if integer via floatEqual(val, round(val));
//     if integer: feeds as int; if not: feeds as double
//   Other types → throw error 0x46 with str(VarType)
// Empty args → throw error 0x88
// Catches boost::io::too_few_args → error 0xA6
// Catches boost::io::too_many_args → error 0xA8 (CustomFunctionsValueException)
std::string CustomFunctions::printF(std::vector<EvalResultValue> const& args,
                                     std::string const& /*funcName*/) {  // @0x16c470
    // @0x16c4a5: check args.empty()
    if (args.empty()) {
        // @0x16d1d0: throw error 0x88
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x88)));
    }

    // @0x16c4c0: first arg must be VarType==3 (string)
    auto const& fmtArg = args[0];
    // Extract format string from first argument
    std::string fmtStr = fmtArg.value_.toString();

    // @0x16c530: construct boost::basic_format from format string
    boost::format formatter(fmtStr);

    // @0x16c580..0x16cf80: iterate remaining args and feed to formatter
    try {
        for (size_t i = 1; i < args.size(); ++i) {
            auto const& arg = args[i];
            int varType = static_cast<int>(arg.varType_);

            if (varType == 3) {
                // String arg — feed directly
                // @0x16c620: string feed path
                std::string s = arg.value_.toString();
                formatter % s;
            } else if (varType == 4 || varType == 6) {
                // Numeric arg — check if integer
                // @0x16c6a0: double value path
                double val = arg.value_.toDouble();
                double rounded = std::round(val);
                // @0x16c6e0: floatEqual(val, round(val)) — checks if value is integral
                if (val == rounded && std::isfinite(val)) {
                    // Feed as integer @0x16c710
                    formatter % static_cast<long long>(val);
                } else {
                    // Feed as double @0x16c750
                    formatter % val;
                }
            } else {
                // Unknown type — throw error 0x46 @0x16cf90
                throw CustomFunctionsValueException(
                    ErrorMessages::format(static_cast<ErrorMessageT>(0x46),
                                          std::to_string(varType)),
                    i);
            }
        }

        // @0x16cfa0: return str(formatter)
        return boost::str(formatter);

    } catch (boost::io::too_few_args const&) {
        // @0x16d060: error 0xA6 — not enough arguments for format string
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xA6)));
    } catch (boost::io::too_many_args const&) {
        // @0x16d120: error 0xA8 — too many arguments for format string
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xA8)), 0);
    }
}

// addWaitCycles @0x16d8c0 — 630 bytes (0x16d8c0..0x16db36)
//
// Gets a register via Resources::getRegisterNumber(), then emits:
//   1. addi(reg, AsmRegister(0), Immediate(cycles))  — load cycle count
//   2. suser(reg, Address(0x69))                     — write to wait-cycles user register
// Both instructions are inserted into results->assemblers_.
void CustomFunctions::addWaitCycles(int cycles,
                                     std::shared_ptr<EvalResults> results,
                                     std::shared_ptr<Resources> res) {  // @0x16d8c0
    // @0x16d8f0: call Resources::getRegisterNumber() @0x1e4bb0
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    AsmRegister zero(0);

    // @0x16d930: call AsmCommands::addi(reg, AsmRegister(0), Immediate(cycles)) @0x275c70
    auto addiResult = asmCommands_->addi(reg, zero, Immediate(cycles));

    // @0x16d9a0: insert all addi results into results->assemblers_
    for (auto& asm_entry : addiResult) {
        results->assemblers_.push_back(std::move(asm_entry));  // @0x16d9e0
    }

    // @0x16da30: call AsmCommands::suser(reg, Address(0x69)) @0x277350
    // Address 0x69 is the wait-cycles user register address
    auto suserEntry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x69));

    // @0x16daa0: push_back into results->assemblers_
    results->assemblers_.push_back(std::move(suserEntry));  // @0x16dae0
    // @0x16db00..0x16db36: cleanup, move res shared_ptr
}

// writeLS64bit @0x16dbb0 — 1184 bytes (0x16dbb0..0x16e04f)
//
// Writes a 64-bit value to two user registers (low 32 bits then high 32 bits).
// Gets a scratch register via Resources::getRegisterNumber(), then emits:
//   1. addi(reg, AsmRegister(0), Immediate(value & 0xFFFFFFFF))  — load low 32
//   2. suser(reg, Address(reg1))                                 — write to reg1
//   3. addi(reg, AsmRegister(0), Immediate(value >> 32))         — load high 32
//   4. suser(reg, Address(reg2))                                 — write to reg2
void CustomFunctions::writeLS64bit(unsigned long value, int reg1, int reg2,
                                    std::shared_ptr<EvalResults> results,
                                    std::shared_ptr<Resources> res) {  // @0x16dbb0
    // @0x16dbf0: call Resources::getRegisterNumber() @0x1e4bb0
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    AsmRegister zero(0);

    // --- Low 32 bits ---
    // @0x16dc30: addi(reg, AsmRegister(0), Immediate(value & 0xFFFFFFFF))
    uint32_t lowBits = static_cast<uint32_t>(value & 0xFFFFFFFF);
    auto addiLow = asmCommands_->addi(reg, zero, Immediate(static_cast<int>(lowBits)));

    // @0x16dca0: insert into results->assemblers_
    for (auto& asm_entry : addiLow) {
        results->assemblers_.push_back(std::move(asm_entry));  // @0x16dce0
    }

    // @0x16dd20: suser(reg, Address(reg1))
    auto suserLow = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(
        static_cast<unsigned int>(reg1)));
    results->assemblers_.push_back(std::move(suserLow));  // @0x16dd90

    // --- High 32 bits ---
    // @0x16dde0: addi(reg, AsmRegister(0), Immediate(value >> 32))
    uint32_t highBits = static_cast<uint32_t>(value >> 32);
    auto addiHigh = asmCommands_->addi(reg, zero, Immediate(static_cast<int>(highBits)));

    // @0x16de50: insert into results->assemblers_
    for (auto& asm_entry : addiHigh) {
        results->assemblers_.push_back(std::move(asm_entry));  // @0x16de90
    }

    // @0x16ded0: suser(reg, Address(reg2))
    auto suserHigh = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(
        static_cast<unsigned int>(reg2)));
    results->assemblers_.push_back(std::move(suserHigh));  // @0x16df40
    // @0x16df80..0x16e04f: cleanup, move res shared_ptr
}

// generateWaveform @0x15a9f0 — 0x4C0 bytes (ends at 0x15aeb0)
// Copies args into a new vector, prepends a string EvalResultValue containing
// the waveform name, then copies the Resources shared_ptr and delegates to
// CustomFunctions::generate(newArgs, resCopy).
// On catch of CustomFunctionsValueException (personality type 1): re-throws as
//   CustomFunctionsValueException(what(), argIndex) via __cxa_throw.
// On catch of CustomFunctionsException (personality type 2): re-throws as
//   CustomFunctionsException(what()) via __cxa_throw.
std::shared_ptr<EvalResults> CustomFunctions::generateWaveform(
    std::string const& name,
    std::vector<EvalResultValue> const& args,
    std::shared_ptr<Resources> res) {  // @0x15a9f0

    // @0x15aa19: xorps → zero-init local vector<EvalResultValue> newArgs
    std::vector<EvalResultValue> newArgs;

    // @0x15aa28: r12 = args.data(), rbx = args.data() + args.size() (end ptr)
    // @0x15aa3e: r14 = end - begin (byte size)
    // @0x15aa41: if size == 0 → skip allocation
    if (!args.empty()) {
        // @0x15aa72: allocate r14 bytes, copy args into newArgs
        newArgs.assign(args.begin(), args.end());
    }

    // @0x15aaa3: Build an EvalResultValue containing `name` as a string value.
    // @0x15aaa7: DWORD [rbp-0xa8] = 4 (VarType_Const under corrected mapping —
    //            note: even though the variant payload is a std::string, the
    //            varType_ tag is Const, not String. TODO: confirm whether this
    //            is intentional or whether the surrounding code expects
    //            VarType_String here.)
    // @0x15aab4: copy name string into the EvalResultValue at rbp-0x98
    // @0x15ab04: DWORD [rbp-0xa0] = 3 (subType=Numeric), QWORD [rbp-0x78] = 3,
    //            DWORD [rbp-0x70] = type
    // This constructs an EvalResultValue with type=string containing `name`,
    // then prepends it at the front of newArgs.

    // @0x15ab70: AsmRegister(-1) placeholder
    // @0x15ab7d: call vector::insert at position 0 (prepend the name value)

    // Simplified: prepend a string-typed EvalResultValue with `name`
    // EvalResultValue nameVal;
    // nameVal.setString(name);
    // newArgs.insert(newArgs.begin(), std::move(nameVal));

    // @0x15abdd: copy res shared_ptr (increment refcount)
    // @0x15ac03: call CustomFunctions::generate(newArgs, resCopy) @0x149940
    std::shared_ptr<EvalResults> result;
    try {
        result = generate(newArgs, std::move(res));  // @0x15ac0a → @0x149940
    }
    catch (CustomFunctionsValueException& e) {
        // @0x15ad00: __cxa_begin_catch, personality type 1
        // @0x15ad41: allocate 0x40 byte exception
        // @0x15adba: construct string from e.what() (or empty string if null)
        // @0x15adca: re-throw as CustomFunctionsValueException(msg, e.argIndex)
        // The argIndex is read from e+0x20.
        throw CustomFunctionsValueException(
            e.what() ? e.what() : "",
            0 /* e.argIndex_ — offset +0x20 in caught exception */);  // @0x15adcd
    }
    catch (CustomFunctionsException& e) {
        // @0x15ad34: __cxa_begin_catch, personality type 2
        // @0x15ad73: construct string from e.what() (or empty string if null)
        // @0x15ad7f: re-throw as CustomFunctionsException(msg)
        throw CustomFunctionsException(
            e.what() ? e.what() : "");  // @0x15ad82
    }

    return result;  // @0x15ac97
}

// ============================================================================
// SeqC built-in function implementations
//
// Signature: shared_ptr<EvalResults>(vector<EvalResultValue> const&, shared_ptr<Resources>)
// Registered in funcMap_ during construction.
//
// Categories:
//   - Thin play wrappers: playWave/Now/Indexed/IndexedNow → play()/playIndexed()
//   - Thin aux wrappers:  playAuxWave/Indexed → play()/playIndexed() with different SubFunc
//   - Simple 0-arg:       waitWave, waitPlayQueueEmpty, sync, randomSeed
//   - Simple 1-arg:       setRate, setTrigger, getCnt, lock, unlock
//   - Formatting:         error, info (use printF + asmMessage)
//   - Complex multi-arg:  playZero, playHold, wait, setDIO, getDIO, etc.
// ============================================================================

// ---- Thin play() wrappers ------------------------------------------------

std::shared_ptr<EvalResults> CustomFunctions::playWave(  // @0x1352f0 (189B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWave", static_cast<AwgDeviceType>(0x1f7));
    return play(args, std::move(res), SubFunc::Default);
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveNow(  // @0x1353b0 (192B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWaveNow", static_cast<AwgDeviceType>(5));
    return play(args, std::move(res), SubFunc::Now);
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveIndexed(  // @0x135480 (182B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWaveIndexed", static_cast<AwgDeviceType>(5));
    return playIndexed(args, std::move(res), SubFunc::Default);
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveIndexedNow(  // @0x135550 (182B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWaveIndexedNow", static_cast<AwgDeviceType>(5));
    return playIndexed(args, std::move(res), SubFunc::Now);
}

// ---- Thin delegating wrappers ---

std::shared_ptr<EvalResults> CustomFunctions::playAuxWaveIndexed(  // @0x136930 (182B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playAuxWaveIndexed", static_cast<AwgDeviceType>(5));
    return playIndexed(args, std::move(res), SubFunc::Aux);
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveDigTrigger(  // @0x1386a0 (512B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWaveDigTrigger", static_cast<AwgDeviceType>(5));
    return play(args, std::move(res), SubFunc::DigTrigger);
}

// ---- Complex play wrappers (own PlayArgs construction, no play()/playIndexed() delegation) ---

std::shared_ptr<EvalResults> CustomFunctions::playAuxWave(  // @0x135610 (~5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // TODO: Complex wrapper — constructs PlayArgs inline, calls parse(),
    // parseOptionalRate(), then emits instructions directly.
    // Does NOT delegate to play().
    checkFunctionSupported("playAuxWave", static_cast<AwgDeviceType>(5));
    throw CustomFunctionsException("playAuxWave: not yet reconstructed");  // stub
}

std::shared_ptr<EvalResults> CustomFunctions::playDIOWave(  // @0x1369f0 (~3.4KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // TODO: Complex wrapper — constructs PlayArgs inline, calls parse(),
    // parseOptionalRate(), then emits instructions directly.
    // Does NOT delegate to play().
    checkFunctionSupported("playDIOWave", static_cast<AwgDeviceType>(5));
    throw CustomFunctionsException("playDIOWave: not yet reconstructed");  // stub
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveDIO(  // @0x137740 (784B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // TODO: Complex wrapper — emits via AsmCommands::wvft() directly.
    // Completely standalone, no play() delegation.
    checkFunctionSupported("playWaveDIO", static_cast<AwgDeviceType>(5));
    throw CustomFunctionsException("playWaveDIO: not yet reconstructed");  // stub
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveZSync(  // @0x137a50 (~3.2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // TODO: Complex wrapper — parses args inline, reads ZSYNC_DATA_RAW,
    // ZSYNC_DATA_PROCESSED_A/B constants. No play() delegation.
    checkFunctionSupported("playWaveZSync", static_cast<AwgDeviceType>(5));
    throw CustomFunctionsException("playWaveZSync: not yet reconstructed");  // stub
}

// ---- Simple 0-arg functions -----------------------------------------------

std::shared_ptr<EvalResults> CustomFunctions::waitWave(  // @0x13a980 (620B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("waitWave", static_cast<AwgDeviceType>(0x1f7));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), "waitWave"));
    auto result = std::make_shared<EvalResults>(static_cast<VarType>(1));
    auto asm_entry = asmCommands_->wwvf();
    result->assemblers_.push_back(std::move(asm_entry));
    return result;
}

std::shared_ptr<EvalResults> CustomFunctions::waitPlayQueueEmpty(  // @0x145240 (626B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("waitPlayQueueEmpty", static_cast<AwgDeviceType>(2));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), "waitPlayQueueEmpty"));
    auto result = std::make_shared<EvalResults>(static_cast<VarType>(1));
    auto asm_entry = asmCommands_->wwvfq();
    result->assemblers_.push_back(std::move(asm_entry));
    return result;
}

std::shared_ptr<EvalResults> CustomFunctions::sync(  // @0x14e690 (569B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("sync", static_cast<AwgDeviceType>(0x1f7));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), "sync"));
    auto result = std::make_shared<EvalResults>(static_cast<VarType>(1));
    addSyncCommand(result, std::move(res));
    return result;
}

std::shared_ptr<EvalResults> CustomFunctions::randomSeed(  // @0x1497c0 (384B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("randomSeed", static_cast<AwgDeviceType>(0x1f7));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xa9), "randomSeed"));
    // Host-side only: seeds the TLS random object. No assembly emitted.
    // Binary calls GlobalResources::random.seedRandom() — the TLS mt19937_64.
    // seedRandom() is not yet exposed in our headers.
    return std::make_shared<EvalResults>();
}

std::shared_ptr<EvalResults> CustomFunctions::now(  // @0x14cbc0 (611B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("now", static_cast<AwgDeviceType>(5));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), "now"));
    auto result = std::make_shared<EvalResults>();  // zero-init (no VarType ctor)
    AsmRegister reg(0);
    auto asm_entry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x1c));
    result->assemblers_.push_back(std::move(asm_entry));
    return result;
}

// ---- Formatting functions -------------------------------------------------

std::shared_ptr<EvalResults> CustomFunctions::error(  // @0x14d830 (536B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    auto result = std::make_shared<EvalResults>();  // zero-init
    std::string msg = printF(args, "error");
    auto asm_entry = asmCommands_->asmMessage(msg, true);
    result->assemblers_.push_back(std::move(asm_entry));
    return result;
}

std::shared_ptr<EvalResults> CustomFunctions::info(  // @0x14da50 (531B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    auto result = std::make_shared<EvalResults>();  // zero-init
    std::string msg = printF(args, "info");
    auto asm_entry = asmCommands_->asmMessage(msg, false);
    result->assemblers_.push_back(std::move(asm_entry));
    return result;
}

// ---- Simple 1-arg functions -----------------------------------------------

std::shared_ptr<EvalResults> CustomFunctions::setRate(  // @0x14c370 (933B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setRate", static_cast<AwgDeviceType>(5));
    if (args.size() != 1)
        throw CustomFunctionsException("setRate requires exactly 1 argument");  // error 0xc0
    // Binary: extract int arg, validate type, call asmRate
    //   int rate = extractArg(args[0]).toInt();
    //   auto asm = asmCommands_->asmRate(rate);
    //   result->appendAsm(asm);
    return nullptr;
}

// ---- Remaining stubs (bodies not yet reconstructed) -----------------------
// Each stub notes its binary address and size where known.

std::shared_ptr<EvalResults> CustomFunctions::setDIO(                                                                                                                       // @0x130780 (~2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // Uses waitState_ protocol instead of checkFunctionSupported
    if (waitState_ == 0) waitState_ = 1;
    else if (waitState_ != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x4f)));
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), std::string("setDIO")));

    // Check device type for high-bank DIO support
    bool supported = (config_->deviceType == static_cast<AwgDeviceType>(0x40) ||
                      config_->deviceType == static_cast<AwgDeviceType>(0x100) ||
                      config_->deviceType == static_cast<AwgDeviceType>(0x80));

    auto results = std::make_shared<EvalResults>(VarType(1));
    auto const& arg = args[0];

    if (static_cast<int>(arg.varType_) == 2) {
        // Immediate value
        AsmRegister reg(arg.value_.toInt());
        auto asmEntry = asmCommands_->sdio(reg, supported);
        results->assemblers_.push_back(std::move(asmEntry));
    } else if ((static_cast<int>(arg.varType_) & ~1) == 4) {
        // Variable — construct from immediate then sdio
        int regNum = Resources::getRegisterNumber();
        AsmRegister newReg(regNum);
        AsmRegister r0(0);
        auto addiEntries = asmCommands_->addi(newReg, r0, Immediate(arg.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto asmEntry = asmCommands_->sdio(newReg, supported);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), std::string("setDIO")));
    }
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getDIO(                                                                                                                       // @0x131040 (~1KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    if (waitState_ == 0) waitState_ = 1;
    else if (waitState_ != 1)
        throw CustomFunctionsException(ErrorMessages::format(static_cast<ErrorMessageT>(0x4f)));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), std::string("getDIO")));
    bool supported = (config_->deviceType == static_cast<AwgDeviceType>(0x40) ||
                      config_->deviceType == static_cast<AwgDeviceType>(0x100) ||
                      config_->deviceType == static_cast<AwgDeviceType>(0x80));
    auto results = std::make_shared<EvalResults>();
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    auto asmEntry = asmCommands_->ldio(reg, supported);
    results->assemblers_.push_back(std::move(asmEntry));
    results->setValue(VarType(2), regNum);
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getDIOTriggered(                                                                                                               // @0x131410 (~700B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    if (waitState_ == 0) waitState_ = 1;
    else if (waitState_ != 1)
        throw CustomFunctionsException(ErrorMessages::format(static_cast<ErrorMessageT>(0x4f)));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), std::string("getDIOTriggered")));
    auto results = std::make_shared<EvalResults>();
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    auto asmEntry = asmCommands_->ldiotrig(reg);
    results->assemblers_.push_back(std::move(asmEntry));
    results->setValue(VarType(2), regNum);
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getZSyncData(                                                                                                                // @0x1316f0 (~3KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("getZSyncData", static_cast<AwgDeviceType>(0x1fe));

    // waitState_: 0 → set to 2; must be 2 to proceed
    if (waitState_ == 0)
        waitState_ = 2;
    else if (waitState_ != 2)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x4f)));

    // Arg count check: deviceType==4 requires exactly 1 arg; others require 1-2
    auto deviceType = config_->deviceType;
    if (static_cast<int>(deviceType) == 4) {
        if (args.size() != 1)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
                    std::string("getZSyncData"), 1, args.size()));
    } else {
        if (args.size() < 1 || args.size() > 2)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x5c),
                    std::string("getZSyncData"), 1, 2, args.size()));
    }

    // Extract first arg value
    auto const& arg0 = args[0];
    int argVal = arg0.value_.toInt();

    // Try matching ZSYNC_DATA_RAW
    auto rawResult = res->readConst("ZSYNC_DATA_RAW", EDirection_Write);
    int rawVal = rawResult.value_.toInt();

    bool matched = (argVal == rawVal);
    bool matchedProcessedA = false;
    bool matchedProcessedB = false;

    if (!matched) {
        // Check if device supports ZSYNC_DATA_PROCESSED constants
        int dt = static_cast<int>(deviceType);
        bool supportsProcessed = (dt == 2 || dt == 16 || dt == 32 || dt == 64 ||
                                  dt == 0x80 || dt == 0x100);
        if (supportsProcessed) {
            // Try ZSYNC_DATA_PROCESSED_A
            auto procAResult = res->readConst("ZSYNC_DATA_PROCESSED_A", EDirection_Write);
            int procAVal = procAResult.value_.toInt();
            if (argVal == procAVal) {
                matchedProcessedA = true;
            } else {
                // Try ZSYNC_DATA_PROCESSED_B
                auto procBResult = res->readConst("ZSYNC_DATA_PROCESSED_B", EDirection_Write);
                int procBVal = procBResult.value_.toInt();
                matchedProcessedB = (argVal == procBVal);
            }
        }
    }

    if (!matched && !matchedProcessedA && !matchedProcessedB)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x75)));

    // Create results and set wait cycles register
    auto results = std::make_shared<EvalResults>(VarType(1));
    setWaitCyclesReg(args, results, res);

    // Get a register and emit the appropriate load instruction
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);

    if (static_cast<int>(deviceType) == 8) {                                  // @0x131c53
        // Special device: ld(reg, 0x6a)
        auto asmEntry = asmCommands_->ld(reg, 0x6a);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        // Re-read consts to determine which variant matched for the ld dispatch
        auto rawResult2 = res->readConst("ZSYNC_DATA_RAW", EDirection_Write);
        int rawVal2 = rawResult2.value_.toInt();
        if (argVal == rawVal2) {                                              // @0x131d33
            // ZSYNC_DATA_RAW → ldiotrig
            auto asmEntry = asmCommands_->ldiotrig(reg);
            results->assemblers_.push_back(std::move(asmEntry));
        } else {
            // Try ZSYNC_DATA_PROCESSED_A
            auto procAResult2 = res->readConst("ZSYNC_DATA_PROCESSED_A", EDirection_Write);
            int procAVal2 = procAResult2.value_.toInt();
            if (argVal == procAVal2) {                                        // @0x131e23
                auto asmEntry = asmCommands_->ld(reg, 0x6b);
                results->assemblers_.push_back(std::move(asmEntry));
            } else {
                // Must be ZSYNC_DATA_PROCESSED_B
                auto procBResult2 = res->readConst("ZSYNC_DATA_PROCESSED_B", EDirection_Write);
                int procBVal2 = procBResult2.value_.toInt();
                if (argVal == procBVal2) {                                    // @0x131f80
                    auto asmEntry = asmCommands_->ld(reg, 0x6c);
                    results->assemblers_.push_back(std::move(asmEntry));
                }
            }
        }
    }

    results->setValue(VarType(2), static_cast<int>(reg));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getFeedback(                                                                                                                 // @0x132420 (~4KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("getFeedback", static_cast<AwgDeviceType>(0x1fe));

    // waitState_: 0 → set to 2; must be 2 to proceed
    if (waitState_ == 0)
        waitState_ = 2;
    else if (waitState_ != 2)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x4f)));

    // Arg count check: deviceType==4 requires exactly 1 arg; others require 1-2
    auto deviceType = config_->deviceType;
    if (static_cast<int>(deviceType) == 4) {
        if (args.size() != 1)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x5b),
                    std::string("getFeedback"), 1, args.size()));
    } else {
        if (args.size() < 1 || args.size() > 2)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x5c),
                    std::string("getFeedback"), 1, 2, args.size()));
    }

    // Extract first arg value
    auto const& arg0 = args[0];
    int argVal = arg0.value_.toInt();

    // Try matching ZSYNC_DATA_RAW
    bool matched = false;
    {
        auto rawResult = res->readConst("ZSYNC_DATA_RAW", EDirection_Write);
        int rawVal = rawResult.value_.toInt();
        matched = (argVal == rawVal);
    }

    if (!matched) {
        // Check if device supports ZSYNC_DATA_PROCESSED constants (bitmask 0x4000000040004001)
        int dt = static_cast<int>(deviceType);
        bool supportsZSync = (dt == 2 || dt == 16 || dt == 32 || dt == 64 ||
                              dt == 0x80 || dt == 0x100);
        if (supportsZSync) {
            auto procAResult = res->readConst("ZSYNC_DATA_PROCESSED_A", EDirection_Write);
            if (argVal == procAResult.value_.toInt()) {
                matched = true;
            } else {
                auto procBResult = res->readConst("ZSYNC_DATA_PROCESSED_B", EDirection_Write);
                matched = (argVal == procBResult.value_.toInt());
            }
        }

        // Additional check for deviceType == 0x20: try QA_DATA constants    // @0x13282f
        if (!matched && dt == 0x20) {
            auto qaRawResult = res->readConst("QA_DATA_RAW", EDirection_Write);
            if (argVal == qaRawResult.value_.toInt()) {
                matched = true;
            } else {
                auto qaProcResult = res->readConst("QA_DATA_PROCESSED_D", EDirection_Write);
                matched = (argVal == qaProcResult.value_.toInt());
            }
        }
    }

    if (!matched)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x75)));

    // Create results and set wait cycles register
    auto results = std::make_shared<EvalResults>(VarType(1));
    setWaitCyclesReg(args, results, res);

    // Get a register and emit the appropriate load instruction
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);

    if (static_cast<int>(deviceType) == 8) {                                  // @0x132ad8
        auto asmEntry = asmCommands_->ld(reg, 0x6a);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        // Re-read consts to determine which variant matched for the ld dispatch
        auto rawResult2 = res->readConst("ZSYNC_DATA_RAW", EDirection_Write);
        if (argVal == rawResult2.value_.toInt()) {                            // @0x132bbb
            auto asmEntry = asmCommands_->ldiotrig(reg);
            results->assemblers_.push_back(std::move(asmEntry));
        } else {
            auto procAResult2 = res->readConst("ZSYNC_DATA_PROCESSED_A", EDirection_Write);
            if (argVal == procAResult2.value_.toInt()) {                      // @0x132cab
                auto asmEntry = asmCommands_->ld(reg, 0x6b);
                results->assemblers_.push_back(std::move(asmEntry));
            } else {
                auto procBResult2 = res->readConst("ZSYNC_DATA_PROCESSED_B", EDirection_Write);
                if (argVal == procBResult2.value_.toInt()) {                  // @0x132da0
                    auto asmEntry = asmCommands_->ld(reg, 0x6c);
                    results->assemblers_.push_back(std::move(asmEntry));
                } else {
                    // QA_DATA variants
                    auto qaRaw2 = res->readConst("QA_DATA_RAW", EDirection_Write);
                    if (argVal == qaRaw2.value_.toInt()) {                    // @0x132e91
                        auto asmEntry = asmCommands_->ld(reg, 0xc0);
                        results->assemblers_.push_back(std::move(asmEntry));
                    } else {
                        auto qaProc2 = res->readConst("QA_DATA_PROCESSED_D", EDirection_Write);
                        if (argVal == qaProc2.value_.toInt()) {               // @0x132fd5
                            auto asmEntry = asmCommands_->ld(reg, 0xc1);
                            results->addAssembler(asmEntry);                  // @0x133008 uses addAssembler
                        }
                    }
                }
            }
        }
    }

    results->setValue(VarType(2), static_cast<int>(reg));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setID(                                                                                                                        // @0x1334a0 (~2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setID", static_cast<AwgDeviceType>(0x1f7));
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), std::string("setID")));
    bool supported = (config_->deviceType == static_cast<AwgDeviceType>(0x40) ||
                      config_->deviceType == static_cast<AwgDeviceType>(0x100) ||
                      config_->deviceType == static_cast<AwgDeviceType>(0x80));
    auto results = std::make_shared<EvalResults>(VarType(1));
    auto const& arg = args[0];
    if (static_cast<int>(arg.varType_) == 2) {
        AsmRegister reg(arg.value_.toInt());
        auto asmEntry = asmCommands_->sid(reg, supported);
        results->assemblers_.push_back(std::move(asmEntry));
    } else if ((static_cast<int>(arg.varType_) & ~1) == 4) {
        int regNum = Resources::getRegisterNumber();
        AsmRegister newReg(regNum);
        auto addiEntries = asmCommands_->addi(newReg, AsmRegister(0), Immediate(arg.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto asmEntry = asmCommands_->sid(newReg, supported);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), std::string("setID")));
    }
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::assignWaveIndex(                                                                                                            // @0x133c40 (~5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("assignWaveIndex", static_cast<AwgDeviceType>(0x1f7));

    // Copy args and extract optional name string                              // @0x133c93
    std::vector<EvalResultValue> argsCopy(args.begin(), args.end());
    auto optNameVec = parseOptionalString(argsCopy);

    bool hasName = !optNameVec.empty();                                        // @0x133d39: [rbp-0x80]==1
    std::string optName;
    if (hasName) {
        optName = optNameVec[0].value_.toString();
    }

    if (hasName) {
        // Validate name against C-like identifier regex                       // @0x133d43
        // TODO: uses boost::regex with static guard at b84740. Regex validates
        // optName is a C-like identifier. Omitted — would need boost::regex include.
        // Insert name into field_168_ (unordered_set)                         // @0x133e2b
        field_168_.insert(optName);
    }

    // Build PlayArgs and parse the argument list                              // @0x133eba
    auto const& config = *config_;
    PlayArgs playArgs(config, wavetableFront_,
                      warningCallback_, std::string("assignWaveIndex"), false);
    auto parseEnd = playArgs.parse(argsCopy);

    // The element returned by parse must be a var-type (or 2 → int);          // @0x133f52
    // extract the wave index from it
    if ((static_cast<int>(parseEnd->varType_) | 0x2) != 0x6) {                 // (varType | 2) must == 6, i.e., 4 or 6
        // TODO: throw error                                                   // @0x134c7a
    }
    int waveIndex = parseEnd->value_.toInt();                                  // @0x133f64

    // parseEnd + 1 must equal end of argsCopy                                 // @0x133f73
    // (verifies that the index was the last argument)

    // Create results                                                          // @0x133f7d
    auto results = std::make_shared<EvalResults>(VarType(1));

    // getMaxSampleLength                                                      // @0x133fc7
    int64_t maxSampleLen = playArgs.getMaxSampleLength();

    // Build channel args by iterating wave assignments                        // @0x133fd6
    int channelIndex = config.channelsPerGroup[0];  // [config+0x24]
    auto const& assignments = playArgs.waveAssignments_[channelIndex];

    std::vector<EvalResultValue> channelArgs;
    uint32_t mask = 0x3fff;                                                    // r12d = 0x3fff

    for (size_t i = 0; i < assignments.size(); ++i) {                          // @0x134055
        auto const& wa = assignments[i];
        if (wa.type != 4) {                                                    // @0x13405d
            channelArgs.push_back(wa.value);
        }
        // Get waveform name from value                                        // @0x134079
        std::string waveName = wa.value.value_.toString();  // TODO: original accesses value+0x08 sub-field
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
    std::shared_ptr<WaveformFront> wf;
    if (!channelArgs.empty()) {
        short channelParam = static_cast<short>(config.channelsPerGroup[0]);   // [config+0x14]
        wf = mergeWaveforms(channelArgs, channelParam, false,
                           std::string("assignWaveIndex"), 0, maxSampleLen);
    }

    // genPlayConfig                                                           // @0x134387
    if (wf) {
        // wf->field_48_ = true;  // TODO: WaveformFront+0x48 field not yet named  // @0x1342f1

        // Check if channelArgs has >= 2 elements, get second arg name         // @0x1342f5
        bool singleChannel = true;
        if (channelArgs.size() >= 2) {
            std::string secondName = channelArgs[1].value_.toString();
            singleChannel = secondName.empty();
        }

        auto playConfig = asmCommands_->genPlayConfig(
            wf, singleChannel, true, true, false, -1, 0, false, mask);         // @0x1343bb

        // Build the config word from PlayConfig static fields                 // @0x1343c9
        // (complex bit-shifting using PlayConfig::channelsShift/Mask,
        //  rateShift/Mask, suppressShift/Mask, fourChannelShift/Mask,
        //  defaultRateShift/Mask, dummyShift/Mask, markerBitsShift/Mask,
        //  triggerShift/Mask, precompFlagShift/Mask, holdSuppressExceptSigouts)
        // The assembled word is stored at wf->currentCwvf (offset +0x68)      // @0x1344d5
        // TODO: The exact bit manipulation is complex; see disasm @0x1343c9-0x1344d5

        // wavetableFront_->assignWaveIndex(wf, waveIndex)                     // @0x134504
        wavetableFront_->assignWaveIndex(wf, waveIndex);

        // If name present: wavetableFront_->updateWave(wf, name)              // @0x13453a
        if (hasName) {
            wavetableFront_->updateWave(wf, optName);
        }

        // Set wf->field_70_ from config_->field_4C                            // @0x1345b4
        if (wf) {
            // wf->field_70_ = config_->field_4C;  // TODO: both fields not yet named
        }
    }

    // checkOffspecWaveLength                                                   // @0x1345e7
    if (wf) {
        // checkOffspecWaveLength(wf, config_->field_4C);  // TODO: field_4C not yet named
    }

    // asmLoadPlaceholder                                                      // @0x134625
    auto asmEntry = asmCommands_->asmLoadPlaceholder();

    // Copy waveform name into assembler node                                  // @0x134635
    // (copies wf->name string into the AsmList::Asm entry's assembler node)
    // Then sets up the assembler node linkage and pushes to results            // @0x13472e

    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::prefetch(                                                                                                                    // @0x1351d0 (300B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("prefetch", static_cast<AwgDeviceType>(0x2));
    return play(args, std::move(res), SubFunc::Default);                      // forwards to play() with SubFunc 0
}
std::shared_ptr<EvalResults> CustomFunctions::prefetchIndexed(                                                                                                              // @0x135290 (100B)
    std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("prefetchIndexed", static_cast<AwgDeviceType>(0x0));  // mask 0 → always unsupported
    return nullptr;  // unreachable — checkFunctionSupported throws
}

std::shared_ptr<EvalResults> CustomFunctions::playZero(                                                                                                                     // @0x1387f0 (2112B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("playZero", static_cast<AwgDeviceType>(0x1ff));
    if (args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3d),
                                  std::string("playZero"), 1, static_cast<int>(args.size())));
    if (args.size() >= 3)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x45),
                                  std::string("playZero"), 2, static_cast<int>(args.size())));

    auto results = std::make_shared<EvalResults>(VarType(1));
    int length = args[0].value_.toInt();
    length = checkPlayAlignment(length);                                         // @0x15b190

    // Optional second arg: play rate
    int rate = -1;
    if (args.size() >= 2)
        rate = getPlayRate(args[1], "playZero", false);

    // Emit asmPlay with empty waveforms, hold=false
    std::vector<std::shared_ptr<WaveformFront>> emptyWfs;
    int channelIndex = config_->deviceIndex;
    AsmRegister reg0(0);
    AsmRegister regInv(-1);
    auto asmEntry = asmCommands_->asmPlay(
        std::move(emptyWfs), channelIndex,
        false /*isHold*/, false /*fourChannel*/, false /*isBool*/,
        rate, 0x3FFF /*suppress*/, false /*isHoldMode*/,
        reg0, length, regInv, 0 /*trigger*/);
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::playHold(                                                                                                                     // @0x139030 (~1.8KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("playHold", static_cast<AwgDeviceType>(0x1f2));
    if (args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3d),
                                  std::string("playHold"), 1, static_cast<int>(args.size())));
    if (args.size() >= 3)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x45),
                                  std::string("playHold"), 2, static_cast<int>(args.size())));

    auto results = std::make_shared<EvalResults>(VarType(1));
    int length = args[0].value_.toInt();
    length = checkPlayAlignment(length);

    int rate = -1;
    if (args.size() >= 2)
        rate = getPlayRate(args[1], "playHold", false);

    // Emit asmPlay with empty waveforms, hold=true
    std::vector<std::shared_ptr<WaveformFront>> emptyWfs;
    int channelIndex = config_->deviceIndex;
    AsmRegister reg0(0);
    AsmRegister regInv(-1);
    auto asmEntry = asmCommands_->asmPlay(
        std::move(emptyWfs), channelIndex,
        true /*isHold*/, false /*fourChannel*/, false /*isBool*/,
        rate, 0x3FFF /*suppress*/, false /*isHoldMode*/,
        reg0, length, regInv, 0 /*trigger*/);
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
// @0x139760 (4640B, ends ~0x13a8a0)
std::shared_ptr<EvalResults> CustomFunctions::wait(
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x139782: args.size() == 1 (byte size 0x38)
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "wait"));       // @0x13a5d5

    // @0x139792..0x13982c: extract arg[0] into local EvalResultValue
    auto const& arg = args[0];

    // @0x13985d..0x13986e: allocate EvalResults(VarType(1))
    auto results = std::make_shared<EvalResults>(VarType(1));                        // @0x139842

    // @0x13987b: check arg type — if type == 2 (register), emit warning and return
    if (static_cast<int>(arg.varType_) == 1) {                                       // @0x13987e: cmp eax,1
        // @0x139889: warning for bool type
        auto msg = ErrorMessages::format(static_cast<ErrorMessageT>(0x36), "wait");  // @0x13989c
        if (warningCallback_)                                                        // @0x1398ac
            warningCallback_(msg);                                                   // @0x1398bf
    }

    // @0x1398e2: check varType
    auto varType = static_cast<int>(arg.varType_);

    if (varType == 2) {
        // @0x13991a: arg is a register — emit suser(reg, addr) with code 0x69
        auto asmEntry = asmCommands_->suser(arg.reg_, detail::AddressImpl<unsigned int>(0x69)); // @0x139935: suser(reg, 0x69)
        results->assemblers_.push_back(std::move(asmEntry));                         // @0x139948..0x1399ab
    } else if ((varType & ~1) == 4) {
        // @0x1399bc: numeric value — convert to double, check >= 0
        double val = arg.value_.toDouble();                                          // @0x1399c8
        if (val < 0.0)                                                               // @0x1399d0
            throw CustomFunctionsValueException(                                     // @0x13a682
                errMsg[static_cast<ErrorMessageT>(0xe2)], 0);

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
        if (devType == 0x100 || devType == 0x80)
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
            auto suserEntry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x69)); // @0x139ea0
            results->assemblers_.push_back(std::move(suserEntry));                   // @0x139ea5..0x139ec8
        } else {
            // @0x139cc9: complex path — need clock rate scaling
            double dval = arg.value_.toDouble();                                     // @0x139cd0

            // @0x139cd9: compare with DeviceConstants field_3C (sequencer clock cycles)
            double clockCycles = static_cast<double>(devConst_->field_3C);           // @0x139ce0
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
                AsmRegister zero(0);                                                 // @0x139d14

                auto erv = res->readConst("AWG_WAIT_TRIGGER",
                    static_cast<EDirection>(1));                                     // @0x139d42: readConst
                int constVal = erv.value_.toInt();                                   // @0x139d51

                auto addiEntries = asmCommands_->addi(
                    reg, zero, Immediate(constVal));                                 // @0x139d7b
                for (auto& e : addiEntries)
                    results->assemblers_.push_back(std::move(e));                    // @0x139d80..0x139dab

                // @0x13a09b: emit suser(reg, 0x1a) — load trigger value
                auto suserEntry1 = asmCommands_->suser(
                    reg, detail::AddressImpl<unsigned int>(0x1a));                    // @0x13a0ae
                results->assemblers_.push_back(std::move(suserEntry1));              // @0x13a0b3..0x13a125

                // @0x13a174: emit wtrig(reg, reg) — wait on trigger
                auto wtrigEntry = asmCommands_->wtrig(reg, reg);                     // @0x13a182
                results->assemblers_.push_back(std::move(wtrigEntry));               // @0x13a187..0x13a1bd

                // @0x13a259: second register for remaining wait count
                int regNum2 = Resources::getRegisterNumber();                        // @0x13a259
                AsmRegister reg2(regNum2);                                           // @0x13a264

                // @0x13a28d: compute remaining = arg.value_.toInt() - devConst_->field_3C
                int remaining = arg.value_.toInt() - static_cast<int>(devConst_->field_3C); // @0x13a29a
                AsmRegister zero2(0);                                                // @0x13a281
                auto addiEntries2 = asmCommands_->addi(
                    reg2, zero2, Immediate(remaining));                              // @0x13a2c5
                for (auto& e : addiEntries2)
                    results->assemblers_.push_back(std::move(e));                    // @0x13a2ca..0x13a2fb

                // @0x13a3dc: emit suser(reg2, 0x1a)
                auto suserEntry2 = asmCommands_->suser(
                    reg2, detail::AddressImpl<unsigned int>(0x1a));                   // @0x13a3ec
                results->assemblers_.push_back(std::move(suserEntry2));              // @0x13a3f1..0x13a468

                // @0x13a4b6: emit wtrig(reg2, reg2)
                auto wtrigEntry2 = asmCommands_->wtrig(reg2, reg2);                  // @0x13a4c0
                results->assemblers_.push_back(std::move(wtrigEntry2));              // @0x13a4c5..0x13a54a
            }
        }
    } else {
        // @0x13a62f: unsupported type
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "wait"));        // @0x13a652
    }

done:
    // @0x13a5c0: return results
    return results;                                                                  // @0x13a5c3..0x13a5d4
}
std::shared_ptr<EvalResults> CustomFunctions::waitTrigger(                                                                                                                   // @0x13abf0 (1734B, ends ~0x13b2b6)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // @0x13ac0d-0x13ac36: construct SSO string "waitTrigger"
    // @0x13ac49: call checkFunctionSupported("waitTrigger", AwgDeviceType(0x5))
    checkFunctionSupported("waitTrigger", static_cast<AwgDeviceType>(0x5));

    // @0x13ac4e-0x13ac5c: args.size() check — (end - begin) must equal 0x70 = 2 * sizeof(EvalResultValue)
    if (args.size() != 2)                                                                       // @0x13ac5c: jne 0x13b309 (error)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitTrigger"));             // @0x13b309-0x13b354

    // @0x13ac62-0x13acf0: extract args[0] → local EvalResultValue copy (value at rbp-0x160..rbp-0x150 region)
    // @0x13ad01-0x13adaf: extract args[1] → local EvalResultValue copy (value at rbp-0x128..rbp-0x118 region)

    // @0x13adb6-0x13add4: validate both args are integer types: (varType & ~1) == 4
    if ((static_cast<int>(args[0].varType_) & ~1) != 4 ||                                       // @0x13adbf: cmp eax, 4; jne 0x13b2b7
        (static_cast<int>(args[1].varType_) & ~1) != 4)                                         // @0x13add1: cmp eax, 4; jne 0x13b2b7
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitTrigger"));             // @0x13b2b7-0x13b304

    // @0x13adda-0x13ae06: allocate EvalResults with VarType(1)
    auto results = std::make_shared<EvalResults>(VarType(1));                                    // @0x13addf: new(0x98); @0x13ae06: EvalResults::EvalResults(VarType(1))

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
    checkFunctionSupported("waitAnaTrigger", static_cast<AwgDeviceType>(0x5));

    // @0x13b519..0x13b529: args.size() == 2 (byte size == 0x70)
    if (args.size() != 2)                                                                        // @0x13b529: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitAnaTrigger"));          // @0x13be56

    // @0x13b52f..0x13b6a2: copy both args into locals, validate both are int types
    if ((static_cast<int>(args[0].varType_) & ~1) != 4 ||                                       // @0x13b68a: and eax,0xfffffffd; cmp eax,4
        (static_cast<int>(args[1].varType_) & ~1) != 4)                                         // @0x13b69c: same check on arg[1]
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitAnaTrigger"));          // @0x13be01

    // @0x13b6a8..0x13b6d4: allocate EvalResults(VarType(1))
    auto results = std::make_shared<EvalResults>(VarType(1));                                    // @0x13b6cf

    // @0x13b6e4..0x13b79d: build two local EvalResultValues for readConst results,
    //   with AsmRegister(-1) placeholders

    // @0x13b7a2..0x13b7bd: switch on args[0].value_.toInt(): 1 → AWG_ANA_TRIGGER1, 2 → AWG_ANA_TRIGGER2
    int trigIndex = args[0].value_.toInt();                                                      // @0x13b7ae: call Value::toInt()
    EvalResultValue erv;
    if (trigIndex == 1) {                                                                        // @0x13b7bd: cmp eax,1; jne
        // @0x13b7c3..0x13b817: readConst("AWG_ANA_TRIGGER1", EDirection_Write)
        erv = res->readConst("AWG_ANA_TRIGGER1", static_cast<EDirection>(1));                    // @0x13b7e9
    } else if (trigIndex == 2) {                                                                 // @0x13b7b8: cmp eax,2; je
        // @0x13b819..0x13b86d: readConst("AWG_ANA_TRIGGER2", EDirection_Write)
        erv = res->readConst("AWG_ANA_TRIGGER2", static_cast<EDirection>(1));                    // @0x13b83f
    } else {
        // @0x13b8c6..0x13b8dc: unsupported trigger index — check deviceType == 2 and range 3..8
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitAnaTrigger"));          // @0x13bea8
    }

    // @0x13b86d..0x13b8e2: copy readConst AsmRegister into local; check args[1].toBool()

    // @0x13b8e2..0x13b929: if args[1].value_.toBool(), copy erv into second local EvalResultValue
    //   (for the second wtrig operand)

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

    // @0x13ba8e..0x13bae3: addi(reg2, AsmRegister(0), Immediate(args[1] erv value))
    //   The second register loads the second trigger const value (from args[1] path)
    AsmRegister zero2(0);                                                                        // @0x13baaa
    int trigVal2 = erv.value_.toInt();                                                           // @0x13bab6 — reuses same erv for same-trigger case
    auto addiEntries2 = asmCommands_->addi(reg2, zero2, Immediate(trigVal2));                    // @0x13bae3

    // @0x13bae8..0x13bb0f: insert addi results into results->assemblers_
    for (auto& e : addiEntries2) results->assemblers_.push_back(std::move(e));                   // @0x13bb0f

    // @0x13bbde..0x13bbfc: wtrig(reg1, reg2)
    auto wtrigEntry = asmCommands_->wtrig(reg1, reg2);                                           // @0x13bbfc

    // @0x13bc01..0x13bcb0: push_back wtrig result
    results->assemblers_.push_back(std::move(wtrigEntry));                                       // @0x13bc78

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
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitDigTrigger"));      // @0x13c1dc
        EvalResultValue arg0 = args[0];                                                          // @0x13c16f..0x13c273: copy arg[0]
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                        // @0x13c294: and eax,0xfffffffd; cmp eax,4
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitDigTrigger"));

        auto results = std::make_shared<EvalResults>(VarType(1));                                // @0x13c2a0..0x13c2cf

        int trigIdx = arg0.value_.toInt();                                                       // @0x13c2e1

        // @0x13c304..0x13c30d: check trigIdx in {1, 2} (lea eax,[rbx-1]; cmp eax,1; ja error)
        if (trigIdx < 1 || trigIdx > 2)                                                          // @0x13c307
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitDigTrigger"));      // @0x13cbef

        // @0x13c313..0x13c3ad: build "AWG_DIG_TRIGGER" + to_string(trigIdx) + "_INDEX"
        std::string constName = "AWG_DIG_TRIGGER" + std::to_string(trigIdx) + "_INDEX";          // @0x13c31d..0x13c36f
        EvalResultValue erv = res->readConst(constName, static_cast<EDirection>(1));              // @0x13c3a8

        // @0x13c423..0x13c434: asmWtrigLSPlaceholder(erv.value_.toInt())
        int constVal = erv.value_.toInt();                                                       // @0x13c3b9
        auto asmEntry = asmCommands_->asmWtrigLSPlaceholder(constVal);                           // @0x13c434
        results->assemblers_.push_back(std::move(asmEntry));                                     // @0x13c47c..0x13c4dc

        return results;                                                                          // @0x13c5bd
    } else {
        // Unsupported-device path: 2 args → readConst + addi + wtrig (like waitAnaTrigger)
        if (args.size() != 2)                                                                    // @0x13c1c6: cmp rax,0x70
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitDigTrigger"));

        EvalResultValue arg0 = args[0];                                                          // @0x13c5e9..0x13c68f: copy arg[0]
        EvalResultValue arg1 = args[1];                                                          // copy arg[1]
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                        // @0x13c6a0
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitDigTrigger"));
        if ((static_cast<int>(arg1.varType_) & ~1) != 4)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitDigTrigger"));

        auto results = std::make_shared<EvalResults>(VarType(1));

        // @0x13c6ac..0x13c706: build two local EvalResultValues with AsmRegister(-1)
        // @0x13c712..0x13c7a3: switch on arg0.toInt(): 1 → AWG_DIG_TRIGGER1, 2 → AWG_DIG_TRIGGER2
        int trigIndex = arg0.value_.toInt();                                                     // @0x13c2e1 (second path)
        EvalResultValue erv;
        if (trigIndex == 1) {                                                                    // @0x13c717
            erv = res->readConst("AWG_DIG_TRIGGER1", static_cast<EDirection>(1));                // @0x13c720..0x13c74c
        } else if (trigIndex == 2) {                                                             // @0x13c712
            erv = res->readConst("AWG_DIG_TRIGGER2", static_cast<EDirection>(1));                // @0x13c777..0x13c7a3
        } else {
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitDigTrigger"));      // @0x13cc9d
        }

        // @0x13c7cc..0x13c7da: copy readConst register into local
        // Emit addi + wtrig (same pattern as waitAnaTrigger)
        int regNum1 = Resources::getRegisterNumber();
        AsmRegister reg1(regNum1);
        AsmRegister zero1(0);
        int trigVal1 = erv.value_.toInt();
        auto addiEntries1 = asmCommands_->addi(reg1, zero1, Immediate(trigVal1));
        for (auto& e : addiEntries1) results->assemblers_.push_back(std::move(e));

        int regNum2 = Resources::getRegisterNumber();
        AsmRegister reg2(regNum2);
        AsmRegister zero2(0);
        int trigVal2 = erv.value_.toInt();
        auto addiEntries2 = asmCommands_->addi(reg2, zero2, Immediate(trigVal2));
        for (auto& e : addiEntries2) results->assemblers_.push_back(std::move(e));

        auto wtrigEntry = asmCommands_->wtrig(reg1, reg2);
        results->assemblers_.push_back(std::move(wtrigEntry));

        return results;
    }
}
std::shared_ptr<EvalResults> CustomFunctions::waitOnGrid(                                                                                                                   // @0x13d000 (900B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("waitOnGrid", static_cast<AwgDeviceType>(0x1c0));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), std::string("waitOnGrid")));
    auto results = std::make_shared<EvalResults>(VarType(1));
    // TODO: reads a resource constant for grid trigger value, then emits asmWtrigLSPlaceholder
    // For now emit placeholder with value 0
    auto asmEntry = asmCommands_->asmWtrigLSPlaceholder(0);                        // @0x13d0f0 approx
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::waitOnSync(                                                                                                                   // @0x13d3a0 (600B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("waitOnSync", static_cast<AwgDeviceType>(0x1c0));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), std::string("waitOnSync")));
    auto results = std::make_shared<EvalResults>(VarType(1));
    AsmRegister reg(0);
    auto asmEntry = asmCommands_->st(reg, detail::AddressImpl<unsigned int>(0x92));
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::waitDIOTrigger(  // @0x13d630 (1726B, ends ~0x13dcf0)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x13d64d: check waitState_ (+0x1c0)
    // If already 1 (DIO) → ok, continue.
    // If 0 → set to 1 (DIO).
    // If anything else (i.e. 2 = ZSync) → throw error 0x4f.
    if (waitState_ == 1) {                                                       // @0x13d653: cmp eax, 1; je skip
        /* already DIO — ok */
    } else if (waitState_ == 0) {                                                // @0x13d658: test eax,eax; jne error
        waitState_ = 1;                                                          // @0x13d660: mov [r14+0x1c0], 1
    } else {
        // @0x13db56: throw ErrorMessages[0x4f] — mixing DIO/ZSync wait states
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x4f)));            // @0x13db6f
    }

    // @0x13d66b: check args.empty() — args.end != args.begin means non-empty → throw 0x42
    if (!args.empty()) {                                                          // @0x13d672: jne error
        // @0x13db06: throw format(0x42, "waitDIOTrigger")
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), "waitDIOTrigger"));  // @0x13db26
    }

    // @0x13d678: allocate EvalResults(VarType(1))
    auto results = std::make_shared<EvalResults>(VarType(1));                    // @0x13d67d..0x13d6a2

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
        if (deviceType == 0x80 || deviceType == 0x100)                           // @0x13d82f..0x13d83f
            supported = true;
    }

    if (supported) {
        // === Supported device path: readConst + asmWtrigLSPlaceholder ===

        // @0x13d6d3: rsi = res.get() (from shared_ptr [r12])
        // @0x13d6d7..0x13d708: construct SSO string "AWG_MAP_TRIGGER_INDEX" (21 chars, tag=0x2a)
        //   and call res->readConst("AWG_MAP_TRIGGER_INDEX", EDirection_Write)
        EvalResultValue trigConst = res->readConst("AWG_MAP_TRIGGER_INDEX",
                                                    static_cast<EDirection>(1));  // @0x13d708: EDirection_Write=1
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

        // @0x13d872..0x13d8aa: readConst("AWG_MAP_TRIGGER", EDirection_Write)
        //   SSO string at rbp-0x60: tag=0x1e (len=15), "AWG_MAP_TRIGGER"
        EvalResultValue trigConst = res->readConst("AWG_MAP_TRIGGER",
                                                    static_cast<EDirection>(1)); // @0x13d8aa
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
    checkFunctionSupported("waitZSyncTrigger", static_cast<AwgDeviceType>(0x1fe));

    // @0x13dd40: check waitState_ (+0x1c0)
    if (waitState_ == 2) {                                                           // @0x13dd47: cmp eax, 2; je
        /* already ZSync — ok */
    } else if (waitState_ == 0) {                                                    // @0x13dd4c: test eax,eax; jne error
        waitState_ = 2;                                                              // @0x13dd54: mov [r14+0x1c0], 2
    } else {
        // @0x13e2a8: throw ErrorMessages[0x4f] — mixing DIO/ZSync wait states
        throw CustomFunctionsException(
            ErrorMessages::get(static_cast<ErrorMessageT>(0x4f)));                   // @0x13e2c1
    }

    // @0x13dd5f: check args.empty()
    if (!args.empty()) {                                                              // @0x13dd66: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), "waitZSyncTrigger")); // @0x13e278
    }

    // @0x13dd6c: allocate EvalResults(VarType(1))
    auto results = std::make_shared<EvalResults>(VarType(1));                        // @0x13dd71..0x13dd96

    // @0x13dda2: deviceType = config_->deviceType
    int deviceType = static_cast<int>(config_->deviceType);                          // @0x13dda5

    // @0x13dda7..0x13de41: device-type dispatch via jump table on (devType-2)
    // Supported devices for the direct path: 0x40, 0x80, 0x100,
    // plus jump-table cases for devType values 2..0x20
    // Two groups: some go to heap-string "AWG_ZSYNC_TRIGGER_INDEX" (23 chars),
    // others go to SSO "AWG_MAP_TRIGGER_INDEX" (21 chars).
    // Unsupported devices fall through to the addi+wtrig path.

    bool supported = false;
    bool useZSyncConst = false;

    if (deviceType <= 0x3f) {
        unsigned idx = static_cast<unsigned>(deviceType) - 2;
        if (idx <= 0x1e) {
            // Jump table at 0x958924 dispatches per device type.
            // From the binary, cases for HDAWG-family (2,3) use heap string;
            // SHF-family (8,16,18,32,64) use SSO string.
            // The specific jump table maps device codes to one of 3 targets:
            //   1) heap path (AWG_ZSYNC_TRIGGER_INDEX) — @0x13ddcc
            //   2) SSO path (AWG_MAP_TRIGGER_INDEX) — falls through to 0x13de47
            //   3) unsupported — @0x13e022
            // For simplicity, check the bitmask for supported + which constant:
            // Devices going to heap: devType in {2, 3} (HDAWG4, HDAWG8)
            // Devices going to SSO: same as waitDIOTrigger set minus HDAWG
            constexpr uint64_t supportedMask = 0x4000000040004041ULL;
            if ((supportedMask >> idx) & 1) {
                supported = true;
                // HDAWG devices (2,3) use "AWG_ZSYNC_TRIGGER_INDEX"
                if (deviceType == 2 || deviceType == 3)
                    useZSyncConst = true;
            }
        }
    } else {
        if (deviceType == 0x40 || deviceType == 0x80 || deviceType == 0x100) {       // @0x13de30..0x13de41
            supported = true;
            // These use SSO path "AWG_MAP_TRIGGER_INDEX"
        }
    }

    if (supported) {
        // === Supported device path: readConst + asmWtrigLSPlaceholder ===
        int trigValue;
        if (useZSyncConst) {
            // @0x13ddcc: heap string "AWG_ZSYNC_TRIGGER_INDEX" (23 chars)
            EvalResultValue trigConst = res->readConst("AWG_ZSYNC_TRIGGER_INDEX",
                                                        static_cast<EDirection>(1)); // @0x13de1d
            trigValue = trigConst.value_.toInt();                                     // @0x13de29
        } else {
            // @0x13de47: SSO string "AWG_MAP_TRIGGER_INDEX" (21 chars)
            EvalResultValue trigConst = res->readConst("AWG_MAP_TRIGGER_INDEX",
                                                        static_cast<EDirection>(1)); // @0x13de7c
            trigValue = trigConst.value_.toInt();                                     // @0x13de88
        }

        // @0x13dee7: call asmWtrigLSPlaceholder(trigValue)
        auto asmEntry = asmCommands_->asmWtrigLSPlaceholder(trigValue);              // @0x13dee7

        // @0x13deec..0x13e00d: store result into assemblers_ + push into results
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        // === Unsupported device path: getRegisterNumber + addi + wtrig ===

        // @0x13e022: allocate a register
        int regNum = Resources::getRegisterNumber();                                 // @0x13e022
        AsmRegister reg(regNum);                                                     // @0x13e02d

        // @0x13e032..0x13e087: readConst("AWG_MAP_TRIGGER", EDirection_Write)
        EvalResultValue trigConst = res->readConst("AWG_MAP_TRIGGER",
                                                    static_cast<EDirection>(1));     // @0x13e087
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
    checkFunctionSupported("waitCntTrigger", static_cast<AwgDeviceType>(0x2));

    // @0x13e4c5..0x13e4d1: args.size() == 1
    if (args.size() != 1) {                                                          // @0x13e4d1: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "waitCntTrigger")); // @0x13e8c2
    }

    // @0x13e4d7..0x13e4dd: config_->deviceType == 2 (HDAWG)
    if (static_cast<int>(config_->deviceType) != 2) {                                // @0x13e4dd: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "waitCntTrigger")); // @0x13e917
    }

    // @0x13e4e3..0x13e510: allocate EvalResults(VarType(1))
    auto results = std::make_shared<EvalResults>(VarType(1));

    // @0x13e520..0x13e5c5: copy arg[0] into local EvalResultValue
    EvalResultValue arg0 = args[0];                                                  // deep copy via Value switch

    // @0x13e5c5..0x13e5cb: check arg is int type ((varType & ~1) == 4)
    if ((static_cast<int>(arg0.varType_) & ~1) != 4) {                               // @0x13e5cb: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitCntTrigger")); // @0x13e96c
    }

    // @0x13e5d1..0x13e5e0: range check: arg.toInt() must be <= 1
    int counterIdx = arg0.value_.toInt();                                            // @0x13e5d8
    if (static_cast<unsigned>(counterIdx) > 1) {                                     // @0x13e5e0: ja error
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xd7),
                                  "waitCntTrigger", "either 0 or 1"));               // @0x13e9c3
    }

    // @0x13e5e6..0x13e674: build constant name dynamically
    // to_string(counterIdx) → "0" or "1"
    // insert(0, "AWG_CNT_TRIGGER", 15) → "AWG_CNT_TRIGGER0" or "AWG_CNT_TRIGGER1"
    // append("_INDEX", 6) → "AWG_CNT_TRIGGER0_INDEX" or "AWG_CNT_TRIGGER1_INDEX"
    std::string constName = std::to_string(counterIdx);                              // @0x13e5f3
    constName.insert(0, "AWG_CNT_TRIGGER", 15);                                     // @0x13e60d
    constName.append("_INDEX", 6);                                                   // @0x13e63f

    // @0x13e661..0x13e680: readConst(constName, EDirection_Write)
    EvalResultValue trigConst = res->readConst(constName,
                                                static_cast<EDirection>(1));         // @0x13e674
    int trigValue = trigConst.value_.toInt();                                         // @0x13e680

    // @0x13e6eb..0x13e6f9: asmWtrigLSPlaceholder(trigValue)
    auto asmEntry = asmCommands_->asmWtrigLSPlaceholder(trigValue);                  // @0x13e6f9
    results->assemblers_.push_back(std::move(asmEntry));

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::waitDemodOscPhase(                                                                                                               // @0x13eba0 (~3KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // @0x13ebc4..0x13ebf1: checkFunctionSupported("waitDemodOscPhase", 0x1)
    checkFunctionSupported("waitDemodOscPhase", static_cast<AwgDeviceType>(0x1));

    // @0x13ebf6..0x13ec20: check args.size() == 1 or 2
    if (args.size() == 2) {                                                                      // @0x13ec1c: cmp rax,2
        // @0x13ec26..0x13eca4: 2 args → warn via warningCallback_
        std::string warnMsg = "waitDemodOscPhase: second argument is ignored";                   // @0x13ec42..0x13ec86 (heap string, 70 bytes)
        if (warningCallback_) {                                                                  // @0x13ec91: test rdi,rdi
            warningCallback_(warnMsg);                                                           // @0x13eca4: call [rax+0x30]
        }
    } else if (args.size() != 1) {                                                               // @0x13ec16
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitDemodOscPhase"));       // @0x13f532
    }

    // @0x13ecc7..0x13edae: copy arg[0] into local, check int type
    EvalResultValue arg0 = args[0];                                                              // @0x13ecc7
    if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                            // @0x13edae: and eax,0xfffffffd; cmp eax,4
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitDemodOscPhase"));       // @0x13f4e0

    // @0x13edba..0x13ee0d: build two EvalResultValue locals with AsmRegister(-1)
    // @0x13ee12..0x13ee42: make_shared<EvalResults>(VarType(1))
    auto results = std::make_shared<EvalResults>(VarType(1));                                    // @0x13ee37

    // @0x13ee4f..0x13ee5c: switch on arg0.value_.toInt() - 1 (8-way jump table)
    int trigIdx = arg0.value_.toInt();                                                           // @0x13ee52
    EvalResultValue erv;
    switch (trigIdx) {                                                                           // @0x13ee57: dec eax; cmp eax,7; ja error
        case 1: erv = res->readConst("AWG_DEMOD_TRIGGER1", static_cast<EDirection>(1)); break;   // @0x13ee79
        case 2: erv = res->readConst("AWG_DEMOD_TRIGGER2", static_cast<EDirection>(1)); break;   // @0x13f059
        case 3: erv = res->readConst("AWG_DEMOD_TRIGGER3", static_cast<EDirection>(1)); break;   // @0x13ef39
        case 4: erv = res->readConst("AWG_DEMOD_TRIGGER4", static_cast<EDirection>(1)); break;   // @0x13ef99
        case 5: erv = res->readConst("AWG_DEMOD_TRIGGER5", static_cast<EDirection>(1)); break;   // @0x13eed9
        case 6: erv = res->readConst("AWG_DEMOD_TRIGGER6", static_cast<EDirection>(1)); break;   // @0x13f0b9
        case 7: erv = res->readConst("AWG_DEMOD_TRIGGER7", static_cast<EDirection>(1)); break;   // @0x13f116
        case 8: erv = res->readConst("AWG_DEMOD_TRIGGER8", static_cast<EDirection>(1)); break;   // @0x13eff9
        default:
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitDemodOscPhase"));   // @0x13f477
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
    checkFunctionSupported("waitSineOscPhase", static_cast<AwgDeviceType>(0x1f2));

    // @0x13f7e4..0x13f7eb: check config_->numChannelGroups >= 2 → throw 0xde
    if (config_->numChannelGroups >= 2)                                                          // @0x13f7e7: cmp [rax+0x1c],2
        throw CustomFunctionsException(
            ErrorMessages::get(static_cast<ErrorMessageT>(0xde)));                               // @0x13fdf3

    // @0x13f7f1..0x13f87a: build EvalResultValue locals, make_shared<EvalResults>(VarType(1))
    auto results = std::make_shared<EvalResults>(VarType(1));                                    // @0x13f875

    auto devType = static_cast<int>(devConst_->deviceType);                                      // @0x13f887

    bool didReadConst = false;
    bool isRegisterArg = false;
    EvalResultValue erv;

    if (devType == 2) {
        // @0x13f88f..0x13f8a5: device type 2 (HDAWG): requires 1 arg
        if (args.size() != 1)                                                                    // @0x13f8a1: cmp rax,0x38
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitSineOscPhase"));    // @0x13fe5e

        EvalResultValue arg0 = args[0];                                                          // @0x13f8af..0x13f946
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                        // @0x13f94d
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitSineOscPhase"));    // @0x13feb1

        int oscIdx = arg0.value_.toInt();                                                        // @0x13f960
        if (oscIdx == 1) {                                                                       // @0x13f968
            erv = res->readConst("AWG_DEMOD_TRIGGER1_INDEX", static_cast<EDirection>(1));        // @0x13f9f6..0x13fa4f
            didReadConst = true;
        } else if (oscIdx == 2) {                                                                // @0x13f971
            erv = res->readConst("AWG_DEMOD_TRIGGER2_INDEX", static_cast<EDirection>(1));        // @0x13f977..0x13f9d0
            didReadConst = true;
        } else if (static_cast<int>(arg0.varType_) == 2) {                                      // @0x13fac2: register type
            isRegisterArg = true;
        } else {
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "waitSineOscPhase"));    // @0x13ff52
        }
    } else {
        // @0x13fb18..0x13fb26: device types 0x10 or 0x20 (SHF family) with 0 args
        if (devType == 0x10 || devType == 0x20) {                                                // @0x13fb1b,0x13fb1d
            if (args.empty()) {                                                                  // @0x13fb2b: cmp rax,[r12]
                erv = res->readConst("AWG_DEMOD_TRIGGER1_INDEX", static_cast<EDirection>(1));    // @0x13fb35..0x13fb91
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
    checkFunctionSupported("waitTimestamp", static_cast<AwgDeviceType>(0x2));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), std::string("waitTimestamp")));
    auto results = std::make_shared<EvalResults>();
    AsmRegister reg(0);
    auto asmEntry = asmCommands_->st(reg, detail::AddressImpl<unsigned int>(0x1b));  // @0x14028a
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
// @0x1403b0 (~6.5KB, ends ~0x141a00)
std::shared_ptr<EvalResults> CustomFunctions::resetOscPhase(
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // @0x1403d0..0x14040f: checkFunctionSupported("resetOscPhase", 0x1fe)
    checkFunctionSupported("resetOscPhase", static_cast<AwgDeviceType>(0x1fe));      // @0x14040f

    // @0x140414: get device type
    auto devType = static_cast<int>(devConst_->deviceType);                          // @0x140418

    // Device-dependent handling
    if (devType == 0x40 || devType == 0x80 || devType == 0x100) {
        // @0x14052f: SHFQA/SHFSG/SHFQC path
        // Check args count
        if (args.size() >= 2) {
            // @0x141811: too many args
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "resetOscPhase"));
        }
        if (args.size() == 1) {
            // @0x140560: extract arg[0] value
            auto const& arg = args[0];
            if ((static_cast<int>(arg.varType_) & ~1) != 4)                          // @0x140a2b
                throw CustomFunctionsException(
                    ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "resetOscPhase"));

            int oscMask = arg.value_.toInt();                                        // @0x140a3b
            // @0x140a47: validate mask against devConst numDIOBits (osc bit width)
            if ((oscMask >> devConst_->numDIOBits) != 0)                             // @0x140a53
                throw CustomFunctionsException(
                    ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "resetOscPhase"));

            // @0x140a8d: allocate register, build addi + st + suser sequence
            int regNum = Resources::getRegisterNumber();                             // @0x140a8d
            AsmRegister reg(regNum);                                                 // @0x140a98

            auto results = std::make_shared<EvalResults>();                           // @0x140aa2

            // @0x140b1d..0x140b46: addi(reg, R0, Immediate(oscMask))
            AsmRegister zero(0);                                                     // @0x140b18
            auto addiEntries = asmCommands_->addi(reg, zero, Immediate(oscMask));    // @0x140b46
            for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));// @0x140b4b..0x140b81

            // @0x140c55..0x140c77: st(reg, addr) — address depends on deviceType==8
            int stAddr = (devConst_->deviceType == 8) ? 0x7a : 0x78;                // @0x140c5a: cmp [rax],8; sete
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
                auto suserEntry = asmCommands_->suser(
                    reg2, detail::AddressImpl<unsigned int>(0x69));                   // @0x140ed0
                results->assemblers_.push_back(std::move(suserEntry));               // @0x140ed5..0x140f5d
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

            int stAddr = (devConst_->deviceType == 8) ? 0x7a : 0x78;
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
                auto suserEntry = asmCommands_->suser(
                    reg2, detail::AddressImpl<unsigned int>(0x69));
                results->assemblers_.push_back(std::move(suserEntry));
            }

            return results;
        }
    } else if (devType >= 2 && devType <= 0x20) {
        // @0x14044e: Hirzel device types (HDAWG, UHFQA, UHFLI, etc.)
        if (args.size() >= 2) {
            // @0x1418e3: too many args
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "resetOscPhase"));
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
                    ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "resetOscPhase"));

            oscMask = arg.value_.toInt();                                            // @0x141051
            if (!oscMaskCheckHirzel(oscMask))                                        // @0x14105c
                throw CustomFunctionsException(
                    ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "resetOscPhase"));
        }

        // @0x14060b..0x140628: allocate register, create EvalResults
        // @0x1405c9: 0 args → special path with st only
        if (args.empty()) {
            auto results = std::make_shared<EvalResults>();                           // @0x1405ce

            int regNum = Resources::getRegisterNumber();                             // @0x140628
            AsmRegister reg(regNum);                                                 // @0x140633
            AsmRegister zero(0);                                                     // @0x140649
            auto addiEntries = asmCommands_->addi(reg, zero, Immediate(1));          // @0x140679
            for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));// @0x14067e..0x1406b3

            // @0x140849..0x14085d: st(reg, 0x5f) — store to osc reset register
            auto stEntry = asmCommands_->st(reg, detail::AddressImpl<unsigned int>(0x5f)); // @0x14085d
            results->assemblers_.push_back(std::move(stEntry));                      // @0x140862..0x14091b

            // @0x14093f..0x14095c: st(R0, 0x5f) — clear
            AsmRegister r0(0);                                                       // @0x140941
            auto stEntry2 = asmCommands_->st(r0, detail::AddressImpl<unsigned int>(0x5f)); // @0x14095c
            results->assemblers_.push_back(std::move(stEntry2));                     // @0x140961..0x1409e5

            // @0x140d4e: if numAWGCores > 0, emit additional suser
            if (devConst_->numAWGCores > 0) {
                int regNum2 = Resources::getRegisterNumber();
                AsmRegister reg2(regNum2);
                AsmRegister zero2(0);
                auto addiEntries2 = asmCommands_->addi(
                    reg2, zero2, Immediate(static_cast<int>(devConst_->numAWGCores)));
                for (auto& e : addiEntries2)
                    results->assemblers_.push_back(std::move(e));
                auto suserEntry = asmCommands_->suser(
                    reg2, detail::AddressImpl<unsigned int>(0x69));
                results->assemblers_.push_back(std::move(suserEntry));
            }

            return results;
        } else {
            // 1 arg — with validated oscMask
            auto results = std::make_shared<EvalResults>();

            int regNum = Resources::getRegisterNumber();
            AsmRegister reg(regNum);
            AsmRegister zero(0);
            auto addiEntries = asmCommands_->addi(reg, zero, Immediate(oscMask));
            for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

            auto stEntry = asmCommands_->st(reg, detail::AddressImpl<unsigned int>(0x5f));
            results->assemblers_.push_back(std::move(stEntry));

            AsmRegister r0(0);
            auto stEntry2 = asmCommands_->st(r0, detail::AddressImpl<unsigned int>(0x5f));
            results->assemblers_.push_back(std::move(stEntry2));

            if (devConst_->numAWGCores > 0) {
                int regNum2 = Resources::getRegisterNumber();
                AsmRegister reg2(regNum2);
                AsmRegister zero2(0);
                auto addiEntries2 = asmCommands_->addi(
                    reg2, zero2, Immediate(static_cast<int>(devConst_->numAWGCores)));
                for (auto& e : addiEntries2)
                    results->assemblers_.push_back(std::move(e));
                auto suserEntry = asmCommands_->suser(
                    reg2, detail::AddressImpl<unsigned int>(0x69));
                results->assemblers_.push_back(std::move(suserEntry));
            }

            return results;
        }
    } else {
        // @0x141964: unsupported device type
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "resetOscPhase"));
    }
}
// @0x141df0 (~4KB, ends ~0x142d50)
std::shared_ptr<EvalResults> CustomFunctions::setSinePhase(
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // @0x141e11..0x141e49: checkFunctionSupported("setSinePhase", 0x1f2)
    checkFunctionSupported("setSinePhase", static_cast<AwgDeviceType>(0x1f2));       // @0x141e49

    // @0x141e4e..0x141e7a: allocate EvalResults(VarType(1))
    auto results = std::make_shared<EvalResults>(VarType(1));                        // @0x141e53

    // @0x141e8a: get device type
    auto devType = static_cast<int>(devConst_->deviceType);

    // Phase 1: handle deviceType == 2 (HDAWG) — 2-arg path
    if (devType == 2) {                                                              // @0x141e8c: cmp eax,2
        // @0x141e99: need 2 args
        if (args.size() != 2)                                                        // @0x141eae: cmp rax,0x70
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xd1), "setSinePhase")); // @0x142a8b

        auto const& arg0 = args[0];  // oscillator index
        auto const& arg1 = args[1];  // phase value

        // @0x142018: validate both args are integer/numeric types
        if ((static_cast<int>(arg0.varType_) & ~1) != 4 ||                           // @0x142021
            (static_cast<int>(arg1.varType_) & ~1) != 4)                             // @0x142033
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), "setSinePhase")); // @0x142942

        // @0x14203d: validate oscillator index 0 or 1
        int oscIndex = arg0.value_.toInt();                                          // @0x142043
        if (oscIndex < 0 || oscIndex >= 2)                                           // @0x142047, 0x142058
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xd3), "setSinePhase")); // @0x142995

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
            auto asmEntry = asmCommands_->suser(
                reg, detail::AddressImpl<unsigned int>(0x70));                        // @0x14223d
            results->assemblers_.push_back(std::move(asmEntry));                     // @0x142242..0x142265
        } else {
            // @0x1421e6: suser(reg, 0x71)
            auto asmEntry = asmCommands_->suser(
                reg, detail::AddressImpl<unsigned int>(0x71));                        // @0x1421fa
            results->assemblers_.push_back(std::move(asmEntry));                     // @0x1421ff..0x142227
        }

        // @0x14232d..0x142369: compute node index = field_20 * field_1C + field_24
        int nodeIdx = devConst_->field_20 * devConst_->maxBlocks + devConst_->field_24; // @0x14232d

        // @0x142369..0x1426ef: add oscIndex*2 to nodeIdx, then lookup/access nodes
        nodeIdx = oscIndex + nodeIdx * 2;                                            // @0x142369: lea r12d,[r12+rbx*2]
    }

    // Phase 2: handle deviceType == 0x20 or 0x10 (SHFSG, SHFQA)
    if (devType == 0x20 || devType == 0x10) {                                        // @0x14239b, 0x1423a0
        // @0x1423a6: need 1 arg (phase only, no osc index)
        if (args.size() != 1)                                                        // @0x1423bb: cmp rax,0x38
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xd2), "setSinePhase")); // @0x1429e8

        auto const& arg = args[0];

        // @0x14244a: validate arg type
        if ((static_cast<int>(arg.varType_) & ~1) != 4)                              // @0x14244a
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), "setSinePhase")); // @0x142a3b

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
        auto asmEntry = asmCommands_->suser(
            reg, detail::AddressImpl<unsigned int>(0x70));                            // @0x1425ed
        results->assemblers_.push_back(std::move(asmEntry));                         // @0x1425f2..0x14266d

        // @0x1426bf: node lookup and access
        int nodeOffset = devConst_->field_20;                                        // @0x1426c2
    }

    // Phase 3: for deviceType == 2 — node path construction and lookup
    if (devType == 2) {                                                              // @0x1426f1
        // @0x1426fa..0x142750: build path "/oscs/" + to_string(nodeIdx) + "/phaseshift"
        auto path = "/oscs/" + std::to_string(static_cast<unsigned long>(0)) + "/phaseshift"; // simplified
        auto node = lookupNode(path);                                                // @0x1427bd
        addNodeAccess(node, AccessMode::Custom);                                      // @0x1427ce
    }

    // Phase 4: for deviceType == 0x20 or 0x10 — different node path
    if (devType == 0x20 || devType == 0x10) {                                        // @0x14280f
        // @0x14281d..0x142873: build path "/sines/" + to_string(nodeOffset) + "/phaseshift/value"
        auto path = "/sines/" + std::to_string(static_cast<unsigned long>(0)) + "/phaseshift/value"; // simplified
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
    checkFunctionSupported("incrementSinePhase", static_cast<AwgDeviceType>(0x1f2)); // @0x142df5

    // @0x142dfa..0x142e26: allocate EvalResults(VarType(1))
    auto results = std::make_shared<EvalResults>(VarType(1));                        // @0x142dff

    // @0x142e36: get device type
    auto devType = static_cast<int>(devConst_->deviceType);

    // Phase 1: handle deviceType == 2 (HDAWG) — 2-arg path
    if (devType == 2) {                                                              // @0x142e38: cmp eax,2
        // @0x142e45: need 2 args
        if (args.size() != 2)                                                        // @0x142e5a: cmp rax,0x70
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xd1), "incrementSinePhase")); // @0x143a3b

        auto const& arg0 = args[0];  // oscillator index
        auto const& arg1 = args[1];  // phase increment

        // Validate both args are integer/numeric types
        if ((static_cast<int>(arg0.varType_) & ~1) != 4 ||
            (static_cast<int>(arg1.varType_) & ~1) != 4)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), "incrementSinePhase")); // @0x1438f2

        // Validate oscillator index 0 or 1
        int oscIndex = arg0.value_.toInt();
        if (oscIndex < 0 || oscIndex >= 2)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xd3), "incrementSinePhase")); // @0x143945

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
            auto asmEntry = asmCommands_->suser(
                reg, detail::AddressImpl<unsigned int>(0x72));                        // @0x1431e8
            results->assemblers_.push_back(std::move(asmEntry));
        } else {
            auto asmEntry = asmCommands_->suser(
                reg, detail::AddressImpl<unsigned int>(0x73));                        // @0x1431a5
            results->assemblers_.push_back(std::move(asmEntry));
        }

        // Compute node index
        int nodeIdx = devConst_->field_20 * devConst_->maxBlocks + devConst_->field_24;
        nodeIdx = oscIndex + nodeIdx * 2;
    }

    // Phase 2: handle deviceType == 0x20 or 0x10 (SHFSG, SHFQA)
    if (devType == 0x20 || devType == 0x10) {
        // Need 1 arg
        if (args.size() != 1)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xd2), "incrementSinePhase"));

        auto const& arg = args[0];

        if ((static_cast<int>(arg.varType_) & ~1) != 4)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), "incrementSinePhase"));

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
        auto asmEntry = asmCommands_->suser(
            reg, detail::AddressImpl<unsigned int>(0x72));                            // @0x143598
        results->assemblers_.push_back(std::move(asmEntry));

        int nodeOffset = devConst_->field_20;
    }

    // Phase 3: for deviceType == 2 — node path construction and lookup
    if (devType == 2) {
        auto path = "/oscs/" + std::to_string(static_cast<unsigned long>(0)) + "/phaseshift";
        auto node = lookupNode(path);
        addNodeAccess(node, AccessMode::Custom);
    }

    // Phase 4: for deviceType == 0x20 or 0x10
    if (devType == 0x20 || devType == 0x10) {
        auto path = "/sines/" + std::to_string(static_cast<unsigned long>(0)) + "/phaseshift/value";
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
    checkFunctionSupported("waitDemodSample", static_cast<AwgDeviceType>(0x1));      // @0x143dba

    // @0x143dbf: args.size() == 1 (byte size 0x38)
    if (args.size() != 1)                                                            // @0x143dcd
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "waitDemodSample")); // @0x144f15

    // @0x143dd3..0x143e58: extract arg[0]
    auto const& arg = args[0];

    // @0x143e74: validate arg type
    if ((static_cast<int>(arg.varType_) & ~1) != 4)                                  // @0x143e7a
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "waitDemodSample")); // @0x144f67

    // @0x143e80..0x143eba: build EvalResultValue locals for readConst results
    // Setup: two local EvalResultValues with default values
    // varType_ = 1, value_ = 0, address_ = 4, etc.

    // @0x143ecd: allocate register
    int regNum = Resources::getRegisterNumber();                                     // @0x143ecd (inlined from path)

    // @0x143f1d: switch on arg value (demod trigger index 1-8)
    int trigIdx = arg.value_.toInt();

    // @0x143f20: validate range — must be 0..7
    if (trigIdx < 0 || trigIdx > 7)                                                  // @0x143f20: ja 0x144715
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "waitDemodSample"));

    // @0x143f26..0x1446d0: switch table — 8 cases, each does readConst for the corresponding trigger
    EvalResultValue erv;
    static const char* trigNames[] = {
        "AWG_DEMODRATE_TRIGGER1", "AWG_DEMODRATE_TRIGGER2",
        "AWG_DEMODRATE_TRIGGER3", "AWG_DEMODRATE_TRIGGER4",
        "AWG_DEMODRATE_TRIGGER5", "AWG_DEMODRATE_TRIGGER6",
        "AWG_DEMODRATE_TRIGGER7", "AWG_DEMODRATE_TRIGGER8"
    };

    // Each case at @0x143f3d, @0x14402d, @0x14411d, @0x14420d, @0x1442fd, @0x1443ed, @0x1444dd, @0x1445cd
    // calls: res->readConst(trigNames[trigIdx], EDirection(1))
    erv = res->readConst(trigNames[trigIdx], static_cast<EDirection>(1));             // @0x143f72..0x144602

    // @0x1446d0..0x144715: extract readConst result value

    // @0x1447fd: build assembly — allocate registers and emit addi + wtrig
    auto results = std::make_shared<EvalResults>(VarType(1));

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

    // @0x1449de: addi for second register (trigger address from arg)
    int argVal = arg.value_.toInt();
    auto addiEntries2 = asmCommands_->addi(reg2, zero2, Immediate(argVal));          // @0x1449de
    for (auto& e : addiEntries2) results->assemblers_.push_back(std::move(e));

    // @0x144aef: third register for combined value
    int regNum3 = Resources::getRegisterNumber();                                    // @0x144aef
    AsmRegister reg3(regNum3);                                                       // @0x144b19
    AsmRegister zero3(0);                                                            // @0x144b36 (approx)

    // @0x144b52: addi for third register
    auto addiEntries3 = asmCommands_->addi(reg3, zero3, Immediate(trigConst));       // @0x144b52
    for (auto& e : addiEntries3) results->assemblers_.push_back(std::move(e));

    // @0x144c7b: wtrig(reg1, reg2) — wait for demod trigger
    auto wtrigEntry = asmCommands_->wtrig(reg1, reg2);                               // @0x144c7b
    results->assemblers_.push_back(std::move(wtrigEntry));

    // @0x144d77: wtrig(reg3, reg3) — second wait
    auto wtrigEntry2 = asmCommands_->wtrig(reg3, reg3);                              // @0x144d77
    results->assemblers_.push_back(std::move(wtrigEntry2));

    // TODO: @0x144e00..0x144f00 — additional device-specific node lookup/access paths
    // for various device types (HDAWG etc.), similar to setSinePhase pattern.
    // Full reconstruction deferred; address range 0x144e00..0x145200.

    return results;                                                                  // @0x144efc
}
std::shared_ptr<EvalResults> CustomFunctions::setTrigger(                                                                                                                   // @0x1454c0 (1552B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setTrigger", static_cast<AwgDeviceType>(0x3f));
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xcf), std::string("setTrigger")));
    auto results = std::make_shared<EvalResults>(VarType(1));
    auto const& arg = args[0];
    if (static_cast<int>(arg.varType_) == 2) {
        AsmRegister reg(arg.value_.toInt());
        auto asmEntry = asmCommands_->strig(reg);
        results->assemblers_.push_back(std::move(asmEntry));
    } else if ((static_cast<int>(arg.varType_) & ~1) == 4) {
        int regNum = Resources::getRegisterNumber();
        AsmRegister newReg(regNum);
        auto addiEntries = asmCommands_->addi(newReg, AsmRegister(0), Immediate(arg.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto asmEntry = asmCommands_->strig(newReg);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xcf), std::string("setTrigger")));
    }
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getTrigger(                                                                                                                   // @0x145ad0 (~1.6KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // No checkFunctionSupported — validates arg directly
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), std::string("getTrigger")));
    auto const& arg = args[0];
    if ((static_cast<int>(arg.varType_) & ~1) != 4)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), std::string("getTrigger")));
    auto results = std::make_shared<EvalResults>();
    int regNum1 = Resources::getRegisterNumber();
    int regNum2 = Resources::getRegisterNumber();
    AsmRegister reg1(regNum1);
    AsmRegister reg2(regNum2);
    // addi(reg1, R0, Immediate(arg.toInt())) → ltrig(reg1) → andr(reg1, reg2)
    auto addiEntries = asmCommands_->addi(reg1, AsmRegister(0), Immediate(arg.value_.toInt()));
    for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
    auto ltrigEntry = asmCommands_->ltrig(reg1);
    results->assemblers_.push_back(std::move(ltrigEntry));
    auto andrEntry = asmCommands_->andr(reg1, reg2);
    results->assemblers_.push_back(std::move(andrEntry));
    results->setValue(VarType(2), regNum1);
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setInternalTrigger(                                                                                                           // @0x146140 (~1.5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setInternalTrigger", static_cast<AwgDeviceType>(0x1c0));
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xd0), std::string("setInternalTrigger")));
    auto results = std::make_shared<EvalResults>(VarType(1));
    auto const& arg = args[0];
    if (static_cast<int>(arg.varType_) == 2) {
        AsmRegister reg(arg.value_.toInt());
        auto asmEntry = asmCommands_->sinttrig(reg);
        results->assemblers_.push_back(std::move(asmEntry));
    } else if ((static_cast<int>(arg.varType_) & ~1) == 4) {
        int regNum = Resources::getRegisterNumber();
        AsmRegister newReg(regNum);
        auto addiEntries = asmCommands_->addi(newReg, AsmRegister(0), Immediate(arg.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto asmEntry = asmCommands_->sinttrig(newReg);
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xd0), std::string("setInternalTrigger")));
    }
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getAnaTrigger(                                                                                                         // @0x146740 (~3.2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("getAnaTrigger", static_cast<AwgDeviceType>(0x5));
    if (args.size() != 1)                                                                          // @0x1467af: cmp rax,0x38
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), std::string("getAnaTrigger")));
    auto const& arg = args[0];
    if ((static_cast<int>(arg.varType_) & ~1) != 4)                                                // @0x14685a: and eax,0xfffffffd; cmp eax,0x4
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), std::string("getAnaTrigger")));
    // Read the trigger constant based on arg value (1 or 2)
    EvalResultValue localArg = arg;                                                                // @0x1468b1..1468cb: copy arg, call toInt
    int argVal = localArg.value_.toInt();
    if (argVal == 1) {                                                                             // @0x1468d5: cmp eax,0x1
        auto erv = res->readConst("AWG_ANA_TRIGGER1", EDirection_Write);                           // @0x146907: call readConst
        localArg = erv;
    } else if (argVal == 2) {                                                                      // @0x1468d2: cmp eax,0x2
        auto erv = res->readConst("AWG_ANA_TRIGGER2", EDirection_Write);                           // @0x14695e: call readConst
        localArg = erv;
    } else {
        // deviceType==2 check: if arg is in range 3..9, fall through to register allocation
        if (config_->deviceType != 2)                                                              // @0x1469e5: cmp [rcx],0x2
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xd7),
                    std::string("getAnaTrigger"), std::string("HDAWG")));
        if (static_cast<unsigned int>(argVal - 3) >= 6u)                                           // @0x1469f1: add eax,0xfffffffd; cmp eax,0x6
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xd7),
                    std::string("getAnaTrigger"), std::string("SHFSG/SHFQC")));
    }
    // Allocate two registers and build the EvalResults                                            // @0x1469fa
    int regNum1 = Resources::getRegisterNumber();
    int regNum2 = Resources::getRegisterNumber();
    AsmRegister reg1(regNum1);
    AsmRegister reg2(regNum2);
    auto results = std::make_shared<EvalResults>();                                                // @0x146a1a: new 0x98
    // addi(reg1, R0, constVal)                                                                    // @0x146ace: call addi
    auto addiEntries = asmCommands_->addi(reg1, AsmRegister(0), Immediate(localArg.value_.toInt()));
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
    results->setValue(VarType(2), static_cast<int>(reg1));                                         // @0x146ff1: call setValue(VarType,int)
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getDigTrigger(                                                                                                         // @0x147420 (~3.2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // NOTE: no checkFunctionSupported at top — validates arg first, then deviceType
    if (args.size() != 1)                                                                          // @0x147442: cmp rax,0x38
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), std::string("getDigTrigger")));
    auto const& arg = args[0];
    if ((static_cast<int>(arg.varType_) & ~1) != 4)                                                // @0x14750e: and eax,0xfffffffd; cmp eax,0x4
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), std::string("getDigTrigger")));
    // Read the trigger constant based on arg value (1 or 2)
    EvalResultValue localArg = arg;
    int argVal = localArg.value_.toInt();                                                          // @0x147570: call toInt
    if (argVal == 1) {                                                                             // @0x147581: cmp eax,0x1
        auto erv = res->readConst("AWG_DIG_TRIGGER1", EDirection_Write);                           // @0x1475b1: call readConst
        localArg = erv;
    } else if (argVal == 2) {                                                                      // @0x14757f: cmp eax,0x2
        auto erv = res->readConst("AWG_DIG_TRIGGER2", EDirection_Write);                           // @0x14760e: call readConst
        localArg = erv;
    } else {
        // deviceType==2 check                                                                     // @0x14768c..14769e
        if (config_->deviceType != 2)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xd7),
                    std::string("getDigTrigger"), std::string("HDAWG")));
        if (static_cast<unsigned int>(argVal - 3) >= 6u)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xd7),
                    std::string("getDigTrigger"), std::string("SHFSG/SHFQC")));
    }
    // Allocate two registers and build the EvalResults                                            // @0x1476a4
    int regNum1 = Resources::getRegisterNumber();
    int regNum2 = Resources::getRegisterNumber();
    AsmRegister reg1(regNum1);
    AsmRegister reg2(regNum2);
    auto results = std::make_shared<EvalResults>();
    // addi(reg1, R0, constVal) → ltrig(reg1) → andr(reg1, reg2)
    auto addiEntries = asmCommands_->addi(reg1, AsmRegister(0), Immediate(localArg.value_.toInt()));
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
    results->setValue(VarType(2), static_cast<int>(reg1));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setInt(                                                                                                                // @0x1480d0 (~2.5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("setInt", static_cast<AwgDeviceType>(0x7));
    if (args.size() != 2)                                                                          // @0x148131: cmp rax,0x70
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xbc), std::string("setInt")));
    auto const& arg0 = args[0];
    auto const& arg1 = args[1];
    // Validate: arg0 must be string (varType_==3)                                                 // @0x1483bc: cmp [rbp-0x98],0x3
    if (static_cast<int>(arg0.varType_) != 3)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xbc), std::string("setInt")));
    // Validate: arg1 must be numeric (bitmask 0x54: bits 2,4,6 = types 2,4,6)                    // @0x1483d5..1483dd: bt 0x54,ecx
    int arg1Type = static_cast<int>(arg1.varType_);
    if (arg1Type > 6 || !((0x54 >> arg1Type) & 1))
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xbe), std::string("setInt")));
    // Call writeToNode(arg0, arg1, defaultEvalResultValue, res)                                   // @0x1486f2: call writeToNode
    EvalResultValue emptyErv{};
    emptyErv.varType_ = VarType(3);
    emptyErv.value_ = Value(1.0);
    emptyErv.varSubType_ = VarSubType(2);
    writeToNode(arg0, arg1, emptyErv, std::move(res));
    return nullptr;
}
std::shared_ptr<EvalResults> CustomFunctions::setDouble(                                                                                                             // @0x148ac0 (~3.3KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("setDouble", static_cast<AwgDeviceType>(0x7));
    // Accept 2 or 3 args: (size & ~1) == 2                                                       // @0x148b36..148b3a: and rcx,0xfffffffffffffffe; cmp rcx,0x2
    size_t sz = args.size();
    if ((sz & ~static_cast<size_t>(1)) != 2)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xb8), std::string("setDouble")));
    auto const& arg0 = args[0];
    auto const& arg1 = args[1];
    // Default 3rd arg: VarType(3), double 1.0, VarSubType(2)                                     // @0x148d88..148dd0
    EvalResultValue arg2{};
    arg2.varType_ = VarType(3);
    arg2.value_ = Value(1.0);
    arg2.varSubType_ = VarSubType(2);
    // If 3 args provided, copy the 3rd arg over                                                   // @0x148df6: cmp rax,0xa8
    if (args.size() == 3) {
        arg2 = args[2];
    }
    // Validate: arg0 must be string (varType_==3)                                                 // @0x148e33: cmp [rbp-0x98],0x3
    if (static_cast<int>(arg0.varType_) != 3)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xb8), std::string("setDouble")));
    // Validate: arg1 must be numeric (bitmask 0x54)                                               // @0x148e43..148e51: bt 0x54,eax
    int arg1Type = static_cast<int>(arg1.varType_);
    if (arg1Type > 6 || !((0x54 >> arg1Type) & 1))
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xbb), std::string("setDouble")));
    // Validate: arg2.varType_ must be int type ((varType & ~1) == 4)                              // @0x148e5a..148e63: and eax,0xfffffffd; cmp eax,0x4
    if ((static_cast<int>(arg2.varType_) & ~1) != 4)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xba), std::string("setDouble")));
    // Call writeToNode(arg0, arg1, arg2, res)                                                     // @0x149186: call writeToNode
    writeToNode(arg0, arg1, arg2, std::move(res));
    return nullptr;
}
std::shared_ptr<EvalResults> CustomFunctions::generate(                                                                                                              // @0x149940 (~2.8KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // No checkFunctionSupported
    if (args.empty())                                                                              // @0x14995e: cmp rbx,[rdx+0x8]
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x66)));
    auto const& firstArg = args[0];
    // First arg must be string (varType_==3)                                                      // @0x149a23: cmp eax,0x3
    if (static_cast<int>(firstArg.varType_) != 3)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x66)));
    // If single arg and first arg has VarSubType==2 (marker), return early                        // @0x149a4b: cmp rax,0x1; jbe
    if (args.size() <= 1) {
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x61),
                firstArg.value_.toString()));
    }
    // Check if second arg has VarSubType==2 → early return with setValue                          // @0x149bb1: cmp [rbx-0x28],0x2 (varType_); 149bbb: cmp [r12+0x4],0x2 (varSubType_)
    if (static_cast<int>(args[1].varType_) == 2 && static_cast<int>(args[1].varSubType_) == 2) {  // @0x149bc1: je 149d57
        auto results = std::make_shared<EvalResults>();
        Value emptyVal;                                                                            // @0x149d61..149d77: VarType(4), default Value
        results->setValue(VarType(5), VarSubType(2), emptyVal);                                    // @0x149d8c: call setValue(VarType,VarSubType,Value)
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
                ErrorMessages::format(static_cast<ErrorMessageT>(0x67),
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
    checkFunctionSupported("setUserReg", static_cast<AwgDeviceType>(0x1ff));
    if (args.size() != 2)                                                                          // @0x14a48d: cmp rax,0x70
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xc7), std::string("setUserReg")));
    auto const& arg0 = args[0];
    auto const& arg1 = args[1];
    // Validate: arg0 must be int type ((varType & ~1) == 4)                                       // @0x14a5f2..14a5f5: and eax,0xfffffffd; cmp eax,0x4
    if ((static_cast<int>(arg0.varType_) & ~1) != 4)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xc7), std::string("setUserReg")));
    // Range-check arg0 against devConst_->memoryDepth                                             // @0x14a60a..14a624
    int arg0Val = arg0.value_.toInt();
    if (static_cast<int64_t>(arg0Val) >= static_cast<int64_t>(devConst_->memoryDepth) || arg0Val < 0) {
        // Check if varSubType_ == 2 (allows bypass)                                               // @0x14a62d: cmp [rbp-0x11c],0x2
        if (static_cast<int>(arg0.varSubType_) != 2)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xc7), std::string("setUserReg")));
    }
    // Create results with VarType(1)                                                              // @0x14a633..14a65e
    auto results = std::make_shared<EvalResults>(VarType(1));
    int regNum = Resources::getRegisterNumber();                                                   // @0x14a66e: call getRegisterNumber
    AsmRegister reg(regNum);
    // Branch on arg1 type                                                                         // @0x14a67e: cmp eax,0x2
    if (static_cast<int>(arg1.varType_) == 2) {
        // arg1 is a register: suser(reg, arg0.toInt())                                            // @0x14a6b3: call suser
        auto suserEntry = asmCommands_->suser(AsmRegister(arg1.value_.toInt()),
                                              detail::AddressImpl<unsigned int>(arg0.value_.toInt()));
        results->assemblers_.push_back(std::move(suserEntry));
    } else if ((static_cast<int>(arg1.varType_) & ~1) == 4) {                                     // @0x14a71e: and eax,0xfffffffd; cmp eax,0x4
        // arg1 is int: addi(reg, R0, arg1.toInt()) then suser(reg, arg0.toInt())                  // @0x14a774: call addi; @0x14a8d4: call suser
        auto addiEntries = asmCommands_->addi(reg, AsmRegister(0), Immediate(arg1.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto suserEntry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(arg0.value_.toInt()));
        results->assemblers_.push_back(std::move(suserEntry));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xc7), std::string("setUserReg")));
    }
    // addi(reg, R0, Imm(0xb)) + suser(reg, 0x10)                                                 // @0x14a9b8..14aaf9
    {
        auto addiEntries = asmCommands_->addi(reg, AsmRegister(0), Immediate(0xb));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto suserEntry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x10));
        results->assemblers_.push_back(std::move(suserEntry));
    }
    // addi(reg, R0, arg0.toInt()) + suser(reg, 0x11)                                             // @0x14abee..14ad29
    {
        auto addiEntries = asmCommands_->addi(reg, AsmRegister(0), Immediate(arg0.value_.toInt()));
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));
        auto suserEntry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x11));
        results->assemblers_.push_back(std::move(suserEntry));
    }
    // If deviceType==2: luser(reg2, 0) + suser(reg2, 0) + addSyncCommand                         // @0x14adec: cmp [rax],0x2
    if (config_->deviceType == 2) {
        int regNum2 = Resources::getRegisterNumber();                                              // @0x14adf5: call getRegisterNumber
        AsmRegister reg2(regNum2);
        auto luserEntry = asmCommands_->luser(reg2, detail::AddressImpl<unsigned int>(0));          // @0x14ae1d: call luser(reg2, 0)
        results->assemblers_.push_back(std::move(luserEntry));
        auto suserEntry2 = asmCommands_->suser(reg2, detail::AddressImpl<unsigned int>(0));         // @0x14aef5: call suser(reg2, 0)
        results->assemblers_.push_back(std::move(suserEntry2));
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
    checkFunctionSupported("getUserReg", static_cast<AwgDeviceType>(0x1ff));

    // @0x14b4e0..0x14b4ee: args.size() == 1
    if (args.size() != 1) {                                                          // @0x14b4ee: jne error
        throw CustomFunctionsException(
            ErrorMessages::get(static_cast<ErrorMessageT>(0x68)));                   // @0x14badb
    }

    // @0x14b4f4..0x14b557: allocate EvalResults (default ctor, no VarType arg)
    auto results = std::make_shared<EvalResults>();

    // @0x14b563..0x14b5ec: copy arg[0] into local EvalResultValue
    EvalResultValue arg0 = args[0];

    // @0x14b5e9..0x14b5ef: check arg is int type ((varType & ~1) == 4)
    if ((static_cast<int>(arg0.varType_) & ~1) != 4) {                               // @0x14b5ef: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), "getUserReg"));  // @0x14bb21
    }

    // @0x14b5f5..0x14b617: range check: 0 <= arg.toInt() < devConst_->memoryDepth (+0x30)
    int userRegIdx = arg0.value_.toInt();                                            // @0x14b5fc
    if (static_cast<int64_t>(userRegIdx) >= static_cast<int64_t>(devConst_->memoryDepth) // @0x14b607: cmp [rcx+0x30], rax
        || userRegIdx < 0) {                                                         // @0x14b615: test eax,eax; jns ok
        if (static_cast<int>(arg0.varType_) != 2) {                                  // @0x14b619: cmp [rbp-0x7c], 2; jne throw
            throw CustomFunctionsValueException(
                ErrorMessages::get(static_cast<ErrorMessageT>(0x69)), 1);            // @0x14bb6f
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

    // @0x14b72d..0x14b747: setValue(VarType(2), regNum)
    results->setValue(VarType(2), static_cast<int>(reg));                             // @0x14b747

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
        auto suserEntry = asmCommands_->suser(reg2, detail::AddressImpl<unsigned int>(0x69)); // @0x14b8c9
        results->assemblers_.push_back(std::move(suserEntry));
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
    checkFunctionSupported("getSweeperLength", static_cast<AwgDeviceType>(0x5));

    // @0x14bcf0..0x14bcfe: args.size() == 1
    if (args.size() != 1) {                                                          // @0x14bcfe: jne error
        throw CustomFunctionsException(
            ErrorMessages::get(static_cast<ErrorMessageT>(0x6c)));                   // @0x14c167
    }

    // @0x14bd04..0x14bda3: copy arg[0] into local EvalResultValue
    EvalResultValue arg0 = args[0];

    // @0x14bda3..0x14bda9: check arg is int type ((varType & ~1) == 4)
    if ((static_cast<int>(arg0.varType_) & ~1) != 4) {                               // @0x14bda9: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), "getSweeperLength")); // @0x14c1ad
    }

    // @0x14bdaf..0x14bdc1: range check: arg.toInt() must be <= 1 (or varType==2 fallback)
    int sweepIdx = arg0.value_.toInt();                                              // @0x14bdb9
    if (sweepIdx != 1) {                                                             // @0x14bdc1: cmp eax,1; je ok
        if (static_cast<int>(arg0.varType_) != 2) {                                  // @0x14bdc3: cmp varType, 2; jne throw
            throw CustomFunctionsValueException(
                ErrorMessages::get(static_cast<ErrorMessageT>(0x6d)), 1);            // @0x14c1f8
        }
    }

    // @0x14bdd0..0x14bdfa: build intermediate EvalResultValue for readConst result
    // Initialize an empty EvalResultValue with AsmRegister(-1) placeholder
    EvalResultValue constResult;
    constResult.varType_ = VarType(0);
    constResult.varSubType_ = VarSubType(0);

    // @0x14bdff..0x14bf04: choose constant name based on sweepIdx
    sweepIdx = arg0.value_.toInt();                                                  // @0x14be02: second toInt call
    if (sweepIdx == 2) {
        // @0x14be10..0x14be89: readConst("AWG_USERREG_SWEEP_COUNT1", EDirection_Write)
        EvalResultValue rc = res->readConst("AWG_USERREG_SWEEP_COUNT1",
                                             static_cast<EDirection>(1));            // @0x14be61
        constResult.varType_ = rc.varType_;
        constResult.value_ = rc.value_;
    } else {
        // @0x14be8b..0x14bf04: readConst("AWG_USERREG_SWEEP_COUNT0", EDirection_Write)
        EvalResultValue rc = res->readConst("AWG_USERREG_SWEEP_COUNT0",
                                             static_cast<EDirection>(1));            // @0x14bedc
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

    // @0x14c0cb..0x14c0e1: setValue(VarType(2), regNum)
    results->setValue(VarType(2), static_cast<int>(reg));                             // @0x14c0e1

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setPrecompClear(                                                                                                              // @0x14c720 (~1KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setPrecompClear", static_cast<AwgDeviceType>(0x2));
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xc2), std::string("setPrecompClear")));
    if ((static_cast<int>(args[0].varType_) & ~1) != 4)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xc1), std::string("setPrecompClear")));
    auto results = std::make_shared<EvalResults>(VarType(1));
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
    checkFunctionSupported("at", static_cast<AwgDeviceType>(0x5));

    // @0x14ce7f..0x14ce8d: args.size() == 1 (byte size == 0x38)
    if (args.size() != 1)                                                                        // @0x14ce89: cmp rax,0x38
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "at"));                      // @0x14d5f1

    // @0x14ce93..0x14cf2e: copy arg[0] into local
    EvalResultValue arg0 = args[0];                                                              // @0x14ce93

    // @0x14cf35..0x14cf8a: if varType == 1 (bool), warn with error 0x36
    if (static_cast<int>(arg0.varType_) == 1) {                                                  // @0x14cf35: cmp eax,1
        std::string warnMsg = ErrorMessages::format(static_cast<ErrorMessageT>(0x36), "at");     // @0x14cf41..0x14cf4d
        if (warningCallback_) {                                                                  // @0x14cf59
            warningCallback_(warnMsg);                                                               // @0x14cf6c: call [rax+0x30]
        }
    }

    // @0x14cf8f..0x14cff6: make_shared<EvalResults>() — default ctor, NOT VarType(1)
    auto results = std::make_shared<EvalResults>();                                               // @0x14cf94

    // @0x14cffa..0x14d007: check arg0.varType_ == 2 (register type)
    if (static_cast<int>(arg0.varType_) == 2) {                                                  // @0x14d000: cmp eax,2
        // @0x14d00d..0x14d024: suser(arg0.asmRegister_, 0x1d) — arg already in register
        auto suserEntry = asmCommands_->suser(arg0.reg_, detail::AddressImpl<unsigned int>(0x1d)); // @0x14d024
        results->assemblers_.push_back(std::move(suserEntry));                                   // @0x14d029..0x14d090
    } else {
        // @0x14d095..0x14d0fb: int type path — getRegisterNumber, addi, then suser
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                        // @0x14d095: and eax,0xfffffffd; cmp eax,4
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "at"));                  // @0x14d648

        int regNum = Resources::getRegisterNumber();                                             // @0x14d0a1
        AsmRegister reg(regNum);                                                                 // @0x14d0ac

        AsmRegister zero(0);                                                                     // @0x14d0c2
        int val = arg0.value_.toInt();                                                           // @0x14d0ce
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(val));                        // @0x14d0fb

        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));                // @0x14d100..0x14d136

        // @0x14d22b..0x14d243: suser(reg, 0x1d)
        auto suserEntry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x1d));     // @0x14d243
        results->assemblers_.push_back(std::move(suserEntry));                                   // @0x14d248..0x14d2a8
    }

    // @0x14d303..0x14d311: second getRegisterNumber → reg2
    int regNum2 = Resources::getRegisterNumber();                                                // @0x14d303
    AsmRegister reg2(regNum2);                                                                   // @0x14d311

    // @0x14d316..0x14d360: readConst("AWG_TIME_TRIGGER", EDirection_Write), addi
    AsmRegister zero2(0);                                                                        // @0x14d32e
    EvalResultValue timeTrigErv = res->readConst("AWG_TIME_TRIGGER", static_cast<EDirection>(1)); // @0x14d360
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
    checkFunctionSupported("lock", static_cast<AwgDeviceType>(0x5));
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x79), std::string("lock")));
    if (static_cast<int>(args[0].varType_) != 5)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x7a), std::string("lock")));
    std::string name = args[0].value_.toString();
    std::optional<std::string> optName(name);
    auto wf = wavetableFront_->getWaveformByName(optName);
    if (!wf)
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xe3), name), 0);
    auto results = std::make_shared<EvalResults>(VarType(1));
    auto asmEntry = asmCommands_->asmLockPlaceholder(wf, config_->deviceIndex);
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::unlock(                                                                                                                       // @0x14e180 (1295B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("unlock", static_cast<AwgDeviceType>(0x5));
    if (args.size() != 1)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xdc), std::string("unlock")));
    if (static_cast<int>(args[0].varType_) != 5)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xdd), std::string("unlock")));
    std::string name = args[0].value_.toString();
    std::optional<std::string> optName(name);
    auto wf = wavetableFront_->getWaveformByName(optName);
    if (!wf)
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xe3), name), 0);
    auto results = std::make_shared<EvalResults>(VarType(1));
    auto asmEntry = asmCommands_->asmUnlockPlaceholder(wf, config_->deviceIndex);
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::getCnt(  // @0x14e8d0 (1258B, ends ~0x14edba)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // @0x14e8ed..0x14e91d: checkFunctionSupported("getCnt", 0x2)
    checkFunctionSupported("getCnt", static_cast<AwgDeviceType>(0x2));

    // @0x14e922..0x14e930: args.size() == 1
    if (args.size() != 1) {                                                          // @0x14e930: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), "getCnt"));      // @0x14ebc5
    }

    // @0x14e936..0x14e93c: config_->deviceType == 2 (HDAWG)
    if (static_cast<int>(config_->deviceType) != 2) {                                // @0x14e93c: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3f), "getCnt"));      // @0x14ec1a
    }

    // @0x14e942..0x14e99a: allocate EvalResults (default ctor)
    auto results = std::make_shared<EvalResults>();

    // @0x14e9a1..0x14ea26: copy arg[0] into local EvalResultValue
    EvalResultValue arg0 = args[0];

    // @0x14ea2f..0x14ea35: check arg is int type ((varType & ~1) == 4)
    if ((static_cast<int>(arg0.varType_) & ~1) != 4) {                               // @0x14ea35: jne error
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), "getCnt"));      // @0x14ec6f
    }

    // @0x14ea3b..0x14ea4e: range check: arg.toInt() < devConst_->field_54 (+0x54)
    int counterIdx = arg0.value_.toInt();                                            // @0x14ea42
    if (counterIdx >= devConst_->field_54) {                                      // @0x14ea4e: jl ok
        if (static_cast<int>(arg0.varType_) != 2) {                                  // @0x14ea50: cmp varType, 2; jne throw
            throw CustomFunctionsValueException(
                ErrorMessages::get(static_cast<ErrorMessageT>(0x6b)), 0);            // @0x14ecba
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

    // @0x14eb54..0x14eb6a: setValue(VarType(2), regNum)
    results->setValue(VarType(2), static_cast<int>(reg));                             // @0x14eb6a

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::waitQAResultTrigger(                                    // @0x14edc0
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {

    // @0x14ede0-0x14ee19: build function name string and check device support
    checkFunctionSupported("waitQAResultTrigger", static_cast<AwgDeviceType>(0x4));                    // @0x14ee19: call checkFunctionSupported

    // @0x14ee1e-0x14ee25: args must be empty (args.end == args.begin)
    if (!args.empty())                                                                                 // @0x14ee25: jne 0x14f1ff (throw)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), "waitQAResultTrigger"));           // @0x14f1ff-0x14f24a

    // @0x14ee2b-0x14ee5d: allocate EvalResults with VarType::Integer
    auto results = std::make_shared<EvalResults>(VarType(1));                                          // @0x14ee30: new(0x98); @0x14ee55: EvalResults(VarType(1))

    // @0x14ee61-0x14eea2: build local EvalResultValue (erv) with varType=4 (for readConst)
    //   erv at [rbp-0x90]: varType=4, which_=1, value int=0, other fields=0
    //   erv2 at [rbp-0x88]: varType=1, which_=0, value int=0

    // @0x14eea9-0x14eeb6: create placeholder register AsmRegister(-1)
    //   AsmRegister(-1) stored at [rbp-0x60]

    // @0x14eebb-0x14eee8: readConst("AWG_DEMOD_TRIGGER2", EDirection_Write) via res
    auto erv = res->readConst("AWG_DEMOD_TRIGGER2", EDirection_Write);                                // @0x14eee8: call Resources::readConst

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
    checkFunctionSupported("getQAResult", static_cast<AwgDeviceType>(0x4));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), std::string("getQAResult")));
    auto results = std::make_shared<EvalResults>();
    int regNum = Resources::getRegisterNumber();
    AsmRegister reg(regNum);
    auto asmEntry = asmCommands_->ld(reg, detail::AddressImpl<unsigned int>(0x61));
    results->assemblers_.push_back(std::move(asmEntry));
    results->setValue(VarType(2), regNum);
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::startQAResult(                                                                                                             // @0x14f620 (~2.7KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("startQAResult", static_cast<AwgDeviceType>(0x4));                               // @0x14f67a

    // @0x14f67f: max 2 args (throws 0x45 if >= 3)
    if (args.size() >= 3)                                                                                   // @0x14f688
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x45), std::string("startQAResult")));

    // @0x14f6a2: readConst("QA_INT_ALL", EDirection(1)) — default integration mask
    auto qaIntAllErv = res->readConst("QA_INT_ALL", static_cast<EDirection>(1));                            // @0x14f6d4
    int qaIntAll = qaIntAllErv.value_.toInt();                                                              // @0x14f6e0

    int resultAddr = 0;                                                                                     // @0x14f700: [rbp-0x100] = 0

    // @0x14f70c: if args not empty, first arg overrides qaIntAll
    if (!args.empty()) {
        auto const& arg0 = args[0];
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                                   // @0x14f71e: type check
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3e)));
        qaIntAll = arg0.value_.toInt();                                                                     // @0x14f72a

        // @0x14f736: if 2nd arg exists
        if (args.size() >= 2) {
            auto const& arg1 = args[1];
            if ((static_cast<int>(arg1.varType_) & ~1) != 4)                                               // @0x14f748
                throw CustomFunctionsException(
                    ErrorMessages::format(static_cast<ErrorMessageT>(0x3e)));
            resultAddr = arg1.value_.toInt();                                                               // @0x14f754
        }
    }

    auto results = std::make_shared<EvalResults>(VarType(1));                                               // @0x14f760

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
    checkFunctionSupported("startQAMonitor", static_cast<AwgDeviceType>(0x4));                              // @0x150110

    // @0x150115: max 1 arg (throws 0x45 if >= 2)
    if (args.size() >= 2)                                                                                   // @0x15011e
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x45), std::string("startQAMonitor")));

    int monitorVal = 0;                                                                                     // @0x150140

    // @0x15014c: if 1 arg, extract value
    if (!args.empty()) {
        auto const& arg0 = args[0];
        if ((static_cast<int>(arg0.varType_) & ~1) != 4)                                                   // @0x15015e
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3e)));
        monitorVal = arg0.value_.toInt();                                                                   // @0x15016a
    }

    auto results = std::make_shared<EvalResults>(VarType(1));                                               // @0x150178

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
    checkFunctionSupported("executeTableEntry", static_cast<AwgDeviceType>(0x1f2));                         // @0x15096a

    // @0x15096f: at least 1 arg required
    if (args.empty())                                                                                       // @0x150978
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3d), std::string("executeTableEntry")));

    auto const& arg0 = args[0];                                                                             // @0x1509a0

    auto results = std::make_shared<EvalResults>(VarType(1));                                               // @0x1509b0

    // @0x150a60: handle wait cycles from remaining args
    setWaitCyclesReg(args, results, res);                                                                   // @0x150a68

    int argType = static_cast<int>(arg0.varType_);                                                          // @0x150a70

    if (argType == 4) {
        // Integer constant path                                                                             // @0x150a80
        int tableIndex = arg0.value_.toInt();                                                               // @0x150a8c

        // Try matching known ZSYNC/QA data constants
        auto zsyncRaw = res->readConst("ZSYNC_DATA_RAW", static_cast<EDirection>(1));                       // @0x150ac0
        if (tableIndex == zsyncRaw.value_.toInt()) {
            // shift = 1                                                                                     // @0x150ad0
            auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                1 << (devConst_->numOutputPorts + 1));                                                      // @0x150ae0: 1 << (field_78 + 1)
            results->assemblers_.push_back(std::move(asmEntry));
        } else {
            auto zsyncProcA = res->readConst("ZSYNC_DATA_PROCESSED_A", static_cast<EDirection>(1));         // @0x150b60
            if (tableIndex == zsyncProcA.value_.toInt()) {
                // shift = 9                                                                                 // @0x150b70
                auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                    1 << (devConst_->numOutputPorts + 9));                                                  // @0x150b80
                results->assemblers_.push_back(std::move(asmEntry));
            } else {
                auto zsyncProcB = res->readConst("ZSYNC_DATA_PROCESSED_B", static_cast<EDirection>(1));     // @0x150c00
                if (tableIndex == zsyncProcB.value_.toInt()) {
                    // shift = 0xd                                                                           // @0x150c10
                    auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                        1 << (devConst_->numOutputPorts + 0xd));                                            // @0x150c20
                    results->assemblers_.push_back(std::move(asmEntry));
                } else if (config_->deviceType == static_cast<AwgDeviceType>(0x20)) {
                    // SHFQA-specific constants                                                              // @0x150ca0
                    auto qaRaw = res->readConst("QA_DATA_RAW", static_cast<EDirection>(1));                 // @0x150cd0
                    if (tableIndex == qaRaw.value_.toInt()) {
                        // TODO: exact shift for QA_DATA_RAW path                                            // @0x150ce0
                        auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                            1 << (devConst_->numOutputPorts + 1));                                          // placeholder shift
                        results->assemblers_.push_back(std::move(asmEntry));
                    } else {
                        auto qaProcD = res->readConst("QA_DATA_PROCESSED_D", static_cast<EDirection>(1));   // @0x150d60
                        if (tableIndex == qaProcD.value_.toInt()) {
                            // shift = 0x10                                                                  // @0x150d70
                            auto asmEntry = asmCommands_->wvft(AsmRegister(0),
                                1 << (devConst_->numOutputPorts + 0x10));                                   // @0x150d80
                            results->assemblers_.push_back(std::move(asmEntry));
                        } else {
                            throw CustomFunctionsException(
                                ErrorMessages::get(static_cast<ErrorMessageT>(0x2f)));                      // @0x150e00
                        }
                    }
                } else {
                    throw CustomFunctionsException(
                        ErrorMessages::get(static_cast<ErrorMessageT>(0x2f)));                              // @0x150e40
                }
            }
        }
    } else if (argType == 2) {
        // Register path                                                                                     // @0x150e80
        auto asmEntry = asmCommands_->wvft(AsmRegister(arg0.value_.toInt()),
            1 << (devConst_->numOutputPorts + 1));                                                          // @0x150ea0
        results->assemblers_.push_back(std::move(asmEntry));
    } else if ((argType & ~1) == 4) {
        // Numeric (int/uint) path — direct table entry index                                                // @0x150f00
        int entryIndex = arg0.value_.toInt();                                                               // @0x150f10
        if (entryIndex < 0)
            throw CustomFunctionsValueException(
                ErrorMessages::get(static_cast<ErrorMessageT>(0x30)), 0);                                   // @0x150f40
        if ((entryIndex >> devConst_->numOutputPorts) != 0)                                                 // @0x150f50: validate index fits
            throw CustomFunctionsValueException(
                ErrorMessages::get(static_cast<ErrorMessageT>(0x30)), 0);                                   // @0x150f80
        auto asmEntry = asmCommands_->wvft(AsmRegister(0), entryIndex);                                    // @0x150fa0
        results->assemblers_.push_back(std::move(asmEntry));
    } else {
        throw CustomFunctionsException(
            ErrorMessages::get(static_cast<ErrorMessageT>(0x2d)));                                          // @0x151000
    }

    return results;                                                                                         // @0x151060
}
std::shared_ptr<EvalResults> CustomFunctions::setPRNGSeed(                                                                                                               // @0x1513e0 (~1.6KB, ends ~0x151a68)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("setPRNGSeed", static_cast<AwgDeviceType>(0x1f2));                           // @0x151435

    // @0x151443: exactly 1 argument required
    if (args.size() != 1)                                                                               // @0x151447: cmp rax, 0x38
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xcf)));                                   // @0x15189c

    auto results = std::make_shared<EvalResults>(VarType(1));                                           // @0x15144d

    int argType = static_cast<int>(args[0].varType_);                                                   // @0x151502: [rbp-0x78]

    if (argType == 2) {
        // Integer literal path — emit suser directly with value as immediate                            // @0x151518
        auto asmEntry = asmCommands_->suser(
            AsmRegister(args[0].value_.toInt()),                                                        // @0x151528
            detail::AddressImpl<unsigned int>(0x74));                                                    // PRNG seed register
        results->assemblers_.push_back(std::move(asmEntry));                                            // @0x151535
    } else if ((argType & ~2) == 4) {
        // Float/double path — range-check then load via register                                        // @0x1515a9
        double dval = args[0].value_.toDouble();                                                        // @0x1515bd

        if (dval < 0.0)                                                                                 // @0x1515ca
            throw CustomFunctionsValueException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xcb)), 0);                            // @0x1518db
        if (dval == 0.0)                                                                                // @0x1515dc
            throw CustomFunctionsValueException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xcc)), 0);                            // @0x151908
        if (dval > 4294967295.0)                                                                        // @0x1515f1: constant at 0x956038
            throw CustomFunctionsValueException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0xcd)), 0);                            // @0x151949

        int regNum = Resources::getRegisterNumber();                                                    // @0x1515ff
        AsmRegister seedReg(regNum);
        AsmRegister zero(0);
        int seedVal = args[0].value_.toInt();                                                           // @0x151625
        auto addiEntries = asmCommands_->addi(seedReg, zero, Immediate(seedVal));                       // @0x15164f
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));                       // @0x151689

        auto asmEntry = asmCommands_->suser(seedReg, detail::AddressImpl<unsigned int>(0x74));          // @0x15180a
        results->assemblers_.push_back(std::move(asmEntry));                                            // @0x15180f
    }

    return results;                                                                                      // @0x15178e
}
std::shared_ptr<EvalResults> CustomFunctions::getPRNGValue(                                                                                                                 // @0x151a70 (600B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("getPRNGValue", static_cast<AwgDeviceType>(0x1f2));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), std::string("getPRNGValue")));
    auto results = std::make_shared<EvalResults>();
    int regNum = Resources::getRegisterNumber();                                   // @0x151b34
    AsmRegister reg(regNum);
    auto asmEntry = asmCommands_->luser(reg, detail::AddressImpl<unsigned int>(0x77));  // @0x151b44
    results->assemblers_.push_back(std::move(asmEntry));
    results->setValue(VarType(2), regNum);                                         // @0x151c1b
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setPRNGRange(                                                                                                              // @0x151ce0 (~2.4KB, ends ~0x152683)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("setPRNGRange", static_cast<AwgDeviceType>(0x1f2));                           // @0x151d3a

    // @0x151d3f: exactly 2 arguments required
    if (args.size() != 2)                                                                               // @0x151d48: cmp rax, 0x70
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xce)));                                   // @0x1524a4

    auto results = std::make_shared<EvalResults>(VarType(1));                                           // @0x151d52

    // @0x151eeb: both args must NOT be type 2 (integer literal → accepted, float needs validation)
    // Actually: type check validates non-float integer types
    // @0x151f0b-0x151f69: range and ordering validation
    int val0 = args[0].value_.toInt();                                                                  // @0x151f0b
    if (val0 < 0)                                                                                       // @0x151f12
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xce)), 0);                                // @0x152463
    int val1 = args[1].value_.toInt();                                                                  // @0x151f22
    if (val1 < 0)                                                                                       // @0x151f29
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xce)), 0);

    if (val0 > 0xFFFE)                                                                                 // @0x151f37: cmp eax, 0xfffe; jg
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xce)), 0);
    if (val1 >= 0xFFFF)                                                                                 // @0x151f4a: cmp eax, 0xffff; jge
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xce)), 0);

    int rangeMin = args[0].value_.toInt();                                                              // @0x151f58
    int rangeMax = args[1].value_.toInt();                                                              // @0x151f62
    if (rangeMin > rangeMax)                                                                            // @0x151f69
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xce)));                                   // @0x152522

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
        auto asmEntry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x75));              // @0x15210d
        results->assemblers_.push_back(std::move(asmEntry));                                            // @0x15211c
    }

    // Instruction 2: addi reg, R0, #(rangeMax - rangeMin + 1) → suser reg, 0x76 (PRNG range span)    // @0x1521e5
    {
        AsmRegister zero(0);
        int span = rangeMax - rangeMin + 1;                                                             // @0x1521ea
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(span));                              // @0x152218
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));                       // @0x152244
    }
    {
        auto asmEntry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x76));              // @0x152328
        results->assemblers_.push_back(std::move(asmEntry));                                            // @0x15233c
    }

    return results;                                                                                      // @0x15244d
}
std::shared_ptr<EvalResults> CustomFunctions::startQA(                                                                                                                   // @0x152690 (~6.2KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("startQA", static_cast<AwgDeviceType>(0xc));                                     // @0x1526f0

    // @0x1526f5: validate arg count — max depends on device type
    size_t maxArgs = (config_->deviceType == static_cast<AwgDeviceType>(8)) ? 5 : 4;                       // @0x152700
    if (args.size() > maxArgs)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x45), std::string("startQA")));

    // @0x152740: validate all provided args are int type ((varType | 2) == 6 → types 4,5,6,7)
    for (size_t i = 0; i < args.size(); ++i) {                                                             // @0x152748
        if ((static_cast<int>(args[i].varType_) | 2) != 6)
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3e)));
    }

    auto results = std::make_shared<EvalResults>(VarType(1));                                               // @0x152790

    // Defaults
    int integrationWeightsMask = 0;                                                                         // only used for deviceType==8
    int qaIntAll = 0;
    int monitorEnable = 0;
    int resultLengthShift = 0;
    int resultAddr = 0;

    // @0x152840: if deviceType == 8 (SHFQA), read QA_GEN_ALL and extract first arg as weights mask
    bool qaGenAllEnabled = false;
    if (config_->deviceType == static_cast<AwgDeviceType>(8)) {                                            // @0x152840
        auto qaGenAllErv = res->readConst("QA_GEN_ALL", static_cast<EDirection>(1));                        // @0x152870
        int qaGenAll = qaGenAllErv.value_.toInt();                                                          // @0x152880

        if (!args.empty()) {
            integrationWeightsMask = args[0].value_.toInt();                                                // @0x1528a0
            if (~qaGenAll & integrationWeightsMask)                                                         // @0x1528b0: validate mask
                throw CustomFunctionsException(
                    ErrorMessages::format(static_cast<ErrorMessageT>(0x3e)));
        }
        qaGenAllEnabled = true;                                                                             // bit 30 flag
    }

    // @0x152900: read QA_INT_ALL
    auto qaIntAllErv = res->readConst("QA_INT_ALL", static_cast<EDirection>(1));                            // @0x152930
    qaIntAll = qaIntAllErv.value_.toInt();                                                                  // @0x152940

    // @0x152960: args[1] (or args[0] for non-SHFQA) — integration trigger mask
    size_t intTrigIdx = (config_->deviceType == static_cast<AwgDeviceType>(8)) ? 1 : 0;
    if (args.size() > intTrigIdx) {
        int intTrigMask = args[intTrigIdx].value_.toInt();                                                  // @0x152970
        if (~qaIntAll & intTrigMask)                                                                        // @0x152980: validate
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3e)));
        qaIntAll = intTrigMask;                                                                             // override with user value
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
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3e)));
    }

    // @0x152a40: optional result address arg
    size_t raIdx = rlsIdx + 1;
    if (args.size() > raIdx) {
        resultAddr = args[raIdx].value_.toInt();                                                            // @0x152a50
        if (resultAddr >= 256)                                                                              // @0x152a60
            throw CustomFunctionsException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x3e)));
    }

    AsmRegister zero(0);

    // @0x152aa0: first register — integration weights / result address composite
    {
        int regNum = Resources::getRegisterNumber();                                                        // @0x152aa4
        AsmRegister reg(regNum);
        int imm = (resultAddr << 24) | integrationWeightsMask;                                             // @0x152ac0
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(imm));                                  // @0x152ae0
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

        auto asmEntry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x78));                  // @0x152bc0
        results->assemblers_.push_back(std::move(asmEntry));
    }

    // @0x152c80: second register — integration trigger / monitor / gen composite
    {
        int regNum = Resources::getRegisterNumber();                                                        // @0x152c84
        AsmRegister reg(regNum);
        int imm = (resultLengthShift << 22) | qaIntAll
                  | (monitorEnable << 31)
                  | (qaGenAllEnabled ? (1 << 30) : 0);                                                     // @0x152cc0
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(imm));                                  // @0x152ce0
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

        auto asmEntry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x79));                  // @0x152dc0
        results->assemblers_.push_back(std::move(asmEntry));
    }

    // @0x1533df: if deviceType == 4 (UHFQA), emit third register for result length
    if (config_->deviceType == static_cast<AwgDeviceType>(4)) {                                            // @0x1533df
        int regNum = Resources::getRegisterNumber();                                                        // @0x1533e8
        AsmRegister reg(regNum);
        auto addiEntries = asmCommands_->addi(reg, zero, Immediate(resultLengthShift));                    // @0x153400
        for (auto& e : addiEntries) results->assemblers_.push_back(std::move(e));

        auto asmEntry = asmCommands_->suser(reg, detail::AddressImpl<unsigned int>(0x7a));                  // @0x1534e0
        results->assemblers_.push_back(std::move(asmEntry));
    }

    return results;                                                                                         // @0x153580
}
std::shared_ptr<EvalResults> CustomFunctions::resetRTLoggerTimestamp(                                                                                                        // @0x153f90 (700B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("resetRTLoggerTimestamp", static_cast<AwgDeviceType>(0x1f6));
    if (!args.empty())
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x42), std::string("resetRTLoggerTimestamp")));
    auto results = std::make_shared<EvalResults>(VarType(1));
    AsmRegister reg(0);
    // Address depends on device type: 0x62 for UHFQA (type 4), else 0x6d
    unsigned int addr = (config_->deviceType == static_cast<AwgDeviceType>(4)) ? 0x62u : 0x6du;
    auto asmEntry = asmCommands_->st(reg, detail::AddressImpl<unsigned int>(addr));
    results->assemblers_.push_back(std::move(asmEntry));
    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::configFreqSweep(                                                                                                            // @0x154240 (~5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("configFreqSweep", static_cast<AwgDeviceType>(0x1f8));

    // Requires exactly 3 args (0xa8 bytes / sizeof(EvalResultValue))          // @0x1542b3
    if (args.size() != 3)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x43), std::string("configFreqSweep")));

    auto results = std::make_shared<EvalResults>(VarType(1));

    // Extract args[0], args[1], args[2] — the three are treated as EvalResultValues
    auto const& arg0 = args[0];  // oscillator index
    auto const& arg1 = args[1];  // start frequency
    auto const& arg2 = args[2];  // step frequency (or end frequency)

    // Check none are VarType(2) (register-bound)                              // @0x15468c
    if (static_cast<int>(arg0.varType_) == 2 ||
        static_cast<int>(arg1.varType_) == 2 ||
        static_cast<int>(arg2.varType_) == 2) {
        // TODO: throw error — register-bound args not supported               // @0x1551b9
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x3e), std::string("configFreqSweep")));
    }

    // arg0 must be non-negative and < devConst_->field_84 - 1                 // @0x1546bf
    double oscIndex = arg0.value_.toDouble();
    if (oscIndex < 0.0)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x68)));           // @0x1550ed

    if (oscIndex > static_cast<double>(devConst_->numDIOBits - 1))
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x68)));

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
    //   HDAWG/SHFSG/SHFQC (0x40/0x80/0x100): "osc/<index>/freq"             // @0x154cb3
    //   SHFQA/PQSC (0x10/0x20): "/dev.../<awgIndex>/..."                      // @0x154de9
    // lookupNode(path) + addNodeAccess(item, AccessMode(2))                   // @0x154d7f
    auto dt = static_cast<int>(config_->deviceType);
    std::string nodePath;
    if (dt == 0x40 || dt == 0x80 || dt == 0x100) {
        nodePath = "osc/" + std::to_string(oscIntVal) + "/freq";               // @0x154cca
    } else if (dt == 0x10 || dt == 0x20) {
        std::string awgIdx = std::to_string(config_->unknown_20);                // @0x154df3 [config+0x20]
        nodePath = "/dev.../oscs/" + awgIdx + "/freq/" + std::to_string(oscIntVal);  // TODO: exact path format
    }
    if (!nodePath.empty()) {
        auto nodeItem = lookupNode(nodePath);
        addNodeAccess(nodeItem, static_cast<AccessMode>(2));
    }

    // TODO: Additional node path construction for more device types            // @0x154fbf
    // and cleanup of local EvalResultValue copies

    return results;
}
std::shared_ptr<EvalResults> CustomFunctions::setSweepStep(                                                                                                               // @0x155640 (~5KB)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("setSweepStep", static_cast<AwgDeviceType>(0x1f8));

    // Requires exactly 2 args (0x70 bytes / sizeof(EvalResultValue))               // @0x1556a2
    if (args.size() != 2)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x44),
                                  std::string("setSweepStep"), 2, args.size()));     // @0x156586

    auto results = std::make_shared<EvalResults>(VarType(1));                        // @0x1556b8

    auto const& arg0 = args[0];  // oscillator index
    auto const& arg1 = args[1];  // sweep step value

    // arg0 must not be register-bound                                               // @0x155853
    if (static_cast<int>(arg0.varType_) == 2)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x40),
                                  std::string("setSweepStep")));                     // @0x1565f9

    // arg0 must be in range [0, devConst_->numDIOBits - 1]                          // @0x155860
    double oscIndex = arg0.value_.toDouble();
    if (oscIndex < 0.0)
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x6e), 0,
                                  std::string("setSweepStep")), 0);                  // @0x15652f
    if (oscIndex > static_cast<double>(devConst_->numDIOBits - 1))
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x6e), 0,
                                  std::string("setSweepStep")), 0);

    // Branch on arg1.varType                                                        // @0x1558a9
    if (static_cast<int>(arg1.varType_) == 2) {
        // arg1 is register-bound: suser(arg1.register, 0x8d)                       // @0x1558b2
        auto suserEntry = asmCommands_->suser(arg1.reg_, 0x8d);
        results->assemblers_.push_back(std::move(suserEntry));
    } else if ((static_cast<int>(arg1.varType_) & ~2) == 4) {
        // arg1 is a numeric value: validate >= 0, then addi                         // @0x15594e
        double stepVal = arg1.value_.toDouble();
        if (stepVal < 0.0)
            throw CustomFunctionsValueException(
                ErrorMessages::format(static_cast<ErrorMessageT>(0x6e), 1,
                                      std::string("setSweepStep")), 0);              // @0x156649

        int regNum1 = Resources::getRegisterNumber();                                // @0x15596b
        AsmRegister reg1(regNum1);
        AsmRegister r0(0);
        int stepInt = arg1.value_.toInt();
        auto addiEntries = asmCommands_->addi(reg1, r0, Immediate(stepInt));
        results->assemblers_.insert(results->assemblers_.end(),
                                    addiEntries.begin(), addiEntries.end());          // @0x1559c6
    }

    // Common path: addi(reg2, R0, arg0.toInt()) into assemblers_                    // @0x155ac8
    int regNum2 = Resources::getRegisterNumber();
    AsmRegister reg2(regNum2);
    AsmRegister r0(0);
    int oscIntVal = arg0.value_.toInt();                                             // @0x155b02
    auto addiEntries2 = asmCommands_->addi(reg2, r0, Immediate(oscIntVal));
    results->assemblers_.insert(results->assemblers_.end(),
                                addiEntries2.begin(), addiEntries2.end());            // @0x155b1f

    // setValue(VarType(2), regNum2)                                                  // @0x155c41
    results->setValue(VarType(2), static_cast<int>(reg2));

    // suser(reg2, 0x8c) -> push to assemblers_                                     // @0x155c50
    auto suserEntry2 = asmCommands_->suser(reg2, 0x8c);
    results->assemblers_.push_back(std::move(suserEntry2));

    // addWaitCycles(10, results, res)                                               // @0x155d78
    addWaitCycles(10, results, res);

    // Device-type-dependent node path construction                                  // @0x155e06
    auto dt = static_cast<int>(config_->deviceType);
    std::string nodePath;
    if (dt == 0x10 || dt == 0x20) {
        // SHFSG/SHFQC: "qachannels/<awgIndex>/oscs/<oscIndex>/freq"                // @0x155e2b
        std::string awgIdx = std::to_string(config_->unknown_20);
        nodePath = "qachannels/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (dt == 0x8) {
        // HDAWG: "generators/<awgIndex>/oscs/<oscIndex>/freq"                       // @0x1560fb
        std::string awgIdx = std::to_string(config_->unknown_20);
        nodePath = "generators/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (dt == 0x40 || dt == 0x80 || dt == 0x100) {
        // SHFQA/PQSC: "oscs/<oscIndex>/freq"                                       // @0x156019
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
    checkFunctionSupported("setOscFreq", static_cast<AwgDeviceType>(0x1f8));

    // Requires exactly 2 args (0x70 bytes / sizeof(EvalResultValue))               // @0x156ad1
    if (args.size() != 2)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x44),
                                  std::string("setOscFreq"), 2, args.size()));       // @0x1579e4

    auto results = std::make_shared<EvalResults>(VarType(1));                        // @0x156ae7

    auto const& arg0 = args[0];  // oscillator index
    auto const& arg1 = args[1];  // frequency

    // Neither arg may be register-bound (VarType == 2)                              // @0x156c88
    if (static_cast<int>(arg0.varType_) == 2 ||
        static_cast<int>(arg1.varType_) == 2) {
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x40),
                                  std::string("setOscFreq")));                       // @0x157a54
    }

    // arg0 must be in range [0, devConst_->numDIOBits - 1]                          // @0x156c9e
    double oscIndex = arg0.value_.toDouble();
    if (oscIndex < 0.0)
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x6e), 0,
                                  std::string("setOscFreq")), 0);                    // @0x15798d
    if (oscIndex > static_cast<double>(devConst_->numDIOBits - 1))
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x6e), 0,
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
    auto suserEntry2 = asmCommands_->suser(reg2, 0x8c);
    results->assemblers_.push_back(std::move(suserEntry2));

    // addWaitCycles(10, results, res)                                               // @0x1572de
    addWaitCycles(10, results, res);

    // Device-type-dependent node path construction                                  // @0x15736c
    int oscIntVal = arg0.value_.toInt();
    auto dt = static_cast<int>(config_->deviceType);
    std::string nodePath;
    if (dt == 0x10 || dt == 0x20) {
        // SHFSG/SHFQC: "qachannels/<awgIndex>/oscs/<oscIndex>/freq"                // @0x157391
        std::string awgIdx = std::to_string(config_->unknown_20);
        nodePath = "qachannels/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (dt == 0x8) {
        // HDAWG: "generators/<awgIndex>/oscs/<oscIndex>/freq"                       // @0x15765e
        std::string awgIdx = std::to_string(config_->unknown_20);
        nodePath = "generators/" + awgIdx + "/oscs/" + std::to_string(oscIntVal) + "/freq";
    } else if (dt == 0x40 || dt == 0x80 || dt == 0x100) {
        // SHFQA/PQSC: "oscs/<oscIndex>/freq"                                       // @0x15757c
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
    checkFunctionSupported("configureFeedbackProcessing", static_cast<AwgDeviceType>(0x1f2));  // @0x157ec7

    // Requires exactly 4 args (0xe0 bytes / sizeof(EvalResultValue))               // @0x157ee8
    if (args.size() != 4)
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x44),
                                  std::string("configureFeedbackProcessing"),
                                  4, args.size()));                                  // @0x158dbd

    auto results = std::make_shared<EvalResults>(VarType(1));                        // @0x157efd

    auto const& arg0 = args[0];  // source (feedback source index)
    auto const& arg1 = args[1];  // shift
    auto const& arg2 = args[2];  // number of bits
    auto const& arg3 = args[3];  // threshold

    // Args 1, 2 (from arg1.varType), and 3 must not be register-bound              // @0x1584ba
    if (static_cast<int>(arg1.varType_) == 2 ||
        static_cast<int>(arg2.varType_) == 2 ||
        static_cast<int>(arg3.varType_) == 2) {
        throw CustomFunctionsException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x40),
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x6e), 0,
                                  std::string("configureFeedbackProcessing")), 0);   // @0x158c27
    }

    // Validate arg1 (shift): must be in [0, 32)                                     // @0x158697
    int shiftVal = arg1.value_.toInt();
    if (shiftVal < 0 || shiftVal >= 0x20) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x6e), 1,
                                  std::string("configureFeedbackProcessing")), 0);   // @0x158ca9
    }

    // Validate arg2 (number of bits): must be in (0, 17)                            // @0x1586c0
    int numBitsVal = arg2.value_.toInt();
    if (numBitsVal <= 0 || numBitsVal >= 0x11) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x6e), 2,
                                  std::string("configureFeedbackProcessing")), 0);   // @0x158d05
    }

    // Validate arg3 (threshold): must be in [0, 0x1000)                             // @0x1586e1
    int thresholdVal = arg3.value_.toInt();
    if (thresholdVal < 0 || thresholdVal >= 0x1000) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0x6e), 3,
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x9d), cmdName_));
    }

    // @0x15d7e0..0x15d817: Pre-scan loop.
    // Split args into two regions: String/Const args at front, others after.
    // Also accumulate hasMarker_ from VarSubType==2 entries.
    auto it = args.begin();
    auto end = args.end();
    VarType firstType = args.front().varType_;                 // @0x15d7ec

    auto boundary = it;
    for (auto scan = it; scan != end; ++scan) {
        if (scan->varSubType_ == static_cast<VarSubType>(2)) {
            hasMarker_ = true;                                 // @0x15d819
        }
        if (scan->varType_ == VarType(5) || scan->varType_ == VarType(3)) {
            // String or Const — part of the implicit-channel region
            ++boundary;
        }
        // For non-String/Const, don't advance boundary
    }

    // @0x15d81d..0x15d854: Dispatch on first arg's type
    int channelCount;
    if (firstType == VarType(5) || firstType == VarType(3)) {
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0x9e),
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
                ErrorMessages::format(static_cast<ErrorMessageT>(0xee), cmdName_),
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
                    syntheticArg.varType_ = VarType(4);
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
                ErrorMessages::format(static_cast<ErrorMessageT>(0xee), cmdName_),
                static_cast<size_t>(it - begin));
        }

        // Inner loop: consume channel-number (Cvar) args, then one waveform arg
        while (true) {
            if (it->varType_ == VarType(4)) {                 // @0x1700c9
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
                        ErrorMessages::format(static_cast<ErrorMessageT>(0xec),
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
                            ErrorMessages::format(static_cast<ErrorMessageT>(0xf0), channelNum),
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
                        ErrorMessages::format(static_cast<ErrorMessageT>(0xed),
                                              waveName, 1,
                                              static_cast<int>(wf->signal.channels())),
                        static_cast<size_t>(it - begin));
                }
            }

            // Emit WaveAssignments for all accumulated channel groups  @0x170400+
            for (auto& [groupIdx, channels] : groupChannels) {
                auto& assignments = waveAssignments_[groupIdx];

                WaveAssignment wa;
                wa.type = static_cast<int>(it->varType_);
                wa.subType = static_cast<int>(it->varSubType_);
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

// PlayArgs::secureLoadWaveform @0x1711a0 — stub
// Loads a waveform by name from wavetable_, throwing on failure.
std::shared_ptr<WaveformFront> PlayArgs::secureLoadWaveform(
    std::string const& name, size_t /*argIndex*/) const {             // @0x1711a0
    std::optional<std::string> optName(name);
    auto wf = wavetable_->getWaveformByName(optName);
    if (!wf) {
        throw CustomFunctionsValueException(
            ErrorMessages::format(static_cast<ErrorMessageT>(0xe9), name), 0);
    }
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
            if (wa.subType == 2)                               // @0x15da7f: marker → break inner
                break;
            if (wa.type == 4)                                  // @0x15da86: type 4 → skip
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
                    ErrorMessages::format(static_cast<ErrorMessageT>(0xe9), name), 0);
            }

            if (!waveform->file && waveform->thirdString.empty()) {
                // @0x15db49: throw error 0xea — waveform has no data
                throw CustomFunctionsValueException(
                    ErrorMessages::format(static_cast<ErrorMessageT>(0xea)), 0);
            }

            // @0x15db5f: read signal length at waveform+0xD0 (= signal.length_)
            uint32_t len = static_cast<uint32_t>(waveform->signal.length());
            maxLen = std::max(maxLen, len);                    // @0x15db66
        }
    }
    return static_cast<int64_t>(maxLen);
}

// PlayArgs::addChannelWave @0x170ec0 — stub
void PlayArgs::addChannelWave(int /*channel*/, EvalResultValue const& /*val*/) {  // @0x170ec0
    // TODO: ~500B function
}

// ============================================================================
// Free functions
// ============================================================================

std::vector<EvalResultValue> parseOptionalString(std::vector<EvalResultValue>& /*args*/) {  // @0x15d3e0
    // ~480B function — extracts optional string arg from the end of args vector.
    // Stub: returns empty (no optional string parsed).
    return {};
}

int getPlayRate(EvalResultValue const& /*val*/, std::string const& /*name*/, bool /*strict*/) {  // @0x163730
    // ~512B function — extracts play rate from an EvalResultValue.
    // Stub: returns 0 (default rate).
    return 0;
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
            ErrorMessages::format(static_cast<ErrorMessageT>(0xee), cmdName),
            itemIndex);
    }
    return rate;
}

} // namespace zhinst
