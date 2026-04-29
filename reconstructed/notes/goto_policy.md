# `goto` Usage Policy

Date: 2026-04-29 (Phase 39b)

The reconstructed codebase contains 135 `goto` sites. Before "cleaning
up" any of them, read this file. Most are not bugs and not bad style
in this codebase's context — they are deliberate reconstructions of
control flow from the original binary.

## The three buckets

### Bucket 1 — Generated code (24 sites): off-limits

| File | `goto` count |
|---|---|
| `seqc_parser.tab.c` | 18 |
| `seqc_lexer.c` | 6 |

These are Bison and Flex output. Any cleanup must happen in the
grammar/lexer source files (`seqc_parser.y`, `seqc_lexer.l`,
`asm_lexer.l`, `asm_parser.y`), and `goto` is the parser-generator's
own idiom there too. **Do not edit the `.tab.c` / `seqc_lexer.c` files
to remove gotos.**

### Bucket 2 — Faithful binary CFG reconstructions (~105 sites): leave

| File | `goto` count | Subject |
|---|---|---|
| `seqc_ast_nodes_evaluate.cpp` | 44 | identifier-resolution mega-switch with shared `name_tail:` / `default_error:` tails |
| `prefetch.cpp` | 43 | worklist state machine reconstructing the binary's CFG |
| `prefetch_placesingle.cpp` | 20 | placement state machine (load/play/finalize labels) |
| `prefetch_prepare.cpp` | 11 | worklist with `cleanup_validwaves` / `push_next_and_loop` |
| `prefetch_emit.cpp` | 7 | tree-walk with `treeWalk` / `advanceLoad` / `notFoundInTree` labels |
| `asm_optimize.cpp` (2 of 3) | 2 | outer-loop `next_instruction:` continue |
| `custom_functions_play.cpp` | 1 | `step9_return:` shared early-exit |

Identifying signs that a `goto` is in this bucket:

- The line carries a `// @0x…` or `// 0x… → 0x…` binary-address breadcrumb.
- The label name appears in the file's notes (e.g. `notes/optimization_passes.md`,
  `notes/prefetch.md`) or is a synonymous translation of an
  address (`enqueue_next`, `cleanup_parent`, `do_swap_and_enqueue`,
  `name_tail`, `advance`, `treeWalk`).
- The function is a single large CFG block reconstructed from one binary
  function (one symbol address in the `// 0x…` comments at function entry).

Rationale for keeping these:

1. **Differential-debugging breadcrumbs.** Every `// 0x…` line lets
   you GDB-trace the original binary against the recon. Restructuring
   into helper functions or `do { … } while (false)` ladders breaks
   that 1:1 correspondence.
2. **Codegen drift risk.** Helper-function extraction changes register
   allocation and basic-block layout. The build (and its `nm` parity
   audits against the binary) is one of the project's anchors.
3. **Re-verification cost.** Each restructuring would require a full
   round of differential testing plus re-tracing the binary CFG to
   ensure no semantic drift. The cost outweighs the readability gain.

**Policy**: do not restructure a `goto` in this bucket without first
re-tracing the binary CFG and adding a note to
`notes/incidental_findings.md` recording the verification.

### Bucket 3 — Small isolated cleanup candidates (≈6 sites): case-by-case

These are gotos where the surrounding function is small, the binary
CFG is uncomplicated, and a structured replacement would not muddy
the differential-debugging story.

| File:line | Label | Verdict (per Phase 39b audit) |
|---|---|---|
| `zi_folder.cpp:153` → `:183` | `resolve_home` | Binary-faithful (mirrors `jmp 0x2cf4eb`); refactor into `resolveHome()` helper is possible but not required. **Leave for now.** |
| `node.cpp:372,376` → `:433` | `throw_error` | **Safe to refactor** into `[[noreturn]] static void throwSwapError()` — both throws happen after the function's normal `return;`, so no shared scope. ~5-line edit. |
| `csv_parser.cpp:549` → `:583` | `skip_comment` | Could replace `goto skip_comment;` with `++lineNum; continue;` directly, but doing so requires hoisting the `continue` past the surrounding inner block. Marginal improvement. **Leave.** |
| `csv_parser.cpp:859` → `:890` | `skip_comment_ir` | Same shape and verdict as above. **Leave.** |
| `asm_list.cpp:366` → `:584` | `cleanup_and_next` | Tail is `wavetableFront++;` then loop continues. Vectors are RAII-cleaned. Could be `wavetableFront++; continue;` at the goto site, but the binary's `0x266d87` is the explicit cleanup landing pad; keeping the label preserves the breadcrumb. **Leave.** |
| `custom_functions_play.cpp:607,624,630` → `step9_return` | `step9_return` | Pinned to `// @0x15fdfd`, `// @0x15fdf6`, `// @0x16098c` breadcrumbs. **Bucket 2 (binary-faithful), leave.** |
| `asm_optimize.cpp:980` → `:1126` | `cleanup` | Single `goto` to function-end cleanup. RAII would let this be `return`, but optimizer hot path — verify codegen first. **Leave for now.** |

Net result of audit: of ~6 candidates, only **`node.cpp` `throw_error`**
is a clean win that doesn't require additional verification work. It
is not currently scheduled (Phase 39b is research-only); a future
sub-phase may take it on.

## Quick rule for future contributors

> **If the `goto` line has a `// @0x…` or `// 0x…` comment, it's in
> Bucket 2 — leave it. If you can't find a matching binary address
> in the file, it might be Bucket 3 — but still verify before editing.**

## Inline-asm corollary

The same "binary fidelity" principle applied to the only inline-asm
site in the codebase (`util_wave.cpp:96`) — but there the SSE2
intrinsic `_mm_max_sd` lowers to the identical `maxsd` instruction on
all major compilers, so the inline asm could be replaced with no loss
of binary fidelity. See IF-106 in `incidental_findings.md`.
