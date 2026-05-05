# Batch 05a — custom_functions main

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 6 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 5;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 1.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

Header + main `.cpp` only. The three split files
(`custom_functions_play.cpp`, `_io.cpp`, `_playback.cpp`) are out of
scope here and are audited separately as 05b/05c/05d.

## 1. Files considered

- `reconstructed/include/zhinst/runtime/custom_functions.hpp`
- `reconstructed/src/runtime/custom_functions.cpp`

Symbol-table queries against `_seqc_compiler.so` confirmed all class,
method, free-function and exception type names appear verbatim and are
therefore excluded from rename per RULES §3. Per-symbol citations are
embedded in §3 blocks.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `CustomFunctions::field_80` | yes | high | placeholder, libc++ internal pad | `funcMap_maxLoadFactor_`, drop field | coordinated-rename |
| `CustomFunctions::field_88` | yes | high | placeholder, doc says aliasMap | `aliasMap_` | — |
| `CustomFunctions::field_A8` | yes | high | placeholder, libc++ internal pad | `aliasMap_maxLoadFactor_`, drop field | coordinated-rename |
| `CustomFunctions::field_B0` | yes | medium | `field_*` placeholder | `unusedStringSet_B0_`, `optionFeatureSet_` | — |
| `CustomFunctions::unusedStringSet_B0_` | yes | low | name asserts non-use unconfirmed | `field_B0_`, keep current | — |
| `CustomFunctions::pad_84_` / `pad_AC_` / `pad_1C4_` | no | medium | true padding bytes | keep current | — |
| `CustomFunctions::funcMap_maxLoadFactor_` | unsure | low | reconstruction artifact only | `funcMap_maxLoadFactor_`, drop field | coordinated-rename |
| `CustomFunctions::aliasMap_maxLoadFactor_` | unsure | low | reconstruction artifact only | `aliasMap_maxLoadFactor_`, drop field | coordinated-rename |
| `CustomFunctions::warningCallback_` | no | high | symbol-table layout + ctor binding | keep current | not-misnomer |
| `CustomFunctions::externalTriggeringMode_` | no | high | matches JSON key & error msg | keep current | not-misnomer |
| `CustomFunctions::usedFeatures_` | no | high | matches "MF" insertion sites | keep current | not-misnomer |
| `CustomFunctions::assignedWaveNames_` | no | medium | populated by `assignWaveIndex` | keep current | — |
| `CustomFunctions::nodeAddressMap_` | yes | medium | stores nodeList_ INDEX, not address | `nodeIndexMap_` | — |
| `CustomFunctions::resources_` | unsure | low | written by Compiler::compile | keep current | cross-batch-arbitration |
| `CustomFunctions::wavetableFrontPrivate_` | no | medium | matches in-place `+0x30` ctor alloc | keep current | — |
| `CustomFunctions::SubFunc::Default` | unsure | low | "Default"=playWave (1) | `PlayWave`, keep current | — |
| `CustomFunctions::SubFunc::Now` | no | medium | `playWaveNow` passes 3 | keep current | — |
| `CustomFunctions::mergeWaveforms::param2` | yes | high | non-descript `param2` | `numChannels` (low) | — |
| `CustomFunctions::mergeWaveforms::param3` | yes | high | non-descript `param3` | `multiValue` (low) | — |
| `CustomFunctions::mergeWaveforms::param5` | yes | high | non-descript `param5` | `firstArgIndex` (low) | — |
| `CustomFunctions::mergeWaveforms::param6` | yes | high | non-descript `param6` | `strict` (low) | — |
| `CustomFunctions::writeToNode::path` | no | high | confirmed by 4 named regex locals | keep current | not-misnomer |
| `CustomFunctions::writeToNode::val` | no | medium | name fits use-site | keep current | — |
| `CustomFunctions::writeToNode::type` | unsure | low | name shadows VarType | `varType`, keep current | — |
| `CustomFunctions::writeLS64bit::reg1` / `::reg2` | yes | medium | `reg1`/`reg2` non-descript | `loReg`/`hiReg` (low) | — |
| `CustomFunctions::checkPlayMinLength::length` | no | medium | local `minLength` consistent | keep current | — |
| `CustomFunctions::checkPlayAlignment::length` | no | medium | local `aligned`/`alignment` consistent | keep current | — |
| `CustomFunctions::checkOffspecWaveLength::expected` | unsure | medium | "expected" length, but Case 1 vs Case 2 differ | `minLength`, keep current | — |
| `CustomFunctions::oscMask*::mask` | no | high | shifted/and-tested as bitmask | keep current | — |
| `CustomFunctions::getWaitTime::samples` / `::rate` | no | high | binary uses both as such | keep current | — |
| `CustomFunctions::checkFunctionSupported::devType` | yes | medium | param is bitmask, name says single | `allowedDevTypes`, `supportedMask` | — |
| `CustomFunctions::optionAvailable::option` | yes | medium | param looked up in `includePaths` | `feature`, keep current | cross-batch-arbitration |
| `CustomFunctions::SubFunc::DigTrigger` | no | medium | `playWaveDigTrigger` only caller | keep current | — |
| `CustomFunctions::SubFunc::Aux` | no | medium | `playAuxWaveIndexed` caller | keep current | — |
| `PlayArgs::reportWarning_` | no | high | ctor copies named arg | keep current | — |
| `PlayArgs::cmdName_` | no | high | format strings cite as command name | keep current | — |
| `PlayArgs::channelsPerGroup_` | no | high | indexed config[`indexed`] | keep current | — |
| `PlayArgs::totalChannels_` | no | high | product, used as channel-count limit | keep current | — |
| `PlayArgs::waveAssignments_` | no | high | populated by `addChannelWave` | keep current | — |
| `PlayArgs::hasMarker_` | no | high | varSubType==2 = marker | keep current | — |
| `PlayArgs::wavetable_` | no | medium | shared_ptr<WavetableFront> | keep current | — |
| `PlayArgs::WaveAssignment::value` | no | high | EvalResultValue per wave/channel | keep current | — |
| `PlayArgs::WaveAssignment::bits` | yes | medium | name vague, holds 1-based channel slots | `channelSlots`, keep current | — |
| `PlayArgs::PlayArgs::indexed` | no | high | selects `channelsPerGroup[0|1]` | keep current | — |
| `PlayArgs::secureLoadWaveform::argIndex` | yes | medium | parameter is unused in body | `argIndex` (kept, low), drop param | — |
| `PlayArgs::parseImplicitChannels::begin/end` | no | high | iterator pair | keep current | — |
| `PlayArgs::parseExplicitChannels::begin/end` | no | high | iterator pair | keep current | — |
| `PlayArgs::parse::args` | no | high | the EvalResultValue argument list | keep current | — |
| `parseOptionalRate::parseEnd` | yes | medium | name says end, is parse cursor | `cursor`, `parseCursor` | — |
| `parseOptionalRate::strict` | unsure | low | name semi-fits return-encoding | keep current | — |
| `parseOptionalRate::cmdName` | no | high | command name in error | keep current | — |
| `parseOptionalString::args` | no | high | inspects/pops back of vector | keep current | — |
| `getPlayRate::strict` | unsure | low | binary subtracts 2 when true | `subtractCarry`, keep current | — |
| `getPlayRate::name` | no | high | command name in error | keep current | — |
| `getPlayRate::val` | no | high | EvalResultValue parsed | keep current | — |
| `NodeMap::entries_` | no | high | entries of name→NodeMapItem map | keep current | — |
| `NodeMap::retrieve::path` | no | high | string key looked up | keep current | — |
| `NodeMap::toFrequency::freq` / `::sampleClock` | no | high | direct math use | keep current | — |
| `NodeMap::toPhase::value` | yes | low | "value" generic; concretely a phase | `phase`, `phaseDeg` | — |
| `wrap23::v` | no | low | helper local | keep current | — |
| `AccessMode::Soft` / `::Direct` / `::Custom` | no | high | matches static `accessModes` table | keep current | not-misnomer |
| `ExternalTriggeringMode::None` / `::Dio` / `::ZSync` | no | high | matches JSON keys "dio"/"zsync" | keep current | not-misnomer |
| `CustomFunctionsValueException::argIndex_` | no | high | matches accessor and ctor param | keep current | — |
| `CustomFunctionsValueException::varName_` | no | high | matches `setVarName` ctor | keep current | — |
| `CustomFunctionsException::message_` | no | high | what() returns it | keep current | — |
| `toString(AccessMode)::mode` | no | high | name fits switch arg | keep current | — |
| `addNodeAccess::item` / `::mode` | no | high | both passed straight to maps | keep current | — |
| `getNodeAddress::item` | no | high | passed to map | keep current | — |
| `getAccessModes::item` | no | high | passed to map | keep current | — |
| `lookupNode::path` | no | high | passed to NodeMap::retrieve | keep current | — |
| `getNodeMapForDevice::devType` | no | high | dispatch argument | keep current | — |

