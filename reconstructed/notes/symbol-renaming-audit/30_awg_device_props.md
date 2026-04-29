# Batch 30 — awg_device_props

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 1 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 1;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/awg_device_props.hpp`
- `reconstructed/src/awg_device_props.cpp`

Use-site survey via `grep -rn` across `reconstructed/{src,include}` for
each in-scope symbol. Primary consumer is
`reconstructed/src/compile_seqc.cpp` (the single call site that
builds `AWGCompilerConfig` from an `AwgDeviceProps`). Cross-batch
counterparts consulted:

- `reconstructed/notes/symbol-renaming-audit/23_awg_compiler_config.md`
  — anchors `isHirzel`, `sampleFormat`, `addressImpl`,
  `numChannelGroups`, `deviceType` cluster on the consumer side
  (`AWGCompilerConfig::isHirzel` is the cluster-#1 source-of-truth,
  rated `not-misnomer / high`).
- `reconstructed/notes/symbol-renaming-audit/31_device_constants.md`
  — sibling factory cluster, also driven by `AwgDeviceType` /
  `DeviceTypeCode`.

Binary symbol table consulted via
`nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so`.
Authoritative under §3 (excluded from rename):

- Type `zhinst::AwgPathPatterns` — mangled in
  `AwgPathPatterns::AwgPathPatterns(AwgPathPatterns const&)` (0x2cc4f0)
  and `AwgPathPatterns::~AwgPathPatterns()` (0x2cc480).
- Type `zhinst::AwgDeviceProps` — mangled in
  `AwgDeviceProps::~AwgDeviceProps()` (0xf81e0) and as the return type
  of all 9 `getAwgDeviceProps<…>` specializations.
- Type `zhinst::AwgSequencerType` — mangled in
  `toAwgDeviceType(zhinst::DeviceTypeCode, zhinst::AwgSequencerType)`
  (0x2cbd60), `makeUnsupportedAwgSequencerErrorMessage(...)` (0x2cbdd0)
  and `toString(zhinst::AwgSequencerType)` (0x2cbce0).
- Type `zhinst::AwgDeviceType` — mangled as a non-type template arg
  in all 9 `getAwgDeviceProps<(zhinst::AwgDeviceType)N>` symbols
  (and elsewhere; cf. batch 23).
- Type `zhinst::DeviceTypeCode` — appears in many mangled symbols
  (e.g. boost sort instantiations); excluded.
- Free functions `toAwgDeviceType`,
  `makeUnsupportedAwgSequencerErrorMessage`,
  `toString(AwgSequencerType)`, all 9
  `getAwgDeviceProps<…>` specializations — excluded.
- Methods `AwgPathPatterns::AwgPathPatterns(const&)`,
  `AwgPathPatterns::~AwgPathPatterns`,
  `AwgDeviceProps::~AwgDeviceProps` — excluded.

NOT in the symbol table (in scope):

- All non-static data members of `AwgPathPatterns` and `AwgDeviceProps`
  (Itanium mangling does not encode them).
- All `AwgSequencerType` enumerators.
- All free-function and method parameter names.
- Anonymous-namespace helpers we added in the recon
  (`buildAwgDeviceProps`, the 6 `awgPathPatterns*()` accessors,
  `kFpgaRevisionPattern`, `kSlaveRevisionPattern`).

Tier-2 `.rodata` evidence (verbatim in `_seqc_compiler.so`,
verified by `strings | grep`):

- `"maxelfsize"` — JSON key paired with `AwgDeviceProps::maxElfSize`
  at `compile_seqc.cpp:247`.
- `"/$device$/awgs/$index$/elf/data"` /
  `".../elf/progress"` / `".../enable"` —
  Default pattern triple.
- `"/$device$/qachannels/$index$/generator/elf/data"` (and the two
  siblings) — GrimselQa triple.
- `"/$device$/sgchannels/$index$/awg/elf/data"` (and siblings) —
  GrimselSg triple.
- `"/$device$/system/fpgarevision"` and
  `"/$device$/system/slaverevision"` — the two literal values
  assigned to the `AwgDeviceProps::fpgaRevisionPattern` field.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AwgPathPatterns::elfDataPattern` | no | high | rodata path verbatim | keep current (high) | not-misnomer |
