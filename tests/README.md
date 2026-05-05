# Differential ELF Testing

Validates the reconstructed SeqC compiler against the original binary by
compiling the same SeqC programs with both and comparing the ELF output
section-by-section.

**Test suite size:** 1346 test cases across 14 device types, organized into 8 groups

## Architecture

```
diff_test.py            ← original test harness (for single tests / debugging)
diff_test_fast.py      ← fast batch runner (preferred for full suite)
compile_worker.py      ← subprocess that loads one .so and compiles
compile_worker_batch.py← batch worker for fast runner
dump_elf.py            ← ELF section-by-section comparison/analysis tool
symbol_size_compare.py ← binary symbol size analysis tool
gdb/                   ← GDB trace helper scripts for debugging
cases/
  manifest.json        ← test case definitions (or manifest-*.json group files)
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

**Working with the large test suite:**

The test suite contains 1346 tests. Common filtering strategies:

```bash
# Run only HDAWG tests
python3 tests/diff_test_fast.py --filter hdawg

# Run documentation examples
python3 tests/diff_test_fast.py --filter _doc_

# Run error validation tests
python3 tests/diff_test_fast.py --filter err_

# Run a specific test
python3 tests/diff_test.py --filter hdawg_playZero -v
```

For more advanced filtering (by tags, groups), see the manifest v2.0 format
documentation in `MANIFEST_FORMAT.md`.

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

## Debugging Tools

### dump_elf.py - Detailed ELF comparison

When a test fails with byte differences, use `dump_elf.py` to see exactly
what differs:

```bash
# Compare original vs recon for a specific test
python3 tests/dump_elf.py tests/cases/hdawg_playZero.seqc HDAWG8 \
    --samplerate 2.4e9 --both

# Dump original only
python3 tests/dump_elf.py tests/cases/shfqa_feedback.seqc SHFQA4 \
    --sequencer qa

# Dump recon only
python3 tests/dump_elf.py tests/cases/some_test.seqc HDAWG8 \
    --samplerate 2.4e9 --recon
```

With `--both`, it compiles with both compilers and shows a side-by-side
comparison of all differing sections. Identical sections are shown as
one-liners. This is the primary workflow for understanding test failures.

Key sections to check:
- `.asm` - Sequencer instruction disassembly (text diff)
- `.text` - Raw instruction bytes (binary diff)
- `.waveforms` - Waveform metadata JSON
- `.wf_*` - Waveform sample data
- `.wavemem` - Memory usage accounting

### symbol_size_compare.py - Binary size analysis

Compare function sizes between original and reconstructed binaries to identify
optimization mismatches:

```bash
python3 tests/symbol_size_compare.py \
    --orig _seqc_compiler.so \
    --recon reconstructed/build/_seqc_compiler.so
```

**Note:** Build the recon binary with `-O2` (Release mode) for meaningful
comparison. Debug builds (`-O0`) will show large size ratios everywhere.

### gdb/ - GDB trace helpers

The `gdb/` directory contains Python scripts for tracing the original binary's
execution under GDB. These are invaluable for understanding control flow and
verifying reconstruction hypotheses.

**Key scripts:**
- `gdb_trace.py` - Generic UHFLI trace (placeholder test)
- `gdb_trace_hdawg.py` - HDAWG executeTableEntry test
- `gdb_trace_multi.py` - Multi-waveform test
- `gdb_playzero.py` - playZero with ternary argument
- `gdb_trace_moveloads.txt` - GDB command file for Prefetch tracing

**Usage example:**

```bash
# Run a trace script under GDB
gdb -batch -x tests/gdb/some_commands.txt \
    --args python3 tests/gdb/gdb_trace_hdawg.py 2>&1 | grep ">>>"
```

See `gdb/README.md` for detailed debugging workflows and best practices.

## Test Organization

Tests are organized in manifest files under `tests/cases/`. The manifest format
supports both v1.0 (flat array) and v2.0 (hierarchical groups with imports).

**Current organization (8 top-level groups, 1346 tests):**

| Group | Tests | Description |
|-------|-------|-------------|
| `core` | 234 | Core functionality - basic instructions, control flow, waveforms |
| `ziai` | 459 | ZIAI (compiler analysis) - validation of individual SeqC constructs across devices |
| `ziasm` | 468 | ZIASM (assembler) - low-level assembly tests in 11 subgroups (conditionals, firmware, misc, playconfig, prefetching, readout, register, triggers, user, various, waits) |
| `documentation_examples` | 92 | Examples from product documentation and user guides |
| `zhinst` | 60 | Zurich Instruments tools (webserver AWG presets) |
| `labone_examples` | 14 | LabOne software integration examples |
| `large_programs` | 13 | Large complex programs - stress tests |
| `error_validation` | 6 | Compiler error and warning message validation |

**Manifest files:**
- `manifest.json` - Main file (v2.0 format, imports all group manifests)
- `manifest-{group}.json` - Individual group manifests
- `manifest.v1.json` - Backup of original flat format

Tests can be filtered by name pattern, group, or tags (v2.0 manifests only).

See `MANIFEST_FORMAT.md` for complete documentation on adding tests, using
groups, and defining reusable configurations.
