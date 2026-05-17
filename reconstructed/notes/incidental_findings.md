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

## IF-294  seqcc: single-dash long flags require ad-hoc argv pre-rewrite

**Severity**: cosmetic
**Status**: open
**Discovered**: while implementing T2 default compile path.

**Symptom**: CLI11 (vendored v2.4.2, single-header) rejects long option
names with a single leading dash at registration time with
`CLI::BadNameString: Long names strings require 2 dashes -march`.  This
makes the gcc-style spelling `-march=HDAWG8` / `-mtune=key=value` /
`-mdevopts=...` impossible to register directly.

**Current workaround** (`tools/seqcc/src/main.cpp::rewriteGccLongFlags`):
walk argv before `CLI11_PARSE`, and for any token that starts with
`-march`, `-mtune`, or `-mdevopts` followed by end-of-string or `=`,
prepend a second `-` to produce `--march=`/`--mtune=`/`--mdevopts=`.
Tagged `// TODO(IF-294)` in `main.cpp`.

**Issues with the workaround**:
1. **Manually-maintained allowlist** — every new gcc-style flag added in
   T3+ (`-mtune` extensions, possibly `-mfloat-abi=`, etc.) has to
   remember to extend `kRewrite`.  Easy to forget; failure mode is a
   confusing "long names strings require 2 dashes" abort at startup.
2. **No round-trip in help text** — `seqcc --help` advertises `--march`,
   not `-march`; users discovering the flag from `--help` won't realise
   the gcc-style spelling works.  The current help blurb mentions it in
   prose only.
3. **Doesn't compose with CLI11 features** — flags registered by the
   workaround can't use CLI11's `->envname()`, completion hints, or
   subcommand routing without further special-casing.

**Possible fixes** (none implemented):
- Switch to a parser that allows single-dash long flags natively
  (`argparse`, `cxxopts`, hand-written).  CLI11's rejection is by
  design and won't be changed upstream.
- Patch CLI11 locally to relax the check.  Vendored header; trivial
  one-line change but creates a maintenance debt vs upstream.
- Drop the gcc-style spelling and use only `--march`, `--mtune`,
  `--mdevopts`.  Breaks the stated design goal of "gcc-style flags"
  in `tools/seqcc/DESIGN.md` §3.

**Verification**: confirmed by directly registering `-march` as a CLI11
long name during T2 development — produces the abort quoted above.

## IF-295  seqcc: `-mtune` channel conflates tuning, config, and pipeline knobs

**Severity**: cosmetic
**Status**: open
**Discovered**: while implementing T2 default compile path.

**Symptom**: in gcc, `-mtune=<microarch>` selects an instruction
scheduling target — a micro-optimisation hint.  seqcc T2 currently uses
`-mtune=KEY=VALUE` as a generic "JSON config kwarg" channel: legitimate
tuning keys (none yet exist), per-call config (`samplerate`,
`sequencer`), and core compilation inputs (`wavepath`, `waveforms`,
`filename`) all share the same syntax.

**Why this exists**: it's a one-line implementation that mirrors the
Python binding's `**kwargs` dict — every kwarg becomes a JSON key, no
per-key special-casing.  T2 prioritised byte-equality over surface
ergonomics.

**Concrete problems**:
1. **`--wave-path`/`--waveforms` from the Phase T design doc are
   not actually registered** as dedicated flags; they only work via
   `-mtune=wavepath=...` / `-mtune=waveforms=...`.  Users following
   `tools/seqcc/DESIGN.md` §3 will hit "unknown option".
2. **`filename` and `sequencer` aren't tuning knobs** — they shape the
   compile output, not its quality.  Better surfaced as
   `--filename=` / `--sequencer=` first-class flags.
3. **`-mtune` as the dumping ground for everything** means future
   genuine tuning knobs (loop-unroll limit, optimisation-pass flags,
   etc.) collide semantically with the existing keys.

**Proposed surface for T3** (not yet implemented):
- Promote `wavepath`, `waveforms`, `filename`, `sequencer` to dedicated
  `--wave-path`, `--waveforms`, `--filename`, `--sequencer` flags.
- Keep `-mtune=KEY=VALUE` as the **escape hatch** for kwargs the driver
  doesn't yet have a first-class flag for (matches gcc's `-mtune=`
  semantics: a way to set a low-level knob without a dedicated flag).
- Drop the `isNumericKey` allowlist in `options.cpp` — make it a CLI
  validation rule on the dedicated `--samplerate=` flag instead.

**Verification**: `seqcc --help` output shows only `--mtune` /
`--march` / `--mdevopts` / `--index` / `-o` registered; the
design-doc-promised `--wave-path` and `--waveforms` are absent.

## IF-296  seqcc: `--index` and `-mtune=index=` collide; the latter is silently a no-op

**Severity**: likely-bug
**Status**: open
**Discovered**: while triaging IF-295.

**Symptom**: seqcc accepts both `--index=N` (which writes
`opts.awgIndex`) and `-mtune=index=N` (which writes a JSON `index` key
into the config object).  `compileSeqc()` reads the AWG index **only**
from its `awgIndex` positional parameter (see
`reconstructed/src/codegen/compile_seqc.cpp` ~L137-148) — it never
inspects the JSON config for an `index` key.  The Python binding hides
this by routing `index=N` kwargs through a special-cased positional;
seqcc doesn't replicate that special case, so `-mtune=index=N` is
**silently dropped** by the compiler.

**Reproduction** (T2 build):
```
echo 'playZero(64);' > /tmp/t.seqc
seqcc -march=HDAWG8 -mtune=samplerate=2.4e9 -mtune=index=3 -o /tmp/a.elf /tmp/t.seqc
seqcc -march=HDAWG8 -mtune=samplerate=2.4e9                -o /tmp/c.elf /tmp/t.seqc
cmp /tmp/a.elf /tmp/c.elf   # → identical
```

The user intent was "AWG core 3"; the actual compile happened on core
0 with no warning.

**Two distinct fixes needed**:
1. **seqcc-side**: when `-mtune=index=N` is parsed in `options.cpp`,
   either (a) reject it with "use --index=N instead" or (b) special-case
   it to populate `opts.awgIndex` and drop it from `opts.tune`.  (b) is
   the binding's behaviour and what users probably expect.
2. **Defensive check**: in T3 when `--index=` becomes the canonical
   flag, drop the `index` entry from `isNumericKey` so the kwarg
   channel can't even take a numeric `index` value.  Tagged
   `// TODO(IF-296)` in `options.cpp`.

**Verification**: confirmed empirically; ELF byte-identical with and
without `-mtune=index=3`.  Disassembly of compileSeqc @0xf58a0 shows
no JSON-key lookup for `"index"`.

**Severity rationale**: marked likely-bug because the failure mode
is a silent miscompile (wrong AWG core), not a hard error.  Only saved
from being user-visible because seqcc is brand-new in T2 and has no
production users yet.

## IF-297  seqcc: `-mdevopts=` is a coined name; not idiomatic

**Severity**: cosmetic
**Status**: open
**Discovered**: while reviewing T2 surface.

**Symptom**: the third positional argument to the binding's
`compile_seqc()` is a *device-options string* — a newline-separated
list of feature flags like `"MF\nME"` for HDAWG multi-frequency /
multi-execution.  seqcc T2 exposes this as `-mdevopts=...` — a name
that doesn't appear in gcc, clang, or any toolchain prior art and
doesn't match either the binding's parameter name (`options`) or the
internal name (`upperOptions`).

**Why it's awkward**:
- gcc uses `-m<single-feature>` (e.g. `-msse4`, `-mavx2`), not a packed
  newline-separated string.
- A newline in a shell flag value is unwieldy: users have to write
  `-mdevopts=$'MF\nME'` or `-mdevopts="MF
  ME"`.
- The binding calls this parameter `options`, so the natural seqcc
  flag would be `--options=...` — but that's too generic and easily
  confused with seqcc's own driver options.

**Proposed surface for T3**:
- Promote to repeatable `-mdevopt=FEATURE` (note: singular) so multiple
  features stack: `-mdevopt=MF -mdevopt=ME`.  seqcc joins them with
  `\n` when building the binding's `options` string.
- Keep `-mdevopts=` (plural) as a deprecated alias accepting the raw
  newline-separated string, for users who already have it in a script.

**Verification**: not yet attempted; confirmed via reading
`reconstructed/src/codegen/compile_seqc.cpp` ~L172 and the docstring
in `pybind_seqc.cpp`.

**TODO references**: `tools/seqcc/src/main.cpp` carries
`// TODO(IF-297)` at the `--mdevopts` registration.

## IF-298  seqcc: repeatable options consume the trailing positional argument

**Date**: 2026-05-16
**Severity**: low (usability quirk; not a correctness issue)
**Status**: fixed in T3a — `->expected(1)->allow_extra_args(false)` on
each repeatable option (`-mdevopt`, `-mdevopts`, `-mtune`).

**Context**: With the T3a flag surface, `-mdevopt`, `-mtune`,
`--waveforms`, and similar option flags are registered as repeatable
(`std::vector<std::string>`) CLI11 options.  CLI11's default greedy
matching causes invocations such as

```
seqcc --march=HDAWG8 -mdevopt=MF input.seqc
```

to interpret `input.seqc` as a second value for `-mdevopt`, producing
`seqcc: no input file`.

**Workarounds**:
- Insert `--` before the positional input:
  `seqcc --march=HDAWG8 -mdevopt=MF -- input.seqc`
- Place the positional input before any repeatable option:
  `seqcc --march=HDAWG8 input.seqc -mdevopt=MF`

The diff harness (`tests/tools/test_seqcc_diff.py`) takes the `--`
route uniformly.

**Possible fixes for a later sub-phase**:
- Pin each repeatable option to `->expected(1)` so CLI11 takes one
  value per occurrence only.  This is the gcc-style behaviour users
  actually expect from `-mdevopt=FEATURE`.
- Or register the positional as `->required()` and document the `--`
  pattern in `--help`.

**TODO references**: none yet in source; revisit in T3b when other
repeatable options (`--dump=`) land.

## IF-300  seqcc: `add_option_function<std::string>->expected(1, INT_MAX)` silently drops repeated occurrences

**Date**: 2026-05-16
**Severity**: high (silent data loss; affected T3a `-mdevopt`/`-mtune`)
**Status**: fixed in T3b — switched to
`add_option(vector)->expected(1)->take_all()->allow_extra_args(false)`.

**Context**: T3a's IF-298 "fix" used CLI11's callback-style
`add_option_function<std::string>` with `expected(1, INT_MAX)` for
repeatable single-value options.  The intent was: callback fires once
per occurrence, each with one value.  The actual behaviour: the
callback fires **exactly once**, with only the **last** occurrence's
value.  Earlier occurrences are silently discarded.

Concretely, `seqcc -mdevopt=MF -mdevopt=ME ...` registered only `ME`.
The T3a test suite never asserted multi-occurrence behaviour
(IF-297's `test_legacy_packed_mdevopts_matches_repeatable` ran
against `playZero(64);` which compiles to the same ELF regardless of
`MF`/`ME`), so the regression hid behind a green suite for a full
sub-phase.

Symptomatic in T3b when wiring `--dump=KIND` repeatable; only the
last `--dump=` value was processed, despite the callback being
identical to `-mdevopt`'s.

**Correct CLI11 idiom for repeatable single-value, no positional
slurping** (verified against the CLI11 documentation, section
"Containers of options" and the multi-option-policy table):

```cpp
std::vector<std::string> values;
app.add_option("--flag", values, "help text")
    ->expected(1)              // exactly one value per occurrence
    ->take_all()               // policy: TakeAll across occurrences
    ->allow_extra_args(false); // do not slurp trailing positionals
```

All three modifiers are required:
- Without `expected(1)`, CLI11 allows multi-value occurrences and
  slurps anything that doesn't look like a flag.
- Without `take_all()`, the default `Throw` policy aborts on the
  second occurrence ("At Most 1 required but received N").
- Without `allow_extra_args(false)`, CLI11's vector-default
  `allow_extra_args` re-enables positional slurping.

The earlier `add_option_function<std::string>` form is fundamentally
unsuited for this case: CLI11 packages all values across all
occurrences as a single string vector before invoking the user
callback, then `lexical_cast`s to the template `T`.  With `T =
std::string`, only the *last* element ends up bound to the callback
parameter (it's a `Throw`-policy single-value option type that
overflowed silently).

**Regression test**: `test_repeatable_mdevopt_registers_all_occurrences`
in `tests/tools/test_seqcc_diff.py` uses the compile-report
`maxelfsize` field as a witness: HDAWG8 with `ME` present yields
2 GiB, without `ME` yields 256 MiB.  Comparing
`-mdevopt=MF` vs `-mdevopt=MF -mdevopt=ME` thus reliably detects
silent drops without depending on subtle ELF-byte differences.

**TODO references**: none in source; the fix lives at the four
`add_option(..., vector_target)->expected(1)->take_all()
->allow_extra_args(false)` call sites in `tools/seqcc/src/main.cpp`
(`--mdevopt`, `--mdevopts`, `--mtune`, `--dump`).


## IF-301  Sanctioned recon edit: introspection sink for `compileSeqc()`

