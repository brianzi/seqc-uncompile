# TODO — Reconstructed zhinst SeqC Compiler

> **Phase D (Inline code documentation) is the active phase.**
> Sub-phases D0–D10 plus D-AUDIT-1/2/3 are complete; D11–D15 are open.
> All earlier reconstruction phases (1–62, plus the symbol-renaming
> Phases D/R/S) are archived under `reconstructed/notes/archive/`.

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

- [x] **D3 — Pipeline-driver functions** _(complete; archived)_
      Full `\brief` / `\details` / `\param` / `\return` / `\throws`
      across 14 driver methods (`Compiler::compile`, `runPrefetcher`,
      `AWGCompiler::writeToStream`, `Prefetch::{preparePlays,
      placeLoads, fillInPlaceholders}`, `WavetableIR::{updateWaveforms,
      assignWaveformAllocationSizes}`, `AsmOptimize::{optimizePre,
      optimizePost}Waveform`, `FrontEndLoweringFacade::lower`,
      `CustomFunctions::call`, `WaveformGenerator::{getOrCreateWaveform,
      call, eval}`).  Doxygen warning cleanup 149 → 0 via
      `MACRO_EXPANSION` + `\cond INTERNAL` wrappers + canonical-name
      rename for `WaveformFile::operator==`.  IF-207 / IF-208 fixed
      in same phase.  Commits: `d7685d2`, `ff1f747`, `09dc245`,
      `e80b126`, `0dffcc2`.
      _Full per-batch narrative: see
      `reconstructed/notes/archive/TODO_phases_D3-D5_D-AUDIT-1.md`._

- [x] **D4 — Public methods of high-traffic classes** _(complete; archived)_
      Class-by-class brief sweep across `Compiler`, `Prefetch`,
      `WavetableIR` / `WavetableFront` / `WavetableManager`,
      `CustomFunctions` (8 sub-batches), `AsmOptimize`, and
      `WaveformGenerator` (4 sub-batches).  Surfaced 25 IFs
      (IF-209..IF-233) of which the likely-bugs were fixed in same
      phase: IF-209 (`setLineNr` propagation), IF-210
      (`setCancelCallback` propagation), IF-213 (`findLockedPlay`
      stub), IF-216 (`Prefetch::allocate` Wait→Lock symbol fix),
      IF-217 (`backwardTree` loop/next swap), IF-218
      (`expandSetVar` parent-chain walk), IF-219 (`createLoad`
      short-circuit), IF-223 (Table sub-paths A/B/C with IF-241..245
      cascade), IF-224 (`play_cervino_indexed_nonsplit` body), IF-226
      (`getUsedCache` body), IF-227 (`WavetableIR::size()` divisor),
      IF-230 family (parameter-label drift), IF-231 (`rand` 3-arg
      semantics).  Numerous cosmetic block-header drifts corrected
      in same commits.  Commits include `1e7cd32`, `e8a2600`,
      `03ebeea`, `a3c6af5`, `0f91745`, `3ebf2e3`, `bafa9a9`,
      `fbc7f4a`, `5821661`, `25f5aa0`, `389b48e`, `a861b55`,
      `bf89075`, `83f4973`, `73a2e5f`, `d68f5a3`.
      _Full per-batch narrative: see
      `reconstructed/notes/archive/TODO_phases_D3-D5_D-AUDIT-1.md`._

- [x] **D-AUDIT-1 — `WaveformGenerator` factory parameter-label audit**
      _(complete; archived)_
      Spawned from D4 Batch 6b verify-then-write.  Audited all 16
      multi-arity factories for arity-blind label-string drift via
      `objdump` `movabs` / `lea+movupd` decode.  Surfaced two
      semantic bugs (IF-231 `rand`; IF-234 trig family `sin`/`cos`/
      `sawtooth`/`triangle` 3-arg binding) and cosmetic drifts in
      `chirp`, `lfsrGaloisMarker`, `gauss`.  All fixed in same
      phase; coverage test `hdawg_doc_trig_3arg` added.  Audit-
      method clarifications (symbol-table for ranges,
      `movupd` for long labels, regex pitfalls) folded back into
      the canonical recipe.
      _Full per-finding narrative: see
      `reconstructed/notes/archive/TODO_phases_D3-D5_D-AUDIT-1.md`._

