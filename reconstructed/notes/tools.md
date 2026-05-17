# SeqC toolchain (`tools/`) {#notes_tools}

\note **Toolchain reference material.** This page documents
newly-written command-line tooling (`seqcc`, `seqas`, `seqdump`)
that drives the existing public reconstructed APIs at finer-grained
boundaries than the `_seqc_compiler.so` Python entry point exposes.
Unlike the reverse-engineering reference pages in this section, it
is **not** disassembled binary behaviour — every tool stage is a
call into already-reconstructed code.  The authoritative deep-dive
lives in `tools/seqcc/DESIGN.md`; this page is the topic-organised
cross-reference entry point that indexes it alongside the rest of
`reconstructed/notes/`.

## Binaries

| Binary    | argv[0] dispatch | Purpose |
|-----------|------------------|---------|
| `seqcc`   | primary          | gcc/clang-style driver over the full SeqC pipeline. |
| `seqas`   | symlink          | Alias for `seqcc -x asm` — exposes the legacy `AWGAssembler` path (consumes `.seqasm`, emits ELF with `e_entry + 0x80`). |
| `seqdump` | symlink          | ELF inspector — decodes every section documented in `elf_format.md`.  Read-only; never invokes the compiler. |

Current driver version: `0.11.0-T10a` (see
`tools/seqcc/src/main.cpp:kSeqccVersion`).

## Pipeline mapping

The driver's `--from=<stage>` / `--to=<stage>` vocabulary mirrors the
stage boundaries documented in `pipeline.md` and implemented as
public `step*` methods on `Compiler` (9 steps) and `AWGCompiler`
(3 steps).  Stage-by-stage table with method names and binary
addresses: see `tools/seqcc/DESIGN.md` §3 and §3.1.

| `--to=<stage>` | Stops after | Underlying step method |
|---|---|---|
| `parse`     | flex/bison → `Expression`              | `Compiler::stepParse`        |
| `astgen`    | `toSeqCAst` → `SeqCAstNode`            | `Compiler::stepToSeqCAst`    |
| `lower`     | `FrontEndLoweringFacade::lower`        | `Compiler::stepLower`        |
| `preamble`  | preamble + node-tree walk              | `Compiler::stepBuildAsmPreamble` |
| `opt-pre`   | `AsmOptimize::optimizePreWaveform`     | `Compiler::stepOptPre`       |
| `prefetch`  | `Compiler::runPrefetcher`              | `Compiler::stepPrefetch`     |
| `opt-post`  | `AsmOptimize::optimizePostWaveform`    | `Compiler::stepOptPost`      |
| `unsync`    | unsync-cervino                         | `Compiler::stepUnsyncCervino`|
| `project`   | project + final checks                 | `Compiler::stepProject`      |
| `asm`       | encoded opcode bytes (`.text`)         | `AWGCompiler::stepAssembleOpcodes` |
| `check`     | device-limit enforcement               | `AWGCompiler::stepCheckLimits` |
| `link`      | ELF (default)                          | `AWGCompiler::writeToStream` |

The `--from=<stage>` surface is currently limited to `--from=asm`
(round-trip already exercised by the binary's `serializeRoundTrip`
debug flag at `compiler.cpp:574-577`).  Other resume points are
deferred — see DESIGN.md §7 and the "T6-future" entries in TODO.md.

## Source layout

```
tools/seqcc/
├── DESIGN.md                    Authoritative design document.
├── CMakeLists.txt               Single seqcc target; symlinks created at install.
├── third_party/                 CLI11 (vendored, header-only).
└── src/
    ├── main.cpp                 Entry; argv[0] dispatch (seqcc/seqas/seqdump).
    ├── options.{hpp,cpp}        CLI11 parser + AWGCompilerConfig builder.
    ├── optflags.{hpp,cpp}       `-O`/`-f` optimisation flag parser.
    ├── driver.{hpp,cpp}         SeqcDriver state machine — owns AWGCompiler,
    │                            calls step methods one at a time, captures IR.
    ├── ir_sinks.hpp             Standalone driver-side struct holding captured
    │                            IR handles (lowered AST, WavetableFront, AsmList,
    │                            assembler text).  No recon-header dependency since T10a.
    ├── stage.{hpp,cpp}          --from / --to stage enum + name table.
    ├── compile.{hpp,cpp}        Top-level compile orchestration (single path
    │                            via SeqcDriver since T10a).
    ├── dump.{hpp,cpp}           Per-stage IR rendering (JSON/text/ELF).
    ├── elf_reader.{hpp,cpp}     ELF inspection for seqdump.
    └── seqdump.{hpp,cpp}        seqdump-specific entry.
```

Build target is auto-registered by `reconstructed/CMakeLists.txt`'s
`add_subdirectory(../tools/seqcc)`; no manual source listing required.

## IR capture mechanism

Since **T10a** (2026-05) the driver captures intermediate IR
exclusively through public accessors on the recon-side `Compiler`
and `AWGCompiler` types:

- `AWGCompiler::compiler() → Compiler&` (IF-307, T6.1).
- `Compiler::ast() → shared_ptr<Node const>` (T10a).
- `Compiler::wavetable() → shared_ptr<WavetableFront const>` (T10a).
- `Compiler::asmList() → AsmList const&` (IF-307, T6.1).

The earlier `compileSeqcWithIR()` / `CompileSeqcIntrospection` /
`fillIntrospection()` scaffold (IF-301) has been retired —
`compile_seqc.{hpp,cpp}` is back to the original-binary
single-entry-point footprint.

## Related notes

- `pipeline.md` — full SeqC compilation pipeline (stages, not tools).
- `optimization_passes.md` — `-O`/`-f` flag semantics, pass bitmask.
- `elf_format.md` — section formats `seqdump` decodes.
- `seqcc_from_design.md` — `--from=<stage>` data-flow analysis (why
  most resume modes are deferred).
- `incidental_findings.md` — IF-300..IF-309 cover seqcc-related
  process notes and sanctioned recon edits (CLI11 fixes, jsonConfig
  optimization-flags key, step-method refactor, `compiler()`
  accessor, `--to=asm` semantics, placeholder grammar).

## Test gates

The toolchain has its own test surface under `tests/tools/`:

- `test_seqcc_smoke.py` — argv/exit-code smoke (4 cases).
- `test_seqcc_diff.py` — option parser + flag-precedence tests
  (46 cases).
- `test_seqcc_to.py` — `--to=<stage>` artefact validation (4 cases).
- `test_seqdump.py` — ELF decode round-trip (16 cases).

These run independently of the main `diff_test_fast.py` ELF
regression suite (1612 cases) and must stay green at every
sub-phase wrap-up.
