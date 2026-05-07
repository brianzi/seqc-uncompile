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
**Status**: **fixed** (prior session) — inline comments corrected to `Cervino/klausen` and `Hirzel` to match verified enum mapping (UHFLI=1 Cervino, HDAWG=2 Hirzel)
**Severity**: suspicious (misleading, may cause future errors)

Lines 58-59 and 82 of `reconstructed/src/runtime/static_resources.cpp`:

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
**Status**: **dismissed** (2026-05-05) — libstdc++ `_GLIBCXX_ASSERTIONS` catches the null `shared_ptr` dereference that the original libc++ binary exercises silently as UB. Root cause: `toSeqCAst()` returns null for pure-comment inputs; guarded by `if (seqcAst)` at `compiler.cpp:290`. Assertion no longer fires.
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
**Status**: **confirmed/correct** (GDB-verified 2026-05-05)
**Severity**: suspicious (resolved — reconstruction is accurate)

`Resources::getVariable` at 0x1eb0a0 walks parents via two mechanisms:
1. `parent_.lock()` — weak_ptr at this+0x40 (control block at this+0x48); binary +301 loads +0x48, then calls `lock()`
2. `grandparent_` — raw `shared_ptr` at this+0x18; binary +413 loads +0x18 as fallback

GDB disassembly of 0x1eb0a0 confirms both paths match the reconstruction at `resources.cpp:307-338` exactly. The dual walk is intentional and the recon is correct.

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
**Status**: **fixed** (prior session) — `checkFunctionSupported` at `custom_functions.cpp:571` correctly uses `FuncNotSupported` (73) with format `"function '%1%' not supported on %2% devices"`. Both original and recon produce identical output. Was fixed as part of RC-6 (error message map m[73]/m[74] swap + try/catch).
**Severity**: likely-bug

Original produces: `function 'X' not supported on SHFQA devices`
Recon produces: `function 'X' reaches end without returning a SHFQA`

The "reaches end without returning" message is a completely wrong error
path — this is not a return-type error, it's a device-support check.
The `checkFunctionSupported` function likely throws the wrong exception
or formats the wrong error message template.

---

## IF-9  `UnknownError47` (=47) has no entry in error message map; QA_DATA_RAW path wrong

**Source**: hdawg_executeTable `map::at` error
**Status**: **fixed** (2026-05-05, revised 2026-05-06) — GDB-verified at binary 0x151254: `QA_DATA_RAW` (or `QA_DATA_PROCESSED_D`) as a named-const arg to `executeTableEntry` on `SHFQC_SG` causes the binary to throw `UnknownError47` (=47), but the binary's own `ErrorMessages` map has no entry for key 47. The binary therefore crashes with `std::map::at` throwing `std::out_of_range` instead of producing a proper error message. **This is a bug in the original ZI binary.**

Two-stage fix:
  1. (Initial) Removed the SHFQC_SG `QA_DATA_RAW`/`QA_DATA_PROCESSED_D` branch in `custom_functions_registers.cpp` so `constMatched` stays false and the fallthrough reproduces the same crash path as the binary.
  2. (Revised, commit `ef7cd44`) Recon now throws `CustomFunctionsException(ErrorMessages::format(UnknownError47, …))` *explicitly*, and we define `m[47]` in `error_messages.cpp:182` with the message:
     `"Invalid constant argument used for executeTableEntry (the original binary has no m[47] and crashes here with map::at — you're welcome, ZI)"`
     This is an **intentional divergence**: the recon produces a clean exception with a proper message instead of crashing with `map::at`. The cheeky message string makes it self-documenting if anyone ever sees it in the wild.

**Severity**: likely-bug (in upstream binary)

`executeTableEntry` references `UnknownError47` (error code 47) at lines
2701 and 2706 of `custom_functions_io.cpp`.  But `error_messages.cpp`
has no `m[47]` entry in the binary.  It does have `m[48] = "Invalid constant argument
used for executeTableEntry"` which matches the semantics — the binary author probably
intended to use 48 here but typo'd 47.

The binary addresses at 0x150e00 and 0x150e40 should be checked to
confirm whether the error code loaded is 47 or 48. (GDB at 0x151254 confirms 47.)

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
- **Status**: **fixed** (verified 2026-05-05 — runtime loop generation confirmed via `.asm` diff for `hdawg_nested_repeat`; 1341/1341 tests pass byte-identical)
- **Severity**: likely-bug (was: 4 test failures; now resolved)
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
**Status**: **fixed** (verified 2026-05-05)
**Severity**: medium (resolved)

3-arg `sine(length, amp, phase)` in recon produces a constant waveform
where every sample = `sin(amplitude + phase)`. Original binary produces
27572 for `sine(64, 1.0, 0.0)` = `sin(1.0+0.0)*32767` ≈ 27572 ✓.
Recon matches exactly. The formula `sin(amplitude + phase)` is correct.

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
**Status**: **dismissed** (2026-05-05) — test is byte-identical in current recon. `unsyncCervino()` correctly captures `wavetableFrontIndex_` at Step 16 (after all AST eval), giving the correct final line number. No SyncPlaceholderCervino node exists in this test case; the description's "SetVar vs Load placeholder" root cause does not apply. Was a transient issue in an earlier reconstruction pass.
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
**Status**: **fixed** (verified 2026-05-05 — VarType_Var dispatch confirmed present in `custom_functions_playback.cpp:937-956`; 1341/1341 tests pass)
**Severity**: bug (resolved)

`custom_functions_playback.cpp:937` unconditionally calls
`args[0].value_.toInt()`. When the argument is a runtime Var (e.g. from
a ternary `(c > 0) ? 32 : 64`), the Value has type Unspecified. The
original binary dispatches on varType_: VarType_Var uses the register
instead of calling toInt(). This is likely a systematic issue across
many CustomFunctions methods.

## IF-63  SeqCArray: index() and array() accessors swapped

**Source**: hdawg_array_index "arrays just supported with type wave" error
**Status**: **fixed** (verified 2026-05-05 — `index()` returns `index_` and `array()` returns `array_` in `seqc_ast_node.hpp:427-428`; swap was corrected; 1341/1341 tests pass)
**Severity**: bug (resolved)

## IF-64  waitTimestamp rejects 1-arg form

**Source**: hdawg_wait_variants test failure
**Status**: **fixed** (verified 2026-05-05 — `waitTimestamp` at `custom_functions_wait.cpp:813` accepts any number of args and ignores them; binary confirmed same via disassembly; 1341/1341 tests pass)
**Severity**: bug (resolved)

## IF-65  Missing DIO/osc node map entries for SHFQA2, SHFLI, GHFLI, VHFLI

**Source**: shfqa2_combo, shfli_combo, vhfli_combo, ghfli_combo failures
**Status**: **dismissed** (2026-05-05) — binary disassembly confirms the node maps for SHFLI/GHFLI/VHFLI contain only `oscs/N/freq` entries (8 per device, loop 0..7); no DIO entries exist in the binary for these types. SHFQA similarly has no `_/dios/0/output` entry. String `_/dios/0/output32` does not appear anywhere in the binary. All 1341 tests pass — the node maps in `get_node_map.cpp` are complete and correct.
**Severity**: bug

The node map tables in `get_node_map.cpp` are incomplete:
- SHFQA: missing `_/dios/0/output`
- SHFLI/GHFLI/VHFLI: missing `_/dios/0/output32`
- SHFSG: `configFreqSweep`/`setOscFreq` uses wrong `qachannels/` path
  instead of `sgchannels/` for SHFSG devices

## IF-66  getDigTrigger register assignment: ltrig overwrites mask

