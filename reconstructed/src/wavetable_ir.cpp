// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableIR method implementations
// ============================================================================

#include "zhinst/wavetable_ir.hpp"
#include "zhinst/wavetable_front.hpp"
#include "zhinst/memory_allocator.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <sstream>

namespace zhinst {

// 0x29ce20 — WavetableIR::WavetableIR(const WavetableFront&, ...)
//
// Initializes from a WavetableFront:
// 1. Stores deviceConstants ptr, address (both fields)
// 2. Constructs CachedParser at +0x10 from (wavetableSize, path)
// 3. Creates a new WavetableManager<WaveformIR> (size 0x48) initialized from
//    front's internal WavetableManager (at front+0x1D0)
// 4. Constructs WaveIndexTracker at +0x78 from (deviceConstants->maxWaveIndex, manager)
// 5. Zeroes usedWaveforms_ vector at +0xA0
// 6. Copies cancelCallback_ weak_ptr at +0xB8
// 7. Iterates front.manager->waveforms, creates WaveformIR from each WaveformFront,
//    and inserts into this->manager_
WavetableIR::WavetableIR(const WavetableFront& front,
                          const DeviceConstants& constants,
                          detail::AddressImpl<uint32_t> address,
                          size_t wavetableSize,
                          const boost::filesystem::path& path,
                          std::weak_ptr<CancelCallback> cancelCb)  // 0x29ce20
    : deviceConstants_(&constants),
      addressBase_(address.value),
      firstWaveformOffset_(address.value),
      cachedParser_(wavetableSize, path),
      manager_(nullptr),
      waveIndexTracker_(constants.maxWaveIndex, *front.manager_),
      usedWaveforms_(),
      cancelCallback_(std::move(cancelCb))
{
    // Allocate the manager from the front's internal manager
    auto* frontMgr = front.manager_;  // at front+0x1D0
    manager_.reset(new detail::WavetableManager<WaveformIR>(frontMgr->samplesPerWave));

    // Copy waveforms from front, converting WaveformFront -> WaveformIR
    auto* begin = frontMgr->waveforms_.data();
    auto* end = frontMgr->waveforms_.data() + frontMgr->waveforms_.size();
    for (auto* it = begin; it != end; ++it) {
        auto wfIR = std::make_shared<WaveformIR>(*it);
        manager_->insertWaveform(std::move(wfIR));
    }
}

// 0x29d230 — WavetableIR::WavetableIR(const WavetableManager<WaveformIR>&, ...)
//
// Initializes from an existing WavetableManager<WaveformIR>:
// 1. Stores deviceConstants, address fields
// 2. Constructs CachedParser
// 3. Copies the WavetableManager (via make_unique copy ctor)
// 4. Constructs WaveIndexTracker from (deviceConstants->maxWaveIndex, mgr)
// 5. Zeroes usedWaveforms_, copies cancelCallback_
WavetableIR::WavetableIR(const detail::WavetableManager<WaveformIR>& mgr,
                          const DeviceConstants& constants,
                          detail::AddressImpl<uint32_t> address,
                          size_t wavetableSize,
                          const boost::filesystem::path& path,
                          std::weak_ptr<CancelCallback> cancelCb)  // 0x29d230
    : deviceConstants_(&constants),
      addressBase_(address.value),
      firstWaveformOffset_(address.value),
      cachedParser_(wavetableSize, path),
      manager_(std::make_unique<detail::WavetableManager<WaveformIR>>(mgr)),
      waveIndexTracker_(constants.maxWaveIndex, mgr),
      usedWaveforms_(),
      cancelCallback_(std::move(cancelCb))
{
}

// 0x29d550 — WavetableIR::~WavetableIR()
//
// Destruction order:
// 1. Release cancelCallback_ weak_ptr (+0xC0 control block)
// 2. Destroy usedWaveforms_ vector (release all shared_ptrs, free buffer)
// 3. Destroy waveIndexTracker_.indices_ set (tree destroy at +0x80)
// 4. Destroy manager_ unique_ptr (calls WavetableManager dtor + free)
// 5. Destroy cachedParser_ (SSO strings at +0x40, +0x58, then map tree at +0x10)
WavetableIR::~WavetableIR()  // 0x29d550
{
    // Compiler-generated destruction in reverse order of members
}

// 0x29d670 — WavetableIR::toJson() const
//
// Delegates to manager_->toJson(), then wraps it in a JSON object:
//   { "wavetable_manager": <manager_->toJson()> }
// Uses boost::json initializer_list construction.
boost::json::value WavetableIR::toJson() const  // 0x29d670
{
    boost::json::value managerJson = manager_->toJson();
    return boost::json::value{
        {"wavetable_manager", std::move(managerJson)}
    };
}

// 0x29db10 — WavetableIR::fromJson(value, DeviceConstants, address, size, path, cancelCb)
//
// 1. Extracts json.as_object().at("wavetable_manager")
// 2. Calls WavetableManager<WaveformIR>::fromJson on that sub-value
// 3. Allocates shared_ptr_emplace<WavetableIR> (0xE0 bytes control block)
// 4. Constructs WavetableIR in-place from the deserialized manager
// 5. Returns shared_ptr<WavetableIR>
std::shared_ptr<WavetableIR> WavetableIR::fromJson(
    boost::json::value json,
    const DeviceConstants& constants,
    detail::AddressImpl<uint32_t> address,
    size_t wavetableSize,
    const boost::filesystem::path& path,
    std::weak_ptr<CancelCallback> cancelCb)  // 0x29db10
{
    const auto& obj = json.as_object();
    const auto& managerJson = obj.at("wavetable_manager");

    auto mgr = detail::WavetableManager<WaveformIR>::fromJson(managerJson, constants);

    return std::make_shared<WavetableIR>(mgr, constants, address, wavetableSize, path, cancelCb);
}

// 0x29e090 — WavetableIR::operator==(const WavetableIR&) const
//
// Compares:
// 1. *manager_ == *other.manager_ (calls WavetableManager::operator==)
// 2. addressBase_ == other.addressBase_
// 3. firstWaveformOffset_ == other.firstWaveformOffset_
// 4. *manager_ == *other.manager_ (second call — tail call optimization)
//
// Actually from disasm: first calls manager==, then checks address fields,
// then tail-calls manager== again (checking both stored & live).
bool WavetableIR::operator==(const WavetableIR& other) const  // 0x29e090
{
    if (!(*manager_ == *other.manager_))
        return false;
    if (addressBase_ != other.addressBase_)
        return false;
    if (firstWaveformOffset_ != other.firstWaveformOffset_)
        return false;
    return (*manager_ == *other.manager_);
}

// 0x29e270 — WavetableIR::begin() const
//
// Returns manager_->waveforms_.data() (vector begin ptr)
const std::shared_ptr<WaveformIR>* WavetableIR::begin() const  // 0x29e270
{
    return manager_->waveforms_.data();
}

// 0x29e280 — WavetableIR::end() const
const std::shared_ptr<WaveformIR>* WavetableIR::end() const  // 0x29e280
{
    return manager_->waveforms_.data() + manager_->waveforms_.size();
}

// 0x29e290 — WavetableIR::size() const
size_t WavetableIR::size() const  // 0x29e290
{
    return (manager_->waveforms_.end_ - manager_->waveforms_.begin_) / sizeof(std::shared_ptr<WaveformIR>);
}

// 0x29e2b0 — WavetableIR::getWaveformByName(const optional<string>&) const
//
// If name has no value, returns empty shared_ptr.
// Otherwise looks up name in manager_->nameIndex_ (unordered_map<string,size_t>).
// If found, returns manager_->waveforms_[index] (with bounds check).
// If not found, returns empty shared_ptr.
std::shared_ptr<WaveformIR> WavetableIR::getWaveformByName(
    const std::optional<std::string>& name) const  // 0x29e2b0
{
    if (!name.has_value())
        return nullptr;

    auto it = manager_->nameIndex_.find(*name);
    if (it == manager_->nameIndex_.end())
        return nullptr;

    size_t index = it->second;
    return manager_->waveforms_.at(index);
}

// 0x29e320 — WavetableIR::getNextSegmentAddress() const
uint32_t WavetableIR::getNextSegmentAddress() const  // 0x29e320
{
    return addressBase_;
}

// 0x29e330 — WavetableIR::getFirstWaveformOffset() const
uint32_t WavetableIR::getFirstWaveformOffset() const  // 0x29e330
{
    return firstWaveformOffset_;
}

// 0x29e340 — WavetableIR::allocateWaveforms(bool fifoMode)
//
// Main allocation logic:
// 1. Locks cancelCallback_ weak_ptr
// 2. Creates a lambda that counts total samples and tracks waveform count
// 3. Calls forEachUsedWaveform with that lambda (WaveOrder::Natural after !fifoMode xor)
// 4. Destroys the lambda
// 5. Computes firstWaveformOffset from (totalSamples * 32 + alignment) / alignment
//    where alignment = deviceConstants_->waveformAlignment (at DC+0x14)
// 6. Creates MemoryAllocator-like structure with cache line placeholders
// 7. Calls forEachUsedWaveform again with allocation lambda
// 8. Updates firstWaveformOffset_ and addressBase_
void WavetableIR::allocateWaveforms(bool fifoMode)  // 0x29e340
{
    // Lock cancel callback
    std::shared_ptr<CancelCallback> cancelLock;                   // 0x29e373
    if (cancelCallback_.controlBlock()) {
        cancelLock = cancelCallback_.lock();
    }

    // Phase 1: count total samples and total allocation size      // 0x29e398
    size_t totalSamples = 0;
    uint32_t totalSize = 0;
    uint32_t waveCount = 0;

    forEachUsedWaveform(
        [&](const std::shared_ptr<WaveformIR>& wf) {
            totalSamples += wf->getSampleCount();
            totalSize += wf->allocationByteSize;
            waveCount++;
        },
        fifoMode ? WaveOrder::Natural : WaveOrder::ByName);       // 0x29e3ec

    // Compute first waveform offset with alignment                // 0x29e3fa
    uint32_t alignment = deviceConstants_->waveformAlignment;      // DC+0x14
    uint32_t computedOffset;
    if (fifoMode) {                                                // 0x29e400
        computedOffset = 0;
    } else {                                                       // 0x29e409
        uint64_t raw = totalSamples * 32 + alignment + 0x53;
        computedOffset = (uint32_t)(raw - (raw % alignment));
    }

    // Phase 2: Build cache-line occupancy vector and allocate     // 0x29e437
    uint32_t memorySizeInSamples = deviceConstants_->waveformMemorySize;  // DC+0x0C
    uint32_t maxBlocksPerCL = deviceConstants_->cachePageCount;           // DC+0x18
    uint32_t numCacheLines = memorySizeInSamples / alignment;             // 0x29e458

    std::vector<uint32_t> cacheLineUsage;
    if (alignment <= memorySizeInSamples) {                                // 0x29e461
        cacheLineUsage.resize(numCacheLines, 0xFFFFFFFF);                 // sentinel // 0x29e472
    }

    // Allocate waveforms within cache-line structure               // 0x29e477
    // APPROXIMATE: allocation lambda assigns addresses using
    // the cache-line usage vector (similar to MemoryAllocator::allocateCLAligned)
    forEachUsedWaveform(
        [&](const std::shared_ptr<WaveformIR>& wf) {
            // Allocate waveform position within cache-line structure
            // using computedOffset and cacheLineUsage vector
        },
        WaveOrder::Natural);                                       // 0x29e4b1

    // Update member offsets                                        // 0x29e4d9
    firstWaveformOffset_ += computedOffset;                        // 0x29e4dc
    addressBase_ += computedOffset + totalSize;                    // 0x29e4e2
}