- [x] **D5 — Internal helpers / opcodes / leaves** _(complete; archived)_
      Coverage sweep 28.4% → 95.2% across 20 sub-batches (D5-1
      through D5-20) covering `PlayConfig`/`Signal`/`Waveform`,
      `DeviceConstants`/`Register`, `Cache`/`CachedParser`,
      `Resources` family, Asm parser/expression family,
      `AWGAssemblerImpl`/`AWGCompilerImpl`, `WaveformFront`,
      reconstruction-comment preservation audit (D5-AUDIT),
      `Exception`/`MemoryAllocator`/`SeqC*List`, `zhinst::`
      free-function gap (D5-9a..c, D5-10a..c), bulk class-member
      sweep (D5-11..17), top zhinst:: structs (D5-18/19), and
      long-tail (D5-20).  +1900 documented symbols.  New IFs
      this phase: IF-235..IF-240, IF-254.  Final tag counts at
      phase close: 7 `\unclear`, 10 `\verifyme`, 82 `\binarynote`.
      Style guide §14 (content-preservation rule) and §15 (macro
      doc-comment pitfalls) introduced during this phase.
      Workflow: subagent dispatch for mechanical sweeps; pure-
      additive diffs enforced; per-batch verify-then-write.
      Commits include `1bbc46a`, `0104d2b`, `5b8462d`, `74c5012`,
      `0bf65ce`, `c251ebe`, `2d547ae`, `bd0c54b`, `5d08a34`,
      `025eb8b`, `048d357`, `396adf3`, `188bd25`, `b289920`,
      `6256e53`, `3ce51ec`.
      _Full per-sub-batch narrative + follow-up items: see
      `reconstructed/notes/archive/TODO_phases_D3-D5_D-AUDIT-1.md`._

