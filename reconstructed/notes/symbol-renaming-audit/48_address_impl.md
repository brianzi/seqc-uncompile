# Batch 48 — address_impl

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 1 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 0;
> B3 (already resolved during Phase D/R): 1;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/address_impl.hpp` (header-only template; no .cpp).

Cross-referenced (read-only) for usage survey:
- `reconstructed/src/value.cpp`
- `reconstructed/src/asm_commands.cpp`
- `reconstructed/src/prefetch.cpp`, `prefetch_emit.cpp`
- `reconstructed/src/wavetable_front.cpp`, `wavetable_ir.cpp`
- `reconstructed/src/custom_functions_io.cpp`, `custom_functions_play.cpp`

`nm --demangle` confirms (excluded from rename per §3):
- Type `zhinst::detail::AddressImpl<unsigned int>` — appears in many mangled
  function signatures (e.g. `Cache::allocate(..., AddressImpl<unsigned int>, ...)`,
  `Prefetch::clampToCache(AddressImpl<unsigned int>)`, `AsmCommands::ld/st/...`).
- The free function
  `operator<<(basic_ostream&, AddressImpl<unsigned int>)` at
  `0x1c7ce0` (mangled `_ZN6zhinstlsITkNSt3__117unsigned_integralEjEERNS1_13basic_ostreamI...`) — function name is the operator, authoritative.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AddressImpl::value` | unsure | low | wrapper holds non-address values too | keep current, `raw_`, `bits_` | — |
| `AddressImpl::AddressImpl(T)::v` | no | medium | conventional ctor-arg name | keep current | — |
| `AddressImpl::operator=(T)::v` | no | medium | conventional setter arg | keep current | — |
| `zhinst::operator<<(ostream&, AddressImpl<uint32_t>)::os` | no | high | standard ostream param name | keep current | not-misnomer |
| `zhinst::operator<<(ostream&, AddressImpl<uint32_t>)::addr` | no | medium | matches type role | keep current | — |

No symbols flagged at medium/high as misnomers.

## 3. Detailed findings

### AddressImpl::value  [unsure / low / —]

Evidence:
- `address_impl.hpp:20`  `T value;` — sole data member.
- `value.cpp:26`  `os << boost::format("%u") % addr.value;`
- `value.cpp:43`  `data_.address.value = v;` (Immediate setter)
- `value.cpp:142,158,176,191`  `data_.address.value` read in
  `Immediate::operator int()`, `operator unsigned int()`, equality, `to_string`.
- `prefetch_emit.cpp:59`  `uint32_t addrVal = addr.value;`
- `wavetable_front.cpp:57-58`  `address_ = addr.value; address2_ = addr.value;`
- `wavetable_ir.cpp:58-59,99-100`  `addressBase_(address.value), firstWaveformOffset_(address.value),`
- `asm_commands.cpp:520,533,659`  `static_cast<int32_t>(addr.value)`,
  `addr.value += 0x64;`
- `prefetch.cpp:59`  `static_cast<detail::AddressImpl<uint32_t>>(devConst.waveformMemorySize)` — the wrapped value is a *byte memory size*, not an address.
- `custom_functions_play.cpp:121,1626,...`  `detail::AddressImpl<unsigned int>(kSuserWaitLegacy)`, `kSuserNodeTag`, `kSuserWaitCycles`, ... — the wrapped value is a *suser opcode/node-id*, not an address.
- `custom_functions_io.cpp:2109,2114`  `detail::AddressImpl<unsigned int>(arg0.value_.toInt())` — wraps a generic immediate.

Interpretation:
- The member is the underlying `T`-typed payload of a wrapper used for
  several distinct semantic categories (memory addresses, byte sizes,
  suser opcodes, raw immediates) — i.e. the *type* `AddressImpl` is in
  practice a generic uint-wrapper distinguishable from `int` for
  overload/variant disambiguation, not strictly an address.
- The member name `value` is generic ("the wrapped scalar") and matches
  every use site mechanically: every read/write treats it as the
  underlying scalar regardless of its semantic role.
- The mismatch is at the level of the *type name*, not the member name;
  but the type name is excluded from rename per §3.

Judgement:
- The member name is not a misnomer in itself — it accurately describes
  "the wrapped scalar value" — but the symbol carries the unease that
  the enclosing type is broader than its name suggests; flagging the
  member as `unsure / low` records that observation without
  overclaiming.

