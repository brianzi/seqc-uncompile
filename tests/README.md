# Differential ELF Testing

Validates the reconstructed SeqC compiler against the original binary by
compiling the same SeqC programs with both and comparing the ELF output
section-by-section.

## Architecture

```
diff_test.py            ← original test harness (for single tests / debugging)
diff_test_fast.py      ← fast batch runner (preferred for full suite)
compile_worker.py      ← subprocess that loads one .so and compiles
compile_worker_batch.py← batch worker for fast runner
cases/
  manifest.json        ← test case definitions
  *.seqc               ← SeqC source files (referenced by manifest)
```

Each test case runs in **two isolated subprocesses** — one loading the
original `_seqc_compiler.so`, the other loading the reconstructed module.
This avoids symbol conflicts from having both in the same process.

## Prerequisites

- Python 3.10+
- The original `_seqc_compiler.so` at the repo root (already present)
- No pip dependencies required (ELF parsing is built into the harness)

## Usage

### Running the full test suite (recommended: use fast runner)

```bash
python3 tests/diff_test_fast.py
```

The fast runner uses fork-per-test batch workers for dramatically reduced
startup overhead. Each side (original + recon) loads the .so once, then forks
a child per test case with Linux COW isolation.

Options:
- `-j N` — parallelism per side (default: CPU count)
- `--filter PATTERN` — run only tests matching pattern
- `-v` — verbose output
- `--original-only` — smoke test original binary only

### Smoke-test the original binary

```bash
python3 tests/diff_test_fast.py --original-only
```

This confirms all test cases compile successfully with the original binary.

### When to use which runner

| Use case | Recommended |
|----------|-------------|
| Running full test suite | `diff_test_fast.py` |
| Debugging a single test | `diff_test.py` |
| CI / automated runs | `diff_test_fast.py` |
| Single-case comparison | `diff_test.py` with `--filter` |

The fast runner skips ELF parsing for byte-identical results (verified by
SHA-256), making it significantly faster for large test suites.

### Legacy: original test harness

```bash
python3 tests/diff_test.py
```

Use this only for debugging individual tests or when you need the simpler
output format.

Default paths:
- Original module: `../_seqc_compiler.so` (repo root)
- Reconstructed module: `../reconstructed/build/_seqc_compiler.so`

Override with:
```bash
python3 tests/diff_test.py \
    --original-dir /path/to/original \
    --recon-dir /path/to/reconstructed/build
```

### Filtering

Run a subset of tests:
```bash
python3 tests/diff_test.py --filter hdawg
python3 tests/diff_test.py --filter shfqa
```

### Verbose output

```bash
python3 tests/diff_test.py -v
```

## Adding test cases

Edit `cases/manifest.json`. Each entry needs:

| Field        | Required | Description                                       |
|--------------|----------|---------------------------------------------------|
| `name`       | yes      | Unique test name                                  |
| `code`       | *        | Inline SeqC code                                  |
| `file`       | *        | Path to .seqc file (relative to `cases/`)         |
| `devtype`    | yes      | e.g. `"HDAWG8"`, `"SHFQA4"`, `"SHFSG8"`          |
| `index`      | no       | AWG core index (default: 0)                       |
| `options`    | no       | Device options string                             |
| `samplerate` | no       | Required for HDAWG (e.g. `2.4e9`)                 |
| `sequencer`  | no       | Required for SHFQC (`"qa"`, `"sg"`, or `"auto"`)  |

*Provide either `code` (inline) or `file` (external .seqc), not both.

## Building the reconstructed module

The module builds automatically with the static library:

```bash
cd reconstructed/build
cmake .. -DPYBIND11_FINDPYTHON=ON \
         -DPYTHON_EXECUTABLE=/usr/bin/python3 \
         -DPYTHON_INCLUDE_DIR=/usr/include/python3.14
cmake --build . -j$(nproc)
```

This produces `_seqc_compiler.so` alongside `libzhinst_seqc.a`.

**Important**: The `Python3_EXECUTABLE` and include path must match the Python
interpreter you'll use to run the tests, or pybind11's version check will
reject the module at import time.

### Current status

The module **builds and loads** (using RTLD_LAZY to tolerate ~16 remaining
undefined symbols), but **crashes at runtime** when `compile_seqc` is called
because the compilation pipeline hits unimplemented methods (AsmCommands,
AWGAssembler::getReport, Prefetch wvf helpers, etc.).

See the "Remaining blockers" section in `TODO.md` for the specific symbols.

## What the comparison checks

When both compilers succeed:
- Byte-identical ELF → **PASS**
- Otherwise, section-by-section comparison:
  - Missing sections in either output
  - Section size mismatches
  - Content differences (reports first divergent byte offset)
  - ELF header field mismatches (e_type, e_machine, e_entry)

When one or both error:
- Both error with identical message → **PASS**
- Different errors or only one errors → **FAIL**
