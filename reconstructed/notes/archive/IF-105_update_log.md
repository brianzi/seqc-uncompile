# IF-105 update log (archived 2026-05-08)

This file holds the chronological "update 1..6" sections plus the
"IF-103 / IF-105 status summary" that were originally inlined under
`## IF-105` in `reconstructed/notes/incidental_findings.md`.  They
document the multi-pass debugging journey for the
`uhf_doc_tv_mode` indexed-play emission (binary `0x1da77f`).

The IF-105 entry in `incidental_findings.md` retains the problem
statement, GDB-verified execution flow, and final status; the
detailed update history below was archived to keep the main notes
file focused on current-state findings.

Status at archive time: **fixed (Phase 41 series, all 6 updates
applied)**.  `uhf_doc_tv_mode` byte-identical; full suite
2499/2499 (1600 main + 774 stress + 92 doc + 6 errors + 14 labone +
13 large).

Cross-references:
- Main entry: `reconstructed/notes/incidental_findings.md` `## IF-105`
- Sibling entry: `## IF-103` (parent investigation)
- Phase 41 series in `TODO.md`

---

### IF-105 update (after partial implementation attempt)

The `load_indexed_play` body has been reconstructed in
`reconstructed/src/codegen/prefetch_placesingle.cpp` (matching binary 0x1da77f
for the no-Repeats / non-Hirzel / `< minIndexedSize` branch).  However,
**the recon never reaches it** for `uhf_doc_tv_mode`.

A debug fprintf at step 4 confirmed:
```
STEP4: lengthReg.valid=0 val=-1 indexOff.valid=0 val=-1
```

The Load node's `lengthReg` and `indexOffsetReg` are BOTH invalid in
the recon, whereas the original binary takes step 4's "lengthReg
valid && != 0" branch (verified via GDB).  So the missing wiring is
**upstream**: the Load node needs its `lengthReg` (the same lengthReg
field that lives on the Play node from the indexed-play call) to be
copied or referenced.

Likely culprits — places where Load and Play nodes are paired up:
- `prepareTree` / Load-creation pass (where Load nodes are extracted
  from playWaveIndexed)
- The shared-back-reference (`loadRef`) wiring — perhaps the play's
  lengthReg should also be propagated to the load
- The "case 1 falls through to case 2" merger artifact (Phase 10.8b
  comment in the file header)

**This is now a separate root cause** distinct from the missing
`load_indexed_play` body.  The body is correct and ready for use once
the upstream wiring is fixed; until then the body is dead code.

**Action items** (to be promoted to TODO.md):
- Investigate where `Node::lengthReg` is set during prepareTree / load
  extraction in the Cervino path.
- GDB-trace the original at the play→load wiring point to see where
  the load's lengthReg is populated (likely during a copy from a
  paired play node).

### IF-105 update 2 (root cause located, partial fix landed)

**Status**: partially fixed — wiring activated, emission body now
runs; remaining diff is placement/ordering, not wiring.

The lengthReg is set correctly on the FIRST Load node by
`Prefetch::createLoad` (prefetch.cpp:2085, copies from the source Play
node's lengthReg which `asmPlay` populates from `indexReg`).  However,
`Prefetch::moveLoadsToFront` (prefetch.cpp:0x1ccad0) creates a SECOND
Load node and BFS-walks the tree to find matching Loads (cur->type ==
Load with matching wave name).  When found, it inherits the matched
Load's `play` weak_ptrs into the new Load and splices the old Load out.
The recon (and likely the original) did not previously copy
`cur->lengthReg` into the new Load — so the new Load that survives in
the tree, and therefore the one that reaches `placeSingleCommand` step
4, had `lengthReg.isValid() == false` and the indexed-play emission
path was not taken.

**Fix** (this commit): in moveLoadsToFront's namesMatch block (recon
line ~841 / binary 0x1cd313), copy `cur->lengthReg` into
`loadNode->lengthReg` before splicing cur out.  Guarded so the assignment
only occurs when loadNode is currently invalid and cur is valid.