| `AwgPathPatterns::elfProgressPattern` | no | high | rodata path verbatim | keep current (high) | not-misnomer |
| `AwgPathPatterns::enablePattern` | no | high | rodata path verbatim | keep current (high) | not-misnomer |
| `AwgPathPatterns::AwgPathPatterns(3-arg)::elfData` | no | medium | param feeds elfDataPattern | keep current (high) | — |
| `AwgPathPatterns::AwgPathPatterns(3-arg)::elfProgress` | no | medium | param feeds elfProgressPattern | keep current (high) | — |
| `AwgPathPatterns::AwgPathPatterns(3-arg)::enable` | no | medium | param feeds enablePattern | keep current (high) | — |
| `AwgDeviceProps::deviceType` | no | high | anchored by batch 23 consumer | keep current (high) | not-misnomer |
| `AwgDeviceProps::elfDataPattern` | no | high | copied from path triple | keep current (high) | not-misnomer |
| `AwgDeviceProps::elfProgressPattern` | no | high | copied from path triple | keep current (high) | not-misnomer |
| `AwgDeviceProps::enablePattern` | no | high | copied from path triple | keep current (high) | not-misnomer |
| `AwgDeviceProps::maxElfSize` | no | high | JSON key "maxelfsize" verbatim | keep current (high) | not-misnomer |
| `AwgDeviceProps::addressImpl` | no | high | anchored by batch 23 consumer | keep current (high) | not-misnomer |
| `AwgDeviceProps::sampleFormat` | no | high | anchored by batch 23 consumer | keep current (high) | not-misnomer |
| `AwgDeviceProps::isHirzel` | no | high | cluster-#1 source-of-truth | keep current (high) | not-misnomer |
| `AwgDeviceProps::fpgaRevisionPattern` | unsure | low | also slaverevision for HDAWG | `revisionPattern`, keep current | — |
| `AwgSequencerType::Auto` | no | high | toString → "auto" | keep current (high) | not-misnomer |
| `AwgSequencerType::QA` | no | high | toString → "QA" | keep current (high) | not-misnomer |
| `AwgSequencerType::SG` | no | high | toString → "SG" | keep current (high) | not-misnomer |
| `toString(AwgSequencerType)::seq` | no | medium | switched on by name | keep current (high) | — |
| `toAwgDeviceType::code` | no | high | jump-table key on (code-4) | keep current (high) | — |
| `toAwgDeviceType::seq` | no | high | only consulted for SHFQC | keep current (high) | — |
| `getAwgDeviceProps<…>::dt` | no | high | only HDAWG calls dt.hasOption | keep current (high) | — |
| `makeUnsupportedAwgSequencerErrorMessage::code` | no | high | passed to toString(code) | keep current (high) | — |
| `makeUnsupportedAwgSequencerErrorMessage::seq` | no | high | switched to seqName | keep current (high) | — |

Anon-namespace helpers (recon-added; in scope but routinely fine):
listed in §4.

## 3. Detailed findings

### AwgDeviceProps::isHirzel  [no / high / not-misnomer]

Evidence:
- `awg_device_props.hpp:147` declaration `bool isHirzel; // +0x60`.
- `awg_device_props.cpp:159,204,228,236,244,252,260` set the value
  per-template; the comment table at `awg_device_props.cpp:172-180`
  enumerates `true` for HDAWG/SHFSG/SHFQC_SG/SHFLI/GHFLI/VHFLI and
  `false` for UHFLI/UHFQA/SHFQA — the canonical "Hirzel-generation"
  device family.
- Sole consumer call site (cross-TU):
  `compile_seqc.cpp:199` `config.isHirzel = props.isHirzel;`
  feeds `AWGCompilerConfig::isHirzel` (+0x18).
- Batch 23 (`23_awg_compiler_config.md:92-163`) anchors
  `AWGCompilerConfig::isHirzel` as cluster-#1 source-of-truth, with
  30+ downstream Hirzel-vs-Cervino branches verifiable in `prefetch*`,
  `custom_functions*`, `awg_compiler.cpp`, `compiler.cpp`.

Interpretation:
- The producer side here populates the field that batch 23 has
  already vindicated as the canonical Hirzel/Cervino device-family
  marker. Field name is consistent across producer (this file) and
  consumer (`AWGCompilerConfig::isHirzel`).

