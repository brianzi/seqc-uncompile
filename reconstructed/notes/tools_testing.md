# Toolchain Testing {#notes_tools_testing}

How the `seqcc` / `seqas` / `seqdump` toolchain is tested, how it
relates to the main differential-test harness, and how to add new
test cases.  For user-facing CLI documentation see
\ref notes_tools_user_guide; for design rationale see
\ref notes_tools_design.

[TOC]

## Overview

The toolchain is backstopped by **two independent test layers**:

1. **`tests/tools/`** — the toolchain's own suites.  Verify the CLI
   surface (flags, exit codes, output formats, short-circuit
   semantics) and that the driver routes each pipeline stage to the
   correct reconstructed entry point.
2. **`tests/diff_test_fast.py`** — the main differential-test
   harness (1612 cases).  Compiles every test input through both
   the original `_seqc_compiler.so` and the reconstructed compiler,
   then asserts byte-equality of the resulting ELFs.  The toolchain
   inherits this coverage transparently because every `seqcc`
   invocation routes through the same reconstructed `Compiler` and
   `AWGCompiler` types.

Both layers must stay green at every sub-phase wrap-up.

## Toolchain suites (`tests/tools/`)

Four files, currently 70 cases total.  Each runs the actual `seqcc`
or `seqdump` binary via `subprocess` — no Python bindings involved
in these tests, so they verify the same path users hit.

| File                      | Cases | Focus |
|---------------------------|------:|-------|
| `test_seqcc_smoke.py`     | 4     | Argv / exit-code smoke: `--help`, `--version`, missing `-march`, no input. |
| `test_seqcc_diff.py`      | 46    | Option parser, flag precedence (legacy vs new), `-O` / `-f` parsing, `--dump=` artefact emission, `--to=` byte-equality vs default, `-S` / `-E` sugar equivalence, dump-format / ast-lowered round-trip. |
| `test_seqcc_to.py`        | 4     | `--to=<stage>` short-circuit semantics: produces non-trivial ELF for `link`, text-not-ELF for `asm` and `lower`, all three payloads distinct and shorter than the full ELF. |
| `test_seqdump.py`         | 16    | ELF decode round-trip; mirrors `tests/dump_elf.py`. |

### Running them

```bash
# Individually
python tests/tools/test_seqcc_smoke.py
python tests/tools/test_seqcc_diff.py
python tests/tools/test_seqcc_to.py
python tests/tools/test_seqdump.py

# Verbose
python tests/tools/test_seqcc_diff.py -v
```

Each file is a standalone `unittest` script; there is no pytest
fixture or shared conftest beyond a small helper that finds the
`seqcc` / `seqdump` binary under `reconstructed/build/seqcc/`.

### Binary discovery

Every test file uses the same helper:

```python
def find_seqcc() -> Path:
    candidates = [
        REPO_ROOT / "reconstructed" / "build" / "seqcc" / "seqcc",
        REPO_ROOT / "build" / "seqcc" / "seqcc",
    ]
    for c in candidates:
        if c.exists() and os.access(c, os.X_OK):
            return c
    raise FileNotFoundError("seqcc binary not found; build the seqcc target first")
```

If the binary is missing the tests print `SKIP:` and exit `0` — they
do not fail the run.  This keeps CI green on configurations that
don't have a Doxygen / build environment.

## Main differential suite (`diff_test_fast.py`)

The full regression sweep used by every sub-phase wrap-up:

```bash
# Full suite (fast — fork-per-test batch workers)
python tests/diff_test_fast.py
```

1612 cases as of `0.11.0-T10a`.  Test cases are defined in
`tests/cases/manifest.json` (a v2.0 import shell) which pulls in
nine sub-manifests by topic (`manifest-core.json`,
`manifest-zhinst.json`, `manifest-documentation.json`,
`manifest-errors.json`, `manifest-stress.json`, …).

`seqcc` does not need its own entries in `manifest.json` — the
manifest exercises the underlying `compile_seqc()` path, and
`seqcc` routes through the same reconstructed code.  Toolchain-
specific regressions (CLI parsing, short-circuit semantics, dump
formatting) live in `tests/tools/` instead.

For single-test verbose runs:

```bash
python tests/diff_test.py --filter <name> -v
```

See `tests/diff_test_fast.py` and `tests/diff_test.py` for the
harness implementation.

## Smoke tests (`test_seqcc_smoke.py`)

Minimal sanity checks.  Verifies the binary exists, exits cleanly
on trivial inputs, and prints the expected usage / version text.

```python
def test_help_exits_zero(self): ...
def test_version_exits_zero_and_prints_version(self): ...
def test_missing_march_returns_two(self): ...
def test_no_input_returns_nonzero(self): ...
```

`test_missing_march_returns_two` is the **canonical usage-error
contract**: invoking with an input file but no `-march` exits with
status `2`.

## Differential / option-parser tests (`test_seqcc_diff.py`)

