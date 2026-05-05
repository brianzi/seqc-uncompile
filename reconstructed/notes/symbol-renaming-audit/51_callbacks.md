# Batch 51 — callbacks

## 1. Files considered

- `reconstructed/include/zhinst/core/callbacks.hpp`
- `reconstructed/src/core/callbacks.cpp`

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `ProgressCallback::setProgress::progress` | no | medium | matches method name and unit | keep current (medium) | — |

## 3. Detailed findings

### ProgressCallback::setProgress::progress  [no / medium / —]

Evidence:
- include/zhinst/core/callbacks.hpp:48 `virtual void setProgress(double progress);`
- src/core/callbacks.cpp:17 `void ProgressCallback::setProgress(double /*progress*/) {`
- src/codegen/awg_compiler.cpp:515 `progress->setProgress(1.0);` (call site, where `progress` here is the
  shared_ptr lock of `progressCallback_`, not the parameter — but the magnitude
  passed, `1.0`, is the only call site found and is consistent with a
  fractional progress value in [0, 1]).
- nm: `zhinst::ProgressCallback::setProgress(double)` is in the binary
  symbol table (method name authoritative per §3); the enclosing class
  is `ProgressCallback`. Parameter names are not encoded.

Interpretation:
- The method name `setProgress` is fixed by the binary symbol table.
- The body is empty in the binary (no-op default), so there is no
  internal usage that could contradict the parameter name.
- The single observed call site passes `1.0`, consistent with the
  conventional "fraction of work completed" interpretation that the
  identifier `progress` invites.

Judgement:
- Not a misnomer: the parameter name simply restates the method's
  subject, and no observed use contradicts that meaning.

Proposals:
- keep current (medium)

Locations consulted:
- declared: include/zhinst/core/callbacks.hpp:48
- defined:  src/core/callbacks.cpp:17
- used:     src/codegen/awg_compiler.cpp:515 (only non-commented call site found)

## 4. Symbols inspected and judged routinely fine

- (none — `setProgress::progress` is the sole in-scope symbol and is
  treated in §3 because it is the only finding in the batch.)

## 5. Coverage

- **Fully covered:** parameter `progress` of
  `ProgressCallback::setProgress`. This is the only in-scope symbol
  declared in the two files of this batch.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type names `zhinst::CancelCallback` and `zhinst::ProgressCallback`
    — both appear in the binary symbol table (e.g. `typeinfo for
    zhinst::ProgressCallback`, `vtable for zhinst::ProgressCallback`,
    and as parameter types in mangled signatures such as
    `zhinst::Compiler::setCancelCallback(std::__1::weak_ptr<zhinst::CancelCallback>)`).
    Excluded per §3 tier-1.
  - Method names `CancelCallback::isCancelled`, `ProgressCallback::~ProgressCallback`,
    `ProgressCallback::setProgress` — `setProgress` and the two
    `~ProgressCallback` dtor entries are in the symbol table; excluded
    per §3. `isCancelled` has no binary symbol (the class has no
    binary vtable — implementations live in the Python binding layer
    per the file header), so it is not authoritatively excluded by
    `nm`; however, this batch elected to treat it as routine: the
    name describes a boolean cancellation query, every call site
    (`wavetable_ir.cpp:273,830`, `prefetch_emit.cpp:196`,
    `asm_optimize.cpp:514,999,976` context) uses it as
    `if (...->isCancelled()) { abort/cleanup }`, and there is no
    contradicting evidence. No flag.
  - The two `cancelCb` / `cancelCallback_` /
    `progressCallback_` consumer-side symbols (in `Compiler`,
    `AWGCompilerImpl`, `Prefetch`, `WavetableIR`, `AsmOptimize`) are
    declared in those classes' files and belong to their respective
    batches (e.g. `09_prefetch.md` already records
    `Prefetch::cancelCb_` as not-misnomer).
