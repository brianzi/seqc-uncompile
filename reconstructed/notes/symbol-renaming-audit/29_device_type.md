# Batch 29 — device_type

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 8 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 3;
> B3 (already resolved during Phase D/R): 5;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/device_type.hpp`
- `reconstructed/src/device_type.cpp`

Authoritative symbol-table reference:
`nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so`.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `DeviceFamily` (enum) | no | high | type in binary symbols | keep current | not-misnomer |
| `DeviceFamily::*` (enumerators) | no | high | strings in toString rodata | keep current | not-misnomer |
| `DeviceTypeCode` (enum) | no | high | type in binary symbols | keep current | not-misnomer |
| `DeviceTypeCode::*` (enumerators) | no | high | strings in toString/toCode | keep current | not-misnomer |
| `DeviceOption` (enum) | no | high | type in binary symbols | keep current | not-misnomer |
| `DeviceOption::None` | unsure | medium | duplicate of MF=0; sentinel | keep current, drop alias | — |
| `DeviceOption::TenG` | yes | high | placeholder for "10G" | `Option10G`, keep current | — |
| `DeviceOption::Sixteen_W` | yes | high | placeholder for "16W" | `Option16W`, keep current | — |
| other `DeviceOption::*` | no | high | strings in toString rodata | keep current | not-misnomer |
| `sfc::FeaturesCode` (struct) | no | medium | name in source_location literal | keep current | not-misnomer |
| `sfc::FeaturesCode::value` (field) | unsure | low | sole field; bare name | keep current, `bits` | — |
| `sfc::Hf2Option` etc. (enums) | no | high | types in binary symbols | keep current | not-misnomer |
| `sfc::*Option::Bit0xNNNN` (enumerators) | yes | high | recon-invented placeholders | semantic names, keep current | coordinated-rename |
| `DeviceOptionSet` (type) | no | high | type in binary symbols | keep current | not-misnomer |
| `DeviceOptionSet::values_` | unsure | medium | "values" is generic | keep current, `byCode_` | — |
| `DeviceOptionSet::byName_` | no | medium | name-keyed map; faithful | keep current | not-misnomer |
| `DeviceOptionSet::family_` | no | high | matches family() getter | keep current | not-misnomer |
| `DeviceOptionSet::insert::opt` | no | high | DeviceOption value | keep current | — |
| `DeviceOptionSetConstIterator::iter_` | no | medium | wrapped iterator | keep current | — |
| `detail::DeviceTypeImpl` (type) | no | high | type in binary symbols | keep current | not-misnomer |
| `DeviceTypeImpl::clone` (method) | yes | high | binary mangles `doClone` | `doClone` | — |
| `DeviceTypeImpl::code_` | no | high | matches code() getter | keep current | not-misnomer |
| `DeviceTypeImpl::family_` | no | high | matches family() getter | keep current | not-misnomer |
| `DeviceTypeImpl::options_` | no | high | matches options() getter | keep current | not-misnomer |
| `DeviceTypeImpl::ctor::args` (tuple) | no | low | unpacked tuple | keep current | — |
| `detail::OptionCodePair<T>` (type) | no | high | type in binary symbols | keep current | not-misnomer |
| `OptionCodePair::mask` | no | medium | AND-tested vs options bitmask | keep current | not-misnomer |
| `OptionCodePair::code` | no | medium | inserted as DeviceOption code | keep current | not-misnomer |
| `initializeSfcOptions::known` (param) | no | medium | knownOptions array | keep current | not-misnomer |
| `initializeSfcOptions::options` (param) | no | medium | options bitmask | keep current | not-misnomer |
| `initializeSfcOptions::family` (param) | no | high | matches DeviceOptionSet family | keep current | — |
| `generateMfSfc::deviceTypeName` (param) | no | medium | passed to toDeviceTypeCode | keep current | — |
| `generateMfSfc::options` (param) | no | medium | DeviceOptionSet input | keep current | — |
| `DeviceType` (type) | no | high | type in binary symbols | keep current | not-misnomer |
| `DeviceType::impl_` | no | high | pimpl pointer | keep current | not-misnomer |
| `DeviceType::deviceType` (method) | yes | medium | returns Impl*, not DeviceType | `impl`, `deviceTypeImpl` | — |
| `DeviceType::ctor(family,options)::options` | unsure | low | bitmask, not OptionSet | keep current, `optionsMask` | — |
| `DeviceType::ctor(string,string)::options` | no | medium | newline-joined options string | keep current | — |
| `DeviceType::ctor(string,vector)::options` | no | medium | option-name vector | keep current | — |
| `DeviceType::belongsTo::f` | yes | low | bare 1-letter name | `family` | — |
| `DeviceType::operator==::lhs/rhs` | no | high | standard convention | keep current | — |
| `expand::family` (param) | no | medium | bitmask decomposed | keep current | — |
| `toString(DeviceFamily)::family` | no | high | matches arg role | keep current | — |
| `toString(DeviceTypeCode)::code` | no | high | matches arg role | keep current | — |
| `toString(DeviceOption,family)::opt/family` | no | high | matches usage | keep current | — |
| `toString(DeviceOptionSet,...)::set/family/separator` | no | high | matches usage | keep current | — |
| `toStrings::families` (param) | no | high | input vector | keep current | — |
| `toDeviceTypeCode::s` (param) | unsure | low | bare 1-letter name | `name`, `s` | — |
| `toDeviceFamily::s` (param) | unsure | low | bare 1-letter name | `name`, `s` | — |
| `toDeviceOption::s` (param) | unsure | low | bare 1-letter name | `name`, `s` | — |
| `toDeviceOptions::opts` (param) | no | medium | option-name vector | keep current | — |
| `splitDeviceOptions::s` (param) | unsure | low | bare 1-letter name | `optionsStr`, `s` | — |
| `getOptionsAsString::dt/separator` | no | medium | matches usage | keep current | — |
| `hasOption::dt/opt` | no | high | matches usage | keep current | — |
| predicate `is*::dt` (param) | no | high | DeviceType under test | keep current | — |
| `allDevices()::allDevicesSet` (local static) | no | high | data symbol in binary | keep current | not-misnomer |
| `toDeviceFamily::familyNames` (local static) | no | high | data symbol in binary | keep current | not-misnomer |
| `toDeviceTypeCode::codes` (local static) | unsure | low | not in symbol table | keep current, `deviceTypeCodes` | — |
| `makeDevicesSet` (anon-ns helper) | no | high | name in binary symbols | keep current | not-misnomer |
| `expand::out/bit/f` (locals) | no | medium | math/bit-iter context | keep current | — |
| `isIa::broad_mask/unconditional_mask` (locals) | no | medium | bitmask roles documented | keep current | — |
| `hasMf::probe` (local) | no | medium | probe option for hasOption | keep current | — |
| `toDeviceFamily::it` / `familyNames` lookup | no | medium | std::map iterator | keep current | — |

## 3. Detailed findings

### DeviceFamily (enum) and enumerators  [no / high / not-misnomer]

Evidence:
- `nm --demangle` shows `zhinst::DeviceFamily` in dozens of mangled
  signatures (e.g. `toString(DeviceFamily)`, `expand(DeviceFamily)`,
  `DeviceTypeImpl::family() const`, `DeviceOptionSet::DeviceOptionSet
  (DeviceFamily)`).
- `toString(DeviceFamily)` at `device_type.cpp:322-338` literally maps
  each enumerator to the same identifier text (`HF2`→"HF2",
  `HDAWG`→"HDAWG", `SHF`→"SHF", …); this is faithful from the rodata
  jump table at 0x961eac referenced in the header comment at
  `device_type.hpp:38`.

Interpretation:
- The type name is authoritative. The 11 enumerator names are also
  used verbatim as the `toString` output strings — i.e. the binary
  itself encodes them as labels.

Judgement:
- Per RULES §3 + §4d tier-2, enumerator names that appear verbatim as
  user-facing strings are excluded from rename.

Locations consulted:
- declared: include/zhinst/device_type.hpp:40-53
- toString: src/device_type.cpp:322-338
- nm output for `zhinst::DeviceFamily` symbols

### DeviceTypeCode (enum) and enumerators  [no / high / not-misnomer]

Evidence:
- `nm --demangle` shows `zhinst::DeviceTypeCode` in
  `toString(DeviceTypeCode)`, `toDeviceTypeCode`, `allDevices()`,
  `toAwgDeviceType(DeviceTypeCode, AwgSequencerType)`.
- `toString(DeviceTypeCode)` at `device_type.cpp:414-451` and
  `toDeviceTypeCode` at `device_type.cpp:574-619` map every enumerator
  to its identifier text (e.g. `HDAWG8`→"HDAWG8", `SHFQA4`→"SHFQA4").

Interpretation:
- Type name authoritative; all 33 enumerator names appear verbatim as
  user-facing device-type strings.

Judgement:
- Not misnomers.

Locations consulted:
- declared: include/zhinst/device_type.hpp:61-95
- toString / toDeviceTypeCode bodies in src/device_type.cpp

### DeviceOption (enum)  [no / high / not-misnomer]

Evidence:
- `nm --demangle` shows `zhinst::DeviceOption` in
  `toString(DeviceOption, DeviceFamily)`, `toDeviceOption`,
  `DeviceOptionSet::contains(DeviceOption)`,
  `DeviceTypeImpl::hasOption(DeviceOption)`, etc.

Interpretation:
- Authoritative type name.

Judgement:
- Not a misnomer.

Locations consulted:
- declared: include/zhinst/device_type.hpp:112
- nm output

### DeviceOption::None  [unsure / medium / —]

Evidence:
- `device_type.hpp:113-114` declares `None = 0` and `MF = 0` as
  duplicate enumerators of value 0 with a comment "binary uses 0
  dually".
- `toString(DeviceOption, DeviceFamily)` at `device_type.cpp:464`
  treats value 0 as "MF"/"MFK" — `None` has no rendering.
- `toDeviceOption` at `device_type.cpp:780-822` has no `"None"` entry;
  there is no parse path for it.

Interpretation:
- `None` is a recon-introduced alias to give code a "no option"
  spelling for value 0. It does not appear as a user-facing string.
  The binary's serialization treats 0 only as MF/MFK.

Judgement:
- Unsure: the alias is convenient but is a recon invention with no
  binary backing. Whether it is misleading depends on whether any
  caller actually uses `DeviceOption::None`.

Proposals:
- keep current  (medium) — preserves the convenience alias.
- drop alias `None`, force callers to use `MF`  (low) — matches binary
  more strictly but loses readability.

Locations consulted:
- declared: include/zhinst/device_type.hpp:113-114
- toString: src/device_type.cpp:461-497
- toDeviceOption: src/device_type.cpp:780-822

### DeviceOption::TenG  [yes / high / —]

Evidence:
- `device_type.hpp:124`:
  `TenG = 10,   // toString -> "10G"  (renamed; identifier can't start w/ digit)`
