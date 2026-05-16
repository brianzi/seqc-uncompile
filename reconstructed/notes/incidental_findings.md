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
- **IF-201..IF-300 closed entries** (48 entries: `fixed`,
  `dismissed`, `confirmed-fixed-cosmetic`, `confirmed-dismissed`,
  `confirmed-no-action`, `superseded`, `closed`): see
  [`archive/IF_201-300.md`](archive/IF_201-300.md).  Open entries
  from this ID range remain in the active sections below.

The active sections below cover the **open** entries from
IF-100..IF-200 plus the open entries from IF-201..IF-292 (latest
entries from Phase D, Phase F/G audits, and the compile_seqc
binding-kwarg coverage push).  IDs cited from `reconstructed/src/`
and `reconstructed/include/` comments may reference either the
active file or the archive.

---

> **Active entries below.**  Closed / deferred / tracking-only entries
> have been archived to
> [`archive/IF_1-99.md`](archive/IF_1-99.md),
> [`archive/IF_100-200.md`](archive/IF_100-200.md), and
> [`archive/IF_201-300.md`](archive/IF_201-300.md).

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

## IF-292  `compile_seqc` binding drops `None` kwargs; `waveforms=None` autoscan path missing

**Severity**: medium  
**Status**: open  
**Discovered**: while adding difftest cases for `compile_seqc` kwargs.

**Symptom**: when calling `compile_seqc(..., wavepath=<dir>, waveforms=None)`
the original binary scans `<dir>` for `*.csv` files and registers each as a
waveform (documented behaviour: *"Set [waveforms] to `None` to include all CSV
files in `wavepath`."*).  The reconstruction errors with:

```
no waveform with the name 'wave_file' found
tried to access uninitialized waveform
```

Reproduced with `tests/cases/manifest-core.json :: wavepath_nonempty`
(currently untagged from `compile-kwargs` to keep the suite green).

**Two distinct gaps:**

1. **Binding (`pybind_seqc.cpp::pyCompileSeqc`)** — the kwarg-merge loop
   tries `cast<double>` then `cast<string>`, and silently drops anything
   that's not one of those.  `None` falls into the catch-all and the
   `waveforms` key never makes it into the JSON config at all.  The
   original must serialize `None` as JSON `null` (or a sentinel) so the
   key is *present* with a "scan-all" marker.

2. **`compile_seqc` core (`reconstructed/src/codegen/compile_seqc.cpp`
   ~L221)** — the `waveforms` reader only handles `is_string()`.  Even
   if the binding wrote `null`, the recon would still skip
   `addWaveforms` and end up with an empty list.  There is no path
   that, given a non-empty `wavepath` and a "scan all" marker, calls
   something like `boost::filesystem::directory_iterator` over
   `wavepath` to collect `*.csv` files.

**Verification**: confirmed against the original via
`tests/diff_test_fast.py --filter wavepath_nonempty` (PASS for original,
FAIL for recon).

**Next steps**:
- GDB-trace original binding to see how `None` is serialized into the
  JSON config.
- GDB-trace original `compileSeqc` to find the autoscan call site (likely
  in or near the existing `addWaveforms` dispatch in
  `AWGCompilerImpl::addWaveforms` / `WaveformTable::newWaveformFromFile`).
- Implement both pieces and re-tag the manifest case.

## IF-293  `LaboneVersion` globals trapped behind `#ifdef ZHINST_HAS_PYBIND11`

**Severity**: cosmetic
**Status**: open
**Discovered**: while wiring `tools/seqcc/src/main.cpp` (Phase T1).

**Symptom**: the version strings `zhinst::LaboneVersion::fullVersionWithBuild`
and `zhinst::LaboneVersion::commitHash` are *declared* (`extern`) and
*defined* inside the `#ifdef ZHINST_HAS_PYBIND11` block of
`reconstructed/src/pybind_seqc.cpp` (lines 14 and 259).  In the binary
they live at `0xb01ad0` and `0xb01ad8` and are populated by the LabOne
build system at link time — they are not pybind-specific data, they are
part of the LabOne core ABI that pybind11 happens to read.

**Consequence**: any non-pybind consumer (the new `seqcc` driver in
`tools/seqcc/`, and any future C++ binary or test fixture that wants the
version) cannot link against these symbols even though the static
archive `zhinst_seqc.a` would happily provide them if the `#ifdef`
weren't in the way.  As a workaround, `tools/seqcc/src/main.cpp` defines
its own private `kSeqccVersion` string.

**Fix sketch** (deferred — purely cosmetic, no behaviour difference):

1. Move the two `const char*` definitions out of the
   `#ifdef ZHINST_HAS_PYBIND11` block — they belong with the rest of
   the LabOne version data, not with the pybind glue.
2. Move the matching `extern` declarations into a new tiny header
   `reconstructed/include/zhinst/infra/labone_version.hpp` so consumers
   don't have to re-declare them.
3. `tools/seqcc/src/main.cpp` then unifies `kSeqccVersion` to use
   `zhinst::LaboneVersion::fullVersionWithBuild` (with a fallback for
   the local build-system default).

**Verification**: not yet attempted; trivial to confirm with `nm
reconstructed/build/libzhinst_seqc.a | grep LaboneVersion` before and
after the move.

**TODO references**: `tools/seqcc/src/main.cpp` carries a `// TODO(IF-293):`
comment at the version-string definition pointing to this entry.

