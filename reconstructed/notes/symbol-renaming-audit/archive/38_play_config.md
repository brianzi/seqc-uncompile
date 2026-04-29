# Batch 38 — play_config

Files in scope:
- `reconstructed/include/zhinst/play_config.hpp`
- `reconstructed/src/play_config.cpp`

## Preamble — symbols inspected

Data members of `struct PlayConfig`:
- `channelMask`            (AddressImpl<uint32_t>)
- `rate`                   (int32_t)
- `suppress`               (AddressImpl<uint32_t>)
- `is4Channel`             (bool)
- `markerBits`             (AddressImpl<uint32_t>)
- `trigger`                (AddressImpl<uint32_t>)
- `precompFlags`           (AddressImpl<uint32_t>)
- `now`                    (bool)
- `hold`                   (bool)
- `dummy`                  (bool)

Static constants (shifts/masks): `channelsShift`, `rateShift`,
`suppressShift`, `fourChannelShift`, `defaultRateShift`, `dummyShift`,
`markerBitsShift`, `triggerShift`, `precompFlagShift`, plus the
matching `*Mask` constants and `holdSuppressExceptSigouts`. Names
mirror the bit-layout doc-comment and the static-const dispatch
referenced from many call sites (`prefetch_emit.cpp:149-150`,
`prefetch.cpp` etc.) — none flagged.

Parameters/locals of method bodies in `play_config.cpp`:
- `PlayConfig::encodeCwvf(int defaultRate)` — parameter `defaultRate`
- locals in `encodeCwvf`: `dummyFlag`, `channels`, `effectiveRate`,
  `rateBits`, `suppressVal`, `fourChannelVal`, `defaultRateBit`,
  `dummyBit`, `word`
- `PlayConfig::encodeCwvf(const PlayConfig& config, int defaultRate)`
  static overload — parameters `config`, `defaultRate`
- `PlayConfig::operator!=(const PlayConfig& other)` — parameter `other`
- `PlayConfig::toJson()` — no parameters/locals
- `PlayConfig::fromJson(const boost::json::value& jv)` — parameter
  `jv`, local `pc`, local `obj`

Note: each PlayConfig data-member name is mirrored 1:1 by a JSON key
in `toJson`/`fromJson` (`"channelMask"`, `"rate"`, `"suppress"`,
`"is4Channel"`, `"markerBits"`, `"trigger"`, `"precompFlags"`,
`"now"`, `"hold"`, `"dummy"`). The JSON-keyed mapping is strong
evidence that these names are faithful reconstructions taken from
the original binary's serializer, so the bar for flagging them is
high. Suspect blocks below note this where relevant.

## Suspect blocks

```
- symbol:        PlayConfig::now
- declared at:   include/zhinst/play_config.hpp:38
- defined/used:  src/play_config.cpp:103-115 (toJson key "now"),
                 127 (fromJson), 91-95 (operator!= — note: now is
                 deliberately NOT compared);
                 src/asm_commands.cpp:984
                   (config.now = isFourChannelBool param);
                 src/prefetch_splitplay.cpp:39-40 (header comment
                   "config.now (bool) — confirmed (was 'indexed')"),
                   308 ("bool indexed = raw->config.now"),
                   401 (same);
                 src/prefetch_placesingle.cpp:473, 507, 554, 563,
                   571, 583, 634 — all of the form
                   "bool is4Ch = ...->config.now"
- observations:  Every consumer reads `config.now` and binds it to a
                 local named `is4Ch` or `indexed` and then passes it
                 as the "four-channel" / indexed-mode boolean to
                 wvfImpl/wvfRegImpl/insertPlay. The value never feeds
                 the encodeCwvf packing (encodeCwvf does not read
                 `now`), and operator!= deliberately excludes it from
                 the equality test (see comment at play_config.cpp:73
                 "now is NOT compared (deliberately excluded —
                 transient flag)"). The producer-side write in
                 genPlayConfig (asm_commands.cpp:984) sources it from
                 a parameter currently named `isFourChannelBool`,
                 reinforcing that the value carried is a four-channel/
                 indexed flag, not a "play immediately" flag.
                 Counter-evidence: the JSON serializer emits the key
                 as "now" (toJson at play_config.cpp:112; fromJson at
                 :127), and the audit's exclusion rule treats
                 binary-derived names as faithful. The
                 prefetch_splitplay.cpp:39 comment "(was 'indexed')"
                 indicates the field was already renamed once toward
                 the JSON key.
- judgement:     The name conflicts with every observed consumer use,
                 but is supported by the JSON serializer key, which
                 is presumed to come from the original binary. Worth
                 surfacing for synthesis to decide whether the JSON
                 key itself is the canonical name (in which case the
                 field stays `now` and the consumer-side locals are
                 the things to rename) or whether the JSON key is
                 also a placeholder.
- confidence:    low
- proposals:     (none — defer; if not faithful, candidates supported
                 by usage would be `is4Channel`/`indexed`/`fourCh`,
                 but those collide with the existing `is4Channel`
                 field which means a different thing — see below.)
- status:        verify-not-original
```

