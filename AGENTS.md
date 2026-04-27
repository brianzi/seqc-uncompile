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
- Reconstructed headers go in `reconstructed/include/zhinst/`.
- Reconstructed implementations go in `reconstructed/src/`.
- Every `.cpp` file should note the binary addresses of the functions
  it reconstructs, in comments next to each function definition.

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
  cross-reference identifiers (numbered items 1-116+) and is not
  reorganized by topic.
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
(IF-1, IF-2, ...), a severity, and a status.  Entries are never deleted,
only updated to "confirmed", "dismissed", or "fixed".

At sub-phase wrap-up, incidental findings are triaged:
- **likely-bug**: promoted to a TODO.md work item
- **suspicious**: kept for future investigation
- **cosmetic/dismissed**: left in the file for historical record

This prevents loss of hard-won observations during deep debugging.

## Build verification

- The build system is `reconstructed/CMakeLists.txt` (cmake + a build
  directory at `reconstructed/build/`). Sources are picked up via
  `file(GLOB CONFIGURE_DEPENDS src/*.cpp)`, so new .cpp files added
  under `src/` are auto-registered — no CMakeLists edit required for
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

The original binary is `_seqc_compiler.so`, a Python extension module.
The test helper `tests/gdb_trace.py` invokes it via Python. To trace:

1. **Write a small test script** (or use `tests/gdb_trace.py`):

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
   gdb -batch -x /tmp/gdb_trace.txt --args python3 tests/gdb_trace.py 2>&1 \
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
  byte: `*(unsigned char*)0x<addr>` should be `0x55` (push rbp) for
  function entries.
- **libc++ string layout** (SSO): bit 0 of byte 0 indicates long (1) vs
  short (0). Short: length = byte0 >> 1, data at ptr+1. Long: data ptr
  at ptr+16, length at ptr+8.

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
