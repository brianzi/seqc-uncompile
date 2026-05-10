// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Cache class — waveform cache memory management
//
// Confirmed from:
//   - Cache::Cache()               0x282920
//   - Cache::getSize()             0x282940
//   - Cache::getPageSize()         0x282950
//   - Cache::getFreeMemory()       0x282960
//   - Cache::getScope()            0x2829a0
//   - Cache::allocate(5-arg)       0x282a30
//   - Cache::allocate(CacheType)   0x282be0
//   - Cache::getBestPosition()     0x282cf0
//   - Cache::memoryWrite()         0x283020
//   - Cache::stillInMemory()       0x2832e0
//   - Cache::reuse()               0x2833d0
//   - Cache::play()                0x2834c0
//   - Cache::resetPlay()           0x283640
//   - Cache::free()                0x283690
//   - Cache::print()               0x283b50
//   - Cache::Pointer::str()        0x283c30
//   - CacheException               0x2835a0
//   - CacheException::what()       0x284200
// ============================================================================
#pragma once

#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "zhinst/asm/address_impl.hpp"

namespace zhinst {

struct WaveformIR;
struct DeviceConstants;

// ============================================================================
// CacheException — inherits std::exception, has string message_
//
// Offset  Size  Type         Name      Notes
// +0x00   8     vptr
// +0x08   24    std::string  message_
// sizeof(CacheException) = 0x20
// ============================================================================
//! \brief Diagnostic thrown by `Cache` when an allocation cannot be
//! satisfied within the device's wave-cache region.
//!
//! Carries a free-form `message_` payload (returned verbatim by
//! `what()`).  Raised from `Cache::allocate` overloads when neither the
//! reuse path nor `getBestPosition` finds a suitable hole.
class CacheException : public std::exception {
public:
    //! \brief Construct with the diagnostic text returned later by
    //! `what()`.
    //! \param msg Human-readable error message; copied into the
    //!        exception payload.
    explicit CacheException(std::string const& msg);  // 0x2835a0

    //! \brief Destroy the exception and free the stored message.
    ~CacheException() override;                       // 0x283600

    //! \brief Return the stored diagnostic text.
    //! \details Returns a pointer to an empty string when no message
    //!          was supplied at construction.
    //! \return Null-terminated C string owned by the exception; valid
    //!         for the lifetime of `*this`.
    const char* what() const noexcept override;       // 0x284200

private:
    std::string message_;  //!< Diagnostic text returned by `what()`. +0x08
};

// ============================================================================
// Cache class
//
// Offset  Size  Type                          Name       Notes
// +0x00   4     uint32_t                      size_      total cache size
// +0x04   4     int32_t                       pageSize_  page/alignment size
// +0x08   1     bool                          isHirzel_  if true, always append at end
// +0x09   7     (padding)
// +0x10   24    vector<shared_ptr<Pointer>>   pointers_
// sizeof(Cache) = 0x28
// ============================================================================
//! \brief Wave-cache occupancy tracker for a single AWG sequencer
//! during prefetch planning.
//!
//! Owned by `Prefetch` and constructed once per compilation from the
//! device's wave-cache size and page (cache-line) size; on Hirzel
//! devices the `isHirzel_` flag forces a strict append-at-end
//! allocation policy.  Each resident waveform slot is recorded as a
//! `Pointer` (position, size, hash, repeat count, owning
//! `WaveformIR`, lifecycle `PointerState`); `pointers_` holds them in
//! allocation order.
//!
//! The two `allocate` overloads find or create a slot — the
//! 5-argument form supports the general placement search (with a
//! `nameMap` of currently-resident waveforms, a `maxBranches`
//! limit, and a `split` toggle), the 2-argument `CacheType` form
//! handles the simpler aligned/normal placement.  `getBestPosition`
//! is the underlying placement search; `memoryWrite` /
//! `stillInMemory` / `reuse` track residency across loads;
//! `play` / `resetPlay` / `free` advance the per-slot
//! `PointerState` (Ready → Playing → LastPlayed → Free) as the
//! AST walk emits play instructions.  `getScope` returns a fresh
//! sub-scope `Cache` mirroring the parent's geometry, used for
//! branch-local placement tries.
class Cache {
public:
    // PointerState enum (int32_t)
    //   0 = Ready      ("ready")
    //   1 = LastPlayed ("last played")
    //   2 = Playing    ("playing")
    //   3 = Free       ("free")
    //! \brief Lifecycle state of a cached waveform slot, advanced by
    //! `play()` / `resetPlay()` / `free()` as the AST walk emits the
    //! sequencer instruction stream.
    enum class PointerState : int {
        Ready      = 0,   //!< Resident and available for the next play.
        LastPlayed = 1,   //!< Played in the previous play instruction; will be promoted to `Free` on the next `play()`.
        Playing    = 2,   //!< Currently playing; will be promoted to `Ready` on the next `play()`.
        Free       = 3,   //!< Slot is unused and the bytes may be reused.
    };

