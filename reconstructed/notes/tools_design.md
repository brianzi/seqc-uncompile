# Toolchain Design {#notes_tools_design}

\note **Toolchain reference material.**  Design rationale and internal
architecture of the `seqcc` / `seqas` / `seqdump` toolchain.  For
user-facing CLI documentation see \ref notes_tools_user_guide; for
the test harness see \ref notes_tools_testing.  The authoritative
upstream design document is `tools/seqcc/DESIGN.md` in the source
tree — this page is the user-facing distillation.

[TOC]

## Motivation

The reconstructed SeqC compiler is reachable through the
`_seqc_compiler.so` Python binding's single monolithic entry point:

```python
compile_seqc(code, devtype, options, index, **kwargs) -> (elf_bytes, info_dict)
```

That is adequate for differential testing against the original
binary but inadequate for:

- **Debugging the reconstruction itself.**  When a test diff appears
  at the ELF level, the only previous ways to inspect intermediate
  state were setting the original binary's debug-flag bits
  (`AWGCompilerConfig + 0x90`) or hand-instrumenting reconstructed
  code.
- **Pipeline experimentation.**  Running only the parser, only the
  optimisation passes on a hand-written `.seqasm`, or round-tripping
  a lowered AST through JSON.
- **Tool composition.**  Wiring `seqcc` into editor pipelines, build
  systems, and `make`-style rules; sharing intermediate artefacts in
  bug reports.

`seqcc` is a stand-alone command-line driver, modelled on
`gcc` / `clang`, that exposes the compilation pipeline at the natural
stage boundaries already present in `zhinst::Compiler::compile()`
(reconstructed in `reconstructed/src/codegen/compiler.cpp` from
binary address `0x11f150`).

## Scope

### In scope

- A single executable `seqcc` with gcc-style flags.
- Symlinks `seqas` and `seqdump` dispatched on `argv[0]`.
- Stop-after-stage IR dumps (`-S`, `-E`, `--to=<stage>`).
- Target selection via `-march=<devtype>` and the dedicated
  `--sequencer` / `--samplerate` / `--index` / `--mdevopt` flags,
  producing an `AWGCompilerConfig` byte-identical to the one
  `compileSeqc()` produces for the equivalent Python invocation.
- Optimisation control via `-O0..3` and `-f<pass>` / `-fno-<pass>`.
- Differential validation: byte-equal ELFs against `compile_seqc()`
  for a curated subset of `tests/cases/manifest.json` cases — see
  \ref notes_tools_testing.
- `seqdump`: read-only ELF inspector mirroring `tests/dump_elf.py`.

### Out of scope (explicitly deferred)

- **`Expression` and `SeqCAstNode` deserialisation.**  Neither has a
  `fromJson` / parser; `--from=ast-raw` and `--from=ast-seqc` are not
  supported and would require designing a JSON schema for ~30 AST
  node types.
- **`--from=ast-lowered` and `--from=wavetable-ir`** — see the
  data-flow analysis in `reconstructed/notes/seqcc_from_design.md`
  (source tree) for the deferral reasoning.  The single shipped
  `--from=` mode is the natural round-trip cut at `--from=asm-pre`.
- **A separate `seqld` linker binary.**  The ELF writer in
  `AWGCompilerImpl::writeToStream` is tightly coupled to the
  in-memory `Compiler` and `WavetableIR` state; factoring an
  intermediate object format is a larger refactor than Phase T
  warranted.
- **Exposing every `AWGCompilerConfig` field as a flag.**  Only the
  fields the Python binding exposes are surfaced; the remainder
  (e.g. `channelsPerGroup`, `cacheType`, `addressImpl`, `isHirzel`,
  `numCores`) retain the device-derived defaults computed in
  `compileSeqc()`.
- **Re-implementing the compiler core.**  Every pipeline stage is
  reached by calling existing reconstructed code; the driver is pure
  orchestration.

## Pipeline anatomy

The driver vocabulary mirrors the stages documented in
\ref notes_pipeline and implemented in `Compiler::compile()`
(`reconstructed/src/codegen/compiler.cpp`).  Since **T5b** each
numbered stage below is a public `step*` method on `Compiler` (and
the three back-end stages are public `step*` methods on
`AWGCompiler`), so the driver calls them one at a time and inspects
the IR between calls — no callback machinery needed.

