// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CustomFunctions — implementation of 12 analyzed methods + exception classes.
//
// Remaining ~80 SeqC built-in function bodies are stubbed with TODO + addresses.
// ============================================================================

#include "zhinst/custom_functions.hpp"
#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/resources.hpp"
#include "zhinst/types.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace zhinst {

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

    // TODO: actual NodeMap likely exposes retrieve()/contains()/etc.
    // Method observed: NodeMap::retrieve(string const&) const @0x1c55d0
    //   — returns std::pair<NodeMapData*, NodeMapItem-tail-fields> or similar.
    NodeMapItem retrieve(std::string const& path) const;  // @0x1c55d0

    std::map<std::string, NodeMapItem> entries_;
};

// ============================================================================
// EvalResultValue
// ============================================================================

EvalResultValue::~EvalResultValue() {  // @0x15c820
    // Delegates to Value::~Value() logic — if the embedded value's variant
    // holds a string (abs(which) >= 3), free the heap string if long.
    value_.~Value();
}

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
// NodeMapData hierarchy
// ============================================================================

NodeMapData::~NodeMapData() {}  // @0x1c5720

VirtAddrNodeMapData::VirtAddrNodeMapData(VirtAddrNodeMapData const& other) {  // @0x1c4dc0
    // TODO: copy fields — layout unknown
    (void)other;
}

VirtAddrNodeMapData::~VirtAddrNodeMapData() {}  // @0x1c4ec0

bool VirtAddrNodeMapData::compareEq(NodeMapData const& /*other*/) const {  // @0x1c4cc0
    // TODO: stub
    return false;
}

size_t VirtAddrNodeMapData::hash() const {  // @0x1c4f10
    // TODO: stub
    return 0;
}

NodeMapData* VirtAddrNodeMapData::clone() const {  // @0x1c51e0
    // TODO: stub
    return nullptr;
}

void VirtAddrNodeMapData::getJson(boost::json::object& /*obj*/) const {  // @0x1c5240
    // TODO: stub
}

DirectAddrNodeMapData::~DirectAddrNodeMapData() {}  // @0x1c5730

bool DirectAddrNodeMapData::compareEq(NodeMapData const& /*other*/) const {  // @0x1c5460
    // TODO: stub
    return false;
}

size_t DirectAddrNodeMapData::hash() const {  // @0x1c5370
    // TODO: stub
    return 0;
}

NodeMapData* DirectAddrNodeMapData::clone() const {  // @0x1c53c0
    // TODO: stub
    return nullptr;
}

void DirectAddrNodeMapData::getJson(boost::json::object& /*obj*/) const {  // @0x1c5400
    // TODO: stub
}

// ============================================================================
// NodeMapItem
// ============================================================================

bool NodeMapItem::operator==(NodeMapItem const& other) const {  // @0x1c5490
    // TODO: likely delegates to data->compareEq(*other.data) plus field comparison
    if (data && other.data)
        return data->compareEq(*other.data) && typeIdx == other.typeIdx;
    return data == other.data && typeIdx == other.typeIdx;
}

uint32_t NodeMapItem::fastAddress() const {  // @0x1c5470
    return fastAddr;
}

boost::json::value NodeMapItem::toJson() const {  // @0x1c54f0
    // TODO: stub
    return {};
}

size_t NodeMapItem::size() const {  // @0x1c55a0
    // TODO: stub
    return 0;
}

// ============================================================================
// MathCompiler — ctor + method stubs
// ============================================================================

