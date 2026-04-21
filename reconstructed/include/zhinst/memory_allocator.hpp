// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Class: zhinst::MemoryAllocator, zhinst::MemoryBlock
// Methods: dtor @0x29f2d0, allocateCLAligned (inlined),
//          allocateReloadingCL (inlined),
//          allocateFirstSuitableFreeBlock<T> @0x2aa960/0x2aac80/0x2ad190
// ============================================================================
#pragma once

#include <cstdint>
#include <deque>
#include <set>
#include <vector>

namespace zhinst {

struct DeviceConstants;

// ============================================================================
// MemoryBlock — 12-byte free/allocated block descriptor
//
// Confirmed: libc++ deque __block_size = 4096/12 = 341
// (visible as imul $0x155 in deque iteration, buffer alloc = 0xFFC bytes)
//
// Return convention from allocateFirstSuitableFreeBlock:
//   rax = start | (end << 32)
//   dl  = flags (bit 0 = valid, bit 8 = crossesCacheLine)
// ============================================================================
struct MemoryBlock {
    uint32_t start;     // +0x00  start address of block
    uint32_t end;       // +0x04  end address (start + size)
    uint32_t flags;     // +0x08  bit 0 = valid, bit 8 = crossesCacheLine
};
static_assert(sizeof(MemoryBlock) == 12, "MemoryBlock must be 12 bytes");

// ============================================================================
// MemoryAllocator — waveform memory allocator with cache-line awareness
//
// Total size: 0x70 (112 bytes)
// No vtable (non-virtual class).
// Constructor is always inlined at call sites (allocateWaveforms,
// allocateWaveformsForFifo in WavetableIR).
//
// Offset  Size  Type                  Field                Notes
// ------  ----  --------------------  -------------------  ---------------------------
// 0x00    8     DeviceConstants*      deviceConstants_     Source of memory geometry
// 0x08    4     uint32_t              startOffset_         Where allocation begins
// 0x0C    4     uint32_t              lastAllocEnd_        0xFFFFFFFF sentinel initially
// 0x10    4     uint32_t              memorySizeInSamples_ DC->waveformMemorySize (+0x0C)
// 0x14    4     uint32_t              cacheLineSize_       DC->waveformAlignment (+0x14)
// 0x18    4     uint32_t              maxBlocksPerCL_      DC->cachePageCount or maxBlocks
// 0x1C    4     (padding)
// 0x20    24    vector<uint32_t>      cacheLineUsage_      Per-CL ownership; 0xFFFFFFFF=free
// 0x38    4     uint32_t              numCacheLines_       memorySizeInSamples / cacheLineSize
// 0x3C    4     (padding)
// 0x40    48    deque<MemoryBlock>    freeBlocks_          Free block list (341 per page)
// ============================================================================
class MemoryAllocator {
public:
    // Constructor is inlined at call sites — no standalone function.
    // Pseudocode:
    //   MemoryAllocator(DeviceConstants* dc, uint32_t startOffset) {
    //       deviceConstants_ = dc;
    //       startOffset_ = startOffset;
    //       lastAllocEnd_ = 0xFFFFFFFF;
    //       memorySizeInSamples_ = dc->waveformMemorySize;
    //       cacheLineSize_ = dc->waveformAlignment;
    //       maxBlocksPerCL_ = dc->cachePageCount; // or maxBlocks, depending on path
    //       numCacheLines_ = memorySizeInSamples_ / cacheLineSize_;
    //       cacheLineUsage_.resize(numCacheLines_, 0xFFFFFFFF);
    //   }

    ~MemoryAllocator();  // 0x29f2d0

    // Allocate a block with cache-line alignment.
    // Two-phase: first tries fast path (fits in one CL), then general path.
    // Both phases call allocateFirstSuitableFreeBlock with different lambdas.
    // INLINED — no standalone address.
    MemoryBlock allocateCLAligned(unsigned int size);

    // Allocate avoiding cache-line conflicts with existing waveforms.
    // Builds a set of used CL indices, then finds a position with no overlap.
    // Fallback when allocateCLAligned fails.
    // INLINED — no standalone address.
    MemoryBlock allocateReloadingCL(unsigned int size,
                                    std::set<unsigned long> const& usedAddrs);

    // Template: iterate freeBlocks_ deque, apply predicate to each block.
    // Three instantiations:
    //   allocateCLAligned::lambda#1  @0x2aa960 (fast: single CL check)
    //   allocateCLAligned::lambda#2  @0x2aac80 (general: multi-CL ownership)
    //   allocateReloadingCL::lambda  @0x2ad190 (conflict avoidance)
    template <typename Pred>
    MemoryBlock allocateFirstSuitableFreeBlock(Pred pred);

private:
    DeviceConstants*        deviceConstants_;      // +0x00
    uint32_t                startOffset_;           // +0x08
    uint32_t                lastAllocEnd_;          // +0x0C  sentinel 0xFFFFFFFF
    uint32_t                memorySizeInSamples_;   // +0x10
    uint32_t                cacheLineSize_;         // +0x14
    uint32_t                maxBlocksPerCL_;        // +0x18
    uint32_t                pad_1C_;                // +0x1C
    std::vector<uint32_t>   cacheLineUsage_;        // +0x20  per-CL owner; 0xFFFFFFFF=free
    uint32_t                numCacheLines_;          // +0x38
    uint32_t                pad_3C_;                // +0x3C
    std::deque<MemoryBlock> freeBlocks_;            // +0x40  free block list
};
static_assert(sizeof(MemoryAllocator) == 0x70,
              "MemoryAllocator must be 0x70 (112) bytes");

} // namespace zhinst
