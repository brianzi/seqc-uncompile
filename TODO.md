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

## Phase D7 — Doxygen topical reorganisation

Reshape the doxygen site from a flat collection of per-file promoted
notes into a six-section topical reference covering the workings of
the reconstructed implementation: target architecture, SeqC language,
compiler pipeline, runtime, ELF output, and toolchain.  Drive
principle: drop reconstruction narrative (no addresses, no struct
layouts, no disassembly) from promoted pages; keep all such material
in `reconstructed/notes/` source files only.

Plan file: `.opencode/plans/1779007408145-misty-panda.md`.

### Phase D7-A — Structural changes (single commit)

- [x] **A.1** Rewrite `DOXYGEN_NOTES_PAGES` in `reconstructed/CMakeLists.txt`
      as the new six-section ordered list; add the 5 new topical
      pages; drop the 9 archived pages.
- [x] **A.2** Move 9 Class B files to `reconstructed/notes/archive/`:
      `goto_policy.md`, `memory_allocator_analysis.md`,
      `static_resources_cervino_consts.md`, `binary_contents_excluded.md`,
      `write_waves_to_elf.md`, `differential_testing.md`,
      `splitreg_loop_model.md`, `writeToNode_block_d_protocol.md`,
      `libcpp_abi.md`.  Drop dead `\ref` links from `tools.md` and
      `tools_testing.md`.
- [x] **A.3** Create 3 new page stubs (`custom_functions.md`,
      `prefetch_scheduling.md`, `play_config.md`).  Add `\page`
      anchor to existing-but-unpromoted `frontend_lowering.md`.
      Each stub is a single `\unclear` block with a scope statement
      and crosslinks so the IA resolves cleanly.
- [x] **A.4** Rewrite `reconstructed/docs/architecture.md` mainpage
      around six topical sections (Target Architecture / SeqC
      Language / Compiler Pipeline / Runtime & Resources / ELF
      Output / Toolchain), each linking into its subpages via
      `\subpage`.  Removed the flat "Reverse-engineering reference"
      table.
- [x] **A.5** Build the docs (`cmake --build . --target docs` from
      `reconstructed/build/`).  Warning log unchanged from prior
      state; all `\subpage` / `\ref` links resolve.  Coverage 94.6%.
- [x] **A.6** Commit + sub-phase wrap-up.

### Phase D7-B — Per-page content cleanup & authoring (six batches)

All six batches closed 2026-05-17.  Per-batch summaries are recorded
in `OVERVIEW.md :: Phase D7 — Doxygen topical reorganisation`.

- [x] **B.1 — §6 Toolchain banner cleanup.**  Commit `4c3e6ed`.
      Banner cleanup on the four tool pages; binary-address column
      and IF-300..309 sanctioned-edit log removed from
      `tools_design.md`.  Tool page *contents* remain WIP-owned
      independently.

- [x] **B.2 — §5 ELF Output.**  Commit `7806011`.  Three address
      citations stripped from `elf_format.md`; `elf_reader.md`
      rewritten down to `.linenr` format reference + reader API.

- [x] **B.3 — §1 Target Architecture.**  Commit `5c3af40` plus
      follow-ups `a26a5da`.  Banner stripped on all six §1 pages.
      `opcode_encoding.md` lead-in rewritten; offset tables and
      error-ID table dropped.  `fb_instruction.md` Source-Locations
      appendix removed.  `cervino_vs_hirzel.md` bitmask aside and
      numeric `AwgDeviceType` column dropped.  `awg_device_props.md`
      rewritten as a per-device property table.  Follow-ups added
      0x14 / 0x18 entries to `special_registers.md` (with IF-312
      for the `kSuserUserRegBase` misnomer), added `custom_functions`
      crosslinks across four §1 pages, and expanded the mainpage §1
      ISA prose to three paragraphs.  Decision: do not promote
      `device_type.md` (Class B working note).

- [x] **B.4 — §4 Runtime & §2 SeqC Language.**  Commit `974b81b`.
      Authored `custom_functions.md` from scratch.  Trimmed
      `seqc_parser_grammar.md`.  Reduced `logging_tracing.md` to a
      short summary plus the public API surface.  Rewrote
      `device_constants.md` as a per-device limits table.

- [x] **B.5 — §3 Compiler Pipeline (core).**  Commit `9efdf0f`.
      Rewrote `pipeline.md` as a 12-step walkthrough.  Rewrote
      `node_tree_structure.md` as the link-semantics + mutating-API
      reference.  Reframed `waveform_generator_funcmap.md` as the
      DSL catalogue (33 generators grouped by topic).

- [x] **B.6 — §3 Compiler Pipeline (new internals).**  Commit
      `c0ffacd`.  Wholesale-rewrote `frontend_lowering.md` as
      user-facing reference.  Replaced `prefetch_scheduling.md`
      stub with the three-phase walkthrough + cache model + CWVF
      tracking.  Authored `play_config.md` from scratch.  Trimmed
      `magic_numbers_proposal.md` (Declaration / Usage / source-line
      citations dropped).  Trimmed `asm_parser_grammar.md` (Binary
      Addresses, table forensics, parity section dropped).  Trimmed
      `optimization_passes.md` (heading addresses, Register-field-
      semantics section, Algorithm-reconstructions appendix dropped).
      Final docs build clean at the 12-warning baseline; coverage
      96.9%.

### Phase D7-C — Follow-up audits (separate, deferred)

