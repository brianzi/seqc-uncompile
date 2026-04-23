# Phase 16a Audit — Marker & Stub Sweep

Generated 2026-04-23. Transient catalog file; will be folded into a topic
file (proposed: `outstanding_work.md`) at sub-phase wrap-up.

Audit covers `reconstructed/src/*.cpp` and
`reconstructed/include/zhinst/*.hpp` only.

## Methodology

1. `grep -rnE "\b(TODO|FIXME|TBD|APPROXIMATE|VERIFY|XXX|HACK)\b"` → 112 hits.
2. `grep -rnE "throw .*(not implemented|unimplemented)"` → 32 hits (all
   in `waveform_generator.cpp`).
3. `grep -rnE "(deferred|not (yet )?(implemented|reconstructed)|placeholder)"`
   minus marker-already-flagged → 118 hits, mostly legitimate
   "placeholder" semantic noun usage; 8-10 are real stub indicators.
4. Trivial-return body scan (single-statement functions whose only
   non-comment body is `return {};`/`return nullptr;`/`return 0;` etc.)
   → 10 candidates across 9 files, all already documented stubs.
5. Per-file namespace/class inventory: `grep -E "^[\w\s\*&<>:,]*\b\w+::\w+\("`
   to find files defining methods of multiple unrelated types.

---

## Section A — Marker counts by file (top offenders)

| File | Markers | Notes |
|------|---------|-------|
| `src/custom_functions.cpp` | 56 | **CRITICAL.** See Section C. |
| `src/waveform_generator.cpp` | 21 + 32 throw-stubs = 53 | **CRITICAL.** See Section D. |
| `include/zhinst/seqc_ast_node.hpp` | 5 | print/clone macro stubs + evaluate() signature TBD |
| `include/zhinst/custom_functions.hpp` | 5 | mirrors src — see Section C |
| `src/cached_parser.cpp` | 3 | All boost-archive related; documented as "real lib not linked" |
| `include/zhinst/awg_device_props.hpp` | 3 | Phase 14b-iv field-name verification |
| `src/seqc_ast_node.cpp` | 2 | print + clone full-recursion deferred |
| `src/compiler.cpp` | 2 | evaluate() virtual not yet wired |
| `include/zhinst/elf_reader.hpp` | 2 | trailing pad_ dword purpose |
| 13 other files | 1 each | mostly per-phase verification notes |

Total source files with markers: **22**.

---

## Section B — Throw-stubs (silent failure mode)

`src/waveform_generator.cpp` is the only file using
`throw std::runtime_error("X not implemented")` as a stub pattern.
**32 distinct functions** with this pattern. Crucially, only some carry
TODO comments — others would silently appear "implemented" to a marker
grep that didn't also scan for throw text.

DSP function bodies (lines 666-769):
- Trig: `sin`, `cos`, `sinc`, `ramp`, `sawtooth`, `triangle`, `chirp`
- Pulses: `drag`, `blackman`, `hamming`, `hann`, `mask`, `marker`
- Random: `rand`, `randomGauss`, `randomUniform`, `lfsrGaloisMarker`
- Filters: `rrc`, `filter`
- Composition: `vect`, `placeholder`, `join`, `add`, `interleave`,
  `multiply`, `cut`, `flip`, `circshift`, `merge`, `grow`
- Other: `readWave` (479), `markerImpl` (489), `interpolateLinear` (499),
  `eval` (385) — Eval-blocked

TODO.md row "WaveformGenerator waveform function bodies | ~35" is in the
right ballpark but should explicitly say **"34 throw-stubs in
waveform_generator.cpp"** to make audits trivial.

---

## Section C — `custom_functions.cpp` (CRITICAL — multiple issues)

### C1. Pile-up of unrelated types (10 distinct classes in one .cpp)

Per `grep` of method definitions, the file currently holds methods for:

| Class | Lines | Should live in |
|-------|-------|----------------|
| `EvalResultValue` | 52-148 | `eval_result_value.cpp` (paired with `frontend_lowering` data model — see Phase 15a-i `frontend_lowering.md`) |
| `NodeMapData` | 77 | `node_map_data.cpp` |
| `VirtAddrNodeMapData` | 79-104 | (same) |
| `DirectAddrNodeMapData` | 105 | (same) |
| `NodeMapItem` | (1 line) | (same) |
| `MathCompiler` | 155-310 | `math_compiler.cpp` (TODO.md Phase 14a estimates 67 symbols not yet reconstructed — this is the seed) |
| `MathCompilerException` | 320-326 | (same) |
| `CustomFunctionsException` | 333-341 | stays in `custom_functions.cpp` |
| `CustomFunctionsValueException` | 348-356 | stays |
| `CustomFunctions` | 367-949 | stays |

