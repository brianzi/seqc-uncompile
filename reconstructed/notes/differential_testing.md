# Differential Testing {#notes_differential_testing}

\note **Reverse-engineering reference material.** This page is part of
the `reconstructed/notes/` set: deep-dive technical notes for
contributors working on the reconstruction. It cites binary addresses,
opcodes, and disassembly observations directly so they remain
discoverable from the rendered site. The standard documentation-voice
rules for API briefs (no binary citations outside `\binarynote`) do
**not** apply to this page.

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

## Known limitations

### Return type mismatch

The pybind11 binding returns `(bytes, json_string)` where the original
returns `(bytes, dict)`. The test worker handles this by comparing only
the ELF bytes. The JSON metadata comparison is not currently exercised
by the harness.

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
