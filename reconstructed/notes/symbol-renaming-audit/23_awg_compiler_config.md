# Batch 23 — awg_compiler_config

## 1. Files considered

- `reconstructed/include/zhinst/awg_compiler_config.hpp`
- `reconstructed/src/awg_compiler_config.cpp`

Use-site survey via `grep -r` across `reconstructed/src/**.cpp` and
`reconstructed/include/zhinst/**.hpp`. Notable consumer files:
`compile_seqc.cpp` (constructs the config), `compiler.cpp`,
`prefetch*.cpp`, `custom_functions*.cpp`, `awg_compiler.cpp`,
`static_resources.cpp`, `asm_commands.cpp`, `write_waves_to_elf.cpp`,
`seqc_ast_nodes_evaluate.cpp`.

Cross-batch counterparts consulted:
- `reconstructed/notes/symbol-renaming-audit/36_cache.md`
  (`Cache::appendMode_`, `Cache::Cache::appendMode` — both flagged
  unsure/low pending the `isHirzel` source-of-truth verdict here).
- `reconstructed/notes/symbol-renaming-audit/09_prefetch.md`
  (`Prefetch::split_` confirmed as a *derived superset* of Hirzel,
  and `Prefetch::isHirzel_()`/`set_isHirzel_()` flagged as dead
  alias misnomers of identical shape to `appendMode()` here).

Binary symbol table consulted via
`nm --demangle _seqc_compiler.so | grep -E "AWGCompilerConfig|appendMode|splitIndex|syncVersion"`.
Authoritative under §3 (excluded from rename):

- Type `zhinst::AWGCompilerConfig`.
- `zhinst::AWGCompilerConfig::~AWGCompilerConfig()` (0xf8080).
- `zhinst::AWGCompilerConfig::getAwgDeviceTypeString(zhinst::AwgDeviceType)`
  (0x270080).
- `zhinst::AWGCompilerConfig::getAwgDeviceTypeFromString(string const&)`
  (0x270180).
- `zhinst::AWGCompilerConfig::getChannelGroupingModeString() const`
  (0x270b10).

NOT in the symbol table (pure recon-added accessors, in scope):

- `AWGCompilerConfig::appendMode()`
- `AWGCompilerConfig::splitIndex()`
- `AWGCompilerConfig::syncVersion()`

