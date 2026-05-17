// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveIndexTracker and WavetableException method implementations
// ============================================================================

#include "zhinst/waveform/wave_index_tracker.hpp"
#include "zhinst/core/error_messages.hpp"
#include "zhinst/core/types.hpp"   // kNoWaveIndex sentinel

namespace zhinst {

// ============================================================================
// WavetableException
// ============================================================================

// 0x29a7e0 — WavetableException::WavetableException(string const&)
//
// Sets vtable pointer, copies the message string into this->message_ at +0x08.
WavetableException::WavetableException(const std::string& msg)  // 0x29a7e0
    : message_(msg)
{
    // vtable set by compiler
}

// 0x29a840 — WavetableException::~WavetableException()
//
// Sets vtable, destroys message_ string, then calls ~exception().
WavetableException::~WavetableException()  // 0x29a840
{
    // implicit string destruction + base dtor
}

// 0x29f9d0 — WavetableException::what() const
//
// Returns message_.c_str(), or a static empty string if message is empty.
const char* WavetableException::what() const noexcept  // 0x29f9d0
{
    if (message_.empty()) {
        return "";  // static string at rip+0x6605f3
    }
    return message_.c_str();
}

// ============================================================================
// WaveIndexTracker
// ============================================================================

// 0x29a5e0 — WaveIndexTracker::WaveIndexTracker(int)
//
// Initializes:
//   [this+0x00] = maxIndex
//   [this+0x08] = &this[0x10]  (begin_node points to end sentinel)
//   [this+0x10] = 0            (end sentinel left = null root)
//   [this+0x18] = 0            (size = 0)
//   [this+0x20] = 0            (autoIndex = 0)
WaveIndexTracker::WaveIndexTracker(int maxIdx)  // 0x29a5e0
    : maxIndex(maxIdx)
    , indices_()
    , autoIndex_(0)
{
}

// 0x29a600 — WaveIndexTracker::assign(int index)
//
// Resets autoIndex_ to 0, then falls through to assignAuto(index).
void WaveIndexTracker::assign(int index)  // 0x29a600
{
    autoIndex_ = 0;
    assignAuto(index);
}

// 0x29a620 — WaveIndexTracker::assignAuto(int index)
//
// Inserts index into the set. Throws WavetableException if:
//   - index already exists in the set (ErrorMessage 0xF9 = "duplicate wave index")
//   - index >= maxIndex (ErrorMessage 0xFA = "wave index out of range")
// Returns the index on success.
int WaveIndexTracker::assignAuto(int index)  // 0x29a620
{
    // Check if index already in set
    auto it = indices_.find(index);
    if (it != indices_.end()) {
        // Duplicate index — throw ErrorMessage 0xF9
        throw WavetableException(
            ErrorMessages::format(WaveIndexUsed));
    }

    // Check if index >= maxIndex
    if (index >= maxIndex) {
        // Out of range — throw ErrorMessage 0xFA
        throw WavetableException(
            ErrorMessages::format(WaveIndexExceedsTable));
    }

    // Insert into set
    indices_.insert(index);
    return index;
}

// 0x29a7d0 — WaveIndexTracker::usedWaveIndices() const
//
// Simply returns a pointer/reference to the set at +0x08.
// Disassembly: lea rax, [rdi+0x08]; ret
const std::set<int>& WaveIndexTracker::usedWaveIndices() const  // 0x29a7d0
{
    return indices_;
}

// 0x29a880 — WaveIndexTracker::getNextAutoIndex()
//
// Advances autoIndex_ past any indices that are already in the set.
// While autoIndex_ is found in the set, increment it.
void WaveIndexTracker::getNextAutoIndex()  // 0x29a880
{
    while (indices_.find(autoIndex_) != indices_.end()) {
        ++autoIndex_;
    }
}

// 0x29a8e0 — WaveIndexTracker::hasGaps()
//
// Returns true if autoIndex_ < the maximum element in the set.
// If set is empty, returns false.
bool WaveIndexTracker::hasGaps()  // 0x29a8e0
{
    if (indices_.empty()) {
        return false;
    }
    // Find the maximum element (rightmost in the tree)
    auto it = indices_.end();
    --it;
    return autoIndex_ < *it;
}

// ============================================================================
// Template constructor — 0x29d000 (WaveformFront) / 0x29d410 (WaveformIR)
//
// WaveIndexTracker(int maxIndex, WavetableManager<T> const& mgr)
//
// Initializes the tracker and populates the set with all non-(kNoWaveIndex) waveIndex
//   - Reads waveform->waveIndex at offset 0x6C
//   - If waveIndex != kNoWaveIndex, inserts it into the set (skipping duplicates)
// ============================================================================
template<typename T>
WaveIndexTracker::WaveIndexTracker(int maxIdx,  // 0x29d000 / 0x29d410
                                   const detail::WavetableManager<T>& mgr)
    : maxIndex(maxIdx)
    , indices_()
    , autoIndex_(0)
{
    // mgr.waveforms_ is a vector<shared_ptr<T>> at mgr+0x30
    // Each shared_ptr is 16 bytes; element+0x00 = raw pointer to T
    const auto& waveforms = mgr.waveforms_;
    for (const auto& wp : waveforms) {
        int idx = wp->waveIndex;  // Waveform+0x6C
        if (idx != kNoWaveIndex) {
            indices_.insert(idx);  // insert (ignores duplicates)
        }
    }
}

} // namespace zhinst

// ============================================================================
// Explicit template instantiations.
// The binary defines two specializations of the templated ctor at:
//   0x29d000  — WaveIndexTracker(int, WavetableManager<WaveformFront> const&)
//   0x29d410  — WaveIndexTracker(int, WavetableManager<WaveformIR> const&)
// Forcing instantiation here emits the matching symbols so the rest of the
// reconstructed code (which calls these ctors via make_shared / direct
// construction) can link against this archive without depending on header-
// inclusion ordering.
// ============================================================================
#include "zhinst/waveform/wavetable_front.hpp"  // WavetableManager<T> full definition
#include "zhinst/waveform/waveform_front.hpp"
#include "zhinst/waveform/waveform_ir.hpp"
namespace zhinst {
//! \cond INTERNAL
// Explicit template instantiations of the templated WaveIndexTracker
// constructor for the two WavetableManager element types in use.
template WaveIndexTracker::WaveIndexTracker(
    int, const detail::WavetableManager<WaveformFront>&);
template WaveIndexTracker::WaveIndexTracker(
    int, const detail::WavetableManager<WaveformIR>&);
//! \endcond
} // namespace zhinst
