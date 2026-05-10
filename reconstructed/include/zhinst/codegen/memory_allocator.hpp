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
//! \brief Free / allocated region descriptor used by `MemoryAllocator`.
//!
//! Represents a half-open `[start, end)` byte range together with a
//! flags byte (bit 0 = the range was successfully allocated; bit 8 =
//! the range straddles a cache-line boundary).  Returned by value
//! from each `MemoryAllocator::allocate*` call and stored in the
//! allocator's free-list deque between allocations.
struct MemoryBlock {
    uint32_t start;     //!< Inclusive start byte address of the block. +0x00  start address of block
    uint32_t end;       //!< Exclusive end byte address (`start + size`). +0x04  end address (start + size)
    uint32_t flags;     //!< Result flags: bit 0 set on successful allocation; bit 8 set when the block straddles a cache-line boundary. +0x08  bit 0 = valid, bit 8 = crossesCacheLine
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
// 0x10    4     uint32_t              memorySizeInBytes_   DC->waveformMemorySize (+0x0C)
// 0x14    4     uint32_t              cacheLineSizeBytes_  DC->waveformAlignment (+0x14)
// 0x18    4     uint32_t              maxBlocksPerCL_      DC->cachePageCount or maxBlocks
// 0x1C    4     (padding)
// 0x20    24    vector<uint32_t>      cacheLineUsage_      Per-CL ownership; 0xFFFFFFFF=free
// 0x38    4     uint32_t              numCacheLines_       memorySizeInBytes / cacheLineSizeBytes
// 0x3C    4     (padding)
// 0x40    48    deque<MemoryBlock>    freeBlocks_          Free block list (341 per page)
// ============================================================================
//! \brief Cache-line-aware bump allocator for AWG waveform memory.
//!
//! Manages a single linear region of waveform memory (size and
//! cache-line geometry derived from `DeviceConstants`) and hands out
//! `MemoryBlock` ranges via two strategies:
//!
//! - `allocateCLAligned` is the default path used by
//!   `WavetableIR::allocateWaveforms`; it tries a fast single-cache-line
//!   fit first, then falls back to a multi-line allocation that may
//!   straddle a cache-line boundary (recorded in the returned flags
//!   so the consumer can later set `WaveformIR::crossesCacheLine_`).
//! - `allocateReloadingCL` is the FIFO-reloading fallback, invoked by
//!   `WavetableIR::allocateWaveformsForFifo` when a waveform won't fit
//!   into the remaining free space; callers pass the set of
//!   cache-line indices already owned by other waveforms in
//!   `usedAddrs`, and the allocator finds a position whose cache
//!   lines are entirely outside that set so the new waveform can be
//!   reloaded without disturbing the resident ones.
//!
//! Both routines walk `freeBlocks_` via `allocateFirstSuitableFreeBlock`
//! with a strategy-specific predicate; `cacheLineUsage_` tracks
//! per-cache-line ownership (a sentinel `0xFFFFFFFF` denotes a free
//! line) so multi-block fits within one cache line can be detected
//! without re-scanning the free list.  An allocation failure returns
//! a `MemoryBlock` with bit 0 of `flags` cleared, which callers test
//! with `!(block.flags & 1)` to detect overflow.
class MemoryAllocator {
public:
    //! \brief Construct an allocator over a device's waveform memory region.
    //!
    //! \details Captures `dc` (used later to read `waveformAlignment`,
    //! `waveformElfAlignment`, `cachePageCount` / `maxBlocks` on every
    //! allocation), records `startOffset` as the first byte available
    //! for allocation, and arms `lastAllocEnd_` with the sentinel
    //! `0xFFFFFFFF` so that the very first call to
    //! `allocateFirstSuitableFreeBlock` treats the entire post-`startOffset`
    //! region as one large tail.  When `dc->waveformAlignment` is non-zero
    //! the per-cache-line ownership table `cacheLineUsage_` is sized to
    //! `waveformMemorySize / waveformAlignment` slots and every entry is
    //! initialised to the free sentinel `0xFFFFFFFF`.
    //!
    //! \param dc          Device constants supplying the memory geometry;
    //!                    must outlive this allocator.
    //! \param startOffset First byte (in waveform-memory address space)
    //!                    available for allocation.
    //!
    //! \verifyme — initialisation values are taken from the inlined
    //! constructor patterns visible at the call sites in `WavetableIR`;
    //! the standalone constructor symbol does not exist in the binary.
    // Constructor is inlined at call sites — no standalone function.
    MemoryAllocator(const DeviceConstants* dc, uint32_t startOffset);

