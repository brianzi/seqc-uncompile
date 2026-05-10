// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableIR — IR-stage wavetable, wraps WavetableManager<WaveformIR>
//
// Total size: 0xC8 bytes (200 bytes)
//
// Confirmed from:
//   - WavetableIR ctor (from WavetableFront) at 0x29ce20
//   - WavetableIR ctor (from WavetableManager) at 0x29d230
//   - WavetableIR destructor at 0x29d550
//   - Field accesses throughout all methods
// ============================================================================
#pragma once

#include "zhinst/waveform/waveform_ir.hpp"
#include "zhinst/waveform/wave_index_tracker.hpp"
#include "zhinst/device/device_constants.hpp"
#include "zhinst/asm/address_impl.hpp"
#include "zhinst/io/cached_parser.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <set>
#include <functional>

namespace boost { namespace filesystem { class path; } }
namespace boost { namespace property_tree {
    template<typename Key, typename Data, typename KeyCompare> class basic_ptree;
    typedef basic_ptree<std::string, std::string, std::less<std::string>> ptree;
} }

namespace zhinst {

class WavetableFront;
class CancelCallback;

namespace detail {
    template<typename T> class WavetableManager;
}  // AddressImpl now via #include "zhinst/asm/address_impl.hpp"

enum class WaveOrder : int {
    Natural = 0,       // No sorting
    ByWaveIndex = 1,   // Sort by wave index
    ByIndex = 2,       // Sort by wave index
};

// ============================================================================
// WavetableIR layout — 0xC8 bytes
//
// Offset  Size  Type                                      Name
// ------  ----  ----                                      ----
// 0x00     8    const DeviceConstants*                    deviceConstants_
// 0x08     4    uint32_t                                  addressBase_
// 0x0C     4    uint32_t                                  firstWaveformOffset_
// 0x10    0x60  CachedParser                              cachedParser_
//               (size 0x60: 0x10 = tree root, inherits
//                boost::filesystem::path + map cache)
// 0x70     8    unique_ptr<WavetableManager<WaveformIR>>  manager_
// 0x78    0x28  WaveIndexTracker                          waveIndexTracker_
// 0xA0    0x18  vector<shared_ptr<WaveformIR>>            usedWaveforms_
// 0xB8    0x10  weak_ptr<CancelCallback>                  cancelCallback_
// 0xC8          END
//
// Note: the set<int> within WaveIndexTracker starts at absolute +0x80:
//   +0x78: WaveIndexTracker.maxIndex
//   +0x80: WaveIndexTracker.indices_ (__begin_node_)
//   +0x88: WaveIndexTracker.indices_ (__end_node_.__left_ = root)
//   +0x90: WaveIndexTracker.indices_ (size)
//   +0x98: WaveIndexTracker.autoIndex_
// ============================================================================
//! IR-stage wavetable: owns the `WaveformIR` collection for one AWG
//! core after front-end lowering, and drives placement of those
//! waveforms into the device's wave memory.
//!
//! Constructed either from a finished `WavetableFront` or directly
//! from a manager during JSON deserialisation. Beyond storing the
//! waveform list, it carries the address allocator state, the
//! used-waveform pruning set, and a weak reference to the
//! cancellation callback so long-running placement can be aborted.
//!
//! `allocateWaveforms()` and `allocateWaveformsForFifo()` are the
//! main placement entry points; helpers like `alignWaveformSizes()`
//! and `assignWaveIndexImplicit()` are run as part of those passes.
class WavetableIR {
public:
    const DeviceConstants* deviceConstants_;                             // +0x00
    uint32_t addressBase_;                                              // +0x08
    uint32_t firstWaveformOffset_;                                      // +0x0C
    CachedParser cachedParser_;                                         // +0x10 (0x60 bytes)
    std::unique_ptr<detail::WavetableManager<WaveformIR>> manager_;    // +0x70
    WaveIndexTracker waveIndexTracker_;                                  // +0x78 (0x28 bytes)
    std::vector<std::shared_ptr<WaveformIR>> usedWaveforms_;           // +0xA0 (0x18 bytes)
    std::weak_ptr<CancelCallback> cancelCallback_;                      // +0xB8 (0x10 bytes)

