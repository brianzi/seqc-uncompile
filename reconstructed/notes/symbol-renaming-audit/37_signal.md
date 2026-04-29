# Batch 37 — signal

## 1. Files considered

- `reconstructed/include/zhinst/signal.hpp`
- `reconstructed/src/signal.cpp`

Cross-reference reads (read-only):
- `reconstructed/include/zhinst/rawwave.hpp` (counterpart parameters
  `samples` / `markers` / `markerBits` of `RawWaveHirzel16` ctor and
  `byteSize_ = channels*length*2` byte-unit cross-check).
- `reconstructed/notes/symbol-renaming-audit/35_rawwave.md`
  (already-judged counterparts).
- `reconstructed/notes/symbol-renaming-audit/14_waveform.md`,
  `38_play_config.md` (sample/byte/channel concepts).
- `reconstructed/src/waveform_generator.cpp` (heaviest call site for
  every Signal ctor — see grep for `Signal(` invocations at lines
  228, 242, 268, 602, 724, 774, 819, 856, 1923, 1986, 2216, 2314,
  2336, 2437, 2499, 2623, 2706, etc.).
- `reconstructed/src/waveform_ir.cpp:169,207`,
  `reconstructed/src/wavetable_ir.cpp:539,800,837`,
  `reconstructed/src/prefetch.cpp:1653,1662`,
  `reconstructed/src/custom_functions.cpp:904,1005,1114`,
  `reconstructed/src/custom_functions_play.cpp:432,1021` —
  uses of `signal.channels()`, `signal.length()`, `signal.markerBits_`.

### Symbol-table verification (RULES §3, `nm --demangle _seqc_compiler.so`)

Excluded from rename (encoded as type/method/free-function names):

- **Type `zhinst::Signal`** (appears in every Signal ctor mangled symbol).
- **Type `zhinst::Signal::ReserveOnly`** — encoded **nested** in Signal:
  `zhinst::Signal::Signal(zhinst::Signal::ReserveOnly const&, unsigned long,
   zhinst::MarkerBitsPerChannel const&)` at 0x25ef50. The *type-name*
  `ReserveOnly` is therefore authoritative. (The reconstruction places
  the type at namespace `zhinst::` rather than nested in `Signal`; that
  is a structural defect, **not** a name choice — out of scope here,
  noted in §4.)
- **Type alias `zhinst::MarkerBitsPerChannel`** — encoded in mangled
  parameter type of three Signal ctors (0x25f1a0, 0x25ef50, 0x106340).
- **Enum `zhinst::SampleFormat`** — encoded in mangled name of
  `Signal::getRawData(zhinst::SampleFormat) const` (0x293ec0),
  `WaveformIR::toJsonElement(zhinst::SampleFormat)` (0x2c5440),
  `WavetableIR::getJsonIndex(zhinst::SampleFormat) const` (0x29f480),
  `ElfWriter::addWaveform(..., zhinst::SampleFormat, ...)` (0x2939f0).
- **All 11 method names**: every ctor, `~Signal`, `append(double,uint8_t)`,
  `append(Signal&)`, `resizeSamples`, `checkAllocation`, `toJson`,
  `fromJson`, `getRawData`, `operator==` — each appears in nm.

Not in symbol table (in scope):
- All ctor parameter names, all data-member names, all local names,
  the two enumerators `Cervino` / `Hirzel16`, the empty tag struct's
  presence (no members anyway).