**Source**: hdawg_misc_funcs byte diff
**Status**: **fixed** (verified 2026-05-05 — `custom_functions_registers.cpp:220+` puts ltrig into reg1 and mask into reg2, then `andr(reg1, reg2)` — correct order; 1341/1341 tests pass)
**Severity**: bug (resolved)

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
**File**: reconstructed/src/waveform/waveform_generator.cpp
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
`reconstructed/src/codegen/prefetch_placesingle.cpp` (matching binary 0x1da77f
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
- IF-105 update 6 (`Prefetch::optimize` Load+SetVar branch:
  inverted parentLoad type check at 0x1cdeae): **fixed** in
  this commit — see update 6 below.  This was the root cause
  of both the placement (A) and register-allocation (C)
  diffs in tv_mode.
- tv_mode now byte-identical (suite 257/259, +1, no regressions).

### IF-105 update 6 (optimize() inverted parentLoad type check)

**Status**: fixed — `uhf_doc_tv_mode` now byte-identical.
Suite went from 256/259 to 257/259 with no regressions.

**Site**: `Prefetch::optimize`, the Load-with-SetVar-parent
branch around binary 0x1cdeae (recon `prefetch.cpp:1110`).

**Bug**: the recon implemented the parentLoad-type gate as
"rewrite asmId only when parentLoad is a Loop", i.e.

```cpp
if (parentLoad && parentLoad->type == NodeType::Loop) {
    curNode->asmId = parent->asmId;
}
```

The actual binary control flow at 0x1cdeae is the opposite:

```
1cdeae: cmp DWORD PTR [rax+0x44], 0x8     ; parentLoad->type == Loop?
1cdeb6: je   1ce685                       ; YES → SKIP rewrite
1cdebc: mov  rax, [rbp-0xc0]              ; (parent shared_ptr)
1cdec3: mov  eax, [rax+0x14]              ;   eax = parent->asmId
1cdec6: mov  [r14+0x14], eax              ;   curNode->asmId = parent->asmId
1cdeca: jmp  1ce685
```

The rewrite happens **only when parentLoad->type != Loop**.
For tv_mode, parentLoad is itself a SetVar (type 0x10), so:
- original: 0x10 != 0x8 → rewrite Load.asmId 44 → 32 (the
  SetVarPlaceholder asmId)
- buggy recon: 0x10 != 0x8 → check fails → asmId stays at 44

The asmId difference cascaded into:
1. **Issue A (placement)**: the surviving Load's asmId
   determines the placeholder lookup target in
   `placeSingleCommand` / placement helpers.  asmId=32 sits
   before user statements in the AsmList, asmId=44 sits
   after — so the load setup block was emitted at the END of
   the for-body in recon vs the START in the original.
2. **Issue C (registers)**: with the load setup emitted at
   a different point in the assembly stream, the
   register allocator processes the temp-register
   requests in a different order, yielding R3/R4 vs R6/R3.

Fixing the polarity of the parentLoad type check fixes
both diffs in one shot.

**GDB verification recipe** (the key trace that revealed
the bug):

```
hooks at:
  0x1cbfbf  cmp eax,ecx          (required vs cacheSize)
  0x1cc250  cmp [rbx+0xbc],0     (split_ check)
  0x1cc327  call optimize         (optimize entry)
  0x1cdec6  mov [r14+0x14],eax    (asmId rewrite site)
```

For tv_mode the original prints:
- required=252000 cacheSize=131072  → required > cacheSize
- at 1cc250 split_=0
- calling optimize
- 1cdec6 SetVar+lengthReg match: cur=0x... oldAsmId=44 → newAsmId=32

The recon (with debug fprintfs added at the same C++ sites)
showed:
- required=252000 cacheSize=131072 appendMode=0
- split_=0 before optimize check
- Prefetch::optimize called
- visiting Load node asmId=44 parent=SetVar (type 16)
- Load with SetVar parent: regsMatch=1
- regsMatch but parentLoad type wrong: type=16

— the third-line diagnostic ("type=16") immediately
identified the inverted gate.  Re-examining the disasm at
0x1cdeae confirmed `je SKIP` (skip when type==Loop) rather
than `jne SKIP` (skip when type!=Loop).

**Lesson**: when a binary has a `cmp X, K; je TARGET`
pattern, the rewrite (or whatever is in the fallthrough
path) executes when `X != K`.  This is easy to invert when
hand-translating to C++ because the natural English reading
of the comment ("if parentLoad type is Loop, then ...") often
ends up describing the **skipped** branch rather than the
executed one.  Always cross-check polarity with a runtime
trace when the recon predicate looks right but the
behavior is wrong.

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

## IF-106  Inline `asm volatile("maxsd …")` in `util_wave.cpp` replaced with `_mm_max_sd` intrinsic

**Source**: Phase 39a code-smell sweep
**Status**: fixed (2026-04-29, Phase 39a)
**Severity**: cosmetic + portability cleanup (no behavioural change on
non-NaN input, identical machine code on NaN input)

### What was there

`util_wave.cpp:96` (single inline-asm site in the entire codebase):

```cpp
double lo = -1.0;
asm volatile("maxsd %1, %0" : "+x"(lo) : "x"(sample));
scaled = lo * kFullScale;
```

The accompanying comments included a half-finished alternative
(`double clamped = (sample >= -1.0) ? sample : -1.0;`) that was
computed and then dead-stored, plus a "Let's just use:" preamble
that left the rationale unclear.

### Why the inline asm existed

The binary uses x86 SSE2 `maxsd` whose NaN-propagation rule is
"if either operand is NaN, return the second source". No portable
C++ construct replicates this exactly:

| Construct | NaN behaviour | Matches `maxsd src, dst`? |
|---|---|---|
| `std::max(-1.0, NaN)` | unspecified (typically returns first arg) | ✗ |
| `std::fmax(-1.0, NaN)` | returns the non-NaN argument | ✗ |
| `(sample >= -1.0) ? sample : -1.0` | NaN compares false → returns `-1.0` | ✗ |
| `_mm_max_sd(_mm_set_sd(-1.0), _mm_set_sd(sample))` | returns second source on NaN | ✓ |

### Fix

Replaced with the SSE2 intrinsic from `<emmintrin.h>`. GCC, Clang
and MSVC all lower `_mm_max_sd` to a single `maxsd`, so the emitted
instruction is identical to the original binary's. The `clamped`
dead variable and stale comment were removed; rationale was
collapsed into a single coherent block.

Verified: `cmake --build .` clean, `python tests/diff_test.py`
remains 257/259 (same baseline; the two failures are the unrelated
libc++ PRNG ABI cases).

### Scan-coverage note

The earlier Phase-39 read-only scan used regex
`\basm\s*[\(\{]|__asm__|__asm\b` which missed `asm volatile "…"`
(the `volatile` qualifier sits between `asm` and the string literal,
which is followed by a string token rather than `(`). The correct
pattern is `\basm\s+volatile|\basm\s*[\(\{]|__asm__`. After this
fix, the codebase has zero inline-asm sites under either pattern.

## IF-107  `Prefetch::determineFixedWaves` enqueued `next` twice causing O(2^N) BFS

**Severity**: critical (perf), confirmed-fixed
**File**: `reconstructed/src/codegen/prefetch_helpers.cpp:589` (function
`Prefetch::determineFixedWaves`, binary 0x1cb200)

**Symptom**: `hdawg_cvar_unroll` test ran 155× slower than original
(4500ms vs 29ms). Profile via GDB attach-sampling showed 100% of
samples in this function's worklist pop / shared_ptr destruction.
Per-loop timing showed exponential growth: 1 loop=3ms, 2=6ms, 3=13ms,
4=390ms, 5=5500ms.

**Root cause**: The recon enqueue block was

```cpp
if (cur->next) worklist.push_back(cur->next);
for (auto& child : cur->branches) worklist.push_back(child);
if (cur->next) worklist.push_back(cur->next);  // BUG (was elseBranch)
```

The duplicate `next` push doubles the worklist on every step in a
sibling chain → O(2^N). The "(was elseBranch)" comment captured the
origin: during recon, two distinct Node fields were collapsed onto
`next`.

**Truth from binary disassembly** (0x1cbb80..0x1cbe17):
- Always push `next` (+0xB8) if non-null.
- If `type == Loop` (0x8): push `loop` (+0xE0) if non-null.
- If `type == Branch` (0x4): iterate `branches` (+0xC8..+0xD0) and
  push each.
- Loop and Branch are mutually exclusive; the binary uses a single
  `cmp eax, 0x8 / je / cmp eax, 0x4 / jne` chain.

The recon was wrong on two axes: it pushed `branches` unconditionally
(should be Branch-only) and pushed `next` twice (should be `loop` for
Loop nodes only).

**Fix**: Replaced enqueue block with the type-gated logic matching the
binary; moved enqueueing to the end of the loop body where the binary
places it (executed for all node types, not only Play).

**Result**: `hdawg_cvar_unroll` 4500ms → 11ms. Full suite 257/259
preserved (only the 2 known libc++ PRNG ABI failures remain).

## IF-108  `WaveformGenerator::rand` uses MINSTD LCG, not MT19937_64

**Severity**: critical (correctness), confirmed-fixed
**File**: `reconstructed/src/waveform/waveform_generator.cpp:1548`
(`WaveformGenerator::rand`, binary 0x251cf0); algorithm in
`reconstructed/src/infra/prng_libcxx.cpp:seqc_minstd_normal_amplitude`.

**Symptom**: `hdawg_doc_random_waves` and `hdawg_doc_randomSeed`
differential tests failed (recon `.wf___rand_*` bytes differed from
original). Initial assumption: libc++ vs libstdc++ `mt19937_64` +
`normal_distribution` ABI mismatch. Recon was routing `rand` through
the same libc++ MT19937_64 shim used by `randomGauss` and
`randomUniform`. The shim was correct for the latter two, but
produced output that matched the binary's `randomGauss` for `rand` —
indicating `rand` and `randomGauss` use **different** PRNGs.

**Root cause**: `WaveformGenerator::rand` does NOT use the shared
`GlobalResources::random[]` MT19937_64. It uses a custom inline
**Park-Miller MINSTD LCG** with state reset to 1 at the start of
every call, followed by a Marsaglia polar Box-Muller transform.

**Truth from binary disassembly** (0x251cf0..0x252800):

- Initial LCG state: `mov $0x1, %edx` at 0x25255c (constant seed=1
  per call — verified by `hdawg_doc_randomSeed` producing two
  byte-identical waveforms despite an intervening `randomSeed()`).
- Multiplier: `imul $0xbc8f, %rdx, %rcx` at 0x2525f0 (= 48271, the
  Park-Miller minstd multiplier).
- Modulus reduction by `2^31 - 1`: vectorized Granlund-Möller mod
  using `mul r12` (r12 = 0x200000005) plus `shr/shl/sub/add` chain.
- 4 LCG samples per Box-Muller trial (vectorized into xmm regs).
- Two uniforms in `[-1, 1)` per trial via the libc++/libstdc++
  uniform_real_distribution-style combine pattern using rodata
  constants at 0x8fc680..0x8fc6e0:
    * 0x8fc680: low-32 bitmask `[0xFFFFFFFF, 0]×2`
    * 0x8fc690: `2^52` (low-half bias)
    * 0x8fc6a0: `2^53` (high-half bias)
    * 0x8fc6b0: `2^53 + 2^32` (combined bias for unbiasing)
    * 0x8fc6c0: `2^31 - 2 = 2147483646` (range divisor)
    * 0x8fc6d0: `(2^31 - 2)^2 ≈ 4.6116e18` (range²)
    * 0x8fc6e0: `-1.0` (offset to map [0,2) → [-1,1))
  Combine: `u = 2 * (low + high * (2^31-2)) / (2^31-2)^2 - 1`.
- Marsaglia polar rejection at 0x252711..0x252723:
  `s = u² + v²`; if `s ≥ 1` (`ucomisd s, xmm10=1.0; ja`) or `s == 0`
  (`ucomisd s, 0; jne done; jnp regen`), regenerate.
- Acceptance: `factor = sqrt(-2 ln(s) / s)`. Two normals per pair.
- **Output emit order**: lane 1 (`v*factor`) first via `unpckhpd`,
  then lane 0 (`u*factor`) cached and emitted on the next outer
  iteration. Verified by sample comparison.
- Final sample: `(z * stddev + mean) * amplitude` then
  `Signal::append(value, 0)`.

**Cross-check with sibling functions**:
- `randomGauss` (0x252930) calls `std::__1::normal_distribution<double>::
  operator()<mersenne_twister_engine<...>>` at 0x253207, with TLS
  state from `GlobalResources::random` — pure libc++ MT19937_64.
- `randomUniform` (0x253440) inlines the MT19937_64 tempering
  (`shl 0x25, xor; shr 0x2b, xor; ...`) and uniform_real_distribution
  conversion at 0x2539a0..0x2539e0. Also pure libc++ MT64.
- `randomSeed` (custom_functions_playback.cpp:875) calls
  `Random::seedRandom` (0x16be80) which seeds the MT19937_64 state.
  It does NOT touch the LCG (which is reset to 1 every `rand()`
  call regardless), so `randomSeed()` is effectively a no-op for
  `rand` but reseeds `randomGauss`/`randomUniform`.

**Why the libc++ shim approach worked for randomGauss/randomUniform
but not rand**: the libc++ vs libstdc++ Box-Muller pair-order
hypothesis was correct for those two functions (which DO use libc++
`normal_distribution` / `uniform_real_distribution`). But `rand` was
never using the stdlib distribution at all — it was using the
hand-rolled inline MINSTD + polar implementation in the binary, so
no stdlib shim could fix it. Reverse-engineering the assembly
revealed the actual algorithm.

**Fix**: Added `seqc_minstd_normal_amplitude()` to
`reconstructed/src/infra/prng_libcxx.cpp` (portable C++; no stdlib PRNG).
Wired into `WaveformGenerator::rand` (`waveform_generator.cpp:1548`),
replacing the prior libc++ MT64 shim call. `randomGauss` and
`randomUniform` continue to use the libc++ shim unchanged.

**Verification**:
- Standalone C++ verifier (`/tmp/verify_minstd.cpp`) reproduces
  first 12 samples of `rand(1024, 1.0, 0, 0.1)` byte-identical to
  original ELF.
- `hdawg_doc_randomSeed` produces two byte-identical waveforms
  (proves `randomSeed()` does not affect `rand`'s LCG seed).
- Full diff suite: **259/259 passing** (was 257/259).

**Lesson**: Whenever a "stdlib ABI mismatch" hypothesis is on the
table for a single failing function, look at what stdlib calls the
binary actually makes. If there are no `call` instructions to
distribution operators in the function, it's not a stdlib problem —
it's a hand-rolled inline implementation. Disassemble before assuming.

---

## IF-109  Bytes-vs-samples confusion across wave-memory subsystem

- **Source**: audit batches 02, 14, 16, 36, 46, 48
- **Severity**: likely-bug
- **Status**: **fixed** (Phase 44.1, 2026-05-05)
- **Description**: Multiple subsystems conflate byte counts with sample counts when computing wave memory sizes, offsets, and allocation. Parameters named `size` or `length` are sometimes in bytes (×2 for 16-bit samples) and sometimes in samples, with no consistent convention.
- **Fix**: Renamed all misnamed identifiers in Phase 44.1:
  - `MemoryAllocator::memorySizeInSamples_` → `memorySizeInBytes_` (holds `dc->waveformMemorySize`, a byte count)
  - `MemoryAllocator::cacheLineSize_` → `cacheLineSizeBytes_` (holds `dc->waveformAlignment` = 4096 bytes)
  - `Cache::allocate` / `getBestPosition` parameter `numSamples` → `numBytes` (receives `totalBits/8`)
  - `prefetch.cpp` local `numSamplesForCache` → `numBytesForCache`
  - `memory_allocator.cpp` line 184: `cacheLineSize_` → `cacheLineSizeBytes_` in code (not just comments)
  - All 1341 tests pass after renames.

---

## IF-110  `Value::pad_04_` is not padding; `subType_` shape unclear

- **Source**: audit batch 11
- **Severity**: suspicious
- **Status**: **dismissed** (Phase R, GDB-confirmed 2026-04-29) — `pad_04_` is genuine alignment padding
- **Description**: The field named `pad_04_` in `Value` was suspected of being accessed by binary code and therefore not mere padding. Additionally, `subType_` has a shape (enum? bitfield?) that doesn't match the current `int` declaration.
- **Phase R source review**: confirmed that `EvalResultValue::varSubType_` (a `VarSubType` enum at outer-struct +0x4) is the slot copied at `resources.cpp:1686` (`out.subType_ = var->subTypeRaw`) — that is **not** `Value::pad_04_` (which is at inner Value+0x04, embedded at EvalResultValue+0x08+0x04 = +0xC). The current recon treats `Value::pad_04_` as padding (`value.hpp:160`).
- **Phase R disasm sweep** (objdump on `_seqc_compiler.so`):
  - `Value::toDouble`@0x15a560..0x15a780 — every read of the Value object uses offsets `(%rdi)`, `0x8(%rdi)`, `0x10(%rdi)`. **No `0x4(%rdi)` reads.**
  - `Value::toInt`@0x15c250..0x15c4f0 — same: only `(%rdi)` (type_), `0x8(%rdi)` (which_), `0x10(%rdi)` (storage). **No `0x4` reads.**
  - `Value::operator==`@0x21a780..0x21aa00 — only `(%rdi)`, `(%rsi)`, `0x8(%rdi)`, `0x10(%rdi)`, `0x18(%rdi)`. **No `0x4` reads.**
  - `Value::toBool`@0x164200, `Value::toString`@0x15de50, `~Value`@0x15a9c0 — all checked, **no `0x4(%rdi)` reads.**
- **Phase R GDB trace** (`/tmp/if110_trace.txt`, 17 hits across `toDouble`/`toInt` driven by `var c = a + b; const PI = 3.14159;` style snippets through HDAWG8 path):
  - At every hit, `+0x0=3` (Double) and `+0x8=2` (which==Double) — consistent.
  - `+0x4` slot values observed: `0`, `21845` (0x5555), `32512` (0x7F00), `32767` (0x7FFF). The same `this` pointer (e.g. `0x7fffffffc3a0`) shows different `+0x4` values across hits (0, 0, 21845). These are classic uninitialized-memory bit patterns, not deterministic state.
  - If `+0x4` were a sub-type tag, values would be small and deterministic per `(type_, which_)` pair. They are not.
- **Conclusion**: `Value::pad_04_` is genuinely uninitialized alignment padding (4 bytes between `type_:int32` and the 8-byte-aligned `which_`). The recon name is correct; no source change needed. GDB evidence preserved at `/tmp/if110_trace.txt`.

---

## IF-111  `namespace Assembler` + `AssemblerInstr` should be one class

- **Source**: audit batches 26, 33 (type-suspicion + logic-bug: recon split of one binary class)
- **Severity**: suspicious
- **Status**: **fixed** (Phase D commit `5a44521`, Cluster M)
- **Description**: The binary implements a single `Assembler` class with instruction-building methods. The reconstruction artificially split this into a free-function namespace `Assembler` and a separate `AssemblerInstr` struct, causing awkward coupling and potential semantic drift.
- **Resolution**: Phase D commit `5a44521` merged `namespace Assembler` functions into a unified `class Assembler` (formerly `AssemblerInstr`). All call sites updated; tests 259/259 throughout.

---

## IF-112  `NodeMapItem::hasFast` int conflated with `AccessMode` enum

- **Source**: audit batches 27 (type-suspicion + logic-bug)
- **Severity**: cosmetic (downgraded from likely-bug)
- **Status**: **dismissed** (Phase R) — GDB-confirmed bool typing is correct
- **Description**: `NodeMapItem::hasFast` was suspected of carrying an `AccessMode` enum value (Soft=0/Direct=1/Custom=2) because the playback dispatch reads it as `static_cast<AccessMode>(node.hasFast)` (custom_functions_play.cpp:1511) while other call sites pass literal `static_cast<AccessMode>(2)` (Custom) to `addNodeAccess`.
- **Phase R GDB resolution** (commit per Phase R wrap-up):
  - Trace plan: break at `lookupNode` @ `0x15c582` (after typeIdx/fastAddr/hasFast stored into the sret buffer at `[rbx+0x08..0x10]`); print `*(unsigned char*)($rbx+0x10)` per hit.
  - Driver: `/tmp/if112_trace.py` compiles every entry from `tests/cases/manifest.json` (all device families, ~50 cases) through the original `_seqc_compiler.so`.
  - Result (`/tmp/if112_trace.txt`): **51 hits, distribution `0`:41, `1`:10, `2`:0**. Only `0` and `1` ever appear.
  - Static cross-check: every writer of `hasFast` lives in `get_node_map.cpp` (`addDirect`/`addVirt` helpers) and only ever passes literal `false` (default) or `true`. `Custom(2)` enters the system exclusively via the **other** argument of `addNodeAccess` (the `mode` parameter at custom_functions_io.cpp:89, :1543, :1580, :1680, :1687, :3120, :3225, :3325) — never via `hasFast`.
  - Decision: keep `bool hasFast` typing. The `AccessMode(hasFast)` cast at custom_functions_play.cpp:1511 is intentional — `false` → `Soft(0)` (no fast address ⇒ slow/soft access), `true` → `Direct(1)` (fast address ⇒ direct dispatch). It coincidentally produces the correct `AccessMode` value because the field is overloaded but never sees Custom(2).
- **Action taken**: no structural change. Field comment in `node_map_data.hpp:106-118` and inline comment at `custom_functions_play.cpp:1511` updated to document the dual-role / GDB result. Joint resolution with **Arb 5** (was: deferred type-fix int→AccessMode); Arb 5 is now closed as "rejected, bool is correct".

---

## IF-113  `Cache::Pointer::hash_` is not a hash; prefetch wrap-address semantics

- **Source**: audit batch 36
- **Severity**: suspicious
- **Status**: **kept** (Phase R) — formula is hash-like; consumer role documented
- **Description**: The field named `hash_` in `Cache::Pointer` does not store a hash value — binary analysis shows it holds a wrap-around address or index used by the prefetch subsystem. Misnaming obscures the actual cache-line logic.
- **Resolution**: Source review of all writers/readers:
  - Written only in `cache.cpp:132` for the Aligned cache type:
    `ptr->hash_ = ~(ptr->position_ ^ (ptr->position_ + halfSz));` — this is in fact a hash-like derivation of `position_` (XOR with a folded sum, then bitwise NOT).
  - Cleared in `cache.cpp:184` (`pointer->hash_ = 0`) on the Normal path.
  - Read at `prefetch_splitplay.cpp:325` and passed to `insertPlay` as the "start-address hash key" (per the comment at :321). The receiver uses it as an index/key, not as an address that can be dereferenced.
  - Read at `cached_parser.cpp:60,247` in a different `hash_` field (`CachedParser::hash_`, a `std::vector<uint>`) — unrelated.
  The value is genuinely a hash of the position; the prefetch consumer uses it as a hash key to resolve the wrap. Name kept; consumer comment at `prefetch_splitplay.cpp:321` documents the wrap role.

---

## IF-114  `PlayConfig::now` named 'now' but read as 4-channel flag

- **Source**: audit batch 38
- **Severity**: suspicious
- **Status**: **kept** (Phase R) — name preserved for binary-contract fidelity
- **Description**: `PlayConfig::now` is named as if it means "play immediately" but the binary reads it as a flag indicating 4-channel playback mode. Callers testing `now` may misinterpret the semantics.
- **Resolution**: The JSON serialization key in the binary is literally `"now"` (`play_config.cpp:127`: `pc.now = obj.at("now").as_bool();`). Renaming the field would diverge the JSON contract from the binary's serialization. Producer site `asm_commands.cpp:984` writes `config.now = playNow;` from a parameter named `playNow`, and consumer sites in `prefetch_*` use the value as `is4Ch` because in this hardware the "play-now" path coincides with 4-channel mode. The dual-meaning is documented at `prefetch_splitplay.cpp:39` and the `genPlayConfig::isFourChannelBool` audit (batch 10). Field name kept as `now`; semantic dual use is documented locally.

---

## IF-115  Strict/useAbsolute/showLine polarity-inverted booleans

- **Source**: audit batches 39, 47, 52
- **Severity**: likely-bug
- **Status**: **partially dismissed** (2026-05-05)
- **Severity**: likely-bug (was; remaining items cosmetic/dead)
- **Description**: Several boolean parameters had inverted polarity. Resolution per item:
  - `useAbsolute`: **fixed** by IF-117 (renamed to `useMapped`).
  - `MathCompiler::functionExists(..., bool strict)`: `return strict | found` matches binary exactly — the inversion is in the binary too; dead-path only (call site hardcodes `false`). No action needed.
  - `showLine`: not found in current source — either renamed in an earlier phase or belongs to a not-yet-reconstructed component. Cannot audit; dismissed pending future work.

---

## IF-116  `Expression::valueType` int slot is actually `EDirection` enum

- **Source**: audit batch 42
- **Severity**: suspicious
- **Status**: **fixed** (2026-04-29) — type-fix complete
- **Description**: `Expression::valueType` is declared as `int` but the binary only stores values 0/1/2 corresponding to `EDirection` (input/output/inout or similar). Using raw int allows invalid values and obscures intent.
- **Resolution**:
  - **Rename done**: the field is now named `direction` (not `valueType`) at `expression.hpp:125`.
  - **Type-fix applied** (2026-04-29): Changed `int32_t direction` → `EDirection direction` in `expression.hpp:125`. Updated all 5 assignment sites in `expression.cpp` (lines 181, 249, 412, 439, 574) to use `EDirection::eINOUT`. Updated all 13 assignment sites in `seqc_parser.tab.c` to use `EDirection::eIN`. Updated default constructor to use `EDirection::eINOUT`. Build clean, 259/259 tests pass.

---

## IF-117  `addWaveform::useAbsolute` polarity confirmed; flipped with rename

- **Source**: audit batch 47
- **Severity**: likely-bug
- **Status**: fixed (commit e22c1b5, renamed to `useMapped`)
- **Description**: The `useAbsolute` parameter had inverted polarity — `true` meant "use mapped addressing". Confirmed via binary trace and fixed by renaming to `useMapped` with correct polarity in phase-D commit 14.
- **Action**: None — resolved.

---

## IF-118  `AddressImpl<T>` wrapper overgeneral

- **Source**: audit batch 48
- **Severity**: suspicious
- **Status**: **kept** (Phase R) — instantiation set verified, de-templating not worth churn
- **Description**: `AddressImpl<T>` is templated but only instantiated with one type in the binary. The template adds complexity (SFINAE, specialization surface) without benefit and may mask the actual address-width semantics.
- **Resolution**: Source-only audit of all instantiations (~300 sites) shows `AddressImpl` is parameterized as `<unsigned int>` and `<uint32_t>` — these are the **same type** on the target ABI but appear textually distinct across files. Concrete instantiations: `appendSuser` (custom_functions_io.cpp, custom_functions_play.cpp, ~80 sites), `Cache` constructors and `getSize()` (cache.cpp/hpp), `WavetableFront::WavetableFront` (wavetable_front.cpp:52, wavetable_ir.cpp), `PlayConfig` member fields `channelMask/suppress/markerBits/trigger/precompFlags` (play_config.hpp:30-37), `Immediate` variant slot (struct_layouts.md:215-225). De-templating to a concrete `AddressImpl` would require touching every call site and would not change behavior since only one width (32-bit unsigned) is ever instantiated. Kept as-is; the template form does not currently hide misnamed semantics.

---

## IF-119  `setPRNGSeed` integer-literal path constructs `AsmRegister` from value

- **Source**: audit batch 05c2
- **Severity**: likely-bug
- **Status**: **fixed** (Phase R)
- **Description**: When `setPRNGSeed` receives an integer literal, the reconstruction passes the raw integer value to `AsmRegister` constructor, treating the seed value as a register number. The binary instead emits an immediate-load instruction.
- **Phase R source audit**: at `custom_functions_io.cpp:2773` the integer-literal branch (`argType == 2`, comment "Integer literal path") constructs `AsmRegister(args[0].value_.toInt())` from the value rather than using `args[0].reg_`. In every sibling method (e.g. `setSweepStep:3161`), `argType == 2` is the **register** branch and code uses `args[0].reg_`. Either the comment is wrong (this is the register branch and the value-toInt path is a logic bug) OR `varType_==2` is overloaded only here.
- **Resolution (disasm, no GDB needed)**: `objdump -d` on `_seqc_compiler.so` at @0x1513e0 shows the `argType == 2` branch (= `VarType_Var`, variable-bound) is in fact the **register** branch:
  - @0x151507: `mov rdx, QWORD PTR [rbx+0x30]` loads `args[0].reg_` (the 8-byte `AsmRegister` field at +0x30) into `rdx`.
  - @0x15150f–0x151512: `cmp eax, 0x2 / jne 1515a6` — `rdx` is preserved across the branch.
  - @0x151518–0x151528: sets `rdi/rsi/ecx` (vec, asmCommands, addr=`kSuserPrngSeed`=0x74) and calls `AsmCommands::suser(AsmRegister, AddressImpl<uint>)` with `rdx` still holding `args[0].reg_`.
  - The integer-literal handling actually lives in the `(argType & ~2) == 4` branch (matches `VarType_Const`/`VarType_Cvar`), which already allocates a register and emits `addi` to load the seed immediate — that branch was already correct.
  - Sibling-method evidence (e.g. `setSweepStep`, `wvft@2734-2738`) confirms `argType==2` is the register/Var branch everywhere.
- **Fix**: replaced `AsmRegister(args[0].value_.toInt())` with `args[0].reg_`, and rewrote the misleading "Integer literal path" comment to "Variable path (VarType_Var)". `custom_functions_io.cpp:2771-2778`. Tests 259/259 (no test currently exercises `setPRNGSeed(varName)` so the prior bug had no observable effect on the suite, but the source now matches the binary).
- **Trace logs**: `/tmp/if119_var_trace.txt`, `/tmp/if119_literal_trace.txt`.

---

## IF-120  `configFreqSweep` magic literals — constants exist but are unused

- **Source**: audit batch 05c
- **Severity**: suspicious
- **Status**: **fixed** (Phase R)
- **Description**: Named constants for `configFreqSweep` magic values exist in the reconstruction (from rodata) but the implementation uses inline numeric literals instead. This makes maintenance harder and risks drift.
- **Resolution**: Replaced all 4 remaining raw `0x8c`/`0x8d`/`0x8e`/`0x8f` SUSER addresses in `custom_functions_io.cpp::configFreqSweep` and the per-step variant with `kSuserSweepOscIdx` / `kSuserSweepControl` / `kSuserSweepStartLo` / `kSuserSweepStartHi` from `types.hpp`. (Two earlier sites in the function had already been migrated.) Tests 259/259.

---

## IF-121  `DeviceOpts` namespace full duplicate of anonymous-namespace `k*` set

- **Source**: audit batch 22
- **Severity**: suspicious
- **Status**: **fixed** (Phase R, removed dead namespace from `device_factories.hpp`)
- **Description**: `DeviceOpts` namespace defines the same constant set as the anonymous-namespace `k*` constants (e.g. `kMaxWaveLen`, `kMinGranularity`). One is redundant and may diverge silently.
- **Resolution**: Verified `DeviceOpts::*` had zero references in source. The live set lives in `device_factories.cpp` (`kSubtypeMask`, `kSubtype1..4`). Dead `DeviceOpts` namespace removed; tests 259/259.

---

## IF-123  `SeqcParserContext::errorCallback_` never wired in recon `Compiler` constructor

- **Source**: hdawg_func_return_int difftest investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The original binary's `Compiler::Compiler` (at ctor+0x2cb) calls
  `parserContext_.setErrorCallback(lambda)` where the lambda body is
  `messages_.parserMessage(lineNr, msg)`. The recon constructor never set this
  callback, leaving `SeqcParserContext::raiseError` a no-op — syntax errors from
  the bison parser were silently discarded (no error message in `messages_`), so
  `messages_.hadCompilerError()` returned false and the compiler continued past
  a parse failure.
- **Resolution**: Added `parserContext_.setErrorCallback(...)` call to
  `Compiler::Compiler` in `reconstructed/src/codegen/compiler.cpp`. GDB-confirmed: the
  lambda vtable at binary offset `0x9e62ac+rip` at ctor+685 disassembles to
  `mov 0x8(%rdi),%rdi; mov (%rsi),%esi; add $0x38,%rdi; jmp parserMessage`
  (captures `this`; `0x38` = `messages_` offset).

## IF-124  `Compiler::compile` null-seqcAst crash (libstdc++ vs libc++ ABI)

- **Source**: hdawg_func_return_int difftest investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: When the SeqC parser fails (seqcAst is null), the recon
  calls `FrontEndLoweringFacade::lower(..., *seqcAst, ...)`, triggering
  libstdc++ `__shared_ptr_deref` assertion (SIGABRT). The original binary
  uses libc++ which has different null-deref behavior for shared_ptrs.
  GDB-confirmed: binary at `Compiler::compile+1644` has `test %rbx,%rbx;
  je +1671` — a null guard on the raw seqcAst pointer; if null, jumps past
  refcount and into `lower()` where libc++'s ABI tolerates the null.
- **Resolution**: Wrapped steps 7–8 (SeqC AST print + frontend lowering) in
  `if (seqcAst)` guard in `compiler.cpp::Compiler::compile`. With IF-123 also
  fixed, the parse error is now properly stored in `messages_` so step 9's
  `hadCompilerError()` check throws the right exception.

## IF-125  `SeqCBreakStatement::evaluate` had spurious `inSwitch_` guard

- **Source**: hdawg_switch_basic / hdawg_switch_fallthrough difftest investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The recon's `SeqCBreakStatement::evaluate` had an
  `if (!state.inSwitch_)` guard that skipped the error when inside a switch.
  The original binary at `0x226970` has no such branch: the first instructions
  are `ErrorMessages::format(0xd5, "break")` + `errorMessage(...)` unconditionally,
  followed by allocating and returning an empty `EvalResults`.
  GDB-confirmed: traced with a switch-body input; `inSwitch_=0x61` (true) but
  the binary still calls `errorMessage` and returns empty EvalResults.
- **Resolution**: Removed the `if (!state.inSwitch_)` guard. The function now
  unconditionally emits error 0xd5 and returns empty `EvalResults`. This fixed
  `hdawg_switch_basic` (which had break outside switch — was being silently
  ignored). The `hdawg_switch_fallthrough` inline case (break inside switch)
  remains failing because the recon is missing the `playZero(16)` warning —
  a separate pre-existing switch evaluation order issue.

## IF-126  `seqc_parser.tab.c` missing verbose error output; `EDirection` cast errors in grammar

- **Source**: `hdawg_func_return_int` / `hdawg_func_nested_call` investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The pre-existing `seqc_parser.tab.c` was generated without `%define parse.error verbose`, so `yyerror` / `seqc_error` received only the bare string `"syntax error"` instead of the detailed `"syntax error, unexpected IDENTIFIER, expecting ';'"` that the original binary produces. Separately, the grammar file used integer literals `0` for `direction` field assignments instead of `EDirection::eIN`, which caused compile errors when regenerating with a stricter bison version.
- **Resolution**: Added `%define parse.error verbose` to `seqc_parser.y`, fixed all `->direction = 0` assignments to `EDirection::eIN`, regenerated `seqc_parser.tab.c` and `seqc_parser.tab.h`. The bison-generated verbose error strings now match the original binary output exactly. Tests `hdawg_func_return_int` and `hdawg_func_nested_call` pass; full suite 326/327.

## IF-127  `evalCases` fallthrough: recon grammar nests `case 0: case 1:` but binary has flat list

- **Source**: `hdawg_switch_fallthrough` (inline code variant) investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The recon grammar resolves `case 0: case 1: stmt;` with a shift (Rule 94), producing a nested AST where `case 1: stmt` is the body of `case 0:`. The binary grammar resolves with a reduce (Rule 96), producing a flat list: `case 0` (null body), `case 1` (stmt body), `break` (bare sibling). Consequence: in the recon's `evalCases`, `case 1`'s body (`playZero(16)`) was never evaluated (it was inside `case 0`'s body as a case label only), so the playZero warning was not emitted before the break error. GDB-confirmed: original binary has `case 0` with `body()=nullptr` (pointer at offset +0x20 is 0x0).
- **Root cause**: Shift/reduce conflict in the grammar; recon bison version (3.8.2, 232 states) resolves differently than the original binary's bison version (233 states).
- **Resolution**: Fixed in the grammar: changed Rules 94 and 95 (`KW_CASE expr ':' statement` and `KW_DEFAULT ':' statement`) to use `unlabeled_statement` instead of `statement`. This eliminates the shift/reduce conflict for `KW_CASE`/`KW_DEFAULT` tokens, forcing the parser to reduce with Rule 96/97 (no body) when another case label follows. The recon now produces a flat AST identical to the binary — GDB-confirmed: `case 0` body pointer at +0x20 is 0x0 in both. Parser tables regenerated. All 327 tests pass.

## IF-122  `Resources::parent_` strong/weak pointer inversion

- **Source**: audit batch 19a
- **Severity**: likely-bug
- **Status**: **fixed** (Phase D commit `612eb2a`, Cluster N)
- **Description**: `Resources::parent_` is stored as a `shared_ptr` (strong reference) but the binary uses a `weak_ptr` to avoid reference cycles in the resource tree. The strong reference prevents parent deallocation and causes memory leaks in deep scope chains.
- **Resolution**: Phase D commit `612eb2a` swapped the strong/weak slots and renamed the strong slot to `grandparent_` (the binary's actual semantic — the recon's `parent_` was at +0x18, the true direct-parent weak slot at +0x28; what `parent_` referred to was a transitively-owned grandparent). All access sites updated; tests 259/259 throughout.

## IF-128  `Value::toInt()` missing uint32_t overflow retry for large hex literals

- **Source**: `ziai_analyze_setDIO` difftest investigation
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The `Value::toInt()` Double case used `static_cast<int32_t>(floor(d))` unconditionally. For hex literals like `0xAAAAAAAA` (2863311530.0 as double), this is undefined behavior — the value exceeds INT32_MAX. The binary's `toInt()` at 0x15c250 uses the x86 `cvttsd2si` instruction which returns the "indefinite" sentinel 0x80000000 on overflow, then detects the overflow and retries via `uint32_t` truncation. The recon omitted this retry, yielding wrong values (UB in practice produced 0x80000000 = -2147483648, which then split incorrectly by `addi32` into addi imm=0x0 and addiu imm=0x80000 instead of the correct 0xAAA/0xAAAAA). The symptom was `setDIO(0xAAAAAAAA)` emitting wrong machine code: `addi R1,R0,0` + `addiu R1,R1,524288` instead of `addi R1,R0,2730` + `addiu R1,R1,699050`.
- **Root cause**: Missing overflow path in `Value::toInt()` for the Double case. The comment at the top of the function noted "On int overflow, catches and retries as uint32_t" but the implementation lacked this logic.
- **Resolution**: Added range check in the Double case; values outside [INT32_MIN, INT32_MAX] are cast via `static_cast<int32_t>(static_cast<uint32_t>(truncated))`, which wraps correctly and matches the binary behavior. All 13 `ziai_analyze_setDIO_*` tests now pass byte-identical. No regressions (776 pass, 3 equiv, 12 failed — same as before).

## IF-129  `resetOscPhase`: UHFQA uses direct pulse-reset, not `oscs/phasereset` node write

- **Source**: `ziai_analyze_resetOscPhase_uhfqa` difftest failure
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The recon's `resetOscPhase()` had a single `else if (devType >= 2 && devType <= 0x20)` branch covering all "Hirzel" devices (HDAWG, UHFQA). For HDAWG, the correct behaviour is to call `writeToNode("oscs/phasereset", oscMask)`. But for UHFQA, the original binary uses a **completely different code path** — GDB-confirmed via jump table at `0x14043e` in the binary: UHFQA (devType=4, index=2) jumps to `0x1405ba`, not to the HDAWG path at `0x14044e`. The UHFQA path emits a pulse-reset sequence directly: `addi R_n, R0, 1; st R_n, 0x5f; st R0, 0x5f`. This writes 1 then 0 to hardware register address `0x5f` (phasereset). No node write is performed. The UHFQA node map does not contain `"oscs/phasereset"`, so the recon's `lookupNode("oscs/phasereset")` call threw a compilation error.
- **Root cause**: The recon incorrectly grouped UHFQA with the HDAWG (Hirzel) path in a single `else if` branch, missing that the binary uses per-device jump-table dispatch and UHFQA has its own distinct path.
- **Resolution**: Added a UHFQA-specific branch before the generic Hirzel branch in `custom_functions_io.cpp::resetOscPhase()` (`reconstructed/src/runtime/custom_functions_io.cpp:1419`). UHFQA now emits the direct `addi/st/st` pulse-reset sequence to register `0x5f`. GDB-verified: binary offsets `0x140858` and `0x14095c` both execute `st(..., 0x5f)` with reg=1 then reg=0. All 11 `resetOscPhase` tests now pass byte-identical (788 total passing, 0 failed).

## IF-130  `incrementSinePhase` Phase 3 path uses `awgIndex` instead of `oscIndex`

- **Source**: `ziai_analyze_incrementSinePhase_hdawg8/hdawg4` difftest failure
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `CustomFunctions::incrementSinePhase()` in `custom_functions_io.cpp` is structured as four separate `if (devType == ...)` phases. Phase 1 (devType==2, HDAWG) reads `oscIndex = args[0].value_.toInt()` and uses it for suser codegen. Phase 3 (also devType==2) builds the node path for `addNodeAccess`. However, Phase 3 incorrectly used `config_->awgIndex` (the AWG channel index, always 0 for index=0 tests) instead of `oscIndex` (the sine generator argument from the user). For `incrementSinePhase(1, 45.0)`, this produced `nodes_json: {"nodes":[{"name":"SINEPHASE","index":[0],...}]}` instead of the correct `index:[1]`. The `setSinePhase` counterpart correctly uses `oscIndex` in its single combined `if (devType==2)` block (both suser and node path in the same scope). The bug arose because `incrementSinePhase` split the two operations across separate `if` blocks, placing `oscIndex` out of scope for Phase 3.
- **Root cause**: Scoping error — `oscIndex` is declared inside Phase 1's `if` block but needed again in Phase 3's separate `if` block. The fix re-reads `oscIndex` from `args[0].value_.toInt()` in Phase 3.
- **Resolution**: Changed line 1680 in `custom_functions_io.cpp`: added `int oscIndex = args[0].value_.toInt();` and replaced `config_->awgIndex` with `oscIndex` in the path string. Confirmed by running the original binary directly (outputs `index:[1]` for `incrementSinePhase(1, 45.0)`). All 10 `incrementSinePhase` tests now pass byte-identical. Full suite: 788 passed, 3 equiv, 0 failed.

## IF-131  `alui` Case 3 (large-immediate ORI/ANDI/XNORI): `alur` dst/src arguments reversed

- **Source**: `ziasm_register_arithmetic_11` difftest failure (all 13 devices)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `AsmCommands::alui()` in `asm_commands.cpp` handles the case where a bitwise OR/AND/XNOR immediate is too large for a single immediate instruction. It loads the constant into `dst` via `ADDI` + optional `ADDIU`, then performs a register-register operation. The final `alur` call at line 384 was `alur(regCmd, src, dst)` — but this encodes `dst=src, src=dst`, which swaps the register fields in the instruction word. The original binary emits `orr R2, R1` (destination R2 holds the loaded constant, source R1 is the variable), while the recon emitted `orr R1, R2`.
- **Root cause**: Simple argument-order inversion in the `alur(regCmd, src, dst)` call. The correct order is `alur(regCmd, dst, src)` since `dst` is where the constant was loaded and the result should accumulate.
- **Resolution**: Changed `asm_commands.cpp:384` from `alur(regCmd, src, dst)` to `alur(regCmd, dst, src)`. All 13 `ziasm_register_arithmetic_11` tests now pass byte-identical. No regressions in full suite.

## IF-132  `waitAnaTrigger`: reg2 loaded with trigger address instead of wait-flag argument

- **Source**: `ziasm_triggers_3_uhfli/uhfawg/uhfqa` difftest failure (`.asm` and `.text` differ; wrong immediate in `addi R2`)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `CustomFunctions::waitAnaTrigger()` emits two `addi` instructions: reg1 gets the trigger address (from `readConst("AWG_ANA_TRIGGER1/2")`), reg2 gets the wait condition (args[1], 0 or 1). The recon incorrectly loaded `trigVal` (the trigger address) into reg2 instead of `args[1].value_.toInt()` (the wait flag). For `waitAnaTrigger(2, 0)`, this produced `addi R2, R0, 2` instead of `addi R2, R0, 0`. A stale comment ("Phase S.2 M5") incorrectly justified reusing `trigVal` for reg2.
- **Root cause**: Wrong value used for the second `addi` — `trigVal` instead of `args[1].value_.toInt()`.
- **Resolution**: Changed `custom_functions_io.cpp` in `waitAnaTrigger()` to compute `int waitFlag = args[1].value_.toInt()` and use it for the second `addi`. All 3 `ziasm_triggers_3` tests now pass byte-identical.

## IF-133  `waitDigTrigger` unsupported-device path: false-wait branch emits extra `addi` + wrong `wtrig` form

- **Source**: `ziasm_triggers_4_uhfli/uhfawg/uhfqa` difftest failure (`.asm` size 308 vs 284, extra instruction)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `CustomFunctions::waitDigTrigger()` unsupported-device path checks `args[1].toBool()` (the wait flag). When wait=true (`useSameReg`), it emits `wtrig(reg1, reg1)`. When wait=false, the original binary emits `wtrig(reg1, R0)` — using R0 (which is always 0 in this ISA) directly without allocating a new register. The recon instead allocated a fresh register, emitted an extra `addi R2, R0, trigVal`, then `wtrig(reg1, R2)` — adding one spurious instruction and encoding the wrong value in the second operand.
- **Root cause**: Incorrect assumption that the false-wait branch mirrors the true-wait structure with a new register. The binary reuses R0 as a constant-zero register.
- **Resolution**: Changed the `useSameReg=false` branch in `custom_functions_io.cpp::waitDigTrigger()` to `wtrig(reg1, AsmRegister(0))`. All 3 `ziasm_triggers_4` tests now pass byte-identical. No regressions in full suite (1207 passed, 3 equiv, 49 failed).

## IF-135  `WaveformGenerator::merge` reserveOnly path: `channels_` not set to input count

- **Source**: `ziasm_prefetching_0` difftest — all `.wf___playWave_*` sections half the expected byte size; `channels=1` in `.waveforms` for dual-channel assignWaveIndex waveforms
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `WaveformGenerator::merge()` (0x25f5c0) has two paths: one for normal (non-reserveOnly) signals and one for `reserveOnly_` signals (placeholder waves). In the reserveOnly path (0x25fa98), the function constructs and returns a `Signal(ReserveOnly, length, mergedBits)`. The `ReserveOnly` Signal constructor initializes `channels_=0`. In the non-reserveOnly path (0x25fc70), `result.channels_ = numChannels` is explicitly set. The reserveOnly path was missing this assignment.
- **Effect**: When `assignWaveIndex(1, 2, w, 1, 2, w, idx)` is called with a dual-channel placeholder wave, `mergeWaveforms` calls `merge()` on two entries of the same placeholder. The result had `channels_=0` instead of `channels_=2`. This caused: (a) `Signal::getRawData()` to compute `byteSize_ = 0 * length * 2 = 0` for NOBITS ELF sections (all `.wf_` sections half the expected size, since `channels_=1` after clamping by some path), (b) `genPlayConfig()` to see `channels_=0→1` and compute `channelMask=1` instead of `3`, setting wrong `play_config` in `.waveforms` metadata, (c) wrong waveform memory accounting in `.wavemem`.
- **Root cause**: Missing `result.channels_ = static_cast<uint16_t>(signals.size())` after the `Signal(ReserveOnly, ...)` construction in `merge()`.
- **Resolution**: Added `result.channels_ = static_cast<uint16_t>(signals.size())` before the return in the reserveOnly branch of `merge()`. Fixes 24 tests (+1190 → +1214 passing in 1259-test suite). `ziasm_prefetching_0` still has additional failures in the prefetch address calculation and waveform ordering (separate pre-existing bugs).
- **Location**: `reconstructed/src/waveform/waveform_generator.cpp:2627-2635`

## IF-134  `writeWavesToElfAbsolute`: wrong skip condition for placeholder waveforms (Cervino/UHF devices)

- **Source**: `ziasm_playconfig_cwfv_0_uhfli/uhfawg/uhfqa` difftest failures (`.wf_` sections missing in recon)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: In `writeWavesToElfAbsolute` (used for absolute-addressing devices: UHFLI, UHFAWG, UHFQA), the recon checked `wf->signal.data().empty()` (i.e., `samples_.empty()`) to skip waveforms with no data. However, the binary at `0x10e1aa` checks `signal.length_ == 0` (the uint64 at WaveformIR+0x80+0x50 = offset 0xD0 from WaveformIR base). `placeholder` waveforms have `reserveOnly_=true` so `samples_` is always empty, but `length_ > 0` (e.g., 1000). The incorrect empty-samples check caused all placeholder waveforms to be skipped in the absolute-addressing path, producing ELFs with missing `.wf_` sections.
- **Root cause**: `Signal::data()` returns `samples_` (physical samples), which is empty for reserveOnly signals. The binary instead checks the logical `length_` field, which is set even for placeholder (reserve-only) waves.
- **Resolution**: Changed condition to `wf->signal.length() == 0`. All 3 UHFLI/UHFAWG/UHFQA tests for `ziasm_playconfig_cwfv_0` now pass byte-identical.

## IF-136  `playAuxWave`/`playDIOWave`/`playWaveIndexed`: missing prefetch instructions and wrong waveform emit on Cervino (UHF) devices

- **Source**: `ziasm_playconfig_cwfv_2_uhfli/uhfawg/uhfqa`, `ziasm_playconfig_cwfv_3_uhfli/uhfawg/uhfqa`, `ziasm_various_playback_stuff_1_uhfli/uhfawg/uhfqa`, `ziasm_various_playback_stuff_2_uhfli/uhfawg/uhfqa` difftest failures (`.asm` size diff, `.wf_` section missing in recon)
- **Severity**: likely-bug
- **Status**: **stale/unverifiable** (2026-05-05 — test cases `cwfv_2`, `cwfv_3`, `vps_1/vps_2` not in current manifest; cannot confirm or dismiss; 1341/1341 passing)
- **Description**: After fixing IF-134 (the placeholder waveform data issue), several tests still fail:
  - `cwfv_2` (`playAuxWave`): The AuxWave generates a new waveform `__playWaveI_4_3` (4000 samples) which appears in original `.waveforms` but not recon. Original emits `prf`/`wprf`/`wvf` for it; recon emits only `cwvf` without the corresponding `wvf`. The AuxWave waveform is not reaching `collectUsedWaves` (likely because `playAuxWave` doesn't set `results->node_` to chain the node into the prefetch tree, OR the waveform is not getting `used=true`).
  - `cwfv_3` (`playDIOWave`): `.asm` size diff, `.channels` diff, `.waveforms` diff. `playDIOWave` also appears to not properly chain its node into results->node_ (line 634 of custom_functions_playback.cpp doesn't set `results->node_`). Additionally `.channels` byte differs (orig=0xe0 vs recon=0xc0) suggesting a channel mask calculation difference.
  - `vps_1/vps_2` (`playWaveIndexed`): Recon generates MORE instructions than original (recon=381 vs orig=350). Register assignment and instruction ordering differ — the original uses `addi R1, R0, 200; addi R3, R1, 0; ssl R3, R3; addr R3, R2; wvf R3, R0, 256; wwvf` while recon generates a different sequence with an extra `wwvf` and different register usage.
- **Root cause**: Not yet fully investigated. Likely multiple separate bugs in `playAuxWave`/`playDIOWave` node chaining and `playIndexed` register allocation.
- **Location**: `reconstructed/src/runtime/custom_functions_playback.cpp:352`, `634`, and `reconstructed/src/runtime/custom_functions_play.cpp` (playIndexed section)

## IF-137  `lock()`/`unlock()` missing `results->node_` assignment

- **Source**: `ziasm_misc_2` difftest failures (`.linenr` off-by-1 in `wavetableFrontIndex_`)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `lock()` and `unlock()` in `custom_functions_io.cpp` returned `EvalResults` without setting `results->node_` to the LockPlaceholder/UnlockPlaceholder node. Without this, the node chain only contained the Play node; `placeCommands` called `placeSingleCommand` once (wavetableFront=3). After all prefetch processing, `wavetableFrontIndex_` was left at 3 (not 4). Then `unsyncCervino()` (compiler Step 16) emitted `st R0, 68` / `st R0, 69` with stale index=3 vs 4. The `.linenr` section reflected this wrong value.
- **Root cause**: Missing `results->node_ = asmEntry.node` assignment in both `lock()` (binary: 0x14de27-0x14de47) and `unlock()` (0x14e33d-0x14e35c).
- **Resolution**: Added `results->node_ = asmEntry.node` in both functions. All `ziasm_misc_2` variants pass.
- **Location**: `reconstructed/src/runtime/custom_functions_io.cpp` (lock and unlock functions)

## IF-138  `getSampleClock()` wrong string literal for `DEVICE_SAMPLE_RATE` lookup

- **Source**: `ziasm_firmware_syscall_trap_1_hdawg8/hdawg4` difftest failures (missing `.required_sample_rate` ELF section)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `getSampleClock()` called `resources_->variableExists("$DEVICE_SAMPLE_RATE")` and `resources_->readConst("$DEVICE_SAMPLE_RATE", ...)` with a `$`-prefixed string literal. The binary's `constDeviceSampleRateE` global at 0x959dc0 is a libc++ SSO string where byte 0 = `$` (0x24) encodes length=18 and is-short. The actual string DATA starting at byte 1 is `"DEVICE_SAMPLE_RATE"` (18 chars, no `$`). So the binary's `variableExists` call matches variables stored as `"DEVICE_SAMPLE_RATE"`, not `"$DEVICE_SAMPLE_RATE"`. The recon's `"$DEVICE_SAMPLE_RATE"` literal would construct a 19-char C++ string with `$` as the first character, causing a miss and `usedSampleRate_` in `StaticResources::getVariable` never being set. Without the flag, `awg_compiler.cpp` does not emit the `.required_sample_rate` ELF section.
- **Root cause**: Misreading the binary's SSO string layout: `$` in the rodata is the SSO length byte, not the first character of the string.
- **Confirmed by**: GDB trace showing `StaticResources::getVariable` called with len=18, data=`"DEVICE_SAMPLE_RATE"` (no `$`).
- **Resolution**: Changed `"$DEVICE_SAMPLE_RATE"` to `"DEVICE_SAMPLE_RATE"` in `getSampleClock()`. All `ziasm_firmware_syscall_trap_1_hdawg8/hdawg4` tests pass.
- **Location**: `reconstructed/src/runtime/custom_functions.cpp:537-547`

## IF-139  `writeToNode` slow-commit hardcodes scale=1.0 instead of using `type` argument

- **Source**: `ziasm_firmware_syscall_trap_3` difftest failures (`.asm` and `.text` size diff — recon missing scale-dependent commit value)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: In `writeToNode()`, the common tail for BC cases 0,1,5 and the case 2 slow-arm both emit a "slow-commit" value via `addi(destReg, R0, float32_bits) + suser(kSuserNodeSlowCommit=0x14) + trap`. The recon hardcoded `Immediate(0x3F800)` = `float32(1.0) >> 12` in both `addiu` calls. The binary at 0x16643b-16648e instead loads `type.value_.toDouble()`, converts to `float32`, and calls `addi(float32_bits)` — which generates a hi/lo split (two instructions) when the lower 12 bits are nonzero. When `setDouble(path, val, scale)` is called with scale ≠ 1.0, the wrong hardcoded value was emitted.
- **Encoding**: The sequencer stores a 32-bit float as: `addi R2, R0, (float32_bits & 0xFFF)` (lower 12 bits) + `addiu R2, R2, (float32_bits >> 12)` (upper 20 bits). When lower 12 bits = 0 (e.g., float32(1.0) = 0x3F800000), only `addiu R2, R0, 0x3F800` is needed.
- **Resolution**: Replaced `addiu(Immediate(0x3F800))` with `addi(Immediate(float32(type.value_.toDouble()) bits))` in both the BC common tail and the case 2 slow-arm.
- **Location**: `reconstructed/src/runtime/custom_functions_play.cpp` (writeToNode BC common tail and case 2 slow-arm)

## IF-140  `writeToNode` case 2 fast-arm generates two triplets instead of one + slow-commit

- **Source**: `ziasm_firmware_syscall_trap_4` difftest failures (recon generates 1 extra instruction, no trap)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: In `writeToNode()` case 2 (typeIdx=2, I+Q sine write), the fast-arm (varType==2, register) incorrectly generated TWO triplets: tag=0xc (I-channel) + tag=0xd (Q-channel), with no commit or trap. The binary at 0x165587-165747 generates only ONE triplet (tag=0xc, addr, val.reg_) then `jmp 0x16636b` to the slow-commit region, which emits: floatEqual warning on type.value_, `addi(float32(scale))`, suser(kSuserNodeSlowCommit=0x14), trap.
- **Root cause**: The recon duplicated the Q-channel triplet (tag=0xd) into the fast-arm, imitating the slow-arm structure, instead of mirroring the binary's single-triplet + shared commit pattern. Additionally, the slow-commit floatEqual warning passes 3 args including `"integer"` hint (at 0x1663bc: `lea 0x79a579(%rip), %rcx` = `"integer"`), not 2 args.
- **Confirmed by**: GDB trace showing `type.value_` (vartype=4=Const, value_which=2=double) is what the slow-commit reads at 0x16636b.
- **Resolution**: Replaced the two-triplet fast-arm with: one triplet (tag=0xc) + floatEqual warning on `type.value_` (with `"integer"` hint) + `addi(float32(scale))` + suser(0x14) + trap.
- **Location**: `reconstructed/src/runtime/custom_functions_play.cpp` (writeToNode case 2 fast-arm)

## IF-140  `playDIOWave` missing `results->node_` assignment

- **Source**: `ziasm_playconfig_cwfv_3` difftest (UHFLI/UHFAWG/UHFQA)
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: `playDIOWave` in the recon did not set `results->node_` after calling `asmPlay` and pushing the asm entry. As a result, the DIOWave play node was absent from the `ast_` tree. `Prefetch::prepareTree` never visited it, `linkLoad` was never called, and `loadRef` was never set. When `placeSingleCommand` processed the play node, it found `loadRef=nil && dummy=false` and fell through to `play_finalize` with an empty `tempList`, emitting no wvf instruction.
- **Confirmed by**: GDB traces showing (1) `linkLoad` called 3 times in original (two dummy=0, one dummy=1), but only twice in recon (one dummy=0, one dummy=1); (2) `assignLoad` never called directly for DIOWave node; (3) `mov %rcx, 0x38(%rax)` at binary offset `+1840` writes `results->node_` from the play node shared_ptr; (4) `placeSingleCommand` dummy-check hit only once in original (for playZero), never for DIOWave.
- **Root cause**: The reconstruction previously read the binary's `playDIOWave` epilogue as not writing `results->node_` (offset `+0x38`). A broader disassembly search found the write at `+1840` — after an inlined push_back loop at `+1680`–`+1750`. Our earlier disassembly scan only looked in `+2014`–`+2251`.
- **Resolution**: Added `results->node_ = results->assemblers_.back().node;` immediately after the `push_back` in `playDIOWave` (`custom_functions_playback.cpp` line ~629). All 3 test variants now pass byte-identical. No regressions across all test subsets.

## IF-141  `WaveformGenerator::merge` checked only `signals[0].reserveOnly_` instead of all signals

- **Source**: `ziasm_playconfig_cwfv_2` difftest (UHFLI/UHFAWG/UHFQA) — `.wf___playWaveI_4_3` missing from ELF
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: The binary at `0x25fa80` accumulates an `allReserveOnly` flag by ANDing each signal's `reserveOnly_` bit in the load loop (`movzbl -0x50(%rbp),%eax; and 0xca(%rbx),%al`). The reconstruction only checked `signals[0].reserveOnly_`. When `playAuxWave(w, 10)` was called with a placeholder (`reserveOnly_=true`) and zeros (`reserveOnly_=false`), the recon incorrectly treated all signals as reserveOnly, returning an empty `Signal` instead of merging them. The resulting waveform had `length_=0` and was silently skipped by the ELF writer (`signal.length()==0` guard in `awg_compiler.cpp`).
- **Confirmed by**: Debug prints tracing `signal.length()` at the ELF write site; GDB disassembly of the AND-accumulation loop in `merge`.
- **Root cause**: Reconstruction mistakenly generalized the loop as checking only the first element.
- **Resolution**: Changed `merge` in `waveform_generator.cpp` to accumulate `allReserveOnly = allReserveOnly && sig.reserveOnly_` over all signals before the early-return check.

## IF-142  `WaveformGenerator::interleave` missing `length_` assignment after markerBits resize

- **Source**: `ziasm_playconfig_cwfv_2` difftest — waveform present but wrong size
- **Severity**: likely-bug
- **Status**: **fixed**
- **Description**: Binary at `0x25823d` executes `mov %rax, 0x50(%rbx)` which sets `result.length_ = result.samples_.size()` AFTER the markerBits resize block. An earlier reconstruction note placed this assignment at `0x258159` (which actually sets `channels_=1`), and it was subsequently removed. Without this assignment, `result.length_` remained 0 even though `samples_` was correctly populated, causing the waveform to be skipped by the ELF writer.
- **Confirmed by**: Debug prints showing `signal.length()==0` at ELF write site even when `samples_.size()==4000`; disassembly confirming `mov %rax, 0x50(%rbx)` at `0x25823d` (after `0x258220`–`0x25823c` markerBits block).
- **Root cause**: Misidentification of which instruction at `0x258159` sets `length_` vs `channels_`.
- **Resolution**: Restored `result.length_ = result.samples_.size()` at the correct location (after markerBits resize) in `interleave` in `waveform_generator.cpp`. All 3 `cwfv_2` tests now pass byte-identical.

## IF-143  `playWaveIndexed` (split_ devices): four bugs causing wrong prefetch and emit

- **Source**: `ziasm_various_playback_stuff_1/2` difftests — UHFLI/UHFAWG/UHFQA
- **Severity**: likely-bug (all fixed)
- **Status**: **fixed**
- **Description**: Four distinct reconstruction errors caused the indexed-play path for Hirzel split devices (UHFLI/UHFAWG/UHFQA) to emit wrong instructions:

  **Bug A** (`custom_functions_play.cpp:1092`): `Immediate rateImm(rate)` used `rate=-1` (the `parseOptionalRate` sentinel for "no rate arg") instead of the actual offset constant `parseEnd[0].value_.toInt()`. This caused `addi R1, R0, -1` instead of `addi R1, R0, 200`. GDB-confirmed: `addi` call args at `0x161e56` with correct immediate=200 in original.

  **Bug B** (`prefetch.cpp`, `moveLoadsToFront`): an extra `if (!loadNode->lengthReg.isValid() && cur->lengthReg.isValid()) loadNode->lengthReg = cur->lengthReg;` block propagated `lengthReg` from a matched Play node to the new Load node. GDB-confirmed: at `placeSingleCommand` step=4 for Load nodes on split devices, `lengthReg.valid=0` — the original binary does NOT propagate it here.

  **Bug C** (`prefetch_placesingle.cpp`, `play_cervino_indexed` split branch): after the ssl loop, the code was missing the common finalize block: `try_emplace(nodeStates_, node)` → get `registerCervino` → `addr(idxReg, registerCervino)` → `wvfImpl(idxReg, totalSize, is4Ch)`. GDB-confirmed path: `jge 0x1db6f8` → `je 0x1dbb75` → `addr` at `0x1dbbb9` → `jmp 0x1d9d3a` → `wvfImpl` at `0x1d9ef7`.

  **Bug D** (`prefetch.cpp`, `splitPath` in `allocate`): `if (playNode && playNode->length != 0)` indexed-path check was incorrectly placed inside `splitPath` (split_=true branch). For split_ devices, the original binary ALWAYS uses the full-wave `numSamplesForCache` formula regardless of `playNode->length`. Only the `!split_` (Cervino) path has the `playNode->length != 0` indexed check. This caused `numSamplesForCache=256` (128×2) instead of the correct `2000` (1000×2 bytes), resulting in `prf R1, R2, 256` instead of `prf R1, R2, 2016`.

- **Confirmed by**: GDB traces at `0x162343` (asmPlay call args), `0x1d1a62` (play node lock), `0x1d1cb7` (Cache::allocate args showing rcx=2000), `0x1d1c52` (full-wave formula inputs), `0x1dbbb9` (addr call), `0x1d9ef7` (wvfImpl call).
- **Resolution**: 
  - Bug A: `Immediate rateImm(parseEnd[0].value_.toInt())`
  - Bug B: removed the erroneous `lengthReg` propagation block in `moveLoadsToFront`
  - Bug C: added `nodeStates_.try_emplace` + `addr` + `wvfImpl` after ssl loop in split branch
  - Bug D: gated indexed-path check with `!split_`: `if (!split_ && playNode && playNode->length != 0)`
  - Result: 1264/1272 passing (same as before; 6 new tests now pass byte-identical)

## IF-144  `MemoryAllocator::allocateFirstSuitableFreeBlock`: wrong free-block ordering after split

- **Found during**: investigating `ziasm_prefetching_0` difftest failures (all 10 device variants)
- **Source**: `memory_allocator.cpp`, `wavetable_ir.cpp` (FIFO Phase 2 allocation path)
- **Severity**: likely-bug
- **Status**: **fixed**

- **Description**: When `allocateFirstSuitableFreeBlock` consumed a free block and split it into `before` and `after` remainders, the two pieces were re-inserted using `insert(begin(), before)` and `insert(end(), after)`. This broke the implicit address-ordering of `freeBlocks_`: after wlong (5120B, waveIndex=5) was allocated at 0x80002000 from the tail of the free region `[0x80000400, 0xFFFFFFFF)`, the deque became `[{0x80003400, 0xFFFFFFFF}, {0x80000400, 0x80002000}]` — tail-after-wlong first, gap-before-wlong second. Subsequent small waveforms (waveIndex=2,3,4) matched the tail gap first (because wlong's cacheLineUsage claimed that CL), placing them at 0x80003400+ instead of 0x80000600+.

  **Root cause confirmed by GDB**: The binary stores *allocated* blocks in the deque (not free blocks), so it naturally iterates gaps in ascending address order. The recon's free-block deque must also be maintained in address order for correctness.

  **Fix**: use index-based insertion so both remainders are inserted at the consumed block's position in address order: `after` first at `idx`, then `before` at `idx`, yielding `[..., before, after, ...]`.

  **Incorrect addresses (recon before fix)**:
  - waveIndex=2 → 0x80003400 (orig: 0x80000600)
  - waveIndex=3 → 0x80000600 (orig: 0x80000800)
  - waveIndex=4 → 0x80003600 (orig: 0x80000a00)

  **Correct addresses after fix**: all waveform `addressValue` fields match the original binary exactly.

- **Confirmed by**: GDB trace of `allocateFirstSuitableFreeBlock` deque page contents after each Phase 2 allocation; recon debug `fprintf` showing before/after block values; sub-batch test results (all 1272 tests pass with 0 failures).
- **Resolution**: `memory_allocator.cpp:274–290`: replaced `insert(begin(), before) + insert(end(), after)` with index-based `insert(begin()+idx, after) + insert(begin()+idx, before)`.

## IF-145  `assignWaveIndexImplicit`: Phase 1 lambda was a no-op stub

- **Found during**: same `ziasm_prefetching_0` investigation
- **Source**: `wavetable_ir.cpp`, `assignWaveIndexImplicit`
- **Severity**: likely-bug
- **Status**: **fixed** (earlier in this session)

---

## IF-143: playWaveIndexed emits extra ssl/addr/prf instructions (load_indexed_play misfiring)

- **Found during**: `ziasm_various_playback_stuff_1/2` investigation
- **Source**: `prefetch.cpp` (`createLoad`, `moveLoadsToFront`) and `prefetch_placesingle.cpp` (`load_indexed_play` trigger)
- **Severity**: likely-bug
- **Status**: **fixed**

### Description

`playWaveIndexed` on UHF devices (UHFLI/UHFAWG/UHFQA) generated extra instructions before the fix:

- An extra `addi R4, R1, 0` + `ssl R4, R4` + `addr R3, R4` + `wwvf` + `prf`/`wprf` block appeared before the correct variable-init + ssl/addr/wvf sequence.
- The `prf` size was also wrong (256 instead of 320 for a placeholder(160) wave).

### Root Cause (three interacting bugs)

**Bug A** (`prefetch.cpp`, `moveLoadsToFront`, formerly lines 842-853):
A previous reconstruction incorrectly copied `lengthReg` from the matched play node to the load node inside `moveLoadsToFront`. Binary-verified (deep-thinking model analysis): at `0x1cd313`, the binary calls `vector::insert` to merge play lists but does **not** access offset `+0x88` (lengthReg). This copy was removed.

**Bug B** (`prefetch_placesingle.cpp`, `load_indexed_play` trigger, step 4):
The `load_indexed_play` path fires when the load node's `lengthReg` is valid and non-zero. Since `createLoad` legitimately copies `lengthReg` (needed for the large-waveform `split_=false` path), the guard needs to check `split_`. Binary-verified: at `0x1d1a84` the binary checks `0xbc(%r14)` (`split_`) before reaching `0x1d1a9b` (`playNode->length`). Fixed by adding `if (!split_)` around step 4.

**Bug C** (`prefetch.cpp`, `allocate`, cache size computation, formerly line 1649):
The indexed allocation path (`playNode->length * channels * 2`) was entered unconditionally when `playNode->length != 0`. For small waveforms where `split_=true`, the binary skips this path entirely (the `split_=true` jmp at `0x1d1a84` bypasses `0x1d1a9b`). Fixed by adding `!split_` to the condition.

### Effect of fix

- `ziasm_various_playback_stuff_1/2` (UHFLI/UHFAWG/UHFQA): 6 tests fixed (FAIL → PASS).
- `uhf_doc_tv_mode` (large waveform, `split_=false`): unaffected, still passes.
- No regressions.

- **Description**: The Phase 1 lambda in `assignWaveIndexImplicit` was reconstructed as a no-op that returned immediately without assigning auto-indices. The binary assigns indices to all waveforms with `waveIndex == -1` by calling `waveIndexTracker_.assignAuto()` in a lower-bound loop over `waveIndexTracker_.indices_`. This caused all waveforms with explicit `assignWaveIndex(...)` calls to keep their correct indices, but the placeholder waveform (which has no explicit index) was left with `waveIndex = -1` instead of the next available index (7 for this test), which then cascaded into wrong `fixed_` / `addressValue` for the Phase 1 FIFO allocation.
- **Resolution**: implemented the lower-bound loop over `indices_` in `assignWaveIndexImplicit` Phase 1 lambda.

---

## IF-146  Resources::Variable layout: subTypeRaw at +0x04, value.type_ at +0x08

**Source**: Phase 20e-ii correction sweep
**Status**: confirmed (fixed)
**Severity**: cosmetic (was a bug, now correct in source)

Earlier reconstruction had `pad_04` at +0x04 and `subType` at +0x08. The
disassembly shows the binary reads from +0x04 in `getVariableSubType`
(@0x1e4580), and `addVariable` / `addConst` callers write the caller-supplied
`st` arg to +0x04 with a hardcoded secondary-tag literal at +0x08. Correct
layout: `subTypeRaw` at +0x04, `value.type_` at +0x08. See `resources.hpp`
`Variable` struct and `resources.cpp` `addConst` (@0x1e7150).

---

## IF-147  Resources::Function ctor: four args, not three

**Source**: Phase 20e-ii correction sweep
**Status**: confirmed (fixed)
**Severity**: cosmetic (was a bug, now correct in source)

The fourth argument is a `weak_ptr<Resources>` for the parent scope. The
function's own child scope is constructed inside the ctor via
`std::allocate_shared` and stored at +0x60. Binary ctor @0x1eaa00.

---

## IF-148  VarType enum mapping corrected (Phase 19c-followup Finding 1)

**Source**: Phase 19c-followup
**Status**: confirmed (fixed)
**Severity**: cosmetic (was wrong, now correct)

Previous mapping had Const=3 / Cvar=4 / String=5 / Wave=6. Verified mapping
from binary jump table at 0x95c2a0 (function @0x247dd0):
  0→"notype", 1→"void", 2→"var", 3→"string", 4→"const", 5→"wave", 6→"cvar".

---

## IF-149  CustomFunctions field layout: earlier pass used wrong destructor

**Source**: Phase reconstruction correction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was a bug, now correct in source)

An earlier pass mis-attributed offsets +0xF8/+0x100/+0x128/+0x150/+0x168 to a
destructor at 0x1306c1, which belongs to pybind11 internals, NOT
CustomFunctions. All field offsets re-verified against real dtor @0x127c90 and
methods: lookupNode @0x15c530, addNodeAccess @0x15c6c0, getNodeAddress @0x16ba10.

---

## IF-150  CustomFunctions::nodeMap_: pointer not inline map

**Source**: lookupNode reconstruction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was a bug, now correct in source)

Was previously declared as `std::map<std::string, NodeMapItem>` (24 bytes
inline). Real layout: single 8-byte pointer at +0xD0, dereferenced and passed
to `NodeMap::retrieve(...)` — from `lookupNode` @0x15c530 line 0x15c54e.

---

## IF-151  AWGAssemblerImpl: spurious fields from earlier reconstruction

**Source**: Phase reconstruction correction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was noise, now removed)

Previously-listed `field_70_` (void*), `pad0_`, `opcodes_begin_/_end_` and
`sourceLines_begin_/_end_` had no corresponding storage in the binary.
`opcodes_` and `sourceLines_` are std::vectors accessed at offsets 0x50 and
0x78 via normal vector interface. The region 0x00..0xF0 previously misidentified
as `remaining_fields_[0x80]` is actually the embedded AsmParserContext at
offset 0xf0.

---

## IF-152  Prefetch: pageSize_ was hallucinated

**Source**: Prefetch reconstruction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was noise, now removed)

`pageSize_` appeared only in the constructor init list, was never read
elsewhere. The +0xBC slot is the bool `split_` initialized separately.

---

## IF-153  WaveformFront: args_ was hallucinated

**Source**: WaveformFront reconstruction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was noise, now removed)

`args_` had no usage anywhere in the binary. Other previously-misidentified
fields were actually aliases for Waveform members already present:
sampleLength→Waveform::signal.length_ (+0xD0), fileType→waveformType (+0x18),
isModified→dirty_ (+0xDC), funDescrName_→Waveform::funDescrName (+0x50).

---

## IF-154  Cache::Pointer: pageCount was hallucinated

**Source**: Cache reconstruction
**Status**: confirmed (fixed)
**Severity**: cosmetic (was noise, now removed)

No `Cache::Pointer::pageCount` symbol exists in binary and no consumer in
reconstructed source ever accessed such a field. Pointer is exactly 0x24 bytes.

---

## IF-155  Empty input not rejected by recon (error 43 unreachable)

**Source**: zivibes intake — `sp_01_empty.seqc` on HDAWG8 / SHFQA4 / SHFSG8 / UHFQA
**Status**: **fixed** — all 4 sp_01_empty cases pass; full suite 1600/1600
**Severity**: likely-bug

### Final fix

Added the missing `0x11f283 → 0x11f557` early-return branch in
`Compiler::compile`: when `parse()` returns a null `Expression*`, allocate
the same empty `WavetableIR` the binary does and return immediately
with an empty `vector<Assembler>` asmList.  Downstream
`AWGCompilerImpl::writeToStream` (already guarded in IF-155 partial fix
at `awg_compiler.cpp:751-759`) then sees the empty opcode vector and
throws `EmptyInput` (error id 43) — matching the binary.

`reconstructed/src/codegen/compiler.cpp` ~line 251:
```cpp
auto expr = parse(normalized);
if (!expr) {                                                // 0x11f28a..0x11f28d
    auto wavetableIR = std::allocate_shared<WavetableIR>(
        std::allocator<WavetableIR>(), *wavetable_, *deviceConstants_,
        detail::AddressImpl<uint32_t>(config_->addressImpl),
        static_cast<size_t>(config_->wavetableSize),
        config_->searchPath, cancelCallback_);
    return CompileResult{std::vector<Assembler>{}, std::move(wavetableIR)};
}
```

### Original analysis (kept for reference)

Original binary, given a SeqC source consisting only of comments/whitespace
(no statements, no waveform definitions), errors with:
  `"Compilation failed: nothing to write, empty input"`
This corresponds to error-message id 43 in `error_messages.cpp:178`
(`m[43] = "nothing to write, empty input"`).

Recon's `Compiler::compile` runs the full pipeline (label, placeholder,
trailer emit) for empty input and ends up with `assemblerImpl_->getOpcode()`
returning **5 instructions** (`cwvf 5242816`, `st R0, 146`, `wwvf`, `nop`,
`end`).  `writeToStream` then proceeds without raising any error.