```
- symbol:        PlayConfig::is4Channel
- declared at:   include/zhinst/play_config.hpp:33
- defined/used:  src/play_config.cpp:108 (toJson "is4Channel"), 147
                 (fromJson), 83 (operator!=);
                 src/asm_commands.cpp:981
                   (config.is4Channel = isHoldMode param);
                 src/prefetch_helpers.cpp:205-213
                   (Prefetch::getUsedFourChannelMode iterates entries
                   and returns true if any entry has is4Channel set,
                   reading PlayConfig+0x0C);
                 src/prefetch.cpp:131, 395, 427, 450, 459, 480, 534,
                   etc. — equality-compared between current and
                   saved CWVF state, treated as a regular config bit.
- observations:  The consumer-side semantics line up with the name:
                 `getUsedFourChannelMode` literally answers "is any
                 entry in 4-channel mode" by reading this field, and
                 the field is packed into the `fourChannel` slot of
                 the play word only when `is4Channel` is true
                 (encodeCwvf at play_config.cpp:46). The JSON key
                 also matches.
                 The only mismatch is the producer side: in
                 `AsmCommands::genPlayConfig`
                 (asm_commands.cpp:981) the field is assigned from
                 a parameter currently named `isHoldMode`. That is
                 a parameter-name problem in `genPlayConfig`, not a
                 problem with the field name. (Out of scope for this
                 batch — `genPlayConfig` lives in `asm_commands.*`.)
- judgement:     Field name fits the dominant usage and the JSON
                 serializer key. Not flagging.
- confidence:    n/a (no flag)
```

```
- symbol:        local `dummyFlag` in PlayConfig::encodeCwvf
- declared at:   src/play_config.cpp:33
- defined/used:  src/play_config.cpp:33, 36, 53
- observations:  Computed as `hold || dummy` and used to (a) force
                 `channels` to 1 (line 36) and (b) drive the dummy
                 bit (line 53). The header comment block at
                 play_config.cpp:18-30 calls this combined value
                 "dummy mode" and notes "hold and dummy share the
                 same encoding — both force channels=1 and set the
                 dummy bit. The suppress substitution only happens
                 for hold." So the local genuinely represents the
                 *combined* hold-or-dummy condition, not either flag
                 individually.
- judgement:     `dummyFlag` is mildly misleading because half of
                 its truthiness comes from `hold`, not `dummy`; a
                 reader could think it tracks just the `dummy` field.
                 The author already wrote "dummy mode is active
                 (hold OR dummy)" in the surrounding comment to
                 disambiguate, which is a soft signal that the name
                 alone doesn't carry the meaning. Local-only, low
                 risk.
- confidence:    low
- proposals:     dummyMode  (alts: holdOrDummy, channelsForcedToOne)
```

No flags raised against the remaining inspected symbols
(`channelMask`, `rate`, `suppress`, `markerBits`, `trigger`,
`precompFlags`, `hold`, `dummy`, the static shift/mask constants,
`holdSuppressExceptSigouts`, `encodeCwvf` parameter `defaultRate`,
the static overload parameters, `operator!=` parameter `other`,
`fromJson` parameter `jv` and locals `pc`/`obj`, and the remaining
encodeCwvf locals). They each match a JSON serializer key (for the
data members) or a self-explanatory role at every observed call
site.

## Coverage

Fully covered:
- All 10 data members of `PlayConfig`.
- All static `*Shift` / `*Mask` constants and
  `holdSuppressExceptSigouts`.
- All parameters of all methods declared in `play_config.hpp` and
  defined in `play_config.cpp` (`encodeCwvf`, static overload,
  `operator!=`, `toJson`, `fromJson`).
- All locals in `play_config.cpp` method bodies.
- Cross-file usage of every flagged member (sampled across
  `prefetch.cpp`, `prefetch_emit.cpp`, `prefetch_placesingle.cpp`,
  `prefetch_splitplay.cpp`, `prefetch_helpers.cpp`,
  `asm_commands.cpp`, `custom_functions_io.cpp`, `node.*`).

Deferred / out of scope for this batch:
- Parameter names of `AsmCommands::genPlayConfig` (declared in
  `include/zhinst/asm_commands.hpp:201-204`, defined in
  `src/asm_commands.cpp:960-1044`). Multiple parameters there
  (`isHold`, `fourChannel`, `isFourChannelBool`, `isBool`,
  `holdCount`, `isHoldMode`) appear scrambled relative to the fields
  they assign — e.g. `config.is4Channel = isHoldMode` and
  `config.now = isFourChannelBool`. This is a producer-side naming
  problem that should be picked up in the `asm_commands` audit
  batch, not here.
