# TODO.md archive — Phases D3, D4, D5, D-AUDIT-1 (archived 2026-05-12)

This file preserves the full per-batch narrative for the four largest
documentation-phase entries that completed in Phase D.  Every item
below is ticked; no open work was archived.

## Why archived

After D5-20 closed (95.2% doxygen coverage) and D-AUDIT-1 completed,
phases D3, D4, D5, and D-AUDIT-1 together occupied ~896 lines of
`TODO.md` — ~60% of the file — with no forward-looking work.  Per
AGENTS.md "Notes organization", finished phase narrative belongs in
`archive/`, not in the live task list.  TODO.md retains a compact
summary paragraph + commit anchors + a pointer to this file for each
phase.

## Suite state at archive time

- **1603/1603** tests passing (`python tests/diff_test_fast.py`)
- **0** doxygen warnings (`cmake --build . --target docs`)
- Coverage: **2934/3081 (95.2%)** (`reconstructed/docs/coverage.sh`)
- Tag counts: `\unclear=1`, `\verifyme=0`, `\binarynote=40`,
  `\unverifiable=5`

## Source

The bodies below were lifted verbatim from `TODO.md` lines 73–651
(D3, D4, D-AUDIT-1) and 653–962 (D5) prior to the D15.4 trim commit.

---

## D3 — Pipeline-driver functions

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