**Test impact**: `uhf_doc_tv_mode` now emits the
`addiu/addi/addi/ssl/addr/wwvf/prf/wprf` block from `load_indexed_play`,
and the spurious pre-loop `prf R2, R1, 252000` is gone.  Recon ELF
260568 → 260728 (orig 260688).  Test still fails on three remaining
issues:
1. The new block is emitted BEFORE the for-loop instead of INSIDE the
   loop body (placeLoads ordering).  Original puts it after `false5:`
   inside the for body.
2. Spurious `addi R3, R1, 0; ssl R3, R3` still emitted inside the loop
   from the play_cervino_indexed path (IF-103 part 2).
3. `prf` immediate differs (orig 2528 from `clampToCache(cacheSize/2)`
   on smaller cacheSize; recon 126000 = full size/2).  Likely linked to
   issue 1 — the load is placed at the wrong scope so its cache size
   accounting differs.
4. Register allocation differs (orig R3,R6 vs recon R3,R2).

These are no longer wiring issues — they are emission-body /
placement issues.  The dead-code activation goal of IF-105 is met.
Suite score remains 256/259 (no regression).

**Action items** (remaining, to be promoted to TODO.md):
- Investigate placeLoads ordering for indexed-play loads (why the
  load isn't placed inside the for-loop body).
- Suppress the spurious play_cervino_indexed emission of addi+ssl
  when the indexed-load has already emitted them (IF-103 part 2).
- Verify clampToCache argument matches binary semantics for the
  in-loop case.

### IF-105 update 3 (placeLoads no-split path activated; getRequiredMemory fixed)

**Status**: partially fixed — moveLoadsToFront no longer hoists the
indexed Load; emission now lands inside the for-loop body.

GDB-traced the original on `uhf_doc_tv_mode` and discovered that
`placeLoads()` actually takes the **no-split** path (does not call
`moveLoadsToFront`) for this case.  Recon was incorrectly taking the
split path because `Prefetch::getRequiredMemory()` was reconstructed
with the wrong page-count formula.

**Root cause in getRequiredMemory** (prefetch_helpers.cpp:120):
The inner loop computes `numPages` from the binary at `0x1cc9d0`:
```
r9  = DC+0x40 (waveformGranularity)   ; floor cap, NOT cap
r10 = DC+0x44 (waveformPageSize)      ; round-up divisor
eax = roundUp(numRepeats, r10)
eax = max(eax, r9)                    ; cmova at 0x1cc9ea — was reconstructed as min
```
The recon had `numPages = min(rounded, granularity)`, which clamped
massive waveforms (e.g. 126 000 samples) down to the granularity (16),
so `getRequiredMemory()` returned 32 instead of 252 000 for tv_mode.
That fooled `placeLoads` into the split path → moveLoadsToFront →
indexed-Load hoisted out of the loop.

**Fix** (this commit): change `min` to `max` and rename locals to
`granularityFloor`/`pageSize` to match binary semantics.

**Test impact for `uhf_doc_tv_mode`**:
- Before: pre-loop emission of `addiu/addi/wwvf/prf/wprf` block
  (260 728 bytes, blocks BEFORE `while1:`).
- After: block correctly emitted inside the for-loop body
  (260 760 bytes, after the wtrigs).  No-split path confirmed via
  `cmp eax,ecx; jbe split` Intel-syntax disassembly.
- Suite score still 256/259, no regressions.

**Remaining diff for tv_mode** (3 issues, all in load_indexed_play
emission body, not placement-pipeline):
1. Ordering inside loop body: original places the indexed-Load setup
   right after `false5: brz R4, end7` (start of body); recon places
   it after the user statements (setID + wtrigs).  Likely the
   placeholder used by load_indexed_play is being inserted at the
   wrong scope position.
2. Extra `wwvf` emitted (recon has two, original has one).  Need to
   re-check the load_indexed_play body — possibly the
   `cacheSize >= minIndexedSize` branch incorrectly emits both
   wwvf paths.
3. `prf` immediate differs (orig 2528 from
   `clampToCache(cacheSize/2)` with the in-loop cache size; recon
   42016 with a different cache size).  Linked to issue 1 or to
   `clampToCache` argument semantics.
4. Register allocation differs (orig R3,R6 vs recon R4,R3) —
   downstream of the above.

These remain open as IF-105 follow-ups.

### IF-105 update 4 (indexed-allocation cache size + clampToCache args fixed)

**Status**: partially fixed — load setup now byte-correct
(`addiu/addi/addr/wwvf/prf 2528/wprf` matches original), but
placement and `play_cervino_indexed` body still wrong.

GDB-traced the cache allocation path for tv_mode and found two
distinct bugs:

1. **Cache::allocate path selection** in `Prefetch::allocate` (the
   "splitPath" block at recon line 1622, binary 0x1d1a9b..0x1d1efc).
   The original takes a *separate* indexed-allocation path at binary
   0x1d1e01 when `playNode->length != 0` — it bypasses the full-wave
   size calculation and instead uses
   `numSamples = playNode->length * channels * 2`.  Recon was always
   using the full-wave formula (rounding `signal.length` to pageSize
   then multiplying by channels and bps/8), giving cache size 84032
   for tv_mode where original gets 2528.

2. **`load_indexed_play` prf immediate** in `prefetch_placesingle.cpp`
   at the prf call (binary 0x1dab7d-0x1dab87).  The recon called
   `clampToCache(cacheSize/2)` but the binary loads
   `mov 0x4(%rax),%esi` (= `cachePtr->size_`, no division) and passes
   that directly to `clampToCache`.  Removed the `/2`.

**Fixes** (this commit):
- `prefetch.cpp` splitPath: branch on `playNode && playNode->length != 0`
  → use indexed numSamples formula.  Otherwise keep existing full-wave
  formula.
- `prefetch_placesingle.cpp` load_indexed_play: drop the `/2` in the
  clampToCache call.

**Test impact**:
- `uhf_doc_tv_mode`: 260760 → 260728 (orig 260688, 40 bytes off).
- The `addiu R3, R0, 851969 / addi R6, R0, 0 / addi R4, R1, 0 /
  ssl R4, R4 / addr R3, R4 / wwvf / prf R3, R6, 2528 / wprf` block
  now appears with correct immediate (2528).
- Suite still 256/259, no regressions.

**Remaining structural issues** (not fixed in this pass):

A. **Placement of load_indexed_play tempList**: the load setup is
   currently inserted at the END of the for-loop body (after user
   `setID/wait/setTrigger/wtrigs`), but the original places it at
   the START of the body (right after `false5: brz R4, end7`).  The
   placeholder used by case-1 Load may be at the playWaveIndexed
   site instead of the loop-entry site.  Likely needs investigation
   of where `placeholder` (Asm with matching asmId) ends up after
   `placeLoads` runs in the no-split path.

B. **Missing wvf instruction**: original emits
   `wvf R6, R0, 2520` (R6 = load's regC, length = `playNode->length *
   channels * 2 = 2520`).  The recon's `play_cervino_indexed`
   currently emits `addi R3, R1, 0; ssl R3, R3` (the addi/ssl
   computed at binary 0x1dac9a..0x1dae1d) but does NOT call
   `Prefetch::wvfs` (visible at binary 0x1db9c9 inside the common
   indexed-finalize 0x1db6f8..0x1db942).  The addi/ssl appear to be
   emitted to a tempList that should be discarded or routed
   elsewhere; only the `wvf` should reach the output stream.
   Significant rework of the play_cervino_indexed body needed.

C. **Register allocation** differs (orig R3,R6 vs recon R4,R3) —
   downstream of the alloc-order changes from A/B.

These three are no longer "instruction-level" tweaks but require
restructuring of the play-side indexed emission and the placement
pipeline.  Deferred.

### IF-103 / IF-105 status summary

- IF-103 part 1 (`load_indexed_play` body missing): **fixed** in
  prior commit (a247dfb).
- IF-103 part 2 (`play_cervino_indexed` extra addi+ssl, missing
  wvf): **fixed** in this commit (see IF-105 update 5 below).
- IF-105 wiring (`lengthReg` propagation in moveLoadsToFront):
  **fixed** in 6aa0602.  However for tv_mode the wiring is moot
  because moveLoadsToFront should not run at all (see update 3).
- IF-105 update 3 (`getRequiredMemory` formula): **fixed** in
  prior commit.
- IF-105 update 4 (indexed cache-size + clampToCache(/2)):
  **fixed** in prior commit.
- IF-105 update 5 (`play_cervino_indexed` non-split body):
  **fixed** in this commit.
- IF-105 update 6 (`Prefetch::optimize` Load+SetVar branch:
  inverted parentLoad type check at 0x1cdeae): **fixed** in
  this commit — see update 6 below.  This was the root cause
  of both the placement (A) and register-allocation (C)
  diffs in tv_mode.
- tv_mode now byte-identical (suite 257/259, +1, no regressions).

### IF-105 update 6 (optimize() inverted parentLoad type check)

**Status**: fixed — `uhf_doc_tv_mode` now byte-identical.
Suite went from 256/259 to 257/259 with no regressions.

**Site**: `Prefetch::optimize`, the Load-with-SetVar-parent
branch around binary 0x1cdeae (recon `prefetch.cpp:1110`).

**Bug**: the recon implemented the parentLoad-type gate as
"rewrite asmId only when parentLoad is a Loop", i.e.

```cpp
if (parentLoad && parentLoad->type == NodeType::Loop) {
    curNode->asmId = parent->asmId;
}
```

The actual binary control flow at 0x1cdeae is the opposite:

```
1cdeae: cmp DWORD PTR [rax+0x44], 0x8     ; parentLoad->type == Loop?
1cdeb6: je   1ce685                       ; YES → SKIP rewrite
1cdebc: mov  rax, [rbp-0xc0]              ; (parent shared_ptr)
1cdec3: mov  eax, [rax+0x14]              ;   eax = parent->asmId
1cdec6: mov  [r14+0x14], eax              ;   curNode->asmId = parent->asmId
1cdeca: jmp  1ce685
```

The rewrite happens **only when parentLoad->type != Loop**.
For tv_mode, parentLoad is itself a SetVar (type 0x10), so:
- original: 0x10 != 0x8 → rewrite Load.asmId 44 → 32 (the
  SetVarPlaceholder asmId)
- buggy recon: 0x10 != 0x8 → check fails → asmId stays at 44

The asmId difference cascaded into:
1. **Issue A (placement)**: the surviving Load's asmId
   determines the placeholder lookup target in
   `placeSingleCommand` / placement helpers.  asmId=32 sits
   before user statements in the AsmList, asmId=44 sits
   after — so the load setup block was emitted at the END of
   the for-body in recon vs the START in the original.
2. **Issue C (registers)**: with the load setup emitted at
   a different point in the assembly stream, the
   register allocator processes the temp-register
   requests in a different order, yielding R3/R4 vs R6/R3.

Fixing the polarity of the parentLoad type check fixes
both diffs in one shot.

**GDB verification recipe** (the key trace that revealed
the bug):

```
hooks at:
  0x1cbfbf  cmp eax,ecx          (required vs cacheSize)
  0x1cc250  cmp [rbx+0xbc],0     (split_ check)
  0x1cc327  call optimize         (optimize entry)
  0x1cdec6  mov [r14+0x14],eax    (asmId rewrite site)
```

For tv_mode the original prints:
- required=252000 cacheSize=131072  → required > cacheSize
- at 1cc250 split_=0
- calling optimize
- 1cdec6 SetVar+lengthReg match: cur=0x... oldAsmId=44 → newAsmId=32

The recon (with debug fprintfs added at the same C++ sites)
showed:
- required=252000 cacheSize=131072 appendMode=0
- split_=0 before optimize check
- Prefetch::optimize called
- visiting Load node asmId=44 parent=SetVar (type 16)
- Load with SetVar parent: regsMatch=1
- regsMatch but parentLoad type wrong: type=16

— the third-line diagnostic ("type=16") immediately
identified the inverted gate.  Re-examining the disasm at
0x1cdeae confirmed `je SKIP` (skip when type==Loop) rather
than `jne SKIP` (skip when type!=Loop).

**Lesson**: when a binary has a `cmp X, K; je TARGET`
pattern, the rewrite (or whatever is in the fallthrough
path) executes when `X != K`.  This is easy to invert when
hand-translating to C++ because the natural English reading
of the comment ("if parentLoad type is Loop, then ...") often
ends up describing the **skipped** branch rather than the
executed one.  Always cross-check polarity with a runtime
trace when the recon predicate looks right but the
behavior is wrong.

### IF-105 update 5 (play_cervino_indexed non-split body reconstructed)

**Status**: fixed — the recon now emits the correct single
`wvf R*, R0, 2520` instruction for tv_mode's play_cervino_indexed
non-split path.

**GDB trace** of the original on `uhf_doc_tv_mode` revealed the
actual control flow taken at `play_cervino_indexed` (0x1dabb9):

```
0x1dabb9 (entry)
  → cmp [r15+0xbc], 1      ; check split_
  → jne 0x1db60f             ; NOT split → take non-split branch
0x1db60f (non-split)
  → emplace nodeStates_[node] → registerCervino  saved in [rbp-0x148]
  → length = node->length  saved in [rbp-0x58]
  → emplace → cachePtr->size_ loaded
  → cmp size, minIndexedSize
    jae 0x1dbb04           ; large-cache: totalSize = length * channels
    fall:                  ; small-cache: totalSize = length * channels * 2
  → jmp 0x1dbb6d → 0x1d9d3a
0x1d9d3a (shared wvfImpl emission)
  → cmp deviceType, 2; jne 0x1d9d4e
  → cmp pageCount, [devConst+0xc]; jne 0x1d9ee2
0x1d9ee2 (single wvfImpl)
  → wvfImpl(registerCervino, totalSize, is4Ch)  ; emits "wvf R6, R0, 2520"
  → append result to tempList
  → check lengthReg.isValid() && != 0 && !split_ && cachePtr->size_ >= minIdx
    → cachePtr->size_ (2528) < minIndexedSize (4096) → jb finalize
0x1dba0d (play_finalize)
  → insert tempList into output
```

The previous recon had the non-split branch (the `else` block in
`play_cervino_indexed`) emitting addi+ssl based on the binary at
0x1daed4, but 0x1daed4 is a different scenario altogether
(another split-check entry-point), not the actual non-split path
of 0x1dabb9.  The actual non-split path is the 0x1db60f →
0x1dbb6d → 0x1d9d3a chain that converges into the same wvfImpl
emission used by play_cervino_nonsplit (without the indexed
gate).

**Fix** (this commit): replace the `else` branch of
`play_cervino_indexed` with code that:
1. Reads `node->length`, `nodeStates_[node].registerCervino`,
   `nodeStates_[node].cachePtr->size_`, and the wave's
   `signal.channels_`.
2. Computes totalSize via the cachePtr->size_ vs minIndexedSize
   branch (large-cache: length*channels; small-cache:
   length*channels*2).
3. Calls `wvfImpl(registerCervino, totalSize, is4Ch)` and appends
   the result to tempList.
4. If `cachePtr->size_ >= minIndexedSize`, additionally emits the
   addi/addi/prf/wprf block (the 0x1d9fab path).

For tv_mode, only step 3 produces output (size < minIdx → step 4
skipped) — matching the `wvf R*, R0, 2520` in the original.

**Test impact**:
- `uhf_doc_tv_mode`: ELF size 260728 → 260688 (matches original).
- `.asm` size 1229 → 1211 (matches original).
- Test still fails because:
  - Issue A (placement): the load_indexed_play tempList is
    inserted at the END of the for-loop body (after user
    `setID/wait/setTrigger/wtrigs`); the original places it at
    the START (right after `false5: brz R4, end7`).
  - Issue C (registers): orig allocates R6 for registerCervino
    and R3 for registerHirzel; recon allocates R3 and R4
    respectively.  Likely downstream of the alloc-order
    changes from prior fixes.
- Suite score 256/259, no regressions.

These two issues are placement-pipeline / resource-allocation
problems, not codegen-body problems.  Deferred.

