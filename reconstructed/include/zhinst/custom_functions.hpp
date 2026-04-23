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
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/json.hpp>

#include <zhinst/asm_register.hpp>
#include <zhinst/value.hpp>

namespace zhinst {

// Forward declarations
class AsmCommands;
struct AWGCompilerConfig;
struct DeviceConstants;
class EvalResults;
class NodeMap;          // wraps std::map<std::string, NodeMapItem>; size 24B
                        // (used as unique_ptr<NodeMap> in CustomFunctions +0xF8)
class Resources;
class WaveformFront;
class WaveformGenerator;
class WavetableFront;
enum AwgDeviceType : int;
enum VarType : int32_t;
enum VarSubType : int32_t;

// ============================================================================
// EvalResultValue — 0x38 (56) bytes
//
// Typed value passed to all SeqC built-in functions and through the
// frontend lowering pipeline (vector<EvalResultValue> args).
//
// Layout:
//   +0x00  4  VarType       varType_    — outer type tag (Int=2, Double=3, String=4, ...)
//   +0x04  4  VarSubType    varSubType_ — sub-type qualifier (usually 0)
//   +0x08  40 Value         value_      — embedded variant (0x28 bytes)
//   +0x30  8  AsmRegister   reg_        — register binding (default: value=-1, valid=false)
//
// CORRECTION 2026-04-23 (Phase 15a-i): Fields renamed from opaque
// field_00/field_08/which_/data_/field_30 to meaningful names.
// Evidence from:
//   - EvalResults::setValue(VarType, string) @0x20af20 stores VarType in [erv+0x00]
//     and VarSubType=0 in [erv+0x04]
//   - Dtor @0x15c820 reads Value.which_ at [this+0x10] = ERV+0x08+0x08
//   - setWaitCyclesReg @0x15ca90 reads AsmRegister at [base+0x68] = ERV[1]+0x30
//   - AsmRegister::AsmRegister(int) called with esi=-1 at @0x15caea
// ============================================================================
struct EvalResultValue {
    VarType     varType_;      // +0x00 — outer type tag
    VarSubType  varSubType_;   // +0x04 — sub-type qualifier
    Value       value_;        // +0x08 — embedded Value (0x28 bytes)
    AsmRegister reg_;          // +0x30 — register binding

    ~EvalResultValue();  // @0x15c820
};
static_assert(sizeof(EvalResultValue) == 0x38,
              "EvalResultValue must be 0x38 bytes");

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
// NodeMapData — polymorphic base for node map entries
//
// vtable @0xb02c98.  Two subclasses:
//   VirtAddrNodeMapData  — vtable @0xb04cd8
//   DirectAddrNodeMapData — vtable @0xb04d30
// ============================================================================
class NodeMapData {
public:
    virtual ~NodeMapData();                                  // @0x1c5720
    virtual bool compareEq(NodeMapData const& other) const = 0;
    virtual size_t hash() const = 0;
    virtual NodeMapData* clone() const = 0;
    virtual void getJson(boost::json::object& obj) const = 0;
};

class VirtAddrNodeMapData : public NodeMapData {
public:
    VirtAddrNodeMapData(VirtAddrNodeMapData const&);         // @0x1c4dc0
    ~VirtAddrNodeMapData() override;                         // @0x1c4ec0, D0 @0x1c56c0
    bool compareEq(NodeMapData const& other) const override; // @0x1c4cc0
    size_t hash() const override;                            // @0x1c4f10
    NodeMapData* clone() const override;                     // @0x1c51e0
    void getJson(boost::json::object& obj) const override;   // @0x1c5240

    // Layout (0x38 total) confirmed from D0 @0x1c56c0 (delete[size]=0x38),
    // copy ctor @0x1c4dc0 (memcpy + __throw_length_error from vector<int>),
    // and getJson @0x1c5240 (writes "name" key from string field, then iterates
    // vector reading int32_t per element):
    //   +0x00   8  vptr (from base)
    //   +0x08  24  std::string             name_       — node name (JSON "name")
    //   +0x20  24  std::vector<int32_t>    addresses_  — virtual address list
    std::string             name_;       // +0x08
    std::vector<int32_t>    addresses_;  // +0x20
};

class DirectAddrNodeMapData : public NodeMapData {
public:
    ~DirectAddrNodeMapData() override;                       // @0x1c5730
    bool compareEq(NodeMapData const& other) const override; // @0x1c5460
    size_t hash() const override;                            // @0x1c5370
    NodeMapData* clone() const override;                     // @0x1c53c0
    void getJson(boost::json::object& obj) const override;   // @0x1c5400