- [x] **D4 — Public methods of high-traffic classes**
  - [x] Order: `Compiler` → `Prefetch` → `WavetableIR/Front` →
        `CustomFunctions` (subdivided) → `AsmOptimize` →
        `WaveformGenerator` → `Resources` family → AST nodes
  - [x] Per-class commits and `\unclear` triage
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
        WaveformGenerator propagation); both **GDB-confirmed and
        fixed** in the same batch — `Compiler::setLineNr` now also
        calls `asmCommands_->setWavetableFrontIndex(nr)` and tail-
        calls `wavetable_->setLineNr(nr)`; `Compiler::setCancelCallback`
        now propagates the `weak_ptr` into
        `waveformGen_->cancelCallback_` (previously mislabelled
        `reserved_B0_`).
  - [x] **D4 Batch 2a** — `Prefetch` class, ctor/dtor and the three
        `preparePlays` sub-passes (`prepareTree`, `countBranches`,
        `definePlaySize`).  Surfaced IF-212 (block-header summary
        comments in `prefetch_prepare.cpp` contradicted the actual
        function bodies — `Play`/`Load` case labels swapped, `Lock`
        called "Wait" with stale `appendMode` / `field_0x18` /
        `pageSize_` / `maxProgramSize` / `field_0x1E` field names)
        and IF-213 (`Prefetch::findLockedPlay` is a stub that always
        returns `nullptr`, so `prepareTree`'s locked-play merge path
        is dead code).  IF-212 fixed in the same batch by rewriting
        all three block-headers to match the verified bodies; IF-213
        left open for a separate Lock-using-test investigation.
        AGENTS.md gained a new "Verify-then-write: code is the
        source of truth" section formalising the workflow.
  - [x] **D4 Batch 2b** — `Prefetch::determineFixedWaves`,
        `getMemoryHighWatermark`, `getRequiredMemory`,
        `moveLoadsToFront` (4 methods).  Bodies verified directly;
        confirmed `getMemoryHighWatermark` clamps page count *down*
        to `maxWaveformLength` (`min`) while `getRequiredMemory`
        clamps *up* to the same field (`max`) — opposite operations
        on the same `DeviceConstants` field.
  - [x] **D4 Batch 2c** — `Prefetch::optimize`, `optimizeSync`,
        `optimizeCwvf`, `allocate` (4 of 4 methods documented).
        Surfaced and fixed IF-214 (15-site "BFS" misnomer in
        `prefetch.cpp` / `prefetch_helpers.cpp` — actually LIFO
        via `std::deque::back()` / `pop_back()`), IF-215
        (`Prefetch::optimize` block-header listed dispatched type
        as `Play 0x02` when body cmps `Load 0x01`; rebuilt header
        from a body-verified read of the four real cases plus the
        three Load-parent sub-cases), and IF-216 (recon body bug:
        `Prefetch::allocate:1573` dispatched on `NodeType::Wait`
        (`0x200000`) where the binary cmps `$0x40` (`Lock`); same
        Play↔Load swap pattern in the surrounding block-header).
        IF-216 GDB-confirmed and fixed in the follow-up: symbol
        renamed `Wait` → `Lock`, label `handleWait` →
        `handleLock`, `handleRate80` → `handleUnlock`, block-
        header rewritten from verified evidence, doc brief for
        `allocate` written.  No regressions; 1600/1600 tests pass.
  - [x] **D4 Batch 2c follow-up (IF-216)** — investigated
        `Prefetch::allocate` Lock/Unlock dispatch by GDB-tracing
        `_seqc_compiler.so` at `0x1d0fb0` on
        `tests/cases/uhfli_misc_funcs.seqc`.  Confirmed the
        `cmp $0x40` site is taken by genuine `Lock`-typed nodes
        and the `cmp $0x80` site by `Unlock`-typed nodes.  Recon
        symbol fixes applied above.  GDB driver and recipe
        committed at `tests/gdb/gdb_trace_lock.py` and
        `tests/gdb/gdb_lock_trace.txt`.
  - [x] **D4 Batch 2c follow-up (IF-213)** — fixed.
        `Prefetch::findLockedPlay` reconstructed at
        `prefetch_helpers.cpp:432-525` from full disassembly of
        `0x1d3dd0..0x1d4442` (the earlier `0x1d4a10` end
        estimate overstated the range — everything past
        `0x1d4442` is `Node::remove`, a separate function).
        Implementation is a LIFO `std::deque` worklist walk:
        for each popped node, if `type == Play` and
        `wavesPerDev[deviceIndex]` matches `waveform->name`,
        return it; otherwise push all `branches` entries, push
        `loop` if present, then push `next` unless this is an
        `Unlock` whose own wave name matches the target (in
        which case the Unlock closes the Lock scope and we drop
        the sibling chain).  `waveform->name` is read directly
        from the inherited `Waveform::name` field at WaveformIR
        offset 0.  Tests: 1602/1602 (no behaviour change for
        the existing corpus — the only Lock test has the
        trivial `lock; play; unlock` pattern where the
        `createLoad` fallback happens to produce identical
        ELF).  Recommend a future regression test exercising
        an `Unlock` that terminates an in-scope chain to close
        the last static-only verification gap.  See IF-213 for
        full disassembly trail.
  - [x] **D4 Batch 2d** — Prefetch tree-rewrite helpers (13
        methods): `getUsedWavesForDevice`, `collectUsedWaves`,
        `linkLoad`, `removeBranches`, `expandSetVar`,
        `findLockedPlay`, `createLoad`, `mergeLoads`, `assignLoad`,
        `globalCwvf`, `backwardTree`, `sameLoads`,
        `nodeByCachePointer`.  Surfaced three likely-bugs
        (IF-217/218/219) and a cosmetic cluster (IF-220).  Cosmetic
        comment drift fixed in same commit; likely-bug code paths
        flagged with `\verifyme` in briefs and explicit IF markers
        in recon comments, deferred to dedicated follow-ups.
        1600/1600 tests, build clean, 0 doxygen warnings.
        (commit `1e7cd32`)
  - [x] **D4 Batch 2d follow-up (IF-217)** — fixed.
        `Prefetch::backwardTree` now enqueues `cur->loop` instead
        of re-enqueueing `cur->next` in the third visitor block
        (`prefetch_helpers.cpp:264-273`).  Confirmed against
        binary disassembly: at `0x1d5af0` the third dispatch
        reads `mov 0xe0(%r12),%rax` (loop), with the matching
        control-block read at `0x1d5b87`
        (`mov 0xe8(%r12),%rcx`) and shared_ptr copy at
        `0x1d5b8f` (`movups 0xe0(%r12),%xmm0`) — primary
        evidence from objdump, no GDB needed per AGENTS.md
        evidence hierarchy.  Tests remain 1602/1602 as
        predicted (latent bug; `Node::parent` is already
        populated earlier in the lowering pipeline).  Block-
        header comment was already neutral and remains
        accurate.  No regression test added — exercising the
        bug requires inspecting `parent.lock()` post-
        `backwardTree`, which no public API surfaces; future
        work if/when the parent back-link is consulted.  IF-217
        marked **fixed** in `incidental_findings.md`.
  - [x] **D4 Batch 2d follow-up (IF-218)** — fixed.
        `Prefetch::expandSetVar` rewritten at
        `prefetch_helpers.cpp:372-432` to match the binary
        (`0x1d3af0..0x1d3dd0`).  The function walks the **parent
        chain** via `node->parent.lock()` (not the sibling
        `next` chain), gates each ancestor on
        `parent->type == NodeType::Loop` (not `node->type ==
        SetVar`), and at every Loop ancestor whose `loop` body
        head is the currently-tracked child, splices a thin
        `make_shared<Node>(SetVar, asmId, config_->numChannelGroups)`
        clone in front of that child via
        `Node::insertBefore`.  Only `lengthReg` is copied from
        the original.  IF-218 hypothesis was wrong on all three
        points (sibling walk, SetVar gate, orphan clones); see
        the updated IF-218 entry for the full disassembly trail.
        Tests: 1602/1602 (no behaviour change — corpus does not
        exercise SetVar-inside-Loop on multi-group devices).
  - [x] **D4 Batch 2d follow-up (IF-219)** — fixed.
        `Prefetch::createLoad` now has the already-loaded
        short-circuit at `prefetch.cpp:2215-2225`:
        `if (auto loaded = n->loadRef.lock()) return result;`.
        Confirmed against objdump (no GDB needed): binary at
        `0x1d4a54-0x1d4aa0` inlines `weak_ptr::lock()` and
        returns null when the lock yields a live Load; the
        extra `__ptr_ == 0` defensive check the compiler emits
        at `0x1d4a6b` is redundant in practice (weak_ptr never
        zeroes `__ptr_` on expiration) so the idiomatic
        `lock()` form is the source-level equivalent.  Block-
        header summary rewritten to reflect the restored guard
        and the previously-noted stale labels (Play/Load swap,
        assignLoad 3-arg form, `Node::play` not `loadTargets_`,
        `markedForLoad` not `fixed_`).  Tests remain 1602/1602
        as predicted (bug was latent — single-pass
        `preparePlays` only visits each node once).  IF-219
        marked **fixed** in `incidental_findings.md`.
  - [x] **D4 Batch 2e-i** — Prefetch placement dispatch (3
        methods): `placeCommands`, `findPlaceholder`,
        `placeSingleCommand`.  Surfaced two cosmetic
        comment-drift findings (IF-221 NodeType::Branch labelled
        "placeholder"; IF-222 file-header NodeType labels for
        Table / PlainLoad / AwgReady), one false-positive audit
        claim about `PlayConfig::now` (dismissed pre-logging),
        and two likely-bug stubs (IF-223 Table case missing
        smap/ssl/addr/prf tail; IF-224
        `play_cervino_indexed_nonsplit` empty body).  Cosmetic
        drift fixed in same commit; stubs flagged with
        `\verifyme` in briefs and explicit IF markers in recon
        comments, deferred to dedicated follow-ups.  1600/1600
        tests, build clean, 0 doxygen warnings.
  - [x] **D4 Batch 2e-i follow-up (IF-223)** — partially fixed
        (sub-paths A and B reconstructed; sub-path C deferred via
        `\verifyme`).  Consolidated objdump audit revealed the
        IF's "smap + ssl loop + addr + prf, mirrors
        cervino_nonsplit" hypothesis was wrong about both scope
        and pattern.  The Table case actually has **three
        sub-paths**, all converging at `out->insert` at
        `0x1da6df` (parallel to, but distinct from, the Play-case
        `play_finalize` at `0x1dba0d`):
        A (cachePtr empty) emits `addi(reg2,zero,encodedCwvf) +
        smap(reg1,reg2,0x400|tableIndex)`; B (cachePtr non-empty,
        no lengthReg) additionally emits
        `smap(reg1,stateReg,tableIndex) + addi(reg2,zero,
        totalSize) + smap(reg1,reg2,0x800|tableIndex)`; C
        (lengthReg valid && != R0) branches to `0x1daed4..
        0x1db740` for split / non-split tail with ssl/addi/prf/
        wprf.  Sub-paths A and B now reconstructed at
        `prefetch_placesingle.cpp:1083-1227`; sub-path C deferred
        because the split-tail at `0x1db562..0x1db60a` shares
        code with `play_cervino_indexed_nonsplit` (recon line
        884-908) and needs separate factoring (IF-244).  Two
        pre-existing recon bugs also fixed in same pass:
        fabricated `>=0x1000000` cwvf-vs-cwvfr branch and wrong
        `defaultRate` argument to `encodeCwvf`.  Five new IFs
        filed for sub-path C open questions (IF-241..IF-245).
        Tests: 1602/1602 (corpus does not exercise sub-path C).
        Build clean, 0 doxygen warnings.
  - [x] **D4 Batch 2e-i follow-up (IF-224)** — fixed.
        `play_cervino_indexed_nonsplit` label body reconstructed
        at `prefetch_placesingle.cpp:863-913` from full
        disassembly of `0x1db562..0x1db60a`.  Despite the
        earlier "wwvf + ssl loop + addr + prf" hypothesis, the
        actual block is much smaller: it emits exactly one
        `prf(registerHirzel, registerCervino,
        clampToCache(cachePtr->size_ / 2))` followed by one
        `wprf()` into `tempList`, then falls through to
        `play_finalize`.  The wwvf/ssl/addr/addi sequence was
        emitted earlier in `placeSingleCommand` before reaching
        this label.  Block-header comment corrected.  Tests:
        1602/1602 (no behaviour change — corpus does not
         exercise the gate combination that routes here).  See
          IF-224 for full disassembly trail.
   - [x] **D4 Batch 2e-i follow-up (IF-244)** — fixed
         (documentation-only relabel; no goto graph changes).
         The block at `prefetch_placesingle.cpp:884-908`
         (now relocated and renamed `table_indexed_with_clamp`)
         is the Table sub-path C2 split tail at
         `0x1db562..0x1db60a`, which calls `clampToCache` and
         exits via `0x1db60a → 0x1db911`.  A new
         documentation-only `play_cervino_indexed_nonsplit` block
         was added immediately above it covering the real Play
         tail at `0x1db4ad..0x1db55d` (inline unconditional
         `wprf` before raw `prf`, no `clampToCache`, exits
         `0x1db55d → 0x1db92e`).  Neither label is `goto`'d in
         the current recon — both are documentation captures so
         that future dispatch reconstruction (Play side, and
         IF-223 sub-path C on the Table side) can wire them up
         with the correct body.  IF-223 sub-path C `\verifyme`
         note updated to reference the new `table_indexed_with_clamp`
         label.  File-header IF-224 reference updated to IF-244
         + IF-223.  Optional `emitPrfEpilogueAndInsert_` helper
         (action item 3 from IF-244) deferred — will be revisited
         when the dispatch wiring is reconstructed.  Tests:
         1602/1602 (no behaviour change — neither label
         executes).  Build clean, 0 new doxygen warnings.
   - [x] **D4 Batch 2e-i follow-up (IF-223 sub-path C)** —
         fixed.  With IF-244 unblocked (mislabel resolved) and
         IF-241..IF-243 resolved (formula, idxReg name, and
         `awgCfg_+0xC` premise corrections), IF-223 sub-path C
         was reconstructed inline within the Table case at
         `prefetch_placesingle.cpp` ~lines 1258-1432.  Three
         branches now dispatch on `lengthRegInvalidOrZero` and
         `Prefetch::split_`: (B) length invalid → existing
         smap-triplet with `totalSize = sizePerDevice /
         (isHirzel ? 1 : pagesNeeded)` and `stateReg = isHirzel
         ? Hirzel : Cervino`; (C-non-split) length valid &&
         `!split_` → re-converges with B's smap-triplet but with
         `totalSize = (length * numChannels) * 2` (signed 32-bit
         per IF-241) and **forced** `stateReg = registerCervino`
         (audit §4f-1); (C-split) length valid && `split_` →
         allocates `idxReg` (per IF-242), emits
         `addi(idxReg, lengthReg, 0)` + per-channel `ssl(idxReg)`
         loop bound by `numChannels` + `addr(idxReg, regHirzel)`
         + `prf(regHirzel, regCervino, clampToCache(cacheSize >>
         1))` + `wprf` (the latter unconditional with `\verifyme`
         pending IF-244 action item 3 helper extraction; binary
         gates on `!isHirzel` at `0x1db935`).  Tests: 1602/1602
         (corpus does not exercise sub-path C; behaviour change
         is latent until a `playWaveTable` test with non-empty
         cache + valid per-channel length register is added).
         Build clean, 0 new doxygen warnings.  IF-223 marked
         **fixed** in `incidental_findings.md`.
   - [x] **D4 Batch 2e-ii** — Prefetch waveform-instruction
        helpers (7 methods): `clampToCache`, `wvfImpl`,
        `wvfRegImpl`, `wvfs`, `needsNewCwvf`, `splitPlay`,
        `insertPlay`.  No new IFs surfaced; the existing
        block-header summaries in `prefetch_emit.cpp` /
        `prefetch_splitplay.cpp` matched the bodies after
        line-by-line audit.  1600/1600 tests, build clean,
        0 doxygen warnings.
  - [x] **D4 Batch 2e-iii** — Prefetch cache / queries / debug
        printer (4 methods): `getUsedCache`, `getUsedChannels`,
        `getUsedFourChannelMode`, `print`.  Surfaced one
        cosmetic comment-drift (IF-225, `getUsedChannels`
        block-header named the reduced field `channelMask` when
        the body and binary both read `suppress`) and one
        likely-bug stub (IF-226, `getUsedCache` returns 0
        unconditionally; only caller is the debug-only
        `print`).  Cosmetic drift fixed in same commit; stub
        flagged with `\verifyme` in brief and explicit IF
        marker in recon comments, deferred to dedicated
        follow-up.  1600/1600 tests, build clean, 0 doxygen
        warnings.
  - [x] **D4 Batch 2e-iii follow-up (IF-226)** — fixed.
        `Prefetch::getUsedCache` body reconstructed at
        `prefetch_helpers.cpp:806-948`.  Simple recursive walk
        over `next`/`loop`/`branches` with per-node leaf
        contribution gated on `Node::type` and `PrefetcherNodeState::state`.
        Disassembly range `0x1c7eb0..0x1c8738` (next function
        `preparePlays` at `0x1c8740`).  **Correction to the IF**:
        original entry claimed the only caller was `Prefetch::print`
        (debug-only), but `getUsedCache` is in fact called from
        production code at `prefetch.cpp:1112,1118` (the
        `mergeWaveforms`-adjacent loop at `0x1cdcfd-0x1cddab`),
        where the result debits `nodeStates_[current].usedCache_`.
        Stub returning 0 silently turned that debit into a no-op
        and masked the `Load + length == 0` refund (negated
        contribution) entirely.  `computeWaveformMemoryBytes` is
        inlined in the body, matching the binary at
        `0x1c843a..0x1c8485` / `0x1c855d..0x1c8616`.  Tests
        remain 1602/1602 — the bug was latent because the
        difftest corpus doesn't stress mis-balanced cache
        accounting.  No regression test added (effect is on
        internal state, not ELF output).  IF-226 marked **fixed**
        in `incidental_findings.md`.
  - [x] **D4 Batch 3a** — `WavetableIR` accessors and the shared
        `detail::getUniqueName` helper (8 briefs):
        `getUniqueName`, `WavetableIR::{begin, end, size,
        getWaveformByName, getNextSegmentAddress,
        getFirstWaveformOffset, getJsonIndex}`.  Surfaced one
        latent bug (IF-227, `WavetableIR::size()` body divided
        the element count by `sizeof(shared_ptr)` = 16,
        silently yielding `count/16`; binary at `0x29e290` does
        the divide at the raw libc++ pointer level; replaced
        body with `manager_->waveforms_.size()` in same
        commit).  Latent because no live caller exists.
        1600/1600 tests, build clean, 0 doxygen warnings.
  - [x] **D4 Batch 3b** — `WavetableIR` ctors / dtor /
        serialization / allocation API (12 briefs):
        `WavetableIR(WavetableFront, ...)`,
        `WavetableIR(WavetableManager, ...)`,
        `~WavetableIR`, `toJson`, `fromJson`, `operator==`,
        `allocateWaveforms`, `forEachUsedWaveform`,
        `assignWaveIndexImplicit`, `setUsedWaveforms`,
        `allocateWaveformsForFifo`, `alignWaveformSizes`,
        `loadWaveform`.  Combined with the D3-era briefs for
        `updateWaveforms` and `assignWaveformAllocationSizes`,
        `WavetableIR` is now fully documented (every public
        method has a brief).  Headers-only diff; 1600/1600
        tests, 0 doxygen warnings.
  - [x] **D4 Batch 3c-i** — `WavetableFront` ctor / dtor /
        lifecycle methods (10 briefs): ctor, dtor,
        `dummyWarning`, `begin`, `end`,
        `setWarningCallback`, `getMemorySize`, `toString`,
        `loadWaveform`, `setLineNr`.  Headers-only diff;
        1600/1600 tests, 0 doxygen warnings.
  - [x] **D4 Batch 3c-ii** — `WavetableFront` factory and
        query/utility surface (13 briefs): `newEmptyWaveform`,
        `newWaveformFromFile` ×2, `newWaveform` ×2,
        `waveformExists`, `getWaveformByName`,
        `getWaveformByFunDescr`, `copyWaveform`,
        `checkWaveformInitialized`,
        `getWaveformSampleLength`, `updateDioTableUsage`,
        `assignWaveIndex`, `updateWave`.  `WavetableFront`
        is now fully documented.  Headers-only diff;
        1600/1600 tests, 0 doxygen warnings.
  - [x] **D4 Batch 3c-iii** — `WavetableManager<T>` templated
        method briefs (14) + 2 trivial frontend-lowering
        dtors.  Manager methods cover both `WaveformFront`
        and `WaveformIR` instantiations: `~WavetableManager`,
        `newEmptyWaveform`, `newWaveformFromFile` ×2,
        `newWaveform` (Front 5-arg, IR 4-arg),
        `getWaveformForFront`, `copyWaveform`, `updateWave`,
        `insertWaveform`, the IR-rebuild ctor, `toJson`,
        `fromJson`, `operator==`, `setLineNr`.  Frontend-
        lowering dtors: `FrontendLoweringContext::~` and
        `FrontendLoweringState::~`.  D4 Batch 3 (Wavetable
        namespace + frontend-lowering structs) closes here:
         59 briefs across 5 commits + 1 latent bug fix
         (IF-227).  1600/1600 tests, build clean, 0 doxygen
         warnings.
  - [x] **D4 Batch 4** — `CustomFunctions` class, complete
        coverage across 8 sub-batches:
        - **4a** (`e8a2600`): 22 briefs — periphery
          (`NodeMap`, exception classes, `PlayArgs`, free
          helpers).
        - **4b** (`03ebeea`): 28 briefs — ctor/dtor +
          dispatch / utilities (`call`, `checkFunctionSupported`,
          `checkExternalTriggeringMode`, `lookupNode`,
          `addNodeAccess`, etc.).
        - **4c** (`a3c6af5`): 5 briefs — DIO/ZSync built-ins.
        - **4d** (`0f91745`): 6 briefs — play-family thin
          wrappers.
        - **4e** (`3ebf2e3`): 10 briefs — `setID`,
          `assignWaveIndex`, `prefetch`, `prefetchIndexed`,
          `playAuxWave`, `playDIOWave`, `playWaveDIO`,
          `playWaveZSync`, `playZero`, `playHold`; surfaced
          IF-228 (pervasive integer-literal magic numbers,
          cosmetic-only).
        - **4f** (`bafa9a9`): 8 briefs — `waitWave`,
          `waitPlayQueueEmpty`, `sync`, `randomSeed`, `now`,
          `error`, `info`, `setRate`; corrected mask-label
          drift (`HDAWG | UHFAWG` → real masks; UHFAWG is not
          an enumerator).
        - **4g** (`fbc7f4a`): 16 briefs — every built-in in
          `custom_functions_wait.cpp`.
        - **4h** (`5821661`): 31 briefs — every built-in in
          `custom_functions_registers.cpp` (`setTrigger`,
          `getTrigger`, trigger family, `setInt`/`setDouble`,
          `generate`, user-reg family, `setPrecompClear`,
          `setWaveDIO`, `at`, `lock`/`unlock`, `getCnt`, QA
          family, `executeTableEntry`, PRNG family, `startQA`,
          `resetRTLoggerTimestamp`, sweep family,
          `configureFeedbackProcessing`).
        Header is now fully documented (0 undocumented
        `EvalResults`-returning declarations; 0 doxygen
        warnings with `WARN_IF_UNDOCUMENTED=YES`).
        1600/1600 tests passing throughout.
  - [x] **D4 Batch 5** — `AsmOptimize` class, complete coverage
        in a single commit (`25f5aa0`):
        - Audited the 5 pre-existing briefs
          (`OptimizeException`, class brief,
          `optimizePreWaveform`, `optimizePostWaveform`); all
          accurate against the binary, no IF needed.
        - 3 lifecycle briefs added: ctor, dtor,
          `prepareResources`.
        - 17 private-method briefs added: 5 query helpers
          (`isRead`, `isWritten`, `isLabelCalled`,
          `getNextActionForReg`, `registerIsNeverWritten`); 8
          peephole passes (`deadCodeElimination`,
          `oneStepJumpElimination`, `removeUnusedLabels`,
          `mergeLabels`, `mergeRegisterZeroing`,
          `removeUnusedRegs`, `reportUserMessages`,
          `simplifyAssign`); 4 register-allocator routines
          (`registerAllocation`, `splitConstRegisters`,
          `splitReg`, `registerUpdate`).
        - Flipped `EXTRACT_PRIVATE` to `YES` in
          `Doxyfile.in`.  Side effects elsewhere are zero
          because `EXTRACT_ALL=NO` keeps undocumented privates
          silently skipped tree-wide.
        - Corrected one stale legacy comment on
          `isLabelCalled` (scan range is `[begin, it)`, not
          "after `it`") in the same commit.
        1600/1600 tests, 0 doxygen warnings.
   - [x] **D4 Batch 6** — `WaveformGenerator` class, sub-batched.
         6a/6b/6c/6d complete; coverage audit confirmed 0
         undocumented WaveformGenerator members and 0 doxygen
         warnings, so no further sub-batches needed.
         - **6a** (`389b48e`): ~14 briefs — lifecycle / readers /
           shape helpers.  Surfaced IF-229 (cosmetic class brief
           overstatement).
         - **6b** (`a861b55`, fixes in `bf89075`): 18 simple-shape
           factory briefs (`zeros`, `ones`, `sin`, `cos`, `sinc`,
           `ramp`, `sawtooth`, `triangle`, `gauss`, `drag`,
           `blackman`, `hamming`, `hann`, `rect`, `mask`,
           `marker`, `rrc`, `vect`).  Surfaced IF-230 — fixed
           `rrc` 3-arg and `sinc` 4-arg arity-blind label
           strings.
         - **6c** (`83f4973`, fixes in `73a2e5f`/`d68f5a3`): 8
           briefs (`chirp`, `rand`, `randomGauss`,
           `randomUniform`, `lfsrGaloisMarker`, `placeholder`,
           `filter`, `circshift`).  Surfaced IF-231 (`rand`
           3-arg semantic bug, fixed) and extended IF-230 to
           `chirp` (7 spaced-vs-camelCase labels) and
           `lfsrGaloisMarker` (`"2 (marker)"` → `"2 (markerBit)"`).
         - **6d**: 9 combinator briefs (`join`, `add`,
           `interleave`, `scale`, `multiply`, `cut`, `flip`,
           `merge`, `grow`).  Verify-then-write against function
           bodies; existing IFs (IF-162, IF-176, IF-181, IF-188)
           cited in user-facing briefs.  No new IFs.
         1601/1601 tests, 0 doxygen warnings throughout.

