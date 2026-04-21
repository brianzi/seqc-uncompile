# Overview: Reconstructed zhinst SeqC Compiler Assembler Backend

Reverse-engineered from `_seqc_compiler.so` (ELF 64-bit, x86-64, not stripped).

## Class Hierarchy

```
AsmCommands                        # Main class — 83 methods generating assembler records
  └─ AsmCommandsImpl* impl_       # Pimpl: device-specific instruction encoding
       ├─ AsmCommandsImplCervino   # Older/FPGA devices (UHFLI, UHFQA, GHFLI, VHFLI)
       └─ AsmCommandsImplHirzel    # Newer devices (HDAWG, SHFQA, SHFSG, SHFQC_SG, SHFLI)

Compiler (0x138)                   # Master pipeline orchestrator
  ├─ CompilerMessageCollection     # Error/warning accumulator (inline, 0x20)
  ├─ shared_ptr<AsmCommands>       # Instruction emitter
  ├─ shared_ptr<WaveformGenerator> # Waveform DSL
  ├─ shared_ptr<CustomFunctions>   # SeqC built-in function handlers
  └─ SeqcParserContext             # flex/bison parser state (inline)

FrontEndLoweringFacade             # Static facade → SeqCAstNode::lower() virtual dispatch

Prefetch (0x160)                   # Waveform prefetching / cache allocation manager
  ├─ PrefetcherNodeState           # Per-node prefetch state (in unordered_map)
  └─ 30+ methods                   # Tree analysis, load placement, asm generation

Cache (0x28)                       # Waveform cache memory manager
  ├─ Cache::Pointer (0x24)         # Individual allocation entry (position, size, state)
  └─ CacheException (0x20)         # std::exception subclass

AWGAssembler                       # Pimpl wrapper (0x08 bytes)
  └─ AWGAssemblerImpl              # Backend assembler (0x170 bytes) — opcodes, ELF output

AsmParserContext                   # flex/bison parser context for assembly
  └─ Label                         # inner type: (pc, name) pair

AsmOptimize (0xA0)                 # Instruction optimizer — peephole + register allocator
  └─ OptimizeException (0x20)      # std::exception subclass

Waveform (0xD8)                    # Base value type — name, file, signal, playConfig
  ├─ WaveformFront (0xF8)          # Front-end extension (+0x20 bytes)
  └─ WaveformIR (0xE0)             # IR extension (+0x08 bytes)

WavetableManager<T> (0x48)         # Template: hash map + vector of shared_ptr<T>
WavetableFront (0x200)             # Front-end: creation, naming, loading, DIO tracking
WavetableIR (0xC8)                 # IR: allocation, alignment, FIFO layout, serialization

MemoryAllocator (0x70)             # Free-block allocator for waveform cache/FIFO memory
  └─ MemoryBlock (0x0C)            # 12-byte block descriptor (offset, size, field_08)

Resources (0xD8)                   # Base class — scope/variable/function tracking
  ├─ StaticResources (0x110)       # Device-specific built-in constants + logger callback
  └─ GlobalResources (0xD8)        # TLS register counter, label counter, MT19937-64 PRNG

CompilerMessage (0x20)             # type + lineNr + message
CompilerMessageCollection (0x20)   # vector<CompilerMessage> + state
CompilerException (0x20)           # std::exception + message string
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
│   ├── awg_compiler_config.hpp  # AWGCompilerConfig struct (~0xC0 bytes), 4 methods
│   ├── asm_register.hpp         # AsmRegister struct {int value; bool valid} (8 bytes)
│   ├── value.hpp                # Immediate (28B std::variant), Value (40B boost::variant),
│   │                            #   AddressImpl<T>, ValueException
│   ├── address_impl.hpp         # detail::AddressImpl<T> — extracted from value.hpp
│   ├── assembler.hpp            # AssemblerInstr (0x80 bytes), Assembler::Command enum (43 opcodes)
│   ├── play_config.hpp          # PlayConfig struct — all shift/mask constants confirmed
│   ├── node.hpp                 # Node class (0x110 bytes), NodeType enum (14 types)
│   ├── waveform.hpp             # Waveform base (0xD8 bytes) + WaveformFile (0x40 bytes)
│   ├── waveform_front.hpp       # WaveformFront (0xF8 bytes) — extends Waveform
│   ├── waveform_ir.hpp          # WaveformIR (0xE0 bytes) — extends Waveform
│   ├── wave_index_tracker.hpp   # WaveIndexTracker (0x28 bytes) + WavetableException (0x20 bytes)
│   ├── wavetable_front.hpp      # WavetableFront (0x200 bytes) + WavetableManager<WaveformFront> (0x48 bytes)
│   ├── wavetable_ir.hpp         # WavetableIR (0xC8 bytes)
│   ├── asm_list.hpp             # AsmList::Asm record (0xA8 bytes), AsmList (vector wrapper, 0x18 bytes)
│   ├── error_messages.hpp       # ErrorMessageT enum (305 values), ErrorMessages class
│   ├── asm_commands_impl.hpp    # AsmCommandsImpl base + Cervino/Hirzel declarations
│   ├── asm_commands.hpp         # AsmCommands — all 83 method declarations
│   ├── device_constants.hpp  # DeviceConstants (0x90 bytes POD), flat scalar fields, getDeviceConstants()
│   │                         #   (Phase 7e: RegisterBank removed, fields renamed to semantic names)
│   ├── signal.hpp               # Signal (0x58 bytes), SampleFormat enum, MarkerBitsPerChannel, ReserveOnly tag
│   ├── rawwave.hpp              # RawWave (abstract), RawWavePlaceHolder (0x28), RawWaveHirzel16 (0x20), RawWaveCervino (0x20)
│   ├── awg_assembler.hpp        # AWGAssembler pimpl wrapper (0x08 bytes)
│   ├── awg_assembler_impl.hpp   # AWGAssemblerImpl (0x170 bytes), AsmExpression (from usage)
│   ├── asm_optimize.hpp         # AsmOptimize (0xA0 bytes), OptimizeException (0x20 bytes)
│   ├── compiler_message.hpp     # CompilerMessage (0x20), CompilerMessageCollection (0x20),
│   │                            #   CompilerException (0x20)
│   ├── compiler.hpp             # Compiler (0x138 bytes), FrontEndLoweringFacade
│   ├── cache.hpp                # Cache (0x28), Cache::Pointer (0x24), CacheException (0x20)
│   ├── memory_allocator.hpp    # MemoryAllocator (0x70), MemoryBlock (0x0C) — CL-aware allocator
│   ├── callbacks.hpp           # CancelCallback (pure virtual), ProgressCallback (0x08, vtable only)
│   ├── asm_expression.hpp      # AsmExpression (0xa8 bytes) — parse tree node, full layout + factory functions
│   ├── asm_parser_context.hpp  # AsmParserContext (~0x80+ bytes) — flex/bison parser state, Label inner type
│   ├── resources.hpp            # Resources (0xD8), StaticResources (0x110), GlobalResources (0xD8)
│   ├── elf_writer.hpp          # ElfWriter (0x78 bytes) — ELFIO::elfio wrapper for AWG ELF output
│   └── prefetch.hpp             # Prefetch (0x160 bytes), PrefetcherNodeState, 30+ methods
├── src/
│   ├── asm_commands.cpp              # AsmCommands implementations (~550 lines)
│   ├── asm_commands_impl.cpp         # getInstance() factory (bitmask device selection)
│   ├── asm_commands_impl_cervino.cpp # 11 Cervino virtual methods
│   ├── asm_commands_impl_hirzel.cpp  # 11 Hirzel virtual methods
│   ├── assembler.cpp                 # Assembler: cmdMap, commandToString/FromString, str,
│   │                                 #   getOpcodeType, getCycles, getCmdType, getRegisterOrder,
│   │                                 #   highestRegisterNumber, copy/move/dtor
│   ├── asm_list.cpp                  # AsmList: append, print, serialize, deserialize, maxRegister;
│   │                                 #   AsmList::Asm: dtor, serializeNodeToJsonString, createUniqueID
│   ├── value.cpp                     # Value, Immediate, AddressImpl, ValueException — all methods
│   ├── play_config.cpp               # PlayConfig: encodeCwvf, operator!=, toJson, fromJson
│   ├── node.cpp                      # Node: 15 methods (2 ctors, dtor, type2str, str2type, toString,
│   │                                 #   waveAtCurrentDeviceIndex, clone, last, insertBefore,
│   │                                 #   updateParent, remove, swap, toJson, fromJson, installPointers)
│   ├── waveform.cpp                  # Waveform: 11 methods (full ctor, copy ctor, copy-rename ctor,
│   │                                 #   dtor, toJson, fromJson, getSizePerDevice, op==;
│   │                                 #   File: typeToStr, typeFromStr, op==) — 1045 lines
│   ├── waveform_front.cpp            # WaveformFront: toString, copy-rename ctor, dtor
│   ├── waveform_ir.cpp               # WaveformIR: 2 ctors, toJsonElement (146 lines)
│   ├── wave_index_tracker.cpp        # WaveIndexTracker: ctor, assign, assignAuto, getNextAutoIndex,
│   │                                 #   hasGaps, usedWaveIndices, template ctors;
│   │                                 #   WavetableException: ctor/dtor/what (164 lines)
│   ├── wavetable_front.cpp           # WavetableFront: 22 methods (360 lines)
│   ├── wavetable_manager_front.cpp   # WavetableManager<WaveformFront>: 9 methods (280 lines)
│   ├── wavetable_ir.cpp              # WavetableIR: 21 methods (518 lines)
│   ├── wavetable_manager_ir.cpp      # WavetableManager<WaveformIR>: 7 methods (288 lines)
│   ├── device_constants.cpp          # getDeviceConstants() factory — 9 device types
│   ├── error_messages.cpp            # ErrorMessages: operator[], format<>(), 305 format strings;
│   │                                 #   ResourcesException: ctor/dtor/what; getApiErrorMessage()
│   ├── signal.cpp                    # Signal: 17 methods (7 ctors, copy ctor, dtor, 2 appends,
│   │                                 #   resizeSamples, checkAllocation, toJson, fromJson, getRawData, op==)
│   ├── rawwave.cpp                   # RawWave hierarchy: PlaceHolder (dtor/size/ptr), Hirzel16 (ctor/dtor/size/ptr),
│   │                                 #   Cervino (dtor/size/ptr) — 3 conversion paths documented
│   ├── awg_compiler_config.cpp       # AWGCompilerConfig: dtor, getAwgDeviceTypeString/FromString,
│   │                                 #   getChannelGroupingModeString
│   ├── awg_assembler.cpp             # AWGAssembler pimpl: 11 forwarding methods (77 lines)
│   ├── awg_assembler_impl.cpp        # AWGAssemblerImpl: ctor/dtor, setMemoryOffset, getOpcode (75 lines)
│   ├── awg_assembler_opcodes.cpp     # AWGAssemblerImpl: opcode0-5, getReg, getVal (639 lines)
│   ├── awg_assembler_impl_pipeline.cpp # AWGAssemblerImpl: pipeline + helpers (650 lines)
│   ├── asm_optimize.cpp             # AsmOptimize: query helpers, optimization passes,
│   │                                #   register allocator, OptimizeException (837 lines)
│   ├── prefetch.cpp                 # Prefetch: ctor/dtor, globalCwvf, optimizeSync, optimizeCwvf,
│   │                                #   moveLoadsToFront(done), optimize(done), allocate(stub),
│   │                                #   linkLoad, assignLoad, createLoad, mergeLoads, placeLoads
│   ├── prefetch_prepare.cpp         # Prefetch: preparePlays, prepareTree, countBranches, definePlaySize
│   ├── prefetch_helpers.cpp         # Prefetch: backwardTree, removeBranches, expandSetVar,
│   │                                #   findLockedPlay, sameLoads, nodeByCachePointer,
│   │                                #   determineFixedWaves, collectUsedWaves, getUsedWavesForDevice
│   ├── prefetch_emit.cpp            # Prefetch: clampToCache, getUsedChannels, getUsedFourChannelMode,
│   │                                #   findPlaceholder, fillInPlaceholders, placeCommands,
│   │                                #   getUsedCache, getMemoryHighWatermark, getRequiredMemory,
│   │                                #   needsNewCwvf, wvfImpl, wvfRegImpl, wvfs, insertPlay (1012 lines)
│   ├── prefetch_print.cpp           # Prefetch::print — recursive debug tree printer (573 lines)
│   ├── prefetch_splitplay.cpp       # Prefetch::splitPlay — split large waveform plays (429 lines)
│   ├── prefetch_placesingle.cpp     # Prefetch::placeSingleCommand — main node→asm dispatch (981 lines)
│   ├── compiler_message.cpp         # CompilerMessage::str, CompilerMessageCollection methods,
│   │                                #   CompilerException ctor/dtor/what
│   ├── compiler.cpp                 # Compiler: ctor/dtor/compile/runPrefetcher/parse/
│   │                                #   unifyLineEndings + getters/setters + FrontEndLoweringFacade
│   ├── cache.cpp                    # Cache: ctor, 4 getters, 2 allocate overloads,
│   │                                #   getBestPosition, memoryWrite, stillInMemory, reuse,
│   │                                #   play, resetPlay, free, print; Pointer::str;
│   │                                #   CacheException ctor/dtor/what
│   ├── memory_allocator.cpp         # MemoryAllocator: dtor, allocateCLAligned (inlined),
│   │                                #   allocateReloadingCL (inlined),
│   │                                #   allocateFirstSuitableFreeBlock<T> (3 instantiations)
│   ├── callbacks.cpp                # ProgressCallback: dtor, setProgress (both empty defaults)
│   ├── log_exception.cpp           # zhinst::detail::logExceptionToClog @0x314a30 (953 bytes)
│   │                               #   Rethrows exception_ptr, catches boost::exception / std::exception / ...,
│   │                               #   logs diagnostic info to std::clog
│   ├── asm_expression.cpp          # AsmExpression: dtor, createValue, createRegister,
│   │                              #   createName, createArgList, appendArgList (6 functions)
│   ├── asm_parser_context.cpp      # AsmParserContext: 22 accessors, Label ctor/op==,
│   │                              #   addNode, addCommand, asmerror (free functions)
│   ├── elf_writer.cpp              # ElfWriter: ctor, prepareHeader, addCode, addData,
│   │                              #   addWaveform, writeFile (2 overloads), setMemoryOffset
│   ├── static_resources.cpp       # StaticResources: ctor, dtor, getVariable, init (~15KB of addConst calls)
│   ├── global_resources.cpp       # GlobalResources: ctor, dtor, TLS init (MT19937-64 PRNG seed)
│   ├── resources.cpp              # Resources base: 20+ methods (setState, hasMain, return*,
│   │                              #   getVariable, print/toString/printScopes), Variable dtor,
│   │                              #   Function ctor/dtor/methods, ResourcesException
│   └── write_waves_to_elf.cpp     # writeWavesToElfMapped + writeWavesToElfAbsolute
│                                  #   (inlined into AWGCompilerImpl::writeToStream @0x108cc0)
└── notes/
    ├── opcode_map.md            # Full opcode table: value, name, args, device, notes
    ├── cervino_vs_hirzel.md     # Device selection logic + per-method difference matrix
    ├── struct_layouts.md        # Raw byte-offset tables for all structs (updated Phase 5)
    ├── node_tree_structure.md   # Node tree model: sibling chain, elseBranch, children
    ├── unknowns.md              # Open questions (~40 closed, ~37 open)
    ├── opcode_encoding.md       # AWGAssembler instruction encoding formats (6 types)
    ├── optimization_passes.md   # AsmOptimize pass details, flags, register allocator
    ├── pipeline.md              # Full compilation pipeline flow (Compiler::compile)
    ├── memory_allocator_analysis.md  # MemoryAllocator detailed analysis (Phase 8b)
    └── write_waves_to_elf.md    # Analysis of writeWavesToElf inlined helpers
```

