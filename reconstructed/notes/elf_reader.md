# ElfReader / ElfWriter {#notes_elf_reader}

`ElfReader` and `ElfWriter` are the round-trip layer for the
ELF32 containers produced by the SeqC compiler.  `ElfWriter` is used
inside `AWGCompilerImpl::writeToStream` (and the legacy assembler
path) to lay out sections, segments, and the entry-point header.
`ElfReader` is consumed by LabOne and by the cached-waveform parser
to pull individual sections back out of a finalised file.

For the full enumeration of sections, segments, and per-variant
schemas see \ref notes_elf_format.  This page covers the **`.linenr`
section format** (which has no other dedicated home) and the
reader-side API.

## `.linenr` section format

The `.linenr` ELF section is a flat array of 16-byte records, one
per emitted sequencer instruction (labels and `cmd == -1` entries
are skipped).  It serves as a source map from instruction addresses
back to SeqC source lines.

### Record layout (writer side)

Each record is 4 × `int32`, pushed sequentially:

| Offset | Value              | Description                                    |
|--------|--------------------|------------------------------------------------|
| +0     | `counter + offset` | Absolute instruction index                     |
| +4     | `counter`          | Relative instruction index (dense, 0-based)    |
| +8     | `seq`              | Sequence position (1-based, sparse)            |
| +12    | `lineNumber`       | SeqC source line                               |

- `counter` increments only for real instructions (not labels, not
  skipped entries).
- `seq` starts at 1 and increments for every asm-list entry
  including labels, so it has gaps where labels appeared.  In the
  textual assembly output a label and its following instruction
  share the same source line — so `seq` looks like a "line number"
  that disagrees with the actual line-number column.
- The only call site is `getLineMap(0)`, so `offset` is always 0
  and columns 0 and 1 are always identical.  Column 0 is
  effectively dead.

### Origin of `lineNumber`

The source line number is propagated during AST evaluation:
`SeqCAstNode::type_` carries the source line → `Compiler::setLineNr()`
pushes it to `AsmCommands` and `WavetableFront` → stored on each
emitted `AsmList::Asm` entry.

### Reader side (`ElfReader::getLineMap`)

The reader reinterprets the same 16-byte records differently from
the writer: it reads offset +4 as a `uint64_t` (8 bytes), packing
columns 1 and 2 into a single value:

```
addr = (uint64_t)seq << 32 | (uint32_t)counter
```

Column 0 (offset +0) is skipped entirely.  The returned `Line`
struct is `{ uint64_t addr; uint32_t line; }`, so consumers receive
the instruction index and sequence number as a packed 64-bit
"address" — effectively a tuple of two `int32` values — plus the
source line number.

This is a public API consumed by Python / LabOne; there are no
other in-tree consumers.

## Section lookup semantics

`ElfReader::getSection(name)` walks the section list and **throws
`ElfException`** on miss (rather than returning `nullptr`).  The
caller-side convention is therefore `try { ... } catch (...) {
outdated = true; }`, used in `cached_parser.cpp` to detect
cache-format mismatches.

## See also

- \ref notes_elf_format — full ELF section / segment schema for all
  three ELF variants.
- \ref notes_pipeline — where `getLineMap` is invoked during the
  emit stage.
