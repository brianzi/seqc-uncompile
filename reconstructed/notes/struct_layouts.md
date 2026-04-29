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

## Assembler (0x80 = 128 bytes) — CORRECTED Phase 2

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
| +0x08  | 0x80 | Assembler          | assembler       |                          |
| +0x88  | 4    | int                | wavetableFront  |                          |
| +0x8C  | 4    | (padding)          |                 |                          |
| +0x90  | 16   | shared_ptr<Node>   | node            | May be null              |
| +0xA0  | 1    | bool               | noOpt           | (cmd-3) < 3u             |
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
| +0x20  | 24   | std::string         | functionArgs    | "functionArgs"     | function arguments string       |
| +0x38  | 16   | shared_ptr\<File\>  | file            | "fileName"         | source file reference           |
| +0x48  | 1    | bool                | used            | "load"             | set by asmPlay/asmPrefetch      |
| +0x49  | 3    | (padding)           |                 |                    |                                 |
| +0x4C  | 4    | uint32_t            | addressValue    | "globalAddress"    | AddressImpl\<uint\>             |
| +0x50  | 24   | std::string         | funDescrName     | "genFunc"          | generator function name         |
| +0x68  | 4    | uint32_t            | playConfig      | "playConfig"       | packed PlayConfig               |
| +0x6C  | 4    | int32_t             | playIndex       | "waveIndex"        | -1 = compute playConfig         |
| +0x70  | 4    | int                 | minLengthSamples| "minLengthSamples" | from DeviceConstants+0x40       |
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
| +0xD8  | 4    | int            | useCount_ | init=1 in copy ctor             |
| +0xDC  | 1    | bool           | dirty_  | init=0 in copy ctor             |
| +0xDD  | 1    | bool           | hasDuplicate_  | copied from source              |
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
| +0x40  | 4    | uint32_t      | waveformGranularity | Waveform page cap. Also WaveformFront.minLengthSamples |
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
| +0x14  | 4    | uint16_t[2]           | channelsPerGroup  | [0]=normal, [1]=indexed; read by PlayArgs ctor |
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

## AsmExpression (0xa8-byte parse tree node) — Revised Phase 10c

Reconstructed from dtor @0x28b1f0 + consumer analysis. No vtable.
Allocated via `make_shared` (0xC0 bytes total including 0x18-byte control block).

| Offset | Size | Type                                      | Name       | Notes                              |
|--------|------|-------------------------------------------|------------|------------------------------------|
| +0x00  | 4    | int                                       | type       | 1=register, 2=label/name, 3=integer |
| +0x04  | 4    | (padding)                                 |            |                                    |
| +0x08  | 24   | std::string                               | name       | Command/register/label name        |
| +0x20  | 24   | std::string                               | str2       | Secondary string (NOP comment)     |
| +0x38  | 4    | uint32_t (Assembler::Command)             | command    | Instruction command enum           |
| +0x3C  | 4    | int32_t                                   | value      | Reg# (type==1) or imm (type==3)   |
| +0x40  | 24   | vector\<shared_ptr\<AsmExpression\>\>     | children   | Operand sub-expressions            |
| +0x58  | 1    | uint8_t                                   | pad_58     | Zeroed spacer                      |
| +0x5C  | 4    | int32_t                                   | labelPc    | Label program counter              |
| +0x60  | 24   | std::string                               | labelName  | Label identifier                   |
| +0x78  | 1    | bool                                      | hasLabel   | Guards labelName (optional-like)   |
| +0x80  | 24   | std::string                               | comment    | JSON blob / comment text           |
| +0x98  | 1    | bool                                      | hasComment | Guards comment ("noOpt" flag)      |
| +0xA0  | 1    | bool                                      | noOptOverride_           | noOpt override                     |

Corrections from Phase 4 version:
- +0x20 is `str2` (secondary string), not `comment` — comment is at +0x80
- +0x58 is NOT lineNumber — it's a zeroed spacer byte
- +0x78 is `hasLabel` (bool guard), not `labelType`
- +0x98 is `hasComment` (bool guard for +0x80 string), was called `noOpt`
- Optional fields use manual bool guards after the string (matching std::optional layout)

## WaveformIR (0xE0 bytes) — NEW Phase 5

Extends Waveform base (0xD8 bytes) with 8 bytes of IR-specific fields.

| Offset | Size | Type     | Name      | Notes                              |
|--------|------|----------|-----------|------------------------------------|
| +0x00  | 0xD8 | Waveform | (base)    | Inherited Waveform layout          |
| +0xD8  | 2    | uint16_t | irField1  | IR-specific field                  |
| +0xDA  | 1    | bool     | crossesCacheLine_ | true for fillers / when block crosses a CL |
| +0xDC  | 4    | int32_t  | elfAlignment_  | IR-specific field                  |

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
| +0x08  | 4    | uint32_t                            | optFlags_         | Optimization pass bitmask          |
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
| +0x24  | 1    | uint8_t                       | usedFourChannelMode_ | Set by getUsedFourChannelMode()    |
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
| +0x08  | 1    | bool                              | isHirzel_   | If true, always append at end      |
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

## ElfWriter (0x78 = 120 bytes) — NEW Phase 9a

Inherits from ELFIO::elfio (0x70 bytes). No vtable of its own.

| Offset | Size | Type                | Name            | Notes                              |
|--------|------|---------------------|-----------------|------------------------------------|
| +0x00  | 8    | void*               | sections_self_  | ELFIO: stores 'this' (Sections proxy) |
| +0x08  | 8    | void*               | segments_self_  | ELFIO: stores 'this' (Segments proxy) |
| +0x10  | 8    | elf_header*         | header_         | ELFIO: elf_header_impl\<Elf32_Ehdr\> |
| +0x18  | 24   | vector\<section*\>  | sections_       | ELFIO: section pointer vector      |
| +0x30  | 24   | vector\<segment*\>  | segments_       | ELFIO: segment pointer vector      |
| +0x48  | 1    | bool                | converter_      | ELFIO: endian converter flag       |
| +0x49  | 7    | (padding)           |                 |                                    |
| +0x50  | 24   | vector\<char\>      | (internal)      | ELFIO: internal string buffer      |
| +0x68  | 8    | uint64_t            | (layout_off)    | ELFIO: layout offset tracker       |
| +0x70  | 8    | uint64_t            | memoryOffset_   | Entry point / segment base addr    |

Ctor @0x2934a0: creates ELFIO 32-bit LE header (ELF magic, ELFCLASS32, ELFDATA2LSB,
e_ehsize=0x34, e_shentsize=0x28, os_abi=1), then prepareHeader(machineType).
setMemoryOffset @0x294410: trivial store to +0x70.

ELF sections created:
- ".text": SHT_PROGBITS, SHF_ALLOC|SHF_EXECINSTR, align=64
- ".dd\_\<name\>": SHT_PROGBITS, SHF_ALLOC, descriptor/padding data
- ".wf\_\<name\>": SHT_PROGBITS (or SHT_NOBITS for reserveOnly), SHF_ALLOC, waveform data
- ".shstrtab": SHT_STRTAB (mandatory, created by ELFIO)

Segments: PT_LOAD for code (PF_R|PF_X, align=64) and waveforms (PF_W, alignment from DeviceConstants).

## AsmParserContext (~0x80+ bytes) — NEW Phase 9b

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x00  | 1    | bool | isComment_       | Zeroed as DWORD with next 3 bools |
| +0x01  | 1    | bool | isLineComment_   | Mutually exclusive with block |
| +0x02  | 1    | bool | isBlockComment_  | Mutually exclusive with line |
| +0x03  | 1    | bool | hadSyntaxError_  | Set by asmerror() |
| +0x04  | 1    | bool | doOpt_           | Enable/disable optimization |
| +0x05  | 3    | pad  |                  | |
| +0x08  | 4    | int  | lineNumber_      | Reset to 1 by clearSyntaxError |
| +0x0C  | 4    | int  | programCounter_  | Incremented per instruction |
| +0x10  | 0x30 | std::function | errorCallback_ | void(int, string const&) |
| +0x40  | 0x18 | vector\<char*\> | stringCopies_ | strdup'd strings for parser |
| +0x58  | ~0x28| unordered_set\<string\> | labels_ | Label dedup for addCommand |

### AsmParserContext::Label (0x20 bytes)

