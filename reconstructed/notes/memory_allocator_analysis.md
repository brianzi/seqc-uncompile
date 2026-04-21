# MemoryAllocator & MemoryBlock Analysis

## Source: destructor at 0x29f2d0

### Full Disassembly of ~MemoryAllocator()

```
000000000029f2d0 <zhinst::MemoryAllocator::~MemoryAllocator()>:
  29f2d0: push   %rbp
  29f2d1: mov    %rsp,%rbp
  29f2d4: push   %rbx
  29f2d5: push   %rax
  29f2d6: mov    %rdi,%rbx             # rbx = this
  29f2d9: add    $0x40,%rdi            # rdi = this + 0x40
  29f2dd: call   2a0cf0                # deque<MemoryBlock>::~deque()
  29f2e2: mov    0x20(%rbx),%rdi       # rdi = this->vector.data
  29f2e6: test   %rdi,%rdi
  29f2e9: je     29f301                # if (data == nullptr) skip
  29f2eb: mov    %rdi,0x28(%rbx)       # this->vector.end = this->vector.data
  29f2ef: mov    0x30(%rbx),%rsi       # rsi = this->vector.capacity_end
  29f2f3: sub    %rdi,%rsi             # rsi = capacity_end - data = alloc size
  29f2f6-29f2fc: jmp operator delete   # operator delete(data, size)
  29f301-29f307: ret
```

### Destruction Order

1. **deque\<MemoryBlock\>** at offset +0x40 (destroyed first via explicit call)
2. **vector** at offset +0x20 (trivially-destructible elements, just free buffer)
3. Scalar fields need no destruction

### deque\<MemoryBlock\>::~deque() at 0x2a0cf0

Key observations from the deque destructor:
- Each buffer block freed with `mov $0xffc,%esi` → allocation = **0xFFC = 4092 bytes**
- Block size parameter = 341 (visible in `imul $0x155` and `mov $0x155` in insert/iterator code)
- 341 × 12 = 4092, confirming **sizeof(MemoryBlock) = 12**
- Special handling for 1 remaining block (sets size 0xAA = 170) and 2 remaining (0x155 = 341)
  - These are `__start_` adjustments: 170 = 341/2 (half-block offset for begin iterator)

## MemoryBlock Layout (12 bytes)

```
struct MemoryBlock {
    uint32_t start;     // +0x00  start address of block
    uint32_t end;       // +0x04  end address (start + size)
    uint32_t flags;     // +0x08  bit 0 = valid, bit 8 = crossesCacheLine
};
```

Return convention from allocateFirstSuitableFreeBlock:
- `rax` = `start | (end << 32)` — low 32 = start, high 32 = end
- `dl` = flags — bit 0 = valid, bit 8 = crossesCacheLine

Consumer in WavetableIR::allocateWaveformsForFifo (at 0x2aa874):
- `WaveformIR+0x4C` (memoryOffset) ← block.start
- `WaveformIR+0xDA` (crossesCacheLine) ← flags bit 8

Evidence for field roles:
- `allocateFirstSuitableFreeBlock` at 0x2aa960 accesses elements via
  `lea (%rdx,%rdx,2),%rax; lea (%r15,%rax,4),%rbx` → stride = 12
- lambda#2 at 0x2accd0 does alignment math with cache-line sizes,
  suggesting offset+size model for free-block tracking

### Computation confirming 12 bytes

The deque uses explicit `__block_size = 341`:
- libc++ default: `__block_size = max(4096 / sizeof(T), 16)` when not specified
- Here `4096 / sizeof(T)` rounded down = 341 → `sizeof(T) = floor(4096/341) = 12`
- Actual buffer = `341 * 12 = 4092 = 0xFFC` (confirmed in deque dtor)

## MemoryAllocator Layout (0x70 = 112 bytes)

