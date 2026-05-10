# Working Process

## Iteration Cycle

1. **Execute**: Pick the next task from TODO.md. Disassemble, reconstruct,
   and update source files under `reconstructed/`.

2. **Document**: Update OVERVIEW.md to reflect what changed. Add or revise
   files under `reconstructed/notes/` with new findings (struct offsets, opcode details,
   open questions resolved or discovered).

3. **Propose**: Present a summary of what was learned to the user. Suggest:
   - New work items to add to TODO.md
   - Reprioritization or reordering of existing items
   - Items that can be closed or merged
   - Scope changes (expand or narrow)

4. **Review**: Discuss proposals with the user. Only update TODO.md after
   agreement.

## Reconstruction Principle: Structure AND Methods Together

When analyzing a class or struct from disassembly, always reconstruct both
its **layout** (header) and its **method implementations** (.cpp) in the
same pass. Do not defer method reconstruction to a later phase.

Concretely, when working on a type:
1. Determine the struct/class layout → write/update the `.hpp` header.
2. Identify and disassemble all methods → write the `.cpp` implementation
   file immediately.
3. If the method list is too large for one session, implement the methods
   that were actually analyzed and leave the rest as TODO stubs with
   addresses noted in comments.

This avoids the "layout-first, methods-later" gap where structural
findings are integrated but analyzed method logic is lost or must be
re-derived.

## Verify-then-write: code is the source of truth

**Every claim written into a source file, header, doc comment, or
notes file MUST be verified against primary evidence before it is
written.**  Primary evidence is, in descending order of trust:

1. The disassembly of `_seqc_compiler.so` (via `objdump -d` or GDB).
2. The actual body of the reconstructed function (the `.cpp` source
   for the symbol being described).
3. The verified field name / offset in the canonical struct header
   (the `.hpp` for the type being referenced).

**Secondary sources are NEVER sufficient on their own.**  Secondary
sources include:

- `// ====` block-header summary comments at the top of recon
  functions.  These were written during early reconstruction and
  frequently drift out of sync with the body when fields are
  renamed, enums are reworked, or branches are corrected.
- Prose summaries in `notes/` files.
- The summary text of a previously-closed incidental finding.
- Variable names / parameter names from earlier reconstruction
  passes that were later relabelled.
- Any agent's recollection of "what this function does" from a
  prior session.

### Concrete workflow

When writing a doc comment, audit note, or any new descriptive prose
about an existing function or struct:

1. **Read the body.**  Open the `.cpp` (or `.hpp` for inline
   methods) and read the actual code, not the comment block above
   it.  Do this even if a summary comment exists and looks
   authoritative.
2. **Cross-check field names.**  Every field reference in the prose
   must match the canonical name in the type's `.hpp` *as currently
   committed*, not as it was named when the summary was written.
3. **Cross-check called helpers.**  When the prose claims "calls X
   which does Y", open X's body and verify Y.  Do not trust X's
   summary comment.  In particular, watch for stub bodies that
   contain the intended logic only as a comment.
4. **For semantics derived from the binary**, prefer GDB / objdump
   over reading the recon body — see "GDB tracing for binary
   analysis" below.
5. **Log discrepancies as IF-NNN entries** in
   `reconstructed/notes/incidental_findings.md` immediately, before
   continuing the main task.  A wrong block-header summary is an
   IF.  A stub body whose intended behaviour exists only as a
   comment is an IF.
6. **Fix the secondary source** in the same edit pass when the
   discrepancy is purely descriptive (e.g. rewriting a wrong
   summary comment to match the verified body).  This prevents the
   next agent from being misled by the same stale text.

### Anti-patterns

- Writing a `\brief` from the `// ====` block-header summary
  without opening the function body.
- Trusting a parameter / field name from a recon comment when the
  type's header has since been renamed (e.g. `pageSize_` →
  `maxBranches_`, `field_0x18` → `isHirzel`, `field_0x1E` →
  `dummy`).
- Quoting a `notes/` file's claim about a function's behaviour
  without re-verifying against the current `.cpp`.
