# `seqcc --from=<stage>` — Design Note

> **Status:** Design under discussion.  Filed at the boundary of
> Phase T6 to capture data-flow analysis findings before any
> recon edit lands.  Do not implement until the user approves a
> re-scoped T6 plan based on this note.

## Purpose

Phase T6 in `tools/seqcc/DESIGN.md` proposes:

> `--from=ast-lowered` (input = JSON via `Node::fromJson`),
> `--from=asm` (input = `.seqasm` via `AsmList::deserialize`),
> `--from=wavetable-ir` (JSON via `WavetableIR::fromJson`).

A first-pass analysis of `Compiler::compile()`'s step-method
boundaries (post-T5b) and of the actual round-trip surface of
each IR's serialiser uncovered three issues the original sketch
did not account for:

1. **The single-node `--dump=ast-lowered` artefact is not
   round-trippable.**  It emits one `Node::toJson(idMap)` blob,
   not the whole tree.
2. **Each `--from=` value needs more than just the deserialised
   IR.**  Two of the three need additional `Compiler` state
   (`compileResources_`, `compilePlaceholderAsm_`,
   `compileOptimizer_`) that the user cannot reasonably supply.
3. **Two of the three `--from=` values share the same resume
   boundary** (`stepOptPost`), differing only in which IR file
   the user supplies — which means they are not really three
   independent entry points.

This note enumerates the boundaries, the populated-slot picture
per mode, the construction work the driver would have to do on
behalf of the user, and the test-contract options.  It then
proposes a re-scoped T6 plan.

## 1. Pipeline state at each step boundary

The state slots referenced below are:

| Slot                       | Owner                  | Populated by      | Read by                                   |
|---                         |---                     |---                |---                                        |
| `config_`, `deviceConstants_`, `wavetable_`, `asmCommands_`, `customFunctions_`, `waveformGen_`, `messages_` | `Compiler` ctor | ctor (from `AWGCompilerImpl` ctor inputs) | every step |
| `parserContext_`           | `Compiler`             | ctor              | `parse` (called by `stepParse`)            |
| `compileExpr_`             | `Compiler` (T5b.2)     | `stepParse`       | `stepToSeqCAst`                            |
| `compileSeqcAst_`          | `Compiler` (T5b.2)     | `stepToSeqCAst`   | `stepLower`                                |
| `compileStaticResources_`  | `Compiler` (T5b.2)     | `stepToSeqCAst`   | `stepProject`                              |
| `compileResources_`        | `Compiler` (T5b.2)     | `stepToSeqCAst`   | `stepLower`, `stepBuildAsmPreamble` (`hasMain()`) |
| `compileLowerResult_`      | `Compiler` (T5b.2)     | `stepLower`       | `stepBuildAsmPreamble`                     |
| `ast_`                     | `Compiler` (binary)    | `stepLower`       | `stepBuildAsmPreamble`                     |
| `compileRootNode_`         | `Compiler` (T5b.2)     | `stepBuildAsmPreamble` | walked, then `ast_`                   |
| `asmList_`                 | `Compiler` (binary)    | `stepBuildAsmPreamble` | every later step                       |
| `compilePlaceholderAsm_`   | `Compiler` (T5b.2)     | `stepBuildAsmPreamble` | `stepPrefetch`                         |
| `compileOptimizer_`        | `Compiler` (T5b.2)     | `stepOptPre`      | `stepOptPost`                              |
| `compileWavetableIR_`      | `Compiler` (T5b.2)     | `stepOptPre`      | `stepPrefetch`, `stepProject`              |

The "binary" rows are pre-existing public/private members of the
binary's `Compiler`; the "T5b.2" rows are lifted locals added by
the recon to survive between step methods.  All T5b.2 slots are
**private**.

After T5b, `Compiler::reset()` zeroes every T5b.2 slot
(`compiler.cpp:125`).  The binary's pre-existing `ast_`,
`asmList_`, etc. are reset by the binary's own `reset()` logic
preserved at the same address.

## 2. Per-`--from=` resume-boundary table

For each proposed `--from=` value, the table below lists:

- **Resume at**: the first `step*` method that must be called.
- **Skipped steps**: methods the driver does *not* call.
- **Populated from input**: slots filled by parsing the input file.
- **Populated by driver helper**: slots the driver must construct
  itself before the resume step (because they are per-compile
  scaffolding, not input-derived).
- **Issue**: any blocker preventing a clean implementation.

### 2.1 `--from=ast-lowered`

