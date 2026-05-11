# ELF Output Format {#notes_elf_format}

\note **Reverse-engineering reference material.** This page is part of
the `reconstructed/notes/` set: deep-dive technical notes for
contributors working on the reconstruction. It cites binary addresses,
opcodes, and disassembly observations directly so they remain
discoverable from the rendered site. The standard documentation-voice
rules for API briefs (no binary citations outside `\binarynote`) do
**not** apply to this page.

Three distinct ELF file variants are produced by this codebase, all
sharing the same base header settings but distinguished by `e_machine`:

| Variant | Producer | `e_machine` | Purpose |
|---------|----------|-------------|---------|
| Main compiler output | `AWGCompilerImpl::writeToStream` @0x108cc0 | 2 | Full SeqC compilation result |
| Legacy assembler output | `AWGAssemblerImpl::writeToFile` @0x10a630 | 2 | Direct-assembly pipeline (simpler) |
| Waveform cache | `CachedParser::cacheFile` @0x115b40 | 3 | Cached parsed waveform data |

## Common ELF header

All variants use: `ELFCLASS32`, `ELFDATA2LSB`, `e_type = ET_NONE`,
`e_flags = 0`. The entry point (`e_entry`) is set to `memoryOffset_`
(or `memoryOffset_ + 0x80` for the legacy assembler variant).

See `elf_reader.md` for reader-side struct layout, `getSection()`
semantics, and `ElfException` details.

---

## Main Compiler Output (e_machine = 2)

Written by `AWGCompilerImpl::writeToStream` (`awg_compiler.cpp:635-822`).
Up to 16 named sections plus per-waveform section pairs.

### Waveform sections (per waveform, conditional on wavetableIR existing)

| Section | ELF Type | Flags | Align | Content |
|---------|----------|-------|-------|---------|
| `.wf_<name>` | `SHT_PROGBITS` or `SHT_NOBITS` | `SHF_ALLOC` | waveform alignment | Raw sample data from `Signal::getRawData()`. `SHT_NOBITS` when `useMapped && reserveOnly`. |
| `.dd_<name>` | `SHT_PROGBITS` | `SHF_ALLOC` | waveform alignment | Zero-filled padding (descriptor data). Only emitted if `padSize > 0`. |

### Fixed sections

