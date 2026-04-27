# Differential Testing

## Approach

The reconstructed `_seqc_compiler.so` is validated by **section-level ELF
comparison** against the original binary at repo root. Both modules are
loaded in **isolated subprocesses** (via `tests/compile_worker.py`) to
avoid symbol conflicts from having two copies of the same pybind11
module in one process.

### Test harness: `tests/diff_test.py`

1. For each test case in `tests/cases/manifest.json`:
   - Spawns a subprocess loading the **original** `_seqc_compiler.so`
   - Spawns a subprocess loading the **reconstructed** `_seqc_compiler.so`
   - Both call `compile_seqc(code, devtype, options, index, samplerate=…)`
2. Parses both outputs as ELF files (minimal 32/64-bit parser, no
   pyelftools dependency).
3. Compares:
   - ELF header fields (e_type, e_machine, e_entry)
   - Section names (must match exactly)
   - Section data (byte-for-byte SHA-256 comparison)
4. Reports PASS if all named sections have identical content, FAIL
   otherwise.

### Subprocess worker: `tests/compile_worker.py`

- Uses `RTLD_LAZY` to tolerate unresolved symbols from incomplete
  reconstruction (they crash only if actually called at runtime).
- Handles the return-type mismatch: original returns `(bytes, dict)`,
  reconstruction returns `(bytes, json_string)` — the worker normalises
  both to raw bytes + JSON metadata.

### Test case format: `tests/cases/manifest.json`

Each entry specifies:
- `name` — unique test identifier
- `code` or `file` — inline SeqC source or path to `.seqc` file
- `devtype` — device type string (e.g. `"HDAWG8"`, `"SHFQA4"`)
- `index` — AWG core index (0, 1, …)
- `samplerate` (optional) — sample rate in Hz
- `sequencer` (optional) — sequencer type (`"qa"`, `"sg"`)

## Current coverage (28 test cases, 2026-04-26)

### By device type

| Device | Cases | Notes |
|--------|-------|-------|
| HDAWG8 | 24 | index 0 (23) + index 1 (1) |
| SHFQA4 | 2 | sequencer=qa |
| SHFSG8 | 2 | sequencer=sg |

### By language feature

| Feature | Test cases |
|---------|------------|
| Empty program / nop | hdawg_nop, shfqa_nop, shfsg_nop |
| `playZero` | hdawg_playZero, hdawg_waitWave |
| `playHold` | hdawg_playHold |
| `wait` | hdawg_wait |
| `setTrigger` | hdawg_setTrigger, shfqa_basic |
| `waitWave` | hdawg_waitWave, hdawg_playHold |
| `var` declarations | hdawg_variable, hdawg_arithmetic, hdawg_assign_ops |
| `const` declarations | hdawg_const, hdawg_arithmetic |
| Arithmetic (+, -, *) | hdawg_arithmetic, hdawg_assign_ops |
| Comparison (==, <, >=, >) | hdawg_comparisons, hdawg_ternary |
| Logical (&&, \|\|, !) | hdawg_logical |
| Bitwise (&, \|, ^, <<, >>) | hdawg_bitwise |
| Unary (-, ~) | hdawg_unary |
| Increment/decrement (++, --) | hdawg_inc_dec |
| Ternary (?:) | hdawg_ternary |
| `for` loop | hdawg_for_loop, hdawg_mixed_loops |
| `while` loop | hdawg_while_loop, hdawg_nop |
| `do-while` loop | hdawg_do_while |
| `repeat` | hdawg_repeat_playZero, hdawg_nested_repeat, hdawg_mixed_loops, shfsg_loops |
| `if/else` | hdawg_if_else, hdawg_comparisons |
| Nested control flow | hdawg_nested_repeat, hdawg_mixed_loops |
| Multi-AWG index | hdawg_awg1 |
| Multi-device type | shfqa_basic, shfsg_loops |

## Known limitations

### ELF size gap

All tests pass section-content comparison, but overall ELF file sizes
differ consistently (reconstruction is ~10-30% smaller). This is due to
structural ELF differences — likely section alignment, padding, or
section header table placement — not code differences. The ELFIO library
version and configuration used by the reconstruction may produce
different structural layout than the original binary's ELFIO build.

Example: `hdawg_for_loop` orig=1644 bytes, recon=1320 bytes, but all
named sections have identical content.

### Return type mismatch

The pybind11 binding returns `(bytes, json_string)` where the original
returns `(bytes, dict)`. The test worker handles this by comparing only
the ELF bytes. The JSON metadata comparison is not yet implemented.