Tier-2 RTTI / `.rodata` strings located via
`strings _seqc_compiler.so | grep -E "^(UHFLI|HDAWG|UHFQA|SHFQA|SHFSG|SHFLI|GHFLI|VHFLI|cervino|hirzel|klausen|grimsel|gurnigel|maloja|4x2|2x4|1x8)"`:
all `getAwgDeviceTypeString` return values and all
`getAwgDeviceTypeFromString` codenames are present verbatim — strong
positive evidence that the method bodies in
`awg_compiler_config.cpp` faithfully reproduce binary `.rodata`. The
parameter names of those methods (`type`, `str`) are not in scope as
misnomers — no contradicting evidence.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AWGCompilerConfig::deviceType` | no | high | indexes device dispatch everywhere | keep current (high) | not-misnomer |
| `AWGCompilerConfig::sampleFormat` | no | high | passed to `addWaveform` as format | keep current (high) | not-misnomer |
| `AWGCompilerConfig::deviceSampleRate` | no | high | NaN→default sample rate | keep current (high) | — |
| `AWGCompilerConfig::addressImpl` | no | medium | feeds `AddressImpl<uint>` ctor | keep current (high) | — |
| `AWGCompilerConfig::channelsPerGroup` | no | medium | uint16 pair indexed by `indexed` | keep current (high) | — |
| `AWGCompilerConfig::isHirzel` | no | high | source of cluster, name verified | keep current (high) | not-misnomer |
| `AWGCompilerConfig::cacheType` | no | medium | feeds `Cache::CacheType` enum | keep current (medium) | — |
| `AWGCompilerConfig::numChannelGroups` | no | high | 1/2/4 → "4x2"/"2x4"/"1x8" | keep current (high) | not-misnomer |
| `AWGCompilerConfig::awgIndex` | no | high | builds awg/sgchannel node paths | keep current (high) | not-misnomer |
| `AWGCompilerConfig::deviceIndex` | no | medium | indexes device-table lookups | keep current (high) | — |
| `AWGCompilerConfig::serializeRoundTrip` | no | medium | == 1 triggers debug round-trip | keep current (medium) | — |
| `AWGCompilerConfig::debugDumpPath` | no | high | path for serialized ASM dump | keep current (high) | — |
| `AWGCompilerConfig::debugDumpEnabled` | no | high | gates `debugDumpPath` write | keep current (high) | — |
| `AWGCompilerConfig::debugJsonPath` | no | high | path for JSON dump | keep current (high) | — |
| `AWGCompilerConfig::debugJsonEnabled` | no | high | gates `debugJsonPath` write | keep current (high) | — |
| `AWGCompilerConfig::includePaths` | no | high | vector of include dirs | keep current (high) | — |
| `AWGCompilerConfig::optimizationFlags` | no | medium | bitmask passed to AsmOptimize | keep current (medium) | — |
| `AWGCompilerConfig::debugFlags` | no | medium | bitmask of dump-AST flags | keep current (medium) | — |
| `AWGCompilerConfig::numCores` | unsure | low | only-write site, no consumer | keep current, `pad_94` | verify-not-original |
| `AWGCompilerConfig::channelGrouping` | yes | medium | actually loop-unroll limit | `loopUnrollLimit`, `unrollLimit`, keep current | — |
| `AWGCompilerConfig::compressSource` | no | medium | gates source-section compress | keep current (high) | — |
| `AWGCompilerConfig::wavetableSize` | no | medium | sign-extended to size_t | keep current (high) | — |
| `AWGCompilerConfig::searchPath` | no | high | wave-file lookup root | keep current (high) | — |
| `AWGCompilerConfig::appendMode()` | yes | high | dead alias for `isHirzel` | drop alias | coordinated-rename |
| `AWGCompilerConfig::splitIndex()` | yes | high | dead alias for `cacheType` | drop alias | coordinated-rename |
| `AWGCompilerConfig::syncVersion()` | yes | high | misnomer alias for `numChannelGroups` | drop alias | coordinated-rename |
| `AWGCompilerConfig::getAwgDeviceTypeString::type` (param) | no | high | switched on by jump table | keep current (high) | — |
| `AWGCompilerConfig::getAwgDeviceTypeFromString::str` (param) | no | high | iequals'd against codenames | keep current (high) | — |
| `getAwgDeviceTypeString` / `getAwgDeviceTypeFromString` / `getChannelGroupingModeString` (methods) | (out of scope) | — | binary symbol table | — | — |
| Type `AWGCompilerConfig` | (out of scope) | — | binary symbol table | — | — |

Padding fields (`pad_1a`, `pad_49`, `pad_69`, `pad_9c`, `pad_9e`,
`pad_a4`) are inert layout filler, not data — not flagged.

## 3. Detailed findings

### AWGCompilerConfig::isHirzel  [no / high / not-misnomer]

Evidence:
- `awg_compiler_config.hpp:31` table comment:
  `bool isHirzel  // 0x18 — 1 = Hirzel device (verified: cmpb $0x1,0x18(%rax) at 0x1d6c47)`
- `awg_compiler_config.hpp:68` declaration with the same byte-level
  verification comment.
- `compile_seqc.cpp:199` `config.isHirzel = props.isHirzel;`
- `awg_device_props.hpp:121`
  `// +0x60 isHirzel — consumer: compileSeqc @0xf67cd → config.isHirzel (+0x18)`
- `notes/awg_device_props.md:154` records the props-side field
  rename from a placeholder to `isHirzel`, attested as the
  Hirzel-vs-Cervino device-family marker.
- `notes/device_type.md:971` lists `isHirzel = true for HDAWG /
  SHFSG / SHFQC_SG / SHFLI / GHFLI / VHFLI` — a fixed device-family
  classification.
- 30+ direct read sites all named `config(_)->isHirzel`, e.g.:
  - `prefetch.cpp:61` `std::make_shared<Cache>(…, config.isHirzel)`
  - `prefetch.cpp:773-774,1366,2114`
  - `prefetch_emit.cpp:60,732`
  - `prefetch_placesingle.cpp:170,203,224,482,617,627,692,712,823,914`
  - `prefetch_splitplay.cpp:229,402`
  - `prefetch_prepare.cpp:202`
  - `prefetch_print.cpp:122,207,498`
  - `custom_functions_play.cpp:608,630,634,648`
  - `custom_functions_playback.cpp:284,566`
  - `awg_compiler.cpp:390`
  - `compiler.cpp:544`
- `prefetch.cpp:2267` is the **only** call site that uses the
  `appendMode()` accessor instead of the field — and the comment at
  2263 spells the test out: `if required <= cacheSize OR
  config_->appendMode()`. All other Hirzel-gated code paths read
  `config_->isHirzel` directly.

Interpretation:
- The field is set from `AwgDeviceProps::isHirzel` (a device-family
  classifier) and read at every Hirzel-vs-Cervino branch in the
  generator. Both producer and the overwhelming majority of
  consumers use the bare name `isHirzel`.
- Documentation and tier-2 cross-files (awg_device_props notes,
  device_type notes) consistently treat this byte as the Hirzel
  device classifier.