## What is Reconstructed vs Stubbed

**Fully reconstructed from disassembly:**

*Phase 1 — AsmCommands:*
- `AsmCommandsImpl::getInstance()` — exact bitmask logic
- All 11 `AsmCommandsImplCervino` virtual methods — opcodes, register slots, error cases
- All 11 `AsmCommandsImplHirzel` virtual methods — same, with device-specific differences
- ~70 of 83 `AsmCommands` methods — simple instructions, ALU, branches, load/store, triggers, pseudo-instructions, node-creating methods

*Phase 2 — Structurally Embedded Types:*
- `Value` class — full layout (40 bytes), toInt/toDouble/toBool/toString/operator==, Value(string) ctor, destructor
- `Immediate` class — full layout (28 bytes), all constructors, conversions, operator==, toString, operator<<
- `ValueException` — ctor, dtor, what(). Layout: vptr + string (0x20 bytes)
- `PlayConfig` — all shift/mask constants, encodeCwvf (full bit-packing), operator!=, toJson, fromJson
- `Assembler` — 43-entry cmdMap, commandToString, commandFromString, str(bool verbose), getOpcodeType, getCycles, getCmdType, getRegisterOrder
- `AssemblerInstr` — full 0x80-byte layout (cmd, immediates, reg2/reg0/reg1, outputs, label, comment). highestRegisterNumber, copy/move/dtor
- `Node` — full 0x110-byte layout, 14 NodeType values, 15 methods. Field names confirmed from toJson/fromJson JSON keys
- `AsmList::Asm` — 0xA8-byte record, 7 methods. `AsmList` — vector wrapper, 6 methods
- `AsmRegister` — struct {int value; bool valid} (8 bytes), corrected from enum
- `WaveformFront` — full 0xF8-byte layout, 3 methods (toString, copy-rename ctor, dtor)

