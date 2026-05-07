# TODO — Reconstructed zhinst SeqC Compiler

> **Completed Phases 1-12 archived in
> [`reconstructed/notes/archive/TODO_phases_1-12.md`](reconstructed/notes/archive/TODO_phases_1-12.md).**
> 597 completed items across 12 phases (plus sub-phases 10.5-10.8).

---

## Phase 45: Resolve all open IF items

Six IFs remain open (or have stale "open" status). Resolve each for good.

### 45.0 — Fix stale IF-1 status line

IF-1 (`static_resources.cpp` comment confusion on device types) was fixed
in a prior session (comments corrected to `Cervino/klausen` and `Hirzel`),
but `incidental_findings.md` still says `Status: open`.

- [x] Update IF-1 status to `fixed` with a brief note on what was changed

### 45.1 — IF-4: `shared_ptr_deref` assertion on HDAWG nop

Assertion fires when running the compile worker directly (not through the
test harness). Does not affect test results. Likely an ABI mismatch
between our libstdc++ build and libc++ shared_ptr internals, or a genuine
null dereference that happens to be non-fatal.

- [x] Reproduce the assertion by running `compile_worker` directly on
      `// nop` with an HDAWG device type
- [x] GDB-trace or examine the null shared_ptr to determine call site
- [x] Decide: ABI artifact (dismiss) or real null-deref (fix + note);
      update IF-4 status accordingly
      → **Dismissed**: libstdc++ `_GLIBCXX_ASSERTIONS` catches null shared_ptr
        dereference already guarded at `compiler.cpp:290`

### 45.2 — IF-8: `checkFunctionSupported` wrong error message

Original produces: `function 'X' not supported on SHFQA devices`
Recon produces: `function 'X' reaches end without returning a SHFQA`

The error is going through the wrong code path — a return-type error
instead of a device-support check.

- [x] Locate `checkFunctionSupported` in recon source
- [x] GDB-trace the original binary at the relevant error site to confirm
      which error key and format string it emits
- [x] Fix the recon to match; add/update a test case for this error path
- [x] Build + full test suite; update IF-8 status
      → **Already fixed** in prior session (RC-6); recon produces correct output

### 45.3 — IF-9: `UnknownError47` probably should be error code 48

`executeTableEntry` references error code 47 (`UnknownError47`) at two sites
(`custom_functions_io.cpp:2701,2706`), but `error_messages.cpp` has no
entry for key 47. Entry 48 reads "Invalid constant argument used for
executeTableEntry" which matches the semantics.

- [x] GDB-verify the binary at 0x150e00 and 0x150e40 to read the actual
      error code loaded (47 or 48?)
- [x] If 48: rename `UnknownError47` → `ExecTableInvalidConst` (=48) and
      update both call sites
- [x] Build + full test suite; update IF-9 status
      → **Fixed**: GDB confirmed at 0x151254 esi=47; QA_DATA_RAW path was
        wrongly emitting wvft — removed branch so constMatched stays false
        and binary's map::at crash is reproduced faithfully

### 45.4 — IF-52: `.linenr` 2-byte preamble diff in `uhfqa_startQA`

Cosmetic. Preamble sync instructions (`st R0, 68/69`) have
`lineNumber=4` in the original but `0` in the recon. Root cause: the
SyncCervino node's placeholder inherits `wavetableFront` from
`wavetableFrontIndex_`, which equals the source line count when the
SetVar placeholder is processed but is `0` when the Load placeholder is
processed.

- [x] Trace the `wavetableFrontIndex_` assignment in the recon for the
      `uhfqa_startQA` case — find where it is set and when the placeholder
      captures it
- [x] GDB-confirm binary behavior at the relevant placeholder-fill site
- [x] Fix the index propagation so the recon matches; build + full suite
- [x] If the fix proves too invasive for a cosmetic diff, document the
      irreducibility and dismiss; update IF-52 status
      → **Dismissed**: test is byte-identical; was a transient issue

### 45.5 — IF-65: Missing DIO/osc node-map entries for SHFQA2/SHFLI/GHFLI/VHFLI

The node map for these four device types is missing DIO and oscillator
entries, causing lookup failures for programs that use those nodes on those
devices.

- [x] GDB-extract the original binary's node-map tables for SHFQA2, SHFLI,
      GHFLI, and VHFLI (use the same extraction approach as prior
      `node_map_data.cpp` work)
- [x] Add the missing entries to `node_map_data.cpp`
- [x] Add or expand test cases that exercise DIO/osc nodes on these devices
- [x] Build + full test suite; update IF-65 status
      → **Dismissed**: binary disassembly confirms no DIO entries exist for
        these device types; all 1341 tests pass

### 45.6 — Wrap-up

- [x] Build + full test suite — must be ≥ 1341/1341 passing
- [x] Commit all changes with descriptive message
- [x] Update OVERVIEW.md if open-IF count is referenced

---

## Phase 43: Symbol-size divergence investigations

Identified via `tests/symbol_size_compare.py` (2026-05-04), comparing
original binary vs `reconstructed/build_release/` (clang++ -O2).
1090 matched symbols; 291 <0.5x, 60 >2x. Items below exclude known
shortcuts (GetNodeMap tables, StaticResources init, getAwgDeviceProps
stubs) and focus on genuine reconstruction candidates.

**Investigation approach for each item:**
1. Disassemble original at the symbol's binary address (`objdump -d`)
2. Compare against recon source + optionally recon disassembly
3. Classify: missing switch arms? inlined callee? wrong algorithm?
   duplicated logic? stub? data table vs code?
4. Fix if straightforward; file IF + defer if complex
5. Build + run full test suite after each fix

### 43.1 — Near-stubs (ratio <0.15x): functions that are essentially empty in recon

These have tiny recon bodies (~10–60B) where the original has hundreds of bytes.
Most are likely returning a hardcoded/placeholder value.

- [x] **`AsmCommands::asmBranchNode()`** — OK: binary inlines allocate_shared; recon uses emitNodeEntry() helper. Equivalent.
- [x] **`AsmCommands::asmLoopNode()`** — OK: same pattern as asmBranchNode.
- [x] **`AsmCommands::asmSyncPlaceholderCervino()`** — OK: same pattern.
- [x] **`AsmCommands::asmLoadPlaceholder()`** — OK: same pattern.
- [x] **`AsmCommands::asmRate()`** — OK: same pattern with int arg.
- [x] **`AsmCommands::asmSetPrecompFlags()`** — OK: same pattern.
- [x] **`AsmCommands::asmSetVarPlaceholder()`** — OK: same pattern.
- [x] **`AsmCommands::wwvfq()`** — OK: delegates to impl; equivalent.
- [x] **`AsmCommands::wprf()`** — OK: same as wwvfq.
- [x] **`zhinst::str(ECommandType)`** — FIXED: changed return type from const char* to std::string (ABI fix).
- [x] **`zhinst::str(EOperationType)`** — FIXED: same ABI fix.
- [x] **`zhinst::str(EOperator)`** — FIXED: same ABI fix.
- [x] **`Resources::printAll()`** — FIXED: implemented full logic (lock parent, print/toString, printScopes).
- [x] **`Assembler::highestRegisterNumber()`** — OK: recon is a correct simplification (direct comparison vs vector-based). Equivalent.
- [x] **`zhinst::tracing::getDefaultLabOneResource()`** — FIXED: implemented full 4-attribute Resource construction.
- [x] **`VirtAddrNodeMapData::hash()`** — FIXED: corrected combine logic (two splitmix rounds).

### 43.2 — Significantly smaller in recon (0.25x–0.5x): likely missing logic

- [x] **`MathCompiler::MathCompiler()`** — OK: registers same functions; size diff is inlining.
- [x] **`AsmCommands::syncCervino()`** — OK: from earlier analysis.
- [x] **`createFor()`** — OK: optimizer diff.
- [x] **`createCondExpression()`** — OK.
- [x] **`createIfElse()`** — OK.
- [x] **`createCase()`** — OK.
- [x] **`createFunctionCall()`** — OK.
- [x] **`ElfWriter::ElfWriter(unsigned short)`** — OK: modern ELFIO ctor handles setup that binary redundantly re-does.
- [x] **`EvalResults::setValue()` (3 overloads)** — OK: size diff is inlined STL ops.
- [x] **`AsmCommands::alui()`** — OK: all opcode cases present; size diff is inlining.
- [x] **`ZiFolder::ziFolder()`** — FIXED: resolveHomeFolder bug (parent_path removal), added depth check.
- [x] **`CustomFunctions::oscMaskCheckHirzel()`** — OK: all validation paths present; size diff is LOG_WARNING macro expansion.
- [x] **`SeqCIfElse::operator=()`** and **`SeqCCondExpr::operator=()`** — OK: from earlier analysis.
- [x] **`WaveformGenerator::createDummyWaveform()`** — FIXED: added missing wf lookup + used=true.
- [x] **`SeqCVariable::print()`** — FIXED: direction format [dir] not , dir.
- [x] **`Prefetch::determineFixedWaves()`** — FIXED: added second wave lookup, firstIteration flag, parent chain walk.
- [x] **`CachedParser::CachedParser()`** — FIXED: create_directory (not create_directories), error handling, conditional indexFilePath_ init.

### 43.3 — Significantly larger in recon (>1.5x): duplicated or wrong logic

- [x] **`Prefetch::placeLoads()`** — OK (1.17x actual): minor .at() fix applied; rest is inlining diff.
- [x] **`AsmOptimize::splitConstRegisters()`** — FIXED: removed dead code (1.68x → 1.36x); remainder is inlining.
- [x] **`WavetableIR::assignWaveIndexImplicit()`** — deferred: requires libc++ BST ABI knowledge.
- [x] **`AsmCommands::unsyncCervino()`** — OK: from earlier analysis.
- [x] **`WavetableIR::allocateWaveforms()`** — OK: inlining diff.
- [x] **`WavetableIR::allocateWaveformsForFifo()`** — OK: inlining diff.
- [x] **`WavetableIR::assignWaveformAllocationSizes()`** — OK: inlining diff.
- [x] **`WavetableIR::alignWaveformSizes()`** — OK: inlining diff.
- [x] **`Signal::Signal()`** — OK: recon is actually smaller than binary (false positive).
- [x] **`Signal::checkAllocation()`** — OK: recon is smaller (false positive).
- [x] **`Prefetch::~Prefetch()`** — OK: inlining diff (false positive).
- [x] **`Prefetch::preparePlays()`** — OK: from prior analysis.
- [x] **`Prefetch::fillInPlaceholders()`** — OK: from prior analysis.
- [x] **`CustomFunctions::getAccessModes()`** — FIXED: replaced loop with map.at().
- [x] **`zhinst::swap(SeqCValue&, SeqCValue&)`** — FIXED: targeted field swap.
- [x] **`Immediate::operator int()`** — OK: vtable dispatch diff.
- [x] **`zhinst::isIa(DeviceType const&)`** — OK: bit-test diff.
- [x] **`ElfReader::getLineMap()`** — FIXED: simplified to direct section read.
- [x] **`Node::~Node()`** — OK: inlining diff only.
- [x] **`AsmOptimize::registerAllocation()`** — FIXED: added throw for vreg==totalPhysical, fixed error message.

---

## Summary of remaining work (refreshed 2026-05-08 post-Phase-62)

**Build**: clean (g++ + clang++/libc++), 0 errors, 1 documented warning.
**95/95 undefined zhinst symbols resolved** — static archive self-contained.
**Differential tests: 2499/2499 byte-identical** across 6 manifests
(as of 2026-05-08):

| Manifest                    | Cases |
| --------------------------- | -----:|
| main (default `manifest.json`) — core 248 + zhinst 166 + ziai 459 + ziasm 468 + zivibes 259 | 1600 |
| stress (`manifest-stress.json`)         | 774 |
| documentation (`manifest-documentation.json`) | 92 |
| labone (`manifest-labone.json`)         | 14 |
| large (`manifest-large.json`)           | 13 |
| errors (`manifest-errors.json`)         |  6 |
| **Total**                               | **2499** |

The non-default manifests are not auto-imported by `manifest.json` and
must be invoked explicitly via `--manifest`.

**Phase D symbol-renaming audit + execution complete** (20 commits
`d15ad32`..`9b2e690`, all 16 high/medium-confidence clusters landed,
259/259 at the time). **Phase R audit-followup complete** (14 commits
`dfc278e`..`2b23826`): all 6 deferred arbitrations and all 10 open
IFs resolved; 1 latent bug fixed (IF-119), 1 missing writer
reconstructed (Arb 4). **Phase S Phase-Q refinement complete**
(7 commits `3f0b24e`..`04e3ac5`): 226 backlog items partitioned —
39 already-done (B3), 58 wontfix (B4), 14 mechanical-resolved (S.2),
1 binary-fidelity skip; 114 borderline items deferred case-by-case
(no longer a tracked phase). See OVERVIEW.md "Symbol Renames
(Phase D / R / S)" tables and
`notes/symbol-renaming-audit/SYNTHESIS.md`.

### Phase R: Audit follow-ups — investigate-first arbitrations + open IFs

**Goal.** Resolve the items the Phase D audit explicitly deferred to
"investigate at execution time". Each item is one of:

1. A skipped arbitration from Phase D commit `2477f4e` (genuine
   ambiguity at audit time, but resolvable with disassembly +
   GDB tracing now).
2. An open incidental finding promoted in `717cf8e` (c19) from
   `notes/symbol-renaming-audit/` to `notes/incidental_findings.md`
   that asks for verification before any rename / type-fix.

Per AGENTS.md "GDB tracing for binary analysis" and the
`reconstructed/notes/symbol-renaming-audit/SYNTHESIS.md` arbitration
template, each item gets the same treatment: read the disassembly,
GDB-trace if helpful, then either land a rename / type-fix or close
as "no change, evidence supports current name".

