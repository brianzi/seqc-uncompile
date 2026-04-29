# Batch 41 — device_subclasses

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 4 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 0;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 4.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

Header:

- `reconstructed/include/zhinst/device_subclasses.hpp` (312 lines)

Per-family implementation files:

- `reconstructed/src/device_hdawg.cpp`
- `reconstructed/src/device_shf.cpp`
- `reconstructed/src/device_uhf.cpp`
- `reconstructed/src/device_vhf.cpp`
- `reconstructed/src/device_ghf.cpp`
- `reconstructed/src/device_mf.cpp`
- `reconstructed/src/device_hf2.cpp`
- `reconstructed/src/device_pqsc.cpp`
- `reconstructed/src/device_qhub.cpp`
- `reconstructed/src/device_shfacc.cpp`
- `reconstructed/src/device_hwmock.cpp`
- `reconstructed/src/device_unknown.cpp`

Cross-referenced (read-only):

- `reconstructed/include/zhinst/device_type.hpp`        (base `DeviceTypeImpl`, `DeviceOption*`, `sfc::*Option::Bit0xNNNN` enums)
- `reconstructed/include/zhinst/device_factories.hpp`   (`opts` parameter cluster)
- `reconstructed/notes/symbol-renaming-audit/22_device_factories.md`
- `reconstructed/notes/symbol-renaming-audit/29_device_type.md`
- `reconstructed/notes/symbol-renaming-audit/30_awg_device_props.md`
- `reconstructed/notes/symbol-renaming-audit/40_generic_device_type.md` (presence)
- `reconstructed/notes/symbol-renaming-audit/54_mf_sfc.md`               (presence)

Authoritative symbol table:

- `nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so`

## 2. Overview

The 32 subclass headers are mechanical look-alikes (only the type
name and the `clone()` wording differ). To avoid 32×3 nearly-identical
table rows, they are summarised with a single representative row each.
Where the same symbol exists per-family with identical evidence shape
("the parameter `opts`"), one row covers all instances.

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `Hf2li::Hf2li::opts` (and 26 sibling ctors taking `unsigned long opts`) | no | high | matches batch-22 cluster | keep current | not-misnomer |
| `<Subclass>::clone` (override; ×32 subclasses) | yes | high | binary base = `doClone` | `doClone` | coordinated-rename |
| `(anon)::buildInlineShfOptions` (free fn, device_shf.cpp) | unsure | low | recon-invented helper name | keep current; `buildShfOptionsInline` | — |
| `buildInlineShfOptions::opts` | no | medium | mirrors ctor cluster | keep current | not-misnomer |
| `buildInlineShfOptions::family` | no | medium | matches arg type/use | keep current | not-misnomer |
| `buildInlineShfOptions::addRtr` | no | medium | bool selector, comment OK | keep current | — |
| `buildInlineShfOptions::addPlus` | no | medium | bool selector, comment OK | keep current | — |
| `buildInlineShfOptions::addLrt` | no | medium | bool selector, comment OK | keep current | — |
| `(anon)::buildVhfFf` (free fn, device_vhf.cpp) | unsure | low | recon-invented; "Ff" terse | keep current; `buildVhfFfOption` | — |
| `(anon)::buildGhfFf` (free fn, device_ghf.cpp) | unsure | low | recon-invented; "Ff" terse | keep current; `buildGhfFfOption` | — |
| `(anon)::buildShfaccFf` (free fn, device_shfacc.cpp) | unsure | low | recon-invented; "Ff" terse | keep current; `buildShfaccFfOption` | — |
| `buildVhfFf::opts` / `buildGhfFf::opts` / `buildShfaccFf::opts` | no | medium | one-arg helper, role obvious | keep current | not-misnomer |
| `kHf2liKnownOptions`, `kHf2isKnownOptions` (anon-ns array consts) | no | medium | matches initializeSfcOptions arg | keep current | — |
| `kMfliKnownOptions`, `kMfiaKnownOptions` | no | medium | same pattern | keep current | — |
| `kUhfliKnownOptions`, `kUhfawgKnownOptions`, `kUhfqaKnownOptions`, `kUhfiaKnownOptions` | no | medium | same pattern | keep current | — |
| `kHdawg4KnownOptions`, `kHdawg8KnownOptions` | no | medium | same pattern | keep current | — |
| `kShfqa2KnownOptions`, `kShfqcKnownOptions`, `kShfliKnownOptions` | no | medium | same pattern | keep current | — |
| `kGhfliKnownOptions` | no | medium | same pattern | keep current | — |
| `kVhfliKnownOptions` | no | medium | same pattern | keep current | — |