    // ---- Constructors ----

    // From WavetableFront — 0x29ce20
    /*! \brief Build the IR-stage wavetable from a finished
     *  front-end wavetable.
     *
     *  \details Snapshots the configuration that placement needs:
     *  device constants, the per-AWG base address, the sample
     *  cache (sized to `wavetableSize`, rooted at `path`), and a
     *  weak handle to the cancel callback so long allocation
     *  passes can be aborted.  Then converts every
     *  `WaveformFront` owned by `front`'s manager into a
     *  `WaveformIR` and inserts it into a freshly allocated
     *  `WavetableManager<WaveformIR>` whose `numDefs_` /
     *  `numDefs2_` counters are seeded from the front-end
     *  manager so synthetic-name minting (`getUniqueName`)
     *  continues from where the front left off.
     *
     *  \param front          Front-end wavetable to clone from.
     *  \param constants      Device-constants snapshot used for
     *                        every subsequent allocation pass.
     *  \param address        Per-core wave-memory base address.
     *  \param wavetableSize  CSV cache capacity, in entries.
     *  \param path           Project directory used to resolve
     *                        relative `placeholder` waveform paths.
     *  \param cancelCb       Weak handle polled by allocation
     *                        passes to honour user cancellations.
     */
    WavetableIR(const WavetableFront& front,
                const DeviceConstants& constants,
                detail::AddressImpl<uint32_t> address,
                size_t wavetableSize,
                const boost::filesystem::path& path,
                std::weak_ptr<CancelCallback> cancelCb);

    // From WavetableManager<WaveformIR> — 0x29d230
    /*! \brief Build the IR-stage wavetable directly from a
     *  pre-existing waveform manager.
     *
     *  \details Used by `fromJson()` to reconstitute a wavetable
     *  whose waveform list has already been deserialised.
     *  Copies `mgr` into the new wavetable, attaches the
     *  configuration arguments, and leaves `usedWaveforms_`
     *  empty — callers populate it via `setUsedWaveforms()`
     *  once the in-use subset is known.
     *
     *  \param mgr            Source manager.  Copied (not moved)
     *                        so the caller's reference remains
     *                        valid.
     *  \param constants      Device-constants snapshot.
     *  \param address        Per-core wave-memory base address.
     *  \param wavetableSize  CSV cache capacity, in entries.
     *  \param path           Project directory for placeholder
     *                        waveform resolution.
     *  \param cancelCb       Weak cancel-callback handle.
     */
    WavetableIR(const detail::WavetableManager<WaveformIR>& mgr,
                const DeviceConstants& constants,
                detail::AddressImpl<uint32_t> address,
                size_t wavetableSize,
                const boost::filesystem::path& path,
                std::weak_ptr<CancelCallback> cancelCb);

    // Destructor — 0x29d550
    //! Releases the manager, used-waveform list, wave-index
    //! tracker, sample cache, and cancel-callback handle in
    //! reverse declaration order.  No special teardown beyond
    //! the implicit member destructors.
    ~WavetableIR();

    // ---- Serialization ----

    // toJson — 0x29d670
    //! Serialise the wavetable for round-trip via `fromJson()`.
    //!
    //! Wraps the manager's JSON representation in a single-key
    //! object: `{ "wavetable_manager": <manager_->toJson()> }`.
    //! Address allocator state and the cancel-callback handle
    //! are intentionally not serialised — they are
    //! reconstructed from the deserialisation arguments.
    //!
    //! \return JSON object containing the manager payload.
    boost::json::value toJson() const;