MathCompiler::MathCompiler() {  // @0x1c2250
    // The binary inlines ~28 single-arg emplace_unique sequences and
    // ~5 multi-arg ones, each of the form:
    //   construct stack string "name"
    //   __tree::__emplace_unique_key_args<string, piecewise_construct_t,
    //       tuple<string&&>, tuple<>>(...)
    //   write &MathCompiler::fn into the new node's std::function slot (+0x40)
    // (See @0x1c2295..0x1c3517 for the full sequence.)
    //
    // Semantically equivalent reconstruction below — the compiler will lower
    // these emplaces into the same __tree calls.
    using namespace std::placeholders;
    auto bind1 = [this](double (MathCompiler::*p)(double)) {
        return std::bind(p, this, _1);
    };
    auto bindN = [this](double (MathCompiler::*p)(std::vector<double> const&)) {
        return std::bind(p, this, _1);
    };
    singleArgFns_.emplace("abs",   bind1(&MathCompiler::abs));
    singleArgFns_.emplace("acos",  bind1(&MathCompiler::acos));
    singleArgFns_.emplace("acosh", bind1(&MathCompiler::acosh));
    singleArgFns_.emplace("asin",  bind1(&MathCompiler::asin));
    singleArgFns_.emplace("asinh", bind1(&MathCompiler::asinh));
    singleArgFns_.emplace("atan",  bind1(&MathCompiler::atan));
    singleArgFns_.emplace("atanh", bind1(&MathCompiler::atanh));
    singleArgFns_.emplace("ceil",  bind1(&MathCompiler::ceil));
    singleArgFns_.emplace("cos",   bind1(&MathCompiler::cos));
    singleArgFns_.emplace("cosh",  bind1(&MathCompiler::cosh));
    singleArgFns_.emplace("exp",   bind1(&MathCompiler::exp));
    singleArgFns_.emplace("floor", bind1(&MathCompiler::floor));
    singleArgFns_.emplace("ln",    bind1(&MathCompiler::ln));
    singleArgFns_.emplace("log",   bind1(&MathCompiler::log));
    singleArgFns_.emplace("log2",  bind1(&MathCompiler::log2));
    singleArgFns_.emplace("log10", bind1(&MathCompiler::log10));
    singleArgFns_.emplace("round", bind1(&MathCompiler::round));
    singleArgFns_.emplace("sign",  bind1(&MathCompiler::sign));
    singleArgFns_.emplace("sin",   bind1(&MathCompiler::sin));
    singleArgFns_.emplace("sinh",  bind1(&MathCompiler::sinh));
    singleArgFns_.emplace("sqrt",  bind1(&MathCompiler::sqrt));
    singleArgFns_.emplace("tan",   bind1(&MathCompiler::tan));
    singleArgFns_.emplace("tanh",  bind1(&MathCompiler::tanh));

    multiArgFns_.emplace("avg", bindN(&MathCompiler::avg));
    multiArgFns_.emplace("max", bindN(&MathCompiler::max));
    multiArgFns_.emplace("min", bindN(&MathCompiler::min));
    multiArgFns_.emplace("pow", bindN(&MathCompiler::pow));
    multiArgFns_.emplace("sum", bindN(&MathCompiler::sum));
}

// MathCompiler::~MathCompiler @0x1592f0 — destroys multiArgFns_ then tail-calls
// singleArgFns_'s tree-destroy. Implicitly defaulted; reverse declaration order
// of the two maps gives the correct dtor sequence.

// Single-argument functions — mostly trivial wrappers around <cmath>
double MathCompiler::abs(double x)   { return std::fabs(x); }      // @0x1c3520
double MathCompiler::acos(double x)  { return std::acos(x); }      // @0x1c3530
double MathCompiler::acosh(double x) { return std::acosh(x); }     // @0x1c35f0
double MathCompiler::asin(double x)  { return std::asin(x); }      // @0x1c36b0
double MathCompiler::asinh(double x) { return std::asinh(x); }     // @0x1c3770
double MathCompiler::atan(double x)  { return std::atan(x); }      // @0x1c3780
double MathCompiler::atanh(double x) { return std::atanh(x); }     // @0x1c3790
double MathCompiler::cos(double x)   { return std::cos(x); }       // @0x1c3850
double MathCompiler::cosh(double x)  { return std::cosh(x); }      // @0x1c3860
double MathCompiler::exp(double x)   { return std::exp(x); }       // @0x1c3870
double MathCompiler::ln(double x)    { return std::log(x); }       // @0x1c3880
double MathCompiler::log(double x)   { return std::log(x); }       // @0x1c3940
double MathCompiler::log2(double x)  { return std::log2(x); }      // @0x1c3a00
double MathCompiler::log10(double x) { return std::log10(x); }     // @0x1c3ac0
double MathCompiler::sign(double x)  { return (x > 0) - (x < 0); } // @0x1c3b80
double MathCompiler::sin(double x)   { return std::sin(x); }       // @0x1c3ba0
double MathCompiler::sinh(double x)  { return std::sinh(x); }      // @0x1c3bb0
double MathCompiler::sqrt(double x)  { return std::sqrt(x); }      // @0x1c3bc0
double MathCompiler::tan(double x)   { return std::tan(x); }       // @0x1c3be0
double MathCompiler::tanh(double x)  { return std::tanh(x); }      // @0x1c3bf0
double MathCompiler::ceil(double x)  { return std::ceil(x); }      // @0x1c3c00
double MathCompiler::round(double x) { return std::round(x); }     // @0x1c3c10
double MathCompiler::floor(double x) { return std::floor(x); }     // @0x1c3c20

