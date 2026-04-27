# Incidental Findings

Discrepancies, suspicious patterns, and possible bugs discovered while
investigating differential test failures.  Items here are **not necessarily
the root cause** of the test that exposed them — they are leads that need
independent verification against the binary.

Each entry has:
- **Source**: which investigation surfaced it
- **Status**: open / confirmed / dismissed
- **Severity**: cosmetic / suspicious / likely-bug

---

## IF-1  `static_resources.cpp` comment/name confusion on device types

**Source**: RC-3 investigation (ZSYNC_DATA_RAW not found)
**Status**: open
**Severity**: suspicious (misleading, may cause future errors)

Lines 58-59 and 82 of `reconstructed/src/static_resources.cpp`:

```cpp
if (config.deviceType == UHFQA /*SHFSG*/ ||
    config.deviceType == UHFLI /*HD*/) {      // 0x1ec917
```

```cpp
if (config.deviceType == HDAWG /*Cervino/SHF*/) {   // 0x1ecd4c
```

Per `cervino_vs_hirzel.md`, UHFLI=1 is Cervino and HDAWG=2 is Hirzel.
But the inline comments say HDAWG is "Cervino/SHF" and UHFLI is "HD".
These comments are backwards relative to the device-type codenames.

**Question**: Are these just wrong comments, or could the enum names
themselves be swapped in the reconstruction (i.e. what we call `UHFLI`
is actually binary-value 2 = Hirzel)?  The enum values were verified
against `toAwgDeviceType` at 0x2cbd60 and look correct — UHFLI=1,
HDAWG=2 — so the comments are simply wrong.

**Action**: Clean up the comments to match the verified mapping.

---

## IF-2  `Resources` constructor stores grandparent, not parent, in `parent_`

**Source**: RC-3 investigation (ZSYNC_DATA_RAW scope-chain failure)
**Status**: confirmed against binary
**Severity**: likely-bug (causes scope-chain breakage)

Binary `Resources::Resources` at 0x1e3420:
1. Copies `weak_ptr` arg → `parentWeak_` at +0x40  (direct parent, weak)
2. Locks the weak_ptr (0x1e34f1)
3. If lock succeeds, reads `locked_parent->parent_` (+0x18/+0x20) and
   stores into `this->parent_` (+0x18/+0x20)

So `this->parent_` = **grandparent** (strong ref), while `parentWeak_`
= direct parent (weak ref).

Our reconstruction at `resources.cpp:84` had:
```cpp
: parent_(parent.lock())   // WRONG: sets parent_ = direct parent
```

This was changed to:
```cpp
if (auto p = parentWeak_.lock()) {
    parent_ = p->parent_;   // grandparent, matching binary
}
```

**Impact**: The previous code gave `parent_` the wrong semantics.
`getVariable` walks `parentWeak_` first, then `parent_`.  With the old
code, scope walking still works because `parent_` pointed to the direct
parent.  But the **lifetime semantics** are wrong: `parent_` is supposed
to keep the *grandparent* alive (strong ref) while the direct parent is
held only weakly.  This likely caused the expired `parentWeak_` seen in
the ZSYNC_DATA_RAW failure.

**Note**: The fix has been applied but not yet tested.  If `parentWeak_`
still expires after this fix, there is a separate lifetime issue.

---

## IF-3  `addConst` sets `flags = 0` — binary sets `flags = 1`

**Source**: RC-3 investigation
**Status**: confirmed against binary
**Severity**: likely-bug (fixed)

Binary `addConst` at 0x1e71c0: `movb $0x1, -0x38(%rbp)` sets the flags
field to 1 (VarFlag_Written) **after** the variant_assign call, just
before pushing the Variable into the vector.

Our reconstruction at `resources.cpp:524` had `v.flags = 0`.  This
causes `readConst(..., eOUT)` to throw "uninitialized variable" because
the flags check at 0x1e7da4 (`cmpb $0x0, 0x50(%r12)`) requires
`flags & 0xFF != 0`.

**Fix applied**: Changed to `v.flags = VarFlag_Written`.

**Note**: This was masked because tests that don't call `readConst` on
these constants still pass.  The bug only manifests when a function
actually reads a constant with direction `eOUT`.

---

## IF-4  `shared_ptr_deref` assertion on HDAWG nop via direct worker invocation

**Source**: RC-3 debugging (running compile_worker directly)
**Status**: open
**Severity**: suspicious

When running the compile worker directly (not through diff_test.py) for
an HDAWG nop (`// nop`), a GCC assertion fires:

```
shared_ptr_base.h:1344: _Tp* std::__shared_ptr_deref(_Tp*):
  Assertion '__p != nullptr' failed.
```

This doesn't manifest in the test harness (hdawg_nop passes).  It may be
a GCC libstdc++ debug assertion triggered by our use of libc++-ABI
shared_ptrs inside a libstdc++ build.  Or it may indicate a genuine null
shared_ptr dereference that happens to be non-fatal in release mode.

**Action**: Investigate whether this is an ABI mismatch artifact or a
real null-deref bug.

---

## IF-5  `getVariable` parent-walk has both `parentWeak_` and `parent_` paths

**Source**: RC-3 investigation
**Status**: open (needs binary verification)
**Severity**: suspicious

`Resources::getVariable` at 0x1eb0a0 walks parents via two mechanisms:
1. `parentWeak_.lock()` — direct parent (weak)
2. `parent_` — grandparent (strong)

Given IF-2 (parent_ = grandparent), the fallback to `parent_` means
`getVariable` can skip one scope level if the direct parent's weak ref
expires.  This is presumably intentional — it prevents the variable
lookup from failing entirely if the intermediate scope is gone.

**Question**: Does the binary's `getVariable` also have this dual path,
or is our reconstruction adding a fallback that doesn't exist?

**Action**: Disassemble 0x1eb0a0 and verify the parent-walk logic
matches our reconstruction at `resources.cpp:307-338`.

---

## IF-6  `Value` string ABI: 32-byte libstdc++ vs 24-byte libc++ union slot

**Source**: Prior session (RC-1 fix), re-encountered during struct layout review
**Status**: partially fixed (prior session)
**Severity**: known issue, workaround in place

