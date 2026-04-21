# Struct Layouts

Raw offset maps from disassembly analysis. All offsets are from the start
of the respective struct.

## AsmRegister (8 bytes) — NEW Phase 2

| Offset | Size | Type  | Name   | Notes                              |
|--------|------|-------|--------|------------------------------------|
| +0x00  | 4    | int   | value  | Register number (0-15), -1 = unset |
| +0x04  | 1    | bool  | valid  | Whether register slot is active    |
| +0x05  | 3    | pad   |        | Alignment padding to 8 bytes       |

Previously modeled as `enum class AsmRegister : int`. Corrected based on Phase 2
analysis showing separate value and valid fields stored as 8-byte units.

## AsmCommands (this pointer)

| Offset | Size | Type                         | Name                | Notes                     |
|--------|------|------------------------------|---------------------|---------------------------|
| +0x00  | 8    | (vtable or base)             |                     |                           |
| +0x10  | 8    | unique_ptr<AsmCommandsImpl>  | impl_               | Pimpl for device dispatch |
| +0x20  | 16?  | shared_ptr<WavetableFront>   | wavetable_          | Approx location           |
| +0x40  | ?    | function<void(string const&)>| errorHandler_       | For toInt32 error reports |
| +0x50  | 4    | int                          | wavetableFrontIndex_| Passed as lineNumber to impl_ |
| +0x54  | 4    | int                          | numChannelGroups_   | From AWGCompilerConfig+0x1c; 1/2/4; Node vector pre-alloc |

## AssemblerInstr (0x80 = 128 bytes) — CORRECTED Phase 2

| Offset | Size | Type               | Name        | Notes                              |
|--------|------|--------------------|-------------|------------------------------------|
| +0x00  | 4    | uint32_t           | cmd         | Assembler::Command opcode          |
| +0x04  | 4    | (padding)          |             |                                    |
| +0x08  | 24   | vector<Immediate>  | immediates  | Input operands (begin/end/cap)     |
| +0x20  | 8    | AsmRegister        | reg2        | **Destination** (int+bool+3pad)    |
| +0x28  | 8    | AsmRegister        | reg0        | Source register 1                  |
| +0x30  | 8    | AsmRegister        | reg1        | Source register 2                  |
| +0x38  | 24   | vector<Immediate>  | outputs     | **Output operands** (NEW Phase 2)  |
| +0x50  | 24   | std::string        | label       | Branch target (SSO, 32 bytes?)     |
| +0x68  | 24   | std::string        | comment     | Annotation string (NEW Phase 2)    |
| +0x80  |      | END                |             |                                    |

Key corrections from Phase 2:
- Register order is reg2(dest)/reg0(src1)/reg1(src2), NOT reg0/reg1/reg2
- +0x38 is a second vector<Immediate> (outputs), NOT part of label
- +0x50 is label (moved from +0x38)
- +0x68 is comment string (was unknown)
- AsmRegister is struct {int value; bool valid;} (8 bytes), not enum

## AsmList::Asm (0xA8 bytes) — CONFIRMED Phase 2b

Previously called "AsmEntry". Real nested type is `AsmList::Asm`.
Confirmed from ~Asm (0x122dd0), append (0x15d180), print (0x264250), serialize (0x2646d0).

| Offset | Size | Type               | Name            | Notes                    |
|--------|------|--------------------|-----------------|--------------------------|
| +0x00  | 4    | int                | sequenceId      | From createUniqueID(false) (TLS+0x40) |
| +0x04  | 4    | (padding)          |                 |                          |
| +0x08  | 0x80 | AssemblerInstr     | assembler       |                          |
| +0x88  | 4    | int                | wavetableFront  |                          |
| +0x8C  | 4    | (padding)          |                 |                          |
| +0x90  | 16   | shared_ptr<Node>   | node            | May be null              |
| +0xA0  | 1    | bool               | isWaveformCmd   | (cmd-3) < 3u             |
| +0xA1  | 7    | (padding)          |                 | Align to 0xA8            |

## AsmList (0x18 bytes) — CONFIRMED Phase 2b

Exactly `std::vector<AsmList::Asm>`. No additional fields.
Confirmed from ~AsmList (0x11d5b0), append (0x15d180), maxRegister (0x269910).

| Offset | Size | Type  | Name     | Notes               |
|--------|------|-------|----------|----------------------|
| +0x00  | 8    | ptr   | begin    | data pointer         |
| +0x08  | 8    | ptr   | end      | past-the-end pointer |
| +0x10  | 8    | ptr   | capacity | capacity end pointer |

## Node (0x110 bytes) — CONFIRMED Phase 2d

Fully confirmed from simple ctor (0x12ace0), full ctor (0x26c4a0), dtor (0x12afe0).
Field names confirmed from toJson/fromJson JSON key strings in .rodata.
Inherits `enable_shared_from_this<Node>` (weak_ptr at +0x00/+0x08).

| Offset | Size | Type                          | Name                    | JSON Key                  | Notes                         |
|--------|------|-------------------------------|-------------------------|---------------------------|-------------------------------|
| +0x00  | 8    | ptr (enable_shared_from_this) | _sp_ptr                 |                           | weak_ptr embedded             |
| +0x08  | 8    | ctrl (enable_shared_from_this)| _sp_ctrl                |                           | dtor: __release_weak if set   |
| +0x10  | 4    | int                           | nodeId                  | "nodeId"                  | TLS counter (node_id++)       |
| +0x14  | 4    | int                           | asmId                   | "asmId"                   | toString prints this          |
| +0x18  | 8    | uint64_t                      | _reserved0              |                           | zeroed in ctor                |
| +0x20  | 8    | uint64_t                      | _reserved1              |                           | zeroed; dtor: __release_weak  |
| +0x28  | 24   | vector<optional<string>>      | wavesPerDev             | "wavesPerDev"             | waveform names per device     |
| +0x40  | 4    | int                           | deviceIndex             | "deviceIndex"             | index into wavesPerDev; -1=none|
| +0x44  | 4    | NodeType (int)                | type                    | "type"                    | toString prints type2str()    |
| +0x48  | 32   | PlayConfig                    | config                  | "config"                  | primary play config           |
| +0x68  | 32   | PlayConfig                    | currentCwvf             | "currentCwvf"             | current CWVF config           |
| +0x88  | 8    | AsmRegister                   | lengthReg               | "lengthReg"               | int(4) + bool(4 padded)       |
| +0x90  | 4    | int                           | length                  | "length"                  |                               |
| +0x94  | 8    | AsmRegister                   | indexOffsetReg          | "indexOffsetReg"          |                               |
| +0x9C  | 4    | int                           | tableIndex              | "tableIndex"              | -1 = none                     |
| +0xA0  | 24   | vector<weak_ptr<Node>>        | play                    | "play"                    | dtor iterates, releases weak  |
| +0xB8  | 16   | shared_ptr<Node>              | next                    | "next"                    | next in sibling chain         |
| +0xC8  | 24   | vector<shared_ptr<Node>>      | branches                | "branches"                | child nodes (Branch type 4)   |
| +0xE0  | 16   | shared_ptr<Node>              | loop                    | "loop"                    | loop/else branch link         |
| +0xF0  | 16   | weak_ptr<Node>                | parent                  | "parent"                  | dtor: __release_weak          |
| +0x100 | 4    | int                           | globalRate              | "globalRate"              | rate for Rate nodes           |
| +0x104 | 4    | unsigned int                  | defaultPrecompFlags     | "defaultPrecompFlags"     | precompFlags for SetPrecomp   |
| +0x108 | 1    | bool                          | loopBodyRunsAtLeastOnce | "loopBodyRunsAtLeastOnce" |                               |
| +0x109 | 1    | bool                          | branchMaySkipAllBodies  | "branchMaySkipAllBodies"  |                               |
| +0x10A | 2    | (padding)                     |                         |                           |                               |
| +0x10C | 4    | int                           | trig                    | "trig"                    |                               |

