# `seqcc` — SeqC Toolchain Driver Design

> Status: **Draft / Phase T design document.** Approved by user
> discussion; implementation begins at sub-phase T1.  Filed under
> `tools/seqcc/` rather than `reconstructed/notes/` because the
> toolchain is newly-written tooling, not reconstructed binary
> behaviour.

## 1. Motivation

The reconstructed SeqC compiler is reachable today only through the
`_seqc_compiler.so` Python binding, which exposes a single
end-to-end entry point `compile_seqc(code, devtype, options, index,
**kwargs) -> (elf_bytes, info_dict)`.  This monolithic interface is
adequate for differential testing against the original binary but
inadequate for:

- **Debugging the reconstruction itself.**  When a test diff appears
  at the ELF level, the only way to inspect intermediate state is to
  set the original binary's debug-flag bits (`AWGCompilerConfig
  +0x90`) or to hand-instrument reconstructed code.
- **Pipeline experimentation.**  There is no way to run only the
  parser, only the optimisation passes on a hand-written `.seqasm`,
  or to round-trip a lowered AST through JSON.
- **Tool composition.**  A standard CLI driver lets users wire
  `seqcc` into editor pipelines, build systems, and `make`-style
  rules, and to share intermediate artefacts in bug reports.

`seqcc` is a stand-alone command-line driver, modelled on
`gcc`/`clang`, that exposes the SeqC compilation pipeline at the
natural stage boundaries already present in
`Compiler::compile()` (reconstructed in
`reconstructed/src/codegen/compiler.cpp` from binary address
`0x11f150`).

## 2. Scope and non-goals

### In scope

- A single executable `seqcc` providing fine-grained pipeline control
  via gcc-style flags.
- Symlinks `seqas` (assembler-only entry to `AWGAssembler`) and
  `seqdump` (ELF inspector) dispatched on `argv[0]`.
- Dumping intermediate representations before / after named
  optimisation sub-passes, in formats that already have serialisers
  in the reconstructed code base.
- Reading mid-pipeline IRs back in (`--from=<stage>`) for the three
  IRs that already have `fromJson` / `deserialize` round-trip
  support: `Node` (lowered AST), `AsmList` (assembly), and
  `WavetableIR`.
- Target selection via `-march=<devtype>` and `-mtune=<key>=<val>`,
  producing an `AWGCompilerConfig` byte-identical to the one
  `compileSeqc()` produces for the equivalent Python invocation.
- Differential validation: byte-equal ELFs against
  `compile_seqc()` for a curated subset of the existing
  `tests/cases/manifest.json` cases.

### Out of scope (explicitly deferred)

- **`Expression` and `SeqCAstNode` deserialisation.**  Neither has
  an existing `fromJson` / parser.  `--from=ast-raw` and
  `--from=ast-seqc` are *not* supported in Phase T.  Adding them
  would require designing a JSON schema and writing both halves of
  the round-trip — a separate phase if a use case emerges.
- **`--from=ast-lowered` and `--from=wavetable-ir` are deferred
  beyond Phase T6.**  The data-flow analysis in
  `reconstructed/notes/seqcc_from_design.md` showed that:
  - The existing `--dump=ast-lowered` artefact is a single-node
    `Node::toJson(idMap)` blob, not a whole-tree dump; no
    whole-tree `Node` serialisation format exists outside the
    `AsmList::serialize` / `deserialize` round-trip.  Re-entering
    at this boundary would require designing and emitting that
    format on the `--to=` side first.
  - `--from=wavetable-ir` cannot be driven from a single input
    file: `stepOptPost` reads both `asmList_` and
    `compileWavetableIR_`, so the user would have to supply two
    coherent IR blobs simultaneously.
  Phase T6 keeps only `--from=asm` (the round-trip already
  exercised by the binary's `serializeRoundTrip` debug flag at
  `compiler.cpp:574-577`).  The deferred modes are tracked as
  TODO.md "T6-future" entries.
- **Separate linker binary `seqld`.**  The ELF writer in
  `AWGCompilerImpl::writeToStream` is tightly coupled to the
  in-memory `Compiler` and `WavetableIR` state; there is no
  reconstructed object-file format that could serve as the
  link-step input.  Factoring one is a larger refactor than this
  phase warrants.
- **Exposing every `AWGCompilerConfig` field as a flag.**  Only
  the fields the Python binding exposes (`samplerate`, `filename`,
  `wavepath`, `waveforms`, `sequencer`, `index`) plus a small
  optimisation-related set (`-O`, `-f`, `--loop-unroll-limit`) are
  surfaced.  The remainder (e.g. `channelsPerGroup`, `cacheType`,
  `addressImpl`, `isHirzel`, `numCores`) retain the device-derived
  defaults computed in `compileSeqc()`.
- **Re-implementing the compiler core.**  Every pipeline stage is
  reached by calling existing reconstructed code; the driver is
  pure orchestration.

## 3. Pipeline anatomy (existing, not new)

The driver vocabulary mirrors the stages already documented in
`reconstructed/notes/pipeline.md` and implemented in
`Compiler::compile()` (`reconstructed/src/codegen/compiler.cpp`).
Since **T5b** each numbered stage below is a public step method on
`Compiler` (and the three back-end stages are public step methods on
`AWGCompiler`), so the driver calls them one at a time and inspects
the IR between calls — no callback machinery needed.  See §5.4.

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
  ├─[opt-pre]    AsmOptimize::optimizePreWaveform                            │ (T5b.3, see below)
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
  │                                                               │   methods (T5b.5)
  └─[link]       AWGCompilerImpl::writeToStream → ELF             ┘
```

