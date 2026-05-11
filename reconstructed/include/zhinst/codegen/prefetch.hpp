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
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "zhinst/waveform/play_config.hpp"
#include "zhinst/asm/address_impl.hpp"
#include "zhinst/asm/asm_register.hpp"
#include "zhinst/runtime/cache.hpp"
#include "zhinst/waveform/waveform_ir.hpp"
#include "zhinst/asm/assembler.hpp"
#include "zhinst/asm/asm_list.hpp"   // findPlaceholder returns AsmList::Asm*

#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/bimap/set_of.hpp>

namespace zhinst {

// Forward declarations (used as pointer/shared_ptr members)
class AsmList;
class AsmCommands;
class Node;
class Resources;
class CancelCallback;
struct AWGCompilerConfig;
struct DeviceConstants;
struct WavetableIR;

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
//   0xC0     0x20  PlayConfig                                        cwvfConfig_  (resolved: channelMask=-1, rate=0, etc.)
//                   +0xC0 = channelMask (int32, init -1)
//                   +0xC4 = rate (int32, init 0)
//                   +0xC8 = suppress (int32, init 0)
//                   +0xCC = is4Channel (bool, init false)
//                   +0xD0 = markerBits (int64/size_t, init 0)
//                   +0xD8 = remaining PlayConfig fields (trigger, precompFlags, hold, dummy)
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

//! \brief Cache-aware waveform prefetch / load planner over the lowered SeqC AST.
//!
//! Constructed once per compilation by `Compiler::runPrefetcher` and
//! driven through three ordered phases:
//!
//! - `preparePlays` walks the AST in three sub-passes (`prepareTree`
//!   to classify nodes and insert load placeholders, `countBranches`
//!   to record per-node branching, and `definePlaySize` to compute
//!   waveform play sizes and assign registers), populating
//!   `nodeStates_` along the way.
//! - `placeLoads` decides where in the program to insert load
//!   instructions (creating, merging, and assigning loads) so that
//!   every play's waveform is resident in the cache by the time it
//!   executes — minimising redundant reloads while respecting the
//!   cache's capacity and replacement geometry.  When the device
//!   uses cache-type 1, `Compiler::runPrefetcher` first calls
//!   `determineFixedWaves` to pin specific waveforms before
//!   `placeLoads` runs.
//! - `fillInPlaceholders` rewrites the input `AsmList`, replacing the
//!   `Compiler`-supplied placeholder instructions with the concrete
//!   load / play / cwvf / wvfs sequences chosen by the previous
//!   phases.
//!
//! Several large public method clusters (`wvfImpl`, `wvfRegImpl`,
//! `wvfs`, `splitPlay`, `insertPlay`, `clampToCache`) are the per-play
//! emitter helpers used by `placeCommands` / `placeSingleCommand`
//! during placeholder fill-in; `optimize`, `optimizeSync`,
//! `optimizeCwvf`, `globalCwvf`, `mergeLoads`, `removeBranches`, and
//! `expandSetVar` are the AST-rewrite passes invoked from within the
//! three driver phases.  After fill-in, the `Compiler` reads back
//! `getUsedChannels` / `getUsedFourChannelMode` for the ELF
//! `.channels` section.
class Prefetch {
public:
    // PrefetcherNodeState is used as value type in the nodeStates_ map.
    // Total size: 0x40 (64 bytes)
    //
    // PrefetcherNodeState layout (offsets relative to struct start):
    //   The hash_node value starts at +0x20 from hash_node*.
    //   The shared_ptr<Node> key is 16 bytes (+0x20..+0x2F).
    //   PNS starts at hash_node+0x30.
    //
    //   +0x00  8  AsmRegister   registerHirzel   (set for Hirzel devices; ALSO reused as
    //                                            "lengthReg" for length tracking — same 8-byte slot,
    //                                            verified mov [rax+0x20],rcx at 0x1cb03d in
    //                                            definePlaySize where rax = hash_node value pointer)
    //   +0x08  8  AsmRegister   registerCervino  (set for Cervino devices)
    //   +0x10  4  int32_t       state            (init=3; ALSO consumed as "counter" — same field,
    //                                            verified mov [rax+0x30],0 at 0x1ce77a and
    //                                            mov edx,[rax+0x30] at 0x1d1153)
    //   +0x14  4  int32_t       branchCount      (init=1, set in countBranches)
    //   +0x18  4  int32_t       refTrack         (init=0, inc'd/dec'd in optimize)
    //   +0x1C  4  int32_t       pageSize         (init=1; ALSO written as "playSize" —
    //                                            verified mov [rax+0x3c],r13d at 0x1cb00b/0x1cb0ea
    //                                            in definePlaySize)
    //   +0x20  4  int32_t       usedCache_       (init=0; verified mov r15d,[rax+0x40] at 0x1cdcfd/0x1ce61e)
    //   +0x24  4  (padding)
    //   +0x28 16  shared_ptr<Cache::Pointer> cachePtr  (init=null)
    //   +0x38  1  bool          crossesCacheLine (init=false; copied from
    //                                              WaveformIR::crossesCacheLine_
    //                                              by assignLoad)
    //   +0x39  7  (padding)
    //
    // Hallucinated PNS fields removed entirely:
    //   - lengthReg, counter, playSize, usedCache → aliases dropped (Cluster E)
    //   - totalSize → was actually a stack local in placeSingleCommand (-0x140(%rbp))
    //   - firstTime → no binary access anywhere
    //! \brief Per-AST-node bookkeeping owned by `Prefetch::nodeStates_`.
    //!
    //! `Prefetch` walks the AST several times (`countBranches`,
    //! `definePlaySize`, `placeLoads`, `allocate`, ...) and
    //! accumulates per-node decisions in a hash map keyed by
    //! `shared_ptr<Node>`; this is the value type.  `state` is a
    //! small state machine (initialised to `3` = "unloaded") tracked
    //! by `optimize`; `branchCount` and `pagesNeeded` come from the
    //! branch-counting and play-sizing passes; `cachePtr` and
    //! `usedCache_` are populated when `allocate` reserves a
    //! `Cache::Pointer` for the node; the two per-device-family
    //! register slots (`registerHirzel`, `registerCervino`) hold the
    //! `AsmRegister` assigned to this load by `assignLoad`.
    //! `crossesCacheLine` is copied from the load's
    //! `WaveformIR::crossesCacheLine_` and consulted in
    //! `placeSingleCommand` on Hirzel devices to gate emission of the
    //! cache-clamped `prf` prefetch sequence required when a single
    //! load straddles a cache-line boundary.
    struct PrefetcherNodeState {
        //! \brief Hirzel-family register slot assigned by `assignLoad`; reused as the `lengthReg` slot during length tracking.
        AsmRegister registerHirzel;                           // +0x00
        //! \brief Cervino-family register slot assigned by `assignLoad`.
        AsmRegister registerCervino;                          // +0x08
        //! \brief Per-node state machine tracked by `optimize`; initial value `3` means "unloaded".
        int32_t state = 3;                                    // +0x10 (3=unloaded)
        //! \brief Number of branches reaching this node, populated by `countBranches`.
        int32_t branchCount = 1;                              // +0x14
        //! \brief Live reference-count tracker maintained by `optimize`.
        int32_t refTrack = 0;                                 // +0x18
        //! \brief Number of cache pages this node's load occupies (also written through the `playSize` alias by `definePlaySize`).
        int32_t pagesNeeded = 1;                                 // +0x1C
        //! \brief Cache utilisation counter populated when `allocate` reserves a slot.
        int32_t usedCache_ = 0;                            // +0x20
        //! \brief ABI padding to 8-byte alignment for `cachePtr`.
        int32_t _pad24 = 0;                                   // +0x24
        //! \brief Cache slot reserved for this node by `allocate` (null until reservation).
        std::shared_ptr<Cache::Pointer> cachePtr;             // +0x28
        //! \brief Copied from the load's `WaveformIR::crossesCacheLine_`; gates `prf` emission on Hirzel devices when a load straddles a cache line.
        bool crossesCacheLine = false;                        // +0x38
        //! \brief ABI padding rounding the struct to 0x40 bytes.
        char _pad39[7] = {};                                  // +0x39 padding (no field here)

        // ====================================================================
        // Legacy alias names removed (Cluster E). Callers now use fields
        // directly: registerHirzel, state, pageSize, usedCache_.
        // ====================================================================
    };

    // Constructor                                                     // 0x1c5850
    /*! \brief Construct a prefetch planner bound to a lowered SeqC AST
     *  and the device's waveform cache.
     *
     *  \details Stores borrowed references to `config` and `devConst`
     *  (both kept by pointer for the lifetime of the planner), takes
     *  ownership of the four shared / weak resources, and seeds the
     *  internal state needed before the first traversal:
     *
     *    - `cwvfConfig_.channelMask` is set to the `0xFFFFFFFF`
     *      sentinel that `globalCwvf` treats as "no `cwvf` config
     *      observed yet".
     *    - The device cache is allocated via
     *      `make_shared<Cache>(devConst.waveformMemorySize,
     *      devConst.sampleLength, config.isHirzel)`.
     *    - `waveformMaps_` is sized to `config.numChannelGroups`.
     *    - `nodeStates_[root_]` is default-constructed and its
     *      `usedCache_` field is then overwritten with the freshly
     *      allocated `cache_->getSize()` so the root bookkeeping
     *      starts with the full cache budget.
     *    - `maxBranches_` is initialised to `1` by the in-class
     *      member initialiser; `countBranches` only ever raises it.
     *
     *  \param config        The compiler configuration (referenced).
     *  \param devConst      Device geometry constants (referenced).
     *  \param asmCommands   Shared pointer to the assembly emitter,
     *                       used by the `wvf*` / `splitPlay` /
     *                       `insertPlay` helpers during fill-in.
     *  \param root          The lowered AST root the planner walks.
     *  \param wavetableIR   The wavetable IR queried by the
     *                       traversals for per-name `WaveformIR`
     *                       lookups.
     *  \param logFunc       Callback used by `placeLoads` to surface
     *                       the informational "wave fitting cache"
     *                       messages.
     *  \param cancelCb      Cancellation hook polled by the
     *                       optimisation and placement passes.
     */
    Prefetch(
        AWGCompilerConfig const& config,
        DeviceConstants const& devConst,
        std::shared_ptr<AsmCommands> asmCommands,
        std::shared_ptr<Node> root,
        std::shared_ptr<WavetableIR> wavetableIR,
        std::function<void(std::string const&)> const& logFunc,
        std::weak_ptr<CancelCallback> cancelCb);