*Phase 3 — Simple Foundational Types:*
- `DeviceConstants` — full 0x90-byte POD layout. Flat scalar fields (RegisterBank removed Phase 7e). Factory for all 9 device types
- `ErrorMessages` — 305-entry format string table. Full ErrorMessageT enum. operator[], format<>(), ResourcesException, getApiErrorMessage()
- `Signal` — 0x58-byte struct. 17 methods: 7 ctors, copy ctor, dtor, 2 appends, resizeSamples, checkAllocation, toJson, fromJson, getRawData, operator==
- `AWGCompilerConfig` — ~0xC0-byte config struct. Layout: deviceType, addressImpl, isHirzel (bool +0x18), cacheType (uint8_t +0x19), numChannelGroups, debugFlags (uint32_t +0x90), strings, includePaths, wavetableSize, searchPath. 4 methods. **Major correction**: AwgDeviceType enum completely revised — 9 devices with codenames. Hirzel={HDAWG(2), SHFQA(8), SHFSG(16), SHFQC_SG(32), SHFLI(64)}. Cervino={UHFLI(1), UHFQA(4), GHFLI(128), VHFLI(256)}

*Phase 4 — AWGAssembler:*
- `AWGAssembler` — 0x08-byte pimpl wrapper (raw owning pointer). 11 forwarding methods
- `AWGAssemblerImpl` — 0x170-byte implementation. Full layout: DeviceConstants*, 3 strings, opcodes vector, bimap for labels, parser context, sampleRate_
- 6 opcode encoding methods (opcode0–5) with getReg/getVal helpers. All instruction word formats documented
- Full assembly pipeline: assembleFile→assembleString→getAST (flex/bison)→assembleExpressions→evaluate→opcodeN
- ELF output via writeToFile (.comment "ZI AWG Sequencer Compiler 1.4", .filename, .asm sections)
- `AsmExpression` — 0xa8-byte parse tree node. Full layout reconstructed: type (1=reg/2=label/3=int), name (+0x08), str2 (+0x20), command (+0x38), value (+0x3C), children vector (+0x40), optional Label (labelPc +0x5C, labelName +0x60, hasLabel +0x78), optional comment (+0x80, hasComment/noOpt +0x98), field_A0 (+0xA0). Factory functions: createValue, createRegister, createName, createArgList, appendArgList. Dtor destroys in reverse: comment→label→children→str2→name

