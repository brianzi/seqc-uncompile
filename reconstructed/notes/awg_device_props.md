# AWG Device Props (Phase 14b-iii)

Notes for `reconstructed/include/zhinst/device/awg_device_props.hpp` and
`reconstructed/src/device/awg_device_props.cpp`.

## Symbols decoded

| Symbol | Address | Notes |
|--------|---------|-------|
| `AwgPathPatterns::AwgPathPatterns(AwgPathPatterns const&)` | 0x2cc4f0 | Copy ctor (3 strings) |
| `AwgPathPatterns::~AwgPathPatterns()` | 0x2cc480 | Dtor (3 strings) |
| `(anonymous)::AwgDevicePropsWithDefault::~AwgDevicePropsWithDefault()` | 0x2cc870 | Dtor for the local helper that backs each `getAwgDeviceProps<T>`'s temporary (3 inherited strings + bool@+0x48 + 4th string@+0x50) |
| `AwgDeviceProps::~AwgDeviceProps()` | 0xf81e0 | Out-of-line dtor (4 strings @ +0x08, +0x20, +0x38, +0x68) |
| `getAwgDeviceProps<UHFQA(4)>` | 0x2cc5f0 | Template specialization |
| `getAwgDeviceProps<UHFLI(1)>` | 0x2cc900 | |
| `getAwgDeviceProps<HDAWG(2)>` | 0x2ccb80 | **Only one that consults `dt`** (calls `dt.hasOption(0x13)`) |
| `getAwgDeviceProps<SHFQA(8)>` | 0x2cce30 | |
| `getAwgDeviceProps<SHFSG(16)>` | 0x2cd0c0 | |
| `getAwgDeviceProps<SHFQC_SG(32)>` | 0x2cd350 | |
| `getAwgDeviceProps<SHFLI(64)>` | 0x2cd5e0 | |
| `getAwgDeviceProps<VHFLI(256)>` | 0x2cd870 | Uses `awgPathPatternsMaloja` (= GrimselLi copy) |
| `getAwgDeviceProps<GHFLI(128)>` | 0x2cdb00 | Uses `awgPathPatternsGurnigel` (= GrimselLi copy) |
| `toAwgDeviceType(DeviceTypeCode, AwgSequencerType)` | 0x2cbd60 | 29-entry jump table over `(code-4)` for codes 4..32; SHFQC special case |
| `makeUnsupportedAwgSequencerErrorMessage(DeviceTypeCode, AwgSequencerType)` | 0x2cbdd0 | Builds `"Unsupported device or sequencer type (<dtype>, sequencer: <seq>)."` |
| Static init populating all 6 path-pattern globals | 0xdc900..0xdce30 | In `_GLOBAL__sub_I_properties.cpp` |

## AwgDeviceProps layout (binary: libc++; reconstruction: libstdc++)

| Offset (libc++) | Offset (libstdc++) | Size | Field | Note |
|---:|---:|---:|---|---|
| +0x00 | +0x00 | 4 | `AwgDeviceType deviceType` | one-hot bit per device |
| +0x04 | +0x04 | 4 | (padding) | |
| +0x08 | +0x08 | 24 / 32 | `std::string elfDataPattern` | path pattern |
| +0x20 | +0x28 | 24 / 32 | `std::string elfProgressPattern` | path pattern |
| +0x38 | +0x48 | 24 / 32 | `std::string enablePattern` | path pattern |
| +0x50 | +0x68 | 8 | `uint64_t maxWaveformSamples` | **field name INFERRED** |
| +0x58 | +0x70 | 8 | `uint64_t maxWaveformBytes` | **field name INFERRED** |
| +0x60 | +0x78 | 1 | `bool supportsExtraFeature` | **field name INFERRED** |
| +0x61 | +0x79 | 7 | (padding) | |
| +0x68 | +0x80 | 24 / 32 | `std::string fpgaRevisionPattern` | also "slaverevision" for HDAWG |
| Total | | | | 0x80 (libc++) / 0xa0 (libstdc++) |

## AwgPathPatterns layout

| Offset (libc++) | Offset (libstdc++) | Size | Field |
|---:|---:|---:|---|
| +0x00 | +0x00 | 24 / 32 | `std::string elfDataPattern` |
| +0x18 | +0x20 | 24 / 32 | `std::string elfProgressPattern` |
| +0x30 | +0x40 | 24 / 32 | `std::string enablePattern` |
| Total | | | 0x48 (libc++) / 0x60 (libstdc++) |

## AwgDevicePropsWithDefault (anonymous-namespace local helper)