The `Value` union was designed for libc++ 24-byte strings but we build
with libstdc++ which uses 32-byte strings.  Prior fix added padding in
the union.  However, the `Variable` struct's `name` field at +0x38 is
also a `std::string` — if libstdc++ makes it 32 bytes, the Variable
struct becomes 0x60 instead of 0x58 and `getVariable`'s linear scan
(which uses pointer arithmetic based on the binary's 0x58 stride) will
be wrong.

**Note**: This has NOT been checked.  If `sizeof(Variable) != 0x58`,
every variable lookup is broken.  The `static_assert` at line 264 of
`resources.hpp` has `|| true` making it a no-op.

**Note**: Verified: `sizeof(Variable) = 0x68` (vs binary 0x58),
`sizeof(Value) = 0x30` (vs binary 0x28), `sizeof(Resources) = 0xe8`
(vs binary 0xD8).  All due to libstdc++ 32-byte strings vs libc++
24-byte strings.  This is **not a bug for the reconstructed build**
because all code is recompiled with our layout.  It only matters if
binary-offset hardcoding crosses the boundary, which it doesn't.

**Action**: ~~Print `sizeof(Variable)` and compare against 0x58.~~
Verified; no action needed unless we mix binary and recon code.

---

## IF-7  `GlobalResources` ctor missing `parent_` assignment

**Source**: RC-3 investigation (ZSYNC_DATA_RAW scope-chain failure)
**Status**: fixed
**Severity**: likely-bug (was the actual root cause of RC-3)

`GlobalResources::GlobalResources` at 0x12a710:
- Calls `Resources("global", empty_weak_ptr{})` (base ctor with no parent)
- Then at 0x12a779-0x12a78b copies the `parent` shared_ptr arg into
  `this->parent_`

The reconstruction had the comment documenting this copy but the code
was missing.  Added `parent_ = parent;` in the constructor body.

**Impact**: Without this, GlobalResources had no parent chain at all.
StaticResources (where all builtin constants live) was unreachable from
any variable lookup.  This was the root cause of every "uninitialized
variable" error for builtin constants (ZSYNC_DATA_RAW, QA_GEN_ALL, etc.).

**Fix**: Applied in `global_resources.cpp`.  Tests went from 54 → 55.
`shfqa_startQA` now passes.

---

## IF-8  `checkFunctionSupported` error message format differs from binary

**Source**: shfqa_executeTable, shfqa_setPRNG test failures
**Status**: open
**Severity**: likely-bug

Original produces: `function 'X' not supported on SHFQA devices`
Recon produces: `function 'X' reaches end without returning a SHFQA`

The "reaches end without returning" message is a completely wrong error
path — this is not a return-type error, it's a device-support check.
The `checkFunctionSupported` function likely throws the wrong exception
or formats the wrong error message template.

---

## IF-9  `UnknownError47` (=47) has no entry in error message map; likely should be 48

**Source**: hdawg_executeTable `map::at` error
**Status**: open
**Severity**: likely-bug

`executeTableEntry` references `UnknownError47` (error code 47) at lines
2701 and 2706 of `custom_functions_io.cpp`.  But `error_messages.cpp`
has no `m[47]` entry.  It does have `m[48] = "Invalid constant argument
used for executeTableEntry"` which matches the semantics.

The binary addresses at 0x150e00 and 0x150e40 should be checked to
confirm whether the error code loaded is 47 or 48.

**Action**: Verify binary error code at those addresses.  If 48, rename
`UnknownError47` to `ExecTableInvalidConst` (=48) and update call sites.

---


## IF-10  `executeTableEntry` numeric-path bitmask: `& ~1` should be `& ~2`

**Source**: hdawg_executeTable `map::at` / const-path fallthrough
**Status**: confirmed against binary, fix applied
**Severity**: likely-bug (causes executeTableEntry to fail for literal integer args)

Binary at 0x150ffa: `and $0xfffffffd, %eax` → `argType & ~2`, matching
VarType 4 (Const) and 6 (Cvar).

Our reconstruction had `(argType & ~1) == 4` which matches 4 and 5
(Wave).  Since the const path at `argType == 4` is checked first, the
numeric path effectively only catches 5 (Wave) vs binary's 6 (Cvar).

Integer literals in SeqC produce `VarType_Cvar` (=6) through the
evaluator, so `executeTableEntry(0)` enters the const-matching path
(VarType 4 ≠ 6), skips register path (6 ≠ 2), then in binary hits
numeric path via `(6 & ~2) = 4 == 4`.  In recon, `(6 & ~1) = 6 ≠ 4`
→ falls through to error.

**Fix**: Changed to `(argType & ~2) == 4`.

---

## IF-11  Error message map indices 73/74 swapped

**Source**: RC-6 investigation (shfqa_executeTable)
**Status**: fixed
**Severity**: likely-bug

Binary's `checkFunctionSupported` at 0x15af29 uses `esi = 0x49` (=73) for
"function '%1%' not supported on %2% devices".  Our `error_messages.cpp`
had m[73] = "reaches end without returning" and m[74] = "not supported".
Swapped to match the binary.

**Fix**: Swapped m[73] and m[74] in `error_messages.cpp`.

---

## IF-12  Missing try/catch in SeqCFunctionCall::evaluate Path B

**Source**: RC-6 investigation (shfqa_executeTable)
**Status**: fixed
**Severity**: likely-bug

Binary's `SeqCFunctionCall::evaluate` has exception handlers covering
both Path A (user-defined function body) and Path B (custom function
call via `CustomFunctions::call()`).  Path B's handlers catch:
- selector 1 → `CustomFunctionsException` at 0x20fec6
- selector 2 → `CustomFunctionsValueException` at 0x20fe11
- selector 3 → `CompilerException` at 0x20fdf0

Our reconstruction only had try/catch around Path A (body eval).  Path B
was completely unwrapped, so `CustomFunctionsException` from
`checkFunctionSupported` would propagate to `compileSeqc` instead of
being caught and routed through `errorMessage(e.what(), lineNr_)`.

**Fix**: Wrapped Path B in try/catch for `CustomFunctionsException` and
`CompilerException`, calling `errorMessage()` with the node's lineNr_.

---

## IF-13  Spurious output-param overloads in AsmCommands/Prefetch headers

**Source**: RC-8 investigation (shfqa_setOscFreq symbol lookup failure)
**Status**: fixed
**Severity**: likely-bug (causes link-time undefined symbols)

The headers `asm_commands.hpp` and `prefetch.hpp` declared `void`
output-param overloads (e.g. `void addr(AsmList::Asm&, ...)`) for 10+
AsmCommands methods and 3 Prefetch methods.  These overloads do not
exist in the binary — the binary only has return-by-value forms.

The matching call sites in `prefetch_placesingle.cpp` (~35 sites) used
the non-existent output-param forms.  Since the reconstructed library
is a static archive, the linker silently tolerated the undefined symbols
until the shared module was loaded at runtime.

**Fix**: Removed all void overload declarations from headers.  Converted
all call sites to return-value form (e.g. `out = func(args)` or
`auto v = func(args); for (auto& a : v) list.append(a);` for vector
returns).

---

## IF-14  `mergeWaveforms` Phase 5 incorrectly gated on `multiValue`

**Source**: RC-1 investigation (playWave_zeros failure)
**Status**: fixed
**Severity**: likely-bug (caused all single-waveform playWave to fail)

The reconstruction had `if (multiValue) { ... getWaveformByName ... }`
around Phase 5.  Static disassembly analysis had concluded Phase 5 was
multi-value-only.  **GDB tracing proved this wrong**: the binary executes
Phase 5 unconditionally for both single and multi-value paths.  For
single-waveform playWave, Phase 5 finds the waveform by name (created
earlier by `wave w = zeros(64)`) and returns it directly, skipping
Phase 6 entirely.

**Fix**: Removed the `if (multiValue)` gate around Phase 5.

---

## IF-15  `getOrCreateWaveform` cache logic was inverted

**Source**: RC-1 investigation (playWave_zeros failure)
**Status**: fixed
**Severity**: likely-bug

The reconstruction had: found-in-set → cache hit (lookup), not-in-set →
factory call.  The binary's actual logic (verified by disassembly at
0x25bca0): not-in-set → try `getWaveformByFunDescr` first, and if that
fails, call factory; found-in-set → always call factory (re-create).

