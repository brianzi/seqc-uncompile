# Batch 22 — device_factories

## 1. Files considered

- `reconstructed/include/zhinst/device_factories.hpp`
- `reconstructed/src/device_factories.cpp`

Cross-referenced (read-only):

- `reconstructed/include/zhinst/device_subclasses.hpp`
- `reconstructed/include/zhinst/device_type.hpp`
- `reconstructed/src/device_type.cpp`

Authoritative symbol table:

- `nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so`

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `DeviceFamilyFactory::makeDeviceType::opts` | no | medium | local cluster uses `opts` | keep current | not-misnomer |
| `DeviceFamilyFactory::doMakeDeviceType::opts` (all overrides) | no | medium | matches subclass ctor param | keep current | not-misnomer |
| `Hf2Factory::doMakeDeviceType::sub` (and twins) | no | low | local, narrow scope | keep current | — |
| `makeDeviceFamilyFactory::family` | no | medium | matches caller param name | keep current | not-misnomer |
| `DeviceOpts::SubtypeMask` | unsure | low | unused; header constant | keep current; remove if dead | in-scope (no DeviceOpts symbols/strings in nm) |
| `DeviceOpts::Subtype1` … `Subtype4` | unsure | low | slot index, per-family meaning | keep current; `SubtypeSlotN`; remove if dead | in-scope (no DeviceOpts symbols/strings in nm) |
| `DeviceOpts::FF` | unsure | low | unused; meaning unverified | keep current; `FFOption` | in-scope (no DeviceOpts symbols/strings in nm) |
| `DeviceOpts::RTR` | unsure | low | unused; meaning unverified | keep current; `RTROption` | in-scope (no DeviceOpts symbols/strings in nm) |
| `DeviceOpts::PLUS` | unsure | low | unused; meaning unverified | keep current; `PLUSOption` | in-scope (no DeviceOpts symbols/strings in nm) |
| `DeviceOpts::LRT` | unsure | low | unused; meaning unverified | keep current; `LRTOption` | in-scope (no DeviceOpts symbols/strings in nm) |
| `(anon)::kSubtypeMask` | no | medium | matches usage exactly | keep current | not-misnomer |
| `(anon)::kSubtype1` … `kSubtype4` | no | low | slot id; comments misleading | keep current; rename slot semantically | — |
| `DeviceOpts` (namespace) | unsure | low | duplicate of anon-ns set | keep current; merge with anon-ns | — |

(All factory class names — `DeviceFamilyFactory`, `NoDeviceTypeFactory`,
`UnknownDeviceTypeFactory`, `Hf2Factory`, `MfFactory`, `UhfFactory`,
`HdawgFactory`, `ShfFactory`, `ShfaccFactory`, `GhfFactory`,
`PqscFactory`, `QhubFactory`, `HwmockFactory`, `VhfFactory` — and all
method names visible in `nm` are excluded per §3 and listed in §5.)

## 3. Detailed findings

### `DeviceFamilyFactory::makeDeviceType::opts`  [no / medium / not-misnomer]

Evidence:

- include/zhinst/device_factories.hpp:114
  `std::unique_ptr<DeviceTypeImpl> makeDeviceType(unsigned long opts);`
- src/device_factories.cpp:44
  `std::unique_ptr<DeviceTypeImpl> DeviceFamilyFactory::makeDeviceType(unsigned long opts) { return doMakeDeviceType(opts); }`
- src/device_type.cpp:194
  `DeviceType::DeviceType(DeviceFamily family, unsigned long options) { … impl_ = factory->makeDeviceType(options).release(); }`
- include/zhinst/device_subclasses.hpp:50,57,153,212,236,283
  Subclass ctors all use `unsigned long opts`:
  `explicit Hf2li(unsigned long opts);`, `explicit Shf(unsigned long opts);`, etc.
- src/device_factories.cpp:109,126,149,170,190,217,234,250,278,292
  All `doMakeDeviceType(unsigned long opts)` overrides use `opts` and
  forward it unchanged into the subclass ctor.