    // Phase 1: count total samples and total allocation size  // 0x29e398
    // Lambda $_0 captures: &cancelLock, &totalSize, this, &totalSamples, &waveCount
    forEachUsedWaveform(
        [&](const std::shared_ptr<WaveformIR>& wf) {
            // Accumulates totalSamples, totalSize, waveCount from each waveform
            totalSamples += wf->getSampleCount();
            totalSize += wf->allocationByteSize;
            waveCount++;
        },
        fifoMode ? WaveOrder::Natural : WaveOrder::ByName);  // 0x29e3ec

    // Compute firstWaveformOffset with alignment             // 0x29e3fa
    uint32_t alignment = deviceConstants_->waveformAlignment; // DC+0x14
    uint32_t computedOffset;
    if (fifoMode) {                                           // 0x29e400
        computedOffset = 0;
    } else {                                                  // 0x29e409
        uint64_t raw = totalSamples * 32 + alignment + 0x53;
        computedOffset = (uint32_t)(raw - (raw % alignment));
    }

    // Phase 2: Build cache-line occupancy vector and allocate // 0x29e437
    uint32_t memorySizeInSamples = deviceConstants_->waveformMemorySize;  // DC+0x0C
    uint32_t maxBlocksPerCL = deviceConstants_->cachePageCount;           // DC+0x18
    uint32_t numCacheLines = memorySizeInSamples / alignment;             // 0x29e458
    std::vector<uint32_t> cacheLineUsage;
    if (alignment <= memorySizeInSamples) {                                // 0x29e461
        cacheLineUsage.resize(numCacheLines, unusedCacheLine);            // 0x29e472
    }

