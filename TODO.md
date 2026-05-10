# TODO — Reconstructed zhinst SeqC Compiler

> **All historical phases archived.**  Reconstruction has reached a
> steady state with no open work items as of 2026-05-08.

## Current state

**Build**: clean (g++ + clang++/libc++), 0 errors, 1 documented warning.
**Differential tests: 2407/2407 byte-identical** across all 10 unique
leaf manifests:

| Manifest                     | Cases |
| ---------------------------- | -----:|
| default `manifest.json`      | 1600 |
| &nbsp;&nbsp;`manifest-core.json`          | 248 |
| &nbsp;&nbsp;`manifest-zhinst.json`        |  74 |
| &nbsp;&nbsp;`manifest-ziai.json`          | 459 |
| &nbsp;&nbsp;`manifest-ziasm.json`         | 468 |
| &nbsp;&nbsp;`manifest-zivibes.json`       | 259 |
| &nbsp;&nbsp;`manifest-documentation.json` |  92 |
| `manifest-stress.json` (standalone) | 774 |
| `manifest-labone.json` (standalone) |  14 |
| `manifest-large.json` (standalone)  |  13 |
| `manifest-errors.json` (standalone) |   6 |
| **Total unique**             | **2407** |

Standalone manifests are not auto-imported by `manifest.json` and must
be invoked explicitly via `--manifest <name>.json`.

## Open work items

### Phase D — Inline code documentation

Long-running phase to add Doxygen-rendered API documentation to the
reconstructed code base.  See `reconstructed/docs/architecture.md` for
the rendered mainpage and `reconstructed/notes/comment_style_guide.md`
§13 for the documentation comment conventions.

**Accuracy discipline:** every claim must be backed by disassembly
evidence, GDB verification, a notes-file cross-reference, or test
coverage.  When none apply, use `\unclear`, `\verifyme`, or
`\binarynote` rather than guess.  These tags aggregate onto dedicated
cross-reference pages so the backlog is discoverable.

- [x] **D0 — Setup** _(complete)_
  - [x] `reconstructed/docs/Doxyfile.in` with theme, aliases, warning config
  - [x] `docs` cmake target (`cmake --build . --target docs`)
  - [x] Doxygen Awesome CSS theme under `reconstructed/docs/theme/`
  - [x] `reconstructed/docs/architecture.md` mainpage stub
  - [x] `reconstructed/docs/coverage.sh` baseline tracker
  - [x] §13 added to `comment_style_guide.md`

- [x] **D1 — Architecture mainpage** _(complete)_
  - [x] Flesh out `architecture.md` with pipeline overview, component
        map, type-relationship diagram, ELF output reference, notes/
        cross-references — all sourced from `OVERVIEW.md`,
        `notes/pipeline.md`, and existing topic notes (no new claims)
  - [x] Builds clean, TOC + 9 H2 sections render

- [x] **D2 — Class-level `\brief` on every public header** _(complete)_
  - [x] Topological order: Batches 1–11, ~144 public classes briefed
        across `core/`, `infra/`, `io/`, `ast/`, `asm/`, `codegen/`,
        `runtime/`, `waveform/`, `device/`
  - [x] Per-batch commits with verify-then-write workflow against `.cpp`
  - [x] `WARN_IF_UNDOCUMENTED = YES` in Doxyfile (flipped at wrap-up)
  - [x] No undocumented-symbol warnings remaining; the 1441 warnings
        in `build/docs/doxygen-warnings.log` are all parser cross-
        reference issues (overload resolution + macro-hidden symbols),
        not coverage gaps — promoted to D3 as cleanup items below

- [x] **D3 — Pipeline-driver functions** (~30 functions)
  - [x] Full `\brief` + `\details` + `\param` + `\return` + `\throws`
  - [x] Targets: `Compiler::compile`, `runPrefetcher`,
        `AWGCompiler::writeToStream`, `Prefetch::{preparePlays,
        placeLoads, fillInPlaceholders}`, `WavetableIR::{updateWaveforms,
        assignWaveformAllocationSizes}`, `AsmOptimize::{optimizePre/Post
        Waveform}`, `FrontEndLoweringFacade::lower`,
        `CustomFunctions::call`, `WaveformGenerator::{getOrCreateWaveform,
        call, eval}` (14 driver methods total across 5 batches)
  - [x] Per-group commits (Batches 1-5: d7685d2, ff1f747, 09dc245,
        e80b126, 0dffcc2)
  - [x] **Doxygen warning cleanup** (149 → 0):
        - Enabled `MACRO_EXPANSION=YES` + `EXPAND_ONLY_PREDEF=YES` with
          `EXPAND_AS_DEFINED` listing the AST class-generator macros
          (`SEQC_TRIVIAL_LEAF`, `SEQC_UNARY`, `SEQC_OPERATOR`,
          `SEQC_BINARY`, `SEQC_LIST`) and `ZHINST_DECLARE_FACTORY` so
          Doxygen sees their generated member declarations (resolved
          ~140 of the warnings)
        - Wrapped explicit template instantiations in `error_messages.cpp`
          and `wave_index_tracker.cpp` in `\cond INTERNAL` (Doxygen
          can't bind explicit-instantiation lines to their parameterised
          template declaration; resolved 56 warnings)
        - Wrapped `logging.cpp`'s `BOOST_LOG_GLOBAL_LOGGER`-generated
          `ZiLogger` and the `detail::LogRecord` definitions in
          `\cond INTERNAL` (the public docs already live in
          `infra/logging.hpp`; resolved 3 warnings)
        - Renamed `Waveform::File::operator==` to its canonical
          `WaveformFile::operator==` in `waveform.cpp` to match the
          header-side declaration (resolved 1 warning; the `using
          File = WaveformFile;` alias inside `class Waveform` is the
          source of the confusion)
        - Re-aligned the numbered list items in
          `compiler.hpp::compile`/`runPrefetcher` `\details` blocks so
          single-digit and double-digit markers share the same column
          (resolved 2 "Invalid list item" warnings)
  - [x] **IF-207 fix**: swapped `MESSAGE`/`ERROR_MSG` values corrected
        in the banner comment above `AsmOptimize::reportUserMessages()`
        in `include/zhinst/asm/asm_optimize.hpp:253` and the matching
        comment in `src/asm/asm_optimize.cpp:651` (cosmetic)
  - [x] **IF-208 fix**: renamed `PrefetcherNodeState::useDA` to
        `crossesCacheLine` (cross-cache-line load flag on Hirzel) at
        all four read/write sites and updated the inline layout comment
        in `include/zhinst/codegen/prefetch.hpp:159,193`. The unrelated
        local-variable `useDA = devConst_->hasPrecomp` shadows in
        `prefetch.cpp` were left as-is per the audit recommendation.

