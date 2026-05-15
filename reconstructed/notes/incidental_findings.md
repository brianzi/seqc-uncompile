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

## Archives

- **IF-1..IF-99** (all closed): see
  [`archive/IF_1-99.md`](archive/IF_1-99.md).  Documents the early
  reconstruction era (Phases 1–35).
- **IF-100..IF-200 closed entries** (98 of 101 in range, including
  sub-variants IF-140b and IF-143b): see
  [`archive/IF_100-200.md`](archive/IF_100-200.md).  Documents the
  middle reconstruction era (Phases 36–62).  The three open entries
  from this ID range (IF-100, IF-102, IF-104) remain in the active
  sections below.

The active sections below cover the **open** entries from
IF-100..IF-200 plus **IF-201..IF-254** (latest entries from Phase D
and follow-up audits).  IDs cited from `reconstructed/src/` and
`reconstructed/include/` comments may reference either the active
file or the archive.

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

## IF-104  ELFIO NOBITS section file-offset layout differs between vendored and system builds

**Source**: `shfsg_doc_ct_placeholder` test failure investigation
**Status**: confirmed-fixed-cosmetic (fix applied at test harness; recon
unaffected; re-verified post-F3 2026-05-13 — `compare_results` in
`tests/diff_test.py:382-399` special-cases `SHT_NOBITS` correctly,
and `tests/diff_test_fast.py:36` reuses the same function via
import, so both harnesses are NOBITS-aware.  The 1603-case suite
exercises the `manualdocs_ai_synthesized:shfsg:ct_placeholder`
case and PASSes.)
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
**Status**: **fixed** — verified 2026-05-11.  The `prepareTree`,
`countBranches`, and `definePlaySize` block-headers now match
their bodies (the body itself was always correct; only the
prose drifted, and the prose has since been rewritten to use
`NodeType::Load`/`Play`/`Lock`/`isHirzel`/`maxBranches_`).
The remaining stale references inside the `Lock` case body
(case-label comment "Wait", "if append mode", "Not append
mode", and the local variable `waitNode`) were corrected in
the same pass: case-label is now "Lock", the gate comment
explains Hirzel devices skip the merge logic, and the local
was renamed `waitNode → lockNode`.  Build clean, 1603/1603
tests pass.
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
**Status**: ✅ **fixed 2026-05-11** (E1+E2+E3+E4+E5 all done).
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

- E1: ✅ **done 2026-05-11**.  Replaced
  `static_cast<AwgDeviceType>(5)` (= `UHFLI | UHFQA`) with
  `kDevCervino` (9 sites in `custom_functions_playback.cpp`).
  Replaced redundant `static_cast<AwgDeviceType>(HDAWG)`,
  `(UHFLI)`, `(UHFQA)`, `(SHFLI)`, `(VHFLI)`, `(GHFLI)`,
  `(SHFQC_SG)` with the bare enumerator (the cast was a no-op
  since the operand was already `AwgDeviceType`).  Replaced
  numeric `(2)`, `(4)`, `(8)` with `HDAWG` / `UHFQA` / `SHFQA`
  enumerators (`custom_functions_registers.cpp` ×7,
  `custom_functions_playback.cpp` ×1).  Total: ~24 call sites.
  Two casts kept legitimately: `(devConst_->deviceType)` and
  `(current)` are int→enum widening casts; two more kept for
  `(UHFLI | HDAWG | UHFQA)` because the bitwise-OR result is
  `int` and needs the cast back to `AwgDeviceType`.  Build
  clean, 1603/1603 tests pass — fully NFC.
- E2: ✅ **done 2026-05-11**.  Added `kPlayTriggerMaskFull =
  0x3FFF` and `kPlayTriggerMaskAuxMerge = 0x3FC3` to
  `core/types.hpp` (with briefs explaining the 14-bit mask
  layout and aux-wave variant).  Replaced 17 literal sites
  across `custom_functions_play.cpp` (3),
  `custom_functions_playback.cpp` (6), and
  `custom_functions_dio.cpp` (1).  The `clearChannelTriggerByte`
  helper proposal is dropped: the `mask &= ~(0x40 << shiftBits)`
  pattern appears in only one live site
  (`custom_functions_playback.cpp:518`); a helper would
  obscure rather than clarify.  Build clean, 1603/1603 tests
  pass — fully NFC.
- E3: ✅ **done 2026-05-11**.  Added `kZSyncShiftRaw=1`,
  `kZSyncShiftProcessedA=9`, `kZSyncShiftProcessedB=0xd` as
  file-scope constexpr in the anonymous namespace of
  `custom_functions_playback.cpp` (single-site usage; doesn't
  warrant a public header).  The disassembly comments
  (`@0x137fab`, `@0x138049`, `@0x1380ea`) are kept for
  trace-back to the binary.  Build clean, 1603/1603 tests
  pass — fully NFC.
- E4: ✅ **done 2026-05-11**.  Added factory
  `AsmRegister::UnsetSlot()` returning `{0, false}` — the
  binary's actual stored bytes for `AsmRegister(-1)`
  construction (the `cmovg`/`setns` idiom collapses `-1` to
  `value=0`).  Distinct from `Invalid()` (`{-1, false}`) in
  bytes, equivalent under the equality rule.  Replaced 6 live
  `AsmRegister(-1)` sites: `custom_functions_playback.cpp`
  (×2: `regInv` in `playAuxWave` and `playDIOWave`),
  `custom_functions_play.cpp:2454`,
  `custom_functions.cpp:913`, `asm_optimize.cpp:635`,
  `resources_function.cpp:377` (whose stale "valid=true"
  comment is also corrected).  Added rate-floor constants
  `kMinRatePlayAux=4` and `kMinRatePlayDIO=1` in the
  `custom_functions_playback.cpp` anonymous namespace,
  replacing the bare `rate <= 4` and `rate <= 1` literals at
  lines 181 and 482.  Build clean, 1603/1603 tests pass —
  fully NFC.
- E5: ✅ **done 2026-05-11**.  Audit confirmed `PlayConfig`
  already exposes named position-aligned `*Mask` /
  `*Shift` constants for every encoded field, plus the
  `holdSuppressExceptSigouts` sentinel.  The remaining magic
  literals lived in `play_config.cpp::encodeCwvf`'s packing
  block as per-field width masks (`0x3u`, `0xFu`, `0x3FFFu`,
  `0x1u`).  Rewrote each line as
  `(value << fieldShift) & fieldMask` using the existing named
  constants — bit-exact equivalent because each `fieldMask`
  equals `(widthMask << fieldShift)`.  `genPlayConfig` in
  `asm_commands.cpp` carries no encoded-word literals (it only
  stores raw field values), so no changes there.  Build clean,
  1603/1603 tests pass — fully NFC.

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
**Status**: **closed-documented** — kept as a declaration with an
`\unclear` brief (retagged from `\verifyme` in 2026-05-13:
`\verifyme` implies a hypothesis with a binary referent that
could be GDB- or test-verified, but no binary symbol matching
this declaration has been located, so the more accurate marker
per AGENTS.md tag rules is `\unclear`).  The brief in
`resources.hpp` notes the absence of both a caller in the
reconstructed tree and a matching binary symbol; declaration
retained to mirror the original class layout.
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
the optional `emitPrfEpilogueAndInsert_` helper, was queued as TODO
D9.4 and **dropped** at the D9.3 wrap-up — with the two dead label
blocks deleted by D9.3 (commit `d1515c8`), only three inline call
sites remain, each with subtle differences (clamp vs no-clamp,
gated vs unconditional `wprf`); a shared helper would obscure rather
than clarify. See also IF-246 (D9.1 reframed item 1 as a Load-side
mis-attribution, fixed by deletion in D9.3) and IF-247 (D9.2
confirmed the `wprf` gate, promoted to a real `if (!config_->isHirzel)`
in D9.3)). Voice-rule cleanup applied to `prefetch_placesingle.cpp`
doc comments: addresses, register names, and `_seqc_compiler.so`
references were moved out of `//!` briefs into `//` reconstruction blocks
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


## IF-246  IF-244's `play_cervino_indexed_nonsplit` block is mis-attributed and incomplete (GDB-confirmed)

**Severity**: **likely-bug** (recon documentation block describes a different binary path than IF-244 claimed).
**Status**: fixed (D9.3 commit `d1515c8` deleted the mis-attributed block; D10 commit `a6f92d9` rewrote `load_cervino_prf` Path B1 with the full binary emission shape, verified byte-identical by `core:uhfawg_load_cervino_prf_path_b1`).
**Source**: `reconstructed/src/codegen/prefetch_placesingle.cpp:865-941` (deleted in D9.3); `prefetch_placesingle.cpp:342-` (Path B1, rewritten in D10); IF-244.
**Discovered**: D9.1 GDB verification of IF-244 hypotheses.

### Findings

D9.1 set breakpoints at `0x1db4ad`, `0x1db4bd`, `0x1db52b`, `0x1db55d`, `0x1db92e`, `0x1d85f6` (`isHirzel` cmp), and the `load_cervino_prf` entry `0x1d9c33`. Two inputs were traced:
- UHFAWG `wave w = placeholder(65536); playWaveIndexed(w, 0, 128);` — reaches `0x1db4ad` via `0x1d85fa jne → load_cervino_prf @0x1d9c33 → Path B @0x1db1f3 → Path B1 @0x1db223` (gate: `cacheSize == waveformMemorySize`).
- HDAWG8 with `tests/cases/hdawg_doc_prefetch.seqc` reached `0x1db92e` only via sub-path A (`0x1d8629`/`0x1d86cb`), not via the `0x1db4ad..0x1db55d` block.

### Three corrections to IF-244

1. **Side attribution wrong**. The block at `0x1db4ad..0x1db55d` is reached only via the **Load** dispatch (`load_cervino_prf @0x1d9c33`), never via the Play dispatch. IF-244's claim that it is the "actual Play `cervino_indexed_nonsplit` tail" is unsupported by xref evidence; the predecessor list of `0x1db92e` that IF-244 cites (line 6766-6773) attributes `0x1db55d` to Play, but D9.1 traced `0x1db55d` reached only after a Load path.

2. **Emission shape wrong**. The actual binary body emits **two** `prf` calls with `wprf` between them: `prf @0x1db2b3` (cacheSize/2) → `2× addi` → `wprf @0x1db4bd` → `prf @0x1db52b` (cacheSize/2 again) → `jmp 0x1db55d → 0x1db92e`. IF-244 documents only the second `prf` and characterises `wprf` as "inline before prf (unconditional)" — true relative to the second `prf`, but omits the first `prf` entirely.

3. **Dispatch gate wrong**. IF-244 hypothesised `!isHirzel && !split_ && lengthReg.isValid() && pagesNeeded < 2 && indexed && Play-case`. The actual gate chain is `!isHirzel` (`0x1d85fa`) → enters `load_cervino_prf` → `numRepeats < 2` → `cachePtr->size_ == devConst_->waveformMemorySize`. There is no `lengthReg.isValid()` test on this path and no Play-case condition.

### Recon impact

The label-only block `play_cervino_indexed_nonsplit` at `prefetch_placesingle.cpp:865-941` is wrong on its face: it is documented as Play-side, has no incoming goto, and its body is missing the first `prf` and `2× addi`. The closest correct implementation is the **Path B1 stub inside `load_cervino_prf`** at `prefetch_placesingle.cpp:343-349`, which already emits "one prf with halfSize" but is itself incomplete (also missing the first prf, the addis, and the wprf-between).

### Action

Tracked as TODO **D9.3** (replace dead block with correct Load-side reconstruction) and **D10** (full rewrite of `load_cervino_prf` Path B1 from scratch using D9.1 evidence). IF-244's `play_cervino_indexed_nonsplit` documentation block should be deleted in D9.3.


## IF-247  `wprf` gate at shared tail `0x1db935` confirmed `!isHirzel`

**Severity**: confirmed (resolved a `\verifyme`).
**Status**: fixed (D9.3 commit `d1515c8` promoted the `\verifyme` to a real `if (!config_->isHirzel)` gate at the inline Table-C-split `wprf` site; D10 commit `a6f92d9` additionally test-verified the gate's interaction with the Path B1 local `wprf` — UHFAWG (`isHirzel=0`) emits two `wprf` instructions on Path B1, byte-identical to the binary, via `core:uhfawg_load_cervino_prf_path_b1`). The Table-C-split site itself **cannot be test-verified** from SeqC because `NodeType::Table` is unreachable from the front-end (see IF-249); the gate is correct on its own merits and protects future code paths.
**Source**: `reconstructed/src/codegen/prefetch_placesingle.cpp` Table-C-split `wprf` site (was lines 1398-1407, now gated).
**Discovered**: D9.2 GDB verification of IF-244 caveat.

### Finding

The disassembly at the shared prefetch tail is:
```
0x1db92e:   mov    (%r15),%rax            ; rax = this->config_
0x1db931:   cmpb   $0x0,0x18(%rax)         ; cmp config_->isHirzel, 0
0x1db935:   jne    0x1db963                ; if isHirzel != 0 → SKIP wprf
0x1db937..0x1db95e:                        ; wprf emit + AsmList::append + Asm::~Asm
0x1db963:   ...                            ; tempList → out insert
```

GDB-verified `[rax+0x18]` values:
- UHFAWG (isHirzel=0): gate falls through, `wprf` IS called.
- HDAWG8 (isHirzel=1): gate is taken, `wprf` is NOT called.

### Recon impact

The Table-C-split inline emission at `prefetch_placesingle.cpp:1404-1407` emits `wprf` unconditionally. For HDAWG / SHFSG / SHFQC_SG / SHFLI (all `isHirzel=1`) flowing through this path, the recon **over-emits** `wprf`. This is a **latent divergence** — no test in the corpus currently exercises Table sub-path C2 on a modern device (D8 case 2 is the corresponding coverage gap).

### Action

Promote the `\verifyme` at line 1398-1407 to a real `if (!config_->isHirzel)` gate, tracked under **D9.3**. The fix is mechanical and one-line; the test verification (D8 case 2) is the prerequisite for landing it without a regression risk.


## IF-248  D9.1 "isHirzel inversion" claim is a naming-convention confusion, not a code bug

**Severity**: cosmetic / dismissed.
**Status**: confirmed-dismissed.
**Source**: D9.1 trace report.
**Discovered**: D9.1 GDB verification, follow-up audit.

### Background

The D9.1 subagent's report stated that the recon's "Hirzel = HDAWG / Cervino = SHFSG/SHFQC" interpretation is "inverted" because GDB shows HDAWG, SHFSG, and SHFQC_SG all have `config_->isHirzel = 1` (and UHFAWG has `0`). The subagent flagged this as potentially affecting "every code path gated on `config_->isHirzel`."

### Why it is dismissed

The recon's `awg_device_props.cpp` already assigns `isHirzel = true` to HDAWG / SHFSG / SHFQC_SG / SHFLI and `false` to UHFQA / UHFAWG / SHFQA, exactly matching the GDB-verified flag values. The header docstring at `awg_compiler_config.hpp:31,87` documents `isHirzel = 1` as "Hirzel device" / "selects the modern sequencer." All gate logic in the recon reads `config_->isHirzel` as the same boolean the binary reads — there is no semantic inversion at the **flag** level.

What the D9.1 subagent observed as "inversion" is a **prose-naming inconsistency**: some recon comments (e.g. `prefetch_placesingle.cpp:626`, `IF-244` text) use "Cervino" as a label for the `!isHirzel` arm of certain dispatches, which clashes with internal Zurich Instruments codename conventions where "Cervino" is itself a modern hardware generation. This is a prose/comment cleanup issue with no impact on emitted code or test outcomes.

### Action

None on code or tests. Optional follow-up: when documentation pass D7 sweeps `\verifyme` / `\unclear` tags, audit prose uses of "Hirzel" / "Cervino" in code comments for naming consistency. Not promoted to a TODO entry — too low-value to schedule.

## IF-249  `NodeType::Table` is unreachable from the SeqC front-end (both binary and recon)

**Severity**: cosmetic / documentation (rules out a class of latent-path test cases).
**Status**: confirmed.
**Discovered**: D8 case 2 scoping (subagent `ses_1e80272b8ffeib81kyIXaNE8hf`, 2026-05-11).

### Background

D8 case 2 set out to author a SeqC test that exercises the
**Table sub-path C-split** in
`reconstructed/src/codegen/prefetch_placesingle.cpp:1270-1339`,
which would test-verify the `!isHirzel`-gated `wprf` emission
fixed in D9.3 (IF-247).  The path requires a `Sync` node whose
parent is `NodeType::Table` (`0x200`) and whose `lengthReg` is
valid && != R0, with `split_==1`.

### Investigation

The subagent attempted **eleven** different SeqC inputs against
the original `_seqc_compiler.so` with breakpoints at:

- `placeSingleCommand` entry (`0x1d7940`)
- The `case NodeType::Table` arm (`0x1d7ebb`)
- The C-split dispatch (`0x1daed4`)
- The C-split body (`0x1daf55`)
- The C-non-split arm (`0x1db749`)

Inputs covered HDAWG8, SHFSG4, SHFQC, UHFQA, and UHFAWG, with
combinations of `placeholder()` (sizes 8192–524288),
`assignWaveIndex`, `executeTableEntry` (singletons, branches,
1000-iteration loops), `playWave`, `playWaveIndexed`, the
existing `tests/cases/hdawg_cmd_table*.seqc`, and
`tests/cases/stress/many_placeholders.seqc`.

**The `case NodeType::Table` branch was never entered for any
input.**  Counter at the Table case stayed at 0 across all
runs, while `placeSingleCommand` itself was entered 3..20
times per run.

### Structural confirmation

The subagent then audited the binary directly:

1. `AsmCommands::asmTable` at `0x2797c0` (the only function in
   either binary or recon that emits `NodeType::Table` via
   `emitNodeEntry`) has **zero call sites** —
   `objdump -dC _seqc_compiler.so | grep -E "call.*AsmCommands::asmTable"`
   returns no hits.
2. No `mov $0x200, %esi` immediately precedes any `Node::Node`
   ctor call (`0x12ace0` / `0x26c4a0`) in `.text`.
3. No direct `movl $0x200, 0x44(...)` store of the type field
   exists in `.text` outside of unrelated openssl/dl noise.

The recon mirrors this: `grep -rn 'asmTable\b' reconstructed/`
shows only the declaration in `asm_commands.hpp:1274` and the
definition at `asm_commands.cpp:1107`.  No callers.

`executeTableEntry` itself does **not** create Table-typed
nodes; it emits a `wvft` asm instruction directly into the
result list, leaving any surrounding Sync/Play nodes typed as
Play / SyncCervino / SyncHirzel.

The only remaining production site for `NodeType::Table` is
`Node::from_json` (matched by the `"table"` string in
`node.cpp:179`), which deserialises a pre-built JSON AST.  The
`compile_seqc(...)` Python entry point (the only public binding
exposed by `_seqc_compiler.so`) never feeds this path — `dir(sc)`
returns only `['compile_seqc']`.

### Implications

- **D8 case 2 is unreachable from the SeqC front-end** and was
  dropped (TODO update co-landed with this IF).
- **D8 case 3** (`playWaveTable` with non-empty cache + valid
  per-channel length register) was speculative on the existence
  of a `playWaveTable` custom function; that function does not
  exist (`grep` for "playWaveTable" returns no source hits).
  Also dropped.
- **IF-247's `!isHirzel` gate fix** (D9.3, the inline
  Table-C-split `wprf` emission in
  `prefetch_placesingle.cpp:1335-1338`) is correct on its own
  merits — the code shape matches the binary's shared tail at
  `0x1db935` — but **cannot be test-verified from SeqC**.  The
  fix protects against future code paths that might wire the
  Table dispatch up (e.g. via JSON-AST input or a future custom
  function), without affecting any currently-reachable behaviour.
