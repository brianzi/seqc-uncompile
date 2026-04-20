# TODO: Reverse Engineering Tasks

Status: `[ ]` not started · `[~]` in progress · `[x]` complete

---

## Phase 1: Core AsmCommands (COMPLETE)

All 83 methods + both Impl classes fully reconstructed. See OVERVIEW.md.

## Phase 2: Structurally Embedded Types (COMPLETE)

### 2a. AssemblerInstr / Assembler (~0x80 bytes)

- [x] Enumerate all `zhinst::Assembler::` symbols from the binary
- [x] Pin down the unknown 0x30 bytes at offset +0x50..+0x80
- [x] Update `assembler.hpp` with confirmed layout
- [x] Reconstruct `Assembler::commandToString()` — dump cmdMap at 0xb84c20
- [x] Reconstruct `Assembler::str()` — full instruction formatting
- [x] Reconstruct copy ctor, move assignment, destructor
- [x] Reconstruct `getOpcodeType()`, `getCycles()`, `getCmdType()`, `getRegisterOrder()`, `highestRegisterNumber()`
- [x] Identify any methods that do final binary instruction encoding
- [x] Create `src/assembler.cpp` with all above implementations
- [x] Identify the 3 new opcodes: 0x60000007, 0xF1000000, 0xFB000000

### 2b. AsmEntry / AsmList::Asm (0xA8 bytes)

- [x] Confirm the 0xA8-byte layout against additional call sites
- [x] Enumerate `zhinst::AsmList::` symbols; determine container type
- [x] Understand the `AsmList::Asm` nested type (was "AsmEntry")
- [x] Reconstruct AsmList methods (append, print, serialize, deserialize, maxRegister)
- [x] Reconstruct AsmList::Asm methods (dtor, serializeNodeToJsonString, createUniqueID)
- [x] Update `asm_list.hpp` and create `src/asm_list.cpp`

### 2c. PlayConfig Methods

- [x] Trace address loads in `genPlayConfig` / `asmPlay` to find .rodata locations
- [x] Extract all shift/mask constant values
- [x] Update `play_config.hpp` and `src/asm_commands.cpp`
- [x] Implement `encodeCwvf()` — bit-packing (0x1dc500)
- [x] Implement `operator!=()` (0x1d5770)
- [x] Implement `toJson()` (0x269d60)
- [x] Implement `fromJson()` (0x26b440) — discovered during enumeration
- [x] Create `src/play_config.cpp`

### 2d. Node Methods

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
- [x] Write `notes/node_tree_structure.md`
- [x] Declare and reconstruct `toJson()` (0x264b90)
- [x] Declare and reconstruct `fromJson()` (0x268280) — static
- [x] Declare and reconstruct `installPointers()` (0x269020) — static

### 2e. WaveformFront

- [x] Enumerate all `zhinst::WaveformFront::` symbols
- [x] Determine name/string access mechanism (operator*, getName(), etc.)
- [x] Fill in unknown gaps between known field offsets
- [x] Reconstruct key methods (toString, copy-rename ctor, dtor)
- [x] Update `waveform_front.hpp` and create `src/waveform_front.cpp`

### 2f. Value / Immediate Methods

- [x] Determine if Value is std::variant, tagged union, or other
- [x] Reconstruct `toInt()`, `toDouble()`, variant visitation
- [x] Update `value.hpp`
- [x] Enumerate remaining `zhinst::Value::` and `zhinst::Immediate::` symbols
- [x] Move method bodies from header to `src/value.cpp`
- [x] Reconstruct `Value(string const&)` constructor (0x22c2b0)
- [x] Reconstruct `operator<<(ostream&, Immediate)` (0x290b90)
- [x] Reconstruct `operator<<(ostream&, AddressImpl<uint>)` (0x1c7ce0)
- [x] Reconstruct `ValueException` ctor/dtor/what (0x16e7f0, 0x16e850, 0x16f110)
- [x] Create `src/value.cpp`

### Phase 2 wrap-up