- **C.1 — `static_cast<>` / magic-number audit.**  Per
      `temp-NOTES-review-seqc-compiler-doxygen.md` §"Code quality
      improvements": audit `static_cast<>` sites for enums that
      should be used, missing enum values, named-constant
      candidates, and enum/int round-trip opportunities (overloads,
      auto-cast).  Not coupled to the docs phase; tracked here so
      it isn't lost.

  - [x] **C.1.a — Audit report.**  Read-only sweep of all in-scope
        subsystems with triage classification (FIX-NOW F1..F10,
        DEFER D1..D7, PROPOSE-ENUM P1..P10, DISMISS).  Deliverable:
        `reconstructed/notes/magic_number_audit.md`.  Cross-checked
        against `notes/magic_numbers_proposal.md`,
        `notes/error_message_audit.md`, `notes/special_registers.md`,
        IF-312.  6 open questions in §10 awaiting review.
  - [x] **C.1bis — ErrorMessageT structural realignment.**
        Out-of-band sub-phase inserted between C.1.a and C.1.b.
        Coupled enum names to format-string templates structurally,
        replacing F1's flat literal-→named-name substitution with a
        full realignment pass.  Commits 8a83a2e, 7cae001, 4b18daa,
        9a90cee.  Findings:
        - 4 value-preserving 3-cycle rotations (IF-313/314/315/316,
          clusters A/C/B/D at slots 12-15, 39-42, 30-32, 250-252)
        - F1's "~57 sites" estimate was a 2.9× undercount; true
          scope was 164 bare-int call sites across 15 files
        - `error_messages.cpp`'s 360 `m[N]` table entries rewritten
          to named-enum indexing so future renames are a compile
          error rather than a silent mis-binding
        - 3 stale alias enumerators removed (`InvalidRegister`,
          `ValueOverflow`, `UnsupportedOp`)
        - Stale prose in `resources.cpp:1269-1276,1549-1553,1585-1587`
          (a prior agent's "halt!" comment about the slot-32 cast
          workaround) rewritten to reflect the resolved state
        - `magic_number_audit.md` updated: F1 marked superseded, D7
          superseded by IF-314/315, D1-D6 IF reservations shifted
          to IF-317..IF-322
        - Net effect: F1 and D7 are both done; remaining FIX-NOW
          batches F2..F10 still pending
        `diff_test_fast` 1613/1613 byte-identical at every sub-batch.
  - [x] **C.1.b — Resolve open questions from audit §10.**
        Resolved by user this session:
        - Q2: F5 stays in `core/types.hpp` (structural reorg out of scope).
        - Q3: Use existing `VarType_Unset=0` / `VarSubType_Default=0`
          as sentinels; scoped-enum migration deferred to its own phase
          (would touch ~250 sites including `(t|0x2)==6` and
          `combineTable[lhs-1][rhs]` arithmetic patterns).
        - Q4: 10 commits, clean bisection.
        - Q5: IF range — next available is IF-329 (IF-325..IF-328
          allocated this session).
        - Q6: Pybind layer not touched.
  - [x] **C.1.c — File DEFER items IF-317..IF-322.**  Done in
        this session.  IF-318 (D2 `NodeType::Loop`) verified and
        substituted in the same commit (trivial FIX-NOW, IF closed
        immediately).  IF-320 (D4 `getAwgDeviceTypeFromString` —
        audit's "parseCpuType" was an inaccurate name) promoted to
        FIX-NOW candidate for follow-up: 9 raw `static_cast<
        AwgDeviceType>(N)` returns, all values mappable to existing
        enumerators.  IF-317/319/321/322 left as documented
        suspicious findings pending GDB verification.
  - [ ] ~~**C.1.d — FIX-NOW batch F1 (ErrorMessageT, ~57 sites).**~~
        Done via C.1bis (commits 8a83a2e, 7cae001, 4b18daa, 9a90cee).
  - [x] **C.1.e — FIX-NOW batches F2..F4 (enum-cast cleanup).**
        F2 `Assembler::Command` 12 sites — 0b0cd7d (also added
        `COMMENT_NOP = 0x4`, filed IF-326).  F3 `AccessMode` 4 code +
        2 doc sites — 95e586a.  F4.a `VarSubType` bulk 213 sites in
        `static_resources.cpp` — d7cf905.  F4.b `VarType`/`VarSubType`
        mixed ~25 sites across 7 files — eb35ca9.
  - [x] **C.1.f — FIX-NOW batches F5..F8 (new/hoisted constants).**
        F5 `kHirzelDevTypeMinus2Mask` 7 sites — 3eb3165 (also fixed
        three wrong-list inline comments, IF-325).  F6
        `kCsvSeparatorMask` 4 sites hoisted — 65a2694.  F7
        `kGoldenRatioHash`/`kHashMul` hoisted — 6dff1b9.  F8
        `kFullScale14`/`15`/`16` rename — 926dbc1.
  - [x] **C.1.g — FIX-NOW batches F9..F10 (sentinel cleanup).**
        F9 `kSuserUserRegBase` → `kAddrOscPhaseReset` — 5961d7c
        (closes IF-312).  F10.a `0xFFFFFFFF` 10 code + 7 doc sites —
        c223242.  F10.b `-1` 8 sites (per-site triage; partial
        scope) — 8131b73 (filed IF-327, IF-328).
  - [x] **C.1.i — ErrorMessages::format arg-count / placeholder-count
        audit.**  Targeted investigation of triggering IFs (IF-323,
        IF-324) done in this session; both **dismissed**:
        - IF-324: template at slot 159 actually has two placeholders
          (`"%1% sample rate can only be described with a const, not
          with a %2%"`); the IF quoted a truncated version with only
          `%1%`.  Binary and recon identical (verified via `strings`
          and end-to-end recompile).
        - IF-323: throw branch unreachable through any current
          caller (`AsmCommands::alui` only invoked with
          ADDI/ANDI/ORI/XNORI, all handled); no better-fit template
          exists in the binary; `UnknownFunction` is the
          closest-available message.  Defensive code; leave faithful.

        The broader 628-site arg-count sweep across all named
        `ErrorMessages::format` call sites is **deferred** — neither
        triggering IF was a real bug, so the priority is low.  If
        pursued: parse template `%N%` counts from
        `error_messages.cpp`, count args at each call site, report
        mismatches.  Defer until either (a) another IF-324-like
        observation arises, or (b) a future phase has spare cycles.
  - [x] **C.1.h — Sub-phase wrap-up.**  Done in commit 8a6b5ec
        (this session): re-ran full diff suite + seqcc tools after
        F10.b, updated `magic_number_audit.md` F-summary with
        per-batch completion status and commit hashes, fixed the
        wrong device-list in §4 for the Hirzel mask (5 devices,
        not 6 — UHFQA is NOT in the family).  IF-312 closed by F9.
        TODO updated for C.1.b/e/f/g.  PROPOSE-ENUM items
        (P1..P10) promotion to Phase D7-C.2 deferred.
  - [x] **C.1.j — FIX-NOW follow-up: IF-320 (`getAwgDeviceType
        FromString` raw returns, 9 sites).**  Done.  Replaced
        9 `static_cast<AwgDeviceType>(N)` returns with named
        enumerators (`UHFLI`, `HDAWG`, `UHFQA`, `SHFQA`, `SHFSG`,
        `SHFQC_SG`, `SHFLI`, `GHFLI`, `VHFLI`).  IF-320 closed.

- **C.2 — PROPOSE-ENUM promotion (P-batch).**  Followup to C.1
      executing the audit's PROPOSE-ENUM items.  Each P-item is
      verified against current source before execution; the audit's
      P1/P8 framing turned out to mistarget the actual code (see
      IF-329/IF-330), so executed scope is literal→named conversions
      against existing enums plus one genuinely-new constant.

  - [x] **C.2.a — P1 `ImmediateKind` literal→named (7 sites).**
        Commit f338d12.  Audit proposed a new `VariantSlot` enum for
        `Value::which_`, but verification found zero consuming
        switches.  The 7 switches in `ast/value.cpp` dispatch on
        `Immediate::index_` (existing `ImmediateKind` enum, A1).
        Pivoted to literal→named.  IF-329 filed.
  - [x] **C.2.b — P4 `kPrefetchAddr20BitMask` (7 sites).**
        Commit fa7ad4d.  Declared `kPrefetchAddr20BitMask = 0xFFFFFu`
        in `core/types.hpp` alongside existing prefetch sentinels.
        Converted 5 sites in `prefetch_emit.cpp` and 2 in
        `prefetch_placesingle.cpp`; `>= 0x100000` boundary tests
        rewritten as `> kPrefetchAddr20BitMask`.
  - [x] **C.2.c — P8 `WaveformFile::Type` literal→named (3 sites).**
        Commit 9f39e82.  Audit proposed extending the enum, but
        verification found the 4 extensions map to only 2 existing
        enumerators (CSV, RAW), and extending would break
        `typeToStr`/`typeFromStr` static maps (binary symbols at
        0x2a3a90 / 0x2a63c0).  Pivoted to literal→named.  IF-330 filed.
  - [x] **C.2.d.1 — P2 device feature bits (7 sites).**  Commit
        f6d9471.  Declared `kDevFlagFF/RTR/Plus/LRT` in
        `device_factories.hpp` under `zhinst::detail`; converted 7
        literal sites across `device_ghf.cpp`, `device_shf.cpp`,
        `device_shfacc.cpp`, `device_vhf.cpp`.  Header-level placement
        justified per the comment block (IF-121 had removed an
        unreferenced `DeviceOpts` namespace; new constants
        re-introduced because they have real call sites this time).
  - [x] **C.2.d.2 — P3 NodeType literal→named (6 sites).**  Commit
        83e5485.  Audit's "separate NodeTypeMask cluster" framing was
        wrong — the 5 literals are exactly the existing `NodeType`
        enumerators (`SyncCervino`/`Table`/`SyncHirzel`/`PlainLoad`/
        `AwgReady`).  Pivoted to literal→named in
        `prefetch_placesingle.cpp` (5 audit-cited + 1 bonus
        `NodeType::Play` swept at line 575).  IF-331 filed.
  - [x] **C.2.d.3 — P7 Assembler::Command literal→named (7 sites).**
        Audit's "new `kOpTag…` constants" framing was wrong — all 8
        literals (`0xF0/F1/F2/F6/F7/F8/FD/FE/FF 000000`) are already
        enumerators in `Assembler::Command` (`asm/assembler.hpp:67`,
        unscoped `uint32_t`).  Pivoted to literal→named in
        `awg_assembler_opcodes.cpp` (lines 473, 477, 569, 575,
        599-601).  IF-332 filed.  Third consecutive PROPOSE-ENUM
        mistarget (P1, P3, P7) — pattern documented in IF-332.
  - [x] **C.2.d.4 — P5 kRegallocBarrierCmdMask (4 sites).**
        IF-317 resolved by enumeration of `Assembler::Command` (no
        GDB required): of values with `cmd+1 ∈ [0,5]`, only
        `INVALID`/`LABEL`/`COMMENT_NOP` are present, and all three
        are documented barriers in the enum itself.  Named
        `kRegallocBarrierCmdMask` at namespace scope in
        `include/zhinst/asm/asm_optimize.hpp` (both consumer files
        already include it as their own class header).  4 sites
        converted in `src/asm/asm_optimize.cpp` and
        `src/asm/asm_optimize_regalloc.cpp`.
  - [x] **C.2.d.5 — P6 kVarTypeScalarNumericMask (6 sites).**
        IF-322 resolved without GDB: of `VarType` values in
        `[0,6]`, only `Var`/`Const`/`Cvar` are integer-valued
        operand categories; excluded types (`Unset`/`Void`/`String`/
        `Wave`) are non-numeric.  Error names at every call site
        confirm the criterion: `SetIntVarConstSecond` /
        `SetDoubleVarConstSecond` for setInt/setDouble RHS,
        `ExpectsOffsetAndLength` for playWave length/offset args,
        `MixedChannelNumbering` for implicit channel indices.
        Pre-existing in-source comment "arg1 must be numeric"
        already used this framing.  Named `kVarTypeScalarNumericMask`
        next to the existing `isConstOrCvar` helper in
        `include/zhinst/runtime/resources.hpp` (already included by
        all three consumer files).  6 sites converted in
        `src/runtime/custom_functions.cpp` (2),
        `src/runtime/custom_functions_play.cpp` (2),
        `src/runtime/custom_functions_registers.cpp` (2).
  - [x] **C.2.d.6 — P9 SuserAddr/kSuser deduplication.**
        Audit's "two parallel naming schemes" framing was
        misleading.  Survey: `kSuser*` (`core/types.hpp`) has 100
        call sites across 36 distinct names;
        `DeviceConstants::SuserAddr` (`device/device_constants.hpp`)
        has **0** call sites — it was recon-invented and never
        referenced (consistent with the existing assessment at
        `notes/symbol-renaming-audit/31_device_constants.md:493`,
        which had already flagged the wrapper as "recon-invented").
        Resolution: deleted the entire `SuserAddr` struct from
        `device_constants.hpp` (replaced with a comment pointer to
        `core/types.hpp`), added the one missing `kSuserGenericUser0
        = 0x00` entry to `core/types.hpp` under a new
        "Sync handshake (HDAWG only)" sub-section, swept the 2
        `0`-literal call sites in
        `src/runtime/custom_functions_registers.cpp:399,401`.  Net:
        single canonical naming, no remaining duplication.
  - [x] **C.2.d.7 — P10 Category B long-tail sweep.**
        Site survey found earlier phases had already cleaned
        nearly all Category B entries (see IF-334 for the per-item
        table).  Real (non-comment, in code) literal sites
        remaining:
        - B1 `VarType` — 1 site
          (`custom_functions.cpp:1088`, `varType_ == 4`).  Swept to
          `VarType_Const`.  Adjacent `varSubType_ == 2` literal
          left unrenamed pending IF-333 verification (the recon
          comment labels it a "marker" but the enum value 2 is
          `VarSubType_FunctionArg`).
        - B9 `Assembler::INVALID` — 0 real sites; one stale
          `cmd == -1` comment at `asm_optimize.cpp:152` updated for
          consistency.
        - B3 `DeviceTypeCode(33)` — gated by IF-319 (unknown-sentinel
          handling); not in scope for P10.
        - B10 `ErrorMessageT` — owned by C.1bis.
        - All other B-items (B2/B4/B5/B6/B7/B8/B11) had no remaining
          literal sites in code.
        IF-334 filed to record that the audit's "long tail"
        framing was based on stale surveys.
  - [x] **C.2.d — All 10 P-items landed or formally deferred.**
        P1/P2/P3/P4/P5/P6/P7/P8/P9/P10 done (sub-entries
        C.2.d.1..C.2.d.7 plus the initial f338d12/fa7ad4d/9f39e82
        batch).  Out-of-scope deferrals: P-row B3
        (`DeviceTypeCode(33)` Unknown sentinel) gated by IF-319;
        P-row B10 (`ErrorMessageT`) owned by C.1bis.
  - [x] **C.2.e — Sub-phase wrap-up.**  Audit doc (§8 PROPOSE-ENUM
        table) updated with per-item DONE markers for all 10 rows;
        IF-329 (P1 mistarget), IF-330 (P8 mistarget), IF-331 (P3
        mistarget), IF-332 (P7 mistarget), IF-333 (`varSubType_ == 2`
        ambiguity surfaced during P10), IF-334 (P10 audit
        description was stale) recorded; IF-317 and IF-322
        marked fixed (P5/P6 resolutions).  TODO.md updated.

## Phase X — `compile_seqc` binding-kwarg coverage

Differential-test coverage of every keyword argument in the public
`compile_seqc(code, devtype, options, index, **kwargs)` Python
binding.  Driven by `help(_seqc_compiler.compile_seqc)`.

Status snapshot (after this phase's first wave):

| kwarg        | coverage             | notes                                              |
|--------------|----------------------|----------------------------------------------------|
| `samplerate` | full (HDAWG)         | every HDAWG case                                   |
| `sequencer`  | `qa`/`sg`/`auto`     | added `sequencer_auto_shf{qa,sg}` in this phase    |
| `wavepath`   | `""` form only       | autoscan form covered in X2 (`wavepath_autoscan_dir`) |
| `waveforms`  | string-list form     | autoscan (`None`) form fixed in X2 (IF-292 closed) |
| `filename`   | full                 | X1 fixed IF-335, three cases added                 |
| `options`    | smoke only           | string accepted but no ELF-visible diff; X3 below  |

- [x] **X1 — `filename` kwarg coverage.**  Done.  Root-cause
      diagnosis recorded as IF-335: `compile_seqc.cpp:314`
      hardcoded `"output"` as the second arg to `writeToStream`;
      the local `filename` was misrouted into `config.debugDumpPath`
      and never reached the ELF writer.  Disassembly of
      `compileSeqc @0xf58a0` showed the binary initialises its
      local `filename` string (rbp-0xb0) to `"output"` via four
      `mov` immediates at `0xf5e79..0xf5e93`, then conditionally
      overwrites it from the JSON `"filename"` key.  Fixes:
      defaulted `filename = "output"` + provided-flag tracking in
      both `reconstructed/src/codegen/compile_seqc.cpp` and
      `tools/seqcc/src/driver.cpp`; forwarded `filename` directly
      to `writeToStream`; changed `awg_compiler.cpp:1551-1558` to
      pass the full `format` (not basename) to `getJsonArguments`;
      removed the divergent auto-populate block at
      `tools/seqcc/src/main.cpp:466-472`.  Added three manifest
      cases (`filename_simple`, `filename_with_path`,
      `filename_empty`) under `core:general` with the
      `compile-kwargs` tag.  1616/1616 difftests, 70/70 pytest.

- [x] **X2 — IF-292 follow-up: `wavepath` + `waveforms=None` autoscan.**
      **Done** in three commits.
      - **A+B** (`80c6f96`) — `pybind_seqc.cpp::pyCompileSeqc` rewritten
        type-aware (`None`→`null`, `bool`/`int`/`float`/`str`/`bytes`
        each preserved; unknown silently skipped).  Strict validators in
        `compile_seqc.cpp` for the four user-facing kwargs
        (`samplerate`, `filename`, `wavepath`, `waveforms`) with
        binary-exact error messages.  Added
        `filename_none_rejected`, `wavepath_none_rejected`,
        `waveforms_none_no_wavepath` manifest cases under
        `core:general` with the `kwarg-type-validation` tag.
      - **C** (this commit) — GDB-confirmed at `@0xf6492` that the
        binary unconditionally appends `wavepath` onto the
        `waveformPaths` vector whenever both `waveforms` and
        `wavepath` keys are present (string or `null`).
        `compile_seqc.cpp` now mirrors that push.
        `AWGCompilerImpl::addWaveforms` gained a pre-pass that expands
        directory entries via
        `boost::filesystem::recursive_directory_iterator` (matching
        the binary path at `@0x10535a`).  New
        `wavepath_autoscan_dir` manifest case is byte-identical.
        Remaining sub-case (`waveforms` *absent* + default
        `Data/awg/waves` folder probe at `@0xf64a1`) is untested by
        any existing manifest case; tracked as a future
        enhancement.  IF-292 closed.

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

- [X] **T3b — JSON IR dumps (partial).**  Implemented `--dump=asm`,
      `--dump=waveforms`, `--dump=wavemem`, `--dump=compile-report`
      via ELF section extraction (asm/waveforms/wavemem) and
      info-JSON parsing (compile-report) — no recon edits required.
      `--dump-dir=` and `--dump-format=` wired (format is advisory in
      T3b).  Each `--dump=` is repeatable and accepts optional `:path`.
      Also fixed IF-300: T3a's repeatable `-mdevopt`/`-mtune` silently
      dropped all but the last occurrence.

- [X] **T3c — AST-lowered dump.**  Implemented `--dump=ast-lowered`.
      Sanctioned recon edit under AGENTS.md "Tooling vs reconstructed
      code" policy (approved by user before edit; documented as
      IF-301): added a minimum-footprint introspection sink
      (`CompileSeqcIntrospection`) and a second compile entry point
      `compileSeqcWithIR()` that shares its body with `compileSeqc()`
      via a static `compileSeqcImpl()` — so ELF output is byte-equal
      whether the driver requests dumps or not (verified by a new
      test `test_dump_ast_lowered_preserves_elf_byte_equality`).
      AST root reachable via free function
      `compilerLoweredAst(AWGCompiler const&)` with a single
      `friend` grant; no public method added to `AWGCompiler`.
      Driver passes an identity `asmId → asmId` map to
      `Node::toJson()` because the proper densified map (built in
      `AsmList::serialize()` pass 1) requires capturing the AsmList
      alongside the AST — deferred (IF-302) until a second
      asm-stage dump kind needs it.

- [x] **T3d (follow-up) — ast-lowered idMap parity.**  Captured the
      `AsmList` snapshot into `CompileSeqcIntrospection` via a new
      `AWGCompilerImpl::getAsmList()` accessor (Strategy B — no new
      friend grant; the existing `fillIntrospection()` friend
      already reaches the pimpl).  The accessor returns a
      `shared_ptr<AsmList const>` copy of `compiler_.asmList_`
      so the captured list outlives the AWGCompiler's destructor.
      Driver-side: `dump.cpp::buildDenseIdMap` runs pass-1 of
      `AsmList::serialize()` (skip NOPs and empty placeholders,
      assign sequential indices keyed by `sequenceId`); the
      resulting map is fed to `Node::toJson()`.  The previous
      identity-map workaround is retained only as a safety net
      (`fillMissingIdsIdentity`) for AST nodes whose `asmId` isn't
      present in the captured AsmList — without it,
      `Node::toJson(idMap)` would throw `std::out_of_range`.
      During implementation discovered that pybind has no
      "ast-lowered" dump at all — `Node::toJson(idMap)` is only
      called inside `AsmList::serialize()`, so the framing
      "byte-identical to pybind" in IF-302 was incorrect; corrected
      in the IF-302 update.  +3 tests in `T3dAstLoweredIdMap`
      (deterministic dump, valid JSON, no out-of-range across
      several program shapes).  Verified: diff_test_fast 1612/1612,
      seqcc_diff 50/50.

- [ ] **T4 — Text-only IR dumps.**  Add `--dump=ast-raw`,
      `--dump=ast-seqc`, `--dump=prefetch`, `--dump=cache`,
      `--dump=resources`.  Where the existing `print()` writes to
      `std::cout` only, add a `print(std::ostream&)` overload
      delegating to the existing implementation (minor cleanup of
      reconstructed code).  Where the existing API already takes a
      stream, use it directly.  Extend `CompileSeqcIntrospection`
      (the T3c sink) with whatever stage-specific IRs are needed
      — that's the supported extension point now; do **not** add
      more friend grants or free helpers on `AWGCompiler`.

  - [X] **T4-prep — strategy-B introspection refactor.**  Retired
        per-stage `compilerLoweredAst()` friend grant + free helper
        in favour of a single `fillIntrospection(AWGCompiler const&,
        CompileSeqcIntrospection&)` friend declared in
        `compile_seqc.hpp`.  All future Phase-T IR captures extend
        the helper body + struct fields rather than adding more
        friend grants on `AWGCompiler`.  Zero behaviour change.

  - [X] **T4a — `--dump=wavetable` (text).**  First T4 dump kind,
        sourced from a new `wavetable` field on
        `CompileSeqcIntrospection` (populated by `fillIntrospection()`
        via a private `AWGCompilerImpl::getWavetable()` accessor),
        rendered via `WavetableFront::toString()` — the only public
        wavetable serialiser the recon exposes.  JSON variant
        deferred (IF-303): `WavetableFront::toJson()` doesn't exist
        in the recon, only the private inner
        `WavetableManager<>::toJson()` does, and exposing it would
        require another friend grant outside our single-friend
        principle on `AWGCompiler`.  ELF byte-equality regression
        confirmed.

  - [ ] **T4b — text dumps from `std::cout`-bound print methods.**
        `--dump=prefetch`, `--dump=cache`, `--dump=resources`.
        Each requires a `print(std::ostream&)` overload on the
        respective recon class (currently `std::cout`-only —
        `Prefetch::print(node, indent)`, `Cache::print()`,
        `Resources::print()/printAll()/printScopes()`).  Per-method
        sanctioned recon edits; each documented as its own IF.
        Sink-side: capture the relevant object handles into
        `CompileSeqcIntrospection`.  Discuss approach with user
        before starting — multiple small recon edits add up.

  - [ ] **T4c — `ast-raw` / `ast-seqc`.**  Requires the `Compiler`
        to optionally retain `seqcAst` (the parser-output AST,
        currently a local in `Compiler::compile()` destroyed before
        return).  A `bool keepRawAst_` config flag plus a
        `std::shared_ptr<SeqCAstNode> rawAst_` member on `Compiler`,
        gated so default callers pay zero cost.  Larger recon edit
        than T4a/T4b; discuss before starting.

- [X] **T5 — `-S`, `-E`, `--to=<stage>`.**  Stop-after-stage routing
      for the primary `-o` output.  Implemented in commit (T5):
      - New `tools/seqcc/src/stage.{hpp,cpp}` defines the canonical
        pipeline-stage table consumed by both CLI validation
        (`CLI::IsMember(stageNames())`) and `--print-stages`.
      - `--to=STAGE` selects the primary output; `-E` is sugar for
        `--to=lower`, `-S` for `--to=asm`, `-c` for `--to=link`
        (default).  Rightmost wins, matching gcc semantics.
      - `dump.cpp` gains `renderStagePrimary(stage, elf, sinks)`,
        shared with the existing `--dump=ast-lowered` and
        `--dump=asm` renderers — so `-E -o X` and
        `--dump=ast-lowered:X` produce byte-identical files.
      - `-o -` writes the primary artifact to stdout (pipe-friendly
        for `-E | jq .` and `-S | less`).
      - Supported stages today: `link`, `lower`, `asm`.  Other
        known stages (`parse`, `astgen`, `opt-pre`, `prefetch`,
        `opt-post`, `assemble`) are listed by `--print-stages` as
        `(unsupported)` and rejected at parse time with a clear
        diagnostic.
      - 9 new test cases in `tests/tools/test_seqcc_diff.py`
        (default ≡ explicit, `-E`/`-S` parity with `--dump=`,
        stage rejection, `--print-stages`, rightmost-wins).
      - **Caveat**: `--to=` is a *logical* stop, not a literal one
        — the full pipeline always runs internally and only the
        output routing changes.  Documented as **IF-304**.
      - Driver version bumped to `0.5.0-T5`.
      - Verified: `diff_test_fast` 1612/1612; `test_seqcc_smoke` 4/4;
        `test_seqcc_diff` 35/35.

  - [x] **T5a — seqcc-owned outer driver (`SeqcDriver`).**  Move
        the ~30-line outer compile flow (`AWGCompiler` ctor →
        `addWaveforms` → `compileString` → `writeToStream` → assemble
        compile-report + wavemem-JSON → pack) from
        `compileSeqcImpl()` into a new `tools/seqcc/src/driver.{hpp,
        cpp}` class.  Replicates config-marshalling on the driver
        side (built by the existing `target.cpp` from `-march`/
        `-mtune`).  **Zero recon edits.**  Initially gated by a
        `SEQCC_USE_OWNED_DRIVER=ON/OFF` CMake option so the existing
        `compileSeqcWithIR` path remains a fallback during T5a–T5c.
        A/B byte-equality regression: extend `test_seqcc_diff.py`
        to run every existing case through both paths.  Prerequisite
        for T5b and T6.  No new IFs expected (driver-side only).
      - [x] **T5a.1** — driver skeleton: `tools/seqcc/src/driver.{hpp,
            cpp}` with `SeqcDriver::compile()` pass-through to
            `compileSeqcWithIR()`; CMake `SEQCC_USE_OWNED_DRIVER`
            option (default OFF); zero call sites.
      - [x] **T5a.2** — own the outer flow: port `compileSeqcImpl`
            L159–360 into `SeqcDriver::compile()` (JSON-config parse,
            sequencer/device dispatch, `AwgDeviceProps` lookup,
            waveform-path defaults, `AWGCompilerConfig` population,
            `AWGCompiler` ctor + `addWaveforms` + `compileString` +
            `writeToStream`, info-JSON assembly, `fillIntrospection`).
            `compile.cpp::runCompile()` routes through `SeqcDriver`
            under `#ifdef SEQCC_USE_OWNED_DRIVER`.  Two anonymous-
            namespace helpers (`parseSequencerType`,
            `dispatchGetAwgDeviceProps`) re-implemented locally in
            `driver.cpp` — no recon edits.  Verified gate ON:
            seqcc_diff 46/46 byte-equal, diff_test_fast 1612/1612.
      - [x] **T5a.3** — A/B fixture: second `seqcc_owned` CMake
            target with `SEQCC_USE_OWNED_DRIVER=1` always defined,
            plus `tests/tools/test_seqcc_ab.py` (15 cases, reusing
            `test_seqcc_diff.py::CASES`) asserting byte-identical
            ELFs between `seqcc` (legacy) and `seqcc_owned`
            (driver).  Both targets retire alongside the option at
            T10a.  Verified 15/15.
      - [x] **T5a.4** — flip the default and prepare for T5b: set
            `SEQCC_USE_OWNED_DRIVER` default to ON in
            `tools/seqcc/CMakeLists.txt` (driver is now the
            primary path; legacy `compileSeqcWithIR` retained only
            as a comparison reference until T10a).  Update
            DESIGN.md §5.4 — withdraw the `passTap_` approach,
            point to T5b as the stepwise plan.  Bump driver
            version to `0.8.0-T5a`.

  - [ ] **T5b — stepwise compilation API (supersedes T5
        follow-up).**  Refactor `Compiler::compile()` (the 779-line
        monolith in `reconstructed/src/codegen/compiler.cpp` L235–
        ~780) into 9 public step methods on `Compiler` named for
        the 12 annotated pipeline steps (`stepParse`,
        `stepToSeqCAst`, `stepLower`, `stepBuildAsmPreamble`,
        `stepOptPre`, `stepPrefetch`, `stepOptPost`,
        `stepUnsyncCervino`, `stepProject`).  Lift cross-step stack
        locals (`expr`, `seqcAst`, `lowerResult`, `resources`,
        `staticResources`, `rootNode`) into private `Compiler`
        members.  `Compiler::compile(source)` is rewritten as the
        canonical step sequence.  Then refactor
        `AWGCompilerImpl::compileString()` similarly into
        `stepInnerCompile` + `stepAssembleOpcodes` +
        `stepCheckLimits`; public `AWGCompiler::compileString(s)`
        stays a one-liner.  **Structure-preserving refactor** —
        diff_test_fast 1612/1612 is the byte-equality invariant.
        Sanctioned recon exception authorised by the binary
        addresses already documented inline in
        `reconstructed/src/codegen/compiler.cpp` (verified comment
        anchors at L234, L248, L251, L300, L327, L350, L453, L496,
        L506, L510, L519, L543).  IF-306 documents the refactor
        and the address cross-references in full.  Driver consumes
        the new API: `--to=<stage>` becomes a literal stop (closes
        IF-304); new dump artefacts unlocked (`asm-pre-opt`,
        `asm-post-pre-opt`, `asm-post-prefetch`, `asm-final`,
        `wavetable-ir`).  DESIGN.md §5.4 `passTap_` approach was
        withdrawn at T5a.4 (superseded; no longer compatible with
        AGENTS.md `6b2d504` hands-off policy).
      - [x] **T5b.1** — open the entry: file IF-306 with the
            verified-address authorisation table and the risk
            register (member ordering, state leakage between
            compiles, exception safety in step boundaries,
            `reset()` semantics under partial pipelines).
      - [x] **T5b.2** — lift `Compiler::compile()` cross-step
            locals (`expr`, `seqcAst`, `lowerResult`, `resources`,
            `staticResources`, `rootNode`, `placeholderAsm`,
            `wavetableIR`, `optimizer`) to private members.  Body
            still monolithic; only the storage moves.  Update
            `Compiler::reset()` to zero the new members.  Verify
            `diff_test_fast` 1612/1612 holds.
      - [x] **T5b.3** — split `Compiler::compile()` into the 9
            step methods, calling them sequentially from the
            public entry point.  Verify the suite after each
            split (not just at the end) so a regression
            bisects to a single step boundary.
      - [x] **T5b.4** — promote the 9 step methods to public in
            `compiler.hpp`.  Pure visibility change.
      - [x] **T5b.5** — split `AWGCompilerImpl::compileString()`
            into `stepInnerCompile` + `stepAssembleOpcodes` +
            `stepCheckLimits`; promote.  Rewrite
            `SeqcDriver::compile()` to call the three step
            forwarders, branching on `opts.toStage` to skip the
            back end for `--to=lower` / `--to=asm`.  **Scope:
            partial AWGCompiler-level short-circuit** —
            `Compiler::compile()` still runs to completion inside
            `stepInnerCompile`; deeper short-circuit (skipping
            front-end steps) would require exposing `Compiler`'s
            stepwise interface on `AWGCompiler` and is deferred.
            New test `tests/tools/test_seqcc_to.py` (4 cases)
            asserts user-visible payload shape for the three
            stages.  Partially closes IF-304 (back-end stages
            literally skipped; front-end stages still run).
      - [x] **T5b.6** — wrap-up: regression sweep all green
            (5 tool-test gates + diff_test_fast 1612/1612);
            IF-306 status → "fixed"; DESIGN.md §3 refreshed
            with step-method mapping table (new §3.1) and the
            pipeline diagram now shows the `Compiler::compile`
            / `AWGCompilerImpl::compileString` partitions;
            DESIGN.md §5.4 withdrawal note updated to mark the
            T5b replacement as landed; driver version bumped to
            `0.9.0-T5b`.

- [ ] **T6 — `--from=asm` (re-scoped).**  Start-at-stage entry
      for the assembly IR.  **Re-scoped 2026-05** per
      `reconstructed/notes/seqcc_from_design.md`: the original
      sketch covered three modes (`ast-lowered`, `asm`,
      `wavetable-ir`); data-flow analysis showed only `--from=asm`
      is round-trippable from a single input file.  The other two
      modes are deferred to T6-future entries below.
      Input is auto-detected from the `.seqasm` extension; `-x asm`
      forces.  Resume boundary is between `stepOptPre` and
      `stepPrefetch` — the same point exercised by the binary's
      `serializeRoundTrip` debug flag at `compiler.cpp:598-602`
      (corrected from earlier design note per **IF-308**).
      **Depends on T5b** and adds a sanctioned recon edit
      (**IF-307**, pre-approved): `AWGCompiler::compiler()` pImpl
      accessor, factored public `Compiler::setupResources()`
      helper (code-move from `stepToSeqCAst:351-363`), two narrow
      public setters `setAsmList` / `setPlaceholderAsm`.  T6.2
      also addresses **IF-308** by refactoring `--to=asm` to dump
      at the binary's natural cut point.  Test contract: strict
      byte-equal ELF round-trip.

  - [x] **T6.1 — recon edits (IF-307).**  Add forward-declared
        `class Compiler;` + public `Compiler& compiler()` accessor
        to `awg_compiler.hpp`; implement in `awg_compiler.cpp`.
        Factor `setupResources()` out of `stepToSeqCAst`; add
        `setAsmList` / `setPlaceholderAsm` to `compiler.hpp`/`.cpp`.
        Verify 5 gates green (`diff_test_fast` 1612/1612,
        `test_seqcc_diff` 46/46, `test_seqcc_ab` 15/15,
        `test_seqcc_smoke` 4/4, `test_seqdump` 16/16,
        `test_seqcc_to` 4/4) — pure additive change, byte-equality
        must hold.  **Landed**, commit `fa0229c`.  Note: the
        added `stepInnerCompileFromAsmList` overload + `setAsmList`
        / `setPlaceholderAsm` setters are now dead-code after the
        T6.2 Q3 demotion (no driver caller), but the cost of
        reverting was judged not worth the churn.

  - [x] **T6.2 — driver wiring + round-trip test (Q3-demoted).**
        Original plan (a + b below) was partially executed; the
        round-trip resume path (b) was demoted to Q3 (deferred
        indefinitely) when implementation surfaced that
        reconstructing the post-`stepOptPre` state cleanly requires
        transporting more than just the AsmList (WavetableIR,
        `Compiler::ast_`, possibly more sidecars).  See **IF-308**
        Resolution section for the full rationale.

        **(a) LANDED** — `--to=asm-pre` stage: driver drives the
        front end stage-by-stage via the IF-307 `compiler()`
        accessor (`reset → setupResources → stepParse →
        stepToSeqCAst → stepLower → stepBuildAsmPreamble →
        stepOptPre`), then captures `asmList_.serialize()` into
        `IRSinks::preprefetchAsmText` for dump rendering.  This is
        a one-way diagnostic dump only.  `--to=asm` continues to
        dump the post-pipeline assembly listing (relabelled as a
        debug artifact in DESIGN.md).

        **(b) DROPPED (Q3)** — `--from=asm` CLI option, `-x LANG`
        option, sidecar transport, byte-equal-ELF round-trip test.
        All in-flight driver code reverted.  The IF-307
        `stepInnerCompileFromAsmList` / `setAsmList` /
        `setPlaceholderAsm` recon-side surface remains in the
        binary's reconstruction as landed (now dead-code).

        **Incidental fidelity fixes LANDED** during the round-trip
        debugging (independently correct, kept regardless of Q3):
        - **IF-309**: lexer/parser keyword `placeholder_line` →
          `placeholder` + line-tail capture re-routed through the
          existing `#`-comment lex rule.
        - **IF-310**: `AsmParserContext::addNode()` restored
          `comment` / `hasComment` writeback (GDB-verified at
          `+0x28c243-0x28c297`).
        - **IF-311**: `assembleStringToExpressionsVec()` retargeted
          to write `nopComment` (+0x20) / `noOptOverride_` (+0xA0)
          instead of `comment` (+0x80) / `hasComment` (+0x98)
          (objdump-verified at `+0x286e40`).
        - All three gated by `ZHINST_RECON_ASMLIST_KEYWORD_FIX`
          (ON by default).

  - [x] **T6.3 — wrap-up.**  Driver version bumped to `0.10.0-T6`.
        IF-307 status: landed but partially dead-code.  IF-308
        status: partially fixed (`--to=asm-pre` dump-point
        correction landed; round-trip resume Q3-demoted).
        IF-309/310/311 status: fixed (gated).  DESIGN.md §T6
        updated with the Q3-demotion narrative.

- [ ] **T6-future-a — `--from=ast-lowered`.**  Deferred from T6.
      The existing `--dump=ast-lowered` artefact is a single-node
      `Node::toJson(idMap)` blob; no whole-tree `Node` dump format
      exists outside `AsmList::serialize` / `deserialize`.  Would
      require designing and emitting a whole-tree format on the
      `--to=` side first, then writing a matching deserialiser.
      Re-open if a concrete use case appears.

- [ ] **T6-future-b — `--from=wavetable-ir`.**  Deferred from T6.
      `stepOptPost` reads both `asmList_` and `compileWavetableIR_`,
      so a single input file is insufficient.  Would require a
      multi-file `--from=` convention or a packed driver-input
      format that bundles both IRs.  Re-open if a concrete use
      case appears.

- [ ] **T7 — `seqas` mode + `-x asm` dual path.**  When invoked as
      `seqas` (or `seqcc -x asm`), route `.seqasm` inputs through the
      `AWGAssembler` legacy entry point (entry-point offset `+0x80`).
      Also expose `--from=asm` on the main pipeline (the
      `Compiler::compile` path's assemble stage) so users can compare
      both paths.  Document the e_entry difference in driver `--help`.

- [x] **T8a — `-O<n>` / `-f[no-]<pass>` / `--print-passes`.**
      Plumb `AWGCompilerConfig::optimizationFlags` through a new
      `optimizationFlags` jsonConfig key (sanctioned recon edit,
      IF-305 — pre-approved by user, "Path 1": one-shot additive
      read of an existing config field, default 0xFF preserves
      byte-identical behaviour for all pre-T8 callers).  Driver:
      `tools/seqcc/src/optflags.{hpp,cpp}` defines the pass table
      (jump-elim 0x01, label-cleanup 0x02, dead-code-elim 0x04,
      reg-zero-merge 0x08, reg-alloc 0x10), curated levels
      (`-O0`..`-O3`; `-O2` computed from table, `-O3` = 0xFF for
      gcc parity), `applyPassToggle`, `printPassTable`.
      `main.cpp::extractOptFlags` pre-parses argv before CLI11
      sees it (gcc-style: many short flags sharing the `-O`/`-f`
      prefix don't fit CLI11's option model).  Rightmost-wins on
      `-O` conflict; `-f<pass>` composes on top of preceding
      `-O<n>` (or seeds 0xFF when no `-O` was given).  Unknown
      `-f<name>` left in argv for CLI11 to surface as
      "unrecognised option".  `options.cpp::buildJsonConfig`
      emits `"optimizationFlags": N` only when the user actually
      passed a flag.  Tests (`T8OptimizationFlags`, +8 cases):
      default == pybind baseline, `-O3` == default, `-O0` differs
      and is larger, `-O0 -freg-alloc` composes, `-fno-reg-alloc`
      differs, unknown pass rejected, `--print-passes` lists
      passes + levels, rightmost-`-O` wins.  Verified:
      diff_test_fast 1612/1612, seqcc_diff 43/43, smoke 4/4.
      Driver version `0.6.0-T8`.

- [ ] **T8b — Optimisation-pass dump taps.**  Add the
      `std::function<void(std::string_view, …)>` taps to `AsmOptimize`,
      `Prefetch`, `Cache`.  Insert tap calls at each named pass
      boundary.  Implement `--dump-after=<pass>` and
      `--dump-before=<pass>` in the driver.  Update
      `notes/optimization_passes.md` with the pass name → bit
      mapping (T8a already cross-checked the mapping against
      `asm_optimize.cpp` L236/L252/L257/L263/L268).

- [x] **T9 — `seqdump` mode.**  Port the section-listing logic from
      `tests/dump_elf.py` to C++ in `tools/seqcc/seqdump.cpp` (or as
      a sub-mode of the main binary).  Supports `--section=<name>`,
      `--all`, `--diff <other.elf>` (side-by-side comparison like the
      Python tool's `--both`).
      - **Status:** done.
      - **Commit:** TBD.
      - **What landed.**  New `tools/seqcc/src/seqdump.{hpp,cpp}` —
        hand-rolled argparser (no CLI11; seqdump's surface is too
        small and too gcc-style for CLI11 to add value).  Built as a
        `seqdump` symlink to the `seqcc` binary (POST_BUILD custom
        command in `tools/seqcc/CMakeLists.txt`); dispatch happens
        via argv[0] before any seqcc CLI11 / gcc-flag rewriting in
        `main.cpp`.  Driver version bumped to `0.7.0-T9`.
      - **Features.**  Single-ELF dump enumerates every section with
        a heuristic pretty-printer matching `tests/dump_elf.py`:
        `.text` decoded as 32-bit LE instruction words; `.linenr`
        decoded as 16-byte `(abs, ctr, seq, line)` records;
        `.channels` / `.version_bin` printed as hex; `.wf_*` printed
        as a 16-sample 16-bit LE preview; textual sections detected
        by printable-ASCII heuristic; other sections shown as
        64-byte hex preview.  `--section=<name>` (repeatable)
        filters; unknown section warns but exits 0.  `--max-lines=N`
        (default 200) truncates long text dumps.
      - **Diff mode.**  `--diff <other.elf> <elf>` reports each
        section as `IDENTICAL`, `DIFFERS`, or `MISSING IN
        LEFT|RIGHT`; differing sections print full side-by-side
        decoded contents with `--- LEFT ---` / `--- RIGHT ---`
        labels.  Trailing summary line `=== N section(s) differ
        ===`.  Exit always 0 — `seqdump --diff` is a reporting
        tool, not a pass/fail gate (callers grep `DIFFERS` or parse
        the count to gate).
      - **Tests.**  New `tests/tools/test_seqdump.py` — 16 cases
        covering: symlink topology, `--version` / `--help` /
        no-args, unrecognised flags, full-dump enumerates expected
        sections (`.text`, `.asm`, `.waveforms`, `.wavemem`), `.asm`
        decoded as text, `.linenr` decoded as records, single &
        repeated `--section=` filter, unknown-section warning,
        non-ELF rejection, identical-vs-self diff reports `=== 0
        section(s) differ ===`, default-vs-`-O0` diff reports
        non-zero count, `--- LEFT ---` / `--- RIGHT ---` labels,
        `--section=` honoured in diff mode.
      - **Recon edits.**  None — pure driver work.  Reused existing
        `tools/seqcc/src/elf_reader.{hpp,cpp}` for ELF parsing.
      - **Held until T7.**  `seqas` symlink not yet created — Phase
        T7 owns assembler personality.
      - **Verified.**  diff_test_fast 1612/1612, test_seqcc_smoke
        4/4, test_seqcc_diff 46/46, test_seqdump 16/16.

- [x] **T10 — Wrap-up.**  Landed 2026-05.  Added toolchain section
      to `OVERVIEW.md` (between File Structure and Open Questions)
      with version, pipeline mapping, IR-capture mechanism, and
      test-gate summary.  Added `reconstructed/notes/tools.md` as
      the topic-organised cross-reference entry point (indexes
      `tools/seqcc/DESIGN.md`, the stage table, source layout, IR
      capture accessors, and test gates).  Final diff_test_fast
      suite: 1612/1612 green (run at T10a wrap-up).  Tool tests:
      70/70 across `test_seqcc_smoke` + `test_seqcc_diff` +
      `test_seqcc_to` + `test_seqdump`.  Deferred items
      (`--from=ast-lowered`, `--from=wavetable-ir`, `seqld`,
      `ast-seqc` deserialisation) are filed in DESIGN.md §7 and
      the "T6-future" TODO entries.  **Phase T complete.**

  - [x] **T10a — Retire `compileSeqcWithIR`.**  Landed 2026-05.
        Deleted `compileSeqcWithIR()` + `CompileSeqcIntrospection`
        from `reconstructed/include/zhinst/codegen/compile_seqc.{hpp,
        cpp}`; deleted `fillIntrospection()` and the matching
        friend grant on `AWGCompiler`; deleted the three private
        `AWGCompilerImpl::get{LoweredAst,Wavetable,AsmList}()`
        accessors.  Driver-side: `tools/seqcc/src/ir_sinks.hpp` is
        now a standalone struct (no recon-header dependency) with
        its own owning fields; `SeqcDriver::compile()` populates
        them directly via `AWGCompiler::compiler() →
        Compiler::{ast(), wavetable(), asmList()}` (IF-307 +
        T10a-added `ast()` / `wavetable()` accessors parallel to
        the existing T6.1 `asmList()` accessor).  Removed the
        `SEQCC_USE_OWNED_DRIVER` CMake option, the `seqcc_owned`
        A/B target, and `tests/tools/test_seqcc_ab.py`.  Pybind's
        `compileSeqc()` keeps its current shape unchanged.  Driver
        version bumped to `0.11.0-T10a`.  Verified: diff_test_fast
        1612/1612, test_seqcc_smoke 4/4, test_seqcc_diff 46/46,
        test_seqcc_to 4/4, test_seqdump 16/16.  Closes IF-301
        (introspection scaffold retired).

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
| [`IF_201-300.md`](reconstructed/notes/archive/IF_201-300.md) | Closed Incidental Findings IF-201..IF-300 (91 entries; IF-292 now closed) | ~6280 |
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
  — active entries: 2 still-open IFs (IF-100, IF-102).
  All closed / deferred / tracking-only entries archived to
  [`archive/IF_1-99.md`](reconstructed/notes/archive/IF_1-99.md),
  [`archive/IF_100-200.md`](reconstructed/notes/archive/IF_100-200.md),
  and
  [`archive/IF_201-300.md`](reconstructed/notes/archive/IF_201-300.md).
- [`reconstructed/notes/`](reconstructed/notes/) — topic-organized
  technical notes (struct layouts, opcodes, optimization passes, etc.).
- [`AGENTS.md`](AGENTS.md) — working process, GDB recipe,
  differential-testing workflow.
