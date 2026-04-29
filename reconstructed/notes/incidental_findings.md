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

## IF-23 — SeqCAssign Row 1 (Var=Const/Cvar) missing ADDI + SetVarPlaceholder
- **Source**: hdawg_arithmetic differential failure; GDB trace of binary
- **Status**: **fixed** (2026-04-27)
- **Severity**: critical (root cause of ~25+ test failures)
- **Detail**: SeqCAssign::evaluate Row 1 (lhsType=Var, rhsType ∈ {Const,Cvar})
  was missing the `addi(lhsReg, R0, constValue)` + `asmSetVarPlaceholder(lhsReg)`
  emission after `updateVar(name)`. Binary address range: `0x24400a-0x24549d`.
  GDB trace proved the binary calls `addi(AsmRegister, AsmRegister, Value)` at
  `0x245364` (via `0x27a020`) followed by `asmSetVarPlaceholder` at `0x24549d`.
  The reconstruction had a comment "Row 1: no asm copy" which was incorrect.
  Fix: emit `ctx.asmCommands->addi(lhsReg, AsmRegister(0), rhsResult.getValue())`
  and `ctx.asmCommands->asmSetVarPlaceholder(lhsReg)` into result->assemblers_.
  This produces the `addi Rn, R0, <value>` instructions for `var x = <const>;`.

## IF-24 — Hirzel ssl/ssr operand order swapped
- **Source**: hdawg_arithmetic differential failure; binary encoding comparison
- **Status**: **fixed** (2026-04-27)
- **Severity**: medium (affected all multiply-by-power-of-2 and shift operations)
- **Detail**: `AsmCommandsImplHirzel::ssl()` and `ssr()` had `regSrc=reg, regDst=R0`
  but the binary encodes `regDst=reg, regSrc=R0`. Verified by comparing binary
  encoding: original `0x62000005` = `ssl R2, R0` (regDst=2, regSrc=0) vs recon
  `0x60200005` = `ssl R0, R2` (regDst=0, regSrc=2). Fix: swap the fields.

---

## IF-25  `registerAllocation` overlap check was one-directional

**Source**: hdawg_comparisons register off-by-1 investigation
**Status**: fixed
**Severity**: likely-bug → fixed

The recon's overlap check only tested `physRange.back() >= virtRange.front()`,
which is half of a proper interval overlap test. The binary (at `0x27f94f-0x27f95b`
+ `0x27fb40-0x27fb4c`) performs a bidirectional check:
`physRange.front() <= virtRange.back() AND physRange.back() >= virtRange.front()`.

Additionally, the recon appended live range indices unsorted during merge
(`push_back` without sorting), causing the flat physRange vector to lose
ordering. Added `std::sort` after each merge.

Binary data structure is actually `vector<vector<vector<int>>>` (nested),
not flat — each physRegs[preg] accumulates separate range vectors. The
flat+sorted approach produces equivalent results for the overlap check.

**Fix**: `asm_optimize.cpp` lines ~1003-1010 (bidirectional check) and
~1057 (sort after merge).

---

## IF-26  `wvfs` (Hirzel) stored register in `regDst` instead of `regSrc`

**Source**: GDB comparison of binary asm state at registerAllocation entry
**Status**: fixed
**Severity**: likely-bug → fixed

Binary `AsmCommandsImplHirzel::wvfs` at `0x27d071` stores the register
parameter at `-0x90(%rbp)` = AssemblerInstr offset +0x20 = `regSrc`.
The recon had `result.assembler.regDst = reg` (wrong field).

**Fix**: `asm_commands_impl_hirzel.cpp` line 84: changed `regDst` to `regSrc`.

---

## IF-27  `playHold` passed `isHold=true, isBool=false` — binary has opposite

**Source**: hdawg_playHold `wvfs 0` vs `wvfs 1` mismatch
**Status**: fixed
**Severity**: likely-bug → fixed

Binary `playHold` at `0x1391b8-0x1391d7` passes: `isHold=false` (r8d=0),
`isBool=true` (push $0x1). The recon had these swapped.

Since `config.hold = isBool` (verified at `genPlayConfig` 0x278b23), and
`dummyType = config.hold` in `prefetch_placesingle.cpp:208`, the isBool
value directly controls the wvfs first argument.

**Fix**: `custom_functions_playback.cpp` line 990-994: changed
`isHold=true, isBool=false` to `isHold=false, isBool=true`.

---

## IF-28  `removeUnusedRegs` always invalidated `regDst`, ignoring fallback to `regAux`

**Source**: hdawg_many_vars investigation (instruction count mismatch)
**Status**: fixed
**Severity**: likely-bug → fixed

Binary at 0x27eaba-0x27eabe: when `regDst` is invalid or == AsmRegister(0),
the code falls back to `regAux` as the dest slot. When eliminating a
write-only register, it invalidates whichever slot was *selected* (stored
at `[rbp-0x58]`), not always `regDst`. The reconstruction always
invalidated `regDst`, which left stale `regAux` references that prevented
subsequent optimizations from removing dead instructions.

**Fix**: `asm_optimize.cpp` lines 568-572, 654: track `destSlot` pointer
and invalidate the selected slot.

---

## IF-29  Missing `asmSetVarPlaceholder` in SeqCAssign Row 2 (Var=Var)

**Source**: hdawg_many_vars (591 vs 543 instruction diff)
**Status**: fixed
**Severity**: likely-bug → fixed

Binary at 0x24495a: after compound expression assignments like `var f = a + b`,
the binary emits an INVALID (cmd=0xFFFFFFFF) separator via
`AsmCommands::asmSetVarPlaceholder(lhsReg)`. This was present in Row 1
(Var=Const) but missing in Row 2 (Var=Var). The missing separator caused
the optimizer to merge instructions across assignment boundaries.

**Fix**: `seqc_ast_nodes_evaluate.cpp` line 3200: added
`asmCommands->asmSetVarPlaceholder(lhsReg)` call.

---

## IF-30  `SeqCStmtList::evaluate` set `hasError_=true` on normal child nodes

**Source**: hdawg_deep_nesting (718 vs 694 — while loop body prematurely terminated)
**Status**: fixed
**Severity**: likely-bug → fixed

The reconstruction had `hasError_ = true` inside the node-chaining block
(whenever `childResult->node_` was non-null). The binary (0x212800) has
two completely separate paths: childHadError==0 does name-building +
node-chaining only; childHadError==1 does return-value extraction +
hasError_=true + break. GDB verified at 0x212e88 (`movb $0x1,0x30(%rax)`)
that this instruction never fires for normal (non-error) children during
hdawg_deep_nesting compilation.

**Fix**: `seqc_ast_nodes_evaluate.cpp` lines 6265-6322: rewrote to match
binary's two-path structure.

---

## IF-31  `setUserReg` used `value_.toInt()` for Var-type argument instead of `reg_`

**Source**: hdawg_full_program (751 vs 485 instruction diff)
**Status**: fixed
**Severity**: likely-bug → fixed

Binary at 0x14a691: for Var-type arg1, `setUserReg` reads the register
from `EvalResultValue.reg_` (offset +0x30), not from `value_.toInt()`.
For Var types, `value_` is left as Unspecified (default Value), so
`toInt()` returns 0, causing the wrong register to be emitted.

**Fix**: `custom_functions_io.cpp` line 2107: changed
`AsmRegister(arg1.value_.toInt())` to `arg1.reg_`.

---

## IF-32  Lexer missing `endLineComment()` on newline

**Source**: Linux-specific `//` comment parsing failure
**Status**: fixed
**Severity**: critical

`seqc_lexer.c` rule 77 was missing `endLineComment()` on newline.
`//` comments on Linux silently discarded all subsequent tokens because
the comment state was never terminated at end-of-line.

**Fix**: Added `endLineComment()` call in lexer rule 77 newline action.

---

## IF-33  `SeqCValue::payload_` sized for libc++ string (24B), not libstdc++ (32B)

**Source**: "bad variant access" on any string literal
**Status**: fixed
**Severity**: critical

`SeqCValue::payload_` was 24 bytes (libc++ `std::string` size) but
libstdc++ `std::string` is 32 bytes. The overflow corrupted the `tag_`
field, causing `std::bad_variant_access` on any string literal.

**Fix**: Replaced with union-based storage to accommodate both ABIs.