- [x] **D6 — Convert evergreen `notes/` files into Doxygen `\page`s**
      *(complete; D6-A promoted 13 EVERGREEN files, D6-B promoted 14
      of 16 MIXED files with 2 moved to EXCLUDED)*

  Notes-files audit (39 markdown files under `reconstructed/notes/`)
  classified each file as EVERGREEN (suitable for as-is promotion),
  MIXED (needs cleanup before promotion), or WORKING-DOC (excluded —
  status tracking, dated phase entries, audit-in-progress logs).
  See user-approved split: 13 promote-as-is, 16 promote-with-cleanup,
  10 exclude.

  - [x] **D6-A — Promote the 13 EVERGREEN files as-is.** Done.
        Approach: added `\page` headers + a one-paragraph
        "reverse-engineering reference material" banner to each file
        (per voice-policy carve-out approved by user — binary
        addresses and disasm vocabulary remain in-line for these
        pages, since their audience is reverse-engineering reference,
        not API documentation).  Wired `notes/` into Doxygen via a
        per-file include list in `reconstructed/CMakeLists.txt`
        (`DOXYGEN_NOTES_PAGES` variable) so the 26 non-promoted files
        stay excluded.  Cross-linked from `architecture.md` mainpage
        under a new "Reverse-engineering reference" section.
        Documentation roadmap section in `architecture.md` updated
        (was "D2 wrap-up"; now reflects D5-closed / D6-A complete /
        D6-B deferred).
        Tests 1602/1602; 0 doxygen warnings; build clean.

        Files (PROMOTE-AS-IS):
    - [x] `cervino_vs_hirzel.md` → `notes_cervino_vs_hirzel`
    - [x] `elf_format.md` → `notes_elf_format`
    - [x] `fb_instruction.md` → `notes_fb_instruction`
    - [x] `goto_policy.md` → `notes_goto_policy`
    - [x] `logging_tracing.md` → `notes_logging_tracing`
    - [x] `memory_allocator_analysis.md` → `notes_memory_allocator_analysis`
    - [x] `node_tree_structure.md` → `notes_node_tree_structure`
    - [x] `opcode_encoding.md` → `notes_opcode_encoding`
    - [x] `opcode_map.md` → `notes_opcode_map`
    - [x] `pipeline.md` → `notes_pipeline`
    - [x] `special_registers.md` → `notes_special_registers`
    - [x] `static_resources_cervino_consts.md` → `notes_static_resources_cervino_consts`
    - [x] `waveform_generator_funcmap.md` → `notes_waveform_generator_funcmap`

  - [x] **D6-B — Promote 14 of 16 MIXED files after cleanup**
        *(complete; see commits `64a1601` batch 1, `a837454` batch 2,
        `de1f165` batch 3)*.  Each promoted file gained a `\page`
        anchor + the standard reverse-engineering banner; phase
        tags, dated audit logs, RESOLVED markers, "Open items /
        Open questions" lists, "Recon bugs" trailers, future-work
        sections and similar transient framing were stripped.
        Verify-then-write per AGENTS.md caught one notes-only
        field-name discrepancy (IF-254: `Variable::flags` type and
        Frozen-bit position).  Two files moved to EXCLUDED instead
        of promoted (see below).  Files (audit notes recorded inline):
    - [x] `asm_parser_grammar.md` — drop reconstruction-plan +
          future-work sections. *(batch 2)*
    - [x] `awg_device_props.md` — drop phase tags + future-subphase
          sentence; collapsed inferred-vs-verified field names to
          canonical names. *(batch 1)*
    - [x] `binary_contents_excluded.md` — drop date + Phase 34
          progress refs. *(batch 1)*
    - [x] `device_constants.md` — drop "Phase 14b-iii.5" preamble.
          *(batch 1)*
    - [x] `differential_testing.md` — drop coverage snapshot +
          "potential future" sections. *(batch 2)*
    - [x] `elf_reader.md` — drop "Open items" + Phase 14d framing.
          *(batch 2)*
    - [x] `libcpp_abi.md` — drop "Phase 21g Dual-build strategy" +
          phase subheaders. *(batch 3)*
    - [x] `magic_numbers_proposal.md` — strip RESOLVED tags +
          investigation pointers; IF-254 fix to Variable::flags
          description. *(batch 2)*
    - [x] `optimization_passes.md` — strip Phase 15c correction +
          carry-forward list (verified `simplifyAssign` and
          `splitReg` are real implementations now, so the
          carry-forward warning was stale). *(batch 3)*
    - [x] `placeholder_field_names.md` — **moved to EXCLUDED**
          (working-doc audit log; see EXCLUDED section below).
    - [x] `seqc_language_features_excluded.md` — strip
          coverage-tooling meta. *(batch 1)*
    - [x] `seqc_parser_grammar.md` — strip Phase 30f Discovery framing.
          *(batch 2)*
    - [x] `splitreg_loop_model.md` — strip trailing "Recon bugs"
          section. *(batch 2)*
    - [x] `struct_layouts.md` — **moved to EXCLUDED** (1925-line
          working-doc with phase tags in nearly every section
          header; content has largely migrated into per-header
          struct comments in canonical `.hpp`s; see EXCLUDED
          section below).
    - [x] `writeToNode_block_d_protocol.md` — strip "Open questions" +
          phase correction headers. *(batch 2)*
    - [x] `write_waves_to_elf.md` — strip "(RESOLVED in Phase N)" tags.
          *(batch 1)*

  - **EXCLUDED (10) — working-doc tracking files, not for `\page`
    promotion:** `bytes_vs_samples_audit.md`, `comment_style_guide.md`
    (internal style guide, separate audience), `device_type.md`,
    `error_message_audit.md`, `frontend_lowering.md`,
    `incidental_findings.md`, `linker_resolution.md`,
    `placeholder_field_names.md` *(audit log of completed
    placeholder-field renames; tracking artifact)*,
    `struct_layouts.md` *(1925-line cumulative offset table; content
    largely migrated into per-header struct comments in canonical
    `.hpp`s)*, `unknowns.md`.

**Coverage baseline at end of D0:** 4/2712 symbols documented (0.1%).
Run `reconstructed/docs/coverage.sh` to track progress.