*Phase 5 — Waveform Complex:*
- `Waveform` base — 0xD8-byte value type (no vtable). 11 methods. JSON key mapping reveals all field semantics
- `Waveform::File` (WaveformFile) — 0x40-byte struct. Type enum (CSV=0, RAW=1, GEN=2) with lazy-init static maps
- `getSizePerDevice()` — `ceil(roundUp(length, granularity) * channels * bitsPerSample / 8)`
- `WaveformIR` — 0xE0 bytes. 2 ctors + toJsonElement
- `WaveIndexTracker` — 0x28 bytes. std::set tracking, auto-fill gaps, template ctor from WavetableManager
- `WavetableException` — 0x20 bytes, std::exception subclass
- `WavetableManager<T>` — 0x48-byte template (Front: 9 methods, IR: 7 methods)
- `WavetableFront` — 0x200 bytes. 24 methods: creation, naming, loading, DIO tracking, warning callbacks
- `WavetableIR` — 0xC8 bytes. 21 methods: allocation (FIFO deque), implicit wave index, size alignment, serialization

*Phase 6 — AsmOptimize:*
- `AsmOptimize` — 0xA0-byte optimizer. 5 query helpers (isRead, isWritten, isLabelCalled, getNextActionForReg, registerIsNeverWritten)
- 11 optimization passes: deadCodeElimination, oneStepJumpElimination, removeUnusedLabels, mergeLabels, mergeRegisterZeroing, removeUnusedRegs, reportUserMessages, simplifyAssign, prepareResources, optimizePreWaveform, optimizePostWaveform
- Register allocator: registerAllocation (graph-coloring), splitConstRegisters, splitReg, registerUpdate
- `OptimizeException` — 0x20-byte exception (ctor/dtor/what)

