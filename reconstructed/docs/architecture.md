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
data, and a number of metadata sections (see [ELF Output](#sec-elf)).

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
LabOne.  A standalone CLI driver (`seqcc`) exposes the same pipeline
without a Python dependency — see [Toolchain](#sec-tools).

## Pipeline overview

`Compiler::compile()` is a 12-step pipeline.  The diagram below is the
contract; each step is detailed under [Compiler Pipeline](#sec-pipeline).

```
Source string
  │
  ├─ 1. unifyLineEndings           normalise \r\n / \r → \n
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
  ├─ 6. Linearise Node tree        deque<Node> → AsmList instructions
  │
  ├─ 7. AsmOptimize::optimizePreWaveform     dead code elimination
  │
  ├─ 8. Serialise / deserialise    canonical-form round-trip
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

## Component map

The codebase is organised by include subdirectory; each one owns a
distinct concern.  The table below maps directories to the components
they contain and the topical reference pages where their internals
are described.

| Directory  | Role | Key types | Reference |
|------------|------|-----------|-----------|
| `core/`    | Cross-cutting infrastructure: exceptions, error messages, logging. | `ZIAWGCompilerException`, `CompilerMessage`, `ErrorMessages` | [Runtime & Resources](#sec-runtime) |
| `device/`  | Device-type discrimination and per-device constants. | `DeviceType`, `DeviceConstants` | [Target Architecture](#sec-arch) |
| `infra/`   | STL adapters and small utilities (boost / libc++ shims). | various | — |
| `ast/`     | Parser-level `Expression`, typed `SeqCAstNode` hierarchy, evaluation, frontend lowering. | `Expression`, `SeqCAstNode`, `Value`, `EvalResults`, `FrontEndLoweringFacade` | [Compiler Pipeline](#sec-pipeline), [SeqC Language](#sec-language) |
| `asm/`     | Assembly representation: `AsmList`, optimisation passes, `AsmCommands` device dispatch. | `AsmList`, `AsmCommands`, `AsmCommandsImplCervino`, `AsmCommandsImplHirzel`, `AsmOptimize`, `AsmRegister` | [Compiler Pipeline](#sec-pipeline), [Target Architecture](#sec-arch) |
| `codegen/` | Pipeline driver, prefetcher, AWG backend, math sub-compiler. | `Compiler`, `Prefetch`, `AWGAssembler`, `AWGCompiler`, `MathCompiler`, `MemoryAllocator` | [Compiler Pipeline](#sec-pipeline) |
| `runtime/` | SeqC runtime model: scopes, registers, built-in functions, cache. | `Resources`, `StaticResources`, `GlobalResources`, `CustomFunctions`, `Cache`, `NodeMapData` | [Runtime & Resources](#sec-runtime), [SeqC Language](#sec-language) |
| `waveform/`| Waveforms, signals, wavetables, waveform DSL, PlayConfig. | `Waveform`, `WaveformFront`, `WaveformIR`, `Signal`, `PlayConfig`, `WaveformGenerator`, `WavetableFront`, `WavetableIR` | [Compiler Pipeline](#sec-pipeline), [SeqC Language](#sec-language) |
| `io/`      | Disk / ELF I/O: cached waveform parser, ELF reader/writer, ZI folder layout. | `CachedParser`, `ElfReader`, `ElfWriter`, `ZiFolder` | [ELF Output](#sec-elf) |

The Python extension entry point `pybind_seqc.cpp` lives directly under
`reconstructed/src/`.  It binds `compile_seqc()` and a handful of
helpers; everything else flows through `Compiler`.

# Topical Reference

The remainder of this site is organised by topic.  Each section is a
small landing area that links into the underlying reference pages.

## §1 Target Architecture {#sec-arch}

The instruction-set architecture executed by the SeqC sequencer, its
memory-mapped register file, and the per-device-family variations the
compiler must take into account.

The sequencer is a fixed-width 32-bit RISC-like core with a small,
specialised instruction set covering arithmetic, control flow, waveform
playback, marker output, special-register access, and feedback-driven
branching.  Two device families share the bulk of the ISA but differ
in capabilities and a handful of opcodes; the compiler emits a
device-tailored instruction stream from a common `AsmList`.

Each instruction is a single 32-bit word with the opcode in the high
byte and a family-specific operand layout in the low 24 bits (see
\ref notes_opcode_encoding).  Register state lives in a small bank
addressed `R0..R15`; `R0` is hard-zero on read and discards on write.
Hardware state outside the register bank — triggers, DIO, oscillator
phase, user registers, ZSync data, QA results, PRNG, frequency sweep
— is reached through a memory-mapped address space accessed by `ld`
/ `st` and their aliases `luser` / `suser` (see
\ref notes_special_registers).  The feedback path uses a dedicated
multi-source `fb` instruction (see \ref notes_fb_instruction) that
samples a ZSync / QA / DIO source and branches on the result in a
single fixed-latency operation.

The Cervino back-end targets UHFLI / UHFQA / GHFLI / VHFLI; the
Hirzel back-end targets HDAWG / SHFQA / SHFSG / SHFQC / SHFLI.  The
two share the bulk of the ISA but Hirzel adds extended waveform
commands, a dedicated unconditional jump opcode, and the feedback
path on the high-bandwidth devices.  See \ref notes_cervino_vs_hirzel
for the full capability matrix.

- \subpage notes_opcode_encoding — bit-format breakdown of every
  opcode family.
- \subpage notes_opcode_map — complete opcode→mnemonic table with
  scheduling class and frontend emitters.
- \subpage notes_fb_instruction — feedback (`fb`) instruction:
  opcode, bitfields, source enum, SeqC binding.
- \subpage notes_special_registers — memory-mapped register address
  map (`ld` / `st` / `luser` / `suser`).
- \subpage notes_cervino_vs_hirzel — capability matrix and method
  differences between the two device families.
- \subpage notes_awg_device_props — per-device-family properties
  (sample rate, memory, ports, sequencer caps).

## §2 SeqC Language {#sec-language}

The user-facing surface of the language: grammar, semantic
restrictions, and the catalogue of built-in (custom) functions that
make up the runtime API.

SeqC syntactically resembles a stripped-down C: integer and floating
arithmetic, variables, `if` / `for` / `while`, function definitions,
and built-in calls into the AWG runtime.  The language deliberately
omits a number of C constructs (no preprocessor, no C-style casts, no
pointer arithmetic — see the rejected-constructs catalogue).
Waveform-valued expressions are a first-class concept built around the
*waveform DSL* (`zeros`, `sine`, `gauss`, `add`, …).

- \subpage notes_seqc_parser_grammar — flex/bison grammar and token
  table.
- \subpage notes_seqc_language_features_excluded — SeqC constructs
  that look valid but are rejected by the compiler.
- \subpage notes_custom_functions — catalogue of SeqC built-in
  functions (`playWave`, `setTrigger`, `wait`, …) and their emit
  strategies.

The waveform-building DSL — `zeros`, `sine`, `gauss`, `add`, … — runs
at compile time inside `WaveformGenerator`.  It is documented under
the pipeline section because the DSL is part of how waveforms reach
the codegen back-end; see
[Waveform DSL](\ref notes_waveform_generator_funcmap) under §3.

## §3 Compiler Pipeline {#sec-pipeline}

The internal stages that turn parsed SeqC into sequencer machine code.

The pipeline is broadly: **parse → typed AST → frontend lowering →
linearisation → pre-waveform optimisation → prefetch → post-waveform
optimisation → emit**.  Each stage transforms a well-defined IR into
the next; the major IRs are the parser `Expression`, the typed
`SeqCAstNode` tree, the node tree built during lowering, and the
flat `AsmList` of assembler instructions that survives through
optimisation and prefetch.

- \subpage notes_pipeline — step-by-step walk of `Compiler::compile()`.
- \subpage notes_frontend_lowering — frontend lowering data model
  (`FrontEndLoweringFacade`, `EvalResults`, `Value`, `LowerResult`).
- \subpage notes_node_tree_structure — node-tree IR: link semantics
  and traversal.
- \subpage notes_asm_parser_grammar — the `.seqasm` text grammar used
  for canonical-form round-tripping (internal IR).
- \subpage notes_prefetch_scheduling — waveform-prefetch sub-pipeline.
- \subpage notes_optimization_passes — `AsmOptimize`: DCE, jump
  elimination, register zeroing, register allocation.
- \subpage notes_play_config — PlayConfig bitfield and the CWVF
  register that `playWave`-family builtins update.
- \subpage notes_waveform_generator_funcmap — waveform-building DSL
  (`zeros`, `sine`, `gauss`, `add`, …).
- \subpage notes_magic_numbers_proposal — catalogue of unnamed integer
  fields with proposed enums / named constants.

## §4 Runtime & Resources {#sec-runtime}

Compile-time models of runtime-side state: device constants surfaced
to the program, and the logging / tracing surface.

The `Resources` family (`Resources` → `StaticResources` →
`GlobalResources`) is the symbol-table and scope-tracking layer.
Device-specific built-in constants (clock rates, register addresses,
opcode availability bits) are injected via `StaticResources` from
`DeviceConstants`, so a SeqC program can reference them by name.

- \subpage notes_device_constants — per-device constants surfaced into
  the SeqC namespace.
- \subpage notes_logging_tracing — logging / tracing surface
  (boost.log and the stubbed OpenTelemetry layer).

## §5 ELF Output {#sec-elf}

The compiler's product is an **ELF32 little-endian** container —
**not** a standard executable but a custom firmware bundle.  Three
related ELF variants are produced (main compiler output, legacy
assembler output, and a waveform cache); the reader counterpart is
used by LabOne to load programs onto a device.

| Section | Format | Content |
|---------|--------|---------|
| `.text` | binary, 4-byte LE words | Sequencer machine code. |
| `.asm` | text (optionally zlib-compressed) | Assembly listing of `.text`. |
| `.linenr` | binary, 16-byte records | Source map: per-instruction `(addr, lineNumber)` records. |
| `.waveforms` | JSON | Per-waveform metadata index. |
| `.wf_<name>` | binary | Raw waveform sample bytes. |
| `.wavemem` | JSON | Wave-memory accounting. |
| `.channels` | binary | Channel-info array (conditional). |
| `.nodes_json`, `.arguments`, `.version_json`, `.version_bin` | JSON / packed binary | Header / configuration metadata. |

`e_entry` encodes the sequencer instruction-memory entry point.

- \subpage notes_elf_format — authoritative schema of every section,
  for all three ELF variants.
- \subpage notes_elf_reader — `ElfReader` / `ElfWriter`, including the
  `.linenr` record format.

## §6 Toolchain {#sec-tools}

The reconstructed compiler is reachable as a stand-alone command-line
toolchain (no Python required) — a single binary `seqcc` providing
gcc/clang-style staged compilation, plus two symlinks `seqas` and
`seqdump` that dispatch on `argv[0]` to the legacy assembler path and
the ELF inspector respectively.

| Binary    | Purpose |
|-----------|---------|
| `seqcc`   | Full SeqC pipeline driver.  `--from=<stage>` / `--to=<stage>` flags mirror the public `step*` methods on `Compiler`. |
| `seqas`   | `seqcc -x asm` — exposes the legacy `AWGAssembler` path. |
| `seqdump` | ELF inspector.  Decodes every section listed under §5. |

- \subpage notes_tools — toolchain overview and quick start.
- \subpage notes_tools_user_guide — user-facing CLI reference.
- \subpage notes_tools_design — design rationale and internals.
- \subpage notes_tools_testing — test harness and CI gates.

The toolchain is **newly-written** code that drives only existing
public reconstructed APIs; it never reimplements compiler logic.  Its
documentation pages are owned independently of the topical reference
above.

# Conventions

## Reading the source

A few conventions used everywhere:

- Every reconstructed `.cpp` notes the binary address of each function
  it implements in a `// 0x<addr>` comment next to the definition.
- Reconstruction-only commentary uses plain `//`.  Doxygen documentation
  uses `//!` and `/*! */` (see `notes/comment_style_guide.md` §13).
- Whenever a doc comment cannot be backed by disassembly, GDB, or test
  evidence, it must use one of `\unclear` / `\verifyme` / `\binarynote`
  / `\unverifiable` rather than guess (see
  [Accuracy discipline](#accuracy-discipline) below).

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
- `\unverifiable` — hypothesis about a code path or symbol that has no
  SeqC-reachable entry, so cannot be confirmed by GDB tracing of compile
  inputs.

Each tag aggregates onto its own cross-reference page, accessible from
the *Related Pages* section of the navigation, so the entire
documentation backlog is discoverable rather than scattered.

## Project layout pointers

- `reconstructed/include/zhinst/` — public headers.
- `reconstructed/src/` — implementations.
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