(All 32 subclass type names — `Hf2`, `Hf2li`, `Hf2is`, `Mf`, `Mfli`,
`Mfia`, `Uhf`, `Uhfli`, `Uhfawg`, `Uhfqa`, `Uhfia`, `Hdawg`, `Hdawg4`,
`Hdawg8`, `Shf`, `Shfqa2`, `Shfqa4`, `Shfsg2`, `Shfsg4`, `Shfsg8`,
`Shfqc`, `Shfli`, `Shfacc`, `Shfppc2`, `Shfppc4`, `Ghf`, `Ghfli`,
`Pqsc`, `Qhub`, `Hwmock`, `Vhf`, `Vhfli`, `UnknownDevice` — and all
constructor / destructor names are excluded per §3 and listed in §5.
The `sfc::*Option::Bit0xNNNN` enumerators referenced in the
`kXxxKnownOptions` arrays, and the `DeviceOption::TenG` /
`DeviceOption::Sixteen_W` placeholder values referenced in the SHF and
UHF arrays, are flagged in batch 29 (device_type) and not duplicated
here — see Cross-references.)

## 3. Detailed findings

### `<Subclass>::opts` parameter (×27 ctors)  [no / high / not-misnomer]

This row covers every constructor parameter named `opts` across all 27
single-argument subclass ctors in the header (Hf2li, Hf2is, Mfli, Mfia,
Uhfli, Uhfawg, Uhfqa, Uhfia, Hdawg4, Hdawg8, Shf, Shfqa2, Shfqa4,
Shfsg2, Shfsg4, Shfsg8, Shfqc, Shfli, Shfacc, Shfppc2, Shfppc4, Ghf,
Ghfli, Vhf, Vhfli — 25 declared in the header — plus the implicit
analogous parameter in `Mf`/`Hdawg`/`Hf2`/`Pqsc`/`Qhub`/`Hwmock` zero-arg
ctors which take none).

Evidence:

- include/zhinst/device_subclasses.hpp:50,57,74,81,98,105,112,119,136,
  143,153,160,167,174,181,188,195,202,212,219,226,236,243,283,290 —
  every single-arg ctor declared with `unsigned long opts`.
- src/device_hdawg.cpp:47,54 — `Hdawg4::Hdawg4(unsigned long opts)` /
  `Hdawg8::Hdawg8(unsigned long opts)`, body forwards `opts` to
  `initializeSfcOptions(...)`.
- src/device_shf.cpp:83,93,103,113,120,127,137,147 — same pattern, all
  Shf-family subclasses take `unsigned long opts`.
- src/device_hf2.cpp:59,72; src/device_uhf.cpp:81,88,95,102;
  src/device_vhf.cpp:42,49; src/device_ghf.cpp:43,50;
  src/device_mf.cpp:56,66; src/device_shfacc.cpp:25,32,39 — same pattern.
- src/device_factories.cpp (cited in batch 22): every
  `<X>Factory::doMakeDeviceType(unsigned long opts)` forwards `opts`
  unchanged into the matching subclass ctor.

Interpretation:

- The parameter is the per-family device-options bitmask, threaded
  unmodified from `DeviceFamilyFactory::doMakeDeviceType` into each
  subclass ctor and from there into either `initializeSfcOptions(...)`
  or the inline-bit helpers (`buildInlineShfOptions`, `buildVhfFf`,
  `buildGhfFf`, `buildShfaccFf`).
- The whole device_factories ↔ device_subclasses ↔ inline-helper cluster
  unanimously names this value `opts`.

Judgement:

- Not a misnomer. Same conclusion as batch 22 for the factory side; the
  subclass ctor side is the producer-half of the same `opts` cluster.

Proposals:

- keep current  (high)

Cross-reference:

- Batch 22 (device_factories) already records "keep current" for the
  factory-side `opts` parameter. This batch confirms the subclass-ctor
  half of the same cluster.
- Batch 29 (device_type) `DeviceType::DeviceType(2-arg)::options` is
  the lone outlier (`options`, not `opts`); arbitration deferred to
  synthesis (see batch 22 §3).

Locations consulted:

- declared: include/zhinst/device_subclasses.hpp (lines listed above)
- defined : every per-family .cpp file in this batch
- callers : src/device_factories.cpp (per batch 22)


### `<Subclass>::clone` override (×32)  [yes / high / coordinated-rename]

Every concrete subclass declares `DeviceTypeImpl* clone() const override;`
in the header and defines it in its .cpp. Names:

- Hf2, Hf2li, Hf2is, Mf, Mfli, Mfia, Uhf, Uhfli, Uhfawg, Uhfqa, Uhfia,
  Hdawg, Hdawg4, Hdawg8, Shf, Shfqa2, Shfqa4, Shfsg2, Shfsg4, Shfsg8,
  Shfqc, Shfli, Shfacc, Shfppc2, Shfppc4, Ghf, Ghfli, Pqsc, Qhub,
  Hwmock, Vhf, Vhfli, UnknownDevice — 33 sites total (the special
  `UnknownDevice` included).

Evidence:

- include/zhinst/device_subclasses.hpp:45,52,59,69,76,83,93,100,107,
  114,121,131,138,145,155,162,169,176,183,190,197,204,214,221,228,238,
  245,255,265,275,285,292,308 — every override declared as `clone()`.
- src/device_hdawg.cpp:45,52,59 — `DeviceTypeImpl* Hdawg::clone() const
  { return new Hdawg(*this); }` (and Hdawg4, Hdawg8 same).
- src/device_shf.cpp:88,98,108,118,125,132,142,152 — same body shape.
- src/device_hf2.cpp:53,66,79; src/device_uhf.cpp:79,86,93,100,107;
  src/device_mf.cpp:51,61,71; src/device_vhf.cpp:46,53;
  src/device_ghf.cpp:47,55; src/device_shfacc.cpp:29,36,43;
  src/device_pqsc.cpp:14; src/device_qhub.cpp:14;
  src/device_hwmock.cpp:14; src/device_unknown.cpp:45 — same.