- `toString(DeviceOption,family)` at `device_type.cpp:474`:
  `case 10: return "10G";`
- `toDeviceOption` at `device_type.cpp:794`:
  `{"10G",   DeviceOption::TenG},      // DeviceOptionName::e10g`

Interpretation:
- Inline comments at all three sites acknowledge the recon name is a
  workaround because the binary name "10G" cannot be a C++ identifier.
- The original-binary `DeviceOptionName::e10g` literal is the closest
  source-level analogue; the user-facing string is "10G".

Judgement:
- Yes, `TenG` is a placeholder name — neither the binary's RTTI/rodata
  spelling nor an idiomatic project naming. A house-style fix is
  warranted, though the constraint (leading digit) makes any choice
  imperfect.

Proposals:
- `Option10G`  (medium) — common workaround pattern.
- `TenGigabit` / `TenGigabitEthernet` (low) — speculative expansion.
- keep current  (medium) — already documented; cost of churn.

Cross-reference:
- See `Sixteen_W` below for a parallel case.

Locations consulted:
- declared: include/zhinst/device_type.hpp:124
- toString: src/device_type.cpp:474
- toDeviceOption: src/device_type.cpp:794

### DeviceOption::Sixteen_W  [yes / high / —]

Evidence:
- `device_type.hpp:137`: `Sixteen_W = 23, // toString -> "16W"`
- `device_type.cpp:487`: `case 23: return "16W";`
- `device_type.cpp:807`: `{"16W",   DeviceOption::Sixteen_W}, // DeviceOptionName::w16`