- The `case NodeType::Table` arms across the prefetch pipeline
  (`prepareTree`, `countBranches`, `placeSingleCommand`,
  `print_tree`) and `AsmCommands::asmTable` are **dead in both
  binary and recon for the SeqC entry point**.  They are kept for
  binary fidelity (the `vtable` and dispatch tables in the
  binary still reference these branches).

### Action

None on code.  Documentation: D8 cases 2 and 3 marked obsolete
in TODO.md.  Future work that needs to exercise these paths
should consider adding a JSON-AST pybind binding rather than
trying to coerce a SeqC source through.

## IF-250  `AWGAssemblerImpl::asmSource_` and `unusedStr038_` are layout-swapped

**Severity**: medium (cosmetic / reverse-engineering accuracy;
no behavioural diff observed in the diff-test suite).

**Status**: **fixed** (2026-05-12) — `+0x20` slot renamed to
`unusedStr020_`, `+0x38` slot renamed to `asmSource_`; ctor init
list reordered to match physical offset order; pipeline writer at
`assembleFile` and reader at `writeToFile` now correctly target
the `+0x38` slot by name as well as by data flow.  Full diff-test
suite passes 1603/1603 with no ELF-byte changes (the swap is
purely a cosmetic / naming realignment; both fields were
default-constructed string slots, so semantics are unchanged).
Promoted to TODO.md as **D-AUDIT-2** on 2026-05-12; closed same
day.

**Discovered**: D7 `\unclear` triage (subagent
`ses_1e7d6eb12ffeR6yx6GhCf6l4gO`).

The reconstructed `AWGAssemblerImpl` declares two `std::string`
slots:

```cpp
std::string asmSource_;     // 0x020   (per recon)
std::string unusedStr038_;  // 0x038   (per recon)
```

with `asmSource_` carrying the cached `.asm` source text and
`unusedStr038_` documented as a placeholder.  Disassembly
contradicts that ordering — `this+0x38` is the slot the binary
actually uses as the cached source, and `this+0x20` is the
unused one.

### Evidence

`AWGAssemblerImpl::assembleFile` (`0x285ec0`):

```
0x286047: lea r15, [rbx+0x38]                  ; r15 = &this->str038
0x28604b: test BYTE PTR [rbx+0x38], 0x1        ; SSO check (free old long form if any)
0x2860ac: mov rsi, QWORD PTR [rbx+0x38]
0x2860b0: mov rdi, QWORD PTR [rbx+0x48]
0x2860b8: call _ZdlPvm                         ; release previous long-form buffer
0x2860c1: mov QWORD PTR [r15+0x10], rax        ; write new string capacity
0x2860c9: movups XMMWORD PTR [r15], xmm0       ; write new string size + flag
0x2860d3: call AWGAssemblerImpl::assembleString(rsi=r15)
```

i.e. `this->str038 = <file contents>; assembleString(this->str038);`.

`AWGAssemblerImpl::writeToFile` (`0x288570`):

```
0x288827: mov BYTE PTR [rbp-0x40], 0x8         ; build local string ".asm"
0x28882b: mov DWORD PTR [rbp-0x3f], 0x6d73612e ;   .asm
0x288836: movzx eax, BYTE PTR [r14+0x38]       ; SSO byte of this->str038
0x28883b: lea rsi, [r14+0x39]                  ; short-form data ptr
0x288845: cmovne rsi, QWORD PTR [r14+0x48]     ; long-form data ptr if SSO bit set
0x28884a: cmovne rdx, QWORD PTR [r14+0x40]     ; long-form length
0x28885a: call ElfWriter::addData(elf, data, len, &".asm")
```

i.e. `elf.addData(this->str038.data(), this->str038.size(), ".asm")`.

The recon's pipeline file even **already documents** the
correct layout in a comment block —
`reconstructed/src/codegen/awg_assembler_impl_pipeline.cpp:48-54`:

```
//   +0x08  std::string filename_          — source filename
//   +0x20  (padding?)
//   +0x38  std::string asmSource_         — assembled source text
```

— but the header declares them inverted, and the pipeline body
then writes/reads through the wrongly-named slot.

### Why the diff-test still passes

Behaviourally the data still flows the same way: contents go
into one slot, `assembleString` reads from the same slot,
`writeToFile` writes from the same slot.  The bug is purely
that the slot at +0x20 (which the recon calls `asmSource_`) is
the dead one in the binary, while the binary's real source
cache lives at +0x38 (which the recon calls `unusedStr038_`).

### Action

Promote to a TODO entry (medium): rename the +0x20 field to
`unusedStr020_`, rename +0x38 to `asmSource_`, retarget the
ctor initialiser list and the two consumers
(`assembleFile`, `writeToFile`) at the new name.  After the
swap, drop the `\unclear` on the new +0x20 slot and replace
its brief with the same "constructed/destructed only" wording
used for the other layout placeholders.

Verify ELF byte-equality after the rename.

## IF-251  `getApiErrorMessage` consults wrong container (recon vs binary)

**Severity**: cosmetic (observable mapping coincides today).

**Status**: **fixed** (2026-05-12) — `getApiErrorMessage` in
`reconstructed/src/core/error_messages.cpp` now consults a
dedicated anonymous-namespace `apiErrorMessages` map populated
at static-init time with the same 52 entries (16384–16389 minus
16388, 32768–32800, 36864–36877) the binary's BSS table at
`0xb85230` holds.  The compiler-diagnostic
`ErrorMessages::messages` map is no longer the lookup target.
Strings are duplicated rather than copy-derived, mirroring the
binary's two-independent-tables data flow.  Full diff-test suite
passes 1603/1603 (no observable change since the two tables
agree on shared keys).  Promoted to TODO.md as **D-AUDIT-3** on
2026-05-12; closed same day.

**Discovered**: D7 `\verifyme` triage batch 2 (subagent
`ses_1e7d64aa6ffelLz6pUQpwrg6rv`), `error_messages.cpp:142`.

The reconstructed `ErrorMessages::getApiErrorMessage` body
calls `ErrorMessages::messages.find(code)` and falls back to
the unknown-error string at `0x962ba8`.  The binary's
`getApiErrorMessage` at `0x2e4820` instead consults a separate
anonymous-namespace flat-hash table:

```
0x2e4820: ...
0x2e4??: mov 0x8a0a11(%rip),%r8 # b85238 <_ZN6zhinst12_GLOBAL__N_116apiErrorMessagesE+0x8>
```

i.e. it reads from `apiErrorMessages` at BSS `0xb85230`, not
from the public `ErrorMessages::messages` map.

### Why the diff-test still passes

The keys both tables hold (16384–16389, 32768–32800,
36864–36877) map to identical strings, so user-visible output
matches.  Any future divergence between the two tables would
surface as a behavioural diff.

### Action

Promote to a TODO entry (cosmetic): introduce an
anonymous-namespace static `apiErrorMessages` table mirroring
the binary's BSS layout, and have `getApiErrorMessage` read
from it instead of from the public messages map.  This keeps
the recon faithful to the binary's data-flow even where the
observable result already matches.


## IF-252  `CalVer::triple()` returns `CalVer const&` for caller-side reinterpret

**Severity**: cosmetic (no callers in binary or recon today;
strict-aliasing UB only materialises if a caller actually
reinterpret-casts the result).

**Status**: **kept (documented)** (2026-05-12) — `\binarynote` on
`infra/calver.hpp` already flags the strict-aliasing hazard; zero
call sites exist in either the binary or the recon, so the UB is
latent.  No source change required; revisit only if a caller
materialises or if the function is ever deleted.

**Discovered**: D7 `\binarynote` triage (asm+infra batch).

`CalVer::triple()` (`infra/calver.hpp:96`, definition
`infra/calver.cpp:135`) is declared to return `CalVer const&`
and the binary at `0x100260` does exactly `mov %rdi,%rax;
ret` — i.e. it returns the receiver pointer unchanged.  The
declared contract (per the `\binarynote` block on the recon
header) is that callers may then reinterpret the returned
reference as `std::array<size_t, 3> const&`, exploiting the
fact that `year_`/`month_`/`patch_` occupy the same first 24
bytes as `array<size_t, 3>`.

### Why this is flagged

1. The reinterpret would be a strict-aliasing violation
   (`CalVer` and `std::array<size_t, 3>` are unrelated types).
   GCC and Clang are permitted to assume aliased loads through
   the two pointers do not alias and may reorder/coalesce
   accesses.
2. `std::array<T, N>` is permitted to contain trailing padding
   for alignment, although on x86-64 with `size_t = uint64_t`
   this is not an issue in practice.

### Why it does not bite today

A grep across the entire codebase finds zero call sites in the
recon (only the declaration and definition).  An
`objdump`-level search of `_seqc_compiler.so` for callers of
`_ZNK6zhinst6CalVer6tripleEv` (mangled symbol) finds zero call
sites in the binary either.  The function is effectively dead
code preserved for ABI / vtable parity.  As long as nothing
calls it, the UB never materialises.

### Action

None required.  Document in the existing `\binarynote` on the
header (already present) that the reinterpret would be UB if
performed.  Consider deleting the function entirely once we
are confident no out-of-tree consumer relies on it; for now
keep it for binary-faithfulness.


## IF-253  `runningOnMfDevice` recon claims `/proc/device-tree/model` check, binary parses LabOne manifest

**Severity**: cosmetic (constant-`false` stub was observably correct on every PC).

**Status**: **fully resolved** (2026-05-12, D17) — the symbol family
has been reconstructed for real in
`reconstructed/{include,src}/io/zi_environment.{hpp,cpp}` (eight
public entry points: `runningOnMf{,64}Device(){,(string)}`,
`hasMediaDevNode`, `makeDirectories`, `markFileHidden`,
`initBoostFilesystemForUnicode`).  The stub at
`reconstructed/src/core/stubs.cpp:34` was removed in the same
change-set; the comment that remains there is a one-line breadcrumb
pointing at the new home.  All 1603 difftests pass on a PC test-host
(manifest absent → `false` cached → behaviour unchanged from the
old stub for the workstation case).

**Discovered**: D7 `\verifyme` triage (batch B) of `core/stubs.cpp:36`.

The reconstructed stub at `reconstructed/src/core/stubs.cpp:18-40`
carried an inline comment "`0x2ce770` — checks `/proc/device-tree/model`
for `Zurich Instruments MF`" and a `\details` paragraph repeating
the same claim.  Both were wrong on two counts:

1. **Wrong address.**  `0x2ce770` is inside
   `ZiFolder::sessionSaveDirectoryName`, not at any function
   boundary.  The actual `zhinst::runningOnMfDevice()` symbol
   lives at `0x2ec580` (a separate single-arg overload taking a
   `std::string const&` exists at `0x2ec160`).
2. **Wrong mechanism.**  The body at `0x2ec580` does not touch
   `/proc/device-tree/model` (no such literal exists in the
   binary; `strings _seqc_compiler.so | grep device-tree` is
   empty).  Instead it calls
   `(anonymous namespace)::readManifest()` (which lazily parses
   a `boost::property_tree::ptree` into a function-local static
   `laboneManifest`) and then
   `(anonymous namespace)::isMf(laboneManifest)`, caching the
   resulting `bool` in a function-local static `runningOnMf`
   gated by a Meyers-singleton `__cxa_guard`.

The original constant-`false` stub remained observationally
correct on any PC-hosted test run because the manifest lookup
on a workstation also returns `false`; all difftests passed
through both the stub era and the proper reconstruction.

### Resolution

`reconstructed/src/io/zi_environment.cpp` mirrors the binary's
anon-namespace helpers (`readManifestImpl`, `doIsMf`, `isMf`,
`isMf64`, `laboneManifest`) and the eight public entry points.
The `0xb852b0` Meyers-singleton ptree is modelled by the
`laboneManifest()` function-local static; the per-public-entry
`__cxa_guard`-protected `bool` caches at `0xb85290` /
`0xb852a0` are modelled by separate function-local statics in
`runningOnMfDevice()` / `runningOnMf64Device()`.  Path-arg
overloads bypass the cache and call the helpers fresh, matching
the binary's split between the cached and uncached forms.

## IF-254  `magic_numbers_proposal.md` had wrong `Variable::flags` type and Frozen-bit position

**Severity**: cosmetic (notes-file only; no source-code impact).

**Status**: fixed (notes file rewritten to match canonical header,
2026-05-11).

**Discovered**: D6-B batch 2 promotion audit of
`reconstructed/notes/magic_numbers_proposal.md`.

The notes file's "A4. `Variable::flags`" entry described the field
type as `uint16_t` and the Frozen flag as bit 1 (mask `0x02`).  The
canonical header `reconstructed/include/zhinst/runtime/resources.hpp`
has been the authority since at least the resources rework:

- `int16_t flags;` (signed, not unsigned) at offset +0x50.
- `static constexpr int16_t VarFlag_Written = 0x01;` (bit 0 — matches notes).
- `static constexpr int16_t VarFlag_Frozen  = 0x100;` (bit 8 — notes
  said bit 1, which is wrong).

