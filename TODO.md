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
- [x] Create `include/zhinst/error_messages.hpp` with ErrorMessageT enum + declarations
- [x] Create `src/error_messages.cpp`
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

## Phase 4: AWGAssembler (COMPLETE)

### 4a. AWGAssembler base (interface)

- [x] Reconstruct `AWGAssembler(DeviceConstants const&)` ctor (0x285040)
- [x] Reconstruct `~AWGAssembler()` dtor (0x285090)
- [x] Reconstruct `setMemoryOffset(unsigned int)` (0x285120)
- [x] Reconstruct `getOpcode() const` (0x285140)
- [x] Reconstruct `getReport() const` (0x285150)
- [x] Reconstruct `printOpcode(int) const` (0x285170)
- [x] Create `include/zhinst/awg_assembler.hpp`

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
- [x] Create `src/awg_assembler_impl.cpp`

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
- [x] Create `include/zhinst/waveform.hpp` (separate from waveform_front.hpp)
- [x] Create `src/waveform.cpp`
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
- [x] Create `src/wavetable_manager_front.cpp` and `src/wavetable_manager_ir.cpp`

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
- [x] Update `include/zhinst/wavetable_front.hpp`
- [x] Create `src/wavetable_front.cpp` and `src/wave_index_tracker.cpp`

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
- [x] Create `include/zhinst/waveform_ir.hpp` and `include/zhinst/wavetable_ir.hpp`
- [x] Create `src/waveform_ir.cpp` and `src/wavetable_ir.cpp`

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
- [x] Create `include/zhinst/asm_optimize.hpp`
- [x] Create `src/asm_optimize.cpp`

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
- [x] Create `include/zhinst/compiler_message.hpp`, `src/compiler_message.cpp`
- [x] Create `include/zhinst/compiler.hpp`, `src/compiler.cpp`
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
- [x] Create `include/zhinst/cache.hpp`, `src/cache.cpp`

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
- [x] Create `include/zhinst/prefetch.hpp`, `src/prefetch.cpp`

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
- [ ] Update wavetable_ir.cpp to use proper MemoryAllocator calls (deferred — minor integration)

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

- [ ] `prefetch_splitplay.cpp` (22 APPROXIMATE) — field offsets + register mappings
- [ ] `prefetch.cpp` (18 APPROXIMATE) — optimize() and allocate() logic
- [ ] `prefetch_print.cpp` (17 APPROXIMATE) — output formatting details
- [ ] `prefetch_emit.cpp` (8 APPROXIMATE) — wvf/placeCommands details
- [ ] `prefetch_placesingle.cpp` (2 APPROXIMATE) — cervino paths
- [ ] Sub-phase wrap-up

### 10.5g. wavetable_ir.cpp allocation lambda bodies

The Phase 2 allocation lambdas in `allocateWaveforms` and `allocateWaveformsForFifo`
remain approximate stubs. The logic involves inlined MemoryAllocator cache-line
allocation (allocateCLAligned / allocateReloadingCL) inside lambda closures.

- [ ] `allocateWaveforms` Phase 2 lambda — cache-line allocation using cacheLineUsage vector
- [ ] `allocateWaveformsForFifo` lambda$_0 — CL-aligned allocation with set tracking
- [ ] `allocateWaveformsForFifo` lambda$_1 — finalize offsets pass
- [ ] Sub-phase wrap-up

### 10.5h. static_resources.cpp full constant extraction

The `init()` function is ~15KB of addConst() calls. Currently only 14+14+3 constants
are extracted (rate + trigger families). ~100+ more remain (DIO, MARKER, QA, ZSYNC,
integration, device-specific constants).

- [ ] Extract remaining HDAWG/Hirzel constants (DIO, MARKER, etc.)
- [ ] Extract remaining UHF/Cervino constants (QA, integration, etc.)
- [ ] Extract common constants (ZSYNC, misc.)
- [ ] Sub-phase wrap-up

#### Phase 10.5 wrap-up

- [ ] Update OVERVIEW.md
- [ ] Update unknowns.md (close any resolved during revisits)
- [ ] Propose TODO.md adjustments

---

## Phase 11: Frontend / Lowering Layer

### 11a. Expression struct + parser action functions (24 symbols)

Expression is a plain struct (no vtable), managed via shared_ptr.
Used as the old AST produced by the SeqC flex/bison parser.
~20 free "create*" functions build Expression trees from parser actions.

- [ ] Determine Expression struct layout from copy ctor @0x1bfa30 (288 bytes — non-trivial)
- [ ] Reconstruct Expression copy ctor @0x1bfa30
- [ ] Reconstruct parser action functions:
      createOperator @0x1bf830, createAssignOperator @0x1bf9c0,
      createArray @0x1bfb50, createListType @0x1bfb70,
      createOrAppend* (5 variants: ArgList/ListType/DeclList/ParamList/StmtList),
      createFunctionCall @0x1bfe60, createFunction @0x1c0000,
      createIf @0x1c0530, createIfElse @0x1c06c0, createSwitch @0x1c08d0,
      createCase @0x1c0a60, createCondExpression @0x1c0bf0,
      createFor @0x1c0e00, createWhile @0x1c1080, createRepeat @0x1c1210,
      createDoWhile @0x1c13a0, addVariableType @0x1bf560
- [ ] Determine EOperator, EOperationType, ECommandType enum values
- [ ] Reconstruct seqc_error @0x2ca1b0 (bison error handler)
- [ ] Note: seqc_parse @0x2ca2a0 is bison-generated — defer (like asmparse)
- [ ] Create expression.hpp + expression.cpp
- [ ] Sub-phase wrap-up

### 11b. SeqCAstNode + subclasses (12 symbols)