Interpretation:

- The value is the per-family device-options bitmask. Two short names
  are in use across the codebase: `opts` (factories, subclass ctors)
  and `options` (`DeviceType` ctor only — single-site).
- Within the device_factories ↔ device_subclasses cluster `opts` is
  the unanimous convention.

Judgement:

- Not a misnomer. Mild he-said/she-said with the single `options`
  call site in `DeviceType::DeviceType`, but the local cluster's
  consistency makes `opts` the more defensible name here.

Proposals:

- keep current  (high)

Cross-reference:

- Counterpart `DeviceType::DeviceType(2-arg)::options` lives in
  batch 29 (device_type, pending).

Locations consulted:

- declared: include/zhinst/device_factories.hpp:114,121
- defined : src/device_factories.cpp:44, plus all ten override sites
- callers : src/device_type.cpp:189-196

### `DeviceFamilyFactory::doMakeDeviceType::opts`  [no / medium / not-misnomer]

Evidence:

- include/zhinst/device_factories.hpp:121,164
  Pure-virtual and macro-generated overrides both use `opts`.
- src/device_factories.cpp:109,126,149,170,190,217,234,250,278,292
  Every derived override forwards `opts` directly to the matching
  subclass ctor (whose own param is also `opts`).
- nm — `Hf2Factory::doMakeDeviceType(unsigned long)` etc. Method
  name itself is excluded; this finding is about the *parameter*.

Interpretation:

- All ten overrides take the same value, named identically, and
  forward it identically.

Judgement:

- Not a misnomer; coordinated and locally consistent.

Proposals:

- keep current  (high)

Cross-reference:

- Same cluster as `makeDeviceType::opts` above; same arbitration
  with batch 29.

Locations consulted:

- declared: include/zhinst/device_factories.hpp:121,136,150,164
- defined : src/device_factories.cpp (all `doMakeDeviceType` bodies)

### `Hf2Factory::doMakeDeviceType::sub` (and `Mf|Uhf|Hdawg|Shfacc|Ghf|Vhf` twins)  [no / low / —]

Evidence:

- src/device_factories.cpp:110,127,150,171,218,235,293
  `auto sub = opts & kSubtypeMask;`
  followed by `if (sub == kSubtypeN) return new XxxN(opts);`

Interpretation:

- Variable holds the masked-out subtype-selector bits ([8:6]) used
  immediately for the dispatch decision. `sub` is short for
  "subtype" and is the obvious abbreviation.

Judgement:

- Not a misnomer; locally clear given the constant names it is
  compared against.

Proposals:

- keep current  (medium)
- `subtype`     (low)  — only if a wider rename of subtype-related
                          identifiers is undertaken.

Locations consulted:

- src/device_factories.cpp:110, 127, 150-156, 171, 218, 235, 293

### `makeDeviceFamilyFactory::family`  [no / medium / not-misnomer]

Evidence:

- include/zhinst/device_factories.hpp:194
  `makeDeviceFamilyFactory(DeviceFamily family);`
- src/device_factories.cpp:303
  `makeDeviceFamilyFactory(DeviceFamily family) { switch (family) { … } }`
- src/device_type.cpp:188-196
  Both call sites pass a parameter literally named `family`:
  `DeviceType::DeviceType(DeviceFamily family) { auto factory = detail::makeDeviceFamilyFactory(family); … }`

Interpretation:

- Parameter name matches caller name and matches the type
  (`DeviceFamily`).

Judgement:

- Not a misnomer.

Proposals:

- keep current  (high)

Locations consulted:

- declared: include/zhinst/device_factories.hpp:194
- defined : src/device_factories.cpp:303
- callers : src/device_type.cpp:188-196

### `DeviceOpts::SubtypeMask`, `Subtype1` … `Subtype4`  [unsure / low / verify-not-original]

Evidence:

- include/zhinst/device_factories.hpp:81-91 — declares the namespace
  `DeviceOpts` with `SubtypeMask` (0x1C0), `Subtype1`..`Subtype4`,
  `FF`, `RTR`, `PLUS`, `LRT`.
- `grep -rnE "DeviceOpts::(FF|RTR|PLUS|LRT|Subtype)" reconstructed/`
  returns **zero** matches outside the declaration.
- src/device_factories.cpp:25-30 — anonymous namespace inside the
  .cpp re-declares `kSubtypeMask`, `kSubtype1`..`kSubtype4` with
  identical values and these are the constants actually consumed
  (15 use sites in this file).
- nm — `DeviceOpts::*` does not appear (not exported), consistent
  with `inline constexpr` namespace-scope constants. They are
  therefore in scope per §3.

Interpretation:

- The header set `DeviceOpts::*` is dead code; the live set is the
  anonymous-namespace duplicate in the .cpp. Two parallel constant
  sets exist with the same numeric values and overlapping but
  non-identical naming (`SubtypeMask` vs `kSubtypeMask`; `Subtype1`
  vs `kSubtype1`).
- The slot semantics differ across families: slot 1 is `Hf2li`,
  `Mfli`, `Hdawg4`, `Uhfli`, `Ghfli`, `Vhfli` (often "LI"
  but Hdawg uses it for the 4-channel variant); slot 2 is
  `Hf2is`, `Mfia`, `Hdawg8`, `Uhfawg`. There is no single
  cross-family meaning, so a numeric "Subtype1..4" naming is
  defensible.

Judgement:

- Unsure as a name *misnomer* — the names are not actively wrong, but
  the parallel duplicate set raises the question whether the header
  set was intended to replace the anon-ns set or vice versa. Status
  `verify-not-original` because the situation is structural rather
  than purely a name choice.

Proposals:

- keep current names; remove the dead header set  (medium)
- rename the anon-ns set to drop the `k` prefix and re-export via
  `DeviceOpts::` (or vice versa) — coordinated cleanup  (low)
- `SubtypeMask` → `kSubtypeMask` for naming-style parity  (low)

Locations consulted:

- declared: include/zhinst/device_factories.hpp:80-91
- declared: src/device_factories.cpp:24-30
- used    : src/device_factories.cpp:110,127,150-156,171,218,235,293

### `DeviceOpts::FF`, `RTR`, `PLUS`, `LRT`  [unsure / low / verify-not-original]

Evidence:

- include/zhinst/device_factories.hpp:87-90
  `constexpr unsigned long FF = 0x020;` (and RTR=0x2000, PLUS=0x4000,
  LRT=0x8000)
- No use sites anywhere in `reconstructed/`.
- `DeviceOption` enum (batch 31, device_constants) and `sfc::*Option`
  enums in `device_subclasses.hpp` carry the bit-value vocabulary
  that is actually consumed.

Interpretation:

- These are unused header constants. The names mimic well-known
  Zurich Instruments option mnemonics (FF, RTR, PLUS, LRT) but
  without consumers we cannot verify the bit-value mapping against
  the binary's option encoder.

Judgement:

- Unsure. The names look plausible and short, matching a vendor
  convention, but lack of usage means no live evidence either way.
  Status `verify-not-original` — synthesis should decide whether to
  cross-check against the SFC option tables or simply delete.

Proposals:

- keep current  (low)
- delete (dead code)  (medium)
- if kept: `FFOption`, `RTROption`, `PLUSOption`, `LRTOption` for
  clarity that they are bit constants  (low)

Locations consulted:

- declared: include/zhinst/device_factories.hpp:87-90
- used    : nowhere

### `(anon)::kSubtypeMask`  [no / medium / not-misnomer]

Evidence:

- src/device_factories.cpp:25 `constexpr unsigned long kSubtypeMask = 0x1C0ul; // bits [8:6]`
- src/device_factories.cpp:110,127,150,171,218,235,293
  `auto sub = opts & kSubtypeMask;` — the only operations on this
  constant are bitwise-AND used as a mask.

