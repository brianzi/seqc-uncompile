# Batch 05c part 2 — custom_functions_io (lines 2650..3433)

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 7 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 1; B2 (borderline, deferred): 3;
> B3 (already resolved during Phase D/R): 3;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

Follow-up to `05c_custom_functions_io.md`, covering the 10 methods
deferred from part 1. **Read-only scan**; only this file is edited.

## 1. Files considered

- `reconstructed/src/custom_functions_io.cpp` lines 2650–3433
  (lines 1–2649 are out of scope; covered by the part-1 report).

Symbol-table verification for the 10 methods in scope:

```
$ nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so | \
    grep -E 'CustomFunctions::(executeTableEntry|setPRNG(Seed|Range|Value)|getPRNGValue|startQA\(|resetRTLoggerTimestamp|configFreqSweep|setSweepStep|setOscFreq|configureFeedbackProcessing)\('
0000000000150900 t zhinst::CustomFunctions::executeTableEntry(...)
00000000001513e0 t zhinst::CustomFunctions::setPRNGSeed(...)
0000000000151a70 t zhinst::CustomFunctions::getPRNGValue(...)
0000000000151ce0 t zhinst::CustomFunctions::setPRNGRange(...)
0000000000152690 t zhinst::CustomFunctions::startQA(...)
0000000000153f90 t zhinst::CustomFunctions::resetRTLoggerTimestamp(...)
0000000000154240 t zhinst::CustomFunctions::configFreqSweep(...)
0000000000155640 t zhinst::CustomFunctions::setSweepStep(...)
0000000000156a70 t zhinst::CustomFunctions::setOscFreq(...)
0000000000157e60 t zhinst::CustomFunctions::configureFeedbackProcessing(...)
```

All 10 method names are tier-1 authoritative — **excluded from rename
per RULES §3**.

Helper free/method names also confirmed in nm:

- `CustomFunctions::writeLS64bit(unsigned long, int, int, …)` @0x16dbb0
- `CustomFunctions::addWaitCycles(int, …)` @0x16d8c0
- `NodeMap::toFrequency(double, double)` @0x1c5630
- `CustomFunctions::getSampleClock() const` @0x16ba80

— excluded from rename. Their *parameter names* live in their own
TUs (out of scope for this batch).

`kSuser*`, `kDev*` constants and DeviceConstants member names
(`numOutputPorts`, `numDIOBits`, `awgIndex`, etc.) do **not** appear
in `nm`; they remain in scope but their declarations live in
`include/zhinst/types.hpp` and `include/zhinst/device_constants.hpp`
respectively. Concerns about those declarations are recorded as
**cross-batch** (batch 31 device_constants, batch 27 node_map_data).
This batch only flags *use-sites* and notes the misuse pattern; the
declarations themselves are the responsibility of those batches.

## 2. Overview

Conventions established in part 1 still apply and are not repeated:
the `args` / `res` parameter pair (`high / not-misnomer`),
`<*>::reg`, `<*>::zero`, `<*>::asmEntry`, `<*>::addiEntries`,
`<*>::regNum`, `<*>::results` scaffolding (routine), `appendSuser`
helper (not-misnomer).

Grouping by method, in source order. Per method: params → locals.