The legacy second entry point `AWGAssembler` consumes `.seqasm` text
directly and produces an ELF whose `e_entry` is shifted by `+0x80`
relative to the main pipeline's output (per
`reconstructed/notes/elf_format.md`).  `seqcc -x asm` (or `seqas`)
exposes that path.

### 3.1 Step-method mapping (T5b)

`Compiler::compile()` is now a 1-line dispatcher that calls 9 public
`step*` methods in sequence (see
`reconstructed/include/zhinst/codegen/compiler.hpp`):

| Step method        | Pipeline stage(s) above            | Binary boundary |
|--------------------|------------------------------------|-----------------|
| `stepParse`        | parse (steps 1–3b in source)       | `0x11f150`      |
| `stepToSeqCAst`    | astgen (5–6)                       | `0x11f7b0`      |
| `stepLower`        | lower (7–9)                        | `0x11f911`      |
| `stepBuildAsmPreamble` | preamble + node-tree walk (10–11d) | `0x11fb1a`  |
| `stepOptPre`       | opt-pre + WavetableIR build (12–13c) | `0x120707`    |
| `stepPrefetch`     | prefetch (14)                      | `0x120c92`      |
| `stepOptPost`      | opt-post (15)                      | `0x120e2d`      |
| `stepUnsyncCervino`| unsync-cervino (16)                | `0x120f2b`      |
| `stepProject`      | project + final checks (17–19b)    | `0x121345`      |

`AWGCompilerImpl::compileString()` is similarly a 3-line dispatcher
calling three public `step*` forwarders on `AWGCompiler`:

| Step method            | Pipeline stage above |
|------------------------|----------------------|
| `stepInnerCompile`     | wraps `Compiler::compile()` + asm-text build |
| `stepAssembleOpcodes`  | assemble |
| `stepCheckLimits`      | check |

`writeToStream()` (link) remains a separate public entry point on
`AWGCompiler`.

(Pass bitmask values are from `reconstructed/notes/optimization_passes.md`.
The default value of `optimizationFlags` is `0xFF` — every pass on —
hardcoded in `compileSeqc()`; seqcc overrides it via the
`optimizationFlags` `jsonConfig` key, see IF-305.)

## 4. Command-line surface

### 4.1 Synopsis

```
seqcc [options] <input>...
seqas [options] <input>...                  # alias for `seqcc -x asm`
seqdump [options] <elf>...                  # ELF inspection only
```

### 4.2 Mode flags (clang-style)

| Flag | Meaning | Equivalent `--to` |
|---|---|---|
| `-c` *(default)* | Compile to ELF. | `--to=link` |
| `-S` | Emit `.seqasm` text after `opt-post`. | `--to=opt-post` |
| `-E` | Emit lowered AST after `lower` (JSON). | `--to=lower` |
| `--to=<stage>` | Stop after `<stage>`, emit that IR. | — |
| `--from=<stage>` | Start at `<stage>`; input must be that IR's serialised form. | — |
| `-x <lang>` | Force input language: `seqc`, `asm`, `node-json`, `asmlist`, `wavetable-json`. Default: detect from extension. | — |
| `-o <path>` | Output file (`-` for stdout). Default: input basename with stage-appropriate suffix. | — |

### 4.3 Target architecture (gcc-style)