**Fix**: Restructured `getOrCreateWaveform` to match binary flow.

---

## IF-16  `grow()` used `readPositiveInt` with `minVal=1` but binary reads `toInt()` directly

**Source**: RC-1 investigation (playWave_zeros failure)
**Status**: fixed
**Severity**: likely-bug

The reconstruction's `grow()` called `readPositiveInt(args[1], ..., 1, "grow")`
which rejects value 0.  The binary at 0x260640 reads `args[1].toInt()`
directly and if the result is 0, returns immediately (no-grow needed).
This matters when `mergeWaveforms` appends `Value(0)` as the trailing
length hint.

**Fix**: Changed `grow()` to call `args[1].toInt()` directly and return
early when targetLen == 0.

---

## IF-17  Error message 96 (`ArgOverflow`) had wrong format string

**Source**: RC-1 investigation (boost::too_many_args crash)
**Status**: fixed
**Severity**: likely-bug

Message 96 was `"argument %1% of %2% is larger than the waveform length"`
(2 placeholders) but callers pass 3 arguments.  Binary-verified correct
format: `"argument %1% of %2% must be %3%"`.

**Fix**: Updated `error_messages.cpp` m[96].

---

## IF-18  `mergeWaveforms` Phase 4 uses stale vector pointer (binary latent bug)

**Source**: RC-1 investigation via GDB tracing
**Status**: confirmed (replicated in reconstruction)
**Severity**: cosmetic (binary works correctly despite the bug)

GDB tracing at 0x15e311-0x15e326 showed the binary's Phase 4 computes
`values.size()` using register `r12` which holds `values.begin()` from
Phase 1 — BEFORE Phase 3's `push_back` which may reallocate the vector.
The stale pointer causes the size computation to return the pre-append
count (number of waveform Values only, excluding the trailing Int).

For single-waveform cases this gives `multiValue = false` (count=1),
which is actually correct behavior — single waveforms should not take
the multi-value merge path.  The bug is benign because the resulting
control flow is correct.

**Fix**: Reconstruction captures `waveformCount = values.size()` before
Phase 3 append and uses it for the `multiValue` check.

---

## IF-19  Error message table globally off-by-one

**Source**: RC-9 investigation — `boost::too_few_args` in format calls
**Status**: fixed
**Severity**: critical (affected ALL error message lookups)

The entire `ErrorMessages::messages` table in `error_messages.cpp` was
shifted by +1: our key N contained the original binary's key (N-1).
Key 0 was missing entirely. Additionally, several local reorderings
existed (keys 12-15, 30-33, 39-43, 250-255).

**Root cause**: The original reconstruction derived the table from
rodata string references without verifying the actual key→value
mapping in the `std::map`. The off-by-one likely came from starting
enumeration at 1 instead of 0.

**Impact**: Every `ErrorMessages::format(ErrorMessageT(N), ...)` call
retrieved the wrong message. Most of the time this was harmless (wrong
text but same number of `%N%` placeholders). But when the shifted
message had fewer placeholders than the caller provided arguments,
`boost::format` threw `too_few_args`. This was the root cause of the
3 regression tests (hdawg_mixed_loops, hdawg_nested_if_loop,
hdawg_deep_nesting).