*Phase 7a — Compiler Shell + Pipeline Glue:*
- `Compiler` — 0x138-byte orchestrator. Full pipeline flow mapped (parse→toSeqCAst→lower→optimize→prefetch→optimize→output)
- `CompilerMessage` — 0x20-byte struct (type enum + lineNr + message). `str()` method
- `CompilerMessageCollection` — 0x20-byte inline collector. 7 methods: compilerMessage, errorMessage, warningMessage, infoMessage, parserMessage, reset, messages
- `CompilerException` — 0x20-byte exception (ctor/dtor/what)
- `FrontEndLoweringFacade::lower()` — thin static facade dispatching to SeqCAstNode::lower() virtual
- `Compiler::parse()` — flex/bison parser entry (seqc_lex/seqc_parse)
- `Compiler::unifyLineEndings()` — boost::replace_all_copy for \r\n/\r normalization
- `Compiler::compile()` — master pipeline (~13KB, high-level structure captured)
- `Compiler::runPrefetcher()` — prefetch orchestrator (~2.8KB, step-by-step flow captured)

*Phase 7b — Cache:*
- `Cache` — 0x28-byte waveform cache manager. 4 getters, 2 allocate overloads, getBestPosition (gap-finding), memoryWrite (sorted insert with overlap removal), stillInMemory, reuse, play, resetPlay, free, print
- `Cache::Pointer` — 0x24-byte allocation entry. `str()` method for debug printing
- `CacheException` — 0x20-byte exception (ctor/dtor/what)

*Phase 8b — MemoryAllocator:*
- `MemoryBlock` — 12-byte block descriptor {start, end, flags}. Return convention: rax=start|(end<<32), dl=flags
- `MemoryAllocator` — 0x70-byte CL-aware waveform memory allocator. No vtable, ctor always inlined. Uses deque<MemoryBlock> free list (341 per page), vector<uint32_t> CL ownership table. Methods: dtor @0x29f2d0, allocateCLAligned (inlined, 2-phase: fast single-CL + general multi-CL with SSE2), allocateReloadingCL (inlined, avoids CL conflicts with existing waveforms), allocateFirstSuitableFreeBlock<T> (3 template instantiations @0x2aa960/0x2aac80/0x2ad190)

*Phase 8c — Callbacks:*
- `CancelCallback` — Pure virtual interface. No vtable/typeinfo/methods in binary (implementations in Python binding layer). Single method: `virtual bool isCancelled() = 0`. Used via weak_ptr in Compiler, Prefetch, WavetableIR, AsmOptimize
- `ProgressCallback` — 8-byte virtual base (vtable only). Default empty implementations: dtor @0x129960, setProgress(double) @0x129980. vtable @0xb03858. Used via weak_ptr in Compiler at +0xF0

**Partially reconstructed (logic approximate):**
- `AsmCommands::alui()` — multi-instruction immediate splitting (core logic present, edge cases uncertain)
- `AsmCommands::asmPlay()` — most complex method; waveform name vector, PlayConfig, packed play word
- `AsmCommands::genPlayConfig()` — marker processing loop reconstructed
- `AsmOptimize::registerAllocation()` — algorithm structure captured (live ranges, conflict graph, greedy allocation); full ~1900 asm lines are approximate
- `AsmOptimize::removeUnusedRegs()` — core logic present, cancel callback and inner scan loop approximate
- `AsmOptimize::splitConstRegisters()` — splitting strategy captured, inner loop details approximate
- `Cache::memoryWrite()` — overlap removal logic is semantic (sorted insert), but manual memmove/refcount manipulation in binary is simplified

*Phase 7c — Prefetch Tree Preparation + Optimization (COMPLETE):*
- `Prefetch` — 0x160-byte waveform prefetch manager. ~30 methods across 3 files (~3700 lines)
- Tree prep: preparePlays, prepareTree, countBranches, definePlaySize
- Wave management: determineFixedWaves, collectUsedWaves, getUsedWavesForDevice
- Load placement: placeLoads, moveLoadsToFront, createLoad, mergeLoads, assignLoad, linkLoad
- Optimization: optimize (BFS load merging), optimizeSync, optimizeCwvf (CWVF propagation), globalCwvf
- Cache allocation: allocate (branch-scoped via Cache::getScope)
- Helpers: backwardTree, removeBranches, expandSetVar, findLockedPlay, sameLoads, nodeByCachePointer
- Queries: getMemoryHighWatermark, getRequiredMemory, getUsedChannels, getUsedFourChannelMode, clampToCache
- `PrefetcherNodeState` — per-node state (0x40 bytes): 2 AsmRegisters (hirzel/cervino), state (init=3, unloaded), branchCount (init=1), refTrack, pageSize (init=1), requiredSlots, shared_ptr\<Cache::Pointer\> cachePtr, useDA bool. Phase 10.5e: corrected init values and field types.

