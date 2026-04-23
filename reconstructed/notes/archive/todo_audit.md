# Phase 10.7a: TODO/APPROXIMATE Marker Audit

Total: ~174 markers across 30 files (excluding legitimate uses of "placeholder"
in the Node/AsmList domain sense).

## Category Summary

| Category | Count | Priority | Description |
|----------|-------|----------|-------------|
| A. TBD field offsets (headers) | ~65 | HIGH | Fields added to headers with "offset TBD" — no confirmed binary offset |
| B. Wrong/missing member access | ~25 | HIGH | Code references non-existent members or uses wrong types |
| C. libc++ internals / void* casts | ~12 | MEDIUM | Binary uses libc++ ABI details (weak_ptr control blocks) |
| D. Stub/pending types | ~6 | LOW | Expression, SeqCAstNode, CachedParser — awaiting Phase 11/12 |
| E. APPROXIMATE logic | ~10 | MEDIUM | Reconstructed logic uncertain (bitmasks, enum values, aliasing) |
| F. Minor / cosmetic | ~10 | LOW | Informational TODOs, deferred checks, duplicate defs |

---

## A. TBD Field Offsets in Headers (HIGH priority)

These fields were added during Phase 10.6 to make code compile, but have no
confirmed offset in the binary. Risk: they bloat struct sizes and may mask
the real field locations.

### node.hpp (32 markers — worst offender)
- Lines 247-294: **48 fields** added as "offset TBD". Many are likely aliases
  for existing confirmed fields (e.g. `playConfig` may alias `config`,
  `length2` may alias `length`, `parent_ptr` may alias the weak_ptr at +0x18).
- `load`/`loadCtrl` at lines 247-248: aliased from `_reserved0`/`_reserved1`
- **Action**: Cross-reference all prefetch/placesingle/splitplay accesses
  against known Node layout (0x110 bytes). Many TBD fields MUST map to
  existing fields — Node can't be >0x110 bytes.

### waveform_ir.hpp (11 markers)
- Lines 49-62: `numPages`, `fixed_`, `playCount`, `irBool0`, `channelCount`,
  `address`, `waveformOffset`, `sizeInBytes`, `used`, `getSampleCount()`
- WaveformIR is 0xE0 bytes (Waveform base 0xD8 + 8 bytes). The TBD fields
  may map to the base Waveform fields or the 8-byte extension.

### prefetch.hpp (11 markers)
- PrefetcherNodeState: lines 132-137 (`firstTime`, `lengthReg`, `counter`,
  `totalSize`, `usedCache`, `playSize`) — offset TBD within the 0x40-byte struct
- Prefetch class: lines 278-282 (`pageSize_`, `isHirzel_`, `npCerv`)

### awg_compiler_config.hpp (10 markers)
- Lines 88-99: `cacheSize`, `channelIndex`, `appendMode`, `splitIndex`,
  `baseGrainSize`, `channelGrains`, `syncVersion`, `seqCount`, `deviceSampleRate`
- AWGCompilerConfig is ~0xC0 bytes; these may overlap existing fields.

### waveform_front.hpp (9 markers)
- Lines 66-79: `isAllocated`, `channels`, `sampleLength`, `fileType`,
  `isModified`, `funDescrName_`, `args_`
- WaveformFront is 0xF8 = Waveform(0xD8) + 0x20 extension.

### device_constants.hpp (6 markers)
- Lines 154-162: `grainSize`, `maxWaveformLength`, `maxWaveIndex`,
  `maxDioTableEntries` — likely aliases for existing DC fields. Struct is
  0x90 bytes; these push it beyond.

### awg_assembler_impl.hpp (6 markers)
- Lines 108-127: field at 0x70, `opcodes_begin_/end_`, `sourceLines_begin_/end_`

### signal.hpp (4 markers)
- Lines 83-87: `granularity()`, `maxLength()`, `minLength()`, `bitsPerSample()`
  — convenience methods with hardcoded defaults.

### error_messages.hpp (5 markers)
- Lines 394-400: `InvalidRegister`, `UnsupportedOp`, `ValueOverflow` — placeholder
  enum values (-1, -2, -3). Real values unknown.
- Line 452: `ResourcesException` layout unconfirmed (but reconstructed in resources.hpp)

### Other headers
- `cache.hpp:112` — `pageCount` TBD
- `asm_list.hpp:63` — `lineNumber` TBD
- `asm_expression.hpp:115` — `lineNumber` TBD
- `resources.hpp:51,56` — VarSubType, EDirection values TBD
- `awg_assembler.hpp:23` — return type TBD

---

## B. Wrong/Missing Member Access (HIGH priority)

Code that references non-existent members or uses wrong types, currently
commented out or stubbed.

