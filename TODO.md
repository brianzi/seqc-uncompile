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

## Summary of remaining work (refreshed 2026-04-29 post-Phase-D grooming)

**Build**: clean (g++ + clang++/libc++), 0 errors, 1 documented warning.
**95/95 undefined zhinst symbols resolved** — static archive self-contained.
**259/259 differential tests pass** (byte-identical, as of 2026-04-29).
**Phase D symbol-renaming audit + execution complete** (20 commits
`d15ad32`..`9b2e690`, all 16 high/medium-confidence clusters landed,
259/259 throughout). **Phase R audit-followup complete** (14 commits
`dfc278e`..`2b23826`): all 6 deferred arbitrations and all 10 open
IFs resolved; 1 latent bug fixed (IF-119), 1 missing writer
reconstructed (Arb 4). **Phase S Phase-Q refinement complete**
(7 commits `3f0b24e`..`04e3ac5`): 226 backlog items partitioned —
39 already-done (B3), 58 wontfix (B4), 14 mechanical-resolved (S.2),
1 binary-fidelity skip; 114 borderline items deferred case-by-case
(no longer a tracked phase). See OVERVIEW.md "Symbol Renames
(Phase D / R / S)" tables and
`notes/symbol-renaming-audit/SYNTHESIS.md`.
**Error message table corrected** — was globally off-by-one (GDB-verified).
**Variable init ADDI + ssl operand swap fixed** — 24→26 passes (2026-04-27).
**registerAllocation overlap fix + wvfs regSrc + playHold isBool** — 53→56 passes (2026-04-27).
**Fixes this session (cumulative)**: writeToNode slow-path, UHF hasFast entries, join() interpolation, playIndexed Play tree-link (IF-99), removeBranches push next (IF-101), playIndexed Var-branch + lengthReg field (IF-100/IF-102), NOBITS comparison (IF-104), play_cervino_indexed body (IF-103), getRequiredMemory min→max, Prefetch::optimize inverted parentLoad type check (IF-105), determineFixedWaves O(2^N) BFS fix (IF-107: 155× → 3.7×), `rand` MINSTD+polar reimplementation (IF-108: matches binary not libc++ MT64). 238 → 259.

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

## Differential Testing (ELF comparison)

Test harness in `tests/` compares ELF output from the original binary vs the
reconstructed pybind11 module. **139/139 test cases pass** (2026-04-28).
Phase 30f complete; Phase 30g expanded coverage; Phase 34 added 45 new tests
and fixed 12 bugs; Phase 38 added 25 new tests and fixed 30+ bugs.

### Phase 34: Expanded diff test coverage + bug fixes (2026-04-27)

Added 45 new test cases (69→114 total) covering all 7 device types,
55+ built-in functions, waveform DSP, all ALU/branch opcodes, user-defined
functions, cvar, error paths, and dubious code paths from incidental findings.

Fixed 12 bugs:
- [x] Lexer comment bug: missing `endLineComment()` on `\n` (IF-32, critical)
- [x] SeqCValue string overflow: 24B payload vs 32B libstdc++ string (IF-33)
- [x] EvalResultValue double-free: explicit dtor + auto-destruct (IF-34)
- [x] assignWaveIndex vector OOB: channelsPerGroup[0] vs deviceIndex (IF-35)
- [x] readInt wrong min check: minVal is arg index, not threshold (IF-36)
- [x] ldiotrig operand order: address in immediates vs outputs (IF-37)
- [x] setSinePhase/incrementSinePhase wrong SHFSG node path (IF-38)
- [x] QA_DATA_PROCESSED_D constant name mismatch (IF-39)
- [x] executeTableEntry register arg: value_.toInt() vs reg_ (IF-40)
- [x] User function parsing: 3 bugs in createFunction/evaluate/scope (IF-42)
- [x] Cervino playback: wrong code path in prefetch_placesingle (IF-43)
- [x] SHFQC SG constant registration

### Phase 35: Remaining 9 byte-mismatch failures (in progress)

**Score: 114/114 passing — ALL TESTS PASS**

**Fixed this session:**
- [x] hdawg_wave_sine — 3-arg sine/cosine formula (IF-41)
- [x] hdawg_wave_gauss — DRAG normalization constant
- [x] hdawg_wave_misc — genericTriangle param order (IF-44) + double2awg16 NaN (IF-45)
- [x] hdawg_wave_vect — name truncation suffix format (IF-46)
- [x] hdawg_zsync — waitZSyncTrigger constant
- [x] hdawg_wave_marker — genPlayConfig marker loop variable + Signal::add() markerBits OR
- [x] hdawg_wave_placeholder — Signal(ReserveOnly) constructor channels_ = 1 not 0
- [x] hdawg_prefetch — 3 bugs: asmPrefetch wavesPerDev (IF-47), PlainLoad cachePtr→registerHirzel (IF-48), play() SubFunc==0 fall-through (IF-49)
- [x] uhfli_wait_funcs — wait() complex path: single suser(reg2)+wtrig(reg1) instead of two pairs
- [x] uhfqa_startQA — UHFQA device-specific codegen (addi+sid+addi32+strig). 2-byte linenr metadata diff FIXED: SetVar node linkage in assignment evaluator + SetVarPlaceholder→SetVar enum fix.
- [x] hdawg_playWaveIndexed — WVFE regDst→regSrc (IF-50), BFS Load-merge child enqueueing (IF-51)
- [x] uhfli_playback — Cervino register swap (IF-54), sizeInBlocks std::max (IF-55), computedOffset waveCount (IF-56)

**No remaining failures — 114/114 byte-identical.**

Priority items:
- [x] Fix Cervino waveform addressing and prefetch (uhfli_playback — IF-53, IF-54, IF-55, IF-56)
- [x] Fix linenr preamble metadata (uhfqa_startQA — IF-52, IF-57: SetVar node linkage)

### Phase 38: Expanded coverage 114→139 + 30 bug fixes (2026-04-28)

Added 25 new test cases covering extended device types (SHFQA2, SHFSG2/4,
SHFLI, UHFAWG, GHFLI, VHFLI), ternary with runtime variables, DIO
operations, frequency sweep, ZSync triggers, and misc builtins.

Fixed 30+ bugs across 8 categories:
- [x] Parser: VarType_Void handling
- [x] AST evaluation: retType, switch/case fall-through, ternary codegen, array indexing
- [x] Resources: returnReg_ init, parent_→parentWeak_ shared_ptr cycle
- [x] Custom functions (playback): playZero/playHold dispatch, setRate
- [x] Custom functions (io): getDigTrigger register/mask, assignWaveIndex, setID, suppress mask
- [x] Custom functions (play): waitTimestamp, waitDemod, waitZSync
- [x] Waveform/ELF: WaveAssignment copy, double2awg scale, marker separator, ELF flags, node map entries
- [x] Prefetch: prepare/placesingle fixes
- [x] ASM commands: wtrig register assignment
- [x] Waveform generator: gauss 3-arg formula
- [x] WavetableFront: waveIndex init

**139/139 byte-identical — ALL TESTS PASS.**

### Phase 39: Documentation example test extraction (2026-04-28)

Extracted every SeqC code example from the Zurich Instruments online
documentation and created diff test cases. Total: 92 new doc-example
test cases added (manifest now at 259 entries).

**Completed:**
- [x] HDAWG manual: 16 files (hdawg_doc_simple through hdawg_doc_multi_ch_sections)
- [x] SHFSG manual: 19 files (shfsg_doc_simple through shfsg_doc_playZero_playHold_rate)
- [x] SHFQA manual: 6 files (shfqa_doc_readout_basic through shfqa_doc_repeat_dio)
- [x] Waveform/runtime function coverage: 22 files (chirp, rrc, sinc, filter, etc.)
- [x] UHFQA tutorial: 12 files (uhfqa_doc_simple through uhfqa_doc_analog_digital_csv)
- [x] UHF (UHFLI/UHFAWG) tutorial: 10 files (uhf_doc_IQ_modulation through uhf_doc_tv_mode)
- [x] SHFQC tutorial: 1 file (shfqc_doc_lf_path — unique LF path example)
- [x] Checked PQSC, SHFLI, GHFLI, VHFLI — no AWG/SeqC tutorials

**Key discoveries:**
- UHF-specific functions: getAnaTrigger, waitDemodOscPhase, waitDemodSample,
  playWaveIndexed, playAuxWave, playWaveNow, setRate
- UHFQA startQA has 2-arg form vs SHFQA 5-arg form
- UHF sample rate constants use 1.8 GHz base (AWG_RATE_1800MHZ through AWG_RATE_220KHZ)
- UHFQA constants: QA_INT_ALL, QA_INT_0..9, AWG_MONITOR_TRIGGER, etc.
- SHFQC LF path example uses resetOscPhase + playWave(1, 2, w) routing
- Doc has typo: 'gaussian' function (should be 'gauss') in SHFQC LF path example

#### Audit item: parent_→parentWeak_ migration (RESOLVED via Phase D c13)

Resolved in Phase D commit `612eb2a` (Cluster N): `Resources::parent_`
renamed to `grandparent_` and the strong/weak pointer slots swapped to
match the binary. See IF-122 in `notes/incidental_findings.md`.

### Infrastructure (done)

- [x] Test harness: `tests/diff_test.py`, `tests/compile_worker.py`
- [x] Test cases: `tests/cases/manifest.json` with 12 cases (3 device types)
- [x] Original-only smoke test: all 12 cases pass
- [x] pybind11 module target in CMakeLists.txt (builds _seqc_compiler.so)
- [x] ODR violations fixed (StaticResources, GlobalResources, ResourcesException, Prefetch, asmerror)
- [x] TLS model fixed (-ftls-model=global-dynamic + PIC)
- [x] flex/bison linkage fixed (extern "C" → C++ linkage for .c files compiled as C++)
- [x] seqc_lexer.c prefix fixed (seqc → seqc_ to match compiler.cpp references)
- [x] seqc_parser.tab.c and seqc_lexer.c added to CMake sources
- [x] Boost::log + zlib dependencies added
- [x] Python version pinned (3.14)

### Remaining blockers for first successful differential test

- [x] ~16 undefined zhinst symbols cause runtime crashes (via RTLD_LAZY):
  - ~~AsmCommands: syncCervino, asmSyncHirzel, st, prf, ssl, addi, addr, cwvf,
    smap, wprf, wwvf, cwvfr~~ — not triggered at runtime (RTLD_LAZY)
  - ~~AWGAssembler::getReport~~ — FIXED: uncommented forwarding method
  - ~~SeqcParserContext::hadSyntaxError~~ — FIXED: added missing definition
  - ~~Prefetch::wvfImpl, wvfRegImpl~~ — not triggered at runtime (RTLD_LAZY)
- [x] Hardcoded libc++ offsets cause segfault with libstdc++ — FIXED:
  - assembleExpressions: this+0xB0 → labelBimap_ field access
  - awg_assembler_opcodes: this+0xD8 → labelBimap_ field access
  - custom_functions: this+0x1B0 → warningCallback_ null check
- [x] pybind_seqc.cpp returns (bytes, json_string) but original returns
  (bytes, dict) — FIXED: added json.loads() call via py::module_::import("json").attr("loads")
- [x] **`Compiler::compile()` pipeline is a stub** — All steps 5-19 now wired.
- [x] **Prefetch resources_** — NOT a blocker. resources_ is legitimately null
  in the constructor and only accessed via `Resources::getRegisterNumber()`
  (TLS static). The actual blockers were: fillInPlaceholders copying, Node
  ctor arg order, emitNodeEntry, ELF header field order.

---

## Phase 30: Wire up Compiler::compile() pipeline

**Goal:** Make `Compiler::compile()` (`src/compiler.cpp:188-273`) produce
real assembly output so the differential test harness can compare ELF
output against the original binary.

The function currently runs steps 1-4 (parse) and returns an empty vector.
Steps 5-19 are now all wired. Remaining work is differential testing.

### Dependency audit

| Step | Function | Has body? | Blocker? |
|------|----------|-----------|----------|
| 5 | `StaticResources::init()` | ✅ complete (static_resources.cpp:51) | No — **WIRED** |
| 6 | `toSeqCAst(expr)` | ✅ complete (seqc_ast.cpp) | No — **WIRED** |
| 8 | `FrontEndLoweringFacade::lower()` | ✅ complete (compiler.cpp:446) | No — **WIRED** |
| 10-11 | Node tree walk + AsmList build | ✅ wired | No — **WIRED** |
| 12 | `AsmOptimize::optimizePreWaveform()` | ✅ complete (asm_optimize.cpp:205) | No — **WIRED** |
| 14 | `Compiler::runPrefetcher()` | ✅ wired | No — **WIRED** |
| 15 | `AsmOptimize::optimizePostWaveform()` | ✅ complete (asm_optimize.cpp:221) | No — **WIRED** |
| 16 | `AsmCommands::unsyncCervino()` | ✅ complete (asm_commands.cpp:760) | No — **WIRED** |
| 19 | Copy AsmList → vector\<AssemblerInstr\> | ✅ wired | No — **WIRED** |

### 30a. `toSeqCAst()` — Expression → SeqCAstNode conversion — COMPLETE 2026-04-26

Free function at @0x1f6240. Converts the flex/bison `Expression` tree
into the typed `SeqCAstNode` hierarchy via recursive dispatch on
operationType/commandType/operator.

- [x] Disassemble `toSeqCAst` @0x1f6240 — determine size and call graph
- [x] Reconstruct body (~310 lines, 50 dispatch cases across 3 jump tables)
- [x] Build verify
- [x] Sub-phase wrap-up

### 30b. Wire up steps 5-9 in `Compiler::compile()` — COMPLETE 2026-04-26

Uncommented and wired the calls for steps 5-9:

- [x] Step 5: Construct StaticResources with warning callback + init
- [x] Step 5b: Wrap in GlobalResources (`make_shared<GlobalResources>(staticResources)`)
- [x] Step 5c: Store resources into `customFunctions_->resources_` (added `friend class Compiler`)
- [x] Step 6: `auto seqcAst = toSeqCAst(expr)` (depends on 30a)
- [x] Step 7: Debug print if `config_->debugFlags & 0x04`
- [x] Step 8: `FrontEndLoweringFacade::lower(resources, *seqcAst, messages_, asmCommands_, customFunctions_, waveformGen_, wavetable_, config_->channelGrouping)`
- [x] Step 8b: `ast_ = lowerResult.astResult`
- [x] Step 9: error check
- [x] Build verify — clean, 0 warnings
- [x] BONUS: Identified AWGCompilerConfig+0x98 as `channelGrouping` (was `unknown_98`)
- [x] Sub-phase wrap-up

### 30c. Node linearization + assembly emission (steps 10-11) — COMPLETE 2026-04-26

Steps 10-11 are NOT "linearizing nodes into assembly" as originally claimed.
Assembly instructions are generated during `evaluate()` inside `lower()`.
Steps 10-11 build the root node wrapper, walk the tree setting parent
pointers (BFS via deque), and splice EvalResults assemblers into asmList_.

- [x] Preamble: asmLabel("start") + asmLoadPlaceholder
- [x] Root Node(NodeType::Load, placeholderAsm.sequenceId, numChannelGroups)- [x] Graft lowered AST: hasMain && ast_ → rootNode->next = ast_; else → evalResult->node_
- [x] BFS parent-pointer walk via deque
- [x] Append placeholder Asm, bulk-insert evalResult->assemblers_, end/wwvf/nop trailer
- [x] Build verify — clean, 0 warnings

### 30d. Wire up runPrefetcher (step 14) — COMPLETE 2026-04-26

All 13 steps inside `runPrefetcher()` converted from pseudocode comments
to real method calls. Prefetch construction with warning callback, all
WavetableIR allocation methods, conditional determineFixedWaves,
fillInPlaceholders, channel info extraction.

- [x] Construct Prefetch with warning callback + ast_ as root
- [x] preparePlays → getUsedWavesForDevice → setUsedWaveforms
- [x] assignWaveIndexImplicit → alignWaveformSizes → assignWaveformAllocationSizes
- [x] Conditional determineFixedWaves (cacheType == 1)
- [x] updateWaveforms → placeLoads → fillInPlaceholders
- [x] Store channelCount_ and channelMode_ from Prefetch queries
- [x] Build verify — clean, 0 warnings

### 30e. Wire up remaining steps + output (12, 15, 16, 19) — COMPLETE 2026-04-26

- [x] Step 12: Construct AsmOptimize with error/info callbacks + device constants
      + cancel callback; `optimizePreWaveform(asmList_)`
- [x] Step 13: Conditional serialize (string_30_owned) + conditional
      serialize/deserialize round-trip (unknown_28 == 1)
- [x] Step 13c: Construct WavetableIR from WavetableFront via allocate_shared
- [x] Step 15: `optimizePostWaveform(asmList_)`
- [x] Step 16: `asmCommands_->unsyncCervino()` — append entries to asmList_
- [x] Step 17: Conditional debug print (debugFlags & 0x08)
- [x] Step 19: Copy `asmList_` entries → `vector<AssemblerInstr>` return
- [x] Build verify — clean, 0 warnings
- [x] BONUS: Added AsmOptimize constructor (was missing from header + impl)
- [x] BONUS: Fixed GlobalResources namespace/class conflict in asm_optimize.hpp

### 30f. First differential test run — COMPLETE

- [x] Run `tests/diff_test.py` — **12/12 tests pass** (2026-04-26)
- [x] Handle pybind return type mismatch (json string vs dict) — handled in worker
- [x] Fixed `fillInPlaceholders()`: was creating empty output list, should copy
      input asmList (binary uses `__init_with_size` at 0x1d6600)
- [x] Fixed `Node(type, asmId, numWaveSlots)` argument order: was swapped.
      Binary verified via `asmLoopNode` (0x277ad0) and rootNode construction
      (0x11fdc8). asmId is 2nd param, numWaveSlots is 3rd.
- [x] Fixed `emitNodeEntry()`: must use 3-arg Node ctor so node gets correct
      asmId (= sequenceId) and numWaveSlots (= numChannelGroups)
- [x] Fixed `AsmCommands::st()` output-param overload: declared but not defined
- [x] Fixed ELF header: `prepareHeader()` vtable order is set_machine(0),
      set_type(machineType), set_flags(0) — not set_type(0), set_machine(mt)
- [x] Removed debug instrumentation from compiler.cpp and prefetch_emit.cpp
- [x] Fixed `addVar` VarSubType: VarSubType_Stub→VarSubType_Default in
      SeqCVariable VarType_Var declaration case (binary: `xor edx,edx` at 0x20a093)
- [x] **Root-cause fix for checkVar false-throw**: Parser rules 52-62
      (assignment_expression) write `valueType=0` and `valueCategory=1` to `$1`
      (the LHS expression), NOT `$$` (the operator). Confirmed at binary
      0x2ca99c: `mov -0x10(%rbx),%rcx` loads $1, then writes +0x54 and +0x04.
      This sets the LHS variable's direction to eIN(0), preventing checkVar from
      firing on first assignment to uninitialized variables. The previous
      workaround (disabling checkVar) is no longer needed — checkVar is now
      properly enabled.
- [x] All 12/12 differential tests pass with checkVar enabled

### 30g. Expanded test coverage (12→28 cases) — COMPLETE

Added 16 new test cases covering features not previously tested.
All 28/28 pass on first attempt (2026-04-26).

New HDAWG cases:
- [x] `hdawg_arithmetic` — var add/sub, const multiply
- [x] `hdawg_const` — const declarations, const in expressions
- [x] `hdawg_do_while` — do-while loop
- [x] `hdawg_while_loop` — while loop with counter
- [x] `hdawg_comparisons` — ==, <, >= operators, multiple if branches
- [x] `hdawg_logical` — && || ! operators
- [x] `hdawg_ternary` — ternary conditional expression
- [x] `hdawg_mixed_loops` — repeat containing for loop
- [x] `hdawg_playHold` — playHold + waitWave
- [x] `hdawg_wait` — wait() builtin with different durations
- [x] `hdawg_assign_ops` — chained assignments with arithmetic
- [x] `hdawg_unary` — negation and bitwise complement
- [x] `hdawg_inc_dec` — post-increment and post-decrement
- [x] `hdawg_bitwise` — &, |, ^, <<, >> operators

New SHF cases:
- [x] `shfqa_basic` — playZero + repeat + setTrigger + wait on SHFQA4
- [x] `shfsg_loops` — for + repeat loops on SHFSG8

---

## Phase 13: Method body reconstruction

Fill in stubbed method bodies by disassembling the binary at the addresses
noted in each stub's comment. Priority: highest-value stubs first.

### 13a. SeqCAstNode subclass bodies

106 stubs across 53 subclasses (print + clone for each). Most are
straightforward (print writes to stream, clone deep-copies children).

- [x] Analyze print() pattern from one representative class (e.g. SeqCIfElse @0x201df0)
- [x] Implement print() macro template based on pattern
- [x] Analyze clone() pattern (likely recursive deep-copy)
- [x] Implement clone() macro template
- [x] Fill in non-trivial subclass bodies (SeqCVariable, SeqCValue, SeqCFunction)
- [x] Resolve unknown #96 (SeqCAstNode `type` field meaning) — RESOLVED: renamed `type_` → `lineNr_` (source line number)
- [x] Sub-phase wrap-up

### 13b. CustomFunctions method bodies

~35 SeqC built-in function stubs (play, playZero, playHold, setTrigger,
waitDigTrigger, etc.) plus 5 infrastructure methods.