- This is the *origin* of the cluster #1 (Hirzel/Cervino) chain —
  every downstream "appendMode_" / "split_" / "isHirzel_()" trail
  that batches 09/09b/36 followed leads back to this byte.

Judgement:
- The name is correct and authoritative for the cluster; not a
  misnomer.

Proposals:
- keep current  (high)

Cross-reference:
- Anchors `Cache::appendMode_` (batch 36, flagged unsure/low):
  with `isHirzel` confirmed canonical here, the Cache field's
  rename target should be `isHirzel_` (the value's faithful
  provenance), notwithstanding that internally it gates an
  "always place at offset 0" path. Synthesis decision.
- Confirms `Prefetch::split_` (batch 09) is a *derived superset*
  of `isHirzel`, not an alias; both names are correct on their
  own sides.
- Confirms `Prefetch::isHirzel_()`/`set_isHirzel_()` (batch 09)
  are dead misnomers (they read `split_`, which is broader than
  Hirzel).

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:68
- used: see 30+ sites enumerated above
- related: include/zhinst/awg_device_props.hpp:121;
  notes/awg_device_props.md:154; notes/device_type.md:971

### AWGCompilerConfig::appendMode()  [yes / high / coordinated-rename]

Evidence:
- `awg_compiler_config.hpp:124`
  `bool appendMode() const { return isHirzel; }`
- `awg_compiler_config.hpp:101-103` documenting comment:
  `appendMode : was actually isHirzel test at +0x18 (verified
  0x1cbfc6 in Prefetch::placeLoads: cmp BYTE PTR [rax+0x18], 0x0)`
- Sole call site under `reconstructed/src/`:
  `prefetch.cpp:2267`
  `if (required <= static_cast<size_t>(cacheSize) || config_->appendMode())`
  with surrounding comment at 2263: `check if required <= cacheSize
  OR config_->appendMode()`.
- Every other Hirzel-gated branch in the codebase reads
  `config_->isHirzel` directly (30+ sites; see the `isHirzel` block
  above).
- `nm --demangle _seqc_compiler.so | grep appendMode` returns
  nothing — the accessor does not exist in the original binary.

Interpretation:
- The accessor is a recon-added forwarding wrapper that returns
  `isHirzel`. The header comment explicitly records that the name
  was a misread during initial reconstruction.
- It has exactly one caller, in a single-line condition that would
  read identically (and more honestly) as `config_->isHirzel`.
- The accessor is structurally identical to the dead
  `Prefetch::isHirzel_()` / `set_isHirzel_()` aliases that batch 09
  flagged for removal.

Judgement:
- Yes, `appendMode()` is a misnomer alias over `isHirzel`; should
  be removed and the one call site migrated to direct field read.

Proposals:
- drop accessor, migrate `prefetch.cpp:2267` to
  `config_->isHirzel`  (high)

Cross-reference:
- Same shape as `Prefetch::isHirzel_()` / `set_isHirzel_()` —
  batch 09 (also "drop entirely").
- Different from `Cache::appendMode_` (the field): that one stores
  the value, this one merely re-aliases it. Removing this accessor
  does not affect the Cache batch decision.

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:124
- used: src/prefetch.cpp:2263,2267 (sole site)
- related: include/zhinst/prefetch.hpp:301-311 (parallel dead aliases)

### AWGCompilerConfig::splitIndex()  [yes / high / coordinated-rename]

Evidence:
- `awg_compiler_config.hpp:125`
  `uint8_t splitIndex() const { return cacheType; }`
- `awg_compiler_config.hpp:104-106` documenting comment:
  `splitIndex : same field as cacheType at +0x19 (verified
  0x1d6c4d in Prefetch::clampToCache: movzx edx, BYTE PTR [rax+0x19])`
- `grep -rn "splitIndex()" reconstructed/src/` returns **zero**
  call sites.
- `cacheType` itself is read at: `awg_compiler.cpp:298`,
  `prefetch_emit.cpp:64`, `prefetch_placesingle.cpp:917-922`
  (under the name `channelIdx`), `compiler.cpp:539,544`,
  `compile_seqc.cpp:200` (write-only).
- `nm` shows no `splitIndex` symbol.

Interpretation:
- The accessor exists as documentation of a former misread and has
  no live consumers. Dead vocabulary.

Judgement:
- Yes, dead alias and misnomer.

Proposals:
- drop accessor entirely  (high)

Cross-reference:
- Companion to `appendMode()` and `syncVersion()`; same shape,
  same disposition.

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:125
- used: none under src/

### AWGCompilerConfig::syncVersion()  [yes / high / coordinated-rename]

Evidence:
- `awg_compiler_config.hpp:126`
  `int syncVersion() const { return numChannelGroups; }`