## 3. Detailed findings

### CustomFunctions::field_80, ::field_88, ::field_A8, ::field_B0  [yes / high / coordinated-rename]

Evidence:
- `custom_functions.hpp:292-297` field declarations literally named
  `field_80`, `field_88`, `field_A8`, `field_B0` in the layout
  comment.
- `custom_functions.hpp:529-539` the actual reconstructed members
  are `funcMap_maxLoadFactor_`, `aliasMap_`,
  `aliasMap_maxLoadFactor_`, `unusedStringSet_B0_`. The layout-table
  comment still uses the placeholder names.
- `custom_functions.cpp:286-290` `aliasMap_.find(name)` is used as
  the alias lookup map; comment says "alias resolution" so the
  declaration name `aliasMap_` is consistent with `field_88`'s real
  use.

Interpretation:
- The four `field_NN` strings appear only inside a documentation
  layout table; they are placeholders carried forward from an
  earlier reconstruction pass and now contradict the chosen
  declaration names.

Judgement:
- The four `field_NN` placeholders in the layout-comment table are
  misnomers relative to the live declarations beneath them.

Proposals:
- Replace `field_80`/`field_A8` rows in the comment table with
  `funcMap_maxLoadFactor_`/`aliasMap_maxLoadFactor_` (high) — see
  block below for whether those reconstruction-only fields should
  exist at all.