// Multi-argument functions
double MathCompiler::avg(std::vector<double> const& v) {  // @0x1c3c30
    // Binary computes sum / size unconditionally (no empty guard); on empty,
    // result is 0/0 = NaN. The integer-to-double conversion at 1c3c64-1c3c81
    // is the standard libc++ uint64→double sequence.
    double s = 0.0;
    for (auto x : v) s += x;
    return s / static_cast<double>(v.size());
}

double MathCompiler::max(std::vector<double> const& v) {  // @0x1c3c90
    // Binary is a manual 2-element-bounded loop using maxsd / cmova.
    // std::max_element is the semantic equivalent.
    return *std::max_element(v.begin(), v.end());
}

double MathCompiler::min(std::vector<double> const& v) {  // @0x1c3cf0
    return *std::min_element(v.begin(), v.end());
}

double MathCompiler::pow(std::vector<double> const& v) {  // @0x1c3d50
    // Binary: if (size != 2) throw MathCompilerException(format(FuncExactly2Args, "pow"));
    // else jmp pow@plt with v[0], v[1].
    if (v.size() != 2) {
        throw MathCompilerException(
            ErrorMessages::format<char const*>(ErrorMessageT::FuncExactly2Args, "pow"));
    }
    return std::pow(v[0], v[1]);
}

double MathCompiler::sum(std::vector<double> const& v) {  // @0x1c3e10
    double s = 0.0;
    for (auto x : v) s += x;
    return s;
}

bool MathCompiler::functionExists(std::string const& name, size_t argCount, bool strict) const {  // @0x1c3e50
    // Disassembly logic:
    //   bl = strict (input)
    //   it = singleArgFns_.find(name)
    //   if (it != end)  al = (argCount == 1)
    //   else            it2 = multiArgFns_.find(name)
    //                   if (it2 != end)  al = (argCount != 0)
    //                   else             return false   (ebx <- 0)
    //   return bl | al
    bool found = false;
    auto it = singleArgFns_.find(name);
    if (it != singleArgFns_.end()) {
        found = (argCount == 1);
    } else {
        auto it2 = multiArgFns_.find(name);
        if (it2 == multiArgFns_.end()) {
            return false;
        }
        found = (argCount != 0);
    }
    return strict | found;
}

double MathCompiler::call(std::string const& name, std::vector<double> const& args) {  // @0x1c3eb0
    // Disassembly logic:
    //   it = singleArgFns_.find(name)
    //   if (found && args.size() == 1)  return it->second(args[0])
    //   if (found && args.size() != 1)  throw FuncSingleArg, name
    //   it2 = multiArgFns_.find(name)
    //   if (found2)                     return it2->second(args)   (tail call)
    //   throw UnknownFunction, name
    auto it = singleArgFns_.find(name);
    if (it != singleArgFns_.end()) {
        if (args.size() == 1) {
            return it->second(args[0]);
        }
        throw MathCompilerException(
            ErrorMessages::format<std::string const&>(ErrorMessageT::FuncSingleArg, name));
    }
    auto it2 = multiArgFns_.find(name);
    if (it2 != multiArgFns_.end()) {
        return it2->second(args);
    }
    throw MathCompilerException(
        ErrorMessages::format<std::string const&>(ErrorMessageT::UnknownFunction, name));
}

// ============================================================================
// MathCompilerException
// ============================================================================