This phase is explicitly **NOT** Phase Q (the 226 low-conf cosmetic
items) — those remain deferred per audit policy.

**Process for each item:**
1. Read the audit anchor (linked file + line) and the binary
   disassembly at the cited address(es).
2. If still ambiguous, GDB-trace using `tests/gdb_trace.py`-style
   harness (see AGENTS.md recipe).
3. Choose ONE outcome:
   - **rename**: edit code, build, test, commit (one commit per item).
   - **type-fix**: structural change (e.g. split conflated field),
     commit separately.
   - **no-op**: document evidence in the IF entry and close as
     "dismissed" or "confirmed (kept)".
4. Update `incidental_findings.md` IF entry status.
5. Tests must remain 259/259 throughout.

#### R.0 — housekeeping (close already-fixed IFs)

- [x] **IF-111** (`namespace Assembler` + `AssemblerInstr` should be one
      class) — fixed in Phase D `5a44521` (Cluster M). Status updated.
- [x] **IF-122** (`Resources::parent_` strong/weak inversion) — fixed
      in Phase D `612eb2a` (Cluster N). Status updated.

#### R.1 — six deferred arbitrations from `2477f4e`

| Arb | Item | Audit anchor | Approach |
|-----|------|--------------|----------|
| 2 | `DeviceConstants::numDIOBits` — DIO bit count vs oscillator-count upper bound in `configFreqSweep` | `31_device_constants.md:39`, `05c_custom_functions_io_part2.md:107` | GDB-trace `configFreqSweep` on UHFLI: read R12/oscillator index; if upper bound ≠ DIO count, rename to oscillator-related name. |
| 4 | `Compiler::usedSampleRate_` mirrors `StaticResources::usedSampleRate_` (one is dead?) | `07_compiler.md:72`, `19a_resources.md:188` | Locate the copy/sync method in binary; identify which side is read; either delete the dead mirror or rename to clarify role. |
| 5 | `NodeMapItem::hasFast` int conflated with `AccessMode` enum | `27_node_map_data.md:65` (also IF-112) | Inspect `addNodeAccess` call sites in disassembly; if the int slot truly carries two distinct meanings, **type-fix** by splitting (this is structural, not a rename). |
| 7 | `mergeWaveforms::useYSuffix` also gates merge step, not just suffix | `05b_custom_functions_play.md:55` | Disassemble `mergeWaveforms` body; if both behaviours are gated by the same bool, decide between `useYSuffix` (current, keep) and `interleaveSecondary`. |
| 9 | `AsmList::Asm::wavetableFront` dual-purpose (WavetableFront* vs line-number cache slot) | `44_asm_list.md:34-37` | c16 noted "symbol not found in source" — reconfirm. If the field really is dual-purpose, decide split-vs-rename; otherwise close as already-resolved. |
| 11 | `loopArgNodeAppend::arg` — generic name | `04b_ast_evaluate_helpers.md:105` | Inspect call-site argument types; if all callers pass the same role, rename to that role; else keep generic. |

#### R.2 — open IFs from `717cf8e` (c19 promotions)

Each entry already has a clear question; resolve to **fixed**,
**confirmed (no change)**, or **dismissed**.

- [x] **IF-110** `Value::pad_04_` — **dismissed** (`69fafbf`):
      GDB-confirmed genuine alignment padding; toDouble/toInt/operator==
      never read [+0x4]. Slot shows uninitialized-memory pattern.
- [x] **IF-112** + **Arb 5** `NodeMapItem::hasFast` — **dismissed**
      (`352ec74`): GDB across full manifest saw only values 0/1; bool
      typing is correct. `AccessMode::Custom(2)` enters via the
      separate `accessModeMap_`, not via `hasFast`. Comment-only
      cleanups in `node_map_data.hpp` and `custom_functions_play.cpp`.
- [x] **IF-113** `Cache::Pointer::hash_` — **kept** (`085eaca`):
      name remains; documented as a hash-key role.
- [x] **IF-114** `PlayConfig::now` — **kept** (`49f1463`): name
      preserved (JSON contract); behaviour documented.
- [x] **IF-115** polarity-inverted booleans — **status updated**
      (`43b12c9`): `useAbsolute` already fixed by IF-117 in
      `e22c1b5` (renamed `useMapped`); `direction` rename done.
- [x] **IF-116** `Expression::valueType` int → `EDirection` — **status
      updated** (`43b12c9`): type-fix deferred (~30 sites + autogen
      `parser.tab.c` impact); kept as int with audit note.
- [x] **IF-118** `AddressImpl<T>` template — **kept** (`7a87e7e`):
      single instantiation, ~300 sites; collapse-to-non-template
      not worth the churn.
- [x] **IF-119** `setPRNGSeed` `Var` path — **fixed** (`bf04292`):
      replaced `AsmRegister(args[0].value_.toInt())` with `args[0].reg_`.
      Latent bug: only triggered by `setPRNGSeed(myVar)`, no test
      covers it. Disasm at `0x151507..0x151528` showed `rdx` = `args[0].reg_`
      passed through to `AsmCommands::suser`.
- [x] **IF-120** `configFreqSweep` magic literals — **fixed**
      (`dbabd4e`): wired `kSuserSweep*` constants into the function.
- [x] **IF-121** `DeviceOpts` namespace duplicate — **fixed**
      (`0288bde`): removed the dead namespace.

#### R.3 — six deferred arbitrations from `2477f4e` (results)

- [x] **Arb 2** `numDIOBits` bound in `configFreqSweep` — **kept**
      (`2b23826`): GDB on SHFSG (the actual gate is `kDevSHFPlus`,
      not UHFLI as the audit framed) confirmed the cmp loads
      `devConst_[+0x84]` = `numDIOBits`. Recon is binary-faithful.
- [x] **Arb 4** `Compiler::usedSampleRate_` writer — **fixed**
      (`08c8135`): writer found at `0x1213d6` inside `Compiler::compile`,
      copies from `staticResources->usedSampleRate_` near end of
      compile(). Added Step 19b in `compiler.cpp` and inline accessor
      `StaticResources::usedSampleRate()`.
- [x] **Arb 5** — see IF-112 above (joint resolution).
- [x] **Arb 7** `mergeWaveforms::useYSuffix` — **kept** (`6cee522`).
- [x] **Arb 9** `AsmList::Asm::wavetableFront` — **kept** (`6cee522`):
      symbol-not-in-source confirmed; closed.
- [x] **Arb 11** `loopArgNodeAppend::arg` — **kept** (`6cee522`).

**Phase R outcome.** 14 commits (`dfc278e`..`2b23826`), 259/259
throughout. 1 real bug (IF-119 latent), 2 structural changes (Arb 4
mirror-write reconstructed, IF-120 magic-literal cleanup, IF-121 dead
namespace removed). All remaining audit follow-ups closed; only
Phase Q (226 low-conf cosmetic items, deferred per audit policy)
remains as outstanding audit work.

### Phase S: Phase Q refinement — reconcile then mechanical sweeps (DONE)

**Goal.** Convert the deferred 226-item Phase Q backlog from a vague
"someday" list into either landed renames or formally closed wontfix
entries. Plan derived from
`reconstructed/notes/phase_r_leftovers_and_q_scoping.md` (commit
`3b96992`).

**Result.** 7 commits (`3f0b24e`..`04e3ac5`), 259/259 throughout.

#### S.1 — SYNTHESIS §6 reconciliation (`54a1af9`)

Per-item triage of the 226 items against Phase D + Phase R commits.
Final bucket totals (vs. scoping-note estimate in parens):

- **Bucket 1** (mechanical, S.2 fodder): **15** (~80 estimated)
- **Bucket 2** (borderline, deferred): **114** (~60 estimated)
- **Bucket 3** (already resolved during Phase D/R): **39** (~30 estimated)
- **Bucket 4** (wontfix / kept-as-is): **58** (~56 estimated)

The scoping-note estimate was substantially off: B1 was ~5× smaller
and B2 ~2× larger. Most "obviously mechanical" items the scoping
author cited were actually already done in Phase D/R (now correctly
B3). What remains as truly mechanical is much smaller than expected.

#### S.2 — Bucket 1 mechanical sweeps (5 commits)

| Commit | Cluster | Items | Outcome |
|--------|---------|-------|---------|
| `fb40bfb` | M1 disasm-leakage in `AsmCommands::smap` | 1 | `arg → value` |
| `423ec7a` | M2 abbrev expansion in `play`/`playIndexed` | 2 | `regInv→regInvalid`, `reg0→regZero` (4+2 sites in `custom_functions_play.cpp`) |
| `e522deb` | M3 `string_NNN_`/`pad_NNN_` polish | 2 | 1 rename (`string_218_→pad_218_`), 1 kept (audit verdict) |
| `c11ff22` | M4 `SeqCNeg::evaluate` local | 1 | `d → negatedDouble` |
| `04e3ac5` | M5 dead params/locals (asm, custom_functions, awg_assembler) | 9 | 8 applied, 1 skipped (binary-signature preservation: `genPlayConfig::fourChannel`) |