```
.seqc source
  │
  ├─[parse]      flex/bison (seqc_parse)        → shared_ptr<Expression>     ┐
  │                                                                          │
  ├─[astgen]     toSeqCAst()                    → shared_ptr<SeqCAstNode>    │
  │                                                                          │
  ├─[lower]      FrontEndLoweringFacade::lower  → LowerResult                │
  │                                                + initial AsmList         │ Compiler::compile()
  │                                                                          │ — 9 public step methods
  ├─[opt-pre]    AsmOptimize::optimizePreWaveform                            │
  │                 └─ deadCodeElimination             (flag bit 0x04)       │
  │                                                                          │
  ├─[prefetch]   Compiler::runPrefetcher        → WavetableIR + AsmList'     │
  │                                                                          │
  ├─[opt-post]   AsmOptimize::optimizePostWaveform                           │
  │                 ├─ oneStepJumpElimination          (flag bit 0x01)       │
  │                 ├─ removeUnusedLabels + mergeLabels (flag bit 0x02)      │
  │                 ├─ mergeRegisterZeroing            (flag bit 0x08)       │
  │                 ├─ removeUnusedRegs + registerAllocation (flag bit 0x10) │
  │                 └─ reportUserMessages              (always)              ┘
  │
  ├─[assemble]   AsmList → encoded opcode bytes (.text section)   ┐ AWGCompilerImpl
  │                                                               │ ::compileString()
  ├─[check]      device-limit enforcement (max ELF, wavemem)      │ — 3 public step
  │                                                               │   methods
  └─[link]       AWGCompilerImpl::writeToStream → ELF             ┘
```

The legacy second entry point `AWGAssembler` consumes `.seqasm` text
directly and produces an ELF whose `e_entry` is shifted by `+0x80`
relative to the main pipeline's output.  `seqas` (and `seqcc -x asm`,
when implemented) exposes that path.

### Step-method mapping

`Compiler::compile()` is a 1-line dispatcher that calls 9 public
`step*` methods in sequence (see
`reconstructed/include/zhinst/codegen/compiler.hpp`):

| Step method             | Pipeline stage(s)                      | Binary boundary |
|-------------------------|----------------------------------------|-----------------|
| `stepParse`             | parse                                  | `0x11f150`      |
| `stepToSeqCAst`         | astgen                                 | `0x11f7b0`      |
| `stepLower`             | lower                                  | `0x11f911`      |
| `stepBuildAsmPreamble`  | preamble + node-tree walk              | `0x11fb1a`      |
| `stepOptPre`            | opt-pre + `WavetableIR` build          | `0x120707`      |
| `stepPrefetch`          | prefetch                               | `0x120c92`      |
| `stepOptPost`           | opt-post                               | `0x120e2d`      |
| `stepUnsyncCervino`     | unsync-cervino                         | `0x120f2b`      |
| `stepProject`           | project + final checks                 | `0x121345`      |

`AWGCompilerImpl::compileString()` is similarly a 3-line dispatcher
calling three public `step*` forwarders on `AWGCompiler`:

| Step method            | Pipeline stage                                  |
|------------------------|-------------------------------------------------|
| `stepInnerCompile`     | wraps `Compiler::compile()` + asm-text build    |
| `stepAssembleOpcodes`  | assemble                                        |
| `stepCheckLimits`      | check                                           |

`writeToStream()` (link) remains a separate public entry point on
`AWGCompiler`.

Pass bitmask values are documented in \ref notes_optimization_passes.
The default value of `optimizationFlags` is `0xFF` (every pass on),
hardcoded in `compileSeqc()`; `seqcc` overrides it via the
`optimizationFlags` `jsonConfig` key (see incidental finding IF-305).

## IR capture mechanism

The driver captures intermediate IR exclusively through public
accessors on the recon-side `Compiler` and `AWGCompiler` types
(post-T10a, 2026-05):

```cpp
// AWGCompiler-level
AWGCompiler::compiler() -> Compiler&                  // IF-307, T6.1

// Compiler-level — handles outlive the AWGCompiler destructor
Compiler::ast()         -> shared_ptr<Node const>     // T10a
Compiler::wavetable()   -> shared_ptr<WavetableFront const>  // T10a
Compiler::asmList()     -> AsmList const&             // IF-307, T6.1
```

The earlier `compileSeqcWithIR()` / `CompileSeqcIntrospection` /
`fillIntrospection()` introspection scaffold (incidental finding
IF-301) has been retired; `compile_seqc.{hpp,cpp}` is back to the
original-binary single-entry-point footprint.

## Sanctioned reconstruction edits

The toolchain has driven a small number of carefully-scoped
modifications to reconstructed code, each documented as an
incidental finding (see the source-tree `incidental_findings.md`):

| IF      | What                                                                                          |
|---------|-----------------------------------------------------------------------------------------------|
| IF-300  | seqcc: `add_option_function<std::string>->expected(1, INT_MAX)` silently drops repeated occurrences (fixed at T3a). |
| IF-305  | `optimizationFlags` `jsonConfig` key for `compileSeqc()` — additive surface; no behavioural change. |
| IF-306  | Stepwise `Compiler::compile()` / `AWGCompilerImpl::compileString()` refactor (T5b).            |
| IF-307  | `AWGCompiler::compiler()` accessor + narrow `Compiler` setters (T6).                          |
| IF-308  | `--to=asm` dumps post-pipeline asm, not the binary's natural round-trip cut.                  |
| IF-309  | Bison grammar: `placeholder_line` keyword + line-tail capture vs `placeholder` + post-`#` JSON. |