---

## IF-34  `EvalResultValue` destructor double-free via explicit `value_.~Value()`

**Source**: crash on destruction of EvalResultValue containing a Value
**Status**: fixed
**Severity**: high

`EvalResultValue` destructor explicitly called `value_.~Value()`, but
the compiler auto-destructs members — causing a double-free.

**Fix**: Removed the explicit destructor call.

---

## IF-35  `assignWaveIndex` used `config.channelsPerGroup[0]` instead of `config.deviceIndex`

**Source**: vector OOB in assignWaveIndex for HDAWG8
**Status**: fixed
**Severity**: high

`assignWaveIndex` used `config.channelsPerGroup[0]` (=2 for HDAWG8) as
index into `playArgs.waveAssignments_` vector. Should be
`config.deviceIndex` (=0). Caused vector out-of-bounds access.

**Fix**: Changed index to `config.deviceIndex`.

---

## IF-36  `readInt` enforced `minVal` as threshold — binary uses it as error arg index

**Source**: `marker(64, 1)` failure
**Status**: fixed
**Severity**: medium

`readInt` in waveform_generator enforced `result < minVal` check, but
binary analysis shows `minVal` is only an argument index for error
reporting, not a minimum threshold. This broke `marker(64, 1)`.

**Fix**: Removed the `result < minVal` check.

---

## IF-37  `ldiotrig()` placed address constant in `immediates` instead of `outputs`

**Source**: reversed operand rendering: `ld 104, R1` instead of `ld R1, 104`
**Status**: fixed
**Severity**: high

`ldiotrig()` in both Hirzel and Cervino `asm_commands` placed the
address constant in `immediates` instead of `outputs`, causing reversed
operand rendering in the output.

**Fix**: Moved address constant to `outputs` in both implementations.

---

## IF-38  `setSinePhase`/`incrementSinePhase` wrong node paths for SHFSG

**Source**: SHFSG sine phase test failures
**Status**: fixed
**Severity**: medium

Used wrong node paths for SHFSG: `/sines/16/phaseshift/value` instead
of `sgchannels/<idx>/sines/0/phaseshift`.

**Fix**: Corrected node path generation for SHFSG device type.

---

## IF-39  `static_resources.cpp` registered `QA_DATA_PROCESSED` but lookup uses `QA_DATA_PROCESSED_D`

**Source**: "uninitialized variable" error for QA_DATA_PROCESSED_D
**Status**: fixed
**Severity**: low

`static_resources.cpp` registered the constant as `QA_DATA_PROCESSED`
but code in `custom_functions_io.cpp` looks up `QA_DATA_PROCESSED_D`.

**Fix**: Changed registered constant name to `QA_DATA_PROCESSED_D`.

---

## IF-40  `executeTableEntry` register-path used `arg0.value_.toInt()` instead of `arg0.reg_`

**Source**: executeTableEntry with register-type argument
**Status**: fixed
**Severity**: medium

`executeTableEntry` with register-type arg called `arg0.value_.toInt()`
instead of using `arg0.reg_`. Same class of bug as IF-31.

**Fix**: Changed to use `arg0.reg_`.

---

## IF-41  3-arg `sine(length, amp, phase)` formula mismatch with binary

**Source**: waveform comparison for `sine(64, 1.0, 0.0)`
**Status**: open/suspicious
**Severity**: medium

Waveform generator 3-arg `sine(length, amp, phase)` uses default
`nPeriods=1.0` and formula `amp * sin(2π*nPeriods*i/length + phase)`.
But the original binary produces `sin(amp)*32767` as a constant for
`phase=0` — suggesting different argument semantics or formula in the
3-arg case. Needs binary disassembly of the 3-arg code path.

---

## IF-42  User-defined function parsing had 3 bugs

**Source**: user-defined function test failures
**Status**: fixed
**Severity**: high

Three bugs in user-defined function parsing/evaluation:
1. Wrong arg mapping in `createFunction`: used `type_specifier` instead
   of `function_declarator` for name/params.
2. `SeqCFunction::evaluate` did unnecessary `dynamic_cast<SeqCOperation*>`
   that failed for single-param functions.
3. Function body evaluated with outer scope instead of function's own scope.

**Fix**: All three issues corrected in parser and evaluator.

---

## IF-43  Cervino (UHFLI) waveform playback wrong code path in prefetcher

**Source**: Cervino playback test failures
**Status**: partially-fixed
**Severity**: high

`prefetch_placesingle.cpp` had a completely wrong code path for Cervino
(UHFLI) waveform playback: used smap/ssl/addr/wwvf instead of unified
register selection + shared wvfImpl path.

**Partially fixed**: main code path corrected. Remaining issues:
- Wrong waveform base address (0x2000 vs 0xd0001000)
- Missing prefetch instructions
- Swapped wvf register order

---

## IF-44  genericTriangle parameter order swapped in reconstruction

**Source**: hdawg_wave_misc failure (sawtooth/triangle produce non-zero for 3-arg with phase=0)
**Status**: fixed
**Severity**: high (wrong output)

The reconstructed `genericTriangle` had parameter order `(length, amplitude,
riseRatio, phase, period)` but the binary's ABI (GDB-verified) is
`(length, amplitude, nPeriods, riseRatio, phase)` — xmm0=amp, xmm1=nPeriods,
xmm2=riseRatio, xmm3=phase.

Additionally, the 3-arg sawtooth/triangle call sites swap arguments vs the
4-arg path: 3-arg passes `(nPeriods_default, user_phase, riseRatio, amplitude)`
while 4-arg passes `(amplitude, nPeriods, riseRatio, phase)`. This is because
the 3-arg code path stores values in different stack slots that get loaded into
different xmm registers at the common call site.

---

## IF-45  double2awg16 NaN handling: std::max vs maxsd semantics

**Source**: hdawg_wave_misc failure (NaN samples produce 0x0001 instead of 0x0000)
**Status**: fixed
**Severity**: high (wrong waveform data)

`double2awg16` used `std::max(-1.0, sample)` which returns -1.0 when sample is
NaN (comparison with NaN is false, std::max returns first arg). The binary uses
x86 `maxsd` instruction which returns the second source operand (the NaN) when
either operand is NaN. Fixed with inline asm `maxsd`.

---

## IF-46  Waveform name truncation suffix format

**Source**: hdawg_wave_vect failure (metadata mismatch in .waveforms section)
**Status**: fixed
**Severity**: medium (metadata-only)

Reconstruction used `", ..."` as the truncation suffix for long vect() names.
Binary uses `" ...)"` (space-dots-closeparen). The close paren is part of the
original function call syntax being preserved.

---

---

## IF-47  asmPrefetch did not store waveform name in node->wavesPerDev

**Source**: hdawg_prefetch failure (PlainLoad node missing from output)
**Status**: fixed
**Severity**: likely-bug

`asmPrefetch()` in `asm_commands.cpp` had a comment "Copy waveform name into
node->wavesPerDev[nameIndex]" but no actual code. The sister function
`asmLockPlaceholder()` does store the name. Without the name, the PlainLoad
handler in `placeSingleCommand` could not look up the waveform and bailed out.

---

## IF-48  PlainLoad handler checked cachePtr instead of registerHirzel

**Source**: hdawg_prefetch failure (PlainLoad node present but not emitting code)
**Status**: fixed
**Severity**: likely-bug

The reconstructed PlainLoad (0x4000) handler in `prefetch_placesingle.cpp`
checked `nodeStates_[node].cachePtr == nullptr` and returned early. GDB
disassembly of the binary at 0x1d7f68 confirmed the handler checks
`registerHirzel.isValid()` instead — PlainLoad nodes never get a cachePtr
allocated (the `allocate()` pass skips type 0x4000). Also, the handler
emitted cwvf instructions which the binary does not — it only emits
`addi + prf`. The prf size comes from `waveform->allocationByteSize`,
not from `cachePtr->size_`.

---

## IF-49  play() SubFunc==0 fell through to asmPlay

**Source**: hdawg_prefetch failure (extra Play node in recon tree)
**Status**: fixed
**Severity**: likely-bug

In `custom_functions_play.cpp`, SubFunc==0 (the Prefetch path) called
`asmPrefetch` but discarded its result, then fell through to `asmPlay`
which created an unwanted Play node. The binary only creates the PlainLoad
node for prefetch — no Play node. Fix: capture the `asmPrefetch` result,
push it to `results->assemblers_` and chain its node into `results->node_`,
then skip the `asmPlay` code path.