- `awg_compiler_config.hpp:107-110` documenting comment:
  `syncVersion : same field as numChannelGroups at +0x1C
  (verified 0x1d7bb5 in Prefetch::placeSingleCommand and
  0x270b1c in getChannelGroupingModeString: cmp DWORD PTR
  [rax+0x1c], <imm>)`
- Two live call sites:
  `prefetch_placesingle.cpp:999` `if (cfg->syncVersion() < 2)`
  `prefetch_placesingle.cpp:1065`
  `if (cfg->syncVersion() < 2 || cfg->deviceType != 2)`
- All other reads of the same byte use the bare field
  `numChannelGroups` (10+ sites in `prefetch.cpp:63,755,2099`,
  `prefetch_helpers.cpp:42,122,361,665`,
  `custom_functions_io.cpp:1260,2145,2216`,
  `custom_functions_play.cpp:520,1369,1412`,
  `custom_functions.cpp:384,418,456,759-760`,
  `asm_commands.cpp:43`, `compiler.cpp:324`,
  `prefetch_emit.cpp:789`).
- `getChannelGroupingModeString` (the verified-faithful method
  whose body reads `numChannelGroups` at +0x1C and returns
  `"4x2"/"2x4"/"1x8"`) is itself the canonical evidence that the
  byte is a *channel-group count*, not a "sync version".
- `nm` shows no `syncVersion` symbol.

Interpretation:
- The two `syncVersion()` call sites both compare the value against
  2, which is consistent with "is the device split into ≥2 channel
  groups" — i.e. exactly the `numChannelGroups` semantic.
- The "sync version" label has no support in the binary's strings
  or in any other consumer.

Judgement:
- Yes, the alias name is wrong: the byte is the channel-group
  count, not a sync version.

Proposals:
- drop accessor; migrate the two call sites to
  `cfg->numChannelGroups < 2`  (high)

Cross-reference:
- Companion to `appendMode()` and `splitIndex()`.

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:126
- used: src/prefetch_placesingle.cpp:999,1065
- related: src/awg_compiler_config.cpp:77-87 (getChannelGroupingModeString)

### AWGCompilerConfig::numChannelGroups  [no / high / not-misnomer]

Evidence:
- `awg_compiler_config.hpp:71` `int numChannelGroups; // 1, 2, or 4`
- `awg_compiler_config.cpp:82-87` (in the binary-faithful
  `getChannelGroupingModeString`):
  ```
  switch (numChannelGroups) {
      case 1:  return "4x2";
      case 2:  return "2x4";
      case 4:  return "1x8";
  ```
  Strings `"4x2"`, `"2x4"`, `"1x8"` are present verbatim in the
  binary's `.rodata` (this method's address 0x270b10 is in the
  binary symbol table).
- `compile_seqc.cpp:201` `config.numChannelGroups = 1;`
- `asm_commands.cpp:43` `, numChannelGroups_(config.numChannelGroups)`
  — initializes a like-named field (already vindicated in batch 10
  per `notes/symbol-renaming-audit/10_asm_commands.md:52`).
- Used as `vector::resize` argument
  (`prefetch.cpp:63 waveformMaps_.resize(config.numChannelGroups)`)
  and as a Node ctor channel-count argument
  (`prefetch.cpp:755,2099`) — both consistent with "count of
  channel groups".

Interpretation:
- The faithful binary method `getChannelGroupingModeString` proves
  that the value 1/2/4 maps to channel-grouping textual modes, i.e.
  the field really is a count of channel groups. Tier-2 evidence
  via the quoted `.rodata` strings.

Judgement:
- The name is correct; not a misnomer.

Proposals:
- keep current  (high)

Cross-reference:
- See `syncVersion()` block: that accessor's name is the misnomer,
  not this field.
- `AsmCommands::numChannelGroups_` (batch 10) — same value, same
  name, also vindicated.

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:71
- used: src/awg_compiler_config.cpp:82-87; src/compile_seqc.cpp:201;
  src/asm_commands.cpp:43; src/prefetch.cpp:63,755,2099;
  src/prefetch_helpers.cpp:42,122,361,665; many more under
  custom_functions*.

### AWGCompilerConfig::channelGrouping  [yes / medium / —]

Evidence:
- `awg_compiler_config.hpp:85` `int32_t channelGrouping; // 0x98 —
  passed to FrontEndLoweringFacade::lower() as last arg`
- `compile_seqc.cpp:206` `config.channelGrouping = 0x20000;
  // +0x98 — 131072; compile-time loop unroll limit`
- `compiler.cpp:283` `config_->channelGrouping); // [config+0x98]`
  passed as the last argument to `lower()`.
- `compiler.cpp:672,681`
  ```
  int channelGrouping)
  ...
  context.channelGrouping = channelGrouping;
  ```
  — copied into `FrontendLoweringState::channelGrouping`.