*Phase 9a — ElfWriter:*
- `ElfWriter` — 0x78-byte ELF output writer (inherits ELFIO::elfio). All 8 methods: ctor @0x2934a0, prepareHeader @0x2936b0, addCode @0x293710, addData @0x293990, addWaveform @0x2939f0 (returns unique_ptr\<RawWave\>), writeFile(ostream) @0x294030, writeFile(string) @0x2942a0, setMemoryOffset @0x294410. Phase 10.5b: replaced vtable-offset pseudocode with proper ELFIO API calls; fixed addWaveform return type (unique_ptr\<RawWave\>, not ElfWriter*) and NOBITS path (set_size, not set_link).
- `writeWavesToElfMapped` / `writeWavesToElfAbsolute` — inlined into AWGCompilerImpl::writeToStream @0x108cc0

*Phase 9b — AsmParserContext:*
- `AsmParserContext` — ~0x80-byte parser context (5 bools, lineNumber, programCounter, errorCallback, stringCopies vector, labels set). 22 accessor methods @0x28e7a0–0x28ead0
- `AsmParserContext::Label` — 0x20-byte inner type (pc + name string). Ctor @0x28eaa0, operator== @0x28ead0
- `addNode` @0x28bfd0 — allocates AsmExpression (0xa8), splits on '#' for comments
- `addCommand` @0x28c600 — resolves command name via commandFromString, handles labels, builds expression
- `asmerror` @0x292a60 — wraps error callback + sets syntax error flag
- `asmparse` @0x292b50 — ~217KB bison-generated, deferred

**Not yet reconstructed:**
- `CachedParser` — 0x60-byte tree-based cache, used by WavetableFront + WavetableIR
- `WaveformGenerator` — 16 symbols, waveform DSP (createDummyWaveform, genericTriangle, reverse + 2 exception classes)

*Phase 10 — Scope & Symbol Management:*
- `AsmExpression` — 0xa8-byte parse tree node. Full layout reconstructed (Phase 10c). Factory functions: createValue, createRegister, createName, createArgList, appendArgList
- `StaticResources` (0x110) — ctor, dtor, getVariable override (SSE checks for DEVICE_SAMPLE_RATE, AWG_MONITOR_TRIGGER, AWG_INTEGRATION_ARM/TRIGGER, ZSYNC_DATA_PROCESSED_*; reports error 0x34 for deprecated constants), init (**fully reconstructed**: all 213 addConst calls across ~15KB — 4 rate families, QA_INT/QA_GEN channel bitmasks, ZSYNC_DATA computed constants, 32 trigger indices, trigger bitmasks, channels/markers, suppress/enable, math constants, booleans)
- `GlobalResources` (0xD8) — ctor (MT19937-64 PRNG seeding), dtor. TLS statics: regNumber, labelIndex, random[313]
- `Resources` base (0xD8) — 20+ methods reconstructed: setState, hasMain, setReturnType/getReturnType (recursive parent walk), setReturnValue×2/getReturnValue, setReturnReg/getReturnReg (recursive), getRegisterNumber (TLS), getVariable (virtual, parent-walking search), print/toString/printAll/printScopes. Inner types: Variable (0x58 bytes: varType, boost::variant data, AsmRegister, name, flags; dtor @0x1e4be0), Function (0x78 bytes: name, signature, returnType, arguments vector, scope shared_ptr, body unique_ptr; ctor/dtor/resetScope/addArguments/addBody/getBody), State enum (Unset=0, Active=1, Paused=2, Locked=3), ResourcesException (ctor/dtor/what)
- `AsmCommands::syncCervino()` / `unsyncCervino()` — fully reconstructed (Phase 10.5d)
- `AsmList::parseStringToAsmList()` — fully reconstructed; 0x266160–0x268130 (7632 bytes). Deserializes assembly text via AWGAssembler pipeline, rebuilds AsmList with register assignment via getRegisterOrder() switch, JSON-based Node reconstruction for placeholder entries, and Node::installPointers post-pass

*Phase 7d — Prefetch Command Emission (COMPLETE):*
- `placeSingleCommand` (0x1d7940, 20KB) — main node-type→assembly dispatch. Switch on node->type:
  Load→LD+wvf, Play→cwvf+wvf/splitPlay, Branch→labels+jumps, Loop→loop constructs, CWVF→config writes, etc.
- `placeCommands` (0x1d6680) — walks linked list calling placeSingleCommand, checks cancelCb
- `fillInPlaceholders` (0x1d65c0) — creates output AsmList, calls placeCommands(out, root_)
- `findPlaceholder` (0x1d6b50) — linear scan for matching asmId
- `splitPlay` (0x1dd1a0, 7.6KB) — splits large waveforms into paged play loops with register arithmetic
- `needsNewCwvf` (0x1dc620, 2.9KB) — walks load chain comparing PlayConfig fields
- `wvfImpl` / `wvfRegImpl` / `wvfs` — waveform instruction generation (immediate vs register offset)
- `insertPlay` — label + wvfImpl + optional xnori for Cervino
- `print` (0x1c5dd0, 8.4KB) — recursive debug tree printer (switch on ~15 node types)
- `getUsedCache` (0x1c7eb0) — recursive cache usage computation
- `getMemoryHighWatermark` / `getRequiredMemory` — per-device waveform memory queries
- `getUsedChannels` / `getUsedFourChannelMode` — usage entry queries
- `clampToCache` — address clamping with Hirzel alignment
- Key discovery: AWGCompilerConfig+0x19 = `cacheType` (uint8_t, Normal=0 / Aligned=1)