- nm --demangle: `0x2d3280 t zhinst::detail::DeviceTypeImpl::doClone()
  const` is the only `clone`/`doClone` zhinst-detail symbol exported
  by the binary. The base virtual is named `doClone` in the binary.
  (Subclass override symbols are not separately exported — they are
  reached only via the per-class vtable[0] slot — but per Itanium the
  vtable slot inherits the base's declared name.)
- Batch 29 §3 ("DeviceTypeImpl::clone (method)") establishes
  authoritatively that the base virtual is `doClone`, not `clone`,
  and proposes the rename.

Interpretation:

- All 33 subclass overrides override the same virtual slot. If the base
  is `doClone`, every override must rename in lockstep — they cannot
  be renamed individually without breaking the override relationship.
- The recon's body convention `return new Hdawg(*this);` etc. is
  unaffected by the rename; only the function identifier changes.

Judgement:

- Yes, all 33 subclass overrides are misnomers in the same way and for
  the same reason as the base: the binary name is `doClone`.

Proposals:

- `doClone`     (high) — coordinated with batch 29 base rename.
- keep current  (low)  — only viable if batch 29 is also kept; the
  base and overrides must agree.

Cross-reference:

- Counterpart base method `DeviceTypeImpl::clone` in batch 29
  (device_type) §3, judgement "yes / high / —", proposed `doClone`.
- Status `coordinated-rename` because the 33 subclass overrides plus
  the base must rename together.

Locations consulted:

- declared: include/zhinst/device_subclasses.hpp (33 sites listed above)
- defined : every per-family .cpp file in this batch
- nm      : 0x2d3280 zhinst::detail::DeviceTypeImpl::doClone() const


### `(anon)::buildInlineShfOptions` (and family `buildVhfFf`, `buildGhfFf`, `buildShfaccFf`)  [unsure / low / —]

Evidence:

- src/device_shf.cpp:65-76 — anonymous-namespace free function
  `DeviceOptionSet buildInlineShfOptions(unsigned long opts,
  DeviceFamily family, bool addRtr, bool addPlus, bool addLrt)`.
  Body: insert `FF` if `opts & 0x20`; conditionally insert `RTR` /
  `PLUS` / `LRT` for the matching bits.
- src/device_shf.cpp:3-22 (header comment) describes the disassembly
  pattern as "an inline if/insert chain on a few selector bits" — the
  helper is a recon-introduced consolidation of identical code at
  multiple subclass ctors.
- src/device_vhf.cpp:33-37 `buildVhfFf(unsigned long opts)` — same
  pattern, single bit; called by `Vhf::Vhf` only.
- src/device_ghf.cpp:34-38 `buildGhfFf(unsigned long opts)` — same.
- src/device_shfacc.cpp:17-21 `buildShfaccFf(unsigned long opts)` —
  same; called by `Shfacc`, `Shfppc2`, `Shfppc4`.
- nm --demangle: none of `buildInlineShfOptions`, `buildVhfFf`,
  `buildGhfFf`, `buildShfaccFf` appear in the binary (anon-namespace
  helpers, not exported).

Interpretation:

- All four are recon inventions: the binary has no equivalent symbol;
  the original code very likely inlined the if/insert chain inside
  each ctor. Their names are entirely a recon choice.
- `buildInlineShfOptions` is descriptive: it builds a SHF-family
  `DeviceOptionSet` from inline option bits. The "Inline" qualifier
  distinguishes it from the template `initializeSfcOptions` path.
- `buildVhfFf` / `buildGhfFf` / `buildShfaccFf` are slightly cryptic:
  "Ff" reads at first glance as "FlipFlop" / "FullFrame"; here it is
  the `DeviceOption::FF` enumerator (single bit). The function comments
  do clarify ("inline FF helper").

Judgement:

- Unsure. The names are not factually wrong — they are technically
  accurate descriptions of recon-invented helpers. But "Ff" reads
  poorly as a bare suffix; a small clarification (`buildVhfFfOption`
  etc.) would make intent obvious without changing the structure.

Proposals:

- keep current             (medium) — names are accurate and self-
  documented at one-line scope.
- `buildShfOptionsInline`  (low)    — alphabetical/naming hygiene.
- `buildVhfFfOption` /
  `buildGhfFfOption` /
  `buildShfaccFfOption`    (low)    — disambiguate "Ff" suffix.

Locations consulted:

- declared+defined: src/device_shf.cpp:65-76, src/device_vhf.cpp:33-37,
  src/device_ghf.cpp:34-38, src/device_shfacc.cpp:17-21
- callers       : same files (subclass ctor initializer lists)
- nm            : not present (anon-ns)


## 4. Symbols inspected and judged routinely fine

Per-family `.cpp` `kXxxKnownOptions` arrays (all 14 of them):
`kHf2liKnownOptions`, `kHf2isKnownOptions`, `kMfliKnownOptions`,
`kMfiaKnownOptions`, `kUhfliKnownOptions`, `kUhfawgKnownOptions`,
`kUhfqaKnownOptions`, `kUhfiaKnownOptions`, `kHdawg4KnownOptions`,
`kHdawg8KnownOptions`, `kShfqa2KnownOptions`, `kShfqcKnownOptions`,
`kShfliKnownOptions`, `kGhfliKnownOptions`, `kVhfliKnownOptions`.

- Each is the table of `OptionCodePair<sfc::XxxOption>` passed to
  `initializeSfcOptions(table, family, opts)`. The `KnownOptions`
  suffix is the natural reading of how it's used.
- Anonymous-namespace constants — not exported (verified via `nm`),
  so no external "true name" exists.
- `k`-prefix is the convention already used elsewhere in `reconstructed/`.

`buildInlineShfOptions` selector parameters `addRtr`, `addPlus`, `addLrt`:

- Bool flags whose `true` value enables the corresponding bit-test in
  the body. Comment at declaration documents the bit value (`0x2000 ->
  RTR (27)`, etc.). Names match what they do.

`buildInlineShfOptions::opts`, `buildVhfFf::opts`, `buildGhfFf::opts`,
`buildShfaccFf::opts`:

- Same `opts` cluster as the subclass ctors, see §3.

`buildInlineShfOptions::family`:

- Forwarded directly into `DeviceOptionSet(family)`; the parameter name
  matches the type name.

`UnknownDevice` synthetic-sentinel ctor body:

- `UnknownDevice::UnknownDevice() : DeviceTypeImpl(static_cast<...>(33),
  static_cast<...>(0x800)) {}` — the literals `33` and `0x800` are
  documented in the file header as binary-observed sentinels (movabs
  rax,0x80000000021). No identifiers in scope that aren't already
  type names (excluded).


## 5. Coverage

**Fully covered:**

- Header `device_subclasses.hpp` in full.
- All 12 implementation files in this batch in full.
- Cross-checked against `nm --demangle` for every subclass type name,
  ctor, dtor, and `clone`/`doClone` symbol.

**Excluded per RULES §3 (binary-table authoritative):**

- All 32 subclass type names (`Hf2`, `Hf2li`, `Hf2is`, `Mf`, `Mfli`,
  `Mfia`, `Uhf`, `Uhfli`, `Uhfawg`, `Uhfqa`, `Uhfia`, `Hdawg`,
  `Hdawg4`, `Hdawg8`, `Shf`, `Shfqa2`, `Shfqa4`, `Shfsg2`, `Shfsg4`,
  `Shfsg8`, `Shfqc`, `Shfli`, `Shfacc`, `Shfppc2`, `Shfppc4`, `Ghf`,
  `Ghfli`, `Pqsc`, `Qhub`, `Hwmock`, `Vhf`, `Vhfli`, `UnknownDevice`)
  — `typeinfo for zhinst::detail::<X>` present in `nm` output for
  every one (sample lines confirmed: `0xb09418 d typeinfo for
  zhinst::detail::Hf2`, `0xb09c80 d typeinfo for zhinst::detail::Pqsc`,
  etc., 32 total).
- All ctor and dtor symbols for those 32 classes (e.g.
  `0x2e0830 t zhinst::detail::Hf2::Hf2()`,
  `0x2e0e30 t zhinst::detail::Mfli::Mfli(unsigned long)`,
  `0x2e2930 t zhinst::detail::Shfqc::Shfqc(unsigned long)` …).

**Deferred / cross-batch (not handled here, see referenced batches):**

- `sfc::Hf2Option::Bit0xNNNN`, `sfc::MfOption::Bit0xNNNN`,
  `sfc::UhfOption::Bit0xNNNN`, `sfc::HdawgOption::Bit0xNNNN`,
  `sfc::ShfOption::Bit0xNNNN`, `sfc::VhfOption::Bit0xNNNN`
  enumerators referenced in every `kXxxKnownOptions` array — flagged
  with status `coordinated-rename` in batch 29.
- `DeviceOption::TenG` (Uhfia table), `DeviceOption::Sixteen_W`
  (Shfqa2, Shfqc tables), and `DeviceOption::FF`/`RTR`/`PLUS`/`LRT`
  semantics — flagged in batch 29 and (`FF`/`RTR`/`PLUS`/`LRT` only)
  the dead-namespace `DeviceOpts::*` set in batch 22.
- The `initializeSfcOptions` template (used from the template-path
  ctors) — declared in `device_type.hpp`, covered by batches 29/40/54.
- Per-family `sfc::*Option` enum definitions themselves — owned by
  batches 29 and 54 (mf_sfc).

**Out of scope (not actionable in any batch):**

- Comment text like "vtable b09368", "@ 0x2e0830", ".rodata 0x96298c"
  — addresses, not identifiers.
- The literals `33` and `0x800` in `UnknownDevice` — values, not
  identifiers.