- [x] **D7 — Verify-triage sweep** (`\verifyme` / `\unclear` /
      `\binarynote` backlog) *(complete; see commits `2b4d43d`,
      `c8df0ca`, `78b1a5d`, `e4b93c8`, `383ba8f` plus prior
      round-1 / round-2 commits)*
  - Burn down the doc-accuracy backlog accumulated through D2–D6:
    12 `\verifyme` (hypotheses pending GDB or test verification),
    7 `\unclear` (semantics unknown), 82 `\binarynote` (non-idiomatic
    behaviour the caller must know about).
  - For each `\verifyme`: GDB-trace or write a targeted test to
    confirm the hypothesis; on pass, drop the tag and rewrite the
    brief in declarative voice.  On fail, log an IF and fix.
  - For each `\unclear`: investigate the actual semantics (binary
    + test corpus); resolve to a concrete brief or escalate as IF.
  - For each `\binarynote`: re-confirm the surprise still holds
    against the current recon; tighten or remove if no longer apt.
  - Coverage target: drop `\verifyme` to ≤3, `\unclear` to ≤2,
    `\binarynote` to ≤40 (those that genuinely surprise consumers).
  - **Outcome**: all targets met.  Final tag counts: `\unclear=2`
    (≤2 ✓), `\verifyme=0` (≤3 ✓), `\binarynote=40` (≤40 ✓).  Round 3
    triaged 33 sites across 4 commits (~13 Drops, ~14 Tightens to
    `\note` / `\warning`, plus Keeps for genuine caller-visible API
    surprises).  Five sites whose hypotheses are structurally
    unverifiable from SeqC inputs were moved to a new `\unverifiable`
    bucket (`\unverifiable=5`, no target — see commit `383ba8f`).

- [x] **D8 — Coverage-gap tests for latent prefetch paths**
  - The IF-223 / IF-244 reconstructions touch code paths the
    current suite did not exercise; the changes were
    static-only verified.  Author seqc cases that hit:
    1. ~~Play `cervino_indexed_nonsplit`~~ — **obsolete** per IF-246.
       D9.1 GDB-traced the binary block `0x1db4ad..0x1db55d` and
       confirmed it is reached **only** via the Load dispatch
       (`load_cervino_prf` Path B1), never via Play.  No Play-side
       counterpart exists, so no Play test case is needed.  The
       Load-side test moved to case 4 below.
    2. ~~Table sub-path C2 (`split_==1` AND
       `lengthReg.isValid() && lengthReg != R0`)~~ — **obsolete**
       per IF-249.  D8 case 2 scoping (subagent
       `ses_1e80272b8ffeib81kyIXaNE8hf`) GDB-confirmed that
       `NodeType::Table` is unreachable from the SeqC front-end
       across 11 inputs spanning every device type.  The shared
       `AsmCommands::asmTable` helper has zero call sites in both
       binary and recon; only `Node::from_json` produces Table
       nodes, and the public `compile_seqc(...)` binding never
       feeds that path.  IF-247's `!isHirzel` gate at this site
       is correct on its own merits but cannot be test-verified.
    3. ~~`playWaveTable` with non-empty cache + valid per-channel
       length register~~ — **obsolete** per IF-249.  No such
       custom function exists in the SeqC API; the case was
       speculative on the existence of `playWaveTable`.
    4. [x] **Load Path B1** (`cachePtr->size_ ==
       waveformMemorySize`) — covered by
       `core:uhfawg_load_cervino_prf_path_b1` (commit `a6f92d9`,
       co-landed with D10).  Test was authored red against the
       old incomplete stub and passes byte-identical after D10.
  - Each test should produce byte-identical ELF between original
    and recon; failures will graduate the affected IFs from
    "static-only verified" to "test-verified" or surface real
    bugs.