---

## IF-50  WVFE register field: regDst vs regSrc

**Source**: hdawg_playWaveIndexed register allocation mismatch
**Status**: fixed
**Severity**: likely-bug

The single-register WVFE opcode (Hirzel-specific) stores the waveform
register in `regSrc` (as a read source), not `regDst`. GDB confirmed:
binary WVFE instructions have `regDst=R0, regSrc=Rn`. The recon had
`regDst=Rn, regSrc=R-1` which caused the register allocator to treat
the waveform register as a write, producing different live ranges and
incorrect register allocation. Fixed in `asm_commands_impl_hirzel.cpp`.

---

## IF-51  BFS Load-merge missing child enqueueing

**Source**: hdawg_playWaveIndexed waveform register reuse failure
**Status**: fixed
**Severity**: likely-bug

In `prefetch.cpp`, `moveLoadsToFront`'s BFS loop merges duplicate Load
nodes with the same waveform name. After a merge+updateParent, the recon
did `continue` (skipping child enqueueing), but the binary falls through
to the child-enqueueing code at 0x1cd0f0. This meant nodes inside
branches beyond the merged Load were never visited, preventing
register sharing for waveforms played multiple times (e.g. same waveform
in an if-else branch). Fixed by removing the `continue`.

---

## IF-52  linenr preamble metadata (uhfqa_startQA 2-byte diff)

**Source**: uhfqa_startQA investigation
**Status**: open
**Severity**: cosmetic

Preamble sync instructions (`st R0, 68/69`) have `lineNumber=4`
(source line count) in binary but `0` in recon. Root cause is the
SyncCervino node's placeholder inheriting `wavetableFront` from
`wavetableFrontIndex_` which equals the source line count when the
SetVar placeholder is processed, but 0 when the Load placeholder is
processed. Low priority — doesn't affect code semantics.

---

## IF-53  Cervino waveform addressing (uhfli_playback)

**Source**: uhfli_playback investigation
**Status**: fixed
**Severity**: bug

Three issues in uhfli_playback:
1. Missing Cervino prefetch instructions (addiu + prf + wprf)
2. Waveform register order reversed (wvf R1,R0 vs wvf R0,R1)
3. Enormous `.dd_` padding section (768MB) due to
   `waveform->addressValue` being 0x30000000+ for UHFLI.
All three are likely related to deep Cervino-specific waveform
addressing logic in the prefetch system.

## IF-54  Cervino wvf/wvfi register encoding swapped

**Source**: uhfli_playback fix
**Status**: fixed
**Severity**: bug

In `asm_commands_impl_cervino.cpp`, the `wvf` and `wvfi` instructions
had regSrc and regAux swapped in the encoding.  Fixed by swapping the
operand order to match the binary.

## IF-55  Phase 1 & 2 lambda sizeInBlocks: std::min vs std::max

**Source**: uhfli_playback / wavetable_ir.cpp investigation
**Status**: fixed
**Severity**: bug

The `sizeInBlocks` formula in Phases 1 and 2 of `mergeWaveforms` used
`std::min` but the binary uses `cmova` (conditional move if above),
which corresponds to `std::max`.  Fixed in `wavetable_ir.cpp`.

## IF-56  computedOffset formula: totalSamples*32 vs waveCount*32

**Source**: uhfli_playback / wavetable_ir.cpp investigation
**Status**: fixed
**Severity**: bug

The `computedOffset` formula multiplied `totalSamples * 32` but the
binary multiplies `waveCount * 32`.  Fixed in `wavetable_ir.cpp`.

## IF-57  Assignment evaluator: SetVar node not linked into node tree

**Source**: uhfqa_startQA / seqc_ast_nodes_evaluate.cpp investigation
**Status**: fixed
**Severity**: bug

Two bugs in the assignment evaluator (SeqCAssign::evaluate):

1. `asmSetVarPlaceholder()` created nodes with `NodeType::SetVarPlaceholder`
   (0x10000) but the binary uses `NodeType::SetVar` (0x10). Fixed in
   `asm_commands.cpp:913`.

2. After pushing the SetVar placeholder into `result->assemblers_`, the
   recon did not set `result->node_ = placeholder.node`. The binary's
   convergence code at 0x245535 does `movups %xmm0, 0x38(%rbx)` which
   writes the placeholder's node shared_ptr into `result->node_` (offset
   +0x38). Without this, the StmtList evaluator's node chaining loop
   never linked the SetVar node into the tree, so `fillInPlaceholders` /
   `placeSingleCommand` never processed it, leaving `wavetableFrontIndex_`
    at 0 instead of 4. Fixed in `seqc_ast_nodes_evaluate.cpp` for both
   Row 1 (line 3180) and Row 2 (line 3201).

## IF-58  SeqCFunction::evaluate uses type() instead of varType() for return type

**Source**: hdawg_return_func / hdawg_nested_func test failures
**Status**: fixed
**Severity**: bug

`SeqCFunction::evaluate` at `seqc_ast_nodes_evaluate.cpp:9647` used
`retType()->type()` to get the return VarType, but `type()` returns
`lineNr_` (+0x0C). The binary reads `varType_` at +0x14 via
`retType()->varType()`. This caused user functions to be registered
with `VarType_Void`, making all var-returning functions fail.

## IF-59  Resources constructor: returnReg_ initialized as Invalid instead of R0

**Source**: hdawg_return_func / hdawg_nested_func test failures
**Status**: fixed
**Severity**: bug

`resources.cpp:90` initialized `returnReg_` as `AsmRegister::Invalid()`
= `{-1, false}`. The binary initializes it as `AsmRegister(0)` =
`{0, true}` (xor %esi,%esi + call AsmRegister(int)). During function
definition, `SeqCReturnStatement::evaluate` calls `getReturnReg()` which
returns this default, causing "addi without valid register" with the
invalid default.

## IF-60  SeqCSwitchCase: __switch_jump_table should be AWG_WAIT_TRIGGER

**Source**: hdawg_switch_case test failure
**Status**: fixed
**Severity**: bug

All three `readConst()` calls in `SeqCSwitchCase::evaluate` (lines 7646,
7699, 7747) used the string `"__switch_jump_table"` — a variable that
doesn't exist. The binary at 0x219511/0x218b80 constructs an inline SSO
string from .rodata containing `"AWG_WAIT_TRIGGER"` (a pre-registered
constant from static_resources.cpp). Fixed by replacing all 6 occurrences.

## IF-61  WaveAssignment copy ctor: wrong Value variant discriminator mapping

**Source**: hdawg_play_dual_ch / hdawg_cmd_table / uhfawg_combo crashes
**Status**: fixed
**Severity**: bug (SIGSEGV)

`custom_functions.cpp:788` WaveAssignment copy constructor had wrong
variant mapping: case 0→int, 1→double, 2→string. Correct mapping is
0→int, 1→bool, 2→double, 3→string. When a wave variable (which_==3,
string waveform name) was copied, it hit the default case and storage
was zero-filled. Later toString() on the zeroed storage → SIGSEGV.
Fixed to use `(which_ >> 31) ^ which_` decoding with correct thresholds.

## IF-62  playZero/playHold missing VarType_Var dispatch

**Source**: hdawg_ternary / shfsg_misc_funcs "unspecified value type" error
**Status**: open (needs binary disassembly at 0x1387f0)
**Severity**: bug

`custom_functions_playback.cpp:937` unconditionally calls
`args[0].value_.toInt()`. When the argument is a runtime Var (e.g. from
a ternary `(c > 0) ? 32 : 64`), the Value has type Unspecified. The
original binary dispatches on varType_: VarType_Var uses the register
instead of calling toInt(). This is likely a systematic issue across
many CustomFunctions methods.

## IF-63  SeqCArray: index() and array() accessors swapped

**Source**: hdawg_array_index "arrays just supported with type wave" error
**Status**: open
**Severity**: bug

In `seqc_ast_node.hpp:428-429`, `index()` returns `first_` (the wave
variable) and `array()` returns `second_` (the index expression). These
are swapped — `first_` is the wave variable, `second_` is the index.
Fix: swap the accessor implementations.