    // fromJson (static, returns shared_ptr<WavetableIR>) — 0x29db10
    /*! \brief Deserialise a wavetable previously emitted by
     *  `toJson()`.
     *
     *  \details Reads the embedded `wavetable_manager` object,
     *  rebuilds a `WavetableManager<WaveformIR>` via its own
     *  `fromJson`, and constructs a fresh `WavetableIR` around
     *  it using the manager-taking constructor.
     *
     *  \param json           JSON object as produced by `toJson()`.
     *  \param constants      Device-constants snapshot for the
     *                        target core.
     *  \param address        Per-core wave-memory base address.
     *  \param wavetableSize  CSV cache capacity.
     *  \param path           Project directory for placeholder
     *                        waveform resolution.
     *  \param cancelCb       Weak cancel-callback handle.
     *  \return Shared owner of the reconstructed wavetable.
     *  \throws boost::system::system_error  When the JSON is
     *                        missing the `wavetable_manager`
     *                        key or fails the inner
     *                        manager-side validation.
     */
    static std::shared_ptr<WavetableIR> fromJson(
        boost::json::value json,
        const DeviceConstants& constants,
        detail::AddressImpl<uint32_t> address,
        size_t wavetableSize,
        const boost::filesystem::path& path,
        std::weak_ptr<CancelCallback> cancelCb);

    // ---- Comparison ----

    // operator== — 0x29e090
    //! Structural equality for cache-key comparisons.
    //!
    //! Two wavetables compare equal iff their managers compare
    //! equal (same waveform list, same name index) **and** they
    //! share the same `addressBase_` and `firstWaveformOffset_`.
    //! The `cachedParser_`, `waveIndexTracker_`, used-waveform
    //! list, and cancel callback do not participate.
    //!
    //! \param other  Wavetable to compare against.
    //! \return True when both wavetables would place identical
    //!         payloads at identical addresses.
    bool operator==(const WavetableIR& other) const;

    // ---- Iterators / accessors ----

    // begin — 0x29e270
    //! Pointer to the first waveform in the underlying manager's
    //! waveform list.  Together with `end()` this lets the IR be
    //! used directly with range-based `for` loops, but iteration
    //! sees *every* waveform the manager owns — not only those in
    //! `usedWaveforms_`.  Use `forEachUsedWaveform()` when only
    //! the in-use subset is wanted.
    //!
    //! \return Pointer to the first element of the manager's
    //!         waveform vector.
    const std::shared_ptr<WaveformIR>* begin() const;

    // end — 0x29e280
    //! One-past-the-end pointer of the underlying manager's
    //! waveform list.  Pairs with `begin()` for whole-list
    //! traversal.
    //!
    //! \return Sentinel pointer one past the last waveform.
    const std::shared_ptr<WaveformIR>* end() const;

    // size — 0x29e290
    //! Number of waveforms tracked by the manager.  Counts every
    //! waveform owned by the manager, including filler entries
    //! synthesised by `assignWaveIndexImplicit()` and waveforms
    //! that were never marked used.
    //!
    //! \return Element count of the manager's waveform vector.
    size_t size() const;

    // getWaveformByName — 0x29e2b0
    //! Look up a waveform by source-language name.
    //!
    //! Resolves the name through the manager's `nameToIndex_`
    //! map and returns the matching `WaveformIR`.  Used by the
    //! front-end lowering when a `playWave`-style call references
    //! a previously declared waveform identifier.
    //!
    //! \param name  Optional waveform name.  An empty optional or
    //!              an unknown name both return a null
    //!              `shared_ptr` rather than throwing.
    //! \return The matching waveform, or null if `name` is empty
    //!         or not registered with the manager.
    std::shared_ptr<WaveformIR> getWaveformByName(
        const std::optional<std::string>& name) const;

    // getNextSegmentAddress — 0x29e320
    //! Current allocation high-water mark.
    //!
    //! Returns the device-memory address that the next waveform
    //! placed by the allocator will receive.  Equal to the
    //! configured base address before any allocation runs, and
    //! advanced by `allocateWaveforms()` /
    //! `allocateWaveformsForFifo()` past every placed waveform.
    //!
    //! \return Next free device-memory address (in bytes).
    uint32_t getNextSegmentAddress() const;