## Signal (0x58 = 88 bytes) — NEW Phase 3c

Waveform signal container. Holds sample data, per-sample markers, and per-channel
marker bit ORs. Supports reserve-only mode (lazy allocation via checkAllocation()).

| Offset | Size | Type              | Name         | JSON Key      | Notes                          |
|--------|------|-------------------|--------------|---------------|--------------------------------|
| +0x00  | 24   | vector\<double\>  | samples_     | "data"        | Waveform sample values         |
| +0x18  | 24   | vector\<uint8_t\> | markers_     | "marker"      | Per-sample marker bytes        |
| +0x30  | 24   | vector\<uint8_t\> | markerBits_  | "markerBits"  | Per-channel marker bit ORs     |
| +0x48  | 2    | uint16_t          | channels_    | "channels"    | Number of channels             |
| +0x4A  | 1    | bool              | reserveOnly_ | "reserveOnly" | Lazy allocation flag           |
| +0x4B  | 5    | (padding)         |              |               |                                |
| +0x50  | 8    | uint64_t          | length_      | "length"      | Samples per channel            |
| +0x58  |      | END               |              |               |                                |

Related types:
- `MarkerBitsPerChannel` = typedef for `std::vector<uint8_t>`
- `SampleFormat` enum: Cervino=0 (16-bit via double2awg), Hirzel16=1
- `ReserveOnly` = empty tag struct for constructor overload dispatch

## Waveform (base class, 0xD8 bytes) — CONFIRMED Phase 5a

Base class of WaveformFront. All 11 methods reconstructed. Field names confirmed
from toJson/fromJson JSON keys. Signal is 0x58 bytes (corrected Phase 3c).

| Offset | Size | Type                | Name            | JSON key           | Notes                           |
|--------|------|---------------------|-----------------|--------------------|---------------------------------|
| +0x00  | 24   | std::string         | name            | "name"             | waveform name (SSO)             |
| +0x18  | 4    | File::Type (enum)   | waveformType    | "type"             | 0=CSV, 1=RAW, 2=GEN            |
| +0x1C  | 4    | (padding)           |                 |                    |                                 |
| +0x20  | 24   | std::string         | secondaryName   | "functionArgs"     | function arguments string       |
| +0x38  | 16   | shared_ptr\<File\>  | file            | "fileName"         | source file reference           |
| +0x48  | 1    | bool                | used            | "load"             | set by asmPlay/asmPrefetch      |
| +0x49  | 3    | (padding)           |                 |                    |                                 |
| +0x4C  | 4    | uint32_t            | addressValue    | "globalAddress"    | AddressImpl\<uint\>             |
| +0x50  | 24   | std::string         | thirdString     | "genFunc"          | generator function name         |
| +0x68  | 4    | uint32_t            | playWord        | "playConfig"       | packed PlayConfig               |
| +0x6C  | 4    | int32_t             | playIndex       | "waveIndex"        | -1 = compute playWord           |
| +0x70  | 4    | int                 | seqRegWidth     | "minLengthSamples" | from DeviceConstants+0x40       |
| +0x74  | 4    | int                 | field74         | "allocationSize"   | init=0                          |
| +0x78  | 8    | DeviceConstants*    | deviceConstants |                    | pointer to device config         |
| +0x80  | 0x58 | Signal              | signal          | "signal"           | see Signal layout above          |
| +0xD8  |      | END                 |                 |                    |                                  |

## WaveformFile (Waveform::File, 0x40 bytes) — CONFIRMED Phase 5a

Managed object inside shared_ptr at Waveform+0x38. Confirmed from File::operator== (0x2a9680)
and fromJson make_shared allocation (0x58 = 0x18 ctrl + 0x40 object).

| Offset | Size | Type              | Name      | Notes                           |
|--------|------|-------------------|-----------|---------------------------------|
| +0x00  | 24   | std::string       | name      | filename                        |
| +0x18  | 4    | int32_t           | field18   | compared in operator==          |
| +0x1C  | 4    | int32_t           | field1C   | compared in operator==          |
| +0x20  | 4    | int32_t           | field20   | compared in operator==          |
| +0x24  | 4    | (padding)         |           |                                 |
| +0x28  | 24   | vector\<uint8_t\> | data      | file contents (binary blob)     |
| +0x40  |      | END               |           |                                 |

## WaveformFront (extends Waveform, 0xF8 bytes total) — CONFIRMED Phase 2e

Confirmed from allocate_shared (0x110 - 0x18 control block = 0xF8), copy-rename ctor (0x2a2510),
toString (0x2c5120), __on_zero_shared dtor (0x2a1300).

| Offset | Size | Type           | Name        | Notes                           |
|--------|------|----------------|-------------|---------------------------------|
| +0x00  | 0xD8 | Waveform       | (base)      | see Waveform layout above       |
| +0xD8  | 4    | int            | frontField1 | init=1 in copy ctor             |
| +0xDC  | 1    | bool           | frontBool1  | init=0 in copy ctor             |
| +0xDD  | 1    | bool           | frontBool2  | copied from source              |
| +0xDE  | 2    | (padding)      |             |                                 |
| +0xE0  | 24   | vector<Value>  | values      | per-parameter values (0x28 each)|
| +0xF8  |      | END            |             |                                 |