### GDB-confirmed binary flow

1. **`Compiler::compile @0x11f150`** is called.
2. After `parse()` returns at `0x11f283`: binary checks `test %r14, %r14`
   and at `0x11f28d` does `je 0x11f557` — **if expr is NULL, jumps to a
   short alternate path that bypasses steps 10–11** (label, placeholder,
   trailer).  This path still allocates a `WavetableIR` etc. but never
   appends any opcodes to `assemblerImpl_`.
3. Control returns to `AWGCompilerImpl::writeToStream @0x108cc0`.
4. `assembler_.getOpcode()` returns an empty vector (`begin == end`).
5. `@0x108d0b cmp begin/end`; `@0x108d0f je 0x109a00`.
6. `@0x109a00..0x109a14`: `__cxa_allocate_exception(0x60)` →
   `mov $0x2b, %esi` → throws `ZIAWGCompilerException` with
   `ErrorMessages::format(EmptyInput)` (id 43).

GDB observation for `sp_01_empty.seqc`:
- `assembler_.getOpcode()` returned `begin=0x0 end=0x0 size=0` (binary)
- recon emits 5 opcodes for the same input

### What was fixed in this pass

`writeToStream` now correctly raises `EmptyInput` when opcodes are empty
(matching `0x108d0f → 0x109a00 → 0x109a14`), instead of silently
`return`-ing.  See `awg_compiler.cpp:751-759` after this commit.  This
guard is dormant for `sp_01_empty.seqc` because recon's pipeline never
produces an empty opcode vector.

