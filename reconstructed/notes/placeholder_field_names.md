# Placeholder Field Names — Audit & Proposed Renames

Systematic inventory of all nondescript / offset-based / generic field names
remaining in reconstructed headers, organized by confidence level.

---

## High confidence (semantic meaning clear from usage sites)

### WaveformFront (`waveform_front.hpp`)

| Current | Proposed | Evidence |
|---------|----------|----------|
| `frontField1` (+0xD8, int) | `useCount_` | Always init=1. `notes/frontend_lowering.md:444` calls it `refcount_or_dirty_`. Decremented in `copyWaveform` path (frontend_lowering.md:375). Bumped on cache hit in `waveform_generator.cpp:309-317` (TODO stub). Classic reference counter pattern. |
| `frontBool1` (+0xDC, bool) | `dirty_` | Set `true` after in-place numeric writes (`seqc_ast_nodes_evaluate.cpp:3246`) and marker writes (`:3271`). Already aliased via `isModified()`/`setModified()` accessors. |
| `frontBool2` (+0xDD, bool) | `hasDuplicate_` | Copied from source in copy ctor (`waveform_front.cpp:64`). Checked in `secureLoadWaveform` (`custom_functions.cpp:982`). Already aliased via `hasDuplicate()`/`setHasDuplicate()` accessors. |

### WaveformIR (`waveform_ir.hpp`)

| Current | Proposed | Evidence |
|---------|----------|----------|
| `irField2` (+0xDC, int32_t) | `elfAlignment_` | Initialized from `DeviceConstants::waveformElfAlignment` in all 3 ctors (`waveform_ir.cpp:37,59,126`). Used directly as ELF segment alignment (`elf_writer.cpp:158`: `seg->set_align(alignment)`). Negated to alignment mask in `write_waves_to_elf.cpp:146`. |

### Waveform base (`waveform.hpp`)

| Current | Proposed | Evidence |
|---------|----------|----------|
| `thirdString` (+0x40, std::string) | `funDescrName` | Comment already says `"genFunc" in JSON`. Accessor `funDescrName()` exists in `waveform_front.hpp:105`. The field name just hasn't been updated to match. |

### AsmExpression (`asm_expression.hpp`)

| Current | Proposed | Evidence |
|---------|----------|----------|
| `field_A0` (+0xA0, bool) | `isWaveformCmdOverride_` | Comment already says "isWaveformCmd override flag". Name just wasn't updated. |

### ElfReader (`elf_reader.hpp`)

| Current | Proposed | Evidence |
|---------|----------|----------|
| `pad_` (uint32_t) | `ddSectionIndex_` | Comment says "purpose unknown — possibly a flags field" but `getWaveform()` reads `ddSections_[pad_]` (line 135). It's an index into the dd-sections vector, not padding. |

---

## Medium confidence (partial semantic hints)

### AsmExpression (`asm_expression.hpp`)

| Current | Proposed | Evidence |
|---------|----------|----------|
| `str2` (std::string) | `nopComment` | Comment says "secondary string (NOP comment text)". Reasonable name but only one usage pattern observed. |

### Resources (`resources.hpp`)

| Current | Proposed | Evidence |
|---------|----------|----------|
| `flags_88_` (int16_t) | `scopeBoundaryFlags_` | Used as scope boundary flag; accessors `atScopeBoundary()`/`setAtScopeBoundary()` at lines 386/389. "flags" is generic but the accessor names confirm scope-boundary semantics. May contain other flag bits. |

### AWGAssemblerImpl (`awg_assembler_impl.hpp`)

| Current | Proposed | Evidence |
|---------|----------|----------|
| `str2_` (std::string) | ??? | Comment: "purpose TBD". No consumer reconstructed yet. |

---

## Unknown (insufficient evidence to name)

### WaveformFile (`waveform.hpp`)

| Current | Type | Evidence |
|---------|------|----------|
| `field18` (+0x18, int32_t) | ??? | Init 0 in WaveformFile ctor. Forced to 0 in IR conversion. Participates in `operator==`. |
| `field1C` (+0x1C, int32_t) | ??? | Init 0 in WaveformFile ctor. Forced to 1 in IR conversion. Participates in `operator==`. |
| `field20` (+0x20, int32_t) | ??? | Init 0 in WaveformFile ctor. Forced to 1 in IR conversion. Participates in `operator==`. |

### AWGCompilerConfig (`awg_compiler_config.hpp`)

| Current | Type | Evidence |
|---------|------|----------|
| `unknown_28` (+0x28, uint64_t) | ??? | No reconstructed consumer. May be set by AWGCompilerImpl. |
| `string_30` (+0x30, std::string) | ??? | Conditionally owned (dtor checks flag at +0x48). |
| `string_50` (+0x50, std::string) | ??? | Conditionally owned (dtor checks flag at +0x68). |
| `unknown_88` (+0x88, uint64_t) | ??? | Adjacent to debugFlags. No consumer. |
| `unknown_98` (+0x98, char[8]) | ??? | Between numCores and wavetableSize. No consumer. |

---

## True padding / ABI internals (no rename needed)

These are correctly identified as padding or libc++ ABI fields:

- `WaveformGenerator::pad_78_` — 8B gap, no reads
- `WaveformGenerator::reserved_B0_` — dead shared_ptr, no setter in binary
- `MemoryAllocator::pad_1C_`, `pad_3C_` — alignment padding
- `AWGAssemblerImpl::pad_memOffset_`, `pad_currentLine_` — alignment
- `CustomFunctions::unusedStringSet_B0_` — no consumer found
- `*_maxLoadFactor_` fields — libc++ unordered_map ABI internals