## PlayConfig (0x20 = 32 bytes) — CORRECTED Phase 2

| Offset | Size | Type     | Name         | Notes                              |
|--------|------|----------|--------------|-------------------------------------|
| +0x00  | 4    | uint32_t | channelMask  | AddressImpl<uint>                   |
| +0x04  | 4    | int      | rate         | (was: holdCount)                    |
| +0x08  | 4    | uint32_t | suppress     | AddressImpl<uint>                   |
| +0x0C  | 1    | bool     | is4Channel   | (was: fourChannel)                  |
| +0x10  | 4    | uint32_t | markerBits   | AddressImpl<uint>                   |
| +0x14  | 4    | uint32_t | trigger      | AddressImpl<uint>                   |
| +0x18  | 4    | uint32_t | precompFlags | AddressImpl<uint>                   |
| +0x1C  | 1    | bool     | now          | (was: isBool)                       |
| +0x1D  | 1    | bool     | hold         | (was: isHold)                       |
| +0x1E  | 1    | bool     | dummy        | (was: isDummy)                      |

Play-word bit layout (encodeCwvf):
- Bits  0-1 : channels (2 bits)
- Bits  2-5 : rate (4 bits)
- Bits  6-19: suppress (14 bits)
- Bits 20-21: fourChannel / is4Channel (2 bits)
- Bit  22   : defaultRate (1 bit)
- Bit  23   : dummy (1 bit)
- Bits 24-27: markerBits (4 bits)
- Bits 28-29: trigger (2 bits)
- Bits 30-31: precompFlag (2 bits)

Special constant: holdSuppressExceptSigouts = 0x27C (636)

## Immediate (0x1C = 28 bytes) — std::variant<AddressImpl<uint>, int, string>

Source: `/builds/labone/labone/ziAWG/ziAWGUtils/src/main/include/Value.hpp`

| Offset | Size | Type                   | Name     | Notes                              |
|--------|------|------------------------|----------|------------------------------------|
| +0x00  | 24   | union                  | storage  | Holds AddressImpl/int/string       |
| +0x18  | 4    | uint32_t               | index_   | 0=AddressImpl<uint>, 1=int, 2=str  |

Variant index mapping:
- 0: `AddressImpl<uint32_t>` — also used by `Immediate(unsigned int)` ctor
- 1: `int`
- 2: `std::string` (libc++ SSO, 24 bytes)
- 0xFFFFFFFF: valueless_by_exception

Key observations:
- `Immediate(uint)` and `Immediate(AddressImpl<uint>)` produce identical code (both set index=0)
- `holdsUnsigned()` simply checks `index_ == 0`
- Visitor dispatch uses function-pointer tables in `.data.rel.ro` (not switch statements)
- Destructor sets index to 0xFFFFFFFF after destroying active member

## Value (0x28 = 40 bytes) — boost::variant<int, bool, double, string> + outer tag

Source: `/builds/labone/labone/ziAWG/ziAWGUtils/src/main/include/Value.hpp`

| Offset | Size | Type              | Name      | Notes                              |
|--------|------|-------------------|-----------|------------------------------------|
| +0x00  | 4    | ValueType (enum)  | type_     | 0=unspec, 1=int, 2=bool, 3=dbl, 4=str |
| +0x04  | 4    | (padding)         |           |                                    |
| +0x08  | 4    | int32_t           | which_    | boost::variant discriminator       |
| +0x0C  | 4    | (padding)         |           |                                    |
| +0x10  | 24   | union             | storage_  | int/bool/double/string             |
| +0x28  |      | END               |           |                                    |

Outer type tag (ValueType) at +0x00:
- 0: Unspecified → all conversions throw ValueException
- 1: Int → int32_t at +0x10
- 2: Bool → bool (1 byte) at +0x10
- 3: Double → double (8 bytes) at +0x10
- 4: String → std::string (24 bytes SSO) at +0x10
- >4: throws "unknown value type"

Boost which_ at +0x08:
- Decoded as: `(which_ >> 31) ^ which_` (handles negative during assignment)
- 0=int, 1=bool, 2=double, 3=string
- Destructor uses which_ (not type_) to decide if string needs freeing

Conversion methods throw `ValueException` (boost::throw_exception) with messages like:
- "unspecified value type detected in toDouble conversion"
- "unknown value type detected in toInt conversion"
Type-mismatch on boost::get throws `boost::bad_get`.

`toInt()` double→int path: range-checks via boost::numeric, uses ceil (negative)
or floor (positive), then cvttsd2si. On int overflow, catches and retries as
uint32_t conversion (unsigned fallback).

`toBool()` string path: compares first 4 bytes against "true" (memcmp).
`toBool()` double path: checks `fabs(d) >= epsilon` (constant from .rodata).

`operator==` for doubles uses `fabs(a - b) < epsilon` (approximate equality).

## DeviceConstants (0x90 = 144 bytes) — Phase 3a, REVISED Phase 7e

POD struct, no vtable. Populated by `getDeviceConstants(AwgDeviceType)` at 0x2cc0c0.
Source: `/builds/labone/labone/ziAWG/ziAWGDevice/src/constants.cpp`

**Phase 7e revision:** The original "RegisterBank" sub-struct groupings were removed.
Cross-referencing all consumer sites (Prefetch, WavetableIR, AWGAssemblerImpl, Cache,
WaveformFront) revealed the 16-byte groups are unrelated scalar fields. Field names
now reflect verified semantic usage from binary disassembly.

