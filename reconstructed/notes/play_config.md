# PlayConfig and the CWVF Register {#notes_play_config}

`PlayConfig` captures *how* a single `playWave` (and friends)
executes: which channels output the waveform, what sample rate
to use, which marker bits to drive, whether the play holds or
forwards, what suppress mask gates the outputs, and a handful
of single-bit modifiers (`now`, `hold`, `dummy`, `is4Channel`).

The struct is built by the front-end during lowering of
`playWave`-family custom functions, attached to the
corresponding `Node` in the waveform IR, threaded unchanged
through the optimisation passes and the prefetcher, and finally
packed into a 32-bit **CWVF** ("current waveform configuration")
word by `encodeCwvf()`.  The packed word is what the emitted
sequencer code writes to the CWVF register.

Header / source:

- `reconstructed/include/zhinst/waveform/play_config.hpp`
- `reconstructed/src/waveform/play_config.cpp`

[TOC]

## Fields

| Field          | Type       | JSON key       | Role                                                                                |
|----------------|------------|----------------|-------------------------------------------------------------------------------------|
| `channelMask`  | `uint32_t` | `channelMask`  | Per-channel enable mask; forced to `1` in the encoded word when `hold` or `dummy`. |
| `rate`         | `int32_t`  | `rate`         | Sample-rate divider; negative = "use `defaultRate` passed to `encodeCwvf`".         |
| `suppress`     | `uint32_t` | `suppress`     | 14-bit suppress mask; replaced by `holdSuppressExceptSigouts` when `hold`.          |
| `is4Channel`   | `bool`     | `is4Channel`   | When set, places `channelMask` in the 4-channel field of the encoded word.          |
| `markerBits`   | `uint32_t` | `markerBits`   | 4 marker bits driven on the output during this play.                                |
| `trigger`      | `uint32_t` | `trigger`      | 2-bit trigger-source / mode selector.                                               |
| `precompFlags` | `uint32_t` | `precompFlags` | 2-bit pre-compensation filter flags.                                                |
| `now`          | `bool`     | `now`          | Dispatch-time flag: play immediately, skip the trigger wait.                        |
| `hold`         | `bool`     | `hold`         | Sustain the previous output (no new samples consumed).                              |
| `dummy`        | `bool`     | `dummy`        | Placeholder play with no sample output (slot reservation).                          |

## Encoded CWVF word

`encodeCwvf(defaultRate)` packs the fields into a single 32-bit
word with the following layout:

| Bits   | Width | Field             | Notes                                                                |
|-------:|------:|-------------------|----------------------------------------------------------------------|
|  0..1  | 2     | `channels`        | `(hold \|\| dummy) ? 1 : channelMask`                                |
|  2..5  | 4     | `rate`            | `max(rate, 0)` — the sign bit is moved into the `defaultRate` flag   |
|  6..19 | 14    | `suppress`        | `hold ? holdSuppressExceptSigouts : suppress`                        |
| 20..21 | 2     | `fourChannel`     | `is4Channel ? channelMask : 0`                                       |
| 22     | 1     | `defaultRate`     | `rate < 0 ? 1 : 0` — record that `defaultRate` argument was used     |
| 23     | 1     | `dummy`           | `hold \|\| dummy`                                                    |
| 24..27 | 4     | `markerBits`      | `markerBits & 0xF`                                                   |
| 28..29 | 2     | `trigger`         | `trigger & 0x3`                                                      |
| 30..31 | 2     | `precompFlag`     | `precompFlags & 0x3`                                                 |

The packing constants are exposed as `static constexpr`
`*Shift` and `*Mask` members on `PlayConfig` and are the
canonical reference for the bit layout.

`holdSuppressExceptSigouts = 0x27C` (decimal 636) is the
suppress-mask value substituted into the encoded word when
`hold` is set; it suppresses every output path except the
signal outputs.

## Hold / dummy entanglement

`hold` and `dummy` share an encoding: both force the
2-bit channel field to `1` and set the `dummy` bit.  `hold`
*also* swaps the suppress mask for the sigouts-only sentinel.
The intent is that a "held" play behaves, from the sequencer's
point of view, like a 1-channel dummy with non-signal paths
muted; the wire format does not need to distinguish the two
cases.

## Comparison semantics

`operator!=` is **deliberately asymmetric**:

- `now` is excluded entirely.  It is a transient dispatch flag,
  not part of the play's identity.
- `hold` is only compared when `rate > 0`.  Below that, the
  play is at the default rate and `hold` has no observable
  effect.

The asymmetry is intentional for play-word deduplication inside
the prefetcher; do **not** use this operator as a substitute
for exact field-wise equality.

## JSON serialisation

`toJson()` emits all ten fields above as a flat object, using
the JSON keys in the table.  Numeric address-typed fields are
written as unsigned integers; `rate` is signed; the four
booleans (`is4Channel`, `now`, `hold`, `dummy`) are JSON
booleans.

`fromJson(jv)` is the inverse and accepts the same keys.
Unsigned numeric fields are parsed via
`boost::json::value::to_number<uint64_t>()` (so integer,
unsigned, and double JSON values are all accepted and
range-checked); `rate` is parsed via `as_int64()`; the booleans
via `as_bool()`.

## See also

- \ref notes_special_registers — the CWVF word is the value the
  sequencer code writes to the CWVF register.
- \ref notes_custom_functions — `playWave` and friends that
  populate `PlayConfig` from their argument lists.
- \ref notes_prefetch_scheduling — the prefetcher consumes
  `PlayConfig` to decide cache placement and to deduplicate
  play words.
- \ref notes_opcode_map — `suser` / `luser` instructions used
  to write / read the CWVF register.