| Offset | Size | Type | Field |
|--------|------|------|-------|
| +0x00  | 4    | int  | pc    |
| +0x08  | 0x18 | string | name |

22 accessor methods @0x28e7a0–0x28ead0. Free functions: addNode @0x28bfd0,
addCommand @0x28c600, asmerror @0x292a60. asmparse @0x292b50 is ~217KB
bison-generated — deferred.

## Resources (0xD8 = 216 bytes) — Phase 10a/10b

Base class for scope/symbol management. Inherits enable_shared_from_this.
vtable @0xb04e38. Single virtual: getVariable().

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x00  | 8    | vptr | vtable |  |
| +0x08  | 16   | weak_ptr | (enable_shared_from_this) | Hidden base member |
| +0x18  | 16   | shared_ptr\<Resources\> | grandparent_ |  |
| +0x28  | 24   | std::string | name_ | "static", "global", etc. |
| +0x40  | 16   | weak_ptr\<Resources\> | parent_ |  |
| +0x50  | 4    | int32_t | state_ | State enum: 0=Unset,1=Active,2=Paused,3=Locked |
| +0x54  | 4    | VarType | returnType_ |  |
| +0x58  | 0x28 | Value | returnValue_ |  |
| +0x80  | 8    | AsmRegister | returnReg_ |  |
| +0x88  | 2    | int16_t | flags_88_ |  |
| +0x90  | 24   | vector\<Variable\> | variables_ |  |
| +0xA8  | 24   | vector\<shared_ptr\<Function\>\> | functions_ |  |
| +0xC0  | 24   | vector\<shared_ptr\<Resources\>\> | children_ |  |

### Resources::Variable (0x58 = 88 bytes)

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x00  | 8    | int64_t | varType | VarType as int64 (2=var,3=const,...) |
| +0x08  | 4    | int32_t | which_ | boost::variant discriminator |
| +0x0C  | 4    | (pad) |  |  |
| +0x10  | 24   | union | variant data | boost::variant\<int,bool,double,string\> |
| +0x28  | 8    | (pad) |  |  |
| +0x30  | 8    | AsmRegister | reg | {value, valid} |
| +0x38  | 24   | std::string | name |  |
| +0x50  | 2    | int16_t | flags | 1 = written/set |
| +0x52  | 6    | (pad) |  |  |

### Resources::Function (0x78 = 120 bytes)

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x00  | 8    | void* | pad_00_ | zeroed in ctor |
| +0x08  | 8    | __shared_weak_count* | weakRef_ | released in dtor |
| +0x10  | 24   | std::string | name |  |
| +0x28  | 24   | std::string | signature |  |
| +0x40  | 4    | VarType | returnType |  |
| +0x44  | 4    | (pad) |  |  |
| +0x48  | 24   | vector\<Variable\> | arguments |  |
| +0x60  | 16   | shared_ptr\<Resources\> | scope |  |
| +0x70  | 8    | unique_ptr\<SeqCAstNode\> | body | freed via vtable+0x30 |

## StaticResources (0x110 = 272 bytes) — Phase 10a

Extends Resources. Overrides getVariable() @0x129e60.
init() @0x1ec8f0 populates built-in constants (AWG_RATE_*, integration, etc.)

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| +0x00  | 0xD8 | Resources | (base) |  |
| +0xD8  | 1    | bool | usedSampleRate_ |  |
| +0xE0  | 0x30 | std::function | (init callback) |  |

## GlobalResources (0xD8 = 216 bytes) — Phase 10a

Same size as Resources — no instance fields. Uses TLS statics:
- regNumber (TLS+0x48, int32_t, init=1)
- labelIndex (TLS+0x4c, int32_t, init=0)
- random (TLS+0x50, MT19937-64 with seed 0x1571)

## readConst/readString/readWave/readCvar return type — Phase 18b-iii

Previously declared as `void`. Actually return `EvalResultValue` by value (sret convention).
The hidden sret pointer in `rdi` pushes `this` to `rsi`. The demangler doesn't show
the return type, which is why they appeared void.

Call pattern at call sites:
```
lea  rdi, [rbp-0x108]     ; sret buffer
mov  rsi, [r15]            ; Resources* (from shared_ptr)
lea  rdx, [rbp-0x48]       ; string const& name
mov  ecx, 0x1              ; EDirection = Write
call readConst             ; @0x1e7d70
; caller reads [rbp-0x108] directly as EvalResultValue
```

Inside readConst, a virtual call `[vtable+0x10]` on the Resources object performs
the actual constant lookup (vtable slot 2 = getVariable override on StaticResources).

## User register address map (CustomFunctions builtins)

| Address | Purpose | Used by |
|---------|---------|---------|
| 0x1b | Timestamp | waitTimestamp |
| 0x1c | Now value | now() |
| 0x1d | AT trigger | at() |
| 0x61 | QA result | getQAResult |
| 0x62/0x6d | RT logger (device-dep) | resetRTLoggerTimestamp |
| 0x69 | Wait-cycles user reg | wait() |
| 0x74 | PRNG seed | setPRNGSeed |
| 0x75 | PRNG range offset | setPRNGRange |
| 0x76 | PRNG range span | setPRNGRange |
| 0x77 | PRNG value read | getPRNGValue |
| 0x78 | QA gen+int config | startQA |
| 0x79 | QA gen+int config (2) | startQA |
| 0x7a | QA result length (UHFQA) | startQA |
| 0x8c | Sweep osc index | setSweepStep, setOscFreq |
| 0x8d | Sweep step/freq mode | setSweepStep, setOscFreq |
| 0x8e-0x8f | LS64bit freq data | setOscFreq |
| 0x92 | Sync trigger | waitOnSync |

## EvalResults setValue family — common pattern

All `EvalResults::setValue(...)` overloads share the same implementation
shape, differing only in which fields of the synthesized
`EvalResultValue` they populate:

1. Build `EvalResultValue` on stack (varType, varSubType=0 unless
   explicit, value default or copied from arg, reg = AsmRegister(-1)
   unless an `int` is passed in which case `AsmRegister(int).valid=true`).
2. `operator new(0x38)` for the new vector buffer.
3. Copy fields into the freshly-allocated element (with the Value's
   variant `which_` jump table, identical across all overloads — this
   is the inlined `Value` copy ctor).
4. Walk old `values_` buffer back-to-front, freeing any heap strings
   in the embedded `Value` (SSO check at `[elem+0x10]` byte 0 == 1),
   then `operator delete` the buffer.
5. Overwrite values_'s {begin,end,cap} pointers with the new buffer.

Confirmed across @0x211b70, @0x16bfb0, @0x15c850, @0x20ad20, @0x2107b0,
@0x247600. The 5 overloads that don't take a Value also skip the Value
copy block but still emit the same buffer-replacement tail.

The VarType-taking ctor @0x176bc0 uses the same pattern but goes
through `vector<>::vector(size_t, T const&)` rather than direct
allocation, since it's constructing rather than reassigning.

`setValue(double) @0x2136a0` hard-codes the EvalResultValue varType_ to
the literal `4`. Under the **corrected VarType mapping** established in
Phase 19c-followup (Finding 1), 4 = `VarType_Const` — so this remains the
"setValue with a built-in VarType" (the other overloads take it as a
param). The disasm also writes 3 into subType (= VarSubType_Vect).

---

## VarType enum — corrected mapping (Phase 19c-followup, Finding 1)

The `str(VarType) @0x247dd0` jump table at .rodata 0x95c2a0 gives the
canonical mapping:

| numeric | label              | string returned |
|---------|--------------------|-----------------|
|   0     | `VarType_Unset`    | "notype" (default branch) |
|   1     | `VarType_Void`     | "void" |
|   2     | `VarType_Var`      | "var" |
|   3     | `VarType_String`   | "string" |
|   4     | `VarType_Const`    | "const" |
|   5     | `VarType_Wave`     | "wave" |
|   6     | `VarType_Cvar`     | "cvar" |

Cross-checked against record-tag writes in addX overloads:

| family  | full add tag | stub add tag | matches enum |
|---------|--------------|--------------|--------------|
| Var     | 2 (only stub)| 2            | ✓ Var=2 |
| String  | 3            | 3            | ✓ String=3 |
| Const   | 4            | 4            | ✓ Const=4 |
| Wave    | 5            | 5            | ✓ Wave=5 |
| Cvar    | 6            | 6            | ✓ Cvar=6 |