| Offset | Size | Type          | Name                | Notes                                          |
|--------|------|---------------|---------------------|-------------------------------------------------|
| +0x00  | 4    | uint32_t      | deviceType          | AwgDeviceType enum value                        |
| +0x04  | 1    | bool          | hasExtendedReg      | true only for HDAWG (type=2)                    |
| +0x05  | 3    | (padding)     |                     |                                                 |
| +0x08  | 4    | uint32_t      | waveformRegBase     | HW register address (0x95b2/0x110f6/etc.)       |
| +0x0C  | 4    | uint32_t      | waveformMemorySize  | minBlockSize (WavetableIR), cacheSize (Prefetch) |
| +0x10  | 4    | uint32_t      | sampleLength        | Cache ctor arg (16–64)                          |
| +0x14  | 4    | uint32_t      | waveformAlignment   | pageSize (clampToCache), alignment (WavetableIR) |
| +0x18  | 4    | uint32_t      | cachePageCount      | clampToCache array[cacheType=0]                 |
| +0x1C  | 4    | uint32_t      | maxBlocks           | clampToCache[1], WavetableIR maxBlocks. Always 2 |
| +0x20  | 4    | uint32_t      | field_20            | Always 16 — no known consumer                   |
| +0x24  | 4    | uint32_t      | field_24            | Always 64 — no known consumer                   |
| +0x28  | 8    | uint64_t      | registerDepth       | Register index upper bound (getReg). Always 16   |
| +0x30  | 8    | uint64_t      | memoryDepth         | Memory address upper bound (opcode4). Always 1024 |
| +0x38  | 4    | uint32_t      | sequencerRegBase    | HW register address (0x115c/0x0d05)             |
| +0x3C  | 4    | uint32_t      | field_3C            | Always 6 — no known consumer                    |
| +0x40  | 4    | uint32_t      | waveformGranularity | Waveform page cap. Also WaveformFront.seqRegWidth |
| +0x44  | 4    | uint32_t      | waveformPageSize    | Waveform page divisor (8–48)                    |
| +0x48  | 4    | uint32_t      | field_48            | Values: 0, 128, 384 — no known consumer         |
| +0x4C  | 4    | uint32_t      | field_4C            | Values: 16, 32, 96 — no known consumer          |
| +0x50  | 4    | uint32_t      | bitsPerSample       | Memory bits calc. Always 16                     |
| +0x54  | 4    | uint32_t      | field_54            | Values: 0, 2 — no known consumer                |
| +0x58  | 8    | uint64_t      | waveformMemSize     | 0x400/0x4000/0x8000                             |
| +0x60  | 8    | uint64_t      | maxSequenceLen      | 16000                                           |
| +0x68  | 4    | uint32_t      | seqClockDivider     | 0 or 1165                                       |
| +0x6C  | 4    | (padding)     |                     |                                                 |
| +0x70  | 8    | double        | samplingRate        | Hz (1.8e9–12.0e9)                              |
| +0x78  | 4    | uint32_t      | numOutputPorts      | 0, 10, or 12                                    |
| +0x7C  | 4    | uint32_t      | numAWGCores         | 0, 3, or 5                                      |
| +0x80  | 1    | bool          | hasDIO              | DIO support flag                                |
| +0x81  | 3    | (padding)     |                     |                                                 |
| +0x84  | 4    | uint32_t      | numDIOBits          | 0, 6, or 8                                      |
| +0x88  | 1    | bool          | hasPrecomp          | Precomp support + Prefetch DA control            |
| +0x89  | 7    | (padding)     |                     | Align to 0x90                                   |

Nested type `DeviceConstants::Register` contains anonymous enums:
- `{unnamed type#7}` = 0x44 (68) — sync register A
- `{unnamed type#8}` = 0x45 (69) — sync register B

Device type mapping (CORRECTED Phase 3d from getAwgDeviceTypeString/FromString):
| Enum | Device     | Codename      | SampRate | WaveMem | Hirzel | Notes              |
|------|-----------|---------------|----------|---------|--------|--------------------|
| 1    | UHFLI     | cervino       | 1.8 GHz  | 1024    | no     | Old UHF lock-in    |
| 2    | HDAWG     | hirzel        | 2.4 GHz  | 16384   | yes    | Only hasExtendedReg|
| 4    | UHFQA     | klausen       | 1.8 GHz  | 1024    | no     | Old UHF QA         |
| 8    | SHFQA     | grimsel_qa    | 2.0 GHz  | 16384   | yes    | SHF quantum analyzer|
| 16   | SHFSG     | grimsel_sg    | 2.0 GHz  | 32768   | yes    | SHF signal gen     |
| 32   | SHFQC_SG  | grimsel_qc_sg | 2.0 GHz  | 32768   | yes    | SG part of combo   |
| 64   | SHFLI     | grimsel_li    | 2.0 GHz  | 32768   | yes    | SHF lock-in        |
| 128  | GHFLI     | gurnigel      | 12.0 GHz | 32768   | no     | High-speed; Cervino |
| 256  | VHFLI     | maloja        | 2.0 GHz  | 32768   | no     | Shares config w/64; Cervino |

## AWGCompilerConfig (~0xC0 bytes) — NEW Phase 3d

Compiler configuration struct. Passed to Compiler, Prefetch, CustomFunctions,
StaticResources. Contains device type, channel grouping, include paths, search path.
4 own methods: dtor, getAwgDeviceTypeString (static), getAwgDeviceTypeFromString (static),
getChannelGroupingModeString (const).

| Offset | Size | Type                  | Name              | Notes                              |
|--------|------|-----------------------|-------------------|------------------------------------|
| +0x00  | 4    | AwgDeviceType (int)   | deviceType        | Checked by getChannelGroupingModeString |
| +0x04  | 4    | int                   | unknown_04        |                                    |
| +0x08  | 4    | int                   | unknown_08        |                                    |
| +0x0C  | 4    | int                   | unknown_0c        |                                    |
| +0x10  | 4    | uint32_t              | addressImpl       | AddressImpl\<uint\> for WavetableFront |
| +0x14  | 4    | int                   | unknown_14        |                                    |
| +0x18  | 1    | bool                  | isHirzel          | 1 = Hirzel device (confirmed Phase 7c) |
| +0x19  | 1    | uint8_t               | cacheType         | 0=Normal, 1=Aligned (discovered Phase 7d, clampToCache) |
| +0x1A  | 2    | (padding)             |                   |                                    |
| +0x1C  | 4    | int                   | numChannelGroups  | 1/2/4; only meaningful for HDAWG   |
| +0x20  | 4    | (unknown)             | unknown_20        |                                    |
| +0x24  | 4    | int                   | deviceIndex       | Device index (confirmed Phase 7c)  |
| +0x30  | 24   | std::string           | string_30         | Conditionally owned (guard at +0x48) |
| +0x48  | 1    | bool                  | string_30_owned   | Controls dtor of string_30         |
| +0x50  | 24   | std::string           | string_50         | Conditionally owned (guard at +0x68) |
| +0x68  | 1    | bool                  | string_50_owned   | Controls dtor of string_50         |
| +0x70  | 24   | vector\<string\>      | includePaths      | begin/end/cap at +0x70/+0x78/+0x80 |
| +0x88  | 8     | (unknown)             | unknown_88        |                                    |
| +0x90  | 4     | uint32_t              | debugFlags        | Bitmask: 0x02=old AST, 0x04=SeqC AST, 0x08=tree/asm |
| +0x94  | 12    | (unknown)             | unknown_94        |                                    |
| +0xA0  | 4    | int32_t               | wavetableSize     | Sign-extended to size_t            |
| +0xA4  | 4    | (pad)                 |                   |                                    |
| +0xA8  | 24   | boost::filesystem::path | searchPath      | Dtor frees at +0xA8/+0xB8         |