MathCompilerException::MathCompilerException(std::string const& msg)  // @0x1c40c0
    : msg_(msg) {}

MathCompilerException::~MathCompilerException() {}  // @0x1c4120

const char* MathCompilerException::what() const noexcept {  // @0x1c41b0
    return msg_.c_str();
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
    // TODO: likely formats message_ + argIndex_ + varName_ together
    return message_.c_str();
}

void CustomFunctionsValueException::setVarName(std::string const& name) {  // @0x210750
    varName_ = name;
}

// ============================================================================
// CustomFunctions — constructor / destructor
// ============================================================================

CustomFunctions::CustomFunctions(  // @0x12bcf0 — 19KB, registers 87 built-in functions
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
    // TODO: actual bind registration omitted — 19KB of repetitive SSO string
    // construction + std::function binding. The funcMap_ remains empty in this
    // reconstruction; call() will fall through to the find() path which won't
    // find anything. To make this functional, each line above should be:
    //   funcMap_["name"] = [this](auto& a, auto r){ return this->name(a, std::move(r)); };
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
        // TODO: validate arg count against alias info and report via error callback
        // The alias vector contains expected arg count strings or alternative names.
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
        // TODO: warningCallback_ with error message (error code 0xF5)
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
        int aligned = ((length + alignment - 1) / alignment) * alignment;
        // TODO: warningCallback_ with error message (error code 0xE7)
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
    // TODO: full implementation requires understanding AWGCompilerConfig features
    // and device class/group index dispatching.
    // Stub: delegate to Grimsel check as fallback.
    (void)mask;
    return false;
}

// oscMaskSetAllHirzel @0x15bf50
// Computes all-oscillator mask based on deviceClass/groupIndex and "MF" feature.
unsigned int CustomFunctions::oscMaskSetAllHirzel() {  // @0x15bf50
    // TODO: full implementation requires feature vector lookup + switch on device class.
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
    if (devConst_->deviceType == 2) {
        // HDAWG path
        int val = ((samples + 7) >> (rate + 7)) / 8;  // TODO: verify shift direction
        return std::max(0, val - 3);
    } else {
        return ((samples + 3) >> (rate + 3)) / 4;  // TODO: verify shift direction
    }
}

// initNodeMap @0x16b740
// Lazy-init. Dispatches to GetNodeMap<N>::get() based on deviceType.
void CustomFunctions::initNodeMap() {  // @0x16b740
    if (nodeMap_) return;  // already initialized
    // TODO: dispatch based on devConst_->deviceType to template-specialized
    // GetNodeMap<N>::get() functions, which construct a NodeMap and assign
    // nodeMap_ = std::make_unique<NodeMap>(...).
}

// getNodeAddress @0x16ba10
// Tries dynamic_cast<DirectAddrNodeMapData*>, else looks up nodeAddressMap_.
uint32_t CustomFunctions::getNodeAddress(NodeMapItem const& item) const {  // @0x16ba10
    if (auto* direct = dynamic_cast<DirectAddrNodeMapData*>(item.data)) {
        // DirectAddrNodeMapData contains the address directly
        // TODO: return direct->address_;
        (void)direct;
    }
    auto it = nodeAddressMap_.find(item);
    if (it != nodeAddressMap_.end()) {
        return it->second;
    }
    return 0;  // TODO: may throw instead
}

// getSampleClock @0x16ba80
// Reads "$DEVICE_SAMPLE_RATE" from resources_ (+0x10), fallback to devConst_->samplingRate (+0x70).
double CustomFunctions::getSampleClock() const {  // @0x16ba80
    // TODO: Try resources_->readConst("$DEVICE_SAMPLE_RATE", EDirection_Read) first
    // and extract the double value. If not available, fall back:
    return devConst_->samplingRate;
}

// getAccessModes @0x16be50
// Looks up accessModeMap_ (+0x128). Returns const set<AccessMode>&.
std::set<AccessMode> const& CustomFunctions::getAccessModes(NodeMapItem const& item) const {  // @0x16be50
    auto it = accessModeMap_.find(item);
    if (it != accessModeMap_.end()) {
        return it->second;
    }
    // TODO: binary may throw here rather than return empty set.
    // Using static empty set as fallback.
    static const std::set<AccessMode> empty;
    return empty;
}

