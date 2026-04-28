// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CustomFunctions — SeqC built-in function dispatch and device-specific logic.
//
// Size: 0x1E0 (480 bytes) — from __on_zero_shared_weak allocation 0x200
//       minus 0x20 control block.
//
// Ctor: 0x12bcf0    Dtor: 0x127c90
//
// Contains ~80 SeqC built-in function implementations (playWave, wait, etc.)
// bound as std::function entries in an unordered_map at +0x60.
//
// Source path (inferred): CustomFunctions.cpp
//
// Phase 16b file split (audit §C1): extracted classes moved to dedicated
// headers:
//   - EvalResultValue                       → eval_result_value.hpp
//   - NodeMapData / VirtAddr / DirectAddr   → node_map_data.hpp
//   - NodeMapItem (+ std::hash spec)         → node_map_data.hpp
//   - MathCompiler / MathCompilerException  → math_compiler.hpp
// CustomFunctions[Value]Exception remain here.
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/json.hpp>

#include <zhinst/asm_register.hpp>
#include <zhinst/eval_result_value.hpp>
#include <zhinst/math_compiler.hpp>
#include <zhinst/node_map_data.hpp>
#include <zhinst/value.hpp>

namespace zhinst {

// Forward declarations
class AsmCommands;
class AWGCompilerImpl;
struct AWGCompilerConfig;
struct DeviceConstants;
class EvalResults;
class Resources;
class WavetableFront;
class WaveformFront;
class WaveformGenerator;
enum AwgDeviceType : int;
// NodeMap — wraps std::map<std::string, NodeMapItem>; size 24B
// (used as unique_ptr<NodeMap> in CustomFunctions +0xF8)
// Originally local to custom_functions.cpp; promoted to header during Phase 22b
// file split so that multiple TUs can use it.
class NodeMap {
public:
    NodeMap() = default;
    ~NodeMap() = default;
    NodeMapItem retrieve(std::string const& path) const;  // @0x1c55d0
    static int toPhase(float value);  // @0x1c5680
    static uint64_t toFrequency(double freq, double sampleClock);  // @0x1c5630
    std::map<std::string, NodeMapItem> entries_;
};

// ============================================================================
// ExternalTriggeringMode — mutual-exclusion latch for DIO vs ZSync I/O.
//
// Binary evidence: error message for mixing (code 79) reads
//   "DIO and ZSync external triggering can't be mixed in the same program"
// and the JSON output key is "external_triggering" (values "dio"/"zsync").
//
// Underlying storage is int32_t at CustomFunctions+0x1C0.
// ============================================================================
enum class ExternalTriggeringMode : int32_t {
    None  = 0,   // unset — no I/O function compiled yet
    Dio   = 1,   // DIO family (setDIO, getDIO, getDIOTriggered, waitDIOTrigger, playDIOWave, playWaveDIO)
    ZSync = 2,   // ZSync family (getZSyncData, getFeedback, waitZSyncTrigger, playWaveZSync)
};

// ============================================================================
// AccessMode — comparable enum-like type used in set<AccessMode>
//
// Has a free function toString(AccessMode) with static table @0x9573c0.
// ============================================================================
enum class AccessMode : int32_t {
    // Verified from static SSO string table at .rodata 0x9573c0:
    //   [0] = "soft", [1] = "direct", [2] = "custom"
    // Bounds check at 0x108a3f: cmp rax, 3; jae throw
    Soft   = 0,
    Direct = 1,
    Custom = 2,
};

// Free function: toString(AccessMode)
// Static table at .rodata 0x9573c0
std::string toString(AccessMode mode);

inline bool operator<(AccessMode a, AccessMode b) {
    return static_cast<int32_t>(a) < static_cast<int32_t>(b);
}

// ============================================================================
// CustomFunctionsException — 0x20 bytes
//   vtable(+0x00) + string message_(+0x08)
//   what() returns message_ or "CustomFunctions Exception" if empty.
// ============================================================================
class CustomFunctionsException : public std::exception {
    std::string message_;  // +0x08
public:
    explicit CustomFunctionsException(std::string const& msg);  // @0x15a4c0
    ~CustomFunctionsException() override;                        // @0x15a520, D0 @0x16e6c0
    const char* what() const noexcept override;                  // @0x16e710
};

// ============================================================================
// CustomFunctionsValueException — 0x40 bytes
//   vtable(+0x00) + string message_(+0x08) + size_t argIndex_(+0x20)
//   + string varName_(+0x28)
// ============================================================================
class CustomFunctionsValueException : public std::exception {
    std::string message_;   // +0x08
    size_t      argIndex_;  // +0x20
    std::string varName_;   // +0x28
public:
    CustomFunctionsValueException(std::string const& msg, size_t argIndex);  // @0x163d00
    ~CustomFunctionsValueException() override;                                // @0x163d70, D0 @0x172f70
    const char* what() const noexcept override;                               // @0x172fd0
    void setVarName(std::string const& name);                                 // @0x210750
    std::string const& varName() const { return varName_; }                    // inline accessor
    size_t argIndex() const { return argIndex_; }                              // inline accessor
};

// ============================================================================
// PlayArgs — helper for parsing play-function arguments
//
// parse @0x15d7b0, addChannelWave @0x170ec0
// ============================================================================
// PlayArgs — helper struct for parsing play-function arguments
//
// Ctor @0x15d600   Dtor @0x15efe0   Size: 0x80 (128 bytes)
//
// Field layout (fully reconstructed from ctor + dtor):
//
// Offset  Size  Type                                     Name            Init
// ------  ----  ----                                     ----            ----
// 0x00    16    shared_ptr<WavetableFront>               wavetable_      copied from arg
// 0x10    32    std::function<void(string const&)>       reportWarning_  copied from arg
//               (inline buf at +0x10, invoker ptr at +0x30)
// 0x30    8     (part of std::function — invoker/manager ptr)
// 0x38    8     (padding / std::function tail)
// 0x40    24    std::string                              cmdName_        copied from arg
// 0x58    2     uint16_t                                 channelsPerGroup_  config[0x14 + indexed*2]
// 0x5A    2     uint16_t                                 totalChannels_  channelsPerGroup_ * numChannelGroups
// 0x5C    4     (padding)
// 0x60    24    vector<vector<WaveAssignment>>            waveAssignments_  numChannelGroups empty inner vecs
// 0x78    1     bool                                     hasMarker_      false
//               (read/written by parse() at 0x15d7d5/0x15d819)
// 0x79    7     (padding to 0x80)
//
// IMPORTANT: Previous "partial field layout" listing offsets +0x1A8, +0x1C0,
// +0x2B0, +0x2B8, +0x2A0 was WRONG.  Those were stack-local variables in
// play() (at rbp-0x78, rbp-0x60, etc.) adjacent to — but not inside — the
// PlayArgs object at rbp-0x220.  Corrected offsets for the play() locals:
//   rbp-0x78 = SubFunc param (int, stored at 15f0c7)
//   Other locals are separate from PlayArgs entirely.
//
// Ctor logic (0x15d600):
//   1. Copy shared_ptr<WavetableFront> → +0x00, atomic inc refcount
//   2. Copy std::function (callback) → +0x10..+0x37
//   3. Copy string cmdName → +0x40..+0x57 (SSO or heap)
//   4. channelsPerGroup_ = config.channelsPerGroup[indexed]
//   5. totalChannels_    = channelsPerGroup_ * config.numChannelGroups
//   6. Allocate waveAssignments_ with numChannelGroups empty inner vectors
//      (element size 0x18 = sizeof(vector<WaveAssignment>)), memset to 0
//   7. hasMarker_ = false
//
// Dtor (0x15efe0) destroys in reverse:
//   1. waveAssignments_ (+0x60): destroy inner vecs, free backing array
//   2. cmdName_ (+0x40): free if heap-allocated
//   3. reportWarning_ (+0x10/+0x30): invoke function manager destructor
//   4. wavetable_ (+0x00/+0x08): decrement shared_ptr refcount
// ============================================================================
struct PlayArgs {
    // Constructor @0x15d600
    PlayArgs(AWGCompilerConfig const& config,
             std::shared_ptr<WavetableFront> wavetable,
             std::function<void(std::string const&)> reportWarning,
             std::string const& cmdName,
             bool indexed);
    ~PlayArgs();  // @0x15efe0