`Frozen` lives in the **high byte** of the 16-bit `flags` word; the
low byte holds `Written`/initialised state and `Frozen` is set by
`Function::addArgument` to gate subsequent `update*`-with-value calls.
This was already verified-correct in `resources.cpp` (`v.flags |=
static_cast<int16_t>(scopeBoundaryFlags_)` patterns, where
`scopeBoundaryFlags_` is OR'd into the high byte).

The notes file was the only secondary source carrying the wrong
bit position; corrected during the page promotion.  No source-code
or test changes were required.

## IF-255  `swap(SeqCValue&, SeqCValue&)` header brief contradicted the cpp body

**Severity**: low (documentation only; no behavioural impact).

**Status**: fixed (header brief rewritten to match the verified body,
2026-05-12).

**Discovered**: D12 `\binarynote` triage audit
(`reconstructed/src/ast/seqc_ast_node.cpp:1007` vs
`reconstructed/include/zhinst/ast/seqc_ast_node.hpp:2023`).

### Observation

The header declaration

```cpp
friend void swap(SeqCValue& a, SeqCValue& b);
```

carried a `\brief` claiming the function "exchanges the base
subobject and the tagged payload (`payload_` + `tag_`)".  The
verified body at `0x1fe410` (recon at `seqc_ast_node.cpp:1007`)
swaps **only** `varType_` and the tagged payload; the base subobject
fields (`valueCategory_`, `lineNr_`, `direction_`) stay with their
respective objects.  The body itself carries a `\binarynote` that
correctly describes this asymmetry, but the header brief contradicted
it — a textbook secondary-source drift of the kind AGENTS.md
"Verify-then-write" §2-3 specifically warns against (the brief was
likely written from the surrounding "SeqCAstNode-family swap"
pattern rather than from the function body).

### Why this matters

D12 audit confirmed the cpp-side `\binarynote` is genuine
"non-idiomatic caller-visible surprise" (kept, not demoted).  A
caller relying on the header brief would assume full memberwise
swap and silently keep the wrong `lineNr_` / `valueCategory_` /
`direction_` values on each operand — exactly the contract this
function does not provide.

### Action

Header brief rewritten to spell out the partial-swap contract and
cross-reference the definition's `\binarynote` for the rationale.
Body unchanged.  No tests exercise the asymmetry directly (and
`SeqCValue::swap` does not appear on any difftest path), so no
regression test added; the inline brief alignment is the canonical
remediation.

### Lesson

Reinforces the verify-then-write rule: when writing a header brief
for a function whose definition lives in a separate `.cpp`, the
authoritative source is the **body**, not the surrounding "looks
like the other swaps in this family" pattern.

---

## IF-256  `ElfReader::ddSectionIndex_` `\unverifiable` rationale was wrong about reads

**Severity**: low (documentation only; no behavioural impact).

**Status**: fixed (rationale rewritten to match verified usage,
2026-05-12).

**Discovered**: D13 `\unverifiable` audit
(`reconstructed/include/zhinst/io/elf_reader.hpp:228` vs
`reconstructed/src/io/elf_reader.cpp:175`).

### Observation

The header carried an `\unverifiable` rationale claiming the field
is "written by the constructor but never read by any reconstructed
accessor."  The verified body of `ElfReader::getWaveform()`
(recon at `elf_reader.cpp:177-178`, binary `@0x2c3d40`) explicitly
reads `ddSectionIndex_` as the index into `ddSections_`:

```cpp
if (ddSectionIndex_ >= ddSections_.size()) return result;
auto* sec = ddSections_[ddSectionIndex_];
```

The brief at line 222 of the same header even acknowledges this
("Current selector into `ddSections_` used by `getWaveform()`"),
so the rationale text contradicts both the body and the sibling
brief in the same file.

### Why the site is still `\unverifiable`

The field is **read** but never **written to a non-zero value** in
any reconstructed code path, and there is no `compile_seqc` input
that drives `getWaveform()` to observe a non-zero selector.  The
"unverifiable" property is "the binary's purpose for a non-zero
selector cannot be confirmed", not "the field is never read".

### Action

Header rationale rewritten to accurately describe the situation:
field is read by `getWaveform()` but always observed as 0 in
reconstructed paths, so the semantic role of a non-zero value
remains unverified.  Tag remains `\unverifiable`.

### Lesson

When filing an `\unverifiable` rationale, distinguish "no read
sites" from "no observable non-default value".  Confirm against the
current `.cpp` body, not against a prior recollection of the field
state.

## IF-257  `platform.cpp` carried a duplicate, partially-wrong copy of the manifest helpers

**Severity**: cosmetic (anon-namespace duplicates with no callers; no
linker collision, no observable behaviour).

**Status**: fixed (2026-05-12, D17) — the dead anon-namespace block in
`reconstructed/src/core/platform.cpp` has been removed; canonical
implementations live in `reconstructed/src/io/zi_environment.cpp`.

**Discovered**: D17 (zi_environment cluster reconstruction).

While reconstructing `zhinst::runningOnMfDevice` and friends, the
binary's anon-namespace helpers (`readManifest(string)`,
`readManifest()`, `doIsMf`, `isMf`, `isMf64`) were found to be
already present as a dead anon-namespace block in
`reconstructed/src/core/platform.cpp:29-101`.  They had no callers in
that translation unit (the only intended caller was the manifest-
driven `runningOnMfDevice`, which never landed there because the
symbol was stubbed in `core/stubs.cpp` instead).

In addition to being dead code, the `isMf64` body in `platform.cpp`
documented the platform-string check as just `platform.size() == 10`
with a stale `// "mf_dev5640" is 10 chars` annotation.  The actual
binary at `0x2ec430` checks `platform.size() == 10 AND
platform == "linuxARM64"`, encoded as a 10-byte XOR against the
literal `"linuxARM64"` (8-byte XOR `0x4d524178756e696c` = "linuxARM"
plus 2-byte XOR `0x3436` = "64").  The body would have been
behaviourally wrong on any non-linuxARM64 manifest with a
length-10 platform string (e.g. a hypothetical `"mf_dev5640"` value
would have been mis-classified as MF64).

### Resolution

The whole anon-namespace block, plus the unused
`#include <boost/property_tree/...>` lines, were removed from
`platform.cpp`.  A short breadcrumb comment in their place points
at `src/io/zi_environment.cpp` for the canonical home.  The new
implementation in `zi_environment.cpp` carries the correct
`platform == "linuxARM64"` byte-for-byte check.

### Lesson

Speculative anon-namespace stubs in unrelated TUs are easy to lose
track of; whenever a stub gets written ahead of its real
reconstruction, leave a TODO entry pointing at it.  When the proper
reconstruction lands, sweep for older copies in sibling files
before declaring the cluster done.


## IF-258  D18 cluster `ast_misc` was already substantially reconstructed; only `SeqCRepeat::cond()` accessor was misnamed `count()`

**Severity**: minor (one accessor name disagreed with binary; no
behavioural divergence — field semantics and clone wiring were
correct).

**Status**: fixed (2026-05-12, D18) — `SeqCRepeat`'s `SEQC_BINARY`
instantiation in `seqc_ast_node.hpp:1196` and `SEQC_BINARY_IMPL` in
`seqc_ast_node.cpp:508` both renamed first child accessor from
`count` → `cond`; sole caller in `seqc_ast_eval_control.cpp:2368`
updated; `\binarynote` added to header explaining the binary's
quirky accessor name (the field is semantically the repeat count
but the binary spells the accessor `cond()`).

**Discovered**: D18 audit (clone-virtualisation cluster).

The TODO description for D18 claimed:

> recon currently routes every clone through one non-virtual
> `Node::clone()` at `reconstructed/src/ast/node.cpp:221`, meaning
> per-subclass payloads can be silently sliced when cloned through a
> `Node*` base pointer.

This was based on a misread.  `Node::clone()` at
`src/ast/node.cpp:221` is the **codegen IR** `Node`, an unrelated
class hierarchy.  The AST base `SeqCAstNode::doClone()` is **already
pure virtual** at `seqc_ast_node.hpp:191`, and all 53 subclasses
already override it via the family macros (`SEQC_TRIVIAL_LEAF_IMPL`
×6, `SEQC_UNARY_IMPL` ×5, `SEQC_OPERATOR_IMPL` ×22, `SEQC_BINARY_IMPL`
×5, `SEQC_LIST_IMPL` ×4) plus 11 hand-written overrides
(`SeqCOperation`, `SeqCOperator`, `SeqCFunctionCall`, `SeqCArray`,
`SeqCCaseEntry`, `SeqCIfElse`, `SeqCCondExpr`, `SeqCFunction`,
`SeqCForLoop`, `SeqCVariable`, `SeqCValue`).  Total: 53 = the binary's
53 `SeqC*::clone() const` symbols.

The only real divergence was that the binary publishes
`SeqCRepeat::cond() const` at `0x203b80` (returns +0x18) and
`SeqCRepeat::body() const` at `0x203b90` (returns +0x20), with no
`count()` symbol — so the recon's accessor name `count()` did not
match.  The fix is purely cosmetic at the API level (no callers
outside `seqc_ast_eval_control.cpp` used the old name).

### Naming asymmetry (deliberate)

The recon spells the AST clone slot `doClone()` while the binary
spells it `clone()`.  This recon-side rename is **deliberate** —
it disambiguates the AST clone from the unrelated codegen IR
`Node::clone()` at `src/ast/node.cpp:221`.  No fix needed; left
as-is.

### Lesson

When a TODO description cites a specific function as the source of
a bug, verify it exists with the claimed signature in the cited
file before designing a fix.  A 30-second `grep` would have caught
the AST-vs-codegen confusion months before D18 was scheduled.

## IF-259  D19 audit: most "absent" small-cluster symbols are already covered or non-actionable

**Severity**: cosmetic / informational.

**Status**: documented (2026-05-12, D19) — closure note for the
small-cluster bundle.  See OVERVIEW.md "D19 (small-cluster bundle)
complete" section for the full per-cluster triage table.

**Discovered**: D19 audit (small-cluster bundle).

The D19 cluster list of ~28 symbols across 10 small clusters
(`exceptions`, `numeric`, `random`, `base64`, `awg_config`,
`node_misc`, `compiler_helpers`, `misc`, `anon_helpers`,
`device_option`) was triaged against the current recon state.
Findings:

1. **`Random::seedRandom()` (297 B @0x16be80)** — already
   functionally covered.  The binary's body opens
   `std::random_device("/dev/urandom")` and seeds
   `mt19937_64` into the TLS state array
   `GlobalResources::random[313]`.  The recon does the exact same
   work via `seqc_libcxx_mt19937_seed_urandom(GlobalResources::random)`
   called from `CustomFunctions::randomSeed`
   (`runtime/custom_functions_playback.cpp:887`), wrapping the
   libc++ shim in `infra/prng_libcxx.cpp:63`.  The binary's
   `Random::seedRandom()` is just the inlined version of that
   exact sequence on the TLS array, treated as a `Random`
   "this" pointer.  No additional reconstruction needed.

2. **`WavetableFront::dummyWarning(string const&, int)` (159 B
   @0x29ac60)** — stub filled with a one-line `boost::log` warning
   record matching the binary's behaviour: `LogRecord(Severity::
   Warning) << "Warning not tracked: " << msg`.  The binary's
   159 B include inlined LogRecord ctor/dtor + two
   `formatted_write` calls; the recon defers to the
   `infra/logging.hpp` `operator<<` overloads for identical
   semantics.  Test suite always installs a real callback so the
   change is invisible to difftests.

3. **`tracing::TraceProvider::~TraceProvider() (105 B @0x0fa5e0)`**
   — `= default` is exact-equivalent.  The binary's 105 B body
   inlines the refcount-decrement and conditional vtable-call
   shutdown sequence for `nostd::shared_ptr<TracerProvider>`
   `provider_`; the recon's `= default` defers the same sequence to
   the member's own dtor.  Comment upgraded; no code change.

4. **`SeqCIfElse::operator=(SeqCIfElse) (367 B @0x201f70)` and
   `SeqCCondExpr::operator=(SeqCCondExpr) (367 B @0x203d20)`** —
   already correct as copy-and-swap (`swap(*this, o); return
   *this;`).  The binary's 367 B inline the per-child
   `doClone()` into the operator body itself rather than relying
   on the by-value parameter's copy-ctor invocation; behavioural
   semantics are identical.  Comments upgraded; no code change.

5. **`exceptions::core` cluster (13 boost::wrapexcept thunks)** —
   D14 inventory already noted "no hand-written code is required;
   this cluster is informational only".  These are MI offset
   thunks auto-emitted by the compiler when the exception class
   hierarchy uses multiple inheritance.  No recon work needed.

6. **`base64::encode` (910 B), `numeric::*` (3 helpers, 394 B),
   `compiler_helpers::nodeListToJson` (796 B, currently inlined
   into `awg_compiler.cpp`), `awg_config::AwgPathPatterns ctor`
   (254 B), `device_option::*` (90 B), `node_misc::pauPoffIwrap`
   (43 B), `misc::*` (5 helpers, 570 B)** — zero observable
   callers outside the binary; difftest suite cannot exercise
   them.  Subject to the D16 caveat (`\unverifiable` regime).
   Deferred to the grand finale per user direction.

### Conclusion

The original D19 work estimate ("~28 symbols across 10 clusters
reconstructed in a single batched commit series") was based on
the D14 inventory's "absent" tag, which conflated three distinct
states: (a) symbol absent because the binary inlines what the
recon does as a free function, (b) symbol absent because the
recon stubbed it, and (c) symbol absent because nobody calls it
in difftests so it was deferred.  Only (b) requires real
reconstruction work — and after this audit, only `dummyWarning`
fell into that bucket cleanly (the two `operator=` were already
correct in semantics; only their comments needed updating).

### Lesson

Cluster-level counts in the D14 inventory should be treated as
upper bounds on remaining work, not as estimates.  Triage is
mandatory before scheduling: every "absent" symbol may be
already-covered, semantically-equivalent-via-different-shape, or
genuinely-stubbed; only the third category needs source changes.

---

## IF-260  diagnostics_text "Cluster-wide observations" claimed entity strings unused

**Severity**: cosmetic (stale documentation)
**Status**: fixed

The "Cluster-wide observations" block at the end of the D16-cluster-5
section of `diagnostics_text.md` claimed that the rodata literals
`"&amp;"`, `"&Omega;"`, `"&#937;"` at `0x90ce98..0x90ceaf` were
"not referenced from any of the five functions above" and tracked
them as "a pointer to a not-yet-analyzed sibling that does HTML-entity
escaping".

This was already obsolete: the D16 batch section *immediately below*
in the same file fully documents `entityNameToNumber`,
`entityNumberToTxt`, `xmlUnescape`, and `xmlUnescapeCopy` as the
consumers of that table, with full byte-mapped entity tables at lines
1776..1790 and 1845..1855.

Re-verified by `objdump -d --start-address=0x2f4290 ...` showing
27+ `lea` instructions in `entityNameToNumber` referencing
`0x90ce47..0x90cef1`, and by inspection of the existing notes section.

Fixed by rewriting the bullet at line ~1535 to point readers to the
D16 batch and to identify the still-unanalyzed siblings
(`zhinst::quote` @0x2fa6a0, `zhinst::xmlEscapeCritical` @0x2fa7e0,
`zhinst::xmlEscapeUtf8Critical` @0x2faaa0) that ALSO consume the
same rodata block at `0x2fa8b7`/`0x2fa957` etc.

This is exactly the failure mode the verify-then-write rule warns
about: a summary written in an earlier session became false when a
later session added the D16 batch but did not back-update the
cluster-wide summary.


## IF-261  D14 inventory missed `zhinst::quote` from diagnostics_text cluster

**Severity**: cosmetic (scope inventory gap)
**Status**: fixed (scope expanded in D16)

The D14 inventory cluster `diagnostics_text::core` listed 19 symbols
totalling 33,844 B.  During D16 subagent analysis, a 20th symbol was
discovered that uses the same `.rodata` entity table:

- `zhinst::quote(std::string&)` @0x2fa6a0, 311 B

The symbol was already reconstructed in `src/core/platform.cpp:193`
under an older pass, so no new code is needed.  D16 scope was
expanded to acknowledge it (notes file `diagnostics_text.md`
documents the body), but no duplicate body was added to
`src/core/diagnostics_text.cpp` — the platform.cpp implementation
remains canonical.

Lesson: when a cluster is enumerated by reading caller graphs, leaf
functions with only callsite-string-table evidence can be missed.
D14's enumeration relied on call-graph traversal; `quote` is a
pure leaf (no callers, no callees of interest), so it didn't surface
through that scan.

## IF-262  `xmlUnescape` recon lacks `escapeMaliciousXmlEscapedSequences` post-pass

**Severity**: cosmetic
**Status**: dismissed (superseded by IF-263; see below)

During D16 reconstruction of `zhinst::xmlUnescape` (@0x2fadd0), the
disassembly was observed to construct TWO function-local static
`boost::regex` objects (Itanium guards at b85308 and b85350), both
from the same pattern at `.rodata` 0x90d057
(`&#x[0-9a-fA-F]+;|&#[0-9]+;|&amp;|&lt;|&gt|&quot;|&apos;`).  The
second static is used to drive a secondary `regex_search` pass over
the decoded output — likely a sanitiser that re-escapes
maliciously-encoded XML escape sequences (e.g. an input that decodes
to a literal `&amp;` should not be left as `&amp;` after decoding).

The recon at `src/core/diagnostics_text.cpp:42` implements only the
first decoding pass and assigns the result back to `s` without the
secondary sanitiser pass.  There is no test path through the Python
bindings that exercises `xmlUnescape`, so the gap is invisible to the
test suite.

Reproduction (when a diff-test harness exists): input a string like
`"&amp;amp;"` — the binary should produce `"&amp;"` (one decode +
re-escape), the recon currently produces `"&"` (one decode).

Action: identify the second-pass helper symbol (candidate names:
`escapeMaliciousXmlEscapedSequences`, `escapeAlreadyEscaped`,
something in an anonymous namespace).  Disassemble at the address
identified by the b85350 guard's protected call.  Reconstruct as a
helper in the same TU.

**Update (2026-05-12, Phase E2):** Both the regex-pattern claim and
the prediction in this entry were wrong.  Direct file-offset readout
of `.rodata @ 0x90d057` shows the actual NUL-terminated pattern is
26 bytes:

    &#x[0-9a-fA-F]+;|&#[0-9]+;