Interpretation:
- Same pattern as `TenG`: the binary-side name is "16W" (data) and
  `DeviceOptionName::w16` (anon-ns C++ pointer name). The recon
  spelling `Sixteen_W` is a leading-digit workaround that diverges
  from the rest of the enum's UPPERCASE convention.

Judgement:
- Yes, placeholder name with a stylistic break (underscore + camel
  fragment).

Proposals:
- `Option16W`  (medium) — parallel to proposed `Option10G`.
- `W16`        (low)    — matches anon-ns helper `DeviceOptionName::w16`.
- keep current (medium).

Cross-reference:
- Coordinate with `TenG`.

Locations consulted:
- declared: include/zhinst/device_type.hpp:137
- toString: src/device_type.cpp:487
- toDeviceOption: src/device_type.cpp:807

### sfc::FeaturesCode (struct)  [no / medium / not-misnomer]

Evidence:
- `device_type.hpp:170-197` documents the source: a
  `BOOST_THROW_EXCEPTION` source_location literal embedded in
  `detail::generateMfSfc` records the function as
  `sfc::FeaturesCode zhinst::detail::generateMfSfc(...)`.
- The free-function return type does NOT appear in Itanium mangling, so
  this string-literal evidence is the only hook — but it is faithful
  binary `.rodata` content.

Interpretation:
- Type name is recovered from a hard-coded source-location string in
  the binary, which is a tier-2 form of positive evidence.

Judgement:
- Not a misnomer.

Locations consulted:
- declared: include/zhinst/device_type.hpp:183-197
- generateMfSfc: nm symbol at 0x2de910