    // Returns iterator past last consumed arg
    std::vector<EvalResultValue>::const_iterator
    parse(std::vector<EvalResultValue> const& args);                  // @0x15d7b0

    int64_t getMaxSampleLength() const;                               // @0x15d9f0
    void addChannelWave(int channel, EvalResultValue const& val);     // @0x170ec0

    struct WaveAssignment {
        // Stride 0x50 (80 bytes) per entry — confirmed by playAuxWave's
        // inner copy loop @0x135990..0x1359d6 which copies exactly 0x38
        // bytes from WA+0..WA+0x38 into a vector<EvalResultValue> slot.
        //
        // CORRECTION 2026-04-24 (Phase 21a, playAuxWave reconstruction):
        // The earlier `int type; int subType; EvalResultValue value;`
        // layout summed to 0x58, contradicting the 0x50 stride.  In fact
        // the WaveAssignment STARTS with the EvalResultValue directly —
        // the previously-named `type`/`subType` fields are just
        // EvalResultValue::varType_ and varSubType_ (the first 8 bytes of
        // an EvalResultValue).  Confirmed by:
        //   * @0x135854: `lea rsi, [rbx+0x8]` then call Value::toString
        //     ⇒ wa.value.value_ lives at WA+0x08 ⇒ wa.value lives at WA+0
        //   * @0x135990 inner copy: 0x38 bytes from WA+0 → vec<EvalResultValue>
        EvalResultValue value;       // +0x00 (0x38 bytes)
        std::vector<int> bits;       // +0x38, channel bit assignments
        // total = 0x50 ✓

