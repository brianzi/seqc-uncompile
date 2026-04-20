# TODO: Reverse Engineering Tasks

Status: `[ ]` not started · `[~]` in progress · `[x]` complete

---

## Phase 2: Structurally Embedded Types

Types directly constructed/returned/accessed by AsmCommands methods.
Precise layouts here will refine and correct the Phase 1 reconstruction.

### 2a. AssemblerInstr / Assembler (~0x80 bytes)

- [x] Enumerate all `zhinst::Assembler::` symbols from the binary
- [x] Pin down the unknown 0x30 bytes at offset +0x50..+0x80
- [x] Update `assembler.hpp` with confirmed layout
- [ ] Reconstruct `Assembler::commandToString()` — dump cmdMap at 0xb84c20
- [ ] Reconstruct `Assembler::str()` — full instruction formatting
- [ ] Reconstruct copy ctor, move assignment, destructor
- [ ] Reconstruct `getOpcodeType()`, `getCycles()`, `getCmdType()`, `getRegisterOrder()`, `highestRegisterNumber()`
- [ ] Identify any methods that do final binary instruction encoding
- [ ] Create `src/assembler.cpp` with all above implementations
- [ ] Identify the 3 new opcodes: 0x60000007, 0xF1000000, 0xFB000000

### 2b. AsmEntry / AsmList::Asm (0xA8 bytes)

- [ ] Confirm the 0xA8-byte layout against additional call sites
- [ ] Enumerate `zhinst::AsmList::` symbols; determine container type
- [ ] Understand the `AsmList::Asm` enum in `Compiler::runPrefetcher` signature
- [ ] Reconstruct AsmList methods (iteration, insertion, serialization)
- [ ] Update `asm_list.hpp` and create `src/asm_list.cpp`

### 2c. PlayConfig Methods

Layout and constants are complete. Methods need implementation.

- [x] Trace address loads in `genPlayConfig` / `asmPlay` to find .rodata locations
- [x] Extract all shift/mask constant values
- [x] Update `play_config.hpp` and `src/asm_commands.cpp`
- [x] Implement `encodeCwvf()` — bit-packing (0x1dc500)
- [x] Implement `operator!=()` (0x1d5770)
- [x] Implement `toJson()` (0x269d60)
- [x] Implement `fromJson()` (0x26b440) — discovered during enumeration
- [x] Create `src/play_config.cpp`

### 2d. Node Methods

Layout is complete. Methods need implementation.

- [x] Enumerate all `zhinst::Node::` symbols
- [x] Confirm NodeType enum values against additional uses outside AsmCommands
- [x] Fill in the +0x04..+0x28 gap (24 unknown bytes)
- [x] Confirm fields beyond +0x104
- [x] Update `node.hpp`
- [x] Implement simple constructor (0x12ace0)
- [x] Implement full constructor (0x26c4a0)
- [x] Implement destructor (0x12afe0)
- [x] Implement `toString()` (0x264440)
- [x] Implement `waveAtCurrentDeviceIndex()` (0x1c7de0)
- [x] Implement `type2str()` (0x269970), `str2type()` (0x26ac00)
- [x] Implement `clone()` (0x1d5d40)
- [x] Implement tree methods: `last`, `insertBefore`, `updateParent`, `remove`, `swap`
- [x] Identify and reconstruct tree-traversal / child management methods
- [x] Create `src/node.cpp`

### 2e. WaveformFront

- [ ] Enumerate all `zhinst::WaveformFront::` symbols
- [ ] Determine name/string access mechanism (operator*, getName(), etc.)
- [ ] Fill in unknown gaps between known field offsets
- [ ] Reconstruct key methods (name access, marker iteration)
- [ ] Update `waveform_front.hpp` and create `src/waveform_front.cpp`

### 2f. Value / Immediate Methods

Structural layout is complete (was Phase 3a). Methods are inline in header;
need to be moved to .cpp and any remaining symbols reconstructed.

- [x] Determine if Value is std::variant, tagged union, or other
- [x] Reconstruct `toInt()`, `toDouble()`, variant visitation
- [x] Update `value.hpp`
- [ ] Enumerate remaining `zhinst::Value::` and `zhinst::Immediate::` symbols
- [ ] Move method bodies from header to `src/value.cpp`
- [ ] Reconstruct any methods not yet covered (constructors, assignment, etc.)

### Phase 2 wrap-up

- [x] Review and update `OVERVIEW.md` to reflect new findings
- [x] Review and update `notes/struct_layouts.md`
- [x] Review `notes/unknowns.md` — close resolved questions, add new ones
- [ ] Re-evaluate Phase 3/4 ordering; insert any newly discovered work items

---

## Phase 3: Supporting Types

### 3a. ~~Value / Immediate~~ → moved to 2f

### 3b. ErrorMessages / ResourcesException

- [ ] Enumerate `zhinst::ErrorMessages::` symbols
- [ ] Map all ErrorMessageT enum values (0, 5, 0xB confirmed; likely more)
- [ ] Extract format strings from .rodata
- [ ] Reconstruct `ErrorMessages::format()` template
- [ ] Update `error_messages.hpp` and create `src/error_messages.cpp`

### 3c. AWGCompilerConfig / DeviceConstants

- [ ] Enumerate symbols
- [ ] Determine which config fields affect AsmCommands behavior
- [ ] Reconstruct the relevant subset (header + methods together)
- [ ] Add new header if warranted

### Phase 3 wrap-up

- [ ] Review and update `OVERVIEW.md`
- [ ] Review `notes/unknowns.md`
- [ ] Re-evaluate remaining phase ordering; insert new work items

---

## Phase 4: Broader Infrastructure

### 4a. AsmList (container)

- [ ] Reconstruct iteration, insertion, serialization methods
- [ ] Understand how downstream code consumes AsmEntry records

### 4b. WavetableFront

- [ ] Reconstruct `getWaveformByName(optional<string>)`
- [ ] Understand waveform registration / lookup data structure
- [ ] Update `wavetable_front.hpp` and create `src/wavetable_front.cpp`

### Phase 4 wrap-up

- [ ] Review and update `OVERVIEW.md`
- [ ] Review `notes/unknowns.md`
- [ ] Re-evaluate remaining phase ordering; insert new work items

---

## Phase 5: Integration — Compilation Pipeline

Main call sites that drive AsmCommands:

- [ ] Disassemble `Compiler::runPrefetcher`
- [ ] Disassemble `FrontEndLoweringFacade::lower`
- [ ] Disassemble `Prefetch::Prefetch` constructor
- [ ] Disassemble `CustomFunctions::CustomFunctions` constructor
- [ ] Map the overall compilation pipeline flow
- [ ] Document in `notes/` or a new `notes/pipeline.md`

### Phase 5 wrap-up

- [ ] Final review of `OVERVIEW.md`
- [ ] Final review of all `notes/` files
- [ ] Identify further components worth reconstructing (or declare scope complete)

---

## Deferred / Low Priority

- [ ] Full reconstruction of `syncCervino()` (~1000 asm lines)
- [ ] Full reconstruction of `unsyncCervino()` (~1000 asm lines)
- [ ] Full reconstruction of `addi32()` (32-bit immediate edge cases)
- [ ] Add `CMakeLists.txt` to compile reconstructed code as a validation step
- [ ] Write comparison tests against the real `.so`