    // getFirstWaveformOffset — 0x29e330
    //! Offset of the first waveform's payload relative to the
    //! wave-memory base.
    //!
    //! In cache-managed mode this leaves room for the per-wave
    //! header table that the runtime expects ahead of the
    //! waveform payloads; in FIFO mode the offset is zero.  Set
    //! by `allocateWaveforms()` when it computes the alignment
    //! pre-roll.
    //!
    //! \return Offset (in bytes) from the wave-memory base to
    //!         the first waveform payload.
    uint32_t getFirstWaveformOffset() const;

    // ---- Allocation methods ----

    // allocateWaveforms — 0x29e340
    /*! \brief Place every used waveform in the cache-managed
     *  region of wave memory.
     *
     *  \details Two-phase allocator used when the working set
     *  fits in cache:
     *    1. **Sizing pass** — for every used waveform, ensures
     *       the sample buffer is loaded (`loadWaveform`),
     *       computes the aligned byte size, and lays the
     *       waveforms out tail-to-tail starting at
     *       `addressBase_`, inserting `waveformAlignment`
     *       padding only when a waveform would otherwise span
     *       a cache-line boundary or when the previous
     *       waveform was already an oversized one.
     *    2. **Cache-line marking pass** — folds every
     *       waveform's address into the cache-line occupancy
     *       vector and sets `WaveformIR::crossesCacheLine_`
     *       on the entries that consume more than one line
     *       (the prefetcher uses that bit to schedule a `prf`
     *       ahead of the play).
     *
     *  Iteration order in phase 1 is `WaveOrder::Natural` for
     *  FIFO mode (preserves the registration order used to
     *  stream waveforms in) and `WaveOrder::ByWaveIndex`
     *  otherwise (compacts low-index waveforms toward the base
     *  so wave-index-driven addressing remains tight).
     *
     *  Throws via `WavetableException` when a waveform that
     *  was registered but never marked used is encountered.
     *
     *  \param fifoMode  Selects the iteration order described
     *                   above.  Always paired with
     *                   `allocateWaveformsForFifo()` from
     *                   `updateWaveforms`; this method only
     *                   handles the cache-managed strategy.
     *  \throws WavetableException  When an unused-but-
     *                   registered waveform reaches the
     *                   sizing pass.
     */
    void allocateWaveforms(bool fifoMode);

    // forEachUsedWaveform — 0x29e5e0
    /*! \brief Apply `callback` to every used waveform in a
     *  configurable order.
     *
     *  \details Builds an index permutation over
     *  `usedWaveforms_`, sorts it according to `order`
     *  (`Natural` skips sorting; `ByIndex` sorts by
     *  `WaveformIR::addressValue` ascending; `ByWaveIndex`
     *  sorts by `WaveformIR::waveIndex` ascending), then
     *  invokes `callback` on each waveform in the sorted
     *  order.  This indirection lets the caller iterate the
     *  same underlying list in whichever order the current
     *  pass needs without disturbing the storage order.
     *
     *  \param callback  Functor applied to each used
     *                   waveform.  Exceptions thrown from the
     *                   callback propagate to the caller.
     *  \param order     Iteration order — see `WaveOrder`.
     */
    void forEachUsedWaveform(
        std::function<void(const std::shared_ptr<WaveformIR>&)> callback,
        WaveOrder order) const;

    // assignWaveIndexImplicit — 0x29e8a0
    /*! \brief Assign wave indices to waveforms that did not
     *  receive an explicit one, then synthesise filler
     *  waveforms to plug any remaining gaps.
     *
     *  \details Two phases:
     *    1. **Implicit assignment** — every used waveform
     *       whose `waveIndex` is still `-1` is given the next
     *       free index from `waveIndexTracker_`, advancing
     *       past any explicitly-assigned indices.
     *    2. **Gap filling** — once the natural waveforms have
     *       been assigned, walks the still-unassigned indices
     *       below the highest-used one and synthesises a
     *       reserve-only "filler" waveform per gap (named via
     *       `getUniqueName("filler", numDefs_, numDefs2_++)`).
     *       Fillers are appended to `usedWaveforms_`, marked
     *       `used_=true` and `crossesCacheLine_=true`, and
     *       receive the gap's index.
     *
     *  Filler waveforms become `SHT_NOBITS`-style entries in
     *  the ELF (no sample data emitted) but reserve the
     *  index slot so wave-index addressing stays dense.
     */
    void assignWaveIndexImplicit();