The earlier reconstruction had Const=3 / Cvar=4 / String=5 / Wave=6,
which produced the spurious "tag 4 vs Const=3 discrepancy" noted in
Phase 19b. There is **no** separate "record tag" enum; `Variable+0x00`
IS the VarType directly.

---

## Variable layout — final (Phase 20e-ii Batch 5a wrap-up cleanup)

Size 0x58 (88 bytes), confirmed by `add r14, 0x58` at 1e8441 and stride
of vector iterations.

| offset | size | field           | notes |
|--------|------|-----------------|-------|
| +0x00  | 4    | `type` (VarType) | low 32 of an 8B-aligned slot; pad +0x04 is `subTypeRaw` (next row) |
| +0x04  | 4    | `subTypeRaw` (VarSubType) | caller-supplied `st` arg (read by getVariableSubType @0x1e4580) |
| +0x08  | 40   | **`value` (embedded Value)** | full `Value` object: `type_` (+0x00), `which_` (+0x08), `storage_` (+0x10..+0x27) |
| +0x30  | 8    | `reg` (AsmRegister) | |
| +0x38  | 24   | `name` (std::string SSO) | |
| +0x50  | 2    | `flags` (int16_t) | low byte: "set"/written; high byte (+0x51): "frozen" parameter (Function::addArgument sets this) |
| +0x52  | 6    | (padding) | |

The 40-byte block at +0x08 is **a complete embedded `Value` object**.
Evidence: `readString @0x1e5db5` does `add rsi, 0x8; call Value::toString()`,
passing `&v+0x8` as `this` to a Value method. Earlier reconstructions
modeled the block as raw scalar fields (`flagWord`, `which_`,
`variantStorage`, `pad_28`); these are now consolidated into a single
`Value value` member that aligns with the 40-byte (0x28) Value layout
in `value.hpp`.

### Hardcoded `value.type_` literals per add* path

The per-add* literals previously called "flagWord secondary tag" are
just `Value::type_` (`ValueType` enum) values written to set up a
type-consistent default Value:

| add* path                           | value.type_ | value.which_ | value.storage_         |
|-------------------------------------|-------------|--------------|------------------------|
| addVar (stub)                       | Int=1       | 0            | i = 0                  |
| addConst (stub, @0x1e74e0)          | Int=1       | 0            | i = 0                  |
| addCvar (stub, @0x1e8650)           | Int=1       | 0            | i = 0                  |
| addString (stub, @0x1e54f0)         | String=4    | 3            | empty std::string      |
| addWave (stub, @0x1e64f0)           | String=4    | 3            | empty std::string      |
| addConst(double, @0x1e7010)         | Double=3    | 2            | d = val                |
| addCvar(double, @0x1e8180)          | Double=3    | 2            | d = val                |
| addString(name, val, @0x1e5020)     | String=4    | 3            | string-SSO from val    |
| addWave(name, val, @0x1e6020)       | String=4    | 3            | string-SSO from val    |

Note: the addString/addWave **stub** overloads construct an empty
std::string in the Value variant (via boost::variant_assign), unlike the
addConst/addCvar/addVar stubs which leave the default-Int payload.
Confirmed by disasm at @0x1e5667 (addString stub) and @0x1e6667 (addWave
stub) which call `boost::variant::variant_assign` to install a string
variant.

`Variable::~Variable @0x1e4be0`: defaulted in source. The `Value`
subobject's destructor handles its own variant-payload cleanup
(long-form string at +0x18 of Value when `abs(which_) >= 3`); the
`name` string at +0x38 of Variable is destroyed automatically.

### VarSubType — partial mapping
| numeric | label                | source path |
|---------|----------------------|-------------|
|   0     | `VarSubType_Default` | StaticResources general-constant init |
|   1     | `VarSubType_Stub`    | addVar; bare-stub addX(name, st) overloads |
|   2     | `VarSubType_FunctionArg` | Function::addArgument (parameter binding); also pre-marks +0x50 = 1 and sets +0x51 frozen-byte |
|   3     | `VarSubType_Vect` | full addConst, full addCvar |
|   4     | `VarSubType_String`  | full addString, full addWave |

(Value 2 confirmed in Phase 20e-ii Sub-phase 5b: `VarSubType_FunctionArg`
is passed by `Function::addArgument @0x1e9f60` to all 5 `scope->add*`
calls. Values ≥5 not yet observed.)

## Resources function-operation methods — Batch 6 (Phase 20e-ii)

Return type corrections discovered during disassembly:

| Method | Old decl | Corrected | Evidence |
|--------|---------|-----------|----------|
| `getPossibleFunctions` | `void` | `vector<string>` | sret via rdi @0x1e9740; vector built with name+sig concatenations |
| `addFunction` | `void` | `shared_ptr<Function>` | sret via rdi @0x1e9c10; returns `functions_.back()` after push_back |
| `getRegister` | `void` | `int` | scalar return in eax @0x1eba50; AsmRegister::operator int() |

### getRegister auto-allocation (@0x1eba50)

When a variable's AsmRegister is invalid or equals `AsmRegister(0)`,
`getRegister` auto-allocates from `GlobalResources::regNumber` (TLS +0x48).
Pattern: read TLS regNumber, post-increment, construct `AsmRegister(old)`,
write to `var->reg`. This is the only observed call site that writes to
`GlobalResources::regNumber`.

### functionExists two-phase lookup (@0x1e9110)

- If `sig` is non-empty: delegates to `getFunction(name, sig)` and returns
  non-null check (full name+sig match via `sameArgString`).
- If `sig` is empty: name-only local scan (no `sameArgString`), then
  recurses to parent via `parent_.lock()`.

### addFunction shared_from_this pattern (@0x1e9c10)

Calls `shared_from_this()` on `this` (reads ESFT weak_ptr at +0x08/+0x10)
to create a `weak_ptr<Resources>` passed to `Function(name, sig, rt, weak)`.
Throws `AlreadyDefined` (0xab=171) if `functionExistsInScope` returns true.

## StaticResources / GlobalResources — Batch 7 (Phase 20e-ii)

### StaticResources ctor (@0x129cb0)

- Constructs Resources base with name `"static"` and empty `weak_ptr` parent.
- `usedSampleRate_` (+0xD8) initialized to `false`.
- Copies `std::function<void(string const&)> logger` parameter via libc++
  internal clone mechanism (modeled as placement-new in source).

### StaticResources::getVariable (@0x129e60)

- **Pre-check**: `name == "DEVICE_SAMPLE_RATE"` (18-byte SSE compare) →
  sets `usedSampleRate_ = true`.
- **Delegates** to `Resources::getVariable(name)`.
- **Deprecation warnings** (5 constant names, non-blocking — still returns
  the Variable*):
  - `"AWG_MONITOR_TRIGGER"` (19B SSE), `constAwgIntegrationTrigger` (bcmp),
    `"AWG_INTEGRATION_ARM"` (19B SSE) → hint: `"'startQA' function"`
  - `zsyncDataPqscRegister` (bcmp) → hint: `"'ZSYNC_DATA_PROCESSED_A'"`
  - `zsyncDataPqscDecoder` (bcmp) → hint: `"'ZSYNC_DATA_PROCESSED_B'"`
- Error code: `DeprecatedConst = 52` (0x34).
- SSE "overlap trick" for 19-byte strings: `pcmpeqb` bytes[0..15] and
  bytes[3..18], `pand`, `pmovmskb == 0xffff`.

### GlobalResources ctor (@0x12a710)

- Constructs Resources base with name `"global"` and empty `weak_ptr` parent.
- Copies `parent` shared_ptr into `parent_` (+0x18).
- Initializes TLS: `regNumber = 1`, `labelIndex = 0`.
- Seeds MT19937-64: `random[0] = 0x1571`, multiplier `0x5851f42d4c957f2d`,
  loop 311 iterations, `random[312] = 0` (mti index).

### GlobalResources dtor (@0x12ab40)

- Trivial: calls `Resources::~Resources()` then `operator delete(this, 0xd8)`.
  Defaulted in reconstructed source.

## PlayArgs::WaveAssignment (0x50 = 80 bytes) — CORRECTED Phase 21a

Earlier reconstruction described this as
`{ int type; int subType; EvalResultValue value; vector<int> bits; }`
which sums to 0x58 and contradicts the 0x50 stride observed in the
binary's `vector<WaveAssignment>` operations. The correct layout starts
with the EvalResultValue directly:

| Offset | Size | Type / Field             | Notes                                                       |
|--------|------|--------------------------|-------------------------------------------------------------|
| +0x00  | 0x38 | EvalResultValue value    | First 8B = `varType_` + `varSubType_` (the historical "type"/"subType") |
| +0x38  | 0x18 | vector\<int\> bits       | Channel bit assignments (port indices 1..N)                |
| total  | 0x50 |                          | static_assert in `custom_functions.hpp:194`                |

Evidence:
- `@0x135854` (playAuxWave): `lea rsi, [rbx+0x8]` then `call Value::toString`
  ⇒ `wa.value.value_` lives at `WA+0x08` ⇒ `wa.value` lives at `WA+0`.
- `@0x135990` (playAuxWave inner copy): copies exactly 0x38 bytes from
  `WA+0` into a `vector<EvalResultValue>` slot.

**Cascading fix** (Phase 21a Sub-batch 5): 8 references in
`custom_functions.cpp` updated from `wa.type`/`wa.subType` to
`wa.value.varType_`/`wa.value.varSubType_`.

## Play wrappers — signature & dispatch matrix (Phase 21a)

All 5 wrappers have signature
`shared_ptr<EvalResults> name(vector<EvalResultValue> const& args, shared_ptr<Resources> res)`
and follow a common skeleton (externalTriggeringMode_ check → arg-count guard →
PlayArgs construction → asm emission). Distinguishing parameters:

| Wrapper        | Addr     | extTrigMode | arg-count guard | PlayArgs ctor    | parseOptionalRate | Rate-guard error | Empty-args error |
|----------------|----------|------------|-----------------|------------------|-------------------|------------------|------------------|
| playWaveDIO    | 0x137740 | check `==1` | `args.empty()` | n/a (direct wvft) | n/a               | n/a              | 0x42             |
| playWaveZSync  | 0x137a50 | check `==2` | `args.size()==1`| n/a (readConst)  | n/a               | n/a              | 0x5c             |
| playDIOWave    | 0x1369f0 | none       | `args.empty()` | `indexed=false`  | `strict=false`    | 0xa1 (≤1)        | 0x3d             |
| playAuxWave    | 0x135610 | none       | (tolerant)     | `indexed=true`   | `strict=true`     | 0xa0 (≤4)        | (handled by parse)|
| playAuxWaveIdx | 0x136930 | (existing — Phase 18b) | | | | | |

Key per-wrapper details:
- **playWaveZSync**: Single Const arg matched against 3 readConst names
  → bit-shift `(1, 9, 0xd)`. Mask formula is `shift << numOutputPorts`
  (multi-bit pattern preserved into high mask).
- **playDIOWave**: Per-bit trigger mask clearing
  `mask &= ~(0x40 << (7*b))` for each `b in WaveAssignment::bits`.
- **playAuxWave**: Channel scatter (`wa.value → channelArgs[bits[i]-1]`),
  zero-fill empty slots via `waveformGen_->call("zeros", {Value(len)})`,
  constant trigger mask (0x3FFF or 0x3FC3), `asmPlay isHoldMode=true`.

## mergeWaveforms (0x15e060) — 6-arg semantics (Phase 21a Sub-batch 3)

Signature confirmed via mangled tail `...sbRK<string>ib` (last param
is `bool`, not `int64_t`):

```
shared_ptr<WaveformFront> mergeWaveforms(
    vector<EvalResultValue> const& args,
    int                  channelCount,
    bool                 useYSuffix,
    string const&        callerName,
    int                  requestedLength,    // -1 = derive from args
    bool                 useFunDescrPath);
```

7-phase body (893 disasm → 288 C++ lines @ `custom_functions.cpp:767-1054`):
1. Empty-args fast path (single-channel passthrough).
2. Per-arg waveform extraction + length determination.
3. Length validation (error 0x9E format = `(string, short, ushort)` —
   confirms `Js t` mangling tail meaning `s=short, t=ushort`).
4. Channel-count consistency check (uses `Signal::channels()` —
   `wf->signal.channels()`, accessor at `signal.hpp:83`).
5. Name synthesis (with optional `_y` suffix).
6. Factory selection (merge / interleave / grow). Three factory targets
   confirmed by direct `lea rax,[rip+...]` loads:
   - `@0x15e78a` → `WaveformGenerator::interleave` `@0x258140`
   - `@0x15e7d9` → `WaveformGenerator::merge`      `@0x25f5c0`
   - `@0x15e85e` → `WaveformGenerator::grow`       `@0x260640`

   Multi-value branch dispatch (`@0x15e774`):
   `test bl, bl; je 15e7c7` — `bl` from `[rbp-0x48]` is a function-local
   value computed earlier in the multi-value path (NOT a direct incoming
   parameter). The exact derivation is still under investigation.
   Single-value branch is unconditionally GROW (`@0x15e84b`).
   Source approximates with `(multiValue, useYSuffix)` mapping.
7. WaveformFront construction + signal copy.

**Pre-existing call-site bugs fixed in same sub-batch**: both
`play` (line ~874) and `assignWaveIndex` (line ~2044) were passing
wrong arguments — these were latent reconstruction errors that the
build tolerated only because mergeWaveforms was a stub.

**Phase 21a-followup updates**:
- `param5` is `(int)PlayArgs::getMaxSampleLength()` at both call sites
  (set @0x15f634 in play, @0x13400a in assignWaveIndex). Pushed as
  8-byte stack slot but truncated to 32 bits by callee per SysV ABI.
- `param6` (bool) in play() is `(ch != referenceChannelIndex)` where
  `referenceChannelIndex` is a per-iteration int at PlayArgs/state +0x24.
  Source approximates as `(ch != channelIndex)`.
- `param6` (bool) in assignWaveIndex is hardcoded `false` (`push 0x0`
  literal @0x13424a).

**Phase 21b-prereq-A: full call-site map**:

| Binary site | Enclosing fn  | Source line | Status      |
|-------------|---------------|-------------|-------------|
| @0x134252   | assignWaveIndex | 2892      | implemented |
| @0x135ddc   | playAuxWave   | 1809        | implemented |
| @0x136cfa   | playDIOWave   | 2083        | implemented |
| @0x15fa7b   | play()        | 1164        | implemented |
| @0x161c2b   | playIndexed   | (stubbed)   | **MISSING** |

`playIndexed` (0x160e00, 6428 bytes) is currently a ~64-line skeleton
in `custom_functions.cpp:1219-1282`. The missing mergeWaveforms call
site is one symptom of a broader gap — the function needs full
reconstruction. This re-classifies the work as a new TODO item
(promote to its own sub-phase) rather than a quick audit fix.

## playIndexed @0x160e00 — 18-phase structural map (Phase 21b-prereq-B)

| # | Address range            | Description                                                  |
|---|--------------------------|--------------------------------------------------------------|
| 1 | 0x160e00..0x160f15       | cmdName switch (Wave/AuxWave/WaveNow + default→LogRecord)    |
| 2 | 0x160f16                 | arg-count guard: `(end-begin)/56 > 2` ≡ `args.size() >= 3`   |
| 3 | 0x160f99..0x160fd1       | `PlayArgs(*config_, wf, cb, cmdName, indexed=(sub==Aux))`    |
| 4 | 0x16104c..0x1611af       | `parse(args)` then `parseOptionalRate(strict=(sub==Aux))`    |
| 5 | 0x1611b4..0x161228       | rate guard, varType ∈ {4,6} check, `EvalResults(Void)`, waveIndex |
| 5b | 0x16131b..0x1613c9      | waveIndex==0 → format error 0x9c, warningCallback_, return   |
| 6 | 0x1612b4..0x161319 (Aux) | name validation: `checkWaveformInitialized(wa.value.toString())` per WA |
| 7 | 0x161410..0x1615f0       | per-channel arg gather + 14-bit triggerMask (init 0x3fff)    |
| 8 | 0x161853..0x161867       | `getWaveformSampleLength(name)` probe                        |
| 9 | 0x161951                 | `WaveformGenerator::call("zeros", {len})`                    |
| 10 | 0x161c2b                | mergeWaveforms — 5th binary call site                        |
| 11 | 0x161d76                | `WavetableFront::loadWaveform(combined)`                     |
| 12 | 0x161df1+0x161e56        | `getRegisterNumber()` + `addi(reg, 0, Imm(waveIndex))`       |
| 13 | 0x161ee2                | `asmSetVarPlaceholder(reg)`                                  |
| 14 | 0x161f79                | Assembler push_back (addi+placeholder emission)              |
| 15 | 0x16214a                | `checkOffspecWaveLength(combined, ?)`                        |
| 16 | 0x162343                | `asmPlay(...)` — NOT asmTable as prior skeleton wrongly had  |
| 17 | 0x162487+/0x162511       | post-asmPlay Assembler push_back                             |
| 18 | 0x16283a..end            | error message factories (0x9c, 0xa0, 0x3d, 0x3e, 0xC8, ...)  |