### What remains unfixed (still failing)

The root cause: recon's `Compiler::compile` does not take the
"parse returned null → skip trailer emission" branch at the equivalent
of `0x11f283/0x11f557`.  Recon's `parse()` (at `compiler.cpp:151`) wraps
whatever the bison-generated `seqc_parse` produces in a `shared_ptr`;
for empty source, the parser may produce a non-null empty-block
Expression rather than the null pointer the binary expects.

**To finish the fix**:
1. GDB-trace the binary's parse() on empty source to see whether it
   returns a true `nullptr` Expression* or whether the null-check at
   `0x11f28a` is on something else (e.g., the unwrapped raw pointer
   from `seqc__scan_string` rather than the parser result).
2. GDB-trace what the alternate path at `0x11f557..0x11f5b6` actually
   does (the `WavetableIR` allocation) and find where it returns from
   `compile()` without reaching the trailer-emit block.
3. Mirror that branch in recon's `Compiler::compile` so empty input
   produces an empty assemblerImpl_ — the existing `EmptyInput` guard
   in `writeToStream` will then fire and produce the correct error.

Promoted to TODO 47.1 (closed — fixed via early-return branch above).


---

## IF-156  Recon register allocator stricter than binary on ~17-variable programs

**Source**: zivibes intake — `hb_b_reg_count.seqc` (17 `var` declarations)
**Status**: **fixed** — `hb_b_reg_count` now byte-identical; full suite 1596/1600
**Severity**: likely-bug