| Field | Value |
|---|---|
| Resume at | `stepBuildAsmPreamble` |
| Skipped steps | `stepParse`, `stepToSeqCAst`, `stepLower` |
| Populated from input | `ast_` (the lowered AST root) |
| Populated by driver helper | `compileStaticResources_`, `compileResources_` (so `hasMain()` returns true and the warning callback is bound), `messages_` is reset by `reset()` |
| Issue | **The current `--dump=ast-lowered` artefact is single-node only — `Node::toJson(idMap)` serialises one node with `next`/`loop`/`branches` as integer ID references.  There is no whole-tree dump format today.**  The only whole-tree serialiser is `AsmList::serialize`, which emits placeholders inline in the asm text DSL (`AsmList::deserialize` at `compiler.cpp:574` does the matching `Node::fromJson + installPointers`).  Round-tripping `--dump=ast-lowered → --from=ast-lowered` is therefore impossible until either (a) a whole-tree `Node` dump format is added, or (b) the resume input is taken from an `AsmList` text DSL file (which makes this mode identical to `--from=asm`). |

### 2.2 `--from=asm`

| Field | Value |
|---|---|
| Resume at | `stepOptPre` (if the input is pre-prefetch) **or** `stepOptPost` (if the input already contains prefetch results) |
| Skipped steps | `stepParse`, `stepToSeqCAst`, `stepLower`, `stepBuildAsmPreamble`, optionally `stepOptPre` + `stepPrefetch` |
| Populated from input | `asmList_`, `compilePlaceholderAsm_` (recovered by scanning `asmList_` for the load placeholder anchor) |
| Populated by driver helper | `compileStaticResources_` + `compileResources_` (still needed by `stepProject`'s `usedSampleRate_` lookup, even though `stepOptPre`/`stepOptPost` don't read them); `compileWavetableIR_` if resuming at `stepOptPost`; `compileOptimizer_` if resuming at `stepOptPost` (and freshly constructed, not deserialised — see below) |
| Issue | **Whether `--from=asm` resumes at `stepOptPre` or `stepOptPost` depends on the *content* of the input file**, not the user's flag.  An `AsmList` text DSL that contains a load placeholder marker is pre-prefetch; one that does not is post-prefetch.  The driver would need to detect this from the input.  Additionally, `compileOptimizer_` carries pass-internal state (label maps, register allocation tables) accumulated during `stepOptPre`; the binary's `AsmOptimize` does not have a `serialize`/`deserialize` round-trip, so resuming at `stepOptPost` requires constructing a fresh `compileOptimizer_` and trusting that the asm input is already in the shape the post-pass machinery expects.  In practice this works — most of the post-passes are stateless on the asm and only the register-allocation pass holds state, which is reset per-call. |

### 2.3 `--from=wavetable-ir`

| Field | Value |
|---|---|
| Resume at | `stepOptPost` (the only step that reads `compileWavetableIR_` outside `stepOptPre`/`stepPrefetch`/`stepProject`) |
| Skipped steps | `stepParse`, `stepToSeqCAst`, `stepLower`, `stepBuildAsmPreamble`, `stepOptPre`, `stepPrefetch` |
| Populated from input | `compileWavetableIR_` |
| Populated by driver helper | `asmList_` (**not in the input file at all**), `compileStaticResources_`, `compileResources_`, `compileOptimizer_` (freshly constructed) |
| Issue | **`--from=wavetable-ir` cannot work from a single input file.**  `stepOptPost` reads both `asmList_` and `compileWavetableIR_`; the wavetable JSON alone is insufficient.  This mode would need either (a) two input files (`--from=wavetable-ir --asm-input=foo.asm`), or (b) a combined dump format that carries both an `AsmList` and a `WavetableIR`.  Neither exists today.  This mode is effectively `--from=asm-after-prefetch + wavetable-ir-sidecar`. |

## 3. Common construction work

Three things the driver would have to do for *any* `--from=`
mode that skips `stepToSeqCAst`:

1. **Construct `compileStaticResources_` + `compileResources_`** from `config_` + `deviceConstants_` + `messages_`, exactly mirroring `stepToSeqCAst` lines 351-363.  This is per-compile scaffolding — the user can't supply it because it depends on driver-side compiler state.
2. **Publish `compileResources_` into `customFunctions_->resources_`** (line 363).  Without this, any post-lowering step that touches `customFunctions_` (currently none of the back-end steps, but a future change could break this) would null-pointer-deref.
3. **Decide whether to call `messages_.reset()`** (already inside `Compiler::reset()`).  The driver calls `reset()` before populating any slot, so this is automatic.

