# SeqC Toolchain {#notes_tools}

\note **Toolchain reference material.**  This page and its three
sub-pages document the `seqcc` / `seqas` / `seqdump` command-line
toolchain — newly-written tooling that drives the existing public
reconstructed APIs at finer-grained boundaries than the
`_seqc_compiler.so` Python entry point exposes.  No tool reimplements
compiler logic.  Unlike the reverse-engineering reference pages in
this section, the content here is *not* disassembled binary
behaviour.

[TOC]

## Overview

The reconstructed SeqC compiler is reachable as a stand-alone
command-line toolchain (no Python required) since **Phase T**.  A
single binary `seqcc` provides gcc/clang-style staged compilation;
two symlinks `seqas` and `seqdump` dispatch on `argv[0]` to the
legacy `AWGAssembler` path and the ELF inspector respectively.

| Binary    | Purpose                                                                                                                          |
|-----------|----------------------------------------------------------------------------------------------------------------------------------|
| `seqcc`   | Full SeqC pipeline driver.  `--to=<stage>` flags mirror the public `step*` methods on `Compiler` (9 steps) and `AWGCompiler` (3). |
| `seqas`   | `seqcc -x asm` — legacy `AWGAssembler` path (consumes `.seqasm`, emits ELF with `e_entry + 0x80`).                               |
| `seqdump` | ELF inspector.  Decodes every section documented under \ref notes_elf_format.                                                    |

Current driver version: **`0.11.0-T10a`**.

### Why a CLI toolchain exists

The reconstructed compiler was originally reachable only through the
`_seqc_compiler.so` Python binding's monolithic
`compile_seqc(code, devtype, options, index, **kwargs) -> (elf_bytes,
info_dict)` entry point.  That is adequate for differential testing
against the original binary, but inadequate for:

- **Debugging the reconstruction itself.**  When a test diff appears
  at the ELF level, the only way to inspect intermediate state used
  to be setting the original binary's debug-flag bits or hand-
  instrumenting reconstructed code.
- **Pipeline experimentation.**  Running only the parser, only the
  optimisation passes, or capturing the lowered AST in isolation.
- **Tool composition.**  Wiring `seqcc` into editor pipelines, build
  systems, `make`-style rules, and bug reports.

`seqcc` provides those, modelled on `gcc` / `clang`, with stage
boundaries that already exist in `zhinst::Compiler::compile()` —
each stage is a public `step*` method post-T5b.

### Quick start

```bash
# Build the driver (auto-registered by reconstructed/CMakeLists.txt)
cmake --build reconstructed/build --target seqcc

# Compile a SeqC file to ELF for an HDAWG8 at 2.4 GS/s
reconstructed/build/seqcc/seqcc \
    --march=HDAWG8 --samplerate=2.4e9 \
    program.seqc -o program.elf

# Inspect the resulting ELF
reconstructed/build/seqcc/seqdump program.elf

# Stop after frontend lowering and emit the lowered AST as JSON
seqcc -E --march=HDAWG8 --samplerate=2.4e9 program.seqc -o program.ast.json

# Stop after assembly and emit .seqasm text
seqcc -S --march=HDAWG8 --samplerate=2.4e9 program.seqc -o program.asm
```

## Sub-pages

The detailed documentation is split into three companion pages:

- \subpage notes_tools_user_guide — **User guide / reference manual.**
  Man-page-style coverage of the shipped CLI: synopsis, every option,
  input and output file formats, exit codes, examples.  Read this
  first if you want to *use* the toolchain.

- \subpage notes_tools_design — **Design rationale.**  Why the
  toolchain exists, scope and non-goals, the pipeline-stage mapping
  (with binary addresses), the IR-capture mechanism, and the
  internal source layout.  Read this if you want to *modify* or
  *extend* the toolchain.

- \subpage notes_tools_testing — **Tests and CI.**  How the
  toolchain is tested (the four `tests/tools/` suites), how it
  relates to the main `diff_test_fast.py` regression suite, and how
  to add new test cases.

The full upstream design document is `tools/seqcc/DESIGN.md` in the
source tree; the pages above are the user-facing distillation.

## Related notes

- \ref notes_pipeline — full SeqC compilation pipeline (stages, not
  tools).
- \ref notes_optimization_passes — `-O` / `-f` flag semantics and the
  optimisation-pass bitmask.
- \ref notes_elf_format — section formats that `seqdump` decodes.
- \ref notes_differential_testing — the main differential-test
  harness that backstops the toolchain.