- [x] **D-AUDIT-1 — Audit `WaveformGenerator` factory parameter-label strings**
      _(spawned from D4 Batch 6b verify-then-write; see IF-230)_
  - Background: during 6b audit, two factories (`rrc` at 0x254290
    and `sinc` at 0x24b6e0) were found to mislabel `read*` calls
    in their multi-arity overloads.  The binary uses inline
    `movabs $<8-byte-string>,%rax` immediates that are
    arity-blind: e.g. `rrc`'s 3-arg path uses `"3 (position)"` and
    `"4 (beta)"` even though the user passes them as args 2 and
    3, and `sinc`'s 4-arg path uses `"3 (position)"` and
    `"3 (beta)"` (same `"3 (beta)"` literal as the 3-arg form,
    not `"4 (beta)"`).  Both fixed in source as part of 6b.
  - Audit the remaining multi-arity factories for the same
    pattern: `gauss`, `sin`, `cos`, `sawtooth`, `triangle`,
    `drag`, `blackman`, `hamming`, `hann`, `chirp`, `rand`,
    `randomGauss`, `randomUniform`, `lfsrGaloisMarker`,
    `placeholder`, `vect`.  Method:
    ```
    objdump -d _seqc_compiler.so | awk '/^0+<entry> </,/ret/' \
      | grep -B1 'call.*read' | grep movabs
    ```
    decode each immediate as little-endian ASCII and compare to
    the recon's literal arguments.  For long strings (>15 chars)
    the binary uses `lea rip-rel + movups xmm0` from rodata
    instead of inline `movabs` — decode the rodata target with
    `objdump -s --start-address=0x<addr> --stop-address=0x<addr+N>`.
  - **Progress as of D4 Batch 6c (2026-05-10):**
    - [x] `chirp` (0x250bb0) — 7 labels fixed (camelCase
          `startFrequency`/`stopFrequency`/`phase` → spaced
          `start frequency`/`stop frequency`/`initial phase`)
          across 5/4/3-arg paths.  GDB-verified.  Commit `d68f5a3`.
    - [x] `rand` (0x251cf0) — 3-arg path was a *semantic* bug,
          not just a label; fixed under IF-231 (commit `73a2e5f`).
    - [x] `randomGauss` (0x252930) — re-audited; already correct
          (fixed under IF-205 earlier).
    - [x] `randomUniform` (0x253440) — re-audited; already correct.
    - [x] `lfsrGaloisMarker` (0x253bc0) — `"2 (marker)"` →
          `"2 (markerBit)"` (commit `d68f5a3`).
    - [x] `gauss` (0x24ddb0) — fixed under IF-232.
    - [x] `drag` (0x24e950), `blackman` (0x24f530),
          `hamming` (0x24fd20), `hann` (0x250250),
          `vect` (0x255570), `placeholder` (0x255850) —
          re-audited (D-AUDIT-1 follow-up sweep, 2026-05-10);
          all clean, no label drift.
    - [x] `sin` (0x24a0f0), `cos` (0x24abd0),
          `sawtooth` (0x24c8b0), `triangle` (0x24d330) —
          3-arg paths were a *semantic* bug (same shape as
          IF-231 `rand`): bound `(length, amplitude, phase)`
          but binary binds `(length, phase, nPeriods)` with
          `amplitude=1.0`.  Fixed under **IF-234** plus 8
          cosmetic 4-arg label drifts (`"3 (phase)"` →
          `"3 (phase offset)"`, `"4 (nPeriods)"` →
          `"4 (number of periods)"`).  Coverage test
          `hdawg_doc_trig_3arg.seqc` (manifest entry
          `trig_3arg`) added; 1602/1602 tests passing.
  - **Audit-method clarifications** (folded back into the
    recipe above as of 2026-05-10 sweep):
    1. Derive function end addresses from the `.text`
       symbol-table size (`objdump -t | grep <symbol>`),
       NOT from the next factory in a hand-curated list —
       e.g. `cos`'s nominal range overlaps `sinc`.
    2. Long parameter labels (≥16 chars) load via
       `lea rip-rel + movupd %xmm0` (SSE2 packed-double),
       not `movups` as the recipe suggested.  Match
       `movup[sd]` in audit scripts.
    3. `objdump -d` rip-rel comments use unprefixed hex
       (`# 905dc8`); regexes expecting `0x` after the comment
       marker silently miss them.
    4. `objdump -s` packs hex bytes into ~16 four-byte groups
       per line.  A `(?:[0-9a-f]{2,8} ?){1,4}` cap on the
       byte-group repetition truncates the dump.  Use
       `{1,8}` or `+`.
  - Likely cosmetic for the remaining factories (impacts
    user-visible error-message text only, no test currently
    exercises these error paths) but binary-faithfulness
    regressions should be fixed in the same commit as discovery,
    with each finding logged under IF-230's "Likely scope" section.
  - **Status: complete.**  All 16 multi-arity factories audited;
    semantic bugs (IF-231 `rand`, IF-234 trig family) and label
    drifts (`chirp`, `lfsrGaloisMarker`, `gauss`) all fixed.

