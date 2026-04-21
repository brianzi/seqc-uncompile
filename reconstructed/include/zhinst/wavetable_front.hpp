// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableFront and WavetableManager<WaveformFront>
//
// WavetableFront layout — 0x200 bytes total:
//   0x000: DeviceConstants* deviceConstants_
//   0x008: uint32_t addressValue_       (from AddressImpl<uint32_t>)
//   0x00C: uint32_t addressValue2_      (duplicate/copy)
//   0x010: std::ostringstream oss_       (0x108 bytes, to 0x118)
//   0x118: CachedParser cachedParser_    (0x60 bytes, to 0x178)
//   0x178: std::map<size_t, size_t> dioTableUsage_  (0x18 bytes, to 0x190)
//   0x190: std::function<void(const std::string&, int)> warningCallback_ (0x20 bytes, to 0x1B0)
//   0x1B0: (warningCallback_ __func ptr, points to 0x190 by default)
//   0x1C0: (padding/unused 16 bytes)
//   0x1D0: WavetableManager<WaveformFront>* manager_
//   0x1D8: WaveIndexTracker waveIndexTracker_ (0x28 bytes, to 0x200)
//          0x1D8: int lineNr_ / startIndex (first field of manager, stored separately)
//          0x1E0: std::set<int> root node ptr
//          0x1E8: set internal data (16 bytes)
//          0x1F8: int flags/state
//
// WavetableManager<WaveformFront> layout — 0x48 bytes:
//   0x00: int lineNr_
//   0x04: int waveformCounter_
//   0x08: unordered_map<string, size_t> nameToIndex_ (hash table, ~0x28 bytes)
//   0x30: vector<shared_ptr<WaveformFront>> waveforms_ (begin)
//   0x38: vector end
//   0x40: vector capacity end
//   0x28: float (1.0f default at +0x28 in the new'd block)
//
// Actually refined from constructor at 0x29ab10:
//   The 0x48-byte allocation is zeroed then:
//     +0x28 = 0x3f800000 (1.0f)
//     +0x30, +0x38, +0x40 = 0 (vector)
//   This matches:
//     0x00: int lineNr_          (set from DeviceConstants+0x60)
//     0x04: int counter_
//     0x08: unordered_map<string, size_t> nameToIndex_ (hash table)
//     0x28: float amplitudeDefault_ (1.0f)
//     0x30: shared_ptr<WaveformFront>* begin_ (vector data)
//     0x38: shared_ptr<WaveformFront>* end_
//     0x40: shared_ptr<WaveformFront>* cap_
//
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.hpp"
#include "address_impl.hpp"

namespace zhinst {

namespace detail {

// ============================================================================
// WavetableManager<WaveformFront> — 0x48 bytes
// Heap-allocated, owned by WavetableFront
// ============================================================================
template <typename WaveformT>
class WavetableManager {
public:
    // 0x00
    int lineNr_;
    // 0x04
    int waveformCounter_;
    // 0x08 — unordered_map<string, size_t>: name -> index in waveforms_ vector
    std::unordered_map<std::string, size_t> nameToIndex_;
    // 0x30
    std::vector<std::shared_ptr<WaveformT>> waveforms_;

    // --- Methods ---
    ~WavetableManager();                                    // 0x29fa40

    std::shared_ptr<WaveformT> newEmptyWaveform(
        const std::string& name,
        const DeviceConstants& dc);                         // 0x29aec0

    std::shared_ptr<WaveformT> newWaveformFromFile(
        const std::string& name,
        const std::string& filename,
        Waveform::File::Type type,
        const DeviceConstants& dc);                         // 0x29b110

    std::shared_ptr<WaveformT> newWaveformFromFile(
        const std::string& name,
        const Signal& signal,
        AddressImpl<uint32_t> addr,
        const std::string& filename,
        Waveform::File::Type type,
        const DeviceConstants& dc);                         // 0x29b560

    std::shared_ptr<WaveformT> newWaveform(
        const std::string& name,
        const Signal& signal,
        const std::string& funName,
        const std::vector<Value>& args,
        const DeviceConstants& dc);                         // 0x29ba00

    std::shared_ptr<WaveformT> getWaveformForFront(
        const std::string& funName,
        const std::vector<Value>& args) const;              // 0x29c210

    std::shared_ptr<WaveformT> copyWaveform(
        std::shared_ptr<WaveformT> src);                    // 0x29c440