- `seqc_ast_nodes_evaluate.cpp:8202,8501,8779,10010` — the field is
  consumed at four for/while/repeat loop sites in AST evaluation as
  `if (iterCount > ctx.channelGrouping) { error 0x7b }` — i.e. as a
  **maximum loop iteration / unroll limit** (cap of 0x20000 ==
  131072).
- It is **not** read anywhere as a channel-grouping mode (which
  would be a small int 1/2/4, or "4x2"/"2x4"/"1x8" — that's
  `numChannelGroups`).

Interpretation:
- The 4-byte value at +0x98 is a maximum-iteration / loop-unroll
  cap (default 0x20000) used by the AST evaluator, completely
  unrelated to channel grouping. The current name strongly
  collides with the genuinely-channel-grouping field
  `numChannelGroups` and with the binary-faithful method
  `getChannelGroupingModeString`.
- The name was apparently chosen from disassembly context
  (parameter position into `lower()`) rather than from observed
  consumer behavior in the AST evaluator.

Judgement:
- Yes, the field name misdescribes the value: it is a loop-unroll
  iteration limit, not a channel-grouping mode.

Proposals:
- `loopUnrollLimit`  (medium)
- `unrollLimit`      (medium)
- `maxIterations`    (low)
- keep current       (low) — only if the name is original to the
  binary, which has not been verified (not in `nm` because it is a
  non-static data member; symbol table cannot decide here)

Cross-reference:
- Couples with the matching field in
  `FrontendLoweringState::channelGrouping`
  (`include/zhinst/frontend_lowering.hpp:52`) — synthesis should
  rename both together. Coordinated-rename across files but
  in-batch judgement is unambiguous on the value's role.
- Distinct from `numChannelGroups` (+0x1C) and unrelated to
  `getChannelGroupingModeString`.

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:85
- used: src/compile_seqc.cpp:206; src/compiler.cpp:283,672,681;
  src/seqc_ast_nodes_evaluate.cpp:8202,8501,8779,10010
- related: include/zhinst/frontend_lowering.hpp:43,52;
  include/zhinst/compiler.hpp:92

### AWGCompilerConfig::deviceType  [no / high / not-misnomer]

Evidence:
- `awg_compiler_config.hpp:63` `AwgDeviceType deviceType; // 0x00`
- Used as the dispatch key in `getAwgDeviceTypeString` (binary-
  faithful method 0x270080), in `getChannelGroupingModeString`
  (`if (static_cast<int>(deviceType) != 2) return "";` —
  HDAWG-only), and in:
  - `asm_commands.cpp:39` `AsmCommandsImpl::getInstance(config.deviceType)`
  - `awg_compiler.cpp:164` `getDeviceConstants(config.deviceType)`
  - `static_resources.cpp:58-359` (10+ comparisons against device
    constants)
  - `compile_seqc.cpp:193,200` setter and HDAWG-test
- The enum `AwgDeviceType` is itself in the binary symbol table
  (per `getAwgDeviceTypeString(zhinst::AwgDeviceType)` mangling) —
  this field stores a value of that authoritative enum.

Interpretation:
- Both the type name and the field semantic are confirmed by tier-1
  evidence (binary symbol table) and by faithful `.rodata` strings
  produced by the conversion methods.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:63
- used: src/awg_compiler_config.cpp:38,78; src/compile_seqc.cpp:193,200;
  src/asm_commands.cpp:39; src/awg_compiler.cpp:164;
  src/static_resources.cpp:58,82,103,130,138,208,231,341,359;
  src/compiler.cpp:544; src/prefetch_placesingle.cpp:1065

### AWGCompilerConfig::sampleFormat  [no / high / not-misnomer]

Evidence:
- `awg_compiler_config.hpp:64` `SampleFormat sampleFormat;`
- `write_waves_to_elf.cpp:71,153`
  `SampleFormat format = static_cast<SampleFormat>(config.sampleFormat);`
  passed to `addWaveform(waveform, format, …)`.
- `compile_seqc.cpp:194`
  `config.sampleFormat = SampleFormat(props.sampleFormat);`
- `notes/awg_device_props.md:153` records the props-side field
  rename and verifies the consumer mapping.
- `SampleFormat` is itself a project enum already in scope of other
  audits (defined in `waveform_ir.hpp`); not litigated here.

Interpretation:
- Name, type, producer, and consumer are all consistent and
  vindicated by sibling notes.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:64
- used: src/compile_seqc.cpp:194; src/write_waves_to_elf.cpp:71,153
- related: notes/awg_device_props.md:153

### AWGCompilerConfig::awgIndex  [no / high / not-misnomer]

