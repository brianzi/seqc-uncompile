# Batch 54 — mf_sfc

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 1 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 1;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/src/mf_sfc.cpp` (96 lines, single free function
  `zhinst::detail::generateMfSfc` plus an anonymous-namespace helper
  `bitIf`).

The associated header declaration lives in
`reconstructed/include/zhinst/device_type.hpp` (line 460); only the
parameter names there are in scope, and they are the same names as in
the .cpp definition. The enclosing types (`DeviceOptionSet`,
`DeviceOption`, `DeviceTypeCode`, `sfc::FeaturesCode`, `sfc::MfOption`)
and the function name `generateMfSfc` itself are all present in the
binary symbol table and therefore out of scope (RULES §3) — they were
audited in batch 29 (`device_type`).

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `bitIf` (free fn, anon ns) | unsure | low | clear but ad-hoc name | keep current, `bitFromBool`, `shiftBit` | — |
| `bitIf::b` (param) | yes | medium | one-letter, undescriptive | `set`, `present` | — |
| `bitIf::shift` (param) | no | high | matches usage `1 << shift` | keep current | not-misnomer |
| `generateMfSfc::deviceTypeName` (param) | no | medium | already audited in batch 29 | keep current | — |
| `generateMfSfc::options` (param) | no | medium | already audited in batch 29 | keep current | — |
| `generateMfSfc::code` (local) | no | medium | result of `toDeviceTypeCode` | keep current | not-misnomer |
| `generateMfSfc::commonBits` (local) | no | medium | OR-composed bit mask | keep current | not-misnomer |
| `generateMfSfc::oss` (local) | no | low | conventional `ostringstream` name | keep current | — |

## 3. Detailed findings

### bitIf (free function, anonymous namespace)  [unsure / low / —]

Evidence:
- mf_sfc.cpp:26-29
  ```cpp
  // Helper: shift a bool to a specific bit position.
  constexpr uint64_t bitIf(bool b, unsigned shift) {
      return b ? (uint64_t{1} << shift) : uint64_t{0};
  }
  ```
- Used 9 times at mf_sfc.cpp:70-78, always as
  `bitIf(options.contains(DeviceOption::X), N)`.
- Not present in `nm --demangle _seqc_compiler.so` — it is a recon
  helper (likely inlined out of the binary; the binary's actual code
  at 0x2de910..0x2dead0 is straight-line `test/setne/shl/or`).

Interpretation:
- The name `bitIf` is grammatically a verb-ish portmanteau ("bit if
  true"). It is not standard terminology, but the meaning is guessable
  from one read of the body. The helper is purely a recon convenience
  with no binary counterpart, so its name is unconstrained.

Judgement:
- Borderline — the name is non-idiomatic but conveys intent; reasonable
  alternatives exist but no concrete observation argues the current
  name is wrong. Mark `unsure / low`.

Proposals:
- keep current  (medium)
- `bitFromBool`  (low)
- `shiftBit`  (low)

Locations consulted:
- declared/defined: src/mf_sfc.cpp:27
- used: src/mf_sfc.cpp:70-78

### bitIf::b (parameter)  [yes / medium / —]

Evidence:
- mf_sfc.cpp:27 — `constexpr uint64_t bitIf(bool b, unsigned shift)`.
- Used once in the body, mf_sfc.cpp:28: `return b ? ...`.
- All 9 call sites pass `options.contains(DeviceOption::X)` — i.e. the
  argument's domain meaning is "is this option present in the set".

Interpretation:
- The parameter is a single-letter name (`b`) for what is, at every
  call site, a boolean predicate "option is present". Single-letter
  parameter names are exactly the kind of generic placeholder §4a
  flags as misleading.

Judgement:
- Yes, misnomer — a one-letter `b` is uninformative even for a tiny
  helper.

Proposals:
- `set`  (medium)        // matches "bit set if true"
- `present`  (low)       // matches the call-site semantics

Locations consulted:
- declared: src/mf_sfc.cpp:27
- used:     src/mf_sfc.cpp:28

### bitIf::shift (parameter)  [no / high / not-misnomer]

Evidence:
- mf_sfc.cpp:27 — `unsigned shift`.
- mf_sfc.cpp:28 — `(uint64_t{1} << shift)`.
- Call sites pass the literal bit-position constants `0,1,2,5,10,11,
  14,15,17` (mf_sfc.cpp:70-78).

Interpretation:
- The parameter is consumed exactly as the right operand of `<<`, and
  the literals supplied are the bit positions documented in the file
  header (lines 39-50). The name `shift` precisely describes that
  role.

Judgement:
- Not a misnomer.

Locations consulted:
- declared: src/mf_sfc.cpp:27
- used:     src/mf_sfc.cpp:28, 70-78

### generateMfSfc::code (local)  [no / medium / not-misnomer]

Evidence:
- mf_sfc.cpp:66 — `auto const code = toDeviceTypeCode(deviceTypeName);`
- mf_sfc.cpp:80 — `if (code == DeviceTypeCode::MFIA)`
- mf_sfc.cpp:83 — `if (code == DeviceTypeCode::MFLI)`
- mf_sfc.cpp:90 — `oss << ... << code << ...` (operator<< for
  `DeviceTypeCode`).

Interpretation:
- The variable's type is `DeviceTypeCode` (per the symbol-table-
  excluded factory `toDeviceTypeCode`), and every use compares or
  formats it as such. `code` is a natural shortening of "device type
  code". No competing interpretation is plausible in this 30-line
  function.

Judgement:
- Not a misnomer.

Locations consulted:
- declared: src/mf_sfc.cpp:66
- used:     src/mf_sfc.cpp:80, 83, 90

### generateMfSfc::commonBits (local)  [no / medium / not-misnomer]

Evidence:
- mf_sfc.cpp:69-78 — initialized as the OR of nine `bitIf(...)` terms.
- mf_sfc.cpp:81 — `return sfc::FeaturesCode{commonBits | uint64_t{0x80}};`
- mf_sfc.cpp:84 — `return sfc::FeaturesCode{commonBits | uint64_t{0x40}};`
- File-header comment lines 38-50 documents this as the
  "common option-bit composition shared by MFLI and MFIA".

Interpretation:
- The value is exactly the bits common to both device flavors before
  OR-ing in the type-discriminator bit (0x40 for MFLI, 0x80 for MFIA).
  The name describes precisely that.

Judgement:
- Not a misnomer.

Locations consulted:
- declared: src/mf_sfc.cpp:69
- used:     src/mf_sfc.cpp:81, 84

### generateMfSfc::oss (local)  [no / low / —]

Evidence:
- mf_sfc.cpp:88-92 — `std::ostringstream oss; oss << "..." << code << "."; ... Exception(oss.str())`.

Interpretation:
- `oss` is the customary abbreviation for `ostringstream` throughout
  C++ codebases. Used purely to assemble the throw message.

Judgement:
- Not a misnomer (routine).

## 4. Symbols inspected and judged routinely fine

- `generateMfSfc::deviceTypeName` (param) — already audited in batch
  29 (no/medium); name matches its sole use as the input to
  `toDeviceTypeCode`.
- `generateMfSfc::options` (param) — already audited in batch 29
  (no/medium); type `DeviceOptionSet const&`, queried with
  `.contains(DeviceOption::*)`.

## 5. Coverage

- **Fully covered:** every in-scope symbol declared inside
  `src/mf_sfc.cpp` — the anonymous-namespace helper `bitIf` and its
  two parameters; the four locals (`code`, `commonBits`, `oss`) and
  two parameters of `generateMfSfc`.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - `zhinst::detail::generateMfSfc` (free-function name) — present in
    `nm --demangle _seqc_compiler.so` at `0x2de910` (RULES §3).
  - `sfc::FeaturesCode`, `sfc::MfOption`, `DeviceOptionSet`,
    `DeviceOption`, `DeviceTypeCode` (type names and members) —
    present in mangled symbols and audited under batch 29
    (`device_type`). In particular the 39 `sfc::*Option::Bit0xNNNN`
    placeholder cluster, including `sfc::MfOption::Bit0x00001` ..
    `Bit0x20000` consumed indirectly here, is owned by batch 29; this
    batch does not duplicate findings on those enum members. They
    remain a `coordinated-rename` candidate at synthesis time, and
    when renamed the consumer table in `src/device_mf.cpp`
    (`kMfliKnownOptions`, `kMfiaKnownOptions`) must be updated in
    lock-step with this file's documentation comments.
  - `BOOST_THROW_EXCEPTION` (third-party macro), `bitIf` is not in
    the binary so type-name-exclusion does not apply.
