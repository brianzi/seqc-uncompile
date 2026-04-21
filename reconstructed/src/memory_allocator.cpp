// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Class: zhinst::MemoryAllocator
//
// Methods:
//   ~MemoryAllocator()                         — 0x29f2d0
//   allocateCLAligned(uint)                     — inlined (lambdas @0x2aa960, 0x2aac80)
//   allocateReloadingCL(uint, set<ulong>)       — inlined (lambda @0x2ad190)
//   allocateFirstSuitableFreeBlock<T>(T)        — 0x2aa960 / 0x2aac80 / 0x2ad190
//   allocateCLAligned::lambda#2::operator()     — 0x2accd0
// ============================================================================

#include "zhinst/memory_allocator.hpp"
#include "zhinst/device_constants.hpp"

#include <algorithm>
#include <cstring>

namespace zhinst {

// ---------------------------------------------------------------------------
// ~MemoryAllocator — 0x29f2d0 (0x38 bytes)
//
// Destroys:
//   1. deque<MemoryBlock> at +0x40 (calls deque::~deque @0x2a0cf0)
//   2. vector<uint32_t> at +0x20 (frees backing buffer)
// Scalars at +0x00..+0x1F and +0x38 need no destruction.
// ---------------------------------------------------------------------------
MemoryAllocator::~MemoryAllocator() {  // 0x29f2d0
    // Compiler-generated: ~deque() then ~vector()
}

// ---------------------------------------------------------------------------
// allocateCLAligned — INLINED (no standalone address)
//
// Two-phase allocation:
//   Phase 1 (lambda#1 @0x2aa960): Fast path — block fits within one cache line.
//     Checks alignment, CL ownership match, and that allocation doesn't cross
//     a CL boundary.
//   Phase 2 (lambda#2 @0x2aac80): General path — may span multiple CLs.
//     Computes multi-CL-aware alignment, checks and fills CL ownership slots
//     using SSE2 vectorized comparison (operator() @0x2accd0).
//     Decrements numCacheLines_ on ownership acquisition.
//
// Called from WavetableIR::allocateWaveformsForFifo inlined at ~0x2aa740.
// ---------------------------------------------------------------------------
MemoryBlock MemoryAllocator::allocateCLAligned(unsigned int size) {
    // Phase 1: fast single-CL path
    if (size < deviceConstants_->waveformAlignment) {  // DC+0x14
        MemoryBlock result = allocateFirstSuitableFreeBlock(
            [&](unsigned int blockStart, unsigned int blockSize) -> MemoryBlock {
                uint32_t align = deviceConstants_->waveformAlignment;
                uint32_t aligned = (blockStart + align - 1) & ~(align - 1);
                if (blockSize - aligned < size)
                    return {0, 0, 0};  // doesn't fit

                // Verify CL ownership matches
                uint32_t slot = aligned / cacheLineSize_;
                if (slot < cacheLineUsage_.size() &&
                    cacheLineUsage_[slot] != 0xFFFFFFFF &&
                    cacheLineUsage_[slot] != (aligned - (aligned % cacheLineSize_)))
                    return {0, 0, 0};

                // Verify doesn't cross CL boundary
                uint32_t nextBoundary = ((aligned / cacheLineSize_) + 1) * cacheLineSize_;
                if (nextBoundary < blockStart + blockSize &&
                    nextBoundary - aligned < size)
                    return {0, 0, 0};

                return {aligned, aligned + size, 1};  // valid
            });
        if (result.flags & 1)
            return result;
    }

    // Phase 2: general multi-CL path (lambda#2 operator() @0x2accd0)
    return allocateFirstSuitableFreeBlock(
        [&](unsigned int blockStart, unsigned int blockSize) -> MemoryBlock {
            uint32_t clSize = deviceConstants_->waveformAlignment;
            uint32_t alignQ = clSize;
            if (size > clSize)
                alignQ = deviceConstants_->maxBlocks * clSize;

            uint32_t aligned = (blockStart + alignQ - 1) & ~(alignQ - 1);
            if (aligned < blockStart || aligned >= blockStart + blockSize)
                return {0, 0, 0};
            if (blockStart + blockSize - aligned < size)
                return {0, 0, 0};

            // Check and fill CL ownership slots (SSE2 vectorized in binary)
            uint32_t startSlot = aligned / cacheLineSize_;
            uint32_t endSlot = (aligned + size + cacheLineSize_ - 1) / cacheLineSize_;
            for (uint32_t s = startSlot; s < endSlot && s < cacheLineUsage_.size(); ++s) {
                if (cacheLineUsage_[s] != 0xFFFFFFFF)
                    return {0, 0, 0};  // slot occupied
            }
            // Claim slots
            uint32_t clBase = aligned - (aligned % cacheLineSize_);
            for (uint32_t s = startSlot; s < endSlot && s < cacheLineUsage_.size(); ++s) {
                cacheLineUsage_[s] = clBase;
                clBase += cacheLineSize_;
            }
            numCacheLines_ -= (endSlot - startSlot);

            uint32_t flags = 1;  // valid
            if (endSlot > startSlot + 1)
                flags |= (1 << 8);  // crossesCacheLine
            return {aligned, aligned + size, flags};
        });
}

// ---------------------------------------------------------------------------
// allocateReloadingCL — INLINED (no standalone address)
//
// Fallback allocator when allocateCLAligned fails. Avoids cache-line conflicts
// with existing waveform allocations.
//
// lambda instantiation @0x2ad190:
//   For each candidate position, checks that none of its CL-aligned addresses
//   appear in the usedAddrs set. If a conflict is found, shifts to next
//   alignment boundary and retries.
//
// Called from WavetableIR::allocateWaveformsForFifo inlined at ~0x2ad02e.
// ---------------------------------------------------------------------------
MemoryBlock MemoryAllocator::allocateReloadingCL(
    unsigned int size,
    std::set<unsigned long> const& usedAddrs)  // CL indices of existing waveforms
{
    return allocateFirstSuitableFreeBlock(
        [&](unsigned int blockStart, unsigned int blockSize) -> MemoryBlock {
            uint32_t align = deviceConstants_->waveformAlignment;
            uint32_t aligned = (blockStart + align - 1) & ~(align - 1);

            while (aligned < blockStart + blockSize) {
                if (blockStart + blockSize - aligned < size) {
                    return {0, 0, 0};  // doesn't fit
                }

                // Check CL conflict: for each CL this allocation would touch,
                // verify no entry exists in usedAddrs
                bool conflict = false;
                uint32_t numCLsNeeded = maxBlocksPerCL_;
                uint32_t addr = aligned;
                for (uint32_t i = 0;
                     i < numCLsNeeded && addr < blockStart + blockSize;
                     ++i, addr += align) {
                    uint32_t key = (addr % memorySizeInSamples_) / cacheLineSize_;
                    auto it = usedAddrs.lower_bound(key);
                    if (it != usedAddrs.end() && *it == key) {
                        conflict = true;
                        break;
                    }
                }

                if (!conflict)
                    return {aligned, aligned + size, 1};  // valid, no conflicts

                // Shift to next alignment boundary
                aligned += align;
            }
            return {0, 0, 0};
        });
}

// ---------------------------------------------------------------------------
// allocateFirstSuitableFreeBlock<T> — template
//
// Three instantiations in binary:
//   @0x2aa960 — allocateCLAligned::lambda#1 (fast single-CL)
//   @0x2aac80 — allocateCLAligned::lambda#2 (general multi-CL)
//   @0x2ad190 — allocateReloadingCL::lambda (conflict avoidance)
//
// Algorithm:
//   1. Iterate freeBlocks_ deque (element size 12, page size 341)
//   2. For each free block, apply predicate
//   3. On success: split the free block (remove consumed portion, insert
//      remainders back into deque via deque::insert @0x2aae70)
//   4. Also checks the "tail" region [lastFreeBlock.end, startOffset_+memorySizeInSamples_)
//   5. Returns MemoryBlock with flags=0 on failure
//
// Deque iteration uses magic constant 0x8060180601806019 for division by 341.
// ---------------------------------------------------------------------------
template <typename Pred>
MemoryBlock MemoryAllocator::allocateFirstSuitableFreeBlock(Pred pred) {
    // Iterate free blocks
    for (auto it = freeBlocks_.begin(); it != freeBlocks_.end(); ++it) {
        MemoryBlock& block = *it;
        MemoryBlock result = pred(block.start, block.end);
        if (result.flags & 1) {
            // Split: remove consumed range from free block
            uint32_t origStart = block.start;
            uint32_t origEnd = block.end;

            freeBlocks_.erase(it);

            // Insert remainder before allocation
            if (result.start > origStart) {
                MemoryBlock before = {origStart, result.start, 0};
                // Insert at appropriate position
                freeBlocks_.insert(freeBlocks_.begin(), before);
            }
            // Insert remainder after allocation
            if (result.end < origEnd) {
                MemoryBlock after = {result.end, origEnd, 0};
                freeBlocks_.insert(freeBlocks_.end(), after);
            }

            return result;
        }
    }

    // Try tail region: [startOffset_, startOffset_ + memorySizeInSamples_)
    uint32_t tailEnd = startOffset_ + memorySizeInSamples_;
    uint32_t tailStart = freeBlocks_.empty()
        ? startOffset_
        : freeBlocks_.back().end;

    if (tailStart < tailEnd) {
        MemoryBlock result = pred(tailStart, tailEnd);
        if (result.flags & 1) {
            // Insert remainder before
            if (result.start > tailStart) {
                MemoryBlock before = {tailStart, result.start, 0};
                freeBlocks_.push_back(before);
            }
            // Insert remainder after
            if (result.end < tailEnd) {
                MemoryBlock after = {result.end, tailEnd, 0};
                freeBlocks_.push_back(after);
            }
            return result;
        }
    }

    return {0, 0, 0};  // allocation failed
}

// Explicit template instantiations are not needed since methods are inlined
// in the binary. The template body above captures the shared algorithm from
// all three instantiations at 0x2aa960, 0x2aac80, and 0x2ad190.

} // namespace zhinst