## IF-64  waitTimestamp rejects 1-arg form

**Source**: hdawg_wait_variants test failure
**Status**: open (needs binary disassembly at 0x1401c0)
**Severity**: bug

`custom_functions_io.cpp:1256-1258` unconditionally rejects any
arguments to `waitTimestamp`. The original binary accepts 0 or 1 args.

## IF-65  Missing DIO/osc node map entries for SHFQA2, SHFLI, GHFLI, VHFLI

**Source**: shfqa2_combo, shfli_combo, vhfli_combo, ghfli_combo failures
**Status**: open (needs binary node map extraction)
**Severity**: bug

The node map tables in `get_node_map.cpp` are incomplete:
- SHFQA: missing `_/dios/0/output`
- SHFLI/GHFLI/VHFLI: missing `_/dios/0/output32`
- SHFSG: `configFreqSweep`/`setOscFreq` uses wrong `qachannels/` path
  instead of `sgchannels/` for SHFSG devices

## IF-66  getDigTrigger register assignment: ltrig overwrites mask

**Source**: hdawg_misc_funcs byte diff
**Status**: open (needs verification)
**Severity**: bug

`custom_functions_io.cpp:1934-1936` puts ltrig result into reg1, which
overwrites the mask already in reg1. Binary puts ltrig into reg2 and
uses andr(reg2, reg1). Also, triggers 3-8 may have wrong mask computation.

## IF-67 through IF-96  Phase 38 bug fixes (batch entry)

**Source**: Phase 38 expanded differential testing (114→139 cases), 2026-04-28
**Status**: fixed
**Severity**: bug (all 30 items)

All discovered and fixed during Phase 38 test expansion. The bugs fall
into 8 categories listed below. Individual items are not numbered
separately because they were fixed in a single session without per-bug
commit granularity.

**Parser (IF-67)**:
- VarType_Void: missing case in parser caused wrong codegen for void functions

**AST evaluation (IF-68 through IF-72)**:
- retType not propagated correctly through function return paths
- switch/case fall-through logic inverted
- Ternary codegen: wrong register for runtime-variable ternary
- Array indexing: off-by-one in multi-dimensional array element access
- Assignment evaluator: compound assignment with nested expressions

**Resources (IF-73, IF-74)**:
- returnReg_ not initialized (caused garbage register in return sequences)
- parent_ (shared_ptr) should be parentWeak_ (weak_ptr) — reference cycle.
  ~15+ uses remain; audit needed.

**Custom functions — playback (IF-75, IF-76)**:
- playZero/playHold dispatch: wrong SubFunc enum routing
- setRate: missing device-type gate on rate validation

**Custom functions — io (IF-77 through IF-80)**:
- getDigTrigger: register/mask assignment order (extends IF-66)
- assignWaveIndex: channelsPerGroup indexing
- setID: wrong immediate encoding
- suppress mask: bit-width mismatch

**Custom functions — play (IF-81 through IF-83)**:
- waitTimestamp: wrong register pair for timestamp readback
- waitDemod: missing device-specific path for UHFLI
- waitZSync: constant value mismatch

**Waveform/ELF (IF-84 through IF-88)**:
- WaveAssignment copy semantics: shallow copy of wave data pointer
- double2awg scale factor: NaN handling for edge cases
- Marker separator: wrong delimiter in multi-marker waves
- ELF flags: section flags not matching binary output
- Node map entries: missing entries for SHFQA/SHFLI/GHFLI/VHFLI devices

**Prefetch (IF-89, IF-90)**:
- prepare(): wrong wave count computation
- placesingle(): incorrect cache pointer dereference

**ASM commands (IF-91)**:
- wtrig: register assignment order swapped

**Waveform generator (IF-92)**:
- gauss 3-arg formula: DRAG normalization constant wrong

**WavetableFront (IF-93)**:
- waveIndex init: uninitialized value caused wrong table index

---

## IF-97  Original binary SIGSEGV on empty void function body

**Source**: test case creation for coverage expansion
**Status**: confirmed — **bug in the original binary**, not the reconstruction
**Severity**: original-binary-bug

An empty void function crashes the original `_seqc_compiler.so` with a
segmentation fault (signal 11).  The reconstruction should handle this
gracefully (either compile it or emit a clear error), but matching the
crash is not a goal.

**Minimal reproducer**:

```c
// Device: HDAWG8, samplerate: 2.4e9, index: 0
void doNothing() {
}

doNothing();
playZero(32);
```

**Invocation**:

```python
import _seqc_compiler as sc
sc.compile_seqc(
    'void doNothing() {\n}\n\ndoNothing();\nplayZero(32);\n',
    'HDAWG8', {}, 0, samplerate=2.4e9)
# → SIGSEGV (process killed)
```

**Analysis**: The crash likely occurs because `SeqCFunction::evaluate`
(or a callee) dereferences `body()` which is null for an empty function.
The binary has no null check on this path.  This is a genuine bug in the
shipped compiler — not a reconstruction artifact.

---

## IF-98  join() interpolation algorithm

**Severity**: medium
**Status**: fixed
**Date**: 2026-04-29
**File**: reconstructed/src/waveform_generator.cpp
**Binary offset**: 0x255da0

**Observation**: `wave w3 = join(w1, w2, 8);` — the third (numeric) arg
is an interpolation length, not a target total length.  For N waves
joined with interpolation length L, the output is:

```
[w1 samples] [L interp samples] [w2 samples] [L interp samples] ... [wN samples] [L zero pad]
```

i.e. each wave is followed by an L-sample block:
- between waves: a linear ramp from `w[i].last` to `w[i+1].first`,
  computed as `w[i].last + (k+1)/L * (w[i+1].first - w[i].last)` for
  k = 0..L-1 (so the final ramp sample equals `w[i+1].first`).
- after the last wave: L zero samples (zero-pad block).

Total length = sum(w[i].length) + N*L (only when L > 0 and there is
more than one wave; otherwise the interp/pad blocks are skipped).

**Empirical evidence** (HDAWG8, `rect(32, 1.0)` joined to `rect(32, -1.0)`
with interpLen=8): output is 80 samples = 32 ones, 8-sample ramp
(0.75, 0.5, 0.25, 0, -0.25, -0.5, -0.75, -1), 32 negative ones, 8 zeros.

**Previous reconstruction bug**: the recon computed total length as
`sum + 2*(N-1)*L` (treating both sides of each boundary as interp
samples), which is numerically equal to N*L only for N=2 — but the
samples themselves were never written; `Signal::append` was called
which just concatenates wave samples directly.  Both the length math
(coincidentally right for N=2) and the sample emission needed to be
fixed.

**Note**: the non-interpolation code path (when L==0) is preserved
exactly to avoid disturbing other join() callers (e.g. marker joins
in `hdawg_doc_marker_gauss`).  Markers in the interpolation/pad
regions are zero-filled.

---

## IF-99  `playIndexed` Phase 17 missing `results->node_` chaining

**Source**: `uhf_doc_tv_mode` failure investigation
**Status**: confirmed, fixed
**Severity**: likely-bug → **fixed**

`CustomFunctions::playIndexed` (`custom_functions_play.cpp` Phase 17,
binary @0x162462..0x162511) only pushed `playEntry` into
`results->assemblers_` and never assigned `results->node_ = playEntry.node`
(or chained via `results->node_->next`).

Without that assignment, the Play node produced by `asmPlay` is *not*
linked into the node tree that `Prefetch::prepareTree` walks. As a
consequence:
- `prepareTree` never visits the Play node, so `linkLoad` →
  `createLoad` is never invoked for it.
- `collectUsedWaves` is never called for that Play, so its
  `wavesPerDev[0]` (set by `asmPlay` to the merged `__join_*` wave name)
  never reaches `waveformMaps_` / `wavetableIR_->usedWaveforms_`.
- Result: the merged waveform is not registered, so the ELF's
  `.waveforms` JSON is empty and the `.wf___join_*` section is missing.

The simple-play path at @0x160438..0x160468 already does this chaining
(`if (results->node_) results->node_->next = asmEntry.node; else
results->node_ = asmEntry.node;`). `playIndexed` must do the same.

**Fix** (custom_functions_play.cpp Phase 17): mirror the simple-play
pattern before the assemblers_ push_back:

```cpp
if (results->node_) {
    results->node_->next = playEntry.node;
} else {
    results->node_ = playEntry.node;
}
results->assemblers_.push_back(std::move(playEntry));
```

**Verification**: stderr instrumentation in `asmPlay`,
`Prefetch::prepareTree`, `Prefetch::createLoad`, and
`Prefetch::collectUsedWaves` showed:
- Before fix: Play node (with correct `wavesPerDev[0]=__join_28_297`)
  is created by `asmPlay` but never visited by `prepareTree`. Only the
  Load wrapper from `compiler.cpp:322` is walked.
- After fix: `prepareTree` reaches the Play node, `linkLoad` →
  `createLoad` produces a Load with the wave name, and
  `collectUsedWaves` inserts `__join_28_297` into `waveformMaps_[0]`.
- ELF size: 4064 → 260532 (orig 260688). Recovered `.channels`,
  `.wf___join_28_297`, `.waveforms`, `.wavemem`. Remaining diff
  (≈156 bytes in `.asm`/`.linenr`/`.text`) is a separate bug — see
  IF-100.

## IF-100  `playIndexed` indexed-play codegen still incorrect

**Source**: `uhf_doc_tv_mode` post-IF-99 investigation
**Status**: Phase 12 Var-branch fixed; remaining diff traced to
IF-102 (placesingle wrong field) and IF-103 (incomplete
play_cervino_indexed body).
**Severity**: likely-bug

After IF-99 fix, `uhf_doc_tv_mode` still differs in `.asm`/`.text`/`.linenr`.

The failing test calls `playWaveIndexed(w_array, t, waveform_length)`
where `t` is a runtime variable (Var=2) and `waveform_length=1260` is a
const. Per Phase 12 comment in `custom_functions_play.cpp:1055-1059`:

> For varType == Var(2): @0x161f5c..0x161f6a takes a different
> branch — pulls a pre-computed AsmRegister from a stack-saved slot
> at [rbp-0x328] and skips the addi/SetVarPlaceholder pair. That
> path has not been independently traced; modeled as the same
> logical operation here.

The recon currently always emits the Const/Cvar `addi indexReg, R0,
Immediate(rate)` path. For Var(2), the original instead reuses a
pre-computed register (likely the register holding `t`) so the play's
length/index can vary per loop iteration.

Concrete asm diff:
- **Original** (per-iteration prefetch hoisted *inside* the loop):

  ```
  for4:  addi R3, R1, -125999
         ...
         addiu R3, R0, 851969
         addi R6, R0, 0
         addi R4, R1, 0
         ssl R4, R4         ; scale loop var t
         addr R3, R4        ; add to base address
         wwvf
         prf R3, R6, 2528   ; per-row prefetch (2520+pad)
         ...
         wvf R6, R0, 2520   ; per-row play (1260*2 channels)
  ```

- **Recon** (single static prefetch outside loop, full-wave play):

  ```
  cwvf 5242753
  addiu R1, R0, 851969
  prf R1, R6, 252000     ; whole-wave prefetch
  ...
  for4: ...
        wvf R6, R0, 252000  ; plays the entire 252000-sample wave
  ```

The recon's `playIndexed` is essentially treating the call as a static
playWave of `combined`, ignoring the index/length args at codegen time.

**Suspected root cause**: the Var-path of Phase 12 must (a) skip the
`addi`/`asmSetVarPlaceholder` pair, (b) install the pre-computed
register holding `t` as `indexReg`, and (c) ensure `regVal` /
`indexOffsetReg` flow into `asmPlay` so the resulting Play node carries
both the per-iteration length (1260*channels) and the offset register
— driving optimization passes to keep the prefetch *inside* the for
loop.

**Next steps**:
1. GDB-trace original at @0x161f5c..0x161f6a with the failing input to
   confirm exactly which AsmRegister slot is read from `[rbp-0x328]`
   and how it threads into `asmPlay`.
2. Check `asmPlay`'s `regVal` / `reg` / `reg2` / `regInv` semantics
   when called with a non-zero indexReg/lengthReg — likely the post-
   waveform optimization pass is responsible for hoisting the prefetch
   out, but only does so when `wvf` length is a constant.
3. After Phase 12 Var-path fix, verify the prefetch hoisting matches
   by inspecting the optimization pipeline (`optimizePostWaveform`).

This is a substantial subsystem investigation in its own right —
deferred from the IF-99 fix to keep that change focused.

## IF-101  removeBranches missing push of node->next in multi-branch path

**Severity**: bug (fixed)
**Status**: fixed

In `Prefetch::removeBranches` (0x1d3530), when the branch retains one or
more children (multi-branch path, originally @0x1d3759-0x1d37b6), the
binary pushes `node->next` (the join node after the branch) onto the
traversal stack BEFORE pushing each branch shared_ptr. The
reconstruction was only pushing the branches.

Without the `node->next` push, `prepareTree`'s BFS terminated as soon
as the branch arms (typically leaf Plays with `next = nullptr`) were
processed. Any code following the if/else (e.g. `wait(3000); playWave;
if/else`) was never visited by `prepareTree`, so its Play nodes never
had `linkLoad` called on them, leaving `loadRef` empty, and triggering
`prefetch Error: missing load for node` in `Prefetch::allocate`.

**Test**: uhf_doc_feedforward (now byte-identical, was failing).
**Score**: 254 → 255 of 259 passing.
**Fix**: prefetch_helpers.cpp `removeBranches` else branch — push
`n->next` before iterating branches.

## IF-102  Prefetch::placeSingleCommand uses wrong field (indexOffsetReg vs lengthReg)

**Source**: `uhf_doc_tv_mode` post-IF-100 investigation
**Status**: partially fixed
**Severity**: bug (fixed for indexed-play dispatch; other call sites
unaudited)

`Prefetch::placeSingleCommand` in `prefetch_placesingle.cpp` had
multiple sites that referenced `node->indexOffsetReg` (+0x94) where
the binary actually reads `node->lengthReg` (+0x88).  The two fields
are adjacent and easy to confuse, but binary verification at the
following addresses confirms the correct offset is +0x88:

- 0x1d91d4: `add $0x88, %rdi` before `isValid` — the cervino_nonsplit
  → cervino_indexed dispatch check (recon line 520 in original file).
- 0x1d9f3b/0x1d9f50: post-wvf indexed-play dispatch (recon line 588).
- 0x1dac6f: `mov 0x88(%rax), %r13` then passed as 3rd arg to
  `addi(idxReg, ?, Immediate(0))` in cervino_indexed_split branch
  (recon line 658).
- 0x1d9953: same pattern in cervino_indexed2_hirzel (recon line 743).

Without these fixes, `play_cervino_indexed` was never entered, even
when the playWaveIndexed had a runtime offset register set in
`lengthReg` by `asmPlay`.  The recon was unconditionally taking the
`play_cervino_nonsplit` static-prefetch path.

**Fix** (this commit): updated lines 520, 588, 658, 693, 743 to read
`node->lengthReg`.  Other `indexOffsetReg` references in the file
(lines 190, 457, 466) were NOT audited and may also be wrong; left
in place pending verification per their own binary addresses.

**Test impact**: `uhf_doc_tv_mode` now reaches `play_cervino_indexed`
and emits the `addi R3, R1, 0; ssl R3, R3` indexed-prep instructions,
moving the recon ELF from 260532 → 260568 (orig 260688).  Test still
fails — the remaining diff is the body of `play_cervino_indexed`
itself which is incomplete (see IF-103).

## IF-103  `play_cervino_indexed` body is not fully reconstructed

**Source**: `uhf_doc_tv_mode` post-IF-102 investigation
**Status**: open (investigation deepened — see IF-105)
**Severity**: bug (substantial reconstruction work)

After IF-102, `uhf_doc_tv_mode` enters `play_cervino_indexed` but the
emitted instruction sequence is incomplete relative to the original.

**UPDATE (post-GDB trace)**: The bulk of the missing instructions are
NOT emitted by `play_cervino_indexed` (0x1dabb9) at all — they are
emitted by the **case 1 Load** node going through the indexed-Load
path at **0x1da77f**, which the recon currently stubs out as
`load_indexed_play` → `load_finalize`.  See IF-105 for the full
breakdown of which node emits which instructions.

Original (UHFLI tv-mode loop, indexed playback):