    // Destructor                                                      // 0x11eed0
    //! \brief Trivial destructor; releases all owned shared / weak
    //! pointers and the per-node bookkeeping containers via implicit
    //! member destruction.
    ~Prefetch();

    // --- Public methods (addresses noted) ---

    /*! \brief Run the three tree-preparation passes that populate the
     *  per-node prefetch state before any load placement is attempted.
     *
     *  \details Calls, in order:
     *    1. `prepareTree(root_)`   — classify each node by `NodeType`
     *       and apply per-type tree transformations: `Play` /
     *       `PlainLoad` collect their used waveforms, `Load` / `Table`
     *       call `linkLoad`, `Branch` is flattened into the traversal
     *       stack via `removeBranches`, `SetVar` is rewritten via
     *       `expandSetVar`, and `Wait` (in non-append mode) inspects
     *       each branch's first wave: if `findLockedPlay` returns an
     *       existing locked play the wait is removed and any
     *       neighbouring load is merged via `mergeLoads`, otherwise
     *       a fresh load is synthesised via `createLoad`.  In the
     *       `Wait` and `Play` / `PlainLoad` branches the visited
     *       waveforms are also flagged as used on the corresponding
     *       `WaveformIR` and folded into `collectUsedWaves`.
     *    2. `countBranches(root_)` — accumulate the branch fan-out
     *       per node into `nodeStates_`.
     *    3. `definePlaySize(root_)` — compute each play's effective
     *       sample size and assign register slots.
     *
     *  After this call, `nodeStates_`, the per-node play sizes, and
     *  the `WaveformIR::used_` flags are all populated, which is the
     *  precondition for `getUsedWavesForDevice`,
     *  `determineFixedWaves`, `placeLoads`, and `fillInPlaceholders`.
     */
    void preparePlays();                                               // 0x1c8740
    /*! \brief Walk the AST once and apply per-`NodeType` rewrites that
     *  prepare each node for branch-counting and play-sizing.
     *
     *  \details Drives a `std::stack<std::shared_ptr<Node>,
     *  std::deque<...>>` initialised with `node`, popping nodes LIFO
     *  and dispatching on `Node::type`:
     *
     *    - `NodeType::Load`: collect the wave name at the node's
     *      device index from `wavesPerDev`, then iterate the
     *      `node->play` weak-pointer range, locking each entry,
     *      resolving its waveform via `wavetableIR_`, and setting
     *      `WaveformIR::markedForLoad`.  Same `markedForLoad`
     *      treatment is applied to every entry of the local
     *      `validWaves` vector.  Then call `collectUsedWaves(node)`
     *      and push `node->next`.
     *    - `NodeType::Play`: forward to `linkLoad(node)`, then push
     *      `node->next`.
     *    - `NodeType::Branch`: forward to `removeBranches(node, stack)`,
     *      which flattens the branch's children onto the same
     *      traversal stack so they are visited in this pass.
     *    - `NodeType::Loop`: push both `node->next` and `node->loop`.
     *    - `NodeType::SetVar`: rewrite via `expandSetVar(node)`, then
     *      push `node->next`.
     *    - `NodeType::Lock`: on Hirzel targets (`config_->isHirzel`)
     *      just push `node->next`.  On non-Hirzel targets, walk the
     *      `next` chain looking for an existing locked play of the
     *      same wave via `findLockedPlay`; on a hit, drop the
     *      current node and merge any pending load with `mergeLoads`,
     *      on a miss synthesise a fresh load via `createLoad`,
     *      copy `node->asmId` onto it, and splice it in via
     *      `Node::insertBefore`.  The walk terminates when no load
     *      is held after a step.
     *    - `NodeType::Table`: forward to `linkLoad(node)`, then push
     *      `node->next`.
     *    - `NodeType::PlainLoad`: call `collectUsedWaves(node)`,
     *      then push `node->next`.
     *    - All other types: push `node->next` only.
     *
     *  \param node  Subtree root to process; typically `root_`.
     */
    void prepareTree(std::shared_ptr<Node> node);                      // 0x1c8870
    /*! \brief Propagate per-node branch counts through `nodeStates_`
     *  and record the program-wide maximum into `maxBranches_`.
     *
     *  \details Stack-driven traversal (LIFO via
     *  `std::deque::back()` / `pop_back()`).  Every visited node is
     *  emplaced into `nodeStates_` with a default-constructed
     *  `PrefetcherNodeState` (which seeds `branchCount = 1`), then
     *  dispatched on `Node::type`:
     *
     *    - `NodeType::Branch`: read the parent's `branchCount`,
     *      compute `newCount = branchCount + branches.size() - 1`,
     *      raise `maxBranches_` to `newCount` when larger, push
     *      `node->next` while copying the parent's count onto its
     *      entry, then for each branch child whose stored
     *      `branchCount` is smaller than `newCount`, overwrite it
     *      and push the child.
     *    - `NodeType::Play` and `NodeType::Table`: skip when
     *      `node->next` is null; otherwise read the parent's
     *      `branchCount` and either write `1` onto the next entry
     *      (when the parent value is zero, marking the very first
     *      visit) or propagate the parent's value, then push `next`.
     *    - All other types: copy `branchCount` onto `node->next`
     *      (when present) and onto `node->loop` (when present), and
     *      push each child that was updated.
     *
     *  \param node  Subtree root to process; typically `root_`.
     */
    void countBranches(std::shared_ptr<Node> node);                    // 0x1c9b30
    /*! \brief Compute each play's effective sample size and wave-
     *  memory footprint, and reserve a length register when a play
     *  needs more than one cache page.
     *
     *  \details Stack-driven traversal that, for every visited node,
     *  pushes `node->next`, every entry of `node->branches`, and
     *  `node->loop` (when present) onto the stack, then continues
     *  unless the node is a `NodeType::Play` whose `config.dummy`
     *  flag is clear.
     *
     *  For each non-dummy `Play`:
     *
     *    1. Resolve the wave name at `deviceIndex` from `wavesPerDev`.
     *    2. Look up `WaveformIR` via
     *       `wavetableIR_->getWaveformByName`, read
     *       `waveform->signal.length_` as `waveLength`.
     *    3. Compute `playSize` by rounding `waveLength` up to the
     *       device `grainSize`, clamped *up* by
     *       `Waveform::minLengthSamples` (the operation taken from
     *       the binary is `if (minLenSamples > playSize) playSize =
     *       minLenSamples`, i.e. a max, not a min).
     *    4. When `playSize != waveLength`, call
     *       `waveform->signal.resizeSamples(playSize)`.
     *    5. Compute `memWords = ceil(channelCount * alignedLen *
     *       bitsPerSample / 8)` using the per-waveform
     *       `DeviceConstants` (`grainSize`, `maxWaveformLength`,
     *       `bitsPerSample`).
     *    6. Compute the per-page budget
     *       `memPerPage = devConst_->waveformMemorySize /
     *       maxBranches_`.
     *    7. When `memPerPage < memWords`, the play does not fit in a
     *       single page: compute `pagesNeeded` (folding in
     *       `node->length` when nonzero, otherwise from the
     *       waveform's own footprint).  When `pagesNeeded >= 2`,
     *       store it into `nodeStates_[node].pagesNeeded`, reserve
     *       a register through `Resources::getRegisterNumber()` and
     *       store the resulting `AsmRegister` in the node's
     *       `registerHirzel` slot (which doubles as the length-
     *       tracking register here), and mirror both the register
     *       and `pagesNeeded` onto the parent's entry when
     *       `node->parent.lock()` succeeds.
     *
     *  \param node  Subtree root to process; typically `root_`.
     */
    void definePlaySize(std::shared_ptr<Node> node);                   // 0x1ca370
    /*! \brief Mark every reachable waveform that fits the device's
     *  fixed-allocation budget as `WaveformIR::fixed_`, optionally
     *  walking the parent chain to make the decision when the
     *  immediate per-waveform cap is exceeded.
     *
     *  \details Stack-driven traversal of `root_` (LIFO via
     *  `std::deque`).  For every `NodeType::Play` node visited:
     *
     *    1. Resolve the wave name at `deviceIndex` from `wavesPerDev`
     *       and look up its `WaveformIR` via `wavetableIR_`.  Skip
     *       when the wave is already `fixed_`.
     *    2. Compute the per-allocation cap
     *       `maxAlloc = devConst_->maxBlocks * waveformAlignment` and
     *       compare it against `WaveformIR::allocationByteSize`.
     *    3. When the wave fits, mark it `fixed_ = true`.
     *    4. When the wave does not fit and this is the very first
     *       processed play, still mark it `fixed_ = true` (the first
     *       play is treated as a forced anchor).  On any subsequent
     *       oversized play, walk `parent.lock()` upwards looking for
     *       another `Play`, `Loop`, or `Branch` ancestor:
     *         - For a `Play` ancestor, recompute its byte footprint
     *           from `bitsPerSample * length` and compare against
     *           `sineNodeBase * waveformAlignment`; stop the walk
     *           when this cap is exceeded.  When the ancestor's
     *           own wave is *not* yet `fixed_`, mark the *current*
     *           wave `fixed_` and stop.  When the ancestor *is*
     *           already `fixed_`, keep walking upwards.
     *         - For a `Loop` or `Branch` ancestor, mark the current
     *           wave `fixed_` and stop.
     *
     *  After processing the play (or for non-`Play` nodes), the
     *  traversal pushes `node->next`, plus either `node->loop` (for
     *  `Loop`) or every entry of `node->branches` (for `Branch`),
     *  onto the worklist.
     *
     *  Called by `Compiler::runPrefetcher` only when the device's
     *  cache type is `1`; the resulting `fixed_` flags subsequently
     *  steer `placeLoads` and the cache allocator.
     */
    void determineFixedWaves();                                        // 0x1cb200
    /*! \brief Decide where each waveform load is emitted in the
     *  scheduled tree, optionally splitting the program when the
     *  waveform working set exceeds the device cache.
     *
     *  \details Steps:
     *    1. Compute `getMemoryHighWatermark()` and, on SHFSG /
     *       SHFQC_SG, abort with a `ZIAWGCompilerException` carrying
     *       the formatted "wave memory exceeded" message when the
     *       watermark is larger than `DeviceConstants::waveformMemorySize`.
     *    2. Compute `getRequiredMemory()`.  If it fits in the cache
     *       *or* the target is Hirzel-class, set `split_ = true` and
     *       call `moveLoadsToFront(root_)` to hoist each load to the
     *       front of the scheduled program (the result becomes the
     *       new working root).
     *    3. On HDAWG with at least two cores, prepend a
     *       `NodeType::SyncHirzel` marker to the working root's
     *       child chain; on HDAWG / SHFQA / SHFSG / SHFQC_SG prepend a
     *       `NodeType::AwgReady` marker.
     *    4. When the program was *not* split (single-shot fits in
     *       cache), look up the appropriate informational message
     *       (`WaveNotFittingCache` for multi-core HDAWG,
     *       `WaveNotFittingCacheGapless` otherwise), report it via
     *       `logFunc_`, then run `optimize(root_)`.
     *    5. `optimizeSync(root_)` and copy the root's `playConfig`
     *       into `cwvfConfig`.
     *    6. `optimizeCwvf(root_)` followed by `allocate(root_, cache_)`
     *       to bind the resulting load nodes to concrete cache slots.
     *
     *  Cancellation is checked indirectly through the optimisation
     *  passes; this method itself does not poll `cancelCb_`.
     *
     *  \throws ZIAWGCompilerException When the SHFSG / SHFQC_SG memory
     *  watermark exceeds `DeviceConstants::waveformMemorySize`.
     */
    void placeLoads();                                                 // 0x1cbf60
    /*! \brief Compute the largest per-device waveform-memory address
     *  reached after subtracting the device's base address, used by
     *  `placeLoads` to decide whether SHFSG / SHFQC_SG programs fit.
     *
     *  \details Iterates `getUsedWavesForDevice(devIdx)` for either
     *  every channel group (when `numChannelGroups >= 2` and
     *  `deviceType == HDAWG`) or just `config_->deviceIndex`
     *  otherwise.  For each waveform:
     *
     *  - When `signal.length_ != 0`, round it up to
     *    `DeviceConstants::grainSize` and clamp the result *down* to
     *    `maxWaveformLength` (`min(rounded, maxWaveformLength)`).
     *  - Otherwise the page count is zero.
     *  - Multiply by `signal.channels_` and `bitsPerSample`, divide
     *    by 8 rounding up.
     *  - Subtract `config_->addressImpl` from `WaveformIR::addressValue`
     *    and add the byte count to give a per-waveform "net memory"
     *    figure.
     *
     *  Returns the maximum of those net figures across all waveforms
     *  on all visited devices.
     *
     *  \return Largest net waveform-memory address (in bytes) reached
     *          across all visited devices and their waveforms.
     */
    size_t getMemoryHighWatermark() const;                             // 0x1cc650
    /*! \brief Compute the per-device sum of waveform-memory bytes
     *  required and return the maximum across devices.
     *
     *  \details Same per-device iteration scheme as
     *  `getMemoryHighWatermark`, but per-waveform the operation is
     *  reversed in two places:
     *
     *  - The page count uses `max(rounded, maxWaveformLength)`
     *    instead of `min` (waveforms below the per-device floor are
     *    inflated to the floor for accounting purposes).
     *  - Per-waveform byte counts are *summed* within a device
     *    rather than max-reduced, and `addressValue` /
     *    `config_->addressImpl` are not subtracted.
     *
     *  Returns the maximum of those per-device sums.  Used by
     *  `placeLoads` to decide whether a single-shot run fits in
     *  cache or needs splitting.
     *
     *  \return Largest per-device sum of waveform-memory bytes.
     */
    size_t getRequiredMemory() const;                                  // 0x1cc930
    /*! \brief Hoist a freshly-synthesised `Load` node ahead of every
     *  waveform on the current device that needs loading, splicing it
     *  into the existing node chain.
     *
     *  \details Returns `node` unchanged when `node` is null or has
     *  no `next` sibling, or when
     *  `getUsedWavesForDevice(config_->deviceIndex)` is empty.
     *
     *  Otherwise iterates the result of `getUsedWavesForDevice` and,
     *  for each `WaveformIR` whose `markedForLoad` flag is set,
     *  constructs a new `Node(NodeType::Load, node->asmId,
     *  config_->numChannelGroups)`, copies the wave name into the
     *  new node's `wavesPerDev[deviceIndex]` slot, sets
     *  `deviceIndex`, reserves a register through
     *  `Resources::getRegisterNumber()` and emplaces a corresponding
     *  `PrefetcherNodeState` (storing the register in
     *  `registerHirzel` or `registerCervino` depending on
     *  `config_->isHirzel`), then walks the original tree to wire
     *  every matching `Play` to the new load and splice it into
     *  place via `Node::updateParent`.
     *
     *  \param node  The current head of the chain to hoist loads
     *               in front of.
     *  \return      The (possibly updated) head of the chain — when
     *               loads were inserted, the first inserted load
     *               becomes the new head.
     */
    std::shared_ptr<Node> moveLoadsToFront(std::shared_ptr<Node> node); // 0x1ccad0
    /*! \brief Core merge / hoist / clone pass that compresses
     *  `Load` nodes against their parent context.
     *
     *  \details Walks the tree LIFO via a `std::deque` worklist
     *  (`push_back` / `back` / `pop_back`).  For each popped node
     *  the parent is resolved from `Node::parent` (weak_ptr) and
     *  one of four cases is taken on the *current* node's type:
     *
     *  - `NodeType::Loop` (`0x08`) — push `next`, then push the
     *    loop body (`Node::loop`).
     *  - `NodeType::Branch` (`0x04`) — push `next`, then push
     *    every child in `Node::branches`.
     *  - `NodeType::Load` (`0x01`) — the optimization target.
     *    Dispatches further on `parent->type`:
     *
     *    - Parent is `SetVar` (`0x10`) and parent's `lengthReg`
     *      equals current's: when the parent's own parent exists
     *      and is not a Loop, copy parent's `asmId` to current;
     *      then push current's `next` and clean up.
     *    - Parent is `Load` (`0x01`): copy parent's `asmId` to
     *      current, then walk the ancestor chain
     *      (`parent->parent->...`) while every step is a `Load`.
     *      For each ancestor call `sameLoads(ancestor, current)`
     *      — on a hit invoke `mergeLoads(current, ancestor)`,
     *      push `root_`, and clean up.  On a full chain miss,
     *      fall through to the waveform-fit check.
     *    - Parent is `Play` (`0x02`): call `sameLoads(current,
     *      parent)`.  On a hit, look up `current` in
     *      `nodeStates_`; if its `pagesNeeded >= 2` push current's
     *      `next` and continue; otherwise reset parent's state,
     *      lock parent's `loadRef`, call `mergeLoads(current,
     *      parent->loadRef)`, push `root_`, and clean up.  On a
     *      miss, fall through to the waveform-fit check.
     *    - Other parent type: look up parent in `nodeStates_` to
     *      grab `usedCache_`, then fall through to the
     *      waveform-fit check.
     *
     *    The waveform-fit tail (sub-cases that did not merge):
     *    resolve the current node's waveform name from
     *    `wavesPerDev[deviceIndex]`, look up its `WaveformIR`
     *    via `wavetableIR_`, compute the byte size as
     *    `channels * ceil(sampleCount / alignment) * alignment *
     *    bitsPerSample / 8` capped by `playConfig.maxSamples`,
     *    divide by `nodeStates_[current].pagesNeeded` to get the
     *    pages required, and if `parent.usedCache_` is too small
     *    push `next` and continue.  Otherwise the waveform fits:
     *    on `refTrack > 0` swap nodes via `Node::swap`, fix up
     *    `asmId`, re-push `root_`; on `refTrack <= 0` clone the
     *    current node, splice the clone in after
     *    `Node::last(current)` via `Node::updateParent`, detach
     *    the original, and call `assignLoad(clone, current,
     *    config_->isHirzel)` before re-pushing `root_`.
     *
     *  - Any other node type — push `next` if present.  No further
     *    work.
     *
     *  Throws `std::out_of_range` when an expected entry is
     *  missing from `nodeStates_`.
     *
     *  \param node  Root of the sub-tree to optimize.  Typically
     *               `root_`, but the algorithm is re-entered on
     *               clones spliced in by other passes.
     */
    void optimize(std::shared_ptr<Node> node);                         // 0x1cdae0
    /*! \brief Reorder `Placeholder` and `Load` nodes so that any
     *  `Placeholder` is positioned immediately before the next
     *  `Load` it precedes.
     *
     *  \details LIFO walk via `std::deque`
     *  (`push_back` / `back` / `pop_back`) starting from `node`.
     *  At each step the children (`next`, every entry in
     *  `branches`, and `loop`) are pushed first; then the current
     *  node's type drives a small state machine that tracks the
     *  most recently seen `Placeholder`:
     *
     *  - `NodeType::Load` — when a pending `Placeholder` is
     *    tracked, call `Node::swap(lastPlaceholder, current)` to
     *    move the placeholder ahead of this load.
     *  - `NodeType::Placeholder` — record the node as the new
     *    `lastPlaceholder`.
     *  - Anything else — clear `lastPlaceholder`.
     *
     *  This guarantees that downstream passes see each
     *  placeholder adjacent to the load it parameterises, which is
     *  the layout the wavetable patch-up code in `placeCommands`
     *  expects.
     *
     *  \param node  Root of the sub-tree to scan.  Usually
     *               `root_`.
     */
    void optimizeSync(std::shared_ptr<Node> node);                     // 0x1cf7b0
    /*! \brief Propagate the running `PlayConfig` (channel-mask,
     *  rate, marker / trigger / precomp flags) down the node
     *  chain so that every node carries the correct
     *  `currentCwvf`.
     *
     *  \details Iterative walk along `next` (with recursion into
     *  `branches` and `loop`).  Carries three running quantities
     *  initialised from the input node: `cwvf` (the running
     *  `PlayConfig`), `globalRate`, and `defaultPrecompFlags`.
     *
     *  Per-node behaviour (key `NodeType` values):
     *
     *  - `Branch` (`0x04`) — write `globalRate`, then for each
     *    branch child propagate the running config and recurse;
     *    after all branches converge, adopt the common state if
     *    every branch agreed (PlayConfig field-by-field, with
     *    special hold/rate handling, plus
     *    `defaultPrecompFlags`); otherwise reset to "invalid"
     *    (channel mask `-1`, all flags zero) so the next `Play`
     *    forces a fresh CWVF instruction.
     *  - `Table` (`0x200`) — write the running config and check
     *    whether the node qualifies for CWVF folding
     *    (`config.dummy || config.hold`, plus rate / precomp
     *    conditions); if so copy the running CWVF over the
     *    node's default config and call `globalCwvf(node)` to
     *    update global agreement.
     *  - `Loop` (`0x08`) — write the config, propagate it into
     *    the loop body, walk the body's `next` chain to its tail,
     *    and adopt the tail's config as the new running state.
     *  - `Rate` (`0x20`) — write the config, then adopt the
     *    node's `defaultPrecompFlags` going forward.
     *  - `SetPrecomp` (`0x1000`) — write the config, then adopt
     *    the node's `globalRate` going forward.
     *  - Any other type — write the running config to the node.
     *
     *  After the per-type handling the running CWVF is also
     *  written to the `Prefetch`-owned tracking fields, then the
     *  walk advances to `node->next`.
     *
     *  Throws `ZIAWGCompilerException` (formatted via
     *  `ErrorMessages::format`) on two structural failures: the
     *  post-`Branch` next-chain scan finding no qualifying
     *  terminator, and the analogous post-`SetPrecomp` scan
     *  failure.
     *
     *  \param node  Root of the sub-tree to propagate through.
     *               Usually `root_`.
     */
    void optimizeCwvf(std::shared_ptr<Node> node);                     // 0x1cfc70
    /*! \brief Reserve and reuse cache slots for every waveform
     *  referenced by the prefetch tree, threading the per-device
     *  `Cache` through scoped child caches at branches and loops.
     *
     *  \details Iterates the linked list rooted at `node` (via
     *  `Node::next`) and dispatches on `Node::type`:
     *
     *  - `Load` (`0x01`) — the heavy case.  Reads the waveform
     *    name from `wavesPerDev[deviceIndex]`, inserts
     *    `(name, true)` into `nameMap_`, then locks the node's
     *    `loadRef` weak_ptr (+0x18).  When the locked load already
     *    has a `Cache::Pointer` whose state is at least 2, copies
     *    the pointer into the current node's `nodeStates_` slot
     *    and advances.  Otherwise, when `Cache::stillInMemory`
     *    reports the load is reusable, calls `Cache::reuse`
     *    followed by `nodeByCachePointer` + `mergeLoads` to fold
     *    the current node into the original.  Failing both reuse
     *    paths, allocates fresh storage: looks up the
     *    `WaveformIR` via `wavetableIR_`, computes the byte size
     *    as `channels * ceil(length/grainSize) * grainSize *
     *    bitsPerSample / 8` (or `playNode->length * channels * 2`
     *    for indexed-play), calls `Cache::allocate(waveIR,
     *    byteSize, nameMap_, maxBranches_, split_)`, and stores
     *    the returned pointer.
     *  - `Play` (`0x02`) — locks the node's load reference and
     *    calls `Cache::play()` on the load's cache pointer.
     *  - `Branch` (`0x04`) — for each entry in `Node::branches`
     *    creates a scoped `Cache` via `cache->getScope()` and
     *    recursively calls `allocate(child, scopedCache)`.
     *  - `Loop` (`0x08`) — when `Node::loop` is non-null, creates
     *    a scoped `Cache` and recurses into the loop body.
     *  - `Lock` (`0x40`) — reads the waveform name and inserts
     *    `(name, true)` into `nameMap_`; no cache work.
     *  - `Unlock` (`0x80`) — same, but inserts `(name, false)`
     *    to clear the mark left by the matching `Lock`.
     *  - `Table` (`0x200`) — locks the node's load reference and
     *    calls `Cache::play()` on the current node's cache
     *    pointer.  When the load reference is null and
     *    `Node::config.dummy` is also clear, throws
     *    `ZIAWGCompilerException` with `ErrorMessages::format`
     *    code 0xA2.
     *  - Any other type — silently advances to `next`.
     *
     *  Every `std::exception` raised inside the loop is caught
     *  and re-thrown as `ZIAWGCompilerException` with
     *  `ErrorMessages::format` code 0xA2 wrapping the original
     *  message.
     *
     *  \param node   Head of the chain to allocate cache for.
     *                Typically `root_` on the top-level call;
     *                child nodes when recursed from `Branch` or
     *                `Loop`.
     *  \param cache  Per-device `Cache` instance (or a scope
     *                derived from one) tracking memory layout
     *                for the current sub-tree.
     */
    void allocate(std::shared_ptr<Node> node, std::shared_ptr<Cache> cache); // 0x1d0fb0
    /*! \brief Return every `WaveformIR` registered for `deviceIdx` in
     *  insertion order, skipping names whose `WavetableIR` lookup
     *  fails.
     *
     *  \details Reads `waveformMaps_[deviceIdx]` (one
     *  `WaveformBimap` per channel group, populated by
     *  `collectUsedWaves`) and walks its right (integer-keyed,
     *  index-ordered) view, calling
     *  `wavetableIR_->getWaveformByName` on each entry and pushing
     *  any non-null result into the returned vector.  Returns an
     *  empty vector when `deviceIdx` is out of range; never throws.
     *  The result count may be smaller than the bimap size when a
     *  wave name no longer resolves.
     *
     *  \param deviceIdx  Zero-based channel-group index into
     *                    `waveformMaps_`.
     *  \return  Vector of `WaveformIR` shared pointers in the order
     *           the names were first registered for this device.
     */
    std::vector<std::shared_ptr<WaveformIR>> getUsedWavesForDevice(size_t deviceIdx) const; // 0x1d2d60
    /*! \brief Register every wave name carried by `node` into the
     *  per-channel-group `waveformMaps_` bimaps.
     *
     *  \details For each channel group `i` in
     *  `[0, config_->numChannelGroups)`, when
     *  `node->wavesPerDev[i].has_value()` the function inserts
     *  `(name, nextIndex)` into `waveformMaps_[i]`, where
     *  `nextIndex` is the current size of the bimap's right view.
     *  Boost bimap insertion is idempotent for existing names — the
     *  recorded index reflects insertion order across all callers.
     *  No-op when `node` is null.
     *
     *  \param node  Node whose `wavesPerDev` slots should be folded
     *               into the global per-device wave registry.
     */
    void collectUsedWaves(std::shared_ptr<Node> node);                 // 0x1d31c0
    /*! \brief Synthesise a `Load` for `node` (a play or table node)
     *  and splice it in immediately before `node` in the IR.
     *
     *  \details Calls `createLoad(node)`; when the result is
     *  non-null, splices it in via `Node::insertBefore(loadNode)`.
     *  Silently does nothing when `createLoad` returns null (wrong
     *  `NodeType`, dummy flag set, or any of the other early-out
     *  conditions in `createLoad`).
     *
     *  \param node  Play (`NodeType::Play`) or table
     *               (`NodeType::Table`) node before which the new
     *               load should appear.
     */
    void linkLoad(std::shared_ptr<Node> node);                         // 0x1d33f0
    /*! \brief Compact a `Branch` node's children, push the
     *  surviving branches and the post-branch `next` onto the
     *  caller's traversal stack, and splice the branch out when no
     *  children remain.
     *
     *  \details No-op when `node` is null or its type is not
     *  `NodeType::Branch`.  Otherwise scans
     *  `node->branches` (`+0xC8`) with a write-pointer pass that
     *  drops null `shared_ptr` entries; if any were removed, sets
     *  `node->branchMaySkipAllBodies = true` (`+0x109`).  When the
     *  vector is now empty, locks `node->parent` and calls
     *  `Node::updateParent(parent, node, node->next)` to splice the
     *  branch out, then pushes `node->next` onto `stack`.
     *  Otherwise pushes `node->next` first (when non-null) followed
     *  by every surviving branch — pushing `next` is essential so
     *  that `prepareTree`'s LIFO walk visits the post-branch code,
     *  not just the branch arms (regression-tested by
     *  `uhf_doc_feedforward`).
     *
     *  \param node   The Branch node to prune.
     *  \param stack  Caller's traversal stack; receives follow-up
     *                nodes (the `next` join and each surviving
     *                branch, or just `next` when the branch is
     *                spliced out).
     */
    void removeBranches(std::shared_ptr<Node> node,
        std::stack<std::shared_ptr<Node>>& stack) const;               // 0x1d3530
    /*! \brief Expand each `SetVar` node in the sibling chain into
     *  one clone per additional channel group.
     *
     *  \details Walks the parent chain upward from `node`; for each
     *  `Loop` ancestor whose body head is the current child, splices
     *  a thin `SetVar` clone (carrying only the original's
     *  `lengthReg`) in front of the loop body via `Node::insertBefore`.
     *  No-op when no ancestor is a `Loop`.  See IF-218 (closed) for
     *  the prior stub history.
     *
     *  \param node  Head of the sibling chain to scan.
     */
    void expandSetVar(std::shared_ptr<Node> node) const;               // 0x1d3af0
    /*! \brief Search the subtree rooted at `node` for a `Play` of
     *  `waveform` that lives inside an unmatched `Lock` scope.
     *
     *  \details LIFO-walks the subtree rooted at `node`, comparing
     *  each `Play` node's wave name against `waveform`'s.  Returns
     *  the matched `Play` node, or `nullptr` if none lies within an
     *  open `Lock` scope.  See IF-213 (closed) for the prior stub
     *  history.
     *
     *  \param node      Subtree root to search.
     *  \param waveform  Target waveform the matching `Play` must
     *                   reference.
     *  \return  The matched `Play` node, or `nullptr` if none.
     */
    std::shared_ptr<Node> findLockedPlay(std::shared_ptr<Node> node,
        std::shared_ptr<WaveformIR> waveform) const;                   // 0x1d3dd0
    /*! \brief Allocate a fresh `Load` IR node for the given play /
     *  table node, register its book-keeping, and mark its
     *  waveforms as load-targeted.
     *
     *  \details Returns `nullptr` when `node` is null, when
     *  `node->type` is neither `NodeType::Play` (`0x02`) nor
     *  `NodeType::Table` (`0x200`), or when `node->config.dummy`
     *  is set.  Otherwise:
     *
     *    1. Reserves a register via `Resources::getRegisterNumber`.
     *    2. Constructs `std::make_shared<Node>(NodeType::Load,
     *       asmId, numChannelGroups)` and copies `wavesPerDev`,
     *       `lengthReg`, and the configured `deviceIndex` onto it.
     *    3. Emplaces the new load into `nodeStates_` and stores
     *       the fresh `AsmRegister` in either `registerHirzel`
     *       (`+0x00` of `PrefetcherNodeState`) or `registerCervino`
     *       (`+0x08`) depending on `config_->isHirzel`.
     *    4. Calls `assignLoad(node, result, isHirzel)` to back-link
     *       the source play.
     *    5. Pushes `node` (as a weak_ptr) into `result->play`
     *       (`+0xA0`).
     *    6. For every named wave on `node`, looks up the
     *       `WaveformIR` via `wavetableIR_` and sets
     *       `markedForLoad = true` (`+0xD8`; this is **not** the
     *       neighbouring `fixed_` flag at `+0xD9`).
     *    7. Calls `collectUsedWaves(node)`.
     *
     *  Returns `nullptr` early when `node`'s parent is a `Load`
     *  that already references this play, avoiding a duplicate
     *  load.  See IF-219 (closed) for the prior history of the
     *  missing short-circuit.
     *
     *  \param node  Source `Play` or `Table` node.
     *  \return  The freshly constructed `Load`, or `nullptr` when
     *           the source is ineligible.
     */
    std::shared_ptr<Node> createLoad(std::shared_ptr<Node> node);       // 0x1d4a10
    /*! \brief Fold every play-target of `src` (a `Load`) onto
     *  `dst` (also a `Load`) and remove `src` from the IR.
     *
     *  \details No-op when either argument is null or either is not
     *  of `NodeType::Load`.  Otherwise iterates `src->play`
     *  (`+0xA0`); for each weak_ptr that locks to a non-null
     *  target, calls `assignLoad(target, dst, config_->isHirzel)`
     *  to redirect the target's `loadRef` and copy its register
     *  slot, then linear-scans `dst->play` and pushes the same
     *  weak_ptr when not already present.  After the loop,
     *  unconditionally calls `Node::remove(src)` to splice `src`
     *  out of the tree, regardless of how many targets were
     *  successfully reassigned.  Callers must drop their own
     *  references to `src` after the call.
     *
     *  \param node   The destination `Load` (absorbs `other`'s
     *                play-targets); the parameter is named
     *                `dst` in the implementation.
     *  \param other  The source `Load`, removed from the IR on
     *                return; named `src` in the implementation.
     */
    void mergeLoads(std::shared_ptr<Node> node,
        std::shared_ptr<Node> other);                                  // 0x1d5040
    /*! \brief Back-link a play node to a load and copy the matching
     *  per-device register slot from the load's bookkeeping onto
     *  the play's bookkeeping.
     *
     *  \details No-op when `load` is null.  Otherwise sets
     *  `node->loadRef = load` (overwriting any prior weak_ptr),
     *  emplaces both `load` and `node` into `nodeStates_`
     *  (default-constructing `PrefetcherNodeState` on first
     *  insertion), and copies one register field from the load's
     *  state to the play's: `registerHirzel` (`+0x00`) when
     *  `isHirzel` is true, otherwise `registerCervino` (`+0x08`).
     *  The selection is governed entirely by `isHirzel`, not by
     *  which slot is actually populated on the load.
     *
     *  \param node      Play node to back-link.
     *  \param load      Load node owning the register slot to copy
     *                   (early-out when null).
     *  \param isHirzel  Selects the Hirzel (`+0x00`) vs Cervino
     *                   (`+0x08`) register slot.
     */
    void assignLoad(std::shared_ptr<Node> node,
        std::shared_ptr<Node> const& load, bool isHirzel);                 // 0x1d53a0
    /*! \brief Maintain `cwvfConfig_` / `globalCwvfValid_` against
     *  successive `Play` nodes for the global-CWVF optimisation.
     *
     *  \details No-op when `node` is null or `node->type` is not
     *  `NodeType::Play` (`0x02`).  Optionally bails out early when
     *  the node carries the `dummy` (`+0x66`) or `hold` (`+0x65`)
     *  flag and either `rate == 0` or
     *  (`rate == -1 && globalRate <= 0`) and the device has
     *  precomp (`devConst_->hasPrecomp & 1`).
     *
     *  Otherwise:
     *    - On the very first eligible call (`cwvfConfig_.channelMask
     *      == 0xFFFFFFFF` sentinel, set by the constructor), copies
     *      `node->config` into `cwvfConfig_`, stashes the node in
     *      `lastCwvfNode_`, and sets `globalCwvfValid_ = true`.
     *    - On subsequent calls, compares each PlayConfig field
     *      (`channelMask`, `rate`, `suppress`, `is4Channel`,
     *      `markerBits`, `trigger`, `precompFlags`, `dummy`; `hold`
     *      only when `cwvfConfig_.rate > 0`); on any mismatch sets
     *      `globalCwvfValid_ = false`.  The flag is never re-set to
     *      true after the first call.
     *
     *  \param node  Candidate `Play` node to fold into the global
     *               CWVF check.
     */
    void globalCwvf(std::shared_ptr<Node> node);                       // 0x1d5620
    /*! \brief Populate every visited node's `parent` weak_ptr to
     *  point at its IR predecessor.
     *
     *  \details LIFO walk via a `std::deque<shared_ptr<Node>>`
     *  worklist seeded with `node`.  For each popped current node:
     *    - When `cur->next` is non-null, sets
     *      `cur->next->parent = current` and pushes `cur->next`.
     *    - For every entry of `cur->branches`, sets
     *      `child->parent = current` and pushes `child`.
     *    - When `cur->loop` is non-null, sets
     *      `cur->loop->parent = current` and pushes `cur->loop`,
     *      so loop-body children also receive a parent back-link.
     *      See IF-217 (closed) for the prior bug.
     *
     *  \param node  Subtree root whose descendants should be
     *               back-linked.
     */
    void backwardTree(std::shared_ptr<Node> node) const;                // 0x1d57d0
    /*! \brief Decide whether two load-bearing nodes can share a
     *  single load (same wave name, page count, and length
     *  register).
     *
     *  \details Reads `wavesPerDev[deviceIndex]` for both `a` and
     *  `b` (treating a negative `deviceIndex` as "no name").  When
     *  exactly one side has a value, returns `false`; when both
     *  have values their strings must compare equal; when both are
     *  empty the comparison continues.  Then looks up both nodes
     *  in `nodeStates_` and returns `false` if their `pagesNeeded`
     *  values differ (`PrefetcherNodeState::pagesNeeded`,
     *  `PNS+0x1C` per `prefetch.hpp:192`).  Finally returns whether
     *  `a->lengthReg` equals `b->lengthReg` (`+0x88`).
     *
     *  \throws std::out_of_range  if either node is missing from
     *          `nodeStates_`.  Callers must therefore have run a
     *          phase that registers both (e.g. `assignLoad`,
     *          `createLoad`, or `allocate`) before invoking
     *          `sameLoads`.
     *
     *  \param a  First node to compare.
     *  \param b  Second node to compare.
     *  \return  `true` only if the wave names, `pagesNeeded`, and
     *           `lengthReg` all match.
     */
    bool sameLoads(std::shared_ptr<Node> a,
        std::shared_ptr<Node> b) const;                                // 0x1d5e20
    /*! \brief Find the first `Load` node whose wave name matches
     *  the cache pointer's string.
     *
     *  \details LIFO walk over `root_` via a
     *  `std::deque<shared_ptr<Node>>`.  For each visited node,
     *  when `n->type == NodeType::Load` (`1`) and
     *  `n->deviceIndex >= 0` and
     *  `wavesPerDev[deviceIndex].has_value()`, compares the wave
     *  name against `ptr->str()`; on match returns `n`.  Children
     *  are pushed in order: every entry of `branches`, then `loop`
     *  (when non-null), then `next` (when non-null), so traversal
     *  order is determined by the resulting LIFO stack.  Returns
     *  `nullptr` when no Load matches.
     *
     *  \param ptr  Cache pointer whose wave-name string is the
     *              search key.
     *  \return  The first matching `Load` node in LIFO traversal
     *           order, or `nullptr`.
     */
    std::shared_ptr<Node> nodeByCachePointer(
        std::shared_ptr<Cache::Pointer> ptr) const;                    // 0x1d60d0