Interpretation:

- The value is used solely as a bit mask; the name says exactly that.

Judgement:

- Not a misnomer.

Proposals:

- keep current  (high)

Locations consulted:

- declared: src/device_factories.cpp:25
- used    : src/device_factories.cpp (7 sites)

### `(anon)::kSubtype1` … `kSubtype4`  [no / low / —]

Evidence:

- src/device_factories.cpp:26-29 — values `0x40`, `0x80`, `0xC0`,
  `0x100`. Inline comments claim "slot 1 (LI variants)",
  "slot 2 (IS/AWG/8 variants)", "slot 3 (QA variants)",
  "slot 4 (IA variants)".
- Cross-check usage:
  - slot 1 → `Hf2li`, `Mfli`, `Uhfli`, `Hdawg4`, `Ghfli`, `Vhfli`
  - slot 2 → `Hf2is`, `Mfia`, `Uhfawg`, `Hdawg8`, `Shfppc4`
  - slot 3 → `Uhfqa`
  - slot 4 → `Uhfia`

Interpretation:

- The comment generalisations ("LI variants", "IS/AWG/8 variants",
  "IA variants") do not hold across families: slot 1 covers `Hdawg4`
  (channel count, not LI); slot 2 covers `Hdawg8` and `Shfppc4`
  (not just IS/AWG); slot 4 holds `Uhfia` only (called "IA" but it
  is also documented as `Mfia` at slot 2).
- The numeric `kSubtype1..4` names themselves carry no false
  semantic claim; the inline comments are the misleading element.

Judgement:

- Not a misnomer for the *names*. The comments are misleading but
  comments are out of audit scope.

Proposals:

- keep current  (high)
- `kSubtypeSlot1` … `kSubtypeSlot4` for slight clarity  (low)

Locations consulted:

- src/device_factories.cpp:25-29 and all use sites already cited

### `DeviceOpts` (namespace name)  [unsure / low / —]

Evidence:

- include/zhinst/device_factories.hpp:81 `namespace DeviceOpts { … }`
- The namespace contains only unused constants; see two preceding
  blocks.
- Parallel concept exists as `DeviceOption` enum (batch 31 device
  constants) and `sfc::HdawgOption`/`MfOption`/etc. enums.

Interpretation:

- Multiple "device option" identifiers exist (`DeviceOption`,
  `DeviceOptionSet`, `sfc::*Option`, plus this `DeviceOpts`
  namespace) with overlapping responsibilities.
  `DeviceOpts` does not clearly say which slot of that taxonomy it
  occupies.

Judgement:

- Unsure. The namespace is dead and its name is generic-and-short
  in a space where similarly-named-but-distinct types exist; if it
  is kept, a more specific name would help. If it is removed, the
  question is moot.

Proposals:

- keep current  (low)
- remove namespace (dead code)  (medium)
- if kept: `DeviceOptsBits`, `DeviceFactoryOpts`  (low)

Locations consulted:

- declared: include/zhinst/device_factories.hpp:81-91
- used    : nowhere

## 4. Symbols inspected and judged routinely fine

- `Hf2Factory::doMakeDefault`, `MfFactory::doMakeDefault`,
  `UhfFactory::doMakeDefault`, `HdawgFactory::doMakeDefault`,
  `ShfFactory::doMakeDefault`, `ShfaccFactory::doMakeDefault`,
  `GhfFactory::doMakeDefault`, `PqscFactory::doMakeDefault`,
  `QhubFactory::doMakeDefault`, `HwmockFactory::doMakeDefault`,
  `VhfFactory::doMakeDefault`, `NoDeviceTypeFactory::doMakeDefault`,
  `UnknownDeviceTypeFactory::doMakeDefault`,
  `DeviceFamilyFactory::doMakeDefault` — virtual methods that
  construct a default-options instance of the family. The "do"
  prefix and the verb "makeDefault" together describe exactly what
  each override does. These symbols are not in `nm` (likely
  reached only through the vtable) and so technically remain in
  scope per §3, but the names match usage unambiguously.