// ============================================================================
// CustomFunctions — utility method stubs (not in the 12 analyzed set)
// ============================================================================

void CustomFunctions::checkFunctionSupported(std::string const& /*name*/, AwgDeviceType /*devType*/) {  // @0x15aeb0
    // TODO: stub
}

void CustomFunctions::checkWaveformMinLengthTrig(std::shared_ptr<WaveformFront> /*wf*/) {  // @0x15b000
    // TODO: stub
}

void CustomFunctions::checkOffspecWaveLength(std::shared_ptr<WaveformFront> /*wf*/, int /*expected*/) {  // @0x15b370
    // TODO: stub
}

bool CustomFunctions::optionAvailable(std::string const& /*option*/) {  // @0x15b9c0
    // TODO: stub
    return false;
}

NodeMapItem CustomFunctions::lookupNode(std::string const& /*path*/) {  // @0x15c530
    // TODO: stub
    return {};
}

void CustomFunctions::setWaitCyclesReg(std::vector<EvalResultValue> const& /*args*/,
                                        std::shared_ptr<EvalResults> /*results*/,
                                        std::shared_ptr<Resources> /*res*/) {  // @0x15ca90
    // TODO: stub
}

void CustomFunctions::mergeWaveforms(std::vector<EvalResultValue> const& /*args*/,
                                      short /*param2*/, bool /*param3*/,
                                      std::string const& /*name*/,
                                      int /*param5*/, bool /*param6*/) {  // @0x15e060
    // TODO: stub
}

std::shared_ptr<EvalResults> CustomFunctions::play(
    std::vector<EvalResultValue> const& /*args*/,
    std::shared_ptr<Resources> /*res*/,
    SubFunc /*subFunc*/) {  // @0x15f090
    // TODO: stub
    return nullptr;
}

std::shared_ptr<EvalResults> CustomFunctions::playIndexed(
    std::vector<EvalResultValue> const& /*args*/,
    std::shared_ptr<Resources> /*res*/,
    SubFunc /*subFunc*/) {  // @0x160e00
    // TODO: stub
    return nullptr;
}

void CustomFunctions::writeToNode(EvalResultValue /*path*/, EvalResultValue /*val*/,
                                   EvalResultValue /*type*/, std::shared_ptr<Resources> /*res*/) {  // @0x164550
    // TODO: stub
}

void CustomFunctions::addSyncCommand(std::shared_ptr<EvalResults> /*results*/,
                                      std::shared_ptr<Resources> /*res*/) {  // @0x16bb30
    // TODO: stub
}

std::string CustomFunctions::printF(std::vector<EvalResultValue> const& /*args*/,
                                     std::string const& /*fmt*/) {  // @0x16c470
    // TODO: stub
    return {};
}

void CustomFunctions::addWaitCycles(int /*cycles*/,
                                     std::shared_ptr<EvalResults> /*results*/,
                                     std::shared_ptr<Resources> /*res*/) {  // @0x16d8c0
    // TODO: stub
}

void CustomFunctions::writeLS64bit(unsigned long /*value*/, int /*reg1*/, int /*reg2*/,
                                    std::shared_ptr<EvalResults> /*results*/,
                                    std::shared_ptr<Resources> /*res*/) {  // @0x16dbb0
    // TODO: stub
}