- "Updating the recap" or progress summary with claims that were
  not re-verified in the current session.
- Letting a subagent fill in doc briefs from summaries — subagents
  inherit this rule and must follow the same workflow.

### Why this matters

The reconstruction has been through many rounds of field renames,
enum corrections, and stub fills.  Every such change creates a
window where prose summaries lag the code.  Documentation written
from those stale summaries propagates errors directly into the
user-facing Doxygen site, where they are much harder to spot than
in code.  Worse, a wrong doc comment can mislead a future
reconstruction pass that consults it as ground truth.

The cost of opening the function body is seconds.  The cost of a
wrong doc brief is hours of downstream confusion plus an awkward
correction commit.

## Conventions

- TODO.md is the shared task list. Changes require user review.
- **All actionable work items go into TODO.md — nowhere else.** Do not
  create "outstanding work", "audit findings", "action items", or any
  other file that is a synonym for a TODO list. If an audit or analysis
  produces actionable items, add them directly to TODO.md.
- OVERVIEW.md is the living reference for what exists and its status.
- `reconstructed/notes/` holds detailed technical data (offsets, opcodes,
  comparisons, unknowns) that doesn't belong in source comments.
- Each TODO.md phase ends with an explicit wrap-up step that triggers
  steps 2-4 above.
- **Sub-phase wrap-ups:** Steps 2-4 also happen at the end of each
  sub-phase (e.g. 2a, 2b, 2e), not only at the end of a full phase.
  This ensures findings are documented and TODO.md stays current.
- Reconstructed headers go in `reconstructed/include/zhinst/<subsystem>/`;
  the `.cpp` mirror lives at `reconstructed/src/<subsystem>/`.
  Subsystems currently include: `asm/`, `ast/`, `codegen/`, `core/`,
  `device/`, `infra/`, `io/`, `runtime/`, `waveform/`.  Place new files
  alongside their existing peers; create a new subsystem directory only
  when no existing one fits.
- The Python module entry point lives at `reconstructed/src/pybind_seqc.cpp`.
  The `_module_shim.cpp` you may see in `reconstructed/build/` is
  generated boilerplate — do not edit it.
- Every `.cpp` file should note the binary addresses of the functions
  it reconstructs, in comments next to each function definition.

## Git commit discipline

- **Commit after every sub-phase wrap-up** (step 2 of the iteration
  cycle) and whenever TODO.md is updated with significant changes.
- Commit messages should be concise and describe what was fixed/changed,
  e.g. "fix 13 tests: wavemem formatting, cut() end-index, chirp/sinc
  3-arg overloads".
- Include the test score in the commit message body (e.g.
  "1597/1600 passing").
- Do NOT commit mid-debug with known regressions — always verify the
  full test suite before committing.
- Typical commit points: after fixing a batch of test failures, after
  adding new tests, after audits, after documentation updates.

## Notes organization

- Notes files in `reconstructed/notes/` are organized by **topic, not by
  phase**. Filenames should describe the long-lived subject (e.g.
  `optimization_passes.md`, `device_type.md`), never `phase_NNx.md`.
- When a sub-phase produces findings, integrate them into the relevant
  existing topic file. Create a new topic file only when the subject
  genuinely doesn't fit any existing one.
- Phase-named transient files are acceptable mid-session as scratchpads,
  but must be folded into topic files at sub-phase wrap-up (step 2 of
  the iteration cycle).
- Exception: `unknowns.md` is a special tracking file with stable
  cross-reference identifiers (originally 1-116, now extending into the
  120s; older items live in `archive/unknowns_full_1-116.md`) and is
  not reorganized by topic.
- `archive/` holds historical or superseded content; phase-completion
  audits and snapshots that contain no long-lived technical content can
  be retired there.

## Incidental discovery discipline

While investigating a specific bug or test failure, the agent will
frequently encounter discrepancies that are **not** the root cause of
the immediate issue but are genuine mismatches between reconstruction
and binary.  Examples:

- Comments that contradict verified enum mappings
- Constructor/destructor logic that differs from the binary
- Struct field semantics that don't match disassembly
- ABI assumptions (libc++ vs libstdc++) that affect layout or behavior
- Code paths that exist in the binary but are missing/different in recon

**Rule**: Every such observation must be recorded in
`reconstructed/notes/incidental_findings.md` **immediately**, before
continuing the main investigation.  Each entry gets a stable ID
(IF-100, IF-101, ...; entries IF-1..IF-99 are archived in
`reconstructed/notes/archive/IF_1-99.md`), a severity, and a status.
Entries are never deleted, only updated to "confirmed", "dismissed",
or "fixed".

At sub-phase wrap-up, incidental findings are triaged:
- **likely-bug**: promoted to a TODO.md work item
- **suspicious**: kept for future investigation
- **cosmetic/dismissed**: left in the file for historical record

This prevents loss of hard-won observations during deep debugging.

## Build verification

- The build system is `reconstructed/CMakeLists.txt` (cmake + a build
  directory at `reconstructed/build/`). Sources are picked up via
  `file(GLOB_RECURSE ZHINST_SEQC_SOURCES CONFIGURE_DEPENDS src/*.cpp)`,
  so new .cpp files added under `src/` (including any `src/<subsystem>/`
  subdirectory) are auto-registered — no CMakeLists edit required for
  pure source additions.
- **Per-file `g++ -fsyntax-only` is NOT a substitute for a real build.**
  It misses: linker errors (undefined symbols, multiple-definition
  conflicts), "declared but not defined" warnings, ODR violations
  across TUs, and any cross-TU consistency issues.
- **At the end of every sub-phase wrap-up (step 2 of the iteration
  cycle), run `cmake --build .` from `reconstructed/build/` and
  triage all warnings.** "Used but never defined" warnings in
  particular are real bugs that the static-archive link silently
  tolerates but would break any future executable link.
- If any new file requires a CMakeLists change (e.g., new dependency,
  new target, non-`src/*.cpp` source location), update CMakeLists.txt
  in the same commit as the source change.
- The `compile_commands.json` produced under `reconstructed/build/`
  is the source of truth for compiler flags when invoking
  `-fsyntax-only` directly. Keep that file regenerated after any
  CMakeLists or dependency change.

## Documentation system

The reconstructed compiler ships an inline-Doxygen documentation site
rendered with Doxygen Awesome CSS.  The full how-to — markers, custom
aliases, coverage tracking, theme update procedure, and the multi-phase
plan — lives in `reconstructed/docs/README.md`.  Read it before writing
or editing any documentation comments.

Pointers for agents:

- **Comment markers**: use `//!` (single-line) and `/*! */` (multi-line)
  for documentation.  Plain `//` is reserved for reconstruction notes
  and is **not** picked up by Doxygen.  Doxygen `///` and `/** */` are
  forbidden — see `reconstructed/notes/comment_style_guide.md` §13.
- **Voice & content** for `//!` / `/*! */` blocks: document what the
  code does (behaviour, contract), do **not** name the original binary
  or cite addresses / vtable offsets, and leave existing `//`
  reconstruction comments as-is when adding `//!` briefs above them.
  Full rules and an annotated example: see "Voice & content rules" in
  `reconstructed/docs/README.md`.
- **Accuracy discipline**: never invent details to fill a doc comment.
  Three custom tags exist for the unknown / uncertain / divergent cases:
  - `\unclear` — we don't yet know what this does.
  - `\verifyme` — we have a hypothesis that is **not** GDB- or
    test-verified yet.  State the hypothesis explicitly.
  - `\binarynote` — non-idiomatic API behaviour the caller must be
    aware of.  Avoid in user-facing class-level briefs; reserve for the
    rare member-level case where surprising behaviour must be flagged.
    Mentioning the original binary is permitted *only* inside a
    `\binarynote` body and only when it explains the surprise.
  Each tag generates a dedicated cross-reference page so the backlog is
  discoverable.