**Severity**: process-note (no behavioural bug).
**Status**: **retired (T10a, 2026-05)**.  The introspection scaffold
(`compileSeqcWithIR()` entry point, `CompileSeqcIntrospection`
carrier struct, `fillIntrospection()` friend helper, and the three
private `AWGCompilerImpl::get{LoweredAst,Wavetable,AsmList}()`
accessors) was deleted once the owned `SeqcDriver` (T5a) became the
only seqcc compile path.  The driver now captures the same IR
handles directly via the public `AWGCompiler::compiler() →
Compiler::{ast(), wavetable(), asmList()}` accessors — two of which
(`ast()` and `wavetable()`) were added at T10a as a small sanctioned
recon edit parallel to the existing IF-307 `asmList()` accessor.
Net surface change is a reduction: one public `AWGCompiler`
accessor (`compiler()`, IF-307) + three inline `Compiler` getters
replace one parallel entry point + one carrier struct + one friend
grant + one helper + three pImpl accessors.  Recon-side
`compile_seqc.{hpp,cpp}` is back to the original-binary
single-entry-point footprint.

The historical notes that follow describe the design before T10a
and are kept for the audit trail.

---

The `seqcc` driver needs access to intermediate IRs (currently the
lowered AST, with more stages to follow in Phase T) for its `--dump`
surface.  Those IRs live behind `AWGCompiler`'s private impl and
were not reachable via any public API.  Three minimum-footprint
edits to reconstructed code were made instead of carrying the
plumbing as a tool-side hack:

1. `reconstructed/include/zhinst/codegen/awg_compiler.hpp`:
   - Forward declaration `class Node;` (needed for the free-fn
     decl below).
   - Single `friend` grant on `AWGCompiler` for a non-member helper:
     `friend std::shared_ptr<Node const> compilerLoweredAst(AWGCompiler const&) noexcept;`
   - Free-function declaration outside the class (matching
     namespace scope).
   - **No public method added.**
2. `reconstructed/src/codegen/awg_compiler.cpp`:
   - Private inline accessor `AWGCompilerImpl::getLoweredAst()`.
   - Free function `zhinst::compilerLoweredAst(AWGCompiler const&)`
     defined here (where `AWGCompilerImpl` is complete), delegates
     to the impl accessor.
3. `reconstructed/include/zhinst/codegen/compile_seqc.hpp` (new) +
   `reconstructed/src/codegen/compile_seqc.cpp` refactor:
   - Existing `compileSeqc()` body extracted into static
     `compileSeqcImpl(..., CompileSeqcIntrospection* sink)`.
   - Public `compileSeqc()` calls it with `sink == nullptr`
     (byte-identical behaviour to pre-edit).
   - New `compileSeqcWithIR()` calls it with a real sink that
     receives `loweredAst` via `compilerLoweredAst(compiler)`.

**Why this exception was allowed**:

- The driver could not be implemented without it.  Workarounds
  (re-running parse/lower in the driver, RTTI hacks on
  `AWGCompiler`'s vtable, reinterpret-cast over the layout) were
  all more invasive or fragile than the friend grant.
- The new surface is strictly additive — `compileSeqc()` and
  every existing caller of `AWGCompiler` are byte-equivalent
  before and after the edit (verified: diff_test_fast 1612/1612,
  seqcc ELF-byte-equality test confirms `compileSeqcWithIR`
  produces identical bytes to `compileSeqc`).
- Footprint is minimal: 1 friend decl + 1 free-fn decl in the
  public header; 1 private accessor + 1 free-fn defn in the .cpp;
  1 new small public header for the introspection struct; an
  internal refactor of `compile_seqc.cpp` that adds a single
  static function and a second entry point.

**Retirement plan**: when/if the reconstruction grows a first-
class introspection API (e.g. a `Compiler::passManager()` style
hook that returns per-stage IRs), `compileSeqcWithIR` and
`compilerLoweredAst` should be removed in favour of that.  Until
then the surface remains, gated by the seqcc dump pipeline.

**TODO references**: TODO.md Phase T → T3c (done in this commit).
Future Phase T entries that need additional IR stages
(`asm-pre-opt`, `wavetable-ir`, etc.) extend
`CompileSeqcIntrospection` rather than adding new entry points.


## IF-302  `Node::toJson(idMap)` requires asm-id remap not exposed to seqcc

**Severity**: low (workaround in place, output is structurally
valid but ids differ from pybind serialisation).
**Status**: **fixed in T3d** (commit landing alongside this update).
The proper densified pass-1 map is now computed driver-side
from `CompileSeqcIntrospection::asmList`, populated by
`fillIntrospection()` via a new `AWGCompilerImpl::getAsmList()`
accessor.  The identity-map workaround is retained only as a
safety net (`fillMissingIdsIdentity` in `tools/seqcc/src/dump.cpp`)
for AST nodes whose `asmId` is not present in the captured
AsmList — without it, `Node::toJson()` would throw
`std::out_of_range` from `idMap.at()`.

**Caveat on "parity"**: closer inspection during T3d shows that
the pybind path **never** serialises `Node` standalone — the
only call site of `Node::toJson(idMap)` is inside
`AsmList::serialize()` at `asm_list.cpp:76`, where the map and
the call happen at the same point in the pipeline.  There is
therefore no "pybind ast-lowered dump" for seqcc to be
byte-identical to; the `--dump=ast-lowered` artifact is a
seqcc-only diagnostic.  T3d still does the right thing — feed
the densified map computed from the same algorithm pybind
uses internally — but the framing "byte-identical to pybind"
in the original IF text was incorrect.

**Historical workaround text** (kept for archaeology):

`Node::toJson(const std::unordered_map<int,int>& idMap) const`
(`reconstructed/src/ast/node.cpp:506`) unconditionally calls
`idMap.at(asmId)` to look up the JSON-serialisable id for the
current node.  The proper map is built in `AsmList::serialize()`
pass 1 (`reconstructed/src/asm/asm_list.cpp:187-200`): iterate
asm entries, skip `opcode == 4` (NOP) and
`opcode == -1 && !node`, assign sequential indices keyed by
`entry.sequenceId`.

The seqcc driver only captures the lowered AST root through
`CompileSeqcIntrospection`; it has no AsmList in hand at dump
time (the AsmList is built later, deeper in
`compileSeqcImpl`).  Calling `toJson({})` with an empty map
throws `std::out_of_range` for any node with `asmId >= 0`, which
is all of them.

**Workaround currently shipping** (`tools/seqcc/src/dump.cpp`
`collectAsmIdsIdentity`): walk the AST via `next`, `loop`, and
`branches`, collecting every reachable node's `asmId` into an
identity map (`asmId → asmId`).  This makes
`toJson` succeed and produces structurally-valid JSON.  The
resulting `asmId` cross-references in the JSON, however, point at
the original ids rather than the densified post-pass-1 indices
that pybind's serialisation would use, so the output is not
byte-identical to a hypothetical pybind equivalent.  For
debugging the dump is fully usable.

**Proper fix** (queued in TODO.md): capture the `AsmList`
alongside the AST in `CompileSeqcIntrospection`, run the
pass-1 densification driver-side, and feed the real map to
`toJson`.  Defer until a second IR stage (`asm-pre-opt` or
similar) is wired up — capturing the AsmList for ast-lowered
alone would be wasteful since later dump kinds will need it
anyway.

**TODO references**: TODO.md Phase T → "ast-lowered idMap parity"
(to be added when the AsmList introspection lands).


## IF-303  `WavetableFront` has no public JSON serialiser

**Severity**: low (workaround in place via text dump).
**Status**: open; queued in TODO.md Phase T as a follow-up to T4a.

`WavetableFront` exposes only `toString()` publicly
(`reconstructed/include/zhinst/waveform/wavetable_front.hpp:228`),
which produces a per-waveform text block.  The JSON serialiser
`toJson()` lives on the *inner* templated
`detail::WavetableManager<WaveformT>` class
(`wavetable_front.hpp:303`) and is reached through
`WavetableFront::manager_`, a private member
(`wavetable_front.hpp:26`).

For seqcc's T4a `--dump=wavetable` we picked the text path so the
introspection sink stays clean (single friend grant on
`AWGCompiler`).  Exposing the JSON would require either:

- A new public `WavetableFront::toJson()` method that delegates
  to `manager_->toJson()`.  Strictly additive; minimal recon
  surface increase; would let us add `--dump=wavetable-json`
  alongside the text variant.
- A friend grant on `WavetableFront` for the seqcc helper.
  Worse — violates the single-friend principle we just
  established for `AWGCompiler`, and the same logic would have
  to recur for every future "private-inner-serialiser" case.

**Recommended fix when revisited**: add the public
`WavetableFront::toJson()` delegate, document as a B3 sanctioned
exception, then register `wavetable-json` as a sibling dump kind
to the existing text `wavetable`.  Defer until a concrete
debugging need arises for the JSON form.

**TODO references**: TODO.md Phase T → "T4a follow-up:
WavetableFront::toJson() delegate" (to be added if/when needed).

## IF-304  seqcc: `--to=<stage>` is a logical stop, not a literal one

**Discovered**: 2026-05, during T5 implementation.

**Severity**: low (documentation / user-expectation).

**Status**: **partially fixed at T5b.5 (2026-05)**.  The back-end
stages (`stepAssembleOpcodes`, `stepCheckLimits`, `writeToStream`
ELF link) are now literally skipped when `opts.toStage` is
`lower` or `asm`.  The front-end stages inside
`Compiler::compile()` still run to completion — see "Remaining
gap" below.

### Resolution achieved at T5b.5

`SeqcDriver::compile()` was rewritten to call the three
`AWGCompiler::stepXxx` forwarders sequentially:

1. `stepInnerCompile(source)` — always required (populates the
   IR sinks and the cached `assemblerText`).
2. If `opts.toStage` is `link` (default): call
   `stepAssembleOpcodes()`, `stepCheckLimits()`, then
   `writeToStream()` to produce the ELF.
3. If `opts.toStage` is `lower` or `asm`: return immediately
   with an empty ELF.  The driver's `dump.cpp::renderStagePrimary`
   reads `sinks.assemblerText` (for `--to=asm`) or the lowered-
   IR JSON from `sinks` (for `--to=lower`) — no ELF parsing
   needed.

For HDAWG8 `hdawg_doc_simple.seqc`:

| Stage | Output size | Back-end run? |
|---|---|---|
| `--to=link` | 27088 bytes (ELF) | yes |
| `--to=asm`  | 360 bytes (text) | no |
| `--to=lower`| 609 bytes (JSON) | no |

### Remaining gap

The user-chosen T5b.5 scope was "partial: AWGCompiler-level
only".  `Compiler::compile()` (the front-end stack) still runs
end-to-end inside `stepInnerCompile` even when the driver only
wants the lowered AST.  A full literal early-exit for `-E`
would additionally require:

- Either constructing a `Compiler` directly from the driver
  and calling its public `stepParse` … `stepLower` methods,
  stopping at `stepLower`.  This needs an accessor on
  `AWGCompiler` that exposes the inner `Compiler&` (or a way
  to construct a free-standing `Compiler` — currently blocked
  because `Compiler`'s constructor needs `WavetableFront` and
  `DeviceConstants` owned by `AWGCompilerImpl`).
- Or new `AWGCompiler::compileUpToLower()` / similar
  short-circuit entry points that internally drive only the
  needed front-end steps.

Neither is justified at T5b: the dominant cost saving is
already realised (the back-end stages — `Project`, the AsmList
→ opcode translation, the device-limit checks, and ELF
serialisation — were the bulk of the post-lower cost).  A
future workload that makes the residual front-end cost
measurable in `-E` mode would be the trigger to revisit this
as a B3 sanctioned recon exception.

### Verification

- `tests/tools/test_seqcc_to.py` (T5b.5, 4 cases): asserts
  payload shape for `--to=link` / `--to=asm` / `--to=lower`
  and that the three byte streams differ in size as expected.
- All 5 regression gates clear at T5b.5:
  `diff_test_fast` 1612/1612, `test_seqcc_diff` 46/46,
  `test_seqcc_ab` 15/15, `test_seqcc_smoke` 4/4,
  `test_seqdump` 16/16.

**TODO references**: TODO.md Phase T → T5b.5 (done); residual
gap tracked here, no separate TODO item until profiling
justifies it.

---

## IF-305  Sanctioned recon edit: `optimizationFlags` jsonConfig key for `compileSeqc()`

**Severity**: process-note (no behavioural bug).
**Status**: sanctioned exception under AGENTS.md "Tooling vs
reconstructed code".  Approved by user before edit landed
(Phase T8, "Path 1").

The `seqcc` driver needs to expose gcc-style `-O<n>` and
`-f[no-]<pass>` flags that ultimately control the bitmask in
`AWGCompilerConfig::optimizationFlags` (+0x88), consumed by
`AsmOptimize`.  The original binary hardcodes this field to
`0xFF` in `compile_seqc.cpp`; the pybind binding offers no
kwarg for it.

**Edit**: `reconstructed/src/codegen/compile_seqc.cpp` (~L242)
reads an optional `optimizationFlags` key from the existing
`jsonConfig` `boost::json::object`, accepting `int64`, `uint64`,
or non-negative `double`.  Default remains `0xFF` when the key
is absent, so:

- The pybind path (which never injects the key) is
  byte-identical to pre-T8.
- diff_test_fast 1612/1612 holds with no regressions.
- The recon side has no new function/type/header surface; only
  one local variable + one `if-contains` block + one
  assignment changed from a literal to a variable.

**Why this exception is justified**:

1. The field already exists in `AWGCompilerConfig`; the original
   binary already wires it through to `AsmOptimize`.  We are
   not introducing new compiler behaviour — only making an
   existing knob reachable from the tooling.
2. `jsonConfig` is by design a free-form key-value channel for
   callers to inject overrides without expanding the C++ ABI of
   `compileSeqc()`.  Adding a new recognised key is the
   idiomatic extension point.
3. The alternative (Path 2 — a new `compileSeqcWithFlags()`
   wrapper that takes an explicit `optimizationFlags`
   parameter) would add a third public entry point alongside
   `compileSeqc()`/`compileSeqcWithIR()`, plus a friend grant
   to `AWGCompiler`, plus duplicate plumbing.  The user
   explicitly chose Path 1 to minimise recon surface.
4. The change is one-shot: T8 closes this lane.  Future
   optimisation work in seqcc reuses the same jsonConfig key
   with no further recon edits.

**Driver-side wiring**: `tools/seqcc/src/optflags.{hpp,cpp}`
(pass table, `-O<n>` mask computation, `applyPassToggle`,
`printPassTable`); `tools/seqcc/src/main.cpp::extractOptFlags`
pre-parses argv before CLI11 sees it; `tools/seqcc/src/options.cpp`
emits `"optimizationFlags": N` only when the user actually
passed a flag.

**Verification**:
- diff_test_fast 1612/1612 (default-path byte-identical).
- seqcc_diff 43/43 (+8 T8 cases).
- `-O3` ELF byte-equals default ELF (both 0xFF).
- `-O0` ELF byte-differs from default and is strictly larger.
- `-fno-reg-alloc` byte-differs from default.
- Unknown `-f<name>` surfaces as a CLI11 "unrecognised option"
  diagnostic (not silently ignored).

**TODO references**: TODO.md Phase T → T8 (done).

## IF-306  Sanctioned recon edit: stepwise `Compiler::compile()` / `AWGCompilerImpl::compileString()` refactor (T5b)

**Severity**: process-note (no behavioural bug; structure-preserving
refactor).
**Status**: **fixed (T5b.6, 2026-05)**.  All six sub-phases
(T5b.1 – T5b.6) have landed; the stepwise refactor is in place
on both `Compiler` and `AWGCompilerImpl`, the seqcc driver
consumes the step methods, and the design document (§3.1 and
§5.4 of `tools/seqcc/DESIGN.md`) has been updated to reflect
the realised pipeline.  Final regression sweep: all 5 gates
green (`diff_test_fast` 1612/1612, `test_seqcc_diff` 46/46,
`test_seqcc_ab` 15/15, `test_seqcc_smoke` 4/4, `test_seqdump`
16/16, `test_seqcc_to` 4/4).  Sanctioned under AGENTS.md
"Tooling vs reconstructed code" / "Allowed exceptions"; user
approval recorded before each sub-phase edit landed.

### Motivation

The `seqcc` driver needs to drive the compile pipeline stage by
stage so that:

- `--to=<stage>` becomes a literal stop (closes IF-304), not a
  logical one — `seqcc -E foo.seqc` should not pay the full
  post-lower cost.
- `--from=<stage>` (T6) can construct a `Compiler`, hand-populate
  mid-pipeline state from a deserialised IR, and resume from the
  step that follows the chosen entry point.
- Additional dump artefacts unlocked by mid-pipeline observation
  (`asm-pre-opt`, `asm-post-pre-opt`, `asm-post-prefetch`,
  `asm-final`, `wavetable-ir`) become trivial — the driver
  inspects `compiler.asmList_` / `compiler.wavetableIR_` between
  step calls instead of needing per-pass callback machinery.

The withdrawn DESIGN.md §5.4 `passTap_` approach would have
satisfied these needs by inserting `std::function` callback
members on `AsmOptimize`, `Prefetch`, `Cache`, plus ~12 tap
call sites in the reconstructed `.cpp` files.  That approach
was withdrawn at T5a.4 because it conflicts with AGENTS.md
commit `6b2d504` ("tooling vs reconstructed code: hands off"),
which bans tooling-driven additions of new members, `friend`
grants, or hooks on reconstructed classes.

The T5b refactor replaces it: split the existing monolithic
methods into named step methods that are public, structurally
mirror the binary, and require no new callback infrastructure.

### Scope of the edit

Two files, two methods:

1. `reconstructed/src/codegen/compiler.cpp` — split
   `Compiler::compile()` (current L235–566) into 9 step methods:
   - `stepParse` — steps 1–3b (reset, line-ending normalisation,
     parse, empty-input early-out).
   - `stepToSeqCAst` — steps 5–6 (static/global resources,
     SeqC-AST conversion).
   - `stepLower` — steps 7–9 (front-end lowering, error check).
   - `stepBuildAsmPreamble` — steps 10–11d (preamble, node-tree
     walk, EvalResults insert, trailer).
   - `stepOptPre` — step 12 + step 13 debug-dump + step 13b
     round-trip + step 13c WavetableIR construction.
   - `stepPrefetch` — step 14 (`runPrefetcher`).
   - `stepOptPost` — step 15.
   - `stepUnsyncCervino` — step 16.
   - `stepProject` — steps 17–19b (debug print, error check,
     build output vector, cache `usedSampleRate_`).
2. `reconstructed/src/codegen/awg_compiler.cpp` — split
   `AWGCompilerImpl::compileString()` similarly into 3 steps:
   - `stepInnerCompile` — call `Compiler::compile()`.
   - `stepAssembleOpcodes` — translate `CompileResult` to opcode
     vector.
   - `stepCheckLimits` — apply max-elf-size / wavemem-size limits
     and emit diagnostics.

   The public `AWGCompiler::compileString()` becomes a one-liner
   that calls the three steps in order.

Cross-step stack locals in `Compiler::compile()` are promoted to
private `Compiler` members so they survive between step calls:
- `expr` (raw `Expression` AST from parse) → `parsedExpr_`
- `seqcAst` (typed AST for lowering) → `seqcAst_`
- `lowerResult` (`FrontEndLoweringFacade::LowerResult`) →
  `lowerResult_`
- `resources` (`GlobalResources`) → `compileResources_`
- `staticResources` (`StaticResources`) → `compileStaticResources_`
- `rootNode` → `compileRootNode_`
- `placeholderAsm` → `compilePlaceholderAsm_`
- `wavetableIR` → `compileWavetableIR_`
- `optimizer` (`AsmOptimize`) → `compileOptimizer_`

Pre-existing `ast_`, `asmList_`, `messages_`, `wavetable_`,
`asmCommands_`, `customFunctions_`, `waveformGen_`, `config_`,
`deviceConstants_`, `cancelCallback_`, `usedSampleRate_`,
`reserved18_` remain unchanged.

The 9 step methods are public so the seqcc driver can call them
individually.  No new friend grants; no new callback members.

### Authorisation (verified)

Each step boundary cited above corresponds to a verified binary
address already documented inline in
`reconstructed/src/codegen/compiler.cpp`:

| Step | Binary address | Source comment |
|---|---|---|
| `stepParse` entry | `0x11f150` | `// 0x11f150 (~13KB)` (L234) |
| step 2 boundary | `0x11f268` | `// 0x11f268` (L248) |
| step 3 boundary | `0x11f27e` | `// 0x11f27e` (L251) |
| step 6 boundary | `0x11f7b0` | `// 0x11f7b0` (L300) |
| step 8 boundary | `0x11f911` | `// 0x11f911` (L327) |
| step 10 boundary | `0x11fb1a` | `// 0x11fb1a` (L350) |
| step 12 boundary | `0x120707` | `// 0x120707` (L453) |
| step 13c boundary | `0x120c92` | `// 0x120c92` (L496) |
| step 14 boundary | `0x120d60` | `// 0x120d60` (L506) |
| step 15 boundary | `0x120e2d` | `// 0x120e2d` (L510) |
| step 16 boundary | `0x120f2b` | `// 0x120f2b` (L519) |
| step 19 boundary | `0x121345` | `// 0x121345` (L543) |

The proposed step-method split lands its boundaries on these
verified addresses.  No new control flow is introduced; the
refactor is a partitioning of the existing straight-line body,
not a reshaping of it.

### Why this exception is justified

1. **Structural fidelity to the binary**: each new step method
   maps onto a numbered phase that the original binary already
   exhibits (the `0x11f...` / `0x120...` addresses above are
   sequential blocks in the disassembly).  We are not inventing
   new compile phases; we are giving names to phases the binary
   already has.
2. **No new external surface**: no new types, no new free
   functions, no `friend` grants, no callback fields, no
   `std::function` storage.  Only existing private locals are
   lifted to private members, plus the methods themselves are
   promoted to public.
3. **Behaviour preserved by construction**: the public
   `Compiler::compile(source)` becomes a sequential call of the
   9 step methods in the same order the original body executed
   them.  The byte-equality invariant is `diff_test_fast 1612/1612`
   plus `test_seqcc_diff 46/46`.
4. **Single-shot**: T5b closes this lane.  T6 (`--from=<stage>`)
   reuses the same stepwise API with no further recon edits; T10a
   retires the deprecated `compileSeqcWithIR()` /
   `CompileSeqcIntrospection` introspection sink (no longer
   needed once the driver owns the `Compiler` and can read its
   members directly).
5. **Alternative is worse**: the withdrawn `passTap_` approach
   would have added ~12 callback fields and call sites scattered
   across 4 .cpp files, with associated comment markers and
   condition-checking overhead in every pass.  The stepwise
   refactor concentrates the change in two .cpp files and adds
   zero runtime cost.

### Driver-side consumption (forward reference)

The seqcc driver (`tools/seqcc/src/driver.cpp`,
`SeqcDriver::compile()`) will be rewritten in T5b.5 to construct
a `Compiler` directly and call its step methods sequentially.
This replaces the current pass-through to
`compileSeqcWithIR()` (T5a.2) and lets `--to=<stage>` short-
circuit after the requested step.

### Verification

Each sub-phase commit must clear:

- `diff_test_fast` 1612/1612 (byte-equality invariant — the
  refactor is wrong if any test changes output).
- `test_seqcc_diff` 46/46 (driver still byte-equal to pybind).
- `test_seqcc_ab` 15/15 (legacy and driver paths still agree).
- `test_seqcc_smoke` 4/4 + `test_seqdump` 16/16 (no toolchain
  regression).

### Risks

- **Member ordering churn**: lifting 9 stack locals to members
  changes the `Compiler` class layout.  Anyone holding raw
  pointers across `Compiler` instances would be affected; in
  practice nothing outside the recon tree does this.  The
  ABI-stability invariant is "this binary is not consumed as a
  shared library outside the recon project", which holds.
- **State leakage between compiles**: lifted members survive
  across `compile()` calls.  Step methods must not assume their
  inputs are zero-initialised — `Compiler::reset()` (already
  existing, L125) is the right place to zero the lifted members.
  Audit `reset()` after the lift; this is part of T5b.2's
  acceptance criteria.
- **Exception safety**: the original body has try/catch around
  `optimizePreWaveform` and `optimizePostWaveform`.  The step
  split must preserve those — `stepOptPre` and `stepOptPost`
  contain the try blocks in full, not the calling code.
- **`reset()` semantics for partial pipelines**: when T6 lands
  `--from=<stage>`, the driver will call `Compiler::reset()`
  then populate selected members directly, skipping earlier
  steps.  `reset()` must leave the object in a state where
  later step methods can be called without preceding ones
  having run.  This may surface as an IF if the recon `reset()`
  has any cross-step initialisation we missed.

### TODO references

- TODO.md Phase T → T5b (in progress).
- Closes IF-304 at T5b.5.
- Prerequisite for T6.
- Prerequisite for T10a (retirement of `compileSeqcWithIR` /
  `CompileSeqcIntrospection`).

## IF-307  Sanctioned recon edit: `AWGCompiler::compiler()` accessor + `Compiler::setupResources()` helper + narrow setters (T6)

**Severity**: process-note (no behavioural bug; additive surface
to enable `seqcc --from=asm`).
**Status**: **proposed (2026-05)** — pre-approved by user before
T6.1 edits land.  Extended at T6.2 (see "T6.2 extensions" section
below).  Will be updated to "fixed" when T6 closes.

### Motivation

Phase T6 lands `seqcc --from=asm`, the start-at-stage entry point
that round-trips an `.seqasm` file through `AsmList::deserialize`
and resumes the pipeline at `stepOptPre`.  The data-flow analysis
in `reconstructed/notes/seqcc_from_design.md` shows that to do this
from the driver side, three pieces of `Compiler` state must be
reachable that are not reachable today:

1. The driver needs a handle to the inner `Compiler` instance
   owned by `AWGCompilerImpl`, in order to call `stepOptPre`
   onwards.  `AWGCompiler` is a pImpl over `AWGCompilerImpl`
   (sole member `impl_*`); the inner `Compiler compiler_` lives
   at `AWGCompilerImpl+0x0C0` (0x138 bytes), and there is no
   public accessor.
2. The driver needs to populate `compileStaticResources_` and
   `compileResources_` — per-compile scaffolding built by
   `stepToSeqCAst:351-363` from `config_`, `deviceConstants_`,
   and `messages_`.  These cannot be supplied from a single
   input file; the driver must run the same construction
   sequence as `stepToSeqCAst`.
3. The driver needs to write `asmList_` (private, binary-layout
   member at `compiler.hpp:803`) and `compilePlaceholderAsm_`
   (private, T5b.2 lifted, at `compiler.hpp:843+`) from the
   deserialised `.seqasm` contents.

### Scope of the edit

Three files, four additions.  Total: ~5 lines of header
additions, ~15 lines of `.cpp` additions, ~2 lines of in-place
refactor inside `stepToSeqCAst`.

1. **`reconstructed/include/zhinst/codegen/awg_compiler.hpp`** —
   forward-declare `class Compiler`; add public
   `Compiler& compiler()` and `Compiler const& compiler() const`
   accessors.  Implementation lives in `awg_compiler.cpp` so
   the public header does not need to include `compiler.hpp`.
2. **`reconstructed/src/codegen/awg_compiler.cpp`** — implement
   the two `compiler()` accessors as one-liners returning
   `impl_->compiler_`.
3. **`reconstructed/include/zhinst/codegen/compiler.hpp`** /
   **`reconstructed/src/codegen/compiler.cpp`** — factor the
   resources-construction block (`stepToSeqCAst` body lines
   351-363) into a new public `void setupResources()` member.
   `stepToSeqCAst` becomes a 1-line call to `setupResources()`
   followed by the existing AST-conversion code.  Binary path
   is unchanged (same code, same order, called from the same
   site).
4. **`reconstructed/include/zhinst/codegen/compiler.hpp`** /
   **`.cpp`** — add two narrow public setters (Option 7b from
   the design note):
   - `void setAsmList(AsmList list)` — stores into private
     `asmList_`.
   - `void setPlaceholderAsm(AsmList::Asm asm)` — stores into
     private `compilePlaceholderAsm_`.

   Both are one-line bodies in the `.cpp` to keep the header
   independent of `AsmList`'s full definition where the
   forward-declare suffices.

### Authorisation (verified)

All claims below were verified by reading the cited source files
in this session.  Binary-address claims are sourced from the
verified-address table in IF-306 (T5b sanctioned exception)
and `reconstructed/notes/seqcc_from_design.md` §1.

| Slot | Header line | Section |
|---|---|---|
| `asmList_` | `compiler.hpp:803` | private (started at 742) |
| `compilePlaceholderAsm_` | `compiler.hpp:843+` | private (started at 742) |
| `compileResources_` / `compileStaticResources_` | `compiler.hpp:843+` | private |
| `stepToSeqCAst` resources block | `compiler.cpp:351-363` | inside `step*` method body |
| Inner `Compiler compiler_` member | `awg_compiler.cpp` pImpl, offset +0x0C0 | `AWGCompilerImpl` private |
| Existing public sections on `Compiler` | `compiler.hpp:194` (ctor + API), `compiler.hpp:667` (T5b.4 step methods) | public |

### Why this exception is justified

1. **Minimum-footprint** under the AGENTS.md "Tooling vs
   reconstructed code" policy: the alternative (re-implementing
   `setupResources` inside the driver) would duplicate per-compile
   scaffolding that is not part of `seqcc`'s scope, and would risk
   silent drift between the driver's copy and the recon body if
   `stepToSeqCAst` ever changes.  The factored helper keeps a
   single source of truth.