    // Asm placement
    /*! \brief Produce the prefetch-aware assembly list by copying
     *  `asmList` and walking the scheduled node tree to expand each
     *  load placeholder into the concrete instructions that perform
     *  the load.
     *
     *  \details Allocates an `AsmList out(asmList)` (a deep copy that
     *  preserves the original `sequenceId`-keyed entries used by
     *  `findPlaceholder`), then calls `placeCommands(&out, root_)`
     *  which:
     *    - emits the program-wide `cwvf` (or `addi` + `cwvfr` for
     *      large defaults) at the slot just past any leading
     *      `NodeType::Loop` placeholder when this is the root and
     *      `globalCwvfValid_` is false; and
     *    - walks the `Node::next` chain calling `placeSingleCommand`
     *      on each node, polling `cancelCb_` between iterations and
     *      breaking the walk when a cancellation is observed.
     *
     *  Each `placeSingleCommand` invocation locates the matching
     *  placeholder slot in `out` via `findPlaceholder` (linear scan
     *  by `sequenceId`) and replaces it with the load instruction
     *  sequence selected for that node, including the Hirzel-only
     *  cache-clamped `prf` prefetch sequence when the node's
     *  `crossesCacheLine_`-derived flag is set on the corresponding
     *  `PrefetcherNodeState`.
     *
     *  \param asmList  The instruction list produced by the pre-
     *                  waveform optimisation pass; it carries the
     *                  load placeholders that `placeCommands`
     *                  expands.  Not modified.
     *  \return         A new `AsmList` with every placeholder
     *                  rewritten into the concrete load sequence
     *                  chosen by `placeLoads`.
     *  \throws ZIAWGCompilerException  Propagated from
     *                  `findPlaceholder` when a node's `asmId` does
     *                  not match any entry in the copied list
     *                  (programmer-error condition).
     */
    AsmList fillInPlaceholders(AsmList const& asmList);                // 0x1d65c0