Evidence:
- `awg_compiler_config.hpp:72` `int32_t awgIndex; // 0x20`
- `compile_seqc.cpp:202`
  `config.awgIndex = static_cast<int32_t>(awgIndex);`
- Read at 15+ sites where it builds node paths via
  `std::to_string(config_->awgIndex)`:
  - `custom_functions_io.cpp:1579,1679,1686,3116,3210,3214,3218,3310,3314,3318`
    embed it in `"sgchannels/<idx>/sines/0/phaseshift"`,
    `"sines/<idx>/phaseshift"`, etc.
  - `custom_functions.cpp:386,395,401,406,420,429,435,439,457`
    use it as a "group index" with `LOG_WARNING("Invalid group
    index: " << config_->awgIndex)`.
  - `custom_functions_play.cpp:1378,1421` validate
    `channelIdx != config_->awgIndex` / `oscIdx != config_->awgIndex`.

Interpretation:
- Field carries the AWG core / channel index used to disambiguate
  per-core node paths. Consumer log strings call it "group index"
  in some sites but the `awgIndex` name is consistent with the
  Python-binding parameter (`compile_seqc.cpp:202`) that
  initializes it.

Judgement:
- Name correctly describes the value; not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:72
- used: src/compile_seqc.cpp:202; src/custom_functions.cpp:386-457;
  src/custom_functions_io.cpp:1539-3318 (10+ sites);
  src/custom_functions_play.cpp:1357-1421

### AWGCompilerConfig::numCores  [unsure / low / verify-not-original]

Evidence:
- `awg_compiler_config.hpp:84`
  `int32_t numCores = 0; // +0x94 — number of AWG cores (binary
  default is 0)`
- `compile_seqc.cpp:205`
  `config.numCores = 0; // +0x94 — binary sets 0 (not 1)`
- No reads at offset +0x94 located under `reconstructed/src/`.
- Two `numCores` reads exist in `prefetch.cpp:2280,2312`, but their
  in-source comments say `// +0x1C — TODO: verify if numChannelGroups`
  — i.e. those reads target +0x1C (the `numChannelGroups` byte),
  not +0x94, and are mis-attributed in recon (a separate recon
  bug, not a misnomer of this field).
- `nm` cannot decide: non-static data members are not encoded.

Interpretation:
- The field at +0x94 is set to 0 unconditionally and never read in
  the reconstructed code. Either the binary really does have a
  consumer at +0x94 that recon hasn't surfaced, or this field is
  inert padding mistakenly named "numCores". Cannot decide here.

Judgement:
- Unsure: the name's accuracy depends on a consumer at +0x94 that
  the audit could not locate; flag low.

Proposals:
- keep current  (medium)
- `pad_94`     (low) — only if synthesis confirms no live consumer

Cross-reference:
- The mis-attributed `numCores` reads at `prefetch.cpp:2280,2312`
  are an *incidental finding* (offset bug, not a name issue),
  out of scope for this audit but worth a TODO entry post-audit.

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:84
- used: src/compile_seqc.cpp:205 (write-only); no reads at +0x94
- mis-attributed reads at +0x1C: src/prefetch.cpp:2280,2312

### AWGCompilerConfig::cacheType  [no / medium / —]

Evidence:
- `awg_compiler_config.hpp:69`
  `uint8_t cacheType; // 0x19 — 0=Normal, 1=Aligned`
- `compile_seqc.cpp:200`
  `config.cacheType = (props.deviceType == AwgDeviceType::HDAWG) ? 1 : 0;`
- `awg_compiler.cpp:298` `uint8_t cacheType = config_->cacheType;`
- `prefetch_emit.cpp:64` `uint8_t cacheType = config_->cacheType;`
- `compiler.cpp:539,544`
  `if (config.cacheType == 1) { … }`
  `wavetableIR->updateWaveforms(config.cacheType != 0 && config.isHirzel, …)`