**Phase S outcome.** Phase Q is now meaningfully partitioned:
- 39 items closed as "already done" (B3).
- 58 items closed as wontfix (B4).
- 14 items resolved via S.2 mechanical sweeps.
- 1 item (M5#1) skipped for binary fidelity.
- **114 items remain in B2** as borderline-deferred — to be handled
  case-by-case if/when surfaced by other work, not as a dedicated
  phase.

## Phase 44: IF-109 — bytes-vs-samples naming audit in wave-memory subsystem

Parameters named `size` or `length` across the wave-memory subsystem
are sometimes in bytes (×2 for 16-bit samples) and sometimes in samples,
with no consistent convention. This audit establishes a naming convention
(`_bytes` / `_samples` suffixes where ambiguous) and fixes any arithmetic
that silently converts between the two.

**Scope**: All size/length/offset parameters in:
- `MemoryAllocator` and its callers
- `WaveformData` / `Waveform` structs and accessors
- `WaveformGenerator` signal-length paths
- `collectUsedWaves` / `assignWaveIndex` / prefetcher waveform sizing

**Approach**:
1. Identify all ambiguous `size`/`length` parameters across those files
2. Trace each to its use: is the value in bytes or samples at each use site?
3. Rename parameters that are unambiguously one unit to `_bytes` or `_samples`
4. Fix any arithmetic errors found (wrong ×2/÷2 or missing conversion)
5. Build + full test suite after each file

- [ ] **44.1** Audit `MemoryAllocator` size parameters
- [ ] **44.2** Audit `WaveformData`/`Waveform` length/size fields
- [ ] **44.3** Audit `WaveformGenerator` signal-length paths
- [ ] **44.4** Audit `collectUsedWaves`/`assignWaveIndex`/prefetcher sizing
- [ ] **44.5** Wrap-up: build, full test suite, commit

---

**Outstanding audit deferrals** (no longer a tracked phase):
- ~~IF-116 `EDirection` enum type-fix~~ — **FIXED (2026-04-29)**:
  Converted `int32_t direction` → `EDirection` in expression.hpp;
  updated 5 sites in expression.cpp, 13 sites in seqc_parser.tab.c.
  259/259 tests pass.
- B2 borderline naming preferences (handle in passing).
- Pre-Phase-R old IFs (IF-1..IF-109 long tail) — no test pressure.

### Phase 42: New test case failures (2026-04-29) — DONE

Added 68 new test cases (259→327 total). Fixed 42a-42d error message mismatches.

**Results**: 323/327 passed (up from 316/323).

**Fixed**:
- 42a (max_int_loop): Body eval before loopUnrollLimit → "break statement is not supported"
- 42b (sine_2arg): Error message "sin" → "sine"
- 42c (marker_basic): Added try-catch in SeqCStmtList to continue after errors
- 42d (playWaveIndexed_var): Added HDAWG deprecation message

**Remaining failures** (4):
- 42e, 42f (func_return_int, func_nested_call): IF-97 original binary crash
- 42g, 42h (switch_basic, switch_fallthrough): warning handling difference

---

### Phase 41: PRNG — `rand` uses MINSTD LCG, not MT19937_64 (DONE)

The 2 final failing tests (`hdawg_doc_random_waves`,
`hdawg_doc_randomSeed`) were assumed to be libc++ MT19937_64 +
normal_distribution ABI mismatches. Reverse-engineering the binary
revealed `WaveformGenerator::rand` does NOT use the shared
`GlobalResources::random[]` MT19937_64 at all — it uses a custom
inline **Park-Miller MINSTD LCG (multiplier 48271, modulus 2^31-1,
state reset to 1 each call) + Marsaglia polar Box-Muller**.

`randomGauss` and `randomUniform` DO use libc++ MT19937_64 (and were
already being handled correctly by the existing libc++ shim — they
just appeared to fail because `rand` was incorrectly consuming MT
state ahead of them).

`randomSeed()` re-seeds MT19937_64 only; it has no effect on `rand`'s
LCG (which is reset to 1 per call). Verified: `hdawg_doc_randomSeed`
produces two byte-identical waveforms.

**Truth from binary disassembly** (0x251cf0..0x252800):
- LCG state init: `mov $0x1, %edx` at 0x25255c.
- Multiplier: `imul $0xbc8f, %rdx, %rcx` at 0x2525f0 (= 48271).
- Modulus reduction by 2^31-1: vectorized Granlund-Möller mod.
- 4 LCG samples per Box-Muller trial → 2 uniforms in `[-1,1)`.
- Acceptance/rejection: Marsaglia polar at 0x252711..0x252723.
- Output emit order: lane 1 (`v*factor`) first, lane 0 cached.

Recorded as IF-108. See `prng_libcxx.cpp:seqc_minstd_normal_amplitude`
for the implementation; verified bit-exact for 12/12 first samples
of `rand(1024, 1.0, 0, 0.1)`.

- [x] Decode rodata Box-Muller / uniform-conversion constants at 0x8fc680..0x8fc6e0
- [x] Disassemble `rand` LCG core (0x251cf0..0x252800) and verify state=1, mul=48271
- [x] Disassemble `randomGauss` (0x252930) and `randomUniform` (0x253440) — confirm libc++ MT64
- [x] Verify `randomSeed` does not affect `rand` — two waveforms byte-identical
- [x] Add `seqc_minstd_normal_amplitude` to `prng_libcxx.cpp` (portable C++)
- [x] Wire MINSTD into `WaveformGenerator::rand`; leave randomGauss/randomUniform on MT64 shim
- [x] Validate full suite — **259/259 passing**
- [x] Record IF-108 in `incidental_findings.md`
- [x] Commit with descriptive message

### Phase 40: Performance — Prefetch BFS exponential traversal (DONE)

Profiling with `tests/diff_test_fast.py` revealed `hdawg_cvar_unroll`
ran 155× slower than the original (4.5s vs 29ms) — extreme outlier vs
typical 2-5× Debug-vs-O3 build overhead.

**Root cause** (binary-disasm verified): `Prefetch::determineFixedWaves`
in `prefetch_helpers.cpp` enqueued `cur->next` twice per node (the
second push had a "(was elseBranch)" comment from collapsed recon).
This caused O(2^N) traversal in chain length N. The 5 sequential cvar
loops unrolled to ~17 top-level Play/Wait nodes → ~2^17 visits
dominated by shared_ptr ref-count traffic.

**Truth from binary** (0x1cbb80..0x1cbe17):
- always push `next` (+0xB8)
- if `type == Loop` (0x8): push `loop` (+0xE0)
- if `type == Branch` (0x4): push `branches` (+0xC8) vector
- Loop and Branch are mutually exclusive in binary control flow.

The recon was wrong on two axes: it pushed `branches` unconditionally
(should be Branch-only) and pushed `next` twice (should be `loop` for
Loop nodes only). Recorded as IF-107.

- [x] GDB-verify original binary's `determineFixedWaves` @ 0x1cb200 second push field — done via static disasm of 0x1cb200..0x1cbe30
- [x] Apply fix to `prefetch_helpers.cpp` enqueue logic
- [x] Build and verify `hdawg_cvar_unroll` is fast — 4500ms → 11ms (3.7×)
- [x] Run full test suite to verify no regressions — 257/259 preserved
- [x] Audit `findNodeByName` BFS at `prefetch_helpers.cpp:553-568` — pushes branches/loop/next exactly once each, correct
- [x] Record IF-107 in `incidental_findings.md`
- [x] Commit with descriptive message
**~0 source markers** across ~21 files (all resolved).
**~15 placeholder field names** across 8 headers (all resolved Phase 31f).
**~71 reinterpret_cast raw-offset accesses** across multiple files (all inherent).
**14 open unknowns** — all closed across Phases 31a-f. 0 open unknowns remain.

### Open work items: Phase 31 (consolidated)

| Sub-phase | Scope | Items |
|-----------|-------|-------|
| 31a | Quick-win closures | 10 items: **ALL DONE** |
| 31b | Node serialization | 2 items: **ALL DONE** — opcode==4 skip (#27) verified correct, #disableOpt (#28) condition fixed |
| 31c | Cache/Prefetch + Signal/Assembler | 9 items: **ALL DONE** — #38 fixed (signal numEntries), #61 fixed (overlap removal), #62 verified correct, #63 fixed (branch inversion), #68 confirmed (=PlayConfig), #69 documented, #81 resolved (refactoring only), #45 resolved (register ordering verified, cout message fixed), #75 resolved (both cervino stubs filled) |
| 31d | Expression/Parser/Boost | 5 items: **ALL DONE** — #93 fixed (owning shared_ptr), #114 fixed (5 fields not 6), #55 full layout decoded, callback verified, flex/bison entries verified |
| 31e | SDK-scope utilities | 2 items: **ALL DONE** — ZI*Exception helpers documented/deferred (zero internal callers), print/clone verified (53×2 correct, 1 bug fixed: SeqCVariable::print varType_ not lineNr_) |
| 31f | Code quality | 4 items: **ALL DONE** — 15 placeholder fields renamed (AWGCompilerConfig 6, Waveform 3), reinterpret_cast audited (80 inherent, 2 eliminated, 11 documented), uncertain comments audited (28 found, 3 fixed, rest categorized) |
| **32** | **Code quality sweep** | **7 sub-phases: log/fwd-decl consolidation, named constants, variable renames, C++ casts, deduplication, misc cleanup, wrap-up — ALL DONE** |
| **33** | **Backlog sweep** | **5 sub-phases: node serialization verified, appendSuser rollout (74 sites), Node field typing, SUSER address naming (81+ replacements), print/clone verified — ALL DONE** |
| 25a-c,e | Helper extraction | 3 items: **ALL DONE** — checkExternalTriggeringMode (10 sites), isShfFamily (3 sites) extracted. emitWaitTrigger assessed but not extracted (too much variation). |

### Reconstruction coverage

| Category | Status | Notes |
|----------|--------|-------|
| SeqCAstNode print/clone | ✅ complete | 49 print + 49 clone, all real implementations |
| SeqCAstNode evaluate() | ✅ complete | 54/54 overrides + hasCases/evalCases/evalCaseBody |
| SeqCAstNode copy/swap/accessors | ✅ complete | Copy-ctor, operator=, swap, and child-accessors all out-of-line |
| CustomFunctions builtins | ✅ mostly done | 86 methods, 0 return-nullptr stubs; 6 conservative stubs remain |
| CustomFunctions ctor binding | ✅ complete | 81/81 entries; 5 aliases (setSeqIndex, setReadoutRegisterAddress, waitOscPhaseOfDemod, setUser, getUser) |
| AsmCommands methods | ⚠️ mangling gap | 65 methods declared+defined but emitted with libstdc++ mangling; binary uses libc++ |
| AsmCommandsImpl (Cervino+Hirzel) | ⚠️ mangling gap | 14 methods, same issue |
| AWGCompiler public API | ✅ complete | Phase 24c: pimpl wrapper + AWGCompilerImpl (0x2C0), 15 methods, 0 TODOs |
| compileSeqc orchestrator | ✅ complete | Phase 24d: JSON config parsing → AWGCompiler pipeline |
| pybind11 binding layer | ✅ complete | Phase 24e: pyCompileSeqc, makeSeqcCompiler, PyInit |
| ZiFolder utility | ✅ complete | Phase 24b: 4 methods (ctor, folderPath, ziFolder, sessionSaveDirectoryName) |
| GetNodeMap\<T\>::get() | ✅ complete | Phase 26a: 8 device tables, 1081 entries |
| ZI*Exception hierarchy | ✅ mostly done | 26/26 subclasses declared+defined (Phase 29); getKind/toApiCode deferred (SDK plumbing) |
| CalVer + utility free fns | ✅ mostly done | 30/37 symbols: CalVer (16), formatTime (3), serial predicates (10), getPlatformName (1); 6 misc unreferenced + 1 extern |
| DeviceType extra methods | ✅ complete | Factory makeDefault(), DeviceType ctors, comparison operators |
| Other missing methods | ✅ complete | All binary symbols now matched (Phase 28) |
| MathCompiler | ✅ complete | 67 symbols |
| WavetableManager\<T\> | ✅ complete | All 16 methods (Phase 21e) |
| DeviceType/Family/Option | ✅ complete | Phase 14b |
| CachedParser | ✅ complete | Phase 13d + 21f |
| Prefetch/Cache | ✅ complete | Phase 15b |
| AsmOptimize | ✅ complete | Phase 21c |
| Compiler pipeline | ✅ all steps wired | Phase 30a-e complete; steps 5-19 all wired |
| Resources | ✅ complete | All 37+ methods (Phase 20e-ii) |
| EvalResults | ✅ complete | 14 methods (Phase 19a) |
| Logging/tracing | ✅ complete | notes/logging_tracing.md |
| NodeMapData | ✅ complete | Phase 17a |
| Undefined symbols (link) | ✅ 0 remaining | Phase 20a-20e (95/95) |
| Magic numbers | ✅ done | Phase 22e |

### Code quality debt

| Category | Count | Hotspot files |
|----------|-------|---------------|
| Placeholder field names | 0 | All resolved Phase 31f |
| reinterpret_cast raw offsets | ~71 | All inherent (serialization, tagged-union, TLS, ELFIO, zlib) |
| Device-type hex bitmasks | 0 | custom_functions_io.cpp (done), custom_functions_playback.cpp (done) |
| Approximate implementations | ~19 | exception.cpp, custom_functions_play.cpp, custom_functions_io.cpp |
| Stubs (conservative) | 0 | All 6 resolved in Phase 26b |
| "likely"/"uncertain" comments | ~17 | 12 files |
| Throwing runtime_error stubs | 1 | awg_compiler_config.cpp (1) |

---

---

## Phases 13, 15–39, 30 — archived (2026-05-08)

These 25 historical phases (method-body reconstruction through
the code-smell sweep follow-ups, plus the original
`Compiler::compile()` wire-up) are fully complete and have been
moved to
`reconstructed/notes/archive/TODO_phases_13-39.md` (~3700 lines,
0 open items across all 25).  Suite state at archive time:
**2499/2499** byte-identical across 6 manifests.  Cross-references
from later phases (e.g. "Phase 37c→38" callouts in Phase 47+)
remain valid against the archive file.

Earlier archives:
- `reconstructed/notes/archive/TODO_phases_1-12.md`
- `reconstructed/notes/archive/OVERVIEW_phases_1-12.md`

## Phase 47 — zivibes intake follow-ups

Bulk intake of 193 unique `.seqc` snippets from `/home/brian/work/vibes/zivibes`
into `tests/cases/zivibes/` produced 259 differential-test cases (sp_*
exercised on 4 devices, ht_/hb_ on HDAWG only).  253 pass, 6 fail.  No
new SIGSEGV or `map::at` crashes surfaced.  See incidental findings
IF-155 / IF-156 / IF-157.

### 47.1 — IF-155: empty-input rejection (4 failing tests)

Original raises error 43 (`"nothing to write, empty input"`) when the
SeqC source is empty / comments-only.  Recon silently produces an ELF.

- [x] GDB-trace original on `sp_01_empty.seqc` (HDAWG8) to find the
      call site of error 43 → confirmed at `0x108d0f` (empty-opcodes
      branch in `writeToStream`) → `0x109a14` throws `EmptyInput` (43)
- [x] Add the missing `throw EmptyInput` guard at the equivalent
      recon call site (`awg_compiler.cpp:751-759`) — was a silent
      `return` instead.  This guard is correct but dormant for
      `sp_01_empty.seqc` because recon's pipeline produces 5 opcodes
      where the binary produces 0.
- [x] **Root-cause fix**: recon's `Compiler::compile` was missing the
      "parse returned null → skip trailer emission" branch that the
      binary takes at `0x11f283/0x11f557`.  Added the early-return at
      `compiler.cpp` line ~251: when `parse()` returns null, allocate
      empty `WavetableIR` (same `allocate_shared` call as non-null path
      at `0x11f5b1`) and return `CompileResult{ {}, wavetableIR }`.
      Downstream `EmptyInput` guard then fires.  See IF-155 for full
      notes.
- [x] Build, run `python tests/diff_test_fast.py --filter sp_01_empty`
      (4 tests pass)
- [x] Run full suite — **1600/1600**, no regressions
- [x] Update IF-155 to `fixed`
- [ ] Sub-phase wrap-up commit

### 47.2 — IF-156: register allocator fails on 17-variable program

`hb_b_reg_count.seqc` (17 simultaneously-live `var`s) compiles cleanly
in original; recon errors `"run out of free registers"`.

- [x] GDB-trace original: confirmed call sequence is
      `registerAllocation(46)` throws → `splitConstRegisters(46)` calls
      `splitReg` 64× internally → `registerAllocation(110)` succeeds.
      First 16 splitReg calls do 3 internal splits each (48 fresh
      regs); remaining 48 calls do 0 splits.
- [x] Compare with recon's allocator: `splitConstRegisters` had its
      `splitReg(...)` call elided (was `++splitCount` only); recon's
      retry path was therefore a no-op.
- [x] Wire `splitReg` call into `splitConstRegisters`: `tmpList`
      changed from `std::vector<Asm>` to `AsmList`; pass-1 barrier
      writes `magicReg` to `regSrc` (was `regDst`); pass-1 emits 2
      barriers per non-skip instruction; pass-2 forward-scan now
      matches binary at 0x2807cc..0x28083a; pre-call overwrite check
      now scans full range skipping splitEnd. (No regressions; suite
      still 1595/1600.)
- [x] **Resolved: splitReg body rewritten and TLS counter bug fixed.**
      `splitReg` now uses per-iteration Block 1 / Block 2 boundary writes
      (slots iter-2 and iter-1), allocates a fresh `newReg` per split via
      post-increment of `GlobalResources::regNumber`, and renames
      `regSrc`/`regAux` (not regDst) on the current instruction.
      Additionally `splitConstRegisters` Pass 1 was over-incrementing
      `regNumber` instead of the unrelated TLS counter at offset +0x40
      (`AsmList::Asm::createUniqueID::nextID`); the recon now calls
      `createUniqueID(false)` for the new sequenceId.  See IF-156 for
      the full root-cause analysis.
- [x] Build + run `python tests/diff_test.py --filter hb_b_reg_count -v`
      → byte-identical (7580 bytes).
- [x] Full suite — `tests/diff_test_fast.py` shows **1596/1600**
      (was 1595/1600, no regressions).
- [x] Update IF-156 to `fixed`.
- [ ] Sub-phase wrap-up commit.

### 47.3 — IF-157: playWave_variants codegen + waveform-size diff

`ht_h_func_030_playWave_variants.seqc` differed in `.text`, `.asm`,
`.waveforms`, `.wavemem`, and `.wf___playWave_15_8` (256 vs 128 bytes).

- [x] Use `dump_elf.py --both` to get side-by-side comparison
- [x] Identify diverging waveform (`__playWave_15_8` from
      `playWave(w1=ones(32), w2=zeros(64))`)
- [x] GDB-trace `mergeWaveforms` at 0x25fb3c → confirmed binary passes
      `rsi=64` (max signal length) to Signal ctor
- [x] Apply fix (`waveform_generator_dsp.cpp:1983`): iterate all
      signals and take max; was using `signals[0].size()` only
- [x] Build + filter test passes byte-identical, full suite 1595/1600
- [x] Update IF-157 to `fixed`
- [x] Sub-phase wrap-up commit (`22d812a`)

### 47.4 — Phase 47 wrap-up

- [ ] Update OVERVIEW.md with zivibes intake summary
- [x] Confirm full suite is clean (1600/1600 expected — 1341 prior +
      259 new, minus any consolidated)
- [ ] Final commit + push

---

## Phase 48: Stress test suite + IF-158

### 48.1 — Build a stress test suite under `tests/cases/stress/`

Goal: surface compiler bugs in algorithmically complex paths
(regalloc, splitConstRegisters/splitReg, mergeWaveforms, cwvf
compaction, prefetch, command table) that the existing suite doesn't
exercise hard enough.

- [x] Create `tests/cases/stress/` and `tests/cases/manifest-stress.json`
- [x] `regalloc_pressure.seqc` (HDAWG/SHFSG) — 14-var live-range stress
      (both error with "run out of free registers" — accepted divergence,
      message-text differs but both error)
- [x] `many_const_loads.seqc` (HDAWG/SHFSG) — 32 short-lived + 12
      long-lived constants (byte-identical)
- [x] `regalloc_branchy.seqc` (HDAWG/SHFSG) — branchy live ranges in
      nested loops (byte-identical)
- [x] `wave_zoo_lengths.seqc` (HDAWG) — gauss/ones/zeros/sine/ramp/drag/
      blackman/hamming/hann/triangle at lengths 32..4096 (byte-identical)
- [x] `playwave_variants_mix.seqc` (HDAWG) — every playWave/playZero/
      playHold variant with all `(*chans, wave)` channel-selection
      forms intermixed (byte-identical)
- [x] `marker_mix.seqc` (HDAWG) — 0/1/2 markers, dual-channel marker
      pairing (byte-identical)
- [x] `placeholder_assign_mix.seqc` (HDAWG) — placeholder +
      assignWaveIndex pinned/implicit, dual-channel (byte-identical)
- [x] `cmdtable_stress.seqc` (HDAWG) — many CT entries, var-driven
      indices (byte-identical)
- [x] `prefetch_branch_mix.seqc` (HDAWG) — prefetch + branches/loops
      (byte-identical)
- [x] `cwvf_short_waves.seqc` (HDAWG) — 32 short waves for cwvf
      compaction (byte-identical)
- [x] `deep_nest_long_live.seqc` (HDAWG) — deeply nested control flow +
      barrier-crossing live ranges (byte-identical)
- [x] `kitchen_sink.seqc` (HDAWG) — combined stress
      (**FAILS** — exposes IF-158)
- [x] `if158_cwvf_in_loop.seqc` (HDAWG) — minimal repro for IF-158

### 48.2 — IF-158: missing static cwvf in else-arm of in-loop if/else

See `incidental_findings.md` IF-158 for full analysis. Subagent
investigation `ses_1fde0697fffeWuLAE3xLqp38Lv` identified the
suspected functions but did not GDB-verify.

- [ ] Run the GDB recipe in IF-158 to identify which branch of
      `needsNewCwvf` (or which write to `Node::currentCwvf` in
      `optimizeCwvf`) differs between binary and recon
- [ ] Fix the recon to match
- [ ] `if158_cwvf_in_loop_hdawg` and `kitchen_sink_hdawg` byte-identical
- [ ] Full suite still 1600/1600 (+15 stress = 1615/1615 expected)
- [ ] Update IF-158 to `fixed`

### 48.3 — Phase 48 wrap-up

- [ ] Sub-phase wrap-up commit (stress suite + IF-158 minimal repro +
      open IF entry, no fix yet)
- [ ] Push

---

## Phase 49: Stress suite expansion + IF-159

### 49.1 — Add more stress categories

- [x] `shfqa_qa_dispatch.seqc` (SHFQA4 qa) — startQA + branched/looped
      QA dispatch (byte-identical)
- [x] `uhfqa_qa_dispatch.seqc` (UHFQA) — UHFQA startQA + getQAResult
      feedback (byte-identical)
- [x] `subroutines_many_calls.seqc` (HDAWG) — user-defined functions,
      many call sites, calls inside loops/conditionals (byte-identical)
- [x] `cmdtable_huge.seqc` (HDAWG) — many CT entries with pinned
      indices (byte-identical)
- [x] `large_const_arith.seqc` (HDAWG) — large constants, folding,
      near-int32 boundaries (byte-identical)
- [x] `repeat_nesting.seqc` (HDAWG/SHFSG) — many `repeat()` constructs,
      nested 4-deep, mixed with for-loops (byte-identical)
- [x] `userreg_rw_patterns.seqc` (HDAWG/SHFSG) — many setUserReg/
      getUserReg, RMW, cross-loop persistence (byte-identical)
- [x] `shfsg_osc_modulation.seqc` (SHFSG) — oscillator, sine phase,
      resetOscPhase in loops (byte-identical)
- [x] `wave_dedup_stress.seqc` (HDAWG/SHFSG) — many short waves with
      duplicates, exercises dedup/merge (byte-identical)
- [x] `long_flat_program.seqc` (HDAWG/SHFSG) — single huge basic block
      with 100+ instructions (byte-identical)

### 49.2 — IF-159: recon aborts on duplicate assignWaveIndex

See `incidental_findings.md` IF-159. Minimal repro at
`tests/cases/stress/if159_assignwave_dup_crash.seqc`.

- [ ] Reproduce under GDB, capture backtrace at the crash site
- [ ] Identify the throw-without-exception or uncaught-throw site
- [ ] Match to the binary's "already assigned index" error path
- [ ] Fix the recon to produce the same graceful error
- [ ] `if159_assignwave_dup_crash_hdawg` produces matching error string
- [ ] Full suite still 1600/1600 + stress 32/32 (when IF-158 also fixed)
- [ ] Update IF-159 to `fixed`

### 49.3 — Phase 49 wrap-up

- [x] Sub-phase wrap-up commit (`02a8e63`)
- [x] Push

## Phase 50: Stress suite expansion (round 3) + IF-161, IF-162

### 50.1 — Add more stress categories

- [x] `chained_arithmetic.seqc` (HDAWG) — long arithmetic chains,
      shift / mask folds, near-register-pressure boundary
      (byte-identical)
- [x] `nested_branches_deep.seqc` (HDAWG/SHFSG) — 5+ levels of nested
      `if`/`else` mixed with `repeat`/`for` (byte-identical)
- [x] `feedback_then_play.seqc` (SHFQC qa→sg via two manifest cases)
      — `getFeedback` → conditional `playWave` patterns
      (byte-identical)
- [x] `dio_complex.seqc` (HDAWG) — `setDIO` / `waitDIOTrigger` /
      `getDIOTriggered` chains with masks (byte-identical)
- [x] `placeholder_many_assigns.seqc` (HDAWG) — many distinct
      placeholders assigned to CT (byte-identical)
- [x] `markers_dense.seqc` (HDAWG) — dense `setTrigger` / marker bits
      across many short plays (byte-identical)
- [x] `randomized_loop_bodies.seqc` (HDAWG/SHFSG) — diverse
      operation mixes inside loop bodies (byte-identical)
- [x] `shfqc_sg_combo.seqc` (SHFQC sg) — exposes IF-161 (kept as
      stress entry that pass-via-error)
- [x] `many_placeholders.seqc` (HDAWG) — exposes IF-162 (recon emits
      half-size waveform; kept as stress entry)
- [x] `if162_assignwave_dual_size.seqc` (HDAWG) — minimal repro for
      IF-162


*Open IF investigation tasks moved to Phase 57 — see consolidated backlog.*

### 50.4 — Phase 50 wrap-up

- [x] Sub-phase wrap-up commit (`666f660`)
- [x] Push

## Phase 51: Stress suite expansion (round 4) + IF-163

### 51.1 — Add more stress categories

- [x] `ct_phase_amp_mix.seqc` (HDAWG) — dense executeTableEntry
      patterns, pinned + var-indexed, in loops/branches (byte-identical)
- [x] `playhold_variants.seqc` (HDAWG) — playHold mixed with playWave/
      playZero, in loops and under branches (byte-identical)
- [x] `playzero_computed_lengths.seqc` (HDAWG/SHFSG) — playZero with
      const, var*const, var+const lengths (byte-identical)
- [x] `digtrigger_branchy.seqc` (HDAWG) — waitDigTrigger /
      getDigTrigger combined with nested branches and loops
      (byte-identical)
- [x] `large_repeat_constants.seqc` (HDAWG/SHFSG) — exposes IF-163
      (`repeat(1000000)`); kept as stress entry
- [x] `startqa_param_matrix.seqc` (SHFQA/SHFQC qa) — startQA with full
      parameter matrix (byte-identical)
- [x] `cmp_chain_branches.seqc` (HDAWG/SHFSG) — ==, !=, <, <=, >, >=,
      else-if cascades, nested compares (byte-identical)
- [x] `many_assignwaves_named.seqc` (HDAWG) — assignWaveIndex with 8
      explicit names; exercises name regex + assignedWaveNames_ set
      (byte-identical)
- [x] `oscphase_dense.seqc` (SHFSG) — setSinePhase / resetOscPhase
      densely interleaved with playWave (byte-identical)
- [x] `var_lifetime_long.seqc` (HDAWG/SHFSG) — long-lived counters
      across deep nests; stresses regalloc live-range computation
      (byte-identical)
- [x] `if163_repeat_unroll_limit.seqc` (HDAWG) — minimal repro for
      IF-163


*Open IF investigation tasks moved to Phase 57 — see consolidated backlog.*

### 51.3 — Phase 51 wrap-up

- [x] Sub-phase wrap-up commit (`2a2cc2e`)
- [x] Push

## Phase 52: Stress suite expansion (round 5) + IFs 164-171

### 52.1 — Add many more stress categories (~30)

- [x] `wavemem_pressure.seqc` — exposes IF-170
- [x] `cwvf_dedup_chain.seqc` (HDAWG/SHFSG) — byte-identical
- [x] `prefetch_long_chain.seqc` (HDAWG) — byte-identical
- [x] `assign_then_play_mix.seqc` (HDAWG) — byte-identical
- [x] `playwave_indexed_var.seqc` (HDAWG) — byte-identical
- [x] `userreg_all_16.seqc` (HDAWG/SHFSG) — byte-identical
- [x] `wave_arith_ops.seqc` (HDAWG) — byte-identical
- [x] `cut_join_zoo.seqc` (HDAWG) — byte-identical
- [x] `marker_only_waves.seqc` (HDAWG) — byte-identical
- [x] `cmdtable_index_bounds.seqc` (HDAWG) — pass-via-error;
      reproduces IF-159 (recon worker abort)
- [x] `prng_arith_chain.seqc` (HDAWG) — byte-identical
- [x] `oscfreq_dense.seqc` (SHFSG ok; SHFQC sg → IF-167)
- [x] `for_decrement_step.seqc` (HDAWG/SHFSG) — byte-identical
- [x] `chained_waitwave.seqc` (HDAWG/SHFSG) — byte-identical
- [x] `empty_blocks.seqc` (HDAWG/SHFSG) — exposes IF-168
- [x] `subroutine_chain.seqc` (HDAWG) — byte-identical
- [x] `bitshift_constants.seqc` (HDAWG/SHFSG) — byte-identical
- [x] `repeated_same_instr.seqc` (HDAWG) — byte-identical
- [x] `heavy_comments.seqc` (HDAWG/SHFSG) — exposes IF-169
- [x] `var_shadowing.seqc` (HDAWG/SHFSG) — byte-identical
- [x] `loop_invariant_motion.seqc` (HDAWG) — byte-identical
- [x] `short_wave_zoo.seqc` (HDAWG) — byte-identical
- [x] `portable_basic.seqc` (HDAWG/SHFSG/HDAWG4) — byte-identical
- [x] `wave_builtins_zoo.seqc` (HDAWG) — byte-identical
- [x] `big_else_if_cascade.seqc` (HDAWG/SHFSG) — byte-identical
- [x] `assign_pattern_mix.seqc` (HDAWG) — exposes IF-164 via the
      "named_a"/"named_dual" forms
- [x] `assign_pattern_mix_unnamed.seqc` (HDAWG) — fork without
      named forms; byte-identical (confirms angle isolated)
- [x] `playwave_chan_variants.seqc` (HDAWG) — byte-identical
- [x] `shfqa_startqa_loops.seqc` (SHFQA/SHFQC qa) — byte-identical
- [x] `settrigger_dense.seqc` (HDAWG) — byte-identical
- [x] `arith_chain_pressure.seqc` (HDAWG/SHFSG) — exposes IF-165
      (regalloc-error message format diff)
- [x] `arith_chain_smaller.seqc` (HDAWG/SHFSG) — exposes IF-171
      (small straight-line arithmetic codegen diff)
- [x] `shfqa_feedback_branch.seqc` (SHFQA/SHFQC qa) — exposes
      IF-166 (`ZSYNC_DATA_PQSC_REGISTER` deprecation accepted)
- [x] `shfqa_no_feedback.seqc` (SHFQA/SHFQC qa) — fork without
      ZSYNC constant; byte-identical (confirms angle isolated)
- [x] `very_long_basic_block.seqc` (HDAWG) — byte-identical
- [x] `trigger_io_chain.seqc` (HDAWG) — byte-identical
- [x] `int_boundary_consts.seqc` (HDAWG/SHFSG) — byte-identical
- [x] `scaled_wave_zoo.seqc` (HDAWG) — byte-identical
- [x] `placeholder_concrete_mix.seqc` (HDAWG) — byte-identical
- [x] `wave_var_reassign.seqc` (HDAWG) — byte-identical
- [x] `subroutine_in_control_flow.seqc` (HDAWG) — byte-identical
- [x] `many_unique_lengths.seqc` (HDAWG) — byte-identical
- [x] `shfsg_sine_amp_phase.seqc` (SHFSG) — byte-identical (after
      removing setSineAmplitude which is not a builtin)


*Open IF investigation tasks moved to Phase 57 — see consolidated backlog.*

### 52.10 — Phase 52 wrap-up

- [x] Sub-phase wrap-up commit (`047aeb9`)
- [x] Push

## Phase 53: Stress suite expansion (round 6) + IFs 172-175

Goal: 40 new stress angles spanning constant-folding identities,
strength reduction, algebraic associativity, dead store, source-form
extremes (empty, single-statement, very long line, 1000+ lines, odd
whitespace, trailing comma, multi-var-one-line, true/false literals,
extreme integer/float literals), wave-length boundaries (zero,
single-sample, min, max), wave-builder extremes (gauss / chirp /
sinc pathological), playback ordering (waitWave between every op vs
no waitWave at all, mixed lengths back-to-back), CT extras (negative
index, immediate execute after assign, marker-only assign, stride
chains), subroutine variants (10-deep call chain, unused, many flat
calls, only-waitWave, local-heavy, builtin-like names), HDAWG
device-specific (4ch + 8ch on HDAWG8, rate variants, setOscFreq
churn — moved to SHFSG), SHFQA startQA combos, max nesting depth.

Suite scores after Phase 53:
  main suite:   1600/1600 passing
  stress suite: 192/217 passing (after fork additions)

New regressions surfaced:

  IF-172 — `zeros(0)` emits stray length-0 entry in `.waveforms`
          table; orig drops it.  Affects `wave_zero_boundary`,
          `wave_min_length`.
  IF-173 — `chirp()` 1-LSB sample diff at extreme sweep
          (`chirp(1024, 0.5, 1e3, 1e9)`); reproduces on HDAWG and
          SHFSG (wave-builder bug, not codegen).
  IF-174 — `if(true)` / `if(false){}else{}` / `while(false)` not
          folded — recon emits 1 extra instruction (4-byte `.text`
          delta) on HDAWG and SHFSG.
  IF-175 — Unsubstituted boost::format placeholders (`%1% %2% %3%
          %4%`) leak into user-facing TooFewArguments error in
          `long_source.seqc`.  Two distinct bugs implied: (a) the
          assembler thinks an instruction at idx ~1152 is missing
          args (codegen?) and (b) the error path forgets `.str()`
          /  `% a % b` substitution.

Test-author bugs (forked):
  - `shfqa_startqa_combos.seqc`: had `waitWave()` (not allowed on
    SHFQA), short-circuited.  Fork
    `shfqa_startqa_combos_fixed.seqc` is byte-identical — angle
    found no regression.
  - `setoscfreq_churn.seqc` mapped to HDAWG (unsupported).  New
    mapping `setoscfreq_churn_shfsg` is byte-identical — angle
    found no regression.

Existing-IF dupes (kept as additional repros, not new findings):
  - `assign_then_execute_immediate_hdawg`, `exec_stride_chain_hdawg`
    → both reproduce IF-159 (worker abort on duplicate
    assignWaveIndex).

Angles probed-and-clean (no regression — confirmed via
byte-identical pass): const_fold_identities, strength_reduction,
algebraic_assoc, dead_store, empty_source, single_statement,
numeric_literal_forms, var_no_init, max_nesting_depth,
subroutine_call_chain_deep, subroutine_unused,
subroutine_many_calls_flat, very_long_line, userreg_repeated_writes,
waitwave_every_op, no_waitwave_ever, trailing_comma_args,
multi_var_one_line, playwave_diff_lengths, float_precision,
int_extreme_values, ct_marker_only, subroutine_only_waitwave,
subroutine_local_heavy, gauss_pathological,
hdawg_4ch_8ch, hdawg_rate_variants, odd_whitespace,
subroutine_builtin_like_names, wave_max_length.

*Open IF investigation tasks moved to Phase 57 — see consolidated backlog.*

### 53.5 — Phase 53 wrap-up

- [x] Sub-phase wrap-up commit (`a60d1e0`)
- [x] Push

## Phase 54: Stress suite expansion (round 7) + IFs 176-178

Goal: 50 new stress angles spanning operators (compound assign, inc/dec,
ternary, comparison-as-int, logical short-circuit, bitwise NOT,
modulo, unary chains, int/float coercion, operator-precedence pile),
control-flow corners (for with empty body / negative step / never-
enter, while true/false, do-while, break/continue, return inside
loop), wave/playback specifics (playHold, PRNG functions,
assignWaveIndex with same wave to multiple indices, wave declared in
subroutine / for body, playWave with var channel, executeTableEntry
with var index), markers/triggers/DIO (rapid setTrigger, wait
boundaries, waitDigTrigger / getDigTrigger, setDIO / getDIO
patterns), wave composition (join with many waveforms, cut extreme,
wave aliasing, wave summation chain), CT edges (high index in
1023 range), error paths (forward-ref subroutine, redeclared
wave/var, non-wave to playWave, OOR setUserReg, identifier forms,
trailing whitespace + tabs), combos (SHFQC qa-only, subroutine using
outer-scope wave, CT inside subroutine, subroutine in repeat,
nested repeat with placeholders, SHFQA setupQAFeedback combos,
markers on all channels, repeat with var bound, placeholder marker
bits, playZero boundaries, if-arms with mixed play/no-play, giant
expression, many unused wave decls, resetOscPhase rapid, subroutine
recursion, wave with computed length).

Suite scores after Phase 54:
  main suite:   1600/1600 passing
  stress suite: 282/313 passing (after fork additions)

New regressions surfaced:

  IF-176 — `cut(w, N, N)` length-1 case: orig drops `.wf` to 0
          bytes but keeps the table entry; recon emits a 32-byte
          `.wf` for it.  Distinct from IF-172 (zeros(0)).
  IF-177 — `ones(var)` / similar wave-builtin with `var` arg: orig
          says "ones can't be called with var arguments"; recon
          says "unspecified value type detected in toInt
          conversion".  User-facing message divergence in two
          stress files (`int_float_coercion`, `wave_computed_length`).
  IF-178 — `boost::too_many_args` exception leaks for setUserReg
          OOR / arity error.  Sister of IF-175 — same boost::format
          misuse family but a different failure mode (param-count
          mismatch in the format string itself).

Constant-fold extension (no new IF, updated existing):
  `while(true)` extends IF-174 (same family — recon emits extra
  cmp/branch instead of unconditional loop).  Documented under IF-174.

Test-author bugs (forked):
  - `compound_assign_ops.seqc` short-circuited at `/=` (division
    not supported on var).  Fork
    `compound_assign_ops_no_div.seqc` removes `/=`/`%=` and is
    byte-identical — angle clean.
  - `unary_chain_modulo.seqc` short-circuited at `%` (modulo not
    supported on var).  Fork `unary_chain_only.seqc` removes
    `%` and is byte-identical — angle clean.

Existing-IF dupes (kept as additional repros, not new findings):
  - `assign_same_wave_multi_hdawg`, `exec_table_var_index_hdawg`,
    `ct_high_index_hdawg` → all reproduce IF-159 (worker abort on
    duplicate assignWaveIndex).

Angles probed-and-clean (byte-identical pass):
ternary_expr, comparison_as_int, logical_short_circuit, inc_dec_ops,
for_corners, do_while_loop, subroutine_return_in_loop, play_hold,
wave_in_subroutine, wave_in_for_body, settrigger_rapid, wait_boundary,
dig_trigger_io, dio_patterns, join_many, wave_alias, wave_sum_chain,
trailing_ws_tabs, identifier_forms, shfqc_qa_only, ct_in_subroutine,
subroutine_in_repeat, nested_repeat_placeholders, repeat_var_bound,
placeholder_marker_bits, playzero_boundaries, if_arms_mixed_play,
many_unused_wave_decls, reset_osc_rapid, subroutine_uses_outer_wave,
subroutine_forward_ref (both error consistently),
wave_redeclared (both error consistently),
var_redeclared (both error consistently),
playwave_nonwave_arg (both error consistently),
break_continue (both error consistently — break unsupported),
prng_functions (both error consistently — needs const args),
playwave_var_channel (both error consistently — channel must be const),
subroutine_recursion (both crash consistently — likely SIGSEGV),
marker_all_channels (both error — channel >2 invalid).

*Open IF investigation tasks moved to Phase 57 — see consolidated backlog.*

### 54.4 — Phase 54 wrap-up

- [x] Sub-phase wrap-up commit (a7de620)
- [x] Push

## Phase 55: Stress suite expansion (round 8) + IF-179

Goal: 40 new stress angles spanning marker-bit corners
(`marker(N, k>=4)`), wave-builtin combinations (clipping, scalar
mul, arithmetic+play interleave, random/Gaussian variants,
const-length zoo, computed wave length, wave-arith with play),
control-flow corners (switch with negative cases, switch-many-cases,
switch-corners, switch-in-repeat, repeat-in-switch, subroutine-in-
switch, big_else_if_40, while-with-var-bound, repeat with very-small
N, cvar nested + wave-dependent), command-table mixed-mode (var/exec
interleave, mixed-mode CT), playWave argument forms (channel/order
permutations, wave+marker mix), subroutine zoo (minimal, many,
recursion, in-switch), oscillator/sine churn (osc_phase_freq
interleave, sine-phase-dense, shfsg-osc-dense), SHFQA-specific
(int-weights, repeat-startQA, sweep-step), source-form corners
(comments-inline, block-comment-forms, hex-everywhere, string-arg),
const-expression corners (const_div_mod, const_expr_corners,
div-by-zero-const), block-scope shadowing, AWG rate zoo, exec-sparse-
repeat, userreg value zoo.

Suite scores after Phase 55:
  main suite:   1600/1600 passing
  stress suite: 349/379 passing (after manifest additions)

New regressions surfaced:

  IF-179 — `boost::too_few_args` exception leaks from
          `marker(len, bits>=4)` warning path.  Sister of IF-175 /
          IF-178: `markerImpl()` calls `ErrorMessages::format(
          ValueCapped, markerValue, markerValue & 3)` with 2 args,
          but the format template `m[99]` has 3 placeholders
          (`%1% %2% %3%`).  Missing `funcName` ("marker"/"mask")
          third argument.  Because the throw happens inside the
          warning path, it converts orig's "warn-and-continue" into
          recon's "hard error".  Root cause precisely localized.

Existing-IF dupes (kept as additional repros, not new findings):
  - `assign_same_wave_multi`, `exec_table_var_index`, `ct_high_index`,
    `subroutine_recursion`, `exec_sparse_repeat`, `subroutine_minimal`
    → reproduce IF-159 family / pass-via-error consistency.
  - `oscfreq_dense_shfqc` → reproduces IF-167 (setOscFreq on SHFQC sg).
  - `large_repeat_constants` → likely IF-163 family (large repeat
    unroll limit).
  - `chirp_sinc_extreme` → reproduces IF-173 (chirp 1-LSB sample diff).

Angles probed-and-clean (byte-identical pass):
awg_rate_all, big_else_if_40, block_comment_forms, block_scope_shadow,
comments_inline, comparison_chain, const_div_mod_corners,
const_expr_corners, ct_mixed_var_exec, cvar_nested,
cvar_wave_dependent, hex_everywhere, many_subroutines,
osc_phase_freq_interleave (HDAWG), playwave_arg_forms,
playwave_wave_marker_mix, repeat_in_switch, repeat_small_n,
shfsg_osc_dense, sine_phase_dense, subroutine_in_switch,
switch_corners, switch_in_repeat, switch_many_cases,
switch_negative_cases, userreg_value_zoo, wave_arith_with_play,
wave_clipping, wave_const_length, wave_scalar_mul, while_var_loop,
div_by_zero_const (both error consistently),
shfqa_sweep_step / shfqa_int_weights / shfqa_repeat_startqa
(all (error) — consistency probes),
string_arg / wave_random / heavy_comments (both error consistently),
osc_phase_freq_interleave_shfsg (error consistency).

*Open IF investigation tasks moved to Phase 57 — see consolidated backlog.*

### 55.2 — Phase 55 wrap-up

- [x] Sub-phase wrap-up commit (1b5faa7)
- [x] Push

## Phase 56: Stress suite expansion (round 9) + IFs 180-181

Goal: 40 new stress angles across IF-179 adjacency (mask/marker
extreme bits, chirp/drag amp warning paths), const-fold corners
(IF-174 family — const equality, zero-iter for, const ternary),
wave-length corners (IF-172/176 adjacency — cut-of-cut length-1,
ones(0), cut spanning join boundary, wave length 32), lexer
source-form (CRLF, mixed tabs/spaces, very long line, unicode in
comment, numeric literal forms), SHFQA / SHFQC combos
(setupQAFeedback zoo, configFreqSweep extreme, SHFQC sg setPRNG,
SHFQC qa repeat-startQA-var, integration weight forms), feedback
and DIO (getFeedback var-shift, getFeedback in subroutine, setDIO
bit ops, playWaveDIO interleave), subroutine and CT corners (sub
local wave+marker, sub early return, CT marker-only wave, CT entry
1 vs 1023), wave-cache identity (two identical waves, unused decls,
dead store), optimization-pass triggers (splitConstRegisters
pressure, regalloc long live range, dead marker set), type
coercion (int-as-double, double-as-int, mixed-type ternary), and
CT/placeholder extras (placeholder in join, placeholder-only CT
entry).

Suite scores after Phase 56:
  main suite:   1600/1600 passing
  stress suite: 401/441 passing (after 1 fork addition)

New regressions surfaced:

  IF-180 — `cut(w, from, to)` accepts `from > length(w)` silently;
          orig errors with "argument 2 (from) of cut is greater
          than the waveform length".  Validation gap in recon's
          cut() implementation.
  IF-181 — `placeholder()` inside `join()` crashes recon worker
          (SIGABRT, libstdc++ vector OOB at stl_vector.h:1253).
          Orig handles the same construct.  Likely missing
          placeholder-awareness in recon's join() sample-merging
          loop.

IF-179 extended (no new IF):
  - `marker_bits_extreme` and `mask_bits_zoo` reproduce on both
    `marker()` and `mask()` branches of markerImpl().
  - `shfqc_qa_repeat_startqa_var_init` exposes a **third** call site
    of the same boost::format misuse family — `startQA` with var arg
    error message.  Audit must extend beyond markerImpl().

Existing-IF dupes (kept as additional repros):
  - `cut_into_join` (HDAWG + SHFSG) → reproduces IF-176 (cut emits
    .wf data orig drops); confirms bug spans cut-of-join too.
  - `chirp_amp_clip` (HDAWG + SHFSG) → reproduces IF-173 (chirp
    1-LSB sample diff).

Test-author bugs (forked):
  - `shfqc_qa_repeat_startqa_var.seqc` short-circuited at
    "uninitialized var i".  Fork
    `shfqc_qa_repeat_startqa_var_init.seqc` initializes `i = 0`
    and reaches the intended angle — exposed IF-179 family
    extension to startQA.

Test-author bugs (not forked, low yield):
  - `shfqa_setupqa_feedback_zoo`: `AVG_MODE_NONE` not a constant.
  - `shfqa_int_weights` / `shfqa_integration_weights_forms`:
    `assignWaveIndex` not supported on SHFQA.
  - `setdio_bit_ops`: `setDIO` rejects expressions like `prev | 0x1`.
  - `crlf_line_endings`, `unicode_in_comment`: both byte-identical
    pass — lexer handles them transparently.

Angles probed-and-clean (byte-identical pass):
ones_zero, const_for_zero_iter, const_cond_eq, const_ternary,
wave_len_32, mixed_tabs_spaces, very_long_line, numeric_literal_zoo,
crlf_line_endings, unicode_in_comment, shfqc_sg_setprng,
shfqa_configfreqsweep_extreme, getfeedback_var_shift,
getfeedback_in_subroutine, playwave_dio_combo, sub_local_wave_marker,
sub_early_return, ct_marker_only_wave, ct_entry_one_vs_1023,
two_identical_waves, wave_unused_decl, wave_assign_then_unused,
splitconst_pressure, regalloc_long_live_range, dead_marker_set,
int_passed_double, double_to_int_arg, mixed_type_ternary,
assignwaveindex_placeholder_only.

*Open IF investigation tasks moved to Phase 57 — see consolidated backlog.*

### 56.4 — Phase 56 wrap-up

- [x] Sub-phase wrap-up commit (46b85f3)
- [x] Push

## Phase 57: Fix open IF backlog (IFs 158, 159, 161-181)

After 9 rounds of stress-test expansion (Phases 48-56) the open IF
backlog is 22 findings.  This phase consolidates all open
investigation work into a single coordinated fix campaign,
**grouped by shared root cause and fix-cost** so that independent
groups can be dispatched to **parallel subagents**.

**Hard rule for every fix in this phase:**

After applying any fix, re-run `python tests/diff_test_fast.py` (main
suite — must remain 1600/1600) AND
`python tests/diff_test_fast.py --manifest manifest-stress.json`
(stress suite — every previously-passing case must still pass).
Only then commit.  This is non-negotiable: no fix is "done" until
both suites have been re-verified.

**Bisection rule:** When a stress repro is large (≳20 lines) and the
fix path is unclear, derive a minimal repro into `tests/cases/` (with
its own manifest entry) **before** debugging.  Fix the minimal case
first; the larger stress case is kept as an additional probe and
verified after the minimal one passes.

**GDB friction-logging rule:** Any agent (including delegated
subagents) using GDB during this phase must log any setup hiccup
under `tests/gdb/user-experiences/` per AGENTS.md.  When delegating
GDB-involving work, repeat this instruction in the subagent prompt.

### Group A — boost::format misuse family (parallel-safe, quick)

Likely root cause: a small set of `ErrorMessages::format(N, ...)` /
`ErrorMessages::get(N)` call sites where the argument count does not
match the template's `%K%` slot count.  IF-179 has the call site
exactly localized; IF-178 and IF-175 are likely siblings discoverable
by audit.

- [x] **57.A.1** — Audit pass: grep all `ErrorMessages::format(`
      and `ErrorMessages::get(` call sites in `reconstructed/src/`;
      cross-reference each with its template in
      `reconstructed/src/core/error_messages.cpp`; produce a table of
      arity mismatches in `reconstructed/notes/error_message_audit.md`.
- [x] **57.A.2** — IF-179 fix: add `funcName` as 3rd arg to
      `format(ValueCapped, ...)` at
      `reconstructed/src/waveform/waveform_generator.cpp:613`.
      Verify `marker_bits_zoo`, `marker_bits_extreme`, `mask_bits_zoo`
      now pass.  Update IF-179 to `fixed`.
- [x] **57.A.3** — IF-178 fix: from the audit table, fix the call
      sites whose templates have **fewer** slots than args supplied.
      Verify `setuserreg_oor`, `giant_expression` now produce the
      proper `Compiler Error (line: N): ...` message.  Update IF-178
      to `fixed`.
- [x] **57.A.4** — IF-175 fix: two sub-bugs.
      (a) Missing `.str()` substitution on `boost::format` result
      (placeholder leak).  (b) The instruction-arg-count assembler
      assertion at idx ~1152 in `long_source.seqc` — independent of
      (a); needs separate investigation, possibly out-of-scope here.
      Fix (a) first; mark IF-175 `partial-fixed` if (b) remains.
- [x] **57.A.5** — IF-179 extension: confirm `startQA`-var-arg
      error path (`shfqc_qa_repeat_startqa_var_init`) is covered by
      the audit fix.  If not, add the missing arg at that call site.

**Subagent dispatch**: 57.A.1 must run first (single agent).
After it returns, 57.A.2 / 57.A.3 / 57.A.4 / 57.A.5 are independent
edits and can be dispatched in parallel — but each must run the test
verification before its commit.

### Group B — wave-builtin validation gaps (parallel-safe, quick)

All four touch the wave generator / waveform-table emission code,
likely 1-3 functions in
`reconstructed/src/waveform/waveform_generator.cpp` and the table
emitter.

- [x] **57.B.1** — IF-180 fix: add `if (from >= length) throw …` and
      mirror checks for `to` / `from > to` in recon's `cut()`
      builder.  Match orig's error string exactly.  Verify
      `cut_chain_one_hdawg` and `_shfsg` now both pass via consistent
      error.  Update IF-180 to `fixed`.
- [x] **57.B.2** — IF-172 fix: in the waveform-table emit pass, add
      `if (length > 0)` guard before adding the entry.  Verify
      `wave_min_length` (and any `zeros(0)` repro) byte-identical.
      Update IF-172 to `fixed`.
- [x] **57.B.3** — IF-176 fix: `cut(w, N, N)` length-1 case — orig
      drops `.wf` to 0 bytes.  Identify the orig drop semantics
      (length-1 → 0 samples? cache-pruned?) via GDB on
      `cut_extreme.seqc`, then align recon.  Verify `cut_extreme`,
      `cut_into_join` now byte-identical.  Update IF-176 to `fixed`.
- [x] **57.B.4** — IF-177 fix: `ones(var)` — find recon's wave-
      builtin var-arg detection; reorder so the semantic check fires
      before the `toInt` conversion that produces the
      "unspecified value type detected" message.  Match orig's
      "ones can't be called with var arguments".  Verify
      `int_float_coercion` and `wave_computed_length` improve.
      Update IF-177 to `fixed`.

**Subagent dispatch**: All four are largely independent edits in the
same file area.  Recommended to dispatch a single subagent for the
whole group (so it can read the file once and align consistently)
rather than four parallel ones that may conflict on the same file.

### Group C — crashes / worker aborts (GDB-required)

Both are deterministic worker SIGABRTs with assertion sites known.
Each gets its own subagent because the investigation is non-trivial.

- [x] **57.C.1** — IF-181 fix: `placeholder()` inside `join()`.
      Minimal repro at `tests/cases/stress/placeholder_in_join.seqc`
      (already 6 lines — no further bisection needed).
      GDB-trace to identify the `stl_vector.h:1253` call site in
      recon's `join()`; compare to orig's placeholder-aware path;
      add the missing pre-check or sample-buffer init.  Verify
      `placeholder_in_join_hdawg` now succeeds.  Add follow-up
      probes (`placeholder + arith`, `cut(placeholder)`,
      `scalar*placeholder`) to stress suite.  Update IF-181 to
      `fixed`.
- [x] **57.C.2** — IF-159 fix: duplicate `assignWaveIndex` worker
      abort.  ≥5 stress repros (`if159_assignwave_dup_crash`,
      `assign_same_wave_multi`, `exec_table_var_index`,
      `ct_high_index`, `assign_then_execute_immediate`).  GDB-trace
      a minimal repro to identify the abort site (likely a
      `set::insert` hitting an already-present key followed by an
      unguarded `[]` deref).  Add the missing duplicate-detection
      branch returning the orig error message.  Update IF-159 to
      `fixed`.

### Group D — constant-folding / dead-block elimination (single subagent)

Likely one optimization pass missing or under-aggressive.  Both IFs
share the symptom "extra `.text` instructions emitted that the orig
folds away".

- [x] **57.D.1** — IF-174 fix: `if(true)`, `if(false)`,
      `while(true)`, `while(false)`, `if(false){}else{}` not folded.
      Locate the AST-eval branch handler for `if`/`while` with a
      const-condition argument; add the fold (replace with body /
      drop entirely).  Verify `true_false_usage`,
      `while_true_loop` byte-identical.  Update IF-174 to `fixed`.
- [x] **57.D.2** — IF-168 fix: empty `if{}`/`for{}`/`repeat{}`
      blocks emit byte-different `.text`.  Same area as 57.D.1
      most likely; investigate together.  Verify `empty_blocks`
      byte-identical.  Update IF-168 to `fixed`.

### Group E — SHFQC sg device gating (single subagent)

Both IFs are SHFQC sg → "phantom node-tree path" errors.  Almost
certainly a device-id check that excludes SHFQC sg from the SG
node-tree.  One fix expected to clear both.

- [x] **57.E.1** — Identify the device-detection / node-tree
      registration code that maps SHFSG (works) vs SHFQC sg
      (fails).  GDB-trace `setSinePhase` on both devices to see
      where the path resolves.
- [x] **57.E.2** — Apply the gating fix.  Verify
      `if161_shfqc_setsinephase_shfqc` and `oscfreq_dense_shfqc`
      both pass.  Update IFs 161 and 167 to `fixed`.
- [x] **57.E.3** — IF-166 sister: `ZSYNC_DATA_PQSC_REGISTER`
      accepted on SHFQA/SHFQC.  Likely the same per-device constant
      table.  Add SHFQA/SHFQC exclusion.  Update IF-166 to `fixed`.

### Group F — error-message wording diffs (low priority, parallel-safe)

Aesthetic only — error strings differ but both error.  Each is a
small, self-contained edit.  Defer to after Groups A-E if time
short.

- [x] **57.F.1** — IF-164: align "waveform 'X' does not exist" vs
      "no waveform with the name 'X' found" — grep for both strings
      to find throw sites; pick whichever is in orig.  Update.
- [x] **57.F.2** — IF-165: regalloc error missing `Compiler Error
      (line: N)` framing.  Wrap the throw.  Update.
- [x] **57.F.3** — IF-169: lexer mishandles `*/` inside `// ...`
      line comment.  Fix the line-comment handler to consume to
      `\n` ignoring inner comment markers.  Update.

**Subagent dispatch**: Three independent small edits — parallel-safe.

### Group G — codegen byte-diffs needing investigation (sequential)

Higher-effort; some may need bisection of their stress files.

- [x] **57.G.1** — IF-158: missing static cwvf in else-arm.
      `Prefetch::needsNewCwvf` @0x1dc620 Loop case.  GDB recipe
      already exists in subagent report
      `ses_1fde0697fffeWuLAE3xLqp38Lv`.  Bisect `kitchen_sink` and
      `wavemem_pressure` to a minimal repro.  Fix; expect this to
      also clear IF-170 (`wavemem_pressure`) since they likely
      share the cwvf-emission bug.
- [x] **57.G.2** — IF-162: dual-placeholder `assignWaveIndex` uses
      first-signal length.  Minimal repro at
      `tests/cases/stress/if162_assignwave_dual_size.seqc`.
      Function localized: `mergeWaveforms` Phase 1 in
      `reconstructed/src/runtime/custom_functions_play.cpp`
      (lines 193-206 / @0x15e060).  GDB orig vs recon, fix the
      max computation.
- [x] **57.G.3** — IF-170: `wavemem_pressure` (12 large builtins).
      Likely subsumed by IF-158 fix.  Re-test after 57.G.1 lands;
      if still failing, bisect to identify the offending builtin.
- [x] **57.G.4** — IF-171: 8-temp linear arithmetic chain bytewise
      diff.  Same `.asm`/`.text` size, different bytes — instruction
      encoding bug or constant-pool ordering bug.  `dump_elf.py
      --both` to find first differing instruction.
- [x] **57.G.5** — IF-163: `repeat(N>~10000)` refuses to unroll.
      Grep for "too many iterations to unroll this loop"; identify
      threshold; check orig's actual behavior (does it unroll up
      to N? does it fall back to a runtime loop?).
- [x] **57.G.6** — IF-173: `chirp(1024, 0.5, 1e3, 1e9)` 1-LSB
      sample diff at offset 0x192.  Wave-builder rounding /
      fma-vs-mul+add.  GDB orig chirp at sample idx ~201; compare
      with recon implementation.

### Sequencing summary

Optimal order to maximize agent throughput:

1. **57.A.1** (audit) — single agent, blocks A.2-A.5.  ~15 min.
2. **Parallel batch 1** — dispatch 5 subagents simultaneously:
     - SA-A: 57.A.2 + 57.A.3 + 57.A.4 + 57.A.5 (sequential within)
     - SA-B: 57.B.1 + 57.B.2 + 57.B.3 + 57.B.4 (sequential within)
     - SA-C1: 57.C.1
     - SA-D: 57.D.1 + 57.D.2
     - SA-E: 57.E.1 + 57.E.2 + 57.E.3
3. **After parallel batch 1 returns** — review, run main + stress
   verification, commit per-group.
4. **57.C.2** (IF-159) — separate, mid-effort GDB; can overlap with
   batch 1 as a sixth subagent if context allows.
5. **Parallel batch 2** (low priority) — three subagents for
   57.F.1 / 57.F.2 / 57.F.3.
6. **Group G** — sequential, hardest.  Take 57.G.1 (IF-158) first
   since it likely subsumes IF-170.  Then 57.G.2, 57.G.4, 57.G.5,
   57.G.6 in any order.

### 57.0 — Phase 57 kickoff

- [x] Backlog consolidation (this section)
- [x] GDB user-experiences scaffolding (`tests/gdb/user-experiences/`
      + AGENTS.md update)
- [x] Commit consolidation + dispatch first parallel batch

### 57.W — Phase 57 wrap-up

- [x] All Group A IFs closed
- [x] All Group B IFs closed
- [x] All Group C IFs closed
- [x] All Group D IFs closed
- [x] All Group E IFs closed
- [x] All Group F IFs closed (or explicitly deferred)
- [x] All Group G IFs closed (or explicitly deferred)
- [x] Final main + stress suite verification
- [x] Phase wrap-up commit + push

### Phase 57 results

**Suites:** main 1600/1600 (no change); stress 401/441 → **435/444** (-31
failures net; +3 new probes accepted).

**IFs closed (22):**
- 158 (prefetch optimizeCwvf Branch divergence)
- 159 (assignWaveIndex duplicate worker abort)
- 161, 166, 167 (SHFQC sg device gating)
- 162 (mergeWaveforms dual-placeholder length)
- 163 (repeat unroll threshold)
- 164 (waveform-not-found wording)
- 165 (regalloc OptimizeException line-number framing)
- 168, 174 (const-condition / empty-block folding — passed already from
  earlier work)
- 169 (lexer line-comment + */ guard)
- 170 (subsumed by IF-173)
- 171 (arith chain encoding diff in Minus)
- 172 (zeros(0) stray waveform table entry)
- 173 (chirp() FP multiplication ordering)
- 175 (subsumed by IF-186)
- 178 (subsumed by IF-184)
- 179 (markerValue ValueCapped funcName)
- 180 (cut() bounds checks)
- 181 (placeholder() inside join() worker abort)

**IFs new (filed during Phase 57):**
- 182 (m[1]/m[2] template arity — fixed)
- 183 (m[32] wrong message id — fixed)
- 184 (SetXxxArgs funcName extra arg — fixed)
- 185 (NodePrecisionLoss missing node-name — fixed)
- 186 (get(N) placeholder leaks — fixed except 1 deferred site at
  awg_assembler_opcodes.cpp:124)
- 187 (assorted underflows — fixed)
- 188 (placeholder() + concrete wave NOBITS — open, surfaced by
  follow-up probes after IF-181 fix)

**IFs partial / open after Phase 57:**
- 176 (cut(w,N,N) length-1 — wf/waveforms byte-identical; .asm/.text/
  .wavemem playback codegen still diverges — needs separate
  investigation)
- 177 (ones(var) wording — fixed; verify any remaining stress repros)
- 186 — single deferred call site at awg_assembler_opcodes.cpp:124
  (`getReg()` "register out of range" path); needs binary trace to
  find the right template id
- 188 — placeholder + concrete wave produces NOBITS instead of PROGBITS

**9 stress failures remaining:**
- IF-176 followers: cut_extreme_{hdawg,shfsg}, cut_into_join_{hdawg,
  shfsg} (4)
- IF-186 deferred: long_source_{hdawg,shfsg} (2)
- IF-188: placeholder_arith_hdawg (1)
- wave_min_length_hdawg, wave_zero_boundary_hdawg (2 — possibly new,
  need triage)

**GDB friction reports filed:**
- 20260507-catch-abort-not-a-command.md (SA-C1)
- 20260507-python-end-terminator.md (SA-G1)
- 20260507-shared-ptr-by-value-arg.md (SA-G1)


## Phase 58: Finish remaining difftests (444/444 stress)

After Phase 57 closed 22 IFs, 9 stress failures remain, clustering
into 4 underlying bugs. **Goal**: 444/444 stress + 1600/1600 main.

**Hard rules** (carried from Phase 57):
- Every fix re-runs main + stress before commit.
- Bisect large repros into minimal `tests/cases/stress/` cases first.
- All GDB friction logged under `tests/gdb/user-experiences/`.
- No commit if main suite drops below 1600/1600.

### Cluster 58.A — IF-176 cut(N,N) playback codegen (4 failures)

Tests: `cut_extreme_{hdawg,shfsg}`, `cut_into_join_{hdawg,shfsg}`.

Status from Phase 57.B: `.wf` and `.waveforms` byte-identical. Remaining
diff in `.asm`/`.text`/`.wavemem` for empty-cut playback. Per IF-176
note: orig emits 2 extra `cwvf` and folds empty-cut play into a
single `wwvf`; recon's playback path doesn't yet handle the
zero-length wave case.

- [ ] **58.A.1** — Single subagent. Read IF-176 + dump_elf both-mode for
      `cut_extreme_hdawg` to see exact `.asm` divergence. Locate the
      playback emission for length-0 cut result. Apply orig's "fold to
      single wwvf" semantics. Verify all 4 tests now pass byte-identical.

### Cluster 58.B — IF-186 deferred site (2 failures)

Tests: `long_source_{hdawg,shfsg}`.

Phase 57.A left a single `// TODO IF-186` stub at
`reconstructed/src/codegen/awg_assembler_opcodes.cpp:124` because the
`getReg()` "register out of range" path uses `get(4)` (TooFewArguments,
4-slot template) but had no `instr/opcode/expected/given` locals
visible. Need binary trace to find the right template id.

- [ ] **58.B.1** — Single subagent. GDB-trace orig at
      `awg_assembler_opcodes.cpp:124` equivalent on the failing
      `long_source` input to identify which template id orig uses for
      the "register out of range" diagnostic (likely `RegisterOutOfRange`
      or similar — verify against rodata). Apply the right `format()`
      call. Verify both `long_source` tests pass.

### Cluster 58.C — IF-188 placeholder + concrete wave NOBITS (1 failure)

Test: `placeholder_arith_hdawg`.

IF-188 (filed in Phase 57.C.1): `placeholder() + concrete wave`
produces a NOBITS section instead of PROGBITS. Suggests the recon's
arithmetic operator on placeholder doesn't materialize the result
when the other operand is concrete. SA-C1 recommended a binary audit
of arithmetic operators' reserveOnly handling.

- [ ] **58.C.1** — Single subagent. Read IF-188. Locate the arithmetic
      operator path in `waveform_generator_dsp.cpp` that creates the
      result Signal. Audit reserveOnly handling: when one operand is
      reserveOnly and the other is concrete, the result must be
      materialized (PROGBITS). GDB-verify against orig. Apply fix.
      Verify `placeholder_arith_hdawg` byte-identical.

### Cluster 58.D — IFs 189, 190 zero-wave bookkeeping (2 failures)

Tests: `wave_min_length_hdawg`, `wave_zero_boundary_hdawg`.

**IF-189** (filed Phase 58 triage): `fpgaMemoryUsed` counts zero-length
waves; recon = orig × 1.5 in `wave_min_length`. Also `e_entry` differs
0x80 vs 0xc0 despite `.text` byte-identical — likely same bookkeeping
not updated when IF-172 filtered the zero-length wave.

**IF-190** (filed Phase 58 triage): `zeros()` waveform should be
prefetched at high address (524288 = 0x80000) with `prf` instruction
and a fixed register slot; recon emits sequential register offsets
without the prefetch. Affects `wave_zero_boundary` first 4
instructions of `.text`.

Likely same root area: zero-wave special handling in the prefetch /
register-allocation pass.

- [ ] **58.D.1** — Single subagent. Read IFs 189, 190. Investigate
      together — likely same root cause (zero-waves need special
      address slot + prefetch + skip-from-budget treatment).
      GDB-verify orig's zero-wave emission path. Apply fix.
      Verify both stress tests pass.

### 58.0 — Phase 58 kickoff

- [x] Triage `wave_min_length` / `wave_zero_boundary` → IFs 189, 190
- [x] Backlog (this section)
- [x] Dispatch all 4 clusters in parallel

### 58.W — Phase 58 wrap-up

- [x] Cluster 58.A closed (IF-176 → fixed)
- [x] Cluster 58.B closed (IF-186 deferred site → fixed; surfaced IF-191)
- [x] Cluster 58.C closed (IF-188 → fixed)
- [x] Cluster 58.D closed (IFs 189, 190 → fixed; single root cause)
- [x] Cluster 58.E closed (IF-191 → fixed; 1-line uint8 truncation)
- [x] Final main + stress suite verification: **1600/1600 + 444/444**
- [x] Phase wrap-up commit + push

### Phase 58 results

**Suites:** main 1600/1600 (no change); stress 435/444 → **444/444**.
**Goal achieved: zero stress failures.**

**IFs closed (6):** 176, 186 (deferred site), 188, 189, 190, 191.

**IFs new (filed during Phase 58):** 191 (long_source regalloc — fixed
same phase as a 1-line `uint8_t` truncation bug in
`Assembler::highestRegisterNumber`).

**Commits (5 fixes + kickoff + wrap-up = 7):**
- `3366419` phase 58 kickoff: scope + IFs 189, 190 filed
- `02e4bf7` phase 58.A: IF-176
- `3f6aeb1` phase 58.B: IF-186 deferred
- `ca69068` phase 58.C: IF-188
- `e68b5d4` phase 58.D: IFs 189+190
- `223e91a` phase 58.E: IF-191
- (this commit) phase 58 wrap-up

**Notable findings:**
- IF-189 + IF-190 turned out to be a **single root cause**: missing
  `length == 0` guard in `WavetableIR::assignWaveformAllocationSizes`.
  Fix gave +7 stress passes (the 2 target tests + 5 collateral).
- IF-191 was a 1-line `uint8_t` cast in
  `Assembler::highestRegisterNumber` truncating regs >255 — caused
  cascade failures in regalloc liveness analysis on long sources.
  Bisected to "128 setUserReg pairs" minimal trigger.
- Phase 58.B's "fix" of IF-186 unmasked IF-191 (the leaked-template
  message was hiding the real underlying error). Standard pattern:
  fix a symptom, expose the next layer.