The largest suite.  Compiles trivial SeqC sources through both
`seqcc` *and* the Python `compile_seqc()` binding, then asserts:

- **Byte-equal ELFs** when the two invocations are semantically
  equivalent.  Catches regressions in the `AWGCompilerConfig`
  builder.
- **Flag-precedence invariants** between the new dedicated flags
  (`--samplerate`, `--mdevopt`) and the legacy escape hatches
  (`--mtune samplerate=…`, `--mdevopts NEWLINE_SEPARATED`).
- **`-O<n>` / `-f<pass>` parsing**: rightmost-wins on conflict,
  individual toggles compose with curated levels, unknown
  `-f<unknown>` is rejected with usage error.
- **`--dump=` semantics**: each requested artefact is written to
  the expected filename, explicit `:PATH` overrides, missing dumps
  do not affect the ELF, `--dump=ast-lowered` preserves ELF byte-
  equality (the dump is read-only).
- **`-S` / `-E` sugar equivalence**: `seqcc -S` produces the same
  bytes as `seqcc --to=asm`; `seqcc -E` produces the same bytes as
  `seqcc --to=lower`; both produce the same bytes as the
  corresponding `--dump=` artefact extracted from a full compile.

Two parametrised dimensions cover the bulk of the 46 cases:

- Device variants (HDAWG4 / HDAWG8 / SHFQA4 / SHFSG8 / SHFQC).
- Optimisation flag combinations (`-O0..3` × `±fdead-code-elim`).

## Short-circuit tests (`test_seqcc_to.py`)

Verifies the post-T5b.5 contract that `--to=<stage>` short-
circuits genuinely skip the back-end steps, rather than running the
full pipeline and discarding the ELF:

```python
def test_to_link_produces_elf(self):
    """--to=link emits a non-trivial 32-bit ELF (ELFCLASS32)."""

def test_to_asm_produces_text_not_elf(self):
    """--to=asm emits assembler text, not an ELF."""

def test_to_lower_produces_json_not_elf(self):
    """--to=lower emits lowered-AST JSON, not an ELF."""

def test_short_circuit_payloads_differ_from_link(self):
    """The three stages produce distinct byte streams,
    and the short-circuit outputs are much smaller than the ELF."""
```

The byte-equality of the produced `asm` / `lower` against the
full-pipeline run is covered separately by `test_seqcc_diff.py`;
this file only asserts the user-visible short-circuit invariants
(non-empty, correct magic bytes, smaller than the full ELF).

## `seqdump` tests (`test_seqdump.py`)

ELF decode round-trip.  Compiles a curated set of inputs through
`seqcc`, dumps each one with `seqdump`, and asserts:

- Every documented section appears in the dump.
- `--section=<name>` returns only the requested section.
- `--diff` correctly identifies differing sections between two
  ELFs.
- `--max-lines=N` truncates text sections to *N* lines.

Coverage mirrors `tests/dump_elf.py` (the original Python-based
inspector used during reconstruction).

## Adding new test cases

### A new `tests/tools/` case

Pick the suite that matches the dimension being tested:

| Behaviour                                | File                       |
|------------------------------------------|----------------------------|
| Argv / exit code on a new CLI surface    | `test_seqcc_smoke.py`      |
| Option parsing / flag interaction        | `test_seqcc_diff.py`       |
| New `--to=<stage>` short-circuit         | `test_seqcc_to.py`         |
| New ELF section or `seqdump` flag        | `test_seqdump.py`          |

Each file is a single `unittest.TestCase` subclass; add a method
named `test_<descriptive_name>` and follow the existing helpers
(`_run`, `_compile`).

### A new differential case

Toolchain regressions caused by an underlying compiler change are
caught by adding to `tests/cases/manifest.json`.  See AGENTS.md
"Differential testing" for the full procedure.  Briefly:

1. Drop the `.seqc` source under `tests/cases/` (or use inline
   `code`).
2. Add a `cases[]` entry to the appropriate sub-manifest, referencing
   an existing `definitions` template via `"@base": "@<name>"`.
3. Verify with `python tests/diff_test_fast.py --filter <new_name>`.
4. Run the full suite to confirm no incidental regressions.

## Wrap-up gate

Every sub-phase wrap-up must show all five gates green before the
phase can be marked complete:

```bash
python tests/tools/test_seqcc_smoke.py     # 4/4
python tests/tools/test_seqcc_diff.py      # 46/46
python tests/tools/test_seqcc_to.py        # 4/4
python tests/tools/test_seqdump.py         # 16/16
python tests/diff_test_fast.py             # 1612/1612
```

Mid-phase commits with known regressions are not permitted (per
AGENTS.md "Git commit discipline").

## Related notes

- \ref notes_tools — toolchain entry point and quick start.
- \ref notes_tools_user_guide — user-facing CLI reference.
- \ref notes_tools_design — design rationale and internals.