- **Build the site**: `cmake --build . --target docs` from
  `reconstructed/build/`.  The target is registered automatically when
  Doxygen is installed; if Doxygen is missing, cmake skips it without
  failing.
- **Coverage check**: `reconstructed/docs/coverage.sh` from the repo
  root prints documented-symbol percentage and `\unclear` /
  `\verifyme` / `\binarynote` counts.
- **Phase plan**: see `TODO.md` Phase D entries.  The current phase
  determines whether `WARN_IF_UNDOCUMENTED` is on or off — do not flip
  that flag without a TODO entry sanctioning it.
- **Theme overrides**: never edit `reconstructed/docs/theme/doxygen-awesome.css`
  (vendored, pinned).  Put local overrides in
  `reconstructed/docs/theme/custom.css` so version bumps stay clean.

## GDB tracing for binary analysis

**GDB is the single most effective tool for resolving reconstruction
ambiguities.** Reading disassembly statically is slow, error-prone, and
frequently leads to wrong conclusions about control flow (e.g. which
branches are conditional on what, whether a phase is gated by a flag).
A single GDB trace can answer in seconds what takes hours of manual
disassembly reading.

### When to use GDB

- When a differential test fails and the root cause is unclear after
  a first look at the disassembly.
- When you have a hypothesis about control flow (e.g. "Phase 5 is
  conditional on multiValue") — **verify it with GDB before coding**.
- When register/memory values at a specific instruction matter (e.g.
  "what is rax at this cmp?").
- When you need to know which code paths are actually taken for a
  specific test input.

### Recipe: tracing the original binary

The original binary is `_seqc_compiler.so` (at the repo root,
`./_seqc_compiler.so`), a Python extension module.
The test helper `tests/gdb/gdb_trace.py` invokes it via Python. To trace:

1. **Write a small test script** (or use `tests/gdb/gdb_trace.py`):

   ```python
   import sys; sys.path.insert(0, '.')
   import _seqc_compiler as sc
   result = sc.compile_seqc('wave w = zeros(64);\nplayWave(w);',
                            'HDAWG8', {}, 0, samplerate=2.4e9)
   ```

2. **Write a GDB command file** (`/tmp/gdb_trace.txt`):

   ```gdb
   set pagination off
   set confirm off
   set auto-load safe-path /

   # Catch library load via solib events
   set stop-on-solib-events 1
   run

   # Wait until _seqc_compiler.so is loaded
   python
   import gdb
   while True:
       info = gdb.execute("info sharedlibrary _seqc_compiler", to_string=True)
       if '_seqc_compiler' in info and '0x' in info.split('_seqc_compiler')[0].split('\n')[-1]:
           for line in info.split('\n'):
               if '_seqc_compiler' in line and '0x' in line:
                   base = int(line.split()[0], 16)
                   break
           break
       gdb.execute("continue")

   gdb.execute("set stop-on-solib-events 0")

   # Set breakpoints at binary offsets
   for name, off in [
       ('myFunc_entry', 0x15e060),
       ('myFunc_check', 0x15e326),
   ]:
       bp = gdb.Breakpoint(f'*{base + off}')
       gdb.execute(f"""commands {bp.number}
   silent
   printf ">>> {name} (0x{off:x})\\n"
   info registers rax rbx rcx rdx rdi rsi r12 r14 r15
   continue
   end""")
   end

   continue
   quit
   ```

3. **Run it:**

   ```bash
   gdb -batch -x /tmp/gdb_trace.txt --args python3 tests/gdb/gdb_trace.py 2>&1 \
     | grep -E '>>>|rax|rbx'
   ```

### Key gotchas

- **Do NOT use `break FunctionName`** — the .so has no debug symbols, and
  `break PyInit__seqc_compiler` silently fails ("not defined"). Use
  `set stop-on-solib-events 1` to catch library load instead.
- **Breakpoint `commands` with `continue` are essential** for batch mode.
  Without `commands ... continue ... end`, GDB stops at the first
  breakpoint and the subsequent `python` block runs against a stopped
  inferior, then the program never resumes.
- **Base address**: Read from `info sharedlibrary _seqc_compiler`. The
  first hex column is the text segment base. Add binary offsets to this.
- **Verify addresses**: Before trusting a breakpoint, check the first
  byte: `*(unsigned char*)0x<addr>` is usually `0x55` (push rbp) for
  function entries — but functions built with `-fomit-frame-pointer`
  may start with `push` of another callee-saved register, `sub rsp`,
  or a stack canary load (`mov rax, fs:[0x28]`).  An unknown opcode at
  the address is the real red flag.
- **libc++ string layout** (SSO, 24 bytes total).  Bit 0 of byte 0
  indicates form: short (0) or long (1).
  - Short form (≤22 chars): byte 0 = `(size << 1)`; inline data at
    `ptr+1` for up to 22 chars.
  - Long form: 8-byte `(capacity << 1) | 1` at `+0x00`, 8-byte size
    at `+0x08`, 8-byte data pointer at `+0x10`.

  See `reconstructed/notes/libcpp_abi.md` for the full reference,
  including allocator interactions and how this differs from the
  libstdc++ layout.

### Lessons learned

- Phase 5 of `mergeWaveforms` was incorrectly gated on `multiValue` in
  the reconstruction. Static disassembly analysis concluded it was
  multi-only. A single GDB trace proved it is unconditional — saving
  hours of wrong-path debugging.
- The `getOrCreateWaveform` cache logic was inverted (found-in-set meant
  "call factory" not "cache hit"). GDB would have caught this instantly
  by checking which branch was taken.
- **Default to GDB verification** whenever a control-flow assumption
  drives a code change. The cost is ~30 seconds; the cost of a wrong
  assumption is hours.

### Logging GDB friction (mandatory)

Whenever using GDB for binary analysis, if **anything about the GDB
setup itself** trips you up — a breakpoint that didn't fire, a
hung batch script, a wrong base-address calculation, a junk
register/memory read, an unexpected interaction between GDB,
Python, libstdc++ or libc++ — write a short report under
`tests/gdb/user-experiences/`.  See that directory's `README.md`
for the format.  This applies to **every agent and every
delegated subagent**: when you delegate a task that may involve
GDB, instruct the subagent to follow this rule.

Goal: grow the GDB reference material so each future session
hits fewer of the same friction points.  If GDB worked first try
with no surprises, do not log anything — these reports exist to
surface friction, not to accumulate trivia.

## Differential testing

### Running tests

**Always use `diff_test_fast.py` for the full suite** — it is dramatically
faster than `diff_test.py` thanks to fork-per-test batch workers.
`diff_test.py` is still useful for single-test verbose runs (it has cleaner
per-test output), but **never use it for regression checks** over the full
suite.

```bash
# Full suite (fast — use this for regression checks)
python tests/diff_test_fast.py

# Single test (verbose — diff_test.py is fine here)
python tests/diff_test.py --filter hdawg_play_dual_ch -v

# Smoke-test original only (no recon)
python tests/diff_test_fast.py --original-only

# Regex-style filter (matches substring in test name)
python tests/diff_test_fast.py --filter 'hdawg_doc'
```

### Test manifest

Test cases are defined in `tests/cases/manifest.json`, a **v2.0
schema** that is a thin import shell:

```json
{ "version": "2.0", "imports": ["./manifest-core.json", "..."] }
```

Each imported sub-manifest (`manifest-core.json`, `manifest-zhinst.json`,
`manifest-documentation.json`, `manifest-errors.json`,
`manifest-stress.json`, `manifest-large.json`, `manifest-labone.json`,
`manifest-ziai.json`, `manifest-ziasm.json`, `manifest-zivibes.json`)
has the shape:

```json
{
  "version": "2.0",
  "definitions": {
    "hdawg8":   { "devtype": "HDAWG8", "index": 0,
                  "samplerate": 2400000000.0 },
    "shfqa4_qa":{ "devtype": "SHFQA4", "sequencer": "qa" }
  },
  "groups": [
    { "name": "core",
      "tags": ["..."],
      "groups": [ /* nested */ ],
      "cases": [
        { "name": "play_dual_ch",
          "file": "play_dual_ch.seqc",
          "@base": "@hdawg8" },
        { "name": "awgidxs",
          "code": "playZero(64);",
          "devices": [{"index":0},{"index":1}],
          "@base": "@hdawg8" }
      ]
    }
  ]
}
```

Field reference:

- `definitions` — named device-config templates referenced from cases
  via `"@base": "@<name>"`.  The base supplies `devtype`, `index`,
  `samplerate`, and `sequencer` defaults; the case may override any
  field.
- `groups` — hierarchical grouping; tags propagate down.  Use
  `--groups <path>` / `--tags <tag>` to filter.
- `cases[].file` vs `cases[].code` — exactly one of: path to .seqc
  source relative to `tests/cases/`, or inline source string.
- `cases[].devices` — optional array; each entry parametrises one
  expansion of the case (e.g. running on every AWG index 0..3).
- `devtype` — one of: `HDAWG4`, `HDAWG8`, `SHFQA2`, `SHFQA4`,
  `SHFSG2`, `SHFSG4`, `SHFSG8`, `SHFQC`, `SHFLI`, `UHFLI`, `UHFQA`,
  `UHFAWG`, `GHFLI`, `VHFLI`.
- `samplerate` — **HDAWG only**.  Non-HDAWG devices must NOT include
  this field or the original binary errors with "'samplerate' is
  relevant for HDAWG only."
- `sequencer` — `"qa"` or `"sg"` for SHFQA / SHFQC / SHFSG; omit for
  others.

To enumerate cases without running them: `python tests/diff_test_fast.py
--show-only`.  Current suite size is around 1600 cases.

To **add** a new test case:

1. Drop the `.seqc` source under `tests/cases/` (or use inline `code`).
2. Add a `cases[]` entry to the appropriate sub-manifest
   (`manifest-zhinst.json` for compiler regressions,
   `manifest-errors.json` for expected-error inputs,
   `manifest-documentation.json` for documentation samples, etc.).
   Reference an existing `definitions` template via `"@base": "@<name>"`.
3. Verify: `python tests/diff_test_fast.py --filter <new_case_name>`.
4. Run the full suite to confirm no incidental regressions.

`tests/MANIFEST_FORMAT.md` is the canonical reference for the manifest
schema; consult it for edge cases (multi-device parametrisation, tag
inheritance, group nesting).

### compile_seqc() return value

The Python binding returns a **tuple** `(elf_bytes, info_dict)`, not a
dict.  Access the ELF as `result[0]`.

### SeqC output ELF format

The compiler emits a custom **32-bit little-endian ELF** (EI_CLASS=1,
EI_DATA=1) — not a standard executable but a firmware container.
**Do not assume 64-bit** — all headers and section entries use the
ELF32 format.

`reconstructed/notes/elf_format.md` is the **authoritative** schema for
all three ELF variants (main compiler output, legacy assembler output,
and waveform cache), including the full section list, conditional
emissions, segment layout, and reader-side consumption.
`reconstructed/notes/elf_reader.md` is authoritative for the `.linenr`
record format.  **Do not duplicate per-section facts in this file —
edit the notes instead.**

`e_entry` equals the device's `addressImpl` constant — typically
`0x80000000` for HDAWG and SHF devices, with other values for UHFLI /
UHFQA — plus `+0x80` for the legacy `AWGAssembler` pipeline.  See
`reconstructed/src/device/awg_device_props.cpp` for per-device values.
Differences in `e_entry` between original and recon usually mean the
recon generates a different number of instructions.

For day-to-day debugging the most-touched sections are `.text`,
`.asm`, `.linenr`, `.waveforms`, `.wf_<name>`, `.wavemem`, and
`.channels`.  Section formats vary (binary, plain text, JSON,
optionally zlib-compressed for `.c` and `.asm` depending on a config
flag); consult `notes/elf_format.md` before assuming.

#### Debugging byte diffs

When `diff_test.py` reports a byte diff, use `tests/dump_elf.py` to get
a detailed side-by-side comparison:

```bash
# Compare original vs recon, showing all differing sections
python tests/dump_elf.py tests/cases/hdawg_play_dual_ch.seqc HDAWG8 \
    --samplerate 2.4e9 --both

# Dump original only
python tests/dump_elf.py tests/cases/shfqa_feedback.seqc SHFQA4 \
    --sequencer qa

# Dump recon only
python tests/dump_elf.py tests/cases/shfqa_feedback.seqc SHFQA4 \
    --sequencer qa --recon
```

The `--both` flag is the primary debugging workflow: it compiles with
both original and recon, parses both ELFs, and for each differing
section prints the full decoded contents side-by-side. Identical
sections are shown as one-liners.

**Interpretation guide for common diff patterns:**

- `.asm` text diff: Compare the disassembly lines directly. Look for
  wrong mnemonics, wrong register numbers (e.g. `R1` vs `R2`), wrong
  immediates, missing/extra instructions.
- `.text` content diff at a specific offset: Divide offset by 4 to get
  the instruction index, then compare that instruction in `.asm`.
- `.text` size diff: Recon generates more or fewer instructions.
  Compare `.asm` line counts.
- `.wf_*` content diff: Waveform sample data differs. Usually a
  waveform generator bug (wrong formula, wrong scaling constant).
- `.waveforms` JSON diff: Metadata differs (wrong `play_config`,
  `marker_bits`, channel count, etc.).
- `.wavemem` size/content diff: Wave memory accounting differs.
- `.channels` diff: Channel bitmask or assignment wrong.
- `.linenr` diff: Line number mapping differs (usually follows from
  instruction count differences).
- `e_entry` diff: Entry point offset differs (usually follows from
  instruction count differences).
- Missing `.wf_*` section: Waveform not compiled into output — check
  waveform registration, `assignWaveIndex` node chaining, prefetch
  `collectUsedWaves`.

### Fixing difftest failures: required workflow

A failing difftest is a **symptom**. The goal is to find and fix the
underlying reconstruction error, not just to make the test pass.

**Mandatory steps for every difftest fix:**

1. **Understand the symptom** — run the test verbose (`-v`) and read the
   full error output. Is it a byte diff, an error mismatch, or a crash?

2. **Locate the binary behavior** — use GDB to trace the original binary
   on the failing input. Do not guess what the binary does from reading
   recon source alone; verify with GDB.  For error-output mismatches,
   set a breakpoint at the relevant `errorMessage` / `warningMessage`
   site in the original and confirm which code path is taken and why.

3. **Form a hypothesis** — identify which recon function differs from
   the binary, and exactly what the binary does that the recon doesn't.
   Write this down before touching any source file.

4. **Verify the hypothesis with GDB** — before editing any source, use
   GDB to confirm the hypothesis.  A single trace confirming the branch
   taken or register value saves hours of wrong-path work.

5. **Apply the fix** — edit the single function that was wrong.  The fix
   should make the recon match the binary; it should not be a workaround
   that merely produces the right test output through a different path.

6. **Build** — `cmake --build .` from `reconstructed/build/`. Fix any
   warnings or errors.

7. **Run the specific test** — `python tests/diff_test.py --filter <name> -v`.
   Confirm it passes.

8. **Run the full suite** — `python tests/diff_test_fast.py`. Confirm no
   regressions.

9. **Document** — add a finding to `reconstructed/notes/incidental_findings.md`
   or update the relevant topic notes file with what was wrong and how
   it was confirmed.

**Anti-patterns to avoid:**

- Editing source before GDB-verifying the binary behavior.
- "Fixing" a test by matching its error output string through a different
  code path than the binary uses.
- Applying multiple fixes at once without testing each individually.
- Committing before the full suite is clean.
