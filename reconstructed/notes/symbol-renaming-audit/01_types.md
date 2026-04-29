# Batch 01 — types

## 1. Files considered

- `reconstructed/include/zhinst/types.hpp`

Cross-references (read-only, for usage survey):

- `reconstructed/include/zhinst/resources.hpp`
- `reconstructed/include/zhinst/awg_compiler_config.hpp`
- `reconstructed/include/zhinst/awg_device_props.hpp`
- `reconstructed/src/asm_commands.cpp`
- `reconstructed/src/custom_functions.cpp`
- `reconstructed/src/custom_functions_io.cpp`
- `reconstructed/src/custom_functions_play.cpp`
- `reconstructed/src/custom_functions_playback.cpp`
- `reconstructed/src/seqc_ast_node.cpp`
- `reconstructed/src/resources.cpp`
- `reconstructed/src/awg_compiler_config.cpp`
- `reconstructed/src/awg_device_props.cpp`

Binary symbol-table check (per §3, against `_seqc_compiler.so`) used to
exclude type names that are mangled and therefore authoritative.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `zhinst::AsmCommands` (and Impl/Cervino/Hirzel) | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::Node` | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::WaveformFront` | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::WavetableFront` | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::WaveformGenerator` | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::CompilerMessageCollection` | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::AWGCompilerConfig` | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::DeviceConstants` | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::CancelCallback` | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::Resources` | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::SeqCAstNode` | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::CustomFunctions` | — | — | type name in binary | — | omitted (§3 exclusion) |
| `zhinst::AwgDeviceType` (type) | — | — | type name in binary | — | omitted (§3 exclusion) |
| `AwgDeviceType::UHFLI` .. `VHFLI` (members) | no | high | match display strings in `.rodata` | keep current | not-misnomer |
| `AwgDeviceType::SHFQC_SG` | no | medium | reconstruction-only suffix, but unambiguous | keep current | not-misnomer |
| `zhinst::EDirection` (type) | — | — | type name in binary | — | omitted (§3 exclusion) |
| `EDirection::eIN` / `eOUT` / `eINOUT` | no | high | match `.rodata` `str()` table | keep current | not-misnomer |
| `zhinst::kRateInherit` | unsure | low | declared, never referenced | keep current; remove if dead | — |
| `zhinst::kNoWaveIndex` | unsure | low | declared, never referenced | keep current; remove if dead | — |
| `zhinst::kNoNodeId` | unsure | low | declared, never referenced | keep current; remove if dead | — |
| `zhinst::kNoPlayIndex` | unsure | low | declared, never referenced | keep current; remove if dead | — |
| `zhinst::kChannelTag_I` | yes | low | snake_case violates convention | `kChannelTagI` | coordinated-rename |
| `zhinst::kChannelTag_Q` | yes | low | snake_case violates convention | `kChannelTagQ` | coordinated-rename |
| `zhinst::kSuserNodeTag` | no | medium | usage matches multi-word write low-word role | keep current | not-misnomer |
| `zhinst::kSuserNodeAddr` | no | medium | usage matches mid-word (register index) role | keep current | not-misnomer |
| `zhinst::kSuserNodeValue` | no | medium | usage matches high-word value role | keep current | not-misnomer |
| `zhinst::kSuserNodeValueHi` | no | low | single use; matches double-precision-high comment | keep current | — |
| `zhinst::kSuserNodeSlowCommit` | no | low | two uses match slow-path commit | keep current | — |
| `zhinst::kSuserNodeCommit` | no | low | three uses match write-finalize role | keep current | — |
| `zhinst::kSuserNodeDirect` | no | medium | many uses; consistent with single-value direct write | keep current | not-misnomer |
| `zhinst::kSuserNodeFreqCommit` | unsure | low | comment-only; only one use site | keep current | — |
| `zhinst::kSuserNodeDirectB` | no | low | four uses, all on Q-channel/high32 path | keep current | — |
| `zhinst::kSuserTriggerLoad` | no | low | uses in `wtrig` / waitTrigger | keep current | — |
| `zhinst::kSuserTimestamp` | no | low | single use in `at(...)` timestamp store | keep current | — |
| `zhinst::kSuserNow` | no | low | single use in playback "now" path | keep current | — |
| `zhinst::kSuserRTLoggerData` | no | low | uses in `at()` RT-logger path | keep current | — |
| `zhinst::kSuserSyncA` / `kSuserSyncB` | no | medium | pair, used in `syncCervino` impl | keep current | not-misnomer |
| `zhinst::kSuserSyncHirzel` | no | medium | used in `asmSyncHirzel` impl | keep current | not-misnomer |
| `zhinst::kSuserWaitCycles` | no | medium | many uses in wait/cycle paths | keep current | not-misnomer |
| `zhinst::kSuserUserRegBase` | unsure | low | declared, never referenced | keep current; remove if dead | — |
| `zhinst::kSuserQAResult` | no | low | single use; matches getQAResult role | keep current | — |
| `zhinst::kSuserRTLoggerReset` | unsure | low | declared, never referenced | keep current; remove if dead | — |
| `zhinst::kSuserRTLoggerResetHdawg` | unsure | low | declared, never referenced | keep current; remove if dead | — |
| `zhinst::kSuserWaitLegacy` | no | low | two uses; matches play-internal wait | keep current | — |
| `zhinst::kSuserSinePhase0/1` | no | medium | uses in setSinePhase per channel | keep current | not-misnomer |
| `zhinst::kSuserSinePhaseInc0/1` | no | medium | uses in incrementSinePhase per channel | keep current | not-misnomer |
| `zhinst::kSuserPrngSeed` | no | medium | uses in PRNG-seed code paths | keep current | not-misnomer |
| `zhinst::kSuserPrngRangeLow` | no | low | matches PRNG range setup | keep current | — |
| `zhinst::kSuserPrngRangeSpan` | no | low | matches PRNG range setup | keep current | — |
| `zhinst::kSuserPrngValue` | no | low | matches getPRNGValue | keep current | — |
| `zhinst::kSuserQAWeights` | no | low | matches QA weights/result store | keep current | — |
| `zhinst::kSuserQATrigger` | no | low | matches QA trigger composite | keep current | — |
| `zhinst::kSuserQAResultLen` | unsure | low | declared, never referenced | keep current; remove if dead | — |
| `zhinst::kSuserSweepOscIdx` | no | low | matches sweep osc-index setup | keep current | — |
| `zhinst::kSuserSweepControl` | no | low | matches sweep control writes | keep current | — |
| `zhinst::kSuserSweepStartLo/Hi` | unsure | low | declared, never referenced | keep current; remove if dead | — |
| `zhinst::kSuserSweepStepLo/Hi` | unsure | low | declared, never referenced | keep current; remove if dead | — |
| `zhinst::kSuserWaitOnSync` | no | low | single use in waitOnSync | keep current | — |
| `zhinst::kAddrTrigger` | no | medium | used in `ltrig`/`strig` impl | keep current | not-misnomer |
| `zhinst::kAddrInternalTrig` | no | low | used in `sinttrig` impl | keep current | — |
| `zhinst::kDevAll` | no | medium | mask 0x1FF = all 9 devices | keep current | not-misnomer |
| `zhinst::kDevAllButUHF` | yes | medium | excludes only UHFLI, not UHFQA | `kDevAllButUHFLI` | coordinated-rename |
| `zhinst::kDevHirzel` | yes | medium | name implies all-Hirzel, but includes GHFLI/VHFLI which are Cervino/non-Hirzel by `getInstance()` | `kDevHirzelPlusLI`, keep current | — |
| `zhinst::kDevSHFPlus` | no | low | matches "SHF+ family" usage | keep current | — |
| `zhinst::kDevLIFamily` | no | medium | uses align with LI-family branches | keep current | not-misnomer |
| `zhinst::kDevCervino` | yes | medium | comment & code use it as "UHF only", not all-Cervino | `kDevUHF` (already an alias) | coordinated-rename |
| `zhinst::kDevUHF` | no | medium | accurate alias of 0x005 | keep current; deprecate `kDevCervino` | coordinated-rename |
| `zhinst::kDevPreSHFLI` | unsure | low | only one use site; name describes ordering | keep current | — |
| `zhinst::kDevQA` | no | low | single use; UHFQA+SHFQA matches "QA" | keep current | — |
| `zhinst::kDevHirzelAll` | yes | medium | "Hirzel + UHFLI" is a misleading "All" qualifier | `kDevHirzelPlusUHFLI` | — |
| `zhinst::kDevHirzelPlusUHFQA` | no | low | name describes contents accurately | keep current | — |
| `zhinst::kDevNone` | no | medium | mask 0 / "always unsupported" | keep current | not-misnomer |

## 3. Detailed findings

### `AwgDeviceType::UHFLI` .. `VHFLI` (member names)  [no / high / not-misnomer]

Evidence:
- `_seqc_compiler.so` `.rodata` strings (objdump): `UHFLI`, `HDAWG`,
  `UHFQA`, `SHFQA`, `SHFSG`, `SHFLI`, `GHFLI`, `VHFLI`, `SHFQC` are
  all present verbatim.
- `reconstructed/src/awg_compiler_config.cpp:44` returns
  `"SHFQC (SG)"` from a switch on value 32, and other cases return
  the bare display strings.
- Header comments state these are display strings used by
  `getAwgDeviceTypeString`/`getAwgDeviceTypeFromString` (binary
  addresses 0x270080/0x270180).

Interpretation:
- The enum-member identifiers are equal to the device display
  strings preserved in the binary's read-only data, except for
  `SHFQC_SG` which the binary stringifies as "SHFQC (SG)" and
  parses from "grimsel_qc_sg".

Judgement:
- Not misnomers — the names match the binary's own user-facing
  designators.

Locations consulted:
- declared: include/zhinst/types.hpp:34-44
- used: src/awg_compiler_config.cpp:44, src/awg_compiler.cpp:227,1148,
  src/awg_device_props.cpp:233, src/custom_functions_io.cpp:1299 etc.

### `AwgDeviceType::SHFQC_SG`  [no / medium / not-misnomer]

Evidence:
- `awg_compiler_config.cpp:44`: `case 32: return "SHFQC (SG)";` —
  the display string contains a space and parentheses, so it cannot
  be a C++ identifier verbatim.
- `awg_compiler_config.cpp:62`: parses input `"grimsel_qc_sg"` →
  value 32. The codename is `grimsel_qc_sg`.
- `awg_device_props.cpp:315`: `if (seq == AwgSequencerType::SG)
  return AwgDeviceType::SHFQC_SG;` — the value is reached only on
  the SG-sequencer branch of an SHFQC device.

Interpretation:
- The binary has no single-token identifier for value 32; both
  display ("SHFQC (SG)") and codename ("grimsel_qc_sg") forms
  contain "SG"/"sg" as a sub-token of SHFQC. The reconstructed
  identifier `SHFQC_SG` faithfully composes the device family
  with its sequencer role.

Judgement:
- Not a misnomer — it accurately reflects what value 32 represents
  and is unambiguous at every use site.

Locations consulted:
- declared: include/zhinst/types.hpp:40
- used: src/awg_device_props.cpp:233,235,315; src/awg_compiler.cpp:227,1148;
  src/compile_seqc.cpp:63; src/custom_functions_io.cpp:1299,1547,1647,1685,2709

### `EDirection::eIN` / `eOUT` / `eINOUT`  [no / high / not-misnomer]

Evidence:
- `seqc_ast_node.cpp:35-37`: `case EDirection::eIN: return "in";
  case EDirection::eOUT: return "out"; case EDirection::eINOUT:
  return "inout";` — the binary's `EDirection::str()` (@0x1c1730)
  emits exactly these three lowercase strings.
- `_seqc_compiler.so` strings: `eOUT` and `eINOUT` are present
  verbatim in the binary (likely as RTTI/typeinfo expansion or
  similar — `eIN` is a substring of others so cannot be confirmed
  alone, but the pair confirms the prefix convention).
- `resources.cpp:563,570,1697,1751,1818`: pattern is
  `if (dir != EDirection::eIN && ...)` — i.e. eIN is the lenient
  read-only path; non-eIN is the strict path.
- `custom_functions_io.cpp` (~50 sites): `res->readConst(name,
  EDirection::eOUT)` — read calls pass `eOUT` to indicate they
  expect the variable to have been written-to.

Interpretation:
- The serialized strings authoritatively bind `eIN→"in"`,
  `eOUT→"out"`, `eINOUT→"inout"`. The dual semantics
  (AST parameter direction vs Resources read/write-strict path)
  share the same enum because they share the same notion of
  data flow. Header comment (types.hpp:62-72) documents the dual
  use.

Judgement:
- Not misnomers; the names match the binary's serializer table.

Locations consulted:
- declared: include/zhinst/types.hpp:68-72
- str() table: src/seqc_ast_node.cpp:35-37
- AST callers: src/seqc_ast_node*.cpp
- Resources callers: src/resources.cpp:563,570,1697,1751,1818;
  src/custom_functions_io.cpp (many); src/custom_functions_playback.cpp:774-786;
  src/custom_functions.cpp:543

### `kRateInherit`, `kNoWaveIndex`, `kNoNodeId`, `kNoPlayIndex`  [unsure / low / —]

Evidence:
- `grep -rn '\bkRateInherit\b' src/ include/` → 0 hits outside the
  declaration in `types.hpp:76`.
- Same for `kNoWaveIndex` (types.hpp:77), `kNoNodeId` (types.hpp:78),
  `kNoPlayIndex` (types.hpp:79).
- Header comments name the file each was supposedly extracted from
  (`prefetch.cpp`, `wavetable_front.cpp`, `node.cpp`,
  `wave_index_tracker.cpp`).

Interpretation:
- These constants are declared but never used by any
  reconstructed translation unit. The named files use the raw
  literal `-1` instead (or use a different sentinel). The
  comments suggest they were introduced as documentation rather
  than to replace existing literals.

Judgement:
- Cannot judge naming quality without observed call sites; the
  *names* read clearly, but the symbols are dead code from the
  reconstruction's point of view.

Proposals:
- keep current (medium) — names are descriptive.
- remove (low) — if they are documentation-only and the call
  sites still spell `-1`, they belong in a comment, not a header
  symbol.
- coordinate replacement of literal `-1` at the named sites with
  these constants in a future task (low).

Locations consulted:
- declared: include/zhinst/types.hpp:76-79
- used: (none in src/ or include/)

### `kChannelTag_I`, `kChannelTag_Q`  [yes / low / coordinated-rename]

Evidence:
- `types.hpp:82-83`: `constexpr int kChannelTag_I = 0x0C;` and
  `kChannelTag_Q = 0x0D;`.
- `grep -rn '\bkChannelTag_[IQ]\b' src/ include/` → 0 hits outside
  the declaration.
- Convention (RULES §7) for constants is `ns::kName` — a single
  CamelCase tail, no underscore separating the channel letter.

Interpretation:
- The names violate the project's stated `kName` convention by
  including an underscore before a one-letter suffix. Symbols are
  unused so renaming has no cascade cost.

Judgement:
- Misnomer in the convention sense (form, not meaning).

Proposals:
- `kChannelTagI` / `kChannelTagQ` (medium) — strip the underscore
  to fit `k<CamelCase>` convention.
- keep current (low).

Cross-reference:
- writeToNode in `custom_functions_play.cpp` deals with I/Q
  channels (0x0C/0x0D), but the *literals* not these constants are
  used. A coordinated cleanup could introduce both.

Locations consulted:
- declared: include/zhinst/types.hpp:82-83

### `kDevAllButUHF`  [yes / medium / coordinated-rename]

Evidence:
- `types.hpp:50`: `kDevAllButUHF = 0x1FE` with comment
  "all except UHFLI". 0x1FE = 0x1FF & ~UHFLI(=1).
- Note that `UHFQA(=4)` is also a UHF device, but is included in
  the mask. The comment is correct ("UHFLI"); the *name* says
  "UHF" which colloquially covers both UHFLI and UHFQA.
- `custom_functions_io.cpp:129,228,340,366,1044`: uses for
  `getZSyncData`, `getFeedback`, `setID`, `assignWaveIndex`,
  `waitZSyncTrigger`. These functions are unsupported on UHFLI
  but supported on UHFQA, so the *value* matches the intent.

Interpretation:
- The bitmask value matches the documented intent ("everything
  except UHFLI"), but the symbol name `kDevAllButUHF` is
  ambiguous because "UHF" could mean UHFLI or all-UHF. The
  coexisting `kDevUHF` constant aliases UHFLI+UHFQA, so the same
  prefix has two meanings.

Judgement:
- Misnomer — the abbreviation "UHF" is overloaded between this
  symbol (which means UHFLI only) and `kDevUHF` (which means
  UHFLI+UHFQA).

Proposals:
- `kDevAllButUHFLI` (medium) — disambiguates.
- keep current (low).

Cross-reference:
- See `kDevUHF` / `kDevCervino` finding below — same family of
  abbreviation collisions.

Locations consulted:
- declared: include/zhinst/types.hpp:50
- used: src/custom_functions_io.cpp:129,228,340,366,1044

### `kDevHirzel`  [yes / medium / —]

Evidence:
- `types.hpp:51`: `kDevHirzel = 0x1F2 = HDAWG | SHFQA | SHFSG |
  SHFQC_SG | SHFLI | GHFLI | VHFLI`. Comment: "(sequencerRegBase
  =0x0d05)".
- `types.hpp:32-33` (header preamble): "Hirzel: HDAWG, SHFQA,
  SHFSG, SHFQC_SG, SHFLI" and "Cervino: UHFLI, UHFQA, GHFLI,
  VHFLI".
- The two statements are mutually inconsistent: by the preamble's
  own taxonomy, GHFLI(128) and VHFLI(256) are *Cervino*, not
  Hirzel — yet `kDevHirzel` includes them.
- `custom_functions_io.cpp:1257,1478,1591`: `kDevHirzel` is used
  to gate `waitSineOscPhase`, `setSinePhase`,
  `incrementSinePhase`, which are sine-related features available
  on the SHF+ family, not specifically on Hirzel cores.

Interpretation:
- The mask groups devices by some hardware capability (likely the
  shared sequencerRegBase=0x0D05 noted in the comment), not by
  the Hirzel/Cervino lineage as described in the preamble. The
  name therefore conflicts with the project's own definition of
  "Hirzel".

Judgement:
- Misnomer — name implies a lineage subset that the value
  contradicts.

Proposals:
- `kDevSequencerRegBase0D05` (low) — matches the comment's
  technical justification.
- `kDevHirzelLike` (low) — flags the "Hirzel-ish" intent without
  asserting strict lineage.
- keep current (medium) — if "Hirzel" is internally accepted
  shorthand for this capability cluster, renaming may cause more
  confusion than it fixes.

Locations consulted:
- declared: include/zhinst/types.hpp:51
- used: src/custom_functions_io.cpp:1257,1334,1383,1411,1478,1523,
  1576,1591,1636,1638,1672

### `kDevCervino` and `kDevUHF`  [yes / medium / coordinated-rename]

Evidence:
- `types.hpp:54-55`: both have value 0x005 = UHFLI(1) + UHFQA(4).
  `kDevUHF` is declared as an explicit alias.
- Header preamble (types.hpp:32-33) defines Cervino as
  "UHFLI(1), UHFQA(4), GHFLI(128), VHFLI(256)" — i.e. Cervino
  contains four devices, not two.
- Only the UHF subset (UHFLI+UHFQA) actually has value 0x005.
- Use sites (custom_functions_io.cpp:698,765,1861,2226,2315,2379;
  custom_functions_io.cpp uses only 7) gate functions like
  `waitTrigger`, `waitAnaTrigger`, `getAnaTrigger`,
  `getSweeperLength`, `at`, `lock` — all UHF-only, not all-Cervino.

Interpretation:
- `kDevCervino` is a misnomer by the file's own definition of
  Cervino lineage. `kDevUHF` correctly describes the bitmask.
  The two are intentional aliases, but the wrong-named one is
  the more frequently used at call sites.

Judgement:
- `kDevCervino` is a misnomer; `kDevUHF` is correct.

Proposals:
- Replace all uses of `kDevCervino` with `kDevUHF` and remove
  the `kDevCervino` symbol (medium).
- keep both (low) — if there is value in preserving the
  reconstruction history.

Cross-reference:
- Related to `kDevAllButUHF` finding above (same UHF/Cervino
  ambiguity).

Locations consulted:
- declared: include/zhinst/types.hpp:54-55
- used: src/custom_functions_io.cpp:698,765,1861,2226,2315,2379;
  (kDevUHF: 0 uses)

### `kDevHirzelAll`  [yes / medium / —]

Evidence:
- `types.hpp:58`: `kDevHirzelAll = 0x1F7` with comment
  "all except UHFQA (= kDevHirzel | UHFLI)".
- `custom_functions_io.cpp:340,366` and 4 other sites use it to
  gate `setID`, `assignWaveIndex`, etc.

Interpretation:
- The name says "Hirzel All" but the value adds UHFLI (a Cervino
  device per the preamble) to the Hirzel mask. The qualifier
  "All" is misleading because it does not mean "all Hirzel
  devices" — it means "Hirzel ∪ {UHFLI}".

Judgement:
- Misnomer — "All" suggests a closure operation that doesn't
  match the value.

Proposals:
- `kDevHirzelPlusUHFLI` (medium) — matches the actual contents
  and follows the same naming pattern as the existing
  `kDevHirzelPlusUHFQA`.
- keep current (low).

Locations consulted:
- declared: include/zhinst/types.hpp:58
- used: src/custom_functions_io.cpp:340,366 + 4 others

### kSuser* dead constants (RTLoggerReset, RTLoggerResetHdawg, UserRegBase, QAResultLen, SweepStartLo/Hi, SweepStepLo/Hi)  [unsure / low / —]

Evidence:
- `grep -rn '\bkSuser(RTLoggerReset|RTLoggerResetHdawg|UserRegBase|QAResultLen|SweepStartLo|SweepStartHi|SweepStepLo|SweepStepHi)\b' src/ include/`
  → 0 hits outside declaration.
- All have descriptive comments tying them to known binary
  addresses or functions (e.g. "User register base
  (getUserReg/setUserReg)", "RT logger timestamp reset (HDAWG)").

Interpretation:
- These constants document hardware register addresses but no
  reconstructed code path consumes them yet. Their *names* are
  consistent with the rest of the kSuser* family — the question
  is whether they should exist at all until call sites are
  reconstructed.

Judgement:
- Not a name-quality issue; a usage/dead-code issue. Names
  themselves seem fine.

Proposals:
- keep current (medium) — leave as a documented register-map
  reference for future reconstruction.
- move to a documentation-only block (low) — if they will
  never be consumed.

Locations consulted:
- declared: include/zhinst/types.hpp:115,117,118,136,141,142,143,144

## 4. Symbols inspected and judged routinely fine

- `kDevSHFPlus` — name accurately describes the SHF+ family (SHFQA,
  SHFSG, SHFQC_SG, SHFLI, GHFLI, VHFLI); 4 use sites match.
- `kDevLIFamily` — value 0x1C0 = SHFLI+GHFLI+VHFLI; 3 use sites all
  for LI-family-only features (`waitOnGrid`, `waitOnSync`,
  `setInternalTrigger`).
- `kDevAll` — 0x1FF = all 9 devices; 3 use sites for universally
  supported functions.
- `kDevQA` — 0x00C = UHFQA+SHFQA; single use, name matches.
- `kDevPreSHFLI` — 0x03F = first 6 devices in declaration order;
  single use; name describes the ordering basis directly.
- `kDevHirzelPlusUHFQA` — 0x1F6, name spells out the addition
  explicitly.
- `kDevNone` — 0x0; matches "always unsupported" idiom in
  `prefetchIndexed`.
- `kSuserNodeTag/Addr/Value/ValueHi/SlowCommit/Commit/Direct/DirectB/FreqCommit`
  — node-write protocol register names; consistent with appendSuser
  call patterns (multi-word write low/mid/high words and commit).
- `kSuserTriggerLoad`, `kSuserTimestamp`, `kSuserNow`,
  `kSuserRTLoggerData`, `kSuserSyncA/B/Hirzel`, `kSuserWaitCycles`,
  `kSuserQAResult`, `kSuserWaitLegacy`, `kSuserSinePhase0/1`,
  `kSuserSinePhaseInc0/1`, `kSuserPrngSeed/RangeLow/RangeSpan/Value`,
  `kSuserQAWeights/QATrigger`, `kSuserSweepOscIdx/Control`,
  `kSuserWaitOnSync` — all match their use sites.
- `kAddrTrigger`, `kAddrInternalTrig` — used in `ltrig`/`strig`/
  `sinttrig` impls; values 0x22/0x23 match commented binary refs.

## 5. Coverage

- **Fully covered:** all symbols declared in
  `include/zhinst/types.hpp` (14 forward-declared types, 2 enum
  types and their members, 60+ namespace-scope constants).
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - All 14 forward-declared class/enum type names — they appear
    verbatim in the mangled symbol table of `_seqc_compiler.so`
    and are excluded by §3.
  - The two enum *type* names (`AwgDeviceType`, `EDirection`)
    likewise appear in mangled form (e.g.
    `zhinst::CustomFunctions::writeToNode(...)` parameter types
    and `N6zhinst10EDirectionE`).
  - The enum *members* and all named constants are in scope and
    are covered above.
- **Confidence note:** None of the constants appear in
  `nm --demangle` output, consistent with `inline constexpr` /
  `constexpr` having no external linkage. Type-name exclusions
  were verified against `nm` output in this session.