    // setUsedWaveforms — 0x29ece0
    //! Replace the in-use waveform subset.
    //!
    //! Copies `waveforms` into `usedWaveforms_`, replacing the
    //! previous contents.  Self-assignment is a no-op.  The
    //! address allocator and wave-index tracker are *not*
    //! reset — callers that swap in a new used set are
    //! expected to re-run the relevant placement passes.
    //!
    //! \param waveforms  New in-use list.
    void setUsedWaveforms(const std::vector<std::shared_ptr<WaveformIR>>& waveforms);

    // updateWaveforms — 0x29ed10
    /*! \brief Allocate device memory addresses for every used waveform,
     *  selecting between the cache-managed and FIFO allocation
     *  strategies.
     *
     *  \details A two-line dispatcher: when `fifoMode` is true,
     *  delegates to `allocateWaveformsForFifo()` (the streaming /
     *  Hirzel-style strategy where the prefetcher reuses cache lines
     *  via `MemoryAllocator::allocateReloadingCL`); otherwise
     *  delegates to `allocateWaveforms(allocFifoMode)` (the
     *  fits-in-cache strategy where every used waveform gets a
     *  unique tail-region address).  Both strategies write the
     *  resulting per-waveform `addressValue` and
     *  `crossesCacheLine_` fields on each `WaveformIR` and update
     *  the IR's `addressBase_` to the end of the last allocation.
     *
     *  Called from `Compiler::runPrefetcher` after
     *  `assignWaveformAllocationSizes` has populated each
     *  waveform's `allocationByteSize`, and before
     *  `Prefetch::placeLoads` consumes those addresses to plan
     *  load placement.
     *
     *  \param fifoMode        Selects FIFO vs cache strategy.
     *                         True for streaming compiles where
     *                         the working set exceeds device cache.
     *  \param allocFifoMode   Forwarded to `allocateWaveforms`
     *                         when `fifoMode` is false; ignored
     *                         otherwise.
     *  \throws WavetableException  Wrapped from the inner
     *                         allocator when waveform memory
     *                         overflows.
     */
    void updateWaveforms(bool fifoMode, bool allocFifoMode);

    // allocateWaveformsForFifo — 0x29ed30
    /*! \brief Place every used waveform in the FIFO / streaming
     *  region of wave memory.
     *
     *  \details Two-pass allocator that reuses cache lines:
     *    1. **Fixed pass** — every waveform with the
     *       `WaveformIR::fixed_` flag set is allocated via
     *       `MemoryAllocator::allocateCLAligned` first,
     *       grabbing a stable address it will keep for the
     *       lifetime of the sequence.  Each allocated cache
     *       line is recorded in a "do not reload" set.
     *    2. **Reloading pass** — the remaining waveforms try
     *       the cache-aligned allocator first, and on
     *       failure fall back to
     *       `MemoryAllocator::allocateReloadingCL` which
     *       evicts cache lines outside the do-not-reload set.
     *       Successfully reloaded waveforms have
     *       `crossesCacheLine_` cleared because the runtime
     *       guarantees a fresh load before each play.
     *
     *  After both passes the high-water mark of the underlying
     *  allocator is written back to `addressBase_` so callers
     *  can read the next-free address through
     *  `getNextSegmentAddress()`.
     *
     *  \throws WavetableException  When either pass cannot
     *                       satisfy a request — wraps the
     *                       inner allocator's failure with a
     *                       "Waveform memory overflow"
     *                       message.
     */
    void allocateWaveformsForFifo();