SeqCAstNode is the new AST with virtual lower(). Single vtable in binary —
subclasses (SeqCIfElse, SeqCRepeat, SeqCForLoop, SeqCNoCmd, etc.) are in
anonymous namespaces. Trivial 16-byte dtor suggests thin virtual base.

- [ ] Determine SeqCAstNode base layout from ctor @0x1fda00 + dtor @0x209000
- [ ] Reconstruct children() @0x1fda20, getListElements() @0x209dd0
- [ ] Reconstruct swap() @0x1fda40
- [ ] Reconstruct printSeqCAst() @0x1fa3c0 (thin wrapper + anon namespace helper)
- [ ] Enumerate anonymous-namespace subclass vtables from the SeqCAstNode vtable region
- [ ] Determine EValueCategory, EDirection enum values
- [ ] Create seqc_ast_node.hpp + seqc_ast_node.cpp
- [ ] Sub-phase wrap-up

### 11c. FrontendLowering types (3 symbols)

Very small — just FrontendLoweringContext dtor @0x1233b0,
FrontendLoweringState dtor @0x1c2190, and constWaveform (anon ns) @0x22c9f0.
FrontEndLoweringFacade::lower() already reconstructed in Phase 7a.

- [ ] Determine FrontendLoweringContext layout from dtor @0x1233b0
- [ ] Determine FrontendLoweringState layout from dtor @0x1c2190
- [ ] Reconstruct constWaveform @0x22c9f0
- [ ] Create/update frontend_lowering.hpp + frontend_lowering.cpp
- [ ] Sub-phase wrap-up

### 11d. CustomFunctions (25 symbols)

Built-in SeqC function implementations. ~15 methods + 2 exception classes.
Note: compiler.cpp getNodeAccessList/getNodeToModeMap placeholders depend on
CustomFunctions layout — fix those when this phase is done.

- [ ] Determine CustomFunctions layout from dtor @0x127c90
- [ ] Reconstruct check methods: checkPlayMinLength @0x15b100, checkPlayAlignment @0x15b190
- [ ] Reconstruct oscMask methods: oscMaskCheckGrimsel @0x15ba90, oscMaskCheckHirzel @0x15bab0,
      oscMaskSetAllHirzel @0x15bf50, oscMaskSetAllGrimsel @0x15c0b0
- [ ] Reconstruct addNodeAccess @0x15c6c0, getWaitTime @0x163930
- [ ] Reconstruct initNodeMap @0x16b740, getNodeAddress @0x16ba10,
      getSampleClock @0x16ba80, getAccessModes @0x16be50
- [ ] Reconstruct CustomFunctionsException (dtor @0x15a520/0x16e6c0, what @0x16e710)
- [ ] Reconstruct CustomFunctionsValueException (dtor @0x163d70/0x172f70, what @0x172fd0)
- [ ] Create custom_functions.hpp + custom_functions.cpp
- [ ] Sub-phase wrap-up

#### Phase 11 wrap-up

- [ ] Update OVERVIEW.md
- [ ] Update notes/
- [ ] Propose TODO.md adjustments

---

## Phase 12: Waveform DSL & Utilities

### 12a. WaveformGenerator (16 symbols)

Waveform DSP functions. 3 own methods + 2 exception classes.

- [ ] Determine WaveformGenerator layout from dtor @0x127840
- [ ] Reconstruct createDummyWaveform @0x25be70
- [ ] Reconstruct genericTriangle @0x25e0c0
- [ ] Reconstruct reverse @0x260f20
- [ ] Reconstruct WaveformGeneratorException (dtor @0x25ca60, what @0x261820)
- [ ] Reconstruct WaveformGeneratorValueException (dtor @0x25c500, what @0x2617a0)
- [ ] Create waveform_generator.hpp + waveform_generator.cpp
- [ ] Sub-phase wrap-up

### 12b. CachedParser (~12 own methods + ~60 Boost serialization)

Tree-based cache for parsed waveform files. ~12 own methods,
rest is Boost.Serialization template instantiations (document, don't reconstruct).

- [ ] Determine CachedParser layout from ctor @0x2afa70 + dtor @0x29aac0
- [ ] Reconstruct loadCacheIndex @0x2afec0, saveCacheIndex @0x2b03c0
- [ ] Reconstruct cleanCache @0x2b0140, removeOldFiles @0x2b01a0
- [ ] Reconstruct CacheEntry (copy ctor, op=, dtor, serialize template)
- [ ] Reconstruct CachedFile (dtor @0x2b1f70)
- [ ] Document Boost serialization instantiation pattern (don't reconstruct all)
- [ ] Create cached_parser.hpp + cached_parser.cpp
- [ ] Sub-phase wrap-up

### 12c. CsvParser — CANCELLED

No zhinst::CsvParser symbols found in binary. Either inlined or
in a different module.

#### Phase 12 wrap-up

- [ ] Update OVERVIEW.md
- [ ] Update notes/
- [ ] Propose TODO.md adjustments

---

## Deferred / Low Priority

- [x] ~~Full reconstruction of `syncCervino()`~~ — moved to Phase 10.5d
- [x] ~~Full reconstruction of `unsyncCervino()`~~ — moved to Phase 10.5d
- [x] ~~Full reconstruction of `addi32()`~~ — moved to Phase 10.5c
- [x] ~~AWGCompilerConfig~~ — fully reconstructed in Phase 3d
- [ ] MathCompiler (67 symbols) — separate math expression compiler
- [ ] DeviceType/DeviceFamily/DeviceTypeCode/DeviceOption (150 symbols) — device enumeration
- [ ] logging + tracing infrastructure (73 symbols)
- [ ] Add `CMakeLists.txt` to compile reconstructed code as a validation step
- [ ] Write comparison tests against the real `.so`
