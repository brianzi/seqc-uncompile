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
| +0x54  | 4    | int                          | nodeExtraRef_       | Passed to Node constructors |

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

## AsmEntry (0xA8 bytes)

| Offset | Size | Type               | Name            | Notes                    |
|--------|------|--------------------|-----------------|--------------------------|
| +0x00  | 4    | int                | sequenceId      | From TLS counter         |
| +0x04  | 4    | (padding)          |                 |                          |
| +0x08  | 0x80 | AssemblerInstr     | assembler       |                          |
| +0x88  | 4    | int                | wavetableFront  |                          |
| +0x8C  | 4    | (padding)          |                 |                          |
| +0x90  | 16   | shared_ptr<Node>   | node            | May be null              |
| +0xA0  | 1    | bool               | isWaveformCmd   | (cmd-3) < 3u             |

## Node (0x110 bytes) — CONFIRMED

Fully confirmed from simple ctor (0x12ace0), full ctor (0x26c4a0), dtor (0x12afe0).
Inherits `enable_shared_from_this<Node>` (weak_ptr at +0x00/+0x08).

| Offset | Size | Type                          | Name          | Notes                         |
|--------|------|-------------------------------|---------------|-------------------------------|
| +0x00  | 8    | ptr (enable_shared_from_this) | _sp_ptr       | weak_ptr embedded             |
| +0x08  | 8    | ctrl (enable_shared_from_this)| _sp_ctrl      | dtor: __release_weak if set   |
| +0x10  | 4    | int                           | id            | TLS counter (node_id++)       |
| +0x14  | 4    | int                           | deviceIndex   | toString prints this          |
| +0x18  | 8    | uint64_t                      | _reserved0    | zeroed in ctor                |
| +0x20  | 8    | uint64_t                      | _reserved1    | zeroed; dtor: __release_weak  |
| +0x28  | 24   | vector<optional<string>>      | waves         | waveform names per device     |
| +0x40  | 4    | int                           | waveformIndex | index into waves; -1=none     |
| +0x44  | 4    | NodeType (int)                | type          | toString prints type2str()    |
| +0x48  | 32   | PlayConfig                    | playConfig1   | 0x20 bytes                    |
| +0x68  | 32   | PlayConfig                    | playConfig2   | 0x20 bytes                    |
| +0x88  | 8    | AsmRegister                   | reg1          | int(4) + bool(4 padded)       |
| +0x90  | 4    | int                           | regVal        |                               |
| +0x94  | 8    | AsmRegister                   | reg2          |                               |
| +0x9C  | 4    | int                           | tableIndex    | -1 = none                     |
| +0xA0  | 24   | vector<weak_ptr<Node>>        | weakRefs      | dtor iterates, releases weak  |
| +0xB8  | 16   | shared_ptr<Node>              | nextSibling   | next in sibling chain         |
| +0xC8  | 24   | vector<shared_ptr<Node>>      | children      | child nodes (Branch type 4)   |
| +0xE0  | 16   | shared_ptr<Node>              | elseBranch    | conditional/else branch link  |
| +0xF0  | 16   | weak_ptr<Node>                | parent        | dtor: __release_weak          |
| +0x100 | 4    | int                           | intField1     | rate for Rate nodes           |
| +0x104 | 4    | unsigned int                  | uintField     | precompFlags for SetPrecomp   |
| +0x108 | 1    | bool                          | boolField1    |                               |
| +0x109 | 1    | bool                          | boolField2    |                               |
| +0x10A | 2    | (padding)                     |               |                               |
| +0x10C | 4    | int                           | intField2     |                               |

## WaveformFront

| Offset | Size | Type      | Name           | Notes                       |
|--------|------|-----------|----------------|-----------------------------|
| +0x48  | 1    | bool      | used           | Set by asmPlay/asmPrefetch  |
| +0x68  | 4    | uint32_t  | playWord       | Packed PlayConfig           |
| +0x6C  | 4    | int32_t   | playIndex      | If < 0, compute playWord    |
| +0xB0  | 8    | uint8_t*  | markerData     | Start of marker array       |
| +0xB8  | 8    | uint8_t*  | markerDataEnd  | End of marker array         |
| +0xC8  | 2    | uint16_t  | sampleWidth    | Bits per sample             |

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