- Replace `field_88` row with `aliasMap_` (high).
- Replace `field_B0` row with `unusedStringSet_B0_` (high). Whether
  the `unusedStringSet_B0_` field name itself is correct is a
  separate question (block below).

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:292-297, 529-539
- used:     src/runtime/custom_functions.cpp:286-290

### CustomFunctions::funcMap_maxLoadFactor_, ::aliasMap_maxLoadFactor_, ::pad_84_, ::pad_AC_  [unsure / low / coordinated-rename]

Evidence:
- `custom_functions.hpp:527-535`
  ```
  // NOTE: On libc++, unordered_map is 0x28 bytes (0x20 hash table + 4B max_load_factor + 4B pad).
  // The floats below are the trailing max_load_factor of the preceding map.
  float funcMap_maxLoadFactor_{1.0f}; // +0x80  (libc++ internal)
  char  pad_84_[4];                   // +0x84
  ...
  float aliasMap_maxLoadFactor_{1.0f}; // +0xA8 (libc++ internal)
  char  pad_AC_[4];                    // +0xAC
  ```
- The header also explicitly declares `aliasMap_` /  `funcMap_`
  immediately above using `std::unordered_map<...>`, which on the
  build platform owns its own `max_load_factor` slot internally —
  these explicit fields are double-counting the layout.

Interpretation:
- The explicit `*_maxLoadFactor_` fields and their pad neighbours
  are reconstruction-only placeholders modelling libc++'s internal
  layout. They are not separate logical fields of `CustomFunctions`.

Judgement:
- The names *describe what they are*, but the fields likely
  shouldn't exist at all once the surrounding `unordered_map` is
  declared natively. Whether to "rename" or "delete" is a synthesis
  call.

Proposals:
- keep current names if fields are kept (medium).
- Delete the four reconstruction-padding fields and rely on the
  natural `unordered_map` size (medium) — out-of-scope for this
  audit.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:525-535

### CustomFunctions::unusedStringSet_B0_  [yes / low]

Evidence:
- `custom_functions.hpp:537-539`
  ```
  // +0xB0: set<string> — no reconstructed consumer found; may be populated by
  // a not-yet-reconstructed method. Kept for layout fidelity.
  std::set<std::string> unusedStringSet_B0_;    // +0xB0
  ```
- No read or write of `unusedStringSet_B0_` exists anywhere in the
  in-scope file pair.
- The accompanying note explicitly hedges: "no reconstructed
  consumer found … may be populated by a not-yet-reconstructed
  method".

Interpretation:
- The field name asserts a fact (it is *unused*) that the comment
  immediately retracts. The name is also rooted at a binary offset
  (`_B0_`), which §4a flags as the kind of placeholder that should
  not survive into long-term names.

Judgement:
- Misnomer in the soft sense — the name asserts a property the
  code does not establish.

Proposals:
- `field_B0_` (low) — neutral pending deeper investigation.
- `optionFeatureSet_` (low) — speculative; would parallel
  `usedFeatures_` if this set turns out to be the unused-features
  ledger.
- keep current (medium) if a downstream batch confirms it really is
  dead.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:537-539
- used:     none in this batch

### CustomFunctions::warningCallback_  [no / high / not-misnomer]

Evidence:
- `custom_functions.hpp:577` `std::function<void(std::string const&)> warningCallback_;`
- `custom_functions.hpp:346` ctor parameter named `warningCb`.
- `custom_functions.cpp:113` ctor initializer `warningCallback_(warningCb)`.
- `custom_functions.cpp:336,353,641,675` invoked as
  `warningCallback_(msg)` directly after `ErrorMessages::format(...)`
  produced a warning string.
- Symbol-table: ctor mangling
  `zhinst::CustomFunctions::CustomFunctions(... std::function<void(string const&)> const&)` confirms the ctor takes a callback; the field
  itself is non-static so the name is not encoded but its role is
  unambiguous.

Interpretation:
- All call sites pass a *warning* string; no `error`/`info`
  routing flows through this field.