---

## D5 — Internal helpers / opcodes / leaves

- [x] **D5 — Internal helpers / opcodes / leaves** _(complete; 95.2%
      coverage achieved, remaining 41 entries deemed not worth pursuit
      per D5-20 wrap-up)_
  - Scoping data captured 2026-05-10 from `reconstructed/build/docs/xml/`:
    coverage 874/3081 (28.4%); largest unbriefed compounds (gap = total - documented):
    - `zhinst::` namespace free functions / types: 177
    - `zhinst::Resources`: 70
    - `zhinst::AWGAssemblerImpl`: 39
    - `zhinst::DeviceConstants` + nested `SuserAddr`: 67
    - `zhinst::PlayConfig`: 34
    - `zhinst::AWGCompilerImpl`: 33
    - `zhinst::AsmParserContext`: 32
    - `zhinst::Signal`: 27
    - `zhinst::Cache`: 21
    - `zhinst::Waveform`: 24
    - `zhinst::WavetableFront`: 28 (WavetableManager: 26)
    - `zhinst::Resources::Function`: 19
    - `zhinst::AsmExpression` / `Assembler` / `MemoryAllocator` /
      `CachedParser` / `Exception` / `WaveformFront` (15-19 each)
    - `zhinst::SeqCArgList` / `SeqCDeclList` / `SeqCStmtList`: 15 each
  - Sub-batch order (proposed, may evolve):
    - [x] **D5-1** — PlayConfig + Signal + Waveform.
          Done in `1bbc46a`. +98 briefs, coverage 28.4% → 31.5%.
          3 new `\binarynote` (PlayConfig::operator!= asymmetry,
          PlayConfig::now exclusion, Signal::append non-const
          other due to checkAllocation mutation); 1 new
          `\unclear` on WaveformFile::columnMode (CSV
          column-mode descriptor — semantic role still TBD).
          No IFs surfaced.
    - [x] **D5-2** — DeviceConstants + SuserAddr + Register.
          Done in `0104d2b`. +68 briefs, coverage 31.5% →
          33.8%. All addresses cross-checked against
          `notes/special_registers.md`. No IFs; no `\unclear`
          / `\verifyme` / `\binarynote` added.
    - [x] **D5-3** — Cache + CachedParser. Done in `5b8462d`.
          +67 briefs, coverage 33.8% → 35.9%. 4 new
          `\binarynote` (Cache::getCachedFile miss-via-empty,
          cacheFile byte-budget estimate, cacheFileOutdated
          parameter-name surprise, CacheEntry::pinned_ omitted
          from serialize). No IFs.
          - [ ] **Follow-up (low-priority)**: write
                `reconstructed/notes/cache.md` consolidating
                (a) the PointerState machine
                (play / resetPlay / reuse), (b) the splitting
                heuristic in Cache::allocate(5-arg) (cross-refs
                `unknowns.md` #63), (c) the nameMap semantics in
                getBestPosition (cross-refs #62, already
                resolved). Currently this knowledge lives only
                in cache.cpp body comments. Not blocking.
          - [ ] **Follow-up (cosmetic)**: rename
                `cacheFileOutdated` parameter `name` →
                `cachedElfPath` in the public header to match
                the .cpp body. Improves API ergonomics but is a
                reconstruction-only relabel; needs to be
                explicitly flagged that the binary signature is
                unchanged.
    - [x] **D5-4** — Resources family. Done in `74c5012`.
          +120 briefs, coverage 35.9% → 39.8%. Five new
          `\binarynote` (Function::addArgument diagnostic line
          number, Function::getBody null-deref hazard,
          variableExistsInScope grandparent fallback, checkVar
          lax type acceptance, readCvar TypeMismatchWrite +
          literal "CVAR" diagnostic). One new `\verifyme`
          (StaticResources::errorReportTarget — see IF-235).
          IF-235 logged (low/confirmed): orphan declared-but-
          undefined helper.
    - [x] **D5-5** — Asm parser/expression family. Done in
          `0bf65ce`. +98 briefs, coverage 39.8% → 43.0%. Eight
          new `\binarynote` (comment-state cross-locking
          no-ops, clearSyntaxError name-vs-effect mismatch,
          cleanStringCopies pointer invalidation, addCommand
          double-null contract, highestRegisterNumber full
          register-index preservation). One new `\verifyme`
          on str(shared_ptr<AsmExpression>) — see IF-236.
          IF-236 logged (medium/confirmed): missing
          reconstruction of live binary symbol at 0x28cd20
          (1373 bytes, ostringstream-based AsmExpression →
          text serialiser).
          - [ ] **Follow-up (D-class work, not blocking)**:
                reconstruct
                `str(shared_ptr<AsmExpression>)` body. No test
                exercises this path so it can wait.
          - [ ] **Follow-up (cross-reference doc)**: write
                `notes/parser_contexts_compared.md` cross-
                referencing AsmParserContext and
                SeqcParserContext (near-twin structures: same
                comment-state machine, same errorCallback_ +
                boost::log fallback, same line-counter
                pattern). Saves future agents from re-deriving
                the parallel.
    - [x] **D5-6** — AWGAssemblerImpl + AWGCompilerImpl. Done
          in `c251ebe`. +42 briefs, coverage 43.0% → 44.4%.
          Lower symbol delta than expected because Doxygen
          merges declaration + out-of-line definition into a
          single documented symbol. Seven new `\binarynote`
          (assembleAsmList LABEL no-advance, writeToFile
          opcode-buffer clear, parserMessage syntax-error
          flip, compileString silent state reset, addWaveforms
          unrecognised-extension recording, writeToStream
          silent no-output on prior syntax error,
          writeAssemblerToFile silent no-op on empty text).
          Two new inline `\unclear` (unusedStr038_, pad_218_;
          not counted by coverage.sh regex). IF-237 logged
          (low/confirmed, fixed-cosmetic): the
          '// ---- Error reporting helpers ----' header
          comment claimed only errorMessage flips
          setSyntaxError; recon body of parserMessage also
          flips it. Captured via `\binarynote`; subagent
          suggests a GDB pass.
          - [ ] **Follow-up (low priority, GDB)**: verify
                IF-237. One breakpoint at 0x289190 + a
                watchpoint on parserCtx_+0x03 confirms whether
                the binary really flips hadSyntaxError_ in
                parserMessage, or whether the recon body
                copy-pasted from errorMessage by mistake.
          - [ ] **Follow-up (D-class, low priority)**: extract
                pipeline orchestration notes from
                awg_compiler.cpp block-headers into
                `notes/awg_compiler_impl.md`, leaving each
                `// ====` as a one-line address comment. The
                writeToStream ELF-section emission order and
                device-conditional branches (mapped vs absolute
                mode, `.nodes` vs `.nodes_json`) currently live
                only in scattered comments.
    - [x] **D5-7** — Waveform-IR remainder: WavetableFront /
          WavetableManager / WaveformFront. Done (2d547ae).
          Scope-discovery finding: WavetableFront /
          WavetableManager were already fully briefed in
          D4-3c-i/ii/iii (445c0c0, 91d9e41, f08c46f); the
          genuinely-uncovered file in this scope was
          `waveform_front.hpp` (struct-level brief only).
          Result: +19 briefs on WaveformFront (5 fields,
          4 declared methods, 9 inline accessors) plus 1
          brief on `WavetableManager<T>` default ctor.
          One new \\binarynote (WaveformFront 3-arg ctor
          inlined into WavetableManager::newWaveformFromFile).
          No IFs surfaced. Coverage 44.4% → 45.0%.
    - [x] **D5-AUDIT** — Reconstruction-comment preservation
          audit + restoration (IF-238). Done (bd0c54b).
          Four-way subagent audit of `git diff 0116fe7..HEAD`
          surfaced 14 silently-dropped recon-only `//` lines
          across 5 files (binary-address anchors + opcode
          rename history + 'only ctor in binary' note);
          all restored as // comments adjacent to the //!
          briefs. One STALE/CORRECTED case
          (`Function::addArgument` `VarSubType_Stub` vs
          `_FunctionArg`) verified — //! is correct, pre-pass
          // text was drifted; forensic record in IF-238.
          Style-guide §14 added: 'content preservation, not
          form preservation' rule; verify-then-write applies
          to deletion as well as addition. Build clean,
          docs 0-warning, 1602/1602.
    - [x] **D5-8** — Remaining smaller compounds: Exception,
          MemoryAllocator, SeqC*List trio. Done (5d08a34).
          Three parallel subagent dispatches, each held to §14
          coexistence rule. Result: +78 documented symbols,
          coverage 45.0% → 47.5%.
          - core/exception.hpp: 17/17 documented; 2 new
            \\binarynote (empty-msg default substitution,
            description() return type surprise).
          - codegen/memory_allocator.hpp: 19/19 documented; 1
            new \\verifyme on the ctor (no standalone symbol;
            field-init reconstructed from inlined call sites).
          - ast/seqc_ast_node.hpp: 14 briefs added inside the
            SEQC_LIST macro; propagated to all three list
            classes via Doxygen macro expansion. Per class:
            14/15 documented (private elements_ remains
            undocumented per Doxygen norms).
          - Implementation finding: single-line //! briefs
            inside backslash-continued macro bodies break the
            macro (// extends to end of spliced logical line,
            eating subsequent macro tokens). MUST use /*! */
            for every brief inside a macro regardless of
            length. See comment_style_guide.md follow-up below.
          - Diff is pure-additive (0 lines removed verified
            via \`git diff | grep -cE '^-[^-]'\` = 0).
    - [x] **D5-8 follow-up — macro doc-comment style note**
          Done in this commit. Added §15 to
          `comment_style_guide.md` covering three macro-body
          pitfalls: (15.1) `//!` inside backslash-continued macro
          bodies eats subsequent tokens — must use `/*! */`
          regardless of brief length; (15.2) `MACRO_EXPANSION=YES`
          can mis-attach briefs to nearby macro invocations,
          producing spurious "argument X not found" warnings —
          inline `\param` prose in `\details` as the workaround
          (encountered with `BOOST_LOG_GLOBAL_LOGGER` in D5-20);
          (15.3) macro-defined symbol families need a per-invocation
          brief plus an enclosing `\name` group.
    - [x] **D5-9** — `zhinst::` namespace free functions
          (diffuse; tackle by file).  Closed in three parallel
          sub-batches:
          - D5-9a: core/exception.hpp + .cpp (53 entries — 26
            ZHINST_DECLARE_EXCEPTION + 26 ZHINST_DEFINE_EXCEPTION
            invocations + 2 macro defs + makeDefaultErrorCode).
            Macro briefs don't propagate; per-invocation //!
            required.  \name family group wraps the 26
            declarations.
          - D5-9b: infra/calver.hpp + .cpp (11 entries — all
            CalVer helpers and operators).
          - D5-9c: device/awg_device_props.hpp + .cpp (27
            entries — AwgSequencerType + enumerators + primary
            template + 9 specialisations + 3 free helpers).
            Per-enumerator `// toString -> "X"` comments
            preserved verbatim below new //!< briefs per §14.
          Total +91 documented; coverage 47.5% → 50.4% (1463 →
          1552; crossed 50% threshold).  Commit 025eb8b.
    - [x] **D5-10** — Remaining `zhinst::` namespace free
          functions (61 entries across 26 files).  Closed in
          three parallel sub-batches:
          - D5-10a (AST cluster, 25): node.hpp enum operators
            (\name group, 6); EValueCategory + enumerators (4);
            11 swap() + 2 str() in seqc_ast_node.cpp;
            expression.cpp helpers (3); toSeqCAst;
            operator<<(AddressImpl<uint32_t>).
          - D5-10b (Error/Core cluster, 14): ErrorMessageT
            enum, errMsg singleton, 3 resource-id strings
            (\name group), 4 *None sentinels, getApiErrorMessage,
            runningOnMfDevice, awg2double/marker stubs,
            xmlEscapeSeqToInt, compileSeqc entry point, S/BF
            typedefs in \cond INTERNAL.
          - D5-10c (Codegen/Device/Misc cluster, 22):
            OptPassFlag + RegAction enums, asm_register::isValid,
            LabelBimap, sectionAsString, custom_functions enums
            + operator<, NodeTypeIdx, WaveOrder, nextID,
            getCmdMap, awg2double/marker/double16 forward decls,
            computeWaveformMemoryBytes, DeviceType operators (4),
            getNodeMapForDevice, floatEqual.
          Two IFs: IF-239 (awg2marker uint8/uint16 ODR latent),
          IF-240 (DeviceOptionSet operator== summary
          understates check).  §14 verified — 2 mechanical
          -lines were content-equivalent reformats, no // recon
          comments lost.  Coverage 50.4% → 52.9% (+76 net);
          namespacezhinst undoc gap: 61 → 0. Commit 048d357.
    - [x] **zhinst:: free-function gap eliminated** end of D5-10.
          ~150 entries documented across D5-8/9/10.
    - [x] **D5-11..D5-15** — Bulk class-member sweep across 27
          files in parallel sub-batches (subagent-dispatched).
          AST cluster (53 SeqC AST classes including macro-body
          field briefs for SEQC_BINARY/SEQC_UNARY/SEQC_LIST
          generators), device cluster (32 device subclasses +
          ZHINST_DECLARE_FACTORY invocations + DeviceType /
          DeviceOptionSet), asm cluster (Hirzel/Cervino variants,
          AsmCommands family), waveform cluster (rawwave,
          wave_index_tracker, wavetable_front, waveform_generator,
          wavetable_ir), io cluster (elf_reader, elf_writer,
          zi_folder), runtime cluster (csv_parser, get_node_map,
          wavetable_manager_{front,ir}). +929 documented (+30.1pp);
          coverage 52.9% → 83.0%. Class-member gap 1048 → 80.
          §14 hex-token-count audit verified pure-additive on
          all 6 files where D5-14 had 137 line deltas (proven
          §14-compliant `//!` rewrites, not `//` recon removals).
          Subagent-dispatch contract refined after D5-13
          self-mis-classified as complete and D5-14 rewrote
          existing `//!` briefs: explicit "verify before edit",
          "pure additive", and pre/post gap-scan reproducibility
          required. Commit 396adf3.
    - [x] **D5-16** — redispatched after D5-13 self-mis-classified
          as "complete". 111 members across 11 classes
          (CustomFunctions/Compiler/Prefetch/AWGAssemblerImpl/
          AWGAssembler + 6 small): all → 0. +110, 83.0% → 86.6%.
          0 deletions, +3 `\unclear`, +2 `\binarynote`. Commit
          188bd25.
    - [x] **D5-17** — final class-member sweep: 69 → 0 across 33
          small classes. +79, 86.6% → 88.9%. Macro-body field
          briefs for SEQC_BINARY/SEQC_UNARY/SEQC_LIST propagate
          to 4/5/3 instantiations respectively; per §15.3 the
          per-class invocation brief is still required.
          0 deletions. Commit b289920.
    - [x] **Class-member gap closed** end of D5-17.
          1048 → 0 across 161 classes (D5-11..D5-17 cumulative).
    - [x] **D5-18 + D5-19** — top zhinst:: structs (~104:
          AWGCompilerConfig 29, Expression 13, AsmRegister 12,
          WaveformIR 11, Prefetch::PrefetcherNodeState 11, CalVer
          11, PlayArgs 10, AwgPathPatterns 10, AwgDeviceProps 10,
          PlayArgs::WaveAssignment 7) plus long-tail structs +
          nested-namespace free functions (~80: FrontendLoweringContext
          6, ErrorCode 6, AddressImpl 5, FrontendLoweringState 5,
          AsmList::Asm 5, Prefetch::UsageEntry 4, util::wave 7,
          tracing 3, logging 3, etc.). +171 documented;
          coverage 88.9% → 94.4%. zhinst:: undoc 237 → 77.
          35 line "deletions" across 6 headers verified
          §14-compliant: old "// +0xNN — outer type tag" recon
          comments folded into //!< briefs on the same line with
          the offset annotation preserved verbatim; hex-token
          counts preserved 1:1 in all 6 files. 2 `\unclear` on
          `serializeRoundTrip` / `optimizationFlags` (no
          reconstructed consumer); 1 `\binarynote` on
          `CalVer::triple`. Commit 6256e53.
    - [x] **D5-20** — final long-tail sweep targeting the 33
          real (non-anonymous-namespace) gaps left after D5-18/19:
          struct members (AddressImpl, AsmList::Asm,
          UsageEntry, AWGAssemblerImpl::Message, Register
          anon-enum briefs, Waveform::File typedef);
          namespace briefs + free functions (tracing namespace
          + getDefaultLabOneResource + makeDefaultSpanProcessor,
          logging namespace + Severity + ZiLogger macro +
          logging::detail + logExceptionToClog, util namespace +
          util::wave + double2awg/double2awg1m/double2awg16/
          hash/hash2str, sfc namespace, detail namespace +
          makeDeviceFamilyFactory). Five missing `\param` /
          `\return` slots filled after WARN_NO_PARAMDOC=YES
          surfaced them; one BOOST_LOG_GLOBAL_LOGGER attachment
          warning worked around per §15.2 by inlining `\param`
          prose. Coverage 94.4% → 95.2%; zhinst:: undoc 77 → 41
          (35 anonymous-namespace artefacts + 6 stragglers).
          Commit 3ce51ec.
    - [x] **D5 complete.** Coverage 28.4% → 95.2% across the full
          phase. 1602/1602 tests passing throughout; 0 doxygen
          warnings at end. 7 `\unclear`, 10 `\verifyme`, 82
          `\binarynote`. New IFs introduced this phase: 235–240.