- [ ] **D4 — Public methods of high-traffic classes**
  - [ ] Order: `Compiler` → `Prefetch` → `WavetableIR/Front` →
        `CustomFunctions` (subdivided) → `AsmOptimize` →
        `WaveformGenerator` → `Resources` family → AST nodes
  - [ ] Per-class commits and `\unclear` triage
  - [x] **D4 Batch 1** — `Compiler` class (16 public methods):
        constructor, destructor, `unifyLineEndings`, `parse`,
        `printAST`, `reset`, `setCancelCallback`,
        `setProgressCallback`, `getNodeAccessList`,
        `getNodeToModeMap`, `getChannelInfo`,
        `usedDeviceSampleRate`, `hadSyntaxError`,
        `getCompileMessages`, `setLineNr`, `getLineMap`.  `compile`
        and `runPrefetcher` were already done in D3 Batch 1.  Surfaced
        IF-209 (`setLineNr` missing AsmCommands + WavetableFront
        propagation) and IF-210 (`setCancelCallback` missing
        WaveformGenerator propagation); both flagged on the docs via
        `\verifyme`.

- [ ] **D5 — Internal helpers / opcodes / leaves** _(on demand)_

- [ ] **D6 — Convert evergreen `notes/` files into Doxygen `\page`s**
  - [ ] `optimization_passes.md`
  - [ ] `device_type.md`
  - [ ] `prefetch.md` (if present)
  - [ ] `wavetable.md` (if present)
  - [ ] `incidental_findings.md` (as known-issues page)

**Coverage baseline at end of D0:** 4/2712 symbols documented (0.1%).
Run `reconstructed/docs/coverage.sh` to track progress.

## Archives

All historical reconstruction work is preserved under
`reconstructed/notes/archive/`:

| File | Phases | Lines |
| ---- | ------ | -----:|
| [`TODO_phases_1-12.md`](reconstructed/notes/archive/TODO_phases_1-12.md) | 1–12 (initial reconstruction; 597 items) | ~2700 |
| [`TODO_phases_13-39.md`](reconstructed/notes/archive/TODO_phases_13-39.md) | 13, 15–39, 30 (method bodies, code-smell sweeps) + Differential Testing narrative | ~3900 |
| [`TODO_phases_43-62.md`](reconstructed/notes/archive/TODO_phases_43-62.md) | 43, 44, 45, 47–62 (symbol-size investigations, IF-109 audit, IF resolutions, stress suite, 4-device coverage) | ~2100 |
| [`OVERVIEW_phases_1-12.md`](reconstructed/notes/archive/OVERVIEW_phases_1-12.md) | OVERVIEW phase narrative for Phases 1–12 | ~2300 |
| [`OVERVIEW_phases_13-25.md`](reconstructed/notes/archive/OVERVIEW_phases_13-25.md) | OVERVIEW phase narrative for Phases 14a–25 | ~320 |
| [`OVERVIEW_phase_13_14_narrative.md`](reconstructed/notes/archive/OVERVIEW_phase_13_14_narrative.md) | Phase 13–14 supplementary narrative | ~250 |
| [`IF-105_update_log.md`](reconstructed/notes/archive/IF-105_update_log.md) | Chronological update history for IF-105 | ~390 |
| [`unknowns_full_1-116.md`](reconstructed/notes/archive/unknowns_full_1-116.md) | Pre-2026-04-22 history of `unknowns.md` items 1–116 | ~1200 |
| [`phase_15b_prefetch_audit.md`](reconstructed/notes/archive/phase_15b_prefetch_audit.md) | Phase 15b prefetch audit | ~80 |
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
  — IF-1..IF-200 (oldest entries IF-1..IF-99 archived to
  [`archive/IF_1-99.md`](reconstructed/notes/archive/IF_1-99.md)).
- [`reconstructed/notes/`](reconstructed/notes/) — topic-organized
  technical notes (struct layouts, opcodes, optimization passes, etc.).
- [`AGENTS.md`](AGENTS.md) — working process, GDB recipe,
  differential-testing workflow.