2. **No new external surface beyond what `--from=` requires**: no
   new types, no new free functions, no `friend` grants, no
   callback fields.  One pImpl accessor (already idiomatic on
   `AWGCompiler`), one helper method, two setters.
3. **Behaviour preserved by construction**: `setupResources()`
   is a code move, not a behaviour change; the public entry
   `Compiler::compile(source)` still calls `stepToSeqCAst` which
   still calls `setupResources()` as its first action.
   `diff_test_fast 1612/1612` is the byte-equality invariant.
4. **Symmetric with T5b**: T5b made the pipeline addressable
   stage-by-stage; T6 makes the per-stage state addressable.
   `setAsmList` / `setPlaceholderAsm` are the input-side mirror of
   the step-method outputs.
5. **Alternative considered and rejected**: 7a (promote
   `asmList_` and `compilePlaceholderAsm_` to public) would zigzag
   the public/private boundary in the binary-layout member region
   (line 742+), forcing surrounding members to move and risking
   layout-related regressions.  7c (recover placeholder by scanning)
   was rejected because the placeholder may carry state that is
   not solely identifiable from its opcode.

### T6.2 extensions (landed 2026-05, status proposed)

Three additions to the originally-sanctioned T6.1 surface, all
additive and verified byte-clean against `diff_test_fast 1612/1612`
+ all 85 tool-side tests on the working-tree build:

1. **`Compiler::asmList() const` accessor**
   (`reconstructed/include/zhinst/codegen/compiler.hpp`, inline
   one-liner after `setPlaceholderAsm`).  Returns
   `AsmList const& asmList_`.  Needed by the driver's refactored
   `--to=asm` path so that the dump can capture `asmList_` at the
   binary's natural cut (after `stepOptPre`, before `stepPrefetch`)
   — see IF-308 for why that cut matters.  Read-only; does not
   widen the mutation surface.
2. **`AWGCompilerImpl::stepInnerCompileFromAsmList(source, list,
   placeholder)`** (declaration + ~95-line implementation in
   `reconstructed/src/codegen/awg_compiler.cpp`).  Mirrors the
   prelude of `stepInnerCompile` but drives the inner `Compiler`
   via the resume sequence
   `reset → setupResources → setAsmList → setPlaceholderAsm →
   stepPrefetch → stepOptPost → stepUnsyncCervino → stepProject`
   (skipping `stepOptPre` because the input is already
   post-`stepOptPre` per IF-308).  Copies the result to
   `compileString_asmList_` and `wavetableIR_`, re-builds
   `assemblerText_` with the identical pretty-print loop used by
   `stepInnerCompile`, and wraps inner exceptions as
   `ZIAWGCompilerException` with the same message-append
   contract.  This keeps the resume entry point inside the recon
   side (one source of truth for assembler-text formatting) and
   gives the driver a single back-end-equivalent entry to call.
3. **`AWGCompiler::stepInnerCompileFromAsmList` forwarder**
   (`reconstructed/include/zhinst/codegen/awg_compiler.hpp` +
   `awg_compiler.cpp`) — pImpl forwarder symmetric with the other
   `step*` forwarders.  Also adds `#include
   "zhinst/asm/asm_list.hpp"` to the header because the signature
   takes `AsmList::Asm` by value (forward-declare insufficient).

These additions preserve the IF-307 minimum-footprint rationale:
the alternative — driving the resume sequence directly from the
driver — would force `stepInnerCompileFromAsmList`'s ~95-line
prelude (including the assembler-text pretty-print loop, the
exception-wrapping contract, and the `wavetableIR_` / 
`compileString_asmList_` copy-out) to live in `tools/seqcc/`,
duplicating recon logic in a place where it would silently drift.
Keeping it on the recon side means the driver's `--from=asm`
branch is the same three-line shape as a normal compile.

### Driver-side consumption (forward reference)

The seqcc driver (`tools/seqcc/src/driver.cpp`) will be extended in
T6.2 with a new resume path:

```cpp
// pseudocode (post-T6.2 extensions — single recon-side entry)
AsmList list = AsmList::deserialize(/* contents of input.seqasm */);
// Identify the placeholder by NodeType::Load — sole pre-prefetch
// producer (verified at asm_commands.cpp:939-941).  Replaces the
// earlier sidecar-file design.
AsmList::Asm placeholder = /* scan `list` for node->type == NodeType::Load */;
// One call into the recon side handles reset → setupResources →
// setAsmList → setPlaceholderAsm → stepPrefetch → stepOptPost →
// stepUnsyncCervino → stepProject, plus assembler-text pretty-print
// and exception wrapping.  Resumes at stepPrefetch because input is
// already post-stepOptPre per IF-308.
awgCompiler.stepInnerCompileFromAsmList(source, std::move(list), placeholder);
// ... then back-end stages via stepAssembleOpcodes / stepCheckLimits
// / writeToStream as in the normal compile path.
```

### Verification

T6.1 commit must clear:

- `diff_test_fast` 1612/1612 (byte-equality invariant — the
  factor/setter additions are wrong if any test changes output).
- `test_seqcc_diff` 46/46.
- `test_seqcc_ab` 15/15.
- `test_seqcc_smoke` 4/4 + `test_seqdump` 16/16 + `test_seqcc_to`
  4/4.

T6.2 additionally requires `tests/tools/test_seqcc_from.py` to pass
strict byte-equal ELF round-trip (compile to `.seqasm` via
`--to=asm`, then resume from `.seqasm` via `--from=asm`, then
compare the resulting ELF to the original direct-compile ELF).

### Risks

- **State leakage**: `setAsmList` / `setPlaceholderAsm` are write-
  any-time entry points.  Calling them outside the
  `reset() → setupResources() → set*()  → stepOptPre()` sequence
  is undefined; document this in the doc comment.
- **Layout invariants**: adding new public methods does not change
  binary layout of existing members; the two new setters live in
  the existing public method region (started at `compiler.hpp:667`
  by T5b.4).  No member-data additions in this IF.
- **Resource construction divergence**: if a future recon change
  modifies `stepToSeqCAst`'s resource-construction block without
  also moving the change into `setupResources()`, the T6 resume
  path would silently see different resources from a normal
  compile.  Mitigation: `stepToSeqCAst` calls `setupResources()`
  directly, so there is only one resource-construction site to
  maintain.

### TODO references

- TODO.md Phase T → T6 (re-scoped per `seqcc_from_design.md`).
- Closes IF-304 in full at T6.1 (the `Compiler` accessor unlocks
  literal short-circuit on `--to=lower` / `--to=asm`).
- Prerequisite for T10a (retirement of `compileSeqcWithIR` —
  no change in scope from IF-306).

## IF-308  seqcc: `--to=asm` dumps post-pipeline asm, not the binary's natural round-trip cut

**Status:** partially fixed (Q3-demoted; round-trip resume deferred
indefinitely, see "Resolution" below)
**Severity:** likely-bug (design)
**Discovered:** during T6.2 implementation planning (post-T6.1).
**Amended:** T6.2 wrap-up.

### Observation

The current `--to=asm` implementation in `tools/seqcc/src/dump.cpp`
(`renderStagePrimary("asm")`) returns `sinks.assemblerText`, which
is populated by `AWGCompilerImpl::stepInnerCompile` from
`compileString_asmList_`.  That `AsmList` is the return value of
`Compiler::compile()` — i.e. the **post-full-pipeline** AsmList,
after `stepOptPre` + `stepPrefetch` + `stepOptPost` +
`stepUnsyncCervino` + `stepProject` have all run.

By contrast, the binary's own serialize/deserialize round-trip
debug path (`Compiler::stepOptPre`, recon
`reconstructed/src/codegen/compiler.cpp:598-602`, binary
`@0x1209a1`) round-trips the AsmList at a very specific point:
**after `optimizePreWaveform` runs**, but **before**
`stepPrefetch` and `stepOptPost`.  Quoting the binary's own code:

```cpp
if (config_->serializeRoundTrip == 1) {
    auto serialized = asmList_.serialize();
    asmList_.deserialize(serialized);
}
```

This sits inside `stepOptPre` immediately after
`asmList_ = compileOptimizer_->optimizePreWaveform(asmList_)` and
immediately before the `WavetableIR` construction that
`stepPrefetch` consumes.  That is the **only** point in the
binary's own pipeline where the AsmList round-trips through its
text DSL.

### Why the mismatch matters

For `--from=asm` (planned in T6.2) to produce byte-equal ELFs, the
dumped `.seqasm` must be a faithful capture of `asmList_` at a
point that can be safely re-entered.  The binary's choice of
"after `optimizePreWaveform`, before prefetch" is correct because:

1. The load placeholder anchor (the unique `NodeType::Load` entry
   that `runPrefetcher` expands) is still present in `asmList_` at
   that point — `stepPrefetch` is the consumer that rewrites it.
2. `compileWavetableIR_` has not yet been populated, so there is
   no second IR that the dump would need to bundle.
3. `compileOptimizer_` state past this point is per-instruction
   register allocation that the post-passes reset per-call, so
   reconstructing a fresh `AsmOptimize` on the `--from=asm` side
   is sufficient.

The current `--to=asm` output captures `asmList_` *after* all
those transformations have already happened.  Specifically:

- The load placeholder has been replaced with prefetched load
  sequences (one `PlainLoad` per cached waveform).
- `compileWavetableIR_` is populated and would need to be
  re-emitted as a sidecar for any meaningful resume.
- Post-prefetch register allocation has been applied.

Resuming from such an asm would not faithfully reproduce the
binary's behaviour — the prefetch decisions are baked in, and any
upstream change (config flag, device retune) would silently apply
to the second compile but not the first.

### Original T6 design assumption

`reconstructed/notes/seqcc_from_design.md` §2.2 ("`--from=asm`")
implicitly assumed the dump point was pre-prefetch:

