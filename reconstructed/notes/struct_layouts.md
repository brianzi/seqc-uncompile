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

## Waveform (base class, 0xD8 bytes) — CORRECTED Phase 3c

Base class of WaveformFront. CORRECTED: Signal is 0x58 bytes (not 48).
The fields previously listed as separate (markers, sampleWidth, numSamples)
are actually inside Signal (markerBits_, channels_, length_).

| Offset | Size | Type                | Name            | Notes                           |
|--------|------|---------------------|-----------------|---------------------------------|
| +0x00  | 24   | std::string         | name            | waveform name (SSO)             |
| +0x18  | 4    | File::Type (enum)   | waveformType    | 0=CSV, 1=RAW, 2=GEN            |
| +0x1C  | 4    | (padding)           |                 |                                 |
| +0x20  | 24   | std::string         | secondaryName   | additional name/path            |
| +0x38  | 16   | shared_ptr\<File\>  | file            | source file reference           |
| +0x48  | 1    | bool                | used            | set by asmPlay/asmPrefetch      |
| +0x49  | 3    | (padding)           |                 |                                 |
| +0x4C  | 4    | int                 | field4C         | unknown                         |
| +0x50  | 24   | std::string         | thirdString     | additional string field         |
| +0x68  | 4    | uint32_t            | playWord        | packed PlayConfig               |
| +0x6C  | 4    | int32_t             | playIndex       | -1 = compute playWord           |
| +0x70  | 4    | int                 | seqRegWidth     | from DeviceConstants.sequencerReg.width |
| +0x74  | 4    | int                 | field74         | init=0                          |
| +0x78  | 8    | DeviceConstants*    | deviceConstants | pointer to device config         |
| +0x80  | 0x58 | Signal              | signal          | see Signal layout above          |
| +0xD8  |      | END                 |                 |                                  |

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

## RegisterBank (16 bytes) — NEW Phase 3a

Hardware register region descriptor. Four instances embedded in DeviceConstants.

| Offset | Size | Type     | Name   | Notes                              |
|--------|------|----------|--------|------------------------------------|
| +0x00  | 4    | uint32_t | base   | Starting register address          |
| +0x04  | 4    | uint32_t | stride | Address stride between groups      |
| +0x08  | 4    | uint32_t | width  | Registers per group                |
| +0x0C  | 4    | uint32_t | depth  | Number of groups/entries            |

## DeviceConstants (0x90 = 144 bytes) — NEW Phase 3a

POD struct, no vtable. Populated by `getDeviceConstants(AwgDeviceType)` at 0x2cc0c0.
Source: `/builds/labone/labone/ziAWG/ziAWGDevice/src/constants.cpp`

| Offset | Size | Type          | Name              | Notes                                |
|--------|------|---------------|-------------------|--------------------------------------|
| +0x00  | 4    | uint32_t      | deviceType        | AwgDeviceType enum value             |
| +0x04  | 1    | bool          | hasExtendedReg    | true only for HDAWG (type=2)         |
| +0x05  | 3    | (padding)     |                   |                                      |
| +0x08  | 16   | RegisterBank  | waveformReg       | Primary waveform register bank       |
| +0x18  | 16   | RegisterBank  | commandTableReg   | Command table register bank          |
| +0x28  | 8    | uint64_t      | numOutputChannels | Typically 16                         |
| +0x30  | 8    | uint64_t      | registerSpace     | Typically 1024                       |
| +0x38  | 16   | RegisterBank  | sequencerReg      | Sequencer control register bank      |
| +0x48  | 16   | RegisterBank  | auxReg            | Auxiliary/DIO register bank          |
| +0x58  | 8    | uint64_t      | waveformMemSize   | 0x400/0x4000/0x8000                  |
| +0x60  | 8    | uint64_t      | maxSequenceLen    | 16000                                |
| +0x68  | 4    | uint32_t      | seqClockDivider   | 0 or 1165                            |
| +0x6C  | 4    | (padding)     |                   |                                      |
| +0x70  | 8    | double        | samplingRate      | Hz (1.8e9–12.0e9)                   |
| +0x78  | 4    | uint32_t      | numOutputPorts    | 0, 10, or 12                         |
| +0x7C  | 4    | uint32_t      | numAWGCores       | 0, 3, or 5                           |
| +0x80  | 1    | bool          | hasDIO            | DIO support flag                     |
| +0x81  | 3    | (padding)     |                   |                                      |
| +0x84  | 4    | uint32_t      | numDIOBits        | 0, 6, or 8                           |
| +0x88  | 1    | bool          | hasPrecomp        | Precompensation support              |
| +0x89  | 7    | (padding)     |                   | Align to 0x90                        |

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
| +0x18  | 4    | int                   | unknown_18        |                                    |
| +0x1C  | 4    | int                   | numChannelGroups  | 1/2/4; only meaningful for HDAWG   |
| +0x20  | 16   | (unknown)             | unknown_20        |                                    |
| +0x30  | 24   | std::string           | string_30         | Conditionally owned (guard at +0x48) |
| +0x48  | 1    | bool                  | string_30_owned   | Controls dtor of string_30         |
| +0x50  | 24   | std::string           | string_50         | Conditionally owned (guard at +0x68) |
| +0x68  | 1    | bool                  | string_50_owned   | Controls dtor of string_50         |
| +0x70  | 24   | vector\<string\>      | includePaths      | begin/end/cap at +0x70/+0x78/+0x80 |
| +0x88  | 24   | (unknown)             | unknown_88        |                                    |
| +0xA0  | 4    | int32_t               | wavetableSize     | Sign-extended to size_t            |
| +0xA4  | 4    | (pad)                 |                   |                                    |
| +0xA8  | 24   | boost::filesystem::path | searchPath      | Dtor frees at +0xA8/+0xB8         |

Channel grouping (HDAWG only):
- numChannelGroups=1 → "4x2" (4 groups of 2 channels)
- numChannelGroups=2 → "2x4" (2 groups of 4 channels)
- numChannelGroups=4 → "1x8" (1 group of 8 channels)