    void updateWave(
        std::shared_ptr<WaveformT> wf,
        const std::string& newName);                        // 0x29ccf0

    void insertWaveform(std::shared_ptr<WaveformT> wf);    // 0x2a1200

    void setLineNr(int nr);                                 // (inlined via WavetableFront)
};

} // namespace detail

// ============================================================================
// WavetableFront — 0x200 bytes
// ============================================================================
class WavetableFront {
public:
    // 0x000: DeviceConstants*
    const DeviceConstants* deviceConstants_;
    // 0x008: AddressImpl<uint32_t> (stored twice)
    uint32_t address_;
    uint32_t address2_;
    // 0x010: std::ostringstream (0x108 bytes)
    std::ostringstream oss_;
    // 0x118: CachedParser (0x60 bytes)
    char cachedParser_[0x60]; // CachedParser
    // 0x178: std::map<size_t, size_t> dioTableUsage_
    std::map<size_t, size_t> dioTableUsage_;
    // 0x190: std::function<void(const string&, int)> warningCallback_
    std::function<void(const std::string&, int)> warningCallback_;
    // 0x1B0: (internal function storage pointer — points to warningCallback_ SSO or heap)
    char warningCallbackStorage_[0x20];
    // 0x1D0: WavetableManager<WaveformFront>* manager_
    detail::WavetableManager<WaveformFront>* manager_;
    // 0x1D8: WaveIndexTracker waveIndexTracker_ (0x28 bytes)
    char waveIndexTracker_[0x28];

    // --- Constructor / Destructor ---
    WavetableFront(
        const DeviceConstants& dc,
        detail::AddressImpl<uint32_t> addr,
        size_t lineNr,
        const boost_filesystem_path& path);                 // 0x29ab10

    ~WavetableFront();                                      // 0x29a940

    // --- Accessors ---
    static void dummyWarning(const std::string& msg, int level); // 0x29ac60

    const std::shared_ptr<WaveformFront>* begin() const;    // 0x29ad00
    const std::shared_ptr<WaveformFront>* end() const;      // 0x29ad20

    void setWarningCallback(
        std::function<void(const std::string&, int)> cb);   // 0x29ad40

    size_t getMemorySize() const;                           // 0x29adc0

    // --- Waveform creation (delegates to manager_) ---
    std::shared_ptr<WaveformFront> newEmptyWaveform(
        const std::string& name);                           // 0x29ae90

    std::shared_ptr<WaveformFront> newWaveformFromFile(
        const std::string& name,
        const std::string& filename,
        Waveform::File::Type type);                         // 0x29b0e0

    std::shared_ptr<WaveformFront> newWaveformFromFile(
        const std::string& name,
        const Signal& signal,
        const std::string& filename,
        Waveform::File::Type type);                         // 0x29b520

    std::shared_ptr<WaveformFront> newWaveform(
        const std::string& name,
        const Signal& signal,
        const std::string& funName,
        const std::vector<Value>& args);                    // 0x29b9d0

    std::shared_ptr<WaveformFront> newWaveform(
        const Signal& signal,
        const std::string& funName,
        const std::vector<Value>& args);                    // 0x29bce0

    // --- Query / Utility ---
    std::string toString() const;                           // 0x29bd90

    void loadWaveform(std::shared_ptr<WaveformFront> wf);   // 0x29bff0

    bool waveformExists(const std::string& name) const;     // 0x29c160

    std::shared_ptr<WaveformFront> getWaveformByName(
        const std::optional<std::string>& name) const;      // 0x29c180

    std::shared_ptr<WaveformFront> getWaveformByFunDescr(
        const std::string& funName,
        const std::vector<Value>& args) const;              // 0x29c1f0

    std::shared_ptr<WaveformFront> copyWaveform(
        const std::shared_ptr<WaveformFront>& src);         // 0x29c3b0

    void checkWaveformInitialized(
        const std::string& name);                           // 0x29c540

    uint32_t getWaveformSampleLength(
        const std::string& name);                           // 0x29c860

    bool updateDioTableUsage(size_t key, size_t value);     // 0x29ca10

    void assignWaveIndex(
        std::shared_ptr<WaveformFront> wf, int index);      // 0x29cb40

    void updateWave(
        std::shared_ptr<WaveformFront> wf,
        const std::string& newName);                        // 0x29cc70

    void setLineNr(int nr);                                 // 0x29ce10
};

} // namespace zhinst
