// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Class: zhinst::Prefetch
//
// Confirmed from:
//   - Prefetch::Prefetch()           0x1c5850
//   - Prefetch::~Prefetch()          0x11eed0
//   - Prefetch::allocate()           0x1d0fb0
//   - Prefetch::preparePlays()       0x1c8740
//   - Prefetch::fillInPlaceholders() 0x1d65c0
//   - Prefetch::getUsedChannels()    0x1df2f0
//   - Prefetch::getUsedFourChannelMode() 0x1df400
//   - Prefetch::clampToCache()       0x1d6ca0 (wvfImpl context)
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "play_config.hpp"

#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/bimap/set_of.hpp>

namespace zhinst {

// Forward declarations
struct AWGCompilerConfig;
struct DeviceConstants;
class AsmCommands;
class Node;
class WavetableIR;
class Cache;
class Resources;
class WaveformIR;
struct CancelCallback;
class AsmList;

namespace detail {
    template<typename T> using AddressImpl = T;
}

// ============================================================================
// Prefetch class — 0x160 bytes (352 bytes)
//
// Manages waveform prefetching / cache allocation for the sequencer.
//
// Layout:
//   Offset  Size  Type                                              Field
//   ------  ----  ------------------------------------------------  -----------------------
//   0x00      8   AWGCompilerConfig const*                          config_
//   0x08      8   DeviceConstants const*                            devConst_
//   0x10     0x28  unordered_map<shared_ptr<Node>, PrefetcherNodeState>  nodeStates_
//             │    +0x10: bucket_list ptr
//             │    +0x18: bucket_count (with hasher)
//             │    +0x20: first_node ptr
//             │    +0x28: element_count (with key_equal)
//             │    +0x30: max_load_factor = 1.0f
//   0x38     0x28  unordered_map<string, bool>                      nameMap_
//             │    +0x38: bucket_list ptr
//             │    +0x40: bucket_count
//             │    +0x48: first_node ptr
//             │    +0x50: element_count
//             │    +0x58: max_load_factor = 1.0f
//   0x60     0x10  shared_ptr<Node>                                  root_
//   0x70     0x10  shared_ptr<AsmCommands>                           asmCommands_
//   0x80     0x10  shared_ptr<Resources>                             resources_  (initially null)
//   0x90     0x10  shared_ptr<Cache>                                 cache_
//   0xA0     0x18  vector<bimap<optional<string>, unsigned long>>    waveformMaps_
//   0xB8      4    int32_t                                           maxBranches_  (init=1, max branch depth)
//   0xBC      1    bool                                              split_
//   0xBD      3    (padding)
//   0xC0      4    int32_t                                           unknown_c0_  (init -1)
//   0xC4      4    int32_t                                           unknown_c4_  (init 0)
//   0xC8      4    int32_t                                           unknown_c8_  (init 0)
//   0xCC      1    bool                                              unknown_cc_  (init false)
//   0xCD      3    (padding)
//   0xD0      8    int64_t/size_t                                    unknown_d0_  (init 0)
//   0xD8      8    (zeroed, padding or field)                        unknown_d8_  (init 0)
//   0xE0     0x18  vector<UsageEntry>                                usageEntries_
//             │    Elements are 0x20 bytes, with int at +0x08, bool at +0x0C
//   0xF8     0x10  shared_ptr<Node>                                  currentNode_ (initially null)
//   0x108     8    (padding)
//   0x110    0x10  shared_ptr<WavetableIR>                           wavetableIR_
//   0x120    0x28  function<void(string const&)>                     logFunc_
//             │    +0x120: __buf_ (32 bytes inline storage)
//             │    +0x140: __f_ (callable pointer)
//   0x148     8    (padding within function or alignment)
//   0x150    0x10  weak_ptr<CancelCallback>                          cancelCb_
//             │    +0x150: ptr
//             │    +0x158: ctrl_blk
//   0x160    ---   END
// ============================================================================

class Prefetch {
public:
    // PrefetcherNodeState is used as value type in the nodeStates_ map.
    // Inserted via emplace with default ctor, then field at +0x40 (relative
    // to hash node value) is set to cache_.getSize().
    // TODO: determine full PrefetcherNodeState layout from usage.
    // PrefetcherNodeState layout (offsets relative to struct start):
    //   The hash_node value starts at +0x20 from hash_node*.
    //   The shared_ptr<Node> key is 16 bytes (+0x20..+0x2F).
    //   PNS starts at hash_node+0x30.
    //   So hash_node+0x34 = PNS+0x04 = branchCount
    //        hash_node+0x3c = PNS+0x0C = playSize
    //        hash_node+0x40 = PNS+0x10 = cacheSize (from ctor)
    //        hash_node+0x50 = PNS+0x20 = lengthReg
    //
    // Confirmed from:
    //   - countBranches: reads/writes +0x34 (branchCount)
    //   - definePlaySize: reads/writes +0x3c (playSize), +0x20 (lengthReg via AsmRegister)
    //   - constructor: writes +0x40 (cacheSize)
    struct PrefetcherNodeState {
        // Layout within unordered_map hash node value area:
        // (Offsets are relative to the value start, which is +0x20 from
        //  the hash_node start after the key shared_ptr<Node> at +0x10)
        //
        //   +0x00 (0x20 from node) — AsmRegister registerHirzel  (set when isHirzel)
        //   +0x08 (0x28 from node) — AsmRegister registerCervino (set when !isHirzel)
        //   +0x10 (0x30 from node) — int counter (set to 0 for Play with refcount < 2)
        //   +0x18 (0x38 from node) — int refTrack (decremented/incremented in optimize)
        //   +0x1C (0x3C from node) — int pageSize (used for waveform size division)
        //   +0x20 (0x40 from node) — int usedCache (set from Cache::getUsedCache())
        //   +0x28 (0x48 from node) — shared_ptr<Cache::Pointer> cachePtr
        //   +0x30 (0x50 from node) — (ctrl block for cachePtr)
        //   +0x38 (0x58 from node) — bool useDA (from devConst_->hasPrecomp)
        AsmRegister registerHirzel;       // +0x00
        AsmRegister registerCervino;      // +0x08
        int counter = 0;                  // +0x10
        int refTrack = 0;                 // +0x18
        int pageSize = 0;                 // +0x1C
        int usedCache = 0;               // +0x20
        std::shared_ptr<void> cachePtr;  // +0x28 (actually shared_ptr<Cache::Pointer>)
        bool useDA = false;              // +0x38
    };