    // alignWaveformSizes — 0x29f150
    /*! \brief Round every used waveform's sample count up to
     *  device-grain alignment.
     *
     *  \details Walks `usedWaveforms_` in `Natural` order and
     *  rounds each waveform's sample count up to a multiple
     *  of `DeviceConstants::grainSize`, then clamps the
     *  result to at least `WaveformIR::minLengthSamples`
     *  (the per-waveform minimum imposed by the device's DMA
     *  burst size).  Zero-length signals are left untouched
     *  so that `zeros(0)` and degenerate `cut(w, N, N)`
     *  collapses do not get inflated to a full grain.
     *
     *  Resizes the signal in place via
     *  `Signal::resizeSamples` only when the aligned count
     *  differs from the current count, so the common case
     *  (already aligned) avoids a sample-buffer reallocation.
     */
    void alignWaveformSizes();

    // assignWaveformAllocationSizes — 0x29f1d0
    /*! \brief Compute and store the per-waveform device-memory
     *  footprint (`WaveformIR::allocationByteSize`) for every used
     *  waveform.
     *
     *  \details Walks `forEachUsedWaveform(WaveOrder::Natural)` and
     *  for each entry:
     *    1. Polls the cancellation callback and bails on cancel.
     *    2. Calls `loadWaveform` when the waveform's sample buffer
     *       is empty (placeholder waveforms are realised lazily so
     *       their sample count is known here).
     *    3. Computes the aligned sample count: zero-length signals
     *       map to zero (so `zeros(0)` and degenerate `cut(w,N,N)`
     *       contribute nothing to the memory budget), otherwise
     *       the sample count is rounded up to
     *       `DeviceConstants::grainSize` and then *raised* to
     *       `DeviceConstants::maxWaveformLength` when that is
     *       larger — every non-empty waveform consumes at least
     *       one full waveform block.
     *    4. Multiplies by `signal.channels()` and
     *       `DeviceConstants::bitsPerSample`, converts to bytes
     *       (rounding up), and aligns the result to a 64-byte
     *       boundary before storing it as `allocationByteSize`.
     *
     *  Called from `Compiler::runPrefetcher` immediately before
     *  `updateWaveforms`, since the allocator needs the byte size
     *  of every waveform before it can place them.
     */
    void assignWaveformAllocationSizes();

    // loadWaveform — 0x29f310
    /*! \brief Materialise a placeholder waveform's sample data
     *  from its source CSV.
     *
     *  \details Lazy-load helper used by the allocation
     *  passes when they encounter a waveform whose sample
     *  buffer is still empty.  Skips the load when:
     *    - the waveform already has its `waveformType` set
     *      (in-memory waveforms registered via `playWave`
     *      from a SeqC array), or
     *    - the sample buffer is already populated.
     *
     *  Otherwise routes through `CsvParser::csvFileToWaveform`
     *  using `cachedParser_` so repeated references to the
     *  same CSV path within a compile share parser state.
     *  Exceptions raised by the parser propagate
     *  unwrapped — callers wrap them in
     *  `WavetableException` if they want a wavetable-level
     *  diagnostic.
     *
     *  \param waveform  Waveform to (re)hydrate.  The
     *                   shared_ptr is held by value so the
     *                   parser's downstream consumers can
     *                   keep their own owners.
     */
    void loadWaveform(std::shared_ptr<WaveformIR> waveform);

    // getJsonIndex — 0x29f480
    //! Serialise the used-waveform list as the `.waveforms` ELF
    //! section payload.
    //!
    //! Emits a JSON document of the form
    //! `{ "waveforms": [ <entry>, <entry>, ... ] }` where each
    //! entry is `WaveformIR::toJsonElement(format)` — the
    //! per-waveform metadata (name, length, channel count,
    //! address, sample-format-specific scaling) consumed by the
    //! runtime to map source-level waveform identifiers to
    //! placed device memory.  Entries are emitted in
    //! `WaveOrder::ByWaveIndex` order, and waveforms that are
    //! unused, have zero allocation, or have a zero raw
    //! sample length (`zeros(0)` and `cut(w, N, N)` collapses)
    //! are skipped.
    //!
    //! \param format  Sample-format selector forwarded to each
    //!                waveform's `toJsonElement` so the embedded
    //!                scaling matches the device's DAC width.
    //! \return Single-line JSON string ready to be embedded as
    //!         the `.waveforms` section content.
    std::string getJsonIndex(SampleFormat format) const;
};

} // namespace zhinst