### asm_commands.cpp (8 markers)
- Line 286: `imm.value` doesn't exist (Immediate is variant)
- Lines 930, 948, 958: genPlayConfig/asmPlay field names wrong
  (`sampleWidth`, `markerData`, `markerDataEnd` don't exist on WaveformFront)
- Lines 1034-1036: `pack()` doesn't exist → `encodeCwvf(defaultRate)`

### prefetch_placesingle.cpp (5 markers)
- Line 52: `currentAsmId_` not a member of AsmCommands
- Lines 126-127: `loadNode` initialization unknown
- Line 607: `channelGrains` should be array access
- Line 699: `load` type mismatch (Node* vs weak_ptr)

### prefetch_prepare.cpp (5 markers)
- Line 136: weak_ptr<Node> needs lock() for wave name
- Line 161: stack/deque type mismatch for removeBranches
- Line 660: `PlayConfig::unknown_1e` not defined
- Lines 709, 725: Signal reference vs pointer confusion

### prefetch_helpers.cpp (3 markers)
- Line 223: duplicate clampToCache definition
- Line 325: `branchesModified` not a Node member
- Line 539: `slotIndex`, `data[]`, `hasValue` not Node members

### wavetable_ir.cpp (5 markers)
- Line 49: `samplesPerWave` not a member
- Line 224: `controlBlock()` not standard
- Line 433: `getUniqueName` not a member
- Line 603: `fileType` field missing on WaveformIR
- Line 617: CsvParser not declared

### wavetable_manager_ir.cpp (4 markers)
- Lines 113-129: WaveformIR ctor mismatch, Signal metadata copy

### wavetable_manager_front.cpp (1 marker)
- Line 231: `values[i]` comparison needs proper accessor

### prefetch_emit.cpp (3 markers)
- Lines 133-137: findPlaceholder return type/semantics
- Line 190: `nodeType` field on Asm

### waveform_ir.cpp (1 marker)
- Line 117: marker bits access pattern

### awg_assembler_impl_pipeline.cpp (3 markers)
- Lines 323, 433, 584: Label add, opcodes_capacity_, uint64 vs uint32

---

## C. libc++ Internals / void* Casts (MEDIUM priority)

Binary uses libc++ ABI details for weak_ptr/shared_ptr control blocks.
Reconstructed code can't replicate this portably.

### prefetch.cpp (12 markers)
- Lines 1007, 1081, 1140, 1181, 1197, 1336: `loadCtrl`/`parent_ctrl` is `void*`
- Lines 1528, 1593, 1747: libc++ internal — reinterpret_cast of control block
- Line 1598: `loadNode.reset(cur->loadPtr, ...)` aliasing constructor
- Line 2146: "also set control block"

### prefetch_splitplay.cpp (1 marker)
- Line 127: libc++ internal control block cast

**Action**: These are fundamentally non-portable. Best approach: document the
ABI pattern in notes/ and use semantic equivalents (weak_ptr::lock()).

---

## D. Stub/Pending Types (LOW priority — Phase 11/12)

- `expression.hpp`: minimal stub, pending Phase 11a
- `seqc_ast_node.hpp`: minimal stub, pending Phase 11b
- `cached_parser.hpp`: opaque 0x60 storage, pending Phase 12b

No action until those phases.

---

## E. APPROXIMATE Logic (MEDIUM priority)

### prefetch.cpp (5 markers)
- Line 475: bitmask 0x114 for Play/Branch/Loop
- Line 499: enum name ordering
- Line 511: NodeType::Play cmp $0x2
- Line 532: shared_ptr aliasing constructor
- Line 755: optional<string> waveName copy

### prefetch_splitplay.cpp (4 markers)
- Line 125: lookupNode source
- Line 172: totalLength assignment
- Line 323: cachePtr->position lookup
- Line 344: loop counter decrement

(These are the 10.5f-deferred items already tracked in TODO.md.)

---

## F. Minor / Cosmetic (LOW priority)

- `static_resources.cpp:80` — binary ordering note
- `static_resources.cpp:201` — device subtype bitmask TODO
- `static_resources.cpp:322` — hardcoded 2.4e9 from rodata
- `asm_parser_context.cpp:342` — "up to first space" string extraction
- `prefetch.cpp:344,581` — goto labels not defined
- `prefetch.cpp:1637` — duplicate case value note
- `signal.hpp:83-87` — hardcoded convenience method defaults

---

## Recommended Resolution Order

1. **node.hpp TBD fields** (32 markers) — highest impact. Cross-reference all
   consumer sites against the confirmed 0x110-byte layout. Many fields are
   duplicates. This single effort could eliminate ~40% of all markers.

2. **asm_commands.cpp field access** (8 markers) — wrong field names in core
   emission code. Needs targeted re-disassembly of genPlayConfig/asmPlay.

3. **prefetch_placesingle.cpp / prefetch_prepare.cpp** (10 markers) — wrong
   member access. Partially depends on node.hpp resolution.

4. **wavetable_ir.cpp / wavetable_manager_ir.cpp** (9 markers) — missing
   members and wrong ctors.

5. **waveform_ir.hpp / waveform_front.hpp TBD fields** (20 markers) — need
   layout confirmation from binary.

6. **awg_compiler_config.hpp TBD fields** (10 markers) — layout confirmation.

7. **device_constants.hpp aliases** (6 markers) — likely duplicates of
   existing fields.

8. **Everything else** — libc++ internals (document), APPROXIMATE (already
   tracked), stubs (Phase 11/12).