**Three CRITICAL bug fixes vs prior skeleton applied in 21b-prereq-B**:
1. arg-count `< 2` → `< 3` (off by one)
2. `indexed = (subFunc != Aux)` → `(subFunc == Aux)` (was inverted)
3. removed wrong `asmTable` call; binary uses `addi+asmPlay` pattern

**Phase 5b discovery**: waveIndex==0 is a non-throwing warning path.
Binary @0x161236 `test eax,eax; je 16131b` jumps to a code path that
formats error 0x9c, calls `warningCallback_(...)` via vtable[+0x30]
indirect through `[res+0x1b0]`, then jumps to the success-tail cleanup
@0x1625ea and returns the (empty) results. NOT a CustomFunctionsException.

**Open structural unknowns blocking phases 8-18 reconstruction**:
- ~~`Resources` field at +0x24~~ — **RESOLVED 2026-04-24** (21b-followup-1):
  the access pattern is `[r12]; [rax+0x24]` where `r12 = this`
  (CustomFunctions*), not `*res`. So `[r12] = this->config_` (at
  CustomFunctions+0x00) and `[config_+0x24] = AWGCompilerConfig::deviceIndex`
  (per `awg_compiler_config.hpp:70`). The accessor is simply
  `config_->deviceIndex` — already named.
- The per-channel `WaveAssignment` outer container at `[rbp-0x440]` —
  outer base added to the scaled-by-deviceIndex offset.
  **PARTIALLY RESOLVED 2026-04-24**: confirmed by destructor symbol
  `vector<vector<PlayArgs::WaveAssignment>>::__base_destruct_at_end`
  at @0x162661. Type matches `playArgs.waveAssignments_` (at PlayArgs
  +0x60). However the [rbp-0x440] stack local is NOT physically the
  same as the PlayArgs field — it's a separate stack vector that gets
  populated somewhere upstream (no explicit sret-write to -0x440 found
  in disasm; possibly populated as a side effect of `PlayArgs::parse`
  via inlined out-parameter). Source-level model: treat as
  `playArgs.waveAssignments_[deviceIndex]`. See 21b-followup-3 for
  the residual trace.

**SysV ABI puzzle at the playIndexed mergeWaveforms call site**
(0x161c2b) — **RESOLVED 2026-04-24** (21b-followup-2):

Initial confusion: with sret-rdi and a 6-arg signature, args should
land in rsi/rdx/rcx/r8/r9/stack[0]. But all 5 call sites consistently
load `rdx = lea` of a stack slot (a pointer) where the signature says
arg2 is `short`. Side-by-side audit table:

| Site                | rdi        | rsi    | rdx          | rcx               | r8     | r9          | st[0]      | st[1]            |
|---------------------|------------|--------|--------------|-------------------|--------|-------------|------------|------------------|
| 0x134252 assignWI   | lea sret   | r14    | lea[rbp-0xb0]| movsx[rax+0x14]   | xor=0  | lea[rbp-0x40]| 0          | [rbp-0x238]      |
| 0x135ddc playAux    | lea sret   | rbx    | lea[rbp-0x78]| movsx[rax+0x16]   | mov 1  | lea[rbp-0x90]| 0          | 0                |
| 0x136cfa playDIO    | lea sret   | r14    | lea[rbp-0x40]| movsx[rax+0x14]   | xor=0  | lea[rbp-0x58]| 0          | [rbp-0xa8]       |
| 0x15fa7b play       | lea sret   | rbx    | lea[rbp-0x70]| movsx[r13+0x14]   | xor=0  | lea[rbp-0x50]| rax (bool) | [rbp-0xa8]       |
| 0x161c2b playIndexed| lea sret   | r12    | lea[rbp-0x90]| movsx[rax+rcx]    | sete b | lea[rbp-0x40]| 0          | 0                |

Resolution comes from inspecting the mergeWaveforms entry @0x15e060:
```
15e074:  mov [rbp-0xe0], r9     ; arg5 = string& callerName
15e07b:  mov [rbp-0xec], r8d    ; arg4 = bool useYSuffix
15e082:  mov r15d, ecx          ; arg3 = short channelCount
15e085:  mov rbx, rdx           ; arg2 = vector<EvalResultValue>&
15e088:  mov [rbp-0xe8], rsi    ; arg1 = ??? (used at 0x15e13b)
15e08f:  mov r14, rdi           ; sret slot
```
Then @0x15e13b: `rax = [rbp-0xe8]; rdi = [rax+0x20]; call
WavetableFront::getWaveformSampleLength`. Field +0x20 of the saved
rsi is a `WavetableFront*` — that's the `wavetableFront_` field of
`CustomFunctions`. So **rsi is the implicit `this` pointer** and
mergeWaveforms is a non-static member function (which we already
knew from the demangled name `zhinst::CustomFunctions::mergeWaveforms`).

The "6 args" of the demangled signature are the **explicit** args;
the implicit `this` consumes rsi, shifting the explicit args by one
register. Final mapping:

| Reg / slot   | Meaning                                                  |
|--------------|----------------------------------------------------------|
| rdi          | sret slot for `shared_ptr<WaveformFront>` return         |
| rsi          | implicit `this` (CustomFunctions*)                       |
| rdx          | arg1: `vector<EvalResultValue> const&`                   |
| rcx          | arg2: `short channelCount`                               |
| r8           | arg3: `bool useYSuffix`                                  |
| r9           | arg4: `string const& callerName`                         |
| stack[0]     | arg5: `int requestedLength`                              |
| stack[1]     | arg6: `bool useFunDescrPath`                             |

**Outcome**: no bug; the 4 already-implemented call sites are
correct; no source changes needed. The C++ compiler handles `this`
and sret automatically. The only deliverable from this audit is the
register-layout table above (now part of this notes file) so future
reconstruction work doesn't re-stumble on the same puzzle.

## playIndexed phases 8-18 reconstruction (21b-prereq-B-cont, 2026-04-24)

Following the phases 1-7 source reconstruction in the prior sub-phase,
phases 8-18 were reconstructed in `custom_functions.cpp`. All 18
phases now have executable C++ (with TODO markers documenting the
residual unknowns absorbed into 21b-followup-3).

Key findings from phases 8-18 disasm:

**Phase 9 source-level helper**: The inline construction of the
literal `"zeros"` string + a `vector<Value>{Value(int=baseLen)}` +
call to `WaveformGenerator::call("zeros", args)` is exactly what
`WaveformGenerator::createDummyWaveform(int length)` does (see
`waveform_generator.hpp:137`, "@0x25be70"). The binary inlines this
helper instead of calling it (likely cross-TU inlining at LTO time).
Source-level we restore the call.

**Phase 10 channelCount source**: The `[rbp-0xc8]` offset variable
holds either 0x14 or 0x16 depending on the prior path (set at
@0x16182a writes 0x16, set at @0x161825 in a different branch writes
0x16 too). Both 0x14 and 0x16 are 16-bit shorts in `AWGCompilerConfig`
adjacent to `channelsPerGroup[0]`. Modeled as `channelsPerGroup[1]`
pending precise per-branch trace.

**Phase 11 bounds check**: Before `loadWaveform` the binary checks
`waveIndex > combined->[+0xd0]` and throws via the long-form length
error at @0x1629cb. The `+0xd0` field of `WaveformFront` is a sample-
length cap; not yet named. Left as TODO.

**Phase 12 addi return type discovery**: `AsmCommands::addi(reg, reg,
Imm)` returns `vector<AsmList::Asm>` (not single `Asm`) — caught at
build time during this sub-phase and corrected. The binary inlines the
vector's `__insert_with_size` into a destination Assembler, which is
why it doesn't look like a normal vector return.