Judgement:
- Not a misnomer; this is the producer end of the canonical
  `isHirzel` cluster.

Proposals:
- keep current  (high)

Cross-reference:
- Anchored by `AWGCompilerConfig::isHirzel` (batch 23,
  `not-misnomer / high`). Synthesis: keep both.

Locations consulted:
- declared: include/zhinst/awg_device_props.hpp:147
- written: src/awg_device_props.cpp:159 (helper) + 9 template sites
- consumed: src/compile_seqc.cpp:199

### AwgDeviceProps::maxElfSize  [no / high / not-misnomer]

Evidence:
- `awg_device_props.hpp:144` `uint64_t maxElfSize; // JSON "maxelfsize"`.
- `awg_device_props.cpp:156` set in helper; template values 0x10000000
  (UHFLI/UHFQA), 0x80000000 (Shf families), conditional 0x10000000 vs
  0x80000000 for HDAWG depending on `DeviceOption::ME` (Memory
  Extension) at lines 200-201.
- `compile_seqc.cpp:247` `result["maxelfsize"] = props.maxElfSize;`
  emits the value verbatim under that JSON key.
- `strings _seqc_compiler.so | grep maxelfsize` returns one match —
  the `.rodata` JSON key string is present verbatim in the binary.

Interpretation:
- Tier-2 evidence: the JSON serializer key matches the field name
  modulo case. Per-template values are sample-count caps for the ELF
  payload. Both the name and the unit (samples / bytes — see below)
  match consumer behavior.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/awg_device_props.hpp:144
- written: src/awg_device_props.cpp:156 + 9 template sites (esp. 201)
- consumed: src/compile_seqc.cpp:247

### AwgDeviceProps::addressImpl  [no / high / not-misnomer]

Evidence:
- `awg_device_props.hpp:145` `uint32_t addressImpl; // +0x58`.
- `compile_seqc.cpp:196` `config.addressImpl = props.addressImpl;`
- Downstream: `awg_compiler.cpp:166`
  `WavetableFront(deviceConstants_, config.addressImpl, …)` and
  `compiler.cpp:437`
  `detail::AddressImpl<uint32_t>(config_->addressImpl)` — the value
  is the type-`AddressImpl` constructor argument, i.e. the waveform
  memory base address. Per-template values in this file: 0xd0000000
  (UHFLI/UHFQA), 0x80000000 (HDAWG), 0x00000000 (Shf families).
- Batch 23 (`23_awg_compiler_config.md:59,745-747`) rates
  `AWGCompilerConfig::addressImpl` `not-misnomer / medium-high`.

Interpretation:
- The field carries the `AddressImpl<uint32_t>` ctor argument; both
  the name and the value-domain match. Producer-side name is
  consistent with the consumer-side anchor.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Cross-reference:
- Consumer side: `AWGCompilerConfig::addressImpl` (batch 23).

Locations consulted:
- declared: include/zhinst/awg_device_props.hpp:145
- written: src/awg_device_props.cpp:157 + 9 template sites
- consumed: src/compile_seqc.cpp:196

### AwgDeviceProps::sampleFormat  [no / high / not-misnomer]

Evidence:
- `awg_device_props.hpp:146` `uint32_t sampleFormat; // SampleFormat enum`.
- Per-template values 0/1/2 (`awg_device_props.cpp:172-180` table).
- `compile_seqc.cpp:194`
  `config.sampleFormat = SampleFormat(props.sampleFormat);` —
  consumer immediately reinterprets as the `SampleFormat` enum.
- Downstream: `write_waves_to_elf.cpp:71,153`
  `SampleFormat format = static_cast<SampleFormat>(config.sampleFormat);
  addWaveform(wf, format, …);`.
- Batch 23 (`23_awg_compiler_config.md:447-474`) anchors the
  consumer field as `not-misnomer / high`.

Interpretation:
- Producer-side raw int is converted to `SampleFormat` at the
  consumer. Field name accurately describes the role.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/awg_device_props.hpp:146
- written: src/awg_device_props.cpp:158 + 9 template sites
- consumed: src/compile_seqc.cpp:194; src/write_waves_to_elf.cpp:71,153

### AwgDeviceProps::deviceType  [no / high / not-misnomer]

