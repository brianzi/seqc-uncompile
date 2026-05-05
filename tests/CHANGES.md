# Test Framework Enhancement - Summary of Changes

## Overview

Enhanced the SeqC compiler test framework with hierarchical organization, 
advanced filtering, modular manifest structure, and comprehensive documentation.

**Date:** 2026-05-05  
**Total tests:** 1346 (unchanged)  
**Files changed:** 15+ files created/modified

---

## What Changed

### 1. Python Scripts Organization

**Moved to `tests/gdb/`:**
- `gdb_trace.py`
- `gdb_trace_hdawg.py`
- `gdb_trace_multi.py`
- `gdb_playzero.py`
- `gdb_trace_moveLoadsToFront.py`
- `gdb_trace_placeLoads.py`
- `gdb_trace_moveloads.txt`

**Removed:**
- `w6_test.py` (redundant one-liner)

**Kept in `tests/`:**
- Core test runners (`diff_test.py`, `diff_test_fast.py`)
- Worker scripts (`compile_worker.py`, `compile_worker_batch.py`)
- Analysis tools (`dump_elf.py`, `symbol_size_compare.py`)

### 2. Manifest Structure (v2.0)

**Original:** Single `manifest.json` with 1346 tests in flat array

**New:** Modular hierarchical structure with 8 manifest files:

```
manifest.json (main, 64 KB)
├── manifest-core.json (implicit in main file, 234 tests)
├── manifest-ziai.json (84 KB, 459 tests)
├── manifest-ziasm.json (118 KB, 468 tests in 11 subgroups)
├── manifest-documentation.json (19 KB, 92 tests)
├── manifest-zhinst.json (14 KB, 60 tests)
├── manifest-labone.json (3.4 KB, 14 tests)
├── manifest-large.json (3.4 KB, 13 tests)
└── manifest-errors.json (2.4 KB, 6 tests)

manifest.v1.json (backup, 219 KB)
```

### 3. Group Organization

**8 top-level groups:**

| Group | Tests | Subgroups | Description |
|-------|-------|-----------|-------------|
| `core` | 234 | 0 | Core functionality tests |
| `ziai` | 459 | 0 | ZIAI analysis (individual constructs) |
| `ziasm` | 468 | 11 | ZIASM assembly tests with subgroups |
| `documentation_examples` | 92 | 0 | Product documentation examples |
| `zhinst` | 60 | 1 | Zhinst tools (webserver_presets) |
| `labone_examples` | 14 | 0 | LabOne API examples |
| `large_programs` | 13 | 0 | Large stress tests |
| `error_validation` | 6 | 0 | Error message validation |

**ZIASM subgroups:**
- `conditionals` (52), `firmware` (18), `misc` (57), `playconfig` (19)
- `prefetching` (10), `readout` (6), `register` (156), `triggers` (76)
- `user` (26), `various` (9), `waits` (39)

### 4. Hierarchical Test Naming

**Old format (v1.0):**
```
ziai_analyze_arithmetic_hdawg8
ziasm_conditionals_0_hdawg8
hdawg_ex_ziAWG_functional_simple_example
```

**New format (v2.0):**
```
ziai:arithmetic_hdawg8[HDAWG8, sr=2.4e9]
ziasm:conditionals:test_0[HDAWG8, sr=2.4e9]
zhinst:webserver_presets:functional_simple_example[HDAWG8, sr=2.4e9]
```

### 5. New Features

**Manifest v2.0 features:**
- Hierarchical groups with nesting
- Import/merge from multiple manifest files
- `@reference` syntax for reusing definitions
- Multi-device expansion (`"devices": ["@hdawg8", "@uhfli"]`)
- Test metadata (comments, tags)
- Auto-tagging by device family

**CLI enhancements:**
```bash
# Discovery
--list-groups          # Show all groups and counts
--list-tags            # Show all tags and counts

# Filtering
--groups GROUP1,GROUP2         # Include specific groups
--exclude-groups GROUP1        # Exclude groups
--tags TAG1,TAG2              # Include tests with tags
--exclude-tags TAG1           # Exclude tests with tags
--filter PATTERN              # Name pattern matching (existing)
```

**Auto-generated tags:**
- Device family: `hdawg`, `shfqa`, `shfsg`, `uhf`, `lockin`
- Full device: `hdawg8`, `shfqa4`, etc.
- Group tags: `ziai`, `ziasm`, `errors`, `documentation`, etc.

### 6. New Documentation

**Created:**
- `MANIFEST_FORMAT.md` (16 KB) - Complete v2.0 specification
- `gdb/README.md` (10 KB) - GDB debugging workflows
- `QUICKSTART.md` - Quick reference guide
- `CHANGES.md` (this file) - Summary of changes

**Updated:**
- `README.md` - Added tools, stats, organization table

### 7. Backward Compatibility

✅ **100% backward compatible:**
- v1.0 manifests continue to work indefinitely
- No breaking changes to test runners
- Original manifest backed up as `manifest.v1.json`
- Auto-detection of v1.0 vs v2.0 format