The matching header `include/zhinst/custom_functions.hpp` has the same
pile-up: 7 unrelated class definitions besides `CustomFunctions` itself.

**Not currently tracked anywhere in TODO.md or unknowns.md.**

### C2. Stub density inside CustomFunctions

Beyond the pile-up, the `CustomFunctions` class itself has substantial
unfinished work:

- Lines 604-693: 14 utility methods all body = `// TODO: stub` + trivial
  return. Several are non-trivial dispatch hubs:
  - `play` @0x15f090
  - `playIndexed` @0x160e00
  - `generateWaveform` @0x15a9f0
  - `mergeWaveforms` @0x15e060
  - `writeToNode` @0x164550
  - `addSyncCommand` @0x16bb30
  - `printF` @0x16c470
  - `addWaitCycles` @0x16d8c0
  - `writeLS64bit` @0x16dbb0

  **These are NOT one-liners** — they are core dispatch functions with
  large binary bodies. TODO.md row "CustomFunctions built-in function
  bodies | 27 | 27 one-liner stubs remain" undercounts and miscategorises
  them.

- Lines 738-777: 6 thin "play wrapper" functions (`playAuxWave`,
  `playAuxWaveIndexed`, `playDIOWave*`, etc.) all delegate to `play()`
  with `SubFunc::Default` tagged "// SubFunc value TBD". Since `play()`
  itself is a stub, the wrappers are effectively double-stubbed.

- Line 422: `CustomFunctions::CustomFunctions` ctor body has
  `// TODO: actual bind registration omitted — 19KB of repetitive SSO
  string` — the binding of 87 built-in functions to `funcMap_` is
  documented as omitted. **This means the funcMap is empty at runtime,
  which would break dispatch**. Critical correctness gap.

### C3. Recommended split

Proposed Phase 16b output for `custom_functions.cpp`:

```
src/custom_functions.cpp        (CustomFunctions + its 2 exception classes only, ~620 lines)
src/math_compiler.cpp           (MathCompiler + MathCompilerException, ~180 lines)
src/node_map_data.cpp           (NodeMapData + Virt/DirectAddr subclasses + NodeMapItem)
src/eval_result_value.cpp       (EvalResultValue methods — could also live with frontend_lowering)
include/zhinst/math_compiler.hpp
include/zhinst/node_map_data.hpp
include/zhinst/eval_result_value.hpp  (or fold into frontend_lowering.hpp)
```

Headers split similarly. The `custom_functions.hpp` header would
forward-declare or `#include` the new headers.

---

## Section D — `waveform_generator.cpp` (HIGH priority)

- 4 distinct classes mixed: `WaveformGenerator`, `WaveformGeneratorException`,
  `WaveformGeneratorValueException`, `ErrorMessages` (the last is
  duplicated across many files — see Section F).
- 32 throw-stubs (Section B).
- 21 marker comments documenting expected signatures.
- Already partially tracked: TODO.md "WaveformGenerator waveform function
  bodies | ~35".

Recommended:
- Split exceptions to `waveform_generator_exceptions.cpp` (low priority).
- The throw-stubs ARE the work; bringing the count down 1-by-1 is
  Phase 16d+ material.

---

## Section E — Other multi-class files

Inventory grouped by legitimacy:

### Legitimate groupings (do NOT split)

| File | Classes | Reason |
|------|---------|--------|
| `device_factories.cpp` | 14 (all *Factory) | All implement DeviceFamilyFactory virtual; tightly coupled. |
| `device_shf.cpp` | 8 (Shf family) | One device family. |
| `device_uhf.cpp` | 5 (Uhf family) | Same. |
| `seqc_ast_node.cpp` | 9 (SeqC AST hierarchy) | Same class hierarchy. |
| `device_type.cpp` | DeviceType + DeviceTypeImpl + DeviceOptionSet[Const]Iterator | Closely coupled internal types. |
| `cached_parser.cpp` | CachedParser + CachedFile + CacheEntry | Logical unit. |
| `prefetch.cpp`, `prefetch_helpers.cpp`, `prefetch_prepare.cpp` | Prefetch + Node helpers | Node-typed free functions used internally. |
| `tracing.cpp` | TraceProvider + ScopedSpan + Resource | One subsystem. |

### Worth a closer look (lower priority than C/D)

| File | Pile-up | Recommended action |
|------|---------|---------------------|
| `compiler.cpp` | Compiler + FrontEndLoweringFacade | Likely split — facade is a small adapter. **Verify size first.** |
| `value.cpp` | Value + Immediate + ValueException | Immediate is its own type hierarchy; could split. |
| `exception.cpp` | Exception + ZIAWGCompilerException + ZIAWGOptimizerException | All under common base; legitimate. |
| `wave_index_tracker.cpp` | WaveIndexTracker + WavetableException + ErrorMessages | ErrorMessages is the real outlier (see F). |