## Potential future developments

### More test cases

**Language features not yet tested:**
- `switch/case` statements
- User-defined functions (`void foo() { … }` + `foo();`)
- `cvar` (compile-time variables with runtime fallback)
- String variables and operations
- Waveform operations (`wave w = zeros(1024); playWave(w);`)
- Waveform arithmetic (`wave w = w1 + w2;`, `scale(w, 0.5)`)
- `assignWaveIndex` (command table entries)
- DIO/ZSync playback (`playWaveDIO`, `playWaveZSync`)
- `setInt`/`setDouble` (node I/O via writeToNode)
- Oscillator functions (`setSinePhase`, `resetOscPhase`, `setOscFreq`)
- QA functions (`startQA`, `startQAResult`, `executeTableEntry`)
- PRNG functions (`setPRNGSeed`, `setPRNGRange`, `getPRNGValue`)
- Feedback processing (`configureFeedbackProcessing`, `getFeedback`)
- `generate` (user-defined waveform generation)
- Nested function calls / recursive patterns
- `return` statements within functions
- Error cases (compile-time errors should match between original and reconstruction)

**Additional device types:**
- UHFLI, UHFQA (Cervino family)
- SHFQC (dual-sequencer: qa + sg)
- GHFLI, VHFLI (newer Cervino-like)
- HDAWG4 (4-channel variant)

**Edge cases:**
- Maximum nesting depth
- Large loop counts (optimizer stress)
- Empty loops / dead code
- Multiple AWG indices on same device
- Programs that produce warnings (non-fatal compilation messages)

### Metadata comparison

The current harness only compares ELF bytes. The compilation also
returns metadata (JSON dict / string) containing compiler messages,
waveform info, etc. A future enhancement would compare this metadata
field-by-field, which would catch differences in:
- Warning messages (text and line numbers)
- Waveform memory allocation info
- Node access lists

### ELF byte-identical matching

Closing the ELF size gap would allow byte-identical comparison (the
strongest possible validation). This likely requires:
- Matching the ELFIO version and build configuration used by the
  original binary
- Matching section alignment settings in `ElfWriter::prepareHeader()`
- Potentially matching the section header table offset calculation

### Error case testing

Programs that should fail compilation can also be differentially tested:
both compilers should produce the same error message. The harness already
supports this (compares error strings when both fail). Useful error cases:
- Undefined variable references
- Type mismatches (`var * var`)
- Unsupported device functions (`waitWave` on SHFQA)
- Syntax errors

### Performance regression testing

For large programs, track compilation time as a secondary metric. Not
critical for correctness but useful for catching algorithmic regressions
in the optimizer or prefetcher.

---

## Bug-fixing discipline

**Critical rule: when a differential test reveals a failure, do NOT fix
the reconstructed code based on C++ reasoning alone.** The reconstruction
is a reverse-engineered replica of a binary — if we "fix" something
based on what we think the code should do rather than what the binary
actually does, we risk diverging from the original. Divergence compounds:
each unverified fix can mask a real layout or logic error that surfaces
later in a harder-to-diagnose way.

### Required workflow for every test failure fix

1. **Reproduce**: Run the failing test case and capture both error
   messages (original + recon).
2. **Hypothesize**: Form a theory about the root cause from the recon
   source code and the error symptoms.
3. **Verify against binary**: Disassemble the relevant function(s) in
   the original binary to confirm the hypothesis. Check:
   - Struct layouts and field offsets (are they what we assumed?)
   - Control flow (does the branch go where we think?)
   - ABI details (calling convention, return types, union sizes)
4. **Implement fix**: Only after binary confirmation, apply the fix to
   the reconstructed source. Cite the binary address(es) that justify
   the change.
5. **Test**: Re-run the failing test case(s) to confirm the fix.

### Why this matters

- A `free(): invalid size` crash might look like a "use std::string
  instead of char[24] in a union" fix. But the real question is: what
  does the binary's layout look like at the relevant offset? The answer
  might be "the union is 24 bytes because the binary uses libc++
  (24-byte string)" and the real fix is something else entirely (e.g.
  a missing copy in a different struct, or a wrong field offset
  upstream).
- Changing struct sizes without binary verification can break other
  code that depends on the layout (e.g. serialization, reinterpret_cast
  byte offsets, cross-TU access patterns).
- The differential test approach is only valuable if we maintain
  faithfulness to the binary. Once we start making "logical" fixes
  untethered from disassembly evidence, we lose that guarantee.