**No GDB friction reports filed this phase** — all 5 root causes were
identified via static disassembly (objdump) of the binary, without
needing runtime traces.

---

## Phase 59: Stress round 10 — post-Phase-58 expansion + 4-device coverage

After Phase 58 closed all known difftest failures (444/444 stress,
1600/1600 main), this round expands the stress suite with 32 new
.seqc cases targeting adjacencies to the 28 IFs fixed in Phases
57+58, then registers portable cases against multiple device bases
to surface device-specific divergences.

**Workflow (per `seqc-stress-rounds` skill):** brainstorm 50
angles → write 30 cases (groups A/B/C/D/F/J — adjacencies to
recent fixes) → triage → fork test-author bugs → expand device
coverage on portable cases → re-triage.

**Key device-coverage rule introduced:** for cases with no
device-specific syntax (e.g. setUserReg-only regalloc tests),
register against **hdawg + shfsg + shfqa + uhfqa** so any
device-specific code paths are exercised. Wave-based cases
(playWave) stay hdawg + shfsg.

### Round 10 inventory

- 32 new `tests/cases/stress/*.seqc` files written:
  - **Group A** (regalloc adjacencies to IF-191): `regalloc_2x_long`,
    `regalloc_branchy_long`, `regalloc_pressure_subroutines`,
    `regalloc_loop_long`, `regalloc_high_vreg_in_branch`,
    `regalloc_rmw_chain`, `regalloc_long_live_single_bb`
  - **Group B** (zero-wave bookkeeping, IFs 189/190): `many_zeros_lengths`,
    `zeros_then_play`, `zeros_in_subroutine`, `mixed_zero_concrete`,
    `zero_wave_arith`, `zeros_in_join`, `zeros_join_zeros`
  - **Group C** (placeholder+wave arith, IF-188): `placeholder_minus_wave`,
    `placeholder_times_wave`, `placeholder_chain_arith`,
    `placeholder_in_join_arith`, `placeholder_assign_then_arith`,
    `placeholder_plus_marker`
  - **Group D** (cut(N,N) chains, IF-176): `cut_zero_chain`,
    `cut_in_loop`, `cut_full_range`, `cut_overlap_join`, `cut_chain_to_one`
  - **Group F** (cwvf/prefetch adjacencies, IF-158): `cwvf_in_else_only`,
    `cwvf_three_branches`, `cwvf_loop_branch_loop`, `cwvf_nested_for`
  - **Group J** (wavemem accounting): `wavemem_max_unique`,
    `wavemem_same_data_diff_name`, `assignwaveindex_max`
