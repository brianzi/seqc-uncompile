# ElfReader / ElfWriter

For a complete description of the ELF output format — all three variants,
every section, segments, and reader-side consumption — see
[elf_format.md](elf_format.md).

ElfWriter (`src/io/elf_writer.cpp`, 248 lines, ctor 0x2934a0, prepareHeader,
addCode, addData, addWaveform, writeFile ×2, setMemoryOffset) was
reconstructed in earlier phases without ambiguity. ElfReader required
significant correction in Phase 14d, documented below.

## ElfReader layout corrections

Old stub was 0x90 bytes and described the trailing region as a "raw byte
buffer". Disassembly of the ctor and dtor shows otherwise:

| Offset      | Field             | Type                                 |
|-------------|-------------------|--------------------------------------|
| +0x00..+0x6F | (base)            | private `ELFIO::elfio` (112 bytes)   |
| +0x70       | `formatSection_`  | `ELFIO::section*`                    |
| +0x78..+0x90 | `ddSections_`     | `std::vector<ELFIO::section*>`       |
| +0x90       | `ddSectionIndex_`            | `uint32_t` (zeroed by ctor; purpose TBD) |

Total size: 0x98 bytes (was 0x90).

The vector identification comes from the dtor at 0x2b18c0, which calls
`operator delete` on `[this+0x78]` after checking it against `[this+0x80]`
(end pointer) — classic libstdc++ `vector<T*>` teardown. Push pattern in
`readHeader` matches a normal `vector::push_back`.

## readHeader() sentinel reinterpretation

The original stub described the early-exit checks as
`get_section_offset() != 1` and `get_section_entry_size() != 1`. Those
accessors don't exist on `ELFIO::elfio`, and even if they did, neither
"section table file offset == 1" nor "section header entry size == 1" makes
semantic sense as a guard.

The actual vtable offsets are +0x20 and +0x30, which on `ELFIO::elfio`
correspond to `get_class()` and `get_encoding()`. Both compare against the
constant `1`, which decodes to `ELFCLASS32` (1) and `ELFDATA2LSB` (1)
respectively. This is consistent with ElfWriter only ever producing 32-bit
little-endian ELF files — anything else means a foreign producer and the
section table is conservatively ignored.

## getSection() semantics correction

Old stub claimed `getSection` returned `nullptr` on miss. The disassembly
at 0x2c4000 walks the linear section list comparing names; if no match is
found, control falls through to a string-build of `"section not found: "
+ name` followed by `__cxa_throw` of an `ElfException`. So **getSection
throws on miss**. This matches how `cached_parser.cpp` uses it (outer
`try { ... } catch (...) { outdated = true; }` blocks).

## ElfException class

- vtable @0xb08b40, typeinfo @0xb08b68.
- In the binary it inherits privately from `std::bad_exception`. The
  reconstructed source uses `public std::exception` for simplicity, matching
  the precedent set by `zhinst::Exception` earlier — public-exception API
  is what callers expect, the private inheritance is purely an
  implementation detail of how libstdc++'s exception hierarchy was
  threaded through this build.
- Single field at +0x08: SSO `std::string` containing
  `"ELF Exception: " + message`.
- The literal `"ELF Exception: "` is materialized via three `movabs`
  immediates rather than a `.rodata` reference:
  * `0x6563784520464c45` = `"ELF Exce"`
  * `0x6e6f697470656378` = `"xception"`
  * `0x203a`             = `": "`
  This is a libc++/libstdc++ string-literal-fusion optimization the
  compiler applied because the string is exactly 16 bytes and fits two
  qwords + one word.
- `(anonymous namespace)::makeMessage::exceptionType` @0x95fbd8 likely
  caches the same prefix as a static `std::string` for the empty-message
  fast path.

Method addresses:
- `ElfException(const std::string&)` @0x2c7a40
- `~ElfException()` @0x2c7b60 (full dtor)
- `~ElfException()` @0x2c35e0 (cxa cleanup variant referenced by `__cxa_throw`)
- `what() const` @0x2c7b40 — returns `message_.c_str()`

## ELFIO API gotcha — sectionAsString helper

The pre-Phase-14d cached_parser.cpp called `sec->getDataAsString()`, which
was a fictional method invented by the old ElfReader stub. Real
`ELFIO::section` exposes `get_data() : const char*` and
`get_size() : Elf_Xword` (see `/usr/include/elfio/elfio_section.hpp:53`).

