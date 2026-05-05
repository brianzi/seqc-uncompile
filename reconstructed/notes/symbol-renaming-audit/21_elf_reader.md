# Batch 21 — elf_reader

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 2 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 2;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/io/elf_reader.hpp`
- `reconstructed/src/io/elf_reader.cpp`

Cross-referenced for usage:
- `reconstructed/src/io/cached_parser.cpp` (only known consumer of `ElfReader`).

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `ElfException` (type) | no | high | name in binary symtab | keep current | not-misnomer |
| `ElfException::message_` | no | medium | matches usage; std-idiomatic | keep current | — |
| `ElfException::ElfException::message` | no | medium | name fits use | keep current | — |
| `ElfException::what::` (no params) | — | — | — | — | — |
| `ElfReader` (type) | no | high | name in binary symtab | keep current | not-misnomer |
| `ElfReader::formatSection_` | unsure | medium | holds `.format` only | keep current, `dotFormatSection_` | — |
| `ElfReader::ddSections_` | no | high | holds `.dd*` sections | keep current | — |
| `ElfReader::ddSectionIndex_` | unsure | low | always zero, purpose unknown | keep current, `unknownDword_` | — |
| `ElfReader::ElfReader::path` | no | medium | actually a filesystem path | keep current | — |
| `ElfReader::isElfFile::path` | no | medium | actually a filesystem path | keep current | — |
| `ElfReader::getSection::name` | no | high | ELF section name | keep current | — |
| `ElfReader::readHeader` (method) | yes | medium | scans sections, not header | name in binary symtab → keep | not-misnomer |
| `ElfReader::getCode` (method) | yes | medium | reads `.format`, not code | name in binary symtab → keep | not-misnomer |
| `ElfReader::getWaveform` (method) | unsure | low | reads `.dd[idx]` section | name in binary symtab → keep | not-misnomer |
| `ElfReader::SectionData` (type) | no | low | not in symtab; descriptive | keep current | — |
| `ElfReader::SectionData::format` | yes | medium | stores ELF section *type* | `sectionType` (medium) | — |
| `ElfReader::SectionData::data` | no | high | raw bytes | keep current | — |
| `ElfReader::Line` (type) | unsure | low | name in binary symtab | keep current | not-misnomer |
| `ElfReader::Line::addr` | unsure | low | mapped from `.linenr` rec+4 | keep current, `instruction` | — |
| `ElfReader::Line::line` | no | medium | source line number | keep current | — |
| `ElfReader::getLineMap::lines` (local) | no | medium | accumulator | keep current | — |
| `ElfReader::getLineMap::count` (local) | no | medium | record count | keep current | — |
| `ElfReader::isElfFile::stream` (local) | no | high | the ifstream | keep current | — |
| `ElfReader::isElfFile::magic` (local) | no | high | the magic word | keep current | — |
| `ElfReader::readHeader::sec`, `sec_uptr`, `name` (locals) | no | medium | descriptive | keep current | — |
| `sectionAsString::sec` (free fn param) | no | medium | the ELFIO section | keep current | — |

## 3. Detailed findings

### `ElfException` (type)  [no / high / not-misnomer]

Evidence:
- `nm --demangle _seqc_compiler.so` shows `ElfException::ElfException(...)`,
  `ElfException::~ElfException()`, `ElfException::what() const`,
  `typeinfo for ElfException`, `vtable for ElfException`.
- All five symbols live in the **global namespace** in the binary, not
  `zhinst::`.

Interpretation:
- The class name `ElfException` is encoded verbatim in the symbol table,
  including its enclosing scope (global). This is tier-1 authoritative
  per RULES §3.

Judgement:
- Name is correct; not a misnomer.

Proposals:
- keep current  (high)

Note (not part of judgement on the name, recorded per §2a as a
side-observation): the reconstruction places `ElfException` inside
`namespace zhinst`, but the binary symbol is unqualified. Scope is a
type-shape concern, not a renaming finding.

Locations consulted:
- declared: include/zhinst/io/elf_reader.hpp:75
- defined:  src/io/elf_reader.cpp:36,41,44

### `ElfReader::formatSection_`  [unsure / medium / —]

Evidence:
- include/zhinst/io/elf_reader.hpp:153 declares the field at +0x70.
- src/io/elf_reader.cpp:124-126 — assigned only when `name == ".format"`.
- src/io/elf_reader.cpp:159-171 — `getCode()` reads its raw bytes,
  size-aligned to 4.
- src/io/cached_parser.cpp:317,361,378 — the `.format` section in cached
  ELFs holds a single byte `'3'` (the cache-format version).

Interpretation:
- The field exclusively caches the section literally named `.format`.
- The header comment "section type from ELFIO header" on
  `SectionData::format` is unrelated to this field's name.
- `format` here is the literal section name, not a "data format" in any
  semantic sense. The pointer's type and role are clearly that of a
  cached `.format` section pointer.

Judgement:
- Name is technically truthful (it points at the section called
  `.format`) but the term "format" without the leading dot can mislead
  a reader into thinking the field describes a data format. Marginal.

Proposals:
- keep current        (medium)
- `dotFormatSection_` (low)

Locations consulted:
- declared: include/zhinst/io/elf_reader.hpp:153
- used:     src/io/elf_reader.cpp:124-126,159-171

### `ElfReader::ddSections_`  [no / high / —]

Evidence:
- src/io/elf_reader.cpp:134-136 — populated for any section whose name
  starts with `.dd`.
- src/io/elf_writer.cpp:167-169 — ElfWriter creates sections named
  `.dd_<wfName>` (zero-padding sections per waveform).
- src/io/elf_reader.cpp:178 — `getWaveform()` indexes into this vector.

Interpretation:
- The container holds exactly the sections whose name has the `.dd`
  prefix. The current name reflects this directly.

Judgement:
- Not a misnomer.

Locations consulted:
- declared: include/zhinst/io/elf_reader.hpp:154
- used:     src/io/elf_reader.cpp:134-136,177-178

### `ElfReader::ddSectionIndex_`  [unsure / low / —]

Evidence:
- include/zhinst/io/elf_reader.hpp:24,159 — comment says
  "ddSectionIndex_ / unused dword (zeroed by ctor; purpose unknown)".
- src/io/elf_reader.cpp:177 — used as the index into `ddSections_` in
  `getWaveform()`. Always 0 because nothing ever writes to it.
- No write site exists in the file; not exposed via setter.

Interpretation:
- Today the field is consistently used as an index, but it is never
  modified, so its real role is conjectured. The header itself flags
  uncertainty.

Judgement:
- Name is plausible but the role is unverified; "Index" suggests it
  varies, which it does not.

Proposals:
- keep current     (medium)
- `unknownDword_`  (low) — only if a future audit confirms the field is
  vestigial.

Locations consulted:
- declared: include/zhinst/io/elf_reader.hpp:159
- used:     src/io/elf_reader.cpp:177

### `ElfReader::readHeader` (method)  [yes / medium / not-misnomer]

Evidence:
- `nm --demangle` shows `zhinst::ElfReader::readHeader()` at 0x2c3850.
- src/io/elf_reader.cpp:109-138 — the method does not parse the ELF header
  (ELFIO already did that in `load(path)`); it walks the section table
  and partitions sections into `formatSection_` / `ddSections_`.

Interpretation:
- The method's actual job is "scan and classify sections", not "read the
  ELF header". The name is misleading at source-level. However, the
  identifier `readHeader` is encoded in the binary symbol table — tier-1
  authoritative per RULES §3.

Judgement:
- Name does not faithfully describe behaviour, but it is fixed by the
  original binary; out of rename scope.

Proposals:
- keep current  (high) — required by §3.

Locations consulted:
- declared: include/zhinst/io/elf_reader.hpp:149
- defined:  src/io/elf_reader.cpp:109

### `ElfReader::getCode` (method)  [yes / medium / not-misnomer]

Evidence:
- `nm --demangle` shows `zhinst::ElfReader::getCode() const` at 0x2c3bc0.
- src/io/elf_reader.cpp:159-171 — returns the raw bytes of the section
  cached as `formatSection_`, i.e. the `.format` section (which holds a
  single version byte, per cached_parser.cpp:317,361).

Interpretation:
- The method does not return executable code; it returns the contents
  of the `.format` section. The name suggests something quite
  different. However the symbol is in the binary symbol table.

Judgement:
- Misnomer at the source level, but excluded from rename per §3.

Proposals:
- keep current  (high) — required by §3.

Locations consulted:
- declared: include/zhinst/io/elf_reader.hpp:133
- defined:  src/io/elf_reader.cpp:159

### `ElfReader::getWaveform` (method)  [unsure / low / not-misnomer]

Evidence:
- `nm --demangle` shows `zhinst::ElfReader::getWaveform() const` at 0x2c3d40.
- src/io/elf_reader.cpp:175-188 — returns raw bytes of
  `ddSections_[ddSectionIndex_]`, i.e. one of the `.dd_<wfName>`
  sections.
- src/io/elf_writer.cpp:167-169 — `.dd_<name>` sections are zero-padding
  sections inserted alongside the actual waveform data.

Interpretation:
- Whether the `.dd` payload is "the waveform" or merely a padding
  alignment artefact is unclear from the recon today. The name is
  consistent with the upstream API contract regardless. Authoritative
  in the symbol table.

Judgement:
- Name is in the binary symtab; out of rename scope.

Proposals:
- keep current  (high) — required by §3.

Locations consulted:
- declared: include/zhinst/io/elf_reader.hpp:136
- defined:  src/io/elf_reader.cpp:175

### `ElfReader::SectionData::format`  [yes / medium / —]

Evidence:
- include/zhinst/io/elf_reader.hpp:122 — `std::uint32_t format = 0;
  // section type from ELFIO header`.