    // Lambda $_1 captures: &computedOffset, &stackAllocatorStruct        // 0x29e477
    // Calls allocate on each waveform, assigning addresses/offsets
    forEachUsedWaveform(
        [&](const std::shared_ptr<WaveformIR>& wf) {
            // Allocate waveform within the cache-line structure
            // using computedOffset and the local allocator state
            // (memorySizeInSamples, alignment, maxBlocksPerCL,
            //  cacheLineUsage, numCacheLines)
        },
        WaveOrder::Natural);                                  // 0x29e4b1

    // Update member offsets                                   // 0x29e4d9
    firstWaveformOffset_ += computedOffset;                   // 0x29e4dc
    addressBase_ += computedOffset + totalSize;               // 0x29e4e2
}

// 0x29e5e0 — WavetableIR::forEachUsedWaveform(function, WaveOrder) const
//
// 1. Gets usedWaveforms_ size, allocates index array [0..N-1]
// 2. If WaveOrder == ByIndex (2): stable_sort by wave index comparison
//    If WaveOrder == ByName (1): stable_sort by name comparison
// 3. Iterates sorted indices, for each calls callback(usedWaveforms_[index])
// 4. Frees index array
void WavetableIR::forEachUsedWaveform(
    std::function<void(const std::shared_ptr<WaveformIR>&)> callback,
    WaveOrder order) const  // 0x29e5e0
{
    size_t count = (usedWaveforms_.end() - usedWaveforms_.begin());
    if (count == 0) return;

    // Allocate index array
    size_t* indices = new size_t[count];
    std::iota(indices, indices + count, 0);

    // Sort based on order
    if (order == WaveOrder::ByIndex) {
        // stable_sort by wave index (comparator uses WavetableIR* context)
        std::stable_sort(indices, indices + count, [this](size_t a, size_t b) {
            return usedWaveforms_[a]->waveIndex < usedWaveforms_[b]->waveIndex;
        });
    } else if (order == WaveOrder::ByName) {
        // stable_sort by name
        std::stable_sort(indices, indices + count, [this](size_t a, size_t b) {
            return usedWaveforms_[a]->name < usedWaveforms_[b]->name;
        });
    }

    // Call callback in sorted order
    for (size_t i = 0; i < count; ++i) {
        size_t idx = indices[i];
        callback(usedWaveforms_[idx]);
    }

    delete[] indices;
}