---

## Section F — `ErrorMessages` ubiquity

`ErrorMessages` appears as a class in **9 different .cpp files**:
`asm_commands.cpp`, `asm_commands_impl_cervino.cpp`,
`asm_commands_impl_hirzel.cpp`, `awg_assembler_impl_pipeline.cpp`,
`error_messages.cpp`, `prefetch.cpp`, `wave_index_tracker.cpp`,
`waveform_generator.cpp` (and likely a few inline mentions).

This is probably **anonymous-namespace per-TU instances** of the same
pattern (string-table struct of error message constants) rather than a
single class — but warrants verification. If it's truly the same class,
this is an ODR violation waiting to manifest under a non-static-archive
link.

**Not tracked in TODO.md or unknowns.md.** Should be a Phase 16b item.

---

## Section G — Cross-reference: TODO.md summary table accuracy

| Row | Current | Reality | Status |
|-----|---------|---------|--------|
| SeqCAstNode print/clone bodies | 1 | 2 markers + macro-expanded 53×2=106 stubs | **stale** |
| CustomFunctions built-in function bodies | 27 | 14 utility stubs + 6 wrapper stubs + ctor binding gap | **wrong category, undercount** |
| WaveformGenerator waveform function bodies | ~35 | 32 throw-stubs + 4 misc | accurate (~) |
| CachedParser method bodies | 0 (just updated) | 3 markers — all docs about boost-archive | **was 7, now 0 is correct** |
| CustomFunctions field unknowns | 6 | unknowns 101/102/104 closed; remaining = 0-2? | needs recount |
| Prefetch/Cache approximate logic | 10 | 0 markers in prefetch (per Phase 15b audit) | **stale** — already closed |
| AsmOptimize approximate logic | 2 | 1 marker in asm_optimize.cpp | accurate |
| Compiler pipeline gaps | 1 | 2 markers in compiler.cpp | minor stale |
| MathCompiler | 67 symbols | 0 reconstructed (only ctor + dispatch in custom_functions.cpp) | accurate |
| WavetableManager\<T\> | 14 methods | not re-audited | unknown |
| DeviceType/Family/Option | 150 symbols | partially done in 14b-ii (devices reconstructed) | needs recount |
| Logging/tracing | done | confirmed | accurate |

---

## Section H — Cross-reference: unknowns.md actionable items

Currently in Actionable section:
- **#98**: FrontendLoweringState::result type — already typed correctly;
  awaiting second confirmation site. Low effort to close.
- **#10**: smap remaining logic (~0x1E6 bytes after alui call). Real work.

Currently in Carry-forward (Phase 15c):
- **simplifyAssign @0x280e10** stub
- **splitReg @0x281000** stub
- **assembler.hpp register field rename**

Blocked items (#18, #54, #55, #59) — depend on parser/pipeline analysis.

These are **already tracked**; the audit doesn't add new ones to
unknowns.md, but does surface that the carry-forward items deserve
explicit TODO.md rows in the summary table.

---

## Section I — Untracked work surfaced by this audit

Items with no current TODO.md or unknowns.md row:

1. **custom_functions.cpp / hpp split** (Section C1) — file-organization
   issue, not algorithmic.
2. **CustomFunctions::CustomFunctions ctor 19KB binding block** (Section
   C2 last bullet) — runtime correctness gap.
3. **CustomFunctions 14 utility-stub methods** (Section C2 first bullet)
   — undercount in current TODO row 14.
4. **`ErrorMessages` ODR investigation** (Section F).
5. **waveform_generator.cpp: split exceptions, recount throw-stubs**
   (Section D) — minor.
6. **seqc_ast_node.cpp print/clone macro expansion** (53 subclasses ×
   2 methods) — undercount in TODO row 13 (SeqCAstNode print/clone).

---

## Summary

- **22 source files with markers**; only 2 are critical: `custom_functions.cpp`
  (56) and `waveform_generator.cpp` (21+32).
- **One major pile-up file**: `custom_functions.{cpp,hpp}` mixes 10
  unrelated types and contains an undisclosed runtime correctness gap
  (empty funcMap).
- **One subtle stub pattern**: `throw std::runtime_error("X not
  implemented")` only used in `waveform_generator.cpp`; not all carry
  TODO comments. **Future audits MUST grep for this throw pattern in
  addition to TODO/FIXME/etc.**
- **TODO.md summary table has 6+ stale or inaccurate rows** (Section G).
- **unknowns.md is mostly accurate**; doesn't need new entries from this
  audit, but the carry-forward items deserve TODO.md rows.
- **6 untracked work items** identified (Section I).