```
addiu R3, R0, 851969     ; addi for cwvf encoding (>=0x1000000)
addi  R6, R0, 0          ; stateRegC
addi  R4, R1, 0          ; copy lengthReg (R1) into R4
ssl   R4, R4             ; shift left for 2-channel
addr  R3, R4             ; R3 += R4 (offset address)
wwvf
prf   R3, R6, 2528       ; per-iter prefetch with idx=R3, base=R6
wprf
... (trigger writes) ...
wvf   R6, R0, 2520       ; per-iter play
```

Recon emits only `addi R3, R1, 0; ssl R3, R3` and is missing the
addr / wwvf / prf / wprf / wvf sequence.  Additionally, recon emits a
spurious static `prf R2, R1, 252000` BEFORE the loop (likely a leftover
from the prepareTree pipeline still treating the wave as a static
play; the Load placement may need to follow the indexed dispatch).

**Source ranges to reconstruct** (binary addresses already noted in
`prefetch_placesingle.cpp` comments):
- 0x1dabb9..0x1db1ff (split branch): SSL loop body, addr emission,
  optional wwvf, prf with clampToCache, optional wprf.
- 0x1daed4..0x1db4xx (non-split branch): same logic.
- 0x1db6f8..0x1db942 (common indexed finalize): assembly emission of
  the per-iteration play instructions, including the wvf with
  `node->length * channels * 2` byte count (matching the
  `totalSize` computation already present).
- Common cwvf-encoded addiu emission upstream (the `addiu R3, R0,
  851969` instruction precedes the indexed addi+ssl block — its
  source needs to be located).

Estimated work: comparable in scope to a fresh recon of a 200+ byte
function region, including SSL loop, addr/wwvf/prf clamping, and
cwvf encoding.  Deferred from the IF-102 fix to keep that change
focused.

**Test impact**: 1 test still failing (`uhf_doc_tv_mode`).  Suite
score unchanged at 255/259.

---

## IF-104  ELFIO NOBITS section file-offset layout differs between vendored and system builds

**Source**: `shfsg_doc_ct_placeholder` test failure investigation
**Status**: confirmed
**Severity**: cosmetic (diff-test artifact, not a semantic bug)

The original binary statically links its own copy of ELFIO, while the
reconstruction links against the system `elfio` package.  The two
versions assign different `sh_offset` values to `SHT_NOBITS` sections.

For `shfsg_doc_ct_placeholder.seqc` (two `placeholder()` waveforms via
`assignWaveIndex`):

| section                      | original off | recon off |
|------------------------------|--------------|-----------|
| `.wf___placeholder_3_0` NB   | 0x1000       | 0x1000    |
| `.wf___placeholder_4_1` NB   | 0x1000       | 0x1040    |
| `.text` PROGBITS             | 0x1000       | 0x1040    |

Both sets of headers are semantically equivalent — NOBITS sections
declare address+size only and occupy no file bytes.  But because
`diff_test.py` was reading raw bytes at `[sh_offset, sh_offset+size)`
for *every* section, the byte-overlap between NOBITS and the colocated
PROGBITS produced spurious "content" diffs.  The original happened to
overlap NOBITS over `.text` (so both ph3 and ph4 "data" looked like
text bytes), while the recon padded the NOBITS gap with zeros before
`.text`.

**Determinism check**: original output is byte-identical across 5
consecutive runs — the bytes are not uninitialized memory, just
file-offset overlap.

**Fix**: `tests/diff_test.py` now special-cases `SHT_NOBITS` sections
to compare type/size/addr instead of file bytes (the same semantics
ELF loaders use).  No code change in `reconstructed/`.

**Test impact**: `shfsg_doc_ct_placeholder` now passes, suite score
255 → 256/259.

## IF-105  `case 1` Load with `lengthReg!=0` (path 0x1da77f) is the actual emitter for indexed-play setup

**Source**: `uhf_doc_tv_mode` deeper investigation (extends IF-103)
**Status**: open (proposed fix below)
**Severity**: bug (substantial work; ~50–100 lines of new emission code)

### GDB-verified execution flow for `uhf_doc_tv_mode`

```
1st placeSingleCommand call (Load, no wave):
  case 1 → load_fallback → static_prf_path_load (0x1d8013)
  emits: cwvf 5242753

2nd placeSingleCommand call (Load, has wave + lengthReg != 0):
  case 1 (0x1d79f8)
  → step 1: alloc registerHirzel
  → step 2: addi(regH, R0, addrValue)        ; emits "addiu R3, R0, 851969"
  → step 3 (cervino, next!=null):
    addi(regC, R0, cachePos)                  ; emits "addi R6, R0, 0"
  → step 4: lengthReg.isValid() && != 0
    → JMP 0x1da77f (NOT 0x1d85f6 / load_cervino_prf)

  At 0x1da77f (the "load_indexed_play" body, NOT yet implemented):
  - check nodeStates_[node].cachePtr->numRepeats_ >= 2  (0x1da7a8)
    if yes: wwvf()                             (0x1da7b9, NOT taken on this test)
  - alloc tempReg                              (0x1da7da)
  - addi(tempReg, lengthReg, Imm(0))           ; emits "addi R4, R1, 0"
  - SSL loop over wave's numPages (channels @ +0xc8):
      ssl(tempReg)                             ; emits "ssl R4, R4"
      (loops `numPages` times — for tv_mode wave, channels=1 so 1 iter
       wait actually channels for waveAtCurrentDeviceIndex returns
       sample-pages count, need to double-check; output shows 1 ssl)
  - addr(stRegHirzel, tempReg)                 ; emits "addr R3, R4"   (0x1daa86)
  - check cacheSize >= minIndexedSize          (0x1daad2)
    if yes: jmp 0x1db562 (alternative path)
  - wwvf()                                     ; emits "wwvf"          (0x1daae9)
  - prf(stRegH, stRegC, clampToCache(cacheSize/2))
                                              ; emits "prf R3, R6, 2528" (0x1dab9f)
  - jmp 0x1db911 → 0x1db92e finalize:
    if !isHirzel: wprf()                      ; emits "wprf"          (0x1db937)
  - insert tempList into output at placeholder  (0x1db963)

3rd placeSingleCommand call (Play indexed):
  case 2 (play_entry, 0x1d7d49)
  → eventually reaches play_cerv_indexed_split (0x1dabb9)
  Per .asm output, this emits ONLY:
    wvf R6, R0, 2520
  (despite the 0x1dabb9 disassembly containing addi+ssl+... — those
   results may go into a different list/placeholder, or the addi+ssl
   are computed but not appended in this register-set; needs further
   GDB tracing of the play call to verify what is actually appended.)
```

### Recon's incorrect output

The recon currently emits, BEFORE the for-loop:
```
addiu R2, R0, 851969    ← from case 1 step 2 (regH addi)
addi  R1, R0, 0         ← from case 1 step 3 (regC addi)
prf   R2, R1, 252000    ← from load_cervino_prf path B2 (full waveformMemorySize)
wprf                    ← from load_finalize
```
But the original places ALL of these (with adjusted prf size, and with
extra ssl/addr/wwvf in between) INSIDE the for-loop, via the 0x1da77f
path, which is not yet reconstructed.

### What needs implementing