- `prefetch_placesingle.cpp:917-922` reads `config_->cacheType` and
  binds it to a local named `channelIdx` (a separate recon-side
  misnomer for the local, already noted in
  `awg_compiler_config.hpp:115` as "channelIndex was actually
  config_->cacheType").
- The `Cache::CacheType` enum (`Normal`/`Aligned`) is a project
  enum (batch 36 covered).

Interpretation:
- Producer assigns 1 for HDAWG, 0 otherwise. Consumers use the
  value as both a `Cache::CacheType` selector and as an index into
  `devConst_` arrays at +0x18 (where the local was misnamed
  `channelIdx`). The field-level name is consistent with its
  primary semantic (cache-allocation type).

Judgement:
- Not a misnomer at the field level. The local-variable misuse
  (`channelIdx = config_->cacheType`) is a separate concern
  belonging to the file in which it appears.

Proposals:
- keep current  (medium)

Cross-reference:
- Local `channelIdx` misnomer in `prefetch_placesingle.cpp:917-922`
  — out of this batch's scope; relevant prefetch batch already
  notes it (`awg_compiler_config.hpp:115` historical comment).

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:69
- used: src/compile_seqc.cpp:200; src/awg_compiler.cpp:298;
  src/prefetch_emit.cpp:64; src/prefetch_placesingle.cpp:917-922;
  src/compiler.cpp:539,544

### AWGCompilerConfig::serializeRoundTrip  [no / medium / —]

Evidence:
- `awg_compiler_config.hpp:74`
  `uint64_t serializeRoundTrip; // 0x28 — no reconstructed consumer;
  may be set by AWGCompilerImpl`
- `compiler.cpp:427` `if (config_->serializeRoundTrip == 1) { … }`
  triggers an AsmList serialize+deserialize debug round-trip.
- `notes/placeholder_field_names.md:47` records the historical
  rename from `unknown_28` and the consumer location.

Interpretation:
- One live consumer, behavior matches name (debug round-trip
  toggle). Header comment "no reconstructed consumer" is stale —
  `compiler.cpp:427` is the consumer.

Judgement:
- Not a misnomer; minor doc-comment rot only (out of audit scope).

Proposals:
- keep current  (medium)

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:74
- used: src/compiler.cpp:427
- related: notes/placeholder_field_names.md:47

### AWGCompilerConfig::optimizationFlags  [no / medium / —]

Evidence:
- `awg_compiler_config.hpp:82`
  `uint64_t optimizationFlags; // 0x88 — no reconstructed consumer;
  adjacent to debugFlags`
- `compile_seqc.cpp:204`
  `config.optimizationFlags = 0xFF; // binary hardcodes 0xFF`
- `compiler.cpp:411`
  `static_cast<uint32_t>(config_->optimizationFlags)` passed into
  the AsmOptimize ctor.
- `awg_compiler.cpp:720` documenting comment
  `Check config_->optimizationFlags (bool at config+0x88)` (the
  `(bool ...)` text is wrong about type but right about offset).
- `notes/placeholder_field_names.md:50` records the rename.

Interpretation:
- Bitmask passed to the AsmOptimize ctor; default 0xFF means "all
  passes". Name fits.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (medium)

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:82
- used: src/compile_seqc.cpp:204; src/compiler.cpp:411;
  src/awg_compiler.cpp:720 (comment only)

### AWGCompilerConfig::debugDumpEnabled / debugJsonEnabled / debugDumpPath / debugJsonPath  [no / high / —]

Evidence:
- `awg_compiler_config.hpp:75-79` declarations.
- `compiler.cpp:417-418`
  `if (config_->debugDumpEnabled) { writeFile(config_->debugDumpPath, …); }`
- `compiler.cpp:497`
  `// if (config.debugJsonEnabled) { … writeFile(debugJsonPath, …); }`
  (commented stub mirroring the dump pair).
- `compile_seqc.cpp:210` `config.debugDumpPath = filename;` (sole
  setter; presumably also flips the `_Enabled` flag, though that
  set isn't shown).
- Dtor (`awg_compiler_config.cpp:18-19,28-29`) frees the strings
  conditionally on the `_Enabled` flag.

Interpretation:
- Names accurately describe the role: each pair is (path,
  enabled-flag). Path / flag separation matches the dtor's
  conditional-free pattern.

Judgement:
- Not misnomers.

Proposals:
- keep current  (high) for all four

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:75-79
- used: src/awg_compiler_config.cpp:18-29; src/compile_seqc.cpp:210;
  src/compiler.cpp:417-418,497

### AWGCompilerConfig::compressSource  [no / medium / —]

Evidence:
- `awg_compiler_config.hpp:88`
  `bool compressSource; // 0x9D — if true, compress source sections
  in ELF (verified at 0x108f48)`
- `awg_compiler.cpp:811`
  `// @0x108f48: check config->compressSource for source embedding
  compression flag`
- No other consumers located; the field's semantic is the comment's
  description, verified at the cited binary address.

Interpretation:
- Single-bit gate for source-section compression; name fits.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:88
- used: src/awg_compiler.cpp:811 (comment-cited consumer)

### AWGCompilerConfig::wavetableSize / searchPath  [no / high / —]

Evidence:
- `awg_compiler_config.hpp:90,92` declarations.
- `awg_compiler.cpp:166-167`
  `WavetableFront(deviceConstants_, config.addressImpl,
  static_cast<size_t>(config.wavetableSize), config.searchPath)`
- `compile_seqc.cpp:213,215`
  `config.searchPath = boost::filesystem::path(configWavePath);`
  / `config.searchPath = waveSearchPath;`
- `compiler.cpp:506-508` (commented stub) mirrors the same call.

Interpretation:
- `wavetableSize` is the wavetable allocation cap (sign-extended to
  size_t for `WavetableFront`); `searchPath` is the directory in
  which `.wave` files are looked up. Both names match.

Judgement:
- Not misnomers.

Proposals:
- keep current  (high) for both

Locations consulted:
- declared: include/zhinst/awg_compiler_config.hpp:90,92
- used: src/awg_compiler.cpp:166-167; src/compile_seqc.cpp:213,215;
  src/compiler.cpp:506-508

## 4. Symbols inspected and judged routinely fine

- `AWGCompilerConfig::deviceSampleRate` — `double` at +0x08; NaN
  means "use default"; read in `static_resources.cpp:243` and
  computed against device-default elsewhere; name fits.
- `AWGCompilerConfig::addressImpl` — `uint32_t` passed verbatim
  into `AddressImpl<unsigned int>` ctor at `awg_compiler.cpp:166`;
  name fits.
- `AWGCompilerConfig::channelsPerGroup` (uint16[2]) — indexed
  `[indexed ? 1 : 0]` in `custom_functions.cpp:757`; both elements
  used; name fits.
- `AWGCompilerConfig::deviceIndex` — used in
  `custom_functions_io.cpp:416,510,515` as the device-table index;
  name fits. (`compiler.cpp:526` casts it to `size_t` for
  another lookup.)
- `AWGCompilerConfig::includePaths` — `vector<string>` of
  preprocessor include directories; standard role.
- `AWGCompilerConfig::debugFlags` — bitmask `0x02` / `0x04` / `0x08`
  for AST-print toggles, verified at `compiler.cpp:551` and at
  `awg_compiler_config.hpp:48` table comment.
- `AWGCompilerConfig::~AWGCompilerConfig()` — no parameters of
  concern; matches binary 0xf8080 dtor.
- `getAwgDeviceTypeString::type` — switched on in a jump table; the
  parameter name matches its role as the `AwgDeviceType` enum
  selector; binary-faithful method body.
- `getAwgDeviceTypeFromString::str` — `iequals` argument against
  `.rodata` codenames; standard role.
- `getChannelGroupingModeString` (no parameters) — `const` method,
  reads `deviceType` and `numChannelGroups`; both reads match
  binary disassembly comments.

## 5. Coverage

- **Fully covered:**
  - All non-padding data members of `AWGCompilerConfig` (24 fields).
  - All recon-added forwarding accessors (`appendMode()`,
    `splitIndex()`, `syncVersion()`).
  - All parameters of in-scope methods
    (`getAwgDeviceTypeString::type`,
    `getAwgDeviceTypeFromString::str`).
  - The `~AWGCompilerConfig()` dtor (no parameters).
  - Cross-batch arbitration with batches 09 and 36 on the
    `isHirzel` cluster.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type `AWGCompilerConfig` — present in mangled symbols
    (e.g. `Compiler::Compiler(AWGCompilerConfig const&, ...)`);
    excluded under §3.
  - Methods `getAwgDeviceTypeString`, `getAwgDeviceTypeFromString`,
    `getChannelGroupingModeString`, `~AWGCompilerConfig` — present
    in `nm` output as `t` symbols; excluded under §3.
  - Type `AwgDeviceType` (enum) — referenced in mangled symbols;
    its enumerators `UHFLI`/`HDAWG`/etc. are tier-2-faithful via
    `getAwgDeviceTypeString`'s `.rodata` strings (in scope but
    routinely-fine; not flagged).
  - `SampleFormat` enum — defined in `waveform_ir.hpp`, separate
    batch.
  - Padding fields (`pad_1a`, `pad_49`, `pad_69`, `pad_9c`,
    `pad_9e`, `pad_a4`) — inert layout filler, not in scope.
- **Cross-batch arbitration outcomes:**
  - `AWGCompilerConfig::isHirzel` is the cluster-#1 source of
    truth: name confirmed correct (high). Recommendation for
    synthesis on `Cache::appendMode_` (batch 36, unsure/low):
    rename to `isHirzel_` to match this anchor.
  - `Prefetch::split_` (batch 09) is correctly distinguished from
    `isHirzel` here: it is a *derived superset* triggered by
    either `appendMode()` (= `isHirzel`) OR `required <= cacheSize`.
    Both names stand on their respective sides.
  - `Prefetch::isHirzel_()` / `set_isHirzel_()` (batch 09): dead
    misnomers. The same shape recurs here as
    `AWGCompilerConfig::appendMode()` (single live caller, all
    other Hirzel branches use the field directly) — flagged for
    removal. `splitIndex()` and `syncVersion()` are companion dead
    aliases of the same shape.
- **Status:** `complete`.