// 0x29e8a0 — WavetableIR::assignWaveIndexImplicit()
//
// 1. Calls forEachUsedWaveform with a lambda that increments autoIndex
//    (WaveOrder::Natural)
// 2. Finds next available auto index in the set (skipping used ones)
// 3. While there are unused indices in the set:
//    a. Finds the tree successor (next used index)
//    b. If autoIndex >= that index, skip past it
//    c. Otherwise: create a new "filler" waveform via manager_->newWaveform(...)
//    d. Push the new waveform into usedWaveforms_
//    e. Set the new waveform's irBool1 = true
//    f. Call waveIndexTracker_.assignAuto(autoIndex)
//    g. Set the waveform's waveIndex (at +0x6C offset from waveform)
//    h. Re-scan for next gap
void WavetableIR::assignWaveIndexImplicit()  // 0x29e8a0
{
    // Phase 1: iterate used waveforms to find max index
    auto countFn = [this](const std::shared_ptr<WaveformIR>& wf) {
        // no-op counting lambda
    };
    forEachUsedWaveform(countFn, WaveOrder::Natural);

    // Find next auto index past all used ones
    auto& tree = waveIndexTracker_.indices_;
    int autoIdx = waveIndexTracker_.autoIndex_;

    // Skip past existing entries
    while (true) {
        auto it = tree.lower_bound(autoIdx);
        if (it == tree.end()) break;
        if (*it > autoIdx) break;
        autoIdx++;
        waveIndexTracker_.autoIndex_ = autoIdx;
    }

    // Check if there's a gap to fill
    if (tree.empty()) return;

    // Fill gaps with filler waveforms
    while (true) {
        // Find next entry after current position
        auto it = tree.end();
        // ... walk tree to find successor
        // If no successor or autoIdx >= successor, done
        if (autoIdx >= *it) return;

        // Create filler waveform
        Signal emptySignal;
        std::string fillerName = "filler";
        auto newWf = manager_->newWaveform(
            getUniqueName(fillerName, manager_->numDefs(), manager_->numDefs() + 1),
            emptySignal, fillerName, *deviceConstants_);

        usedWaveforms_.push_back(std::move(newWf));

        // Mark as filler and assign index
        auto& lastWf = usedWaveforms_.back();
        lastWf->irBool1 = true;  // +0x48 relative to WaveformIR extension
        waveIndexTracker_.assignAuto(autoIdx);
        lastWf->waveIndex = autoIdx;  // at offset 0x6C in Waveform

        // Advance autoIndex
        autoIdx++;
        waveIndexTracker_.autoIndex_ = autoIdx;

        // Skip past used entries again
        while (true) {
            auto it2 = tree.lower_bound(autoIdx);
            if (it2 == tree.end()) return;
            if (*it2 > autoIdx) break;
            autoIdx++;
            waveIndexTracker_.autoIndex_ = autoIdx;
        }
    }
}