- [x] Review and update `OVERVIEW.md` to reflect new findings
- [x] Review and update `notes/struct_layouts.md`
- [x] Review `notes/unknowns.md` — close resolved questions, add new ones
- [x] Write `notes/node_tree_structure.md`
- [x] Correct TLS offset (0x44 not 0x40) in OVERVIEW.md and notes/unknowns.md
- [x] Add AsmOptimize and AWGAssembler to Phase 4 for future investigation
- [x] Re-evaluate Phase 3/4 ordering; insert any newly discovered work items

---

## Phase 3: Simple Foundational Types

### 3a. DeviceConstants + getDeviceConstants

DeviceConstants is a POD struct with no own methods, but is referenced by nearly
every downstream type. The `getDeviceConstants(AwgDeviceType)` factory at 0x2cc0c0
populates it. Nested `DeviceConstants::Register` sub-struct with anonymous enums.

- [x] Disassemble `getDeviceConstants(AwgDeviceType)` factory (0x2cc0c0)
- [x] Determine struct layout from field accesses in factory + known consumers
- [x] Identify `DeviceConstants::Register` nested sub-struct and its anonymous enums
- [x] Map fields used by AsmCommands (e.g. +0x40 → WaveformFront.field70)
- [x] Map fields used by Waveform ctor, WavetableFront ctor, AWGAssembler ctor
- [x] Create `include/zhinst/device_constants.hpp` with full layout
- [x] Create `src/device_constants.cpp` with `getDeviceConstants()` implementation
- [x] Update `notes/struct_layouts.md` with DeviceConstants offset table

### 3a wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/unknowns.md
- [ ] Propose TODO.md adjustments

### 3b. ErrorMessages / ResourcesException

BSS message table at 0xb84c38, ~53 symbols (mostly template instantiations of
`format<>()`). Core logic is small: `operator[]` lookup + boost::format wrapper.