| Symbol | Misnomer? | Conf | Justification (≤5w) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `executeTableEntry::args/res` | no | high | Dispatch-contract pair (part 1) | keep current | not-misnomer |
| `executeTableEntry::arg0` | no | high | First (only) argument copy | keep current | not-misnomer |
| `executeTableEntry::argType` | no | high | int copy of varType_ | keep current | not-misnomer |
| `executeTableEntry::constMatched` | no | high | Tracks ZSync/QA const hit | keep current | not-misnomer |
| `executeTableEntry::tableIndex` | no | high | int from arg0 in const path | keep current | not-misnomer |
| `executeTableEntry::zsyncRaw/zsyncProcA/zsyncProcB/qaRaw/qaProcD` | no | high | EvalResultValues from named consts | keep current | not-misnomer |
| `executeTableEntry::asmEntry` | no | medium | Single-emit AsmList::Asm | keep current | — |
| `executeTableEntry::entryIndex` | no | high | Numeric path: direct table idx | keep current | not-misnomer |
| `executeTableEntry` (use of `numOutputPorts`) | unsure | medium | Used as bit-position, not "ports" | cross-batch flag (see §3) | cross-batch-arbitration |
| `setPRNGSeed::argType` | no | high | int copy of varType_ | keep current | not-misnomer |
| `setPRNGSeed::dval` | no | medium | double range-checked branch | keep current | — |
| `setPRNGSeed::seedReg` / `seedVal` | no | high | Names match role exactly | keep current | not-misnomer |
| `setPRNGSeed` integer-literal path passes `AsmRegister(args[0].value_.toInt())` to `appendSuser` | yes | medium | Treats an integer value as a register number | cross-batch flag (see §3) | cross-batch-arbitration |
| `getPRNGValue::reg` / `regNum` / `asmEntry` | no | medium | Routine | keep current | — |
| `setPRNGRange::val0` / `val1` | yes | low | Read twice; same as rangeMin/rangeMax | drop val0/val1, hoist range vars | — |
| `setPRNGRange::rangeMin` / `rangeMax` | no | high | Names match role | keep current | not-misnomer |
| `setPRNGRange::span` | no | high | rangeMax-rangeMin+1 | keep current | not-misnomer |
| `setPRNGRange::reg` / `zero` / `addiEntries` | no | medium | Routine | keep current | — |
| `startQA::maxArgs` | no | high | Per-device max-arg count | keep current | not-misnomer |
| `startQA::integrationWeightsMask` | no | high | SHFQA arg0 weights mask | keep current | not-misnomer |
| `startQA::qaIntAll` | no | high | Initially const, then user mask | keep current | not-misnomer |
| `startQA::monitorEnable` | no | high | Bit 31 in composite | keep current | not-misnomer |
| `startQA::resultLengthShift` | unsure | medium | UHFQA path uses for `resultAddr` immediate, not as length-shift | see §3 | — |
| `startQA::resultAddr` | no | high | Result-address byte | keep current | not-misnomer |
| `startQA::qaGenAllEnabled` | no | high | Bit-30 derived flag | keep current | not-misnomer |
| `startQA::qaGenAllErv` / `qaGenAll` / `qaIntAllErv` | no | high | EvalResultValue + int from named const | keep current | not-misnomer |
| `startQA::intTrigIdx` / `monIdx` / `rlsIdx` / `raIdx` | no | medium | Per-device arg-slot indices | keep current | — |
| `startQA::intTrigMask` | no | high | Validated against qaIntAll | keep current | not-misnomer |
| `startQA::imm` | no | medium | Composite immediate; matches part-1 pattern | keep current | — |
| `startQA::composite` (UHFQA branch) | no | high | Composite int for addi32 | keep current | not-misnomer |
| `startQA::zeroEntries` | no | medium | Pre-sid zero-init addi | keep current | — |
| `startQA::addi32Entries` | no | high | Distinguishes addi32 from addi | keep current | not-misnomer |
| `startQA` UHFQA second register block uses `resultAddr` again | yes | medium | Comment at :3009 calls out the dual use; not a name issue but a logic one | (not a rename — see §3) | — |
| `resetRTLoggerTimestamp::reg` / `asmEntry` | no | medium | Routine | keep current | — |
| `resetRTLoggerTimestamp::addr` | no | high | unsigned int address constant | keep current | not-misnomer |
| `resetRTLoggerTimestamp` magic numbers `0x62`/`0x6d` | yes | medium | Should use named SuserAddr constants | named constants `SuserAddr::*` (medium) | — |
| `configFreqSweep::arg0` | yes | low | Documented "oscillator index"; name doesn't say so | `oscArg`, keep current | — |
| `configFreqSweep::arg1` | yes | low | "start frequency" | `startFreqArg` | — |
| `configFreqSweep::arg2` | yes | low | "step frequency (or end frequency)" | `stepFreqArg` | — |
| `configFreqSweep::oscIndex` (double) | yes | medium | double of oscillator index, shadowed later by `oscIntVal` | unify on one name; type-suspicion: should be int | — |
| `configFreqSweep::startFreq` / `stepFreq` | no | high | Names match | keep current | not-misnomer |
| `configFreqSweep::sampleClock` / `sampleClock2` | no | medium | Two reads of the same value; `2` suffix is noise | drop `sampleClock2`, reuse | — |
| `configFreqSweep::startFreqEncoded` / `stepFreqEncoded` | no | high | uint64_t result of toFrequency | keep current | not-misnomer |
| `configFreqSweep::regNum` / `reg` / `r0` / `oscIntVal` / `addiEntries` / `suserEntry` | no | medium | Routine | keep current | — |
| `configFreqSweep::dt` / `nodePath` / `awgIdx` / `nodeItem` | no | high | Same convention as part-1 functions | keep current | not-misnomer |
| `configFreqSweep` use of `devConst_->numDIOBits` as osc bound | yes | high | Using "DIO bits" as oscillator-count bound | cross-batch flag (see §3) | cross-batch-arbitration |
| `setSweepStep::arg0` / `arg1` | yes | low | Documented but generic names | `oscArg`, `stepArg` | — |
| `setSweepStep::oscIndex` (double) | yes | medium | Same shadow pattern as configFreqSweep | unify | coordinated-rename |
| `setSweepStep::stepVal` / `stepInt` | no | high | Float guarded then int for addi | keep current | not-misnomer |
| `setSweepStep::reg1`/`reg2`/`regNum1`/`regNum2`/`r0` | no | medium | Routine register pair | keep current | — |
| `setSweepStep::oscIntVal` | no | high | int for addi immediate | keep current | not-misnomer |
| `setSweepStep::addiEntries` / `addiEntries2` / `suserEntry` / `suserEntry1` / `suserEntry2` | no | medium | Routine | keep current | — |
| `setSweepStep::dt` / `nodePath` / `awgIdx` / `nodeItem` | no | high | Same as configFreqSweep | keep current | not-misnomer |
| `setOscFreq::arg0` / `arg1` | yes | low | "oscillator index" / "frequency" | `oscArg`, `freqArg` | — |
| `setOscFreq::oscIndex` (double) | yes | medium | Same shadow as siblings | unify | coordinated-rename |
| `setOscFreq::freq` / `sampleClock` / `freqEncoded` | no | high | Names match | keep current | not-misnomer |
| `setOscFreq::reg1`/`reg2`/`regNum1`/`regNum2`/`r0` / `addiEntries` / `addiEntries2` / `suserEntry1` / `suserEntry2` | no | medium | Routine | keep current | — |
| `setOscFreq` first suser uses literal `0x8d` not `kSuserSweepControl` | yes | medium | Inconsistent with sibling `setSweepStep` | use `kSuserSweepControl` | — |
| `setOscFreq` writeLS64bit uses `0x8e, 0x8f` literals | yes | medium | Inconsistent; constants exist | use `kSuserSweepStartLo/Hi` | — |
| `configFreqSweep` writeLS64bit uses `0x8e/0x8f` and `0x90/0x91` literals | yes | medium | Constants exist | use `kSuserSweepStart*` / `kSuserSweepStep*` | coordinated-rename |
| `configFreqSweep` suser literal `0x8c` | yes | medium | Constant exists | use `kSuserSweepOscIdx` | coordinated-rename |
| `configureFeedbackProcessing::arg0` / `arg1` / `arg2` / `arg3` | yes | medium | Documented as source/shift/numBits/threshold; names hide the role | `sourceArg` / `shiftArg` / `numBitsArg` / `thresholdArg` | — |
| `configureFeedbackProcessing::shift` | yes | medium | Holds `1 << numOutputPorts`, used as additive offset for source IDs — not a "shift" amount | `srcBase`, `sourceMask` | — |
| `configureFeedbackProcessing::src1` / `src2` / `src3` | no | medium | Valid source IDs | keep current | — |
| `configureFeedbackProcessing::validSources` | no | high | unordered_set of valid source IDs | keep current | not-misnomer |
| `configureFeedbackProcessing::sourceVal` / `shiftVal` / `numBitsVal` / `thresholdVal` | no | high | Names match | keep current | not-misnomer |
| `configureFeedbackProcessing::sourceToMode` | no | high | unordered_map<int,int> source→mode | keep current | not-misnomer |
| `configureFeedbackProcessing::mode` | no | high | int mode value | keep current | not-misnomer |
| `configureFeedbackProcessing::encoded` | no | high | Composite immediate | keep current | not-misnomer |
| `configureFeedbackProcessing::fbEntry` | no | medium | Routine | keep current | — |
| `configureFeedbackProcessing` use of `0x20` literal for SHFSG | yes | low | Should use `AwgDeviceType::SHFSG` symbolic | symbolic enum (low) | — |

