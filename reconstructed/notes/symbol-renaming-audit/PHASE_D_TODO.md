# Phase D Execution TODO

This file is a checklist of the 20-step Phase D execution sequence
defined in SYNTHESIS.md §11. Phase D begins when the first commit
below lands. As soon as Commit 1 touches source, the symbol-renaming
audit formally closes and AGENTS.md normal workflow resumes
(TODO.md tracking, OVERVIEW.md updates, build verify per sub-phase,
per-sub-phase commits).

Until then, edits remain restricted to `reconstructed/notes/symbol-renaming-audit/`
per RULES §11.

When Commit 1 lands, this file should be promoted to `TODO.md` (or
its items merged into TODO.md) and removed from the audit folder.

## Checklist

- [x] **Commit 1** — settle `nm` re-checks (~20 verify-not-original symbols, no source edits, audit notes only). **This commit closes the audit.** (landed in this commit; 33 resolved: 1 out / 28 in / 4 investigate)
- [x] **Commit 2** — Cluster H (`clone` → `doClone`, base + ×32+ subclass overrides). Mechanical, vtable-atomic.
- [x] **Commit 3** — Cluster F (`SeqCAstNode::type` → `lineNr` cascade, ×54).
- [x] **Commit 4** — Cluster G (binary AST node `first_`/`second_` ×8 classes; update SEQC_BINARY_IMPL macro + ctor params).
- [x] **Commit 5** — Cluster J (Waveform JSON-key drift; 9 renames; tier-2 anchored).
- [x] **Commit 6** — Cluster B (`isWaveformCmd` semantic inversion → **`noOpt` / `skipOptimization`**).
- [x] **Commit 7** — Cluster A (`flag` → cluster-B-canonical; AsmCommands br/brz/brnz/brgz + impl/Cervino/Hirzel overrides).
- [ ] **Commit 8** — Cluster K (PlayConfig producer/field swap; genPlayConfig + asmPlay + asmTable).
- [ ] **Commit 9** — Cluster L (AsmCommandsImpl::wvf/wvfi param drift; vtable-coordinated).
- [ ] **Commit 10** — Cluster D (`channelGrouping` → `loopUnrollLimit`; 3-leg coordinated).
- [ ] **Commit 11** — Cluster C (Hirzel family + Cache aliasing; drop both alias families; `Cache::appendMode_` → `isHirzel_`; `Cache::getBestPosition::appendMode` → `gapScan`). After this, Arbitration 10 can be resolved.
- [ ] **Commit 12** — Cluster E (drop forwarding-accessor aliases; rename PNS `requiredSlots` → `usedCache_`).
- [ ] **Commit 13** — Cluster N (`Resources::parent_` → `grandparent_`; two-step rename to avoid name collision; GlobalResources ctor param).
- [ ] **Commit 14** — High-confidence singletons (§4; 35 items, grouped by batch).
- [ ] **Commit 15** — Cluster M (Assembler type recomposition; GDB+RTTI confirm first; partly structural).
- [ ] **Commit 16** — Open arbitrations (§2; one PR per arbitration; investigate-first items: 1, 2, 4, 5, 7, 9, 11; resolve approved items: 3 swap, 6 keep lhs/rhs, 8 cmdToken/argList).
- [ ] **Commit 17** — Medium-confidence singletons (§5; 111 items; single commit).
- [ ] **Commit 18** — Dead-code removals (§7; 17 items; group by batch where applicable).
- [ ] **Commit 19** — Promote incidental bugs (§9; 7 IF-IDs added to `incidental_findings.md`). Promote type-suspicion observations (§8) to IF-IDs.
- [ ] **Commit 20+** — Phase Q cosmetic/style pass (§6 226 low/unsure items).

Side cluster (can land between commits 1–13 in its own dedicated commit):
- [ ] **Cluster I** — `sfc::*Option::Bit0xNNNN` (~39 enumerators; per-enum semantic naming decided at execution; coordinated with `device_mf.cpp` consumer tables and `mf_sfc.cpp` doc-comments).

## Per-commit discipline (per AGENTS.md)

- Run `cmake --build .` from `reconstructed/build/` before commit; triage all warnings.
- Run `python tests/diff_test.py` after every commit; revert on regression (do NOT amend).
- For Cluster M and the investigate-first arbitrations: GDB-trace before any source change.