### Root cause (final)

Two bugs compounded:

1. **splitReg body** was a stub.  Reconstructed per the binary disassembly
   at 0x281000..0x2814cc as a per-iteration Block 1 / Block 2 boundary
   writer with threshold ≥10, fresh-reg allocation from
   `GlobalResources::regNumber`, current-instruction renaming of regSrc +
   regAux (NOT regDst), and an epilogue that invalidates start.cmd
   (and end.cmd if `end != list.end()`) only when `allSplitOk && didSplit`.
   GDB-verified per-call counts: 3, 3, 3, … (16 productive calls), then
   0, 0, … (48 trailing calls on planted ADDIs); total 64 calls.

2. **splitConstRegisters Pass 1** was incrementing the wrong TLS counter.
   The barrier-creation loop at 0x2804f4 reads `(%r14)` where r14 was set
   at 0x2804b2 via `__tls_get_addr` for the TLS module symbol at offset
   `b7acf8` plus `+0x40`.  TLS+0x40 is `AsmList::Asm::createUniqueID`'s
   `nextID` counter — **not** `GlobalResources::regNumber` (which lives at
   TLS+0x48, accessed via the separate symbol at `b7ad10`).  The recon
   line `int newSeqId = GlobalResources::regNumber++;` over-incremented
   regNumber by ~120 per compile, so when `splitReg` later allocated
   fresh regs from regNumber it produced values around 169..216 — far
   outside the live-range table (sized `numSlots = numRegs+1 = 111`)
   that `registerAllocation` builds.  This caused the allocator to skip
   those regs entirely (silent OOB-guarded `addToLiveRange`), leaving
   them un-renamed.  The text re-emit in `compileString` then re-parsed
   regs ≥ 16 and `getReg` rejected them as "register out of range" —
   surfacing as `Assembler message at N : ... %1% ... %4% argument(s)
   given` because of an unrelated msg-id mismatch in the recon's
   error_messages table.

   Fix at `reconstructed/src/asm/asm_optimize_regalloc.cpp:440`:
   ```cpp
   int newSeqId = AsmList::Asm::createUniqueID(false);  // TLS+0x40
   ```
   instead of `GlobalResources::regNumber++` (TLS+0x48).

