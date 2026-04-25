# Prefetch Tree Preparation — Analysis Notes

## Methods analyzed
| Method | Address range | Size |
|--------|-------------|------|
| `preparePlays()` | 0x1c8740–0x1c8870 | 0x130 |
| `prepareTree(node)` | 0x1c8870–0x1c9b30 | 0x12C0 |
| `countBranches(node)` | 0x1c9b30–0x1ca370 | 0x840 |
| `definePlaySize(node)` | 0x1ca370–0x1cb200 | 0xE90 |

## Key discoveries

### 1. Prefetch +0xB8 is maxBranches_, NOT pageSize_
- Initialized to 1 in constructor (`movl $1, 0xb8(%rbx)`)
- `countBranches()` reads/writes it as a running maximum of branch depth
  (0x1c9da6: `mov 0xb8(%rsi),%ecx`, then `cmovle`/`mov` back)
- `definePlaySize()` uses it as a divisor for memory-per-page calculation
  (0x1cab76: `divl 0xb8(%r12)`)
- Name "maxBranches_" confirmed by usage pattern: starts at 1, takes
  max(current, branchCount + branches.size() - 1)

### 2. PrefetcherNodeState layout (within hash_node)
Hash node layout (libc++ `__hash_node`):
```
+0x00: __next_ (pointer)
+0x08: __hash_ (size_t)  
+0x10: pair.first  = shared_ptr<Node> key (16 bytes)
+0x20: pair.second = PrefetcherNodeState starts here
```

PrefetcherNodeState offsets (from struct start):
| Offset | Size | Type | Field | Evidence |
|--------|------|------|-------|----------|
| +0x04 | 4 | int | branchCount | countBranches reads/writes hash_node+0x34 |
| +0x0C | 4 | int | playSize | definePlaySize writes hash_node+0x3c |
| +0x10 | 4 | int | cacheSize | Constructor writes hash_node+0x40 |
| +0x20 | 8 | AsmRegister | lengthReg | definePlaySize writes hash_node+0x50 via AsmRegister ctor |

### 3. WaveformIR field at +0xD8: `used_` (bool)
- Set to true by prepareTree when processing Play nodes with valid waves
- 0x1c9850: `movb $0x1, 0xd8(%rax)` after `getWaveformByName()`
- WaveformIR extends Waveform (0xD8 base), so +0xD8 = first byte of WaveformIR extension

### 4. PlayConfig +0x1E: skip/dynamic flag
- Node offset +0x66 = config(+0x48) + 0x1E
- definePlaySize skips Play nodes where this flag is true (0x1ca7a0)

### 5. prepareTree dispatch table
Via jump table at 0x95ae98 (16 entries, indexed by type-1):
| NodeType | Value | Handler | Action |
|----------|-------|---------|--------|
| Play | 0x01 | 0x1c8a6f | Extract wave, mark used, collectUsedWaves |
| Load | 0x02 | 0x1c8d13 | linkLoad, push next |
| (default) | 0x03 | 0x1c945c | push next |
| Branch | 0x04 | 0x1c8cac | removeBranches |
| Loop | 0x08 | 0x1c8b85 | push next + loop child |
| SetVar | 0x10 | 0x1c8db9 | expandSetVar, push next |
| Wait/Lock | 0x40 | 0x1c8e64 | (explicit cmp) insert Load nodes |
| Table | 0x200 | 0x1c93b6 | (explicit cmp) linkLoad |
| PlainLoad | 0x4000 | 0x1c8adf | (explicit cmp) collectUsedWaves |

### 6. Helper methods called
- `collectUsedWaves(node)` at 0x1d31c0
- `linkLoad(node)` at 0x1d33f0
- `removeBranches(node, stack)` at 0x1d3530
- `expandSetVar(node)` at 0x1d3af0
- `findLockedPlay(node, waveform)` at 0x1d3dd0
- `createLoad(node)` at 0x1d4a10
- `mergeLoads(parent, node)` at 0x1d5040
- `Node::remove(node)` at 0x1d4440
- `Node::insertBefore(node)` at 0x1cd860
- `WavetableIR::getWaveformByName(opt_name)` at 0x29e2b0
- `Signal::resizeSamples(size)` at 0x1dff70
- `Resources::getRegisterNumber()` at 0x1e4bb0 (static)
- `AsmRegister::AsmRegister(int)` at 0x28eb40

### 7. Algorithm summary

**preparePlays()**: Three-pass pipeline on the node tree:
1. `prepareTree` — Classifies nodes, inserts Load/PlainLoad instructions,
   handles Wait nodes by creating prefetch loads, marks waveforms as used.
2. `countBranches` — Propagates branch counts through the tree, tracking
   maximum branching depth in `maxBranches_`.
3. `definePlaySize` — For Play nodes, computes waveform sizes aligned to
   hardware granularity, determines if splitting is needed based on
   available cache memory per page, assigns length registers.

The core purpose is preparing the node tree for waveform cache allocation:
determining which waveforms need prefetching, how many cache pages each
play needs, and inserting the necessary Load instructions.