    // Contains a direct address — used by getNodeAddress for fast path
    // (dynamic_cast<DirectAddrNodeMapData*> in getNodeAddress @0x16ba10).
    // Layout confirmed from getNodeAddress (16ba3f: `add rax, 8; mov eax, [rax]`):
    //   +0x00  8  vptr (from base)
    //   +0x08  4  uint32_t   addr_     direct hardware address
    uint32_t addr_;  // +0x08 (after 8-byte vptr from NodeMapData)
};

// ============================================================================
// NodeMapItem — 0x18 (24) bytes
//
// Hashable, comparable. Used as key in unordered_map and element in vectors.
//
// Layout (from operator== @0x1c5490 and fastAddress @0x1c5470):
//   +0x00  8  NodeMapData*   data     (polymorphic, dynamic_cast in getNodeAddress)
//   +0x08  4  int32_t        typeIdx  (node type/index)
//   +0x0C  4  uint32_t       fastAddr (fast address)
//   +0x10  1  bool           hasFast  (has fast address)
//   +0x11  7  (padding)
// ============================================================================
struct NodeMapItem {
    NodeMapData*  data;       // +0x00
    int32_t       typeIdx;    // +0x08
    uint32_t      fastAddr;   // +0x0C
    bool          hasFast;    // +0x10
    char          pad_11[7];  // +0x11

    bool operator==(NodeMapItem const& other) const;  // @0x1c5490
    uint32_t fastAddress() const;                      // @0x1c5470

    boost::json::value toJson() const;                 // @0x1c54f0
    size_t size() const;                               // @0x1c55a0
};
static_assert(sizeof(NodeMapItem) == 0x18,
              "NodeMapItem must be 0x18 bytes");

// ============================================================================
// MathCompiler — inline sub-object within CustomFunctions at +0xC8
//
// Ctor @0x1c2250.  Contains function dispatch maps for math builtins.
// Size: 0x30 bytes (inferred from CustomFunctions layout gap +0xC8..+0xF8).
// ============================================================================
class MathCompiler {
public:
    MathCompiler();  // @0x1c2250

    // Single-argument math functions
    double abs(double);        // @0x1c3520
    double acos(double);       // @0x1c3530
    double acosh(double);      // @0x1c35f0
    double asin(double);       // @0x1c36b0
    double asinh(double);      // @0x1c3770
    double atan(double);       // @0x1c3780
    double atanh(double);      // @0x1c3790
    double cos(double);        // @0x1c3850
    double cosh(double);       // @0x1c3860
    double exp(double);        // @0x1c3870
    double ln(double);         // @0x1c3880
    double log(double);        // @0x1c3940
    double log2(double);       // @0x1c3a00
    double log10(double);      // @0x1c3ac0
    double sign(double);       // @0x1c3b80
    double sin(double);        // @0x1c3ba0
    double sinh(double);       // @0x1c3bb0
    double sqrt(double);       // @0x1c3bc0
    double tan(double);        // @0x1c3be0
    double tanh(double);       // @0x1c3bf0
    double ceil(double);       // @0x1c3c00
    double round(double);      // @0x1c3c10
    double floor(double);      // @0x1c3c20

    // Multi-argument math functions
    double avg(std::vector<double> const&);  // @0x1c3c30
    double max(std::vector<double> const&);  // @0x1c3c90
    double min(std::vector<double> const&);  // @0x1c3cf0
    double pow(std::vector<double> const&);  // @0x1c3d50
    double sum(std::vector<double> const&);  // @0x1c3e10

    bool functionExists(std::string const& name, size_t argCount, bool strict) const;  // @0x1c3e50
    double call(std::string const& name, std::vector<double> const& args);             // @0x1c3eb0

    // Layout (0x30 bytes total) — confirmed from CustomFunctions::~CustomFunctions @0x127c90:
    //   +0x00  24B  std::map<std::string, std::function<double(double)>>                singleArgFns_
    //                  (dtor at 127e7e calls __tree<__value_type<string, function<double(double)>>>::destroy)
    //   +0x18  24B  std::map<std::string, std::function<double(std::vector<double> const&)>>  multiArgFns_
    //                  (dtor at 127e64 calls __tree<__value_type<string, function<double(vector<double> const&)>>>::destroy)
    //
    // NOTE: The dtor visits +0x18 (multi-arg) first then +0x00 (single-arg) — that's
    //       reverse-construction order, so single-arg is at +0x00 in the canonical layout.
    std::map<std::string, std::function<double(double)>>                       singleArgFns_;  // +0x00
    std::map<std::string, std::function<double(std::vector<double> const&)>>   multiArgFns_;   // +0x18
};

// ============================================================================
// MathCompilerException
// ============================================================================
class MathCompilerException : public std::exception {
    std::string msg_;
public:
    explicit MathCompilerException(std::string const& msg);  // @0x1c40c0
    ~MathCompilerException() override;                        // @0x1c4120, D0 @0x1c4160
    const char* what() const noexcept override;               // @0x1c41b0
};

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
};

} // namespace std
// Specializations needed for unordered_map<NodeMapItem, ...>
namespace std {
template<> struct hash<zhinst::NodeMapItem> {
    size_t operator()(zhinst::NodeMapItem const& item) const {
        // Delegates to NodeMapData::hash() — simplified here
        return item.data ? item.data->hash() : 0;
    }
};
} // namespace std

