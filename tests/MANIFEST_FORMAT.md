# Manifest Format Specification

The SeqC test framework supports two manifest formats for defining test cases:

- **v1.0**: Flat array of test cases (simple, backward compatible)
- **v2.0**: Hierarchical groups with imports, definitions, and rich metadata

Both formats are fully supported. v1.0 manifests continue to work indefinitely.

---

## Quick Start

### v1.0 Format (Simple)

```json
[
  {
    "name": "hdawg_basic_loop",
    "code": "while(1);",
    "devtype": "HDAWG8",
    "index": 0,
    "samplerate": 2.4e9
  },
  {
    "name": "shfqa_feedback",
    "file": "shfqa_feedback.seqc",
    "devtype": "SHFQA4",
    "sequencer": "qa"
  }
]
```

### v2.0 Format (Hierarchical)

```json
{
  "version": "2.0",
  
  "definitions": {
    "hdawg8": {
      "devtype": "HDAWG8",
      "index": 0,
      "samplerate": 2.4e9
    }
  },
  
  "groups": [
    {
      "name": "basic_tests",
      "comment": "Core functionality validation",
      "tags": ["core", "smoke"],
      "defaults": "@hdawg8",
      
      "cases": [
        {
          "name": "nop",
          "code": "while(1);",
          "comment": "Minimal infinite loop"
        }
      ]
    }
  ]
}
```

---

## v2.0 Format Reference

### Top-Level Structure

```json
{
  "version": "2.0",
  "definitions": { ... },
  "imports": [ ... ],
  "groups": [ ... ]
}
```

#### `version` (string, optional)

Format version identifier. If omitted, v1.0 is assumed.

- `"2.0"` — enables hierarchical groups, imports, definitions, tags

#### `definitions` (object, optional)

Named compiler configuration templates for reuse.

```json
"definitions": {
  "hdawg8": {
    "devtype": "HDAWG8",
    "index": 0,
    "samplerate": 2.4e9
  },
  "shfqa4_qa": {
    "devtype": "SHFQA4",
    "sequencer": "qa"
  }
}
```

Reference definitions using `@name` syntax (see below).

#### `imports` (array of strings, optional)

List of manifest files to merge into this manifest. Paths are relative to the current manifest's directory.

```json
"imports": [
  "manifest-errors.json",
  "manifest-documentation.json",
  "groups/hdawg/basic.json"
]
```

Circular imports are detected and cause an error.

#### `groups` (array of group objects, optional)

Hierarchical test groups. Each group contains test cases and optional nested groups.

---

### Group Structure

```json
{
  "name": "group_name",
  "comment": "Human-readable description",
  "tags": ["tag1", "tag2"],
  "defaults": "@definition_name",
  "cases": [ ... ],
  "groups": [ ... ]
}
```

#### `name` (string, required)

Group name. Used in hierarchical test naming and filtering.

Example: Group `"hdawg_basic"` with test `"nop"` becomes:
```
hdawg_basic:nop[HDAWG8, sr=2.4e+09]
```

#### `comment` (string, optional)

Human-readable description of the group's purpose. Not used by test runners.

#### `tags` (array of strings, optional)