---

## Usage Examples

### Discovery

```bash
# List all groups
python3 tests/diff_test_fast.py --list-groups

# List all tags
python3 tests/diff_test_fast.py --list-tags
```

### Group Filtering

```bash
# Run only ZIAI tests
python3 tests/diff_test_fast.py --groups ziai

# Run ZIASM and documentation
python3 tests/diff_test_fast.py --groups ziasm,documentation_examples

# Exclude large tests
python3 tests/diff_test_fast.py --exclude-groups large_programs
```

### Tag Filtering

```bash
# Run only HDAWG tests
python3 tests/diff_test_fast.py --tags hdawg

# Run all lockin device tests
python3 tests/diff_test_fast.py --tags lockin

# Exclude error tests
python3 tests/diff_test_fast.py --exclude-tags errors
```

### Combined Filtering

```bash
# Documentation examples for HDAWG only
python3 tests/diff_test_fast.py \
    --groups documentation_examples \
    --tags hdawg

# All tests except large and errors
python3 tests/diff_test_fast.py \
    --exclude-groups large_programs \
    --exclude-tags errors

# ZIASM conditionals only (via name filter)
python3 tests/diff_test_fast.py \
    --groups ziasm \
    --filter conditionals
```

---

## Technical Details

### Manifest Loader (`manifest_loader.py`)

**Key components:**
- `ManifestLoader` class - Handles v1/v2 parsing
- `TestCase` dataclass - Canonical test representation
- Reference resolution (`@name` → definition lookup)
- Import processing (recursive, with circular detection)
- Multi-device expansion
- Auto-tagging by device type

**Public API:**
```python
from manifest_loader import load_manifest, load_manifest_as_dicts
from pathlib import Path

# Load as TestCase objects (full metadata)
tests = load_manifest(Path('cases/manifest.json'))

# Load as dicts (backward compatibility)
tests_dict = load_manifest_as_dicts(Path('cases/manifest.json'))
```

### Test Runner Integration

Both `diff_test.py` and `diff_test_fast.py` updated to:
1. Use `manifest_loader` instead of `json.load`
2. Accept new CLI filter arguments
3. Support discovery commands (`--list-*`)
4. Handle hierarchical test names

No changes to worker scripts (`compile_worker*.py`) - they receive
tests in the same dict format as before.

---

## Validation Results

✅ All validations passed:

- **Test count:** 1346 tests (preserved)
- **Groups:** 8 top-level, 11 ZIASM subgroups
- **Imports:** 7 manifest files merged correctly
- **Naming:** Hierarchical format with correct formatting (`sr=2.4e9`)
- **Filtering:** All filter combinations work
- **Backward compatibility:** v1.0 manifest loads correctly
- **Test execution:** Both runners work with new structure

---

## File Inventory

**New files (9):**
```
tests/manifest_loader.py
tests/MANIFEST_FORMAT.md
tests/QUICKSTART.md
tests/CHANGES.md
tests/gdb/README.md
tests/cases/manifest-ziai.json
tests/cases/manifest-ziasm.json
tests/cases/manifest-zhinst.json
tests/cases/manifest.v1.json
```

**Modified files (6):**
```
tests/README.md
tests/diff_test.py
tests/diff_test_fast.py
tests/cases/manifest.json
tests/cases/manifest-errors.json
tests/cases/manifest-documentation.json
tests/cases/manifest-labone.json
tests/cases/manifest-large.json
```

**Moved files (7):**
```
tests/gdb_*.py → tests/gdb/
tests/gdb_*.txt → tests/gdb/
```

**Removed files (1):**
```
tests/w6_test.py
```

---

## Benefits

1. **Better organization:** 1346 tests split into 8 logical groups
2. **Easier navigation:** Hierarchical naming shows test structure
3. **Flexible filtering:** Filter by group, tag, or name pattern
4. **Modular manifests:** Smaller files, easier to edit/review
5. **Rich metadata:** Comments and tags for documentation
6. **Reusable configs:** Definitions eliminate duplication
7. **Comprehensive docs:** 40+ KB of new documentation
8. **Future-proof:** v2.0 format supports expansion (nested groups, imports)
9. **No disruption:** 100% backward compatible

---

## Migration Notes

**No migration required.** The original v1.0 manifest is backed up and
the framework supports both formats indefinitely.

**To use v2.0 features in new manifests:**
1. Add `"version": "2.0"`
2. Use `"definitions"` for common configs
3. Organize tests in `"groups"`
4. Reference definitions with `@name`
5. Use `"imports"` to merge multiple files

See `MANIFEST_FORMAT.md` for complete specification.

---

## Future Enhancements

Possible future additions (not implemented):
- Test dependencies (skip if prerequisite fails)
- Test timeouts (per-test or per-group)
- Custom test metadata (JIRA tickets, owners, etc.)
- Performance baselines (regression detection)
- Random test selection (fuzzing)
- Parallel execution within groups

The v2.0 format is designed to accommodate these features without
breaking changes.