    /*! \brief Walk a Node linked list and emit assembly for each node.
     *
     *  \details
     *  When invoked on the root node, first emits a leading `cwvf`
     *  instruction (or `addi`+`cwvfr` pair when the encoded value
     *  exceeds the immediate range) carrying the default rate / suppress
     *  bits derived from `PlayConfig::defaultRateShift` /
     *  `PlayConfig::suppressShift`.  The instruction is inserted just
     *  past any leading entries whose attached node is of type
     *  `NodeType::Branch` (a transient placement marker), so that the
     *  global cwvf precedes the first real emission.  This per-root
     *  emission happens at most once per `placeCommands` call and is
     *  gated by `globalCwvfValid_`.
     *
     *  Then walks the `Node::next` chain, calling `placeSingleCommand`
     *  on each node until the chain ends or the cancellation callback
     *  reports cancellation.  Branches and loop bodies are visited
     *  recursively from inside `placeSingleCommand` (cases 4 / 8).
     *
     *  \param out   Output `AsmList` whose placeholder slots are
     *               progressively replaced by real instructions.
     *  \param node  Head of the node chain to emit.  Empty pointer is a
     *               no-op.
     */
    void placeCommands(AsmList* out, std::shared_ptr<Node> node);      // 0x1d6680

    /*! \brief Emit assembly for a single node, dispatching on
     *         `Node::type`.
     *
     *  \details
     *  Locates the placeholder slot for the node via `findPlaceholder`,
     *  copies the slot's wavetable-front context into the assembler,
     *  then dispatches on the `NodeType` value at `Node+0x44`:
     *
     *  - `Load (0x01)`         : allocates the Hirzel state register if
     *                            needed, emits `addi` for the wave
     *                            address, optionally an `addi` for the
     *                            cervino state register, then either a
     *                            cache-aware `prf` (Hirzel,
     *                            cross-cache-line case) or routes into
     *                            the indexed-play / cervino-prf paths.
     *  - `Play (0x02)`         : emits the playWave sequence, including
     *                            the cervino non-split path with `cwvf`,
     *                            ssl loops, `addr`, and `prf`
     *                            instructions; routes through
     *                            `splitPlay` when `split_` is set and
     *                            through indexed variants when the node
     *                            carries a length register.
     *  - `Branch (0x04)`       : recurses with `placeCommands` for each
     *                            entry of `Node::branches`.
     *  - `Loop (0x08)`         : recurses with `placeCommands` for
     *                            `Node::loop`.
     *  - `SyncCervino (0x100)` : emits the cervino synchronisation
     *                            sequence inline.
     *  - `Table (0x200)`       : encodes the table cwvf and emits it,
     *                            either as a single `cwvf` or as
     *                            `addi`+`cwvfr` for large immediates.
     *                            \verifyme  The `Table` arm is
     *                            reconstructed from disassembly only
     *                            and is unverifiable from SeqC inputs:
     *                            `NodeType::Table` is unreachable from
     *                            the SeqC front-end (see IF-249).
     *  - `SyncHirzel (0x2000)` : emits `asmSyncHirzel` when the device
     *                            is HDAWG with at least two channel
     *                            groups; otherwise no-op.
     *  - `PlainLoad (0x4000)`  : emits `addi` + cache-clamped `prf` for
     *                            the wave at the node's current device
     *                            index.
     *  - `AwgReady (0x8000)`   : emits a single `st(R0, 0x92)`.
     *
     *  All emitted instructions are accumulated into a temporary
     *  `AsmList` and inserted into `out` at the placeholder position
     *  (re-found on each insert because the underlying vector may
     *  reallocate).
     *
     *  \param out   Output `AsmList` containing placeholder slots for
     *               this node's instructions.
     *  \param node  Node whose instructions to emit; must be the same
     *               shared pointer used when the placeholder was
     *               registered.
     *
     *  \throws ZIAWGCompilerException  Propagated from
     *          `findPlaceholder` if the node has no placeholder slot
     *          in `out`.
     */
    void placeSingleCommand(AsmList* out, std::shared_ptr<Node> node); // 0x1d7940

