// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Cache class implementation
// ============================================================================

#include "zhinst/cache.hpp"
#include "zhinst/waveform_ir.hpp"
#include "zhinst/error_messages.hpp"

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
Cache::Cache(detail::AddressImpl<uint32_t> size, int pageSize, bool appendMode)
    : size_(size)
    , pageSize_(pageSize)
    , appendMode_(appendMode)
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
    auto scope = std::make_shared<Cache>(size_, pageSize_, appendMode_);
    // Note: in the binary this is done via __shared_ptr_emplace<Cache> + manual copy
    // The new cache copies only size_+pageSize_ (as 8 bytes), appendMode_, and the pointers vector
    scope->pointers_ = pointers_;
    return scope;
}

// 0x282a30
std::shared_ptr<Cache::Pointer> Cache::allocate(
    std::shared_ptr<WaveformIR> waveform,
    detail::AddressImpl<uint32_t> numSamples,
    std::unordered_map<std::string, bool> const& nameMap,
    int pageSize,
    bool split)
{
    // Compute free memory in pages
    uint32_t freeMemory = size_;
    for (auto const& p : pointers_) {
        if (p->state_ == PointerState::Free) continue;
        freeMemory -= p->size_;
        if (freeMemory > size_) { freeMemory = 0; break; }
    }
    uint32_t freePages = freeMemory / pageSize;

    std::shared_ptr<Pointer> result;

    if (!split || numSamples < freePages) {
        // Case: don't split, or whole waveform fits — allocate two parts
        // First: compute number of sub-allocations
        uint32_t numAllocs = numSamples / freePages;
        numAllocs++;
        uint32_t halfSize = size_ / 2;  // actually size_ >> 1 = r10d >> 1
        uint32_t altAllocs = numSamples / halfSize;
        if (numAllocs <= altAllocs) {
            numAllocs = altAllocs;
        }
        uint32_t chunkSize = numSamples / numAllocs;

        // Allocate first chunk as Aligned
        result = allocate(waveform, chunkSize, nameMap, CacheType::Aligned);

        // Compute hash and numRepeats for the pointer
        Pointer* ptr = result.get();
        uint32_t halfSz = ptr->size_ / 2;
        ptr->hash_ = ~(ptr->position_ ^ (ptr->position_ + halfSz));
        ptr->numRepeats_ = numSamples / halfSz + 1;
    } else {
        // Case: split mode — allocate the full numSamples as Normal
        result = allocate(waveform, numSamples, nameMap, CacheType::Normal);
    }

    return result;
}