    //! \brief Alignment policy for `allocate(CacheType)`.
    //! \details `Aligned` doubles the effective page size for the
    //!          alignment computation, used when a waveform must
    //!          straddle two cache lines.
    enum class CacheType : int {
        Normal  = 0,   //!< Round size up to a single page (`pageSize_`).
        Aligned = 1,   //!< Round size up to a double page (`2 * pageSize_`).
    };

    // Offset  Size  Type                   Name         Notes
    // +0x00   4     uint32_t               position_    start address
    // +0x04   4     uint32_t               size_        bytes allocated
    // +0x08   4     uint32_t               hash_        ~(pos ^ (pos + size/2))
    // +0x0C   4     uint32_t               numRepeats_  numBytes/pageSize + 1
    // +0x10   16    shared_ptr<WaveformIR> waveform_
    // +0x20   4     PointerState           state_
    // sizeof(Cache::Pointer) = 0x24 (libc++); 0x28 (libstdc++)
    //! \brief One slot in the wave cache: a contiguous byte range
    //! holding the samples of a single `WaveformIR` together with
    //! its lifecycle state.
    //!
    //! \details `position_` and `size_` describe the slot's location
    //! inside the device wave-cache region; `hash_` and `numRepeats_`
    //! are populated by the splitting heuristic in
    //! `Cache::allocate(...,maxBranches,split)` to drive the
    //! sequencer's address-wrapping for waveforms larger than the
    //! cache. `state_` is advanced by `Cache::play` / `resetPlay` /
    //! `free` as the prefetch walk emits play instructions.
    struct Pointer {
        uint32_t position_;                     //!< Start offset in the wave cache, in bytes. +0x00
        uint32_t size_;                         //!< Allocated length in bytes (page-aligned). +0x04
        uint32_t hash_;                         //!< Wrap-mask used by split waveforms; `~(position_ ^ (position_ + size_/2))`. Zero for non-split allocations. +0x08
        uint32_t numRepeats_;                   //!< Number of cache-sized chunks the sequencer must stream for split waveforms; `1` for non-split allocations. +0x0C
        std::shared_ptr<WaveformIR> waveform_;  //!< Source waveform whose samples occupy this slot. +0x10
        PointerState state_;                    //!< Current lifecycle state (Ready / Playing / LastPlayed / Free). +0x20

        // No additional fields: Pointer is exactly 0x24 bytes.

        //! \brief Render the slot as `"<position> - <end> -- <state>"`
        //! for `Cache::print` / debugging.
        //! \return Human-readable single-line description; `"INVALID
        //!         STATE"` is substituted when `state_` is out of range.
        std::string str() const;                // 0x283c30
    };
    // Note: sizeof(Pointer) == 0x24 in libc++ ABI, 0x28 in libstdc++ (extra tail padding).

    // Constructor
    //! \brief Construct an empty cache for a single AWG sequencer.
    //! \param size     Total wave-cache region size, in bytes.
    //! \param pageSize Cache-line / alignment granularity in bytes;
    //!                 every allocation is rounded up to a multiple
    //!                 of this value.
    //! \param isHirzel If `true`, every allocation is forced to
    //!                 position `0` with the requested size — the
    //!                 placement search is skipped. Used for devices
    //!                 whose sequencer expects a strict append-at-end
    //!                 layout.
    Cache(detail::AddressImpl<uint32_t> size, int pageSize, bool isHirzel);  // 0x282920

    //! \brief Total cache region size, in bytes, as configured at
    //! construction.
    //! \return Configured cache size.
    detail::AddressImpl<uint32_t> getSize() const;       // 0x282940

