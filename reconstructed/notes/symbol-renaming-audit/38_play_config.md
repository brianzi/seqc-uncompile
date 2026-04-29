# Batch 38 — play_config

## 1. Files considered

- `reconstructed/include/zhinst/play_config.hpp`
- `reconstructed/src/play_config.cpp`

Cross-file usage surveyed in: `prefetch.cpp`, `prefetch_emit.cpp`,
`prefetch_helpers.cpp`, `prefetch_placesingle.cpp`,
`prefetch_splitplay.cpp`, `prefetch_prepare.cpp`, `asm_commands.cpp`,
`custom_functions_io.cpp`, `node.cpp`, `prefetch.hpp`,
`asm_commands.hpp`.

Symbol-table verification (`nm --demangle _seqc_compiler.so | grep -i
playconfig`):

- Type `zhinst::PlayConfig` — appears in mangled symbols of
  `Node::Node`, `vector<PlayConfig>::push_back`, etc. → **excluded**
  (§3, tier 1).
- Methods `PlayConfig::encodeCwvf(int) const`,
  `PlayConfig::operator!=`, `PlayConfig::toJson() const`,
  `PlayConfig::fromJson(boost::json::value const&)` — present in
  symbol table → **excluded**.
- Static data members `channelsShift`, `rateShift`, `suppressShift`,
  `fourChannelShift`, `defaultRateShift`, `dummyShift`,
  `markerBitsShift`, `triggerShift`, `precompFlagShift`, the matching
  `*Mask` constants, and `holdSuppressExceptSigouts` — all appear in
  the BSS section of `nm` output (`b zhinst::PlayConfig::dummyShift`
  etc.) → **excluded** (§3 — they have external linkage as
  ODR-used static-constexpr members and are present verbatim).
- JSON-key strings `channelMask`, `is4Channel`, `markerBits`,
  `precompFlags` confirmed in `strings` output → **tier-2 positive
  evidence** for the matching member names.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `PlayConfig::channelMask` (data member) | no | high | binary JSON key match | keep current (high) | not-misnomer |
| `PlayConfig::rate` (data member) | no | medium | usage and JSON key match | keep current (high) | not-misnomer |
| `PlayConfig::suppress` (data member) | no | medium | usage and JSON key match | keep current (high) | — |
| `PlayConfig::is4Channel` (data member) | no | high | binary JSON key + consumer match | keep current (high) | not-misnomer |
| `PlayConfig::markerBits` (data member) | no | high | binary JSON key match | keep current (high) | not-misnomer |
| `PlayConfig::trigger` (data member) | no | medium | usage matches name | keep current (high) | — |
| `PlayConfig::precompFlags` (data member) | no | high | binary JSON key match | keep current (high) | not-misnomer |
| `PlayConfig::now` (data member) | unsure | low | consumers read as 4-ch flag | keep current; rename per genPlayConfig audit | cross-batch-arbitration |
| `PlayConfig::hold` (data member) | no | medium | usage and JSON key match | keep current (high) | — |
| `PlayConfig::dummy` (data member) | no | medium | usage and JSON key match | keep current (high) | — |
| `PlayConfig::encodeCwvf::defaultRate` (param) | no | medium | matches semantics | keep current (high) | — |
| `PlayConfig::encodeCwvf(static)::config` (param) | no | high | trivial forwarder | keep current (high) | — |
| `PlayConfig::encodeCwvf(static)::defaultRate` (param) | no | medium | matches semantics | keep current (high) | — |
| `PlayConfig::encodeCwvf::dummyFlag` (local) | yes | low | half driven by `hold` | dummyMode, holdOrDummy, keep current | — |
| `PlayConfig::encodeCwvf::channels` (local) | no | medium | name fits role | keep current (high) | — |
| `PlayConfig::encodeCwvf::effectiveRate` (local) | no | medium | name fits role | keep current (high) | — |
| `PlayConfig::encodeCwvf::rateBits` (local) | no | low | role-descriptive | keep current (high) | — |
| `PlayConfig::encodeCwvf::suppressVal` (local) | no | low | role-descriptive | keep current (high) | — |
| `PlayConfig::encodeCwvf::fourChannelVal` (local) | no | low | role-descriptive | keep current (high) | — |
| `PlayConfig::encodeCwvf::defaultRateBit` (local) | no | low | role-descriptive | keep current (high) | — |
| `PlayConfig::encodeCwvf::dummyBit` (local) | no | low | role-descriptive | keep current (high) | — |
| `PlayConfig::encodeCwvf::word` (local) | no | medium | conventional accumulator | keep current (high) | — |
| `PlayConfig::operator!=::other` (param) | no | high | conventional comparator name | keep current (high) | — |
| `PlayConfig::fromJson::jv` (param) | no | medium | conventional JSON value name | keep current (high) | — |
| `PlayConfig::fromJson::pc` (local) | no | medium | local PlayConfig accumulator | keep current (high) | — |
| `PlayConfig::fromJson::obj` (local) | no | high | name describes object view | keep current (high) | — |

