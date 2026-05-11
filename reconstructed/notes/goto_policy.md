# `goto` Usage Policy {#notes_goto_policy}

\note **Reverse-engineering reference material.** This page is part of
the `reconstructed/notes/` set: deep-dive technical notes for
contributors working on the reconstruction. It cites binary addresses,
opcodes, and disassembly observations directly so they remain
discoverable from the rendered site. The standard documentation-voice
rules for API briefs (no binary citations outside `\binarynote`) do
**not** apply to this page.

Date: 2026-04-29 (Phase 39b, updated Phase 39d)

The reconstructed codebase contains 153 `goto` sites (post-Phase 39d).
Before "cleaning up" any of them, read this file. Almost all are not
bugs and not bad style in this codebase's context — they are deliberate
reconstructions of control flow from the original binary.

> **Phase 39d update**: The earlier 135 figure undercounted Bucket 2
> by ~18 sites in `prefetch*.cpp` (regex was line-anchored against
> 4-space indent only). The corrected per-file counts below match
> `grep -rE "^\s*goto\s+\w+" reconstructed/src`.

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

### Bucket 2 — Faithful binary CFG reconstructions (129 sites): leave

| File | `goto` count | Subject |
|---|---|---|
| `seqc_ast_nodes_evaluate.cpp` | 44 | identifier-resolution mega-switch with shared `name_tail:` / `default_error:` tails |
| `prefetch.cpp` | 43 | worklist state machine reconstructing the binary's CFG |
| `prefetch_placesingle.cpp` | 20 | placement state machine (load/play/finalize labels) |
| `prefetch_prepare.cpp` | 11 | worklist with `cleanup_validwaves` / `push_next_and_loop` |
| `prefetch_emit.cpp` | 7 | tree-walk with `treeWalk` / `advanceLoad` / `notFoundInTree` labels |
| `asm_optimize.cpp` | 3 | outer-loop `next_instruction:` continue + `cleanup` early-exit |
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

### Bucket 3 — Small isolated cleanup candidates: resolved (Phase 39d)

All Bucket 3 candidates have been triaged. Three were refactored in
Phase 39d; the rest were promoted to Bucket 2 after closer inspection.

| File:line | Label | Phase 39d resolution |
|---|---|---|
| `zi_folder.cpp:153` → `:183` | `resolve_home` | **Refactored** (39d-ii): body extracted into `resolveHomeFolder` lambda. Build + tests pass. |
| `node.cpp:372,376` → `:433` | `throw_error` | **Refactored** (39d-i): both throws routed through `[[noreturn]] throwSwapNotConnected()` in anonymous namespace. Build + tests pass. |
| `csv_parser.cpp:549,859` → `:583,890` | `skip_comment[_ir]` | **Refactored** (39d-iii): replaced with `bool hasTimeColumn` flag pattern, both labels removed. Build + tests pass. |
| `asm_list.cpp:366` → `:584` | `cleanup_and_next` | **Refactored** (39d-iv): replaced with `wavetableFront++; continue;` at the goto site. The 3 vectors are loop-local — RAII cleans them at scope exit. The `0x266d87` breadcrumb is preserved as a comment on the loop tail. |
| `custom_functions_play.cpp:607,624,630` → `step9_return` | `step9_return` | Pinned to `// @0x15fdfd`, `// @0x15fdf6`, `// @0x16098c` breadcrumbs — promoted to Bucket 2. |
| `asm_optimize.cpp:980` → `:1126` | `cleanup` | Optimizer hot path; codegen-sensitive — promoted to Bucket 2. |

Net result: 4 sites refactored, 2 promoted to Bucket 2. No cleanup
candidates remain. Bucket 3 is now empty.

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