    //! \brief Cache-line alignment in bytes used to round allocation
    //! sizes.
    //! \return Configured page size.
    int getPageSize() const;                             // 0x282950

    //! \brief Bytes not currently held by a non-`Free` slot.
    //! \details Iterates `pointers_`, subtracting the size of every
    //! non-`Free` slot from the configured cache size; clamps to `0`
    //! if the running subtraction underflows.
    //! \return Free byte count.
    uint32_t getFreeMemory() const;                      // 0x282960

    //! \brief Snapshot the cache for branch-local placement trials.
    //! \details Returns a fresh `Cache` that mirrors the parent's
    //! geometry (size, page size, Hirzel flag) and starts with a copy
    //! of the parent's `pointers_` vector. Mutations on the returned
    //! cache do not affect the parent, allowing the prefetch planner
    //! to try alternative placements per branch and discard them.
    //! \return Newly allocated detached `Cache`.
    std::shared_ptr<Cache> getScope() const;             // 0x2829a0

    //! \brief Reserve a slot for a waveform, splitting it across
    //! cache-sized chunks if it does not fit in free memory.
    //! \details If `split` is `true`, or if the waveform fits in the
    //! per-branch free budget (`getFreeMemory() / maxBranches`), this
    //! delegates to the simple `CacheType::Normal` overload. Otherwise
    //! it computes a chunk size via the splitting heuristic, allocates
    //! the first chunk as `CacheType::Aligned`, and back-fills the
    //! returned pointer's `hash_` and `numRepeats_` so the sequencer
    //! can stream the remaining chunks via address wrapping.
    //! \param waveform    Waveform whose samples will occupy the slot.
    //! \param numBytes    Total byte size of the waveform's sample data.
    //! \param nameMap     Names of waveforms expected to be evicted in
    //!                    this round; entries mapped to `true` are
    //!                    treated as already-free during placement.
    //! \param maxBranches Number of concurrently-active branches the
    //!                    cache must accommodate; used as the divisor
    //!                    of free memory.
    //! \param split       Force the un-split (Normal) path even when
    //!                    free space is tight.
    //! \return Pointer to the newly inserted slot.
    //! \throws CacheException if no fitting position can be found.
    std::shared_ptr<Pointer> allocate(
        std::shared_ptr<WaveformIR> waveform,
        detail::AddressImpl<uint32_t> numBytes,
        std::unordered_map<std::string, bool> const& nameMap,
        int maxBranches,
        bool split);                                     // 0x282a30

    //! \brief Reserve a page-aligned slot for a waveform without
    //! splitting.
    //! \details Rounds `numBytes` up to a multiple of `pageSize_`
    //! (or `2 * pageSize_` for `CacheType::Aligned`), runs
    //! `getBestPosition` to choose a location, marks the resulting
    //! slot `Ready`, attaches `waveform`, and inserts it into
    //! `pointers_` via `memoryWrite`.
    //! \param waveform   Waveform whose samples will occupy the slot.
    //! \param numBytes   Raw byte size before alignment.
    //! \param nameMap    Names of waveforms expected to be evicted in
    //!                   this round; entries mapped to `true` are
    //!                   treated as already-free during placement.
    //! \param cacheType  `Normal` for single-page alignment;
    //!                   `Aligned` for double-page alignment.
    //! \return Pointer to the newly inserted slot.
    //! \throws CacheException if no fitting position can be found.
    std::shared_ptr<Pointer> allocate(
        std::shared_ptr<WaveformIR> waveform,
        detail::AddressImpl<uint32_t> numBytes,
        std::unordered_map<std::string, bool> const& nameMap,
        CacheType cacheType);                            // 0x282be0