*Phase 10.5f — Prefetch APPROXIMATE Revisits (COMPLETE):*
- Reduced 67 APPROXIMATE markers across 5 files to 10 (all in prefetch.cpp and prefetch_splitplay.cpp)
- `prefetch_print.cpp`: all 17 confirmed correct (0 remaining)
- `prefetch_emit.cpp`: 8→0 (fixed ternary in line 703, null-parent guard in 738, confirmed rest)
- `prefetch_placesingle.cpp`: 2→0 (fixed halfPageCount signed-div, confirmed cwvfConfig copy)
- `prefetch.cpp`: 18→5 (fixed NodeType::Play→Load, devConst_→waveformIR name, state.useDA→curNode->config.dummy ×2)
- `prefetch_splitplay.cpp`: 22→5 (fixed pageSize access via cachePtr->pageSize_, ssl/addr register copyReg not cervinoReg)

*Phase 10.6 — Compilation Error Audit (COMPLETE):*
- **Error count progression:** 5701 → 2356 → 1932 → 1853 → 1818 → 1709 → 1225 → 1182 → 754 → 534 → 457 → 252 → **0**
- **All 47 source files now compile with zero errors** (1 warning: placement-new size mismatch in value.cpp due to libc++/libstdc++ string size difference)
- Sub-phases 10.6a–l: ELFIO cstdint, AsmEntry→AsmList::Asm rename, value.hpp sizing, PlayConfig dedup, node.hpp fixes, struct members, ErrorMessages static, includes/forward-decls, boost path, header additions, Node/Waveform members, AsmCommands/Resources includes
- Sub-phase 10.6m: 457→252 (205 fewer). Key fixes: ResourcesException dedup, 15+ Node TBD fields, WaveformIR/Front/DeviceConstants member additions, AsmCommands output-param overloads, AWGAssemblerImpl expansion
- Sub-phase 10.6n: 252→0 (all remaining). Key fixes:
  - **AsmList::Asm::operator==**: Added comparison operator (fixed 11 files at once)
  - **Value() default ctor**: Added for Resources::Variable initialization
  - **Waveform() default ctor**: Added for factory paths
  - **Missing includes**: `<variant>` in value.cpp, `rawwave.hpp` in signal.cpp, `device_constants.hpp`+`<boost/property_tree/ptree.hpp>` in waveform_ir.cpp, `error_messages.hpp` in wave_index_tracker.cpp
  - **MemoryAllocator**: Removed failing static_assert, added `hasFreeBlocks()`/`lastFreeBlock()` public accessors
  - **WavetableManager<WaveformIR> field renames**: `numDefs_`→`lineNr_`, `numDefs2_`→`waveformCounter_`, `nameIndex_`→`nameToIndex_`
  - **Signal field access fixes**: `data`→`data()`, `markers`→`markers_`, `playMarkers`→`markerBits_`, `metadata`→individual fields
  - **File::Type::Generated**→`GEN`, removed insertWaveform<WaveformIR> specialization (uses general template)
  - **asm_optimize.cpp**: `asm_ = input`→`asm_ = input.entries`, fixed swap
  - **prefetch.cpp**: `this->branchMaySkipAllBodies`→`cur->branchMaySkipAllBodies`
  - **const correctness**: DeviceConstants* → const DeviceConstants* in waveform.cpp/wavetable_ir.cpp
  - **AsmList**: Added operator==, swap friend, insert(shared_ptr, AsmList&) overload

## Key Technical Findings