        WaveAssignment() = default;
        WaveAssignment(WaveAssignment const& o);  // @0x171c00 — variant-aware copy
        WaveAssignment& operator=(WaveAssignment const&) = default;
        WaveAssignment(WaveAssignment&&) = default;
        WaveAssignment& operator=(WaveAssignment&&) = default;
    };
    // 0x50 on libc++ (binary), larger on libstdc++ due to Value's string union.
    static_assert(sizeof(WaveAssignment) >= 0x50,
                  "PlayArgs::WaveAssignment must be at least 0x50 bytes");

    // --- Fields (0x80 bytes total) ---
    std::shared_ptr<WavetableFront>              wavetable_;          // +0x00
    std::function<void(std::string const&)>      reportWarning_;      // +0x10 (48B in libc++, but only 32B+8B used here; +0x30 = invoker)
    std::string                                  cmdName_;            // +0x40
    uint16_t                                     channelsPerGroup_;   // +0x58
    uint16_t                                     totalChannels_;      // +0x5A
    // 4 bytes padding                                                // +0x5C
    std::vector<std::vector<WaveAssignment>>     waveAssignments_;    // +0x60
    bool                                         hasMarker_;          // +0x78

private:
    // parse() helpers
    int parseImplicitChannels(
        std::vector<EvalResultValue>::const_iterator begin,
        std::vector<EvalResultValue>::const_iterator end);            // @0x16fb30
    int parseExplicitChannels(
        std::vector<EvalResultValue>::const_iterator begin,
        std::vector<EvalResultValue>::const_iterator end);            // @0x170000
    std::shared_ptr<WaveformFront> secureLoadWaveform(
        std::string const& name, size_t argIndex) const;             // @0x1711a0
};

// Free functions related to CustomFunctions argument parsing
std::optional<std::string> parseOptionalString(std::vector<EvalResultValue>& args);  // @0x15d3e0
int getPlayRate(EvalResultValue const& val, std::string const& name, bool strict);     // @0x163730

// parseOptionalRate — parse optional rate argument from arg list     @0x163980
int parseOptionalRate(
    std::vector<EvalResultValue>::const_iterator begin,
    std::vector<EvalResultValue>::const_iterator end,
    std::vector<EvalResultValue>::const_iterator parseEnd,
    std::string const& cmdName,
    bool strict);

// ============================================================================
// CustomFunctions — 0x1E0 (480) bytes
//
// Member layout (from REAL dtor @0x127c90 and ctor @0x12bcf0).
//
// HISTORICAL NOTE: An earlier pass mis-attributed offsets +0xF8/+0x100/+0x128/+0x150
// /+0x168 to types derived from a destructor at 0x1306c1 — but 0x1306c1 belongs
// to a DIFFERENT class (pybind11::detail::internals or similar), NOT CustomFunctions.
// All field types below have been re-verified against the dtor at 0x127c90 (which
// IS CustomFunctions::~CustomFunctions) and selected method bodies (lookupNode
// @0x15c530, addNodeAccess @0x15c6c0, getNodeAddress @0x16ba10).
//
// Offset  Size  Type                                              Name
// ------  ----  ----                                              ----
// 0x00    8     AWGCompilerConfig const*                          config_
// 0x08    8     DeviceConstants const*                            devConst_
// 0x10    16    shared_ptr<Resources>                             resources_
// 0x20    16    shared_ptr<WavetableFront>                        wavetableFront_
// 0x30    16    shared_ptr<WavetableFront>                        wavetableFrontPrivate_
// 0x40    16    shared_ptr<WaveformGenerator>                     waveformGen_
// 0x50    16    shared_ptr<AsmCommands>                           asmCommands_
// 0x60    32    unordered_map<string, function<shared_ptr<EvalResults>(
//                  vector<EvalResultValue> const&,
//                  shared_ptr<Resources>)>>                        funcMap_
// 0x80    4     float                                             field_80 (1.0f max_load_factor)
// 0x84    4     (padding)
// 0x88    32    unordered_map<string, vector<string>>             field_88
// 0xA8    4     float                                             field_A8 (1.0f max_load_factor)
// 0xAC    4     (padding)
// 0xB0    24    set<string>                                       field_B0
// 0xC8    48    MathCompiler                                      mathCompiler_
// 0xF8    8     unique_ptr<NodeMap>                               nodeMap_
//                  (CONFIRMED from lookupNode @0x15c530: reads [this+0xf8] as
//                   single ptr, passes to NodeMap::retrieve(string const&) const.
//                   NodeMap dtor @128110 size-24, so NodeMap == { map<string,NodeMapItem> }.)
// 0x100   40    unordered_map<NodeMapItem, uint32_t>              nodeAddressMap_
//                  (CONFIRMED from addNodeAccess @0x15c6c0 + getNodeAddress @0x16ba10:
//                   value is the index of the NodeMapItem within nodeList_.)
// 0x128   40    unordered_map<NodeMapItem, set<AccessMode>>       accessModeMap_
//                  (CONFIRMED from addNodeAccess hash-table emplace at 15c6f5.)
// 0x150   24    vector<NodeMapItem>                               nodeList_
//                  (CONFIRMED from addNodeAccess emplace_back_slow_path call
//                   at 15c7e4 with 24-byte stride.)
// 0x168   40    unordered_set<string>                             assignedWaveNames_
//                  (Resolved Phase 14a: dtor at 127cf2 walks bucket array with
//                   string-node destruction; 1.0f max_load_factor at +0x188 in
//                   ctor at 12bec9 confirms unordered_set. Spans +0x168..+0x190.)
// 0x190   48    std::function<void(string const&)>                warningCallback_
//                  (CONFIRMED from dtor pattern at 127cb0: small-buffer check
//                   `cmp [rbx+0x1b0], (rbx+0x190)` and virtual destroy via vtable.)
// 0x1C0   4     ExternalTriggeringMode (int32_t)                    externalTriggeringMode_
//                  (None=0, Dio=1, ZSync=2; mutual-exclusion latch for I/O bus family)
// 0x1C4   4     padding
// 0x1C8   24    set<string>                                       usedFeatures_
// ============================================================================

class CustomFunctions {
    friend class Compiler;  // Compiler::compile() directly writes resources_ (+0x10)
    friend class AWGCompilerImpl;  // getJsonVersion reads externalTriggeringMode_ and usedFeatures_ directly
public:
    // SubFunc — enum for play() / playIndexed() dispatch
    // Confirmed from binary: playWave passes 1, playWaveNow passes 3.
    enum class SubFunc : int {
        Prefetch   = 0,   // prefetch (prefetch-only path, no asmPlay)
        Default    = 1,   // playWave, playWaveIndexed
        Aux        = 2,   // playAuxWaveIndexed (via playIndexed)
        Now        = 3,   // playWaveNow, playWaveIndexedNow
        DigTrigger = 4,   // playWaveDigTrigger (via play)
        // Note: playAuxWave, playDIOWave, playWaveDIO, playWaveZSync
        // are complex wrappers that do NOT delegate to play()/playIndexed().
    };