### sfc::FeaturesCode::value (field)  [unsure / low / —]

Evidence:
- Sole field of the wrapper, `device_type.hpp:184`:
  `std::uint64_t value;`
- Used as `value` by the explicit conversion operator at
  `device_type.hpp:189` and the `==`/`!=` friends at lines 191-196.

Interpretation:
- Generic name on a single-member POD. There is no binary backing
  (instance fields are never in the symbol table) and no comment
  flagging it as uncertain.

Judgement:
- Unsure. The name is plain but not actively misleading in a wrapper
  this thin.

Proposals:
- keep current  (medium).
- `bits`        (low) — emphasizes the bitmask role per the surrounding
  comment ("composed uint64 bitmask").

Locations consulted:
- declared: include/zhinst/device_type.hpp:184

### sfc::Hf2Option / MfOption / UhfOption / HdawgOption / ShfOption / VhfOption (types)  [no / high / not-misnomer]

Evidence:
- `nm --demangle` shows all six type names verbatim in the
  instantiations of `initializeSfcOptions<sfc::XxxOption, N>` (e.g.
  `initializeSfcOptions<sfc::Hf2Option, 6ul>` at 0x2e0d50).

Interpretation:
- Authoritative type names.

Judgement:
- Not misnomers.

Locations consulted:
- declared: include/zhinst/device_type.hpp:199-273
- nm output for `initializeSfcOptions<sfc::*Option, N>`

### sfc::*Option::Bit0xNNNN (enumerators)  [yes / high / coordinated-rename]

Evidence:
- All enumerators across `Hf2Option`, `MfOption`, `UhfOption`,
  `HdawgOption`, `ShfOption`, `VhfOption` are spelled `Bit0x0001`,
  `Bit0x0002`, etc. — e.g. `device_type.hpp:201-208`:
  ```
  Bit0x0001 = 0x0001,  // -> code 0 (MF/MFK)
  Bit0x0002 = 0x0002,  // -> code 3 (PLL)
  ```
- The header comment block at `device_type.hpp:147-162` openly states
  "Bit values were decoded from the (mask, code) pairs in the 15
  knownOptions arrays".
- Enumerators are never used by name except as initializers for the
  `OptionCodePair<T>::mask` field of the per-family `knownOptions`
  arrays (deferred to phase 14b-ii-b).
- The trailing `// -> code N (NAME)` comment at every enumerator is
  the actual semantic content; the identifier itself only restates the
  numeric value.

Interpretation:
- These names are explicit recon placeholders that encode only the
  literal bit value. They carry no semantic meaning beyond what the
  enumerator's value already shows. A reader cannot distinguish
  `Hf2Option::Bit0x0001` from `MfOption::Bit0x00001` without consulting
  the comment.

Judgement:
- Yes, all `Bit0xNNNN` enumerators are placeholders. They satisfy the
  RULES §4a "generic / non-descript" criterion explicitly.

Proposals:
- Replace each with a semantic name derived from the comment, e.g.
  `Hf2Option::MfMask`, `Hf2Option::PllMask`, `MfOption::MdMask`,
  `UhfOption::QaMask`, `HdawgOption::FfMask`, …  (medium).
  This preserves "this is a bitmask in the SFC vocabulary" while
  saying which DeviceOption the bit maps to.
- Alternatively `Hf2Option::Mf`, `Hf2Option::Pll`, …  (low) — shorter
  but collides with the global `DeviceOption::Mf`.
- keep current  (low) — explicit but unhelpful.

Status: `coordinated-rename` — all 39 enumerators across 6 enums
should be renamed in one pass, since they share the same scheme.

Locations consulted:
- declared: include/zhinst/device_type.hpp:199-273
- header comment block: include/zhinst/device_type.hpp:147-162
- intended use site: per-family `initializeXxxOptions` helpers
  (deferred to phase 14b-ii-b; not present in this file)

### DeviceOptionSet (type)  [no / high / not-misnomer]

Evidence:
- `nm --demangle` shows `zhinst::DeviceOptionSet` in many ctors,
  `insert`, `contains`, `family`, `operator==`, plus as a value type
  in the `initializeSfcOptions` instantiations.

Interpretation:
- Authoritative type name.

Judgement:
- Not a misnomer.

Locations consulted:
- declared: include/zhinst/device_type.hpp:339
- nm output

### DeviceOptionSet::values_  [unsure / medium / —]

Evidence:
- `device_type.hpp:369`:
  `std::unordered_set<DeviceOption> values_;       // +0x00, 40 bytes`