Channel grouping (HDAWG only):
- numChannelGroups=1 → "4x2" (4 groups of 2 channels)
- numChannelGroups=2 → "2x4" (2 groups of 4 channels)
- numChannelGroups=4 → "1x8" (1 group of 8 channels)

## AWGAssembler (0x08 bytes) — NEW Phase 4

Pimpl wrapper. Raw owning pointer (not unique_ptr — dtor manually calls ~Impl
then `operator delete(ptr, 0x170)`). All methods forward to AWGAssemblerImpl.

| Offset | Size | Type                | Name   | Notes                           |
|--------|------|---------------------|--------|---------------------------------|
| +0x00  | 8    | AWGAssemblerImpl*   | pImpl_ | Allocated with `new(0x170)`     |

## AWGAssemblerImpl (0x170 = 368 bytes) — NEW Phase 4

Full assembler implementation. Contains parser state, opcode output buffer,
label resolution bimap, error message accumulator, and ELF output support.

| Offset | Size | Type                          | Name             | Notes                              |
|--------|------|-------------------------------|------------------|------------------------------------|
| +0x00  | 8    | DeviceConstants const*        | deviceConstants_ | Set from ctor arg                  |
| +0x08  | 24   | std::string                   | filename_        | Source file path (from assembleFile) |
| +0x20  | 24   | std::string                   | str1_            | Unknown string                     |
| +0x38  | 24   | std::string                   | asmSource_       | Full assembly source text          |
| +0x50  | 24   | std::vector\<uint64_t\>       | opcodes_         | Output opcode buffer               |
| +0x68  | 4    | uint32_t                      | memoryOffset_    | ELF memory offset                  |
| +0x6C  | 4    | (padding)                     |                  |                                    |
| +0x70  | 8    | uint64_t                      | lineNumber_      | Current source line being processed |
| +0x78  | 24   | std::vector\<std::string\>    | sourceLines_     | Per-line source strings            |
| +0x90  | 24   | std::vector\<Message\>        | messages_        | Accumulated error/warning messages |
| +0xA8  | 8    | size_t                        | bimapCount0_     | Bimap internal state               |
| +0xB0  | 96   | boost::bimap\<string,int\>    | labels_          | Label name ↔ program counter       |
| +0x110 | 8    | (unknown)                     |                  | Checked in dtor vs &this+0x100     |
| +0x118 | 8    | (unknown)                     |                  |                                    |
| +0x120 | 8    | ptr                           | streamObj_       | Dtor: virtual dtor if != &this+0x100 |
| +0x128 | 8    | (reserved)                    |                  |                                    |
| +0x130 | 24   | std::vector\<?\>              | vec3_            | Unknown vector                     |
| +0x148 | 8    | ptr                           | hashTable_       | Freed with capacity*8              |
| +0x150 | 8    | size_t                        | hashTableCap_    | Hash table capacity                |
| +0x158 | 8    | ptr                           | linkedList_      | Linked list head (nodes 0x28 each) |
| +0x160 | 8    | size_t                        | count4_          |                                    |
| +0x168 | 4    | float                         | sampleRate_      | Init 1.0f (0x3f800000)            |
| +0x16C | 4    | (padding)                     |                  | Align to 0x170                     |

## AWGAssemblerImpl::Message (0x20 bytes) — NEW Phase 4

Error/warning message accumulated during assembly.

| Offset | Size | Type        | Name       | Notes                    |
|--------|------|-------------|------------|--------------------------|
| +0x00  | 8    | uint64_t    | lineNumber | Source line number        |
| +0x08  | 24   | std::string | text       | Message text (SSO)       |

## AsmExpression (parse tree node) — NEW Phase 4

Reconstructed from usage in opcode encoding and pipeline methods.
Allocated via `make_shared` (0xC0 bytes total including control block).