    //! \brief Release the free-list deque and the per-cache-line table.
    //!
    //! \details Compiler-generated body: destroys `freeBlocks_` first,
    //! then `cacheLineUsage_`.  All other fields are scalars.
    ~MemoryAllocator();  // 0x29f2d0

    //! \brief Allocate `size` bytes with cache-line-aware placement.
    //!
    //! \details Runs in two stages:
    //! - **Stage 1 (fast re-use):** when `size < waveformAlignment`,
    //!   walk the free list looking for a free block whose
    //!   `waveformElfAlignment`-aligned start lies inside a cache-line
    //!   slot already owned by the same cache-line base, with enough
    //!   room left before the next `waveformAlignment` boundary.  Free
    //!   slots (sentinel `0xFFFFFFFF`) are *not* accepted at this
    //!   stage — only re-use of an already-claimed cache line.
    //! - **Stage 2 (general multi-CL):** align to `waveformAlignment`
    //!   (or `maxBlocks * waveformAlignment` when the request exceeds a
    //!   single cache line), require every cache-line slot the
    //!   allocation would cover to be free, then claim those slots and
    //!   set the `crossesCacheLine` flag (bit 8) in the returned block.
    //!
    //! \param size Allocation size in bytes.
    //! \return On success, a `MemoryBlock` `{start, end, flags}` with
    //!         bit 0 set; on failure, `{0, 0, 0}`.  Callers should test
    //!         `block.flags & 1` to distinguish.
    // Allocate a block with cache-line alignment.
    // Two-phase: first tries fast path (fits in one CL), then general path.
    // Both phases call allocateFirstSuitableFreeBlock with different lambdas.
    // INLINED — no standalone address.
    MemoryBlock allocateCLAligned(unsigned int size);

    //! \brief Allocate `size` bytes avoiding cache-line conflicts with an
    //!        existing waveform set (FIFO-reload fallback).
    //!
    //! \details Walks the free list and, for each candidate
    //! `waveformAlignment`-aligned start, verifies that none of the
    //! `maxBlocksPerCL_` cache-line indices the allocation would touch
    //! appear in `usedAddrs`.  On a conflict the candidate is bumped by
    //! `waveformAlignment` and re-tried within the same free block;
    //! when the block is exhausted the search moves on to the next.
    //! Used by `WavetableIR::allocateWaveformsForFifo` after
    //! `allocateCLAligned` fails to make room for a reloading waveform.
    //!
    //! \param size      Allocation size in bytes.
    //! \param usedAddrs Cache-line indices already occupied by waveforms
    //!                  that must remain resident; entries are
    //!                  `(addr % memorySizeInBytes_) / cacheLineSizeBytes_`.
    //! \return On success, a `MemoryBlock` with bit 0 of `flags` set
    //!         (the `crossesCacheLine` bit is *not* set on this path);
    //!         on failure, `{0, 0, 0}`.
    // Allocate avoiding cache-line conflicts with existing waveforms.
    // Builds a set of used CL indices, then finds a position with no overlap.
    // Fallback when allocateCLAligned fails.
    // INLINED — no standalone address.
    MemoryBlock allocateReloadingCL(unsigned int size,
                                    std::set<unsigned long> const& usedAddrs);

    //! \brief Drive the shared free-list scan with a strategy-specific
    //!        placement predicate.
    //!
    //! \details Iterates `freeBlocks_` in order, calling
    //! `pred(block.start, block.end)` for each free block.  When
    //! `pred` returns a block whose `flags & 1` is set, the consumed
    //! range is removed from the free list and the (up to) two
    //! remainders are inserted back at the same position so deque
    //! order is preserved.  When the free-list scan exhausts without a
    //! hit, one final candidate is tried at the tail region
    //! `[freeBlocks_.back().end, lastAllocEnd_)` (or
    //! `[startOffset_, lastAllocEnd_)` when the deque is empty).
    //!
    //! \tparam Pred Callable `(uint32_t blockStart, uint32_t blockEnd)
    //!              -> MemoryBlock`.
    //! \param  pred Strategy supplied by the caller; one of three
    //!              binary lambdas (single-CL fast, multi-CL general,
    //!              reloading-conflict avoidance).
    //! \return The block returned by `pred` on its first hit, or
    //!         `{0, 0, 0}` if no candidate position succeeded.
    // Template: iterate freeBlocks_ deque, apply predicate to each block.
    // Three instantiations:
    //   allocateCLAligned::lambda#1  @0x2aa960 (fast: single CL check)
    //   allocateCLAligned::lambda#2  @0x2aac80 (general: multi-CL ownership)
    //   allocateReloadingCL::lambda  @0x2ad190 (conflict avoidance)
    template <typename Pred>
    MemoryBlock allocateFirstSuitableFreeBlock(Pred pred);

