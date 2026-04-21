// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableFront method implementations
// ============================================================================

#include "zhinst/wavetable_front.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/signal.hpp"
#include "zhinst/value.hpp"
#include "zhinst/waveform_front.hpp"

namespace zhinst {

// 0x29a940 — WavetableFront::~WavetableFront()
WavetableFront::~WavetableFront() {
    // Destroy waveIndexTracker_ set (at this+0x1E0)
    // set<int> tree destroy at this+0x1E0 with root at this+0x1E8

    // Delete manager_
    if (manager_) {
        manager_->~WavetableManager();
        operator delete(manager_, 0x48);
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
    const boost_filesystem_path& path)
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

    // Initialize WaveIndexTracker at this+0x1D8
    // waveIndexTracker_.startIndex_ = dc.wavetableStartIndex; (dc+0x60)

    // Initialize set at this+0x1E0 (empty)
    // this+0x1F8 = 0 (flags)
}

// 0x29ac60 — WavetableFront::dummyWarning(const string&, int)
void WavetableFront::dummyWarning(const std::string& msg, int /*level*/) {
    // Creates a LogRecord with severity=4 (WARNING)
    // Logs "Unhandled warning: " + msg
    // (using boost::log formatting)
}

// 0x29ad00 — WavetableFront::begin() const
const std::shared_ptr<WaveformFront>* WavetableFront::begin() const {
    return manager_->waveforms_.data();  // manager_+0x30
}

// 0x29ad20 — WavetableFront::end() const
const std::shared_ptr<WaveformFront>* WavetableFront::end() const {
    // returns manager_+0x38 (vector end pointer)
    return reinterpret_cast<const std::shared_ptr<WaveformFront>*>(
        *reinterpret_cast<void**>(reinterpret_cast<char*>(manager_) + 0x38));
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
    auto* begin = reinterpret_cast<std::shared_ptr<WaveformFront>*>(
        *reinterpret_cast<void**>(reinterpret_cast<char*>(manager_) + 0x30));
    auto* end = reinterpret_cast<std::shared_ptr<WaveformFront>*>(
        *reinterpret_cast<void**>(reinterpret_cast<char*>(manager_) + 0x38));

    for (auto* it = begin; it != end; ++it) {
        WaveformFront* wf = it->get();
        if (wf->isAllocated() != true) continue;  // wf+0x48 != 1

        uint16_t channels = wf->channels();   // wf+0xC8 (uint16_t)
        uint32_t length = wf->sampleLength(); // wf+0xD0
        Signal* sig = wf->signal();           // wf+0x78

        uint32_t alignedLen;
        if (length == 0) {
            alignedLen = 0;
        } else {
            uint32_t minLen = sig->minLength();    // sig+0x40
            uint32_t granularity = sig->granularity(); // sig+0x44
            // Round up length to granularity, ensure >= minLen
            uint32_t rounded = ((length + granularity - 1) / granularity) * granularity;
            alignedLen = (minLen > rounded) ? minLen : rounded;
        }

        // Calculate memory: channels * bitsPerSample * alignedLen, rounded up to bytes
        int32_t bitsPerSample = sig->bitsPerSample(); // sig+0x50
        size_t totalBits = (size_t)channels * bitsPerSample * alignedLen;
        size_t bytes = (totalBits + 7) / 8;

        if (bytes == 0) continue;

        // If length != 0, recalculate with the signal's actual parameters
        if (length != 0) {
            uint32_t minLen2 = sig->minLength();
            uint32_t gran2 = sig->granularity();
            uint32_t rounded2 = ((length + gran2 - 1) / gran2) * gran2;
            uint32_t aligned2 = (minLen2 > rounded2) ? minLen2 : rounded2;
            size_t bits2 = (size_t)channels * aligned2;
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
    int baseIndex = manager_->lineNr_;
    int counter = manager_->waveformCounter_;
    manager_->waveformCounter_ = counter + 1;

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
    if (ptr->fileType() != 0) return;

    // Check signal allocation: ptr+0x80 is Signal
    // Signal::checkAllocation()
    // If signal's data begin == data end, parse the CSV
    // CsvParser::csvFileToWaveform<WaveformFront>(wf, deviceType)
    // deviceType comes from *deviceConstants_ (first field)

    // On exception (catch type 1 = std::exception):
    //   extract what() message
    //   throw WavetableException(msg)
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
    // Copy name string
    std::string nameCopy = name;

    // Look up in manager_->nameToIndex_
    auto it = manager_->nameToIndex_.find(nameCopy);
    std::shared_ptr<WaveformFront> wf;
    if (it != manager_->nameToIndex_.end()) {
        size_t idx = it->second;
        wf = manager_->waveforms_.at(idx);
    }

    // Make another copy of the shared_ptr and call checkWaveformInit
    auto wf2 = wf;
    // checkWaveformInit(wf2.get(), name):
    //   if ptr == nullptr: throw WavetableException(ErrorMessages::format(0xe9, name))
    //   if ptr->data() == nullptr && ptr->filename is empty:
    //     throw WavetableException(ErrorMessages::format(0xea))
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

    // Return wf->sampleLength() at wf+0xD0
    return wf.get()->sampleLength();
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

    // Compare total < deviceConstants_->maxDioTableEntries (dc+0x0C)
    return total < (size_t)deviceConstants_->maxDioTableEntries();
}

// 0x29cb40 — WavetableFront::assignWaveIndex(shared_ptr<WaveformFront>, int)
void WavetableFront::assignWaveIndex(
    std::shared_ptr<WaveformFront> wf, int index)
{
    WaveformFront* ptr = wf.get();
    int currentIndex = ptr->waveIndex();  // ptr+0x6C

    if (currentIndex == index) return;

    if (currentIndex != -1) {
        // Already has a different index assigned — error
        // throw WavetableException(ErrorMessages::format(0xf8, wf->name()))
        throw; // WavetableException
    }

    // Clear flags at this+0x1F8
    // *(int*)(this + 0x1F8) = 0;

    // Call waveIndexTracker_.assignAuto(index) at this+0x1D8
    // Then set ptr->waveIndex_ = index
    ptr->setWaveIndex(index);
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
    manager_->lineNr_ = nr;
}

} // namespace zhinst