    // Constructor                                                     // 0x1c5850
    Prefetch(
        AWGCompilerConfig const& config,
        DeviceConstants const& devConst,
        std::shared_ptr<AsmCommands> asmCommands,
        std::shared_ptr<Node> root,
        std::shared_ptr<WavetableIR> wavetableIR,
        std::function<void(std::string const&)> const& logFunc,
        std::weak_ptr<CancelCallback> cancelCb);

    // Destructor                                                      // 0x11eed0
    ~Prefetch();

    // --- Public methods (addresses noted) ---

    void preparePlays();                                               // 0x1c8740
    void prepareTree(std::shared_ptr<Node> node);                      // 0x1c8870
    void countBranches(std::shared_ptr<Node> node);                    // 0x1c9b30
    void definePlaySize(std::shared_ptr<Node> node);                   // 0x1ca370
    void determineFixedWaves();                                        // 0x1cb200
    void placeLoads();                                                 // 0x1cbf60
    size_t getMemoryHighWatermark() const;                             // 0x1cc650
    size_t getRequiredMemory() const;                                  // 0x1cc930
    std::shared_ptr<Node> moveLoadsToFront(std::shared_ptr<Node> node); // 0x1ccad0
    void optimize(std::shared_ptr<Node> node);                         // 0x1cdae0
    void optimizeSync(std::shared_ptr<Node> node);                     // 0x1cf7b0
    void optimizeCwvf(std::shared_ptr<Node> node);                     // 0x1cfc70
    void allocate(std::shared_ptr<Node> node, std::shared_ptr<Cache> cache); // 0x1d0fb0
    std::vector<std::shared_ptr<WaveformIR>> getUsedWavesForDevice(size_t deviceIdx) const; // 0x1d2d60
    void collectUsedWaves(std::shared_ptr<Node> node);                 // 0x1d31c0
    void linkLoad(std::shared_ptr<Node> node);                         // 0x1d33f0
    void removeBranches(std::shared_ptr<Node> node,
        std::stack<std::shared_ptr<Node>>& stack) const;               // 0x1d3530
    void expandSetVar(std::shared_ptr<Node> node) const;               // 0x1d3af0
    std::shared_ptr<Node> findLockedPlay(std::shared_ptr<Node> node,
        std::shared_ptr<WaveformIR> waveform) const;                   // 0x1d3dd0
    std::shared_ptr<Node> createLoad(std::shared_ptr<Node> node);       // 0x1d4a10
    void mergeLoads(std::shared_ptr<Node> node,
        std::shared_ptr<Node> other);                                  // 0x1d5040
    void assignLoad(std::shared_ptr<Node> node,
        std::shared_ptr<Node> const& load, bool flag);                 // 0x1d53a0
    void globalCwvf(std::shared_ptr<Node> node);                       // 0x1d5620
    void backwardTree(std::shared_ptr<Node> node) const;                // 0x1d57d0
    bool sameLoads(std::shared_ptr<Node> a,
        std::shared_ptr<Node> b) const;                                // 0x1d5e20
    std::shared_ptr<Node> nodeByCachePointer(
        std::shared_ptr<Cache::Pointer> ptr) const;                    // 0x1d60d0

