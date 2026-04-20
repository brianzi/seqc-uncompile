# Overview: Reconstructed zhinst SeqC Compiler Assembler Backend

Reverse-engineered from `_seqc_compiler.so` (ELF 64-bit, x86-64, not stripped).

## Class Hierarchy

```
AsmCommands                        # Main class — 83 methods generating assembler records
  └─ AsmCommandsImpl* impl_       # Pimpl: device-specific instruction encoding
       ├─ AsmCommandsImplCervino   # Older/FPGA devices
       └─ AsmCommandsImplHirzel    # HDAWG, UHF, SHF devices
```

`AsmCommands` methods produce `AsmList::Asm` records, each containing an
`AssemblerInstr` (opcode + registers + immediates) and optionally a
`shared_ptr<Node>` for higher-level AST constructs. Device-specific
encoding differences (opcode selection, register slot assignment, I/O
addresses) are handled by virtual dispatch through `impl_`.

## File Structure

```
reconstructed/
├── include/zhinst/
│   ├── types.hpp                # Forward declarations, AwgDeviceType enum (corrected values)
│   ├── awg_compiler_config.hpp  # AWGCompilerConfig struct (0xC0 bytes), 4 methods
│   ├── asm_register.hpp         # AsmRegister struct {int value; bool valid} (8 bytes)
│   ├── value.hpp                # Immediate (28B std::variant), Value (40B boost::variant),
│   │                            #   AddressImpl<T>, ValueException — declarations only
│   ├── assembler.hpp            # AssemblerInstr (0x80 bytes), Assembler::Command enum (43 opcodes)
│   ├── play_config.hpp          # PlayConfig struct — all shift/mask constants confirmed
│   ├── node.hpp                 # Node class (0x110 bytes), NodeType enum (14 types)
│   ├── waveform_front.hpp       # WaveformFront (0xF8 bytes) — Waveform base (0xD8), Signal corrected to 0x58
│   ├── wavetable_front.hpp      # WavetableFront — interface stub
│   ├── asm_list.hpp             # AsmList::Asm record (0xA8 bytes), AsmList (vector wrapper, 0x18 bytes)
│   ├── error_messages.hpp       # ErrorMessageT enum (305 values), ErrorMessages class, ResourcesException
│   ├── asm_commands_impl.hpp    # AsmCommandsImpl base + Cervino/Hirzel declarations
│   ├── asm_commands.hpp         # AsmCommands — all 83 method declarations
│   ├── device_constants.hpp     # DeviceConstants (0x90 bytes POD), RegisterBank, getDeviceConstants()
│   ├── signal.hpp               # Signal (0x58 bytes), SampleFormat enum, MarkerBitsPerChannel, ReserveOnly tag
│   └── awg_compiler_config.hpp  # AWGCompilerConfig (~0xC0 bytes), AwgDeviceType enum (moved from types.hpp)
├── src/
│   ├── asm_commands.cpp         # AsmCommands implementations (~550 lines)
│   ├── awg_compiler_config.cpp  # AWGCompilerConfig: dtor, getAwgDeviceTypeString,
│   │                            #   getAwgDeviceTypeFromString, getChannelGroupingModeString
│   ├── asm_commands_impl.cpp    # getInstance() factory (bitmask device selection)
│   ├── asm_commands_impl_cervino.cpp  # 11 Cervino virtual methods
│   ├── asm_commands_impl_hirzel.cpp   # 11 Hirzel virtual methods
│   ├── assembler.cpp            # Assembler methods: cmdMap, commandToString, commandFromString,
│   │                            #   str, getOpcodeType, getCycles, getCmdType, getRegisterOrder,
│   │                            #   highestRegisterNumber, copy/move/dtor docs
│   ├── asm_list.cpp             # AsmList: append, print, serialize, deserialize, maxRegister;
│   │                            #   AsmList::Asm: dtor, serializeNodeToJsonString, createUniqueID
│   ├── value.cpp                # Value: ctor(string), toDouble, toInt, toBool, toString, op==, dtor;
│   │                            #   Immediate: 4 ctors, dtor, holdsUnsigned, op int/uint, op==;
│   │                            #   Free: toString(Immediate), op<<(Immediate), op<<(AddressImpl);
│   │                            #   ValueException: ctor, dtor, what()
│   ├── play_config.cpp          # PlayConfig: encodeCwvf, operator!=, toJson, fromJson
│   ├── node.cpp                 # Node: 2 ctors, dtor, type2str, str2type, toString,
│   │                            #   waveAtCurrentDeviceIndex, clone, last, insertBefore,
│   │                            #   updateParent, remove, swap, toJson, fromJson,
│   │                            #   installPointers (15 methods total)
│   └── waveform_front.cpp       # WaveformFront: toString, copy-rename ctor, dtor
│       device_constants.cpp     # getDeviceConstants() factory — 9 device types, full field population
│       error_messages.cpp       # ErrorMessages: operator[], format<>() pattern, 305 format strings;
│                                #   ResourcesException: ctor/dtor/what; getApiErrorMessage()
│       signal.cpp               # Signal: 17 methods — 7 ctors, copy ctor, dtor, 2 appends,
│                                #   resizeSamples, checkAllocation, toJson, fromJson, getRawData, op==
│       awg_compiler_config.cpp  # AWGCompilerConfig: dtor, getAwgDeviceTypeString/FromString,
│                                #   getChannelGroupingModeString
└── notes/
    ├── opcode_map.md            # Full opcode table: value, name, args, device, notes
    ├── cervino_vs_hirzel.md     # Device selection logic + per-method difference matrix
    ├── struct_layouts.md        # Raw byte-offset tables for all structs (updated Phase 3a)
    ├── node_tree_structure.md   # Node tree model: sibling chain, elseBranch, children
    └── unknowns.md              # Open questions (9 resolved, 21 open)
```