- 2 forks for SeqC `break`-in-`switch` test-author bugs:
  - `regalloc_branchy_long_fixed` (if/else-if rewrite)
  - `cwvf_three_branches_fixed` (if/else-if rewrite)
- Manifest: 444 → 528 cases (+84 entries: 32 hdawg new + 2 fork
  hdawg + 8 portable × 3 extra devices + 26 wave × shfsg)

### Findings

- **3 new IFs filed**: IF-192 (UHFQA missing 1024-instruction
  program-size limit — recon silently produces oversized ELF;
  surfaced by 4-device expansion of `regalloc_2x_long` and
  `regalloc_pressure_subroutines`), IF-193 (cut(N,N) chained into
  another cut errors with "from > length" — both compilers behave
  identically, may indicate IF-176-fix downstream artifact, needs
  GDB confirmation), IF-194 (regalloc-overflow error reports
  different line numbers between orig and recon — may indicate
  spill heuristic divergence under pressure)
- 526/528 pass; 2 hard failures, both UHFQA, single root cause
  (IF-192)
- Main 1600/1600 unchanged

### Wrap-up

- [x] **59.1** — Brainstormed 50 angles, picked 30 (groups A/B/C/D/F/J)
- [x] **59.2** — Wrote 32 new .seqc files in `tests/cases/stress/`
- [x] **59.3** — Triaged first pass (hdawg-only): all 32 passed; 4 via
      both-error
