# Batch 01 — types.hpp

File: `reconstructed/include/zhinst/core/types.hpp` (156 lines)

## Preamble — what is in this file

A full read of the header shows it contains exclusively:

1. **Forward declarations** of classes (lines 13–27). Type names — out
   of scope per RULES "Out of scope: Function, method, or type names".
2. **`enum AwgDeviceType`** (lines 34–44) — an enum type with named
   bitmask enumerators (`UHFLI`, `HDAWG`, `UHFQA`, `SHFQA`, `SHFSG`,
   `SHFQC_SG`, `SHFLI`, `GHFLI`, `VHFLI`). These are enumerator names,
   not parameters, fields, or locals. Additionally the comments and
   `OVERVIEW.md`-style provenance note state the values were
   "Confirmed from getAwgDeviceTypeString (0x270080) and
   getAwgDeviceTypeFromString (0x270180)" and the codenames match the
   original binary's RTTI-style strings — these are faithful
   reconstructions of names from the binary. Out of scope.
3. **Named `AwgDeviceType` bitmask combinations** `kDevAll`,
   `kDevAllButUHF`, `kDevHirzel`, `kDevSHFPlus`, `kDevLIFamily`,
   `kDevCervino`, `kDevUHF`, `kDevPreSHFLI`, `kDevQA`, `kDevHirzelAll`,
   `kDevHirzelPlusUHFQA`, `kDevNone` (lines 49–60) — namespace-scope
   `inline constexpr` constants. Not parameters, not struct/class data
   members, not locals. Out of scope under the RULES definition of
   "Data members" / "Parameters" / "Locals".
4. **`enum class EDirection`** (lines 68–72) with enumerators `eIN`,
   `eOUT`, `eINOUT`. Enumerator names; additionally the header marks
   the type as a faithful binary reconstruction
   ("Binary name: zhinst::EDirection (mangled: NS_10EDirectionE)" and
   "str() @0x1c1730 maps: 0→\"in\", 1→\"out\", 2→\"inout\""). Out of
   scope.
5. **Sentinel constants** `kRateInherit`, `kNoWaveIndex`, `kNoNodeId`,
   `kNoPlayIndex` (lines 76–79) — namespace-scope constants. Out of
   scope (not params/fields/locals).
6. **Channel-tag constants** `kChannelTag_I`, `kChannelTag_Q`
   (lines 82–83) — namespace-scope constants. Out of scope.
7. **SUSER register-address constants** (lines 92–147), prefixed
   `kSuser*` — namespace-scope `constexpr uint32_t`. Out of scope.
8. **LD/ST direct-address constants** `kAddrTrigger`,
   `kAddrInternalTrig` (lines 153–154) — namespace-scope `constexpr
   uint32_t`. Out of scope.

There are **no struct or class definitions** in this file, **no free
function declarations**, and **no inline function bodies that could
contain local variables**. Consequently, the in-scope symbol set
defined by RULES (parameters of free functions / methods, data members,
obviously misleading locals) is **empty** for this file.

## Symbols inspected

All symbols in the file were enumerated above and classified as out of
scope. None were eligible for an in-scope judgement.

## Suspect blocks

(none)

## Coverage

- **Fully covered:** `reconstructed/include/zhinst/core/types.hpp` — every
  declaration in the file was inspected and classified. No in-scope
  symbols exist in this header, so zero suspect blocks are produced.
- **Deferred / not covered:** none.

Status: **complete** for this batch. No follow-up batch needed for this
assignment. (Naming concerns about the constants or enumerators
themselves, e.g. whether `kDevUHF` is a useful alias for `kDevCervino`
or whether `kSuserNodeDirectB` is descriptive, would belong to a
separate "constant naming" pass that is outside the audit's stated
scope of params/fields/locals.)
