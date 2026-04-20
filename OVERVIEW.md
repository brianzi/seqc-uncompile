# Overview: Reconstructed zhinst SeqC Compiler Assembler Backend

Reverse-engineered from `_seqc_compiler.so` (ELF 64-bit, x86-64, not stripped).

## Class Hierarchy

```
AsmCommands                        # Main class — 83 methods generating assembler records
  └─ AsmCommandsImpl* impl_       # Pimpl: device-specific instruction encoding
       ├─ AsmCommandsImplCervino   # Older/FPGA devices
       └─ AsmCommandsImplHirzel    # HDAWG, UHF, SHF devices
```

`AsmCommands` methods produce `AsmEntry` records, each containing an
`AssemblerInstr` (opcode + registers + immediates) and optionally a
`shared_ptr<Node>` for higher-level AST constructs. Device-specific
encoding differences (opcode selection, register slot assignment, I/O
addresses) are handled by virtual dispatch through `impl_`.

## File Structure

```
reconstructed/
├── include/zhinst/
│   ├── types.hpp                # Forward declarations, AwgDeviceType enum
│   ├── asm_register.hpp         # AsmRegister struct {int value; bool valid} (8 bytes)
│   ├── value.hpp                # Immediate (28B std::variant), Value (40B boost::variant), AddressImpl<T>
│   ├── assembler.hpp            # AssemblerInstr (0x80 bytes), Assembler::Command enum (43 opcodes)
│   ├── play_config.hpp          # PlayConfig struct — all shift/mask constants confirmed
│   ├── node.hpp                 # Node class (0x110 bytes), NodeType enum (14 types)
│   ├── waveform_front.hpp       # WaveformFront — known field offsets only
│   ├── wavetable_front.hpp      # WavetableFront — interface stub
│   ├── asm_list.hpp             # AsmEntry record (0xA8 bytes), AsmList container stub
│   ├── error_messages.hpp       # ErrorMessageT enum, ResourcesException
│   ├── asm_commands_impl.hpp    # AsmCommandsImpl base + Cervino/Hirzel declarations
│   └── asm_commands.hpp         # AsmCommands — all 83 method declarations
├── src/
│   ├── asm_commands.cpp         # AsmCommands implementations (~550 lines)
│   ├── asm_commands_impl.cpp    # getInstance() factory (bitmask device selection)
│   ├── asm_commands_impl_cervino.cpp  # 11 Cervino virtual methods
│   ├── asm_commands_impl_hirzel.cpp   # 11 Hirzel virtual methods
│   ├── assembler.cpp            # Assembler methods: cmdMap, commandToString, commandFromString,
│                                #   str, getOpcodeType, getCycles, getCmdType, getRegisterOrder,
│                                #   highestRegisterNumber, copy/move/dtor docs
│   ├── play_config.cpp          # PlayConfig: encodeCwvf, operator!=, toJson, fromJson
│   └── node.cpp                 # Node: 2 ctors, dtor, type2str, str2type, toString,
│                                #   waveAtCurrentDeviceIndex, clone, last, insertBefore,
│                                #   updateParent, remove, swap (12 methods total)
└── notes/
    ├── opcode_map.md            # Full opcode table: value, name, args, device, notes
    ├── cervino_vs_hirzel.md     # Device selection logic + per-method difference matrix
    ├── struct_layouts.md        # Raw byte-offset tables for all structs (updated Phase 2)
    └── unknowns.md              # Open questions (6 resolved Phase 2, 6 new added)
```

## What is Reconstructed vs Stubbed

