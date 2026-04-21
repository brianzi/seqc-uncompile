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