    /*! \brief Locate the placeholder `Asm` slot reserved for a given
     *         node.
     *
     *  \details
     *  Linear scan over `out` looking for the entry whose `sequenceId`
     *  matches `node->asmId`.  Returns a raw pointer into the
     *  underlying vector so callers can compute an iterator (and
     *  recompute it after each `insert`, since the vector may
     *  reallocate).
     *
     *  \param out   `AsmList` to search.  Must have been seeded with
     *               placeholder slots by `fillInPlaceholders`.
     *  \param node  Node whose placeholder is wanted; the lookup key is
     *               `node->asmId`.
     *  \return  Pointer to the matching placeholder slot.  Never
     *           `nullptr` on the success path.
     *
     *  \throws ZIAWGCompilerException  When no placeholder with a
     *          matching `sequenceId` is found in `out`.
     */
    AsmList::Asm* findPlaceholder(AsmList* out,
        std::shared_ptr<Node> node);                                   // 0x1d6b50

    // Waveform instruction helpers

    /*! \brief Clamp a waveform address to the device's available cache
     *         capacity.
     *
     *  \details
     *  For non-Hirzel devices the address is simply clamped to the
     *  20-bit hardware limit (`0xFFFFF`).
     *
     *  For Hirzel devices the cache capacity is computed from the
     *  device's per-cache-type page count
     *  (`DeviceConstants::cachePageCount` for `cacheType == 0`,
     *  `DeviceConstants::maxBlocks` for `cacheType == 1`) multiplied by
     *  `DeviceConstants::waveformAlignment` (the page size).  The
     *  result is `min(capacity, addr, 0xFFFFF)`.  When the cache type
     *  is 1 (aligned) the final value is rounded up to the next
     *  page-size boundary so the emission lands on a grain edge.
     *
     *  \param addr  Address (or size) to clamp, in bytes.
     *  \return  Clamped address suitable as the immediate of a `prf` /
     *           cache-aware emission.
     */
    detail::AddressImpl<uint32_t> clampToCache(
        detail::AddressImpl<uint32_t> addr) const;                     // 0x1d6c40