- [x] **D9 — Resolve IF-244 dead label blocks and Table-C-split wprf gate** *(all sub-items D9.1–D9.4 done/dropped; commits `f97effd`, `8423555`, `d1515c8`, `d0b4170`)*
  - **Background**: IF-244 action items 1+2 produced two label-only
    blocks in `prefetch_placesingle.cpp` (`play_cervino_indexed_nonsplit`
    at lines 865-941; `table_indexed_with_clamp` at lines 951-1022).
    Neither block has any incoming `goto` — they are dead
    documentation. GDB tracing in D9.1/D9.2 (now complete; see
    IF-246 / IF-247 / IF-248) reframed the situation: the
    `play_cervino_indexed_nonsplit` block is **mis-attributed** (binary
    `0x1db4ad..0x1db55d` is reached only via the Load dispatch —
    `load_cervino_prf` Path B1 — never via Play) and incomplete
    (missing the first `prf` and `2x addi`). The Table-C-split inline
    emission at lines 1398-1407 emits `wprf` unconditionally, but the
    binary gates it on `!isHirzel` (read from `AWGCompilerConfig` via
    `Prefetch::config_`).

  - [x] **D9.1 — GDB-verify the Play indexed-nonsplit divergences**
    (subagent `ses_1e9b705b9ffe6stbJmBc71NSAF`). Outcome: IF-244's
    Play-side claim disproved. The `0x1db4ad..0x1db55d` block is
    Load-side Path B1 only; recon's inline Play emission at lines
    675-722 is **not** the counterpart of this block. See **IF-246**.

  - [x] **D9.2 — GDB-verify the Table-C-split `wprf` gate** (subagent
    `ses_1e9a95462ffesKCS1nbPbpLpOH`). Outcome: gate confirmed at
    `0x1db92e`: `cmpb $0,0x18(%rax); jne` reads
    `AWGCompilerConfig::isHirzel` (offset `+0x18`) and **skips** `wprf`
    when set. UHFAWG (isHirzel=0) emits wprf;
    HDAWG/SHFSG/SHFQC_SG/SHFLI (isHirzel=1) skip. See **IF-247**. A
    separate "isHirzel inversion" alarm raised during D9.1 was
    investigated and dismissed — recon's per-device assignments in
    `awg_device_props.cpp` already match the binary. See **IF-248**.

  - [x] **D9.3 — Apply the resolved actions to source** (commit
    `d1515c8`).
    - Deleted the dead `play_cervino_indexed_nonsplit` block (was
      `prefetch_placesingle.cpp:865-941`); corrected description
      preserved in IF-246; full Path B1 reconstruction tracked by D10.
    - Deleted the dead `table_indexed_with_clamp` block (was
      `prefetch_placesingle.cpp:951-1022`); content documented in
      IF-244; the C2 dispatch from the Table case (IF-223 follow-up)
      should be modelled at the call site when wired up.
    - Promoted the `\verifyme` at lines 1398-1407 to a real
      `if (!config_->isHirzel)` gate around the inline Table-C-split
      `wprf` emission. Latent over-emission for modern devices on
      Table sub-path C2 closed (no current test exercises this path).
    - Build clean, 1602/1602 tests pass, no new doxygen warnings.
      Backlog tag deltas: `\verifyme` 16->13, `\binarynote` 81->80.

  - [~] **D9.4 — Dropped: extract `emitPrfEpilogueAndInsert_` helper**.
    With the two dead blocks deleted in D9.3, only three inline call
    sites remain (cervino-nonsplit / cervino-split / table-C-split),
    each with subtle differences (clamp vs no-clamp, gated vs
    unconditional `wprf`). A shared helper would have to wrap so much
    conditional behaviour that it would obscure rather than clarify.
    Dropped per iteration-cycle review at D9.3 wrap-up.

- [x] **D10 — Reconstruct `load_cervino_prf` Path B1 fully** (commit
  `a6f92d9`, follow-on from IF-246)
  - Replaced the incomplete stub at `prefetch_placesingle.cpp:342-349`
    with the full binary emission shape per IF-246:
    `prf -> 2x addi -> wprf -> prf -> jmp load_finalize`.
  - **Correction to the original TODO**: the local `wprf` at
    0x1db4bd is **unconditional**, not gated on `!isHirzel`.  Only
    the load_finalize tail `wprf` is gated per IF-247.  UHFAWG
    (the only device reaching this path) therefore emits **two**
    `wprf` instructions on this code path; a `\verifyme` was added
    near the local `wprf` to flag this expectation.
  - **Test verification**: `core:uhfawg_load_cervino_prf_path_b1`
    (added under D8 case 4) was authored red against the old stub
    (orig=76 vs recon=60 bytes of `.text`); the rewrite makes it
    pass byte-identical.
  - Disassembly was delegated to a subagent
    (`ses_1e81ed419ffeMdQ2iZx7K8gpxS`); the proposed C++ block
    matched the binary on first build (no iteration needed).

