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
**Status**: sanctioned exception under AGENTS.md "Tooling vs
reconstructed code".  Approved by user before edit landed.

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
**Status**: open; queued in TODO.md Phase T as a refinement of
the `ast-lowered` dump.

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

**Status**: documented; no fix planned.

`seqcc --to=STAGE` (and its sugar `-S` / `-E`) is advertised as
"stop after STAGE" in the spirit of `gcc -S` / `gcc -E`.  In
gcc the early-exit is literal: with `-E`, the backend is never
invoked.  With seqcc, the early-exit is **logical only** — the
compile always runs to completion internally, and only the
*output routing* changes.

Concretely:

- `seqcc -E foo.seqc` calls `zhinst::compileSeqcWithIR()`,
  which runs the full pipeline (parse → astgen → lower → opt-pre
  → prefetch → opt-post → assemble → link to ELF), then routes
  the captured `loweredAst` IR sink to `-o` and discards the
  ELF.
- `seqcc -S foo.seqc` calls `zhinst::compileSeqc()` (or
  `compileSeqcWithIR()` when `--dump=ast-lowered` is also
  present), then pulls the `.asm` section out of the produced
  ELF and routes that to `-o`.

The user-visible compile-cost difference between `--to=lower`,
`--to=asm`, and `--to=link` is therefore **zero**.

**Why**: `zhinst::compileSeqc()` and `compileSeqcWithIR()` are
single-shot end-to-end entry points; the underlying
`AWGCompiler::compile()` does not expose stage-by-stage
suspension.  Introducing a real early-exit would require:

- Either a new `AWGCompiler::compileUpTo(stage)` entry point
  with its own state machine and a way to harvest partial
  output — significant additive recon surface, and a maintenance
  burden every time a new pass lands.
- Or a `setjmp/longjmp`-style abort-mid-compile that bypasses
  destructors — incompatible with the current implementation's
  RAII-heavy state.

Neither is justified at T5: the dominant cost of `seqcc -E` is
already the lexer + parser + lowering passes, which a literal
early-exit would only marginally accelerate.  The full-pipeline
run on a typical sequence takes a few milliseconds.

**Recommended action when revisited**: if a future workload
makes the wasted post-lower passes measurable (e.g. very large
sequences in a tight `-E` loop), add `compileUpTo(stage)` as a
B3 sanctioned recon exception.  Until then, treat `--to=` as a
selector for "which IR do you want as primary output", not a
performance lever.

**TODO references**: TODO.md Phase T → "T5 follow-up: optional
literal early-exit" (to add if needed).