Used as the stack temporary inside each `getAwgDeviceProps<T>`. Layout
in the binary:

| Offset (libc++) | Size | Field |
|---:|---:|---|
| +0x00 | 0x48 | `AwgPathPatterns base` (the 3 path strings) |
| +0x48 | 1 | `bool supportsExtraFeature` (the value pre-stored) |
| +0x49 | 7 | (padding) |
| +0x50 | 24 | `std::string fpgaRevisionPattern` (the 4th string) |
| Total | 0x68 | |

The dtor at 0x2cc870 destroys strings at offsets 0, 0x18, 0x30, 0x50 —
matching the 4 strings (3 inherited from `AwgPathPatterns` plus the
4th in the helper). We do NOT reproduce this helper as a separate
type in the reconstruction; instead, `buildAwgDeviceProps` (an
anonymous-namespace helper) directly constructs the final
`AwgDeviceProps` from the per-template inputs.

## Per-template constants

| T | addr | bool | +0x50 (samples) | +0x58 (bytes) | path patterns | 4th string |
|---|---|---:|---:|---:|---|---|
| UHFLI(1) | 0x2cc900 | 0 | 0x10000000 (256M) | 0xd0000000 (~3.5G) | Default | fpgarevision |
| HDAWG(2) | 0x2ccb80 | 1 | cond(ME) | 0x180000000 (6G) | Default | slaverevision |
| UHFQA(4) | 0x2cc5f0 | 0 | 0x10000000 | 0xd0000000 | Default | fpgarevision |
| SHFQA(8) | 0x2cce30 | 0 | 0x80000000 (2G) | 0x200000000 (8G) | GrimselQa | fpgarevision |
| SHFSG(16) | 0x2cd0c0 | 1 | 0x80000000 | 0x100000000 (4G) | GrimselSg | fpgarevision |
| SHFQC_SG(32) | 0x2cd350 | 1 | 0x80000000 | 0x100000000 | GrimselSg | fpgarevision |
| SHFLI(64) | 0x2cd5e0 | 1 | 0x80000000 | 0x100000000 | GrimselLi | fpgarevision |
| VHFLI(256) | 0x2cd870 | 1 | 0x80000000 | 0x100000000 | Maloja (= GrimselLi copy) | fpgarevision |
| GHFLI(128) | 0x2cdb00 | 1 | 0x80000000 | 0x100000000 | Gurnigel (= GrimselLi copy) | fpgarevision |

HDAWG's conditional sample limit:
```
mov   esi, 0x13                    ; DeviceOption value 13 = ME (Memory Extension)
call  zhinst::DeviceType::hasOption
test  r14b, r14b
mov   eax, 0x80000000              ; "ME set" value
mov   r15d, 0x10000000             ; "ME clear" value
cmovne r15, rax                    ; pick high if ME set
mov   QWORD PTR [rbx+0x50], r15
```

## Path-pattern globals

| Global | Address | String 1 (data) | String 2 (progress) | String 3 (enable) |
|---|---|---|---|---|
| `awgPathPatternsDefault` | 0xb84fc0 | `/$device$/awgs/$index$/elf/data` (29) | `.../awgs/$index$/elf/progress` (35) | `.../awgs/$index$/enable` (29) |
| `awgPathPatternsGrimselQa` | 0xb85008 | `/$device$/qachannels/$index$/generator/elf/data` (47) | `.../generator/elf/progress` (51) | `.../generator/enable` (45) |
| `awgPathPatternsGrimselSg` | 0xb85050 | `/$device$/sgchannels/$index$/awg/elf/data` (41) | `.../awg/elf/progress` (45) | `.../awg/enable` (39) |
| `awgPathPatternsGrimselLi` | 0xb85098 | `/$device$/raw/awgs/$index$/elf/data` (35) | `.../raw/awgs/$index$/elf/progress` (39) | `.../raw/awgs/$index$/enable` (33) |
| `awgPathPatternsGurnigel` | 0xb850e0 | (copy of GrimselLi, copy-constructed at static init) | | |
| `awgPathPatternsMaloja` | 0xb85128 | (copy of GrimselLi) | | |

The 4th-string literals (used as `fpgaRevisionPattern` in the helper):

| Literal | Address | Length | Used by |
|---|---|---:|---|
| `/$device$/system/fpgarevision` | 0x90b3f8 | 29 | UHFLI, UHFQA, SHFQA, SHFSG, SHFQC_SG, SHFLI, GHFLI, VHFLI |
| `/$device$/system/slaverevision` | 0x90b416 | 30 | HDAWG only |

## Error-message format