> Resume at `stepOptPre` (if the input is pre-prefetch) **or**
> `stepOptPost` (if the input already contains prefetch results)
> [...] Whether `--from=asm` resumes at `stepOptPre` or
> `stepOptPost` depends on the *content* of the input file.

The design note correctly identified the two possible resume
points but did not surface that the existing `--to=asm`
implementation in `dump.cpp` always produces the second form
(post-prefetch), which closes off the cleaner resume path.

### Fix (planned for T6.2)

Refactor `--to=asm` to dump `asmList_` at the binary's natural
cut point — i.e. immediately after `stepOptPre` and before
`stepPrefetch`.  This makes `--from=asm` a clean
`setAsmList(deserialised) → setPlaceholderAsm(loadPlaceholder) →
stepPrefetch` resume, mirroring the binary's own round-trip
exactly.

Driver-side mechanics:

1. Drive the front end stage-by-stage via the IF-307
   `AWGCompiler::compiler()` accessor instead of going through
   `AWGCompiler::stepInnerCompile` (which would force the full
   `Compiler::compile()` sequence).  Call:
   `compiler().reset() → setupResources() → stepParse → stepToSeqCAst
   → stepLower → stepBuildAsmPreamble → stepOptPre`, then dump
   `asmList_.serialize()` as `--to=asm` output.
2. The placeholder is identifiable in the deserialised list by
   `node && node->type == NodeType::Load` (verified
   unique pre-prefetch; only producer is
   `AsmCommands::asmLoadPlaceholder()`).  No sidecar required.
3. For `--from=asm`: deserialise, scan for the unique
   `NodeType::Load` entry, then drive the back end via
   `setAsmList → setPlaceholderAsm → stepPrefetch → stepOptPost →
   stepUnsyncCervino → stepProject` plus the
   `AWGCompilerImpl::stepAssembleOpcodes` + `stepCheckLimits` +
   `writeToStream` back end.

This means the driver needs to call inner `Compiler` step methods
*before* the back end as well — which the T6.1 `compiler()`
accessor already enables.  However the outer `AWGCompilerImpl`
state (`sourceText_`, `compileMessages_`, `wavetableIR_`,
`assemblerText_`, `compileString_asmList_`) must still be
populated for `stepAssembleOpcodes` / `stepCheckLimits` /
`writeToStream` to function.  Resolution: the driver assembles
that state from the values it already has on hand (the inner
`Compiler::stepProject()` return, the assembler-text re-pretty-
printed from the post-pipeline AsmList) — no new
`stepInnerCompileFromAsm` recon method needed, only the existing
T6.1 surface.

### Cross-references

- Original `--to=asm` implementation:
  `tools/seqcc/src/dump.cpp::renderStagePrimary("asm")` and
  `tools/seqcc/src/driver.cpp:318` (where
  `sinks.assemblerText` is captured from
  `compiler.assemblerText()`).
- Binary round-trip debug flag:
  `reconstructed/src/codegen/compiler.cpp:598-602`.
- T6 design note: `reconstructed/notes/seqcc_from_design.md`
  §2.2.
- Sanctioned T6.1 recon surface: IF-307.

### Severity rationale

Marked likely-bug (design) because:

- The current `--to=asm` output is correct *as a debug dump* —
  the user can read the post-pipeline assembly listing — but
  unsuitable as `--from=asm` input.
- T6's central deliverable (round-trip-capable `--from=asm`)
  requires the dump-point fix.  Without it, T6.2 either ships a
  test that asserts round-trip on broken inputs (passes by
  accident or asserts non-equality) or skips the round-trip
  contract entirely (defeats the design goal).
- No binary mismatch — the binary's own `--to=asm` equivalent
  (the debug `serializeRoundTrip` flag) dumps at the correct
  point.  This is purely a tool-side design correction.

### TODO references

- TODO.md Phase T → T6.2 (refactor `--to=asm` dump point as the
  primary T6.2 work).
- Fix lands as part of T6.2.  Status flips "proposed" → "fixed"
  on T6.3 wrap-up.

### Resolution (T6.2 wrap-up)

The partial fix landed as the new `--to=asm-pre` stage rather
than as an overhaul of `--to=asm`.  `--to=asm` continues to dump
the post-pipeline assembly listing (now correctly labelled as a
*debug* artifact); `--to=asm-pre` is the new front-end-only path
that captures `asmList_.serialize()` at the binary's natural
round-trip cut (post-`stepOptPre`, pre-`stepPrefetch`) via the
IF-307 `AWGCompiler::compiler()` accessor.

**The round-trip `--from=asm` resume path was demoted to Q3 and
will not be pursued.**  During T6.2 implementation we discovered
that reconstructing the post-`stepOptPre` state cleanly requires
transporting more than just the AsmList:

1. **AsmList** — captured by `--to=asm-pre`. ✓
2. **WavetableIR** — `compileWavetableIR_` is constructed inside
   `stepOptPre` step 13c and consumed by `stepPrefetch`; would
   require a `<out>.wavetableir.json` sidecar plus matching
   `WavetableIR::toJson`/`fromJson` plus a recon-side
   `Compiler::setWavetableIR` setter.
3. **`Compiler::ast_`** — the next would-be sidecar.  `Prefetch`'s
   constructor takes `ast_` as `root`, and
   `Prefetch::optimizeSync(root_)` dereferences a null `Node` at
   offset 0xB8 (`Node::next`) when `ast_` is absent.  Would
   require an AST sidecar plus the existing `Node::fromJson`
   path plus a `Compiler::setAst` setter.
4. **Possibly more** — each new sidecar revealed the next during
   debugging; no clean stopping point was apparent without a
   full audit of every field `stepPrefetch` and downstream
   passes consume.

Each sidecar would also require a new recon-side setter, which
violates the "minimum-footprint" rule for sanctioned recon edits
(IF-307 only added one such accessor; widening it that far would
amount to a substantial recon-side ABI expansion).

The user chose Q3 (drop the round-trip goal) as the clean
stopping point.  `--to=asm-pre` remains as a one-way diagnostic
dump for inspecting the binary's natural cut point, which is
useful in its own right for debugging asm-list serialisation
(and indeed drove the discovery of IF-309/310/311 below).

What landed in the working tree and stayed:

- `--to=asm-pre` driver path + `IRSinks::preprefetchAsmText` +
  dump renderer (`tools/seqcc/src/{driver,dump,stage}.cpp`).
- IF-307 extensions to `AWGCompiler` (the existing
  `stepInnerCompileFromAsmList` overload + `setAsmList` /
  `setPlaceholderAsm` setters) — now dead-code with no driver
  caller, but the cost of reverting the recon commit was
  judged not worth the churn.

What was reverted from the T6.2 in-flight changes:

- `--from=asm` CLI option + `-x LANG` option + reconciliation
  logic + `validateFromToCombination` + `fromStageNames`.
- `wavetableJsonForResume` parameter on `SeqcDriver::compile()`.
- `IRSinks::preprefetchWavetableJson` sidecar field.
- `Compiler::setWavetableIRFromJson` + `Compiler::wavetableIR()`
  accessor (added during the sidecar push, then removed).
- 4th `wavetableJson` parameter on
  `AWGCompiler::stepInnerCompileFromAsmList` (reverted to 3-arg
  form to match the in-binary signature).
- Sidecar write / read blocks in `tools/seqcc/src/compile.cpp`.

Three GDB-/objdump-verified binary-fidelity fixes (IF-309/310/311
below) were *kept* — they were discovered during the round-trip
debugging but are independently correct regardless of whether
round-trip resume is ever revived.