std::shared_ptr<EvalResults> CustomFunctions::generateWaveform(
    std::string const& /*name*/,
    std::vector<EvalResultValue> const& /*args*/,
    std::shared_ptr<Resources> /*res*/) {  // @0x15a9f0
    // TODO: stub
    return nullptr;
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

// ---- Thin aux play wrappers (delegate to play/playIndexed with SubFunc TBD) ---

std::shared_ptr<EvalResults> CustomFunctions::playAuxWave(  // @0x135610
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // TODO: calls play() with aux-specific SubFunc
    checkFunctionSupported("playAuxWave", static_cast<AwgDeviceType>(5));
    return play(args, std::move(res), SubFunc::Default);  // SubFunc value TBD
}

std::shared_ptr<EvalResults> CustomFunctions::playAuxWaveIndexed(  // @0x136930
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // TODO: calls playIndexed() with aux-specific SubFunc
    checkFunctionSupported("playAuxWaveIndexed", static_cast<AwgDeviceType>(5));
    return playIndexed(args, std::move(res), SubFunc::Default);  // SubFunc value TBD
}

// ---- Thin DIO/ZSync/DigTrigger play wrappers ---

std::shared_ptr<EvalResults> CustomFunctions::playDIOWave(  // @0x1369f0
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // TODO: calls play() with DIO SubFunc
    checkFunctionSupported("playDIOWave", static_cast<AwgDeviceType>(5));
    return play(args, std::move(res), SubFunc::Default);  // SubFunc value TBD
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveDIO(  // @0x137740
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    // TODO: calls play() with DIO SubFunc
    checkFunctionSupported("playWaveDIO", static_cast<AwgDeviceType>(5));
    return play(args, std::move(res), SubFunc::Default);  // SubFunc value TBD
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveZSync(  // @0x137a50
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWaveZSync", static_cast<AwgDeviceType>(5));
    return play(args, std::move(res), SubFunc::Default);  // SubFunc value TBD
}

std::shared_ptr<EvalResults> CustomFunctions::playWaveDigTrigger(  // @0x1386a0
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("playWaveDigTrigger", static_cast<AwgDeviceType>(5));
    return play(args, std::move(res), SubFunc::Default);  // SubFunc value TBD
}

// ---- Simple 0-arg functions -----------------------------------------------

std::shared_ptr<EvalResults> CustomFunctions::waitWave(  // @0x13a980 (620B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("waitWave", static_cast<AwgDeviceType>(0x1f7));
    if (!args.empty())
        throw CustomFunctionsException("waitWave takes no arguments");  // error 0x42
    // Binary: auto result = make_shared<EvalResults>(VarType(1));
    //         asmCommands_->wwvf();  // "wait waveform finished"
    //         result->appendAsm(asmResult);
    // EvalResults not yet defined (unknown #99); returning nullptr.
    return nullptr;
}

std::shared_ptr<EvalResults> CustomFunctions::waitPlayQueueEmpty(  // @0x145240 (626B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("waitPlayQueueEmpty", static_cast<AwgDeviceType>(2));
    if (!args.empty())
        throw CustomFunctionsException("waitPlayQueueEmpty takes no arguments");  // error 0x42
    // Binary: auto result = make_shared<EvalResults>(VarType(1));
    //         asmCommands_->wwvfq();  // "wait waveform queue empty"
    //         result->appendAsm(asmResult);
    return nullptr;
}

std::shared_ptr<EvalResults> CustomFunctions::sync(  // @0x14e690 (569B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res) {
    checkFunctionSupported("sync", static_cast<AwgDeviceType>(0x1f7));
    if (!args.empty())
        throw CustomFunctionsException("sync takes no arguments");  // error 0x42
    // Binary: auto result = make_shared<EvalResults>(VarType(1));
    //         addSyncCommand(result, res);
    return nullptr;
}

std::shared_ptr<EvalResults> CustomFunctions::randomSeed(  // @0x1497c0 (384B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("randomSeed", static_cast<AwgDeviceType>(0x1f7));
    if (!args.empty())
        throw CustomFunctionsException("randomSeed takes no arguments");  // error 0xa9
    // Host-side only: seeds the TLS random object. No assembly emitted.
    // Binary: GlobalResources::random.seedRandom();
    //         return make_shared<EvalResults>();
    return nullptr;
}

std::shared_ptr<EvalResults> CustomFunctions::now(  // @0x14cbc0 (611B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    checkFunctionSupported("now", static_cast<AwgDeviceType>(5));
    if (!args.empty())
        throw CustomFunctionsException("now takes no arguments");  // error 0x42
    // Binary: auto result = make_shared<EvalResults>();
    //         AsmRegister reg(0);
    //         auto asm = asmCommands_->suser(reg, Address(0x1c));
    //         result->appendAsm(asm);
    return nullptr;
}

// ---- Formatting functions -------------------------------------------------

std::shared_ptr<EvalResults> CustomFunctions::error(  // @0x14d830 (536B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // Binary: auto result = make_shared<EvalResults>();
    //         string msg = printF(args, "error");
    //         asmCommands_->asmMessage(msg, true);
    //         result->appendAsm(asmResult);
    return nullptr;
}

std::shared_ptr<EvalResults> CustomFunctions::info(  // @0x14da50 (531B)
    std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> /*res*/) {
    // Binary: auto result = make_shared<EvalResults>();
    //         string msg = printF(args, "info");
    //         asmCommands_->asmMessage(msg, false);
    //         result->appendAsm(asmResult);
    return nullptr;
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

std::shared_ptr<EvalResults> CustomFunctions::setDIO(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }                // @0x130780 (~2KB)
std::shared_ptr<EvalResults> CustomFunctions::getDIO(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }                // @0x131040 (~1KB)
std::shared_ptr<EvalResults> CustomFunctions::getDIOTriggered(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }       // @0x131410 (~700B)
std::shared_ptr<EvalResults> CustomFunctions::getZSyncData(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }          // @0x1316f0 (~3KB)
std::shared_ptr<EvalResults> CustomFunctions::getFeedback(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }           // @0x132420 (~4KB)
std::shared_ptr<EvalResults> CustomFunctions::setID(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }                 // @0x1334a0 (~2KB)
std::shared_ptr<EvalResults> CustomFunctions::assignWaveIndex(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }       // @0x133c40 (~5KB)
std::shared_ptr<EvalResults> CustomFunctions::prefetch(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }              // @0x1351d0 (~300B)
std::shared_ptr<EvalResults> CustomFunctions::prefetchIndexed(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }       // @0x135290 (~100B)

std::shared_ptr<EvalResults> CustomFunctions::playZero(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }              // @0x1387f0 (2112B)
std::shared_ptr<EvalResults> CustomFunctions::playHold(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }              // @0x139030 (~1.8KB)
std::shared_ptr<EvalResults> CustomFunctions::wait(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }                  // @0x139760 (4640B)
std::shared_ptr<EvalResults> CustomFunctions::waitTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }           // @0x13abf0 (~2KB)
std::shared_ptr<EvalResults> CustomFunctions::waitAnaTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }        // @0x13b4b0 (~3KB)
std::shared_ptr<EvalResults> CustomFunctions::waitDigTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }        // @0x13c110 (~4KB)
std::shared_ptr<EvalResults> CustomFunctions::waitOnGrid(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }            // @0x13d000 (~900B)
std::shared_ptr<EvalResults> CustomFunctions::waitOnSync(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }            // @0x13d3a0 (~600B)
std::shared_ptr<EvalResults> CustomFunctions::waitDIOTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }        // @0x13d630 (~1.7KB)
std::shared_ptr<EvalResults> CustomFunctions::waitZSyncTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }      // @0x13dcf0 (~1.8KB)
std::shared_ptr<EvalResults> CustomFunctions::waitCntTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }        // @0x13e460 (~1.8KB)
std::shared_ptr<EvalResults> CustomFunctions::waitDemodOscPhase(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }     // @0x13eba0 (~3KB)
std::shared_ptr<EvalResults> CustomFunctions::waitSineOscPhase(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }      // @0x13f790 (~2.5KB)
std::shared_ptr<EvalResults> CustomFunctions::waitTimestamp(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }         // @0x1401c0 (~500B)
std::shared_ptr<EvalResults> CustomFunctions::resetOscPhase(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }         // @0x1403b0 (~6.5KB)
std::shared_ptr<EvalResults> CustomFunctions::setSinePhase(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }          // @0x141df0 (~4KB)
std::shared_ptr<EvalResults> CustomFunctions::incrementSinePhase(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }    // @0x142da0 (~4KB)
std::shared_ptr<EvalResults> CustomFunctions::waitDemodSample(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }       // @0x143d50 (~5.2KB)
std::shared_ptr<EvalResults> CustomFunctions::setTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }            // @0x1454c0 (1552B)
std::shared_ptr<EvalResults> CustomFunctions::getTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }            // @0x145ad0 (~1.6KB)
std::shared_ptr<EvalResults> CustomFunctions::setInternalTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }    // @0x146140 (~1.5KB)
std::shared_ptr<EvalResults> CustomFunctions::getAnaTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }         // @0x146740 (~3.2KB)
std::shared_ptr<EvalResults> CustomFunctions::getDigTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }         // @0x147420 (~3.2KB)
std::shared_ptr<EvalResults> CustomFunctions::setInt(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }                // @0x1480d0 (~2.5KB)
std::shared_ptr<EvalResults> CustomFunctions::setDouble(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }             // @0x148ac0 (~3.3KB)
std::shared_ptr<EvalResults> CustomFunctions::generate(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }              // @0x149940 (~2.8KB)
std::shared_ptr<EvalResults> CustomFunctions::setUserReg(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }            // @0x14a420 (~4KB)
std::shared_ptr<EvalResults> CustomFunctions::getUserReg(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }            // @0x14b480 (~2KB)
std::shared_ptr<EvalResults> CustomFunctions::getSweeperLength(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }      // @0x14bca0 (~1.7KB)
std::shared_ptr<EvalResults> CustomFunctions::setPrecompClear(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }       // @0x14c720 (~1KB)
std::shared_ptr<EvalResults> CustomFunctions::setWaveDIO(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }            // @0x14cae0 (~200B)
std::shared_ptr<EvalResults> CustomFunctions::at(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }                    // @0x14ce30 (~2.5KB)
std::shared_ptr<EvalResults> CustomFunctions::lock(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }                  // @0x14dc70 (1286B)
std::shared_ptr<EvalResults> CustomFunctions::unlock(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }                // @0x14e180 (1295B)
std::shared_ptr<EvalResults> CustomFunctions::getCnt(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }                // @0x14e8d0 (~1KB)
std::shared_ptr<EvalResults> CustomFunctions::waitQAResultTrigger(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }   // @0x14edc0 (~1.4KB)
std::shared_ptr<EvalResults> CustomFunctions::getQAResult(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }           // @0x14f380 (~700B)
std::shared_ptr<EvalResults> CustomFunctions::startQAResult(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }         // @0x14f620 (~2.7KB)
std::shared_ptr<EvalResults> CustomFunctions::startQAMonitor(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }        // @0x1500b0 (~2.1KB)
std::shared_ptr<EvalResults> CustomFunctions::executeTableEntry(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }     // @0x150900 (~2.7KB)
std::shared_ptr<EvalResults> CustomFunctions::setPRNGSeed(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }           // @0x1513e0 (~1.6KB)
std::shared_ptr<EvalResults> CustomFunctions::getPRNGValue(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }          // @0x151a70 (~600B)
std::shared_ptr<EvalResults> CustomFunctions::setPRNGRange(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }          // @0x151ce0 (~2.4KB)
std::shared_ptr<EvalResults> CustomFunctions::startQA(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }               // @0x152690 (~6.2KB)
std::shared_ptr<EvalResults> CustomFunctions::resetRTLoggerTimestamp(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; } // @0x153f90 (~700B)
std::shared_ptr<EvalResults> CustomFunctions::configFreqSweep(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }       // @0x154240 (~5KB)
std::shared_ptr<EvalResults> CustomFunctions::setSweepStep(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }          // @0x155640 (~5KB)
std::shared_ptr<EvalResults> CustomFunctions::setOscFreq(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }            // @0x156a70 (~5KB)
std::shared_ptr<EvalResults> CustomFunctions::configureFeedbackProcessing(std::vector<EvalResultValue> const& /*args*/, std::shared_ptr<Resources> /*res*/) { return nullptr; }  // @0x157e60 (~5.6KB)

// ============================================================================
// Free functions
// ============================================================================

std::vector<EvalResultValue> parseOptionalString(std::vector<EvalResultValue>& /*args*/) {  // @0x15d3e0
    // TODO: stub
    return {};
}

int getPlayRate(EvalResultValue const& /*val*/, std::string const& /*name*/, bool /*strict*/) {  // @0x163730
    // TODO: stub
    return 0;
}

} // namespace zhinst