// 0x29ece0 — WavetableIR::setUsedWaveforms(const vector<shared_ptr<WaveformIR>>&)
//
// Assigns the source vector to usedWaveforms_ (at +0xA0).
// Simple vector assign with size_t element count.
void WavetableIR::setUsedWaveforms(
    const std::vector<std::shared_ptr<WaveformIR>>& waveforms)  // 0x29ece0
{
    if (&usedWaveforms_ == &waveforms) return;
    usedWaveforms_.assign(waveforms.begin(), waveforms.end());
}

// 0x29ed10 — WavetableIR::updateWaveforms(bool fifoMode, bool allocFlag)
//
// If fifoMode: calls allocateWaveformsForFifo()
// Else: calls allocateWaveforms(allocFlag)
void WavetableIR::updateWaveforms(bool fifoMode, bool allocFlag)  // 0x29ed10
{
    if (fifoMode) {
        allocateWaveformsForFifo();
    } else {
        allocateWaveforms(allocFlag);
    }
}

// 0x29ed30 — WavetableIR::allocateWaveformsForFifo()
//
// FIFO allocation strategy:
// 1. Creates MemoryAllocator (inlined) from deviceConstants_
// 2. Calls forEachUsedWaveform (Natural order) with lambda$_0 that allocates
//    each waveform into FIFO memory via the allocator
// 3. Calls forEachUsedWaveform (Natural order) with lambda$_1 to finalize
//    waveform addresses/offsets
// 4. Reads back the last deque entry to update addressBase_
// 5. On memory overflow exception: wraps in WavetableException
void WavetableIR::allocateWaveformsForFifo()  // 0x29ed30
{
    const DeviceConstants* dc = deviceConstants_;
    uint32_t memorySizeInSamples = dc->waveformMemorySize;  // DC+0x0C
    uint32_t alignment = dc->waveformAlignment;              // DC+0x14
    uint32_t maxBlocks = dc->maxBlocks;                      // DC+0x1C

    // Inlined MemoryAllocator construction                   // 0x29ed5a
    uint32_t numCacheLines = memorySizeInSamples / alignment;
    std::vector<uint32_t> cacheLineUsage;
    if (alignment <= memorySizeInSamples) {
        cacheLineUsage.resize(numCacheLines, 0xFFFFFFFF);    // sentinel fill
    }
    uint32_t lastAllocEnd = 0xFFFFFFFF;                       // sentinel

    // Phase 1: allocate each waveform via CL-aligned allocation // 0x29ee0d
    std::set<size_t> allocatedSet;
    try {
        forEachUsedWaveform(
            [&](const std::shared_ptr<WaveformIR>& wf) {
                // allocateCLAligned with fallback to allocateReloadingCL
                // Sets wf->addressValue (+0x4C) from allocation result
                // Sets wf->irBool1 (+0xDA) = crossesCacheLine flag
                // Tracks allocation in allocatedSet
            },
            WaveOrder::Natural);                              // 0x29ee40
    } catch (const std::exception& e) {                       // 0x29f070
        throw WavetableException(
            std::string("Waveform memory overflow: ") + e.what());
    }

    // Phase 2: finalize offsets                               // 0x29ee67
    forEachUsedWaveform(
        [&](const std::shared_ptr<WaveformIR>& wf) {
            // Second pass to finalize waveform offsets
        },
        WaveOrder::Natural);                                  // 0x29ee93

    // Update addressBase_ from last allocation position       // 0x29eef2
    // Reads final position from deque page (division by 341 for page indexing)
    // addressBase_ = finalPosition;
}