- [x] **D-AUDIT-2 — Swap layout-misnamed `AWGAssemblerImpl` string slots**
      *(promoted from IF-250, severity medium; completed 2026-05-12)*

  Fixed: `+0x20` slot renamed to `unusedStr020_`, `+0x38` slot
  renamed to `asmSource_` in the `AWGAssemblerImpl` header (layout
  comment and field declarations both updated).  Ctor init list in
  `awg_assembler_impl.cpp` reordered to match the new physical
  offset order.  Pipeline writer (`assembleFile`) and reader
  (`writeToFile`) in `awg_assembler_impl_pipeline.cpp` now
  correctly target the `+0x38` slot by name as well as by data
  flow.  Dropped the `\unclear` from the new `+0x20` slot's brief.
  Build clean; full diff-test suite passes 1603/1603 with no ELF
  byte changes.  IF-250 marked **fixed**.

- [x] **D-AUDIT-3 — Mirror binary's `apiErrorMessages` anon-namespace table**
      *(promoted from IF-251, severity cosmetic; completed 2026-05-12)*

  Fixed: introduced an anonymous-namespace `apiErrorMessages`
  `std::map<int, std::string>` in
  `reconstructed/src/core/error_messages.cpp`, populated at
  static-init time by an `ApiErrorMessagesInitializer` struct with
  all 52 entries the binary's BSS table at `0xb85230` holds
  (16384–16389 minus 16388; 32768–32800; 36864–36877).
  `getApiErrorMessage` now reads from this dedicated table instead
  of from `ErrorMessages::messages`, restoring the binary's
  two-independent-tables data-flow shape.  Strings are duplicated
  literals (per user direction "duplicate literals (faithful)"),
  not copy-derived from `messages`.  Build clean; full diff-test
  suite passes 1603/1603 (no observable change since the two
  tables agree on shared keys).  IF-251 marked **fixed**.

- [ ] **D11 — Resolve the lone remaining `\unclear` site (and any new `\verifyme`)**
      *(scope: small; expected outcome: drive `\unclear` count to 0)*

  After D7 the open backlog is `\unclear=1`, `\verifyme=0`.  The single
  surviving `\unclear` is unknown without inspection — the first task
  is to identify it via `./reconstructed/docs/coverage.sh` plus a
  targeted grep across `reconstructed/{include,src}/`.

  Steps:
  1. Locate the `\unclear` site; record the symbol, file:line, and
     the surrounding behavioural question in a short audit table at
     the top of this entry.
  2. Triage the site against the verify-then-write rules in AGENTS.md:
     - If a hypothesis exists → demote to `\verifyme` with the
       hypothesis spelled out, then fall through to step 3.
     - If the behaviour is genuinely unknowable from disassembly →
       upgrade to `\unverifiable` with a brief justification.
     - If the behaviour can be confirmed by GDB / reading the body →
       resolve outright (delete the tag, replace with verified prose).
  3. For any `\verifyme` produced in step 2, run a GDB trace against
     `_seqc_compiler.so` per the AGENTS.md "GDB tracing" recipe to
     confirm or refute the hypothesis; convert to verified prose on
     confirm, or to `\unverifiable` on refute.
  4. Final state: `\unclear=0`, `\verifyme=0`.  Update the OVERVIEW.md
     status block with the new counts.

- [ ] **D12 — Audit `\binarynote` sites (40) for reclassification**
      *(scope: medium; expected outcome: drive count down via Tighten
      to `\note` / `\warning` / verified prose where appropriate)*

  Per AGENTS.md voice rules, `\binarynote` is reserved for
  **non-idiomatic caller-visible surprise** that the caller must be
  aware of.  Many of the 40 surviving sites were filed before `\note`
  and `\warning` were sanctioned as Tighten targets and likely qualify
  for demotion.  D-AUDIT-1 already established the precedent of
  demoting parameter-label `\binarynote`s; this phase generalises that
  sweep.

  Steps:
  1. Enumerate all 40 sites
     (`grep -rn '\\binarynote' reconstructed/{include,src}/`).
     Build a one-row-per-site triage table with: file:line, symbol,
     current `\binarynote` body, and a proposed disposition:
     - **keep** — genuine non-idiomatic caller-visible surprise.
     - **→ `\note`** — informational, no caller invalidation hazard
       (e.g. device-gating notes, internal-codename → family-name
       rephrasings per AGENTS.md voice-rule guidance).
     - **→ `\warning`** — caller-invalidation surprise (returned
       reference invalidated by the next call, etc.).
     - **delete** — the surprise no longer applies after a later
       reconstruction or rename; replace with verified prose.
  2. Apply the proposed dispositions in batches (≤10 sites per
     commit) so any incidental discoveries stay attributable.
  3. After the sweep, update the OVERVIEW.md status block with the
     new `\binarynote` count and a one-line summary of the
     disposition split.
  4. **Followup**: any **\\binarynote → keep** disposition that
     points to a still-stubbed binary behaviour should be promoted
     into D14 (scout cluster) for prioritisation as a real
     reconstruction target.

