# TODO — Reconstructed zhinst SeqC Compiler

> **Phases D / E / F / G are complete and archived.**  Active phases
> are **X** (`compile_seqc` binding-kwarg coverage) and **T**
> (toolchain driver `seqcc`).  Full closed sub-task lists for D–G
> are at
> [`reconstructed/notes/archive/TODO_phases_D-G.md`](reconstructed/notes/archive/TODO_phases_D-G.md);
> all earlier reconstruction phases (1–62, plus the symbol-renaming
> Phases D/R/S) are archived under `reconstructed/notes/archive/`.

## Current state

**Build**: clean (g++ + clang++/libc++), 0 errors, 0 doxygen warnings
(2 environmental latex errors in `doxygen-warnings.log` only).
**Differential tests: 2693/2693 byte-identical** across all 10 unique
leaf manifests:

| Manifest                     | Cases |
| ---------------------------- | -----:|
| default `manifest.json`      | 1603 |
| &nbsp;&nbsp;`manifest-core.json`          | 248 |
| &nbsp;&nbsp;`manifest-zhinst.json`        |  74 |
| &nbsp;&nbsp;`manifest-ziai.json`          | 459 |
| &nbsp;&nbsp;`manifest-ziasm.json`         | 468 |
| &nbsp;&nbsp;`manifest-zivibes.json`       | 262 |
| &nbsp;&nbsp;`manifest-documentation.json` |  92 |
| `manifest-stress.json` (standalone) | 1057 |
| `manifest-labone.json` (standalone) |  14 |
| `manifest-large.json` (standalone)  |  13 |
| `manifest-errors.json` (standalone) |   6 |
| **Total unique**             | **2693** |

**Diff-test harness** (`tests/diff_unreachable/harness.py`): 1626/1626
binding-unreachable cases passing.

Standalone manifests are not auto-imported by `manifest.json` and must
be invoked explicitly via `--manifest <name>.json`.

## Open work items

### Phases D, E, F, G — archived

All sub-tasks for the inline-documentation, binding-unreachable harness,
backlog-hardening, and `\binarynote` re-audit phases are complete.
Full task lists with per-sub-phase verdicts are archived at
[`reconstructed/notes/archive/TODO_phases_D-G.md`](reconstructed/notes/archive/TODO_phases_D-G.md).
Narrative summaries live in `OVERVIEW.md`.

Final state at archive cut: 1603/1603 main difftests + 1626/1626
harness cases passing; 0 doxygen warnings; `\binarynote` count
audited twice (Phases F1 and G).

## Phase X — `compile_seqc` binding-kwarg coverage

Differential-test coverage of every keyword argument in the public
`compile_seqc(code, devtype, options, index, **kwargs)` Python
binding.  Driven by `help(_seqc_compiler.compile_seqc)`.