### Verified

- `tests/diff_test.py --filter hb_b_reg_count -v` → byte-identical (7580 bytes).
- Full suite via `tests/diff_test_fast.py`: **1596/1600** (was 1595/1600, no regressions).
- GDB on original confirmed `regNumber=244` at first splitReg call (after
  Pass 1's 122 createUniqueID++), then `247, 250, 253, …` per call —
  the binary really does use regNumber for fresh splits.  My recon now
  matches that trajectory because Pass 1 no longer touches regNumber.

### Historical notes (kept for reference)

- IF-156's earlier per-call breakdown (16 productive × 3 splits + 48
  zero-split calls = 64 total) is correct and matches the post-fix recon.
- The `[rbp-0x48]` field in splitConstRegisters is reused: in Pass 1 it
  holds `asm_.end()` (set at 0x2804a5), in Pass 2 it is reset to 0 at
  0x280737 and then `incq`-ed at 0x280877 once per outer iter to count
  splitReg calls.  Return value is `splitCount + numRegs` (= 64 + 46 =
  110 for `hb_b_reg_count`).

### Recon-side state (post commit `<tbd>`)

- `splitConstRegisters` is now correctly wired to call `splitReg` (was
  comment-only `++splitCount` before).
- Pass-1 barrier construction now writes `magicReg` to `regSrc` (not
  `regDst`) — required for the post-pass strip filter to match the binary.