    //! \brief Query whether the free list is non-empty.
    //! \return `true` if `freeBlocks_` currently holds at least one
    //!         block, `false` otherwise.
    bool hasFreeBlocks() const { return !freeBlocks_.empty(); }

    //! \brief The most recently appended free block (back of the deque).
    //! \return A copy of `freeBlocks_.back()`; undefined when the
    //!         deque is empty (caller must check `hasFreeBlocks()` first).
    MemoryBlock lastFreeBlock() const { return freeBlocks_.back(); }

    //! \brief Largest `start` address among the current free blocks.
    //! \return The maximum `start` field over `freeBlocks_`, or `0` if
    //!         the free list is empty.
    uint32_t maxFreeBlockStart() const {
        uint32_t m = 0;
        for (auto& b : freeBlocks_) if (b.start > m) m = b.start;
        return m;
    }

private:
    //! \brief Source of memory geometry (alignment, capacity, page count).
    //! \details Borrowed; not owned by the allocator.
    const DeviceConstants*  deviceConstants_;      // +0x00
    //! \brief First waveform-memory byte the allocator may hand out.
    uint32_t                startOffset_;           // +0x08
    //! \brief Upper bound for the tail-region scan; sentinel
    //!        `0xFFFFFFFF` initially so the whole post-`startOffset_`
    //!        region is treated as one tail block.
    uint32_t                lastAllocEnd_;          // +0x0C  sentinel 0xFFFFFFFF
    //! \brief Total size of the managed waveform-memory region in bytes
    //!        (mirrors `DeviceConstants::waveformMemorySize`).
    uint32_t                memorySizeInBytes_;    // +0x10
    //! \brief Cache-line size in bytes
    //!        (mirrors `DeviceConstants::waveformAlignment`).
    uint32_t                cacheLineSizeBytes_;   // +0x14
    //! \brief Maximum number of cache lines a single allocation may
    //!        consume (mirrors `DeviceConstants::cachePageCount` /
    //!        `maxBlocks`).
    uint32_t                maxBlocksPerCL_;        // +0x18
    //! \brief ABI tail padding to align the following `vector` to
    //!        eight-byte boundary; not used at runtime.
    uint32_t                pad_1C_;                // +0x1C
    //! \brief Per-cache-line ownership table.
    //! \details Element `i` holds the cache-line base address of the
    //! waveform that currently owns slot `i`, or the sentinel
    //! `0xFFFFFFFF` when the slot is free.  Sized at construction to
    //! `memorySizeInBytes_ / cacheLineSizeBytes_` entries.
    std::vector<uint32_t>   cacheLineUsage_;        // +0x20  per-CL owner; 0xFFFFFFFF=free
    //! \brief Number of cache-line slots in `cacheLineUsage_`.
    //! \details Set at construction to
    //! `memorySizeInBytes_ / cacheLineSizeBytes_`; decremented by each
    //! multi-CL allocation by the number of slots it claims.
    uint32_t                numCacheLines_;          // +0x38
    //! \brief ABI tail padding to align the following `deque` to
    //!        eight-byte boundary; not used at runtime.
    uint32_t                pad_3C_;                // +0x3C
    //! \brief Ordered list of free `[start, end)` regions.
    //! \details Maintained by `allocateFirstSuitableFreeBlock`: each
    //! successful allocation removes the consumed slice and re-inserts
    //! the (up to two) remainders in place, preserving address order.
    std::deque<MemoryBlock> freeBlocks_;            // +0x40  free block list
};
// static_assert(sizeof(MemoryAllocator) == 0x70,
//               "MemoryAllocator must be 0x70 (112) bytes");

} // namespace zhinst
