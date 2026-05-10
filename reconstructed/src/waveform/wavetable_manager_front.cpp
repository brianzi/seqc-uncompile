// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableManager<WaveformFront> method implementations
// ============================================================================

#include "zhinst/waveform/wavetable_front.hpp"
#include "zhinst/waveform/wavetable_helpers.hpp"
#include "zhinst/device/device_constants.hpp"
#include "zhinst/waveform/signal.hpp"
#include "zhinst/ast/value.hpp"
#include "zhinst/waveform/waveform_front.hpp"

namespace zhinst {
namespace detail {

// 0x2a0fd0 — getUniqueName(const string& base, int index, int counter).
// Definition lives in wavetable_helpers.hpp as an inline detail-namespace
// helper (single ODR-clean definition shared by all wavetable TUs); since
// this TU is already inside `namespace zhinst::detail`, the helper is
// directly in scope without any using-declaration.

// 0x29fa40 — WavetableManager<WaveformFront>::~WavetableManager()
//! \copydoc zhinst::detail::WavetableManager::~WavetableManager
template<>
WavetableManager<WaveformFront>::~WavetableManager() {
    // Release all shared_ptrs in waveforms_ vector (iterate from end to begin)
    if (waveforms_.data()) {
        for (auto it = waveforms_.end(); it != waveforms_.begin();) {
            --it;
            it->reset();
        }
        // Free vector storage
    }

    // Destroy nameToIndex_ unordered_map
    // Walk linked list of hash nodes (this+0x18), free strings and nodes
    // Free bucket array (this+0x08)
}

// 0x2a1200 — WavetableManager<WaveformFront>::insertWaveform(shared_ptr<WaveformFront>)
//! \copydoc zhinst::detail::WavetableManager::insertWaveform
template<>
void WavetableManager<WaveformFront>::insertWaveform(
    std::shared_ptr<WaveformFront> wf)
{
    // Get current vector size (before insertion) as the index
    size_t idx = waveforms_.size();

    // Push back the shared_ptr into waveforms_
    waveforms_.emplace_back(wf);

    // Insert name -> idx mapping into nameToIndex_
    const std::string& name = wf.get()->name;
    nameToIndex_.emplace(name, idx);
}

// 0x29aec0 — WavetableManager<WaveformFront>::newEmptyWaveform(const string&, const DeviceConstants&)
//! \copydoc zhinst::detail::WavetableManager::newEmptyWaveform
template<>
std::shared_ptr<WaveformFront>
WavetableManager<WaveformFront>::newEmptyWaveform(
    const std::string& name,
    const DeviceConstants& dc)
{
    int baseIdx = numDefs_;  // this+0x00
    int counter = numDefs2_; // this+0x04
    numDefs2_ = counter + 1;

    std::string uniqueName = getUniqueName(name, baseIdx, counter);

    // Allocate WaveformFront (0x110 bytes: 0x10 refcount header + 0xF8 + 0x08 padding)
    // Initialize: set vtable, zero refcounts
    // Copy uniqueName into waveform name field (offset +0x18 in object, +0x30 in alloc block)
    // Set file type = GEN (2) at +0x30
    // Zero all signal/data fields
    // Set waveIndex = -1 at +0x84
    // Set defaultBitsPerSample from dc.defaultBitsPerSample (dc+0x40) at +0x88
    // Set deviceConstants ptr at +0x90
    // Set various extension fields to defaults
    //   +0xF0 = 1 (playback count)
    //   +0xF4 = 0 (flags)
    //   +0xF8..+0x108 = 0

    auto wf = std::make_shared<WaveformFront>(uniqueName, Waveform::File::Type::GEN, dc);
    // Inlined ctor: sets name, fileType=GEN(2), waveIndex=-1, bitsPerSample=dc[0x40],
    //   deviceConstants ptr, playIndex=1, all signal/file/args zeroed

    // Insert into vector and name map
    insertWaveform(wf);

    return wf;
}

// 0x29b110 — WavetableManager<WaveformFront>::newWaveformFromFile(name, filename, type, dc)
//! \copydoc zhinst::detail::WavetableManager::newWaveformFromFile(const std::string&,const std::string&,Waveform::File::Type,const DeviceConstants&)
template<>
std::shared_ptr<WaveformFront>
WavetableManager<WaveformFront>::newWaveformFromFile(
    const std::string& name,
    const std::string& filename,
    Waveform::File::Type type,
    const DeviceConstants& dc)
{
    // Allocate WaveformFront (0x110 bytes)
    // Initialize with name, set file type
    // Zero all data fields, set defaults (same as newEmptyWaveform)

    // Create Waveform::File object (0x58 bytes):
    //   Copy filename string at +0x18
    //   Set +0x30 = {0, 1} (two ints: channel=0, version=1)
    //   Set +0x38 = 1 (loaded flag)
    //   Zero +0x40..+0x50

    // Store file shared_ptr in waveform: wf+0x50 (File* ptr), wf+0x58 (ctrl block)

    // Look up existing waveform with same name in nameToIndex_
    std::string nameCopy = name;
    auto it = nameToIndex_.find(nameCopy);
    std::shared_ptr<WaveformFront> existing;
    if (it != nameToIndex_.end()) {
        size_t idx = it->second;
        existing = waveforms_.at(idx);
    }

    // If existing found, mark both as having duplicates: wf+0xDD = 1
    if (existing) {
        existing.get()->setHasDuplicate(true); // +0xDD
    }

    // Create waveform with inlined ctor, then attach File object post-construction
    auto wf = std::make_shared<WaveformFront>(name, type, dc);
    wf->setFile(std::make_shared<Waveform::File>(filename));  // File at +0x38
    if (existing)
        wf->setHasDuplicate(true);  // +0xDD

    insertWaveform(wf);

    return wf;
}

// 0x29b560 — WavetableManager<WaveformFront>::newWaveformFromFile(name, signal, addr, filename, type, dc)
//! \copydoc zhinst::detail::WavetableManager::newWaveformFromFile(const std::string&,const Signal&,AddressImpl<uint32_t>,const std::string&,Waveform::File::Type,const DeviceConstants&)
template<>
std::shared_ptr<WaveformFront>
WavetableManager<WaveformFront>::newWaveformFromFile(
    const std::string& name,
    const Signal& signal,
    AddressImpl<uint32_t> addr,
    const std::string& filename,
    Waveform::File::Type type,
    const DeviceConstants& dc)
{
    // Same as above but also copies signal data into waveform:
    //   - Copies signal.samples (double vector) into wf+0x98
    //   - Copies signal.markers (byte vector) into wf+0xB0
    //   - Copies signal.markers2 (byte vector) into wf+0xC8
    //   - Copies signal.playbackConfig (16 bytes) into wf+0xE0

    // Also creates File object and stores it

    // Sets wf->fileType_ from the 'type' parameter (stored at wf+0x4C after file)
    // Marks duplicate if name already exists

    // Look up and handle duplicates same as above
    // Insert waveform

    auto wf = std::make_shared<WaveformFront>(name, type, dc);
    wf->signal = signal;                                          // copy Signal data
    wf->setFile(std::make_shared<Waveform::File>(filename));      // File at +0x38
    // address stored at +0x34 (AddressImpl value)

    // Duplicate detection same as 4-arg overload
    std::string nameCopy = name;
    auto it = nameToIndex_.find(nameCopy);
    if (it != nameToIndex_.end()) {
        auto& existing = waveforms_.at(it->second);
        existing->setHasDuplicate(true);
        wf->setHasDuplicate(true);
    }

    insertWaveform(wf);
    return wf;
}

// 0x29ba00 — WavetableManager<WaveformFront>::newWaveform(name, signal, funName, args, dc)
//! \copydoc zhinst::detail::WavetableManager::newWaveform(const std::string&,const Signal&,const std::string&,const std::vector<Value>&,const DeviceConstants&)
template<>
std::shared_ptr<WaveformFront>
WavetableManager<WaveformFront>::newWaveform(
    const std::string& name,
    const Signal& signal,
    const std::string& funName,
    const std::vector<Value>& args,
    const DeviceConstants& dc)
{
    // Allocate WaveformFront, initialize with name, type=GEN(2)
    // Copy signal data (samples, markers, markers2, playConfig)
    // Copy funName into wf+0x68 (the function description string)
    // Copy args vector into wf+0xF8 (the Values vector at WaveformFront extension)

    auto wf = std::make_shared<WaveformFront>(name, Waveform::File::Type::GEN, dc);
    wf->signal = signal;                   // copy Signal data into +0x80
    wf->setFunDescrName(funName);          // Waveform::funDescrName at +0x50
    wf->genArgs_ = args;                     // vector<Value> at +0xE0

    // Insert
    insertWaveform(wf);
    return wf;
}

// 0x29c210 — WavetableManager<WaveformFront>::getWaveformForFront(funName, args) const
//! \copydoc zhinst::detail::WavetableManager::getWaveformForFront
template<>
std::shared_ptr<WaveformFront>
WavetableManager<WaveformFront>::getWaveformForFront(
    const std::string& funName,
    const std::vector<Value>& args) const
{
    // Iterate all waveforms
    for (auto& wf_ptr : waveforms_) {
        WaveformFront* wf = wf_ptr.get();

        // Skip if file type != GEN (2)
        if (wf->waveformType != Waveform::File::Type::GEN) continue;  // wf+0x18

        // Compare function name (wf+0x50 is a string field "funDescr")
        // Check string length matches, then memcmp content
        if (wf->funDescrName() != funName) continue;

        // Compare args vector size
        // wf+0xE8 - wf+0xE0 must equal args size
        if (wf->genArgs_.size() != args.size()) continue;

        // Check wf+0xDC == 0 (not modified flag)
        if (wf->isModified()) continue;

        // Compare each Value element using Value::operator== (0x21a780).
        bool match = true;
        for (size_t i = 0; i < args.size(); i++) {
            if (!(wf->genArgs_[i] == args[i])) {
                match = false;
                break;
            }
        }
        if (!match) continue;

        return wf_ptr;
    }

    return nullptr;
}

// 0x29c440 — WavetableManager<WaveformFront>::copyWaveform(shared_ptr<WaveformFront>)
//! \copydoc zhinst::detail::WavetableManager::copyWaveform
template<>
std::shared_ptr<WaveformFront>
WavetableManager<WaveformFront>::copyWaveform(
    std::shared_ptr<WaveformFront> src)
{
    // Generate unique name from source waveform's name
    WaveformFront* srcPtr = src.get();
    int baseIdx = numDefs_;
    int counter = numDefs2_;
    numDefs2_ = counter + 1;

    std::string uniqueName = getUniqueName(srcPtr->name, baseIdx, counter);

    // allocate_shared<WaveformFront>(allocator, src, uniqueName)
    // This calls WaveformFront copy constructor with new name
    auto copy = std::allocate_shared<WaveformFront>(
        std::allocator<WaveformFront>{}, src, uniqueName);

    insertWaveform(copy);
    return copy;
}

// 0x29ccf0 — WavetableManager<WaveformFront>::updateWave(shared_ptr<WaveformFront>, const string&)
//! \copydoc zhinst::detail::WavetableManager::updateWave
template<>
void WavetableManager<WaveformFront>::updateWave(
    std::shared_ptr<WaveformFront> wf,
    const std::string& newName)
{
    WaveformFront* ptr = wf.get();

    // Get current name from waveform, find old index in nameToIndex_
    const std::string& oldName = ptr->name;
    size_t oldIdx; // = nameToIndex_[oldName] — insert if not present

    // Find and remove old name mapping
    auto it = nameToIndex_.find(oldName);
    if (it != nameToIndex_.end()) {
        oldIdx = it->second;
        // Remove the node from hash table
        nameToIndex_.erase(it);
    }

    // Update waveform's internal name to newName
    ptr->setName(newName);

    // Re-insert with new name mapping to same index
    nameToIndex_[ptr->name] = oldIdx;
}

} // namespace detail
} // namespace zhinst