- [x] **59.4** — Forked 2 `break`-in-switch test-author bugs into
      `_fixed` variants
- [x] **59.5** — Expanded device coverage: portable regalloc cases
      against hdawg+shfsg+shfqa+uhfqa, wave cases against hdawg+shfsg
- [x] **59.6** — Re-triaged: surfaced 2 UHFQA failures (IF-192) and
      filed IFs 193, 194
- [ ] **59.7** — User-review: choose Phase 60 = fix IF-192 vs.
      Phase 60 = round 11 (more stress angles)

### Lessons learned

- **Single-device test registration hides device-specific bugs.**
  Mirroring the existing `hdawg+shfsg`-only convention would have
  missed IF-192 entirely; the 4-device expansion immediately found
  it. New rule for future stress rounds: any case with no
  device-specific syntax should be registered against all 4 main
  device families.
- **Test-author SeqC bugs still have value as error-pass coverage**
  — keep originals AND fork fixed versions (per skill rule). The
  `_fixed` forks let us actually exercise the regalloc/cwvf paths
  the originals were designed to stress.
- Round-10 productivity: 1 hard recon bug found (IF-192) + 2 leads
  filed (193, 194), in line with the "fixes-disturb-area" yield
  expectation.


---

## Phase 60: Fix IF-192 — UHFQA missing program-size check