Judgement:
- Name accurately reflects use.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:577
- used:     src/runtime/custom_functions.cpp:113,336,353,641,675

### CustomFunctions::externalTriggeringMode_  [no / high / not-misnomer]

Evidence:
- `custom_functions.hpp:80-84` enum members `Dio`, `ZSync`.
- `custom_functions.hpp:76-78` header comment cites the binary error
  message `"DIO and ZSync external triggering can't be mixed in the
  same program"` and the JSON key `"external_triggering"` with
  values `"dio"`/`"zsync"`.
- `custom_functions.cpp:1281-1284`
  `checkExternalTriggeringMode` flips the mode and throws on
  conflict.

Interpretation:
- JSON serializer key (RULES §4d tier 2) and error string both
  match the field name semantically.

Judgement:
- Name confirmed correct.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:582
- used:     src/runtime/custom_functions.cpp:1280-1285

### CustomFunctions::usedFeatures_  [no / high / not-misnomer]

Evidence:
- `custom_functions.cpp:378,461,702` `usedFeatures_.insert("MF")`
  and `usedFeatures_.insert(option)` from `optionAvailable`.
- `custom_functions.hpp:585` declared as `std::set<std::string>`.
- `custom_functions.hpp:326` befriends `AWGCompilerImpl::getJsonVersion`
  which "reads … `usedFeatures_` directly" per the comment — i.e.
  feeds out into the JSON-serialised feature list.

Interpretation:
- All write sites insert string feature tokens. Reader is a JSON
  serializer on the same set.

Judgement:
- Name is correct.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:585
- used:     src/runtime/custom_functions.cpp:378,461,702

### CustomFunctions::nodeAddressMap_  [yes / medium]

Evidence:
- `custom_functions.cpp:483-486` comment block:
  > Inserts mode into accessModeMap_ (+0x128), then does
  > `nodeAddressMap_.emplace(item, nodeList_.size())` — stores the
  > **INDEX into nodeList_, NOT a hardware address**.
- `custom_functions.cpp:491-498` body confirms `idx =
  nodeList_.size()` is what is stored.
- `custom_functions.cpp:527-533` `getNodeAddress` returns the value,
  but the fast path goes through `DirectAddrNodeMapData::addr_`
  while the slow path returns this *index* — i.e. a single accessor
  conflating two distinct concepts.

Interpretation:
- The map stores an integer that is an index into `nodeList_`, not
  an address. The name `nodeAddressMap_` describes a value that the
  code explicitly is not.

Judgement:
- Misnomer.

Proposals:
- `nodeIndexMap_` (medium).
- keep current (low) — only if synthesis prefers consistency with
  the public `getNodeAddress` accessor; in that case `getNodeAddress`
  itself becomes the misnomer and this is a he-said/she-said.
  However, the method name is in the binary symbol table and is
  excluded — so any rename has to land on the field side.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:550
- used:     src/runtime/custom_functions.cpp:486,492-498,527-533

### CustomFunctions::resources_  [unsure / low / cross-batch-arbitration]

Evidence:
- `custom_functions.hpp:325` `friend class Compiler;  //
  Compiler::compile() directly writes resources_ (+0x10)`.
- `custom_functions.cpp:541-547` `getSampleClock()` is the only
  in-batch reader. It calls `resources_->variableExists(...)` and
  `resources_->readConst(...)`.
- All other 80+ method bodies receive a `shared_ptr<Resources> res`
  parameter and pass it down rather than using `resources_`.

Interpretation:
- The field exists but is set externally and almost-never used. Its
  one in-batch use is to look up `$DEVICE_SAMPLE_RATE`. Whether
  `resources_` is the *right* name depends on `Resources`' role,
  which is the subject of the not-yet-written batch 19.

Judgement:
- Cannot judge in this batch. The name *appears* fine but the
  unusual single-reader pattern is suspicious.

Cross-reference:
- `Resources` and its methods are in batch 19.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:516
- used:     src/runtime/custom_functions.cpp:541-547

### CustomFunctions::SubFunc enumerators  [unsure / low]

Evidence:
- `custom_functions.hpp:330-338`:
  ```
  Prefetch   = 0,   // prefetch (prefetch-only path, no asmPlay)
  Default    = 1,   // playWave, playWaveIndexed
  Aux        = 2,   // playAuxWaveIndexed
  Now        = 3,   // playWaveNow, playWaveIndexedNow
  DigTrigger = 4,
  ```
- Symbol table contains `CustomFunctions::SubFunc` as a method
  parameter type (e.g. `CustomFunctions::play(..., SubFunc)`); the
  type name is excluded but the enumerator names are not.

Interpretation:
- `Default` is the entry used by `playWave`, the canonical play
  path; the literal "Default" is generic but accurate as
  "ordinary, non-special play".