| Flag | Maps to `AWGCompilerConfig` / `compileSeqc()` field | Notes |
|---|---|---|
| `-march=<devtype>` | `deviceId` in `compileSeqc()` | One of HDAWG4, HDAWG8, SHFQA2, SHFQA4, SHFSG2, SHFSG4, SHFSG8, SHFQC, SHFLI, UHFLI, UHFQA, UHFAWG, GHFLI, VHFLI. |
| `-mtune=sequencer=<v>` | `AwgSequencerType` (`qa`, `sg`, `auto`) | SHF families only. Mirrors JSON `"sequencer"` field. |
| `-mtune=index=<n>` | `awgIndex` (4th positional arg) | Default 0. |
| `-mtune=samplerate=<hz>` | JSON `"samplerate"` → `config.deviceSampleRate` | HDAWG only; matches binary's "samplerate is relevant for HDAWG only" error otherwise. |
| `-mtune=options=<csv>` | `options` (5th positional arg); CSV joined with `\n` | Maps to LabOne device-options string. |

#### 4.3.1 Cervino / Hirzel selection

The Cervino vs Hirzel split is a *property of the device's
`AwgDeviceProps`*, not an independent device.  It is set by
`dispatchGetAwgDeviceProps()` in `compile_seqc.cpp` based on the
devtype.  By default `seqcc` derives `isHirzel` from `-march` exactly
as `compileSeqc()` does — no flag needed.

A `--mtune=variant=cervino|hirzel` override is provided for
debugging the reconstruction (e.g. when a Hirzel-default device
needs to be forced through the Cervino code path).  Using it emits a
warning to stderr because it can produce config combinations the
binary itself never produces; not for production use.

### 4.4 Optimisation flags

| Flag | Semantics |
|---|---|
| `-O0` | `optimizationFlags = 0x00`. Every pass disabled. |
| `-O1` | `optimizationFlags = 0x05` (`deadCodeElimination` + `oneStepJumpElimination`). |
| `-O2` *(default)* | `optimizationFlags = 0xFF`. Matches binary. |
| `-fdead-code-elim` / `-fno-dead-code-elim` | Toggle bit `0x04`. |
| `-fjump-elim` / `-fno-jump-elim` | Toggle bit `0x01`. |
| `-flabel-cleanup` / `-fno-label-cleanup` | Toggle bit `0x02` (removeUnusedLabels + mergeLabels). |
| `-freg-zero-merge` / `-fno-reg-zero-merge` | Toggle bit `0x08`. |
| `-freg-alloc` / `-fno-reg-alloc` | Toggle bit `0x10`. |
| `--loop-unroll-limit=<n>` | `config.loopUnrollLimit`. Default 131072 (`0x20000`), matches binary. |

Naming follows the user-visible pass names from
`reconstructed/notes/optimization_passes.md`.  The full pass list and
their bit positions are printed by `seqcc --print-passes`.

### 4.5 Inputs and search paths

| Flag | Equivalent JSON field in `compileSeqc()` |
|---|---|
| `--wave-path=<dir>` | `"wavepath"` (overrides default `<labone-data>/awg/waves`) |
| `--waveforms=<a;b;c>` | `"waveforms"` (`;`-separated pre-compiled waveform paths) |
| `--debug-dump-path=<path>` | `"filename"` → `config.debugDumpPath` |

### 4.6 Dumps

```
--dump=<artifact>[:path]      repeatable; emit IR or report
--dump-after=<pass>           repeatable; emit asm after named pass
--dump-before=<pass>          repeatable; emit asm before named pass
--dump-dir=<dir>              destination dir (default: alongside -o)
--dump-format=auto|json|text  format hint (auto = native)
```

**Artifact catalogue:**

| Artifact name | Source method | Default format | Round-trippable? |
|---|---|---|---|
| `ast-raw` | `Expression::print()` (captured stdout) | text tree | ❌ |
| `ast-seqc` | `SeqCAstNode::print()` (captured stdout) | text tree | ❌ |
| `ast-lowered` | `Node::toJson(idMap)` (0x264b90) | JSON | ✅ |
| `asm` | `AsmList::serialize()` (0x2646d0) | text DSL | ✅ |
| `asm-pre-opt` | `AsmList::serialize()` *before* `optimizePreWaveform` | text DSL | ✅ |
| `asm-post-pre-opt` | `AsmList::serialize()` *after* `optimizePreWaveform`, before `runPrefetcher` | text DSL | ✅ |
| `asm-post-prefetch` | `AsmList::serialize()` *after* `runPrefetcher` | text DSL | ✅ |
| `asm-final` | `AsmList::serialize()` *after* `optimizePostWaveform` | text DSL | ✅ |
| `wavetable-ir` | `WavetableIR::toJson()` (0x29db10 forward) | JSON | ✅ |
| `prefetch` | `Prefetch::print(node, indent)` (0x1c5dd0) | text | ❌ |
| `cache` | `Cache::print()` (0x283b50) | text | ❌ |
| `resources` | `Resources::print()` (0x1ebbe0) | text | ❌ |
| `compile-report` | `AWGCompiler::getCompileReport()` + wavemem JSON | JSON (same shape as Python `info_dict`) | n/a |
| `elf-sections` | `seqdump` logic; mirrors `tests/dump_elf.py` | text listing | n/a |
| `all` | All of the above where applicable to the chosen target. | — | — |