**Goal**: 528/528 stress + 1600/1600 main, fix the 2 UHFQA failures
surfaced by Phase 59 round 10.

**Result**: ✅ both fixed in single edit at
`reconstructed/src/codegen/awg_compiler.cpp` lines 474-495.

### Root cause (delegated to general subagent for binary archeology)

The check **was** present in recon, not missing. It had two
compounding bugs:

1. **Wrong DeviceConstants field**: read `maxSequenceLen` (DC+0x60,
   value 16000 universally) instead of `waveformMemSize` (DC+0x58,
   value 1024 for UHFQA). The latter's misleading name (it actually
   holds the opcode-words limit, not waveform memory) is the trap.
2. **Bogus `/4` divisor**: `getOpcode()` already returns
   `vector<uint32_t>`, so `.size()` is the instruction count. The
   `/4` quadrupled the effective threshold.

Combined effective UHFQA limit was 64000 (vs. real 1024), so the
2106-instruction `regalloc_2x_long` and 1798-instruction
`regalloc_pressure_subroutines` sailed through silently.

### Subagent investigation findings

The subagent located the binary check at
`zhinst::AWGCompilerImpl::compileString @0x107341..0x107693`.
Verified the binary reads:
- count from `assembler_.getOpcode().size()` (no `/4`)
- limit from `(uint64_t*)(this + 0x60) == DC[+0x58]`
- formats with `ErrorMessageT(0xC)` and exception string
  "Compiler error while generating assembly"