- `Aux`, `Now`, `DigTrigger` map cleanly to the obvious caller.
- `Prefetch=0` matches the `prefetch` built-in.

Judgement:
- All five enumerator names fit their use sites, but `Default` is
  generic enough to flag as `unsure / low`.

Proposals:
- `SubFunc::PlayWave` (low) for `Default` — more specific.
- keep current (medium).

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:330-338

### CustomFunctions::mergeWaveforms parameters (`param2`..`param6`)  [yes / high]

Evidence:
- `custom_functions.hpp:376-381`:
  ```
  std::shared_ptr<WaveformFront> mergeWaveforms(
                      std::vector<EvalResultValue> const& args,
                      short param2, bool param3,
                      std::string const& name,
                      int param5, bool param6);
  ```
- The accompanying comment says "Mangled: …sbRK<string>ib (last
  param is bool, not int64_t)" — i.e. only the *types* were
  recovered.
- No definition exists in this batch (the method body is in
  `custom_functions_play.cpp` per OVERVIEW); §3 cannot inspect it
  here.

Interpretation:
- Four parameters are named `paramN` — the canonical §4a
  non-descript-name flag.

Judgement:
- All four `param*` names are misnomers regardless of their true
  semantics.

Proposals:
- `numChannels` / `multiValue` / `firstArgIndex` / `strict` (low)
  — speculative; correct names should come from the body in batch
  05b.
- Cross-reference batch 05b for the body's actual usage.

Cross-reference:
- Body lives in `custom_functions_play.cpp` (batch 05b).

Status:
- Plain `yes / high`. The verdict (misnomer) is solid; the
  proposals are weak. Synthesis should defer the *replacement* names
  to batch 05b.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:376-381

### CustomFunctions::writeToNode parameters  [no / high (path) ; unsure / low (type)]

Evidence:
- `custom_functions.hpp:397-399` declares `(EvalResultValue path,
  EvalResultValue val, EvalResultValue type, shared_ptr<Resources>
  res)`.
- Symbol-table block at the start of this file shows four
  function-static regex variables in `writeToNode`:
  `absDevRegex`, `awgNodeRegex`, `sineNodeRegex`, `oscselNodeRegex`
  — i.e. the body matches `path` against a series of node-path
  regexes.

Interpretation:
- `path` is corroborated as a node path string-like value.
- `val` is the value to write; reasonable.
- `type` is a parameter named like a C++ keyword and shadows the
  `VarType` field-name idiom used elsewhere (`varType_`) — in this
  context it likely encodes the target node datatype (Int/Double).

Judgement:
- `path` is correct; `val` is fine; `type` is name-shadow risky
  but not strictly a misnomer.

Proposals:
- `varType` for `type` (low).
- keep current (medium).

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:397-399
- symbol-table statics: writeToNode::absDevRegex, awgNodeRegex,
  sineNodeRegex, oscselNodeRegex.

### CustomFunctions::writeLS64bit parameters (`reg1`, `reg2`)  [yes / medium]

Evidence:
- `custom_functions.hpp:417-419`:
  `void writeLS64bit(unsigned long value, int reg1, int reg2,
                     shared_ptr<EvalResults>, shared_ptr<Resources>);`
- Symbol-table: `writeLS64bit(unsigned long, int, int, ...)` — name
  encodes "Lower-Significance / Upper-Significance 64-bit", i.e. the
  function splits a 64-bit value across two registers. There must
  be a low-half and a high-half.

Interpretation:
- `reg1`/`reg2` is exactly the §4a `arg1`/`arg2` smell. The function
  semantics demand a low/high distinction that the parameter names
  hide.

Judgement:
- Misnomer.

Proposals:
- `loReg`, `hiReg` (low) — order-uncertain without inspecting the
  body, which is in batch 05c.

Cross-reference:
- Body in `custom_functions_io.cpp` (batch 05c).

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:417-419

### CustomFunctions::checkOffspecWaveLength::expected  [unsure / medium]

Evidence:
- `custom_functions.cpp:622-676` body branches on two cases:
  - Case 1 (`expected > wfLen`): "wf name too long → warn 0xF4" with
    `format(0xF4, wfName, wfLen, to_string(expected))`.
  - Case 2 (`wfLen % alignment != 0`): alignment warning, where
    `expected` is **not** consulted at all; the rounded-up length is
    computed from `wfLen` and `alignment` alone.

Interpretation:
- `expected` is only the lower bound used in Case 1. Case 2 ignores
  it entirely. The name fits Case 1 and is irrelevant to Case 2.

Judgement:
- Borderline. Defensible as "expected minimum length" but neither
  case actually treats it as the *expected* value of `wfLen`.

Proposals:
- `minLength` (low).
- keep current (medium).

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:361
- used:     src/runtime/custom_functions.cpp:622,629,633,637