    /*! \brief Emit a waveform-play instruction (`wvf` / `wvfi`) with
     *         an immediate offset.
     *
     *  \details
     *  Produces an `AsmList` containing either a single `wvf`/`wvfi`
     *  instruction (when `offset` fits in the 20-bit immediate, i.e.
     *  `< 0x100000`) or an `addi` + `wvf`/`wvfi` pair (when the offset
     *  is too large, in which case a freshly allocated temporary
     *  register is loaded with the offset and the play instruction
     *  uses that register with a zero immediate).
     *
     *  \param reg      Wave-state register to play from.
     *  \param offset   Byte offset into the waveform memory.
     *  \param indexed  When `true`, emit the indexed variant `wvfi`;
     *                  when `false`, emit `wvf`.
     *  \return  AsmList holding one or two instructions ready to be
     *           spliced into the placement output.
     */
    AsmList wvfImpl(AsmRegister reg, int offset, bool indexed) const;  // 0x1d6ca0

    /*! \brief Emit a waveform-play instruction (`wvf` / `wvfi`) whose
     *         offset comes from a register.
     *
     *  \details
     *  Allocates a temporary address register, copies `offset` into it
     *  with `addi(tempReg, offset, 0)`, then emits an `ssl(tempReg)`
     *  to set the sample length and finally a `wvf` or `wvfi`
     *  instruction (depending on `indexed`) using `reg` as the
     *  wave-state register and the temporary as the address with a
     *  zero immediate.
     *
     *  \param reg      Wave-state register to play from.
     *  \param offset   Register holding the byte offset into waveform
     *                  memory.
     *  \param indexed  When `true`, emit the indexed variant `wvfi`;
     *                  when `false`, emit `wvf`.
     *  \return  AsmList holding the addi + ssl + wvf/wvfi sequence.
     */
    AsmList wvfRegImpl(AsmRegister reg, AsmRegister offset,
        bool indexed) const;                                           // 0x1d7020