## 3. Detailed findings

### PlayConfig::channelMask  [no / high / not-misnomer]

Evidence:
- `play_config.cpp:105`  `{"channelMask",  static_cast<uint64_t>(channelMask.value)},`
- `play_config.cpp:132-133`  `pc.channelMask  = detail::AddressImpl<uint32_t>{ ... obj.at("channelMask").to_number<uint64_t>())};`
- `strings _seqc_compiler.so | grep '^channelMask$'` → `channelMask`
- `play_config.cpp:36` (encodeCwvf): `uint32_t channels = dummyFlag ? 1u : channelMask.value;`
- `asm_commands.cpp:1004`  `config.channelMask = channelMask;` (where local `channelMask` is computed from waveform's `signal.channels_`).

Interpretation:
- The exact string `"channelMask"` is preserved in the original
  binary's `.rodata` and is the JSON serializer key. The field
  receives a per-channel bitmask computed from the waveform.

Judgement:
- The name matches the binary-faithful JSON key and the field's role.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/play_config.hpp:30
- used: src/play_config.cpp:36,77,105,132; src/asm_commands.cpp:989,1004; src/prefetch.cpp:131 (read), src/prefetch_helpers.cpp:194

### PlayConfig::is4Channel  [no / high / not-misnomer]

Evidence:
- `play_config.cpp:108`  `{"is4Channel",   is4Channel},`
- `play_config.cpp:147`  `pc.is4Channel = obj.at("is4Channel").as_bool();`
- `strings _seqc_compiler.so | grep '^is4Channel$'` → `is4Channel`
- `play_config.cpp:46` (encodeCwvf): `uint32_t fourChannelVal = is4Channel ? channelMask.value : 0u;`
- `prefetch_helpers.cpp:216`  `if (entry.config.is4Channel) return true;` inside `Prefetch::getUsedFourChannelMode`.
- `asm_commands.cpp:981`  `config.is4Channel = isHoldMode;` — producer-side parameter `isHoldMode` is the suspicious side.

Interpretation:
- The string `"is4Channel"` is in the binary's `.rodata` as the
  serializer key. Consumer code (`getUsedFourChannelMode`,
  comparisons in `prefetch_emit.cpp`) treats this field as the
  four-channel-mode boolean. Encoding logic uses it to gate the
  `fourChannel` slot of the play word. The producer-side mismatch
  (`config.is4Channel = isHoldMode`) is in `genPlayConfig` parameter
  naming, not in the field.

Judgement:
- The field name matches the verified binary JSON key and the
  consumer-side semantics.

Proposals:
- keep current  (high)

Cross-reference:
- Producer-side parameter `AsmCommands::genPlayConfig::isHoldMode`
  in batch 49 (asm_commands_impl) is the suspect side.

Locations consulted:
- declared: include/zhinst/play_config.hpp:33
- used: src/play_config.cpp:46,83,108,147; src/asm_commands.cpp:981; src/prefetch.cpp:131,395,427,450; src/prefetch_emit.cpp:334,380,424,459,480,534,603; src/prefetch_helpers.cpp:213,216

### PlayConfig::markerBits  [no / high / not-misnomer]

Evidence:
- `play_config.cpp:109`  `{"markerBits",   static_cast<uint64_t>(markerBits.value)},`
- `strings _seqc_compiler.so | grep '^markerBits$'` → `markerBits`
- `asm_commands.cpp:1010-1040` packs marker pairs into `markerBits`
  and assigns `config.markerBits = adjusted;`.
- `play_config.cpp:63`  `word |= (markerBits.value & 0xFu) << markerBitsShift;` packs into bits [27:24].

Interpretation:
- The string `"markerBits"` is in `.rodata`. The field stores
  packed marker-channel bits and is encoded into the play word's
  marker slot.

Judgement:
- Name matches binary-faithful JSON key and field semantics.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/play_config.hpp:35
- used: src/play_config.cpp:63,85,109,136; src/asm_commands.cpp:990,1010-1040

### PlayConfig::precompFlags  [no / high / not-misnomer]

Evidence:
- `play_config.cpp:111`  `{"precompFlags", static_cast<uint64_t>(precompFlags.value)},`
- `strings _seqc_compiler.so | grep '^precompFlags$'` → `precompFlags`
- `play_config.cpp:65`  `word |= (precompFlags.value & 0x3u) << precompFlagShift;`
- `asm_commands.cpp:983`  `config.precompFlags = 0;` (literal zero — no other producer).

Interpretation:
- The string `"precompFlags"` is in `.rodata`. Field is packed
  into the precomp slot of the play word [31:30] and currently
  always written as 0 by `genPlayConfig`.

Judgement:
- Name matches binary-faithful JSON key.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/play_config.hpp:37
- used: src/play_config.cpp:65,89,111,140; src/asm_commands.cpp:983

### PlayConfig::rate  [no / medium / not-misnomer]

Evidence:
- `play_config.cpp:106`  `{"rate", rate},`
- `play_config.cpp:39`  `int effectiveRate = (rate >= 0) ? rate : defaultRate;`
- `play_config.cpp:76,79`  comparator: `if (rate != other.rate) return true;`
- `play_config.cpp:94`  `if (rate > 0 && hold != other.hold)` — rate
  controls whether `hold` is part of identity.
- `asm_commands.cpp:979`  `config.rate = holdCount;` — producer-side
  parameter is named `holdCount`, but field consumer treats it as a
  rate (negative = "use default").

Interpretation:
- JSON key `"rate"` is shared between serializer and the field.
  All field-level uses treat it as a clamped/negative-as-sentinel
  rate value. Producer parameter `holdCount` in `genPlayConfig` is
  the suspicious side, not the field.

Judgement:
- Field name matches its observed semantics and the JSON key.

Proposals:
- keep current  (high)

Cross-reference:
- Producer-side `AsmCommands::genPlayConfig::holdCount` in
  batch 49 is the suspect side.

Locations consulted:
- declared: include/zhinst/play_config.hpp:31
- used: src/play_config.cpp:31,39,79,94,106,144; src/asm_commands.cpp:979

### PlayConfig::now  [unsure / low / cross-batch-arbitration]

Evidence:
- `play_config.cpp:112`  `{"now", now},`  (JSON serializer key)
- `play_config.cpp:73-74` comment: `// `now` is NOT compared (deliberately excluded — transient flag)`
- `play_config.cpp:76-96`: `operator!=` deliberately omits `now`.
- `asm_commands.cpp:984`  `config.now = isFourChannelBool;` (producer assigns from a parameter named `isFourChannelBool`).
- `prefetch_splitplay.cpp:39-40` header comment: `+0x64  config.now (bool) — confirmed (was "indexed")`
- `prefetch_splitplay.cpp:308`  `bool indexed = raw->config.now;`
- `prefetch_splitplay.cpp:401`  `bool indexed2 = raw->config.now;`
- `prefetch_placesingle.cpp:554`  `bool is4Ch = npW->config.now;`
- `prefetch_placesingle.cpp:588,635,644,652,664,715` — six more sites all of the form `bool is4Ch = ...->config.now;`
- `strings _seqc_compiler.so | grep -E '^now$'` → no match (string
  too short/common to be conclusive either way).

Interpretation:
- Every read site outside `play_config.cpp` binds the field to a
  local named `is4Ch` or `indexed` and uses it as the
  four-channel/indexed-mode flag handed to downstream `wvf*` /
  `insertPlay` calls. The producer in `genPlayConfig` writes it
  from a parameter currently named `isFourChannelBool`. The field
  is excluded from equality comparison, marked "transient", which
  fits a "do this play immediately" interpretation but does not
  contradict a four-channel interpretation either. The JSON
  serializer emits `"now"`, but unlike `channelMask` /
  `is4Channel` / `markerBits` / `precompFlags`, the literal string
  `"now"` is not directly verifiable in `.rodata` (too common).

Judgement:
- Name disagrees with every observed consumer; cannot decide here
  whether the field or the producer parameter is the misnomer
  without arbitration against batch 49.

Proposals:
- keep current  (low) — the JSON key is `"now"` and the
  serializer is presumed faithful; consumers may simply be
  misnamed.
- is4Channel2  / indexed  / playNow  (low) — only viable if
  synthesis decides the JSON key itself is a placeholder.

Cross-reference:
- Counterpart `AsmCommands::genPlayConfig::isFourChannelBool` in
  batch 49 (asm_commands_impl) — that name and `config.now` are
  he-said/she-said.

Locations consulted:
- declared: include/zhinst/play_config.hpp:38
- used: src/play_config.cpp:73,112,127; src/asm_commands.cpp:984; src/prefetch_splitplay.cpp:39-40,308,401; src/prefetch_placesingle.cpp:554,588,635,644,652,664,715

### PlayConfig::encodeCwvf::dummyFlag  [yes / low / —]

Evidence:
- `play_config.cpp:33`  `bool dummyFlag = hold || dummy;`
- `play_config.cpp:36`  `uint32_t channels = dummyFlag ? 1u : channelMask.value;`
- `play_config.cpp:53`  `uint32_t dummyBit = dummyFlag ? 1u : 0u;`
- Surrounding doc comment, `play_config.cpp:18-30`: `"hold and dummy share the same encoding — both force channels=1 and set the dummy bit."` and (in body) line 32: `// Determine if "dummy mode" is active (hold OR dummy)`.

Interpretation:
- The variable's true content is "either `hold` or `dummy` is
  true". A reader who sees only the name `dummyFlag` could
  reasonably conclude it tracks just the `dummy` field. The author
  added a clarifying comment, indicating the name alone does not
  carry the meaning.

Judgement:
- Mildly misleading because the value also reflects `hold`, not
  only `dummy`.

Proposals:
- dummyMode      (medium)
- holdOrDummy    (medium)
- keep current   (low)

Locations consulted:
- declared/used: src/play_config.cpp:33,36,53

## 4. Symbols inspected and judged routinely fine

- `PlayConfig::suppress` — JSON key `"suppress"` matches; encode
  path treats it as a 14-bit suppress bitmask and substitutes
  `holdSuppressExceptSigouts` when `hold`; uses match name.
- `PlayConfig::trigger` — JSON key `"trigger"`; packed into the
  trigger slot [29:28]; producer assigns from same-named parameter
  in `genPlayConfig`; matches.
- `PlayConfig::hold` — JSON key `"hold"`; encode-path uses match
  name; comparator gates `hold` equality on `rate > 0`, but that
  is a semantic quirk of the comparator, not of the field name.
- `PlayConfig::dummy` — JSON key `"dummy"`; encode path uses it as
  one half of the "force channels=1" condition; matches.
- `PlayConfig::encodeCwvf::defaultRate` (instance + static
  overload) — matches the role (fallback rate when `rate` is
  negative); doc comment at `:38-40` confirms.
- `PlayConfig::encodeCwvf(static)::config` — trivial forwarder
  parameter; the name is conventional.
- `PlayConfig::encodeCwvf::channels`, `effectiveRate`, `rateBits`,
  `suppressVal`, `fourChannelVal`, `defaultRateBit`, `dummyBit`,
  `word` — each is a one-shot derivation step whose role is
  obvious from the immediate surrounding code; `word` is the
  conventional packed-result accumulator.
- `PlayConfig::operator!=::other` — conventional comparator
  parameter name.
- `PlayConfig::fromJson::jv` — conventional `boost::json::value`
  parameter name; matches usage `.as_object()`.
- `PlayConfig::fromJson::pc` — local under-construction
  PlayConfig accumulator.
- `PlayConfig::fromJson::obj` — bound to `jv.as_object()`; reads
  `obj.at(...)` repeatedly; the name describes what it is.
- All static `*Shift` / `*Mask` constants and
  `holdSuppressExceptSigouts` — out of scope (excluded under §3:
  present in `nm` symbol table as exported BSS members), and
  inspection of one cross-file consumer
  (`prefetch_emit.cpp:149-150`) shows the names match the bit
  layout they describe.

## 5. Coverage

Fully covered:
- All 10 data members of `PlayConfig`.
- All parameters of all five method definitions in
  `play_config.cpp` (instance `encodeCwvf`, static-overload
  `encodeCwvf`, `operator!=`, `toJson`, `fromJson`).
- All locals in those method bodies.
- Cross-file usage of every flagged member, sampled at the
  locations listed in each "Locations consulted" footer.

Not covered (out of scope per §3):
- Type name `PlayConfig` — present in `nm` symbol table.
- Method names `encodeCwvf`, `operator!=`, `toJson`, `fromJson`
  — present in `nm` symbol table.
- Static data members `channelsShift`, `rateShift`,
  `suppressShift`, `fourChannelShift`, `defaultRateShift`,
  `dummyShift`, `markerBitsShift`, `triggerShift`,
  `precompFlagShift`, `channelsMask`, `rateMask`, `suppressMask`,
  `fourChannelMask`, `defaultRateMask`, `dummyMask`,
  `markerBitsMask`, `triggerMask`, `precompFlagMask`,
  `holdSuppressExceptSigouts` — all present in `nm` symbol table
  as exported BSS members.

Deferred (cross-batch):
- Producer-side parameters of `AsmCommands::genPlayConfig` in
  `asm_commands.cpp:960-964` (`isHold`, `fourChannel`,
  `isFourChannelBool`, `isBool`, `holdCount`, `isHoldMode`) —
  flagged in this batch only via cross-references; the actual
  judgement on those parameter names belongs to batch 49
  (asm_commands_impl).