namespace zhinst {

// ============================================================================
// PlayArgs — helper for parsing play-function arguments
//
// parse @0x15d7b0, addChannelWave @0x170ec0
// ============================================================================
struct PlayArgs {
    static PlayArgs parse(std::vector<EvalResultValue> const& args);  // @0x15d7b0
    void addChannelWave(int channel, EvalResultValue const& val);     // @0x170ec0

    // TODO: field layout unknown
};

// Free functions related to CustomFunctions argument parsing
std::vector<EvalResultValue> parseOptionalString(std::vector<EvalResultValue>& args);  // @0x15d3e0
int getPlayRate(EvalResultValue const& val, std::string const& name, bool strict);     // @0x163730

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
// 0x168   16    vector<T>  (T size 8 bytes, trivially destructible)  field_168 (TBD)
//                  (Dtor at 127cf2: `delete[size*8]` only — no per-element loop,
//                   no string/vptr destruction. Element type T unknown; could be
//                   uint64_t, void*, NodeMapItem*, double, etc. Need ctor and
//                   write-site analysis to identify T. NOT touched by lookupNode,
//                   addNodeAccess, getNodeAddress, or initial CustomFunctions ctor;
//                   likely populated by a different method (initNodeMap? or one
//                   of the playWave family).)
// 0x178   8     (size_t size_ for vector at +0x168)
// 0x180   8     (size_t cap_  for vector at +0x168) — NOTE: dtor reads [+0x170]
//                  for size, so layout may be { ptr,size,cap } at 168/170/178
//                  (libc++ vector). +0x180/0x188 might be a separate float field.
// 0x190   48    std::function<void(string const&)>                warningCallback_
//                  (CONFIRMED from dtor pattern at 127cb0: small-buffer check
//                   `cmp [rbx+0x1b0], (rbx+0x190)` and virtual destroy via vtable.)
// 0x1C0   8     (likely padding or count field; dtor sets +0x1C0 to 0xFFFFFFFF
//                  in addNodeAccess +0x179 region — TODO verify)
// 0x1C8   24    set<string>                                       usedFeatures_
// ============================================================================

class CustomFunctions {
public:
    // SubFunc — enum for play() / playIndexed() dispatch
    // Confirmed from binary: playWave passes 1, playWaveNow passes 3.
    enum class SubFunc : int {
        Default    = 1,   // playWave, playWaveIndexed
        Now        = 3,   // playWaveNow, playWaveIndexedNow
        // Remaining values observed but exact mapping TBD:
        // Aux        = ?,
        // AuxIndexed = ?,
        // DIO        = ?,
        // ZSync      = ?,
        // DigTrigger = ?,
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

    void mergeWaveforms(std::vector<EvalResultValue> const& args,
                        short param2, bool param3,
                        std::string const& name,
                        int param5, bool param6);  // @0x15e060

    std::shared_ptr<EvalResults> play(
        std::vector<EvalResultValue> const& args,
        std::shared_ptr<Resources> res,
        SubFunc subFunc);  // @0x15f090

    std::shared_ptr<EvalResults> playIndexed(
        std::vector<EvalResultValue> const& args,
        std::shared_ptr<Resources> res,
        SubFunc subFunc);  // @0x160e00

    int getWaitTime(int samples, int rate);  // @0x163930

    void writeToNode(EvalResultValue path, EvalResultValue val,
                     EvalResultValue type, std::shared_ptr<Resources> res);  // @0x164550

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

    float                                              field_80_{1.0f};        // +0x80
    char                                               pad_84_[4];             // +0x84

    std::unordered_map<std::string, std::vector<std::string>> aliasMap_;         // +0x88

    float                                              field_A8_{1.0f};        // +0xA8
    char                                               pad_AC_[4];             // +0xAC

    std::set<std::string>                              field_B0_;              // +0xB0

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
    // → field_168 IS `std::unordered_set<std::string>` (libc++ 40B container).
    //   Spans +0x168..+0x190 (next field warningCallback_ starts at +0x190).
    //   The 1.0f at +0x188 (set in ctor at 12bec9) is the max_load_factor.
    //
    // Likely semantics: tracks names of features/functions encountered (similar
    // to WaveformGenerator::createdNames_). Empty in this binary; populated by
    // some method we haven't analyzed yet.
    std::unordered_set<std::string>                    field_168_;              // +0x168 (40B, ends just before +0x190)

    std::function<void(std::string const&)>            warningCallback_;        // +0x190 (48B)

    // TODO: gap between warningCallback_ end (+0x1C0) and usedFeatures_ (+0x1C8)
    char                                               pad_1C0_[8];             // +0x1C0..+0x1C7

    std::set<std::string>                              usedFeatures_;           // +0x1C8
};

} // namespace zhinst