Evidence:
- `awg_device_props.hpp:140` `AwgDeviceType deviceType; // +0x00`.
- Per-template the field is set to the matching one-hot
  `AwgDeviceType` enumerator (UHFLI=1 .. VHFLI=256).
- `compile_seqc.cpp:193` `config.deviceType = props.deviceType;`
- The enum `AwgDeviceType` is in the binary symbol table (mangled in
  `AWGCompilerConfig::getAwgDeviceTypeString(zhinst::AwgDeviceType)`
  per batch 23) and its enumerators are tier-2 vindicated by the
  `getAwgDeviceTypeString` body's `.rodata` strings.

Interpretation:
- Field stores the `AwgDeviceType` one-hot bit per device family;
  name and type fully match.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/awg_device_props.hpp:140
- written: src/awg_device_props.cpp:152 + 9 template sites
- consumed: src/compile_seqc.cpp:193

### AwgDeviceProps::elfDataPattern / elfProgressPattern / enablePattern  [no / high / not-misnomer]

Evidence:
- `awg_device_props.hpp:141-143` declarations.
- `awg_device_props.cpp:153-155` field-by-field copies from
  `AwgPathPatterns::elfDataPattern` / `elfProgressPattern` /
  `enablePattern` — same names on both sides.
- The literal path strings populated into these fields are present
  verbatim in `.rodata`:
  - `"/$device$/awgs/$index$/elf/data"`
  - `"/$device$/awgs/$index$/elf/progress"`
  - `"/$device$/awgs/$index$/enable"`
  - `"/$device$/qachannels/$index$/generator/elf/data"` (and two siblings)
  - `"/$device$/sgchannels/$index$/awg/elf/data"` (and two siblings)
- `strings _seqc_compiler.so | grep -E "qachannels.*generator|sgchannels.*awg/elf|/enable"`
  confirms.

Interpretation:
- Tier-2: the path literals encode the LabOne node addresses for the
  ELF data / progress / enable nodes. The names "elfDataPattern",
  "elfProgressPattern", "enablePattern" describe exactly what the
  patterns address.

Judgement:
- Not misnomers.

Proposals:
- keep current  (high) for all three

Locations consulted:
- declared: include/zhinst/awg_device_props.hpp:141-143
- written: src/awg_device_props.cpp:153-155 (+ string literals at
  src/awg_device_props.cpp:80-122)
- consumed: cross-TU only via the AwgDeviceProps copy; no direct
  per-field reads outside this file's helper (the full props object
  is not currently consumed beyond the 5 fields listed in
  compile_seqc.cpp:193-247).

### AwgDeviceProps::fpgaRevisionPattern  [unsure / low / —]

Evidence:
- `awg_device_props.hpp:148`
  `std::string fpgaRevisionPattern; // +0x68 (also slaverevision for HDAWG)`
- `awg_device_props.cpp:130-133` literal definitions:
  ```
  ".../system/fpgarevision"   29 chars  — used by all except HDAWG
  ".../system/slaverevision"  30 chars  — used by HDAWG only
  constexpr const char kFpgaRevisionPattern [] = "/$device$/system/fpgarevision";
  constexpr const char kSlaveRevisionPattern[] = "/$device$/system/slaverevision";
  ```
- `awg_device_props.cpp:204` HDAWG specialization passes
  `kSlaveRevisionPattern` to the same field.
- `strings _seqc_compiler.so | grep -E "fpgarevision|slaverevision"`
  finds both literals in `.rodata`.