| Offset | Size | Type                                      | Name       | Notes                              |
|--------|------|-------------------------------------------|------------|------------------------------------|
| +0x00  | 4    | int                                       | type       | 1=register, 2=label, 3=integer    |
| +0x08  | 24   | std::string                               | name       | Label/symbol name (type==2)        |
| +0x20  | 24   | std::string                               | comment    | Comment text (from // extraction)  |
| +0x38  | 4    | int (Assembler::Command)                  | command    | Instruction command enum           |
| +0x3C  | 4    | int                                       | value      | Reg# (type==1) or imm (type==3)   |
| +0x40  | 24   | vector\<shared_ptr\<AsmExpression\>\>     | children   | Operand sub-expressions            |
| +0x58  | 4    | int                                       | lineNumber | Source line number                 |
| +0x60  | 24   | std::string                               | labelName  | Label identifier                   |
| +0x78  | 1    | bool                                      | labelType  | 0=definition, 1=reference          |
| +0x90  | 1    | bool                                      | noOpt      | Disable optimization flag          |
| +0xA0  | 1    | bool                                      | field_A0   | Unknown                            |

## WaveformIR (0xE0 bytes) — NEW Phase 5

Extends Waveform base (0xD8 bytes) with 8 bytes of IR-specific fields.

| Offset | Size | Type     | Name      | Notes                              |
|--------|------|----------|-----------|------------------------------------|
| +0x00  | 0xD8 | Waveform | (base)    | Inherited Waveform layout          |
| +0xD8  | 2    | uint16_t | irField1  | IR-specific field                  |
| +0xDA  | 1    | bool     | irBool1   | IR-specific flag                   |
| +0xDC  | 4    | int32_t  | irField2  | IR-specific field                  |

## WaveIndexTracker (0x28 bytes) — NEW Phase 5

Tracks assigned wave indices; auto-fills gaps in implicit assignment.

| Offset | Size | Type         | Name       | Notes                           |
|--------|------|--------------|-----------|---------------------------------|
| +0x00  | 4    | int          | maxIndex_  | Maximum allowed wave index      |
| +0x08  | 24   | std::set<int>| indices_   | Set of assigned indices         |
| +0x20  | 4    | int          | autoIndex_ | Next auto-assigned index        |

## WavetableException (0x20 bytes) — NEW Phase 5

Inherits std::exception. Standard ctor/dtor/what pattern.

| Offset | Size | Type        | Name     | Notes                     |
|--------|------|-------------|----------|---------------------------|
| +0x00  | 8    | vptr        |          | std::exception vtable     |
| +0x08  | 24   | std::string | message_ | Error message             |

## WavetableManager\<T\> (0x48 bytes) — NEW Phase 5

Template class instantiated for WaveformFront and WaveformIR.

| Offset | Size | Type                                | Name          | Notes                           |
|--------|------|-------------------------------------|---------------|---------------------------------|
| +0x00  | 4    | int                                 | lineNr_       | Current line number context     |
| +0x04  | 4    | int                                 | waveformCounter_ | Auto-incrementing counter    |
| +0x08  | 24   | unordered_map\<string,size_t\>      | nameToIndex_  | Name→vector index lookup        |
| +0x30  | 24   | vector\<shared_ptr\<T\>\>           | waveforms_    | Ordered waveform storage        |

## WavetableFront (0x200 bytes) — NEW Phase 5

Front-end waveform table: creation, naming, loading, DIO tracking.

| Offset | Size | Type                                | Name              | Notes                              |
|--------|------|-------------------------------------|-------------------|------------------------------------|
| +0x00  | 8    | DeviceConstants*                    | deviceConstants_  | Device config pointer              |
| +0x08  | 4    | AddressImpl\<uint\>                 | address_          | Base address                       |
| +0x10  | 0x60 | ostringstream                       | oss_              | String stream for formatting       |
| +0x70  | 0x60 | CachedParser                        | cachedParser_     | Tree-based cache (opaque 0x60)     |
| +0xD0  | 48   | map\<size_t,size_t\>                | dioTableUsage_    | DIO table usage tracking           |
| +0x100 | 32   | function\<void(string const&)\>     | warningCallback_  | Warning handler                    |
| +0x120 | 8    | WavetableManager\<WaveformFront\>*  | manager_          | Heap-allocated manager             |
| +0x128 | 0x28 | WaveIndexTracker                    | waveIndexTracker_ | Index tracking                     |
| +0x150 | ?    | (remaining fields)                  |                   | Padding/alignment to 0x200         |

Note: Offsets above 0x150 not fully mapped. Total size 0x200 confirmed from allocation.

## WavetableIR (0xC8 bytes) — NEW Phase 5

IR waveform table: allocation, alignment, FIFO layout, serialization.

| Offset | Size | Type                                        | Name                  | Notes                              |
|--------|------|---------------------------------------------|-----------------------|------------------------------------|
| +0x00  | 8    | DeviceConstants*                            | deviceConstants_      | Device config pointer              |
| +0x08  | 4    | uint32_t                                    | addressBase_          | Base address for allocation        |
| +0x0C  | 4    | uint32_t                                    | firstWaveformOffset_  | Offset of first waveform           |
| +0x10  | 0x60 | CachedParser                                | cachedParser_         | Tree-based cache (opaque 0x60)     |
| +0x70  | 8    | unique_ptr\<WavetableManager\<WaveformIR\>\>| manager_              | Owned manager                      |
| +0x78  | 0x28 | WaveIndexTracker                            | waveIndexTracker_     | Index tracking                     |
| +0xA0  | 24   | vector\<shared_ptr\<WaveformIR\>\>          | usedWaveforms_        | Filtered used waveforms            |
| +0xB8  | 16   | weak_ptr\<CancelCallback\>                  | cancelCallback_       | Cancellation support               |

## AsmOptimize (0xA0 bytes) — NEW Phase 6

Instruction optimizer. Flag-controlled peephole passes plus graph-coloring
register allocator. Operates on a working copy of the AsmList.

| Offset | Size | Type                                | Name              | Notes                              |
|--------|------|-------------------------------------|-------------------|------------------------------------|
| +0x00  | 4    | uint32_t                            | numPhysicalRegs_  | Physical register count (from device) |
| +0x04  | 4    | (padding)                           |                   |                                    |
| +0x08  | 4    | uint32_t                            | flags_            | Optimization pass bitmask          |
| +0x0C  | 4    | (padding)                           |                   |                                    |
| +0x10  | 24   | std::vector\<AsmList::Asm\>         | asm_              | Working copy of instructions       |
| +0x28  | 8    | (padding)                           |                   | Align to 0x30                      |
| +0x30  | 48   | std::function\<void(string,int)\>   | errorCallback_    | Error reporting callback           |
| +0x60  | 48   | std::function\<void(string,int)\>   | warningCallback_  | Warning reporting callback         |
| +0x90  | 16   | shared_ptr\<CancelCallback\>        | cancel_           | Cancellation support               |
| +0xA0  |      | END                                 |                   |                                    |

Optimization flags (bitmask at +0x08):
- 0x01 = oneStepJumpElimination
- 0x02 = removeUnusedLabels + mergeLabels
- 0x04 = deadCodeElimination
- 0x08 = mergeRegisterZeroing
- 0x10 = removeUnusedRegs + registerAllocation

## OptimizeException (0x20 bytes) — NEW Phase 6

Inherits std::exception. Same pattern as ResourcesException and WavetableException.

| Offset | Size | Type        | Name     | Notes                                    |
|--------|------|-------------|----------|------------------------------------------|
| +0x00  | 8    | vptr        |          | std::exception vtable                    |
| +0x08  | 24   | std::string | message_ | If empty, what() returns "Optimize Exception" |

## CompilerMessage (0x20 bytes) — NEW Phase 7a

| Offset | Size | Type                       | Name    | Notes                              |
|--------|------|----------------------------|---------|------------------------------------|
| +0x00  | 4    | CompilerMessageType (enum) | type    | 0=Error, 1=Warning, 2=Info         |
| +0x04  | 4    | int                        | lineNr  | Source line number (-1 = unknown)   |
| +0x08  | 24   | std::string                | message | Error/warning text (SSO)           |

## CompilerMessageCollection (0x20 bytes) — NEW Phase 7a

Inline in Compiler at +0x38. Accumulates errors/warnings with duplicate filtering.

| Offset | Size | Type                          | Name      | Notes                           |
|--------|------|-------------------------------|-----------|---------------------------------|
| +0x00  | 24   | vector\<CompilerMessage\>     | messages_ | Element size 0x20               |
| +0x18  | 4    | int                           | lineNr_   | Current line context            |
| +0x1C  | 1    | bool                          | hadError_ | Set by errorMessage()           |

## CompilerException (0x20 bytes) — NEW Phase 7a

Inherits std::exception. Same pattern as ResourcesException, WavetableException, OptimizeException.

| Offset | Size | Type        | Name     | Notes                              |
|--------|------|-------------|----------|------------------------------------|
| +0x00  | 8    | vptr        |          | std::exception vtable              |
| +0x08  | 24   | std::string | message_ | If empty, what() returns ""        |

## Compiler (0x138 bytes) — NEW Phase 7a

Master compilation orchestrator. Owns parser state, message collection,
assembly list, and shared pointers to major subsystems.

| Offset | Size | Type                          | Name              | Notes                              |
|--------|------|-------------------------------|-------------------|------------------------------------|
| +0x00  | 8    | AWGCompilerConfig const*      | config_           | Stored from ctor arg               |
| +0x08  | 8    | DeviceConstants const*        | deviceConstants_  | Stored from ctor arg               |
| +0x10  | 4    | int32_t                       | lineNr_           | Propagated to AsmCommands+WavetableFront |
| +0x14  | 2    | uint16_t                      | flags_            | Init 0                             |
| +0x16  | 2    | (padding)                     |                   |                                    |
| +0x18  | 8    | (reserved)                    |                   | Zeroed in ctor                     |
| +0x20  | 4    | int32_t                       | channelCount_     | Set by Prefetch::getUsedChannels() |
| +0x24  | 1    | uint8_t                       | channelMode_      | Set by getUsedFourChannelMode()    |
| +0x25  | 1    | uint8_t                       | usedSampleRate_   |                                    |
| +0x26  | 2    | (padding)                     |                   |                                    |
| +0x28  | 16   | shared_ptr\<Node\>            | ast_              | From parse() result                |
| +0x38  | 32   | CompilerMessageCollection     | messages_         | Inline (0x20 bytes)                |
| +0x58  | 24   | vector\<string\>              | sourceFiles_      |                                    |
| +0x70  | 24   | vector\<string\>              | sourceLines_      | Filled by parse()                  |
| +0x88  | 24   | AsmList                       | asmList_          | Working assembly list (0x18)       |
| +0xA0  | 16   | shared_ptr\<WavetableFront\>  | wavetable_        | From ctor arg                      |
| +0xB0  | 16   | shared_ptr\<AsmCommands\>     | asmCommands_      | Created in ctor (alloc 0x80)       |
| +0xC0  | 16   | shared_ptr\<WaveformGenerator\>| waveformGen_     | Created in ctor (alloc 0xE0)       |
| +0xD0  | 16   | shared_ptr\<CustomFunctions\> | customFunctions_  | Created in ctor (alloc 0x200)      |
| +0xE0  | 16   | weak_ptr\<CancelCallback\>    | cancelCallback_   | Set via setCancelCallback()        |
| +0xF0  | 16   | weak_ptr\<ProgressCallback\>  | progressCallback_ | Set via setProgressCallback()      |
| +0x100 | 56   | SeqcParserContext             | parserContext_    | flex/bison state + error callback  |
| +0x138 |      | END                           |                   |                                    |

## Cache (0x28 = 40 bytes) — NEW Phase 7b

Waveform cache memory manager. Tracks allocated waveform pointers in sorted
order by position. Supports append mode (always place at end) or best-fit
gap-finding allocation.

| Offset | Size | Type                              | Name        | Notes                              |
|--------|------|-----------------------------------|-------------|------------------------------------|
| +0x00  | 4    | uint32_t                          | size_       | Total cache size (AddressImpl)     |
| +0x04  | 4    | int32_t                           | pageSize_   | Alignment/page size                |
| +0x08  | 1    | bool                              | appendMode_ | If true, always append at end      |
| +0x09  | 7    | (padding)                         |             |                                    |
| +0x10  | 24   | vector\<shared_ptr\<Pointer\>\>   | pointers_   | Sorted by position                 |
| +0x28  |      | END                               |             |                                    |

## Cache::Pointer (0x24 = 36 bytes) — NEW Phase 7b

Individual cache allocation entry. Tracks position, size, waveform reference,
and playback state.

| Offset | Size | Type                      | Name        | Notes                              |
|--------|------|---------------------------|-------------|------------------------------------|
| +0x00  | 4    | uint32_t                  | position_   | Start address (AddressImpl)        |
| +0x04  | 4    | uint32_t                  | size_       | Allocated samples                  |
| +0x08  | 4    | uint32_t                  | hash_       | ~(position ^ (position + size/2))  |
| +0x0C  | 4    | uint32_t                  | numRepeats_ | numSamples / halfSize + 1          |
| +0x10  | 16   | shared_ptr\<WaveformIR\>  | waveform_   | Associated waveform                |
| +0x20  | 4    | PointerState (int)        | state_      | 0=Ready,1=LastPlayed,2=Playing,3=Free |
| +0x24  |      | END                       |             |                                    |

## CacheException (0x20 bytes) — NEW Phase 7b

Inherits std::exception. Same pattern as other exception types.

| Offset | Size | Type        | Name     | Notes                              |
|--------|------|-------------|----------|------------------------------------|
| +0x00  | 8    | vptr        |          | std::exception vtable              |
| +0x08  | 24   | std::string | message_ | If empty, what() returns ""        |

## Prefetch (0x160 = 352 bytes) — NEW Phase 7c

Waveform prefetch manager. Manages cache allocation, load placement, and
CWVF optimization for the sequencer pipeline.

| Offset | Size | Type                                              | Name             | Notes                              |
|--------|------|---------------------------------------------------|------------------|------------------------------------|
| +0x00  | 8    | AWGCompilerConfig const*                          | config_          |                                    |
| +0x08  | 8    | DeviceConstants const*                            | devConst_        |                                    |
| +0x10  | 0x28 | unordered_map\<shared_ptr\<Node\>, PNS\>         | nodeStates_      | PNS = PrefetcherNodeState          |
| +0x38  | 0x28 | unordered_map\<string, bool\>                    | nameMap_         | Wave name → reusable flag          |
| +0x60  | 0x10 | shared_ptr\<Node\>                                | root_            |                                    |
| +0x70  | 0x10 | shared_ptr\<AsmCommands\>                         | asmCommands_     |                                    |
| +0x80  | 0x10 | shared_ptr\<Resources\>                           | resources_       | Initially null                     |
| +0x90  | 0x10 | shared_ptr\<Cache\>                               | cache_           | Created in ctor                    |
| +0xA0  | 0x18 | vector\<bimap\<optional\<string\>,ulong\>\>       | waveformMaps_    | N = numChannelGroups               |
| +0xB8  | 4    | int32_t                                           | maxBranches_     | Init 1, max branch depth           |
| +0xBC  | 1    | bool                                              | split_           | Init false                         |
| +0xBD  | 3    | (padding)                                         |                  |                                    |
| +0xC0  | 0x20 | PlayConfig                                        | cwvfConfig_      | Init: channelMask=-1, rest=0       |
| +0xE0  | 0x18 | vector\<UsageEntry\>                              | usageEntries_    | 0x20-byte elements                 |
| +0xF8  | 0x10 | shared_ptr\<Node\>                                | lastCwvfNode_    | Last node seen in globalCwvf       |
| +0x108 | 1    | bool                                              | globalCwvfValid_ |                                    |
| +0x109 | 7    | (padding)                                         |                  |                                    |
| +0x110 | 0x10 | shared_ptr\<WavetableIR\>                         | wavetableIR_     |                                    |
| +0x120 | 0x28 | function\<void(string const&)\>                   | logFunc_         |                                    |
| +0x148 | 8    | (padding)                                         |                  |                                    |
| +0x150 | 0x10 | weak_ptr\<CancelCallback\>                        | cancelCb_        |                                    |
| +0x160 |      | END                                               |                  |                                    |

## PrefetcherNodeState — NEW Phase 7c

Per-node state stored in `nodeStates_` unordered_map.

| Offset | Size | Type                          | Name             | Notes                              |
|--------|------|-------------------------------|------------------|------------------------------------|
| +0x00  | 8    | AsmRegister                   | registerHirzel   | Set when isHirzel                  |
| +0x08  | 8    | AsmRegister                   | registerCervino  | Set when !isHirzel                 |
| +0x10  | 4    | int                           | counter          | Load reference count               |
| +0x14  | 4    | (padding)                     |                  |                                    |
| +0x18  | 4    | int                           | refTrack         | Shared-load ref tracking           |
| +0x1C  | 4    | int                           | pageSize         | Waveform page division             |
| +0x20  | 4    | int                           | usedCache        | Cache bytes used                   |
| +0x24  | 4    | (padding)                     |                  |                                    |
| +0x28  | 0x10 | shared_ptr\<Cache::Pointer\>  | cachePtr         | Cache allocation                   |
| +0x38  | 1    | bool                          | useDA            | From devConst_->useDA              |

## MemoryBlock (0x0C = 12 bytes) — NEW Phase 8b

| Offset | Size | Type     | Name  | Notes                                          |
|--------|------|----------|-------|-------------------------------------------------|
| +0x00  | 4    | uint32_t | start | Start address of block                          |
| +0x04  | 4    | uint32_t | end   | End address (start + size)                      |
| +0x08  | 4    | uint32_t | flags | bit 0=valid, bit 8=crossesCacheLine             |

Return convention: `rax = start | (end << 32)`, `dl = flags`.
Deque page size = 341 elements (4092 bytes per page = 0xFFC).

## MemoryAllocator (0x70 = 112 bytes) — NEW Phase 8b

Non-virtual class. Constructor always inlined at call sites.
Used by WavetableIR::allocateWaveforms and allocateWaveformsForFifo.

| Offset | Size | Type              | Name                | Notes                              |
|--------|------|-------------------|---------------------|------------------------------------|
| +0x00  | 8    | DeviceConstants*  | deviceConstants_    | Source of memory geometry           |
| +0x08  | 4    | uint32_t          | startOffset_        | Where allocation begins            |
| +0x0C  | 4    | uint32_t          | lastAllocEnd_       | 0xFFFFFFFF sentinel initially      |
| +0x10  | 4    | uint32_t          | memorySizeInSamples_| DC->waveformMemorySize (+0x0C)     |
| +0x14  | 4    | uint32_t          | cacheLineSize_      | DC->waveformAlignment (+0x14)      |
| +0x18  | 4    | uint32_t          | maxBlocksPerCL_     | DC->cachePageCount or maxBlocks    |
| +0x1C  | 4    | (padding)         |                     |                                    |
| +0x20  | 24   | vector\<uint32_t\>| cacheLineUsage_     | Per-CL owner; 0xFFFFFFFF=free      |
| +0x38  | 4    | uint32_t          | numCacheLines_      | memorySizeInSamples / cacheLineSize|
| +0x3C  | 4    | (padding)         |                     |                                    |
| +0x40  | 48   | deque\<MemoryBlock\>| freeBlocks_       | Free block list (341 per page)     |

## RawWave (abstract base, 0x08 bytes) — NEW Phase 8d

| Offset | Size | Type | Name | Notes |
|--------|------|------|------|-------|
| +0x00  | 8    | vptr |      | vtable: dtor D1, dtor D0, size(), ptr() |

Base typeinfo @0xb07800. Pure virtual: size() and ptr().

## RawWavePlaceHolder (0x28 = 40 bytes) — NEW Phase 8d

| Offset | Size | Type              | Name      | Notes                              |
|--------|------|-------------------|-----------|-------------------------------------|
| +0x00  | 8    | vptr              |           | vtable @0xb077c8                    |
| +0x08  | 8    | size_t            | byteSize_ | channels * length * 2               |
| +0x10  | 24   | vector\<char\>    | buffer_   | Lazily allocated in ptr() (mutable) |

## RawWaveHirzel16 (0x20 = 32 bytes) — NEW Phase 8d

| Offset | Size | Type                | Name  | Notes                              |
|--------|------|---------------------|-------|-------------------------------------|
| +0x00  | 8    | vptr                |       | vtable @0xb07820                    |
| +0x08  | 24   | vector\<uint16_t\>  | data_ | Encoded samples via double2awg*     |

Ctor @0x297140: scans MarkerBitsPerChannel (OR all & 0x03 via SSE2):
0→double2awg16, 1→double2awg1m, >1→double2awg.

## RawWaveCervino (0x20 = 32 bytes) — NEW Phase 8d

| Offset | Size | Type                | Name  | Notes                              |
|--------|------|---------------------|-------|-------------------------------------|
| +0x00  | 8    | vptr                |       | vtable @0xb07868                    |
| +0x08  | 24   | vector\<uint16_t\>  | data_ | Encoded via double2awg (inline ctor)|