Proposals:
- keep current  (medium) — `value` is faithful to "wrapped scalar"; any
  rename inherits the type's overgeneral name without improving
  clarity.
- `raw_`  (low) — emphasizes that this is the underlying representation.
- `bits_`  (low) — neutral, drops the address connotation.

Locations consulted:
- declared: include/zhinst/address_impl.hpp:20
- used:     value.cpp:26,43,142,158,176,191; prefetch_emit.cpp:59;
  wavetable_front.cpp:57-58; wavetable_ir.cpp:58-59,99-100;
  asm_commands.cpp:520,533,659.

### AddressImpl::AddressImpl(T)::v  [no / medium / —]

Evidence:
- `address_impl.hpp:23`  `AddressImpl(T v) : value(v) {}`

Interpretation:
- Single-argument constructor of a one-field wrapper. The parameter is
  immediately stored into `value`. `v` is the conventional one-letter
  name for the constructor argument that initialises a single member.

Judgement:
- Not a misnomer.

### AddressImpl::operator=(T)::v  [no / medium / —]

Evidence:
- `address_impl.hpp:25`  `AddressImpl& operator=(T v) { value = v; return *this; }`

Interpretation:
- Symmetrical with the constructor; same conventional argument name.

Judgement:
- Not a misnomer.

### zhinst::operator<<(ostream&, AddressImpl<uint32_t>)::os  [no / high / not-misnomer]

Evidence:
- `address_impl.hpp:33`  `std::ostream& operator<<(std::ostream& os, detail::AddressImpl<uint32_t> addr);`
- Definition `value.cpp:25-27`:
  ```
  std::ostream& operator<<(std::ostream& os, detail::AddressImpl<uint32_t> addr) {
      os << boost::format("%u") % addr.value;
      return os;
  }
  ```

Interpretation:
- `os` is the universally idiomatic C++ name for the ostream parameter
  of a stream insertion operator. Used as such here without divergence.

Judgement:
- Not a misnomer — recorded for completeness as positive evidence
  (§4d, tier 4).

### zhinst::operator<<(ostream&, AddressImpl<uint32_t>)::addr  [no / medium / —]

Evidence:
- `address_impl.hpp:33`  parameter `detail::AddressImpl<uint32_t> addr`.
- `value.cpp:26`  body uses `addr.value` only.

Interpretation:
- Parameter name matches the type's nominal role ("address-ish wrapper").
  The name is consistent with the convention used at almost every other
  call site that passes an `AddressImpl<uint32_t>` value (see
  `asm_commands.cpp:512,525,554,562,651`, `custom_functions_io.cpp:38`,
  `custom_functions_play.cpp:42,46`, `prefetch.cpp` clampToCache reviewed
  in batch 09 part 2).
- Although the wrapped value at some call sites is not strictly an
  address (e.g. opcodes, byte sizes), at the operator<< printing
  site there is no semantic context narrower than "the wrapped
  uint" — `addr` is as informative as the type itself.

Judgement:
- Not a misnomer at this site; consistent with the project-wide
  convention for `AddressImpl<uint32_t>` parameters.

## 4. Symbols inspected and judged routinely fine

- `AddressImpl::AddressImpl()` (default ctor, 0-arg) — no parameters; trivially zero-initializes `value`.
- Template parameter `T` — out of scope per §2.

## 5. Coverage

- **Fully covered:** every named symbol declared in
  `address_impl.hpp` (the type itself, its single data member, both
  constructors' and `operator=`'s parameters, and the namespace-scope
  `operator<<` declaration's parameters).
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type name `AddressImpl` — appears in mangled binary symbols for
    `Cache::*`, `Prefetch::*`, `AsmCommands::*`, `WavetableIR::*`,
    `Waveform::*`, `Immediate::*`, `WavetableFront::*`,
    `ElfWriter::addWaveform`, etc. (verified via
    `nm --demangle _seqc_compiler.so`). Authoritative; excluded.
  - Free function `operator<<` (the operator name itself) — appears
    in the binary at `0x1c7ce0` with mangled symbol confirming the
    template signature. Excluded.
  - Template parameter `T` — §2 exclusion.
  - Cross-batch observation: the *type* `AddressImpl` is used as a
    generic uint-wrapper (addresses, byte sizes, suser opcodes,
    immediates). This is structural, not a name-audit finding for
    this batch's in-scope symbols; recorded here for synthesis only.
