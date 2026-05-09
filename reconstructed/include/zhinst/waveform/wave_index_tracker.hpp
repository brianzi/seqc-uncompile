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
//! Tracks which wave indices have already been assigned within one
//! wavetable, and hands out the next available index on demand.
//!
//! `assign()` reserves a specific index (raising
//! `WavetableException` on a duplicate or out-of-range value);
//! `assignAuto()` reserves the lowest unused index. `hasGaps()`
//! lets callers detect a non-contiguous index space, which downstream
//! emitters need to know about.
class WaveIndexTracker {
public:
    int maxIndex;                   // +0x00
    // +0x04: padding
    std::set<int> indices_;         // +0x08 (internally 24 bytes in libc++)
    int autoIndex_;                 // +0x20

    // Constructor — 0x29a5e0
    explicit WaveIndexTracker(int maxIndex);

    // Template constructor from WavetableManager — 0x29d000 / 0x29d410
    template<typename T>
    WaveIndexTracker(int maxIndex, const detail::WavetableManager<T>& mgr);

    // Assign a specific index (resets autoIndex to 0, then calls assignAuto) — 0x29a600
    void assign(int index);

    // Insert index into the set; throws WavetableException on duplicate or >= maxIndex — 0x29a620
    int assignAuto(int index);

    // Returns const ref to the internal set — 0x29a7d0
    const std::set<int>& usedWaveIndices() const;

    // Advance autoIndex past any used indices — 0x29a880
    void getNextAutoIndex();

    // Returns true if autoIndex < max element in set — 0x29a8e0
    bool hasGaps();
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
//! Failure raised by `WaveIndexTracker` (and other wavetable
//! consistency checks) when the requested wave-index assignment
//! would conflict with an existing entry or exceed the configured
//! maximum.
class WavetableException : public std::exception {
public:
    std::string message_;   // +0x08

    // Constructor — 0x29a7e0
    explicit WavetableException(const std::string& msg);

    // Destructor — 0x29a840
    ~WavetableException() override;

    // what() — 0x29f9d0
    const char* what() const noexcept override;
};

} // namespace zhinst