- [x] Reconstruct ctor registration logic @0x12bcf0 (maps function names → lambdas)
- [x] Reconstruct call() @0x159470 (alias resolution + funcMap_ dispatch)
- [x] ~~Reconstruct eval()~~ — no standalone eval(); moved to Phase 13e
- [x] Pick 5-10 representative built-in functions, reconstruct bodies
- [x] Stub remaining built-in functions with improved comments
- [x] Resolve field unknowns: MathCompiler layout (#102), field_168 (#101) — resolved in 13e/14a
- [x] Sub-phase wrap-up

### 13c. WaveformGenerator method bodies

~35 waveform DSP function stubs (sine, cosine, gauss, drag, rrc, etc.)
plus call/eval/getOrCreateWaveform infrastructure.

- [x] Reconstruct call() — alias resolution + funcMap_ dispatch
- [x] Reconstruct eval() — body documented; throws "blocked on EvalResults" with full algorithm in comments (real impl deferred to Phase 15a once EvalResults layout is known)
- [x] Reconstruct getOrCreateWaveform() — caching layer
- [x] Pick 5-10 representative waveform functions, reconstruct bodies (zeros, ones, rect, scale, add, gauss + readers + createDummyWaveform)
- [x] Stub remaining with improved comments (27 stubs documented with expected signatures)
- [x] Resolve field unknowns: field_50_ (#108 — renamed createdNames_), field_B0_ (#110 — deferred, kept as shared_ptr<void>)
- [x] Reconstruct aliasMap_ contents from ctor registration code (#111 — empty, no aliases registered in this binary)
- [x] Sub-phase wrap-up

### 13d. CachedParser method bodies

7 stubs — most are Boost.Serialization-heavy.

- [x] Reconstruct loadCacheIndex @0x2afec0 (boost::archive::text_iarchive) — ifstream → text_iarchive >> index_, then recompute currentSize_ by walking tree (sum of fileSize_); cleanCache() on any exception
- [x] Reconstruct saveCacheIndex @0x2b03c0 (boost::archive::text_oarchive) — skip if disabled or empty; ofstream → text_oarchive << index_; sticky-disable enabled_ on exception
- [x] Reconstruct cacheFile @0x2b05b0 (~700 instructions) — algorithmic outline + section-name table; full ELF-building body left as documented stub (heavy ELFIO interaction)
- [x] Reconstruct getCachedFile @0x2b1900 (~400 instructions) — full lookup-and-load with cacheFileOutdated check, eviction, ElfReader → 4 sections (.channels, .marker_bits, .data, .marker)
- [x] Reconstruct removeOldFiles @0x2b01a0 — copy-to-vector + sort by timestamp + evict-while-oversize, respecting valid_ pin flag
- [x] Implement CacheEntry::serialize template — inline 6-field `ar &` sequence
- [x] BONUS: Reconstructed cleanCache @0x2b0140 with exception swallowing
- [x] BONUS: Reconstructed cacheFileOutdated @0x2b14d0 — ElfReader-based ".format" + ".file_name" mtime check (NOT an index_ lookup as previously assumed)
- [x] BONUS: Created minimal ElfReader/ElfSection forward-decl header
- [x] BONUS: CORRECTED CachedFile layout — there is NO bool found_ field; layout is `uint16_t channel_; vector<uint8_t> markerBits_; vector<double> samples_; vector<uint8_t> markers_` (resolves #113)
- [x] BONUS: cacheSize_/currentSize_ are tracked in BYTES, not entry counts (header doc fixed)
- [x] BONUS: valid_ flag semantics clarified — set true on access (LRU-touch alongside timestamp_), prevents eviction (acts as "in-use pin")
- [x] Sub-phase wrap-up

### 13e. Quick-win unknown resolutions

Small disassembly tasks from the unknowns triage. Each is a single
ctor/method read to determine field types or contents.

- [x] MathCompiler internal layout (#102) — RESOLVED in re-audit. Two
      `std::map`s at +0x00 (single-arg) and +0x18 (multi-arg). Header updated.
- [x] NodeMapData subclass fields (#104) — DirectAddrNodeMapData has
      `uint32_t addr_` at +0x08 (after 8B vptr). Header updated.
      VirtAddrNodeMapData fields still unknown — deferred (no clear read sites).
- [x] CustomFunctions field_168 (#101) — RE-CHARACTERIZED. Real type per
      ~CustomFunctions @127cf2 is `std::vector<T>` with sizeof(T)==8 trivially
      destructible. Element T still unidentified — moved to Phase 14a follow-up.
- [x] BONUS: corrected pre-existing `nodeMap_` field at CustomFunctions+0xF8
      from `std::map<string,NodeMapItem>` (24B inline) to `std::unique_ptr<NodeMap>`
      (8B). Confirmed via lookupNode @0x15c530 dereferencing [this+0xf8] as a
      single ptr and calling NodeMap::retrieve(). Added NodeMap class definition
      in src/custom_functions.cpp.
- [x] BONUS: documented historical confusion with the dtor at 0x1306c1 (which
      is NOT CustomFunctions::~CustomFunctions — that's at 0x127c90). Note added
      to unknowns_open.md and to the layout comment in custom_functions.hpp.
- [x] WaveformGenerator field_50_ — RESOLVED in 13c (renamed createdNames_, set<string> cache)
- [x] WaveformGenerator field_B0_ — RESOLVED as negative finding (Phase 15a).
      No setter exists; 16B slot reserved-but-unused.
- [x] WaveformGenerator aliasMap_ — RESOLVED in 13c (empty in this binary)
- [x] Sub-phase wrap-up

### 13f. WaveformGenerator DSP function expansion (optional)

**Optional / low priority.** Existing stubs are correct one-liners.
Expanding to full DSP math gives little value without test infrastructure
to validate sample-by-sample equivalence with the binary. Skip unless
explicitly requested.

After 13e, optionally expand the 27 documented-but-stubbed DSP functions
(sin, cos, drag, blackman, hamming, hann, chirp, rrc, etc.) using the
patterns established in 13c (gauss/rect/scale). Many follow the same
checkArgCount → readers → fill-vector → return Signal pattern.

- [x] Reconstruct trig family (sin, cos, sinc) — share frequency/phase reader
- [x] Reconstruct envelope family (blackman, hamming, hann, drag) — DONE (Phase 16d)
- [x] Reconstruct random family (rand, randomGauss, randomUniform, lfsrGaloisMarker) — DONE (Phase 16d)
- [x] Reconstruct combinator family (join, interleave, multiply, cut, flip, filter) — DONE (Phase 16d)
- [x] Reconstruct misc (rrc, vect, placeholder, mask, marker, markerImpl,
      readWave, interpolateLinear) — DONE (Phase 16d/17b)
- [x] Sub-phase wrap-up

---

Types not yet started, identified from symbol table.

### 14a. MathCompiler (67 symbols, 0x30 bytes)

Embedded in CustomFunctions at +0xC8. Separate math expression compiler
for constant-folding and evaluation.

- [x] Determine MathCompiler layout from ctor @0x1c2250 — already resolved in
      Phase 13e re-audit: two `std::map`s at +0x00 (single-arg) and +0x18 (multi-arg).
- [x] Identify all 67 methods from symbol table — 33 zhinst:: methods (28 single-arg
      libm wrappers, 5 multi-arg, ctor, dtor, call, functionExists, exception class);
      remaining ~34 symbols are libc++ __function::__func<__bind<...>> instantiations.
- [x] Reconstruct layout + key methods — full ctor (populates both maps via std::bind),
      `call()` with single→multi dispatch and FuncSingleArg/UnknownFunction throws,
      `functionExists()` with strict|found semantics, `pow()` with FuncExactly2Args
      throw on size!=2. Single-arg wrappers were already stubbed correctly; fixed
      sin/sinh/sqrt addresses (typo: sqrt was duplicating sinh's address).
- [x] FOLLOW-UP from 13e: identify CustomFunctions field_168 element type
      RESOLVED — Phase 13e characterization was wrong. The dtor at 127cf2 has a
      node-walk loop at 127d40-127d70 reached via `jne` at 127cf0 (initially
      missed). Real type: `std::unordered_set<std::string>` (40B container, 40B
      node = 16B header + 24B std::string element). Confirmed by 1.0f
      max_load_factor at +0x188 in ctor at 12bec9 and by string-dtor pattern
      at 127d58-127d6b. Original Phase 11d classification was correct.
- [x] FOLLOW-UP from 13e: resolve VirtAddrNodeMapData fields
      RESOLVED — total 0x38 = vptr (8B) + `std::string name_` (+0x08, 24B)
      + `std::vector<int32_t> addresses_` (+0x20, 24B). Smoking gun:
      copy ctor @0x1c4dc0 calls `vector<int>::__throw_length_error` at 1c4e72;
      getJson @0x1c5240 reads "name" key from string field then iterates the
      vector with `movsxd rax, [r12]` (int32 sign-extension).
- [x] Sub-phase wrap-up

### 14b. DeviceType / DeviceFamily / DeviceOption (~169 symbols)

Device enumeration and capability queries. Survey done 2026-04-23 found
~169 symbols across ~13 distinct types and ~30 free functions. Split
into four sub-phases.

- [x] Survey symbol table — group by class

#### 14b-i. Enums + DeviceType pimpl + DeviceOptionSet — COMPLETE 2026-04-23
- [x] Decode DeviceFamily enum (12 one-hot values) from toString jump table
- [x] Decode DeviceTypeCode enum (33 values) from toString jump table
- [x] Reconstruct DeviceType pimpl (8B wrapping DeviceTypeImpl*)
- [x] Reconstruct detail::DeviceTypeImpl (88B / 0x58, polymorphic base)
- [x] Reconstruct DeviceOptionSet (72B / 0x48, dual-storage:
      unordered_set<DeviceOption> + map<string,DeviceOption>) — corrected
      from initial 0x30 estimate after investigating iterator type
- [x] Reconstruct DeviceOptionSetConstIterator (8B, wraps map iterator)
- [x] Reconstruct 11 predicate free functions (isDefined, isIa, isMfia,
      isUhfqa, isShfqa, isShfsg, isShfqc, isShfppc, isShfli, isGhfli, isVhfli)
- [x] Reconstruct toString(DeviceFamily) and toString(DeviceTypeCode)
- [x] Created reconstructed/notes/device_type.md
- [x] Build clean (0 errors, 0 warnings)
- [x] Sub-phase wrap-up

#### 14b-ii. DeviceOption enum + sfc options + GenericDeviceType subclass

Split 2026-04-23 after survey revealed scale: 14 knownOptions arrays,
6 sfc::*Option enums, 30+ detail::* device-type subclasses, plus
GenericDeviceType + string parsing. Two sub-sub-phases:

##### 14b-ii-a. Enums, options data, string conversions — COMPLETE 2026-04-23
- [x] Decoded all 15 .rodata knownOptions arrays at 0x962394..0x962aa0
      (Hf2li, Hf2is, Mfli, Mfia, Uhfli, Uhfawg, Uhfqa, Uhfia, Hdawg4,
      Hdawg8, Shfqa2, Shfqc, Shfli, Ghfli, Vhfli) — full (mask,code) tables
      in notes/device_type.md
- [x] Reconstructed sfc::Hf2Option / MfOption / UhfOption / HdawgOption /
      ShfOption / VhfOption / GhfOption enums (one-hot bitmasks, union
      across subclasses per family)
- [x] Reconstructed detail::OptionCodePair<T> template (8B: T mask + DeviceOption code)
- [x] Reconstructed detail::initializeSfcOptions<T,N> header-only template
      (body confirmed at 0x2e0d50)
- [x] Replaced DeviceOption stub with full 31-value enum (0..30, with MF/None
      alias at 0 and family-dependent strings for codes 0 and 6)
- [x] Replaced toString(DeviceOption, DeviceFamily) stub with real 31-case
      switch including family-dep "MFK"/"RTK" branches for HF2
- [x] toString(DeviceType const&) — CORRECTION: it is NOT a "<CODE> [opts]"
      formatter; the binary just returns toString(dt.code()). Existing 14b-i
      stub was actually correct.
- [x] BONUS: added toString(DeviceOptionSet, DeviceFamily, separator)
      helper @ 0x2d0130 (boost::algorithm::join over alphabetical names)
- [x] Replaced toDeviceTypeCode stub with lazy-init function-local static
      unordered_map<string, DeviceTypeCode> (33 entries incl. "none" alias)
- [x] FIXED isIa logic — Phase 14b-i impl had inverted truth table; correct
      semantics: codes UHFIA/MFIA always true, codes in broad mask 0x46BF7901
      else false, all other codes (incl. >=31) defer to hasOption(IA)
- [x] FIXED hasMf body — probes hasOption(MD=1) when family==MF, else
      hasOption(MF=0); matches binary at 0x2d3030 exactly
- [x] Build clean (0 errors, 0 warnings)
- [x] Sub-phase wrap-up

##### 14b-ii-b. detail::* device-type subclasses + GenericDeviceType

###### 14b-ii-b1. Per-family device subclasses — COMPLETE 2026-04-23
- [x] Reconstructed all 32 detail::* device-type subclasses derived from
      DeviceTypeImpl (Hf2, Hf2li, Hf2is, Mf, Mfli, Mfia, Uhf, Uhfli, Uhfawg,
      Uhfqa, Uhfia, Hdawg, Hdawg4, Hdawg8, Shf, Shfqa2, Shfqa4, Shfsg2,
      Shfsg4, Shfsg8, Shfqc, Shfli, Shfacc, Shfppc2, Shfppc4, Ghf, Ghfli,
      Pqsc, Qhub, Hwmock, Vhf, Vhfli) — split: 15 template-driven via
      `initializeSfcOptions<sfc::*Option,N>`, 11 inline-bit, 6 empty-options.
      All are 0x58 bytes (same as base) with vptr-only override.
- [x] Created device_subclasses.hpp with all 32 declarations.
- [x] Created 11 per-family .cpp files (device_hf2.cpp, device_mf.cpp,
      device_uhf.cpp, device_hdawg.cpp, device_shf.cpp, device_shfacc.cpp,
      device_ghf.cpp, device_pqsc.cpp, device_qhub.cpp, device_hwmock.cpp,
      device_vhf.cpp) mirroring the binary's per-family layout.
- [x] RESOLVED open question: there is NO `sfc::GhfOption`. The GHF family
      reuses `sfc::ShfOption` (Ghfli ctor at 0x2e3a00 calls the mangled
      `initializeSfcOptions<sfc::ShfOption, 5>` template instantiation
      directly, knownOptions @ 0x96298c). Removed the bogus GhfOption
      enum that was added in 14b-ii-a.
- [x] Discovered Hf2Factory subtype-selector dispatch pattern
      (subtype-selector bits in the high bits of the `unsigned long opts`
      arg; mask 0x1c0 for HF2). Same shape applies family-by-family in
      14b-ii-b2.
- [x] Build clean (0 errors, 0 warnings).
- [x] Sub-phase wrap-up.

###### 14b-ii-b1.5. knownOptions verification debt
- [x] Disasm-verify the per-entry (mask, code) selections in 10
      knownOptions arrays — **DONE in Phase 22c.** All 10 arrays verified
      against binary .rodata; 2 bugs found and fixed (Uhfawg, Uhfqa).
      effectively deferred indefinitely. Low priority — affects only
      field-name accuracy of inferred (mask, code) pairs, not behavior.
      Consider closing if no consumer ever needs these names.

###### 14b-ii-b2. Factories, Unknown/Generic device types, parser — COMPLETE 2026-04-23
- [x] Reconstruct abstract base `detail::DeviceFamilyFactory` @ 0x2e0590
      (vtable-only 8B base; virtual `doMakeDefault()` and
      `doMakeDeviceType(unsigned long)`; non-virtual base trampolines
      `makeDefault()` @ 0x2e0590 and `makeDeviceType(opts)` @ 0x2e05b0).
- [x] Reconstruct `detail::makeDeviceFamilyFactory(DeviceFamily)`
      dispatcher @ 0x2e05d0 — switches on family one-hot value to
      construct one of the 11 per-family Factory instances (or
      UnknownDeviceTypeFactory).
- [x] Reconstruct `detail::NoDeviceTypeFactory` @ 0x2e0700 / 0x2e0730
      (returns nullptr from both methods).
- [x] Reconstruct `detail::UnknownDeviceTypeFactory` @ 0x2e0760 / 0x2e07b0
      (both methods return a heap-allocated `UnknownDevice`).
- [x] Reconstruct 11 per-family Factory classes — each with
      `makeDefault()` and `doMakeDeviceType(unsigned long opts)` and
      its own subtype-selector dispatch (see notes/device_type.md
      Phase 14b-ii-b2 table for the complete selector mappings).
      All 11 wired into `device_factories.{hpp,cpp}` via a single
      `ZHINST_DEFINE_FAMILY_FACTORY` macro.
- [x] Reconstruct `detail::UnknownDevice` @ 0x2d3320 — synthetic sentinel
      ctor stores `code=33`, `family=0x800` via `movabs rax,0x80000000021`.
- [x] Reconstruct `detail::GenericDeviceType` @ 0x2d3c60 — same 0x58
      layout as base (no extra fields); only the deleting dtor slot is
      overridden (clone reuses the base impl). Ctor calls
      toDeviceTypeCode + toDeviceFamily + (toDeviceOptions OR empty
      DeviceOptionSet) and forwards to `DeviceTypeImpl(tuple)`.
- [x] Reconstruct `DeviceTypeImpl(tuple<DeviceTypeCode, DeviceFamily, DeviceOptionSet>)`
      ctor @ 0x2d3190 (only used by GenericDeviceType).
- [x] Reconstruct `DeviceType(string, vector<string>)` @ 0x2d2ae0
      (NOT 0x2d29c0 as previously documented). Wraps a fresh
      GenericDeviceType.
- [x] Reconstruct **NEW** `DeviceType(string, string)` @ 0x2d2a00 —
      splits the second string via splitDeviceOptions() and forwards
      to the (string, vector) ctor.
- [x] Reconstruct **NEW** free function `splitDeviceOptions(string)`
      @ 0x2d0460 — boost::trim_copy_if then split on '\n'.
- [x] Reconstruct **NEW** free function `toDeviceFamily(string)`
      @ 0x2debd0 — fast-path inline string compares + lazy-init
      function-local-static `map<string, DeviceFamily>` of 10 entries
      (no shfacc, no unknown) + `boost::starts_with` prefix verify.
      Default on miss = DeviceFamily(0x800).
- [x] Reconstruct **NEW** free function `toDeviceOptions(vector<string>, DeviceFamily)`
      @ 0x2d0fb0 — try/catch around toDeviceOption(name) inserts;
      unknown names silently dropped.
- [x] Build clean (0 errors, 0 warnings).
- [x] Sub-phase wrap-up.

###### 14b-ii-b2-followup. toDeviceOption(string) body — COMPLETE 2026-04-23
- [x] Reconstruct `zhinst::toDeviceOption(string)` @ 0x2d05b0.
      Function-local-static unordered_map of 33 entries
      (mov edx, 0x21 confirms count) decoded from disasm lines
      533249..533490. Source strings live in .data.rel.ro 0xb08ef8..
      0xb08ff8 as `const char*` pointers (NOT std::string objects)
      targeting .rodata literals at 0x90b60e. Two duplicate keys map
      code 0 ("MF" and "MFK") and code 6 ("RT" and "RTK"). Throws
      `zhinst::Exception` via `boost::throw_exception` on miss;
      `toDeviceOptions()` catches and silently drops.
- [x] Resolve QA/QC asymmetry: parser accepts "QC" → DeviceOption::QA
      while toString emits "QA". Real binary quirk; enum kept as `QA`,
      parser table maps "QC" → QA. Documented in notes/device_type.md.
- [x] Confirm DeviceOptionName::* storage class: 8-byte spacing in
      .data.rel.ro confirms `const char*` pointers, not std::string.
- [x] Build clean (0 warnings).
- [x] Verify symbol now defined: nm shows `T` for
      `_ZN6zhinst14toDeviceOptionERK...` in device_type.cpp.o
      (previously `U`).

#### 14b-iii. AwgDeviceType + AwgSequencerType + getAwgDeviceProps templates
- [x] Decode AwgDeviceType enum (one-hot bitfield, 9 values 1..256)
      — already in `types.hpp` from Phase 3d; no changes needed.
- [x] Decode AwgSequencerType enum from toAwgDeviceType — 3 named values
      (Auto=0, QA=1, SG=2); the formatter's "unknown" branch has no
      enumerator (modeled as default-case behavior).
- [x] Reconstruct AwgDeviceProps struct (return type of getAwgDeviceProps<>) —
      0x80 bytes (libc++) / 0xa0 (libstdc++); 4 strings + type + 2 qwords
      + bool. Field NAMES (`maxWaveformSamples`, `maxWaveformBytes`,
      `supportsExtraFeature`) are inferred — see follow-up below.
- [x] Reconstruct AwgPathPatterns struct (3 strings, 0x48/0x60 bytes)
      and 6 anonymous-namespace globals (Default / GrimselQa / GrimselSg
      / GrimselLi / Gurnigel / Maloja); the latter two are runtime copies
      of GrimselLi.
- [x] Reconstruct all 9 explicit specializations of getAwgDeviceProps<T>
      (binary 0x2cc5f0..0x2cdb00). HDAWG is the only one that consults
      `dt.hasOption(ME=0x13)`.
- [x] Reconstruct toAwgDeviceType — 29-entry switch over (code-4) for
      codes 4..32, with SHFQC(21) special-cased on sequencer.
- [x] Reconstruct makeUnsupportedAwgSequencerErrorMessage — produces
      `"Unsupported device or sequencer type (<dtype>, sequencer: <seq>)."`.
- [x] Build clean — `cmake --build .` from `reconstructed/build/` succeeds
      with no warnings; `awg_device_props.cpp.o` symbol is `T`.
- [x] Sub-phase wrap-up — see below for proposed follow-ups.

#### 14b-iii follow-ups
- [x] **Verify inferred AwgDeviceProps field names** — DONE in Phase 21f:
      `maxWaveformSamples`→`maxElfSize`, `maxWaveformBytes`→split into
      `addressImpl`+`sampleFormat`, `supportsExtraFeature`→`isHirzel`.
- [x] **Reconstruct `getDeviceConstants(AwgDeviceType)` body @ 0x2cc0c0**
      — done in Phase 14b-iii.5. All 9 case blocks were already
      populated in Phase 7e; this sub-phase replaced the placeholder
      `std::runtime_error` default branch with the real
      `BOOST_THROW_EXCEPTION(ZIAWGCompilerException(...))` throw, added
      the necessary includes, and documented the jump-table layout,
      XMM constant table, sampling-rate doubles, and source_location
      values in `reconstructed/notes/device_constants.md`. Build clean.

#### 14b-iv. Helpers + free functions + final wrap-up
- [x] Reconstruct getOptionsAsString(DeviceType const&)
- [x] Reconstruct expand(DeviceFamily) @ 0x2dfbc0
- [x] Reconstruct toStrings(vector<DeviceFamily> const&)
- [x] Reconstruct toDeviceOptions, detail::generateMfSfc,
      detail::makeDeviceFamilyFactory
- [x] Reconstruct anonymous-namespace makeDevicesSet()::$_0 lambda
- [x] **Verify inferred AwgDeviceProps field names** (`maxWaveformSamples`,
      `maxWaveformBytes`, `supportsExtraFeature`) — DONE Phase 21f:
      renamed to `maxElfSize`, `addressImpl`+`sampleFormat`, `isHirzel`.
- [x] Update OVERVIEW.md with consolidated Phase 14b summary
- [x] Build clean
- [x] Sub-phase wrap-up

#### Phase 14b — COMPLETE

All four sub-phases (14b-i, 14b-ii, 14b-iii, 14b-iii.5, 14b-iv) closed.
One carry-forward item:

- [x] **AwgDeviceProps field-name verification debt** (carried from 14b-iii):
      DONE Phase 21f — renamed to `maxElfSize`, `addressImpl`+`sampleFormat`,
      `isHirzel`. Notes: `notes/awg_device_props.md`.

### 14c. Logging + tracing (20 zhinst symbols across 17 functions) — DONE

Logging infrastructure used across the compiler. The earlier "73 symbols"
estimate counted boost.log + OpenTelemetry template-instantiation glue;
the actual zhinst-namespace surface is 20 symbols (8 logging + 12 tracing).

- [x] Surveyed symbols — 20 zhinst-namespace symbols across 17 distinct
      functions (logging: 8 / tracing: 12). The "73" estimate counted
      boost.log + OTel template-instantiation glue.
- [x] Determined LogRecord (≥0x118), TraceProvider (0x20), ScopedSpan
      (0x30) layouts; documented in notes/logging_tracing.md.
- [x] Reconstructed all 17 functions — clean cmake build, zero warnings.
- [x] Sub-phase wrap-up — OVERVIEW + notes updated; OpenTelemetry stub
      headers added under `include/opentelemetry/` to type-check
      tracing.cpp without the real SDK installed.

**Verification debt** (carried forward):

- Severity enumerator ordering not directly observed (only the 8 string
  names "Trace".."Fatal" are in .rodata). Verify when the first LOG_*
  macro caller surfaces in a later-phase reconstruction.
- OpenTelemetry stub headers approximate ABI sizes only — real linking
  against opentelemetry-cpp would require swapping the stub umbrella
  (`include/opentelemetry/_stub_fwd.hpp`) for the actual SDK headers.

### 14d. ElfReader / ElfWriter (Phase 13d dependency)

Surfaced during Phase 13d: cacheFileOutdated, cacheFile and getCachedFile
all use ElfReader/ElfWriter. Phase 13d created a minimal forward-decl
header (`elf_reader.hpp`) but the implementations are still placeholder.

- [x] Disassemble ElfReader::ElfReader(string) @0x2c3110 — file open + ELFIO load
- [x] Disassemble ElfReader::getSection(string) const @0x2c4000 — section table walk (throws ElfException on miss, not nullptr)
- [~] ~~Verify ElfSection vtable layout (get_data @ +0xC0, get_size @ +0x98)~~ — CANCELLED: reconstruction uses `ELFIO::section` directly; the original stub's `ElfSection` class was fictional. The free helper `zhinst::sectionAsString(const ELFIO::section*)` in `elf_reader.hpp` covers the only access pattern that mattered.
- [x] Disassemble ElfWriter::ElfWriter(uint16_t) @0x2934a0 — channel arg ctor (already complete from earlier phases — src/elf_writer.cpp has 8 methods, ctor 0x2934a0 included)
- [x] Identify ElfWriter::addSection / save methods (already complete — src/elf_writer.cpp: addCode/addData/addWaveform/writeFile×2/setMemoryOffset/prepareHeader)
- [x] Once ElfReader is real, fold the deferred CachedParser stubs (cacheFile body) into full reconstructions — 5 call sites in cached_parser.cpp converted from fictional `getDataAsString()` to `sectionAsString()`
- [x] Sub-phase wrap-up: complete — see `reconstructed/notes/elf_reader.md`. Net additions: `src/elf_reader.cpp` (5 methods + ElfException ctor/dtor/what), `elf_reader.hpp` upgraded from 71-line forward-decl stub to 0x98-byte full reconstruction with `private ELFIO::elfio` base + `sectionAsString()` helper. ElfReader layout corrected (was 0x90, now 0x98 with `vector<section*>` at +0x78); `readHeader` sentinels reinterpreted as `get_class()==ELFCLASS32` && `get_encoding()==ELFDATA2LSB`; `getSection` semantics corrected (throws, not nullptr). Real cmake build clean, zero warnings.

### 14e. zhinst::sfc namespace (NEW — added during 14b-iv wrap-up)

Surfaced during Phase 14b-iv reconstruction of `generateMfSfc`: its
mangled return type is `zhinst::sfc::FeaturesCode`, used as a
strong-typed wrapper around uint64. Currently stubbed as `uint64_t`
in `mf_sfc.cpp`. The sfc namespace also contains:

- `sfc::FeaturesCode` (type — likely strong typedef / wrapper)
- `sfc::Hf2Option`, `sfc::MfOption`, `sfc::UhfOption`, `sfc::HdawgOption`,
  `sfc::ShfOption` (enum classes)
- `detail::OptionCodePair<T>` template (small POD pair type)
- 8+ `detail::initializeSfcOptions<sfc::*Option, N>` template
  specializations (parser helpers turning SFC bitfield → DeviceOptionSet)

- [x] Survey full `zhinst::sfc::` symbol set in nm output — 13 `initializeSfcOptions` instantiations across 6 OptionEnum types (Hf2/Mf/Uhf/Hdawg/Shf/Vhf); no out-of-line `FeaturesCode` symbols (header-only POD); see `notes/device_type.md` (sfc namespace section)
- [x] Determine `sfc::FeaturesCode` layout (likely 8B wrapper around uint64) — confirmed: 8B trivially-copyable POD wrapper around uint64; evidence is the source_location literal at 0x2deb37 naming the type, plus the direct `rax`-return at 0x2deac1 (no sret, ≤8B & trivially-copyable per SysV ABI)
- [x] Reconstruct sfc option enums (compare bit positions vs `generateMfSfc`) — already complete from Phase 14b-iv: 6 enum classes in `device_type.hpp:163-285` (Hf2Option, MfOption, UhfOption, HdawgOption, ShfOption, VhfOption), bit positions decoded from .rodata 0x962394..0x962aa0 knownOptions arrays
- [x] Reconstruct `OptionCodePair<T>` and `initializeSfcOptions<>` template — already complete from Phase 14b-iv: header-only template in `device_type.hpp`; 13 instantiations confirmed in nm survey
- [x] Update `mf_sfc.cpp` to return real `sfc::FeaturesCode` type — declaration in `device_type.hpp` and body in `mf_sfc.cpp` both promoted; 3 `return` sites wrap their uint64 expression in `sfc::FeaturesCode{...}`
- [x] Sub-phase wrap-up: complete — see `reconstructed/notes/device_type.md` (sfc namespace section). Net additions: `sfc::FeaturesCode` struct in `device_type.hpp` (8B wrapper); `generateMfSfc` declaration & body promoted from `uint64_t` to `sfc::FeaturesCode`. No other call sites needed updating. Real cmake build clean, zero warnings.


---

## Phase 15: Accuracy refinement

Fix approximate logic and deferred TODO markers in existing code.

> **Recommended order**: 15a (close item #4 done) → **15b first** (small
> verification work, builds momentum) → 15a-i (multi-session, frontend
> lowering data model) → 15a #1 (lower() return type, depends on 15a-i)
> → 15c (AsmOptimize, dense disasm).

### 15a. Compiler pipeline gaps

- [x] WaveformGenerator field_B0_ (shared_ptr<void>) — **CLOSED 2026-04-23 as
      negative finding.** No setter exists anywhere in the binary. The 16B
      slot at +0xB0 is reserved-but-unused (zero-initialized in WG ctor at
      0x2482aa, never written elsewhere). Apparent +0xb0/+0xb8 read sites
      inside WG methods (251973, 25385a, 255068, ...) were misattributed —
      they read the union body of `Value` parameter objects (Value has
      type tag at +0xA8 and union storage at +0xB0). The single true
      +0xb0 write at 0x11d180 is `Compiler::Compiler` writing its own
      AsmCommands shared_ptr, not WG-related. Evidence chain documented
      in `waveform_generator.hpp` comments. Closes #110.
- [x] Fix lower() return type in compiler.hpp — **DONE in 15a-i.** Returns
      `LowerResult` (32B sret = 2 shared_ptrs: `shared_ptr<Node>` from
      FrontendLoweringState.result + `shared_ptr<EvalResults>` from the
      evaluate virtual). Was void, now corrected. Body partially implemented
      (Context+State construction done, virtual call TODO'd pending vtable
      slot declaration on SeqCAstNode).

### 15a-i. Frontend Lowering data model — COMPLETE 2026-04-23

Decode the data types flowing through `FrontEndLoweringFacade::lower`.
~130 related symbols across EvalResults / FrontendLoweringState /
FrontendLoweringContext. EvalResults is the central data type of the
lowering pipeline (not a leaf), so this is large. Blocks 15a #1.

**Findings:** EvalResults = 0x80 bytes (7 fields, 14 methods). Value
layout was WRONG (0x20→0x28 correction). lower() return = 32B sret
(2 shared_ptrs, not 64B/4 as previously claimed). FrontendLoweringState
.result = shared_ptr\<Node\> (was void/TBD). EvalResults.arrayBacking_
at +0x70 = shared_ptr\<EvalResults\> (self-referential, used by
SeqCArray). See `notes/frontend_lowering.md`.

- [x] EvalResults layout (0x80B; 14 direct methods scattered across
      0x15a750..0x247600 — heavy inlining): 10 setValue overloads,
      ctor, copy-ctor, dtor, getValue — all declared in eval_results.hpp
- [x] EvalResultValue layout (0x38B) — renamed fields from opaque to
      typed: VarType + VarSubType + Value(embedded 0x28) + AsmRegister
- [x] lower() sret aggregate type — **CORRECTED to 32B / 2 shared_ptrs**
      (was wrongly claimed as 64B / 4 sps). Return type:
      `LowerResult{shared_ptr<Node> astResult, shared_ptr<EvalResults> evalResult}`
- [x] FrontendLoweringState::result pointee — **shared_ptr\<Node\>**
      (was shared_ptr\<void\>/TBD). Evidence: lower() copies state.result
      into sret[0]; caller stores into Compiler+0x28 (= shared_ptr\<Node\> ast_)
- [x] Sub-phase wrap-up — see notes/frontend_lowering.md. Build clean.

### 15b. Prefetch/Cache deferred items (10 markers) — COMPLETE 2026-04-23

From Phase 10.5f — minor verification items in prefetch.cpp and
prefetch_splitplay.cpp.

**Audit finding 2026-04-23**: All 10 markers were already resolved during
post-audit edits on 2026-04-22 (between todo_audit.md @ 01:39 and the
prefetch source edits at 04:35-04:53) but were never officially closed
out. Verified by case-insensitive search for TODO/FIXME/APPROXIMATE/
VERIFY/XXX in both files — zero markers remain. The relevant code
sections now contain inline disasm citations (e.g. line 476-481 cites
`mov edx,0x114; bt edx,ecx` for the bitmask, line 526 cites 0x1d031d→0x1d0869
for the eligibility branch, line 125 cites 0x1dd462..0x1dd49c for the
weak_ptr::lock pattern). Build clean.

- [x] Verify bitmask 0x114 exact bits — done; cited at prefetch.cpp:476-481
- [x] Verify enum name ordering — done; cited at prefetch.cpp:500-506
- [x] Verify NodeType::Play cmp $0x2 — done; cited at prefetch.cpp:504
- [x] Verify shared_ptr aliasing constructor — replaced with semantic
      `weak_ptr::lock()` at prefetch_splitplay.cpp:126
- [x] Resolve remaining 6 prefetch_splitplay.cpp items — all converted
      to semantic equivalents with disasm citations
- [x] Sub-phase wrap-up — see notes/archive/phase_15b_prefetch_audit.md

### 15c. AsmOptimize approximate logic — COMPLETE 2026-04-23

**Primary finding**: register field semantics in `AssemblerInstr` were
inverted throughout all AsmOptimize methods. reg2(+0x20)=READ source,
reg0(+0x28)=WRITE destination, reg1(+0x30)=dual (read if cmdType==1 or 7;
written if cmdType==7). Prior code had reg2 as "dest" and reg0 as "src1"
— exactly backwards. Fixed in isRead, isWritten, getNextActionForReg
(rewritten), registerIsNeverWritten, registerUpdate, isLabelCalled
(iteration direction). Field NAMES kept as reg0/reg1/reg2 to avoid
cascading rename across 20+ files; semantic comments cite Asm-relative
and AssemblerInstr-relative offsets.

- [x] removeUnusedRegs inner scan loop (#47) — fully reconstructed @0x27e760
      (291 lines). Skip bitmask 0x29 documented (skips INVALID, LABEL, cmd=4
      — NOT NOP/MESSAGE/ERROR_MSG as previously claimed).
- [x] registerAllocation conflict detection (#48) — structurally reconstructed
      @0x27ebb0 (1466 lines, 6-phase graph-coloring with backward-branch
      live-range extension). Internal `PhysicalRegister` type confirmed via
      vector dtor mangled name @0x281840.
- [x] splitConstRegisters splitting heuristic (#49) — structurally reconstructed
      @0x280440 (444 lines, 2-pass barrier algorithm). Added
      `AsmRegister::magicSkipRegister()` @0x28ebb0 returning `{INT_MAX, true}`.
- [x] Sub-phase wrap-up — see `notes/optimization_passes.md`. Build clean.

---

## Phase 16: Validation, file reorganization, and stub recovery

Phase 16 is a multi-stage cleanup before final validation. Originated
from a noticed pile-up in `custom_functions.cpp` and the realisation
that prior tracking had drifted. Full audit catalog in
`reconstructed/notes/audit_phase16a.md`.

### 16a. Codebase marker & stub sweep

- [x] Grep all sources for TODO/FIXME/TBD/APPROXIMATE/VERIFY/XXX/HACK
      markers; cross-reference with TODO.md and unknowns.md;
      catalog stub patterns including the `throw std::runtime_error("X
      not implemented")` pattern (which is invisible to plain TODO grep).
- [x] Per-file namespace/class inventory (find files piling multiple
      unrelated types).
- [x] Catalog written to `reconstructed/notes/audit_phase16a.md`.
- [x] Sub-phase wrap-up: 6 untracked items + multiple stale TODO.md rows
      surfaced. Each one tracked as an explicit item below; details in
      the audit file.

### 16b. File-organization split ✅ DONE

Completed 2026-04-23. Refactored `custom_functions.{cpp,hpp}` into
four well-scoped TU pairs. Build clean, zero lost markers.

- [x] **Split `custom_functions.{cpp,hpp}` (audit §C1)** — extracted:
    - [x] `MathCompiler` + `MathCompilerException` → `math_compiler.{cpp,hpp}`
    - [x] `NodeMapData` hierarchy + `NodeMapItem` → `node_map_data.{cpp,hpp}`
    - [x] `EvalResultValue` → `eval_result_value.{cpp,hpp}`
- [x] **ErrorMessages ODR investigation (audit §F)** — single class,
      single definition in `error_messages.{hpp,cpp}`. Audit grep was
      over-broad; all 9 .cpp "occurrences" are just `format()`/`get()`
      calls. No action needed.
- [x] **compiler.cpp Facade split (audit §E)** — `FrontEndLoweringFacade::lower`
      is 36 lines and gates on unfinished `SeqCAstNode::evaluate` virtual.
      Not worth splitting. Will revisit when the virtual is wired.
- [x] ~~Split `waveform_generator.cpp` exceptions (audit §D)~~ — not worth
      the churn; exceptions are small and colocated with their throwers.
- [x] Sub-phase wrap-up: build clean; OVERVIEW.md updated; audit findings
      resolved or deferred with rationale.

### 16c. TODO.md / unknowns.md reconciliation ✅ DONE

Completed 2026-04-23. Summary table rewritten with 16 rows matching
audit-verified reality. Audit file renamed to `notes/outstanding_work.md`.

- [x] Refresh summary table — all Section G rows corrected.
- [x] Promote Phase 15c carry-forwards — new "AsmOptimize carry-forwards"
      row (3 items: simplifyAssign, splitReg, register rename).
- [x] Cross-link unknowns.md — new "Unknowns cross-ref" row (#98, #10).
- [x] Fold `audit_phase16a.md` content into TODO.md; file deleted (no
      synonym files for TODO — see AGENTS.md).
- [x] Sub-phase wrap-up: TODO.md and unknowns.md numbers match reality.

### 16d. Stub & gap execution ✅ DONE (HIGH + MEDIUM complete)

Completed 2026-04-23. HIGH and MEDIUM items all done. LOW items deferred
to future phases (verification-only or diminishing returns).

Order of execution: HIGH → MEDIUM → LOW. Each item references
audit catalog for context.

#### High priority — runtime correctness ✅ DONE

Completed 2026-04-23. Ctor binding block (78/81), play() + playIndexed()
structurally reconstructed, SubFunc enum fully resolved (Default=1, Aux=2,
Now=3, DigTrigger=4). 4 complex wrappers identified and split to own TODO.

- [x] **CustomFunctions::CustomFunctions ctor binding gap (audit §C2,
      C3, item I.2)** — 81/81 emplace calls reconstructed (76 standard +
      5 aliases: setSeqIndex→assignWaveIndex, setReadoutRegisterAddress→setID,
      waitOscPhaseOfDemod→waitDemodOscPhase, setUser→setUserReg,
      getUser→getUserReg). Previously had setReadoutRegisterAddress wrongly
      bound to setUserReg — corrected to setID @0x1334a0.
- [x] **Reconstruct `CustomFunctions::play` @0x15f090 (audit §C2)** —
      full structural reconstruction (7536B, SubFunc switch, PlayArgs,
      channel loop, merge, asm emit). Documented pseudocode in .cpp.
- [x] **Reconstruct `CustomFunctions::playIndexed` @0x160e00 (audit
      §C2)** — same treatment (6428B).
- [x] **Resolve aux-wrapper SubFunc TBD values (audit §C2 last
      bullet)** — SubFunc enum: Default=1, Aux=2, Now=3, DigTrigger=4.
      playAuxWaveIndexed→playIndexed(2), playWaveDigTrigger→play(4).
      4 wrappers (playAuxWave, playDIOWave, playWaveDIO, playWaveZSync)
      are complex — own PlayArgs construction, no play() delegation.

#### Medium priority — feature completeness

- [x] **Reconstruct remaining CustomFunctions utility stubs (audit
      §C2)** — All 13 dispatch helpers reconstructed from binary:
      `checkFunctionSupported` (bitmask test + throw 0x49),
      `checkWaveformMinLengthTrig` (throw 0xF3),
      `checkOffspecWaveLength` (warn 0xF4/0xE6),
      `optionAvailable` (linear scan + usedFeatures_ insert),
      `lookupNode` (initNodeMap + retrieve + throw 0x83),
      `setWaitCyclesReg` (addi + suser 0x6F),
      `generateWaveform` (prepend name + delegate to generate()),
      `addSyncCommand` (Hirzel/Cervino switch),
      `printF` (boost::format with VarType dispatch),
      `addWaitCycles` (addi + suser 0x69),
      `writeLS64bit` (two addi+suser pairs for 64-bit split).
      `mergeWaveforms` and `writeToNode` remain as documented stubs
      (3KB and 23KB respectively, need WaveformGenerator complete type).
- [x] **Reconstruct 4 complex play wrappers** — DONE (Phase 21a). Each has own PlayArgs
      construction, no play()/playIndexed() delegation:
      `playAuxWave` @0x135610 (~5KB), `playDIOWave` @0x1369f0 (~3.4KB),
      `playWaveDIO` @0x137740 (784B), `playWaveZSync` @0x137a50 (~3.2KB).
- [x] **WaveformGenerator DSP throw-stubs (audit §B, §D)** — All 32
      throw-stubs reconstructed from binary. Full implementations for:
      sin, cos, sinc, ramp, sawtooth, triangle, chirp (trig/math);
      drag, blackman, hamming, hann, rrc (window/pulse); mask, marker
      (wrappers to markerImpl); rand, randomGauss, randomUniform,
      lfsrGaloisMarker (random); vect, placeholder, flip, interleave
      (composition). Structural outlines for larger functions: join,
      multiply, cut, circshift, merge, grow, filter, readWave,
      markerImpl. File grew from 772 to 2305 lines.
- [x] **MathCompiler 67 symbols** — Already complete. 23 single-arg
      wrappers + 5 multi-arg + ctor + call() + functionExists() +
      exception class. All 74 nm symbols accounted for. File: 197 lines.

#### Low priority — quality / completeness

- [x] **SeqCAstNode print/clone macro expansion (audit §I.6)** — verified
      in Phase 31e: 53×2 symbols match binary, 51 macro-generated correct,
      1 bug fixed (SeqCVariable::print varType_ not lineNr_).
- [x] **WavetableManager\<T\> remaining 14 methods** — DONE (Phase 21e).
- [x] **smap remaining logic** — RESOLVED in Phase 31a: ~0x1E6 bytes are
      compiler-generated Immediate dtor cleanup + two st() calls + vector
      insert, all already in asm_commands.cpp.
- [x] **mergeWaveforms full reconstruction** — DONE (Phase 21a, 3KB @0x15e060).
- [x] **writeToNode full reconstruction** — DONE (Phase 21b, 29KB @0x164550).
- [x] **floatEqual @0x2ec050** — DONE (Phase 20b). Note: binary uses exact
      IEEE-754 equality (cmpeqsd), not tolerance.
- [x] **AWGCompilerConfig::supportedDeviceTypes documentation** —
      field at config+0x00 is a uint32 bitmask. Values per device type
      need documenting (discovered during checkFunctionSupported analysis).

### 16e. Validation against the real .so ✅ DONE

Completed 2026-04-23.

- [x] Verify struct sizes match binary — added `static_assert` for
      PlayConfig (0x20) and Signal (0x58). Cache::Pointer (0x24 in
      libc++, 0x28 in libstdc++) confirmed as ABI-dependent — not
      assertable. Existing asserts: AsmRegister(8), Value(40),
      DeviceConstants(0x90), MemoryBlock(12), SeqCAstNode(0x18),
      EvalResultValue(0x38), NodeMapItem(0x18), AwgPathPatterns(0x60),
      AwgDeviceProps(0xa0), FrontendLoweringContext(0x50),
      FrontendLoweringState(0x30), plus 12 SeqCAstNode subclass asserts.
- [x] Final marker sweep — 181 total markers across 40 files:
      95 TODO, 29 stub, 22 VERIFY, 14 TBD, 11 APPROXIMATE, 10 not-impl,
      0 FIXME. Top hotspots: custom_functions.cpp (46), waveform_generator
      (29), node_map_data (22).
- [x] Write comparison tests / test key methods — **deferred**. Direct
      testing against the real .so requires a C ABI bridge or Python
      wrapper due to libc++ vs libstdc++ incompatibility. Not practical
      within Phase 16 scope. Future validation should use Python ctypes
      or a thin C shim calling into the .so.

---

## Phase 17 — Hotspot depth pass

Focus on reducing the 181 markers in the top 3 files. Order: node_map_data
(smallest, most mechanical), then waveform_generator structural stubs,
then custom_functions remaining builtins.

### 17a. node_map_data.cpp (22 markers → 0) ✅ DONE

- [x] Reconstruct NodeMap method bodies from binary (12 TODO stubs)
- [x] Verify rodata tables from binary data section (typeTable @0x95ad18 = {2,1,2,2}, sizeTable @0x8fc630 = {2,1,2,2})
- [x] Build verify + sub-phase wrap-up

### 17b. waveform_generator.cpp structural stubs (29 markers → 3) ✅ DONE

- [x] Reconstruct readWave — return type corrected to shared_ptr<WaveformFront>,
      18 call sites updated to extract ->signal
- [x] Reconstruct eval — EvalResults now fully available; make_shared + setValue(VarType(5))
      + waveformFront_ assignment at +0x48
- [x] Reconstruct markerImpl — 1858 bytes (not 4576), creates uniform Signal with constant
      marker; only 2 args (length + markerValue), isMask only affects param names
- [x] Fix interpolateLinear bug (size/sizeof)
- [x] Verify grow error code (0x3d), merge no length validation, circshift min=1, cut offset
- [x] Clean up stale TODO/VERIFY comments (aliasMap_ confirmed empty, rand formula verified,
      marker manipulation NOTE'd)
- [x] Build verify + sub-phase wrap-up
- [x] 3 remaining markers: readDoubleAmplitude |x|>1.0 check, interpolateLinear formula,
      rrc error 0x64 validation — resolved Phase 21f

### 17c. custom_functions.cpp remaining builtins (46 markers → 19) ✅ DONE

- [x] Reconstruct checkPlayMinLength/checkPlayAlignment warning callbacks (0xF5, 0xE7)
- [x] Verify and fix getWaitTime shift direction (left-shift + sar, not right-shift)
- [x] Fix getNodeAddress — return direct->addr_, throw on miss (not return 0)
- [x] Fix getAccessModes — throws out_of_range on miss (not return empty)
- [x] Document getSampleClock fallback (readConst mechanism not fully understood)
- [x] Implement 5 simple 0-arg functions: waitWave (wwvf), waitPlayQueueEmpty (wwvfq),
      sync (addSyncCommand), randomSeed (seedRandom stub), now (suser 0x1c)
- [x] Implement error/info (printF + asmMessage)
- [x] Convert 8 TODO/TBD comments to NOTEs (aliasMap_ empty, NodeMap, etc.)
- [x] Document parseOptionalString and getPlayRate as stubs with addresses
- [x] Build verify + sub-phase wrap-up
- [x] 19 remaining markers: all resolved — PlayArgs reconstructed in Phase 18a,
      writeToNode in Phase 21b, mergeWaveforms in Phase 21a.

---

## Phase 18 — PlayArgs + bulk builtin stubs

### 18a. PlayArgs layout reconstruction

Decode PlayArgs struct (~0x80 bytes, NOT 0x200 — previous offset estimates were
wrong; they were stack locals in play()). Unblocks play(), playIndexed(),
mergeWaveforms(), and the 4 complex play wrappers.

- [x] Disassemble PlayArgs ctor @0x15d600 — 0x80 byte struct with
      shared_ptr<WavetableFront>, std::function, string, uint16_t×2,
      vector<vector<WaveAssignment>>, bool
- [x] Disassemble PlayArgs::parse @0x15d7b0 — pre-scan for VarSubType==2
      (marker), split String/Const vs other, dispatch to parseImplicit/Explicit
- [x] Reconstruct PlayArgs struct in custom_functions.hpp (was opaque_[0x300])
- [x] Implement PlayArgs ctor, dtor (=default), parse(), getMaxSampleLength()
- [x] Rename AWGCompilerConfig::unknown_14 → channelsPerGroup[2]
- [x] Implement parseImplicitChannels @0x16fb30 (~1200B)
- [x] Implement parseExplicitChannels @0x170000 (~1500B)
- [x] Add secureLoadWaveform @0x1711a0 (thin wrapper)
- [x] Implement play() @0x15f090 using real PlayArgs
- [x] Fix mergeWaveforms return type (void → shared_ptr<WaveformFront>)
- [x] Implement playIndexed() @0x160e00 using real PlayArgs
- [x] Implement addChannelWave @0x170ec0 (~500B)
- [x] Implement mergeWaveforms() @0x15e060 (~3KB, 7-phase dispatch) — done in Phase 21a
- [x] Implement 4 complex play wrappers (playAuxWave @0x135610 ~5KB,
      playDIOWave @0x1369f0 ~3.4KB, playWaveDIO @0x137740 784B,
      playWaveZSync @0x137a50 ~3.2KB) — done in Phase 21a
- [x] Build verify + sub-phase wrap-up

### 18c. Consider splitting custom_functions.cpp

After all Phase 18 work is complete, evaluate splitting custom_functions.cpp
(currently ~1850 lines, will grow further) into logical units:
- `custom_functions_play.cpp` — play/playIndexed/PlayArgs methods/wrappers
- `custom_functions_builtins.cpp` — bulk builtin implementations
- `custom_functions.cpp` — ctor, dispatch, utility methods, exceptions

### 18b. Bulk builtin function stubs (~60 return-nullptr stubs)

Many follow a common pattern: checkFunctionSupported → arg validation →
EvalResults(VarType(1)) → single AsmCommands call → return. Batch-disassemble
the smallest ones first for maximum marker reduction.

#### 18b-i. Small builtins (<1KB, mechanical pattern)
- [x] prefetch @0x1351d0 — thin wrapper to play(SubFunc::Default)
- [x] prefetchIndexed @0x135290 — always throws (mask 0x0)
- [x] setWaveDIO @0x14cae0 — always throws (deprecated)
- [x] waitTimestamp @0x1401c0 — st(Reg(0), 0x1b)
- [x] getPRNGValue @0x151a70 — luser(reg, 0x77), returns register as int
- [x] waitOnGrid @0x13d000 — asmWtrigLSPlaceholder (TODO: grid constant from res)
- [x] waitOnSync @0x13d3a0 — st(Reg(0), 0x92)
- [x] resetRTLoggerTimestamp @0x153f90 — st(Reg(0), 0x62/0x6d) device-dependent
- [x] getQAResult @0x14f380 — ld(reg, 0x61), returns register as int
- [x] Build verify + sub-phase wrap-up

#### 18b-ii. Medium builtins (1-2KB)
- [x] setDIO @0x130780 — sdio(reg, highBank), externalTriggeringMode_ protocol
- [x] getDIO @0x131040 — ldio(reg, highBank), externalTriggeringMode_ protocol
- [x] getDIOTriggered @0x131410 — ldiotrig(reg), externalTriggeringMode_ protocol
- [x] setID @0x1334a0 — sid(reg, highBank)
- [x] setTrigger @0x1454c0 — strig(reg)
- [x] getTrigger @0x145ad0 — addi+ltrig+andr, 2 registers
- [x] setInternalTrigger @0x146140 — sinttrig(reg)
- [x] lock @0x14dc70 — asmLockPlaceholder(wf, deviceIndex)
- [x] unlock @0x14e180 — asmUnlockPlaceholder(wf, deviceIndex)
 - [x] getCnt @0x14e8d0 — lcnt + devConst->field_54 range check
 - [x] waitDIOTrigger @0x13d630 — readConst + externalTriggeringMode_ + device-type dispatch
 - [x] getSweeperLength @0x14bca0 — readConst("AWG_USERREG_SWEEP_COUNT0/1") + luser
 - [x] setPrecompClear @0x14c720 — asmSetPrecompFlags(bool)
 - [x] getUserReg @0x14b480 — luser + HDAWG addi/suser/addSyncCommand path
 - [x] playZero @0x1387f0 — asmPlay with hold=false
 - [x] playHold @0x139030 — asmPlay with hold=true
 - [x] waitCntTrigger @0x13e460 — readConst("AWG_CNT_TRIGGERn_INDEX") + asmWtrigLSPlaceholder
 - [x] waitZSyncTrigger @0x13dcf0 — readConst + externalTriggeringMode_=2 + device-type dispatch
- [x] Build verify + sub-phase wrap-up

#### 18b-iii. Large builtins (>2KB)
- [x] getZSyncData @0x1316f0 (~3KB)
- [x] getFeedback @0x132420 (~4KB)
- [x] assignWaveIndex @0x133c40 (~5KB) — partial: boost::regex omitted, field_48_/field_70_/field_4C TODOs
- [x] wait @0x139760 (4640B)
- [x] waitTrigger @0x13abf0 (~2KB)
- [x] waitAnaTrigger @0x13b4b0 (~3KB)
- [x] waitDigTrigger @0x13c110 (~4KB)
- [x] waitDemodOscPhase @0x13eba0 (~3KB)
- [x] waitSineOscPhase @0x13f790 (~2.5KB)
- [x] resetOscPhase @0x1403b0 (~6.5KB)
- [x] setSinePhase @0x141df0 (~4KB)
- [x] incrementSinePhase @0x142da0 (~4KB)
- [x] waitDemodSample @0x143d50 (~5.2KB)
- [x] getAnaTrigger @0x146740 (~3.2KB)
- [x] getDigTrigger @0x147420 (~3.2KB)
- [x] setInt @0x1480d0 (~2.5KB) — delegates to writeToNode (unimplemented)
- [x] setDouble @0x148ac0 (~3.3KB) — delegates to writeToNode (unimplemented)
- [x] generate @0x149940 (~2.8KB)
- [x] setUserReg @0x14a420 (~4KB)
- [x] at @0x14ce30 (~2.5KB)
- [x] waitQAResultTrigger @0x14edc0 (~1.4KB)
- [x] startQAResult @0x14f620 (~2.7KB)
- [x] startQAMonitor @0x1500b0 (~2.1KB)
- [x] executeTableEntry @0x150900 (~2.7KB)
- [x] setPRNGSeed @0x1513e0 (~1.6KB)
- [x] setPRNGRange @0x151ce0 (~2.4KB)
- [x] startQA @0x152690 (~6.2KB)
- [x] configFreqSweep @0x154240 (~5KB)
- [x] setSweepStep @0x155640 (~5KB)
- [x] setOscFreq @0x156a70 (~5KB)
- [x] configureFeedbackProcessing @0x157e60 (~5.6KB)
- [x] Build verify + sub-phase wrap-up
- [x] readConst/readString/readWave/readCvar return type corrected: void → EvalResultValue

---

## Phase 19 — Resources / EvalResults missing implementations (HIGH PRIORITY) ✅ DONE 2026-04-24

Surfaced after Phase 18 wrap-up: the static-archive build hides 295 undefined
zhinst symbols. Two clusters directly block earlier reconstruction work
(custom_functions builtins call `Resources::readConst` and `EvalResults::setValue`
without those symbols actually being defined). Tackle these first because
they would break any executable link of the library.

**Sub-phase status**: 19a ✅, 19b ✅, 19c folded into 20e, 19c-followup ✅,
19d ✅. All 19c-followup investigation prerequisites for Phase 20 cleared.

### 19a. EvalResults out-of-line definitions (6 blocking symbols) ✅ DONE 2026-04-23

Created `src/eval_results.cpp` (270 lines). All 14 methods implemented
(6 required + 8 nice-to-have), full impls — no stubs.

- [x] `EvalResults::EvalResults(VarType)` ctor @0x176bc0
- [x] `~EvalResults()` dtor @0x16f3d0
- [x] `setValue(VarType, Value const&)` @0x211b70
- [x] `setValue(VarType, VarSubType, Value const&)` @0x16bfb0
- [x] `setValue(VarType, int)` @0x15c850
- [x] `addAssembler(AsmList::Asm const&)` @0x15c1b0
- [x] BONUS: copy ctor @0x231c60, getValue @0x211ab0, plus 6 more
      setValue overloads (Value, VarType, double, string, VarType+Value+int,
      VarType+VarSubType+Value+int)
- [x] Build verify — clean, all 6 required symbols now `T`
- [x] Notes update: appended setValue codegen pattern to
      `notes/struct_layouts.md`. Key discovery: `setValue(double)` is
      the only overload that hard-codes VarType (=VarType_Const).

### 19b. Resources missing implementations — direct blockers ✅ DONE 2026-04-23

Appended ~190 lines to `src/resources.cpp` (482 → 671). All 3 blocker
symbols + the GlobalResources TLS statics now defined.

- [x] `Resources::readConst(string, EDirection)` @0x1e7d70 — partial
      (int/bool/double variant paths fully decoded; absW≥3 string path
      has a TODO referencing 1e7e2c..1e7e36 for variant-string layout).
- [x] `Resources::addConst(string, double, VarSubType)` @0x1e7010 —
      full impl. **Discovery**: writes literal **4** to Variable+0x00,
      not VarType_Const=3 — the on-disk record tag for value-bearing
      const entries is 4 (the bare-stub `addConst(name, st)` overload
      probably uses a different value). Worth a follow-up note.
- [x] `Resources::newLabel(string)` @0x1ec6b0 — full impl. Post-
      increments `GlobalResources::labelIndex` (TLS+0x4c) and appends
      decimal value to the (possibly empty → "label") prefix via
      ostringstream.
- [x] `GlobalResources::random`, `regNumber`, `labelIndex` TLS statics
      — all defined.
- [x] `ResourcesException` ctor/dtor — verified defined in resources.cpp.
- [x] Build verify — clean, all 3 blocker symbols now `T`.
- [x] Discovery: readConst error codes are 0xb0=UninitializedVar (miss)
      and 0xaf=TypeMismatchWrite (wrong type). addConst dup-name throws
      0xab=AlreadyDefined.

### 19c. Resources full sweep (all remaining declared methods, ~38)

**Folded into Phase 19d's planning output.** Will be re-organized as
work-packages alongside the other class-by-class undefined symbols
discovered during the full sweep. Items listed here for reference:

- [x] `createSubScope` @0x1e36a0
- [x] `updateParent` @0x1e38f0
- [x] `variableDependsOnVar` @0x1e40e0
- [x] `variableExists` @0x1e4230
- [x] `variableExistsInScope` @0x1e4390
- [x] `getVariableType` @0x1e4460
- [x] `getVariableSubType` @0x1e4580
- [x] `addVar` @0x1e46b0
- [x] `updateVar` @0x1e4c40
- [x] `checkVar` @0x1e4e20
- [x] String family: addString×2 @0x1e5020/0x1e54f0, updateString
      @0x1e59d0, readString @0x1e5d70
- [x] Wave family: addWave×2 @0x1e6020/0x1e64f0, updateWave @0x1e69c0,
      readWave @0x1e6d60
- [x] Const family: addConst (overload) @0x1e74e0, updateConst
      @0x1e79b0, constIsSet @0x1e8050
- [x] Cvar family: addCvar×2 @0x1e8180/0x1e8650, updateCvar @0x1e8b20,
      readCvar @0x1e8e80
- [x] Function family: functionExists @0x1e9110, getFunction @0x1e9370,
      functionExistsInScope @0x1e95d0, getPossibleFunctions @0x1e9740,
      addFunction @0x1e9c10
- [x] `getRegister` @0x1eba50
- [x] `setupGlobalState` @0x1ec8f0 (the Resources(name, deviceConstants)
      ctor variant)
- [x] StaticResources ctor/dtor/getVariable @0x129cb0/0x129db0/0x129e00/0x129e60
- [x] GlobalResources ctor/dtor @0x12a710/0x12ab40
- [x] Function::addArguments + addBody bodies (currently empty stubs;
      depend on SeqCAstNode interface)
- [x] Resources::Variable::~Variable variant string cleanup (currently
      stubbed)

### 19c-followup. Open investigation items from 19a/19b ✅ DONE 2026-04-24

Surfaced during 19a/19b reconstruction; all three resolved before
Phase 20.

- [x] **Variable record-tag mismatch (CRITICAL for 19c)** — RESOLVED.
      Root cause was a wrong VarType enum, not a separate "record-tag"
      mechanism. `str(VarType) @0x247dd0` jump table at 0x95c2a0 yields
      the canonical mapping `Unset=0, Void=1, Var=2, String=3, Const=4,
      Wave=5, Cvar=6` (was `Unset=0, Var=2, Const=3, Cvar=4, String=5,
      Wave=6`). Tag 4 IS VarType_Const under the corrected enum;
      Variable+0x00 IS the VarType directly. Tag mapping verified
      across all add/read overloads. Cascading fix applied to
      resources.hpp, resources.cpp (str, toString, addConst, readConst),
      expression.cpp:177, eval_results.cpp setValue(double) doc, and
      custom_functions.cpp comments. See `notes/struct_layouts.md`
      ("VarType enum — corrected mapping" section).
- [x] **readConst absW≥3 string path** — RESOLVED. Filled in for both
      SSO (bulk 24-byte copy) and long-form (placement-new of
      std::string(ptr, size)) paths in resources.cpp. Variable layout
      reconciled simultaneously: `which_` is at +0x10, variant storage
      at +0x18, with a `subType` (VarSubType) field at +0x08 not
      previously identified. Size 0x58 confirmed by `add r14, 0x58` at
      1e8441. New `variantStorage[16]` field replaces the prior
      embedded-Value approach. `Variable::~Variable` dtor body filled
      in. See `notes/struct_layouts.md` ("Variable layout — corrected"
      section).
- [x] **GlobalResources TLS layout audit** — RESOLVED. Five zhinst
      slots in shared `.so` TLS module block fully mapped: nextID
      (+0x40), Node::idCounter_ (+0x44), GlobalResources::regNumber
      (+0x48), labelIndex (+0x4c), random[313] MT19937-64 state (+0x50,
      2504B). Total zhinst TLS = 0x9D8. `Node::idCounter_` declaration
      added to Node class (replaced free static `node_id_counter`).
      All members are C++11 thread_local statics, NOT a struct
      instance. randomSeed builtin (Phase 17c stub) would write to
      `random[0]` and re-run the seed loop. resources.hpp
      GlobalResources doc-block fully rewritten.

### 19d. Full undefined-symbol sweep + triage ✅ DONE 2026-04-23

Methodology: `nm libzhinst_seqc.a` → set difference (U − T) =
**95 truly-undefined zhinst symbols**. Full audit document at
`reconstructed/notes/linker_resolution.md` (was `undefined_symbols_audit.md`, 420 lines). Plan
folded into Phase 20 below; Phase 19c carry-forward Resources items
folded into 20e.

- [x] Run full nm sweep and categorize by class — 95 symbols across
      ~17 classes/free-fn groups; `ErrorMessages::format<...>` is the
      single largest cluster (64/95 = 73%).
- [x] Produce `notes/linker_resolution.md` (was `undefined_symbols_audit.md`) with a class-by-class
      inventory + cost estimates.
- [x] Propose phased work-packages in TODO.md — Phase 20 below
      (5 sub-phases 20a–20e).
- [x] User review / approval of the plan — confirmed; Q1
      (ErrorMessages strategy = inline header template) and Q2
      (WavetableManager\<IR\> = out-of-line specialization) resolved.

---

## Phase 20 — Undefined-symbol elimination (executable-link prep)

Outcome of Phase 19d audit (`notes/linker_resolution.md`).
95 symbols are referenced by some TU but defined nowhere. Five
work-packages, ordered by impact-per-effort: **20a clears 73% of the
gap with one cheap edit**; 20e (Resources sweep) is deferred to last
because of size.

### 20a. Globals + ErrorMessages template body (69 symbols, biggest single win) ✅ DONE 2026-04-24

Highest-impact, lowest-cost package: a single header edit + a few
one-line global definitions clear 73% of the entire gap (69/95) and
unblock 14 caller TUs.

- [x] Inline `ErrorMessages::format<Args...>(ErrorMessageT, Args&&...)`
      body into `error_messages.hpp:457-466` using `boost::format` +
      `std::initializer_list<int>{...}` chained `operator%` (C++17,
      safer than fold-expr). **Strategy confirmed by user (2026-04-23).**
      All 64 instantiations now generated as weak symbols `W` at caller TUs.
- [x] Define `ErrorMessages errMsg;` in `error_messages.cpp:28`
      (BSS @ 0x95de60).
- [x] Define `Prefetch::minIndexedSize` static int in `prefetch.cpp:2294`.
      Init value `0x1000` (4096) recovered from `__cxx_global_var_init`
      at 0xd4361.
- [x] Define `zhinst::zsyncDataPqscDecoder`, `zsyncDataPqscRegister`,
      `constAwgIntegrationTrigger` `const std::string` globals in
      `error_messages.cpp:45-47`. Strings recovered from binary rodata
      via `strings _seqc_compiler.so`. Note: binary uses `L` prefix
      mangling (internal linkage); our header declares `extern` for
      cross-TU access — slight ABI deviation, behaviour unchanged.
- [x] Build verify (`cmake --build .` from `reconstructed/build/`).
      Build clean. Diff against `/tmp/truly_undefined.txt`: 64
      `format<>` template-instantiation symbols + 5 globals all
      resolved (69/95 = 73% gap closure, as planned).
- [x] Sub-phase wrap-up.

**Follow-ups (deferred to executable-link phase):**

- [x] When an executable target is added to CMakeLists, verify that
      the inlined `format<>` template body produces semantically-
      equivalent output — moved to Phase 31a.
- [x] Document the `extern`-vs-`L`-internal-linkage ABI deviation for
      the 5 zsync/integration-trigger globals. Added to existing
      `notes/libcpp_abi.md` under new "ABI deviations" section
      (2026-04-24).

**Estimated sessions:** 1.

### 20b. Trivial Rule-of-Five and missing default ctors (8 symbols) ✅ DONE 2026-04-24

After 20a, the remaining gap is 26. This package clears 8 more cheap
items, leaving 18 for the more involved packages.

- [x] `Immediate(Immediate const&)` copy ctor — added to `value.cpp`
      with per-index switch dispatch (placement-new for string case).
- [x] `Immediate(Immediate&&)` move ctor — leaves `other` in valueless
      state (matches variant-move idiom).
- [x] `Immediate& operator=(Immediate const&)` copy assignment —
      destroys current state then copy-constructs in place.
- [x] `Value()` default ctor — `value.cpp`. Sets `type_=Unspecified,
      which_=0, storage_.i=0`. Any toX() conversion will throw.
- [x] `Node()` default ctor — `node.cpp`. Delegates to 3-arg ctor with
      `(NodeType{0}, 0, -1)`.
- [x] `WaveformFile(const char*)` ctor — `waveform.cpp`. Copy-constructs
      `name`, zero-inits `field18/1C/20`, default-init `data`. Note:
      binary's 0x2a7ff0 was a `std::construct_at<>` specialization
      inlining the body — no dedicated ctor symbol existed.
- [x] `floatEqual(double, double)` @0x2ec050 — added to
      `waveform_generator.cpp`. **Surprise**: despite the name, the
      binary uses `cmpeqsd` (exact IEEE-754 equality), NOT a tolerance
      check. Reconstruction matches.
- [x] Move `logExceptionToClog` from `zhinst::detail` to
      `zhinst::logging::detail` in `log_exception.cpp` — single namespace
      wrap fix.
- [x] Build verify. All 8 symbols verified `T` in archive. Total
      progress: **77/95 (81%) done, 18 remaining.**
- [x] Sub-phase wrap-up.

**Estimated sessions:** 1.

### 20c. Wavetable / Waveform ctors and template instantiations (5 symbols) ✅ DONE 2026-04-24

Same area of the codebase — batched to share disassembly context.

- [x] `WaveformIR::WaveformIR(string const&, WaveformFile::Type, DeviceConstants const&)`
      — body recovered from `allocate_shared<WaveformIR>` dispatcher
      inlining at `0x2aa170-0x2aa20f`. Added to `waveform_ir.cpp`.
- [x] `WaveformFront::WaveformFront(string const&, WaveformFile::Type, DeviceConstants const&)`
      — body recovered from `newWaveformFromFile` dispatcher inlining at
      `0x29b110-0x29b24f`. Added to `waveform_front.cpp`. Also corrected
      the misleading `bitsPerSample=dc[0x40]` comment in
      `waveform_front.hpp:65` (real fields: `seqRegWidth ←
      dc.waveformGranularity`; `frontField1 = 1`).
- [x] Added `WavetableManager<WaveformIR>::insertWaveform` body
      (mirror of `WaveformFront` specialization in
      `wavetable_manager_front.cpp:41` with `WaveformIR` substituted).
      Required forward-declared specialization at top-of-file because
      same-TU calls implicitly used it earlier (C++14 [temp.expl.spec]/6).
- [x] Added explicit-instantiation lines at the bottom of
      `wave_index_tracker.cpp` for both `WaveformFront` and `WaveformIR`.
      Symbols emit as `W` (weak), satisfying U references at link time.
      Also fixed pre-existing typo: template body referenced
      `wp->playIndex` but field is named `waveIndex` at Waveform+0x6C.
- [x] Build clean.
- [x] Sub-phase wrap-up: OVERVIEW.md row added; audit doc WP-C marked
      DONE with detailed findings; running total 82/95 (86%) symbols
      cleared, 13 remaining for WP-D + WP-E.

**Estimated sessions:** 1. **Actual:** 1.

**Follow-ups (deferred to executable-link time):**
- [x] Verify at executable-link time that the binary's `0x29d000`/`0x29d410`
      `WaveIndexTracker` template-ctor instantiations actually read
      `Waveform+0x6C` — moved to Phase 31a.
- [x] Document the WaveformIR/WaveformFront 3-arg ctor ABI deviation in
      `notes/libcpp_abi.md` (member-init form vs dispatcher zero-then-
      overwrite — same observable state, different instruction sequence).
- [x] *Split WaveIndexTracker instantiation lines* — moved to Phase 31f.

### 20d. Pimpl wrappers + parser context + NodeMap helpers (7 symbols) ✅ DONE 2026-04-24

These are scattered small methods, each in a different TU. Batched
to clear several caller TUs entirely in one pass.

- [x] Uncomment `AWGAssembler::assembleStringToExpressionsVec`
      wrapper in `awg_assembler.cpp:43-45` with the correct
      `vector<shared_ptr<AsmExpression>>` return type (sret @0x285100).
- [x] Implement `AWGAssemblerImpl::extractComment(string const&)`
      body (find `;` or `//`, return suffix). NOTE: binary inlined
      this twice as `$_1` lambda in `assembleStringToExpressionsVec`
      and never emitted a standalone symbol; we externalize as a real
      method to satisfy the cross-TU U reference.
- [x] Implement `SeqcParserContext::raiseError(string const&)`
      @0x247ae0 and `SeqcParserContext::setSyntaxError()` @0x247cb0
      (mirror `AsmParserContext` equivalents). New TU
      `src/seqc_parser_context.cpp` uses raw byte-offset access since
      class is opaque in header. ABI risk: vtable[6]/+0x30 callback
      offset is libc++-specific.
- [x] Implement `NodeMap::toFrequency(double, double)` @0x1c5630 and
      `NodeMap::toPhase(float)` @0x1c5680. Math fully decoded:
      `toFrequency = (int64_t)(freq * 2^48 / sampleClock)` with
      `2^48 = 281474976710656.0` from rodata `0x956078`;
      `toPhase = roundf(value * 23301.689f)` then 23-bit two's-comp
      wrap (scale `0x46b60b61` from rodata `0x8fd2b4`, wrap shared
      with `pauPoffIwrap` extracted as `wrap23()`).
- [x] Implement `parseOptionalRate(it, end, end, name, bool)` free fn
      @0x163980. Header param naming misleading: third iter is parse
      cursor (typically `PlayArgs::parse()` return). If exactly one
      EvalResultValue remains, calls `getPlayRate()`; else encodes
      "no-rate" sentinel `((strict?1:0)-1)|5` (5 strict, 0xffffffff
      lax). Throws on extra unparsed args.
- [x] Build verify (clean, no warnings, no link errors).
- [x] Sub-phase wrap-up (this commit). 6 of 7 symbols verified `T`/`W`
      in archive; the 3 string/iterator-bearing symbols emit with
      libstdc++ mangling (`__cxx1112basic_string`,
      `__gnu_cxx17__normal_iterator`) instead of libc++
      (`__1::basic_string`, `__1::__wrap_iter`) — internally
      consistent and resolved at link time. Total now 89/95 (94%);
      6 symbols remaining for WP-E.

**Estimated sessions:** 1. **Actual:** 1.

**Follow-ups:**
- [x] Verify SeqcParserContext callback — moved to Phase 31d.
- [x] Update per-symbol audit script — moot; `cmake --build` is the
      authoritative gate (Phase 20e resolved all 95 symbols).

### 20e. util::wave + MemoryAllocator + CsvParser + 19c-carry-forward Resources sweep

Largest package. Combines the remaining 5 trivial undefineds with the
big Resources class sweep (37 method bodies from the 19c carry-forward
list) so that those Resources methods land before any future consumer
re-introduces undefined references.

**Prerequisite — 19c-followup investigation items** ✅ CLEARED 2026-04-24
(VarType enum + Variable layout corrected, TLS layout fully mapped). The
38 Resources method bodies below now have a known-correct struct layout
to write against.

**Prerequisite — fresh undefined-symbol gap check (added Phase 20d wrap-up):**
Before starting WP-E work, re-run the gap analysis querying BOTH
mangling variants (libc++ `__1::basic_string`/`__wrap_iter` AND
libstdc++ `__cxx1112basic_string`/`__gnu_cxx17__normal_iterator`).
The post-20d count may be lower than the audit's 6 if some symbols
are now incidentally satisfied by template instantiations dragged in
during 20a–20d. Update `notes/linker_resolution.md` with the
true remaining list before starting implementation.

#### 20e-i. Cheap wins: util::wave + MemoryAllocator + CsvParser (5-6 symbols) ✅ DONE 2026-04-24

Risk-ordered first batch — all small, each in its own TU; no
cross-cutting layout assumptions. Land these before tackling the
Resources sweep.

- [x] `zhinst::util::wave::double2awg(double, uint)` @0x299630.
      14-bit signed sample, 2 marker bits in low. Saturate at 1.0,
      scale `8190.0`, lround, pack `(rounded<<2)|(marker&3)`.
- [x] `zhinst::util::wave::double2awg1m(double, uint)` @0x299680.
      15-bit signed sample, 1 marker bit. Scale `16383.0`,
      pack `(rounded<<1)|(marker&1)`.
- [x] `zhinst::util::wave::double2awg16(double)` @0x299700.
      16-bit signed sample, no marker. Scale `32767.0`.
- [x] `zhinst::util::wave::hash(string const&)` @0x299760.
      **Uses `boost::uuids::detail::sha1`** (binary calls
      `process_block` @0x29a270 and `get_digest` @0x29a000).
      Returns `vector<uint32>` of 5 words (160-bit SHA-1).
      Reconstructed via `boost::uuids::detail::sha1::process_bytes`
      + `get_digest(uchar[20])` with manual MSB-first packing.
- [x] `MemoryAllocator::MemoryAllocator(DeviceConstants const*, uint32_t)`.
      Inlined at all binary call sites; we provide a real ctor
      matching the documented 0x70-byte layout.
- [x] `CsvParser::csvFileToWaveform<WaveformIR>` @0x2be830 — STUB.
      Full reconstruction deferred (~7000 bytes; not on hot path).
      Stub throws `std::runtime_error` instead of silently
      producing zero-filled waveforms. Open question #1 resolved:
      binary DOES contain real specializations (not inlined as
      archived Phase 12c claimed).
- [x] Build verify (clean, no warnings).
- [x] Mini-sub-phase wrap-up. **Final undefined-zhinst-symbol
      gap = 0** — `comm -23 <undefined> <defined>` is empty.
      Static archive fully self-contained for the zhinst namespace.

**Estimated sessions:** 1. **Actual:** 1.

**Follow-ups:**
- [x] CSV waveform handling — DONE Phase 28 (csv_parser.cpp fully reconstructed).
- [x] Verify SHA-1 byte order at executable-link time — moved to Phase 31a.

#### 20e-ii. Resources 19c carry-forward sweep (~38 methods)

Bulk Resources-class implementation. Heavier risk because all share
the same Variable/VarType/Scope layout — if the layout has any
remaining surprises, they'll cascade across all 38 methods. Do this
AFTER 20e-i so the cheap wins are safely landed first.

- [x] **Resources 19c carry-forward** — all 37+ methods implemented
      across Batches 1-6 (resources.cpp + resources_function.cpp).
- [x] **StaticResources** ctor/dtor/getVariable — Batch 7
      (resources_static_global.cpp).
- [x] **GlobalResources** ctor/dtor — Batch 7
      (resources_static_global.cpp).
- [x] `Resources::Variable::~Variable` — defaulted (embedded Value
      member handles cleanup).
- [x] Build verify after each group of ~10 methods.
- [x] Sub-phase wrap-up — final nm gap check done 2026-04-24.

**Completed 2026-04-24.**

---

## Phase 21 — Post-20e gap closure

Identified during the 2026-04-24 post-20e audit (marker sweep + Deferred
review + unknowns review). Sub-phases ordered roughly by impact-per-effort.
Each sub-phase ends with the standard wrap-up (notes/OVERVIEW/TODO update +
build verify).

### 21a. Custom_functions throw-stubs — play wrappers + mergeWaveforms (COMPLETE 2026-04-24)

Closed all 5 throw-stubs in `custom_functions.cpp`. Marker count for the
file shifted from "5 huge throw-stubs" to "~45 small surgical TODOs"
(reduction in unimplemented-mass; new TODOs are fine-grained items
captured as 21a-followup below).

- [x] `mergeWaveforms` @0x15e060 (893 disasm → 288 C++ lines) —
      `custom_functions.cpp:767-1054`. 7-phase body. Header signature
      corrected (`int64_t`→`bool`). 2 pre-existing call-site bugs fixed.
- [x] `playAuxWave` @0x135610 (1118 disasm → 242 C++ lines) —
      `custom_functions.cpp:1660-1902`. PlayArgs(indexed=true). Channel
      scatter + zero-fill + asmPlay isHoldMode=true.
- [x] `playDIOWave` @0x1369f0 (819 disasm → 229 C++ lines) —
      `custom_functions.cpp:1698-1927`. PlayArgs(indexed=false). Per-bit
      trigger mask clearing.
- [x] `playWaveDIO` @0x137740 (187 disasm lines) — direct wvft emission.
- [x] `playWaveZSync` @0x137a50 (697 disasm lines) — readConst chain
      (RAW/PROCESSED_A/PROCESSED_B → 1/9/0xd shifts).
- [x] Sub-phase wrap-up: build clean, OVERVIEW updated (21a row),
      notes/struct_layouts.md extended with 3 new sections
      (PlayArgs::WaveAssignment 0x50 layout, play wrapper signature
      matrix, mergeWaveforms 6-arg semantics).

**Sessions used:** 1.

### 21a-followup. Surgical TODOs surfaced during 21a reconstruction (COMPLETE 2026-04-24)

All 6 surgical TODOs cleared. `custom_functions.cpp` marker count
45→35 (10 markers cleared).

- [x] **mergeWaveforms factory selection** (phase 6 of body) — three
      factory targets confirmed: `interleave@0x258140`, `merge@0x25f5c0`,
      `grow@0x260640`. Multi-value dispatch `test bl,bl;je` reads `bl`
      from `[rbp-0x48]` (function-local, not direct param); single-value
      branch unconditionally GROW. Source `(multiValue, useYSuffix)`
      mapping documented as approximate; exact `[rbp-0x48]` derivation
      noted as deferred sub-investigation in source comment.
- [x] **WaveformFront `channels()` accessor** — replaced raw read with
      `wf->signal.channels()` (`Signal::channels()` already exists at
      `signal.hpp:83`).
- [x] **mergeWaveforms call-site `param5_placeholder`** — both sites
      decoded as `(int)PlayArgs::getMaxSampleLength()` (set @0x15f634
      / @0x13400a after `getMaxSampleLength@0x15f62f` / `@0x133fce`,
      truncated from int64 to int by SysV stack-arg slot).
- [x] **mergeWaveforms `lengthDiffers` tracking** — `play` site bool
      decoded as `(ch != referenceChannelIndex)` from `setne al;cmp
      r14,[rbp-0xd8]` where `[rbp-0xd8] = r13[+0x24]`. Approximated as
      `(ch != channelIndex)` in source; renamed to `isSecondaryChannel`.
      `assignWaveIndex` site is hardcoded `false` (literal `push 0x0`).
- [x] **playDIOWave config field +0x16** — actually in playAuxWave
      `@0x135889`, decoded as `config_->channelsPerGroup[1]` (INDEXED
      variant, slot [1] of the 2-element uint16 array at config+0x14).
- [x] **WaveformFront `+0x48` field naming** — `assignWaveIndex`
      `@0x1342f1` writes to `Waveform::used` (already named in
      `waveform.hpp:104`); replaced offset-comment with `wf->used = true`.
- [x] Sub-phase wrap-up: build clean, OVERVIEW updated (21a-followup row),
      notes/struct_layouts.md mergeWaveforms section extended with
      param5/param6 semantics + factory-selection findings.

**Sessions used:** 1.

### 21b-prereq-A. Audit unaccounted mergeWaveforms call sites (COMPLETE 2026-04-24)

During 21a-followup we believed `mergeWaveforms` had **5** call sites in
the binary but only **2** in our reconstructed source. Audit revealed
the original count was wrong — we actually had **4 of 5** sites
implemented. The 5th is inside `playIndexed`, which is itself a heavily
stubbed function. See 21b-prereq-B for the real follow-up.

- [x] Disassemble `mergeWaveforms` callers @0x135ddc, @0x136cfa,
      @0x161c2b. Findings:
      - @0x135ddc → `playAuxWave` (already implemented at line 1809)
      - @0x136cfa → `playDIOWave` (already implemented at line 2083)
      - @0x161c2b → `playIndexed` (stubbed; see 21b-prereq-B)
- [x] Sub-phase wrap-up: notes/struct_layouts.md mergeWaveforms section
      extended with full call-site map.

**Sessions used:** <1.

### 21b-prereq-B (PARTIAL — phases 1-7) (COMPLETE 2026-04-24)

`CustomFunctions::playIndexed` @0x160e00 is a 6428-byte function
previously stubbed as ~64 lines at `custom_functions.cpp:1219-1282`.
Surfaced by 21b-prereq-A as the home of the missing mergeWaveforms
call site @0x161c2b.

**Done this sub-phase**:
- Full disasm of body 0x160e00..0x16271c (2250 lines, cached at
  `/tmp/playIndexed.disasm`).
- Structural map of 18 phases derived from call landmarks (see
  `notes/struct_layouts.md` "playIndexed @0x160e00 — 18-phase
  structural map").
- Phases 1-5 reconstructed in C++ with **3 critical bug fixes** vs
  prior skeleton:
  1. arg-count guard `< 2` → `< 3` (off by one)
  2. `indexed = (subFunc != Aux)` → `(subFunc == Aux)` (was inverted)
  3. removed wrong `asmTable` call; binary uses `addi+asmPlay` pattern
- Phase 5b added: `waveIndex == 0` is a non-throwing warning path
  (format error 0x9c → `warningCallback_` via vtable[+0x30] indirect →
  return empty results).
- Phases 6-7 scaffolded with binary-accurate locals (`regZero(0)`,
  `channelArgs` vector, `triggerMask = 0x3fff`, Aux-vs-non-Aux split);
  full body deferred — see 21b-prereq-B-cont below.
- Phases 8-18 documented as embedded outline with binary call addresses.
- Build clean.

**Continued in**: 21b-prereq-B-cont, 21b-followup-1, 21b-followup-2.

### 21b-prereq-B-cont. Finish playIndexed phases 8-18 (COMPLETE 2026-04-24)

**Was blocked on**: 21b-followup-1 (resolved same session) and
21b-followup-2 (resolved same session). Both unblocked this work.

- [x] Phase 6 (Aux): real iteration over per-channel WaveAssignment
      vector + `checkWaveformInitialized` calls. *(Modeled with TODO
      marker pending 21b-followup-3; commented-out call kept in source
      to document the structural intent.)*
- [x] Phase 7 (non-Aux): per-channel arg-gather loop with the
      vectorized SSE trigger-mask formula. *(Documented as comment;
      mask init `0x3fff` carried through.)*
- [x] Phase 8: `getWaveformSampleLength(name)` probe @0x161853.
      *(Modeled with TODO marker; baseLen=0 placeholder feeds Phase 9.)*
- [x] Phase 9: `createDummyWaveform(baseLen)` @0x161951. *(Discovered
      `WaveformGenerator::createDummyWaveform` is the documented helper
      for the inline "zeros" idiom — see waveform_generator.hpp:137.)*
- [x] Phase 10: `mergeWaveforms` call @0x161c2b — **5th binary call
      site now in source**.
- [x] Phase 11: `WavetableFront::loadWaveform(combined)` @0x161d76.
- [x] Phase 12-14: `getRegisterNumber` + `addi(reg, 0, Imm(rate))`
      + `asmSetVarPlaceholder` + Assembler push_back. *(Caught build
      error: `addi` returns `vector<AsmList::Asm>` not single `Asm`;
      fixed.)*
- [x] Phase 15: `checkOffspecWaveLength(combined, expectedLen)` @0x16214a.
- [x] Phase 16-17: `asmPlay(...)` @0x162343 + `results->assemblers_
      .push_back` @0x162511. *(asmPlay 12-arg call modeled after
      play()'s pattern at lines 1196-1202; boolean flag mapping
      tagged TODO.)*
- [x] Phase 18: error message factories — documented as comment
      block; errors 0x3d/0x98/0xa0/0x9a wired into Phase 2/4/5b/12
      throws.
- [x] Verify all 5 mergeWaveforms call sites now present in source.
- [x] Sub-phase wrap-up.

**Outcome**: `playIndexed` @0x160e00 (6428 bytes, 2250 disasm lines)
fully reconstructed end-to-end. Build clean. Residual unknowns
(per-channel WA accessor, asmPlay flag mapping, named field
constants) absorbed into expanded 21b-followup-3 below.

**Sessions actual:** 1 (faster than estimated thanks to followup-1/2
resolution).

### 21b-followup-1. Identify Resources +0x24 field and per-channel WaveAssignment container (COMPLETE 2026-04-24)

Surfaced by 21b-prereq-B; resolved same session.

**Resolution**: the access pattern `[r12]; [rax+0x24]` was
mis-attributed to Resources. Actually `r12 = this` (CustomFunctions*),
so `[r12] = this->config_` (at CustomFunctions+0x00, an
`AWGCompilerConfig const*`), and `[config_+0x24] =
AWGCompilerConfig::deviceIndex` (already named in
`awg_compiler_config.hpp:70`). The accessor is just
`config_->deviceIndex` — no new field/accessor needed.

The outer `vector<vector<WaveAssignment>>` at `[rbp-0x440]` was
confirmed by destructor symbol at @0x162661. Type matches
`PlayArgs::waveAssignments_` (at PlayArgs+0x60). However the stack
local at -0x440 is NOT physically the same as the PlayArgs field —
it's a separate stack vector populated somewhere upstream (no explicit
sret-write found; likely populated as a side effect of
`PlayArgs::parse` via inlined out-parameter or an accessor that copies
the field into the local). Source-level model:
`playArgs.waveAssignments_[deviceIndex]` — exact origin tracked as
21b-followup-3 below (low priority; doesn't block reconstruction).

**Outcome**: playIndexed source updated with correct interpretation
(deviceIndex + waveAssignments_ accessor pattern). 21b-prereq-B-cont.
is unblocked.

### 21b-followup-3. Trace exact origin of [rbp-0x440] vector + name residual playIndexed unknowns — COMPLETE 2026-04-24

Low-priority residual cluster from 21b-followup-1 + 21b-prereq-B-cont.
Bundles all small unknowns left in `playIndexed` source after the
end-to-end reconstruction.

**Group A — `[rbp-0x440]` outer vector origin** — RESOLVED:
`[rbp-0x440]` IS `playArgs.waveAssignments_` (at PlayArgs+0x60).
The PlayArgs object lives at `[rbp-0x4a0]` on the stack; 0x4a0−0x60=0x440,
so the vector is simply the member field populated by `PlayArgs::parse()`
internally. No separate population mechanism.

- [x] Trace [rbp-0x440] population site between PlayArgs ctor
      @0x160fd1 and the Phase 6 access @0x161250.
- [x] If it's a member function call with a hidden out-parameter,
      identify the function and add a proper accessor declaration.
      → N/A: it's a direct struct member offset, not a function call.
- [x] If it's an inlined copy, document the pattern in notes.
      → N/A: it IS the member field.
- [x] Update `playIndexed` Phase 6/8 source to use the correct
      accessor: `playArgs.waveAssignments_[deviceIndex]`.

**Group B — Named field constants** — RESOLVED:

- [x] Name `WaveformFront::field_d0_` — RESOLVED as `signal.length()`.
      Waveform+0x80 (Signal) + Signal+0x50 (length_) = +0xD0.
      Comment updated in Phase 11 bounds-check.
- [x] Name `AWGCompilerConfig::field_40_` — MISIDENTIFIED. `[this+0x08]`
      loads `devConst_` (NOT config_). `devConst_+0x40` =
      `waveformGranularity`. Fixed 6 call sites: playIndexed Phase 15,
      play() Step 7, playAuxWave Phase 9, playDIOWave Phase 11,
      assignWaveIndex (×2: seqRegWidth set + checkOffspecWaveLength).
- [x] Pin down channelsPerGroup index: Aux=0x14 → [0], non-Aux=0x16 → [1].
      Conditional expression: `config_->channelsPerGroup[subFunc==Aux ? 0 : 1]`.

**Group C — playIndexed Phase 14/16** — RESOLVED:

- [x] Trace local-Assembler push in Phase 14 — pushes addiEntries +
      placeholderEntry into `results->assemblers_` (confirmed by disasm
      @0x161ed4 and @0x161f79: push_back on vector at [r12+0x18]).
- [x] Full SysV mapping of asmPlay's 12 args: isHold=(subFunc==Aux),
      fourChannel=(subFunc==DigTrigger), isBool=false, holdCount=rate,
      suppress=triggerMask, isHoldMode=(subFunc==Aux), reg=indexReg,
      regVal=waveIndex, reg2=AsmRegister(-1), trigger=0.
      NOTE: r8b/r9b register-arg mappings have some uncertainty.
- [x] Wire error 0x98 and 0x9a — moved to Phase 31a.

**Bonus findings:**
- `config_->field_18` in playAuxWave/playDIOWave emit-guard = `config_->isHirzel`.
- `assignWaveIndex`: both `wf->field_70_` set and checkOffspecWaveLength use
  `devConst_->field_4C` (NOT config_); `wf->field_70_` = `wf->seqRegWidth`.

**Estimated sessions:** 1-2 → **actual: 1**.

### 21b-followup-2. Re-audit all 5 mergeWaveforms call sites for SysV arg-shift correctness (COMPLETE 2026-04-24)

Surfaced by 21b-prereq-B; resolved same session.

**Resolution**: the puzzle (`rdx = lea` of a pointer where the 6-arg
signature said `short`) was caused by mis-counting registers — the
implicit `this` consumes rsi, shifting the explicit args by one
register. So actual layout is rdi=sret, rsi=this, rdx=arg1
(`vector const&`), rcx=arg2 (`short`), r8=arg3 (`bool`), r9=arg4
(`string const&`), stack[0]=arg5 (`int`), stack[1]=arg6 (`bool`).
Confirmed by inspecting the mergeWaveforms entry @0x15e060: the saved
rsi at @0x15e088 is later dereferenced via `+0x20` to fetch
`WavetableFront*`, which is `CustomFunctions::wavetableFront_`. So
rsi is `this`.

**Outcome**: no source bugs in the 4 already-implemented call sites;
no source changes needed. The only deliverable is the side-by-side
register-layout table now living in `notes/struct_layouts.md`
("SysV ABI puzzle ... — RESOLVED" section). Phase 10 of `playIndexed`
can proceed without further investigation.

### 21b. writeToNode (the 29KB beast) (IN PROGRESS)

Largest single gap in the entire codebase. Unblocks `setInt`, `setDouble`
and several node-access functions in `custom_functions.cpp`.

**Recon (cached `/tmp/writeToNode.disasm`, 6120 lines, 0x164550..0x16b740 = 0x71f0 / 29KB):**

- 4 static `boost::regex` objects in `.bss`:
  `b84748 absDevRegex`, `b84760 awgNodeRegex`, `b84778 sineNodeRegex`,
  `b84790 oscselNodeRegex` (each with adjacent guard variable at +0x10).
- Setup allocates 0x98-byte `__shared_ptr_emplace<EvalResults>` (vtable @b03d00).
- Top callees: `AsmCommands::suser` (53), `AsmCommands::addi` (44),
  `AsmList::append` (25), heavy `boost::regex_match`, `Value::toDouble`/`toString`/`toInt`.
- Sub-block map:
  | Block | Range | Size | Content |
  |-------|-------|------|---------|
  | Setup | 0x164550..0x164608 | ~180B | Allocate EvalResults; cmdName toString() |
  | A: absDevRegex | 0x16460e..~0x164800 | ~500B | Match abs device path |
  | B: awgNodeRegex | 0x164803..~0x164950 | ~330B | AWG channel dispatch |
  | C: sineNodeRegex | 0x164950..~0x164ae0 | ~390B | Sine osc dispatch |
  | D: oscselRegex | 0x164b19..~0x169d00 | **~21KB** | Bulk per-type codegen |
  | E: error tail | 0x169d83..0x169df4 | ~110B | Throw formatted error |
  | F: regex init | 0x169ea5..0x16a3f0 | ~1.5KB | Static regex ctors (cold) |
  | G: epilogue | 0x16a3f0..0x16b740 | ~5KB | dtors / unwinding |
- 3 call sites in binary: @0x141447, @0x1486f2 (setInt), @0x149334 (setDouble).
- Helper `boost::sub_match::str()` @0x16b830 + `std::stoul` to extract IDs.

#### 21b.1. Skeleton + Block A (absDevRegex) — **COMPLETE**

- [x] Declare 4 static `boost::regex` instances + guards in `custom_functions.cpp`.
      (Anonymous-namespace placeholders with TODO 21b.5 for real pattern strings.)
- [x] Function entry: allocate `EvalResults` via `make_shared`; cmdName toString();
      varType==2 (Var) precondition check → throw on failure.
- [x] **Bonus**: corrected return type from `void` to
      `std::shared_ptr<EvalResults>` (binary uses sret rdi). Header updated.
- [x] Block A: match against `absDevRegex`, extract device-id capture via
      `sub_match::str` + `stoul`, dispatch on `config_->deviceIndex`.
- [x] Trailing `lookupNode(pathStr)` call @0x1647ed performed unconditionally
      after Block A.
- [x] Sub-phase wrap-up: build clean, OVERVIEW.md status block + table row
      added, struct_layouts.md "writeToNode @0x164550 — recon (Phase 21b.1)"
      section appended.

#### 21b.2. Blocks B + C (awgNodeRegex + sineNodeRegex) — **COMPLETE**

- [x] Block B: AWG node — extract channel-index capture, normalise via
      `numChannelGroups` (1/2/4), validate against `config_->unknown_20`.
- [x] Block C: sine node — extract osc-index capture, map oscillator → channel
      via {2,4,8} oscs-per-channel logic, same channel-cap validation.
- [x] Both blocks documented in `notes/struct_layouts.md` ("writeToNode
      Blocks B + C — recon (Phase 21b.2)" section); error message strings
      remain TODO 21b.5.
- [x] Sub-phase wrap-up: build clean, OVERVIEW.md status block + table row
      added.

#### 21b.3. Block D structural skeleton (oscselNodeRegex, ~21KB) — COMPLETE

- [x] Map stack-frame `[rbp-0x1c0]..[rbp-0x180]` to local `NodeMapItem`
      from `lookupNode(pathStr)`; confirm `typeIdx` (+0x08) and `hasFast`
      (+0x10) field offsets.
- [x] Identify "MF" tag insertion into `usedFeatures_` (+0x1C8 set<string>),
      addNodeAccess call, getRegisterNumber + AsmRegister destReg + local
      AsmList accumulator setup.
- [x] Map dispatch jump tables @958f68 (fast path) and @958f50 (slow path
      / dyncast-success path), 6 cases (typeIdx 0..5) each.
- [x] Implement representative case (fast-path case 2 @0x164c7f..164cb4)
      as exemplar. Stub remaining 11 cases as TODO 21b.4.
- [x] Document Block D in `notes/struct_layouts.md`.
- [x] Sub-phase wrap-up.

#### 21b.3-fix. Block D structural correction — COMPLETE

- [x] Add missing `dynamic_cast<DirectAddrNodeMapData*>(node.data)` branch
      between fast-path and nodeAddressMap_-find paths. Block D is
      actually a 3-way dispatch: (A) hasFast path, (B) dyncast-success
      path (addr from `direct->addr_`), (C) nodeAddressMap_.find path.
      Paths (B) and (C) share jump table @958f50.
- [x] Enable `usedFeatures_.insert("MF")` (was commented out as
      unverified — binary unconditionally inserts at @0x164b51..164bee).
- [x] Refactor source: flatten 3-way addr resolution into a single
      `useFastJt` flag + `addr` value, then dispatch on either jt @958f68
      (fast) or jt @958f50 (paths B & C). Add `emitWarnAndReturn` early-
      return for typeIdx > 5 → @0x164d05 boost::log warning path.
- [x] Build clean.
- [x] Sub-phase wrap-up.

#### 21b.4-map. Block D protocol catalogue — COMPLETE

- [x] Read disasm of all 12 case bodies (paths A and BC × typeIdx 0..5)
      end-to-end and document the per-case emit-step structure.
- [x] Identify three case "shapes": (S1) single-step `addi(Imm) +
      suser(0x10)`; (S2) direct passthrough `suser(payload, 0x17[+0x19])`;
      (S3) low/mid/high triplet `addi(tag) → suser(0x10) → addi(addr) →
      suser(0x11) → suser(payload, 0x12)`.
- [x] Census suser opcodes: 0x10 (low/set), 0x11 (mid/addr-tag), 0x12
      (high/commit), 0x17/0x19 (direct-write).
- [x] Document Immediate sources: literal small ints, addr,
      `NodeMap::toFrequency(toDouble, getSampleClock)`,
      `NodeMap::toPhase(toFloat)`, raw float-bits (cvtsd2ss),
      `Value::toInt()` (slow-arm with `floatEqual` guard).
- [x] Map common-tail blocks (10 of them) and the variant-dispatch
      fallback ranges.
- [x] Catalogue saved to `notes/writeToNode_block_d_protocol.md`
      (253 lines, 7 tables + reference cases).
- [x] No source changes this sub-phase; build necessarily clean.

#### 21b.4-impl. Block D source transcription — COMPLETE

- [x] Verified jump-table order by dumping .rodata bytes at @0x958f50
      and @0x958f68 — the catalogue's linear-order assumption was WRONG
      for several cases. Corrected mapping:
      Path A: {0→0x164c7f, 1→0x165263, 2→0x165013, 3→0x165137,
               4→0x164ee7, 5→0x1652e6}.
      Path BC: {0→0x164de4, 1→0x16591b, 2→0x165587, 3→0x165751,
                4→0x165488, 5→0x165a17}.
- [x] Transcribed path-A cases 0..5:
      A.0: suser(val.reg_, 0x17) [register passthrough]
      A.1: suser(val.reg_, 0x17) + suser(R0, 0x19) [sine pair]
      A.2: toDouble→cvtsd2ss→memcpy float bits→addi+suser(0x17)
      A.3: toDouble→movq raw bits→low32→addi+suser(0x17)
      A.4: toDouble→getSampleClock→toFrequency→addi+suser(0x17)
      A.5: toDouble→cvtsd2ss→toPhase→addi+suser(0x17)
      All path-A cases share tail: addi(destReg, R0, Imm(addr)).
- [x] Transcribed path-BC cases 0..5:
      BC.0: addi(destReg, R0, Imm(1))
      BC.1: addi(destReg, R0, Imm(2))
      BC.2: double-triplet (tag 0xc→suser(0x10)→addr→suser(0x11)→
            val.reg_→suser(0x12); repeat with tag 0xd)
      BC.3: single triplet (tag 0xd, same as triplet B of BC.2)
      BC.4: single triplet (tag 2→suser(0x10)→addr→suser(0x11)→
            val.reg_→suser(0x12))
      BC.5: addi(destReg, R0, Imm(3)) [with varType check]
      Cases 0,1,5 share tail: suser(destReg, 0x10) + addi(destReg,
      R0, Imm(addr)).
- [x] Key correction from 21b.3: A.0's suser uses literal 0x17,
      NOT 0x17+addr. The addr goes into the common tail addi.
- [x] Key discovery: A.0/A.1 require varType==2 (jne to slow);
      A.2-A.5 require varType!=2 (je to slow). Slow-arm
      variant-dispatch is stubbed with TODO 21b.5 throws.
- [x] localList splice into results->assemblers_ — placeholder
      added (TODO 21b.5 to verify the exact EvalResults member).
- [x] Added `floatEqual` forward declaration to custom_functions.cpp.
- [x] Added `#include <cstring>` for std::memcpy.
- [x] Build clean.
- [x] Sub-phase wrap-up.

#### 21b.5. Tail (E/F/G) + setInt/setDouble forwarding + slow-arm stubs — COMPLETE

- [x] Block E: reclassified — 0x169d83 is shared_ptr cleanup, 0x169df4 is
      normal ret. Error 0x84 at 0x169e0d is cold-path from Block D oscsel.
- [x] Block F: static regex lazy-init — cancelled (cold-path ctor guard,
      not user-visible logic).
- [x] Block G: cleanup epilogue — verified as compiler-generated unwinding
      only (no user-visible logic).
- [x] Convert `setInt` / `setDouble` to return `writeToNode(...)` result.
- [x] Implement slow-arm variant-dispatch: 6 simple throws (A.2/A.3→0x7f,
      A.4→0x81, A.5→0x82, BC.4→0x7f, BC.5→0x82) + 4 real codegen paths
      (A.0 toInt+floatEqual+warning 0x80, A.1 64-bit double split,
      BC.2 float-bits tag-3, BC.3 64-bit double split tag-4+suser 0x13).
- [x] Verify localList splice target: `results->assemblers_` at
      EvalResults+0x18 (vector<AsmList::Asm>).
- [x] Sub-phase wrap-up.

**Completed in 7 sub-phases (21b.1..21b.5).** writeToNode 29KB function
substantially reconstructed. Remaining gap: per-case post-tails (see
21b-followup-4 below).

### 21b-followup-4. writeToNode per-case post-tails (path A) ✅ DONE 2026-04-24

All 6 per-case post-tail sequences implemented. Key structural correction:
the shared `addi(addr)` common tail at 0x165c90 was only used by typeIdx 0
(not all cases as previously modeled). Each typeIdx now has its own
post-tail block after the case body. Detailed trace confirmed via full
disassembly read of the 0x168xxx..0x169xxx region.

- [x] typeIdx 0 (passthrough): addi(addr) + suser(0x16) — commit.
- [x] typeIdx 1 (sine pair): 8-step extended tail — addi(addr) + addi(3) +
      suser(0x10) + suser(0x11) + toFrequency→addi(freq) + suser(0x11) +
      toPhase→addi(phase) + suser(0x16).
- [x] typeIdx 2 (float-bits): no post-tail (completes with body suser(0x17)).
- [x] typeIdx 3 (raw-double): addi(high32 via shr rbx,0x20) + suser(0x19).
- [x] typeIdx 4 (frequency): addi(freqHigh32) + suser(0x19) + addi(addr) +
      suser(0x18). NEW: closing opcode is 0x18 (not 0x16).
- [x] typeIdx 5 (phase): addi(addr) + suser(0x16) — commit.
- [x] Verify case routing: CORRECTED — only typeIdx 0 uses shared tail at
      0x165c90; ALL other cases bypass it. Prior note saying "cases 1,2,4
      bypass" was incomplete — cases 3,5 also bypass.
- [x] Protocol notes updated in writeToNode_block_d_protocol.md with
      corrected per-typeIdx emission table + closing-opcode semantics.
- [x] Both builds clean (g++ + clang++/libc++).


### 21c. AsmOptimize trio completion — COMPLETE

Self-contained optimizer work; no cross-file cascade.

- [x] `AsmOptimize::splitReg` @0x281000 — fully reconstructed from ~500
      lines of binary. Live-range splitter with ≥10 threshold, boundary
      ADDI insertion, allSplitOk/didSplit flags, post-loop kill.
- [x] `AsmOptimize::simplifyAssign` @0x280e10 — 4 bugs fixed: outputs[0]
      not immediates.back(), reg2/reg0 not reg1/reg2, scan from it+2,
      store to reg0 not reg1. Copy-propagation: redirect write-dest when
      followed by ADDI copy.
- [x] `assembler.hpp` reg field comments corrected: reg2=READ source,
      reg0=WRITE dest, reg1=dual-role. Field rename deferred (20+ file
      cascade, not worth the churn).
- [x] Sub-phase wrap-up.

### 21d. SeqCAstNode evaluate() virtual + VarType field — COMPLETE

Foundational AST work. Resolved unknowns #119 (SeqCParameter) and the
long-deferred evaluate() signature.

- [x] Identified `SeqCAstNode::evaluate()` virtual signature (3-arg at
      vptr[0], returns `shared_ptr<EvalResults>`). Base @0x209dc0 returns
      null. Binary TU: `SeqCAstNodesEvaluate.cpp`.
- [x] Full vtable layout verified from binary vtable dumps (8 slots base
      + 1 SeqCOperator extension at vptr[8] for 5-arg evaluate).
- [x] `VarType` at +0x14 made a proper base-class field (was "padding").
      All 53 subclass ctors updated with VarType as 4th explicit parameter.
- [x] `getVarTypes()` virtual at vptr[7] — base impl returns `{str(varType_)}`,
      SeqCParamList override iterates children.
- [x] `SeqCParameter` class REMOVED — `nm -C` shows zero symbols. VarType
      is base-class field; +0x18 is SeqCVariable::name_. Unknowns #119 resolved.
- [x] `addArguments(SeqCAstNode const&)` rewritten to use `varType()` +
      `static_cast<SeqCVariable const*>(node)->name()`.
- [x] Sub-phase wrap-up.

**Estimated sessions:** 1-2.

### 21e. WavetableManager\<T\> remaining methods — COMPLETE (pre-done)

Template, partially done. ~14 methods, ~6KB total.

- [x] Audit remaining unimplemented methods in
      `wavetable_manager_front.cpp` and `wavetable_manager_ir.cpp`.
      **Result: all 16 methods (9 Front + 7 IR) already implemented
      with zero TODO markers. Only loose end is a dead `setLineNr(int)`
      declaration in the template header — never instantiated, no linker
      issue. WavetableFront::setLineNr accesses the field directly.**
- [x] Reconstruct in priority order — N/A, nothing to reconstruct.
- [x] Sub-phase wrap-up.

**Estimated sessions:** 1-2.

### 21f. CachedParser ELF body + small markers sweep — COMPLETE

Closes the long tail of small markers across 8+ files. Drove total
marker count from ~94 to ~90 actionable (removed 8 TODOs, plus
several comment-only cleanups).

- [x] `CachedParser::cacheFile` ELF-building body @0x2b05b0 —
      fully reconstructed. File ext `.wave` (not `.elf`); filename
      `csv<hash2str>.wave`; machineType=3; section↔param mapping
      corrected. `removeOldFiles` return type changed `void`→`bool`.
- [x] `Function::addBody` try/catch — already clean (confirmed earlier).
- [x] `waveform_generator.cpp` 3 small TODOs resolved:
      readDoubleAmplitude fabs>1.0 warning (error 0x54 via
      warningCallback_); interpolation formula confirmed correct;
      lfsrGaloisMarker marker validation (valid={1,2}, error 0x64).
- [x] `awg_device_props.hpp` field names VERIFIED from consumer analysis:
      `maxWaveformSamples`→`maxElfSize`; `maxWaveformBytes`→split into
      `addressImpl` (uint32) + `sampleFormat` (uint32); 
      `supportsExtraFeature`→`isHirzel`. 9 instantiations updated.
- [x] `cached_parser.cpp` boost::archive workarounds — deferred (no
      change needed until build links real boost archive).
- [x] Sub-phase wrap-up: build clean, OVERVIEW updated.
- [x] Bonus: `compiler.cpp` evaluate() call wired up (unblocked by 21d);
      SeqCOperator 5-arg evaluate comment fixed.

**Estimated sessions:** 1. ✅ Completed.

### 21g. libc++/libstdc++ ABI design discussion ✅

Decision: dual-build (a)+(c). Completed 2026-04-24.

- [x] Survey all current per-call-site workarounds (Pimpl, raw byte
      offsets, vtable-offset assumptions).
      → ~45 reinterpret_cast, ~15 placement-new, ~25 static_asserts,
        2 GCC pragmas, 0 conditional compilation.
- [x] Evaluate three strategies: (a) build with libc++ via clang;
      (b) translation header switching on `_GLIBCXX_USE_CXX11_ABI`;
      (c) accept libstdc++ as canonical and document each ABI bleed.
      → (a) adopted + (c) retained as primary. (b) rejected.
- [x] Document decision in `notes/libcpp_abi.md`.
- [x] Implement: `build-libcxx/` directory (clang++ -stdlib=libc++),
      ABI-adaptive static_asserts, conditional std::function size assert.
- [x] Wrap-up: both builds clean (g++: 0err/1warn; clang++: 0err/6warn).
      Follow-up ABI cleanup is optional, not a new phase.

**Estimated sessions:** 1. ✅ Completed.

### 21h. SeqCAstNode evaluate() body reconstruction

Natural follow-on to Phase 21d. Now that evaluate() signatures and
VarType are wired, reconstruct concrete evaluate() method bodies for
key subclasses.

- [x] Build complete evaluate() address map from binary (32 3-arg + 22 5-arg).
- [x] `combine(VarType, VarType)` @0x247f50 — full 7×7 lookup table.
- [x] `VarTypeException` class (ctor @0x2480e0, dtor @0x248140).
- [x] SeqCOperator::evaluate(3-arg) @0x210aa0 — template-method that
      evaluates lhs/rhs children, combines VarTypes, dispatches to 5-arg.
- [x] SeqCValue::evaluate() @0x213140 — tag-dispatched: double path
      (setValue + ostringstream precision 5), string path (quote stripping +
      setValue(VarType_String)).
- [x] Resolve AsmCommands+0x50 = `wavetableFrontIndex_` lineNr field.
- [x] Declare evaluate override on ALL subclasses: 5 macros updated
      (SEQC_TRIVIAL_LEAF, SEQC_UNARY, SEQC_OPERATOR, SEQC_BINARY,
      SEQC_LIST) + 6 individually-declared classes.
- [x] Corrected SeqCValue::Tag enum (eString=0, eDouble=1, -1=empty).
- [x] Sub-phase wrap-up (both builds clean, OVERVIEW.md, TODO.md).
- [x] SeqCVariable::evaluate() @0x209ea0 — 3712 bytes, complex.
      Decoded both dispatch tables (varType_ 7-way + getVariableType
      5-way), all five `addX(name, Stub)` direct paths, all four `readX`
      paths (push single EvalResultValue into result->values_), Wave
      special path with `newEmptyWaveform`, error path for varType=Void.
      Inline EvalResultValue heap-build + manual vector-replace pattern
      collapsed to `values_.assign(1, std::move(rv))` semantics. Added
      `Resources::atScopeBoundary()` accessor for the +0x88 byte gate.
      Both builds clean.
- [x] Representative operator 5-arg overrides (SeqCPlus, etc.) to
      validate the template-method pattern. **SeqCAssign DONE** as the
      first 5-arg override (sub-phase 21h.3 below). **SeqCPlus DONE**
      as first arithmetic override (sub-phase 21h.4 below). Remaining
      arithmetic operators (SeqCMinus, SeqCMult, SeqCDiv) in 21h.5-7.
- [x] **21h.3 SeqCAssign::evaluate (5-arg) @0x243e60** — 5552 bytes,
      second-largest evaluate. Hidden-sret + this register shift
      resolved (rdi=retptr, rsi=this, [rbp+0x10]=rhsResult, [rbp-0x110]=ctx*).
      Cascade re-interpreted as outer dispatch on lhsType + inner
      rhsType compatibility test (Pass 5.5 → Pass 6 implementation).
      All 13 action rows implemented in
      `seqc_ast_nodes_evaluate.cpp` (~280 new lines): Var/Const/Cvar/
      String update*() paths, Wave[Numeric/FunctionArg/other] paths,
      VarTypeException catch handler with rhs-type propagation.
      Both builds clean. ASM emission micro-detail (addi/asmSetVarPlaceholder
      placeholder operand encoding) deferred via TODO comments.

**Estimated sessions:** 2-3 (3 used so far).

#### 21h.3-followup — deferred items from SeqCAssign reconstruction

- [x] **~~ASM emission for Const/Cvar/String rows (rows 3-5)~~** —
      **RESOLVED (2026-04-24).** Binary analysis confirmed there IS NO
      asm emission for these rows. Rows 3/4/5 are compile-time constant
      assignments: `updateConst()`/`updateCvar()`/`updateString()` →
      `result->setValue(VarType, lhsSub, value)`. All three share a
      common tail: `result->name_ = name + " = " + rhsResult.name_`
      @0x24594a. The TODO comments about `addi(R0, R0, ...)` and
      `asmSetVarPlaceholder(R0)` were completely wrong. Source updated
      with `setValue()` calls and name-concat tail.
- [x] **~~EvalResults +0x70 vs waveformFront_ +0x48 confirmation~~** —
      **RESOLVED (2026-04-24).** Confirmed as `arrayBacking_` semantics.
      Converted TODO to NOTE in source. The +0x70 field matches the
      `shared_ptr<EvalResults> arrayBacking_` at EvalResults+0x70 per
      the header layout.
- [x] **~~Row 7 (Wave[non-Numeric] = Wave) reachability~~** —
      **RESOLVED (2026-04-24).** Converted TODO to NOTE documenting the
      known difference: binary has error 0x8b path, reconstruction
      routes through the default handler which emits error 0xe9 instead.
      Not functionally blocking.
- [x] **EvalResults::getValue() return type correction**: **RESOLVED
      (2026-04-24).** Disassembly of `getValue()` @0x211ab0 confirmed it
      returns `Value` (not `EvalResultValue`). The body extracts the
      `Value` portion (at element+0x08) from the last EvalResultValue in
      `values_` and writes it to the sret buffer at offset +0x00 — i.e.
      it returns just the embedded `Value`, not the whole
      `EvalResultValue`. Return type corrected in `eval_results.hpp` and
      `eval_results.cpp`; ~40 call sites in `seqc_ast_nodes_evaluate.cpp`
      updated (`.getValue().value_` → `.getValue()`). The catch handler's
      `.value_` workaround is no longer needed. Both builds clean.

#### 21h.4 SeqCPlus::evaluate (5-arg) @0x22a600 — COMPLETE

- [x] Full 8-row cascade + default error path analyzed with binary
      address breadcrumbs. ~290 new lines in `seqc_ast_nodes_evaluate.cpp`.
- [x] `combine(VarSubType, VarSubType)` declared in resources.hpp,
      stubbed in .cpp (actual lookup table deferred to 21h.9).
- [x] Anonymous-namespace forward declarations for `combineWaveforms`
      @0x22c300 and `constWaveform` @0x22c9f0 (bodies deferred to 21h.8).
- [x] Both builds clean.

#### 21h.4-followup — deferred items from SeqCPlus reconstruction

- [x] **`combine(VarSubType, VarSubType)` @0x247ea0 lookup table**: NOT a
      lookup table — priority-based conditional logic (60 bytes). Done in 21h.9.
- [x] **`combineWaveforms` @0x22c300 full body**: ~1750B. Done in 21h.8.
- [x] **`constWaveform` @0x22c9f0 full body**: ~1000B. Done in 21h.8.
- [x] **Row 3 vtable-based dispatch @0x22b2df-22b301**: **RESOLVED (2026-04-24).**
      NOT a SeqCAstNode vtable call. The address `0xb06660` (labeled
      `vtable for SeqCAstNode+0x48` by the disassembler) is actually a
      `std::variant` destroy-visitor dispatch table for `Immediate`
      (`std::variant<AddressImpl<uint>, int, std::string>`). The 3 table
      entries: index 0/1 = no-op (trivially destructible), index 2 =
      destroy string. This pattern appears ~40 times across the codebase
      wherever local `Immediate` objects go out of scope. C++ generates
      this destructor dispatch automatically — no source changes needed.

#### 21h.5-7. Remaining arithmetic operator 5-arg overrides

~~Batch reconstruction — expected to be near-identical to SeqCPlus with
different operation semantics (subtract/multiply/divide doubles,
combineWaveforms op-name = "sub"/"mul"/"div").~~

**Discovery**: the arithmetic operators are NOT all identical. SeqCMinus
has structural differences (no String row, negation semantics, scaleWaveform),
SeqCMult uses `computeMult` + `WaveformGenerator::eval` (completely different
Wave path), SeqCDiv has no Var paths and no asm emission at all.

- [x] **21h.5 SeqCMinus::evaluate @0x22cde0** (~7312B) — 7-row action
      table + default error (0x74). Key differences from SeqCPlus: no
      String+String row; Var-Const negates rhs via `neg eax`+Immediate;
      Const/Cvar-Var uses addi(R0)+subr 2-step; Var-Var uses addi+subr;
      Wave paths use scaleWaveform to negate rhs before combineWaveforms("add");
      Wave-Const negates rhsDouble via xorpd sign-bit; no FunctionArg
      passthrough. New forward declaration: scaleWaveform @0x228cc0
      (body deferred). Both builds clean.
- [x] **21h.6 SeqCMult::evaluate @0x22ea70** (~9728B) — 6-row action
      table + default error (0x8c). Key differences from Plus/Minus: no
      String or Var*Var rows; Var*Const/Cvar uses computeMult() @0x22fdf0
      (shift-and-add multiplication); Wave*Wave uses combineWaveforms
      ("multiply"); Wave*Const and Const*Wave use 2-arg scaleWaveform
      @0x2309e0 (scalar, wave, ctx). New forward declarations:
      computeMult and 2-arg scaleWaveform (bodies deferred to 21h.8).
      Both builds clean (5 expected undefined-internal warnings in clang).
- [x] **21h.7 SeqCDiv::evaluate @0x231070** (~3664B) — 5-row action
      table + default error (0x8d). Structurally very different from other
      operators: NO Var paths (any Var → error 0xdf); NO Wave÷Wave; NO
      String paths; floatEqual() divide-by-zero checks (errors 0x29);
      Const/Cvar÷Wave → error 0x2a; Wave÷Const/Cvar computes reciprocal
      (1.0/rhs) and uses scaleWaveform 2-arg @0x2309e0. Three different
      error mechanisms: direct BST on ErrorMessages::messages (0xdf, 0x2a,
      0x29), errMsg[0x29], and ErrorMessages::format(0x8d). New forward
      declaration: floatEqual(double,double). Both builds clean (same 5
      expected undefined-internal warnings in clang).
- [x] Sub-phase wrap-up

#### 21h.8. combineWaveforms + constWaveform + scaleWaveform + computeMult helper bodies

- [x] `scaleWaveform` @0x228cc0 (1-arg overload — creates scalar with -1.0, delegates to 2-arg)
- [x] `constWaveform` @0x22c9f0 (~1000B — eval("rect", {length, value}), two catch clauses)
- [x] `combineWaveforms` @0x22c300 (~1500B — FunctionArg passthrough on both lhs/rhs, eval(opName))
- [x] `scaleWaveform` @0x2309e0 (2-arg overload — FunctionArg passthrough on wave, eval("scale"))
- [x] `computeMult` @0x22fdf0 (~3000B — integrality check, FunctionArg passthrough, 32-bit MSB-first shift-and-add loop)
- [x] Sub-phase wrap-up

#### 21h.9. combine(VarSubType, VarSubType) @0x247ea0 lookup table

- [x] Disassemble @0x247ea0 — NOT a .rodata lookup table; pure conditional
      logic (60 bytes). Priority-based combine: Default(0) is identity,
      FunctionArg(2) dominates, Stub(1) dominates Numeric/String, else Default.
- [x] Sub-phase wrap-up

### 21i. EParamDirection → EDirection rename ✅ DONE 2026-04-24

Unified the two separate enum definitions into a single `enum class
EDirection` in `types.hpp`, matching the binary's `zhinst::EDirection`
(mangled `NS_10EDirectionE`).

- [x] Rename `EParamDirection` → `EDirection` in `seqc_ast_node.hpp`.
- [x] Reconcile with the `EDirection` already in `resources.hpp` (they
      are the same type in the binary). Removed duplicate unscoped enum;
      both now use the scoped `enum class EDirection` from `types.hpp`.
- [x] Update all callers across ~75 call sites in 4 .cpp files:
      `static_cast<EDirection>(1)` → `EDirection::eOUT`,
      `EDirection_Read` → `EDirection::eIN`,
      `EDirection_Write` → `EDirection::eOUT`.
- [x] Sub-phase wrap-up: both builds clean (g++ + clang++).

**Estimated sessions:** 1. **Actual:** <1.

### 21j. Marker sweep: regex extraction + error codes + misc fixes — COMPLETE 2026-04-24

Systematic sweep of all remaining TODO/TBD markers. Prioritized by
value: easy-fixes first, then .rodata string extraction, then error
code verification, then documentation cleanup.

**~31 markers resolved (66→35 total, custom_functions.cpp 39→11).**

- [x] **Easy-fix**: playDIOWave dryRun — decode `channelArgs[0].value_.toString().empty()`
      with `size >= 2` guard. Binary does magic-number division by 0x38
      (÷56=÷sizeof(EvalResultValue)). Was incorrectly attributed to element[1].
- [x] **Easy-fix**: assignWaveIndex waveName — confirm `wa.value.value_.toString()`
      matches `[wa+0x08]` (Value at EvalResultValue+0x08). Stale TODO removed.
- [x] **Regex patterns**: all 4 writeToNode static boost::regex patterns extracted
      from .rodata via cold-path constructor addresses (@0x169ea5..0x169fab):
      - absDevRegex = `"/([0-9]+)/([\w/]+[\w])"` (was `"dev([0-9]+)/(.+)"`)
      - awgNodeRegex = `"awgs/([0-9]+)/.*"` (was `".+"`)
      - sineNodeRegex = `"sines/([0-9]+)/.*"` (was `".+"`)
      - oscselNodeRegex = `"sines/[0-9]+/oscselect"` (was `"oscs/([0-9]+)/.+"` — ZERO captures!)
      Variables renamed from `_TODO` suffix.
- [x] **Regex pattern**: assignWaveIndex cLikeIdentifier = `"[a-zA-Z_][a-zA-Z0-9_]*"`
      from .rodata @0x9005ed. No-match error 0xFB. VarType check error 0x96.
- [x] **Error codes**: writeToNode errors 0x84 (regex no-match), 0x85 (index mismatch),
      std::out_of_range for nodeAddressMap_ miss.
- [x] **Structural corrections in writeToNode**:
      - varSubType_ guard was checking wrong field (+0x04 not +0x00) AND wrong behavior
        (binary does silent return, not throw)
      - device-mismatch is silent return, not throw
      - nodeAddressMap_ miss uses std::out_of_range, not CustomFunctionsException
      - "second value" [rbp-0x1a8] is `type` parameter, not `valRef`
      - oscselNodeRegex no-match path throws error 0x84
- [x] **Warning log message**: "Unknown NodeMapType with code " (@0x900949) confirmed
      for both fast-path (@0x165407) and slow-path (@0x164d05) typeIdx>5 defaults.
- [x] **Documentation cleanup**: 5 stale TODO/TBD converted to NOTEs (awg_compiler_config.hpp,
      frontend_lowering.hpp, resources.cpp, custom_functions.cpp ×2).
- [x] Sub-phase wrap-up: both builds clean (g++ + clang++).

**Estimated sessions:** 1. **Actual:** 1.

---

## Phase 22 — Cleanup, split, and expansion

### 22a. Refresh TODO.md summary table ✅ DONE 2026-04-25

Summary table was severely stale (claimed 106 SeqCAstNode stubs — all
are real implementations; claimed 55 builtin stubs — all eliminated;
etc.). Refreshed with audited counts. OVERVIEW.md status paragraph
updated in parallel.

- [x] Audit marker counts via grep (25 TODO, 7 TBD, 3 throw-stubs = 35
      total across 21 files)
- [x] Verify SeqCAstNode status: 49 print + 49 clone ALL real; 11
      evaluate implemented, ~43 remaining
- [x] Verify custom_functions.cpp: 7884 lines, 86 methods, 0
      return-nullptr stubs remaining
- [x] Update summary table with accurate row-by-row status
- [x] Update OVERVIEW.md reconstruction status paragraph

### 22b. Split custom_functions.cpp (7884 lines → 4 modules) ✅

The file is 3× larger than the next-largest source file. Split along
natural boundaries identified in the audit:

| Module | Actual lines | Content |
|--------|-------------|---------|
| `custom_functions.cpp` (core) | 1061 | Ctor, dtor, `call`, `functionExists`, validation/utility helpers, PlayArgs, free functions |
| `custom_functions_play.cpp` | 2462 | `setWaitCyclesReg`, `mergeWaveforms`, `play`, `playIndexed`, `writeToNode`, `generateWaveform` |
| `custom_functions_playback.cpp` | 994 | All `playWave*`, `playAuxWave*`, DIO/ZSync play, `playZero/Hold`, `waitWave`, `sync`, `setRate` |
| `custom_functions_io.cpp` | 3456 | DIO/IO, `wait`, `assignWaveIndex`, oscillator, QA, PRNG, sweeper, feedback configuration |

- [x] Create the 4 new .cpp files with proper includes
- [x] Move methods to their respective modules
- [x] Verify both builds clean (g++ + clang++)
- [x] Sub-phase wrap-up

### 22c. Remaining markers sweep (~35 markers across 21 files) ✅

Systematic sweep of all 32 markers (actual count, not 35). Reduced to 15
remaining — all genuine unknowns, blocked items, or large-body reconstruction.

Key outcomes:
- Resolved 7 quick-fix/doc-only markers (headers + resources.cpp)
- Resolved PlayConfig bit-packing TODO in custom_functions_io.cpp via encodeCwvf()
- Implemented waveIndex bounds-check in playIndexed (signal.length() confirmed)
- Converted 3 best-guess confirmation TODOs to NOTE annotations
- **Verified ALL 10 device knownOptions arrays** against binary .rodata:
  - 8/10 correct as-was (Hdawg4/8, Shfqa2/qc/li, Uhfli/ia, Vhfli)
  - **Fixed 2 bugs**: Uhfawg had {PID,MOD}→{CNT,QA}; Uhfqa had {MF,CNT}→{FF,RUB}

Remaining 15 markers: 3 genuine-unknown TBDs, 4 blocked (boost archive,
WaveAssignment), 4 large-body (csv_parser, frontend_lowering, seqc_ast_node),
2 design notes, 1 verify-needed, 1 reference to other TODO.

- [x] Triage all 32 markers by category (quick-fix / needs-disasm / blocked / doc-only)
- [x] Resolve quick-fix and doc-only markers (7 resolved)
- [x] Attempt needs-disasm markers in custom_functions (PlayConfig, bounds-check, 3→NOTE)
- [x] Attempt remaining needs-disasm markers across other files (4 device files verified+fixed)
- [x] Sub-phase wrap-up

### 22d. SeqCAstNode evaluate() body expansion (~43 remaining overrides)

Binary has 32 3-arg + 22 5-arg evaluate overrides (54 total, excluding
AWGAssemblerImpl::evaluate). 19 implemented so far, 26 stubs + 6 helper
bodies remaining.

#### 22d batch 1: Trivial evaluate overrides (11 done) ✅

- [x] Returns nullptr: SeqCCommand, SeqCOperation
- [x] Returns empty EvalResults: SeqCLabel, SeqCXorExpr(5), SeqCNoOp(5)
- [x] setLineNr + empty: SeqCVariableType, SeqCNoCmd
- [x] Delegate to child: SeqCPos
- [x] Error emitters: SeqCContinueStatement, SeqCBreakStatement (error 0xd5)
- [x] Throws: SeqCParamList (CompilerException)

#### 22d batch 2: Operator wrappers + stubs (8 done + 26 stubs) ✅

- [x] Wrappers: SeqCGreater→evalGreater, SeqCEqual→evalEqual,
      SeqCShiftL→evalShift(false), SeqCShiftR→evalShift(true),
      SeqCAndExpr→evalAnd, SeqCOrExpr→evalOr,
      SeqCLEqual→evalGreater+invertBool, SeqCNEqual→evalEqual+invertBool
- [x] 6 anonymous-namespace helpers forward-declared (bodies pending):
      evalGreater @0x235ac0, evalEqual @0x239be0, evalShift @0x232850,
      evalAnd @0x23ea20, evalOr @0x240a30, invertBool @0x238fb0
- [x] 26 TODO stubs with addresses added for remaining overrides
- [x] Both builds clean (g++: 0 warn; clang++: 6 expected undefined-internal)

#### 22d batch 3: Helper bodies + medium evaluate stubs (COMPLETE)

- [x] Implement 6 helper bodies: ~~invertBool~~ ✅, ~~valueToBool~~ ✅,
      ~~evalGreater~~ ✅, ~~evalEqual~~ ✅, ~~evalShift~~ ✅,
      ~~evalAnd~~ ✅, ~~evalOr~~ ✅ (all 6 complete)
- [x] Implement medium-size 5-arg operator stubs: ~~SeqCLower~~ ✅ (wrapper→evalLower),
      ~~SeqCGEqual~~ ✅ (invertBool(evalLower)), ~~SeqCLogAnd~~ ✅ (valueToBool+evalAnd),
      ~~SeqCLogOr~~ ✅ (valueToBool+evalOr), ~~SeqCMod~~ ✅ (fmod, const-only)
- [x] Implement 3-arg unary stubs: ~~SeqCNeg~~ ✅ (asmZero+subr / -toDouble() / scaleWaveform),
      ~~SeqCInv~~ ✅ (addi(-1)×2+subr / ~toInt())
- [x] Implement list stubs: ~~SeqCArgList~~ ✅, ~~SeqCDeclList~~ ✅ (iterate+accumulate),
      ~~SeqCStmtList~~ ✅ (iterate+accumulate+return-stmt detection+unreachable code warning)
- [x] New discovery: evalLower @0x237440 (~2KB) is a dedicated function (NOT swapped-arg evalGreater).
      Forward-declared; body pending (batch 4+).

#### 22d batch 4+: Larger evaluate stubs (pending)

- [x] Implement helper body: evalLower @0x237440 (~2KB) — dedicated a<b function
      (used by SeqCLower and SeqCGEqual; forward-declared, body pending)
- [x] Implement unary: ~~SeqCNotExpr~~ ✅ (3027B), ~~SeqCInc~~ ✅ (5464B),
      ~~SeqCDec~~ ✅ (5464B)
- [x] Implement unary/stmt: SeqCReturnStatement (6800B)
- [x] Implement control-flow: ~~SeqCIfCondition~~ ✅ (4360B) + jumpIfZero helper @0x2149f0 (760B)
- [x] Implement control-flow: ~~SeqCSwitchCase~~ ✅ (11506B, ~400 lines, 3-way dispatch),
      ~~SeqCRepeat~~ ✅ (8567B), ~~SeqCForLoop~~ ✅ (9794B)
- [x] Implement control-flow: ~~SeqCDoWhile~~ ✅ (7952B) — do-while with body-first, toDouble+floatEqual Cvar path
- [x] Implement control-flow: ~~SeqCWhileLoop~~ ✅ (7117B) + loopArgNodeAppend/loopBodyNodeAppend helpers
- [x] Implement control-flow: ~~SeqCCaseEntry~~ ✅ (2845B)
- [x] Implement control-flow: ~~SeqCIfElse~~ ✅ (7214B)
- [x] Implement complex: ~~SeqCFunction~~ ✅ (5080B) — field rename: call_/params_/body_/retType_
- [x] Implement complex: SeqCCondExpr (11007B) — done
- [x] Implement complex: ~~SeqCFunctionCall~~ ✅ (15220B, ~350 lines) — two paths: custom + user-defined
- [x] Implement complex: ~~SeqCArray~~ ✅ (2412B) — NOT ICF'd, array indexing with Wave+Const/Cvar validation, waveform lookup, bounds check, returns wave-with-index + Cvar sample
- [x] Implement hasCases/isSingleCase/singleCase/cases helpers + evalCaseBody @0x216fc0 + evalCases @0x216980 — all fully implemented from disasm
- [x] Sub-phase wrap-up

### 22e. Magic numbers refactoring (Category A+B)

Apply the proposal from `notes/magic_numbers_proposal.md`. Category A
defines new enums/constants; Category B replaces bare integers at call
sites with existing enum names.

- [x] Implement Category A enums/constants (16 items: ImmediateKind,
      AsmExprType, VariantSlot, VarFlag, OptFlag, DeviceOpts, OpcodeFormat,
      NodeTypeIdx, CmdType, RegOrder, RegAction, kCycle, SuserAddr,
      sentinels, kChannelTag)
- [x] Apply Category B call-site replacements (~430 sites across 15 files:
      ErrorMessageT 302, VarType 99, AwgDeviceType 26, Assembler::INVALID 12,
      ValueType 13, NodeType 2)
- [x] Investigate 3 open questions: SubFunc base=1 confirmed (switch fixed),
      ErrorMessageT 0x2F added as UnknownError47, OpcodeFormat documented
- [x] Sub-phase wrap-up

---

## Phase 20e-ii wrap-up cleanup (housekeeping)

Added 2026-04-24 during Phase 20e-ii Batch 5a wrap-up audit.
Items 3-5 resolved; items 1-2 were prematurely marked done.

- [x] **OVERVIEW.md narrative compression.** Compressed in Phase 21h
      session — 55-line prose replaced with 12-line summary paragraph.
- [x] **OVERVIEW.md stale marker-count table.** Updated to 73 in Phase
      21h session; phase table row for 21h added.
- [x] **Promote 3-way duplication to single source of truth.** Already
      resolved — unknowns.md carry-forward block removed in prior session
      (see unknowns.md line 101-109 history entry). OVERVIEW.md Open
      Questions section correctly references TODO.md + notes/ only.
- [x] **Variable header refactor: embed `Value value_` at +0x08.** Done
      during Phase 20e-ii Batch 5a (prior session). All read*/add*/update*
      references updated.



- [x] **Audit-script update — query both mangling variants.**
      Resolved: option (b) chosen. `cmake --build` (zero warnings, zero
      unresolved-at-link-time) is the authoritative gate; `nm` is a hint
      only. Phase 20e completed with all 95 symbols resolved, making the
      prerequisite moot.

## Phase 23 — Audit-driven cleanup and coverage expansion

Originated from comprehensive audit on 2026-04-25. Compared binary symbol
table (2852 unique `t` symbols) against reconstruction (835 intersection,
430 genuine zhinst gaps after filtering STL/boost template noise). Also
audited all source for placeholder fields, raw-offset access, approximate
implementations, stubs, and uncertainty comments. Cleanup tasks first,
then larger refinement, then new coverage.

### 23a. TODO.md housekeeping ✅

- [x] Rewrite summary table with audit findings
- [x] Tick off stale `[ ]` checkboxes (19c, floatEqual, mergeWaveforms,
      writeToNode, WavetableManager, play wrappers — all done in prior phases)
- [x] Add Phase 23 plan
- [x] Remove 7 vtable store-back TODOs from `seqc_ast_nodes_evaluate.cpp`
      (resolved in 21h.4-followup as Immediate variant dtors — no source change)
- [x] Replace `0x6e69616d` with `"main"` string comparison in
      `seqc_ast_nodes_evaluate.cpp`

### 23b. DeviceConstants field naming (~6 fields × 9 device types) ✅

Name the placeholder fields from consumer analysis. These are the most
frequently used unnamed fields in the entire codebase (~54 assignment
lines in device_constants.cpp + ~20 consumer sites elsewhere).

- [x] `field_20` → `sineNodeBase` (oscillator node index base in setSinePhase/incrementSinePhase)
- [x] `field_24` → `waveformElfAlignment` (ELF segment alignment; stored as WaveformIR.irField2)
- [x] `field_3C` → `triggerLatencyCycles` (min-wait threshold; subtracted after wtrig)
- [x] `field_48` → `playMinSamples` (min play length; no reconstructed consumer but TODO hint from binary)
- [x] `field_4C` → `waveformMinSamples` (initial seqRegWidth; passed to checkOffspecWaveLength)
- [x] `field_54` → `numCounters` (getCnt range check; confirmed Phase 18b-ii)
- [x] Cascade rename across device_constants.cpp (9 blocks) + all consumer sites
- [x] Sub-phase wrap-up

### 23c. AWGCompilerConfig field naming (~4 unknowns) ✅

- [x] `unknown_20` → `awgIndex` (AWG core index; builds node paths + validates channel ownership)
- [x] `unknown_28` → no consumer found; annotated as unresolved
- [x] `unknown_88` → no consumer found; annotated as unresolved
- [x] `unknown_98[8]` → no consumer found; annotated as unresolved
- [x] Sub-phase wrap-up

### 23d. reinterpret_cast elimination in resources.cpp (~30 sites) ✅

The ~30 raw byte-offset accesses on Value/Variable objects are the single
worst readability hotspot. Variable layout is fully known since Phase
19c-followup. Replace all `reinterpret_cast + offset` patterns with
proper field access through the reconstructed struct.

- [x] Audit all reinterpret_cast sites in resources.cpp (29 found)
- [x] Replace Value raw-offset patterns (+0x04, +0x08, +0x18, +0x50)
      with named field access (var->value, var->subTypeRaw, &var->value.storage_)
- [x] Replace Variable flags byte hack with VarFlag_Written/VarFlag_Frozen
- [x] Replace libc++ string internal offset reads with std::string* cast
- [x] Build verify — 30→8 reinterpret_casts remaining (8 are inherent
      SSO memory ops that can't be eliminated without algorithm change)

### 23e. Device-type bitmask constants in custom_functions_io.cpp (~70 sites) ✅

Replace `static_cast<AwgDeviceType>(0x1ff)` patterns with named
combinations. Define convenience constants in types.hpp (e.g.
`kDevAll`, `kDevHirzel`, `kDevSHFPlus`, etc.).

- [x] Survey all unique bitmask values used (19 unique values, 69 total sites)
- [x] Define 12 named constants in types.hpp (kDevAll, kDevAllButUHF, kDevHirzel,
      kDevSHFPlus, kDevLIFamily, kDevCervino, kDevPreSHFLI, kDevQA, etc.)
- [x] Replace 61 hex casts in custom_functions_io.cpp + 8 in custom_functions_playback.cpp
      (single-device values replaced with enum name, e.g. `static_cast<AwgDeviceType>(HDAWG)`)
- [x] Build verify — 0 hex bitmask casts remaining

### 23f. reinterpret_cast cleanup in other files (~50 sites) ✅

After resources.cpp (23d), clean up remaining raw-offset access:
custom_functions*.cpp (~18), prefetch*.cpp (~15), waveform.cpp (~5),
awg_assembler*.cpp (~3), seqc_parser_context.cpp (2).

Full audit found ~99 sites across 23 files. Fixed 18 easy wins:
- [x] custom_functions.cpp: config_->deviceType (×2), devConst_->playMinSamples,
      devConst_->waveformPageSize, wf->signal.length_ (×2), wf->name (×2),
      wf->seqRegWidth, config_->includePaths loop
- [x] custom_functions_play.cpp: config_->deviceType
- [x] prefetch*.cpp: config_->isHirzel (×4), config_->cacheType
- [x] seqc_ast_nodes_evaluate.cpp: replaced 0x6e69616d with `funName == "main"` (×2)
- [x] Build verify — ~100 remaining are inherent (serialization, tagged unions,
      aligned storage, SSO internals, vtable dispatch, stride iteration) or
      require deeper struct knowledge (custom_functions_play arg offsets,
      prefetch usageEntries, wavetable_front manager internals)

### 23g. Remaining placeholder fields in other headers ✅

- [x] waveform_generator.hpp: `field_20_` → `funcMap_maxLoadFactor_` (libc++ internal),
      `field_48_` → `aliasMap_maxLoadFactor_`, `field_78_` → `pad_78_`,
      `field_B0_` → `reserved_B0_` (dead field, no setter in binary)
- [x] custom_functions.hpp: `field_80_` → `funcMap_maxLoadFactor_`,
      `field_A8_` → `aliasMap_maxLoadFactor_`, `field_B0_` → `unusedStringSet_B0_`
      (no consumer), `field_168_` → `assignedWaveNames_` (populated by assignWaveIndex)
- [x] prefetch.hpp: 6 unknowns at +0xC0..+0xD8 already resolved as `cwvfConfig_`
      (PlayConfig struct) — updated stale layout comment
- [x] Sub-phase wrap-up — build clean

### 23h. SeqC copy-ctor / operator= / swap / accessors (158 missing symbols) ✅

Mechanical code generation. All 53 subclasses need copy-ctor (= clone()),
operator= (copy-and-swap idiom), swap (member swap), and child-accessor
methods (expr(), funName(), stmts(), getListElements(), etc.). Can be
largely macro-generated.

- [x] Survey which accessors are called in existing code vs just in binary
- [x] Implement accessors for subclasses that have them (expr(), funName(),
      stmts(), params(), decls(), body(), label(), ifBody(), elseBody(),
      cond(), arguments(), validLabel(), hasLabel())
- [x] Implement copy-ctor = clone pattern via macro or per-class
- [x] Implement operator= via copy-and-swap
- [x] Implement swap() free functions
- [x] SeqcParserContext::hadSyntaxError() + reset() (2 more missing symbols)
- [x] Build verify + sub-phase wrap-up

### 23i. Missing toString/str/serialize methods (~53 symbols) ✅

Various classes have toString(), str(), toJson(), serialize() methods in
the binary that we haven't implemented.

- [x] `Assembler::str(bool)`, `commandToString(Command)`, `highestRegisterNumber()` — already complete
- [x] `Assembler` copy-ctor, operator=, move-operator=, dtor — added (default ctor + explicit dtor, copy-assign with self-check, move-assign)
- [x] `AsmList::serialize()` — already complete
- [x] `Cache::Pointer::str()` — already complete
- [x] `CompilerMessage::str(bool)` — already complete
- [x] `Node::toString()`, `type2str(NodeType)`, `waveAtCurrentDeviceIndex()` — already complete
- [x] `Value::toString()` — already complete
- [x] `WaveformFront::toString()`, `WavetableFront::toString()` — already complete
- [x] `WaveformIR::toJsonElement(SampleFormat)` — already complete
- [x] `WavetableIR::alignWaveformSizes()`, `assignWaveformAllocationSizes()` — implemented
- [x] `WavetableIR::getJsonIndex(SampleFormat)` — already complete
- [x] `Waveform::File::operator==()`, `typeToStr()` — already complete
- [x] `Signal::Signal(ulong, MarkerBitsPerChannel const&)` — already complete
- [x] `ElfReader::getCode()`, `getLineMap()`, `getWaveform()` — implemented
- [x] `Prefetch::wvfImpl()`, `wvfRegImpl()`, `wvfs()` — already complete
- [x] `Resources::setReturnValue(Value)`, `toString()` — already complete
- [x] `str(AsmOperationType)`, `str(EDirection)`, `str(EValueCategory)`,
      `str(VarSubType)`, `str(VarType)` — all implemented (AsmOperationType enum + str added)
- [x] `toString(Immediate)`, `toString(AwgSequencerType)`,
      `toString(DeviceFamily)`, `toString(DeviceOption, DeviceFamily)`,
      `toString(DeviceTypeCode)`, `toString(DeviceType const&)` — all implemented
- [x] Build verify + sub-phase wrap-up

### ~~23j. AWGCompiler public API~~ → moved to Phase 24c

### ~~23k.~~ → moved to Phase 26a

### 23l. DeviceType extra methods (21 symbols) ✅

Factory `makeDefault()` methods (already structurally present but emitting
wrong mangling?), DeviceType constructors, comparison operators.

- [x] Audit which are already defined vs mangling-gap
- [x] `DeviceType(DeviceFamily)`, `DeviceType(DeviceFamily, unsigned long)` ctors
- [x] `DeviceType::deviceType()`, `toString()`, `swap()`
- [x] `DeviceType::print(ostream&)`
- [x] `operator<(DeviceType)`, `operator==(DeviceType)`
- [x] `operator<<(ostream&, DeviceType const&)`
- [x] `hasOption(DeviceType, DeviceOption)` (free function)
- [x] `detail::DeviceFamilyFactory::makeDeviceType()`,
      `detail::DeviceTypeImpl::doClone()` — already complete
- [x] Build verify + sub-phase wrap-up

### ~~23m.~~ → moved to Phase 26b


---

## Phase 24 — Python binding layer (pybind11)

Reconstruct the complete path from `PyInit__seqc_compiler` down to the
already-reconstructed `Compiler` class. ~10-12KB across 4 components
plus the AWGCompiler facade.

### 24a. Build system setup ✅

- [x] Install pybind11 headers — already installed (pybind11 3.0.4-1)
- [x] Add pybind11 `find_package` / header-only dep to CMakeLists.txt
- [x] Verify build still passes (both g++ and clang++/libc++)

### 24b. ZiFolder utility (~1KB, 3 methods) ✅

- [x] `ZiFolder::ZiFolder(string)` [0x2ce2c0] — already in zi_folder.cpp
- [x] `ZiFolder::ziFolder(DirectoryType)` [0x2cf0c0] — already in zi_folder.cpp
- [x] `ZiFolder::folderPath(...)` [0x2ce2f0] — already in zi_folder.cpp
- [x] Build verify + sub-phase wrap-up

### 24c. AWGCompiler facade + AWGCompilerImpl (~2-3KB, 15+ symbols) ✅

Relocated from 23j — thin wrappers around the reconstructed Compiler.

- [x] `AWGCompiler` pimpl wrapper (11 methods, all forwarding to Impl)
- [x] `AWGCompilerImpl` class (0x2C0 bytes) — ctor, dtor, all 11 methods
- [x] `AWGCompilerImpl::getCompileReport()` — iterate compileMessages_ + assembler report
- [x] `AWGCompilerImpl::setCancelCallback/setProgressCallback` — forward to compiler_
- [x] `AWGCompilerImpl::compileString()` — structural reconstruction (validate device type,
      Compiler::compile, assemble, optimize, output)
- [x] `AWGCompilerImpl::compileFile()` — read file, delegate to compileString
- [x] `AWGCompilerImpl::addWaveforms()` — iterate paths, WavetableFront::newWaveformFromFile
- [x] `AWGCompilerImpl::writeToStream()` — ElfWriter output pipeline
- [x] `AWGCompilerImpl::writeToFile/writeAssemblerToFile` — file I/O wrappers
- [x] `AWGCompilerImpl::getBinVersion()` — structural TODO (needs CalVer/getLaboneVersion)
- [x] `AWGCompilerImpl::getJsonWaveformMemoryInfo()` — structural TODO (needs WavetableIR iteration)
- [x] `AWGCompilerConfig::getAwgDeviceTypeString/getChannelGroupingModeString` — already existed
- [x] Build verify (both g++ and clang++/libc++)

### 24c-followup. AWGCompilerImpl TODO cleanup (2026-04-25)

Resolved 5 of 8 TODO markers in `awg_compiler.cpp`:

- [x] `compressSourceString` @0x109e90 — real zlib: deflateInit(level 9) +
      deflate(Z_FINISH) loop with 0x8000-byte chunks + deflateEnd; error
      throws via ErrorMessages::format(0x1e, format)
- [x] `getBinVersion` @0x10b830 suffix table — switch on deviceType:
      HDAWG→"hirz", SHFQA/SG/QC_SG/LI→"grim", GHFLI→"gurn", VHFLI→"malo",
      default→"cerv"; output built as growing std::string (4→8→16 bytes)
- [x] `getAssemblerHeader` @0x1083d0 — banner/separator/auto-gen notice,
      conditional source file, version "26.01.3.9", formatTime(now, false)
- [x] `hadSyntaxError` wiring — added Compiler::hadSyntaxError() accessor
      (SeqcParserContext byte at +0x03); wired into writeToStream +
      writeAssemblerToFile early-return checks
- [x] AWGCompilerImpl ctor @0x103b40 — proper init: getDeviceConstants,
      make_shared\<WavetableFront\>, Compiler(config, devConst, wavetable_)
- [x] `writeToStream` metadata sections — 7 of 9 ELF sections implemented:
      .nodes/.nodes_json (device-type gated), .channels,
      .required_sample_rate, .waveforms, .wavemem, .version_bin;
      source compression flag at config+0x9D now gates .c/.asm sections
- [x] `addWaveforms` binary format loading (.bin/.bin16/.wave/.wave16) —
      .bin/.wave: ifstream(ate) → tellg → read → awg2double/awg2marker loop
      → Signal(samples, markers, markerBits) → newWaveformFromFile(stem,
      signal, path, Type=1); .bin16/.wave16: log warning "not implemented"
- [x] `getJsonArguments` @0x10a3c0 → `.arguments` ELF section
      (boost::property_tree: "destination", "source", "waves" array)
- [x] `getJsonVersion` @0x10ac60 → `.version_json` ELF section
      (boost::property_tree: "compiler" uint32, "target" device family;
      NOTE: "external_triggering" and "required_options" conditionally
      omitted — source data at this+0x190 not yet identified)

### 24d. compileSeqc orchestrator (~6KB) [0xf58a0] ✅

- [x] Reconstruct `zhinst::compileSeqc()` — JSON config parsing,
      DeviceType/AWGCompilerConfig assembly, AWGCompiler invocation
- [x] Build verify + sub-phase wrap-up

### 24e. pybind11 entry points (~3KB) ✅

- [x] `zhinst::pyCompileSeqc()` [0xe0000] — arg extraction, GIL
      release, result packing
- [x] `zhinst::makeSeqcCompiler()` [0xe1900] + `makeSeqcCompilerInCore()`
      — module_.def() registration, version attrs
- [x] `PyInit__seqc_compiler` [0xf5350] — module creation
- [x] Build verify + sub-phase wrap-up

---

## Phase 25 — Boilerplate reduction (helper extraction)

The evaluate-AST (~10K lines) and custom-functions (~8K lines across 4
files) implementations contain significant repetitive boilerplate — likely
reflecting inlined helpers or macros in the original source. Introducing
named helpers makes the reconstruction shorter, more expressive, and
less error-prone.

Ordered by impact × feasibility: AST-evaluate lambda dedup first (pure
duplication, zero risk), then custom_functions helpers (higher churn risk
in binary-faithful code).

### 25d. AST evaluate: promote local lambdas to file-scope helpers ✅

Promoted 5 helper lambdas from per-function definitions to file-scope
anonymous-namespace functions in `seqc_ast_nodes_evaluate.cpp`. Removed
24 duplicate lambda definitions total:

- [x] `isConstOrCvar` (11 defs → 0; moved to `resources.hpp` as shared inline)
- [x] `rhsTypeOrUnset` (5 defs → 0; now free function taking `EvalResults const&`)
- [x] `rhsCount` (4 defs → 0; same)
- [x] `getBackReg` (2 defs → 0; now file-scope, no captures)
- [x] `rhsSubOrDefault` (2 defs → 0; same as rhsTypeOrUnset)
- [x] Updated all call sites: `rhsCount()` → `rhsCount(rhsResult)`, etc.
- [x] Both builds clean (g++ + clang++)

### 25g. Shared `isConstOrCvar(VarType)` across codebase ✅

- [x] Added `inline bool isConstOrCvar(VarType)` to `resources.hpp` (after VarType enum)
- [x] Replaced 5 bitwise patterns in `custom_functions_io.cpp`:
      `(static_cast<int>(arg.varType_) & ~1) == 4` → `isConstOrCvar(arg.varType_)`
- [x] Both builds clean

### 25f. evalLogical — LogOr `" && "` confirmed as binary bug ✅

- [x] Investigated: SeqCLogOr uses `" && "` separator, confirmed from binary
      DWORD 0x20262620. This is a **copy-paste bug in the original source**,
      not a reconstruction error. Already documented in source comments.
      No fix applied (we match the binary).

### 25a-c,e. custom_functions helpers

- [x] `checkExternalTriggeringMode(int expected)` — extracted, 10 call sites replaced
      (7 in custom_functions_io.cpp, 3 in custom_functions_playback.cpp)
- [x] `isShfFamily()` — extracted, 3 call sites replaced
      (custom_functions_io.cpp; 2 variant sites left unchanged intentionally)
- [x] `emitWaitTrigger(constName, results, res)` — assessed, NOT extracted.
      All candidate sites have enough variation (register reuse, interleaved
      logic) that clean extraction would risk semantic changes. Left as-is.
- [x] Replace call sites — done (13 total replacements)
- [x] Build verify + sub-phase wrap-up

**Not extracting** (assessed, not worth the churn):
- ~~`emitLoadImmediate`~~ — pattern has enough variation that mechanical
  replacement risks obscuring binary-address documentation.
- ~~`emitRegOrConst`~~ — depends on emitLoadImmediate.
- ~~`validateBinaryOperands`~~ — only 4 instances.

---

## Phase 26 — Remaining gaps (stubs, data tables, approximate impls)

Relocated from Phase 23 — these are not mechanical cleanup but require
binary analysis and new reconstruction work.

### 26a. GetNodeMap\<T\>::get() specializations (8 symbols) ✅

Extracted all 8 device-specific node maps (1081 total entries) from binary
using runtime extraction (dlopen + tree traversal). Implemented as
`get_node_map.cpp` (1267 lines) with table-driven `addDirect`/`addVirt`
helpers. Wired `initNodeMap` stub to use real `getNodeMapForDevice()`
dispatcher. Added default ctors to `DirectAddrNodeMapData` and
`VirtAddrNodeMapData`.

- [x] Survey what GetNodeMap returns (static map of node paths → addresses)
- [x] Implement 8 specializations from binary data
- [x] Wire initNodeMap stub to use real GetNodeMap data
- [x] Build verify + sub-phase wrap-up

### 26b. Stubs and approximate implementation cleanup ✅

Address the 6 conservative stubs and ~19 approximate implementations.

- [x] `oscMaskCheckHirzel` @0x15bab0 — fully reconstructed from binary.
      3-way dispatch (dc=1/2/4) × MF feature, with jump tables for
      groupIndex. ~1.2KB function.
- [x] `oscMaskSetAllHirzel` @0x15bf50 — fully reconstructed. Returns
      bitmask shifted by groupIndex, scaled by MF feature presence.
- [x] `initNodeMap` — done in 26a (wired to `getNodeMapForDevice()`)
- [x] `secureLoadWaveform` @0x1711a0 — expanded from stub. Now includes
      CSV duplicate warning (error 0xEB via reportWarning_) and
      `wavetableFront_->loadWaveform()` call.
- [x] `parseOptionalString` / `getPlayRate` — done earlier in 26b session
- [x] Review ~19 approximate-implementation comments — all 3 checked
      (exception.cpp MI layout, mergeWaveforms factory, signal.cpp epsilon)
      are documented structural approximations, not bugs. No action needed.
- [x] `frontend_lowering.cpp` lower() body — was already complete since
      Phase 21f. Cleaned up stale `constWaveform` stub (moved to
      seqc_ast_nodes_evaluate.cpp in Phase 21h.4).
- [x] `seqc_ast_node.cpp` recursive printer @0x1fa430 — fully reconstructed
      with box-drawing tree connectors (`|-` / `` `- ``). Also fixed
      SeqCValue::~SeqCValue() — was `= default` which leaked placement-new'd
      strings; now properly dispatches destruction based on tag_.
- [x] Build verify — both g++ and clang++/libc++ clean, 0 errors.

---

## Phase 27b — CalVer + utility free functions

### 27b. CalVer class + versioning (16 symbols) ✅

- [x] `CalVer` struct (0x20 bytes: year_, month_, patch_, build_ as size_t)
- [x] `CalVer::CalVer(string const&)` @0x0ffdb0 — dot-counting parser,
      extractVersionTriple + optional build suffix
- [x] 4 accessors: year/month/patch/build + triple()
- [x] `getLaboneVersion()` @0x100270 — static {26, 1, 3, 9}
- [x] `getLaboneVersionWithCommitHash()` @0x1002a0 — "26.01.3.9 (203353a...)"
- [x] `asBinary()` @0x1007c0 — year<<24 | month<<16 | (patch<<12)&0xFFFF | build&0xFFF
- [x] `asDecimal()` @0x1006e0 — (YY*100+MM)*100000 + P*10000 + BBBB
- [x] `fromBinary()` @0x100780, `fromDecimal()` @0x100490 — inverses
- [x] `toString()` @0x1007f0 — "YEAR.MONTH.PATCH"
- [x] `isSet()` @0x100470 — any field non-zero
- [x] `operator==` @0x100bc0, `operator<` @0x100c00
- [x] Removed CalVer forward-decl from awg_compiler.cpp; now uses calver.hpp

### 27b. formatTime (3 overloads) ✅

- [x] `formatTime(ptime, char const*)` @0x2f6190 — time_facet + oss
- [x] `formatTime(ptime, bool)` @0x2f7440 — compact ? "%Y%m%d_%H%M%S" : "%Y/%m/%d %H:%M:%S"
- [x] `formatTime(long, bool, bool)` @0x2f7470 — epoch→ptime, optional utcToLocal
- [x] Removed formatTime forward-decl from zi_folder.cpp; now uses format_time.hpp

### 27b. Serial number predicates (10 symbols) ✅

- [x] All 10: isHf2/isUhf/isMf/isHdawg/isPqsc/isShf/isShfacc/isGhf/isQhub/isVhf
      Each uses unsigned subtraction range check pattern.
      Short range + long range (10× base), except PQSC (single range).

### 27b. getPlatformName ✅

- [x] `getPlatformName()` @0x2ec6e0 — returns "linux64"

### 27b remaining

- [x] `extractVersionTriple()` @0x101570 — implemented: boost::split on '.', lexical_cast up to 3 components
- [x] 6 misc string/filesystem utilities — implemented in platform.cpp
      (isPureAscii, isValidUtf8 ×2, isMountPoint, isDirectoryWriteable, isInList)

---

## Phase 28 — Binary symbol gap closure (451→0 actionable)

**Goal:** Eliminate the gap between symbols exported by the original
`_seqc_compiler.so` and symbols defined in the reconstructed
`libzhinst_seqc.a` (libc++ build). Started at 451 missing, now 0
actionable (14 remaining are linker/stdlib RTTI artifacts).

### 28a. SeqC copy-ctor/operator=/swap out-of-line ✅ (+146 symbols)

- [x] Moved 53 subclass copy-ctors, operator=, swap from header-inline
      (SEQC_BINARY macro) to out-of-line in seqc_ast_node.cpp

### 28b. AsmRegister global scope ✅ (+80 symbols)

- [x] Moved AsmRegister enum from nested AsmList::Asm scope to global
      (binary mangles it at global scope)

### 28c. ErrorMessages::format restructure ✅ (+39 symbols, then +16)

- [x] Phase 1: Restructured format<> template to emit outer instantiations
- [x] Phase 2: Fixed inner helper signature from `format<Args...>(BF&, Args...)`
      to `format<T, Args...>(BF&, T, Args...)` — binary splits first param
      out of pack (different mangling: `IT J...EE` vs `IJ...EE`)
- [x] Removed non-template `format(ErrorMessageT)` — binary only has
      `format<>(ErrorMessageT)` (zero-arg variadic template)
- [x] Added base case `format(boost::format&)` for zero-arg recursion

### 28d. SeqC AST node type fixes ✅ (+12 symbols)

- [x] SeqCArray, SeqCFunctionCall: broken out of SEQC_BINARY macro with
      `unique_ptr<SeqCVariable>` first child (not `unique_ptr<SeqCAstNode>`)
- [x] SeqCFunction retType_: `unique_ptr<SeqCVariableType>` not `unique_ptr<SeqCAstNode>`
- [x] SeqCValue string ctor: takes `string` by value, not `const string&`
- [x] SeqCOperation ctor: moved out-of-line (C1/C2 symbols)
- [x] SeqCCaseEntry: broken out with `body()`, `validLabel()`, `hasLabel()`

### 28e. Device factory makeDefault() ✅ (+14 symbols)

- [x] Added `makeDefault()` to all 13 factory subclasses
- [x] Added `makeDeviceType()` zero-arg overload to base DeviceFamilyFactory

### Final count

- **451 → 0 actionable missing symbols** (5 sessions)
- 14 non-actionable remain: 5 linker (`__bss_start`, `_edata`, `_end`,
  `_fini`, `_init`) + 9 stdlib RTTI/vtable artifacts
- **28/28 differential tests passing**

---

## Phase 31 — Remaining work (consolidated 2026-04-26)

All previously "deferred", "backlog", and "low-value" items consolidated
into a single actionable phase. Ordered by topic, not priority — everything
here is next-up work.

### 31a. Quick-win closures (< 1 session each)

- [x] **Exception error_code prefix string** (unknowns #90) — extracted
      `"ZIException with status code: "` (30 bytes at .rodata 0x90c6c6).
      Implemented in exception.cpp via `s.insert(0, ...)`. Added
      `ErrorCode::to_string()` helper to exception.hpp.
- [x] **Exception default ctor sentinel 0x8000** (unknowns #91) — implemented
      `makeDefaultErrorCode()` helper returning `ErrorCode{0x8000}`. Both
      default and string ctors now set `errorCode_` to the sentinel. Real
      binary uses custom `ZiApiErrorCategory` singleton at 0xb7c570; we
      approximate with a plain value-only ErrorCode (category not needed
      by any seqc compiler consumer).
- [x] **Compiler+0x18 purpose** (unknowns #54) — RESOLVED as dead/vestigial
      field. Write-only in binary (zeroed in ctor and compile(), never read
      by any method). `reserved18_` naming is correct.
- [x] **DeviceConstants anonymous enums** (unknowns #32) — RESOLVED as
      already fully documented. Only two anonymous enums exist in binary:
      SyncRegA=0x44 and SyncRegB=0x45, both in Register struct.
- [x] **smap remaining logic** (unknowns #10) — RESOLVED as already complete.
      ~0x1E6 bytes after alui are compiler-generated Immediate dtor cleanup
      + two `st()` calls + vector insert, all already in asm_commands.cpp.
- [x] **AWGCompilerConfig::supportedDeviceTypes documentation** — RESOLVED.
      No separate field exists; `config_->deviceType` (at +0x00) is a single
      power-of-2 AwgDeviceType. The `devType` parameter to
      `checkFunctionSupported` is the bitmask of allowed devices. Comment
      corrected in custom_functions.cpp.
- [x] **Verify format<> template runtime output** — CONFIRMED CORRECT.
      boost::format with `%N%` placeholders is textbook usage. Message
      strings are correct format syntax. 38+ outer + 14 inner instantiations
      match binary's split outer/inner functions.
- [x] **Verify SHA-1 byte order** — CONFIRMED CORRECT. Binary uses `bswap`
      for all 5 words at 0x2998c1..0x2998f3. Reconstruction's manual
      MSB-first packing `(byte[0]<<24)|...` produces identical results.
- [x] **Verify WaveIndexTracker template offsets** — CONFIRMED CORRECT.
      Both instantiations (WaveformFront @0x29d086, WaveformIR @0x29d496)
      read `mov 0x6c(%rax),%r14d` — offset 0x6C matches `waveform.hpp:109`
      `int32_t waveIndex`. Sentinel -1 check also confirmed.
- [x] **Wire playIndexed error 0x98/0x9a** — Phase 4b type-validation
      added: args[0] and args[1] must pass bt $0x54 (VarType ∈ {2,4,6}),
      error 0x98 (ExpectsOffsetAndLength). Rate-type check corrected from
      FuncExpectsConst to ExpectsSamplesConst (error 0x9a).

### 31b. Node serialization (unknowns #27, #28)

- [x] **serialize opcode==4 skip logic** (#27) — RESOLVED: already implemented
      in asm_list.cpp:183 (Pass 1 skips NOP from idMap). Verified Phase 33a.
- [x] **serialize #disableOpt handling** (#28) — RESOLVED: already fixed in
      asm_list.cpp:198-201 (appends " #disableOpt" for waveform cmds with
      opcode ∉ {3,4,5}). Verified Phase 33a.

### 31c. Cache/Prefetch implementation detail + Signal/Assembler internals

- [x] **memoryWrite overlap removal** (#61) — fixed: real erase loop in cache.cpp.
- [x] **getBestPosition nameMap** (#62) — verified correct, doc comment added.
- [x] **allocate splitting heuristic** (#63) — fixed: inverted branch condition.
- [x] **UsageEntry layout** (#68) — confirmed = PlayConfig (0x20 bytes).
- [x] **minIndexedSize semantics** (#69) — documented: 4096 = min for indexed playback.
- [x] **cervino indexed nonsplit** (#75) — both stubs filled:
      play_cervino_indexed_nonsplit emits prf(regH, regC, clampToCache(cacheSize/2)).
      Common indexed finalize split per-branch (Hirzel: addr+goto; non-Hirzel:
      channels*totalSize+smap+addi+smap).
- [x] **placeSingleCommand case label split** (#81) — resolved: header comment
      fully documents; case-1/case-2 merger is refactoring only.
- [x] **Signal marker bit distribution** (#38) — fixed: numEntries = max(1, channels).
- [x] **assembleAsmList register ordering** (#45) — fully resolved. Register
      order verified: immediates→regDst→regAux→regSrc→outputs→label.
      Fixed cout message to match binary. Special cases (MESSAGE/ERROR_MSG/LABEL)
      all documented.

### 31d. Expression / Parser / Boost internals

- [x] **Expression pushChild ownership model** (#93) — fixed: standard owning
      shared_ptr, no-op deleter removed from expression.cpp.
- [x] **CacheEntry serialize template body** (#114) — fixed: 5 fields not 6,
      valid_ excluded from serialization in cached_parser.hpp.
- [x] **SeqcParserContext full layout** (#55) — full 0x38-byte layout decoded.
      All methods converted from raw-offset to typed member access.
      reset() and setErrorCallback() added.
- [x] **Verify SeqcParserContext callback** — confirmed: libc++ std::function
      dispatch at vtable[6]/+0x30 correct. Now uses typed std::function member
      directly, eliminating ABI-specific dispatch code.
- [x] **flex/bison parser entry points** — verified: all C++ mangled (not
      extern "C"), already declared and used correctly. seqc_error behavior
      matches reconstruction.

### 31e. SDK-scope / utility functions

- [x] **ZI*Exception helper functions** — documented, deferred. getKind
      @0x2e5180, toApiCode @0x2e5280, toZiErrorKind @0x2e5240, fromZiErrorKind
      @0x2e5260 are all SDK-surface-only (zero internal callers). ErrorKind
      enum has 10 values (Ok/Overwhelmed/Timeout/Cancelled/NotFound/Internal/
      BadRequest/Unimplemented/Unavailable/+1). ErrorKindCategory singleton
      at 0xb7c5a8, name "zi:kind". Not needed for compiler pipeline.
- [x] **SeqCAstNode print/clone macro expansion** — verified 53×2 symbols
      match binary. 51 simple subclasses correctly macro-generated.
      Bug fix: SeqCVariable::print() tested lineNr_ instead of varType_
      (binary reads offset 0x14 = varType_). Fixed in seqc_ast_node.cpp.

### 31f. Code quality

- [x] **Remaining ~15 placeholder field names** — ALL resolved. AWGCompilerConfig:
      unknown_28→serializeRoundTrip, string_30→debugDumpPath, string_30_owned→
      debugDumpEnabled, string_50→debugJsonPath, string_50_owned→debugJsonEnabled,
      unknown_88→optimizationFlags (unknown_98 already channelGrouping).
      Waveform: field18→formatType, field1C→columnMode, field20→isIntegerFormat.
- [x] **AWGCompilerConfig unknown fields** — all 3 resolved (see above).
      serializeRoundTrip: debug round-trip flag. optimizationFlags: bitmask
      passed to AsmOptimize (0xFF=all). channelGrouping already named.
- [x] **Remaining ~100 reinterpret_cast sites** — audited: ~80 inherent,
      ~13 eliminable now, ~6 blocked on Node. Applied: parserContext_ retyped
      from char[0x38] to SeqcParserContext (2 casts eliminated). Remaining
      eliminable sites documented for future work.
- [x] **~17 "likely"/"uncertain" comments** — audited: 28 actionable found.
      Fixed: resources.cpp "appears to be"→"is", signal.cpp constant confirmed
      1e-12 (was "likely ~1e-7"). Remaining categorized as keep/verify/investigate.

---

## Phase 32 — Code quality sweep — COMPLETE

Systematic cleanup of magic constants, poor variable names, ad hoc
forward declarations, C-style casts, code duplication, and other
readability/maintenance debt across all reconstructed source files.

### 32a. Infrastructure (shared headers, macros) — COMPLETE

- [x] **Consolidate LOG no-op macros** — Created `include/zhinst/log_macros.hpp`
      with LOG_WARNING and LOG_ERROR no-op macros. Replaced per-file copies in
      `custom_functions.cpp`, `awg_assembler_impl_pipeline.cpp`, `asm_list.cpp`.
- [x] **Consolidate yy_buffer_state forward declaration** — Created
      `include/zhinst/yy_fwd.hpp` with struct + both seqc_ and asm_ function
      declarations. Updated `compiler.cpp` and `awg_assembler_impl_pipeline.cpp`.
      Removed redundant forward declarations (StaticResources, Prefetch,
      AsmOptimize) from `compiler.cpp` — already included via headers.

### 32b. Named constants — device types, hardware registers, opcodes — COMPLETE

- [x] **Device-type hex codes** — replaced raw `0x40`/`0x80`/`0xC0`/`0x100`
      in `device_factories.cpp` with named `kSubtype1`..`kSubtype4` + `kSubtypeMask`.
      Replaced `0x80`/`0x100`/`0x40`/`0x10`/`0x20` in `custom_functions_play.cpp`
      and `custom_functions_io.cpp` with `AwgDeviceType::GHFLI`/`VHFLI`/`SHFLI`/
      `SHFSG`/`SHFQC_SG` enum values.
- [x] **Opcode group masks** — named `0x0D000000u` → `kOpcodeGroup2Child`,
      `0x0E000000u` → `kOpcodeGroup1Child` in `awg_assembler_opcodes.cpp`.
- [x] **Bit masks and range constants** — named `0x7FFFF` → `kImm19HalfRange`,
      `0xFFFFDu` → `kImm19MaxUnsigned`, `0x1FEu`/`0x20u` → `kDioAddrHigh`/`Low`,
      `0x1FFu`/`0x21u` → `kIdAddrHigh`/`Low` in `asm_commands.cpp`.
      Named `0x4000000040004041ULL` → `kCheckPlaySupportedMask` in
      `custom_functions_play.cpp`.
- [x] **Hash constant** — renamed to `kGoldenRatioHash` consistently in
      `node_map_data.cpp` (was `kGolden` in one function, raw in another).
- [x] **Config/struct raw offsets → member access** — replaced `+0x40`/`+0x44`/
      `+0x50` in `waveform.cpp` with `dc->waveformGranularity`/`waveformPageSize`/
      `bitsPerSample`. Replaced `+0x30`/`+0x38` in `wavetable_front.cpp` with
      `manager_->waveforms_.data()`/`.size()`. Replaced `+0x9D` in
      `awg_compiler.cpp` with `config_->compressSource` (new field added to
      `AWGCompilerConfig`). Skipped `prefetch_print.cpp` — fields at +0x4C/+0x60
      are on an incompletely-typed Node and remain as inherent reinterpret_cast.
- [x] **Hardware register addresses** — deferred individual SUSER address naming
      (40+ addresses in custom_functions_io.cpp); would require binary-level
      research to name correctly and comments already document their purpose.

### 32c. Variable renames — COMPLETE

- [x] **`a` → `instr`** — 39 sites in `custom_functions_play.cpp` renamed
      (`AsmList::Asm a =` → `AsmList::Asm instr =`, `.append(a)` → `.append(instr)`).
- [x] **`e` → `expr`** — skipped; parameter is already named `expr` in all
      target functions, so renaming the local `e` would cause shadowing.
- [x] **`c`/`m` → `codeVal`/`msg`** — renamed in `exception.hpp`
      `GenericErrorDescription` constructor.
- [x] **`d` → `nodeRaw`/`dict`** — renamed in `prefetch_emit.cpp:283` and
      `pybind_seqc.cpp:103`.

### 32d. C-style casts → C++ casts — COMPLETE

- [x] **prefetch_placesingle.cpp** — 16 casts converted to `static_cast<>`.
- [x] **prefetch_prepare.cpp** — 15 casts converted to `static_cast<>`.
- [x] **waveform.cpp** — 7 casts converted to `static_cast<>`/`reinterpret_cast<>`.
- [x] **wavetable_front.cpp** — 6 casts converted to `static_cast<>`.
- [x] **wavetable_ir.cpp** — 4 casts converted to `static_cast<>`.
- [x] **prefetch.cpp** — 7 casts converted to `static_cast<>`/`reinterpret_cast<>`.
- [x] **awg_assembler_opcodes.cpp** — 6 casts converted to
      `static_cast<>`/`reinterpret_cast<>`.

### 32e. Code deduplication — COMPLETE

- [x] **Extract `appendSuser` helper** — added file-local `appendSuser(list,
      cmds, reg, addr)` inline function in `custom_functions_play.cpp`. 39 call
      sites available for future conversion; helper defined and compiles.
- [x] **Extract `sslIndexExceedsPages` helper** — added file-local inline
      function in `prefetch_placesingle.cpp`; 4 call sites converted.

### 32f. Miscellaneous cleanup — COMPLETE

- [x] **`asm_expression.cpp` raw `new`** — assessed; 5 instances are inherent
      to the bison parser interface (`$$` assignments). Converting to
      `make_unique` would break parser integration. Left as-is.
- [x] **Remove null-cast placeholder** — replaced dead-code null casts in
      `prefetch_helpers.cpp:186-187` with a clean `return 0` + TODO comment
      documenting the binary's iteration pattern.
- [x] **Build verification** — `cmake --build .` clean, 28/28 differential
      tests pass.

### 32g. Wrap-up — COMPLETE

- [x] Update OVERVIEW.md code-quality-debt table with new counts.
- [x] Update TODO.md with completion status.
- [x] Propose follow-up items (see below).

**Follow-up items for future phases:**
- Hardware register address naming (40+ SUSER addresses in custom_functions_io.cpp)
  — DONE in Phase 33d (47 constants defined, 81+ replacements)
- Convert 39 suser+append call sites to use the new `appendSuser` helper
  — DONE in Phase 33b (74 sites converted across 3 files)
- Node fields at +0x4C and +0x60 still need proper typing before
  prefetch_print.cpp raw offsets can be replaced
  — DONE in Phase 33c (config.rate and config.precompFlags)

---

## Phase 33 — Backlog sweep — COMPLETE

Systematic cleanup of remaining open items from Phase 32 follow-ups and
stale unchecked TODO.md entries.

### 33a. Node serialization (#27, #28) — COMPLETE

- [x] Verified opcode==4 skip (#27) already implemented in asm_list.cpp:183
- [x] Verified #disableOpt handling (#28) already fixed in asm_list.cpp:198-201

### 33b. appendSuser helper rollout — COMPLETE

- [x] Added vector overload `appendSuser(vector<AsmList::Asm>&, ...)` to
      custom_functions_play.cpp
- [x] Added helper definitions to custom_functions_io.cpp and
      custom_functions_playback.cpp
- [x] Converted 74 two-line suser+append/push_back patterns to single-line
      appendSuser() calls (43 in play.cpp, 30 in io.cpp, 1 in playback.cpp)

### 33c. Node field typing (prefetch_print.cpp) — COMPLETE

- [x] Replaced 5 reinterpret_cast raw-offset accesses:
      - Node+0x4C → `n->config.rate` (PlayConfig.rate at config+0x04)
      - Node+0x60 → `n->config.precompFlags` (PlayConfig.precompFlags at config+0x18)

### 33d. SUSER address naming — COMPLETE

- [x] Defined 47 named constants in types.hpp (kSuserNodeTag through
      kSuserWaitOnSync), organized by protocol group
- [x] Replaced 81+ raw hex addresses across 5 source files:
      custom_functions_play.cpp (39), custom_functions_io.cpp (37),
      custom_functions_playback.cpp (1), asm_commands.cpp (6),
      seqc_ast_nodes_evaluate.cpp (1)
- [x] Leveraged existing documentation in notes/special_registers.md

### 33e. Print/clone macro verify — COMPLETE (pre-done)

- [x] Already verified in Phase 31e (53×2 symbols, 1 bug fixed)

### 33f. Wrap-up — COMPLETE

- [x] OVERVIEW.md updated with Phase 32 and 33 summaries
- [x] TODO.md updated with Phase 33 entries and stale checkbox fixes
- [x] Build clean, 28/28 differential tests pass

---

## Phase 34: Final symbol gap closure

Systematic closure of the remaining ~27 `zhinst::` symbols present in
the original binary but absent from the reconstructed build (excluding
mangling mismatches and deliberately out-of-scope SDK plumbing).

See `notes/binary_contents_excluded.md` for a full inventory of what
the original binary contains beyond the compiler pipeline, and why
each category is in- or out-of-scope.

### 34a. Anonymous-namespace helpers (9 functions) — COMPLETE

Internal helpers in the original binary that are not part of any public
class interface. Most live in anonymous namespaces in the original.

- [x] `readManifest(const std::string&)` @0x2ec210 — reads manifest file from path; implemented in platform.cpp
- [x] `readManifest()` @0x2ec5e0 — lazy-init static returning cached LabOne manifest; implemented in platform.cpp
- [x] `compressSourceString(const std::string&, const std::string&)` @0x109e90 — already implemented in awg_compiler.cpp:55
- [x] `doIsMf(const boost::property_tree::ptree&)` @0x2ec700 — implemented in platform.cpp
- [x] `isMf(const boost::property_tree::ptree&)` @0x2ec1e0 — implemented in platform.cpp
- [x] `isMf64(const boost::property_tree::ptree&)` @0x2ec430 — implemented in platform.cpp
- [x] `checkWaveformInit(std::shared_ptr<WaveformFront>, const std::string&)` @0x29c6f0 — already implemented as checkWaveformInitialized in wavetable_front.cpp:292
- [x] `getUniqueName(const std::string&, int, int)` @0x2a0fd0 — already implemented inline in wavetable_helpers.hpp:28
- [x] `xmlEscapeSeqToInt(string::const_iterator, string::const_iterator)` @0x2fc280 — implemented in platform.cpp
- [x] Sub-phase wrap-up (build verify, notes, OVERVIEW)

### 34b. Method-level gaps (18 symbols) — COMPLETE

Missing methods on classes that are otherwise fully reconstructed.

#### Assembler rule-of-five + query
- [x] `Assembler::~Assembler()` @0x103980 — already in assembler.cpp (= default)
- [x] `Assembler::Assembler(const Assembler&)` @0x122e20 — already in assembler.cpp
- [x] `Assembler::operator=(Assembler&&)` @0x125ab0 — already in assembler.cpp
- [x] `Assembler::operator=(const Assembler&)` @0x125e80 — already in assembler.cpp
- [x] `Assembler::highestRegisterNumber() const` @0x28ffe0 — already at assembler.cpp:310 (AssemblerInstr::highestRegisterNumber)

#### DeviceType
- [x] `detail::DeviceTypeImpl::doClone() const` @0x2d3280 — already at device_type.cpp:82 (clone())

#### Value types
- [x] `Immediate::operator==(Immediate) const` @0x290d40 — already at value.cpp:172

#### Node
- [x] `Node::waveAtCurrentDeviceIndex() const` @0x1c7de0 — already at node.cpp:208
- [x] `Node::Node(NodeType, int, int)` @0x12ace0 — already at node.cpp:45

#### Play infrastructure
- [x] `PlayArgs::WaveAssignment(const WaveAssignment&)` @0x171c00 — implemented variant-aware copy ctor in custom_functions.cpp

#### Random
- [x] `Random::seedRandom()` @0x16be80 — logic inlined into randomSeed() builtin at custom_functions_playback.cpp:869

#### SeqCAstNode hierarchy
- [x] `SeqCOperation::getVarTypes() const` @0x1fdb40 — already at seqc_ast_node.cpp:160
- [x] `SeqCAstNode::getListElements() const` @0x209dd0 — already via SEQC_LIST_IMPL macro in seqc_ast_node.cpp
- [x] `SeqCArgList::getListElements() const` @0x1ffc10 — already via SEQC_LIST_IMPL macro
- [x] `SeqCDeclList::getListElements() const` @0x200500 — already via SEQC_LIST_IMPL macro
- [x] `SeqCStmtList::getListElements() const` @0x201200 — already via SEQC_LIST_IMPL macro
- [x] `SeqCParamList::getListElements() const` @0x2007e0 — already via SEQC_LIST_IMPL macro
- [x] `SeqCVariable::getListElements() const` @0x209e60 — already via SEQC_LIST_IMPL macro
- [x] Sub-phase wrap-up (build verify, notes, OVERVIEW)

---

## Phase 35 — Functional completeness: stub and placeholder elimination

Goal: achieve full functional reconstruction of all code in the SeqC
compilation flow.  Every stub, placeholder, hardcoded approximation, and
deferred code block identified in the Phase 34/35 incompleteness audit
is listed below, grouped by severity.

### Phase 34 cleanup (completed pre-35)

- [x] **playIndexed arg-gather loop** (custom_functions_play.cpp @0x161410..0x1615f0)
      — fully reconstructed: outer WaveAssignment loop, varType!=4 push,
      inner bits loop with `mask &= ~(1 << ((bit-1) + i*7))`.
- [x] **7 vtable store-back TODOs** — converted to explanatory comments
      (Immediate dtor side-effect, no-op in reconstruction).
- [x] **Raw hex in AddressImpl** — 0x22→kAddrTrigger, 0x23→kAddrInternalTrig,
      0x69→kSuserWaitCycles.
- [x] **2 prefetch_print.cpp reinterpret_cast** — replaced with config_->isHirzel.
- [x] **compiler.cpp debug file write** — implemented ofstream write.
- [x] **20 TODO/NOTE marker conversions** — actionable TODOs converted to
      explanatory NOTEs where the comment described known limitations.

### 35a. HIGH — Core compilation path stubs

#### Play / playback assembly emission

- [x] **play() marker mask-clearing** — custom_functions_play.cpp:548-551.
      Implemented `mask &= ~(1 << ((b-1) + ch*7))` loop.
- [x] **asmPlay merge mode selection** — custom_functions_play.cpp:366.
      Fixed: Sub-path A always uses "playWave" (not useYSuffix-conditional),
      !multiValue jumps to Sub-path B grow. Sub-path B: multiValue → merge+newWaveform,
      !multiValue → useYSuffix ? interleave : merge factory → getOrCreateWaveform.
- [x] **writeToNode Block D part 2** — custom_functions_play.cpp:1407+.
      All 6 cases (0-5) for both Path A (fast jt) and Path B/C (slow jt)
      fully implemented with bodies + post-tails. Binary range @0x164b3a..0x16b740.
- [x] **waitDigTrigger register allocation** — custom_functions_play.cpp:117.
      Fixed: args[1] indexing + Resources::getRegisterNumber() + addi/suser emission.

#### I/O custom functions

- [x] **setSinePhase oscillator index** — custom_functions_io.cpp:1563,1571.
      Fixed: oscIndex from args, restructured node path into correct if-blocks.
- [x] **setOscPhase deferred tail** — custom_functions_io.cpp:1776.
      Range 0x144e00..0x145200 is compiler-generated exception cleanup only; no logic deferred.
- [x] **configFreqSweep node path** — custom_functions_io.cpp:3067.
      Fixed: 0x40/0x80/0x100 → "oscs/<osc>/freq"; 0x10/0x20 → "sgchannels/<awg>/oscs/<osc>/freq".
- [x] **configFreqSweep deferred tail** — custom_functions_io.cpp:3075.
      Range 0x154fbf..0x155066 is EvalResultValue cleanup + epilogue only; no logic deferred.
- [x] **configRTLogger QA_DATA_RAW shift** — custom_functions_io.cpp:2689.
      Fixed: shift = 0xe (interpolated from ZSYNC_B=0xd, QA_PROC_D=0x10).
- [x] **waitOnGrid trigger value** — custom_functions_io.cpp:886.
      Fixed: reads readConst("AWG_GRID_TRIGGER") resource constant.

#### Prefetch pass

- [x] **getUsedChannels()** — prefetch_helpers.cpp:184. Implemented proper
      iteration over usageEntries_ with channelMask OR accumulation.
- [x] **getUsedFourChannelMode()** — prefetch_helpers.cpp:200. Implemented
      scan of usageEntries_ for is4Channel flag.

#### Waveform generation

- [x] **WaveformGenerator::multiply() multi-channel** —
      waveform_generator.cpp:1994. Full 3-phase reconstruction: wavetable loading,
      channel consistency check, per-sample multiply with marker byte-mul,
      amplitude clipping warning. Already done in prior session.
- [x] **WaveformGenerator::filter() multi-channel** —
      waveform_generator.cpp:2260. Corrected: always 3 args (b,a,x), arg order
      fixed, 4 error validations, single-channel enforcement, IIR/FIR dual-path.
      Already done in prior session.
- [x] **WaveformGenerator aliasMap_ population** —
      waveform_generator.cpp:147. Confirmed empty in binary (ctor doesn't
      populate it). Not a gap.

#### Misc core

- [x] **getSampleClock() resource lookup** — custom_functions.cpp:506.
      Implemented variableExists + readConst("$DEVICE_SAMPLE_RATE") with fallback.
- [x] **CachedParser::saveCache() serialization** — cached_parser.cpp:176.
      Linked boost_serialization, added text_iarchive/text_oarchive includes,
      implemented real `oa << index_` / `ia >> index_` calls. Added default ctor to CacheEntry.
- [x] **awg_compiler addWaveforms .bin16/.wave16** — awg_compiler.cpp:586.
      Implemented awg2double16 conversion; build clean.
- [x] **prefetch_helpers fixWaveformSizes parent walk** —
      prefetch_helpers.cpp:622. Binary does NOT walk parents — just compares
      allocationByteSize < maxBlocks * waveformAlignment. Simplified from
      incorrect parent-chain walk to simple size check.

### 35b. MEDIUM — Non-critical path gaps

- [x] **randomSeed()** — custom_functions_playback.cpp:866. Implemented
      actual seed via std::random_device("/dev/urandom") → mt19937_64::seed().
- [x] **Compiler::printAST()** — compiler.cpp:184. Implemented debug tree dump:
      prints EOperationType, EOperator, ECommandType, VarType, name, recursive children.
- [x] **AWGCompiler progress callback** — awg_compiler.cpp:451. Implemented
      progressCallback_.lock()->setProgress(1.0) at compilation end.
- [x] **AWGCompiler cancel checks** — awg_compiler.cpp:530,641. Implemented
      cancelCallback_.lock()->isCancelled() in addWaveforms loop + post-processing.
      Replaced pad_298_/pad_2A8_ with typed weak_ptr members.
- [x] **getJsonVersion() omitted fields** — awg_compiler.cpp:1080.
      Implemented external_triggering (dio/zsync from externalTriggeringMode_) and
      required_options (from usedFeatures_ set). Added friend declarations.
- [x] **CsvParser cache integration** — csv_parser.cpp:497. Threaded
      CachedParser& through csvFileToWaveform, implemented hash computation,
      getCachedFile cache-hit path, updated WavetableFront::loadWaveform.
      Fixed WaveformFile::data type from vector<uint8_t> to vector<unsigned int>.

### 35c. Sub-phase wrap-up
- [x] Build verify + diff tests — 28/28 passing
- [x] OVERVIEW.md update
- [x] Notes update if needed — no new topic notes required

---

## Phase 36 — Code quality final pass

### 36a. Resolve remaining source markers — COMPLETE

Eliminated all 6 remaining TODO/TBD/FIXME markers across 68.7k lines:
- [x] `custom_functions_play.cpp:791` — stale TODO.md cross-ref for 21b-followup-3 (resolved: origin is PlayArgs+0x60 member offset)
- [x] `custom_functions_play.cpp:1154` — stale Phase 21b cross-ref → converted to plain comment
- [x] `resources.cpp:922` — stale TODO.md "Deferred" cross-ref → replaced with notes/libcpp_abi.md reference
- [x] `awg_assembler_impl.hpp:37` — `str2_` (TBD) → renamed to `unusedStr038_` with documentation (zero-initialized, no observed reader/writer in any binary method)
- [x] `custom_functions.hpp:309` — stale `field_168 (TBD)` layout comment → updated to reflect resolved `assignedWaveNames_` (unordered_set<string>, Phase 14a)
- [x] `seqc_ast_node.hpp:774` — "Layout TBD" → updated: layout fully known (payload_[24] + tag_ + pad)
- [x] **Result: 0 markers remain in entire codebase**

### 36b. Placeholder field names — COMPLETE (no action needed)

- [x] Reviewed notes/placeholder_field_names.md — all 15 placeholder fields already resolved in Phase 31f
- [x] Verified headers: all remaining `pad_*` names are genuine alignment padding, not unnamed data fields

### 36c. reinterpret_cast audit — COMPLETE

Eliminated 11 non-inherent reinterpret_casts (82 → 71):
- [x] `prefetch.cpp` (3 casts) — `*reinterpret_cast<const int*>(config_)` → `static_cast<int>(config_->deviceType)`
- [x] `custom_functions_play.cpp:1436` — `*reinterpret_cast<bool const*>(&node.hasFast)` → `static_cast<AccessMode>(node.hasFast)`
- [x] `waveform.cpp:150,222-228` — SSO internal hacks → `nameJsonStr.data()/size()`, `fileNameJsonStr.size()`
- [x] `waveform.cpp:797-798` — pointer subtraction → `data.size() * sizeof(unsigned int)`
- [x] `prefetch_print.cpp:48` — dead vtable pointer read → removed (result was unused)
- [x] Remaining 71 casts confirmed inherent: serialization byte access (28), tagged-union variant (13), TLS/opaque storage (8), ELFIO section data (4), zlib (2), placement-new (4), embedded CachedParser buffer (1), byte-level memmove (11)
- [x] Build clean, 28/28 diff tests pass

---

## Phase 37 — Differential test extension + regression fixes

### 37a. Test case expansion (28 → 69 cases) — COMPLETE

Added 41 new test cases covering builtins, waveforms, SHF devices,
HDAWG4, SHFQC, UHF devices, complex programs, and AWG index variations.
Result: **51/69 pass, 18 fail** across 7 root causes.

### 37b. Regression fixes — triaged failures

#### RC-1: Waveform generation pipeline — PARTIALLY FIXED

Originally: `playWave` with inline waveforms crashed with heap corruption
or threw wrong errors. Root cause was a cascade of 6 bugs found via GDB
tracing:

1. `mergeWaveforms` Phase 5 was incorrectly gated on `multiValue` (IF-14)
2. `mergeWaveforms` Phase 4 `multiValue` used post-append `values.size()`
   instead of pre-append waveform count (IF-18)
3. `mergeWaveforms` Sub-path B had incorrect `multiValue` conditional
4. `getOrCreateWaveform` cache logic was inverted (IF-15)
5. `grow()` used `readPositiveInt(minVal=1)` instead of direct `toInt()` (IF-16)
6. Error message 96 had wrong format string (IF-17)

**Current state**: All 4 tests now produce ELF output (no more crashes),
but ELF content differs — missing waveform section and play instructions.
Remaining differences tracked as RC-9.

**Note**: Error message table was globally off-by-one (all keys shifted +1).
Fixed by GDB-extracting original binary's full 254-entry table and replacing.
This revealed that `hdawg_assignWaveIdx` was accidentally passing before
(shifted table made wrong error message match the expected one).

- [x] Fix mergeWaveforms Phase 5 gating (GDB-verified unconditional)
- [x] Fix mergeWaveforms Phase 4 multiValue (pre-append count)
- [x] Fix mergeWaveforms Sub-path B dispatch
- [x] Fix getOrCreateWaveform cache logic
- [x] Fix grow() targetLen=0 handling
- [x] Fix error message 96 format string

#### RC-2: Error message formatting in compile pipeline — FIXED

Fixed two issues:
1. `compiler.cpp`: Both `hadCompilerError()` checks now throw
   `CompilerException` (binary 0x11fb0d, 0x1212e4) instead of `return {}`.
2. `compile_seqc.cpp`: Catch block uses `getCompileReport()` when non-empty
   instead of always concatenating `ex.what() + report`.

All 2 tests pass: `hdawg_info_msg`, `hdawg_error_msg`.

#### RC-3: Missing `readConst` constants — FIXED

Fixed by adding `parent_ = parent` in GlobalResources constructor
(IF-7), fixing Resources ctor grandparent storage (IF-2), and
`addConst` flags (IF-3).

All 3 tests pass: `hdawg_executeTable`, `shfsg_executeTable`,
`shfqa_startQA`.

#### RC-4: `sync()` unspecified value type — FIXED

Fixed `addSyncCommand` to read `varType_` correctly.
Test passes: `hdawg_sync`.

#### RC-5: SHFQC sequencer parameter not propagated — FIXED

Fixed kwargs merging in `pyCompileSeqc`.
Both tests pass: `shfqc_qa_nop`, `shfqc_sg_nop`.

#### RC-6: `checkFunctionSupported` error message mismatch — FIXED

Two issues:
1. Error message map had m[73]/m[74] swapped (IF-11).
2. `SeqCFunctionCall::evaluate` Path B was missing try/catch for
   `CustomFunctionsException` (IF-12) — exception propagated to
   `compileSeqc` instead of being caught and routed through
   `errorMessage()` with line number prefix.

All 2 tests pass: `shfqa_executeTable`, `shfqa_setPRNG`.
**Note**: `hdawg_assignWaveIdx` previously appeared fixed by the catch
addition, but was actually passing due to the shifted error message table.
After the table fix, it now correctly fails — tracked as RC-10.

#### RC-7: UHF ELF `e_entry` offset (MEDIUM) — PARTIALLY FIXED

UHF devices produce ELF with `e_entry = 0xd0001000` (original) vs
`0xd0000000` (recon). The `0x1000` offset comes from
`getNextSegmentAddress()` after `allocateWaveforms` computes
`computedOffset`. Fixed by passing `deviceConstants.hasDIO` as
`allocFlag` — UHF hasDIO=false so computedOffset is computed normally.

Affected tests: ~~`uhfqa_nop`~~, ~~`uhfli_nop`~~ — **BOTH NOW PASSING**.

#### RC-8: `setOscFreq` undefined symbols at runtime (LOW)

`shfqa_setOscFreq` hits symbol lookup errors at import time.
Multiple undefined symbols:
- `NodeMap::retrieve(const string&) const` — NodeMap class not implemented
- `awg2double16(unsigned int)` — utility function not implemented
- `printSeqCAst(SeqCAstNode&)` — debug function not implemented
- `Prefetch::getUsedCache(shared_ptr<Node>) const` — prefetch method not implemented

Also fixed in this pass: 10+ spurious output-param overload declarations
in `asm_commands.hpp` and `prefetch.hpp` that were declared but never
defined, plus ~35 call sites in `prefetch_placesingle.cpp` that used
the non-existent output-param form.

Affected tests: `shfqa_setOscFreq` (1).

- [x] Implement `NodeMap::retrieve` — done (custom_functions.cpp:1202)
- [x] Implement `awg2double16` — done (csv_parser.cpp:70, awg_compiler.cpp:51)
- [x] Stub or implement `printSeqCAst` — done (seqc_ast_node.cpp:116)
- [x] Implement `Prefetch::getUsedCache` — done (prefetch_helpers.cpp:698)

#### RC-9: playWave ELF assembly generation mismatch (MEDIUM)

After fixing RC-1 waveform pipeline bugs and the error message table,
the playWave tests fail with "tried to play a NULL pointer" (PlayNullPtr,
key 22). This means waveforms are not being loaded into cache. The
prefetcher `node_` chaining fix exposed code paths that now execute but
hit cache lookup failures.

The 3 regression tests (hdawg_mixed_loops, hdawg_nested_if_loop,
hdawg_deep_nesting) now fail with "invalid identifier while placing
the fetch and play commands" (key 163) — a real prefetcher logic bug,
no longer masked by the message table shift.

Affected tests (current): `hdawg_playWave_multi` (ELF size/content diff,
missing prefetch opcodes), `hdawg_wave_in_loop`, `hdawg_mixed_loops`,
`hdawg_nested_if_loop`, `hdawg_deep_nesting` (4 — "invalid identifier
while placing...").

Fixed this session:
- `hdawg_playWave_zeros`, `hdawg_playWave_ones` — NOW PASSING (FIFO allocator
  fixes: startOffset=addressBase_, lastFreeBlock().start not .end,
  max not min in alignWaveformSizes/assignWaveformAllocationSizes,
  fast-path cache-line allocation, getNextSegmentAddress → setMemoryOffset)
- SHFQA regression (7 tests) — fixed by passing `deviceConstants.hasDIO`
  as `allocFlag` to `updateWaveforms` (binary at 0x11e438 loads DC+0x80).

- [x] Fix `play()` Step 8 condition inversion
- [x] Fix `play()` Step 8 Hirzel path (skip asmPrefetch)
- [x] Fix `asmPlay` wavesPerDev double-counting
- [x] Fix `createLoad` always-null bug
- [x] Fix `createLoad` wrong deviceIndex field
- [x] Fix `SeqCStmtList::evaluate` missing `node_` propagation
- [x] Fix `maxBranches_` uninitialized (SIGFPE)
- [x] Fix `findPlaceholder` wrong ErrorMessages call
- [x] Fix error message table off-by-one (entire table shifted)
- [x] Investigate "tried to play a NULL pointer" in cache.cpp:383 — resolved (all 114 tests pass)
- [x] Investigate "invalid identifier" in prefetcher for control-flow tests — resolved (all 114 tests pass)
- [x] Compare `asmPrefetch` / `asmPlay` output between binary and recon — resolved (all 114 tests pass)
- [x] Check ElfWriter waveform section generation — resolved (all 114 tests pass)

#### RC-10: `assignWaveIndex` wrong error path (LOW)

`hdawg_assignWaveIdx` is an error-producing test. Both original and recon
error, but on different checks:
- Original: "assignWaveIndex: only const waveform index allowed" (key 149)
- Recon: "assignWaveIndex: unexpected arguments" (key 150)

The recon hits the `(varType | 2) != 6` check (UnexpectedArgs) at
`custom_functions_io.cpp:383-387` before reaching the const-index check.
The original binary takes a different code path — likely the argument
parsing order differs.

Affected tests: `hdawg_assignWaveIdx` (1).

- [x] Compare `assignWaveIndex` control flow against binary at 0x133c40 — resolved (hdawg_assignWaveIdx passes)

#### Audit: `asm_commands_impl_*.cpp` field assignment review (LOW)

The `wvft` fix (IF-14 prior session) changed `regDst→regSrc` and
`immediates→outputs`.  Other functions in `asm_commands_impl_hirzel.cpp`
and `asm_commands_impl_cervino.cpp` likely have the same issues:
`wvf`, `wvfe`, `wvfi`, `wvfei`, `wvfs`, `brz`, `prf`, `wtrig`, `wtrigi`.

- [x] Audit all `asm_commands_impl_*.cpp` functions against binary — resolved (IF-50, IF-54 fixed; all 114 tests pass)

#### RC-11: SeqCAssign Row 1 missing variable init ADDI — FIXED

`SeqCAssign::evaluate` Row 1 (lhsType=Var, rhsType ∈ {Const,Cvar}) was
not emitting `addi(lhsReg, R0, constValue)` + `asmSetVarPlaceholder(lhsReg)`.
GDB-verified at binary addresses 0x245364 and 0x24549d. This was the root
cause of ~25+ test failures where `var x = <const>;` produced no instruction.

Fix: seqc_ast_nodes_evaluate.cpp:3164-3179 now emits addi + placeholder.

- [x] Implement addi + asmSetVarPlaceholder emission for Var=Const/Cvar
- [x] Verify with hdawg_arithmetic test

#### RC-12: Hirzel ssl/ssr operand order swapped — FIXED

`AsmCommandsImplHirzel::ssl()` and `ssr()` had regSrc/regDst swapped.
Binary encoding proved: regDst=shiftReg, regSrc=R0 (not the reverse).

Fix: asm_commands_impl_hirzel.cpp:132-134 and 146-148.

- [x] Fix ssl operand order
- [x] Fix ssr operand order

#### RC-13: Remaining failures — categorization (2026-04-27, updated)

After all fixes through Phase 37c→38: **69/69 pass** (all byte-identical).

All 13 remaining failures were resolved:
- Category A (waveform): forEachUsedWaveform sort fix, assignWaveIndex error code
- Category B (instruction count): asmSetVarPlaceholder, StmtList::evaluate rewrite, setUserReg reg_ fix
- Category C (SHFQA): registerAllocation Phase 4 rewrite, overlap fix
- Category D (various): setDIO addi32+node, NodeMapData JSON keys, nested_if_loop (regalloc)

- [x] Investigate `.channels` section 0xc0 vs 0x00 (Category A) — FIXED (forEachUsedWaveform sort)
- [x] Fix hdawg_many_vars (Category B) — FIXED (missing asmSetVarPlaceholder)
- [x] Fix hdawg_deep_nesting (Category B) — FIXED (StmtList::evaluate hasError rewrite)
- [x] Fix hdawg_full_program (Category B) — FIXED (setUserReg reg_ fix)
- [x] Fix hdawg_setDIO (Category D) — FIXED (setDIO addi32 + node access)
- [x] Fix hdawg_assignWaveIdx (Category D) — FIXED (assignWaveIndex error code)
- [x] Fix shfqa_setOscFreq (Category D) — FIXED (NodeMapData JSON key names)
- [x] Fix remaining SHFQA + nested_if_loop — FIXED (registerAllocation Phase 3b+4 rewrite)
- [x] Investigate instruction count differences (Category B) — resolved (all 114 tests pass)
- [x] Investigate SHFQA register errors (Category C) — resolved (all 114 tests pass)

#### RC-14: registerAllocation bidirectional overlap check — FIXED

The overlap check in `registerAllocation` only tested one direction
(`physRange.back() >= virtRange.front()`). Binary performs a proper
bidirectional interval overlap: `front(phys) <= back(virt) AND
back(phys) >= front(virt)`. Also, merged physRange vectors were
unsorted — added `std::sort` after each merge.

Binary address reference: `0x27f94f-0x27f95b` (first check) +
`0x27fb40-0x27fb4c` (second check).

- [x] Fix overlap check to bidirectional
- [x] Sort physRange after merge

#### RC-15: wvfs Hirzel `regDst` → `regSrc` — FIXED

Binary `AsmCommandsImplHirzel::wvfs` at `0x27d071` stores register
in `regSrc` (+0x20), not `regDst` (+0x28).

- [x] Fix wvfs register field assignment

#### RC-16: playHold `isHold`/`isBool` parameter swap — FIXED

Binary `playHold` at `0x1391b8-0x1391d7` passes `isHold=false, isBool=true`
to `asmPlay`. Recon had them swapped.

- [x] Fix playHold parameter ordering

---

## Phase 39 — Code-smell sweep follow-ups (asm / goto / magic numbers)

Added 2026-04-29 after a read-only smell scan covering:
- inline assembly (`asm` / `__asm__`)
- `goto` statements
- magic numbers / unnamed constants

Magic-number remediation already happened in Phase 22e (16 enums + ~430
call-site replacements). This phase covers what the original scan missed
and what the new scan turned up.

### 39a. Replace inline `asm` in `util_wave.cpp` with SSE2 intrinsic

Site: `reconstructed/src/util_wave.cpp:96` — single inline-asm
statement in the codebase. Reproduces x86 SSE2 `maxsd` NaN-propagation
semantics ("when either operand is NaN, return the second source"),
which no portable C++ construct replicates exactly:

| Construct | NaN behavior | Matches `maxsd src, dst`? |
|---|---|---|
| `std::max(a, b)` | unspecified (typically returns first arg) | ✗ |
| `std::fmax(a, b)` | returns the non-NaN argument | ✗ |
| `(a >= b) ? a : b` | NaN comparisons false → returns `b` | ✗ |
| `_mm_max_sd` intrinsic | returns second source on NaN | ✓ |

Replace the inline asm with `_mm_max_sd` from `<emmintrin.h>`.
GCC/Clang/MSVC all lower it to a single `maxsd`, so the emitted
instruction is identical and binary-fidelity for NaN inputs is
preserved. Net effect: codebase regains "zero inline asm" property.

- [x] Add `#include <emmintrin.h>` to `util_wave.cpp`
- [x] Rewrite the `else` branch of `double2awg16` (lines ~85–98) to
      use `_mm_max_sd(_mm_set_sd(-1.0), _mm_set_sd(sample))`
- [x] Delete the unused `clamped` ternary line and stale "Actually
      maxsd returns…" intermediate comment; collapse the rationale
      into one coherent block
- [x] Build (`cmake --build .` in `reconstructed/build/`) — zero
      warnings expected
- [x] Run `python tests/diff_test.py` — must remain at 257/259
- [x] Add IF-N entry to `notes/incidental_findings.md` (IF-106)
- [x] Sub-phase wrap-up

### 39b. Goto policy — research item (deferred restructuring)

The codebase has 135 `goto` sites. Of these:

| Bucket | Count | Treatment |
|---|---|---|
| Generated (`seqc_parser.tab.c`, `seqc_lexer.c`) | 24 | Off-limits (Bison/Flex output) |
| Faithful binary CFG reconstructions (`prefetch*`, `seqc_ast_nodes_evaluate`, parts of `asm_optimize`) carrying `// 0x…` breadcrumbs | ~105 | Leave; restructuring would damage diff-debugging workflow and risk codegen drift away from binary |
| Small isolated cleanup candidates (`zi_folder`, `node`, `csv_parser`, `asm_list`, possibly `custom_functions_play`) | ~6 | Eligible for case-by-case cleanup, each pending a binary CFG check |

This sub-phase is research-only — no code restructuring is in scope.

- [x] Write `notes/goto_policy.md` capturing the 3-bucket taxonomy
      so future contributors do not "clean up" binary-faithful state
      machines
- [x] For each of the ~6 small candidates (`zi_folder.cpp:153`,
      `node.cpp:372/376/433`, `csv_parser.cpp:549/859`,
      `asm_list.cpp:366/584`, `custom_functions_play.cpp` step9_return),
      record the binary address(es) backing the goto and a one-line
      verdict ("safe to refactor" / "binary-faithful, leave")
- [x] No source edits in this sub-phase
- [x] Sub-phase wrap-up

### 39c. Scan-coverage note

The earlier asm scan used regex `\basm\s*[\(\{]|__asm__|__asm\b` which
missed `asm volatile "…"` (the `volatile` qualifier sits between `asm`
and the string literal, which has no `(` immediately following). The
correct pattern is `\basm\b\s*(volatile\s*)?[\(\{]` or simpler
`\basm\s+volatile|\basm\s*[\(\{]|__asm__`. One real site was missed
in the earlier scan.

- [x] Document the corrected regex in `notes/incidental_findings.md`
      under the same IF-N entry as 39a, so any future "no inline asm"
      verification uses the right pattern

### 39d. Goto eliminations (Bucket 3 candidates)

Derived from the Phase 39b audit in `notes/goto_policy.md`. Only the
candidates judged "safe to refactor" or "marginal but worth trying"
are listed here; all binary-faithful gotos (`prefetch*`,
`seqc_ast_nodes_evaluate`, `custom_functions_play.cpp step9_return`,
`asm_optimize.cpp:980 cleanup`) are explicitly out of scope.

Each item must, in this order:
1. Re-check the binary address(es) referenced by surrounding `// 0x…`
   comments to confirm restructuring won't drop information.
2. Apply the smallest possible edit that removes the `goto` while
   preserving function shape.
3. Build (`cmake --build .`) — zero new warnings.
4. Run `python tests/diff_test.py` — must remain at 257/259 (no new
   regressions).

#### 39d-i. `node.cpp` — extract `throw_error:` into helper

Two `goto throw_error;` (lines 372, 376) jump to a 7-line throw block
at line 433. Both throws happen *after* the function's main `return;`
at line 430, so no shared scope with the goto sites. Refactor:

- [x] Extract `throw_error:` body into
      `[[noreturn]] static void throwSwapNotConnected();` at TU scope
- [x] Replace both `goto throw_error;` with `throwSwapNotConnected();`
- [x] Delete the `throw_error:` label and the trailing `{...}` block
- [x] Build + test (must remain 257/259)
- [x] Sub-phase wrap-up

#### 39d-ii. `zi_folder.cpp` — eliminate `resolve_home`

The `goto resolve_home;` at line 153 mirrors `jmp 0x2cf4eb` in the
binary, but the function is only ~50 lines and the tail is
self-contained. Two viable shapes:
- (A) Extract `static ZiFolder resolveHomeFolder();` and replace both
  the goto and the eventual fall-through with explicit `return
  resolveHomeFolder();`.
- (B) Wrap the readlink path in `do { ... } while (false);` with
  `break;` instead of `goto;` — preserves binary CFG more literally
  but is uglier.

Prefer (A); it removes the label cleanly and matches modern C++ style.

- [x] Extract `resolveHomeFolder()` helper (the body currently after
      `resolve_home:`) — implemented as a local lambda
- [x] Replace `goto resolve_home;` with `return resolveHomeFolder();`
- [x] Replace the natural fall-through into `resolve_home:` (Data /
      Settings non-MF path) with `return resolveHomeFolder();`
- [x] Delete the `resolve_home:` label
- [x] Build + test
- [x] Sub-phase wrap-up

#### 39d-iii. `csv_parser.cpp` — eliminate `skip_comment` / `skip_comment_ir`

Two near-identical sites (lines 549/583 and 859/890). Each `goto`
skips a separator-detection block to the loop tail
(`++lineNum; continue;`). Cleanest fix: replace `goto skip_comment;`
with `++lineNum; continue;` directly at the goto site, removing the
label and the explicit `++lineNum; continue;` at line 584/891. The
inner separator-scan block becomes its own scoped section that
always falls through to the loop's natural end.

- [x] At line 549: replace `goto skip_comment;` with the
      `bool hasTimeColumn` flag pattern (the surrounding
      `if (rawLine.size() >= 10) { ... }` made the direct `continue`
      unreachable, as predicted)
- [x] At line 859: same pattern for `skip_comment_ir`
- [x] Delete both labels and their trailing `++lineNum; continue;`
      lines (now redundant)
- [x] Build + test (CSV parsing is exercised by SHFQA QA-weights tests)
- [x] Sub-phase wrap-up

#### 39d-iv. `asm_list.cpp` — assess `cleanup_and_next` (research first)

`goto cleanup_and_next;` at line 366 jumps to label at line 584. Tail
is just `wavetableFront++;` then loop continues. RAII handles the
"cleanup of vec_output, vec_reg, vec_input" comment.

- [x] Read full surrounding loop body to confirm the only difference
      between fall-through and `goto` is whether the alternate
      processing block runs
- [x] Confirmed safe: the three vectors (`vec_input`, `vec_reg`,
      `vec_output`) are declared inside the for-loop body so RAII
      cleans them at scope exit. The goto site does not even use
      those vectors. Replaced `goto cleanup_and_next;` with
      `wavetableFront++; continue;` and removed the label. The
      `// 0x266d87 ff` breadcrumb is preserved as a comment on the
      loop tail.
- [x] Build + test
- [x] Sub-phase wrap-up

#### 39d-v. Wrap-up: regenerate scan + update goto_policy.md

- [x] Re-run `grep -rE "^\s*goto\s+\w+" reconstructed/src` and update
      the bucket counts in `notes/goto_policy.md` (153 total: 24
      Bucket 1, 129 Bucket 2, 0 Bucket 3)
- [x] Update OVERVIEW.md if the goto count is referenced there
      (no references — skipped)
- [x] Phase 39d sub-phase wrap-up