**Phase 14 Assembler push tracking**: The disasm shows two near-
identical inline `vector::push_back` blocks (@0x161ed4 and @0x161f53)
into a per-call Assembler instance whose pointer comes from
`[rbp-0xc0]` (a saved arg slot). At source level this looks like
"build local Asm entries, push them into the active assembler". The
exact owner is not yet traced — left as TODO.

**Phase 16 asmPlay 12-arg call**: 12 SysV slots is unusual; the
binary loads them via 4 stack pushes + 8 register slots. Modeled
structurally after `play()`'s asmPlay call (lines 1196..1202 in
`custom_functions.cpp`) since the field meanings are identical
(waveforms vector, indexReg from getRegisterNumber, regInv=AsmReg(-1),
holdCount=rate, trigger=triggerMask). The boolean flag mapping
(isHold/fourChannel/isBool/isHoldMode) is approximate and tagged TODO.

**Phase 17 push to results->assemblers_**: Confirmed via `lea rdi,
[r15+0x18]` where `r15 = [r13]` and `r13` was the saved first-arg
slot. `+0x18` matches `EvalResults::assemblers_` at `eval_results.hpp:75`.

**Phase 18 cleanup**: 0x1cf bytes of dtor calls + return path. The
exception tail factories @0x162848+ generate four error formats:
- 0x3d "wrong number of arguments" → Phase 2 throw
- 0x98 "invalid first-arg type" → not yet wired
- 0xa0 "rate too low" → Phase 4 throw
- 0x9a "invalid wave-index type" → not yet wired

Plus `std::__throw_bad_function_call` @0x162971 (called when
`warningCallback_` is empty in Phase 5b — already guarded by `if
(warningCallback_)` in source).

**Build status**: clean after the reconstruction (one inline build
fix for `addi` return type; no warnings, no link errors).


## writeToNode @0x164550 — recon (Phase 21b.1)

**Function size**: 0x164550 → 0x16b740 = 0x71f0 bytes (29KB,
6120 disasm lines), bigger than the historical 23KB estimate.
Cached slice: `/tmp/writeToNode.disasm`
(`sed -n '155684,161803p' /tmp/disasm_full.txt`).

### Signature correction

The header had declared:

```cpp
void writeToNode(EvalResultValue, EvalResultValue, EvalResultValue,
                 std::shared_ptr<Resources>);
```

Disassembly shows otherwise. At entry:

```
164550:  push rbp ; mov rbp,rsp ; push r15..rbx
16455d:  sub rsp, 0xad8
164564:  mov [rbp-0x358], r9       ; r9 = res shared_ptr
16456b:  mov [rbp-0x1a8], r8       ; r8 = &type
164572:  mov [rbp-0x50], rcx       ; rcx = &val
164576:  mov r15, rdx              ; rdx = &path
164579:  mov r14, rsi              ; rsi = this
16457c:  mov rbx, rdi              ; rdi = SRET slot   <— return value
16457f:  mov edi, 0x98
164584:  call operator new
...
1645cc:  mov [rbx], rcx            ; sret[0] = control-block payload
1645d6:  mov [rbx+8], rax          ; sret[8] = ptr
```

Two 8-byte stores into `[rbx]` / `[rbx+8]` is the libc++
`shared_ptr<EvalResults>` layout. Therefore the return type must be
`std::shared_ptr<EvalResults>`. Header updated accordingly. The
existing `setInt` call site `writeToNode(...); return nullptr;` is
left in place with a TODO marker for 21b.5 to convert it into a
forwarder.

### EvalResultValue passing convention

`EvalResultValue` is non-trivially-copyable (contains `Value` which
contains `std::string`), so it is passed by hidden reference per the
Itanium C++ ABI even when declared by value. Hence `rdx = &path`,
`rcx = &val`, `r8 = &type`. The shared_ptr (16B trivially copyable)
goes in `r9` (8B) plus a stack slot for the control-block pointer.

### Static regex objects

4 `boost::regex` instances live in `.bss`:

| Regex | Address | Guard | Purpose |
|-------|---------|-------|---------|
| `absDevRegex` | b84748 | b84758 | absolute device path `dev<N>/<rest>` |
| `awgNodeRegex` | b84760 | b84770 | AWG channel-relative node |
| `sineNodeRegex` | b84778 | b84788 | sine/oscillator node |
| `oscselNodeRegex` | b84790 | b847a0 | osc-select node (largest dispatch) |

Pattern strings are constructed lazily in cold paths
(@0x169ea5, @0x169efd, @0x169f54, @0x169fab — Block F). These
strings are TODO 21b.5; the source uses placeholder patterns in an
anonymous namespace.

### Sub-block map (size + addresses)

| Block | Range | Size | Content |
|-------|-------|------|---------|
| Setup | 0x164550..0x164608 | ~180B | Allocate EvalResults; cmdName toString() |
| A: absDevRegex | 0x16460e..~0x164800 | ~500B | Match abs device path |
| B: awgNodeRegex | 0x164803..~0x164950 | ~330B | AWG channel dispatch |
| C: sineNodeRegex | 0x164950..~0x164ae0 | ~390B | Sine osc dispatch |
| D: oscselRegex | 0x164b19..~0x169d00 | **~21KB** | Bulk per-type codegen |
| E: error tail | 0x169d83..0x169df4 | ~110B | Throw formatted error |
| F: regex init | 0x169ea5..0x16a3f0 | ~1.5KB | Static regex ctors (cold) |
| G: epilogue | 0x16a3f0..0x16b740 | ~5KB | dtors / unwinding |

3 binary call sites of writeToNode all in `CustomFunctions`:
@0x141447 (likely setUserReg), @0x1486f2 (`setInt`),
@0x149334 (`setDouble`).

### Block A (absDevRegex) decoded

After `Value::toString()` produces the working path string at
`[rbp-0x1a0]`, the libc++ short-string deref pattern unpacks
begin/size:

```
164642:  movzx eax, BYTE PTR [rbp-0x1a0]   ; first byte = (size<<1)|long_flag
164649:  mov esi, eax ; shr esi, 1          ; short-mode size = byte>>1
16464d:  test al, 0x1                       ; bit 0 = long-mode flag
16464f:  lea rbx, [rbp-0x19f]               ; short-mode begin = inline buf
164656:  mov rdi, [rbp-0x190]               ; long-mode begin
16465d:  cmove rdi, rbx                     ; pick depending on flag
164661:  cmovne rsi, [rbp-0x198]            ; long-mode size
164669:  add rsi, rdi                       ; rsi = end = begin + size
```

Then `boost::regex_match(begin, end, &matches@[rbp-0x220], absDevRegex, 0)`.

On match: extract two consecutive sub_match captures via
`boost::sub_match::str()` @0x16b830:

```
1646da:  shr rax, 3 ; imul eax, 0xaaaaaaab   ; (end-begin)/24 = num_submatches
1646e0:  add rsi, 0x48                       ; sub_match base + 0x48
1646e4:  cmp eax, 0x4 ; cmovl rsi, &empty    ; bounds: need >= 4 submatches
1646f9:  call sub_match::str()               ; → device-id string
```

Then `std::stoul(devIdStr, nullptr, 10)` → `requestedDev` and
validate against `[r14] [+0x24]` = `this->config_->deviceIndex`
(the Phase 21b-followup-1 finding: `r14=this`, not `res`; first 8
bytes of `*this` is `config_`).

```
164734:  mov rax, [r14]              ; r14 = this; rax = config_
164737:  cmp [rax+0x24], r12d        ; deviceIndex vs requestedDev
16473b:  jne 169d83                   ; → error tail (device mismatch)
```

On success: read sub_match[+0x60] (next consecutive capture, +0x18
= one sub_match struct further) → "rest of path" string. Copy it
into the working buffer via 16-byte xmm move (libc++ string
SSO move):

```
1647df:  movups xmm0, [rbp-0x178]    ; new string from sub_match
1647e6:  movaps [rbp-0x1a0], xmm0    ; store into working buffer
```

Whether absDevRegex matched or not, control reaches @0x1647ed which
unconditionally calls `lookupNode(pathStr)`. The result is consumed
by Blocks B/C/D for the subsequent dispatch.

### Block A error path (@0x169d83 → "device mismatch")

Allocates 0x40-byte exception via `__cxa_allocate_exception`,
constructs message string, and (presumably — full reconstruction in
21b.5) throws `CustomFunctionsException`. Source approximation:

```cpp
throw CustomFunctionsException(
    "writeToNode: device id in path does not match configured device");
```

