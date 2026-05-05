# Test Framework Quick Start

## Running Tests

### Full suite (fast)
```bash
python3 tests/diff_test_fast.py
```

### Single test (verbose)
```bash
python3 tests/diff_test.py --filter hdawg_nop -v
```

### By group
```bash
python3 tests/diff_test_fast.py --groups error_validation
python3 tests/diff_test_fast.py --exclude-groups large_programs
```

### By tag
```bash
python3 tests/diff_test_fast.py --tags hdawg,waveform
python3 tests/diff_test_fast.py --exclude-tags slow
```

### Discovery
```bash
# List all groups
python3 tests/diff_test_fast.py --list-groups

# List all tags
python3 tests/diff_test_fast.py --list-tags

# Show selected tests in pretty format (don't run)
python3 tests/diff_test_fast.py --groups ziai --show-only
python3 tests/show_manifest.py --groups labone_examples -v
```

## Test Organization

**1346 tests** organized into **8 top-level groups**:

- `core` (234 tests) - Core functionality tests (basic instructions, control flow, waveforms)
- `ziai` (459 tests) - ZIAI analysis tests (individual SeqC construct validation)
- `ziasm` (468 tests) - ZIASM assembly tests (11 subgroups: conditionals, firmware, misc, playconfig, prefetching, readout, register, triggers, user, various, waits)
- `documentation_examples` (92 tests) - Product documentation examples
- `zhinst` (60 tests) - Zurich Instruments tools (webserver presets)
- `labone_examples` (14 tests) - LabOne API integration examples
- `large_programs` (13 tests) - Large/complex stress tests
- `error_validation` (6 tests) - Compiler error/warning validation

## Manifest Files

```
tests/cases/
  manifest.json              ← Main manifest (v2.0, imports all others)
  manifest-errors.json       ← Error validation tests
  manifest-documentation.json← Documentation examples
  manifest-labone.json       ← LabOne API examples
  manifest-large.json        ← Large stress tests
  manifest-ziai.json         ← ZIAI analysis tests
  manifest-ziasm.json        ← ZIASM assembly tests (11 subgroups)
  manifest-zhinst.json       ← Zhinst tools/webserver presets
  manifest.v1.json           ← Backup of original flat manifest
```

## Adding Tests

See `MANIFEST_FORMAT.md` for complete documentation.

**Simple v2.0 example:**

```json
{
  "version": "2.0",
  "groups": [{
    "name": "my_tests",
    "defaults": {"devtype": "HDAWG8", "samplerate": 2.4e9},
    "cases": [
      {"name": "test1", "code": "while(1);"}
    ]
  }]
}
```

## Tools

- `diff_test_fast.py` - Fast parallel runner (preferred)
- `diff_test.py` - Single-threaded runner (debugging)
- `dump_elf.py` - Section-by-section ELF comparison
- `symbol_size_compare.py` - Binary size analysis
- `show_manifest.py` - Pretty-print test manifest with colors
- `gdb/` - GDB trace helpers (see `gdb/README.md`)

## Documentation

- `README.md` - Test framework overview
- `MANIFEST_FORMAT.md` - Complete manifest v2.0 specification
- `gdb/README.md` - GDB debugging workflows