For text-only `print()` methods that currently take no argument and
write to `std::cout`, sub-phase T4 adds a `print(std::ostream&)`
overload on the existing reconstructed types and re-implements the
old zero-arg `print()` as `print(std::cout)`.  This is the second of
two minimally-invasive additions to reconstructed code.

**Pass names** accepted by `--dump-after`/`--dump-before` (from
`asm_optimize.cpp` and `asm_optimize_regalloc.cpp`):

```
dead-code-elim          deadCodeElimination
jump-elim               oneStepJumpElimination
remove-unused-labels    removeUnusedLabels
merge-labels            mergeLabels
merge-reg-zeroing       mergeRegisterZeroing
remove-unused-regs      removeUnusedRegs
register-alloc          registerAllocation
split-const-regs        splitConstRegisters       (fallback path on alloc failure)
report-user-messages    reportUserMessages
prefetch.prepare        Prefetch::preparePlays
prefetch.place          Prefetch::placeLoads
prefetch.fill           Prefetch::fillInPlaceholders
prefetch.optimize       Prefetch::optimize (internal loop)
cache.set               Cache::set (post-update tap)
```

### 4.7 Diagnostics

| Flag | Effect |
|---|---|
| `-v` | Verbose pipeline progress to stderr (one line per stage entry/exit). |
| `--print-stages` | List canonical stage names and exit. |
| `--print-passes` | List optimisation sub-pass names + bit positions and exit. |
| `--print-targets` | List devtypes and the `AwgDeviceProps` they resolve to. |
| `--version` | Print version (same as `_seqc_compiler.__version__`). |
| `--help` | Show usage. |

## 5. Internal architecture

### 5.1 Source layout

```
tools/                                  ← new top-level directory
  seqcc/
    DESIGN.md                           ← this document
    README.md
    CMakeLists.txt
    third_party/
      CLI11.hpp                         ← vendored single-header
    src/
      main.cpp                          ← argv[0] dispatch (seqcc/seqas/seqdump)
      driver.hpp / driver.cpp           ← pipeline orchestrator
      options.hpp / options.cpp         ← CLI11 spec; produces DriverConfig
      stage.hpp / stage.cpp             ← Stage enum, name<->enum, ordering
      passes.hpp / passes.cpp           ← Pass enum, name<->enum, bit mapping
      target.hpp / target.cpp           ← -march/-mtune → AWGCompilerConfig
      dump.hpp / dump.cpp               ← artifact registry + emitters
      seqas.cpp                         ← assembler-only mode body
      seqdump.cpp                       ← ELF inspector
      iostream_util.hpp / .cpp          ← `-o -`, atomic write, cout-capture
```

`tools/` lives at the repository root, parallel to `reconstructed/`.
Per the user direction, **no newly-written source goes under
`reconstructed/`**; the directory is reserved for reconstruction of
binary behaviour.

### 5.2 Build integration

`reconstructed/CMakeLists.txt` gains a single line:

```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../tools/seqcc/CMakeLists.txt")
    add_subdirectory(../tools/seqcc ${CMAKE_BINARY_DIR}/seqcc)
endif()
```

The `tools/seqcc/CMakeLists.txt`:

- Declares an executable target `seqcc` from `src/*.cpp`.
- Links `zhinst_seqc` (the existing static archive) and the same
  Boost components used by it.
- Adds `${CMAKE_CURRENT_SOURCE_DIR}/third_party` to its include path
  for CLI11.
- Installs (`install(TARGETS seqcc)` plus
  `install(CODE "...")` for the `seqas` and `seqdump` symlinks).

No modification is required to `file(GLOB_RECURSE
ZHINST_SEQC_SOURCES … src/*.cpp)` in
`reconstructed/CMakeLists.txt` because the new files live outside
`reconstructed/src/`.

### 5.3 Driver core (`driver.cpp`)

The driver is a thin state machine over the existing
`AWGCompiler`/`Compiler` API.  Pseudocode:

```cpp
struct DriverConfig {
    Stage from = Stage::Source;
    Stage to   = Stage::Link;
    std::filesystem::path input;
    std::filesystem::path output;
    AWGCompilerConfig awgConfig;     // built by target.cpp from -march/-mtune
    std::vector<DumpSpec> dumps;     // built by dump.cpp from --dump=...
    bool verbose = false;
};

int run(const DriverConfig& cfg) {
    // 1. Load input into the appropriate IR.
    auto state = loadInput(cfg);   // PipelineState carries whichever IRs exist so far

    // 2. Walk stages [from..to] in order, calling the existing API at each step.
    for (Stage s : range(cfg.from, cfg.to)) {
        emitDumpsBefore(s, state, cfg.dumps);
        state = advance(s, state, cfg);
        emitDumpsAfter(s, state, cfg.dumps);
    }

    // 3. Serialise the final IR according to `--to` and `-o`.
    return emitFinal(state, cfg);
}
```

`advance(Stage::Parse, ...)` calls `parse()` on a long-lived
`Compiler` instance, `advance(Stage::OptPre, ...)` calls
`AsmOptimize::optimizePreWaveform`, etc.  Where the existing public
API does not expose a stage boundary (e.g. inside
`AWGCompiler::compileString`), the driver instantiates the inner
`Compiler` directly via the same construction path
`AWGCompilerImpl::compileString` uses.

For `--from=` modes, `loadInput()` constructs the `Compiler` /
`Prefetch` instance and seeds its state from the supplied JSON / text
via `Node::fromJson`, `AsmList::deserialize`, or
`WavetableIR::fromJson` as appropriate.

### 5.4 Pass-boundary taps — **WITHDRAWN (superseded by T5b)**

> **Status:** The `passTap_` callback design described below is
> withdrawn as of T5a.4.  It conflicts with AGENTS.md commit
> `6b2d504` ("tooling vs reconstructed code: hands off"), which
> bans tooling-driven additions of new members, `friend` grants,
> or hooks on reconstructed classes — `passTap_` is exactly such
> an addition.
>
> **Replacement (landed):** the stepwise `Compiler` /
> `AWGCompilerImpl` API delivered in **T5b** — see §3.1 above.
> `Compiler::compile()` is now a dispatcher over 9 public step
> methods (T5b.3) and `AWGCompilerImpl::compileString()` is a
> dispatcher over 3 public step forwarders on `AWGCompiler`
> (T5b.5).  The driver in `tools/seqcc/src/driver.cpp` calls
> these forwarders sequentially and inspects the IR sinks
> between calls; `--to=lower` and `--to=asm` skip the back-end
> forwarders entirely (IF-304 partial closure, T5b.5).  No new
> callback machinery on reconstructed types; no per-pass
> branches inside the reconstructed `.cpp`.  Sub-pass
> granularity (e.g. taps inside `AsmOptimize` between individual
> optimisation passes) is deferred to a future phase if a
> concrete consumer need ever materialises — at which point the
> same stepwise refactor pattern would be applied to
> `AsmOptimize::run()`.
>
> The T5b refactor is itself a recon edit, but it is *sanctioned*
> under AGENTS.md "Allowed exceptions": the step boundaries land
> on verified binary addresses already documented in
> `reconstructed/notes/pipeline.md` (`0x11f268`, `0x11f27e`,
> `0x11f7b0`, `0x11fb1a`, `0x120707`, `0x120c92`, `0x120d60`,
> `0x120e2d`, `0x120f2b`, `0x121345`).  The refactor preserves
> those boundaries; it does not introduce new APIs that the
> original binary lacks at the public-vtable level.  See IF-306
> for the full sanctioned-exception write-up.
>
> The original `passTap_` description is retained below for
> historical context; do not implement it.

---

**Historical design (do not implement):**

To support `--dump-before/after=<pass>`, three reconstructed classes
gain a single null-by-default callback member:

```cpp
// asm_optimize.hpp — added member
class AsmOptimize {
    // ... existing fields ...

    /// Optional tap fired immediately before / after each named
    /// pass.  Null by default; only the seqcc driver installs it.
    /// The first arg is the pass name (e.g. "dead-code-elim"); the
    /// second is "before" or "after"; the third is the current
    /// AsmList.  Adding this member does NOT change behaviour when
    /// the callback is null.
    ///
    /// NEW: This is a tooling hook, not reconstructed binary
    /// behaviour.  See tools/seqcc/DESIGN.md §5.4.
    std::function<void(std::string_view, std::string_view, const AsmList&)>
        passTap_;
};
```

