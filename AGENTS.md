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