Status: **partially fixed** rather than "fixed" because the
original design goal ("byte-equal round-trip resume from
`.seqasm`") is not achieved; only the one-way dump-point
correction landed.

---

## IF-309  Bison grammar: `placeholder_line` keyword + line-tail capture vs `placeholder` + post-`#` JSON

**Status:** fixed (working tree, gated by
`ZHINST_RECON_ASMLIST_KEYWORD_FIX`)
**Severity:** likely-bug (binary mismatch)
**Discovered:** during T6.2 round-trip debugging when the
`--to=asm-pre` dump showed `placeholder_line {…json…}` instead of
the binary's `placeholder # {…json…}`.

### Observation

The reconstructed `asm_lexer.l.in` / `asm_parser.y.in` defined a
single token `placeholder_line` whose semantic value captured the
entire rest of the line as a free-form string.  GDB tracing of
the original binary's lexer/parser on the same input showed two
distinct tokens: a bare `placeholder` keyword followed by a
standard `# <json>` comment, with the JSON parsed by the existing
`#`-comment lex rule that writes to `AsmExpression::comment`
(offset +0x80, populated by `AsmParserContext::addNode`).

### Why it matters

`AsmList::parseStringToAsmList` Case D
(`reconstructed/src/asm/asm_list.cpp:555-579`) reads
`expr->noOpt()` (alias for `hasComment`, offset +0x98) and feeds
`expr->comment` (offset +0x80) through `boost::json::parse` →
`Node::fromJson`.  With the reconstructed `placeholder_line`
token, the JSON ended up in a *different* field
(`AsmExpression::nopComment` at +0x20, the `//` line-comment
slot, written by `assembleStringToExpressionsVec`) and Case D
silently saw an empty `comment` — every placeholder round-trip
lost its node payload.

### Fix

`reconstructed/src/asm/asm_lexer.l.in`:

- Replace single `placeholder_line` rule + line-tail capture with
  a bare `placeholder` keyword token.

`reconstructed/src/asm/asm_parser.y.in`:

- Replace the `placeholder placeholder_line` production with
  `placeholder` (which inherits the existing
  `instruction_with_optional_comment` machinery that routes the
  trailing `#` JSON into `addNode()` → `comment` / `hasComment`).

`reconstructed/CMakeLists.txt`:

- Add `configure_file()` substitution variables for the two
  templated `.in` sources so the keyword swap can be ifdef-gated
  via `ZHINST_RECON_ASMLIST_KEYWORD_FIX` (ON by default).
- Add `target_compile_definitions(... PUBLIC
  ZHINST_RECON_ASMLIST_KEYWORD_FIX=$<BOOL:...>)` so dependent TUs
  see the same gate.

### Verification

- `--to=asm-pre` now produces text that matches the binary's
  `serializeRoundTrip` debug-flag output byte-for-byte (verified
  on `wave w = ones(64); playWave(w);`).
- Full diff suite 1612/1612 passing — no regression with the
  flag ON.
- IF-310 and IF-311 below are the addNode and
  `assembleStringToExpressionsVec` corrections that go with this
  one; the three together restore complete `placeholder # {json}`
  round-trip fidelity.

### Cross-references

- IF-310 — `addNode()` `comment`/`hasComment` writeback.
- IF-311 — `assembleStringToExpressionsVec()` `nopComment` /
  `noOptOverride_` fields.
- AsmExpression layout: `nopComment` @ +0x20 (`//` line comment),
  `comment` @ +0x80 (`#` JSON), `hasComment` @ +0x98 (alias
  `noOpt()`), `noOptOverride_` @ +0xA0.

---

## IF-310  `AsmParserContext::addNode()`: missing `comment` / `hasComment` writeback

**Status:** fixed (working tree, gated by
`ZHINST_RECON_ASMLIST_KEYWORD_FIX`)
**Severity:** likely-bug (binary mismatch)
**Discovered:** during IF-309 debugging.

### Observation

GDB-verified at `_seqc_compiler.so` `+0x28c243-0x28c297`: the
binary's `AsmParserContext::addNode` copies the trailing `#`
comment string into `AsmExpression::comment` (offset +0x80) and
sets `AsmExpression::hasComment = true` (offset +0x98) so that
the downstream `parseStringToAsmList` Case D recognises this
entry as a node-bearing placeholder.

The reconstruction in
`reconstructed/src/asm/asm_parser_context.cpp::addNode()` was
missing both writes — the `comment` field stayed empty and
`hasComment` stayed false even when a `#`-JSON suffix was
present, so Case D never triggered.

### Fix

```cpp
node->comment = std::move(comment);
node->hasComment = true;
```

added inside `addNode()` immediately after the existing field
population, gated by `ZHINST_RECON_ASMLIST_KEYWORD_FIX`.

### Verification

- Direct address verification via GDB breakpoint at the binary's
  `addNode` entry confirmed the two writes to +0x80 / +0x98.
- Combined with IF-309 + IF-311, the `--to=asm-pre` output
  round-trips through `parseStringToAsmList` Case D and recovers
  the node payload.

---

## IF-311  `assembleStringToExpressionsVec()`: wrong `AsmExpression` fields written for `//`-style line comments

**Status:** fixed (working tree, gated by
`ZHINST_RECON_ASMLIST_KEYWORD_FIX`)
**Severity:** likely-bug (binary mismatch)
**Discovered:** during IF-309/310 debugging.

### Observation

objdump-verified at `_seqc_compiler.so` `+0x286e40`: the binary's
`assembleStringToExpressionsVec` (the inner helper called by
`AsmList::serialize`) writes the `//`-style line comment into
`AsmExpression::nopComment` (offset +0x20, via `r13 += 0x20`) and
sets `AsmExpression::noOptOverride_` (offset +0xA0, via `mov
%al, 0xa0(%rcx)`).

The reconstruction in
`reconstructed/src/codegen/awg_assembler_impl_pipeline.cpp::
assembleStringToExpressionsVec()` was writing to `ast->comment`
(the `#` JSON slot at +0x80) and `ast->noOpt()` (the
`hasComment` alias at +0x98) — the wrong two fields, both
belonging to the placeholder-node payload path.  This corrupted
the round-trip in the opposite direction: serialised text
emitted `//` content into the JSON-bearing field, then
`parseStringToAsmList` Case D saw a non-empty `comment` on
non-placeholder entries and tried to JSON-parse the `//` text.

### Fix

Re-route the two writes in
`assembleStringToExpressionsVec()` to the correct fields:

```cpp
ast->nopComment = …;        // was ast->comment
ast->noOptOverride_ = true; // was ast->noOpt() = true
```

gated by `ZHINST_RECON_ASMLIST_KEYWORD_FIX`.

### Verification

- Field offsets verified via objdump on the original binary
  (`r13 += 0x20` for `nopComment`; `mov %al, 0xa0(%rcx)` for
  `noOptOverride_`).
- With IF-309 + IF-310 + IF-311 all applied, the full diff suite
  (1612/1612) passes with no regression.

### Cross-references

- IF-309 — lexer/parser keyword swap.
- IF-310 — `addNode()` comment writeback.
- AsmExpression layout: see IF-309.

---

## IF-312  `kSuserUserRegBase = 0x5F` is misnamed: 0x5F is the oscillator-phase-reset address, not a user-register base

- **Source**: B.3 follow-up audit of `special_registers.md` ld/st
  pairs against `reconstructed/src/asm/asm_commands*.cpp` and the
  `kSuser*` constants in `reconstructed/include/zhinst/core/types.hpp`.
- **Status**: fixed (Phase D7-C.1 F9 — renamed `kSuserUserRegBase`
  → `kAddrOscPhaseReset` and moved to the `kAddr*` block in
  `types.hpp`).
- **Severity**: cosmetic (misleading name)

### Observation

`reconstructed/include/zhinst/core/types.hpp:160` declares

```cpp
constexpr uint32_t kSuserUserRegBase   = 0x5F;  //!< User-register base (`getUserReg` / `setUserReg`).
```

but `0x5F` is **not** the user-register base.

- `setUserReg` / `getUserReg` access the general user-register
  space starting at address `0x00`
  (`reconstructed/src/runtime/custom_functions_registers.cpp:399,
  401, 456, 549`), passed through `luser(reg, idx)` /
  `suser(reg, idx)` with `idx` in `[0, 0x3FF]`.
- `0x5F` is emitted in `resetOscPhase` as a `st(reg, 0x5f)` /
  `st(R0, 0x5f)` pulse pair on the UHFQA path
  (`custom_functions_wait.cpp:918, 941, 945`).

`special_registers.md` §8 already documents `0x5F` correctly as
*Oscillator phase reset (pulse)*.  The discrepancy is purely in
the constant name in `types.hpp`.

### Suggested fix

Rename `kSuserUserRegBase` → `kAddrOscPhaseReset` (or similar,
matching the `kAddr*` convention for `ld`/`st`-direct addresses
rather than `kSuser*` which is reserved for `suser`-addressed
registers).  Update the `\brief` to "Oscillator phase reset
(pulse, written via `st`/`st 0`)."

Grep callers before renaming — none currently consume the constant,
so this is a low-risk rename.

### Cross-references

- `special_registers.md` §8 — correct documentation of `0x5F`.
- IF-228 — earlier suser-address audit.

## IF-313  `ErrorMessageT` cluster A: slots 12/13/15 three-cycle rotation

**Severity:** medium (silent mis-labelling; binary semantics preserved).
**Status:** fixed (Phase D7-C.1bis sub-batches 1-3, commit 8a83a2e).

**Discovered:** during D7-C.1bis enum/template realignment audit.

### Problem

In the pre-D7-C.1bis reconstructed `ErrorMessageT` enum, the names at
slots 12, 13, 15 did not match the string templates the binary's
`ErrorMessages::messages` initializer assigns to those slots.  The
mis-naming was a value-preserving three-cycle:

| slot | pre-D7-C.1bis name      | template text (binary)              | post-rotation name   |
|-----:|--------------------------|--------------------------------------|----------------------|
|   12 | `ArrayIndexOutOfRange`  | "program is too large to fit..."     | `ProgramTooLarge`    |
|   13 | `ProgramTooLarge`       | "array operand must be a single wave"| `ArraysOnlyWave`     |
|   15 | `ArraysOnlyWave`        | "array index out of range"           | `ArrayIndexOutOfRange` |

### Resolution

Rotated the three names in `error_messages.hpp` so each enumerator now
sits at the slot whose template it actually describes.  Symbolic
call-site grep flagged 2 hits in `awg_compiler.cpp` and 2 in
`seqc_ast_eval_control.cpp`; context inspection confirmed each call's
intent matches the post-rotation name (e.g. `control.cpp:548,619` were
under comments referencing slot indices `0xd`/`0xf` — the rotated
names match those slots).

### Cross-references

- `error_message_audit.md` cluster A.
- IF-315 — same pattern at slots 30/31/32 (`ModifyConst` cluster), the
  one where stale resources.cpp prose was discovered.

## IF-314  `ErrorMessageT` cluster C: slots 39/40/42 three-cycle rotation

**Severity:** medium.
**Status:** fixed (Phase D7-C.1bis sub-batches 1-3, commit 8a83a2e).

**Discovered:** during D7-C.1bis enum/template realignment audit;
originally surfaced as `unknowns.md#92`'s sibling concern about slot 32
mis-naming (see IF-315).

### Problem

Value-preserving three-cycle in the pre-D7-C.1bis enum:

| slot | pre-D7-C.1bis name        | template text                              | post-rotation name      |
|-----:|----------------------------|---------------------------------------------|-------------------------|
|   39 | `CantDivConstByWave`      | "%1% expects a var or const argument"       | `ExpectsVarOrConst`     |
|   40 | `ExpectsVarOrConst`       | "invalid device number..."                  | `InvalidDeviceNr`       |
|   42 | `InvalidDeviceNr`         | "can't divide a const by a wave"            | `CantDivConstByWave`    |

### Resolution

Rotated names; verified the 16 symbolic call sites for `ExpectsVarOrConst`
all sit in `expects-a-var-or-const` argument-validation paths
(`seqc_ast_eval_control.cpp`, `seqc_ast_eval_arithmetic.cpp`).  Single
call site for `CantDivConstByWave` is in `seqc_ast_eval_arithmetic.cpp`
inside the divide-by-wave operand branch.

### Cross-references

- `error_message_audit.md` cluster C.

## IF-315  `ErrorMessageT` cluster B: slots 30/31/32 three-cycle rotation (closes `resources.cpp` open comment)

**Severity:** medium (had triggered an explicit `static_cast<ErrorMessageT>(32)`
workaround in `resources.cpp` with a "halt!" comment from a prior agent).
**Status:** fixed (Phase D7-C.1bis sub-batches 1-3 commit 8a83a2e for the
enum rotation; sub-batch 5 commit 4b18daa for the call-site rename;
sub-batch 6 for the stale prose at `resources.cpp:1269-1276,1549-1553,1585-1587`).

**Discovered:** during D7-C.1bis enum/template realignment audit;
the divergence at slot 32 had been independently flagged by a prior
agent in `resources.cpp:1269-1276` ("Either the slot name in the
reconstructed enum is wrong, or the message file just happens to
reuse this slot. To stay faithful to the binary we cast to slot 32
directly with a cast.").

### Problem

Value-preserving three-cycle in the pre-D7-C.1bis enum:

| slot | pre-D7-C.1bis name           | template text                                  | post-rotation name        |
|-----:|-------------------------------|-------------------------------------------------|---------------------------|
|   30 | `ModifyConst`                | "could not compress sections in output file..." | `CompressError`           |
|   31 | `CompressError`              | "conditional expression expects a var..."       | `ConditionalNeedVarConst` |
|   32 | `ConditionalNeedVarConst`    | "tried to modify const value"                   | `ModifyConst`             |

### Resolution

Rotated names.  `resources.cpp:1589` now reads `errMsg[ModifyConst]`
instead of `errMsg[static_cast<ErrorMessageT>(32)]`.  The three stale
prose blocks at `:1269-1276`, `:1549-1553`, `:1585-1587` that
explained the mis-naming and the workaround were rewritten to reflect
the resolved state (they now reference IF-315 instead of the
ambiguous `unknowns.md item #92`).

### Cross-references

- `error_message_audit.md` cluster B.
- `resources.cpp:1269-1276,1549-1553,1585-1587` — stale prose
  rewritten in same sub-batch.

## IF-316  `ErrorMessageT` cluster D: slots 250/251/252 three-cycle rotation

**Severity:** medium.
**Status:** fixed (Phase D7-C.1bis sub-batches 1-3, commit 8a83a2e).

**Discovered:** during D7-C.1bis enum/template realignment audit.

### Problem

Value-preserving three-cycle in the pre-D7-C.1bis enum:

| slot | pre-D7-C.1bis name           | template text                              | post-rotation name      |
|-----:|-------------------------------|---------------------------------------------|-------------------------|
|  250 | `WaveNameInUse`              | "waveform index exceeds wavetable size"     | `WaveIndexExceedsTable` |
|  251 | `WaveIndexExceedsTable`      | "invalid waveform name '%1%'"               | `InvalidWaveformName`   |
|  252 | `InvalidWaveformName`        | "waveform name '%1%' is already in use"     | `WaveNameInUse`         |

### Resolution

Rotated names; one symbolic call site (`dio.cpp:376`) for what is now
`InvalidWaveformName` (slot 251 / 0xFB) was inspected — the surrounding
code does invalid-waveform-name validation, so post-rotation name fits.
The site for `WaveIndexExceedsTable` is in `wave_index_tracker.cpp` next
to the `WaveIndexUsed` sibling slot, both wavetable-index validators.

### Cross-references

- `error_message_audit.md` cluster D.

## IF-323  `ErrorMessageT::UnknownFunction` used for an ALU opcode-dispatch failure

**Severity:** suspicious (binary semantics preserved; the question is
whether the binary itself picked the right slot).
**Status:** **dismissed** — resolved as hypothesis 2 (closest-fit template).

**Discovered:** during Phase D7-C.1bis sub-batch 5 context-eyeball pass.
**Resolved:** during D7-C.1.i targeted investigation.

### Resolution

Investigation under C.1.i confirmed:

1. **The throw branch is unreachable through any current caller.**
   `AsmCommands::alui` (the only function with the `else throw` path)
   is called only with `ADDI`, `ANDI`, `ORI`, `XNORI` (grep of
   `reconstructed/src/asm/asm_commands.cpp`).  `ADDI` is handled by
   Case 1 / Case 2 earlier in the body; `ANDI` / `ORI` / `XNORI` are
   the three explicit mappings.  No caller ever passes another opcode.

2. **No better-fit template exists in the binary.**  `strings
   _seqc_compiler.so` finds no "no register-form equivalent",
   "unsupported opcode", "cannot lower immediate", or similar
   templates that would describe the actual failure mode.  The
   binary's choice of `UnknownFunction` is the closest-available
   message for "we don't know how to assemble this command".

3. **Diff-tests stay clean.**  Since the path is unreachable from
   any documented SeqC input, the misleading English of the rendered
   message is academic.

The branch is defensive code against future ALU-immediate opcodes
that the lowering pass doesn't recognise.  Leaving the recon as-is
(faithful to the binary) is correct.

### Original observation (preserved for history)

### Observation

`asm/asm_commands.cpp:379-381` falls through to an exception when an
immediate-form ALU command does not have a register-form equivalent:

```cpp
if (cmd == Assembler::ANDI)       regCmd = Assembler::ANDR;
else if (cmd == Assembler::ORI)   regCmd = Assembler::ORR;
else if (cmd == Assembler::XNORI) regCmd = Assembler::XNORR;
else
    throw ResourcesException(
        ErrorMessages::format(UnknownFunction,
                              Assembler::commandToString(cmd).c_str()));
```

`UnknownFunction` is slot 0xD8 (216), template `"calling unknown
function '%1%'"`.  The argument substituted into `%1%` is an
**Assembler::Command** name (e.g. `"ADDI"`, `"SUBI"`), not a SeqC
function name.  The English of the resulting message ("calling unknown
function 'ADDI'") is misleading for the actual failure (an unsupported
ALU opcode reached the immediate-→register lowering pass).

### Hypotheses

1. Binary chose slot 216 deliberately and we should leave it (the
   reconstruction is faithful — diff_test_fast passes byte-identical).
2. Binary chose 216 because no more specific "no register-form
   equivalent for opcode X" template exists, and "unknown function" is
   the closest semantic fit when the user-visible failure is "we don't
   know how to assemble this command".
3. Binary has a slot collision / template misalignment we haven't
   detected — i.e. slot 216 *might* historically have meant "unknown
   opcode" and the template string drifted to "unknown function" at a
   later point.

### Investigation TODO

- GDB-trace the binary on an input that triggers this path (e.g.
  hand-crafted `.asm` source using an ALU immediate-form opcode without
  a register-form equivalent — verify that even exists in the user-
  reachable surface area).  Confirm the exact slot and template
  rendered.
- Search the binary's templates for any string that better matches
  "no register-form equivalent for this opcode".  If none exists, this
  is hypothesis 2 and dismissible.
- Audit whether this path is reachable from any documented SeqC input
  at all.  If unreachable through normal SeqC compilation, the
  user-visible message text is academic.

### Cross-references

- Phase D7-C.1bis sub-batch 5 (commit 4b18daa) made this site use the
  named enum; the prior bare-int form would have hidden the semantic
  mismatch.
- `error_message_audit.md` slot 216 entry.

## IF-324  `SampleRateConstOnly` template ('%1%' only) is called with two arguments

**Severity:** suspicious — possible pre-existing recon bug, possibly
also present in the binary, possibly handled gracefully by
`boost::format`.
**Status:** **dismissed** — false alarm; template has two placeholders,
not one.

**Discovered:** during Phase D7-C.1bis sub-batch 5 context-eyeball pass.
**Resolved:** during D7-C.1.i targeted investigation.

### Resolution

The IF-324 observation quoted the template at slot 159 as:

> `"%1% sample rate can only be described with a const"`

This quote was incomplete.  The actual template in
`reconstructed/src/core/error_messages.cpp:412` is:

```
"%1% sample rate can only be described with a const, not with a %2%"
```

— **two placeholders**, matching the 2-arg call site
(`name`, `str(val.varType_)`).  The binary's template is identical
(verified via `strings _seqc_compiler.so`):

```
$ strings _seqc_compiler.so | grep -F "sample rate can only"
%1% sample rate can only be described with a const, not with a %2%
```

End-to-end verification: compiling `var myRate; myRate = 1;
playZero(64, myRate);` on HDAWG8 throws the expected message:

```
Compiler Error (line: 4): playZero sample rate can only be
described with a const, not with a var
```

Both `%1% = "playZero"` and `%2% = "var"` substitute correctly.
The boost::format default-strict mode is satisfied (2 placeholders,
2 args).

The IF was filed based on a truncated quote from
`error_message_audit.md`; no actual bug.  The broader 628-site
arg-count audit is therefore lower priority — but still worth
running at some point to catch genuine mismatches if any exist.

### Original observation (preserved for history)

### Observation

`runtime/custom_functions.cpp:1172-1174` (function `getPlayRate`,
@0x163730):

```cpp
// Not a const/cvar: throw with error 0x9f
throw CustomFunctionsException(
    ErrorMessages::format(SampleRateConstOnly, name, str(val.varType_)));
```

The template at slot 159 (0x9F) is:

```
"%1% sample rate can only be described with a const"
```

— **a single placeholder**, but the call site passes **two arguments**
(`name`, `str(val.varType_)`).

### Why this matters

`boost::format` behaviour with excess arguments depends on stream
flags.  In strict mode (the default when `boost::io::all_error_bits`
is set) it throws `boost::io::too_many_args_bit`.  In permissive mode
(if any `*_bit` is cleared) it silently ignores the excess.

If the binary's `ErrorMessages::format` runs in strict mode, this
site **would always throw a boost::format exception instead of the
intended `CustomFunctionsException`** — i.e. the user gets the wrong
exception type and the wrong error text every time the
"playRate is not a const/cvar" branch is hit.

### Hypotheses

1. The reconstructed `ErrorMessages::format` happens to run in
   permissive mode and silently drops the extra arg; the test suite
   covers a path that hits this site and happens to render correctly
   on both binary and recon, so diff_test_fast doesn't flag it.
2. The site is unreachable in the current test corpus (no playRate
   test passes a non-const/non-cvar argument) and the bug has never
   manifested in either binary or recon.
3. The binary's actual call passes one arg, and our reconstruction of
   `getPlayRate` has an extra arg that doesn't match the binary.  In
   this case the recon is the bug and the difftest only stays clean
   because the path isn't exercised.

### Investigation TODO

- Check `ErrorMessages::format` (`reconstructed/src/core/error_messages.cpp`)
  for its `boost::format` configuration — strict or permissive?
- GDB-trace `getPlayRate` at @0x163770 in the original binary with an
  input that hits the throw branch (e.g. `playWave(zeros(64),
  rate=myVar);` where `myVar` is a `var int`).  Read the actual arg
  count pushed.
- If the binary passes 1 arg and recon passes 2: fix the recon (drop
  `str(val.varType_)`).
- If the binary passes 2: keep the call but flag the template as
  underspecified — consider whether the template should be
  `"%1% sample rate can only be described with a const, %2% given"`
  (matching the shape of nearby `OnlyConstVarNegated` etc.).
- Audit other `ErrorMessages::format` call sites for the same
  arg-count-vs-placeholder-count mismatch.  Could be a broader pattern.

### Cross-references

- Phase D7-C.1bis sub-batch 5 (commit 4b18daa) made this site use the
  named enum; the prior bare-int form (`static_cast<ErrorMessageT>(0x9f)`)
  hid the arg-count mismatch behind a numeric literal.
- A grep for similar mismatches (templates with N placeholders called
  with M args, M != N) across all 164 newly-named sites would be a
  cheap follow-up audit and may surface more cases.

## IF-325  Inconsistent reconstruction comments around `0x4000000040004041` mask

**Severity:** cosmetic (documentation-only; binary semantics correct).
**Status:** partially-fixed by Phase D7-C.1 F5 (uses now reference the
named constant `kHirzelDevTypeMinus2Mask`; comments revised where the
literal was inlined).

**Discovered:** during Phase D7-C.1 F5 (cross-file hoist of the
bitmap to `core/types.hpp`).

### Observation

The bitmask `0x4000000040004041ULL` selects (devType - 2) bit
positions for the Hirzel-implemented assembler backend subset.  The
mask contains bits **{0, 6, 14, 30, 62}**, corresponding to devType
values **{2, 8, 16, 32, 64}** = **{HDAWG, SHFQA, SHFSG, SHFQC_SG,
SHFLI}** (five devices).

Several reconstruction comments document this incorrectly:

- `src/asm/asm_commands_impl.cpp:19` — correct: "Bits {0, 6, 14, 30,
  62} → device values {2, 8, 16, 32, 64} → Hirzel."
- `src/runtime/custom_functions_play.cpp:70` — wrong: "checks bits
  0,6,14,16,30,62" (claims bit 16 is set; bit 16 is *not* set) and
  "devType values: 2,3,8,16,18,32,64" (claims 3 and 18; neither is
  matched).
- `src/runtime/custom_functions_wait.cpp:114` — incomplete: "checks
  deviceType-2 for bits 0,6,30,62" (missing bit 14).
- `src/runtime/custom_functions_wait.cpp:340` — wrong: "Supported:
  deviceType in {2,3,8,16,18,32,64}" (claims 3 and 18; same error
  as play.cpp:70).