## What is Reconstructed vs Stubbed

**Fully reconstructed from disassembly:**
- `AsmCommandsImpl::getInstance()` — exact bitmask logic
- All 11 `AsmCommandsImplCervino` virtual methods — opcodes, register slots, error cases
- All 11 `AsmCommandsImplHirzel` virtual methods — same, with device-specific differences
- ~70 of 83 `AsmCommands` methods — simple instructions, ALU, branches, load/store, triggers, pseudo-instructions, node-creating methods
- `Value` class — full layout (40 bytes), toInt/toDouble/toBool/toString/operator==, Value(string) ctor, destructor. All methods in src/value.cpp
- `Immediate` class — full layout (28 bytes), all constructors, conversions, operator==, toString, operator<<. All methods in src/value.cpp
- `ValueException` — ctor, dtor, what(). Layout: vptr + string (0x20 bytes)
- `PlayConfig` — all shift/mask constants, encodeCwvf (full bit-packing), operator!=, toJson, fromJson
- `Assembler` — 43-entry cmdMap with all opcode names, commandToString (reverse lookup), commandFromString (case-insensitive), str(bool verbose) disassembly output
- `Assembler::getOpcodeType()`, `getCycles()`, `getCmdType()`, `getRegisterOrder()` — full switch tables reconstructed
- `AssemblerInstr::highestRegisterNumber()` — collects valid regs, returns packed (1<<32)|regIndex
- `AssemblerInstr` — full 0x80-byte layout confirmed (cmd, immediates, reg2/reg0/reg1, outputs, label, comment)
- `AssemblerInstr` copy/move/dtor — compiler-generated; documented (bulk memcpy for POD regs, pilfer-style move)
- `Node` — full 0x110-byte layout, 14 NodeType values, 15 methods: 2 ctors, dtor, type2str, str2type, toString, waveAtCurrentDeviceIndex, clone, last, insertBefore, updateParent, remove, swap, toJson, fromJson, installPointers
- `Node` field names confirmed from toJson/fromJson JSON keys: nodeId, asmId, wavesPerDev, deviceIndex, config, currentCwvf, lengthReg, length, indexOffsetReg, play, next, branches, loop, globalRate, defaultPrecompFlags, loopBodyRunsAtLeastOnce, branchMaySkipAllBodies, trig
- `Node` link roles confirmed: +0xB8=next (sibling chain), +0xE0=loop (loop/else branch), +0xF0=parent (weak_ptr), +0xC8=branches (for Branch type 4)
- `AsmList::Asm` — 0xA8-byte record, real nested type name confirmed (was "AsmEntry"). 7 methods: dtor, serializeNodeToJsonString, createUniqueID (inlined, TLS+0x40)
- `AsmList` — exactly `std::vector<AsmList::Asm>` (0x18 bytes, no extra fields). 6 methods: dtor, append, print, serialize, deserialize, maxRegister. parseStringToAsmList stubbed (~7000 bytes, complex parser)
- `AsmRegister` — struct {int value; bool valid} (8 bytes), corrected from enum
- `WaveformFront` — full 0xF8-byte layout (Waveform base 0xD8 + 0x20 extension). 3 methods: toString (format string), copy-rename ctor (delegates to Waveform base ctor), dtor. Waveform::File::Type enum confirmed (CSV=0, RAW=1, GEN=2). CORRECTED Phase 3c: Signal at +0x80 is 0x58 bytes (not 48) — the fields previously called markers/sampleWidth/numSamples are Signal's markerBits_/channels_/length_.
- `DeviceConstants` — full 0x90-byte POD layout. 4 RegisterBank sub-structs (waveform, commandTable, sequencer, aux), scalar device params (samplingRate, waveformMemSize, maxSequenceLen, etc.). Factory `getDeviceConstants()` reconstructed for all 9 device types (UHFQA, HDAWG8, UHFLI, HDAWG4, SHFSG, SHFQA, SHFQC-QA, SHFQC-SG, SHFQC-v2). Nested `Register` type with anonymous enum constants (SyncRegA=0x44, SyncRegB=0x45).
- `ErrorMessages` — 305-entry format string table (std::map<int,string> at BSS 0xb84c38). Full ErrorMessageT enum reconstructed: 253 compiler messages (1–255, gaps at 47/53), 5 status codes (16384+), 33 API errors (32768+), 14 firmware errors (36864+). operator[] and format<>() (boost::format wrapper, ~40 template instantiations documented). ResourcesException: ctor/dtor/what. getApiErrorMessage(): hash-table lookup on separate BSS table.
- `Signal` — 0x58-byte struct (corrected from 48 bytes). Full layout: 3 vectors (samples_, markers_, markerBits_), channels_ (uint16_t), reserveOnly_ (bool), length_ (uint64_t). 17 methods: 7 constructors (size, fill, MarkerBitsPerChannel, move-from-components, reserve-only, copy-from-refs, samples+channels), copy ctor, dtor, append(double,uint8_t), append(Signal&), resizeSamples, checkAllocation (lazy materializer), toJson, fromJson, getRawData (Cervino/Hirzel16/PlaceHolder), operator== (fuzzy float comparison). MarkerBitsPerChannel = vector<uint8_t> typedef. SampleFormat enum: Cervino=0, Hirzel16=1. ReserveOnly tag type for ctor dispatch. RawWaveData hierarchy discovered: RawWavePlaceHolder, RawWaveCervino (double2awg), RawWaveHirzel16.
- `AWGCompilerConfig` — ~0xC0-byte config struct. 4 methods: dtor (strings + vector<string> + path cleanup), `getAwgDeviceTypeString()` (static, switch→SSO string), `getAwgDeviceTypeFromString()` (static, cascading iequals), `getChannelGroupingModeString()` (HDAWG-only, numChannelGroups→"4x2"/"2x4"/"1x8"). **Major correction**: AwgDeviceType enum values completely revised — 9 devices with codenames (cervino, hirzel, klausen, grimsel_qa/sg/qc_sg/li, gurnigel, maloja). Hirzel = {HDAWG(2), SHFQA(8), SHFSG(16), SHFQC_SG(32), SHFLI(64)}. Cervino = {UHFLI(1), UHFQA(4), GHFLI(128), VHFLI(256)}.
- `AWGCompilerConfig` — 0xC0-byte struct. Layout: deviceType, addressImpl, numChannelGroups, two conditional strings (+0x30/+0x50), vector<string> includePaths (+0x70), wavetableSize (+0xA0), searchPath (+0xA8). 4 methods fully reconstructed: dtor (0xf8080), getAwgDeviceTypeString (0x270080, jump-table SSO returns), getAwgDeviceTypeFromString (0x270180, iequals cascading), getChannelGroupingModeString (0x270b10, HDAWG-only "4x2"/"2x4"/"1x8"). AwgDeviceType enum corrected: 9 device types with codenames (cervino/hirzel/klausen/grimsel_*/gurnigel/maloja).

