# Incidental Findings

Discrepancies, suspicious patterns, and possible bugs discovered while
investigating differential test failures.  Items here are **not necessarily
the root cause** of the test that exposed them — they are leads that need
independent verification against the binary.

Each entry has:
- **Source**: which investigation surfaced it
- **Status**: open / confirmed / dismissed
- **Severity**: cosmetic / suspicious / likely-bug

---

## Archive: IF-1..IF-99

Entries IF-1 through IF-99 (all `fixed`, `dismissed`, or `confirmed`)
are archived in [`archive/IF_1-99.md`](archive/IF_1-99.md).  They
document the early reconstruction era (Phases 1–35) and are not
referenced from any current source file.

The active sections below cover **IF-100..IF-200**, which include the
IDs cited from `reconstructed/src/` and `reconstructed/include/`
comments.

---

## IF-100  `playIndexed` indexed-play codegen still incorrect

**Source**: `uhf_doc_tv_mode` post-IF-99 investigation
**Status**: Phase 12 Var-branch fixed; remaining diff traced to
IF-102 (placesingle wrong field) and IF-103 (incomplete
play_cervino_indexed body).
**Severity**: likely-bug

After IF-99 fix, `uhf_doc_tv_mode` still differs in `.asm`/`.text`/`.linenr`.

The failing test calls `playWaveIndexed(w_array, t, waveform_length)`
where `t` is a runtime variable (Var=2) and `waveform_length=1260` is a
const. Per Phase 12 comment in `custom_functions_play.cpp:1055-1059`:

> For varType == Var(2): @0x161f5c..0x161f6a takes a different
> branch — pulls a pre-computed AsmRegister from a stack-saved slot
> at [rbp-0x328] and skips the addi/SetVarPlaceholder pair. That
> path has not been independently traced; modeled as the same
> logical operation here.

The recon currently always emits the Const/Cvar `addi indexReg, R0,
Immediate(rate)` path. For Var(2), the original instead reuses a
pre-computed register (likely the register holding `t`) so the play's
length/index can vary per loop iteration.

Concrete asm diff:
- **Original** (per-iteration prefetch hoisted *inside* the loop):

  ```
  for4:  addi R3, R1, -125999
         ...
         addiu R3, R0, 851969
         addi R6, R0, 0
         addi R4, R1, 0
         ssl R4, R4         ; scale loop var t
         addr R3, R4        ; add to base address
         wwvf
         prf R3, R6, 2528   ; per-row prefetch (2520+pad)
         ...
         wvf R6, R0, 2520   ; per-row play (1260*2 channels)
  ```

- **Recon** (single static prefetch outside loop, full-wave play):

  ```
  cwvf 5242753
  addiu R1, R0, 851969
  prf R1, R6, 252000     ; whole-wave prefetch
  ...
  for4: ...
        wvf R6, R0, 252000  ; plays the entire 252000-sample wave
  ```

The recon's `playIndexed` is essentially treating the call as a static
playWave of `combined`, ignoring the index/length args at codegen time.

**Suspected root cause**: the Var-path of Phase 12 must (a) skip the
`addi`/`asmSetVarPlaceholder` pair, (b) install the pre-computed
register holding `t` as `indexReg`, and (c) ensure `regVal` /
`indexOffsetReg` flow into `asmPlay` so the resulting Play node carries
both the per-iteration length (1260*channels) and the offset register
— driving optimization passes to keep the prefetch *inside* the for
loop.

**Next steps**:
1. GDB-trace original at @0x161f5c..0x161f6a with the failing input to
   confirm exactly which AsmRegister slot is read from `[rbp-0x328]`
   and how it threads into `asmPlay`.
2. Check `asmPlay`'s `regVal` / `reg` / `reg2` / `regInv` semantics
   when called with a non-zero indexReg/lengthReg — likely the post-
   waveform optimization pass is responsible for hoisting the prefetch
   out, but only does so when `wvf` length is a constant.
3. After Phase 12 Var-path fix, verify the prefetch hoisting matches
   by inspecting the optimization pipeline (`optimizePostWaveform`).

This is a substantial subsystem investigation in its own right —
deferred from the IF-99 fix to keep that change focused.

## IF-101  removeBranches missing push of node->next in multi-branch path

**Severity**: bug (fixed)
**Status**: fixed

In `Prefetch::removeBranches` (0x1d3530), when the branch retains one or
more children (multi-branch path, originally @0x1d3759-0x1d37b6), the
binary pushes `node->next` (the join node after the branch) onto the
traversal stack BEFORE pushing each branch shared_ptr. The
reconstruction was only pushing the branches.

Without the `node->next` push, `prepareTree`'s BFS terminated as soon
as the branch arms (typically leaf Plays with `next = nullptr`) were
processed. Any code following the if/else (e.g. `wait(3000); playWave;
if/else`) was never visited by `prepareTree`, so its Play nodes never
had `linkLoad` called on them, leaving `loadRef` empty, and triggering
`prefetch Error: missing load for node` in `Prefetch::allocate`.

**Test**: uhf_doc_feedforward (now byte-identical, was failing).
**Score**: 254 → 255 of 259 passing.
**Fix**: prefetch_helpers.cpp `removeBranches` else branch — push
`n->next` before iterating branches.

## IF-102  Prefetch::placeSingleCommand uses wrong field (indexOffsetReg vs lengthReg)

**Source**: `uhf_doc_tv_mode` post-IF-100 investigation
**Status**: partially fixed
**Severity**: bug (fixed for indexed-play dispatch; other call sites
unaudited)

`Prefetch::placeSingleCommand` in `prefetch_placesingle.cpp` had
multiple sites that referenced `node->indexOffsetReg` (+0x94) where
the binary actually reads `node->lengthReg` (+0x88).  The two fields
are adjacent and easy to confuse, but binary verification at the
following addresses confirms the correct offset is +0x88:

- 0x1d91d4: `add $0x88, %rdi` before `isValid` — the cervino_nonsplit
  → cervino_indexed dispatch check (recon line 520 in original file).
- 0x1d9f3b/0x1d9f50: post-wvf indexed-play dispatch (recon line 588).
- 0x1dac6f: `mov 0x88(%rax), %r13` then passed as 3rd arg to
  `addi(idxReg, ?, Immediate(0))` in cervino_indexed_split branch
  (recon line 658).
- 0x1d9953: same pattern in cervino_indexed2_hirzel (recon line 743).

Without these fixes, `play_cervino_indexed` was never entered, even
when the playWaveIndexed had a runtime offset register set in
`lengthReg` by `asmPlay`.  The recon was unconditionally taking the
`play_cervino_nonsplit` static-prefetch path.

**Fix** (this commit): updated lines 520, 588, 658, 693, 743 to read
`node->lengthReg`.  Other `indexOffsetReg` references in the file
(lines 190, 457, 466) were NOT audited and may also be wrong; left
in place pending verification per their own binary addresses.

**Test impact**: `uhf_doc_tv_mode` now reaches `play_cervino_indexed`
and emits the `addi R3, R1, 0; ssl R3, R3` indexed-prep instructions,
moving the recon ELF from 260532 → 260568 (orig 260688).  Test still
fails — the remaining diff is the body of `play_cervino_indexed`
itself which is incomplete (see IF-103).

## IF-103  `play_cervino_indexed` body is not fully reconstructed

**Source**: `uhf_doc_tv_mode` post-IF-102 investigation
**Status**: fixed (Phase 41 series — see "IF-103 / IF-105 status summary"
below at line ~1889 and "IF-105 update 5" at line ~2001).
- Part 1 (`load_indexed_play` body) fixed in commit a247dfb.
- Part 2 (`play_cervino_indexed` extra addi+ssl, missing wvf) fixed
  in IF-105 update 5 (non-split body reconstructed).
- Verified: `uhf_doc_tv_mode` byte-identical, all suites green
  (1600/1600 main, 774/774 stress).
**Severity**: bug (substantial reconstruction work)

After IF-102, `uhf_doc_tv_mode` enters `play_cervino_indexed` but the
emitted instruction sequence is incomplete relative to the original.

**UPDATE (post-GDB trace)**: The bulk of the missing instructions are
NOT emitted by `play_cervino_indexed` (0x1dabb9) at all — they are
emitted by the **case 1 Load** node going through the indexed-Load
path at **0x1da77f**, which the recon currently stubs out as
`load_indexed_play` → `load_finalize`.  See IF-105 for the full
breakdown of which node emits which instructions.

Original (UHFLI tv-mode loop, indexed playback):

```
addiu R3, R0, 851969     ; addi for cwvf encoding (>=0x1000000)
addi  R6, R0, 0          ; stateRegC
addi  R4, R1, 0          ; copy lengthReg (R1) into R4
ssl   R4, R4             ; shift left for 2-channel
addr  R3, R4             ; R3 += R4 (offset address)
wwvf
prf   R3, R6, 2528       ; per-iter prefetch with idx=R3, base=R6
wprf
... (trigger writes) ...
wvf   R6, R0, 2520       ; per-iter play
```

Recon emits only `addi R3, R1, 0; ssl R3, R3` and is missing the
addr / wwvf / prf / wprf / wvf sequence.  Additionally, recon emits a
spurious static `prf R2, R1, 252000` BEFORE the loop (likely a leftover
from the prepareTree pipeline still treating the wave as a static
play; the Load placement may need to follow the indexed dispatch).

**Source ranges to reconstruct** (binary addresses already noted in
`prefetch_placesingle.cpp` comments):
- 0x1dabb9..0x1db1ff (split branch): SSL loop body, addr emission,
  optional wwvf, prf with clampToCache, optional wprf.
- 0x1daed4..0x1db4xx (non-split branch): same logic.
- 0x1db6f8..0x1db942 (common indexed finalize): assembly emission of
  the per-iteration play instructions, including the wvf with
  `node->length * channels * 2` byte count (matching the
  `totalSize` computation already present).
- Common cwvf-encoded addiu emission upstream (the `addiu R3, R0,
  851969` instruction precedes the indexed addi+ssl block — its
  source needs to be located).

Estimated work: comparable in scope to a fresh recon of a 200+ byte
function region, including SSL loop, addr/wwvf/prf clamping, and
cwvf encoding.  Deferred from the IF-102 fix to keep that change
focused.

**Test impact**: 1 test still failing (`uhf_doc_tv_mode`).  Suite
score unchanged at 255/259.

---

## IF-104  ELFIO NOBITS section file-offset layout differs between vendored and system builds

**Source**: `shfsg_doc_ct_placeholder` test failure investigation
**Status**: confirmed
**Severity**: cosmetic (diff-test artifact, not a semantic bug)

The original binary statically links its own copy of ELFIO, while the
reconstruction links against the system `elfio` package.  The two
versions assign different `sh_offset` values to `SHT_NOBITS` sections.

For `shfsg_doc_ct_placeholder.seqc` (two `placeholder()` waveforms via
`assignWaveIndex`):

| section                      | original off | recon off |
|------------------------------|--------------|-----------|
| `.wf___placeholder_3_0` NB   | 0x1000       | 0x1000    |
| `.wf___placeholder_4_1` NB   | 0x1000       | 0x1040    |
| `.text` PROGBITS             | 0x1000       | 0x1040    |

Both sets of headers are semantically equivalent — NOBITS sections
declare address+size only and occupy no file bytes.  But because
`diff_test.py` was reading raw bytes at `[sh_offset, sh_offset+size)`
for *every* section, the byte-overlap between NOBITS and the colocated
PROGBITS produced spurious "content" diffs.  The original happened to
overlap NOBITS over `.text` (so both ph3 and ph4 "data" looked like
text bytes), while the recon padded the NOBITS gap with zeros before
`.text`.

**Determinism check**: original output is byte-identical across 5
consecutive runs — the bytes are not uninitialized memory, just
file-offset overlap.

**Fix**: `tests/diff_test.py` now special-cases `SHT_NOBITS` sections
to compare type/size/addr instead of file bytes (the same semantics
ELF loaders use).  No code change in `reconstructed/`.

**Test impact**: `shfsg_doc_ct_placeholder` now passes, suite score
255 → 256/259.

## IF-105  `case 1` Load with `lengthReg!=0` (path 0x1da77f) is the actual emitter for indexed-play setup

**Source**: `uhf_doc_tv_mode` deeper investigation (extends IF-103)
**Status**: fixed (Phase 41 series, all 6 updates applied — see
"IF-103 / IF-105 status summary" below at line ~1889 and "IF-105
update 6" at line ~1911 which closed the final placement+regalloc
divergence). `uhf_doc_tv_mode` byte-identical; suite went
256/259 → 257/259 → 1600/1600 main + 774/774 stress (all green
post-Phase-62).
**Severity**: bug (substantial work; ~50–100 lines of new emission code)

### GDB-verified execution flow for `uhf_doc_tv_mode`

```
1st placeSingleCommand call (Load, no wave):
  case 1 → load_fallback → static_prf_path_load (0x1d8013)
  emits: cwvf 5242753

2nd placeSingleCommand call (Load, has wave + lengthReg != 0):
  case 1 (0x1d79f8)
  → step 1: alloc registerHirzel
  → step 2: addi(regH, R0, addrValue)        ; emits "addiu R3, R0, 851969"
  → step 3 (cervino, next!=null):
    addi(regC, R0, cachePos)                  ; emits "addi R6, R0, 0"
  → step 4: lengthReg.isValid() && != 0
    → JMP 0x1da77f (NOT 0x1d85f6 / load_cervino_prf)

  At 0x1da77f (the "load_indexed_play" body, NOT yet implemented):
  - check nodeStates_[node].cachePtr->numRepeats_ >= 2  (0x1da7a8)
    if yes: wwvf()                             (0x1da7b9, NOT taken on this test)
  - alloc tempReg                              (0x1da7da)
  - addi(tempReg, lengthReg, Imm(0))           ; emits "addi R4, R1, 0"
  - SSL loop over wave's numPages (channels @ +0xc8):
      ssl(tempReg)                             ; emits "ssl R4, R4"
      (loops `numPages` times — for tv_mode wave, channels=1 so 1 iter
       wait actually channels for waveAtCurrentDeviceIndex returns
       sample-pages count, need to double-check; output shows 1 ssl)
  - addr(stRegHirzel, tempReg)                 ; emits "addr R3, R4"   (0x1daa86)
  - check cacheSize >= minIndexedSize          (0x1daad2)
    if yes: jmp 0x1db562 (alternative path)
  - wwvf()                                     ; emits "wwvf"          (0x1daae9)
  - prf(stRegH, stRegC, clampToCache(cacheSize/2))
                                              ; emits "prf R3, R6, 2528" (0x1dab9f)
  - jmp 0x1db911 → 0x1db92e finalize:
    if !isHirzel: wprf()                      ; emits "wprf"          (0x1db937)
  - insert tempList into output at placeholder  (0x1db963)

3rd placeSingleCommand call (Play indexed):
  case 2 (play_entry, 0x1d7d49)
  → eventually reaches play_cerv_indexed_split (0x1dabb9)
  Per .asm output, this emits ONLY:
    wvf R6, R0, 2520
  (despite the 0x1dabb9 disassembly containing addi+ssl+... — those
   results may go into a different list/placeholder, or the addi+ssl
   are computed but not appended in this register-set; needs further
   GDB tracing of the play call to verify what is actually appended.)
```

### Recon's incorrect output

The recon currently emits, BEFORE the for-loop:
```
addiu R2, R0, 851969    ← from case 1 step 2 (regH addi)
addi  R1, R0, 0         ← from case 1 step 3 (regC addi)
prf   R2, R1, 252000    ← from load_cervino_prf path B2 (full waveformMemorySize)
wprf                    ← from load_finalize
```
But the original places ALL of these (with adjusted prf size, and with
extra ssl/addr/wwvf in between) INSIDE the for-loop, via the 0x1da77f
path, which is not yet reconstructed.

### What needs implementing

1. **`load_indexed_play` body** (case 1 step 4 → 0x1da77f path) — this
   currently `goto load_finalize` and produces nothing useful.  Needs:
   - `numRepeats >= 2` check; emit wwvf() if so
   - alloc tempReg
   - addi(tempReg, lengthReg, Imm(0))
   - ssl loop over `numPages` (waveform's signal.channels_ +0xc8)
   - addr(stRegHirzel, tempReg)
   - `cacheSize >= minIndexedSize` branch (if yes → emit per the
     0x1db562 alternative path: addi + addi + wprf + prf with
     numRepeats etc.; if no → emit wwvf+prf+wprf as above)
   - For the no branch (this test's path):
     wwvf() + prf(stRegH, stRegC, clampToCache(cacheSize/2)) + wprf()
   - Insert tempList at placeholder

2. **`play_cervino_indexed` actual emission** — needs further GDB
   tracing to determine exactly which instructions reach the OUTPUT
   stream (only `wvf` is observed in the .asm).  May involve:
   - Dispatch to a separate placeholder for the per-iter wvf
   - The addi+ssl+addr+wwvf computed at 0x1dabb9 may be DEAD CODE for
     this configuration, or emitted to a side stream

3. **Fixes to existing `load_cervino_prf` path** — currently always
   takes Path B2 (cacheSize != waveformMemorySize → full size prf),
   producing the `prf R2, R1, 252000`.  This pre-loop emission must
   not happen when lengthReg!=0; the indexed_play branch (item 1)
   should run instead.  Step 4's `goto load_indexed_play` is correct;
   the bug is just that load_indexed_play is a stub.

### Estimated scope

- ~30 lines for the `load_indexed_play` body (item 1, no-Repeats branch)
- ~20 lines for the `>=minIndexedSize` alternative (the 0x1db562 path)
- Need a 2nd GDB session to nail down what `play_cervino_indexed`
  actually appends to output (item 2)
- Risk of regressing other tests that take case 1 step 4 (any indexed
  load on Cervino/Hirzel devices)

Recommendation: implement in two patches —
(a) load_indexed_play (no-Repeats, no-Hirzel, < minIndexedSize) —
    targets exactly tv_mode and verifies the diff reduces to just the
    `wvf` instruction;
(b) play_cervino_indexed wvf emission — only after (a) is verified.

### IF-105 update history (archived 2026-05-08)

Updates 1–6 plus the "IF-103 / IF-105 status summary" that
documented the Phase 41 multi-pass debugging journey have been
moved to
`reconstructed/notes/archive/IF-105_update_log.md` to keep this
file focused on current-state findings.  The problem statement,
GDB-verified execution flow, and final status above are
preserved.  All updates have been applied; `uhf_doc_tv_mode` is
byte-identical and the full suite is 2499/2499.

## IF-106  Inline `asm volatile("maxsd …")` in `util_wave.cpp` replaced with `_mm_max_sd` intrinsic

**Source**: Phase 39a code-smell sweep
**Status**: fixed (2026-04-29, Phase 39a)
**Severity**: cosmetic + portability cleanup (no behavioural change on
non-NaN input, identical machine code on NaN input)

### What was there

`util_wave.cpp:96` (single inline-asm site in the entire codebase):

```cpp
double lo = -1.0;
asm volatile("maxsd %1, %0" : "+x"(lo) : "x"(sample));
scaled = lo * kFullScale;
```

The accompanying comments included a half-finished alternative
(`double clamped = (sample >= -1.0) ? sample : -1.0;`) that was
computed and then dead-stored, plus a "Let's just use:" preamble
that left the rationale unclear.

### Why the inline asm existed

The binary uses x86 SSE2 `maxsd` whose NaN-propagation rule is
"if either operand is NaN, return the second source". No portable
C++ construct replicates this exactly:

| Construct | NaN behaviour | Matches `maxsd src, dst`? |
|---|---|---|
| `std::max(-1.0, NaN)` | unspecified (typically returns first arg) | ✗ |
| `std::fmax(-1.0, NaN)` | returns the non-NaN argument | ✗ |
| `(sample >= -1.0) ? sample : -1.0` | NaN compares false → returns `-1.0` | ✗ |
| `_mm_max_sd(_mm_set_sd(-1.0), _mm_set_sd(sample))` | returns second source on NaN | ✓ |

### Fix

Replaced with the SSE2 intrinsic from `<emmintrin.h>`. GCC, Clang
and MSVC all lower `_mm_max_sd` to a single `maxsd`, so the emitted
instruction is identical to the original binary's. The `clamped`
dead variable and stale comment were removed; rationale was
collapsed into a single coherent block.

Verified: `cmake --build .` clean, `python tests/diff_test.py`
remains 257/259 (same baseline; the two failures are the unrelated
libc++ PRNG ABI cases).

### Scan-coverage note

The earlier Phase-39 read-only scan used regex
`\basm\s*[\(\{]|__asm__|__asm\b` which missed `asm volatile "…"`
(the `volatile` qualifier sits between `asm` and the string literal,
which is followed by a string token rather than `(`). The correct
pattern is `\basm\s+volatile|\basm\s*[\(\{]|__asm__`. After this
fix, the codebase has zero inline-asm sites under either pattern.

## IF-107  `Prefetch::determineFixedWaves` enqueued `next` twice causing O(2^N) BFS

**Status**: fixed (Phase 42 series).
**Severity**: critical (perf), confirmed-fixed
**File**: `reconstructed/src/codegen/prefetch_helpers.cpp:589` (function
`Prefetch::determineFixedWaves`, binary 0x1cb200)

**Symptom**: `hdawg_cvar_unroll` test ran 155× slower than original
(4500ms vs 29ms). Profile via GDB attach-sampling showed 100% of
samples in this function's worklist pop / shared_ptr destruction.
Per-loop timing showed exponential growth: 1 loop=3ms, 2=6ms, 3=13ms,
4=390ms, 5=5500ms.

**Root cause**: The recon enqueue block was

```cpp
if (cur->next) worklist.push_back(cur->next);
for (auto& child : cur->branches) worklist.push_back(child);
if (cur->next) worklist.push_back(cur->next);  // BUG (was elseBranch)
```

The duplicate `next` push doubles the worklist on every step in a
sibling chain → O(2^N). The "(was elseBranch)" comment captured the
origin: during recon, two distinct Node fields were collapsed onto
`next`.

**Truth from binary disassembly** (0x1cbb80..0x1cbe17):
- Always push `next` (+0xB8) if non-null.
- If `type == Loop` (0x8): push `loop` (+0xE0) if non-null.
- If `type == Branch` (0x4): iterate `branches` (+0xC8..+0xD0) and
  push each.
- Loop and Branch are mutually exclusive; the binary uses a single
  `cmp eax, 0x8 / je / cmp eax, 0x4 / jne` chain.

The recon was wrong on two axes: it pushed `branches` unconditionally
(should be Branch-only) and pushed `next` twice (should be `loop` for
Loop nodes only).

**Fix**: Replaced enqueue block with the type-gated logic matching the
binary; moved enqueueing to the end of the loop body where the binary
places it (executed for all node types, not only Play).

**Result**: `hdawg_cvar_unroll` 4500ms → 11ms. Full suite 257/259
preserved (only the 2 known libc++ PRNG ABI failures remain).

## IF-108  `WaveformGenerator::rand` uses MINSTD LCG, not MT19937_64

**Status**: fixed (Phase 42 series — `seqc_minstd_normal_amplitude` added
to `prng_libcxx.cpp`; `hdawg_doc_random_waves` and `hdawg_doc_randomSeed`
now byte-identical).
**Severity**: critical (correctness), confirmed-fixed
**File**: `reconstructed/src/waveform/waveform_generator.cpp:1548`
(`WaveformGenerator::rand`, binary 0x251cf0); algorithm in
`reconstructed/src/infra/prng_libcxx.cpp:seqc_minstd_normal_amplitude`.

**Symptom**: `hdawg_doc_random_waves` and `hdawg_doc_randomSeed`
differential tests failed (recon `.wf___rand_*` bytes differed from
original). Initial assumption: libc++ vs libstdc++ `mt19937_64` +
`normal_distribution` ABI mismatch. Recon was routing `rand` through
the same libc++ MT19937_64 shim used by `randomGauss` and
`randomUniform`. The shim was correct for the latter two, but
produced output that matched the binary's `randomGauss` for `rand` —
indicating `rand` and `randomGauss` use **different** PRNGs.

**Root cause**: `WaveformGenerator::rand` does NOT use the shared
`GlobalResources::random[]` MT19937_64. It uses a custom inline
**Park-Miller MINSTD LCG** with state reset to 1 at the start of
every call, followed by a Marsaglia polar Box-Muller transform.

**Truth from binary disassembly** (0x251cf0..0x252800):

- Initial LCG state: `mov $0x1, %edx` at 0x25255c (constant seed=1
  per call — verified by `hdawg_doc_randomSeed` producing two
  byte-identical waveforms despite an intervening `randomSeed()`).
- Multiplier: `imul $0xbc8f, %rdx, %rcx` at 0x2525f0 (= 48271, the
  Park-Miller minstd multiplier).
- Modulus reduction by `2^31 - 1`: vectorized Granlund-Möller mod
  using `mul r12` (r12 = 0x200000005) plus `shr/shl/sub/add` chain.
- 4 LCG samples per Box-Muller trial (vectorized into xmm regs).
- Two uniforms in `[-1, 1)` per trial via the libc++/libstdc++
  uniform_real_distribution-style combine pattern using rodata
  constants at 0x8fc680..0x8fc6e0:
    * 0x8fc680: low-32 bitmask `[0xFFFFFFFF, 0]×2`
    * 0x8fc690: `2^52` (low-half bias)
    * 0x8fc6a0: `2^53` (high-half bias)
    * 0x8fc6b0: `2^53 + 2^32` (combined bias for unbiasing)
    * 0x8fc6c0: `2^31 - 2 = 2147483646` (range divisor)
    * 0x8fc6d0: `(2^31 - 2)^2 ≈ 4.6116e18` (range²)
    * 0x8fc6e0: `-1.0` (offset to map [0,2) → [-1,1))
  Combine: `u = 2 * (low + high * (2^31-2)) / (2^31-2)^2 - 1`.
- Marsaglia polar rejection at 0x252711..0x252723:
  `s = u² + v²`; if `s ≥ 1` (`ucomisd s, xmm10=1.0; ja`) or `s == 0`
  (`ucomisd s, 0; jne done; jnp regen`), regenerate.
- Acceptance: `factor = sqrt(-2 ln(s) / s)`. Two normals per pair.
- **Output emit order**: lane 1 (`v*factor`) first via `unpckhpd`,
  then lane 0 (`u*factor`) cached and emitted on the next outer
  iteration. Verified by sample comparison.
- Final sample: `(z * stddev + mean) * amplitude` then
  `Signal::append(value, 0)`.

**Cross-check with sibling functions**:
- `randomGauss` (0x252930) calls `std::__1::normal_distribution<double>::
  operator()<mersenne_twister_engine<...>>` at 0x253207, with TLS
  state from `GlobalResources::random` — pure libc++ MT19937_64.
- `randomUniform` (0x253440) inlines the MT19937_64 tempering
  (`shl 0x25, xor; shr 0x2b, xor; ...`) and uniform_real_distribution
  conversion at 0x2539a0..0x2539e0. Also pure libc++ MT64.
- `randomSeed` (custom_functions_playback.cpp:875) calls
  `Random::seedRandom` (0x16be80) which seeds the MT19937_64 state.
  It does NOT touch the LCG (which is reset to 1 every `rand()`
  call regardless), so `randomSeed()` is effectively a no-op for
  `rand` but reseeds `randomGauss`/`randomUniform`.

**Why the libc++ shim approach worked for randomGauss/randomUniform
but not rand**: the libc++ vs libstdc++ Box-Muller pair-order
hypothesis was correct for those two functions (which DO use libc++
`normal_distribution` / `uniform_real_distribution`). But `rand` was
never using the stdlib distribution at all — it was using the
hand-rolled inline MINSTD + polar implementation in the binary, so
no stdlib shim could fix it. Reverse-engineering the assembly
revealed the actual algorithm.

**Fix**: Added `seqc_minstd_normal_amplitude()` to
`reconstructed/src/infra/prng_libcxx.cpp` (portable C++; no stdlib PRNG).
Wired into `WaveformGenerator::rand` (`waveform_generator.cpp:1548`),
replacing the prior libc++ MT64 shim call. `randomGauss` and
`randomUniform` continue to use the libc++ shim unchanged.

**Verification**:
- Standalone C++ verifier (`/tmp/verify_minstd.cpp`) reproduces
  first 12 samples of `rand(1024, 1.0, 0, 0.1)` byte-identical to
  original ELF.
- `hdawg_doc_randomSeed` produces two byte-identical waveforms
  (proves `randomSeed()` does not affect `rand`'s LCG seed).
- Full diff suite: **259/259 passing** (was 257/259).

**Lesson**: Whenever a "stdlib ABI mismatch" hypothesis is on the
table for a single failing function, look at what stdlib calls the
binary actually makes. If there are no `call` instructions to
distribution operators in the function, it's not a stdlib problem —
it's a hand-rolled inline implementation. Disassemble before assuming.

---

## IF-109  Bytes-vs-samples confusion across wave-memory subsystem

- **Source**: audit batches 02, 14, 16, 36, 46, 48
- **Severity**: likely-bug
- **Status**: **fixed** (Phase 44.1, 2026-05-05)
- **Description**: Multiple subsystems conflate byte counts with sample counts when computing wave memory sizes, offsets, and allocation. Parameters named `size` or `length` are sometimes in bytes (×2 for 16-bit samples) and sometimes in samples, with no consistent convention.
- **Fix**: Renamed all misnamed identifiers in Phase 44.1:
  - `MemoryAllocator::memorySizeInSamples_` → `memorySizeInBytes_` (holds `dc->waveformMemorySize`, a byte count)
  - `MemoryAllocator::cacheLineSize_` → `cacheLineSizeBytes_` (holds `dc->waveformAlignment` = 4096 bytes)
  - `Cache::allocate` / `getBestPosition` parameter `numSamples` → `numBytes` (receives `totalBits/8`)
  - `prefetch.cpp` local `numSamplesForCache` → `numBytesForCache`
  - `memory_allocator.cpp` line 184: `cacheLineSize_` → `cacheLineSizeBytes_` in code (not just comments)
  - All 1341 tests pass after renames.

---

## IF-110  `Value::pad_04_` is not padding; `subType_` shape unclear

- **Source**: audit batch 11
- **Severity**: suspicious
- **Status**: **dismissed** (Phase R, GDB-confirmed 2026-04-29) — `pad_04_` is genuine alignment padding
- **Description**: The field named `pad_04_` in `Value` was suspected of being accessed by binary code and therefore not mere padding. Additionally, `subType_` has a shape (enum? bitfield?) that doesn't match the current `int` declaration.
- **Phase R source review**: confirmed that `EvalResultValue::varSubType_` (a `VarSubType` enum at outer-struct +0x4) is the slot copied at `resources.cpp:1686` (`out.subType_ = var->subTypeRaw`) — that is **not** `Value::pad_04_` (which is at inner Value+0x04, embedded at EvalResultValue+0x08+0x04 = +0xC). The current recon treats `Value::pad_04_` as padding (`value.hpp:160`).
- **Phase R disasm sweep** (objdump on `_seqc_compiler.so`):
  - `Value::toDouble`@0x15a560..0x15a780 — every read of the Value object uses offsets `(%rdi)`, `0x8(%rdi)`, `0x10(%rdi)`. **No `0x4(%rdi)` reads.**
  - `Value::toInt`@0x15c250..0x15c4f0 — same: only `(%rdi)` (type_), `0x8(%rdi)` (which_), `0x10(%rdi)` (storage). **No `0x4` reads.**
  - `Value::operator==`@0x21a780..0x21aa00 — only `(%rdi)`, `(%rsi)`, `0x8(%rdi)`, `0x10(%rdi)`, `0x18(%rdi)`. **No `0x4` reads.**
  - `Value::toBool`@0x164200, `Value::toString`@0x15de50, `~Value`@0x15a9c0 — all checked, **no `0x4(%rdi)` reads.**
- **Phase R GDB trace** (`/tmp/if110_trace.txt`, 17 hits across `toDouble`/`toInt` driven by `var c = a + b; const PI = 3.14159;` style snippets through HDAWG8 path):
  - At every hit, `+0x0=3` (Double) and `+0x8=2` (which==Double) — consistent.
  - `+0x4` slot values observed: `0`, `21845` (0x5555), `32512` (0x7F00), `32767` (0x7FFF). The same `this` pointer (e.g. `0x7fffffffc3a0`) shows different `+0x4` values across hits (0, 0, 21845). These are classic uninitialized-memory bit patterns, not deterministic state.
  - If `+0x4` were a sub-type tag, values would be small and deterministic per `(type_, which_)` pair. They are not.
- **Conclusion**: `Value::pad_04_` is genuinely uninitialized alignment padding (4 bytes between `type_:int32` and the 8-byte-aligned `which_`). The recon name is correct; no source change needed. GDB evidence preserved at `/tmp/if110_trace.txt`.

---

## IF-111  `namespace Assembler` + `AssemblerInstr` should be one class

- **Source**: audit batches 26, 33 (type-suspicion + logic-bug: recon split of one binary class)
- **Severity**: suspicious
- **Status**: **fixed** (Phase D commit `5a44521`, Cluster M)
- **Description**: The binary implements a single `Assembler` class with instruction-building methods. The reconstruction artificially split this into a free-function namespace `Assembler` and a separate `AssemblerInstr` struct, causing awkward coupling and potential semantic drift.
- **Resolution**: Phase D commit `5a44521` merged `namespace Assembler` functions into a unified `class Assembler` (formerly `AssemblerInstr`). All call sites updated; tests 259/259 throughout.

---

## IF-112  `NodeMapItem::hasFast` int conflated with `AccessMode` enum

- **Source**: audit batches 27 (type-suspicion + logic-bug)
- **Severity**: cosmetic (downgraded from likely-bug)
- **Status**: **dismissed** (Phase R) — GDB-confirmed bool typing is correct
- **Description**: `NodeMapItem::hasFast` was suspected of carrying an `AccessMode` enum value (Soft=0/Direct=1/Custom=2) because the playback dispatch reads it as `static_cast<AccessMode>(node.hasFast)` (custom_functions_play.cpp:1511) while other call sites pass literal `static_cast<AccessMode>(2)` (Custom) to `addNodeAccess`.
- **Phase R GDB resolution** (commit per Phase R wrap-up):
  - Trace plan: break at `lookupNode` @ `0x15c582` (after typeIdx/fastAddr/hasFast stored into the sret buffer at `[rbx+0x08..0x10]`); print `*(unsigned char*)($rbx+0x10)` per hit.
  - Driver: `/tmp/if112_trace.py` compiles every entry from `tests/cases/manifest.json` (all device families, ~50 cases) through the original `_seqc_compiler.so`.
  - Result (`/tmp/if112_trace.txt`): **51 hits, distribution `0`:41, `1`:10, `2`:0**. Only `0` and `1` ever appear.
  - Static cross-check: every writer of `hasFast` lives in `get_node_map.cpp` (`addDirect`/`addVirt` helpers) and only ever passes literal `false` (default) or `true`. `Custom(2)` enters the system exclusively via the **other** argument of `addNodeAccess` (the `mode` parameter at custom_functions_io.cpp:89, :1543, :1580, :1680, :1687, :3120, :3225, :3325) — never via `hasFast`.
  - Decision: keep `bool hasFast` typing. The `AccessMode(hasFast)` cast at custom_functions_play.cpp:1511 is intentional — `false` → `Soft(0)` (no fast address ⇒ slow/soft access), `true` → `Direct(1)` (fast address ⇒ direct dispatch). It coincidentally produces the correct `AccessMode` value because the field is overloaded but never sees Custom(2).
- **Action taken**: no structural change. Field comment in `node_map_data.hpp:106-118` and inline comment at `custom_functions_play.cpp:1511` updated to document the dual-role / GDB result. Joint resolution with **Arb 5** (was: deferred type-fix int→AccessMode); Arb 5 is now closed as "rejected, bool is correct".

---

## IF-113  `Cache::Pointer::hash_` is not a hash; prefetch wrap-address semantics

- **Source**: audit batch 36
- **Severity**: suspicious
- **Status**: **kept** (Phase R) — formula is hash-like; consumer role documented
- **Description**: The field named `hash_` in `Cache::Pointer` does not store a hash value — binary analysis shows it holds a wrap-around address or index used by the prefetch subsystem. Misnaming obscures the actual cache-line logic.
- **Resolution**: Source review of all writers/readers:
  - Written only in `cache.cpp:132` for the Aligned cache type:
    `ptr->hash_ = ~(ptr->position_ ^ (ptr->position_ + halfSz));` — this is in fact a hash-like derivation of `position_` (XOR with a folded sum, then bitwise NOT).
  - Cleared in `cache.cpp:184` (`pointer->hash_ = 0`) on the Normal path.
  - Read at `prefetch_splitplay.cpp:325` and passed to `insertPlay` as the "start-address hash key" (per the comment at :321). The receiver uses it as an index/key, not as an address that can be dereferenced.
  - Read at `cached_parser.cpp:60,247` in a different `hash_` field (`CachedParser::hash_`, a `std::vector<uint>`) — unrelated.
  The value is genuinely a hash of the position; the prefetch consumer uses it as a hash key to resolve the wrap. Name kept; consumer comment at `prefetch_splitplay.cpp:321` documents the wrap role.

---

## IF-114  `PlayConfig::now` named 'now' but read as 4-channel flag

- **Source**: audit batch 38
- **Severity**: suspicious
- **Status**: **kept** (Phase R) — name preserved for binary-contract fidelity
- **Description**: `PlayConfig::now` is named as if it means "play immediately" but the binary reads it as a flag indicating 4-channel playback mode. Callers testing `now` may misinterpret the semantics.
- **Resolution**: The JSON serialization key in the binary is literally `"now"` (`play_config.cpp:127`: `pc.now = obj.at("now").as_bool();`). Renaming the field would diverge the JSON contract from the binary's serialization. Producer site `asm_commands.cpp:984` writes `config.now = playNow;` from a parameter named `playNow`, and consumer sites in `prefetch_*` use the value as `is4Ch` because in this hardware the "play-now" path coincides with 4-channel mode. The dual-meaning is documented at `prefetch_splitplay.cpp:39` and the `genPlayConfig::isFourChannelBool` audit (batch 10). Field name kept as `now`; semantic dual use is documented locally.

---

## IF-115  Strict/useAbsolute/showLine polarity-inverted booleans

- **Source**: audit batches 39, 47, 52
- **Severity**: likely-bug
- **Status**: **partially dismissed** (2026-05-05)
- **Severity**: likely-bug (was; remaining items cosmetic/dead)
- **Description**: Several boolean parameters had inverted polarity. Resolution per item:
  - `useAbsolute`: **fixed** by IF-117 (renamed to `useMapped`).
  - `MathCompiler::functionExists(..., bool strict)`: `return strict | found` matches binary exactly — the inversion is in the binary too; dead-path only (call site hardcodes `false`). No action needed.
  - `showLine`: not found in current source — either renamed in an earlier phase or belongs to a not-yet-reconstructed component. Cannot audit; dismissed pending future work.

---

## IF-116  `Expression::valueType` int slot is actually `EDirection` enum

- **Source**: audit batch 42
- **Severity**: suspicious
- **Status**: **fixed** (2026-04-29) — type-fix complete
- **Description**: `Expression::valueType` is declared as `int` but the binary only stores values 0/1/2 corresponding to `EDirection` (input/output/inout or similar). Using raw int allows invalid values and obscures intent.
- **Resolution**:
  - **Rename done**: the field is now named `direction` (not `valueType`) at `expression.hpp:125`.
  - **Type-fix applied** (2026-04-29): Changed `int32_t direction` → `EDirection direction` in `expression.hpp:125`. Updated all 5 assignment sites in `expression.cpp` (lines 181, 249, 412, 439, 574) to use `EDirection::eINOUT`. Updated all 13 assignment sites in `seqc_parser.tab.c` to use `EDirection::eIN`. Updated default constructor to use `EDirection::eINOUT`. Build clean, 259/259 tests pass.

---

## IF-117  `addWaveform::useAbsolute` polarity confirmed; flipped with rename

- **Source**: audit batch 47
- **Severity**: likely-bug
- **Status**: fixed (commit e22c1b5, renamed to `useMapped`)
- **Description**: The `useAbsolute` parameter had inverted polarity — `true` meant "use mapped addressing". Confirmed via binary trace and fixed by renaming to `useMapped` with correct polarity in phase-D commit 14.
- **Action**: None — resolved.

---

## IF-118  `AddressImpl<T>` wrapper overgeneral

- **Source**: audit batch 48
- **Severity**: suspicious
- **Status**: **kept** (Phase R) — instantiation set verified, de-templating not worth churn
- **Description**: `AddressImpl<T>` is templated but only instantiated with one type in the binary. The template adds complexity (SFINAE, specialization surface) without benefit and may mask the actual address-width semantics.
- **Resolution**: Source-only audit of all instantiations (~300 sites) shows `AddressImpl` is parameterized as `<unsigned int>` and `<uint32_t>` — these are the **same type** on the target ABI but appear textually distinct across files. Concrete instantiations: `appendSuser` (custom_functions_io.cpp, custom_functions_play.cpp, ~80 sites), `Cache` constructors and `getSize()` (cache.cpp/hpp), `WavetableFront::WavetableFront` (wavetable_front.cpp:52, wavetable_ir.cpp), `PlayConfig` member fields `channelMask/suppress/markerBits/trigger/precompFlags` (play_config.hpp:30-37), `Immediate` variant slot (struct_layouts.md:215-225). De-templating to a concrete `AddressImpl` would require touching every call site and would not change behavior since only one width (32-bit unsigned) is ever instantiated. Kept as-is; the template form does not currently hide misnamed semantics.

---

## IF-119  `setPRNGSeed` integer-literal path constructs `AsmRegister` from value

- **Source**: audit batch 05c2
- **Severity**: likely-bug
- **Status**: **fixed** (Phase R)
- **Description**: When `setPRNGSeed` receives an integer literal, the reconstruction passes the raw integer value to `AsmRegister` constructor, treating the seed value as a register number. The binary instead emits an immediate-load instruction.
- **Phase R source audit**: at `custom_functions_io.cpp:2773` the integer-literal branch (`argType == 2`, comment "Integer literal path") constructs `AsmRegister(args[0].value_.toInt())` from the value rather than using `args[0].reg_`. In every sibling method (e.g. `setSweepStep:3161`), `argType == 2` is the **register** branch and code uses `args[0].reg_`. Either the comment is wrong (this is the register branch and the value-toInt path is a logic bug) OR `varType_==2` is overloaded only here.
- **Resolution (disasm, no GDB needed)**: `objdump -d` on `_seqc_compiler.so` at @0x1513e0 shows the `argType == 2` branch (= `VarType_Var`, variable-bound) is in fact the **register** branch:
  - @0x151507: `mov rdx, QWORD PTR [rbx+0x30]` loads `args[0].reg_` (the 8-byte `AsmRegister` field at +0x30) into `rdx`.
  - @0x15150f–0x151512: `cmp eax, 0x2 / jne 1515a6` — `rdx` is preserved across the branch.
  - @0x151518–0x151528: sets `rdi/rsi/ecx` (vec, asmCommands, addr=`kSuserPrngSeed`=0x74) and calls `AsmCommands::suser(AsmRegister, AddressImpl<uint>)` with `rdx` still holding `args[0].reg_`.
  - The integer-literal handling actually lives in the `(argType & ~2) == 4` branch (matches `VarType_Const`/`VarType_Cvar`), which already allocates a register and emits `addi` to load the seed immediate — that branch was already correct.
  - Sibling-method evidence (e.g. `setSweepStep`, `wvft@2734-2738`) confirms `argType==2` is the register/Var branch everywhere.
- **Fix**: replaced `AsmRegister(args[0].value_.toInt())` with `args[0].reg_`, and rewrote the misleading "Integer literal path" comment to "Variable path (VarType_Var)". `custom_functions_io.cpp:2771-2778`. Tests 259/259 (no test currently exercises `setPRNGSeed(varName)` so the prior bug had no observable effect on the suite, but the source now matches the binary).
- **Trace logs**: `/tmp/if119_var_trace.txt`, `/tmp/if119_literal_trace.txt`.

---

## IF-120  `configFreqSweep` magic literals — constants exist but are unused

- **Source**: audit batch 05c
- **Severity**: suspicious
- **Status**: **fixed** (Phase R)
- **Description**: Named constants for `configFreqSweep` magic values exist in the reconstruction (from rodata) but the implementation uses inline numeric literals instead. This makes maintenance harder and risks drift.
- **Resolution**: Replaced all 4 remaining raw `0x8c`/`0x8d`/`0x8e`/`0x8f` SUSER addresses in `custom_functions_io.cpp::configFreqSweep` and the per-step variant with `kSuserSweepOscIdx` / `kSuserSweepControl` / `kSuserSweepStartLo` / `kSuserSweepStartHi` from `types.hpp`. (Two earlier sites in the function had already been migrated.) Tests 259/259.

---

## IF-121  `DeviceOpts` namespace full duplicate of anonymous-namespace `k*` set

- **Source**: audit batch 22
- **Severity**: suspicious
- **Status**: **fixed** (Phase R, removed dead namespace from `device_factories.hpp`)
- **Description**: `DeviceOpts` namespace defines the same constant set as the anonymous-namespace `k*` constants (e.g. `kMaxWaveLen`, `kMinGranularity`). One is redundant and may diverge silently.
- **Resolution**: Verified `DeviceOpts::*` had zero references in source. The live set lives in `device_factories.cpp` (`kSubtypeMask`, `kSubtype1..4`). Dead `DeviceOpts` namespace removed; tests 259/259.

---

## IF-123  `SeqcParserContext::errorCallback_` never wired in recon `Compiler` constructor

- **Source**: hdawg_func_return_int difftest investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The original binary's `Compiler::Compiler` (at ctor+0x2cb) calls
  `parserContext_.setErrorCallback(lambda)` where the lambda body is
  `messages_.parserMessage(lineNr, msg)`. The recon constructor never set this
  callback, leaving `SeqcParserContext::raiseError` a no-op — syntax errors from
  the bison parser were silently discarded (no error message in `messages_`), so
  `messages_.hadCompilerError()` returned false and the compiler continued past
  a parse failure.
- **Resolution**: Added `parserContext_.setErrorCallback(...)` call to
  `Compiler::Compiler` in `reconstructed/src/codegen/compiler.cpp`. GDB-confirmed: the
  lambda vtable at binary offset `0x9e62ac+rip` at ctor+685 disassembles to
  `mov 0x8(%rdi),%rdi; mov (%rsi),%esi; add $0x38,%rdi; jmp parserMessage`
  (captures `this`; `0x38` = `messages_` offset).

## IF-124  `Compiler::compile` null-seqcAst crash (libstdc++ vs libc++ ABI)

- **Source**: hdawg_func_return_int difftest investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: When the SeqC parser fails (seqcAst is null), the recon
  calls `FrontEndLoweringFacade::lower(..., *seqcAst, ...)`, triggering
  libstdc++ `__shared_ptr_deref` assertion (SIGABRT). The original binary
  uses libc++ which has different null-deref behavior for shared_ptrs.
  GDB-confirmed: binary at `Compiler::compile+1644` has `test %rbx,%rbx;
  je +1671` — a null guard on the raw seqcAst pointer; if null, jumps past
  refcount and into `lower()` where libc++'s ABI tolerates the null.
- **Resolution**: Wrapped steps 7–8 (SeqC AST print + frontend lowering) in
  `if (seqcAst)` guard in `compiler.cpp::Compiler::compile`. With IF-123 also
  fixed, the parse error is now properly stored in `messages_` so step 9's
  `hadCompilerError()` check throws the right exception.

## IF-125  `SeqCBreakStatement::evaluate` had spurious `inSwitch_` guard

- **Source**: hdawg_switch_basic / hdawg_switch_fallthrough difftest investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The recon's `SeqCBreakStatement::evaluate` had an
  `if (!state.inSwitch_)` guard that skipped the error when inside a switch.
  The original binary at `0x226970` has no such branch: the first instructions
  are `ErrorMessages::format(0xd5, "break")` + `errorMessage(...)` unconditionally,
  followed by allocating and returning an empty `EvalResults`.
  GDB-confirmed: traced with a switch-body input; `inSwitch_=0x61` (true) but
  the binary still calls `errorMessage` and returns empty EvalResults.
- **Resolution**: Removed the `if (!state.inSwitch_)` guard. The function now
  unconditionally emits error 0xd5 and returns empty `EvalResults`. This fixed
  `hdawg_switch_basic` (which had break outside switch — was being silently
  ignored). The `hdawg_switch_fallthrough` inline case (break inside switch)
  remains failing because the recon is missing the `playZero(16)` warning —
  a separate pre-existing switch evaluation order issue.

## IF-126  `seqc_parser.tab.c` missing verbose error output; `EDirection` cast errors in grammar

- **Source**: `hdawg_func_return_int` / `hdawg_func_nested_call` investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The pre-existing `seqc_parser.tab.c` was generated without `%define parse.error verbose`, so `yyerror` / `seqc_error` received only the bare string `"syntax error"` instead of the detailed `"syntax error, unexpected IDENTIFIER, expecting ';'"` that the original binary produces. Separately, the grammar file used integer literals `0` for `direction` field assignments instead of `EDirection::eIN`, which caused compile errors when regenerating with a stricter bison version.
- **Resolution**: Added `%define parse.error verbose` to `seqc_parser.y`, fixed all `->direction = 0` assignments to `EDirection::eIN`, regenerated `seqc_parser.tab.c` and `seqc_parser.tab.h`. The bison-generated verbose error strings now match the original binary output exactly. Tests `hdawg_func_return_int` and `hdawg_func_nested_call` pass; full suite 326/327.

## IF-127  `evalCases` fallthrough: recon grammar nests `case 0: case 1:` but binary has flat list

- **Source**: `hdawg_switch_fallthrough` (inline code variant) investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The recon grammar resolves `case 0: case 1: stmt;` with a shift (Rule 94), producing a nested AST where `case 1: stmt` is the body of `case 0:`. The binary grammar resolves with a reduce (Rule 96), producing a flat list: `case 0` (null body), `case 1` (stmt body), `break` (bare sibling). Consequence: in the recon's `evalCases`, `case 1`'s body (`playZero(16)`) was never evaluated (it was inside `case 0`'s body as a case label only), so the playZero warning was not emitted before the break error. GDB-confirmed: original binary has `case 0` with `body()=nullptr` (pointer at offset +0x20 is 0x0).
- **Root cause**: Shift/reduce conflict in the grammar; recon bison version (3.8.2, 232 states) resolves differently than the original binary's bison version (233 states).
- **Resolution**: Fixed in the grammar: changed Rules 94 and 95 (`KW_CASE expr ':' statement` and `KW_DEFAULT ':' statement`) to use `unlabeled_statement` instead of `statement`. This eliminates the shift/reduce conflict for `KW_CASE`/`KW_DEFAULT` tokens, forcing the parser to reduce with Rule 96/97 (no body) when another case label follows. The recon now produces a flat AST identical to the binary — GDB-confirmed: `case 0` body pointer at +0x20 is 0x0 in both. Parser tables regenerated. All 327 tests pass.

## IF-122  `Resources::parent_` strong/weak pointer inversion

- **Source**: audit batch 19a
- **Severity**: likely-bug
- **Status**: **fixed** (Phase D commit `612eb2a`, Cluster N)
- **Description**: `Resources::parent_` is stored as a `shared_ptr` (strong reference) but the binary uses a `weak_ptr` to avoid reference cycles in the resource tree. The strong reference prevents parent deallocation and causes memory leaks in deep scope chains.
- **Resolution**: Phase D commit `612eb2a` swapped the strong/weak slots and renamed the strong slot to `grandparent_` (the binary's actual semantic — the recon's `parent_` was at +0x18, the true direct-parent weak slot at +0x28; what `parent_` referred to was a transitively-owned grandparent). All access sites updated; tests 259/259 throughout.

## IF-128  `Value::toInt()` missing uint32_t overflow retry for large hex literals

- **Source**: `ziai_analyze_setDIO` difftest investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The `Value::toInt()` Double case used `static_cast<int32_t>(floor(d))` unconditionally. For hex literals like `0xAAAAAAAA` (2863311530.0 as double), this is undefined behavior — the value exceeds INT32_MAX. The binary's `toInt()` at 0x15c250 uses the x86 `cvttsd2si` instruction which returns the "indefinite" sentinel 0x80000000 on overflow, then detects the overflow and retries via `uint32_t` truncation. The recon omitted this retry, yielding wrong values (UB in practice produced 0x80000000 = -2147483648, which then split incorrectly by `addi32` into addi imm=0x0 and addiu imm=0x80000 instead of the correct 0xAAA/0xAAAAA). The symptom was `setDIO(0xAAAAAAAA)` emitting wrong machine code: `addi R1,R0,0` + `addiu R1,R1,524288` instead of `addi R1,R0,2730` + `addiu R1,R1,699050`.
- **Root cause**: Missing overflow path in `Value::toInt()` for the Double case. The comment at the top of the function noted "On int overflow, catches and retries as uint32_t" but the implementation lacked this logic.
- **Resolution**: Added range check in the Double case; values outside [INT32_MIN, INT32_MAX] are cast via `static_cast<int32_t>(static_cast<uint32_t>(truncated))`, which wraps correctly and matches the binary behavior. All 13 `ziai_analyze_setDIO_*` tests now pass byte-identical. No regressions (776 pass, 3 equiv, 12 failed — same as before).

## IF-129  `resetOscPhase`: UHFQA uses direct pulse-reset, not `oscs/phasereset` node write

- **Source**: `ziai_analyze_resetOscPhase_uhfqa` difftest failure
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The recon's `resetOscPhase()` had a single `else if (devType >= 2 && devType <= 0x20)` branch covering all "Hirzel" devices (HDAWG, UHFQA). For HDAWG, the correct behaviour is to call `writeToNode("oscs/phasereset", oscMask)`. But for UHFQA, the original binary uses a **completely different code path** — GDB-confirmed via jump table at `0x14043e` in the binary: UHFQA (devType=4, index=2) jumps to `0x1405ba`, not to the HDAWG path at `0x14044e`. The UHFQA path emits a pulse-reset sequence directly: `addi R_n, R0, 1; st R_n, 0x5f; st R0, 0x5f`. This writes 1 then 0 to hardware register address `0x5f` (phasereset). No node write is performed. The UHFQA node map does not contain `"oscs/phasereset"`, so the recon's `lookupNode("oscs/phasereset")` call threw a compilation error.
- **Root cause**: The recon incorrectly grouped UHFQA with the HDAWG (Hirzel) path in a single `else if` branch, missing that the binary uses per-device jump-table dispatch and UHFQA has its own distinct path.
- **Resolution**: Added a UHFQA-specific branch before the generic Hirzel branch in `custom_functions_io.cpp::resetOscPhase()` (`reconstructed/src/runtime/custom_functions_io.cpp:1419`). UHFQA now emits the direct `addi/st/st` pulse-reset sequence to register `0x5f`. GDB-verified: binary offsets `0x140858` and `0x14095c` both execute `st(..., 0x5f)` with reg=1 then reg=0. All 11 `resetOscPhase` tests now pass byte-identical (788 total passing, 0 failed).

## IF-130  `incrementSinePhase` Phase 3 path uses `awgIndex` instead of `oscIndex`

- **Source**: `ziai_analyze_incrementSinePhase_hdawg8/hdawg4` difftest failure
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `CustomFunctions::incrementSinePhase()` in `custom_functions_io.cpp` is structured as four separate `if (devType == ...)` phases. Phase 1 (devType==2, HDAWG) reads `oscIndex = args[0].value_.toInt()` and uses it for suser codegen. Phase 3 (also devType==2) builds the node path for `addNodeAccess`. However, Phase 3 incorrectly used `config_->awgIndex` (the AWG channel index, always 0 for index=0 tests) instead of `oscIndex` (the sine generator argument from the user). For `incrementSinePhase(1, 45.0)`, this produced `nodes_json: {"nodes":[{"name":"SINEPHASE","index":[0],...}]}` instead of the correct `index:[1]`. The `setSinePhase` counterpart correctly uses `oscIndex` in its single combined `if (devType==2)` block (both suser and node path in the same scope). The bug arose because `incrementSinePhase` split the two operations across separate `if` blocks, placing `oscIndex` out of scope for Phase 3.
- **Root cause**: Scoping error — `oscIndex` is declared inside Phase 1's `if` block but needed again in Phase 3's separate `if` block. The fix re-reads `oscIndex` from `args[0].value_.toInt()` in Phase 3.
- **Resolution**: Changed line 1680 in `custom_functions_io.cpp`: added `int oscIndex = args[0].value_.toInt();` and replaced `config_->awgIndex` with `oscIndex` in the path string. Confirmed by running the original binary directly (outputs `index:[1]` for `incrementSinePhase(1, 45.0)`). All 10 `incrementSinePhase` tests now pass byte-identical. Full suite: 788 passed, 3 equiv, 0 failed.

## IF-131  `alui` Case 3 (large-immediate ORI/ANDI/XNORI): `alur` dst/src arguments reversed

- **Source**: `ziasm_register_arithmetic_11` difftest failure (all 13 devices)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `AsmCommands::alui()` in `asm_commands.cpp` handles the case where a bitwise OR/AND/XNOR immediate is too large for a single immediate instruction. It loads the constant into `dst` via `ADDI` + optional `ADDIU`, then performs a register-register operation. The final `alur` call at line 384 was `alur(regCmd, src, dst)` — but this encodes `dst=src, src=dst`, which swaps the register fields in the instruction word. The original binary emits `orr R2, R1` (destination R2 holds the loaded constant, source R1 is the variable), while the recon emitted `orr R1, R2`.
- **Root cause**: Simple argument-order inversion in the `alur(regCmd, src, dst)` call. The correct order is `alur(regCmd, dst, src)` since `dst` is where the constant was loaded and the result should accumulate.
- **Resolution**: Changed `asm_commands.cpp:384` from `alur(regCmd, src, dst)` to `alur(regCmd, dst, src)`. All 13 `ziasm_register_arithmetic_11` tests now pass byte-identical. No regressions in full suite.

## IF-132  `waitAnaTrigger`: reg2 loaded with trigger address instead of wait-flag argument

- **Source**: `ziasm_triggers_3_uhfli/uhfawg/uhfqa` difftest failure (`.asm` and `.text` differ; wrong immediate in `addi R2`)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `CustomFunctions::waitAnaTrigger()` emits two `addi` instructions: reg1 gets the trigger address (from `readConst("AWG_ANA_TRIGGER1/2")`), reg2 gets the wait condition (args[1], 0 or 1). The recon incorrectly loaded `trigVal` (the trigger address) into reg2 instead of `args[1].value_.toInt()` (the wait flag). For `waitAnaTrigger(2, 0)`, this produced `addi R2, R0, 2` instead of `addi R2, R0, 0`. A stale comment ("Phase S.2 M5") incorrectly justified reusing `trigVal` for reg2.
- **Root cause**: Wrong value used for the second `addi` — `trigVal` instead of `args[1].value_.toInt()`.
- **Resolution**: Changed `custom_functions_io.cpp` in `waitAnaTrigger()` to compute `int waitFlag = args[1].value_.toInt()` and use it for the second `addi`. All 3 `ziasm_triggers_3` tests now pass byte-identical.

## IF-133  `waitDigTrigger` unsupported-device path: false-wait branch emits extra `addi` + wrong `wtrig` form

- **Source**: `ziasm_triggers_4_uhfli/uhfawg/uhfqa` difftest failure (`.asm` size 308 vs 284, extra instruction)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `CustomFunctions::waitDigTrigger()` unsupported-device path checks `args[1].toBool()` (the wait flag). When wait=true (`useSameReg`), it emits `wtrig(reg1, reg1)`. When wait=false, the original binary emits `wtrig(reg1, R0)` — using R0 (which is always 0 in this ISA) directly without allocating a new register. The recon instead allocated a fresh register, emitted an extra `addi R2, R0, trigVal`, then `wtrig(reg1, R2)` — adding one spurious instruction and encoding the wrong value in the second operand.
- **Root cause**: Incorrect assumption that the false-wait branch mirrors the true-wait structure with a new register. The binary reuses R0 as a constant-zero register.
- **Resolution**: Changed the `useSameReg=false` branch in `custom_functions_io.cpp::waitDigTrigger()` to `wtrig(reg1, AsmRegister(0))`. All 3 `ziasm_triggers_4` tests now pass byte-identical. No regressions in full suite (1207 passed, 3 equiv, 49 failed).

## IF-135  `WaveformGenerator::merge` reserveOnly path: `channels_` not set to input count

- **Source**: `ziasm_prefetching_0` difftest — all `.wf___playWave_*` sections half the expected byte size; `channels=1` in `.waveforms` for dual-channel assignWaveIndex waveforms
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `WaveformGenerator::merge()` (0x25f5c0) has two paths: one for normal (non-reserveOnly) signals and one for `reserveOnly_` signals (placeholder waves). In the reserveOnly path (0x25fa98), the function constructs and returns a `Signal(ReserveOnly, length, mergedBits)`. The `ReserveOnly` Signal constructor initializes `channels_=0`. In the non-reserveOnly path (0x25fc70), `result.channels_ = numChannels` is explicitly set. The reserveOnly path was missing this assignment.
- **Effect**: When `assignWaveIndex(1, 2, w, 1, 2, w, idx)` is called with a dual-channel placeholder wave, `mergeWaveforms` calls `merge()` on two entries of the same placeholder. The result had `channels_=0` instead of `channels_=2`. This caused: (a) `Signal::getRawData()` to compute `byteSize_ = 0 * length * 2 = 0` for NOBITS ELF sections (all `.wf_` sections half the expected size, since `channels_=1` after clamping by some path), (b) `genPlayConfig()` to see `channels_=0→1` and compute `channelMask=1` instead of `3`, setting wrong `play_config` in `.waveforms` metadata, (c) wrong waveform memory accounting in `.wavemem`.
- **Root cause**: Missing `result.channels_ = static_cast<uint16_t>(signals.size())` after the `Signal(ReserveOnly, ...)` construction in `merge()`.
- **Resolution**: Added `result.channels_ = static_cast<uint16_t>(signals.size())` before the return in the reserveOnly branch of `merge()`. Fixes 24 tests (+1190 → +1214 passing in 1259-test suite). `ziasm_prefetching_0` still has additional failures in the prefetch address calculation and waveform ordering (separate pre-existing bugs).
- **Location**: `reconstructed/src/waveform/waveform_generator.cpp:2627-2635`

## IF-134  `writeWavesToElfAbsolute`: wrong skip condition for placeholder waveforms (Cervino/UHF devices)

- **Source**: `ziasm_playconfig_cwfv_0_uhfli/uhfawg/uhfqa` difftest failures (`.wf_` sections missing in recon)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: In `writeWavesToElfAbsolute` (used for absolute-addressing devices: UHFLI, UHFAWG, UHFQA), the recon checked `wf->signal.data().empty()` (i.e., `samples_.empty()`) to skip waveforms with no data. However, the binary at `0x10e1aa` checks `signal.length_ == 0` (the uint64 at WaveformIR+0x80+0x50 = offset 0xD0 from WaveformIR base). `placeholder` waveforms have `reserveOnly_=true` so `samples_` is always empty, but `length_ > 0` (e.g., 1000). The incorrect empty-samples check caused all placeholder waveforms to be skipped in the absolute-addressing path, producing ELFs with missing `.wf_` sections.
- **Root cause**: `Signal::data()` returns `samples_` (physical samples), which is empty for reserveOnly signals. The binary instead checks the logical `length_` field, which is set even for placeholder (reserve-only) waves.
- **Resolution**: Changed condition to `wf->signal.length() == 0`. All 3 UHFLI/UHFAWG/UHFQA tests for `ziasm_playconfig_cwfv_0` now pass byte-identical.

## IF-136  `playAuxWave`/`playDIOWave`/`playWaveIndexed`: missing prefetch instructions and wrong waveform emit on Cervino (UHF) devices

- **Source**: `ziasm_playconfig_cwfv_2_uhfli/uhfawg/uhfqa`, `ziasm_playconfig_cwfv_3_uhfli/uhfawg/uhfqa`, `ziasm_various_playback_stuff_1_uhfli/uhfawg/uhfqa`, `ziasm_various_playback_stuff_2_uhfli/uhfawg/uhfqa` difftest failures (`.asm` size diff, `.wf_` section missing in recon)
- **Severity**: likely-bug
- **Status**: **stale/unverifiable** (2026-05-05 — test cases `cwfv_2`, `cwfv_3`, `vps_1/vps_2` not in current manifest; cannot confirm or dismiss; 1341/1341 passing)
- **Description**: After fixing IF-134 (the placeholder waveform data issue), several tests still fail:
  - `cwfv_2` (`playAuxWave`): The AuxWave generates a new waveform `__playWaveI_4_3` (4000 samples) which appears in original `.waveforms` but not recon. Original emits `prf`/`wprf`/`wvf` for it; recon emits only `cwvf` without the corresponding `wvf`. The AuxWave waveform is not reaching `collectUsedWaves` (likely because `playAuxWave` doesn't set `results->node_` to chain the node into the prefetch tree, OR the waveform is not getting `used=true`).
  - `cwfv_3` (`playDIOWave`): `.asm` size diff, `.channels` diff, `.waveforms` diff. `playDIOWave` also appears to not properly chain its node into results->node_ (line 634 of custom_functions_playback.cpp doesn't set `results->node_`). Additionally `.channels` byte differs (orig=0xe0 vs recon=0xc0) suggesting a channel mask calculation difference.
  - `vps_1/vps_2` (`playWaveIndexed`): Recon generates MORE instructions than original (recon=381 vs orig=350). Register assignment and instruction ordering differ — the original uses `addi R1, R0, 200; addi R3, R1, 0; ssl R3, R3; addr R3, R2; wvf R3, R0, 256; wwvf` while recon generates a different sequence with an extra `wwvf` and different register usage.
- **Root cause**: Not yet fully investigated. Likely multiple separate bugs in `playAuxWave`/`playDIOWave` node chaining and `playIndexed` register allocation.
- **Location**: `reconstructed/src/runtime/custom_functions_playback.cpp:352`, `634`, and `reconstructed/src/runtime/custom_functions_play.cpp` (playIndexed section)

## IF-137  `lock()`/`unlock()` missing `results->node_` assignment

- **Source**: `ziasm_misc_2` difftest failures (`.linenr` off-by-1 in `wavetableFrontIndex_`)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `lock()` and `unlock()` in `custom_functions_io.cpp` returned `EvalResults` without setting `results->node_` to the LockPlaceholder/UnlockPlaceholder node. Without this, the node chain only contained the Play node; `placeCommands` called `placeSingleCommand` once (wavetableFront=3). After all prefetch processing, `wavetableFrontIndex_` was left at 3 (not 4). Then `unsyncCervino()` (compiler Step 16) emitted `st R0, 68` / `st R0, 69` with stale index=3 vs 4. The `.linenr` section reflected this wrong value.
- **Root cause**: Missing `results->node_ = asmEntry.node` assignment in both `lock()` (binary: 0x14de27-0x14de47) and `unlock()` (0x14e33d-0x14e35c).
- **Resolution**: Added `results->node_ = asmEntry.node` in both functions. All `ziasm_misc_2` variants pass.
- **Location**: `reconstructed/src/runtime/custom_functions_io.cpp` (lock and unlock functions)

## IF-138  `getSampleClock()` wrong string literal for `DEVICE_SAMPLE_RATE` lookup

- **Source**: `ziasm_firmware_syscall_trap_1_hdawg8/hdawg4` difftest failures (missing `.required_sample_rate` ELF section)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `getSampleClock()` called `resources_->variableExists("$DEVICE_SAMPLE_RATE")` and `resources_->readConst("$DEVICE_SAMPLE_RATE", ...)` with a `$`-prefixed string literal. The binary's `constDeviceSampleRateE` global at 0x959dc0 is a libc++ SSO string where byte 0 = `$` (0x24) encodes length=18 and is-short. The actual string DATA starting at byte 1 is `"DEVICE_SAMPLE_RATE"` (18 chars, no `$`). So the binary's `variableExists` call matches variables stored as `"DEVICE_SAMPLE_RATE"`, not `"$DEVICE_SAMPLE_RATE"`. The recon's `"$DEVICE_SAMPLE_RATE"` literal would construct a 19-char C++ string with `$` as the first character, causing a miss and `usedSampleRate_` in `StaticResources::getVariable` never being set. Without the flag, `awg_compiler.cpp` does not emit the `.required_sample_rate` ELF section.
- **Root cause**: Misreading the binary's SSO string layout: `$` in the rodata is the SSO length byte, not the first character of the string.
- **Confirmed by**: GDB trace showing `StaticResources::getVariable` called with len=18, data=`"DEVICE_SAMPLE_RATE"` (no `$`).
- **Resolution**: Changed `"$DEVICE_SAMPLE_RATE"` to `"DEVICE_SAMPLE_RATE"` in `getSampleClock()`. All `ziasm_firmware_syscall_trap_1_hdawg8/hdawg4` tests pass.
- **Location**: `reconstructed/src/runtime/custom_functions.cpp:537-547`

## IF-139  `writeToNode` slow-commit hardcodes scale=1.0 instead of using `type` argument

- **Source**: `ziasm_firmware_syscall_trap_3` difftest failures (`.asm` and `.text` size diff — recon missing scale-dependent commit value)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: In `writeToNode()`, the common tail for BC cases 0,1,5 and the case 2 slow-arm both emit a "slow-commit" value via `addi(destReg, R0, float32_bits) + suser(kSuserNodeSlowCommit=0x14) + trap`. The recon hardcoded `Immediate(0x3F800)` = `float32(1.0) >> 12` in both `addiu` calls. The binary at 0x16643b-16648e instead loads `type.value_.toDouble()`, converts to `float32`, and calls `addi(float32_bits)` — which generates a hi/lo split (two instructions) when the lower 12 bits are nonzero. When `setDouble(path, val, scale)` is called with scale ≠ 1.0, the wrong hardcoded value was emitted.
- **Encoding**: The sequencer stores a 32-bit float as: `addi R2, R0, (float32_bits & 0xFFF)` (lower 12 bits) + `addiu R2, R2, (float32_bits >> 12)` (upper 20 bits). When lower 12 bits = 0 (e.g., float32(1.0) = 0x3F800000), only `addiu R2, R0, 0x3F800` is needed.
- **Resolution**: Replaced `addiu(Immediate(0x3F800))` with `addi(Immediate(float32(type.value_.toDouble()) bits))` in both the BC common tail and the case 2 slow-arm.
- **Location**: `reconstructed/src/runtime/custom_functions_play.cpp` (writeToNode BC common tail and case 2 slow-arm)

## IF-140  `writeToNode` case 2 fast-arm generates two triplets instead of one + slow-commit

- **Source**: `ziasm_firmware_syscall_trap_4` difftest failures (recon generates 1 extra instruction, no trap)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: In `writeToNode()` case 2 (typeIdx=2, I+Q sine write), the fast-arm (varType==2, register) incorrectly generated TWO triplets: tag=0xc (I-channel) + tag=0xd (Q-channel), with no commit or trap. The binary at 0x165587-165747 generates only ONE triplet (tag=0xc, addr, val.reg_) then `jmp 0x16636b` to the slow-commit region, which emits: floatEqual warning on type.value_, `addi(float32(scale))`, suser(kSuserNodeSlowCommit=0x14), trap.
- **Root cause**: The recon duplicated the Q-channel triplet (tag=0xd) into the fast-arm, imitating the slow-arm structure, instead of mirroring the binary's single-triplet + shared commit pattern. Additionally, the slow-commit floatEqual warning passes 3 args including `"integer"` hint (at 0x1663bc: `lea 0x79a579(%rip), %rcx` = `"integer"`), not 2 args.
- **Confirmed by**: GDB trace showing `type.value_` (vartype=4=Const, value_which=2=double) is what the slow-commit reads at 0x16636b.
- **Resolution**: Replaced the two-triplet fast-arm with: one triplet (tag=0xc) + floatEqual warning on `type.value_` (with `"integer"` hint) + `addi(float32(scale))` + suser(0x14) + trap.
- **Location**: `reconstructed/src/runtime/custom_functions_play.cpp` (writeToNode case 2 fast-arm)

## IF-140b  `playDIOWave` missing `results->node_` assignment

(ID disambiguation 2026-05-08: this entry shared the IF-140 number with
the earlier `writeToNode` entry above; renamed to IF-140b for clarity.
Both are fixed.)

- **Source**: `ziasm_playconfig_cwfv_3` difftest (UHFLI/UHFAWG/UHFQA)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `playDIOWave` in the recon did not set `results->node_` after calling `asmPlay` and pushing the asm entry. As a result, the DIOWave play node was absent from the `ast_` tree. `Prefetch::prepareTree` never visited it, `linkLoad` was never called, and `loadRef` was never set. When `placeSingleCommand` processed the play node, it found `loadRef=nil && dummy=false` and fell through to `play_finalize` with an empty `tempList`, emitting no wvf instruction.
- **Confirmed by**: GDB traces showing (1) `linkLoad` called 3 times in original (two dummy=0, one dummy=1), but only twice in recon (one dummy=0, one dummy=1); (2) `assignLoad` never called directly for DIOWave node; (3) `mov %rcx, 0x38(%rax)` at binary offset `+1840` writes `results->node_` from the play node shared_ptr; (4) `placeSingleCommand` dummy-check hit only once in original (for playZero), never for DIOWave.
- **Root cause**: The reconstruction previously read the binary's `playDIOWave` epilogue as not writing `results->node_` (offset `+0x38`). A broader disassembly search found the write at `+1840` — after an inlined push_back loop at `+1680`–`+1750`. Our earlier disassembly scan only looked in `+2014`–`+2251`.
- **Resolution**: Added `results->node_ = results->assemblers_.back().node;` immediately after the `push_back` in `playDIOWave` (`custom_functions_playback.cpp` line ~629). All 3 test variants now pass byte-identical. No regressions across all test subsets.

## IF-141  `WaveformGenerator::merge` checked only `signals[0].reserveOnly_` instead of all signals

- **Source**: `ziasm_playconfig_cwfv_2` difftest (UHFLI/UHFAWG/UHFQA) — `.wf___playWaveI_4_3` missing from ELF
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The binary at `0x25fa80` accumulates an `allReserveOnly` flag by ANDing each signal's `reserveOnly_` bit in the load loop (`movzbl -0x50(%rbp),%eax; and 0xca(%rbx),%al`). The reconstruction only checked `signals[0].reserveOnly_`. When `playAuxWave(w, 10)` was called with a placeholder (`reserveOnly_=true`) and zeros (`reserveOnly_=false`), the recon incorrectly treated all signals as reserveOnly, returning an empty `Signal` instead of merging them. The resulting waveform had `length_=0` and was silently skipped by the ELF writer (`signal.length()==0` guard in `awg_compiler.cpp`).
- **Confirmed by**: Debug prints tracing `signal.length()` at the ELF write site; GDB disassembly of the AND-accumulation loop in `merge`.
- **Root cause**: Reconstruction mistakenly generalized the loop as checking only the first element.
- **Resolution**: Changed `merge` in `waveform_generator.cpp` to accumulate `allReserveOnly = allReserveOnly && sig.reserveOnly_` over all signals before the early-return check.

## IF-142  `WaveformGenerator::interleave` missing `length_` assignment after markerBits resize

- **Source**: `ziasm_playconfig_cwfv_2` difftest — waveform present but wrong size
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: Binary at `0x25823d` executes `mov %rax, 0x50(%rbx)` which sets `result.length_ = result.samples_.size()` AFTER the markerBits resize block. An earlier reconstruction note placed this assignment at `0x258159` (which actually sets `channels_=1`), and it was subsequently removed. Without this assignment, `result.length_` remained 0 even though `samples_` was correctly populated, causing the waveform to be skipped by the ELF writer.
- **Confirmed by**: Debug prints showing `signal.length()==0` at ELF write site even when `samples_.size()==4000`; disassembly confirming `mov %rax, 0x50(%rbx)` at `0x25823d` (after `0x258220`–`0x25823c` markerBits block).
- **Root cause**: Misidentification of which instruction at `0x258159` sets `length_` vs `channels_`.
- **Resolution**: Restored `result.length_ = result.samples_.size()` at the correct location (after markerBits resize) in `interleave` in `waveform_generator.cpp`. All 3 `cwfv_2` tests now pass byte-identical.

## IF-143  `playWaveIndexed` (split_ devices): four bugs causing wrong prefetch and emit

- **Source**: `ziasm_various_playback_stuff_1/2` difftests — UHFLI/UHFAWG/UHFQA
- **Severity**: likely-bug (all fixed)
- **Status**: **fixed**
- **Description**: Four distinct reconstruction errors caused the indexed-play path for Hirzel split devices (UHFLI/UHFAWG/UHFQA) to emit wrong instructions:

  **Bug A** (`custom_functions_play.cpp:1092`): `Immediate rateImm(rate)` used `rate=-1` (the `parseOptionalRate` sentinel for "no rate arg") instead of the actual offset constant `parseEnd[0].value_.toInt()`. This caused `addi R1, R0, -1` instead of `addi R1, R0, 200`. GDB-confirmed: `addi` call args at `0x161e56` with correct immediate=200 in original.

  **Bug B** (`prefetch.cpp`, `moveLoadsToFront`): an extra `if (!loadNode->lengthReg.isValid() && cur->lengthReg.isValid()) loadNode->lengthReg = cur->lengthReg;` block propagated `lengthReg` from a matched Play node to the new Load node. GDB-confirmed: at `placeSingleCommand` step=4 for Load nodes on split devices, `lengthReg.valid=0` — the original binary does NOT propagate it here.

  **Bug C** (`prefetch_placesingle.cpp`, `play_cervino_indexed` split branch): after the ssl loop, the code was missing the common finalize block: `try_emplace(nodeStates_, node)` → get `registerCervino` → `addr(idxReg, registerCervino)` → `wvfImpl(idxReg, totalSize, is4Ch)`. GDB-confirmed path: `jge 0x1db6f8` → `je 0x1dbb75` → `addr` at `0x1dbbb9` → `jmp 0x1d9d3a` → `wvfImpl` at `0x1d9ef7`.

  **Bug D** (`prefetch.cpp`, `splitPath` in `allocate`): `if (playNode && playNode->length != 0)` indexed-path check was incorrectly placed inside `splitPath` (split_=true branch). For split_ devices, the original binary ALWAYS uses the full-wave `numSamplesForCache` formula regardless of `playNode->length`. Only the `!split_` (Cervino) path has the `playNode->length != 0` indexed check. This caused `numSamplesForCache=256` (128×2) instead of the correct `2000` (1000×2 bytes), resulting in `prf R1, R2, 256` instead of `prf R1, R2, 2016`.

- **Confirmed by**: GDB traces at `0x162343` (asmPlay call args), `0x1d1a62` (play node lock), `0x1d1cb7` (Cache::allocate args showing rcx=2000), `0x1d1c52` (full-wave formula inputs), `0x1dbbb9` (addr call), `0x1d9ef7` (wvfImpl call).
- **Resolution**: 
  - Bug A: `Immediate rateImm(parseEnd[0].value_.toInt())`
  - Bug B: removed the erroneous `lengthReg` propagation block in `moveLoadsToFront`
  - Bug C: added `nodeStates_.try_emplace` + `addr` + `wvfImpl` after ssl loop in split branch
  - Bug D: gated indexed-path check with `!split_`: `if (!split_ && playNode && playNode->length != 0)`
  - Result: 1264/1272 passing (same as before; 6 new tests now pass byte-identical)

## IF-144  `MemoryAllocator::allocateFirstSuitableFreeBlock`: wrong free-block ordering after split

- **Found during**: investigating `ziasm_prefetching_0` difftest failures (all 10 device variants)
- **Source**: `memory_allocator.cpp`, `wavetable_ir.cpp` (FIFO Phase 2 allocation path)
- **Severity**: likely-bug
- **Status**: **fixed**

- **Description**: When `allocateFirstSuitableFreeBlock` consumed a free block and split it into `before` and `after` remainders, the two pieces were re-inserted using `insert(begin(), before)` and `insert(end(), after)`. This broke the implicit address-ordering of `freeBlocks_`: after wlong (5120B, waveIndex=5) was allocated at 0x80002000 from the tail of the free region `[0x80000400, 0xFFFFFFFF)`, the deque became `[{0x80003400, 0xFFFFFFFF}, {0x80000400, 0x80002000}]` — tail-after-wlong first, gap-before-wlong second. Subsequent small waveforms (waveIndex=2,3,4) matched the tail gap first (because wlong's cacheLineUsage claimed that CL), placing them at 0x80003400+ instead of 0x80000600+.

  **Root cause confirmed by GDB**: The binary stores *allocated* blocks in the deque (not free blocks), so it naturally iterates gaps in ascending address order. The recon's free-block deque must also be maintained in address order for correctness.

  **Fix**: use index-based insertion so both remainders are inserted at the consumed block's position in address order: `after` first at `idx`, then `before` at `idx`, yielding `[..., before, after, ...]`.

  **Incorrect addresses (recon before fix)**:
  - waveIndex=2 → 0x80003400 (orig: 0x80000600)
  - waveIndex=3 → 0x80000600 (orig: 0x80000800)
  - waveIndex=4 → 0x80003600 (orig: 0x80000a00)

  **Correct addresses after fix**: all waveform `addressValue` fields match the original binary exactly.

- **Confirmed by**: GDB trace of `allocateFirstSuitableFreeBlock` deque page contents after each Phase 2 allocation; recon debug `fprintf` showing before/after block values; sub-batch test results (all 1272 tests pass with 0 failures).
- **Resolution**: `memory_allocator.cpp:274–290`: replaced `insert(begin(), before) + insert(end(), after)` with index-based `insert(begin()+idx, after) + insert(begin()+idx, before)`.

## IF-145  `assignWaveIndexImplicit`: Phase 1 lambda was a no-op stub

- **Found during**: same `ziasm_prefetching_0` investigation
- **Source**: `wavetable_ir.cpp`, `assignWaveIndexImplicit`
- **Severity**: likely-bug
- **Status**: **fixed** (earlier in this session)

---

## IF-143b: playWaveIndexed emits extra ssl/addr/prf instructions (load_indexed_play misfiring)

(ID disambiguation 2026-05-08: this entry shared the IF-143 number with
the earlier `playWaveIndexed (split_ devices)` entry above; renamed to
IF-143b for clarity.  Both are fixed.)

- **Found during**: `ziasm_various_playback_stuff_1/2` investigation
- **Source**: `prefetch.cpp` (`createLoad`, `moveLoadsToFront`) and `prefetch_placesingle.cpp` (`load_indexed_play` trigger)
- **Severity**: likely-bug
- **Status**: **fixed**

### Description

`playWaveIndexed` on UHF devices (UHFLI/UHFAWG/UHFQA) generated extra instructions before the fix:

- An extra `addi R4, R1, 0` + `ssl R4, R4` + `addr R3, R4` + `wwvf` + `prf`/`wprf` block appeared before the correct variable-init + ssl/addr/wvf sequence.
- The `prf` size was also wrong (256 instead of 320 for a placeholder(160) wave).

### Root Cause (three interacting bugs)

**Bug A** (`prefetch.cpp`, `moveLoadsToFront`, formerly lines 842-853):
A previous reconstruction incorrectly copied `lengthReg` from the matched play node to the load node inside `moveLoadsToFront`. Binary-verified (deep-thinking model analysis): at `0x1cd313`, the binary calls `vector::insert` to merge play lists but does **not** access offset `+0x88` (lengthReg). This copy was removed.

**Bug B** (`prefetch_placesingle.cpp`, `load_indexed_play` trigger, step 4):
The `load_indexed_play` path fires when the load node's `lengthReg` is valid and non-zero. Since `createLoad` legitimately copies `lengthReg` (needed for the large-waveform `split_=false` path), the guard needs to check `split_`. Binary-verified: at `0x1d1a84` the binary checks `0xbc(%r14)` (`split_`) before reaching `0x1d1a9b` (`playNode->length`). Fixed by adding `if (!split_)` around step 4.

**Bug C** (`prefetch.cpp`, `allocate`, cache size computation, formerly line 1649):
The indexed allocation path (`playNode->length * channels * 2`) was entered unconditionally when `playNode->length != 0`. For small waveforms where `split_=true`, the binary skips this path entirely (the `split_=true` jmp at `0x1d1a84` bypasses `0x1d1a9b`). Fixed by adding `!split_` to the condition.

### Effect of fix

- `ziasm_various_playback_stuff_1/2` (UHFLI/UHFAWG/UHFQA): 6 tests fixed (FAIL → PASS).
- `uhf_doc_tv_mode` (large waveform, `split_=false`): unaffected, still passes.
- No regressions.

- **Description**: The Phase 1 lambda in `assignWaveIndexImplicit` was reconstructed as a no-op that returned immediately without assigning auto-indices. The binary assigns indices to all waveforms with `waveIndex == -1` by calling `waveIndexTracker_.assignAuto()` in a lower-bound loop over `waveIndexTracker_.indices_`. This caused all waveforms with explicit `assignWaveIndex(...)` calls to keep their correct indices, but the placeholder waveform (which has no explicit index) was left with `waveIndex = -1` instead of the next available index (7 for this test), which then cascaded into wrong `fixed_` / `addressValue` for the Phase 1 FIFO allocation.
- **Resolution**: implemented the lower-bound loop over `indices_` in `assignWaveIndexImplicit` Phase 1 lambda.

---

## IF-146  Resources::Variable layout: subTypeRaw at +0x04, value.type_ at +0x08

**Source**: Phase 20e-ii correction sweep
**Status**: confirmed (fixed)
**Severity**: cosmetic (was a bug, now correct in source)

Earlier reconstruction had `pad_04` at +0x04 and `subType` at +0x08. The
disassembly shows the binary reads from +0x04 in `getVariableSubType`
(@0x1e4580), and `addVariable` / `addConst` callers write the caller-supplied
`st` arg to +0x04 with a hardcoded secondary-tag literal at +0x08. Correct
layout: `subTypeRaw` at +0x04, `value.type_` at +0x08. See `resources.hpp`
`Variable` struct and `resources.cpp` `addConst` (@0x1e7150).

---

## IF-147  Resources::Function ctor: four args, not three

**Source**: Phase 20e-ii correction sweep
**Status**: confirmed (fixed)
**Severity**: cosmetic (was a bug, now correct in source)

The fourth argument is a `weak_ptr<Resources>` for the parent scope. The
function's own child scope is constructed inside the ctor via
`std::allocate_shared` and stored at +0x60. Binary ctor @0x1eaa00.

---

## IF-148  VarType enum mapping corrected (Phase 19c-followup Finding 1)

**Source**: Phase 19c-followup
**Status**: confirmed (fixed)
**Severity**: cosmetic (was wrong, now correct)

Previous mapping had Const=3 / Cvar=4 / String=5 / Wave=6. Verified mapping
from binary jump table at 0x95c2a0 (function @0x247dd0):
  0→"notype", 1→"void", 2→"var", 3→"string", 4→"const", 5→"wave", 6→"cvar".

---

## IF-149  CustomFunctions field layout: earlier pass used wrong destructor

**Source**: Phase reconstruction correction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was a bug, now correct in source)

An earlier pass mis-attributed offsets +0xF8/+0x100/+0x128/+0x150/+0x168 to a
destructor at 0x1306c1, which belongs to pybind11 internals, NOT
CustomFunctions. All field offsets re-verified against real dtor @0x127c90 and
methods: lookupNode @0x15c530, addNodeAccess @0x15c6c0, getNodeAddress @0x16ba10.

---

## IF-150  CustomFunctions::nodeMap_: pointer not inline map

**Source**: lookupNode reconstruction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was a bug, now correct in source)

Was previously declared as `std::map<std::string, NodeMapItem>` (24 bytes
inline). Real layout: single 8-byte pointer at +0xD0, dereferenced and passed
to `NodeMap::retrieve(...)` — from `lookupNode` @0x15c530 line 0x15c54e.

---

## IF-151  AWGAssemblerImpl: spurious fields from earlier reconstruction

**Source**: Phase reconstruction correction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was noise, now removed)

Previously-listed `field_70_` (void*), `pad0_`, `opcodes_begin_/_end_` and
`sourceLines_begin_/_end_` had no corresponding storage in the binary.
`opcodes_` and `sourceLines_` are std::vectors accessed at offsets 0x50 and
0x78 via normal vector interface. The region 0x00..0xF0 previously misidentified
as `remaining_fields_[0x80]` is actually the embedded AsmParserContext at
offset 0xf0.

---

## IF-152  Prefetch: pageSize_ was hallucinated

**Source**: Prefetch reconstruction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was noise, now removed)

`pageSize_` appeared only in the constructor init list, was never read
elsewhere. The +0xBC slot is the bool `split_` initialized separately.

---

## IF-153  WaveformFront: args_ was hallucinated

**Source**: WaveformFront reconstruction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was noise, now removed)

`args_` had no usage anywhere in the binary. Other previously-misidentified
fields were actually aliases for Waveform members already present:
sampleLength→Waveform::signal.length_ (+0xD0), fileType→waveformType (+0x18),
isModified→dirty_ (+0xDC), funDescrName_→Waveform::funDescrName (+0x50).

---

## IF-154  Cache::Pointer: pageCount was hallucinated

**Source**: Cache reconstruction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was noise, now removed)

No `Cache::Pointer::pageCount` symbol exists in binary and no consumer in
reconstructed source ever accessed such a field. Pointer is exactly 0x24 bytes.

---

## IF-155  Empty input not rejected by recon (error 43 unreachable)

**Source**: zivibes intake — `sp_01_empty.seqc` on HDAWG8 / SHFQA4 / SHFSG8 / UHFQA
**Status**: **fixed** — all 4 sp_01_empty cases pass; full suite 1600/1600
**Severity**: likely-bug

### Final fix

Added the missing `0x11f283 → 0x11f557` early-return branch in
`Compiler::compile`: when `parse()` returns a null `Expression*`, allocate
the same empty `WavetableIR` the binary does and return immediately
with an empty `vector<Assembler>` asmList.  Downstream
`AWGCompilerImpl::writeToStream` (already guarded in IF-155 partial fix
at `awg_compiler.cpp:751-759`) then sees the empty opcode vector and
throws `EmptyInput` (error id 43) — matching the binary.

`reconstructed/src/codegen/compiler.cpp` ~line 251:
```cpp
auto expr = parse(normalized);
if (!expr) {                                                // 0x11f28a..0x11f28d
    auto wavetableIR = std::allocate_shared<WavetableIR>(
        std::allocator<WavetableIR>(), *wavetable_, *deviceConstants_,
        detail::AddressImpl<uint32_t>(config_->addressImpl),
        static_cast<size_t>(config_->wavetableSize),
        config_->searchPath, cancelCallback_);
    return CompileResult{std::vector<Assembler>{}, std::move(wavetableIR)};
}
```

### Original analysis (kept for reference)

Original binary, given a SeqC source consisting only of comments/whitespace
(no statements, no waveform definitions), errors with:
  `"Compilation failed: nothing to write, empty input"`
This corresponds to error-message id 43 in `error_messages.cpp:178`
(`m[43] = "nothing to write, empty input"`).

Recon's `Compiler::compile` runs the full pipeline (label, placeholder,
trailer emit) for empty input and ends up with `assemblerImpl_->getOpcode()`
returning **5 instructions** (`cwvf 5242816`, `st R0, 146`, `wwvf`, `nop`,
`end`).  `writeToStream` then proceeds without raising any error.

### GDB-confirmed binary flow

1. **`Compiler::compile @0x11f150`** is called.
2. After `parse()` returns at `0x11f283`: binary checks `test %r14, %r14`
   and at `0x11f28d` does `je 0x11f557` — **if expr is NULL, jumps to a
   short alternate path that bypasses steps 10–11** (label, placeholder,
   trailer).  This path still allocates a `WavetableIR` etc. but never
   appends any opcodes to `assemblerImpl_`.
3. Control returns to `AWGCompilerImpl::writeToStream @0x108cc0`.
4. `assembler_.getOpcode()` returns an empty vector (`begin == end`).
5. `@0x108d0b cmp begin/end`; `@0x108d0f je 0x109a00`.
6. `@0x109a00..0x109a14`: `__cxa_allocate_exception(0x60)` →
   `mov $0x2b, %esi` → throws `ZIAWGCompilerException` with
   `ErrorMessages::format(EmptyInput)` (id 43).

GDB observation for `sp_01_empty.seqc`:
- `assembler_.getOpcode()` returned `begin=0x0 end=0x0 size=0` (binary)
- recon emits 5 opcodes for the same input

### What was fixed in this pass

`writeToStream` now correctly raises `EmptyInput` when opcodes are empty
(matching `0x108d0f → 0x109a00 → 0x109a14`), instead of silently
`return`-ing.  See `awg_compiler.cpp:751-759` after this commit.  This
guard is dormant for `sp_01_empty.seqc` because recon's pipeline never
produces an empty opcode vector.

### What remains unfixed (still failing)

The root cause: recon's `Compiler::compile` does not take the
"parse returned null → skip trailer emission" branch at the equivalent
of `0x11f283/0x11f557`.  Recon's `parse()` (at `compiler.cpp:151`) wraps
whatever the bison-generated `seqc_parse` produces in a `shared_ptr`;
for empty source, the parser may produce a non-null empty-block
Expression rather than the null pointer the binary expects.

**To finish the fix**:
1. GDB-trace the binary's parse() on empty source to see whether it
   returns a true `nullptr` Expression* or whether the null-check at
   `0x11f28a` is on something else (e.g., the unwrapped raw pointer
   from `seqc__scan_string` rather than the parser result).
2. GDB-trace what the alternate path at `0x11f557..0x11f5b6` actually
   does (the `WavetableIR` allocation) and find where it returns from
   `compile()` without reaching the trailer-emit block.
3. Mirror that branch in recon's `Compiler::compile` so empty input
   produces an empty assemblerImpl_ — the existing `EmptyInput` guard
   in `writeToStream` will then fire and produce the correct error.

Promoted to TODO 47.1 (closed — fixed via early-return branch above).


---

## IF-156  Recon register allocator stricter than binary on ~17-variable programs

**Source**: zivibes intake — `hb_b_reg_count.seqc` (17 `var` declarations)
**Status**: **fixed** — `hb_b_reg_count` now byte-identical; full suite 1596/1600
**Severity**: likely-bug

### Root cause (final)

Two bugs compounded:

1. **splitReg body** was a stub.  Reconstructed per the binary disassembly
   at 0x281000..0x2814cc as a per-iteration Block 1 / Block 2 boundary
   writer with threshold ≥10, fresh-reg allocation from
   `GlobalResources::regNumber`, current-instruction renaming of regSrc +
   regAux (NOT regDst), and an epilogue that invalidates start.cmd
   (and end.cmd if `end != list.end()`) only when `allSplitOk && didSplit`.
   GDB-verified per-call counts: 3, 3, 3, … (16 productive calls), then
   0, 0, … (48 trailing calls on planted ADDIs); total 64 calls.

2. **splitConstRegisters Pass 1** was incrementing the wrong TLS counter.
   The barrier-creation loop at 0x2804f4 reads `(%r14)` where r14 was set
   at 0x2804b2 via `__tls_get_addr` for the TLS module symbol at offset
   `b7acf8` plus `+0x40`.  TLS+0x40 is `AsmList::Asm::createUniqueID`'s
   `nextID` counter — **not** `GlobalResources::regNumber` (which lives at
   TLS+0x48, accessed via the separate symbol at `b7ad10`).  The recon
   line `int newSeqId = GlobalResources::regNumber++;` over-incremented
   regNumber by ~120 per compile, so when `splitReg` later allocated
   fresh regs from regNumber it produced values around 169..216 — far
   outside the live-range table (sized `numSlots = numRegs+1 = 111`)
   that `registerAllocation` builds.  This caused the allocator to skip
   those regs entirely (silent OOB-guarded `addToLiveRange`), leaving
   them un-renamed.  The text re-emit in `compileString` then re-parsed
   regs ≥ 16 and `getReg` rejected them as "register out of range" —
   surfacing as `Assembler message at N : ... %1% ... %4% argument(s)
   given` because of an unrelated msg-id mismatch in the recon's
   error_messages table.

   Fix at `reconstructed/src/asm/asm_optimize_regalloc.cpp:440`:
   ```cpp
   int newSeqId = AsmList::Asm::createUniqueID(false);  // TLS+0x40
   ```
   instead of `GlobalResources::regNumber++` (TLS+0x48).

### Verified

- `tests/diff_test.py --filter hb_b_reg_count -v` → byte-identical (7580 bytes).
- Full suite via `tests/diff_test_fast.py`: **1596/1600** (was 1595/1600, no regressions).
- GDB on original confirmed `regNumber=244` at first splitReg call (after
  Pass 1's 122 createUniqueID++), then `247, 250, 253, …` per call —
  the binary really does use regNumber for fresh splits.  My recon now
  matches that trajectory because Pass 1 no longer touches regNumber.

### Historical notes (kept for reference)

- IF-156's earlier per-call breakdown (16 productive × 3 splits + 48
  zero-split calls = 64 total) is correct and matches the post-fix recon.
- The `[rbp-0x48]` field in splitConstRegisters is reused: in Pass 1 it
  holds `asm_.end()` (set at 0x2804a5), in Pass 2 it is reset to 0 at
  0x280737 and then `incq`-ed at 0x280877 once per outer iter to count
  splitReg calls.  Return value is `splitCount + numRegs` (= 64 + 46 =
  110 for `hb_b_reg_count`).

### Recon-side state (post commit `<tbd>`)

- `splitConstRegisters` is now correctly wired to call `splitReg` (was
  comment-only `++splitCount` before).
- Pass-1 barrier construction now writes `magicReg` to `regSrc` (not
  `regDst`) — required for the post-pass strip filter to match the binary.
- Pass-1 emits **two** barriers per non-skip instruction (matching binary
  at 0x2805e0..0x2806d4).
- Pass-2 forward-scan logic now matches the binary's branching at
  0x2807cc..0x28083a, including the `regSrc==destReg` requirement for
  outer cmd `ADDI` and the `regSrc==0` requirement for outer cmd
  `INVALID/cmd4`.
- Pre-call overwrite check now scans `[it+1, list.end())` skipping
  `splitEnd` (matches 0x28088b..0x2808d1), instead of stopping at
  `scanEnd`.
- `tmpList` is now an `AsmList` (was `std::vector<Asm>`) so the
  splitReg call type-checks; layouts are identical.

### Open: splitReg body still produces too few splits

With the catch path wired up, recon's `splitReg` produces only 1 split
per call where the binary produces 3. Symptoms:
- `b_reg_count` still throws "run out of free registers"
- No regressions in the suite (1595/1600 holds); other tests don't exercise
  the multi-split case

The recon `splitReg` body at `asm_optimize_regalloc.cpp:673-783`
currently:
- Allocates `newReg` only on the first split (`if (!didSplit)` guard)
- Always overwrites the same fixed `startOff`/`endOff` slots, so
  multiple splits within one call would overwrite each other's
  boundary writes anyway.

The binary at `0x2811af mov 0x88(r12),%r14d` clobbers what we thought
was `instrCount`, then immediately at `0x2811b7..0x2811cf` does
nontrivial work involving a stack local at `[rbp-0x50]` (a counter
that gets `++(*ptr)` at `0x2811bd`) and a stack region at `[rbp-0x1e8]`
which is later passed to something via `r13`. This region is not yet
reverse-engineered; it likely allocates a new boundary slot for each
split rather than reusing fixed `startOff`/`endOff`.

**Reproduces**: `python tests/diff_test.py --filter b_reg_count -v`

Remains in TODO.

---

## IF-157  playWave_variants: waveform size halved + multiple section diffs

**Source**: zivibes intake — `ht_h_func_030_playWave_variants.seqc` on HDAWG8
**Status**: **fixed** in commit `22d812a`
**Severity**: likely-bug

Differences vs original:
- `e_entry`: orig=0x800002c0 recon=0x80000240 (32 fewer instructions)
- `.text`: first diff at offset 0x14 (instruction 5)
- `.asm`: text diff
- `.waveforms`: JSON diff
- `.wavemem`: numeric diff
- **`.wf___playWave_15_8`: size 256 vs 128** — recon emitted a waveform
  exactly half the size of the original

### Root cause

`WaveformGenerator::merge` (`waveform_generator_dsp.cpp:1983`) computed
`frameCount` from `signals[0].samples_.size()` only — taking the first
channel's length. For `playWave(w1, w2)` with `w1=ones(32)` and
`w2=zeros(64)`, this produced a 32-sample merged waveform instead of 64.

GDB-confirmed at binary offset `0x25fb3c`: `rsi=64` (the maximum) is
passed to the `Signal` constructor. Binary takes the **max** length
across all input signals, not the first one's.

### Fix

Iterate all signals and take the max length:

```cpp
size_t frameCount = 0;
for (const auto& sig : signals)
    frameCount = std::max(frameCount, sig.samples_.size());
```

**Resolved test**: `ht_h_func_030_playWave_variants` now byte-identical.

---


## IF-158  Missing static `cwvf` in else-arm of in-loop if/else after cwvfr-arm

**Source**: stress test `kitchen_sink_hdawg`, isolated to `if158_cwvf_in_loop.seqc`
**Status**: fixed (phase 57.G.1)
**Severity**: likely-bug

### Resolution

Two related bugs in `Prefetch::optimizeCwvf` Branch handling
(`reconstructed/src/codegen/prefetch.cpp`):

1. The `expectedCwvf` snapshot was being overwritten on every branch
   iteration instead of only the first.  The binary uses a separate
   `isFirstBranch` flag at stack slot `-0x90(%rbp)` (initialized from
   `r15b=1` at 0x1cfdb7, cleared to 0 at 0x1cfe10/0x1d0230 after each
   iteration's recursive `optimizeCwvf` returns).  The recon had
   conflated this with the `allSameConfig` accumulator at `-0x48(%rbp)`,
   so divergence between the if-arm (`channelMask=3`) and else-arm
   (`channelMask=1`) tail configs was never observed: each iteration
   trivially compared `expectedCwvf` against itself.

   Effect: the Branch's running cwvf was never reset to the
   "diverged" sentinel (`channelMask = 0xFFFFFFFF`), so the surrounding
   Loop inherited a wrong common-state cwvf and `needsNewCwvf` saw
   `prev->channelMask == cur->channelMask` and skipped the static
   `cwvf` reset emit.

2. While auditing `Prefetch::needsNewCwvf` Loop case
   (`prefetch_emit.cpp` 0x1dc7e8–0x1dc806) a polarity bug was found
   and fixed: when `prev->loop == curNode`, the binary preserves
   `seenDifference` (jumps to 0x1dc810 which sets `sil = r12d`); the
   recon was instead writing `holdDiffers`.  This was not the trigger
   for this test but was confirmed via static disassembly.

**Verification**:
- `python tests/diff_test.py --manifest manifest-stress.json --filter if158 -v` → PASS (byte-identical, 16380 bytes)
- `python tests/diff_test.py --manifest manifest-stress.json --filter kitchen -v` → PASS (byte-identical, 45000 bytes)
- Main suite 1600/1600.
- Stress: 21 → 19 failures (cleared `if158_cwvf_in_loop_hdawg`,
  `kitchen_sink_hdawg`; no regressions).

**Root-cause confirmation**: GDB-traced original `needsNewCwvf` calls
showed orig's Loop ancestor of the else-arm Play had
`currentCwvf.channelMask = 0xFFFFFFFF` (divergence sentinel) while
recon's had `1` (else-arm value).  Walking the parent chain in both,
the Branch node showed `cm=0xFFFFFFFF` in orig vs `cm=1` in recon —
isolating the bug to the Branch divergence-detection logic upstream
of `needsNewCwvf`.

### Original symptom

A for-loop containing an if/else where:
- the if-arm plays a dual placeholder (`playWave(phA, phB)`) → emits
  `cwvfr Rn` (register-form cwvf, runtime config), and
- the else-arm plays a single-channel placeholder (`playWave(1, phC)`)
  → should emit a static `cwvf <imm>` to reset the config,

drops the static `cwvf` instruction in the recon's else-arm. Resulting
diff is exactly one missing `.text` instruction (4 bytes), one missing
`.asm` line, and one missing `.linenr` entry (8 bytes).

The same if/else pattern **outside** any loop emits correctly in both
arms. The trigger is purely the surrounding for-loop.

### Minimal repro

`tests/cases/stress/if158_cwvf_in_loop.seqc`:

```seqc
wave g1  = gauss(128, 64, 16);
wave phA = placeholder(512, true, true);
wave phB = placeholder(512, true, true);
wave phC = placeholder(2048, false, false);

assignWaveIndex(phA, phB, 50);
assignWaveIndex(1, phC, 51);

var u0 = getUserReg(0);

playWave(1, g1);            // <-- removing this makes the bug disappear

for (var k = 0; k < 4; k = k + 1) {
  if (u0 + k > 5) {
    playWave(phA, phB);     // dual placeholder → cwvfr in if-arm
  } else {
    playWave(1, phC);       // single placeholder → static cwvf in else-arm
  }                          //                     (DROPPED in recon)
  waitWave();
}
```

Run: `python tests/diff_test.py --manifest manifest-stress.json --filter if158 -v`

### Suspected location

Per subagent investigation `ses_1fde0697fffeWuLAE3xLqp38Lv`:

| Function | File:line | Binary addr |
|---|---|---|
| `Prefetch::needsNewCwvf` | `reconstructed/src/codegen/prefetch_emit.cpp:272` | `0x1dc620` |
| `Prefetch::placeSingleCommand` (Play case) | `reconstructed/src/codegen/prefetch_placesingle.cpp:445` | `0x1d7d49`–`0x1d7e21` |
| `Prefetch::optimizeCwvf` (Branch / Loop cases) | `reconstructed/src/codegen/prefetch.cpp:292`, `:561` | `0x1cfd74`, `0x1d046f` |

Hypothesis (unconfirmed, requires GDB):

`needsNewCwvf` walks up the parent chain comparing `Node::currentCwvf`
(+0x68). For an in-loop Play, the walk passes through the if/else
Branch (skipped) and then into the for-Loop's Loop node case, which has
~250 lines of inlined PlayConfig comparisons with `seenDifference`,
`loopBodyRunsAtLeastOnce`, `prev->loop == curNode`, and
`prev->branchMaySkipAllBodies` paths.  Likely candidates:

1. `prefetch_emit.cpp:354–371` — Loop case `prev->loop.get() == curNode.get()`
   test and its surrounding `seenDifference` / `loopBodyRunsAtLeastOnce`
   logic (wrong polarity or wrong field offset).
2. `prefetch_emit.cpp:414` (`checkSeenDifference` label) — wrong
   polarity of `seenDifference` causing fall-through to `treeWalk`
   instead of comparing the running config.
3. `prefetch.cpp:564` Loop case — `curNode->currentCwvf = cwvf` early
   write *before* loop-body recursion may carry stale pre-body config
   instead of the post-body invalid sentinel set by Branch divergence
   at `prefetch.cpp:457–466`.

### Recommended next step

Per AGENTS.md "GDB tracing for binary analysis": set breakpoints at
`needsNewCwvf` entry/exit and the placesingle dispatch sites, run on
the minimal repro, and observe which branch the binary takes for the
elsePlay — vs which branch recon takes.  GDB recipe in subagent report
(see commit message of stress-suite commit for task ID).

### Related tests

- `kitchen_sink_hdawg` (full kitchen sink) — fails with same signature
- `if158_cwvf_in_loop_hdawg` (minimal repro) — fails with same signature


 ## IF-159  Recon aborts on duplicate `assignWaveIndex` for same waveform

**Source**: stress test `cmdtable_huge` (during reduction)
**Status**: fixed (phase 57.C.2)
**Severity**: likely-bug (crash vs. graceful error)

### Fix (phase 57.C.2)

`WavetableFront::assignWaveIndex` in
`reconstructed/src/waveform/wavetable_front.cpp:354` had a literal
`throw;` (re-throw) on the "already assigned different index" branch
with no in-flight exception, which std::terminate-d the worker with
`terminate called without an active exception` (worker exit -6).

Replaced with the correct binary behavior:
```cpp
throw WavetableException(
    ErrorMessages::format(WaveAlreadyAssigned, ptr->name));
```

`WaveAlreadyAssigned` is ErrorMessage id 248 (=0xF8): `"waveform %1%
has already assigned index"`. The recon now emits the exact same
diagnostic as the binary on duplicate-assign, byte-identical.

### Verification

- All 5 stress repros (`if159_assignwave_dup_crash`,
  `assign_same_wave_multi`, `exec_table_var_index`, `ct_high_index`,
  `assign_then_execute_immediate`) now produce byte-identical error
  messages to the original (previously the recon worker SIGABRT'd).
- Main suite: 1600/1600 passing.
- Stress suite: 419/444 (unchanged — the 5 were already PASS in the
  difftest because both errored, just with different messages).

### Symptom

When the same waveform is passed to `assignWaveIndex` twice with
different (channel, index) tuples, the original binary throws a clean
compiler error:

```
Compiler Error (line: N): waveform <name> has already assigned index
```

The recon instead **aborts** the worker with:

```
worker exit -6: terminate called without an active exception
```

This is a missing exception-throw or wrong control flow somewhere in
the second-assignment path of `assignWaveIndex` — the recon presumably
reaches a `throw;` with no in-flight exception, or hits an uncaught
exception that std::terminate catches.

### Minimal repro

`tests/cases/stress/if159_assignwave_dup_crash.seqc`:

```seqc
wave g1 = gauss(128, 64, 16);

assignWaveIndex(1, 2, g1, 100);
assignWaveIndex(1, g1, 150);   // <-- recon crashes here
```

Run: `python tests/diff_test.py --manifest manifest-stress.json --filter if159 -v`

The stress harness reports PASS because both errored, but the recon's
behavior is wrong — should be a clean compiler error string, not a
worker abort.

### Suspected location

`assignWaveIndex` codegen / waveform-registration path. Look for:
- `WavetableEntry`-already-assigned check that should `throw` a
  `CompilerError` but instead falls through, or
- An exception thrown from inside a destructor / catch block (which
  std::terminate-s with `terminate called without an active exception`).

Likely files: `reconstructed/src/codegen/custom_functions_*.cpp`,
`reconstructed/src/codegen/waveform_*.cpp`, or wherever
`assignWaveIndex` dispatches.

### Recommended next step

1. Run the worker directly on the minimal repro under GDB to capture
   the crash backtrace.
2. Identify the throw site / destructor exception.
3. Compare with the binary's "already assigned" error path and fix.

## IF-161  setSinePhase rejected on SHFQC sg with phantom node check

**Status**: fixed (phase 57.E)
**Severity**: bug
**Found**: stress phase 50 (`shfqc_sg_combo.seqc`, isolated to
`if161_shfqc_setsinephase.seqc`)

### Root cause

`dispatchHighDevType` in `reconstructed/src/runtime/get_node_map.cpp`
(binary `@0x1ba360`) was missing the `case 32` (SHFQC_SG) arm: a
comment said "no GetNodeMap specialization in binary", and SHFQC_SG
fell through to the VHFLI default. As a result, the SHFQC sg node
tree had only `oscs/N/freq` virtual entries (the VHFLI/lyrebird
node-map) and no `sgchannels/...` paths, so `setSinePhase` and
`setOscFreq` reported the sgchannels node as missing.

The binary's jump table at `0x9598f8` shows that idx0 (SHFSG=16) and
**idx1 (SHFQC_SG=32) both point to `0x1ba393` → `GetNodeMap<SHFSG>::get()`**.
Verified by reading the jump table contents directly:
```
idx0: target=0x1ba393   (SHFSG)
idx1: target=0x1ba393   (SHFSG — SHFQC_SG aliases this)
```

### Fix

Added `case 32: return GetNodeMap<AwgDeviceType::SHFSG>::get();` to
`dispatchHighDevType`. SHFQC_SG now uses the same node tree as plain
SHFSG, matching the binary.

### Verification

- `if161_shfqc_setsinephase_shfqc[SHFQC, seq=sg]` now byte-identical.
- `oscfreq_dense_shfqc[SHFQC, seq=sg]` now byte-identical (also fixes IF-167).
- Main suite still 1600/1600.
- Stress suite: 416 → 419 passing (+3).

### Symptom

On `devtype=SHFQC sequencer=sg`, the recon errors out with:

```
Error: node 'sgchannels/0/sines/0/phaseshift' doesn't exist
```

The original binary compiles the same source without complaint.  The
exact same source on `devtype=SHFSG{4,8} sequencer=sg` is byte-identical
between orig and recon — i.e. `setSinePhase(0)` is supported on the
SHFSG sg path but the SHFQC sg path rejects it.

### Minimal repro

`tests/cases/stress/if161_shfqc_setsinephase.seqc`:

```seqc
setSinePhase(0);
playZero(64);
```

Run on SHFQC sg → recon errors; on SHFSG8 sg → byte-identical pass.

### Suspected location

The `setSinePhase` codegen path validates a node name against the
device's node tree.  For SHFQC, the recon's node tree (or the prefix
used to construct the node string) appears to omit
`sgchannels/N/sines/N/phaseshift`, while the binary has it.  Likely
either:

- A device-specific node-tree initialization missing entries for SHFQC
  sg channels, or
- A path-construction routine using `qachannels/...` prefix on SHFQC
  even when sequencer=sg.

Files to inspect:
- `reconstructed/src/runtime/custom_functions_*.cpp` for `setSinePhase`
  registration / dispatch.
- `reconstructed/src/runtime/device_*.cpp` (or wherever device node
  trees are built) for SHFQC sg-channel nodes.

### Recommended next step

1. GDB on original SHFQC sg compile, breakpoint at the node-lookup site
   to confirm which node-tree entry succeeds.
2. Compare the recon's lookup path; locate missing entry.
3. Add the SHFQC sg-channel sine nodes (or fix the prefix construction).

## IF-162  assignWaveIndex dual placeholder uses first-signal length

**Status**: fixed (phase 57.G.2)
**Severity**: bug
**Found**: stress phase 50 (`many_placeholders.seqc`, isolated to
`if162_assignwave_dual_size.seqc`)

### Resolution

Bug was in `WaveformGenerator::merge` (`waveform_generator_dsp.cpp`),
**reserveOnly result path** (binary 0x25fa98) — the recon constructed
the result Signal as `Signal(tag, signals[0].length_, mergedBits)`,
taking the FIRST signal's length unconditionally.

Binary instead computes the length as the running max during the
per-signal load loop (0x25f7c9..0x25f7e2: `r12d = max(r12d,
signal.length_)` gated on `channels_!=0`), then post-loop folds in the
trailing requestedLength hint via `cmp %r12d,%eax; cmovg %eax,%r12d`
at 0x25f9bb..0x25f9c4, then validates each signal's length is `<=
r12d` (else throws error 0xe8). The accumulated `r12d` is the value
passed to the reserveOnly Signal ctor at 0x25fa98.

Structurally this is the same bug as IF-157 (which fixed the
non-reserveOnly path of `merge`). The IF-157 fix did not propagate to
the reserveOnly branch because for non-placeholder waves the load
loop's `checkAllocation()` makes `samples_.size() == length_`, so the
non-reserveOnly path's `frameCount = max(samples_.size())` already
gave the right answer — but for placeholders (`reserveOnly_=true`),
`samples_` stays empty and the function takes the early-return
reserveOnly path which had its own first-signal-length bug.

Fix:

```cpp
size_t resultLength = static_cast<size_t>(requestedLength > 0 ? requestedLength : 0);
for (auto& s : signals) {
    if (s.channels_ != 0 && s.length_ > resultLength)
        resultLength = s.length_;
}
ReserveOnly tag;
Signal result(tag, resultLength, mergedBits);
```

**Verification**:
- `if162_assignwave_dual_size_hdawg` byte-identical (5320 bytes).
- Main suite 1600/1600.
- Stress: 21 → 19 failures (cleared `if162_assignwave_dual_size_hdawg`
  and one related case from the same family — likely
  `many_placeholders.seqc` or another dual-placeholder
  assignWaveIndex case; no regressions).

### Original symptom

`assignWaveIndex(1, p_small, 2, p_large, idx)` with two placeholders of
different sizes computes the merged waveform's sample length from the
FIRST placeholder instead of the MAX of both.  The recon emits a
half-size `.wf___playWave_*` section.

For the minimal repro (p_small=256, p_large=512, dual-channel):

| section | orig | recon |
|---|---|---|
| `.wf___playWave_13_4` size | 2048 bytes (length=512) | 1024 bytes (length=256) |

Symptom is structurally identical to IF-157 (which fixed the same
first-signal-length bug on the `playWave` path inside
`WaveformGenerator::merge`).  Initial suspicion was the
`assignWaveIndex` / pinned-CT-entry codegen path; actual root cause
turned out to be the **reserveOnly** branch inside `merge` itself,
which IF-157 did not touch.

### Minimal repro

`tests/cases/stress/if162_assignwave_dual_size.seqc`:

```seqc
wave p_small = placeholder(256, true, true);
wave p_large = placeholder(512, true, true);

assignWaveIndex(1, p_small, 2, p_large, 13);
executeTableEntry(13);
```

Run: `python tests/diff_test.py --manifest manifest-stress.json --filter if162 -v`

## IF-163  Recon refuses to unroll repeat(N) above its threshold

**Status**: fixed (phase 57.G.3)
**Severity**: bug
**Found**: stress phase 51 (`large_repeat_constants.seqc`, isolated to
`if163_repeat_unroll_limit.seqc`)

### Symptom

For `repeat(N)` with sufficiently large constant N, the recon errored:

```
Compiler Error (line: N): too many iterations to unroll this loop,
use a const variable for infinite loop or a var variable for this many iterations
```

The original binary compiled the same source successfully — it emits
a sequencer loop instruction instead of unrolling.

Confirmed before fix:
- `repeat(10000) { playZero(32); }` — both pass byte-identical
- `repeat(1000000) { playZero(32); }` — recon errored, original passed

### Root cause

In `SeqCRepeat::evaluate` the recon (file
`reconstructed/src/ast/seqc_ast_eval_control.cpp`, the
`countInt >= 2` branch) evaluated the body in a `maybe_unroll`
sub-scope FIRST and then, if `countInt > ctx.loopUnrollLimit`, raised
error 0x7b ("too many iterations to unroll").

The binary disassembly at `0x222fcf`–`0x2231d6` does the opposite:

```
0x222f76: cmp $0x2,%eax            ; countInt vs 2
0x222f79: jge 0x222fcf              ; >=2 → limit-check branch
0x222fcf: mov %eax,-0x110(%rbp)
0x222fd5: cmp 0x48(%r13),%eax       ; countInt vs loopUnrollLimit
0x222fd9: jg  0x2231d6              ; > limit → JUMP TO RUNTIME LOOP INIT
0x222fdf: ...                       ; fall through: do maybe_unroll body eval
...
0x2231d6: mov 0x8(%r13),%rbx        ; runtime-loop init (NOT an error site)
0x2231e8: xor %esi,%esi              ; AsmRegister(0) for addi
0x2231ea: call AsmRegister::ctor
0x2231ff: call Immediate(int)        ; Immediate(countInt)
0x22321b: call AsmCommands::addi     ; addi(counterReg, 0, countInt)
```

There is **no** error-0x7b throw site anywhere in `SeqCRepeat::evaluate`
for the const-count path. When `countInt > loopUnrollLimit`, the
binary skips the body eval entirely and emits a runtime loop
(`addi(counterReg, R0, countInt)` then `goto var_path`).

The previous reconstruction misread `0x2231d6` as an error site
(see comment "Error 0x7b: too many iterations") when it is in fact
the runtime-loop init label that the `jg` jumps TO.

### Fix

Move the `countInt > loopUnrollLimit` check to BEFORE the maybe_unroll
body eval.  When the limit is exceeded, skip body eval, emit
`addi(counterReg, R0, countInt)`, set `hasEndLabel=false`, and
`goto var_path`.  Removed the error 0x7b path entirely.

### Verification

- GDB-traced orig at `0x221c10`/`0x222f76`/`0x222fd5`/`0x2231d6`/`0x2231e8`
  with `repeat(200000) { playZero(64); }`. Trace hit `Cvar_path` →
  `limit_check` → (jg taken) → `addi_init` → `var_path`. No error
  ever fired; ELF size matched recon `repeat(10) { playZero(64); }`
  exactly (1416 bytes), confirming a constant-size runtime loop.
- `if163_repeat_unroll_limit_hdawg` stress test: PASS.
- `large_repeat_constants_hdawg/shfsg` stress tests: PASS (also
  fixed by the same change).
- Main suite: 1600/1600.
- Stress fails: 19 → 16 (3 fixed, no regressions).

## IF-164  assignWaveIndex named-form: recon error wording differs

**Status**: fixed (phase 57.F)
**Severity**: bug (cosmetic / error-string)
**Found**: stress phase 52 (`assign_pattern_mix.seqc`)

### Symptom

`assignWaveIndex("named_a", 1, w_a, 3)` where `"named_a"` was never
registered as a wave name:

```
orig:  Compiler Error (line: 11): waveform 'named_a' does not exist
recon: Compiler Error (line: 11): no waveform with the name 'named_a' found
```

Same error class, different wording.

### Root cause

Both error templates exist in the binary rodata:
  - 227 (`WaveformNotExist`) = `"waveform '%1%' does not exist"`
  - 233 (`WaveformNotFound`) = `"no waveform with the name '%1%' found"`

`PlayArgs::secureLoadWaveform` (recon @0x1711a0) was using template
233, but the binary at @0x171461 passes `0xE3 = 227`. Verified by
`objdump -d` over the format() call sites: 0xE3 is used at @0x14e02a
(lock), @0x14e543 (unlock), @0x171461 (secureLoadWaveform); 0xE9 is
used at @0x15dc39 (PlayArgs::getMaxSampleLength) and @0x29c76c
(WavetableFront::checkWaveformInitialized).

### Fix

`reconstructed/src/runtime/custom_functions.cpp:1050` — change
`WaveformNotFound` → `WaveformNotExist` in `PlayArgs::secureLoadWaveform`.

## IF-165  Compiler error format missing 'Compiler Error (line: N)' prefix

**Status**: fixed (phase 57.F)
**Severity**: bug (cosmetic / error-string)
**Found**: stress phase 52 (`arith_chain_pressure.seqc`)

### Symptom

When the recon hits "run out of free registers" (and possibly other
codegen-late errors), it emits the bare message instead of the
standard `Compiler Error (line: N): <msg>` framing the binary uses:

```
orig:  Compiler Error (line: 19): run out of free registers, please reduce complexity
recon: run out of free registers, please reduce complexity
```

### Root cause

Two issues:

1. `OptimizeException` was missing its `lineNumber_` field at +0x20.
   The binary's exception layout is:
   - +0x00 vtable
   - +0x08 std::string message_
   - +0x20 int lineNumber_   (set at throw site from asm_[vreg].lineNumber)
   - +0x28 total

   The throw site at @0x2802b5 does `mov %ebx, 0x20(%r14)` to store
   the line number into the freshly-allocated exception.

2. `Compiler::compile()` was missing the catch handler that converts
   `OptimizeException` into a `messages_.errorMessage(what, lineNr)`
   call.  Binary @0x121d09 catches the exception, reads message_
   (offset 0x10/0x18 long-form, or +0x09 SSO short-form), reads
   lineNumber_ at +0x20, and calls
   `CompilerMessageCollection::errorMessage(msg, line)` (12b720).
   That collection is what `compile_seqc.cpp:265` reads via
   `compiler.getCompileReport()` — which is then prefixed by
   `CompilerMessage::str()` with `"Compiler Error (line: N): "`.

Without the catch, the OptimizeException propagated up to the outer
generic handler in `compile_seqc.cpp` which used `ex.what()` directly
(no prefix).

### Fix

- `reconstructed/include/zhinst/asm/asm_optimize.hpp`: add
  `lineNumber_` field at +0x20 and `lineNumber()` accessor; add
  ctor overload `(msg, lineNr)`.
- `reconstructed/src/asm/asm_optimize.cpp`: implement new ctor.
- `reconstructed/src/asm/asm_optimize_regalloc.cpp`: pass
  `asm_[vreg].lineNumber()` into the OptimizeException at the two
  throw sites (lines 276 and 372).
- `reconstructed/src/codegen/compiler.cpp`: wrap
  `optimizePreWaveform` and `optimizePostWaveform` calls in
  `try/catch (OptimizeException const&)` that calls
  `messages_.errorMessage(e.what(), e.lineNumber())` then re-throws.

### Caveat: line number accuracy

The recon throws with `asm_[vreg].lineNumber()` where `vreg` is the
virtual register that failed to allocate.  The binary's indexing of
`asm_[idx]` to retrieve the line number uses a more complex
`r12+0x20`-based lookup that wasn't fully reconstructed; the recon
approximates by indexing with `vreg` directly.  This produces a
real-but-different line number (e.g. 12 vs 19 in the test case).
Diff-test treats this as "error messages differ (accepted)".
3. Wrap the throw with the standard error-message framing.

## IF-166  ZSYNC_DATA_PQSC_REGISTER accepted on SHFQA/SHFQC by recon

**Status**: fixed (phase 57.E)
**Severity**: bug
**Found**: stress phase 52 (`shfqa_feedback_branch.seqc`)

### Root cause

The constant `ZSYNC_DATA_PQSC_REGISTER` is correctly NOT registered
for SHFQA in `static_resources.cpp` (HDAWG/SHFSG/SHFQC_SG only).
However, `StaticResources::getVariable` in
`reconstructed/src/runtime/resources_static_global.cpp` was running
the deprecation-warning checks **even when `Resources::getVariable`
returned null** for the lookup, emitting a deprecation warning that
masked the real "tried to access unknown variable" error.

The binary at `@0x129ee6` does:
```
cmp QWORD PTR [rbx], 0x0
je  12a28d                 ; if var == null, skip deprecation, return null
```

i.e. the deprecation checks are gated on a non-null lookup result.

### Fix

Added an `if (!var) return nullptr;` early-return in
`StaticResources::getVariable` between the `Resources::getVariable`
delegation and the deprecation-name comparisons.

### Verification

- `shfqa_feedback_branch_shfqa[SHFQA4, seq=qa]` now reports the
  matching `tried to access unknown variable 'ZSYNC_DATA_PQSC_REGISTER'`
  error (was reporting the deprecation warning string).
- Same for `shfqa_feedback_branch_shfqc[SHFQC, seq=qa]`.
- Main suite still 1600/1600.

### Symptom

On SHFQA / SHFQC qa, the constant `ZSYNC_DATA_PQSC_REGISTER` is:

```
orig:  Compiler Error (line: 6): tried to access unknown variable 'ZSYNC_DATA_PQSC_REGISTER'
recon: Warning (line: 6): constant 'ZSYNC_DATA_PQSC_REGISTER' is deprecated, please use 'ZSYNC_DATA_PROCESS<truncated>'
```

The recon accepts the constant (with a deprecation warning), while
the original binary refuses to define it on SHFQA/SHFQC sequencers.
The constant is typically a HDAWG-only feedback-source identifier;
recon's constant-table or per-device guard is missing the SHFQA/SHFQC
exclusion.

### Minimal repro

`tests/cases/stress/shfqa_feedback_branch.seqc`, line 6:
`var fb = getFeedback(ZSYNC_DATA_PQSC_REGISTER, 0);` on SHFQA4 or SHFQC qa.

### Recommended next step

1. Locate the constant table that registers `ZSYNC_DATA_PQSC_REGISTER`.
2. Find the per-device gating (HDAWG vs SHFQA/SHFQC).
3. Add the missing per-device exclusion.

## IF-167  setOscFreq rejected on SHFQC sg with phantom node check

**Status**: fixed (phase 57.E — same fix as IF-161)
**Severity**: bug (same shape as IF-161)
**Found**: stress phase 52 (`oscfreq_dense.seqc`)

### Symptom

On `devtype=SHFQC sequencer=sg`, `setOscFreq(0, 100e6)` fails with:

```
recon: Compiler Error (line: 4): node 'sgchannels/0/oscs/0/freq' doesn't exist
```

while the original binary compiles fine.  Same source on plain SHFSG
sg passes byte-identical.  This is structurally identical to IF-161
(setSinePhase phantom node on SHFQC sg) — both are missing
sg-channel node-tree entries on SHFQC.

### Minimal repro

`tests/cases/stress/oscfreq_dense.seqc` on `devtype=SHFQC sequencer=sg`.

### Recommended next step

1. Same investigation as IF-161.  Likely both fixed by the same
   change to the SHFQC sg node tree.
2. Once IF-161 is fixed, re-test this case.

## IF-168  Empty if{}/for{}/repeat{} blocks generate different code

**Status**: fixed (passing as of phase 57.D verification)
**Severity**: bug (codegen)
**Found**: stress phase 52 (`empty_blocks.seqc`)

### Resolution

`empty_blocks_hdawg` and `empty_blocks_shfsg` are byte-identical on
the current main.  The fix landed implicitly via earlier 57.A–C
work; no codegen change was needed in 57.D.  Phase 57.D additionally
hardened `AsmOptimize::oneStepJumpElimination` to scan past
non-matching LABELs (matching binary @0x27e130 semantics), which is
not required by the current test surface but removes a known
binary-vs-recon control-flow divergence.

### Symptom

A program containing only empty-bodied control-flow constructs
(`if(x){}`, `if(x){}else{}`, `for(...){}`, `repeat(N){}`, plus
nested empties) compiles to byte-different `.text` between orig and
recon.  Both succeed, no error messages — pure codegen divergence.

### Minimal repro

`tests/cases/stress/empty_blocks.seqc` on HDAWG and SHFSG
(byte-different on both).

### Recommended next step

1. Use `tests/dump_elf.py --both` to compare `.asm` text and pinpoint
   which empty construct generates the difference.
2. Likely candidates: empty-loop branch placement, missing
   loop-counter setup elision, or extra no-op instructions in the
   empty branch arm.

## IF-169  Lexer: `*/` inside `// ...` line comment confuses recon

**Status**: fixed (phase 57.F)
**Severity**: bug (lexer)
**Found**: stress phase 52 (`heavy_comments.seqc`)

### Symptom

Recon parses a single-line comment `// foo /* bar */ baz` incorrectly
when the comment text contains both `/*` and `*/` markers — the lexer
treated `*/` as a block-comment terminator and ended comment state
mid-line.

```
recon: Compiler Error (line: 32): syntax error, unexpected IDENTIFIER, expecting ';'
```

### Root cause

`SeqcParserContext` field layout was wrong AND the start/end methods
were missing their guards.  Binary layout (verified by re-disassembly
of @0x247c00..@0x247c70):

  - +0x00 isComment_     (composite, set by either start)
  - +0x01 lineComment_   (NOT blockComment_ as the recon had)
  - +0x02 blockComment_  (NOT lineComment_)

And the methods have these guards (which the recon was missing):

  - `startBlockComment()`: if (lineComment_) return; — don't enter
    block-comment mode if already inside a line comment.
  - `endBlockComment()`:   if (lineComment_) return; — don't clear
    state if a line comment is active.  **This is what makes `*/`
    inside `// ...` a harmless no-op.**
  - `startLineComment()`:  if (blockComment_) return;
  - `endLineComment()`:    if (blockComment_) return;

Without the `endBlockComment()` guard, the `*/` lex action cleared
comment state mid-line, and subsequent text was tokenized as code.

### Fix

- `reconstructed/include/zhinst/ast/seqc_parser_context.hpp`: swap
  `lineComment_` and `blockComment_` field order to +0x01/+0x02.
- `reconstructed/src/ast/seqc_parser_context.cpp`: add the four
  cross-state guards to the start/end methods.

Verified byte-identical output on `heavy_comments.seqc`.

## IF-170  wavemem_pressure: chirp builtin sample-data diff

**Status**: fixed (2026-05-07, phase 57.G.5) — subsumed by IF-173 fix.
The single divergent section `.wf___chirp_14_23` was the same chirp-loop
FP-ordering bug; correcting `(di*di)*k` ordering in `chirp` made
`wavemem_pressure_hdawg` byte-identical.
**Severity**: bug (waveform generator)
**Found**: stress phase 52 (`wavemem_pressure.seqc`)

### Symptom

`tests/cases/stress/wavemem_pressure.seqc` on HDAWG: ELF size matches
(56000 bytes) but `.wf___chirp_14_23` (the 14th chirp invocation)
diverges starting at byte offset 0xC10.  All other sections including
`.wavemem`, `.text`, `.waveforms`, the other 11 builtin waveforms
(blackman, cosine, drag×2, gauss×2, hann, ones, rect, sinc, sine) are
byte-identical.

### What it is NOT

After phase 57.G.1, the cwvf-related part of this test (initially
suspected to overlap with IF-158) is confirmed unrelated: only the
chirp sample data differs.  Wavemem accounting and codegen are
correct.

### Recommended next step

Isolate a single-call repro of the failing chirp invocation and
compare against `Waveform::chirp` reconstruction; likely a parameter
formula or accumulator-precision issue at large offset/length.

### Original symptom

Compiling 12 large (2048-sample) waves of various builtin shapes back
to back yielded a `.text` and likely `.wf_*` byte diff between orig
and recon.  Initial triage suspected wavemem accounting; phase 57.G.1
narrowed to `.wf___chirp` only.

## IF-171  arith_chain (8-temp) bytewise codegen diff

**Status**: fixed (2026-05-07, phase 57.G.4) — `SeqCMinus::evaluate`
in `reconstructed/src/ast/seqc_ast_eval_arithmetic.cpp` had two
reconstruction bugs in the assemblers_-merge ordering for
subtraction:

1. **Row 3 (Var-Var)** swapped the merge order: recon merged
   `rhsResult.assemblers_` first then `lhsResult.assemblers_`, while
   the binary's path B (entered at @0x22cf78, Var-Var case) merges
   `lhsResult.assemblers_` first (via `mov -0x70(%rbp),%rcx` =
   lhsResult at @0x22cfc6, insert at @0x22cff3) then
   `rhsResult.assemblers_` second (via `mov 0x10(%rbp),%rax` =
   rhsResult at @0x22cffd, insert at @0x22d01b).
2. **Row 2 (Const/Cvar - Var)** added a spurious second merge of
   `lhsResult.assemblers_`. The binary's path A (entered at
   @0x22cf09, Const/Cvar-Var case) only does ONE insert at @0x22cf3a
   merging `rhsResult.assemblers_` — `lhsResult` is Const/Cvar so
   has no assemblers to merge.

The previous reconstruction had conflated the addresses for two
different code paths (path A = Row 2, path B = Row 3), causing both
rows to be miswritten.

### Verification

- `arith_chain_smaller_hdawg` and `arith_chain_smaller_shfsg` now
  pass byte-identical.
- Main: 1600/1600.  Stress: 430/444 (was 428/444 → +2).

### Symptom (historical)

A linear arithmetic chain with 8 temporaries (well below regalloc
pressure) compiled to byte-different `.text` and `.asm` between orig
and recon — same byte count, different bytes.  Visible diff:
sub-expression `(t3+t4) - (t5+t6)` had its two side IRs emitted in
swapped order.  Same on HDAWG and SHFSG (codegen pass, not
device-specific).

## IF-172  zeros(0) emits stray waveform table entry

**Status**: fixed (2026-05-07, phase 57.B) — added length==0 guard at
`wavetable_ir.cpp:792` (getJsonIndex filter); skips waveform-table
entries whose `signal.length() == 0`. `wave_zero_boundary_shfsg` now
passes; `wave_zero_boundary_hdawg` still fails on `.asm`/`.text`
(playback codegen for zero-length wave is a separate gap, similar to
IF-176 remaining playback gap).
**Severity**: bug (waveform table)
**Found**: stress phase 53 (`wave_zero_boundary.seqc`, `wave_min_length.seqc`)

### Symptom

`zeros(0)` causes recon to emit a `length=0` waveform entry in the
`.waveforms` JSON; orig drops it entirely (filtered out before the
table is written).  Affects `.waveforms` size, `.wavemem` accounting
(`fpgaMemoryUsed` differs), and `e_entry` placement on devices where
the resulting playZero/playWave changes instruction count.

### Minimal repro

```seqc
wave w_empty = zeros(0);
playWave(1, 2, w_empty);
waitWave();
```

orig `.waveforms`:  no entry for `__zeros_*` of length 0.
recon `.waveforms`: includes `{"name":"__zeros_3_1","function":"zeros(0)","length":"0",...}`.

Both `.wf___zeros_*` sections are written as 0-byte sections by both
sides — the divergence is only in the table.  Two stress files
demonstrate it: `wave_zero_boundary` (explicit `zeros(0)`) and
`wave_min_length` (where `zeros(0)` is a side-effect of declaring
`wave w_zero = zeros(0)`).

### Recommended next step

1. Find the waveform-table emit pass and locate the filter that drops
   length-0 entries.  Likely a missing `if (length > 0)` guard in
   recon's table emission.

## IF-173  chirp() sample numerics: extreme sweep produces 1-LSB off samples

**Status**: fixed (2026-05-07, phase 57.G.5) — recon's chirp loop computed
`theta = f0*di + phase + k*di*di`, which C++ evaluates as
`(f0*di + phase) + ((k*di)*di)`.  The binary at 0x251a76-0x251a7f instead
computes `(di*di)*k` first, then adds: `xmm0 = di*di; xmm0 *= k; xmm0 += xmm1`.
For large `di` and large `k` (e.g. `chirp(1024, 0.5, 1e3, 1e9)` where
k≈3e6), `(k*di)*di` and `(di*di)*k` round differently in the 53rd bit of
the mantissa, producing 1-LSB diffs at ~83 of 1024 samples.  Aligning the
multiplication order in `WaveformGenerator::chirp`
(`reconstructed/src/waveform/waveform_generator_dsp.cpp:835`) made all
chirp tests byte-identical.  Subsumes IF-170.
**Severity**: bug (waveform numerics)
**Found**: stress phase 53 (`chirp_sinc_extreme.seqc` HDAWG + SHFSG)

### Verification
- `chirp_sinc_extreme_{hdawg,shfsg}`: byte-identical.
- `chirp_amp_clip_{hdawg,shfsg}`: byte-identical.
- `wavemem_pressure_hdawg`: byte-identical (IF-170 subsumed).
- Main: 1600/1600.  Stress: 435/444 (was 430/444 → +5).

### Symptom

`chirp(1024, 0.5, 1e3, 1e9)` (6-decade frequency sweep) produces
1024 16-bit samples that differ between orig and recon at offset
0x192 (sample index 201) of the `.wf___chirp_*` section: orig=0x0d
recon=0x0c.  Same `.wf` size, just sample value off by 1 LSB.
Likely a cumulative phase / floating-point rounding difference in
the chirp generator.  Reproduces on both HDAWG and SHFSG (so it is
in the wave-builder, not device-specific codegen).

### Minimal repro

```seqc
wave w = chirp(1024, 0.5, 1e3, 1e9);
playWave(1, 2, w);
waitWave();
```

### Recommended next step

1. GDB-trace the chirp builder in orig at the same input; capture
   the exact float ops near sample index 201.
2. Compare with recon's chirp implementation; check for fma vs
   mul+add, accumulator vs per-sample formula, double vs float.

## IF-174  if(true)/if(false)/while(false)/while(true) not folded — emits dead branch

**Status**: fixed (passing as of phase 57.D verification)
**Severity**: bug (constant folding)
**Found**: stress phase 53 (`true_false_usage.seqc` HDAWG + SHFSG)

### Resolution

`true_false_usage_hdawg`, `true_false_usage_shfsg`, and
`while_true_loop_*` are byte-identical on the current main.  Fixed
implicitly via earlier 57.A–C work.  Phase 57.D added a defensive
correctness improvement to `oneStepJumpElimination` (scan past
non-matching LABELs to match binary @0x27e130) — no behavioral
change on the current test surface but eliminates a known control-
flow divergence.

### Symptom

Source containing constant-condition branches such as `if (true)`,
`if (false) { ... } else { ... }`, and `while (false) { ... }`
compiles to extra instructions in recon: `.text` is 168 bytes vs
orig's 164 bytes (4 bytes = 1 extra instruction); `.asm` is 816 vs
798 bytes; `.linenr` is 672 vs 656 bytes.  Pattern strongly suggests
orig folds the always-true / always-false branch at compile time and
elides the dead arm; recon emits both arms (or a runtime branch) for
at least one of these constructs.

### Minimal repro

`tests/cases/stress/true_false_usage.seqc`.

### Recommended next step

1. Compare `.asm` side-by-side to identify which of the four
   constructs (`if(true)`, `if(false){}`, `if(false){}else{}`,
   `while(false)`) is the divergent one.
2. Look at the AST-eval branch-handling code for handling of
   constant boolean conditions.

## IF-175  Unsubstituted boost::format placeholders in error messages

**Status**: fixed (2026-05-07, phase 57.A) — see IF-186 fix at awg_assembler_opcodes.cpp:333. Line 124 site deferred (no locals available; needs GDB to determine correct template id).
**Severity**: bug (user-facing — leaks `%1% %2% %3% %4%`)
**Found**: stress phase 53 (`long_source.seqc` HDAWG + SHFSG)

### Symptom

A 375-line source of trivial `setUserReg` / `playZero` calls (no
user error in the source itself) causes recon to emit:

```
Compilation failed: Assembler message at 1152 :
instruction %1% (opcode %2%) expects at least %3% argument(s),
%4% argument(s) given
```

The placeholders `%1%`, `%2%`, `%3%`, `%4%` are unsubstituted —
this is the raw template from `error_messages.cpp:139` (message id
4, `TooFewArguments`).  Orig succeeds with no error on the same
source, suggesting:

(a) recon has a real codegen bug that produces an instruction with
    too few arguments at instruction index ~1152, AND
(b) the error-emission path forgets to substitute the boost::format
    arguments.

Both are independent bugs; (b) is a serious user-facing regression
(every TooFewArguments error in the wild would print raw `%1%`).

### Minimal repro

`tests/cases/stress/long_source.seqc` (HDAWG and SHFSG both reproduce).

### Suggested investigation paths

1. Find the call sites that build the TooFewArguments message (likely
   in the assembler; grep for `ErrorMessages::get(4)`).  Check whether
   the result is fed through a boost::format `%` chain and `.str()`,
   or merely concatenated as the raw template string.
2. Run a binary diff on the same input under GDB to see what (if
   anything) the orig assembler emits at instruction 1152, and why
   recon thinks an arg is missing.
## IF-176  cut(w, N, N) length-1 case: orig drops .wf, recon emits 32 samples

**Status**: fixed (2026-05-07, phase 58.A) — added an early-return for
`startIdx == endIdx` in `WaveformGenerator::cut`
(`waveform_generator_dsp.cpp:1685`). Orig (0x259c69-0x259cf4) returns a
*fully zeroed* Signal in this case — channels_=0, length_=0, all vectors
empty (markerBits dropped). The non-reserveOnly path (0x259cd6) zeroes
the entire struct including channels_ at +0x48 via overlapping movups
writes; the reserveOnly path (0x259c7a) does the same but additionally
sets reserveOnly_=1 at +0x4A. Without channels_=0 the empty-cut Play
node carried channels_=1 (from input markerBits.size()=1), making all
plays look identical to `Prefetch::globalCwvf` → `globalCwvfValid_=true`
→ initial cwvf 0x4FFFC0 + transition cwvf 0x4FFF00 both suppressed. Now
recon emits both cwvf instructions and matches orig byte-for-byte. All
4 stress tests (`cut_extreme_{hdawg,shfsg}`, `cut_into_join_{hdawg,shfsg}`)
pass; main 1600/1600 maintained.

### Phase 57.B partial (now superseded)
`cut()` formula corrected in `waveform_generator_dsp.cpp:1645` so
`from == to` produces `cutLen = 0`. With the companion IF-172 fix in
`wavetable_ir.cpp`, `.wf___cut_*` and `.waveforms` became byte-identical
for `cut_extreme`/`cut_into_join`. Remaining diff in playback codegen
was traced to channels_ propagation (this fix).
**Severity**: bug (waveform table + .wf emission)
**Found**: stress phase 54 (`cut_extreme.seqc` HDAWG + SHFSG)

### Symptom

`cut(w, 0, 0)` and `cut(w, 255, 255)` (start == end, intended
length-1 cuts) cause a divergence:

  orig: `.wf___cut_*` is **0 bytes** (sample data dropped)
        `.waveforms` table still lists entry with length="32"
  recon: `.wf___cut_*` is **64 bytes (32 samples)**
         `.waveforms` table also lists entry with length="32"

So orig and recon agree about the table entry, but disagree about
whether to emit any sample data for it.  Affects `e_entry`, `.text`,
`.asm`, `.linenr` (extra/different instructions to play it).
Distinct from IF-172 — that was about `zeros(0)` (length 0 in both
table and `.wf`); this is about cut() collapsing to length 1 / 0
samples.

### Minimal repro

```seqc
wave w = ones(256);
wave c = cut(w, 0, 0);
playWave(1, 2, c);
waitWave();
```

### Recommended next step

1. GDB orig at the cut() builder; see what happens to length when
   `start == end`.  Is the length 1 or 0? Is the sample data
   genuinely empty or is it dropped at a later filter?
2. Find the recon cut() implementation; align with whichever
   semantics orig uses.

## IF-177  ones(var) / setUserReg(int, var) error wording diverges

**Status**: fixed (2026-05-07, phase 57.B) — added
`ValueType::Unspecified` check at the start of
`WaveformGenerator::readInt` (`waveform_generator.cpp:485`) before the
`val.toInt()` call. Now throws `WaveformGeneratorValueException` with
template id 103 (`CantCallWithVar` → "<func> can't be called with var
arguments") instead of leaking the generic "unspecified value type
detected in toInt conversion" exception. `int_float_coercion_*` and
`wave_computed_length_*` stress tests now pass.
**Severity**: bug (user-facing — wrong error message)
**Found**: stress phase 54 (`int_float_coercion.seqc`,
           `wave_computed_length.seqc` HDAWG + SHFSG)

### Symptom

When a user passes a `var` (runtime variable) where a `const` is
required (e.g. `wave w = ones(x);` where `x` is a `var`), orig
reports the actual semantic error:

```
Compiler Error (line: 4): ones can't be called with var arguments
```

Recon instead reports a generic / internal error:

```
Compiler Error (line: 4): unspecified value type detected in toInt conversion
```

Both reject the input — but recon's message is wrong (talks about
"toInt conversion", not "var arguments").  Reproduces in two stress
files independently (`int_float_coercion`, `wave_computed_length`).

### Minimal repro

```seqc
var x = 5;
wave w = ones(x);
playWave(1, 2, w);
```

### Recommended next step

1. Find the recon code that calls toInt on a `var`-typed expression
   in the wave-builtin path; the check for "is this var?  bail with
   semantic message" is missing or misordered.

## IF-178  boost::too_many_args exception leaks in error path

**Status**: fixed (2026-05-07, phase 57.A) — superseded by IF-184 fixes (18 SetXxxArgs sites + IF-182 16 awg_assembler sites).
**Severity**: bug (user-facing — wrong error message; sister of IF-175)
**Found**: stress phase 54 (`setuserreg_oor.seqc`,
           `giant_expression.seqc` HDAWG + SHFSG)

### Symptom

For inputs that should produce a normal `setUserReg`-related
diagnostic (out-of-range register, arity mismatch), recon throws
out a raw boost exception message:

```
Compilation failed: boost::too_many_args: format-string referred to
fewer arguments than were passed
```

Orig reports the proper messages:

```
Compilation failed: Compiler Error (line: 3): setUserReg register
must be in the range of 0 to 15
```

```
Compilation failed: Compiler Error (line: 3): setUserReg expects
exactly two arguments
```

Same family as IF-175 (boost::format misuse) but a different failure
mode: IF-175 leaks **unsubstituted placeholders** (`%1% %2%`); this
one throws **too_many_args** because the format string has fewer
`%N%` slots than the caller is feeding it.  Implies recon's
ErrorMessages::get(...) call sites do not always match the format
template's parameter count.

### Minimal repro

```seqc
setUserReg(-1, 5);
```
or
```seqc
setUserReg(0, 1, 2);
```

### Recommended next step

1. Grep all `ErrorMessages::get(N)` sites alongside their `% a % b`
   chains; verify the param counts match what the template at
   `error_messages.cpp` line ~139, 196, 224 etc. declares.
2. Likely a single misuse pattern (call site adds an extra `% x`).

## IF-179  boost::too_few_args exception leaks from marker(len, bits>=4) warning path

**Status**: fixed (2026-05-07, phase 57.A) — added missing `funcName` 3rd arg at waveform_generator.cpp:613 to ValueCapped format call.
**Severity**: bug (user-facing — recon hard-errors where orig succeeds with a warning)
**Found**: stress phase 55 (`marker_bits_zoo.seqc` HDAWG)

### Symptom

For `marker(64, 4)` / `marker(64, 7)` (any markerValue >= 4),
orig accepts the input, emits a warning, and continues compilation.
Recon throws a hard compilation error:

```
Compilation failed: Compiler Error (line: 7): boost::too_few_args:
format-string referred to more arguments than were passed
Compiler Error (line: 8): boost::too_few_args: format-string ...
```

The opposite direction of IF-178 (which is `too_many_args`).  Same
underlying disease as IF-175 / IF-178: ErrorMessages format-template
arity does not match the call site's argument count.

### Root cause

`reconstructed/src/waveform/waveform_generator.cpp:613` —
`markerImpl()` warning path:

```cpp
std::string msg = ErrorMessages::format(
    ValueCapped,
    markerValue, markerValue & 3);     // 2 args
```

But the format template (`error_messages.cpp:234`,
`m[99] = ValueCapped`) has **three** placeholders:

```
"%3% value %1% is larger than the maximum possible value 3, will be capped to %2%"
```

`%3%` is the function name ("marker" / "mask").  Call site is missing
the third argument, so `boost::format` raises `too_few_args`.  Because
this happens inside the warning-callback path, the exception is not
caught locally and propagates as a fatal compilation error — flipping
"warn and continue" into "hard error".

### Minimal repro

```seqc
wave w = marker(64, 4);
playWave(1, 2, w);
waitWave();
```

### Recommended next step

1. Add `funcName` as the third argument to the `format(ValueCapped, ...)`
   call at `waveform_generator.cpp:613`.
2. Grep for other `ErrorMessages::format(ValueCapped, ...)` call sites
   (e.g. cap-value diagnostics in other builtins) and audit the same
   way.
3. Once fixed, `marker_bits_zoo_hdawg` should pass with a warning-diff
   (or byte-identical if warnings aren't compared); add a follow-up
   stress case for `mask(N, M>=4)` to cover the `isMask=true` branch.

### Phase 56 update — additional triggers

`marker_bits_extreme.seqc`, `mask_bits_zoo.seqc` (HDAWG) reproduce
the same `boost::too_few_args` for both the `marker()` and `mask()`
sides of `markerImpl()`.  Confirms IF-179 covers both branches via
the shared `format(ValueCapped, ...)` call site.

Additionally, `shfqc_qa_repeat_startqa_var_init.seqc` (SHFQC qa)
shows a **third** call site exhibiting the same family bug:
`startQA` with var argument — orig issues
`function 'startQA' expects (const) as argument`, recon throws
`boost::too_few_args`.  Likely a different format template with
similar arity mismatch.  Audit must extend beyond `markerImpl()`.

## IF-180  cut(w, from, to) accepts out-of-range from when from > length

**Status**: fixed (2026-05-07, phase 57.B) — added two bounds checks at
the start of `WaveformGenerator::cut` (`waveform_generator_dsp.cpp:1645`)
and corrected the param-name strings from `"2 (offset)"`/`"3 (length)"`
to `"2 (from)"`/`"3 (to)"` (verified via `strings _seqc_compiler.so`).
Both `from >= length(w)` and `to >= length(w)` now throw
`ArgGreaterThanLength` (template id 88). Verified via probes: orig
errors on `from == length` and `to == length`, but allows `from > to`.
`cut_chain_one_hdawg`/`cut_chain_one_shfsg` now pass.
**Severity**: bug (validation gap — silent acceptance of invalid input)
**Found**: stress phase 56 (`cut_chain_one.seqc` HDAWG + SHFSG)

### Symptom

`cut(w, from, to)` with `from >= length(w)` should be rejected.
Orig validates and errors:

```
Compiler Error (line: N): argument 2 (from) of cut is greater
than the waveform length
```

Recon silently accepts the call and produces (apparently empty
or garbage) output, then carries on.  Direction reversed from
IF-176: IF-176 was about content emission; IF-180 is about
**missing input validation**.

### Minimal repro

```seqc
wave w = gauss(128, 0.5, 32);
wave w2 = cut(w, 0, 0);     // length-1 cut
wave w3 = cut(w2, 5, 5);    // from=5 into a length-1 wave -> orig errors
playWave(1, 2, w3);
waitWave();
```

### Recommended next step

1. Locate recon's `cut()` implementation (likely
   `waveform_generator.cpp` or a sibling).
2. Add the bounds check: `if (from >= length) throw …` matching
   orig's error message.
3. Cross-check the `to` bound (`to >= length`) and `from > to`
   ordering — likely the same code path is missing all three
   checks.

## IF-181  placeholder() inside join() crashes recon worker (vector OOB)

**Status**: fixed (2026-05-07, phase 57.C.1)
**Severity**: bug (worker SIGABRT — orig succeeds, recon dies)
**Found**: stress phase 56 (`placeholder_in_join.seqc` HDAWG)

### Symptom

`assignWaveIndex(1, 2, join(placeholder(N), realWave), 0)` —
recon worker aborts with libstdc++ debug assertion:

```
worker exit -6:
/usr/include/c++/16.1.1/bits/stl_vector.h:1253:
std::vector<_Tp, _Alloc>::reference std::vector<_Tp, _Alloc>::operator[]
... Assertion '__n < this->size()' failed.
```

Orig handles the same construct without error.

### Minimal repro

```seqc
wave a = ones(64);
wave p = placeholder(64);
wave j = join(p, a);
assignWaveIndex(1, 2, j, 0);
executeTableEntry(0);
waitWave();
```

### Root cause (verified by GDB on orig)

Two cooperating bugs in the placeholder/join pipeline:

1. **`WaveformGenerator::placeholder` did not always emit a markerBits
   byte.**  When neither marker0 nor marker1 was passed, recon left
   `markerBits_` empty.  The original binary @0x255be6-0x255c01
   *unconditionally* `operator new[1]`s a single byte and writes the
   OR'd marker bits (which is 0 when no markers) — so the orig's
   `placeholder(64)` Signal has `markerBits_.size() == 1`, not 0.
   GDB confirmation on `placeholder_in_join.seqc`: every placeholder
   passed into `Signal::append` had `markerBits_` of byte-size 1.

   Without this byte, `Signal::append` (which iterates
   `this->markerBits_.size()` and OR-indexes `other.markerBits_[i]`)
   read past the end of the empty vector → libstdc++ debug assert
   under `_GLIBCXX_ASSERTIONS`.  Orig's libc++ doesn't bounds-check,
   but the construct is still UB; supplying the byte makes it well
   defined and matches orig byte-for-byte.

2. **`WaveformGenerator::join` short-circuited when `first.reserveOnly_`
   was true** — returning a fresh reserve-only Signal of total length
   without ever materializing or appending the rest of the operands.
   Orig has no such short-circuit (verified via static disassembly of
   0x255da0): even if the first operand is a placeholder, orig falls
   into the regular materialization path, where `Signal::append`
   calls `checkAllocation()` on each operand to zero-fill the
   placeholder samples and concatenates everything into a concrete
   sample buffer.  After fix #1 alone, the test still failed because
   recon emitted `[ones, zeros]` for `join(p, a)` instead of
   `[zeros, ones]`.

### Fix

`reconstructed/src/waveform/waveform_generator_dsp.cpp`:

- `placeholder()`: drop the `if (bits != 0)` guard around
  `markerBits.push_back(bits)` — always push exactly one byte.
- `join()`: remove the `if (first.reserveOnly_) { ... return ...; }`
  short-circuit; let placeholder operands flow through
  `checkAllocation()` + `Signal::append` like every other Signal.

### Test result

`placeholder_in_join_hdawg` is now byte-identical
(orig=2776, recon=2776).  Main suite stays at 1600/1600.  Stress
suite: 407 → 408 (+1).

### Follow-up probes (added in same commit)

`stress/placeholder_arith.seqc` (placeholder + ones), `cut_placeholder.seqc`,
and `scalar_mul_placeholder.seqc` were added to surface related
placeholder-in-arithmetic bugs.  `cut_placeholder` and
`scalar_mul_placeholder` pass byte-identical; `placeholder_arith`
exposes a new bug filed as **IF-188**.


## IF-182  ErrorMessages template m[1]/m[2] arity mismatches across awg_assembler

Source: Phase 57.A.1 audit (`error_message_audit.md`).

**Symptom**: every assembler register/value error throws
`boost::io::too_many_args` instead of producing the intended diagnostic.

**Root cause**: `error_messages.cpp` templates are correctly reconstructed
(matches binary rodata at offsets ~80746):

```
m[1] = "opcode %1% register %1% not given"   (1 slot — uses %1% twice)
m[2] = "opcode %1% value %1% not given"      (1 slot — uses %1% twice)
```

But 16 call sites in `src/codegen/awg_assembler_opcodes.cpp` (lines 231,
241, 274, 283, 292, 301, 367, 379, 390, 404, 415, 430, 467, 514, 524, 578)
pass **2 args** (opcode, position) → `too_many_args`.

**Fix**: drop the second arg at every call site. The template intentionally
substitutes the same value (the missing register/value identifier) twice.
Verified against binary rodata — templates are correct, calls are wrong.

**Severity**: HIGH — every assembler register/value error throws boost
exception instead of diagnostic.

**Affected stress tests**: any test that triggers a missing-register or
missing-value assembler diagnostic.

**Status**: fixed (2026-05-07, phase 57.A) — dropped extra `position` arg at all 16 sites in awg_assembler_opcodes.cpp.


## IF-183  ErrorMessages m[32] wrong message-id at asm_commands.cpp:452,458

Source: Phase 57.A.1 audit.

**Symptom**: `toInt32()` overflow path produces nonsensical "tried to modify
const value" + boost `too_many_args` exception instead of "value X is out
of range for 32 bits".

**Root cause**: call passes id `0x20` (32 = `ConditionalNeedVarConst`,
template "tried to modify const value", 0 slots) with 2 args (double, 32).
The intended id is **5** (`ValueOutOfRange`, template "value %1% is out
of range for %2% bits") — confirmed against binary rodata at offset
80750. Likely `0x20` vs `0x05` transcription error in recon.

**Fix**: change `format(ErrorMessageT(0x20), ...)` → `format(ErrorMessages::ValueOutOfRange, ...)` (id 5) at both lines.

**Severity**: HIGH.

**Status**: fixed (2026-05-07, phase 57.A) — replaced `ErrorMessageT(0x20)` with `ErrorMessageT::ValueOutOfRange` at asm_commands.cpp:452 and :458.


## IF-184  ErrorMessages SetXxxArgs family — call sites pass extra funcName arg

Source: Phase 57.A.1 audit. Subsumes IF-178 root-cause analysis.

**Symptom**: `setRate`, `setUserReg`, `setInt`, `setDouble`,
`setTrigger`, `setInternalTrigger`, `setPrecomp*`, `lock`, `unlock` —
when called with bad arg patterns, recon throws `boost::io::too_many_args`
exception instead of producing the intended diagnostic.

**Root cause**: 18 call sites across `custom_functions_registers.cpp`
and `custom_functions_playback.cpp` pass `std::string("setXxx")` as a
funcName arg to templates that have **0 slots**. Binary rodata
(strings 80926–80962) confirms these messages are verbatim with no
`%1%` placeholder:

```
setRate expects a const argument
setRate expects just one const argument
setUserReg expects a const as first argument
setUserReg register must be in the range of 0 to 15
setUserReg expects exactly two arguments
setUserReg expects a var or const as second argument
setInt expects 2 arguments
setInt expects a string as first argument, %1% given      ← (this one DOES have a slot)
setInt expects a var or const as second argument, %1% given
setDouble expects 2 or 3 arguments
setDouble expects a string as first argument, %1% given
setDouble expects a var or const as second argument, %1% given
setDouble expects a const as third argument, %1% given
setTrigger expects a single const or var argument
setInternalTrigger expects a single const or var argument
lock expects exactly one argument
unlock expects exactly one argument
```

(Some of the above DO have a `%1%` for the actual-given-type — those
templates are different ids and not in the IF-184 batch.)

**Fix**: drop the `std::string("setXxx")` funcName arg from the 18
overflow call sites. **Do not add slots to templates** — that would
produce different output than the binary.

**Affected stress tests**: `setuserreg_oor`, `giant_expression`,
others that hit the SetXxxArgs paths.

**Status**: fixed (2026-05-07, phase 57.A) — dropped `std::string("setXxx")` arg at all 18 sites in custom_functions_registers.cpp and custom_functions_playback.cpp (:914, :920).


## IF-185  ErrorMessages NodePrecisionLoss underflow (3 sites)

Source: Phase 57.A.1 audit.

**Symptom**: any non-integer setInt warning path
(`custom_functions_play.cpp:1617`, `:1646`, `:2069`) throws
`boost::io::too_few_args`.

**Root cause**: template m[128] = `"node '%1%' requires an %2% value,
therefore the double precision is lost"` has 2 slots; call passes 1
(`valStr`). Missing arg is the **node-name string**.

**Fix**: add the node-name as 2nd arg to `format(NodePrecisionLoss, ...)`
at all 3 sites.

**Severity**: HIGH — affects any non-integer setInt warning.

**Status**: fixed (2026-05-07, phase 57.A) — added `"integer"` 2nd arg at custom_functions_play.cpp:1618, :1647, :2070 (template m[128] uses %2% for the type-name hint, not node-name).


## IF-186  ErrorMessages get(4)/get(222) placeholder leaks — superset of IF-175

Source: Phase 57.A.1 audit.

**Symptom**: raw `%1% %2% %3% %4%` substring appears in user-facing
error message.

**Root cause**: 3 call sites use `ErrorMessages::get(N)` instead of
`ErrorMessages::format(N, ...)` against multi-slot templates, so
placeholders are never substituted:

- `src/codegen/awg_assembler_opcodes.cpp:124` — `get(4)`, m[4]
  TooFewArguments has 4 slots.
- `src/codegen/awg_assembler_opcodes.cpp:333` — same.
- `src/runtime/custom_functions_wait.cpp:761` — `get(222)`, m[222]
  NotSupportedGrouping has 2 slots; affects waitSineOscPhase
  channel-grouping diagnostic.

**Fix**: change each `get(N)` to `format(N, ...args)` with the
correct args. For m[4]: `(instr, opcode, expected, given)`. For m[222]:
`("waitSineOscPhase", numChannelGroups)`.

This subsumes IF-175 and adds the m[222] case.

**Status**: fixed (2026-05-07, phase 57.A + 58.B) — fixed awg_assembler_opcodes.cpp:333 (replaced `get(4)` with `format(TooFewArguments, cmdName, 3, 2, nChildren)`) and custom_functions_wait.cpp:761 (replaced `get(NotSupportedGrouping)` with `format(NotSupportedGrouping, "waitSineOscPhase", numChannelGroups)`). **Phase 58.B** fixed the deferred awg_assembler_opcodes.cpp:124 site: replaced `get(4)` with `get(3)` (RegisterNotExist, 0 slots). Static disassembly of orig 0x2892b0..0x289367 was unambiguous — both the `regNum<0` branch (js at 0x2892c0) and the `regNum>=registerDepth` branch (fall-through past 0x2892cc ja) land at the same code block at 0x2892d2 which performs `std::map::find` with key=3 (the `cmp $0x4 jge` / `cmp $0x3 je` traversal pattern is RB-tree find). The previous `get(4)` was emitting raw `%1%..%4%` placeholders. Verified: stress `long_source_hdawg/shfsg` no longer leaks the template; they now report "register does not exist" matching the orig diagnostic. The tests still fail because recon hits this error path while orig succeeds — that is a separate downstream bug (recon allocates a register index >= registerDepth where orig does not), tracked outside IF-186.


## IF-187  ErrorMessages assorted underflows in custom_functions_play/playback/registers

Source: Phase 57.A.1 audit. The non-IF-184/185/186 underflows.

The remaining 11 underflow sites surfaced by the audit (not already
covered by IF-179/184/185):

| File:Line | ID | Slots | Args | Missing arg(s) |
|---|---|---|---|---|
| custom_functions_play.cpp:496 | 215 IndexMustBe | 2 | 1 | the index range string |
| custom_functions_play.cpp:2274 | 136 FuncSingleArg | 1 | 0 | funcName |
| custom_functions_play.cpp:2312 | 70 FuncInvalidArgType | 4 | 1 | funcName, argName, expectedType |
| custom_functions_play.cpp:2324 | 166 FormatMoreArgs | 1 | 0 | funcName |
| custom_functions_play.cpp:2328 | 168 FormatCantInterpret | 1 | 0 | funcName |
| custom_functions_playback.cpp:861 | 169 FormatFuncArgs | 3 | 1 | expected & given counts |
| custom_functions_registers.cpp:815,875,1147 | 69 FuncExpectsMaxArgs | 3 | 1 | max-args, given-args |
| custom_functions_registers.cpp:828,836,884,1153,1175,1190,1210,1219 | 62 FuncExpectsConst | 1 | 0 | funcName |
| custom_functions_registers.cpp:921 | 61 FuncMinArgs | 3 | 1 | min-args, given-args |

All produce `boost::io::too_few_args` at runtime instead of the proper
diagnostic.

**Fix**: at each call site, supply the missing arg(s). Most just need
the funcName from the caller's context (already known locally as a
`std::string`).

**Status**: fixed (2026-05-07, phase 57.A) — supplied missing args at all 14 sites:
- play.cpp:496 (IndexMustBe + "3 or larger"), :2274 (FuncSingleArg + funcName), :2312 (FuncInvalidArgType + funcName/i/expected), :2324 (FormatMoreArgs + funcName), :2328 (FormatCantInterpret + funcName).
- playback.cpp:861 (FormatFuncArgs + 0/given).
- registers.cpp:815/875/1147 (FuncExpectsMaxArgs + max/given), :921 (FuncMinArgs + 1/0), :828/836/884/1153/1175/1190/1210/1219 (FuncExpectsConst + funcName).


## IF-188  placeholder() + concrete wave produces wrong section type

**Status**: FIXED in phase 58.C (SA-58C)
**Severity**: bug (byte-mismatch — orig succeeds, recon emits diff bytes)
**Found**: phase 57.C.1 follow-up probe (`placeholder_arith.seqc`)

### Resolution

Removed bogus early `if (first.reserveOnly_) return first;`
short-circuit at the top of `WaveformGenerator::add` in
`waveform_generator_dsp.cpp`. The previous comment claimed it
"matches the disassembly's reserveOnly_ short-circuit" — but
disassembly review of `add` at 0x256ff0 shows **no** early
reserveOnly check. The `testb $0x1, -0x60(%rbp)` near the top
(0x257103) is an SSO-bit test on a libc++ string allocated by
`Value::toString()` (the parameter-name string), not a
reserveOnly_ test. The actual reserveOnly_ check (offset 0xca on
Signal) lives at 0x257663 inside the per-operand merge loop and
simply triggers checkAllocation-style zero-fill (matches recon's
`first.checkAllocation()` pre-loop and per-operand
`s.checkAllocation()` calls).

After removing the short-circuit, `placeholder(64) + ones(64)`
correctly materializes: placeholder is zero-filled by
checkAllocation, ones contributes 1.0 per sample, the result
Signal is constructed from the samples-vector ctor (which sets
reserveOnly_=false), so .wf___add_2_3 emits PROGBITS with full
sample data matching orig (32767/0x7fff per sample).

### Verification

- `stress:placeholder_arith_hdawg` PASS (was FAIL).
- Stress suite: 440/444 (was 439/444, +1).
- Main suite: 1600/1600 (no regression).
- `scale` (operator*-by-scalar) keeps its early reserveOnly
  short-circuit because `scalar_mul_placeholder` (the only
  scalar-only operand case) still passes — reserveOnly
  propagation is correct there. If a future probe finds a
  `placeholder * concrete-wave` case that breaks, audit `scale`
  similarly.

### Sibling audit
- `subtract`: not present as a separate method in DSP source
  (no caller seems to need it). No-op.
- `multiply` (0x258750): inspected briefly — uses an
  `allReserveOnly` accumulator and pads via inlined
  `checkAllocation`, no top-level short-circuit. Likely correct
  for mixed cases; left untouched.
- `join` (0x255da0): IF-181 already removed its short-circuit.
- Fix area: `add` only.

### Symptom

```seqc
wave w = placeholder(64) + ones(64);
playWave(w);
```

Differential test `placeholder_arith_hdawg`: section
`.wf___add_2_3` differs in **type** (orig=1 i.e. SHT_PROGBITS,
recon=8 i.e. SHT_NOBITS).  Sample data length matches but recon
emits a NOBITS section (deferred allocation) where orig emits a
real PROGBITS payload.

### Probable root cause (superseded — see Resolution above)

After IF-181 was fixed, `join()` no longer short-circuits on
reserveOnly operands.  The same short-circuit pattern likely exists
in the binary-arithmetic operators (`Signal operator+`, `operator*`,
etc.) — they probably propagate `reserveOnly_=true` when one operand
is reserveOnly, instead of materializing.  Orig presumably forces
materialization once a non-reserveOnly operand participates.


## IF-189  fpgaMemoryUsed counts zero-length waves

Source: Phase 58 triage of stress test `wave_min_length_hdawg`.

**Symptom**: `.wavemem` JSON differs:
- orig: `{"exceedsFpgaMemory":false,"fpgaMemoryUsed":1.220703125E-4}`
- recon: `{"exceedsFpgaMemory":false,"fpgaMemoryUsed":1.8310546875E-4}`

Ratio: recon = orig × 1.5. Test compiles 3 waves: `ones(32)`,
`ones(16)`, `zeros(0)`. After IF-172 fix, the `zeros(0)` waveform
table entry is suppressed and `.wf___zeros_12_5` is 0 bytes — but
recon's fpga-memory accounting still adds the 16-sample budget for
it, inflating the total by 50%.

Note also `e_entry: orig=0x80000080  recon=0x800000c0` despite
`.text` being byte-identical. Two possible explanations:
1. Recon writes `e_entry` based on a stale instruction-count
   counter that wasn't decremented when IF-172 dropped the entry.
2. `.text` is identical but recon's section-header offset for
   `.text` differs.

Both diffs are likely the same "bookkeeping not updated when IF-172
filtered the zero-length wave" bug.

**Fix area**: the wavemem-budget computation pass + entry-point
calculation. Likely both call into the wave-table iterator and
count entries unconditionally.

**Status**: fixed (phase 58.D). Root cause was actually upstream of both
budget and entry-point computation: `WavetableIR::assignWaveformAllocationSizes`
(`reconstructed/src/waveform/wavetable_ir.cpp:852`) did not guard against
`signal.length() == 0`. With sampleCount=0, `aligned` started at 0 then
the `max(aligned, maxWaveformLength)` clamp inflated it to a full
waveform block, giving `zeros(0)` a non-zero `allocationByteSize`. That
bogus size both:
  1. flowed into the fpgaMemoryUsed accumulator in
     `awg_compiler.cpp:304` (whose own `allocationByteSize == 0` filter
     was therefore not enough to skip the zero wave), and
  2. shifted addresses computed by the Phase 1 lambda in
     `allocateWaveforms`, which in turn shifted the wave-table layout
     used by the prefetch pass — explaining IF-190 as the same bug.

Fix: add `if (sampleCount == 0) aligned = 0; else { …existing… }` so the
allocation byte size is 0 when the raw signal is empty.

**Verification**:
- `wave_min_length_hdawg`: byte-identical (2592 bytes), e_entry now 0x80.
- `wave_zero_boundary_hdawg`: byte-identical (2944 bytes), `.text` and
  `.asm` match orig (prf instruction + high-address slot at 524288 now
  emit correctly).
- Main suite 1600/1600.
- Stress: 435 → 442 (cleared IF-189, IF-190; +5 collateral
  passes from corrected wave-table layout in tests that touched
  zero-length waves).
- Confirms IFs 189 and 190 share a single root cause.


## IF-190  zeros() waveform not prefetched at high address; sequential register offsets emitted

Source: Phase 58 triage of stress test `wave_zero_boundary_hdawg`.

**Symptom**: with multiple waves where one is `zeros(0)`, orig
emits a special prefetch sequence:

```
cwvf 5242625
addiu R1, R0, 524288       ← zeros high-addr load
prf R1, R0, 4096            ← prefetch the zero region
addi R2, R0, 64             ← w_one address (offset 64 from base)
addiu R2, R2, 524288
addi R3, R0, 128            ← w_two address
addiu R3, R3, 524288
```

Recon emits sequential offsets only:
```
cwvf 5242625
addi R1, R0, 64             ← skips the zeros high-addr + prefetch
addiu R1, R1, 524288
addi R2, R0, 128
addiu R2, R2, 524288
addi R3, R0, 192            ← shifted by 64 because R1/R2/R3 use sequential offsets
addiu R3, R3, 524288
```

So recon (a) drops the `prf` instruction, (b) doesn't allocate
the high-address (524288 = 0x80000) zeros pool, and (c) uses
register offsets 64/128/192 instead of orig's 0(zeros)/64/128.

This affects `e_entry` (0xc0 vs 0x100) and the first 4 instructions
of `.text`.

Likely the same area as IFs 158/172/170 — the prefetch pass +
zero-wave handling. The zero-wave gets a special fixed address
slot and must be prefetched separately.

**Status**: fixed (phase 58.D). Same root cause as IF-189: see IF-189
resolution. The "drops the prf instruction / wrong register offsets"
symptom was a downstream consequence of `assignWaveformAllocationSizes`
giving `zeros(0)` a non-zero allocation. With the zero wave correctly
sized to 0, the wave-table layout, prefetch register slots, and
`prf` emission all match orig byte-for-byte.


## IF-191  long_source: regalloc allocates beyond registerDepth=16 on very large input

Source: Phase 58.B post-mortem of IF-186 fix.

**Symptom**: `long_source_{hdawg,shfsg}` stress test errors with
`Assembler message at 1152 : register does not exist` (and similar
at 1153). Orig compiles successfully. After IF-186 fix at
awg_assembler_opcodes.cpp:128 (changed `get(4)` → `get(3)`), the
diagnostic message text is now correct but the underlying error is
real: recon's register allocator emits an out-of-range register
reference at instruction index ~1152.

**Root cause** (Phase 58.E): NOT the regalloc algorithm — the bug
was in `Assembler::highestRegisterNumber()` at
`reconstructed/src/asm/assembler.cpp:301`.  The reconstruction had
`return (1LL << 32) | static_cast<uint8_t>(maxReg);` — truncating
the max register number to 8 bits.  The orig binary at 0x2901b3..
0x2901db computes `ebx = (eax & 0xffffff00) | (eax & 0xff)` which
is the FULL int value (a value-preserving identity); the recon
collapsed that to a uint8_t cast.

Once `GlobalResources::regNumber` crossed 256 within a single
optimization pass (which happens ~exactly at the 128th `setUserReg`
call: 128 × 2 vregs/call ≈ 256), `highestRegisterNumber` would
report register 256 as 0, 257 as 1, etc.  This in turn made
`AsmList::maxRegister()` and `AsmOptimize::removeUnusedRegs()`
under-report `numRegs` to `registerAllocation`, so the live-range
table was sized too small and the high vregs were never visited.
The asm list emerged from optimization with R256+ references
intact, and the assembler emit pass (`getReg`) then complained
"register does not exist".

**Fix**: change the cast to `static_cast<uint32_t>(maxReg)`.

**Verification**: minimal repro is 128+ `setUserReg` calls in
sequence; bisect showed exactly 127 calls = OK, 128 = fail.
Debug printf in step 19 of `compile()` confirmed
`asmList_.maxRegister() = 1` (truncated) while a manual scan of
`regSrc/regDst/regAux` found R256 at instruction 1153.  After
fix, both `long_source_{hdawg,shfsg}` pass byte-identical;
main suite 1600/1600, stress 444/444.

**Status**: FIXED in Phase 58.E.

---

## IF-192  UHFQA missing 1024-instruction program-size limit

**Source**: Phase 59 round 10 — `regalloc_2x_long_uhfqa`,
`regalloc_pressure_subroutines_uhfqa`
**Status**: **FIXED** in Phase 60 — `awg_compiler.cpp:474..495`
check 10 swapped to `waveformMemSize` (DC+0x58), removed bogus
`/4`, set `lineNr=-1`, fixed exception string to match binary.
**Severity**: likely-bug

The original UHFQA compiler enforces a hard limit of **1024
instructions** per sequencer program and rejects with:

```
Compilation failed: Compiler Error: program is too large to fit
into memory - has <N> instructions, maximum is 1024
```

Recon **lacks this check entirely** for UHFQA: large programs that
exceed 1024 instructions compile silently and produce an oversized
ELF.

Failing cases (round 10, expanded device coverage):
- `regalloc_2x_long_uhfqa` — orig: 2106 instructions, errored;
  recon: succeeded
- `regalloc_pressure_subroutines_uhfqa` — orig: 1798 instructions,
  errored; recon: succeeded

Same `.seqc` files compile fine on HDAWG/SHFSG/SHFQA (they have
no such limit, or a much higher one — to be verified).

**Hypothesis**: There is a per-device instruction-count check
somewhere late in the compile pipeline (probably in the
finalize/assembler stage). The UHFQA arm of that check is missing
or its threshold constant is wrong (set to 0 or never reached).

**Investigation steps**:
1. `strings _seqc_compiler.so | grep -F 'too large to fit'` to
   locate the format string and xref to find the check site.
2. Check whether `Assembler::finalize()` (or equivalent) has a
   device-keyed switch with a missing UHFQA branch.
3. Compare with SHFQA/HDAWG limits (they likely have the same
   pattern but different constants).

**Verification**: minimal repro is `regalloc_2x_long.seqc` (300
setUserReg pairs producing ~2100 instructions on UHFQA).

**Root cause (Phase 60)**: not "missing check" as initially
hypothesized — the check **was** present at
`awg_compiler.cpp:474-487`, but suffered from **two compounding
bugs**:

1. Wrong DC field: read `maxSequenceLen` (DC+0x60, value 16000)
   instead of `waveformMemSize` (DC+0x58, value 1024 for UHFQA).
2. Bogus `/4` divisor: `getOpcode()` already returns
   `vector<uint32_t>`, so `.size()` is the instruction count.
   The `/4` made the threshold 4× larger.

Combined, the effective UHFQA limit was 64000 (vs. true 1024),
so 2106-instruction programs sailed through. Both bugs had to be
fixed; neither alone would have made the test pass.

The misnamed `waveformMemSize` field (see IF-195) is what made
the original recon attempt pick the wrong field. A parallel bug
likely exists in check 11 (msg 0xF1) — see IF-196.

---

## IF-193  cut(w, N, N) length-1 result chained into another cut errors

**Source**: Phase 59 round 10 — `cut_zero_chain` (passed via
both-error, but error reason was suspect)
**Status**: dismissed (2026-05-07) — verified consistent with binary
via static disassembly analysis. No code change needed.
**Severity**: cosmetic (semantic edge case, both compilers agree)

Test case `cut_zero_chain.seqc`:
```
wave w = ones(64);
wave a = cut(w, 5, 5);    // length-1 wave (post-IF-176 fix)
wave b = cut(a, 0, 0);    // length-1 of length-1
playWave(b);
```

Both orig and recon error on line 4 with "argument 2 (from) of cut
is greater than the waveform length".

### Resolution

Explanation 1 from the original note is correct, and the binary
does exactly the same thing the recon does. Verified via
disassembly of `WaveformGenerator::cut` (0x2598d0):

1. **Bound check site** at 0x259c49–0x259c63:
   ```
   259c49: mov  -0xf0(%rbp),%r13          ; r13 = WaveformFront*
   259c50: mov  0xd0(%r13),%eax           ; eax = signal.length_ (WF +0x80 + Sig +0x50)
   259c57: cmp  %eax,%r14d                ; cmp from, length
   259c5a: jge  25a0cb                    ; from >= length → throw template 88
   259c60: cmp  %eax,%r12d                ; cmp to, length
   259c63: jae  25a129                    ; to >= length  → throw template 88
   ```
   Both checks are `>=` (signed `jge` for `from`, unsigned `jae` for
   `to`), exactly matching recon `waveform_generator_dsp.cpp:1666–1680`.

2. **Length value for `a`**: orig's IF-176 path (0x259cd6–0x259cf4)
   zeroes the entire Signal struct including `length_` at +0x50.
   So `a`'s length is 0 in the binary, identical to recon (which
   constructs `Signal(..., length=0)` at `waveform_generator_dsp.cpp:1707`).

3. **Resulting evaluation**: `cut(a, 0, 0)` → `from=0 >= length=0`
   → `jge` taken → throw `ArgGreaterThanLength` (template id 88).
   Same code path, same value, same template, same message in
   both implementations.

Suite: 1600/1600 main + 768/768 stress (no change).

---

## IF-194  Regalloc-overflow error reports differ in line attribution

**Source**: Phase 59 round 10 — `regalloc_long_live_single_bb`
(passed via both-error with "accepted" mismatch)
**Status**: fixed (2026-05-07) — line attribution corrected, all 4
stress variants now report line 18 matching the binary exactly
(previously reported lines 10/12). Suite remains 1600/1600 +
768/768.
**Severity**: cosmetic (no compilation-success divergence)

Test case has ~50 vars in one basic block. Both compilers emit
"run out of free registers, please reduce complexity", but on
**different line numbers**:
- orig: line 18
- recon: line 10 / 12 (depending on device)

### Root cause (static analysis of `_seqc_compiler.so`)

The recon's two throw sites in `AsmOptimize::registerAllocation`
read the line number using **vreg as the asm-list index**:

    int lineNr = (vreg < asm_.size()) ? asm_[vreg].lineNumber() : -1;

That is wrong: `vreg` is a register number in 1..numRegs, **not**
an instruction index into `asm_`.  The binary instead uses the
**first instruction-index in some live range** as the asm-list
key, then reads its `wavetableFront` (= `lineNumber()`) field.

Disassembly at `0x2800f6..0x280106` (vreg == totalPhysical site):

    2800f6:  mov 0x20(%r12),%rcx        ; rcx = *iter.value (smallest preg ≥ vreg
                                          still in conflicts set)
    2800fb:  lea (%rcx,%rcx,2),%rcx     ; rcx *= 3
    2800ff:  mov (%rax,%rcx,8),%rax     ; rax = physRegs[*it].__begin_  (24-byte stride)
    280103:  mov (%rax),%rax            ; rax = physRegs[*it][0].__begin_
    280106:  mov (%rax),%ebx            ; ebx = physRegs[*it][0][0]  (first asm-list index)
    ...
    2801fd:  imul $0xa8,%rbx,%rdx       ; rdx = ebx * sizeof(Asm) = 168
    280204:  mov 0x88(%rcx,%rdx,1),%ebx ; ebx = asm_[idx].wavetableFront

Same pattern at `0x280164..0x28016b` (vreg ≥ numPhysical &
!physVreg.empty() site), except using `physVreg` instead of
`physRegs[*it]` (i.e. vreg's own live-range, not preg's).

### Fix

`reconstructed/src/asm/asm_optimize_regalloc.cpp:275-298` and
`:380-398` — replace `asm_[vreg].lineNumber()` with
`asm_[ physRegs[X][0][0] ].lineNumber()`, with the appropriate X
(preg `*it` for the totalPhysical site, vreg for the second
site).  Bounds-checked against `physRegs.size()`,
`physRegs[X].empty()`, `physRegs[X][0].empty()`, and
`asm_.size()`.

### Decision

The error was purely a line-attribution bug.  The two compilers
**agree** on which spill point fails (vreg == totalPhysical with
identical conflicts state); they only disagreed about which
source line to print.  No spill-heuristic divergence — no
constructible test where one succeeds and the other fails.
After the fix, all 4 stress variants of
`regalloc_long_live_single_bb` pass with identical error
messages (line 18 in all cases).


---

## IF-195  DeviceConstants field at +0x58 is misnamed `waveformMemSize`

**Source**: Phase 60 fix of IF-192
**Status**: fixed (2026-05-07, phase 62 cleanup) — renamed to `maxProgramSize` in
`reconstructed/include/zhinst/device/device_constants.hpp:173` and propagated
to all 9 readers across `device_constants.cpp`, `awg_compiler.cpp`, and a
comment in `prefetch_prepare.cpp:622`. Suite remains 1600/1600 + 768/768.
**Severity**: cosmetic (will mislead future readers)

The `DeviceConstants` field at offset `+0x58` is currently named
`waveformMemSize` in `reconstructed/include/zhinst/device_constants.hpp`,
but its actual semantics — verified by GDB-less disassembly of
`AWGCompilerImpl::compileString @0x107397..0x10739e` — is **maximum
sequencer-program length in opcode words** (the instruction-memory
depth).

Values: UHFAWG=1024, UHFQA=1024, HDAWG=16384, SHF*=32768. These
are the limits for the message-12 "program is too large" check.

The misnomer caused IF-192 (silent acceptance of oversized programs
on UHFQA) to slip through review: the recon's check 10 used
`maxSequenceLen` (= 16000 always) instead of `waveformMemSize`,
because the latter's name suggested it had nothing to do with
program size.

**Action**: rename the field to `maxProgramSize` (or
`maxOpcodeWords`) in a follow-up phase, propagate to all readers
(currently 2 sites: `awg_compiler.cpp:476` after IF-192 fix,
`awg_compiler.cpp:501` in check 11 — see IF-196).

---

## IF-196  awg_compiler.cpp check 11 (msg 0xF1) parallel field-swap bug

**Source**: Phase 60 fix of IF-192 — discovered while inspecting
the adjacent code path
**Status**: confirmed and fixed (2026-05-07)
**Severity**: bug (incorrect limit, but no observed test regression)

### Confirmation (static analysis of `_seqc_compiler.so`)

Disassembly at `0x10743d..0x107448` (the cmp+ja that gates the
`ErrorMessageT(0xF1)` block at `0x10769d`):

```
10743d:  mov  -0x60(%rbp),%rbx          ; rbx = this (AWGCompilerImpl*)
107441:  mov  0x68(%rbx),%rcx           ; rcx = [this+0x68]
107445:  cmp  %rcx,%r15                 ; r15 = nonNullWaveformCount
107448:  ja   10769d                    ; > limit -> error
```

Per `awg_compiler.hpp` layout, `deviceConstants_` is at +0x08
of `AWGCompilerImpl`, so `[this+0x68] == [deviceConstants_+0x60]
== maxSequenceLen` (= 16000 universally).

Cross-check: the parallel post-IF-192 check 10 at `0x10739e`
reads `0x60(%rbx)` == `[deviceConstants_+0x58]` ==
`waveformMemSize`. Confirmed: the two checks did have their
fields swapped in pre-IF-192 recon, and IF-192 only fixed half
the swap.

### Fix

`reconstructed/src/codegen/awg_compiler.cpp:518` — change
`deviceConstants_.waveformMemSize` to
`deviceConstants_.maxSequenceLen`.

### Test impact

None on existing suites: 1600/1600 main, 768/768 stress before
and after.  The pre-fix code used 1024 (UHFQA) / 16384 (HDAWG) /
32768 (SHF*) as the wavetable-entry cap; post-fix it is 16000
universally.  No test currently has > 1024 distinct non-null
wavetable entries, so neither value triggers the check.  The fix
is correctness-only relative to the binary's behavior on
hypothetical large-wavetable programs.

### Note on `nonNullWaveformCount` semantics

The binary loop counts only entries where
`[wf+0x48] == 1` AND `[wf+0xd0] != 0`.  The recon (line 506-516
of awg_compiler.cpp) currently counts all non-null `shared_ptr`
entries unconditionally — see existing comment "Check if waveform
is a 'playback' type and count it".  Until a test exercises this
path the recon condition is untested, but it is a separate
concern from IF-196 (which is purely about the limit field).

## IF-197  randomGauss arg-count error reports `4` instead of `3`

**Status**: fixed (2026-05-07, phase 61) — `waveform_generator_dsp.cpp:952`.
**Severity**: bug (cosmetic — wrong number in error message)
**Found**: stress phase 61 backfill (`wave_random_hdawg`)

### Symptom

`randomGauss` accepts 3 or 4 args. When called with the wrong
count, recon reported "expects 4 argument(s)" while the binary
reports "expects 3 argument(s)". The binary's `FuncExactArgs2`
template is fed the **minimum** valid arity for variadic-with-
defaults functions, not the maximum.

```
orig:  Compiler Error (line: 3): function 'randomGauss' expects 3 argument(s), 2 argument(s) given
recon: Compiler Error (line: 3): function 'randomGauss' expects 4 argument(s), 2 argument(s) given
```

### Fix

`waveform_generator_dsp.cpp:952` — change literal `4` to `3` in
the `ErrorMessages::format(FuncExactArgs2, "randomGauss", N, ...)`
call.

### Sibling: randomUniform (confirmed and fixed, 2026-05-07)

`randomUniform` (same file, line 1003) used the same pattern with
literal `2`. Valid arities are 1 or 2. Confirmed by static
disassembly at `0x253ac3`: the binary passes `mov $0x1,%ecx`
(literal `1`) to `ErrorMessages::format(FuncExactArgs2, ...)`.

Confirming differential test: `tests/cases/stress/wave_random_uniform_oor.seqc`
calls `randomUniform(64, 0.5, 0.1)` (3 args). Before fix:

```
orig:  function 'randomUniform' expects 1 argument(s), 3 argument(s) given
recon: function 'randomUniform' expects 2 argument(s), 3 argument(s) given
```

Fix: `waveform_generator_dsp.cpp:1003` literal `2` → `1`. After fix
both error strings are byte-identical. Registered as
`wave_random_uniform_oor_hdawg` and `wave_random_uniform_oor_shfsg`
in `manifest-stress.json`.

Note: the originally proposed test (0-arg call `randomUniform()`)
does **not** reach the `FuncExactArgs2` branch — it is caught
upstream with "called function ... without arguments". Use a
too-many-args call (3) to surface the divergence.

### Audit sweep: gauss, rand (confirmed and fixed, 2026-05-07)

A full sweep of every `FuncExactArgs2` site in the recon was performed
to find any remaining IF-197-pattern bugs.  Methodology: enumerate all
`ErrorMessages::format(FuncExactArgs2, name, N, ...)` call sites; for
each, derive the function's valid arity set from the surrounding
`if (args.size() ...)` guard; any site whose literal `N` does not
equal the *minimum* of that set is a candidate.  Each candidate was
then verified against `_seqc_compiler.so` disassembly by locating the
`mov $0xN,%ecx` immediately preceding the `ErrorMessages::format`
tail-call.

Sites audited (file = `reconstructed/src/waveform/waveform_generator_dsp.cpp`
unless noted):

| Line | Builtin | Valid arities | Recon literal | Binary literal | Verdict |
|------|---------|---------------|---------------|----------------|---------|
| 124 | add | ≥2 | 2 | 2 | match |
| 170 | gauss | {3,4} | **4** | **3** (`0x24e7e6`) | **MISMATCH → fixed** |
| 214 | sin | {3,4} | 3 | 3 (`0x24aa27`) | match |
| 268 | cos | {3,4} | 3 | 3 | match |
| 334 | sinc | {3,4} | 3 | 3 | match |
| 398 | ramp | {3} | 3 | 3 | match (exact) |
| 436 | sawtooth | {3,4} | 3 | 3 | match |
| 474 | triangle | {3,4} | 3 | 3 | match |
| 529 | drag | {3,4} | 3 | 3 | match |
| 635 | blackman | {2,3} | 2 | 2 (`0x24fc59`) | match |
| 697 | hamming | {1,2} | 1 | 1 | match |
| 750 | hann | {1,2} | 1 | 1 (`0x2501a7`) | match |
| 893 | rand | {3,4} | **4** | **3** (`0x25284c`) | **MISMATCH → fixed** |
| 952 | randomGauss | {3,4} | 3 | 3 | match (IF-197) |
| 1003 | randomUniform | {1,2} | 1 | 1 | match (IF-197 sibling) |
| 1115 | rrc | {3,4,5} | 3 | 3 | match |
| 1244 | placeholder | {1,2,3} | 1 | 1 | match |
| 1295 | join | ≥1 | 1 | 1 | match |
| 1319 | join | ≥1 | 1 | 1 | match |
| `waveform_generator.cpp:605` | marker/mask | {2} exact | 2 | 2 | match (exact) |
| `waveform_generator.cpp:738` | helper (exact) | exact | `expected` | n/a | n/a (variable, not literal) |
| `custom_functions_dio.cpp:131` | getZSyncData/UHFQA | {1} exact | 1 | 1 | match (exact) |
| `custom_functions_dio.cpp:230` | getFeedback/UHFQA | {1} exact | 1 | 1 | match (exact) |

Fixes applied:
- `waveform_generator_dsp.cpp:170` — `gauss` literal `4` → `3`.
  Binary `mov $0x3,%ecx` at `0x24e7e6`.
- `waveform_generator_dsp.cpp:893` — `rand` literal `4` → `3`.
  Binary `mov $0x3,%ecx` at `0x25284c`.

Differential tests added:
- `tests/cases/stress/wave_gauss_oor.seqc` — calls `gauss(64,1.0,32.0,8.0,0.0)`
  (5 args).  Registered as `wave_gauss_oor_hdawg` and `wave_gauss_oor_shfsg`.
  After fix both engines produce byte-identical
  `function 'gauss' expects 3 argument(s), 5 argument(s) given`.
- `tests/cases/stress/wave_rand_oor.seqc` — calls `rand(64,1.0,0.0,1.0,0.0)`
  (5 args).  Registered as `wave_rand_oor_hdawg` and `wave_rand_oor_shfsg`.
  After fix the arity wording itself is byte-identical
  (`function 'rand' expects 3 argument(s), 5 argument(s) given`); the
  diff is "accepted-differ" only because the binary additionally emits
  a `Warning (line: N): function 'rand' is deprecated, please use 'randomGauss'`
  prefix that recon does not emit.  That deprecation-warning gap is
  out-of-scope for this audit and is logged separately as IF-200 below.

Suite verification: main 1600/1600 unchanged; stress grew by 4 entries
to 774/774, all green.

## IF-200  `rand` deprecation warning missing in recon

**Status**: fixed (2026-05-08) — root cause was that the recon
`WaveformGenerator` ctor left `aliasMap_` empty (per a previous
incorrect read of the binary).  Re-disassembly of `0x248200` shows
two `<string,string>` emplace calls at `0x24957e` and `0x2495ee`
(into the hash table at `this+0x28`) populating
`aliasMap_["mask"]="marker"` and `aliasMap_["rand"]="randomGauss"`.
Fix: add those two inserts in `waveform_generator.cpp:163-164`.

A second discrepancy was uncovered while verifying the fix: the
recon `WaveformGenerator::call` was redirecting the funcMap_ lookup
to use the *aliased* name (`lookupName = aliasIt->second`).  The
binary keeps using the *original* name (`%r14`, set once from `%rdx`
at entry and reused at the funcMap_ find at `0x25c20b`); the alias
value is consumed only as the second `format(DeprecatedFunc, ...)`
argument.  This matters because the dispatched implementation's own
arity / type errors echo the requested name verbatim — e.g.
`function 'rand' expects 3 argument(s)`, not `'randomGauss'`.  Fixed
in `waveform_generator.cpp:364-403`.

After both fixes `wave_rand_oor_{hdawg,shfsg}` pass byte-identical
(error strings match exactly, no longer flagged as
"differ (accepted)") and the full 6-manifest suite remains 2499/2499.

### Original (open) entry — preserved for history

Originally opened (2026-05-07) — discovered while constructing the
`wave_rand_oor` differential test for the IF-197 audit sweep.
**Severity**: cosmetic (warning text only — does not affect codegen)
**Found**: IF-197 audit sweep, phase 62.audit

### Symptom

The original binary emits a deprecation warning whenever the
`rand` builtin is invoked:

```
Warning (line: N): function 'rand' is deprecated, please use 'randomGauss'
```

The recon emits no warning.  When `rand` is otherwise valid the
ELF still matches byte-for-byte (warnings are not in the ELF), so
this only surfaces in error-output difftests where the warning
prefixes the error string.

### Notes

The diagnostic is emitted from somewhere in the `rand` entry path
(@`0x251cf0`).  The `randomGauss` and `randomUniform` siblings do
not emit such a warning.  Locating and reconstructing the warning
emission site is straightforward but was deferred to keep the
IF-197 audit scope tight — the audit was about arity-error wording
only.

### Workaround

`wave_rand_oor_*` stress entries currently rely on diff_test's
"accepted-differ" tolerance for the warning prefix.  When IF-200
is fixed, those entries will tighten to byte-identical automatically.

## IF-198  setUserReg range-check missing on recon

**Status**: fixed (2026-05-07) — root cause was *not* the same as IF-177.
The recon's `CustomFunctions::setUserReg` (custom_functions_registers.cpp:339)
threw `CustomFunctionsException(format(SetUserRegArgs))` ("expects
exactly two arguments") at *every* validation site — arg-count, arg0
type, arg0 range, and arg1 type — instead of using the four distinct
error templates the binary emits. Re-disassembling `0x14a420` with
4 throw sites (`14b1ec` → 0xc7=199, `14b22b` → 0xc5=197, `14b264` →
0xc6=198 via `CustomFunctionsValueException` argIndex=1, `14b2a8` →
0xc8=200) confirmed the correct mapping. Fix:
- arg0 type-check now throws `SetUserRegConstFirst` (197).
- arg0 range-check now throws `SetUserRegRange` (198) wrapped in
  `CustomFunctionsValueException(msg, 1)` to match `0x14b264` (note
  the larger `__cxa_allocate_exception` size 0x40 and the `mov $0x1,%edx`
  argIndex argument that distinguish the value exception from the
  plain `CustomFunctionsException`).
- arg1 type-check now throws `SetUserRegVarConst` (200).

All 4 `setuserreg_oor_*` stress cases now produce a byte-identical
error string ("setUserReg register must be in the range of 0 to 15")
with no further "(accepted-differ)" reliance. Suite stays at
1600/1600 main + 768/768 stress. Note: IF-177 (the `ones(var)` /
"<func> can't be called with var arguments" wording) is unrelated;
the original "duplicate of IF-177" note was wrong — IF-177 fixes
the var-arg wording in `WaveformGenerator::readInt`, while IF-198
fixes the wrong template selection in `setUserReg` itself for
fully-const arguments.

**Severity**: bug (user-facing — wrong error message)

## IF-199  setPrecompClear flag not threaded into per-play cwvf encoding

**Status**: fixed
**Severity**: bug (codegen — recon misses precomp-flag toggles between plays)
**Found**: stress phase 62 (4 cases: `precomp_chain_toggle`,
`precomp_cwvf_branches`, `precomp_in_loop`, `precomp_marker_cwvf`)

### Symptom

When `setPrecompClear(0|1)` is interleaved with `playWave` calls,
the original binary emits a fresh `cwvf` (or `cwvfr`) instruction
before each play whose precomp-flag changed, encoding the new
flag into the play config. **Recon emits only the initial `cwvf`
and never re-emits it** — every subsequent play uses the original
flag.

### Root cause

`CustomFunctions::setPrecompClear` in
`reconstructed/src/runtime/custom_functions_registers.cpp:550-565`
constructed the SetPrecomp `AsmList::Asm` entry, pushed it into
`results->assemblers_`, but **failed to set
`results->node_ = asmEntry.node`**. As a result the SetPrecomp
Node was never linked into the AST `next`-chain via the standard
`tail->next = bodyEval->node_` assignment in the eval pipeline,
so `Prefetch::optimizeCwvf` never visited any type=0x1000
(SetPrecomp) node.

GDB confirmation (recon `_seqc_compiler.so`):
- `asmSetPrecompFlags(flags=…)` called 10 times alternating
  `0x1, 0x0, 0x1, …` (correct).
- `optimizeCwvf` dispatch at 0x5ded7f saw types `0x1, 0x2, 0x8000`
  but **never type 0x1000**, even though 10 SetPrecomp nodes were
  created.

Compare original `setPrecompClear` at 0x14c8a0–0x14c945: after
calling `asmSetPrecompFlags`, it copies the AsmList::Asm into the
`assemblers_` vector AND writes the entry's node shared_ptr to
`results->node_` (`r15+0x50`) at 0x14c940 (`movups [r15+0x50],
xmm0`).

Other registrations such as `setRate`
(`custom_functions_registers.cpp:653`) correctly do
`results->node_ = asmEntry.node;` before pushing — `setPrecompClear`
was an outlier.

### Fix

```cpp
auto asmEntry = asmCommands_->asmSetPrecompFlags(flag);
results->node_ = asmEntry.node;  // chain into AST next-chain (original @0x14c940)
results->assemblers_.push_back(std::move(asmEntry));
return results;
```

### Verification

- `precomp_chain_toggle.seqc` (HDAWG8, sr=2.4e9) now byte-identical
  to original (6972 B; all 19 sections IDENTICAL).
- Full diff_test_fast.py: 1600/1600 passing, no regressions.

### Earlier (incorrect) hypothesis — kept for posterity

Initially suspected the SetPrecomp dispatch in `optimizeCwvf`
(prefetch.cpp:696) was missing a `cwvf.precompFlags =
curNode->defaultPrecompFlags` write. That is incorrect — the
binary's SetPrecomp dispatch case does **not** update the running
cwvf's precompFlags either. Instead, the propagation happens at
the Play case (binary 0x1d03c2 reloads `r11d` from
`-0x30(rbp)`/`defaultPrecompFlags`). The recon's prefetch logic
was already correct; the SetPrecomp nodes simply never reached
it.

### Lesson

A symptom in a downstream pass (cwvf-merge emitting only one
`cwvf`) misled initial investigation toward the prefetch
propagation logic. The real bug was a single missing assignment
in the eval-result construction five layers upstream. **GDB
trace of node-type dispatch counts is decisive** — once we saw
`type=0x1000` never appeared at the dispatch despite 10
`asmSetPrecompFlags` calls, the upstream chaining defect became
obvious.



## IF-201  `playWaveIndexedNow` emits `wvf` instead of `wvfi`

**Source**: coverage round (Phase 62-style), `cov_playWaveIndexedNow_*`
**Status**: fixed (2026-05-08; commit dddc8ca)
**Severity**: likely-bug
**Found**: 2026-05-08

**Resolution**: `prefetch_placesingle.cpp` (`play_cervino_indexed`,
`split_=true` branch, around line 778) emitted a direct
`asmCommands_->wvf(idxReg, R0, totalSize)` call.  The binary, on the
same path (entry 0x1dabc7), falls through `0x1dbb70 → 0x1d9d3a →
0x1d9ef7` and calls `wvfImpl(idxReg, totalSize, config.now)`, where
`config.now` selects `wvfi` over `wvf`.  GDB-confirmed at 0x1d9ef7
with `r8d=1` for `playWaveIndexedNow`.  Replaced the direct `wvf`
call with `wvfImpl(idxReg, totalSize, node->config.now)`.  All 3
`cov_playWaveIndexedNow_*` tests now pass; stress 894 → 897, no
regressions in default/errors/labone/large suites.

### Symptom

`playWaveIndexedNow(wave, offset, length)` on UHFAWG / UHFQA emits the
non-immediate playback opcode `wvf` whereas the original binary emits
the immediate variant `wvfi`. The non-immediate-now sibling
`playWaveIndexed` (without `Now`) is byte-identical, so the bug is
specific to the `SubFunc::Now` codegen path of `playIndexed`.

```
orig .asm:    wvfi R3, R0, 128
recon .asm:   wvf  R3, R0, 128
```

The .text size is the same (the two opcodes differ in one bit pattern,
not in instruction count).

### Repro

Three of three new tests fail this way:

- `cov_playWaveIndexedNow_solo_uhfawg`  — single-call form
- `cov_playWaveIndexedNow_solo_uhfqa`   — same source on UHFQA
- `cov_playWaveIndexedNow_loop_uhfawg`  — inside a `for` loop

Minimal repro (UHFAWG, kDevCervino):
```
wave w = join(gauss(64,32,8), ones(64));
playWaveIndexedNow(w, 0, 64);
```

### Recommended next step

GDB-trace `playIndexed(args, res, SubFunc::Now)` in the original binary
at the wvf-emission site. Locate the asmCommands_ method dispatched
when `SubFunc::Now` is in effect and compare to the `SubFunc::Default`
dispatch. The recon `playIndexed` likely shares the same emit-call for
both subfuncs; the binary almost certainly switches between
`asmWvf*`/`asmWvfi*` based on `subFunc == Now`.

Files: `reconstructed/src/runtime/custom_functions_play.cpp` (search
for `playIndexed` and its switch on `SubFunc`).

---

## IF-202  `merge` and `grow` registered in recon `funcMap_` but unknown to original

**Source**: coverage round, `cov_merge_*` and `cov_grow_*`
**Status**: fixed (2026-05-08; see waveform_generator_funcmap.md)
**Severity**: likely-bug (cosmetic for now — recon accepts more than binary)
**Found**: 2026-05-08

**Resolution**: Disassembly audit of `WaveformGenerator` ctor at
0x248200..0x249b90 confirmed the binary registers exactly 33 funcMap_
entries — `zeros..circshift` — and neither `merge` (0x25f5c0) nor
`grow` (0x260640) is among them.  Removed the two recon-only
`funcMap_["merge"]` and `funcMap_["grow"]` registrations from
`reconstructed/src/waveform/waveform_generator.cpp`.  The methods
themselves are kept because they are still called internally from
`custom_functions_play.cpp`.  All 11 `cov_merge_*`/`cov_grow_*` tier-1
stress tests now pass; no regressions elsewhere.  See
`reconstructed/notes/waveform_generator_funcmap.md` for the
authoritative key list.

### Symptom

Recon successfully compiles SeqC programs that call `merge(w1,w2)` or
`grow(w, length)` as freestanding waveform-DSL functions.  The
**original binary rejects them**:

```
Compilation failed: Compiler Error (line: N): calling unknown function 'merge'
Compilation failed: Compiler Error (line: N): calling unknown function 'grow'
```

`WaveformGenerator::merge` (@0x25f5c0) and `WaveformGenerator::grow`
(@0x260640) clearly exist as compiled methods in the binary, with
non-trivial implementations (~0x10c0 and ~0x8e0 bytes respectively).
But the binary's funcMap_ registration block (`WaveformGenerator` ctor
@~0x248200) apparently does **not** include `"merge"` or `"grow"` keys.
Recon `waveform_generator.cpp:151-152` does include them, so recon
exposes more functions than the binary does.

### Repro

Six of six new tests fail this way:

- `cov_merge_2wave_{hdawg,shfsg,shfqa,uhfqa}`
- `cov_merge_4wave_hdawg`
- `cov_merge_diff_length_hdawg`
- `cov_grow_solo_{hdawg,shfsg,shfqa,uhfqa}`
- `cov_grow_chain_hdawg`

### Recommended next step

GDB-trace the funcMap_ population in the binary's `WaveformGenerator`
ctor (@0x248200). Walk the emplace calls and confirm whether `"merge"`
and `"grow"` strings are absent. If absent, the methods are likely
called only internally (e.g. from `mergeWaveforms` reduction or from
indexed-wave assembly), not via the user-facing dispatch.

If confirmed, the fix is to remove lines 151-152 from
`reconstructed/src/waveform/waveform_generator.cpp` (and possibly
prune the alias-map / dispatch path comments referencing them). The
methods themselves remain — they're called by other recon code paths.

### Note

This is the inverse of IF-200's `mask`/`rand` aliasMap omission: there
the recon was missing entries the binary had; here recon has entries
the binary lacks.

---

## IF-203  `setInternalTrigger(var)` triggers internal "unspecified value type" error on SHFLI

**Source**: coverage round, `cov_setInternalTrigger_solo_shfli`
**Status**: fixed (commit cfc82ea)
**Severity**: likely-bug
**Found**: 2026-05-08
**Resolution**: Var-arg branch in `setInternalTrigger`
(custom_functions_registers.cpp:99) wrongly called
`AsmRegister(arg.value_.toInt())`; binary @0x146265 reads `arg.reg_`
(offset +0x30) directly, mirroring `setTrigger` @0x1455e5. GDB-confirmed
varType==2 path emits sinttrig with reg_. Also fixes IF-204 as a side
effect (the GHFLI playZero warning was a downstream symptom, not an
independent bug).

### Symptom

A `for`-loop calling `setInternalTrigger(i)` with a runtime `var` arg
compiles cleanly on the original binary (SHFLI), but recon throws an
internal compiler error:

```
Compilation failed: Compiler Error (line: 8): unspecified value type
detected in toInt conversion
```

The const-arg variants `setInternalTrigger(0)` and
`setInternalTrigger(1)` (lines 2 and 4 of the same file) succeed —
the problem is specific to register / var arguments.

### Repro

```
setInternalTrigger(0);     // OK
setInternalTrigger(1);     // OK
var i;
for (i = 0; i < 3; i++) {
  setInternalTrigger(i);   // recon: "unspecified value type detected in toInt conversion"
  playZero(64);
}
```

### Recommended next step

GDB-break inside `CustomFunctions::setInternalTrigger` (@0x146140) on
the original with the var-arg input and observe which arg-type branch
is taken. Recon (`custom_functions_registers.cpp:91`) likely calls
`args[0].value_.toInt()` unconditionally; the binary probably checks
`varType_` and emits an addi/register-based instruction sequence for
the var case (similar to what `at()` does at @0x14d095).

### Note

The companion test `cov_setInternalTrigger_solo_ghfli` (GHFLI) fails
differently — see IF-204.

---

## IF-204  Spurious `playZero(32)` minimum-length warning on GHFLI but not on original

**Source**: coverage round, `cov_setInternalTrigger_solo_ghfli`
**Status**: fixed (cascade of IF-203 — no source change required)
**Severity**: cosmetic / suspicious
**Found**: 2026-05-08
**Resolved**: 2026-05-08 (no code change; fixed by commit cfc82ea — IF-203)

### Resolution

Not an independent bug.  GDB-traced binary's `checkPlayMinLength`
(@0x15b100) on the failing GHFLI input and confirmed:

- Binary's `devConst_->maxWaveformLength` for GHFLI = **96** (matches
  recon, rodata @0x8fc7c0+0x8 = `60 00 00 00`).
- Binary DOES enter the warning branch at 0x15b116 with ESI=32, ECX=96.
- The recon's `maxWaveformLength = 96` for GHFLI is therefore correct.

The "spurious warning + Compiler Error" error string in the failing
output was produced by the `Compilation failed:` catch-all in
`compile_seqc.cpp:266` concatenating the (correctly emitted) warning
text with the message of an exception thrown later.  The exception
itself came from `setInternalTrigger(0)` going down the wrong arg-type
branch on SHFLI/GHFLI (IF-203).  Once IF-203 was fixed (commit
cfc82ea), `setInternalTrigger` no longer threw, the warning was
emitted normally on both sides, and the test became byte-identical
(`compileReport` matches between original and recon).

Verified: stress run after IF-203 fix shows
`cov_setInternalTrigger_solo_ghfli` PASS byte-identical (2044 bytes).

### GDB trace summary (kept for posterity)

```
>>> playZero_const_call_check (0x1388f0)  esi=0x20  eax=0x20
>>> checkPlayMinLength_entry  (0x15b100)  esi=0x20
>>> checkPlayMinLength_cmp    (0x15b112)  esi=0x20  ecx=0x60   # 32 < 96
>>> checkPlayMinLength_warn   (0x15b116)  warning branch entered
```

So binary emits the warning identically; the divergence was purely in
whether a *subsequent* exception aborted compilation.

### Lesson

When a warning appears recon-only and the recon path also throws a
compiler error in the same compile, suspect that the warning is
genuinely shared but the throw site is recon-only.  GDB-checking the
warning site BEFORE assuming a constants mismatch saved a wrong-path
device-constants edit that would have regressed other GHFLI tests.

### Symptom

`playZero(32)` on GHFLI elicited a recon-only warning that aborted
compilation:

```
Warning (line: 3): play length 32 is below minimum of 96 samples,
will be extended
```

The original compiler accepts the same source byte-identically with no
warning emitted. The minimum-length figure (96) appears to come from a
device-constants path that recon evaluates at compile time but the
binary either suppresses or computes differently for GHFLI.

### Repro

Same source as IF-203 — but the GHFLI codepath errors on the warning,
not on the var-arg trigger:

```
setInternalTrigger(0);
playZero(32);   // <- warning here, recon-only
setInternalTrigger(1);
playZero(32);
```

The first `playZero(32)` precedes the for-loop where IF-203 manifests,
so on GHFLI we see this warning *first* and the for-loop is never
reached.

### Recommended next step (historical, no longer applicable)

Compare the GHFLI device-constants block (`devConst_`) field for
"minimum playZero length" between recon and binary. Likely recon is
applying an HDAWG/SHFSG-class minimum to GHFLI when the binary keeps
GHFLI at `1` or has a separate suppression rule.

GDB-trace the path that emits `Warning ... below minimum of N samples`
on a small recon-only program for GHFLI to find the comparison site.

(GDB trace was performed and showed the binary uses the same
constant — see Resolution above.  No constants change needed.)

---

## IF-205  3-arg `randomGauss` waveform samples differ from binary

**Source**: coverage round, `cov_randomGauss_solo_{hdawg,shfsg}`
**Status**: fixed (waveform_generator_dsp.cpp randomGauss 3-arg signature; commit 3562176)
**Severity**: likely-bug

**Resolution** (2026-05-08): Hypothesis A (wrong default) was
*structurally* correct, but the wrong slot was defaulted.  The 3-arg
form is **`randomGauss(length, mean, stddev)` with default
amplitude=1.0**, not `randomGauss(length, amplitude, mean)` with
default stddev=1.0.  Disassembly of the 3-arg branch (0x252bc2) shows:
no `readDoubleAmplitude` call, readDouble strings `"2 (mean)"`
(@0x252e19) and `"3 (standard deviation)"` (@0x253045), and a
`movsd 0x956030(%rip), %xmm0; movsd %xmm0, -0x78(%rbp)` at 0x25305a
that injects the rodata constant 1.0 into the **amplitude** slot.
Test input `randomGauss(256, 1.0, 0)` → mean=1.0, stddev=0,
amplitude=1.0 → every sample = 1.0 → saturates to int16 max 0x7fff,
matching the original ELF byte-for-byte.  Also fixed: the 4-arg path
in recon used wrong index strings `"2 (mean)"` / `"3 (standard
deviation)"`; the binary uses `"3 (mean)"` (built via `inc rax` from
`"2 (mean)"`) and `"4 (standard deviation)"` (rodata 0x905ea9).
Stress: 894 → 897 (+3).

**Found**: 2026-05-08

### Symptom

`randomGauss(length, amplitude, mean)` (3-arg form, default stddev)
produces a waveform whose first sample differs between recon and
binary:

```
section '.wf___randomGauss_2_1' (256 samples, 512 bytes):
  orig: first byte 0xff   recon: first byte 0x19
```

The companion 4-arg call `randomGauss(length, amplitude, mean, stddev)`
in the same file produces a **byte-identical** waveform
(`.wf___randomGauss_3_3` is IDENTICAL). The 3-arg path defaults
stddev=1.0 in recon (`waveform_generator_dsp.cpp:948`) — but the
binary likely uses a different default, or the PRNG state advances
differently between the 3-arg and 4-arg entry paths, or the default is
read from a different rodata constant.

### Repro

```
wave a = randomGauss(256, 1.0, 0);          // orig != recon
wave b = randomGauss(256, 0.5, 0.0, 0.25);  // identical
playWave(a);
playWave(b);
```

Failures: `cov_randomGauss_solo_hdawg`, `cov_randomGauss_solo_shfsg`.
The `randomUniform` 1-arg / 2-arg variants pass without diff, so the
RNG itself (mt19937_64 from `GlobalResources::random`) is correct.

### Recommended next step

GDB-break at the 3-arg entry of `WaveformGenerator::randomGauss`
(@0x252930, ~3-arg path at 0x252973 or so) and read the loaded
stddev double constant. The recon comment (line 947 of
`waveform_generator_dsp.cpp`) cites "Default stddev = 1.0 from rodata
@0x956030" — re-read that rodata in the binary and confirm the value.
If it is something other than 1.0 (e.g. 0.4 or 2π), update the recon
default.

Alternatively, the bug could be in the readDouble helper — the binary
might consume an extra PRNG word for the missing-arg path, advancing
the mt19937_64 state by one before the sample loop. Compare
`prng.next()` call counts between the 3-arg and 4-arg paths.

---

## IF-206  Wrong inline comments on `prefetch.cpp` device-type check (GDB-confirmed, fixed)

**Source**: Magic constants audit (holistic cleanup pass)
**Status**: fixed
**Severity**: likely-bug (mislabelled comment could mislead future changes)

`Prefetch::placeLoads()` at `prefetch.cpp:2364` had the condition:

```cpp
if (devType == 0x20 /*SHFSG*/ || devType == 0x10 /*SHFQA*/) {
```

Both inline comments were wrong. GDB trace on the original binary confirmed:
- `cmp $0x20` is hit when `deviceType == SHFQC_SG` (esi=0x20=32)
- `cmp $0x10` is hit when `deviceType == SHFSG` (esi=0x10=16)

The constants were correct; the comments were wrong. The condition checks
whether the device is SHFQC_SG **or** SHFSG for a memory overflow guard.
`SHFQA` (0x08) and `SHFSG` (0x10) were mislabelled; had future work assumed
the comment rather than the constant, the SHFQC_SG device would have been
silently excluded from a memory safety check.

Fixed in same pass: updated comment to `/*SHFQC_SG*/` and `/*SHFSG*/`,
and surrounding disassembly note updated to match. Also converted the
raw `0x20`/`0x10` literals to `AwgDeviceType::SHFQC_SG`/`AwgDeviceType::SHFSG`
as part of the broader magic-constants cleanup (B1).

## IF-207  Swapped MESSAGE/ERROR_MSG values in `asm_optimize.hpp` banner comment

**Source**: D2 Batch 7b verification (verify-then-write workflow)
**Status**: fixed in D3 cleanup — comment corrected in
`include/zhinst/asm/asm_optimize.hpp:253` and
`src/asm/asm_optimize.cpp:651` to read
`MESSAGE (cmd==3) and ERROR_MSG (cmd==5)`.
**Severity**: cosmetic (comment-only)

The header banner comment above `AsmOptimize::reportUserMessages()` in
`include/zhinst/asm/asm_optimize.hpp:160-162` reads:

> Extract user MESSAGE (cmd==5) and ERROR_MSG (cmd==3) instructions

The values are swapped relative to the canonical `Assembler::Command`
enum (`MESSAGE = 0x03`, `ERROR_MSG = 0x05`) defined in `assembler.hpp`
and confirmed by the implementation at `src/asm/asm_optimize.cpp:659`
which dispatches via the symbolic enum constants
(`if (cmd == Assembler::ERROR_MSG)` ... `if (cmd == Assembler::MESSAGE)`).
The same swap appears in the corresponding source-file banner at
`src/asm/asm_optimize.cpp:651`. Both should read
`MESSAGE (cmd==3) and ERROR_MSG (cmd==5)`.

Code is correct (uses the symbolic enum); only the surrounding
documentation comments are wrong. Promote to TODO.md as a one-line
comment fix at next sub-phase wrap-up.

## IF-208  Misleading `useDA` field name in `PrefetcherNodeState`

**Source**: D2 Batch 8b verification (verify-then-write workflow); user
caught a speculative "dual-amplitude / precompensation" gloss derived
from the field name.
**Status**: fixed in D3 cleanup — field renamed to `crossesCacheLine`
in `include/zhinst/codegen/prefetch.hpp:193` and at all four
read/write sites (`src/codegen/prefetch.cpp:834,837` write,
`src/codegen/prefetch.cpp:1495,1845` comments,
`src/codegen/prefetch_placesingle.cpp:204` read). Surrounding layout
comment also updated. The unrelated `bool useDA = devConst_->hasPrecomp`
stack locals in `prefetch.cpp` lines 87/331/560/600 were left as-is
per the audit recommendation (those genuinely encode the precomp
flag and are scoped strictly to the optimizer/createLoad bodies, so
no shadowing remains after the field rename).
**Severity**: cosmetic (field-name / comment clarity)

The `bool useDA` field at `PrefetcherNodeState +0x38` (declared in
`include/zhinst/codegen/prefetch.hpp`) does **not** track a "DA" or
"precompensation" property despite the name and the surrounding
inline comment `// (init=false, precomp/DA flag)`.  Verified usage:

- **Set** in `Prefetch::assignLoad` at `src/codegen/prefetch.cpp:834`:
  ```cpp
  bool useDA = waveformIR->crossesCacheLine_;  // 0x1ccd0a movzbl 0xda(%rax)
  it2->second.useDA = useDA;                   // 0x1ccd37 mov %r14b,0x58(%rax)
  ```
  i.e. the field is a copy of `WaveformIR::crossesCacheLine_`.

- **Read** in `Prefetch::placeSingleCommand` at
  `src/codegen/prefetch_placesingle.cpp:204`:
  ```cpp
  if (config_->isHirzel) {
      auto& stateDA = nodeStates_[node];
      if (stateDA.useDA) { ... emit prf with clampToCache ... }
  }
  ```
  i.e. consumed only on Hirzel devices to gate emission of the
  cache-clamped `prf` prefetch sequence required when a single load
  straddles a cache-line boundary.

The misleading name comes from local-variable shadows elsewhere in
`prefetch.cpp` (lines 87, 331, 560, 600) where a different,
unrelated `bool useDA = devConst_->hasPrecomp;` is used — those are
genuine "use precomp" flags, but they are stack locals, not the PNS
field.  The field reuses the identifier, creating a false visual
association.

**Recommended**: rename the field to `crossesCacheLine` (or
`needsClampedPrefetch`) and remove the `precomp/DA` parenthetical
from the inline layout comment.  Keep this as a TODO.md item rather
than a doc-batch fix because the rename touches all read/write
sites and warrants a dedicated commit + difftest run.

Doc-batch mitigation: the `\brief` for `PrefetcherNodeState` describes
the field by behaviour (`crossesCacheLine_` source + Hirzel `prf`
gating) rather than by the misleading name.

## IF-209  `Compiler::setLineNr` recon body missing AsmCommands + WavetableFront propagation

**Severity**: medium (functional gap, not just cosmetic).
**Status**: GDB-confirmed and fixed in D4 Batch 1 follow-up.
**Discovered**: D4 Batch 1 audit, while writing `\brief` for the
six-line getters/setters cluster on `Compiler`.

The reconstructed body of `Compiler::setLineNr(int)` in
`reconstructed/src/codegen/compiler.cpp:691-695` is:

```cpp
void Compiler::setLineNr(int nr) {
    lineNr_ = nr;
    // Propagate to AsmCommands at *(this+0xB0)+0x50
    // Also tail-calls WavetableFront::setLineNr on *(this+0xA0)
}
```

i.e. only the field write is implemented; the AsmCommands and
WavetableFront propagation noted in the inline comment is **missing
from the body**.  The propagation targets exist:
- `WavetableFront::setLineNr(int)` at
  `reconstructed/src/waveform/wavetable_front.cpp:388-391` writes
  `manager_->numDefs_` (matches binary `0x29ce10`).
- `AsmCommands` field at `+0x50` is presumed to be its own line-number
  cache (used by every `setWavetableFrontIndex` call site, e.g.
  `reconstructed/src/ast/seqc_ast_eval_arithmetic.cpp:100`); the
  setter has not been reconstructed yet.

Whether the gap matters in practice depends on who calls
`Compiler::setLineNr`.  It is exposed via the `AWGCompiler` /
`AWGCompilerImpl` chain but is not currently exercised by any of the
1600 differential tests (otherwise we would already see asm-list line
mismatches).  Worth verifying with GDB which sites in the binary
actually trigger this propagation before deciding whether to extend the
recon body or just document the discrepancy.

**Action**: GDB-trace the binary `Compiler::setLineNr` (vtable look-up
or direct symbol; address visible in disassembly) on a representative
test case to confirm whether the AsmCommands + WavetableFront writes
fire unconditionally or only along a code path no current test
exercises.  Then either extend the recon body to include the
propagation, or downgrade IF-209 to "by-design" with a `\binarynote`
on `setLineNr`.

**Resolution**: `objdump -d _seqc_compiler.so` on
`_ZN6zhinst8Compiler9setLineNrEi` confirms the binary unconditionally
performs all three writes:

```
123640: push rbp; mov rbp, rsp
123644: mov [rdi+0x10], esi          ; lineNr_ = nr
123647: mov rax, [rdi+0xb0]          ; rax = asmCommands_.ptr_
12364e: mov [rax+0x50], esi          ; asmCommands_->wavetableFrontIndex_ = nr
123651: mov rdi, [rdi+0xa0]          ; rdi = wavetable_.ptr_
123658: pop rbp
123659: jmp WavetableFront::setLineNr ; tail call
```

`asmCommands_->wavetableFrontIndex_` already had a public setter
(`AsmCommands::setWavetableFrontIndex`); the recon body was extended
to call it and then `wavetable_->setLineNr(nr)`.  See
`reconstructed/src/codegen/compiler.cpp:691-695` and the `\details`
brief on `Compiler::setLineNr`.

Doc-batch mitigation: the `\brief` for `Compiler::setLineNr`
describes the *current* recon behaviour (sets `lineNr_`) and does not
promise propagation to `AsmCommands` or `WavetableFront`.  Adds a
`\verifyme` referencing IF-209 so the doc reader is alerted to the
known gap.

## IF-210  `Compiler::setCancelCallback` recon body missing WaveformGenerator propagation

**Severity**: low (functional gap, but cancellation is best-effort).
**Status**: GDB-confirmed and fixed in D4 Batch 1 follow-up.
**Discovered**: D4 Batch 1 audit, alongside IF-209.

The reconstructed body of `Compiler::setCancelCallback` in
`reconstructed/src/codegen/compiler.cpp:643-647` is:

```cpp
void Compiler::setCancelCallback(std::weak_ptr<CancelCallback> cb) {
    cancelCallback_ = std::move(cb);
    // Also propagates to sub-object at *(this+0xC0)+0xB0
    // (likely WaveformGenerator's cancel callback)
}
```

Layout review confirms `+0xC0` is `waveformGen_` (the
`shared_ptr<WaveformGenerator>` member).  The inline comment
hypothesises that the binary additionally writes the same `weak_ptr`
into `WaveformGenerator` at offset `+0xB0`.  The reconstructed body
performs only the `cancelCallback_` write; no propagation.

`WaveformGenerator` does not currently expose any `setCancelCallback`
API (`grep -r "setCancelCallback" reconstructed/include/zhinst/waveform/`
returns nothing), and nothing in the reconstructed pipeline reads a
cancel callback from `WaveformGenerator`.  Like IF-209 the gap does
not surface in the differential tests because no test cancels
mid-compile.

**Action**: GDB-confirm whether the binary writes through
`waveformGen_->cancelCallback_` here, then either extend the recon
body and add the field to `WaveformGenerator`, or downgrade IF-210
to "by-design" with a `\binarynote`.

**Resolution**: `objdump -d _seqc_compiler.so` on
`_ZN6zhinst8Compiler17setCancelCallbackENSt3__18weak_ptrINS_14CancelCallbackEEE`
confirms the binary unconditionally performs both writes: the local
`cancelCallback_` store at `+0xE0`, then a 16-byte
`movups xmm0, [rax+0xb0]` where `rax = *(this+0xc0) =
waveformGen_.ptr_`.  The existing `reserved_B0_` member was
mislabelled — it is a real `weak_ptr<CancelCallback>` slot used by
this propagation path.  Fixes:

- `reconstructed/include/zhinst/waveform/waveform_generator.hpp`:
  rename `reserved_B0_` → `cancelCallback_`, retype to
  `weak_ptr<CancelCallback>`, add a `setCancelCallback` setter and a
  `CancelCallback` forward declaration.
- `reconstructed/src/waveform/waveform_generator.cpp`: update the
  member-initialiser list to use the new name.
- `reconstructed/src/codegen/compiler.cpp:643-647`: extend
  `Compiler::setCancelCallback` to call
  `waveformGen_->setCancelCallback(std::move(cb))` after the local
  store.

`WaveformGenerator` itself still has no reader for the slot; the
field is propagated for forward compatibility (a future cooperative-
cancellation point inside an expensive waveform builder will be able
to poll the same hook the outer pipeline polls).

Doc-batch mitigation: the `\brief` for `Compiler::setCancelCallback`
describes only the `cancelCallback_` store and adds a `\verifyme`
referencing IF-210.

## IF-211  `Waveform::File::operator==` qualifier inconsistency in waveform.cpp

**Severity**: cosmetic.
**Status**: fixed in D3 warning-triage commit `2cd360b`.
**Discovered**: D3 doxygen warning triage.

`reconstructed/src/waveform/waveform.cpp` defined `operator==`,
`typeToStr`, and `typeFromStr` using the in-class type alias
`Waveform::File::` (the alias is `using File = WaveformFile;` inside
`class Waveform`).  This compiled correctly because the alias resolves
identically, but Doxygen's symbol binder could not match the qualified
name back to the class, generating a "no uniquely matching class
member" warning.

`operator==` was renamed to its canonical `WaveformFile::operator==`
in commit `2cd360b`.  `typeToStr` and `typeFromStr` were left in the
`Waveform::File::` form because they did not generate doxygen
warnings (their out-of-line definitions matched the header
declarations directly via the alias-aware lookup that doxygen
handles for static members but not for non-static `operator==`).

## IF-212  `prefetch_prepare.cpp` summary comments contradict code

**Severity**: cosmetic, but high-risk for downstream doc accuracy.
**Status**: open.
**Discovered**: D4 Batch 2a verify-then-write check of
`Prefetch::prepareTree`.

The `// ====` block-header comments at the top of every function in
`reconstructed/src/codegen/prefetch_prepare.cpp` describe the
function semantics in prose form.  Several claims in the
`prepareTree` block-header (lines 44-88) **contradict the actual
function body** that immediately follows:

1. Line 51 claims `Play (0x01)` is the wave-collection / used-mark
   branch; lines 56-57 claim `Load (0x02)` calls `linkLoad`.
   The code at lines 122-167 has these reversed: the first big
   `case NodeType::Load:` (NodeType value `0x0001`) is the wave-
   collection branch, and `case NodeType::Play:` (value `0x0002`)
   calls `linkLoad`.  The block-header has the type *names* swapped
   relative to the case-label dispatch.

2. Line 67 calls the conditional "Wait" with type `0x40`, gated on
   "config_->field_0x18 (not append mode)".  The code at line 199
   uses `case NodeType::Lock:` (value `0x40`, name confirmed in
   `node.hpp:51`; `Wait` is a separate enumerator with value
   `0x200000` per `node.hpp:69`), and the gate is
   `if (config_->isHirzel)` (line 202), where `isHirzel` is the
   verified label for `+0x18` per `awg_compiler_config.hpp:31,81`
   (the `appendMode` interpretation was already dropped per
   `awg_compiler_config.hpp:114-115`).  The block-header narrative
   was never updated when the field was renamed.

This is a documentation-accuracy bug, not a code-correctness bug:
the body itself uses the correct `NodeType::Lock` and
`config_->isHirzel`.  The risk is that future doc-writing passes
(D4+) trust the block-header summaries instead of the code, which
would propagate the inversions into user-facing Doxygen briefs.

**Action**: rewrite the prepareTree block-header comment in
`prefetch_prepare.cpp` to match the body.  Then audit the
`countBranches` and `definePlaySize` block-headers in the same file
for similar drift before D4 Batch 2a writes briefs against them.

## IF-213  `Prefetch::findLockedPlay` body is a stub that always returns null

**Severity**: likely-bug.
**Status**: **fixed** (commit pending) — body reconstructed at
`prefetch_helpers.cpp:432-525` from full disassembly of
`0x1d3dd0..0x1d4442`.  The previously-recorded end address
`0x1d4a10` overstated the function range by ~2 KB; everything
above `0x1d4442` belongs to a different function
(`Node::remove`).
**Discovered**: D4 Batch 2a verify-then-write check of `prepareTree`'s
`Lock`-handling branch.

**Resolution**: confirmed by GDB (Play branch, see original IF-213
trace in this entry's history) plus full static disassembly
covering the Unlock branch (which the GDB trace did not exercise
because no current corpus test routes an Unlock-with-matching-wave
through it).  The function is a LIFO `std::deque` worklist walk
over the AST with the following per-node logic:

1. If `node->type == NodeType::Play` and
   `node->wavesPerDev[node->deviceIndex]` is engaged and equal to
   `waveform->name`, return the node immediately
   (binary site `0x1d3f8f..0x1d43cc`, `bcmp` at `0x1d404a`).
2. Unconditionally push every `branches` entry, then push `loop`
   if non-null (binary `0x1d4080..0x1d41c6`).
3. If `next` is null, do nothing further.  Otherwise, if
   `node->type == NodeType::Unlock` **and** the Unlock node's own
   wave name at `deviceIndex` equals `waveform->name`, do NOT
   push `next` (the Unlock closes the Lock scope for our wave);
   otherwise push `next`
   (binary `0x1d41c6..0x1d42d0`, second `bcmp` at `0x1d428e`).

`waveform->name` is read directly out of the inherited
`Waveform::name` field at WaveformIR offset 0.

The function does not touch `nodeStates_`, `wavetableIR_`,
`Cache`, or any other `Prefetch` member.  Push order
(branches → loop → next) is preserved so the LIFO pop order
matches the binary for any differential test that turns up
multiple-Play scenarios in the future.

**Why tests still pass at 1602/1602 even before the fix**: the
only `Lock` test in the manifest (`tests/cases/uhfli_misc_funcs.seqc`)
has a trivial `lock(w); playWave(w); unlock(w);` pattern where
`prepareTree`'s `createLoad` fallback (the always-taken path
when `findLockedPlay` returns null) happens to produce the same
ELF the binary would produce on the match path.  More elaborate
Lock patterns (multiple Plays sharing a waveform inside a Lock,
or nested Locks) would diverge silently — no such tests exist.
ELF output for the existing test is unchanged with the fix in
place.

**Open**: the Unlock-terminates-scope branch is confirmed only
by static disassembly; a GDB trace on a hand-written
`Lock { playWave(w); } ... Unlock; playWave(w);` style test
would close the last verification gap.  Recommend adding such a
test in a future regression-coverage pass.

## IF-214  "BFS" misnomer pervasive in Prefetch traversal comments

**Severity**: cosmetic, doc-risk.
**Status**: fixed in same edit pass that logged the IF.
**Discovered**: D4 Batch 2b/2c verify-then-write of
`Prefetch::determineFixedWaves`, `optimize`, `optimizeSync`,
`optimizeCwvf`, `allocate`, `moveLoadsToFront`,
`getMemoryHighWatermark`, `getRequiredMemory`, and
`collectUsedWaves`.

At least 9 distinct comment sites in
`reconstructed/src/codegen/prefetch.cpp` and
`reconstructed/src/codegen/prefetch_helpers.cpp` describe the
traversal as "BFS" (or "BFS/DFS") when the actual code is a LIFO
stack walk: `std::deque<shared_ptr<Node>> worklist;
worklist.push_back(...); ...; auto cur = worklist.back();
worklist.pop_back();`.  `back()` + `pop_back()` is LIFO, so these
are depth-first traversals using a deque as a stack, not BFS.

Concrete sites (incomplete list, found via
`grep -n "BFS" reconstructed/src/codegen/prefetch*.cpp`):

- `prefetch.cpp:145`  `setParents` — "BFS traversal using a deque"
- `prefetch.cpp:743`  `moveLoadsToFront` — "does a BFS over the tree"
- `prefetch.cpp:856,861,972`  inner Play-rewrite loop in
  `moveLoadsToFront` — "BFS traversal setup", "BFS loop", "BFS"
- `prefetch.cpp:1000,1078`  `optimize` — "BFS traversal of the
  node tree using a deque as worklist", "Main BFS loop"
- `prefetch_helpers.cpp:231`  `setParents` cousin — "BFS traversal
  that sets parent weak_ptr"
- `prefetch_helpers.cpp:277,331`  `removeBranches` — references
  "prepareTree's BFS" (which is itself a stack walk —
  `std::stack<shared_ptr<Node>>` — also not BFS)
- `prefetch_helpers.cpp:392`  `findLockedPlay` — "BFS/DFS
  traversal" (hedge; the code is LIFO)
- `prefetch_helpers.cpp:509`  third-cousin tree walker — "BFS
  traversal starting from root node"

Risk is identical to IF-212: doc-writing passes that quote the
prose summaries propagate the misnomer into user-facing Doxygen
briefs.  D4 Batch 2a's three Prefetch sub-pass briefs already use
the verified phrasing ("LIFO traversal", "stack-driven traversal",
"Stack-driven traversal (LIFO via deque::back / pop_back)"); D4
Batch 2b's `determineFixedWaves` brief uses the same.

**Action**: in a single comment-only edit pass, rewrite each "BFS"
mention in the two files to match what the code actually does
("LIFO stack walk via `std::deque::back()` / `pop_back()`" or
similar).  No source-code changes are needed.

**Resolution**: all 15 sites in `prefetch.cpp` and
`prefetch_helpers.cpp` rewritten to "LIFO walk", "LIFO loop",
"stack walk", or equivalent that names `std::deque::back()` /
`pop_back()` explicitly.  `grep -n "BFS"` on both files returns
empty.  Build clean, 1600/1600 tests pass.

## IF-215  `Prefetch::optimize` block-header misidentifies dispatched NodeType

**Severity**: cosmetic, doc-risk.
**Status**: fixed in same edit pass that logged the IF.
**Discovered**: D4 Batch 2c verify-then-write of `Prefetch::optimize`.

The `// ====` block-header comment for `Prefetch::optimize`
(`prefetch.cpp:997-1061`) opens with:

```
// 1. **Play nodes (type == 0x02)**: The core optimization target.
```

and goes on to describe the parent-walking / `sameLoads` /
`mergeLoads` logic as the Play-node case.  The function body line
1144-1145 actually dispatches:

```
} else if (nodeType ==
           static_cast<int>(NodeType::Load)) { // 0x1cde1a: cmp $0x1
  // --- LOAD node (type == 0x01) — this is the Play optimization case ---
```

i.e. the case-label is `NodeType::Load` (= `0x0001`), not
`NodeType::Play` (= `0x0002`).  The comment block has them
swapped, and the inline label on line 1146 contradicts itself
("LOAD node ... — this is the Play optimization case").

Per the verified enum in `reconstructed/include/zhinst/ast/node.hpp:45-46`:

```
Load               = 0x0001,   // "load"
Play               = 0x0002,   // "play"
```

This is the same Play↔Load name swap that IF-212 documented in
`prepareTree`.  The code is correct (it dispatches on the value
that the binary reads); the prose is wrong.

**Action**: rewrite the `optimize` block-header comment to refer
to `Load` (not `Play`) for the parent-walking case, and remove the
self-contradicting "this is the Play optimization case" line.
Audit the rest of the `optimize` body for any further mislabelled
case branches in the same pass.  Defer the doc brief for
`Prefetch::optimize` until the block-header has been corrected and
re-read against the body.

**Resolution**: block-header (`prefetch.cpp:1006-1063`) fully
rewritten from a body-verified read of the dispatch sites at
`0x1cde00`–`0x1cf4b3`.  The four real cases (`Loop`/`Branch`/
`Load`/default) are now listed in the order the source dispatches,
and the three `Load`-parent sub-cases (`SetVar`/`Load`/`Play`/other)
are spelled out with the verified `cmp` constants.  The
self-contradictory inline label on line 1148 ("LOAD node ... — this
is the Play optimization case") was rewritten to "the core
optimization case".  The mislabelled inline at line 1157 ("Check
if parent is a Loop (type == 0x10 i.e. SetVar)") was rewritten to
"Check if parent is a SetVar (type == 0x10)".  No source-code
changes were needed.  Build clean, 1600/1600 tests pass.  The
brief for `Prefetch::optimize` (D4 Batch 2c) can now safely
consult the corrected header.

## IF-216  `Prefetch::allocate` dispatches on `NodeType::Wait` where binary cmps `$0x40` (`Lock`)

**Severity**: likely-bug.
**Status**: fixed (D4-2c follow-up); GDB-confirmed.
**Discovered**: D4 Batch 2c verify-then-write of
`Prefetch::allocate` (`prefetch.cpp:1552-2070`, original at
`0x1d0fb0`).

The high-NodeType dispatch at `prefetch.cpp:1571-1584` is:

```cpp
if (static_cast<int>(type) > 0x3F) { // 0x1d1025: cmp $0x3f; jg
  if (type == NodeType::Wait) { // 0x1d1090: cmp $0x40 → 0x1d140d
    goto handleWait;
  } else if (static_cast<int>(type) == 0x80) { ...
```

Per the verified `NodeType` enum in
`reconstructed/include/zhinst/ast/node.hpp:45-69`:

- `Lock = 0x0040`
- `Unlock = 0x0080`
- `Table = 0x0200`
- `Wait = 0x200000`

The address comment on the cmp is unambiguous: `cmp $0x40`.  That
matches `Lock`, not `Wait`.  The recon code symbolically names
this `NodeType::Wait`, which the compiler will lower to `cmp
$0x200000` — guaranteed to never match a Lock node and equally
guaranteed to potentially mismatch a Wait node coming through this
path.

The `handleWait:` label at `prefetch.cpp:1797` and the
neighbouring block-header further mislabel the `0x40` branch as
"Wait", and `0x80` as "Rate?" (`Unlock` per the enum).  These are
documentation mistakes in addition to the recon bug, but the
*code* mistake is the `NodeType::Wait` symbol.

Knock-on block-header errors in the same comment block:

- `prefetch.cpp:1483` "type=1 (Play)" — should be `Load`.
- `prefetch.cpp:1484` "type=2 (Load/SetVar)" — should be `Play`.
- `prefetch.cpp:1487` "type=0x40 (Wait)" — should be `Lock`.
- `prefetch.cpp:1488` "type=0x80 (Rate?)" — should be `Unlock`.

These are the same Play↔Load swap pattern as IF-212 / IF-215, plus
two new mislabels for the high-bit types.

**Why tests still pass at 1600/1600**: no test exercises a Lock or
Wait node reaching `Prefetch::allocate`.  Lock-using SeqC programs
appear to be either rare in the test corpus or stripped before
allocation.  This is likely the same coverage gap that masks
IF-213 (`findLockedPlay` stub).

**Action**:
1. **Defer** writing the Doxygen brief for `Prefetch::allocate`
   until the dispatch is fixed and the block-header is rewritten.
2. GDB-trace the original binary at `0x1d0fb0` on a Lock-using
   SeqC program (e.g. anything using `setUserReg`/`waitDigTrigger`
   inside a lock; or write a minimal reproducer if no test covers
   it).  Confirm whether the `cmp $0x40` branch handles `Lock`,
   `Wait`, or both, and whether `Wait`-typed nodes reach
   `allocate` via a different path or are stripped earlier.
3. Once GDB-confirmed, change the symbolic name in
   `prefetch.cpp:1573` to match (`NodeType::Lock` is the
   address-comment-implied fix), rewrite the block-header at
   `prefetch.cpp:1483-1490` from the verified body, and run the
   full test suite to confirm no regressions.
4. Add a regression test if no existing case exercises the path.
5. Then write the doc brief for `Prefetch::allocate` against the
   corrected source.

This entry is a sibling of IF-213 (`findLockedPlay` stub) — both
are likely-bugs in the Lock-handling pipeline that no existing
test currently exposes.  Investigating them together (a single
Lock-using GDB session) may resolve both.

**Resolution (D4-2c follow-up)**: GDB-traced the original binary
at `0x1d0fb0` on `tests/cases/uhfli_misc_funcs.seqc`
(`lock(w); playWave(w); unlock(w);`).  Trace at the dispatch
sites confirms:

- `0x1d1090` (`cmp $0x40` → `0x1d140d`) fires with `eax = 0x40`,
  i.e. the branch is taken by genuine `Lock`-typed nodes.  The
  recon's `NodeType::Wait` (`0x200000`) was a typo for
  `NodeType::Lock` (`0x40`).
- `0x1d1099` (`cmp $0x80` → `0x1d145a`) fires with `eax = 0x80`,
  i.e. the branch is taken by `Unlock`-typed nodes.  The
  surrounding "Rate?" label was wrong.
- `0x1d10a4` (`cmp $0x200` → `0x1d10af`) is the `Table` branch;
  unchanged.

Fixed in the same commit:

- `prefetch.cpp:1573` `NodeType::Wait` → `NodeType::Lock`.
- `goto handleWait` / `handleWait:` / `waitInsertNameMap` →
  `handleLock` / `lockInsertNameMap` (cosmetic but matches
  semantics).
- `goto handleRate80` / `handleRate80:` / `rate80InsertNameMap`
  → `handleUnlock` / `unlockInsertNameMap`.
- The `// type=0x40 (Wait)` and `// type=0x80` block comments on
  the dispatch labels rewritten to `(Lock)` / `(Unlock)` and
  describe the matched-name-map semantics
  (`nameMap_[name] = true` for Lock, `false` for Unlock).
- The function-level block-header at `prefetch.cpp:1475-1554`
  fully rewritten: the type list now lists every verified case
  in source order with the correct enum names; the
  per-case-summary section reorganised so each NodeType has its
  own block.

Verified clean: 1600/1600 tests pass, build clean, 0 doxygen
warnings.  The Doxygen brief for `Prefetch::allocate` was added
in the same commit (no longer deferred).

GDB driver: `tests/gdb/gdb_trace_lock.py`.
GDB recipe: `tests/gdb/gdb_lock_trace.txt`.

## IF-217  `Prefetch::backwardTree` re-enqueues `next` instead of `loop`

**Severity**: likely-bug.
**Status**: **fixed** (commit pending) — recon body now reads
`cur->loop` (+0xE0) at the third dispatch, matching the binary.
**Discovered**: D4 Batch 2d verify-then-write of
`Prefetch::backwardTree` (`prefetch_helpers.cpp:235-265`,
original at `0x1d57d0`).

**Resolution**: confirmed against binary disassembly without
needing GDB.  At `0x1d5af0` the third dispatch begins with:

```
1d5af0:  mov    0xe0(%r12),%rax        ; cur->loop (+0xE0)
1d5b87:  mov    0xe8(%r12),%rcx        ; loop's __cntrl_ (+0xE8)
1d5b8f:  movups 0xe0(%r12),%xmm0       ; copy full shared_ptr<Node>
```

This unambiguously reads `+0xE0` (loop), not `+0xB8` (next), and
mirrors the layout of the first dispatch at `0x1d5949`
(`mov 0xb8(%r12),%rax`) which reads next.  The reconstruction at
`prefetch_helpers.cpp:264-273` was flipped from `cur->next` →
`cur->loop` in both the field read and the push.  Full test
suite remained at 1602/1602 (as predicted in the original IF
entry — the corpus does not exercise loop bodies whose
`parent.lock()` is consulted post-`backwardTree`).

The function is a LIFO worklist walk that should set every
visited node's children to back-link to it via `Node::parent`.
The body visits three child links per pop:

1. `cur->next` (sibling chain at `+0xB8`) — handled at
   `prefetch_helpers.cpp:247-251`.
2. Every entry of `cur->branches` (vector at `+0xC8`) — handled
   at `prefetch_helpers.cpp:254-257`.
3. A third `if (cur->next) { cur->next->parent = current;
   worklist.push_back(cur->next); }` at
   `prefetch_helpers.cpp:259-263`, with the inline comment
   `// For next / loop` and the parenthetical `// +0xB8 (was
   elseBranch — confirmed same as next)`.

The third block is **identical** to the first, both syntactically
and semantically: it re-reads `cur->next`, re-writes its
`parent`, and re-pushes it.  The intended third visitor — by
analogy with every other walker in this file (`prepareTree`,
`countBranches`, `optimizeSync`, `nodeByCachePointer`,
`determineFixedWaves`, `findLockedPlay`) — is `cur->loop`
(`+0xE0`, the loop-body / else-branch link), so that loop bodies
get their parent back-pointers and are re-walked.

As-written, `Node::loop` children **never receive a parent
back-link** from `backwardTree`.  Any caller that relies on
`parent.lock()` from inside a loop body will see a stale or
default-constructed weak_ptr.

**Historical note** (pre-fix): tests still passed at 1600/1600
because callers of `backwardTree` are limited (it is only invoked
from a small set of Prefetch passes that themselves walk the
tree top-down and already hold parent context).  The
`Node::parent` weak_ptr is also commonly initialised earlier in
the lowering pipeline (`AwgCompilerNodes::lower*` sets parents
at construction time), so the back-link populated by
`backwardTree` was largely redundant for the inputs the test
corpus produces.  A regression test specifically exercising
loop bodies whose parent link is stale would expose this.

## IF-218  `Prefetch::expandSetVar` body is a stub that creates orphan SetVar clones

**Severity**: likely-bug (stub).
**Status**: **fixed** (commit pending) — body rewritten at
`prefetch_helpers.cpp:372-432` to match the binary's parent-chain
walk; block-header summary updated. The original IF hypothesis
("walks sibling chain, gates on SetVar type, drops clones") was
**wrong on all three points** — see "Resolution" below.
**Discovered**: D4 Batch 2d verify-then-write of
`Prefetch::expandSetVar` (`prefetch_helpers.cpp:352-375`,
original at `0x1d3af0`).

**Resolution**: full disassembly of `0x1d3af0..0x1d3dd0` (subagent
report, this session) showed the binary does the following:

1. Outer loop walks **node's parent chain** via the
   `parent` weak_ptr at `+0xF0/+0xF8` (locks at `0x1d3b23` for the
   initial parent and `0x1d3c3d` for each subsequent
   `parent->parent`).  No `next` (sibling-chain) walk exists.
2. Per-iteration gate at `0x1d3b6d` is
   `cmpl $0x8, 0x44(%r13)` — i.e. `parent->type == NodeType::Loop`
   — **not** `node->type == SetVar`.  The caller
   (`prepareTree`'s `NodeType::SetVar` case) is trusted to pass a
   SetVar node; the function gates on the *ancestor* being a Loop.
3. When the gate fires, `allocate_shared<Node>(SetVar, asmId,
   config_->numChannelGroups)` at `0x1d3ba1` produces a thin clone;
   only `lengthReg` (`+0x88`) is copied from the original at
   `0x1d3baa..0x1d3bb2`.  The clone is then conditionally spliced
   via `Node::insertBefore` at `0x1d3cf2` — but only when
   `parent->loop.__ptr_ == current_child.get()`
   (cmp at `0x1d3bc0`).  This guard ensures we only insert in
   front of a Loop ancestor's body **head**, not a mid-chain
   descendant.
4. The "scratch" slot `[rbx]/[rbx+8]` is the input shared_ptr's own
   stack storage, repurposed mid-function to track the current
   child as the walk advances upward (`mov [rbx], r13` at
   `0x1d3c09`); the original input is preserved in `[rbp-0x78]`.

The recon now matches semantics.  `nodeStates_` is intentionally
not updated for the clone — downstream lookups rely on
`operator[]` default-construction.

**Why tests still pass at 1602/1602 even before the fix**: no test
in the manifest currently exercises a SetVar nested inside a Loop
on a multi-channel-group device, so the missing splice was
invisible.  The fix is latent for the current corpus; ELF output
is unchanged.  A regression test that places a SetVar inside a
Loop on a multi-group HDAWG would exercise the new splice path.

## IF-219  `Prefetch::createLoad` missing already-loaded short-circuit

**Severity**: likely-bug.
**Status**: **fixed** (commit pending) — guard restored at
`prefetch.cpp:2215-2225`, block-header summary updated to match.
**Discovered**: D4 Batch 2d verify-then-write of
`Prefetch::createLoad` (`prefetch.cpp:2192-2279`, original at
`0x1d4a10`).

**Resolution**: confirmed by objdump (no GDB needed).  The
binary at `0x1d4a54-0x1d4aa0` (with continuation at
`0x1d4f51-0x1d4f8a`) implements an already-loaded guard that
the optimizer expanded inline from `weak_ptr::lock()`:

```
1d4a54:  mov  0x20(%rbx),%rdi              ; n->loadRef.__cntrl_
1d4a58:  test %rdi,%rdi                    ; cntrl null?
1d4a5f:  je   1d4aac                       ; → dummy check (no guard)
1d4a61:  call __shared_weak_count::lock    ; rax = locked cntrl or null
1d4a66:  test %rax,%rax
1d4a69:  je   1d4aa5                       ; expired → reload n, dummy check
1d4a6b:  cmpq $0,0x18(%rbx)                ; n->loadRef.__ptr_ (defensive)
1d4a70:  je   1d4f51                       ; __ptr_ == 0 → release, continue
1d4a76..1d4aa0:                            ; __ptr_ != 0 → release, return null
```

The recon body now reads:

```cpp
if (auto loaded = n->loadRef.lock())
    return result;
```

which is the idiomatic source-level equivalent.  The extra
`__ptr_ == 0` check in the binary is defensive: a `weak_ptr`
never zeroes its `__ptr_` field on expiration (only the strong
count is decremented), so under normal use `lock()` returning a
non-null `shared_ptr` implies `__ptr_` is non-null.  The
defensive re-check is the compiler's lowering of
`shared_ptr::operator bool` on the inlined temporary, not a
source-level safety net.

**Why tests still pass at 1602/1602**: the bug was latent on
the current call graph (single-pass `preparePlays` only invokes
`linkLoad` once per node, so duplicate loads never had a chance
to form on the test corpus).  The guard now ensures re-entry
safety for any future caller that consults `loadRef` after a
prior pass attached a load.

**Related stale labels in the legacy block-header** (now also
corrected in the same edit):

- `0x200` had been called "SetPlay" at several block-header
  lines; the verified `NodeType` enum has `0x200 = Table` and
  `SetVar = 0x10`.  Same Play↔Load swap pattern as IF-215.
- The block-header documented `assignLoad(this, node,
  loadNode, isHirzel)` (4 args); the actual call is
  `assignLoad(node, result, isHirzel)` (3 args).
- The block-header said "loadTargets_ (+0xA0)"; the field is
  `Node::play` (verified at `node.hpp:104`).
- The block-header said "mark waveform as not fixed (+0xD8 =
  false)"; the body sets `wfm->markedForLoad = true` at
  `prefetch.cpp:2270` — the inline comment correctly notes this
  is *not* the `fixed_` flag; the contradiction was purely with
  the (now-rewritten) block-header summary.

## IF-220  Block-header & inline-comment drift across batch 2d Prefetch helpers

**Severity**: cosmetic (purely descriptive; no code-behaviour
impact).
**Status**: fixed in same commit as discovery.
**Discovered**: D4 Batch 2d verify-then-write of the 13
tree-rewrite helpers in `prefetch.cpp` and
`prefetch_helpers.cpp`.

Cluster of stale comment / block-header drift.  Each individually
is small; logged together because they share the same root cause
(secondary-source rot from earlier rename / enum-correction
passes — same family as IF-212 and IF-215).

Sites fixed in this commit (all comment-only edits, no body
changes):

| File:Line | Was | Now |
|---|---|---|
| `prefetch.cpp:65` | "For SetVar (type==0x02) nodes" | "For Play (`NodeType::Play`, 0x02) nodes" |
| `prefetch.cpp:2151,2161` | "+0x20 offset" / "+0x28 offset" for `registerHirzel`/`registerCervino` | "+0x00 / +0x08 per `prefetch.hpp:187-188`" |
| `prefetch.cpp:2289-2291` | "if all were merged, removes the source" | "unconditionally removes the source after the loop" |
| `prefetch.cpp:2306-2309` | "for each weak_ptr in dst->play, check parent chain back to src" | rewritten to describe the actual `srcTargets` iteration |
| `prefetch_helpers.cpp:439` | "PNS.playSize() (+0x3C from hash node, PNS+0x0C)" | "PNS.pagesNeeded (+0x3C, PNS+0x1C)" |
| `prefetch_helpers.cpp:495` | inline `// PNS+0x1C (hash_node+0x3C)` | unchanged (already correct) |
| `prefetch_helpers.cpp:510` | "is a type-1 (Play) node" | "is a `Load` (type 1) node" |
| `prefetch_helpers.cpp:560` | "Push next pointer (+0xe0)" | "Push loop/else-branch (+0xE0)" |

The body of each function was verified line-by-line against
`reconstructed/include/zhinst/ast/node.hpp:44-117` (NodeType +
Node layout) and `reconstructed/include/zhinst/codegen/prefetch.hpp:186-203`
(PrefetcherNodeState).  No behavioural divergence beyond what is
tracked in IF-217 / IF-218 / IF-219.

## IF-221  `Prefetch::placeCommands` mislabels Loop as "Placeholder"

**Severity**: cosmetic.
**Status**: fixed in same commit as discovery.
**Discovered**: D4 Batch 2e-i verify-then-write of
`Prefetch::placeCommands` (`prefetch_emit.cpp:130-203`,
original at `0x1d6680`).

The block-header at `prefetch_emit.cpp:135` and the inline
comment at `prefetch_emit.cpp:154` describe the leading-entry
skip as "type-4 (placeholder) entries".  The body literally
checks `static_cast<int>(insertPos->node->type) == 4`, which is
`NodeType::Loop` per the verified enum
(`reconstructed/include/zhinst/ast/node.hpp:48`).
`NodeType::Placeholder` is `0x100000`, a distinct value.

Same Play↔Load / Wait↔Lock confusion family as IF-212 / IF-215 /
IF-216, just for a different bit.  Fixed by relabelling both
the block-header and the inline comment as `NodeType::Loop`
(0x08).

## IF-222  `Prefetch::placeSingleCommand` file-header NodeType label drift

**Severity**: cosmetic.
**Status**: fixed in same commit as discovery.
**Discovered**: D4 Batch 2e-i verify-then-write of
`Prefetch::placeSingleCommand`
(`prefetch_placesingle.cpp:67-1132`, original at `0x1d7940`).

The file-level summary comment lists the high-NodeType cases
the function dispatches on, with three errors per the verified
`NodeType` enum:

- `0x200` is labelled "Sync (Load/Play)" — actual value is
  `NodeType::Table`.  The body at line 1024 dispatches Table
  semantics (locks `loadRef`, allocates registers, encodes a
  CWVF immediate).  Inline comment at line 1023 ("`nodeType ==
  0x200: Sync (Load/Play)`") repeats the same wrong label.
- `0x8000` is labelled "CWVF-store" / "Store/Reset node" — actual
  value is `NodeType::AwgReady`.  Body emits a single
  `st(R0, 0x92)` consistent with AwgReady-set semantics.
- `0x4000` is `NodeType::PlainLoad` and is handled by the body
  at lines 1076-1118 but is **omitted** from the file-header's
  case-attribution table.

Fixed by rewriting the file-header table to list every case in
source order with the verified enum names (Load, Play, Branch,
Loop, SyncCervino, Table, SyncHirzel, PlainLoad, AwgReady) and
correcting the inline `0x200` and `0x8000` comments.

## IF-223  `Prefetch::placeSingleCommand` `case 0x200 (Table)` body is partially stubbed

**Severity**: likely-bug (stub).
**Status**: **fixed** (all three sub-paths reconstructed: A, B, and
C with both C-non-split and C-split branches).  See sub-path table
below for the verified structure.  The C-split `wprf` emission is
flagged `\verifyme` pending IF-244 action item 3
(`emitPrfEpilogueAndInsert_` helper extraction) — currently emitted
unconditionally where the binary gates on `!isHirzel` at
`0x1db935`.  Tests remain 1602/1602: corpus does not exercise
sub-path C.
**Discovered**: D4 Batch 2e-i verify-then-write of
`Prefetch::placeSingleCommand`
(`prefetch_placesingle.cpp:1024-1063`, original at `0x1d7940` /
case dispatch at `0x1d7a5b`).

### Verified structure (per consolidated objdump audit)

The IF's original "smap + ssl loop + addr + prf" hypothesis was
wrong about both **scope** and **pattern**.  The Table case
actually has **three sub-paths**, all converging at `out->insert`
at `0x1da6df` (the parallel epilogue, distinct from the
Play-case `play_finalize` at `0x1dba0d`):

| Sub-path | Gate | Range | Emits |
|---|---|---|---|
| **A** cachePtr empty | `loadState->cachePtr->size_ == 0` | `0x1d8b3c..0x1d932b` → jmp `0x1da6df` | `addi(reg2,zero,encodedCwvf)` + `smap(reg1,reg2,0x400\|tableIndex)` |
| **B** cachePtr non-empty, no lengthReg | `cachePtr.size_!=0 && (!lengthReg.isValid() \|\| lengthReg==R0)` | falls through `0x1d94a5..0x1da6df` | A + `smap(reg1,stateReg,tableIndex)` + `addi(reg2,zero,totalSize)` + `smap(reg1,reg2,0x800\|tableIndex)` where `totalSize = wfir->getSizePerDevice() / (isHirzel ? 1 : pagesNeeded)` and `stateReg = isHirzel ? registerHirzel : registerCervino` |
| **C** cachePtr non-empty, lengthReg valid | `cachePtr.size_!=0 && lengthReg.isValid() && lengthReg!=R0` | jmp `0x1daed4..0x1db740` | At `0x1daed4` the gate is `cmpb $1, [r15+0xbc]; jne 0x1db749`. **C-non-split** (`split_ != 1`, `jne` taken): re-converges with sub-path B at `0x1da56e` with `totalSize = 2*length*numChannels` (computed at `0x1db820..0x1db823`). **C-split** (`split_ == 1`, fall-through): emits `addi(idxReg, lengthReg, 0)` + per-channel `ssl(idxReg)` loop bound by `numChannels` + `addr(idxReg, registerHirzel\|Cervino)` + `prf` + `wprf`, same totalSize formula. (Polarity verified by IF-241; prior labelling swapped C1/C2.) |

Window correction: the IF originally cited `0x1d8b3c..0x1dba0d`
as the Table tail.  In reality that window is only ~20 % Table
code (the inlined `encodeCwvf` at `0x1d89d6..0x1d8b9c` plus the
smap-triplet at `0x1d932b..0x1da6df`); the rest is interleaved
Play-case code.  The Table-exclusive ssl/addr/prf/wprf region
lives at **`0x1daed4..0x1db740`** (single xref from `0x1d949f`).

### Resolution (sub-paths A and B)

Reconstructed at `prefetch_placesingle.cpp:1083-1227` (commit
pending).  Verified APIs:

- `smap(AsmRegister r1, AsmRegister r2, int value)` returns
  `std::vector<AsmList::Asm>`; `r1` is the scratch/key register
  loaded with the immediate, `r2` is the value register stored.
- `PlayConfig::encodeCwvf(int defaultRate)` packs the cwvf word;
  `defaultRate = (config.rate < 0) ? node->globalRate : config.rate`
  per binary `0x1d8a35..0x1d8a3c` sign-test + cmov.

Pre-existing recon bugs that were **also fixed** in this pass:

1. **Fabricated `>=0x1000000` cwvf-vs-cwvfr branch** at the old
   lines 1088-1098.  The Table case has no such fallback; it
   always emits `addi(reg2, R0, Imm(encoded))`.  The fabricated
   branch was conflating the Table path with the Play
   `cervino_nonsplit` path at `0x1d8087..0x1d8157`.
2. **Wrong `defaultRate` argument** at the old line 1084:
   `PlayConfig::encodeCwvf(npSync->config, npSync->globalRate)`
   unconditionally passed `globalRate`.  Should be the ternary
   above.  (Tests still pass because `encodeCwvf` internally
   clamps negative `defaultRate` to 0, so for `rate >= 0` cases
   not yet exercised in the corpus the output happens to match.)

### Deferred (sub-path C)

Sub-path C (lengthReg valid && != R0, branching to `0x1daed4`)
is **not** reconstructed.  The current code falls through to
`out->insert` after only emitting smap #1, which is wrong but
matches sub-path A's output for now and produces a `\verifyme`
marker in the source.

Reasons for deferral:
- The split-tail at `0x1db562..0x1db60a` partially shares code
  with `play_cervino_indexed_nonsplit` (recon at
  `prefetch_placesingle.cpp:884-908`) — needs a separate
  factoring pass before reconstructing.
- Five open questions remain (see below) that need GDB
  verification on table-using test inputs before committing
  code.

### Why tests pass at 1602/1602 even with sub-path C deferred

The test corpus does not exercise `playWaveTable` with both a
populated wavetable cache AND a valid per-channel length
register.  Tests using `playWaveTable` either have empty caches
(hits sub-path A) or invalid/zero `lengthReg` (hits sub-path B).
Adding a regression test for sub-path C is a follow-up item.

### Open questions to resolve before sub-path C

1. **Sub-path C1 totalSize formula**: binary computes
   `2 * node->length * waveform.numChannels` at `0x1daee5`,
   `0x1daffa`, `0x1daf1d`.  GDB-verify on a table test before
   coding.
2. **`bgReg` allocation purpose**: at `0x1daf5c` a fresh register
   is allocated and bound via `addi(bgReg, lengthReg, 0)`, then
   used as input to `ssl(bgReg)`.  Need to trace the
   `0x1db82b → 0x1dbc7d → 0x1dbba9` chain to confirm what comes
   after the ssl loop.
3. **`awgCfg_+0xC` field identity**: the comparison at `0x1db223`
   `cmp 0xc(%rcx),%eax` (where `rcx = awgCfg_`, `eax = cachePtr->size_`)
   gates the alternate prf encoding.  Field unknown — plausibly
   `awgCfg_->minIndexedSize` or `cacheLineSize`.
4. **C2-tail / play_cervino_indexed_nonsplit factoring**: the
   block at `0x1db562..0x1db60a` and the wprf at
   `0x1db92e..0x1db952` appear shared between Table-C2 and Play
   cervino_indexed_nonsplit (recon line 884-908).  Cross-check
   xrefs and gating before factoring into a helper.
5. **`-0x118(%rbp) optional<string> engaged flag` writes**: the
   `cmpb $1, -0x118(%rbp)` checks throughout the C path are
   cosmetic (gating the opt's heap-data destructor) and have no
   instruction-emission impact.

Each open question is mirrored in this file as a separate IF
entry (IF-241 .. IF-245) for individual triage.



## IF-224  `Prefetch::placeSingleCommand` `play_cervino_indexed_nonsplit` label is a stub

**Severity**: likely-bug (stub).
**Status**: **fixed** (commit pending) — body reconstructed at
`prefetch_placesingle.cpp:863-913` from full disassembly of
`0x1db562..0x1db60a`.  The implementation is much smaller than
the IF originally suggested.
**Discovered**: D4 Batch 2e-i verify-then-write of
`Prefetch::placeSingleCommand`
(`prefetch_placesingle.cpp:862-868`, original at `0x1db562`).

**Resolution**: confirmed by objdump (no GDB needed).  The
labelled block emits exactly two `AsmList::Asm` entries into
`tempList` and then falls through to `play_finalize`:

```
[0x1db562..0x1db5b0] load asmCommands_ / 3× nodeStates_[node] expand
[0x1db5cc..0x1db5d8] cacheSize = cachePtr->size_; cacheSize >>= 1
[0x1db5dd]          clamped = clampToCache(cacheSize)
[0x1db5e2..0x1db605] prf(registerHirzel, registerCervino, clamped),
                     tempList.append(prf)
[0x1db60a]          jmp shared-tail 0x1db911
[0x1db918..0x1db952] dtor prf-temp, then wprf() & tempList.append(wprf)
[0x1db952 → play_finalize]
```

The IF's earlier conjecture about a "wwvf + ssl loop + addr"
trio at this label was wrong — those instructions are emitted
earlier in `placeSingleCommand` (before the dispatch reaches
this label) and the label itself is only the prf+wprf finalize
epilogue.  The block-header comment in the recon source has
been corrected accordingly.

**Why tests still pass at 1602/1602 even before the fix**: same
coverage gap as IF-223 — the test corpus does not exercise the
exact combination of `!isHirzel && !split_ && lengthReg.isValid()
&& pagesNeeded < 2 && indexed` that routes through this label.
ELF output is unchanged.

## IF-225  `Prefetch::getUsedChannels` block-header mislabels reduced field

**Severity**: cosmetic.
**Status**: fixed in same commit as discovery.
**Discovered**: D4 Batch 2e-iii verify-then-write of
`Prefetch::getUsedChannels`
(`prefetch_helpers.cpp:178-201`, original at `0x1df2f0`).

The block-header at lines 182-184 says the function "reads
`channelMask` at +0x08 (uint32_t), inverts it (~mask), and ORs
into the accumulator".  The body actually reads
`entry.config.suppress` (`PlayConfig+0x08` per
`reconstructed/include/zhinst/waveform/play_config.hpp:41`),
not `channelMask` (`PlayConfig+0x00`, line 39).  The binary
comment at line 194 ("`mov 0x8(%rdx),%esi` — reads
`PlayConfig+0x08 = suppress`") agrees with the body and
contradicts the block-header.

The function name (`getUsedChannels`) reinforces the
field-name confusion in the header but is itself a contract,
not an offset claim — the function returns the OR-reduction of
inverted `suppress` masks across `usageEntries_`, which
encodes which channels are *not* suppressed, hence "used".

Same anti-pattern as the IF-220 cluster (block-header drifted
out of sync with the body).  Fixed by rewriting the
block-header to name `suppress` and to explain that the
inverted-OR reduction yields the union of channels that were
not suppressed by any entry.

## IF-226  `Prefetch::getUsedCache` body is a stub

**Severity**: likely-bug (stub).
**Status**: **fixed** (commit pending) — body reconstructed
from binary disassembly, replacing the unconditional `return 0`.
**Discovered**: D4 Batch 2e-iii verify-then-write of
`Prefetch::getUsedCache`
(`prefetch_helpers.cpp:806-820`, original at `0x1c7eb0`).

The original stub was:

```cpp
int Prefetch::getUsedCache(std::shared_ptr<Node> node) const {
    (void)node;
    return 0;
}
```

**Correction to the original IF**: the "only caller is
`Prefetch::print` (debug-only)" claim was **wrong**.
`getUsedCache` is also called from the `mergeWaveforms`-adjacent
loop at `prefetch.cpp:1112,1118` (binary `0x1cdcfd-0x1cddab`),
which debits `nodeStates_[current].usedCache_` by the returned
value:

```cpp
int actualUsed = getUsedCache(current);
if (usedCache >= actualUsed) {
    int used2 = getUsedCache(current);
    nodeStates_[current].usedCache_ -= used2;
}
```

The stub returning 0 made the gate `usedCache >= 0` trivially
true (so the branch always fired) and the subtraction
`usedCache_ -= 0` a no-op, silently dropping the cache
accounting.  No test failures were observed because the
production corpus apparently does not stress the path where
the `usedCache_` debit changes downstream allocation
decisions, but the bug was latent in production code, not
debug-only as originally claimed.

**Resolution**: confirmed by full disassembly of `0x1c7eb0..0x1c8738`
(no GDB needed).  The function is a simple recursive walk
over `Node::next` (+0xB8), `Node::loop` (+0xE0), and
`Node::branches` (+0xC8..+0xD0), with a per-node leaf
contribution that depends on `Node::type` and the prefetcher
state:

| Case | Contribution |
|---|---|
| `Load` + `length != 0` | `(length * channels) / pagesNeeded` (**signed** `idivl` at `0x1c83db`) |
| `Load` + `length == 0` | `-computeWaveformMemoryBytes(wfm) / pagesNeeded` (unsigned `divl`; **negated** at `0x1c8664`) |
| `Play` + `state != 0` + `length != 0` | `(length * channels) / pagesNeeded` (signed) |
| `Play` + `state != 0` + `length == 0` | `+computeWaveformMemoryBytes(wfm) / pagesNeeded` (unsigned, **no** negation) |
| `Play` + `state == 0` (Loaded) | `0` |
| All other `NodeType` | `0` |

where `wfm = wavetableIR_->getWaveformByName(node->wavesPerDev[node->deviceIndex])`
(empty optional when `deviceIndex < 0`), and `pagesNeeded =
nodeStates_.at(node).pagesNeeded` (PNS+0x1C).  Both
`nodeStates_.at(...)` lookups in the Play path are kept
distinct, matching the binary's double-`find` pattern
(`0x1c8241` for the state gate, `0x1c83c2`/`0x1c848c` for
`pagesNeeded`).

`computeWaveformMemoryBytes` was inlined by the compiler at
`0x1c843a..0x1c8485` / `0x1c855d..0x1c8616`; the reconstruction
likewise inlines it in `getUsedCache` rather than reach into
the file-local static defined in `prefetch_emit.cpp:249`,
keeping the TU self-contained.

**Negation semantics**: only the `Load + length == 0` path
negates the contribution.  This is a *refund* of the cache
quota for a Load that has been emitted but has not yet had a
length assigned — the cache pages it reserved have not yet
been consumed and are returned to the pool.  The stub
returning 0 silently masked this refund path entirely; tests
still pass because the difftest corpus does not stress
incrementally-sized waveforms heavily enough to expose
mis-balanced cache accounting.

Tests remain 1602/1602.  No regression test was added — the
function's effect is on internal `usedCache_` state, not on
the differential ELF output, so a targeted unit test would
need to inspect the prefetcher state mid-pass.  Worth
revisiting if stress tests in `manifest-stress.json` ever
fail with cache-allocation errors.

---

## IF-227  `WavetableIR::size()` recon body silently divides by 16

**Severity**: likely-bug (latent — function has no callers).
**Status**: fixed in same commit (D4 Batch 3a).
**Discovered**: D4 Batch 3a verify-then-write of the
`WavetableIR` accessors (`wavetable_ir.cpp:194-197`,
original at `0x29e290`).

The recon body was:

```cpp
size_t WavetableIR::size() const {
    return (manager_->waveforms_.end() - manager_->waveforms_.begin())
         / sizeof(std::shared_ptr<WaveformIR>);
}
```

`vector<T>::end() - vector<T>::begin()` returns the element
count (a `difference_type`), not a byte distance.  Dividing
that count by `sizeof(shared_ptr<WaveformIR>)` (= 16) yields
`count/16`, which is wrong for any waveform list that is not
a multiple of 16 entries — and zero for any list shorter
than 16.

The original binary at `0x29e290` performs the equivalent
computation at the *raw libc++ vector internals* level:

```
mov    0x70(%rdi),%rcx        ; rcx = manager_
mov    0x38(%rcx),%rax        ; rax = waveforms_._end (T*)
sub    0x30(%rcx),%rax        ; rax -= waveforms_._begin (T*)
sar    $0x4,%rax              ; rax >>= 4 (divide by sizeof(T)=16)
```

The `sub` produces a *byte* distance because the operands
are raw pointer values; `sar $0x4` then divides by
`sizeof(shared_ptr<WaveformIR>) = 16` to recover the element
count.  The correct C++-level expression is simply
`manager_->waveforms_.size()`.

**Why tests still pass at 1600/1600**: a repo-wide grep for
`WavetableIR::size()` (or any indirect call site) returns no
hits.  The accessor exists in the public interface but has
no live callers in the recon, so the divide-by-16 has no
observable effect on differential output.

**Fix**: replaced the body with `return
manager_->waveforms_.size();` in the same commit that added
the `\brief` for `size()` (D4 Batch 3a).  No new test added
because the function is dead code in the current pipeline;
if a future reconstruction wires it into a live caller the
brief alone documents the intended semantics.

## IF-228  Pervasive integer-literal magic numbers in reconstructed sources

**Severity**: cosmetic (readability / doc-accuracy hazard).
**Status**: open.
**Discovered**: D4 Batch 4e while writing briefs for
`playAuxWave`, `playDIOWave`, `playWaveDIO`, `playWaveZSync`,
`playZero`, `playHold` in
`reconstructed/src/runtime/custom_functions_playback.cpp` and
the wave-index built-ins in
`reconstructed/src/runtime/custom_functions_dio.cpp`.

Many reconstructed call sites still carry raw hex/decimal
literals inherited from disassembly, even though the same
value has a name elsewhere in the tree (or could trivially be
given one).  Categories observed so far:

1. **Device-support bitmasks** passed to
   `checkFunctionSupported`.  `core/types.hpp:32-43` already
   exports `kDevAll`, `kDevAllButUHF`, `kDevHirzel`,
   `kDevSHFPlus`, `kDevLIFamily`, `kDevCervino`, `kDevUHF`,
   `kDevPreSHFLI`, `kDevQA`, `kDevHirzelAll`,
   `kDevHirzelPlusUHFQA`, `kDevNone`.  Yet
   `custom_functions_playback.cpp` repeatedly writes
   `static_cast<AwgDeviceType>(5)` (= `kDevCervino`) for
   `playWaveNow`, `playWaveIndexed`, `playWaveIndexedNow`,
   `playAuxWaveIndexed`, `playWaveDigTrigger`, `playAuxWave`,
   `playDIOWave`, `now`, and `setRate`.

2. **Trigger-mask defaults**: `0x3FFF` (full 14-bit mask) and
   `0x3FC3` (aux-merge variant) appear inline in
   `playAuxWave`, `playDIOWave`, `assignWaveIndex`, `play`.
   The per-channel byte-clear pattern
   `mask &= ~(0x40 << (7 * b))` recurs in `playDIOWave` and
   `assignWaveIndex` with the same semantics.

3. **ZSYNC shift constants**: `1`, `9`, `0xD` for
   `ZSYNC_DATA_RAW`, `_PROCESSED_A`, `_PROCESSED_B` in
   `playWaveZSync` (`custom_functions_playback.cpp:777, 783,
   789`).  These are bound to the matched constant's name in
   prose but live as bare ints in the code.

4. **Sentinel registers**: `AsmRegister(-1)` is used as the
   "unused second register" slot in every `asmPlay` call site
   (`playAuxWave`, `playDIOWave`, `playZero`, `playHold`).

5. **Rate-validity floors**: `playAuxWave` rejects
   `rate <= 4`, `playDIOWave` rejects `rate <= 1`.  Both are
   bare literals with no shared symbol.

6. **PlayConfig encoding**: per-field shift/mask constants
   inside `PlayConfig::encodeCwvf` / `genPlayConfig` are
   currently raw literals; not yet audited for promotion to
   named members of `PlayConfig` itself.

7. **`wvft` reg-zero immediate**: `AsmRegister(0)` as the
   first arg of every `wvft(…)` call site
   (`playWaveDIO`, `playWaveZSync`, `executeTableEntry`).

**Why this is filed as cosmetic**: the literals are
binary-faithful — every value matches the original
disassembly — and there is no behavioural bug.  The cost is
borne by readers and by D4 doc-comment accuracy: briefs end
up quoting the literal (e.g. the `playAuxWave` brief
references three separate trigger-mask hex values), which
then drifts whenever the constant is renamed.

**Risk if left open**: a future reconstruction pass renames a
literal in one site without updating its sibling sites,
producing silent semantic drift.  Doc briefs that quote the
literal also rot independently of the call site they
describe.

**Suggested fix scope** (NOT promoted to TODO.md without
user agreement):

- E1: replace `static_cast<AwgDeviceType>(N)` arguments to
  `checkFunctionSupported` with the matching `kDev*` alias
  from `core/types.hpp`.  All required aliases already exist;
  no new constants needed.  Easiest sub-task; expected to
  close ~30 call sites.
- E2: introduce named constants for the trigger-mask
  defaults (`kPlayTriggerMaskFull = 0x3FFF`,
  `kPlayTriggerMaskAuxMerge = 0x3FC3`) and a helper
  `clearChannelTriggerByte(mask, bit)` for the
  `0x40 << (7*b)` pattern.
- E3: name the ZSYNC shift constants in `playWaveZSync`.
- E4: name `AsmRegister(-1)` and the rate-validity floors.
- E5: audit `PlayConfig` field encoding for promotion to
  named members.

**Verification requirement for any fix**: must be NFC at the
binary level — every replacement is a literal-for-name swap
that compiles to the same value.  Confirm by running the full
diff-test suite after each sub-phase and spot-checking ELF
byte output for a representative test.

**Anti-scope**: opcode bit positions inside the assembler
encoders are intentionally left as literals — they belong
next to the encoder body, not in a shared header, and are
already covered by the surrounding context.


## IF-229  `WaveformGenerator::call` brief overstates alias substitution

**Severity**: cosmetic (doc-comment drift).
**Status**: fixed in D4 Batch 6a (this commit).
**Discovered**: D4 Batch 6a verify-then-write audit of
`waveform_generator.cpp:378-408` against the existing
`/*! \brief … */` block at
`waveform_generator.hpp:218-244`.

The pre-existing `\details` text claimed:

> 1. Look `name` up in `aliasMap_`.  When the name is a
>    deprecated alias for a current built-in, format the
>    `DeprecatedFunc` warning through `warningCallback_`
>    and substitute the canonical name for the lookup
>    that follows.

The reconstruction body (and the disassembly at
`0x25c1c8..0x25c20b` which keeps the original `%r14 = name`
across the warning path and uses it as the funcMap_ lookup
key) does **not** substitute.  The alias map serves only to
emit the deprecation warning; the funcMap_ lookup uses the
**original** requested name.

Both deprecated names that exist in `aliasMap_` (`"mask"`
and `"rand"`) also have entries in `funcMap_` registered by
the ctor (lines 133, 135 of `waveform_generator.cpp`), so
the dispatch goes to the deprecated implementation while
the user is warned to migrate.  This is the binary-faithful
behaviour and it is what the test suite covers.

**Fix applied in same commit**: rewrote the brief to reflect
the actual two-step flow (warn-on-alias, then look up the
*original* name; throw on funcMap_ miss).

## IF-230  `WaveformGenerator::rrc` 3-arg path used wrong `readDouble` parameter labels

**Severity**: cosmetic (user-visible error-message text;
binary-faithfulness regression but no test currently
exercises the 3-arg overload's error paths).
**Status**: fixed in D4 Batch 6b (same commit as the brief
that surfaced it).
**Discovered**: D4 Batch 6b verify-then-write audit of
`waveform_generator_dsp.cpp:1117-1200` against the binary
disassembly of `WaveformGenerator::rrc` at 0x254290.

The reconstruction of the 3-argument path
(`rrc(length, position, beta)`) labelled the two `readDouble`
calls as:

```cpp
position = readDouble(args[1], "2 (position)", "rrc");
beta     = readDouble(args[2], "3 (position)", "rrc");
```

The second label was an obvious copy-paste typo (`"3 (position)"`
duplicating the position string for what is actually `beta`).
However, audit of the binary showed that **both** labels were
wrong, not just the typo:

The binary uses exactly two parameter-name strings throughout
all three arity paths (3-arg, 4-arg, 5-arg):

| readDouble site | label loaded into rax (movabs) |
|---|---|
| 0x254a89 (5-arg position)  | `"3 (position)"` |
| 0x254bac (4-arg position)  | `"3 (position)"` |
| 0x254cd5 (3-arg position)  | `"3 (position)"` |
| 0x254dfa (5-arg beta)      | `"4 (beta)"`     |
| 0x254f19 (4-arg beta)      | `"4 (beta)"`     |
| 0x25500b (3-arg beta)      | `"4 (beta)"`     |
| 0x25512e (5-arg width)     | `"5 (width)"`   |

The strings are hardcoded inline as 8-byte SSO short-string
immediates (e.g. `0x7469736f70282033` = `"3 (posit"`) and do
**not** track the actual zero-based argument index in the user
call.  This means in the 3-arg overload, the user gets an error
message referring to "argument 3 (position)" for what they
passed as the second argument, and "argument 4 (beta)" for what
they passed as the third.  This is binary-faithful surprising
behaviour, not a binary bug we should "improve away".

**Fix applied in same commit**: changed the 3-arg path labels to
match the binary (`"3 (position)"` and `"4 (beta)"`) and added a
comment in the source pointing at this IF entry and the verified
addresses.

**Recon-faithfulness implication**: any future audit of error
messages emitted by `rrc` should expect the same label regardless
of overload.  No test currently exercises the 3-arg-path error
output, which is why the typo + miscount went undetected.

### Same pattern in `WaveformGenerator::sinc` (also fixed in this commit)

Cross-checking the other 3/4-arg overloads turned up the same
hardcoded-label-string pattern in `sinc` at 0x24b6e0.  The binary
has only **two** parameter-name strings for position and beta,
shared across both arities:

| readPositiveInt / readDouble site | label loaded into rax |
|---|---|
| 0x24bbcf (3-arg position)         | `"2 (position)"`      |
| 0x24bcf9 (4-arg position)         | `"3 (position)"`      |
| 0x24be20 (3-arg beta)             | `"3 (beta)"`          |
| 0x24bf33 (4-arg beta)             | `"3 (beta)"`          |

The reconstruction of the 4-arg path had:

```cpp
position  = readPositiveInt(args[2], "2 (position)", 2, "sinc");
beta      = readDouble(args[3], "4 (beta)", "sinc");
```

Both labels were wrong: the 4-arg path uses `"3 (position)"` (not
`"2 (position)"`) and `"3 (beta)"` (not `"4 (beta)"`).  Note in
particular that the binary uses the same `"3 (beta)"` literal in
both arities, even though the user-visible argument index for beta
is 3 in the 3-arg form and 4 in the 4-arg form — same arity-blind
hardcoding as `rrc`.  The 3-arg path was already correct.

Fix applied alongside the `rrc` fix.

### Likely scope of this pattern

Both functions affected so far (`rrc`, `sinc`) follow the convention
that the 3-arg overload's labels are taken as the canonical strings,
and the 4/5-arg overloads inherit them verbatim despite the
larger argument indices.  Other multi-arity factories
(`gauss`, `sin`, `cos`, `sawtooth`, `triangle`, `drag`,
`blackman`, `hamming`, `hann`, `chirp`, `rand`, `randomGauss`,
`randomUniform`) should be audited for the same kind of
mis-numbered labels in subsequent batches.  Audit method:
disassemble each entry point, decode all `movabs $0x...,%rax`
immediates that precede `read*` calls, compare strings to the
recon's `read*` literal arguments.

### Update (D4 Batch 6c, 2026-05-10)

Audited and fixed:

**`chirp` (0x250bb0)** — GDB-verified all 12 read-call sites
across 5/4/3-arg paths.  Recon used camelCase `"startFrequency"`,
`"stopFrequency"`, and `"phase"`; binary uses spaced and more
descriptive `"start frequency"`, `"stop frequency"`, and
`"initial phase"`.  Unlike `rrc`/`sinc`, chirp's labels are NOT
arity-blind — each path uses a label string with the correct
user-visible argument index built either via inline movabs SSO
immediates or via `lea rip-rel + movups xmm0` from rodata.
Verified strings (size_byte values match `(len<<1)`):

| Arity | Site     | Label                       |
|-------|----------|-----------------------------|
| 5-arg | 0x250f1b | `"1 (length)"`              |
| 5-arg | 0x251294 | `"2 (amplitude)"`           |
| 5-arg | 0x251605 | `"3 (start frequency)"`     |
| 5-arg | 0x2518b2 | `"4 (stop frequency)"`      |
| 5-arg | 0x2519d1 | `"5 (initial phase)"`       |
| 4-arg | 0x250df3 | `"1 (length)"`              |
| 4-arg | 0x251164 | `"2 (start frequency)"`     |
| 4-arg | 0x2514dc | `"3 (stop frequency)"`      |
| 4-arg | 0x2517f5 | `"4 (initial phase)"`       |
| 3-arg | 0x251040 | `"1 (length)"`              |
| 3-arg | 0x2513ba | `"2 (start frequency)"`     |
| 3-arg | 0x25172d | `"3 (stop frequency)"`      |

Fix: replaced 7 mismatched labels in `chirp` recon with the
binary strings; added per-call address comments.

**`lfsrGaloisMarker` (0x253bc0)** — single-arity (4 args), but
recon's 2nd label was `"2 (marker)"` while the binary uses
`"2 (markerBit)"` (size_byte=0x1a=26=(13<<1), GDB-confirmed).
Fix: one-line label change.

**`randomGauss` and `randomUniform`** — already correct (verified
by re-reading the existing block-header maps; randomGauss was
fixed in IF-205, randomUniform's labels are trivially `"1 (length)"`
and `"2 (amplitude)"` and match).

**`rand` 3-arg path** — discovered to be a *semantic* bug, not
just label drift.  Promoted to its own entry: see IF-231.

The remaining unaudited multi-arity factories (`gauss`, `sin`,
`cos`, `sawtooth`, `triangle`, `drag`, `blackman`, `hamming`,
`hann`) all have only the `length` arg as required and zero or
one optional helpers, so the surface area for label drift is
small; they will be re-audited in the brief-writing pass for
each function.

### Update (D-AUDIT-1 follow-up sweep, 2026-05-10)

Audited the remaining 11 multi-arity factories
(`gauss`, `sin`, `cos`, `sawtooth`, `triangle`, `drag`,
`blackman`, `hamming`, `hann`, `vect`, `placeholder`).  Per-
factory results:

- **`gauss`** — clean (already fixed under IF-232).
- **`drag`**, **`blackman`**, **`hamming`**, **`hann`**,
  **`vect`** (dynamic `to_string(i+1)+" (waveform)"`),
  **`placeholder`** — clean, no label drift.
- **`sin`** (0x24a0f0), **`cos`** (0x24abd0),
  **`sawtooth`** (0x24c8b0), **`triangle`** (0x24d330) —
  promoted to **IF-234** (semantic 3-arg parameter-binding
  bug, same shape as IF-231).  All four also had cosmetic
  4-arg label drift (`"3 (phase)"` → `"3 (phase offset)"`,
  `"4 (nPeriods)"` → `"4 (number of periods)"`); fixed in
  the same edit.

Audit method clarifications surfaced during this sweep
(fold into `D-AUDIT-1` recipe in TODO.md):

1. **Function end addresses**: derive from the `.text`
   symbol-table size (`objdump -t | grep <symbol>`), NOT
   from the next factory in a hand-curated list.  `cos`'s
   nominal range overlaps `sinc`; naively slicing
   `0x24abd0..0x24c8b0` picks up `sinc`'s call sites.
2. **Long parameter labels (≥16 chars)**: the binary uses
   `lea rip-rel + movupd %xmm0` (SSE2 packed-double load),
   not `movups` as the original recipe suggested.  Match
   `movup[sd]` (or just "16-byte SSE load from rip-rel
   rodata") in audit scripts.
3. **`objdump -d` rip-rel comments are unprefixed hex**
   (`# 905dc8`, no `0x`).  Audit regexes expecting `0x`
   after the comment marker silently miss them.
4. **`objdump -s` packs hex bytes into ~16 four-byte
   groups per line**.  A `(?:[0-9a-f]{2,8} ?){1,4}` cap on
   the byte-group repetition truncates the dump.  Use
   `{1,8}` or `+`.

Audit complete for D-AUDIT-1.

## IF-231  `WaveformGenerator::rand` 3-arg overload had wrong parameter binding (semantic bug)

**Severity**: likely-bug (semantic divergence; user-visible
sample values differ; analogous to IF-205 for `randomGauss`).
**Status**: fixed in D4 Batch 6c (waveform_generator_dsp.cpp
`rand` 3-arg signature).
**Discovered**: D4 Batch 6c verify-then-write audit of
`waveform_generator_dsp.cpp:880-935` against the binary
disassembly of `WaveformGenerator::rand` at 0x251cf0.
**GDB-verified**: 2026-05-10 with `rand(64, 0.25, 0.125)` on
HDAWG8 — only three `read*` sites fired in the 3-arg branch
(no `readDoubleAmplitude` call), with paramName SSO inline
bytes confirmed via `x/24bc $rdx`.

### Symptom (latent)

The reconstruction implemented the 3-arg overload as
`rand(length, amplitude, mean)` with `stddev` defaulting to
1.0:

```cpp
} else {  // 3 args   (recon, before fix)
    length    = readPositiveInt(args[0], "1 (length)", 1, "rand");
    amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "rand");
    mean      = readDouble(args[2], "3 (mean)", "rand");
    // stddev remains 1.0
}
```

The binary's 3-arg path is actually
`rand(length, mean, stddev)` with `amplitude` defaulting to
1.0 — the **second** argument, not the fourth, is the one that
defaults.  This is the same pattern as IF-205 for
`randomGauss`, which was fixed earlier; the symmetric bug in
`rand` slipped through because no test exercised the 3-arg
form.

### Binary evidence

Disassembly of `rand` at 0x251cf0 dispatches `cmp $0x4 → je
0x251d6e` (4-arg entry) then `cmp $0x3 → jne throw` and falls
through at 0x251d37 (3-arg entry).  Counting `read*` call
sites between 0x251cf0 and 0x252820 turns up **seven** total:

| addr     | callee               | size_byte | paramName              |
|----------|----------------------|-----------|-----------------------|
| 0x251ea1 | readPositiveInt      | 0x14      | `"1 (length)"` (4-arg) |
| 0x251fc4 | readPositiveInt      | 0x14      | `"1 (length)"` (3-arg) |
| 0x2520ec | readDoubleAmplitude  | 0x1a      | `"2 (amplitude)"` (4-arg only) |
| 0x252205 | readDouble           | 0x10      | `"2 (mean)"` (3-arg)   |
| 0x252321 | readDouble           | 0x10      | `"3 (mean)"` (4-arg, via `inc rax` from `"2 (mean)"`) |
| 0x25244d | readDouble           | 0x2c      | `"3 (standard deviation)"` (3-arg, lea rodata @0x905e92) |
| 0x25250e | readDouble           | 0x2c      | `"4 (standard deviation)"` (4-arg, lea rodata @0x905ea9) |

There is **only one** `readDoubleAmplitude` call site in the
entire function (0x2520ec), confirming that only the 4-arg
path reads amplitude from the user.  The 3-arg path skips it
and the binary loads the rodata constant 1.0 (@0x956030) into
the amplitude slot.

### GDB confirmation

```
>>> HIT readPosInt_3arg_len    size_byte=0x14  inline="1 (length)..."
>>> HIT readDouble_3arg_first  size_byte=0x10  inline="2 (mean)"
>>> HIT readDouble_3arg_second size_byte=0x2c  inline="3 (standard "
```

The `readDoubleAmplitude` breakpoint at 0x2520ec did **not** fire
during a 3-arg call, confirming amplitude is the defaulted slot.

### Fix

Swapped the 3-arg path to `(length, mean, stddev)`, defaulted
`amplitude = 1.0` instead of `stddev = 1.0`, and updated the
disassembly notes to reflect the verified call-site map.  Added
a new test case `random_waves_3arg` (HDAWG8) under
`manifest-documentation.json` that exercises both
`rand(128, 0.0, 0.25)` and `rand(128, 0.5, 0.0, 0.25)`; both
produce byte-identical waveforms vs the binary post-fix.

### Why IF-205 didn't catch this

IF-205 was discovered via a coverage-round test of
`randomGauss`, and its fix targeted `randomGauss` specifically.
The `rand` block-header summary (lines 873-878 pre-fix) inherited
the original-source layout `(length, amplitude, mean)` for the
3-arg form and was never re-verified against the binary.  This is
a textbook AGENTS.md "verify-then-write" failure — the 3-arg
summary was used as primary evidence for downstream reconstruction
when it should have been treated as a stale secondary source.

### Coverage gap closed

`hdawg_doc_random_waves_3arg.seqc` (added with this fix) is the
first test case that exercises the 3-arg `rand` overload.  Prior
to it, only the 4-arg form was covered (by
`hdawg_doc_random_waves.seqc` line 3 and stress
`wave_rand_oor.seqc`).

## IF-232  `WaveformGenerator::gauss` parameter labels and `readInt` vs `readPositiveInt`

**Severity**: cosmetic (user-visible error-message text;
binary-faithfulness regression but no test currently
exercises gauss's error paths).
**Status**: fixed in D-AUDIT-1 (D4 Batch 6 follow-up,
2026-05-10).
**Discovered**: D-AUDIT-1 sweep of remaining
`WaveformGenerator` factories (per the IF-230 audit
methodology) — disassembly of `gauss` at 0x24ddb0 shows
parameter-label movabs immediates and `read*` call targets
that diverge from the recon at
`waveform_generator_dsp.cpp:166-201`.

### Discrepancies

The recon used short, unprefixed labels and the wrong reader
for `length`; the binary uses indexed labels of the
`"<arg-index> (<param>)"` form and `readPositiveInt` for
`length`.  Per-call mapping below; binary call addresses are
GDB-/objdump-verified, parameter-label strings are decoded
from inline `movabs $<imm64>,%rax; mov %rax,off(%rbp)`
sequences plus the trailing `movw`/`movb` size-byte writes
(libc++ short SSO layout — see `notes/libcpp_abi.md`).

| recon (pre-fix) | binary | call site |
|---|---|---|
| `readInt(args[0], "length", 1, "gauss")` (4-arg) | `readPositiveInt(args[0], "1 (length)", 1, "gauss")` | 0x24df6b |
| `readInt(args[0], "length", 1, "gauss")` (3-arg) | `readPositiveInt(args[0], "1 (length)", 1, "gauss")` | 0x24e095 |
| `readDoubleAmplitude(args[1], "amplitude", "gauss")` | `readDoubleAmplitude(args[1], "2 (amplitude)", "gauss")` | 0x24e1c2 |
| `readDouble(args[2], "position", "gauss")` (4-arg) | `readDouble(args[2], "3 (position)", "gauss")` | 0x24e40c |
| `readDouble(args[3], "width", "gauss")` (4-arg) | `readDouble(args[3], "4 (sigma)", "gauss")` | 0x24e62c |
| `readDouble(args[1], "position", "gauss")` (3-arg) | `readDouble(args[1], "2 (position)", "gauss")` | 0x24e2e7 |
| `readDouble(args[2], "width", "gauss")` (3-arg) | `readDouble(args[2], "3 (sigma)", "gauss")` | 0x24e532 |

The most consequential discrepancy is the parameter name
`"width"` vs the binary's `"sigma"` — these are the same
mathematical quantity (Gaussian standard deviation) but
differently named in the user-facing error text.  The
`readInt` → `readPositiveInt` change is behaviourally
indistinguishable for valid inputs (since `min=1` already
rejects negatives in the inner `readInt`), but the error
template and call-target match the binary post-fix.

### Verification

`objdump -d --start-address=0x24df60 --stop-address=0x24df80
_seqc_compiler.so` shows `call 25d490 <readPositiveInt>`,
not `call 25cca0 <readInt>` (symbol addresses from
`objdump -t`).

`objdump -d --start-address=0x24e4f0 --stop-address=0x24e535
_seqc_compiler.so` decodes the `"3 (sigma)"` literal
construction:

```
24e4fa: movb   $0x12,-0x40(%rbp)              # size byte: 0x12 = 9*2 = 9 chars
24e4fe: movabs $0x616d676973282033,%rax       # "3 (sigma" little-endian
24e508: mov    %rax,-0x3f(%rbp)
24e50c: movw   $0x29,-0x37(%rbp)              # ")\0" finishes the 9-char string
```

Same pattern at 0x24e1bX for `"2 (amplitude)"`, 0x24e3D5 /
0x24e2A5 for `"3 (position)"` / `"2 (position)"`, etc.

### Fix

Updated `gauss` in `waveform_generator_dsp.cpp` to use
`readPositiveInt` for both arities, and replaced every
parameter label with the binary's exact string (renaming
the function-local `width` C++ variable left as-is — that
is internal and unaffected by the binary's user-facing
label).

### Why this wasn't caught earlier

`gauss` was reconstructed early (Phase 1-era), before the
verify-then-write workflow was formalised (which happened
during D4 Batch 2d-2e; AGENTS.md §"Verify-then-write").
The original block-header summary (`length`, `amplitude`,
`position`, `width`) was used as the source of truth for
the `read*` call-site labels rather than re-verifying
against the binary.  D-AUDIT-1 specifically targets this
class of stale-summary regression across the
`WaveformGenerator` factory surface.

### Audit findings: remaining factories clean

The D-AUDIT-1 sweep also re-checked `sin`, `cos`,
`sawtooth`, `triangle`, `drag`, `blackman`, `hamming`,
`hann`, `placeholder`, and `vect` (`vect` builds dynamic
labels via `to_string(i+1) + " (waveform)"`, no static
literals to compare).  All ten match the binary's
parameter-label strings and `read*` call targets exactly —
no further fixes required.  D-AUDIT-1 may now be closed
as complete.

## IF-233  `CompilerMessageCollection::parserMessage` does not set `hadError_` (binary-faithful asymmetry)

**Severity**: cosmetic (documentation / mental-model
correction; behaviour matches the binary).
**Status**: confirmed binary-faithful; recon `.cpp` already
matches the binary; one stale recon comment in
`compiler.cpp:303-308` corrected.
**Discovered**: D4 Batch 7b verify-then-write of
`compiler_message.hpp` briefs (2026-05-10).

### The asymmetry

`CompilerMessageCollection::errorMessage(msg, line)` and
`CompilerMessageCollection::parserMessage(line, msg)` both
funnel into `compilerMessage(Error, line, msg)`.  However,
**only `errorMessage` sets the sticky `hadError_` flag**;
`parserMessage` deliberately does not.  This is binary-
faithful and load-bearing — it is not a recon defect.

### Verification (objdump primary)

`errorMessage` at `0x12b720` (8 lines of relevant body):

```
12b72c: test   %edx,%edx               # if (line < 0)
12b72e: jns    12b733
12b730: mov    0x18(%rbx),%edx         #   line = lineNr_;
12b733: mov    %rbx,%rdi
12b736: xor    %esi,%esi                # type = Error (0)
12b738: call   12b750 <compilerMessage>
12b73d: movb   $0x1,0x1c(%rbx)         # hadError_ = true   <-- THE WRITE
```

`parserMessage` at `0x12ba30` (entire body, 5 instructions):

```
12ba34: mov    %rdx,%rcx                # rcx = msg
12ba37: mov    %esi,%edx                # rdx = line
12ba39: xor    %esi,%esi                # type = Error (0)
12ba3b: pop    %rbp
12ba3c: jmp    12b750 <compilerMessage> # tail-call
                                         # NO write to 0x1c(%rdi)
```

`hadError_` is at `+0x1c` per
`hadCompilerError` at `0x12ba80`:
```
12ba84: movzbl 0x1c(%rdi),%eax
```

So the absence of `movb $0x1,0x1c(...)` in `parserMessage`
is intentional: a parser-emitted error reaches
`messages_` (so it shows up in `getCompileReport`) but does
**not** trip `hadCompilerError()`.

### Why the asymmetry is load-bearing

The compile pipeline tracks parser errors through a
**separate** flag, `SeqcParserContext::hadSyntaxError_`,
set at `seqc_parser_context.cpp:106` from inside
`yyerror`.  `Compiler::parse()` consults
`ctx->hadSyntaxError()` immediately after `seqc_parse`
returns and throws `CompilerException("Syntax error
while parsing seqC")` (`compiler.cpp:175-177`).  This
short-circuit fires inside `parse()` at step 3 of
`Compiler::compile`, long before the
`messages_.hadCompilerError()` gate at step 9
(`compiler.cpp:338`) is ever reached.

So the two error-tracking mechanisms are intentionally
disjoint:
1. **Parser errors** — flagged on `SeqcParserContext`,
   trip `parse()`'s own `CompilerException`, never need
   to influence `messages_.hadError_`.
2. **AsmCommands / lowering / evaluation errors** —
   reported via `errorMessage`, tripping
   `messages_.hadError_`, gated at step 9.

Both surface their text through `messages_` for inclusion
in `getCompileReport()`, which is why `parserMessage`
still routes through the same de-dup'd vector.

### Stale comment in `compiler.cpp:303-308`

The fall-through narrative comment in `Compiler::compile`
(at the `if (seqcAst)` block guard) claims:

> "The parse error was already reported via
>  `parserContext_.errorCallback_` →
>  `messages_.parserMessage()`, so
>  `messages_.hadCompilerError()` will be true and step 9
>  will throw the right exception."

This is **wrong** on two counts:

1. `parserMessage` does not set `hadError_`, so
   `hadCompilerError()` will **not** be true after a
   parser error.
2. Step 9 is unreachable on a parser error anyway —
   `parse()` itself throws inside step 3 (verified at
   `compiler.cpp:175-177`), so control never reaches the
   `if (seqcAst)` block when there was a syntax error.

The block guard's actual purpose is to handle the
distinct **empty-input** case: the binary's libc++
`shared_ptr<Expression>` permits a null target where
libstdc++ would assert; the recon adds the explicit null
check so the lowering pass is skipped on empty source.
Step 9's `messages_.hadCompilerError()` gate exists for
post-parse errors (lowering, evaluation, AsmCommands).

### Fix

Comment in `compiler.cpp:303-311` rewritten to describe
the actual invariant (empty-input-protection rationale)
and to cross-reference IF-233 for the `hadError_`
asymmetry.  No source-behaviour change.  The
`compiler_message.hpp` brief for `parserMessage` carries
a `\binarynote` cross-referencing IF-233.

### Why this matters

Without IF-233 documented, future agents reading the
`hadCompilerError()` getter or `parserMessage` body would
reasonably assume the asymmetry is a recon bug and "fix"
it by adding `hadError_ = true` to `parserMessage`.  Such
a "fix" would diverge from the binary and could cause
post-parse phases to short-circuit on parser-only errors
that the binary intentionally lets fall through to the
`parse()`-internal `CompilerException`.

## IF-234  Trig-family 3-arg overloads (`sin`/`cos`/`sawtooth`/`triangle`) had wrong parameter binding (semantic bug)

**Severity**: likely-bug (semantic divergence; user-visible
sample values differ; analogous to IF-205 / IF-231).
**Status**: fixed in D-AUDIT-1 follow-up
(waveform_generator_dsp.cpp `sin`/`cos`/`sawtooth`/`triangle`
3-arg paths).
**Discovered**: D-AUDIT-1 sweep of remaining
`WaveformGenerator` factories (per the IF-230 audit
methodology) — disassembly of the four trig factories at
0x24a0f0, 0x24abd0, 0x24c8b0, 0x24d330 shows only **one**
`readDoubleAmplitude` call site per function (the 4-arg
branch), and the 3-arg branch reads `args[1]` with label
`"2 (phase offset)"` and `args[2]` with label
`"3 (number of periods)"`.
**GDB-verified**: 2026-05-10 with `sine(64, 0.5, 2.0)` /
`cosine(64, 0.25, 1.0)` / `sawtooth(64, 0.0, 1.5)` /
`triangle(64, 0.125, 0.5)` on HDAWG8 — for each call only
two `readDouble` breakpoints fired (`*_phase` and `*_nper`),
the corresponding `readDoubleAmplitude` breakpoint did not
fire.  Subagent ran the trace; see session log.

### Symptom (latent)

The reconstruction implemented all four 3-arg overloads as
`f(length, amplitude, phase)` with `nPeriods` defaulting to
1.0:

```cpp
} else {  // 3 args   (recon, before fix — sin/cos)
    length    = readPositiveInt(args[0], "1 (length)", 1, "sine");
    amplitude = readDoubleAmplitude(args[1], "2 (amplitude)", "sine");
    phase     = readDouble(args[2], "3 (phase)", "sine");
    // pre-fix sin/cos then collapsed to a constant waveform:
    double value = std::sin(amplitude + phase);
    /* fill all samples with `value` */
}
```

For `sawtooth`/`triangle` the recon dispatched
`genericTriangle(length, nPeriods, phase, riseRatio,
amplitude)` after a register-shuffle rationalisation comment
*"binary swaps amp↔nPeriods and phase↔phase in registers"*.

The binary's 3-arg path is actually
`f(length, phase, nPeriods)` with `amplitude` defaulting to
1.0 — the **second** argument, not the third or fourth, is
the one that defaults.  This is the same pattern as IF-205
for `randomGauss` (fixed earlier) and IF-231 for `rand`
(fixed in D4 Batch 6c).  All four trig 3-arg forms slipped
through because no test exercised the 3-arg overload with
values that would diverge from the wrong-binding output.

### Binary evidence

For each of the four factories, the call-site map between
function entry and the next factory shows exactly:

| reader                | label                      | branch            |
|-----------------------|----------------------------|-------------------|
| `readPositiveInt`     | `"1 (length)"`             | 4-arg             |
| `readPositiveInt`     | `"1 (length)"`             | 3-arg             |
| `readDoubleAmplitude` | `"2 (amplitude)"`          | 4-arg only        |
| `readDouble`          | `"2 (phase offset)"`       | 3-arg             |
| `readDouble`          | `"3 (phase offset)"`       | 4-arg             |
| `readDouble`          | `"3 (number of periods)"`  | 3-arg             |
| `readDouble`          | `"4 (number of periods)"`  | 4-arg             |

The single `readDoubleAmplitude` call site per function
confirms that only the 4-arg path reads amplitude from the
user; the 3-arg path loads the rodata constant 1.0 into the
amplitude slot.  Per-factory call-site addresses:

- `sin`  @ 0x24a0f0 — `readDoubleAmplitude` @ 0x24a4f3 (4-arg only).
- `cos`  @ 0x24abd0 — `readDoubleAmplitude` @ 0x24aff3 (4-arg only).
- `sawtooth` @ 0x24c8b0 — `readDoubleAmplitude` @ 0x24ccbb (4-arg only).
- `triangle` @ 0x24d330 — `readDoubleAmplitude` @ 0x24d758 (4-arg only).

The long parameter labels `"2 (phase offset)"` and
`"3 (number of periods)"` exceed the inline-`movabs` 8-byte
window and are loaded from rodata via
`movupd 0x6bb7??(%rip),%xmm0` (e.g. `sin` 3-arg phase-offset
label resolves to rodata @ 0x905d93 = `"2 (phase offset)"`
verified with `objdump -s --start-address=0x905d93
--stop-address=0x905da3`).

### Fix

For each of `sin`, `cos`, `sawtooth`, `triangle`:

1. Default `amplitude = 1.0` and read `phase`/`nPeriods` in
   the 3-arg branch (same `readDouble` calls the binary
   makes).  Remove the constant-waveform collapse special
   case in `sin`/`cos` — the 3-arg path now drives the same
   loop as the 4-arg path.  Remove the
   "binary swaps amp↔nPeriods and phase↔phase in registers"
   comment in `sawtooth`/`triangle` and the trailing
   single-`if`/`else` `genericTriangle` dispatch — there is
   only one dispatch now (4-arg shape).
2. Update the 4-arg label strings from `"3 (phase)"` /
   `"4 (nPeriods)"` to the binary-faithful
   `"3 (phase offset)"` / `"4 (number of periods)"`.
3. Update the 3-arg label strings to
   `"2 (phase offset)"` / `"3 (number of periods)"`.

Added a new test case `trig_3arg`
(`hdawg_doc_trig_3arg.seqc`, registered in
`manifest-documentation.json`) that exercises both arities
of all four factories.  Each pair (`sine(64, 0.5, 2.0)` vs
`sine(64, 1.0, 0.5, 2.0)`, etc.) produces byte-identical
waveforms vs the binary post-fix.

### Why IF-205 / IF-231 didn't catch this

IF-205 (`randomGauss`) and IF-231 (`rand`) were each
discovered by their own coverage round.  The trig family
shares the same binary-side pattern (one
`readDoubleAmplitude` site, amplitude defaulted in 3-arg)
but has a much larger surface (four functions instead of
one).  A systematic audit of all multi-arity factories'
3-arg call-site counts at the time of IF-205 would have
caught all of these — that audit is now D-AUDIT-1 and is
the work item this fix completes.

### Coverage gap closed

`hdawg_doc_trig_3arg.seqc` (added with this fix) is the
first test case that exercises the 3-arg overloads of
`sine`, `cosine`, `sawtooth`, and `triangle` in a way that
distinguishes `(length, phase, nPeriods)` from
`(length, amplitude, phase)`.  Prior to it, only the 4-arg
forms were covered.



## IF-235  `StaticResources::errorReportTarget()` is a declared-but-undefined orphan helper

**Severity**: low (cosmetic / dead declaration)
**Status**: **closed-documented** — kept as a declaration with a
`\verifyme` brief noting it is a binary-faithful orphan; the
brief in `resources.hpp` was rewritten to be voice-rule clean
(no addresses, no binary references). No code change beyond
documentation; declaration retained to mirror the original
class layout.
**Source**: `reconstructed/include/zhinst/runtime/resources.hpp:1081`

### Observation

`StaticResources` declares a protected member
`std::function<void(std::string const&)> errorReportTarget() const;`
that has no definition anywhere in the reconstructed tree
(no body in `static_resources.cpp`,
`resources_static_global.cpp`, or `resources.cpp`) and no
call site (`grep -rn errorReportTarget reconstructed/`
returns only the declaration plus a single passing mention
in `static_resources.cpp:26`).

The static-archive link silently tolerates this because the
symbol is never referenced; an executable link or a virtual-
dispatch use would surface a `relocation against undefined
symbol` failure.

### Why the declaration exists

The `// Returns a callable that forwards to the
std::function stored inline at (functionStorage_,
functionPtr_)` comment block above the declaration documents
the binary's inline access pattern at addresses
`0x12a256-0x12a26d`.  Earlier reconstruction work
hypothesised that the binary exposed this access via a
named helper member, and the declaration was added as a
placeholder for the eventual reconstruction of that helper.
The actual binary appears to inline the `std::function`
re-packaging at every call site rather than using a single
named accessor, so the placeholder was never filled in.

### Action

Documented inline with a `\verifyme` doc comment so the
member shows up on the verify-me backlog page; the
declaration is left in place to preserve the existing
research note.  No code change to source files — purely a
documentation bookkeeping entry.

### Lesson

When reconstruction adds a forward declaration on
hypothesis (rather than from confirmed disassembly), tag
the declaration with `\verifyme` from the outset so it
surfaces on the backlog page and does not silently age
into orphan state.

## IF-236  `str(shared_ptr<AsmExpression>)` declared but never reconstructed

**Severity**: medium (missing reconstruction; live binary symbol)
**Status**: **fixed** — body reconstructed at
`reconstructed/src/asm/asm_expression.cpp` from full disassembly of
the 1373-byte binary function. Format is
`<head> (<tag>)\n` per node followed by recursively rendered
children indented by two spaces, with five tag values
(`cmd`/`reg`/`name`/`value`/`?`) and a Debug log record for
out-of-range type discriminators. Header brief updated; the new
implementation carries a `\verifyme` because no test exercises it
and the exact whitespace has not been confirmed against any
reference output. Build clean + 1602/1602 tests pass.
**Source**: `reconstructed/include/zhinst/asm/asm_expression.hpp:241`
**Binary symbol**: `_ZN6zhinst3strERKNSt3__110shared_ptrINS_13AsmExpressionEEE` at `0x28cd20`, size `0x55d` (1373 bytes), hidden visibility.

### Observation

The free function

```cpp
std::string str(const std::shared_ptr<AsmExpression>& expr);   // 0x28cd20
```

is declared in `asm_expression.hpp` but has no body anywhere in
`reconstructed/src/`.  Unlike IF-235 (which is a placeholder
declaration with no binary counterpart), this one **does**
correspond to a real symbol in `_seqc_compiler.so`:

```
000000000028cd20 l F .text 000000000000055d
  .hidden _ZN6zhinst3strERKNSt3__110shared_ptrINS_13AsmExpressionEEE
```

The function body uses an `ostringstream` (visible from the
`basic_ostringstream::basic_ostringstream` call at `0x28cd45`)
and is ~1.4 KB of disassembly, suggesting a recursive walk over
the `AsmExpression` tree producing a textual rendering — the
inverse of `Assembler::commandFromString`.

No call site in the reconstructed sources references this
declaration, which is why the static-archive link tolerates the
missing body.  In the original binary, the symbol is presumably
called from one of the assembler-debug paths (`Assembler::str`
verbose mode, debug logging, error diagnostics).

### Hypothesis

`str(shared_ptr<AsmExpression>)` is the textual-form
serialiser used by:
  * `Assembler::str(verbose=true)` to render the parsed program.
  * Diagnostic / error-message construction in
    `Assembler::commandFromString` and friends.
  * Possibly the AsmParserContext's syntax-error reporting path.

The 1.4 KB body is consistent with a switch over
`AsmExpression::Type` (Integer, Register, Name, Operation,
ShiftedOperation, etc.) recursively descending into the
`children` vector and emitting separators / operators
appropriate to the expression family.

### Action

Tagged the declaration with `\verifyme` so it surfaces on the
backlog page (subagent in D5-5 caught the gap during brief
sweep).  Reconstruction of the body is appropriate D-class
work but not blocking — no test exercises this path.

### Lesson

Missing reconstructions of live binary symbols can hide for
arbitrarily long when no caller exists in the recon tree.
The doc-coverage sweep is a useful secondary detector for
these gaps because every undocumented symbol forces the
agent to look for a body to verify against.

## IF-237  `AWGAssemblerImpl::parserMessage` also flips the syntax-error flag

**Severity**: low (cosmetic — recon comment is incomplete, body is correct)
**Status**: confirmed, fixed-cosmetic
**Source**: `reconstructed/src/codegen/awg_assembler_impl_pipeline.cpp:572` (body) and `reconstructed/include/zhinst/codegen/awg_assembler_impl.hpp:~250` (header note)

### Observation

The reconstructed `parserMessage(int level, std::string const& msg)`
body at `awg_assembler_impl_pipeline.cpp:572` ends with

```cpp
messages_.push_back(msg);
parserCtx_.setSyntaxError();
```

— i.e. it not only emits a diagnostic but also flips the parser
context's syntax-error flag, exactly like `errorMessage`.  The
pre-existing `// ---- Error reporting helpers ----` comment block
in the header, however, claimed that **only** `errorMessage`
called `setSyntaxError()`:

> "errorMessage additionally calls parserCtx_.setSyntaxError() afterwards."

This is misleading: by the recon body, `parserMessage` does the
same thing.  The asymmetry implied by the comment does not
exist in the recon code.

### Action

* Captured the surprise via a `\binarynote` on the
  `parserMessage` brief in `awg_assembler_impl.hpp` so it shows
  up on the binarynote backlog page.
* Did **not** rewrite the existing `// ---- Error reporting
  helpers ----` block-header comment in this pass — it is plain
  `//` text consumed only by humans and is a candidate for a
  follow-up cleanup along with the rest of the header's
  reconstruction notes.
* Did **not** verify against the original binary in this pass.
  The recon body and the binary may agree (in which case the
  binarynote correctly documents the API) or disagree (in which
  case the recon body is wrong and parserMessage should be a
  pure-non-fatal diagnostic that does not poison
  `hadSyntaxError`).

### Hypothesis to verify (D-class follow-up)

GDB-trace the original `AWGAssemblerImpl::parserMessage @0x289190`
on a known warning input (`msg` mnemonic in an `.asm` source) and
check whether `parserCtx_+0x03 (hadSyntaxError_)` is set after
the call.  If it is **not** set in the binary, the recon body is
wrong (recon writes the flag, binary does not) and `parserMessage`
should be downgraded to a pure-warning emitter.

### Lesson

When briefing a method whose contract is described in a
neighbouring `// ----` comment block, also re-read the body —
the comment block may describe the intended API while the
verified body describes a slightly different one.  Block-header
comments age out of sync with the code they describe.

---

## IF-238  D5-1 through D5-7 silently dropped reconstruction-comment information

**Severity**: medium
**Status**: fixed (this commit)
**Discovered**: 2026-05-11 mid-D5

### Summary

The D5 documentation pass (commits `1bbc46a` D5-1, `0104d2b` D5-2,
`5b8462d` D5-3, `74c5012` D5-4, `0bf65ce` D5-5, `c251ebe` D5-6, plus
the partially-applied D5-7 in `2d547ae`) added Doxygen `//!` briefs
across 14 header / source files but in the process **silently
deleted** a number of plain `//` reconstruction-annotation lines
that carried recon-only facts with no Doxygen-side equivalent.

The lost information consisted primarily of binary-symbol address
anchors (`// 0x2b05b0`, `// Reconstructed from operator!= at
0x1d5770`) and reconstruction-history provenance (rename history
of asm opcodes such as `// (was: UNK_FB)`, `cmdMap` confirmation
notes such as `// confirmed: cmdMap "xorr"`, the "only ctor in the
binary" forbidden-overload note for `CachedParser`, etc.).  Field
offsets, ABI padding markers, and behavioural prose were almost
always *relocated* (folded into the new `//!` briefs) and survived
intact; the genuinely-dropped facts were narrowly the addresses
and the recon-history annotations.

### Audit

A four-way subagent audit walked `git diff 0116fe7..HEAD` for
every removed `-//` line and categorised each as RESTORE,
RELOCATED, COSMETIC, or STALE/CORRECTED.  Across all 14 files,
14 lines were classified as RESTORE:

- `play_config.hpp`: 1 (operator!= address)
- `waveform_front.hpp`: 3 (copy-rename ctor, dtor, toString addresses)
- `assembler.hpp`: 5 (XORR/WWVF cmdMap provenance, WVFEI/WVFET/JMP rename history)
- `resources.hpp`: 1 (`str(VarSubType)` address `0x247ee0`)
- `cached_parser.hpp`: 5 (`cacheFile` / `cacheFileOutdated` / `getCachedFile` / `getHash` addresses, plus "only ctor in binary" note)
- `signal.hpp`, `waveform.hpp`, `wavetable_front.hpp`,
  `asm_expression.hpp`, `asm_parser_context.hpp`, `cache.hpp`,
  `awg_assembler_impl.hpp`, `device_constants.hpp`,
  `awg_compiler.cpp`: 0 (all dropped lines either RELOCATED or COSMETIC)

In addition, one STALE/CORRECTED candidate was identified in
`resources.hpp`: the pre-pass `//` comment above
`Function::addArgument` claimed the implementation calls
`scope->addVar(name, VarSubType_Stub)`, but the new `//!` brief
asserted `VarSubType_FunctionArg`.  The two contradicted each
other; the deletion was a silent correction rather than a silent
loss.

### Verification

`reconstructed/src/runtime/resources_function.cpp:335` — the
verified body of `Resources::Function::addArgument` (binary
`@0x1e9f60`) calls `scope->addVar(name, VarSubType_FunctionArg)`,
matching the new `//!` brief; the pre-pass `// VarSubType_Stub`
text was wrong (drifted text).  This is corroborated by five
independent comments elsewhere in `resources.cpp` (lines 1071,
1089, 1128, 1265, 1297) all referring to `VarSubType_FunctionArg`
as the parameter-binding subtype passed by `Function::addArgument`.

### Resolution

This commit:

1. Restores the 14 RESTORE-bucket lines as `//` annotations
   adjacent to the corresponding `//!` brief blocks (typically as
   trailing `// 0xNNNNNN` address anchors on the declaration
   line, or as a standalone `//` recon note immediately below the
   `//!` block).
2. Leaves the STALE/CORRECTED case unchanged in source — the new
   `//!` brief is verified-correct, and the pre-pass `//` claim
   is verified-wrong.  The forensic record lives in this IF
   instead of being restored as a stale comment.
3. Adds §14 to `reconstructed/notes/comment_style_guide.md`
   defining the `//` ↔ `//!` coexistence rule:
   *content preservation, not form preservation*.  Future doc
   passes must keep recon-only facts (addresses, offsets, ABI
   notes, rename history) somewhere in the file even when the
   form moves.

### Lessons

- Verify-then-write applies to deletion as well as addition.
  Removing a `//` line during a doc pass is equivalent to
  asserting "this fact is preserved elsewhere" — that
  assertion must be checked before the line is dropped.
- Address anchors (`// 0xNNNNNN`) are the most easily-lost
  category because they have no behavioural equivalent in the
  user-facing brief and are visually small.  Future passes
  should sweep address comments specifically.
- Subagent dispatch contracts should explicitly list the
  recon-fact categories that must be preserved (addresses,
  offsets, ABI padding, rename history, "only X in binary"
  forbidden-overload notes, JSON-key inventories,
  notes-file cross-references).



---

## IF-239 — `awg2marker` return-type mismatch between forward decl and definition

**Severity**: low (cosmetic / type-system)
**Status**: **fixed** — `awg2marker` return type aligned to
`uint8_t` at `reconstructed/src/core/stubs.cpp:58`; the phantom
top-level wrappers `zhinst::awg2double` and `zhinst::awg2marker`
were removed (verified absent from the binary symbol table — the
Itanium C++ ABI does not mangle return type into the symbol so a
single `awg2marker` definition with the correct return type
suffices). Brief in `awg_compiler.cpp` was cleaned of binary
internals to comply with the voice rules.

**Location**:
- Forward declaration: `reconstructed/src/codegen/awg_compiler.cpp:52` —
  `uint8_t awg2marker(uint16_t raw);`
- Canonical definition: `reconstructed/src/core/stubs.cpp:58` —
  `uint16_t awg2marker(uint16_t sample) { return sample & 0x3; }`

**Observation**: The forward declaration inside `namespace util::wave`
in `awg_compiler.cpp` advertises a `uint8_t` return type, but the
actual out-of-line definition returns `uint16_t`. The
`using util::wave::awg2marker;` alias in `awg_compiler.cpp` then pulls
in the `uint16_t` version (linker resolves by name + parameter list,
not return type).

**Impact**: All call sites use the result via narrowing assignment
(`& 0x3` is already at most 2 bits), so the type discrepancy is
behaviourally invisible. It is, however, an ODR-adjacent inconsistency
that could trip C++ frontends with stricter type-checking, and a
documentation hazard: a reader of `awg_compiler.cpp` will believe the
return type is `uint8_t`.

**Resolution candidates**:
1. Align the forward decl to `uint16_t` (matches the binary).
2. Re-verify the binary symbol's signature with `objdump -T` /
   `nm --demangle` on `_seqc_compiler.so` to confirm which is correct.

---

## IF-240 — `operator==(DeviceOptionSet const&, DeviceOptionSet const&)` block-header summary understates the check

**Severity**: low (recon body diverged from binary)
**Status**: **fixed** — the spurious `a.family_ == b.family_`
short-circuit added by an earlier reconstruction pass was removed
from `reconstructed/src/device/device_type.cpp:325`. The binary
implementation is a 6-byte tail-call that delegates straight to
`std::operator==` on the underlying option container; the recon
now mirrors that. The block-header summary was already
binary-faithful; only the body needed correction.

**Location**: `reconstructed/src/device/device_type.cpp:291-294`

**Observation**: The block-header comment reads `// @ 0x2cfd80 —
compares the unordered_sets only.` but the body is

```cpp
return a.family_ == b.family_ && a.values_ == b.values_;
```

which compares both the `family_` field and the `values_`
unordered set. The summary is therefore misleading: the family is
part of the equality contract.

**Impact**: Future readers may believe two `DeviceOptionSet`s with
identical option codes but different `DeviceFamily` enums compare
equal — they do not.

**Resolution**: The Doxygen brief added in this pass mentions the
discrepancy under `\binarynote`. The legacy `//` block-header
comment is preserved verbatim (style guide §14) and a `\binarynote`
flag points at this IF for context. If a follow-up pass verifies
against the binary that the family check exists at `0x2cfd80`
itself (and is not a recon over-correction), the legacy comment
can be rewritten.

## IF-241  Table sub-path C totalSize formula confirmed; sub-path polarity correction

**Severity**: confirmed (formula); IF-223 polarity correction needed.
**Status**: **resolved** (formula); follow-up edit to IF-223 sub-path
labels required.
**Discovered**: D4 IF-223 follow-up, subagent audit
`ses_1ea03f82dffex6rJH4zRXSU33x`.

### Confirmed totalSize formula

In the Table sub-path that branches via `je 0x1d949f → 0x1daed4`,
the binary computes:

```
0x1daee5  mov r12d, [rsi+0x90]              ; r12d = node->length
0x1daff2  imul r12d, [rbp-0x158]            ; r12d *= numChannels
0x1daffa  add r12d, r12d                    ; r12d *= 2
0x1daffd  mov [rbp-0x140], r12d             ; spill totalSize
```

So:

```cpp
const int32_t totalSize =
    (static_cast<int32_t>(node->length)
     * static_cast<int32_t>(waveform->signal_.numChannels())) * 2;
```

`numChannels` is read via `movzx eax, WORD PTR [waveform+0xC8]`, so
the source is the uint16 `Waveform::signal_.channels_` field.  The
`imul` and `add` are signed 32-bit; for realistic AWG sizes overflow
is not a concern.

### Polarity correction for IF-223

The gate at `0x1daed4` is `cmpb $1, [r15+0xbc]; jne 0x1db749`.
Therefore:

- `split_ == 1` (true) → **falls through** to the formula above
  (the `addi(idxReg, lengthReg, 0)` + per-channel `ssl(idxReg)`
  loop + `addr(idxReg, stateReg)` + `prf` chain).
- `split_ != 1` (false) → **jumps** to `0x1db749`, the simpler
  branch that re-converges with sub-path B's smap-2 emission.

This is the **opposite polarity** to the labelling in the IF-223
table, where C1 was described as `!split_` and C2 as `split_`.
The actual mapping is:

| Original IF-223 label | Actual binary gate | Body |
|---|---|---|
| C1 (was: `!split_`)   | `split_ != 1` → `0x1db749` | smap-2 share-with-B path |
| C2 (was: `split_==1`) | `split_ == 1` → falls through | `addi+ssl-loop+addr+prf+wprf` |

The IF-223 entry should be edited to swap C1 and C2 labels.  All
formulae and instruction sequences carry across unchanged; only the
gate-polarity description was wrong.

### Convergence point for the simpler (split_!=1) branch

The simpler branch at `0x1db749..0x1db826` does the same
`length * numChannels` multiplication (and `add r12d, r12d`
doubling) at `0x1db820..0x1db823`, then jumps to `0x1da56e`
(the smap-2 emission shared with sub-path B).  So the
totalSize formula is **the same** for both Table-C branches;
only the emission shape differs.

### Action

Edit IF-223's sub-path table to correct the C1/C2 polarity and
record the verified formula above.  No other action — IF-241 is
resolved.


## IF-242  Table sub-path C `bgReg` ≡ Play `idxReg` (rename recommended)

**Severity**: confirmed (naming).
**Status**: **closed-already-applied** — verification at
`reconstructed/src/codegen/prefetch_placesingle.cpp:741+`
confirms the recommended `idxReg` name is already in use in the
sub-path C reconstruction; no source change is required.
**Discovered**: D4 IF-223 follow-up, subagent audit
`ses_1ea03a9cbffezFNN5hEzQeoQDj`.

### Verified role

`bgReg` is a fresh `AsmRegister` allocated at `0x1daf6a`, then:

1. Initialised via `addi(bgReg, lengthReg, Imm(0))` at `0x1dafb3`
   — effectively `mov bgReg, lengthReg`.
2. Used unchanged inside an ssl loop at `0x1db024..0x1db1ee` that
   iterates `numChannels` times: each iteration emits
   `ssl(bgReg)` (single-bit shift left in place; `bgReg <<= 1`
   per channel).
3. After the loop, used as the **destination** operand of
   `addr(bgReg, registerHirzel|Cervino)` at `0x1dbc8d` (Hirzel
   branch) or `0x1dbba9` (Cervino branch) — folds the per-node
   prefetch-cache base address into the now-shifted `bgReg`.

The pattern is **identical** to `idxReg` in the existing Play
recon at `prefetch_placesingle.cpp:739..776` (Cervino indexed
split-play path).  Same allocation, same `addi(_, lengthReg, 0)`
init, same per-channel `ssl` loop, same final `addr(_, stateReg)`
fold, same subsequent feed into `wvfImpl`/`prf`.

### Recommended C++ variable name

**`idxReg`** — for direct symmetry with the existing Play recon
(three other sites already use this name for the same idiom).

### Open question (minor, won't block C reconstruction)

The Table-case ssl loop body re-fetches the waveform name
optional and string-allocates each iteration (the
`0x1db033..0x1db121` block).  This looks redundant compared to
the Play loop but matches the binary; it does not affect bgReg
semantics or the emitted instruction stream.  No action.

### Action

When reconstructing IF-223 sub-path C, name the register
`idxReg` (not `bgReg`).  No other action — IF-242 is resolved.


## IF-243  `awgCfg_+0xC` premise corrected: actually `devConst_->waveformMemorySize`

**Severity**: confirmed (premise correction, not a bug).
**Status**: **resolved** — field already documented in
`device_constants.hpp`.
**Discovered**: D4 IF-223 follow-up, subagent audit
`ses_1ea03532affeTSYYxbzkWiskeB`.

### Premise correction

The IF-223 audit and IF-243 entry described the field at
`[r15+0x08] + 0xC` as `awgCfg_+0xC`.  This is **wrong**:

- `Prefetch+0x00` = `config_` (`AWGCompilerConfig const*`).
- `Prefetch+0x08` = `devConst_` (`DeviceConstants const*`),
  **not** `awgCfg_`.

(`AwgCompilerConfig+0xC` is itself the upper 4 bytes of
`double deviceSampleRate` at `+0x08`, i.e. padding inside a
double — no field there.  The header at
`reconstructed/include/zhinst/codegen/awg_compiler_config.hpp:154`
explicitly anticipates this exact mistake.)

### Verified field

The field at `0x1db223` (`cmp eax, [rcx+0xc]` where
`rcx = [r15+0x8] = devConst_`) is:

- **Name**: `DeviceConstants::waveformMemorySize`
- **Offset**: `+0x0C` of `DeviceConstants`
- **Type**: `uint32_t`
- **Header**: already documented at
  `reconstructed/include/zhinst/device/device_constants.hpp:43-45`.
- **Per-device values**: HDAWG / SHFQA `0x100000`; UHFLI / UHFQA
  `0x20000`; SHFSG / SHFQC_SG / SHFLI / GHFLI / VHFLI `0x60000`.

### Comparison semantics

The comparison at `0x1db223` is `cmp/jne` (equality test, not
ordered).  When `cachePtr->size_ == waveformMemorySize`, the
fall-through main path runs; otherwise `jne 0x1db876` jumps to a
specialised handler.  Semantically: "is the play size exactly
equal to total cache size?" (full-cache-fill specialisation).
This is **narrower** than the original "selects between main and
alternate prf encoding" framing — the broader main-vs-alternate
selection happens at an earlier branch.

### Action

No code change needed; field is already correctly named in
`device_constants.hpp`.  When reconstructing IF-223 sub-path C,
the gate at `0x1db223` should be expressed as
`loadState.cachePtr->size_ == devConst_->waveformMemorySize`,
not via any `awgCfg_+0xC` reference.

The recon's `Prefetch` member access pattern is `devConst_`
(via the `DeviceConstants*` member at `Prefetch+0x08`), confirmed
in `prefetch.hpp:58`.  IF-243 is resolved.


## IF-244  Existing `play_cervino_indexed_nonsplit` recon body actually corresponds to Table-C2; Play has its own separate copy

**Severity**: **likely-bug** (recon mislabeled; promotes to TODO).
**Status**: **fixed** (documentation-only relabel; action items 1
and 2 below executed in commit following IF-244 entry; action item 3,
the optional `emitPrfEpilogueAndInsert_` helper, deferred and
tracked as a separate TODO entry — see TODO.md). Voice-rule
cleanup applied to `prefetch_placesingle.cpp` doc comments:
addresses, register names, and `_seqc_compiler.so` references
were moved out of `//!` briefs into `//` reconstruction blocks
per AGENTS.md.
**Discovered**: D4 IF-223 follow-up, subagent audit
`ses_1ea02f346ffeXjP1970hUIsRqE`.

### Critical finding

The block currently reconstructed at
`prefetch_placesingle.cpp:884-908` and labelled
`play_cervino_indexed_nonsplit` is **mislabelled**.  Its body
matches the binary at `0x1db562..0x1db60a` (which is the
Table sub-path C2 split tail), **not** the binary at
`0x1db4ad..0x1db55d` (which is the actual Play
`cervino_indexed_nonsplit` tail).

Three differences distinguish the two:

| Feature | Real Play (0x1db4ad..0x1db55d) | What recon currently has (= Table-C2, 0x1db562..0x1db60a) |
|---|---|---|
| `clampToCache` call? | **No** — uses raw `cacheSize >> 1` directly (`shr r8d, 1` then `prf`) | **Yes** — calls `clampToCache(cacheSize >> 1)` at `0x1db5dd` |
| `wprf` placement | Emitted **inline before** `prf` at `0x1db4bd` (unconditional) | Deferred to shared `0x1db92e` tail (gated on `!isHirzel` at `0x1db935`) |
| Final exit | `0x1db55d jmp 0x1db92e` (skips `0x1db911`, bypasses wprf gate) | `0x1db60a jmp 0x1db911` (unified cleanup → wprf-if-needed → insert) |

### Xref evidence

- **`0x1db562`** has **exactly one** xref into it: from `0x1daad8`
  (Table sub-path C2 path).  No Play xrefs.
- **`0x1db911`** has 3 distinct predecessors: `0x1d9d03` (Play
  `cervino_indexed_nonsplit`), `0x1dabb4` (Table sub-path C2
  alternate), and `0x1db60a` (fall-through from `0x1db562` Table
  C2 main).
- **`0x1db92e`** has 4 distinct predecessors: `0x1d8629` and
  `0x1d86cb` (sub-path A, very early in placeSingleCommand);
  `0x1db55d` (Play `cervino_indexed_nonsplit` direct exit); and
  fall-through from `0x1db911`.

So `0x1db911` and `0x1db92e` are **genuine** shared tails (3- and
4-way respectively).  But the prf+wprf body itself is **not**
shared between Play and Table.

### Action items (promoting to TODO.md)

1. **Rename** the existing recon block at
   `prefetch_placesingle.cpp:884-908` from
   `play_cervino_indexed_nonsplit` to `table_indexed_with_clamp`
   (or similar) and **move it** out of the Play dispatch into
   the Table dispatch path.  Its body — which calls
   `clampToCache` — corresponds to the binary at
   `0x1db562..0x1db60a` (Table sub-path C2 split).
2. **Add a new recon block** for the actual Play
   `cervino_indexed_nonsplit` tail at
   `0x1db4ad..0x1db55d` that emits `wprf` first (no gate),
   then `prf(regHirzel, regCervino, cacheSize >> 1)` **without**
   `clampToCache`, then exits via `0x1db55d jmp 0x1db92e`.
3. **Optionally** factor the `0x1db911` and `0x1db92e` tails
   into a shared helper:
   ```cpp
   void Prefetch::emitPrfEpilogueAndInsert_(
       AsmList& tempList, AsmList& out,
       const std::shared_ptr<Node>& node, bool prfAsmStillLive);
   ```
   Body: dtor the prf Asm temporary if requested; if `!isHirzel`
   (gated), emit and append `wprf()`; then
   `out.insert(end, tempList.begin(), tempList.end())` and dtor
   tempList.  Eliminates ~30 lines of duplication across 4 call
   sites.

### Why tests pass at 1602/1602 even with the mislabel

The test corpus does not currently exercise:
- Play `cervino_indexed_nonsplit` with `cachePtr->size_` such
  that `clampToCache` would actually clamp (i.e. the recon
  emits a clamped value where the binary emits the raw value).
- Table sub-path C2 (`split_ == 1` AND `lengthReg.isValid() &&
  != R0`).  This is the same coverage gap as IF-223 sub-path C.

When the corpus is extended, the mislabel will manifest as a
divergence on Play `cervino_indexed_nonsplit` tests.

### Caveats

- The interpretation of `[r15+0x18]` at `0x1db935` as
  `isHirzel` (negated) is consistent with the Play wprf-bypass
  behaviour but **not GDB-verified**.  If the helper extraction
  in action item 3 is taken, tag the gate `\verifyme`.
- Did not GDB-trace the actual control flow on a Table-C2 input
  (corpus does not contain one) or a Play
  `cervino_indexed_nonsplit` input.  Static disassembly alone.



## IF-245  Table-case `cmpb $1, -0x118(%rbp)` checks are cosmetic optional<string> dtor gates

**Severity**: cosmetic / dismissed.
**Status**: confirmed (no action needed).
**Discovered**: D4 IF-223 consolidated audit, §9 open-question 5.

The repeated `cmpb $1, -0x118(%rbp)` checks throughout the
Table case (e.g. at `0x1daf2c`) test the engaged-flag of an
`std::optional<std::string>` stack temporary (the optional
returned by `Node::waveAtCurrentDeviceIndex()`).  When set,
the binary follows up with a heap-data destructor for the
optional's owned string.

These checks have **no instruction-emission impact** — they
gate only stack-temporary cleanup.  The recon's use of
`auto waveOptT = npSync->waveAtCurrentDeviceIndex();` already
generates equivalent dtor code via RAII, so no explicit
handling is needed.

Recorded for completeness; no follow-up action.