- `include/zhinst/core/types.hpp:13-16` block-header — correct:
  "Hirzel: HDAWG(2), SHFQA(8), SHFSG(16), SHFQC_SG(32), SHFLI(64)".

The audit report `magic_number_audit.md` §4 also propagated the wrong
set "(devType-2) ∈ {0,1,6,14,16,30,62} = {HDAWG, UHFQA, SHFQA, SHFSG,
SHFQC_SG, SHFLI}".

### Why this matters

Anyone reading `custom_functions_play.cpp` or `wait.cpp` to learn which
devices support `play`/`wait` would conclude UHFQA(4) is supported via
bit-1 (devType 3 = mythical), when in fact UHFQA goes through a
separate dispatch path entirely (the bitmask test fails for UHFQA, but
the function may still succeed via the Cervino impl).  Reading any one
of these files in isolation gives a wrong picture of the dispatch.

### Suggested fix

When F5 lands, all four sites should reference the named constant
(`kHirzelDevTypeMinus2Mask` in `core/types.hpp`) and the inline wrong-
list comments should be deleted in favour of a single correct comment
at the constant declaration.  Audit comment text is updated as part of
that commit; this IF tracks the residual unverified claim — there may
be other comments in the recon (away from the literal sites) that
quote one of the wrong device lists.

### Cross-references

- `notes/cervino_vs_hirzel.md` — authoritative device dispatch map.
- `core/types.hpp` `kDevHirzel = 0x1F2` — AwgDeviceType-bit mask
  (different representation: indexed by AwgDeviceType bit, not by
  (devType-2) bit-position).
- Magic-number audit §4 (file `notes/magic_number_audit.md`) — the
  audit propagated the wrong list as part of its consolidation table.

---

## IF-326 — `Assembler::Command` enum slot 0x4 mis-labelled "unused?" but is an active barrier opcode

**Severity:** medium (documentation accuracy + structural drift risk)
**Status:** fixed
**Found:** Phase D7-C.1 F2 (magic-number audit follow-through).
**Files:** `reconstructed/include/zhinst/asm/assembler.hpp:73`
(declaration); `reconstructed/src/asm/asm_list.cpp:374,387,416,420`;
`reconstructed/src/asm/asm_optimize_regalloc.cpp:537,576,599,613,665`;
`reconstructed/src/asm/asm_commands.cpp:883`.

### Symptom

The `Assembler::Command` enum declaration carried
`// 0x00000004 unused?` between `MESSAGE = 0x3` and `ERROR_MSG = 0x5`,
implying value `4` is an unknown / dead opcode slot.  In fact the
value is **actively used at 7 sites** across the assembler:

- `asm_list.cpp:416-420` builds an instruction with `cmd = 4` whenever
  the input expression carries a free-form comment but no code; the
  inline reconstruction comment correctly calls this a "NOP marker."
- `asm_optimize_regalloc.cpp` checks for `cmd == 4` in five places,
  always paired with `cmd == INVALID`, to treat the slot as a
  register-allocation barrier and to skip emission when the entry's
  `regSrc` field matches the barrier sentinel register.

So `4` is a deliberate, synthetic pseudo-op that:
- has no mnemonic in `getCmdMap()` (never round-trips from asm text),
- is emitted only by the C++ pipeline as a comment-bearing barrier,
- is dropped before final `.text` encoding when its `regSrc` matches
  the barrier sentinel.

### Fix

Renamed the enumerator to `COMMENT_NOP` with a doc brief explaining
the barrier role and the no-mnemonic / never-encoded contract.
Substituted all 7 bare-`4` sites (5 `static_cast<Assembler::Command>(4)`
in the optimiser, 1 declaration cast in `asm_list.cpp`, 1 comparison
in `asm_list.cpp`) with `Assembler::COMMENT_NOP`.  Also updated stale
reconstruction comments in `asm_optimize_regalloc.cpp` that referred
to the slot as `cmd=4` / `cmd4` to use the new name.

### Cross-references

- `assembler.hpp` `Assembler::Command` enum.
- `asm_list.cpp` Case C handler — only producer of `COMMENT_NOP`.
- `asm_optimize_regalloc.cpp` — only consumer; treats it as a barrier
  alongside `INVALID`.


## IF-327 — `Node()` default ctor passes `-1` for `numWaveSlots`

**Severity:** suspicious
**Status:** open

The default constructor `Node::Node()` in `reconstructed/src/ast/node.cpp:35-37`
delegates to `Node(NodeType{0}, 0, -1)`.  The third parameter of the simple
ctor is documented as `numWaveSlots` and used as `wavesPerDev(numWaveSlots)`
— i.e. it sizes a `std::vector<std::optional<std::string>>`.  Passing `-1`
converts to `(size_t)-1 = SIZE_MAX` and would crash with `std::bad_alloc`.

Either:

1. The default ctor is never actually called at runtime (dead code), or
2. The parameter name / interpretation of the third argument is wrong, or
3. The body order is wrong — the original binary may initialise `wavesPerDev`
   from a different source and never look at this parameter.

Discovered during F10.b magic-number cleanup; left as `-1` literal rather
than mapping to a named sentinel because no semantic interpretation
applies cleanly.

**Files**
- `reconstructed/src/ast/node.cpp:35-37` (default ctor)
- `reconstructed/src/ast/node.cpp:45-50` (simple ctor body)


## IF-328 — `wave_index_tracker.cpp` comments call the field `playIndex`, source says `waveIndex`

**Severity:** cosmetic
**Status:** fixed (comments updated)

`reconstructed/src/waveform/wave_index_tracker.cpp` had three stale `//`
reconstruction comments referring to `playIndex` at offset 0x6C, while the
field is actually `Waveform::waveIndex` (confirmed in
`waveform_front.hpp:142` and `waveform_ir.hpp:149`).  Comments rewritten
to `waveIndex` as part of F10.b.

**Files**
- `reconstructed/src/waveform/wave_index_tracker.cpp:139,144,158`


## IF-329 — `magic_number_audit.md` P1 mistargets `Value::which_`

**Severity:** cosmetic (documentation)
**Status:** fixed (P1 pivoted; this note records the discrepancy)

Audit §8 P1 reads "`VariantSlot` enum for `Variable::value.which_`
(Int/Bool/Double/String = 0..3 per `runtime/resources.cpp` comments).
7 switches in `ast/value.cpp` would convert."

Verification in Phase D7-C.2 found:

- There are **zero** `switch (which_)` statements in `reconstructed/src/`.
  The two `which_` references in `runtime/resources.cpp:1786` and
  `custom_functions.cpp:1155` are descriptive comments about the
  binary's jump table, not C++ switches.