- No reader of this field exists in `reconstructed/src/`
  (`grep -rn fpgaRevisionPattern` returns only writes inside this
  file; `compile_seqc.cpp` does not propagate it onto
  `AWGCompilerConfig`). The original binary presumably consumes it
  somewhere (the field wouldn't otherwise be carried), but the
  consumer is not reconstructed yet.

Interpretation:
- For 8 of 9 device families the field literally holds an
  `fpgarevision` path; for HDAWG it holds `slaverevision`. The name
  accurately describes 8/9 of the actual contents but is misleading
  for the HDAWG case (the value is then a *slave* revision path,
  which is a different LabOne node). The header comment
  acknowledges this.
- Without a reconstructed consumer it is not possible to tell
  whether the original binary's data-member name leaned toward the
  generic role ("revisionPattern") or the dominant case
  ("fpgaRevisionPattern"). Symbol table cannot decide
  (non-static member).

Judgement:
- Unsure: name is faithful to the dominant 8/9 case but
  contradicted by the HDAWG case; no consumer to disambiguate.

Proposals:
- `revisionPattern`         (medium) — generic name covers both
                                       fpgarevision and slaverevision
- keep current `fpgaRevisionPattern` (low) — only if synthesis
                                       prefers majority-case naming
- `revisionNodePattern`     (low)

Cross-reference:
- No counterpart field in `AWGCompilerConfig` (batch 23) — the field
  is not propagated onto the consumer config. Possible incidental
  finding: a future reconstruction may surface a missing consumer.

Locations consulted:
- declared: include/zhinst/awg_device_props.hpp:148
- written: src/awg_device_props.cpp:160 + per-template at
  192,204,212,220,228,236,244,252,260
- consumed: none under `reconstructed/src/`

### AwgSequencerType::Auto / QA / SG  [no / high / not-misnomer]

Evidence:
- `awg_device_props.hpp:42-44` enum definition with explicit
  toString comments.
- `awg_device_props.cpp:369-376` `toString(AwgSequencerType)`
  returns `"auto"`, `"QA"`, `"SG"`, `"unknown"` (default).
- `makeUnsupportedAwgSequencerErrorMessage` (at 0x2cbdd0,
  reconstructed at `awg_device_props.cpp:347-366`) emits the same
  4 strings in its formatted error message; the SSO byte patterns
  decoded from disassembly (0x6f747561 LE = "auto", 0x4151 LE =
  "QA", 0x4753 LE = "SG", and 7-char "unknown") match these names
  byte-for-byte.
- The literal strings `"auto"`, `"QA"`, `"SG"`, `"unknown"` appear
  verbatim in `.rodata` at the addresses referenced in the source
  comments (0x90b138, 0x90b15f, 0x90b16d).
- `compile_seqc.cpp:46-50` `parseSequencerType(std::string const&)`
  also maps `"QA"` → `QA`, `"SG"` → `SG`, otherwise `Auto`.
- `toAwgDeviceType` (`awg_device_props.cpp:300-327`) uses the
  enumerators only in the SHFQC branch, in the order
  `QA → SHFQA`, `SG → SHFQC_SG`, `Auto → unsupported`.

Interpretation:
- Tier-2: the binary's stringification maps the three values 0/1/2
  to the literal names "auto"/"QA"/"SG". Enumerator names match
  those literal `.rodata` strings (modulo capitalization for `Auto`).

Judgement:
- Not misnomers.

Proposals:
- keep current  (high) for all three

Locations consulted:
- declared: include/zhinst/awg_device_props.hpp:42-44
- used: src/awg_device_props.cpp:300-327, 347-376;
        src/compile_seqc.cpp:46-50, 119-138

### toAwgDeviceType::code, toAwgDeviceType::seq  [no / high / —]

Evidence:
- Declaration `awg_device_props.hpp:226`
  `AwgDeviceType toAwgDeviceType(DeviceTypeCode code, AwgSequencerType seq);`
- Body `awg_device_props.cpp:300-327` `switch (c) { … }` over
  `static_cast<int>(code)`, with the SHFQC branch additionally
  testing `seq == AwgSequencerType::QA / SG`.
- Disassembly reproduction at `awg_device_props.cpp:266-298` shows
  the binary uses a 29-entry jump table on `(code - 4)` and a cmov
  sequence on `seq` — i.e. the parameter usage matches the names
  one-to-one.
- Sole call site: `compile_seqc.cpp:134`
  `AwgDeviceType awgDT = toAwgDeviceType(dtCode, seqType);` where
  `dtCode` is a `DeviceTypeCode` and `seqType` is an
  `AwgSequencerType`.

Interpretation:
- Both parameters are used exactly per their type; names match the
  types and the call-site argument names.

Judgement:
- Not misnomers.

Proposals:
- keep current  (high) for both

Locations consulted:
- declared: include/zhinst/awg_device_props.hpp:226
- defined: src/awg_device_props.cpp:300
- used: src/compile_seqc.cpp:134

### getAwgDeviceProps<T>::dt  [no / high / —]

Evidence:
- 9 specializations `awg_device_props.cpp:188,196,208,216,224,232,240,248,256`
  declared `(DeviceType const& dt)` (parameter unused in 8 of 9 —
  marked `/*dt*/`).
- HDAWG specialization (`awg_device_props.cpp:197-205`) is the only
  one that consults `dt`:
  ```
  const bool hasME = dt.hasOption(static_cast<DeviceOption>(0x13));
  ```
  matching the disassembly note at `awg_device_props.cpp:198-199`
  ("ME = DeviceOption value 0x13 — confirmed by `mov esi, 0x13`
  followed by call to DeviceType::hasOption at 0x2ccb95").
- Sole external call site (via dispatcher):
  `compile_seqc.cpp:56-69` `dispatchGetAwgDeviceProps(dt, devType)`
  forwards `devType` (a `DeviceType`) to the chosen specialization —
  same type as the parameter name implies.

Interpretation:
- Parameter is the `DeviceType` whose option flags drive the
  HDAWG-only conditional; `dt` is the conventional short form for
  this type used elsewhere in the codebase
  (`compile_seqc.cpp:56,99-142`).

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/awg_device_props.hpp:193-201
- defined: src/awg_device_props.cpp:188,196,208,216,224,232,240,248,256
- used: src/compile_seqc.cpp:56-69

### makeUnsupportedAwgSequencerErrorMessage::code, ::seq  [no / high / —]

Evidence:
- Declaration `awg_device_props.hpp:240-241` and definition
  `awg_device_props.cpp:347-366`. Body uses `code` only as the
  argument to `toString(code)` (a `DeviceTypeCode → string`
  formatter — `DeviceTypeCode` is binary-authoritative per §3) and
  uses `seq` only to dispatch to one of the four sequencer-name
  literals.
- Sole call site: `compile_seqc.cpp:138`
  `std::string errMsg = makeUnsupportedAwgSequencerErrorMessage(dtCode, seqType);`
  inside the branch where `toAwgDeviceType` returned 0
  (unsupported).

Interpretation:
- Parameters carry the device-type code and sequencer kind whose
  unsupported combination triggered the error; names match types.

Judgement:
- Not misnomers.

Proposals:
- keep current  (high) for both

Locations consulted:
- declared: include/zhinst/awg_device_props.hpp:240
- defined: src/awg_device_props.cpp:347
- used: src/compile_seqc.cpp:138

### AwgPathPatterns 3-arg ctor params: elfData / elfProgress / enable  [no / medium / —]

Evidence:
- `awg_device_props.cpp:40-46` ctor body:
  ```
  : elfDataPattern(std::move(elfData)),
    elfProgressPattern(std::move(elfProgress)),
    enablePattern(std::move(enable))
  ```
- Each parameter is moved into the equivalently-named field.
  The header field names are themselves vindicated above.
- All call sites are the 4 anonymous-namespace globals at
  `awg_device_props.cpp:80-122`, which pass string literals
  positionally (`"/.../elf/data"`, `"/.../elf/progress"`,
  `"/.../enable"`).

Interpretation:
- Parameter names match the destination fields and the literal
  strings they carry. Slight asymmetry — the third param drops the
  `Pattern` suffix — but the role is unambiguous.

Judgement:
- Not misnomers.

Proposals:
- keep current  (high) for all three

Locations consulted:
- defined: src/awg_device_props.cpp:40-46
- used: src/awg_device_props.cpp:80-122

## 4. Symbols inspected and judged routinely fine

- `toString(AwgSequencerType)::seq` — sole switch subject; name
  matches type. (Block-worthy detail folded into the
  `AwgSequencerType` enum-members block above.)
- `AwgPathPatterns` defaulted ctors / dtor / copy / move (no
  parameters of concern).
- `AwgDeviceProps::~AwgDeviceProps()` — no parameters; symbol-table
  excluded anyway.
- Recon-added `buildAwgDeviceProps` parameters
  (`type`, `patterns`, `maxElfSize`, `addressImpl`, `sampleFormat`,
  `isHirzel`, `fpgaPattern`) — all align with the same-named fields
  on `AwgDeviceProps`/`AwgPathPatterns`; `fpgaPattern` inherits the
  `fpgaRevisionPattern` ambiguity flagged above but is otherwise a
  pure forwarder.
- Recon-added namespace-helpers
  `awgPathPatternsDefault` / `awgPathPatternsGrimselQa` /
  `awgPathPatternsGrimselSg` / `awgPathPatternsGrimselLi` /
  `awgPathPatternsGurnigel` / `awgPathPatternsMaloja` — names mirror
  the original binary's anonymous-namespace global names exactly
  (verified in `nm`: `zhinst::(anonymous namespace)::awgPathPatternsDefault`
  etc.). Tier-1 evidence that these names came from the original.
- Recon-added constants `kFpgaRevisionPattern` / `kSlaveRevisionPattern`
  — values are the verbatim `.rodata` literals; `k…Pattern` form
  consistent with project convention; not in `nm` (file-static
  constexpr arrays; expected).

## 5. Coverage

- **Fully covered:**
  - All 3 data members of `AwgPathPatterns`.
  - All 7 data members of `AwgDeviceProps`.
  - All 3 parameters of the `AwgPathPatterns` 3-arg ctor.
  - All 3 enumerators of `AwgSequencerType`.
  - Parameters of `toString(AwgSequencerType)`,
    `toAwgDeviceType`, all 9 `getAwgDeviceProps<T>` specializations,
    `makeUnsupportedAwgSequencerErrorMessage`.
  - Cross-batch arbitration with batch 23 on `isHirzel` / `sampleFormat` /
    `addressImpl` / `deviceType` (all reaffirmed not-misnomer).
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type names `AwgPathPatterns`, `AwgDeviceProps`,
    `AwgSequencerType`, `AwgDeviceType`, `DeviceTypeCode` — present
    in mangled symbols (see §1); excluded under §3.
  - Free functions `toAwgDeviceType`,
    `makeUnsupportedAwgSequencerErrorMessage`,
    `toString(AwgSequencerType)`, all 9 `getAwgDeviceProps<…>`
    explicit specializations — present as `t` symbols in `nm`;
    excluded under §3.
  - Methods `AwgPathPatterns::AwgPathPatterns(const&)`,
    `AwgPathPatterns::~AwgPathPatterns`,
    `AwgDeviceProps::~AwgDeviceProps` — present as `t` symbols;
    excluded.
  - The 6 anonymous-namespace path-pattern global *names*
    (`awgPathPatternsDefault` etc.) — present as data symbols in
    `nm` (`b zhinst::(anonymous namespace)::awgPathPatternsDefault`
    at 0xb84fc0 etc.); these names are tier-1 authoritative and
    excluded. The recon-added Meyers-singleton accessor functions
    of the same names are mere wrappers and inherit the names
    faithfully.
- **Anomalies / incidental observations** (not actionable for this
  audit but worth flagging):
  1. `nm` shows `zhinst::(anonymous namespace)::AwgDevicePropsWithDefault::~AwgDevicePropsWithDefault()`
     at 0x2cc870 — an additional anonymous-namespace type related
     to this cluster that is **not** present in the reconstruction.
     Either it is a wrapper (e.g. an `optional<AwgDeviceProps>` or
     "props plus default-marker") used by some path the recon
     hasn't surfaced, or it is the dispatcher / defaulting-helper
     replaced in the recon by `dispatchGetAwgDeviceProps`. This
     is structural, not a naming issue, and out of scope here, but
     the missing reconstruction is a candidate for a TODO entry.
  2. `AwgDeviceProps::fpgaRevisionPattern` has no reconstructed
     consumer (see the field's block) — the field is currently
     write-only in the recon. Likely an incomplete reconstruction
     of the binary's downstream path-formatting code. Not a naming
     defect on its own.
  3. `compile_seqc.cpp:200` derives `config.cacheType` from
     `props.deviceType == AwgDeviceType::HDAWG`; this is a
     producer-side fact already noted by batch 23 and is not a
     props-side naming issue.

- **Cross-batch arbitration outcomes:**
  - `AwgDeviceProps::isHirzel` / `::sampleFormat` / `::addressImpl` /
    `::deviceType` reaffirm batch 23's anchoring; both producer
    (this batch) and consumer (batch 23) sides keep current names.
  - `AwgDeviceProps::fpgaRevisionPattern` is the only candidate
    for a possible rename out of this batch; its synthesis decision
    can wait until a consumer is reconstructed. No arbitration
    conflict because no counterpart exists in the current
    `AWGCompilerConfig`.
- **Status:** `complete`.