Identical taps go on `Prefetch` and `Cache` with type-appropriate
signatures.  Each tap call site in the `.cpp` is one line:

```cpp
if (passTap_) passTap_("dead-code-elim", "before", *current_);
deadCodeElimination();
if (passTap_) passTap_("dead-code-elim", "after",  *current_);
```

Every insertion is preceded by a comment `// NEW (seqcc tap):`
to make the boundary between reconstructed code and tooling hook
unambiguous.  The `if (passTap_)` guard is one cmp+jump per pass —
zero measurable cost when the tap is null.

**Scope of the modification:** ~12 tap call sites across three
files (`asm_optimize.cpp`, `asm_optimize_regalloc.cpp`, `prefetch.cpp`,
`cache.cpp`) plus the member declarations.  No semantic change to
any existing pass.  This is one of two minimally-invasive
modifications to reconstructed code; the other is adding
`print(std::ostream&)` overloads (§4.6).

Both modifications are validated by re-running the full
2693-case differential test suite after every sub-phase
wrap-up.

### 5.5 Argument parsing

CLI11 (header-only, MIT, single file `CLI11.hpp` ≈ 6000 LoC) is
vendored under `tools/seqcc/third_party/CLI11.hpp`.  It is checked
into the repository at a pinned version; updates are explicit
commits referencing the upstream tag.

Rationale (vs. hand-rolled or `boost::program_options`):

- Native support for gcc-style `-march=value` (equals-separated
  long options) and `-fno-pass` toggle pairs.
- Subcommand support handles `--print-stages` / `--print-passes` /
  `--print-targets` cleanly without spreading dispatch logic.
- Header-only — no new linker dependency, no impact on the existing
  static archive's link time.
- `boost::program_options` is awkward for `-Ox` short-stuck-value
  flags and adds a non-trivial runtime dependency for tiny gain.

### 5.6 ELF differential validation

A new test harness `tests/tools/test_seqcc_diff.py`:

1. Loads a curated subset of cases from `tests/cases/manifest*.json`
   covering each device family at least once
   (target ≈ 200 cases for fast iteration; expand if needed).
2. For each case:
   - Runs `_seqc_compiler.compile_seqc(...)` → `elf_python`.
   - Runs `seqcc -march=<devtype> -mtune=samplerate=<sr> ...
     <case>.seqc -o tmp.elf` → `elf_seqcc`.
   - Asserts `elf_python == elf_seqcc` (byte equality, like
     `tests/diff_test_fast.py`).
3. Failures emit the same side-by-side ELF section diff that
   `tests/dump_elf.py --both` produces.

Test runs in CI alongside the existing differential suite.  Any
regression in the driver immediately surfaces against the
authoritative `compile_seqc()` output.

## 6. Sub-phase plan

Each sub-phase ends with the standard wrap-up: build clean, full
differential suite (`tests/diff_test_fast.py`) at 2693/2693, commit
with the test score in the message body.  See `AGENTS.md` for the
iteration cycle.

### T1 — Scaffolding
- Create `tools/seqcc/` skeleton with `CMakeLists.txt`, vendored
  CLI11, minimal `main.cpp` handling `--help` / `--version` only.
- Hook into `reconstructed/CMakeLists.txt` via `add_subdirectory`.
- Smoke test: `tests/tools/test_seqcc_smoke.py` verifying
  `seqcc --help` exits 0.

### T2 — Full default path (`.seqc → ELF`)
- Implement `-march`, `-mtune=<k>=<v>` for sequencer / index /
  samplerate / options, `--wave-path`, `--waveforms`, `-o`.
- Implement `loadInput`/`advance(Stage::*)` for the full default
  chain.  Reuse `AWGCompiler::compileString` + `writeToStream`
  internally for the first cut.
- Implement `tests/tools/test_seqcc_diff.py` and bring it to green
  on ≥ 50 cases across all device families.

### T3 — JSON IR dumps
- `--dump=ast-lowered`, `--dump=asm`, `--dump=wavetable-ir`,
  `--dump=compile-report`.
- `--dump-dir`, `--dump-format`, `:path` per-artifact override.
- All implemented purely on top of existing `toJson` / `serialize`.

### T4 — Text-only IR dumps
- Add `print(std::ostream&)` overloads on `Expression`,
  `SeqCAstNode`, `Prefetch`, `Cache`, `Resources` where missing.
  Existing zero-arg `print()` redefined as `print(std::cout)`.
- Driver wires `--dump=ast-raw`, `--dump=ast-seqc`,
  `--dump=prefetch`, `--dump=cache`, `--dump=resources`.

