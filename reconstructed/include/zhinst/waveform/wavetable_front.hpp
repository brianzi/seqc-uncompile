// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableFront and WavetableManager<WaveformFront>
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

#include "zhinst/core/types.hpp"
#include "zhinst/asm/address_impl.hpp"
#include "zhinst/waveform/signal.hpp"
#include "zhinst/ast/value.hpp"

#include "zhinst/waveform/waveform.hpp"

#include <boost/filesystem/path.hpp>

namespace zhinst {
class WaveformFront;  // used as template argument in WavetableManager<WaveformFront>*
}  // namespace zhinst

namespace zhinst {

namespace detail {

// Offset  Size  Type                                       Name
// +0x00   4     int                                        numDefs_
// +0x04   4     int                                        numDefs2_
// +0x08   40    unordered_map<string, size_t>              nameToIndex_
// +0x28   4     float                                      amplitudeDefault_  (init=1.0f)
// +0x2C   4     (padding)
// +0x30   24    vector<shared_ptr<WaveformT>>              waveforms_
// sizeof(WavetableManager) = 0x48
template <typename WaveformT>
class WavetableManager {
public:
    int numDefs_;           // +0x00
    int numDefs2_;          // +0x04
    std::unordered_map<std::string, size_t> nameToIndex_;   // +0x08 (40B)
    float amplitudeDefault_ = 1.0f;                         // +0x28
    // +0x2C: 4B padding
    std::vector<std::shared_ptr<WaveformT>> waveforms_;     // +0x30

    // --- Methods ---
    WavetableManager() = default;
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

    // IR-specialization methods (declared here, defined in wavetable_manager_ir.cpp)
    WavetableManager(int numDefs, int numDefs2, const std::vector<Waveform>& waveforms);

    std::shared_ptr<WaveformT> newWaveform(
        const std::string& name,
        const Signal& signal,
        const std::string& fillName,
        const DeviceConstants& dc);

    boost::json::value toJson() const;
    static WavetableManager fromJson(const boost::json::value& json, const DeviceConstants& dc);
    bool operator==(const WavetableManager& other) const;

    void setLineNr(int nr);                                 // (inlined via WavetableFront)
};

} // namespace detail

// Offset  Size   Type                                    Name
// +0x000  8      DeviceConstants const*                  deviceConstants_
// +0x008  4      uint32_t                                address_
// +0x00C  4      uint32_t                                address2_
// +0x010  0x108  std::ostringstream                      oss_
// +0x118  0x60   CachedParser (opaque)                   cachedParser_
// +0x178  24     std::map<size_t, size_t>                dioTableUsage_
// +0x190  32     std::function<void(string,int)>         warningCallback_
// +0x1B0  32     (internal warningCallback storage/pad)  warningCallbackStorage_
// +0x1D0  8      WavetableManager<WaveformFront>*        manager_
// +0x1D8  0x28   WaveIndexTracker (opaque)               waveIndexTracker_
// sizeof(WavetableFront) = 0x200
class WavetableFront {
public:
    const DeviceConstants* deviceConstants_;            // +0x000
    uint32_t address_;                                  // +0x008
    uint32_t address2_;                                 // +0x00C
    std::ostringstream oss_;                            // +0x010 (0x108B)
    char cachedParser_[0x60];                           // +0x118
    std::map<size_t, size_t> dioTableUsage_;            // +0x178
    std::function<void(const std::string&, int)> warningCallback_;  // +0x190
    char warningCallbackStorage_[0x20];                 // +0x1B0
    detail::WavetableManager<WaveformFront>* manager_;  // +0x1D0
    char waveIndexTracker_[0x28];                       // +0x1D8

    // --- Constructor / Destructor ---
    WavetableFront(
        const DeviceConstants& dc,
        detail::AddressImpl<uint32_t> addr,
        size_t lineNr,
        const boost::filesystem::path& path);                 // 0x29ab10

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