// 0x29f310 — WavetableIR::loadWaveform(shared_ptr<WaveformIR>)
//
// 1. If waveform->fileType != 0 (has file data already), return early
// 2. Calls signal.checkAllocation() on waveform's signal (+0x80)
// 3. If signal data is empty (begin == end), loads from CSV:
//    - Copies the shared_ptr (incrementing refcount)
//    - Calls CsvParser::csvFileToWaveform<WaveformIR>(shared_ptr, deviceType)
//      where deviceType comes from deviceConstants_->deviceType (at DC+0x00)
// 4. On exception: wraps and rethrows as WavetableException
void WavetableIR::loadWaveform(std::shared_ptr<WaveformIR> waveform)  // 0x29f310
{
    WaveformIR* wf = waveform.get();

    // Check if already loaded (fileType at +0x18 in Waveform)
    if (wf->fileType != 0)
        return;

    // Check signal allocation
    wf->signal.checkAllocation();

    // If signal has no data, load from CSV
    if (wf->signal.data.begin() == wf->signal.data.end())
        return;

    try {
        auto wfCopy = waveform;  // increment refcount
        int deviceType = deviceConstants_->deviceType;  // DC+0x00
        CsvParser::csvFileToWaveform<WaveformIR>(std::move(wfCopy),
                                                  static_cast<AwgDeviceType>(deviceType));
    } catch (const std::exception& e) {
        const char* msg = e.what();
        if (!msg || !*msg) msg = "(unknown error)";
        throw WavetableException(std::string(msg));
    }
}

// 0x29f480 — WavetableIR::getJsonIndex(SampleFormat) const
//
// Builds a JSON index string for all used waveforms:
// 1. Creates a ptree, iterates waveforms via forEachUsedWaveform (WaveOrder::ByName)
// 2. Lambda calls WaveformIR::toJsonElement(format) for each used waveform
//    and appends the result as a child with empty key ("") to a list ptree
// 3. Wraps the waveform list under "waveforms." in an outer ptree
// 4. Serializes to JSON string via boost::property_tree::json_parser::write_json_internal
// 5. Returns the JSON string
std::string WavetableIR::getJsonIndex(SampleFormat format) const  // 0x29f480
{
    using boost::property_tree::ptree;

    // Waveform entries ptree (will become the JSON array under "waveforms")
    ptree waveformsPtree;

    // Lambda captures: &waveformsPtree, &format
    // 0x2af750 — lambda operator()
    forEachUsedWaveform(
        [&waveformsPtree, &format](const std::shared_ptr<WaveformIR>& waveform) {
            // Skip if not used or has no allocation
            if (!waveform->isUsed() || waveform->allocationByteSize == 0)
                return;

            // 0x2c5440 — WaveformIR::toJsonElement(SampleFormat)
            ptree element = waveform->toJsonElement(format);

            // Append as array element (empty key = JSON array entry)
            waveformsPtree.push_back(std::make_pair("", element));
        },
        WaveOrder::ByName);

    // Outer ptree wrapping the waveforms list
    ptree root;
    root.add_child("waveforms", waveformsPtree);

    // Serialize to JSON string
    std::ostringstream oss;
    boost::property_tree::json_parser::write_json(oss, root, false);
    return oss.str();
}

} // namespace zhinst
