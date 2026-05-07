// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableIR method implementations
// ============================================================================

#include "zhinst/waveform/wavetable_ir.hpp"
#include "zhinst/waveform/wavetable_front.hpp"
#include "zhinst/waveform/wavetable_helpers.hpp"
#include "zhinst/codegen/memory_allocator.hpp"
#include "zhinst/core/callbacks.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/json.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <sstream>
#include <algorithm>
#include <numeric>

namespace zhinst {

// 0x2a0fd0 — anonymous-namespace free function shared with wavetable_front.cpp
// and wavetable_manager_front.cpp; produces "__<base>_<index>_<counter>"-style
// unique names for waveforms. Definition lives in wavetable_helpers.hpp as an
// inline detail-namespace helper (single ODR-clean definition shared by all
// wavetable TUs).
using detail::getUniqueName;

// CsvParser is reconstructed under  (loaders); for now declare just the
// static template entry point used by loadWaveform. Symbol:
//   void CsvParser::csvFileToWaveform<WaveformIR>(shared_ptr<WaveformIR>, AwgDeviceType)
//   at 0x2be830.
class CsvParser {
public:
    template <typename WfT>
    static void csvFileToWaveform(CachedParser& cache, std::shared_ptr<WfT> wf, AwgDeviceType deviceType);
};

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
      waveIndexTracker_(constants.maxWaveIndex(), *front.manager_),
      usedWaveforms_(),
      cancelCallback_(std::move(cancelCb))
{
    // Allocate the manager from the front's internal manager
    auto* frontMgr = front.manager_;  // at front+0x1D0
    // 0x29ce5d-0x29ce81: copy numDefs_ + numDefs2_ (combined 8 bytes
    // at WavetableManager+0..+8) from front's manager into the new manager.
    // The binary uses movsd/movlps to do this as a single 8-byte move.
    manager_.reset(new detail::WavetableManager<WaveformIR>());
    manager_->numDefs_         = frontMgr->numDefs_;
    manager_->numDefs2_ = frontMgr->numDefs2_;

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
      waveIndexTracker_(constants.maxWaveIndex(), mgr),
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
    return (manager_->waveforms_.end() - manager_->waveforms_.begin()) / sizeof(std::shared_ptr<WaveformIR>);
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

    auto it = manager_->nameToIndex_.find(*name);
    if (it == manager_->nameToIndex_.end())
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
// 5. Computes firstWaveformOffset from (totalBytes * 32 + alignment) / alignment
//    where alignment = deviceConstants_->waveformAlignment (at DC+0x14)
// 6. Creates MemoryAllocator-like structure with cache line placeholders
// 7. Calls forEachUsedWaveform again with allocation lambda
// 8. Updates firstWaveformOffset_ and addressBase_
void WavetableIR::allocateWaveforms(bool fifoMode)  // 0x29e340
{
    // Lock cancel callback (0x29e373-0x29e394).
    // Binary inlines the libc++ weak_ptr::lock() ABI:
    //   1. Read ctrl block at this+0xC0 (cancelCallback_'s __cntrl_).
    //   2. If null, skip; else call __shared_weak_count::lock() (0x85b9f0).
    //   3. If lock() returned non-null, copy raw ptr from this+0xB8.
    // Reconstructed equivalent uses the public weak_ptr API.
    std::shared_ptr<CancelCallback> cancelLock = cancelCallback_.lock();

    // Phase 1: load waveforms, compute sizes, assign addresses     // 0x29e398
    // Binary lambda $_0 at 0x2a9900:
    //   1. Checks cancel callback (if non-null, calls virtual method)
    //   2. Checks wf->used (+0x48); if false, throws WavetableException
    //   3. If wf->waveformType == 0: calls loadWaveform(wf)
    //   4. Computes sizeInBlocks from signal (same as $_1 below)
    //   5. Sets wf->elfAlignment_ = dc->waveformAlignment
    //   6. Sets wf->addressValue = totalSize + addressBase_
    //   7. totalSize += allocationByteSize; waveCount++; totalBytes updated
    size_t totalBytes = 0;  // unused but kept for symmetry with binary stack layout
    uint32_t totalSize = 0;
    uint32_t waveCount = 0;
    uint32_t lastAllocBytes = 0;

    forEachUsedWaveform(
        [&](const std::shared_ptr<WaveformIR>& wf) {
            // Cancel callback check (0x2a9917-0x2a992f)
            {
                auto cancelLock2 = cancelCallback_.lock();
                if (cancelLock2) {
                    cancelLock2->isCancelled();
                }
            }

            // Check wf->used (0x2a9939)
            if (!wf->used) {                                          // +0x48
                throw WavetableException("Waveform '" + wf->name + "' is not used");
            }

            // Load waveform if needed (0x2a9984-0x2a99a2)
            if (wf->waveformType == Waveform::File::Type{}) {
                loadWaveform(wf);
            }

            // Compute sizeInBlocks (0x2a99d1-0x2a9a08)
            uint16_t channels = wf->signal.channels_;                 // +0xC8
            uint32_t length = static_cast<uint32_t>(wf->signal.length_); // +0xD0
            const DeviceConstants* dc = wf->deviceConstants;          // +0x78

            uint32_t sizeInBlocks;
            if (length == 0) {
                sizeInBlocks = 0;
            } else {
                uint32_t granularity = dc->maxWaveformLength;       // DC+0x40
                uint32_t pageSize = dc->grainSize;             // DC+0x44
                uint32_t rounded = ((length + pageSize - 1) / pageSize) * pageSize;
                sizeInBlocks = std::max(rounded, granularity);
            }
            uint64_t totalBits = static_cast<uint64_t>(sizeInBlocks) * channels * dc->bitsPerSample;
            uint32_t allocationBytes = static_cast<uint32_t>((totalBits + 7) / 8);
            // Binary @0x2a9928: allocationBytes aligned to 64 bytes
            allocationBytes = (allocationBytes + 63) & ~63u;

            // Binary @0x2a9a6a-0x2a9a96: Align totalSize to dc->waveformAlignment
            // before setting addressValue.
            // Condition for alignment (@0x2a9a35-0x2a9a5d):
            //   - waveCount == 0: always align (first waveform).
            //   - lastAllocBytes > waveformAlignment: align.
            //   - totalSize + allocationBytes > alignedLimit: align.
            //     where alignedLimit = ((totalSize + wfAlign - 1) / wfAlign) * wfAlign.
            //   - Otherwise SKIP alignment (waveform fits in current CL region).
            // Correct alignment is critical because addressValue determines Phase 2
            // CL occupancy, which in turn sets crossesCacheLine_ (controls prf emit).
            uint32_t wfAlign = dc->waveformAlignment;                 // DC+0x14
            uint32_t alignedLimit = ((totalSize + wfAlign - 1) / wfAlign) * wfAlign;
            bool needsAlign = (waveCount == 0)
                           || (lastAllocBytes > wfAlign)
                           || (totalSize + allocationBytes > alignedLimit);
            if (needsAlign) {
                totalSize = alignedLimit;
                // Set elfAlignment_ only when alignment is performed (0x2a9a9d)
                // Binary sets this only on the alignment path, not unconditionally.
                wf->elfAlignment_ = dc->waveformAlignment;           // DC+0x14
            }

            // Set addressValue = totalSize + addressBase_ (0x2a9aa3-0x2a9aa9)
            wf->addressValue = totalSize + addressBase_;

            // Accumulate (0x2a9aac-0x2a9ab5)
            totalSize += allocationBytes;
            waveCount++;
            wf->allocationByteSize = allocationBytes;                 // store computed size
            lastAllocBytes = allocationBytes;

            totalBytes += wf->getAllocationByteSize();
        },
        fifoMode ? WaveOrder::Natural : WaveOrder::ByWaveIndex);       // 0x29e3ec

    // Compute first waveform offset with alignment                // 0x29e3fa
    uint32_t alignment = deviceConstants_->waveformAlignment;      // DC+0x14
    uint32_t computedOffset;
    if (fifoMode) {                                                // 0x29e400
        computedOffset = 0;
    } else {                                                       // 0x29e409
        uint64_t raw = waveCount * 32 + alignment + 0x53;
        computedOffset = static_cast<uint32_t>(raw - (raw % alignment));
    }

    // Phase 2: Build cache-line occupancy vector and allocate     // 0x29e437
    uint32_t memorySizeBytes = deviceConstants_->waveformMemorySize;  // DC+0x0C
    uint32_t maxBlocksPerCL = deviceConstants_->cachePageCount;           // DC+0x18
    uint32_t numCacheLines = memorySizeBytes / alignment;             // 0x29e458

    std::vector<uint32_t> cacheLineUsage;
    if (alignment <= memorySizeBytes) {                                // 0x29e461
        cacheLineUsage.resize(numCacheLines, 0xFFFFFFFF);                 // sentinel // 0x29e472
    }

    // Allocate waveforms within cache-line structure               // 0x29e477
    // Lambda $_1 operator() at 0x2a9c80
    forEachUsedWaveform(
        [&](const std::shared_ptr<WaveformIR>& wf) {
            // 1. Add computed offset to waveform address                // 0x2a9c8b
            wf->addressValue += computedOffset;

            // 2. Compute allocation size in bytes from signal properties // 0x2a9c95
            uint16_t channels = wf->signal.channels_;           // +0xC8
            uint32_t length = static_cast<uint32_t>(wf->signal.length_);     // +0xD0
            const DeviceConstants* dc = wf->deviceConstants;          // +0x78

            uint32_t sizeInBlocks;
            if (length == 0) {                                  // 0x2a9ca8
                sizeInBlocks = 0;
            } else {
                uint32_t granularity = dc->maxWaveformLength; // DC+0x40
                uint32_t pageSize = dc->grainSize;       // DC+0x44
                // Round up to multiple of pageSize, cap at granularity
                uint32_t rounded = ((length + pageSize - 1) / pageSize) * pageSize;
                sizeInBlocks = std::max(rounded, granularity);  // 0x2a9cc6
            }

            // totalBits = sizeInBlocks * channels * bitsPerSample       // 0x2a9cdb
            uint64_t totalBits = static_cast<uint64_t>(sizeInBlocks) * channels * dc->bitsPerSample;
            uint32_t allocationBytes = static_cast<uint32_t>((totalBits + 7) / 8); // 0x2a9ce9

            if (allocationBytes == 0)                           // 0x2a9cf6
                return;

            // 3. Cap to max allocation per cache line                   // 0x2a9d07
            uint32_t maxPerCL = maxBlocksPerCL * alignment;
            if (allocationBytes > maxPerCL)
                allocationBytes = maxPerCL;

            // 4. Check waveform fits within current cache line          // 0x2a9d12
            uint32_t offsetInCL = wf->addressValue % memorySizeBytes;
            if (offsetInCL + allocationBytes > memorySizeBytes) // 0x2a9d1a
                return;  // crosses cache-line boundary

            // 5. Determine block range [startBlock, endBlock)           // 0x2a9d22
            uint32_t startBlock = offsetInCL / alignment;
            uint32_t endBlock = (offsetInCL + allocationBytes - 1) / alignment + 1;

            // 6. Verify all blocks in range are unused (0xFFFFFFFF)     // 0x2a9d60
            bool conflict = false;
            for (uint32_t i = startBlock; i < endBlock; ++i) {
                if (cacheLineUsage[i] != 0xFFFFFFFF) {
                    conflict = true;
                    break;
                }
            }
            if (conflict)                                       // 0x2a9e03
                return;

            // 7. Fill cache-line entries with sequential addresses      // 0x2a9e09
            if (startBlock != endBlock) {
                uint32_t addr = wf->addressValue - (wf->addressValue % alignment);
                for (uint32_t i = startBlock; i < endBlock; ++i) { // 0x2a9e20
                    cacheLineUsage[i] = addr;
                    addr += alignment;
                }
            }

            // 8. Decrement available cache-line count                   // 0x2a9e2f
            uint32_t usedEntries = endBlock - startBlock;
            if (numCacheLines >= usedEntries)
                numCacheLines -= usedEntries;
            else
                numCacheLines = 0;

            // 9. Mark waveform as successfully allocated                // 0x2a9e47
            if (maxPerCL != 0)
                wf->crossesCacheLine_ = true;                    // +0xDA
        },
        WaveOrder::Natural);                                       // 0x29e4b1

    // Update member offsets                                        // 0x29e4d9
    firstWaveformOffset_ += computedOffset;                        // 0x29e4dc
    addressBase_ += computedOffset + totalSize;                    // 0x29e4e2
}

// 0x29e5e0 — WavetableIR::forEachUsedWaveform(function, WaveOrder) const
//
// 1. Gets usedWaveforms_ size, allocates index array [0..N-1]
// 2. If WaveOrder == ByIndex (2): stable_sort by wave index comparison
//    If WaveOrder == ByWaveIndex (1): stable_sort by name comparison
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
        // Binary $_1 at 0x2ae780: reads +0x4c (addressValue), unsigned cmp (jae)
        std::stable_sort(indices, indices + count, [this](size_t a, size_t b) {
            return static_cast<unsigned>(usedWaveforms_[a]->addressValue)
                 < static_cast<unsigned>(usedWaveforms_[b]->addressValue);
        });
    } else if (order == WaveOrder::ByWaveIndex) {
        // Binary $_0 at 0x2ad830: reads +0x6c (waveIndex), signed cmp (jge)
        // Despite the enum name, ByWaveIndex sorts by waveIndex ascending.
        std::stable_sort(indices, indices + count, [this](size_t a, size_t b) {
            return usedWaveforms_[a]->waveIndex < usedWaveforms_[b]->waveIndex;
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
//    e. Set the new waveform's crossesCacheLine_ = true (filler bit)
//    f. Call waveIndexTracker_.assignAuto(autoIndex)
//    g. Set the waveform's waveIndex (at +0x6C offset from waveform)
//    h. Re-scan for next gap
void WavetableIR::assignWaveIndexImplicit()  // 0x29e8a0
{
    // Phase 1: assign implicit wave indices to waveforms with waveIndex == -1.
    // Binary $_0 at 0x2A9F10: for each waveform with waveIndex == -1, finds the
    // next available autoIndex (skipping explicitly assigned indices via lower_bound
    // loop), calls waveIndexTracker_.assignAuto(autoIndex), and sets wf->waveIndex.
    // Waveforms with waveIndex != -1 are skipped immediately (ret at +9).
    auto countFn = [this](const std::shared_ptr<WaveformIR>& wf) {
        // Skip waveforms that already have an explicit index assigned.
        if (wf->waveIndex != -1)
            return;

        // Find next available autoIndex past all used indices.
        auto& tree = waveIndexTracker_.indices_;
        int autoIdx = waveIndexTracker_.autoIndex_;
        while (true) {
            auto it = tree.lower_bound(autoIdx);
            if (it == tree.end()) break;
            if (*it > autoIdx) break;
            autoIdx++;
            waveIndexTracker_.autoIndex_ = autoIdx;
        }

        // Assign the next free index to this waveform.
        waveIndexTracker_.assignAuto(autoIdx);
        wf->waveIndex = autoIdx;
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
        // Find the next explicitly-assigned index at or above autoIdx
        auto it = tree.lower_bound(autoIdx);
        if (it == tree.end()) return;

        // If autoIdx already matches an explicit index, skip past it
        if (*it == autoIdx) {
            autoIdx++;
            waveIndexTracker_.autoIndex_ = autoIdx;
            continue;
        }

        // autoIdx < *it — there's a gap, create a filler
        // Binary at 0x29ea3a sets reserveOnly_=true for filler signals,
        // so they become SHT_NOBITS in the ELF (no actual sample data).
        size_t minSamples = static_cast<size_t>(deviceConstants_->maxWaveformLength);
        Signal fillerSignal(ReserveOnly{}, minSamples, MarkerBitsPerChannel{});
        std::string fillerName = "filler";
        // 0x29ea60-0x29ea79: edx = manager_->numDefs_, ecx = manager_->numDefs2_,
        // then numDefs2_ is post-incremented in place. Result string lives at
        // [rbp-0x58] and is passed to newWaveform as the unique name.
        int lineIdx = manager_->numDefs_;
        int counter = manager_->numDefs2_++;
        std::string uniqueName = getUniqueName(fillerName, lineIdx, counter);
        auto newWf = manager_->newWaveform(
            uniqueName, fillerSignal, std::string(), *deviceConstants_);

        usedWaveforms_.push_back(std::move(newWf));

        // Mark as filler and assign index
        auto& lastWf = usedWaveforms_.back();
        lastWf->used = true;                      // fillers must be marked used
        lastWf->crossesCacheLine_ = true;  // +0xDA — fillers always span a CL boundary
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
void WavetableIR::updateWaveforms(bool fifoMode, bool allocFifoMode)  // 0x29ed10
{
    if (fifoMode) {
        allocateWaveformsForFifo();
    } else {
        allocateWaveforms(allocFifoMode);
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
    uint32_t memorySizeBytes = dc->waveformMemorySize;  // DC+0x0C
    uint32_t alignment = dc->waveformAlignment;              // DC+0x14
    uint32_t maxBlocks = dc->maxBlocks;                      // DC+0x1C

    // Inlined MemoryAllocator construction                   // 0x29ed5a
    // Binary: passes addressBase_ as startOffset (tail region starts at
    // 0x80000000 for HDAWG8).
    MemoryAllocator allocator(dc, /*startOffset=*/addressBase_);

    // Phase 1: allocate waveforms with irBool0 == 1          // 0x29ee0d
    std::set<size_t> allocatedSet;
    try {
        forEachUsedWaveform(
            [&](const std::shared_ptr<WaveformIR>& wf) {
                // 0x2aa715: Skip if nothing to allocate
                if (wf->allocationByteSize == 0)              // +0x74
                    return;
                // 0x2aa723: Only process waveforms flagged irBool0 == 1
                if (!wf->fixed_)                             // +0xD9
                    return;

                // 0x2aa740: Allocate via cache-line-aligned strategy
                MemoryBlock block = allocator.allocateCLAligned(wf->allocationByteSize);

                // 0x2aa781: If allocation failed, throw
                if (!(block.flags & 1)) {
                    throw std::runtime_error("Waveform memory overflow in FIFO allocation");
                }

                // 0x2aa792: Track used cache lines if maxBlocks > 0
                if (maxBlocks > 0) {
                    uint32_t startAddr = block.start;
                    uint32_t endAddr = block.end;
                    if (startAddr < endAddr) {                // 0x2aa7c0
                        uint32_t pos = startAddr;
                        for (size_t i = 0; i < maxBlocks && pos < endAddr; ++i) {
                            uint32_t clIdx = (pos % memorySizeBytes) / alignment;
                            allocatedSet.insert(static_cast<size_t>(clIdx));  // 0x2aa817
                            pos += alignment;
                        }
                    }
                }

                // 0x2aa874: Set waveform address and crossing flag
                wf->addressValue = block.start;               // +0x4C
                wf->crossesCacheLine_ = (block.flags >> 8) & 1;  // +0xDA
            },
            WaveOrder::Natural);                              // 0x29ee40
    } catch (const std::exception& e) {                       // 0x29f070
        throw WavetableException(
            std::string("Waveform memory overflow: ") + e.what());
    }

    // Phase 2: allocate remaining waveforms (irBool0 == 0)   // 0x29ee67
    forEachUsedWaveform(
        [&](const std::shared_ptr<WaveformIR>& wf) {
            // 0x2acfc2: Skip if nothing to allocate
            if (wf->allocationByteSize == 0)                  // +0x74
                return;
            // 0x2acfcd: Only process waveforms with irBool0 == 0
            if (wf->fixed_)                                  // +0xD9
                return;

            // 0x2acfe7: Try cache-line-aligned allocation first
            MemoryBlock block = allocator.allocateCLAligned(wf->allocationByteSize);

            if (block.flags & 1) {                            // 0x2ad012
                // Direct assignment — fits without reloading
                wf->addressValue = block.start;               // +0x4C
                wf->crossesCacheLine_ = (block.flags >> 8) & 1;  // +0xDA
                return;
            }

            // 0x2ad02e: Fall back to reloading allocation
            std::set<size_t> localAllocSet(allocatedSet.begin(), allocatedSet.end());
            MemoryBlock block2 = allocator.allocateReloadingCL(
                wf->allocationByteSize, localAllocSet);       // 0x2ad087

            if (!(block2.flags & 1)) {                        // 0x2ad09f
                throw std::runtime_error("Waveform memory overflow in FIFO reloading");
            }

            wf->addressValue = block2.start;                  // +0x4C
            wf->crossesCacheLine_ = 0;                       // +0xDA — reloaded, no CL crossing
        },
        WaveOrder::Natural);                                  // 0x29ee93

    // Update addressBase_ from last allocation position       // 0x29eef2
    // Binary's allocator stores ALLOCATED blocks in the deque (not free blocks).
    // At 0x29eecf it reads deque.back().end = end of last allocated block.
    // Our reconstruction stores FREE blocks instead, so we find the free block
    // with the highest start address, which equals the end of last allocation.
    if (allocator.hasFreeBlocks()) {
        addressBase_ = allocator.maxFreeBlockStart();
    }
}

// 0x29f310 — WavetableIR::loadWaveform(shared_ptr<WaveformIR>)
//
// Binary layout (verified 0x29f310-0x29f3a8):
// 1. r15 = waveform.get()
// 2. cmp [r15+0x18], 0; jne return  ← skip if waveformType already set
// 3. Signal::checkAllocation() on signal_ at r15+0x80
// 4. cmp [r15+0x80], [r15+0x88]; je load  ← if samples_ vector is empty,
//    enter the load path; otherwise return.
// 5. Load path (0x29f354): increment refcount on the shared_ptr, then call
//    CsvParser::csvFileToWaveform<WaveformIR>(wfCopy, deviceConstants_->deviceType).
//
// Notes:
//  * The binary contains NO try/catch. The previous reconstruction's exception
//    wrapping was speculative and has been removed.
//  * The empty-vs-nonempty branch was inverted in the previous reconstruction
//    (returned on empty); the binary loads on empty.
void WavetableIR::loadWaveform(std::shared_ptr<WaveformIR> waveform)  // 0x29f310
{
    WaveformIR* wf = waveform.get();

    // Skip if already loaded (Waveform::waveformType != 0)
    if (wf->waveformType != Waveform::File::Type{})
        return;

    // Allocate any pending signal storage
    wf->signal.checkAllocation();

    // Only load from CSV when the samples vector is empty.
    // Comparison reads vector<double>::__begin_/__end_ at +0x80/+0x88
    // (signal_ at WaveformIR+0x80, samples_ at Signal+0).
    if (!wf->signal.samples_.empty())
        return;

    // 0x29f354: copy the shared_ptr (lock inc on ctrl-block, +0x8) and
    // forward to the CSV loader. The device type is the first int field
    // of DeviceConstants (DC+0x00).
    auto wfCopy = waveform;  // refcount inc
    auto deviceType = static_cast<AwgDeviceType>(deviceConstants_->deviceType);
    CsvParser::csvFileToWaveform<WaveformIR>(cachedParser_, std::move(wfCopy), deviceType);
}

// 0x29f480 — WavetableIR::getJsonIndex(SampleFormat) const
//
// Builds a JSON index string for all used waveforms:
// 1. Creates a ptree, iterates waveforms via forEachUsedWaveform (WaveOrder::ByWaveIndex)
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
            // Skip if not used, has no allocation, or has zero raw sample
            // length. The length-0 check (IF-172/176) catches both
            // `zeros(0)` and `cut(w, N, N)` (which orig collapses to a
            // 0-sample wave per IF-176). Note that allocationByteSize is
            // non-zero for these because alignWaveformSizes pads to
            // minLengthSamples, so the raw signal.length() check is needed.
            if (!waveform->isUsed() || waveform->allocationByteSize == 0
                || waveform->signal.length() == 0)
                return;

            // 0x2c5440 — WaveformIR::toJsonElement(SampleFormat)
            ptree element = waveform->toJsonElement(format);

            // Append as array element (empty key = JSON array entry)
            waveformsPtree.push_back(std::make_pair("", element));
        },
        WaveOrder::ByWaveIndex);

    // Outer ptree wrapping the waveforms list
    ptree root;
    root.add_child("waveforms", waveformsPtree);

    // Serialize to JSON string
    std::ostringstream oss;
    boost::property_tree::json_parser::write_json(oss, root, false);
    return oss.str();
}

// ============================================================================
// WavetableIR::alignWaveformSizes() @0x29f150
//
// Iterates used waveforms and rounds each sample count up to the nearest
// multiple of deviceConstants_->grainSize, clamping to minLengthSamples.
// ============================================================================
void WavetableIR::alignWaveformSizes() {                            // @0x29f150
    forEachUsedWaveform(
        [this](const std::shared_ptr<WaveformIR>& wf) {
            size_t sampleCount = wf->signal.length();
            if (sampleCount == 0) return;

            uint32_t granularity = deviceConstants_->grainSize;  // DC+0x44
            // ceil(sampleCount / granularity) * granularity
            size_t aligned = ((sampleCount + granularity - 1) / granularity)
                             * granularity;
            // Clamp: binary uses cmova → max(aligned, minLengthSamples)  @0x2aa373-0x2aa375
            size_t maxSamples = static_cast<size_t>(wf->minLengthSamples);  // +0x70
            if (maxSamples > aligned) aligned = maxSamples;

            if (aligned != sampleCount) {
                wf->signal.resizeSamples(aligned);
            }
        },
        WaveOrder::Natural);
}

// ============================================================================
// WavetableIR::assignWaveformAllocationSizes() @0x29f1d0
//
// Iterates used waveforms, loads any unloaded placeholders, then computes
// and assigns the per-waveform allocation byte size (aligned to 64 bytes).
// ============================================================================
void WavetableIR::assignWaveformAllocationSizes() {                 // @0x29f1d0
    auto cancelLock = cancelCallback_.lock();

    forEachUsedWaveform(
        [this, cancelLock](const std::shared_ptr<WaveformIR>& wf) {
            // Check cancellation
            if (cancelLock && cancelLock->isCancelled()) return;

            // Ensure waveform data is loaded
            if (wf->signal.data().empty()) {
                loadWaveform(wf);
            }

            uint16_t channelCount = wf->signal.channels();             // +0xC8
            size_t sampleCount = wf->signal.length();                  // +0xD0

            // Mirror Phase 1 of allocateWaveforms: a length==0 signal
            // contributes 0 bytes (zeros(0) / cut(w,N,N) collapse).  Without
            // this guard the max(aligned, maxWaveformLength) below would
            // inflate the allocation to a full waveform block, which both
            // pollutes the fpgaMemoryUsed budget (IF-189) and shifts the
            // wave-table layout used by the prefetch pass (IF-190).
            uint32_t granularity = deviceConstants_->grainSize;
            size_t aligned;
            if (sampleCount == 0) {
                aligned = 0;
            } else {
                aligned = ((sampleCount + granularity - 1) / granularity)
                          * granularity;
                size_t maxSamples = deviceConstants_->maxWaveformLength;
                // Binary uses cmova (max), NOT min — verified GDB @0x2aa4b3:
                // cmp %eax,%r8d; cmova %r8d,%eax → eax = max(aligned, granularity)
                if (maxSamples > aligned) aligned = maxSamples;
            }

            // Compute allocation: aligned_samples * channels * bitsPerSample
            uint32_t bps = deviceConstants_->bitsPerSample;
            size_t bits = aligned * channelCount * bps;
            size_t bytes = (bits + 7) / 8;

            // Align to 64-byte boundary
            int alloc = static_cast<int>((bytes + 63) & ~static_cast<size_t>(63));
            wf->allocationByteSize = alloc;                            // +0x74
        },
        WaveOrder::Natural);
}

} // namespace zhinst