- Used at `device_type.cpp:253` (initial insert in initializer-list
  ctor), `cpp:270` (`values_.find(opt)`), `cpp:274` (`values_.empty()`),
  `cpp:277` (`values_.size()`), `cpp:286` (`values_.insert(opt).second`),
  `cpp:293` (`a.values_ == b.values_` in operator==).
- No binary backing (instance member; never mangled).

Interpretation:
- "values" is a common bare name for "the items the container holds".
  In this case the items are `DeviceOption` codes, kept as the fast
  lookup index parallel to the byName_ map.

Judgement:
- Unsure. Not actively misleading — but a name like `byCode_` would
  parallel `byName_` and make the dual-storage role obvious.

Proposals:
- keep current  (medium).
- `byCode_`     (medium) — parallel to existing `byName_`.

Locations consulted:
- declared: include/zhinst/device_type.hpp:369
- used: src/device_type.cpp:251-293 (multiple)

### DeviceOptionSet::byName_  [no / medium / not-misnomer]

Evidence:
- `device_type.hpp:370`:
  `std::map<std::string, DeviceOption> byName_;    // +0x28, 24 bytes`
- Comment at `device_type.hpp:296`: "Iteration order is alphabetical
  by option name (toString(opt, family))".
- `cpp:254` populates it as `byName_.emplace(toString(opt, family),
  opt)` — key is the option name string.
- `cpp:259-265` exposes its iterator order via begin()/end().

Interpretation:
- The map is keyed by the human-readable option name; the suffix `_`
  denotes a private member. The name accurately describes both the
  storage and its purpose.

Judgement:
- Not a misnomer.

Locations consulted:
- declared: include/zhinst/device_type.hpp:370
- used: src/device_type.cpp:251-289

### DeviceOptionSet::family_  [no / high / not-misnomer]

Evidence:
- `device_type.hpp:371`: `DeviceFamily family_;`
- `cpp:280`: `DeviceFamily DeviceOptionSet::family() const { return
  family_; }`
- Used in operator== (`cpp:293`) and as the second arg to
  `toString(opt, family_)` in `insert` (`cpp:287`).

Interpretation:
- Stores the device family the set is scoped to; the public getter has
  the same name. Faithful.

Judgement:
- Not a misnomer.

Locations consulted:
- declared: include/zhinst/device_type.hpp:371
- used: src/device_type.cpp:280-293

### DeviceTypeImpl::clone (method)  [yes / high / —]

Evidence:
- `nm --demangle _seqc_compiler.so | grep DeviceTypeImpl`:
  `0x2d3280 t zhinst::detail::DeviceTypeImpl::doClone() const`
  (note: `doClone`, not `clone`).
- Header at `device_type.hpp:394-395` declares it as
  `virtual DeviceTypeImpl* clone() const;  // vtable[0]; impl @
  0x2d3280 (doClone in mangled name).`
- `device_type.cpp:82-84` implements it:
  ```
  DeviceTypeImpl* DeviceTypeImpl::clone() const {
      return new DeviceTypeImpl(*this);
  }
  ```
- `cpp:137,148` call it as `other.impl_->clone()` from the copy
  ctor / copy-assign of `DeviceType`.

Interpretation:
- The binary's mangled symbol unambiguously names this method
  `doClone`. The recon already documents this in a comment but kept
  the source-level name `clone`. Per RULES §3, method names from the
  symbol table are authoritative.

Judgement:
- Yes — `clone` is the wrong name; the binary calls it `doClone`.

Proposals:
- `doClone`  (high) — match the binary mangling.

Status: omitted (default) — single-symbol finding; the corresponding
wrapper class `zhinst::utils::Clonable<DeviceTypeImpl>` (visible in
nm as a `typeinfo for` symbol at 0xb09068) is what likely provides
the public `clone()` interface. That is a separate symbol in another
header.

Locations consulted:
- declared: include/zhinst/device_type.hpp:394
- defined:  src/device_type.cpp:82-84
- callers:  src/device_type.cpp:137,148
- nm:       0x2d3280 `DeviceTypeImpl::doClone() const`

### DeviceTypeImpl::code_ / family_ / options_  [no / high / not-misnomer]

Evidence:
- `device_type.hpp:403-405`:
  ```
  DeviceTypeCode  code_;     // +0x08
  DeviceFamily    family_;   // +0x0c
  DeviceOptionSet options_;  // +0x10
  ```
- Each has a public getter with the same base name (`code()`,
  `family()`, `options()`) at lines 397-400 and bodies at
  `cpp:87-98`.

Interpretation:
- Member names exactly mirror their public accessors and types.

Judgement:
- Not misnomers.

Locations consulted:
- declared: include/zhinst/device_type.hpp:403-405
- getters:  src/device_type.cpp:87-98

### DeviceType::deviceType (method)  [yes / medium / —]