Also revealed that `hdawg_assignWaveIdx` was passing by accident —
the shifted table made `UnexpectedArgs` (key 150) return the message
for key 149 ("only const waveform index allowed"), which happened to
match the original binary's error. After the fix, the test correctly
fails (different error path taken).

**Fix**: GDB-extracted the original binary's full 254-entry table
from the `std::map` at BSS 0xb84c38 (using libc++ tree walk after
static init at 0xd5de0). Replaced the entire table in
`error_messages.cpp` to match exactly.

---

## IF-20: `updateWaveforms` second arg is `DC->hasDIO`, not hardcoded `false`

- **Source**: SHFQA regression (7 tests) after adding `setMemoryOffset(getNextSegmentAddress())`
- **Status**: fixed
- **Severity**: likely-bug (caused 7 test regressions)
- **Details**: Binary at 0x11e438: `movzbl 0x80(%r15), %edx` loads
  `DeviceConstants::hasDIO` (+0x80) as the second argument to
  `updateWaveforms`. Our code had `false` hardcoded. For SHFQA
  (`hasDIO=true`), this changes `allocFlag` to true inside
  `allocateWaveforms`, which sets `computedOffset=0` (the `fifoMode`
  branch), keeping `addressBase_=0` and thus `getNextSegmentAddress()=0`.
  Without this fix, SHFQA gets `computedOffset=0x1000` and wrong `e_entry`.
- **Fix**: `compiler.cpp:537` → `deviceConstants.hasDIO` instead of `false`

---

## IF-22: SeqCRepeat::evaluate generates runtime loop, NOT unrolling, when body is non-empty

- **Source**: Investigating prefetcher "invalid identifier" failures for repeat tests
- **Status**: open (major reconstruction error)
- **Severity**: likely-bug (causes 4 test failures + incorrect opcodes for multi-waveform)
- **Details**: Binary's `SeqCRepeat::evaluate` at 0x221c10 for `countInt >= 2`:
  1. Evaluates body once (first iteration) at 0x222ffc
  2. Checks if `firstBodyEmpty` (assemblers empty) at 0x223179-0x22318d
  3. If `firstBodyEmpty == true`: jumps to 0x222f7b (the countInt<2 unrolling path)
  4. If `firstBodyEmpty == false`: falls through to 0x2231d2 which generates a
     **runtime loop** using `addi(reg, 0, count)` + `loop()` node + body + `subi` + `brz`
  Our reconstruction at `seqc_ast_nodes_evaluate.cpp:8638` incorrectly ALWAYS
  unrolls (iterates body `countInt-1` more times in a C++ for-loop). The binary
  only unrolls when the body produces no assemblers (pure side-effects like
  variable assignments).
  
  This explains:
  - Prefetcher failures (4 tests): no Loop node in AST → no loop-related
    entries in AsmList → findPlaceholder can't find matching sequenceId
  - `hdawg_playWave_multi` wrong opcodes: sequential plays are unrolled but
    should be in a runtime loop too? (Actually multi doesn't use repeat —
    separate issue. But the general pattern applies.)
  
  Binary disassembly key addresses:
  - 0x2231c5: `test %r14b, %r14b; jne 222f7b` — branch on firstBodyEmpty
  - 0x2231e8: `AsmRegister(0)` — counter register
  - 0x2231ff: `Immediate(countInt)` — count value
  - 0x22321b: `call AsmCommands::addi` — initialize counter
  - Further code generates loop/subi/brz structure
  
  The fix requires reconstructing the entire runtime loop generation path
  from 0x2231d2 to ~0x223400 (approx 560 bytes of logic).

---

## IF-23: SeqCStmtList::evaluate sets hasError_=true unconditionally when childResult->node_ is non-null

- **Source**: Investigating why repeat unrolling breaks early
- **Status**: confirmed (informational — doesn't matter if repeat doesn't unroll)
- **Severity**: cosmetic (correct per binary; only matters for true unrolling paths)
- **Details**: Binary at 0x212e84-0x212e88: `movb $0x1, 0x30(%rax)` stores 1
  into `result->hasError_` (+0x30) unconditionally after the `setReturnValue`
  block. This block is only reached when `childResult->node_` is non-null
  (0x212c64 `je 212e99` skips it when node is null). Our code at
  `seqc_ast_nodes_evaluate.cpp:6280` sets it unconditionally regardless of
  whether the node chaining block was entered. This is mostly harmless because
  the repeat loop code doesn't actually unroll in the non-empty-body case.

---