    /*! \brief Emit a waveform-set (`wvfs`) instruction sequence.
     *
     *  \details
     *  When `offset < 0x100000`, emits a single
     *  `wvfs(playDummyType, reg, offset)`.  When `reg` is invalid it
     *  is replaced with `R0` for both branches.
     *
     *  When `offset >= 0x100000`, splits the address into a high-bits
     *  `addi` + low-bits `wvfs` pair: a temporary register is loaded
     *  with `addi(tempReg, reg, offset - 0xFFFFF)` and then
     *  `wvfs(playDummyType, tempReg, 0xFFFFF)` is emitted.  When the
     *  caller's `reg` was originally invalid (defaulted to `R0`), the
     *  configuration has at least two channel groups, the address
     *  computed from `DeviceConstants::maxWaveformLength`,
     *  `DeviceConstants::grainSize` and
     *  `DeviceConstants::bitsPerSample` exceeds 20 bits, and the
     *  emitted `addi` is a single `ADDI` instruction, an extra
     *  `addiu(tempReg, tempReg, 0)` is appended to extend the address
     *  into the upper register half before the `wvfs`.
     *
     *  \param type    Dummy / hold flavour selecting which `wvfs`
     *                 opcode variant to emit.
     *  \param reg     Base register; replaced with `R0` when invalid.
     *  \param offset  Byte offset into waveform memory.
     *  \return  AsmList containing the wvfs sequence (1, 2, or 3
     *           instructions depending on the path taken).
     */
    AsmList wvfs(Assembler::PlayDummyType type,
        AsmRegister reg, int offset) const;                            // 0x1d73e0

    // Analysis

    /*! \brief Determine whether a node requires a fresh `cwvf`
     *         instruction.
     *
     *  \details
     *  Walks up `Node::parent` weak pointers comparing the node's
     *  `currentCwvf` (`Node+0x68`, a `PlayConfig`) against ancestor
     *  configurations.  Returns `true` as soon as any of the always-
     *  compared `PlayConfig` fields (`channelMask`, `rate`,
     *  `suppress`, `is4Channel`, `markerBits`, `trigger`,
     *  `precompFlags`, `dummy`) differs from the corresponding field
     *  on an ancestor; the `hold` field participates in the
     *  comparison only when `rate > 0`.
     *
     *  Has one early-out: a Play node whose `config.dummy` or
     *  `config.hold` is set, whose `rate` is either 0 or
     *  `(-1 && globalRate <= 0)`, on a device with `hasPrecomp`,
     *  reports `false` immediately — the precompensation path takes
     *  over the cwvf programming.
     *
     *  Recurses into the parent chain when the parent is a `Loop`
     *  whose first comparison matches but whose containing structure
     *  still has to be examined.
     *
     *  \param node  Node whose cwvf requirement is being decided;
     *               must have a populated `parent` link in the
     *               typical use.
     *  \return  `true` if a new `cwvf` instruction must precede the
     *           node's emission, `false` if the parent chain already
     *           guarantees the same configuration.
     */
    bool needsNewCwvf(std::shared_ptr<Node> node) const;               // 0x1dc620

    /*! \brief Generate the assembly sequence for a waveform play that
     *         spans more than one cache page.
     *
     *  \details
     *  Computes the total waveform byte length, either from the
     *  node's pre-computed length × channel count × 2 fast path
     *  (when `Node::length` is non-zero) or from the waveform's raw
     *  signal data rounded up against
     *  `DeviceConstants::maxWaveformLength` and
     *  `DeviceConstants::bitsPerSample` (when the length must be
     *  derived).
     *
     *  Determines how many full cache half-pages fit and what
     *  remainder is left over by consulting the load node's cache
     *  pointer (`PrefetcherNodeState::cachePtr->numRepeats_` and
     *  `size_`).  Allocates registers for the address, optional
     *  per-PlayConfig copy, optional cervino register, and the
     *  Hirzel state register; emits the initial `addi` carrying the
     *  base address (waveform offset + total length) and one
     *  per-channel `ssl` plus an `addr` to publish the address.
     *
     *  Generates three labels (`play`, `last`, `done`) and emits the
     *  loop body: a first `insertPlay` for the main half-page,
     *  followed by an `addi` to advance the wave register by half a
     *  page, an `addi` + `subr` to maintain a loop counter, an
     *  optional `brgz` that skips the remainder when the count
     *  reaches zero, optional `prf` + `wprf` for non-Hirzel devices,
     *  a `brz` back-edge to the `play` label, a second `insertPlay`
     *  for the remainder (when non-zero), and finally the `done`
     *  label.
     *
     *  \param node  Play node whose waveform must be split across
     *               cache pages.
     *  \return  AsmList containing the entire split-play sequence
     *           ready to be inserted at the node's placeholder.
     */
    AsmList splitPlay(std::shared_ptr<Node> node) const;               // 0x1dd1a0

    /*! \brief Append a labelled play sequence to an in-progress
     *         AsmList.
     *
     *  \details
     *  Emits, in order:
     *
     *  1. An `asmLabel(name)` carrying the supplied label string.
     *  2. The `wvfImpl(reg, addrA, indexed)` waveform-play sequence.
     *  3. On non-Hirzel devices, an `xnori(reg, reg, addrB)` to
     *     mask the cache hash bits into the wave-state register.
     *
     *  \param list     Output AsmList; new instructions are pushed
     *                  to the back.
     *  \param indexed  Forwarded to `wvfImpl`; selects `wvfi`
     *                  versus `wvf`.
     *  \param name     Label name to emit (typically one of the
     *                  `play` / `last` labels generated by
     *                  `splitPlay`).
     *  \param reg      Wave-state register that the play will use
     *                  and that the optional `xnori` will mask.
     *  \param addrA    Byte offset for the play instruction.
     *  \param addrB    Cache hash mask used by the `xnori` mask
     *                  step on non-Hirzel devices; ignored on
     *                  Hirzel.
     */
    void insertPlay(AsmList& list, bool indexed, std::string const& name,
        AsmRegister reg, detail::AddressImpl<uint32_t> addrA,
        detail::AddressImpl<uint32_t> addrB) const;                    // 0x1def50

    // Cache analysis

    /*! \brief Recursively sum the cache footprint of a node and its
     *         descendants.
     *
     *  \details
     *  Walks the node's `next`, `loop`, and `branches` links and
     *  accumulates each visited node's
     *  `PrefetcherNodeState::usedCache_` contribution.  The result
     *  feeds the debug printer (`Prefetch::print`), which displays a
     *  `(usedCache, perNodeUsedCache)` pair next to each Play node.
     *  See IF-226 (closed) for the prior stub history.
     *
     *  \param node  Root of the subtree whose cache footprint is
     *               wanted.
     *  \return  Total cache bytes consumed by the subtree.
     */
    int getUsedCache(std::shared_ptr<Node> node) const;                // 0x1c7eb0