Status snapshot (after this phase's first wave):

| kwarg        | coverage             | notes                                              |
|--------------|----------------------|----------------------------------------------------|
| `samplerate` | full (HDAWG)         | every HDAWG case                                   |
| `sequencer`  | `qa`/`sg`/`auto`     | added `sequencer_auto_shf{qa,sg}` in this phase    |
| `wavepath`   | `""` form only       | non-empty form blocked on IF-292                   |
| `waveforms`  | string-list form     | autoscan (`None`) form blocked on IF-292           |
| `filename`   | none                 | X1 below                                           |
| `options`    | smoke only           | string accepted but no ELF-visible diff; X3 below  |

- [ ] **X1 — `filename` kwarg coverage.**  Add a manifest case that
      passes a non-default `filename=` and produces matching original
      / recon ELF bytes.  Currently passing `filename` causes the
      `.filename` and `.arguments` ELF sections to diverge in size
      between original and recon, so this is a real bug that needs
      to be fixed before the test can be added in the green slice.

      Workflow:
      1. GDB-trace the original on a minimal `filename=` input to see
         what bytes land in `.filename` and how `.arguments` is
         constructed.
      2. Locate the recon write site (likely
         `AWGCompiler::writeToStream` or an ELF-section writer in
         `reconstructed/src/codegen/`).
      3. File an IF if a discrete divergence is identified.
      4. Fix the divergence, then add the test under
         `manifest-core.json :: core:general` with the
         `compile-kwargs` tag.

- [ ] **X2 — IF-292 follow-up: `wavepath` + `waveforms=None` autoscan.**
      Two distinct gaps documented in
      `reconstructed/notes/incidental_findings.md` IF-292:

      a. **Binding layer**
      (`reconstructed/src/pybind_seqc.cpp::pyCompileSeqc`) silently
         drops `None` kwargs because the merge loop only handles
         `cast<double>` and `cast<string>`.  Need to serialize Python
         `None` as JSON `null` (or an agreed sentinel) so the key
         actually reaches the C++ side.

      b. **Compiler core**
      (`reconstructed/src/codegen/compile_seqc.cpp` ~L221).  The
         `waveforms` reader only handles `is_string()`.  When
         `waveforms` is null and `wavepath` is non-empty, the
         original scans `wavepath` for `*.csv` and feeds the
         discovered list into `AWGCompiler::addWaveforms`.  Recon
         has no equivalent path.

      Workflow:
      1. GDB-trace original `pyCompileSeqc` with `waveforms=None` to
         confirm JSON serialisation of Python `None`.
      2. GDB-trace original `compileSeqc` to find the autoscan call
         site (likely near the existing `addWaveforms` dispatch in
         `AWGCompilerImpl::addWaveforms` / `WaveformTable::
         newWaveformFromFile`).
      3. Implement (a) and (b) and re-add the `wavepath_nonempty`
         manifest case (fixtures already in place under
         `tests/cases/waves/`).
      4. Close IF-292.

- [ ] **X3 — Behavioural `options` coverage.**  The three `options_*`
      cases added in the first wave verify orig/recon parity for the
      *parsing* of the options string, but produce byte-identical
      ELFs whether the option is present or not — so they don't
      verify the option's *semantic* effect.  Synthesize SeqC
      programs that exercise option-gated code paths and produce a
      visible ELF diff between "option present" vs "option absent",
      then add both pre/post manifest cases so the difference itself
      is in the corpus.

      Candidate gated paths (from `notes/device_type.md` and
      `custom_functions.cpp`):
      - `"MF"` flag in `usedFeatures_` — emitted into
        `required_options` JSON in `.waveforms`; gates oscillator
        allocation in `getAllOscMask` / `writeToNode` Block D
        (custom_functions.cpp:446..460, custom_functions_play.cpp
        :1441..1507).  Try a program that writes to an oscillator
        node so the `writeToNode` MF-insert path runs.
      - `"AWG"` / `"QA"` on UHFQA — gates the
        `AWG` / `QA` feature insertions in the same family.

      Verification recipe:
      ```bash
      python -c "
      import sys; sys.path.insert(0,'.')
      import _seqc_compiler as sc
      for opts in ['', 'MF']:
          e,_=sc.compile_seqc('<candidate program>','HDAWG8',opts,0,samplerate=2.4e9)
          print(opts, len(e), hash(e))
      "
      ```
      A successful candidate program produces a different ELF for the
      two `opts` values.  Add the program as a `.seqc` fixture plus
      two manifest cases (one with `options`, one without).

- [ ] **X4 — Phase wrap-up.**  Update OVERVIEW with the binding-kwarg
      coverage matrix.  Confirm full diff_test_fast suite stays
      green.  Triage any new IFs filed during X1–X3.

## Phase T — Toolchain driver (`seqcc`)

Build a stand-alone command-line driver that exposes the SeqC
compilation pipeline at a finer granularity than the
`compileSeqc()` entry point, modelled on `gcc`/`clang`.  Lets users
run individual stages, dump intermediate representations before and
after optimisation sub-passes, and feed mid-pipeline IRs back in.

**Design decisions (locked in by user discussion):**

- One executable, `seqcc`, plus symlinks `seqas` (assembler-only
  via the `AWGAssembler` legacy path) and `seqdump` (ELF inspector).
  Dispatch on argv[0].
- gcc-style flags: `-march=<devtype>`, `-mtune=<key>=<val>`,
  `-O<n>`, `-f[no-]<pass>`, `-S`/`-E`/`-c`, `--from=`/`--to=`,
  `-x <lang>`, `-o <path>`.
- Canonical stage names: `parse → astgen → lower → opt-pre →
  prefetch → opt-post → assemble → link`.
- Dumps: `--dump=<artifact>[:path]`, `--dump-before/after=<pass>`,
  `--dump-dir=`, `--dump-format=`.  Use existing serialisers
  (`Node::toJson`, `AsmList::serialize`, `WavetableIR::toJson`,
  `Waveform`/`Signal`/`PlayConfig::toJson`, ELF) where they exist;
  fall back to capturing existing `print()` output for `Expression`,
  `SeqCAstNode`, `Prefetch`, `Cache`, `Resources`.
- `--from=` initially supports the three round-trippable IRs:
  `ast-lowered` (Node), `asm` (AsmList), `wavetable-ir`.
- Source layout: **`tools/seqcc/`**, separate from `reconstructed/`,
  built via `add_subdirectory` from `reconstructed/CMakeLists.txt`.
  Links against the existing `zhinst_seqc` static archive.
- Argument parser: vendored single-header CLI11 under
  `tools/seqcc/third_party/CLI11.hpp`.
- The only modification to reconstructed code is adding a single
  null-by-default `std::function` "pass-boundary tap" to each of
  `AsmOptimize`, `Prefetch`, and `Cache`.  Each insertion gets a
  comment marker so the boundary between reconstruction and tooling
  hook is unambiguous.
- Validation: differential ELF byte-equality against
  `compile_seqc()` for a subset of manifest cases, via a new
  `tests/tools/test_seqcc_diff.py`.

### Sub-phases

- [X] **T1 — Scaffolding.**  Create `tools/seqcc/` with `CMakeLists.txt`,
      vendored `CLI11.hpp`, minimal `main.cpp` that only handles
      `--help` / `--version`.  Hook into `reconstructed/CMakeLists.txt`
      via `add_subdirectory(../tools/seqcc seqcc)`.  Build target
      produces `build/seqcc/seqcc`.  Add `tests/tools/` directory
      and a smoke test that verifies `seqcc --help` exits 0.
      _Commit `47b3b09` — 1612/1612 difftest, 4/4 smoke._

- [X] **T2 — Full default path.**  Implement enough flag parsing for
      `seqcc -march=HDAWG8 -mtune=samplerate=2.4e9 foo.seqc -o foo.elf`
      to produce a byte-identical ELF to what `compileSeqc()` produces.
      Implement `-march=<devtype>`, `-mtune=<k>=<v>` (sequencer, index,
      samplerate, options), `--wave-path=`, `--waveforms=`, `-o`.
      Defaults: same as `compile_seqc()`.  Add the differential test
      harness at `tests/tools/test_seqcc_diff.py` that runs a
      curated subset of manifest cases through both pipelines and
      requires byte-equal ELFs.
      _15/15 diff cases pass byte-equal across HDAWG/SHFQA/SHFSG/SHFQC/UHF.
      `--wave-path` / `--waveforms` are covered via the generic
      `-mtune=wavepath=...` / `-mtune=waveforms=...` channel; dedicated
      flag aliases deferred to T3 alongside the dump-flag surface.
      Output ELF is built by calling the existing `zhinst::compileSeqc()`
      free function — T5/T6 will refactor into direct `AWGCompiler` use
      when pipeline-stage granularity is needed._

- [X] **T3a — Option-surface cleanup.**  Replace T2's catch-all
      `-mtune=KEY=VALUE` with dedicated gcc-style flags for the
      common kwargs, and reject the silent-no-op `-mtune=index=`.
      New flags: `--sequencer`, `--samplerate`, `--filename`,
      `--wave-path`, `--waveforms`, `--index`, repeatable
      `-mdevopt=FEATURE` (singular).  Keeps `-mtune=` as an
      escape hatch for kwargs without a dedicated flag, and
      keeps `-mdevopts=PACKED` as a deprecated back-compat alias.
      _Closes IF-295 / IF-296 / IF-297 / IF-298.  19/19 diff harness
      cases byte-equal (15 existing + 4 new surface-specific), 4/4
      smoke, 1612/1612 difftest, no new build warnings._

- [ ] **T3b — JSON IR dumps.**  Implement `--dump=ast-lowered`,
      `--dump=asm`, `--dump=wavetable-ir`, `--dump=compile-report`
      using the existing `toJson`/`serialize` methods.  `--dump-dir=`
      and `--dump-format=` (auto|json|text).  Each `--dump=` may be
      repeated and optionally takes `:path` to override the default
      filename.

- [ ] **T4 — Text-only IR dumps.**  Add `--dump=ast-raw`,
      `--dump=ast-seqc`, `--dump=prefetch`, `--dump=cache`,
      `--dump=resources`.  Where the existing `print()` writes to
      `std::cout` only, add a `print(std::ostream&)` overload
      delegating to the existing implementation (minor cleanup of
      reconstructed code).  Where the existing API already takes a
      stream, use it directly.

- [ ] **T5 — `-S`, `-E`, `--to=<stage>`.**  Stop-after-stage support.
      The chosen IR is emitted to `-o` in its native format (JSON for
      Node/WavetableIR, text DSL for AsmList, text tree for AST).
      `-S` is sugar for `--to=opt-post` (emits `.seqasm`).  `-E` is
      sugar for `--to=lower` with `--dump=ast-lowered`.

- [ ] **T6 — `--from=<stage>`.**  Start-at-stage for `ast-lowered`,
      `asm`, `wavetable-ir`.  Input file format is auto-detected from
      extension (`.node.json`, `.seqasm`, `.wavetable.json`) or
      forced via `-x <lang>`.  Validates that the supplied stage is a
      legal start point and that no skipped stage is required by the
      target's pipeline.

- [ ] **T7 — `seqas` mode + `-x asm` dual path.**  When invoked as
      `seqas` (or `seqcc -x asm`), route `.seqasm` inputs through the
      `AWGAssembler` legacy entry point (entry-point offset `+0x80`).
      Also expose `--from=asm` on the main pipeline (the
      `Compiler::compile` path's assemble stage) so users can compare
      both paths.  Document the e_entry difference in driver `--help`.

- [ ] **T8 — Optimisation-pass taps.**  Add the
      `std::function<void(std::string_view, …)>` taps to `AsmOptimize`,
      `Prefetch`, `Cache`.  Insert tap calls at each named pass
      boundary.  Implement `--dump-after=<pass>` and
      `--dump-before=<pass>` in the driver.  Implement `-O<n>`
      (mapping to `optimizationFlags` bitmask) and `-f[no-]<pass>`
      toggle flags.  Add `--print-passes` listing all known pass
      names.  Update `notes/optimization_passes.md` with the pass
      name → bit mapping.

- [ ] **T9 — `seqdump` mode.**  Port the section-listing logic from
      `tests/dump_elf.py` to C++ in `tools/seqcc/seqdump.cpp` (or as
      a sub-mode of the main binary).  Supports `--section=<name>`,
      `--all`, `--diff <other.elf>` (side-by-side comparison like the
      Python tool's `--both`).

- [ ] **T10 — Wrap-up.**  Update OVERVIEW.md with the toolchain
      section.  Add `reconstructed/notes/tools.md` documenting the
      driver architecture (stage map, pass map, dump catalogue).
      Run the full diff_test_fast suite to confirm zero regressions
      from the reconstructed-code modifications (T8 taps).  Add any
      deferred work as new TODO items under Phase T or follow-up
      phases.

### Deferred / out-of-scope

- Round-trip of `Expression` and `SeqCAstNode` (no existing
  deserialisers, would be a separate phase if needed).
- Separate "linker" binary (`seqld`).  The ELF writer is tightly
  coupled to `AWGCompiler::writeToStream` and `WavetableIR`;
  factoring a clean object-file boundary is a bigger refactor
  than this phase warrants.
- Exposing every `AWGCompilerConfig` field as a flag.  Only the
  fields that the Python binding exposes (and a handful of
  optimisation-related ones via `-O`/`-f`) are surfaced; the rest
  retain their hardcoded defaults from `compileSeqc()`.

## Archives

All historical reconstruction work is preserved under
`reconstructed/notes/archive/`:

| File | Phases | Lines |
| ---- | ------ | -----:|
| [`TODO_phases_1-12.md`](reconstructed/notes/archive/TODO_phases_1-12.md) | 1–12 (initial reconstruction; 597 items) | ~2700 |
| [`TODO_phases_13-39.md`](reconstructed/notes/archive/TODO_phases_13-39.md) | 13, 15–39, 30 (method bodies, code-smell sweeps) + Differential Testing narrative | ~3900 |
| [`TODO_phases_43-62.md`](reconstructed/notes/archive/TODO_phases_43-62.md) | 43, 44, 45, 47–62 (symbol-size investigations, IF-109 audit, IF resolutions, stress suite, 4-device coverage) | ~2100 |
| [`TODO_phases_D3-D5_D-AUDIT-1.md`](reconstructed/notes/archive/TODO_phases_D3-D5_D-AUDIT-1.md) | Phase D3, D4 (all batches), D5 (all sub-batches), D-AUDIT-1 — full per-batch narrative | ~930 |
| [`TODO_phases_D-G.md`](reconstructed/notes/archive/TODO_phases_D-G.md) | Phases D (D0–D19), E (E1–E3), F (F1–F5), G (G1–G4) — full closed sub-task lists | ~1290 |
| [`OVERVIEW_phases_1-12.md`](reconstructed/notes/archive/OVERVIEW_phases_1-12.md) | OVERVIEW phase narrative for Phases 1–12 | ~2300 |
| [`OVERVIEW_phases_13-25.md`](reconstructed/notes/archive/OVERVIEW_phases_13-25.md) | OVERVIEW phase narrative for Phases 14a–25 | ~320 |
| [`OVERVIEW_phase_13_14_narrative.md`](reconstructed/notes/archive/OVERVIEW_phase_13_14_narrative.md) | Phase 13–14 supplementary narrative | ~250 |
| [`OVERVIEW_symbol_renames_phases_DRS.md`](reconstructed/notes/archive/OVERVIEW_symbol_renames_phases_DRS.md) | Symbol-rename Phases D, R, S per-commit tables | ~100 |
| [`IF-105_update_log.md`](reconstructed/notes/archive/IF-105_update_log.md) | Chronological update history for IF-105 | ~390 |
| [`IF_1-99.md`](reconstructed/notes/archive/IF_1-99.md) | Closed Incidental Findings IF-1..IF-99 | ~1360 |
| [`IF_100-200.md`](reconstructed/notes/archive/IF_100-200.md) | Closed Incidental Findings IF-100..IF-200 (99 entries; 2 open in range kept inline: IF-100, IF-102) | ~3700 |
| [`IF_201-300.md`](reconstructed/notes/archive/IF_201-300.md) | Closed Incidental Findings IF-201..IF-300 (91 entries; IF-292 still open) | ~6280 |
| [`unknowns_full_1-116.md`](reconstructed/notes/archive/unknowns_full_1-116.md) | Pre-2026-04-22 history of `unknowns.md` items 1–116 | ~1200 |
| [`phase_15b_prefetch_audit.md`](reconstructed/notes/archive/phase_15b_prefetch_audit.md) | Phase 15b prefetch audit | ~80 |
| [`phase43_investigation.md`](reconstructed/notes/archive/phase43_investigation.md) | Phase 43: 54-function binary-vs-recon size audit | ~490 |
| [`phase_r_leftovers_and_q_scoping.md`](reconstructed/notes/archive/phase_r_leftovers_and_q_scoping.md) | Phase R leftovers + Phase Q scoping (2026-04-29) | ~310 |
| [`todo_audit.md`](reconstructed/notes/archive/todo_audit.md) | Phase audit pass | ~250 |

## Commit conventions

- Per AGENTS.md: commit after every sub-phase wrap-up; include the
  test score in the commit message body; do not commit mid-debug.
- New work that opens its own phase should be added under
  `## Open work items` above with a `Phase NN: <title>` heading and
  a checklist; archive when complete.

## Other living references

- [`OVERVIEW.md`](OVERVIEW.md) — class hierarchy, current
  reconstruction status, file structure, open questions.
- [`reconstructed/notes/incidental_findings.md`](reconstructed/notes/incidental_findings.md)
  — active entries: 3 still-open IFs (IF-100, IF-102, IF-292).
  All closed / deferred / tracking-only entries archived to
  [`archive/IF_1-99.md`](reconstructed/notes/archive/IF_1-99.md),
  [`archive/IF_100-200.md`](reconstructed/notes/archive/IF_100-200.md),
  and
  [`archive/IF_201-300.md`](reconstructed/notes/archive/IF_201-300.md).
- [`reconstructed/notes/`](reconstructed/notes/) — topic-organized
  technical notes (struct layouts, opcodes, optimization passes, etc.).
- [`AGENTS.md`](AGENTS.md) — working process, GDB recipe,
  differential-testing workflow.
