# OVERVIEW — Symbol Renames (Phases D, R, S)

> **Archived 2026-05-12** — moved from `OVERVIEW.md` during D15 cleanup.
> All three phases (D, R, S) closed long ago; the per-commit tables
> below are historical reference for symbol-rename audits.  Active
> `OVERVIEW.md` retains a one-paragraph pointer to this file.

The Symbol-Renaming Audit (Phases D / R / S) was a multi-month sweep
that produced the canonical names in `reconstructed/include/zhinst/`
today.  Source-of-truth analysis is in
`reconstructed/notes/symbol-renaming-audit/SYNTHESIS.md`.

## Symbol Renames (Phase D)

20 commits (`d15ad32`..`9b2e690`). Score 259/259 throughout (the
size of the diff suite at the time of Phase D, 2026-04-29; the
suite has since grown to 2499/2499).
Per-commit summary:

| Commit | Cluster / Topic | Sites |
|--------|-----------------|-------|
| `d15ad32` c1  | nm-recheck closes audit scan phase | docs only |
| `b857bc3` c2  H | `clone()` → `doClone()` | 19 decls + 17 defs + 59 calls / 4 files |
| `0a993b8` c3  F | `SeqCAstNode::type` param/accessor → `lineNr` | 54 sites / 3 files |
| `8014f3a` c4  G | `first_`/`second_` → role-named in 8 binary AST classes | ~70 sites / 3 files |
| `3e7b911` c5  J | `Waveform`/`WaveformFile` JSON-key field renames | 9 items |
| `a59b4b4` c6+c7 B+A | `isWaveformCmd` → `noOpt` and `flag` → `noOpt` (atomic) | 2 clusters |
| `4346d5a` c8  K | `PlayConfig` producer param renames | producer-side |
| `284b5d1` c9  L | `wvf`/`wvfi` impl param alignment | impl decls |
| `a481fed` c10 D | `channelGrouping` → `loopUnrollLimit` (3-leg) | 3-source rename |
| `c1e3aa3` c11 C | drop Hirzel aliases + `Cache::appendMode_` → `isHirzel_` | aliases + field |
| `26e8b08` c12 E | drop accessor aliases + `PNS::requiredSlots` → `usedCache_` | accessors + field |
| `612eb2a` c13 N | `Resources::parent_` → `grandparent_`, `parentWeak_` swap | 1 class |
| `e22c1b5` c14 §4 | high-confidence singletons | 35 items |
| `5a44521` c15 M | recompose `namespace Assembler` + `AssemblerInstr` → class `Assembler` | recomposition |
| `2477f4e` c16 §2 | open arbitrations | resolved set |
| `da68c0a` c17 | medium-confidence singletons | 31 items |
| `82694d7` c18 | dead-code removals | 2 items |
| `717cf8e` c19 §8/§9 | promote audit findings → `incidental_findings.md` | IF-110..IF-122 |
| `9b2e690` ci  I | `sfc::*Option::Bit0xNNNN` → semantic names (DeviceOption codes) | 52 enumerators / 8 files |

Deferred per audit policy:
- **Phase Q** — 226 low-confidence / unsure cosmetic items
  (stylistic underscore consistency, abbreviation preferences,
  acceptable-but-improvable names). See SYNTHESIS §6.

## Symbol Renames (Phase R) — Audit follow-up

14 commits (`dfc278e`..`2b23826`). Score 259/259 throughout (Phase R
era; the suite has since grown to 2499/2499).
Resolved every deferred arbitration from `2477f4e` and every open
incidental finding promoted in `717cf8e` (IF-110..IF-122).

| Commit | Item | Outcome |
|--------|------|---------|
| `dfc278e` R.0 | IF-111, IF-122 | closed (already fixed in Phase D) |
| `0288bde` IF-121 | dead `DeviceOpts` namespace | removed |
| `dbabd4e` IF-120 | `configFreqSweep` magic literals | wired `kSuserSweep*` constants |
| `49f1463` IF-114 | `PlayConfig::now` | kept (JSON contract) |
| `7a87e7e` IF-118 | `AddressImpl<T>` | kept (single instantiation, ~300 sites) |
| `085eaca` IF-113 | `Cache::Pointer::hash_` | kept |
| `6cee522` Arb 7/9/11 | mergeWaveforms / asm_list / loopArgNodeAppend | kept |
| `43b12c9` IF-115 | polarity-inverted bools | useAbsolute→useMapped fixed; `direction` rename done |
| `43b12c9` IF-116 | `valueType` → `direction` field | type-fix deferred → **FIXED 2026-04-29** |
| `da32249` IF-110/IF-112/IF-119 | trace plans recorded | needs-GDB |
| `69fafbf` IF-110 | `Value::pad_04_` | dismissed (genuine padding, GDB-confirmed) |
| `352ec74` IF-112 + Arb 5 | `NodeMapItem::hasFast` | dismissed (bool correct; GDB on full manifest saw only 0/1) |
| `bf04292` IF-119 | `setPRNGSeed` Var path | **fixed** — latent bug; replaced `AsmRegister(value_.toInt())` with `args[0].reg_` |
| `08c8135` Arb 4 | `Compiler::usedSampleRate_` | **writer reconstructed** at end of `Compiler::compile` (offset `0x1213d6`) |
| `2b23826` Arb 2 | `configFreqSweep` numDIOBits bound | kept (binary-faithful; gate is `kDevSHFPlus`, not UHFLI) |

Outstanding deferrals after Phase R:
- **Phase Q** (226 cosmetic items) — addressed in Phase S below.
- ~~IF-116~~ `EDirection` enum type-fix — **FIXED (2026-04-29)**:
  Converted `int32_t direction` → `EDirection direction` in expression.hpp;
  updated 5 sites in expression.cpp, 13 sites in seqc_parser.tab.c.
  259/259 tests pass.

## Symbol Renames (Phase S) — Phase Q refinement

7 commits (`3f0b24e`..`04e3ac5`). Score 259/259 throughout (Phase S
era; the suite has since grown to 2499/2499).
S.1 reconciled the 226-item backlog into 4 buckets; S.2 landed
mechanical sweeps for Bucket 1.

| Commit | Step | Outcome |
|--------|------|---------|
| `3f0b24e` | TODO | defined Phase S plan |
| `54a1af9` | S.1 | SYNTHESIS §6 reconciliation: B1=15 mech, B2=114 borderline-deferred, B3=39 already-done, B4=58 wontfix |
| `fb40bfb` | S.2 M1 | `AsmCommands::smap` `arg → value` (1 item) |
| `423ec7a` | S.2 M2 | `regInv→regInvalid`, `reg0→regZero` in `play`/`playIndexed` (2 items, 6 sites) |
| `e522deb` | S.2 M3 | `string_218_→pad_218_` in `AWGCompilerImpl` (2 items: 1 rename, 1 kept per audit) |
| `c11ff22` | S.2 M4 | `SeqCNeg::evaluate` local `d → negatedDouble` (1 item) |
| `04e3ac5` | S.2 M5 | dead-param/local removal across asm + custom_functions + awg_assembler (9 items: 8 applied, 1 binary-fidelity skip) |

Phase Q final state: 39 already-done + 58 wontfix + 14 mechanical-resolved
+ 1 binary-fidelity skip = 112 closed; 114 borderline items deferred
case-by-case. No further Phase Q phase planned.