## 3. Detailed findings

### Use of `devConst_->numDIOBits` as oscillator-index upper bound  [yes / high / cross-batch-arbitration]

Evidence:
- src/custom_functions_io.cpp:3068 (configFreqSweep) `if (oscIndex >
  static_cast<double>(devConst_->numDIOBits - 1)) throw …`
- src/custom_functions_io.cpp:3155 (setSweepStep) same
- src/custom_functions_io.cpp:3260 (setOscFreq) same
- include/zhinst/device_constants.hpp:91 / :184 — field at +0x84
  documented as `numDIOBits  // values: 0, 6, or 8`.
- The error here is `InvalidArgValue` (0) for the *oscillator index*
  argument; `arg0` in all three methods is documented as the
  oscillator index.
- Devices with frequency sweeps (SHFQA/SHFSG/SHFQC_SG/HDAWG/SHFLI/
  GHFLI/VHFLI) all carry oscillator counts of 8, but the field is
  also used in DIO contexts elsewhere in the codebase (see batch 31).

Interpretation:
- Either (a) `devConst_->numDIOBits` is being misused as an
  oscillator-count bound by these three methods (logic bug — wrong
  field), or (b) the field at +0x84 is misnamed and is actually
  something like `numOscillators` (or "small per-channel bound" used
  for both DIO bits and oscillator count, which would coincidentally
  share the value 8 for SHF* devices). The name `numDIOBits` is
  documented as values 0/6/8, while oscillator counts are typically
  8 on SHF and 8 on UHF — partial overlap but not identical.
- The arbitration cannot be settled in this batch without seeing all
  use-sites of the field across the binary.

Judgement:
- Inconsistency is the high-confidence finding. Whether the name or
  the use is wrong is for batch 31 (device_constants) to settle.

Proposals:
- Cross-batch arbitration with batch 31 (device_constants).
- If the field name is correct: these three methods have a logic
  error that should be promoted to a TODO post-synthesis.
- If the field is misnamed: rename the field (e.g.
  `numOscillatorsOrDIO`, or split into two fields if disassembly
  shows two distinct uses); these methods then become fine.

Cross-reference:
- `DeviceConstants::numDIOBits` declaration — batch 31
  (device_constants).

Locations consulted:
- src/custom_functions_io.cpp:3068, 3155, 3260.
- include/zhinst/device_constants.hpp:91, 184.

### Use of `devConst_->numOutputPorts` as bit-shift amount  [unsure / medium / cross-batch-arbitration]

Evidence:
- src/custom_functions_io.cpp:2690 `1 << devConst_->numOutputPorts`
  — used as an immediate for `wvft` (table-entry shifted constant).
