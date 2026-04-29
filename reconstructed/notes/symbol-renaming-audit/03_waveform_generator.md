# Batch 03 — waveform_generator

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 1 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 1;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/waveform_generator.hpp`
- `reconstructed/src/waveform_generator.cpp`

Cross-reference reads:
- `reconstructed/include/zhinst/error_messages.hpp` — error enums used
  in `format()` calls (those enum names are out-of-scope here; owned by
  a different batch).
- `reconstructed/include/zhinst/signal.hpp` — `Signal` member names
  (`samples_`, `markers_`, `markerBits_`, `channels_`, `length_`,
  `reserveOnly_`) referenced from this TU but defined elsewhere.
- `reconstructed/include/zhinst/value.hpp` — `Value::type_`, `which_`,
  `storage_`, `ValueType`, `VarType` (different batch).
- `nm --demangle _seqc_compiler.so | grep -i WaveformGenerator` for
  symbol-table verification.

### Symbol-table verification (RULES §3, tier 1 — excluded from rename)

All of the following appear verbatim in `nm --demangle` output and are
therefore **excluded**:

- Types: `WaveformGenerator`, `WaveformGeneratorException`,
  `WaveformGeneratorValueException` (typeinfo + vtable symbols).
- Method names (one nm line each, e.g.
  `0000000000248200 t zhinst::WaveformGenerator::WaveformGenerator(...)`,
  `000000000025bca0 t zhinst::WaveformGenerator::getOrCreateWaveform(...)`,
  `0000000000260f20 t zhinst::WaveformGenerator::reverse(zhinst::Signal&)`,
  …): `WaveformGenerator`, `~WaveformGenerator`, `functionExists`,
  `getOrCreateWaveform`, `createDummyWaveform`, `call`, `eval`,
  `readDouble`, `readDoubleAmplitude`, `readInt`, `readPositiveInt`,
  `readWave`, `genericTriangle`, `markerImpl`, `interpolateLinear`,
  `reverse`, `zeros`, `ones`, `sin`, `cos`, `sinc`, `ramp`, `sawtooth`,
  `triangle`, `gauss`, `drag`, `blackman`, `hamming`, `hann`, `rect`,
  `chirp`, `mask`, `marker`, `rand`, `randomGauss`, `randomUniform`,
  `lfsrGaloisMarker`, `rrc`, `vect`, `placeholder`, `join`, `add`,
  `interleave`, `scale`, `multiply`, `cut`, `flip`, `filter`,
  `circshift`, `merge`, `grow`, `WaveformGeneratorException`,
  `~WaveformGeneratorException`, `what`,
  `WaveformGeneratorValueException`,
  `~WaveformGeneratorValueException`.
  All these method names are excluded from rename.
- `nm` shows **no static data members** for `WaveformGenerator` or its
  exception classes; therefore there are no tier-1 excluded data
  members. All non-static fields below are in scope.
- Parameter names, instance data members, locals, and the auxiliary
  free function `floatEqual` (a `t` symbol present in nm) — names are
  not encoded → **in scope** (the `floatEqual` function name itself
  IS in the symbol table and is excluded; only its parameters `a`, `b`
  are in scope).

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `WaveformGeneratorException::message_` | no | medium | holds exception text | keep current (high) | — |
| `WaveformGeneratorException::WaveformGeneratorException::msg` | no | high | trivial ctor forward | keep current (high) | — |
| `WaveformGeneratorValueException::message_` | no | medium | holds exception text | keep current (high) | — |
| `WaveformGeneratorValueException::argIndex_` | no | medium | 1-based arg index | keep current (high) | — |
| `WaveformGeneratorValueException::WaveformGeneratorValueException::msg` | no | high | trivial ctor forward | keep current (high) | — |
| `WaveformGeneratorValueException::WaveformGeneratorValueException::argIndex` | no | medium | parameter index | keep current (high) | — |
| `WaveformGenerator::funcMap_` | no | medium | name→factory registry | keep current (high) | — |
| `WaveformGenerator::funcMap_maxLoadFactor_` | no | low | layout-fidelity slot | keep current (medium) | — |
| `WaveformGenerator::aliasMap_` | no | medium | deprecated→canonical | keep current (high) | — |
| `WaveformGenerator::aliasMap_maxLoadFactor_` | no | low | layout-fidelity slot | keep current (medium) | — |
| `WaveformGenerator::createdNames_` | yes | medium | misleading cache semantics | factoryAlwaysFreshNames_, neverCachedNames_, keep current | — |
| `WaveformGenerator::wavetableFront_` | no | high | shared_ptr to WavetableFront | keep current (high) | not-misnomer |
| `WaveformGenerator::pad_78_` | no | low | reserved/padding slot | keep current (medium) | — |
| `WaveformGenerator::warningCallback_` | no | high | invoked on deprecation/clip | keep current (high) | not-misnomer |
| `WaveformGenerator::reserved_B0_` | no | high | comment + Phase 15a evidence | keep current (high) | not-misnomer |
| `WaveformGenerator::WaveformGenerator::wavetableFront` | no | high | passed to wavetableFront_ | keep current (high) | — |
| `WaveformGenerator::WaveformGenerator::warningCallback` | no | high | passed to warningCallback_ | keep current (high) | — |
| `WaveformGenerator::functionExists::name` | no | high | funcMap_ key | keep current (high) | — |
| `WaveformGenerator::getOrCreateWaveform::name` | no | high | cache/lookup key | keep current (high) | — |
| `WaveformGenerator::getOrCreateWaveform::args` | no | high | factory args | keep current (high) | — |
| `WaveformGenerator::getOrCreateWaveform::factory` | no | high | invoked to build Signal | keep current (high) | — |
| `WaveformGenerator::createDummyWaveform::length` | no | high | sample count to "zeros" | keep current (high) | — |
| `WaveformGenerator::call::name` | no | high | function name | keep current (high) | — |
| `WaveformGenerator::call::args` | no | high | passed through | keep current (high) | — |
| `WaveformGenerator::eval::name` | no | high | function name | keep current (high) | — |
| `WaveformGenerator::eval::args` | no | high | passed through | keep current (high) | — |
| `WaveformGenerator::readDouble::val` | no | high | source Value | keep current (high) | — |
| `WaveformGenerator::readDouble::paramName` | no | high | used in error format | keep current (high) | — |
| `WaveformGenerator::readDouble::funcName` | no | high | used in error format | keep current (high) | — |
| `WaveformGenerator::readDoubleAmplitude::val,paramName,funcName` | no | high | mirrors readDouble | keep current (high) | — |
| `WaveformGenerator::readInt::argIndex` | yes | high | unused inside body | minVal, keep current | coordinated-rename |
| `WaveformGenerator::readPositiveInt::argIndex` | yes | high | name does not match use | minVal, argIndex, keep current | coordinated-rename |
| `WaveformGenerator::readWave::expectedLength` | unsure | low | semantics unclear | keep current, expectedSize, minLength | — |
| `WaveformGenerator::genericTriangle::nPeriods` | yes | high | called as `amplitude` 3-arg path | keep current; rename callers | cross-batch-arbitration |
| `WaveformGenerator::genericTriangle::riseRatio` | no | medium | matches usage | keep current (high) | — |
| `WaveformGenerator::genericTriangle::phase` | no | high | radian phase offset | keep current (high) | — |
| `WaveformGenerator::genericTriangle::amplitude` | no | high | peak magnitude | keep current (high) | — |
| `WaveformGenerator::genericTriangle::length` | no | high | total sample count | keep current (high) | — |
| `WaveformGenerator::markerImpl::isMask` | no | high | switches func name strings | keep current (high) | — |
| `WaveformGenerator::markerImpl::args` | no | high | conventional | keep current (high) | — |
| `WaveformGenerator::interpolateLinear::xPoints` | yes | medium | per-channel start values | startValues, fromValues, xPoints | — |
| `WaveformGenerator::interpolateLinear::yPoints` | yes | medium | per-channel end values | endValues, toValues, yPoints | — |
| `WaveformGenerator::interpolateLinear::markers` | no | medium | OR-source | keep current (high) | — |
| `WaveformGenerator::interpolateLinear::markerBits` | no | medium | OR-source | keep current (high) | — |
| `WaveformGenerator::interpolateLinear::length` | no | high | segment count | keep current (high) | — |
| `WaveformGenerator::reverse::sig` | no | high | input signal | keep current (high) | — |
| `floatEqual::a, floatEqual::b` | no | high | symmetric == comparison | keep current (high) | — |
| `WaveformGenerator::*::args` (all factory funcs) | no | high | conventional | keep current (high) | — |
| `WaveformGenerator::sinc::position` | no | medium | shifts sinc center | keep current (high) | — |
| `WaveformGenerator::sinc::beta` | no | medium | freq/scale param | keep current (high) | — |
| `WaveformGenerator::ramp::startLevel,endLevel` | no | high | matches binary param string | keep current (high) | — |
| `WaveformGenerator::chirp::startFreq,stopFreq` | no | high | matches binary param string | keep current (high) | — |
| `WaveformGenerator::rrc::position,beta,width` | no | medium | matches RRC math | keep current (high) | — |
| `WaveformGenerator::rrc::amplitude` | no | high | peak scaling | keep current (high) | — |
| `WaveformGenerator::lfsrGaloisMarker::*` (locals) | no | medium | matches LFSR math | keep current (high) | — |
| `WaveformGenerator::placeholder::*` locals | no | medium | matches placeholder semantics | keep current (high) | — |
| `WaveformGenerator::join::interpLen,requestedLength` | no | medium | matches interpolation use | keep current (high) | — |
| `WaveformGenerator::interleave::result` | no | high | merge() return | keep current (high) | — |
| `WaveformGenerator::multiply::allReserveOnly,maxNSamples,channels` (locals) | no | medium | descriptive | keep current (high) | — |
| `WaveformGenerator::cut::startIdx,endIdx,cutLen` | yes | medium | semantics of args mismatched | startIdx/endIdx swap, see §3 | — |
| `WaveformGenerator::circshift::shift` | no | medium | rotation amount | keep current (high) | — |
| `WaveformGenerator::merge::requestedLength,count` | no | medium | matches binary | keep current (high) | — |
| `WaveformGenerator::grow::targetLen` | no | high | new length | keep current (high) | — |
| `WaveformGenerator::filter::aSig,bSig,xSig` | no | high | mirrors a/b/x literature | keep current (high) | — |

## 3. Detailed findings

### `WaveformGenerator::createdNames_`  [yes / medium / —]

Evidence:
- `waveform_generator.hpp:240-242` comment claims:
  "Set of waveform names that have already been generated by call().
   Used by getOrCreateWaveform() to short-circuit cached lookups."
- Constructor initializer at `waveform_generator.cpp:100`:
  `, createdNames_({"rand", "randomGauss", "randomUniform", "placeholder"})`
  — i.e. it is **pre-populated** before any waveform has been
  generated, with exactly the four functions whose semantics produce
  fresh output every time (PRNG-based + placeholder).
- `getOrCreateWaveform` body
  (`waveform_generator.cpp:299-309`) treats the *absence* of the name
  in the set as the trigger for the cache-lookup path
  (`getWaveformByFunDescr`), and the *presence* in the set as the
  trigger for the always-call-the-factory path. So membership in
  `createdNames_` means "do NOT consult the cache".
- Source comment block at `waveform_generator.cpp:276-285` and the
  reconstructed `OVERVIEW`/notes describing this behaviour.
- No code path in this TU ever inserts into `createdNames_` after
  construction; insertions into the wavetable's own caches happen via
  `wavetableFront_->newWaveform(...)` instead.

Interpretation:
- The set is **not** a record of names that have been created. It is
  a static, ctor-only allow-list of function names whose results
  must never be deduplicated by `getOrCreateWaveform`. The current
  name describes the literal mechanism (an entry exists once a name
  has been "created" by the bound function) but the contents are
  fixed at construction and never grow, so callers reading the name
  will form the wrong mental model.

Judgement:
- Yes — name implies a runtime cache log, but the field is a static
  exclusion list of factory names that bypass the cache.

Proposals:
- `nonCachedFunctions_` (medium)
- `alwaysFreshFactoryNames_` (medium)
- `factorySkipCacheNames_` (low)
- keep current (low)

Locations consulted:
- declared: include/zhinst/waveform_generator.hpp:240-242
- ctor init: src/waveform_generator.cpp:100
- read: src/waveform_generator.cpp:299-309 (`getOrCreateWaveform`)

### `WaveformGenerator::wavetableFront_`  [no / high / not-misnomer]

Evidence:
- `waveform_generator.hpp:244-245`
  `std::shared_ptr<WavetableFront> wavetableFront_;`
- ctor parameter (`waveform_generator.hpp:116`,
  `waveform_generator.cpp:94,101`) named `wavetableFront`, type
  `std::shared_ptr<WavetableFront>`.
- Method calls on it: `getWaveformByFunDescr`, `newWaveform`,
  `waveformExists`, `getWaveformByName`, `loadWaveform` — all are
  `WavetableFront` methods (cross-checked via header).
- Type `WavetableFront` is itself in the binary symbol table
  (excluded), so the field's type and name align with the
  reconstructed type name.

Interpretation:
- Field type and uses match the name exactly.

Judgement:
- No — name correctly describes both the type and the object's role.

Proposals:
- keep current (high)

### `WaveformGenerator::warningCallback_`  [no / high / not-misnomer]

Evidence:
- `waveform_generator.hpp:250-251`
  `std::function<void(std::string const&)> warningCallback_;`
- Invoked in `call` (deprecation warning),
  `readDoubleAmplitude` (amplitude-clipped warning),
  `markerImpl` (marker > 3 warning), `sinc`/`drag`/`rrc`
  (position-too-large warning), `multiply` (amplitude exceeded). In
  every site the message is constructed via
  `ErrorMessages::format(...)` with a "warning"-class enum
  (`AmplitudeClipped`, `ArgLargerThanLength`, `ValueCapped`,
  `DeprecatedFunc`).
- ctor parameter at `waveform_generator.hpp:117`,
  `waveform_generator.cpp:95,103`.

Interpretation:
- Every invocation passes a warning-class diagnostic message; the
  callback is never used for non-warning purposes.

Judgement:
- No — name accurately describes role.

Proposals:
- keep current (high)

### `WaveformGenerator::reserved_B0_`  [no / high / not-misnomer]

Evidence:
- `waveform_generator.hpp:90-107` (header banner) and
  `waveform_generator.hpp:253-254` extensive Phase-15a comment
  documenting that the binary contains no writer for offset 0xB0
  on a `WaveformGenerator` instance.
- The slot is zero-initialised in the ctor body
  (`waveform_generator.cpp:104` initializer
  `, reserved_B0_()`) and never read again from this TU.
- No `nm` symbol references this offset for any
  WaveformGenerator-method.

Interpretation:
- Field is intentionally unused; current name accurately advertises
  that fact and the byte-fidelity intent.

Judgement:
- No — `reserved_B0_` honestly labels a documented vestigial slot.

Proposals:
- keep current (high)

### `WaveformGenerator::funcMap_maxLoadFactor_` and `aliasMap_maxLoadFactor_`  [no / low / —]

Evidence:
- `waveform_generator.hpp:228-238` comment block explaining that on
  libc++ `unordered_map` is 0x28 bytes and the trailing 4-byte float
  is `max_load_factor`. These two fields are explicit declarations
  to preserve the binary's struct layout.

Interpretation:
- The names match the libc++ implementation detail they reproduce.

Judgement:
- No — descriptive of their layout-fidelity role.

Proposals:
- keep current (medium)

### `WaveformGenerator::pad_78_`  [no / low / —]

Evidence:
- `waveform_generator.hpp:247-248` comment "8 bytes — padding between
  wavetableFront_ and warningCallback_". No reads or writes anywhere
  in the TU other than the ctor's zero-init.

Interpretation:
- The name explicitly says it is padding; no behavioural use.

Judgement:
- No.

Proposals:
- keep current (medium)

### `WaveformGenerator::readInt::argIndex` and `WaveformGenerator::readPositiveInt::argIndex`  [yes / high / coordinated-rename]

Evidence:
- Declarations:
  - `readInt(Value, std::string const&, int minVal, std::string const&)`
    in `waveform_generator.hpp:166-167` — header parameter is named
    **`minVal`**.
  - `readPositiveInt(Value, std::string const&, int minVal, std::string const&)`
    in `waveform_generator.hpp:168-169` — header parameter is named
    **`minVal`**.
- Definitions in `waveform_generator.cpp:465-476` (`readInt`) and
  `waveform_generator.cpp:478-492` (`readPositiveInt`) rename the
  parameter to **`argIndex`**:

  ```
  int WaveformGenerator::readInt(
      Value val, std::string const& paramName, int argIndex,
      std::string const& funcName)
  ```

- Usage of `argIndex` inside `readPositiveInt`:
  - `int result = readInt(val, paramName, argIndex, funcName);`
    (forwarded as the same positional parameter)
  - `static_cast<size_t>(argIndex)` passed as the
    `WaveformGeneratorValueException::argIndex_` field on throw
    (`waveform_generator.cpp:489`).
- Caller usage across this file: every caller passes a small integer
  literal that corresponds to the **1-based argument position** of
  the value being parsed (e.g. `readPositiveInt(args[1], "2 (marker)",
  2, "lfsrGaloisMarker")` at `cpp:1707`,
  `readPositiveInt(args[3], "4 (initial)", 4, "lfsrGaloisMarker")` at
  `cpp:1709`). It is never used as a minimum-value bound by either
  function body (no `result < argIndex` comparison anywhere).
- `readPositiveInt` only enforces `result >= 0` (cpp:484). There is
  no minimum-value check using this parameter.

Interpretation:
- Header and body disagree on the parameter name. The body name
  matches what is actually done with the value (used as the arg-index
  for the resulting exception). The header name (`minVal`) does not
  match any check in the body — `readPositiveInt` only enforces
  `>= 0`, not `>= minVal`.
- Callers exclusively pass 1-based positional arg indices.

Judgement:
- Yes — the header's `minVal` is the misnomer; the value is
  consistently used as an exception arg-index, never as a minimum
  bound.

Proposals:
- Header rename `minVal` → `argIndex` (high) — coordinated for both
  `readInt` and `readPositiveInt` since they share the misnomer.
- Alternatively keep `minVal` and *also* implement a real
  `result >= minVal` check; but that would be a behavioural change
  out of audit scope. Lower-confidence proposal.

Cross-reference:
- Counterpart `WaveformGeneratorValueException::argIndex_` (this
  batch) — same semantics; this proposal aligns the parameter name
  with the field that ultimately stores it.

Locations consulted:
- declared: include/zhinst/waveform_generator.hpp:166-169
- defined: src/waveform_generator.cpp:465-476, 478-492
- callers (sample): src/waveform_generator.cpp:582,586,722,732,746,
  882-886,938-940,1005-1014,1065,1106-1115,1146-1153,1559-1561,
  1612-1619,1662-1665,1704-1709,2306-2307

### `WaveformGenerator::readWave::expectedLength`  [unsure / low / —]

Evidence:
- Declaration `readWave(Value val, std::string const& paramName,
  int expectedLength, std::string const& funcName)` at
  `waveform_generator.hpp:170-171`.
- Body at `waveform_generator.cpp:511-554`: the parameter is **only**
  used as the `argIndex_` field of the
  `WaveformGeneratorValueException` thrown when the Value is not a
  string (`throw WaveformGeneratorValueException(msg, expectedLength);`
  at cpp:523). No length comparison is performed against the actual
  loaded waveform's length anywhere in the body.
- Comment block at cpp:494-510 describes it as "the
  expected length" but the body uses it purely as an integer index.
- Callers vary: most pass `-1` (e.g. cpp:762, 791, 802, 956, 2305,
  2369, 2524, 2612, 2691); some pass small positives that look like
  arg indices (`filter` at cpp:2398-2404 passes `1`, `2`, `3`).

Interpretation:
- Across call sites the value is sometimes a sentinel (`-1`),
  sometimes a 1-based arg index. The body never treats it as a length
  to check.
- `filter`'s `1`, `2`, `3` reads as arg-index. Callers passing `-1`
  are likely passing a sentinel meaning "no further info"; whatever
  it represents, the only consumer is the exception's `argIndex_`
  field.

Judgement:
- Unsure — same situation as `readInt`/`readPositiveInt`'s `argIndex`
  parameter (it ends up in `argIndex_`), but the `-1` sentinel and
  the original "expected length" comment leave room for the
  possibility that the binary did once perform an
  expected-length check that was compiled out. Cannot decide without
  cross-batch context.

Proposals:
- `argIndex` (medium) — matches actual use as exception arg-index and
  unifies with the readInt/readPositiveInt rename above.
- `expectedLength` (low) — keep the documentary intent.
- keep current (low)

Locations consulted:
- declared: include/zhinst/waveform_generator.hpp:170-171
- defined: src/waveform_generator.cpp:511-554 (only use at cpp:523)
- callers: src/waveform_generator.cpp:762, 791, 802, 956, 2305, 2369,
  2398-2404, 2524, 2612, 2691

### `WaveformGenerator::genericTriangle::nPeriods` (and the 3-arg sawtooth/triangle adapters)  [yes / high / cross-batch-arbitration]

Evidence:
- Header signature
  `genericTriangle(int length, double amplitude, double nPeriods,
  double riseRatio, double phase)`
  (`waveform_generator.hpp:174-175`).
- Body at `waveform_generator.cpp:182-219` — uses the third parameter
  `nPeriods` as the divisor in `samplesPerPeriod = length / nPeriods`,
  matching its name.
- However, the **3-arg** call sites in `sawtooth`
  (`waveform_generator.cpp:1129`) and `triangle` (cpp:1167) pass:
  `genericTriangle(length, /*amplitude=*/nPeriods, /*nPeriods=*/phase,
  0.5/1.0, /*phase=*/amplitude)` — i.e. the local variable named
  `nPeriods` flows into the `amplitude` parameter slot, and the local
  named `amplitude` flows into the `phase` slot. The trailing
  comments explicitly describe this as a "binary swaps amp↔nPeriods
  and phase↔phase in registers" patch, GDB-verified.

Interpretation:
- The function-level name `nPeriods` is correct for the 4-arg flow
  through the function body. The misalignment is at the **call sites**
  in `sawtooth` and `triangle`, where the local variable names
  (`amplitude`, `nPeriods`, `phase`) are swapped with respect to the
  parameter slots they fill. So the names that are truly suspect are
  the *locals* in `sawtooth`/`triangle` — not `genericTriangle`'s own
  parameters.

Judgement:
- The genericTriangle parameter `nPeriods` itself is **not** a
  misnomer; the misnomer is in the calling locals. Recording here
  for cross-batch reference because the calling functions are in the
  same TU but may move to a different review batch later, and because
  the locals are genuinely misleading.

Proposals:
- Keep `genericTriangle::nPeriods` (high).
- Rename the 3-arg-path locals in `sawtooth` and `triangle`
  (`amplitude` and `nPeriods`) to reflect their actual slot
  assignment: e.g. `arg2` and `arg3`, or wrap the swap in a comment
  block — but this is best handled in a later micro-pass since
  changing local names in ad-hoc 3-arg paths risks confusing the
  GDB-verified semantics.

Cross-reference:
- The mismatch is internal to this batch but the proposed cleanup is
  a cosmetic local-rename, deferred. No counterpart symbol in another
  batch.

Locations consulted:
- declared/defined: include/zhinst/waveform_generator.hpp:174-175;
  src/waveform_generator.cpp:182-219
- 3-arg call sites: src/waveform_generator.cpp:1126-1131 (sawtooth);
  cpp:1163-1169 (triangle)

### `WaveformGenerator::interpolateLinear::xPoints` and `yPoints`  [yes / medium / —]

Evidence:
- Header signature at `waveform_generator.hpp:177-181`:
  `interpolateLinear(int length, std::vector<double> const& xPoints,
  std::vector<double> const& yPoints, std::vector<uint8_t> const&
  markers, std::vector<uint8_t> const& markerBits)`
- Body at `waveform_generator.cpp:633-684`. Inside the per-sample
  loop:
  ```
  double startVal = xPoints[n];   // 0x25f523
  double endVal   = yPoints[n];   // 0x25f52c
  double sample = startVal + (endVal - startVal) * segDouble / lengthDouble;
  ```
  The vectors hold per-channel **start values** and **end values**
  used for linear interpolation across the segment, NOT (x,y) Cartesian
  coordinates of points.
- The variable names introduced for them in the loop body are
  `startVal` and `endVal`, contradicting the parameter names.

Interpretation:
- The names `xPoints`/`yPoints` suggest 2-D coordinate samples for an
  interpolating spline; in fact each vector is one of the two endpoints
  of a linear ramp performed independently per channel. The body's own
  local names (`startVal`, `endVal`) confirm the actual semantics.

Judgement:
- Yes — `xPoints`/`yPoints` mis-describe the data; they are not
  abscissa/ordinate pairs but per-channel linear-ramp endpoints.

Proposals:
- `startValues` / `endValues` (medium)
- `fromValues` / `toValues` (medium) — matches the binary's
  `"2 (from)"` / `"3 (to)"` parameter strings (see `strings`
  output for `_seqc_compiler.so` showing `2 (from)` and `3 (to)`),
  which strongly suggests the SeqC-level argument labels are
  `from`/`to`.
- keep current (low)

Locations consulted:
- declared: include/zhinst/waveform_generator.hpp:177-181
- defined: src/waveform_generator.cpp:633-684 (loop body cpp:661-674)
- evidence of param-name strings: `strings _seqc_compiler.so |
  grep '^[1-5] ('` shows `2 (from)`, `3 (to)`.

### `WaveformGenerator::cut` locals — `startIdx`, `endIdx`, `cutLen`  [yes / medium / —]

Evidence:
- Body at `waveform_generator.cpp:2302-2337`:
  ```
  int startIdx = readPositiveInt(args[1], "2 (offset)", 1, "cut");
  int endIdx   = readPositiveInt(args[2], "3 (length)", 1, "cut");
  int cutLen   = endIdx - startIdx + 1;
  ```
- The parameter strings shown to the user are `"2 (offset)"` and
  `"3 (length)"` — i.e. arg2 is described as an *offset* and arg3 as
  a *length*. But the local name `endIdx` and the formula
  `cutLen = endIdx - startIdx + 1` treat arg3 as a 1-based **end
  index**, not a length.

Interpretation:
- Either the local name `endIdx` is wrong (if the binary actually
  reads arg3 as a length, then `cutLen = endIdx`), or the
  user-visible parameter name `"3 (length)"` is wrong (if the binary
  treats it as an end index). Comments at cpp:2294-2310 explicitly
  call it both "length" and "end index" in adjacent lines, confirming
  the reconstruction is uncertain.
- The two interpretations produce different output sizes; this is a
  semantic question, not just a naming one. Marking the local names
  as misleading because they encode an unverified interpretation.

Judgement:
- Yes — local names lock in an interpretation that contradicts the
  literal user-facing param string `(length)`. Same name should not
  carry both meanings simultaneously.

Proposals:
- Verify against the binary (GDB or careful disasm) which
  interpretation is correct, then rename to the matching name
  (`length` and `length` accumulator, or `endIdx`/`startIdx` with
  `cutLen = endIdx - startIdx + 1`).
- Until verified: rename `endIdx` → `arg3` and add a TODO comment
  noting the ambiguity (low).
- keep current (low) — explicit that this is a known
  reconstruction-uncertainty.

Cross-reference:
- This is a **type-suspicion / semantics** observation per RULES §2a;
  recorded as a naming finding because the names presuppose an
  interpretation. The semantic question itself belongs in
  `incidental_findings.md` (out of audit scope to write to).

Locations consulted:
- defined: src/waveform_generator.cpp:2287-2337

### Routine positive observations (no separate blocks)

The following call out brief positive matches not warranting full
blocks. Listed inline rather than in §4 because they appear in the
overview table with confidence ≥ medium:

- `funcMap_` — populated by ctor with literal SeqC function names
  (`waveform_generator.cpp:110-144`). Map type matches name.
- `aliasMap_` — checked-first in `call`
  (`waveform_generator.cpp:342-355`); deprecation-warning flow
  matches "alias" semantics. Empty in this binary version
  (cpp:146-148 comment).
- All factory functions take `args` (vector<Value> const&) — name
  conventional; matches the bound-function signature in the symbol
  table.
- Exception `argIndex_` field is stored from the ctor's `argIndex`
  parameter (cpp:74-77) and only read via `argIndex()` accessor
  (header line 70). Mechanical pass-through; named consistently.
- `WaveformGeneratorException::message_` /
  `WaveformGeneratorValueException::message_` — both stored from
  ctor `msg` parameter, returned from `what()` if non-empty
  (cpp:63-68, 81-86). Conventional exception-message field name.
- Header parameters `wavetableFront`, `warningCallback` of the
  ctor are passed straight to the same-named `_` fields.

## 4. Symbols inspected and judged routinely fine

- `floatEqual::a, floatEqual::b` — symmetric `a == b` operands
  (cpp:48-50). Conventional binary-operation names.
- `WaveformGenerator::createDummyWaveform::length` — passed as the
  argument to the registered `"zeros"` factory.
- `WaveformGenerator::*::name` (in `functionExists`,
  `getOrCreateWaveform`, `call`, `eval`) — used uniformly as the
  funcMap/aliasMap key.
- `WaveformGenerator::*::args` (in same methods + every factory) —
  vector<Value> conventional name.
- `WaveformGenerator::eval::name,args` and the local
  `wf`,`results`,`v` (cpp:392-413) — short-lived locals; descriptive.
- `WaveformGenerator::readDouble::val,paramName,funcName` — appear
  in error format calls; names match exactly.
- `WaveformGenerator::readDoubleAmplitude::val,paramName,funcName,
  result` — local `result` is the value being bounds-checked.
- `WaveformGenerator::readWave::name,waveName,lookupName,wf,
  optName` — short-lived locals; semantics clear from immediate use.
- `WaveformGenerator::genericTriangle::sample,t,phaseOffset,
  fallSamples,riseHalfSamples,riseHalfPlusFall,negAmplitude,
  twoAmplitude,samplesPerPeriod` — domain-specific math locals.
- `WaveformGenerator::reverse::sig,channels,reversedSamples,
  reversedMarkers,totalSamples,totalMarkers,numBlocks,srcBlock,
  markerBlockSize` — all match their literal use.
- `WaveformGenerator::markerImpl::funcName,lenParam,valParam,length,
  markerValue,marker,isMask` — names align with role.
- `WaveformGenerator::interpolateLinear::mbpc,sig,lengthDouble,
  numChannels,seg,segDouble,startVal,endVal,sample,marker` — all
  fit usage; in particular the loop's `startVal/endVal` confirm the
  intent of the renamed `xPoints/yPoints` flagged above.
- `checkArgCount::args,funcName,expected` (file-local helper at
  cpp:702-714) — trivial pass-through.
- All factory bodies' locals (`length`, `amplitude`, `phase`,
  `nPeriods`, `position`, `width`, `sigma`, `beta`, `alpha`, `Nm1`,
  `nd`, `w`, `theta`, `samples`, `sig`, etc.) — domain-specific names
  that match the pulse-shape math; not flagged.
- `WaveformGenerator::lfsrGaloisMarker::length,marker,poly,initial,
  state,lsb,markerByte` — match the LFSR algorithm.
- `WaveformGenerator::vect::sig,paramName,val,i` — trivial.
- `WaveformGenerator::placeholder::length,m0,m1,hasMarker0,hasMarker1,
  bits,markerBits,tag` — match placeholder semantics.
- `WaveformGenerator::join::waves,requestedLength,first,interpLen,
  totalLength,result,channels,nSamples,mergedMarker,paramName` — all
  names match their immediate roles.
- `WaveformGenerator::add::first,sum,markers,paramName,s` — trivial.
- `WaveformGenerator::scale::sig,factor,out` — trivial.
- `WaveformGenerator::multiply::waveforms,markerBits,allReserveOnly,
  channels,maxNSamples,sigChannels,name,wf,sig,nSamples,sampleIdx,
  product,marker,wi,amplitudeExceeded,result,totalSamples` — match
  algorithm.
- `WaveformGenerator::cut::sig,samplesPerFrame,totalSamples,
  startSample,endSample,outSamples,outMarkers,markerStart,markerEnd,
  tag` — match the slice operation. (Local-name flagging is on
  `startIdx`/`endIdx`/`cutLen` only.)
- `WaveformGenerator::flip::sig` — trivial.
- `WaveformGenerator::filter::aSig,bSig,xSig,xData,yData,nX,
  na,nb,aData,bData,n,k,y` — match the difference equation.
- `WaveformGenerator::circshift::sig,result,N,channels,
  framesPerChannel,s,markerN,ms,shift` — match the rotation.
- `WaveformGenerator::merge::count,requestedLength,signals,
  paramName,frameCount,mergedBits,numChannels,result,frame,
  ch,sample,mergedMarker` — match the interleave operation.
- `WaveformGenerator::grow::wf,sig,targetLen,channels,
  currentFrames,result,marker,remaining,tag` — match zero-pad
  semantics.
- `WaveformGenerator::interleave::result` — trivial.

## 5. Coverage

- **Fully covered:** all in-scope (per RULES §2/§3) symbols defined in
  `waveform_generator.hpp` and `waveform_generator.cpp` — namely all
  data members of `WaveformGenerator`, `WaveformGeneratorException`,
  `WaveformGeneratorValueException`; all parameter names of all
  methods (incl. `floatEqual`); and all locals that bear on a naming
  judgement.
- **Out of scope (per RULES §3, tier 1 — excluded):** all method
  names, ctor/dtor names, type names, and the free function
  `floatEqual` (function-name only). All these are present in
  `nm --demangle _seqc_compiler.so` output. The symbol table shows
  no static data members for these classes.
- **Out of scope (per RULES §2):** template parameters (none in
  this batch); member type aliases (none here); error-enum names
  used in `ErrorMessages::format(...)` calls
  (`DeprecatedFunc`, `UnknownFunction`, `ArgMustBeConst`,
  `AmplitudeClipped`, `ArgOverflow`, `ArgMustBeString`,
  `UnknownWaveform`, `FuncExactArgs2`, `ArgMustBePositive`,
  `ArgLargerThanLength`, `ArgNotZero`, `ValueCapped`,
  `ValueMustBe1or2`, `LfsrInitZero`, `VectTooManyArgs`,
  `InconsistentChannels`, `AddExpectsMultiWave`, `FuncMinArgs`,
  `FuncArgs2or3`) — these enumerators are declared in
  `error_messages.hpp` and belong to that batch.
- **Type-suspicion observations recorded in §3** (per RULES §2a):
  - `WaveformGenerator::cut::endIdx` — semantic vs. param-name string
    mismatch.
  - `WaveformGenerator::readWave::expectedLength` — possibly-vestigial.
- **Deferred:** none.
- **Status:** complete.
