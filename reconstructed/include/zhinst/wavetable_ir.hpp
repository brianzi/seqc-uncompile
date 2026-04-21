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

#include "waveform_ir.hpp"
#include "wave_index_tracker.hpp"
#include "device_constants.hpp"

#include <memory>
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
class CachedParser;

namespace detail {
    template<typename T> class WavetableManager;
    template<typename T> struct AddressImpl;
}

enum class WaveOrder : int {
    Natural = 0,       // No sorting
    ByName = 1,        // Sort by name
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
    WavetableIR(const WavetableFront& front,
                const DeviceConstants& constants,
                detail::AddressImpl<uint32_t> address,
                size_t wavetableSize,
                const boost::filesystem::path& path,
                std::weak_ptr<CancelCallback> cancelCb);

    // From WavetableManager<WaveformIR> — 0x29d230
    WavetableIR(const detail::WavetableManager<WaveformIR>& mgr,
                const DeviceConstants& constants,
                detail::AddressImpl<uint32_t> address,
                size_t wavetableSize,
                const boost::filesystem::path& path,
                std::weak_ptr<CancelCallback> cancelCb);

    // Destructor — 0x29d550
    ~WavetableIR();

    // ---- Serialization ----

    // toJson — 0x29d670
    boost::json::value toJson() const;

    // fromJson (static, returns shared_ptr<WavetableIR>) — 0x29db10
    static std::shared_ptr<WavetableIR> fromJson(
        boost::json::value json,
        const DeviceConstants& constants,
        detail::AddressImpl<uint32_t> address,
        size_t wavetableSize,
        const boost::filesystem::path& path,
        std::weak_ptr<CancelCallback> cancelCb);

    // ---- Comparison ----

    // operator== — 0x29e090
    bool operator==(const WavetableIR& other) const;

    // ---- Iterators / accessors ----

    // begin — 0x29e270
    const std::shared_ptr<WaveformIR>* begin() const;

    // end — 0x29e280
    const std::shared_ptr<WaveformIR>* end() const;

    // size — 0x29e290
    size_t size() const;

    // getWaveformByName — 0x29e2b0
    std::shared_ptr<WaveformIR> getWaveformByName(
        const std::optional<std::string>& name) const;

    // getNextSegmentAddress — 0x29e320
    uint32_t getNextSegmentAddress() const;

    // getFirstWaveformOffset — 0x29e330
    uint32_t getFirstWaveformOffset() const;

    // ---- Allocation methods ----

    // allocateWaveforms — 0x29e340
    void allocateWaveforms(bool fifoMode);

    // forEachUsedWaveform — 0x29e5e0
    void forEachUsedWaveform(
        std::function<void(const std::shared_ptr<WaveformIR>&)> callback,
        WaveOrder order) const;

    // assignWaveIndexImplicit — 0x29e8a0
    void assignWaveIndexImplicit();

    // setUsedWaveforms — 0x29ece0
    void setUsedWaveforms(const std::vector<std::shared_ptr<WaveformIR>>& waveforms);

    // updateWaveforms — 0x29ed10
    void updateWaveforms(bool fifoMode, bool allocFlag);

    // allocateWaveformsForFifo — 0x29ed30
    void allocateWaveformsForFifo();

    // alignWaveformSizes — 0x29f150
    void alignWaveformSizes();

    // assignWaveformAllocationSizes — 0x29f1d0
    void assignWaveformAllocationSizes();

    // loadWaveform — 0x29f310
    void loadWaveform(std::shared_ptr<WaveformIR> waveform);

    // getJsonIndex — 0x29f480
    std::string getJsonIndex(SampleFormat format) const;
};

} // namespace zhinst