    // Asm placement
    AsmList fillInPlaceholders(AsmList const& asmList);                // 0x1d65c0
    void placeCommands(AsmList* out, std::shared_ptr<Node> node);      // 0x1d6680
    void placeSingleCommand(AsmList* out, std::shared_ptr<Node> node); // 0x1d7940
    std::shared_ptr<Node> findPlaceholder(AsmList* out,
        std::shared_ptr<Node> node);                                   // 0x1d6b50

    // Waveform instruction helpers
    detail::AddressImpl<uint32_t> clampToCache(
        detail::AddressImpl<uint32_t> addr) const;                     // 0x1d6c40
    AsmList wvfImpl(AsmRegister reg, int offset, bool indexed) const;  // 0x1d6ca0
    AsmList wvfRegImpl(AsmRegister reg, AsmRegister offset,
        bool indexed) const;                                           // 0x1d7020
    AsmList wvfs(Assembler::PlayDummyType type,
        AsmRegister reg, int offset) const;                            // 0x1d73e0

    // Analysis
    bool needsNewCwvf(std::shared_ptr<Node> node) const;               // 0x1dc620
    AsmList splitPlay(std::shared_ptr<Node> node) const;               // 0x1dd1a0
    void insertPlay(AsmList& list, bool flag, std::string const& name,
        AsmRegister reg, detail::AddressImpl<uint32_t> addrA,
        detail::AddressImpl<uint32_t> addrB) const;                    // 0x1def50

    // Cache analysis
    int getUsedCache(std::shared_ptr<Node> node) const;                // 0x1c7eb0

    // Queries
    uint32_t getUsedChannels() const;                                  // 0x1df2f0
    bool getUsedFourChannelMode() const;                               // 0x1df400

    // Print/debug
    void print(std::shared_ptr<Node> node, int indent) const;         // 0x1c5dd0

    // Static
    static int minIndexedSize;  // BSS at 0xb846d8

private:
    using WaveformBimap = boost::bimaps::bimap<
        std::optional<std::string>, unsigned long>;

    AWGCompilerConfig const*                               config_;         // +0x00
    DeviceConstants const*                                 devConst_;       // +0x08
    std::unordered_map<std::shared_ptr<Node>,
                       PrefetcherNodeState>                nodeStates_;     // +0x10 (0x28 bytes)
    std::unordered_map<std::string, bool>                  nameMap_;        // +0x38 (0x28 bytes)
    std::shared_ptr<Node>                                  root_;           // +0x60
    std::shared_ptr<AsmCommands>                           asmCommands_;    // +0x70
    std::shared_ptr<Resources>                             resources_;      // +0x80
    std::shared_ptr<Cache>                                 cache_;          // +0x90
    std::vector<WaveformBimap>                             waveformMaps_;   // +0xA0
    int32_t                                                maxBranches_;    // +0xB8 (init=1, used by countBranches/definePlaySize)
    bool                                                   split_;         // +0xBC (init=false)
    // 3 bytes padding
    PlayConfig                                             cwvfConfig_;    // +0xC0 (0x20 bytes, init: channelMask=-1, rest=0)
    //   +0xC0  channelMask (init -1)
    //   +0xC4  rate (init 0)
    //   +0xC8  suppress (init 0)
    //   +0xCC  is4Channel (init false)
    //   +0xD0  markerBits (init 0)
    //   +0xD4  trigger (init 0)
    //   +0xD8  precompFlags (init 0)
    //   +0xDD  hold
    //   +0xDE  dummy
    std::vector</* UsageEntry 0x20 bytes */>               usageEntries_;  // +0xE0 (24 bytes)
    std::shared_ptr<Node>                                  lastCwvfNode_;  // +0xF8 (last node seen in globalCwvf)
    bool                                                   globalCwvfValid_; // +0x108
    // 7 bytes padding
    std::shared_ptr<WavetableIR>                           wavetableIR_;   // +0x110
    std::function<void(std::string const&)>                logFunc_;       // +0x120 (0x28 bytes)
    // logFunc_.__buf_ at +0x120 (32 bytes), logFunc_.__f_ at +0x140
    // 8 bytes padding at +0x148
    std::weak_ptr<CancelCallback>                          cancelCb_;      // +0x150
    // Total: 0x160 bytes
};

} // namespace zhinst