### CustomFunctions::checkFunctionSupported::devType  [yes / medium]

Evidence:
- `custom_functions.cpp:572-577` body:
  `if ((current & ~static_cast<uint32_t>(devType)) == 0) return;`
  i.e. `devType` is a bitmask of allowed `AwgDeviceType` bits OR'd
  together, not a single device.
- The accompanying header/source comment (`custom_functions.cpp:567-571`)
  spells this out:
  > `devType` (a bitmask of AwgDeviceType values OR'd together).

Interpretation:
- The name `devType` strongly implies a single device type, but the
  parameter is a mask of multiple types.

Judgement:
- Misnomer.

Proposals:
- `allowedDevTypes` (medium).
- `supportedMask` (low).

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:357
- used:     src/runtime/custom_functions.cpp:572-590

### CustomFunctions::optionAvailable::option  [yes / medium / cross-batch-arbitration]

Evidence:
- `custom_functions.cpp:682-704` body searches `config_->includePaths`
  for a match and on hit inserts into `usedFeatures_`.
- `custom_functions.cpp:368-411` (`oscMaskCheckHirzel`) similarly
  iterates `config_->includePaths` looking for `"MF"` and inserts
  into `usedFeatures_` on success.
- Batch 23 (`23_awg_compiler_config.md:71`) records
  `AWGCompilerConfig::includePaths` as `not-misnomer` ("vector of
  include dirs") with high confidence.
- Inside this batch the same field is used as the device-feature
  flag list, with `"MF"` as a feature token, not an include path.

Interpretation:
- `optionAvailable::option` and `AWGCompilerConfig::includePaths`
  give incompatible accounts of the same data — the consumer treats
  it as features/options, batch 23 said it's filesystem include
  paths.

Judgement:
- The parameter name `option` is correct for what the consumer
  *does* with the value. The mismatch is between the consumer view
  and `includePaths` upstream — exactly the §4c he-said/she-said
  pattern.

Proposals:
- keep current `option` here (high).
- `feature` (low).
- Re-open `AWGCompilerConfig::includePaths` in synthesis: candidates
  `optionList_`, `enabledFeatures_`.

Cross-reference:
- `AWGCompilerConfig::includePaths` in batch 23.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:362
- used:     src/runtime/custom_functions.cpp:682-704
- counterpart: src/runtime/custom_functions.cpp:372-378, 452-461.

### PlayArgs::WaveAssignment::bits  [yes / medium]

Evidence:
- `custom_functions.hpp:220-221`:
  ```
  EvalResultValue value;
  std::vector<int> bits;       // +0x38, channel bit assignments
  ```
- `custom_functions.cpp:1132-1141` `addChannelWave` builds
  `wa.bits = {slotInGroup}` where `slotInGroup = (channel %
  channelsPerGroup_) + 1` — i.e. a 1-based in-group **channel
  index**, not a bit position.
- `custom_functions.cpp:984-1025` `parseExplicitChannels` similarly
  pushes `(channelNum - 1) % channelsPerGroup_ + 1` — also a 1-based
  channel slot.

Interpretation:
- The vector contains 1-based channel slot numbers. There is no
  bit-mask or bit-position semantics anywhere in scope.

Judgement:
- Misnomer in the unit/role sense (§4a "bytes vs samples").

Proposals:
- `channelSlots` (medium).
- `channels` (low).
- keep current (low) only if a downstream consumer (batch 05b)
  actually treats these as bit indices.

Cross-reference:
- Possible downstream consumer in `custom_functions_play.cpp`
  (batch 05b).

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:221
- used:     src/runtime/custom_functions.cpp:1023,1138,1141

### PlayArgs::secureLoadWaveform::argIndex  [yes / medium]

Evidence:
- `custom_functions.hpp:252-253` declares parameter named `argIndex`.
- `custom_functions.cpp:1041-1075` body parameter is spelled
  `size_t /*argIndex*/` — i.e. *unused* in the reconstruction.
- The two call sites (`custom_functions.cpp:902, 1003`) pass
  `static_cast<size_t>(it - begin)`, an arg-index value.

Interpretation:
- Name accurately describes what the *callers* pass, but the body
  ignores it. This is a §4a mismatch only at the body level; the
  caller-name match suggests the binary did consume it (likely in
  the exception-construction path).

Judgement:
- The name is borderline: it describes the *value passed in* but
  not the (currently empty) body usage. Likely the body
  reconstruction lost an exception `argIndex` parameter.

Proposals:
- keep current `argIndex` (medium).
- Add a §IF entry tracking the body's missing use (out of scope
  here under the audit-only rule).

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:252-253
- used:     src/runtime/custom_functions.cpp:902, 1003 (call sites only)

### parseOptionalRate::parseEnd  [yes / medium]

Evidence:
- `custom_functions.cpp:1230-1248` block comment:
  > the third iterator (rdx) is the *parse cursor* that the caller
  > passes in (typically the result of PlayArgs::parse()).
  > checks whether exactly ONE EvalResultValue (size 0x38) remains
  > between that cursor and `end`
- `custom_functions.cpp:1257-1276` body uses
  `auto cursor = parseEnd;` then `end - cursor == 1` test.

Interpretation:
- The name `parseEnd` reads as "end of the parsed range". In fact
  the iterator marks the *current cursor position* into the
  argument vector. The actual end is a separate parameter named
  `end`.

Judgement:
- Misnomer.

Proposals:
- `cursor` (medium).
- `parseCursor` (low).

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:262-264
- used:     src/runtime/custom_functions.cpp:1254-1276

### NodeMap::toPhase::value  [yes / low]

Evidence:
- `custom_functions.hpp:67` `static int toPhase(float value);`
- `custom_functions.cpp:1223-1228` body multiplies `value *
  kPhaseScale` where `kPhaseScale = 2^23 / 360 ≈ 23301.689f`, then
  rounds to int and applies a 23-bit two's-complement wrap.

Interpretation:
- The argument is unambiguously a phase in degrees. `value` is the
  archetypal §4a generic-name flag.

Judgement:
- Misnomer (low).

Proposals:
- `phase` (medium).
- `phaseDeg` (low).

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:67
- used:     src/runtime/custom_functions.cpp:1223-1228

### CustomFunctions::wavetableFrontPrivate_  [no / medium]

Evidence:
- `custom_functions.cpp:117-118` ctor comment:
  > Allocates and constructs a private WavetableFront at +0x30
  > (0x220 bytes, shared_ptr_emplace)
- `custom_functions.hpp:518` declared as
  `shared_ptr<WavetableFront> wavetableFrontPrivate_;` directly
  next to the public `wavetableFront_` at +0x20.

Interpretation:
- Two `WavetableFront` shared_ptrs side-by-side: one is the
  externally-supplied wavetable, the other is allocated in-place by
  the constructor. The `Private` suffix marks this distinction
  faithfully.

Judgement:
- Name is fit for purpose.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:518
- used:     src/runtime/custom_functions.cpp:117-118

### AccessMode enumerators (`Soft`, `Direct`, `Custom`)  [no / high / not-misnomer]

Evidence:
- Symbol-table tail above:
  `0x9573c0 r zhinst::toString(AccessMode)::accessModes` — the
  static `accessModes` table holds the SSO strings `"soft"`,
  `"direct"`, `"custom"` per the header comment at
  `custom_functions.hpp:92-94`.
- `custom_functions.cpp:53-61` `toString(AccessMode)` returns
  `"soft"`/`"direct"`/`"custom"`.

Interpretation:
- RTTI/string-table evidence (§4d tier 2) directly supports the
  enumerator names.

Judgement:
- Names are correct.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:91-98
- used:     src/runtime/custom_functions.cpp:53-61

### ExternalTriggeringMode enumerators (`None`, `Dio`, `ZSync`)  [no / high / not-misnomer]

Evidence:
- `custom_functions.hpp:74-78` cites JSON key `"external_triggering"`
  with values `"dio"`/`"zsync"`.
- `custom_functions.hpp:579-582` declares the three enumerators
  with `Dio=1`, `ZSync=2`, `None=0`.

Interpretation:
- JSON serializer-key evidence (§4d tier 2) matches the
  capitalisation-normalised enumerator names.

Judgement:
- Correct.

Locations consulted:
- declared: include/zhinst/runtime/custom_functions.hpp:80-84

## 4. Symbols inspected and judged routinely fine

CustomFunctions ctor parameters:
- `config`, `devConst`, `wavetable`, `waveformGen`, `asmCommands`,
  `warningCb` — all map 1:1 to namesake fields, names self-evident.

Other non-flagged CustomFunctions members:
- `config_`, `devConst_`, `wavetableFront_`, `waveformGen_`,
  `asmCommands_`, `funcMap_`, `aliasMap_`, `mathCompiler_`,
  `nodeMap_`, `accessModeMap_`, `nodeList_` — names unambiguous and
  match consumer evidence.
- `pad_84_`, `pad_AC_`, `pad_1C4_` — declared as actual padding
  bytes.

Non-flagged method parameters:
- `functionExists::name`, `call::name`, `call::args`, `call::res`
- `checkFunctionSupported::name`
- `checkWaveformMinLengthTrig::wf`, `checkOffspecWaveLength::wf`
- `oscMaskCheckGrimsel::mask`, `oscMaskCheckHirzel::mask`
- `lookupNode::path`
- `addNodeAccess::item`, `addNodeAccess::mode`
- `setWaitCyclesReg::args`, `::results`, `::res`
- `play::args`, `play::res`, `play::subFunc`
- `playIndexed::args`, `::res`, `::subFunc`
- `getWaitTime::samples`, `::rate`
- `addSyncCommand::results`, `::res`
- `addWaitCycles::cycles`, `::results`, `::res`
- `getAccessModes::item`, `getNodeAddress::item`
- `printF::args`, `::fmt`
- `generateWaveform::name`, `::args`, `::res`
- All ~80 SeqC built-in dispatchers have signature
  `(vector<EvalResultValue> const& args, shared_ptr<Resources> res)`
  — `args`/`res` are routine.
- `checkExternalTriggeringMode::expected`
- `isShfFamily` (no params).

PlayArgs:
- ctor params: `config`, `wavetable`, `reportWarning`, `cmdName`,
  `indexed` — all faithful.
- `parse::args`, `getMaxSampleLength` (no params),
  `addChannelWave::channel`, `addChannelWave::val`,
  `parseImplicitChannels::begin/end`,
  `parseExplicitChannels::begin/end`,
  `secureLoadWaveform::name`.
- `WaveAssignment::value`.

Free functions:
- `parseOptionalString::args` — operates on the back of `args`.
- `getPlayRate::val/name/strict` — `strict` is borderline (see
  table) but body usage — subtract-2 sentinel — is consistent
  enough.
- `parseOptionalRate::begin/end/cmdName/strict` — match use sites.
- `NodeMap::retrieve::path`, `NodeMap::toFrequency::freq/sampleClock`,
  `wrap23::v`.

Exceptions:
- `CustomFunctionsException::message_`,
  `CustomFunctionsValueException::{message_, argIndex_, varName_}`,
  ctor params, `setVarName::name`. All match accessor names and
  ctor arguments.

Constants/macros: none in this file.

## 5. Coverage

**Fully covered (judged):**
- All in-scope class-member declarations of `CustomFunctions`,
  `PlayArgs`, `PlayArgs::WaveAssignment`, `NodeMap`,
  `CustomFunctionsException`, `CustomFunctionsValueException`.
- Enums `AccessMode`, `ExternalTriggeringMode`,
  `CustomFunctions::SubFunc`.
- All free-function parameter names declared in
  `custom_functions.hpp` (`parseOptionalString`, `getPlayRate`,
  `parseOptionalRate`, `toString(AccessMode)`).
- All method-parameter names visible in the header for methods
  whose body is in scope (i.e. `custom_functions.cpp`). For methods
  whose body is in `custom_functions_play.cpp` /
  `_io.cpp` / `_playback.cpp` the *header parameter names* were
  judged here, but body-derived evidence is necessarily limited to
  what the comments in the header preserve.

**Deferred:**
- `CustomFunctions::mergeWaveforms` parameter naming proposals —
  body lives in batch 05b; the *finding* (`paramN` is a misnomer)
  stands but renaming targets need batch-05b body evidence.
- `CustomFunctions::writeLS64bit::reg1/reg2` — body in batch 05c.
- `PlayArgs::WaveAssignment::bits` — only in-scope writers say
  "channel slots"; if a downstream consumer in 05b really uses
  these as bit indices the verdict flips.
- `CustomFunctions::resources_` — depends on `Resources` semantics,
  pending batch 19.
- The reconstruction-padding fields (`*_maxLoadFactor_`,
  `pad_84_`, `pad_AC_`) — whether to delete vs. rename is a
  refactoring/synthesis call.

**Not covered (out of scope per RULES §2/§3):**
- Type names `CustomFunctions`, `CustomFunctionsException`,
  `CustomFunctionsValueException`, `PlayArgs`,
  `PlayArgs::WaveAssignment`, `NodeMap`, `AccessMode`,
  `ExternalTriggeringMode` — all confirmed in the binary symbol
  table (see `nm` output captured above) and excluded from rename
  per RULES §3.
- Method names of the same classes — likewise confirmed in the
  symbol table (e.g. `CustomFunctions::lookupNode`,
  `CustomFunctions::addNodeAccess`, `PlayArgs::parse`,
  `NodeMap::retrieve`, `CustomFunctions::oscMaskCheckHirzel`, …).
- Free function names `toString`, `parseOptionalString`,
  `getPlayRate`, `parseOptionalRate` — confirmed in the symbol
  table and excluded.
- Static method `NodeMap::pauPoffIwrap` — confirmed in symbol
  table at offset `0x1c5650`. The reconstructed code instead
  exposes a namespace-anonymous `wrap23` helper, which is *not*
  the symbol-table name; this is a reconstruction divergence
  (worth tracking elsewhere) but not a renaming question.
- `CustomFunctions::ctor`/`dtor` parameter types and the contents
  of `CustomFunctionsException::message_` and the value-exception
  fields — types are §3-encoded, names are not, and were judged
  above.