    // --- Ctor / Dtor ---
    CustomFunctions(AWGCompilerConfig const& config,
                    DeviceConstants const& devConst,
                    std::shared_ptr<WavetableFront> wavetable,
                    std::shared_ptr<WaveformGenerator> waveformGen,
                    std::shared_ptr<AsmCommands> asmCommands,
                    std::function<void(std::string const&)> const& warningCb);  // @0x12bcf0
    ~CustomFunctions();  // @0x127c90

    // --- Public API ---
    bool functionExists(std::string const& name) const;  // @0x159410
    std::shared_ptr<EvalResults> call(
        std::string const& name,
        std::vector<EvalResultValue> const& args,
        std::shared_ptr<Resources> res);                  // @0x159470

    // --- Utility methods ---
    void checkFunctionSupported(std::string const& name, AwgDeviceType devType);  // @0x15aeb0
    void checkWaveformMinLengthTrig(std::shared_ptr<WaveformFront> wf);           // @0x15b000
    int  checkPlayMinLength(int length);                                           // @0x15b100
    int  checkPlayAlignment(int length);                                           // @0x15b190
    void checkOffspecWaveLength(std::shared_ptr<WaveformFront> wf, int expected);  // @0x15b370
    bool optionAvailable(std::string const& option);                               // @0x15b9c0