— numeric character references **only**, no named-entity
alternations.  GDB-tracing the original on `"&amp;"` confirmed that
the first `regex_search` returns `false` immediately and the input
is appended verbatim as the trailing tail.  The post-pass
`escapeMaliciousXmlEscapedSequences` (also a numeric-only regex)
is consequently a no-op for any input the first pass leaves intact,
because no decoded UTF-8 byte sequence can re-form a `&#...;`
shape.  The "post-pass gap" hypothesis was based on a notes line
(`diagnostics_text.md:1588`) that invented the named-entity
alternations from intuition rather than from reading the bytes.

Promoted to IF-263 (the actual recon bug — wrong regex pattern,
fixed) and IF-264 (the latent bug surfaced by IF-263's fix —
`xmlEscapeSeqToInt` failing to trim the `&#...;` boilerplate, also
fixed).

---

## IF-263  `xmlUnescape` recon used a regex pattern that did not match the binary

**Severity**: bug
**Status**: fixed (2026-05-12, Phase E2)

`reconstructed/src/core/diagnostics_text.cpp` (function
`xmlUnescape`) initialised its function-local `static boost::regex`
from the literal

    "&#x[0-9a-fA-F]+;|&#[0-9]+;|&amp;|&lt;|&gt|&quot;|&apos;"

— a 47-byte pattern that matches the two numeric-character-reference
forms **and** five named entities.  The `diagnostics_text.md` notes
(line 1588) and the body sketch (lines 1631..1689) both cited this
same 47-byte string as living at `.rodata @ 0x90d057`.

Phase-E2 diff-test harness probed the original `xmlUnescape` with
named-entity inputs and found:

    "&amp;"      → "&amp;"        (unchanged)
    "&gt;"       → "&gt;"         (unchanged)
    "a &amp; b"  → "a &amp; b"    (unchanged)
    "&#65;"      → "A"            (decoded)
    "&#1234;"    → "\xd3\x92"     (decoded — U+04D2 in UTF-8)

Direct readout of `_seqc_compiler.so` at file offset `0x90d057`
(.rodata is loaded at the same VA on x86_64-linux PIC `.so`s) shows
26 NUL-terminated bytes:

    "&#x[0-9a-fA-F]+;|&#[0-9]+;"

— numeric forms only.  GDB-tracing the original on `"&amp;"`
additionally confirmed that the first `regex_search` returns
`false`, the temp string `out` stays empty until the trailing-tail
append at the end of the loop, and the post-pass
`escapeMaliciousXmlEscapedSequences` (which uses the SAME 26-byte
pattern via the `b85350` guard) leaves the result untouched.

Fix: corrected the regex literal in `xmlUnescape` to the actual
26-byte pattern.  Updated the body-sketch and pattern-listing in
`reconstructed/notes/diagnostics_text.md` to match.

After this fix, `tests/diff_unreachable/harness.py --filter
xmlUnescape` passes 22/24 cases; the remaining 2 (`&#65;`,
`&#1234;`) surfaced IF-264.

---

## IF-264  `xmlEscapeSeqToInt` recon did not trim `&#`/`;` boilerplate

**Severity**: bug
**Status**: fixed (2026-05-12, Phase E2)

`reconstructed/src/core/platform.cpp::xmlEscapeSeqToInt` is called
by `xmlUnescape` with the **full match** of an XML numeric character
reference, e.g. `"&#65;"` or `"&#x41;"`, including the leading `&#`
prefix and the trailing `;`.  The recon's body fed the entire range
to `std::istringstream`'s `operator>>(int&)`, which stops at the
first non-digit (`&`) and leaves `result` at its initial value `0`.
Every numeric reference therefore decoded to U+0000 (a single NUL
byte after UTF-8 encoding).

The original binary at `0x2fc280` trims the boilerplate before
parsing:

  - **Decimal path** (`0x2fc2f0`):
    `add $0x2,%r15 ; dec %rbx`
    — drop the leading two bytes (`&#`) and the trailing `;`.
  - **Hex path** (`0x2fc3af`):
    `inc %r14 ; dec %rbx`
    — start one past the `x`/`X` (whose position was found by the
    initial `memchr`) and drop the trailing `;`.

Fix: in `xmlEscapeSeqToInt`, after the `'x'`/`'X'` scan, advance
`digitsBegin` by 2 in the decimal path (skip `&#`) and decrement
`digitsEnd` if the last byte is `;`.

This bug was latent until IF-263 was fixed: previously the
named-entity alternations in the bogus regex meant `xmlEscapeSeqToInt`
was called on a wider variety of matches and the wrong-zero return
was hidden by other regex-pattern mismatches.

After this fix, `tests/diff_unreachable/harness.py --filter
xmlUnescape` passes **24/24**, and the regular regression suite
remains at **1603/1603** (no caller of `xmlEscapeSeqToInt` is
reachable via the Python bindings, so this latent bug never broke a
difftest).

---

## IF-265  `truncateXmlSafe` recon used wrong submatch index and search start

**Severity**: bug
**Status**: fixed (2026-05-12, Phase E2b)

`reconstructed/src/core/diagnostics_text.cpp::truncateXmlSafe` had
two interlocking bugs that together caused 6 of 16 diff-test cases
to fail:

1. **Wrong regex search start.**  The recon walked back from byte
   `maxBytes` to find the most recent `&` and stored the position in
   a variable named `searchEnd`, but then passed `data` (not
   `searchEnd`) as the regex search start.  The `searchEnd`
   variable was never used.

2. **Wrong submatch index for the boundary check.**  The recon used

       const char* entityEnd = (m.size() >= 3) ? m[3].second
                                                : m[0].first;

   The pattern `&#x[0-9a-fA-F]+;|&#[0-9]+;|&amp;|...|&apos;` has no
   capture groups, so `m[3]` is empty/unmatched (`.first ==
   .second`).  Worse, the fallback used `m[0].first` (the entity
   START), so the boundary check was effectively comparing the
   entity-START to the cut, not the entity-END.  In practice the
   `if (entityEnd > data + maxBytes)` body almost never fired, and
   the recon fell through to the raw byte-truncation in
   `truncateUtf8Safe` — splitting entities mid-sequence.

GDB-tracing the original on `("abc&amp;def", n=5)` showed the
binary:
- starts the regex at `data + 3` (the `&` found by the back-up loop),
- gets `m[0]` = `"&amp;"` at offsets 3..8,
- compares `m[0].second (= data+8)` to `cut (= data+5)`,
- since `8 > 5`, calls `string::__erase_external_with_move(s, 3, npos)`,
- yields `"abc"`.

Fix: pass the back-up `&` position as the regex search start,
rename the local from `searchEnd` to `entityCandidate`, replace the
`m.size() >= 3` boundary check with the straightforward
`m[0].second > cut` test, and erase from `m[0].first - data` (the
entity start in the string) to end.

After this fix, `tests/diff_unreachable/harness.py` passes
**481/481** cases across 9 in-place mutators, and the regression
suite remains at **1603/1603** (truncateXmlSafe is binding-
unreachable).

---

## IF-266  `replaceUnit` recon diverges on every input — wrong append arg + branch logic

**Severity**: likely-bug
**Status**: **fixed** (2026-05-12, Phase E2c)

`reconstructed/src/core/diagnostics_text.cpp::replaceUnit`
diverged from the binary on **all 13** harness inputs.  Failure
shape: the recon's suffix-stripping branch appended the function's
`text` argument inside the trailing parentheses, but the original
uses `replacement` instead.  Even simpler cases like
`("5 V", "V", "volt")` showed the divergence:

| input                                | original                | recon (pre-fix)                |
|--------------------------------------|-------------------------|--------------------------------|
| `("5 V", "V", "volt")`               | `"5 V (volt)"`          | `"5 V (5 V)"`                  |
| `("a (V) b", "V", "volt")`           | `"a (volt) b"`          | `"a (V) b (a (V) b)"`          |
| `("VVV", "V", "volt")`               | `"VVV (volt)"`          | `"VVV (VVV)"`                  |
| `("5\\E V", "V", "volt")`            | `"5\\E V (volt)"`       | `"5\\E V (5\\E V)"`            |

### Root cause (GDB-verified)

Three independent reconstruction errors compounded:

1. **Wrong probe regex.**  The probing `regex_match` call at
   `+544 (0x2f7d00)` was reconstructed to use `matchSuffix`
   (the static `(.*?)(?: *\[[0-9]+\])+$` regex) but actually
   uses the **per-call regex `re`** built earlier — pattern
   `(.*?) *\(\Q<unit>\E\)(.*)`.  The `rcx = -0xc0(%rbp)`
   argument loaded at `+0x230 (0x2f7cf3)` is the per-call
   regex, written by `do_assign` at `0x2f7c4b`.

2. **Wrong probe flag.**  The flag passed in `r8d` is
   `0x400 = match_any`, not `match_partial` (`0x2000`).  The
   prior recap mis-mapped the bit value.

3. **Branch direction inverted.**  `bl=0` (no `(<unit>)` in
   `text`) takes the **plain-prose** path at `+0x294 (0x2f7d74)`:
   strip a trailing `[N]` from `text`, then append
   `" (<replacement>)"`.  `bl=1` (text already contains
   `(<unit>)`) takes the **bracketed-unit** path: substitute via
   the per-call regex with replacement `"$1 (<replacement>)$2"`,
   then strip a trailing `[N]` from the result.  The recon had
   these branches reversed AND used the wrong arg in the appends.

GDB output confirming the regex_match return for representative
inputs:

```
("5 V", "V", "volt")     -> regex_match(text, re, match_any) = 0  -> plain-prose path
("a (V) b", "V", "volt") -> regex_match(text, re, match_any) = 1  -> bracketed-unit path
("abc", "x", "y")        -> regex_match(text, re, match_any) = 0  -> plain-prose path
```

### Fix

`reconstructed/src/core/diagnostics_text.cpp::replaceUnit` was
rewritten to:

- probe with the per-call `re` (not `matchSuffix`) using
  `match_any` (not `match_partial`);
- on `!hasParenForm` (plain-prose), strip `[N]` from `text` and
  emit `"<stripped> (<replacement>)"`;
- on `hasParenForm` (bracketed-unit), `regex_replace(text, re,
  "$1 (<replacement>)$2")` then strip `[N]` from the result.

Result: 13/13 harness cases pass; full diff_test_fast suite
still 1603/1603.

### Lessons (logged into AGENTS.md spirit)

- Do not trust block-header summary comments (the recon's
  comment at lines 412–414 attributed the wrong arg to the
  binary and was cited as evidence — classic stale-secondary-
  source failure mode the AGENTS.md "Verify-then-write" rule
  warns about).
- When tracing argument-passing through a function with many
  intermediate calls, breakpoint **at the call instruction**
  and read `rcx`/`rdx`/etc — do not infer arg sources from a
  static read of the disassembly.
- `match_flags` numeric values: `match_any = 1024 = 0x400`,
  `match_partial = 8192 = 0x2000`.  Confirm against the Boost
  enum before naming a flag bit.

This finding closes Phase E2c for `replaceUnit`; the symbol's
`\verifyme` was promoted to verified in the same edit pass.

---

## IF-267  `toDeviceFamily` recon is a different algorithm AND inverted case-sensitivity

**Severity**: likely-bug (potentially high impact)
**Status**: **fixed 2026-05-13** (commit pending) — recon body in
`reconstructed/src/device/device_type.cpp:917` rewritten to match the
verified binary algorithm; 69-input regression suite added to the
`diff_unreachable` harness; full main suite still 1603/1603.

While preparing the harness inputs for `generateSfc`, a direct
ctypes probe of `zhinst::toDeviceFamily(string const&)`
(`_seqc_compiler.so` @ `0x2debd0`, recon at
`reconstructed/src/device/device_type.cpp:917`) revealed two
distinct divergences:

### Divergence 1 — case sensitivity inverted

The recon table is **lowercase** (`"hf2"`, `"uhf"`, `"mf"`,
`"hdawg"`, `"shf"`, `"pqsc"`, `"hwmock"`, `"ghf"`, `"qhub"`,
`"vhf"`).  Probe results vs. the original binary on the same
inputs:

| input        | original (`%eax`) | recon (`%eax`) | meaning              |
|--------------|------------------:|---------------:|----------------------|
| `"MF"`       | `0x004` (MF)      | `0x800` (Unknown) | original accepts upper, recon rejects |
| `"mf"`       | `0x800`           | `0x004`           | recon accepts lower, original rejects |
| `"MFLI"`     | `0x004`           | `0x800`           | same                 |
| `"mfli"`     | `0x800`           | `0x004`           | same                 |
| `"HDAWG8"`   | `0x008` (HDAWG)   | `0x800`           | same                 |
| `"hdawg8"`   | `0x800`           | `0x008`           | same                 |
| `"SHFQA4"`   | `0x010` (SHF)     | `0x800`           | same                 |
| `"shfqa4"`   | `0x800`           | `0x010`           | same                 |
| `"DEFAULT"`  | `0x001` (HF2)     | `0x001`           | match (both special-case `"DEFAULT"`) |
| `"none"`     | `0x000`           | `0x000`           | match                |
| `""`         | `0x000`           | `0x800`           | original returns `0`, recon returns `Unknown` |

The original accepts *uppercase* device-type strings (which is
what `compile_seqc(..., 'HDAWG8', ...)` actually passes); the
recon accepts *lowercase* strings.  These are exact opposites on
every input that drives the binary's main code paths.

### Divergence 2 — different algorithm entirely

The recon uses a `std::map<std::string, DeviceFamily>` with
`upper_bound` + `boost::algorithm::starts_with` for prefix
matching.  The original is **not** a `std::map` lookup at all:
disassembly at `0x2debd0..0x2debf5` shows a **string-length
jump table** (`cmp $0x7,%rdx ; ja Unknown ; jmpq *(...)`)
followed by inlined 4/8-byte immediate compares against the
embedded device-type literals (e.g. `cmpl $0x656e6f6e,(%rcx)` =
`"none"`).  This is a hand-coded length-bucketed dispatcher, not
an STL container lookup.

### Why this hasn't broken the main suite

Two compounding factors mask the bug from `diff_test_fast`:

1. The differential test compares **byte-for-byte ELF output**,
   not intermediate function returns.  If both pipelines reach
   the same final code generation through different routes —
   e.g. by funnelling through `GenericDeviceType` whose ctor
   computes `family` once and consumes only the
   `DeviceTypeCode` for actual codegen decisions — the wrong
   `family` value can be silently dead.

2. There may be an upstream `tolower` (or a parallel uppercase
   variant of the prefix table) that we have not yet
   identified, in which case the recon's lowercase table might
   be the table for *some other* call site and the *original*
   call site we probed uses a different table.  This needs
   verification.

### Action — investigation needed (TODO E4)

This finding is too large to resolve as part of E2c.  Logging
here so it is not lost; the proposed work is:

1. **GDB-trace** the binary's `toDeviceFamily` on the same
   inputs the probe used to confirm the mapping is what the
   ctypes probe says (i.e. that `"HDAWG8"` actually returns
   `0x008` and not via some accidental prologue fastpath).
2. **Disassemble** the full `0x2debd0..0x2dee00` range and
   reconstruct the actual length-bucketed dispatcher.
3. **Audit callers** — search the binary for every call site
   of `toDeviceFamily` and characterise the input strings.  If
   `"DEFAULT"` is the only common input on the main path, the
   wrong table could be live but unreachable in normal usage.
4. **Decide remediation**: either rewrite the recon body to
   match the original dispatcher, or document an explicit
   `\binarynote` if (and only if) we can prove the recon's
   semantics are observationally equivalent in all reachable
   call sites.

### Impact on E2c `generateSfc`

`generateSfc` cannot be diff-tested until this is resolved —
the inputs that drive the binary to the happy path
(`devType="MFLI"` etc.) drive the recon to the throw path, so
**every** `generateSfc` test case would diverge for reasons
unrelated to `generateSfc` itself.  Phase E2c `generateSfc` is
blocked on IF-267.

### Probe artefacts

- `/tmp/probe_devicefamily.py` — the ctypes probe script.  Uses
  the existing `diff_unreachable_string_make/free` shim symbols
  exported by the libc++ test .so.

### Update 2026-05-12 — empirical algorithm verified via extended probe

The extended probe (71 inputs spanning lengths 0..9 and the full
case×family matrix) directly invokes the original
`toDeviceFamily` symbol via a `CFUNCTYPE` at
`orig_base + 0x2debd0`, with the libc++ string built by the
candidate shim.  The orig column is ground truth; results
fully refute the earlier "len>7 → map" model.

**Verified algorithm**:

1. **Length-0 fastpath** → `0x000` (Unknown sentinel `0`).
   `""` returns `0` not `0x800`.
2. **Length-1, length-2** → no fastpath, no map entry of length
   ≤2; always returns `0x800`.  In particular `"MF"`(2)→`0x800`,
   `"HF"`(2)→`0x800`, `"S"`(1)→`0x800`.
