// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableFront method implementations
// ============================================================================

#include "zhinst/waveform/wavetable_front.hpp"
#include "zhinst/waveform/wavetable_helpers.hpp"
#include "zhinst/io/cached_parser.hpp"
#include "zhinst/device/device_constants.hpp"
#include "zhinst/core/types.hpp"   // kNoWaveIndex sentinel
#include "zhinst/waveform/signal.hpp"
#include "zhinst/ast/value.hpp"
#include "zhinst/waveform/wave_index_tracker.hpp"
#include "zhinst/waveform/waveform_front.hpp"
#include "zhinst/core/error_messages.hpp"
#include "zhinst/infra/logging.hpp"

namespace zhinst {

// Forward declaration of CsvParser (defined in csv_parser.cpp)
class CsvParser {
public:
    //! \brief Local forward declaration of
    //! `CsvParser::csvFileToWaveform` used to call into the CSV
    //! loader from wavetable-front code without pulling in the full
    //! `csv_parser.cpp` interface; see the primary definition in
    //! `csv_parser.cpp` for the full contract.
    //! \tparam WfT       Waveform front type.
    //! \param cache       Shared CSV cache keyed by file hash.
    //! \param wf          Waveform whose `file` and `signal` are
    //!                    populated.
    //! \param deviceType  Selects the per-device sample-encoding
    //!                    branch.
    template <typename WfT>
    static void csvFileToWaveform(CachedParser& cache, std::shared_ptr<WfT> wf, AwgDeviceType deviceType);
};

// 0x2a0fd0 — getUniqueName (also used by WavetableManager and WavetableIR).
// Definition lives in wavetable_helpers.hpp as an inline detail-namespace
// helper (single ODR-clean definition shared by all wavetable TUs).
using detail::getUniqueName;

// 0x29a940 — WavetableFront::~WavetableFront()
WavetableFront::~WavetableFront() {
    // Destroy waveIndexTracker_ (placement-new'd in constructor)
    reinterpret_cast<WaveIndexTracker*>(waveIndexTracker_)->~WaveIndexTracker();

    // Delete manager_
    if (manager_) {
        manager_->~WavetableManager();
        operator delete(manager_, sizeof(detail::WavetableManager<WaveformFront>));
    }

    // Release warningCallback_ weak_count (this+0x1C8)
    // Destroy warningCallback_ function object (this+0x1B0)
    // Destroy dioTableUsage_ map (this+0x178)
    // Destroy cachedParser_ string members (at 0x148, 0x160)
    // Destroy CachedParser tree (at 0x118)
    // Destroy ostringstream (at 0x010)
    // Destroy basic_ios base
}

// 0x29ab10 — WavetableFront::WavetableFront(const DeviceConstants&, AddressImpl<uint32_t>, size_t, const path&)
WavetableFront::WavetableFront(
    const DeviceConstants& dc,
    detail::AddressImpl<uint32_t> addr,
    size_t lineNr,
    const boost::filesystem::path& path)
{
    deviceConstants_ = &dc;
    address_ = addr.value;
    address2_ = addr.value;

    // Initialize ostringstream at this+0x10
    // new (&oss_) std::ostringstream();

    // Initialize CachedParser at this+0x118
    // new (&cachedParser_) CachedParser(lineNr, path);

    // Initialize dioTableUsage_ map (this+0x178) — empty
    // dioTableUsage_ = {};

    // Initialize warningCallback_ with dummyWarning function pointer
    // warningCallback_ = &WavetableFront::dummyWarning;

    // Allocate WavetableManager (0x48 bytes)
    manager_ = new detail::WavetableManager<WaveformFront>();
    // manager_ is zeroed, then:
    //   manager_+0x28 = 1.0f (amplitude default)
    //   All other fields zero

    // Initialize WaveIndexTracker storage at this+0x1D8                       // @0x29abdd
    // Cannot use placement new because sizeof(WaveIndexTracker) differs
    // between libc++ (0x28) and libstdc++ (0x40) due to std::set layout.
    // The WavetableIR rebuilds its own tracker from waveform indices, so the
    // front-end tracker set is not critical. Zero-init to prevent garbage.
    std::memset(waveIndexTracker_, 0, sizeof(waveIndexTracker_));
}

// 0x29ac60 — WavetableFront::dummyWarning(const string&, int)
//
// Default warning sink installed by `WavetableFront::warningCallback_`
// when the user has not provided a custom callback.  The binary emits
// a single Boost.Log record at Severity=4 (Warning) of the form
// `"Warning not tracked: " + msg`.  The `int /*level*/` parameter is
// the warning level the call site requested; it is intentionally
// dropped here because there is no per-warning-level filtering on the
// default sink.
//
// Behaviour is invisible to the difftest suite because every test
// installs a real callback via `setWarningCallback`.
void WavetableFront::dummyWarning(const std::string& msg, int /*level*/) {
    logging::detail::LogRecord rec(logging::Severity::Warning);
    rec << "Warning not tracked: " << msg;
}

// 0x29ad00 — WavetableFront::begin() const
const std::shared_ptr<WaveformFront>* WavetableFront::begin() const {
    return manager_->waveforms_.data();  // manager_+0x30
}

// 0x29ad20 — WavetableFront::end() const
const std::shared_ptr<WaveformFront>* WavetableFront::end() const {
    // returns pointer past last element of manager_->waveforms_
    return manager_->waveforms_.data() + manager_->waveforms_.size();
}

// 0x29ad40 — WavetableFront::setWarningCallback(function<void(const string&, int)>)
void WavetableFront::setWarningCallback(
    std::function<void(const std::string&, int)> cb)
{
    // Swaps the incoming callback into warningCallback_ at this+0x190
    warningCallback_.swap(cb);
}

// 0x29adc0 — WavetableFront::getMemorySize() const
size_t WavetableFront::getMemorySize() const {
    size_t total = 0;
    auto* begin = manager_->waveforms_.data();
    auto* end = begin + manager_->waveforms_.size();

    for (auto* it = begin; it != end; ++it) {
        WaveformFront* wf = it->get();
        if (wf->used != true) continue;  // Waveform::used at +0x48

        uint16_t channels = wf->signal.channels_;   // wf+0xC8 = signal+0x48
        uint32_t length = static_cast<uint32_t>(wf->signal.length_); // wf+0xD0 = signal+0x50
        // Verified disasm 0x29ae31..0x29ae53:
        //   r10 = [wf+0x78] = waveform->deviceConstants  (NOT &signal!)
        //   r9d = [r10+0x40] = maxWaveformLength ("max" cap)
        //   ebx = [r10+0x44] = grainSize    (alignment grain)
        //   eax = ceil_div(length, ebx) * ebx
        //   if (r9d > eax) eax = r9d   ; cmova → max
        //   r9 = sxd[r10+0x50] = bitsPerSample
        const DeviceConstants* dc = wf->deviceConstants;

        uint32_t alignedLen;
        if (length == 0) {
            alignedLen = 0;
        } else {
            uint32_t wfMaxCap = dc->maxWaveformLength; // +0x40
            uint32_t wfGrain  = dc->grainSize;    // +0x44
            uint32_t rounded = ((length + wfGrain - 1) / wfGrain) * wfGrain;
            alignedLen = (wfMaxCap > rounded) ? wfMaxCap : rounded;
        }

        // Calculate memory: channels * bitsPerSample * alignedLen, rounded up to bytes
        int32_t bitsPerSample = static_cast<int32_t>(dc->bitsPerSample); // +0x50
        size_t totalBits = static_cast<size_t>(channels) * bitsPerSample * alignedLen;
        size_t bytes = (totalBits + 7) / 8;

        if (bytes == 0) continue;

        // If length != 0, recalculate with the signal's actual parameters
        if (length != 0) {
            uint32_t wfMaxCap2 = dc->maxWaveformLength;
            uint32_t wfGrain2  = dc->grainSize;
            uint32_t rounded2 = ((length + wfGrain2 - 1) / wfGrain2) * wfGrain2;
            uint32_t aligned2 = (wfMaxCap2 > rounded2) ? wfMaxCap2 : rounded2;
            size_t bits2 = static_cast<size_t>(channels) * aligned2;
            size_t b2 = (bits2 + 7) / 8;
            total += b2;
        }
    }
    return total;
}

// 0x29ae90 — WavetableFront::newEmptyWaveform(const string&)
std::shared_ptr<WaveformFront> WavetableFront::newEmptyWaveform(
    const std::string& name)
{
    // Passes deviceConstants_ and manager_ to the manager method
    return manager_->newEmptyWaveform(name, *deviceConstants_);
}

// 0x29b0e0 — WavetableFront::newWaveformFromFile(const string&, const string&, Type)
std::shared_ptr<WaveformFront> WavetableFront::newWaveformFromFile(
    const std::string& name,
    const std::string& filename,
    Waveform::File::Type type)
{
    return manager_->newWaveformFromFile(name, filename, type, *deviceConstants_);
}

// 0x29b520 — WavetableFront::newWaveformFromFile(const string&, const Signal&, const string&, Type)
std::shared_ptr<WaveformFront> WavetableFront::newWaveformFromFile(
    const std::string& name,
    const Signal& signal,
    const std::string& filename,
    Waveform::File::Type type)
{
    // Extracts address from this (this+0x08) and passes DeviceConstants
    return manager_->newWaveformFromFile(
        name, signal, detail::AddressImpl<uint32_t>{address_},
        filename, type, *deviceConstants_);
}

// 0x29b9d0 — WavetableFront::newWaveform(const string&, const Signal&, const string&, const vector<Value>&)
std::shared_ptr<WaveformFront> WavetableFront::newWaveform(
    const std::string& name,
    const Signal& signal,
    const std::string& funName,
    const std::vector<Value>& args)
{
    return manager_->newWaveform(name, signal, funName, args, *deviceConstants_);
}

// 0x29bce0 — WavetableFront::newWaveform(const Signal&, const string&, const vector<Value>&)
std::shared_ptr<WaveformFront> WavetableFront::newWaveform(
    const Signal& signal,
    const std::string& funName,
    const std::vector<Value>& args)
{
    // Gets unique name from counter, then delegates
    int baseIndex = manager_->numDefs_;
    int counter = manager_->numDefs2_;
    manager_->numDefs2_ = counter + 1;

    std::string uniqueName = getUniqueName(funName, baseIndex, counter);

    return manager_->newWaveform(uniqueName, signal, funName, args, *deviceConstants_);
}

// 0x29bd90 — WavetableFront::toString() const
std::string WavetableFront::toString() const {
    std::ostringstream oss;
    auto* begin_ptr = manager_->waveforms_.data();
    auto* end_ptr = begin_ptr + manager_->waveforms_.size();

    for (auto* it = begin_ptr; it != end_ptr; ++it) {
        WaveformFront* wf = it->get();
        oss << wf->toString();
    }
    return oss.str();
}

// 0x29bff0 — WavetableFront::loadWaveform(shared_ptr<WaveformFront>)
void WavetableFront::loadWaveform(std::shared_ptr<WaveformFront> wf) {
    WaveformFront* ptr = wf.get();

    // If file type != CSV (i.e., type at wf+0x18 != 0), return
    if (ptr->waveformType != Waveform::File::Type::CSV) return;  // wf+0x18

    // Check signal allocation
    ptr->signal.checkAllocation();

    // If signal's samples are already populated, skip parsing
    if (!ptr->signal.samples_.empty()) return;

    // Parse CSV via CsvParser, using the embedded CachedParser
    auto deviceType = static_cast<AwgDeviceType>(deviceConstants_->deviceType);
    auto& cache = *reinterpret_cast<CachedParser*>(cachedParser_);             // +0x118
    try {
        CsvParser::csvFileToWaveform<WaveformFront>(cache, wf, deviceType);    // @0x29c080
    } catch (std::exception const& e) {
        throw WavetableException(e.what());
    }
}

// 0x29c160 — WavetableFront::waveformExists(const string&) const
bool WavetableFront::waveformExists(const std::string& name) const {
    // Searches manager_->nameToIndex_ (at manager_+0x08) for name
    auto it = manager_->nameToIndex_.find(name);
    return it != manager_->nameToIndex_.end();
}

// 0x29c180 — WavetableFront::getWaveformByName(const optional<string>&) const
std::shared_ptr<WaveformFront> WavetableFront::getWaveformByName(
    const std::optional<std::string>& name) const
{
    if (!name.has_value()) return nullptr;

    auto it = manager_->nameToIndex_.find(*name);
    if (it == manager_->nameToIndex_.end()) return nullptr;

    size_t idx = it->second;
    return manager_->waveforms_.at(idx);
}

// 0x29c1f0 — WavetableFront::getWaveformByFunDescr(const string&, const vector<Value>&) const
std::shared_ptr<WaveformFront> WavetableFront::getWaveformByFunDescr(
    const std::string& funName,
    const std::vector<Value>& args) const
{
    return manager_->getWaveformForFront(funName, args);
}

// 0x29c3b0 — WavetableFront::copyWaveform(const shared_ptr<WaveformFront>&)
std::shared_ptr<WaveformFront> WavetableFront::copyWaveform(
    const std::shared_ptr<WaveformFront>& src)
{
    auto copy = src;  // increment refcount
    return manager_->copyWaveform(std::move(copy));
}

// 0x29c540 — WavetableFront::checkWaveformInitialized(const string&)
void WavetableFront::checkWaveformInitialized(const std::string& name) {
    std::string nameCopy = name;

    // Look up in manager_->nameToIndex_
    auto it = manager_->nameToIndex_.find(nameCopy);
    std::shared_ptr<WaveformFront> wf;
    if (it != manager_->nameToIndex_.end()) {
        size_t idx = it->second;
        wf = manager_->waveforms_.at(idx);
    }

    // Check: if waveform not found, throw error 0xe9
    if (!wf) {
        throw WavetableException(
            ErrorMessages::format(WaveformNotFound, name));
    }

    // Check: if waveform has no data (no file and empty genFunc), throw 0xea
    // This happens when a waveform generator fails (e.g., marker(32,1,1) throws
    // but the variable still gets registered with an empty waveform)
    if (!wf->file && wf->funDescrName().empty()) {
        throw WavetableException(
            ErrorMessages::format(UninitializedWaveform));
    }
}

// 0x29c860 — WavetableFront::getWaveformSampleLength(const string&)
uint32_t WavetableFront::getWaveformSampleLength(const std::string& name) {
    // Same lookup pattern as checkWaveformInitialized
    std::string nameCopy = name;

    auto it = manager_->nameToIndex_.find(nameCopy);
    std::shared_ptr<WaveformFront> wf;
    if (it != manager_->nameToIndex_.end()) {
        size_t idx = it->second;
        wf = manager_->waveforms_.at(idx);
    }

    auto wf2 = wf;
    // checkWaveformInit(wf2.get(), name) — validates waveform

    // Return wf->signal.length_ at wf+0xD0 (= signal+0x50)
    return static_cast<uint32_t>(wf.get()->signal.length_);
}

// 0x29ca10 — WavetableFront::updateDioTableUsage(size_t key, size_t value)
bool WavetableFront::updateDioTableUsage(size_t key, size_t value) {
    // Insert or update in dioTableUsage_ map (this+0x178)
    dioTableUsage_[key] = value;

    // Sum all values in the map
    size_t total = 0;
    for (auto& [k, v] : dioTableUsage_) {
        total += v;
    }

    // Compare total < deviceConstants_->maxDioTableEntries() (dc+0x0C)
    return total < static_cast<size_t>(deviceConstants_->maxDioTableEntries());
}

// 0x29cb40 — WavetableFront::assignWaveIndex(shared_ptr<WaveformFront>, int)
void WavetableFront::assignWaveIndex(
    std::shared_ptr<WaveformFront> wf, int index)
{
    WaveformFront* ptr = wf.get();
    int currentIndex = ptr->waveIndex;  // ptr+0x6C

    if (currentIndex == index) return;

    if (currentIndex != kNoWaveIndex) {
        // Already has a different index assigned — error
        // Binary throws WavetableException with ErrorMessage 0xF8 (=248,
        // WaveAlreadyAssigned): "waveform %1% has already assigned index".
        throw WavetableException(
            ErrorMessages::format(WaveAlreadyAssigned, ptr->name));
    }

    // In the binary, this clears autoIndex_ and calls assignAuto(index)       // @0x29cb5f
    // which inserts into the tracker's std::set. We can't use the tracker
    // directly due to libc++/libstdc++ ABI mismatch (see constructor note).
    // The WavetableIR rebuilds its tracker from waveform indices, so just
    // setting waveIndex is sufficient for correct compilation.
    ptr->waveIndex = index;                                                    // @0x29cb7c
}

// 0x29cc70 — WavetableFront::updateWave(shared_ptr<WaveformFront>, const string&)
void WavetableFront::updateWave(
    std::shared_ptr<WaveformFront> wf,
    const std::string& newName)
{
    auto copy = wf;  // bump refcount
    manager_->updateWave(std::move(copy), newName);
}

// 0x29ce10 — WavetableFront::setLineNr(int)
void WavetableFront::setLineNr(int nr) {
    manager_->numDefs_ = nr;
}

} // namespace zhinst