Rather than expanding the boilerplate at every call site, a free helper
was added to `elf_reader.hpp`:

```cpp
inline std::string sectionAsString(const ELFIO::section* sec) {
    if (sec == nullptr) return {};
    const char* data = sec->get_data();
    const std::size_t n = static_cast<std::size_t>(sec->get_size());
    if (data == nullptr || n == 0) return {};
    return std::string(data, n);
}
```

The null-check is defensive (the binary doesn't have one because in the
binary `getSection` throws before nullptr could ever propagate). Five
call sites in `cached_parser.cpp` were converted; no other current TU
needed this pattern.

## ELFIO version-specific iteration

The host's ELFIO uses `std::vector<std::unique_ptr<section>> sections_`
exposed as `sections` (a custom Sections proxy). Iteration must use:

```cpp
for (const auto& sec_uptr : sections) {
    ELFIO::section* sec = sec_uptr.get();
    ...
}
```

A previous attempt that wrote `for (ELFIO::section* sec : sections)`
fails to compile against this ELFIO build (the iterator dereferences to
`unique_ptr<section>&`, not `section*`).

## Build verification

`cd reconstructed/build && cmake --build .` succeeds with zero warnings
after the corrections. All TUs link into `libzhinst_seqc.a` cleanly.

## `.linenr` section format

The `.linenr` ELF section is a flat array of 16-byte records, one per
emitted instruction (labels and cmd==-1 entries are skipped). It serves
as a source map from instruction addresses back to SeqC source lines.

### Record layout (writer side — `Compiler::getLineMap` @0x123660)

Each record is 4 × int32, pushed sequentially:

| Offset | Value              | Description                                    |
|--------|--------------------|------------------------------------------------|
| +0     | `counter + offset` | Absolute instruction index                     |
| +4     | `counter`          | Relative instruction index (dense, 0-based)    |
| +8     | `seq`              | Sequence position (1-based, sparse)            |
| +12    | `lineNumber`       | SeqC source line (from AsmList::Asm +0x88)     |

- `counter` increments only for real instructions (not labels, not
  skipped entries).
- `seq` starts at 1 and increments for every asm list entry including
  labels, so it has gaps where labels appeared. This is confusing
  because in the textual assembly output a label and its following
  instruction share the same source line — so `seq` looks like a
  "line number" that disagrees with the actual line number column.
- The only call site is `getLineMap(0)` (in `AWGCompilerImpl::writeToStream`
  @0x109166), so `offset` is always 0 and columns 0 and 1 are always
  identical. Column 0 is effectively dead.

### Origin of `lineNumber`

The source line number is propagated during AST evaluation:
`SeqCAstNode::type_` carries the source line → `Compiler::setLineNr()`
(@0x123640) pushes it to `AsmCommands` and `WavetableFront` → stored at
+0x88 in each `AsmList::Asm` entry when instructions are emitted.

### Reader side (`ElfReader::getLineMap` @0x2c3ef0)

The reader reinterprets the same 16-byte records differently:

```asm
2c3f80: mov  0xc(%r14,%r13,4),%eax   # 32-bit load: offset +12 → line
2c3f85: mov  0x4(%r14,%r13,4),%rcx   # 64-bit load: offset +4  → addr
```

It reads offset +4 as a `uint64_t` (8 bytes), packing columns 1 and 2
into a single value: `addr = (uint64_t)seq << 32 | (uint32_t)counter`.
Column 0 (offset +0) is skipped entirely.

The returned `Line` struct is `{ uint64_t addr; uint32_t line; }`, so
consumers receive the instruction index and sequence number as a packed
64-bit "address" — effectively a tuple of two int32 values — plus the
source line number.

This is a public API consumed by Python/LabOne; no further use exists
within this binary.

## Open items

- Field at +0x90 (`ddSectionIndex_`, uint32) is zeroed by the ctor but never read
  by any of the five reconstructed methods. Could be a flags slot for
  a method we haven't yet identified, or compiler-inserted padding. Left
  as a `uint32_t ddSectionIndex_ = 0` for layout fidelity, with a comment.
- The `~ElfReader` at 0x2b18c0 is curiously placed far from the other
  ElfReader methods (which cluster around 0x2c3000-0x2c4000). This is
  likely the ICF/identical-code-folding artefact of a virtual dtor that
  was merged with another type's trivial-vector-dtor. Not actionable.