    bool oscMaskCheckGrimsel(unsigned int mask);  // @0x15ba90
    bool oscMaskCheckHirzel(unsigned int mask);   // @0x15bab0
    unsigned int oscMaskSetAllHirzel();            // @0x15bf50
    unsigned int oscMaskSetAllGrimsel();           // @0x15c0b0

    NodeMapItem lookupNode(std::string const& path);  // @0x15c530
    void addNodeAccess(NodeMapItem const& item, AccessMode mode);  // @0x15c6c0

    void setWaitCyclesReg(std::vector<EvalResultValue> const& args,
                          std::shared_ptr<EvalResults> results,
                          std::shared_ptr<Resources> res);  // @0x15ca90

    std::shared_ptr<WaveformFront> mergeWaveforms(
                        std::vector<EvalResultValue> const& args,
                        short param2, bool param3,
                        std::string const& name,
                        int param5, bool param6);  // @0x15e060
                        // Mangled: ...sbRK<string>ib (last param is bool, not int64_t)

    std::shared_ptr<EvalResults> play(
        std::vector<EvalResultValue> const& args,
        std::shared_ptr<Resources> res,
        SubFunc subFunc);  // @0x15f090

    std::shared_ptr<EvalResults> playIndexed(
        std::vector<EvalResultValue> const& args,
        std::shared_ptr<Resources> res,
        SubFunc subFunc);  // @0x160e00

    int getWaitTime(int samples, int rate);  // @0x163930

    // Returns shared_ptr<EvalResults> via sret in the binary (rdi at @0x164550
    // line 1 stores into rbx, then [rbx]=control-block-payload, [rbx+8]=ptr).
    std::shared_ptr<EvalResults> writeToNode(EvalResultValue path, EvalResultValue val,
                                              EvalResultValue type,
                                              std::shared_ptr<Resources> res);  // @0x164550

    void initNodeMap();  // @0x16b740
    uint32_t getNodeAddress(NodeMapItem const& item) const;  // @0x16ba10
    double getSampleClock() const;  // @0x16ba80