- Pass-1 emits **two** barriers per non-skip instruction (matching binary
  at 0x2805e0..0x2806d4).
- Pass-2 forward-scan logic now matches the binary's branching at
  0x2807cc..0x28083a, including the `regSrc==destReg` requirement for
  outer cmd `ADDI` and the `regSrc==0` requirement for outer cmd
  `INVALID/cmd4`.
- Pre-call overwrite check now scans `[it+1, list.end())` skipping
  `splitEnd` (matches 0x28088b..0x2808d1), instead of stopping at
  `scanEnd`.
- `tmpList` is now an `AsmList` (was `std::vector<Asm>`) so the
  splitReg call type-checks; layouts are identical.

### Open: splitReg body still produces too few splits

With the catch path wired up, recon's `splitReg` produces only 1 split
per call where the binary produces 3. Symptoms:
- `b_reg_count` still throws "run out of free registers"
- No regressions in the suite (1595/1600 holds); other tests don't exercise
  the multi-split case

The recon `splitReg` body at `asm_optimize_regalloc.cpp:673-783`
currently:
- Allocates `newReg` only on the first split (`if (!didSplit)` guard)
- Always overwrites the same fixed `startOff`/`endOff` slots, so
  multiple splits within one call would overwrite each other's
  boundary writes anyway.

