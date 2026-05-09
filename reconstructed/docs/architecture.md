# SeqC Compiler — Reconstructed {#mainpage}

This site documents the reverse-engineered C++ reconstruction of
`_seqc_compiler.so`, the SeqC compiler shipped with Zurich Instruments
LabOne.  The reconstruction lives in `reconstructed/src/` and
`reconstructed/include/zhinst/`.  It is built into a static library
`libzhinst_seqc.a` and a Python extension module `_seqc_compiler.so`
that is differentially tested against the original binary by
`tests/diff_test_fast.py` (currently 1600/1600 passing).

[TOC]

## What the compiler does

SeqC is a small C-like sequencing language used to drive Zurich
Instruments AWG / SHF instruments.  A SeqC program describes when to
play waveforms, set node values, wait, branch on feedback, and so on.
The compiler turns SeqC source plus a device-type descriptor into an
**ELF32-LE container** holding sequencer machine code, waveform sample
data, and a number of metadata sections (see [ELF output](#elf-output)).

Single entry point, used by every caller:

```cpp
// The top-level orchestrator. See codegen/compiler.hpp.
//
// Constructs Resources / AsmCommands / WaveformGenerator / CustomFunctions
// internally, parses the source, lowers to assembly, runs the prefetcher,
// optimises, and returns the final assembly list.
zhinst::Compiler compiler(config, deviceConstants, wavetable, ...);
auto result = compiler.compile(source, lineNumber);
```

The Python binding `compile_seqc()` wraps `Compiler::compile()` and an
ELF writer to produce the `(elf_bytes, info_dict)` tuple consumed by
LabOne.

## Pipeline overview

`Compiler::compile()` is a 12-step pipeline.  The full per-step
disassembly map lives in `notes/pipeline.md`; the diagram below is the
contract.

```
Source string
  │
  ├─ 1. unifyLineEndings           normalize \r\n / \r → \n
  │
  ├─ 2. parse                      flex/bison (seqc_lex / seqc_parse)
  │       → Expression             raw parser AST
  │
  ├─ 3. toSeqCAst                  Expression → SeqCAstNode (typed AST)
  │
  ├─ 4. FrontEndLoweringFacade     virtual dispatch on SeqCAstNode
  │       → AsmList + Node tree    populated in-place
  │
  ├─ 5. Build assembly preamble    labels, placeholder load
  │
  ├─ 6. Linearize Node tree        deque<Node> → AsmList instructions
  │
  ├─ 7. AsmOptimize::optimizePreWaveform     dead code elimination
  │
  ├─ 8. Serialize / deserialize    canonical-form round-trip
  │
  ├─ 9. runPrefetcher              waveform placement (sub-pipeline)
  │
  ├─ 10. AsmOptimize::optimizePostWaveform   jump elim, label cleanup,
  │                                          register zeroing,
  │                                          register allocation,
  │                                          reportUserMessages
  │
  ├─ 11. unsyncCervino             platform-specific sync teardown
  │
  └─ 12. Build vector<Assembler>   final instruction list
```

The prefetcher (step 9) is itself a sub-pipeline that
allocates waveform memory, assigns wave indices, and emits the load
instructions that fetch waveform data into the cache before each
`playWave`.  See `notes/pipeline.md` and
`reconstructed/include/zhinst/codegen/prefetch.hpp`.

## Component map

The codebase is organised by include subdirectory; each one owns a
distinct concern.  The table below maps directories to the components
they contain and the notes file with the deep-dive details.

| Directory | Role | Key types | Notes |
|-----------|------|-----------|-------|
| `core/` | Cross-cutting infrastructure: exceptions, error messages, logging. | `ZIAWGCompilerException`, `CompilerMessage`, `ErrorMessages` | `notes/error_message_audit.md`, `notes/logging_tracing.md` |
| `device/` | Device-type discrimination and per-device constants. | `DeviceType`, `DeviceConstants` | `notes/device_type.md`, `notes/device_constants.md`, `notes/awg_device_props.md` |
| `infra/` | STL adapters and small utilities (boost / libc++ ABI shims). | various | `notes/libcpp_abi.md` |
| `ast/` | Parser-level `Expression`, typed `SeqCAstNode` hierarchy, evaluation, frontend lowering. | `Expression`, `SeqCAstNode`, `Value`, `EvalResults`, `FrontEndLoweringFacade` | `notes/frontend_lowering.md`, `notes/seqc_parser_grammar.md` |
| `asm/` | Assembly representation: `AsmList`, optimisation passes, `AsmCommands` device dispatch. | `AsmList`, `AsmCommands`, `AsmCommandsImplCervino`, `AsmCommandsImplHirzel`, `AsmOptimize`, `AsmRegister` | `notes/optimization_passes.md`, `notes/asm_parser_grammar.md`, `notes/opcode_map.md` |
| `codegen/` | Pipeline driver, prefetcher, AWG backend, math sub-compiler. | `Compiler`, `Prefetch`, `AWGAssembler`, `AWGCompiler`, `AWGCompilerConfig`, `MathCompiler`, `MemoryAllocator` | `notes/pipeline.md`, `notes/memory_allocator_analysis.md` |
| `runtime/` | SeqC runtime model: scopes, registers, built-in functions, cache. | `Resources`, `StaticResources`, `GlobalResources`, `CustomFunctions`, `Cache`, `NodeMapData` | `notes/static_resources_cervino_consts.md`, `notes/special_registers.md` |
| `waveform/` | Waveforms, signals, wavetables, waveform DSL. | `Waveform`, `WaveformFront`, `WaveformIR`, `Signal`, `PlayConfig`, `WaveformGenerator`, `WavetableFront`, `WavetableIR`, `WavetableManager<T>` | `notes/waveform_generator_funcmap.md`, `notes/bytes_vs_samples_audit.md` |
| `io/` | Disk / ELF I/O: cached waveform parser, ELF reader/writer, ZI folder layout. | `CachedParser`, `ElfReader`, `ElfWriter`, `ZiFolder` | `notes/elf_format.md`, `notes/elf_reader.md` |

The Python extension entry point `pybind_seqc.cpp` lives directly under
`reconstructed/src/`.  It binds `compile_seqc()` and a handful of
helpers; everything else flows through `Compiler`.

## Top-level type relationships

`Compiler` (in `codegen/compiler.hpp`, ~0x138 bytes) is the orchestrator.
It owns the major collaborators as `shared_ptr` members and drives them
through the pipeline.

**Compiler-owned collaborators** (constructed in `Compiler`'s ctor unless
noted; see `notes/pipeline.md` for offsets):

- `AsmCommands` — instruction emitter (`asm/`).  Pimpl with two backends:
  - **Cervino** for older FPGA devices (UHFLI, UHFQA, GHFLI, VHFLI) —
    see `notes/cervino_vs_hirzel.md`.
  - **Hirzel** for newer devices (HDAWG, SHFQA, SHFSG, SHFQC_SG, SHFLI).
- `WaveformGenerator` — waveform DSL functions (`zeros`, `sine`, `gauss`,
  `marker`, `add`, `multiply`, `mergeWaveforms`, …) registered in a
  function-pointer map.  See `waveform/`.
- `CustomFunctions` — SeqC built-ins (`playWave`, `setTrigger`, `wait`,
  `setUserReg`, …) registered in a similar map.  See `runtime/`.
- `WavetableFront` — passed in by the caller; not constructed by
  `Compiler`.  Front-end waveform table (creation, naming, loading,
  DIO tracking).
- `SeqcParserContext` — flex/bison parser state, embedded inline at
  `Compiler+0x100`.

`AsmList` is the working assembly list, embedded inline at
`Compiler+0x88`.  It is populated by the lowering pass (step 4),
extended by the linearisation pass (step 6), and mutated in place
by `AsmOptimize` (steps 7 and 10) and the prefetcher (step 9).

**Created during `compile()`**, not in the ctor:

- `WavetableIR` — IR-side waveform table (allocation, alignment, FIFO
  layout, serialisation).  Produced from `WavetableFront`.
- `Prefetch` — waveform-prefetch scheduler, instantiated by
  `runPrefetcher()`.  Owns a `Cache` (waveform cache memory model) and
  a per-node `PrefetcherNodeState` map.
- `AsmOptimize` — instantiated for each optimisation invocation.

**Symbol-table layer** (`runtime/resources.hpp`):

- `Resources` — base scope/variable/function tracking.
- `StaticResources` — adds device-specific built-in constants and the
  logger callback.  See `notes/static_resources_cervino_consts.md`.
- `GlobalResources` — adds TLS register/label counters and the
  MT19937-64 PRNG.

The two function-pointer maps in `WaveformGenerator` and
`CustomFunctions` are the canonical extension surfaces of the language;
the `notes/waveform_generator_funcmap.md` file enumerates the waveform
DSL entries with their binary addresses.

## ELF output {#elf-output}

The compiler's product is an **ELF32 little-endian** container —
**not** a standard executable but a custom firmware bundle.  Three
related ELF variants are produced (main compiler output, legacy
assembler output, waveform cache); the table below covers the main
output's most-used sections.  See `notes/elf_format.md` for the
authoritative full enumeration (including `.c`, `.filename`,
`.required_sample_rate`, `.nodes`, `.dd_<name>`, segments, and the
other two variants), and `notes/elf_reader.md` for `.linenr`'s exact
record layout.

| Section | Format | Content |
|---------|--------|---------|
| `.text` | binary, 4-byte LE words (`vector<uint32_t>`) | Sequencer machine code. |
| `.asm` | text (optionally zlib-compressed, level 9, depending on `config+0x9D`) | Assembly listing of `.text`. |
| `.linenr` | binary, **16-byte records of 4 × int32** | Source-map: one record per emitted instruction, fields `(absIdx, counter, seq, lineNumber)`.  Reader (`ElfReader::getLineMap`) packs columns 1–2 into a `uint64_t addr` and exposes `(addr, lineNumber)` to consumers.  See `notes/elf_reader.md` §"`.linenr` section format" for the full layout, including why columns 0 and 1 are always identical. |
| `.waveforms` | JSON (from `WavetableIR::getJsonIndex`) | Per-waveform metadata index (channels, marker bits, play config). |
| `.wf_<name>` | binary, format determined by `Signal::getRawData(SampleFormat)` | Raw waveform sample bytes.  Common case for AWG output is signed 16-bit LE samples (full-scale ±8191), but the encoding is `SampleFormat`-dependent and the section may be `SHT_NOBITS` for reserve-only waveforms. |
| `.wavemem` | JSON (from `getJsonWaveformMemoryInfo`) | Wave-memory accounting. |
| `.channels` | binary, array of int32 | Channel info from `compiler.getChannelInfo()`.  Conditional on `chanInfo` being non-empty. |
| `.nodes_json`, `.arguments`, `.version_json`, `.version_bin` | JSON / packed binary | Header / configuration metadata; see `notes/elf_format.md` for per-section schemas. |

`e_entry` encodes the sequencer instruction memory entry point
(typically `0x80000000 + instruction_offset`).

## Reading the source

A few conventions used everywhere:

- Every reconstructed `.cpp` notes the binary address of each function
  it implements in a `// 0x<addr>` comment next to the definition.
- Reconstruction-only commentary uses plain `//`.  Doxygen documentation
  uses `//!` and `/*! */` (see `notes/comment_style_guide.md` §13).
- Whenever a doc comment cannot be backed by disassembly, GDB, or test
  evidence, it must use one of `\unclear` / `\verifyme` / `\binarynote`
  rather than guess (see [Accuracy discipline](#accuracy-discipline)
  below).

## Accuracy discipline {#accuracy-discipline}

Every documentation claim must be backed by one of:

1. Disassembly evidence (binary address cited in the source file).
2. GDB-verified runtime behaviour.
3. Cross-reference to a `reconstructed/notes/*.md` topic file.
4. Differential test coverage in `tests/cases/manifest.json`.

When none of these apply, documentation must use one of the project's
custom Doxygen tags rather than guess:

- `\unclear` — meaning genuinely unknown; needs investigation.
- `\verifyme` — believed correct but not yet verified by GDB or test;
  the hypothesis must be stated explicitly.
- `\binarynote` — verified fact about the binary that diverges from
  idiomatic C++ (informational, not a gap).

Each tag aggregates onto its own cross-reference page, accessible from
the *Related Pages* section of the navigation, so the entire
documentation backlog is discoverable rather than scattered.

## Documentation roadmap

This site is in **Phase D1** (architecture overview — this page).
Subsequent phases:

- **D2** — `\brief` line on every public class.  `WARN_IF_UNDOCUMENTED`
  flips to `YES` and the warning log becomes the coverage tracker.
- **D3** — full documentation of pipeline-driver functions
  (`Compiler::*`, `Prefetch::*`, `AsmOptimize::*`).
- **D4** — public methods of high-traffic classes (`AsmCommands`,
  `WaveformGenerator`, `CustomFunctions`, `Resources`).
- **D5** — internal helpers; close the `\unclear` / `\verifyme`
  backlog.
- **D6** — long-form notes promoted from `notes/` to first-class
  Doxygen pages.

`TODO.md` carries the live phase plan; `reconstructed/docs/coverage.sh`
prints the current symbol-coverage percentage.

## Project layout pointers

- `reconstructed/include/zhinst/` — public headers (~63 files).
- `reconstructed/src/` — implementations (~99 files).
- `reconstructed/notes/` — architectural and forensic notes (the
  canonical reference for binary-level details).
- `reconstructed/notes/comment_style_guide.md` — the authoritative
  source-comment conventions, including §13 on documentation comments.
- `reconstructed/docs/README.md` — how to build / extend this site.
- `tests/` — differential test harness (excluded from this site).
- `AGENTS.md` — development workflow, GDB recipes, ELF section
  reference.
- `OVERVIEW.md` — class-by-class reconstruction status snapshot.

## Building this site

From `reconstructed/build/`:

```
cmake --build . --target docs
```

The HTML output is written to `reconstructed/build/docs/html/`.
A warning log is written to `reconstructed/build/docs/doxygen-warnings.log`.
Coverage progress can be inspected with `reconstructed/docs/coverage.sh`.