| # | Section | Type / Flags | Align | Content | Conditional? |
|---|---------|-------------|-------|---------|--------------|
| 1 | `.text` | `SHT_PROGBITS`, `SHF_ALLOC \| SHF_EXECINSTR` | 64 | Assembled opcodes (`vector<uint32_t>`) | Always |
| 2 | `.filename` | `SHT_PROGBITS`, 0 | 4 | Output filename (basename from format path) | Always |
| 3 | `.c` | `SHT_PROGBITS`, 0 | 4 | SeqC source code — zlib-compressed (level 9) or raw, depending on `config+0x9D` flag | Always |
| 4 | `.asm` | `SHT_PROGBITS`, 0 | 4 | Assembly text listing — same compression flag as `.c` | Always |
| 5 | `.linenr` | `SHT_PROGBITS`, 0 | 4 | Source-map: array of 16-byte records mapping instructions to source lines. See [`.linenr` detail in elf_reader.md](elf_reader.md#linenr-section-format). | Always |
| 6 | `.nodes` | `SHT_PROGBITS`, 0 | 4 | Packed int32 pairs (address, size) for node access list | UHFLI/UHFQA only, if nodeList non-empty |
| 7 | `.nodes_json` | `SHT_PROGBITS`, 0 | 4 | JSON-serialized node list with modes | Non-UHF devices (HDAWG, SHF*, etc.), if nodeList+modeMap exist |
| 8 | `.channels` | `SHT_PROGBITS`, 0 | 4 | Channel info from `compiler_.getChannelInfo()` — array of ints | If chanInfo non-empty |
| 9 | `.required_sample_rate` | `SHT_PROGBITS`, 0 | 4 | Single `float` (4 bytes) — device sample rate | Only if non-NaN and `compiler_.usedDeviceSampleRate()` |
| 10 | `.waveforms` | `SHT_PROGBITS`, 0 | 4 | JSON waveform index from `wavetableIR_->getJsonIndex()` | If wavetableIR exists |
| 11 | `.wavemem` | `SHT_PROGBITS`, 0 | 4 | JSON waveform memory info from `getJsonWaveformMemoryInfo()` | Always |
| 12 | `.arguments` | `SHT_PROGBITS`, 0 | 4 | JSON with "destination", "source", "waves" keys (boost::property_tree) | Always |
| 13 | `.version_json` | `SHT_PROGBITS`, 0 | 4 | JSON with "compiler" (uint), "target" (device family), optional "external_triggering", "required_options" | Always |
| 14 | `.version_bin` | `SHT_PROGBITS`, 0 | 4 | 16 bytes: `[laboneVer:4][suffix:4][addressImpl:4][zero:4]` | Always |

### Segments

| Segment | Type | Flags | Align | Contents |
|---------|------|-------|-------|----------|
| Code | `PT_LOAD` | `PF_R \| PF_X` | 64 | `.text` section at virtual address `memoryOffset_` |
| Waveform (×N) | `PT_LOAD` | `PF_W` | — | `.dd_<name>` (if present) + `.wf_<name>` at virtual address `(addressValue - padSize)` |

---

## Legacy Assembler Output (e_machine = 2)

Written by `AWGAssemblerImpl::writeToFile`
(`awg_assembler_impl_pipeline.cpp:570-609`). A simpler 4-section ELF
for the direct-assembly (non-SeqC) pipeline. Entry point is
`memoryOffset_ + 0x80`.

| # | Section | Content |
|---|---------|---------|
| 1 | `.text` | Assembled opcodes |
| 2 | `.comment` | `"ZI AWG Sequencer Compiler 1.4\n"` |
| 3 | `.filename` | Output filename (basename) |
| 4 | `.asm` | Assembly source text |

Same code/waveform segment structure as above, minus the waveform
segments and metadata sections.

---

## Waveform Cache (e_machine = 3)

Written by `CachedParser::cacheFile` (`cached_parser.cpp:287-358`).
Used to cache parsed waveform CSV/binary files to avoid re-parsing.
Distinguished from compiler output by `e_machine = 3`.

| # | Section | Content |
|---|---------|---------|
| 1 | `.format` | Single byte `'3'` (cache format version) |
| 2 | `.file_name` | Original source file path |
| 3 | `.channels` | `sampleFormat` as 4-byte int |
| 4 | `.marker_bits` | Marker bytes from input |
| 5 | `.data` | Raw sample data as `double[]` |
| 6 | `.marker` | Marker bits from `markerBitsVec` |
| 7 | `.config` | `markerBits` as decimal string (e.g. `"3"`) |

### Cache reader

`CachedParser::cacheFileOutdated()` checks `.format` and `.file_name`
to decide staleness. `CachedParser::getCachedFile()` reads `.channels`,
`.marker_bits`, `.data`, and `.marker` to reconstruct the waveform.
The `.config` section is written but not read back within the binary.

---

## Reader-side consumption summary

| Section | Read internally by | External? |
|---------|--------------------|-----------|
| `.format` | `ElfReader::readHeader` → `formatSection_`; `CachedParser::cacheFileOutdated` | `ElfReader::getCode()` |
| `.dd_<name>` | `ElfReader::readHeader` → `ddSections_[]` | `ElfReader::getWaveform()` |
| `.linenr` | `ElfReader::getLineMap()` | Yes (returns `vector<Line>`) |
| `.file_name` | `CachedParser::cacheFileOutdated` | No |
| `.channels` | `CachedParser::getCachedFile` | No |
| `.marker_bits` | `CachedParser::getCachedFile` | No |
| `.data` | `CachedParser::getCachedFile` | No |
| `.marker` | `CachedParser::getCachedFile` | No |
| All others | — | Yes (via `ElfReader::getSection(name)`) |

"External" means consumed by Python bindings / LabOne / downstream
tooling outside this binary.