**Fully reconstructed from disassembly:**
- `AsmCommandsImpl::getInstance()` — exact bitmask logic
- All 11 `AsmCommandsImplCervino` virtual methods — opcodes, register slots, error cases
- All 11 `AsmCommandsImplHirzel` virtual methods — same, with device-specific differences
- ~70 of 83 `AsmCommands` methods — simple instructions, ALU, branches, load/store, triggers, pseudo-instructions, node-creating methods
- `Value` class — full layout (40 bytes), toInt/toDouble/toBool/toString/operator==
- `Immediate` class — full layout (28 bytes), all constructors, conversions, operator==
- `PlayConfig` — all shift/mask constants, encodeCwvf (full bit-packing), operator!=, toJson, fromJson
- `Assembler` — 43-entry cmdMap with all opcode names, commandToString (reverse lookup), commandFromString (case-insensitive), str(bool verbose) disassembly output
- `Assembler::getOpcodeType()`, `getCycles()`, `getCmdType()`, `getRegisterOrder()` — full switch tables reconstructed
- `AssemblerInstr::highestRegisterNumber()` — collects valid regs, returns packed (1<<32)|regIndex
- `AssemblerInstr` — full 0x80-byte layout confirmed (cmd, immediates, reg2/reg0/reg1, outputs, label, comment)
- `AssemblerInstr` copy/move/dtor — compiler-generated; documented (bulk memcpy for POD regs, pilfer-style move)
- `Node` — full 0x110-byte layout, 14 NodeType values, 12 methods: 2 ctors, dtor, type2str, str2type, toString, waveAtCurrentDeviceIndex, clone, last, insertBefore, updateParent, remove, swap
- `Node` field roles confirmed: +0xB8=nextSibling, +0xE0=elseBranch, +0xF0=parent (weak_ptr), +0xC8=children (for Branch type 4)
- `AsmRegister` — struct {int value; bool valid} (8 bytes), corrected from enum

**Partially reconstructed (logic approximate):**
- `AsmCommands::alui()` — multi-instruction immediate splitting (core logic present, edge cases uncertain)
- `AsmCommands::asmPlay()` — most complex method; waveform name vector, PlayConfig, packed play word
- `AsmCommands::genPlayConfig()` — marker processing loop reconstructed
- `AsmCommands::syncCervino()` / `unsyncCervino()` — stubbed; ~1000 asm lines each, deferred

**Stubbed / placeholder:**
- `WaveformFront` — only known field offsets, no methods
- `WavetableFront` — interface only
- `Node::toJson()` / `fromJson()` / `installPointers()` — discovered but not yet reconstructed
- `ErrorMessages::format()` — template stub, no format strings extracted

## Key Technical Findings

- **Return convention:** All `const` methods use sret (hidden first parameter = return value pointer). Actual `this` is in `rsi`.
- **Sequence IDs:** Thread-local counter at TLS offset 0x40, incremented per instruction emitted.
- **isWaveformCmd:** `(cmd - 3) < 3u` — flags opcodes 3/4/5 (MESSAGE and ERROR_MSG pseudo-instructions). Name may be misleading.
- **Large immediates:** Signed ALU immediate instructions split values > ~18-bit across ADDI (low 12 bits) + ADDIU chain (12 bits at a time).
- **AsmRegister:** 8-byte struct `{int value; bool valid;}`, NOT an enum. Separate valid flag from register number.
- **AssemblerInstr register order:** reg2 (destination) at +0x20, reg0 (src1) at +0x28, reg1 (src2) at +0x30.
- **AssemblerInstr outputs:** Second `vector<Immediate>` at +0x38 holds output operands (distinct from input immediates at +0x08).
- **Opcode names confirmed (Phase 2a):** 43 entries in cmdMap. XORR (0x60000007), WWVF (0xF1000000), WVFEI (0xFB000000), WVFE (0xFA), WVFET (0xFC), JMP (0xFE). New: BRGZ (0xF5000000, branch if >0). Alias: `wwvfq` = WPRF.
- **New NodeTypes (Phase 2):** SyncHirzel=0x2000, AwgReady=0x8000. "Prefetch" renamed to "PlainLoad" (0x4000).
- **Branch penalty:** BRZ, BRNZ, BRGZ cost 3 cycles; all other instructions cost 1.

See `notes/` for detailed opcode tables, struct offset maps, and the full list of open questions.
