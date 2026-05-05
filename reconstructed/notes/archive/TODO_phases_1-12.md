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
- [x] Create `src/asm/assembler.cpp` with all above implementations
- [x] Identify the 3 new opcodes: 0x60000007, 0xF1000000, 0xFB000000

### 2b. AsmEntry / AsmList::Asm (0xA8 bytes)

- [x] Confirm the 0xA8-byte layout against additional call sites
- [x] Enumerate `zhinst::AsmList::` symbols; determine container type
- [x] Understand the `AsmList::Asm` nested type (was "AsmEntry")
- [x] Reconstruct AsmList methods (append, print, serialize, deserialize, maxRegister)
- [x] Reconstruct AsmList::Asm methods (dtor, serializeNodeToJsonString, createUniqueID)
- [x] Update `asm_list.hpp` and create `src/asm/asm_list.cpp`

### 2c. PlayConfig Methods

- [x] Trace address loads in `genPlayConfig` / `asmPlay` to find .rodata locations
- [x] Extract all shift/mask constant values
- [x] Update `play_config.hpp` and `src/asm/asm_commands.cpp`
- [x] Implement `encodeCwvf()` — bit-packing (0x1dc500)
- [x] Implement `operator!=()` (0x1d5770)
- [x] Implement `toJson()` (0x269d60)
- [x] Implement `fromJson()` (0x26b440) — discovered during enumeration
- [x] Create `src/waveform/play_config.cpp`

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
- [x] Create `src/ast/node.cpp`
- [x] Write `notes/node_tree_structure.md`
- [x] Declare and reconstruct `toJson()` (0x264b90)
- [x] Declare and reconstruct `fromJson()` (0x268280) — static
- [x] Declare and reconstruct `installPointers()` (0x269020) — static

### 2e. WaveformFront

- [x] Enumerate all `zhinst::WaveformFront::` symbols
- [x] Determine name/string access mechanism (operator*, getName(), etc.)
- [x] Fill in unknown gaps between known field offsets
- [x] Reconstruct key methods (toString, copy-rename ctor, dtor)
- [x] Update `waveform_front.hpp` and create `src/waveform/waveform_front.cpp`

### 2f. Value / Immediate Methods

- [x] Determine if Value is std::variant, tagged union, or other
- [x] Reconstruct `toInt()`, `toDouble()`, variant visitation
- [x] Update `value.hpp`
- [x] Enumerate remaining `zhinst::Value::` and `zhinst::Immediate::` symbols
- [x] Move method bodies from header to `src/ast/value.cpp`
- [x] Reconstruct `Value(string const&)` constructor (0x22c2b0)
- [x] Reconstruct `operator<<(ostream&, Immediate)` (0x290b90)
- [x] Reconstruct `operator<<(ostream&, AddressImpl<uint>)` (0x1c7ce0)
- [x] Reconstruct `ValueException` ctor/dtor/what (0x16e7f0, 0x16e850, 0x16f110)
- [x] Create `src/ast/value.cpp`

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
- [x] Create `include/zhinst/device/device_constants.hpp` with full layout
- [x] Create `src/device/device_constants.cpp` with `getDeviceConstants()` implementation
- [x] Update `notes/struct_layouts.md` with DeviceConstants offset table

### 3a wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/unknowns.md
- [x] Propose TODO.md adjustments

### 3b. ErrorMessages / ResourcesException

BSS message table at 0xb84c38, ~53 symbols (mostly template instantiations of
`format<>()`). Core logic is small: `operator[]` lookup + boost::format wrapper.