- The 7 switches actually present in `reconstructed/src/ast/value.cpp`
  (lines 84, 95, 117, 154, 170, 188, 203) dispatch on
  `Immediate::index_`, **not** `Value::which_`.  These belong to the
  inner `std::variant<AddressImpl<uint>, int, std::string>` of
  `Immediate`, whose discriminator enum `ImmediateKind` already exists
  in `reconstructed/include/zhinst/ast/value.hpp:157` (A1 from
  `magic_numbers_proposal.md`, fully implemented).
- The two variants are unrelated: `Value::which_` is a
  `boost::variant<int,bool,double,string>` discriminator
  (0=Int/1=Bool/2=Double/3=String); `Immediate::index_` is a
  `std::variant<AddressImpl,int,string>` discriminator
  (0=Address/1=Integer/2=String).

P1 was pivoted to "`ImmediateKind` literal→named, 7 sites in
`ast/value.cpp`" — a genuine A1-residual cleanup using the existing
enum, not a new enum.  The original P1 intent (a `VariantSlot` enum
for `Value::which_`) has no consuming `switch` and was not added.

**Files**
- `reconstructed/notes/magic_number_audit.md:780` (P1 row)
- `reconstructed/src/ast/value.cpp:84,95,117,154,170,188,203`
- `reconstructed/include/zhinst/ast/value.hpp:157` (existing enum)


## IF-330 — `magic_number_audit.md` P8 frames a non-existent enum extension

**Severity:** cosmetic (documentation)
**Status:** fixed (P8 pivoted; this note records the discrepancy)

Audit §8 P8 reads "Extend `WaveformFile::Type` enum (or add a sibling)
for the `.bin / .bin16 / .wave / .wave16` extension ladder in
`codegen/awg_compiler.cpp:1196,1202,1245`."

Verification in Phase D7-C.2 found:

- All four extensions in question map to **only two** existing
  enumerators of `WaveformFile::Type` (`waveform.hpp:58`):
  - `.csv` / `.txt` → `Type::CSV` (= 0)
  - `.bin` / `.wave` → `Type::RAW` (= 1)
  - `.bin16` / `.wave16` → `Type::RAW` (= 1) (same enumerator,
    different binary-sample-width handling done at parse time)
- The three audit-cited call sites pass bare `Type(0)` / `Type(1)`,
  not raw extension-derived constants — they are literal-→-named
  conversions against the existing enum, not an enum extension.
- Extending `Type` would break two binary-faithful symbols:
  `WaveformFile::typeToStr` (0x2a3a90) and
  `WaveformFile::typeFromStr` (0x2a63c0).  Both have lazy-initialised
  static maps containing exactly the three known strings (`"csv"`,
  `"raw"`, `"gen"`); a fourth enumerator would throw `std::out_of_range`
  on round-trip.
- The "extension ladder" is in fact a `std::string` match on `ext`
  (see `awg_compiler.cpp:1196,1202,1245`), not an enum dispatch.
  The right shape for that is a string→enum mapping, which already
  exists implicitly in the if-else chain — no enum surface is required.

P8 was pivoted to "literal→named, 3 sites in `awg_compiler.cpp`" — a
trivial cleanup using the existing enum.  The audit's "extend the
enum" framing was not actioned.

**Files**
- `reconstructed/notes/magic_number_audit.md:787` (P8 row)
- `reconstructed/src/codegen/awg_compiler.cpp:1199,1244,1284`
- `reconstructed/include/zhinst/waveform/waveform.hpp:58` (existing enum)


## IF-317 — `0x29` cmd-set membership LUT in `asm_optimize*.cpp`

**Severity:** suspicious
**Status:** open

Four sites in two files test `(0x29 >> cmdPlus1) & 1` to check
membership in a 3-element set of `Assembler::Command` values:

| site | snippet |
|---|---|
| `reconstructed/src/asm/asm_optimize.cpp:533` | `if (cmdPlus1 <= 5 && ((0x29 >> cmdPlus1) & 1))` |
| `reconstructed/src/asm/asm_optimize_regalloc.cpp:39` | same |
| `reconstructed/src/asm/asm_optimize_regalloc.cpp:153` | same |
| `reconstructed/src/asm/asm_optimize_regalloc.cpp:493` | `bool isSkipCmd = (cmdPlus1 <= 5 && ((0x29 >> cmdPlus1) & 1));` |

`cmdPlus1 = static_cast<int>(cmd) + 1` (where `Assembler::INVALID =
0xFFFFFFFF`, so `cmdPlus1 == 0` for INVALID).  `0x29 = 0b101001`
selects bits 0, 3, 5 → cmds with `(cmd+1) ∈ {0, 3, 5}` →
`{INVALID, LABEL, COMMENT_NOP}` (per the comments at
`asm_optimize_regalloc.cpp:34,422,682`).

The semantic is "barrier opcodes that don't participate in
register-allocation flow" (INVALID = dead instruction, LABEL =
unresolved branch target, COMMENT_NOP = pseudo-barrier carrying
side comment; see IF-326).

**Proposed name:** `kRegallocBarrierCmdMask` (file-scope `constexpr
uint8_t` in `asm_optimize_regalloc.cpp`, with a brief explaining
the bit-positions in terms of `(static_cast<int>(cmd) + 1)`).
Defer naming until verified — the proposed name overlaps three
files, so the home should likely be a shared header (e.g.
`include/zhinst/asm/asm_optimize_internal.hpp`) rather than file
scope.

**GDB verification needed:** confirm the dispatch covers
`INVALID/LABEL/COMMENT_NOP` and no other opcodes (in particular,
verify nothing else has `(cmd+1) ∈ {0,3,5}` after the COMMENT_NOP
finding in IF-326).

**Files**
- `reconstructed/src/asm/asm_optimize.cpp:529-533`
- `reconstructed/src/asm/asm_optimize_regalloc.cpp:34-39, 151-153, 488-493`


## IF-318 — `case static_cast<NodeType>(8)` literal label in `prefetch.cpp`

**Severity:** cosmetic
**Status:** fixed

`reconstructed/src/codegen/prefetch.cpp:1970` had a switch-case
label `case static_cast<NodeType>(8):` despite `NodeType::Loop =
0x0008` being defined in `include/zhinst/ast/node.hpp:61`.
Verified Loop = 0x0008 against the enum and the surrounding
comment (`type=8 (Loop)`); substituted as part of D7-C.1.c.

The sibling site at `prefetch_print.cpp:60`
(`switch (static_cast<NodeType>(typeCode))`) is a legitimate
runtime cast and was left alone.

**Files**
- `reconstructed/src/codegen/prefetch.cpp:1970` (fixed)


## IF-319 — `DeviceTypeCode(33)` / `DeviceFamily(0x800u)` "unknown" sentinels

**Severity:** suspicious
**Status:** open

`device/device_unknown.cpp` and the `DeviceTypeImpl` default
construction in `device_type.cpp` construct sentinel values for
"unknown device":

- `DeviceTypeCode::Unknown = 0` is the documented sentinel.
- `DeviceFamily::Unknown = 0` is the documented sentinel.

However, two non-zero literals appear elsewhere in the device
subsystem and are claimed by the original audit (§5 D3) to be
out-of-range sentinels:

- `DeviceTypeCode(33)` — needs site re-confirmation; not found in
  current `device_type.cpp` or `device_unknown.cpp`.  May have been
  removed during reconstruction or was mis-classified.
- `DeviceFamily(0x800u)` — same.

**Action:** rescan the `device/` subsystem for these literals.
If they're gone, dismiss this IF; if present, GDB-confirm whether
they represent (a) "outside the valid range" markers vs (b)
a missing enumerator that should be added to the enum.

**Files** (claimed; needs verification)
- `reconstructed/src/device/device_type.cpp`
- `reconstructed/src/device/device_unknown.cpp`


## IF-320 — `getAwgDeviceTypeFromString` raw-integer returns (was "parseCpuType")

**Severity:** likely-bug
**Status:** fixed (C.1.j)

`reconstructed/src/codegen/awg_compiler_config.cpp:56-68`
(`AWGCompilerConfig::getAwgDeviceTypeFromString`) returns 9
`static_cast<AwgDeviceType>(N)` literals from a codename-keyed
lookup.  Every value maps to a named `AwgDeviceType` enumerator
(per `include/zhinst/core/types.hpp:30`):

| line | literal | named target |
|---|---|---|
| 57 | `static_cast<AwgDeviceType>(1)` | `AwgDeviceType::UHFLI` |
| 58 | `static_cast<AwgDeviceType>(2)` | `AwgDeviceType::HDAWG` |
| 59 | `static_cast<AwgDeviceType>(4)` | `AwgDeviceType::UHFQA` |
| 60 | `static_cast<AwgDeviceType>(8)` | `AwgDeviceType::SHFQA` |
| 61 | `static_cast<AwgDeviceType>(16)` | `AwgDeviceType::SHFSG` |
| 62 | `static_cast<AwgDeviceType>(32)` | `AwgDeviceType::SHFQC_SG` |
| 63 | `static_cast<AwgDeviceType>(64)` | `AwgDeviceType::SHFLI` |
| 64 | `static_cast<AwgDeviceType>(128)` | `AwgDeviceType::GHFLI` |
| 65 | `static_cast<AwgDeviceType>(256)` | `AwgDeviceType::VHFLI` |

The audit (§5 D4) called the function `parseCpuType` — that name
does not exist in the recon.  Actual symbol is
`AWGCompilerConfig::getAwgDeviceTypeFromString`.

**Suggested fix:** replace the 9 casts with named enumerators in
a follow-up FIX-NOW commit.  Risk low.

**Files**
- `reconstructed/src/codegen/awg_compiler_config.cpp:57-65`


## IF-321 — `holdSuppressExceptSigouts = 0x27C` derivation undocumented

**Severity:** cosmetic
**Status:** open

`reconstructed/include/zhinst/waveform/play_config.hpp:83` defines
`holdSuppressExceptSigouts = 0x27C` (= 636) as a `static inline
constexpr uint32_t`.  The brief explains *when* it's used (replaces
`suppress` in the encoded word when `hold` is set; see
`play_config.cpp:42-43`) but not *why* this particular bit pattern.

`0x27C = 0b1001111100`.  The 14-bit `suppress` field gates output
paths; 0x27C suppresses bits {2, 3, 4, 5, 6, 9, 12} of the
suppress space, equivalently "everything except sigout 0/1 (bits
0/1), {7,8,10,11,13}".  Need a hardware-doc cross-check to confirm
which physical outputs map to which bits, then update the brief
with a one-line "suppresses all destinations except primary signal
outputs" derivation.

**Files**
- `reconstructed/include/zhinst/waveform/play_config.hpp:80-83`
- `reconstructed/src/waveform/play_config.cpp:42-43`


## IF-322 — `0x54` VarType-set semantic name (6 sites)

**Severity:** suspicious
**Status:** open

Six sites in three files test `((0x54 >> vt) & 1)` against
`VarType` values to check membership in `{2, 4, 6}` = `{VarType_Var,
VarType_Const, VarType_Cvar}`:

| site | snippet | sense |
|---|---|---|
| `custom_functions.cpp:884` | `if (vt <= 6 && ((0x54 >> vt) & 1))` | reject if in set |
| `custom_functions.cpp:950` | `if (vt > 6 || !((0x54 >> vt) & 1))` | accept if in set |
| `custom_functions_play.cpp:760` | `if (vt0 > 6 \|\| !((0x54 >> vt0) & 1))` | accept if in set |
| `custom_functions_play.cpp:768` | `if (vt1 > 6 \|\| !((0x54 >> vt1) & 1))` | accept if in set |
| `custom_functions_registers.cpp:251` | `if (arg1Type > 6 \|\| !((0x54 >> arg1Type) & 1))` | accept if in set |
| `custom_functions_registers.cpp:286` | same | same |

`0x54 = 0b01010100`, selecting bits 2, 4, 6.  The audit (§4)
listed 6 sites; that count matches.

`VarType` definitions (per `runtime/resources.hpp:55`):

- `VarType_Var` = 2 — runtime mutable register
- `VarType_Const` = 4 — compile-time constant
- `VarType_Cvar` = 6 — constant variable (compile-time const, but
  bound to a name)

Common semantic: these are the three types that hold a **scalar
numeric value** addressable through a register, as opposed to
`VarType_String` (1), `VarType_Wave` (3), `VarType_FuncArg` (5),
`VarType_Trigger` (7), etc.

**Proposed name:** `kVarTypeScalarNumericMask` (or
`kVarTypeRegisterAssignable`), declared as `constexpr uint8_t` in
a header reachable from all three call sites — e.g.
`runtime/resources.hpp` next to the `VarType_*` definitions.

**GDB verification needed:** trace the original binary at one of
the accept-sites to confirm the rejected types and the error
message issued, so the semantic name reflects the actual rejection
criterion (e.g. "must be assignable to a register", not just
"numeric").

**Files**
- `reconstructed/src/runtime/custom_functions.cpp:871-884, 949-950`
- `reconstructed/src/runtime/custom_functions_play.cpp:756-768`
- `reconstructed/src/runtime/custom_functions_registers.cpp:249-286`
