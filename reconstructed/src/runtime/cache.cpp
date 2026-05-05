// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Cache class implementation
// ============================================================================

#include "zhinst/runtime/cache.hpp"
#include "zhinst/waveform/waveform_ir.hpp"
#include "zhinst/core/error_messages.hpp"

#include <iostream>
#include <sstream>

namespace zhinst {

extern ErrorMessages errMsg;  // at 0x95de60

// ============================================================================
// CacheException
// ============================================================================

// 0x2835a0
CacheException::CacheException(std::string const& msg)
    : message_(msg)
{
}

// 0x283600
CacheException::~CacheException() = default;

// 0x284200
const char* CacheException::what() const noexcept {
    if (message_.empty()) {
        return "";  // returns static empty string at 0x904ef9
    }
    return message_.c_str();
}

// ============================================================================
// Cache
// ============================================================================

// 0x282920
Cache::Cache(detail::AddressImpl<uint32_t> size, int pageSize, bool isHirzel)
    : size_(size)
    , pageSize_(pageSize)
    , isHirzel_(isHirzel)
    , pointers_()
{
}

// 0x282940
detail::AddressImpl<uint32_t> Cache::getSize() const {
    return size_;
}

// 0x282950
int Cache::getPageSize() const {
    return pageSize_;
}

// 0x282960
uint32_t Cache::getFreeMemory() const {
    uint32_t free = size_;
    for (auto const& p : pointers_) {
        if (p->state_ == PointerState::Free) {
            continue;
        }
        free -= p->size_;
        if (free > size_) {  // unsigned underflow check
            return 0;
        }
    }
    return free;
}

// 0x2829a0
std::shared_ptr<Cache> Cache::getScope() const {
    auto scope = std::make_shared<Cache>(size_, pageSize_, isHirzel_);
    // Note: in the binary this is done via __shared_ptr_emplace<Cache> + manual copy
    // The new cache copies only size_+pageSize_ (as 8 bytes), isHirzel_, and the pointers vector
    scope->pointers_ = pointers_;
    return scope;
}

// 0x282a30
std::shared_ptr<Cache::Pointer> Cache::allocate(
    std::shared_ptr<WaveformIR> waveform,
    detail::AddressImpl<uint32_t> numBytes,
    std::unordered_map<std::string, bool> const& nameMap,
    int maxBranches,
    bool split)
{
    // Compute free memory in pages
    uint32_t freeMemory = size_;
    for (auto const& p : pointers_) {
        if (p->state_ == PointerState::Free) continue;
        freeMemory -= p->size_;
        if (freeMemory > size_) { freeMemory = 0; break; }
    }
    uint32_t freePages = freeMemory / maxBranches;

    std::shared_ptr<Pointer> result;

    if (split || numBytes < freePages) {
        // Case: split requested, or whole waveform fits in free memory
        // — allocate the full numBytes as Normal (no splitting needed)
        result = allocate(waveform, numBytes, nameMap, CacheType::Normal);
    } else {
        // Case: NOT split AND waveform exceeds free pages
        // — use double-buffering: compute chunk count, allocate partial as Aligned
        // Splitting heuristic (see unknowns.md #63):
        //   numAllocs = max(numBytes/freePages + 1, numBytes/(size/2))
        //   chunkSize = numBytes / numAllocs
        // The intent is double-buffering: the waveform is larger than cache,
        // so we allocate a chunk that fits, set up hash_ for address wrapping,
        // and numRepeats_ for the sequencer to know how many chunks to stream.
        uint32_t numAllocs = numBytes / freePages;
        numAllocs++;
        uint32_t halfSize = size_ >> 1;
        uint32_t altAllocs = numBytes / halfSize;
        if (numAllocs <= altAllocs) {
            numAllocs = altAllocs;
        }
        uint32_t chunkSize = numBytes / numAllocs;

        // Allocate first chunk as Aligned
        result = allocate(waveform, chunkSize, nameMap, CacheType::Aligned);

        // Compute hash and numRepeats for the pointer
        Pointer* ptr = result.get();
        uint32_t halfSz = ptr->size_ / 2;
        ptr->hash_ = ~(ptr->position_ ^ (ptr->position_ + halfSz));
        ptr->numRepeats_ = numBytes / halfSz + 1;
    }

    return result;
}

// 0x282be0
std::shared_ptr<Cache::Pointer> Cache::allocate(
    std::shared_ptr<WaveformIR> waveform,
    detail::AddressImpl<uint32_t> numBytes,
    std::unordered_map<std::string, bool> const& nameMap,
    CacheType cacheType)
{
    // Compute aligned size
    uint32_t pageSize = pageSize_;
    if (cacheType == CacheType::Aligned) {
        pageSize = pageSize_ * 2;
    }
    uint32_t remainder = numBytes % pageSize;
    uint32_t alignedSize;
    if (remainder == 0) {
        alignedSize = numBytes;
    } else {
        alignedSize = numBytes + pageSize - remainder;
    }

    // Get best position (allocates a Pointer internally)
    auto result = getBestPosition(alignedSize, nameMap, false);

    // Set state to Ready and assign waveform
    Pointer* ptr = result.get();
    ptr->state_ = PointerState::Ready;
    ptr->waveform_ = waveform;

    // Insert into sorted position in pointers_ vector
    memoryWrite(result);

    return result;
}

// 0x282cf0
std::shared_ptr<Cache::Pointer> Cache::getBestPosition(
    detail::AddressImpl<uint32_t> numBytes,
    std::unordered_map<std::string, bool> const& nameMap,
    bool gapScan)
{
    // Create a new Pointer with default values
    // Initial state: position_=0, size_=0, hash_=0, numRepeats_=1, waveform_=null, state_=Free
    auto pointer = std::make_shared<Pointer>();
    pointer->position_ = 0;
    pointer->size_ = 0;
    pointer->hash_ = 0;
    pointer->numRepeats_ = 1;
    pointer->waveform_ = nullptr;
    pointer->state_ = PointerState::Free;

    // If append mode (this->isHirzel_ == true), just place at offset 0 with requested size
    if (isHirzel_) {
        pointer->position_ = 0;
        pointer->size_ = numBytes;
        return pointer;
    }

    // Otherwise find position in existing allocations.
    // Binary logic (0x282cf0+0xa6):
    //   gapScan==false: try to place at end of last allocation (fast path);
    //                      if not enough room, recursively call with gapScan=true.
    //   gapScan==true:  gap-scan all pointers for best-fit gap.
    auto begin = pointers_.begin();
    auto end = pointers_.end();

    // 0x282cf0+0x97: Empty pointers — place at position 0 and emplace_back.
    if (begin == end) {
        pointer->position_ = 0;
        pointer->size_ = numBytes;
        pointers_.emplace_back(pointer);
        return pointer;
    }

    if (!gapScan) {
        // 0x282cf0+0x15b: Fast path — try appending after the last allocation.
        auto lastIt = end;
        --lastIt;
        auto const& lastPtr = *lastIt;
        uint32_t endPos = lastPtr->position_ + lastPtr->size_;

        uint32_t freeSpace = size_ - endPos;
        if (freeSpace >= numBytes) {
            // Fits at end — assign position and return.
            pointer->position_ = endPos;
            pointer->size_ = numBytes;
            return pointer;
        }

        // 0x282cf0+0x180: Not enough room at end — fall back to gap scan.
        // Recursive call with appendMode=true.
        return getBestPosition(numBytes, nameMap, /*gapScan=*/true);
    } else {
        // 0x282cf0+0xaf: Gap-scan path — find smallest gap that fits numBytes.
        // nameMap maps waveform names → bool; entries with value==true are
        // "about to be freed" and are skipped during gap calculation, allowing
        // their space to be reused.  Prefetch builds this map from the set of
        // waveforms it plans to evict in the current allocation round.
        // (See unknowns.md #62 — resolved.)
        uint32_t totalSize = size_;
        uint32_t currentEnd = 0;
        uint32_t bestGap = totalSize;
        uint32_t bestPosition = totalSize;  // sentinel: starts at total size (invalid)

        for (auto it = begin; it != end; ++it) {
            auto const& p = *it;
            if (p->state_ == PointerState::Free) continue;

            // Check if this pointer's waveform name is in the nameMap
            auto mapIt = nameMap.find(p->waveform_->getName());
            if (mapIt != nameMap.end() && mapIt->second) {
                // If mapped to true, skip this pointer (it will be reused/freed)
                continue;
            }

            // Gap between currentEnd and this pointer's position
            uint32_t gap = p->position_ - currentEnd;
            if (gap == numBytes) {
                // Exact fit — use it immediately
                bestPosition = currentEnd;
                break;
            }
            if (gap > numBytes && gap < bestGap) {
                bestGap = gap;
                bestPosition = currentEnd;
            }
            currentEnd = p->position_ + p->size_;
        }

        // If bestPosition == size_, check trailing gap after all allocations
        if (bestPosition == size_) {
            uint32_t trailingEnd = 0;
            for (auto it = begin; it != end; ++it) {
                auto const& p = *it;
                if (p->state_ == PointerState::Free) continue;
                auto mapIt = nameMap.find(p->waveform_->getName());
                if (mapIt != nameMap.end() && mapIt->second) {
                    continue;
                }
                trailingEnd = p->position_ + p->size_;
            }
            uint32_t trailingGap = size_ - trailingEnd;
            if (trailingGap >= numBytes) {
                bestPosition = trailingEnd;
            }
        }

        if (bestPosition == size_) {
            // No room — throw CacheException (errMsg[0x15])
            auto const& msg = errMsg[CacheMemoryFull];
            throw CacheException(msg);
            // Note: in binary this uses boost::throw_exception with source_location
        }

        pointer->position_ = bestPosition;
        pointer->size_ = numBytes;
        return pointer;
    }
}

// 0x283020
void Cache::memoryWrite(std::shared_ptr<Pointer> ptr) {
    // Remove any existing pointers that overlap with ptr's range, then insert
    // ptr at the correct sorted position.
    //
    // Binary does manual memmove-style shared_ptr shifting with inline
    // refcount manipulation. This semantic version uses erase() which is
    // functionally equivalent.

    uint32_t ptrPos = ptr->position_;
    uint32_t ptrEnd = ptrPos + ptr->size_;

    // Phase 1: Remove all overlapping entries.
    // Two cases in binary (left-overlap at 0x283086, right-overlap at 0x28310b),
    // but both ultimately erase every entry whose range intersects [ptrPos, ptrEnd).
    for (auto it = pointers_.begin(); it != pointers_.end(); ) {
        uint32_t existPos = (*it)->position_;
        uint32_t existEnd = existPos + (*it)->size_;

        if (existPos < ptrPos) {
            // Existing starts before new
            if (existEnd <= ptrPos) {
                ++it;           // no overlap — skip
                continue;
            }
            // Left overlap: existEnd > ptrPos — erase this and all subsequent overlapping
            it = pointers_.erase(it);
        } else {
            // Existing starts at or after new
            if (existPos >= ptrEnd) {
                ++it;           // past new range — skip
                continue;
            }
            // Right overlap: existPos < ptrEnd — erase
            it = pointers_.erase(it);
        }
    }

    // Phase 2: Find sorted insertion point (by position_) and insert.
    // Binary at 0x28327b: linear scan comparing position_ values.
    auto insertPos = pointers_.end();
    for (auto it = pointers_.begin(); it != pointers_.end(); ++it) {
        if ((*it)->position_ > ptrPos) {        // 0x283293: cmp [rcx], eax; ja
            insertPos = it;
            break;
        }
    }

    if (insertPos == pointers_.end()) {
        pointers_.emplace_back(ptr);             // 0x2832b4: jmp emplace_back
    } else {
        pointers_.insert(insertPos, ptr);        // 0x2832cd: jmp vector::insert
    }
}

// 0x2832e0
bool Cache::stillInMemory(std::shared_ptr<Pointer> ptr) const {
    if (!ptr) return false;

    for (auto const& p : pointers_) {
        if (p->position_ != ptr->position_) continue;
        if (p->size_ != ptr->size_) continue;
        // Compare waveform names (WaveformIR first field = name_ string)
        // WaveformIR inherits Waveform; first field is name_ (std::string at +0x00)
        auto const& existName = p->waveform_->getName();
        auto const& targetName = ptr->waveform_->getName();
        if (existName == targetName) {
            return true;
        }
    }
    return false;
}

// 0x2833d0
void Cache::reuse(std::shared_ptr<Pointer> ptr) {
    for (auto const& p : pointers_) {
        if (p->position_ != ptr->position_) continue;
        if (p->size_ != ptr->size_) continue;
        // Compare waveform names
        // WaveformIR inherits Waveform; first field is name_ (std::string at +0x00)
        auto const& existName = p->waveform_->getName();
        auto const& targetName = ptr->waveform_->getName();
        if (existName == targetName) {
            p->state_ = PointerState::Ready;
            return;
        }
    }
}

// 0x2834c0
void Cache::play(std::shared_ptr<Pointer> ptr, PointerState playMode) {
    if (!ptr) {
        // Throw CacheException with errMsg[0x16]
        throw CacheException(errMsg[PlayNullPtr]);
    }

    // First: update previous playing/lastPlayed pointer states
    for (auto it = pointers_.begin(); it != pointers_.end(); ++it) {
        PointerState s = (*it)->state_;
        if (s == PointerState::LastPlayed) {
            (*it)->state_ = PointerState::Free;
            break;
        } else if (s == PointerState::Playing) {
            (*it)->state_ = PointerState::Ready;
            break;
        }
    }

    // Find the matching pointer by position and size, set its state
    uint32_t pos = ptr->position_;
    for (auto it = pointers_.begin(); it != pointers_.end(); ++it) {
        if ((*it)->position_ != pos) continue;
        if ((*it)->size_ != ptr->size_) continue;
        if (playMode == PointerState::Free) {
            (*it)->state_ = PointerState::LastPlayed;  // 0x283530: movl $0x1
        } else {
            (*it)->state_ = PointerState::Playing;     // 0x28353d: movl $0x2
        }
        return;
    }
}

// 0x283640
void Cache::resetPlay() {
    for (auto const& p : pointers_) {
        if (p->state_ == PointerState::LastPlayed) {
            p->state_ = PointerState::Free;
            return;
        } else if (p->state_ == PointerState::Playing) {
            p->state_ = PointerState::Ready;
            return;
        }
    }
}

// 0x283690
void Cache::free(std::shared_ptr<Pointer> ptr) {
    if (!ptr) {
        // Throw CacheException with errMsg[0x13]
        throw CacheException(errMsg[FreeNullPtr]);
    }

    // Find matching pointer by position and size
    bool found = false;
    for (auto it = pointers_.begin(); it != pointers_.end(); ++it) {
        if ((*it)->position_ != ptr->position_) continue;
        if ((*it)->size_ != ptr->size_) continue;
        // Found — erase it from vector
        pointers_.erase(it);
        found = true;
        break;
    }

    if (!found) {
        // Throw CacheException with errMsg[0x14]
        throw CacheException(errMsg[FreeModifiedPtr]);
    }
}

// 0x283b50
void Cache::print() const {
    for (auto const& p : pointers_) {
        std::cout << p->str() << "\n";
    }
}

// ============================================================================
// Cache::Pointer
// ============================================================================

// 0x283c30
std::string Cache::Pointer::str() const {
    std::ostringstream oss;

    // Print address range: "position_ .. (position_+size_-1)"
    oss << detail::AddressImpl<uint32_t>(position_);
    oss << " - ";
    oss << (position_ + size_ - 1);
    oss << " -- ";

    // Print state name
    static const char* stateNames[] = {"ready", "last played", "playing", "free"};
    static const size_t stateLens[] = {5, 11, 7, 4};

    if (static_cast<unsigned>(state_) < 4) {
        oss.write(stateNames[static_cast<int>(state_)],
                  stateLens[static_cast<int>(state_)]);
    } else {
        oss << "INVALID STATE";
    }

    return oss.str();
}

} // namespace zhinst