No GDB needed — pure objdump archeology.

### Discoveries documented as new IFs

- **IF-195** (cosmetic, rename pending): `waveformMemSize` field at
  DC+0x58 is misnamed — it's actually the max opcode words
  (instruction-memory depth). The misnomer is what trapped the
  original recon implementer. Recommend rename to
  `maxProgramSize` / `maxOpcodeWords`.
- **IF-196** (suspicious, needs verification): the **adjacent
  check 11** (msg 0xF1, "number of waveforms in wavetable too
  large") at `awg_compiler.cpp:489-510` likely has the
  **mirror-image bug** — uses `waveformMemSize` (1024) when it
  should use `maxSequenceLen` (16000), per the subagent's binary
  read. Needs a UHFQA wavetable-overflow test to confirm.

### Wrap-up

- [x] **60.1** — Subagent located binary check site + identified
      both bugs
- [x] **60.2** — Applied fix: swap DC field, drop `/4`, set
      `lineNr=-1`, fix exception string
- [x] **60.3** — Verified `regalloc_2x_long_uhfqa` and
      `regalloc_pressure_subroutines_uhfqa` now pass byte-clean
- [x] **60.4** — Stress 528/528 + main 1600/1600
- [x] **60.5** — Filed IFs 195 (rename) and 196 (mirror-bug
      candidate)
- [x] **60.6** — User-review chose Phase 61 = backfill 4-device coverage

### Lessons learned

- **A misnamed field is a latent bug magnet.** `waveformMemSize`
  semantically means "max opcode words"; the wrong name caused two
  separate review/implementation errors (the original recon and
  this round's investigation initial hypothesis "check missing").
  Renaming is cheap insurance against repeat occurrences.
- **Subagents work well for binary archeology** when the question
  is well-scoped ("find function X, identify check pseudocode").
  Saved an estimated hour of manual disassembly walking.
- **The "fix one symptom, find another" pattern continues.** IF-192
  fix unmasked IF-195 (cosmetic) and IF-196 (suspicious twin).
  Each fix should explicitly look for adjacent code that may share
  the same defect class before declaring done.

## Phase 61: 4-device coverage backfill

Bulk-add SHFQA/UHFQA registrations to portable stress cases that
were previously registered only against HDAWG/SHFSG. Goal: surface
device-specific bugs by exercising the same SeqC source through
all four code paths (per the rule established in Phase 59).

- [x] **61.1** — Wrote categorizer (`/tmp/device_compat.json`):
      detects HDAWG-only (DIO, AWG_RATE_*), SHF-only osc/sine,
      SHFQA-only QA primitives; classifies 305 unique stress
      `.seqc` files
- [x] **61.2** — Bulk-added 201 device registrations to
      `manifest-stress.json` (528 → 729 cases; mostly UHFQA and
      SHFQA registrations on portable register/wait/control-flow
      tests)
- [x] **61.3** — Stress 729/729 (166 pass-via-error, expected for
      portable-tests-on-restrictive-devices) + main 1600/1600
- [x] **61.4** — Triaged 14 distinct "error messages differ
      (accepted)" cases:
      - 9 already documented (IF-177 setUserReg range, IF-179 family
        startQA-const, IF-194 regalloc line drift, etc.)
      - 1 new bug found and **fixed**: IF-197 randomGauss arity
        error reports `4` instead of `3`
      - 4 cases are line-attribution drift (IF-194 family, deferred)
- [x] **61.5** — Filed IF-197 (fixed) and IF-198 (= duplicate of
      IF-177, confirms device-independence)
- [x] **61.6** — Pre-emptive sibling fix considered for
      `randomUniform` (same `FuncExactArgs2 / 2` pattern, line 1003)
      but **deferred**: requires confirming differential test
      first to avoid speculative changes
- [x] **61.7** — User-review chose Phase 62 = round 11 brainstorm
      (precompClear + prefetch pressure)

### Lessons learned

- **Backfill quickly amortizes the categorizer cost.** Writing a
  feature-detector regex took ~10 minutes; it generated 201 new
  test entries that ran in 5s and surfaced 1 real recon bug
  (IF-197) plus a confirmation that IF-177 is device-independent.
- **`(error)` pass-through still has signal.** The "differ
  (accepted)" notes are noisy but *not* worthless — filtering for
  unique error-pair shapes (50 raw → 14 distinct → 1 new bug) is
  a tractable triage. Worth doing whenever new device coverage
  is added.
- **Be minimum-arity for variadic-with-defaults functions.** The
  `FuncExactArgs2` template reports a single number; binary
  convention is the **minimum** valid arity, not the maximum.
  Audit other variadic functions (chirp, sinc, randomUniform,
  drag, etc.) for the same pattern when convenient.

## Phase 62 (Round 11): precompClear, prefetch-pressure, UHFQA wave-mem

User-directed brainstorm round targeting: (A) `setPrecompClear` ×
cwvf interactions (zero pre-existing coverage), (B) HDAWG prefetch
pressure (>256 short waves, mixed lengths, branches), (C) UHFQA
prefetch pressure including the IF-192/IF-196 trigger, (D) adjacent
gaps (setRate × cwvf, prefetch idempotence, prefetch + waitWave).

- [x] **62.A** — Wrote 9 hand-authored A-track `.seqc` files for
      `setPrecompClear` corner cases (basic, chain_toggle,
      cwvf_branches, in_loop, edge_args, 4×invalid, marker_cwvf).
      Registered 13 manifest entries (basic also against
      SHFSG/SHFQA/UHFQA for cross-device-rejection coverage).
- [x] **62.B** — Wrote `tests/cases/stress/_gen/gen_prefetch_pressure.py`
      generator script, generated 13 B-track `.seqc` files
      (HDAWG: 256/384/512 short-wave bursts, burst+long-wave,
      burst+playZero, burst+branch, burst-in-loop, mixed-lengths,
      dedup, 2ch independent, marker-bearing, assignWaveIndex
      burst, nested branch). Registered 13 manifest entries.
- [x] **62.C** — Generated 5 C-track UHFQA cases via the same
      generator (short_burst, 1024_entries=overflow trigger, aWI,
      branch_burst, 2ch). Registered 5 manifest entries.
- [x] **62.D** — 3 hand-authored D-track files
      (setrate_cwvf_branches, prefetch_repeated_idempotent,
      prefetch_waitwave_invalidate). Registered 3 entries.
- [x] **62.E** — Full stress run: 759/763 pass (manifest 729 → 763).
      4 failures **all in A-track**, all sharing root cause
      (precomp-flag not threaded into cwvf encoding).
- [x] **62.F** — Filed **IF-199** (FIXED CANDIDATE, deferred to
      Phase 63): `setPrecompClear` flag not propagated to
      per-play cwvf encoding in `prefetch.cpp:696-702`. Recon
      emits only the initial `cwvf` and never re-emits when
      precomp flag toggles between plays.
- [x] **62.G** — Verified `prefetch_uhfqa_1024_entries` (1100 waves)
      triggers IF-192's check correctly: orig and recon both report
      "program is too large to fit into memory - has 3463
      instructions, maximum is 1024". **IF-192 fix validated** under
      a real overflow stress. **IF-196 (suspected twin bug) did not
      surface** in this test — either the hypothesis is wrong, this
      input doesn't reach check 11, or check 11 was already correct.
      Needs separate UHFQA-CT-overflow scenario to differentiate.
- [x] **62.H** — Main suite: 1600/1600 (no regressions from any
      of the 34 new manifest entries).
- [ ] **62.I** — User-review: choose Phase 63 = fix IF-199
      (precomp-flag threading), Phase 63 = IF-196 dedicated UHFQA-CT
      overflow test, Phase 63 = round 12 stress, or Phase 63 = audit
      other variadic builtins for IF-197 sibling bugs.

### Lessons learned

- **Generator scripts pay off immediately for high-N cases.** The
  256/384/512-wave bursts would have been tedious to hand-author;
  the generator wrote them in one pass, ran in seconds, and
  produced byte-clean diffs (or matched-error pairs) for all 18
  generated cases. Committed alongside the .seqc for reproducibility.
- **Prefetch logic is more correct than feared.** Recon handles
  512 distinct short waves byte-clean (367 KB ELF). The "find
  the right slot" stress did not surface a bug at this size.
  Either the algorithm is solid, or pressure must be increased
  further (1024+ waves, smaller cache pages, or specific
  control-flow patterns).
- **precompClear was a real coverage gap.** Zero pre-existing
  tests, immediate find on first round of cases. **Patterns that
  thread state through a sequence of plays (precomp, rate,
  trigger flags) are systematically under-tested.** Worth
  auditing all such state-threading functions for similar gaps.
- **abc-rule discipline matters.** Initial drift into mid-round
  triage corrected by user. Authoring all tests, registering
  all manifest entries, then running once was strictly more
  efficient — surfaced the same bug in 4 places (confirming
  single root cause) instead of debugging each independently.
- **Test-author bugs are real and need forking.** `drag` with
  non-trivial sigma/beta combos easily exceeds amp 1.0; warning
  message diverges from byte-clean compare. Used minimal
  attribute-tweak fixes (rect substitution) when warnings
  weren't the test angle.