Evidence:
- `device_type.hpp:492`:
  `detail::DeviceTypeImpl* deviceType() const;                     // @ 0x2d2c20`
- `device_type.cpp:200-202`:
  ```
  detail::DeviceTypeImpl* DeviceType::deviceType() const {
      return impl_;
  }
  ```
- `nm --demangle` shows the method as
  `zhinst::DeviceType::deviceType() const` — i.e. the binary's
  source-level name is in fact `deviceType`.

Interpretation:
- The method is named `deviceType` in the binary, but it returns the
  impl pointer (`detail::DeviceTypeImpl*`), not a `DeviceType`. The
  name is authoritative per §3 (it is in the symbol table) but the
  body shows the name describes the *containing class*, not what is
  returned. This makes the method a "factually correct" but
  semantically confusing accessor.

Judgement:
- Yes — confusing in context, but the symbol table forces the name.
  Per §3 the type/method name is authoritative; per §4d this is a
  positive-evidence "binary forces the name" case rather than a free
  rename.

Proposals:
- keep current  (high) — symbol table is authoritative.
- `impl` / `deviceTypeImpl`  (low) — only viable if a wrapping
  refactor renames the public API; out of scope for this audit.

Status: omitted — RULES §3 compels keeping the name.

Locations consulted:
- declared: include/zhinst/device_type.hpp:492
- defined:  src/device_type.cpp:200-202
- nm:       0x2d2c20 `DeviceType::deviceType() const`

### DeviceType::DeviceType(DeviceFamily, unsigned long)::options (param)  [unsure / low / —]

Evidence:
- `device_type.hpp:472`:
  `DeviceType(DeviceFamily family, unsigned long options);`
- `cpp:194-197`:
  ```
  DeviceType::DeviceType(DeviceFamily family, unsigned long options) {
      auto factory = detail::makeDeviceFamilyFactory(family);
      impl_ = factory->makeDeviceType(options).release();
  }
  ```
- The same parameter is forwarded to per-family
  `initializeSfcOptions<>(known, family, options)` (binary instantiations
  visible at 0x2e0d50 etc.) where it is AND-tested against the
  `OptionCodePair::mask` field — i.e. it is a packed bitmask of
  family-specific option bits, not a `DeviceOptionSet`.

Interpretation:
- The bare name `options` clashes with the dominant `DeviceOptionSet
  options` semantics in the same file (e.g. `DeviceTypeImpl::options_`,
  `toDeviceOptions(... opts ...)`). Calling it `options` while it is
  in fact a packed bitmask weakens type discoverability.

Judgement:
- Unsure. Not strictly wrong (it is "the options"), but the type and
  role differ from sibling `options` symbols.

Proposals:
- keep current   (medium).
- `optionsMask`  (low).
- `featureBits`  (low) — aligns with the SFC ("features code") domain.

Locations consulted:
- declared: include/zhinst/device_type.hpp:472
- defined:  src/device_type.cpp:194-197
- forwarded into `initializeSfcOptions` family (instantiations in nm)

### DeviceType::belongsTo::f (param)  [yes / low / —]

Evidence:
- `device_type.hpp:491`: `bool belongsTo(DeviceFamily f) const;`
- `cpp:182-185`:
  ```
  bool DeviceType::belongsTo(DeviceFamily f) const {
      return (static_cast<uint32_t>(impl_->family()) &
              static_cast<uint32_t>(f)) != 0;
  }
  ```

Interpretation:
- A 1-letter name for a `DeviceFamily` parameter; sibling getters and
  fields in the same file consistently use `family` for the same role.

Judgement:
- Yes, mild misnomer (non-descript). Low confidence because the name
  is short-lived inside a 4-line function.

Proposals:
- `family`  (high).

Locations consulted:
- declared: include/zhinst/device_type.hpp:491
- defined:  src/device_type.cpp:182-185

### toDeviceTypeCode / toDeviceFamily / toDeviceOption / splitDeviceOptions::s (param)  [unsure / low / —]

Evidence:
- All four free functions take `std::string const& s`:
  - `device_type.hpp:561` `toDeviceTypeCode(std::string const& s)`
  - `device_type.hpp:581` `toDeviceFamily(std::string const& s)`
  - `device_type.hpp:587` `toDeviceOption(std::string const& s)`
  - `device_type.hpp:600` `splitDeviceOptions(std::string const& s)`
- Bodies use `s` as `codes.find(s)` (`cpp:613`), `boost::algorithm::
  starts_with(s, it->first)` (`cpp:921`), `table.find(s)` (`cpp:816`),
  `boost::algorithm::trim_copy_if(s, ...)` (`cpp:741`).

Interpretation:
- 1-letter name. The role at each site is "the input string to parse"
  and is short-lived. Routine but bare.