```
Offset  Size  Type                     Field / Notes
------  ----  -----------------------  ----------------------------------
+0x00   8     DeviceConstants*         deviceConstants_ (source of memory geometry)
+0x08   4     uint32_t                 startOffset_ (where allocation begins)
+0x0C   4     uint32_t                 lastAllocEnd_ (0xFFFFFFFF sentinel)
+0x10   4     uint32_t                 memorySizeInSamples_ (DC->waveformMemorySize)
+0x14   4     uint32_t                 cacheLineSize_ (DC->waveformAlignment)
+0x18   4     uint32_t                 maxBlocksPerCL_ (DC->cachePageCount or maxBlocks)
+0x1C   4     (padding)
+0x20   24    vector<uint32_t>         cacheLineUsage_ (per-CL owner; 0xFFFFFFFF=free)
+0x38   4     uint32_t                 numCacheLines_ (memorySizeInSamples / cacheLineSize)
+0x3C   4     (padding)
+0x40   48    deque<MemoryBlock>       freeBlocks_ (libc++ deque = 0x30)
------
Total:  0x70 (112) bytes
```

Constructor is always inlined. Two initialization paths:
- **Normal path** (allocateWaveforms): only +0x10 through +0x38, no deque
- **FIFO path** (allocateWaveformsForFifo): full 0x70 bytes, all fields

### Field Access Evidence

| Offset | Accessed In | Instruction | Register |
|--------|------------|-------------|----------|
| +0x00 | lambda#2 (0x2acceb) | `mov (%r11),%rcx` | 64-bit ptr |
| +0x08 | allocFSFB (0x2aaba3) | `mov 0x8(%r10),%eax` | 32-bit |
| +0x0C | allocFSFB (0x2aaba7) | `mov 0xc(%r10),%r11d` | 32-bit |
| +0x10 | lambda#2 (0x2acd6e) | `mov 0x10(%r11),%esi` | 32-bit |
| +0x14 | lambda#2 (0x2acd72) | `mov 0x14(%r11),%r10d` | 32-bit |
| +0x18 | lambda#2 (0x2acd76) | `mov 0x18(%r11),%r14d` | 32-bit |
| +0x20 | dtor (0x29f2e2), lambda#2 (0x2aabde) | `mov 0x20(%r11),%rdx` | 64-bit ptr |
| +0x38 | lambda#2 (0x2acd56) | `mov 0x38(%r11),%ebx` | 32-bit |
| +0x48 | allocFSFB (0x2aa97e) | `mov 0x48(%r10),%r14` | deque.__map_.__begin_ |
| +0x50 | allocFSFB (0x2aa9aa) | `cmp %r14,0x50(%r10)` | deque.__map_.__end_ |
| +0x60 | allocFSFB (0x2aa982) | `mov 0x60(%r10),%r11` | deque.__start_ |
| +0x68 | allocFSFB (0x2aa9d0) | `add 0x68(%r10),%r11` | deque.__size_ |

## Symbols

```
0x29f2d0  MemoryAllocator::~MemoryAllocator()                   (0x38 bytes)
0x2a0cf0  deque<MemoryBlock>::~deque()                           (0xDB bytes)
0x2aa960  allocateFirstSuitableFreeBlock<lambda#1>               (0x320 bytes)
0x2aac80  allocateFirstSuitableFreeBlock<lambda#2>               (0x1F0 bytes)
0x2accd0  allocateCLAligned lambda#2::operator()                 (0x25B bytes)
0x2ad190  allocateFirstSuitableFreeBlock<allocateReloadingCL>    (0x515 bytes)
0x2aae70  deque<MemoryBlock>::insert                             (0x74A bytes)
0x2ab5c0  deque<MemoryBlock>::__add_front_capacity               (0x42B bytes)
0x2abd70  deque<MemoryBlock>::__add_back_capacity                (0x402 bytes)
0x2ab9f0  deque<MemoryBlock>::__move_and_check                   (0x373 bytes)
0x2ac180  deque<MemoryBlock>::__move_backward_and_check          (0x384 bytes)
```