Item 1 is the awkward one: it is *not* a step method, and it is
*not* exposed as a public helper on `Compiler`.  The driver
would have to either (a) duplicate the 12-line construction
inline (fragile to future changes in `stepToSeqCAst`), or (b)
get a new public helper `Compiler::setupResources()` factored
out of `stepToSeqCAst`.  Option (b) is a small recon edit
(factor a helper, call it from `stepToSeqCAst`'s top).

## 4. Test contract options

### 4.1 Strict byte-equal ELF

The cleanest test would compile a `.seqc` to ELF, dump the
intermediate IR, recompile from the IR, and assert byte-equal
final ELF.  This requires:

- Round-trippable dump format (broken today for
  `--from=ast-lowered`).
- Identical resources / customFunctions state on the resumed
  compile.
- No driver-side construction divergence vs the
  `stepToSeqCAst`-baked state.

**Verdict**: achievable for `--from=asm` (the whole-tree dump
format already exists via `AsmList::serialize`/`deserialize`,
proven by the binary's `serializeRoundTrip` debug flag at
`compiler.cpp:574-577`).  Not currently achievable for
`--from=ast-lowered` or `--from=wavetable-ir`.

### 4.2 Successful recompile only

Looser contract: the resumed compile produces *a* valid ELF (no
errors, no crashes), but byte-equality is not asserted.
Trivially achievable for all three modes but provides little
verification value.

### 4.3 Round-trip via `--to=<earlier-stage>`

Compile `.seqc → --to=asm` to a `.seqasm` file; reload with
`seqcc -x asm --from=asm` (which routes through the legacy
`AWGAssembler` entry, not the main `Compiler`); compare against
the original `--to=link` ELF allowing for the documented
`+0x80` `e_entry` offset.  This is the natural shape for T7
(`seqas` mode) rather than T6.

## 5. Re-scoped T6 proposal

Given the findings above:

### 5a. Drop `--from=ast-lowered` from Phase T6 entirely.

The single-node `--dump=ast-lowered` artefact is not
round-trippable, and a whole-tree `Node` dump format is a
larger feature (new serialiser, new format spec, schema
versioning).  Defer to a future phase if and when a concrete
consumer need materialises.  Document the limitation in
DESIGN.md §2 "Out of scope" alongside the existing
`Expression` / `SeqCAstNode` exclusion.

### 5b. Drop `--from=wavetable-ir` from Phase T6 entirely.

This mode cannot work from a single input file.  The natural
shape would be a combined "post-prefetch state" dump format
(`AsmList` + `WavetableIR` in one file), which is a Phase T6+
feature.  Defer.

### 5c. Land `--from=asm` only, with a narrower scope.

This is the one mode that:

- Has a proven round-trip surface (`AsmList::serialize` /
  `AsmList::deserialize`, with whole-tree `Node` round-trip
  baked in).
- Has a strict-byte-equal test contract (4.1 above).
- Maps cleanly onto a single resume boundary (`stepOptPre`),
  if we constrain the input to be pre-prefetch — exactly what
  `--to=asm` produces today.

Sub-phases:

- **T6.1** — file IF-307 sanctioning the recon edit (see §6).
- **T6.2** — `AWGCompiler::compiler()` accessor (`Compiler&`)
  on the public header; `Compiler::setupResources()` public
  helper factored from `stepToSeqCAst`; **lifted member
  visibility decision** (see §7).
- **T6.3** — driver wiring: `--from=asm` reads input,
  constructs `AWGCompiler` + `AWGCompilerConfig` as today,
  calls `compiler().reset()`, populates `asmList_` via
  `deserialize`, recovers `compilePlaceholderAsm_` from the
  list, calls `setupResources()`, then `stepOptPre` →
  `stepPrefetch` → `stepOptPost` → `stepUnsyncCervino` →
  `stepProject` → back-end forwarders.
- **T6.4** — round-trip test
  `tests/tools/test_seqcc_from.py`: compile `.seqc` two ways
  and assert byte-equal final ELF.

This delivers the user-visible behaviour the original T6
promised, for the one IR where the underlying machinery
actually supports it, with no design ambiguity.

### 5d. Document the deferred modes openly.

Update `tools/seqcc/DESIGN.md` §2 "Out of scope" with
explicit notes for `--from=ast-lowered` and
`--from=wavetable-ir`, citing this design note.  Add a
`\verifyme` cross-reference if it surfaces in user-facing
docs.

## 6. IF-307 scope

Under the re-scoped T6, the sanctioned recon edit is:

1. `reconstructed/include/zhinst/codegen/awg_compiler.hpp`: add
   public `Compiler& compiler()` + `Compiler const& compiler() const`
   accessor.  Implementation in the `.cpp` to keep the public
   header free of `compiler.hpp`.  Forward-declares `class Compiler`.
2. `reconstructed/include/zhinst/codegen/compiler.hpp`: factor a
   public `void setupResources()` helper from `stepToSeqCAst`
   (lines 351-366), and call it from the top of `stepToSeqCAst`
   so the binary path is unchanged.
3. **Visibility change for §7 below**: either promote ~3 lifted
   members to public, or add narrow setters.  See §7.

Total: ~5 lines of header additions, ~15 lines of `.cpp`
additions, ~2 lines of refactor inside `stepToSeqCAst`.

Verification: full 5-gate regression sweep + new
`test_seqcc_from.py` round-trip.

## 7. Lifted-member visibility decision

The driver needs to write `asmList_` (binary-layout member at
`compiler.hpp:803`, **inside the `private:` section** starting
at line 742) and `compilePlaceholderAsm_` (T5b.2 lifted, also
private).  Both are private today.

> **Verify-then-write check (resolved):** `compiler.hpp` has two
> public sections (line 194 for the constructor / public API,
> line 667 for the `step*` methods added by T5b.4) and two
> private sections (line 661 for narrow internal helpers, line
> 742 for the data members).  `asmList_` (803), `asmCommands_`
> (811), `customFunctions_` (819), and all T5b.2 lifted slots
> (843+) are private.

Three options:

### 7a. Make `asmList_` and `compilePlaceholderAsm_` public.

Two access-section changes (or one — they can be grouped under
a new public-data section).  Symmetric in spirit with the
step methods that are already public, but widens the API
surface: every binary-layout member adjacent to `asmList_`
would have to be moved out of the way to keep the public
section coherent, or the public/private boundary would zigzag.

### 7b. Add narrow setters: `setAsmList(AsmList)` + `setPlaceholderAsm(AsmList::Asm)`.

Two new public methods on `Compiler`.  Driver gets exactly
what it needs.  Symmetric with the proposed `setupResources()`
helper.  Adds ~6 lines of recon (2 declarations + 2
one-line bodies) plus the visibility delta is contained.

### 7c. Driver recovers the placeholder from `asmList_` by scanning, after using a setter only for `asmList_` itself.

Reduces 7b's two setters to one (`setAsmList`).  The
placeholder is created by `asmCommands_->asmLoadPlaceholder()`
and appended to `asmList_` at `compiler.cpp:506`; the driver
can iterate the deserialised list and find the entry whose
`assembler.cmd` matches the load-placeholder opcode.  **Needs
verification**: is the placeholder identifiable solely from
its opcode, or does it carry additional state we'd need to
reconstruct?

**Recommended**: 7b — symmetric with `setupResources()`, no
runtime scanning, no recoverability uncertainty.  7c is
attractive but risks an "almost-recoverable" placeholder that
loses some state the binary takes for granted.

## 8. Open questions for the user

1. **Approve the re-scope** (drop `ast-lowered` and
   `wavetable-ir` from T6, keep `--from=asm` only)?
2. **Test contract for T6**: strict byte-equal ELF (§4.1), or
   something looser?  My recommendation is §4.1 — the round-trip
   is already proven by the binary's `serializeRoundTrip` debug
   flag.
3. **Visibility for `compilePlaceholderAsm_`**: 7a (public
   field), 7b (setter), or 7c (recover from `asmList_`)?  My
   recommendation is 7c if the placeholder is recoverable by
   inspection, else 7b.
4. **Defer-doc shape**: update DESIGN.md §2 inline, or file a
   new TODO.md "T6-future" entry with the deferred work?  My
   recommendation is both — DESIGN.md for the user-facing scope
   statement, TODO.md for the implementation tracking.

## 9. References

- `reconstructed/src/codegen/compiler.cpp` — `reset()` at
  line 125, `stepToSeqCAst` at 346, `stepLower` at 375,
  `stepBuildAsmPreamble` at 428, `stepOptPre` at 535,
  `stepPrefetch` at 593, `stepOptPost` at 602,
  `stepUnsyncCervino` at 617, `stepProject` at 636.
- `reconstructed/src/asm/asm_list.cpp` — `deserialize` at line
  234; whole-tree `Node` round-trip via `nodeMap` at 330,
  `installPointers` at 611.
- `reconstructed/src/ast/node.cpp` — `toJson(idMap)` at 475
  (single-node serialiser), `fromJson` at 584 (single-node
  deserialiser), `installPointers` at 655.
- `reconstructed/include/zhinst/waveform/wavetable_ir.hpp` —
  `toJson` at 214, `fromJson` at 239.
- `reconstructed/notes/incidental_findings.md` — IF-304
  (partial closure at T5b.5; full closure unblocked by the
  `compiler()` accessor proposed here), IF-306 (T5b
  sanctioned-exception write-up).
- `tools/seqcc/DESIGN.md` — §2 "Out of scope", §3.1 step-method
  mapping (T5b.6), §6 T6 sub-phase plan (the original sketch
  this note re-scopes).
