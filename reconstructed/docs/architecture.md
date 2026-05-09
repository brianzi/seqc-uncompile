# SeqC Compiler — Reconstructed {#mainpage}

This site documents the reverse-engineered C++ reconstruction of
`_seqc_compiler.so`, the SeqC compiler shipped with Zurich Instruments LabOne.

The reconstruction lives in `reconstructed/src/` and
`reconstructed/include/zhinst/`. It is built into a static library
`libzhinst_seqc.a` and a Python extension module `_seqc_compiler.so`
that is differentially tested against the original binary by
`tests/diff_test_fast.py` (currently 1600/1600 passing).

## Status

This documentation site is in its initial setup phase
(**Phase D0** of the documentation roadmap — see `TODO.md`).
At this point only the framework, theme, and accuracy-discipline
machinery are in place. **Almost no symbols are documented yet.**

Subsequent phases will add per-class and per-function documentation:

- **D1** — High-level architecture narrative on this page.
- **D2** — `\brief` line on every public class.
- **D3** — Full documentation of pipeline-driver functions.
- **D4** — Public methods of high-traffic classes.
- **D5** — Internal helpers.
- **D6** — Long-form notes promoted to Doxygen pages.

## Accuracy discipline

Every documentation claim must be backed by one of:

1. Disassembly evidence (binary address cited in the source file).
2. GDB-verified runtime behavior.
3. Cross-reference to a `reconstructed/notes/*.md` topic file.
4. Differential test coverage in `tests/cases/manifest.json`.

When none of these apply, documentation **must** use one of the
project's custom Doxygen tags rather than guess:

- `\unclear` — meaning genuinely unknown; needs investigation.
- `\verifyme` — believed correct but not yet verified by GDB or test.
- `\binarynote` — verified fact about the binary that diverges from
  idiomatic C++ (informational, not a gap).

Each tag aggregates onto its own cross-reference page, accessible from
the *Related Pages* section of the navigation. This makes the entire
documentation backlog discoverable rather than scattered.

## Project layout pointers

- `reconstructed/include/zhinst/` — public headers (~50 classes).
- `reconstructed/src/` — implementations (~70 .cpp files).
- `reconstructed/notes/` — architectural and forensic notes.
- `reconstructed/notes/comment_style_guide.md` — the authoritative
  source-comment conventions, including §13 on documentation comments.
- `tests/` — differential test harness (excluded from this site).

## Building this site

From `reconstructed/build/`:

```
cmake --build . --target docs
```

The HTML output is written to `reconstructed/build/docs/html/`.
A warning log is written to `reconstructed/build/docs/doxygen-warnings.log`.
Coverage progress can be inspected with `reconstructed/docs/coverage.sh`.