Every entry was approved by the maintainer before landing and
re-verified against the differential test suite.

## Internal architecture

### Source layout

```
tools/
└── seqcc/
    ├── DESIGN.md                       # Authoritative upstream design document.
    ├── CMakeLists.txt                  # Single seqcc target; symlinks created at install.
    ├── third_party/CLI11/              # Vendored CLI11 (header-only).
    └── src/
        ├── main.cpp                    # Entry; argv[0] dispatch (seqcc / seqas / seqdump).
        ├── options.{hpp,cpp}           # CLI11 parser + AWGCompilerConfig builder.
        ├── optflags.{hpp,cpp}          # -O / -f optimisation flag parser (pre-CLI11).
        ├── driver.{hpp,cpp}            # SeqcDriver state machine — owns AWGCompiler,
        │                               # calls step methods one at a time, captures IR.
        ├── ir_sinks.hpp                # Standalone driver-side struct holding captured
        │                               # IR handles (Node, WavetableFront, AsmList,
        │                               # assembler text).  No recon-header dependency.
        ├── stage.{hpp,cpp}             # --from / --to stage enum + name table.
        ├── compile.{hpp,cpp}           # Top-level compile orchestration (single path
        │                               # via SeqcDriver since T10a).
        ├── dump.{hpp,cpp}              # Per-stage IR rendering (JSON / text / ELF section).
        ├── elf_reader.{hpp,cpp}        # ELF inspection (shared by seqdump).
        └── seqdump.{hpp,cpp}           # seqdump-specific entry.
```

Per project policy, **no newly-written source goes under
`reconstructed/`**; that directory is reserved for reconstruction
of binary behaviour.

### Build integration

The `seqcc` target is auto-registered by
`reconstructed/CMakeLists.txt`'s
`add_subdirectory(../tools/seqcc)`; no manual source listing
required.  Build with:

```bash
cmake --build reconstructed/build --target seqcc
```

The `seqdump` and `seqas` symlinks are produced as part of the same
target.

### Driver core (`driver.cpp`)

`SeqcDriver` is a thin state machine over the existing public
reconstructed API:

1. Build an `AWGCompilerConfig` from the parsed CLI options.
2. Construct an `AWGCompiler` (which constructs an internal
   `Compiler`).
3. Call the requested `step*` methods in sequence, stopping at the
   first stage that matches `--to=<stage>` (or running through to
   `writeToStream` for `--to=link`).
4. After each stage, capture the IR handles that were just produced
   via the public accessors.
5. Render the captured IR (or the final ELF) to the requested
   output.

The driver owns nothing the compiler doesn't already own; it just
holds `shared_ptr`s long enough to render them after the
`AWGCompiler` is destroyed.  Captured IR handles outlive the
compiler instance.

### `--to=` short-circuit semantics

When `--to=<stage>` is supplied, the driver branches between the
back-end step methods on `AWGCompilerImpl` (`stepInnerCompile`,
`stepAssembleOpcodes`, `stepCheckLimits`) and `writeToStream()`.
Pre-T5b.5 the same flags ran the full pipeline and then discarded
the ELF; post-T5b.5 the back-end steps are genuinely skipped.  This
is verified by `tests/tools/test_seqcc_to.py` (see
\ref notes_tools_testing).

## Versioning

The driver carries its own version string distinct from the binary
under reconstruction.  Current version: **`0.11.0-T10a`**, defined
at `tools/seqcc/src/main.cpp:kSeqccVersion`.  Version bumps land
alongside meaningful CLI or behavioural changes; the suffix
(`-T<phase>`) tracks the project phase that drove the change.

## Future work

The following are designed but not shipped; see `tools/seqcc/DESIGN.md`
§7 in the source tree for full rationale:

- `--from=ast-lowered`, `--from=wavetable-ir` — would require
  designing whole-tree JSON serialisation and a multi-input
  convention respectively.
- Per-pass dump taps (`--dump-after=`, `--dump-before=`) — design
  complete; not wired.
- Additional `--dump` kinds (`ast-raw`, `ast-seqc`, `prefetch`,
  `cache`, `resources`, per-stage `asm` cuts).
- `seqld` — separate linker binary; deferred pending a stable
  intermediate object format.
- `Expression` / `SeqCAstNode` JSON round-trip.

## Related notes

- \ref notes_tools — toolchain entry point and quick start.
- \ref notes_tools_user_guide — user-facing CLI reference.
- \ref notes_tools_testing — test harness.
- \ref notes_pipeline — full compilation pipeline.
- \ref notes_optimization_passes — `-O` / `-f` semantics.