The exact format string and error code are TODO 21b.5.

### Capture-index uncertainty

The `+0x48` and `+0x60` offsets into `match_results.array_` differ
by one sub_match struct (0x18). They map to capture indices that
depend on libc++'s `match_results` layout (prefix at index 0, then
groups). The source uses Boost API `matches[1]` and `matches[2]`
which are the natural capture-group indices; this should be
confirmed once the real pattern strings are recovered in 21b.5.


## writeToNode Blocks B + C — recon (Phase 21b.2)

After Block A's `lookupNode(pathStr)` call at @0x1647ed, control
falls into a chain of two more `boost::regex_match` tests against
the working `pathStr`. Both tests share the same match_results
buffer (`[rbp-0x220]`) reused across all four regex tests.

### Block B: awgNodeRegex @0x164803..0x16491e (~290B)

Path shape (TODO 21b.5: confirm against recovered pattern string):
something like `awgs/<channelIdx>/<rest>`.

Decoded flow:

```
164803:  movzx eax, BYTE PTR [b84770]      ; awgNodeRegex guard byte
16480c:  je   169efd                        ; → cold ctor path (Block F)
164812:  ... libc++ short-string deref of pathStr ...
164835:  lea rcx, [b84760]                  ; awgNodeRegex
164846:  call boost::regex_match            ; → al = matched
16484d:  je  16491e                         ; not matched → fall through to C
1648b5:  lea rdi, [rbp-0x178]
1648bc:  call sub_match::str()              ; capture[+0x48] → string
1648d3:  call std::stoul(s, nullptr, 10)    ; parse decimal
1648d8:  mov r13, rax                       ; r13 = channelIdx
1648e8:  mov rax, [r14]                     ; r14 = this; rax = config_
1648eb:  mov r12d, [rax+0x1c]               ; r12 = numChannelGroups
1648ef:  cmp r12d, 0x2 ; jne 1649d7
   1648f9: ... r13 = (r13 + (r13>>31)) >> 1 ; signed /2
1649d7:  cmp r12d, 0x4 ; jne 164911 (skip both)
   1649e1: lea ecx, [r13+0x3] ; cmovns ecx,r13d ; sar ecx,0x2 ; r13 = ecx ; signed /4
164911:  mov r14d, [rax+0x20]               ; cap (config_->unknown_20)
164915:  cmp r13d, r14d
164918:  jne 1649ff                          ; → error path
```

Source-level translation:

```cpp
if (boost::regex_match(begin, end, matches, awgNodeRegex)) {
    int channelIdx = std::stoul(matches[1].str(), nullptr, 10);
    int numGroups = config_->numChannelGroups;        // [config+0x1c]
    if (numGroups == 2) channelIdx /= 2;
    else if (numGroups == 4) channelIdx /= 4;
    if (channelIdx != config_->unknown_20)            // [config+0x20]
        throw CustomFunctionsException(/* msg with deviceIndex */);
}
```

Note that the error tail @0x1649ff reads `r15d = [rax+0x24]` =
`config_->deviceIndex` — used only to format the diagnostic.

### Block C: sineNodeRegex @0x16491e..0x164ae4 (~460B)

Path shape (TODO 21b.5): `sines/<oscIdx>/<rest>`.

Decoded flow:

```
16491e:  movzx eax, [b84788]                ; sineNodeRegex guard
164927:  je  169f54                         ; → cold ctor (Block F)
164950:  lea rcx, [b84778]                  ; sineNodeRegex
164961:  call boost::regex_match
16496a:  je  164aea                         ; not matched → Block D
164a72:  call sub_match::str()              ; capture[+0x48]
164a85:  call stoul ; r13 = oscIdx
164aad:  mov rcx, [r14] ; r12d = [rcx+0x1c] ; numChannelGroups
164ab4:  xor eax,eax ; cmp r12d,0x2 ; sete al
164abd:  lea esi, [rax*2+0x2]               ; esi = (numGroups==2) ? 4 : 2
164ac4:  mov eax, r13d ; cdq ; idiv esi     ; eax = oscIdx / esi
164aca:  cmp r12d, 0x4 ; jne 164add
   164ad0: lea edx,[rax+0x3] ; cmovns edx,eax ; sar edx,0x2 ; signed /4 again
164add:  mov r13d, [rcx+0x20]               ; cap
164ae1:  cmp eax, r13d
164ae4:  jne 16a006                         ; → error path
```

Mapping table (oscIdx → derived channelIdx):

| numChannelGroups | First divide | Then if ==4 | Total |
|------------------|--------------|-------------|-------|
| 1 (default)      | / 2          | (skip)      | / 2   |
| 2                | / 4          | (skip)      | / 4   |
| 4                | / 2          | / 4         | / 8   |

Interpretation: 2 oscillators per channel by default, 4 when grouping
into pairs, 8 when grouping into quads.

Source-level translation:

```cpp
if (boost::regex_match(begin, end, matches, sineNodeRegex)) {
    int oscIdx = std::stoul(matches[1].str(), nullptr, 10);
    int numGroups = config_->numChannelGroups;
    int divisor = (numGroups == 2) ? 4 : 2;
    oscIdx /= divisor;
    if (numGroups == 4) oscIdx /= 4;
    if (oscIdx != config_->unknown_20)
        throw CustomFunctionsException(/* msg */);
}
```

### Shared properties

- Both blocks read `config_->numChannelGroups` and
  `config_->unknown_20`. `unknown_20` is therefore a "current
  channel index" or similar per-instance constant — TODO: rename
  to a meaningful field name once we identify another reader.
- Error tail at @0x1649ff and @0x16a006 share the same exception
  type (`CustomFunctionsException`, allocated 0x40 bytes).
- Both regexes consume only capture group [1] (numeric index);
  they don't substitute the working path string the way Block A
  does. The path retains its original (post-Block-A) value going
  into Block D.


## writeToNode Block D — structural skeleton (Phase 21b.3)

Block D (oscselNodeRegex, @0x164b19..~0x169d00) is the bulk of
writeToNode at ~21KB / ~5000 disasm lines. This sub-phase lays
down the structural decomposition; the per-case asm-emission
bodies are TODO 21b.4.

### Stack-frame discovery: NodeMapItem layout confirmed

Earlier audits had the `NodeMapItem` layout in
`node_map_data.hpp:93+` (data*, typeIdx, fastAddr, hasFast at
+0x00/+0x08/+0x0c/+0x10) but had not been cross-checked against
writeToNode's stack usage. This sub-phase confirms it:

- `[rbp-0x1c0]` is the first 8 bytes of a 64-byte local
  `NodeMapItem` returned by `lookupNode(pathStr)` (call @0x1647ed).
- `[rbp-0x1b8]` (read as DWORD at @0x164c5f, @0x164ceb,
  @0x164d45, @0x164db2) is `node.typeIdx` at +0x08.
- `[rbp-0x1b0]` (read as BYTE at @0x164c0e, @0x164c43) is
  `node.hasFast` at +0x10.
- The cleanup at @0x16b6db..16b6f5 calls `[rax+8]` on
  `*[rbp-0x1c0]` — a virtual-destructor call on a raw
  `NodeMapData*`, NOT a libc++ shared_ptr `_M_release()`.
  Confirms `data` is a raw owning pointer.

### "MF" tag insertion into usedFeatures_

@0x164b3a..164b4a constructs an SSO short-string at `[rbp-0x178]`:

```
164b3a:  mov BYTE PTR [rbp-0x178], 0x4    ; SSO header: size=4>>1=2
164b41:  mov WORD PTR [rbp-0x177], 0x464d  ; bytes 'M','F' (LE)
164b4a:  mov BYTE PTR [rbp-0x175], 0x0    ; null terminator
```

@0x164b51..164bee then walks `[r14+0x1c8]` as a `__tree<string>`
and either finds an equal entry or calls
`__tree::__emplace_unique_key_args` to insert. Source-level:

```cpp
this->usedFeatures_.insert("MF");
```

`usedFeatures_` is the `std::set<std::string>` at
`CustomFunctions+0x1C8` (already named in
`custom_functions.hpp:548`). The semantics of "MF" specifically
is unclear; candidates include "Multi-Frequency" / "Marker Flag" /
"MutableField" — TODO 21b.5 to identify by surveying readers
of `usedFeatures_`.

### addNodeAccess + getRegisterNumber + AsmList scaffolding