    // Queries

    /*! \brief Compute the union of channels used by all recorded
     *         playWave configurations.
     *
     *  \details
     *  Iterates over `usageEntries_` (every `PlayConfig` collected
     *  by the prefetcher across the placement pass) and reduces
     *  `~entry.config.suppress` with bitwise OR.  Because `suppress`
     *  is a bitmask of channels that the entry actively suppresses,
     *  inverting and OR-reducing yields the set of channels actually
     *  driven by at least one play — i.e. the channels the device
     *  is "using" for output.
     *
     *  \return  Bitmask of channels driven by at least one playWave
     *           configuration; `0` if `usageEntries_` is empty.
     */
    uint32_t getUsedChannels() const;                                  // 0x1df2f0

    /*! \brief Report whether any recorded playWave configuration uses
     *         four-channel mode.
     *
     *  \details
     *  Linear scan of `usageEntries_` returning `true` on the first
     *  entry whose `PlayConfig::is4Channel` flag is set.  Returns
     *  `false` if the vector is empty or no entry has the flag.
     *
     *  \return  `true` when at least one play uses four-channel mode.
     */
    bool getUsedFourChannelMode() const;                               // 0x1df400

    // Print/debug

    /*! \brief Recursively dump a node tree to `std::cout` for
     *         debugging.
     *
     *  \details
     *  Walks the `Node::next` linked list starting at `node`,
     *  emitting one human-readable line per node prefixed by
     *  `[<indent-spaces>]` and dispatching on `Node::type` to format
     *  type-specific fields:
     *
     *  - `Load`        : `load <wave> (<pagesNeeded>) [@<addr>]
     *                    [@<load-addr> (load-asmID <id>)] with R<reg>
     *                    asmID <id> rate <r> globalRate <g>
     *                    precompFlags <p>`.
     *  - `Play`        : same body as `Load` plus
     *                    `pointing to asmID <id> ...` for each entry
     *                    of `Node::play` and a trailing
     *                    `(<getUsedCache>, <usedCache_>)` pair.
     *  - `Branch`      : `branch` per entry of `Node::branches`,
     *                    each recursing with `indent+2`.
     *  - `Loop`        : `loop` followed by recursion into
     *                    `Node::loop` with `indent+2`.
     *  - `SetVar`      : `setvar  R<reg> asmID <id>`.
     *  - `Rate`        : `rate <globalRate>`.
     *  - `Lock` /
     *    `Unlock`      : `lock <wave>` / `unlock <wave>`.
     *  - `SyncCervino` : `sync_cervino`.
     *  - `Table`       : same body as `Play` but always uses
     *                    `registerCervino`.
     *  - `SetPrecomp`  : `setPrecomp <precompFlags>`.
     *  - `SyncHirzel`  : `sync_hirzel asmID <id>`.
     *  - `PlainLoad`   : `plainload <wave> (<pagesNeeded>) [@<addr>]
     *                    with R<reg> asmID <id>`.
     *  - `AwgReady`    : `awg_ready asmID <id>`.
     *  - default       : warning log "Unknown Node::NodeType with
     *                    code <type>." and stop.
     *
     *  When `node` is null the function falls back to `root_`; if
     *  both are null it returns silently.  Output goes directly to
     *  `std::cout`, not through the configured log function.
     *
     *  \param node    Head of the chain to print; null falls back to
     *                 `root_`.
     *  \param indent  Number of leading spaces to print inside the
     *                 `[ ]` prefix (used to convey tree depth as
     *                 callers recurse with `indent+2`).
     */
    void print(std::shared_ptr<Node> node, int indent) const;         // 0x1c5dd0

    // Static
    //! \brief Lower bound on the played-region size that the
    //!        prefetcher will track as an indexed load.
    //! \details Programs that play smaller chunks fall through
    //! to the non-indexed path so the cache-residency planner
    //! avoids fragmenting its book-keeping on negligible
    //! waveform slices.  Stored in BSS at `0xb846d8` in the
    //! binary; mutated only by tests / debug overrides.
    static int minIndexedSize;  // BSS at 0xb846d8

private:
    //! \brief Bidirectional map between optional waveform names
    //!        and the integer indices used in the assembler
    //!        stream.
    //! \details Used by `assignLoad` and the load-merging
    //! passes to deduplicate identical waveforms across the
    //! emitted `AsmList`.  Anonymous waveforms (e.g. inline
    //! literals) carry `std::nullopt` as the name half.
    using WaveformBimap = boost::bimaps::bimap<
        std::optional<std::string>, unsigned long>;

    //! \brief Per-compilation configuration; captured by raw
    //!        pointer and owned by the enclosing
    //!        `AWGCompilerImpl`.
    AWGCompilerConfig const*                               config_;         // +0x00
    //! \brief Per-device geometry / feature table; captured by
    //!        raw pointer and owned by the enclosing
    //!        `AWGCompilerImpl`.
    DeviceConstants const*                                 devConst_;       // +0x08
    //! \brief Per-AST-node prefetch state populated during
    //!        `prepareTree` / `countBranches` / `definePlaySize`
    //!        and consumed by `placeLoads`.
    std::unordered_map<std::shared_ptr<Node>,
                       PrefetcherNodeState>                nodeStates_;     // +0x10 (0x28 bytes)
    //! \brief Set of waveform names already seen during the
    //!        prepare phase; used to detect duplicate
    //!        registrations.
    std::unordered_map<std::string, bool>                  nameMap_;        // +0x38 (0x28 bytes)
    //! \brief Root of the lowered SeqC AST being analysed.
    std::shared_ptr<Node>                                  root_;           // +0x60
    //! \brief Shared `AsmCommands` registry used to append the
    //!        load / play / cwvf / wvfs instructions emitted
    //!        during fill-in.
    std::shared_ptr<AsmCommands>                           asmCommands_;    // +0x70
    //! \brief Pipeline-wide `Resources` container shared with
    //!        the rest of the compile.
    std::shared_ptr<Resources>                             resources_;      // +0x80
    //! \brief Cache-residency planner that owns the on-device
    //!        waveform-memory book-keeping consulted during
    //!        `placeLoads`.
    std::shared_ptr<Cache>                                 cache_;          // +0x90
    //! \brief One bidirectional name → index map per cache
    //!        page, indexed by page number.
    std::vector<WaveformBimap>                             waveformMaps_;   // +0xA0
    //! \brief Maximum number of conditional branches reachable
    //!        from a node; computed by `countBranches` and
    //!        consumed by `definePlaySize`.
    int32_t                                                maxBranches_;    // +0xB8 (init=1, used by countBranches/definePlaySize)
    //! \brief Flag set by the splitter when a play must be
    //!        broken across multiple load chunks.
    bool                                                   split_;         // +0xBC (init=false)
    // 3 bytes padding
    //! \brief Working `PlayConfig` used by the `cwvf` /
    //!        `globalCwvf` passes; reset between plays and
    //!        threaded through `placeCommands`.
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
    //! \brief Per-cwvf usage record collected as the prefetch pass walks
    //! `playWave` sites.
    //!
    //! \details Each entry pairs a `PlayConfig` (channel mask, rate,
    //! suppress flag, marker bits, trigger, precomp flags, hold /
    //! `dummy` slots) with whatever auxiliary state the cwvf validity
    //! analysis needs.  The 0x20 byte stride observed in the
    //! `usageEntries_` vector grow operations matches `sizeof(PlayConfig)`
    //! exactly, so no additional fields are present in the binary.
    struct UsageEntry {
        PlayConfig config;  //!< Captured `PlayConfig` for one observed `playWave` site.  First 0x20 bytes is a PlayConfig.
        //! \brief Default-constructs an `UsageEntry` holding a zero-initialised `PlayConfig`.
        UsageEntry() = default;
        //! \brief Wraps an existing `PlayConfig` (implicit; used by the
        //! prefetch pass to push the current play context onto the usage list).
        //! \param pc Configuration captured for this usage record.
        UsageEntry(const PlayConfig& pc) : config(pc) {}
    };

    //! \brief Per-cwvf usage entries (one `PlayConfig` per
    //!        observed play) feeding the cwvf-validity tracking
    //!        machinery.
    std::vector<UsageEntry>                                usageEntries_;  // +0xE0 (24 bytes)
    //! \brief Last AST node visited by the `globalCwvf` pass;
    //!        used to detect repeat hits without re-walking the
    //!        tree.
    std::shared_ptr<Node>                                  lastCwvfNode_;  // +0xF8 (last node seen in globalCwvf)
    //! \brief Whether the running `globalCwvf` accumulator is
    //!        still consistent across the AST walk.
    bool                                                   globalCwvfValid_; // +0x108
    // 7 bytes padding
    //! \brief Output `WavetableIR` produced by `placeLoads` and
    //!        consumed by the ELF writer.
    std::shared_ptr<WavetableIR>                           wavetableIR_;   // +0x110
    //! \brief Diagnostic sink for non-fatal prefetcher
    //!        messages; supplied by the embedding compiler.
    std::function<void(std::string const&)>                logFunc_;       // +0x120 (0x28 bytes)
    // logFunc_.__buf_ at +0x120 (32 bytes), logFunc_.__f_ at +0x140
    // 8 bytes padding at +0x148
    //! \brief Cancellation hook polled at each prefetch phase
    //!        boundary; an expired `weak_ptr` disables
    //!        cancellation.
    std::weak_ptr<CancelCallback>                          cancelCb_;      // +0x150
    // Total: 0x160 bytes
};

} // namespace zhinst