    void addSyncCommand(std::shared_ptr<EvalResults> results,
                        std::shared_ptr<Resources> res);  // @0x16bb30

    std::set<AccessMode> const& getAccessModes(NodeMapItem const& item) const;  // @0x16be50

    std::string printF(std::vector<EvalResultValue> const& args,
                       std::string const& fmt);  // @0x16c470

    void addWaitCycles(int cycles,
                       std::shared_ptr<EvalResults> results,
                       std::shared_ptr<Resources> res);  // @0x16d8c0

    void writeLS64bit(unsigned long value, int reg1, int reg2,
                      std::shared_ptr<EvalResults> results,
                      std::shared_ptr<Resources> res);  // @0x16dbb0

    std::shared_ptr<EvalResults> generateWaveform(
        std::string const& name,
        std::vector<EvalResultValue> const& args,
        std::shared_ptr<Resources> res);  // @0x15a9f0

    // --- SeqC built-in function implementations ---
    // All share signature: shared_ptr<EvalResults>(vector<EvalResultValue> const&, shared_ptr<Resources>)
    // Registered in funcMap_ during construction. Bodies stubbed with addresses.

    std::shared_ptr<EvalResults> setDIO(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                // @0x130780
    std::shared_ptr<EvalResults> getDIO(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                // @0x131040
    std::shared_ptr<EvalResults> getDIOTriggered(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x131410
    std::shared_ptr<EvalResults> getZSyncData(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);          // @0x1316f0
    std::shared_ptr<EvalResults> getFeedback(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x132420
    std::shared_ptr<EvalResults> setID(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                 // @0x1334a0
    std::shared_ptr<EvalResults> assignWaveIndex(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x133c40
    std::shared_ptr<EvalResults> prefetch(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x1351d0
    std::shared_ptr<EvalResults> prefetchIndexed(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x135290
    std::shared_ptr<EvalResults> playWave(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x1352f0
    std::shared_ptr<EvalResults> playWaveNow(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x1353b0
    std::shared_ptr<EvalResults> playWaveIndexed(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x135480
    std::shared_ptr<EvalResults> playWaveIndexedNow(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x135550
    std::shared_ptr<EvalResults> playAuxWave(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x135610
    std::shared_ptr<EvalResults> playAuxWaveIndexed(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x136930
    std::shared_ptr<EvalResults> playDIOWave(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x1369f0
    std::shared_ptr<EvalResults> playWaveDIO(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x137740
    std::shared_ptr<EvalResults> playWaveZSync(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x137a50
    std::shared_ptr<EvalResults> playWaveDigTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x1386a0
    std::shared_ptr<EvalResults> playZero(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x1387f0
    std::shared_ptr<EvalResults> playHold(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x139030
    std::shared_ptr<EvalResults> wait(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                  // @0x139760
    std::shared_ptr<EvalResults> waitWave(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x13a980
    std::shared_ptr<EvalResults> waitTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x13abf0
    std::shared_ptr<EvalResults> waitAnaTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);        // @0x13b4b0
    std::shared_ptr<EvalResults> waitDigTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);        // @0x13c110
    std::shared_ptr<EvalResults> waitOnGrid(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x13d000
    std::shared_ptr<EvalResults> waitOnSync(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x13d3a0
    std::shared_ptr<EvalResults> waitDIOTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);        // @0x13d630
    std::shared_ptr<EvalResults> waitZSyncTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);      // @0x13dcf0
    std::shared_ptr<EvalResults> waitCntTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);        // @0x13e460
    std::shared_ptr<EvalResults> waitDemodOscPhase(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);     // @0x13eba0
    std::shared_ptr<EvalResults> waitSineOscPhase(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);      // @0x13f790
    std::shared_ptr<EvalResults> waitTimestamp(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x1401c0
    std::shared_ptr<EvalResults> resetOscPhase(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x1403b0
    std::shared_ptr<EvalResults> setSinePhase(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);          // @0x141df0
    std::shared_ptr<EvalResults> incrementSinePhase(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x142da0
    std::shared_ptr<EvalResults> waitDemodSample(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x143d50
    std::shared_ptr<EvalResults> waitPlayQueueEmpty(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x145240
    std::shared_ptr<EvalResults> setTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x1454c0
    std::shared_ptr<EvalResults> getTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x145ad0
    std::shared_ptr<EvalResults> setInternalTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x146140
    std::shared_ptr<EvalResults> getAnaTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x146740
    std::shared_ptr<EvalResults> getDigTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x147420
    std::shared_ptr<EvalResults> setInt(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                // @0x1480d0
    std::shared_ptr<EvalResults> setDouble(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);             // @0x148ac0
    std::shared_ptr<EvalResults> randomSeed(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x1497c0
    std::shared_ptr<EvalResults> generate(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x149940
    std::shared_ptr<EvalResults> setUserReg(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x14a420
    std::shared_ptr<EvalResults> getUserReg(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x14b480
    std::shared_ptr<EvalResults> getSweeperLength(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);      // @0x14bca0
    std::shared_ptr<EvalResults> setRate(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);               // @0x14c370
    std::shared_ptr<EvalResults> setPrecompClear(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x14c720
    std::shared_ptr<EvalResults> setWaveDIO(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x14cae0
    std::shared_ptr<EvalResults> now(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                   // @0x14cbc0
    std::shared_ptr<EvalResults> at(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                    // @0x14ce30
    std::shared_ptr<EvalResults> error(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                 // @0x14d830
    std::shared_ptr<EvalResults> info(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                  // @0x14da50
    std::shared_ptr<EvalResults> lock(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                  // @0x14dc70
    std::shared_ptr<EvalResults> unlock(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                // @0x14e180
    std::shared_ptr<EvalResults> sync(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                  // @0x14e690
    std::shared_ptr<EvalResults> getCnt(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                // @0x14e8d0
    std::shared_ptr<EvalResults> waitQAResultTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);   // @0x14edc0
    std::shared_ptr<EvalResults> getQAResult(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x14f380
    std::shared_ptr<EvalResults> startQAResult(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x14f620
    std::shared_ptr<EvalResults> startQAMonitor(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);        // @0x1500b0
    std::shared_ptr<EvalResults> executeTableEntry(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);     // @0x150900
    std::shared_ptr<EvalResults> setPRNGSeed(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x1513e0
    std::shared_ptr<EvalResults> getPRNGValue(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);          // @0x151a70
    std::shared_ptr<EvalResults> setPRNGRange(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);          // @0x151ce0
    std::shared_ptr<EvalResults> startQA(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);               // @0x152690
    std::shared_ptr<EvalResults> resetRTLoggerTimestamp(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res); // @0x153f90
    std::shared_ptr<EvalResults> configFreqSweep(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x154240
    std::shared_ptr<EvalResults> setSweepStep(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);          // @0x155640
    std::shared_ptr<EvalResults> setOscFreq(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x156a70
    std::shared_ptr<EvalResults> configureFeedbackProcessing(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);  // @0x157e60

    // --- Accessors for Compiler::getNodeAccessList / getNodeToModeMap ---
    // The binary accesses these via raw pointer arithmetic (this+0x150, this+0x128).
    const std::vector<NodeMapItem>& nodeList() const { return nodeList_; }
    const std::unordered_map<NodeMapItem, std::set<AccessMode>>& accessModeMap() const { return accessModeMap_; }

private:
    // --- Fields (see layout table above) ---
    AWGCompilerConfig const*                           config_;                 // +0x00
    DeviceConstants const*                             devConst_;               // +0x08
    std::shared_ptr<Resources>                         resources_;              // +0x10
    std::shared_ptr<WavetableFront>                    wavetableFront_;         // +0x20
    std::shared_ptr<WavetableFront>                    wavetableFrontPrivate_;  // +0x30
    std::shared_ptr<WaveformGenerator>                 waveformGen_;            // +0x40
    std::shared_ptr<AsmCommands>                       asmCommands_;            // +0x50

    // Function dispatch map: function name → bound member function
    using FuncType = std::function<std::shared_ptr<EvalResults>(
        std::vector<EvalResultValue> const&, std::shared_ptr<Resources>)>;
    std::unordered_map<std::string, FuncType>          funcMap_;                // +0x60

    // NOTE: On libc++, unordered_map is 0x28 bytes (0x20 hash table + 4B max_load_factor + 4B pad).
    // The floats below are the trailing max_load_factor of the preceding map.
    float                                              funcMap_maxLoadFactor_{1.0f}; // +0x80  (libc++ internal)
    char                                               pad_84_[4];             // +0x84

    std::unordered_map<std::string, std::vector<std::string>> aliasMap_;         // +0x88

    float                                              aliasMap_maxLoadFactor_{1.0f}; // +0xA8  (libc++ internal)
    char                                               pad_AC_[4];             // +0xAC

    // +0xB0: set<string> — no reconstructed consumer found; may be populated by
    // a not-yet-reconstructed method. Kept for layout fidelity.
    std::set<std::string>                              unusedStringSet_B0_;    // +0xB0

    MathCompiler                                       mathCompiler_;           // +0xC8

    // CORRECTED: was previously declared as `std::map<std::string, NodeMapItem>`
    // (24 bytes inline). REAL layout from lookupNode @0x15c530 line 15c54e:
    // single 8-byte pointer dereferenced and passed to NodeMap::retrieve(...).
    // NodeMap is a 24-byte heap-allocated wrapper containing a
    // std::map<std::string, NodeMapItem>.
    std::unique_ptr<NodeMap>                           nodeMap_;                // +0xF8 (8B)

    std::unordered_map<NodeMapItem, uint32_t>           nodeAddressMap_;        // +0x100
    std::unordered_map<NodeMapItem, std::set<AccessMode>> accessModeMap_;      // +0x128
    std::vector<NodeMapItem>                           nodeList_;               // +0x150

    // RESOLVED (#101) via deeper dtor analysis at ~CustomFunctions @0x127cf2:
    // The earlier "vector<T>" reading was wrong because I missed the node-walk
    // loop at 127d40-127d70 (it's reached via a `jne` branch when the first
    // node ptr is non-null). The full dtor sequence is:
    //
    //   127cf2: rdi = bucket_array_ptr  ([rbx+0x168])
    //   127cf9: store nullptr there
    //   127d04: if rdi == 0 skip to 127d19
    //   127d09: rsi = bucket_count      ([rbx+0x170])
    //   127d10: rsi <<= 3               (size = N * sizeof(void*))
    //   127d14: operator delete[](rdi, size)  ← bucket array
    //   127d40-127d70 (reached via 127cf0 jne): node-walk loop:
    //     each node = 0x28 (40) bytes = 16B header (next + hash) + 24B std::string
    //     destroys std::string at node+0x10/+0x20 then `operator delete` of node
    //
    // → assignedWaveNames_ IS `std::unordered_set<std::string>` (libc++ 40B container).
    //   Spans +0x168..+0x190 (next field warningCallback_ starts at +0x190).
    //   The 1.0f at +0x188 (set in ctor at 12bec9) is the max_load_factor.
    //
    // Tracks waveform names registered via assignWaveIndex().
    // Tracks waveform names registered via assignWaveIndex().
    std::unordered_set<std::string>                    assignedWaveNames_;      // +0x168 (40B, ends just before +0x190)

    std::function<void(std::string const&)>            warningCallback_;        // +0x190 (48B)

    // External triggering mode: None=unset, Dio=1, ZSync=2.
    // Guards against mixing DIO and ZSync operations in one program.
    // Throw ErrorMessageT(0x4f) if externalTriggeringMode_ != None and != expected.
    ExternalTriggeringMode                             externalTriggeringMode_{ExternalTriggeringMode::None}; // +0x1C0
    char                                               pad_1C4_[4];             // +0x1C4..+0x1C7

    std::set<std::string>                              usedFeatures_;           // +0x1C8

    // --- Extracted helpers (not in binary — refactoring for readability) ---
    void checkExternalTriggeringMode(ExternalTriggeringMode expected);
    bool isShfFamily() const;
};


} // namespace zhinst
