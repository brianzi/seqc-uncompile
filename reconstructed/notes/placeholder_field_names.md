# Placeholder Field Names — Audit & Proposed Renames

Systematic inventory of all nondescript / offset-based / generic field names
remaining in reconstructed headers, organized by confidence level.

---

## Completed renames

### High confidence (all done)

| Old | New | File |
|-----|-----|------|
| `frontField1` | `useCount_` | waveform_front.hpp |
| `frontBool1` | `dirty_` | waveform_front.hpp |
| `frontBool2` | `hasDuplicate_` | waveform_front.hpp |
| `irField2` | `elfAlignment_` | waveform_ir.hpp |
| `thirdString` | `funDescrName` | waveform.hpp |
| `field_A0` | `isWaveformCmdOverride_` | asm_expression.hpp |
| `pad_` | `ddSectionIndex_` | elf_reader.hpp |

### Medium confidence (done)

| Old | New | File |
|-----|-----|------|
| `str2` | `nopComment` | asm_expression.hpp |
| `flags_88_` | `scopeBoundaryFlags_` | resources.hpp |

---

## Still unknown (insufficient evidence to name)

*All placeholder fields resolved as of Phase 31f.* See "Completed renames" above.

### WaveformFile (`waveform.hpp`) — Phase 31f

| Old | New | File | Evidence |
|-----|-----|------|----------|
| `field18` | `formatType` | waveform.hpp | 0=auto-detect, 1=AWG integer, 2=multi-column float. Set in csv_parser.cpp format auto-detection. |
| `field1C` | `columnMode` | waveform.hpp | Single vs multi value per line. Init 0, forced to 1 in IR conversion. |
| `field20` | `isIntegerFormat` | waveform.hpp | 0=float, non-zero=AWG integer. Checked throughout csv_parser.cpp. |

### AWGCompilerConfig (`awg_compiler_config.hpp`) — Phase 31f

| Old | New | File | Evidence |
|-----|-----|------|----------|
| `unknown_28` | `serializeRoundTrip` | awg_compiler_config.hpp | ==1 triggers AsmList serialize+deserialize round-trip (debug flag). |
| `string_30`/`string_30_owned` | `debugDumpPath`/`debugDumpEnabled` | awg_compiler_config.hpp | Path for dumping serialized ASM; set from filename. |
| `string_50`/`string_50_owned` | `debugJsonPath`/`debugJsonEnabled` | awg_compiler_config.hpp | JSON output path; parallel to debugDumpPath. |
| `unknown_88` | `optimizationFlags` | awg_compiler_config.hpp | Bitmask passed to AsmOptimize ctor. 0xFF=all passes. |
| `unknown_98` | `channelGrouping` | awg_compiler_config.hpp | Already renamed in prior phase. |

---

## True padding / ABI internals (no rename needed)

These are correctly identified as padding or libc++ ABI fields:

- `WaveformGenerator::ddSectionIndex_78_` — 8B gap, no reads
- `WaveformGenerator::reserved_B0_` — dead shared_ptr, no setter in binary
- `MemoryAllocator::ddSectionIndex_1C_`, `ddSectionIndex_3C_` — alignment padding
- `AWGAssemblerImpl::ddSectionIndex_memOffset_`, `ddSectionIndex_currentLine_` — alignment
- `CustomFunctions::unusedStringSet_B0_` — no consumer found
- `*_maxLoadFactor_` fields — libc++ unordered_map ABI internals
