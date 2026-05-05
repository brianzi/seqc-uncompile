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

#include "zhinst/codegen/memory_allocator.hpp"
#include "zhinst/device/device_constants.hpp"

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
// MemoryAllocator(const DeviceConstants*, uint32_t)   — INLINED at all
// call sites (allocateWaveforms / allocateWaveformsForFifo). The binary has
// no standalone ctor symbol, so the build host gets an unresolved reference
// from any TU that takes the ctor's address (e.g. `make_unique<MemoryAllocator>`
// or any local construction in code we don't see). We provide a real
// definition matching the documented layout.
//
// Initialization (derived from the dtor's field offsets and the inlined-ctor
// patterns visible in WavetableIR::allocateWaveforms*):
//   +0x00  deviceConstants_     = dc
//   +0x08  startOffset_         = startOffset
//   +0x0C  lastAllocEnd_        = 0xFFFFFFFF (sentinel)
//   +0x10  memorySizeInBytes_   = dc->waveformMemorySize        (DC+0x0C)
//   +0x14  cacheLineSizeBytes_  = dc->waveformAlignment         (DC+0x14)
//   +0x18  maxBlocksPerCL_      = dc->cachePageCount            (DC+0x18)
//   +0x20  cacheLineUsage_      = vector<uint32_t> of numCLs slots, all 0xFFFFFFFF
//   +0x38  numCacheLines_       = memorySizeInBytes_ / cacheLineSizeBytes_
//   +0x40  freeBlocks_          = empty deque<MemoryBlock>
// ---------------------------------------------------------------------------
MemoryAllocator::MemoryAllocator(const DeviceConstants* dc, uint32_t startOffset)
    : deviceConstants_(dc),
      startOffset_(startOffset),
      lastAllocEnd_(0xFFFFFFFFu),
      memorySizeInBytes_(dc->waveformMemorySize),
      cacheLineSizeBytes_(dc->waveformAlignment),
      maxBlocksPerCL_(dc->cachePageCount),
      pad_1C_(0),
      cacheLineUsage_(),
      numCacheLines_(0),
      pad_3C_(0),
      freeBlocks_()
{
    // Initialize per-CL ownership table. cacheLineSize_ may legitimately be
    // 1 (no caching) for some device families; in that case there is one
    // "CL slot" per sample, which the binary tolerates.
    if (cacheLineSizeBytes_ != 0) {
        numCacheLines_ = memorySizeInBytes_ / cacheLineSizeBytes_;
        cacheLineUsage_.assign(numCacheLines_, 0xFFFFFFFFu);
    }
}

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
    // Phase 1: fast single-CL path (lambda#1 @0x2aa960)
    //
    // Binary: Phase 1 uses waveformElfAlignment (DC+0x24, always 64)
    // for alignment, NOT waveformAlignment (DC+0x14, 4096 for HDAWG).
    // Phase 1 is strictly a "re-use within already-claimed CL" fast path.
    // Free CL slots (0xFFFFFFFF) cause rejection — only slots already owned
    // by the correct clBase are accepted.  First-time allocations always
    // fall through to Phase 2 which claims the CL slots.
    //
    // Binary tail path at 0x2aab9f-0x2aac1a:
    //   aligned = align_up(blockStart, waveformElfAlignment)
    //   available = blockEnd - aligned
    //   if (available < size) → fail
    //   slot = (aligned % memorySizeInSamples_) / cacheLineSize_
    //   clBase = aligned - (aligned % cacheLineSize_)
    //   if (cacheLineUsage_[slot] != clBase) → fail
    //   Then check: nextBoundary = (aligned / waveformAlignment + 1) * waveformAlignment
    //   if (nextBoundary >= blockEnd || nextBoundary - aligned >= size) → success
    //   Return {aligned, aligned + size, 1}
    if (size < deviceConstants_->waveformAlignment) {  // DC+0x14
        MemoryBlock result = allocateFirstSuitableFreeBlock(
            [&](unsigned int blockStart, unsigned int blockEnd) -> MemoryBlock {
                uint32_t elfAlign = deviceConstants_->waveformElfAlignment;  // DC+0x24, =64
                uint32_t aligned = (blockStart + elfAlign - 1) & ~(elfAlign - 1);

                // Space check
                if (blockEnd - aligned < size)
                    return {0, 0, 0};

                // CL ownership check: slot must already be claimed with matching clBase.
                // Free slots (0xFFFFFFFF) are NOT accepted — Phase 1 only re-uses.
                uint32_t slot = (aligned % memorySizeInBytes_) / cacheLineSizeBytes_;
                uint32_t clBase = aligned - (aligned % cacheLineSizeBytes_);
                if (slot >= cacheLineUsage_.size() ||
                    cacheLineUsage_[slot] != clBase)
                    return {0, 0, 0};

                // Check allocation fits before next waveformAlignment boundary
                // Binary at +498-529: success if nextBoundary >= blockEnd OR
                // nextBoundary - aligned >= size. Fail if NEITHER holds.
                uint32_t wfAlign = deviceConstants_->waveformAlignment;  // DC+0x14, =4096 bytes
                uint32_t nextBoundary = (aligned / wfAlign + 1) * wfAlign;
                bool fits = (nextBoundary >= blockEnd) ||
                            (nextBoundary - aligned >= size);
                if (!fits)
                    return {0, 0, 0};

                return {aligned, aligned + size, 1};  // valid, no crossesCacheLine
            });
        if (result.flags & 1)
            return result;
    }

    // Phase 2: general multi-CL path (lambda#2 operator() @0x2accd0)
    // Binary: second arg is blockEnd (absolute), not blockSize.
    return allocateFirstSuitableFreeBlock(
        [&](unsigned int blockStart, unsigned int blockEnd) -> MemoryBlock {
            uint32_t clSize = deviceConstants_->waveformAlignment;  // bytes
            uint32_t alignQ = clSize;
            if (size > clSize)
                alignQ = deviceConstants_->maxBlocks * clSize;

            uint32_t aligned = (blockStart + alignQ - 1) & ~(alignQ - 1);
            if (aligned < blockStart || aligned >= blockEnd)
                return {0, 0, 0};
            if (blockEnd - aligned < size)
                return {0, 0, 0};

            // Check and fill CL ownership slots (SSE2 vectorized in binary)
            // Binary: slot index uses (aligned % memorySizeInSamples_) / cacheLineSize_
            // not aligned / cacheLineSize_, because addresses like 0x80000000 wrap via
            // modular arithmetic to stay within the cacheLineUsage_ vector bounds.
            uint32_t modAddr = aligned % memorySizeInBytes_;
            uint32_t numSlots = maxBlocksPerCL_ * cacheLineSizeBytes_;
            if (numSlots > size) numSlots = size;
            uint32_t startSlot = modAddr / cacheLineSizeBytes_;
            uint32_t endSlot = (modAddr + numSlots + cacheLineSizeBytes_ - 1) / cacheLineSizeBytes_;
            if (endSlot > memorySizeInBytes_ / cacheLineSizeBytes_)
                endSlot = memorySizeInBytes_ / cacheLineSizeBytes_;
            for (uint32_t s = startSlot; s < endSlot && s < cacheLineUsage_.size(); ++s) {
                if (cacheLineUsage_[s] != 0xFFFFFFFF)
                    return {0, 0, 0};  // slot occupied
            }
            // Claim slots
            uint32_t clBase = aligned - (aligned % cacheLineSizeBytes_);
            for (uint32_t s = startSlot; s < endSlot && s < cacheLineUsage_.size(); ++s) {
                cacheLineUsage_[s] = clBase;
                clBase += cacheLineSizeBytes_;
            }
            numCacheLines_ -= (endSlot - startSlot);

            // Binary at 0x2acec7-0x2acf18: crossesCacheLine is always set in
            // Phase 2 (multi-CL path). The condition is r14d != 0, where
            // r14d = min(maxBlocksPerCL * cacheLineSize, numSlots), which is
            // always positive for a valid allocator.
            uint32_t flags = 1 | (1 << 8);  // valid + crossesCacheLine
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
        [&](unsigned int blockStart, unsigned int blockEnd) -> MemoryBlock {
            uint32_t align = deviceConstants_->waveformAlignment;
            uint32_t aligned = (blockStart + align - 1) & ~(align - 1);

            while (aligned < blockEnd) {
                if (blockEnd - aligned < size) {
                    return {0, 0, 0};  // doesn't fit
                }

                // Check CL conflict: for each CL this allocation would touch,
                // verify no entry exists in usedAddrs
                bool conflict = false;
                uint32_t numCLsNeeded = maxBlocksPerCL_;
                uint32_t addr = aligned;
                for (uint32_t i = 0;
                     i < numCLsNeeded && addr < blockEnd;
                     ++i, addr += align) {
                    uint32_t key = (addr % memorySizeInBytes_) / cacheLineSizeBytes_;
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

            // Binary at 0x2aac75: calls deque::insert(it_next, remainder) where
            // it_next is the iterator AFTER the erased element (block.end position).
            // This preserves deque order: remainders go back in place of the consumed block.
            auto insertPos = freeBlocks_.erase(it);  // returns iterator to next element

            // Insert "after" remainder first (at insertPos), then "before" before it,
            // so the final order is [before, after] at the original block's position.
            if (result.end < origEnd) {
                MemoryBlock after = {result.end, origEnd, 0};
                insertPos = freeBlocks_.insert(insertPos, after);
            }
            if (result.start > origStart) {
                MemoryBlock before = {origStart, result.start, 0};
                freeBlocks_.insert(insertPos, before);
            }

            return result;
        }
    }

    // Try tail region: binary uses lastAllocEnd_ (initially 0xFFFFFFFF) as
    // the end bound, not startOffset_ + memorySizeInSamples_.
    // @+579: eax = startOffset_, r11d = lastAllocEnd_.
    // @+409: eax = freeBlocks_.back().end, r10d = lastAllocEnd_.
    uint32_t tailEnd = lastAllocEnd_;
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