**Partially reconstructed (logic approximate):**
- `AsmCommands::alui()` — multi-instruction immediate splitting (core logic present, edge cases uncertain)
- `AsmCommands::asmPlay()` — most complex method; waveform name vector, PlayConfig, packed play word
- `AsmCommands::genPlayConfig()` — marker processing loop reconstructed
- `AsmCommands::syncCervino()` / `unsyncCervino()` — stubbed; ~1000 asm lines each, deferred
- `AsmList::parseStringToAsmList()` — stubbed; ~7000 bytes, complex parser with AWGAssembler dependency

**Stubbed / placeholder:**
- `WavetableFront` — interface only

## Key Technical Findings

- **Return convention:** All `const` methods use sret (hidden first parameter = return value pointer). Actual `this` is in `rsi`.
- **Sequence IDs:** Thread-local counter at TLS offset 0x44, incremented per node created (simple ctor) or per instruction emitted (AsmEntry).
- **isWaveformCmd:** `(cmd - 3) < 3u` — flags opcodes 3/4/5 (MESSAGE and ERROR_MSG pseudo-instructions). Name may be misleading.
- **Large immediates:** Signed ALU immediate instructions split values > ~18-bit across ADDI (low 12 bits) + ADDIU chain (12 bits at a time).
- **AsmRegister:** 8-byte struct `{int value; bool valid;}`, NOT an enum. Separate valid flag from register number.
- **AssemblerInstr register order:** reg2 (destination) at +0x20, reg0 (src1) at +0x28, reg1 (src2) at +0x30.
- **AssemblerInstr outputs:** Second `vector<Immediate>` at +0x38 holds output operands (distinct from input immediates at +0x08).
- **Opcode names confirmed (Phase 2a):** 43 entries in cmdMap. XORR (0x60000007), WWVF (0xF1000000), WVFEI (0xFB000000), WVFE (0xFA), WVFET (0xFC), JMP (0xFE). New: BRGZ (0xF5000000, branch if >0). Alias: `wwvfq` = WPRF.
- **New NodeTypes (Phase 2):** SyncHirzel=0x2000, AwgReady=0x8000. "Prefetch" renamed to "PlainLoad" (0x4000).
- **Branch penalty:** BRZ, BRNZ, BRGZ cost 3 cycles; all other instructions cost 1.
- **DeviceConstants (Phase 3a):** POD struct, 0x90 bytes. 9 device types supported (enum values 1/2/4/8/16/32/64/128/256). SHFQC-SG (128) is the only 12 GHz device; all others are 1.8–2.4 GHz. HDAWG8 (2) is the only device with `hasExtendedReg=true`. Source: `/builds/labone/labone/ziAWG/ziAWGDevice/src/constants.cpp`.
- **Signal (Phase 3c):** 0x58 bytes, not 48. This corrects the Waveform base layout: the fields at +0xB0/+0xC8/+0xD0 (previously markers/sampleWidth/numSamples) are actually Signal's internal markerBits_/channels_/length_ fields. `checkAllocation()` is a lazy materializer that does NOT clear `reserveOnly_`. `operator==` uses fuzzy float comparison: `|a-b| <= |b|*eps + eps` (eps=1e-12). `getRawData()` reveals RawWaveData hierarchy (PlaceHolder/Cervino/Hirzel16).
- **AWGCompilerConfig / AwgDeviceType (Phase 3d):** Major enum correction. Previous analysis had wrong device names and Cervino/Hirzel classification. Correct mapping confirmed from `getAwgDeviceTypeString` (jump table) and `getAwgDeviceTypeFromString` (codename strings). 9 devices total; 5 Hirzel, 4 Cervino. Channel grouping only applies to HDAWG (type=2). GHFLI (128, "gurnigel") is 12 GHz but uses Cervino, not Hirzel.

See `notes/` for detailed opcode tables, struct offset maps, and the full list of open questions.