`nm` shows no static data members of Signal.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `Signal::samples_` | no | high | matches Hirzel16 ctor binding | keep current | not-misnomer |
| `Signal::markers_` | no | high | per-sample marker stream | keep current | not-misnomer |
| `Signal::markerBits_` | no | high | JSON key `"markerBits"` in rodata | keep current | not-misnomer |
| `Signal::channels_` | no | high | matches `channels()` accessor + binary | keep current | not-misnomer |
| `Signal::reserveOnly_` | no | high | JSON key `"reserveOnly"` in rodata | keep current | not-misnomer |
| `Signal::length_` | no | medium | sample-frame count, not bytes | keep current | not-misnomer |
| `Signal::Signal(size_t)::length` | no | medium | reserved samples count | keep current | — |
| `Signal::Signal(size_t,double,uint8_t,uint16_t)::numSamples` | unsure | medium | total doubles, not frames | keep current, `numValues` | — |
| `Signal::Signal(size_t,double,uint8_t,uint16_t)::value` | no | medium | fill value | keep current | — |
| `Signal::Signal(size_t,double,uint8_t,uint16_t)::marker` | no | high | per-sample marker byte | keep current | — |
| `Signal::Signal(size_t,double,uint8_t,uint16_t)::channels` | no | high | channel count | keep current | — |
| `Signal::Signal(size_t,double,uint8_t,uint16_t)::numEntries` (local) | no | low | reconstruction-invented | keep current, `mbIters` | — |
| `Signal::Signal(size_t,double,uint8_t,uint16_t)::mbSize` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::Signal(size_t,MarkerBitsPerChannel)::length` | no | medium | sample-frame count | keep current | — |
| `Signal::Signal(size_t,MarkerBitsPerChannel)::markerBitsPerChannel` | no | medium | matches typedef name | keep current | — |
| `Signal::Signal(size_t,MarkerBitsPerChannel)::totalSamples` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::Signal(vec,vec,vec,uint16_t,bool,size_t)::samples` | no | high | bound to `samples_` | keep current | — |
| `Signal::Signal(vec,vec,vec,uint16_t,bool,size_t)::markers` | no | high | bound to `markers_` | keep current | — |
| `Signal::Signal(vec,vec,vec,uint16_t,bool,size_t)::markerBits` | no | high | bound to `markerBits_` | keep current | — |
| `Signal::Signal(vec,vec,vec,uint16_t,bool,size_t)::channels` | no | high | bound to `channels_` | keep current | — |
| `Signal::Signal(vec,vec,vec,uint16_t,bool,size_t)::reserveOnly` | no | high | bound to `reserveOnly_` | keep current | — |
| `Signal::Signal(vec,vec,vec,uint16_t,bool,size_t)::length` | no | medium | bound to `length_` | keep current | — |
| `Signal::Signal(ReserveOnly,size_t,MarkerBitsPerChannel)::tag` | no | medium | unused dispatch tag | keep current | — |
| `Signal::Signal(ReserveOnly,size_t,MarkerBitsPerChannel)::length` | no | medium | sample-frame count | keep current | — |
| `Signal::Signal(ReserveOnly,size_t,MarkerBitsPerChannel)::markerBitsPerChannel` | no | medium | matches typedef name | keep current | — |
| `Signal::Signal(vec const&,vec const&,MarkerBits const&)::samples` | no | high | bound to `samples_` | keep current | — |
| `Signal::Signal(vec const&,vec const&,MarkerBits const&)::markers` | no | high | bound to `markers_` | keep current | — |
| `Signal::Signal(vec const&,vec const&,MarkerBits const&)::markerBitsPerChannel` | no | medium | matches typedef name | keep current | — |
| `Signal::Signal(vec const&,uint16_t)::samples` | no | high | bound to `samples_` | keep current | — |
| `Signal::Signal(vec const&,uint16_t)::channels` | no | high | bound to `channels_` | keep current | — |
| `Signal::Signal(Signal const&)::other` | no | high | conventional | keep current | — |
| `Signal::append(double,uint8_t)::sample` | no | high | one waveform sample | keep current | — |
| `Signal::append(double,uint8_t)::marker` | no | high | one marker byte | keep current | — |
| `Signal::append(double,uint8_t)::sampleIndex` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::append(double,uint8_t)::mbSize` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::append(Signal&)::other` | no | high | conventional | keep current | — |
| `Signal::append(Signal&)::insertPos` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::append(Signal&)::otherSamplesBegin` (local) | unsure | low | dead local? | keep current, remove | — |
| `Signal::append(Signal&)::markerInsertPos` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::append(Signal&)::mbSize` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::resizeSamples::newLength` | unsure | medium | parameter is frame-count | keep current, `newFrameCount` | — |
| `Signal::resizeSamples::totalSamples` (local) | no | medium | reconstruction-invented | keep current | — |
| `Signal::resizeSamples::currentSize` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::resizeSamples::currentMarkerSize` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::checkAllocation::totalSamples` (local) | no | medium | reconstruction-invented | keep current | — |
| `Signal::checkAllocation::zero` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::toJson::dataArray` (local) | no | medium | JSON key is `"data"` | keep current | — |
| `Signal::toJson::markerArray` (local) | no | medium | JSON key is `"marker"` | keep current | — |
| `Signal::toJson::markerBitsArray` (local) | no | medium | JSON key is `"markerBits"` | keep current | — |
| `Signal::fromJson::json` | no | high | conventional | keep current | — |
| `Signal::fromJson::obj` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::fromJson::dataArr`/`markerArr`/`mbArr` (locals) | no | low | reconstruction-invented | keep current | — |
| `Signal::fromJson::samples`/`markers`/`markerBits`/`channels`/`reserveOnly`/`length` (locals) | no | medium | match field names | keep current | — |
| `Signal::getRawData::format` | no | high | matches enum role | keep current | — |
| `Signal::getRawData::placeholder`/`raw`/`numSamples` (locals) | no | low | reconstruction-invented | keep current | — |
| `Signal::operator==::other` | no | high | conventional | keep current | — |
| `Signal::operator==::epsilon` (local constexpr) | no | medium | matches usage | keep current | — |
| `Signal::operator==::numSamples` (local) | no | low | reconstruction-invented | keep current | — |
| `Signal::operator==::diff`/`absDiff`/`absBi`/`tolerance` (locals) | no | low | math conventional | keep current | — |
| `SampleFormat::Cervino` | no | high | rodata string `cervino` | keep current | not-misnomer |
| `SampleFormat::Hirzel16` | no | medium | rodata string `hirzel`; "16" is sample width | keep current | — |
| `MarkerBitsPerChannel` (alias) | n/a | high | encoded in symbol table | keep current | not-misnomer |
| `ReserveOnly` (tag struct) | no | high | encoded in symbol table | keep current | not-misnomer |

## 3. Detailed findings

### `Signal::samples_`  [no / high / not-misnomer]

Evidence:
- `signal.hpp:97` `std::vector<double> samples_;`
- Bound at `signal.cpp:419` to `RawWaveHirzel16` ctor's `samples`
  parameter (already judged `not-misnomer` in batch 35,
  `35_rawwave.md` §3 "RawWaveHirzel16::RawWaveHirzel16::samples").
- All ctors that take a `vector<double>` parameter spell it
  `samples` (signal.cpp:110, 152, 168) and assign it directly into
  `samples_` — no rename divergence at any call site.
- `signal.cpp:432` `raw->data_[i] = util::wave::double2awg(samples_[i],
  markers_[i])` confirms each element is a single waveform sample
  scaled to ±1.0 then packed.

Interpretation:
- Field consistently holds a flat vector of waveform-sample doubles;
  parameter, member, and downstream consumer all use the same name.

Judgement:
- Name is authoritative-by-consistency; not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/signal.hpp:97
- read:     src/signal.cpp:336, 419, 425, 432, 454-458 (operator==)
- written:  src/signal.cpp:33-39, 53, 99, 113, ...

### `Signal::markerBits_`  [no / high / not-misnomer]

Evidence:
- `signal.cpp:357` emits JSON key `"markerBits"`.
- `strings _seqc_compiler.so | grep markerBits` returns the bare
  string `markerBits` — matches the binary's own JSON key (RULES §4d
  tier 2).
- `signal.cpp:387` reads JSON key `"markerBits"` back into the field.
- One byte per channel, OR-reduced from per-sample marker bytes
  (signal.cpp:222, 257-258); element count == `channels_` for almost
  every ctor (e.g. signal.cpp:63, 95, 156, 176).

Interpretation:
- Field is per-channel marker-bit summary; both internal usage and
  external JSON serialization agree on the name.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/signal.hpp:99
- json key: src/signal.cpp:357, 387
- usage:    src/signal.cpp:33, 63, 76-77, 92, 122, 136, 156, 176, 222,
            257-258, 346, 388, 472-475

### `Signal::reserveOnly_`  [no / high / not-misnomer]

Evidence:
- `signal.cpp:354` JSON serializer key `"reserveOnly"`.
- `strings _seqc_compiler.so | grep reserveOnly` returns the bare
  string `reserveOnly` — matches (RULES §4d tier 2).
- Branching field: `signal.cpp:272, 306, 409` switch behavior on this
  flag — "if true, allocations are deferred". Constructor #5
  (signal.cpp:131) is the dedicated reserve-only ctor and sets it.

Interpretation:
- Field gates lazy materialization of `samples_`/`markers_`. JSON key
  is identical, confirming the binary uses this exact spelling.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/signal.hpp:101
- json key: src/signal.cpp:354, 395
- usage:    src/signal.cpp:131, 272, 306, 409

### `Signal::channels_`  [no / high / not-misnomer]

Evidence:
- Accessor `channels()` exposes it (signal.hpp:83). Accessor name itself
  is excluded by the symbol-table rule (free of dependence on the field
  name), but the consistent binding at every call site
  (waveform_generator.cpp:231, custom_functions.cpp:904,1005,
  prefetch.cpp:1653,1662, custom_functions_play.cpp:432,
  wavetable_ir.cpp:837, prefetch_prepare.cpp:717, waveform_ir.cpp:169)
  uses `channels` as the receiver-local name.
- Used as multiplier in `samples_.size() = channels_ * length_`
  expressions (signal.cpp:97, 277, 309, 413).
- Type `uint16_t` matches the `channels` ctor parameter type that is
  recorded in the binary's mangled ctor signature
  (`Signal::Signal(unsigned long, double, unsigned char, unsigned short)`).

Interpretation:
- Field holds the per-Signal channel count.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/signal.hpp:100
- usage:    src/signal.cpp:67, 97, 175, 225, 262, 277, 309, 413
- callers:  src/waveform_generator.cpp:231, src/prefetch.cpp:1653

### `Signal::length_`  [no / medium / not-misnomer]

Evidence:
- `signal.cpp:67` `length_ = numSamples / static_cast<size_t>(channels)`
  — explicitly the *frame* count (i.e. the I/Q pair count for
  `channels_==2`).
- `signal.cpp:225` `length_ = samples_.size() / channels_` repeats this.
- `signal.cpp:158` `length_(samples.size() / markerBitsPerChannel.size())`.
- JSON serializer at `signal.cpp:352` writes the field under key
  `"length"`.
- Read sites use it as a sample-frame count: `wavetable_ir.cpp:539`
  `Signal fillerSignal(ReserveOnly{}, minSamples, …)` — note the
  caller's local is `minSamples` (frames), passed as the constructor's
  `length` parameter.
- `signal.cpp:413` `byteSize_ = channels_ * length_ * 2` — bytes derive
  from frames × channels × 2 bytes-per-sample (cross-checked vs
  rawwave batch 35 §3 `byteSize_`).
- Header comment at `signal.hpp:44` correctly labels offset +0x50 as
  `length_`.

Interpretation:
- Unit is sample-frames (one tick per channel-multiplexed slot), not
  doubles and not bytes. The name is generic but no use-site treats it
  as anything else; the documented JSON key is the same noun.

Judgement:
- Not a misnomer. Confidence is medium because "length" by itself
  doesn't disambiguate frames vs samples vs bytes — but no caller is
  confused, and the binary's own JSON key is verbatim `"length"`.

Proposals:
- keep current  (high — JSON-key match)
- `lengthFrames` (low — would clarify unit but diverges from JSON key
  and from binary header offset documentation)

Locations consulted:
- declared: include/zhinst/signal.hpp:102
- json key: src/signal.cpp:352, 396
- usage:    src/signal.cpp:67, 225, 262, 277, 295, 309, 413
- callers:  src/wavetable_ir.cpp:539, 800; src/custom_functions.cpp:1114

### `Signal::Signal(size_t,double,uint8_t,uint16_t)::numSamples`  [unsure / medium / —]

Evidence:
- `signal.cpp:53` `samples_.resize(numSamples, value);` — fills exactly
  `numSamples` doubles.
- `signal.cpp:67` `length_ = numSamples / channels;` — interpreted as
  the *total* double count, divided by channels to yield frames.
- The sole external caller is `waveform_generator.cpp:602`:
  `return Signal(static_cast<size_t>(length), 0.0, marker,
   /*channels=*/1);` — caller's local is `length` (frames), but with
  `channels=1` "frames" and "doubles" are equal, so the name mismatch
  doesn't manifest at this single site.
- Counterpart parameter name in ctor #1 is `length` (frames-only;
  `channels==1` implicit), and in ctor #5/#6 is `length` (frames).

Interpretation:
- `numSamples` is, semantically, the total slot count of `samples_` —
  i.e. `channels × frames`. The plural "samples" is consistent with the
  field name `samples_`, but the parameter measures *doubles* not
  *frames*, and other ctors use `length` to mean *frames*. This is a
  subtle ambiguity, not an outright contradiction.

Judgement:
- Unsure. The name is internally defensible (`numSamples` = number of
  values in `samples_`), but it diverges from the meaning of `length`
  in the sibling ctors. With only one caller and `channels=1`, the
  inconsistency has no operational effect today.

Proposals:
- keep current  (medium — defensible w.r.t. `samples_`)
- `numValues` (low — disambiguates from "frames")
- `totalSamples` (low — matches the local `totalSamples` used in
  ctors #3 and in `resizeSamples`/`checkAllocation` for the same
  product)

Locations consulted:
- declared: include/zhinst/signal.hpp:52
- defined:  src/signal.cpp:48-79
- caller:   src/waveform_generator.cpp:602

### `Signal::Signal(size_t,double,uint8_t,uint16_t)::numEntries` (local)  [no / low / —]

Evidence:
- `signal.cpp:73` `size_t numEntries = (channels > 0) ? channels : 1;`
  with comment citing binary at 0x25ec5c using `cmp $1; adc $0` to
  realize `max(1, channels)`.
- Used only as the loop bound (signal.cpp:76) for the
  `markerBits_[i % mbSize] |= marker` distribution.

Interpretation:
- A reconstruction-invented local name for the iteration count
  `max(1, channels)`. No call site to compare against; the role is the
  iteration upper bound for distributing one marker byte across
  channels.

Judgement:
- Not flagged. `numEntries` is generic but the comment immediately
  above clarifies meaning; no concrete confusion.

Proposals:
- keep current  (low)
- `iterations` (low)
- `mbIters` (low)

Locations consulted:
- src/signal.cpp:73-77

### `Signal::Signal(ReserveOnly,size_t,MarkerBitsPerChannel)::tag`  [no / medium / —]

Evidence:
- `signal.cpp:131` `Signal(ReserveOnly const& /*tag*/, size_t length,
   MarkerBitsPerChannel const& markerBitsPerChannel)` — the parameter
  is unused (commented out) and exists purely for overload dispatch.
- The struct `ReserveOnly` is itself a zero-byte tag (signal.hpp:34
  `struct ReserveOnly {};`).

Interpretation:
- Conventional name for a tag-dispatch parameter; not consumed in body.

Judgement:
- Not a misnomer. Standard idiom.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/signal.hpp:57
- defined:  src/signal.cpp:131

### `Signal::resizeSamples::newLength`  [unsure / medium / —]

Evidence:
- `signal.cpp:271` parameter `newLength`.
- `signal.cpp:273` `length_ = newLength;` — assigned directly to
  `length_`, which is *frames* per ctor #2 derivation
  (`length_ = numSamples / channels`).
- `signal.cpp:277` `totalSamples = channels_ * newLength;` — confirms
  `newLength` is in the same unit as `length_` (frames), and
  `totalSamples` is the total double slot count.
- Method name itself (`resizeSamples`) is in the binary symbol table
  and excluded.

Interpretation:
- The parameter is a frame count, but the method name reads "resize
  samples". A reader could plausibly assume the argument is the new
  total sample-double count; in fact it's the new frame count and the
  method internally multiplies by `channels_`. This is a soft
  unit-confusion smell.

Judgement:
- Unsure. The name `newLength` is consistent with `length_` (the
  field), and that's the strongest internal anchor. But the method's
  own name (`resizeSamples`) suggests "samples", which would naturally
  pair with `numSamples` rather than `newLength`. The method name is
  excluded from rename, so the only lever here is the parameter.

Proposals:
- keep current  (medium — matches `length_`)
- `newFrameCount` (low — makes the unit explicit)
- `newLengthFrames` (low — same goal)

Locations consulted:
- declared: include/zhinst/signal.hpp:71
- defined:  src/signal.cpp:271-296

### `Signal::append(Signal&)::otherSamplesBegin` (local)  [unsure / low / —]

Evidence:
- `signal.cpp:243` `auto otherSamplesBegin = other.samples_.data();`
- The variable is never read after assignment in the reconstructed
  source. The line sits between two `other.checkAllocation()` calls
  (signal.cpp:242, 244), suggesting it was intended as a re-fetched
  data pointer after possible reallocation, but the value is dropped.

Interpretation:
- This is a reconstruction artifact: either the binary has a dead
  local that the recon faithfully copied, or the recon dropped the
  use site of the value. The duplicated `checkAllocation()` calls at
  242, 244, 251, 252 (four total) reinforce the suspicion that the
  binary's control flow was mechanically transcribed.

Judgement:
- Unsure as a *naming* issue: the name correctly describes the
  pointer's source. The real defect (if any) is dead code or
  duplicated calls, which is **not** a renaming concern — flagged
  here only because the local exists in scope and is otherwise inert.

Proposals:
- keep current  (low)
- (out of scope: remove dead local; consolidate duplicate
  `checkAllocation` calls — flag for incidental_findings, not for
  this audit)

Locations consulted:
- defined: src/signal.cpp:235-263

### `SampleFormat::Cervino`  [no / high / not-misnomer]

Evidence:
- `signal.hpp:29` `Cervino = 0,`
- Used at `signal.cpp:417` as the default branch (the `if Hirzel16`
  body's `else`).
- `strings _seqc_compiler.so | grep -i cervino` returns the lowercase
  string `cervino` (and the binary contains
  `_GLOBAL__sub_I_AsmCommandsImplCervino.cpp`,
  `RawWaveCervino`, `AsmCommandsImplCervino`, multiple
  `*Cervino()` methods — confirming "Cervino" is the project's own
  product/codename).

Interpretation:
- Codename matches a real internal label used pervasively in the
  binary; rodata holds the lowercase string.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/signal.hpp:29
- used:     src/signal.cpp:417 (default branch)

### `SampleFormat::Hirzel16`  [no / medium / —]

Evidence:
- `signal.hpp:30` `Hirzel16 = 1`.
- Selects the `RawWaveHirzel16` construction path
  (signal.cpp:417-419).
- `strings` shows lowercase `hirzel`. The "16" suffix matches the
  16-bit-per-sample variant `RawWaveHirzel16` (vs hypothetical
  `RawWaveHirzel32` that does not exist in the binary).
- Sibling `Cervino` has no numeric suffix. The asymmetry is real:
  `RawWaveHirzel16` is a distinct type, while `RawWaveCervino` is
  the only Cervino variant.

Interpretation:
- The numeric suffix faithfully echoes the type it constructs.

Judgement:
- Not a misnomer. Confidence medium because rodata only confirms
  `hirzel` (no `hirzel16` string verified), so the "16" suffix is
  inferred from the corresponding type name rather than directly
  from the binary's enum-to-string table.

Proposals:
- keep current  (medium)
- `Hirzel` (low — drop suffix to match the codename string in rodata;
  weakens the link to `RawWaveHirzel16`)

Locations consulted:
- declared: include/zhinst/signal.hpp:30
- used:     src/signal.cpp:417

## 4. Symbols inspected and judged routinely fine

- All ctor parameters spelled `samples`, `markers`, `markerBits` /
  `markerBitsPerChannel`, `channels`, `reserveOnly`, `length` are
  bound either directly to identically-named fields or to the
  `MarkerBitsPerChannel` typedef — no he-said/she-said.
- `Signal::Signal(size_t)::length` — single parameter, used as
  `samples_.reserve(length)` with implicit `channels_=1`, so
  frame-count and double-count coincide.
- `Signal::Signal(Signal const&)::other` — conventional copy-ctor
  parameter.
- `Signal::operator==::other` — conventional comparison parameter.
- `Signal::append(double,uint8_t)::sample` / `marker` — same
  semantics as `RawWaveHirzel16::RawWaveHirzel16` parameters
  already judged in batch 35.
- `Signal::append(Signal&)::other` — conventional.
- `Signal::fromJson::json` — conventional deserializer parameter.
- `Signal::getRawData::format` — selects the encoding; matches enum
  semantics.
- All reconstruction-invented locals named after `samples_`,
  `markers_`, `markerBits_`, `length_`, `channels_` field names
  (`samples`, `markers`, `markerBits`, etc. inside `fromJson`,
  `toJson`, `resizeSamples`, `checkAllocation`, `getRawData`,
  `operator==`) — names mirror the field they read/write.
- `Signal::operator==::epsilon` — labeled `1e-12`, matches the
  numeric tolerance that the comment cites at address 0x956350.
- `MarkerBitsPerChannel` typedef name — encoded in mangled symbols;
  authoritative.
- `ReserveOnly` tag-struct name — encoded as
  `zhinst::Signal::ReserveOnly` in mangled symbol; authoritative.

### Side observations (not naming flags)

- **Structural placement of `ReserveOnly`** (signal.hpp:34): the
  binary's mangled symbol places this type **nested in `Signal`**
  (`zhinst::Signal::ReserveOnly`) — `nm` line:
  `t zhinst::Signal::Signal(zhinst::Signal::ReserveOnly const&,
   unsigned long, zhinst::MarkerBitsPerChannel const&)`. The current
  reconstruction declares it at namespace scope `zhinst::ReserveOnly`.
  This is a *placement* defect, not a *name* defect, so it does not
  belong in the renaming-audit deliverables — record under
  incidental_findings.md if not already there.
- **Potentially dead local** in `Signal::append(Signal&)`:
  `otherSamplesBegin` at signal.cpp:243 is never read; the four
  `other.checkAllocation()` calls (242, 244, 251, 252) look like a
  recon transcription artifact. Out of scope here.
- **Redundant branch** at `signal.cpp:281-285` (resizeSamples):
  `if (totalSamples > currentSize) { samples_.resize(totalSamples); }
   else if (totalSamples < currentSize) { samples_.resize(totalSamples); }`
  collapses to an unconditional `samples_.resize(totalSamples)`.
  Same shape at lines 289-293 for `markers_`. Cosmetic, not a naming
  finding.

## 5. Coverage

**Fully covered:**
- All 11 Signal methods (10 in scope by name + dtor): every parameter
  and every recordable local name was examined against the body and
  at least one external caller.
- All 6 instance fields (`samples_`, `markers_`, `markerBits_`,
  `channels_`, `reserveOnly_`, `length_`): cross-checked against
  - JSON keys in `toJson()` / `fromJson()`,
  - rodata strings via `strings`,
  - downstream consumer parameter bindings (RawWaveHirzel16 ctor,
    waveform_generator, prefetch, wavetable_ir, custom_functions),
  - already-completed batch 35 (rawwave) findings.
- The `SampleFormat` enum and both enumerators.
- The `ReserveOnly` tag struct's name and existence (placement noted
  as out-of-scope side observation).
- The `MarkerBitsPerChannel` alias (authoritative-excluded).
- Sample-units / channel-concept watch-outs from the prompt:
  `length_` confirmed = frames; `channels_` confirmed = channel count
  (matches `byteSize_ = channels*length*2` from batch 35); marker-bit
  semantics consistent with batch 35 RawWaveHirzel16.

**Deferred:** none.

**Not covered (out of scope per RULES §2 / §3):**
- Type `Signal` — appears in every Signal-ctor mangled name.
- All Signal method names (11 total, including the 8 ctors,
  dtor, both `append` overloads, `resizeSamples`, `checkAllocation`,
  `toJson`, `fromJson`, `getRawData`, `operator==`) — all in nm.
- Type alias `MarkerBitsPerChannel` — encoded in nm.
- Enum `SampleFormat` — encoded in nm.
- Type `Signal::ReserveOnly` (name only; placement defect noted in §4).
- `boost::json::value` and `std::vector<…>` — third-party / stdlib.
- The `util::wave::double2awg` external function — covered by
  batch 35.
- Forward-declared `RawWave*` class names in signal.hpp:19-22 —
  covered by batch 35 and excluded by symbol-table.
- No macros are defined in `signal.hpp`.
- No namespace-scope constants are defined in this batch.

No cross-batch arbitrations needed: every parameter ↔ field binding
between Signal and its sole non-trivial counterpart
(`RawWaveHirzel16::RawWaveHirzel16`) is consistent and was already
endorsed in batch 35.
