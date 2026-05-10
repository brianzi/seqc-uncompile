// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveIndexTracker — tracks assigned wave indices using a std::set<int>
//
// Confirmed from:
//   - WaveIndexTracker(int) at 0x29a5e0
//   - assign(int) at 0x29a600
//   - assignAuto(int) at 0x29a620
//   - usedWaveIndices() const at 0x29a7d0
//   - getNextAutoIndex() at 0x29a880
//   - hasGaps() at 0x29a8e0
//   - Template ctor<WaveformFront> at 0x29d000
//   - Template ctor<WaveformIR> at 0x29d410
// ============================================================================
#pragma once

#include <cstdint>
#include <exception>
#include <set>
#include <string>

namespace zhinst {

namespace detail {
    template<typename T> class WavetableManager;
}

// ============================================================================
// WaveIndexTracker layout — 0x28 bytes (40 bytes)
//
// Uses libc++ std::set<int> which is a __tree with:
//   __begin_node_ (ptr), __pair1_ (end-node with left/parent/right/size)
//
// Offset  Size  Type              Name            Notes
// ------  ----  ----              ----            -----
// 0x00     4    int               maxIndex        maximum valid index
// 0x04     4    (padding)
// 0x08     8    __tree_node*      __begin_node_   } std::set<int> internal:
// 0x10     8    __tree_node*      left            }   __end_node_.__left_
// 0x18     8    __tree_node*      parent (unused) }   (zeroed)
// 0x20     4    int               autoIndex       next auto-assigned index
// 0x24     4    (padding to 0x28)
//
// Actually the set is stored as:
// 0x08    24    std::set<int>     usedIndices     (tree internals)
// 0x20     4    int               autoIndex
// 0x28          END
//
// Re-examining: libc++ __tree layout for set<int>:
//   +0x00: __end_node_.__left_ (root ptr)      -- this is at WIT+0x08 (= __begin_node_)
// Wait, let's look at the ctor more carefully:
//
// ctor stores:
//   [rdi+0x00] = maxIndex (esi)
//   [rdi+0x08] = rdi+0x10   (__begin_node_ points to end-sentinel)
//   [rdi+0x10] = 0          (end-sentinel left child = null = root)
//   [rdi+0x18] = 0          (end-sentinel right? or size pair)
//   [rdi+0x20] = 0          (autoIndex)
//
// libc++ set<int, less<int>, allocator<int>>:
//   __tree has:
//     __compressed_pair<__end_node_t, __node_allocator> __pair1_;
//       __end_node_t has __left_ (root pointer)
//     __compressed_pair<size_type, value_compare> __pair3_;
//       first = size
//     __begin_node_ = pointer to leftmost node (or &__end_node_ if empty)
//
// Layout (libc++ set internals within WaveIndexTracker):
//   +0x08: __begin_node_ (ptr)    -- points to &[+0x10] when empty
//   +0x10: __end_node_.__left_    -- root pointer (null when empty)
//   +0x18: size                   -- 0 initially
//
// So the real layout is:
// Offset  Size  Type              Name
// 0x00     4    int               maxIndex
// 0x04     4    (padding)
// 0x08    24    std::set<int>     indices   (begin_node=8, end_node left=8, size=8)
// 0x20     4    int               autoIndex
// 0x24     4    (padding)
// 0x28          END (total 40 bytes)
// ============================================================================
//! \brief Tracks which wave indices have already been assigned
//!        within one wavetable, and hands out the next available
//!        index on demand.
//!
//! `assign()` reserves a specific index (raising
//! `WavetableException` on a duplicate or out-of-range value);
//! `assignAuto()` reserves the lowest unused index. `hasGaps()`
//! lets callers detect a non-contiguous index space, which downstream
//! emitters need to know about.
class WaveIndexTracker {
public:
    //! \brief Highest valid index + 1; assignments at or above this
    //!        value throw `WavetableException`.
    int maxIndex;                   // +0x00
    // +0x04: padding
    //! \brief Set of indices currently reserved by `assign` /
    //!        `assignAuto`.
    std::set<int> indices_;         // +0x08 (internally 24 bytes in libc++)
    //! \brief Next auto-assigned index returned by `assignAuto`;
    //!        advanced past every used index by `getNextAutoIndex`.
    int autoIndex_;                 // +0x20