### T5 — Stop-after-stage (`-S`, `-E`, `--to=`)
- Implement `--to=<stage>`.  `-S` ≡ `--to=opt-post`, `-E` ≡
  `--to=lower`.
- Driver emits the chosen IR to `-o` in its native format.

### T6 — Start-at-stage (`--from=`) — **Q3-DEMOTED (2026-05)**
- **Re-scoped (2026-05) per
  `reconstructed/notes/seqcc_from_design.md`.**  Original sketch
  proposed three modes (`ast-lowered`, `asm`, `wavetable-ir`);
  data-flow analysis showed only `--from=asm` is round-trippable
  from a single input file.  The other two are deferred (see §2
  "Out of scope").
- **Q3 demotion (2026-05, T6.2 wrap-up):** the round-trip resume
  path (`--from=asm`) was dropped.  Implementation surfaced that
  reconstructing the post-`stepOptPre` state cleanly requires
  transporting more than just the AsmList — WavetableIR,
  `Compiler::ast_`, and possibly more state via sidecars, each
  needing a new recon-side setter.  Each sidecar added during
  debugging revealed the next missing piece, with no clean
  stopping point apparent.  The user chose Q3 (drop the
  round-trip goal) rather than continue expanding the
  sanctioned recon surface.  See **IF-308** Resolution section
  in `reconstructed/notes/incidental_findings.md` for the full
  rationale.

#### What landed
- **`--to=asm-pre`** stage — one-way diagnostic dump.  Driver
  drives the front end stage-by-stage via the IF-307
  `compiler()` accessor (`reset → stepParse → stepToSeqCAst →
  stepLower → stepBuildAsmPreamble → stepOptPre`), then captures
  `asmList_.serialize()` into `IRSinks::preprefetchAsmText` and
  routes it through `dump.cpp::renderStagePrimary("asm-pre")`.
  No back end runs; there is no ELF.  Default output extension:
  `.seqasm`.
- **`--to=asm`** unchanged — continues to dump the post-pipeline
  assembly listing (now correctly labelled as a *debug* artifact,
  not a round-trip input).
- IF-307 recon surface (forward-declared `Compiler` +
  `AWGCompiler::compiler()` accessor, factored public
  `Compiler::setupResources()` helper, `setAsmList` /
  `setPlaceholderAsm` setters, `stepInnerCompileFromAsmList`
  overload) — landed in commit `fa0229c`.  The
  `stepInnerCompileFromAsmList` overload + the two setters are
  now dead-code with no driver caller; kept in tree because
  reverting the recon commit was judged not worth the churn.
- Three GDB-/objdump-verified binary-fidelity fixes discovered
  during round-trip debugging (gated by
  `ZHINST_RECON_ASMLIST_KEYWORD_FIX`, ON by default):
  - **IF-309**: lexer/parser `placeholder_line` keyword removed;
    placeholder lines now use the standard `placeholder # {json}`
    grammar.
  - **IF-310**: `AsmParserContext::addNode()` restored
    `comment` / `hasComment` writeback.
  - **IF-311**: `assembleStringToExpressionsVec()` retargeted to
    write `nopComment` / `noOptOverride_` (the correct
    `//`-comment fields) instead of `comment` / `hasComment`.

#### What was dropped
- `--from=asm` CLI option, `-x LANG` option, `--from=`/`--to=`
  compatibility validator, `fromStageNames()`, sidecar transport
  (`<out>.wavetableir.json`), `wavetableJsonForResume` parameter,
  `IRSinks::preprefetchWavetableJson`, `Compiler::setWavetableIR`
  setter, `Compiler::wavetableIR()` accessor, the 4th
  `wavetableJson` parameter on `AWGCompiler::
  stepInnerCompileFromAsmList`, `tests/tools/test_seqcc_from.py`.
- Driver-side: removed the `isFromAsm` branch and the
  `findLoadPlaceholder` helper from `driver.cpp`; removed the
  sidecar slurp/write blocks from `compile.cpp`; removed the
  `--from` / `-x` parsing + reconciliation from `main.cpp`.

### T7 — `seqas` mode + `-x asm` dual path
- Symlink `seqas` → `seqcc` install rule.
- `main.cpp` argv[0] dispatch.
- `seqcc -x asm`: route through `AWGAssembler` (legacy entry,
  `+0x80` `e_entry` offset).
- `seqcc --from=asm` (main pipeline): seeds the existing
  `Compiler` instance's `AsmList` from `deserialize` and runs the
  remaining stages.
- Document the `e_entry` difference in `--help` and in
  `notes/elf_format.md`.