3. **Length 3..6 and >7** → fall through to the lazy std::map
   path with `boost::starts_with` prefix matching.  Map keys are
   **uppercase**.  Confirmed key set (≥ 3 chars each):
   `MFI`(0x4), `SHF`(0x10), `HDAWG`(0x8), `UHF`(0x2), `GHF`(0x100),
   `VHF`(0x400), `QHUB`(0x200), `PQSC`(0x20), `HWMOCK`(0x40).
   - `MF`(2)→0x800 but `MFI`(3)→0x4 ⇒ map key for MF family is
     `MFI`, not `MF`.  `MFLI`/`MFIA`/`MFI` all match via prefix.
   - `SHF`(3)→0x10, `SHFA`/`SHFL`/`SHFLI`/`SHFQA42`(7)→0x10:
     prefix `SHF` matches anything starting with `SHF` *unless*
     a longer literal fastpath consumes it first (see length-7).
   - `UHFL`(4)→0x2, `UHFLI`(5)→0x2, `UHFAWG`(6)→0x2: prefix
     `UHF`.
   - `HDAWG822`(8)→0x8: confirms map path is reached for len>7
     too, key `HDAWG`.
   - `HF`(2)→0x800 but no `HF`-prefixed input tested returns
     HF2 → there is **no `HF` map key**.  HF2 is **only**
     reachable via the literal `"DEFAULT"` length-7 fastpath.
4. **Length-4 fastpath** — `"none"` → `0x000`.  `"NONE"` falls
   through to map → `0x800` (no `NONE` key).
5. **Length-7 fastpaths** — three case-sensitive literals:
   - `"DEFAULT"` → `0x001` (HF2)
   - `"SHFPPC2"` → `0x080` (SHFACC) — **bypasses** the SHF
     prefix-map (SHFACC is **not** a map key).
   - `"SHFPPC4"` → `0x080` (SHFACC).
   - All other length-7 inputs (e.g. `"default"`, `"Default"`,
     `"shfppc2"`) fall through to the map.  `"shfppc2"` →
     `0x800` (lowercase, no `SHF` prefix match).

**Updated dispatcher model — final, fully verified**
(static disasm + 87-input ctypes probe):

```
toDeviceFamily(s):
    n = s.size()
    if n == 0: return 0                                     # via jump-table[0]
    if n > 7: return mapLookup(s)                           # bypass jump table
    # jump-table dispatch (lengths 1..7), .rodata @ 0x961d88
    if n == 4 and s == "none":      return 0                # else fall through to map
    if n == 6 and s == "SHFACC":    return 0x80             # else fall through to map
    if n == 7:
        if s == "DEFAULT":          return 1                # HF2 sentinel
        if s == "SHFPPC2":          return 0x80
        if s == "SHFPPC4":          return 0x80
        # else fall through to map
    return mapLookup(s)
```

`mapLookup` is the lazy-init `std::map<string, DeviceFamily,
less<string>>` populated with exactly **10** entries (verified
by reading every pair-construction `lea`/`movl` pair in the
ctor block at `0x2ded89..0x2deecb`):

| key      | value | symbol                                       |
|----------|------:|----------------------------------------------|
| `"HF2"`  | 0x001 | `_ZN6zhinst12_GLOBAL__N_113hf2FamilyNameE`   |
| `"UHF"`  | 0x002 | `_ZN6zhinst12_GLOBAL__N_113uhfFamilyNameE`   |
| `"MF"`   | 0x004 | `_ZN6zhinst12_GLOBAL__N_112mfFamilyNameE`    |
| `"HDAWG"`| 0x008 | `_ZN6zhinst12_GLOBAL__N_115hdawgFamilyNameE` |
| `"SHF"`  | 0x010 | `_ZN6zhinst12_GLOBAL__N_113shfFamilyNameE`   |
| `"PQSC"` | 0x020 | `_ZN6zhinst12_GLOBAL__N_114pqscFamilyNameE`  |
| `"HWMOCK"`|0x040| `_ZN6zhinst12_GLOBAL__N_116hwmockFamilyNameE`|
| `"GHF"`  | 0x100 | `_ZN6zhinst12_GLOBAL__N_113ghfFamilyNameE`   |
| `"QHUB"` | 0x200 | `_ZN6zhinst12_GLOBAL__N_114qhubFamilyNameE`  |
| `"VHF"`  | 0x400 | `_ZN6zhinst12_GLOBAL__N_113vhfFamilyNameE`   |

Lookup mechanism (`0x2ded09..0x2ded61`):
1. `upper_bound(map, s)` → first key strictly greater than `s`.
2. If that's `end()`, return `0x800`.
3. Else step back to the largest key `≤ s` via the rb-tree
   parent/predecessor walk.
4. Call `boost::algorithm::starts_with(s, key)`.
5. If true, return `key`'s mapped `DeviceFamily` value;
   otherwise return `0x800`.

This means the map matches `s` against the **largest key
that is a prefix of s**.  For example `s="MFLI"` →
`upper_bound` returns `PQSC`, predecessor is `MF`,
`starts_with("MFLI","MF")=true`, returns 0x004.

**SHFACC has no map entry** — it is reachable only via the
length-6 literal fastpath and the length-7 `SHFPPC2`/`SHFPPC4`
fastpaths.  The recon must replicate this.

**`""` returns 0** (Unknown sentinel `0`) via jump-table[0]
which targets the epilogue with `%eax=0` (set by
`xor %eax,%eax` at `0x2dec0d` immediately before the
indirect jump).  This is observationally distinct from the
`0x800` sentinel returned by miss paths.

**Recon defects confirmed**:

- Map keys are **lowercase** in recon vs. uppercase in binary.
  Inverts every case decision.
- Recon special-cases `""` to return `0x800` (Unknown sentinel)
  vs. binary's `0`.
- Recon has no `"SHFPPC2"`/`"SHFPPC4"` → SHFACC mapping, and
  no `SHFACC` map entry either.
- Recon's prefix `mf` (2 chars) matches `"mf"` directly; binary's
  prefix is `MFI` (3 chars) so `"MF"` is *not* a match.

**Coverage decision**: the harness will exercise this via the
existing `diff_unreachable` infrastructure (direct ctypes
invocation against orig and recon symbols).  Curated input set
will cover: empty, `none`/`NONE`, `DEFAULT`/`default`,
`SHFPPC2`/`SHFPPC4`, every uppercase prefix (`MFI`, `SHF`,
`HDAWG`, `UHF`, `GHF`, `VHF`, `QHUB`, `PQSC`, `HWMOCK`) plus
representative lowercase counterparts to lock the case
behaviour.  Driving this through `compile_seqc` is **not
required** — the function is reached on a back-edge from
`generateSfc` etc. and the unit-level harness gives us crisper
regression coverage.

### Resolution 2026-05-13

**Recon body rewritten** at
`reconstructed/src/device/device_type.cpp:917` to match the
verified binary algorithm:

- jump-table[0] semantics: empty input returns
  `DeviceFamily(0)`, not `0x800`.
- Length-keyed literal fastpaths replicated at source level:
  `n==4 && s=="none"`, `n==6 && s=="SHFACC"`,
  `n==7 && s=="DEFAULT"|"SHFPPC2"|"SHFPPC4"`.
- Map keys changed from lowercase to **uppercase** (`HF2`,
  `UHF`, `MF`, `HDAWG`, `SHF`, `PQSC`, `HWMOCK`, `GHF`, `QHUB`,
  `VHF`).
- `SHFACC` / `SHFPPC{2,4}` now correctly map to
  `DeviceFamily::SHFACC` (0x80), not `DeviceFamily::SHF` (0x10).
- The `\binarynote` on the function brief in
  `reconstructed/include/zhinst/device/device_type.hpp:973`
  documents the case-sensitivity contract.

**Block-header summary comment** in `device_type.cpp:886`
rewritten from scratch to cite the .rodata jump-table address
(`0x961d88`), the actual map size (10 entries), the actual key
casing, and the SHFACC sentinel.  The previous summary was
written before the binary had been disassembled and was wrong
on every material point.

**Regression coverage**: 69-input batch added to
`tests/diff_unreachable/harness.py` under a new `pod_u32_cref`
ABI shape (uint32_t scalar return + single `const string&`
argument, returned in `%eax`).  Inputs cover empty, every
length 1..7, lower/uppercase variants of every map prefix,
every literal fastpath, and a handful of `len > 7` inputs
that bypass the jump table.  Result: 69/69 PASS, full harness
722/722.

**Verification**: `python tests/diff_test_fast.py` →
1603/1603 (no regressions).  `cmake --build . --target docs`
→ no new warnings.

**Why the main suite was clean both before and after** (still
worth understanding for future similar bugs): the original
binary builds the device-type pipeline through
`makeDeviceType(devType)` which calls `toDeviceFamily(devType)`
internally; the recon's lowercase-prefix table happened to
coincide with the binary's uppercase-prefix table on the
*specific input string each test passes* — except no test
passes a string that crosses the case boundary in a way that
changes the produced ELF.  Before the fix, every recon call
returned `0x800` for the inputs the test suite uses; after the
fix, every recon call returns the same value the binary
returns.  Both states pass the byte-for-byte ELF compare
because the downstream consumer (`makeDeviceType`) only uses
the family value where the binary's same lookup also produces
0x800.  A future regression that adds a code path which
**reads** the family value will now get the correct one.

## IF-268  `browseTo` recon body diverges from binary on three points

**Severity**: medium (correctness — wrong return type, wrong loop
exit, narrower file-type check)
**Status**: **fixed**
**Source**: `reconstructed/src/core/diagnostics_text.cpp:455`,
            `reconstructed/include/zhinst/core/diagnostics_text.hpp:147`

### Observation

Static disasm pass at `0x2eb950` (1739 B) revealed three
divergences between the recon body and the binary:

1. **Return type**: recon was `void browseTo(std::string)`; binary
   returns `bool` via `setns %bl; mov eax,ebx; ret` — the value
   is `(system_result >= 0)`.  Notes file already documented this
   correctly (`bool zhinst::browseTo(std::string path)` at
   `notes/diagnostics_text.md:1465`); only the source files were
   stale.
2. **Empty-parent handling**: recon's outer parent-walk loop did
   `if (parent.empty()) return;` — exiting the function entirely.
   Binary's `find_parent_path_size == 0` check at `0x2eb9f6`
   jumps to `0x2ebbd7`, which is the post-loop second-status
   probe, not the function exit.  Effect: recon would skip the
   shellout for any path whose initial `parent_path()` is empty,
   while the binary would still attempt to `xdg-open` the
   original path in that case.  Fixed by changing `return` to
   `break`.
3. **File-type predicate**: recon stripped to parent only when
   `st.type() == regular_file` (2).  Binary at `0x2ebd2d`
   `cmp r14d,0x3; je 0x2ebe6f` strips to parent for **any
   non-directory type** (regular file, symlink, block/char/fifo/
   socket, type_unknown — i.e. type ≠ 3).  Effect: recon would
   open a symlink or other special file directly, while the
   binary opens its containing directory.  Fixed by changing
   `== regular_file` to `!= directory_file`.

A fourth gap (also fixed): the binary returns `false` early when
the surviving path's status type is ≤ `file_not_found` (the
`cmp r15d,0x1; jbe 0x2ebc50` at `0x2ebc2f`).  Recon was missing
this guard entirely.

### Action

Source body and header brief rewritten to match the binary.
Header brief replaces the inaccurate "Linux branch only" framing
with a description of the actual behaviour (parent-walk
normalisation, non-directory-to-parent stripping, unconditional
`xdg-open` shellout) and documents the `bool` return contract.
The `\binarynote` about the missing quote-escaping is preserved.
`\verifyme` removed from the brief: the body has been verified
against the binary by static disasm reading and the recon now
matches.  The function still has no harness coverage (the
shellout is not amenable to differential testing without
intercepting `system()`), but lack of harness coverage is not
sufficient justification for `\verifyme` once the static
verification is complete.

### Verification

- Disasm of `0x2eb950..0x2ec01f` cross-checked against recon
  body line-by-line.
- 1603/1603 main difftest suite (no regressions; nothing in the
  test suite reaches `browseTo`).
- 739/739 diff_unreachable harness (browseTo not covered;
  unchanged).
- No new doxygen warnings.

### Lesson

A `\verifyme` marker can hide multiple distinct bugs.  When
clearing the last few markers in a phase wrap-up, **always**
re-verify the body against disasm rather than trusting the
existing recon body or the absence of test failures (the test
suite never reaches `browseTo`, so static disasm reading is the
only available verification path).

## IF-269  `Node::type2str` recon emits `"unknnown"` (3 n's); binary emits `"unknown"` (2 n's)

**Source**: F1 `\binarynote` audit (Phase F, 2026-05-13)
**Severity**: likely-bug (cosmetic — only triggered for unrecognised
`NodeType` values, which the test suite never reaches)
**Status**: **fixed 2026-05-13** (commit pending)

### Symptom