- [ ] **D13 — Audit `\unverifiable` sites (5) for promotability**
      *(scope: small; expected outcome: confirm each truly is unverifiable
      from disassembly, or promote to `\verifyme` + GDB trace)*

  Five `\unverifiable` sites exist (per current coverage.sh):
  `reconstructed/include/zhinst/codegen/prefetch.hpp:1138`,
  `reconstructed/src/codegen/prefetch_placesingle.cpp:1109`,
  `reconstructed/include/zhinst/io/elf_reader.hpp:228`,
  `reconstructed/include/zhinst/asm/asm_expression.hpp:243` +
  `reconstructed/src/asm/asm_expression.cpp:205`,
  `reconstructed/include/zhinst/infra/logging.hpp:69`.

  Steps:
  1. For each site, re-read the surrounding context plus the
     original `\unverifiable` rationale prose.  Decide whether the
     "cannot be verified from disassembly" claim still holds in
     light of subsequently-developed reconstruction tooling
     (GDB recipes in AGENTS.md, IF-244/246 dead-label techniques,
     the libc++/libstdc++ ABI notes, etc.).
  2. If still genuinely unverifiable: tighten the rationale prose
     so it explains *why* (e.g. "compiled-out diagnostic path with
     no observable side effect") and leave `\unverifiable` in
     place.
  3. If now verifiable by GDB: demote to `\verifyme` with a
     concrete hypothesis and add a follow-up sub-task to D11 (or
     a fresh D11-style ticket) to run the trace and resolve.
  4. After the sweep, update the OVERVIEW.md status block.

- [ ] **D14 — Scout still-stubbed surface and unreconstructed library
      symbols for new reconstruction-target clusters**
      *(scope: medium; expected outcome: a prioritised cluster list
      with cluster-level work items added back to TODO.md)*

  No structured inventory currently exists of which `_seqc_compiler.so`
  symbols are still stub-only or absent from the recon.  This phase
  produces one and groups the remaining surface into clusters worth
  attacking together.  IF-253's `runningOnMfDevice` proper rebuild is
  one such candidate and should appear in the resulting cluster list.

  Steps:
  1. Build the inventory.  For each defined symbol in
     `_seqc_compiler.so` (via `nm -D --defined-only` filtered to
     `zhinst::` namespace), classify as one of:
     - **reconstructed** — has a body in `reconstructed/src/`
       beyond a trivial stub.
     - **stubbed** — present in recon but body is constant /
       trivially short / has a "stub" comment.
     - **absent** — no recon counterpart exists at all.
  2. Cluster the stubbed/absent set by subsystem (asm/ ast/
     codegen/ core/ device/ infra/ io/ runtime/ waveform/) and by
     functional theme (e.g. "device-environment detection",
     "LabOne manifest parsing", "diagnostics formatting", "ELF
     reader corner cases", "wavemem accounting helpers").
  3. For each cluster: count members, estimate per-symbol size from
     `nm --print-size` or objdump, and write a 2-3 sentence
     description of what the cluster appears to implement.  Flag
     clusters where a single symbol gates many others ("anchor
     symbols").
  4. Promote each cluster (or promising single anchor symbol) into
     a TODO.md entry sized appropriately — small/medium clusters
     become individual D-NN entries; large clusters get their own
     parent + sub-tasks.
  5. Delegation: this phase is largely mechanical inventory work
     and is a natural fit for a subagent (per AGENTS.md "use
     subagents for parallelizable mechanical sweeps").  The agent
     should produce the cluster table here in this TODO entry, not
     in a separate notes file.

- [ ] **D15 — Tracking-docs cleanup, cross-check, and archive cut**
      *(scope: medium; expected outcome: living docs trimmed,
      historical content archived, all three tracking surfaces in
      sync)*

  The active tracking surfaces have grown to:

  | Surface | Lines | Notes |
  | ------- | ----:| ----- |
  | `TODO.md` | 1277 | 14 closed top-level work items inline; only oldest archived to `archive/TODO_phases_*.md` |
  | `OVERVIEW.md` | 1233 | 4 historical "Phase D/R/S — Symbol Renames" sections still inline |
  | `reconstructed/notes/incidental_findings.md` | 7383 | 155 IFs (IF-100..IF-254); only IF-1..IF-99 archived |
  | `reconstructed/notes/unknowns.md` | 144 | small, current |

  Existing archive precedent (`reconstructed/notes/archive/IF_1-99.md`,
  ~1363 lines) shows IFs are archived in batches of ~100; IF-100..IF-200
  is the obvious next cut, ideally restricted to entries whose status
  is **fixed / dismissed / kept / resolved / kept (documented)** so
  open IFs stay surfaced in the active file.

  Steps:
  1. **Cross-check pass** (no archiving yet): walk TODO.md, OVERVIEW.md,
     and incidental_findings.md and verify they agree on the closed
     state of each completed phase / IF.  Fix any disagreement
     (typically OVERVIEW status blocks or IF status fields lagging
     a TODO close).  Record any inconsistency found as an IF
     entry before fixing it (per AGENTS.md incidental-discovery
     rule).
  2. **TODO.md trim**: identify closed top-level items whose entire
     body content is reproduced elsewhere (commit messages, OVERVIEW
     status blocks).  Compress those to a one-line summary +
     commit-ref + "see archive" pointer; full body moves to a new
     `archive/TODO_phases_D0-D10_D-AUDIT-1-3.md` (or similar) only
     if length-warranted.  Items completed within the last ~5
     commits stay inline as a recency window.
  3. **OVERVIEW.md trim**: move the three "Symbol Renames (Phase D/R/S)"
     sections (lines 356–442 inclusive, per current grep) to
     `archive/OVERVIEW_symbol_renames_phases_DRS.md`; replace with
     a short pointer paragraph.
  4. **IF archive cut**: move all **closed** IFs in the IF-100..IF-200
     range from `incidental_findings.md` to a new
     `archive/IF_100-200.md`.  Open IFs in that range stay in the
     active file.  Keep IF-201..IF-254 inline.  Update the
     "Other living references" block in TODO.md and the README of
     `archive/` to point at the new archive file.
  5. **Coherence final check**: re-run a grep-based audit that the
     archive split didn't break any inline references
     (`grep -rn 'IF-1[0-9][0-9]' reconstructed/ TODO.md OVERVIEW.md`
     against the post-split active file should not produce
     dangling pointers).
  6. Commit the cleanup as one or two focused commits (cross-check
     fixes; archive cut), each with the test score and 0 doxygen
     warnings verified.

  This phase is independent of D11/D12/D13/D14 and may run in
  parallel with them.

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
| [`OVERVIEW_symbol_renames_phases_DRS.md`](reconstructed/notes/archive/OVERVIEW_symbol_renames_phases_DRS.md) | Symbol-rename Phases D, R, S per-commit tables | ~100 |
| [`IF-105_update_log.md`](reconstructed/notes/archive/IF-105_update_log.md) | Chronological update history for IF-105 | ~390 |
| [`IF_1-99.md`](reconstructed/notes/archive/IF_1-99.md) | Closed Incidental Findings IF-1..IF-99 | ~1360 |
| [`IF_100-200.md`](reconstructed/notes/archive/IF_100-200.md) | Closed Incidental Findings IF-100..IF-200 (98 entries; 3 open in range kept inline) | ~3650 |
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
  — active entries: 3 still-open IFs from IF-100..IF-200
  (IF-100, IF-102, IF-104) plus IF-201..IF-254.  Closed entries
  in IF-1..IF-99 archived to
  [`archive/IF_1-99.md`](reconstructed/notes/archive/IF_1-99.md);
  closed entries in IF-100..IF-200 archived to
  [`archive/IF_100-200.md`](reconstructed/notes/archive/IF_100-200.md).
- [`reconstructed/notes/`](reconstructed/notes/) — topic-organized
  technical notes (struct layouts, opcodes, optimization passes, etc.).
- [`AGENTS.md`](AGENTS.md) — working process, GDB recipe,
  differential-testing workflow.