- :2698 `9 << devConst_->numOutputPorts`
- :2706 `0xd << devConst_->numOutputPorts`
- :2714 `0xe << devConst_->numOutputPorts`
- :2721 `0x10 << devConst_->numOutputPorts`
- :2738 `1 << (devConst_->numOutputPorts + 1)`
- :2746 `(entryIndex >> devConst_->numOutputPorts) != 0` — index
  validated as fitting in `numOutputPorts` bits.
- :3359 (configureFeedbackProcessing) `int shift = 1 <<
  static_cast<int>(devConst_->numOutputPorts);` — used as an
  *additive base* for source IDs, not as a shift count.
- include/zhinst/device_constants.hpp:87 / :180 — field at +0x78
  documented as `numOutputPorts  // 0, 10, or 12`.

Interpretation:
- The field is used here as a *bit-position*: the boundary between
  the table-entry-index field and the constant-mode field of the
  `wvft` immediate, and as the source-ID offset in the feedback
  encoding. Numerically the values 10/12 fit ("shift by 12 → mask
  bit 0x1000"), but the **role** at every use site in this file is
  a bit-position, not an output-port count.
- Either (a) the field name in DeviceConstants is correct and these
  arithmetic uses are repurposing it (which would itself be
  surprising — it would be a coincidence that `numOutputPorts`
  happens to equal the bit-shift required for the wvft layout), or
  (b) the field name is misleading.

Judgement:
- Inconsistency between name and use is medium-confidence; the
  resolution belongs to batch 31. Use-sites in this file have no
  better names without the upstream decision.

Proposals:
- Cross-batch arbitration with batch 31. Candidate alternative
  names for the field: `tableEntryShift`, `feedbackSourceShift`.
- Locally in this file: keep current uses; do not rename use-sites
  before the field is settled.

Cross-reference:
- `DeviceConstants::numOutputPorts` — batch 31 (device_constants).

Locations consulted:
- src/custom_functions_io.cpp:2690, 2698, 2706, 2714, 2721, 2738,
  2746, 3359.
- include/zhinst/device_constants.hpp:87, 180.

### `setPRNGSeed` integer-literal path: `AsmRegister(args[0].value_.toInt())`  [yes / medium / cross-batch-arbitration]

Evidence:
- src/custom_functions_io.cpp:2772–2774
  ```
  if (argType == 2) {
      // Integer literal path …
      appendSuser(results->assemblers_, asmCommands_,
                  AsmRegister(args[0].value_.toInt()),
                  detail::AddressImpl<unsigned int>(kSuserPrngSeed));
  ```
- `argType == 2` is the **register** (`VarType_Var`) branch in every
  *other* method in this file (see e.g. setSweepStep:3161 where
  the same check picks `arg1.reg_`); the float/numeric branch is
  `(argType & ~2) == 4` (:2775).
- Yet here the comment says "Integer literal path — emit suser
  directly with value as immediate", and the code constructs an
  `AsmRegister` from the *value* (an integer), not from the
  pre-existing register field `args[0].reg_`.

Interpretation:
- One of three things is true:
  1. The branch comment is wrong and this *is* the register branch;
     the source should use `args[0].reg_`, not `AsmRegister(value_
     .toInt())`. (Most likely — matches sibling methods.)
  2. `varType_ == 2` is overloaded and in this method only it means
     "integer literal" (very unlikely; would conflict with every
     other dispatch in the file).
  3. `AsmRegister` has an integer-immediate constructor that is
     intentional (i.e. "use this integer as if it were a register
     number"). Possible but inconsistent with sibling sites.
- This is not a *naming* defect of any one symbol, but of the
  branch comment + a likely logic error. The naming sin is that the
  comment "Integer literal path" mislabels the branch, and the
  reader is invited to believe `varType_ == 2` means "literal".

Judgement:
- Misleading comment + likely logic bug. Not a single-symbol
  rename; promotes to a TODO at synthesis.

Proposals:
- Replace `AsmRegister(args[0].value_.toInt())` with `args[0].reg_`
  (high — pending GDB verification).
- Rewrite the branch comment to "Register path" (high).
- Cross-batch arbitration: confirm the `varType_==2` convention
  with batch 01 (types).

Cross-reference:
- `VarType_Var` semantics — batch 01 (types).
- `EvalResultValue::reg_` — batch 42 (expression).

Locations consulted:
- src/custom_functions_io.cpp:2772–2774.
- src/custom_functions_io.cpp:2775–2796 (numeric path that uses
  `args[0].value_.toInt()` correctly, after range-checks).
- src/custom_functions_io.cpp:3161 (sibling method, register path
  pattern).

### `setPRNGRange::val0` / `val1` vs `rangeMin` / `rangeMax`  [yes / low / —]

Evidence:
- src/custom_functions_io.cpp:2829 `int val0 = args[0].value_.toInt();`
- :2833 `int val1 = args[1].value_.toInt();`
- :2845 `int rangeMin = args[0].value_.toInt();`
- :2846 `int rangeMax = args[1].value_.toInt();`
- :2851 `rangeMin = args[0].value_.toInt();`  (re-read)
- :2852 `rangeMax = args[1].value_.toInt();`  (re-read)

Interpretation:
- Three reads of each argument, only two distinct local-variable
  names. `val0`/`val1` exist only to gate the range-check throws on
  lines 2829–2843. The reconstructed source preserves the binary's
  redundant re-reads.

Judgement:
- `val0`/`val1` are mildly misleading: they suggest a different
  value than `rangeMin`/`rangeMax` but they are the same.

Proposals:
- Drop `val0`/`val1`, hoist `rangeMin`/`rangeMax` to the top, drop
  the duplicate re-reads (medium — cosmetic, also collapses dead
  code).
- Keep current (low) if the redundant reads must mirror the binary.

Locations consulted:
- src/custom_functions_io.cpp:2829–2852.

### `startQA::resultLengthShift` (UHFQA branch) and `startQA::resultAddr` second-register comment  [unsure / medium / —]

Evidence:
- src/custom_functions_io.cpp:2903 `int resultLengthShift = 0;`
- :2948 `resultLengthShift = args[rlsIdx].value_.toInt();`
- :2981 (SHFQA composite) `int imm = (resultLengthShift << 22) |
  qaIntAll | (monitorEnable << 31) | (qaGenAllEnabled ? (1<<30):0);`
- :2995 (UHFQA composite) `int composite = (qaIntAll << 16) |
  ((qaIntAll != 0 ? 1 : 0) << 4) | (monitorEnable << 5) |
  resultAddr;`  — `resultLengthShift` does **not** appear here.
- :3009–:3013 explicit comment "Binary @0x153889: loads [rbp-0x11c]
  (= resultAddr, NOT resultLengthShift)" then `addi(reg, zero,
  Immediate(resultAddr));`

Interpretation:
- The reconstructor was uncertain whether the UHFQA second-register
  immediate was `resultAddr` or `resultLengthShift`; the comment
  records that the binary loads what the reconstruction calls
  `resultAddr`. If the labelling is correct, fine; but the comment
  itself raises a name-suspicion that the meaning of these two
  locals may be swapped on the UHFQA path. The SHFQA path uses
  `resultLengthShift << 22` which is plausible "shift amount"; the
  UHFQA path uses `resultAddr` as a low-byte immediate which is
  plausible "address".
- The names match the SHFQA composite. No evidence in this batch
  contradicts them. The comment at :3009 is a recon-time warning,
  not a renaming flag.

Judgement:
- Names plausibly correct; uncertainty inherited from the recon
  comment. Not a confident misnomer.

Proposals:
- Keep current (medium).
- If GDB confirms different ABI-slot mapping, revisit (low).

Locations consulted:
- src/custom_functions_io.cpp:2903, 2948, 2981, 2995, 3009–3013.

### `resetRTLoggerTimestamp` magic addresses `0x62` / `0x6d`  [yes / medium / —]

Evidence:
- src/custom_functions_io.cpp:3031 `unsigned int addr =
  (config_->deviceType == static_cast<AwgDeviceType>(4)) ? 0x62u
  : 0x6du;`
- include/zhinst/device_constants.hpp does not define `0x62` or
  `0x6d` in `SuserAddr` — the inventory in part 1 listed
  `kSuser*` constants for many adjacent values but these two are
  not present, while `0x6E SyncHirzel` and `0x6F WaitLegacy` are.
- The variable is used at :3032 as an address for `st(reg,
  AddressImpl<unsigned int>(addr))`.

Interpretation:
- `addr` is a meaningful local; the issue is the absence of named
  constants for the two values, which makes the device-type
  conditional opaque. This is a constants-table omission, not a
  rename of `addr` itself.

Judgement:
- `addr` is fine. The two literals should be named.

Proposals:
- Add named constants (e.g. `kRTLoggerResetUHFQA = 0x62`,
  `kRTLoggerResetHirzel = 0x6d`) in `types.hpp` — cross-batch
  with batch 01 (types) / batch 31 (device_constants).
- `addr` itself: keep current (high).

Cross-reference:
- `SuserAddr` table — batch 31 (device_constants).

Locations consulted:
- src/custom_functions_io.cpp:3022–3034.
- include/zhinst/device_constants.hpp:107–140 (SuserAddr table).
- include/zhinst/types.hpp:108–144 (kSuser* family).

### `configFreqSweep::oscIndex` / `setSweepStep::oscIndex` / `setOscFreq::oscIndex` (double)  [yes / medium / coordinated-rename]

Evidence:
- src/custom_functions_io.cpp:3063 `double oscIndex = arg0.value_.toDouble();`
- :3098 `int oscIntVal = arg0.value_.toInt();`  — same value, int form,
  used a few lines later.
- :3150 / :3190 (setSweepStep) same pattern — `double oscIndex` then
  `int oscIntVal`.
- :3255 / :3291 (setOscFreq) same pattern.
- The double form is used **only** for the negative/upper-bound
  range check. Every later use is the int form.

Interpretation:
- Two locals carry the same logical value in two types. The double
  variant exists so the upper-bound check `oscIndex >
  numDIOBits-1` compares safely against a possibly-fractional input.
- The names are not actively misleading — they say what the value
  is — but the *pattern* of having `oscIndex` (double) and
  `oscIntVal` (int) for the same field is repeated three times
  unchanged. A single name (e.g. drop `oscIndex` → `oscIndexD` or
  hoist the int and use only `static_cast<double>(oscIntVal)` in
  the check) would be cleaner. Type-suspicion: the int form is the
  "real" oscillator index; the double exists only to support the
  range-check semantics.

Judgement:
- Mildly misleading by repetition; coordinated rename across the
  three methods would help.

Proposals:
- Rename the doubles to `oscIndexD` (low), or replace the double
  with `static_cast<double>(oscIntVal)` and drop the local
  altogether (medium).
- Keep current (low).

Locations consulted:
- src/custom_functions_io.cpp:3063 / 3098 (configFreqSweep).
- src/custom_functions_io.cpp:3150 / 3190 (setSweepStep).
- src/custom_functions_io.cpp:3255 / 3291 (setOscFreq).

### Use of literal suser addresses where named constants exist  [yes / medium / coordinated-rename]

Evidence:
- src/custom_functions_io.cpp:3085 `writeLS64bit(startFreqEncoded,
  0x8e, 0x8f, results, res);` — should be `kSuserSweepStartLo,
  kSuserSweepStartHi`.
- :3092 `writeLS64bit(stepFreqEncoded, 0x90, 0x91, …);` — should be
  `kSuserSweepStepLo, kSuserSweepStepHi`.
- :3102 `asmCommands_->suser(reg, 0x8c);` — should be
  `kSuserSweepOscIdx`.
- :3275 `asmCommands_->suser(reg1, 0x8d);` — should be
  `kSuserSweepControl`. (sister method `setSweepStep` at :3163 and
  :3182 already uses `kSuserSweepControl`, confirming the
  inconsistency.)
- :3284 `writeLS64bit(freqEncoded, 0x8e, 0x8f, …);` — should be
  `kSuserSweepStartLo, kSuserSweepStartHi`.
- include/zhinst/types.hpp:139–144 — all six constants are defined
  there.

Interpretation:
- The constants exist (verified in types.hpp) and are used by
  `setSweepStep` consistently. The other two methods regress to
  raw hex. This is a use-site cleanup, not a naming change of any
  single symbol.

Judgement:
- The literal usages are misleading by inconsistency; a coordinated
  rename of the literals to the existing constants is appropriate.

Proposals:
- Replace literals with `kSuserSweep*` constants (medium).
- Coordinated rename across configFreqSweep + setOscFreq.

Cross-reference:
- `kSuserSweep*` declarations — batch 01 (types) / batch 31
  (device_constants), already established.

Locations consulted:
- src/custom_functions_io.cpp:3085, 3092, 3102, 3275, 3284, 3298.
- src/custom_functions_io.cpp:3163, 3182, 3199 (sibling correct
  uses).
- include/zhinst/types.hpp:139–144.

### `configureFeedbackProcessing::shift`  [yes / medium / —]

Evidence:
- src/custom_functions_io.cpp:3359 `int shift = 1 << static_cast<int>(
  devConst_->numOutputPorts);`
- :3360 `int src1 = shift + 1;`
- :3361 `int src2 = shift + 2;`
- :3369 `int src3 = shift + 4;`
- :3411 `sourceToMode[shift + 4] = 2;`
- :3422 `int encoded = ((mode & 0x3) << 21) | ((shiftVal & 0x1f) <<
  16) | …`  — note that `shiftVal` (arg1) is the actual *shift
  amount* in the encoding; `shift` (the local at :3359) is not.

Interpretation:
- The local `shift` is used as an additive base for source IDs (a
  base address / bitmask), not as a bit-shift count. The unrelated
  `shiftVal` (arg1) really is the shift amount in the final
  encoding. Two locals with similar names, opposite roles.

Judgement:
- Misleading. `shift` is a base/mask, not a shift.

Proposals:
- `srcBase` (medium), `sourceMask` (low), `sourceIdBase` (low).
- Keep current (low).

Locations consulted:
- src/custom_functions_io.cpp:3358–3411, 3422.

### `configureFeedbackProcessing::arg0..arg3`  [yes / medium / —]

Evidence:
- src/custom_functions_io.cpp:3344–3347
  ```
  auto const& arg0 = args[0];  // source (feedback source index)
  auto const& arg1 = args[1];  // shift
  auto const& arg2 = args[2];  // number of bits
  auto const& arg3 = args[3];  // threshold
  ```
- All four locals carry inline comments documenting their roles.
  The validation throws use `InvalidArgValue, 0..3` (arg index in
  the error), so the slot is meaningful.

Interpretation:
- Comments themselves are evidence the names are placeholders. The
  same-file siblings `configFreqSweep`/`setSweepStep`/`setOscFreq`
  use the same `arg0`/`arg1` pattern with similar comments — this
  is a consistent codebase pattern (part-1 report flagged some of
  these too, e.g. `configFreqSweep::arg2` documented "step
  frequency (or end frequency)"). The pattern is uniform but
  uniformly placeholder-ish.

Judgement:
- Mildly misleading; per-arg names would aid readability and align
  the four-validator block with the four named values it produces
  (`sourceVal`, `shiftVal`, `numBitsVal`, `thresholdVal`).

Proposals:
- `sourceArg`, `shiftArg`, `numBitsArg`, `thresholdArg` (medium).
- Keep current (low) — consistent with sibling methods.

Locations consulted:
- src/custom_functions_io.cpp:3344–3402.

### `executeTableEntry::asmEntry` (3 emit-sites)  [no / medium / not-misnomer]

Evidence:
- src/custom_functions_io.cpp:2689, 2697, 2705, 2713, 2720, 2737,
  2749 — each block constructs a single `asmEntry` from a `wvft`
  call, then `push_back(std::move(asmEntry))`. Convention used
  throughout part-1 methods (e.g. setDIO, getQAResult).

Interpretation:
- Standard one-shot AsmList::Asm scaffolding; matches the rest of
  the file's convention.

Judgement:
- Name is correct.

### `<method>::nodePath` / `nodeItem` / `awgIdx` (configFreqSweep, setSweepStep, setOscFreq)  [no / high / not-misnomer]

Evidence:
- src/custom_functions_io.cpp:3110–3122 (configFreqSweep).
- :3207–:3227 (setSweepStep).
- :3307–:3327 (setOscFreq).
- The node path strings (`"oscs/<n>/freq"`,
  `"sgchannels/<n>/oscs/<n>/freq"`,
  `"qachannels/<n>/oscs/<n>/freq"`,
  `"generators/<n>/oscs/<n>/freq"`) are present verbatim and match
  documented Zurich Instruments node paths.

Interpretation:
- `nodePath`, `nodeItem`, `awgIdx` are accurate descriptions of
  what they hold. `lookupNode` and `addNodeAccess` are confirmed
  in `nm` (out-of-scope method names; cross-ref to batch 20 / 27).

Judgement:
- Correct names.

Cross-reference:
- `lookupNode` / `addNodeAccess` / `NodeMap` / `NodeMapItem` —
  batches 20 (node) / 27 (node_map_data).

## 4. Symbols inspected and judged routinely fine

(One bullet each; ≤ 1 sentence.)

- `executeTableEntry::arg0`, `argType`, `tableIndex`, `entryIndex`,
  `constMatched` — all role-clear.
- `executeTableEntry::zsyncRaw`, `zsyncProcA`, `zsyncProcB`,
  `qaRaw`, `qaProcD` — EvalResultValues from named consts; names
  match the const strings.
- `setPRNGSeed::argType`, `dval`, `regNum`, `seedReg`, `seedVal`,
  `zero`, `addiEntries` — routine.
- `getPRNGValue::regNum`, `reg`, `asmEntry` — routine; method is
  trivial.
- `setPRNGRange::regNum`, `reg`, `zero`, `addiEntries`, `span` —
  routine; `span` matches the arithmetic `rangeMax-rangeMin+1`.
- `startQA::maxArgs`, `qaGenAllErv`, `qaGenAll`, `qaIntAllErv`,
  `qaIntAll`, `intTrigIdx`, `intTrigMask`, `monIdx`, `rlsIdx`,
  `raIdx`, `monitorEnable`, `resultAddr`, `qaGenAllEnabled`,
  `integrationWeightsMask`, `zero`, `regNum`, `reg`, `imm`,
  `composite`, `addiEntries`, `addi32Entries`, `zeroEntries` —
  all roles match.
- `resetRTLoggerTimestamp::reg`, `addr`, `asmEntry` — routine
  (the magic constants are flagged separately above).
- `configFreqSweep::startFreq`, `stepFreq`, `startFreqEncoded`,
  `stepFreqEncoded`, `regNum`, `reg`, `r0`, `oscIntVal`,
  `addiEntries`, `suserEntry`, `dt`, `nodePath`, `awgIdx`,
  `nodeItem`, `sampleClock` — routine. (`sampleClock2` is the only
  noise; flagged in §2.)
- `setSweepStep::stepVal`, `stepInt`, `regNum1`, `regNum2`, `reg1`,
  `reg2`, `r0`, `oscIntVal`, `addiEntries`, `addiEntries2`,
  `suserEntry`, `suserEntry1`, `suserEntry2`, `dt`, `nodePath`,
  `awgIdx`, `nodeItem` — routine.
- `setOscFreq::freq`, `sampleClock`, `freqEncoded`, `regNum1`,
  `regNum2`, `reg1`, `reg2`, `r0`, `oscIntVal`, `addiEntries`,
  `addiEntries2`, `suserEntry1`, `suserEntry2`, `dt`, `nodePath`,
  `awgIdx`, `nodeItem` — routine.
- `configureFeedbackProcessing::src1`, `src2`, `src3`,
  `validSources`, `sourceVal`, `shiftVal`, `numBitsVal`,
  `thresholdVal`, `sourceToMode`, `mode`, `encoded`, `fbEntry` —
  all roles match.
- All `kSuser*` and `kDev*` *uses* in this range — verified
  against the declarations in `types.hpp`. (The opportunities to
  *replace* literals with these constants are flagged separately.)
- `VarType_Void`, `VarType_Var`, `EDirection::eOUT`, `AccessMode(2)`,
  `AwgDeviceType::SHFLI/GHFLI/VHFLI/SHFQC_SG`, error-message enum
  ids (`FuncMinArgs`, `FuncExpectsConst`, `FuncExpectsConstVar`,
  `FuncExpectsNoArgs`, `FuncExpectsNArgs`, `FuncExpectsMaxArgs`,
  `FuncExpectsSingleArg`, `GetUserRegArgs`, `InvalidArgValue`,
  `SetTriggerArgs`, `PrngSeedPositive`, `PrngSeedZero`,
  `PrngSeedMax`, `PrngRangeArgs`, `ExecTableInvalidIndex`,
  `ExecTableExpectsArg`) — enumerator references; declarations
  out of scope (batches 01, 08, 27, 29).
- Helper *calls* `lookupNode`, `addNodeAccess`, `writeLS64bit`,
  `addWaitCycles`, `getSampleClock`, `NodeMap::toFrequency`,
  `Resources::getRegisterNumber`, `Resources::readConst`,
  `setWaitCyclesReg`, `checkFunctionSupported`, `appendSuser`
  — function names verified in `nm` (most listed in §1).
  Parameter names belong to their TUs.

## 5. Coverage

### Fully covered (lines 2650..3433)

All 10 deferred methods scanned end-to-end:

| # | Method | Lines | Binary addr | Status |
|---|---|---|---|---|
| 47 | `executeTableEntry` | 2653–2758 | @0x150900 | ✓ |
| 48 | `setPRNGSeed` | 2759–2800 | @0x1513e0 | ✓ |
| 49 | `getPRNGValue` | 2801–2814 | @0x151a70 | ✓ |
| 50 | `setPRNGRange` | 2815–2879 | @0x151ce0 | ✓ |
| 51 | `startQA` | 2880–3021 | @0x152690 | ✓ |
| 52 | `resetRTLoggerTimestamp` | 3022–3035 | @0x153f90 | ✓ |
| 53 | `configFreqSweep` | 3036–3127 | @0x154240 | ✓ |
| 54 | `setSweepStep` | 3128–3230 | @0x155640 | ✓ |
| 55 | `setOscFreq` | 3231–3330 | @0x156a70 | ✓ |
| 56 | `configureFeedbackProcessing` | 3331–3433 | @0x157e60 | ✓ |

Combined with part 1, all 56 method definitions in
`custom_functions_io.cpp` are now covered.

### Deferred

None.

### Not covered (out of scope per RULES §2/§3)

- All 10 `CustomFunctions::<method>` *names* — present in the
  symbol table (verified via `nm` in §1). Excluded from rename.
- `CustomFunctions::writeLS64bit`, `addWaitCycles`,
  `getSampleClock`, `lookupNode`, `addNodeAccess`,
  `setWaitCyclesReg`, `checkFunctionSupported`, `isShfFamily`,
  `NodeMap::toFrequency`, `Resources::getRegisterNumber`,
  `Resources::readConst` — method/free-function names in symbol
  table; their parameter names live in their own TUs (cross-ref to
  batches 27 (node_map_data), 51 (callbacks), 42 (expression),
  and the eventual headers/dispatcher batches).
- `CustomFunctions` member fields (`asmCommands_`, `config_`,
  `devConst_`) — declared in the header `custom_functions.hpp`,
  out of scope here. Cross-batch with the header batch.
- `CustomFunctions` nested types (`SubFunc`, `PlayArgs`, etc.) —
  declared in header.
- `kSuser*`, `kDev*` *declarations* — live in `types.hpp` /
  `device_constants.hpp` (batches 01 / 31). Use-sites only are
  audited here.
- `DeviceConstants::numDIOBits`, `numOutputPorts`, `awgIndex`,
  `deviceType`, `sineNodeBase` — field *declarations* are batch 31
  (device_constants); this batch flags inconsistencies between
  declared name and observed use. **Two cross-batch arbitrations
  raised**: see §3 entries on `numDIOBits` and `numOutputPorts`.
- `AwgDeviceType` enumerators (`SHFLI`, `GHFLI`, `VHFLI`,
  `SHFQC_SG`) — declared in `types.hpp`; use sites are routine.
- `AccessMode(2)` literal — `AccessMode` enumeration declared in
  the header; the numeric `2` should likely be a named enumerator
  but that is a header-batch concern.
- `EvalResultValue` field accesses (`varType_`, `value_`, `reg_`)
  — field names declared in the header for `EvalResultValue`
  (batch 42 expression).
- All error-message enumerators referenced — batch 08
  (error_messages).

## Cross-references summary

- **Batch 31 (device_constants)** — two open arbitrations:
  - `DeviceConstants::numDIOBits` vs use as oscillator-index bound.
  - `DeviceConstants::numOutputPorts` vs use as bit-shift position.
- **Batch 01 (types)** — `kSuser*` / `kDev*` constant declarations;
  literal-to-named-constant conversions in `configFreqSweep` /
  `setOscFreq`; `VarType_Var == 2` semantics for the `setPRNGSeed`
  branch defect.
- **Batch 27 (node_map_data)** — `NodeMap::toFrequency` parameter
  names; `lookupNode` / `addNodeAccess` parameter names.
- **Batch 42 (expression)** — `EvalResultValue::reg_` /
  `value_` / `varType_` field declarations; relevant to
  `setPRNGSeed` flag.
- **Batch 51 (callbacks)** — `CustomFunctions` dispatcher;
  reaffirms `args` / `res` convention (already established).
- **Batches 10/49 (asm_commands / asm_commands_impl)** —
  `wvft`, `suser`, `addi`, `addi32`, `sid`, `strig`, `fb`, `st`,
  `luser` parameter names. No direct flag from this batch but the
  literal-suser cleanups would touch how those calls are made.
- **Batch 08 (error_messages)** — none of the error enumerators in
  this range are flagged; all match documented role.
- **Batch 20 (node)** — `addNodeAccess(nodeItem, AccessMode)`
  signature.