`makeUnsupportedAwgSequencerErrorMessage` produces:

```
Unsupported device or sequencer type (<dtype>, sequencer: <seq>).
```

Source string fragments:

| Fragment | Address | Length | LE bytes when SSO-written |
|---|---|---:|---|
| `"Unsupported device or sequencer type ("` | 0x90b138 | 0x26 (38) | (heap-alloc, full string) |
| `", sequencer: "` | 0x90b15f | 0xd (13) | (append) |
| `")."` | 0x90b16d | 0x2 (2) | (append) |

The sequencer name is built inline as a SSO string:

| seq value | name | LE write |
|---:|---|---|
| 0 | `"auto"` | size byte 0x08 (= 4*2 long-bit-clear), `*(uint32_t*)(buf+0) = 0x6f747561` ("auto" LE) |
| 1 | `"QA"` | size byte 0x04, `*(uint16_t*)(buf+0) = 0x4151` ("QA" LE) |
| 2 | `"SG"` | size byte 0x04, `*(uint16_t*)(buf+0) = 0x4753` ("SG" LE) |
| other | `"unknown"` | size byte 0x0e, `*(uint32_t*)(buf+0) = 0x6e6b6e75` ("unkn" LE), `*(uint32_t*)(buf+3) = 0x6e776f6e` ("nown" LE; overlap by 1 at index 3 = 'n') |

Note: the libc++ short-string size byte stores `2 * size` (low bit = 0
marks SSO/short form), which is why a 4-char string writes `0x08`.

## Asymmetries / quirks

## Field names — VERIFIED (Phase 21f)

Field names verified from binary consumer analysis:

| Offset | Old name (inferred 14b-iii) | Correct name | Consumer evidence |
|--------|----------------------------|--------------|-------------------|
| +0x50 | `maxWaveformSamples` (uint64) | `maxElfSize` (uint64) | JSON key `"maxelfsize"` in `compileSeqc` @0xf6a41 |
| +0x58 | `maxWaveformBytes` (uint64) | `addressImpl` (uint32) | AWGCompilerImpl ctor @0x103b99 → config.addressImpl (+0x10) |
| +0x5c | (part of above) | `sampleFormat` (uint32) | writeWavesToElf* @0x10e049 → config.sampleFormat (+0x04) |
| +0x60 | `supportsExtraFeature` (bool) | `isHirzel` (bool) | compileSeqc @0xf67cd → config.isHirzel (+0x18) |

The +0x58 was a single qword store in the binary but consumers read
it as two independent uint32_t fields. This explains the previously
puzzling values:
- UHFLI/UHFQA: 0xd0000000 = addressImpl=0xd0000000, sampleFormat=0
- HDAWG: 0x180000000 = addressImpl=0x80000000, sampleFormat=1
- SHFQA: 0x200000000 = addressImpl=0x00000000, sampleFormat=2

`isHirzel` = true for: HDAWG, SHFSG, SHFQC_SG, SHFLI, GHFLI, VHFLI.
`isHirzel` = false for: UHFLI, UHFQA, SHFQA.

2. **AwgSequencerType has no enumerator for "unknown"**: the
   error-message formatter has a default branch that names any
   unrecognized sequencer "unknown", but `toAwgDeviceType` only ever
   distinguishes 0/1/2 (other values fall through to the "return 0"
   path). We model the enum with the 3 named values only.

3. **Gurnigel and Maloja are runtime copies of GrimselLi**: in the
   binary, the static-init code allocates GrimselLi separately and
   then uses `AwgPathPatterns(const&)` copy-ctor to clone it into the
   Gurnigel and Maloja globals. We model these as `const&`-returning
   functions that just return the GrimselLi singleton — semantically
   equivalent and avoids three identical static buffers.

4. **AwgDevicePropsWithDefault is local-only**: it never appears as a
   public API; it's just the temporary stack layout each
   `getAwgDeviceProps<T>` builds before transferring its contents
   (with string moves) into the output `AwgDeviceProps*`. We do not
   reproduce it as a named type.

5. **The same source TU also defines `getDeviceConstants(AwgDeviceType)`**
   (binary 0x2cc0c0). The .rodata path-info string at 0x90b1a8 is
   `/builds/labone/labone/ziAWG/ziAWGDevice/src/constants.cpp` — so
   both `getDeviceConstants` and `getAwgDeviceProps` came from
   `constants.cpp` in the original tree. The `DeviceConstants` struct
   header already exists in `device_constants.hpp` (from earlier
   work); only the `getDeviceConstants` body remains to be
   reconstructed in a future sub-phase.