- [x] Dump BSS message table at 0xb84c38 — extract all ErrorMessageT enum values and format strings
- [x] Reconstruct `ErrorMessages::operator[](ErrorMessageT) const` (0x108380)
- [x] Reconstruct `ErrorMessages::format<>()` no-arg version (0x15d0d0)
- [x] Document the template instantiation pattern (40+ specializations — document, don't reconstruct all)
- [x] Reconstruct `getApiErrorMessage(ZIResult_enum)` (0x2e4820)
- [x] Reconstruct `ResourcesException` ctor/dtor/what (0x1e3a20, 0x1f12f0, 0x1f1340)
- [x] Create `include/zhinst/error_messages.hpp` with ErrorMessageT enum + declarations
- [x] Create `src/error_messages.cpp`
- [x] Note the apiErrorMessages BSS table at 0xb85230

### 3b wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/unknowns.md (close #13 re: format strings)
- [ ] Propose TODO.md adjustments

### 3c. Signal

Signal is the actual sample data type (doubles + markers). **0x58 bytes** (corrected
from 48) inside Waveform base at +0x80. 17 methods, all reconstructed.

- [x] Determine Signal struct layout (0x58 bytes: 3 vectors + channels_ + reserveOnly_ + length_)
- [x] Reconstruct `Signal(unsigned long)` ctor (0x25dd60)
- [x] Reconstruct `Signal(vector<double>, vector<uchar>, ...)` ctor (0x2a8940)
- [x] Reconstruct `Signal(Signal const&)` copy ctor (0x1150e0)
- [x] Reconstruct `~Signal()` dtor (0x106520)
- [x] Reconstruct `append(double, unsigned char)` (0x25de80)
- [x] Reconstruct `append(Signal&)` (0x25f310)
- [x] Reconstruct `resizeSamples(unsigned long)` (0x1dff70)
- [x] Reconstruct `checkAllocation()` (0x246950)
- [x] Reconstruct `toJson() const` (0x2a3e40)
- [x] Reconstruct `fromJson(json::value const&)` static (0x2a65d0)
- [x] Reconstruct `getRawData(SampleFormat) const` (0x293ec0)
- [x] Reconstruct `operator==(Signal const&) const` (0x2a9750)
- [x] Remaining ctors (4 more — MarkerBitsPerChannel variants, reserve-only)
- [x] Create `include/zhinst/signal.hpp` with layout + declarations
- [x] Create `src/signal.cpp`
- [x] Update `notes/struct_layouts.md`

### 3c wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/unknowns.md
- [x] Propose TODO.md adjustments

### 3d. AWGCompilerConfig

Trivial config struct — 4 methods total (dtor + 3 string mappers). Used by
Compiler, Prefetch, CustomFunctions, StaticResources. Layout ~0xC0 bytes from dtor
(has strings at +0x30/+0x50, vector<string> at +0x70, path at +0xA8).
**MAJOR DISCOVERY**: AwgDeviceType enum values completely corrected — 9 devices
with codenames; Hirzel={HDAWG,SHFQA,SHFSG,SHFQC_SG,SHFLI}, Cervino={UHFLI,UHFQA,GHFLI,VHFLI}.

- [x] Determine AWGCompilerConfig struct layout from dtor (0xf8080) and consumers
- [x] Reconstruct `getAwgDeviceTypeString(AwgDeviceType)` static (0x270080)
- [x] Reconstruct `getAwgDeviceTypeFromString(string const&)` static (0x270180)
- [x] Reconstruct `getChannelGroupingModeString() const` (0x270b10)
- [x] Reconstruct `~AWGCompilerConfig()` dtor (0xf8080)
- [x] Create `include/zhinst/awg_compiler_config.hpp` with layout + declarations
- [x] Create `src/awg_compiler_config.cpp`
- [x] Correct AwgDeviceType enum in types.hpp + all references (device_constants, cervino_vs_hirzel, struct_layouts)

### 3d wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/struct_layouts.md
- [x] Update notes/unknowns.md (closed #33 — device type 256 = VHFLI "maloja")
- [x] Propose TODO.md adjustments

### 3e. Quick unknowns research

Targeted investigations to close or narrow open questions in unknowns.md.

- [x] **#35**: Fix epsilon value in signal.cpp — confirmed as 1e-12 (read from 0x956350)
- [x] **#34**: Close — already confirmed (seqRegWidth = sequencerReg.width)
- [x] **#37**: Close — behavioral observation, not an open question (checkAllocation is idempotent by design)
- [x] **#33**: Closed in Phase 3d — device type 256 = VHFLI "maloja"
- [x] **#6**: AsmCommands+0x54 = `numChannelGroups_` from AWGCompilerConfig+0x1c (1/2/4). Pre-alloc size for Node vectors. Renamed from `nodeExtraRef_`.
- [x] **#7**: TLS counters ARE reset in `Compiler::compile()`. +0x40 unconditionally at start (0x11f19f); both +0x40 and +0x44 conditionally later (0x1209b9/0x1209c6).

### 3e wrap-up

- [x] Update notes/unknowns.md (closed #6, #7, #33, #34, #35, #37)
- [x] Propose TODO.md adjustments

### Phase 3 wrap-up

- [x] Review all Phase 3 work for consistency (enum mismatch fixed, stale names cleaned)
- [x] Reconsider deferred items: WaveformGenerator has **54 methods** (not 5 as previously noted) — full waveform DSL (sin/cos/drag/rrc/gaussian/placeholder/interleave/etc.). Stays in Phase 5.
- [x] Re-evaluate Phase 4+ ordering — current 4→5→6→7 is correct. No changes needed.

---

## Phase 4: AWGAssembler

### 4a. AWGAssembler base (interface)

- [ ] Reconstruct `AWGAssembler(DeviceConstants const&)` ctor (0x285040)
- [ ] Reconstruct `~AWGAssembler()` dtor (0x285090)
- [ ] Reconstruct `setMemoryOffset(unsigned int)` (0x285120)
- [ ] Reconstruct `getOpcode() const` (0x285140)
- [ ] Reconstruct `getReport() const` (0x285150)
- [ ] Reconstruct `printOpcode(int) const` (0x285170)
- [ ] Create `include/zhinst/awg_assembler.hpp`

### 4b. AWGAssemblerImpl — encoding methods

- [ ] Reconstruct `opcode0(uint, shared_ptr<AsmExpression>)` (0x2895c0)
- [ ] Reconstruct `opcode1(uint, shared_ptr<AsmExpression>)` (0x289860)
- [ ] Reconstruct `opcode2(uint, shared_ptr<AsmExpression>)` (0x289a10)
- [ ] Reconstruct `opcode3(uint, shared_ptr<AsmExpression>)` (0x289c90)
- [ ] Reconstruct `opcode4(uint, shared_ptr<AsmExpression>)` (0x28a010)
- [ ] Reconstruct `opcode5(uint, shared_ptr<AsmExpression>)` (0x28a610)
- [ ] Determine AsmExpression type layout (used by getReg, getVal, all opcodeN)

### 4c. AWGAssemblerImpl — assembly pipeline

- [ ] Reconstruct `assembleAsmList(vector<Assembler> const&)` (0x2876a0)
- [ ] Reconstruct `assembleStringToExpressionsVec(string const&)` (0x286e40)
- [ ] Reconstruct `assembleString(string const&)` (0x286490)
- [ ] Reconstruct `assembleFile(string const&)` (0x285ec0)
- [ ] Reconstruct `assembleExpressions(vector<shared_ptr<AsmExpression>>&, vector<ulong>&)` (0x285620)
- [ ] Reconstruct `evaluate(shared_ptr<AsmExpression> const&)` (0x285b20)

### 4d. AWGAssemblerImpl — helpers

- [ ] Reconstruct `getReg(shared_ptr<AsmExpression> const&)` (0x2892b0)
- [ ] Reconstruct `getVal(shared_ptr<AsmExpression> const&, int)` (0x289370)
- [ ] Reconstruct `errorMessage(string const&)` (0x289070)
- [ ] Reconstruct `parserMessage(int, string const&)` (0x289190)
- [ ] Reconstruct `getReport() const` (0x285bc0)
- [ ] Reconstruct `writeToFile(string const&)` (0x288570)
- [ ] Reconstruct `getAST(string const&)` (0x286ca0)
- [ ] Create `src/awg_assembler_impl.cpp`

### Phase 4 wrap-up

- [ ] Update OVERVIEW.md
- [ ] Update notes/unknowns.md
- [ ] Write `notes/instruction_encoding.md` documenting the binary encoding format
- [ ] Reconsider deferred items: AWGCompilerConfig (3 methods — may be needed for assembler context)
- [ ] Propose TODO.md adjustments

---

## Phase 5: Waveform Complex

### 5a. Waveform base class + File

- [ ] Reconstruct full Waveform ctor (0x2a71e0 — 13 parameters)
- [ ] Reconstruct `Waveform(Waveform const&)` copy ctor (0x2a8ff0)
- [ ] Reconstruct `~Waveform()` dtor (0x1152e0)
- [ ] Reconstruct `toJson() const` (0x2a33c0)
- [ ] Reconstruct `fromJson(json::value const&, DeviceConstants const&)` static (0x2a54b0)
- [ ] Reconstruct `getSizePerDevice() const` (0x1d5c30)
- [ ] Reconstruct `operator==(Waveform const&) const` (0x2a9510)
- [ ] Reconstruct `File::typeToStr(Type)` (0x2a3a90)
- [ ] Reconstruct `File::typeFromStr(string)` (0x2a63c0)
- [ ] Reconstruct `File::operator==(File const&) const` (0x2a9680)
- [ ] Create `include/zhinst/waveform.hpp` (separate from waveform_front.hpp)
- [ ] Create `src/waveform.cpp`
- [ ] Refactor `waveform_front.hpp` to inherit from `waveform.hpp`

### 5a wrap-up

- [ ] Update OVERVIEW.md
- [ ] Update notes/struct_layouts.md
- [ ] Propose TODO.md adjustments

### 5b. WavetableManager\<T\>

- [ ] Determine WavetableManager struct layout (likely vector + map + DeviceConstants*)
- [ ] Reconstruct `newWaveform(...)` (0x29ba00, Front instantiation)
- [ ] Reconstruct `newWaveformFromFile(...)` — both overloads (0x29b560, 0x29b110)
- [ ] Reconstruct `newEmptyWaveform(string const&, DeviceConstants const&)` (0x29aec0)
- [ ] Reconstruct `updateWave(shared_ptr<T>, string const&)` (0x29ccf0)
- [ ] Reconstruct `copyWaveform(shared_ptr<T>)` (0x29c440)
- [ ] Reconstruct `insertWaveform(shared_ptr<T>)` (0x2a1200)
- [ ] Reconstruct `getWaveformForFront(...) const` (0x29c210)
- [ ] Reconstruct `toJson() const` (0x29d780, IR instantiation)
- [ ] Reconstruct `fromJson(...)` (0x29dd10, IR instantiation)
- [ ] Reconstruct `operator==(...)` (0x29e0e0, IR instantiation)
- [ ] Create `include/zhinst/wavetable_manager.hpp`
- [ ] Create `src/wavetable_manager.cpp` (or template in header)

### 5c. WavetableFront + WaveIndexTracker

- [ ] Reconstruct `WavetableFront(DeviceConstants const&, AddressImpl<uint>, ulong, path const&)` ctor (0x29ab10)
- [ ] Reconstruct `~WavetableFront()` dtor (0x29a940)
- [ ] Reconstruct `begin() const` / `end() const` (0x29ad00, 0x29ad20)
- [ ] Reconstruct `getMemorySize() const` (0x29adc0)
- [ ] Reconstruct `toString() const` (0x29bd90)
- [ ] Reconstruct `updateDioTableUsage(ulong, ulong)` (0x29ca10)
- [ ] Reconstruct `setLineNr(int)` (0x29ce10)
- [ ] Reconstruct WavetableFront waveform methods: newWaveform, newWaveformFromFile, copyWaveform, updateWave, loadWaveform, assignWaveIndex (8 methods, 0x29b0e0–0x29cc70)
- [ ] Reconstruct `WaveIndexTracker(int)` ctor (0x29a5e0)
- [ ] Reconstruct `assign(int)` / `assignAuto(int)` (0x29a600, 0x29a620)
- [ ] Reconstruct `getNextAutoIndex()` / `hasGaps()` / `usedWaveIndices()` (0x29a880, 0x29a8e0, 0x29a7d0)
- [ ] Reconstruct template ctors for WaveIndexTracker (0x29d000, 0x29d410)
- [ ] Reconstruct `WavetableException` ctor/dtor/what (0x29a840, 0x29f980, 0x29f9d0)
- [ ] Update `include/zhinst/wavetable_front.hpp`
- [ ] Create `src/wavetable_front.cpp`

### 5d. WaveformIR + WavetableIR

- [ ] Reconstruct `WaveformIR(shared_ptr<WaveformFront>)` ctor (0x114da0)
- [ ] Reconstruct `WaveformIR::toJsonElement(SampleFormat)` (0x2c5440)
- [ ] Determine WavetableIR struct layout
- [ ] Reconstruct `allocateWaveforms(bool)` (0x29e340)
- [ ] Reconstruct `updateWaveforms(bool, bool)` (0x29ed10)
- [ ] Reconstruct `alignWaveformSizes()` (0x29f150)
- [ ] Reconstruct `assignWaveIndexImplicit()` (0x29e8a0)
- [ ] Reconstruct `allocateWaveformsForFifo()` (0x29ed30)
- [ ] Reconstruct `assignWaveformAllocationSizes()` (0x29f1d0)
- [ ] Reconstruct `getJsonIndex(SampleFormat) const` (0x29f480)
- [ ] Reconstruct `getFirstWaveformOffset() const` / `getNextSegmentAddress() const` (0x29e330, 0x29e320)
- [ ] Reconstruct `begin()` / `end()` / `size()` (0x29e270, 0x29e280, 0x29e290)
- [ ] Reconstruct `toJson() const` (0x29d670)
- [ ] Reconstruct `operator==(WavetableIR const&) const` (0x29e090)
- [ ] Create `include/zhinst/waveform_ir.hpp` and `include/zhinst/wavetable_ir.hpp`
- [ ] Create `src/waveform_ir.cpp` and `src/wavetable_ir.cpp`

### Phase 5 wrap-up

- [ ] Update OVERVIEW.md
- [ ] Update notes/struct_layouts.md
- [ ] Update notes/unknowns.md
- [ ] Reconsider deferred items: WaveformGenerator (5 methods + exception types — closely related)
- [ ] Propose TODO.md adjustments

---

## Phase 6: AsmOptimize

### 6a. Query helpers

- [ ] Reconstruct `isRead(Assembler const&, AsmRegister) const` (0x27d900)
- [ ] Reconstruct `isWritten(Assembler const&, AsmRegister) const` (0x27d960)
- [ ] Reconstruct `isLabelCalled(string const&, iter)` (0x27d9c0)
- [ ] Reconstruct `getNextActionForReg(iter, AsmRegister)` (0x281a10)
- [ ] Reconstruct `registerIsNeverWritten(AsmList&, AsmRegister, iter, iter) const` (0x280f50)

### 6b. Optimization passes

- [ ] Reconstruct `prepareResources(AsmList const&) const` (0x27dab0)
- [ ] Reconstruct `optimizePreWaveform(AsmList const&)` (0x27db40)
- [ ] Reconstruct `optimizePostWaveform(AsmList const&)` (0x27ddf0)
- [ ] Reconstruct `deadCodeElimination()` (0x27dbd0)
- [ ] Reconstruct `oneStepJumpElimination()` (0x27e040)
- [ ] Reconstruct `removeUnusedLabels()` (0x27e1c0)
- [ ] Reconstruct `mergeLabels()` (0x27e330)
- [ ] Reconstruct `mergeRegisterZeroing()` (0x27e640)
- [ ] Reconstruct `removeUnusedRegs()` (0x27e760)
- [ ] Reconstruct `reportUserMessages()` (0x280b60)
- [ ] Reconstruct `simplifyAssign(iter)` (0x280e10)

### 6c. Register allocator

- [ ] Determine `LiveRange` and `PhysicalRegister` internal type layouts
- [ ] Reconstruct `registerAllocation(unsigned long)` (0x27ebb0)
- [ ] Reconstruct `splitConstRegisters(unsigned long)` (0x280440)
- [ ] Reconstruct `splitReg(AsmList&, AsmRegister, iter, iter)` (0x281000)
- [ ] Reconstruct `registerUpdate(vector<int> const&, AsmRegister, AsmRegister)` (0x281680)

### 6d. Exceptions + housekeeping

- [ ] Reconstruct `~AsmOptimize()` dtor (0x123200)
- [ ] Reconstruct `OptimizeException` dtor/what (0x281e00, 0x281e90)
- [ ] Create `include/zhinst/asm_optimize.hpp`
- [ ] Create `src/asm_optimize.cpp`

### Phase 6 wrap-up

- [ ] Update OVERVIEW.md
- [ ] Update notes/unknowns.md (close #5 re: isWaveformCmd, #18 re: outputs vector)
- [ ] Write `notes/optimization_passes.md`
- [ ] Propose TODO.md adjustments

---

## Phase 7: Integration — Compilation Pipeline

- [ ] Disassemble `Compiler::runPrefetcher`
- [ ] Disassemble `FrontEndLoweringFacade::lower`
- [ ] Disassemble `Prefetch::Prefetch` constructor
- [ ] Disassemble `Prefetch::optimize(shared_ptr<Node>)` (0x1cdae0)
- [ ] Disassemble `Prefetch::optimizeSync(shared_ptr<Node>)` (0x1cf7b0)
- [ ] Disassemble `Prefetch::optimizeCwvf(shared_ptr<Node>)` (0x1cfc70)
- [ ] Disassemble `CustomFunctions::CustomFunctions` constructor
- [ ] Map the overall compilation pipeline flow
- [ ] Document in `notes/pipeline.md`

### Phase 7 wrap-up

- [ ] Final review of OVERVIEW.md
- [ ] Final review of all notes/ files
- [ ] Identify further components worth reconstructing (or declare scope complete)

---

## Deferred / Low Priority

- [ ] Full reconstruction of `syncCervino()` (~1000 asm lines)
- [ ] Full reconstruction of `unsyncCervino()` (~1000 asm lines)
- [ ] Full reconstruction of `addi32()` (32-bit immediate edge cases)
- [ ] Full reconstruction of `parseStringToAsmList` (~7000 bytes)
- [ ] WaveformGenerator (54 methods + exception types) — full waveform DSL; reconsider at Phase 5 wrap-up
- [ ] AWGCompilerConfig (3 methods: dtor + 2 string getters) — reconsider at Phase 3/4 wrap-up
- [ ] Add `CMakeLists.txt` to compile reconstructed code as a validation step
- [ ] Write comparison tests against the real `.so`