- src/io/elf_reader.cpp:162,180 — assigned `sec->get_type()`, i.e. the
  ELF section type (SHT_PROGBITS, SHT_NOBITS, …), an ELF-spec field
  literally named `sh_type`.

Interpretation:
- The value stored is the ELF section header `sh_type`, not a "format".
  The header comment itself says "section type". Both write sites
  confirm this.
- ELF spec terminology consistently calls this field "type", not
  "format".

Judgement:
- Misnomer: `format` does not match the spec term for what is stored.

Proposals:
- `sectionType`  (medium)
- `type`         (low) — short but ambiguous given C++'s `type` overload
- keep current   (low)

Locations consulted:
- declared: include/zhinst/io/elf_reader.hpp:122
- used:     src/io/elf_reader.cpp:162,180

### `ElfReader::Line::addr`  [unsure / low / —]

Evidence:
- include/zhinst/io/elf_reader.hpp:128 — `std::uint64_t addr;`.
- src/io/elf_reader.cpp:212 — `memcpy(&ln.addr, rec + 4, sizeof(uint64_t))`,
  i.e. reads 8 bytes from offset +4 of a 16-byte record.
- AGENTS.md / project conventions describe `.linenr` records as "pairs
  of 2×uint32 LE: `(instruction_index, line_number)`".