The binary at `0x2811af mov 0x88(r12),%r14d` clobbers what we thought
was `instrCount`, then immediately at `0x2811b7..0x2811cf` does
nontrivial work involving a stack local at `[rbp-0x50]` (a counter
that gets `++(*ptr)` at `0x2811bd`) and a stack region at `[rbp-0x1e8]`
which is later passed to something via `r13`. This region is not yet
reverse-engineered; it likely allocates a new boundary slot for each
split rather than reusing fixed `startOff`/`endOff`.

**Reproduces**: `python tests/diff_test.py --filter b_reg_count -v`

Remains in TODO.

---

## IF-157  playWave_variants: waveform size halved + multiple section diffs

**Source**: zivibes intake — `ht_h_func_030_playWave_variants.seqc` on HDAWG8
**Status**: **fixed** in commit `22d812a`
**Severity**: likely-bug

Differences vs original:
- `e_entry`: orig=0x800002c0 recon=0x80000240 (32 fewer instructions)
- `.text`: first diff at offset 0x14 (instruction 5)
- `.asm`: text diff
- `.waveforms`: JSON diff
- `.wavemem`: numeric diff
- **`.wf___playWave_15_8`: size 256 vs 128** — recon emitted a waveform
  exactly half the size of the original

### Root cause

`WaveformGenerator::merge` (`waveform_generator_dsp.cpp:1983`) computed
`frameCount` from `signals[0].samples_.size()` only — taking the first
channel's length. For `playWave(w1, w2)` with `w1=ones(32)` and
`w2=zeros(64)`, this produced a 32-sample merged waveform instead of 64.

GDB-confirmed at binary offset `0x25fb3c`: `rsi=64` (the maximum) is
passed to the `Signal` constructor. Binary takes the **max** length
across all input signals, not the first one's.

### Fix

Iterate all signals and take the max length:

```cpp
size_t frameCount = 0;
for (const auto& sig : signals)
    frameCount = std::max(frameCount, sig.samples_.size());
```

**Resolved test**: `ht_h_func_030_playWave_variants` now byte-identical.

---


## IF-158  Missing static `cwvf` in else-arm of in-loop if/else after cwvfr-arm

**Source**: stress test `kitchen_sink_hdawg`, isolated to `if158_cwvf_in_loop.seqc`
**Status**: open
**Severity**: likely-bug

### Symptom

A for-loop containing an if/else where:
- the if-arm plays a dual placeholder (`playWave(phA, phB)`) → emits
  `cwvfr Rn` (register-form cwvf, runtime config), and
- the else-arm plays a single-channel placeholder (`playWave(1, phC)`)
  → should emit a static `cwvf <imm>` to reset the config,

drops the static `cwvf` instruction in the recon's else-arm. Resulting
diff is exactly one missing `.text` instruction (4 bytes), one missing
`.asm` line, and one missing `.linenr` entry (8 bytes).

The same if/else pattern **outside** any loop emits correctly in both
arms. The trigger is purely the surrounding for-loop.

### Minimal repro

`tests/cases/stress/if158_cwvf_in_loop.seqc`:

```seqc
wave g1  = gauss(128, 64, 16);
wave phA = placeholder(512, true, true);
wave phB = placeholder(512, true, true);
wave phC = placeholder(2048, false, false);

assignWaveIndex(phA, phB, 50);
assignWaveIndex(1, phC, 51);

var u0 = getUserReg(0);

playWave(1, g1);            // <-- removing this makes the bug disappear

for (var k = 0; k < 4; k = k + 1) {
  if (u0 + k > 5) {
    playWave(phA, phB);     // dual placeholder → cwvfr in if-arm
  } else {
    playWave(1, phC);       // single placeholder → static cwvf in else-arm
  }                          //                     (DROPPED in recon)
  waitWave();
}
```

Run: `python tests/diff_test.py --manifest manifest-stress.json --filter if158 -v`

### Suspected location

Per subagent investigation `ses_1fde0697fffeWuLAE3xLqp38Lv`:

| Function | File:line | Binary addr |
|---|---|---|
| `Prefetch::needsNewCwvf` | `reconstructed/src/codegen/prefetch_emit.cpp:272` | `0x1dc620` |
| `Prefetch::placeSingleCommand` (Play case) | `reconstructed/src/codegen/prefetch_placesingle.cpp:445` | `0x1d7d49`–`0x1d7e21` |
| `Prefetch::optimizeCwvf` (Branch / Loop cases) | `reconstructed/src/codegen/prefetch.cpp:292`, `:561` | `0x1cfd74`, `0x1d046f` |

Hypothesis (unconfirmed, requires GDB):

`needsNewCwvf` walks up the parent chain comparing `Node::currentCwvf`
(+0x68). For an in-loop Play, the walk passes through the if/else
Branch (skipped) and then into the for-Loop's Loop node case, which has
~250 lines of inlined PlayConfig comparisons with `seenDifference`,
`loopBodyRunsAtLeastOnce`, `prev->loop == curNode`, and
`prev->branchMaySkipAllBodies` paths.  Likely candidates:

1. `prefetch_emit.cpp:354–371` — Loop case `prev->loop.get() == curNode.get()`
   test and its surrounding `seenDifference` / `loopBodyRunsAtLeastOnce`
   logic (wrong polarity or wrong field offset).
2. `prefetch_emit.cpp:414` (`checkSeenDifference` label) — wrong
   polarity of `seenDifference` causing fall-through to `treeWalk`
   instead of comparing the running config.
3. `prefetch.cpp:564` Loop case — `curNode->currentCwvf = cwvf` early
   write *before* loop-body recursion may carry stale pre-body config
   instead of the post-body invalid sentinel set by Branch divergence
   at `prefetch.cpp:457–466`.

### Recommended next step

Per AGENTS.md "GDB tracing for binary analysis": set breakpoints at
`needsNewCwvf` entry/exit and the placesingle dispatch sites, run on
the minimal repro, and observe which branch the binary takes for the
elsePlay — vs which branch recon takes.  GDB recipe in subagent report
(see commit message of stress-suite commit for task ID).

### Related tests

- `kitchen_sink_hdawg` (full kitchen sink) — fails with same signature
- `if158_cwvf_in_loop_hdawg` (minimal repro) — fails with same signature