### T8 — Pass-boundary taps
- Add `passTap_` member to `AsmOptimize`, `Prefetch`, `Cache`.
- Insert tap calls at each named pass boundary (one comment-tagged
  line per boundary).
- Driver implements `--dump-before=<pass>`, `--dump-after=<pass>`,
  `-O0`/`-O1`/`-O2`, `-f[no-]<pass>`, `--print-passes`.
- Update `reconstructed/notes/optimization_passes.md` with the
  user-visible pass-name → bit mapping.
- Re-run full 2693-case differential suite to confirm zero
  regressions from the tap insertions.

### T9 — `seqdump`
- Symlink `seqdump` → `seqcc` install rule.
- Port `tests/dump_elf.py` section dumper to C++.
- Supports `--section=<name>`, `--all`, `--diff <other.elf>`
  (side-by-side, like `dump_elf.py --both`).

### T10 — Wrap-up
- **T10a (landed).**  Retired the `compileSeqcWithIR()` /
  `CompileSeqcIntrospection` / `fillIntrospection()` introspection
  scaffold that the early seqcc driver used to capture
  mid-pipeline IR.  Also retired the parallel `seqcc_owned`
  CMake target and the `SEQCC_USE_OWNED_DRIVER` option.  The
  owned `SeqcDriver` is now the only seqcc compile path; it
  captures IR handles directly via the public
  `AWGCompiler::compiler() → Compiler::{ast(), wavetable(),
  asmList()}` accessors.  Recon-side `compile_seqc.{hpp,cpp}` is
  back to the original-binary single-entry-point footprint.
  Two new sanctioned recon accessors landed alongside (parallel
  to the existing `Compiler::asmList()`): `Compiler::ast()` and
  `Compiler::wavetable()`.  No friend grants on `AWGCompiler` —
  the public `compiler()` accessor (IF-307) carries everything.
- Add `reconstructed/notes/tools.md` documenting the driver
  architecture and pass map.
- Update `OVERVIEW.md` with a toolchain section.
- Final full-suite pass; mark Phase T complete in `TODO.md`.
- Any deferred items (e.g. `ast-seqc` deserialisation, `seqld`)
  filed as their own future phases.

## 7. Open questions / future work

- **`Expression` / `SeqCAstNode` JSON round-trip.**  Would enable
  `--from=ast-raw` / `--from=ast-seqc` symmetrically with the other
  IRs.  Requires a stable JSON schema for ~30 AST node types.
  Deferred until a concrete use case appears.
- **`--from=ast-lowered`.**  Deferred at T6 re-scope (see §2 and
  `reconstructed/notes/seqcc_from_design.md`).  Requires designing
  a whole-tree `Node` dump format and emitting it on the `--to=`
  side; the current `Node::toJson(idMap)` is single-node only.
- **`--from=wavetable-ir`.**  Deferred at T6 re-scope.  Resume at
  `stepOptPost` requires *both* `asmList_` and `compileWavetableIR_`
  to be populated coherently; a single input file is not
  sufficient.  Would need a multi-file `--from=` convention or a
  packed driver-input format.
- **`seqld`-style object format.**  Would require defining a stable
  on-disk representation for `AsmList + WavetableIR + symbol
  table` and refactoring the ELF writer to consume it.  Larger
  refactor; deferred.
- **`-march=native`-style auto-detection.**  Not meaningful for an
  AWG cross-compiler.
- **LSP / language-server integration.**  Out of scope; possible
  follow-up once dump artefacts stabilise.

## 8. References

- `reconstructed/notes/pipeline.md` — `Compiler::compile()` stage
  list with binary addresses.
- `reconstructed/notes/optimization_passes.md` — `AsmOptimize` pass
  catalogue and bitmask semantics.
- `reconstructed/notes/elf_format.md` — output ELF structure and
  the `+0x80` entry-point quirk of the `AWGAssembler` path.
- `reconstructed/src/codegen/compile_seqc.cpp` — the
  `AWGCompilerConfig` defaults `seqcc` must reproduce for byte
  equality.
- `reconstructed/include/zhinst/asm/asm_list.hpp` — `serialize` /
  `deserialize` of the assembly IR.
- `reconstructed/include/zhinst/ast/node.hpp` — `toJson` / `fromJson`
  of the lowered AST.
- `reconstructed/include/zhinst/waveform/wavetable_ir.hpp` —
  `toJson` / `fromJson` of `WavetableIR`.
- `AGENTS.md` — iteration cycle and commit discipline.
- `TODO.md` Phase T — sub-phase checklist tracked separately from
  this design.