Tags applied to all tests in this group (in addition to each test's own tags).

```json
"tags": ["hdawg", "core", "smoke"]
```

Tests inherit group tags + their own tags + auto-generated device tags.

#### `defaults` (object or string, optional)

Default compiler arguments for all tests in this group.

**Direct definition:**
```json
"defaults": {
  "devtype": "HDAWG8",
  "samplerate": 2.4e9
}
```

**Reference:**
```json
"defaults": "@hdawg8"
```

Individual tests can override any default field.

#### `cases` (array of test case objects, optional)

Test cases in this group. See "Test Case Structure" below.

#### `groups` (array of group objects, optional)

Nested groups. Supports arbitrary nesting depth.

```json
"groups": [
  {
    "name": "waveforms",
    "tags": ["waveform"],
    "cases": [ ... ]
  }
]
```

Nested test names: `parent:child:testname[devtype, ...]`

---

### Test Case Structure

```json
{
  "name": "test_name",
  "code": "wave w = zeros(64); playWave(w);",
  "file": "relative/path/to/file.seqc",
  "comment": "Description of this test",
  "tags": ["tag1", "tag2"],
  
  "devtype": "HDAWG8",
  "index": 0,
  "options": "",
  "samplerate": 2.4e9,
  "sequencer": "qa",
  
  "@base": "@definition_name"
}
```

#### `name` (string, required)

Unique test name within the group.

Full hierarchical name is auto-generated:
```
group:subgroup:name[devtype, seq=..., sr=...]
```

#### Source Code (one of `code` or `file` required)

**`code` (string):** Inline SeqC source code.

```json
"code": "wave w = zeros(64);\nplayWave(w);"
```

**`file` (string):** Path to .seqc file, relative to manifest directory.

```json
"file": "hdawg_basic_loop.seqc"
```

Provide exactly one of `code` or `file`, not both.

#### `comment` (string, optional)

Human-readable description. Not used by test runners.

#### `tags` (array of strings, optional)

Test-specific tags (in addition to group tags and auto-tags).

```json
"tags": ["waveform", "dual-channel"]
```

**Auto-generated tags:**
- Device family: `hdawg`, `shfqa`, `shfsg`, `shfqc`, `uhf`, `lockin`
- Full device type: `hdawg8`, `shfqa4`, etc.

Filter by tags:
```bash
python3 diff_test_fast.py --tags hdawg,waveform
python3 diff_test_fast.py --exclude-tags slow
```

#### Compiler Arguments

**`devtype` (string, required unless inherited)**

Device type string. Examples:
- `"HDAWG8"`, `"HDAWG4"`
- `"SHFQA4"`, `"SHFQA2"`
- `"SHFSG4"`, `"SHFSG2"`, `"SHFSG8"`
- `"SHFQC"`
- `"UHFLI"`, `"UHFQA"`, `"UHFAWG"`
- `"GHFLI"`, `"VHFLI"`, `"SHFLI"`

**`index` (integer, optional, default: 0)**

AWG sequencer core index.

**`options` (string, optional, default: "")**

Device-specific options string.

**`samplerate` (float, optional)**

Sample rate in Hz. **Required for HDAWG devices.**

Example: `2.4e9` (2.4 GHz)

**`sequencer` (string, optional)**

Sequencer type. **Required for SHFQC devices.**

- `"qa"` — QA sequencer
- `"sg"` — SG sequencer
- `"auto"` — Auto-select

#### `@base` (string, optional)

Reference a definition and override specific fields.

```json
{
  "name": "test_on_core_1",
  "@base": "@hdawg8",
  "index": 1
}
```

Merges the referenced definition first, then applies local overrides.

---

### Multi-Device Expansion

Define a single test that runs on multiple devices:

```json
{
  "name": "basic_loop",
  "code": "while(1);",
  "devices": ["@hdawg8", "@shfqa4_qa", "@uhfli"]
}
```

**Expands to 3 tests:**
- `group:basic_loop_hdawg8[HDAWG8, sr=2.4e+09]`
- `group:basic_loop_shfqa4_qa[SHFQA4, seq=qa]`
- `group:basic_loop_uhfli[UHFLI]`

Each device spec can be:
- A definition reference: `"@hdawg8"`
- An inline object: `{"devtype": "HDAWG4", "samplerate": 2.4e9}`

Test-specific fields (like `code`, `file`, `comment`, `tags`) are shared. Device-specific fields are taken from each device spec.

---

### Reference Syntax

Use `@name` to reference a definition:

**In `defaults`:**
```json
"defaults": "@hdawg8"
```

**In `@base`:**
```json
{
  "@base": "@hdawg8",
  "index": 1
}
```

**In `devices`:**
```json
"devices": ["@hdawg8", "@hdawg4"]
```

**Undefined reference:** Error at load time.

---

## Filtering and Discovery

### List Groups

```bash
python3 diff_test_fast.py --list-groups
```

Output:
```
Groups:
  core                           1223 tests
  documentation_examples           92 tests
  error_validation                  6 tests
  labone_examples                  12 tests
  large_programs                   13 tests
```

### List Tags

```bash
python3 diff_test_fast.py --list-tags
```

Output:
```
Tags:
  core                           1223 tests
  documentation                    92 tests
  errors                            6 tests
  hdawg                           390 tests
  hdawg8                          302 tests
  shfqa                           148 tests
  ...
```

### Filter by Group

```bash
# Run only tests from specific groups
python3 diff_test_fast.py --groups core,error_validation

# Exclude specific groups
python3 diff_test_fast.py --exclude-groups large_programs
```

### Filter by Tag

```bash
# Run only tests with these tags
python3 diff_test_fast.py --tags hdawg,waveform

# Exclude tests with these tags
python3 diff_test_fast.py --exclude-tags slow,broken
```

### Combined Filtering

```bash
# hdawg tests from documentation group, excluding slow tests
python3 diff_test_fast.py --groups documentation_examples \
    --tags hdawg --exclude-tags slow
```

### Name Pattern Filter

```bash
# Run tests whose full name contains the pattern
python3 diff_test_fast.py --filter playZero
```

Pattern matches anywhere in the hierarchical name:
```
core:hdawg_playZero[HDAWG8, sr=2.4e+09]
```

---

## Migration from v1.0 to v2.0

### Existing v1.0 Manifests

**No migration required.** v1.0 manifests continue to work indefinitely.

The loader auto-detects format:
- Array `[...]` → v1.0
- Object without `"version"` → v1.0
- Object with `"version": "2.0"` → v2.0

### Optional Migration

If you want to use v2.0 features (groups, tags, imports, definitions):

1. **Add version field:**
   ```json
   {
     "version": "2.0",
     ...
   }
   ```

2. **Wrap tests in a group:**
   ```json
   {
     "version": "2.0",
     "groups": [
       {
         "name": "main",
         "cases": [ /* your v1.0 array here */ ]
       }
     ]
   }
   ```

3. **Extract common configurations:**
   ```json
   {
     "version": "2.0",
     "definitions": {
       "hdawg8": {"devtype": "HDAWG8", "index": 0, "samplerate": 2.4e9}
     },
     "groups": [
       {
         "name": "main",
         "defaults": "@hdawg8",
         "cases": [
           {"name": "test1", "code": "..."} // no need to repeat devtype, etc.
         ]
       }
     ]
   }
   ```

4. **Use imports for organization:**
   Split large manifests into topical files and import them.

---

## Examples

### Minimal v2.0 Manifest

```json
{
  "version": "2.0",
  "groups": [{
    "name": "smoke",
    "cases": [
      {
        "name": "nop",
        "code": "while(1);",
        "devtype": "HDAWG8",
        "samplerate": 2.4e9
      }
    ]
  }]
}
```

### Using Definitions

```json
{
  "version": "2.0",
  
  "definitions": {
    "hdawg8": {
      "devtype": "HDAWG8",
      "index": 0,
      "samplerate": 2.4e9
    }
  },
  
  "groups": [{
    "name": "basic",
    "defaults": "@hdawg8",
    "cases": [
      {"name": "nop", "code": "while(1);"},
      {"name": "playZero", "code": "playZero(32);"},
      {"name": "setTrigger", "code": "setTrigger(1); setTrigger(0);"}
    ]
  }]
}
```

All three tests inherit `devtype`, `index`, and `samplerate` from `@hdawg8`.

### Multi-Device Test

```json
{
  "version": "2.0",
  
  "definitions": {
    "hdawg8": {"devtype": "HDAWG8", "samplerate": 2.4e9},
    "uhfli": {"devtype": "UHFLI"}
  },
  
  "groups": [{
    "name": "cross_device",
    "cases": [
      {
        "name": "basic_loop",
        "code": "while(1);",
        "devices": ["@hdawg8", "@uhfli"]
      }
    ]
  }]
}
```

Creates two tests:
- `cross_device:basic_loop_hdawg8[HDAWG8, sr=2.4e+09]`
- `cross_device:basic_loop_uhfli[UHFLI]`

### Nested Groups

```json
{
  "version": "2.0",
  
  "groups": [{
    "name": "hdawg",
    "tags": ["hdawg"],
    "defaults": {"devtype": "HDAWG8", "samplerate": 2.4e9},
    
    "groups": [
      {
        "name": "waveforms",
        "tags": ["waveform"],
        "cases": [
          {"name": "playZero", "code": "playZero(32);"},
          {"name": "zeros", "code": "wave w = zeros(64); playWave(w);"}
        ]
      },
      {
        "name": "control_flow",
        "tags": ["control-flow"],
        "cases": [
          {"name": "while", "code": "while(1);"},
          {"name": "repeat", "code": "repeat(10) { playZero(32); }"}
        ]
      }
    ]
  }]
}
```

Test names:
- `hdawg:waveforms:playZero[HDAWG8, sr=2.4e+09]`
- `hdawg:waveforms:zeros[HDAWG8, sr=2.4e+09]`
- `hdawg:control_flow:while[HDAWG8, sr=2.4e+09]`
- `hdawg:control_flow:repeat[HDAWG8, sr=2.4e+09]`

Tags:
- `playZero` has tags: `["hdawg", "waveform", "hdawg8"]`
- `while` has tags: `["hdawg", "control-flow", "hdawg8"]`

### Imports

**File: `manifest.json`**
```json
{
  "version": "2.0",
  
  "definitions": {
    "hdawg8": {"devtype": "HDAWG8", "samplerate": 2.4e9}
  },
  
  "imports": [
    "manifest-errors.json",
    "manifest-documentation.json"
  ],
  
  "groups": [{
    "name": "core",
    "cases": [ /* core tests */ ]
  }]
}
```

**File: `manifest-errors.json`**
```json
{
  "version": "2.0",
  
  "groups": [{
    "name": "error_validation",
    "tags": ["errors"],
    "cases": [
      {"name": "err_undefined", "file": "err_undefined.seqc", "devtype": "HDAWG8", "samplerate": 2.4e9}
    ]
  }]
}
```

The main manifest loads tests from both files. Total test count = core + errors + documentation.

---

## Best Practices

### 1. Use Definitions for Common Configs

Instead of:
```json
{"name": "test1", "devtype": "HDAWG8", "samplerate": 2.4e9},
{"name": "test2", "devtype": "HDAWG8", "samplerate": 2.4e9},
{"name": "test3", "devtype": "HDAWG8", "samplerate": 2.4e9}
```

Use:
```json
"definitions": {"hdawg8": {"devtype": "HDAWG8", "samplerate": 2.4e9}},
"groups": [{
  "defaults": "@hdawg8",
  "cases": [
    {"name": "test1", "code": "..."},
    {"name": "test2", "code": "..."},
    {"name": "test3", "code": "..."}
  ]
}]
```

### 2. Group by Feature, Not by Device

Good:
```
groups/
  waveforms.json
  control_flow.json
  errors.json
```

Less good:
```
groups/
  hdawg.json (contains all hdawg tests)
  shfqa.json (contains all shfqa tests)
```

Use tags and multi-device expansion to test features across devices.

### 3. Use Tags for Cross-Cutting Concerns

```json
"tags": ["slow", "requires-hardware", "nightly-only"]
```

Then:
```bash
# Fast CI tests only
python3 diff_test_fast.py --exclude-tags slow,requires-hardware

# Nightly build
python3 diff_test_fast.py --tags nightly-only
```

### 4. Keep Manifests Focused

Split into ~5-15 files by feature/topic. Don't create hundreds of tiny files.

Good split:
- `manifest.json` (main, imports all)
- `manifest-errors.json`
- `manifest-documentation.json`
- `manifest-large.json`
- `manifest-labone.json`

### 5. Use Comments Liberally

```json
{
  "name": "edge_case_x",
  "comment": "Regression test for issue #123 - ensures waveform alignment with non-power-of-2 length",
  "code": "...",
  "tags": ["regression", "alignment"]
}
```

Future maintainers will thank you.

---

## Troubleshooting

### "Undefined reference: @foo"

You referenced `@foo` but it's not in `definitions`.

**Fix:** Add the definition or fix the typo.

### "Circular import detected"

Manifest A imports B, which imports A (or a longer cycle).

**Fix:** Remove one of the imports. Restructure into a hierarchy.

### "File not found: test.seqc"

The `file` path is relative to the manifest's directory.

**Fix:** Use correct relative path, or move the .seqc file.

### Tests not appearing in filtered output

Check:
1. Tag/group names are exact matches (case-sensitive)
2. `--exclude-*` isn't removing them
3. `--filter` pattern matches the full hierarchical name

Use `--list-groups` and `--list-tags` to see what's available.

### "provide either code or file, not both"

Test has both `"code"` and `"file"` fields.

**Fix:** Remove one.

---

## Reference: Complete v2.0 Schema

```json
{
  "version": "2.0",
  
  "definitions": {
    "<name>": {
      "devtype": "string",
      "index": 0,
      "options": "",
      "samplerate": 2.4e9,
      "sequencer": "qa"
    }
  },
  
  "imports": [
    "relative/path/to/manifest.json"
  ],
  
  "groups": [
    {
      "name": "string (required)",
      "comment": "string (optional)",
      "tags": ["string", ...],
      "defaults": "@ref or object (optional)",
      
      "cases": [
        {
          "name": "string (required)",
          "code": "string (or use file)",
          "file": "string (or use code)",
          "comment": "string (optional)",
          "tags": ["string", ...],
          
          "devtype": "string",
          "index": 0,
          "options": "",
          "samplerate": 2.4e9,
          "sequencer": "qa",
          
          "@base": "@ref (optional)",
          "devices": ["@ref or object", ...]
        }
      ],
      
      "groups": [ /* nested groups */ ]
    }
  ]
}
```
