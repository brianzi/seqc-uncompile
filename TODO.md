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
  - [ ] **D4 Batch 2c follow-up (IF-213)** — reconstruct
        `Prefetch::findLockedPlay` (currently a stub at
        `prefetch_helpers.cpp:388-424`).  GDB-confirmed during
        IF-216 investigation that the binary at `0x1d3dd0` is a
        ~1.4 KB function that walks the tree, dispatches on Play
        (`0x02`) and Unlock (`0x80`) types, performs `bcmp` on
        waveform names, and returns the matched node via the
        success path at `0x1d43ad`.  Recon stub always returns
        `nullptr`; tests pass only because `prepareTree`'s
        `createLoad` fallback happens to produce the same ELF for
        the trivial `lock(w); playWave(w); unlock(w);` pattern in
        the corpus.  Reconstruction is non-trivial:
        jump-table-style dispatch, two `bcmp` sites, dual return
        paths, weak_ptr handling.  Deferred to a dedicated
        reconstruction effort.  See IF-213 for full evidence.
        GDB driver: `tests/gdb/gdb_trace_lock.py`; recipe:
        `tests/gdb/gdb_findlocked_trace.txt`.
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
  - [ ] **D4 Batch 2d follow-up (IF-217)** — fix
        `Prefetch::backwardTree` to enqueue `cur->loop` instead of
        re-enqueueing `cur->next` in the third visitor block
        (`prefetch_helpers.cpp:259-268`).  Currently loop-body
        children never receive a parent back-link.  GDB-trace the
        original binary at `0x1d57d0` on a SeqC program with a
        `repeat (...)` loop to confirm the third dispatch reads
        `+0xE0` (`loop`) rather than `+0xB8` (`next`); flip the
        two field reads; add a regression test asserting
        `parent.lock()` is non-null on a node inside a loop body
        after `backwardTree` runs.  Tests will likely remain
        1600/1600 — the bug is latent because `Node::parent` is
        already populated earlier in the lowering pipeline.  See
        IF-217 for full evidence.
  - [ ] **D4 Batch 2d follow-up (IF-218)** — reconstruct
        `Prefetch::expandSetVar` body
        (`prefetch_helpers.cpp:352-375`, original `0x1d3af0`).
        Current recon body walks the sibling chain and constructs
        `make_shared<Node>(NodeType::SetVar, ...)` clones in a
        `for (i=1; i<numGroups; ++i)` loop, but the clones are
        never linked into the IR — the temporary goes out of
        scope each iteration.  `objdump -d
        --start-address=0x1d3af0 --stop-address=0x1d3dd0
        _seqc_compiler.so` to identify the per-clone field-copy
        and splice; GDB-trace on a multi-channel-group SeqC
        program that exercises SetVar emission to confirm the
        per-group splat semantics; add regression test.  See
        IF-218 for full evidence.
  - [ ] **D4 Batch 2d follow-up (IF-219)** — investigate whether
        `Prefetch::createLoad` is missing an "already-loaded"
        early-return that the legacy block-header documented.
        `objdump -d --start-address=0x1d4a10
        --stop-address=0x1d5040 _seqc_compiler.so` to confirm
        whether the binary actually has the `parent.lock() &&
        loadRef set → return null` guard before the
        `Resources::getRegisterNumber` call.  If yes, add the
        guard.  Bug is latent on the current call graph (each node
        is only visited once per pass).  See IF-219 for full
        evidence.
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
  - [ ] **D4 Batch 2e-i follow-up (IF-223)** — reconstruct the
        Table-case (`nodeType == 0x200`) tail in
        `Prefetch::placeSingleCommand`
        (`prefetch_placesingle.cpp:1023-1063`, original
        `0x1d8b3c..0x1dba0d`).  Current recon emits only the
        leading cwvf and inserts an incomplete tempList.
        `objdump -d --start-address=0x1d8b3c
        --stop-address=0x1dba0d _seqc_compiler.so` to identify
        the smap / ssl loop / addr / prf instruction sequence
        (the comment claims it mirrors the cervino_nonsplit
        emission already in the file).  GDB-trace the original
        on a SeqC program exercising `playWave(table, ...)` on a
        multi-channel device to confirm which branches fire and
        with what register / size values; reconstruct the tail;
        add a regression test that diffs the `.text` / `.asm`
        sections.  Tests currently 1600/1600 because no case in
        the corpus reaches this divergence.  See IF-223 for full
        evidence.
  - [ ] **D4 Batch 2e-i follow-up (IF-224)** — reconstruct the
        `play_cervino_indexed_nonsplit` label body in
        `Prefetch::placeSingleCommand`
        (`prefetch_placesingle.cpp:862-868`, original
        `0x1db562..0x1db6f8`).  Currently an empty block falling
        through to `play_finalize` with the descriptive comment
        as the only content (wwvf + ssl loop + addr + prf with
        `clampToCache`).  `objdump -d
        --start-address=0x1db562 --stop-address=0x1db6f8
        _seqc_compiler.so` for the body; cross-reference with
        the sibling `play_cervino_indexed2_hirzel` block
        (lines 870-915) which has a full reconstructed body in
        the same pattern.  GDB-trace on a multi-Cervino indexed
        playWave with a small waveform (so `pagesNeeded < 2`)
        to confirm; add a regression test.  Tests currently
        1600/1600 because no case in the corpus reaches this
        path.  See IF-224 for full evidence.
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
  - [ ] **D4 Batch 2e-iii follow-up (IF-226)** — reconstruct
        `Prefetch::getUsedCache` body
        (`prefetch_helpers.cpp:799-815`, original `0x1c7eb0`).
        Currently a stub that always returns 0.  `objdump -d
        --start-address=0x1c7eb0 --stop-address=...
        _seqc_compiler.so` to identify the function body and
        end address (expected to be the next `.text` symbol
        boundary).  Determine the recursion shape (likely walks
        `Node::next`, `Node::loop`, `Node::branches` summing
        per-leaf waveform memory via
        `computeWaveformMemoryBytes`, or sums
        `PrefetcherNodeState::usedCache_` across visited
        nodes).  Reconstruct the body.  Optionally add a unit
        test asserting expected total over a hand-crafted Node
        tree; difftests will not exercise this since the only
        caller is `Prefetch::print` (debug-only printer to
        `std::cout`).  See IF-226 for full evidence.
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
   - [ ] **D4 Batch 6** — `WaveformGenerator` class, sub-batched.
         6a/6b/6c/6d complete; remaining sub-batches TBD if
         further coverage gaps appear.
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

