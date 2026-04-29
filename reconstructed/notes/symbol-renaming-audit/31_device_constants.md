# Batch 31 — device_constants

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 5 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 3;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 2.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/device_constants.hpp`
- `reconstructed/src/device_constants.cpp`

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `DeviceConstants` (type) | no | high | type in binary symbols | keep current | not-misnomer |
| `DeviceConstants::Register` (nested type) | no | high | type in binary symbols | keep current | not-misnomer |
| `DeviceConstants::SuserAddr` (nested type) | unsure | low | not in binary; recon-invented | keep current, `SuserAddress` | — |
| `DeviceConstants::deviceType` | no | medium | matches enum role at sites | keep current | not-misnomer |
| `DeviceConstants::hasExtendedReg` | unsure | low | only set on HDAWG; never read | keep current, `isHdawg` | — |
| `DeviceConstants::waveformRegBase` | no | medium | serialized as `"bitstream"` | keep current | not-misnomer |
| `DeviceConstants::waveformMemorySize` | no | medium | sized cache; cross-batch positive | keep current | not-misnomer |
| `DeviceConstants::sampleLength` | no | high | confirmed Cache page size (b36) | keep current | not-misnomer |
| `DeviceConstants::waveformAlignment` | no | medium | used as alignment grain | keep current | not-misnomer |
| `DeviceConstants::cachePageCount` | unsure | low | meaning generic; cacheType-indexed | keep current | cross-batch-arbitration |
| `DeviceConstants::maxBlocks` | unsure | low | always 2; multiplied as factor | keep current, `cacheBlockCount` | cross-batch-arbitration |
| `DeviceConstants::sineNodeBase` | yes | medium | always 16; used in nodeIdx arith | `nodeIndexFactor`, keep current | — |
| `DeviceConstants::waveformElfAlignment` | yes | medium | always 64; misleading "ELF" | `nodeIndexBase`, `bitsPerWord` | — |
| `DeviceConstants::registerDepth` | no | medium | bound for register index | keep current | not-misnomer |
| `DeviceConstants::memoryDepth` | no | medium | bound for memory address | keep current | not-misnomer |
| `DeviceConstants::sequencerRegBase` | no | low | symmetry w/ waveformRegBase | keep current | — |
| `DeviceConstants::triggerLatencyCycles` | no | medium | subtracted as cycle count | keep current | not-misnomer |
| `DeviceConstants::waveformGranularity` | yes | high | used as a maximum, not grain | `maxWaveformLength` | coordinated-rename |
| `DeviceConstants::waveformPageSize` | unsure | medium | used as alignment grain | `grainSize`, keep current | coordinated-rename |
| `DeviceConstants::playMinSamples` | no | medium | min play length check | keep current | not-misnomer |
| `DeviceConstants::waveformMinSamples` | no | low | seqRegWidth seed; thin evidence | keep current | — |
| `DeviceConstants::bitsPerSample` | no | high | used as bit width in totalBits | keep current | not-misnomer |
| `DeviceConstants::numCounters` | no | high | bounds getCnt counter index | keep current | not-misnomer |
| `DeviceConstants::waveformMemSize` | unsure | low | unit ambiguous (samples?) | keep current, `maxWaveformCount` | — |
| `DeviceConstants::maxSequenceLen` | unsure | low | used as maxWaveIndex too | keep current | cross-batch-arbitration |
| `DeviceConstants::seqClockDivider` | unsure | medium | used as wait-cycle count | keep current, `syncWaitCycles` | — |
| `DeviceConstants::samplingRate` | no | high | Hz value, used as sample rate | keep current | not-misnomer |
| `DeviceConstants::numOutputPorts` | yes | high | used as bit-shift width | `execTableIndexBits` | — |
| `DeviceConstants::numAWGCores` | unsure | medium | written to wait-cycles addr | keep current, `syncCoreCount` | — |
| `DeviceConstants::hasDIO` | no | medium | passed as hasDIO bool | keep current | not-misnomer |
| `DeviceConstants::numDIOBits` | no | high | used as `(1<<x)-1` mask width | keep current | not-misnomer |
| `DeviceConstants::hasPrecomp` | no | medium | passed/used as flag | keep current | not-misnomer |
| `DeviceConstants::grainSize()` | no | medium | matches usage as grain | keep current | coordinated-rename |
| `DeviceConstants::maxWaveformLength()` | no | high | matches usage as max | keep current | coordinated-rename |
| `DeviceConstants::maxDioTableEntries()` | unsure | low | thin evidence | keep current | — |
| `DeviceConstants::maxWaveIndex()` | no | medium | passed to WaveIndexTracker | keep current | — |
| `getDeviceConstants` (free fn) | no | high | name in binary symbols | keep current | not-misnomer |
| `getDeviceConstants::deviceType` (param) | no | medium | switch on AwgDeviceType | keep current | — |
| `Register::SyncRegA`, `SyncRegB` (enumerators) | unsure | low | inferred from suser usage | keep current | — |
| `SuserAddr::*` (35 constants) | no | medium | match suser opcode addresses | keep current | not-misnomer |

## 3. Detailed findings

### DeviceConstants  [no / high / not-misnomer]

Evidence:
- `nm --demangle _seqc_compiler.so | grep DeviceConstants` shows the
  fully-mangled type name `zhinst::DeviceConstants` in dozens of
  symbols (e.g. `getDeviceConstants(AwgDeviceType)`,
  `Prefetch::Prefetch(..., DeviceConstants const&, ...)`,
  `Compiler::Compiler(..., DeviceConstants const&, ...)`).

Interpretation:
- The class name `DeviceConstants` is recorded verbatim in the binary
  symbol table.

Judgement:
- Per RULES §3, type names that appear in the binary symbol table are
  authoritative; this is not a misnomer.

Locations consulted:
- declared: include/zhinst/device_constants.hpp:97
- nm output above

### DeviceConstants::Register  [no / high / not-misnomer]

Evidence:
- `nm` shows `zhinst::DeviceConstants::Register::{unnamed type#7}` and
  `{unnamed type#8}` as template arguments at addresses 0x27b6f0 and
  0x27b860.

Interpretation:
- The nested type `Register` is in the binary symbol table.

Judgement:
- Type name authoritative per RULES §3; not a misnomer.

### DeviceConstants::SuserAddr  [unsure / low / —]

Evidence:
- `nm --demangle ... | grep SuserAddr` returns nothing.
- The class is referenced from `custom_functions_io.cpp` and similar
  via `kSuser*` constants (e.g. `kSuserWaitCycles` at line 2212,
  `kSuserSinePhase0` at line 1523), not via `SuserAddr::Foo`.

Interpretation:
- The wrapper struct `SuserAddr` does not appear in the binary; it
  was introduced by reconstruction as a namespace for these
  constants. The constants themselves are referenced through `k*`
  free-constant names, not via `SuserAddr::`.

Judgement:
- The type name is recon-invented; whether to keep it as a wrapper or
  inline the constants into the `zhinst` namespace is a stylistic
  call. `SuserAddress` would read more naturally as a long form.

Proposals:
- keep current  (medium)
- `SuserAddress`  (low)

Locations consulted:
- declared: include/zhinst/device_constants.hpp:107
- references: src/custom_functions_io.cpp (numerous via `kSuser*`)

### DeviceConstants::hasExtendedReg  [unsure / low / —]

Evidence:
- include/zhinst/device_constants.hpp:143 declaration; comment says
  "true for HDAWG only".
- src/device_constants.cpp lines 39, 70, 101, 132, 163, 194, 225, 256,
  287: only `case 2` (HDAWG) sets it `true`; every other arm sets
  `false`.
- `grep -rn 'hasExtendedReg' reconstructed/src` returns zero
  read-sites outside `device_constants.cpp`.

Interpretation:
- A boolean field that is `true` exactly for the HDAWG device, with
  no observed read sites in the rest of the reconstructed source.
  We have not located *what* "extended register" means.

Judgement:
- Without a use site, we cannot confirm the field really refers to an
  extended register set; it may simply be an "is HDAWG" flag.

Proposals:
- keep current  (medium)
- `isHdawg`  (low)

Locations consulted:
- declared: include/zhinst/device_constants.hpp:143
- written:  src/device_constants.cpp (every `case`)
- read:     none found

### DeviceConstants::cachePageCount, DeviceConstants::maxBlocks  [unsure / low / cross-batch-arbitration]

Evidence:
- include/zhinst/device_constants.hpp:153–154; comment says
  "cache pages (cacheType=0)" / "cache pages (cacheType=1)".
- src/awg_compiler.cpp:299–301 selects between them by `cacheType`:
  ```
  ? deviceConstants_.maxBlocks      // DC+0x1C
  : deviceConstants_.cachePageCount; // DC+0x18
  ```
- `maxBlocks` is always 2 in every device arm; `cachePageCount` is
  4, 104, 0x2000, or 0x6000.
- `maxBlocks` is also referenced as part of `sineNodeBase * maxBlocks
  + waveformElfAlignment` in `custom_functions_io.cpp:1530, 1642` —
  used as a generic factor of 2 in node-index arithmetic.

Interpretation:
- `cachePageCount` and `maxBlocks` are two parallel slots indexed by
  `cacheType`. `maxBlocks` doubles as a factor of 2 in unrelated node
  arithmetic. Whether either name accurately reflects the role is
  unclear without cross-reference to Cache batch (36).

Judgement:
- Names may be approximately right but lack concrete evidence here.
  This is a he-said/she-said with batch 36 (cache) which already
  established positive evidence for adjacent fields.

Proposals:
- keep current  (medium)
- `cacheBlockCount` for `maxBlocks` (low)

Cross-reference:
- Counterparts in batch 36 (cache).

Locations consulted:
- declared: include/zhinst/device_constants.hpp:153,154
- read:     src/awg_compiler.cpp:300,301; src/custom_functions_io.cpp:1530,1642

### DeviceConstants::sineNodeBase  [yes / medium / —]

Evidence:
- include/zhinst/device_constants.hpp:155 declaration; comment says
  "always 16 — base offset for oscillator/sine node index".
- src/custom_functions_io.cpp:1530:
  ```
  int nodeIdx = devConst_->sineNodeBase * devConst_->maxBlocks
              + devConst_->waveformElfAlignment;
  ```
- src/custom_functions_io.cpp:1536: `int nodeOffset = devConst_->sineNodeBase;`

Interpretation:
- `sineNodeBase` is multiplied by `maxBlocks` and added to
  `waveformElfAlignment` to compute a node index. Reading the
  expression literally — a "base" multiplied by something — is
  arithmetically meaningless if `sineNodeBase` is really a
  *base offset*. The value is always 16 across all devices.

Judgement:
- The role in the expression looks more like a factor / multiplier
  than a "base"; the name is misleading.

Proposals:
- `nodeIndexFactor`  (low)
- keep current  (low)

Locations consulted:
- declared: include/zhinst/device_constants.hpp:155
- read:     src/custom_functions_io.cpp:1530,1536,1642,1674

### DeviceConstants::waveformElfAlignment  [yes / medium / —]

Evidence:
- include/zhinst/device_constants.hpp:156 declaration; comment says
  "ELF segment alignment (bitsPerSample × channels)".
- src/memory_allocator.cpp:99,107,119: used as alignment grain in
  `align_up(blockStart, waveformElfAlignment)`. Comment "GDB-verified:
  Phase 1 uses waveformElfAlignment (DC+0x24, always 64)".
- src/waveform_ir.cpp:37,59,126: assigned to `WaveformIR::elfAlignment_`.
- src/custom_functions_io.cpp:1530,1642: also used as additive constant
  in `sineNodeBase * maxBlocks + waveformElfAlignment` node-index
  computation — completely unrelated to ELF.

Interpretation:
- The field is an alignment of 64 used both for memory-block alignment
  (consistent with the name) AND as an additive constant in node
  index arithmetic (unrelated to ELF). The "ELF" qualifier is at best
  partially accurate; it's actually a generic 64-byte alignment /
  base-node value.

Judgement:
- The name overspecifies the role to ELF when it's also used in
  node-index arithmetic; misleading.

Proposals:
- `nodeIndexBase`  (low)
- `wordAlignment`  (low)
- keep current  (low)

Locations consulted:
- declared: include/zhinst/device_constants.hpp:156
- read:     src/memory_allocator.cpp:119; src/waveform_ir.cpp:37,59,126;
            src/custom_functions_io.cpp:1530,1642

### DeviceConstants::waveformGranularity, DeviceConstants::waveformPageSize, grainSize(), maxWaveformLength()  [coordinated-rename]

Evidence:
- include/zhinst/device_constants.hpp:202: `grainSize() { return waveformPageSize; }`.
- include/zhinst/device_constants.hpp:203: `maxWaveformLength() { return waveformGranularity; }`.
- src/prefetch_placesingle.cpp:521-523:
  ```
  int grainSize = dc->grainSize();
  int paddedLen = ((len + grainSize - 1) / grainSize) * grainSize;
  int maxLen = dc->maxWaveformLength();
  ```
- src/waveform.cpp:409: `uint32_t granularity = dc->waveformPageSize;`
- src/wavetable_front.cpp:138: `uint32_t wfGrain = dc->waveformPageSize;`
- src/prefetch_prepare.cpp:698: `int grainSize = static_cast<int>(devConst_->waveformPageSize);`
- src/custom_functions.cpp:346,647: same `int alignment = ... waveformPageSize`.
- src/custom_functions_play.cpp:1124 comment: "Also accessible as
  devConst_->maxWaveformLength()" referring to `waveformGranularity`.
- include/zhinst/waveform_front.hpp:71: comment says
  `Waveform::seqRegWidth = dc.waveformGranularity (dc+0x40)`.

Interpretation:
- The current names are *swapped* relative to actual usage:
  - `waveformPageSize` (DC+0x44) is consistently used as the
    round-up grain / alignment unit (`paddedLen = roundUp(len,
    grainSize)`). The legacy alias method `grainSize()` already
    encodes this.
  - `waveformGranularity` (DC+0x40) is consistently used as a
    maximum wave length cap. The legacy alias `maxWaveformLength()`
    already encodes this.
- Two convenience methods on the same struct already document the
  correct role of each field, but the field names themselves still
  carry the old names that contradict usage.

Judgement:
- Both field names are misnomers; the alias methods are correctly
  named and reflect actual semantics.

Proposals (coordinated):
- Rename `waveformPageSize` → `grainSize_` (or `waveformGrain`)  (high)
- Rename `waveformGranularity` → `maxWaveformLength_` (or
  `maxWaveformSize`)  (high)
- Drop the alias methods after rename, or keep them as deprecated
  shims  (medium)

Locations consulted:
- declared: include/zhinst/device_constants.hpp:164,165,202,203
- read:     src/prefetch_placesingle.cpp:521-523,571-574;
            src/prefetch_prepare.cpp:698; src/waveform.cpp:409;
            src/wavetable_front.cpp:138; src/custom_functions.cpp:346,647;
            src/seqc_ast_nodes_evaluate.cpp:6923;
            src/prefetch_emit.cpp:231,793;
            src/prefetch_splitplay.cpp:103;
            src/waveform_front.cpp:96; src/waveform_ir.cpp:115;
            include/zhinst/waveform_front.hpp:71

### DeviceConstants::seqClockDivider  [unsure / medium / —]

Evidence:
- include/zhinst/device_constants.hpp:175 declaration; comment "0 or 1165".
- src/custom_functions_io.cpp:2202-2212:
  ```
  // addi(reg2, AsmRegister(0), Immediate(devConst_->seqClockDivider))
  int immVal = devConst_->seqClockDivider;
  auto addiEntries = asmCommands_->addi(reg2, zero, Immediate(immVal));
  // suser(reg2, 0x69)  ← kSuserWaitCycles
  appendSuser(... reg2, ... kSuserWaitCycles);
  ```
- The same suser address `kSuserWaitCycles` (0x69) is used at
  custom_functions_io.cpp:1383 to write `numAWGCores` from a sibling
  switch arm, and at 2211 in this arm to write `seqClockDivider`.

Interpretation:
- This field is loaded into a register and written via `suser` to
  the address symbolically named `WaitCycles` (0x69). Treating the
  value 1165 as a *clock divider* doesn't fit a "wait cycles"
  destination; it would more naturally be a wait-cycle count.

Judgement:
- The "clock divider" name conflicts with the fact the value goes
  to a wait-cycles register. Cannot definitively say without GDB;
  the name may be wrong.

Proposals:
- keep current  (low)
- `syncWaitCycles`  (low)

Locations consulted:
- declared: include/zhinst/device_constants.hpp:175
- read:     src/custom_functions_io.cpp:2204; src/custom_functions_play.cpp:1792

### DeviceConstants::numOutputPorts  [yes / high / —]

Evidence:
- include/zhinst/device_constants.hpp:180 declaration; values 0/10/12.
- src/custom_functions_io.cpp:2690,2698,2706,2714,2721:
  ```
  asmCommands_->wvft(AsmRegister(0), 1 << devConst_->numOutputPorts);
  asmCommands_->wvft(AsmRegister(0), 9 << devConst_->numOutputPorts);
  ...
  ```
- src/custom_functions_io.cpp:2738,2746:
  ```
  1 << (devConst_->numOutputPorts + 1)
  if ((entryIndex >> devConst_->numOutputPorts) != 0) ...
  ```

Interpretation:
- The field is consistently used as a **bit-shift width** for an
  exec-table-entry encoding: the low `numOutputPorts` bits hold the
  table-entry index, and bits above hold a small selector (1, 9,
  0xd, 0xe, 0x10). Values 10/12 are the bit-widths of the index
  field, not a number of output ports.
- src/static_resources.cpp:211 (`int n = deviceConstants.numOutputPorts;`)
  is the only call site that *might* read it as a port count, but
  that context is itself inside an HDAWG/SHFSG/SHFQC_SG arm with no
  loop visible in the snippet — needs deeper investigation.

Judgement:
- The dominant use is as a shift width on bitfield-packed table-entry
  values; "numOutputPorts" is misleading.

Proposals:
- `execTableIndexBits`  (high)
- keep current  (low)

Locations consulted:
- declared: include/zhinst/device_constants.hpp:180
- read:     src/custom_functions_io.cpp:2690,2698,2706,2714,2721,2738,2746;
            src/static_resources.cpp:211

### DeviceConstants::numAWGCores  [unsure / medium / —]

Evidence:
- include/zhinst/device_constants.hpp:181 declaration; values 0, 3, 5.
- src/custom_functions_io.cpp:1372,1378:
  ```
  if (devConst_->numAWGCores > 0) {
      ... addi(reg2, zero2, Immediate(static_cast<int>(devConst_->numAWGCores)));
      ... suser(reg2, kSuserWaitCycles);  // 0x69
  }
  ```
- src/custom_functions_io.cpp:1403,1408: same pattern.

Interpretation:
- The field is loaded as an immediate and then written via `suser`
  to address 0x69 (kSuserWaitCycles), the same destination that
  `seqClockDivider` (1165) writes to in the sibling HDAWG arm.
  Treating values 3/5 as "AWG core counts" fits HDAWG/SHFSG/SHFQC_SG
  hardware (HDAWG has 4-5 cores grouped, SHFSG has 4); but the use
  here is to write a small constant into a wait-cycles register.

Judgement:
- Conflicting signals: device-shape values match a core count, but
  use site treats it as a wait-cycle count. Cannot decide without
  GDB or extra context.

Proposals:
- keep current  (medium)
- `syncCoreCount` (low) — splits the difference: a count of cores
  used for sync wait

Locations consulted:
- declared: include/zhinst/device_constants.hpp:181
- read:     src/custom_functions_io.cpp:1372,1378,1403,1408

### DeviceConstants::waveformMemSize, DeviceConstants::maxSequenceLen  [unsure / low / cross-batch-arbitration]

Evidence:
- include/zhinst/device_constants.hpp:173,174 declaration; comments say
  "max waveform memory in samples" / "max sequence length (16000)".
- src/awg_compiler.cpp:477,501: `maxSeqLen = ... .maxSequenceLen`,
  `maxWaveforms = ... .waveformMemSize`.
- include/zhinst/device_constants.hpp:205: `maxWaveIndex() {
  return static_cast<uint32_t>(maxSequenceLen); }` — meaning
  `maxSequenceLen` doubles as the WaveIndexTracker bound.
- src/wavetable_ir.cpp:62,103: `waveIndexTracker_(constants.maxWaveIndex(), ...)`.

Interpretation:
- `maxSequenceLen` is consumed by code that calls it `maxWaveforms` and
  by code that wraps it as `maxWaveIndex`. Either the field name is
  wrong, or both consumers happen to need the same numeric bound and
  the name is fine.
- `waveformMemSize` value 1024/16384/32768 — sample units, not bytes.

Judgement:
- Names may be accurate but multi-purposed; needs cross-reference to
  the consumer batches (Compiler, WavetableIR) to settle.

Proposals:
- keep current  (medium)
- For `waveformMemSize`: `maxWaveformCount`  (low)

Cross-reference:
- WavetableIR / Compiler batches.

Locations consulted:
- declared: include/zhinst/device_constants.hpp:173,174,205
- read:     src/awg_compiler.cpp:477,501; src/wavetable_ir.cpp:62,103

### Register::SyncRegA, Register::SyncRegB  [unsure / low / —]

Evidence:
- include/zhinst/device_constants.hpp:101-102 declaration.
- `nm` only shows the unnamed enum types `{unnamed type#7}`/`{unnamed
  type#8}` at addresses 0x44 / 0x45.
- `SuserAddr::SyncRegA` and `SyncRegB` re-declare the same numeric
  values (0x44, 0x45) at lines 119-120.

Interpretation:
- The original symbols are anonymous enum types (their member names
  aren't in the binary). The recon-chosen names "SyncRegA" / "SyncRegB"
  derive from suser-address symmetry but lack independent confirmation.

Judgement:
- Recon-chosen, no use sites visible to verify "sync" semantics.

Proposals:
- keep current  (medium)

Locations consulted:
- declared: include/zhinst/device_constants.hpp:101,102

### SuserAddr::* (35 constants)  [no / medium / not-misnomer]

Evidence:
- The constants are referenced as `kSuser*` free constants throughout
  `custom_functions_io.cpp`, `custom_functions_play.cpp`, and
  `seqc_ast_nodes_evaluate.cpp`. The naming pattern is consistent
  across both the constant declarations and the call sites.
- Many addresses correlate to clearly named features: 0x70/0x71 →
  sine phase 0/1 (set in `setSinePhase`, lines 1521-1526);
  0x72/0x73 → sine phase increment (line 1671); 0x8C/0x8D → freq
  sweep oscillator/control (lines 3099-3199); 0x69 → wait cycles
  (line 1383, 2212).

Interpretation:
- Although the wrapper struct is recon-invented (see SuserAddr), the
  individual constant names match well-defined hardware functions
  evidenced by calling code (`setSinePhase`, `configFreqSweep`,
  trigger/wait paths). This is multi-site usage evidence.

Judgement:
- Individual constant names fit observed usage; not misnomers.

Locations consulted:
- declared: include/zhinst/device_constants.hpp:108-139
- read:     src/custom_functions_io.cpp (numerous sites listed above);
            src/custom_functions_play.cpp:119;
            src/seqc_ast_nodes_evaluate.cpp:7511,7650

### getDeviceConstants::deviceType (parameter)  [no / medium / —]

Evidence:
- include/zhinst/device_constants.hpp:213; src/device_constants.cpp:28.
- Used immediately as `int dt = static_cast<int>(deviceType);
  switch (dt) { ... }`.

Interpretation:
- The parameter is dispatched on as the device-type enum value.

Judgement:
- Accurate.

## 4. Symbols inspected and judged routinely fine

- `DeviceConstants::deviceType` — written from enum, read in switch arms across compiler.
- `DeviceConstants::waveformRegBase` — serialized as `"bitstream"` in awg_compiler.cpp:1165, also fed into HW register sites.
- `DeviceConstants::waveformMemorySize` — sized `Cache` (cross-batch confirmed in batch 36).
- `DeviceConstants::sampleLength` — Cache page size (cross-batch confirmed in batch 36).
- `DeviceConstants::waveformAlignment` — used as alignment in `clampToCache` and `WavetableIR::allocateWaveforms*`.
- `DeviceConstants::registerDepth` — bound for register-number range check at awg_assembler_opcodes.cpp:117-119.
- `DeviceConstants::memoryDepth` — bound for memory-address range check at awg_assembler_opcodes.cpp:476-480.
- `DeviceConstants::triggerLatencyCycles` — subtracted as a cycle count at custom_functions_io.cpp:669.
- `DeviceConstants::playMinSamples` — passed to `checkPlayMinLength` at custom_functions.cpp:601.
- `DeviceConstants::bitsPerSample` — multiplied as bit count in `totalBits` at multiple sites.
- `DeviceConstants::numCounters` — bounds `getCnt` counter index at custom_functions_io.cpp:2448 (matches error message "counter argument must be either 0 or 1").
- `DeviceConstants::samplingRate` — read as Hz at static_resources.cpp:249.
- `DeviceConstants::hasDIO` — passed as DIO support flag at compiler.cpp:545.
- `DeviceConstants::numDIOBits` — used as `(1<<numDIOBits)-1` mask and `(mask>>numDIOBits)==0` validity check (custom_functions.cpp:362,480; custom_functions_io.cpp:1351,1389).
- `DeviceConstants::hasPrecomp` — read as bool at multiple sites (awg_compiler.cpp:766; prefetch_emit.cpp:291,401; prefetch_placesingle.cpp:515).
- `DeviceConstants::sequencerRegBase` — symmetric with `waveformRegBase`; not seen used in surveyed code, kept routine.
- `DeviceConstants::waveformMinSamples` — values match initial seqRegWidth; thin evidence but not contradicted.
- `DeviceConstants::maxWaveformLength()`, `maxWaveIndex()` — alias methods read correctly per usage.
- `DeviceConstants::maxDioTableEntries()` — used at wavetable_front.cpp:343-344 in DIO table size check; matches name.

## 5. Coverage

- **Fully covered:** all 27 data members, the 4 alias methods, the
  3 nested-type names, the 35 `SuserAddr` constants, the 2
  `Register` enum members, and the `getDeviceConstants` free
  function and its single parameter.
- **Deferred:** `cachePageCount` and `maxBlocks` are flagged
  `cross-batch-arbitration` against batch 36 (cache).
  `maxSequenceLen` / `waveformMemSize` are flagged against the
  WavetableIR / Compiler batches.
- **Not covered (out of scope per RULES §2/§3):**
  - Type names `DeviceConstants` and `DeviceConstants::Register`
    are tier-1 excluded (in symbol table) but recorded as
    positive-evidence blocks per §4d.
  - Free function name `getDeviceConstants` is in symbol table; recorded as
    positive-evidence block.
- **Status:** complete (subject to listed cross-batch arbitrations).