    //! \brief Find the best position for an `numBytes`-byte slot.
    //! \details On Hirzel devices the slot is always placed at
    //! position `0`. Otherwise the search first tries appending after
    //! the last allocation; if that does not fit, it gap-scans
    //! `pointers_` for the smallest sufficient gap (taking an exact
    //! match immediately). Slots in `PointerState::Free` and slots
    //! whose waveform name appears with value `true` in `nameMap` are
    //! treated as available space.
    //! \param numBytes  Aligned byte size requested.
    //! \param nameMap   Names of waveforms slated for eviction; `true`
    //!                  entries are skipped during the gap calculation.
    //! \param gapScan   `false` triggers the fast append-at-end path
    //!                  (which recurses with `true` on failure);
    //!                  `true` performs the gap scan directly.
    //! \return Freshly constructed `Pointer` with `position_` and
    //!         `size_` set; `state_` is `Free`, `waveform_` is null.
    //! \throws CacheException if no fitting position exists.
    std::shared_ptr<Pointer> getBestPosition(
        detail::AddressImpl<uint32_t> numBytes,
        std::unordered_map<std::string, bool> const& nameMap,
        bool gapScan);                                // 0x282cf0

    //! \brief Insert `ptr` into `pointers_`, evicting any existing
    //! entries whose byte range overlaps `ptr`'s.
    //! \details Walks `pointers_` once to erase every overlapping
    //! entry, then inserts `ptr` at the position keyed by
    //! `position_` so the vector stays sorted by start offset.
    //! \param ptr Slot to commit; ownership is shared.
    void memoryWrite(std::shared_ptr<Pointer> ptr);      // 0x283020

    //! \brief Test whether a previously allocated slot is still
    //! resident at the same position with the same waveform.
    //! \param ptr Slot to look up; `nullptr` returns `false`.
    //! \return `true` iff `pointers_` contains an entry with matching
    //!         `position_`, `size_`, and waveform name.
    bool stillInMemory(std::shared_ptr<Pointer> ptr) const;  // 0x2832e0

    //! \brief Mark the slot matching `ptr` as `Ready` so a previous
    //! `play()` does not consume it on the next state transition.
    //! \details Matches by `position_`, `size_`, and waveform name;
    //! silently does nothing if no matching slot exists.
    //! \param ptr Slot to revive.
    void reuse(std::shared_ptr<Pointer> ptr);            // 0x2833d0

    //! \brief Advance the play state machine for one play instruction.
    //! \details First demotes the slot in `Playing` (→ `Ready`) or
    //! `LastPlayed` (→ `Free`) — whichever is found first. Then locates
    //! the slot matching `ptr` by `position_` and `size_` and sets it
    //! to `LastPlayed` if `playMode == Free`, otherwise `Playing`.
    //! \param ptr      Slot about to be played.
    //! \param playMode `Free` for a final play (slot will be freed on
    //!                 the next call); any other value for a normal
    //!                 play.
    //! \throws CacheException if `ptr` is null.
    void play(std::shared_ptr<Pointer> ptr, PointerState playMode);  // 0x2834c0

    //! \brief Undo the most recent `play()` state advance.
    //! \details Demotes the first slot found in `LastPlayed` to
    //! `Free`, or the first slot in `Playing` to `Ready`; stops after
    //! one transition. Used when a tentatively-emitted play is
    //! rolled back during planning.
    void resetPlay();                                    // 0x283640

    //! \brief Remove a slot from the cache.
    //! \param ptr Slot to release; matched by `position_` and `size_`.
    //! \throws CacheException if `ptr` is null, or if no matching
    //!         slot exists in `pointers_`.
    void free(std::shared_ptr<Pointer> ptr);             // 0x283690

    //! \brief Dump every slot to `std::cout`, one line per entry,
    //! using `Pointer::str()`.
    void print() const;                                  // 0x283b50

private:
    uint32_t size_;                                      //!< Total cache region size in bytes. +0x00
    int32_t pageSize_;                                   //!< Page-alignment granularity in bytes. +0x04
    bool isHirzel_;                                      //!< If `true`, allocations are forced to position `0` (append-at-end policy). +0x08
    std::vector<std::shared_ptr<Pointer>> pointers_;     //!< Resident slots, kept sorted by `position_`. +0x10
};

// Constants
// unusedCacheLine at 0x95deac = 0xFFFFFFFF (sentinel value for unallocated)
//! \brief Sentinel value for an unallocated cache line slot.
static constexpr uint32_t unusedCacheLine = 0xFFFFFFFF;

// kCacheFormat at 0x95e8f8 = "3" (SSO string encoding: 0x02, '3', 0x00)
// (This appears to be a static std::string, likely defined elsewhere)

} // namespace zhinst