- [ ] **D5 — Internal helpers / opcodes / leaves**
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
    - [ ] **D5-1** — PlayConfig + Signal + Waveform (~85 briefs).
          Three small POD/value classes used across codegen and
          waveform synthesis.  Low risk; high downstream
          visibility (every later brief that mentions these
          types becomes self-linking).
    - [ ] **D5-2** — DeviceConstants + SuserAddr (~67 briefs;
          mostly bitmask / address constants).  Pairs with the
          existing `cervino_vs_hirzel.md` and `special_registers.md`
          notes.
    - [ ] **D5-3** — Cache + CachedParser (~39 briefs).
    - [ ] **D5-4** — Resources family (~89 briefs incl.
          Resources::Function).  Symbol-table backbone; likely
          to surface IFs.
    - [ ] **D5-5** — Asm parser/expression family (~78 briefs):
          AsmParserContext, AsmExpression, Assembler.
    - [ ] **D5-6** — AWGAssemblerImpl + AWGCompilerImpl (~72
          briefs); private impl classes whose facades were
          documented in 7a.
    - [ ] **D5-7** — Waveform-IR remainder: WavetableFront /
          WavetableManager / WaveformFront.
    - [ ] **D5-8** — Remaining smaller compounds: Exception,
          MemoryAllocator, AsmExpression, SeqC list types, etc.
    - [ ] **D5-9** — `zhinst::` namespace free functions
          (diffuse; tackle by file).
  - Workflow: each sub-batch follows the AGENTS.md
    verify-then-write rule.  Subagent dispatch is encouraged
    for mechanical sweeps; user-verifies before commit.
    Sub-batches commit independently; tests + docs warning
    count between commits.

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