// 0x282be0
std::shared_ptr<Cache::Pointer> Cache::allocate(
    std::shared_ptr<WaveformIR> waveform,
    detail::AddressImpl<uint32_t> numSamples,
    std::unordered_map<std::string, bool> const& nameMap,
    CacheType cacheType)
{
    // Compute aligned size
    uint32_t pageSize = pageSize_;
    if (cacheType == CacheType::Aligned) {
        pageSize = pageSize_ * 2;  // shl by 1
    }
    uint32_t remainder = numSamples % pageSize;
    uint32_t alignedSize;
    if (remainder == 0) {
        alignedSize = numSamples;
    } else {
        alignedSize = numSamples + pageSize - remainder;
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
    detail::AddressImpl<uint32_t> numSamples,
    std::unordered_map<std::string, bool> const& nameMap,
    bool appendMode)
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

    // If append mode (this->appendMode_ == true), just place at offset 0 with requested size
    if (appendMode_) {
        pointer->position_ = 0;
        pointer->size_ = numSamples;
        return pointer;
    }

    // Otherwise find the best gap in existing allocations
    auto begin = pointers_.begin();
    auto end = pointers_.end();

    if (!appendMode) {
        // Scan all pointers, find the smallest gap that fits numSamples
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
            if (gap == numSamples) {
                // Exact fit — use it immediately
                bestPosition = currentEnd;
                break;
            }
            if (gap > numSamples && gap < bestGap) {
                bestGap = gap;
                bestPosition = currentEnd;
            }
            currentEnd = p->position_ + p->size_;
        }

        // If bestPosition == size_, the only gap was found at iteration end
        if (bestPosition == size_) {
            // Check trailing gap after all allocations
            uint32_t trailingEnd = 0;
            for (auto it = begin; it != end; ++it) {
                auto const& p = *it;
                if (p->state_ == PointerState::Free) continue;
                // Same nameMap check
                auto mapIt = nameMap.find(p->waveform_->getName());
                if (mapIt != nameMap.end() && mapIt->second) {
                    continue;
                }
                trailingEnd = p->position_ + p->size_;
            }
            uint32_t trailingGap = size_ - trailingEnd;
            if (trailingGap >= numSamples) {
                bestPosition = trailingEnd;
            }
        }

        if (bestPosition == size_) {
            // No room — throw CacheException (errMsg[0x15])
            auto const& msg = errMsg[static_cast<ErrorMessageT>(0x15)];
            throw CacheException(msg);
            // Note: in binary this uses boost::throw_exception with source_location
        }

        pointer->position_ = bestPosition;
        pointer->size_ = numSamples;
        return pointer;
    } else {
        // appendMode == true from recursive call: place at end of last allocation
        auto lastIt = end;
        --lastIt;
        auto const& lastPtr = *lastIt;
        uint32_t endPos = lastPtr->position_ + lastPtr->size_;

        uint32_t freeSpace = size_ - endPos;
        if (freeSpace < numSamples) {
            // Not enough space — throw CacheException (errMsg[0x15])
            auto const& msg = errMsg[static_cast<ErrorMessageT>(0x15)];
            throw CacheException(msg);
        }

        pointer->position_ = endPos;
        pointer->size_ = numSamples;

        // Add to vector
        pointers_.emplace_back(pointer);
        return pointer;
    }
}

// 0x283020
void Cache::memoryWrite(std::shared_ptr<Pointer> ptr) {
    // Remove any existing pointers that overlap with ptr's range, then insert
    // ptr at the correct sorted position.
    auto begin = pointers_.begin();
    auto end = pointers_.end();

    uint32_t ptrPos = ptr->position_;
    uint32_t ptrEnd = ptrPos + ptr->size_;

    for (auto it = begin; it != end; ++it) {
        uint32_t existPos = (*it)->position_;
        uint32_t existEnd = existPos + (*it)->size_;

        if (existPos < ptrPos) {
            // Existing starts before new — check if it overlaps
            if (existEnd <= ptrPos) continue;
            // Overlap: erase from it+1 onward that also overlaps, then insert
            auto eraseStart = it + 1;
            while (eraseStart != end) {
                // Shift elements down (remove overlapping entries)
                // Binary does manual memmove-style shifting
                ++eraseStart;
            }
            // Update end pointer
            break;
        } else {
            // Existing starts at or after new
            if (existPos >= ptrEnd) continue;
            // Overlap from the right side — remove overlapping entries
            break;
        }
    }

    // Find insertion point (sorted by position)
    begin = pointers_.begin();
    end = pointers_.end();
    auto insertPos = end;
    for (auto it = begin; it != end; ++it) {
        if ((*it)->position_ > ptrPos) {
            insertPos = it;
            break;
        }
    }

    if (insertPos == end) {
        pointers_.emplace_back(ptr);
    } else {
        pointers_.insert(insertPos, ptr);
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
void Cache::play(std::shared_ptr<Pointer> ptr, PointerState state) {
    if (!ptr) {
        // Throw CacheException with errMsg[0x16]
        throw CacheException(errMsg[static_cast<ErrorMessageT>(0x16)]);
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
        if (state == PointerState::Free) {
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
        throw CacheException(errMsg[static_cast<ErrorMessageT>(0x13)]);
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
        throw CacheException(errMsg[static_cast<ErrorMessageT>(0x14)]);
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