- [x] Dump BSS message table at 0xb84c38 — extract all ErrorMessageT enum values and format strings
- [x] Reconstruct `ErrorMessages::operator[](ErrorMessageT) const` (0x108380)
- [x] Reconstruct `ErrorMessages::format<>()` no-arg version (0x15d0d0)
- [x] Document the template instantiation pattern (40+ specializations — document, don't reconstruct all)
- [x] Reconstruct `getApiErrorMessage(ZIResult_enum)` (0x2e4820)
- [x] Reconstruct `ResourcesException` ctor/dtor/what (0x1e3a20, 0x1f12f0, 0x1f1340)
- [x] Create `include/zhinst/core/error_messages.hpp` with ErrorMessageT enum + declarations
- [x] Create `src/core/error_messages.cpp`
- [x] Note the apiErrorMessages BSS table at 0xb85230

### 3b wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/unknowns.md (close #13 re: format strings)
- [x] Propose TODO.md adjustments

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
- [x] Create `include/zhinst/waveform/signal.hpp` with layout + declarations
- [x] Create `src/waveform/signal.cpp`
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
- [x] Create `include/zhinst/codegen/awg_compiler_config.hpp` with layout + declarations
- [x] Create `src/codegen/awg_compiler_config.cpp`
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

## Phase 4: AWGAssembler (COMPLETE)

### 4a. AWGAssembler base (interface)

- [x] Reconstruct `AWGAssembler(DeviceConstants const&)` ctor (0x285040)
- [x] Reconstruct `~AWGAssembler()` dtor (0x285090)
- [x] Reconstruct `setMemoryOffset(unsigned int)` (0x285120)
- [x] Reconstruct `getOpcode() const` (0x285140)
- [x] Reconstruct `getReport() const` (0x285150)
- [x] Reconstruct `printOpcode(int) const` (0x285170)
- [x] Create `include/zhinst/codegen/awg_assembler.hpp`

### 4b. AWGAssemblerImpl — encoding methods

- [x] Reconstruct `opcode0(uint, shared_ptr<AsmExpression>)` (0x2895c0)
- [x] Reconstruct `opcode1(uint, shared_ptr<AsmExpression>)` (0x289860)
- [x] Reconstruct `opcode2(uint, shared_ptr<AsmExpression>)` (0x289a10)
- [x] Reconstruct `opcode3(uint, shared_ptr<AsmExpression>)` (0x289c90)
- [x] Reconstruct `opcode4(uint, shared_ptr<AsmExpression>)` (0x28a010)
- [x] Reconstruct `opcode5(uint, shared_ptr<AsmExpression>)` (0x28a610)
- [x] Determine AsmExpression type layout (used by getReg, getVal, all opcodeN)

### 4c. AWGAssemblerImpl — assembly pipeline

- [x] Reconstruct `assembleAsmList(vector<Assembler> const&)` (0x2876a0)
- [x] Reconstruct `assembleStringToExpressionsVec(string const&)` (0x286e40)
- [x] Reconstruct `assembleString(string const&)` (0x286490)
- [x] Reconstruct `assembleFile(string const&)` (0x285ec0)
- [x] Reconstruct `assembleExpressions(vector<shared_ptr<AsmExpression>>&, vector<ulong>&)` (0x285620)
- [x] Reconstruct `evaluate(shared_ptr<AsmExpression> const&)` (0x285b20)

### 4d. AWGAssemblerImpl — helpers

- [x] Reconstruct `getReg(shared_ptr<AsmExpression> const&)` (0x2892b0)
- [x] Reconstruct `getVal(shared_ptr<AsmExpression> const&, int)` (0x289370)
- [x] Reconstruct `errorMessage(string const&)` (0x289070)
- [x] Reconstruct `parserMessage(int, string const&)` (0x289190)
- [x] Reconstruct `getReport() const` (0x285bc0)
- [x] Reconstruct `writeToFile(string const&)` (0x288570)
- [x] Reconstruct `getAST(string const&)` (0x286ca0)
- [x] Create `src/codegen/awg_assembler_impl.cpp`

### Phase 4 wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/unknowns.md (added #39–#45)
- [x] Write `notes/opcode_encoding.md` documenting the binary encoding format
- [x] Update notes/struct_layouts.md (AWGAssembler, AWGAssemblerImpl, AsmExpression, Message)
- [x] Propose TODO.md adjustments

---

## Phase 5: Waveform Complex (COMPLETE)

### 5a. Waveform base class + File

- [x] Reconstruct full Waveform ctor (0x2a71e0 — 13 parameters)
- [x] Reconstruct `Waveform(Waveform const&)` copy ctor (0x2a8ff0)
- [x] Reconstruct `~Waveform()` dtor (0x1152e0)
- [x] Reconstruct `toJson() const` (0x2a33c0)
- [x] Reconstruct `fromJson(json::value const&, DeviceConstants const&)` static (0x2a54b0)
- [x] Reconstruct `getSizePerDevice() const` (0x1d5c30)
- [x] Reconstruct `operator==(Waveform const&) const` (0x2a9510)
- [x] Reconstruct `File::typeToStr(Type)` (0x2a3a90)
- [x] Reconstruct `File::typeFromStr(string)` (0x2a63c0)
- [x] Reconstruct `File::operator==(File const&) const` (0x2a9680)
- [x] Reconstruct copy-rename ctor (0x114f10)
- [x] Create `include/zhinst/waveform/waveform.hpp` (separate from waveform_front.hpp)
- [x] Create `src/waveform/waveform.cpp`
- [x] Refactor `waveform_front.hpp` to inherit from `waveform.hpp`

### 5b. WavetableManager\<T\>

- [x] Determine WavetableManager struct layout (0x48 bytes: lineNr, counter, unordered_map, vector)
- [x] Reconstruct `newWaveform(...)` (0x29ba00, Front instantiation)
- [x] Reconstruct `newWaveformFromFile(...)` — both overloads (0x29b560, 0x29b110)
- [x] Reconstruct `newEmptyWaveform(string const&, DeviceConstants const&)` (0x29aec0)
- [x] Reconstruct `updateWave(shared_ptr<T>, string const&)` (0x29ccf0)
- [x] Reconstruct `copyWaveform(shared_ptr<T>)` (0x29c440)
- [x] Reconstruct `insertWaveform(shared_ptr<T>)` (0x2a1200 / 0x29d140)
- [x] Reconstruct `getWaveformForFront(...) const` (0x29c210)
- [x] Reconstruct `toJson() const` (0x29d780, IR instantiation)
- [x] Reconstruct `fromJson(...)` (0x29dd10, IR instantiation)
- [x] Reconstruct `operator==(...)` (0x29e0e0, IR instantiation)
- [x] Create `src/waveform/wavetable_manager_front.cpp` and `src/waveform/wavetable_manager_ir.cpp`

### 5c. WavetableFront + WaveIndexTracker

- [x] Reconstruct `WavetableFront(DeviceConstants const&, AddressImpl<uint>, ulong, path const&)` ctor (0x29ab10)
- [x] Reconstruct `~WavetableFront()` dtor (0x29a940)
- [x] Reconstruct `begin() const` / `end() const` (0x29ad00, 0x29ad20)
- [x] Reconstruct `getMemorySize() const` (0x29adc0)
- [x] Reconstruct `toString() const` (0x29bd90)
- [x] Reconstruct `updateDioTableUsage(ulong, ulong)` (0x29ca10)
- [x] Reconstruct `setLineNr(int)` (0x29ce10)
- [x] Reconstruct WavetableFront waveform methods: newWaveform, newWaveformFromFile, copyWaveform, updateWave, loadWaveform, assignWaveIndex (8+ methods)
- [x] Reconstruct `WaveIndexTracker(int)` ctor (0x29a5e0)
- [x] Reconstruct `assign(int)` / `assignAuto(int)` (0x29a600, 0x29a620)
- [x] Reconstruct `getNextAutoIndex()` / `hasGaps()` / `usedWaveIndices()` (0x29a880, 0x29a8e0, 0x29a7d0)
- [x] Reconstruct template ctors for WaveIndexTracker (0x29d000, 0x29d410)
- [x] Reconstruct `WavetableException` ctor/dtor/what (0x29a840, 0x29f980, 0x29f9d0)
- [x] Update `include/zhinst/waveform/wavetable_front.hpp`
- [x] Create `src/waveform/wavetable_front.cpp` and `src/waveform/wave_index_tracker.cpp`

### 5d. WaveformIR + WavetableIR

- [x] Reconstruct `WaveformIR(shared_ptr<WaveformFront>)` ctor (0x114da0)
- [x] Reconstruct `WaveformIR(shared_ptr<Waveform>)` ctor (0x2a9240)
- [x] Reconstruct `WaveformIR::toJsonElement(SampleFormat)` (0x2c5440)
- [x] Determine WavetableIR struct layout (0xC8 bytes)
- [x] Reconstruct `allocateWaveforms(bool)` (0x29e340)
- [x] Reconstruct `updateWaveforms(bool, bool)` (0x29ed10)
- [x] Reconstruct `alignWaveformSizes()` (0x29f150)
- [x] Reconstruct `assignWaveIndexImplicit()` (0x29e8a0)
- [x] Reconstruct `allocateWaveformsForFifo()` (0x29ed30)
- [x] Reconstruct `assignWaveformAllocationSizes()` (0x29f1d0)
- [x] Reconstruct `getJsonIndex(SampleFormat) const` (0x29f480)
- [x] Reconstruct `getFirstWaveformOffset() const` / `getNextSegmentAddress() const` (0x29e330, 0x29e320)
- [x] Reconstruct `begin()` / `end()` / `size()` (0x29e270, 0x29e280, 0x29e290)
- [x] Reconstruct `toJson() const` (0x29d670)
- [x] Reconstruct `operator==(WavetableIR const&) const` (0x29e090)
- [x] Reconstruct `forEachUsedWaveform()` (0x29e5e0)
- [x] Reconstruct `setUsedWaveforms()` (0x29ece0)
- [x] Reconstruct `loadWaveform()` (0x29f310)
- [x] Reconstruct `fromJson()` (0x29db10)
- [x] Create `include/zhinst/waveform/waveform_ir.hpp` and `include/zhinst/waveform/wavetable_ir.hpp`
- [x] Create `src/waveform/waveform_ir.cpp` and `src/waveform/wavetable_ir.cpp`

### Phase 5 wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/struct_layouts.md
- [x] Update notes/unknowns.md
- [x] Reconsider deferred items: WaveformGenerator (54 methods) stays deferred; CachedParser/MemoryAllocator added
- [x] Propose TODO.md adjustments

---

## Phase 6: AsmOptimize (COMPLETE)

### 6a. Query helpers

- [x] Reconstruct `isRead(Assembler const&, AsmRegister) const` (0x27d900)
- [x] Reconstruct `isWritten(Assembler const&, AsmRegister) const` (0x27d960)
- [x] Reconstruct `isLabelCalled(string const&, iter)` (0x27d9c0)
- [x] Reconstruct `getNextActionForReg(iter, AsmRegister)` (0x281a10)
- [x] Reconstruct `registerIsNeverWritten(AsmList&, AsmRegister, iter, iter) const` (0x280f50)

### 6b. Optimization passes

- [x] Reconstruct `prepareResources(AsmList const&) const` (0x27dab0)
- [x] Reconstruct `optimizePreWaveform(AsmList const&)` (0x27db40)
- [x] Reconstruct `optimizePostWaveform(AsmList const&)` (0x27ddf0)
- [x] Reconstruct `deadCodeElimination()` (0x27dbd0)
- [x] Reconstruct `oneStepJumpElimination()` (0x27e040)
- [x] Reconstruct `removeUnusedLabels()` (0x27e1c0)
- [x] Reconstruct `mergeLabels()` (0x27e330)
- [x] Reconstruct `mergeRegisterZeroing()` (0x27e640)
- [x] Reconstruct `removeUnusedRegs()` (0x27e760) — approximate
- [x] Reconstruct `reportUserMessages()` (0x280b60)
- [x] Reconstruct `simplifyAssign(iter)` (0x280e10)

### 6c. Register allocator

- [x] Determine `LiveRange` and `PhysicalRegister` internal type layouts
- [x] Reconstruct `registerAllocation(unsigned long)` (0x27ebb0) — approximate
- [x] Reconstruct `splitConstRegisters(unsigned long)` (0x280440) — approximate
- [x] Reconstruct `splitReg(AsmList&, AsmRegister, iter, iter)` (0x281000)
- [x] Reconstruct `registerUpdate(vector<int> const&, AsmRegister, AsmRegister)` (0x281680)

### 6d. Exceptions + housekeeping

- [x] Reconstruct `~AsmOptimize()` dtor (0x123200)
- [x] Reconstruct `OptimizeException` dtor/what (0x281e00, 0x281e90)
- [x] Create `include/zhinst/asm/asm_optimize.hpp`
- [x] Create `src/asm/asm_optimize.cpp`

### Phase 6 wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/unknowns.md (added #46–#53)
- [x] Write `notes/optimization_passes.md`
- [x] Update notes/struct_layouts.md (AsmOptimize, OptimizeException)
- [x] Propose TODO.md adjustments

---

## Phase 7: Integration — Compilation Pipeline

### 7a. Compiler shell + pipeline glue

- [x] Determine Compiler struct layout (~0x138 bytes) from ctor/dtor
- [x] Reconstruct `Compiler::Compiler(AWGCompilerConfig const&, DeviceConstants const&, shared_ptr<WavetableFront>)` (0x11d080)
- [x] Reconstruct `Compiler::~Compiler()` (0x103660)
- [x] Reconstruct `Compiler::reset()` (0x11dfe0)
- [x] Reconstruct `Compiler::compile(string const&)` (0x11f150) — master pipeline (~13KB)
- [x] Reconstruct `Compiler::runPrefetcher(...)` (0x11dff0) — prefetch orchestrator (~2.8KB)
- [x] Reconstruct `Compiler::parse(string const&)` (0x11d9b0)
- [x] Reconstruct `Compiler::unifyLineEndings(string const&) const` (0x11d720)
- [x] Reconstruct `Compiler::printAST(shared_ptr<Expression>, string const&)` (0x122640) — debug, approximate
- [x] Reconstruct getter/setter methods (setCancelCallback, setProgressCallback, getNodeAccessList, getNodeToModeMap, getChannelInfo, usedDeviceSampleRate, getCompileMessages, setLineNr, getLineMap)
- [x] Reconstruct `FrontEndLoweringFacade::lower(...)` (0x1c1da0) — thin facade (~1KB)
- [x] Reconstruct `CompilerMessage::str(bool) const` (0x104340)
- [x] Reconstruct `CompilerException` ctor/dtor/what (0x11dec0, 0x11df20, 0x123bd0)
- [x] Reconstruct `CompilerMessageCollection::compilerMessage(...)` (0x12b750) + errorMessage/warningMessage/infoMessage/parserMessage/reset
- [x] Create `include/zhinst/core/compiler_message.hpp`, `src/core/compiler_message.cpp`
- [x] Create `include/zhinst/codegen/compiler.hpp`, `src/codegen/compiler.cpp`
- [x] Write `notes/pipeline.md` documenting the full compilation flow

#### 7a wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/unknowns.md (added #54–#60)
- [x] Update notes/struct_layouts.md (Compiler, CompilerMessage, CompilerMessageCollection, CompilerException)
- [x] Propose TODO.md adjustments

### 7b. Cache

- [x] Determine Cache struct layout (~0x28 bytes) from ctor
- [x] Reconstruct `Cache::Cache(AddressImpl<uint>, int, bool)` (0x282920)
- [x] Reconstruct `Cache::allocate(...)` — both overloads (0x282a30, 0x282be0)
- [x] Reconstruct `Cache::free(shared_ptr<Cache::Pointer>)` (0x283690)
- [x] Reconstruct `Cache::getBestPosition(...)` (0x282cf0)
- [x] Reconstruct `Cache::play(...)` / `memoryWrite(...)` / `reuse(...)` / `stillInMemory(...)` (0x2834c0, 0x283020, 0x2833d0, 0x2832e0)
- [x] Reconstruct `Cache::Pointer::str() const` (0x283c30)
- [x] Reconstruct `Cache::print() const` / `resetPlay()` / getters (0x283b50, 0x283640, 0x282940–0x2829a0)
- [x] Create `include/zhinst/runtime/cache.hpp`, `src/runtime/cache.cpp`

#### 7b wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/unknowns.md (added #61–#65)
- [x] Update notes/struct_layouts.md (Cache, Cache::Pointer, CacheException)
- [x] Propose TODO.md adjustments

### 7c. Prefetch — tree preparation + optimization

- [x] Determine Prefetch struct layout (~0x160 bytes) from ctor/dtor
- [x] Reconstruct `Prefetch::Prefetch(...)` ctor (0x1c5850) and `~Prefetch()` dtor (0x11eed0)
- [x] Reconstruct tree prep: `preparePlays()` (0x1c8740), `prepareTree()` (0x1c8870), `countBranches()` (0x1c9b30), `definePlaySize()` (0x1ca370)
- [x] Reconstruct optimization: `optimize()` (0x1cdae0, 7.4KB), `optimizeSync()` (0x1cf7b0), `optimizeCwvf()` (0x1cfc70), `globalCwvf()` (0x1d5620)
- [x] Reconstruct wave management: `determineFixedWaves()` (0x1cb200), `collectUsedWaves()` (0x1d31c0), `getUsedWavesForDevice()` (0x1d2d60)
- [x] Reconstruct load placement: `placeLoads()` (0x1cbf60), `moveLoadsToFront()` (0x1ccad0), `createLoad()` (0x1d4a10), `mergeLoads()` (0x1d5040), `assignLoad()` (0x1d53a0), `linkLoad()` (0x1d33f0)
- [x] Reconstruct helpers: `backwardTree()` (0x1d57d0), `removeBranches()` (0x1d3530), `expandSetVar()` (0x1d3af0), `findLockedPlay()` (0x1d3dd0), `sameLoads()` (0x1d5e20), `nodeByCachePointer()` (0x1d60d0)
- [x] Reconstruct `allocate()` (0x1d0fb0, 7.6KB)
- [x] Create `include/zhinst/codegen/prefetch.hpp`, `src/codegen/prefetch.cpp`

#### 7c wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/unknowns.md
- [x] Update notes/struct_layouts.md
- [x] Propose TODO.md adjustments

### 7d. Prefetch — command emission (COMPLETE)

- [x] Reconstruct `placeSingleCommand()` (0x1d7940, 20KB)
- [x] Reconstruct `placeCommands()` (0x1d6680), `fillInPlaceholders()` (0x1d65c0), `findPlaceholder()` (0x1d6b50)
- [x] Reconstruct `wvfImpl()` (0x1d6ca0), `wvfRegImpl()` (0x1d7020), `wvfs()` (0x1d73e0), `insertPlay()` (0x1def50)
- [x] Reconstruct `splitPlay()` (0x1dd1a0, 7.6KB), `needsNewCwvf()` (0x1dc620)
- [x] Reconstruct `getUsedChannels()` (0x1df2f0), `getUsedFourChannelMode()` (0x1df400)
- [x] Reconstruct `print()` (0x1c5dd0, 8.4KB), `getUsedCache()` (0x1c7eb0)
- [x] Reconstruct `getMemoryHighWatermark()` (0x1cc650), `getRequiredMemory()` (0x1cc930), `clampToCache()` (0x1d6c40)

#### 7d wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/unknowns.md
- [x] Update notes/struct_layouts.md
- [x] Propose TODO.md adjustments

### Phase 7e. DeviceConstants layout revision (COMPLETE)

- [x] Catalog all DeviceConstants field accesses across all reconstructed code
- [x] Verify ambiguous accesses against binary disassembly
- [x] Propose new layout with semantic field names (reviewed with user)
- [x] Update device_constants.hpp — remove RegisterBank, flat scalar fields
- [x] Update device_constants.cpp factory values
- [x] Update all consumer .cpp files (prefetch_emit, prefetch_helpers, prefetch, prefetch_splitplay, prefetch_placesingle, wavetable_ir, prefetch.hpp)
- [x] Update struct_layouts.md, unknowns.md (closed #31, #72)
- [x] Update OVERVIEW.md

### Phase 7 final wrap-up

- [x] Final review of OVERVIEW.md
- [x] Final review of all notes/ files
- [x] Identify further components worth reconstructing → Phases 8–12 planned

---

## Phase 8: Cleanup & Quick Wins

### 8a. AWGCompilerConfig field updates — COMPLETE

- [x] Add `isHirzel` (bool) at +0x18, `cacheType` (uint8_t) at +0x19
- [x] Add `debugFlags` (uint32_t) at +0x90, split unknown_88[4]
- [x] Update awg_compiler_config.hpp struct fields
- [x] Update struct_layouts.md
- [x] Fix source references (prefetch.cpp, prefetch_prepare.cpp)

### 8b. MemoryAllocator — COMPLETE

15 symbols total (5 zhinst::, 10 std:: deque internals). CL-aware allocator used by WavetableIR.

- [x] Enumerate symbols, determine layout (MemoryBlock 12B, MemoryAllocator 0x70)
- [x] Reconstruct dtor @0x29f2d0, allocateCLAligned (inlined), allocateReloadingCL (inlined)
- [x] Reconstruct allocateFirstSuitableFreeBlock<T> (3 instantiations @0x2aa960/0x2aac80/0x2ad190)
- [x] Create header + implementation files
- [x] Update struct_layouts.md, OVERVIEW.md, notes/memory_allocator_analysis.md
- [x] Update wavetable_ir.cpp to use proper MemoryAllocator calls
      (DONE — already integrated: ctor at line 530, allocateCLAligned
       at lines 545/596, allocateReloadingCL at line 638; 6 total
       allocator method calls. No manual allocation code remains.)

### 8c. CancelCallback + ProgressCallback — COMPLETE

CancelCallback: pure virtual, no vtable/typeinfo in binary (Python binding layer).
ProgressCallback: 8-byte virtual base, vtable @0xb03858, empty defaults.

- [x] Determine vtable layout (ProgressCallback: dtor + setProgress; CancelCallback: no binary vtable)
- [x] Reconstruct both interfaces in callbacks.hpp + callbacks.cpp
- [x] CancelCallback already forward-declared in all consumer headers (no update needed)

### 8d. RawWaveData hierarchy (COMPLETE)

24 symbols. PlaceHolder/Cervino/Hirzel16 from Signal::getRawData.

- [x] Enumerate all RawWave* symbols
- [x] Determine base class layout + vtable
- [x] Reconstruct PlaceHolder, Cervino, Hirzel16 subclasses
- [x] Create header + implementation files

### 8e. Close/triage open unknowns — COMPLETE

Targeted binary checks to close or narrow open questions.

- [x] Close #36 (RawWaveData — done in 8d), #51 (CancelCallback — done in 8c),
      #60 (debugFlags — done in 8a), #71 (cacheType — done in 8a)
- [x] Close #5 (isWaveformCmd flags Command values 3,4,5 — confirmed)
- [x] Close #19 (Node+0x18/+0x20 = two weak_ptrs, dtor calls __release_weak)
- [x] Close #20 (AsmRegister: valid=non-negative, value=max(input,0), ctor @0x28eb40)
- [x] Close #29 (parseStringToAsmList: hardcoded HDAWG, convenience function)
- [x] Close #46 (AsmOptimize ctor: always inlined, takes messages+numRegs+devicePtr)
- [x] Close #66-67 (Full NodeType enum: 14 values, 0x80=unlock, 0x8000=awg_ready)
- [x] Close #73 (Node+0x44 IS NodeType type — "refCount" was wrong)
- [x] Close #74 (Node+0x64 = PlayConfig.now at config+0x1C)
- [x] Narrow #18 (outputs vector: population inlined, copy/move visible at 0x122e61/0x125eac)
- [x] Update unknowns.md

#### Phase 8 wrap-up — COMPLETE

- [x] Update OVERVIEW.md (8a-8d class descriptions added)
- [x] Update notes/ (struct_layouts.md, unknowns.md, memory_allocator_analysis.md)
- [x] 13 unknowns closed, 1 narrowed in Phase 8e

---

## Phase 9: AWGAssembler Completion

### 9a. ElfWriter

ELF binary output writer. 7 own methods + 2 anonymous namespace helpers.
No vtable (non-virtual). Layout needs determination from ctor.

- [x] Determine ElfWriter layout from ctor @0x2934a0 (takes uint16_t machineType)
- [x] Reconstruct prepareHeader @0x2936b0
- [x] Reconstruct addCode @0x293710 (adds vector<uint32_t> as .text section)
- [x] Reconstruct addData @0x293990 (adds raw data as named section)
- [x] Reconstruct addWaveform @0x2939f0 (adds WaveformIR data with SampleFormat)
- [x] Reconstruct writeFile @0x294030 (ostream overload) and @0x2942a0 (string overload)
- [x] Reconstruct setMemoryOffset @0x294410
- [x] Reconstruct writeWavesToElfMapped (inlined into writeToStream @0x108cc0, in write_waves_to_elf.cpp)
- [x] Reconstruct writeWavesToElfAbsolute (inlined into writeToStream @0x108cc0, in write_waves_to_elf.cpp)
- [x] Create elf_writer.hpp + elf_writer.cpp
- [x] Sub-phase wrap-up

### 9b. AsmParserContext

Parser context for flex/bison assembly parser. ~20 accessor methods (most trivial)
+ 4 free functions (addNode, addCommand, asmerror, asmparse).

- [x] Determine AsmParserContext layout from methods (all at 0x28e7a0-0x28ead0)
- [x] Reconstruct all accessor methods (isComment, isLineComment, start/endLineComment,
      disableOpt/enableOpt/doOpt, start/endBlockComment, hadSyntaxError/set/clear,
      currentLineNumber/incrementLineNumber, programCounter/incrementProgramCounter,
      trackedStringCopy, cleanStringCopies, Label ctor, Label::operator==)
- [x] Reconstruct setErrorCallback @0x28e610 and raiseError @0x28e690
- [x] Reconstruct addLabel @0x28ea60 and hasLabel @0x28ea80
- [x] Reconstruct addNode @0x28bfd0 (free function)
- [x] Reconstruct addCommand @0x28c600 (free function)
- [x] Reconstruct asmerror @0x292a60 (bison error handler)
- [x] Determine asmparse @0x292b50 scope — ~217KB bison-generated, DEFERRED
- [x] Create asm_parser_context.hpp + asm_parser_context.cpp
- [x] Sub-phase wrap-up

### 9c. parseStringToAsmList

Deserializer: string → AsmList. ~7632 bytes at 0x266160–0x268130.
Uses hardcoded HDAWG device constants.

- [x] Reconstruct parseStringToAsmList @0x266160
- [x] Add to asm_list.cpp (it's a static method on AsmList)
- [x] Sub-phase wrap-up

#### Phase 9 wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/
- [x] Propose TODO.md adjustments

---

## Phase 10: Scope & Symbol Management + AsmExpression

### 10c. AsmExpression (quick win — 4 symbols)

0xa8-byte parse tree node used by AWGAssembler's flex/bison parser.
Already partially documented in awg_assembler_impl.hpp.

- [x] Determine full 0xa8-byte layout from dtor @0x28b1f0 + consumers
- [x] Reconstruct dtor @0x28b1f0
- [x] Reconstruct createArgList @0x28bdc0 (free function)
- [x] Reconstruct appendArgList @0x28bec0 (free function)
- [x] Create asm_expression.hpp + asm_expression.cpp
- [x] Sub-phase wrap-up

### 10a. StaticResources + GlobalResources (~10 symbols)

GlobalResources: 3 TLS statics (regNumber, labelIndex, random) + dtor.
StaticResources: init() @0x1ec8f0, dtor @0x129db0. Has vtable — inherits Resources.

- [x] Determine GlobalResources layout (TLS statics, dtor @0x12ab40)
- [x] Determine StaticResources layout from init @0x1ec8f0 + dtor @0x129db0
- [x] Reconstruct all methods
- [x] Create header + implementation files
- [x] Sub-phase wrap-up

### 10b. Resources base class (~20 symbols)

Resources: setState, hasMain, return value/type/reg, print, toString, etc.
Inner types: Variable (dtor), Function (addBody, getBody, addArguments, resetScope, dtor).

- [x] Determine Resources layout from dtor @0x12a8f0 + method accesses
- [x] Reconstruct all Resources methods (setState, hasMain, return*, getRegisterNumber, print*, toString)
- [x] Reconstruct Resources::Variable (dtor @0x1e4be0)
- [x] Reconstruct Resources::Function (addBody, getBody, addArguments, resetScope, dtor)
- [x] Determine Resources::State enum values
- [x] Create/update header + implementation files
- [x] Sub-phase wrap-up

#### Phase 10 wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/
- [x] Propose TODO.md adjustments

---

## Phase 10.5: Consolidation & Loose Ends

### 10.5a. Housekeeping (quick cleanup)

- [x] Fix OVERVIEW.md: remove RawWaveData + ElfWriter from "Not yet reconstructed"
- [x] Fix OVERVIEW.md: add missing file tree entries (memory_allocator_analysis.md,
      static_resources.cpp, global_resources.cpp, resources.cpp)
- [x] Fix OVERVIEW.md: CachedParser "embedded in" → "used by"
- [x] Close unknowns #42 (AsmExpression layout — done 10c), #43 (AsmParserContext — done 9b),
      #44 (ElfWriter — done 9a)
- [x] Integrate MemoryAllocator into wavetable_ir.cpp (include added; full allocation
      logic re-disassembly deferred to 10.5e with the other wavetable_ir elisions)
- [x] Sub-phase wrap-up

### 10.5b. ElfWriter revisit with ELFIO source

Fetch ELFIO API from https://github.com/serge1/ELFIO to replace vtable-offset
pseudocode with proper API calls. Current elf_writer.cpp has ~400 lines of ABI
analysis comments and all ELFIO calls stubbed as `/* elfio... */ nullptr`.

- [x] Fetch ELFIO headers (elfio.hpp, elfio_section.hpp, elfio_segment.hpp)
- [x] Map the 30+ vtable offsets in elf_writer.cpp header comment to actual ELFIO method names
- [x] Replace all `/* elfio... */ nullptr` stubs with proper ELFIO API calls
- [x] Clean up addWaveform's ~400-line ABI analysis comment block → distill to clean code + brief note
- [x] Fix elf_writer.hpp TODO (addWaveform return type: unique_ptr<RawWave>, not ElfWriter*)
- [x] Fix addWaveform NOBITS path (set_size, not set_link)
- [x] Apply same cleanup to write_waves_to_elf.cpp (fixed return value usage in absolute lambda)
- [x] Sub-phase wrap-up

### 10.5c. asm_commands.cpp revisit (11 TODOs, excluding sync/unsync)

Phase 1 tech debt. Each item requires re-disassembly of the target function.

- [x] `alui()` line 280 — re-disassemble multi-instruction immediate splitting (confirmed: 20-bit signed range check, ADDI split low12+upper, non-ADDI: load+reg-reg with ANDI→ANDR/ORI→ORR/XNORI→XNORR mapping)
- [x] `addi32()` line 361 — always 2-instr: ADDI(low12) + ADDIU(upper), both isWaveformCmd=true
- [x] `luser`/`suser` lines 529/537 — confirmed delegates directly to ld/st (same opcode)
- [~] `smap` line 601 — partially clarified: calls alui(ADDI, r1, reg0, arg), but ~0x1E6 bytes of conditional logic after alui not yet reconstructed; marked APPROXIMATE. Full reconstruction deferred to 10.5e.
- [x] `asmMessage` line 705 — confirmed: Immediate(string), cmd=5 (error) or 3 (message)
- [x] Waveform name access lines 751/867 — confirmed: direct field access at WaveformFront+0x00
- [x] Line 172 — confirmed BRGZ opcode (0xF5000000, not BRNZ)
- [x] `toInt32` line 387 — val.toInt() with catch overflow→INT_MAX, underflow→INT_MIN + errorHandler_
- [x] Sub-phase wrap-up

### 10.5d. syncCervino / unsyncCervino

Two large AsmCommands methods currently stubbed. May need splitting into
multiple sub-agent passes for syncCervino.

- [x] `unsyncCervino()` @0x276d10 (1.2KB / 1232 bytes) — 2 ST instructions: R0→0x44, R0→0x45
- [x] `syncCervino()` @0x275c50 (4.3KB / 4288 bytes) — master/slave sync sequence (flag=true→0x44, false→0x45)
- [x] Sub-phase wrap-up

### 10.5e. Other file revisits (smaller loose ends)

Mixed TODO/elision/APPROXIMATE markers across various files. Each needs targeted re-disassembly.

- [x] `asm_commands.hpp` — Fixed `smap` return type: `AsmEntry` → `vector<AsmEntry>`
- [x] `wavetable_ir.cpp` (5 elisions) — alignWaveformSizes, assignWaveformAllocationSizes, allocateWaveforms (Phase 1), allocateWaveformsForFifo, getJsonIndex (corrected to return std::string via property_tree)
- [x] `wavetable_manager_front.cpp` (4 elisions) — WaveformFront inlined ctor `(name, fileType, devConst)` + post-construction field assignment
- [x] `asm_optimize.cpp` — already clean, no markers found
- [x] `prefetch_helpers.cpp` — already resolved in prior session
- [x] `prefetch.hpp` — PrefetcherNodeState layout: state=3, branchCount=1, pageSize=1, typed shared_ptr<Cache::Pointer>
- [x] `static_resources.cpp` — getVariable() fully reconstructed (SSE checks for DEVICE_SAMPLE_RATE, AWG_MONITOR_TRIGGER, AWG_INTEGRATION_*, ZSYNC_*); init() improved with 14+14+3 actual constant name/value pairs
- [x] `compiler.cpp` — placeholders at getNodeAccessList/getNodeToModeMap depend on CustomFunctions layout (deferred to 11d)
- [x] `waveform.cpp` — no real TODOs, just destruction-order comments
- [x] Sub-phase wrap-up

### 10.5f. Prefetch APPROXIMATE revisits (67 markers across 5 files)

Reconstructed code with uncertain field offsets/register mappings.
Needs re-disassembly to verify and fix each APPROXIMATE annotation.

- [x] `prefetch_splitplay.cpp` (22→5 APPROXIMATE) — 17 resolved, 3 corrected
- [x] `prefetch.cpp` (18→5 APPROXIMATE) — 13 resolved, 4 corrected
- [x] `prefetch_print.cpp` (17→0 APPROXIMATE) — all 17 confirmed correct
- [x] `prefetch_emit.cpp` (8→0 APPROXIMATE) — 4 confirmed, 2 corrected, 2 confirmed after disasm review
- [x] `prefetch_placesingle.cpp` (2→0 APPROXIMATE) — 1 confirmed, 1 corrected
- [x] Sub-phase wrap-up

### 10.5f-deferred. Remaining prefetch APPROXIMATE markers (10 items, low priority)

Residual APPROXIMATE markers that need deeper disassembly analysis for diminishing returns.

**prefetch.cpp (5):**
- [ ] Line 472: bitmask `0x114` — verify exact bits for Play/Branch/Loop type check
- [ ] Line 496: enum name ordering — verify NodeType enum values vs jump table layout
- [ ] Line 508: `NodeType::Play` cmp $0x2 — verify enum value
- [ ] Line 527: `shared_ptr<Node>` aliasing constructor — verify deleter forwarding pattern
- [ ] Line 749: `optional<string> waveName = *waveformIR` — verify copy semantics from WaveformIR

**prefetch_splitplay.cpp (5):**
- [ ] Line 123: `lookupNode` source — may be raw->parent or raw->ref
- [ ] Line 132: lambda deleter `[](Node*){}` — verify actual refcount management
- [ ] Line 173: `totalLength = coveredByFull` assignment — verify mov operand mapping
- [ ] Line 323: `cachePtr->position` from second find — verify hash lookup result access
- [ ] Line 344: loop counter decrement — verify exact decrement semantics

### 10.5g. wavetable_ir.cpp allocation lambda bodies

The Phase 2 allocation lambdas in `allocateWaveforms` and `allocateWaveformsForFifo`
remain approximate stubs. The logic involves inlined MemoryAllocator cache-line
allocation (allocateCLAligned / allocateReloadingCL) inside lambda closures.

- [x] `allocateWaveforms` Phase 2 lambda — cache-line allocation using cacheLineUsage vector (operator() @0x2a9c80)
- [x] `allocateWaveformsForFifo` lambda$_0 — CL-aligned allocation with set tracking (operator() @0x2aa700)
- [x] `allocateWaveformsForFifo` lambda$_1 — reloading fallback allocation (operator() @0x2acfb0)
- [x] Sub-phase wrap-up

### 10.5h. static_resources.cpp full constant extraction

All 213 addConst() calls in init() are now fully reconstructed.

- [x] Extract remaining HDAWG/Hirzel constants (2.0GHz rates, QA_INT/QA_GEN, SHF/SHFQC rates)
- [x] Extract remaining UHF/Cervino constants (already present from 10.5e)
- [x] Extract common constants (triggers, channels, suppress/enable, math, booleans)
- [x] Sub-phase wrap-up

### 10.5i. detail:: namespace cleanup

Survey of ~53 detail:: types, ~200 symbols. Extracted AddressImpl<T> into own header,
reconstructed logExceptionToClog, cleaned up bogus aliases/forward-decls in 8 headers.

- [x] Survey detail:: namespace symbols (53 types, ~200 functions, ~39KB code)
- [x] Extract AddressImpl<T> from value.hpp → address_impl.hpp
- [x] Reconstruct logExceptionToClog → log_exception.cpp
- [x] Fix play_config.hpp broken include path
- [x] Clean bogus AddressImpl aliases/forward-decls in 6 headers (cache, elf_writer, prefetch, waveform, wavetable_front, wavetable_ir)
- [x] Sub-phase wrap-up

#### Phase 10.5 wrap-up

- [x] Update OVERVIEW.md
- [x] Update unknowns.md (closed #70 PrefetcherNodeState)
- [x] Propose TODO.md adjustments (10.5g, 10.5h added; 11d compiler.cpp note added)

---

## Phase 10.6: Compilation Error Audit

Compile each `.cpp` individually, fix errors by category, recompile after
each batch to measure progress and discover cascading issues.

**Procedure per batch:** fix → recompile all → save errors to /tmp/build_errors/ →
compare error counts → analyse new/changed errors → reconsider plan before next batch.

### 10.6a. ELFIO `<cstdint>` ordering — COMPLETE
### 10.6b. `AsmEntry` → `AsmList::Asm` rename — COMPLETE
### 10.6c. `value.hpp` sizing & union issues — COMPLETE
### 10.6d. `play_config.hpp` duplicate `fromJson` — COMPLETE
### 10.6e. `node.hpp` `std::__shared_weak_count` — COMPLETE
### 10.6f. Wrong/missing member names in structs — COMPLETE
### 10.6g. `ErrorMessages` API mismatches — COMPLETE
### 10.6h. Missing includes / forward declarations — COMPLETE
### 10.6i. `boost_filesystem_path` typo — COMPLETE
### 10.6j. Major header-level additions — COMPLETE
### 10.6k. Include fixes, Node/Waveform members — COMPLETE
### 10.6l. AsmCommands/Resources includes, Signal default ctor — COMPLETE
### 10.6m. Batch header + source fixes (457→252) — COMPLETE

Error progression: 5701→2356→1932→1853→1818→1709→1225→1182→754→534→457→**252**

### 10.6n. Massive batch fix (252→0) — COMPLETE

Combined all remaining sub-phases into one batch. Key fixes:
- Added `AsmList::Asm::operator==` (fixed 11 files)
- Added `Value()` default ctor, `Waveform()` default ctor
- Added `#include <variant>` to value.cpp, `#include "rawwave.hpp"` to signal.cpp
- Added `#include "zhinst/device_constants.hpp"` to waveform_ir.cpp
- Added `#include <boost/property_tree/ptree.hpp>` to waveform_ir.cpp
- Removed MemoryAllocator static_assert, added public accessors
- Fixed MemoryAllocator private member access in wavetable_ir.cpp
- Fixed `const DeviceConstants*` const-correctness in waveform.cpp, wavetable_ir.cpp
- Fixed `asm_ = input` → `asm_ = input.entries` in asm_optimize.cpp
- Fixed `this->branchMaySkipAllBodies` → `cur->branchMaySkipAllBodies` in prefetch.cpp
- Fixed `sampleData` → `samples_` in waveform_ir.cpp
- Fixed WavetableManager field names: `numDefs_` → `lineNr_`, `numDefs2_` → `waveformCounter_`, `nameIndex_` → `nameToIndex_`
- Fixed Signal field access: `data` → `data()`, `markers` → `markers_`, `playMarkers` → `markerBits_`
- Fixed `File::Type::Generated` → `File::Type::GEN`
- Removed insertWaveform<WaveformIR> explicit specialization (uses general template)
- Fixed `Signal::data.empty()` → `Signal::data().empty()` in write_waves_to_elf.cpp
- Many other include, type, and field-name fixes across 20+ files

Error progression: 5701→2356→1932→1853→1818→1709→1225→1182→754→534→457→252→**0**

### 10.6o–10.6s — MERGED INTO 10.6n (all complete)

#### Phase 10.6 wrap-up

- [x] All 47 source files compile with zero errors (1 warning: placement-new size in value.cpp)
- [x] Update OVERVIEW.md
- [x] Propose TODO.md adjustments for next phase

#### Phase 10.6 wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/ if needed
- [x] Propose TODO.md adjustments

---

## Phase 10.7: TODO Comment Audit

Phase 10.6 introduced ~174 uncertainty markers (TODO/TBD/APPROXIMATE/PLACEHOLDER)
across 30 files. Most are "offset TBD" fields added to headers to make code
compile — many are hallucinated names with no binary evidence.

**Method for each struct**: Do NOT trust the current .hpp. Go back to the binary:
1. Re-disassemble ctor/dtor fresh — ground-truth layout
2. Enumerate ALL consumers via symbol grep
3. Map field accesses from consumer disassembly to byte offsets
4. Reconcile TBD fields: confirm offset, merge with existing, or delete
5. Update header + fix .cpp files

### 10.7a. Catalog — COMPLETE

- [x] Grep all TODO/APPROXIMATE/TBD/PLACEHOLDER/STUB markers
- [x] Group by file, categorize, prioritize
- [x] Write `notes/todo_audit.md` with full catalog

### 10.7b. Resolve TBD fields by binary re-analysis

Order: Node first (consumed by everything), then dependent types.

- [x] **10.7b-1: Node** (32 TBD → 0)
      Re-disassembled both ctors + dtor. Enumerated ALL consumer functions.
      Mapped every field access to byte offset within 0x110 layout.
      Many TBD fields were aliases (refCount→type, firstChild→next,
      waveNames→wavesPerDev). _reserved0/_reserved1 at +0x18/+0x20 identified
      as decomposed weak_ptr<Node> (raw ptr + ctrl block).

- [x] **10.7b-2: WaveformIR** (11 TBD → 0)
      Re-disassembled ctors @0x114da0/@0x2a9240 + dtor. Layout 0xE0
      = Waveform(0xD8) + 8 bytes (markedForLoad/fixed_/irBool1/elfAlignment_).
      Numerous "TBD fields" were aliases for existing Waveform/Signal members.
      Critical bug fixed in prefetch.cpp:1996 (wrong field set).

- [x] **10.7b-3: WaveformFront** (9 TBD → 0)
      Re-disassembled dtor @0x2a1300. Layout 0xF8. All 7 alleged TBD fields
      were aliases (isAllocated→used, channels/sampleLength→signal.*,
      fileType→waveformType, isModified→dirty_, funDescrName_→funDescrName)
      or hallucinated (args_). Replaced with forwarding accessors.

- [x] **10.7b-4: AWGCompilerConfig** (10 TBD → 0)
      Re-disassembled dtor @0xf8080. Confirmed 0xC0 layout. Findings:
        * appendMode → isHirzel (+0x18)        [accessor]
        * splitIndex → cacheType (+0x19)        [accessor]
        * syncVersion → numChannelGroups (+0x1C)[accessor]
        * deviceSampleRate is REAL: double at +0x08 (was unknown_08/0c).
          Verified movsd xmm0,[r14+0x8] at 0x1ee69b in StaticResources::init.
        * cacheSize → devConst_->waveformMemorySize (NOT a config field)
        * channelIndex → config_->cacheType (same as splitIndex)
        * baseGrainSize → devConst_->waveformAlignment
        * channelGrains → devConst_ uint32_t[2] at +0x18 (cachePageCount/maxBlocks)
        * seqCount → config_->deviceIndex (+0x24)

- [x] **10.7b-5: PrefetcherNodeState + Prefetch class** (9 TBD → 0)
      Re-disassembled Prefetch ctor @0x1c5850. PNS confirmed at 0x40 bytes.
      Findings:
        * lengthReg → registerHirzel (PNS+0x00, same 8-byte slot, verified
          mov [rax+0x20],rcx at 0x1cb03d)
        * counter → state (PNS+0x10, verified mov [rax+0x30] at 0x1ce77a)
        * playSize → pageSize (PNS+0x1C, verified mov [rax+0x3c] at 0x1cb00b)
        * usedCache → requiredSlots (PNS+0x20, verified mov [rax+0x40] at 0x1cdcfd)
        * totalSize → was a function-scoped stack local in placeSingleCommand
          (slot -0x140(%rbp)), NOT a PNS field; marked TODO with placeholder
        * firstTime → no binary access; hallucinated, removed
        * Prefetch::isHirzel_ → split_ alias (both at +0xBC, verified
          cmp BYTE [r15+0xbc] at 0x1d9f65)
        * Prefetch::pageSize_ → only ever in ctor init list, never read; deleted
        * npCerv → was a local Node* var in placeSingleCommand:210, not a member
      All PNS aliases provided as forwarding accessors (lengthReg(), etc.).

- [x] **10.7b-6: DeviceConstants aliases** (6 TBD → 0)
      All 4 alleged extension fields confirmed to be aliases for existing
      0x90-layout fields:
        * grainSize          → waveformPageSize     (+0x44, verified 0x1d919b)
        * maxWaveformLength  → waveformGranularity  (+0x40, verified 0x1d9198)
        * maxDioTableEntries → waveformMemorySize   (+0x0C, verified 0x29cade
                               in WavetableFront::updateDioTableUsage)
        * maxWaveIndex       → (uint32_t)maxSequenceLen (+0x60, verified
                               mov esi,[rbx+0x60] at 0x29ceaa in WavetableIR ctor)
      Replaced field stubs with const accessor methods. Restored
      static_assert(sizeof(DeviceConstants) == 0x90).

- [x] **10.7b-7: asm_commands.cpp wrong fields** (8 markers → 0)
      Re-disasm genPlayConfig @0x2789a0 (NOT 0x1c3940 — that was MathCompiler::log),
      asmPlay @0x278b40 (NOT 0x1c4a60), encodeCwvf @0x1dc500.
      Findings:
        * Stale Immediate-variant TODO at line 286 removed
          (operator int() access already correct).
        * lcnt opcode (0x2756f0): validate register, addr.value += 0x64,
          then call ld(reg, addr). The +0x64 places the address into the
          loop-counter register bank.
        * genPlayConfig field references corrected:
            - [wf+0xc8] is signal.channels_ (uint16_t), NOT "sampleWidth"
            - [wf+0xb0/+0xb8] are signal.markerBits_ vector begin/end
            - holdCount and isBool params accepted but never consumed in
              visible code path; marked with (void) casts for ABI fidelity
        * Hallucinated encodeCwvf call in asmPlay REMOVED.
          PlayConfig::encodeCwvf (0x1dc500) is called from EXACTLY one site
          in the binary — 0x1d8075, inside Prefetch::placeSingleCommand
          (already handled in 10.7b-5). asmPlay never calls it. Verified
          by grep "call.*encodeCwvf" against full disassembly.
      Marker count: 101 → 93.

- [x] **10.7b-8: AWGAssemblerImpl** (6 TBD → 0)
      Re-disasm of ctor (0x285180), dtor (0x2853c0), getOpcode (0x289060),
      setMemoryOffset (0x288560), errorMessage (0x289070),
      parserMessage (0x289190), assembleString (0x286490).
      Total size verified: mov edi,0x170 at 0x285050.
      Findings:
        * field_70_ (void*) was a hallucination. Real field at +0x70 is
          int32_t currentLine_ — read by errorMessage as the leading int
          of an emitted Message; reset to 0 at assembleString+0x16c.
        * Message struct: only ONE int field at +0x00 (named code_),
          NOT two (level + lineNumber). errorMessage writes currentLine_,
          parserMessage writes the level argument; same slot.
        * opcodes_begin_/end_ and sourceLines_begin_/end_ were ALL
          hallucinations. opcodes_ is at +0x50 (verified by getOpcode
          returning [rdi+0x50]); sourceLines_ at +0x78. Pipeline code
          updated to use std::vector .data()/.size()/.push_back().
        * remaining_fields_[0x80] was a stub for the embedded
          AsmParserContext at +0xF0 (size exactly 0x80 = 0x170 - 0xF0).
          Verified by errorMessage doing add rbx,0xf0; setSyntaxError
          which writes byte at +0x03 of parser ctx (= AWG +0xf3 =
          AsmParserContext::hadSyntaxError_).
        * Duplicate fields removed: flags_, initialized_, pad0_,
          lineNumber_ — these were each first-few-bytes of parserCtx_
          incorrectly listed at AWGAssemblerImpl scope.
      Cannot static_assert sizeof on build host: binary uses libc++
      (std::__1::string == 24 bytes) but build host uses libstdc++
      (std::string == 32). Layout verified directly from disasm instead.
      Marker count: 93 → 87.

- [x] **10.7b-9: ErrorMessageT placeholder enums** (3 markers → 0)
      Real enum values verified directly from binary throw sites:
        * InvalidRegister = 0 — verified at AsmCommands::prf+0x166
          (mov esi,0; format("PRF")). Format string: "%1% command without
          valid register".
        * ValueOverflow   = 5 — verified at AsmCommands::wvfs+0xa1
          (mov esi,5; format("WVFS")). Format string: "value %1% is out of
          range for %2% bits".
        * UnsupportedOp   = 11 — verified at AsmCommandsImplCervino::wvfs
          (0x27c2c8), wvft (0x27c3a8), wwvfq (0x27c488); all mov esi,0xb.
          Format string: "%1% is not a valid opcode".
      CRITICAL FINDING: these collide with existing enum entries
      CmdWithoutRegister=1, TooFewArguments=5, UserRegNotExist=11. Cross-
      referenced against the ErrorMessages map initializer at 0xd5de0:
        * key 0 → "%1% command without valid register" (existing enum says 1)
        * key 5 → "value %1% is out of range for %2% bits" (existing says 6)
        * key 11 → "%1% is not a valid opcode"            (existing says 12)
      The entire 1..12 SeqC compiler block of the existing enum is shifted
      by +1 from the binary's actual values. Fixing the off-by-one across
      every entry is OUT OF SCOPE for 10.7b-9 — added a new TODO block at
      the placeholder site noting the discovery; full renumbering deferred
      to a future sub-phase.
      Marker count: 87 → 84.

- [x] **10.7b-10: Signal convenience methods** (4 TBD → 0) ✅
      Confirmed via grep that `Signal::granularity/maxLength/minLength/
      bitsPerSample` do NOT exist as binary symbols — entirely fabricated
      convenience accessors. Only consumers were `prefetch_prepare.cpp`
      (lines 687-688, 715-716) and `wavetable_front.cpp` (lines 122-123,
      130, 138-139). Disassembly proves the values come from elsewhere:
        - Site `Prefetch::definePlaySize` 0x1ca918..945:
            granularity = devConst_->waveformPageSize  (DC+0x44)
            "maxLength" = waveform2->seqRegWidth       (Waveform+0x70 =
                          "minLengthSamples" in JSON; semantically a MIN
                          via cmova → max(aligned, seqRegWidth))
        - Site `Prefetch::definePlaySize` 0x1cab18..4c:
            wfMaxCap = waveform4->deviceConstants->waveformGranularity (+0x40)
            wfGrain  = waveform4->deviceConstants->waveformPageSize    (+0x44)
            sampleBits = waveform4->deviceConstants->bitsPerSample     (+0x50)
        - Site `WavetableFront::getMemorySize` 0x29ae31..53: same pattern,
            r10 = wf->deviceConstants, then +0x40/+0x44/+0x50 reads.
      Removed 4 fake methods from `signal.hpp`; replaced call sites with
      direct DeviceConstants/Waveform field accesses; preserved cmova→max
      semantics. Full build clean (0 errors). Marker count: 84 → 78.

- [x] **10.7b-11: Minor header TBDs** (cache, asm_list, asm_expression, resources,
      cached_parser, error_messages) ✅
      Cache::Pointer::pageCount removed (hallucination, no symbol/consumer).
      AsmList::Asm::lineNumber resolved as forwarding accessor for
      wavetableFront (dual-purpose +0x88 verified at 0x280c02).
      AsmExpression: pad_58/labelPc unified into single labelIndex at +0x58
      (verified at 0x285732, 0x287e43); accessors replace prior reference
      data members. Resources::VarSubType (0,1) and EDirection (0,1) populated.
      CachedParser fully reconstructed (0x60 bytes from ctor 0x2afa70 / dtor
      0x29aac0): embedded std::map<vector<uint>, CacheEntry> + 4 fields,
      5 methods identified; fabricated default ctor removed.
      ZIAWGCompilerException: removed fabricated (int,string) ctor / code_
      field / code() method; documented true inheritance from zhinst::Exception
      (size 0x60, MI with std::bad_exception + boost::exception); added real
      default ctor with hardcoded "compiler exception" message.
      Full build clean (0 errors). Marker count: 78 → 70.

- [x] **10.7b-12: Fix .cpp files** after header reconciliation ✅
      Verification pass complete. After reconciling all 10.7b-1..11 headers,
      scanned src/ for orphan references to removed/renamed members:
      `pageCount`, `code_`, `code()`, `pad_58`, and field-access forms of
      `labelPc`/`lineNumber`/`noOpt`/`labelType` (now accessor methods).
      All hits in src/ are in `//` comments only — no live code references
      stale members. Full -fsyntax-only build succeeded with 0 errors and
      the single pre-existing value.cpp placement-new warning.

- [x] **10.7b-13: Recompile all** — verify zero errors maintained ✅
      Full src/*.cpp -fsyntax-only build: exit 0, only the pre-existing
      8-line value.cpp warning. Saved at /tmp/build_errors_107b12/full.err.

### 10.7c. Document remaining markers

- [x] Document libc++ ABI patterns (void* control block casts) in notes/ ✅
      Created `reconstructed/notes/libcpp_abi.md` covering: type-size
      table (string=24 vs 32, function=0x30 vs 0x20, __tree=0x30 both),
      libc++ basic_string tagged-SSO layout, why we cannot host-static_assert
      sizeof for string-bearing structs, the value.cpp placement-new warning's
      ABI-mismatch root cause, shared_ptr/weak_ptr control-block layout,
      and the `__on_zero_shared_weak` deallocation-size pattern for proving
      class sizes from disasm (used to confirm AsmExpression=0xA8).
- [x] Update unknowns.md with any new open questions ✅
      Added items 78 (CacheEntry layout), 79 (zhinst::Exception base class —
      MI with std::bad_exception + boost::exception, 4 ctors, what/code/description
      methods at 0x2e5870/0x2e5890/0x2e58b0), 80 (GenericErrorDescription<T>
      template referenced by Exception::Exception ctor 0x2e5810).
- [x] Sub-phase wrap-up ✅

#### Phase 10.7 wrap-up

- [x] Update OVERVIEW.md ✅ (10.7b-1..11 sub-phase entries added; 10.7c entry added)
- [x] Update notes/ ✅ (libcpp_abi.md created; unknowns.md items 78/79/80 added)
- [x] Propose TODO.md adjustments ✅ (deferred work tracked as 10.7d/10.7e; src/
      marker hotspots tracked as Phase 10.8; Phase 12b CachedParser scope reduced)

### 10.7d. zhinst::Exception base class (deferred from 10.7b-11) — COMPLETE

Discovered as the true base of ZIAWGCompilerException (currently
declared as `std::exception` in error_messages.hpp for source-level
compatibility). Reconstructing this opens up cleanup of all derived
exception classes. See unknowns.md item 79.

- [x] Disassemble all 5 zhinst::Exception ctors:
        default        @ 0x2e5410
        (string)       @ 0x2e54b0
        (error_code)   @ 0x2e55b0
        (error_code, string) @ 0x2e5700
        (GenericErrorDescription<error_code>) @ 0x2e5810
- [x] Disassemble what() @ 0x2e5870, code() @ 0x2e5890, description() @ 0x2e58b0
- [x] Disassemble dtor @ 0x2e7720; map MI subobject layout
      (std::bad_exception primary at +0x00, boost::exception at +0x08)
- [x] Determine layout of zhinst::GenericErrorDescription<T>
      (= `{T (24B), std::string (24B)}`, 48 bytes total)
- [x] Create reconstructed/include/zhinst/core/exception.hpp
- [x] Create reconstructed/src/core/exception.cpp
- [x] Re-derive ZIAWGCompilerException, ZIAWGOptimizerException from
      zhinst::Exception. (24 other LabOne-API exception derivatives
      documented but not declared — none appear on any seqc compiler
      code path; tracked as unknowns.md #92.)
- [x] Verify full build still clean (0 errors, 0 warnings)
- [x] Sub-phase wrap-up

Resolutions table:

| Subtask                                  | Status   | Notes                                |
|------------------------------------------|----------|--------------------------------------|
| 5 ctor disasm                            | COMPLETE | All ctors verified at given addresses|
| 3 observer disasm                        | COMPLETE | what/code/description                |
| dtor + MI layout                         | COMPLETE | size 0x60, message at +0x48          |
| GenericErrorDescription<T> layout        | COMPLETE | unknowns.md #80 closed               |
| exception.hpp + exception.cpp            | COMPLETE | Source-level approximation of MI     |
| ZIAWGCompilerException re-derivation     | COMPLETE | error_messages.hpp now includes      |
| ZIAWGOptimizerException re-derivation    | COMPLETE | Symmetric with Compiler variant      |
| Clean cmake build                        | COMPLETE | 0 errors, 0 new warnings             |

Marker count: 9 → 8 (-1: forward-ref banner in error_messages.hpp removed).
New unknowns opened: #90 (error_code ctor prefix string),
#91 (default ctor sentinel ZIResult_enum value),
#92 (24 deferred derivative classes).

### 10.7e. ErrorMessageT renumbering (deferred from 10.7b-9) — COMPLETE

The entire 0..254 SeqC compiler block was shifted by +1 from the
binary's actual keys. Corrected all 253 entries to match the binary
map initializer at 0xd5de0.

- [x] Re-disassemble the BSS map initializer at 0xd5de0 to extract the
      complete key→string mapping for keys 0..(at least)12
      (verified full sequence 0x00..0xFF, gaps at 0x2F(47) and 0x35(53))
- [x] For each existing 1..12 enum entry, verify the binary's actual key
      and write a renumbering table (all shifted by -1)
- [x] Renumber CmdWithoutRegister..InvalidOpcode in error_messages.hpp
      (renumbered ALL 253 entries in 0..254 range, not just the assembler block)
- [x] Remove the now-redundant alias entries (InvalidRegister/ValueOverflow/
      UnsupportedOp) — they collapse into the renumbered base names
      (kept as aliases for source compatibility since src/ uses those names)
- [x] Search src/ for any consumer that uses these enum values numerically
      (all call sites use hex literals from disasm — already correct; no
       symbolic enum names are compared against integer values)
- [x] Verify full build still clean (0 errors)
- [x] Sub-phase wrap-up (inline — no doc changes warranted beyond this
      TODO.md entry; OVERVIEW.md updated in Phase 10.7d wrap-up already
      mentions 10.7e as deferred)

Additional findings during renumbering:
- The gap annotations in the original enum were wrong: they stated gaps
  at values 47 and 53 (old numbering), which corresponded to binary keys
  46 and 52 — but those binary keys EXIST. The real binary gaps are at
  keys 47 (0x2F) and 53 (0x35). After the -1 shift, the gap positions
  happen to coincide numerically (47 and 53 in new enum = 47 and 53 in
  binary), which is correct but was accidental — the old annotations
  were masking an off-by-two error (off-by-one shift + off-by-one gap).
- Binary key 0x2E (46) contains "Invalid constant argument used for
  executeTableEntry" (now ExecTableInvalidConst=46). This entry was
  hidden by the misidentified gap in the old enum.
- Binary key 0x34 (52) contains "constant '%1%' is deprecated, please
  use %2%" = DeprecatedConst. Previously numbered 54 (old) → now 52.

---

## Phase 10.8: Resolve src/ marker hotspots

The 70 markers remaining at end of Phase 10.7 are concentrated in a small
number of source files. Many cluster around shared themes that are now
addressable thanks to `notes/libcpp_abi.md`:

- libc++ weak_ptr internal ABI (raw + ctrl block decomposition into
  `void*` fields, e.g. Node +0x18/+0x20)
- approximate field names that need binary verification
- approximate semantics in tight loops (loop counters, accumulator
  types, branch directions)

Marker counts at start of Phase 10.8:

| File                                     | Markers |
|------------------------------------------|---------|
| src/codegen/prefetch.cpp                         | 18      |
| src/codegen/prefetch_placesingle.cpp             | 7       |
| src/codegen/prefetch_splitplay.cpp               | 6       |
| src/waveform/wavetable_ir.cpp                     | 5       |
| src/waveform/wavetable_manager_ir.cpp             | 4       |
| src/runtime/static_resources.cpp                 | 3       |
| src/codegen/prefetch_prepare.cpp                 | 3       |
| src/codegen/prefetch_helpers.cpp                 | 3       |
| src/codegen/prefetch_emit.cpp                    | 3       |
| include/zhinst/waveform/waveform_ir.hpp           | 3       |
| src/codegen/awg_assembler_impl_pipeline.cpp      | 2       |
| include/zhinst/core/error_messages.hpp        | 2       |
| (10 other files at 1 marker each)        | 10      |
| **Total**                                | **70**  |

Marker counts after 10.8a (prefetch.cpp + cascade):

| File                                     | Markers | Δ      |
|------------------------------------------|---------|--------|
| src/codegen/prefetch_placesingle.cpp             | 6       | -1     |
| src/waveform/wavetable_ir.cpp                     | 5       | 0      |
| src/waveform/wavetable_manager_ir.cpp             | 4       | 0      |
| src/codegen/prefetch_splitplay.cpp               | 4       | -2     |
| src/runtime/static_resources.cpp                 | 3       | 0      |
| src/codegen/prefetch_prepare.cpp                 | 3       | 0      |
| src/codegen/prefetch_helpers.cpp                 | 3       | 0      |
| src/codegen/prefetch_emit.cpp                    | 3       | 0      |
| include/zhinst/waveform/waveform_ir.hpp           | 3       | 0      |
| src/codegen/awg_assembler_impl_pipeline.cpp      | 2       | 0      |
| include/zhinst/core/error_messages.hpp        | 2       | 0      |
| (others, 1 each)                         | 11      | +1     |
| ~~src/codegen/prefetch.cpp~~                     | 0       | **-18**|
| ~~src/codegen/prefetch_print.cpp~~ (old: 3)      | 0       | **-3** |
| **Total**                                | **49**  | **-21**|

Marker counts after 10.8b (placesingle + splitplay):

| File                                     | Markers | Δ      |
|------------------------------------------|---------|--------|
| src/waveform/wavetable_ir.cpp                     | 5       | 0      |
| src/waveform/wavetable_manager_ir.cpp             | 4       | 0      |
| src/runtime/static_resources.cpp                 | 3       | 0      |
| src/codegen/prefetch_prepare.cpp                 | 3       | 0      |
| src/codegen/prefetch_helpers.cpp                 | 3       | 0      |
| src/codegen/prefetch_emit.cpp                    | 3       | 0      |
| include/zhinst/waveform/waveform_ir.hpp           | 3       | 0      |
| src/codegen/awg_assembler_impl_pipeline.cpp      | 2       | 0      |
| include/zhinst/core/error_messages.hpp        | 2       | 0      |
| (others, 1 each)                         | 11      | 0      |
| ~~src/codegen/prefetch_placesingle.cpp~~         | 0       | **-6** |
| ~~src/codegen/prefetch_splitplay.cpp~~           | 0       | **-4** |
| **Total**                                | **39**  | **-10**|

### 10.8a. prefetch.cpp (18 markers → 0) — COMPLETE

Largest single hotspot. Categories:
- Multiple `goto` labels referenced but not defined (`recheckCwvf`,
  `recheckLoopCwvf`) — re-disasm the basic-block layout to determine
  the correct restructured control flow.
- `void* loadCtrl` / `void* parent_ctrl` / `void* load_ctrl` workarounds
  where the binary uses libc++ weak_ptr internals (raw+ctrl block) but
  source uses `weak_ptr<Node>` — apply the libcpp_abi.md decomposition
  pattern to resolve cleanly.
- APPROXIMATE bit masks (e.g. `int mask = 0x114` for Play/Branch/Loop)
  — verify directly from the cmp/test instructions.
- `std::shared_ptr::get_deleter` non-standard usage — find the actual
  semantic (likely a probe of `__shared_ptr_emplace` vtable for
  type discrimination).
- A "duplicate-case bug at line 1637" (type=2 collides with NodeType::Play=0x0002).
- `std::optional<std::string>` copy of waveform name — verify against
  the disassembled allocation pattern at 0x1ccbcb-0x1ccbff.

- [x] Categorize each of the 18 markers by theme (above)
- [x] Resolve weak_ptr ABI markers using libcpp_abi.md as reference
      (full refactor: Node::loadRef_ptr+loadRef_ctrl → std::weak_ptr<Node>
      loadRef; updated ~12 call sites across 5 files)
- [x] Resolve goto-label markers by mapping basic blocks
      (both are unreachable compiler artifacts; replaced with
      explanatory comments)
- [x] Verify APPROXIMATE constants against disassembly
      (mask 0x114 verified at 0x1d079c; CWVF Play/Table case body
      shared at 0x1d02a8)
- [x] Resolve "duplicate-case" bug at line 1637
      (NOT a duplicate — was the real Play case at 0x1d130d that
      had been incorrectly commented out; the existing
      `case NodeType::Play` at line 1492 was actually the Load case
      at 0x1d1045. Both labels swapped, body uncommented, jump-table
      verified at 0x95af54)
- [x] **BONUS**: Audited `Prefetch::prepareTree` and `Prefetch::print`
      switches via jump-table verification (0x95ae98, 0x95ad98) and
      found the same Play/Load mislabel pattern in both — corrected
      4 case labels total across 3 files. Also corrected
      `Prefetch::optimizeCwvf` shared Play/Table case body.
- [x] Verify full build still clean (0 errors)
      `/tmp/build_errors_108a/full.err` — only pre-existing
      value.cpp warning.
- [x] Sub-phase wrap-up

### 10.8b. prefetch_placesingle.cpp + prefetch_splitplay.cpp (10 markers → 0) — COMPLETE

Companion files to prefetch.cpp. Resolved themes:
- **placesingle (6→0)**: `currentAsmId_` field-name (was `wavetableFrontIndex_`,
  blocked at source level by findPlaceholder return type — left as comment);
  `loadNode` initialization (`np->loadRef.lock()`); WaveformIR+0xC8 stale
  TODO (verified `signal.channels_` is correct); two `totalSize` stack-slot
  reloads (lifted to outer-scope variable shared across indexed branches).
- **splitplay (4→0)**: Cache::Pointer field corrections — `pageSize` /
  `pns.pageSize` calls were actually `cachePtr->numRepeats_` (offset +0xC),
  and `cachePtr->position_` was actually `cachePtr->hash_` (offset +0x8).
  The "decrement by 1 APPROXIMATE" was misread; verified `Immediate(1)`
  literal at 0x1de274.

**Side discovery (not acted on)**: `Prefetch::placeSingleCommand`'s
monolithic `case 1:` body (lines 69-668) actually merges genuine NodeType::Load
code (0x1d79f8..0x1d7a4b + 0x1d8281+) AND NodeType::Play code (0x1d7d49+)
under a single label, despite jump table at 0x95af74 routing them
separately. Documented in file-header comment (prefetch_placesingle.cpp:18-39).
A future structural pass should split this into proper case Load + case Play.

- [x] Walk through all 10 markers; cross-check against now-canonical
      header definitions
- [x] Verify field names against AsmCommands and Cache::Pointer disassembly
- [x] Resolve loop-arithmetic APPROXIMATEs against binary
- [x] Verify full build still clean (0 errors)
      `/tmp/build_errors_108b/full.err` — only pre-existing
      value.cpp warning.
- [x] Sub-phase wrap-up

### 10.8c. wavetable_ir.cpp + wavetable_manager_ir.cpp + waveform_ir.hpp (14 markers → 0) — **COMPLETE**

Wavetable family. Themes resolved:
- Field-name uncertainty: `samplesPerWave` was a wrong guess for an 8-byte
  movsd that's actually `(lineNr_, waveformCounter_)` combined; `fileType`
  was missing because `Waveform::waveformType` (+0x18) is the real field;
  `getUniqueName` is a free function in the anonymous namespace, not a
  member.
- `weak_ptr::controlBlock()` replaced with the standard `lock()` API call
  (the binary inlines libc++'s implementation; see libcpp_abi.md).
- `CsvParser` minimal declaration added (full reconstruction in Phase 12).
- `irBool1` renamed to **`crossesCacheLine_`** based on enumeration of all
  4 writers + the 5 wavetable_ir.cpp call sites.
- WaveformIR found to have a third constructor `(string, File::Type, DC&)`
  inlined into the allocate_shared dispatcher.
- Bonus catch: `prefetch.cpp:742` mislabelled `+0xD8` as `+0xDA / irBool1`;
  fixed to `markedForLoad`.

Marker counts during 10.8c:
| File                              | Before | After |
|-----------------------------------|-------:|------:|
| src/waveform/wavetable_ir.cpp              |      5 |     0 |
| src/waveform/wavetable_manager_ir.cpp      |      4 |     0 |
| include/zhinst/waveform/waveform_ir.hpp    |      3 |     0 |
| src/waveform/waveform_ir.cpp               |      1 |     0 |
| src/waveform/wavetable_manager_front.cpp   |      1 |     0 |
| include/zhinst/waveform/waveform_front.hpp |      1 |     0 |
| **subtotal**                      | **15** | **0** |

Net project marker delta: 39 → 25 (-14). The +1 vs. the original "12
markers" estimate was the prefetch.cpp:742 bonus catch surfaced during
the irBool1 rename.

- [x] Verify each field-name uncertainty against disassembly
- [x] Resolve weak_ptr controlBlock() to standard API
- [x] Determine CsvParser status (kept as forward-decl until Phase 12)
- [x] Pin down irBool1 semantics → `crossesCacheLine_`
- [x] Verify full build still clean (0 errors)
- [x] Sub-phase wrap-up

### 10.8d. Remaining src/ files + include/ headers (25 markers → 9) — **COMPLETE**

The long tail. Targeted: real-code TODO/APPROXIMATE/PLACEHOLDER markers in
src/. Doc-banner TBDs and forward-references to deferred phases (10.7d
Exception base, Phase 11 expression/seqc_ast_node) were intentionally left
in place — they are status markers, not work items.

Marker counts during 10.8d:
- Start: 25 (after 10.8c)
- After asm_parser_context.cpp:        24
- After static_resources.cpp:          21
- After prefetch_emit.cpp + placesingle: 18 (also closed unknowns.md #82)
- After prefetch_prepare.cpp:          15
- After prefetch_helpers.cpp:          12
- After awg_assembler_impl_pipeline.cpp + opcodes_ type fix:  9
- All 9 remaining are intentional doc/banner notes or deferred-phase refs

Resolutions table:

| File | Line(s) | Marker | Resolution |
|---|---|---|---|
| asm_parser_context.cpp | 342 | "up to first space" | `name.find(' ')` + substr (verified `memchr(buf, 0x20, len)` at 0x28c712-0x28c75a) |
| static_resources.cpp | 80 | deprecated-constant warnings blocked | Enabled via new `errorReportTarget()` accessor exposing inline std::function at this+0x100; added externs for `constAwgIntegrationTrigger`, `zsyncDataPqscRegister`, `zsyncDataPqscDecoder` (BSS @ 0xb84690/0xb846a8/0xb846c0) |
| static_resources.cpp | 201 | device subtype always-true | `bt rdx, rcx` mask `0x100010100` selects {0,8,16,32}; replaced `if(true)` with proper bit test |
| static_resources.cpp | 322 | hardcoded 2.4e9 placeholder | Verified at 0x1f0618: binary SKIPS addConst when `dt==2 && isnan(rate)`, no literal substitution; restructured with `emitSampleRate` flag |
| prefetch_emit.cpp | 190 | stale insertPos->nodeType comment | Cleaned: Asm has no nodeType; check is `node->type == 4` |
| prefetch_emit.cpp | (impl) | findPlaceholder return type unknown | **Refactored**: `shared_ptr<Node>` → `AsmList::Asm*` (linear scan with 0xA8 stride, verified at 0x1d6b50). Closes unknowns.md #82. |
| prefetch_emit.cpp | (impl) | clampToCache duplicate | Removed `#if 0` wrapper; canonical impl in prefetch_emit.cpp |
| prefetch_placesingle.cpp | 6 sites | API churn from refactor | Added `placeholderIter()` lambda (`Asm*` → vector iterator; recomputes via index because vector may reallocate); enabled `setWavetableFrontIndex(placeholder->wavetableFront)` (added accessor pair to AsmCommands); converted legacy `insert(node, AsmList&)` callsite to iterator-range form |
| prefetch_prepare.cpp | 136 | "*it is weak_ptr<Node>" | Stale TODO; code already does `it->lock()` correctly. Removed marker. |
| prefetch_prepare.cpp | 161 | stack/deque type mismatch for removeBranches | **Container fix**: traversal local was `std::deque` but symbol at 0x1c8cd7 proves it's `std::stack<shared_ptr<Node>, std::deque<shared_ptr<Node>>>`. Translated all push_back/pop_back/back→push/pop/top in prepareTree (5 sites). Re-enabled `removeBranches(current, stack)` call. |
| prefetch_prepare.cpp | 660 | PlayConfig::unknown_1e undefined | `PlayConfig::dummy` (already defined at +0x1E in play_config.hpp:40) is the right field; offset 0x66 = 0x48 (Node::config) + 0x1E. |
| prefetch_helpers.cpp | 223 | clampToCache duplicate `#if 0` block | Removed dead duplicated body; replaced with NOTE pointing to canonical impl in prefetch_emit.cpp |
| prefetch_helpers.cpp | 325 | branchesModified not a member | **Confirmed `Node::branchMaySkipAllBodies` IS the +0x109 field** (verified at 0x1d3748: `mov BYTE PTR [r14+0x109], 0x1`); enabled assignment |
| prefetch_helpers.cpp | 539 | slotIndex/data[]/hasValue not Node members | Stale; verified at 0x1d6277 the binary reads `wavesPerDev[deviceIndex]` (stride 0x20 = sizeof(optional<string>), has_value at +0x18). Existing wavesPerDev code is correct. |
| awg_assembler_impl_pipeline.cpp | 323 | Label ctor or addLabel? | **Disasm at 0x287e33** shows the binary constructs a *local* `AsmParserContext::Label{labelCounter, labelStr}` purely to hold the string until it's moved into `expr->labelName`. There is **no addLabel/hasLabel registration here**. Removed bogus `parserCtx_.addLabel(labelStr)` call; documented temporary semantics. (The labelName/hasLabel/labelIndex assignment logic at lines 327-338 was already correct — r14 is the make_shared control block, so `[r14+0x70]/+0x78/+0x90]` = obj+0x58/+0x60/+0x78 = labelIndex/labelName/hasLabel.) |
| awg_assembler_impl_pipeline.cpp | 575 | opcodes_ vector<uint64_t> vs vector<uint32_t> | **Type correction across 4 files**: disasm at 0x2885cf-0x2885da passes `&opcodes_` directly to `ElfWriter::addCode(vector<uint32_t> const&)`. Therefore `opcodes_` IS `vector<uint32_t>`. Updated `awg_assembler_impl.hpp` (field type + `getOpcode()` return type), `awg_assembler.hpp` (public `getOpcode()` return type), `awg_assembler_impl.cpp` (impl signature), `awg_assembler.cpp` (impl signature). The vector struct size (0x18) is identical for both element types so layout offsets unchanged. |

Build status after 10.8d: full -fsyntax-only clean across all src/*.cpp,
0 errors, 0 new warnings (only the pre-existing value.cpp placement-new
warning remains).

Remaining 9 markers (all intentional, all left in place):
- 3 deferred-phase forward refs: `error_messages.hpp:494` (Phase 10.7d Exception base),
  `seqc_ast_node.hpp:3`, `expression.hpp:3` (Phase 11)
- 6 informational doc banners (NOT TODOs): `error_messages.hpp:411`,
  `device_constants.hpp:152`, `awg_compiler_config.hpp:105`,
  `awg_assembler.hpp:23`, `awg_assembler_impl.hpp:37`, `cached_parser.hpp:58`

- [x] Walk each remaining marker; resolve or document
- [x] Verify full build still clean (0 errors)
- [x] Sub-phase wrap-up

#### Phase 10.8 wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/
- [x] Propose TODO.md adjustments (Phase 10.7d completed; remaining
      items are 10.5f-deferred APPROXIMATE markers, 10.7e renumbering,
      and Phase 11 forward work. Line 531 MemoryAllocator integration
      closed as already-done. No other adjustments needed.)

---

## Phase 11: Frontend / Lowering Layer

### 11a. Expression struct + parser action functions (24 symbols) — COMPLETE

Expression is a plain struct (no vtable), managed via shared_ptr.
Used as the old AST produced by the SeqC flex/bison parser.
~20 free "create*" functions build Expression trees from parser actions.

- [x] Determine Expression struct layout from copy ctor @0x1bfa30
      (actually 0x58 = 88 bytes, NOT 288 as originally stated;
      libstdc++ build yields 0x60 due to 32-byte string vs libc++ 24)
- [x] Reconstruct Expression copy ctor @0x1bfa30
- [x] Reconstruct parser action functions:
      createValue @0x1bf260, createString @0x1bf2d0,
      createVariable @0x1bf420, createVariableType @0x1bf7c0,
      createOperator @0x1bf830, createAssignOperator @0x1bf9c0,
      createArray @0x1bfb50, createListType @0x1bfb70,
      createOrAppendListType @0x1bfd20 +
        4 wrappers (ArgList @0x1bfd00 / DeclList @0x1bfe00 /
                    ParamList @0x1bfe20 / StmtList @0x1bfe40),
      createFunctionCall @0x1bfe60, createFunction @0x1c0000,
      createCommand @0x1c0330 (variadic),
      createIf @0x1c0530, createIfElse @0x1c06c0, createSwitch @0x1c08d0,
      createCase @0x1c0a60, createCondExpression @0x1c0bf0,
      createFor @0x1c0e00, createWhile @0x1c1080, createRepeat @0x1c1210,
      createDoWhile @0x1c13a0, addVariableType @0x1bf560,
      copyExpression @0x1bef20 (deep recursive copy)
- [x] Determine EOperator (22 values), EOperationType (13 values),
      ECommandType (17 values) — all recovered from binary str() functions
      (jump tables @0x95a82c, @0x95a860, @0x95a8b8)
- [x] Reconstruct seqc_error @0x2ca1b0 (bison error handler)
- [ ] Note: seqc_parse @0x2ca2a0 is bison-generated — defer (like asmparse)
- [x] Create expression.hpp + expression.cpp (+ seqc_parser_context.hpp stub)
- [x] Sub-phase wrap-up
- [x] Bonus: str(EValueCategory) @0x1c16c0 and str(EDirection) @0x1c1730
      enum values discovered (kept for Phase 11b SeqCAstNode work)

Open: TODO #93 (Expression pushChild ownership model — see notes/unknowns.md)

### 11b. SeqCAstNode + subclasses — COMPLETE (layout + base + declarations)

SeqCAstNode is the new AST with virtual interface. 53 named subclasses
(NOT in anonymous namespaces as originally guessed). 24-byte base class,
vtable cluster at .rodata 0xb04f60..0xb060c8.

- [x] Determine SeqCAstNode base layout from ctor @0x1fda00 + dtor @0x209000
- [x] Reconstruct children() @0x1fda20, getListElements() @0x209dd0
- [x] Reconstruct swap() @0x1fda40
- [x] Reconstruct printSeqCAst() @0x1fa3c0 (thin wrapper; anon helper stubbed)
- [x] Enumerate subclass vtables from the SeqCAstNode vtable region (53 classes)
- [x] Determine EValueCategory, EParamDirection enum values (EDirection renamed
      to EParamDirection to avoid collision with resources.hpp — see #94)
- [x] Create seqc_ast_node.hpp + seqc_ast_node.cpp
- [x] Sub-phase wrap-up

Open: TODO #94 (EDirection naming), #95 (deferred subclass bodies), #96 (type field)

### 11b-ext. SeqCAstNode subclass method bodies (deferred)

Full method-body reconstruction for all 53 subclasses.  Scope deliberately
limited in 11b to "layout + base + class declarations only" (user-confirmed).

- [ ] Reconstruct method bodies for trivial-leaf family (7 classes)
- [ ] Reconstruct method bodies for unary family (5 classes)
- [ ] Reconstruct method bodies for SeqCOperator + 22 binary derivatives
- [ ] Reconstruct method bodies for two/three/four-child direct families (13 classes)
- [ ] Reconstruct method bodies for list-node family (4 classes)
- [ ] Reconstruct SeqCVariable and SeqCValue method bodies (special cases)
- [ ] Reconstruct printSeqCAst's anon-namespace helper @0x1fa430 fully
- [ ] Resolve TODO #96 (meaning of `type` field)

### 11c. FrontendLowering types (3 symbols) — COMPLETE

FrontendLoweringContext (0x50B): raw CompilerMessageCollection* + 4 shared_ptrs
+ int channelGrouping.  FrontendLoweringState (0x30B): shared_ptr<void> result +
pad + vector<string>.  constWaveform @0x22c9f0: documented stub (needs
WaveformGenerator::eval + EvalResults layout).

- [x] Determine FrontendLoweringContext layout from dtor @0x1233b0
- [x] Determine FrontendLoweringState layout from dtor @0x1c2190
- [x] Reconstruct constWaveform @0x22c9f0 (documented stub — body deferred)
- [x] Create frontend_lowering.hpp + frontend_lowering.cpp
- [x] Sub-phase wrap-up

Open: TODO #97 (lower() return type), #98 (State result type), #99 (EvalResults layout)

Discovery: FrontEndLoweringFacade::lower() returns a shared_ptr via sret,
not void.  Current compiler.hpp declaration is wrong.  Follow-up items:
- [ ] Fix lower() return type in compiler.hpp (see #97)
- [ ] Reconstruct EvalResults struct (0x80 bytes — see #99)

### 11d. CustomFunctions (~115 symbols) — COMPLETE

Built-in SeqC function implementations. 0x1E0 byte layout fully mapped.
12 methods + 2 exception classes implemented; ~80 SeqC built-in stubs with addresses.
New types: EvalResultValue, NodeMapItem, NodeMapData hierarchy, AccessMode,
MathCompiler, PlayArgs, SubFunc.

- [x] Determine CustomFunctions layout from dtor @0x127c90
- [x] Reconstruct check methods: checkPlayMinLength @0x15b100, checkPlayAlignment @0x15b190
- [x] Reconstruct oscMask methods: oscMaskCheckGrimsel @0x15ba90, oscMaskCheckHirzel @0x15bab0,
      oscMaskSetAllHirzel @0x15bf50, oscMaskSetAllGrimsel @0x15c0b0
- [x] Reconstruct addNodeAccess @0x15c6c0, getWaitTime @0x163930
- [x] Reconstruct initNodeMap @0x16b740, getNodeAddress @0x16ba10,
      getSampleClock @0x16ba80, getAccessModes @0x16be50
- [x] Reconstruct CustomFunctionsException (dtor @0x15a520/0x16e6c0, what @0x16e710)
- [x] Reconstruct CustomFunctionsValueException (dtor @0x163d70/0x172f70, what @0x172fd0)
- [x] Create custom_functions.hpp + custom_functions.cpp
- [x] Sub-phase wrap-up

### 11e. Fix compiler.cpp placeholders — COMPLETE

Now that CustomFunctions types (NodeMapItem, AccessMode) are declared,
updated the getNodeAccessList/getNodeToModeMap stubs in compiler.cpp.

- [x] Update compiler.cpp getNodeAccessList to return proper types
- [x] Update compiler.cpp getNodeToModeMap to return proper types
- [x] Build verify

#### Phase 11 wrap-up — COMPLETE

- [x] Update OVERVIEW.md
- [x] Update notes/
- [x] Propose TODO.md adjustments

---

## Phase 12: Waveform DSL & Utilities

### 12a. WaveformGenerator — COMPLETE

Waveform DSP functions. 0xC0-byte class with ~35 registered waveform functions,
3 implemented methods + 2 exception classes + ~35 waveform stubs.

- [x] Determine WaveformGenerator layout from dtor @0x127840
- [x] Reconstruct createDummyWaveform @0x25be70
- [x] Reconstruct genericTriangle @0x25e0c0
- [x] Reconstruct reverse @0x260f20
- [x] Reconstruct WaveformGeneratorException (dtor @0x25ca60, what @0x261820)
- [x] Reconstruct WaveformGeneratorValueException (dtor @0x25c500, what @0x2617a0)
- [x] Create waveform_generator.hpp + waveform_generator.cpp
- [x] Sub-phase wrap-up

### 12b. CachedParser — COMPLETE

Tree-based cache for parsed waveform files. ~12 own methods,
rest is Boost.Serialization template instantiations (document, don't reconstruct).

**Phase 10.7b-11 already completed:**
- [x] Determine CachedParser layout from ctor @0x2afa70 + dtor @0x29aac0 ✅
      (0x60 bytes: embedded std::map<vector<uint>, CacheEntry> + enabled_/cacheSize_/
      currentSize_/cachePath_/indexFilePath_; created reconstructed/include/zhinst/
      cached_parser.hpp)

**Phase 12b completed:**
- [x] Reconstruct CacheEntry layout (0x60 bytes): name_, filePath_, fileSize_,
      timestamp_, hash_, valid_. Ctor @0x2b10b0, dtor @0x2b1320, op= @0x2b1210.
- [x] Reconstruct CachedFile layout (0x50 bytes): found_, samples_, markers_,
      markerBits_. Dtor @0x2b1f70.
- [x] Stub loadCacheIndex @0x2afec0, saveCacheIndex @0x2b03c0
- [x] Stub cleanCache @0x2b0140, removeOldFiles @0x2b01a0
- [x] Stub cacheFile @0x2b05b0, cacheFileOutdated @0x2b14d0, getCachedFile @0x2b1900
- [x] Reconstruct getHash @0x2b1fe0 (delegates to util::wave::hash)
- [x] Document Boost serialization pattern (unknowns.md #114)
- [x] Create reconstructed/src/io/cached_parser.cpp
- [x] Sub-phase wrap-up (OVERVIEW.md, unknowns.md #113-116)

### 12c. CsvParser — CANCELLED

No zhinst::CsvParser symbols found in binary. Either inlined or
in a different module.

#### Phase 12 wrap-up

- [x] Update OVERVIEW.md
- [x] Update notes/
- [ ] Propose TODO.md adjustments

---

## Deferred / Low Priority

- [x] ~~Full reconstruction of `syncCervino()`~~ — moved to Phase 10.5d
- [x] ~~Full reconstruction of `unsyncCervino()`~~ — moved to Phase 10.5d
- [x] ~~Full reconstruction of `addi32()`~~ — moved to Phase 10.5c
- [x] ~~AWGCompilerConfig~~ — fully reconstructed in Phase 3d
- [ ] MathCompiler (67 symbols) — separate math expression compiler
- [ ] detail::WavetableManager<T> (14 methods, ~6KB) — template waveform table manager
- [ ] DeviceType/DeviceFamily/DeviceTypeCode/DeviceOption (150 symbols) — device enumeration
- [ ] logging + tracing infrastructure (73 symbols)
- [ ] Add `CMakeLists.txt` to compile reconstructed code as a validation step
- [ ] Write comparison tests against the real `.so`