- `nm --demangle` shows `ElfReader::Line` referenced in instantiated
  `std::vector<ElfReader::Line>` symbols (type name authoritative).

Interpretation:
- The field reads 8 bytes where the documented record schema would
  suggest a 4-byte instruction index. Name "addr" is consistent with
  "instruction address" (`0x80000000 + index * 4` per AGENTS.md), but
  the type/offset reconstruction may itself be off. Pure naming
  judgement: "addr" is a defensible descriptor of an instruction-memory
  address, comparable to `e_entry`.

Judgement:
- Not clearly a misnomer; uncertainty here is type-shape, not name.

Proposals:
- keep current   (medium)
- `instruction`  (low) — if the field is ever proven to be a 4-byte
  index rather than a 64-bit address.

Locations consulted:
- declared: include/zhinst/io/elf_reader.hpp:128
- used:     src/io/elf_reader.cpp:212

## 4. Symbols inspected and judged routinely fine

- `ElfException::message_` — single string field used only by `what()`;
  matches its role.
- `ElfException::ElfException::message` (ctor param) — passed straight
  into the prefixed message; name fits.
- `ElfReader::ElfReader::path`, `ElfReader::isElfFile::path` — both are
  filesystem paths supplied by the caller and used as such.
- `ElfReader::getSection::name` — passed to ELF section-name comparison;
  matches ELF spec terminology.
- `ElfReader::isElfFile::stream`, `ElfReader::isElfFile::magic` —
  textbook descriptive locals.
- `ElfReader::readHeader::sec_uptr`, `sec`, `name` — straightforward
  loop variables; name reflects role.
- `ElfReader::getSection::sec_uptr`, `sec` — same pattern as above.
- `ElfReader::getCode::result`, `rawSize`, `alignedSize`, `p` — all
  describe their role transparently.
- `ElfReader::getWaveform::result`, `sec`, `rawSize`, `alignedSize`,
  `p` — same.
- `ElfReader::getLineMap::lines`, `sec`, `sec_uptr`, `data`, `size`,
  `count`, `i`, `rec`, `ln` — all routine.
- `ElfReader::SectionData::data` — vector of raw bytes; transparent.
- `ElfReader::Line::line` — source line number, populated from the
  4-byte field at record+12.
- `sectionAsString` (free function) and its parameter `sec`,
  locals `data` and `n` — descriptive and consistent with the body.

## 5. Coverage

**Fully covered:**
- All in-scope identifiers in `reconstructed/include/zhinst/io/elf_reader.hpp`
  and `reconstructed/src/io/elf_reader.cpp`.
- All `ElfReader` use-sites in `reconstructed/src/io/cached_parser.cpp`.

**Deferred:**
- Whether `ElfReader::Line::addr` is genuinely a 64-bit address or
  reflects an erroneous 8-byte read of two 4-byte fields. Resolving
  that is a type/layout question, not a renaming question, and would
  belong to a separate audit.
- Whether `ElfReader::ddSectionIndex_` is truly an index or a flag of
  some sort: cannot decide from the always-zero state.

**Not covered (out of scope per RULES §2/§3):**
- The class names `ElfReader`, `ElfException`, and the nested type
  `ElfReader::Line` — verbatim in the binary symbol table → tier-1
  excluded; recorded as `not-misnomer` in §2 / §3.
- Method names `ElfReader::ElfReader`, `~ElfReader`, `isElfFile`,
  `readHeader`, `getSection`, `getCode`, `getWaveform`, `getLineMap`,
  `ElfException::ElfException`, `~ElfException`, `what` — all in the
  binary symbol table; method names excluded per §3. Misleading-but-
  authoritative ones (`readHeader`, `getCode`) recorded for the record.
- `ELFIO::*` symbols (third-party library) — not in scope.
- Template parameters and member type aliases — not in scope per §2.
