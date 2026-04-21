// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableManager<WaveformFront> method implementations
// ============================================================================

#include "zhinst/wavetable_front.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/signal.hpp"
#include "zhinst/value.hpp"
#include "zhinst/waveform_front.hpp"

namespace zhinst {
namespace detail {

namespace {
// 0x2a0fd0 — getUniqueName(const string& base, int index, int counter)
std::string getUniqueName(const std::string& base, int index, int counter);
} // anon

// 0x29fa40 — WavetableManager<WaveformFront>::~WavetableManager()
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

// 0x29aec0 — WavetableManager<WaveformFront>::newEmptyWaveform(const string&, const DeviceConstants&)
template<>
std::shared_ptr<WaveformFront>
WavetableManager<WaveformFront>::newEmptyWaveform(
    const std::string& name,
    const DeviceConstants& dc)
{
    int baseIdx = lineNr_;  // this+0x00
    int counter = waveformCounter_; // this+0x04
    waveformCounter_ = counter + 1;

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

    auto wf = std::make_shared<WaveformFront>(/* ... */);

    // Insert into vector and name map
    insertWaveform(wf);

    return wf;
}

// 0x29b110 — WavetableManager<WaveformFront>::newWaveformFromFile(name, filename, type, dc)
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

    // Create copy of new waveform shared_ptr, insert into manager
    auto wf = std::make_shared<WaveformFront>(/* ... */);
    insertWaveform(wf);

    return wf;
}

// 0x29b560 — WavetableManager<WaveformFront>::newWaveformFromFile(name, signal, addr, filename, type, dc)
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

    auto wf = std::make_shared<WaveformFront>(/* ... */);

    // If signal data is not the same as wf's internal buffer, copy it
    // signal at rbx: signal.samples = [rbx+0x00, rbx+0x08]
    //               signal.markers  = [rbx+0x18, rbx+0x20]
    //               signal.markers2 = [rbx+0x30, rbx+0x38]
    //               signal.playConfig = rbx+0x48 (16 bytes)

    insertWaveform(wf);
    return wf;
}

// 0x29ba00 — WavetableManager<WaveformFront>::newWaveform(name, signal, funName, args, dc)
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

    auto wf = std::make_shared<WaveformFront>(/* ... */);

    // Insert
    insertWaveform(wf);
    return wf;
}

// 0x29c210 — WavetableManager<WaveformFront>::getWaveformForFront(funName, args) const
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
        if (wf->fileType() != 2) continue;  // wf+0x18

        // Compare function name (wf+0x50 is a string field "funDescr")
        // Check string length matches, then memcmp content
        if (wf->funDescrName() != funName) continue;

        // Compare args vector size
        // wf+0xE8 - wf+0xE0 must equal args size
        if (wf->argsSize() != args.size()) continue;

        // Check wf+0xDC == 0 (not modified flag)
        if (wf->isModified()) continue;

        // Compare each Value element
        bool match = true;
        for (size_t i = 0; i < args.size(); i++) {
            if (!(wf->arg(i) == args[i])) {
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
template<>
std::shared_ptr<WaveformFront>
WavetableManager<WaveformFront>::copyWaveform(
    std::shared_ptr<WaveformFront> src)
{
    // Generate unique name from source waveform's name
    WaveformFront* srcPtr = src.get();
    int baseIdx = lineNr_;
    int counter = waveformCounter_;
    waveformCounter_ = counter + 1;

    std::string uniqueName = getUniqueName(srcPtr->name(), baseIdx, counter);

    // allocate_shared<WaveformFront>(allocator, src, uniqueName)
    // This calls WaveformFront copy constructor with new name
    auto copy = std::allocate_shared<WaveformFront>(
        std::allocator<WaveformFront>{}, *srcPtr, uniqueName);

    insertWaveform(copy);
    return copy;
}

// 0x29ccf0 — WavetableManager<WaveformFront>::updateWave(shared_ptr<WaveformFront>, const string&)
template<>
void WavetableManager<WaveformFront>::updateWave(
    std::shared_ptr<WaveformFront> wf,
    const std::string& newName)
{
    WaveformFront* ptr = wf.get();

    // Get current name from waveform, find old index in nameToIndex_
    const std::string& oldName = ptr->name();
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
    nameToIndex_[ptr->name()] = oldIdx;
}

// 0x2a1200 — WavetableManager<WaveformFront>::insertWaveform(shared_ptr<WaveformFront>)
template<>
void WavetableManager<WaveformFront>::insertWaveform(
    std::shared_ptr<WaveformFront> wf)
{
    // Get current vector size (before insertion) as the index
    size_t idx = waveforms_.size();

    // Push back the shared_ptr into waveforms_
    waveforms_.emplace_back(wf);

    // Insert name -> idx mapping into nameToIndex_
    const std::string& name = wf.get()->name();
    nameToIndex_.emplace(name, idx);
}

} // namespace detail
} // namespace zhinst
