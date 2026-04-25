# Prefetch::allocate — Detailed Analysis

Binary: 0x1d0fb0–0x1d20b0 (ret), exception handlers through 0x1d271d.

## Cache Allocation Strategy

### Overview
`allocate()` walks the node linked list (via `node->next`) and performs cache
memory allocation for each waveform that needs to be played. It delegates to
`Cache::allocate()` for actual memory reservation and `Cache::play()` for
marking cache entries as in-use during playback simulation.

### Type Dispatch

Jump table at `0x95af54`, indexed by `(type - 1)` for `type ≤ 8`:

| Type Value | Name (approx) | Target   | Action |
|------------|---------------|----------|--------|
| 1          | Play          | 0x1d1045 | Read waveform name, insert into nameMap_ with `true` |
| 2          | Load/SetVar   | 0x1d130d | Lock load weak_ptr, Cache::play() with load's cachePtr |
| 4          | Branch        | 0x1d117a | Iterate branches[], recurse with scoped cache |
| 8          | Loop          | 0x1d1273 | Recurse into loop body with scoped cache |
| 0x40       | Wait          | 0x1d140d | Read waveform name, insert into nameMap_ with `true` |
| 0x80       | (Rate?)       | 0x1d145a | Read waveform name, insert into nameMap_ with `false` |
| 0x200      | Table         | 0x1d10af | Lock load weak_ptr, Cache::play() with load's cachePtr |

Types 3, 5, 6, 7 all map to the default/advance target (0x1d1660) — no action.

### How Branches are Handled

For **Branch** nodes (type 4):
- Iterates `node->branches` vector (`+0xC8` begin, `+0xD0` end), stepping by
  `sizeof(shared_ptr<Node>)` = 16 bytes per entry.
- For each branch child: calls `cache->getScope()` to create a **scoped cache
  snapshot**, then recursively calls `allocate(branchChild, scopedCache)`.
- This means each branch gets its own cache scope — allocations within one
  branch don't affect others. The cache scoping mechanism (via `Cache::getScope()`)
  presumably saves/restores the cache state.

### How Loops are Handled

For **Loop** nodes (type 8):
- Reads `node->loop` (`+0xE0`). If non-null, creates a scoped cache via
  `cache->getScope()` and recursively calls `allocate(loopNode, scopedCache)`.
- Same scoped-cache pattern as branches — loop body allocation is isolated.

### How Load Nodes Work (type 2 and type 0x200)

Load/Table nodes represent pre-loaded waveforms. The algorithm:
1. Lock the node's load weak_ptr (`+0x18`/`+0x20`) to get the referenced load node.
2. Look up `nodeStates_[loadNode]` to get its `cachePtr` (at PNS+0x28, hash_node+0x48).
3. Look up `nodeStates_[curNode]` to get its `counter` (at PNS+0x10, hash_node+0x30),
   used as `Cache::PointerState`.
4. Call `cache->play(loadCachePtr, pointerState)` — this marks the cache entry as
   being played, using the counter as the playback state.

If no load pointer exists and the node's `useDA` flag (at hash_node+0x66) is not set,
throws `ZIAWGCompilerException` with error code `0xA2`.

### How Play Nodes Allocate (The Complex Path)

For **Play** nodes (type 1) that have a load reference, the allocation path
at 0x1d1560–0x1d2016 performs:

1. **Try reuse from load node** (0x1d16ca–0x1d17b3):
   - Look up `nodeStates_[loadNode].cachePtr`.
   - If exists and `counter >= 2`: transfer the cachePtr to `nodeStates_[curNode]`.

2. **Check stillInMemory** (0x1d17b3–0x1d1a19):
   - Call `cache->stillInMemory(cachePtr)` — returns bool.
   - If true: call `cache->reuse(cachePtr)`, then find original owner via
     `nodeByCachePointer(cachePtr)`, then `mergeLoads(owner, currentNode)`.
   - If false: proceed to new allocation.

3. **New allocation** (0x1d1a19–0x1d2016):
   - Iterate play vector (`+0xA0`), lock first weak_ptr to get play target.
   - Check `split_` flag (`+0xBC`) for path selection.
   - **Standard path**: 
     - Get waveform name from `wavesPerDev[deviceIndex]`.
     - `waveformIR = wavetableIR_->getWaveformByName(waveName)` (0x1d1ba7).
     - Get second WaveformIR for size computation (0x1d1c31).
     - Compute byte size from WaveformIR signal metadata:
       ```
       channels = waveformIR->channels       // +0xC8 (uint16)
       sampleCount = signal->sampleCount     // signal+0x40
       granularity = signal->granularity     // signal+0x44
       if (sampleCount > 0):
           numSamples = ceil(sampleCount / granularity) * granularity
           numSamples = max(numSamples, signal->length)
       totalBits = channels * numSamples * bitsPerSample
       byteSize = ceil(totalBits / 8)
       ```
     - Call `cache->allocate(waveformIR, byteSize, nameMap_, maxBranches_, split_)`.
     - Store result in `nodeStates_[curNode].cachePtr`.
   - **Split path** (when `split_` is true):
     - Uses `node->length` (`+0x90`) directly:
       `numSamples = length * channels * 2`.
     - Same `cache->allocate()` call.

### Key Observations

- `nodeStates_` is the central bookkeeping map: `unordered_map<shared_ptr<Node>, PrefetcherNodeState>`.
  Hash node layout: key at +0x10 (16 bytes), PNS value at +0x20. Relevant PNS fields accessed:
  - +0x10 (hash_node+0x30): `counter` — used as `Cache::PointerState` for play()
  - +0x28 (hash_node+0x48): `cachePtr` (shared_ptr<Cache::Pointer>) — the allocated cache entry
  - +0x38 (hash_node+0x58): `useDA` flag — skip allocation for DA nodes
  - +0x66 (hash_node+0x66 — NOT a PNS field, this is within the hash_node metadata): useDA alternate check

- `nameMap_` (`unordered_map<string, bool>`) tracks which waveform names have been seen.
  Play and Wait types insert with `true`, type 0x80 inserts with `false`.

- The `Cache::getScope()` pattern for branches/loops creates isolated cache
  scopes, preventing one branch's allocations from conflicting with another's.
  This is critical for branches that play different waveforms.

- Exception handling: all `std::exception` catches at 0x1d20b1, 0x1d2106,
  0x1d22c4, 0x1d2512, 0x1d25f5 wrap the exception message into
  `ZIAWGCompilerException(ErrorMessages::format(0xA2, what()))`.

### Open Questions

- What exactly is type=0x80? Possibly `NodeType::Rate` or some other marker.
  It inserts into `nameMap_` with `false` while Play/Wait use `true`.
- The `counter` field being used as `Cache::PointerState` — what states exist?
  The `play()` call signature suggests it tracks whether the cache entry is
  being read/written.
- The hash_node+0x66 offset for the useDA check at 0x1d14ee: this seems to be
  a different field than PNS+0x38. Need to verify the exact hash_node layout.
