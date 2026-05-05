# Batch 40 — generic_device_type

## 1. Files considered

- `reconstructed/include/zhinst/device/generic_device_type.hpp`
- `reconstructed/src/device/generic_device_type.cpp`

Cross-references:

- Batch 22 (device_factories) — alternate construction paths feeding
  `GenericDeviceType` from string inputs.
- Batch 29 (device_type) — sibling `DeviceType(string, vector<string>)`
  ctor with parameters of identical names; the `DeviceTypeImpl::clone()`
  → `doClone()` finding establishes the vtable shape that
  `GenericDeviceType` reuses (no clone override; same `doClone`
  reused in slot +0x10 of the GDT vtable @ 0xb09090).
- Batch 30 (awg_device_props), 31 (device_constants) — context for the
  device-type / family / options vocabulary used here.

The class itself is tiny: it adds no fields beyond `DeviceTypeImpl`,
declares one ctor and one (defaulted) dtor, and provides no other
methods. The only in-scope symbols are the two ctor parameters.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `detail::GenericDeviceType` (type) | — | — | in symbol table | — | (out of scope §3) |
| `GenericDeviceType::GenericDeviceType` (ctor) | — | — | in symbol table | — | (out of scope §3) |
| `GenericDeviceType::~GenericDeviceType` (dtor) | — | — | in symbol table | — | (out of scope §3) |
| `GenericDeviceType::ctor::deviceType` (param) | no | medium | passed to toDeviceTypeCode/Family | keep current | not-misnomer |
| `GenericDeviceType::ctor::options` (param) | no | medium | option-name vector for toDeviceOptions | keep current | not-misnomer |

(No fields. No locals — the body is empty; all work is in the
member-initializer list.)

## 3. Detailed findings

### GenericDeviceType::ctor::deviceType (param)  [no / medium / not-misnomer]

Evidence:

- include/zhinst/device/generic_device_type.hpp:37
  `GenericDeviceType(std::string const& deviceType, std::vector<std::string> const& options);`
- src/device/generic_device_type.cpp:46–47, 51–53 — the parameter is fed into
  `toDeviceTypeCode(deviceType)` and `toDeviceFamily(deviceType)`, both
  of which take `std::string const&` per their nm lines:
  `zhinst::toDeviceTypeCode(std::string const&)` @ 0x2d43d0 and
  `zhinst::toDeviceFamily(std::string const&)` @ 0x2debd0.
- src/device/device_type.cpp:121 — the only caller in the reconstruction
  forwards `deviceType` from `DeviceType(string, vector<string>)`:
  `: impl_(new detail::GenericDeviceType(deviceType, options))`. Batch
  29's table accepts that outer parameter as fine
  (`DeviceType::ctor(string,vector)::options` row, and the symmetric
  `deviceType` row by sibling consistency).

Interpretation:

- The string is consumed exclusively as a device-type name (input to
  the two `toDeviceXxx` parsers). The name describes the value's role
  accurately and matches the parameter name used by the immediate
  caller in `device_type.cpp`.

Judgement:

- Not a misnomer; matches usage and is consistent with the caller's
  own parameter name.

Proposals:

- keep current  (medium)

Locations consulted:

- declared:  include/zhinst/device/generic_device_type.hpp:37
- defined:   src/device/generic_device_type.cpp:42–53
- callers:   src/device/device_type.cpp:121, 132
- nm:        toDeviceTypeCode/Family signatures above

### GenericDeviceType::ctor::options (param)  [no / medium / not-misnomer]

Evidence:

- include/zhinst/device/generic_device_type.hpp:38 — declared as
  `std::vector<std::string> const& options`.
- src/device/generic_device_type.cpp:52 — passed into
  `toDeviceOptions(options, toDeviceFamily(deviceType))`. From nm:
  `zhinst::toDeviceOptions(std::vector<std::string,…> const&, zhinst::DeviceFamily)`
  @ 0x2d0fb0. The free function's first parameter is precisely a
  vector-of-option-name strings.
- src/device/device_type.cpp:121 — caller forwards `options` verbatim from
  `DeviceType(string, vector<string>)`. Batch 29 accepted that outer
  `options` parameter as fine
  (`DeviceType::ctor(string,vector)::options`).
- Distinct from the `unsigned long options` bitmask sense flagged
  `unsure` in batch 29 (`DeviceType::ctor(family,options)::options`):
  here the type is concretely `vector<string>` of option *names*, not
  a bitmask, so the flag from batch 29 doesn't transfer.

Interpretation:

- The vector contains option-name strings; that is what
  `toDeviceOptions` parses, and the caller uses the same name.

Judgement:

- Not a misnomer.

Proposals:

- keep current  (medium)

Locations consulted:

- declared:  include/zhinst/device/generic_device_type.hpp:38
- defined:   src/device/generic_device_type.cpp:42–53
- callers:   src/device/device_type.cpp:121, 132
- nm:        `toDeviceOptions(vector<string> const&, DeviceFamily)`

## 4. Symbols inspected and judged routinely fine

- (None beyond the two parameters above; both received full §3
  blocks because of the cross-batch consistency claim.)

## 5. Coverage

- **Fully covered:** the two ctor parameters
  (`GenericDeviceType::ctor::deviceType`,
  `GenericDeviceType::ctor::options`).
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type name `zhinst::detail::GenericDeviceType` — appears verbatim
    in `nm --demangle` (typeinfo, vtable, ctor and dtor symbols at
    0xb090b8 / 0xb09090 / 0x2d3c60 / 0x2d4030). Authoritative per §3.
  - Method names `GenericDeviceType::GenericDeviceType` and
    `~GenericDeviceType` — present in nm output. Authoritative per
    §3.
  - The class has no data members, no other methods, no namespace-
    scope constants, no enums, no macros, and no non-trivial locals
    (the ctor body is empty), so there is nothing else in scope.