1. **`load_indexed_play` body** (case 1 step 4 → 0x1da77f path) — this
   currently `goto load_finalize` and produces nothing useful.  Needs:
   - `numRepeats >= 2` check; emit wwvf() if so
   - alloc tempReg
   - addi(tempReg, lengthReg, Imm(0))
   - ssl loop over `numPages` (waveform's signal.channels_ +0xc8)
   - addr(stRegHirzel, tempReg)
   - `cacheSize >= minIndexedSize` branch (if yes → emit per the
     0x1db562 alternative path: addi + addi + wprf + prf with
     numRepeats etc.; if no → emit wwvf+prf+wprf as above)
   - For the no branch (this test's path):
     wwvf() + prf(stRegH, stRegC, clampToCache(cacheSize/2)) + wprf()
   - Insert tempList at placeholder

2. **`play_cervino_indexed` actual emission** — needs further GDB
   tracing to determine exactly which instructions reach the OUTPUT
   stream (only `wvf` is observed in the .asm).  May involve:
   - Dispatch to a separate placeholder for the per-iter wvf
   - The addi+ssl+addr+wwvf computed at 0x1dabb9 may be DEAD CODE for
     this configuration, or emitted to a side stream

3. **Fixes to existing `load_cervino_prf` path** — currently always
   takes Path B2 (cacheSize != waveformMemorySize → full size prf),
   producing the `prf R2, R1, 252000`.  This pre-loop emission must
   not happen when lengthReg!=0; the indexed_play branch (item 1)
   should run instead.  Step 4's `goto load_indexed_play` is correct;
   the bug is just that load_indexed_play is a stub.

### Estimated scope

- ~30 lines for the `load_indexed_play` body (item 1, no-Repeats branch)
- ~20 lines for the `>=minIndexedSize` alternative (the 0x1db562 path)
- Need a 2nd GDB session to nail down what `play_cervino_indexed`
  actually appends to output (item 2)
- Risk of regressing other tests that take case 1 step 4 (any indexed
  load on Cervino/Hirzel devices)

Recommendation: implement in two patches —
(a) load_indexed_play (no-Repeats, no-Hirzel, < minIndexedSize) —
    targets exactly tv_mode and verifies the diff reduces to just the
    `wvf` instruction;
(b) play_cervino_indexed wvf emission — only after (a) is verified.

### IF-105 update (after partial implementation attempt)

The `load_indexed_play` body has been reconstructed in
`reconstructed/src/prefetch_placesingle.cpp` (matching binary 0x1da77f
for the no-Repeats / non-Hirzel / `< minIndexedSize` branch).  However,
**the recon never reaches it** for `uhf_doc_tv_mode`.

A debug fprintf at step 4 confirmed:
```
STEP4: lengthReg.valid=0 val=-1 indexOff.valid=0 val=-1
```

The Load node's `lengthReg` and `indexOffsetReg` are BOTH invalid in
the recon, whereas the original binary takes step 4's "lengthReg
valid && != 0" branch (verified via GDB).  So the missing wiring is
**upstream**: the Load node needs its `lengthReg` (the same lengthReg
field that lives on the Play node from the indexed-play call) to be
copied or referenced.

Likely culprits — places where Load and Play nodes are paired up:
- `prepareTree` / Load-creation pass (where Load nodes are extracted
  from playWaveIndexed)
- The shared-back-reference (`loadRef`) wiring — perhaps the play's
  lengthReg should also be propagated to the load
- The "case 1 falls through to case 2" merger artifact (Phase 10.8b
  comment in the file header)

**This is now a separate root cause** distinct from the missing
`load_indexed_play` body.  The body is correct and ready for use once
the upstream wiring is fixed; until then the body is dead code.

**Action items** (to be promoted to TODO.md):
- Investigate where `Node::lengthReg` is set during prepareTree / load
  extraction in the Cervino path.
- GDB-trace the original at the play→load wiring point to see where
  the load's lengthReg is populated (likely during a copy from a
  paired play node).

### IF-105 update 2 (root cause located, partial fix landed)

**Status**: partially fixed — wiring activated, emission body now
runs; remaining diff is placement/ordering, not wiring.

The lengthReg is set correctly on the FIRST Load node by
`Prefetch::createLoad` (prefetch.cpp:2085, copies from the source Play
node's lengthReg which `asmPlay` populates from `indexReg`).  However,
`Prefetch::moveLoadsToFront` (prefetch.cpp:0x1ccad0) creates a SECOND
Load node and BFS-walks the tree to find matching Loads (cur->type ==
Load with matching wave name).  When found, it inherits the matched
Load's `play` weak_ptrs into the new Load and splices the old Load out.
The recon (and likely the original) did not previously copy
`cur->lengthReg` into the new Load — so the new Load that survives in
the tree, and therefore the one that reaches `placeSingleCommand` step
4, had `lengthReg.isValid() == false` and the indexed-play emission
path was not taken.

**Fix** (this commit): in moveLoadsToFront's namesMatch block (recon
line ~841 / binary 0x1cd313), copy `cur->lengthReg` into
`loadNode->lengthReg` before splicing cur out.  Guarded so the assignment
only occurs when loadNode is currently invalid and cur is valid.

**Test impact**: `uhf_doc_tv_mode` now emits the
`addiu/addi/addi/ssl/addr/wwvf/prf/wprf` block from `load_indexed_play`,
and the spurious pre-loop `prf R2, R1, 252000` is gone.  Recon ELF
260568 → 260728 (orig 260688).  Test still fails on three remaining
issues:
1. The new block is emitted BEFORE the for-loop instead of INSIDE the
   loop body (placeLoads ordering).  Original puts it after `false5:`
   inside the for body.
2. Spurious `addi R3, R1, 0; ssl R3, R3` still emitted inside the loop
   from the play_cervino_indexed path (IF-103 part 2).
3. `prf` immediate differs (orig 2528 from `clampToCache(cacheSize/2)`
   on smaller cacheSize; recon 126000 = full size/2).  Likely linked to
   issue 1 — the load is placed at the wrong scope so its cache size
   accounting differs.
4. Register allocation differs (orig R3,R6 vs recon R3,R2).

These are no longer wiring issues — they are emission-body /
placement issues.  The dead-code activation goal of IF-105 is met.
Suite score remains 256/259 (no regression).

**Action items** (remaining, to be promoted to TODO.md):
- Investigate placeLoads ordering for indexed-play loads (why the
  load isn't placed inside the for-loop body).
- Suppress the spurious play_cervino_indexed emission of addi+ssl
  when the indexed-load has already emitted them (IF-103 part 2).
- Verify clampToCache argument matches binary semantics for the
  in-loop case.

### IF-105 update 3 (placeLoads no-split path activated; getRequiredMemory fixed)

**Status**: partially fixed — moveLoadsToFront no longer hoists the
indexed Load; emission now lands inside the for-loop body.

GDB-traced the original on `uhf_doc_tv_mode` and discovered that
`placeLoads()` actually takes the **no-split** path (does not call
`moveLoadsToFront`) for this case.  Recon was incorrectly taking the
split path because `Prefetch::getRequiredMemory()` was reconstructed
with the wrong page-count formula.

**Root cause in getRequiredMemory** (prefetch_helpers.cpp:120):
The inner loop computes `numPages` from the binary at `0x1cc9d0`:
```
r9  = DC+0x40 (waveformGranularity)   ; floor cap, NOT cap
r10 = DC+0x44 (waveformPageSize)      ; round-up divisor
eax = roundUp(numRepeats, r10)
eax = max(eax, r9)                    ; cmova at 0x1cc9ea — was reconstructed as min
```
The recon had `numPages = min(rounded, granularity)`, which clamped
massive waveforms (e.g. 126 000 samples) down to the granularity (16),
so `getRequiredMemory()` returned 32 instead of 252 000 for tv_mode.
That fooled `placeLoads` into the split path → moveLoadsToFront →
indexed-Load hoisted out of the loop.

**Fix** (this commit): change `min` to `max` and rename locals to
`granularityFloor`/`pageSize` to match binary semantics.

**Test impact for `uhf_doc_tv_mode`**:
- Before: pre-loop emission of `addiu/addi/wwvf/prf/wprf` block
  (260 728 bytes, blocks BEFORE `while1:`).
- After: block correctly emitted inside the for-loop body
  (260 760 bytes, after the wtrigs).  No-split path confirmed via
  `cmp eax,ecx; jbe split` Intel-syntax disassembly.
- Suite score still 256/259, no regressions.

**Remaining diff for tv_mode** (3 issues, all in load_indexed_play
emission body, not placement-pipeline):
1. Ordering inside loop body: original places the indexed-Load setup
   right after `false5: brz R4, end7` (start of body); recon places
   it after the user statements (setID + wtrigs).  Likely the
   placeholder used by load_indexed_play is being inserted at the
   wrong scope position.
2. Extra `wwvf` emitted (recon has two, original has one).  Need to
   re-check the load_indexed_play body — possibly the
   `cacheSize >= minIndexedSize` branch incorrectly emits both
   wwvf paths.
3. `prf` immediate differs (orig 2528 from
   `clampToCache(cacheSize/2)` with the in-loop cache size; recon
   42016 with a different cache size).  Linked to issue 1 or to
   `clampToCache` argument semantics.
4. Register allocation differs (orig R3,R6 vs recon R4,R3) —
   downstream of the above.

These remain open as IF-105 follow-ups.

### IF-105 update 4 (indexed-allocation cache size + clampToCache args fixed)

**Status**: partially fixed — load setup now byte-correct
(`addiu/addi/addr/wwvf/prf 2528/wprf` matches original), but
placement and `play_cervino_indexed` body still wrong.

GDB-traced the cache allocation path for tv_mode and found two
distinct bugs:

1. **Cache::allocate path selection** in `Prefetch::allocate` (the
   "splitPath" block at recon line 1622, binary 0x1d1a9b..0x1d1efc).
   The original takes a *separate* indexed-allocation path at binary
   0x1d1e01 when `playNode->length != 0` — it bypasses the full-wave
   size calculation and instead uses
   `numSamples = playNode->length * channels * 2`.  Recon was always
   using the full-wave formula (rounding `signal.length` to pageSize
   then multiplying by channels and bps/8), giving cache size 84032
   for tv_mode where original gets 2528.

2. **`load_indexed_play` prf immediate** in `prefetch_placesingle.cpp`
   at the prf call (binary 0x1dab7d-0x1dab87).  The recon called
   `clampToCache(cacheSize/2)` but the binary loads
   `mov 0x4(%rax),%esi` (= `cachePtr->size_`, no division) and passes
   that directly to `clampToCache`.  Removed the `/2`.

**Fixes** (this commit):
- `prefetch.cpp` splitPath: branch on `playNode && playNode->length != 0`
  → use indexed numSamples formula.  Otherwise keep existing full-wave
  formula.
- `prefetch_placesingle.cpp` load_indexed_play: drop the `/2` in the
  clampToCache call.

**Test impact**:
- `uhf_doc_tv_mode`: 260760 → 260728 (orig 260688, 40 bytes off).
- The `addiu R3, R0, 851969 / addi R6, R0, 0 / addi R4, R1, 0 /
  ssl R4, R4 / addr R3, R4 / wwvf / prf R3, R6, 2528 / wprf` block
  now appears with correct immediate (2528).
- Suite still 256/259, no regressions.

**Remaining structural issues** (not fixed in this pass):

A. **Placement of load_indexed_play tempList**: the load setup is
   currently inserted at the END of the for-loop body (after user
   `setID/wait/setTrigger/wtrigs`), but the original places it at
   the START of the body (right after `false5: brz R4, end7`).  The
   placeholder used by case-1 Load may be at the playWaveIndexed
   site instead of the loop-entry site.  Likely needs investigation
   of where `placeholder` (Asm with matching asmId) ends up after
   `placeLoads` runs in the no-split path.

B. **Missing wvf instruction**: original emits
   `wvf R6, R0, 2520` (R6 = load's regC, length = `playNode->length *
   channels * 2 = 2520`).  The recon's `play_cervino_indexed`
   currently emits `addi R3, R1, 0; ssl R3, R3` (the addi/ssl
   computed at binary 0x1dac9a..0x1dae1d) but does NOT call
   `Prefetch::wvfs` (visible at binary 0x1db9c9 inside the common
   indexed-finalize 0x1db6f8..0x1db942).  The addi/ssl appear to be
   emitted to a tempList that should be discarded or routed
   elsewhere; only the `wvf` should reach the output stream.
   Significant rework of the play_cervino_indexed body needed.

C. **Register allocation** differs (orig R3,R6 vs recon R4,R3) —
   downstream of the alloc-order changes from A/B.

These three are no longer "instruction-level" tweaks but require
restructuring of the play-side indexed emission and the placement
pipeline.  Deferred.

### IF-103 / IF-105 status summary

- IF-103 part 1 (`load_indexed_play` body missing): **fixed** in
  prior commit (a247dfb).
- IF-103 part 2 (`play_cervino_indexed` extra addi+ssl, missing
  wvf): **fixed** in this commit (see IF-105 update 5 below).
- IF-105 wiring (`lengthReg` propagation in moveLoadsToFront):
  **fixed** in 6aa0602.  However for tv_mode the wiring is moot
  because moveLoadsToFront should not run at all (see update 3).
- IF-105 update 3 (`getRequiredMemory` formula): **fixed** in
  prior commit.
- IF-105 update 4 (indexed cache-size + clampToCache(/2)):
  **fixed** in prior commit.
- IF-105 update 5 (`play_cervino_indexed` non-split body):
  **fixed** in this commit.
- Remaining tv_mode diff: 0 bytes ELF size mismatch, 1211/1211 .asm
  byte match, but `.asm` content still differs due to:
  (A) placement of load setup block (still emitted AFTER user
      statements; original places it BEFORE) and
  (C) register allocation differences (orig R6 vs recon R3 for
      registerCervino).  Test still fails (256/259) but the
      structural codegen for the play-side is now correct.

### IF-105 update 5 (play_cervino_indexed non-split body reconstructed)

**Status**: fixed — the recon now emits the correct single
`wvf R*, R0, 2520` instruction for tv_mode's play_cervino_indexed
non-split path.

**GDB trace** of the original on `uhf_doc_tv_mode` revealed the
actual control flow taken at `play_cervino_indexed` (0x1dabb9):

```
0x1dabb9 (entry)
  → cmp [r15+0xbc], 1      ; check split_
  → jne 0x1db60f             ; NOT split → take non-split branch
0x1db60f (non-split)
  → emplace nodeStates_[node] → registerCervino  saved in [rbp-0x148]
  → length = node->length  saved in [rbp-0x58]
  → emplace → cachePtr->size_ loaded
  → cmp size, minIndexedSize
    jae 0x1dbb04           ; large-cache: totalSize = length * channels
    fall:                  ; small-cache: totalSize = length * channels * 2
  → jmp 0x1dbb6d → 0x1d9d3a
0x1d9d3a (shared wvfImpl emission)
  → cmp deviceType, 2; jne 0x1d9d4e
  → cmp pageCount, [devConst+0xc]; jne 0x1d9ee2
0x1d9ee2 (single wvfImpl)
  → wvfImpl(registerCervino, totalSize, is4Ch)  ; emits "wvf R6, R0, 2520"
  → append result to tempList
  → check lengthReg.isValid() && != 0 && !split_ && cachePtr->size_ >= minIdx
    → cachePtr->size_ (2528) < minIndexedSize (4096) → jb finalize
0x1dba0d (play_finalize)
  → insert tempList into output
```

The previous recon had the non-split branch (the `else` block in
`play_cervino_indexed`) emitting addi+ssl based on the binary at
0x1daed4, but 0x1daed4 is a different scenario altogether
(another split-check entry-point), not the actual non-split path
of 0x1dabb9.  The actual non-split path is the 0x1db60f →
0x1dbb6d → 0x1d9d3a chain that converges into the same wvfImpl
emission used by play_cervino_nonsplit (without the indexed
gate).

**Fix** (this commit): replace the `else` branch of
`play_cervino_indexed` with code that:
1. Reads `node->length`, `nodeStates_[node].registerCervino`,
   `nodeStates_[node].cachePtr->size_`, and the wave's
   `signal.channels_`.
2. Computes totalSize via the cachePtr->size_ vs minIndexedSize
   branch (large-cache: length*channels; small-cache:
   length*channels*2).
3. Calls `wvfImpl(registerCervino, totalSize, is4Ch)` and appends
   the result to tempList.
4. If `cachePtr->size_ >= minIndexedSize`, additionally emits the
   addi/addi/prf/wprf block (the 0x1d9fab path).

For tv_mode, only step 3 produces output (size < minIdx → step 4
skipped) — matching the `wvf R*, R0, 2520` in the original.

**Test impact**:
- `uhf_doc_tv_mode`: ELF size 260728 → 260688 (matches original).
- `.asm` size 1229 → 1211 (matches original).
- Test still fails because:
  - Issue A (placement): the load_indexed_play tempList is
    inserted at the END of the for-loop body (after user
    `setID/wait/setTrigger/wtrigs`); the original places it at
    the START (right after `false5: brz R4, end7`).
  - Issue C (registers): orig allocates R6 for registerCervino
    and R3 for registerHirzel; recon allocates R3 and R4
    respectively.  Likely downstream of the alloc-order
    changes from prior fixes.
- Suite score 256/259, no regressions.

These two issues are placement-pipeline / resource-allocation
problems, not codegen-body problems.  Deferred.
