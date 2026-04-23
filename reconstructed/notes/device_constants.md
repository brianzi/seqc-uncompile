# DeviceConstants — getDeviceConstants(AwgDeviceType) decode

Phase 14b-iii.5 notes for `zhinst::getDeviceConstants(AwgDeviceType)`.

- Binary: `_seqc_compiler.so`
- Address: `0x2cc0c0 .. 0x2cc47f` (size `0x3c0`)
- Source path (debug): `/builds/labone/labone/ziAWG/ziAWGDevice/src/constants.cpp`
- Same TU as `getAwgDeviceProps<>` specializations (Phase 14b-iii).
- Returns a 0x90-byte `DeviceConstants` zero-initialized then populated
  per-type. Throws `ZIAWGCompilerException` for unsupported types.

## Control flow

1. Zero-init the 0x90 output struct (`memset`).
2. Store the integer device-type code into `dc.deviceType` (offset 0).
3. Compute `idx = deviceType - 1`.
4. If `idx < 64`: jump through table at `.rodata 0x961aac`
   (64 × int32_t relative offsets, indexed by `idx`).
5. If `deviceType == 128` (GHFLI): GHFLI block.
6. Else if `deviceType == 256` (VHFLI): VHFLI block.
7. Else: throw branch.

## Jump table @ .rodata 0x961aac (64 entries)

Only 7 indices land on real per-type populate blocks; the rest fall
through to the throw branch.

| idx | deviceType | label    |
|-----|------------|----------|
|   0 |     1      | UHFLI    |
|   1 |     2      | HDAWG    |
|   3 |     4      | UHFQA    |
|   7 |     8      | SHFQA    |
|  15 |    16      | SHFSG    |
|  31 |    32      | SHFQC_SG |
|  63 |    64      | SHFLI    |

Note that the table indexes by `(type - 1)`, so the 7 valid bit-flag
positions correspond to the 7 valid powers of 2 in
`enum AwgDeviceType { UHFLI=1, HDAWG=2, UHFQA=4, SHFQA=8, SHFSG=16,
SHFQC_SG=32, SHFLI=64, GHFLI=128, VHFLI=256 }`.

## XMM constant table @ .rodata 0x8fc760 .. 0x8fc870

11 × 16-byte aligned constants. Each per-device block loads 2-4 of
these via `movaps xmm0, [rip+disp]` and stores them at fixed offsets
in the output struct, batching adjacent uint32 fields.

All values are cross-checked against the populated field assignments
in `reconstructed/src/device_constants.cpp` and match exactly.

## Sampling-rate doubles (IEEE-754 LE in .rodata)

| Bits                | Value (Hz)  | Used by                                |
|---------------------|-------------|----------------------------------------|
| 0x41dad27480000000  | 1.8e9       | UHFLI, UHFQA                           |
| 0x41ddcd6500000000  | 2.0e9       | SHFQA, SHFSG, SHFQC_SG, SHFLI, VHFLI   |
| 0x41e1e1a300000000  | 2.4e9       | HDAWG                                  |
| 0x42065a0bc0000000  | 12.0e9      | GHFLI                                  |

## Throw branch (0x2cc3f7 .. 0x2cc44d)

Equivalent to:

```cpp
BOOST_THROW_EXCEPTION(ZIAWGCompilerException(
    "Instantiated compiler for unsupported device type"));
```

Disassembly steps:

1. Construct `std::string` from C-string at `.rodata 0x90b170`
   = `"Instantiated compiler for unsupported device type"`.
2. Construct `ZIAWGCompilerException(string)` in stack slot
   `[rbp-0x98]` (ctor `0x2e7360`).
3. Build `boost::source_location` literal:
   - `file     = .rodata 0x90b1a2`
     = `/builds/labone/labone/ziAWG/ziAWGDevice/src/constants.cpp`
   - `function = .rodata 0x90b1dc`
     = `"DeviceConstants zhinst::getDeviceConstants(AwgDeviceType)"`
   - `line     = 0x138 = 312`
   - `column   = 0x41  = 65`
4. Tail-call `boost::throw_exception<ZIAWGCompilerException>(
   ex, src_loc)` @ `0x270ab0`.

Pattern is identical to the `ValueException` throw sites in
`src/value.cpp` and the `Exception` throw sites in `src/device_type.cpp`
(reconstructed in 14b-ii-b2-followup).

## Field-population coverage

All 9 valid device types (UHFLI, HDAWG, UHFQA, SHFQA, SHFSG, SHFQC_SG,
SHFLI, GHFLI, VHFLI) had their `case` blocks populated during Phase 7e.
This sub-phase only corrected the `default:` branch, which previously
used a placeholder `std::runtime_error`.

## Cross-references

- `getDeviceConstants` declared in `include/zhinst/device_constants.hpp`.
- `DeviceConstants` struct: `static_assert(sizeof == 0x90)` (libstdc++).
- `ZIAWGCompilerException` declared in `include/zhinst/exception.hpp`.
- Compare with sibling `getDeviceConstants(AwgDeviceType)` overload
  variant for `getAwgDeviceProps<>` family in `awg_device_props.cpp`
  (Phase 14b-iii) — same TU, same throw idiom.