Judgement:
- Unsure. Borderline — clear from immediate context but bare by
  house-style standards.

Proposals:
- keep current  (medium).
- `name` for the three reverse-lookup functions; `optionsStr` for
  `splitDeviceOptions`  (low).

Locations consulted:
- declared: include/zhinst/device_type.hpp:561,581,587,600
- defined:  src/device_type.cpp:574-925 (multiple)

### `toDeviceTypeCode::codes` (function-local static)  [unsure / low / —]

Evidence:
- `cpp:575`: `static std::unordered_map<std::string, DeviceTypeCode>
  const codes = []{ ... }();`
- `nm --demangle` does NOT show this static (function-local statics of
  this kind are not exported). Compare with the two named statics that
  *are* in nm: `allDevices()::allDevicesSet` (0xb851f0) and
  `toDeviceFamily(...)::familyNames` (0xb85210).
- The binary comment at `cpp:563-572` mentions an original
  static cache `deviceTypeCodesEnd` for the map's end iterator; the
  recon dropped that and just calls `.find()` each call.

Interpretation:
- Recon-chosen identifier; not authoritative. Other lazy-init statics
  in this file follow the pattern of `<thingPlural>` (`familyNames`,
  `allDevicesSet`); `codes` is a slightly less specific fit.

Judgement:
- Unsure. Local-static, never escapes; minor.

Proposals:
- keep current        (medium).
- `deviceTypeCodes`   (low) — parallel to `familyNames`.

Locations consulted:
- declared: src/device_type.cpp:575
- nm: NOT present (in contrast with the two confirmed statics above)

### `toDeviceFamily::familyNames` (function-local static) and `allDevices()::allDevicesSet`  [no / high / not-misnomer]

Evidence:
- `nm --demangle` shows both as data symbols:
  `0xb85210 b zhinst::toDeviceFamily(std::string const&)::familyNames`
  `0xb851f0 b zhinst::allDevices()::allDevicesSet`
- Per RULES §3, function-local static *data* symbols ARE encoded in
  Itanium mangling and are tier-1 authoritative.
- Both names are used at exactly the source-level positions
  (`cpp:902`, `cpp:963`).

Interpretation:
- Static-data names are visible in the binary symbol table, matching
  the recon names verbatim.

Judgement:
- Not misnomers; tier-1 authoritative.

Locations consulted:
- declared: src/device_type.cpp:902 and 963
- nm: 0xb85210 and 0xb851f0

### isIa::broad_mask / unconditional_mask (locals)  [no / medium / —]

Evidence:
- `cpp:649-654`:
  ```
  constexpr uint32_t broad_mask         = 0x46BF7901u;
  constexpr uint32_t unconditional_mask = 0x00000900u;
  if (((broad_mask >> codeVal) & 1u) == 0u) { ... }
  return ((unconditional_mask >> codeVal) & 1u) != 0u;
  ```
- Comment at `cpp:642-643` explains "broad mask covers the codes for
  which the in-bitfield result is meaningful" and "unconditional mask
  marks the two codes ... that are always considered IA".

Interpretation:
- Names accurately describe the comment-documented roles.

Judgement:
- Not misnomers.

Locations consulted:
- defined: src/device_type.cpp:644-655

### hasMf::probe (local)  [no / medium / —]

Evidence:
- `cpp:717-722`:
  ```
  bool hasMf(DeviceType const& dt) {
      DeviceOption probe = (dt.family() == DeviceFamily::MF)
                               ? DeviceOption::MD
                               : DeviceOption::MF;
      return dt.hasOption(probe);
  }
  ```

Interpretation:
- The local is the option to be probed by `hasOption`. Name accurately
  describes the role.

Judgement:
- Not a misnomer.

Locations consulted:
- defined: src/device_type.cpp:717-722

## 4. Symbols inspected and judged routinely fine

(One-liners; no detailed block warranted.)

- `DeviceOptionSetConstIterator` (type) and `iter_` (field): wraps a
  single `std::map<...>::const_iterator`; names are accurate.
- `DeviceOptionSet::insert::opt`, `contains::opt`, ctor's `options`
  (initializer-list) and `family`: each parameter's name matches its
  type and immediate usage.
- `DeviceTypeImpl::ctor::code/family/options` parameters: each forwards
  directly to a same-named member.
- `DeviceTypeImpl::ctor::args` (tuple param): tuple is unpacked
  inline; name conveys "the bundled args" appropriately.
- `OptionCodePair::mask` and `code`: precisely the AND-tested mask and
  the inserted DeviceOption code per the §4 detailed analysis of
  `initializeSfcOptions`.
- `initializeSfcOptions` parameters `known`, `family`, `options`:
  match the in-binary `knownOptions` array name and standard family/
  options bitmask roles.