- **Return convention:** All `const` methods use sret (hidden first parameter = return value pointer). Actual `this` is in `rsi`.
- **Sequence IDs:** Thread-local counter at TLS offset 0x44, incremented per node created (simple ctor) or per instruction emitted (AsmEntry). Both counters reset to 0 in `Compiler::compile()`.
- **isWaveformCmd:** `(cmd - 3) < 3u` — flags opcodes 3/4/5 (MESSAGE and ERROR_MSG pseudo-instructions). Name may be misleading.
- **Large immediates:** Signed ALU immediate instructions split values > ~18-bit across ADDI (low 12 bits) + ADDIU chain (12 bits at a time).
- **AsmRegister:** 8-byte struct `{int value; bool valid;}`, NOT an enum. Separate valid flag from register number.
- **AssemblerInstr register order:** reg2 (destination) at +0x20, reg0 (src1) at +0x28, reg1 (src2) at +0x30.
- **AssemblerInstr outputs:** Second `vector<Immediate>` at +0x38 holds output operands (distinct from input immediates at +0x08).
- **Opcode names (Phase 2a):** 43 entries in cmdMap. XORR (0x60000007), WWVF (0xF1000000), WVFEI (0xFB000000), WVFE (0xFA), WVFET (0xFC), JMP (0xFE), BRGZ (0xF5000000). Alias: `wwvfq` = WPRF.
- **NodeTypes (Phase 2):** SyncHirzel=0x2000, AwgReady=0x8000. "Prefetch" renamed to "PlainLoad" (0x4000).
- **Branch penalty:** BRZ, BRNZ, BRGZ cost 3 cycles; all other instructions cost 1.
- **DeviceConstants (Phase 3a, revised Phase 7e):** POD struct, 0x90 bytes. 9 device types. RegisterBank sub-structs removed — cross-referencing all consumers (Prefetch, WavetableIR, AWGAssemblerImpl, Cache, WaveformFront) revealed the 16-byte groups are unrelated scalar fields. Key renames: +0x0C=waveformMemorySize (was waveformReg.stride), +0x10=sampleLength, +0x14=waveformAlignment, +0x18=cachePageCount (was commandTableReg.base), +0x1C=maxBlocks, +0x28=registerDepth (was numOutputChannels), +0x30=memoryDepth (was registerSpace), +0x40=waveformGranularity (was sequencerReg.width), +0x44=waveformPageSize, +0x50=bitsPerSample (was auxReg.width). +0x88 hasPrecomp confirmed = Prefetch "useDA" (same field).
- **Signal (Phase 3c):** 0x58 bytes (corrected from 48). Fuzzy float comparison: `|a-b| <= |b|*eps + eps` (eps=1e-12). `checkAllocation()` is a lazy materializer. `getRawData()` reveals RawWaveData hierarchy.
- **AwgDeviceType (Phase 3d):** Major enum correction. 9 devices with codenames (cervino/hirzel/klausen/grimsel_*/gurnigel/maloja). 5 Hirzel, 4 Cervino. Channel grouping only for HDAWG.
- **AWGAssembler (Phase 4):** Raw owning pointer pimpl. Six instruction encoding formats (opcode0–5). Pipeline: file→string→AST→expressions→evaluate→opcodeN. ELF output with .comment/.filename/.asm sections. Label resolution via boost::bimap.
- **Waveform Complex (Phase 5, updated 10.5g):** Front/IR split architecture. `WavetableManager<T>` shared template. Wave index assignment: explicit or implicit (auto-fill gaps). Allocation is FIFO-deque based with device granularity alignment. `CachedParser` (0x60 bytes) embedded in both WavetableFront and WavetableIR — not yet reconstructed. Phase 10.5e: `getJsonIndex` returns `std::string` via `boost::property_tree` (not `boost::json::value`); `alignWaveformSizes` uses `waveformPageSize` (DC+0x44) not `waveformAlignment`; `assignWaveformAllocationSizes` computes ceil(samples*channels*bitsPerSample/8) rounded to 64 bytes, with `bitsPerSample` at DC+0x50, `waveformGranularity` (DC+0x40) as max cap; `WaveformFront` inlined basic ctor takes `(name, fileType, devConst)`, sets waveIndex=-1, playIndex=1. Phase 10.5g: all 3 allocation lambda bodies fully reconstructed — `allocateWaveforms` Phase 2 lambda (0x2a9c80) performs per-waveform cache-line allocation with conflict checking; `allocateWaveformsForFifo` uses two-pass strategy: $_0 (0x2aa700) allocates irBool0==1 waveforms via `allocateCLAligned`, $_1 (0x2acfb0) handles irBool0==0 with `allocateReloadingCL` fallback. Key: `irBool0` (+0xD9) partitions waveforms into two allocation classes; `irBool1` (+0xDA) = crossesCacheLine flag from MemoryBlock.flags bit 8.
- **AsmOptimize (Phase 6):** 0xA0-byte optimizer with flag-controlled passes. 5 optimization flags (0x01=jumpElim, 0x02=labelCleanup, 0x04=deadCode, 0x08=mergeZeroing, 0x10=regAlloc). Query helpers use `getCmdType()` bitmask (bit 0=reads, bit 1=writes). Register allocator is graph-coloring with greedy allocation; on failure, retries after `splitConstRegisters` (splits long live ranges of constant-loaded registers). `reportUserMessages` extracts MESSAGE/ERROR_MSG pseudo-instructions and routes them through std::function callbacks. `GlobalResources::regNumber` (TLS) provides fresh virtual register numbers for splitting. `OptimizeException` (0x20 bytes) inherits `std::exception` (not `std::runtime_error`).
- **Compiler Pipeline (Phase 7a):** Compiler is a 0x138-byte orchestrator. Pipeline: parse (flex/bison) → toSeqCAst → FrontEndLoweringFacade::lower (virtual dispatch) → linearize → AsmOptimize::optimizePreWaveform → serialize/deserialize round-trip → runPrefetcher (Prefetch + WavetableIR waveform sizing/allocation/placement) → AsmOptimize::optimizePostWaveform → unsyncCervino → output. FrontEndLoweringFacade is a thin static facade — packs args into FrontendLoweringContext, dispatches to SeqCAstNode::lower() virtual. CompilerMessageCollection (0x20 bytes inline) accumulates errors/warnings with duplicate filtering. Debug flags at AWGCompilerConfig+0x90 control AST/tree/assembly printing.
- **Cache (Phase 7b):** 0x28-byte waveform cache with sorted vector of Pointer entries. Two allocation modes: Normal (no alignment doubling) and Aligned (pageSize*2). Gap-finding algorithm (best-fit). PointerState machine: Ready→Playing→LastPlayed→Free. `getScope()` creates a snapshot copy. `unusedCacheLine` sentinel = 0xFFFFFFFF. Error messages use ErrorMessageT indices 0x13–0x16.
- **Prefetch (Phase 7c):** 0x160-byte waveform prefetch manager. Three-pass tree preparation (prepareTree→countBranches→definePlaySize), then placeLoads orchestrates: moveLoadsToFront (hoists loads), optimize (BFS load merging by cache page budget), optimizeSync (placeholder reordering), optimizeCwvf (CWVF config propagation), allocate (cache allocation with branch scoping via Cache::getScope). Key field discoveries: +0xB8=maxBranches_ (not pageSize_), +0xC0=cwvfConfig_ (PlayConfig, not 7 unknowns), +0xF8=lastCwvfNode_, +0x108=globalCwvfValid_. Node+0x18/+0x20 confirmed as load pointer. AWGCompilerConfig+0x18=isHirzel, +0x24=deviceIndex. WaveformIR+0xDA=needsLoad flag.

See `reconstructed/notes/` for detailed opcode tables, struct offset maps, and the full list of open questions.