`reconstructed/src/ast/node.cpp:157` returned `"unknnown"` (8 chars,
three n's) from the default branch of `Node::type2str(NodeType)`,
with the comment `// sic — typo matches binary`.  The header brief
in `reconstructed/include/zhinst/ast/node.hpp:274..278` carried a
matching `\binarynote` claiming the literal `"unknnown"` was
"preserved from the binary."

Static-disasm verification at `0x269b71` (default branch of the
type2str jump-table dispatch) shows the binary emits `"unknown"`
(7 chars, two n's), not `"unknnown"`.  Both the recon body and
the `\binarynote` were inventions.

### Verification

Default-branch byte sequence at `0x269b71`:

```
269b71: c6 00 0e            movb $0x0e, (%rax)        ; size byte = 7<<1
269b74: c7 41 03 6e 6f 77 6e movl $0x6e776f6e, 3(%rcx) ; "n o w n" at +3..+6
269b7b: c7 01 75 6e 6b 6e   movl $0x6e6b6e75, (%rcx)  ; "u n k n" at +0..+3
269b81: b9 08 00 00 00      mov  $0x8, %ecx
269b86: c6 04 08 00         movb $0x0, (%rax,%rcx,1)  ; null at +8
```

The two `movl` writes overlap at offset +3 (both put `0x6e` = `n`),
so the SSO buffer ends up holding bytes `u n k n o w n` at offsets
0..6 — i.e. the 7-character string `"unknown"`, with size byte
`0x0e/2 = 7`.  The candidate `"unknnown"` would require an
8-character string with size byte `0x10`, and a non-overlapping
write pattern that does not appear in the binary.

`strings _seqc_compiler.so | grep unknnown` returns nothing,
further confirming no triple-n literal exists anywhere in the
binary.

### Fix

- `reconstructed/src/ast/node.cpp:157`: `"unknnown"` → `"unknown"`;
  comment updated.
- `reconstructed/include/zhinst/ast/node.hpp:274..278`:
  `\brief`/`\return` lines updated to say `"unknown"`;
  `\binarynote` removed (the now-correct return is unsurprising
  and does not need a flag).

### Test impact

None expected: no test in the 1603-case main suite or the 739-case
harness invokes `type2str` with an unrecognised `NodeType`.  Full
suite re-run after fix.

### Lesson

A `\binarynote` claiming "preserved from binary" is the strongest
form of authority a doc comment can claim — and therefore the
most damaging when wrong.  F1's audit caught this at the cost of
~30 seconds of disasm reading per site; the fabricated typo
might otherwise have survived indefinitely (no test ever exercised
the default branch).  Reinforces the F1 hypothesis that older
`\binarynote` entries are the highest-yield drift targets.

---

## IF-270  `extractVersionTriple` recon throws on malformed input; binary uses non-throwing convert

**Source**: F2 step 2b-vii harness wiring (CalVer throwing-shape
coverage), 2026-05-13
**Severity**: likely-bug (semantic divergence; orig swallows
parse errors silently, recon raises `boost::bad_lexical_cast`)
**Status**: **fixed** 2026-05-13 (commit pending)

### Symptom

The new `sret_blob_cref_throws` harness shape exposed a
divergence between `_seqc_compiler.so` and the libc++ recon
build for `zhinst::extractVersionTriple(std::string const&)`
(orig @ `0x101570`).

For non-numeric / partially-empty inputs the original returns a
zero-filled `std::array<size_t, 3>`, while the recon throws.
Specific failures from the harness:

| Input        | orig                 | recon                       |
|--------------|----------------------|-----------------------------|
| `""`         | `{0,0,0}`            | throws bad_lexical_cast     |
| `"abc"`      | `{0,0,0}`            | throws bad_lexical_cast     |
| `"1.2.x"`    | `{0,0,0}`            | throws bad_lexical_cast     |
| `"1..2"`     | `{0,0,0}`            | `{1, 2, 0}` (compress_on)   |
| `".1.2"`     | `{0,0,0}`            | throws bad_lexical_cast     |

The `{1,2,0}` case for `"1..2"` is especially telling: with
`token_compress_on` the recon parses both numeric parts cleanly
(no throw) and yet the orig still returns all zeros — so the
divergence is not just "swallows the throw", there is also a
different result-on-failure path.

### Hypothesis (verifyme)

Disasm of the orig at `0x101570..0x101bef` shows direct calls
to `boost::detail::lcast_ret_unsigned<...>::convert` (orig
`0x100c40`), which is the `bool`-returning convert path used by
`boost::conversion::try_lexical_convert`, **not** the throwing
`boost::lexical_cast<size_t>`.  Each call site does a
`test %al,%al` immediately afterwards, consistent with branching
on the bool result.  The result array is initialised to all
zeros up-front and slot writes appear to be guarded by the
convert success bool, so a failed parse leaves the slot at 0
without throwing.

The `"1..2"` → `{0,0,0}` case suggests the orig **does not**
use `token_compress_on` and may have an additional outer guard
that zeroes the entire array on any sub-parse failure — needs
GDB verification to characterise the failure-handling exactly.

### Recon (current)

```cpp
// reconstructed/src/infra/calver.cpp:33..53
std::array<size_t, 3> extractVersionTriple(std::string const& s) {
    std::vector<std::string> parts;
    boost::algorithm::split(parts, s, boost::is_any_of("."),
                            boost::token_compress_on);   // wrong: orig likely token_compress_off
    std::array<size_t, 3> result = {{0, 0, 0}};
    size_t n = parts.size();
    if (n == 0) return result;
    if (n >= 1) result[0] = boost::lexical_cast<size_t>(parts[0]);  // wrong: should be try_lexical_convert
    if (n >= 2) result[1] = boost::lexical_cast<size_t>(parts[1]);
    if (n >= 3) result[2] = boost::lexical_cast<size_t>(parts[2]);
    return result;
}
```

### Fix sketch (not yet applied)

Replace `boost::lexical_cast<size_t>(part)` with
`boost::conversion::try_lexical_convert<size_t>(part, slot)` and
investigate (via GDB) whether a single failure should also zero
the *prior* slots — the `"1..2"` case suggests an outer
"zero everything on any sub-failure" guard that recon currently
lacks.  Compress-mode also needs verifying.

### Harness impact

The `sret_blob_cref_throws` SYMBOLS entry for
`extractVersionTriple` is in place but expected to FAIL on 5 of
13 inputs until this is fixed.  Per workflow we land the harness
wiring with the FAILs surfacing the bug, then file this IF as a
TODO for separate investigation and fix.

### Knock-on

`fromDecimal(string const&)` (orig @ `0x100520`) shares the
recon convention (`std::stoul`) and was audited at fix time:
the orig wraps `std::stoul` in a `try/catch` and *re-throws*
the failure as a `zhinst::Exception` (call to
`boost::throw_exception<zhinst::Exception>` at `0x100676`,
following `__cxa_begin_catch` at `0x10060c`).  This is a
different pattern from `extractVersionTriple` (translate, not
swallow); the harness collapses both-threw outcomes via the
sentinel pair so the divergence is invisible to differential
testing.  Recon currently lets the `std::stoul` exception
propagate as `std::invalid_argument` / `std::out_of_range`,
which is observably different at the C++ type level but not at
the harness shape level.  Logged here for completeness; not
promoted to its own IF unless a future shape exercises the
exception type.

### Resolution (2026-05-13, GDB-verified)

Two bugs in the recon, both confirmed by GDB-tracing the orig
on input `"1..2"`:

1. **Wrong token-compress mode.**  The 4th argument to
   `boost::algorithm::split` is `mov $0x1, %ecx` at `0x1015df`.
   The Boost enum is **`token_compress_on = 0,
   token_compress_off = 1`** (per
   `boost/algorithm/string/constants.hpp`), so the orig uses
   `token_compress_off`, not the recon's `token_compress_on`.
   GDB confirmed: at the size-dispatch site `0x10161b` for
   input `"1..2"`, `%rcx == 3` (orig sees three tokens
   `["1", "", "2"]`), not the two tokens `["1", "2"]` that
   `compress_on` would produce.

2. **Missing outer try/catch.**  The orig wraps the entire
   parse loop in `try { ... } catch (...) { result = {0,0,0}; }`
   — verified via `__cxa_begin_catch` at `0x101bb7` followed by
   three `xorps`/`movups`/`movq $0` writes that zero the
   sret blob before `__cxa_end_catch` at `0x101bcd` and the
   jump to the epilogue at `0x1019d4`.  GDB trace on `"1..2"`
   shows the path `entry → after_split → dispatch (rcx=3) →
   throw_b09 (parts[1]="" empty-string check) → begin_catch →
   epilogue` returning `{0,0,0}`.

Recon fix at `reconstructed/src/infra/calver.cpp:33..56`:
switched the `split` call to `boost::token_compress_off` and
wrapped the three `lexical_cast` lines in `try { ... } catch
(...) { return {{0,0,0}}; }`.  Note: the recon does **not**
need to switch to `try_lexical_convert` — the orig also calls
the throwing `lcast_ret_unsigned::convert` (which routes to
`boost::throw_exception<bad_lexical_cast>` at `0x1010a0`),
proving that the binary swallows the throw via the outer
`catch`, not via a non-throwing convert.  The earlier hypothesis
in this entry was wrong on that point.

Header doc at `reconstructed/include/zhinst/infra/calver.hpp:240..253`
updated: removed the spurious `\throws bad_lexical_cast` and
the "adjacent dots compressed" claim, added a `\binarynote`
documenting the catch-and-zero behaviour with the verified
input list (`""`, `"abc"`, `"1..2"`, `".1.2"`, `"1.2.x"`).

Harness corpus override (`THROW_CORPUS_OVERRIDE` for
`extractVersionTriple` in `tests/diff_unreachable/harness.py`)
removed; the symbol now PASSes 13/13 cases (was restricted to
8/8 non-divergent cases pre-fix).  Net harness count
952 → 957.  Main test suite 1603/1603 unchanged.



## IF-271  Post-F3 audit of `\unverifiable` + `\unclear` backlog — all 6 entries verified accurate

**Status**: confirmed-no-action 2026-05-13

**Discovered**: post-F3 backlog audit (Phase F follow-up).

**Severity**: cosmetic / housekeeping.

**Sites audited:**

| File | Line | Tag | Subject |
|------|-----:|-----|---------|
| `reconstructed/include/zhinst/io/elf_reader.hpp` | 228 | `\unverifiable` | `ElfReader::ddSectionIndex_` |
| `reconstructed/include/zhinst/infra/logging.hpp` | 69 | `\unverifiable` | `zhinst::logging::Severity` enum integer mapping |
| `reconstructed/src/asm/asm_expression.cpp` | 205 | `\unverifiable` | `zhinst::str(AsmExpression)` body |
| `reconstructed/include/zhinst/asm/asm_expression.hpp` | 243 | `\unverifiable` | `zhinst::str(AsmExpression)` declaration |
| `reconstructed/src/codegen/prefetch_placesingle.cpp` | 1109 | `\unverifiable` | `Table` (0x200) sub-paths in `placeSingleCommand` |
| `reconstructed/include/zhinst/codegen/prefetch.hpp` | 1138 | `\unverifiable` | `Table` arm doc in dispatch overview |
| `reconstructed/include/zhinst/runtime/resources.hpp` | 1117 | `\unclear` | `StaticResources::errorReportTarget()` |

**Verification:**

- `ddSectionIndex_`: rationale was already corrected by IF-256
  (closed); no `compile_seqc` input drives `getWaveform()` to
  observe a non-zero selector.  Tag remains accurate.
- `Severity` enum: confirmed only the `LogRecord(Severity)` ctor
  and `severity_logger_mt<Severity>` template instantiations are
  exported; the integer-to-name mapping itself is not fixed by any
  exported symbol.  Tag remains accurate.
- `str(AsmExpression)` at `0x28cd20`: confirmed via objdump that
  the only call to this symbol is the recursive self-call at
  `0x28d017` (inside the function's own range
  `0x28cd20`–`0x28d280`).  No external caller exists in the binary;
  no `compile_seqc` input drives a difftest.  Tag remains accurate
  in both `.cpp` and `.hpp`.
- `Table` arm: NodeType::Table is structurally unreachable from the
  SeqC front-end per IF-249 (closed).  Reachability is a front-end
  design constraint, not a momentary gap.  Tag remains accurate at
  both sites.
- `errorReportTarget()`: confirmed via `objdump -t` that no symbol
  matching `errorReportTarget` / `warningReport` / `reportTarget`
  exists in `_seqc_compiler.so`.  The reconstructed inline comment
  at `resources.hpp:1110-1114` notes that `StaticResources::getVariable`
  inlines the equivalent dispatch at `0x12a256-0x12a26d` (load
  `functionPtr_` at `+0x100`, call `*0x30(%rax)` — libc++
  `__function::__base::__invoke` vtable slot).  Verified the
  disassembly matches: `mov 0x100(%rax),%rdi; test %rdi,%rdi;
  je ...; mov (%rdi),%rax; call *0x30(%rax)`.  The hypothesised
  helper has no discrete symbol — the binary inlines it everywhere.
  `\unclear` (rather than `\unverifiable`) is the correct tag:
  the function may never have existed as a separate definition.

**Outcome**: No source changes.  All 6 sites are appropriately
tagged, accurately phrased, and continue to flag genuine
verification gaps that future evidence (e.g. a non-SeqC entry
point or a debug build of LabOne) could close.

**Action items**: None.  Audit closure recorded so a future
session does not re-audit the same backlog.

---

## IF-272  `ZiFolder::folderPath` recon diverged from the binary on three independent points

**Status**: fixed 2026-05-15 (commit pending; this entry).

**Discovered**: Phase F follow-up.  Built a `diff_unreachable`
harness shape `sret_zifolder_2cref` for
`ZiFolder::folderPath(string const&, string const&) const`
(`@0x2ce2f0`) using the existing 24-byte placement-construct slot
mechanism (a `ZiFolder` is layout-equivalent to a single
`std::string` at offset 0).  All 17 corpus inputs failed before
the fix; all 17 pass after.

**Severity**: likely-bug (silently wrong output for any caller).

**Three divergences against the binary at `0x2ce2f0`–`0x2ce5a9`:**

1. **Vendor-segment branch inverted.**  The recon body
   unconditionally appended `"$Zurich Instruments"` between
   `subdir` and `"LabOne"`.  The binary actually inserts the
   vendor segment **only when `subdir` is NOT one of the two
   canonical host-side roots** `"/data"` (size 5) or
   `"/settings"` (size 9):
   - `2ce374: cmp $0x5,%rcx` → `jne 2ce440` (size!=5 → APPEND)
   - `2ce39b: jne 2ce440` (XOR vs `/data` mismatch → APPEND)
   - `2ce3a1: jmp 2ce480` (`/data` match → SKIP)
   - `2ce43e: je  2ce480` (XOR vs `/settings` zero → SKIP)
   The two block-header summary comments in `zi_folder.cpp:42-54`
   self-flagged the confusion; the body picked the wrong branch
   anyway.

2. **Vendor-segment literal had a stray `$` prefix.**  The recon
   wrote `"$Zurich Instruments"` (19 chars, leading `$`).  The
   binary writes `"Zurich Instruments"` (18 chars, no prefix).
   The `'$'` character in the recon was a misreading of the
   libc++ short-string SIZE byte: `2ce440: movb $0x24,-0x40(%rbp)`
   sets the SSO header byte to `0x24 = 36 = (18 << 1)` — the
   encoded length, not a data character.  rodata at `0x90b4eb`
   confirms the literal is 18 bytes `"Zurich Instruments"`.

3. **`basePath_` had a spurious empty-string guard.**  The recon
   wrote `if (!basePath_.empty()) result /= basePath_`.  The
   binary unconditionally appends `basePath_` (no guard at
   `2ce4bf`–`2ce507`) — only `extra` has the non-empty guard at
   `2ce539`.  This produces the same output for typical inputs
   but diverges when `basePath_` is empty (e.g. the
   `"empty basePath /data"` corpus row): binary emits
   `"/data/LabOne"` (note the `LabOne` segment after the empty
   slot), recon emits `"/data/$Zurich Instruments/LabOne"`
   (compounded with bug 1).  The harness exercises this with the
   `(b"", b"/data", b"")` input.

**Fix:** rewrote the body in
`reconstructed/src/io/zi_folder.cpp:62-83` to:

```cpp
fs::path result(subdir);
const bool isCanonicalRoot =
    (subdir.size() == 5 && subdir == "/data") ||
    (subdir.size() == 9 && subdir == "/settings");
if (!isCanonicalRoot) {
    result /= "Zurich Instruments";
}
result /= "LabOne";
result /= basePath_;
if (!extra.empty()) {
    result /= extra;
}
return result.string();
```

Also corrected the inverted `\brief` and supporting comment block
in `reconstructed/include/zhinst/io/zi_folder.hpp:35-40` and
`:68-89` (these had the vendor-segment condition stated
backwards: "inserted only when `/data` or `/settings`" → "omitted
only when `/data` or `/settings`").  The signature and other
declarations were unchanged.

**Verification:**
- `tests/diff_unreachable/harness.py --filter folderPath`:
  17/17 PASS (was 17/17 FAIL pre-fix).
- Full `tests/diff_unreachable/harness.py`: 1283/1283 PASS
  (was 1266/1266 pre-change; +17 new from this symbol).
- Full `tests/diff_test_fast.py`: 1603/1603 PASS (no regression).
  No `compile_seqc` test reaches `ZiFolder::folderPath` in a
  byte-observable way — the `ziFolder()` factory feeds it but
  none of the SeqC test inputs cause the result to be embedded
  in an emitted ELF, which is why the bug went undetected before
  the harness was built.

**Action items**: None.  Bug fixed; harness regression-locks the
behaviour going forward.

## IF-273  `makeDirectories` recon adds a spurious outer try/catch and drops the 0x8011 error code

**Status**: open (likely-bug, low-impact).

**Discovered**: 2026-05-15, Phase F follow-up audit of
`reconstructed/src/io/zi_environment.cpp`.

**Severity**: likely-bug.  Functionally observable only when the
caller catches the resulting `Exception` and inspects either
`code()` or the message text.

**Two divergences against the binary at `0x2cdef0`–`0x2cdfb5`:**

1. **Wrong `Exception` constructor.**  The recon throws
   `Exception(oss.str())` — single-arg string ctor, default
   sentinel error code `0x8000`.  The binary calls
   `Exception(ErrorCode const&, string)` with the explicit code
   `0x8011` (verified at `2cdf80: mov esi,0x8011` immediately
   before `call 2e5700` = the 2-arg ctor at `0x2e5700`).  The
   value `0x8011 = 32785` corresponds to the
   `ZIResult_enum::ApiBufferTooSmall` slot in
   `reconstructed/include/zhinst/core/error_messages.hpp:374`;
   the semantic mismatch (a buffer-size error code attached to a
   filesystem permissions failure) is a binary quirk worth a
   `\binarynote`, not a recon error to "fix" by renaming.

2. **Spurious outer `try { ... } catch (std::exception const&)`
   block** that translates the message
   `"Could not access directory '<dir>'."`
   into
   `"Could not create directory '<dir>'." + inner.what()`.
   The binary has **no `__cxa_begin_catch` / `__cxa_end_catch`**
   in this function — the prologue/epilogue pattern (push rbp /
   push r14 / push rbx / sub rsp,0x1b0 ... pop rbx / pop r14 /
   pop rbp / ret) and the linear instruction stream from entry
   to the throw site at `0x2cdfb5` show only the cleanup landing
   pad for the throw itself (at `0x2cdfba+`), not a catch
   handler.  The `"Could not create directory '"` literal does
   exist in the binary's `.rodata` but is referenced by a
   different function (not yet identified — likely a separate
   higher-level call site), not by `makeDirectories` itself.

**Fix plan:**
- Drop the outer try/catch from
  `reconstructed/src/io/zi_environment.cpp:251-270`.
- Switch the `BOOST_THROW_EXCEPTION(Exception(oss.str()))` call
  to `Exception(ErrorCode(...0x8011...), oss.str())`.  The
  two-arg ctor signature is at `exception.hpp:288`; the
  ErrorCode form needs to follow whatever pattern the rest of
  the recon already uses for explicit `ZIResult_enum` codes.
- Add a `\binarynote` to the brief explaining that the 0x8011
  code is what the binary emits, even though the matching enum
  name is `ApiBufferTooSmall` (semantically inappropriate for a
  directory-access failure — a binary quirk, not a recon
  decision).

**Action items**: addressed in the same commit as this entry —
see fix in `reconstructed/src/io/zi_environment.cpp` and brief in
`reconstructed/include/zhinst/io/zi_environment.hpp`.

## IF-274  `hasMediaDevNode` recon calls `boost::filesystem::status` once; binary calls it twice

**Status**: open (cosmetic, behavior-equivalent).

**Discovered**: 2026-05-15, same audit pass as IF-273.

**Severity**: cosmetic.  Both forms produce the same boolean
result for every input; the only observable difference is one
extra syscall in the binary.

**Evidence:** at `0x2eb6d6` and `0x2eb6f7` the binary calls the
same `boost::filesystem::detail::status` symbol (`0x36f280`)
twice on the same path / error_code pair.  The first call's
result type is checked (`cmp DWORD PTR [rbp-0x20],0x1; jbe →
return false`) as an early-out for `status_error` /
`file_not_found`; the second call repeats the syscall and its
result is then used both for the `error_code.value()` check
(`0x2eb70f: cmp DWORD PTR [rbp-0xd0],0x0`) and for the
`character_file` (= 5) test (`0x2eb718: cmp DWORD PTR
[rbp-0x20],0x5; sete bl`).

The recon at `reconstructed/src/io/zi_environment.cpp:215-229`
issues a single `status` call.  Result is identical (an existing
`/dev` child whose status changes between two back-to-back
syscalls is not a case any caller can rely on).  The likely
explanation is a missed common-subexpression elimination in the
original build, or a deliberate re-stat to defeat a cache layer
not visible in this snippet.

**Action items**: leave as-is (no behavioural impact).  Recorded
for completeness so a future agent doesn't waste a session
"finding" the same divergence.

## IF-275  zi_folder/zi_environment audit — 9 of 11 functions verified clean against the binary

**Status**: tracking entry; no action item.

**Discovered**: 2026-05-15, Phase F follow-up Task 2.

**Severity**: informational.

**Scope:** disasm-audit of every reconstructed function in
`reconstructed/src/io/zi_folder.cpp` and
`reconstructed/src/io/zi_environment.cpp` after the IF-272
folderPath bug raised the question "are there other latent
SSO-byte misreads in the same files?".

**Verified clean:**
- `ZiFolder::ZiFolder(string)` @0x2ce2c0 — trivial 24-byte move.
- `ZiFolder::ziFolder(DirectoryType)` @0x2cf0c0 (446 disasm
  lines) — MF branch returns `/data` or `/settings`; HOME-fallback
  uses `getenv("HOME")` then `getpwuid_r` reading `pw_dir` at
  `pw+0x18`; throw site at `0x2cf7a1`.
- `ZiFolder::sessionSaveDirectoryName` @0x2ce620 — ostringstream
  chain `"session_" + formatTime(now,true) + "_" + suffix +
  serial` with `suffix = "0"` iff `serial.size()==1`.  All
  rodata literals at `0x90b505` (`"0"`), `0x90b507`
  (`"session_"`), `0x90a347` (`"_"`) verified.
- anon-namespace `readManifestImpl` @0x2ec210 — short-circuits on
  `status.type() <= regular_file`.
- anon-namespace `laboneManifest` @0x2ec5e0 — Meyers singleton
  reading `/opt/zi/LabOne/manifest.json`.
- anon-namespace `doIsMf` @0x2ec700 — `[rbp-0x30]=0x0c` =
  `(6<<1)` for the 6-byte key `"device"`; `cmp WORD PTR
  [rcx],0x666d` = `"mf"` little-endian; `cmp rcx,0x2` for
  size==2.
- anon-namespace `isMf` @0x2ec1e0 — `try { doIsMf } catch(...) {
  return false; }`.
- anon-namespace `isMf64` @0x2ec430 — `[rbp-0x48]=0x10` =
  `(8<<1)` for `"platform"`; XOR vs `0x6d726f6674616c70` =
  `"linuxARM"` then XOR vs `0x3436` = `"64"`; size==10 check.
- `runningOnMfDevice` / `runningOnMf64Device` (both 0-arg cached
  forms and both string-arg fresh forms) — Meyers singletons
  guarded by `__cxa_guard`; cached forms share the
  `laboneManifest()` singleton.
- `markFileHidden` @0x2eb940, `initBoostFilesystemForUnicode`
  @0x2ec020 — Linux no-ops, exactly as recon documents.

**Divergences found:** filed as IF-273 (`makeDirectories`) and
IF-274 (`hasMediaDevNode`).  No further latent SSO-byte misreads
of the IF-272 shape were found.

**Conclusion:** the IF-272 misread was an isolated incident, not
a systemic pattern across the io/ subsystem.  The vendor-segment
literal `"Zurich Instruments"` (size byte `0x24`, length 18,
boundary on the printable-ASCII range) was unusually likely to
trigger the misread; other SSO literals in this code (`"device"`
size byte `0x0c`, `"platform"` size byte `0x10`, `"/dev"` size
byte `0x08`, `"0"` size byte `0x02`) all encode to non-printable
sub-`'!'` bytes and are visibly metadata, not data.

**Action items**: none.

## IF-276  Phase-D doc-tag backlog triage (post-IF-272 audit pass)

**Status**: tracking entry; no action item.

**Discovered**: 2026-05-15, Phase F follow-up Task 3.

**Severity**: informational.

**Scope:** verify-then-write triage of the documentation tag
backlog reported by `reconstructed/docs/coverage.sh`:
- `\unclear` : 1 entry
- `\unverifiable` : 5 entries
- `\binarynote` : 26 entries

The triage's purpose was to confirm each tag still reflects
reality (not stale) following the IF-272 SSO-byte misread, which
raised concerns about doc drift in general.

**Approach:**
- 100% audit of `\unclear` (1/1) and `\unverifiable` (5/5;
  4 sampled by-hand, all clearly justified with cross-references
  to IF-235, IF-249, etc.).
- 33% sample of `\binarynote` (7/26, distributed across
  `waveform/`, `runtime/`, `ast/`, `device/`, `io/`, `core/`,
  `infra/`).  Sample selection covered every subsystem with at
  least one entry and prioritised the larger files
  (`resources.hpp`, `device_type.hpp`, `seqc_ast_node.hpp`,
  `cached_parser.hpp`).  Each entry was verified against either
  the binary disassembly (when the note cited a binary address)
  or the recon source body (when the note described recon
  behavior).

**Result:** **all entries accurate.**
- `\unclear` (1/1 PASS): `runtime::resources.hpp:1117`
  `errorReportTarget()` — no matching binary symbol (verified
  via `nm`); IF-235 cross-reference still valid.
- `\unverifiable` (4/4 sampled PASS):
  `prefetch.hpp:1138` (Table arm, IF-249 ref),
  `elf_reader.hpp:228` (selector field),
  `asm_expression.hpp:243` (str()), `logging.hpp:69` (Severity).
  Each entry explains why the claim is unverifiable from SeqC
  inputs.
- `\binarynote` (7/7 sampled PASS): `play_config.hpp:46`
  (PlayConfig::now), `resources.hpp:658` (variableExistsInScope),
  `resources.hpp:705` (checkVar), `seqc_ast_node.hpp:262`
  (swap), `device_type.hpp:146` (DeviceOption None/MF dual 0),
  `cached_parser.hpp:191` (CachedFile no found-flag),
  `exception.hpp:319` (description() returns errorCode_ ptr),
  `calver.hpp:88` (triple() returns CalVer const&).  Several
  required objdump verification at the cited binary addresses
  (e.g. `swap` @0x1fda40 confirmed swapping only +0x8/+0x10,
  not the vptr at +0x00; `triple` @0x100260 confirmed
  `mov rax,rdi; ret`).

**Conclusion:** the existing tag inventory is healthy.  The
backlog represents genuine uncertainty / quirks worth flagging,
not bookkeeping debt.  No tag-removal commits planned; the
backlog can grow organically as new findings accrue.

**Action items**: none.  The 19 unaudited `\binarynote` entries
remain on file; if a future audit pass wants to extend coverage,
the file list is at the top of `reconstructed/docs/coverage.sh`
output.


## IF-277  `double2awg` / `double2awg1m` mishandled NaN (std::max vs SSE2 `maxsd`)

**Status**: fixed (recon)
**Severity**: cosmetic-bug — the NaN path is unreachable from SeqC user code (sample values come from `wave w = ones(...)`-style generators that produce in-range doubles), so no main-suite test was impacted.  Detected only by the diff_unreachable harness extension to `util::wave::double2*` (Task 4 follow-up).

**Where**: `reconstructed/src/waveform/util_wave.cpp`
- `double2awg`   @ binary 0x299630
- `double2awg1m` @ binary 0x299680
(`double2awg16` @ 0x299700 was already correct — it uses `_mm_max_sd` for exactly this reason.)

**Symptom**: harness comparison of `double2awg(NaN, marker)` yielded:
- original (binary): `marker & 0x3` (e.g. 0, 1, 2, 3)
- recon:             `32768 + marker` (e.g. 32772, 32773, 32774, 32775)

**Root cause**: recon used `std::max(-1.0, sample) * kFullScale`.  When `sample` is NaN, `std::max` returns the first argument (-1.0), producing `-8191`, then `lround = -8191`, then `(-8191 << 2) | marker = -32764` reinterpreted as `uint16_t = 32772`.  The binary instead emits a single `maxsd xmm0(=-1.0), xmm1(=sample)` (objdump confirmed at 0x299658 and 0x2996a8) whose semantics are "NaN propagates from second source".  Hence binary computes `lround(NaN) = 0` (glibc x86_64 implementation-defined behaviour) and returns `0 + (marker & 3) = marker`.

**Fix**: replaced `std::max(-1.0, sample) * kFullScale` with the same `_mm_max_sd(_mm_set_sd(-1.0), _mm_set_sd(sample))` pattern already in use by `double2awg16`.  All three `double2*` now share identical NaN-propagation semantics, matching the binary.

**Verification**:
- `objdump -d _seqc_compiler.so` at both addresses shows the expected `maxsd xmm0,xmm1` instruction.
- diff_unreachable harness: 102/102 PASS for both `double2awg` and `double2awg1m` after the fix, including 6 NaN×marker combinations each.
- Full main suite: 1603/1603 unchanged (no NaN inputs reach this code path from user SeqC).

**Action items**: none — fix already landed.

## IF-278  `AWGCompilerImpl::nodeListToJson` was inlined into `writeToStream`; binary has it as a separate symbol with a redundant intermediate `set` copy

**Severity**: cosmetic (structural divergence, no observable byte difference)
**Status**: fixed (separate symbol now exists; intermediate-set copy intentionally omitted)
**Location**: `reconstructed/src/codegen/awg_compiler.cpp` (function now @ awg_compiler.cpp:~1015 region)
**Binary symbol**: `_ZNK6zhinst15AWGCompilerImpl14nodeListToJson...` @ `0x1088d0`, 796 B
**Caller**: `AWGCompilerImpl::writeToStream` @ `0x108cc0`, single call at `0x109258`

### What the binary does
`nodeListToJson(vector<NodeMapItem> const&, unordered_map<NodeMapItem, set<AccessMode>> const&) const` is a separate non-inlined member returning `boost::json::object` by sret.  Its body:
1. Initialises the sret slot to an empty `boost::json::object` (24-byte SSO form: `{0, 0x07, &boost::json::object::empty_}`).
2. Builds a local `boost::json::array items`.
3. For each `NodeMapItem` in the input vector:
   a. Calls `node.toJson()` (@`0x1c54f0`) → `boost::json::value`.
   b. Looks the node up via `unordered_map::find` (@`0x1153f0`).
   c. **If found**: constructs a *local* `std::set<AccessMode>` and inserts the iterator-pair from `it->second` into it (@`0x115600` `set::insert(begin,end)`), then iterates the local set to build a `boost::json::array modesArr` of inline `boost::json::string` values (constructed at `0x108a8c` from the `(string_view, storage_ptr)` overload, with text drawn from the static SSO table at `0x9573c0` — entries `"soft"`, `"direct"`, `"custom"`).  Inserts `entry["modes"] = modesArr`.
   d. Appends the entry to `items` via `array::emplace_back<object>(...)`.
4. Stores `items` into `root["nodes"]` and returns.

### What the recon used to do
The body was inlined into `writeToStream` directly (awg_compiler.cpp:1182–1198 in commit `2bfaeec`), with no separate `nodeListToJson` member.  This produced byte-identical `.nodes_json` output, but:
- The exported recon `.so` was missing the `nodeListToJson` symbol entirely.
- The recon iterated `it->second` directly without the redundant local-set copy.

### What the recon now does
- Promoted the body into a real public member `AWGCompilerImpl::nodeListToJson(...)` matching the binary signature; `writeToStream` now calls it and serializes the result.
- The intermediate `std::set<AccessMode>` copy step is **intentionally omitted**: the source value is *already* a sorted `std::set<AccessMode>`, so a copy-then-iterate is observationally identical to iterating the source directly.  The emitted JSON byte sequence is unchanged (verified by 1603/1603 difftest pass before and after).
- The recon uses `boost::json::string(toString(m))` rather than the binary's `boost::json::string(string_view, storage_ptr)` constructor with a static-table view.  Output bytes are identical because `toString(AccessMode)` returns the same `"soft"` / `"direct"` / `"custom"` strings.

### Evidence
- `objdump -d --start-address=0x1088d0 --stop-address=0x108be0 _seqc_compiler.so`: full 200-line disassembly confirms the body sketch above.
- `objdump -s --start-address=0x9573c0 _seqc_compiler.so`: confirms the AccessMode SSO string table.
- `objdump -d _seqc_compiler.so | grep "call.*1088d0"`: single caller, `writeToStream` @ `0x109258`.
- `python tests/diff_test_fast.py`: 1603/1603 before and after extraction — `.nodes_json` bytes identical.

**Action items**: none — extraction landed; intermediate-set copy intentionally omitted; remaining minor divergence (string-construction route) is functionally invisible.

## IF-279  `Random::seedRandom` had no recon symbol; old C-shim read 4 bytes from /dev/urandom where binary reads 8

**Severity**: low (functionally indistinguishable from a user perspective; promoted to mangled symbol)
**Status**: fixed
**Location**: `reconstructed/src/infra/prng_libcxx.cpp` (definition, libc++ shim TU); `reconstructed/include/zhinst/infra/random.hpp` (declaration); `reconstructed/src/runtime/custom_functions_playback.cpp` (caller updated)
**Binary symbol**: `_ZN6zhinst6Random10seedRandomEv` @ `0x16be80`, 297 B
**Caller**: `CustomFunctions::randomSeed` @ `0x1497c0`, single call at `0x149832`

### What the binary does
1. Construct an inline `std::string` `"/dev/urandom"` on the stack (24-byte SSO form, size byte 0x18 = `(12<<1)`).
2. Call `std::random_device(string const&)` → opens `/dev/urandom` (file descriptor stored in the random_device).
3. Call `std::uniform_int_distribution<uint64_t>::operator()<random_device>(rd, default_param_type)`.  Default `param_type` is `[0, UINT64_MAX]`.  libc++'s implementation calls `rd()` (which returns `unsigned int`, 4 bytes) twice and combines as `lo | (uint64_t)hi << 32` to fill the 64-bit range.  Net effect: **8 bytes consumed from `/dev/urandom`** for the seed.
4. Store the 64-bit seed at `state[0]`.
5. Run libc++'s mt19937_64 seed-expansion to fill `state[1..312]` (multiplier `0x5851F42D4C957F2D`).
6. Reset `state[312] = 0` (read-index slot).

### What the recon used to do (pre-fix, before IF-279)
The body lived as an `extern "C"` helper `seqc_libcxx_mt19937_seed_urandom(uint64_t* state)` in `prng_libcxx.cpp`.  It:
- Opened `/dev/urandom` via `fopen("/dev/urandom","rb")`.
- Read **4 bytes** into an `unsigned int seed`.
- Ran `std::mt19937_64(seed)` and `memcpy`'d the resulting state.

Two divergences from the binary:
- **No mangled `Random::seedRandom` symbol** was exported.  The recon `.so` was missing the symbol entirely.
- The seed was a 32-bit value zero-extended to 64 bits; the binary uses a full 64-bit value drawn as two 4-byte reads.  The resulting mt19937_64 state bits differ.

The seed difference is invisible to the difftest suite because no test combines `randomSeed()` with subsequent `randomGauss()` / `randomUniform()` calls — `grep -l "randomSeed" tests/cases/` returns five files, all of which call only `randomSeed()` standalone.

### What the recon now does
- New `class Random` declared in `reconstructed/include/zhinst/infra/random.hpp` — a thin typed view onto a 313 × `uint64_t` state array.
- Definition of `Random::seedRandom()` lives in `prng_libcxx.cpp` (the existing libc++ shim TU), built with `clang++ -stdlib=libc++` so the underlying `std::mt19937_64` seed-expansion implementation matches the binary byte-for-byte.
- The `/dev/urandom` read is implemented manually as **two consecutive 4-byte fread calls combined as `lo | (uint64_t)hi << 32`** rather than via `std::random_device + std::uniform_int_distribution`.  Reasons:
  - libc++'s `std::random_device::operator()` is a runtime-resolved symbol (`_ZNSt3__113random_deviceclEv`) provided only by `libc++.so`.  The host `_seqc_compiler.so` is a libstdc++ binary; it has no libc++.so runtime dependency, so calling that symbol would fail at load time (verified by `ImportError: undefined symbol: _ZNSt3__113random_deviceclEv` after a first-attempt fix).
  - The two-4-byte-fread sequence is exactly what libc++'s `random_device("/dev/urandom") + uniform_int_distribution<uint64_t>(default)` would emit observably: 8 bytes consumed in a known order, combined into a uint64 with the low 32 bits in `lo`.
- `randomSeed()` user fn in `custom_functions_playback.cpp` now invokes the method via `reinterpret_cast<Random*>(GlobalResources::random)->seedRandom()`, mirroring the binary's call shape at `0x149832`.
- The legacy `seqc_libcxx_mt19937_seed_urandom` C-shim was removed; the seed-default helper (`seqc_libcxx_mt19937_seed_default`) remains for the deterministic-init path used by `GlobalResources` ctor (separate concern).

### Evidence
- `objdump -d --start-address=0x16be80 --stop-address=0x16bfb0 _seqc_compiler.so`: full disassembly of the 297-byte body confirms the 6-step structure above.
- libc++ source (`<random>` header): `__independent_bits_engine` / `uniform_int_distribution<uint64_t>::__call` calls the URNG `(64 + 31) / 32 = 2` times and combines `result = lo | (uint64_t)hi << bits` where `bits = 32`.
- `nm reconstructed/build/_seqc_compiler.so | grep 6Random`: confirms `_ZN6zhinst6Random10seedRandomEv` is now defined in the recon `.so` (local visibility, intra-`.so` linkage).
- `python tests/diff_test_fast.py`: 1603/1603 PASS after the change (was 1598/1603 in the intermediate state where libc++ `random_device` was used directly and failed at load time with `ImportError`).
- `python tests/diff_unreachable/harness.py`: 1626/1626 PASS, no regressions.

**Action items**: none — fix landed.  No harness coverage added because `seedRandom`'s observable output is non-deterministic (reads `/dev/urandom` per call); a deterministic-mode harness would require fd-injection or `LD_PRELOAD` machinery that is currently unjustified by any test scenario.


## IF-280  `AwgPathPatterns(AwgPathPatterns const&)` — binary symbol is libc++-string copy ctor; recon (libstdc++) inlines `= default` at all callsites

**Status**: documented (close-by-design); cluster `awg_config::device` retired.

**Severity**: cosmetic — observable behavior is identical; no test regression possible.

### Symptom

D14 inventory (`reconstructed/notes/d14_inventory.md:743`) flags
`_ZN6zhinst15AwgPathPatternsC2ERKS0_` @0x2cc4f0 (254 B) as **absent**
in the recon.  `nm reconstructed/build/_seqc_compiler.so | grep
AwgPathPatternsC` confirms only the **3-arg ctor** is emitted
out-of-line; the copy ctor symbol does not exist in the recon.

### Root cause

The header (`reconstructed/include/zhinst/device/awg_device_props.hpp:122`)
declares the copy ctor as `= default`, which is the correct C++
semantics — member-wise copy of three `std::string`s.  Whether the
compiler emits an out-of-line symbol for a defaulted ctor is purely
an inlining decision driven by `-O2` heuristics and call-site
counts.

The binary's body (verified by `objdump -d --start-address=0x2cc4f0`)
is the textbook libc++ 3-string copy sequence:

- For each of the 3 strings: `testb $0x1, (%rsi)` SSO-flag check
  on byte 0; if SSO-short, `movups`/`mov` 24-byte inline copy; if
  SSO-long, call `__init_copy_ctor_external`.
- Plus exception-cleanup unwind path that reverse-destroys
  partially-constructed members.

This is exactly what `= default` produces under libc++.  The
recon, built against **libstdc++**, would need an entirely
different body (libstdc++ string is a single pointer to a
heap-allocated control block — no SSO bit-0 flag at offset 0,
different copy dispatch).

### Why no fix

Two options were considered:

1. **Mirror via libc++ shim TU** (analogous to
   `Random::seedRandom` in `prng_libcxx.cpp`).  Rejected: the
   symbol would operate on libc++-shaped strings, but every
   caller in the recon (`getAwgDeviceProps<*>` instantiations,
   `_GLOBAL__sub_I_properties.cpp`) is compiled against
   libstdc++.  The libc++ symbol would have **no caller** and
   would be dead code.

2. **Force out-of-lining of the libstdc++ copy ctor** (e.g. via
   explicit out-of-line definition in
   `awg_device_props.cpp`).  This would emit a symbol with a
   different body (libstdc++ shape) and a different linkage name
   (`_ZNSt7__cxx11...`-flavoured caller-side code).  It would
   neither match the binary's body nor fill the inventory's
   "absent" gap, since the inventory specifically flags the
   libc++ mangling.

The semantic equivalent — three string copies in declaration
order with proper exception cleanup — is already produced by the
recon at every callsite.  No observable behavior differs.

### Decision

Close cluster `awg_config::device` as **ABI-divergence by
design**.  No source change.  The deferred-cluster table in
OVERVIEW.md is updated to reflect that this cluster is now
documented (count drops from ~8 to ~7).

### Verification

- `nm reconstructed/build/_seqc_compiler.so | grep AwgPathPatternsC`:
  3-arg ctor present (T), copy ctor absent — confirmed inlining.
- `objdump -d _seqc_compiler.so` @0x2cc4f0: confirmed libc++
  3-string copy semantics with cleanup unwind.
- `python tests/diff_test_fast.py`: 1603/1603 PASS (no change
  from prior baseline — semantics already match).

**Action items**: none — close-by-design.

## IF-281  `device_option::device` cluster — restored binary-matching names: `DeviceType::deviceType()` and `DeviceTypeImpl::doClone()`

**Status**: fixed; cluster `device_option::device` retired.

**Severity**: cosmetic — observable behavior unchanged; restores 2 mangled symbols to binary parity.

### Symptom

D14 inventory (`reconstructed/notes/d14_inventory.md:763-778`) flagged
two symbols as **absent** in the recon:

- `_ZNK6zhinst10DeviceType10deviceTypeEv` @0x2d2c20 (9 B)
- `_ZNK6zhinst6detail14DeviceTypeImpl7doCloneEv` @0x2d3280 (81 B)

Both functions existed in the recon under different names:
`DeviceType::impl()` and `DeviceTypeImpl::clone()` respectively.
The bodies were semantically identical (single-field load,
`new DeviceTypeImpl(*this)`), but the source-level identifiers
diverged from the binary's mangled names.

### Root cause

Naming choice during initial reconstruction.  The cluster note
called these "DeviceOption hash/compare" — that prose is wrong;
the actual symbols are the pimpl accessor and the polymorphic
deep-copy method.  The recon followed C++ idiom (`impl()` for
a pimpl accessor; `clone()` for a virtual deep-copy) rather than
the binary's `deviceType()` / `doClone()` pair.

### Fix

Mechanical rename across 15 files:

- `reconstructed/include/zhinst/device/device_type.hpp`:
  `DeviceType::impl()` → `DeviceType::deviceType()`;
  `DeviceTypeImpl::clone()` → `DeviceTypeImpl::doClone()`.
- `reconstructed/include/zhinst/device/device_subclasses.hpp`:
  33 subclass overrides renamed `clone() const override` →
  `doClone() const override`.
- `reconstructed/src/device/device_type.cpp`:
  base impl renamed; both `impl_->clone()` callsites in the
  copy ctor and copy-assignment updated to `impl_->doClone()`.
- `reconstructed/src/device/device_{ghf,hdawg,hf2,hwmock,mf,pqsc,qhub,shf,shfacc,uhf,unknown,vhf}.cpp`:
  33 subclass definitions renamed.

`Node::clone()` and `NodeMapData::clone()` are unrelated and
left untouched.  `DeviceType::impl()` had zero callers in the
recon (and the binary), so the rename had no callsite impact
beyond the 2 sites inside `device_type.cpp` itself.

`GenericDeviceType` correctly retains no override (its vtable
slot reuses `DeviceTypeImpl::doClone` directly per
`generic_device_type.hpp:68-71`); the comments there already
referred to `doClone` and required no edit.

### Verification

- `nm reconstructed/build/_seqc_compiler.so | grep -E
  "10deviceTypeEv|7doCloneEv"`: both symbols now present
  (T = exported text).  The old `4implEv` / `5cloneEv`
  manglings are gone.
- `cmake --build .` clean (no new warnings).
- `python tests/diff_test_fast.py`: 1603/1603 PASS.

### Notes

The cluster overview prose ("hash/compare") in the inventory
was incorrect; it has been struck-through in the inventory
update.  Confirms AGENTS.md guidance that the D14 inventory's
"overview" prose is unreliable and one must always cross-check
the actual mangled symbol in the symbol-list section.

**Action items**: none — fix landed.

## IF-282  `node_misc::core` cluster — promoted anonymous-namespace `wrap23` helper to public `NodeMap::pauPoffIwrap`

**Status**: fixed; cluster `node_misc::core` retired.

**Severity**: cosmetic — observable behavior unchanged; restores 1 mangled symbol to binary parity.

### Symptom

D14 inventory (`reconstructed/notes/d14_inventory.md:792-798`)
flagged `_ZN6zhinst7NodeMap12pauPoffIwrapEj` @0x1c5650 (43 B) as
**absent** in the recon.  The 23-bit two's-complement wrap logic
already existed in `reconstructed/src/runtime/custom_functions.cpp`
as `wrap23` in an anonymous namespace, called only from
`NodeMap::toPhase`.  The binary, by contrast, exposes it as a
public static `NodeMap::pauPoffIwrap(unsigned int)`.

### Binary body (verified via `objdump -d --start-address=0x1c5650`)

```
00000000001c5650 <_ZN6zhinst7NodeMap12pauPoffIwrapEj>:
  mov    $0x400000, %eax
  cmp    $0x400000, %edi
  je     <ret>                  ; v == 0x400000 → return 0x400000
  test   $0x400000, %edi
  jne    <or-extend>             ; bit-22 set → sign-extend
  and    $0x3fffff, %edi         ; else → mask 23 bits
  jmp    <out>
or-extend:
  or     $0xffc00000, %edi
out:
  mov    %edi, %eax
  ret
```

23-bit two's-complement wrap with a special-case for the
sign-bit-only pattern `0x400000` (the encoded `INT23_MIN`,
left as-is to act as a saturation guard).

### Fix

- `reconstructed/include/zhinst/runtime/custom_functions.hpp`:
  add `static unsigned int NodeMap::pauPoffIwrap(unsigned int)`
  declaration with full doc-comment.
- `reconstructed/src/runtime/custom_functions.cpp`: drop the
  anonymous-namespace `wrap23` helper; emit
  `NodeMap::pauPoffIwrap` definition; route `NodeMap::toPhase`
  through `NodeMap::pauPoffIwrap`.

The function name `pauPoffIwrap` reads as "PAU/POFF immediate
wrap" — i.e. the 23-bit fold used by the per-channel
**Phase Accumulator Update** and **Phase Offset** sites that
need to encode a signed-23-bit immediate.  Now that it is
public, future PAU/POFF call-sites can share the same fold
instead of re-deriving it inline.

### Verification

- `nm reconstructed/build/_seqc_compiler.so | grep
  pauPoffIwrap`: `_ZN6zhinst7NodeMap12pauPoffIwrapEj` present
  (T = exported text).
- `cmake --build .` clean.
- `python tests/diff_test_fast.py`: 1603/1603 PASS.

**Action items**: none — fix landed.  If a future audit
identifies binary PAU/POFF call-sites in the assembler/codegen
that do this fold inline, route them through the new public
helper.

---

## IF-283  `GlobalResources` ctor re-seeds `random[]` MT19937-64 every construction; binary seeds once-per-thread

- **Severity**: low (behavioural divergence with no observed
  test impact; tests construct `GlobalResources` once per
  compile, so the second-and-subsequent-construction case
  doesn't fire in the suite).
- **Status**: fixed (2026-05-16).
- **Domain**: `runtime/global_resources.cpp`,
  `include/zhinst/runtime/resources.hpp`.
- **Symbols**: `_ZTHN6zhinst15GlobalResources6randomE`
  (binary @0x1f6090, recon was missing pre-fix);
  `_ZTWN6zhinst15GlobalResources6randomE` (binary @0x1f6180);
  `_ZN6zhinst15GlobalResourcesC1ERKSt10shared_ptrINS_9ResourcesEE`
  (binary @0x12a710).

### Symptom

The D14 inventory listed `_ZTHN6zhinst15GlobalResources6randomE`
as missing from the recon: the binary has it as a strong text
symbol; the recon emitted only weak `_ZTH...` for `regNumber`
and `labelIndex` and **nothing** for `random`.

### Root cause

In the binary, `GlobalResources::random` is a `thread_local`
array whose first-touch in any thread runs the gcc-emitted
TLS init wrapper (`_ZTH...`).  That wrapper is gated on a
guard byte at TLS+0xa18 and seeds the MT19937-64 state
(loop @0x1f60d7..0x1f6125 in the binary).  The
`GlobalResources` constructor itself does NOT re-seed: at
@0x12a80f..0x12a898 it inlines the *same* MT19937-64
recurrence, but the entire region is dead-coded behind the
same TLS+0xa18 guard — once the wrapper has run on this
thread, the ctor's region falls through.

In the recon, `random` was declared
`thread_local uint64_t random[313]` (zero-init, **no**
dynamic initializer), so gcc never emitted `_ZTH...random`.
The seeding lived only in the ctor body, and re-ran every
time `GlobalResources` was constructed on the same thread —
producing the same observable PRNG sequence per construction
(deterministic seed `0x1571`), but doing the work multiple
times and not matching the binary's once-per-thread guard.

### Fix

Change the type from `uint64_t[313]` to
`std::array<uint64_t, 313>` and add a function-call
initializer:

```cpp
thread_local std::array<uint64_t, 313> GlobalResources::random
    = seed_mt19937_64_state();
```

`seed_mt19937_64_state()` (anonymous namespace in
`global_resources.cpp`) builds the seeded state and returns
it.  gcc auto-emits both `_ZTHN...random` (init wrapper,
strong, matches binary @0x1f6090) and `_ZTWN...random`
(access wrapper, matches binary @0x1f6180).  The seeding
loop is removed from the ctor; a single
`(void)random[0];` touch ensures the wrapper runs at least
once before the ctor returns, matching observable behaviour
for the first construction on any thread.

Three call sites that decayed the array to `uint64_t*` now
use `.data()`:

- `custom_functions_playback.cpp:887` —
  `reinterpret_cast<Random*>(GlobalResources::random.data())`.
- `waveform_generator_dsp.cpp:988,1035` —
  `seqc_libcxx_normal_amplitude` /
  `seqc_libcxx_uniform` first-arg.

### Verification

- `nm reconstructed/build/_seqc_compiler.so | grep
  GlobalResources.*random`: `_ZTHN6zhinst15GlobalResources6randomE`
  now strong (`T`), matching binary.  `_ZTW...random` also
  emitted as expected.
- `cmake --build .` clean.
- `python tests/diff_test_fast.py`: 1603/1603 PASS.
- `python tests/diff_unreachable/harness.py`: 1626/1626 PASS.

### Why no test regression

Every difftest constructs `GlobalResources` exactly once
per compile and runs in a fresh process per test (fork-per-
test workers in `diff_test_fast.py`).  The
construct-twice-on-same-thread path the fix repairs is not
exercised by the suite.

**Action items**: none — fix landed.

---

## IF-284  `getKind(Exception const&)` and `getKind(boost::system::error_code const&)` deferred-by-design

- **Severity**: cosmetic (zero callers in recon; deferred
  with explicit rationale).
- **Status**: deferred (2026-05-16).
- **Domain**: `core/exception.hpp` / `core/exception.cpp`.
- **Symbols**:
  - `_ZN6zhinst7getKindERKNS_9ExceptionE` — binary @0x2e5180,
    189 B.
  - `_ZN6zhinst7getKindERKN5boost6system10error_codeE` —
    binary @0x2e50d0, 170 B.

### Why deferred

Both helpers exist in the binary as part of the
`ErrorCodeTraits` family closure but have **zero callers**
in any recon translation unit.  Faithful reconstruction
would require:

1. **For `getKind(Exception const&)`**: a `dynamic_cast`
   chain over `boost::wrapexcept<...>` thunks that read the
   inner `error_code`; this is mechanical but adds another
   `\verifyme` symbol with nothing to verify against.
2. **For `getKind(boost::system::error_code const&)`**: a
   real `boost::system::error_category*` comparison against
   an anon-namespace static `singleErrorKindCategory` (binary
   @0xb7c5a8) and the `boost::system::detail::generic_cat_holder`
   sentinel.  The recon currently uses a `ErrorCode` stand-in
   instead of `boost::system::error_code`, and faking
   `error_code` plus the category-holder machinery would
   add ~200 LoC of infrastructure used by exactly two
   functions that nobody calls.

The cost-benefit therefore tilts heavily against
reconstruction.  Both symbols are documented here as
*deferred-by-design* so future audits know to skip them
unless a recon caller appears (in which case the
`boost::system` interop must be built out properly).

### Verification trail (binary side)

- `objdump -d --disassemble=_ZN6zhinst7getKindERKNS_9ExceptionE
  _seqc_compiler.so`: 189 B; opens with
  vptr/RTTI dispatch on the passed `Exception&`, follows a
  `boost::wrapexcept` thunk path, ultimately tail-calls into
  the `error_code` overload.
- `objdump -d --disassemble=_ZN6zhinst7getKindERKN5boost6system10error_codeE
  _seqc_compiler.so`: 170 B; loads the
  category pointer from `error_code+0x8`, compares to
  `singleErrorKindCategory`'s vtable slot, and branches.

**Action items**: none unless a recon caller appears.  If
this changes, log a follow-up IF and reopen.