- `NoDeviceTypeFactory::doMakeDeviceType::opts` (parameter named
  in the declaration but commented `/*opts*/` in the definition —
  the parameter is unused but the declared name is consistent
  with siblings).
- Similarly `UnknownDeviceTypeFactory::doMakeDeviceType::opts`,
  `PqscFactory::doMakeDeviceType::opts`,
  `QhubFactory::doMakeDeviceType::opts`,
  `HwmockFactory::doMakeDeviceType::opts` — all unused but
  declared consistently.
- `ZHINST_DECLARE_FACTORY` (macro): mechanical, three-line
  expansion clearly parameterised by class name.

## 5. Coverage

**Fully covered:**

- All in-scope identifiers in `device_factories.hpp` and
  `device_factories.cpp`: parameter names of the three base methods
  and ten override pairs; the `family` parameter of the dispatcher
  free function; namespace-scope constants in both `DeviceOpts` and
  the anonymous namespace; the namespace name `DeviceOpts`; the
  declaration macro `ZHINST_DECLARE_FACTORY`; the "do-prefix"
  virtual method names.

**Deferred:**

- Cross-batch arbitration of `opts` ↔ `options` is recorded under
  the relevant detailed block; resolution sits with batch 29
  (device_type, pending) where the single `options` call site lives.

**Not covered (out of scope per RULES §2/§3):**

- All factory **type names** — `DeviceFamilyFactory`,
  `NoDeviceTypeFactory`, `UnknownDeviceTypeFactory`, `Hf2Factory`,
  `MfFactory`, `UhfFactory`, `HdawgFactory`, `ShfFactory`,
  `ShfaccFactory`, `GhfFactory`, `PqscFactory`, `QhubFactory`,
  `HwmockFactory`, `VhfFactory`. Each has a `vtable for …` /
  `typeinfo for …` / `typeinfo name for …` triple in
  `nm --demangle _seqc_compiler.so` (sample lines:
  `vtable for zhinst::detail::Hf2Factory @ 0xb093d0`,
  `typeinfo name for zhinst::detail::DeviceFamilyFactory @ 0x9622bc`).
- All factory **method names** that appear in `nm`:
  `DeviceFamilyFactory::~DeviceFamilyFactory`,
  `DeviceFamilyFactory::makeDeviceType()` (zero-arg) and
  `DeviceFamilyFactory::makeDeviceType(unsigned long)`,
  every per-family `makeDefault()` and `doMakeDeviceType(unsigned long)`,
  every per-family destructor, plus
  `NoDeviceTypeFactory::makeDefault`, `NoDeviceTypeFactory::~…`,
  `UnknownDeviceTypeFactory::makeDefault`,
  `UnknownDeviceTypeFactory::doMakeDeviceType`,
  `UnknownDeviceTypeFactory::~…`,
  `makeDeviceFamilyFactory(DeviceFamily)` (free function).
  All authoritatively excluded under §3 tier-1 (type/method/
  free-function names in the symbol table). Sample nm lines:
  `0x2e05b0 t zhinst::detail::DeviceFamilyFactory::makeDeviceType(unsigned long)`,
  `0x2e0590 t zhinst::detail::DeviceFamilyFactory::makeDeviceType()`,
  `0x2e05d0 t zhinst::detail::makeDeviceFamilyFactory(zhinst::DeviceFamily)`,
  `0x2e09e0 t zhinst::detail::Hf2Factory::makeDefault()`,
  `0x2e0a40 t zhinst::detail::Hf2Factory::doMakeDeviceType(unsigned long)`.
- `DeviceTypeImpl`, `DeviceFamily`, all `Hf2`/`Mf`/`Uhf`/… subclass
  type names: out of scope here (analysed in batches 29 and 41).