```
164c0e:  movzx edx, BYTE PTR [rbp-0x1b0]   ; AccessMode = node.hasFast
164c15:  lea rsi, [rbp-0x1c0]               ; &node
164c1c:  mov rdi, r14                       ; this
164c1f:  call addNodeAccess
164c24:  call Resources::getRegisterNumber  ; returns int in eax
164c2d:  call AsmRegister::AsmRegister(int) ; constructs destReg
164c34:  xorps + movaps + qword zero        ; AsmList localList = {}
```

Note: `AccessMode` here is byte-cast from `node.hasFast`. Either
NodeMapItem's `hasFast` field aliases an AccessMode-style enum, or
this is a separate flag stored in the same byte slot. TODO 21b.5
to verify via `addNodeAccess @0x15c6c0` disasm (its second arg
type may clarify).

### asmCommands_ access pattern

All `suser`/`addi` calls go via `[r14+0x50]` — the raw pointer
slot of `this->asmCommands_` (a `shared_ptr<AsmCommands>` at
`CustomFunctions+0x50`, already named in `custom_functions.hpp:487`).
The `.get()` is implicit — disasm uses `mov rsi, [r14+0x50]` where
the first 8 bytes of a libc++ shared_ptr ARE the raw pointer.

### Dual jump-table dispatch on node.typeIdx

After address resolution, both branches dispatch on
`node.typeIdx` (range 0..5; `cmp rcx, 0x5; ja default`). Two
DISTINCT jump tables are used:

| Branch | Address-source | Jump table |
|--------|---------------|------------|
| Fast path (`node.hasFast == true`) | `node.fastAddress()` @0x1c5470 | @958f68 |
| Slow path (`!hasFast`) | `nodeIndexMap_.find(node)->second` (+0x100 unordered_map) | @958f50 |

The slow path at @0x164d92 first calls `__hash_table::find` on
`this->nodeIndexMap_`; if not found it logs a warning via
`zhinst::logging::detail::LogRecord` (cold path @0x164d05) and
exits via the warning-callback path.

### Per-case asm-emission summary (TODO 21b.4)

Empirical pattern by typeIdx (deduced from disasm sweep at lines
388-700):

| typeIdx | Fast path entry | Slow path entry | Emission pattern |
|---------|----------------|-----------------|------------------|
| 0 | (logging fall-through) | @0x164de4 | `addi(reg, regZero, Immediate(1))` + bulk insert |
| 1 | (TODO) | @0x164ee7 | `toDouble()` → `getSampleClock()` → `NodeMap::toFrequency()` → `Immediate(int)` |
| 2 | @0x164c7f (val.varType==2 → suser+addr=0x17+append) | @0x165013 | `toDouble()` → `cvtsd2ss` → `Immediate(float bits)` |
| 3 | (TODO) | @0x165137 | `toDouble()` → raw `movq` int bits → `Immediate(int)` |
| 4 | (TODO) | @0x165263 | Two `suser` calls (addr=0x17 + addr=0x19) for paired emit |
| 5 | (TODO) | (TODO) | (TODO) |

The fast-path case 2 is implemented in source as an exemplar:

```cpp
if (static_cast<int>(valRef.varType_) != 2)
    throw CustomFunctionsException("...");
AsmList::Asm asm1 = asmCommands_->suser(valRef.reg_,
                        detail::AddressImpl<uint32_t>(0x17u + addr));
localList.append(asm1);
```

Whether `addr` is OR'd into the encoding or whether the literal
`0x17` IS the full address (with `addr` participating in some
other way) is TODO 21b.4 — the binary uses literal `mov ecx, 0x17`
without explicit OR, so `addr` may not factor in for case 2.

### Cumulative call counts in Block D

From the regex sweep in earlier session:

- 53× `AsmCommands::suser`
- 44× `AsmCommands::addi`
- 48× `Assembler::~Assembler` (cleanup of intermediate AsmList::Asm)
- 25× `AsmList::append`
- 48× `AsmRegister::AsmRegister(int)` (per-emit reg construction)
- 44× `Immediate::Immediate(int)` (per-emit immediate construction)

Average per case: ~9 suser + ~7 addi + ~4 append + ~12 cleanup
calls. The repetitiveness across 12 cases (6 fast + 6 slow) is
why Block D is so large despite the simple high-level structure.

----------------------------------------------------------------
writeToNode Block D — structural correction (Phase 21b.3-fix)
----------------------------------------------------------------

The 21b.3 first pass modeled Block D as a 2-way (fast/slow) dispatch
on `node.hasFast`. That's wrong. The actual structure is a **3-way
dispatch on the address resolution method**:

    (A) hasFast == true                              @0x164c50..164c5c
        addr = node.fastAddress()
        switch (node.typeIdx) jump table @958f68

    (B) hasFast == false AND
        dynamic_cast<DirectAddrNodeMapData*>(node.data) != nullptr
                                                      @0x164cb9..164cdf
        addr = direct->addr_   (read from base+0x8)
        switch (node.typeIdx) jump table @958f50

    (C) hasFast == false AND dyncast fails (or data == nullptr)
                                                      @0x164d92..164da8
        auto it = nodeIndexMap_.find(node);
        if (it == end()) throw  @0x16a045
        addr = it->second
        switch (node.typeIdx) jump table @958f50

Paths (B) and (C) **share** the slow-path jump table @958f50; path
(A) uses jt @958f68. The dynamic_cast was missed in the 21b.3 pass
because both (B) and (C) converge to the same code at @0x164dcc
(`mov edx, [rax]` reading the resolved addr, then computing the jt
index).

The dispatch sequence at @0x164dcc..164de2:

    164dcc:  mov    edx, [rax]              # addr (B: direct->addr_, C: it->second)
    164dce:  lea    rax, [rip+0x7f417b]     # 958f50 (jt base)
    164dd5:  movsxd rcx, [rax+rcx*4]        # rcx = node.typeIdx (saved earlier)
    164dd9:  add    rcx, rax
    164ddc:  mov    [rbp-0x17c], edx        # stash addr
    164de2:  jmp    rcx

For path (A) the equivalent at @0x164c5f..164c7d:

    164c5f:  mov    eax, [rbp-0x1b8]        # node.typeIdx
    164c65:  cmp    rax, 0x5
    164c69:  ja     165407                  # default: TODO 21b.4 (likely warning)
    164c6f:  lea    rcx, [rip+0x7f42f2]     # 958f68 (jt base)
    164c76:  movsxd rax, [rcx+rax*4]
    164c7a:  add    rax, rcx
    164c7d:  jmp    rax

Both paths (B) and (C) gate `typeIdx <= 5` via:

    164cfb:  cmp    rcx, 0x5
    164cff:  jbe    164dcc                  # in-range → dispatch
                                            # out-of-range → fall through to:
    164d05:  ...boost::log severity-1 record with the typeIdx number...
    164d75:  ...end record...
    164d81:  mov    rax, [r14]              # this->config_
    164d84:  cmp    [rax], 0x2              # config_->something == 2 ?
    164d87:  je     169940                  # tail returning results
    164d8d:  jmp    169ae6                  # alternate tail (returns results)

Both tails return the (mostly-empty) `results` shared_ptr without
emitting any asm. Modeled in source as `emitWarnAndReturn = true →
return results` (TODO 21b.5: reconstruct the exact log message and
the config_->something==2 branch).

----------------------------------------------------------------
AsmCommands::addi return type (Phase 21b.3-fix observation)
----------------------------------------------------------------

The slow-path case 0 disasm @0x164de4..164e58 reveals that
`AsmCommands::addi(AsmRegister, AsmRegister, Immediate) const`
returns a **vector<Asm>** (not a single Asm), constructed in a
hidden sret slot at `[rbp-0x178]`/`[rbp-0x170]` (begin/end pointers).
The caller computes `(end - begin) / sizeof(Asm)` (where sizeof(Asm)
= 168 bytes via the magic constant `0xcf3cf3cf3cf3cf3d` divisor) and
splices the entire vector into `localList` via
`vector::__insert_with_size`. Then it walks the source vector
backwards calling `Assembler::~Assembler()` on each element to
release the temporaries.

This contradicts the existing `asm_commands.hpp` declaration that
shows `addi` returning a single Asm. **TODO for 21b.4**: revise the
`addi` declaration to `std::vector<AsmList::Asm> addi(...)` to match
the binary, then propagate the change through the rest of writeToNode
where `addi` is called.