    //! \brief Construct an empty tracker with the given upper bound.
    //! \param maxIndex  Highest assignable index + 1; passed
    //! through from the device-specific `maxWavetableEntries`.
    explicit WaveIndexTracker(int maxIndex);                            // 0x29a5e0

    //! \brief Construct a tracker pre-populated from every waveform
    //!        already registered in `mgr`.
    //! \details Iterates `mgr.waveforms_` and inserts each
    //! waveform's stored `waveIndex` into `indices_` (skipping
    //! the `-1` sentinel used for "no index yet").  After
    //! construction `autoIndex_` is advanced to the first unused
    //! slot via the usual `getNextAutoIndex` walk.
    //! \param maxIndex  Highest assignable index + 1.
    //! \param mgr       Source manager whose waveforms supply the
    //!                  initial reservations.
    template<typename T>
    WaveIndexTracker(int maxIndex, const detail::WavetableManager<T>& mgr);  // 0x29d000 / 0x29d410

    //! \brief Reserve a specific index after rewinding the
    //!        auto-index cursor.
    //! \details Sets `autoIndex_ = 0` and then delegates to
    //! `assignAuto(index)` so the same uniqueness / range checks
    //! apply.  Callers that pre-rewound need not call this.
    //! \param index  Index to reserve.
    void assign(int index);                                             // 0x29a600

    //! \brief Reserve `index` and return it after validation.
    //! \details Throws `WavetableException` when `index >=
    //! maxIndex` or when `index` is already present in
    //! `indices_`; otherwise inserts it.
    //! \param index  Index to reserve.
    //! \return The same `index` that was reserved.
    //! \throws WavetableException  On duplicate or out-of-range.
    int assignAuto(int index);                                          // 0x29a620

    //! \brief Read-only view of every index currently reserved.
    //! \return Reference to the internal `std::set<int>`; valid for
    //! the lifetime of `*this`.
    const std::set<int>& usedWaveIndices() const;                       // 0x29a7d0

    //! \brief Advance `autoIndex_` to the lowest unused slot.
    //! \details Iterates from the current `autoIndex_` upward
    //! while `indices_` contains it.  Used internally before each
    //! auto-assignment to skip over manually reserved indices.
    void getNextAutoIndex();                                            // 0x29a880

    //! \brief Report whether the reserved indices skip any values
    //!        below the current maximum.
    //! \return `true` when `autoIndex_` precedes the largest
    //! reserved index (i.e. there is at least one hole), `false`
    //! when the reservations are contiguous from 0.
    bool hasGaps();                                                     // 0x29a8e0
};

// ============================================================================
// WavetableException — thrown by WaveIndexTracker on index conflicts
//
// Layout — 0x20 bytes:
// Offset  Size  Type              Name
// 0x00     8    vtable*           vptr
// 0x08    24    std::string       message
// 0x20          END
//
// Inherits from std::exception (base class ~= just vptr, trivial)
// ============================================================================
//! \brief Failure raised by `WaveIndexTracker` (and other wavetable
//!        consistency checks) when the requested wave-index
//!        assignment would conflict with an existing entry or exceed
//!        the configured maximum.
class WavetableException : public std::exception {
public:
    //! \brief Diagnostic text returned later by `what()`.
    std::string message_;   // +0x08

    //! \brief Construct the exception with the given diagnostic
    //!        message.
    //! \param msg  Pre-formatted diagnostic, owned-by-copy.
    explicit WavetableException(const std::string& msg);                // 0x29a7e0

    //! \brief Release the embedded `message_` storage and chain to
    //!        `~std::exception`.
    ~WavetableException() override;                                     // 0x29a840

    //! \brief Return the stored diagnostic text.
    //! \return `message_.c_str()`; pointer is valid for the lifetime
    //! of `*this`.
    const char* what() const noexcept override;                         // 0x29f9d0
};

} // namespace zhinst