- `generateMfSfc::deviceTypeName`, `options`: device-type name string
  passed to `toDeviceTypeCode`; the options set whose membership is
  bit-tested.
- `expand::out`, `bit`, `f`: short locals inside an obvious
  bit-iteration loop.
- All `toString(...)` parameter names (`family`, `code`, `opt`, `set`,
  `separator`, `dt`) are the standard idioms for those types.
- `toStrings::families`: input vector named for its elements; matches
  the standard pattern.
- `toDeviceOptions::opts`: parallel to `splitDeviceOptions` returning
  the same kind of vector.
- `getOptionsAsString::dt`, `separator`: standard.
- `hasOption(dt, opt)`: standard.
- `is*(DeviceType const& dt)` predicates: dt is the standard short
  name across the file.
- `operator==/operator</operator<<` lhs/rhs/os parameters: standard
  C++ convention.
- `makeDevicesSet` (anonymous-namespace helper): the binary symbol
  `zhinst::(anonymous namespace)::makeDevicesSet()::$_0` (lambda) and
  its surrounding helper at 0x2d4cb0 confirm the source-level name.
- `DeviceFamily` enumerator `Unknown = 0`: matches the toString
  fall-through that returns `""` (and the textual "unknown" returned
  by the trailing default for *unmapped* values, which is a separate
  concept).
- `DeviceTypeCode::Unknown = 0`: identical pattern; the function
  returns `""` for `Unknown` and the literal `"unknown"` only on the
  out-of-range default path.

## 5. Coverage

- **Fully covered:**
  - `DeviceFamily`, `DeviceTypeCode`, `DeviceOption` enums and all
    enumerators, including the recon-introduced placeholders `TenG`,
    `Sixteen_W`, and the alias `None`.
  - `sfc::FeaturesCode` struct, its `value` field, all six
    `sfc::*Option` enums and all 39 `Bit0xNNNN` enumerators
    (treated as a single coordinated batch in §3).
  - `DeviceOptionSet` (fields `values_`, `byName_`, `family_`),
    `DeviceOptionSetConstIterator` (field `iter_`), method parameters
    `insert::opt`, `contains::opt`, ctors' `options` and `family`.
  - `detail::DeviceTypeImpl`: type, all four ctors and their
    parameters, fields `code_`/`family_`/`options_`, virtual
    `clone()` (flagged as `doClone`).
  - `detail::OptionCodePair<T>` fields and
    `detail::initializeSfcOptions` parameters.
  - `detail::generateMfSfc` parameters.
  - `DeviceType` pimpl wrapper: type, `impl_` field, all ctors and
    their parameters, `belongsTo::f`, `deviceType()` method.
  - All free functions in this header pair: `toString` overloads,
    `expand`, `toStrings`, `toDeviceTypeCode`, `toDeviceFamily`,
    `toDeviceOption`, `toDeviceOptions`, `splitDeviceOptions`,
    `getOptionsAsString`, `hasOption`, `allDevices`, the
    `is*`/`hasMf`/`isDefined` predicates, and the `operator<<` /
    `operator==` / `operator<` friends.
  - Function-local statics `toDeviceTypeCode::codes`,
    `toDeviceFamily::familyNames`, `allDevices()::allDevicesSet`.
  - Notable locals (`isIa::broad_mask`/`unconditional_mask`,
    `hasMf::probe`, `expand::out`/`bit`/`f`).
  - Anonymous-namespace helper `makeDevicesSet`.

- **Deferred:**
  - The per-family `detail::*` device subclasses (`Hf2li`, `Mfli`,
    `Uhfqa`, …) and their `initializeXxxOptions(unsigned long)`
    helpers — these are explicitly noted in `device_type.hpp:13-15`
    as deferred to phase 14b-ii-b and are not present in either of
    the audited files. They will need a separate batch when their
    sources land.
  - `DeviceType::ctor(string, vector<string>)` body parsing details
    are still scaffolded via `GenericDeviceType`; the parameter names
    (`deviceType`, `options`) are inspected here, but the
    `GenericDeviceType` symbols themselves belong to batch 40 (per
    the prompt's reference list).

- **Not covered (out of scope per RULES §2/§3):**
  - All standard-library and Boost identifiers and their template
    parameters (e.g. `std::map<...>`, `boost::algorithm::is_space`).
  - All free-function and method names that appear in the binary
    symbol table — these are listed in §3 as `not-misnomer` blocks
    (when discussed) but are not candidates for rename.
  - `using map_type` / `using underlying_iterator` /
    `using const_iterator` member type aliases inside
    `DeviceOptionSetConstIterator` and `DeviceOptionSet` — RULES §2
    excludes member type aliases.
  - The pybind / Python glue: no such code in these two files.
